#pragma once

#include "room_engine/core/placement.hpp"

#include <algorithm>
#include <cmath>
#include <chrono>
#include <cstddef>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <limits>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace room_engine {

enum class EditorSelectionType { None, Wall, Door, Window, Furniture };
struct EditorSelection {
    EditorSelectionType type = EditorSelectionType::None;
    StableId id;
    friend bool operator==(const EditorSelection&, const EditorSelection&) = default;
};

using EditorSelectionSet = std::vector<EditorSelection>;

enum class EditorKey { Undo, Redo, Copy, Paste, Duplicate, Delete, SelectAll, ToggleLock, ToggleVisibility, Save };

struct SnapSettings {
    bool grid = true;
    bool endpoints = true;
    bool corners = true;
    float grid_size = 0.25F;
    float tolerance = 0.18F;
};

struct WallHit { StableId id; float distance = 0.0F; float offset = 0.0F; };

class EditCommand {
public:
    virtual ~EditCommand() = default;
    virtual void redo() = 0;
    virtual void undo() = 0;
    [[nodiscard]] virtual std::string name() const = 0;
    // Commands are inspectable and persistable without exposing implementation details.
    virtual bool serialize(ISerializer& archive, std::string_view key) const {
        archive.write_string(key, name());
        return true;
    }
};

class CommandHistory {
public:
    void execute(std::unique_ptr<EditCommand> command) {
        if (!command) return;
        command->redo();
        if (cursor_ < commands_.size()) commands_.erase(commands_.begin() + static_cast<std::ptrdiff_t>(cursor_), commands_.end());
        commands_.push_back(std::move(command));
        cursor_ = commands_.size();
    }
    bool undo() {
        if (cursor_ == 0) return false;
        commands_[cursor_ - 1]->undo();
        --cursor_;
        return true;
    }
    bool redo() {
        if (cursor_ == commands_.size()) return false;
        commands_[cursor_]->redo();
        ++cursor_;
        return true;
    }
    void clear() noexcept { commands_.clear(); cursor_ = 0; }
    [[nodiscard]] std::size_t undo_count() const noexcept { return cursor_; }
    [[nodiscard]] std::size_t redo_count() const noexcept { return commands_.size() - cursor_; }
    [[nodiscard]] const EditCommand* command(std::size_t index) const noexcept { return index < commands_.size() ? commands_[index].get() : nullptr; }
    bool serialize(ISerializer& archive, std::string_view key = "history") const {
        archive.write_string(std::string{key} + ".count", std::to_string(commands_.size()));
        archive.write_string(std::string{key} + ".cursor", std::to_string(cursor_));
        for (std::size_t i = 0; i < commands_.size(); ++i)
            if (!commands_[i]->serialize(archive, std::string{key} + "." + std::to_string(i))) return false;
        return true;
    }

private:
    std::vector<std::unique_ptr<EditCommand>> commands_;
    std::size_t cursor_ = 0;
};

namespace detail {
inline float distance(Point2 a, Point2 b) { return std::hypot(a.x - b.x, a.y - b.y); }
inline Point2 lerp(Point2 a, Point2 b, float t) { return {a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t}; }
inline float clamp01(float value) { return std::clamp(value, 0.0F, 1.0F); }
inline float distance_to_segment(Point2 point, Point2 start, Point2 end) {
    const Point2 delta{end.x - start.x, end.y - start.y};
    const float length_squared = delta.x * delta.x + delta.y * delta.y;
    if (length_squared <= std::numeric_limits<float>::epsilon())
        return distance(point, start);
    const float t = clamp01(((point.x - start.x) * delta.x +
                             (point.y - start.y) * delta.y) /
                            length_squared);
    return distance(point, lerp(start, end, t));
}
inline StableId next_id(const Room& room, std::string prefix) {
    for (std::size_t i = 1;; ++i) {
        StableId candidate = prefix + "-" + std::to_string(i);
        const bool used = room.id == candidate ||
                          (room.floor && room.floor->id == candidate) ||
                          (room.ceiling && room.ceiling->id == candidate) ||
                          std::any_of(room.walls.begin(), room.walls.end(),
                                      [&](const auto& item) { return item.id == candidate; }) ||
                          std::any_of(room.doors.begin(), room.doors.end(),
                                      [&](const auto& item) { return item.id == candidate; }) ||
                          std::any_of(room.windows.begin(), room.windows.end(),
                                      [&](const auto& item) { return item.id == candidate; }) ||
                          std::any_of(room.furniture.begin(), room.furniture.end(),
                                      [&](const auto& item) { return item.id == candidate; });
        if (!used) return candidate;
    }
}

inline bool opening_fits(const Room& room, const StableId& wall_id, float offset,
                         float width, float bottom, float height,
                         const StableId& excluded_id = {}) {
    const auto wall = std::find_if(room.walls.begin(), room.walls.end(),
                                   [&](const auto& item) { return item.id == wall_id; });
    if (wall == room.walls.end() || !std::isfinite(offset) || !std::isfinite(width) ||
        !std::isfinite(bottom) || !std::isfinite(height) || offset < 0.0F ||
        width <= 0.0F || bottom < 0.0F || height <= 0.0F)
        return false;
    const float end = offset + width;
    const float top = bottom + height;
    if (!std::isfinite(end) || !std::isfinite(top) ||
        end > distance(wall->start, wall->end) + 0.0001F ||
        top > wall->height + 0.0001F)
        return false;
    const auto overlaps = [&](const auto& opening) {
        return opening.id != excluded_id && opening.wall_id == wall_id &&
               offset < opening.offset + opening.width && opening.offset < end;
    };
    return std::none_of(room.doors.begin(), room.doors.end(), overlaps) &&
           std::none_of(room.windows.begin(), room.windows.end(), overlaps);
}

inline bool wall_openings_fit(const Room& room, const WallSegment& wall) {
    const auto fits = [&](const auto& opening) {
        if (opening.wall_id != wall.id) return true;
        const float end = opening.offset + opening.width;
        const float top = opening.bottom + opening.height;
        return std::isfinite(end) && std::isfinite(top) && opening.offset >= 0.0F &&
               opening.width > 0.0F && opening.bottom >= 0.0F && opening.height > 0.0F &&
               end <= distance(wall.start, wall.end) + 0.0001F &&
               top <= wall.height + 0.0001F;
    };
    return std::all_of(room.doors.begin(), room.doors.end(), fits) &&
           std::all_of(room.windows.begin(), room.windows.end(), fits);
}
}  // namespace detail

