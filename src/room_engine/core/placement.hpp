#pragma once

#include "room_engine/core/room_design.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <limits>
#include <span>
#include <vector>

namespace room_engine {

// Point2::y is the world-space Z coordinate throughout the placement helpers.
using FurnitureFootprint = std::array<Point2, 4>;

inline constexpr float placement_geometry_tolerance = 1.0e-5F;

[[nodiscard]] inline bool is_finite_point(Point2 point) noexcept {
    return std::isfinite(point.x) && std::isfinite(point.y);
}

[[nodiscard]] inline bool is_finite_vector(Vec3 value) noexcept {
    return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z);
}

[[nodiscard]] inline bool is_finite_quaternion(Quaternion value) noexcept {
    return std::isfinite(value.w) && std::isfinite(value.x) && std::isfinite(value.y) &&
           std::isfinite(value.z);
}

[[nodiscard]] inline bool is_finite_transform(const Transform& transform) noexcept {
    return is_finite_vector(transform.position) && is_finite_quaternion(transform.rotation) &&
           is_finite_vector(transform.scale);
}

[[nodiscard]] inline bool has_positive_transform_scale(const Transform& transform) noexcept {
    return is_finite_vector(transform.scale) && transform.scale.x > 0.0F &&
           transform.scale.y > 0.0F && transform.scale.z > 0.0F;
}

