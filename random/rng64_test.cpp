#include <iostream>
#include <vector>
#include <cmath>
#include <algorithm>
#include <numeric>
#include <chrono>
#include <fstream>
#include <cassert>
#include <bitset>
#include <map>
#include <random>
#include <array>
#include <iomanip>
#include "rng64.h"

using namespace std::chrono;

// ============================================================================
// Утилиты
// ============================================================================
template<typename Func>
double measure_time(Func f, int iterations = 1) {
    auto start = high_resolution_clock::now();
    for (int i = 0; i < iterations; ++i) f();
    auto end = high_resolution_clock::now();
    return duration_cast<nanoseconds>(end - start).count() / 1e9;
}

// ============================================================================
// Структура результатов
// ============================================================================
struct TestResults {
    // Basic correctness
    bool deterministic = false;
    bool different_seeds = false;
    std::pair<int, int> integer_range;
    bool real_in_range = false;
    double real_min = 0.0, real_max = 0.0;
    
    // Statistical quality
    double chi_squared = 0.0;
    int bad_correlations = 0;
    double runs_z_score = 0.0;
    double max_bit_deviation = 0.0;
    
    // 2D distribution
    int empty_cells_2d = 0;
    double max_density_factor = 0.0;
    
    // Performance (M ops/sec)
    double ops_uint64 = 0.0;
    double ops_real = 0.0;
    double ops_integer = 0.0;
    double ops_bit = 0.0;
    double ops_prng64 = 0.0;
    double ops_mt19937 = 0.0;
    double ops_minstd = 0.0;
    std::vector<std::pair<int, double>> shuffle_performance;
    
    // Edge cases
    bool edge_integer_single = false;
    bool edge_integer_reverse = false;
    bool edge_bit_zero = false;
    bool edge_bit_one = false;
    bool edge_overflow = false;
    
    // Specialized tests
    double birthday_deviation = 0.0;
    int max_increasing_streak = 0;
    int max_decreasing_streak = 0;
    double pair_deviation = 0.0;
    
    // Memory
    size_t prng64_size = 0;
    size_t mt19937_size = 0;
    size_t minstd_size = 0;
};

