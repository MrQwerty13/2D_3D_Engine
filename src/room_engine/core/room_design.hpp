#pragma once

#include "room_engine/core/serialization.hpp"
#include "room_engine/core/transform.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <iomanip>
#include <limits>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace room_engine {

using StableId = std::string;

struct Point2 {
    float x = 0.0F;
    float y = 0.0F;
    friend bool operator==(const Point2&, const Point2&) = default;
};

struct WallSegment {
    StableId id;
    Point2 start{};
    Point2 end{};
    float thickness = 0.2F;
    float height = 2.5F;
    StableId material_id;
};

struct Floor {
    StableId id;
    std::vector<Point2> boundary;
    float elevation = 0.0F;
    StableId material_id;
};

struct Ceiling {
    StableId id;
    std::vector<Point2> boundary;
    float elevation = 2.5F;
    StableId material_id;
};

struct Door {
    StableId id;
    StableId wall_id;
    float offset = 0.0F;
    float width = 0.9F;
    float bottom = 0.0F;
    float height = 2.1F;
    bool open = false;
    StableId material_id;
};

struct Window {
    StableId id;
    StableId wall_id;
    float offset = 0.0F;
    float width = 1.2F;
    float bottom = 0.9F;
    float height = 1.2F;
    StableId material_id;
};

struct Furniture {
    StableId id;
    std::string name;
    Transform transform{};
    Vec3 dimensions{1.0F, 1.0F, 1.0F};
    StableId material_id;
    // Catalog provenance is persisted separately from the display name so an
    // application can resolve the same GLB asset after a project is reopened.
    StableId asset_id;
};

struct Material {
    StableId id;
    std::string name;
    Vec3 albedo{1.0F, 1.0F, 1.0F};
    float roughness = 0.5F;
};

struct Room {
    StableId id;
    std::string name;
    std::vector<WallSegment> walls;
    std::optional<Floor> floor;
    std::optional<Ceiling> ceiling;
    std::vector<Door> doors;
    std::vector<Window> windows;
    std::vector<Furniture> furniture;
};

struct ValidationIssue {
    std::string path;
    std::string message;
    friend bool operator==(const ValidationIssue&, const ValidationIssue&) = default;
};

struct RoomDesign {
    static constexpr std::uint32_t current_version = 2;
    std::uint32_t version = current_version;
    std::vector<Material> materials;
    std::vector<Room> rooms;

    [[nodiscard]] std::vector<ValidationIssue> validate() const;
    [[nodiscard]] bool valid() const { return validate().empty(); }
};

