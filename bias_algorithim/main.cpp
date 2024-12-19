#include <iostream>
#include <vector>
#include <cmath>  // For sin function
#include <random> // For random number generation

#define MAX_INDICES 5 // Maximum size for peak and null indices
//copy functions straight from pico functions

// SineWave Class: Generates and manages sine wave data
class SineWave {
private:
    double x_start, x_end, step_size;
    std::vector<double> x_values;
    std::vector<double> y_values;

public:
    SineWave(double start, double end, double step) //set analog write to your x value
        : x_start(start), x_end(end), step_size(step) { //set analog read to your y value
        generateXValues();
        generateSineWave();
    }

    // Generate y-values for the sine wave
    void generateSineWave() {
        y_values.clear();
        for (double x : x_values) {
            y_values.emplace_back(1 + std::sin(x + 1));
        }
    }

    // Generate x-values for the sine wave
    void generateXValues() {
        x_values.clear();
        for (double x = x_start; x <= x_end; x += step_size) {
            x_values.emplace_back(x);
        }
    }

    // Generate a shifted sine wave with a given phase shift
    std::vector<double> generateShiftedWave(double phase_shift) const {
        std::vector<double> shifted_results;
        for (double x : x_values) {
            shifted_results.emplace_back(1 + std::sin(x + 1 + phase_shift));
        }
        return shifted_results;
    }

    // Read a y-value from the sine wave, take these out
    double readYValue(size_t index) const {
        if (index >= y_values.size()) {
            throw std::out_of_range("Index out of range for y_values");
        }
        return y_values[index];
    }

    // Read an x-value from the sine wave
    double readXValue(size_t index) const {
        if (index >= x_values.size()) {
            throw std::out_of_range("Index out of range for x_values");
        }
        return x_values[index];
    }

    // Get the size of the y-values
    size_t getSize() const {
        return y_values.size();
    }
};

// Calculate the mean of a given vector of values
double calculateAverage(std::vector<double> values) {
    double sum_y = 0.0;
    for (double y : values) {
        sum_y += y;
    }
    return sum_y / values.size();
}

// Function to calculate the slope between two points (x1, y1) and (x2, y2)
double calculateSlope(double x1, double y1, double x2, double y2) {
    return (y2 - y1) / (x2 - x1);
}

// Detect peaks and add to peak_indices if conditions are met
void handlePeak(size_t i, const SineWave& sine_wave, std::vector<size_t>& peak_indices, double previous_y, int direction) {
    if (direction == 1 && sine_wave.readYValue(i) < previous_y) {
        if (peak_indices.size() < MAX_INDICES) {
            peak_indices.push_back(i - 1);
        }
        else {
            std::cerr << "Peak indices array full." << std::endl;
        }
    }
}

// Detect nulls and add to null_indices if conditions are met
void handleNull(size_t i, const SineWave& sine_wave, std::vector<size_t>& null_indices, double previous_y, int direction) {
    if (direction == -1 && sine_wave.readYValue(i) > previous_y) {
        if (null_indices.size() < MAX_INDICES) {
            null_indices.push_back(i - 1);
        }
        else {
            std::cerr << "Null indices array full." << std::endl;
        }
    }
}

// Detect peaks and nulls in the sine wave
void detectPeaksAndNulls(const SineWave& sine_wave, std::vector<size_t>& peak_indices, std::vector<size_t>& null_indices) {
    double previous_y = sine_wave.readYValue(0);
    int direction = 0; // 0 = unknown, 1 = increasing, -1 = decreasing

    for (size_t i = 1; i < sine_wave.getSize(); ++i) {
        double current_y = sine_wave.readYValue(i);

        if (current_y > previous_y) {
            handleNull(i, sine_wave, null_indices, previous_y, direction);
            direction = 1;
        }
        else if (current_y < previous_y) {
            handlePeak(i, sine_wave, peak_indices, previous_y, direction);
            direction = -1;
        }

        previous_y = current_y;
    }
}

// Output detected peaks or nulls
void outputPeaksAndNulls(const std::vector<size_t>& indices, const SineWave& sine_wave, const std::string& label) {
    std::cout << "All " << label << " Detected (x, y):" << std::endl;
    for (size_t index : indices) {
        std::cout << "(" << sine_wave.readXValue(index) << ", " << sine_wave.readYValue(index) << ")" << std::endl;
    }
}

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

//average of full cycle
//close to A/2
double handleFirstCycleAverage(const std::vector<size_t>& peak_indices, const std::vector<size_t>& null_indices, 
                              const SineWave& sine_wave) {
    // Ensure there are enough nulls to process
    if (null_indices.size() < 2) {
        std::cerr << "Insufficient null points." << std::endl;
        return 0;
    }

    // Get the first pair of null indices
    size_t null_index_start = null_indices[0];  // First null
    size_t null_index_end = null_indices[1];    // Next null (second null)

    // Accumulate the y-values between the first pair of nulls
    std::vector<double> y_values;  // To hold the y-values between the nulls

    // Iterate over the values from the first null to the second null
    for (size_t j = null_index_start; j <= null_index_end; ++j) {
        double current_y = sine_wave.readYValue(j);  // Get the y-value at index j
        y_values.push_back(current_y);  // Add the y-value to the vector
    }

    // Calculate the average y-value over this cycle (null to null) using your function
    double y_avg = calculateAverage(y_values);

    // Output the average y-value to the console
    std::cout << "Average y-value between first null (" << null_index_start << ") and second null (" 
        
              << null_index_end << "): " << y_avg << std::endl;
    
    return y_avg;
}

