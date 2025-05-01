#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include "pico/stdlib.h"
#include "hardware/pwm.h"
#include "hardware/adc.h"

//TODO: BUTTON RESWEEP, SWITCH SELECT QUAD_PLUS/NULL OR QUAD_PLUS/PEAK, POTENTIAL EDGE CASE HANDELING

/*
SUGGESTED RANGE FOR MODIFIED VARIABLES/ORIGINAL VALUES

TOLERANCE: 0.0032 -> 0.1 : ORIGINAL VALUE 0.0064 QUAD, 0.05 PEAK/NULL : CHANGE FOR GREATER CONTROL LOOP ACCURACY, DECREASE FOR HIGHER ACCURACY, INCREASE FOR NOISE MITIGATION
QUAD_BUFFER: 5 -> 100    : ORIGINAL VALUE 10     : INCREASE THIS VALUE IF WRONG QUAD (+/-) IS FOUND AFTER SWEEP, DECREASE THIS VALUE IF QUAD IS NOT FOUND AT ALL
NULL_BUFFER: 25 -> 500   : ORIGINAL VALUE 200    : INCREASE THIS VALUE IF NULL DOESNT SETTLE, DECREASE THIS VALUE IF THERE IS TO MUCH NOISE WHILE NULL LOCKED 
PEAK_BUFFER: 25 -> 500   : ORIGINAL VALUE 200    : SAME AS ABOVE BUT FOR PEAK
GAIN: 8->32              : ORIGINAL VALUE 8      : NOT ADVISED TO CHANGE, INCREASE FOR FASTER CONVERGENCE TIME TO SETPOINT
*/

//MODIFY THESE--------------------------------------------------------------------------------------------------------------------------------------------------------
float tolerance           = 0.05f;              //Tolerance for reaching setpoint in volts. 
int quad_buffer           = 10;                   //Buffer for quad + / - search. Requires x # of measurments that fall within tolerance and slope conditions before it is deemed valid. Mitigates noise
int peak_buffer           = 50;                  //Buffer for peak control loop. Requires x # of measurments with growing error before changing direction. Mitigates noise
int null_buffer           = 50;                  //Same as above but for null
#define GAIN                8                     //Gain of overall control loop. At each calculation DAC will correct x amount of voltage steps. Change not recommended.
//--------------------------------------------------------------------------------------------------------------------------------------------------------------------

enum setPoint {
    NULL_POINT,
    QUAD_MINUS,
    QUAD_PLUS, 
    PEAK_POINT   
};

enum setPoint set_point = NULL_POINT;

// Serial command buffer
#define MAX_CMD_LEN               100
char cmd_buffer[MAX_CMD_LEN];
int cmd_index = 0;
bool cmd_ready = false;

// Parameter change flags
bool params_changed = false;

#define PWM_PIN                   15                 //PIN 20
#define BUTTON_PIN                14                 //PIN 19
#define LED_PIN                   13
#define NULL_PIN                  18
#define QUAD_PLUS_PIN             19
#define QUAD_MINUS_PIN            20
#define PEAK_PIN                  21
#define ADC_INPUT                 0                  //GPIO 26/PIN 31
#define MAX_VOLTAGE_STEP          65535              //Maximum voltage step DAC
#define MIN_VOLTAGE_STEP          0                  //Minimum voltage step DAC
#define MAX_VOLTAGE               3.3f
#define MIN_VOLTAGE               0.0f
#define NOISE_FLOOR               0.01f              //Noise floor (for null bias)
#define MAX_8BIT_STEPS            256                //Max value for 8-bit
#define MAX_12BIT_STEPS           4096               //Max value for 12-bit 
#define MAX_16BIT_STEPS           65536              //Max value for 16-bit 
#define MOVE_RIGHT                1
#define MOVE_LEFT                 0


const int average_per_read      = 4000;                 //Amount of averaged ADC reads per single ADC read
const int array_size            = MAX_12BIT_STEPS;      //Array size for sweep_pwm

//Initializing variable
float null_setpoint             = 0.0f; 
float peak_setpoint             = 0.0f;
float quad_setpoint             = 0.0f;
float selected_setpoint         = 0.0f;
float current_input_voltage     = 0.0f;
int current_output_voltage_step = 0;