class RoomSnapshotCommand final : public EditCommand {
public:
    RoomSnapshotCommand(Room& room, Room before, Room after, std::string label)
        : room_(room), before_(std::move(before)), after_(std::move(after)), label_(std::move(label)) {}
    void redo() override { room_ = after_; }
    void undo() override { room_ = before_; }
    [[nodiscard]] std::string name() const override { return label_; }
    bool serialize(ISerializer& archive, std::string_view key) const override {
        RoomDesign before_design;
        before_design.rooms.push_back(before_);
        RoomDesign after_design;
        after_design.rooms.push_back(after_);
        MemoryArchive before_archive;
        MemoryArchive after_archive;
        room_engine::serialize(before_design, before_archive);
        room_engine::serialize(after_design, after_archive);
        archive.write_string(std::string{key} + ".name", label_);
        archive.write_string(std::string{key} + ".before", before_archive.read_string("room_design"));
        archive.write_string(std::string{key} + ".after", after_archive.read_string("room_design"));
        return true;
    }

private:
    Room& room_;
    Room before_;
    Room after_;
    std::string label_;
};

class SetSnapshotCommand final : public EditCommand {
public:
    SetSnapshotCommand(std::unordered_set<StableId>& target, std::unordered_set<StableId> before,
                       std::unordered_set<StableId> after, std::string label)
        : target_(target), before_(std::move(before)), after_(std::move(after)), label_(std::move(label)) {}
    void redo() override { target_ = after_; }
    void undo() override { target_ = before_; }
    [[nodiscard]] std::string name() const override { return label_; }
    bool serialize(ISerializer& archive, std::string_view key) const override {
        std::vector<StableId> values(after_.begin(), after_.end());
        std::sort(values.begin(), values.end());
        std::string payload;
        for (const auto& value : values) payload += value + "\n";
        archive.write_string(std::string{key} + ".name", label_);
        archive.write_string(std::string{key} + ".values", payload);
        return true;
    }
private:
    std::unordered_set<StableId>& target_;
    std::unordered_set<StableId> before_;
    std::unordered_set<StableId> after_;
    std::string label_;
};

class FloorPlanEditor {
public:
    explicit FloorPlanEditor(Room& room, std::vector<Material> materials = {})
        : room_(room), materials_(std::move(materials)) {}

