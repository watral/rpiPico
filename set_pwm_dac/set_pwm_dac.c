/**
 * PWM DAC OUTPUT
 */

// Includes
#include "pico/stdlib.h"
#include <stdio.h>
#include "hardware/pwm.h"

// Define the GPIO pin for the PWM output
#define PWM_PIN 15  // You can change this to any pin that supports PWM

// Function to initialize the PWM on the specified pin
void initialize_pwm() {
    gpio_set_function(PWM_PIN, GPIO_FUNC_PWM);
    uint slice_num = pwm_gpio_to_slice_num(PWM_PIN);

    // Set the PWM frequency (optional, but typical for audio/analog-like signals)
    pwm_config config = pwm_get_default_config();
    pwm_config_set_clkdiv(&config, 1.f);  // Adjust clock divider to get desired frequency (optional)
    pwm_init(slice_num, &config, true);  // Start PWM with the config
}

// Function to set the PWM duty cycle based on an input voltage step
void set_pwm_dac(int voltage_step) {
    // Map the voltage_step (0 to 65535) directly to the PWM duty cycle
    // Ensure voltage_step stays within bounds (0 to 65535)
    if (voltage_step < 0) voltage_step = 0;
    if (voltage_step > 65535) voltage_step = 65535;

    // Set the PWM level on the GPIO pin
    pwm_set_gpio_level(PWM_PIN, voltage_step);
}

int main() {
    // Initialize PWM
    initialize_pwm();

    // Variable to cycle through PWM duty cycle from 0 to 65535
    int voltage_step = 0;
    int step = 1;  // Step for incrementing/decrementing the voltage step (PWM duty cycle)

    while (1) {
        // Update PWM duty cycle based on the voltage_step
        set_pwm_dac(voltage_step);

        // Wait for a short period to observe the changes
        sleep_ms(1);

        // Increment or decrement voltage_step
        voltage_step += step;

        // Reverse direction when reaching limits (0 or 65535)
        if (voltage_step >= 65535) {
            voltage_step = 65535;  // Ensure it doesn't exceed the max value
            step = -1;  // Reverse direction
        } else if (voltage_step <= 0) {
            voltage_step = 0;  // Ensure it doesn't go below the min value
            step = 1;  // Reverse direction
        }
    }

    return 0;
}
