#pragma once

#include "room_engine/renderer/renderer.hpp"
#include "room_engine/renderer/viewport.hpp"

#include <cstdint>
#include <filesystem>
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
    std::vector<Material> materials;
    [[nodiscard]] bool valid() const noexcept { return !meshes.empty(); }
};

struct AssetLoadResult {
    std::optional<MeshAsset> asset;
    std::string error;
    [[nodiscard]] explicit operator bool() const noexcept { return asset.has_value(); }
    static AssetLoadResult failure(std::string message) { return {std::nullopt, std::move(message)}; }
};

[[nodiscard]] AssetLoadResult load_gltf(const std::filesystem::path& path);

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
    Material material{};
    std::uint64_t id = 0;
};

class Renderer3D {
public:
    void begin() noexcept { instances_.clear(); }
    void add_mesh(Mesh mesh, Transform transform = {}, Material material = {},
                  std::uint64_t id = 0);
    void add_asset(const MeshAsset& asset, Transform transform = {}, std::uint64_t id = 0);
    void set_ambient_light(AmbientLight light) noexcept { ambient_ = light; }
    void set_directional_light(DirectionalLight light) noexcept { directional_ = light; }
    void add_point_light(PointLight light) { points_.push_back(light); }
    void clear_lights() noexcept { points_.clear(); }
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
};

[[nodiscard]] Mesh make_room_floor(float width = 8.0F, float depth = 6.0F);
[[nodiscard]] Mesh make_room_wall(float length, float height, float thickness = 0.15F);
void populate_sample_room(Renderer3D& renderer);

}  // namespace room_engine
