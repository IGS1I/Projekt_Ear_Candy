/*
 * VS10xx (VS1003/VS1053 family) driver — SPI bring-up, init, and audio tests.
 *
 * Two SPI devices share the same bus (SCK/MOSI/MISO):
 *   SCI  (XCS)  — sends commands to control registers inside the chip (volume, mode, clock, etc.)
 *   SDI  (XDCS) — sends raw audio/MP3 data for the chip's decoder to play
 *
 * Pin assignments are in vs1053b.h. Do not use GPIO16/17 on WROVER boards — those are PSRAM.
 */
#include "vs1053b.h"
#include "sample_mp3.h"

#include <string.h>

#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "esp_err.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

/* SCI framing: first byte of every 4-byte transaction is either READ or WRITE. */
#define VS_SCI_READ   0x03
#define VS_SCI_WRITE  0x02

/* VS10xx SCI register addresses. */
#define VS_REG_MODE    0x00  /* operating mode flags */
#define VS_REG_STATUS  0x01  /* chip status and version */
#define VS_REG_CLOCKF  0x03  /* clock multiplier */
#define VS_REG_VOL     0x0B  /* volume (0x00 = max, 0xFE = silent, per channel) */

static const char *TAG = "vs1053";

/* SCI device handle — commands go here (chip select: XCS). */
static spi_device_handle_t s_vs_sci;

/* SDI device handle — audio/MP3 data goes here (chip select: XDCS). */
static spi_device_handle_t s_vs_sdi;

/*
 * Block until DREQ goes high (chip is ready) or timeout expires.
 * DREQ low means the VS10xx's internal buffer is full — do not send data yet.
 */
static esp_err_t wait_dreq_high(uint32_t timeout_ms)
{
    for (uint32_t i = 0; i < timeout_ms; i++) {
        if (gpio_get_level(VS1053_PIN_DREQ) == 1) {
            return ESP_OK;
        }
        vTaskDelay(pdMS_TO_TICKS(1));
    }
    return ESP_ERR_TIMEOUT;
}

/*
 * Hard-reset the VS10xx by pulsing XRST low then high.
 * After reset, DREQ goes high once the chip has finished its internal boot sequence.
 */
static esp_err_t hard_reset(void)
{
    ESP_LOGI(TAG, "DREQ before reset: %d", gpio_get_level(VS1053_PIN_DREQ));
    gpio_set_level(VS1053_PIN_XRST, 0);
    vTaskDelay(pdMS_TO_TICKS(20));
    gpio_set_level(VS1053_PIN_XRST, 1);
    vTaskDelay(pdMS_TO_TICKS(50));

    esp_err_t err = wait_dreq_high(2000);
    ESP_LOGI(TAG, "DREQ after reset: %d", gpio_get_level(VS1053_PIN_DREQ));
    return err;
}

/*
 * Read a 16-bit SCI register. If label is non-null, log the raw MISO bytes.
 * Seeing all 0x00 or all 0xFF means wiring or chip-select is wrong.
 * VS10xx SCI framing: [opcode, reg, 0xFF, 0xFF] → [don't care, don't care, byte_hi, byte_lo]
 */
static esp_err_t sci_read_xfer(uint8_t reg, uint16_t *value, const char *label)
{
    uint8_t tx[4] = {VS_SCI_READ, reg, 0xFF, 0xFF};
    uint8_t rx[4] = {0};
    spi_transaction_t t = {};
    t.length    = 8 * sizeof(tx);
    t.tx_buffer = tx;
    t.rx_buffer = rx;

    esp_err_t err = spi_device_transmit(s_vs_sci, &t);
    if (err != ESP_OK) {
        return err;
    }
    *value = (static_cast<uint16_t>(rx[2]) << 8) | rx[3];
    if (label != nullptr) {
        ESP_LOGI(TAG, "SCI %s: raw MISO [0..3]=%02X %02X %02X %02X  -> reg16=0x%04X",
                 label, rx[0], rx[1], rx[2], rx[3], *value);
    }
    return ESP_OK;
}

/* Silent SCI read — no logging. Used during normal operation. */
static esp_err_t sci_read(uint8_t reg, uint16_t *value)
{
    return sci_read_xfer(reg, value, nullptr);
}

