#include "pico/stdlib.h"
#include <stdio.h>
#include <string.h>

// Includes dos periféricos do Pico SDK
#include "hardware/spi.h"
#include "hardware/i2c.h"
#include "hardware/gpio.h"

// Nossos próprios arquivos de cabeçalho
#include "include/config.h"
#include "include/lora.h"
#include "include/display.h"
#include "include/led_rgb.h"

// --- Variáveis Globais ---
// Instância principal para o objeto do display
ssd1306_t display;

// Estrutura para armazenar um conjunto de dados recebidos
typedef struct {
    float temperatura;
    float umidade;
    float pressao;
} DadosRecebidos_t;

// Variáveis 'volatile' para comunicação segura entre a interrupção (ISR) e o loop principal
volatile bool novos_dados_recebidos = false;
volatile int ultimo_rssi = 0;
volatile uint32_t pacotes_recebidos = 0;
DadosRecebidos_t dados_atuais = {0.0f, 0.0f, 0.0f}; // Zera os dados na inicialização

// --- FUNÇÕES DE INICIALIZAÇÃO DE HARDWARE ---

/**
 * @brief Inicializa o barramento I2C1 para o display OLED.
 */
void setup_i2c_display() {
    i2c_init(I2C_PORT, I2C_BAUDRATE);
    gpio_set_function(I2C_SDA_PIN, GPIO_FUNC_I2C);
    gpio_set_function(I2C_SCL_PIN, GPIO_FUNC_I2C);
    gpio_pull_up(I2C_SDA_PIN);
    gpio_pull_up(I2C_SCL_PIN);
    printf("I2C1 (Display) inicializado nos pinos SDA=%d, SCL=%d.\n", I2C_SDA_PIN, I2C_SCL_PIN);
}

/**
 * @brief Inicializa o barramento SPI0 e os pinos GPIO associados para o LoRa.
 * ESTA FUNÇÃO É A CORREÇÃO CRÍTICA.
 */
void setup_spi_lora() {
    // Inicializa o periférico SPI em si
    spi_init(LORA_SPI_PORT, 5 * 1000 * 1000); // 5 MHz

    // Informa aos pinos GPIO para serem controlados pelo periférico SPI
    gpio_set_function(LORA_SCK_PIN, GPIO_FUNC_SPI);
    gpio_set_function(LORA_MOSI_PIN, GPIO_FUNC_SPI);
    gpio_set_function(LORA_MISO_PIN, GPIO_FUNC_SPI);
    
    printf("SPI0 (LoRa) inicializado nos pinos SCK=%d, MOSI=%d, MISO=%d.\n", LORA_SCK_PIN, LORA_MOSI_PIN, LORA_MISO_PIN);
}


// --- FUNÇÃO DE CALLBACK DO LORA ---
/**
 * @brief É chamada AUTOMATICAMENTE pela biblioteca LoRa via interrupção
 *        quando um pacote válido é recebido.
 * @param payload Ponteiro para a estrutura com os dados recebidos.
 */
void on_lora_receive(lora_payload_t* payload) {
    // Tenta decodificar a string no formato "T:25.1,H:45.0,P:1012.5"
    // sscanf retorna o número de variáveis preenchidas com sucesso.
    int items_parsed = sscanf((const char*)payload->message, "T:%f,H:%f,P:%f",
                                &dados_atuais.temperatura,
                                &dados_atuais.umidade,
                                &dados_atuais.pressao);

    // Só considera os dados válidos se todos os 3 valores foram encontrados
    if (items_parsed == 3) {
        ultimo_rssi = payload->rssi;
        pacotes_recebidos++;
        // Sinaliza ao loop principal que há dados novos e válidos para processar
        novos_dados_recebidos = true; 
    } else {
        // Ignora pacotes malformados, mas avisa no console para debug
        printf("WARN: Pacote LoRa recebido com formato inesperado: %.*s\n", payload->length, payload->message);
    }
}


// --- FUNÇÃO PRINCIPAL ---

