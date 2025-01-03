#include <iostream>
#include <vector>
#include <cmath>  // For sin function
#include <random> // For random number generation

#define MAX_INDICES 5 // Maximum size for peak and null indices

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


//removed shifted wave, and instead will regrenrate data with phase shift other than 0, functions now resemble rPi pico counterparts
/*
class SineWaveData {
public:
    static std::vector<double> x_values;
    static std::vector<double> y_values;
    static size_t last_accessed_index;  // Track the last accessed index for both X and Y values

    // Static methods to access x and y values
    static void generateData(double x_start, double x_end, double step_size, double phase_shift) {
        x_values.clear();
        y_values.clear();
        for (double x = x_start; x <= x_end; x += step_size) {
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
        std::cout << "Setting PWM DAC with step value: " << step_value << std::endl;

        // Update the last accessed index (this will be the current index of the X value)
        last_accessed_index = index;  // Track the last accessed index
    }

    // Replace readYValue with analog_read(GPIO_PIN), simply pass through the function
    static double analog_read_GPIO() {
        // Simulate reading from an analog pin
        // Replace with the actual GPIO reading code
        double voltage = y_values[last_accessed_index];  // Return the Y value corresponding to the last accessed index
        std::cout << "Reading GPIO for Y value: " << voltage << std::endl;
        return voltage;
    }

    static size_t getSize() {
        return x_values.size();
    }
};

// Initialize static members
size_t SineWaveData::last_accessed_index = 0;  // Initializing the static member
*/

// Static member variable initialization
std::vector<double> SineWaveData::x_values;
std::vector<double> SineWaveData::y_values;

// Peak and Null Detection functions (outside of SineWaveData class)
std::vector<size_t> peak_indices;
std::vector<size_t> null_indices;
std::vector<size_t> quad_minus_indices;
std::vector<size_t> quad_plus_indices;

double threshold = 0.0005;  // Threshold for quad points detection
const double tolerance = 0.0000001;
double y_avg = 0;  // Average value for quad detection

// Method to detect peaks and nulls in the sine wave

// TODO: Instead of reading and writing values, create a sweep function that sweep through the test data using the read and write functions
// and write to a presized array then pass the array seperatly to a detect peaks, detect nulls, and detect quad functions by indexing through the array

//TODO: Implement this scanning algorithim
// Function to scan the PWM range and populate the result array
/*
std::vector<double> scanPWM() {
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

    // Optionally, output the result to verify (only if necessary)
    std::cout << "PWM scan complete. Array populated with " << resultArray.size() << " values." << std::endl;

    return resultArray;  // Return the populated array
}
*/


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

*/

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
