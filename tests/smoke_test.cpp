#include "room_engine/application.hpp"

#include <string_view>

int main() {
    return room_engine::application_name() == std::string_view{"room_engine"} ? 0 : 1;
}
