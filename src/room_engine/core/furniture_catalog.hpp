#pragma once

#include "room_engine/core/room_design.hpp"
#include "room_engine/renderer/renderer_3d.hpp"

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <optional>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace room_engine {

enum class FurnitureCategory { Seating, Tables, Storage, Beds, Lighting, Decor, Other };
enum class WallAlignment { Free, Flush, Centered };

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
inline bool finite(Vec3 v) { return std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z); }
inline bool positive(Vec3 v) { return finite(v) && v.x > 0.0F && v.y > 0.0F && v.z > 0.0F; }
inline float yaw(const Quaternion& q) { return std::atan2(2.0F * (q.w * q.y + q.x * q.z), 1.0F - 2.0F * (q.y * q.y + q.x * q.x)); }
inline Vec3 footprint(Vec3 dimensions, float angle) {
    const float c = std::fabs(std::cos(angle));
    const float s = std::fabs(std::sin(angle));
    return {dimensions.x * c + dimensions.z * s, dimensions.y, dimensions.x * s + dimensions.z * c};
}
inline bool overlap(const Furniture& a, Vec3 a_size, const Furniture& b, Vec3 b_size) {
    return std::fabs(a.transform.position.x - b.transform.position.x) < (a_size.x + b_size.x) * 0.5F &&
           std::fabs(a.transform.position.z - b.transform.position.z) < (a_size.z + b_size.z) * 0.5F;
}
inline StableId catalog_next_id(const Room& room, const StableId& base) {
    StableId id = base;
    std::size_t suffix = 1;
    while (std::any_of(room.furniture.begin(), room.furniture.end(), [&](const Furniture& item) { return item.id == id; })) id = base + "-" + std::to_string(suffix++);
    return id;
}
}  // namespace detail

inline bool FurnitureCatalog::add(FurnitureAssetMetadata metadata, std::string* error) {
    const auto fail = [&](std::string message) { if (error) *error = std::move(message); return false; };
    if (metadata.id.empty()) return fail("furniture asset ID must not be empty");
    if (metadata.name.empty()) return fail("furniture asset name must not be empty");
    if (metadata.model.empty()) return fail("furniture asset model path must not be empty");
    if (!detail::positive(metadata.default_scale)) return fail("furniture asset default scale must be positive and finite");
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
    if (!detail::positive(placement.scale)) { result.error = "furniture placement scale must be positive and finite"; return result; }
    const Vec3 dimensions = loaded.asset->bounds.valid() ? loaded.asset->bounds.dimensions() : metadata->bounds.dimensions();
    if (!detail::positive(dimensions)) { result.error = "furniture asset has no valid bounding box: " + asset_id; return result; }
    placement.scale = {placement.scale.x * metadata->default_scale.x, placement.scale.y * metadata->default_scale.y, placement.scale.z * metadata->default_scale.z};
    if (placement.align_to_wall || !placement.wall_id.empty()) {
        const auto wall = std::find_if(room.walls.begin(), room.walls.end(), [&](const WallSegment& item) { return item.id == placement.wall_id; });
        if (wall == room.walls.end()) { result.error = "wall alignment references unknown wall: " + placement.wall_id; return result; }
        const float length = std::hypot(wall->end.x - wall->start.x, wall->end.y - wall->start.y);
        if (!std::isfinite(placement.wall_offset) || placement.wall_offset < 0.0F || placement.wall_offset > length) { result.error = "wall offset is outside the wall"; return result; }
        const float angle = std::atan2(-(wall->end.y - wall->start.y), wall->end.x - wall->start.x);
        placement.rotation = Quaternion::from_axis_angle({0.0F, 1.0F, 0.0F}, angle) * placement.rotation;
        const float center = placement.wall_offset;
        const float depth = dimensions.z * placement.scale.z;
        const float offset = metadata->wall_alignment == WallAlignment::Free ? 0.0F : wall->thickness * 0.5F + depth * 0.5F;
        placement.position = {wall->start.x + std::cos(angle) * center + std::sin(angle) * offset, placement.position.y,
                              wall->start.y - std::sin(angle) * center + std::cos(angle) * offset};
    }
    Furniture item{furniture_id.empty() ? detail::catalog_next_id(room, asset_id) : std::move(furniture_id), metadata->name,
                    {placement.position, placement.rotation, placement.scale}, {dimensions.x * placement.scale.x, dimensions.y * placement.scale.y, dimensions.z * placement.scale.z}, {}};
    const Vec3 item_footprint = detail::footprint(item.dimensions, detail::yaw(item.transform.rotation));
    for (const auto& other : room.furniture) {
        const Vec3 other_size = detail::footprint(other.dimensions, detail::yaw(other.transform.rotation));
        if (detail::overlap(item, item_footprint, other, other_size)) result.warnings.push_back({other.id, "furniture overlaps '" + other.name + "'"});
    }
    room.furniture.push_back(item);
    result.furniture = std::move(item);
    return result;
}

}  // namespace room_engine
