#include "room_engine/core/room_design.hpp"
#include "room_engine/renderer/renderer_3d.hpp"

#include <chrono>
#include <iostream>

using namespace room_engine;

int main() {
    for (std::size_t count : {100U, 200U, 300U}) {
        RoomDesign design; design.materials.push_back({"mat", "profile", {0.7F, 0.7F, 0.7F}, 0.5F});
        Room room; room.id = "profile"; room.floor = Floor{"floor", {{0, 0}, {40, 0}, {40, 30}, {0, 30}}, 0, "mat"};
        room.walls = {{"wall0", {0, 0}, {40, 0}, .2F, 3.0F, "mat"}, {"wall1", {40, 0}, {40, 30}, .2F, 3.0F, "mat"}, {"wall2", {40, 30}, {0, 30}, .2F, 3.0F, "mat"}, {"wall3", {0, 30}, {0, 0}, .2F, 3.0F, "mat"}};
        for (std::size_t i = 0; i < count; ++i)
            room.furniture.push_back(
                {"f" + std::to_string(i), "box",
                 {{static_cast<float>(i % 40) + .5F, 0,
                   static_cast<float>(i / 40) + .5F},
                  {},
                  {1, 1, 1}},
                 {.8F, 1.0F, .8F}, "mat", {}});
        design.rooms.push_back(std::move(room));
        Renderer3D renderer; const auto start = std::chrono::steady_clock::now(); const bool ok = renderer.update_from_room(design, "profile"); const auto end = std::chrono::steady_clock::now();
        MemoryArchive archive; serialize(design, archive);
        std::cout << count << " furniture,build_ms=" << std::chrono::duration<double, std::milli>(end - start).count() << ",instances=" << renderer.instances().size() << ",project_bytes=" << archive.read_string("room_design").size() << ",valid=" << ok << '\n';
    }
}
