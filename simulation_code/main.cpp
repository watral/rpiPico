#include <iostream>
#include <vector>
#include <cmath>  // For sin function
#include <random> // For random number generation

#define MAX_VOLTAGE 65535 //Maximum array size
#define MIN_VOLTAGE 0
#define SWEEP_RANGE 6.5

/*TODO: 

GO THROUGH NAMING AND PUT IN ALPHABETICAL ORDER

*/

enum setPoint {
    NULL_POINT,
    QUAD_POINT, 
    PEAK_POINT   
};

//Setpoint (Y value)
double null_setpoint = 0;
double peak_setpoint = 0;
double quad_setpoint = 0;

//Starting point (X value)
int null_index = 0;
int peak_index = 0;
int quad_plus_index = 0;
int quad_minus_index = 0;
int quad_index = 0;
int current_index = 0;

//FUNCTIONAL CODE
double threshold = 0.0002;  // Threshold for quad points detection
const double tolerance = 0.0000001;

//TEST CODE
class SineWaveData {
public:
    static std::vector<double> x_values;
    static std::vector<double> y_values;
    static size_t last_accessed_index;  // Track the last accessed index for both X and Y values

    static void generateData(double x_start, double x_end, double phase_shift) {
        x_values.clear();
        y_values.clear();

        // Number of steps for 16-bit resolution (65536 steps)
        size_t num_steps = MAX_VOLTAGE;

        // Calculate step size based on x range and number of steps
        double step_size = (x_end - x_start) / (num_steps - 1);

        for (size_t i = 0; i < num_steps; i++) {
            double x = x_start + i * step_size;
            x_values.push_back(x);
            y_values.push_back(1 + std::sin(x + 1 + phase_shift));  // Generate sine wave with phase shift
        }

        last_accessed_index = 0;  // Initialize last accessed index
    }

    // Set the PWM DAC based on voltage with 16-bit resolution
    static void set_pwm_dac(size_t index) {
        double voltage = x_values[index];  // Get X value (voltage) from the vector
        double voltage_limit = 3.3;  // Assuming a 3.3V limit for PWM
        int max_steps = MAX_VOLTAGE;  // 16-bit PWM resolution (65536 steps)
        int step_value = static_cast<int>((voltage / voltage_limit) * max_steps);

        //This only simulates an analog write
        
        last_accessed_index = index;  // Track the last accessed index for analog_read
    }

    // Replace readYValue with analog_read(GPIO_PIN), simply pass through the function
    static double adc_read() {
        // Simulate reading from an analog pin
        // Replace with the actual GPIO reading code
        double voltage = y_values[last_accessed_index];  // Return the Y value corresponding to the last accessed index
        //std::cout << "Reading GPIO for Y value: " << voltage << std::endl;
        return voltage;
    }

    static size_t getSize() {
        return x_values.size();
    }
};

// Initialize static members
size_t SineWaveData::last_accessed_index = 0;  // Initializing the static member

//TEST CODE
// Static member variable initialization
std::vector<double> SineWaveData::x_values;
std::vector<double> SineWaveData::y_values;

//FUNCTIONAL CODE
void detectPeaks(const std::vector<double>& resultArray) {
    // Initializing the previous_y to the first value in the result array
    double previous_y = resultArray[0];
    int direction = 0; // 0 = unknown, 1 = increasing, -1 = decreasing

    // Iterate through the result array starting from the second element
    for (size_t i = 1; i < resultArray.size(); ++i) {
        double current_y = resultArray[i];

        // Detect peak (increasing to decreasing)
        if (current_y < previous_y) {
            if (direction == 1) {
                // It's a peak, record it
                
                peak_setpoint = previous_y;
                peak_index = i - 1;
                //DEBUG
                std::cout << "Peak detected at index: " << peak_index << " (x, y) = ("
                              << peak_index << ", " << resultArray[peak_index] << ")" << std::endl;
                break;
            }
            direction = -1;  // Set direction to decreasing
        }

        else if (current_y > previous_y) {
            if (direction == -1) {
                //do nothing
            }
            direction = 1;  // Set direction to increasing
        }

        previous_y = current_y;  // Update previous_y for next iteration
    }
}

