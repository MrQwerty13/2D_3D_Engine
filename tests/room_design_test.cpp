#include "room_engine/core/room_design.hpp"
#include "room_engine/core/floor_plan_editor.hpp"

#include <cassert>

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
}
