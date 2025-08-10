#ifndef RGB_LED_H
#define RGB_LED_H

#include <stdbool.h>

// Enum para as cores, facilitando o uso
typedef enum {
    COR_LED_DESLIGADO,
    COR_LED_VERMELHO,
    COR_LED_VERDE,
    COR_LED_AZUL,
    COR_LED_AMARELO,
    COR_LED_CIANO,
    COR_LED_MAGENTA
} CorLed;

/**
 * @brief Inicializa os pinos GPIO para o LED RGB.
 */
void rgb_led_init();

/**
 * @brief Define a cor do LED RGB.
 * @param cor A cor desejada da enumeração CorLed.
 */
void rgb_led_set_color(CorLed cor);

#endif // RGB_LED_H