void detectNulls(const std::vector<double>& resultArray) {
    // Initializing the previous_y to the first value in the result array
    double previous_y = resultArray[0];
    int direction = 0; // 0 = unknown, 1 = increasing, -1 = decreasing

    // Iterate through the result array starting from the second element
    for (size_t i = 1; i < resultArray.size(); ++i) {
        double current_y = resultArray[i];
      
        // Detect null (decreasing to increasing)
        if (current_y < previous_y) {
            if (direction == 1) {

            }
            direction = -1;  // Set direction to decreasing
        }

        // Detect null (decreasing to increasing)
        else if (current_y > previous_y) {
            if (direction == -1) {
                // It's a null, record it

                null_setpoint = previous_y;
                null_index = i - 1;
                std::cout << "Null detected at index: " << null_index << " (x, y) = ("
                              << null_index << ", " << resultArray[i - 1] << ")" << std::endl;
                break;
            }

            direction = 1;  // Set direction to increasing
        }

        previous_y = current_y;  // Update previous_y for next iteration
    }
}

//Midpoint of a sine wave
void find_quad() {
    quad_setpoint = peak_setpoint / 2;
}

//TODO: CHARLES SAID EITHER QUAD PLUS OR QUAD MINUS CAN BE USED: best to use the one at a greater voltage level

// Method to detect quad minus using the resultArray
void handleQuadMinus(const std::vector<double>& resultArray) {
    for (size_t i = 1; i < resultArray.size(); ++i) {
        double previous_y = resultArray[i - 1];
        double current_y = resultArray[i];

        // Check if previous value is greater than current (decreasing slope)
        if (previous_y > current_y) {
            // Check if current value is close to the average (within threshold)
            if (std::abs(current_y - quad_setpoint) <= threshold) {
                
                //quad_setpoint = current_y;
                quad_minus_index = i;
                //DEBUG
                std::cout << "Quad Minus detected at index: " << quad_minus_index << " (y) = "
                          << resultArray[quad_minus_index] << std::endl;
                break;

            }
        }
    }
}


//TODO: EDGE CASE: QUAD DETECTED BUT INDEX IS CLOSE TO 0
// Method to detect quad plus using the resultArray
void handleQuadPlus(const std::vector<double>& resultArray) {
    for (size_t i = 1; i < resultArray.size(); ++i) {
        double previous_y = resultArray[i - 1];
        double current_y = resultArray[i];

        // Check if previous value is less than current (increasing slope)
        if (previous_y < current_y) {
            // Check if current value is close to the average (within threshold)
            if (std::abs(current_y - quad_setpoint) <= threshold) {
                
                quad_plus_index = i;
                
                //DEBUG
                std::cout << "Quad Plus detected at index: " << quad_plus_index << " (y) = "
                          << resultArray[quad_plus_index] << std::endl;
                break;

            }
        }
    }
}

//TODO: ADJUST THRESHOLD, DURING TESTING A FEW DUPLICATES WERE FOUND
// Function to detect both Quad Minus and Quad Plus
void detectQuads(const std::vector<double>& resultArray) {
    
    //DEBUG
    if (peak_setpoint == 0) {
        std::cout << "No peaks detected, unable to calculate Quad Setpoint." << std::endl;
        return; // Early exit if no peaks are detected
    }

    // Use the find_quad function to calculate the Quad Setpoint
    find_quad();

    // Call the functions to detect Quad Minus and Quad Plus
    handleQuadMinus(resultArray);  // Detect Quad Minus points
    handleQuadPlus(resultArray);   // Detect Quad Plus points

    if (quad_plus_index > quad_minus_index) {
        quad_index = quad_plus_index;
    }

    else {
        quad_index = quad_minus_index;
    }
}

//May not need to detect quad if we already know what the setpoint is


