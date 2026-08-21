#include "room_engine/core/room_design.hpp"
#include "room_engine/core/floor_plan_editor.hpp"
#include "room_engine/renderer/renderer_3d.hpp"
#include "room_engine/core/presentation.hpp"

#include <cassert>
#include <cmath>
#include <limits>
#include <filesystem>

using namespace room_engine;

namespace {

RoomDesign valid_design() {
    RoomDesign design;
    design.materials.push_back({"mat-wall", "Paint", {0.8F, 0.8F, 0.8F}, 0.4F});
    Room room;
    room.id = "room-1";
    room.name = "Living room";
    room.walls.push_back({"wall-north", {0.0F, 0.0F}, {4.0F, 0.0F}, 0.2F, 2.5F, "mat-wall"});
    room.floor = Floor{"floor-1", {{0.0F, 0.0F}, {4.0F, 0.0F}, {4.0F, 3.0F}, {0.0F, 3.0F}}, 0.0F, "mat-wall"};
    room.ceiling = Ceiling{"ceiling-1", {{0.0F, 0.0F}, {4.0F, 0.0F}, {4.0F, 3.0F}, {0.0F, 3.0F}}, 2.5F, "mat-wall"};
    room.doors.push_back({"door-1", "wall-north", 0.5F, 0.9F, 0.0F, 2.1F, false, "mat-wall"});
    room.windows.push_back({"window-1", "wall-north", 2.0F, 1.2F, 0.8F, 1.0F, "mat-wall"});
    room.furniture.push_back({"sofa-1", "Sofa", {{1.0F, 0.0F, 1.0F}, {}, {1.0F, 1.0F, 1.0F}}, {2.0F, 0.8F, 0.9F}, "mat-wall"});
    design.rooms.push_back(std::move(room));
    return design;
}

}  // namespace

