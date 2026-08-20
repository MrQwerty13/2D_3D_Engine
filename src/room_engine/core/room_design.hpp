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
    static constexpr std::uint32_t current_version = 1;
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

inline void write_point(std::ostream& out, Point2 point) { out << point.x << ' ' << point.y << ' '; }
inline Point2 read_point(std::istream& in) {
    Point2 point;
    in >> point.x >> point.y;
    return point;
}
inline void write_vec3(std::ostream& out, Vec3 value) { out << value.x << ' ' << value.y << ' ' << value.z << ' '; }
inline Vec3 read_vec3(std::istream& in) {
    Vec3 value;
    in >> value.x >> value.y >> value.z;
    return value;
}
inline void write_transform(std::ostream& out, const Transform& value) {
    write_vec3(out, value.position);
    out << value.rotation.w << ' ' << value.rotation.x << ' ' << value.rotation.y << ' '
        << value.rotation.z << ' ';
    write_vec3(out, value.scale);
}
inline Transform read_transform(std::istream& in) {
    Transform value;
    value.position = read_vec3(in);
    in >> value.rotation.w >> value.rotation.x >> value.rotation.y >> value.rotation.z;
    value.scale = read_vec3(in);
    return value;
}
inline void write_string(std::ostream& out, std::string_view value) { out << std::quoted(value) << ' '; }
inline bool read_string(std::istream& in, std::string& value) { return static_cast<bool>(in >> std::quoted(value)); }

}  // namespace detail

inline std::vector<ValidationIssue> RoomDesign::validate() const {
    std::vector<ValidationIssue> result;
    if (version != current_version) detail::issue(result, "version", "unsupported schema version");

    std::unordered_set<std::string> ids;
    std::unordered_set<std::string> material_ids;
    for (std::size_t i = 0; i < materials.size(); ++i) {
        const auto path = "materials[" + std::to_string(i) + "]";
        if (detail::add_id(ids, materials[i].id, path + ".id", result)) material_ids.insert(materials[i].id);
        if (!detail::finite(materials[i].roughness) || materials[i].roughness < 0.0F || materials[i].roughness > 1.0F)
            detail::issue(result, path + ".roughness", "must be between 0 and 1");
        if (!detail::finite(materials[i].albedo.x) || !detail::finite(materials[i].albedo.y) || !detail::finite(materials[i].albedo.z))
            detail::issue(result, path + ".albedo", "must contain finite values");
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
            if (!detail::positive(wall.thickness)) detail::issue(result, path + ".thickness", "must be positive and finite");
            if (!detail::positive(wall.height)) detail::issue(result, path + ".height", "must be positive and finite");
            if (!detail::finite(wall.start.x) || !detail::finite(wall.start.y) || !detail::finite(wall.end.x) || !detail::finite(wall.end.y) || detail::length(wall.start, wall.end) <= 0.0001F)
                detail::issue(result, path, "wall endpoints must be finite and distinct");
            if (!wall.material_id.empty() && !detail::has_id(material_ids, wall.material_id)) detail::issue(result, path + ".material_id", "references an unknown material");
        }
        if (room.floor && room.floor->boundary.size() < 3) detail::issue(result, base + ".floor.boundary", "must contain at least three points");
        if (room.ceiling && room.ceiling->boundary.size() < 3) detail::issue(result, base + ".ceiling.boundary", "must contain at least three points");
        if (room.floor) {
            detail::add_id(ids, room.floor->id, base + ".floor.id", result);
            if (!room.floor->material_id.empty() && !detail::has_id(material_ids, room.floor->material_id)) detail::issue(result, base + ".floor.material_id", "references an unknown material");
        }
        if (room.ceiling) {
            detail::add_id(ids, room.ceiling->id, base + ".ceiling.id", result);
            if (!room.ceiling->material_id.empty() && !detail::has_id(material_ids, room.ceiling->material_id)) detail::issue(result, base + ".ceiling.material_id", "references an unknown material");
        }

        std::vector<std::pair<float, float>> occupied;
        auto validate_opening = [&](const auto& opening, std::string path) {
            detail::add_id(ids, opening.id, path + ".id", result);
            if (!detail::has_id(wall_ids, opening.wall_id)) detail::issue(result, path + ".wall_id", "references an unknown wall");
            const auto wall_it = std::find_if(room.walls.begin(), room.walls.end(), [&](const WallSegment& w) { return w.id == opening.wall_id; });
            const float wall_length = wall_it == room.walls.end() ? 0.0F : detail::length(wall_it->start, wall_it->end);
            if (!detail::nonnegative(opening.offset) || !detail::positive(opening.width) || opening.offset + opening.width > wall_length + 0.0001F)
                detail::issue(result, path, "opening must fit within its wall");
            if (!detail::nonnegative(opening.bottom) || !detail::positive(opening.height) ||
                (wall_it != room.walls.end() && opening.bottom + opening.height > wall_it->height + 0.0001F))
                detail::issue(result, path, "opening must fit within wall height");
            for (const auto [start, end] : occupied) if (opening.offset < end && start < opening.offset + opening.width)
                detail::issue(result, path, "overlaps another opening on the same wall");
            occupied.emplace_back(opening.offset, opening.offset + opening.width);
            if (!opening.material_id.empty() && !detail::has_id(material_ids, opening.material_id)) detail::issue(result, path + ".material_id", "references an unknown material");
        };
        for (std::size_t i = 0; i < room.doors.size(); ++i) validate_opening(room.doors[i], base + ".doors[" + std::to_string(i) + "]");
        for (std::size_t i = 0; i < room.windows.size(); ++i) validate_opening(room.windows[i], base + ".windows[" + std::to_string(i) + "]");
        for (std::size_t i = 0; i < room.furniture.size(); ++i) {
            const auto path = base + ".furniture[" + std::to_string(i) + "]";
            const Furniture& item = room.furniture[i];
            detail::add_id(ids, item.id, path + ".id", result);
            if (!detail::positive(item.dimensions.x) || !detail::positive(item.dimensions.y) || !detail::positive(item.dimensions.z)) detail::issue(result, path + ".dimensions", "must be positive and finite");
            if (!item.material_id.empty() && !detail::has_id(material_ids, item.material_id)) detail::issue(result, path + ".material_id", "references an unknown material");
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
        out << room.furniture.size() << ' '; for (const auto& f : room.furniture) { detail::write_string(out, f.id); detail::write_string(out, f.name); detail::write_transform(out, f.transform); detail::write_vec3(out, f.dimensions); detail::write_string(out, f.material_id); }
    }
    archive.write_string("room_design", out.str());
}

