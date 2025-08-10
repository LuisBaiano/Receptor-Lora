#include "lora.h"
#include <stdio.h>
#include <string.h>
#include <math.h>
#include "hardware/gpio.h"
#include "pico/time.h"

// ============================================================================
// --- Variáveis Estáticas (Privadas) ---
// ============================================================================

// Armazena a configuração passada durante a inicialização
static lora_config_t _lora_config;

// Ponteiro para a função de callback do usuário para pacotes recebidos
static void (*_on_receive_callback)(lora_payload_t*);

// Rastreia o modo de operação atual do rádio
static uint8_t _current_mode = MODE_STDBY;

// ID do último pacote enviado, usado para correspondência de ACK
static uint8_t _last_header_id = 0;

// Flag volátil para indicar o recebimento de um ACK, usado em lora_send_to_wait
static volatile bool _ack_received = false;

// Buffer para o último ACK recebido, para verificação do ID
static lora_payload_t _last_ack_payload;


// ============================================================================
// --- Protótipos de Funções Estáticas (Privadas) ---
// ============================================================================

static void lora_spi_write_reg(uint8_t reg, const uint8_t *data, size_t len);
static void lora_spi_read_reg(uint8_t reg, uint8_t *data, size_t len);
static uint8_t lora_spi_read_single_reg(uint8_t reg);

static void lora_set_modem_config(modem_config_t modem);
static void lora_set_frequency(float freq_mhz);
static void lora_set_tx_power(uint8_t tx_power);
static void lora_send_ack(uint8_t to, uint8_t id);

static void gpio_irq_handler(uint gpio, uint32_t events);

// ============================================================================
// --- Implementação das Funções Públicas ---
// ============================================================================

bool lora_init(lora_config_t *config) {
    _lora_config = *config;

    // 1. Inicializa SPI
    spi_init(_lora_config.spi_port, 5000000); // Inicializa em 5 MHz
    gpio_set_function(_lora_config.interrupt_pin, GPIO_FUNC_SIO); // O pino de interrupção é um GPIO normal para o SDK
    spi_set_format(_lora_config.spi_port, 8, SPI_CPOL_0, SPI_CPHA_0, SPI_MSB_FIRST);

    // 2. Inicializa pinos GPIO
    gpio_init(_lora_config.cs_pin);
    gpio_set_dir(_lora_config.cs_pin, GPIO_OUT);
    gpio_put(_lora_config.cs_pin, 1); // Desativar CS

    // Se um pino de reset for fornecido, execute o ciclo de reset
    if (_lora_config.reset_pin != 0) { // Assume 0 como "não conectado"
        gpio_init(_lora_config.reset_pin);
        gpio_set_dir(_lora_config.reset_pin, GPIO_OUT);
        gpio_put(_lora_config.reset_pin, 0);
        sleep_ms(10);
        gpio_put(_lora_config.reset_pin, 1);
        sleep_ms(10);
    }

    // 3. Configura o chip LoRa
    // Entra no modo SLEEP e LoRa
    lora_sleep();
    uint8_t op_mode_lora = LONG_RANGE_MODE | MODE_SLEEP;
    lora_spi_write_reg(REG_01_OP_MODE, &op_mode_lora, 1);
    sleep_ms(10);

    // Verifica se o modo foi definido corretamente
    if (lora_spi_read_single_reg(REG_01_OP_MODE) != op_mode_lora) {
        return false;
    }

    // Define os endereços base do FIFO
    uint8_t fifo_addr = 0x00;
    lora_spi_write_reg(REG_0E_FIFO_TX_BASE_ADDR, &fifo_addr, 1);
    lora_spi_write_reg(REG_0F_FIFO_RX_BASE_ADDR, &fifo_addr, 1);

    lora_set_mode_idle();

    // 4. Aplica configurações específicas
    lora_set_modem_config(_lora_config.modem);
    lora_set_frequency(_lora_config.freq);
    lora_set_tx_power(_lora_config.tx_power);
    
    // Define o comprimento do preâmbulo para 8
    uint8_t preamble_msb = 0x00;
    uint8_t preamble_lsb = 0x08;
    lora_spi_write_reg(REG_20_PREAMBLE_MSB, &preamble_msb, 1);
    lora_spi_write_reg(REG_21_PREAMBLE_LSB, &preamble_lsb, 1);
    
    // 5. Configura a interrupção do GPIO
    gpio_set_irq_enabled_with_callback(
        _lora_config.interrupt_pin,
        GPIO_IRQ_EDGE_RISE,
        true,
        &gpio_irq_handler
    );
    
    lora_set_mode_rx_continuous();

    return true;
}