namespace detail {

inline bool finite(float value) { return std::isfinite(value); }
inline float length(Point2 a, Point2 b) {
    return std::hypot(b.x - a.x, b.y - a.y);
}
inline bool positive(float value) { return finite(value) && value > 0.0F; }
inline bool nonnegative(float value) { return finite(value) && value >= 0.0F; }
inline bool finite_vec3(Vec3 value) {
    return finite(value.x) && finite(value.y) && finite(value.z);
}
inline bool positive_vec3(Vec3 value) {
    return finite_vec3(value) && value.x > 0.0F && value.y > 0.0F && value.z > 0.0F;
}
inline bool finite_quaternion(Quaternion value) {
    return finite(value.w) && finite(value.x) && finite(value.y) && finite(value.z);
}

inline void issue(std::vector<ValidationIssue>& result, std::string path, std::string message) {
    result.push_back({std::move(path), std::move(message)});
}

inline bool add_id(std::unordered_set<std::string>& ids, const StableId& id,
                   std::string path, std::vector<ValidationIssue>& result) {
    if (id.empty()) {
        issue(result, std::move(path), "stable ID must not be empty");
        return false;
    }
    if (!ids.insert(id).second) {
        issue(result, std::move(path), "duplicate stable ID");
        return false;
    }
    return true;
}

inline bool has_id(const std::unordered_set<std::string>& ids, const StableId& id) {
    return !id.empty() && ids.contains(id);
}

inline double cross(Point2 a, Point2 b, Point2 c) {
    return static_cast<double>(b.x - a.x) * static_cast<double>(c.y - a.y) -
           static_cast<double>(b.y - a.y) * static_cast<double>(c.x - a.x);
}

inline bool point_on_segment(Point2 point, Point2 start, Point2 end) {
    constexpr double epsilon = 1.0e-8;
    if (std::fabs(cross(start, end, point)) > epsilon) return false;
    return static_cast<double>(point.x) >=
               std::min(static_cast<double>(start.x), static_cast<double>(end.x)) - epsilon &&
           static_cast<double>(point.x) <=
               std::max(static_cast<double>(start.x), static_cast<double>(end.x)) + epsilon &&
           static_cast<double>(point.y) >=
               std::min(static_cast<double>(start.y), static_cast<double>(end.y)) - epsilon &&
           static_cast<double>(point.y) <=
               std::max(static_cast<double>(start.y), static_cast<double>(end.y)) + epsilon;
}

inline int orientation(Point2 a, Point2 b, Point2 c) {
    constexpr double epsilon = 1.0e-8;
    const double value = cross(a, b, c);
    return value > epsilon ? 1 : value < -epsilon ? -1 : 0;
}

inline bool segments_intersect(Point2 a, Point2 b, Point2 c, Point2 d) {
    const int abc = orientation(a, b, c);
    const int abd = orientation(a, b, d);
    const int cda = orientation(c, d, a);
    const int cdb = orientation(c, d, b);
    if (abc != abd && cda != cdb) return true;
    return (abc == 0 && point_on_segment(c, a, b)) ||
           (abd == 0 && point_on_segment(d, a, b)) ||
           (cda == 0 && point_on_segment(a, c, d)) ||
           (cdb == 0 && point_on_segment(b, c, d));
}

inline std::optional<std::string> polygon_error(const std::vector<Point2>& boundary) {
    if (boundary.size() < 3) return "must contain at least three points";
    for (std::size_t i = 0; i < boundary.size(); ++i) {
        const Point2 point = boundary[i];
        if (!finite(point.x) || !finite(point.y)) return "must contain only finite points";
        if (length(point, boundary[(i + 1) % boundary.size()]) <= 0.0001F)
            return "must not contain duplicate adjacent points";
    }

    double twice_area = 0.0;
    for (std::size_t i = 0; i < boundary.size(); ++i) {
        const Point2 a = boundary[i];
        const Point2 b = boundary[(i + 1) % boundary.size()];
        twice_area += static_cast<double>(a.x) * static_cast<double>(b.y) -
                      static_cast<double>(b.x) * static_cast<double>(a.y);
    }
    if (!std::isfinite(twice_area) || std::fabs(twice_area) <= 1.0e-8)
        return "must enclose a non-zero finite area";

    for (std::size_t i = 0; i < boundary.size(); ++i) {
        const std::size_t i_next = (i + 1) % boundary.size();
        for (std::size_t j = i + 1; j < boundary.size(); ++j) {
            const std::size_t j_next = (j + 1) % boundary.size();
            if (i == j || i_next == j || j_next == i) continue;
            if (segments_intersect(boundary[i], boundary[i_next], boundary[j],
                                   boundary[j_next]))
                return "must not self-intersect";
        }
    }
    return std::nullopt;
}

inline void write_point(std::ostream& out, Point2 point) { out << point.x << ' ' << point.y << ' '; }
inline bool read_point(std::istream& in, Point2& point) {
    return static_cast<bool>(in >> point.x >> point.y);
}
inline void write_vec3(std::ostream& out, Vec3 value) { out << value.x << ' ' << value.y << ' ' << value.z << ' '; }
inline bool read_vec3(std::istream& in, Vec3& value) {
    return static_cast<bool>(in >> value.x >> value.y >> value.z);
}
inline void write_transform(std::ostream& out, const Transform& value) {
    write_vec3(out, value.position);
    out << value.rotation.w << ' ' << value.rotation.x << ' ' << value.rotation.y << ' '
        << value.rotation.z << ' ';
    write_vec3(out, value.scale);
}
inline bool read_transform(std::istream& in, Transform& value) {
    return read_vec3(in, value.position) &&
           static_cast<bool>(in >> value.rotation.w >> value.rotation.x >> value.rotation.y >>
                             value.rotation.z) &&
           read_vec3(in, value.scale);
}
inline void write_string(std::ostream& out, std::string_view value) { out << std::quoted(value) << ' '; }
inline bool read_string(std::istream& in, std::string& value) { return static_cast<bool>(in >> std::quoted(value)); }

}  // namespace detail

