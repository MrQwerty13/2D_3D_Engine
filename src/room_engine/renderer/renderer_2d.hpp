#pragma once

#include "room_engine/renderer/renderer.hpp"
#include "room_engine/renderer/viewport.hpp"

#include <cstdint>
#include <optional>
#include <span>
#include <vector>

namespace room_engine {

struct Rect2D { Vec2 min{}; Vec2 max{}; };
struct Sprite2D { Vec2 position{}; Vec2 size{1.0F, 1.0F}; float rotation = 0.0F; Color color{}; Texture texture{}; };
struct Circle2D { Vec2 center{}; float radius = 1.0F; Color color{}; std::uint16_t segments = 32; };
struct Line2D { Vec2 start{}; Vec2 end{}; Color color{}; float width = 1.0F; };

struct DrawItem2D {
    enum class Type { Quad, Rectangle, Circle, Line, Polygon };
    Type type = Type::Rectangle;
    int layer = 0;
    std::uint64_t order = 0;
    Rect2D bounds{};
    Color color{};
    Texture texture{};
    std::vector<Vec2> points;
    float rotation = 0.0F;
    std::uint16_t segments = 32;
    std::uint64_t id = 0;
};

class Renderer2D {
public:
    explicit Renderer2D(Viewport2D& viewport) : viewport_(viewport) {}
    void begin() noexcept { items_.clear(); next_order_ = 0; }
    void draw_sprite(const Sprite2D& sprite, int layer = 0, std::uint64_t id = 0);
    void draw_rect(Rect2D rect, Color color, int layer = 0, std::uint64_t id = 0);
    void draw_circle(Circle2D circle, int layer = 0, std::uint64_t id = 0);
    void draw_line(Line2D line, int layer = 0, std::uint64_t id = 0);
    void draw_polygon(std::span<const Vec2> points, Color color, int layer = 0,
                      std::uint64_t id = 0);
    void flush(Renderer& renderer, const RenderMaterial& material = {});
    [[nodiscard]] std::optional<std::uint64_t> hit_test(ScreenPoint screen) const;
    [[nodiscard]] std::optional<std::uint64_t> select(ScreenPoint screen) noexcept;
    void clear_selection() noexcept { selected_id_.reset(); }
    [[nodiscard]] std::optional<std::uint64_t> selected() const noexcept { return selected_id_; }
    [[nodiscard]] const std::vector<DrawItem2D>& items() const noexcept { return items_; }

private:
    Viewport2D& viewport_;
    std::vector<DrawItem2D> items_;
    std::uint64_t next_order_ = 0;
    std::optional<std::uint64_t> selected_id_;
};

}  // namespace room_engine
