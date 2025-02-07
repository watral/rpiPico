#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/pwm.h"
#include "hardware/adc.h"

#define PWM_PIN 15  //GPIO #
#define ADC_INPUT 0 //GPIO 26

#define MAX_VOLTAGE 65535 //Maximum voltage
#define MIN_VOLTAGE 0

//TODO: firmware isn't flashing to remake file and double check the make settings for it
//TODO: averaging filter for ADC readings

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
int null_voltage = 0;
int peak_voltage = 0;
int quad_plus_voltage = 0;
int quad_minus_voltage = 0;
int quad_voltage = 0;
int current_voltage = 0;

float threshold = 0.01;  // Threshold for quad points detection
const float tolerance = 0.01; //Tolerance for reaching setpoint

/*
//FUNCTIONAL CODE USING FLOAT ARRAY WITH BOUNDS CHECK
void detectPeaks(const float* resultArray, size_t arraySize) {
    // Ensure the array size is valid
    if (resultArray == NULL || arraySize <= 1) {
        
        printf("Error: Invalid array or array size.\n");
        return;
    }

    // Initializing the previous_y to the first value in the result array
    float previous_y = resultArray[0];
    int direction = 0; // 0 = unknown, 1 = increasing, -1 = decreasing

    // Iterate through the result array starting from the second element
    for (size_t i = 0; i < arraySize - 1; i++) {
        if (i >= arraySize) {
            
            printf("Error: Invalid array size.\n");
            return;
        }

        float current_y = resultArray[i];

        // Detect peak (increasing to decreasing)
        if (current_y < previous_y) {
            if (direction == 1) {
                // It's a peak, record it
                peak_setpoint = previous_y;
                peak_voltage = i - 1;
                
                //DEBUG
                printf("PEAK DETECTED\n");
                break;
            }
            direction = -1;  // Set direction to decreasing
        }

        else if (current_y > previous_y) {
            if (direction == -1) {
                // Do nothing
            }
            direction = 1;  // Set direction to increasing
        }

        previous_y = current_y;  // Update previous_y for next iteration
    }
}
*/

//FUNCTIONAL CODE USING FLOAT ARRAY WITH BOUNDS CHECK
void detectPeaks(const float* resultArray, size_t arraySize) {
    // Ensure the array size is valid
    if (resultArray == NULL || arraySize <= 1) {
        
        printf("Error: Invalid array or array size.\n");
        return;
    }

    // Initializing the previous_y to the first value in the result array
    float previous_y = resultArray[0];

    // Iterate through the result array starting from the second element
    for (size_t i = 1; i < arraySize - 1; i++) {
        if (i >= arraySize) {
            
            printf("Error: Invalid array size.\n");
            return;
        }

        float current_y = resultArray[i];

        // Detect peak (increasing to decreasing)
        if (current_y > previous_y) {
                // It's a peak, record it
                peak_setpoint = current_y;
                //peak_voltage = i - 1;

        }
        previous_y = current_y;  // Update previous_y for next iteration
    }
                    
    //DEBUG
    printf("PEAK DETECTED\n"); 
}

// Function to initialize the ADC hardware and select the input (GPIO 26)
void initialize_adc() {
    adc_init();  // Initialize the ADC hardware
    adc_select_input(ADC_INPUT);  // Select ADC input 0 (GPIO 26)

    //DEBUG
    printf("ADC INITIALIZED\n");
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
    const float conversion_factor = 3.3f / 4096.0f;  // 12-bit ADC, range 0-4095
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

void scanPWM() {
    const int MAX_12BIT_VOLTAGE = 4096;   // Max value for a 12-bit DAC (now 4096)
    const int MAX_16BIT_VOLTAGE = 65536;  // Max value for a 16-bit DAC (65536)
    const int MAX_8BIT_VOLTAGE = 256;

    // Calculate the step size dynamically based on the 16-bit and 12-bit ranges
    const int STEP_SIZE = MAX_16BIT_VOLTAGE / MAX_12BIT_VOLTAGE;  // 65536 / 4096 = 16

    float resultArray[MAX_12BIT_VOLTAGE];  // Create the array locally with float type

    // Scan through all possible 12-bit values (increment by the calculated step size in the 16-bit range)
    for (int pwm_value = 0; pwm_value < MAX_12BIT_VOLTAGE - 1; pwm_value++) {
        // Calculate the corresponding 16-bit DAC value
        int dac_value = pwm_value * STEP_SIZE;

        // Set the PWM DAC value (this controls the hardware or simulation)
        set_pwm_dac(dac_value);

        // Read the analog value from GPIO (this will be the system's response)
        float analog_value = read_voltage();  // Use float for the analog value
        //printf("Voltage: %.4f V \n", analog_value);

        // Store the result in the array
        resultArray[pwm_value] = analog_value;
        sleep_ms(10);
    }

    // Pass the array as a reference to the functions to avoid duplication
    detectPeaks(resultArray, MAX_12BIT_VOLTAGE);  // Detect peaks first
    //detectNulls(resultArray);  // Detect nulls next
    //detectQuads(resultArray);  // Detect quads next

    //DEBUG
    printf("PWM SCAN COMPLETE\n");
    printf("%.4f V \n", peak_setpoint);
}


int main()
{
    stdio_init_all();
    
    //Initialize hardware
    initialize_pwm();
    initialize_adc();

/*
    while (true) {
        // Call the read_voltage function and print the result
        float voltage = read_voltage();
        printf("Voltage: %.4f V \n", voltage);

        sleep_ms(100);  // Delay for 100 ms
    }
*/
    while(1) {
        scanPWM();
        peak_setpoint = 0;
        peak_voltage = 0;
    }
}
