#ifndef DISPLAY_H
#define DISPLAY_H

#include "lib/ssd1306/ssd1306.h"
#include "lib/ssd1306/font.h"
#include <stdint.h>

/**
 * @brief Inicializa o display OLED via I2C.
 * @param ssd Ponteiro para a instância do objeto ssd1306_t.
 */
void display_init(ssd1306_t *ssd);

/**
 * @brief Exibe uma tela de boas-vindas para o receptor.
 * @param ssd Ponteiro para a instância do objeto ssd1306_t.
 */
void display_startup_screen(ssd1306_t *ssd);

/**
 * @brief Exibe uma tela indicando que o sistema está aguardando dados.
 * @param ssd Ponteiro para a instância do objeto ssd1306_t.
 */
void display_wait_screen(ssd1306_t *ssd);

/**
 * @brief Atualiza a tela com os dados recebidos via LoRa.
 *
 * @param ssd Ponteiro para a instância ssd1306_t.
 * @param temp Temperatura recebida.
 * @param hum Umidade recebida.
 * @param pres Pressão recebida (em hPa).
 * @param rssi RSSI (força do sinal) do último pacote.
 * @param packets Contagem total de pacotes recebidos.
 */
void display_update_data(ssd1306_t *ssd, float temp, float hum, float pres, int rssi, uint32_t packets);

#endif // DISPLAY_Hs