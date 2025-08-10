#ifndef CONFIG_H
#define CONFIG_H

// --- CONFIGURAÇÃO I2C ---
#define I2C_BAUDRATE        (400 * 1000)

// --- Barramento I2C 1: Para o Display OLED ---
#define I2C1_PORT           i2c1
#define I2C1_SDA_PIN        14
#define I2C1_SCL_PIN        15
#define DISPLAY_I2C_ADDR    0x3C
#define DISPLAY_WIDTH       128
#define DISPLAY_HEIGHT      64

// --- CONFIGURAÇÃO GPIO para o LED RGB ---
#define LED_RED_PIN        13 
#define LED_GREEN_PIN      11 
#define LED_BLUE_PIN       12 


// --- Configurações do LoRa (COM PINOS CORRIGIDOS) ---
#define LORA_SPI_PORT       spi0
#define LORA_SCK_PIN        19  // pino físico 9
#define LORA_MOSI_PIN       18  // pino físico 10
#define LORA_MISO_PIN       4  // pino físico 6
#define LORA_CS_PIN         9  // PINO CORRIGIDO (ex: GPIO9, pino físico 12)
#define LORA_INTERRUPT_PIN  16  // DIO0
#define LORA_RESET_PIN      17 // PINO CORRIGIDO (ex: GPIO10, pino físico 14)


// Endereços LoRa (0-255, onde 255 é broadcast)
#define LORA_ADDRESS_TRANSMITTER 42
#define LORA_ADDRESS_RECEIVER 43

#endif // CONFIG_H