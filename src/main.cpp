#include "room_engine/application.hpp"
#include "room_engine/core/floor_plan_editor.hpp"
#include "room_engine/core/stable_id.hpp"
#include "room_engine/renderer/renderer_2d.hpp"
#include "room_engine/renderer/renderer_3d.hpp"

#include <SDL3/SDL.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <functional>
#include <limits>
#include <optional>
#include <string>

int main() {
    room_engine::Application application;
    if (!application.initialize()) return 1;
    room_engine::Viewport2D viewport{1280.0F, 720.0F};
    room_engine::Renderer2D scene{viewport};
    room_engine::Renderer3D scene_3d;
    room_engine::PerspectiveCamera camera_3d;
    camera_3d.position = {7.0F, 5.5F, 8.0F};
    camera_3d.target = {0.0F, 1.0F, 0.0F};
    bool view_3d = false;
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
                if (event.key.key == SDLK_DELETE || event.key.key == SDLK_BACKSPACE) editor.handle_shortcut(room_engine::EditorKey::Delete);
                if (event.key.key == SDLK_Z && (event.key.mod & SDL_KMOD_CTRL)) editor.handle_shortcut((event.key.mod & SDL_KMOD_SHIFT) ? room_engine::EditorKey::Redo : room_engine::EditorKey::Undo);
                if (event.key.key == SDLK_Y && (event.key.mod & SDL_KMOD_CTRL)) editor.handle_shortcut(room_engine::EditorKey::Redo);
                if (event.key.key == SDLK_C && (event.key.mod & SDL_KMOD_CTRL)) editor.handle_shortcut(room_engine::EditorKey::Copy);
                if (event.key.key == SDLK_V && (event.key.mod & SDL_KMOD_CTRL)) editor.handle_shortcut(room_engine::EditorKey::Paste);
                if (event.key.key == SDLK_D && (event.key.mod & SDL_KMOD_CTRL)) editor.handle_shortcut(room_engine::EditorKey::Duplicate);
                if (event.key.key == SDLK_A && (event.key.mod & SDL_KMOD_CTRL)) editor.handle_shortcut(room_engine::EditorKey::SelectAll);
                if (event.key.key == SDLK_S && (event.key.mod & SDL_KMOD_CTRL)) editor.handle_shortcut(room_engine::EditorKey::Save);
                if (event.key.key == SDLK_3) view_3d = !view_3d;
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
            if (view_3d) {
                room_engine::RoomDesign design;
                room_engine::Room rendered_room = room;
                if (!rendered_room.walls.empty()) {
                    float min_x = std::numeric_limits<float>::max();
                    float min_z = std::numeric_limits<float>::max();
                    float max_x = std::numeric_limits<float>::lowest();
                    float max_z = std::numeric_limits<float>::lowest();
                    for (const auto& wall : rendered_room.walls) {
                        min_x = std::min({min_x, wall.start.x, wall.end.x});
                        min_z = std::min({min_z, wall.start.y, wall.end.y});
                        max_x = std::max({max_x, wall.start.x, wall.end.x});
                        max_z = std::max({max_z, wall.start.y, wall.end.y});
                    }
                    rendered_room.floor = room_engine::Floor{"preview-floor", {{min_x, min_z}, {max_x, min_z}, {max_x, max_z}, {min_x, max_z}}, 0.0F, {}};
                }
                design.rooms.push_back(std::move(rendered_room));
                static_cast<void>(scene_3d.update_from_room(design, room.id));
                scene_3d.flush(*application.renderer(), camera_3d);
            } else {
                application.renderer()->set_camera(viewport.camera());
                scene.begin();
            for (int i = -10; i <= 10; ++i) {
                const float coordinate = static_cast<float>(i);
                scene.draw_line({{coordinate, -7.0F}, {coordinate, 7.0F}, {45, 50, 62, 255}}, -10);
                scene.draw_line({{-10.0F, coordinate}, {10.0F, coordinate}, {45, 50, 62, 255}}, -10);
            }
            for (const auto& wall : room.walls) {
                const auto id = room_engine::stable_scene_id(wall.id);
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
            }
            application.end_frame();
        }
        SDL_Delay(16);
    }
    return 0;
}
