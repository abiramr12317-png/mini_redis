#include "store.hpp"

using namespace std;

namespace miniredis {

void Store::set(const string& key, const string& value,
                  chrono::seconds ttl) {
    auto it = data_.find(key);

    if (it != data_.end()) {
        it->second.value = value;
        if (ttl.count() > 0) {
            it->second.has_ttl = true;
            it->second.expire_at = chrono::steady_clock::now() + ttl;
        } else {
            it->second.has_ttl = false;
        }
        touch(it->second.lru_it);
        return;
    }

    lru_.push_front(key);
    Entry e;
    e.value = value;
    e.lru_it = lru_.begin();
    if (ttl.count() > 0) {
        e.has_ttl = true;
        e.expire_at = chrono::steady_clock::now() + ttl;
    }
    data_.emplace(key, move(e));

    evict_if_needed();
}

optional<string> Store::get(const string& key) {
    auto it = data_.find(key);
    if (it == data_.end()) return nullopt;

    if(is_expired(it->second)) {
        lru_.erase(it->second.lru_it);
        data_.erase(it);
        return nullopt;
    }

    touch(it->second.lru_it);
    return it->second.value;
}

bool Store::del(const string& key) {
    auto it = data_.find(key);
    if (it == data_.end()) {
        return false;
    }
    lru_.erase(it->second.lru_it);
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
            lru_.erase(it->second.lru_it);
            it = data_.erase(it);
        } 
        else ++it;
    }
}

void Store::touch(list<string>::iterator& lru_it) {
    lru_.splice(lru_.begin(), lru_, lru_it);
}

bool Store::remove_first_expired_from_lru_tail() {
    for (auto it = lru_.rbegin(); it != lru_.rend(); ++it) {
        auto data_it = data_.find(*it);
        if (data_it != data_.end() && is_expired(data_it->second)) {
            lru_.erase(data_it->second.lru_it);
            data_.erase(data_it);
            return true;
        }
    }
    return false;
}

void Store::evict_if_needed() {
    if (max_keys_ == 0) return;

    while (data_.size() > max_keys_) {
        if (remove_first_expired_from_lru_tail()) {
            continue;
        }
        if (lru_.empty()) return;

        const string victim = lru_.back();
        auto data_it = data_.find(victim);
        lru_.erase(data_it->second.lru_it);
        data_.erase(data_it);
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