    [[nodiscard]] const Room& room() const noexcept { return room_; }
    [[nodiscard]] Room& room() noexcept { return room_; }
    [[nodiscard]] const SnapSettings& snapping() const noexcept { return snap_; }
    void set_snapping(SnapSettings settings) noexcept {
        if (!std::isfinite(settings.grid_size) || settings.grid_size <= 0.0F)
            settings.grid_size = 0.25F;
        if (!std::isfinite(settings.tolerance) || settings.tolerance < 0.0F)
            settings.tolerance = 0.18F;
        snap_ = settings;
    }
    [[nodiscard]] const EditorSelection& selection() const noexcept { return selection_; }
    [[nodiscard]] const EditorSelectionSet& selections() const noexcept { return selections_; }
    void clear_selection() noexcept { selection_ = {}; selections_.clear(); }
    void set_selection(EditorSelectionSet selections) {
        selections_ = std::move(selections);
        selection_ = selections_.empty() ? EditorSelection{} : selections_.back();
    }
    [[nodiscard]] bool is_locked(const StableId& id) const { return locked_.contains(id); }
    [[nodiscard]] bool is_visible(const StableId& id) const { return !hidden_.contains(id); }
    [[nodiscard]] bool dirty() const noexcept { return dirty_; }
    void mark_saved() noexcept { dirty_ = false; }
    void set_autosave_path(std::filesystem::path path) { autosave_path_ = std::move(path); }
    [[nodiscard]] const std::optional<std::filesystem::path>& autosave_path() const noexcept { return autosave_path_; }
    void set_autosave_interval(std::chrono::milliseconds interval) noexcept {
        autosave_interval_ = std::max(interval, std::chrono::milliseconds{1});
    }
    [[nodiscard]] const std::vector<Material>& materials() const noexcept { return materials_; }
    void set_materials(std::vector<Material> materials) { materials_ = std::move(materials); }
    [[nodiscard]] bool autosave_due(std::chrono::steady_clock::time_point now = std::chrono::steady_clock::now()) const noexcept {
        return dirty_ && autosave_path_.has_value() && now - last_save_ >= autosave_interval_;
    }
    bool tick_autosave(std::chrono::steady_clock::time_point now = std::chrono::steady_clock::now()) {
        if (!autosave_due(now)) return false;
        return save_project(*autosave_path_);
    }
    [[nodiscard]] CommandHistory& history() noexcept { return history_; }
    [[nodiscard]] const CommandHistory& history() const noexcept { return history_; }

    [[nodiscard]] Point2 snap_point(Point2 point, std::optional<StableId> exclude = std::nullopt) const {
        Point2 result = point;
        float best = snap_.tolerance;
        if (snap_.grid && snap_.grid_size > 0.0F) {
            const Point2 grid{std::round(point.x / snap_.grid_size) * snap_.grid_size,
                              std::round(point.y / snap_.grid_size) * snap_.grid_size};
            result = grid;
        }
        if (snap_.endpoints || snap_.corners) {
            for (const auto& wall : room_.walls) {
                if (exclude && wall.id == *exclude) continue;
                for (const Point2 endpoint : {wall.start, wall.end}) {
                    const float d = detail::distance(point, endpoint);
                    if (d <= best) { result = endpoint; best = d; }
                }
            }
        }
        return result;
    }

    [[nodiscard]] std::optional<WallHit> hit_wall(Point2 point, float tolerance = 0.2F) const {
        std::optional<WallHit> result;
        for (const auto& wall : room_.walls) {
            const Point2 delta{wall.end.x - wall.start.x, wall.end.y - wall.start.y};
            const float length_sq = delta.x * delta.x + delta.y * delta.y;
            if (length_sq <= std::numeric_limits<float>::epsilon()) continue;
            const float t = detail::clamp01(((point.x - wall.start.x) * delta.x + (point.y - wall.start.y) * delta.y) / length_sq);
            const Point2 closest = detail::lerp(wall.start, wall.end, t);
            const float distance = detail::distance(point, closest);
            if (distance <= tolerance && (!result || distance < result->distance)) result = WallHit{wall.id, distance, t * std::sqrt(length_sq)};
        }
        return result;
    }

    bool select(Point2 point, float tolerance = 0.25F) {
        for (auto item = room_.furniture.rbegin(); item != room_.furniture.rend(); ++item) {
            const FurnitureFootprint footprint = furniture_footprint_corners(*item);
            if (point_in_or_on_polygon(point, footprint) &&
                select_item({EditorSelectionType::Furniture, item->id}, false))
                return true;
        }
        for (const auto& wall : room_.walls) {
            const float wall_length = detail::distance(wall.start, wall.end);
            if (wall_length <= 0.0F) continue;
            const auto on_wall = [&](float offset, float width) {
                const Point2 a = detail::lerp(wall.start, wall.end, offset / wall_length);
                const Point2 b = detail::lerp(wall.start, wall.end, (offset + width) / wall_length);
                return detail::distance_to_segment(point, a, b) <= tolerance;
            };
            for (const auto& opening : room_.doors) if (opening.wall_id == wall.id && on_wall(opening.offset, opening.width)) {
                if (select_item({EditorSelectionType::Door, opening.id}, false)) return true;
            }
            for (const auto& opening : room_.windows) if (opening.wall_id == wall.id && on_wall(opening.offset, opening.width)) {
                if (select_item({EditorSelectionType::Window, opening.id}, false)) return true;
            }
        }
        if (const auto hit = hit_wall(point, tolerance)) return select_item({EditorSelectionType::Wall, hit->id}, false);
        clear_selection();
        return false;
    }

