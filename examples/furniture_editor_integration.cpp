#include "room_engine/furniture_editor.hpp"

#include <iostream>

int main() {
    room_engine::RoomDesign project;
    project.materials.push_back({"wood", "Oak", {0.55F, 0.32F, 0.16F}, 0.65F});
    room_engine::Room room;
    room.id = "studio";
    room.floor = room_engine::Floor{"floor", {{0, 0}, {6, 0}, {6, 4}, {0, 4}}, 0, "wood"};
    room.walls = {{"north", {0, 0}, {6, 0}, .2F, 2.8F, "wood"},
                  {"east", {6, 0}, {6, 4}, .2F, 2.8F, "wood"},
                  {"south", {6, 4}, {0, 4}, .2F, 2.8F, "wood"},
                  {"west", {0, 4}, {0, 0}, .2F, 2.8F, "wood"}};
    room.furniture.push_back({"table", "Table", {{3, .4F, 2}, {}, {1, 1, 1}},
                              {1.8F, .8F, .9F}, "wood", {}});
    project.rooms.push_back(std::move(room));

    room_engine::Viewport2D viewport{1280, 720};
    room_engine::Renderer2D floor_plan{viewport};
    room_engine::populate_furniture_floor_plan(floor_plan, project.rooms.front());

    room_engine::Renderer3D scene;
    if (!room_engine::populate_furniture_scene(scene, project, "studio")) return 1;
    std::cout << "2D items=" << floor_plan.items().size()
              << ", 3D instances=" << scene.instances().size() << '\n';
}
