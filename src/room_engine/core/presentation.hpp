#pragma once

#include "room_engine/core/room_design.hpp"
#include "room_engine/renderer/renderer.hpp"

#include <filesystem>
#include <string>

namespace room_engine {

struct ExportResult {
    bool ok = false;
    std::string error;
    [[nodiscard]] explicit operator bool() const noexcept { return ok; }
};

[[nodiscard]] ExportResult export_project_json(const RoomDesign&, const std::filesystem::path&);
[[nodiscard]] ExportResult import_project_json(const std::filesystem::path&, RoomDesign&);
[[nodiscard]] ExportResult export_room_glb(const RoomDesign&, const std::filesystem::path&, const StableId& room_id = {});
[[nodiscard]] ExportResult export_floor_plan_svg(const RoomDesign&, const std::filesystem::path&, const StableId& room_id = {});
[[nodiscard]] ExportResult export_screenshot(Renderer&, const std::filesystem::path&);

}  // namespace room_engine