volatile bool button_pressed = false;
volatile bool null_pin = false;
volatile bool quad_minus_pin = false;
volatile bool quad_plus_pin = false;
volatile bool peak_pin = false;
//------------------------------------------------------------------------------------------------------------------
// Serial command processing functions
void process_command(char* cmd) {
    char param_name[20];
    float param_value;
    
    // Parse command for parameter updates
    if (sscanf(cmd, "set %19s %f", param_name, &param_value) == 2) {
        if (strcmp(param_name, "tolerance") == 0) {
            if (param_value >= 0.0032f && param_value <= 0.1f) {
                tolerance = param_value;
                printf("Tolerance set to: %.4f V\n", tolerance);
                params_changed = true;
            } else {
                printf("Invalid tolerance value. Range: 0.0032 to 0.1\n");
            }
        } 
        else if (strcmp(param_name, "quad_buffer") == 0) {
            if (param_value >= 5 && param_value <= 100) {
                quad_buffer = (int)param_value;
                printf("Quad buffer set to: %d\n", quad_buffer);
                params_changed = true;
            } else {
                printf("Invalid quad_buffer value. Range: 5 to 100\n");
            }
        } 
        else if (strcmp(param_name, "null_buffer") == 0) {
            if (param_value >= 25 && param_value <= 500) {
                null_buffer = (int)param_value;
                printf("Null buffer set to: %d\n", null_buffer);
                params_changed = true;
            } else {
                printf("Invalid null_buffer value. Range: 25 to 500\n");
            }
        } 
        else if (strcmp(param_name, "peak_buffer") == 0) {
            if (param_value >= 25 && param_value <= 500) {
                peak_buffer = (int)param_value;
                printf("Peak buffer set to: %d\n", peak_buffer);
                params_changed = true;
            } else {
                printf("Invalid peak_buffer value. Range: 25 to 500\n");
            }
        } 
        else {
            printf("Unknown parameter: %s\n", param_name);
        }
    } 
    else if (strcmp(cmd, "status") == 0) {
        // Print current parameter values
        printf("\n--- CURRENT PARAMETERS ---\n");
        printf("Tolerance    : %.4f\n", tolerance);
        printf("Quad Buffer  : %d\n", quad_buffer);
        printf("Null Buffer  : %d\n", null_buffer);
        printf("Peak Buffer  : %d\n", peak_buffer);
        printf("Gain         : %d (fixed)\n", GAIN);
        printf("------------------------\n\n");
    } 
    else if (strcmp(cmd, "help") == 0) {
        printf("\n--- COMMAND HELP ---\n");
        printf("set tolerance [value]    - Set tolerance (0.0032 to 0.1)\n");
        printf("set quad_buffer [value]  - Set quad buffer (5 to 100)\n");
        printf("set null_buffer [value]  - Set null buffer (25 to 500)\n");
        printf("set peak_buffer [value]  - Set peak buffer (25 to 500)\n");
        printf("status                   - Show current parameter values\n");
        printf("sweep                    - Force a new sweep operation\n");
        printf("help                     - Show this help menu\n");
        printf("--------------------\n\n");
    }
    else if (strcmp(cmd, "sweep") == 0) {
        printf("Initiating manual sweep...\n");
        button_pressed = true;
    }
    else {
        printf("Unknown command. Type 'help' for available commands.\n");
    }
}

// Process any received characters and handle commands
void check_serial_input() {
    int c;
    
    // Non-blocking check for any available characters
    while ((c = getchar_timeout_us(0)) != PICO_ERROR_TIMEOUT) {
        // Echo received character back to terminal
        putchar(c);
        
        if (c == '\r' || c == '\n') {
            if (cmd_index > 0) {
                cmd_buffer[cmd_index] = '\0';  // Null terminate
                cmd_ready = true;
                putchar('\n');  // Echo newline
            }
        } 
        else if (c == 8 || c == 127) {  // Backspace or Delete
            if (cmd_index > 0) {
                cmd_index--;
                // Echo backspace-space-backspace to erase character on terminal
                putchar(8);
                putchar(' ');
                putchar(8);
            }
        } 
        else if (cmd_index < MAX_CMD_LEN - 1) {
            cmd_buffer[cmd_index++] = c;
        }
    }
    
    if (cmd_ready) {
        process_command(cmd_buffer);
        cmd_index = 0;
        cmd_ready = false;
    }
}
//LOGGING

void log_index_error(int index) 
{
    printf("Indexing error: invalid index %d encountered. Please check array bounds.\n", index);
    printf("\n");
}

void log_pwm_scan_complete() 
{
    printf("PWM SCAN COMPLETE\n");

    printf("%-10s: %.4f V\n", "PEAK", peak_setpoint);
    printf("%-10s: %.4f V\n", "NULL", null_setpoint);
    printf("%-10s: %.4f V\n", "QUAD", quad_setpoint);

    printf("\n");
}