int main() {
    // Inicializa a comunicação serial via USB para debug
    stdio_init_all();
    sleep_ms(3000); // Pausa para dar tempo de conectar o monitor serial

    // --- 1. Inicialização dos Periféricos de Hardware ---
    printf("--- Iniciando Hardware do Receptor ---\n");
    rgb_led_init();
    setup_i2c_display();
    setup_spi_lora(); // <<< Chamada da função de correção
    printf("--------------------------------------\n\n");

    // --- 2. Inicialização dos Drivers e Módulos de Software ---
    display_init(&display);
    rgb_led_set_color(COR_LED_AMARELO); // Sinaliza "inicializando"
    display_startup_screen(&display);   // Mostra tela de boas-vindas

    // Prepara a configuração para o módulo LoRa
    lora_config_t config = {
        .spi_port = LORA_SPI_PORT,
        .interrupt_pin = LORA_INTERRUPT_PIN,
        .cs_pin = LORA_CS_PIN,
        .reset_pin = LORA_RESET_PIN,
        .freq = LORA_FREQUENCY,
        .tx_power = LORA_TX_POWER,
        .this_address = LORA_ADDRESS_RECEIVER
    };

    // Inicializa o LoRa. Se falhar, é um erro fatal.
    if (!lora_init(&config)) {
        printf("ERRO FATAL: Falha na inicializacao do LoRa.\n");
        rgb_led_set_color(COR_LED_VERMELHO);
        // Você poderia mostrar um erro no display aqui também
        while (1); // Trava o programa
    }
     
    // --- 3. Finaliza a configuração e entra em modo de operação ---
    lora_on_receive(on_lora_receive); // Registra a função de callback
    
    printf("Inicializacao completa. Endereco: #%d. Aguardando pacotes...\n", LORA_ADDRESS_RECEIVER);
    rgb_led_set_color(COR_LED_AZUL);   // Sinaliza "pronto e aguardando"
    display_wait_screen(&display);     // Mostra tela de espera

    // --- 4. Loop Principal Infinito ---
    while (1) {
        // Verifica se a rotina de interrupção sinalizou novos dados
        if (novos_dados_recebidos) {
            
            // Variáveis locais para armazenar uma cópia segura dos dados
            DadosRecebidos_t dados_copiados;
            int rssi_copiado;
            uint32_t contador_copiado;
            
            // --- Seção Crítica ---
            // Desabilita a interrupção do LoRa temporariamente para evitar que
            // as variáveis globais sejam modificadas enquanto as copiamos.
            gpio_set_irq_enabled(LORA_INTERRUPT_PIN, GPIO_IRQ_EDGE_RISE, false);
            
            novos_dados_recebidos = false;        // Reseta a flag
            dados_copiados = dados_atuais;        // Copia a estrutura de dados
            rssi_copiado = ultimo_rssi;           // Copia o RSSI
            contador_copiado = pacotes_recebidos; // Copia o contador

            // Reabilita a interrupção. O tempo desativado é mínimo.
            gpio_set_irq_enabled(LORA_INTERRUPT_PIN, GPIO_IRQ_EDGE_RISE, true);
            // --- Fim da Seção Crítica ---
            
            
            // A partir daqui, trabalhamos apenas com as cópias, que são seguras.
            
            // 1. Feedback visual para o usuário
            rgb_led_set_color(COR_LED_VERDE);
            
            // 2. Atualiza o display OLED com os dados copiados
            display_update_data(&display,
                                dados_copiados.temperatura,
                                dados_copiados.umidade,
                                dados_copiados.pressao,
                                rssi_copiado,
                                contador_copiado);
            
            // 3. Imprime um log no console para debug
            printf("Pacote #%lu | T:%.1f, H:%.0f, P:%.1f | RSSI: %d\n",
                   contador_copiado, dados_copiados.temperatura,
                   dados_copiados.umidade, dados_copiados.pressao, rssi_copiado);
            
            // 4. Retorna o LED à cor de "pronto" após um breve piscar
            sleep_ms(100);
            rgb_led_set_color(COR_LED_AZUL);
        }

        // Deixa a CPU em um loop de baixa energia. A interrupção do LoRa a acordará.
        tight_loop_contents();
    }

    return 0; // Esta linha nunca será alcançada
}