    bool select(Point2 point, bool additive, float tolerance = 0.25F) {
        for (auto item = room_.furniture.rbegin(); item != room_.furniture.rend(); ++item) {
            const FurnitureFootprint footprint = furniture_footprint_corners(*item);
            if (point_in_or_on_polygon(point, footprint) &&
                select_item({EditorSelectionType::Furniture, item->id}, additive))
                return true;
        }
        for (const auto& wall : room_.walls) {
            const float length = detail::distance(wall.start, wall.end);
            if (length <= std::numeric_limits<float>::epsilon()) continue;
            const auto on_wall = [&](float offset, float width) {
                const Point2 start = detail::lerp(wall.start, wall.end, offset / length);
                const Point2 end =
                    detail::lerp(wall.start, wall.end, (offset + width) / length);
                return detail::distance_to_segment(point, start, end) <= tolerance;
            };
            for (const auto& opening : room_.doors)
                if (opening.wall_id == wall.id && on_wall(opening.offset, opening.width) &&
                    select_item({EditorSelectionType::Door, opening.id}, additive))
                    return true;
            for (const auto& opening : room_.windows)
                if (opening.wall_id == wall.id && on_wall(opening.offset, opening.width) &&
                    select_item({EditorSelectionType::Window, opening.id}, additive))
                    return true;
        }
        if (const auto hit = hit_wall(point, tolerance)) return select_item({EditorSelectionType::Wall, hit->id}, additive);
        if (!additive) clear_selection();
        return false;
    }

    void select_all() {
        selections_.clear();
        for (const auto& wall : room_.walls) if (is_visible(wall.id) && !is_locked(wall.id)) selections_.push_back({EditorSelectionType::Wall, wall.id});
        for (const auto& door : room_.doors) if (is_visible(door.id) && !is_locked(door.id)) selections_.push_back({EditorSelectionType::Door, door.id});
        for (const auto& window : room_.windows) if (is_visible(window.id) && !is_locked(window.id)) selections_.push_back({EditorSelectionType::Window, window.id});
        for (const auto& item : room_.furniture) if (is_visible(item.id) && !is_locked(item.id)) selections_.push_back({EditorSelectionType::Furniture, item.id});
        selection_ = selections_.empty() ? EditorSelection{} : selections_.back();
    }

    bool draw_wall(Point2 start, Point2 end, float thickness = 0.2F, float height = 2.5F) {
        if (!std::isfinite(start.x) || !std::isfinite(start.y) || !std::isfinite(end.x) ||
            !std::isfinite(end.y) || !std::isfinite(thickness) ||
            !std::isfinite(height) || thickness <= 0.0F || height <= 0.0F)
            return false;
        start = snap_point(start);
        end = snap_point(end);
        if (detail::distance(start, end) <= 0.0001F) return false;
        Room after = room_;
        after.walls.push_back({detail::next_id(room_, "wall"), start, end, thickness, height, {}});
        commit(std::make_unique<RoomSnapshotCommand>(room_, room_, std::move(after), "Draw wall"));
        set_selection({{EditorSelectionType::Wall, room_.walls.back().id}});
        return true;
    }

    bool move_endpoint(const StableId& wall_id, bool start, Point2 point) {
        const auto it = std::find_if(room_.walls.begin(), room_.walls.end(), [&](const auto& wall) { return wall.id == wall_id; });
        if (it == room_.walls.end() || is_locked(wall_id) || !std::isfinite(point.x) ||
            !std::isfinite(point.y))
            return false;
        Room after = room_;
        auto& wall = *std::find_if(after.walls.begin(), after.walls.end(), [&](const auto& value) { return value.id == wall_id; });
        Point2 snapped = snap_point(point, wall_id);
        if (start) wall.start = snapped; else wall.end = snapped;
        if (detail::distance(wall.start, wall.end) <= 0.0001F ||
            !detail::wall_openings_fit(after, wall))
            return false;
        commit(std::make_unique<RoomSnapshotCommand>(room_, room_, std::move(after), "Move wall endpoint"));
        set_selection({{EditorSelectionType::Wall, wall_id}});
        return true;
    }

    bool set_wall_length(const StableId& wall_id, float length) {
        const auto it = std::find_if(room_.walls.begin(), room_.walls.end(), [&](const auto& wall) { return wall.id == wall_id; });
        if (it == room_.walls.end() || is_locked(wall_id) || !std::isfinite(length) || length <= 0.0F) return false;
        const float old_length = detail::distance(it->start, it->end);
        if (old_length <= 0.0001F) return false;
        Room after = room_;
        auto& wall = *std::find_if(after.walls.begin(), after.walls.end(), [&](const auto& value) { return value.id == wall_id; });
        const Point2 direction{(wall.end.x - wall.start.x) / old_length, (wall.end.y - wall.start.y) / old_length};
        wall.end = {wall.start.x + direction.x * length, wall.start.y + direction.y * length};
        if (!detail::wall_openings_fit(after, wall)) return false;
        commit(std::make_unique<RoomSnapshotCommand>(room_, room_, std::move(after), "Set wall dimension"));
        set_selection({{EditorSelectionType::Wall, wall_id}});
        return true;
    }

