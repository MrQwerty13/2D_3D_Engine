#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <numeric>
#include <limits>
#include <span>
#include <vector>

namespace room_engine {

// Ear-clipping triangulation for a simple polygon without holes. The returned
// triangles use counter-clockwise 2D winding regardless of the input winding.
template <typename Point>
[[nodiscard]] inline std::vector<std::uint32_t> triangulate_simple_polygon(
    std::span<const Point> points) {
    std::vector<std::uint32_t> triangles;
    if (points.size() < 3 ||
        points.size() > std::numeric_limits<std::uint32_t>::max())
        return triangles;

    double twice_area = 0.0;
    for (std::size_t i = 0; i < points.size(); ++i) {
        const Point& first = points[i];
        const Point& second = points[(i + 1) % points.size()];
        if (!std::isfinite(first.x) || !std::isfinite(first.y)) return {};
        twice_area += static_cast<double>(first.x) * static_cast<double>(second.y) -
                      static_cast<double>(second.x) * static_cast<double>(first.y);
    }
    constexpr double epsilon = 1.0e-10;
    if (!std::isfinite(twice_area) || std::fabs(twice_area) <= epsilon) return triangles;

    std::vector<std::uint32_t> remaining(points.size());
    std::iota(remaining.begin(), remaining.end(), 0U);
    if (twice_area < 0.0) std::reverse(remaining.begin(), remaining.end());

    const auto cross = [&](std::uint32_t a, std::uint32_t b, std::uint32_t c) {
        return (static_cast<double>(points[b].x) - static_cast<double>(points[a].x)) *
                   (static_cast<double>(points[c].y) - static_cast<double>(points[a].y)) -
               (static_cast<double>(points[b].y) - static_cast<double>(points[a].y)) *
                   (static_cast<double>(points[c].x) - static_cast<double>(points[a].x));
    };
    const auto inside_triangle = [&](std::uint32_t point, std::uint32_t a,
                                     std::uint32_t b, std::uint32_t c) {
        return cross(a, b, point) >= -epsilon && cross(b, c, point) >= -epsilon &&
               cross(c, a, point) >= -epsilon;
    };

    triangles.reserve((points.size() - 2U) * 3U);
    while (remaining.size() > 3) {
        bool clipped = false;
        for (std::size_t i = 0; i < remaining.size(); ++i) {
            const std::uint32_t previous =
                remaining[(i + remaining.size() - 1U) % remaining.size()];
            const std::uint32_t current = remaining[i];
            const std::uint32_t next = remaining[(i + 1U) % remaining.size()];
            if (cross(previous, current, next) <= epsilon) continue;

            bool contains_point = false;
            for (const std::uint32_t candidate : remaining) {
                if (candidate == previous || candidate == current || candidate == next)
                    continue;
                if (inside_triangle(candidate, previous, current, next)) {
                    contains_point = true;
                    break;
                }
            }
            if (contains_point) continue;

            triangles.insert(triangles.end(), {previous, current, next});
            remaining.erase(remaining.begin() + static_cast<std::ptrdiff_t>(i));
            clipped = true;
            break;
        }
        if (!clipped) return {};
    }
    triangles.insert(triangles.end(), {remaining[0], remaining[1], remaining[2]});
    return triangles;
}

}  // namespace room_engine
