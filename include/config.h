#ifndef CONFIG_H
#define CONFIG_H

// --- CONFIGURAÇÃO I2C PARA DISPLAY OLED ---
#define I2C_PORT           i2c1
#define I2C_SDA_PIN        14
#define I2C_SCL_PIN        15
#define I2C_BAUDRATE       (400 * 1000)
#define DISPLAY_I2C_ADDR   0x3C
#define DISPLAY_WIDTH      128
#define DISPLAY_HEIGHT     64

// --- CONFIGURAÇÃO GPIO PARA O LED RGB ---
#define LED_RED_PIN        13 
#define LED_GREEN_PIN      11 
#define LED_BLUE_PIN       12 


// ==========================================================
// --- Configurações do LoRa (COM PINOS E PARÂMETROS FINAIS) ---
#define LORA_SPI_PORT       spi0
#define LORA_SCK_PIN        18
#define LORA_MOSI_PIN       19
#define LORA_MISO_PIN       16
#define LORA_CS_PIN         17
#define LORA_INTERRUPT_PIN  8  // DIO0
#define LORA_RESET_PIN      20

// --- Parâmetros da Comunicação LoRa (Devem ser iguais aos do transmissor) ---
#define LORA_FREQUENCY      915.0 // <<< Parâmetro centralizado
#define LORA_TX_POWER       20    // <<< Parâmetro centralizado

// --- Endereços LoRa ---
#define LORA_ADDRESS_TRANSMITTER 1
#define LORA_ADDRESS_RECEIVER    2 // << Endereço deste dispositivo

#endif // CONFIG_H`