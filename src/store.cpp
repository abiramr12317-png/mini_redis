#include "store.hpp"

namespace miniredis {

void Store::set(const std::string& key, const std::string& value) {
    data_[key] = value;
}

std::optional<std::string> Store::get(const std::string& key) {
    auto it = data_.find(key);
    if (it == data_.end()) {
        return std::nullopt;
    }
    return it->second;
}

bool Store::del(const std::string& key) {
    auto it = data_.find(key);
    if (it == data_.end()) {
        return false;
    }
    data_.erase(it);
    return true;
}

} // namespace miniredis