void log_incompatible_array_size(int expected_size, int actual_size) 
{
    printf("ARRAY SIZE ERROR\n");

    printf("%-20s: %d\n", "Expected size", expected_size);
    printf("%-20s: %d\n", "Actual size", actual_size);

    printf("\n");
}

void log_selected_setpoint() 
{
    //Check which setpoint was selected and log it
    if (set_point == PEAK_POINT) 
    {
        printf("SELECTED SETPOINT: PEAK\n");
    }
    else if (set_point == QUAD_PLUS) 
    {
        printf("SELECTED SETPOINT: QUAD PLUS\n");
    }
    else if (set_point == QUAD_MINUS) 
    {
        printf("SELECTED SETPOINT: QUAD MINUS\n");
    }
    else if (set_point == NULL_POINT) 
    {
        printf("SELECTED SETPOINT: NULL\n");
    }
    else 
    {
        //If set_point does not match known values, log an error
        printf("SELECTED SETPOINT: UNKNOWN\n");
    }

    printf("\n");
}

void log_setpoint_reached(float read, float difference) 
{

    printf("Setpoint reached   :\n");
    printf("Selected setpoint  : %.4f V\n", selected_setpoint);
    printf("Read value         : %.4f V\n", read);
    printf("Tolerance          : %.4f V\n", tolerance);
    printf("Difference         : %.4f V\n", difference);

    // Add an extra newline at the end of the log for separation
    printf("\n");
}

void display_startup_message() {
    printf("\n\n");
    printf("==========================================\n");
    printf("     PWM Control System with Serial UI    \n");
    printf("==========================================\n");
    printf("Current Parameters:\n");
    printf("  Tolerance    : %.4f\n", tolerance);
    printf("  Quad Buffer  : %d\n", quad_buffer);
    printf("  Null Buffer  : %d\n", null_buffer);
    printf("  Peak Buffer  : %d\n", peak_buffer);
    printf("  Gain         : %d (fixed)\n", GAIN);
    printf("\n");
    printf("Type 'help' for available commands\n");
    printf("==========================================\n\n");
}

//-------------------------------------------------------------------------------------------------------------------

//Functional Code

float set_precision(float value) //truncate all values to third decimal place (reason: ADC: 0.0008 resolution, truncate to 0.000)
{
    value = value * 1000.0f;
    value = floor(value);
    value = value / 1000.0f;
    
    return value;
}

void initialize_adc() 
{
    adc_init();  
    adc_select_input(ADC_INPUT);  // Select ADC input 0 (GPIO 26)
    //adc_set_clkdiv(96000);        // Set sampling rate to 500 samples per second
    adc_set_clkdiv(1); // Set sampling rate to max
}

void initialize_pwm() 
{
    gpio_set_function(PWM_PIN, GPIO_FUNC_PWM);
    uint slice_num = pwm_gpio_to_slice_num(PWM_PIN);
    pwm_config config = pwm_get_default_config();
    pwm_config_set_clkdiv(&config, 1.f); // Set to fastest frequency to work with external RC filter
    pwm_init(slice_num, &config, true);  // Start PWM with the config
}

void initialize_button()
{
    gpio_init(BUTTON_PIN);
    gpio_set_dir(BUTTON_PIN, GPIO_IN);
    gpio_pull_up(BUTTON_PIN);
}

void initialize_led()
{
    gpio_init(LED_PIN);
    gpio_set_dir(LED_PIN, GPIO_OUT);
}

void initialize_shorts()
{
    gpio_init(NULL_PIN);
    gpio_init(QUAD_PLUS_PIN);
    gpio_init(QUAD_MINUS_PIN);
    gpio_init(QUAD_PLUS_PIN);
    gpio_set_dir(NULL_PIN, GPIO_IN);
    gpio_set_dir(QUAD_MINUS_PIN, GPIO_IN);
    gpio_set_dir(QUAD_PLUS_PIN, GPIO_IN);
    gpio_set_dir(PEAK_PIN, GPIO_IN);
}

float read_voltage() 
{
    uint16_t raw_value = adc_read(); 
    
    const float conversion_factor = MAX_VOLTAGE / MAX_12BIT_STEPS;  // 12-bit ADC, range is 4096 bits
    
    float total_voltage = 0;
    
    for (int i = 0; i < average_per_read; i++) {
        float voltage = raw_value * conversion_factor;
        total_voltage += voltage;
        //sleep_ms(2); //Allow ADC to settle 
        //removed sleep for higher sample rate but higher average
    }

    return set_precision(total_voltage / average_per_read);
}

