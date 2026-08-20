#include "room_engine/renderer/renderer_2d.hpp"

#include <algorithm>
#include <cmath>

namespace room_engine {
namespace {
constexpr float pi = 3.14159265358979323846F;
Vec2 rotate_around(Vec2 point, Vec2 center, float radians) { const Vec2 local{point.x - center.x, point.y - center.y}; return {center.x + local.x * std::cos(radians) - local.y * std::sin(radians), center.y + local.x * std::sin(radians) + local.y * std::cos(radians)}; }
bool contains(const DrawItem2D& item, Vec2 p) {
    if (item.type == DrawItem2D::Type::Circle) { const float dx = p.x - item.bounds.min.x; const float dy = p.y - item.bounds.min.y; return dx * dx + dy * dy <= item.bounds.max.x * item.bounds.max.x; }
    if (item.type == DrawItem2D::Type::Polygon) { bool inside = false; for (std::size_t i = 0, j = item.points.size() - 1; i < item.points.size(); j = i++) { const auto& a = item.points[i]; const auto& b = item.points[j]; if (((a.y > p.y) != (b.y > p.y)) && p.x < (b.x - a.x) * (p.y - a.y) / (b.y - a.y) + a.x) inside = !inside; } return inside; }
    return p.x >= item.bounds.min.x && p.x <= item.bounds.max.x && p.y >= item.bounds.min.y && p.y <= item.bounds.max.y;
}
}

void Renderer2D::draw_sprite(const Sprite2D& s, int layer, std::uint64_t id) { draw_rect({{s.position.x - s.size.x * .5F, s.position.y - s.size.y * .5F}, {s.position.x + s.size.x * .5F, s.position.y + s.size.y * .5F}}, s.color, layer, id); items_.back().type = DrawItem2D::Type::Quad; items_.back().texture = s.texture; items_.back().rotation = s.rotation; }
void Renderer2D::draw_rect(Rect2D r, Color c, int layer, std::uint64_t id) { items_.push_back({DrawItem2D::Type::Rectangle, layer, next_order_++, r, c, {}, {}, 0, 32, id}); }
void Renderer2D::draw_circle(Circle2D c, int layer, std::uint64_t id) { items_.push_back({DrawItem2D::Type::Circle, layer, next_order_++, {c.center, {c.radius, 0}}, c.color, {}, {}, 0, c.segments, id}); }
void Renderer2D::draw_line(Line2D l, int layer, std::uint64_t id) { items_.push_back({DrawItem2D::Type::Line, layer, next_order_++, {{std::min(l.start.x,l.end.x),std::min(l.start.y,l.end.y)}, {std::max(l.start.x,l.end.x),std::max(l.start.y,l.end.y)}}, l.color, {}, {{l.start,l.end}}, 0, 2, id}); }
void Renderer2D::draw_polygon(std::span<const Vec2> p, Color c, int layer, std::uint64_t id) { if (p.empty()) return; DrawItem2D item{DrawItem2D::Type::Polygon, layer, next_order_++, {}, c, {}, std::vector<Vec2>(p.begin(),p.end()), 0, 32, id}; item.bounds.min = item.bounds.max = item.points.front(); for (const auto point : item.points) { item.bounds.min.x=std::min(item.bounds.min.x,point.x); item.bounds.min.y=std::min(item.bounds.min.y,point.y); item.bounds.max.x=std::max(item.bounds.max.x,point.x); item.bounds.max.y=std::max(item.bounds.max.y,point.y); } items_.push_back(std::move(item)); }

void Renderer2D::flush(Renderer& renderer, const RenderMaterial& base_material) {
    std::stable_sort(items_.begin(), items_.end(), [](const auto& a, const auto& b) { return a.layer == b.layer ? a.order < b.order : a.layer < b.layer; });
    for (const auto& item : items_) {
        std::vector<Vertex> vertices;
        RenderMaterial material = base_material;
        material.base_color = item.color;
        material.base_color_texture = item.texture;
        if (item.type == DrawItem2D::Type::Line) {
            material.topology = PrimitiveTopology::Lines;
            for (const auto point : item.points) vertices.push_back({{point.x, point.y, 0}, item.color, {}});
        } else {
            material.topology = PrimitiveTopology::Triangles;
            auto add_triangle = [&](Vec2 a, Vec2 b, Vec2 c) {
                vertices.push_back({{a.x, a.y, 0}, item.color, {}});
                vertices.push_back({{b.x, b.y, 0}, item.color, {}});
                vertices.push_back({{c.x, c.y, 0}, item.color, {}});
            };
            if (item.type == DrawItem2D::Type::Circle) {
                const Vec2 center = item.bounds.min;
                for (std::uint16_t i = 0; i < item.segments; ++i) {
                    const float a = 2 * pi * static_cast<float>(i) / static_cast<float>(item.segments);
                    const float b = 2 * pi * static_cast<float>(i + 1) / static_cast<float>(item.segments);
                    add_triangle(center, {center.x + item.bounds.max.x * std::cos(a), center.y + item.bounds.max.x * std::sin(a)},
                                 {center.x + item.bounds.max.x * std::cos(b), center.y + item.bounds.max.x * std::sin(b)});
                }
            } else {
                std::vector<Vec2> points = item.type == DrawItem2D::Type::Polygon ? item.points : std::vector<Vec2>{{item.bounds.min.x, item.bounds.min.y}, {item.bounds.max.x, item.bounds.min.y}, {item.bounds.max.x, item.bounds.max.y}, {item.bounds.min.x, item.bounds.max.y}};
                if (item.type == DrawItem2D::Type::Quad && item.rotation != 0.0F) {
                    const Vec2 center{(item.bounds.min.x + item.bounds.max.x) * 0.5F, (item.bounds.min.y + item.bounds.max.y) * 0.5F};
                    for (auto& point : points) point = rotate_around(point, center, item.rotation);
                }
                for (std::size_t i = 1; i + 1 < points.size(); ++i) add_triangle(points[0], points[i], points[i + 1]);
            }
        }
        if (!vertices.empty()) renderer.draw(renderer.create_vertex_buffer(vertices), vertices.size(), material);
    }
}
std::optional<std::uint64_t> Renderer2D::hit_test(ScreenPoint screen) const { const Vec2 world = viewport_.screen_to_world(screen); for (auto it=items_.rbegin();it!=items_.rend();++it) if (it->id != 0 && contains(*it,world)) return it->id; return std::nullopt; }
std::optional<std::uint64_t> Renderer2D::select(ScreenPoint screen) noexcept { selected_id_ = hit_test(screen); return selected_id_; }
}
