#include "room_engine/application.hpp"
#include "room_engine/renderer/debug_draw.hpp"

#include <SDL3/SDL.h>

#include <array>

int main() {
    room_engine::Application application;
    if (!application.initialize()) return 1;
    room_engine::PerspectiveCamera camera;
    camera.position = {7.0F, 6.0F, 7.0F};
    room_engine::Material debug_material;
    const std::array triangle_vertices = {
        room_engine::Vertex{{-0.75F, 0.01F, 0.0F}, {240, 80, 80, 255}},
        room_engine::Vertex{{0.75F, 0.01F, 0.0F}, {80, 240, 80, 255}},
        room_engine::Vertex{{0.0F, 0.01F, 1.0F}, {80, 120, 240, 255}}};
    const room_engine::VertexBuffer triangle =
        application.renderer()->create_vertex_buffer(triangle_vertices);
    while (application.running()) {
        application.poll_events();
        if (application.begin_frame()) {
            application.renderer()->set_camera(camera);
            application.renderer()->draw(triangle, triangle_vertices.size(), debug_material);
            room_engine::DebugDraw debug;
            debug.grid(10, 1.0F);
            debug.axes();
            debug.flush(*application.renderer(), debug_material);
            application.end_frame();
        }
        SDL_Delay(16);
    }
    return 0;
}
