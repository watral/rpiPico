#include <iostream>
#include <vector>
#include <cmath>  // For sin function
#include <random> // For random number generation

#define MAX_INDICES 5 // Maximum size for peak and null & quad indices
#define MAX_ARRAY_SIZE 65536 //Maximum array size

//TODO: MAKE PEAK/NULL/QUAD INDICES FIXED ARRAYS WITH A MAX SIZE OF 5, PASS A PIN FOR READ/WRITE, STANDARIZE FUNCTION NAME (CAMELCASE OR _), PUT FUNCTIONS IN ALPHABETICAL ORDER, CREATE A SEPERATE FUNCTION TO APPLY GLOBAL SETPOINT, INDICATE WHAT WILL TRANSFER TO RPIPICO, WHAT WILL NOT, AND WHAT WILL NEED TO BE CHANGED, CONVERT STEPS TO VOLTAGE FOR DEBUG

enum setPoint {
    NULL_POINT,   // 0 by default
    QUAD_POINT, // 1
    PEAK_POINT   // 2
};

double null_setpoint = 0;
double peak_setpoint = 0;
double quad_setpoint = 0;

/*
// Static class that encapsulates the sine wave test data and provides static methods
class SineWaveData {
public:
    static std::vector<double> x_values;
    static std::vector<double> y_values;
    
    // Static methods to access x and y values
    static void generateData(double x_start, double x_end, double step_size, double phase_shift) {
        x_values.clear();
        y_values.clear();
        for (double x = x_start; x <= x_end; x += step_size) {
            x_values.push_back(x);
            y_values.push_back(1 + std::sin(x + 1 + phase_shift));  // Generate sine wave with phase shift
        }
    }

    static double readXValue(size_t index) { //TODO: rewrite this as set_pwm_dac(voltage) and convert voltage to approptiate step given voltage limit
        return x_values[index];
    }

    static double readYValue(size_t index) { //TODO: rewrite this analogRead(GPIO) simply pass the GPIO through. Also, perhaps the last index from readXvalue can be used rather than inserting an index. 
        return y_values[index];
    }

    static size_t getSize() {
        return x_values.size();
    }

    static std::vector<double> generateShiftedWave(double phase_shift) {
        std::vector<double> shifted_y_values;
        for (double x : x_values) {
            shifted_y_values.push_back(std::sin(x + phase_shift));
        }
        return shifted_y_values;
    }
};
*/


//removed shifted wave, and instead will regrenrate data with phase shift other than 0, functions now resemble rPi pico counterparts

class SineWaveData {
public:
    static std::vector<double> x_values;
    static std::vector<double> y_values;
    static size_t last_accessed_index;  // Track the last accessed index for both X and Y values

    static void generateData(double x_start, double x_end, double phase_shift) {
        x_values.clear();
        y_values.clear();

        // Number of steps for 16-bit resolution (65536 steps)
        size_t num_steps = 65536;

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
        int max_steps = 65535;  // 16-bit PWM resolution (65535 steps)
        int step_value = static_cast<int>((voltage / voltage_limit) * max_steps);

        // Here, replace with actual code to set PWM with 16-bit resolution
        // gpio_pwm_write(step_value);  // Replace with your platform's PWM function
        //std::cout << "Setting PWM DAC with step value: " << step_value << std::endl;

        // Update the last accessed index (this will be the current index of the X value)
        last_accessed_index = index;  // Track the last accessed index
    }

