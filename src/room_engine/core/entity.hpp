#pragma once

#include <cstdint>
#include <unordered_set>

namespace room_engine {

class EntityId {
public:
    constexpr EntityId() noexcept = default;
    [[nodiscard]] static constexpr EntityId invalid() noexcept { return EntityId{}; }
    [[nodiscard]] constexpr std::uint64_t value() const noexcept { return value_; }
    [[nodiscard]] constexpr bool valid() const noexcept { return value_ != 0; }

    friend constexpr bool operator==(EntityId, EntityId) noexcept = default;
    friend constexpr auto operator<=>(EntityId, EntityId) noexcept = default;

private:
    explicit constexpr EntityId(std::uint64_t value) noexcept : value_(value) {}
    std::uint64_t value_ = 0;

    friend class EntityRegistry;
};

class EntityRegistry {
public:
    [[nodiscard]] EntityId create() {
        const EntityId id{next_id_++};
        alive_.insert(id.value());
        return id;
    }

    bool destroy(EntityId id) { return alive_.erase(id.value()) != 0; }

    [[nodiscard]] bool alive(EntityId id) const {
        return id.valid() && alive_.contains(id.value());
    }

private:
    std::uint64_t next_id_ = 1;
    std::unordered_set<std::uint64_t> alive_;
};

}  // namespace room_engine