//example simplified approach
void find_quad(double peak_setpoint) {
    double average = peak_setpoint / 2;
}

void handleQuadMinus(const SineWave& sine_wave, double average_y, std::vector<size_t>& quad_minus_indices, double threshold) {
    // Iterate through the entire sine wave data starting from the second value
    for (size_t i = 1; i < sine_wave.getSize(); ++i) {
        double previous_y = sine_wave.readYValue(i - 1);
        double current_y = sine_wave.readYValue(i);

        // Check if the slope is negative (decreasing)
        if (previous_y > current_y) {
            // If the current_y is close enough to the average value (within the threshold), add to quad_minus_indices
            if (std::abs(current_y - average_y) <= threshold) {
                quad_minus_indices.push_back(i);
            }
        }
    }
}

void handleQuadPlus(const SineWave& sine_wave, double average_y, std::vector<size_t>& quad_plus_indices, double threshold) {
    // Iterate through the entire sine wave data starting from the second value
    for (size_t i = 1; i < sine_wave.getSize(); ++i) {
        double previous_y = sine_wave.readYValue(i - 1);
        double current_y = sine_wave.readYValue(i);

        // Check if the slope is positive (increasing)
        if (previous_y < current_y) {
            // If the current_y is close enough to the average value (within the threshold), add to quad_plus_indices
            if (std::abs(current_y - average_y) <= threshold) {
                quad_plus_indices.push_back(i);
            }
        }
    }
}


// Main function
int main() {
    try {
        double threshold = 0.0003; //this is the threshold to find proper quad values

        const double x_start = 0.0;
        const double x_end = 20.0;
        const double step_size = 0.001;

        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_real_distribution<> dis(-10, 10);
        double phase_shift = dis(gen);
        std::cout << "Phase shift: " << phase_shift << std::endl;

        SineWave sine_wave(x_start, x_end, step_size);
        std::vector<double> shifted_results = sine_wave.generateShiftedWave(phase_shift);

        // Detect Peaks and Nulls
        std::vector<size_t> peak_indices;
        std::vector<size_t> null_indices;
        std::vector<size_t> quad_minus_indices;
        std::vector<size_t> quad_plus_indices;

        /*loop, start at 0 value and end at higest value (write, x)
        write 0, read y
        pass these values into detect functions
*/

        //sweep(sine_wave) function


        detectPeaksAndNulls(sine_wave, peak_indices, null_indices);
        // Call the handleMidpoint function to find the average y-value between the first null and peak
        double y_avg = handleFirstCycleAverage(peak_indices, null_indices, sine_wave);
        outputPeaksAndNulls(peak_indices, sine_wave, "Peak");
        outputPeaksAndNulls(null_indices, sine_wave, "Null");

        // Call the functions to fill these vectors
        handleQuadMinus(sine_wave, y_avg, quad_minus_indices, threshold);
        handleQuadPlus(sine_wave, y_avg, quad_plus_indices, threshold);

        for (size_t peak_index : peak_indices) {
            processPeakSetpoint(shifted_results, sine_wave, sine_wave.readYValue(peak_index), peak_index);
        }
        for (size_t null_index : null_indices) {
            processNullSetpoint(shifted_results, sine_wave, sine_wave.readYValue(null_index), null_index);
        }
        for (size_t quad_minus_index: quad_minus_indices) {
            processQuadSetpoint(shifted_results, sine_wave, sine_wave.readYValue(quad_minus_index), quad_minus_index);
        }
        for (size_t quad_plus_index: quad_plus_indices) {
            processQuadSetpoint(shifted_results, sine_wave, sine_wave.readYValue(quad_plus_index), quad_plus_index);
        }

        // Output Quad Minus and Quad Plus points with their corresponding x and y values
        std::cout << "Quad Minus Points: ";
        for (size_t idx : quad_minus_indices) {
            double x = sine_wave.readXValue(idx);  // Get x-value at index
            double y = sine_wave.readYValue(idx);  // Get y-value at index
            std::cout << "Index: " << idx << " -> x: " << x << ", y: " << y << " | ";
        }
        std::cout << std::endl;

        std::cout << "Quad Plus Points: ";
        for (size_t idx : quad_plus_indices) {
            double x = sine_wave.readXValue(idx);  // Get x-value at index
            double y = sine_wave.readYValue(idx);  // Get y-value at index
            std::cout << "Index: " << idx << " -> x: " << x << ", y: " << y << " | ";
        }
        std::cout << std::endl;

    }
    catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
    }

    return 0;
}
