/**
 * @file ToggleTimer.hpp
 * @brief Timer with start/stop/toggle functionality
 */

#ifndef TOGGLE_TIMER_HPP
#define TOGGLE_TIMER_HPP

#include <chrono>
#include <cstdint>
#include <ratio>

/**
 * @class ToggleTimer
 * @brief Timer with start, stop, and toggle support
 * 
 * Measures time between start() and stop() calls.
 * Supports time accumulation across multiple start/stop cycles.
 */
class ToggleTimer {
public:
    using clock = std::chrono::steady_clock;
    using nanoseconds_t = std::chrono::nanoseconds;

private:
    clock::time_point start_point{};
    nanoseconds_t accumulated_time{0};
    bool running = false;

public:
    ToggleTimer() = default;

    /**
     * @brief Start the timer
     * @note Does nothing if already running
     */
    void start() noexcept {
        if (!running) {
            running = true;
            start_point = clock::now();
        }
    }

    /**
     * @brief Stop the timer
     * @note Does nothing if already stopped
     */
    void stop() noexcept {
        if (running) {
            const auto end = clock::now();
            accumulated_time += std::chrono::duration_cast<nanoseconds_t>(end - start_point);
            running = false;
        }
    }

    /**
     * @brief Toggle timer state (start/stop)
     */
    void toggle() noexcept {
        if (running) {
            stop();
        } else {
            start();
        }
    }

    /**
     * @brief Get total measured time
     * @return Time in nanoseconds (nanoseconds_t type)
     * @note Includes current measurement if timer is active
     */
    [[nodiscard]] nanoseconds_t elapsed() const noexcept {
        if (running) {
            const auto current = clock::now();
            return accumulated_time + 
                   std::chrono::duration_cast<nanoseconds_t>(current - start_point);
        }
        return accumulated_time;
    }

    // --- Time retrieval methods in different units ---

    /// @return Time in nanoseconds (integer)
    [[nodiscard]] int64_t nanoseconds() const noexcept {
        return elapsed().count();
    }

    /// @return Time in microseconds (floating-point)
    [[nodiscard]] double microseconds() const noexcept {
        return elapsed().count() / 1'000.0;
    }

    /// @return Time in milliseconds (floating-point)
    [[nodiscard]] double milliseconds() const noexcept {
        return elapsed().count() / 1'000'000.0;
    }

    /// @return Time in seconds (floating-point)
    [[nodiscard]] double seconds() const noexcept {
        return elapsed().count() / 1'000'000'000.0;
    }

    /**
     * @brief Universal method for time retrieval
     * @tparam Duration Duration type (e.g., std::chrono::milliseconds)
     * @return Time in specified units
     */
    template<typename Duration = std::chrono::duration<double>>
    [[nodiscard]] Duration duration() const noexcept {
        return std::chrono::duration_cast<Duration>(elapsed());
    }

    // --- Control methods ---

    /// @return true if timer is running
    [[nodiscard]] bool is_running() const noexcept {
        return running;
    }

    /// @brief Reset timer (stop and zero out)
    void reset() noexcept {
        running = false;
        accumulated_time = nanoseconds_t{0};
    }

    /// @brief Alias for stop()
    void pause() noexcept { stop(); }
    
    /// @brief Alias for start()
    void resume() noexcept { start(); }
    
    /// @return true if any time has been measured
    [[nodiscard]] bool has_measured() const noexcept {
        return accumulated_time.count() > 0;
    }
};

#endif // TOGGLE_TIMER_HPP
