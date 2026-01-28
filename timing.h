#pragma once
#include <chrono>
#include <cstdio>

static std::chrono::steady_clock::time_point begin;
static std::chrono::steady_clock::time_point end;
static bool is_timing = false;

/**
 * @brief Simple toggle timer for measuring code execution.
 *
 * Call once to start the timer, call again to stop and print elapsed time.
 * Outputs in milliseconds, microseconds, and nanoseconds.
 *
 * @return double Returns elapsed time in nanoseconds if stopped, otherwise 0.
 */
double time_code() {
    if (is_timing) {
        // Stop timer
        end = std::chrono::steady_clock::now();

        // Convert once to nanoseconds
        long long ns = std::chrono::duration_cast<std::chrono::nanoseconds>(end - begin).count();
        long long mcs = ns / 1000;
        long long ms  = ns / 1000000;

        // Single printf
        printf("Execution time:\n%lldms\n%lldus\n%lldns\n", ms, mcs, ns);

        is_timing = false;
        return (double)ns;
    } else {
        // Start timer
        is_timing = true;
        begin = std::chrono::steady_clock::now();
        return 0;
    }
}

namespace time_labels {

/**
 * @brief Class for measuring execution time over multiple iterations.
 *
 * Supports single measurement, iteration count, and average time calculation.
 */
class time_label {
private:
    std::chrono::steady_clock::time_point temp_begin = std::chrono::steady_clock::now();
    std::chrono::steady_clock::time_point global_begin = temp_begin;
    std::chrono::steady_clock::time_point temp_end;
    long long number_of_iterations = 0;

public:

    /**
     * @brief Measures time since last mark and prints it.
     *
     * Increments the iteration counter.
     */
    void time() {
        temp_end = temp_begin;
        temp_begin = std::chrono::steady_clock::now();
        number_of_iterations++;

        long long ns  = std::chrono::duration_cast<std::chrono::nanoseconds>(temp_begin - temp_end).count();
        long long mcs = ns / 1000;
        long long ms  = ns / 1000000;

        printf("Execution time:\n%lldms\n%lldus\n%lldns\n", ms, mcs, ns);

        temp_begin = std::chrono::steady_clock::now();
    }

    /**
     * @brief Increments iteration counter without printing.
     */
    void tick() {
        number_of_iterations++;
    }

    /**
     * @brief Prints average time per iteration since creation or last reset.
     */
    void average() {
        temp_begin = std::chrono::steady_clock::now();
        long long total_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(temp_begin - global_begin).count();
        long long ns_per_iter  = total_ns / number_of_iterations;
        long long mcs_per_iter = ns_per_iter / 1000;
        long long ms_per_iter  = ns_per_iter / 1000000;

        printf("Average time over %lld iterations:\n%lldms\n%lldus\n%lldns\n",
               number_of_iterations, ms_per_iter, mcs_per_iter, ns_per_iter);

        temp_begin = std::chrono::steady_clock::now();
    }

    /**
     * @brief Resets the timer and iteration count.
     */
    void reset() {
        number_of_iterations = 0;
        temp_begin = std::chrono::steady_clock::now();
        global_begin = temp_begin;
    }

    /**
     * @brief Placeholder for pause functionality.
     */
    void pause() {}
};

} // namespace time_labels