    bool set_opening_width(const StableId& opening_id, float width) {
        if (is_locked(opening_id) || !std::isfinite(width) || width <= 0.0F) return false;
        Room after = room_;
        auto door = std::find_if(after.doors.begin(), after.doors.end(), [&](const auto& x) { return x.id == opening_id; });
        if (door != after.doors.end()) {
            if (!detail::opening_fits(after, door->wall_id, door->offset, width,
                                      door->bottom, door->height, opening_id))
                return false;
            door->width = width;
        } else {
            auto window = std::find_if(after.windows.begin(), after.windows.end(), [&](const auto& x) { return x.id == opening_id; });
            if (window == after.windows.end()) return false;
            if (!detail::opening_fits(after, window->wall_id, window->offset, width,
                                      window->bottom, window->height, opening_id))
                return false;
            window->width = width;
        }
        const bool is_door = door != after.doors.end();
        commit(std::make_unique<RoomSnapshotCommand>(room_, room_, std::move(after), "Set opening dimension"));
        set_selection({{is_door ? EditorSelectionType::Door : EditorSelectionType::Window, opening_id}});
        return true;
    }

    bool delete_selection() {
        const auto selected = selections_or_single();
        if (selected.empty()) return false;
        for (const auto& item : selected) if (is_locked(item.id)) return false;
        Room after = room_;
        for (const auto& item : selected) {
            if (item.type == EditorSelectionType::Wall) {
                after.walls.erase(std::remove_if(after.walls.begin(), after.walls.end(), [&](const auto& x) { return x.id == item.id; }), after.walls.end());
                after.doors.erase(std::remove_if(after.doors.begin(), after.doors.end(), [&](const auto& x) { return x.wall_id == item.id; }), after.doors.end());
                after.windows.erase(std::remove_if(after.windows.begin(), after.windows.end(), [&](const auto& x) { return x.wall_id == item.id; }), after.windows.end());
            } else if (item.type == EditorSelectionType::Door) after.doors.erase(std::remove_if(after.doors.begin(), after.doors.end(), [&](const auto& x) { return x.id == item.id; }), after.doors.end());
            else if (item.type == EditorSelectionType::Window) after.windows.erase(std::remove_if(after.windows.begin(), after.windows.end(), [&](const auto& x) { return x.id == item.id; }), after.windows.end());
            else if (item.type == EditorSelectionType::Furniture) after.furniture.erase(std::remove_if(after.furniture.begin(), after.furniture.end(), [&](const auto& x) { return x.id == item.id; }), after.furniture.end());
        }
        if (after.walls.size() == room_.walls.size() && after.doors.size() == room_.doors.size() && after.windows.size() == room_.windows.size() && after.furniture.size() == room_.furniture.size()) return false;
        commit(std::make_unique<RoomSnapshotCommand>(room_, room_, std::move(after), "Delete selection"));
        clear_selection();
        return true;
    }

    bool place_door(const StableId& wall_id, float offset, float width = 0.9F) { return place_opening(wall_id, offset, width, true); }
    bool place_window(const StableId& wall_id, float offset, float width = 1.2F) { return place_opening(wall_id, offset, width, false); }

    bool set_wall_properties(const StableId& id, float thickness, float height) {
        if (!std::isfinite(thickness) || !std::isfinite(height) || thickness <= 0.0F || height <= 0.0F) return false;
        Room after = room_;
        const auto it = std::find_if(after.walls.begin(), after.walls.end(), [&](const auto& x) { return x.id == id; });
        if (it == after.walls.end() || is_locked(id)) return false;
        it->thickness = thickness; it->height = height;
        if (!detail::wall_openings_fit(after, *it)) return false;
        commit(std::make_unique<RoomSnapshotCommand>(room_, room_, std::move(after), "Edit wall properties"));
        set_selection({{EditorSelectionType::Wall, id}}); return true;
    }

