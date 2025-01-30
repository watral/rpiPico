#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/adc.h"

#define ADC_INPUT 0 //GPIO 26

// Function to initialize the ADC hardware and select the input (GPIO 26)
void initialize_adc() {
    adc_init();  // Initialize the ADC hardware
    adc_select_input(ADC_INPUT);  // Select ADC input 0 (GPIO 26)
}

// Function to read the ADC value and return the corresponding voltage
float read_voltage() {
    uint16_t raw_value = adc_read();  // Read the raw ADC value (0-4095)
    
    // Convert the raw ADC value to voltage (3.3V reference)
    const float conversion_factor = 3.3f / 4095.0f;  // 12-bit ADC, range 0-4095
    float voltage = raw_value * conversion_factor;
    
    return voltage;
}

int main() {
    stdio_init_all();  // Initialize standard input/output

    initialize_adc();  // Initialize ADC hardware and select input 0 (GPIO 26)

    while (true) {
        // Call the read_voltage function and print the result
        float voltage = read_voltage();
        printf("Voltage: %.2f V \n", voltage);

        sleep_ms(100);  // Delay for 100 ms
    }

    return 0;  // This line will never be reached, as the program runs infinitely
}
