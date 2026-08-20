#include "room_engine/application.hpp"
#include "room_engine/core/floor_plan_editor.hpp"
#include "room_engine/renderer/renderer_2d.hpp"

#include <SDL3/SDL.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <functional>
#include <optional>
#include <string>

int main() {
    room_engine::Application application;
    if (!application.initialize()) return 1;
    room_engine::Viewport2D viewport{1280.0F, 720.0F};
    room_engine::Renderer2D scene{viewport};
    room_engine::Room room;
    room.id = "room-1";
    room.name = "New room";
    room_engine::FloorPlanEditor editor{room};
    std::optional<room_engine::Point2> wall_start;
    bool placing_door = false;
    bool placing_window = false;
    room_engine::RenderMaterial material;
    while (application.running()) {
        application.poll_events([&](const SDL_Event& event) {
            if (event.type == SDL_EVENT_KEY_DOWN && !event.key.repeat) {
                if (event.key.key == SDLK_ESCAPE) { wall_start.reset(); placing_door = false; placing_window = false; }
                if (event.key.key == SDLK_DELETE || event.key.key == SDLK_BACKSPACE) editor.delete_selection();
                if (event.key.key == SDLK_Z && (event.key.mod & SDL_KMOD_CTRL)) editor.history().undo();
                if (event.key.key == SDLK_Y && (event.key.mod & SDL_KMOD_CTRL)) editor.history().redo();
                if (event.key.key == SDLK_D) { placing_door = true; placing_window = false; }
                if (event.key.key == SDLK_W) { placing_window = true; placing_door = false; }
            }
            if (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN && event.button.button == SDL_BUTTON_LEFT) {
                const auto world = viewport.screen_to_world({event.button.x, event.button.y});
                const room_engine::Point2 point{world.x, world.y};
                if (placing_door || placing_window) {
                    if (const auto hit = editor.hit_wall(point)) {
                        if (placing_door) editor.place_door(hit->id, hit->offset);
                        else editor.place_window(hit->id, hit->offset);
                    }
                    placing_door = false; placing_window = false;
                } else if (!wall_start) {
                    if (!editor.select(point)) wall_start = point;
                } else {
                    editor.draw_wall(*wall_start, point);
                    wall_start.reset();
                }
            }
        });
        if (application.begin_frame()) {
            application.renderer()->set_camera(viewport.camera());
            scene.begin();
            for (int i = -10; i <= 10; ++i) {
                const float coordinate = static_cast<float>(i);
                scene.draw_line({{coordinate, -7.0F}, {coordinate, 7.0F}, {45, 50, 62, 255}}, -10);
                scene.draw_line({{-10.0F, coordinate}, {10.0F, coordinate}, {45, 50, 62, 255}}, -10);
            }
            for (const auto& wall : room.walls) {
                const auto id = static_cast<std::uint64_t>(std::hash<std::string>{}(wall.id));
                scene.draw_line({{wall.start.x, wall.start.y}, {wall.end.x, wall.end.y}, {210, 215, 225, 255}, wall.thickness * 12.0F}, 0, id);
                scene.draw_circle({{wall.start.x, wall.start.y}, 0.09F, {90, 190, 245, 255}}, 1, id);
                scene.draw_circle({{wall.end.x, wall.end.y}, 0.09F, {90, 190, 245, 255}}, 1, id);
            }
            for (const auto& door : room.doors) {
                const auto wall = std::find_if(room.walls.begin(), room.walls.end(), [&](const auto& x) { return x.id == door.wall_id; });
                if (wall != room.walls.end()) {
                    const float length = std::hypot(wall->end.x - wall->start.x, wall->end.y - wall->start.y);
                    const auto point = [&](float offset) { return room_engine::Point2{wall->start.x + (wall->end.x - wall->start.x) * offset / length, wall->start.y + (wall->end.y - wall->start.y) * offset / length}; };
                    const auto a = point(door.offset); const auto b = point(door.offset + door.width);
                    scene.draw_line({{a.x, a.y}, {b.x, b.y}, {245, 180, 80, 255}, 0.08F}, 2);
                }
            }
            scene.flush(*application.renderer(), material);
            application.end_frame();
        }
        SDL_Delay(16);
    }
    return 0;
}
