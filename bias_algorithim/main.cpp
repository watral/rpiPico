#include <iostream>
#include <vector>
#include <cmath>  // For sin function
#include <random> // For random number generation

#define MAX_INDICES 5 // Maximum size for peak and null & quad indices
#define MAX_ARRAY_SIZE 65536 //Maximum array size


/*TODO: 

TRY TO REMOVE PEAK/NULL/QUAD INDICI AS VECTOR AND RATHER SINGLE VALUE (CAN START CURRENT INDEX AS 0 AND DRIVE TO SETPOINT), STANDARIZE FUNCTION NAME (CAMELCASE OR _), 
PUT FUNCTIONS IN ALPHABETICAL ORDER,,INDICATE WHAT WILL TRANSFER TO RPIPICO, WHAT WILL NOT, AND WHAT WILL NEED TO BE CHANGED
FIND OPTIMAL QUAD TOLERANCE, ASK MICAH ABOUT QUAD PLUS/MINUS, ASK BROOKE/PAUL FOR TEST SIGNAL CHARACTERISTICS, CREATE INIT FUNCTION FOR CLASS,

*/

enum setPoint {
    NULL_POINT,
    QUAD_POINT, 
    PEAK_POINT   
};

double null_setpoint = 0;
double peak_setpoint = 0;
double quad_setpoint = 0;


//TEST CODE
// Peak and Null Detection functions (outside of SineWaveData class)
std::vector<size_t> peak_indices;
std::vector<size_t> null_indices;
std::vector<size_t> quad_minus_indices;
std::vector<size_t> quad_plus_indices;

//FUNCTIONAL CODE
double threshold = 0.0002;  // Threshold for quad points detection
const double tolerance = 0.0000001;
double y_avg = 0;  // Average value for quad detection

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
        size_t num_steps = MAX_ARRAY_SIZE;

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
        int max_steps = MAX_ARRAY_SIZE;  // 16-bit PWM resolution (65536 steps)
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
                
                //TODO: SEPERATE OUT THIS TEST CODE TO SEERATE FUNCTION
                
                if (peak_indices.size() < MAX_INDICES) {
                    peak_indices.push_back(i - 1);  // Store the index of the peak
                    std::cout << "Peak detected at index: " << i - 1 << " (x, y) = ("
                              << i - 1 << ", " << resultArray[i - 1] << ")" << std::endl;
                }

                /*  FUNCTIONAL CODE

                    peak_setpoint = previous_y;    
                    break;

                    */

                peak_setpoint = previous_y;
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
                
                //TODO: SEPERATE OUT THIS TEST CODE TO SEERATE FUNCTION
                if (null_indices.size() < MAX_INDICES) {
                    null_indices.push_back(i - 1);  // Store the index of the null //TEST CODE
                    
                    
                    std::cout << "Null detected at index: " << i - 1 << " (x, y) = ("
                              << i - 1 << ", " << resultArray[i - 1] << ")" << std::endl;
                }

                /*  FUNCTIONAL CODE

                    null_setpoint = previous_y;
                    break;    

                    */
                
                null_setpoint = previous_y;
            }
            direction = 1;  // Set direction to increasing
        }

        previous_y = current_y;  // Update previous_y for next iteration
    }
}

//Midpoint of a sine wave
double find_quad(double peak_setpoint) {
    return peak_setpoint / 2;
}

//TODO: ASK MICAH WHETHER TO DEFAULT TO QUAD_PLUS OR QUAD_MINUS IN THE FUNCTIONAL CODE

// Method to detect quad minus using the resultArray
void handleQuadMinus(const std::vector<double>& resultArray, double average_y) {
    for (size_t i = 1; i < resultArray.size(); ++i) {
        double previous_y = resultArray[i - 1];
        double current_y = resultArray[i];

        // Check if previous value is greater than current (decreasing slope)
        if (previous_y > current_y) {
            // Check if current value is close to the average (within threshold)
            if (std::abs(current_y - average_y) <= threshold) {
                
                //TEST CODE
                //TODO: SEPERATE OUT THIS TEST CODE TO SEERATE FUNCTION

                quad_minus_indices.push_back(i);
                std::cout << "Quad Minus detected at index: " << i << " (y) = "
                          << resultArray[i] << std::endl;

                /*  FUNCTIONAL CODE
                    
                    quad_setpoint = current_y;
                    break;

                */
            }
        }
    }
}

// Method to detect quad plus using the resultArray
void handleQuadPlus(const std::vector<double>& resultArray, double average_y) {
    for (size_t i = 1; i < resultArray.size(); ++i) {
        double previous_y = resultArray[i - 1];
        double current_y = resultArray[i];

        // Check if previous value is less than current (increasing slope)
        if (previous_y < current_y) {
            // Check if current value is close to the average (within threshold)
            if (std::abs(current_y - average_y) <= threshold) {
                
                //TEST CODE
                //TODO: SEPERATE OUT THIS TEST CODE TO SEERATE FUNCTION
                
                quad_plus_indices.push_back(i);

                //DEBUG
                std::cout << "Quad Plus detected at index: " << i << " (y) = "
                          << resultArray[i] << std::endl;

                /*  FUNCTIONAL CODE
                    
                    quad_setpoint = i;
                    break;

                */



            }
        }
    }
}