namespace placement_detail {

[[nodiscard]] inline double quaternion_norm_squared(Quaternion value) noexcept {
    const double w = static_cast<double>(value.w);
    const double x = static_cast<double>(value.x);
    const double y = static_cast<double>(value.y);
    const double z = static_cast<double>(value.z);
    return w * w + x * x + y * y + z * z;
}

[[nodiscard]] inline bool positive_dimensions(Vec3 dimensions) noexcept {
    return is_finite_vector(dimensions) && dimensions.x > 0.0F && dimensions.y > 0.0F &&
           dimensions.z > 0.0F;
}

[[nodiscard]] inline bool fits_in_float(double value) noexcept {
    constexpr double limit = static_cast<double>(std::numeric_limits<float>::max());
    return std::isfinite(value) && value >= -limit && value <= limit;
}

[[nodiscard]] inline bool fits_in_positive_float(double value) noexcept {
    constexpr double minimum = static_cast<double>(std::numeric_limits<float>::denorm_min());
    return fits_in_float(value) && value >= minimum;
}

[[nodiscard]] inline bool valid_tolerance(float tolerance) noexcept {
    return std::isfinite(tolerance) && tolerance >= 0.0F;
}

[[nodiscard]] inline double cross(Point2 a, Point2 b, Point2 c) noexcept {
    const double ab_x = static_cast<double>(b.x) - static_cast<double>(a.x);
    const double ab_y = static_cast<double>(b.y) - static_cast<double>(a.y);
    const double ac_x = static_cast<double>(c.x) - static_cast<double>(a.x);
    const double ac_y = static_cast<double>(c.y) - static_cast<double>(a.y);
    return ab_x * ac_y - ab_y * ac_x;
}

[[nodiscard]] inline double squared_distance(Point2 a, Point2 b) noexcept {
    const double x = static_cast<double>(b.x) - static_cast<double>(a.x);
    const double y = static_cast<double>(b.y) - static_cast<double>(a.y);
    return x * x + y * y;
}

[[nodiscard]] inline bool point_on_segment(Point2 point, Point2 start, Point2 end,
                                           double tolerance) noexcept {
    const double edge_x = static_cast<double>(end.x) - static_cast<double>(start.x);
    const double edge_y = static_cast<double>(end.y) - static_cast<double>(start.y);
    const double length_squared = edge_x * edge_x + edge_y * edge_y;
    if (!(length_squared > 0.0) || !std::isfinite(length_squared)) return false;

    const double edge_length = std::sqrt(length_squared);
    if (std::fabs(cross(start, end, point)) > tolerance * edge_length) return false;

    const double point_x = static_cast<double>(point.x);
    const double point_y = static_cast<double>(point.y);
    return point_x >= std::min(static_cast<double>(start.x), static_cast<double>(end.x)) -
                          tolerance &&
           point_x <= std::max(static_cast<double>(start.x), static_cast<double>(end.x)) +
                          tolerance &&
           point_y >= std::min(static_cast<double>(start.y), static_cast<double>(end.y)) -
                          tolerance &&
           point_y <= std::max(static_cast<double>(start.y), static_cast<double>(end.y)) +
                          tolerance;
}

[[nodiscard]] inline int orientation(Point2 a, Point2 b, Point2 c,
                                     double tolerance) noexcept {
    const double length = std::sqrt(squared_distance(a, b));
    const double value = cross(a, b, c);
    const double threshold = tolerance * length;
    if (value > threshold) return 1;
    if (value < -threshold) return -1;
    return 0;
}

[[nodiscard]] inline bool segments_intersect(Point2 a, Point2 b, Point2 c, Point2 d,
                                             double tolerance) noexcept {
    const int abc = orientation(a, b, c, tolerance);
    const int abd = orientation(a, b, d, tolerance);
    const int cda = orientation(c, d, a, tolerance);
    const int cdb = orientation(c, d, b, tolerance);
    if (abc != 0 && abd != 0 && cda != 0 && cdb != 0 && abc != abd && cda != cdb)
        return true;
    return (abc == 0 && point_on_segment(c, a, b, tolerance)) ||
           (abd == 0 && point_on_segment(d, a, b, tolerance)) ||
           (cda == 0 && point_on_segment(a, c, d, tolerance)) ||
           (cdb == 0 && point_on_segment(b, c, d, tolerance));
}

[[nodiscard]] inline bool valid_polygon(std::span<const Point2> polygon,
                                        double tolerance) noexcept {
    if (polygon.size() < 3) return false;

    double perimeter = 0.0;
    double twice_area = 0.0;
    for (std::size_t index = 0; index < polygon.size(); ++index) {
        const Point2 current = polygon[index];
        const Point2 next = polygon[(index + 1) % polygon.size()];
        if (!is_finite_point(current)) return false;
        const double edge_length = std::sqrt(squared_distance(current, next));
        if (!(edge_length > tolerance) || !std::isfinite(edge_length)) return false;
        perimeter += edge_length;
        twice_area += static_cast<double>(current.x) * static_cast<double>(next.y) -
                      static_cast<double>(next.x) * static_cast<double>(current.y);
    }
    if (!std::isfinite(perimeter) || !std::isfinite(twice_area) ||
        std::fabs(twice_area) <= tolerance * perimeter)
        return false;

    for (std::size_t first = 0; first < polygon.size(); ++first) {
        const std::size_t first_next = (first + 1) % polygon.size();
        for (std::size_t second = first + 1; second < polygon.size(); ++second) {
            const std::size_t second_next = (second + 1) % polygon.size();
            if (first == second || first_next == second || second_next == first) continue;
            if (segments_intersect(polygon[first], polygon[first_next], polygon[second],
                                   polygon[second_next], tolerance))
                return false;
        }
    }
    return true;
}

[[nodiscard]] inline bool point_in_or_on_polygon_unchecked(
    Point2 point, std::span<const Point2> polygon, double tolerance) noexcept {
    bool inside = false;
    for (std::size_t index = 0; index < polygon.size(); ++index) {
        const Point2 start = polygon[index];
        const Point2 end = polygon[(index + 1) % polygon.size()];
        if (point_on_segment(point, start, end, tolerance)) return true;

        const double start_y = static_cast<double>(start.y);
        const double end_y = static_cast<double>(end.y);
        const double point_y = static_cast<double>(point.y);
        if ((start_y > point_y) == (end_y > point_y)) continue;
        const double intersection_x =
            static_cast<double>(start.x) +
            (point_y - start_y) *
                (static_cast<double>(end.x) - static_cast<double>(start.x)) /
                (end_y - start_y);
        if (static_cast<double>(point.x) < intersection_x) inside = !inside;
    }
    return inside;
}

[[nodiscard]] inline bool valid_footprint(const FurnitureFootprint& footprint) noexcept {
    int turn = 0;
    for (std::size_t index = 0; index < footprint.size(); ++index) {
        if (!is_finite_point(footprint[index]) ||
            !(squared_distance(footprint[index],
                               footprint[(index + 1) % footprint.size()]) > 0.0))
            return false;
        const double corner_cross =
            cross(footprint[index], footprint[(index + 1) % footprint.size()],
                  footprint[(index + 2) % footprint.size()]);
        if (corner_cross == 0.0 || !std::isfinite(corner_cross)) return false;
        const int current_turn = corner_cross > 0.0 ? 1 : -1;
        if (turn != 0 && current_turn != turn) return false;
        turn = current_turn;
    }
    return true;
}

[[nodiscard]] inline bool make_furniture_footprint(const Furniture& furniture,
                                                   FurnitureFootprint& result) noexcept;

inline void add_segment_intersections(Point2 start, Point2 end, Point2 boundary_start,
                                      Point2 boundary_end, double tolerance,
                                      std::vector<double>& parameters) {
    const double ray_x = static_cast<double>(end.x) - static_cast<double>(start.x);
    const double ray_y = static_cast<double>(end.y) - static_cast<double>(start.y);
    const double edge_x =
        static_cast<double>(boundary_end.x) - static_cast<double>(boundary_start.x);
    const double edge_y =
        static_cast<double>(boundary_end.y) - static_cast<double>(boundary_start.y);
    const double offset_x = static_cast<double>(boundary_start.x) -
                            static_cast<double>(start.x);
    const double offset_y = static_cast<double>(boundary_start.y) -
                            static_cast<double>(start.y);
    const double denominator = ray_x * edge_y - ray_y * edge_x;
    const double ray_length_squared = ray_x * ray_x + ray_y * ray_y;
    const double ray_length = std::sqrt(ray_length_squared);
    const double edge_length = std::hypot(edge_x, edge_y);
    const double parameter_tolerance = tolerance / ray_length;
    const double edge_parameter_tolerance = tolerance / edge_length;

    if (denominator != 0.0) {
        const double ray_parameter = (offset_x * edge_y - offset_y * edge_x) / denominator;
        const double edge_parameter = (offset_x * ray_y - offset_y * ray_x) / denominator;
        if (ray_parameter >= -parameter_tolerance &&
            ray_parameter <= 1.0 + parameter_tolerance &&
            edge_parameter >= -edge_parameter_tolerance &&
            edge_parameter <= 1.0 + edge_parameter_tolerance)
            parameters.push_back(std::clamp(ray_parameter, 0.0, 1.0));
        return;
    }

    if (!point_on_segment(boundary_start, start, end, tolerance) &&
        !point_on_segment(boundary_end, start, end, tolerance))
        return;
    const double start_parameter = (offset_x * ray_x + offset_y * ray_y) / ray_length_squared;
    const double end_offset_x = static_cast<double>(boundary_end.x) -
                                static_cast<double>(start.x);
    const double end_offset_y = static_cast<double>(boundary_end.y) -
                                static_cast<double>(start.y);
    const double end_parameter =
        (end_offset_x * ray_x + end_offset_y * ray_y) / ray_length_squared;
    if (start_parameter >= -parameter_tolerance && start_parameter <= 1.0 + parameter_tolerance)
        parameters.push_back(std::clamp(start_parameter, 0.0, 1.0));
    if (end_parameter >= -parameter_tolerance && end_parameter <= 1.0 + parameter_tolerance)
        parameters.push_back(std::clamp(end_parameter, 0.0, 1.0));
}

[[nodiscard]] inline bool segment_inside_polygon(Point2 start, Point2 end,
                                                 std::span<const Point2> polygon,
                                                 double tolerance) {
    const double length = std::sqrt(squared_distance(start, end));
    if (!(length > 0.0) || !std::isfinite(length)) return false;

    std::vector<double> parameters{0.0, 1.0};
    parameters.reserve(polygon.size() + 2);
    for (std::size_t index = 0; index < polygon.size(); ++index) {
        add_segment_intersections(start, end, polygon[index],
                                  polygon[(index + 1) % polygon.size()], tolerance, parameters);
    }
    std::sort(parameters.begin(), parameters.end());
    const double parameter_tolerance = tolerance / length;
    parameters.erase(std::unique(parameters.begin(), parameters.end(),
                                 [parameter_tolerance](double first, double second) {
                                     return std::fabs(first - second) <= parameter_tolerance;
                                 }),
                     parameters.end());

    for (std::size_t index = 0; index + 1 < parameters.size(); ++index) {
        if (parameters[index + 1] - parameters[index] <= parameter_tolerance) continue;
        const double middle = (parameters[index] + parameters[index + 1]) * 0.5;
        const double x = static_cast<double>(start.x) +
                         (static_cast<double>(end.x) - static_cast<double>(start.x)) * middle;
        const double y = static_cast<double>(start.y) +
                         (static_cast<double>(end.y) - static_cast<double>(start.y)) * middle;
        if (!fits_in_float(x) || !fits_in_float(y) ||
            !point_in_or_on_polygon_unchecked(
                {static_cast<float>(x), static_cast<float>(y)}, polygon, tolerance))
            return false;
    }
    return true;
}

}  // namespace placement_detail

