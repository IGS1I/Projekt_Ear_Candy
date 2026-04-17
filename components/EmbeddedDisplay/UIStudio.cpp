#include "UIStudio.hpp"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_rom_sys.h"
#include <string.h>

/********** DUI Class Functions' Implementation **********/

/**
 * @brief Sends a command to the display
 * @param cmd The command to send
 */
void DUI::send_cmd(uint8_t cmd) {
    gpio_set_level(dc, 0); // DC LOW to indicate command
    spi_transaction_t t;
    memset(&t, 0, sizeof(t)); // Zero out the transaction structure
    t.length = 8;        //  1 byte data length(8 bits)
    t.tx_buffer = &cmd; // command byte
    spi_device_polling_transmit(vspi, &t); // Transmit command
}

/** 
 * @brief Sends data to the display
 * @param data Pointer to the data buffer
 * @param length Length of the data to send
 */
void DUI::send_data(const uint8_t *data, int length) {
    if (length == 0) return; // No data to send
    gpio_set_level(dc, 1); // DC HIGH to indicate data
    spi_transaction_t t;
    memset(&t, 0, sizeof(t)); // Zero out the transaction structure
    t.length = length * 8; // length in bits
    t.tx_buffer = data;   // pointer to data buffer
    spi_device_polling_transmit(vspi, &t); // Transmit data
}

/** 
 * @brief Sends a single byte of data to the display
 **/
void DUI::send_data_byte (uint8_t data) {
    send_data(&data, 1);
}

/*-------------- Constructor ---------------*/

/**
 * @brief Constructor for the DUI class. Initializes GPIO pin assignments and display dimensions.
 * @param sda_pin GPIO pin number for Serial Data (SDA)
 * @param scl_pin GPIO pin number for Serial Clock (SCL)
 * @param cs_pin GPIO pin number for Chip Select (CS)
 * @param res_pin GPIO pin number for Reset (RES)
 * @param te_pin GPIO pin number for Tearing Effect (TE)
 * @param w Initial width of the display (in pixels)
 * @param h Initial height of the display (in pixels)
 **/
