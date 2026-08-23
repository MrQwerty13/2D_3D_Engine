#include "room_engine/core/room_design.hpp"
#include "room_engine/core/floor_plan_editor.hpp"
#include "room_engine/core/furniture_catalog.hpp"
#include "room_engine/core/placement.hpp"
#include "room_engine/adapters/furniture_editor.hpp"
#include "room_engine/renderer/renderer_3d.hpp"
#include "room_engine/core/presentation.hpp"

#include <cassert>
#include <cmath>
#include <limits>
#include <filesystem>
#include <fstream>
#include <iterator>

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
    room.furniture.push_back({"sofa-1", "Sofa",
                              {{1.0F, 0.4F, 1.0F}, {}, {1.0F, 1.0F, 1.0F}},
                              {2.0F, 0.8F, 0.9F}, "mat-wall", "sofa-asset"});
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
        RoomDesign design;
        Room first;
        first.id = "first-room";
        first.furniture.push_back(
            {"chair-1", "Chair", {{0.0F, 0.5F, 0.0F}, {}, {1.0F, 1.0F, 1.0F}},
             {1.0F, 1.0F, 1.0F}, {}, "chair"});
        first.furniture.push_back(
            {"chair-2", "Chair", {{2.0F, 0.5F, 0.0F}, {}, {1.0F, 1.0F, 1.0F}},
             {1.0F, 1.0F, 1.0F}, {}, "chair"});
        Room second;
        second.id = "second-room";
        design.rooms.push_back(std::move(first));
        design.rooms.push_back(std::move(second));
        MemoryArchive archive;
        serialize(design, archive);
        const auto loaded = deserialize(archive);
        assert(loaded.has_value());
        assert(loaded->valid());
        assert(loaded->rooms.size() == 2);
        assert(loaded->rooms[0].furniture.size() == 2);
        assert(loaded->rooms[0].furniture[0].asset_id == "chair");

        archive.write_string("room_design", archive.read_string("room_design") + " trailing");
        assert(!deserialize(archive).has_value());
    }
    {
        RoomDesign legacy = valid_design();
        legacy.version = 1;
        legacy.rooms[0].furniture[0].asset_id = "legacy-not-serialized";
        MemoryArchive archive;
        serialize(legacy, archive);
        const auto loaded = deserialize(archive);
        assert(loaded.has_value());
        assert(loaded->version == RoomDesign::current_version);
        assert(loaded->rooms[0].furniture[0].asset_id.empty());

        MemoryArchive oversized;
        oversized.write_string("room_design",
                               "1 0 1 \"room\" \"\" 0 1 \"floor\" 1000001");
        assert(!deserialize(oversized).has_value());
        MemoryArchive future;
        future.write_string("room_design", "999 0 0");
        assert(!deserialize(future).has_value());
    }
    {
        RoomDesign design;
        Room room;
        room.id = "openings";
        room.walls.push_back(
            {"wall-a", {0.0F, 0.0F}, {4.0F, 0.0F}, 0.2F, 2.5F, {}});
        room.walls.push_back(
            {"wall-b", {0.0F, 3.0F}, {4.0F, 3.0F}, 0.2F, 2.5F, {}});
        room.doors.push_back(
            {"door-a", "wall-a", 1.0F, 0.9F, 0.0F, 2.1F, false, {}});
        room.doors.push_back(
            {"door-b", "wall-b", 1.0F, 0.9F, 0.0F, 2.1F, false, {}});
        design.rooms.push_back(room);
        assert(design.valid());
        design.rooms[0].windows.push_back(
            {"overlap", "wall-a", 1.5F, 1.0F, 0.9F, 1.0F, {}});
        assert(!design.valid());
    }
    {
        RoomDesign design = valid_design();
        design.rooms[0].floor->boundary =
            {{0.0F, 0.0F}, {3.0F, 3.0F}, {0.0F, 3.0F}, {3.0F, 0.0F}};
        assert(!design.valid());
        design = valid_design();
        design.rooms[0].furniture[0].transform.scale.x = 0.0F;
        assert(!design.valid());
        design = valid_design();
        design.rooms[0].furniture[0].transform.rotation = {0.0F, 0.0F, 0.0F, 0.0F};
        assert(!design.valid());
    }
    {
        const Floor floor{"floor", {{0.0F, 0.0F}, {4.0F, 0.0F},
                                     {4.0F, 4.0F}, {0.0F, 4.0F}}, 0.0F, {}};
        Furniture first{"first", "First", {{1.0F, 0.5F, 1.0F}, {}, {2.0F, 1.0F, 1.0F}},
                        {1.0F, 1.0F, 1.0F}, {}, {}};
        Furniture second{"second", "Second",
                         {{2.4F, 0.5F, 1.0F}, {}, {1.0F, 1.0F, 1.0F}},
                         {1.0F, 1.0F, 1.0F}, {}, {}};
        const Vec3 scaled = scaled_furniture_dimensions(first);
        assert(scaled.x == 2.0F && scaled.y == 1.0F && scaled.z == 1.0F);
        assert(furniture_footprints_overlap(first, second));
        assert(furniture_footprint_inside_floor(first, floor));
        first.transform.position.x = -0.25F;
        assert(!furniture_footprint_inside_floor(first, floor));

        const Floor concave{"concave", {{0.0F, 0.0F}, {4.0F, 0.0F}, {4.0F, 1.0F},
                                         {1.0F, 1.0F}, {1.0F, 4.0F}, {0.0F, 4.0F}},
                            0.0F, {}};
        const Mesh mesh = make_room_floor(concave);
        assert(mesh.indices.size() == 12);
    }
    {
        const RoomDesign original = valid_design();
        const auto temporary = std::filesystem::temp_directory_path();
        const auto json = temporary / "room_engine_test.room.json";
        const auto glb = temporary / "room_engine_test.glb";
        const auto svg = temporary / "room_engine_test.svg";
        assert(export_project_json(original, json));
        RoomDesign imported;
        assert(import_project_json(json, imported));
        assert(imported.rooms[0].furniture.size() == 1);
        assert(imported.rooms[0].furniture[0].asset_id == "sofa-asset");
        assert(export_room_glb(original, glb, "room-1"));
        const AssetLoadResult exported_asset = load_gltf(glb);
        assert(exported_asset);
        assert(exported_asset.asset->bounds.dimensions().x >= 4.0F);
        assert(exported_asset.asset->bounds.dimensions().z >= 3.0F);
        assert(export_floor_plan_svg(original, svg, "room-1"));

        std::ifstream json_file(json, std::ios::binary);
        const std::string json_text((std::istreambuf_iterator<char>(json_file)), {});
        assert(json_text.find("\"doors\"") != std::string::npos);
        assert(json_text.find("\"windows\"") != std::string::npos);
        assert(json_text.find("\"asset_id\":\"sofa-asset\"") != std::string::npos);
        std::ifstream svg_file(svg, std::ios::binary);
        const std::string svg_text((std::istreambuf_iterator<char>(svg_file)), {});
        assert(svg_text.find("<polygon") != std::string::npos);
        assert(svg_text.find("#4b9dcc") != std::string::npos);

        GltfAssetCache cache;
        FurnitureCatalog catalog(cache);
        FurnitureAssetMetadata metadata;
        metadata.id = "catalog-item";
        metadata.name = "Catalog item";
        metadata.model = glb;
        metadata.default_scale = {2.0F, 1.0F, 2.0F};
        assert(catalog.add(std::move(metadata)));
        Room placed_room;
        placed_room.id = "placed-room";
        placed_room.floor = Floor{"placed-floor",
                                  {{-10.0F, -10.0F}, {10.0F, -10.0F},
                                   {10.0F, 10.0F}, {-10.0F, 10.0F}},
                                  0.0F, {}};
        FurniturePlacement placement;
        placement.position = {0.0F, 0.0F, 0.0F};
        placement.scale = {0.5F, 1.0F, 0.5F};
        const FurniturePlacementResult placed =
            catalog.place(placed_room, "catalog-item", placement);
        assert(placed);
        assert(placed.furniture.has_value());
        assert(placed_room.furniture[0].asset_id == "catalog-item");
        assert(placed_room.furniture[0].transform.scale.x == 1.0F);
        assert(placed_room.furniture[0].dimensions.x ==
               exported_asset.asset->bounds.dimensions().x);
        FurniturePlacement duplicate = placement;
        duplicate.mode = PlacementMode::Strict;
        assert(!catalog.place(placed_room, "catalog-item", duplicate));
        assert(placed_room.furniture.size() == 1);

        RoomDesign placed_design;
        placed_design.rooms.push_back(placed_room);
        Renderer3D asset_scene;
        assert(populate_furniture_scene(asset_scene, placed_design, catalog,
                                        "placed-room"));
        assert(asset_scene.instances().size() >= 2);
        assert(asset_scene.instances().back().id == stable_scene_id("catalog-item"));

        std::filesystem::remove(json);
        std::filesystem::remove(glb);
        std::filesystem::remove(svg);
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
        assert(!editor.place_window(room.walls[0].id, 1.5F));
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
        assert(!editor.set_wall_length(room.walls[0].id, 3.0F));
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
        assert(editor.select({3.0F, 0.05F}, false));
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
        const auto path =
            std::filesystem::temp_directory_path() / "room_engine_command_test.room";
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
    {
        Room room;
        room.id = "paste-room";
        room.walls.push_back(
            {"original-a", {0.0F, 0.0F}, {4.0F, 0.0F}, 0.2F, 2.5F, {}});
        room.walls.push_back(
            {"original-b", {0.0F, 3.0F}, {4.0F, 3.0F}, 0.2F, 2.5F, {}});
        room.doors.push_back(
            {"original-door", "original-a", 1.0F, 0.9F, 0.0F, 2.1F, false, {}});
        room.windows.push_back(
            {"original-window", "original-b", 2.0F, 1.0F, 0.9F, 1.0F, {}});
        FloorPlanEditor editor(room);
        editor.set_selection({{EditorSelectionType::Wall, "original-a"},
                              {EditorSelectionType::Wall, "original-b"},
                              {EditorSelectionType::Door, "original-door"},
                              {EditorSelectionType::Window, "original-window"}});
        assert(editor.copy_selection());
        assert(editor.paste({0.25F, 0.25F}));
        assert(room.walls.size() == 4);
        assert(room.doors.size() == 2);
        assert(room.windows.size() == 2);
        assert(room.doors.back().wall_id == room.walls[2].id);
        assert(room.windows.back().wall_id == room.walls[3].id);
        assert(room.doors.back().wall_id != room.windows.back().wall_id);
    }
    {
        Room room;
        room.id = "selection-room";
        room.furniture.push_back(
            {"desk", "Desk", {{1.0F, 0.5F, 1.0F}, {}, {1.0F, 1.0F, 1.0F}},
             {2.0F, 1.0F, 1.0F}, {}, {}});
        FloorPlanEditor editor(room);
        assert(editor.select({1.0F, 1.0F}));
        assert(editor.selection().type == EditorSelectionType::Furniture);
        assert(editor.copy_selection());
        assert(editor.paste());
        assert(room.furniture.size() == 2);
        assert(room.furniture[0].id != room.furniture[1].id);
    }
    {
        RoomDesign design = valid_design();
        Room room = design.rooms.front();
        const auto path =
            std::filesystem::temp_directory_path() / "room_engine_material_room.room";
        FloorPlanEditor without_materials(room);
        assert(!without_materials.save_project(path));
        FloorPlanEditor editor(room, design.materials);
        assert(editor.save_project(path));
        Room loaded_room;
        FloorPlanEditor loaded(loaded_room);
        assert(loaded.open_project(path));
        assert(loaded.materials().size() == 1);
        assert(loaded.room().walls[0].material_id == "mat-wall");
        std::filesystem::remove(path);
    }
}
