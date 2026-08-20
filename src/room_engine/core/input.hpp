#pragma once

#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>

namespace room_engine {

enum class InputActionState { Released, Pressed, Held };

class InputActions {
public:
    void bind(std::string action, int key_code) { bindings_[std::move(action)] = key_code; }

    void set_key(int key_code, bool down) {
        for (const auto& [action, binding] : bindings_) {
            if (binding != key_code) {
                continue;
            }
            auto& state = states_[action];
            state = down ? (state == InputActionState::Released ? InputActionState::Pressed
                                                                  : InputActionState::Held)
                         : InputActionState::Released;
        }
    }

    [[nodiscard]] InputActionState state(std::string_view action) const {
        const auto it = states_.find(std::string{action});
        return it == states_.end() ? InputActionState::Released : it->second;
    }

    void next_frame() {
        for (auto& [action, current] : states_) {
            (void)action;
            if (current == InputActionState::Pressed) {
                current = InputActionState::Held;
            }
        }
    }

private:
    std::unordered_map<std::string, int> bindings_;
    std::unordered_map<std::string, InputActionState> states_;
};

}  // namespace room_engine
