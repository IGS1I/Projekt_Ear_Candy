/*
 * VS1053b driver — SPI bring-up, register access, audio streaming.
 *
 * Two SPI devices share the same bus (SCK/MOSI/MISO):
 *   SCI  (XCS)  — 4-byte command packets: opcode | reg | value_hi | value_lo
 *   SDI  (XDCS) — raw audio/MP3 data streamed 32 bytes at a time
 *
 * Pin assignments and register addresses are in vs1053b.h.
 * Do NOT use GPIO16/17 on WROVER boards — those are PSRAM.
 */

#include "vs1053b.h"
#include "sample_mp3.h"

#include <string.h>

#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "esp_err.h"
#include "esp_log.h"
#include "rom/ets_sys.h"       /* ets_delay_us() — works at any FreeRTOS tick rate */
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "vs1053";

/* SCI framing opcodes — first byte of every 4-byte SCI transaction. */
#define VS_SCI_READ   0x03
#define VS_SCI_WRITE  0x02

/* SCI device handle — control commands go here (chip select: XCS). */
static spi_device_handle_t s_vs_sci;

/* SDI device handle — audio/MP3 data goes here (chip select: XDCS). */
static spi_device_handle_t s_vs_sdi;

typedef enum {
    AUDIO_STATE_STOPPED = 0,
    AUDIO_STATE_PLAYING,
    AUDIO_STATE_PAUSED,
} audio_state_t;

static audio_state_t s_audio_state = AUDIO_STATE_STOPPED;
static uint8_t s_vol_left = 0x00;   /* attenuation: 0x00 = loudest */
static uint8_t s_vol_right = 0x00;
static uint8_t s_pre_pause_left = 0x00;
static uint8_t s_pre_pause_right = 0x00;
static const uint8_t kVolumeStep = 0x08; /* 4 dB per step (0.5 dB/LSB) */

/* ============================================================================
 * Internal helpers — not exposed in the header
 * ========================================================================= */

/*
 * Block until DREQ goes high (chip ready) or timeout_ms expires.
 * DREQ low = VS1053's internal buffer is full, do not send data yet.
 */
static esp_err_t wait_dreq_high(uint32_t timeout_ms)
{
    for (uint32_t i = 0; i < timeout_ms; i++) {
        if (gpio_get_level(VS1053_PIN_DREQ) == 1) {
            return ESP_OK;
        }
        /*
         * FreeRTOS tick is 100 Hz in this project, so pdMS_TO_TICKS(1) rounds
         * to 0 and does not actually wait. Use a real 1 ms delay so timeout_ms
         * behaves as documented regardless of RTOS tick configuration.
         */
        ets_delay_us(1000);
    }
    return ESP_ERR_TIMEOUT;
}

/*
 * Hard-reset: pulse XRST low then high.
 * DREQ goes high once the chip finishes its internal boot sequence.
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
 * Read a 16-bit SCI register, optionally logging the raw MISO bytes.
 * All-0x00 or all-0xFF on MISO means wiring or chip-select is wrong.
 * VS1053 SCI framing: [opcode, reg, 0xFF, 0xFF] → [dc, dc, byte_hi, byte_lo]
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
    if (err != ESP_OK) return err;

    *value = (static_cast<uint16_t>(rx[2]) << 8) | rx[3];
    if (label != nullptr) {
        ESP_LOGI(TAG, "SCI %s: MISO=[%02X %02X %02X %02X]  reg16=0x%04X",
                 label, rx[0], rx[1], rx[2], rx[3], *value);
    }
    return ESP_OK;
}

/* Silent SCI read — no logging. */
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
 * Send a small SDI packet (used for test-mode commands like sine wave).
 * Checks DREQ first, then sends exactly len bytes.
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
 * GPIO25 = DAC channel 1, GPIO26 = DAC channel 2 on ESP32.
 * ESP-IDF enables the DAC pads during boot which causes the
 * "GPIO 25 is conflict with others" warning and can interfere
 * with our SPI CS (XDCS) and DREQ lines.
 * gpio_reset_pin() releases any analog/special-function claim on
 * a pad and returns it to plain digital GPIO before SPI takes over.
 */
static void disable_dac_on_spi_pins(void)
{
    gpio_reset_pin(VS1053_PIN_XDCS); /* GPIO25 — DAC channel 1 */
    gpio_reset_pin(VS1053_PIN_DREQ); /* GPIO26 — DAC channel 2 */
}

