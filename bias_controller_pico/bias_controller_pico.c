#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/pwm.h"
#include "hardware/adc.h"

#define PWM_PIN 15  //GPIO #
#define ADC_INPUT 0 //GPIO 26

#define MAX_VOLTAGE 65535 //Maximum voltage
#define MIN_VOLTAGE 0

enum setPoint {
    NULL_POINT,
    QUAD_POINT, 
    PEAK_POINT   
};

//Setpoint (Y value)
float null_setpoint = 0;
float peak_setpoint = 0;
float quad_setpoint = 0;

//Starting point (X value)
int null_index = 0;
int peak_index = 0;
int quad_plus_index = 0;
int quad_minus_index = 0;
int quad_index = 0;
int current_index = 0;

float threshold = 0.01;  // Threshold for quad points detection
const float tolerance = 0.01; //Tolerance for reaching setpoint

// Function to initialize the ADC hardware and select the input (GPIO 26)
void initialize_adc() {
    adc_init();  // Initialize the ADC hardware
    adc_select_input(ADC_INPUT);  // Select ADC input 0 (GPIO 26)

    //DEBUG
    printf("ADC INITIALIZED\n")
}

void initialize_pwm() {
    gpio_set_function(PWM_PIN, GPIO_FUNC_PWM);
    uint slice_num = pwm_gpio_to_slice_num(PWM_PIN);

    // Set the PWM frequency (optional, but typical for audio/analog-like signals)
    pwm_config config = pwm_get_default_config();
    pwm_config_set_clkdiv(&config, 1.f);  // Adjust clock divider to get desired frequency (optional)
    pwm_init(slice_num, &config, true);  // Start PWM with the config

    //DEBUG
    printf("DAC INITIALIZED\n");
}

// Function to read the ADC value and return the corresponding voltage
float read_voltage() {
    uint16_t raw_value = adc_read();  // Read the raw ADC value (0-4095)
    
    // Convert the raw ADC value to voltage (3.3V reference)
    const float conversion_factor = 3.3f / 4095.0f;  // 12-bit ADC, range 0-4095
    float voltage = raw_value * conversion_factor;
    
    return voltage;
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

int main()
{
    stdio_init_all();
    
    //Initialize hardware
    initialize_pwm();
    initialize_adc();

}
