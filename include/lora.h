#ifndef LORA_H
#define LORA_H

#include "pico/stdlib.h"
#include "hardware/spi.h"

// ============================================================================
// --- Constantes e Registradores (Portado de Python) ---
// ============================================================================

// --- Registradores ---
#define REG_00_FIFO                 0x00
#define REG_01_OP_MODE              0x01
#define REG_06_FRF_MSB              0x06
#define REG_07_FRF_MID              0x07
#define REG_08_FRF_LSB              0x08
#define REG_09_PA_CONFIG            0x09
#define REG_0D_FIFO_ADDR_PTR        0x0d
#define REG_0E_FIFO_TX_BASE_ADDR    0x0e
#define REG_0F_FIFO_RX_BASE_ADDR    0x0f
#define REG_10_FIFO_RX_CURRENT_ADDR 0x10
#define REG_12_IRQ_FLAGS            0x12
#define REG_13_RX_NB_BYTES          0x13
#define REG_19_PKT_SNR_VALUE        0x19
#define REG_1A_PKT_RSSI_VALUE       0x1a
#define REG_1D_MODEM_CONFIG1        0x1d
#define REG_1E_MODEM_CONFIG2        0x1e
#define REG_20_PREAMBLE_MSB         0x20
#define REG_21_PREAMBLE_LSB         0x21
#define REG_22_PAYLOAD_LENGTH       0x22
#define REG_26_MODEM_CONFIG3        0x26
#define REG_40_DIO_MAPPING1         0x40
#define REG_4D_PA_DAC               0x4d

// --- Modos de Operação ---
#define LONG_RANGE_MODE             0x80
#define MODE_SLEEP                  0x00
#define MODE_STDBY                  0x01
#define MODE_TX                     0x03
#define MODE_RXCONTINUOUS           0x05
#define MODE_CAD                    0x07

// --- Flags de IRQ (Interrupt ReQuest) ---
#define IRQ_FLAG_RX_DONE            0x40
#define IRQ_FLAG_TX_DONE            0x08
#define IRQ_FLAG_CAD_DONE           0x04
#define IRQ_FLAG_CAD_DETECTED       0x01
#define IRQ_FLAGS_CLEAR             0xff // Usado para limpar todos os flags

// --- PA (Power Amplifier) Config ---
#define PA_SELECT                   0x80 // Seleciona o pino PA_BOOST
#define PA_DAC_ENABLE               0x07
#define PA_DAC_DISABLE              0x04

// --- Endereçamento e Flags ---
#define BROADCAST_ADDRESS           255
#define FLAGS_ACK                   0x80

// --- Constantes Físicas ---
#define FXOSC                       32000000.0
#define FSTEP                       (FXOSC / 524288) // (FXOSC / 2^19)

// ============================================================================
// --- Tipos e Estruturas de Dados ---
// ============================================================================

/**
 * @brief Estrutura para armazenar dados de um pacote LoRa recebido.
 */
typedef struct {
    uint8_t message[252];   // Buffer para a mensagem (Payload max 255 - 4 bytes de header)
    uint8_t length;         // Comprimento da mensagem recebida
    uint8_t header_to;      // Endereço do destinatário
    uint8_t header_from;    // Endereço do remetente
    uint8_t header_id;      // ID da mensagem
    uint8_t header_flags;   // Flags da mensagem
    int rssi;               // Received Signal Strength Indicator
    float snr;              // Signal-to-Noise Ratio
} lora_payload_t;

/**
 * @brief Enumeração para configurações de modem predefinidas.
 */
typedef enum {
    BW125_CR45_SF128,  // Médio alcance (Padrão)
    BW500_CR45_SF128,  // Curto alcance, rápido
    BW31_25_CR48_SF512,// Longo alcance, lento
    BW125_CR48_SF4096, // Longo alcance, muito lento
} modem_config_t;

/**
 * @brief Estrutura de configuração para inicializar o módulo LoRa.
 */
typedef struct {
    spi_inst_t *spi_port;  // Instância do SPI (ex: spi0, spi1)
    uint interrupt_pin;    // Pino de interrupção (DIO0)
    uint cs_pin;           // Pino Chip Select (NSS)
    uint reset_pin;        // Pino de Reset (opcional, pode ser setado para um valor inválido se não usado)
    float freq;            // Frequência em MHz (ex: 868.0, 915.0)
    uint8_t tx_power;      // Potência de transmissão em dBm (entre 5 e 23)
    uint8_t this_address;  // Endereço deste nó LoRa (0-254)
    modem_config_t modem;  // Configuração do modem a ser usada
    bool receive_all;      // Se true, recebe pacotes de todos os endereços
    bool acks;             // Se true, habilita envio automático de ACKs
} lora_config_t;


// ============================================================================
// --- Protótipos das Funções Públicas ---
// ============================================================================

/**
 * @brief Inicializa o módulo LoRa e o hardware SPI do RP2040.
 *
 * @param config Ponteiro para a estrutura de configuração LoRa.
 * @return true se a inicialização for bem-sucedida, false caso contrário.
 */
bool lora_init(lora_config_t *config);

/**
 * @brief Define o modo de operação do rádio para modo Idle (Standby).
 */
void lora_set_mode_idle(void);

/**
 * @brief Define o modo de operação do rádio para recepção contínua.
 *        O rádio fica ouvindo por pacotes indefinidamente.
 */
void lora_set_mode_rx_continuous(void);

/**
 * @brief Define o modo de operação do rádio para transmissão.
 */
void lora_set_mode_tx(void);

/**
 * @brief Coloca o rádio no modo de baixo consumo (Sleep).
 */
void lora_sleep(void);

/**
 * @brief Envia um pacote de dados LoRa.
 *
 * Esta é uma função de envio básica que não espera por ACK.
 *
 * @param data Ponteiro para o buffer de dados a ser enviado.
 * @param length O comprimento dos dados a serem enviados.
 * @param header_to O endereço do nó de destino (use BROADCAST_ADDRESS para todos).
 */
void lora_send(const uint8_t *data, size_t length, uint8_t header_to);

/**
 * @brief Envia um pacote e aguarda por um Acknowledgement (ACK).
 *
 * @param data Ponteiro para o buffer de dados a ser enviado.
 * @param length O comprimento dos dados a serem enviados.
 * @param header_to O endereço do nó de destino.
 * @param retries O número de tentativas de envio caso o ACK não seja recebido.
 * @param retry_timeout_ms O timeout em milissegundos para esperar por um ACK.
 * @return true se o ACK foi recebido, false caso contrário.
 */
bool lora_send_to_wait(const uint8_t *data, size_t length, uint8_t header_to, int retries, uint32_t retry_timeout_ms);

/**
 * @brief Define uma função de callback para ser chamada quando um pacote é recebido.
 *
 * @param callback A função a ser chamada. O parâmetro da função é um ponteiro
 *                 para a estrutura `lora_payload_t` com os dados recebidos.
 */
void lora_on_receive(void (*callback)(lora_payload_t*));


/**
 * @brief Desinicializa a interface SPI.
 */
void lora_close(void);

#endif // LORA_H