void lora_on_receive(void (*callback)(lora_payload_t*)) {
    _on_receive_callback = callback;
}

void lora_send(const uint8_t *data, size_t length, uint8_t header_to) {
    lora_set_mode_idle();
    
    uint8_t header[4] = {header_to, _lora_config.this_address, _last_header_id, 0};
    uint8_t payload[255];
    
    memcpy(payload, header, 4);
    memcpy(payload + 4, data, length);
    
    size_t payload_len = length + 4;
    
    // Posiciona o ponteiro do FIFO para a base de TX
    uint8_t fifo_tx_base = 0x00;
    lora_spi_write_reg(REG_0D_FIFO_ADDR_PTR, &fifo_tx_base, 1);

    // Escreve o payload no FIFO
    lora_spi_write_reg(REG_00_FIFO, payload, payload_len);
    
    // Define o tamanho do payload
    lora_spi_write_reg(REG_22_PAYLOAD_LENGTH, (uint8_t*)&payload_len, 1);
    
    // Inicia a transmissão
    lora_set_mode_tx();
}

bool lora_send_to_wait(const uint8_t *data, size_t length, uint8_t header_to, int retries, uint32_t retry_timeout_ms) {
    if (header_to == BROADCAST_ADDRESS) {
        return false; // Não se pode esperar ACK de broadcast
    }

    _last_header_id = (_last_header_id + 1) & 0xFF; // Incrementa e limita a 8 bits
    
    for (int i = 0; i <= retries; i++) {
        _ack_received = false;
        
        // Envia o pacote
        lora_send(data, length, header_to);

        // Espera TX completar (o modo muda para IDLE no IRQ)
        uint64_t start_tx = time_us_64();
        while (_current_mode == MODE_TX) {
            if (time_us_64() - start_tx > 500000) { // Timeout de 500ms para TX
                 break;
            }
        }

        // Entra no modo de recepção para esperar o ACK
        lora_set_mode_rx_continuous();

        uint64_t start_time = time_us_64();
        while ((time_us_64() - start_time) / 1000 < retry_timeout_ms) {
            if (_ack_received) {
                // Verifica se o ID do ACK corresponde ao pacote enviado
                if (_last_ack_payload.header_id == _last_header_id) {
                    lora_set_mode_rx_continuous();
                    return true;
                }
                _ack_received = false; // ID incorreto, continue esperando
            }
        }
    }
    
    lora_set_mode_rx_continuous(); // Retorna ao modo de escuta
    return false;
}

void lora_set_mode_idle() {
    if (_current_mode != MODE_STDBY) {
        uint8_t mode = LONG_RANGE_MODE | MODE_STDBY;
        lora_spi_write_reg(REG_01_OP_MODE, &mode, 1);
        _current_mode = MODE_STDBY;
    }
}

void lora_set_mode_rx_continuous() {
    if (_current_mode != MODE_RXCONTINUOUS) {
        uint8_t mode = LONG_RANGE_MODE | MODE_RXCONTINUOUS;
        lora_spi_write_reg(REG_01_OP_MODE, &mode, 1);
        uint8_t dio_mapping = 0x00; // DIO0 em RxDone
        lora_spi_write_reg(REG_40_DIO_MAPPING1, &dio_mapping, 1);
        _current_mode = MODE_RXCONTINUOUS;
    }
}

void lora_set_mode_tx() {
    if (_current_mode != MODE_TX) {
        uint8_t mode = LONG_RANGE_MODE | MODE_TX;
        lora_spi_write_reg(REG_01_OP_MODE, &mode, 1);
        uint8_t dio_mapping = 0x40; // DIO0 em TxDone
        lora_spi_write_reg(REG_40_DIO_MAPPING1, &dio_mapping, 1);
        _current_mode = MODE_TX;
    }
}