inline std::vector<ValidationIssue> RoomDesign::validate() const {
    std::vector<ValidationIssue> result;
    if (version != current_version)
        detail::issue(result, "version", "unsupported schema version");

    std::unordered_set<std::string> ids;
    std::unordered_set<std::string> material_ids;
    for (std::size_t i = 0; i < materials.size(); ++i) {
        const auto path = "materials[" + std::to_string(i) + "]";
        if (detail::add_id(ids, materials[i].id, path + ".id", result))
            material_ids.insert(materials[i].id);
        if (!detail::finite(materials[i].roughness) || materials[i].roughness < 0.0F ||
            materials[i].roughness > 1.0F)
            detail::issue(result, path + ".roughness", "must be between 0 and 1");
        const Vec3 albedo = materials[i].albedo;
        if (!detail::finite_vec3(albedo) || albedo.x < 0.0F || albedo.x > 1.0F ||
            albedo.y < 0.0F || albedo.y > 1.0F || albedo.z < 0.0F ||
            albedo.z > 1.0F)
            detail::issue(result, path + ".albedo", "must contain values between 0 and 1");
    }

    for (std::size_t ri = 0; ri < rooms.size(); ++ri) {
        const Room& room = rooms[ri];
        const auto base = "rooms[" + std::to_string(ri) + "]";
        detail::add_id(ids, room.id, base + ".id", result);
        std::unordered_set<std::string> wall_ids;
        for (std::size_t wi = 0; wi < room.walls.size(); ++wi) {
            const auto path = base + ".walls[" + std::to_string(wi) + "]";
            const WallSegment& wall = room.walls[wi];
            detail::add_id(ids, wall.id, path + ".id", result);
            if (!detail::add_id(wall_ids, wall.id, path + ".id", result)) continue;
            if (!detail::positive(wall.thickness))
                detail::issue(result, path + ".thickness", "must be positive and finite");
            if (!detail::positive(wall.height))
                detail::issue(result, path + ".height", "must be positive and finite");
            if (!detail::finite(wall.start.x) || !detail::finite(wall.start.y) ||
                !detail::finite(wall.end.x) || !detail::finite(wall.end.y) ||
                detail::length(wall.start, wall.end) <= 0.0001F)
                detail::issue(result, path, "wall endpoints must be finite and distinct");
            if (!wall.material_id.empty() &&
                !detail::has_id(material_ids, wall.material_id))
                detail::issue(result, path + ".material_id",
                              "references an unknown material");
        }

        if (room.floor) {
            detail::add_id(ids, room.floor->id, base + ".floor.id", result);
            if (const auto error = detail::polygon_error(room.floor->boundary))
                detail::issue(result, base + ".floor.boundary", *error);
            if (!detail::finite(room.floor->elevation))
                detail::issue(result, base + ".floor.elevation", "must be finite");
            if (!room.floor->material_id.empty() &&
                !detail::has_id(material_ids, room.floor->material_id))
                detail::issue(result, base + ".floor.material_id",
                              "references an unknown material");
        }
        if (room.ceiling) {
            detail::add_id(ids, room.ceiling->id, base + ".ceiling.id", result);
            if (const auto error = detail::polygon_error(room.ceiling->boundary))
                detail::issue(result, base + ".ceiling.boundary", *error);
            if (!detail::finite(room.ceiling->elevation))
                detail::issue(result, base + ".ceiling.elevation", "must be finite");
            if (!room.ceiling->material_id.empty() &&
                !detail::has_id(material_ids, room.ceiling->material_id))
                detail::issue(result, base + ".ceiling.material_id",
                              "references an unknown material");
        }
        if (room.floor && room.ceiling && detail::finite(room.floor->elevation) &&
            detail::finite(room.ceiling->elevation) &&
            room.ceiling->elevation <= room.floor->elevation)
            detail::issue(result, base + ".ceiling.elevation",
                          "must be above the floor elevation");

        struct OpeningRange {
            float start = 0.0F;
            float end = 0.0F;
        };
        std::unordered_map<StableId, std::vector<OpeningRange>> occupied_by_wall;
        auto validate_opening = [&](const auto& opening, std::string path) {
            detail::add_id(ids, opening.id, path + ".id", result);
            if (!detail::has_id(wall_ids, opening.wall_id))
                detail::issue(result, path + ".wall_id", "references an unknown wall");
            const auto wall_it = std::find_if(
                room.walls.begin(), room.walls.end(),
                [&](const WallSegment& wall) { return wall.id == opening.wall_id; });
            const float wall_length = wall_it == room.walls.end()
                                          ? 0.0F
                                          : detail::length(wall_it->start, wall_it->end);
            const float opening_end = opening.offset + opening.width;
            if (!detail::nonnegative(opening.offset) || !detail::positive(opening.width) ||
                !detail::finite(opening_end) || opening_end > wall_length + 0.0001F)
                detail::issue(result, path, "opening must fit within its wall");
            const float opening_top = opening.bottom + opening.height;
            if (!detail::nonnegative(opening.bottom) || !detail::positive(opening.height) ||
                !detail::finite(opening_top) ||
                (wall_it != room.walls.end() &&
                 opening_top > wall_it->height + 0.0001F))
                detail::issue(result, path, "opening must fit within wall height");
            if (detail::has_id(wall_ids, opening.wall_id) &&
                detail::nonnegative(opening.offset) && detail::positive(opening.width) &&
                detail::finite(opening_end)) {
                auto& occupied = occupied_by_wall[opening.wall_id];
                for (const auto& range : occupied) {
                    if (opening.offset < range.end && range.start < opening_end) {
                        detail::issue(result, path,
                                      "overlaps another opening on the same wall");
                        break;
                    }
                }
                occupied.push_back({opening.offset, opening_end});
            }
            if (!opening.material_id.empty() &&
                !detail::has_id(material_ids, opening.material_id))
                detail::issue(result, path + ".material_id",
                              "references an unknown material");
        };
        for (std::size_t i = 0; i < room.doors.size(); ++i)
            validate_opening(room.doors[i],
                             base + ".doors[" + std::to_string(i) + "]");
        for (std::size_t i = 0; i < room.windows.size(); ++i)
            validate_opening(room.windows[i],
                             base + ".windows[" + std::to_string(i) + "]");
        for (std::size_t i = 0; i < room.furniture.size(); ++i) {
            const auto path = base + ".furniture[" + std::to_string(i) + "]";
            const Furniture& item = room.furniture[i];
            detail::add_id(ids, item.id, path + ".id", result);
            if (!detail::positive_vec3(item.dimensions))
                detail::issue(result, path + ".dimensions", "must be positive and finite");
            if (!detail::finite_vec3(item.transform.position))
                detail::issue(result, path + ".transform.position", "must be finite");
            if (!detail::positive_vec3(item.transform.scale))
                detail::issue(result, path + ".transform.scale",
                              "must be positive and finite");
            const Quaternion rotation = item.transform.rotation;
            const float rotation_length_squared =
                rotation.w * rotation.w + rotation.x * rotation.x +
                rotation.y * rotation.y + rotation.z * rotation.z;
            if (!detail::finite_quaternion(rotation) ||
                !detail::finite(rotation_length_squared) ||
                std::fabs(rotation_length_squared - 1.0F) > 0.001F)
                detail::issue(result, path + ".transform.rotation",
                              "must be finite and normalized");
            if (!item.material_id.empty() &&
                !detail::has_id(material_ids, item.material_id))
                detail::issue(result, path + ".material_id",
                              "references an unknown material");
        }
    }
    return result;
}