//TODO: ADJUST THRESHOLD, DURING TESTING A FEW DUPLICATES WERE FOUND
// Function to detect both Quad Minus and Quad Plus
void detectQuads(const std::vector<double>& resultArray) {
    
    //DEBUG
    if (peak_indices.empty()) {
        std::cout << "No peaks detected, unable to calculate Quad Setpoint." << std::endl;
        return; // Early exit if no peaks are detected
    }

    // Use the find_quad function to calculate the Quad Setpoint
    double quad_setpoint = find_quad(peak_setpoint);

    //DEBUG
    std::cout << "Peak Setpoint: " << peak_setpoint << " | Quad Setpoint: " << quad_setpoint << " | Null Setpoint: " << null_setpoint << std::endl;

    // Call the functions to detect Quad Minus and Quad Plus
    handleQuadMinus(resultArray, quad_setpoint);  // Detect Quad Minus points
    handleQuadPlus(resultArray, quad_setpoint);   // Detect Quad Plus points
}


void scanPWM() {
    std::vector<double> resultArray(MAX_ARRAY_SIZE);  // 16-bit resolution, range 0-MAX_ARRAY_SIZE

    // Scan through all possible PWM values in the 16-bit range
    for (int pwm_value = 0; pwm_value <= MAX_ARRAY_SIZE; pwm_value++) {
        // Set the PWM DAC value (this controls the hardware or simulation)
        SineWaveData::set_pwm_dac(pwm_value);

        // Read the analog value from GPIO (this will be the system's response)
        double analog_value = SineWaveData::adc_read();

        // Store the result in the array
        resultArray[pwm_value] = analog_value;
    }

    //DEBUG
    std::cout << "First 10 values from PWM scan:" << std::endl;
    for (size_t i = 0; i < 10; i++) {
        std::cout << "PWM Value " << i << ": " << resultArray[i] << std::endl;
    }

    // Detect peaks and nulls from the result array
    detectPeaks(resultArray);  // Detect peaks first
    detectNulls(resultArray);  // Detect nulls next
    detectQuads(resultArray); // Detect quads next

    //Output the result to verify 

    std::cout << "PWM scan complete. Array populated with " << resultArray.size() << " values." << std::endl;
}

//TEST CODE
size_t returnShiftedValueQuad() {
    size_t current_index = quad_plus_indices[0]; //Default to first value (This was chosen after a discusion with micah)
    SineWaveData::set_pwm_dac(current_index);
    return current_index;
}

size_t returnShiftedValuePeak() {
    size_t current_index = peak_indices[0];
    SineWaveData::set_pwm_dac(current_index);
    return current_index;
}

size_t returnShiftedValueNull() {
    size_t current_index = null_indices[0];
    SineWaveData::set_pwm_dac(current_index);
    return current_index;
}


//TODO: Test quad_minus, and different points of indicies. 
//FUNCTIONAL CODE
void processQuadSetpoint(size_t current_index) {
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

        // Ensure the index is within valid bounds
        if (current_index >= MAX_ARRAY_SIZE || current_index < 0) {
            std::cerr << "Quad Setpoint not reachable." << std::endl;
            return;
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

void processPeakSetpoint(size_t current_index) {
    size_t steps = 0;
    const double tolerance = 0.001;

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
        
        // Ensure the index is within valid bounds
        if (current_index >= SineWaveData::getSize() || current_index < 0) {
            std::cerr << "Peak Setpoint not reachable: Index out of bounds." << std::endl;
            return;
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

//TODO: use previous logic to adjust current index update, slope tracking (always go in direction of negative slope)
void processNullSetpoint(size_t current_index) {
    size_t steps = 0;
    const double tolerance = 0.01;

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
        
        //DEBUG
        // Ensure the index is within valid bounds
        if (current_index >= SineWaveData::getSize() || current_index < 0) {
            std::cerr << "Null Setpoint not reachable: Index out of bounds." << std::endl;
            return;
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
    double x_end = 20.0;
    double phase_shift = 0;
    SineWaveData::generateData(x_start, x_end, phase_shift);

    // Perform PWM scan and get the result
    scanPWM();

    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<> dis(-1, 1);
        
    phase_shift = dis(gen);

    //DEBUG         
    std::cout << "Phase shift: " << phase_shift << std::endl;
    SineWaveData::generateData(x_start, x_end, phase_shift);

    setPoint set_point = QUAD_POINT;
    quad_setpoint = 1;

    if (set_point == NULL_POINT) {
       //TEST CODE
       size_t current_index = returnShiftedValueNull();
        
        //FUNCTIONAL CODE
       processNullSetpoint(null_setpoint);

       /*REALTIME IMPLEMENTATION
        while(1) {
            processNullSetpoint(quad_setpoint);
        }
        */

    }

    else if (set_point == QUAD_POINT) {
        //TEST CODE
        size_t current_index = returnShiftedValueQuad();
        
        //FUNCTIONAL CODE
        processQuadSetpoint(current_index); 

        /*REALTIME IMPLEMENTATION
        while(1) {
            processQuadSetpoint(quad_setpoint);
        }
        */

    }

    else if (set_point == PEAK_POINT) {
       //TEST CODE
       size_t current_index = returnShiftedValuePeak();
       
       processPeakSetpoint(peak_setpoint);

       /*REALTIME IMPLEMENTATION
        while(1) {
            processPeakSetpoint(quad_setpoint);
        }
        */
    }

    return 0;
}
