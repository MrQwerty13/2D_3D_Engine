#include "room_engine/adapters/furniture_editor.hpp"

#include "room_engine/core/placement.hpp"
#include "room_engine/core/furniture_catalog.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <vector>

namespace room_engine {

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
        const FurnitureFootprint footprint = furniture_footprint_corners(item);
        std::array<Vec2, 4> world{};
        for (std::size_t i = 0; i < footprint.size(); ++i)
            world[i] = {footprint[i].x, footprint[i].y};
        renderer.draw_polygon(world, style.furniture, style.furniture_layer,
                              stable_scene_id(item.id));
    }
}

bool populate_furniture_scene(Renderer3D& renderer, const RoomDesign& design,
                              const StableId& room_id) {
    return renderer.update_from_room(design, room_id);
}

bool populate_furniture_scene(Renderer3D& renderer, const RoomDesign& design,
                              const FurnitureCatalog& catalog, const StableId& room_id) {
    return renderer.update_from_room(
        design, room_id, [&](const Furniture& furniture) -> const MeshAsset* {
            if (furniture.asset_id.empty()) return nullptr;
            const AssetLoadResult& loaded = catalog.load(furniture.asset_id);
            return loaded ? &*loaded.asset : nullptr;
        });
}

}  // namespace room_engine