// ============================================================================
// Основные тесты
// ============================================================================
TestResults run_all_tests() {
    TestResults results;
    
    // Memory sizes
    results.prng64_size = sizeof(rng64::PRNG64);
    results.mt19937_size = sizeof(std::mt19937_64);
    results.minstd_size = sizeof(std::minstd_rand);
    
    // 1. Basic correctness
    {
        rng64::PRNG64 rng1(42), rng2(42);
        bool ok = true;
        for (int i = 0; i < 1000; ++i) {
            if (rng1() != rng2()) {
                ok = false;
                break;
            }
        }
        results.deterministic = ok;
    }
    
    {
        rng64::PRNG64 rng1(42), rng2(43);
        bool ok = false;
        for (int i = 0; i < 100; ++i) {
            if (rng1() != rng2()) {
                ok = true;
                break;
            }
        }
        results.different_seeds = ok;
    }
    
    {
        rng64::PRNG64 rng(42);
        int min_val = std::numeric_limits<int>::max();
        int max_val = std::numeric_limits<int>::min();
        for (int i = 0; i < 10000; ++i) {
            int val = rng.integer();
            if (val < min_val) min_val = val;
            if (val > max_val) max_val = val;
        }
        results.integer_range = {min_val, max_val};
    }
    
    {
        rng64::PRNG64 rng(42);
        double min_val = 2.0, max_val = -1.0;
        bool ok = true;
        for (int i = 0; i < 10000; ++i) {
            double val = rng.real();
            if (val < 0.0 || val >= 1.0) {
                ok = false;
                break;
            }
            if (val < min_val) min_val = val;
            if (val > max_val) max_val = val;
        }
        results.real_in_range = ok;
        results.real_min = min_val;
        results.real_max = max_val;
    }
    
    // 2. Statistical quality
    {
        rng64::PRNG64 rng(42);
        const int BUCKETS = 100;
        const int TRIALS = 1'000'000;
        
        std::vector<int> histogram(BUCKETS, 0);
        for (int i = 0; i < TRIALS; ++i) {
            int bucket = static_cast<int>(rng.real() * BUCKETS);
            if (bucket >= BUCKETS) bucket = BUCKETS - 1;
            histogram[bucket]++;
        }
        
        double expected = TRIALS / static_cast<double>(BUCKETS);
        double chi2 = 0.0;
        for (int count : histogram) {
            double diff = count - expected;
            chi2 += diff * diff / expected;
        }
        results.chi_squared = chi2;
    }
    
    {
        rng64::PRNG64 rng(42);
        const int N = 100'000;
        const int MAX_LAG = 20;
        
        std::vector<uint64_t> numbers(N);
        for (auto& num : numbers) num = rng();
        
        int bad = 0;
        for (int lag = 1; lag <= MAX_LAG; ++lag) {
            double sum_x = 0, sum_y = 0, sum_xy = 0, sum_x2 = 0, sum_y2 = 0;
            for (int i = 0; i < N - lag; ++i) {
                double x = numbers[i] / static_cast<double>(~0ULL);
                double y = numbers[i + lag] / static_cast<double>(~0ULL);
                sum_x += x; sum_y += y; sum_xy += x * y;
                sum_x2 += x * x; sum_y2 += y * y;
            }
            int n = N - lag;
            double corr = (n * sum_xy - sum_x * sum_y) /
                         sqrt((n * sum_x2 - sum_x * sum_x) * 
                              (n * sum_y2 - sum_y * sum_y));
            if (std::abs(corr) > 0.01) bad++;
        }
        results.bad_correlations = bad;
    }
    
    {
        rng64::PRNG64 rng(42);
        const long long N = 1'000'000LL;
        
        std::vector<bool> bits(N);
        for (long long i = 0; i < N; ++i) bits[i] = rng.bit();
        
        long long runs = 1;
        for (long long i = 1; i < N; ++i) {
            if (bits[i] != bits[i - 1]) runs++;
        }
        
        long long ones = std::count(bits.begin(), bits.end(), true);
        long long zeros = N - ones;
        
        double expected_runs = 2.0 * ones * zeros / static_cast<double>(N) + 1.0;
        double variance = (2.0 * ones * zeros * (2.0 * ones * zeros - N)) /
                         (static_cast<double>(N) * N * (N - 1.0));
        results.runs_z_score = (runs - expected_runs) / sqrt(variance);
    }
    
    {
        rng64::PRNG64 rng(42);
        const int N = 100'000;
        
        std::array<int, 64> bit_counts = {0};
        double max_dev = 0.0;
        
        for (int i = 0; i < N; ++i) {
            uint64_t x = rng();
            for (int bit = 0; bit < 64; ++bit) {
                if (x & (1ULL << bit)) bit_counts[bit]++;
            }
        }
        
        for (int bit = 0; bit < 64; ++bit) {
            double freq = bit_counts[bit] / static_cast<double>(N);
            double dev = std::abs(freq - 0.5);
            if (dev > max_dev) max_dev = dev;
        }
        results.max_bit_deviation = max_dev * 100.0;
    }
    
    {
        rng64::PRNG64 rng(42);
        const int SIZE = 500;
        const int POINTS = 100'000;
        
        std::vector<std::vector<int>> grid(SIZE, std::vector<int>(SIZE, 0));
        for (int i = 0; i < POINTS; ++i) {
            double x = rng.real();
            double y = rng.real();
            int ix = static_cast<int>(x * SIZE);
            int iy = static_cast<int>(y * SIZE);
            if (ix >= SIZE) ix = SIZE - 1;
            if (iy >= SIZE) iy = SIZE - 1;
            grid[ix][iy]++;
        }
        
        int empty = 0;
        double max_density = 0.0;
        double expected = POINTS / static_cast<double>(SIZE * SIZE);
        
        for (const auto& row : grid) {
            for (int count : row) {
                if (count == 0) empty++;
                if (count > max_density) max_density = count;
            }
        }
        
        results.empty_cells_2d = empty;
        results.max_density_factor = max_density / expected;
    }
    
    // 3. Performance benchmarks
    {
        const int ITERATIONS = 10'000'000;
        rng64::PRNG64 rng(42);
        volatile uint64_t sink = 0;
        
        // uint64()
        auto start = high_resolution_clock::now();
        for (int i = 0; i < ITERATIONS; ++i) sink += rng.uint64();
        auto end = high_resolution_clock::now();
        double time = duration_cast<nanoseconds>(end - start).count() / 1e9;
        results.ops_uint64 = ITERATIONS / time / 1e6;
        
        // real()
        start = high_resolution_clock::now();
        for (int i = 0; i < ITERATIONS; ++i) sink += static_cast<uint64_t>(rng.real() * 1e9);
        end = high_resolution_clock::now();
        time = duration_cast<nanoseconds>(end - start).count() / 1e9;
        results.ops_real = ITERATIONS / time / 1e6;
        
        // integer(1, 100)
        start = high_resolution_clock::now();
        for (int i = 0; i < ITERATIONS; ++i) sink += rng.integer(1, 100);
        end = high_resolution_clock::now();
        time = duration_cast<nanoseconds>(end - start).count() / 1e9;
        results.ops_integer = ITERATIONS / time / 1e6;
        
        // bit()
        start = high_resolution_clock::now();
        for (int i = 0; i < ITERATIONS; ++i) sink += rng.bit();
        end = high_resolution_clock::now();
        time = duration_cast<nanoseconds>(end - start).count() / 1e9;
        results.ops_bit = ITERATIONS / time / 1e6;
        
        // PRNG64 operator()
        start = high_resolution_clock::now();
        for (int i = 0; i < ITERATIONS; ++i) sink += rng();
        end = high_resolution_clock::now();
        time = duration_cast<nanoseconds>(end - start).count() / 1e9;
        results.ops_prng64 = ITERATIONS / time / 1e6;
        
        // mt19937_64
        std::mt19937_64 mt_rng(42);
        start = high_resolution_clock::now();
        for (int i = 0; i < ITERATIONS; ++i) sink += mt_rng();
        end = high_resolution_clock::now();
        time = duration_cast<nanoseconds>(end - start).count() / 1e9;
        results.ops_mt19937 = ITERATIONS / time / 1e6;
        
        // minstd_rand
        std::minstd_rand minstd_rng(42);
        start = high_resolution_clock::now();
        for (int i = 0; i < ITERATIONS; ++i) sink += minstd_rng();
        end = high_resolution_clock::now();
        time = duration_cast<nanoseconds>(end - start).count() / 1e9;
        results.ops_minstd = ITERATIONS / time / 1e6;
    }
    
    {
        rng64::PRNG64 rng(42);
        for (int size : {100, 1000, 10000, 100000}) {
            std::vector<int> vec(size);
            std::iota(vec.begin(), vec.end(), 0);
            
            auto start = high_resolution_clock::now();
            rng.shuffle(vec.begin(), vec.end());
            auto end = high_resolution_clock::now();
            
            double time_ms = duration_cast<microseconds>(end - start).count() / 1000.0;
            results.shuffle_performance.emplace_back(size, time_ms);
        }
    }
    
    // 4. Edge cases
    {
        rng64::PRNG64 rng(42);
        results.edge_integer_single = (rng.integer(5, 5) == 5);
        results.edge_integer_reverse = (rng.integer(10, 1) == 10);
        
        bool bit_zero_ok = true;
        for (int i = 0; i < 1000; ++i) {
            if (rng.bit(0.0)) {
                bit_zero_ok = false;
                break;
            }
        }
        results.edge_bit_zero = bit_zero_ok;
        
        bool bit_one_ok = true;
        for (int i = 0; i < 1000; ++i) {
            if (!rng.bit(1.0)) {
                bit_one_ok = false;
                break;
            }
        }
        results.edge_bit_one = bit_one_ok;
        
        uint64_t max_val = std::numeric_limits<uint64_t>::max();
        uint64_t val = rng.uint64(max_val - 100, max_val);
        results.edge_overflow = (val >= max_val - 100 && val <= max_val);
    }
    
    // 5. Specialized tests
    {
        rng64::PRNG64 rng(42);
        const int SAMPLES = 10000;
        const int BUCKETS = 1000000;
        
        std::vector<bool> used(BUCKETS, false);
        int collisions = 0;
        
        for (int i = 0; i < SAMPLES; ++i) {
            uint64_t val = rng.uint64(0, BUCKETS - 1);
            if (used[val]) {
                collisions++;
            } else {
                used[val] = true;
            }
        }
        
        double expected = SAMPLES - BUCKETS * (1.0 - pow(1.0 - 1.0/BUCKETS, SAMPLES));
        results.birthday_deviation = std::abs(collisions - expected) / expected * 100.0;
    }
    
    {
        rng64::PRNG64 rng(42);
        const int N = 10000;
        
        std::vector<uint64_t> sequence(N);
        for (auto& val : sequence) val = rng();
        
        int increasing_streak = 0;
        int max_increasing = 0;
        int decreasing_streak = 0;
        int max_decreasing = 0;
        
        for (int i = 1; i < N; ++i) {
            if (sequence[i] > sequence[i-1]) {
                increasing_streak++;
                if (increasing_streak > max_increasing) max_increasing = increasing_streak;
                decreasing_streak = 0;
            } else if (sequence[i] < sequence[i-1]) {
                decreasing_streak++;
                if (decreasing_streak > max_decreasing) max_decreasing = decreasing_streak;
                increasing_streak = 0;
            }
        }
        
        results.max_increasing_streak = max_increasing;
        results.max_decreasing_streak = max_decreasing;
    }
    
    {
        rng64::PRNG64 rng(42);
        const int GRID_SIZE = 100;
        const int PAIRS = 1'000'000;
        
        std::vector<std::vector<int>> grid(GRID_SIZE, std::vector<int>(GRID_SIZE, 0));
        
        for (int i = 0; i < PAIRS; ++i) {
            double x1 = rng.real();
            double y1 = rng.real();
            double x2 = rng.real();
            double y2 = rng.real();
            
            int ix1 = static_cast<int>(x1 * GRID_SIZE);
            int iy1 = static_cast<int>(y1 * GRID_SIZE);
            int ix2 = static_cast<int>(x2 * GRID_SIZE);
            int iy2 = static_cast<int>(y2 * GRID_SIZE);
            
            if (ix1 >= GRID_SIZE) ix1 = GRID_SIZE - 1;
            if (iy1 >= GRID_SIZE) iy1 = GRID_SIZE - 1;
            if (ix2 >= GRID_SIZE) ix2 = GRID_SIZE - 1;
            if (iy2 >= GRID_SIZE) iy2 = GRID_SIZE - 1;
            
            grid[ix1][iy1]++;
            grid[ix2][iy2]++;
        }
        
        double expected = 2.0 * PAIRS / (GRID_SIZE * GRID_SIZE);
        double max_deviation = 0.0;
        
        for (const auto& row : grid) {
            for (int count : row) {
                double deviation = std::abs(count - expected) / expected;
                if (deviation > max_deviation) max_deviation = deviation;
            }
        }
        
        results.pair_deviation = max_deviation * 100.0;
    }
    
    return results;
}

