#ifdef __cplusplus
extern "C" {
#endif

#include "UIStudio.h++"
#include "Backlight.h++"

/************* Definitions *************/

#define SDA_PIN 23
#define SCL_PIN 18
#define CS_PIN  5
#define RES_PIN 4
#define TE_PIN 34

// Standard 16-bit (RGB565) Color Definitions
#define RED    0xF800
#define GREEN  0x07E0
#define BLUE   0x001F
#define BLACK  0x0000
#define WHITE  0xFFFF

// Create a global instance of the display driver
DUI display(SDA_PIN, SCL_PIN, CS_PIN, RES_PIN, TE_PIN, 320, 240);

void app_main(void) {
    while (1) {
        // Main application loop
        vTaskDelay(1000 / portTICK_PERIOD_MS); // Delay for 1 second
        
        fill_canvas(RED);
        delay(1000);
        fill_canvas(GREEN);
        delay(1000);
        fill_canvas(BLUE);
        delay(1000);
    }
}

#ifdef __cplusplus
}
#endif