#pragma once

#include "room_engine/core/room_design.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <limits>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace room_engine {

enum class EditorSelectionType { None, Wall, Door, Window };
struct EditorSelection {
    EditorSelectionType type = EditorSelectionType::None;
    StableId id;
    friend bool operator==(const EditorSelection&, const EditorSelection&) = default;
};

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
};

class CommandHistory {
public:
    void execute(std::unique_ptr<EditCommand> command) {
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

private:
    std::vector<std::unique_ptr<EditCommand>> commands_;
    std::size_t cursor_ = 0;
};

namespace detail {
inline float distance(Point2 a, Point2 b) { return std::hypot(a.x - b.x, a.y - b.y); }
inline Point2 lerp(Point2 a, Point2 b, float t) { return {a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t}; }
inline float clamp01(float value) { return std::clamp(value, 0.0F, 1.0F); }
inline StableId next_id(const Room& room, std::string prefix) {
    for (std::size_t i = 1;; ++i) {
        StableId candidate = prefix + "-" + std::to_string(i);
        const bool used = std::any_of(room.walls.begin(), room.walls.end(), [&](const auto& x) { return x.id == candidate; }) ||
                          std::any_of(room.doors.begin(), room.doors.end(), [&](const auto& x) { return x.id == candidate; }) ||
                          std::any_of(room.windows.begin(), room.windows.end(), [&](const auto& x) { return x.id == candidate; });
        if (!used) return candidate;
    }
}
}  // namespace detail

class RoomSnapshotCommand final : public EditCommand {
public:
    RoomSnapshotCommand(Room& room, Room before, Room after, std::string label)
        : room_(room), before_(std::move(before)), after_(std::move(after)), label_(std::move(label)) {}
    void redo() override { room_ = after_; }
    void undo() override { room_ = before_; }
    [[nodiscard]] std::string name() const override { return label_; }

private:
    Room& room_;
    Room before_;
    Room after_;
    std::string label_;
};

class FloorPlanEditor {
public:
    explicit FloorPlanEditor(Room& room) : room_(room) {}

    [[nodiscard]] const Room& room() const noexcept { return room_; }
    [[nodiscard]] Room& room() noexcept { return room_; }
    [[nodiscard]] const SnapSettings& snapping() const noexcept { return snap_; }
    void set_snapping(SnapSettings settings) noexcept { snap_ = settings; }
    [[nodiscard]] const EditorSelection& selection() const noexcept { return selection_; }
    void clear_selection() noexcept { selection_ = {}; }
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
        for (const auto& wall : room_.walls) {
            const float wall_length = detail::distance(wall.start, wall.end);
            if (wall_length <= 0.0F) continue;
            const auto on_wall = [&](float offset, float width) {
                const Point2 a = detail::lerp(wall.start, wall.end, offset / wall_length);
                const Point2 b = detail::lerp(wall.start, wall.end, (offset + width) / wall_length);
                return detail::distance(point, a) <= tolerance || detail::distance(point, b) <= tolerance;
            };
            for (const auto& opening : room_.doors) if (opening.wall_id == wall.id && on_wall(opening.offset, opening.width)) {
                selection_ = {EditorSelectionType::Door, opening.id}; return true;
            }
            for (const auto& opening : room_.windows) if (opening.wall_id == wall.id && on_wall(opening.offset, opening.width)) {
                selection_ = {EditorSelectionType::Window, opening.id}; return true;
            }
        }
        if (const auto hit = hit_wall(point, tolerance)) { selection_ = {EditorSelectionType::Wall, hit->id}; return true; }
        selection_ = {};
        return false;
    }

    bool draw_wall(Point2 start, Point2 end, float thickness = 0.2F, float height = 2.5F) {
        start = snap_point(start);
        end = snap_point(end);
        if (detail::distance(start, end) <= 0.0001F) return false;
        Room after = room_;
        after.walls.push_back({detail::next_id(room_, "wall"), start, end, thickness, height, {}});
        history_.execute(std::make_unique<RoomSnapshotCommand>(room_, room_, std::move(after), "Draw wall"));
        selection_ = {EditorSelectionType::Wall, room_.walls.back().id};
        return true;
    }

    bool move_endpoint(const StableId& wall_id, bool start, Point2 point) {
        const auto it = std::find_if(room_.walls.begin(), room_.walls.end(), [&](const auto& wall) { return wall.id == wall_id; });
        if (it == room_.walls.end()) return false;
        Room after = room_;
        auto& wall = *std::find_if(after.walls.begin(), after.walls.end(), [&](const auto& value) { return value.id == wall_id; });
        Point2 snapped = snap_point(point, wall_id);
        if (start) wall.start = snapped; else wall.end = snapped;
        if (detail::distance(wall.start, wall.end) <= 0.0001F) return false;
        history_.execute(std::make_unique<RoomSnapshotCommand>(room_, room_, std::move(after), "Move wall endpoint"));
        selection_ = {EditorSelectionType::Wall, wall_id};
        return true;
    }