    bool set_opening_properties(const StableId& id, float width, float bottom, float height) {
        if (!std::isfinite(width) || !std::isfinite(bottom) || !std::isfinite(height) || width <= 0.0F || bottom < 0.0F || height <= 0.0F) return false;
        Room after = room_;
        auto door = std::find_if(after.doors.begin(), after.doors.end(), [&](const auto& x) { return x.id == id; });
        auto window = std::find_if(after.windows.begin(), after.windows.end(), [&](const auto& x) { return x.id == id; });
        const auto wall_id = door != after.doors.end() ? door->wall_id : window != after.windows.end() ? window->wall_id : StableId{};
        const auto wall = std::find_if(after.walls.begin(), after.walls.end(), [&](const auto& x) { return x.id == wall_id; });
        if (wall == after.walls.end() || is_locked(id) || is_locked(wall_id) ||
            !detail::opening_fits(after, wall_id,
                                  door != after.doors.end() ? door->offset : window->offset,
                                  width, bottom, height, id))
            return false;
        if (door != after.doors.end()) { door->width = width; door->bottom = bottom; door->height = height; }
        else if (window != after.windows.end()) { window->width = width; window->bottom = bottom; window->height = height; }
        else return false;
        const bool is_door = door != after.doors.end();
        commit(std::make_unique<RoomSnapshotCommand>(room_, room_, std::move(after), "Edit opening properties"));
        set_selection({{is_door ? EditorSelectionType::Door : EditorSelectionType::Window, id}}); return true;
    }

    bool set_furniture_properties(const StableId& id, Transform transform, Vec3 dimensions) {
        const Quaternion rotation = transform.rotation;
        const float rotation_length_squared =
            rotation.w * rotation.w + rotation.x * rotation.x +
            rotation.y * rotation.y + rotation.z * rotation.z;
        if (!detail::positive_vec3(dimensions) ||
            !detail::finite_vec3(transform.position) ||
            !detail::positive_vec3(transform.scale) ||
            !detail::finite_quaternion(rotation) ||
            !std::isfinite(rotation_length_squared) ||
            std::fabs(rotation_length_squared - 1.0F) > 0.001F || is_locked(id))
            return false;
        Room after = room_;
        const auto it = std::find_if(after.furniture.begin(), after.furniture.end(), [&](const auto& x) { return x.id == id; });
        if (it == after.furniture.end()) return false;
        it->transform = transform; it->dimensions = dimensions;
        commit(std::make_unique<RoomSnapshotCommand>(room_, room_, std::move(after), "Edit furniture properties"));
        set_selection({{EditorSelectionType::Furniture, id}}); return true;
    }

    bool add_furniture(Furniture furniture) {
        if (furniture.id.empty()) furniture.id = detail::next_id(room_, "furniture");
        if (detail::catalog_id_in_use(room_, furniture.id) ||
            !detail::positive_vec3(furniture.dimensions) ||
            !is_valid_placement_transform(furniture.transform))
            return false;
        const double rotation_length_squared =
            placement_detail::quaternion_norm_squared(furniture.transform.rotation);
        if (!std::isfinite(rotation_length_squared) ||
            std::fabs(rotation_length_squared - 1.0) > 0.001)
            return false;
        Room after = room_;
        after.furniture.push_back(std::move(furniture));
        const StableId id = after.furniture.back().id;
        commit(std::make_unique<RoomSnapshotCommand>(room_, room_, std::move(after),
                                                     "Place furniture"));
        set_selection({{EditorSelectionType::Furniture, id}});
        return true;
    }

    bool toggle_lock(const StableId& id) { return toggle_flag(locked_, id, "Toggle lock"); }
    bool toggle_visibility(const StableId& id) { return toggle_flag(hidden_, id, "Toggle visibility"); }
    bool lock_selection() { return set_flag_for_selection(locked_, true, "Lock selection"); }
    bool unlock_selection() { return set_flag_for_selection(locked_, false, "Unlock selection"); }
    bool hide_selection() { return set_flag_for_selection(hidden_, true, "Hide selection"); }
    bool show_selection() { return set_flag_for_selection(hidden_, false, "Show selection"); }

    bool copy_selection() {
        clipboard_ = Room{};
        for (const auto& selected : selections_or_single()) {
            if (selected.type == EditorSelectionType::Wall) {
                auto it = std::find_if(room_.walls.begin(), room_.walls.end(), [&](const auto& x) { return x.id == selected.id; });
                if (it != room_.walls.end()) clipboard_.walls.push_back(*it);
            } else if (selected.type == EditorSelectionType::Door) {
                auto it = std::find_if(room_.doors.begin(), room_.doors.end(), [&](const auto& x) { return x.id == selected.id; });
                if (it != room_.doors.end()) clipboard_.doors.push_back(*it);
            } else if (selected.type == EditorSelectionType::Window) {
                auto it = std::find_if(room_.windows.begin(), room_.windows.end(), [&](const auto& x) { return x.id == selected.id; });
                if (it != room_.windows.end()) clipboard_.windows.push_back(*it);
            } else if (selected.type == EditorSelectionType::Furniture) {
                auto it = std::find_if(room_.furniture.begin(), room_.furniture.end(), [&](const auto& x) { return x.id == selected.id; });
                if (it != room_.furniture.end()) clipboard_.furniture.push_back(*it);
            }
        }
        return !clipboard_.walls.empty() || !clipboard_.doors.empty() || !clipboard_.windows.empty() || !clipboard_.furniture.empty();
    }