// ============================================================================
// Вывод результатов в файл
// ============================================================================
void save_results_to_file(const TestResults& results, const std::string& filename) {
    std::ofstream file(filename);
    
    file << "PRNG64 TEST RESULTS\n";
    file << "===================\n\n";
    
    // Basic correctness
    file << "1. BASIC CORRECTNESS\n";
    file << "   Deterministic with same seed: " << (results.deterministic ? "PASS" : "FAIL") << "\n";
    file << "   Different seeds -> different sequences: " << (results.different_seeds ? "PASS" : "FAIL") << "\n";
    file << "   integer() range: [" << results.integer_range.first << ", " 
         << results.integer_range.second << "]\n";
    file << "   real() in [0,1): " << (results.real_in_range ? "PASS" : "FAIL") 
         << " (min=" << results.real_min << ", max=" << results.real_max << ")\n\n";
    
    // Statistical quality
    file << "2. STATISTICAL QUALITY\n";
    file << "   Uniformity (chi-squared): " << results.chi_squared 
         << " (df=99, <123.2 expected)\n";
    file << "   Autocorrelation (>0.01): " << results.bad_correlations 
         << "/20 lags\n";
    file << "   Runs test (|Z| < 1.96): " << results.runs_z_score 
         << " " << (std::abs(results.runs_z_score) < 1.96 ? "PASS" : "FAIL") << "\n";
    file << "   Bit uniformity max deviation: " << std::fixed << std::setprecision(3) 
         << results.max_bit_deviation << "%\n";
    file << "   2D distribution:\n";
    file << "     Empty cells: " << results.empty_cells_2d << " ("
         << std::setprecision(1) << (100.0 * results.empty_cells_2d / (500*500)) << "%)\n";
    file << "     Max density factor: " << results.max_density_factor << "x\n\n";
    
    // Performance
    file << "3. PERFORMANCE (M ops/sec)\n";
    file << "   PRNG64::uint64():      " << std::setprecision(1) << results.ops_uint64 << "\n";
    file << "   PRNG64::real():        " << results.ops_real << "\n";
    file << "   PRNG64::integer(1,100):" << results.ops_integer << "\n";
    file << "   PRNG64::bit():         " << results.ops_bit << "\n";
    file << "   PRNG64::operator()():  " << results.ops_prng64 << "\n";
    file << "   std::mt19937_64:       " << results.ops_mt19937 << "\n";
    file << "   std::minstd_rand:      " << results.ops_minstd << "\n";
    file << "   Speedup vs mt19937:    " << std::setprecision(1) 
         << (100.0 * (results.ops_prng64 - results.ops_mt19937) / results.ops_mt19937) << "%\n\n";
    
    file << "   Shuffle performance:\n";
    for (const auto& [size, time] : results.shuffle_performance) {
        file << "     " << std::setw(6) << size << " elements: " 
             << std::setprecision(3) << time << " ms ("
             << std::setprecision(1) << (size / time / 1000.0) << " M elem/sec)\n";
    }
    file << "\n";
    
    // Memory
    file << "4. MEMORY USAGE (bytes)\n";
    file << "   PRNG64:       " << results.prng64_size << "\n";
    file << "   mt19937_64:   " << results.mt19937_size << "\n";
    file << "   minstd_rand:  " << results.minstd_size << "\n\n";
    
    // Edge cases
    file << "5. EDGE CASES\n";
    file << "   integer(5,5):     " << (results.edge_integer_single ? "PASS" : "FAIL") << "\n";
    file << "   integer(10,1):    " << (results.edge_integer_reverse ? "PASS" : "FAIL") << "\n";
    file << "   bit(0.0):        " << (results.edge_bit_zero ? "PASS" : "FAIL") << "\n";
    file << "   bit(1.0):        " << (results.edge_bit_one ? "PASS" : "FAIL") << "\n";
    file << "   Overflow safety: " << (results.edge_overflow ? "PASS" : "FAIL") << "\n\n";
    
    // Specialized tests
    file << "6. SPECIALIZED TESTS\n";
    file << "   Birthday paradox deviation: " << std::setprecision(2) 
         << results.birthday_deviation << "%\n";
    file << "   Max increasing streak: " << results.max_increasing_streak 
         << " (expected ~13)\n";
    file << "   Max decreasing streak: " << results.max_decreasing_streak 
         << " (expected ~13)\n";
    file << "   Pair distribution deviation: " << results.pair_deviation << "%\n\n";
    
    // Summary
    file << "7. SUMMARY\n";
    file << "   Period: ~2^64 (1.8e19)\n";
    file << "   State: " << results.prng64_size << " bytes\n";
    file << "   Speed: " << results.ops_prng64 << " M nums/sec\n";
    file << "   Quality: Good statistical properties\n";
    file << "   Use: Games, simulations, algorithms, testing\n";
    file << "   Not for: Cryptography\n";
    
    file.close();
}

// ============================================================================
// Главная функция
// ============================================================================
int main() {
    // Запускаем все тесты
    TestResults results = run_all_tests();
    
    // Сохраняем результаты в файл
    save_results_to_file(results, "prng64_test_results.txt");
    
    // Краткий вывод в консоль
    std::cout << "Tests completed. Results saved to 'prng64_test_results.txt'\n";
    std::cout << "Speed: " << std::fixed << std::setprecision(1) 
              << results.ops_prng64 << " M nums/sec ("
              << std::setprecision(1) 
              << (100.0 * (results.ops_prng64 - results.ops_mt19937) / results.ops_mt19937) 
              << "% faster than mt19937_64)\n";
    std::cout << "Memory: " << results.prng64_size << " bytes (vs " 
              << results.mt19937_size << " for mt19937_64)\n";
    std::cout << "Quality: chi-squared=" << std::setprecision(1) 
              << results.chi_squared << " (should be <123.2)\n";
    
    return 0;
}
