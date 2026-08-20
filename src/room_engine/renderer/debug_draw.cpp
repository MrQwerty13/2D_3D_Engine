#include "room_engine/renderer/debug_draw.hpp"

namespace room_engine {

void DebugDraw::grid(int half_extent, float spacing, Color color) {
    for (int i = -half_extent; i <= half_extent; ++i) {
        const float coordinate = static_cast<float>(i) * spacing;
        line({coordinate, 0.0F, -static_cast<float>(half_extent) * spacing},
             {coordinate, 0.0F, static_cast<float>(half_extent) * spacing}, color);
        line({-static_cast<float>(half_extent) * spacing, 0.0F, coordinate},
             {static_cast<float>(half_extent) * spacing, 0.0F, coordinate}, color);
    }
}

void DebugDraw::axes(float length) {
    line({}, {length, 0.0F, 0.0F}, {220, 60, 60, 255});
    line({}, {0.0F, length, 0.0F}, {60, 220, 60, 255});
    line({}, {0.0F, 0.0F, length}, {60, 100, 240, 255});
}

void DebugDraw::flush(Renderer& renderer, const Material& material) {
    if (!vertices_.empty()) {
        const VertexBuffer buffer = renderer.create_vertex_buffer(vertices_);
        renderer.draw(buffer, vertices_.size(), material);
    }
    clear();
}

}  // namespace room_engine
