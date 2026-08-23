#pragma once

#include <string>
#include <string_view>
#include <unordered_map>

namespace room_engine {

class ISerializer {
public:
    virtual ~ISerializer() = default;
    virtual void write_string(std::string_view key, std::string_view value) = 0;
};

class IDeserializer {
public:
    virtual ~IDeserializer() = default;
    [[nodiscard]] virtual std::string read_string(std::string_view key,
                                                  std::string_view fallback = {}) const = 0;
};

class MemoryArchive final : public ISerializer, public IDeserializer {
public:
    void write_string(std::string_view key, std::string_view value) override {
        values_[std::string{key}] = value;
    }

    [[nodiscard]] std::string read_string(std::string_view key,
                                          std::string_view fallback = {}) const override {
        const auto it = values_.find(std::string{key});
        return it == values_.end() ? std::string{fallback} : it->second;
    }

private:
    std::unordered_map<std::string, std::string> values_;
};

}  // namespace room_engine