[[nodiscard]] inline bool is_valid_placement_transform(const Transform& transform) noexcept {
    return is_finite_transform(transform) && has_positive_transform_scale(transform) &&
           placement_detail::quaternion_norm_squared(transform.rotation) > 0.0;
}

// Returns {0, 0, 0} when the dimensions or placement transform are invalid, or when a
// multiplication cannot be represented by Vec3's float components.
[[nodiscard]] inline Vec3 scaled_furniture_dimensions(const Furniture& furniture) noexcept {
    if (!is_valid_placement_transform(furniture.transform) ||
        !placement_detail::positive_dimensions(furniture.dimensions))
        return {};

    const double x = static_cast<double>(furniture.dimensions.x) *
                     static_cast<double>(furniture.transform.scale.x);
    const double y = static_cast<double>(furniture.dimensions.y) *
                     static_cast<double>(furniture.transform.scale.y);
    const double z = static_cast<double>(furniture.dimensions.z) *
                     static_cast<double>(furniture.transform.scale.z);
    if (!placement_detail::fits_in_positive_float(x) ||
        !placement_detail::fits_in_positive_float(y) ||
        !placement_detail::fits_in_positive_float(z))
        return {};
    return {static_cast<float>(x), static_cast<float>(y), static_cast<float>(z)};
}