/*
 * Drive all shared-bus chip selects high before touching the bus.
 * A floating SD card CS will hold MISO low and corrupt every SCI read —
 * most common reason the VS1053 appears unresponsive on first bring-up.
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

static void log_decoder_state(const char *reason)
{
    uint16_t mode = 0, status = 0, hdat0 = 0, hdat1 = 0, decode_time = 0;
    sci_read(VS_REG_MODE, &mode);
    sci_read(VS_REG_STATUS, &status);
    sci_read(VS_REG_HDAT0, &hdat0);
    sci_read(VS_REG_HDAT1, &hdat1);
    sci_read(VS_REG_DECODE_TIME, &decode_time);
    ESP_LOGW(TAG,
             "%s: DREQ=%d MODE=0x%04X STATUS=0x%04X HDAT1=0x%04X HDAT0=0x%04X DECODE_TIME=%u",
             reason,
             gpio_get_level(VS1053_PIN_DREQ),
             mode,
             status,
             hdat1,
             hdat0,
             decode_time);
}

/* ============================================================================
 * Public API implementation
 * ========================================================================= */

/*
 * Initialize the VS1053:
 *  1. All CS lines high (prevent MISO corruption)
 *  2. SPI bus + SCI / SDI device registration
 *  3. XRST output, DREQ input
 *  4. Hard reset → wait for DREQ
 *  5. Read key registers to confirm SCI comms
 *  6. Set clock multiplier 3× and volume to maximum
 */