    // Replace readYValue with analog_read(GPIO_PIN), simply pass through the function
    static double analog_read_GPIO() {
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


// Static member variable initialization
std::vector<double> SineWaveData::x_values;
std::vector<double> SineWaveData::y_values;

// Peak and Null Detection functions (outside of SineWaveData class)
std::vector<size_t> peak_indices;
std::vector<size_t> null_indices;
std::vector<size_t> quad_minus_indices;
std::vector<size_t> quad_plus_indices;

double threshold = 0.0002;  // Threshold for quad points detection
const double tolerance = 0.0000001;
double y_avg = 0;  // Average value for quad detection

// Method to detect peaks and nulls in the sine wave

// TODO: Instead of reading and writing values, create a sweep function that sweep through the test data using the read and write functions
// and write to a presized array then pass the array seperatly to a detect peaks, detect nulls, and detect quad functions by indexing through the array

//TODO: Implement this scanning algorithim
// Function to scan the PWM range and populate the result array

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
                if (peak_indices.size() < MAX_INDICES) {
                    peak_indices.push_back(i - 1);  // Store the index of the peak
                    std::cout << "Peak detected at index: " << i - 1 << " (x, y) = ("
                              << i - 1 << ", " << resultArray[i - 1] << ")" << std::endl;
                }
            }
            direction = -1;  // Set direction to decreasing
        }
        // Detect null (decreasing to increasing)
        else if (current_y > previous_y) {
            if (direction == -1) {
                // It's a null, record it
                /*
                if (null_indices.size() < MAX_INDICES) {
                    null_indices.push_back(i - 1);  // Store the index of the null
                    std::cout << "Null detected at index: " << i - 1 << " (x, y) = ("
                              << i - 1 << ", " << resultArray[i - 1] << ")" << std::endl;
                }
                */
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

        // Detect peak (increasing to decreasing)
        if (current_y < previous_y) {
            if (direction == 1) {
                // It's a peak, record it
                /*
                if (peak_indices.size() < MAX_INDICES) {
                    peak_indices.push_back(i - 1);  // Store the index of the peak
                    std::cout << "Peak detected at index: " << i - 1 << " (x, y) = ("
                              << i - 1 << ", " << resultArray[i - 1] << ")" << std::endl;
                }
                */
            }
            direction = -1;  // Set direction to decreasing
        }
        // Detect null (decreasing to increasing)
        else if (current_y > previous_y) {
            if (direction == -1) {
                // It's a null, record it
                if (null_indices.size() < MAX_INDICES) {
                    null_indices.push_back(i - 1);  // Store the index of the null
                    std::cout << "Null detected at index: " << i - 1 << " (x, y) = ("
                              << i - 1 << ", " << resultArray[i - 1] << ")" << std::endl;
                }
            }
            direction = 1;  // Set direction to increasing
        }

        previous_y = current_y;  // Update previous_y for next iteration
    }
}

double find_quad(double peak_setpoint) {
    return peak_setpoint / 2;
}

// Method to detect quad minus using the resultArray
void handleQuadMinus(const std::vector<double>& resultArray, double average_y) {
    for (size_t i = 1; i < resultArray.size(); ++i) {
        double previous_y = resultArray[i - 1];
        double current_y = resultArray[i];

        // Check if previous value is greater than current (decreasing slope)
        if (previous_y > current_y) {
            // Check if current value is close to the average (within threshold)
            if (std::abs(current_y - average_y) <= threshold) {
                quad_minus_indices.push_back(i);
                std::cout << "Quad Minus detected at index: " << i << " (y) = "
                          << resultArray[i] << std::endl;
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
                quad_plus_indices.push_back(i);
                std::cout << "Quad Plus detected at index: " << i << " (y) = "
                          << resultArray[i] << std::endl;
            }
        }
    }
}

//TODO: ADJUST THRESHOLD, DURING TESTING A FEW DUPLICTES WERE FOUND
// Function to detect both Quad Minus and Quad Plus
void detectQuads(const std::vector<double>& resultArray) {
    if (peak_indices.empty()) {
        std::cout << "No peaks detected, unable to calculate Quad Setpoint." << std::endl;
        return; // Early exit if no peaks are detected
    }


    //TODO: ARE PEAK AND NULL NEEDED HERE?
    // Find the first peak index and use it to get the Peak Setpoint
    size_t first_null_index = null_indices[0];
    size_t first_peak_index = peak_indices[0];  // First peak index
    null_setpoint = resultArray[first_null_index];
    peak_setpoint = resultArray[first_peak_index];  // Get the peak Y-value
    

    // Use the find_quad function to calculate the Quad Setpoint
    double quad_setpoint = find_quad(peak_setpoint);

    std::cout << "Peak Setpoint: " << peak_setpoint << " | Quad Setpoint: " << quad_setpoint << " | Null Setpoint: " << null_setpoint << std::endl;

    // Call the functions to detect Quad Minus and Quad Plus
    handleQuadMinus(resultArray, quad_setpoint);  // Detect Quad Minus points
    handleQuadPlus(resultArray, quad_setpoint);   // Detect Quad Plus points
}


