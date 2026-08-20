#include "room_engine/application.hpp"

#include <SDL3/SDL.h>

#include <iostream>
#include <utility>

namespace room_engine {

static_assert(!application_name().empty());

Application::Application(RendererConfig config) : config_(std::move(config)) {}
Application::~Application() { shutdown(); }

bool Application::initialize() {
    if (initialized_) return true;
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        std::cerr << "SDL initialization failed: " << SDL_GetError() << '\n';
        return false;
    }
    window_ = SDL_CreateWindow(config_.name.c_str(), static_cast<int>(config_.width),
                               static_cast<int>(config_.height), SDL_WINDOW_RESIZABLE);
    if (window_ == nullptr) {
        std::cerr << "SDL window creation failed: " << SDL_GetError() << '\n';
        SDL_Quit();
        return false;
    }
    renderer_ = Renderer::create(window_, config_);
    initialized_ = renderer_ != nullptr && renderer_->is_ready();
    if (!initialized_) {
        std::cerr << "Renderer initialization failed. Check the bgfx backend and SDL display.\n";
        renderer_.reset();
        SDL_DestroyWindow(window_);
        window_ = nullptr;
        SDL_Quit();
    }
    running_ = initialized_;
    return initialized_;
}

void Application::poll_events(const std::function<void(const SDL_Event&)>& handler) {
    SDL_Event event{};
    while (SDL_PollEvent(&event)) {
        if (handler) handler(event);
        if (event.type == SDL_EVENT_QUIT) running_ = false;
    }
}

bool Application::begin_frame() {
    return running_ && renderer_ != nullptr && renderer_->begin_frame({24, 28, 36, 255});
}

void Application::end_frame() {
    if (renderer_ != nullptr) renderer_->end_frame();
}

void Application::shutdown() noexcept {
    if (!initialized_) return;
    renderer_.reset();
    if (window_ != nullptr) SDL_DestroyWindow(window_);
    window_ = nullptr;
    SDL_Quit();
    running_ = false;
    initialized_ = false;
}

}  // namespace room_engine