esp_err_t vs1053_init(void)
{
    /* Step 1 — disable DAC on GPIO25/26 before SPI takes them over */
    disable_dac_on_spi_pins();
    assert_inactive_chip_selects();

    /* Step 2 — SPI bus */
    spi_bus_config_t buscfg = {};
    buscfg.miso_io_num     = VS1053_PIN_MISO;
    buscfg.mosi_io_num     = VS1053_PIN_MOSI;
    buscfg.sclk_io_num     = VS1053_PIN_SCK;
    buscfg.quadwp_io_num   = -1;
    buscfg.quadhd_io_num   = -1;
    buscfg.max_transfer_sz = 4096;
    ESP_ERROR_CHECK(spi_bus_initialize(VS1053_SPI_HOST, &buscfg, SPI_DMA_CH_AUTO));

    /*
     * SCI clock must be ≤ CLKI/7.
     * Crystal = ~12.288 MHz → max pre-CLOCKF SCI = 1.755 MHz → use 1 MHz.
     * After CLOCKF=3× (36.864 MHz) the ceiling rises to 5.26 MHz; we leave
     * SCI at 1 MHz (register accesses are infrequent — no need to re-init).
     */
    spi_device_interface_config_t sci_cfg = {};
    sci_cfg.clock_speed_hz = 1 * 1000 * 1000;
    sci_cfg.mode           = 0;
    sci_cfg.spics_io_num   = VS1053_PIN_XCS;
    sci_cfg.queue_size     = 1;
    ESP_ERROR_CHECK(spi_bus_add_device(VS1053_SPI_HOST, &sci_cfg, &s_vs_sci));

    /*
     * SDI clock must be ≤ CLKI/4.
     * We never send SDI data before CLOCKF is written (vs1053_init does not
     * call any SDI transfer), so we can safely register SDI at 4 MHz now.
     * After CLOCKF=3× the limit is 9.22 MHz, so 4 MHz is well within spec.
     */
    spi_device_interface_config_t sdi_cfg = {};
    sdi_cfg.clock_speed_hz = 4 * 1000 * 1000;
    sdi_cfg.mode           = 0;
    sdi_cfg.spics_io_num   = VS1053_PIN_XDCS;
    sdi_cfg.queue_size     = 1;
    ESP_ERROR_CHECK(spi_bus_add_device(VS1053_SPI_HOST, &sdi_cfg, &s_vs_sdi));

    /* Step 3 — GPIO directions */
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

    /* Step 4 — hard reset */
    esp_err_t reset_err = hard_reset();
    if (reset_err != ESP_OK) {
        ESP_LOGE(TAG,
                 "DREQ stuck low after reset.\n"
                 "Check: SD_CS held high, XRST/DREQ/XCS wiring, 3V3 power.");
        while (true) {
            ESP_LOGE(TAG, "DREQ=%d  XRST=%d  SD_CS=%d",
                     gpio_get_level(VS1053_PIN_DREQ),
                     gpio_get_level(VS1053_PIN_XRST),
                     gpio_get_level(VS1053_PIN_SD_CS));
            vTaskDelay(pdMS_TO_TICKS(1000));
        }
    }
    ESP_LOGI(TAG, "VS1053 reset OK — DREQ high");

    /* Step 5 — write MODE to a known clean state immediately after reset.
     * SM_SDINEW (bit 11) is the only bit that must be set for 4-wire SPI.
     * Everything else (EarSpeaker, LINE1, test mode …) starts as 0.
     * The chip often retains non-default MODE bits across soft resets;
     * writing 0x0800 here guarantees a predictable decode environment. */
    ESP_ERROR_CHECK(sci_write(VS_REG_MODE, VS_MODE_SM_SDINEW));
    vTaskDelay(pdMS_TO_TICKS(2));

    /* Confirm SCI path — first read after reset often returns stale data. */
    uint16_t mode = 0, status = 0, clockf = 0;
    ESP_ERROR_CHECK(sci_read_xfer(VS_REG_MODE,   &mode,   "MODE (1st, discard)"));
    ESP_ERROR_CHECK(sci_read_xfer(VS_REG_MODE,   &mode,   "MODE"));
    ESP_ERROR_CHECK(sci_read_xfer(VS_REG_STATUS, &status, "STATUS"));
    ESP_ERROR_CHECK(sci_read_xfer(VS_REG_CLOCKF, &clockf, "CLOCKF (pre-write)"));
    ESP_LOGI(TAG, "SCI OK — MODE=0x%04X (expect 0x0800)  STATUS=0x%04X  CLOCKF=0x%04X",
             mode, status, clockf);

    /* Write then read VOL to confirm the full SCI write→read round-trip. */
    ESP_ERROR_CHECK(sci_write(VS_REG_VOL, 0x2020));
    vTaskDelay(pdMS_TO_TICKS(1));
    uint16_t vol = 0;
    ESP_ERROR_CHECK(sci_read_xfer(VS_REG_VOL, &vol, "VOL round-trip"));
    ESP_LOGI(TAG, "VOL readback = 0x%04X  (expect 0x2020)", vol);

    /* Step 6 — clock multiplier (datasheet-recommended value).
     * 0x8800 = SC_MULT=3.5× (bits[15:13]=100) + SC_ADD=1.0× (bits[12:11]=01).
     * CLKI = 12.288 MHz × 3.5 = 43.008 MHz.
     * SC_ADD=1.0× raises the max SCI/SDI clock ceilings:
     *   max SCI = CLKI×(SC_MULT+SC_ADD)/7 = 12.288×4.5/7 ≈ 7.9 MHz
     *   max SDI = CLKI×(SC_MULT+SC_ADD)/4 = 12.288×4.5/4 ≈ 13.8 MHz
     * The PLL stabilises within 100 µs. Wait for DREQ to confirm. */
    ESP_ERROR_CHECK(sci_write(VS_REG_CLOCKF, 0x8800));
    vTaskDelay(pdMS_TO_TICKS(10));  /* ≥1 tick at 100 Hz; gives PLL time to lock */
    wait_dreq_high(200);             /* extra safety: wait for chip to re-assert ready */

    /* Read CLOCKF back to confirm the write landed. */
    ESP_ERROR_CHECK(sci_read_xfer(VS_REG_CLOCKF, &clockf, "CLOCKF (post-write)"));
    ESP_LOGI(TAG, "CLOCKF after write = 0x%04X  (expect 0x8800)", clockf);

    /* Maximum volume — 0x0000 = 0 dB attenuation on both channels. */
    ESP_ERROR_CHECK(sci_write(VS_REG_VOL, 0x0000));
    vTaskDelay(pdMS_TO_TICKS(1));

    ESP_LOGI(TAG, "vs1053_init() complete — CLOCKF=3.5×+1×, SDI=4 MHz, VOL=max");
    return ESP_OK;
}

/* -------------------------------------------------------------------------- */

esp_err_t audio_processor_init(void)
{
    esp_err_t err = vs1053_init();
    if (err == ESP_OK) {
        s_audio_state = AUDIO_STATE_STOPPED;
        s_vol_left = 0x00;
        s_vol_right = 0x00;
    }
    return err;
}

/* -------------------------------------------------------------------------- */

esp_err_t vs1053_read_reg(uint8_t reg, uint16_t *value)
{
    return sci_read(reg, value);
}

esp_err_t vs1053_write_reg(uint8_t reg, uint16_t value)
{
    return sci_write(reg, value);
}

/* -------------------------------------------------------------------------- */

/*
 * Soft reset: set SM_RESET in MODE, wait for DREQ, restore clock multiplier.
 * The chip clears SM_RESET itself once boot is done.
 */
