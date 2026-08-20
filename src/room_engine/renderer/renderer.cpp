#include "room_engine/renderer/renderer.hpp"

#include <algorithm>
#include <array>
#include <cstring>
#include <utility>

#if defined(ROOM_ENGINE_USE_BGFX)
#include <SDL3/SDL.h>
#include <bgfx/bgfx.h>
#include <bgfx/platform.h>
#endif

namespace room_engine {
namespace {

class NullRenderer final : public Renderer {
public:
    bool is_ready() const noexcept override { return true; }
    bool begin_frame(Color) override { return true; }
    void set_camera(const Camera& camera) override { view_projection_ = camera.view_projection(); }
    void draw(const VertexBuffer&, std::size_t, const Material&) override {}
    void draw(const VertexBuffer&, const IndexBuffer&, std::size_t, const Material&) override {}
    VertexBuffer create_vertex_buffer(std::span<const Vertex>) override { return {next_id_++}; }
    IndexBuffer create_index_buffer(std::span<const std::uint16_t>) override { return {next_id_++}; }
    Shader load_shader(std::span<const std::byte>, std::span<const std::byte>) override {
        return {next_id_++};
    }
    Texture create_texture(std::uint32_t, std::uint32_t, std::span<const std::byte>) override {
        return {next_id_++};
    }
    void end_frame() override {}

private:
    std::uint16_t next_id_ = 1;
    Mat4 view_projection_ = Mat4::identity();
};

#if defined(ROOM_ENGINE_USE_BGFX)
class BgfxRenderer final : public Renderer {
public:
    explicit BgfxRenderer(SDL_Window* window, const RendererConfig& config) {
        bgfx::renderFrame();
        bgfx::Init init;
        init.type = bgfx::RendererType::Count;
        init.resolution.width = config.width;
        init.resolution.height = config.height;
        init.resolution.reset = config.vsync ? BGFX_RESET_VSYNC : BGFX_RESET_NONE;
        init.platformData.nwh = native_window_handle(window);
        init.platformData.ndt = native_display_handle(window);
        if (!bgfx::init(init)) {
            return;
        }
        initialized_ = true;
        bgfx::setViewRect(0, 0, static_cast<std::uint16_t>(config.width),
                          static_cast<std::uint16_t>(config.height));
    }