// The archive stores one deterministic, quoted text payload. The leading version makes
// migrations explicit while keeping the model independent from file formats and renderers.
inline void serialize(const RoomDesign& design, ISerializer& archive) {
    std::ostringstream out;
    out << std::setprecision(std::numeric_limits<float>::max_digits10) << design.version << ' ' << design.materials.size() << ' ';
    for (const auto& material : design.materials) { detail::write_string(out, material.id); detail::write_string(out, material.name); detail::write_vec3(out, material.albedo); out << material.roughness << ' '; }
    out << design.rooms.size() << ' ';
    for (const auto& room : design.rooms) {
        detail::write_string(out, room.id); detail::write_string(out, room.name); out << room.walls.size() << ' ';
        for (const auto& wall : room.walls) { detail::write_string(out, wall.id); detail::write_point(out, wall.start); detail::write_point(out, wall.end); out << wall.thickness << ' ' << wall.height << ' '; detail::write_string(out, wall.material_id); }
        out << static_cast<bool>(room.floor) << ' ';
        if (room.floor) { detail::write_string(out, room.floor->id); out << room.floor->boundary.size() << ' '; for (auto p : room.floor->boundary) detail::write_point(out, p); out << room.floor->elevation << ' '; detail::write_string(out, room.floor->material_id); }
        out << static_cast<bool>(room.ceiling) << ' ';
        if (room.ceiling) { detail::write_string(out, room.ceiling->id); out << room.ceiling->boundary.size() << ' '; for (auto p : room.ceiling->boundary) detail::write_point(out, p); out << room.ceiling->elevation << ' '; detail::write_string(out, room.ceiling->material_id); }
        out << room.doors.size() << ' '; for (const auto& d : room.doors) { detail::write_string(out, d.id); detail::write_string(out, d.wall_id); out << d.offset << ' ' << d.width << ' ' << d.bottom << ' ' << d.height << ' ' << d.open << ' '; detail::write_string(out, d.material_id); }
        out << room.windows.size() << ' '; for (const auto& w : room.windows) { detail::write_string(out, w.id); detail::write_string(out, w.wall_id); out << w.offset << ' ' << w.width << ' ' << w.bottom << ' ' << w.height << ' '; detail::write_string(out, w.material_id); }
        out << room.furniture.size() << ' ';
        for (const auto& furniture : room.furniture) {
            detail::write_string(out, furniture.id);
            detail::write_string(out, furniture.name);
            detail::write_transform(out, furniture.transform);
            detail::write_vec3(out, furniture.dimensions);
            detail::write_string(out, furniture.material_id);
            if (design.version >= 2)
                detail::write_string(out, furniture.asset_id);
        }
    }
    archive.write_string("room_design", out.str());
}

