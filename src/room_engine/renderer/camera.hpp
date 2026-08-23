#pragma once

#include "room_engine/renderer/math.hpp"

#include <algorithm>
#include <cmath>

namespace room_engine {

class Camera {
public:
    virtual ~Camera() = default;
    [[nodiscard]] virtual Mat4 projection() const noexcept = 0;
    [[nodiscard]] Mat4 view_projection() const noexcept { return projection() * view(); }

    Vec3 position{0.0F, 2.0F, 5.0F};
    Vec3 target{};
    Vec3 up{0.0F, 1.0F, 0.0F};
    float aspect_ratio = 16.0F / 9.0F;

protected:
    [[nodiscard]] Mat4 view() const noexcept { return look_at(position, target, up); }
};

class PerspectiveCamera final : public Camera {
public:
    float vertical_fov = 1.04719755F;
    float near_plane = 0.01F;
    float far_plane = 1000.0F;

    [[nodiscard]] Mat4 projection() const noexcept override {
        return perspective(vertical_fov, std::max(aspect_ratio, 0.001F), near_plane, far_plane);
    }
};

class OrthographicCamera final : public Camera {
public:
    float width = 20.0F;
    float height = 20.0F;
    float near_plane = -100.0F;
    float far_plane = 100.0F;

    [[nodiscard]] Mat4 projection() const noexcept override {
        const float half_width = width * 0.5F;
        const float half_height = height * 0.5F;
        return orthographic(-half_width, half_width, -half_height, half_height,
                            near_plane, far_plane);
    }
};

}  // namespace room_engine