void set_pwm_dac(int voltage_step) 
{
    //Ensure voltage_step stays within bounds (0 to 65535)
    if (voltage_step < MIN_VOLTAGE_STEP) voltage_step = MIN_VOLTAGE_STEP;
    if (voltage_step > MAX_VOLTAGE_STEP) voltage_step = MAX_VOLTAGE_STEP;

    // Truncate the last 4 bits to make it a 12-bit value
    //voltage_step &= 0xFFF0;  // Mask out the last 4 bits

    //Set the PWM level on the GPIO pin
    pwm_set_gpio_level(PWM_PIN, voltage_step);

    current_output_voltage_step = voltage_step;

}

void detect_peak(const float* result_array, size_t arraySize) 
{
    
    peak_setpoint = 0; //Reset peak setpoint in the case of another sweep
    
    //Initialize variables
    float current_read = 0.0f;
    float previous_read = 0.0f;
     
    //Ensure the array size is valid
    if (result_array == NULL || arraySize <= 1) 
    {  
        log_incompatible_array_size(MAX_12BIT_STEPS, arraySize);
        return;
    }

    previous_read = result_array[0]; //Populate first value

    //Iterate through the result array starting from the second element
    for (size_t i = 1; i < arraySize - 1; i++) 
    {
        if (i >= arraySize) 
        {  
            log_incompatible_array_size(MAX_12BIT_STEPS, arraySize);
            return;  
        }

        current_read = result_array[i];

        //Keep largest value
        if (current_read > peak_setpoint) 
        {
                peak_setpoint = current_read;
        }

        previous_read = current_read;  //Record previous read for next iteration

    }
           
}

void detect_null(const float* result_array, size_t arraySize) {

    null_setpoint = MAX_VOLTAGE; //Reset null setpoint and initialize to MAX_VOLTAGE to narrow to it's minimum
    
    //Initialize variables
    float current_read  = 0.0f;
    float previous_read = 0.0f;

    // Ensure the array size is valid
    if (result_array == NULL || arraySize <= 1) 
    { 
        log_incompatible_array_size(MAX_12BIT_STEPS, arraySize);
        return;
    }

    previous_read = result_array[0];

    //Iterate through the result array starting from the second element
    for (size_t i = 1; i < arraySize - 1; i++) 
    {
        if (i >= arraySize) 
        {
            log_incompatible_array_size(MAX_12BIT_STEPS, arraySize);
            return;
        }

        current_read = result_array[i];

        //Keep smallest value
        if (current_read < null_setpoint) 
        {

                //Ignore values under the noise floor of the external circuit
                if (current_read < NOISE_FLOOR) {
                    
                }

                else {

                null_setpoint = current_read;

                }        
        }
        
        previous_read = current_read;  //Record previous read for next iteration

    }
                
}

void detect_quad() 
{
    //Calculate quad using the detected peak, verified by Brooke
    quad_setpoint = peak_setpoint / 2;
}

void sweep() 
{

    const int step_size = MAX_16BIT_STEPS / array_size; //Scale to array size
    
    float result_array[array_size]; // Create the array locally with float type
    float read = 0.0f;

    for (int pwm_value = MIN_VOLTAGE_STEP; pwm_value < array_size; pwm_value++) 
    {

        int dac_value = pwm_value * step_size;
        
        set_pwm_dac(dac_value);
        //printf("%d:", dac_value);
        read = read_voltage();  
        //printf("%.4f\n", read);
    
        result_array[pwm_value] = read;

    }

    set_pwm_dac(MIN_VOLTAGE_STEP);

    //Pass the array as a reference to the functions to avoid duplication
    detect_peak(result_array, array_size); 
    detect_null(result_array, array_size);  
    detect_quad();                          

    //log_pwm_scan_complete();

}