    ~BgfxRenderer() override { if (initialized_) bgfx::shutdown(); }
    bool is_ready() const noexcept override { return initialized_; }
    bool begin_frame(Color clear_color) override {
        if (!initialized_) return false;
        const std::uint32_t rgba = (static_cast<std::uint32_t>(clear_color.r) << 24U) |
                                   (static_cast<std::uint32_t>(clear_color.g) << 16U) |
                                   (static_cast<std::uint32_t>(clear_color.b) << 8U) |
                                   clear_color.a;
        bgfx::setViewClear(0, BGFX_CLEAR_COLOR | BGFX_CLEAR_DEPTH, rgba, 1.0F, 0);
        bgfx::touch(0);
        return true;
    }
    void set_camera(const Camera& camera) override {
        view_projection_ = camera.view_projection();
        bgfx::setViewTransform(0, nullptr, view_projection_.data());
    }
    void draw(const VertexBuffer& vertices, std::size_t vertex_count, const Material& material) override {
        if (!initialized_ || !vertices.valid() || !material.shader.valid()) return;
        bgfx::setVertexBuffer(0, vertex_buffers_[vertices.id]);
        bgfx::setState(BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A | BGFX_STATE_DEPTH_TEST_LESS |
                       BGFX_STATE_MSAA);
        bgfx::submit(0, shaders_[material.shader.id]);
        (void)vertex_count;
    }
    void draw(const VertexBuffer& vertices, const IndexBuffer& indices, std::size_t index_count,
              const Material& material) override {
        if (!initialized_ || !vertices.valid() || !indices.valid() || !material.shader.valid()) return;
        bgfx::setVertexBuffer(0, vertex_buffers_[vertices.id]);
        bgfx::setIndexBuffer(index_buffers_[indices.id], 0, static_cast<std::uint32_t>(index_count));
        bgfx::setState(BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A | BGFX_STATE_DEPTH_TEST_LESS |
                       BGFX_STATE_MSAA);
        bgfx::submit(0, shaders_[material.shader.id]);
    }
    VertexBuffer create_vertex_buffer(std::span<const Vertex> vertices) override {
        const auto handle = static_cast<std::uint16_t>(vertex_buffers_.size());
        vertex_buffers_.push_back(bgfx::createVertexBuffer(bgfx::copy(vertices.data(),
                                                                      static_cast<std::uint32_t>(vertices.size_bytes())),
                                                           vertex_layout()));
        return {handle};
    }
    IndexBuffer create_index_buffer(std::span<const std::uint16_t> indices) override {
        const auto handle = static_cast<std::uint16_t>(index_buffers_.size());
        index_buffers_.push_back(bgfx::createIndexBuffer(bgfx::copy(indices.data(),
                                                                      static_cast<std::uint32_t>(indices.size_bytes()))));
        return {handle};
    }
    Shader load_shader(std::span<const std::byte> vertex_binary,
                       std::span<const std::byte> fragment_binary) override {
        const auto handle = static_cast<std::uint16_t>(shaders_.size());
        const bgfx::ShaderHandle vertex = bgfx::createShader(bgfx::makeRef(vertex_binary.data(),
                                                                            static_cast<std::uint32_t>(vertex_binary.size_bytes())));
        const bgfx::ShaderHandle fragment = bgfx::createShader(bgfx::makeRef(fragment_binary.data(),
                                                                              static_cast<std::uint32_t>(fragment_binary.size_bytes())));
        programs_.push_back(bgfx::createProgram(vertex, fragment, true));
        shaders_.push_back(programs_.back());
        return {handle};
    }
    Texture create_texture(std::uint32_t width, std::uint32_t height,
                           std::span<const std::byte> rgba8) override {
        const auto handle = static_cast<std::uint16_t>(textures_.size());
        textures_.push_back(bgfx::createTexture2D(static_cast<std::uint16_t>(width),
                                                  static_cast<std::uint16_t>(height), false, 1,
                                                  bgfx::TextureFormat::RGBA8, 0,
                                                  bgfx::copy(rgba8.data(), static_cast<std::uint32_t>(rgba8.size_bytes()))));
        return {handle};
    }
    void end_frame() override { if (initialized_) bgfx::frame(); }

private:
    static bgfx::VertexLayout& vertex_layout() {
        static bgfx::VertexLayout layout;
        static bool initialized = false;
        if (!initialized) {
            layout.begin().add(bgfx::Attrib::Position, 3, bgfx::AttribType::Float)
                .add(bgfx::Attrib::Color0, 4, bgfx::AttribType::Uint8, true).end();
            initialized = true;
        }
        return layout;
    }
    static void* native_window_handle(SDL_Window* window) {
        const SDL_PropertiesID properties = SDL_GetWindowProperties(window);
#if defined(__APPLE__)
        return SDL_GetPointerProperty(properties, SDL_PROP_WINDOW_COCOA_WINDOW_POINTER, nullptr);
#else
        return reinterpret_cast<void*>(SDL_GetNumberProperty(properties,
                                                               SDL_PROP_WINDOW_X11_WINDOW_NUMBER, 0));
#endif
    }
    static void* native_display_handle(SDL_Window*) { return nullptr; }

    bool initialized_ = false;
    Mat4 view_projection_ = Mat4::identity();
    std::vector<bgfx::VertexBufferHandle> vertex_buffers_{{BGFX_INVALID_HANDLE}};
    std::vector<bgfx::IndexBufferHandle> index_buffers_{{BGFX_INVALID_HANDLE}};
    std::vector<bgfx::ProgramHandle> shaders_{{BGFX_INVALID_HANDLE}};
    std::vector<bgfx::ProgramHandle> programs_;
    std::vector<bgfx::TextureHandle> textures_{{BGFX_INVALID_HANDLE}};
};
#endif

}  // namespace

std::unique_ptr<Renderer> Renderer::create(SDL_Window* window, const RendererConfig& config) {
#if defined(ROOM_ENGINE_USE_BGFX)
    return std::make_unique<BgfxRenderer>(window, config);
#else
    (void)window;
    (void)config;
    return std::make_unique<NullRenderer>();
#endif
}

}  // namespace room_engine