esp_err_t vs1053_soft_reset(void)
{
    uint16_t mode = 0;
    sci_read(VS_REG_MODE, &mode);
    sci_write(VS_REG_MODE, mode | VS_MODE_SM_RESET);
    vTaskDelay(pdMS_TO_TICKS(2));

    esp_err_t err = wait_dreq_high(2000);
    if (err == ESP_OK) {
        /* CLOCKF is cleared by soft reset — restore 3× multiplier. */
        sci_write(VS_REG_CLOCKF, 0x6000);
        vTaskDelay(pdMS_TO_TICKS(10));
        ESP_LOGI(TAG, "Soft reset done");
    } else {
        ESP_LOGE(TAG, "Soft reset: DREQ never went high");
    }
    return err;
}

/* -------------------------------------------------------------------------- */

/*
 * Cancel playback cleanly per the VS1053 datasheet:
 *  1. Set SM_CANCEL in MODE.
 *  2. Keep feeding 32-byte zero chunks — the chip needs data to unwind.
 *  3. Poll MODE: when SM_CANCEL clears the chip is ready for a new stream.
 *  4. If not done after 2 KB (64 chunks), fall back to soft reset.
 */
esp_err_t vs1053_cancel(void)
{
    uint16_t mode = 0;
    sci_read(VS_REG_MODE, &mode);
    sci_write(VS_REG_MODE, mode | VS_MODE_SM_CANCEL);

    static const uint8_t zeros[VS_SDI_CHUNK_BYTES] = {0};

    for (int chunk = 0; chunk < 64; chunk++) {
        if (wait_dreq_high(100) != ESP_OK) break;

        spi_transaction_t t = {};
        t.length    = 8 * VS_SDI_CHUNK_BYTES;
        t.tx_buffer = zeros;
        spi_device_transmit(s_vs_sdi, &t);

        uint16_t cur_mode = 0;
        sci_read(VS_REG_MODE, &cur_mode);
        if (!(cur_mode & VS_MODE_SM_CANCEL)) {
            ESP_LOGI(TAG, "Playback cancelled after %d chunks", chunk + 1);
            return ESP_OK;
        }
    }

    ESP_LOGW(TAG, "SM_CANCEL not cleared — falling back to soft reset");
    return vs1053_soft_reset();
}

/* -------------------------------------------------------------------------- */

/*
 * Set output volume independently per channel.
 * VS_REG_VOL high byte = left channel, low byte = right channel.
 * 0x00 = maximum (0 dB attenuation).
 * 0xFE = effectively silent (127 dB attenuation in 0.5 dB steps).
 */
esp_err_t vs1053_set_volume(uint8_t vol_left, uint8_t vol_right)
{
    uint16_t vol = (static_cast<uint16_t>(vol_left) << 8) | vol_right;
    esp_err_t err = sci_write(VS_REG_VOL, vol);
    if (err == ESP_OK) {
        s_vol_left = vol_left;
        s_vol_right = vol_right;
    }
    return err;
}

/* -------------------------------------------------------------------------- */

/*
 * Map a raw ADC / potentiometer reading to volume and apply it.
 *
 * The VS1053 VOL register works backwards from what you'd expect:
 *   0x00 = loudest,  0xFE = quietest.
 *
 * So we invert the dial reading:
 *   dial at maximum (adc_value == adc_max) → attenuation 0x00 (full volume)
 *   dial at minimum (adc_value == 0)       → attenuation 0xFE (silent)
 */
esp_err_t vs1053_set_volume_dial(uint16_t adc_value, uint16_t adc_max)
{
    if (adc_max == 0) return ESP_ERR_INVALID_ARG;

    /* Clamp to valid range then invert. */
    if (adc_value > adc_max) adc_value = adc_max;
    uint8_t attenuation = (uint8_t)(0xFE - ((uint32_t)adc_value * 0xFE / adc_max));

    return vs1053_set_volume(attenuation, attenuation);
}

/* -------------------------------------------------------------------------- */

esp_err_t audio_play_memory(const uint8_t *data, size_t len)
{
    if (data == nullptr || len == 0) return ESP_ERR_INVALID_ARG;

    if (s_audio_state == AUDIO_STATE_PLAYING || s_audio_state == AUDIO_STATE_PAUSED) {
        audio_stop();
    }

    s_audio_state = AUDIO_STATE_PLAYING;
    vs1053_play_buffer(data, len); /* blocking stream */
    if (s_audio_state == AUDIO_STATE_PLAYING) {
        s_audio_state = AUDIO_STATE_STOPPED;
    }
    return ESP_OK;
}

esp_err_t audio_play_sample(void)
{
    return audio_play_memory(kSampleMp3, kSampleMp3Size);
}

