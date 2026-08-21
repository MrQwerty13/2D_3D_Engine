#pragma once

#include "room_engine/core/room_design.hpp"
#include "room_engine/core/stable_id.hpp"
#include "room_engine/renderer/renderer_2d.hpp"
#include "room_engine/renderer/renderer_3d.hpp"

#include <cstdint>
#include <string_view>

namespace room_engine {

class FurnitureCatalog;

struct FurnitureFloorPlanStyle {
    Color floor{235, 235, 230, 255};
    Color wall{45, 50, 62, 255};
    Color furniture{80, 145, 210, 255};
    Color opening{245, 180, 80, 255};
    int floor_layer = -10;
    int wall_layer = 0;
    int furniture_layer = 5;
};

// Rebuilds Renderer2D's retained draw list from the room. Point2 maps to X/Z,
// while furniture Transform position uses X/Z and its Y-axis rotation.
void populate_furniture_floor_plan(Renderer2D&, const Room&,
                                   const FurnitureFloorPlanStyle& style = {});

// Rebuilds the generated 3D scene from the same validated domain model.
[[nodiscard]] bool populate_furniture_scene(Renderer3D&, const RoomDesign&,
                                            const StableId& room_id = {});
[[nodiscard]] bool populate_furniture_scene(Renderer3D&, const RoomDesign&,
                                            const FurnitureCatalog&,
                                            const StableId& room_id = {});

}  // namespace room_engine