void go_to_setpoint() 
{
    float read        = 0.0f;
    float difference  = 0.0f;
    int voltage_step  = 2500;
    int step_size     = MAX_16BIT_STEPS / MAX_12BIT_STEPS; //Use 12-bit step size for faster convergence
    
    log_selected_setpoint();

    set_pwm_dac(voltage_step); 
    
    sleep_ms(100); //Allow DAC to settle (REASON: PWM signal from MAX voltage to MIN voltage takes some time to settle through the external RC circuit)

    read = read_voltage();
 
    voltage_step++;

    difference = fabs(read - selected_setpoint);

    while (difference > tolerance) 
    { 
        
        //Ensure voltage values stay within limits
        if (voltage_step < MIN_VOLTAGE_STEP) 
        {
            log_index_error(voltage_step);
            break;
        }

        if (voltage_step > MAX_VOLTAGE_STEP) 
        {  
            log_index_error(voltage_step);
            break;
        }
        
        //Keep adjusting output until setpoint is reached
        set_pwm_dac(voltage_step);
        read = read_voltage();
        difference = fabs(read - selected_setpoint);
        voltage_step += step_size;
    }

    log_setpoint_reached(read, difference);
    
}

void go_to_quad_plus() 
{
    float read        = 0.0f;
    float difference  = 0.0f;
    float prev_read   = 0.0f;
    int voltage_step  = 0;
    int step_size     = MAX_16BIT_STEPS / MAX_12BIT_STEPS; //Use 12-bit step size for faster convergence
    int buffer        = 0;
    
    log_selected_setpoint();

    set_pwm_dac(voltage_step); 
    
    sleep_ms(100); //Allow DAC to settle (REASON: PWM signal from MAX voltage to MIN voltage takes some time to settle through the external RC circuit)

    read = read_voltage();
 
    voltage_step++;

    difference = fabs(read - selected_setpoint);

    bool is_plus = false;

    while ((difference > tolerance) && (is_plus == false)) 
    { 
        
        //Ensure voltage values stay within limits
        if (voltage_step < MIN_VOLTAGE_STEP) 
        {
            log_index_error(voltage_step);
            break;
        }

        if (voltage_step > MAX_VOLTAGE_STEP) 
        {  
            log_index_error(voltage_step);
            break;
        }

        if((prev_read < read) && (difference > tolerance))
        {
            buffer++;

            if (buffer > quad_buffer)
            {
                is_plus = true;
            }
        }
        
        //Keep adjusting output until setpoint is reached
        set_pwm_dac(voltage_step);
        read = read_voltage();
        difference = fabs(read - selected_setpoint);
        voltage_step += step_size;
        prev_read = read;
    }

    log_setpoint_reached(read, difference);
    
}

void go_to_quad_minus() 
{
    float read        = 0.0f;
    float difference  = 0.0f;
    float prev_read   = 0.0f;
    int voltage_step  = 0;
    int step_size     = MAX_16BIT_STEPS / MAX_12BIT_STEPS; //Use 12-bit step size for faster convergence
    int buffer        = 0;
    
    log_selected_setpoint();

    set_pwm_dac(voltage_step); 
    
    sleep_ms(100); //Allow DAC to settle (REASON: PWM signal from MAX voltage to MIN voltage takes some time to settle through the external RC circuit)

    read = read_voltage();
 
    voltage_step++;

    difference = fabs(read - selected_setpoint);

    bool is_minus = false;

    while ((difference > tolerance) && (is_minus == false)) 
    { 
        
        //Ensure voltage values stay within limits
        if (voltage_step < MIN_VOLTAGE_STEP) 
        {
            log_index_error(voltage_step);
            break;
        }

        if (voltage_step > MAX_VOLTAGE_STEP) 
        {  
            log_index_error(voltage_step);
            break;
        }

        if((prev_read > read) && (difference > tolerance))
        {
            buffer++;

            if (buffer > quad_buffer)
            {
                is_minus = true;
            }
        }
        
        //Keep adjusting output until setpoint is reached
        set_pwm_dac(voltage_step);
        read = read_voltage();
        difference = fabs(read - selected_setpoint);
        voltage_step += step_size;
        prev_read = read;
    }

    log_setpoint_reached(read, difference);
    
}

void process_quad_minus() {
    float read = 0.0f;
    float difference = 0.0f;
    int gain = 8;
    
    difference = fabs(current_input_voltage - selected_setpoint);

    // Iterate until the difference is within the tolerance
    while (difference > tolerance) 
    {
        // Move the index depending on whether the value is increasing or decreasing
        
        if (current_input_voltage > selected_setpoint) 
        {
            
            current_output_voltage_step += gain;
            set_pwm_dac(current_output_voltage_step);
            current_input_voltage = read_voltage();

        }
   
        else 
        {
            
            current_output_voltage_step -= gain;
            set_pwm_dac(current_output_voltage_step);
            current_input_voltage = read_voltage();

        }

        //Handle edge case 
        if (current_output_voltage_step <= MIN_VOLTAGE_STEP || current_output_voltage_step >= MAX_VOLTAGE_STEP) 
        {
            gpio_put(LED_PIN, 1);
            go_to_quad_minus();
            printf("EDGE CASE\n");
            
        }

        difference = fabs(current_input_voltage - selected_setpoint);
        //printf("Difference:      %.4f\n", difference);

    }

    //printf("\n");

}