// The quaternion is normalized before conversion. A zero or non-finite quaternion returns 0.
// Positive yaw follows Quaternion::from_axis_angle({0, 1, 0}, yaw).
[[nodiscard]] inline float quaternion_y_axis_yaw(Quaternion rotation) noexcept {
    if (!is_finite_quaternion(rotation)) return 0.0F;
    const double norm_squared = placement_detail::quaternion_norm_squared(rotation);
    if (!(norm_squared > 0.0) || !std::isfinite(norm_squared)) return 0.0F;

    const double inverse_norm = 1.0 / std::sqrt(norm_squared);
    const double w = static_cast<double>(rotation.w) * inverse_norm;
    const double x = static_cast<double>(rotation.x) * inverse_norm;
    const double y = static_cast<double>(rotation.y) * inverse_norm;
    const double z = static_cast<double>(rotation.z) * inverse_norm;
    const double sine = 2.0 * (w * y + x * z);
    const double cosine = w * w + x * x - y * y - z * z;
    const double yaw = std::atan2(sine, cosine);
    return std::isfinite(yaw) ? static_cast<float>(yaw) : 0.0F;
}

namespace placement_detail {

[[nodiscard]] inline bool make_furniture_footprint(const Furniture& furniture,
                                                   FurnitureFootprint& result) noexcept {
    result = {};
    const Vec3 dimensions = scaled_furniture_dimensions(furniture);
    if (!positive_dimensions(dimensions)) return false;

    const double yaw = static_cast<double>(quaternion_y_axis_yaw(furniture.transform.rotation));
    const double cosine = std::cos(yaw);
    const double sine = std::sin(yaw);
    const double half_x = static_cast<double>(dimensions.x) * 0.5;
    const double half_z = static_cast<double>(dimensions.z) * 0.5;
    const std::array<Point2, 4> local{{
        {-static_cast<float>(half_x), -static_cast<float>(half_z)},
        {static_cast<float>(half_x), -static_cast<float>(half_z)},
        {static_cast<float>(half_x), static_cast<float>(half_z)},
        {-static_cast<float>(half_x), static_cast<float>(half_z)},
    }};

    for (std::size_t index = 0; index < result.size(); ++index) {
        // Quaternion's positive Y rotation maps local +X toward world -Z.
        const double x = static_cast<double>(furniture.transform.position.x) +
                         static_cast<double>(local[index].x) * cosine +
                         static_cast<double>(local[index].y) * sine;
        const double z = static_cast<double>(furniture.transform.position.z) -
                         static_cast<double>(local[index].x) * sine +
                         static_cast<double>(local[index].y) * cosine;
        if (!fits_in_float(x) || !fits_in_float(z)) {
            result = {};
            return false;
        }
        result[index] = {static_cast<float>(x), static_cast<float>(z)};
    }
    if (!valid_footprint(result)) {
        result = {};
        return false;
    }
    return true;
}

}  // namespace placement_detail

// Corners are counter-clockwise in X/Z when viewed from above. Invalid input produces a
// degenerate all-zero footprint; geometry predicates below reject that sentinel.
[[nodiscard]] inline FurnitureFootprint
furniture_footprint_corners(const Furniture& furniture) noexcept {
    FurnitureFootprint result{};
    static_cast<void>(placement_detail::make_furniture_footprint(furniture, result));
    return result;
}