void scanPWM() {
    std::vector<double> resultArray(65536);  // 16-bit resolution, range 0-65535

    // Scan through all possible PWM values in the 16-bit range
    for (int pwm_value = 0; pwm_value <= 65535; pwm_value++) {
        // Set the PWM DAC value (this controls the hardware or simulation)
        SineWaveData::set_pwm_dac(pwm_value);

        // Read the analog value from GPIO (this will be the system's response)
        double analog_value = SineWaveData::analog_read_GPIO();

        // Store the result in the array
        resultArray[pwm_value] = analog_value;
    }

    // Output a portion of the result to the console
    std::cout << "First 10 values from PWM scan:" << std::endl;
    for (size_t i = 0; i < 10; i++) {
        std::cout << "PWM Value " << i << ": " << resultArray[i] << std::endl;
    }

    // Detect peaks and nulls from the result array
    detectPeaks(resultArray);  // Detect peaks first
    detectNulls(resultArray);  // Detect nulls next
    detectQuads(resultArray); // Detect quads next

    // Optionally, output the result to verify (only if necessary)
    std::cout << "PWM scan complete. Array populated with " << resultArray.size() << " values." << std::endl;
}

/*
void detectPeaksAndNulls(const std::vector<double>& resultArray) {
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
                if (peak_indices.size() < MAX_INDICES) {
                    peak_indices.push_back(i - 1);  // Store the index of the peak
                    std::cout << "Peak detected at index: " << i - 1 << " (x, y) = ("
                              << i - 1 << ", " << resultArray[i - 1] << ")" << std::endl;
                }
            }
            direction = -1;  // Set direction to decreasing
        }
        // Detect null (decreasing to increasing)
        else if (current_y > previous_y) {
            if (direction == -1) {
                // It's a null, record it
                if (null_indices.size() < MAX_INDICES) {
                    null_indices.push_back(i - 1);  // Store the index of the null
                    std::cout << "Null detected at index: " << i - 1 << " (x, y) = ("
                              << i - 1 << ", " << resultArray[i - 1] << ")" << std::endl;
                }
            }
            direction = 1;  // Set direction to increasing
        }

        previous_y = current_y;  // Update previous_y for next iteration
    }
}
*/

//TODO: test quad_minus, and different points of indicies. 
void processQuadSetpoint(double quad_setpoint) {
    size_t steps = 0;
    const double tolerance = 0.01;

    // Initial conditions
    size_t current_index = quad_plus_indices[0];  // Assuming quad_plus_indices is defined elsewhere
    SineWaveData::set_pwm_dac(current_index);
    //double previous_y = quad_setpoint;
    double current_y = SineWaveData::analog_read_GPIO();

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
        if (current_index >= SineWaveData::getSize() || current_index < 0) {
            std::cerr << "Quad Setpoint not reachable." << std::endl;
            return;
        }

        //previous_y = current_y;

        // Update PWM DAC with new index and read the new Y value
        SineWaveData::set_pwm_dac(current_index);
        current_y = SineWaveData::analog_read_GPIO();

        steps++;
    }

    // Output the result
    std::cout << "Quad Setpoint Reached: ("
              << current_index << ", " << current_y
              << ") after " << steps << " steps." << std::endl;
}


