// Stress / scale test — not a correctness unit test, a volume test.
//
// Answers: "does this still work, and how fast is it, with a LOT of keys?"
// Run this separately from the doctest suite; it's meant to be read by a
// human (throughput numbers), not asserted pass/fail in CI.
#include <algorithm>
#include <chrono>
#include <iostream>
#include <random>
#include <string>
#include <vector>

#include "store.hpp"

using Clock = std::chrono::steady_clock;

// Generates a pseudo-random alphanumeric string of a given length.
// Deterministic seed so runs are reproducible (important when you're
// trying to compare "before" vs "after" a code change).
std::string random_string(std::mt19937& rng, int len) {
    static const char charset[] =
        "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
    std::uniform_int_distribution<int> dist(0, sizeof(charset) - 2);
    std::string s(len, '\0');
    for (auto& c : s) c = charset[dist(rng)];
    return s;
}

int main(int argc, char** argv) {
    int N = 1'000'000; // default scale — override via argv[1]
    if (argc > 1) N = std::stoi(argv[1]);

    std::cout << "Stress-testing Store with N = " << N << " keys\n\n";

    miniredis::Store store;
    std::mt19937 rng(42); // fixed seed: reproducible runs

    // Pre-generate keys/values so we're timing the store, not the RNG.
    std::vector<std::string> keys;
    std::vector<std::string> values;
    keys.reserve(N);
    values.reserve(N);
    for (int i = 0; i < N; i++) {
        keys.push_back("key:" + std::to_string(i));
        values.push_back(random_string(rng, 20));
    }

    // --- SET benchmark ---
    auto t0 = Clock::now();
    for (int i = 0; i < N; i++) {
        store.set(keys[i], values[i]);
    }
    auto t1 = Clock::now();
    double set_secs = std::chrono::duration<double>(t1 - t0).count();
    std::cout << "SET: " << N << " ops in " << set_secs << "s  ("
              << static_cast<double>(N) / set_secs << " ops/sec)\n";

    // --- GET benchmark (shuffled access order — avoids favoring
    //     sequential-access cache patterns, closer to real-world lookups) ---
    std::vector<int> order(N);
    for (int i = 0; i < N; i++) order[i] = i;
    std::shuffle(order.begin(), order.end(), rng);

    auto t2 = Clock::now();
    int hits = 0;
    for (int idx : order) {
        auto result = store.get(keys[idx]);
        if (result.has_value()) hits++;
    }
    auto t3 = Clock::now();
    double get_secs = std::chrono::duration<double>(t3 - t2).count();
    std::cout << "GET: " << N << " ops in " << get_secs << "s  ("
              << static_cast<double>(N) / get_secs << " ops/sec)\n";

    // --- Correctness check at scale ---
    // A performance number is meaningless if the data is wrong. Verify
    // every value we just read back actually matches what we stored.
    int mismatches = 0;
    for (int i = 0; i < N; i++) {
        auto result = store.get(keys[i]);
        if (!result.has_value() || *result != values[i]) mismatches++;
    }
    std::cout << "Correctness: " << (N - mismatches) << "/" << N
              << " values correct";
    if (mismatches > 0) {
        std::cout << "  *** " << mismatches << " MISMATCHES FOUND ***";
    }
    std::cout << "\n\n";

    // --- DEL benchmark ---
    auto t4 = Clock::now();
    int deleted = 0;
    for (int i = 0; i < N; i++) {
        if (store.del(keys[i])) deleted++;
    }
    auto t5 = Clock::now();
    double del_secs = std::chrono::duration<double>(t5 - t4).count();
    std::cout << "DEL: " << N << " ops in " << del_secs << "s  ("
              << static_cast<double>(N) / del_secs << " ops/sec)\n";
    std::cout << "Deleted " << deleted << "/" << N << " keys (expect all)\n";

    if (mismatches > 0) {
        std::cerr << "\nFAILED: correctness mismatches found under load.\n";
        return 1;
    }
    std::cout << "\nAll " << N << " keys verified correct under load.\n";
    return 0;
}