void scanPWM() {
    std::vector<double> resultArray(MAX_VOLTAGE);  // 16-bit resolution, range 0-MAX_VOLTAGE

    // Scan through all possible PWM values in the 16-bit range
    for (int pwm_value = 0; pwm_value <= MAX_VOLTAGE; pwm_value++) {
        // Set the PWM DAC value (this controls the hardware or simulation)
        SineWaveData::set_pwm_dac(pwm_value);

        // Read the analog value from GPIO (this will be the system's response)
        double analog_value = SineWaveData::adc_read();

        // Store the result in the array
        resultArray[pwm_value] = analog_value;
    }

/*
    //DEBUG
    std::cout << "First 10 values from PWM scan:" << std::endl;
    for (size_t i = 0; i < 10; i++) {
        std::cout << "PWM Value " << i << ": " << resultArray[i] << std::endl;
    }
*/

    // Detect peaks and nulls from the result array
    detectPeaks(resultArray);  // Detect peaks first
    detectNulls(resultArray);  // Detect nulls next
    detectQuads(resultArray); // Detect quads next

    //Output the result to verify 

    std::cout << "PWM scan complete. Array populated with " << resultArray.size() << " values." << std::endl;
}

//FUNCTIONAL CODE

void handleEdgeCase(double setpoint, double tolerance) {
    double current_y;
    
    if (current_index <= MIN_VOLTAGE) {
        current_index = MAX_VOLTAGE;
            while (std::abs(current_y - setpoint) > tolerance) {
                current_index--;
                SineWaveData::set_pwm_dac(current_index);
                current_y = SineWaveData::adc_read();

                if (current_index == MIN_VOLTAGE) {
                    std::cerr << "Setpoint not reachable" << std::endl;
                    for (;;){}
                    //break;
                }
        }
    }

    else if (current_index >= MAX_VOLTAGE) {
        current_index = MIN_VOLTAGE;
            while (std::abs(current_y - setpoint) > tolerance) {
                current_index++;
                SineWaveData::set_pwm_dac(current_index);
                current_y = SineWaveData::adc_read();

                if (current_index == MAX_VOLTAGE) {
                    std::cerr << "Setpoint not reachable" << std::endl;
                    for(;;){}
                    //break;
                }
        }
    }
}

void processQuadSetpoint() {
    size_t step_count = 0;
    const double tolerance = 0.01;
    double current_y = SineWaveData::adc_read();

    // Iterate until the difference is within the tolerance
    while (std::abs(current_y - quad_setpoint) > tolerance) {
        // Move the index depending on whether the value is increasing or decreasing
        
        if (current_y > quad_setpoint) {
            current_index--;
        }

        
        else {
            current_index++;
        }

        //Handle edge case 
        if (current_index <= MIN_VOLTAGE || current_index >= MAX_VOLTAGE) {
            handleEdgeCase(quad_setpoint, tolerance);
            //current_y = SineWaveData::adc_read();
        }

        // Update DAC and read new value
        SineWaveData::set_pwm_dac(current_index);
        current_y = SineWaveData::adc_read();

        step_count++;
    }

    // Output the result
    // DEBUG
    std::cout << "Quad Setpoint Reached: ("
              << current_index << ", " << current_y
              << ") after " << step_count << " steps." << std::endl;
}

void processPeakSetpoint() {
    size_t steps = 0;
    const double tolerance = 0.0001;

    double current_y = SineWaveData::adc_read();
    double next_y = 0;
    bool found_rising_edge = false; 
    bool move_down = false;
    bool move_up = false;

    while (std::abs(current_y - peak_setpoint) > tolerance) {
        if (!found_rising_edge) {
            SineWaveData::set_pwm_dac(current_index + 1);
            next_y = SineWaveData::adc_read();

            if (next_y > current_y) {
                found_rising_edge = true;
                current_index++;
                move_up = true;
            }

            else {
                SineWaveData::set_pwm_dac(current_index - 1);
                next_y = SineWaveData::adc_read();
                if (next_y > current_y) {
                    found_rising_edge = true;
                    current_index--;
                    move_down = true;
                }
            }
        } else {
            if (move_up == true) {
                // Move upward toward the peak    
                current_index++;
            } else if (move_down == true) {
                // Move downward from the peak
                current_index--; 
            }

        }
        
        //Handle edge case 
        if (current_index <= MIN_VOLTAGE || current_index >= MAX_VOLTAGE) {
            handleEdgeCase(peak_setpoint, tolerance);
            //current_y = SineWaveData::adc_read();
        }

        SineWaveData::set_pwm_dac(current_index);

        current_y = SineWaveData::adc_read();
        steps++;
    }

    // Output the result
    // DEBUG
    std::cout << "Peak Setpoint Reached: ("
              << current_index << ", " << current_y
              << ") after " << steps << " steps." << std::endl;
}

