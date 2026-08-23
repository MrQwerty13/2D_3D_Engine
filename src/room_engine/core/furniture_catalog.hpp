#pragma once

#include "room_engine/core/placement.hpp"
#include "room_engine/renderer/renderer_3d.hpp"

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <limits>
#include <optional>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace room_engine {

enum class FurnitureCategory { Seating, Tables, Storage, Beds, Lighting, Decor, Other };
enum class WallAlignment { Free, Flush, Centered };
enum class PlacementMode { Warn, Strict };

struct FurnitureAssetMetadata {
    StableId id;
    std::string name;
    std::filesystem::path model;
    std::filesystem::path thumbnail;
    FurnitureCategory category = FurnitureCategory::Other;
    Vec3 default_scale{1.0F, 1.0F, 1.0F};
    MeshAsset::Bounds bounds{};
    WallAlignment wall_alignment = WallAlignment::Free;
};

struct FurniturePlacement {
    Vec3 position{};
    Quaternion rotation{};
    Vec3 scale{1.0F, 1.0F, 1.0F};
    StableId wall_id;
    float wall_offset = 0.0F;
    bool align_to_wall = false;
    bool rest_on_floor = true;
    PlacementMode mode = PlacementMode::Warn;
};

struct PlacementWarning {
    StableId furniture_id;
    std::string message;
};

struct FurniturePlacementResult {
    std::optional<Furniture> furniture;
    std::vector<PlacementWarning> warnings;
    std::string error;
    [[nodiscard]] explicit operator bool() const noexcept { return furniture.has_value() && error.empty(); }
};

class FurnitureCatalog {
public:
    explicit FurnitureCatalog(GltfAssetCache& cache) : cache_(cache) {}

    [[nodiscard]] bool add(FurnitureAssetMetadata metadata, std::string* error = nullptr);
    [[nodiscard]] bool remove(const StableId& id) { load_results_.erase(id); return assets_.erase(id) != 0; }
    [[nodiscard]] const FurnitureAssetMetadata* find(const StableId& id) const;
    [[nodiscard]] std::vector<const FurnitureAssetMetadata*> list(std::optional<FurnitureCategory> category = {}) const;
    [[nodiscard]] const AssetLoadResult& load(const StableId& id) const;
    [[nodiscard]] FurniturePlacementResult place(Room& room, const StableId& asset_id,
                                                  FurniturePlacement placement, StableId furniture_id = {}) const;

private:
    GltfAssetCache& cache_;
    std::unordered_map<StableId, FurnitureAssetMetadata> assets_;
    mutable std::unordered_map<StableId, AssetLoadResult> load_results_;
};

namespace detail {
inline bool catalog_id_in_use(const Room& room, const StableId& id) {
    return room.id == id || (room.floor && room.floor->id == id) ||
           (room.ceiling && room.ceiling->id == id) ||
           std::any_of(room.walls.begin(), room.walls.end(),
                       [&](const auto& item) { return item.id == id; }) ||
           std::any_of(room.doors.begin(), room.doors.end(),
                       [&](const auto& item) { return item.id == id; }) ||
           std::any_of(room.windows.begin(), room.windows.end(),
                       [&](const auto& item) { return item.id == id; }) ||
           std::any_of(room.furniture.begin(), room.furniture.end(),
                       [&](const auto& item) { return item.id == id; });
}
inline StableId catalog_next_id(const Room& room, const StableId& base) {
    StableId id = base;
    std::size_t suffix = 1;
    while (catalog_id_in_use(room, id)) id = base + "-" + std::to_string(suffix++);
    return id;
}
}  // namespace detail

inline bool FurnitureCatalog::add(FurnitureAssetMetadata metadata, std::string* error) {
    const auto fail = [&](std::string message) { if (error) *error = std::move(message); return false; };
    if (metadata.id.empty()) return fail("furniture asset ID must not be empty");
    if (metadata.name.empty()) return fail("furniture asset name must not be empty");
    if (metadata.model.empty()) return fail("furniture asset model path must not be empty");
    if (!is_finite_vector(metadata.default_scale) || metadata.default_scale.x <= 0.0F ||
        metadata.default_scale.y <= 0.0F || metadata.default_scale.z <= 0.0F)
        return fail("furniture asset default scale must be positive and finite");
    if (assets_.contains(metadata.id)) return fail("duplicate furniture asset: " + metadata.id);
    assets_.emplace(metadata.id, std::move(metadata));
    return true;
}