    bool paste(Point2 offset = {0.25F, 0.25F}) {
        if (clipboard_.walls.empty() && clipboard_.doors.empty() &&
            clipboard_.windows.empty() && clipboard_.furniture.empty())
            return false;
        if (!std::isfinite(offset.x) || !std::isfinite(offset.y)) return false;

        Room after = room_;
        EditorSelectionSet pasted;
        std::unordered_map<StableId, StableId> wall_ids;
        for (auto wall : clipboard_.walls) {
            const StableId original_id = wall.id;
            wall.id = detail::next_id(after, "wall");
            wall_ids.emplace(original_id, wall.id);
            wall.start.x += offset.x;
            wall.end.x += offset.x;
            wall.start.y += offset.y;
            wall.end.y += offset.y;
            after.walls.push_back(wall);
            pasted.push_back({EditorSelectionType::Wall, wall.id});
        }
        for (auto door : clipboard_.doors) {
            if (const auto mapped = wall_ids.find(door.wall_id); mapped != wall_ids.end())
                door.wall_id = mapped->second;
            door.id = detail::next_id(after, "door");
            if (!detail::opening_fits(after, door.wall_id, door.offset, door.width,
                                      door.bottom, door.height))
                return false;
            after.doors.push_back(door);
            pasted.push_back({EditorSelectionType::Door, door.id});
        }
        for (auto window : clipboard_.windows) {
            if (const auto mapped = wall_ids.find(window.wall_id); mapped != wall_ids.end())
                window.wall_id = mapped->second;
            window.id = detail::next_id(after, "window");
            if (!detail::opening_fits(after, window.wall_id, window.offset, window.width,
                                      window.bottom, window.height))
                return false;
            after.windows.push_back(window);
            pasted.push_back({EditorSelectionType::Window, window.id});
        }
        for (auto item : clipboard_.furniture) {
            item.id = detail::next_id(after, "furniture");
            item.transform.position.x += offset.x;
            item.transform.position.z += offset.y;
            after.furniture.push_back(item);
            pasted.push_back({EditorSelectionType::Furniture, item.id});
        }
        commit(std::make_unique<RoomSnapshotCommand>(room_, room_, std::move(after),
                                                     "Paste"));
        set_selection(std::move(pasted));
        return true;
    }
    bool duplicate_selection() { return copy_selection() && paste(); }
    bool copy() { return copy_selection(); }
    bool duplicate() { return duplicate_selection(); }
    bool undo() { const bool changed = history_.undo(); dirty_ = changed || dirty_; return changed; }
    bool redo() { const bool changed = history_.redo(); dirty_ = changed || dirty_; return changed; }
    bool save(const std::filesystem::path& path) { return save_project(path); }
    bool open(const std::filesystem::path& path) { return open_project(path); }

    bool handle_shortcut(EditorKey key) {
        switch (key) { case EditorKey::Undo: return undo(); case EditorKey::Redo: return redo(); case EditorKey::Copy: return copy_selection(); case EditorKey::Paste: return paste(); case EditorKey::Duplicate: return duplicate_selection(); case EditorKey::Delete: return delete_selection(); case EditorKey::SelectAll: select_all(); return true; case EditorKey::ToggleLock: return toggle_selection_flag(locked_, "Toggle lock"); case EditorKey::ToggleVisibility: return toggle_selection_flag(hidden_, "Toggle visibility"); case EditorKey::Save: return autosave_path_.has_value() && save_project(*autosave_path_); }
        return false;
    }

    bool save_project(const std::filesystem::path& path) {
        RoomDesign design;
        design.materials = materials_;
        design.rooms.push_back(room_);
        if (!design.valid()) return false;

        MemoryArchive archive;
        serialize(design, archive);
        const std::string payload = archive.read_string("room_design");
        const std::filesystem::path temporary = path.string() + ".tmp";
        std::ofstream out(temporary, std::ios::binary | std::ios::trunc);
        if (!out) return false;
        out.write(payload.data(), static_cast<std::streamsize>(payload.size()));
        out.flush();
        if (!out) {
            out.close();
            std::error_code ignored;
            std::filesystem::remove(temporary, ignored);
            return false;
        }
        out.close();
        if (!out) {
            std::error_code ignored;
            std::filesystem::remove(temporary, ignored);
            return false;
        }

        std::error_code error;
        std::filesystem::rename(temporary, path, error);
        if (error) {
            std::error_code ignored;
            std::filesystem::remove(temporary, ignored);
            return false;
        }
        dirty_ = false;
        last_save_ = std::chrono::steady_clock::now();
        return true;
    }

