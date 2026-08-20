#pragma once

#include <chrono>

namespace room_engine {

struct FrameTime {
    double delta_seconds = 0.0;
    double total_seconds = 0.0;
};

class IClock {
public:
    virtual ~IClock() = default;
    [[nodiscard]] virtual std::chrono::steady_clock::time_point now() const = 0;
};

class SteadyClock final : public IClock {
public:
    [[nodiscard]] std::chrono::steady_clock::time_point now() const override {
        return std::chrono::steady_clock::now();
    }
};

class FrameTimer {
public:
    explicit FrameTimer(const IClock& clock) : clock_(clock), previous_(clock.now()) {}

    [[nodiscard]] FrameTime tick() {
        const auto current = clock_.now();
        const std::chrono::duration<double> delta = current - previous_;
        previous_ = current;
        total_seconds_ += delta.count();
        return {delta.count(), total_seconds_};
    }

private:
    const IClock& clock_;
    std::chrono::steady_clock::time_point previous_;
    double total_seconds_ = 0.0;
};

}  // namespace room_engine
