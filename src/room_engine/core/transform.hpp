#pragma once

#include <cmath>

namespace room_engine {

struct Vec3 {
    float x = 0.0F;
    float y = 0.0F;
    float z = 0.0F;

    constexpr Vec3 operator+(const Vec3& other) const noexcept {
        return {x + other.x, y + other.y, z + other.z};
    }
    constexpr Vec3 operator-(const Vec3& other) const noexcept {
        return {x - other.x, y - other.y, z - other.z};
    }
    constexpr Vec3 operator*(float scalar) const noexcept {
        return {x * scalar, y * scalar, z * scalar};
    }
};

struct Quaternion {
    float w = 1.0F;
    float x = 0.0F;
    float y = 0.0F;
    float z = 0.0F;

    [[nodiscard]] static Quaternion from_axis_angle(Vec3 axis, float radians) noexcept {
        const float half = radians * 0.5F;
        const float sine = std::sin(half);
        return {std::cos(half), axis.x * sine, axis.y * sine, axis.z * sine};
    }

    [[nodiscard]] Quaternion conjugate() const noexcept { return {w, -x, -y, -z}; }

    [[nodiscard]] Quaternion operator*(const Quaternion& other) const noexcept {
        return {w * other.w - x * other.x - y * other.y - z * other.z,
                w * other.x + x * other.w + y * other.z - z * other.y,
                w * other.y - x * other.z + y * other.w + z * other.x,
                w * other.z + x * other.y - y * other.x + z * other.w};
    }

    [[nodiscard]] Vec3 rotate(Vec3 vector) const noexcept {
        const Quaternion point{0.0F, vector.x, vector.y, vector.z};
        const Quaternion rotated = (*this) * point * conjugate();
        return {rotated.x, rotated.y, rotated.z};
    }
};

struct Transform {
    Vec3 position{};
    Quaternion rotation{};
    Vec3 scale{1.0F, 1.0F, 1.0F};

    [[nodiscard]] Vec3 transform_point(Vec3 point) const noexcept {
        return rotation.rotate({point.x * scale.x, point.y * scale.y, point.z * scale.z}) + position;
    }

    [[nodiscard]] Transform combine(const Transform& child) const noexcept {
        return {transform_point(child.position), rotation * child.rotation,
                {scale.x * child.scale.x, scale.y * child.scale.y, scale.z * child.scale.z}};
    }
};

}  // namespace room_engine