void lora_sleep() {
    if (_current_mode != MODE_SLEEP) {
        uint8_t mode = LONG_RANGE_MODE | MODE_SLEEP;
        lora_spi_write_reg(REG_01_OP_MODE, &mode, 1);
        _current_mode = MODE_SLEEP;
    }
}

void lora_close() {
    spi_deinit(_lora_config.spi_port);
}

// ============================================================================
// --- Implementação das Funções Estáticas (Privadas) ---
// ============================================================================

static void lora_send_ack(uint8_t to, uint8_t id) {
    lora_set_mode_idle();
    
    // Payload do ACK: para, de, id, flag de ACK
    uint8_t payload[4] = {to, _lora_config.this_address, id, FLAGS_ACK};
    
    // Posiciona ponteiro do FIFO
    uint8_t fifo_tx_base = 0x00;
    lora_spi_write_reg(REG_0D_FIFO_ADDR_PTR, &fifo_tx_base, 1);

    // Escreve payload no FIFO
    lora_spi_write_reg(REG_00_FIFO, payload, 4);

    // Define tamanho do payload
    uint8_t len = 4;
    lora_spi_write_reg(REG_22_PAYLOAD_LENGTH, &len, 1);
    
    // Inicia transmissão
    lora_set_mode_tx();
}


/**
 * @brief Manipulador de interrupção principal. Chamado sempre que o pino DIO0 sobe.
 */
static void gpio_irq_handler(uint gpio, uint32_t events) {
    uint8_t irq_flags = lora_spi_read_single_reg(REG_12_IRQ_FLAGS);

    // Limpa os flags de IRQ imediatamente para evitar reentrância
    lora_spi_write_reg(REG_12_IRQ_FLAGS, &irq_flags, 1);

    if (_current_mode == MODE_RXCONTINUOUS && (irq_flags & IRQ_FLAG_RX_DONE)) {
        // --- Pacote Recebido ---
        uint8_t packet_len = lora_spi_read_single_reg(REG_13_RX_NB_BYTES);
        uint8_t rx_current_addr = lora_spi_read_single_reg(REG_10_FIFO_RX_CURRENT_ADDR);
        
        // Posiciona o ponteiro do FIFO no início do pacote recebido
        lora_spi_write_reg(REG_0D_FIFO_ADDR_PTR, &rx_current_addr, 1);

        uint8_t packet[255];
        lora_spi_read_reg(REG_00_FIFO, packet, packet_len);

        if (packet_len < 4) return; // Pacote inválido

        // Extrai RSSI e SNR
        int8_t snr_val = lora_spi_read_single_reg(REG_19_PKT_SNR_VALUE);
        int16_t rssi_val = lora_spi_read_single_reg(REG_1A_PKT_RSSI_VALUE);

        float snr = snr_val / 4.0;
        float rssi;
        if (snr < 0) {
            rssi = rssi_val + snr;
        } else {
            rssi = rssi_val * 16.0 / 15.0;
        }

        if (_lora_config.freq >= 779.0) {
            rssi -= 157;
        } else {
            rssi -= 164;
        }

        lora_payload_t p;
        p.header_to = packet[0];
        p.header_from = packet[1];
        p.header_id = packet[2];
        p.header_flags = packet[3];
        p.length = packet_len > 4 ? packet_len - 4 : 0;
        p.rssi = (int)round(rssi);
        p.snr = snr;

        if (p.length > 0) {
            memcpy(p.message, packet + 4, p.length);
        }

        // --- Lógica de Filtragem e ACK ---
        
        // Ignora se o pacote não é para este nó, a menos que receive_all esteja ativado
        if (p.header_to != _lora_config.this_address && p.header_to != BROADCAST_ADDRESS && !_lora_config.receive_all) {
            return;
        }
        
        // Verifica se é um ACK
        if (p.header_to == _lora_config.this_address && (p.header_flags & FLAGS_ACK)) {
            _last_ack_payload = p;
            _ack_received = true;
        } else { // É uma mensagem normal
             // Se os ACKs estiverem ativados, envia uma confirmação
            if (_lora_config.acks && p.header_to == _lora_config.this_address) {
                lora_send_ack(p.header_from, p.header_id);
            }

            // Chama o callback do usuário, se registrado
            if (_on_receive_callback) {
                _on_receive_callback(&p);
            }
        }
    } else if (_current_mode == MODE_TX && (irq_flags & IRQ_FLAG_TX_DONE)) {
        // --- Transmissão Completa ---
        lora_set_mode_idle(); // Retorna ao modo de espera seguro
    }
}


