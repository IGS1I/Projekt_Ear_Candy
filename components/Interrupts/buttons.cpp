#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "driver/gpio.h"

// Define GPIO pins (Adjust these based on your specific ESP32 board)
#define BACK_PIN GPIO_NUM_0 
#define HOME_PIN GPIO_NUM_1
#define MODE_PIN GPIO_NUM_2

// Queue handle to pass events from ISR to the loop
static QueueHandle_t gpio_evt_queue = NULL;

// Unified ISR handler
static void IRAM_ATTR gpio_isr_handler(void* arg) {
    uint32_t gpio_num = (uint32_t) arg;
    xQueueSendFromISR(gpio_evt_queue, &gpio_num, NULL);
}

void app_main(void) {
    // 1. Configure GPIOs
    gpio_config_t io_conf = {
        .intr_type = GPIO_INTR_NEGEDGE,    // Falling edge (like Arduino FALLING)
        .mode = GPIO_MODE_INPUT,          // Set as input
        .pin_bit_mask = (1ULL << BACK_PIN) | (1ULL << HOME_PIN) | (1ULL << MODE_PIN),
        .pull_up_en = GPIO_PULLUP_ENABLE, // Internal pull-up
    };
    gpio_config(&io_conf);

    // 2. Create a queue to handle GPIO events from ISR
    gpio_evt_queue = xQueueCreate(10, sizeof(uint32_t));

    // 3. Install GPIO ISR service and add handlers
    gpio_install_isr_service(0);
    gpio_add_handler(BACK_PIN, gpio_isr_handler, (void*) BACK_PIN);
    gpio_add_handler(HOME_PIN, gpio_isr_handler, (void*) HOME_PIN);
    gpio_add_handler(MODE_PIN, gpio_isr_handler, (void*) MODE_PIN);

    uint32_t io_num;
    
    // 4. The "Loop" (Infinite Task)
    while (1) {
        // This blocks until a button is pressed, saving CPU power
        if (xQueueReceive(gpio_evt_queue, &io_num, portMAX_DELAY)) {
            if (io_num == BACK_PIN) {
                printf("Back button pressed!\n");
                // Back action here
            } else if (io_num == HOME_PIN) {
                printf("Home button pressed!\n");
                // Home action here
            } else if (io_num == MODE_PIN) {
                printf("Mode button pressed!\n");
                // Mode action here
            }
        }
    }
}
// We create a "Last Pressed" clock to handle the jiggle (Debounce)
long last_time_pressed = 0; 
int debounce_delay = 200; // 200 milliseconds

// This is the "Order Box" loop
while (1) {
    // 1. Wait for a note in the mailbox (This doesn't stop the music!)
    if (xQueueReceive(gpio_evt_queue, &io_num, portMAX_DELAY)) {
        
        long current_time = esp_timer_get_time() / 1000; // Get current time in ms

        // 2. The Debounce Check: "Has it been long enough since the last press?"
        if (current_time - last_time_pressed > debounce_delay) {
            
            // 3. Do the action based on which pin was pushed
            if (io_num == BACK_PIN) { 
                // Skip to previous song
            }
            
            // Record the time of this successful press
            last_time_pressed = current_time;
        }
    }
}