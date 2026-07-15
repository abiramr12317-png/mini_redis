#pragma once

#include <optional>
#include <string>
#include <unordered_map>

namespace miniredis {

class Store {
public:
    void set(const std::string& key, const std::string& value);
    std::optional<std::string> get(const std::string& key);
    bool del(const std::string& key);

private:
    std::unordered_map<std::string, std::string> data_;
};

} // namespace miniredis