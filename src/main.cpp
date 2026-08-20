#include "room_engine/application.hpp"
#include "room_engine/renderer/renderer_2d.hpp"

#include <SDL3/SDL.h>

int main() {
    room_engine::Application application;
    if (!application.initialize()) return 1;
    room_engine::Viewport2D viewport{1280.0F, 720.0F};
    room_engine::Renderer2D scene{viewport};
    room_engine::Material material;
    while (application.running()) {
        application.poll_events();
        if (application.begin_frame()) {
            application.renderer()->set_camera(viewport.camera());
            scene.begin();
            for (int i = -10; i <= 10; ++i) {
                const float coordinate = static_cast<float>(i);
                scene.draw_line({{coordinate, -7.0F}, {coordinate, 7.0F}, {45, 50, 62, 255}}, -10);
                scene.draw_line({{-10.0F, coordinate}, {10.0F, coordinate}, {45, 50, 62, 255}}, -10);
            }
            scene.draw_rect({{-7.0F, -4.0F}, {7.0F, 4.0F}}, {90, 110, 130, 255}, 0, 1);
            scene.draw_rect({{-5.5F, -2.5F}, {-1.0F, 2.5F}}, {180, 125, 80, 255}, 1, 2);
            scene.draw_circle({{3.0F, 0.0F}, 1.5F, {80, 160, 210, 255}}, 1, 3);
            scene.flush(*application.renderer(), material);
            application.end_frame();
        }
        SDL_Delay(16);
    }
    return 0;
}
