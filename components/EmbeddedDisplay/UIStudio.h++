
 #include <stdio.h>
 #include "freertos/FreeRTOS.h"
 #include "freertos/task.h"
 #include "driver/gpio.h"
 #include "esp_rom_sys.h"

// Font bitmap for 5x7 characters (A and B as examples)
const uint8_t font_5x7[] = {
    0x7E, 0x11, 0x11, 0x11, 0x7E, // A
    0x7F, 0x49, 0x49, 0x49, 0x36  // B
};

class DUI {
    private :
        gpio_num_t sda, scl, cs, res, te;
        uint16_t width, height;
public:
    // The 9-bit protocol: Bit 0 is D/C, Bits 1-8 are Data
    void send_9bit(uint8_t data, bool isData);
    void set_orientation(bool horizontal);
    void char_write(int16_t x, uint16_t y, char letter, uint16_t color, uint16_t bg, const uint8_t* font);
    void setup();