    bool set_wall_length(const StableId& wall_id, float length) {
        const auto it = std::find_if(room_.walls.begin(), room_.walls.end(), [&](const auto& wall) { return wall.id == wall_id; });
        if (it == room_.walls.end() || !std::isfinite(length) || length <= 0.0F) return false;
        const float old_length = detail::distance(it->start, it->end);
        if (old_length <= 0.0001F) return false;
        Room after = room_;
        auto& wall = *std::find_if(after.walls.begin(), after.walls.end(), [&](const auto& value) { return value.id == wall_id; });
        const Point2 direction{(wall.end.x - wall.start.x) / old_length, (wall.end.y - wall.start.y) / old_length};
        wall.end = {wall.start.x + direction.x * length, wall.start.y + direction.y * length};
        history_.execute(std::make_unique<RoomSnapshotCommand>(room_, room_, std::move(after), "Set wall dimension"));
        selection_ = {EditorSelectionType::Wall, wall_id};
        return true;
    }

    bool set_opening_width(const StableId& opening_id, float width) {
        if (!std::isfinite(width) || width <= 0.0F) return false;
        Room after = room_;
        auto door = std::find_if(after.doors.begin(), after.doors.end(), [&](const auto& x) { return x.id == opening_id; });
        if (door != after.doors.end()) {
            const auto wall = std::find_if(after.walls.begin(), after.walls.end(), [&](const auto& x) { return x.id == door->wall_id; });
            if (wall == after.walls.end() || door->offset + width > detail::distance(wall->start, wall->end)) return false;
            door->width = width;
        } else {
            auto window = std::find_if(after.windows.begin(), after.windows.end(), [&](const auto& x) { return x.id == opening_id; });
            if (window == after.windows.end()) return false;
            const auto wall = std::find_if(after.walls.begin(), after.walls.end(), [&](const auto& x) { return x.id == window->wall_id; });
            if (wall == after.walls.end() || window->offset + width > detail::distance(wall->start, wall->end)) return false;
            window->width = width;
        }
        const bool is_door = door != after.doors.end();
        history_.execute(std::make_unique<RoomSnapshotCommand>(room_, room_, std::move(after), "Set opening dimension"));
        selection_ = {is_door ? EditorSelectionType::Door : EditorSelectionType::Window, opening_id};
        return true;
    }

    bool delete_selection() {
        if (selection_.type == EditorSelectionType::None) return false;
        Room after = room_;
        if (selection_.type == EditorSelectionType::Wall) {
            after.walls.erase(std::remove_if(after.walls.begin(), after.walls.end(), [&](const auto& x) { return x.id == selection_.id; }), after.walls.end());
            after.doors.erase(std::remove_if(after.doors.begin(), after.doors.end(), [&](const auto& x) { return x.wall_id == selection_.id; }), after.doors.end());
            after.windows.erase(std::remove_if(after.windows.begin(), after.windows.end(), [&](const auto& x) { return x.wall_id == selection_.id; }), after.windows.end());
        } else if (selection_.type == EditorSelectionType::Door) after.doors.erase(std::remove_if(after.doors.begin(), after.doors.end(), [&](const auto& x) { return x.id == selection_.id; }), after.doors.end());
        else after.windows.erase(std::remove_if(after.windows.begin(), after.windows.end(), [&](const auto& x) { return x.id == selection_.id; }), after.windows.end());
        if (after.walls.size() == room_.walls.size() && after.doors.size() == room_.doors.size() && after.windows.size() == room_.windows.size()) return false;
        history_.execute(std::make_unique<RoomSnapshotCommand>(room_, room_, std::move(after), "Delete selection"));
        selection_ = {};
        return true;
    }

    bool place_door(const StableId& wall_id, float offset, float width = 0.9F) { return place_opening(wall_id, offset, width, true); }
    bool place_window(const StableId& wall_id, float offset, float width = 1.2F) { return place_opening(wall_id, offset, width, false); }

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
    bool place_opening(const StableId& wall_id, float offset, float width, bool door) {
        const auto wall = std::find_if(room_.walls.begin(), room_.walls.end(), [&](const auto& x) { return x.id == wall_id; });
        if (wall == room_.walls.end() || !std::isfinite(offset) || !std::isfinite(width) || width <= 0.0F) return false;
        const float wall_length = detail::distance(wall->start, wall->end);
        offset = std::clamp(offset, 0.0F, wall_length);
        if (offset + width > wall_length + 0.0001F) return false;
        Room after = room_;
        if (door) after.doors.push_back(Door{detail::next_id(room_, "door"), wall_id, offset, width, 0.0F, 2.1F, false, {}});
        else after.windows.push_back(Window{detail::next_id(room_, "window"), wall_id, offset, width, 0.9F, 1.2F, {}});
        history_.execute(std::make_unique<RoomSnapshotCommand>(room_, room_, std::move(after), door ? "Place door" : "Place window"));
        selection_ = {door ? EditorSelectionType::Door : EditorSelectionType::Window, door ? room_.doors.back().id : room_.windows.back().id};
        return true;
    }

    Room& room_;
    SnapSettings snap_{};
    EditorSelection selection_{};
    CommandHistory history_;
};

}  // namespace room_engine