// Separating Axis Theorem test for ordered, convex rectangle corners. By default, two
// rectangles that only touch at an edge or corner do not overlap.
[[nodiscard]] inline bool oriented_rectangles_overlap(
    const FurnitureFootprint& first, const FurnitureFootprint& second,
    bool boundary_contact_is_overlap = false) noexcept {
    if (!placement_detail::valid_footprint(first) ||
        !placement_detail::valid_footprint(second))
        return false;

    const auto separated_on_axes = [&](const FurnitureFootprint& axes_from) {
        for (std::size_t edge = 0; edge < axes_from.size(); ++edge) {
            const Point2 start = axes_from[edge];
            const Point2 end = axes_from[(edge + 1) % axes_from.size()];
            const double edge_x = static_cast<double>(end.x) - static_cast<double>(start.x);
            const double edge_y = static_cast<double>(end.y) - static_cast<double>(start.y);
            const double length = std::hypot(edge_x, edge_y);
            if (!(length > 0.0) || !std::isfinite(length)) return true;
            const double axis_x = -edge_y / length;
            const double axis_y = edge_x / length;

            const auto project = [&](const FurnitureFootprint& footprint) {
                double minimum = static_cast<double>(footprint.front().x) * axis_x +
                                 static_cast<double>(footprint.front().y) * axis_y;
                double maximum = minimum;
                for (std::size_t index = 1; index < footprint.size(); ++index) {
                    const double value = static_cast<double>(footprint[index].x) * axis_x +
                                         static_cast<double>(footprint[index].y) * axis_y;
                    minimum = std::min(minimum, value);
                    maximum = std::max(maximum, value);
                }
                return std::array<double, 2>{minimum, maximum};
            };

            const auto first_projection = project(first);
            const auto second_projection = project(second);
            if (boundary_contact_is_overlap) {
                if (first_projection[1] < second_projection[0] ||
                    second_projection[1] < first_projection[0])
                    return true;
            } else if (first_projection[1] <= second_projection[0] ||
                       second_projection[1] <= first_projection[0]) {
                return true;
            }
        }
        return false;
    };

    return !separated_on_axes(first) && !separated_on_axes(second);
}

[[nodiscard]] inline bool furniture_footprints_overlap(
    const Furniture& first, const Furniture& second,
    bool boundary_contact_is_overlap = false) noexcept {
    FurnitureFootprint first_footprint{};
    FurnitureFootprint second_footprint{};
    if (!placement_detail::make_furniture_footprint(first, first_footprint) ||
        !placement_detail::make_furniture_footprint(second, second_footprint))
        return false;
    return oriented_rectangles_overlap(first_footprint, second_footprint,
                                       boundary_contact_is_overlap);
}

// Accepts concave simple polygons in either winding order and treats their boundary as inside.
[[nodiscard]] inline bool point_in_or_on_polygon(
    Point2 point, std::span<const Point2> polygon,
    float tolerance = placement_geometry_tolerance) noexcept {
    if (!is_finite_point(point) || !placement_detail::valid_tolerance(tolerance)) return false;
    const double checked_tolerance = static_cast<double>(tolerance);
    return placement_detail::valid_polygon(polygon, checked_tolerance) &&
           placement_detail::point_in_or_on_polygon_unchecked(point, polygon,
                                                               checked_tolerance);
}

// This checks every footprint edge, not only the four corners, so concave floor boundaries are
// handled correctly. Contact with the floor boundary is allowed.
[[nodiscard]] inline bool furniture_footprint_inside_floor(
    const Furniture& furniture, const Floor& floor,
    float tolerance = placement_geometry_tolerance) {
    if (!placement_detail::valid_tolerance(tolerance)) return false;
    const double checked_tolerance = static_cast<double>(tolerance);
    const std::span<const Point2> boundary{floor.boundary};
    if (!placement_detail::valid_polygon(boundary, checked_tolerance)) return false;

    FurnitureFootprint footprint{};
    if (!placement_detail::make_furniture_footprint(furniture, footprint)) return false;
    for (const Point2 corner : footprint) {
        if (!placement_detail::point_in_or_on_polygon_unchecked(corner, boundary,
                                                                checked_tolerance))
            return false;
    }
    for (std::size_t index = 0; index < footprint.size(); ++index) {
        if (!placement_detail::segment_inside_polygon(
                footprint[index], footprint[(index + 1) % footprint.size()], boundary,
                checked_tolerance))
            return false;
    }

    const Point2 center{furniture.transform.position.x, furniture.transform.position.z};
    return placement_detail::point_in_or_on_polygon_unchecked(center, boundary,
                                                               checked_tolerance);
}

}  // namespace room_engine