void processNullSetpoint() {
    size_t steps = 0;
    const double tolerance = 0.0001;

    double current_y = SineWaveData::adc_read();
    double next_y = 0;
    bool found_falling_edge = false; 
    bool move_down = false;
    bool move_up = false;

    while (std::abs(current_y - null_setpoint) > tolerance) {
        if (!found_falling_edge) {
            SineWaveData::set_pwm_dac(current_index + 1);
            next_y = SineWaveData::adc_read();

            if (next_y < current_y) {
                found_falling_edge = true;
                current_index++;
                move_up = true;
            }

            else {
                SineWaveData::set_pwm_dac(current_index - 1);
                next_y = SineWaveData::adc_read();
                if (next_y < current_y) {
                    found_falling_edge = true;
                    current_index--;
                    move_down = true;
                }
            }
        } else {
            if (move_up == true) {
                // Move upward toward the peak    
                current_index++;
            } else if (move_down == true) {
                // Move downward from the peak
                current_index--; 
            }

        }
        
        //Handle edge case 
        if (current_index <= MIN_VOLTAGE || current_index >= MAX_VOLTAGE) {
            handleEdgeCase(null_setpoint, tolerance);
            //current_y = SineWaveData::adc_read();
        }

        SineWaveData::set_pwm_dac(current_index);

        current_y = SineWaveData::adc_read();
        steps++;
    }

    // Output the result
    // DEBUG
    std::cout << "Null Setpoint Reached: ("
              << current_index << ", " << current_y
              << ") after " << steps << " steps." << std::endl;
}


int main() {
    // Generate sine wave data
    double x_start = 0.0;
    double x_end = SWEEP_RANGE;
    double phase_shift = 0;
    SineWaveData::generateData(x_start, x_end, phase_shift);

    // Perform PWM scan and get the result
    scanPWM();

    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<> dis(-1, 1);
        
    phase_shift = dis(gen);
    //phase_shift = 1.3; //to manually adjust phase shift for debugging

    //DEBUG         
    std::cout << "Phase shift: " << phase_shift << std::endl;
    
    /*
    //TEST
    SineWaveData::generateData(x_start, x_end, phase_shift);
    */

    //FUNCTIONAL
    setPoint set_point = NULL_POINT;
    quad_setpoint = 1;

    if (set_point == NULL_POINT) {
       SineWaveData::set_pwm_dac(null_index);
       current_index = null_index;
       
       //TEST
        SineWaveData::generateData(x_start, x_end, phase_shift);
        
        //FUNCTIONAL CODE
       processNullSetpoint();

       /*REALTIME IMPLEMENTATION
       
       SineWaveData::set_pwm_dac(null_index);
       current_index = null_index;
       
       
        while(1) {
            processNullSetpoint();
        }
        */

    }

    else if (set_point == QUAD_POINT) {
        SineWaveData::set_pwm_dac(quad_plus_index);
        current_index = quad_index;
        //TEST
        SineWaveData::generateData(x_start, x_end, phase_shift);
        
        processQuadSetpoint(); 

        /*REALTIME IMPLEMENTATION

        SineWaveData::set_pwm_dac(quad_plus_index);
        current_index = quad_plus_index;

        while(1) {
            processQuadSetpoint();
        }
        */

    }

    else if (set_point == PEAK_POINT) {
       SineWaveData::set_pwm_dac(peak_index);
       current_index = peak_index;

       //TEST
       SineWaveData::generateData(x_start, x_end, phase_shift);
       
       processPeakSetpoint();

       /*REALTIME IMPLEMENTATION
    
        SineWaveData::set_pwm_dac(peak_index);
        current_index = peak_index;

        while(1) {
            processPeakSetpoint();
        }
        */
    }

    return 0;
}
