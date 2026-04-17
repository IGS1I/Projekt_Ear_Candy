#pragma once

#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "esp_err.h"
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * SPI host + pin assignments
 * ========================================================================= */

#define VS1053_SPI_HOST  SPI2_HOST

#define VS1053_PIN_MOSI  GPIO_NUM_23
#define VS1053_PIN_MISO  GPIO_NUM_19
#define VS1053_PIN_SCK   GPIO_NUM_18
#define VS1053_PIN_SD_CS GPIO_NUM_5   /* SD card CS — drive HIGH when not in use */
#define VS1053_PIN_XCS   GPIO_NUM_4   /* SCI chip select  (commands)  */

/*
 * WROVER WARNING — do NOT use GPIO16 or GPIO17 for any VS1053 pin.
 * On the ESP32-WROVER those two lines are hardwired to the PSRAM chip internally.
 * Connecting anything else to them causes PSRAM corruption and/or a boot crash.
 * XDCS and DREQ were previously wired to GPIO16/17 and produced no audio output
 * because of this conflict — move them to GPIO25 and GPIO26 instead (see below).
 */
#define VS1053_PIN_XDCS  GPIO_NUM_25  /* SDI chip select  (audio data) — was GPIO16, do NOT go back */
#define VS1053_PIN_DREQ  GPIO_NUM_26  /* Data REQuest — high = chip ready for more data — was GPIO17 */
#define VS1053_PIN_XRST  GPIO_NUM_21  /* Active-low reset */

/* ============================================================================
 * SCI register addresses  (write with VS_SCI_WRITE opcode 0x02,
 *                           read  with VS_SCI_READ  opcode 0x03)
 * ========================================================================= */

#define VS_REG_MODE         0x00  /* Mode control — main feature flags              */
#define VS_REG_STATUS       0x01  /* Chip status + version (bits 4-7 = chip ID)     */
#define VS_REG_BASS         0x02  /* Built-in bass / treble enhancer                */
#define VS_REG_CLOCKF       0x03  /* Clock multiplier  (0x6000 = 3× = ~36 MHz)      */
#define VS_REG_DECODE_TIME  0x04  /* Seconds decoded so far — write 0 to reset      */
#define VS_REG_AUDATA       0x05  /* Sample rate & channel config                   */
#define VS_REG_WRAM         0x06  /* WRAM read/write data port                      */
#define VS_REG_WRAMADDR     0x07  /* WRAM address to access via VS_REG_WRAM         */
#define VS_REG_HDAT0        0x08  /* Bitstream header word 0 (frame info)           */
#define VS_REG_HDAT1        0x09  /* Bitstream header word 1 (sync word etc.)       */
#define VS_REG_AIADDR       0x0A  /* Start address of user application in WRAM      */
#define VS_REG_VOL          0x0B  /* Volume: high byte = left, low byte = right     */
                                  /*   0x00 = max volume,  0xFE = silent (0.5dB/step)*/
#define VS_REG_AICTRL0      0x0C  /* Application control register 0                 */
#define VS_REG_AICTRL1      0x0D  /* Application control register 1                 */
#define VS_REG_AICTRL2      0x0E  /* Application control register 2                 */
#define VS_REG_AICTRL3      0x0F  /* Application control register 3                 */

/* ============================================================================
 * MODE register (VS_REG_MODE = 0x00) individual bit flags
 *
 * After hard reset the chip boots with MODE = 0x0800 (SM_SDINEW set).
 * ========================================================================= */

#define VS_MODE_SM_DIFF          0x0001  /* Invert left channel → differential output  */
#define VS_MODE_SM_LAYER12       0x0002  /* Allow MPEG Layer I and II decoding         */
#define VS_MODE_SM_RESET         0x0004  /* Soft reset — chip clears this when done    */
#define VS_MODE_SM_CANCEL        0x0008  /* Cancel current decode stream               */
#define VS_MODE_SM_EARSPEAKER_LO 0x0010  /* EarSpeaker 3D effect — low setting         */
#define VS_MODE_SM_TESTS         0x0020  /* Enable SDI test mode (sine / memory tests) */
#define VS_MODE_SM_STREAM        0x0040  /* Stream mode — skip VBR table of contents   */
#define VS_MODE_SM_EARSPEAKER_HI 0x0080  /* EarSpeaker 3D effect — high setting        */
#define VS_MODE_SM_DACT          0x0100  /* DCLK active edge (0 = rising)              */
#define VS_MODE_SM_SDIORD        0x0200  /* SDI bit order (0 = MSB first)              */
#define VS_MODE_SM_SDISHARE      0x0400  /* Share SPI chip select between SCI and SDI  */
#define VS_MODE_SM_SDINEW        0x0800  /* Native VS1053 SPI mode — always set        */
#define VS_MODE_SM_ADPCM         0x1000  /* ADPCM recording active                     */
#define VS_MODE_SM_LINE1         0x4000  /* Analog input: 0 = microphone, 1 = LINE1    */
#define VS_MODE_SM_CLK_RANGE     0x8000  /* Set if input clock is 12–13 MHz range      */

