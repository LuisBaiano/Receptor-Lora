#include "pico/stdlib.h"
#include <stdio.h>
#include <string.h>
#include "hardware/i2c.h"
#include "include/config.h"
#include "include/lora.h"
#include "include/display.h"
#include "include/led_rgb.h"
// Instância do objeto do display
ssd1306_t display;

// Estrutura para armazenar o último conjunto de dados recebido
struct DadosRecebidos {
    float temperatura;
    float umidade;
    float pressao;
    int rssi;
};

// Variável para guardar os dados. O 'volatile' é essencial para a comunicação segura
// entre a interrupção (callback) e o loop principal.
volatile static struct DadosRecebidos ultimo_dado;
volatile static bool novos_dados_recebidos = false;
static uint32_t contador_pacotes = 0;


/**
 * @brief Função de callback. É chamada AUTOMATICAMENTE pela biblioteca LoRa
 *        quando um pacote válido é recebido. (Executada via interrupção)
 */
void on_lora_receive(lora_payload_t* payload) {
    // 1. Tenta extrair os dados da string recebida
    // O transmissor envia "T:25.1,H:45,P:1012.3"
    sscanf(payload->message, "T:%f,H:%f,P:%f",
           &ultimo_dado.temperatura,
           &ultimo_dado.umidade,
           &ultimo_dado.pressao);

    // 2. Armazena os metadados do pacote
    ultimo_dado.rssi = payload->rssi;
    contador_pacotes++;

    // 3. Sinaliza ao loop principal que há novos dados para processar
    novos_dados_recebidos = true;
}

// Função para inicializar o barramento I2C para o display
void setup_i2c() {
    i2c_init(I2C1_PORT, I2C_BAUDRATE);
    gpio_set_function(I2C1_SDA_PIN, GPIO_FUNC_I2C);
    gpio_set_function(I2C1_SCL_PIN, GPIO_FUNC_I2C);
    gpio_pull_up(I2C1_SDA_PIN);
    gpio_pull_up(I2C1_SCL_PIN);
}

int main() {
    // Inicializa E/S padrão para debug via USB
    stdio_init_all();
    sleep_ms(3000); // Pausa para dar tempo de abrir o monitor serial

    // --- Inicialização dos Periféricos ---
    rgb_led_init();
    setup_i2c();
    display_init(&display);
    
    // Mostra a tela de boas-vindas
    display_startup_screen(&display);
    rgb_led_set_color(COR_LED_AMARELO); // Amarelo enquanto inicializa

    // --- Inicialização do LoRa ---
    lora_config_t config = {
        .spi_port = LORA_SPI_PORT,
        .interrupt_pin = LORA_INTERRUPT_PIN,
        .cs_pin = LORA_CS_PIN,
        .reset_pin = LORA_RESET_PIN,
        .freq = 868.0,
        .this_address = LORA_ADDRESS_RECEIVER
    };

    if (lora_init(&config)) {
        printf("Receptor LoRa inicializado. Endereco: %d\n", LORA_ADDRESS_RECEIVER);
        rgb_led_set_color(COR_LED_AZUL); // Azul indica "pronto e aguardando"
        display_wait_screen(&display);
    } else {
        printf("ERRO FATAL: Falha na inicializacao do LoRa.\n");
        rgb_led_set_color(COR_LED_VERMELHO); // Vermelho indica erro fatal
        // Trava em um loop infinito
        while (1);
    }
    
    // Registra nossa função para ser chamada na chegada de pacotes
    lora_on_receive(on_lora_receive);
    printf("Aguardando pacotes...\n");

    // --- Loop Principal ---
    // Este loop apenas verifica a flag. O recebimento acontece em segundo plano.
    while (1) {
        // Verifica se a interrupção sinalizou a chegada de novos dados
        if (novos_dados_recebidos) {
            
            // Trava e reseta a flag para evitar reprocessamento do mesmo dado
            // Este pequeno bloco garante que a operação é "atômica"
            // e segura contra interrupções.
            gpio_set_irq_enabled(LORA_INTERRUPT_PIN, GPIO_IRQ_EDGE_RISE, false);
            novos_dados_recebidos = false; 
            gpio_set_irq_enabled(LORA_INTERRUPT_PIN, GPIO_IRQ_EDGE_RISE, true);
            
            // --- Feedback Visual e Lógico ---
            
            // 1. Pisca o LED Verde para indicar recebimento
            rgb_led_set_color(COR_LED_VERDE);
            sleep_ms(100); // Mantém o LED verde por 100ms
            rgb_led_set_color(COR_LED_AZUL); // Retorna à cor de "pronto"
            
            // 2. Imprime os dados no console serial para debug
            printf("Pacote #%lu | T: %.1f, H: %.0f, P: %.1f | RSSI: %d\n",
                   contador_pacotes, ultimo_dado.temperatura,
                   ultimo_dado.umidade, ultimo_dado.pressao, ultimo_dado.rssi);

            // 3. Atualiza o display OLED com os novos dados
            display_update_data(&display,
                                ultimo_dado.temperatura,
                                ultimo_dado.umidade,
                                ultimo_dado.pressao,
                                ultimo_dado.rssi,
                                contador_pacotes);
        }

        // Deixa a CPU dormir se não houver nada para fazer, economizando energia.
        // A interrupção do LoRa acordará o sistema quando necessário.
        tight_loop_contents();
    }

    return 0;
}