void process_quad_plus() {
    float read = 0.0f;
    float difference = 0.0f;
    int gain = 8;
    
    difference = fabs(current_input_voltage - selected_setpoint);

    // Iterate until the difference is within the tolerance
    while (difference > tolerance) 
    {
        // Move the index depending on whether the value is increasing or decreasing
        
        if (current_input_voltage > selected_setpoint) 
        {
            
            current_output_voltage_step -= gain;
            set_pwm_dac(current_output_voltage_step);
            current_input_voltage = read_voltage();

        }
   
        else if (current_input_voltage < selected_setpoint)
        {
            
            current_output_voltage_step += gain;
            set_pwm_dac(current_output_voltage_step);
            current_input_voltage = read_voltage();

        }

        //Handle edge case 
        if (current_output_voltage_step <= MIN_VOLTAGE_STEP || current_output_voltage_step >= MAX_VOLTAGE_STEP) 
        {
            gpio_put(LED_PIN, 1);
            go_to_quad_plus();
            printf("EDGE CASE\n");
            
        }

        difference = fabs(current_input_voltage - selected_setpoint);
        //printf("Difference:      %.4f\n", difference);

    }

    //printf("\n");

}

float move(int voltage_step) 
{
    set_pwm_dac(voltage_step);
    return read_voltage();
}

bool process_slope_peak()
{

    bool move_right = MOVE_RIGHT;
    bool move_left = MOVE_LEFT;

    int initial_output_voltage_step = 0;
    int i = 1; //This value keeps tracks of read in the case that the initial read equals the next read

    float initial_read = 0.0f;
    float next_read = 0.0f;

    initial_output_voltage_step = current_output_voltage_step;
    
    initial_read = current_input_voltage;
    next_read = move(initial_output_voltage_step + GAIN);

    while (initial_read == next_read)
    {
        next_read = move(current_output_voltage_step + GAIN);
        i++;
        //printf("i: %d\n", i);
    }

    if (initial_read > next_read)
    {
        return move_left;
    }

    else if (initial_read < next_read)
    {
        move(initial_output_voltage_step - (i*GAIN));
        return move_right;
    }

}

void process_peak()
{
    float difference = 0.0f;
    float prev_difference = 0.0;
    float read = 0.0f;
    int buffer = 0;

    difference = fabs(selected_setpoint - current_input_voltage);
    
    bool direction = process_slope_peak();

    while (difference > tolerance)
    {
        if (direction == MOVE_RIGHT)
            {
                read = move(current_output_voltage_step + GAIN); 
            }

        else if (direction == MOVE_LEFT)
            {
                read = move(current_output_voltage_step - GAIN); 
            }

        if (buffer == 0)
        {
            prev_difference = difference;
        }

        difference = fabs(selected_setpoint - read);

        if (difference > prev_difference)
        {
            buffer++;
            //printf("J\n");
            
            if ((direction == MOVE_RIGHT) && (buffer == peak_buffer))
            {
                direction = MOVE_LEFT;
                buffer = 0;
            }

            else if ((direction == MOVE_LEFT) && (buffer == peak_buffer))
            {
                direction = MOVE_RIGHT;
                buffer = 0;
                
            }
        }
        //printf("difference = %.4f\n", difference);

        if (current_output_voltage_step <= MIN_VOLTAGE_STEP || current_output_voltage_step >= MAX_VOLTAGE_STEP) 
        {
            gpio_put(LED_PIN, 1);
            go_to_setpoint();
            printf("EDGE CASE\n");
            
        }

    }

}

bool process_slope_null()
{

    bool move_right = MOVE_RIGHT;
    bool move_left = MOVE_LEFT;

    int initial_output_voltage_step = 0;
    int i = 1;

    float initial_read = 0.0f;
    float next_read = 0.0f;

    initial_output_voltage_step = current_output_voltage_step;
    
    initial_read = current_input_voltage;
    next_read = move(initial_output_voltage_step + GAIN);

    while (initial_read == next_read)
    {
        next_read = move(current_output_voltage_step + GAIN);
        i++;
        //printf("i: %d\n", i);
    }

    if (initial_read > next_read)
    {
        return move_right;
    }

    else if (initial_read < next_read)
    {
        move(initial_output_voltage_step - (i*GAIN));
        return move_left;
    }

}

