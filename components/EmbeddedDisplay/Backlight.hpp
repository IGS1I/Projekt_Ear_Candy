#ifndef BACKLIGHT_H
#define BACKLIGHT_H

#include <cstdint>
#include "driver/gpio.h"

/********* Backlight Control Functions **********/

/** 
 * @brief Initializes the backlight PWM
 * @param pwm_pin The GPIO pin connected to the backlight PWM signal
 * 
 * This function configures the LEDC peripheral to generate a PWM signal on the specified GPIO pin.
 * The PWM frequency is set to 5 kHz to avoid audible hum,
 * and the duty cycle is initialized. to approximately 25% brightness. 
 */
void init_backlight(gpio_num_t pwm_pin);

/** 
 * @brief Sets the brightness of the backlight
 * @param duty The duty cycle for the PWM signal (0 to 8191 for 13-bit resolution)
 * 
 * This function updates the duty cycle of the PWM signal controlling the backlight brightness.
 * A duty cycle of 0 turns the backlight off, while a duty cycle of 8191 sets it to maximum brightness.
 */
void set_backlight_brightness(uint16_t level);

#endif // BACKLIGHT_H