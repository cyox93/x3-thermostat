#pragma once

/*
 * main-board rev A (ESP32-S3-WROOM-1-N16R8) pin map.
 *
 * Source of truth: the main-board rev A header / FFC assignment.
 * Mirrored in docs/hardware.md — change both together.
 *
 * GPIO 33~37 are wired to the octal PSRAM inside the N16R8 module and must
 * never be used. GPIO 0/3/45/46 are strapping pins, GPIO 19/20 are USB.
 */

/* --- Panel via main FFC → x3_uart_board → panel FFC ------------------- */

/* GC9A01A LCD. IO10~12 are the FSPI IOMUX pins, so SPI2 runs without the
 * GPIO matrix and can use the highest clock. */
#define BOARD_LCD_SPI_HOST      SPI2_HOST
#define BOARD_LCD_PIN_MOSI      11
#define BOARD_LCD_PIN_SCLK      12
#define BOARD_LCD_PIN_CS        10
#define BOARD_LCD_PIN_DC        9
#define BOARD_LCD_PIN_RST       17

/* LCD backlight and LED ring enable share one net (X3 CON1-9). PWM dims the
 * LCD; the ring only sees on/off. 0 turns both off. */
#define BOARD_BL_RING_PIN       14
/* LED ring colour, valid while the ring is enabled. LOW = red, HIGH = white. */
#define BOARD_RING_COLOR_PIN    47

/* Rotary encoder (rotation only, no push). 1 kΩ series R9/R10 on the board. */
#define BOARD_ENC_A_PIN         38
#define BOARD_ENC_B_PIN         48

/* X3 CON1-1. Purpose unknown — static HIGH on the original X3 board.
 * Routed to an ADC1 pin so it can be observed. */
#define BOARD_UNKNOWN_PIN       8

/* --- x3-switch board (AT protocol over UART) ------------------------- */

#define BOARD_X3SWITCH_UART_NUM UART_NUM_1
#define BOARD_X3SWITCH_TX_PIN   18      /* ESP TX → x3-switch RX */
#define BOARD_X3SWITCH_RX_PIN   13      /* ESP RX ← x3-switch TX */
#define BOARD_X3SWITCH_BAUD     115200

/* --- Header J2 → dt200-plc-module J4 ---------------------------------- */

/* /SCTL must be driven open-drain only: the module pulls it to +5V. */
#define BOARD_PLC_SCTL_PIN      39
/* DOUT_BUF: HIGH = carrier detected (3.3 V level via module R23). */
#define BOARD_PLC_DOUT_PIN      40

/* --- Header J2 → AHT25 ------------------------------------------------ */

#define BOARD_I2C_SDA_PIN       41
#define BOARD_I2C_SCL_PIN       42

/* --- Header J2 → ALS-PT19-315C ambient light sensor ------------------- */

/* Analog photocurrent across a load resistor. Must be an ADC1 pin: ADC2
 * (GPIO11~20) is unusable while WiFi is on. IO1 = ADC1_CH0, J2-12. */
#define BOARD_ALS_PIN           1
/* Load resistor on the sensor side (not on the main-board). Sets both the
 * lux scale and the ceiling — change it here when the fitted value changes.
 * 100 kΩ: ~15 mV/lux so night levels (1~10 lux) sit well above ADC noise;
 * saturates around ~190 lux, which only ever means "room lights are on". */
#define BOARD_ALS_LOAD_OHM      100000

/* --- On-board --------------------------------------------------------- */

#define BOARD_RGB_LED_PIN       21      /* WS2812B-0807 */
#define BOARD_BOOT_PIN          0
