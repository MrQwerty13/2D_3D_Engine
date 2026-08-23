#pragma once

#include "room_engine/renderer/renderer.hpp"

#include <memory>
#include <functional>
#include <string_view>

struct SDL_Window;
union SDL_Event;

namespace room_engine {

[[nodiscard]] constexpr std::string_view application_name() noexcept {
    return "room_engine";
}

class Application {
public:
    explicit Application(RendererConfig config = {});
    ~Application();
    Application(const Application&) = delete;
    Application& operator=(const Application&) = delete;

    [[nodiscard]] bool initialize();
    [[nodiscard]] bool running() const noexcept { return running_; }
    void poll_events(const std::function<void(const SDL_Event&)>& handler = {});
    [[nodiscard]] bool begin_frame();
    void end_frame();
    void shutdown() noexcept;
    [[nodiscard]] Renderer* renderer() noexcept { return renderer_.get(); }
    [[nodiscard]] SDL_Window* window() noexcept { return window_; }

private:
    RendererConfig config_;
    SDL_Window* window_ = nullptr;
    std::unique_ptr<Renderer> renderer_;
    bool running_ = false;
    bool initialized_ = false;
};

}  // namespace room_engine
