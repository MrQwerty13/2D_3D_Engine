#pragma once

#include "room_engine/renderer/renderer.hpp"

#include <vector>

namespace room_engine {

class DebugDraw {
public:
    void line(Vec3 start, Vec3 end, Color color = {255, 255, 255, 255}) {
        vertices_.push_back({start, color});
        vertices_.push_back({end, color});
    }

    void grid(int half_extent, float spacing, Color color = {80, 80, 80, 255});
    void axes(float length = 2.0F);
    void clear() noexcept { vertices_.clear(); }
    void flush(Renderer& renderer, const Material& material);

private:
    std::vector<Vertex> vertices_;
};

}  // namespace room_engine