static void lora_spi_write_reg(uint8_t reg, const uint8_t *data, size_t len) {
    gpio_put(_lora_config.cs_pin, 0); // Ativar CS
    uint8_t reg_with_write_bit = reg | 0x80;
    spi_write_blocking(_lora_config.spi_port, &reg_with_write_bit, 1);
    spi_write_blocking(_lora_config.spi_port, data, len);
    gpio_put(_lora_config.cs_pin, 1); // Desativar CS
}

static void lora_spi_read_reg(uint8_t reg, uint8_t *data, size_t len) {
    gpio_put(_lora_config.cs_pin, 0); // Ativar CS
    uint8_t reg_without_write_bit = reg & 0x7F;
    spi_write_blocking(_lora_config.spi_port, &reg_without_write_bit, 1);
    spi_read_blocking(_lora_config.spi_port, 0x00, data, len);
    gpio_put(_lora_config.cs_pin, 1); // Desativar CS
}

static uint8_t lora_spi_read_single_reg(uint8_t reg) {
    uint8_t value;
    lora_spi_read_reg(reg, &value, 1);
    return value;
}


static void lora_set_modem_config(modem_config_t modem) {
    uint8_t config1, config2, config3;

    switch (modem) {
        case BW125_CR45_SF128:
            config1 = 0x72; config2 = 0x74; config3 = 0x04; break;
        case BW500_CR45_SF128:
            config1 = 0x92; config2 = 0x74; config3 = 0x04; break;
        case BW31_25_CR48_SF512:
            config1 = 0x48; config2 = 0x94; config3 = 0x04; break;
        case BW125_CR48_SF4096:
            config1 = 0x78; config2 = 0xc4; config3 = 0x0c; break;
        default: // Padrão
            config1 = 0x72; config2 = 0x74; config3 = 0x04; break;
    }
    lora_spi_write_reg(REG_1D_MODEM_CONFIG1, &config1, 1);
    lora_spi_write_reg(REG_1E_MODEM_CONFIG2, &config2, 1);
    lora_spi_write_reg(REG_26_MODEM_CONFIG3, &config3, 1);
}

static void lora_set_frequency(float freq_mhz) {
    uint32_t frf = (uint32_t)((freq_mhz * 1000000.0) / FSTEP);
    uint8_t frf_bytes[3];
    frf_bytes[0] = (uint8_t)((frf >> 16) & 0xFF); // MSB
    frf_bytes[1] = (uint8_t)((frf >> 8) & 0xFF);  // MID
    frf_bytes[2] = (uint8_t)(frf & 0xFF);         // LSB
    lora_spi_write_reg(REG_06_FRF_MSB, &frf_bytes[0], 1);
    lora_spi_write_reg(REG_07_FRF_MID, &frf_bytes[1], 1);
    lora_spi_write_reg(REG_08_FRF_LSB, &frf_bytes[2], 1);
}

static void lora_set_tx_power(uint8_t tx_power) {
    if (tx_power < 5) tx_power = 5;
    if (tx_power > 23) tx_power = 23;
    
    uint8_t pa_config_val;
    uint8_t pa_dac_val;

    // A lógica de potência alta (+20dBm) usa o pino PA_BOOST e requer um DAC diferente
    if (tx_power > 20) {
        pa_dac_val = PA_DAC_ENABLE; // Habilita o modo de alta potência
        pa_config_val = PA_SELECT | (tx_power - 5);
    } else {
        pa_dac_val = PA_DAC_DISABLE; // Modo de alta potência desabilitado
        pa_config_val = PA_SELECT | (tx_power - 2); // Usa PA_BOOST
    }

    lora_spi_write_reg(REG_4D_PA_DAC, &pa_dac_val, 1);
    lora_spi_write_reg(REG_09_PA_CONFIG, &pa_config_val, 1);
}