void process_null() 
{
    float difference = 0.0f;
    float prev_difference = 0.0;
    float read = 0.0f;
    int buffer = 0;

    difference = fabs(selected_setpoint - current_input_voltage);
    
    bool direction = process_slope_null();

    while (difference > tolerance)
    {
        if (direction == MOVE_RIGHT)
            {
                read = move(current_output_voltage_step + GAIN); 
            }

        else if (direction == MOVE_LEFT)
            {
                read = move(current_output_voltage_step - GAIN); 
            }

        if (buffer == 0)
        {
            prev_difference = difference;
        }

        difference = fabs(selected_setpoint - read);

        if (difference > prev_difference)
        {
            buffer++;
            //printf("J\n");
            
            if ((direction == MOVE_RIGHT) && (buffer == null_buffer))
            {
                direction = MOVE_LEFT;
                buffer = 0;
            }

            else if ((direction == MOVE_LEFT) && (buffer == null_buffer))
            {
                direction = MOVE_RIGHT;
                buffer = 0;
                
            }
        }
        //printf("difference = %.4f\n", difference);


        //EDGE CASE HANDELING
        if (current_output_voltage_step <= MIN_VOLTAGE_STEP || current_output_voltage_step >= MAX_VOLTAGE_STEP) 
        {
            gpio_put(LED_PIN, 1);
            go_to_setpoint();
            printf("EDGE CASE\n");
            
        }

    }
}

void go_to_desired_setpoint()
{
    if (set_point == PEAK_POINT) 
    {
        selected_setpoint = peak_setpoint;
        go_to_setpoint();
    }

    else if (set_point == QUAD_PLUS)
    {
        selected_setpoint = quad_setpoint;
        go_to_quad_plus();
    }

    else if (set_point == QUAD_MINUS)
    {
        selected_setpoint = quad_setpoint;
        go_to_quad_minus();
    }

    else if (set_point == NULL_POINT) 
    {
        selected_setpoint = null_setpoint;
        go_to_setpoint();
    }
}

void button_isr(uint gpio, uint32_t events)
{
    if(gpio == BUTTON_PIN)
    {
        button_pressed = true;
    }
}

/*
void pin_isr(uint gpio, uint32_t events)
{
    if(gpio == NULL_PIN) 
    {
        null_pin = true;
    }
    else if (gpio == QUAD_MINUS_PIN)
    {
        quad_minus_pin = true;
    }
    else if (gpio == QUAD_PLUS_PIN)
    {
        quad_plus_pin = true;
    }
    else if (gpio == PEAK_PIN)
    {
        peak_pin = true;
    }
}
*/

void test_pins()
{
    if(gpio_get(NULL_PIN) == true)
    {
        null_pin = true;
        set_point = NULL_POINT;
    }
    else if (gpio_get(QUAD_MINUS_PIN) == true)
    {
        quad_minus_pin = true;
        set_point = QUAD_MINUS;
    }
    else if (gpio_get(QUAD_PLUS_PIN) == true)
    {
        quad_plus_pin = true;
        set_point = QUAD_PLUS;
    }
    else if (gpio_get(PEAK_PIN) == true)
    {
        peak_pin = true;
        set_point = PEAK_POINT;
    }
}

/*
void select_setpoint()
{
    if (null_pin == true)
    {
        //null_pin = false;
        set_point = NULL_POINT;
        selected_setpoint = null_setpoint;
    }

    else if (quad_minus_pin == true)
    {
        //quad_minus_pin = false;
        set_point = QUAD_MINUS;
        selected_setpoint = quad_setpoint;
    }

    else if (quad_plus_pin == true)
    {
        //quad_plus_pin = false;
        set_point = QUAD_PLUS; 
        selected_setpoint = quad_setpoint;   
    }

    else if (peak_pin == true)
    {
        //peak_pin = false;
        set_point = PEAK_POINT;
        selected_setpoint = peak_setpoint;
    }
}
*/

