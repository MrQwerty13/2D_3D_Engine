#pragma once

#include <string_view>

namespace room_engine {

enum class LogLevel { Debug, Info, Warning, Error };

class ILogger {
public:
    virtual ~ILogger() = default;
    virtual void write(LogLevel level, std::string_view message) = 0;
};

class NullLogger final : public ILogger {
public:
    void write(LogLevel, std::string_view) override {}
};

}  // namespace room_engine