    bool open_project(const std::filesystem::path& path) {
        std::ifstream in(path, std::ios::binary);
        if (!in) return false;
        const std::string payload((std::istreambuf_iterator<char>(in)), {});
        if (!in.eof() && in.fail()) return false;
        MemoryArchive archive;
        archive.write_string("room_design", payload);
        const auto design = deserialize(archive);
        if (!design || design->rooms.size() != 1 || !design->valid()) return false;

        room_ = design->rooms.front();
        materials_ = design->materials;
        history_.clear();
        clear_selection();
        locked_.clear();
        hidden_.clear();
        dirty_ = false;
        last_save_ = std::chrono::steady_clock::now();
        return true;
    }

    [[nodiscard]] std::vector<ValidationIssue> validate() const {
        RoomDesign design;
        design.rooms.push_back(room_);
        auto issues = design.validate();
        for (const auto& wall : room_.walls) {
            for (const auto& other : room_.walls) if (wall.id < other.id && wall.start == other.start && wall.end == other.end)
                issues.push_back({"room.walls", "duplicate wall geometry: " + wall.id + " and " + other.id});
        }
        return issues;
    }

private:
    void commit(std::unique_ptr<EditCommand> command) { history_.execute(std::move(command)); dirty_ = true; }

    bool select_item(EditorSelection item, bool additive) {
        if (is_locked(item.id) || !is_visible(item.id)) return false;
        if (!additive) selections_.clear();
        if (std::find(selections_.begin(), selections_.end(), item) == selections_.end()) selections_.push_back(item);
        selection_ = item; return true;
    }

    [[nodiscard]] EditorSelectionSet selections_or_single() const {
        return selections_.empty() && selection_.type != EditorSelectionType::None ? EditorSelectionSet{selection_} : selections_;
    }

    bool toggle_flag(std::unordered_set<StableId>& flags, const StableId& id, std::string label) {
        if (id.empty()) return false;
        auto before = flags; if (!flags.insert(id).second) flags.erase(id); auto after = flags;
        flags = before; commit(std::make_unique<SetSnapshotCommand>(flags, std::move(before), std::move(after), std::move(label))); return true;
    }

    bool set_flag_for_selection(std::unordered_set<StableId>& flags, bool value, std::string label) {
        const auto selected = selections_or_single(); if (selected.empty()) return false;
        auto before = flags; for (const auto& item : selected) { if (value) flags.insert(item.id); else flags.erase(item.id); }
        if (before == flags) return false; auto after = flags; flags = before;
        commit(std::make_unique<SetSnapshotCommand>(flags, std::move(before), std::move(after), std::move(label))); return true;
    }

    bool toggle_selection_flag(std::unordered_set<StableId>& flags, std::string label) {
        const auto selected = selections_or_single(); if (selected.empty()) return false;
        auto before = flags; for (const auto& item : selected) { if (!flags.insert(item.id).second) flags.erase(item.id); }
        auto after = flags; flags = before; commit(std::make_unique<SetSnapshotCommand>(flags, std::move(before), std::move(after), std::move(label))); return true;
    }

    bool place_opening(const StableId& wall_id, float offset, float width, bool door) {
        const auto wall = std::find_if(room_.walls.begin(), room_.walls.end(), [&](const auto& x) { return x.id == wall_id; });
        if (wall == room_.walls.end() || is_locked(wall_id) || !std::isfinite(offset) || !std::isfinite(width) || width <= 0.0F) return false;
        const float wall_length = detail::distance(wall->start, wall->end);
        offset = std::clamp(offset, 0.0F, wall_length);
        const float bottom = door ? 0.0F : 0.9F;
        const float height = door ? 2.1F : 1.2F;
        if (!detail::opening_fits(room_, wall_id, offset, width, bottom, height))
            return false;
        Room after = room_;
        if (door)
            after.doors.push_back(Door{detail::next_id(room_, "door"), wall_id, offset,
                                       width, bottom, height, false, {}});
        else
            after.windows.push_back(Window{detail::next_id(room_, "window"), wall_id,
                                           offset, width, bottom, height, {}});
        commit(std::make_unique<RoomSnapshotCommand>(room_, room_, std::move(after), door ? "Place door" : "Place window"));
        set_selection({{door ? EditorSelectionType::Door : EditorSelectionType::Window, door ? room_.doors.back().id : room_.windows.back().id}});
        return true;
    }

    Room& room_;
    std::vector<Material> materials_;
    SnapSettings snap_{};
    EditorSelection selection_{};
    EditorSelectionSet selections_;
    std::unordered_set<StableId> locked_;
    std::unordered_set<StableId> hidden_;
    Room clipboard_;
    CommandHistory history_;
    bool dirty_ = false;
    std::optional<std::filesystem::path> autosave_path_;
    std::chrono::milliseconds autosave_interval_{1000};
    std::chrono::steady_clock::time_point last_save_ = std::chrono::steady_clock::now();
};

}  // namespace room_engine
