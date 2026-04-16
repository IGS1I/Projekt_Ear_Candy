#include "driver/ledc.h"
#include "Backlight.h++"

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
        .clk_cfg = LEDC_AUTO_CLK
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
        .hpoint = 0
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