esp_err_t audio_pause(void)
{
    if (s_audio_state != AUDIO_STATE_PLAYING) return ESP_ERR_INVALID_STATE;
    s_pre_pause_left = s_vol_left;
    s_pre_pause_right = s_vol_right;
    ESP_ERROR_CHECK(vs1053_set_volume(0xFE, 0xFE)); /* mute without stream cancel */
    s_audio_state = AUDIO_STATE_PAUSED;
    ESP_LOGI(TAG, "audio_pause()");
    return ESP_OK;
}

esp_err_t audio_resume(void)
{
    if (s_audio_state != AUDIO_STATE_PAUSED) return ESP_ERR_INVALID_STATE;
    ESP_ERROR_CHECK(vs1053_set_volume(s_pre_pause_left, s_pre_pause_right));
    s_audio_state = AUDIO_STATE_PLAYING;
    ESP_LOGI(TAG, "audio_resume()");
    return ESP_OK;
}

esp_err_t audio_stop(void)
{
    esp_err_t err = vs1053_cancel();
    if (err != ESP_OK) {
        /* Keep going; cancel fallback may have already soft-reset. */
        ESP_LOGW(TAG, "audio_stop(): cancel returned 0x%x", (unsigned)err);
    }
    s_audio_state = AUDIO_STATE_STOPPED;
    ESP_LOGI(TAG, "audio_stop()");
    return ESP_OK;
}

esp_err_t audio_volume_up(void)
{
    /* Lower attenuation => louder. */
    uint8_t left = (s_vol_left > kVolumeStep) ? (s_vol_left - kVolumeStep) : 0x00;
    uint8_t right = (s_vol_right > kVolumeStep) ? (s_vol_right - kVolumeStep) : 0x00;
    esp_err_t err = vs1053_set_volume(left, right);
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "audio_volume_up(): L=0x%02X R=0x%02X", left, right);
    }
    return err;
}

esp_err_t audio_volume_down(void)
{
    /* Higher attenuation => quieter. */
    uint16_t left = s_vol_left + kVolumeStep;
    uint16_t right = s_vol_right + kVolumeStep;
    if (left > 0xFE) left = 0xFE;
    if (right > 0xFE) right = 0xFE;
    esp_err_t err = vs1053_set_volume((uint8_t)left, (uint8_t)right);
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "audio_volume_down(): L=0x%02X R=0x%02X", (unsigned)left, (unsigned)right);
    }
    return err;
}

/* -------------------------------------------------------------------------- */

bool audio_is_chip_connected(void)
{
    uint16_t status = 0;
    if (sci_read(VS_REG_STATUS, &status) != ESP_OK) return false;
    /* A valid VS1053 response is neither all-zeros nor all-ones. */
    if (status == 0x0000 || status == 0xFFFF) return false;
    return true;
}

esp_err_t audio_force_mp3_mode(void)
{
    uint16_t mode = 0;
    ESP_ERROR_CHECK(sci_read(VS_REG_MODE, &mode));

    /*
     * Keep SDINEW enabled, clear test/record/line-in bits that can block
     * normal MP3 decode on some modules.
     */
    uint16_t new_mode = mode;
    new_mode |= VS_MODE_SM_SDINEW;
    new_mode &= (uint16_t)~(VS_MODE_SM_TESTS | VS_MODE_SM_ADPCM | VS_MODE_SM_LINE1);

    if (new_mode != mode) {
        ESP_ERROR_CHECK(sci_write(VS_REG_MODE, new_mode));
        vTaskDelay(pdMS_TO_TICKS(2));
    }

    /* Soft reset to ensure decoder core applies mode consistently. */
    ESP_ERROR_CHECK(vs1053_soft_reset());
    ESP_LOGI(TAG, "audio_force_mp3_mode(): MODE=0x%04X", new_mode);
    return ESP_OK;
}

/* -------------------------------------------------------------------------- */

/* MP3 bitrate table in kbps for Layer III. Index 0/15 are invalid. */
static const uint16_t kMp3BitrateKbpsMpeg1L3[16] = {
    0, 32, 40, 48, 56, 64, 80, 96, 112, 128, 160, 192, 224, 256, 320, 0
};
static const uint16_t kMp3BitrateKbpsMpeg2L3[16] = {
    0, 8, 16, 24, 32, 40, 48, 56, 64, 80, 96, 112, 128, 144, 160, 0
};

/*
 * Return true if p points to a plausible MPEG audio frame header.
 * Also returns the bitrate in bps when available.
 */
