#ifdef __cplusplus
extern "C" {
#endif

#include "UIStudio.hpp"
#include "Backlight.hpp"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

/************* Definitions *************/

#define SDA_PIN gpio_num_t(23)
#define SCL_PIN gpio_num_t(18)
#define CS_PIN  gpio_num_t(5)
#define RES_PIN gpio_num_t(4)
#define TE_PIN  gpio_num_t(34)

// Standard 16-bit (RGB565) Color Definitions
#define RED    0xF800
#define GREEN  0x07E0
#define BLUE   0x001F
#define BLACK  0x0000
#define WHITE  0xFFFF

// Create a global instance of the display driver
DUI display(SDA_PIN, SCL_PIN, CS_PIN, RES_PIN, TE_PIN, 320, 240);
display.init();
void app_main(void) {
    while (1) {
        // Main application loop
        vTaskDelay(1000 / portTICK_PERIOD_MS); // Delay for 1 second
        
        display.fill_canvas(RED);
        display.wait_for_te();
        display.fill_canvas(GREEN);
        display.wait_for_te();
        display.fill_canvas(BLUE);
        display.wait_for_te();
    }
}

#ifdef __cplusplus
}
#endif