int main()
{
    stdio_init_all();
    
    //Initialize hardware
    initialize_pwm();
    initialize_adc();
    initialize_button();
    initialize_led();
    initialize_shorts();

    gpio_put(LED_PIN, 0);

    // Print welcome message and help info
    printf("\n=== Interferometer Control System ===\n");
    printf("Type 'help' for available commands\n\n");

    while (!peak_pin && !null_pin && !quad_minus_pin && !quad_plus_pin)
    {
        test_pins();
        check_serial_input();
    }

    
    //sleep_ms(10000); //This is only used if logging data to open putty up in time
    sweep();
    go_to_desired_setpoint();

    gpio_set_irq_enabled_with_callback(BUTTON_PIN, GPIO_IRQ_EDGE_FALL, true, &button_isr);
    
    /*
    gpio_set_irq_enabled_with_callback(NULL_PIN, GPIO_IRQ_EDGE_RISE, true, &pin_isr);
    gpio_set_irq_enabled_with_callback(QUAD_MINUS_PIN, GPIO_IRQ_EDGE_RISE, true, &pin_isr);
    gpio_set_irq_enabled_with_callback(QUAD_PLUS_PIN, GPIO_IRQ_EDGE_RISE, true, &pin_isr);
    gpio_set_irq_enabled_with_callback(PEAK_PIN, GPIO_IRQ_EDGE_RISE, true, &pin_isr);
    */

    while(1) 
    
    {
        check_serial_input();
        current_input_voltage = read_voltage();

        if (current_input_voltage < NOISE_FLOOR)
        { 
            gpio_put(LED_PIN, 1);
        }

        else 
        {
            gpio_put(LED_PIN, 0);
        }

        if(button_pressed)
            {
                button_pressed = false;
                sleep_ms(200);
                sweep();
                go_to_desired_setpoint();
            }
        
        /*
        if(peak_pin || null_pin || quad_plus_pin || quad_minus_pin)
        {
            select_setpoint();
            sweep();
            go_to_desired_setpoint();
        }
        */
        
        if (fabs(current_input_voltage - selected_setpoint) > tolerance) 
        
        {
            
            //DEBUG
            //printf("Process_setpoint     \n");
            //printf("Current input voltage %.4f\n", current_input_voltage);
           
            if (set_point == PEAK_POINT)
            {
                process_peak();
            }

            else if (set_point == QUAD_PLUS)
            {
                process_quad_plus();
            }

            else if (set_point == QUAD_MINUS)
            {
                process_quad_minus();
            }

            else if (set_point == NULL_POINT)
            {
                process_null();
            }

            check_serial_input();
            sleep_ms(1000); //Reason: readable output and control stability 

        }

        else 
       
        {

            //DEBUG
            //printf("Setpoint in limit\n");
            //printf("Input Voltage:   %.4f\n", current_input_voltage);
            sleep_ms(1000);

        }

        // If parameters were changed via serial commands, you might want to update settings
        if (params_changed) {
            printf("Parameters updated. Continuing with new settings...\n");
            params_changed = false;
        }
    
    }  
 
}


//UNUSED CODE


/*
void handle_edge_case() 
{
    printf("Current output voltage step %d:\n", current_output_voltage_step);
    
    //If output voltage goes beyond limits reset it's starting point to the opposing end
    if (current_output_voltage_step <= MIN_VOLTAGE) 
    {
        current_output_voltage_step = MAX_VOLTAGE;
        
        set_pwm_dac(current_output_voltage_step);
        sleep_ms(1000); 
        current_input_voltage = read_voltage();

            //Keep adjusting output voltage until setpoint is reached again    
            while (fabs(current_input_voltage - selected_setpoint) > tolerance) 
            {
                
                current_output_voltage_step--;
                set_pwm_dac(current_output_voltage_step);
                current_input_voltage = read_voltage();

                if (current_output_voltage_step == MIN_VOLTAGE) 
                {
                    printf("Setpoint not reachable\n");
                    
                    //DEBUG
                    for (;;){}
                    //break;
                }
        }
    }

    else if (current_output_voltage_step >= MAX_VOLTAGE) 
    {
        current_output_voltage_step = MIN_VOLTAGE;
        
        set_pwm_dac(current_output_voltage_step);
        sleep_ms(1000); //allow DAC to settle 
        current_input_voltage = read_voltage();
   
            while (fabs(current_input_voltage - selected_setpoint) > tolerance) 
            {
                
                current_output_voltage_step++;
                set_pwm_dac(current_output_voltage_step);
                current_input_voltage = read_voltage();

                if (current_output_voltage_step == MAX_VOLTAGE) 
                {
                    printf("Setpoint not reachable\n");
                    
                    //DEBUG
                    for(;;){}
                    //break;
                }
        }
    }
}

*/