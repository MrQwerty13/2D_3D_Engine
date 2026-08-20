#include "room_engine/application.hpp"

#include <cassert>

int main() {
    assert(room_engine::application_name() == "room_engine");
    return 0;
}
