#include <stdio.h>
#include <string.h>
#include "pico/stdlib.h"
#include "hardware/i2c.h"

// Cabeçalhos do nosso projeto
#include "display.h"
#include "config.h"

/**
 * @brief Inicializa o objeto do display SSD1306.
 * A inicialização do hardware I2C é feita separadamente no main.
 */
void display_init(ssd1306_t *ssd) {
    // Inicializa o objeto ssd1306, associando-o ao barramento I2C correto
    ssd1306_init(ssd, DISPLAY_WIDTH, DISPLAY_HEIGHT, false, DISPLAY_I2C_ADDR, I2C_PORT);

    // Envia a sequência de comandos de configuração para o display
    ssd1306_config(ssd);

    // Limpa o buffer interno e atualiza a tela
    ssd1306_fill(ssd, false);
    ssd1306_send_data(ssd);
    printf("Display inicializado.\n");
}

/**
 * @brief Exibe uma tela de boas-vindas no momento da inicialização.
 */
void display_startup_screen(ssd1306_t *ssd) {
    ssd1306_fill(ssd, false);
    const char *line1 = "Receptor LoRa";
    const char *line2 = "Atividade 14";
    
    // Centraliza o texto horizontalmente
    uint8_t center_x = ssd->width / 2;
    uint8_t pos_x1 = center_x - (strlen(line1) * 8) / 2;
    uint8_t pos_x2 = center_x - (strlen(line2) * 8) / 2;
    
    ssd1306_draw_string(ssd, line1, pos_x1, 16);
    ssd1306_draw_string(ssd, line2, pos_x2, 36);
    
    ssd1306_send_data(ssd);
    sleep_ms(2000); // Mostra a mensagem por 2 segundos
}

/**
 * @brief Exibe uma tela indicando que o sistema está pronto e esperando pacotes.
 */
void display_wait_screen(ssd1306_t *ssd) {
    ssd1306_fill(ssd, false);
    const char *line1 = "Aguardando...";
    
    // Centraliza o texto
    uint8_t center_x = ssd->width / 2;
    uint8_t pos_x1 = center_x - (strlen(line1) * 8) / 2;

    ssd1306_draw_string(ssd, line1, pos_x1, 28);
    ssd1306_send_data(ssd);
}

/**
 * @brief Atualiza a tela com os dados de telemetria recebidos.
 */
void display_update_data(ssd1306_t *ssd, float temp, float hum, float pres, int rssi, uint32_t packets) {
    char buffer[32]; // Buffer para formatar as strings
    ssd1306_fill(ssd, false); // Limpa o buffer antes de desenhar o novo conteúdo

    // Linha 1: Temperatura e Umidade
    // Formata a string "T:25.1C H:45%"
    snprintf(buffer, sizeof(buffer), "T:%.1fC H:%.0f%%", temp, hum);
    ssd1306_draw_string(ssd, buffer, 2, 0);

    // Linha 2: Pressão
    // Formata a string "P: 1012.3 hPa"
    snprintf(buffer, sizeof(buffer), "P: %.1f hPa", pres);
    ssd1306_draw_string(ssd, buffer, 2, 16);

    // Linha 3: Força do sinal (RSSI)
    // Formata a string "Sinal (RSSI): -58"
    snprintf(buffer, sizeof(buffer), "RSSI: %d", rssi);
    ssd1306_draw_string(ssd, buffer, 2, 32);
    
    // Linha 4: Contador de pacotes recebidos
    // Formata a string "Pacotes: #123"
    snprintf(buffer, sizeof(buffer), "Pacotes: #%lu", packets);
    ssd1306_draw_string(ssd, buffer, 2, 48);

    // Envia todo o buffer de uma vez para o display
    ssd1306_send_data(ssd);
}