/* Write a 16-bit value to an SCI register. */
static esp_err_t sci_write(uint8_t reg, uint16_t value)
{
    uint8_t tx[4] = {
        VS_SCI_WRITE,
        reg,
        static_cast<uint8_t>(value >> 8),
        static_cast<uint8_t>(value & 0xFF),
    };
    spi_transaction_t t = {};
    t.length    = 8 * sizeof(tx);
    t.tx_buffer = tx;
    return spi_device_transmit(s_vs_sci, &t);
}

/*
 * Send exactly 8 bytes over SDI (XDCS line). Used for the sine wave test commands.
 * Waits for DREQ before sending so we don't overflow the chip's input buffer.
 */
static esp_err_t sdi_write(const uint8_t *data, size_t len)
{
    if (wait_dreq_high(1000) != ESP_OK) {
        ESP_LOGE(TAG, "DREQ timeout before SDI write");
        return ESP_ERR_TIMEOUT;
    }
    spi_transaction_t t = {};
    t.length    = 8 * len;
    t.tx_buffer = data;
    return spi_device_transmit(s_vs_sdi, &t);
}

/*
 * Stream a buffer of MP3 data to the VS10xx in 32-byte chunks over SDI.
 *
 * The chip has a 2 KB internal audio buffer. DREQ goes high when there is
 * room for at least 32 more bytes. We check DREQ before every chunk.
 * The last chunk is zero-padded to 32 bytes — harmless for MP3.
 */
static void vs1053_play_buffer(const uint8_t *data, size_t len)
{
    const size_t CHUNK = 32;
    uint8_t buf[CHUNK];
    size_t offset = 0;

    while (offset < len) {
        size_t remaining = len - offset;
        size_t to_copy   = (remaining >= CHUNK) ? CHUNK : remaining;

        memcpy(buf, data + offset, to_copy);
        if (to_copy < CHUNK) {
            memset(buf + to_copy, 0x00, CHUNK - to_copy);
        }

        if (wait_dreq_high(500) != ESP_OK) {
            ESP_LOGE(TAG, "DREQ timeout at offset %zu — stopping playback", offset);
            return;
        }

        spi_transaction_t t = {};
        t.length    = 8 * CHUNK;
        t.tx_buffer = buf;
        if (spi_device_transmit(s_vs_sdi, &t) != ESP_OK) {
            ESP_LOGE(TAG, "SDI transmit failed at offset %zu", offset);
            return;
        }
        offset += to_copy;
    }
    ESP_LOGI(TAG, "Playback complete: %zu bytes sent", offset);
}

/*
 * Drive all shared-SPI chip selects high before touching the bus.
 * An SD card module with a floating CS will hold MISO low and corrupt every
 * SCI read — this is the most common reason the VS10xx appears unresponsive.
 * XCS is intentionally excluded here because spi_bus_add_device() owns it.
 */
static void assert_inactive_chip_selects(void)
{
    gpio_config_t cs_cfg = {
        .pin_bit_mask = (1ULL << VS1053_PIN_SD_CS) | (1ULL << VS1053_PIN_XDCS),
        .mode         = GPIO_MODE_OUTPUT,
        .pull_up_en   = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type    = GPIO_INTR_DISABLE,
    };
    ESP_ERROR_CHECK(gpio_config(&cs_cfg));
    gpio_set_level(VS1053_PIN_SD_CS, 1);
    gpio_set_level(VS1053_PIN_XDCS, 1);
}

/*
 * Full VS10xx bring-up sequence:
 *  1. Assert all CS lines high so no device drives MISO during init.
 *  2. Initialise the SPI bus and register SCI + SDI devices.
 *  3. Configure XRST (output) and DREQ (input) GPIOs.
 *  4. Hard-reset the chip and wait for DREQ.
 *  5. Read back key registers to confirm SCI comms.
 *  6. Set clock multiplier (3x) and volume.
 *  7. Sine wave test — plays a 1 kHz tone for 4 s via the SDI test mode.
 *  8. MP3 decode test — streams a hardcoded MP3 sample from flash.
 */
