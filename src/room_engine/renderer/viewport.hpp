#pragma once

#include "room_engine/renderer/camera.hpp"

#include <algorithm>
#include <cmath>

namespace room_engine {

struct ScreenPoint { float x = 0.0F; float y = 0.0F; };
struct ViewportRect { float x = 0.0F; float y = 0.0F; float width = 1.0F; float height = 1.0F; };

class Viewport2D {
public:
    explicit Viewport2D(float width = 1280.0F, float height = 720.0F) : rect_{0, 0, width, height} {
        update_camera();
    }

    void set_rect(ViewportRect rect) noexcept { rect_ = rect; update_camera(); }
    [[nodiscard]] ViewportRect rect() const noexcept { return rect_; }
    [[nodiscard]] OrthographicCamera& camera() noexcept { return camera_; }
    [[nodiscard]] const OrthographicCamera& camera() const noexcept { return camera_; }
    void set_center(Vec2 center) noexcept { center_ = center; update_camera(); }
    [[nodiscard]] Vec2 center() const noexcept { return center_; }
    void pan(Vec2 screen_delta) noexcept {
        const float scale = units_per_pixel();
        center_.x -= screen_delta.x * scale;
        center_.y += screen_delta.y * scale;
        update_camera();
    }
    void zoom(float factor, ScreenPoint anchor = {}) noexcept {
        if (factor <= 0.0F) return;
        const Vec2 before = screen_to_world(anchor);
        zoom_ = std::clamp(zoom_ * factor, 0.01F, 1000.0F);
        update_camera();
        const Vec2 after = screen_to_world(anchor);
        center_.x += before.x - after.x;
        center_.y += before.y - after.y;
        update_camera();
    }
    [[nodiscard]] float zoom_level() const noexcept { return zoom_; }
    [[nodiscard]] float units_per_pixel() const noexcept {
        return camera_.width / std::max(rect_.width, 1.0F);
    }
    [[nodiscard]] Vec2 screen_to_world(ScreenPoint screen) const noexcept {
        const float nx = (screen.x - rect_.x) / std::max(rect_.width, 1.0F);
        const float ny = (screen.y - rect_.y) / std::max(rect_.height, 1.0F);
        return {center_.x + (nx - 0.5F) * camera_.width,
                center_.y + (0.5F - ny) * camera_.height};
    }
    [[nodiscard]] ScreenPoint world_to_screen(Vec2 world) const noexcept {
        return {rect_.x + (world.x - center_.x) / camera_.width * rect_.width + rect_.width * 0.5F,
                rect_.y + (center_.y - world.y) / camera_.height * rect_.height + rect_.height * 0.5F};
    }

private:
    void update_camera() noexcept {
        const float aspect = std::max(rect_.width, 1.0F) / std::max(rect_.height, 1.0F);
        camera_.aspect_ratio = aspect;
        camera_.width = 20.0F / zoom_;
        camera_.height = camera_.width / aspect;
        camera_.position = {center_.x, center_.y, 10.0F};
        camera_.target = {center_.x, center_.y, 0.0F};
    }

    ViewportRect rect_{};
    Vec2 center_{};
    float zoom_ = 1.0F;
    OrthographicCamera camera_{};
};

}  // namespace room_engine
