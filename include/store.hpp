#pragma once

#include <atomic>
#include <chrono>
#include <optional>
#include <string>
#include <unordered_map>
#include <thread>

namespace miniredis {

class Store {
public:
    // ttl == 0 seconds means "no expiry" (same behavior as Milestone 1's SET)
    void set(const std::string& key, const std::string& value,
              std::chrono::seconds ttl = std::chrono::seconds(0));

    std::optional<std::string> get(const std::string& key);
    bool del(const std::string& key);

    size_t len(); // handy for testing active expiry — total live keys right now

    // Launches a background thread that periodically removes expired keys,
    // independent of whether anyone calls get() on them.
    void start_active_expiry(std::chrono::milliseconds interval);
    void stop_active_expiry();

    ~Store();
private:
    struct Entry {
        std::string value;
        std::chrono::steady_clock::time_point expire_at{};
        bool has_ttl = false;
    };

    bool is_expired(const Entry& e) const;
    void sweep_expired();

    std::unordered_map<std::string, Entry> data_;

    std::thread sweeper_;
    std::atomic<bool> sweeper_running_{false};
};

} // namespace miniredis