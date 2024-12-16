#include <iostream>
#include <vector>
#include <cmath>  // For sin function
#include <random> // For random number generation

class SineWave {
private:
    double x_start, x_end, step_size, phase_shift;
    std::vector<double> x_values;
    std::vector<double> results;
    std::vector<double> shifted_results;
    std::vector<size_t> peak_indices;
    std::vector<size_t> null_indices;

    // Generate sine wave values
    void generateSineWave() {
        for (double x = x_start; x <= x_end; x += step_size) {
            double y = 1 + std::sin(x + 1); // Original sine wave equation
            results.push_back(y);
            x_values.push_back(x);
        }
    }

    // Generate shifted sine wave values
    void generateShiftedWave() {
        for (double x : x_values) {
            double shifted_y = 1 + std::sin(x + 1 + phase_shift);
            shifted_results.push_back(shifted_y);
        }
    }

    // Detect peaks and nulls
    void detectPeaksAndNulls() {
        double previous_y = results[0];
        int direction = 0; // 0 = unknown, 1 = increasing, -1 = decreasing

        for (size_t i = 1; i < results.size(); ++i) {
            double current_y = results[i];

            if (current_y > previous_y) {
                if (direction == -1) {
                    null_indices.push_back(i - 1); // Local minimum
                }
                direction = 1;
            } else if (current_y < previous_y) {
                if (direction == 1) {
                    peak_indices.push_back(i - 1); // Local maximum
                }
                direction = -1;
            }

            previous_y = current_y;
        }
    }

public:
    // Constructor to initialize parameters and generate waves
    SineWave(double start, double end, double step, double phase)
        : x_start(start), x_end(end), step_size(step), phase_shift(phase) {
        generateSineWave();
        generateShiftedWave();
        detectPeaksAndNulls();
    }

    // Get quadrature mean y-value
    double getQuadratureMean() {
        double sum_y = 0.0;
        for (double y : results) {
            sum_y += y;
        }
        return sum_y / results.size();
    }

    // Output all peaks and nulls
    void outputPeaksAndNulls() {
        std::cout << "All Peaks Detected (x, y):" << std::endl;
        for (size_t peak_index : peak_indices) {
            std::cout << "(" << x_values[peak_index] << ", " << results[peak_index] << ")" << std::endl;
        }

        std::cout << "All Nulls Detected (x, y):" << std::endl;
        for (size_t null_index : null_indices) {
            std::cout << "(" << x_values[null_index] << ", " << results[null_index] << ")" << std::endl;
        }
    }

    // Process a single point (either peak or null) to find its shifted setpoint
    void processSetpoint(size_t index, bool is_peak) {
        double setpoint = results[index];
        size_t current_index = index;
        size_t steps = 0;
        const double tolerance = 0.001;
        double previous_y = shifted_results[index];

        while (std::abs(shifted_results[current_index] - setpoint) > tolerance) {
            if (is_peak) {
                if (shifted_results[current_index] < previous_y) {
                    current_index = (current_index > 0) ? current_index - 1 : current_index + 1;
                } else {
                    current_index = (current_index < shifted_results.size() - 1) ? current_index + 1 : current_index - 1;
                }
            } else {
                if (shifted_results[current_index] > previous_y) {
                    current_index = (current_index > 0) ? current_index - 1 : current_index + 1;
                } else {
                    current_index = (current_index < shifted_results.size() - 1) ? current_index + 1 : current_index - 1;
                }
            }

            previous_y = shifted_results[current_index];
            steps++;
            if (steps > shifted_results.size()) {
                std::cerr << "Setpoint not reachable." << std::endl;
                return;
            }
        }

        std::cout << (is_peak ? "Peak" : "Null") << " Setpoint Reached: ("
                  << x_values[current_index] << ", " << shifted_results[current_index]
                  << ") after " << steps << " steps." << std::endl;
    }

    // Process all peaks and nulls
    void processAllSetpoints() {
        for (size_t peak_index : peak_indices) {
            processSetpoint(peak_index, true);
        }
        for (size_t null_index : null_indices) {
            processSetpoint(null_index, false);
        }
    }
};

int main() {
    // Define parameters
    const double x_start = 0.0;
    const double x_end = 20.0;
    const double step_size = 0.05;

    // Generate a random phase shift between -0.2 and 0.2
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<> dis(-0.2, 0.2);
    double phase_shift = dis(gen);

    // Create sine wave object
    SineWave sine_wave(x_start, x_end, step_size, phase_shift);

    // Output quadrature mean
    double quad_mean_y = sine_wave.getQuadratureMean();
    std::cout << "Quadrature Mean Y-value: " << quad_mean_y << std::endl;

    // Output all peaks and nulls
    sine_wave.outputPeaksAndNulls();

    // Process all peaks and nulls
    sine_wave.processAllSetpoints();

    return 0;
}