static bool parse_mp3_header(const uint8_t *p, size_t remaining, uint32_t *bitrate_bps)
{
    if (remaining < 4 || p == nullptr || bitrate_bps == nullptr) return false;
    if (p[0] != 0xFF || (p[1] & 0xE0) != 0xE0) return false;   /* sync bits */

    uint8_t version_id   = (p[1] >> 3) & 0x03;  /* 3=MPEG1, 2=MPEG2, 0=MPEG2.5 */
    uint8_t layer_id     = (p[1] >> 1) & 0x03;  /* 1=Layer III */
    uint8_t bitrate_idx  = (p[2] >> 4) & 0x0F;
    uint8_t sampler_idx  = (p[2] >> 2) & 0x03;

    if (layer_id != 0x01) return false;          /* only Layer III expected here */
    if (version_id == 0x01) return false;        /* reserved version */
    if (bitrate_idx == 0x00 || bitrate_idx == 0x0F) return false;
    if (sampler_idx == 0x03) return false;       /* reserved sample rate */

    uint16_t kbps = (version_id == 0x03)
                        ? kMp3BitrateKbpsMpeg1L3[bitrate_idx]
                        : kMp3BitrateKbpsMpeg2L3[bitrate_idx];
    if (kbps == 0) return false;

    *bitrate_bps = static_cast<uint32_t>(kbps) * 1000U;
    return true;
}

/*
 * Skip optional ID3v2 tag, then scan for the first MP3 frame sync.
 * Returns true and fills start_offset / bitrate when found.
 */
static bool find_mp3_start(const uint8_t *data, size_t len, size_t *start_offset, uint32_t *bitrate_bps)
{
    if (data == nullptr || start_offset == nullptr || bitrate_bps == nullptr || len < 4) return false;

    size_t scan_from = 0;
    if (len >= 10 && data[0] == 'I' && data[1] == 'D' && data[2] == '3') {
        /* ID3v2 size is synchsafe: 4 bytes, 7 bits each. */
        size_t tag_size = (static_cast<size_t>(data[6] & 0x7F) << 21) |
                          (static_cast<size_t>(data[7] & 0x7F) << 14) |
                          (static_cast<size_t>(data[8] & 0x7F) << 7)  |
                          (static_cast<size_t>(data[9] & 0x7F));
        scan_from = 10 + tag_size;
    }

    if (scan_from >= len) return false;

    for (size_t i = scan_from; i + 4 <= len; i++) {
        uint32_t bps = 0;
        if (parse_mp3_header(data + i, len - i, &bps)) {
            *start_offset = i;
            *bitrate_bps = bps;
            return true;
        }
    }
    return false;
}

/* -------------------------------------------------------------------------- */

/*
 * Stream a buffer of audio data (MP3, OGG, WAV …) from flash or RAM.
 *
 * The VS1053 has a 2 KB internal buffer. DREQ goes high whenever there is
 * room for at least 32 more bytes. We check DREQ before every chunk and
 * zero-pad the final chunk to a full 32 bytes (harmless for all formats).
 *
 * To use your own audio file:
 *   1. On your Mac, run:  xxd -i song.mp3 > sample_mp3.h
 *   2. Rename the generated array to kSampleMp3 and the size to kSampleMp3Size.
 *   3. Call:  vs1053_play_buffer(kSampleMp3, kSampleMp3Size);
 */
