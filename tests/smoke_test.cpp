#include "room_engine/application.hpp"
#include "room_engine/core/entity.hpp"
#include "room_engine/core/transform.hpp"
#include "room_engine/renderer/viewport.hpp"
#include "room_engine/renderer/renderer_2d.hpp"
#include "room_engine/renderer/renderer_3d.hpp"

#include <cassert>
#include <cmath>
#include <string_view>

void run_room_design_tests();

int main() {
    assert(room_engine::application_name() == std::string_view{"room_engine"});

    room_engine::EntityRegistry entities;
    const auto first = entities.create();
    const auto second = entities.create();
    assert(first.valid());
    assert(first != second);
    assert(entities.alive(first));
    assert(entities.destroy(first));
    assert(!entities.alive(first));
    const auto third = entities.create();
    assert(third.value() > second.value());

    const room_engine::Transform parent{{10.0F, 0.0F, 0.0F}, {}, {2.0F, 2.0F, 2.0F}};
    const room_engine::Transform child{{1.0F, 2.0F, 3.0F}, {}, {1.0F, 1.0F, 1.0F}};
    const auto world = parent.combine(child);
    const auto point = world.transform_point({0.0F, 0.0F, 0.0F});
    assert(std::fabs(point.x - 12.0F) < 0.001F);
    assert(std::fabs(point.y - 4.0F) < 0.001F);
    assert(std::fabs(point.z - 6.0F) < 0.001F);

    room_engine::Viewport2D viewport{800.0F, 600.0F};
    viewport.set_center({10.0F, -4.0F});
    const room_engine::Vec2 world_point{12.5F, -2.0F};
    const auto screen_point = viewport.world_to_screen(world_point);
    const auto round_trip = viewport.screen_to_world(screen_point);
    assert(std::fabs(round_trip.x - world_point.x) < 0.001F);
    assert(std::fabs(round_trip.y - world_point.y) < 0.001F);
    assert(std::fabs(viewport.world_to_screen(viewport.center()).x - 400.0F) < 0.001F);
    assert(std::fabs(viewport.world_to_screen(viewport.center()).y - 300.0F) < 0.001F);

    room_engine::Renderer2D scene{viewport};
    scene.begin();
    scene.draw_rect({{9.0F, -5.0F}, {13.0F, -1.0F}}, {255, 255, 255, 255}, 0, 42);
    assert(scene.select(viewport.world_to_screen({11.0F, -3.0F})).value() == 42);
    assert(scene.selected().value() == 42);

    room_engine::PerspectiveCamera camera;
    camera.position = {0.0F, 0.0F, 5.0F};
    camera.target = {0.0F, 0.0F, 0.0F};
    room_engine::Renderer3D scene_3d;
    room_engine::Mesh triangle;
    triangle.vertices = {{{-1.0F, -1.0F, 0.0F}}, {{1.0F, -1.0F, 0.0F}}, {{0.0F, 1.0F, 0.0F}}};
    triangle.indices = {0, 1, 2};
    scene_3d.add_mesh(triangle, {}, {}, 99);
    const auto hit = scene_3d.raycast({400.0F, 300.0F}, camera, 800.0F, 600.0F);
    assert(hit.has_value());
    assert(hit->id == 99);
    assert(scene_3d.select({400.0F, 300.0F}, camera, 800.0F, 600.0F).value() == 99);
    assert(!room_engine::load_gltf("assets/does-not-exist.glb"));
    assert(!room_engine::load_gltf("tests/fixtures/invalid.gltf"));

    room_engine::Renderer3D room;
    room_engine::populate_sample_room(room);
    assert(room.instances().size() == 5);

    run_room_design_tests();

    return 0;
}
