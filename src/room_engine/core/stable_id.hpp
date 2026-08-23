#pragma once

#include <cstdint>
#include <string_view>

namespace room_engine {

// FNV-1a provides deterministic picking IDs across processes and platforms.
// Zero is reserved by renderer APIs for "no object".
[[nodiscard]] inline std::uint64_t stable_scene_id(std::string_view id) noexcept {
    std::uint64_t value = 14695981039346656037ULL;
    for (const char character : id) {
        value ^= static_cast<unsigned char>(character);
        value *= 1099511628211ULL;
    }
    return value == 0 ? 1 : value;
}

}  // namespace room_engine
