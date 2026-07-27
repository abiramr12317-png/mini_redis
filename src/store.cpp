#include "store.hpp"

using namespace std;

namespace miniredis {

void Store::set(const string& key, const string& value,
                  chrono::seconds ttl) {
    Entry e;
    e.value = value;
    if (ttl.count() > 0) {
        e.has_ttl = true;
        e.expire_at = chrono::steady_clock::now() + ttl;
    }
    data_[key] = e; // overwrite semantics unchanged from Milestone 1
}

optional<string> Store::get(const string& key) {
    auto it = data_.find(key);
    if (it == data_.end()) return nullopt;

    if(is_expired(it->second)) {
        data_.erase(it);
        return nullopt;
    }

    return it->second.value;
}

bool Store::del(const string& key) {
    auto it = data_.find(key);
    if (it == data_.end()) {
        return false;
    }
    data_.erase(it);
    return true;
}

size_t Store::len() {
    return data_.size();
}

bool Store::is_expired(const Entry& e) const {
    return e.has_ttl && chrono::steady_clock::now() >= e.expire_at;
}  

void Store::sweep_expired() {
    for (auto it = data_.begin(); it != data_.end(); ) {
        if (is_expired(it->second)) {
            it = data_.erase(it);
        } 
        else ++it;
    }
}

void Store::start_active_expiry(chrono::milliseconds interval) {
    sweeper_running_ = true;
    sweeper_ = thread([this, interval]() {
        while (sweeper_running_) {
            this_thread::sleep_for(interval);
            if (!sweeper_running_) break;
            sweep_expired();
        }
    });
}

void Store::stop_active_expiry() {
    if (sweeper_running_.exchange(false)) {
        if (sweeper_.joinable()) sweeper_.join();
    }
}

Store::~Store() {
    stop_active_expiry();
}

} // namespace miniredis