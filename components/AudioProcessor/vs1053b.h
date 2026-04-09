#pragma once

#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Shared SPI (SD card + VS10xx SCI/SDI must use same bus pins in hardware).
 * HSPI / SPI2 is typical on ESP32 when display uses VSPI.
 */
#define VS1053_SPI_HOST SPI2_HOST

/* Use GPIO_NUM_* so C++ calls like gpio_get_level() accept gpio_num_t without casts. */
#define VS1053_PIN_MOSI  GPIO_NUM_23
#define VS1053_PIN_MISO  GPIO_NUM_19
#define VS1053_PIN_SCK   GPIO_NUM_18

/** SD card SPI chip select — must be driven HIGH before VS reset if SPI is shared. */
#define VS1053_PIN_SD_CS GPIO_NUM_5

#define VS1053_PIN_XCS  GPIO_NUM_4
/*
 * Do NOT use GPIO 16/17 for VS1053 on ESP32-WROVER when CONFIG_SPIRAM is on — those lines
 * are used for PSRAM and the driver already owns them (gpio: conflict + possible crash).
 * Defaults below are safe on typical WROVER dev boards; change if your PCB routes differently.
 */
#define VS1053_PIN_XDCS  GPIO_NUM_25
#define VS1053_PIN_DREQ  GPIO_NUM_26
#define VS1053_PIN_XRST  GPIO_NUM_21

/**
 * Minimal VS1003/VS1053 SCI bring-up: SPI, GPIO, reset, DREQ wait, register read/write test.
 * On failure (DREQ stuck low), logs in a loop so you can probe wiring without WDT spin.
 */
void vs1053_bringup_test(void);

#ifdef __cplusplus
}
#endif