/* ============================================================================
 * SDI streaming constant
 *
 * DREQ high guarantees room for at least 32 bytes in the chip's 2 KB buffer.
 * Always check DREQ before every 32-byte SDI write — never send more at once.
 * ========================================================================= */

#define VS_SDI_CHUNK_BYTES  32

/* ============================================================================
 * Public API
 * ========================================================================= */

/*
 * Initialize SPI bus, SCI + SDI devices, GPIOs, hard-reset the chip,
 * set clock multiplier to 3×, and set volume to maximum.
 * Call this once in app_main() before anything else.
 */
esp_err_t vs1053_init(void);

/*
 * Audio subsystem initialization entry point used by app_main().
 * Currently wraps vs1053_init() and keeps app code decoupled from
 * chip-specific naming.
 */
esp_err_t audio_processor_init(void);

/* Audio command API (Part 1). */
esp_err_t audio_play_memory(const uint8_t *data, size_t len);
esp_err_t audio_play_sample(void);
esp_err_t audio_pause(void);
esp_err_t audio_resume(void);
esp_err_t audio_stop(void);
esp_err_t audio_volume_up(void);
esp_err_t audio_volume_down(void);
bool audio_is_chip_connected(void);
esp_err_t audio_force_mp3_mode(void);

/*
 * Read / write any SCI register by address (use VS_REG_* defines above).
 */
esp_err_t vs1053_read_reg(uint8_t reg, uint16_t *value);
esp_err_t vs1053_write_reg(uint8_t reg, uint16_t value);

/*
 * Soft-reset the VS1053 decoder (clears current stream state).
 * Waits for DREQ to go high again then restores the clock multiplier.
 * Use this if the decoder gets into a bad state mid-stream.
 */
esp_err_t vs1053_soft_reset(void);

/*
 * Cancel the currently playing stream cleanly.
 * Sets SM_CANCEL, feeds zeros until the chip acknowledges, then stops.
 * Falls back to soft reset if the chip doesn't respond within 2 KB of zeros.
 */
esp_err_t vs1053_cancel(void);

/*
 * Set output volume per channel.
 *   vol_left / vol_right: 0x00 = maximum,  0xFE = silent  (steps of 0.5 dB)
 * Example: vs1053_set_volume(0x00, 0x00) → full volume both channels.
 *          vs1053_set_volume(0x20, 0x20) → ~16 dB attenuation.
 */
esp_err_t vs1053_set_volume(uint8_t vol_left, uint8_t vol_right);

/*
 * Map a potentiometer / ADC reading to volume and apply it.
 *   adc_value : raw ADC count from your dial (0 = dial fully down)
 *   adc_max   : maximum count your ADC produces (e.g. 4095 for 12-bit)
 * Turning the dial to max → 0x00 attenuation (loudest).
 * Turning it to min       → 0xFE attenuation (silent).
 */
esp_err_t vs1053_set_volume_dial(uint16_t adc_value, uint16_t adc_max);

/*
 * Stream a buffer of audio data (MP3, OGG, WAV, etc.) from RAM or flash.
 * Sends in 32-byte chunks, checking DREQ before each one.
 * Blocks until all bytes have been sent.
 *
 * To play the built-in sample:
 *   vs1053_play_buffer(kSampleMp3, kSampleMp3Size);
 *
 * To use your own MP3 converted to a C array (run on your Mac):
 *   xxd -i song.mp3 > sample_mp3.h
 * Then rename the generated array/size variables to kSampleMp3 / kSampleMp3Size.
 */
void vs1053_play_buffer(const uint8_t *data, size_t len);

/*
 * Continuous sine generator test.
 * Initializes the chip, enables SM_TESTS, starts ~1 kHz sine,
 * and keeps it running until board reset / power cycle.
 */
void vs1053_sine_continuous_test(void);

/*
 * Full hardware bring-up and self-test sequence.
 * Runs vs1053_init(), plays a 1 kHz sine wave for 4 s, then streams the
 * built-in flash sample. Useful for verifying wiring before app integration.
 */
void vs1053_bringup_test(void);

#ifdef __cplusplus
}
#endif