//TODO: use previous logic to adjust current index update, slope tracking (always go in direction of positive slope)
void processPeakSetpoint(double peak_setpoint) {
    size_t steps = 0;
    const double tolerance = 0.001;

    size_t current_index = peak_indices[1];  // Initial index (already set)
    SineWaveData::set_pwm_dac(current_index);  // Set PWM DAC with initial index
    double current_y = SineWaveData::analog_read_GPIO();
    double next_y = 0;
    bool found_rising_edge = false; 
    bool move_down = false;
    bool move_up = false;

    while (std::abs(current_y - peak_setpoint) > tolerance) {
        if (!found_rising_edge) {
            SineWaveData::set_pwm_dac(current_index + 1);
            next_y = SineWaveData::analog_read_GPIO();

            if (next_y > current_y) {
                found_rising_edge = true;
                current_index++;
                move_up = true;
            }

            else {
                SineWaveData::set_pwm_dac(current_index - 1);
                next_y = SineWaveData::analog_read_GPIO();
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

        current_y = SineWaveData::analog_read_GPIO();
        steps++;
    }

    // Output the result
    std::cout << "Peak Setpoint Reached: ("
              << current_index << ", " << current_y
              << ") after " << steps << " steps." << std::endl;
}

//TODO: use previous logic to adjust current index update, slope tracking (always go in direction of negative slope)
void processNullSetpoint(double null_setpoint) {
    size_t steps = 0;
    const double tolerance = 0.01;

    size_t current_index = null_indices[1];  // Initial index (already set)
    SineWaveData::set_pwm_dac(current_index);  // Set PWM DAC with initial index
    double current_y = SineWaveData::analog_read_GPIO();
    double next_y = 0;
    bool found_falling_edge = false; 
    bool move_down = false;
    bool move_up = false;

    while (std::abs(current_y - null_setpoint) > tolerance) {
        if (!found_falling_edge) {
            SineWaveData::set_pwm_dac(current_index + 1);
            next_y = SineWaveData::analog_read_GPIO();

            if (next_y < current_y) {
                found_falling_edge = true;
                current_index++;
                move_up = true;
            }

            else {
                SineWaveData::set_pwm_dac(current_index - 1);
                next_y = SineWaveData::analog_read_GPIO();
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
        
        // Ensure the index is within valid bounds
        if (current_index >= SineWaveData::getSize() || current_index < 0) {
            std::cerr << "Null Setpoint not reachable: Index out of bounds." << std::endl;
            return;
        }

        SineWaveData::set_pwm_dac(current_index);

        current_y = SineWaveData::analog_read_GPIO();
        steps++;
    }

    // Output the result
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
        
    std::cout << "Phase shift: " << phase_shift << std::endl;
    SineWaveData::generateData(x_start, x_end, phase_shift);

    setPoint set_point = NULL_POINT;
    quad_setpoint = 1;

    if (set_point == NULL_POINT) {
        /*
        
        SineWaveData::set_pwm_dac(null_indices[0]);
        double analog_value = SineWaveData::analog_read_GPIO();

        */

       processNullSetpoint(null_setpoint);

    }

    else if (set_point == QUAD_POINT) {
        processQuadSetpoint(quad_setpoint);

        /*REALTIME IMPLEMENTATION
        while(1) {
            processQuadSetpoint(quad_setpoint);
        }
        */

    }

    else if (set_point == PEAK_POINT) {
       processPeakSetpoint(peak_setpoint);
    }



    

    return 0;
}























/*
void detectPeaksAndNulls() {
    double previous_y = SineWaveData::readYValue(0);
    int direction = 0; // 0 = unknown, 1 = increasing, -1 = decreasing

    for (size_t i = 1; i < SineWaveData::getSize(); ++i) {
        double current_y = SineWaveData::readYValue(i);

        if (current_y > previous_y) {
            if (direction == -1) {
                if (null_indices.size() < MAX_INDICES) {
                    null_indices.push_back(i - 1);
                    std::cout << "Null detected at index: " << i - 1 << " (x, y) = (" 
                              << SineWaveData::readXValue(i - 1) << ", " << SineWaveData::readYValue(i - 1) << ")" << std::endl;
                }
            }
            direction = 1;
        }
        else if (current_y < previous_y) {
            if (direction == 1) {
                if (peak_indices.size() < MAX_INDICES) {
                    peak_indices.push_back(i - 1);
                    std::cout << "Peak detected at index: " << i - 1 << " (x, y) = ("
                              << SineWaveData::readXValue(i - 1) << ", " << SineWaveData::readYValue(i - 1) << ")" << std::endl;
                }
            }
            direction = -1;
        }

        previous_y = current_y;
    }
}

// Output detected peaks and nulls
void outputPeaksAndNulls(const std::string& label) {
    const std::vector<size_t>& indices = (label == "Peak") ? peak_indices : null_indices;

    std::cout << "All " << label << " Detected (x, y):" << std::endl;
    for (size_t index : indices) {
        std::cout << "(" << SineWaveData::readXValue(index) << ", " << SineWaveData::readYValue(index) << ")" << std::endl;
    }
}

// Function to process peak setpoints, TODO: replace with implementation from process peak, process null, and process quad setpoint from prior implementation
void processSetpoint(const std::vector<double>& shifted_results, double setpoint, size_t index, bool is_peak) {
    size_t current_index = index;
    size_t steps = 0;
    double previous_y = shifted_results[index];

    std::cout << (is_peak ? "Processing Peak Setpoint..." : "Processing Null Setpoint...") << std::endl;

    while (std::abs(shifted_results[current_index] - setpoint) > tolerance) {
        current_index = (shifted_results[current_index] < previous_y) ? current_index - 1 : current_index + 1;

        if (current_index >= shifted_results.size()) {
            std::cerr << "Setpoint not reachable." << std::endl;
            return;
        }

        previous_y = shifted_results[current_index];
        steps++;
    }

    std::cout << (is_peak ? "Peak" : "Null") << " Setpoint Reached: ("
              << SineWaveData::readXValue(current_index) << ", " << shifted_results[current_index]
              << ") after " << steps << " steps." << std::endl;
}

/*
void processQuadSetpoint(const std::vector<double>& shifted_results, const SineWave& sine_wave, double setpoint, size_t index) {
    size_t current_index = index;
    size_t steps = 0;
    const double tolerance = 0.001;
    double previous_y = shifted_results[index];

    // Iterate until the difference is within the tolerance
    while (std::abs(shifted_results[current_index] - setpoint) > tolerance) {
        // Move the index depending on whether the value is increasing or decreasing
        current_index = (shifted_results[current_index] < previous_y) ? current_index - 1 : current_index + 1;

        // Check if we're out of bounds (index too large or too small)
        if (current_index >= shifted_results.size() || current_index < 0) {
            std::cerr << "Quad Setpoint not reachable." << std::endl;
            return;
        }

        // Update the previous_y value to the current one
        previous_y = shifted_results[current_index];
        steps++;
    }

    // Output the result using "quad" instead of "peak"
    std::cout << "Quad Setpoint Reached: ("
              << sine_wave.readXValue(current_index) << ", " << shifted_results[current_index]
              << ") after " << steps << " steps." << std::endl;
}

// Process and align a peak setpoint with the shifted sine wave
void processPeakSetpoint(const std::vector<double>& shifted_results, const SineWave& sine_wave, double setpoint, size_t index) {
    size_t current_index = index;
    size_t steps = 0;
    const double tolerance = 0.0000001;
    double previous_y = shifted_results[index];

    while (std::abs(shifted_results[current_index] - setpoint) > tolerance) {
        current_index = (shifted_results[current_index] < previous_y) ? current_index - 1 : current_index + 1;

        if (current_index >= shifted_results.size()) {
            std::cerr << "Peak Setpoint not reachable." << std::endl;
            return;
        }

        previous_y = shifted_results[current_index];
        steps++;
    }

    std::cout << "Peak Setpoint Reached: ("
        << sine_wave.readXValue(current_index) << ", " << shifted_results[current_index]
        << ") after " << steps << " steps." << std::endl;
}

// Process and align a null setpoint with the shifted sine wave
void processNullSetpoint(const std::vector<double>& shifted_results, const SineWave& sine_wave, double setpoint, size_t index) {
    size_t current_index = index;
    size_t steps = 0;
    const double tolerance = 0.0000001;
    double previous_y = shifted_results[index];

    while (std::abs(shifted_results[current_index] - setpoint) > tolerance) {
        current_index = (shifted_results[current_index] > previous_y) ? current_index - 1 : current_index + 1;

        if (current_index >= shifted_results.size()) {
            std::cerr << "Null Setpoint not reachable." << std::endl;
            return;
        }

        previous_y = shifted_results[current_index];
        steps++;
    }

    std::cout << "Null Setpoint Reached: ("
        << sine_wave.readXValue(current_index) << ", " << shifted_results[current_index]
        << ") after " << steps << " steps." << std::endl;
}

/*

// Find the average for quad calculations
double find_quad(double peak_setpoint) {
    return peak_setpoint / 2;
}

// Method to detect quad minus
void handleQuadMinus(double average_y) {
    for (size_t i = 1; i < SineWaveData::getSize(); ++i) {
        double previous_y = SineWaveData::readYValue(i - 1);
        double current_y = SineWaveData::readYValue(i);

        if (previous_y > current_y) {
            if (std::abs(current_y - average_y) <= threshold) {
                quad_minus_indices.push_back(i);
                std::cout << "Quad Minus detected at index: " << i << " (x, y) = ("
                          << SineWaveData::readXValue(i) << ", " << SineWaveData::readYValue(i) << ")" << std::endl;
            }
        }
    }
}

// Method to detect quad plus
void handleQuadPlus(double average_y) {
    for (size_t i = 1; i < SineWaveData::getSize(); ++i) {
        double previous_y = SineWaveData::readYValue(i - 1);
        double current_y = SineWaveData::readYValue(i);

        if (previous_y < current_y) {
            if (std::abs(current_y - average_y) <= threshold) {
                quad_plus_indices.push_back(i);
                std::cout << "Quad Plus detected at index: " << i << " (x, y) = ("
                          << SineWaveData::readXValue(i) << ", " << SineWaveData::readYValue(i) << ")" << std::endl;
            }
        }
    }
}

// Output quad minus and quad plus points
void outputQuadPoints() {
    std::cout << "Quad Minus Points: ";
    for (size_t idx : quad_minus_indices) {
        double x = SineWaveData::readXValue(idx);
        double y = SineWaveData::readYValue(idx);
        std::cout << "Index: " << idx << " -> x: " << x << ", y: " << y << " | ";
    }
    std::cout << std::endl;

    std::cout << "Quad Plus Points: ";
    for (size_t idx : quad_plus_indices) {
        double x = SineWaveData::readXValue(idx);
        double y = SineWaveData::readYValue(idx);
        std::cout << "Index: " << idx << " -> x: " << x << ", y: " << y << " | ";
    }
    std::cout << std::endl;
}

int main() {
    try {
        const double x_start = 0.0;
        const double x_end = 20.0;
        const double step_size = 0.001;

        double phase_shift = 0;
        // Generate the sine wave data using the static class
        SineWaveData::generateData(x_start, x_end, step_size, phase_shift);

        detectPeaksAndNulls();

        // Find the average for quad points
        double peak_setpoint = SineWaveData::readYValue(peak_indices[1]);
        y_avg = find_quad(peak_setpoint);
        std::cout << "Y_avg: " << y_avg << std::endl;

        // Output detected peaks and nulls
        outputPeaksAndNulls("Peak");
        outputPeaksAndNulls("Null");

        // Handle quad minus and quad plus
        handleQuadMinus(y_avg);
        handleQuadPlus(y_avg);

        // Output Quad Points
        outputQuadPoints();

        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_real_distribution<> dis(-10, 10);
        
        phase_shift = dis(gen);
        
        std::cout << "Phase shift: " << phase_shift << std::endl;

        std::vector<double> shifted_results = SineWaveData::generateShiftedWave(phase_shift);

        for (size_t peak_index : peak_indices) {
            processSetpoint(shifted_results, SineWaveData::readYValue(peak_index), peak_index, true);
        }
        for (size_t null_index : null_indices) {
            processSetpoint(shifted_results, SineWaveData::readYValue(null_index), null_index, false);
        }

    }
    catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
    }

    return 0;
}

*/