[[nodiscard]] inline std::optional<RoomDesign> deserialize(const IDeserializer& archive) {
    try {
        constexpr std::size_t max_payload_bytes = 64U * 1024U * 1024U;
        constexpr std::size_t max_total_records = 1'000'000U;
        const std::string payload = archive.read_string("room_design");
        if (payload.empty() || payload.size() > max_payload_bytes) return std::nullopt;

        std::istringstream in{payload};
        std::uint32_t archive_version = 0;
        std::size_t total_records = 0;
        const auto read_count = [&](std::size_t& count) {
            if (!(in >> count) || count > max_total_records - total_records) return false;
            total_records += count;
            return true;
        };

        std::size_t material_count = 0;
        if (!(in >> archive_version) || archive_version == 0 ||
            archive_version > RoomDesign::current_version || !read_count(material_count))
            return std::nullopt;

        RoomDesign design;
        design.version = RoomDesign::current_version;
        design.materials.reserve(material_count);
        for (std::size_t i = 0; i < material_count; ++i) {
            Material material;
            if (!detail::read_string(in, material.id) ||
                !detail::read_string(in, material.name) ||
                !detail::read_vec3(in, material.albedo) || !(in >> material.roughness))
                return std::nullopt;
            design.materials.push_back(std::move(material));
        }

        std::size_t room_count = 0;
        if (!read_count(room_count)) return std::nullopt;
        design.rooms.reserve(room_count);
        for (std::size_t room_index = 0; room_index < room_count; ++room_index) {
            Room room;
            std::size_t wall_count = 0;
            if (!detail::read_string(in, room.id) ||
                !detail::read_string(in, room.name) || !read_count(wall_count))
                return std::nullopt;
            room.walls.reserve(wall_count);
            for (std::size_t i = 0; i < wall_count; ++i) {
                WallSegment wall;
                if (!detail::read_string(in, wall.id) ||
                    !detail::read_point(in, wall.start) ||
                    !detail::read_point(in, wall.end) ||
                    !(in >> wall.thickness >> wall.height) ||
                    !detail::read_string(in, wall.material_id))
                    return std::nullopt;
                room.walls.push_back(std::move(wall));
            }

            bool present = false;
            if (!(in >> present)) return std::nullopt;
            if (present) {
                Floor floor;
                std::size_t point_count = 0;
                if (!detail::read_string(in, floor.id) || !read_count(point_count))
                    return std::nullopt;
                floor.boundary.reserve(point_count);
                for (std::size_t i = 0; i < point_count; ++i) {
                    Point2 point;
                    if (!detail::read_point(in, point)) return std::nullopt;
                    floor.boundary.push_back(point);
                }
                if (!(in >> floor.elevation) ||
                    !detail::read_string(in, floor.material_id))
                    return std::nullopt;
                room.floor = std::move(floor);
            }

            if (!(in >> present)) return std::nullopt;
            if (present) {
                Ceiling ceiling;
                std::size_t point_count = 0;
                if (!detail::read_string(in, ceiling.id) || !read_count(point_count))
                    return std::nullopt;
                ceiling.boundary.reserve(point_count);
                for (std::size_t i = 0; i < point_count; ++i) {
                    Point2 point;
                    if (!detail::read_point(in, point)) return std::nullopt;
                    ceiling.boundary.push_back(point);
                }
                if (!(in >> ceiling.elevation) ||
                    !detail::read_string(in, ceiling.material_id))
                    return std::nullopt;
                room.ceiling = std::move(ceiling);
            }

            std::size_t door_count = 0;
            if (!read_count(door_count)) return std::nullopt;
            room.doors.reserve(door_count);
            for (std::size_t i = 0; i < door_count; ++i) {
                Door door;
                if (!detail::read_string(in, door.id) ||
                    !detail::read_string(in, door.wall_id) ||
                    !(in >> door.offset >> door.width >> door.bottom >> door.height >>
                      door.open) ||
                    !detail::read_string(in, door.material_id))
                    return std::nullopt;
                room.doors.push_back(std::move(door));
            }

            std::size_t window_count = 0;
            if (!read_count(window_count)) return std::nullopt;
            room.windows.reserve(window_count);
            for (std::size_t i = 0; i < window_count; ++i) {
                Window window;
                if (!detail::read_string(in, window.id) ||
                    !detail::read_string(in, window.wall_id) ||
                    !(in >> window.offset >> window.width >> window.bottom >>
                      window.height) ||
                    !detail::read_string(in, window.material_id))
                    return std::nullopt;
                room.windows.push_back(std::move(window));
            }

            std::size_t furniture_count = 0;
            if (!read_count(furniture_count)) return std::nullopt;
            room.furniture.reserve(furniture_count);
            for (std::size_t i = 0; i < furniture_count; ++i) {
                Furniture furniture;
                if (!detail::read_string(in, furniture.id) ||
                    !detail::read_string(in, furniture.name) ||
                    !detail::read_transform(in, furniture.transform) ||
                    !detail::read_vec3(in, furniture.dimensions) ||
                    !detail::read_string(in, furniture.material_id))
                    return std::nullopt;
                if (archive_version >= 2 &&
                    !detail::read_string(in, furniture.asset_id))
                    return std::nullopt;
                room.furniture.push_back(std::move(furniture));
            }
            design.rooms.push_back(std::move(room));
        }

        in >> std::ws;
        if (!in.eof()) return std::nullopt;
        return std::optional<RoomDesign>{std::move(design)};
    } catch (...) {
        return std::nullopt;
    }
}

}  // namespace room_engine
