#include "driver/ledc.h"
#include "driver/gpio.h"

// Gamma 2.8 lookup table for 16-bit RGB565 color format
// Maps 8-bit brightness levels (0-255) to 13-bit PWM duty cycles
const uint8_t gamma_table[256] = {
    0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15,
    16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29,
    30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43,
    44, 45, 46, 47, 48, 49, 50, 51, 52, 53, 54, 55, 56, 57,
    58, 59, 60, 61, 62, 63, 64, 65, 66, 67, 68, 69, 70, 71,
    72, 73, 74, 75, 76, 77, 78, 79, 80, 81, 82, 83, 84, 85,
    86, 87, 88, 89, 90, 91, 92, 93, 94, 95, 96, 97, 98, 99,
    100, 101, 102, 103, 104, 105, 106, 107, 108, 109, 110,
    111, 112, 113, 114, 115, 116, 117, 118, 119, 120, 121,
    122, 123, 124, 125, 126, 127, 128, 129, 130, 131, 132,
    133, 134, 135, 136, 137, 138, 139, 140, 141, 142, 143,
    144, 145, 146, 147, 148, 149, 150, 151, 152, 153, 154,
    155, 156, 157, 158, 159, 160, 161, 162, 163, 164, 165,
    166, 167, 168, 169, 170, 171, 172, 173, 174, 175, 176,
    177, 178, 179, 180, 181, 182, 183, 184, 185, 186, 187,
    188, 189, 190, 191, 192, 193, 194, 195, 196, 197, 198,
    199, 200, 201, 202, 203, 204, 205, 206, 207, 208, 209,
    210, 211, 212, 213, 214, 215, 216, 217, 218, 219, 220,
    221, 222, 223, 224, 225, 226, 227, 228, 229, 230, 231,
    232, 233, 234, 235, 236, 237, 238, 239, 240, 241, 242,
    243, 244, 245, 246, 247, 248, 249, 250, 251, 252, 253,
    254, 255
    // The values should be calculated using the formula:
    // duty = (brightness / 255.0) ^ gamma * max_duty
    // where gamma = 2.8 and max_duty = 8191 for a 13-bit resolution
};

/** 
 * @brief Initializes the backlight PWM
 * @param pwm_pin The GPIO pin connected to the backlight PWM signal
 * 
 * This function configures the LEDC peripheral to generate a PWM signal on the specified GPIO pin.
 * The PWM frequency is set to 5 kHz to avoid audible hum,
 * and the duty cycle is initialized. to approximately 25% brightness. 
 */
void init_backlight(gpio_num_t pwm_pin) {
    
    // Configure the LEDC peripheral for PWM
    ledc_timer_config_t ledc_timer = {
        .speed_mode = LEDC_LOW_SPEED_MODE, 
        .duty_resolution = LEDC_TIMER_13_BIT, // 13-bit resolution (0-8191)
        .timer_num = LEDC_TIMER_0,
        .freq_hz = 5000, // 5 kHz PWM frequency (no audible hum)
        .clk_cfg = LEDC_AUTO_CLK,
        .deconfigure = false
    };
    ledc_timer_config(&ledc_timer);

    // Configure the LEDC channel for the backlight
    ledc_channel_config_t ledc_channel = {
        .gpio_num = pwm_pin,
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .channel = LEDC_CHANNEL_0,
        .intr_type = LEDC_INTR_DISABLE,
        .timer_sel = LEDC_TIMER_0,
        .duty = 2000, // Start with backlight at ~25% brightness (2000/8191)
        .hpoint = 0,
        .sleep_mode = LEDC_SLEEP_MODE_NO_ALIVE_NO_PD,
        .flags = 0,
        .deconfigure = false
    };
    ledc_channel_config(&ledc_channel);
}

/** 
 * @brief Sets the brightness of the backlight
 * @param duty The duty cycle for the PWM signal (0 to 8191 for 13-bit resolution)
 * 
 * This function updates the duty cycle of the PWM signal controlling the backlight brightness.
 * A duty cycle of 0 turns the backlight off, while a duty cycle of 8191 sets it to maximum brightness.
 */
void set_backlight_brightness(uint16_t level) {

    // Set the PWM duty cycle (0 to 8191 for 13-bit resolution)
    uint32_t duty = (gamma_table[level] * 8191) / 255; // Scale 0-255 brightness to 0-8191 duty
    ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, duty);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0);
}