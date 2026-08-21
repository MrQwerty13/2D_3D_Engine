#pragma once

#include "room_engine/renderer/renderer.hpp"
#include "room_engine/renderer/viewport.hpp"
#include "room_engine/core/room_design.hpp"

#include <cstdint>
#include <filesystem>
#include <cstddef>
#include <unordered_map>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace room_engine {

struct Mesh {
    std::vector<Vertex> vertices;
    std::vector<std::uint32_t> indices;
    [[nodiscard]] bool valid() const noexcept {
        return !vertices.empty() && indices.size() >= 3 && indices.size() % 3 == 0;
    }
};

struct MeshAsset {
    std::vector<Mesh> meshes;
    std::vector<RenderMaterial> materials;
    struct Bounds {
        Vec3 minimum{};
        Vec3 maximum{};
        [[nodiscard]] bool valid() const noexcept { return minimum.x <= maximum.x && minimum.y <= maximum.y && minimum.z <= maximum.z; }
        [[nodiscard]] Vec3 dimensions() const noexcept { return maximum - minimum; }
    } bounds{};
    [[nodiscard]] bool valid() const noexcept { return !meshes.empty(); }
};

struct AssetLoadResult {
    std::optional<MeshAsset> asset;
    std::string error;
    [[nodiscard]] explicit operator bool() const noexcept { return asset.has_value(); }
    static AssetLoadResult failure(std::string message) { return {std::nullopt, std::move(message)}; }
};

[[nodiscard]] AssetLoadResult load_gltf(const std::filesystem::path& path);

// Caches both successful loads and failures. Keeping failures cached avoids repeatedly
// parsing a broken file while an editor panel is refreshing its catalog.
class GltfAssetCache {
public:
    [[nodiscard]] const AssetLoadResult& load(const std::filesystem::path& path);
    void clear() noexcept { entries_.clear(); }
    [[nodiscard]] std::size_t size() const noexcept { return entries_.size(); }

private:
    std::unordered_map<std::string, AssetLoadResult> entries_;
};

struct DirectionalLight {
    Vec3 direction{-0.4F, -1.0F, -0.3F};
    Color color{255, 255, 255, 255};
    float intensity = 1.0F;
};

struct PointLight {
    Vec3 position{0.0F, 2.0F, 0.0F};
    Color color{255, 255, 255, 255};
    float intensity = 1.0F;
    float range = 10.0F;
};

struct AmbientLight {
    Color color{255, 255, 255, 255};
    float intensity = 0.25F;
};

struct PresentationControls {
    float exposure = 1.0F;
    bool enable_point_lights = true;
};

struct Ray {
    Vec3 origin{};
    Vec3 direction{0.0F, 0.0F, -1.0F};
};

struct RayHit {
    std::uint64_t id = 0;
    float distance = 0.0F;
    Vec3 position{};
    Vec3 normal{};
};

struct MeshInstance {
    Mesh mesh;
    Transform transform{};
    RenderMaterial material{};
    std::uint64_t id = 0;
};

class Renderer3D {
public:
    void begin() noexcept { instances_.clear(); }
    void add_mesh(Mesh mesh, Transform transform = {}, RenderMaterial material = {},
                  std::uint64_t id = 0);
    void add_asset(const MeshAsset& asset, Transform transform = {}, std::uint64_t id = 0);
    void set_ambient_light(AmbientLight light) noexcept { ambient_ = light; }
    void set_directional_light(DirectionalLight light) noexcept { directional_ = light; }
    void add_point_light(PointLight light) { points_.push_back(light); }
    void clear_lights() noexcept { points_.clear(); }
    void set_presentation_controls(PresentationControls controls) noexcept { controls_ = controls; }
    [[nodiscard]] PresentationControls presentation_controls() const noexcept { return controls_; }
    // Rebuilds the scene only when the validated 2D design contains the requested room.
    [[nodiscard]] bool update_from_room(const RoomDesign& design, const StableId& room_id = {});
    void flush(Renderer& renderer, const Camera& camera);
    [[nodiscard]] std::optional<RayHit> raycast(ScreenPoint screen, const Camera& camera,
                                                float viewport_width, float viewport_height) const;
    [[nodiscard]] std::optional<std::uint64_t> select(ScreenPoint screen, const Camera& camera,
                                                      float viewport_width, float viewport_height) const;
    [[nodiscard]] const std::vector<MeshInstance>& instances() const noexcept { return instances_; }

private:
    [[nodiscard]] Ray screen_ray(ScreenPoint screen, const Camera& camera,
                                 float viewport_width, float viewport_height) const;
    std::vector<MeshInstance> instances_;
    AmbientLight ambient_{};
    DirectionalLight directional_{};
    std::vector<PointLight> points_;
    PresentationControls controls_{};
};

[[nodiscard]] Mesh make_room_floor(float width = 8.0F, float depth = 6.0F);
[[nodiscard]] Mesh make_room_wall(float length, float height, float thickness = 0.15F);
[[nodiscard]] Mesh make_room_wall(const WallSegment& wall, const std::vector<const Door*>& doors = {},
                                  const std::vector<const Window*>& windows = {});
[[nodiscard]] Mesh make_room_floor(const Floor& floor);
[[nodiscard]] Mesh make_room_ceiling(const Ceiling& ceiling);
[[nodiscard]] Mesh make_room_opening(float width, float height, float thickness);
[[nodiscard]] bool populate_room(Renderer3D& renderer, const RoomDesign& design,
                                 const StableId& room_id = {});
void populate_sample_room(Renderer3D& renderer);

}  // namespace room_engine
