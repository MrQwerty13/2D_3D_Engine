#pragma once

#include <string_view>

namespace room_engine {

[[nodiscard]] constexpr std::string_view application_name() noexcept {
    return "room_engine";
}

}  // namespace room_engine
