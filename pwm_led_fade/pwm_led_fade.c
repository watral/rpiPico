/**
 * PWM DAC OUTPUT
 */

// 
// 
#include "pico/stdlib.h"
#include <stdio.h>
#include "hardware/pwm.h"

// Define the GPIO pin for the PWM output
#define PWM_PIN 15  // You can change this to any pin that supports PWM

//stdio_init_all(); // Initialize USB serial output

// Function to set the PWM duty cycle based on an input voltage between 0 and 3.3V
void set_pwm_dac(float voltage) {
    // Clamp the input voltage to the range 0 to 3.3
    if (voltage < 0) voltage = 0;
    if (voltage > 3.3) voltage = 3.3;

    // Map the voltage (0 to 3.3) to the PWM duty cycle (0 to 65535)
    uint16_t duty_cycle = (uint16_t)((voltage / 3.3) * 65535);

    // Set the PWM level on the GPIO pin
    pwm_set_gpio_level(PWM_PIN, duty_cycle);
}

int main() {
    // Initialize the chosen PWM pin
    gpio_set_function(PWM_PIN, GPIO_FUNC_PWM);
    uint slice_num = pwm_gpio_to_slice_num(PWM_PIN);

    // Set the PWM frequency (optional, but typical for audio/analog-like signals)
    pwm_config config = pwm_get_default_config();
    pwm_config_set_clkdiv(&config, 4.f);  // Adjust clock divider to get desired frequency (optional)
    pwm_init(slice_num, &config, true);  // Start PWM with the config

    // Example: Set a voltage of 1.65V (half of 3.3V)
    float voltage = 1.65;
    set_pwm_dac(voltage);  // Set the PWM duty cycle corresponding to 1.65V

    while (1) {
        // Here you can adjust the voltage value dynamically (e.g., from a sensor or input)
        // Just as an example, change the voltage from 0 to 3.3V and back in a loop
        for (voltage = 0; voltage <= 3.3; voltage += 0.1) {
            set_pwm_dac(voltage);  // Update PWM duty cycle to match voltage
            //printf(voltage); //maybe serial.print?
            sleep_ms(100);  // Wait for a while to observe the changes
        }

        for (voltage = 3.3; voltage >= 0; voltage -= 0.1) {
            set_pwm_dac(voltage);  // Update PWM duty cycle to match voltage
            sleep_ms(100);  // Wait for a while to observe the changes
        }
    }

    return 0;
}
