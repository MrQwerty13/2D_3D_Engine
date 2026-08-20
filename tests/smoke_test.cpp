#include "room_engine/application.hpp"
#include "room_engine/core/entity.hpp"
#include "room_engine/core/transform.hpp"

#include <cassert>
#include <cmath>
#include <string_view>

int main() {
    assert(room_engine::application_name() == std::string_view{"room_engine"});

    room_engine::EntityRegistry entities;
    const auto first = entities.create();
    const auto second = entities.create();
    assert(first.valid());
    assert(first != second);
    assert(entities.alive(first));
    assert(entities.destroy(first));
    assert(!entities.alive(first));
    const auto third = entities.create();
    assert(third.value() > second.value());

    const room_engine::Transform parent{{10.0F, 0.0F, 0.0F}, {}, {2.0F, 2.0F, 2.0F}};
    const room_engine::Transform child{{1.0F, 2.0F, 3.0F}, {}, {1.0F, 1.0F, 1.0F}};
    const auto world = parent.combine(child);
    const auto point = world.transform_point({0.0F, 0.0F, 0.0F});
    assert(std::fabs(point.x - 12.0F) < 0.001F);
    assert(std::fabs(point.y - 4.0F) < 0.001F);
    assert(std::fabs(point.z - 6.0F) < 0.001F);

    return 0;
}
