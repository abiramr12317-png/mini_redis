#pragma once

#include <atomic>
#include <chrono>
#include <optional>
#include <string>
#include <unordered_map>
#include <thread>
#include <list>

namespace miniredis {

class Store {
public:
    static constexpr size_t kDefaultMaxKeys = 10000;
    explicit Store(size_t max_keys = kDefaultMaxKeys) : max_keys_(max_keys) {}
    void set(const std::string& key, const std::string& value,
              std::chrono::seconds ttl = std::chrono::seconds(0));

    std::optional<std::string> get(const std::string& key);
    bool del(const std::string& key);

    size_t len(); // handy for testing active expiry — total live keys right now
    
    void set_max_keys(size_t new_max_keys);
    size_t max_keys() const;
    
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
        std::list<std::string>::iterator lru_it;
    };

    bool is_expired(const Entry& e) const;
    void sweep_expired();

    void touch(std::list<std::string>::iterator& lru_it);
    void evict_if_needed();
    bool remove_first_expired_from_lru_tail();

    std::unordered_map<std::string, Entry> data_;
    std::list<std::string> lru_;
    size_t max_keys_;

    std::thread sweeper_;
    std::atomic<bool> sweeper_running_{false};
};

} // namespace miniredis