void vs1053_play_buffer(const uint8_t *data, size_t len)
{
    if (data == nullptr || len == 0) {
        ESP_LOGE(TAG, "vs1053_play_buffer called with empty buffer");
        return;
    }

    size_t mp3_offset = 0;
    uint32_t bitrate_bps = 128000; /* sensible fallback for unknown streams */
    bool has_mp3_header = find_mp3_start(data, len, &mp3_offset, &bitrate_bps);
    if (has_mp3_header) {
        if (mp3_offset > 0) {
            ESP_LOGW(TAG, "Skipping %zu leading bytes before first MP3 frame", mp3_offset);
            data += mp3_offset;
            len  -= mp3_offset;
        }
        ESP_LOGI(TAG, "Detected MP3 frame header, bitrate ~%u kbps", (unsigned)(bitrate_bps / 1000U));
    } else {
        ESP_LOGW(TAG, "No MP3 frame header found in buffer — streaming raw bytes as-is");
    }

    uint8_t buf[VS_SDI_CHUNK_BYTES];
    size_t offset = 0;

    /* Log progress every 50 KB so the serial monitor shows the track advancing. */
    size_t next_log = 50 * 1024;

    while (offset < len) {
        size_t remaining = len - offset;
        size_t to_copy   = (remaining >= VS_SDI_CHUNK_BYTES) ? VS_SDI_CHUNK_BYTES : remaining;

        memcpy(buf, data + offset, to_copy);
        if (to_copy < VS_SDI_CHUNK_BYTES) {
            memset(buf + to_copy, 0x00, VS_SDI_CHUNK_BYTES - to_copy);
        }

        if (wait_dreq_high(2000) != ESP_OK) {
            ESP_LOGE(TAG, "DREQ timeout at offset %zu/%zu — stopping playback",
                     offset, len);
            log_decoder_state("stream timeout");
            (void)vs1053_soft_reset();
            return;
        }

        spi_transaction_t t = {};
        t.length    = 8 * VS_SDI_CHUNK_BYTES;
        t.tx_buffer = buf;
        if (spi_device_transmit(s_vs_sdi, &t) != ESP_OK) {
            ESP_LOGE(TAG, "SDI transmit failed at offset %zu", offset);
            return;
        }

        /*
         * Hard pacing based on detected MP3 bitrate.
         * Example:
         *   64 kbps  -> 32-byte chunk period = 4.00 ms
         *   128 kbps -> 32-byte chunk period = 2.00 ms
         *
         * Keep this independent of FreeRTOS tick rate (100 Hz on this project).
         */
        uint32_t chunk_us = (VS_SDI_CHUNK_BYTES * 8U * 1000000U) / bitrate_bps;
        if (chunk_us < 500U) chunk_us = 500U;
        if (chunk_us > 8000U) chunk_us = 8000U;
        /* Subtract rough SPI transfer time at 4 MHz (32 bytes ~= 64 us). */
        if (chunk_us > 64U) chunk_us -= 64U;
        ets_delay_us(chunk_us);

        offset += to_copy;

        if (offset >= next_log) {
            ESP_LOGI(TAG, "Streaming … %zu / %zu bytes (%.0f%%)",
                     offset, len, 100.0f * offset / len);
            next_log += 50 * 1024;
        }
    }

    /*
     * End-fill flush (VS1053 application note §9.11):
     * After the last data byte the decoder still has frames in its pipeline.
     * Read the end-fill byte from HDAT1[7:0], send ≥ 2052 bytes of it, then
     * send 2052 bytes of 0x00 to guarantee the final audio frame is output.
     */
    uint16_t hdat1 = 0;
    sci_read(VS_REG_HDAT1, &hdat1);
    uint8_t fill_byte = static_cast<uint8_t>(hdat1 & 0xFF);

    uint8_t fill_buf[VS_SDI_CHUNK_BYTES];
    memset(fill_buf, fill_byte, sizeof(fill_buf));

    /* 2080 bytes = 65 × 32 — rounds up past the 2052-byte minimum. */
    for (int i = 0; i < 65; i++) {
        if (wait_dreq_high(200) != ESP_OK) break;
        spi_transaction_t t = {};
        t.length    = 8 * VS_SDI_CHUNK_BYTES;
        t.tx_buffer = fill_buf;
        spi_device_transmit(s_vs_sdi, &t);
    }

    ESP_LOGI(TAG, "Playback complete: %zu bytes streamed (end-fill 0x%02X sent)",
             offset, fill_byte);
}

/* ============================================================================
 * Bring-up / self-test — useful for verifying hardware before app integration
 * ========================================================================= */

void vs1053_sine_continuous_test(void)
{
    ESP_ERROR_CHECK(vs1053_init());

    uint16_t mode = 0;
    ESP_ERROR_CHECK(sci_read(VS_REG_MODE, &mode));
    ESP_ERROR_CHECK(sci_write(VS_REG_MODE, mode | VS_MODE_SM_TESTS));
    vTaskDelay(pdMS_TO_TICKS(5));
    ESP_ERROR_CHECK(vs1053_set_volume(0x00, 0x00)); /* max volume for debug */

    static const uint8_t sine_on[] = {0x53, 0xEF, 0x6E, 0x44, 0x00, 0x00, 0x00, 0x00};
    ESP_LOGI(TAG, "Continuous sine ON (~1 kHz). Reset board to stop.");
    ESP_ERROR_CHECK(sdi_write(sine_on, sizeof(sine_on)));

    while (true) {
        uint16_t status = 0;
        sci_read(VS_REG_STATUS, &status);
        ESP_LOGI(TAG, "SINE_RUNNING  STATUS=0x%04X  DREQ=%d",
                 status, gpio_get_level(VS1053_PIN_DREQ));
        vTaskDelay(pdMS_TO_TICKS(2000));
    }
}