[[nodiscard]] inline std::optional<RoomDesign> deserialize(const IDeserializer& archive) {
    std::istringstream in{archive.read_string("room_design")};
    RoomDesign design;
    std::size_t count = 0;
    if (!(in >> design.version >> count)) return std::nullopt;
    for (std::size_t i = 0; i < count; ++i) { Material m; if (!detail::read_string(in, m.id) || !detail::read_string(in, m.name)) return std::nullopt; m.albedo = detail::read_vec3(in); in >> m.roughness; design.materials.push_back(std::move(m)); }
    if (!(in >> count)) return std::nullopt;
    for (std::size_t i = 0; i < count; ++i) {
        Room room; if (!detail::read_string(in, room.id) || !detail::read_string(in, room.name) || !(in >> count)) return std::nullopt;
        for (std::size_t j = 0; j < count; ++j) { WallSegment w; if (!detail::read_string(in, w.id)) return std::nullopt; w.start = detail::read_point(in); w.end = detail::read_point(in); in >> w.thickness >> w.height; if (!detail::read_string(in, w.material_id)) return std::nullopt; room.walls.push_back(std::move(w)); }
        bool present = false; in >> present; if (present) { Floor f; std::size_t points; detail::read_string(in, f.id); in >> points; f.boundary.reserve(points); for (std::size_t j = 0; j < points; ++j) f.boundary.push_back(detail::read_point(in)); in >> f.elevation; detail::read_string(in, f.material_id); room.floor = std::move(f); }
        in >> present; if (present) { Ceiling c; std::size_t points; detail::read_string(in, c.id); in >> points; c.boundary.reserve(points); for (std::size_t j = 0; j < points; ++j) c.boundary.push_back(detail::read_point(in)); in >> c.elevation; detail::read_string(in, c.material_id); room.ceiling = std::move(c); }
        in >> count; for (std::size_t j = 0; j < count; ++j) { Door d; detail::read_string(in, d.id); detail::read_string(in, d.wall_id); in >> d.offset >> d.width >> d.bottom >> d.height >> d.open; detail::read_string(in, d.material_id); room.doors.push_back(std::move(d)); }
        in >> count; for (std::size_t j = 0; j < count; ++j) { Window w; detail::read_string(in, w.id); detail::read_string(in, w.wall_id); in >> w.offset >> w.width >> w.bottom >> w.height; detail::read_string(in, w.material_id); room.windows.push_back(std::move(w)); }
        in >> count; for (std::size_t j = 0; j < count; ++j) { Furniture f; detail::read_string(in, f.id); detail::read_string(in, f.name); f.transform = detail::read_transform(in); f.dimensions = detail::read_vec3(in); detail::read_string(in, f.material_id); room.furniture.push_back(std::move(f)); }
        design.rooms.push_back(std::move(room));
    }
    return in.good() || in.eof() ? std::optional<RoomDesign>{std::move(design)} : std::nullopt;
}

}  // namespace room_engine
