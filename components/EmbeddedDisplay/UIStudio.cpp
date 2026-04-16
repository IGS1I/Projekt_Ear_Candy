#include "UIStudio.hpp"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_rom_sys.h"

/********** DUI Class Implementation **********/

void DUI::send_9bit(uint8_t data, bool isData) {
    gpio_set_level(cs, 0);

    // 1. Data/Command Bit (1st bit)
    gpio_set_level(sda, isData ? 1 : 0);
    gpio_set_level(scl, 0);
    esp_rom_delay_us(1); // Short delay for spacing
    gpio_set_level(scl, 1); // Sampled on rising edge

    // 2. 8-bit Payload
    for (int i = 0; i < 8; i++) {
        gpio_set_level(sda, (data & 0x80) ? 1 : 0);
        data <<= 1;
        gpio_set_level(scl, 0);
        esp_rom_delay_us(1); // Short delay for spacing
        gpio_set_level(scl, 1); // Sampled on rising edge
    }
    gpio_set_level(cs, 1); // end of packet
}

// Constructor to initialize GPIO pins and display dimensions
DUI::DUI(gpio_num_t sda_pin, gpio_num_t scl_pin, gpio_num_t cs_pin, gpio_num_t res_pin, gpio_num_t te_pin, uint16_t w, uint16_t h) 
    : sda(sda_pin), scl(scl_pin), cs(cs_pin), res(res_pin), te(te_pin), width(w), height(h) {}

void DUI::init() {
    //GPIO Configuration
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << sda) | (1ULL << scl) | (1ULL << cs) | (1ULL << res) | (1ULL << te),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    gpio_config(&io_conf);

    // Hardware Reset Sequence
    gpio_set_level(res, 0);
    vTaskDelay(pdMS_TO_TICKS(100));
    gpio_set_level(res, 1);
    vTaskDelay(pdMS_TO_TICKS(150));

    // Initialization Sequence (Standard ST7789 IPS)
    send_9bit(0x11, false); // Sleep Out
    vTaskDelay(pdMS_TO_TICKS(120));

    set_orientation(true); // Landscape mode
    send_9bit(0x3A, false); send_9bit(0x05, true); // 16-bit color/RGB565 (COLMOD)

    // NHD Display Optimization Registers
    send_9bit(0xB2, false); send_9bit(0x0C, true); send_9bit(0x0C, true);
    send_9bit(0x00, true); send_9bit(0x33, true); send_9bit(0x33, true);
    send_9bit(0xB7, false); send_9bit(0x35, true);
    send_9bit(0xBB, false); send_9bit(0x2B, true);
    send_9bit(0xC0, false); send_9bit(0x2C, true);
    send_9bit(0xC2, false); send_9bit(0x01, true); send_9bit(0xFF, true);
    send_9bit(0xC3, false); send_9bit(0x11, true);
    send_9bit(0xC4, false); send_9bit(0x20, true);
    send_9bit(0xC6, false); send_9bit(0x0F, true);
    send_9bit(0xD0, false); send_9bit(0xA4, true); send_9bit(0xA1, true);
    
    send_9bit(0x21, false); // Inversion ON (IPS requirement)
    send_9bit(0x29, false); // Display ON
}

void DUI::set_orientation(bool landscape) {
    send_9bit(0x36, false); // Start MADCTL CMD
    if (landscape) { // Landscape
        send_9bit(0x70, true); // MY=0. MX=1, MV=1 (Landscape)
        width = 320;
        height = 240;
    } 
    else { // Portrait
        send_9bit(0x00, true);
        width = 240;
        height = 320;
    }
}

void DUI::set_canvas(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2) {
    // Column Address Set (CASET)
    send_9bit(0x2A, false); // (CASET Command)
    send_9bit(x1 >> 8, true); send_9bit(x1 & 0xFF, true);
    send_9bit(x2 >> 8, true); send_9bit(x2 & 0xFF, true);

    // Row Address Set (RASET)
    send_9bit(0x2B, false); // (RASET Command)
    send_9bit(y1 >> 8, true); send_9bit(y1 & 0xFF, true);
    send_9bit(y2 >> 8, true); send_9bit(y2 & 0xFF, true);

    // Write to RAMWR command (start addr pointer at x1, y1)
    send_9bit(0x2C, false);
}

void DUI::char_write(int16_t x, uint16_t y, char letter, uint16_t color, uint16_t bg, const uint8_t* font) {
    uint8_t char_index = (letter - 'A') * 5; // offset from 'A'

    // Set a canvas exactly the size of a character
    set_canvas(x, y, x + 4, y + 6);

    for (int8_t column = 0; column < 5; column++) {
        uint8_t line = font[char_index + column];
        for (int8_t row = 0; row < 7; row++) {
            if (line & 0x01) {
                send_9bit(color >> 8, true);
                send_9bit(color & 0xFF, true);
            } else {
                send_9bit(bg >> 8, true);
                send_9bit(bg & 0xFF, true);
            }
            line >>= 1;
        }
    }
}

/*
* Waits for the TE signal to go HIGH then LOW, indicating the display has finished its current refresh cycle.
*/
void DUI::wait_for_te() {
    while (gpio_get_level(te) == 0);
    while (gpio_get_level(te) == 1);
}

// Fills display with color
void DUI::fill_canvas (uint16_t color) {
set_canvas(0,0,239,319);

    // 240 * 320 = 76,800 pixels
    for (uint32_t i = 0; i < 76800; i++) {
        // sending 2 bytes per pixel (since 16-bit mode)
        send_9bit(color >> 8, true); // high byte (R + upper G)
        send_9bit(color & 0xFF, true); // Low byte (lower G + B)
    }
}

// Puts display in low power mode
void DUI::sleep() {
send_9bit(0x10, false); // sleep in
}