inline const FurnitureAssetMetadata* FurnitureCatalog::find(const StableId& id) const {
    const auto it = assets_.find(id);
    return it == assets_.end() ? nullptr : &it->second;
}

inline std::vector<const FurnitureAssetMetadata*> FurnitureCatalog::list(std::optional<FurnitureCategory> category) const {
    std::vector<const FurnitureAssetMetadata*> result;
    for (const auto& [id, metadata] : assets_) if (!category || metadata.category == *category) result.push_back(&metadata);
    std::sort(result.begin(), result.end(), [](const auto* a, const auto* b) { return a->name < b->name; });
    return result;
}

inline const AssetLoadResult& FurnitureCatalog::load(const StableId& id) const {
    if (const auto it = load_results_.find(id); it != load_results_.end()) return it->second;
    const auto* metadata = find(id);
    const auto [it, inserted] = load_results_.emplace(id, metadata == nullptr ? AssetLoadResult::failure("unknown furniture asset: " + id) : cache_.load(metadata->model));
    static_cast<void>(inserted);
    return it->second;
}

inline FurniturePlacementResult FurnitureCatalog::place(Room& room, const StableId& asset_id,
                                                         FurniturePlacement placement, StableId furniture_id) const {
    FurniturePlacementResult result;
    const auto* metadata = find(asset_id);
    if (!metadata) { result.error = "unknown furniture asset: " + asset_id; return result; }
    const auto& loaded = load(asset_id);
    if (!loaded) { result.error = "unable to load furniture asset '" + asset_id + "': " + loaded.error; return result; }
    if (!is_finite_vector(placement.position) || !is_finite_vector(placement.scale) ||
        placement.scale.x <= 0.0F || placement.scale.y <= 0.0F ||
        placement.scale.z <= 0.0F || !is_finite_quaternion(placement.rotation)) {
        result.error = "furniture placement transform must be finite with positive scale";
        return result;
    }
    const double rotation_norm_squared =
        static_cast<double>(placement.rotation.w) * placement.rotation.w +
        static_cast<double>(placement.rotation.x) * placement.rotation.x +
        static_cast<double>(placement.rotation.y) * placement.rotation.y +
        static_cast<double>(placement.rotation.z) * placement.rotation.z;
    if (!std::isfinite(rotation_norm_squared) || rotation_norm_squared <= 0.0) {
        result.error = "furniture placement rotation must not be zero";
        return result;
    }
    const float inverse_rotation_norm =
        static_cast<float>(1.0 / std::sqrt(rotation_norm_squared));
    placement.rotation = {placement.rotation.w * inverse_rotation_norm,
                          placement.rotation.x * inverse_rotation_norm,
                          placement.rotation.y * inverse_rotation_norm,
                          placement.rotation.z * inverse_rotation_norm};
    placement.scale = {placement.scale.x * metadata->default_scale.x,
                       placement.scale.y * metadata->default_scale.y,
                       placement.scale.z * metadata->default_scale.z};
    if (!is_finite_vector(placement.scale) || placement.scale.x <= 0.0F ||
        placement.scale.y <= 0.0F || placement.scale.z <= 0.0F) {
        result.error = "combined furniture scale is outside the supported range";
        return result;
    }
    const Vec3 dimensions = loaded.asset->bounds.valid()
                                ? loaded.asset->bounds.dimensions()
                                : metadata->bounds.dimensions();
    if (!is_finite_vector(dimensions) || dimensions.x <= 0.0F || dimensions.y <= 0.0F ||
        dimensions.z <= 0.0F) {
        result.error = "furniture asset has no valid bounding box: " + asset_id;
        return result;
    }
    if (placement.rest_on_floor) {
        const float elevation = room.floor ? room.floor->elevation : 0.0F;
        const float world_height = dimensions.y * placement.scale.y;
        if (!std::isfinite(elevation) || !std::isfinite(world_height)) {
            result.error = "furniture floor placement is outside the supported range";
            return result;
        }
        placement.position.y = elevation + world_height * 0.5F;
    }

    const WallSegment* aligned_wall = nullptr;
    if (placement.align_to_wall || !placement.wall_id.empty()) {
        const auto wall = std::find_if(room.walls.begin(), room.walls.end(), [&](const WallSegment& item) { return item.id == placement.wall_id; });
        if (wall == room.walls.end()) { result.error = "wall alignment references unknown wall: " + placement.wall_id; return result; }
        const float length = std::hypot(wall->end.x - wall->start.x, wall->end.y - wall->start.y);
        if (!std::isfinite(length) || length <= 0.0001F ||
            !std::isfinite(placement.wall_offset) || placement.wall_offset < 0.0F ||
            placement.wall_offset > length) {
            result.error = "wall offset is outside the wall";
            return result;
        }
        const float angle = std::atan2(-(wall->end.y - wall->start.y), wall->end.x - wall->start.x);
        placement.rotation = Quaternion::from_axis_angle({0.0F, 1.0F, 0.0F}, angle) * placement.rotation;
        const float center = placement.wall_offset;
        const float depth = dimensions.z * placement.scale.z;
        const float offset = metadata->wall_alignment == WallAlignment::Flush
                                 ? wall->thickness * 0.5F + depth * 0.5F
                                 : 0.0F;
        placement.position = {wall->start.x + std::cos(angle) * center + std::sin(angle) * offset, placement.position.y,
                              wall->start.y - std::sin(angle) * center + std::cos(angle) * offset};
        aligned_wall = &*wall;
    }

    if (!furniture_id.empty() && detail::catalog_id_in_use(room, furniture_id)) {
        result.error = "duplicate furniture ID: " + furniture_id;
        return result;
    }
    Furniture item{furniture_id.empty() ? detail::catalog_next_id(room, asset_id)
                                        : std::move(furniture_id),
                   metadata->name,
                   {placement.position, placement.rotation, placement.scale},
                   dimensions,
                   {},
                   asset_id};
    if (aligned_wall != nullptr) {
        const FurnitureFootprint footprint = furniture_footprint_corners(item);
        const float wall_length = std::hypot(aligned_wall->end.x - aligned_wall->start.x,
                                             aligned_wall->end.y - aligned_wall->start.y);
        const Point2 direction{(aligned_wall->end.x - aligned_wall->start.x) / wall_length,
                               (aligned_wall->end.y - aligned_wall->start.y) / wall_length};
        float minimum = std::numeric_limits<float>::max();
        float maximum = std::numeric_limits<float>::lowest();
        for (const Point2 corner : footprint) {
            const float projected = (corner.x - aligned_wall->start.x) * direction.x +
                                    (corner.y - aligned_wall->start.y) * direction.y;
            minimum = std::min(minimum, projected);
            maximum = std::max(maximum, projected);
        }
        if (minimum < -placement_geometry_tolerance ||
            maximum > wall_length + placement_geometry_tolerance) {
            result.error = "wall-aligned furniture extends beyond the wall ends";
            return result;
        }
    }
    if (room.floor && !furniture_footprint_inside_floor(item, *room.floor))
        result.warnings.push_back({item.id, "furniture extends outside the room floor"});
    const Vec3 item_size = scaled_furniture_dimensions(item);
    for (const auto& other : room.furniture) {
        const Vec3 other_size = scaled_furniture_dimensions(other);
        const bool overlaps_vertically =
            std::fabs(item.transform.position.y - other.transform.position.y) <
            (item_size.y + other_size.y) * 0.5F;
        if (overlaps_vertically && furniture_footprints_overlap(item, other))
            result.warnings.push_back({other.id,
                                       "furniture overlaps '" + other.name + "'"});
    }
    if (placement.mode == PlacementMode::Strict && !result.warnings.empty()) {
        result.error = result.warnings.front().message;
        return result;
    }
    room.furniture.push_back(item);
    result.furniture = std::move(item);
    return result;
}

}  // namespace room_engine