void vs1053_bringup_test(void)
{
    /* ---- Step 1: hold all CS lines inactive ---- */
    assert_inactive_chip_selects();

    /* ---- Step 2: SPI bus + SCI/SDI devices ---- */
    spi_bus_config_t buscfg = {};
    buscfg.miso_io_num   = VS1053_PIN_MISO;
    buscfg.mosi_io_num   = VS1053_PIN_MOSI;
    buscfg.sclk_io_num   = VS1053_PIN_SCK;
    buscfg.quadwp_io_num = -1;
    buscfg.quadhd_io_num = -1;
    buscfg.max_transfer_sz = 32;
    ESP_ERROR_CHECK(spi_bus_initialize(VS1053_SPI_HOST, &buscfg, SPI_DMA_CH_AUTO));

    /* SCI clock must stay ≤ CLKI/7. At reset CLKI = crystal (~12 MHz) so 250 kHz is safe.
     * Raise this after CLOCKF is written if you need faster command throughput. */
    spi_device_interface_config_t sci_cfg = {};
    sci_cfg.clock_speed_hz = 250 * 1000;
    sci_cfg.mode           = 0;
    sci_cfg.spics_io_num   = VS1053_PIN_XCS;
    sci_cfg.queue_size     = 1;
    ESP_ERROR_CHECK(spi_bus_add_device(VS1053_SPI_HOST, &sci_cfg, &s_vs_sci));

    spi_device_interface_config_t sdi_cfg = {};
    sdi_cfg.clock_speed_hz = 250 * 1000;
    sdi_cfg.mode           = 0;
    sdi_cfg.spics_io_num   = VS1053_PIN_XDCS;
    sdi_cfg.queue_size     = 1;
    ESP_ERROR_CHECK(spi_bus_add_device(VS1053_SPI_HOST, &sdi_cfg, &s_vs_sdi));

    /* ---- Step 3: XRST output, DREQ input ---- */
    gpio_config_t rst_cfg = {
        .pin_bit_mask = 1ULL << VS1053_PIN_XRST,
        .mode         = GPIO_MODE_OUTPUT,
        .pull_up_en   = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type    = GPIO_INTR_DISABLE,
    };
    ESP_ERROR_CHECK(gpio_config(&rst_cfg));
    gpio_set_level(VS1053_PIN_XRST, 1);
    vTaskDelay(pdMS_TO_TICKS(10));

    gpio_config_t dreq_cfg = {
        .pin_bit_mask = 1ULL << VS1053_PIN_DREQ,
        .mode         = GPIO_MODE_INPUT,
        .pull_up_en   = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type    = GPIO_INTR_DISABLE,
    };
    ESP_ERROR_CHECK(gpio_config(&dreq_cfg));

    /* ---- Step 4: reset and wait for DREQ ---- */
    esp_err_t reset_err = hard_reset();
    if (reset_err != ESP_OK) {
        /* Stuck here in a loop so you can probe wiring without triggering the watchdog. */
        ESP_LOGE(TAG,
                 "DREQ stuck low after reset. Check: SD_CS held high, XRST/DREQ/XCS wiring, "
                 "3V3 power, and that DREQ is wired as MCU input (not being driven low).");
        while (true) {
            ESP_LOGE(TAG, "DREQ=%d XRST=%d SD_CS=%d",
                     gpio_get_level(VS1053_PIN_DREQ),
                     gpio_get_level(VS1053_PIN_XRST),
                     gpio_get_level(VS1053_PIN_SD_CS));
            vTaskDelay(pdMS_TO_TICKS(1000));
        }
    }
    ESP_LOGI(TAG, "VS10xx reset OK, DREQ high");

    /* ---- Step 5: verify SCI comms by reading key registers ---- */
    uint16_t mode = 0, status = 0, clockf = 0;
    /* The first SCI read after reset often returns stale data on VS10xx — discard it. */
    ESP_ERROR_CHECK(sci_read_xfer(VS_REG_MODE,   &mode,   "MODE (1st, discard)"));
    ESP_ERROR_CHECK(sci_read_xfer(VS_REG_MODE,   &mode,   "MODE"));
    ESP_ERROR_CHECK(sci_read_xfer(VS_REG_STATUS, &status, "STATUS"));
    ESP_ERROR_CHECK(sci_read_xfer(VS_REG_CLOCKF, &clockf, "CLOCKF"));
    ESP_LOGI(TAG, "SCI summary  MODE=0x%04X  STATUS=0x%04X  CLOCKF=0x%04X", mode, status, clockf);

    /* Write then read VOL to confirm the full SCI write→read path works. */
    ESP_ERROR_CHECK(sci_write(VS_REG_VOL, 0x2020));
    vTaskDelay(pdMS_TO_TICKS(1));
    uint16_t vol = 0;
    ESP_ERROR_CHECK(sci_read_xfer(VS_REG_VOL, &vol, "VOL readback"));
    ESP_LOGI(TAG, "VOL readback = 0x%04X (expect 0x2020)", vol);

    /* ---- Step 6: init clock and volume ---- */
    /* CLOCKF 0x6000 = 3x multiplier → 12.288 MHz crystal runs chip at ~36.9 MHz. */
    ESP_ERROR_CHECK(sci_write(VS_REG_CLOCKF, 0x6000));
    vTaskDelay(pdMS_TO_TICKS(10));

    /* VOL 0x0000 = maximum volume (0x00 per channel, no attenuation). */
    ESP_ERROR_CHECK(sci_write(VS_REG_VOL, 0x0000));
    vTaskDelay(pdMS_TO_TICKS(1));
    ESP_LOGI(TAG, "Init done: CLOCKF=3x, VOL=max");

    /* ---- Step 7: sine wave test ---- */
    /* SM_TESTS (bit 5 of MODE) enables the VS10xx built-in signal generator.
     * Sending the magic SDI sequence below starts a sine wave at ~1 kHz.
     * No audio file needed — the chip generates the tone internally. */
    uint16_t cur_mode = 0;
    ESP_ERROR_CHECK(sci_read(VS_REG_MODE, &cur_mode));
    ESP_ERROR_CHECK(sci_write(VS_REG_MODE, cur_mode | 0x0020));
    vTaskDelay(pdMS_TO_TICKS(5));

    /* Sine start: magic header + frequency byte (0x44 ≈ 1 kHz) + 4 zero padding bytes. */
    static const uint8_t sine_on[]  = {0x53, 0xEF, 0x6E, 0x44, 0x00, 0x00, 0x00, 0x00};
    /* Sine stop: magic "Exit" sequence. */
    static const uint8_t sine_off[] = {0x45, 0x78, 0x69, 0x74, 0x00, 0x00, 0x00, 0x00};

    ESP_LOGI(TAG, "Sine wave ON — you should hear a 1 kHz tone in the headphones");
    ESP_ERROR_CHECK(sdi_write(sine_on, sizeof(sine_on)));
    vTaskDelay(pdMS_TO_TICKS(4000));
    ESP_ERROR_CHECK(sdi_write(sine_off, sizeof(sine_off)));

    /* Restore MODE to normal decode mode (clear SM_TESTS). */
    ESP_ERROR_CHECK(sci_write(VS_REG_MODE, cur_mode));
    ESP_LOGI(TAG, "Sine wave OFF. If you heard the tone, the audio output works.");

    /* ---- Step 8: MP3 decode test ---- */
    /* With SM_TESTS cleared the chip is in normal MP3 decode mode.
     * We stream a small MP3 from flash exactly the same way we will later stream from SD card. */
    vTaskDelay(pdMS_TO_TICKS(500));
    ESP_LOGI(TAG, "Streaming sample MP3 (%u bytes) from flash", (unsigned)kSampleMp3Size);
    vs1053_play_buffer(kSampleMp3, kSampleMp3Size);
    ESP_LOGI(TAG, "MP3 test done. Both SCI (control) and SDI (audio) paths are verified.");

    /* ---- Idle loop ---- */
    while (true) {
        uint16_t st = 0;
        sci_read(VS_REG_STATUS, &st);
        ESP_LOGI(TAG, "STATUS=0x%04X  DREQ=%d", st, gpio_get_level(VS1053_PIN_DREQ));
        vTaskDelay(pdMS_TO_TICKS(2000));
    }
}
