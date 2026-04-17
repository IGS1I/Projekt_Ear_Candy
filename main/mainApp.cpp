#ifdef __cplusplus
extern "C" {
#endif

#include "UIStudio.hpp"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

/************* Definitions *************/

#define SDA_PIN gpio_num_t(23)
#define SCL_PIN gpio_num_t(18)
#define CS_PIN  gpio_num_t(5)
#define DC_PIN  gpio_num_t(22)
#define RES_PIN gpio_num_t(4)
#define TE_PIN  gpio_num_t(34)

// Standard 16-bit (RGB565) Color Definitions
#define RED    0xF800
#define GREEN  0x07E0
#define BLUE   0x001F

// Pass the DC pin into the constructor
DUI display(SDA_PIN, SCL_PIN, CS_PIN, DC_PIN, RES_PIN, TE_PIN, 320, 240);

void app_main(void) {
    display.init();
    
    // Main Loop
    while (1) {
        display.fill_canvas(RED);
        // display.wait_for_te();
        vTaskDelay(pdMS_TO_TICKS(1000));
        
        display.fill_canvas(GREEN);
        // display.wait_for_te();
        vTaskDelay(pdMS_TO_TICKS(1000));
        
        display.fill_canvas(BLUE);
        // display.wait_for_te();
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

#ifdef __cplusplus
}
#endif