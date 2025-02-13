#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "pico/stdlib.h"
#include "hardware/pwm.h"
#include "hardware/adc.h"

//STYLE GUIDELINES: make all equals in a row, for defines make all number values in a row.
//LOGGING: For consistent logging messages make a seperate function that takes a variable to print and keep same format throughout.
//POTENTIAL LOGS: ADC init, DAC init, Scan Begin, Scan Complete (with time), Detected Peak, Quad and Null Voltages, Selected Setpoint.
//Everything else that wont be standard data recieved by the user can be printf statements for debugging

enum setPoint {
    NULL_POINT,
    QUAD_POINT, 
    PEAK_POINT   
};

enum setPoint set_point = QUAD_POINT;

#define PWM_PIN                   15                //GPIO #
#define ADC_INPUT                 0                 //GPIO 26
#define MAX_VOLTAGE_STEP          65535             //Maximum voltage step DAC
#define MIN_VOLTAGE_STEP          0                 //Minimum voltage step DAC
#define MAX_VOLTAGE               3.3f
#define MIN_VOLTAGE               0.0f
#define NOISE_FLOOR               0.01f
#define MAX_8BIT_STEPS            256               //Max value for 8-bit
#define MAX_12BIT_STEPS           4096              //Max value for a 12-bit 
#define MAX_16BIT_STEPS           65536             //Max value for a 16-bit 


//Modify these
const float tolerance           = 0.1f;             //Tolerance for reaching setpoint
const int average_per_read      = 1;                //Amount of averaged ADC reads per single ADC read
const int array_size            = MAX_12BIT_STEPS;  //Array size for sweep_pwm

//Initializing variable
float null_setpoint             = 0; 
float peak_setpoint             = 0;
float quad_setpoint             = 0;
int current_output_voltage_step = 0;

//Unused values thus far
//int null_voltage          = 0;
//int peak_voltage          = 0;
//int quad_plus_voltage     = 0;
//int quad_minus_voltage    = 0;
//int quad_voltage          = 0;


float read_voltage() {
    uint16_t raw_value = adc_read();  // Read the raw ADC value (0-4095)
    
    const float conversion_factor = MAX_VOLTAGE / MAX_12BIT_STEPS;  // 12-bit ADC, range is 4096 bits
    float total_voltage = 0;
    
    for (int i = 0; i < average_per_read; i++) {
        float voltage = raw_value * conversion_factor;
        total_voltage += voltage;
        sleep_ms(2); //Allow ADC to settle
    }

    return total_voltage / average_per_read;
}

// Function to set the PWM duty cycle based on an input voltage step
void set_pwm_dac(int voltage_step) {
    // Map the voltage_step (0 to 65535) directly to the PWM duty cycle
    // Ensure voltage_step stays within bounds (0 to 65535)
    if (voltage_step < MIN_VOLTAGE_STEP) voltage_step = MIN_VOLTAGE_STEP;
    if (voltage_step > MAX_VOLTAGE_STEP) voltage_step = MAX_VOLTAGE_STEP;

    // Set the PWM level on the GPIO pin
    pwm_set_gpio_level(PWM_PIN, voltage_step);

    current_output_voltage_step = voltage_step;

}

//FUNCTIONAL CODE USING FLOAT ARRAY WITH BOUNDS CHECK
void detect_peak(const float* result_array, size_t arraySize) {
    //Initialize global variable
    peak_setpoint = 0; //Reset peak setpoint in the case of another sweep
    
    //Initialize variables
    float current_y = 0.0f;
    float previous_y = 0.0f;
    
    //peak_voltage = 0;
    
    // Ensure the array size is valid
    if (result_array == NULL || arraySize <= 1) {
        
        printf("Error: Invalid array or array size.\n");
        return;
    }

    //[revious_y to the first value in the result array
    previous_y = result_array[0];

    // Iterate through the result array starting from the second element
    for (size_t i = 1; i < arraySize - 1; i++) {
        if (i >= arraySize) {
            
            printf("Error: Invalid array size.\n");
            return;
        }

        current_y = result_array[i];

        // Detect peak (increasing to decreasing)
        if (current_y > peak_setpoint) {
                // It's a peak, record it
                peak_setpoint = current_y;
                //peak_voltage = i;

        }
        previous_y = current_y;  // Update previous_y for next iteration
    }
           
    //DEBUG
    printf("PEAK DETECTED\n"); 
}

//FUNCTIONAL CODE USING FLOAT ARRAY WITH BOUNDS CHECK
void detect_null(const float* result_array, size_t arraySize) {
    //Initialize global variable
    null_setpoint    = MAX_VOLTAGE; //Starts with the max voltage so the algorithim can narrow to it's minimum
    
    //Initialize variables
    float current_y  = 0.0f;
    float previous_y = 0.0f;

    //null_voltage = 0;
    
    // Ensure the array size is valid
    if (result_array == NULL || arraySize <= 1) {
        
        printf("Error: Invalid array or array size.\n");
        return;
    }

    // Initializing the previous_y to the first value in the result array
    previous_y = result_array[0];

    // Iterate through the result array starting from the second element
    for (size_t i = 1; i < arraySize - 1; i++) {
        if (i >= arraySize) {
            
            printf("Error: Invalid array size.\n");
            return;
        }

        current_y = result_array[i];

        // Detect peak (increasing to decreasing)
        if (current_y < null_setpoint) {
                // It's a peak, record it
                
                if (current_y < NOISE_FLOOR) {

                }

                else {

                null_setpoint = current_y;
                //null_voltage = i;

                }
                

        }
        previous_y = current_y;  // Update previous_y for next iteration
    }
                    
    //DEBUG
    printf("NULL DETECTED\n"); 
}