DUI::DUI(gpio_num_t sda_pin, gpio_num_t scl_pin, gpio_num_t cs_pin, gpio_num_t dc_pin, gpio_num_t res_pin, gpio_num_t te_pin, uint16_t w, uint16_t h) 
    : dc(dc_pin), res(res_pin), te(te_pin), width(w), height(h) {
    
    //-----Initialize SPI device non-SPI GPIOs----

    // Output pins: DC, RES
    gpio_config_t output_conf = {
        .pin_bit_mask = (1ULL << dc) | (1ULL << res),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    gpio_config(&output_conf);

    // Input pin: TE (Tearing Effect)
    gpio_config_t input_conf = {
        .pin_bit_mask = (1ULL << te),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    gpio_config(&input_conf);

    // -----Configure Hardware SPI Bus (VSPI)----
    spi_bus_config_t vspibuscfg = {};
    vspibuscfg.mosi_io_num = sda_pin;
    vspibuscfg.miso_io_num = -1; // Not used
    vspibuscfg.sclk_io_num = scl_pin;
    vspibuscfg.quadwp_io_num = -1; // Not used
    vspibuscfg.quadhd_io_num = -1; // Not used
    vspibuscfg.max_transfer_sz = width * 2; // Max size for a full row of pixels (2 bytes per pixel in 16-bit mode)
    // vspibuscfg.max_transfer_sz = width * height * 2 + 8; // Max size for a full-screen update (2 bytes per pixel) + some overhead

    // Initialize bus
    spi_bus_initialize(SPI3_HOST, &vspibuscfg, SPI_DMA_CH_AUTO);

    // -----Attach Display to SPI Bus----
    spi_device_interface_config_t devcfg = {};
    devcfg.clock_speed_hz = 40 * 1000 * 1000; // 40 MHz
    devcfg.mode = 0; // SPI mode 0
    devcfg.spics_io_num = cs_pin; // hardware-controlled CS pin
    devcfg.queue_size = 7;

    spi_bus_add_device(SPI3_HOST, &devcfg, &vspi);
}

/*-------------- Member Functions ---------------*/

/**
 * @brief Initializes the display
 */
void DUI::init() {
// ~Reset~

    // Hardware Reset Sequence
    gpio_set_level(res, 0);
    vTaskDelay(pdMS_TO_TICKS(100));
    gpio_set_level(res, 1);
    vTaskDelay(pdMS_TO_TICKS(150));

// ~Initialization~

    // Sleep Out and wait for display to be ready
    send_cmd(0x11); // Sleep Out
    vTaskDelay(pdMS_TO_TICKS(120));

    // Set color mode to 16-bit (RGB565) and orientation
    set_orientation(true); // True = Landscape mode | False = Portrait mode
    send_cmd(0x3A); send_cmd(0x05); // 16-bit color/RGB565 (COLMOD)

    // ST7789VI Optimization Registers (driver specific)
    send_cmd(0xB2); uint8_t b2[] = {0x0C, 0x0C, 0x00, 0x33, 0x33}; send_data(b2, 5);
    send_cmd(0xB7); send_data_byte(0x35);
    send_cmd(0xBB); send_data_byte(0x2B);
    send_cmd(0xC0); send_data_byte(0x2C);
    send_cmd(0xC2); uint8_t c2[] = {0x01, 0xFF}; send_data(c2, 2);
    send_cmd(0xC3); send_data_byte(0x11);
    send_cmd(0xC4); send_data_byte(0x20);
    send_cmd(0xC6); send_data_byte(0x0F);
    send_cmd(0xD0); uint8_t d0[] = {0xA4, 0xA1}; send_data(d0, 2);
    
    send_cmd(0x21); // Inversion ON (IPS requirement)
    send_cmd(0x29); // Display ON
}

/**
 * @brief Sets the orientation of the display
 * @param landscape True for landscape mode, False for portrait mode
 */
void DUI::set_orientation(bool landscape) {
    send_cmd(0x36); // Start MADCTL CMD
    if (landscape) { // Landscape
        send_cmd(0x70); // MY=0. MX=1, MV=1 (Landscape)
        width = 320;
        height = 240;
    } 
    else { // Portrait
        send_cmd(0x00);
        width = 240;
        height = 320;
    }
}

/**
 * @brief Sets the canvas area for drawing
 * @param x1 X coordinate of the top-left corner
 * @param y1 Y coordinate of the top-left corner
 * @param x2 X coordinate of the bottom-right corner
 * @param y2 Y coordinate of the bottom-right corner
 */
void DUI::set_canvas(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2) {
    // Column Address Set (CASET)
    send_cmd(0x2A); // (CASET Command)
    uint8_t x_data[] = {(uint8_t)(x1 >> 8), (uint8_t)(x1 & 0xFF), (uint8_t)(x2 >> 8), (uint8_t)(x2 & 0xFF)};
    send_data(x_data, 4);

    // Row Address Set (RASET)
    send_cmd(0x2B); // (RASET Command)
    uint8_t y_data[] = {(uint8_t)(y1 >> 8), (uint8_t)(y1 & 0xFF), (uint8_t)(y2 >> 8), (uint8_t)(y2 & 0xFF)};
    send_data(y_data, 4);
    send_cmd(0x2C); // Write to RAMWR command (start addr pointer at x1, y1)
}

/**
 * @brief Writes a single character to the display at specified coordinates with given colors and font
 * @param x X coordinate of the top-left corner of the character
 * @param y Y coordinate of the top-left corner of the character
 * @param letter The ASCII character to be displayed (only supports A-Z in this example)
 * @param color 16-bit RGB565 color for the character
 * @param bg 16-bit RGB565 color for the background
 * @param font Pointer to the font bitmap array (e.g., font_5x7)
 */
void DUI::char_write(int16_t x, uint16_t y, char letter, uint16_t color, uint16_t bg, const uint8_t* font) {
    uint8_t char_index = (letter - 'A') * 5; // offset from 'A'

    // Set a canvas exactly the size of a character
    set_canvas(x, y, x + 4, y + 6);

    // Buffer for 5x7 character (35 pixels * 2 bytes)
    uint8_t buffer[70];
    int buf_idx = 0;

    for (int8_t column = 0; column < 5; column++) {
        uint8_t line = font[char_index + column];
        for (int8_t row = 0; row < 7; row++) {
            uint16_t pixel = (line & 0x01) ? color : bg; // Check if the least significant bit is set 
                                                         // to determine if we draw the character color 
                                                         // or the background color
            buffer[buf_idx++] = pixel >> 8;
            buffer[buf_idx++] = pixel & 0xFF;
            line >>= 1;
        }
    }
    send_data(buffer, 70); // Send the entire character buffer to the display
}

/**
 * @brief Waits for the TE signal to go HIGH then LOW, indicating the display has finished its current refresh cycle.
 */
void DUI::wait_for_te() {
    uint32_t timeout = 1000; // Timeout of 1000ms to prevent infinite blocking
    while (gpio_get_level(te) == 0 && timeout--) {
        esp_rom_delay_us(100); // Sleep for 1ms to avoid busy-waiting
    };

    timeout = 1000; // Reset timeout for the next phase
    while (gpio_get_level(te) == 1 && timeout--) {
        esp_rom_delay_us(100); // Sleep for 1ms to avoid busy-waiting
    };
}

/**
 * @brief Fills the entire canvas with a single color
 * @param color 16-bit RGB565 color to fill the canvas with
 */
void DUI::fill_canvas (uint16_t color) {
    set_canvas(0, 0, width - 1, height - 1);

// Allocate DMA-capable memory for one entire row of pixels
    size_t row_size_bytes = width * 2;
    uint8_t *row_buffer = (uint8_t *)heap_caps_malloc(row_size_bytes, MALLOC_CAP_DMA);
    
    // Fill the row buffer with our target color
    for (int i = 0; i < row_size_bytes; i += 2) {
        row_buffer[i] = color >> 8;
        row_buffer[i+1] = color & 0xFF;
    }

    // Blast the row to the display 'height' number of times
    for (int y = 0; y < height; y++) {
        send_data(row_buffer, row_size_bytes);
    }

    heap_caps_free(row_buffer); // Free memory when done
}

/**
 * @brief Puts the display into sleep mode
 */
void DUI::sleep() {
send_cmd(0x10); // sleep in
}