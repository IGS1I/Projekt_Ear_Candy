#ifndef UISTUDIO_H
#define UISTUDIO_H

 #include <stdio.h>
 #include "driver/gpio.h"
 #include "driver/spi_master.h"

// Font bitmap for 5x7 characters (A and B as examples)
const uint8_t font_5x7[] = {
    0x7E, 0x11, 0x11, 0x11, 0x7E, // A
    0x7F, 0x49, 0x49, 0x49, 0x36  // B
};

class DUI {
    private :
        gpio_num_t dc, res, te;
        uint16_t width, height;
        spi_device_handle_t vspi; // SPI device handle for the display

        void send_cmd(uint8_t cmd);
        void send_data(const uint8_t *data, int length);
        void send_data_byte (uint8_t data);

    public:
        DUI(gpio_num_t sda_pin, gpio_num_t scl_pin, gpio_num_t cs_pin, gpio_num_t dc_pin, gpio_num_t res_pin, gpio_num_t te_pin, uint16_t w, uint16_t h);
        void init();
        void set_orientation(bool landscape);
        void set_canvas(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2);
        void char_write(int16_t x, uint16_t y, char letter, uint16_t color, uint16_t bg, const uint8_t* font);
        void wait_for_te();
        void fill_canvas(uint16_t color);
        void sleep();
};

#endif // UISTUDIO_H