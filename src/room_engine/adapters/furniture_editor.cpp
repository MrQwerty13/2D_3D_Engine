#include "room_engine/adapters/furniture_editor.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <vector>

namespace room_engine {

std::uint64_t stable_scene_id(std::string_view id) noexcept {
    std::uint64_t value = 14695981039346656037ULL;
    for (const char character : id) {
        const auto byte = static_cast<unsigned char>(character);
        value ^= byte;
        value *= 1099511628211ULL;
    }
    return value == 0 ? 1 : value;
}

void populate_furniture_floor_plan(Renderer2D& renderer, const Room& room,
                                   const FurnitureFloorPlanStyle& style) {
    renderer.begin();
    if (room.floor) {
        std::vector<Vec2> boundary;
        boundary.reserve(room.floor->boundary.size());
        for (const Point2 point : room.floor->boundary) boundary.push_back({point.x, point.y});
        renderer.draw_polygon(boundary, style.floor, style.floor_layer,
                              stable_scene_id(room.floor->id));
    }
    for (const WallSegment& wall : room.walls) {
        renderer.draw_line({{wall.start.x, wall.start.y}, {wall.end.x, wall.end.y},
                            style.wall, wall.thickness},
                           style.wall_layer, stable_scene_id(wall.id));
    }
    for (const Door& door : room.doors) {
        const auto wall = std::find_if(room.walls.begin(), room.walls.end(),
                                       [&](const WallSegment& candidate) {
                                           return candidate.id == door.wall_id;
                                       });
        if (wall == room.walls.end()) continue;
        const float length = detail::length(wall->start, wall->end);
        if (length <= 0.0001F) continue;
        const auto at = [&](float offset) {
            const float ratio = offset / length;
            return Vec2{wall->start.x + (wall->end.x - wall->start.x) * ratio,
                        wall->start.y + (wall->end.y - wall->start.y) * ratio};
        };
        renderer.draw_line({at(door.offset), at(door.offset + door.width), style.opening,
                            wall->thickness * 1.5F},
                           style.wall_layer + 1, stable_scene_id(door.id));
    }
    for (const Furniture& item : room.furniture) {
        const float yaw = std::atan2(2.0F * (item.transform.rotation.w * item.transform.rotation.y +
                                            item.transform.rotation.x * item.transform.rotation.z),
                                     1.0F - 2.0F * (item.transform.rotation.y * item.transform.rotation.y +
                                                    item.transform.rotation.z * item.transform.rotation.z));
        const float cosine = std::cos(yaw);
        const float sine = std::sin(yaw);
        const float half_x = item.dimensions.x * item.transform.scale.x * 0.5F;
        const float half_z = item.dimensions.z * item.transform.scale.z * 0.5F;
        const std::array<Vec2, 4> local{{{-half_x, -half_z}, {half_x, -half_z},
                                         {half_x, half_z}, {-half_x, half_z}}};
        std::array<Vec2, 4> world{};
        for (std::size_t i = 0; i < local.size(); ++i) {
            world[i] = {item.transform.position.x + local[i].x * cosine - local[i].y * sine,
                        item.transform.position.z + local[i].x * sine + local[i].y * cosine};
        }
        renderer.draw_polygon(world, style.furniture, style.furniture_layer,
                              stable_scene_id(item.id));
    }
}

bool populate_furniture_scene(Renderer3D& renderer, const RoomDesign& design,
                              const StableId& room_id) {
    return renderer.update_from_room(design, room_id);
}

}  // namespace room_engine
