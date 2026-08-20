#pragma once

#include "room_engine/core/transform.hpp"

#include <array>
#include <cmath>

namespace room_engine {

struct Mat4 {
    std::array<float, 16> value{};

    [[nodiscard]] static constexpr Mat4 identity() noexcept {
        return {{{1.0F, 0.0F, 0.0F, 0.0F, 0.0F, 1.0F, 0.0F, 0.0F,
                  0.0F, 0.0F, 1.0F, 0.0F, 0.0F, 0.0F, 0.0F, 1.0F}}};
    }

    [[nodiscard]] const float* data() const noexcept { return value.data(); }

    [[nodiscard]] Mat4 operator*(const Mat4& rhs) const noexcept {
        Mat4 result{};
        for (int column = 0; column < 4; ++column) {
            for (int row = 0; row < 4; ++row) {
                for (int i = 0; i < 4; ++i) {
                    result.value[static_cast<std::size_t>(column * 4 + row)] +=
                        value[static_cast<std::size_t>(i * 4 + row)] *
                        rhs.value[static_cast<std::size_t>(column * 4 + i)];
                }
            }
        }
        return result;
    }
};

[[nodiscard]] inline Mat4 translation(Vec3 position) noexcept {
    Mat4 result = Mat4::identity();
    result.value[12] = position.x;
    result.value[13] = position.y;
    result.value[14] = position.z;
    return result;
}

[[nodiscard]] inline Mat4 look_at(Vec3 eye, Vec3 target, Vec3 up) noexcept {
    const auto normalize = [](Vec3 v) {
        const float length = std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z);
        return length > 0.0F ? v * (1.0F / length) : Vec3{};
    };
    const Vec3 forward = normalize(target - eye);
    const Vec3 side = normalize({forward.y * up.z - forward.z * up.y,
                                 forward.z * up.x - forward.x * up.z,
                                 forward.x * up.y - forward.y * up.x});
    const Vec3 corrected_up{side.y * forward.z - side.z * forward.y,
                            side.z * forward.x - side.x * forward.z,
                            side.x * forward.y - side.y * forward.x};
    Mat4 result = Mat4::identity();
    result.value[0] = side.x;
    result.value[1] = corrected_up.x;
    result.value[2] = -forward.x;
    result.value[4] = side.y;
    result.value[5] = corrected_up.y;
    result.value[6] = -forward.y;
    result.value[8] = side.z;
    result.value[9] = corrected_up.z;
    result.value[10] = -forward.z;
    result.value[12] = -(side.x * eye.x + side.y * eye.y + side.z * eye.z);
    result.value[13] = -(corrected_up.x * eye.x + corrected_up.y * eye.y + corrected_up.z * eye.z);
    result.value[14] = forward.x * eye.x + forward.y * eye.y + forward.z * eye.z;
    return result;
}

[[nodiscard]] inline Mat4 perspective(float fov_y, float aspect, float near_plane,
                                      float far_plane) noexcept {
    const float tan_half_fov = std::tan(fov_y * 0.5F);
    Mat4 result{};
    result.value[0] = 1.0F / (aspect * tan_half_fov);
    result.value[5] = 1.0F / tan_half_fov;
    result.value[10] = -(far_plane + near_plane) / (far_plane - near_plane);
    result.value[11] = -1.0F;
    result.value[14] = -(2.0F * far_plane * near_plane) / (far_plane - near_plane);
    return result;
}

[[nodiscard]] inline Mat4 orthographic(float left, float right, float bottom, float top,
                                       float near_plane, float far_plane) noexcept {
    Mat4 result = Mat4::identity();
    result.value[0] = 2.0F / (right - left);
    result.value[5] = 2.0F / (top - bottom);
    result.value[10] = -2.0F / (far_plane - near_plane);
    result.value[12] = -(right + left) / (right - left);
    result.value[13] = -(top + bottom) / (top - bottom);
    result.value[14] = -(far_plane + near_plane) / (far_plane - near_plane);
    return result;
}

}  // namespace room_engine
