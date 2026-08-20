#pragma once

#include "room_engine/renderer/camera.hpp"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>
#include <string>
#include <vector>

struct SDL_Window;

namespace room_engine {

struct Color { std::uint8_t r = 0; std::uint8_t g = 0; std::uint8_t b = 0; std::uint8_t a = 255; };
struct Vec2 { float x = 0.0F; float y = 0.0F; };
struct Vertex { Vec3 position{}; Color color{255, 255, 255, 255}; Vec2 uv{}; };
struct VertexBuffer { std::uint16_t id = 0; [[nodiscard]] bool valid() const noexcept { return id != 0; } };
struct IndexBuffer { std::uint16_t id = 0; [[nodiscard]] bool valid() const noexcept { return id != 0; } };
struct Shader { std::uint16_t id = 0; [[nodiscard]] bool valid() const noexcept { return id != 0; } };
struct Texture { std::uint16_t id = 0; [[nodiscard]] bool valid() const noexcept { return id != 0; } };

enum class PrimitiveTopology { Lines, Triangles };

struct Material {
    Color base_color{255, 255, 255, 255};
    float metallic = 0.0F;
    float roughness = 1.0F;
    Texture base_color_texture{};
    Shader shader{};
    PrimitiveTopology topology = PrimitiveTopology::Triangles;
};

struct RendererConfig {
    std::uint32_t width = 1280;
    std::uint32_t height = 720;
    std::string name = "room_engine";
    bool vsync = true;
};

class Renderer {
public:
    virtual ~Renderer() = default;
    [[nodiscard]] static std::unique_ptr<Renderer> create(SDL_Window* window, const RendererConfig& config);
    [[nodiscard]] virtual bool is_ready() const noexcept = 0;
    virtual bool begin_frame(Color clear_color) = 0;
    virtual void set_camera(const Camera& camera) = 0;
    virtual void draw(const VertexBuffer& vertices, std::size_t vertex_count, const Material& material) = 0;
    virtual void draw(const VertexBuffer& vertices, const IndexBuffer& indices, std::size_t index_count,
                      const Material& material) = 0;
    virtual VertexBuffer create_vertex_buffer(std::span<const Vertex> vertices) = 0;
    virtual IndexBuffer create_index_buffer(std::span<const std::uint16_t> indices) = 0;
    virtual Shader load_shader(std::span<const std::byte> vertex_binary,
                               std::span<const std::byte> fragment_binary) = 0;
    virtual Texture create_texture(std::uint32_t width, std::uint32_t height,
                                   std::span<const std::byte> rgba8) = 0;
    virtual void end_frame() = 0;
};

}  // namespace room_engine