/*
 * Full bring-up sequence:
 *  1. vs1053_init()     — hardware init
 *  2. Sine wave test    — 1 kHz tone for 4 s via chip's built-in generator
 *  3. MP3 decode test   — streams kSampleMp3 from flash
 *  4. Idle status loop  — keeps logging STATUS + DREQ so you can probe live
 */
void vs1053_bringup_test(void)
{
    ESP_ERROR_CHECK(vs1053_init());

    /* ---- Sine wave test ---- */
    /*
     * SM_TESTS enables the VS1053's internal signal generator.
     * Sending the 8-byte SDI sequence below starts a sine at ~1 kHz.
     * No audio file needed — the chip generates it internally.
     *
     * We play multiple longer beeps so there is plenty of time to
     * connect/listen and confirm the analog path is alive.
     */
    uint16_t cur_mode = 0;
    ESP_ERROR_CHECK(sci_read(VS_REG_MODE, &cur_mode));
    ESP_ERROR_CHECK(sci_write(VS_REG_MODE, cur_mode | VS_MODE_SM_TESTS));
    vTaskDelay(pdMS_TO_TICKS(5));
    ESP_ERROR_CHECK(vs1053_set_volume(0x00, 0x00)); /* force max during sine test */

    /* 0x44 in the frequency byte ≈ 1 kHz tone. */
    static const uint8_t sine_on[]  = {0x53, 0xEF, 0x6E, 0x44, 0x00, 0x00, 0x00, 0x00};
    static const uint8_t sine_off[] = {0x45, 0x78, 0x69, 0x74, 0x00, 0x00, 0x00, 0x00};

    ESP_LOGI(TAG, "Sine test: 3 long beeps incoming (listen now)");
    vTaskDelay(pdMS_TO_TICKS(1500)); /* small pre-roll before first beep */
    for (int i = 0; i < 3; i++) {
        ESP_LOGI(TAG, "Sine wave ON (%d/3) — ~1 kHz tone", i + 1);
        ESP_ERROR_CHECK(sdi_write(sine_on, sizeof(sine_on)));
        vTaskDelay(pdMS_TO_TICKS(3000));
        ESP_ERROR_CHECK(sdi_write(sine_off, sizeof(sine_off)));
        ESP_LOGI(TAG, "Sine wave OFF (%d/3)", i + 1);
        vTaskDelay(pdMS_TO_TICKS(1200));
    }

    /* Restore MODE to normal decode (clear SM_TESTS). */
    ESP_ERROR_CHECK(sci_write(VS_REG_MODE, cur_mode));
    ESP_LOGI(TAG, "Sine test complete");

    /* ---- MP3 decode test ---- */
    vTaskDelay(pdMS_TO_TICKS(500));
    ESP_LOGI(TAG, "Streaming sample MP3 (%u bytes) from flash",
             (unsigned)kSampleMp3Size);
    vs1053_play_buffer(kSampleMp3, kSampleMp3Size);
    ESP_LOGI(TAG, "MP3 test done — SCI and SDI paths both verified");

    /* ---- Post-playback diagnostics ---- */
    /*
     * DECODE_TIME: seconds the chip decoded. 0 means format not recognised.
     * HDAT1: last MP3 sync header seen — should be 0xFFFA/FB/FC/FE for MP3.
     *        0x0000 = chip never locked to a valid frame.
     * HDAT0: frame info (bitrate / sample-rate bits from last decoded frame).
     */
    {
        uint16_t decode_time = 0, hdat0 = 0, hdat1 = 0;
        sci_read(VS_REG_DECODE_TIME, &decode_time);
        sci_read(VS_REG_HDAT1,       &hdat1);
        sci_read(VS_REG_HDAT0,       &hdat0);
        ESP_LOGI(TAG, "DECODE_TIME=%u s  HDAT1=0x%04X  HDAT0=0x%04X",
                 decode_time, hdat1, hdat0);
        if (hdat1 == 0x0000) {
            ESP_LOGW(TAG, "HDAT1=0 — chip never synced to an MP3 frame. "
                          "Check audio_sample.mp3 is a valid MP3 file.");
        } else {
            ESP_LOGI(TAG, "MP3 sync confirmed — chip decoded %u seconds of audio", decode_time);
        }
    }

    /* ---- Idle loop ---- */
    while (true) {
        uint16_t st = 0;
        sci_read(VS_REG_STATUS, &st);
        ESP_LOGI(TAG, "STATUS=0x%04X  DREQ=%d", st, gpio_get_level(VS1053_PIN_DREQ));
        vTaskDelay(pdMS_TO_TICKS(2000));
    }
}
