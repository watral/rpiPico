#include <iostream>
#include <vector>
#include <cmath>  // For sin function
#include <random> // For random number generation

#define MAX_INDICES 5 // Maximum size for peak and null indices

// SineWave Class: Generates and manages sine wave data
class SineWave {
private:
    double x_start, x_end, step_size;
    std::vector<double> x_values;
    std::vector<double> y_values;

public:
    SineWave(double start, double end, double step)
        : x_start(start), x_end(end), step_size(step) {
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

    // Read a y-value from the sine wave
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
double calculateMean(std::vector<double> values) {
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

// Process and align a peak setpoint with the shifted sine wave
void processPeakSetpoint(const std::vector<double>& shifted_results, const SineWave& sine_wave, double setpoint, size_t index) {
    size_t current_index = index;
    size_t steps = 0;
    const double tolerance = 0.001;
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
    const double tolerance = 0.001;
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

// Function to handle quad minus points (negative slope)
void handleQuadMinus(const std::vector<size_t>& peak_indices, const std::vector<size_t>& null_indices, 
                      const SineWave& sine_wave, std::vector<size_t>& quad_minus_indices) {
    // Ensure there are enough peaks and nulls to process
    if (null_indices.empty() || peak_indices.empty()) {
        std::cerr << "Insufficient peaks or nulls." << std::endl;
        return;
    }

    // Iterate over peak points
    for (size_t peak_index : peak_indices) {
        double peak_x = sine_wave.readXValue(peak_index);
        double peak_y = sine_wave.readYValue(peak_index);
        
        // Iterate through the null points after this peak
        for (size_t null_index = 0; null_index < null_indices.size(); ++null_index) {
            if (null_indices[null_index] > peak_index) {
                double null_x = sine_wave.readXValue(null_indices[null_index]);
                double null_y = sine_wave.readYValue(null_indices[null_index]);
                double max_slope = 0;
                size_t max_slope_index = peak_index;
                
                // Iterate from peak to the next null, calculating slopes at each step
                for (size_t i = peak_index + 1; i <= null_indices[null_index]; ++i) {
                    double current_x = sine_wave.readXValue(i);
                    double current_y = sine_wave.readYValue(i);
                    double slope = calculateSlope(peak_x, peak_y, current_x, current_y);

                    // Check if this slope is the largest so far
                    if (std::abs(slope) > max_slope) {
                        max_slope = std::abs(slope);
                        max_slope_index = i;
                    }
                }

                // Store the index of the quad minus point with the largest slope
                quad_minus_indices.push_back(max_slope_index);
                break; // Move to the next peak
            }
        }
    }
}

// Function to handle quad plus points (positive slope)
void handleQuadPlus(const std::vector<size_t>& peak_indices, const std::vector<size_t>& null_indices, 
                     const SineWave& sine_wave, std::vector<size_t>& quad_plus_indices) {
    // Ensure there are enough peaks and nulls to process
    if (null_indices.empty() || peak_indices.empty()) {
        std::cerr << "Insufficient peaks or nulls." << std::endl;
        return;
    }

    // Iterate over null points
    for (size_t null_index : null_indices) {
        double null_x = sine_wave.readXValue(null_index);
        double null_y = sine_wave.readYValue(null_index);

        // Iterate through the peaks after this null
        for (size_t peak_index = 0; peak_index < peak_indices.size(); ++peak_index) {
            if (peak_indices[peak_index] > null_index) {
                double peak_x = sine_wave.readXValue(peak_indices[peak_index]);
                double peak_y = sine_wave.readYValue(peak_indices[peak_index]);
                double max_slope = 0;
                size_t max_slope_index = null_index;
                
                // Iterate from null to the next peak, calculating slopes at each step
                for (size_t i = null_index + 1; i <= peak_indices[peak_index]; ++i) {
                    double current_x = sine_wave.readXValue(i);
                    double current_y = sine_wave.readYValue(i);
                    double slope = calculateSlope(null_x, null_y, current_x, current_y);

                    // Check if this slope is the largest so far
                    if (std::abs(slope) > max_slope) {
                        max_slope = std::abs(slope);
                        max_slope_index = i;
                    }
                }

                // Store the index of the quad plus point with the largest slope
                quad_plus_indices.push_back(max_slope_index);
                break; // Move to the next null
            }
        }
    }
}

// Main function
int main() {
    try {
        const double x_start = 0.0;
        const double x_end = 20.0;
        const double step_size = 0.05;

        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_real_distribution<> dis(-10, 10);
        double phase_shift = dis(gen);
        std::cout << "Phase shift: " << phase_shift << std::endl;

        SineWave sine_wave(x_start, x_end, step_size);
        std::vector<double> shifted_results = sine_wave.generateShiftedWave(phase_shift);

        double quad_mean_y = calculateMean(shifted_results);
        std::cout << "Mean of shifted sine wave: " << quad_mean_y << std::endl;

        // Detect Peaks and Nulls
        std::vector<size_t> peak_indices;
        std::vector<size_t> null_indices;

        detectPeaksAndNulls(sine_wave, peak_indices, null_indices);
        outputPeaksAndNulls(peak_indices, sine_wave, "Peak");
        outputPeaksAndNulls(null_indices, sine_wave, "Null");

        // Detect and process Quad Plus and Quad Minus Points
        std::vector<size_t> quad_plus_indices;
        std::vector<size_t> quad_minus_indices;

        handleQuadPlus(peak_indices, null_indices, sine_wave, quad_plus_indices);
        handleQuadMinus(peak_indices, null_indices, sine_wave, quad_minus_indices);

        // Output Quad Points
        std::cout << "Quad Plus Points: " << std::endl;
        for (size_t index : quad_plus_indices) {
            std::cout << "(" << sine_wave.readXValue(index) << ", " << sine_wave.readYValue(index) << ")" << std::endl;
        }

        std::cout << "Quad Minus Points: " << std::endl;
        for (size_t index : quad_minus_indices) {
            std::cout << "(" << sine_wave.readXValue(index) << ", " << sine_wave.readYValue(index) << ")" << std::endl;
        }

    }
    catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
    }

    return 0;
}