void detect_quad() {
    quad_setpoint = peak_setpoint / 2;
}


//TODO: FIX THIS CODE
void go_to_setpoint() {
    float setpoint    = 0.0f;
    float read        = 0.0f;
    float difference  = 0.0f;
    int voltage_step  = 0;
    
    
    if (set_point == PEAK_POINT) {
        setpoint = peak_setpoint;
        printf("GO TO PEAK\n");
    }

    else if (set_point == QUAD_POINT) {
        setpoint = quad_setpoint;
        printf("GO TO QUAD\n");
    }

    else if (set_point == NULL_POINT) {
        setpoint = null_setpoint;
        printf("GO TO NULL\n");
    }


    printf("VOLTAGE STEP BEFORE 1ST READ %d \n", voltage_step);
    set_pwm_dac(voltage_step); 
    read = read_voltage();
    
    printf("READ AFTER VOLTAGE STEP %.4f V\n", read);
        
    voltage_step++;

    difference = fabs(read - setpoint);
    printf("DIFFERENCE %.4f V\n", difference);
    
//potential issue here with null point and tolerance value 
    while (difference > tolerance) { 
        if (voltage_step < MIN_VOLTAGE_STEP) {
            printf("INDEXING ERROR\n");
            break;
        }

        if (voltage_step > MAX_VOLTAGE_STEP) {
            printf("INDEXING ERROR\n");
            break;
        }
        
        set_pwm_dac(voltage_step);
        read = read_voltage();
        difference = fabs(read - setpoint);
        voltage_step++;
    }

    printf("INITIAL SETPOINT REACHED\n");
    
}

// Function to initialize the ADC hardware and select the input (GPIO 26)
void initialize_adc() {
    adc_init();  // Initialize the ADC hardware
    adc_select_input(ADC_INPUT);  // Select ADC input 0 (GPIO 26)
    adc_set_clkdiv(96000); // Set sampling rate to 500 samples per second

    //DEBUG
    printf("ADC INITIALIZED\n");
}

void initialize_pwm() {
    gpio_set_function(PWM_PIN, GPIO_FUNC_PWM);
    uint slice_num = pwm_gpio_to_slice_num(PWM_PIN);

    // Set the PWM frequency (optional, but typical for audio/analog-like signals)
    pwm_config config = pwm_get_default_config();
    pwm_config_set_clkdiv(&config, 1.f);  // Adjust clock divider to get desired frequency (fastest possible for RC filter)
    pwm_init(slice_num, &config, true);  // Start PWM with the config

    //DEBUG
    printf("DAC INITIALIZED\n");
}

/*
// Function to read the ADC value and return the corresponding voltage
float read_voltage() {
    uint16_t raw_value = adc_read();  // Read the raw ADC value (0-4095)
    
    // Convert the raw ADC value to voltage (3.3V reference)
    const float conversion_factor = 3.3f / 4096.0f;  // 12-bit ADC, range 0-4095
    float voltage = raw_value * conversion_factor;
    
    return voltage;
}
*/

void scanPWM() {
    /*
    const int MAX_12BIT_VOLTAGE = 4096;   // Max value for a 12-bit DAC (now 4096)
    const int MAX_16BIT_VOLTAGE = 65536;  // Max value for a 16-bit DAC (65536)
    const int MAX_8BIT_VOLTAGE = 256;
    */

    // Calculate the step size dynamically based on the 16-bit and 12-bit ranges
    
    const int step_size = MAX_16BIT_STEPS / array_size;  // 65536 / 4096 = 16

    float result_array[array_size];  // Create the array locally with float type

    // Scan through all possible 12-bit values (increment by the calculated step size in the 16-bit range)
    for (int pwm_value = MIN_VOLTAGE_STEP; pwm_value < array_size; pwm_value++) {
        // Calculate the corresponding 16-bit DAC value
        int dac_value = pwm_value * step_size;

        // Set the PWM DAC value (this controls the hardware or simulation)
        set_pwm_dac(dac_value);

        // Read the analog value from GPIO (this will be the system's response)
        float analog_value = read_voltage();  // Use float for the analog value
        //printf("Voltage: %.4f V \n", analog_value);

        // Store the result in the array
        result_array[pwm_value] = analog_value;
    }

    set_pwm_dac(MIN_VOLTAGE_STEP);

    // Pass the array as a reference to the functions to avoid duplication
    detect_peak(result_array, array_size);  // Detect peak first
    detect_null(result_array, array_size);  // Detect null next
    detect_quad();  // Detect quad next

/*
    // Print the result array with voltage values for all PWM steps
    for (int i = 0; i < array_size - 1; i += 10) {
        printf("PWM Value %d: %.3f V\n", i, result_array[i]);
    }
*/

    //DEBUG
    printf("PWM SCAN COMPLETE\n");
    printf("PEAK\n");
    printf("%.4f V\n", peak_setpoint);
    //printf("%d Index\n", peak_voltage);
    printf("NULL\n");
    printf("%.4f V\n", null_setpoint);
    //printf("%d Index\n", null_voltage);
    printf("QUAD\n");
    printf("%.4f V\n", quad_setpoint);

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
        
        //peak_voltage = peak_voltage * 16;
        //null_voltage = null_voltage * 16;

        for (;;) {
            
            go_to_setpoint();
            //set_pwm_dac(null_voltage);

            //printf("%d Index\n", current_voltage);
            //printf("%.4f V\n", quad_setpoint);

            sleep_ms(10000);
        }
    }
}

