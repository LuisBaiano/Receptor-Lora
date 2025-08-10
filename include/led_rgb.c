#include "pico/stdlib.h"
#include "led_rgb.h"
#include "config.h"

/**
 * @brief Inicializa os pinos GPIO definidos em config.h para serem saídas
 *        e controlar o LED RGB.
 */
void rgb_led_init() {
    // Inicializa cada pino de cor
    gpio_init(LED_RED_PIN);
    gpio_init(LED_GREEN_PIN);
    gpio_init(LED_BLUE_PIN);

    // Define a direção de cada pino como saída (OUTPUT)
    gpio_set_dir(LED_RED_PIN, GPIO_OUT);
    gpio_set_dir(LED_GREEN_PIN, GPIO_OUT);
    gpio_set_dir(LED_BLUE_PIN, GPIO_OUT);
    
    // Garante que o LED comece desligado
    rgb_led_set_color(COR_LED_DESLIGADO);
}

/**
 * @brief Define a cor do LED RGB com base na enumeração CorLed.
 * 
 * Esta implementação assume um LED RGB do tipo "Catodo Comum",
 * onde um nível lógico ALTO (1) acende o pino da cor.
 * 
 * @param cor A cor desejada do tipo CorLed.
 */
void rgb_led_set_color(CorLed cor) {
    // Inicializa o estado de cada pino como DESLIGADO (nível lógico 0)
    bool r = false;
    bool g = false;
    bool b = false;

    // Define quais pinos devem ser ligados com base na cor escolhida
    switch (cor) {
        case COR_LED_VERMELHO:
            r = true;
            break;

        case COR_LED_VERDE:
            g = true;
            break;

        case COR_LED_AZUL:
            b = true;
            break;

        case COR_LED_AMARELO: // Vermelho + Verde
            r = true;
            g = true;
            break;

        case COR_LED_CIANO: // Verde + Azul
            g = true;
            b = true;
            break;

        case COR_LED_MAGENTA: // Vermelho + Azul
            r = true;
            b = true;
            break;
            
        case COR_LED_DESLIGADO:
            // Todos já são 'false', nada a fazer.
            break;
    }

    // Aplica o estado lógico aos pinos GPIO físicos
    gpio_put(LED_RED_PIN, r);
    gpio_put(LED_GREEN_PIN, g);
    gpio_put(LED_BLUE_PIN, b);
}