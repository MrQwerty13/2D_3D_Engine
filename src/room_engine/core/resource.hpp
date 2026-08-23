#pragma once

#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>

namespace room_engine {

class IResource {
public:
    virtual ~IResource() = default;
    [[nodiscard]] virtual std::string_view type_name() const noexcept = 0;
};

class ResourceStore {
public:
    void insert(std::string key, std::shared_ptr<IResource> resource) {
        resources_[std::move(key)] = std::move(resource);
    }

    [[nodiscard]] std::shared_ptr<IResource> find(std::string_view key) const {
        const auto it = resources_.find(std::string{key});
        return it == resources_.end() ? nullptr : it->second;
    }

    bool erase(std::string_view key) { return resources_.erase(std::string{key}) != 0; }

private:
    std::unordered_map<std::string, std::shared_ptr<IResource>> resources_;
};

}  // namespace room_engine