void run_room_design_tests() {
    {
        const auto mesh = make_room_wall(4.0F, 2.5F, 0.2F);
        float min_x = std::numeric_limits<float>::max();
        float max_x = std::numeric_limits<float>::lowest();
        float max_y = std::numeric_limits<float>::lowest();
        for (const auto& vertex : mesh.vertices) {
            min_x = std::min(min_x, vertex.position.x);
            max_x = std::max(max_x, vertex.position.x);
            max_y = std::max(max_y, vertex.position.y);
            assert(std::fabs(vertex.normal.x) + std::fabs(vertex.normal.y) + std::fabs(vertex.normal.z) > 0.99F);
            assert(vertex.uv.x >= 0.0F && vertex.uv.x <= 1.0F);
            assert(vertex.uv.y >= 0.0F && vertex.uv.y <= 1.0F);
        }
        assert(std::fabs(min_x + 2.0F) < 0.001F);
        assert(std::fabs(max_x - 2.0F) < 0.001F);
        assert(std::fabs(max_y - 2.5F) < 0.001F);
    }
    {
        RoomDesign design = valid_design();
        const auto& wall = design.rooms[0].walls[0];
        std::vector<const Door*> doors{&design.rooms[0].doors[0]};
        std::vector<const Window*> windows{&design.rooms[0].windows[0]};
        const auto mesh = make_room_wall(wall, doors, windows);
        for (std::size_t i = 0; i + 2 < mesh.indices.size(); i += 3) {
            const auto& a = mesh.vertices[mesh.indices[i]].position;
            const auto& b = mesh.vertices[mesh.indices[i + 1]].position;
            const auto& c = mesh.vertices[mesh.indices[i + 2]].position;
            const float x = (a.x + b.x + c.x) / 3.0F;
            const float y = (a.y + b.y + c.y) / 3.0F;
            const bool in_door = x > 0.5F && x < 1.4F && y > 0.01F && y < 2.09F;
            const bool in_window = x > 2.0F && x < 3.2F && y > 0.81F && y < 1.79F;
            assert(!in_door && !in_window);
        }
    }
    {
        const RoomDesign design = valid_design();
        Renderer3D scene;
        assert(scene.update_from_room(design, "room-1"));
        assert(scene.instances().size() == 6);
        assert(scene.instances()[0].material.base_color.r == 204);
        RoomDesign invalid = design;
        invalid.rooms[0].walls[0].thickness = 0.0F;
        assert(!scene.update_from_room(invalid, "room-1"));
        assert(scene.instances().size() == 6);
    }
    {
        RoomDesign design = valid_design();
        design.rooms[0].walls[0].start = design.rooms[0].walls[0].end;
        assert(!design.valid());
    }
    {
        RoomDesign design = valid_design();
        design.rooms[0].doors[0].width = 5.0F;
        assert(!design.valid());
    }
    {
        RoomDesign design = valid_design();
        design.rooms[0].windows[0].wall_id = "missing-wall";
        assert(!design.valid());
    }
    {
        RoomDesign design = valid_design();
        design.rooms[0].furniture[0].id = "wall-north";
        assert(!design.valid());
    }
    {
        const RoomDesign original = valid_design();
        MemoryArchive archive;
        serialize(original, archive);
        const auto loaded = deserialize(archive);
        assert(loaded.has_value());
        assert(loaded->valid());
        assert(loaded->version == original.version);
        assert(loaded->materials[0].id == "mat-wall");
        assert(loaded->rooms[0].walls[0].id == "wall-north");
        assert(loaded->rooms[0].doors[0].id == "door-1");
        assert(loaded->rooms[0].windows[0].wall_id == "wall-north");
        assert(loaded->rooms[0].furniture[0].transform.position.x == 1.0F);
    }
    {
        MemoryArchive archive;
        archive.write_string("room_design", "not a room design");
        assert(!deserialize(archive).has_value());
    }
    {
        const RoomDesign original = valid_design();
        const auto json = std::filesystem::path{"/private/tmp/room_engine_test.room.json"};
        const auto glb = std::filesystem::path{"/private/tmp/room_engine_test.glb"};
        const auto svg = std::filesystem::path{"/private/tmp/room_engine_test.svg"};
        assert(export_project_json(original, json));
        RoomDesign imported;
        assert(import_project_json(json, imported));
        assert(imported.rooms[0].furniture.size() == 1);
        assert(export_room_glb(original, glb, "room-1"));
        assert(load_gltf(glb));
        assert(export_floor_plan_svg(original, svg, "room-1"));
        std::filesystem::remove(json); std::filesystem::remove(glb); std::filesystem::remove(svg);
    }
    {
        Room room;
        room.id = "editor-room";
        FloorPlanEditor editor(room);
        assert(editor.draw_wall({0.03F, 0.02F}, {4.01F, 0.02F}));
        assert(room.walls.size() == 1);
        assert((room.walls[0].start == Point2{0.0F, 0.0F}));
        assert((room.walls[0].end == Point2{4.0F, 0.0F}));
        assert(editor.place_door(room.walls[0].id, 1.0F));
        assert(editor.place_window(room.walls[0].id, 2.0F));
        assert(editor.history().undo_count() == 3);
        assert(editor.history().undo());
        assert(room.windows.empty());
        assert(editor.history().redo());
        assert(room.windows.size() == 1);
        assert(editor.select({3.8F, 0.05F}));
        assert(editor.selection().type == EditorSelectionType::Wall);
        assert(editor.move_endpoint(room.walls[0].id, false, {4.49F, 0.01F}));
        assert((room.walls[0].end == Point2{4.5F, 0.0F}));
        assert(editor.set_wall_length(room.walls[0].id, 5.0F));
        assert((room.walls[0].end == Point2{5.0F, 0.0F}));
        assert(editor.delete_selection());
        assert(room.walls.empty());
        assert(room.doors.empty());
        assert(editor.history().undo());
        assert(room.walls.size() == 1);
    }
    {
        Room room;
        room.id = "command-room";
        FloorPlanEditor editor(room);
        assert(editor.draw_wall({0.0F, 0.0F}, {4.0F, 0.0F}));
        const auto wall_id = room.walls.front().id;
        assert(editor.set_wall_properties(wall_id, 0.3F, 3.0F));
        assert(editor.place_door(wall_id, 1.0F));
        const auto door_id = room.doors.front().id;
        assert(editor.set_opening_properties(door_id, 1.0F, 0.0F, 2.0F));
        assert(editor.select({2.0F, 0.05F}, false));
        assert(editor.toggle_lock(wall_id));
        assert(!editor.move_endpoint(wall_id, false, {5.0F, 0.0F}));
        assert(editor.toggle_lock(wall_id));
        assert(editor.copy_selection());
        assert(editor.handle_shortcut(EditorKey::Duplicate));
        assert(room.walls.size() == 2);
        assert(editor.history().undo());
        assert(room.walls.size() == 1);
        assert(editor.history().redo());
        assert(room.walls.size() == 2);
        assert(editor.toggle_visibility(room.walls.back().id));
        assert(!editor.is_visible(room.walls.back().id));
        assert(editor.history().undo());
        assert(editor.is_visible(room.walls.back().id));
        for (std::size_t i = 0; i < editor.history().undo_count(); ++i) {
            const auto* command = editor.history().command(i);
            assert(command != nullptr);
            MemoryArchive archive;
            assert(command->serialize(archive, "command"));
            assert(!archive.read_string("command.name").empty());
        }
        const auto path = std::filesystem::path{"/private/tmp/room_engine_command_test.room"};
        assert(editor.save_project(path));
        Room loaded_room;
        FloorPlanEditor loaded(loaded_room);
        assert(loaded.open_project(path));
        assert(loaded.room().walls.size() == room.walls.size());
        assert(loaded.room().walls[0].thickness == room.walls[0].thickness);
        std::filesystem::remove(path);
        MemoryArchive malformed;
        malformed.write_string("room_design", "1 0 1 \"room\" 1 \"wall\"");
        assert(!deserialize(malformed).has_value());
    }
}
