#include "room_engine/renderer/renderer_3d.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <fstream>
#include <limits>
#include <sstream>

namespace room_engine {
namespace {

struct JsonObject { std::string_view text; };

std::string read_file(const std::filesystem::path& path, std::string& error) {
    std::ifstream file(path, std::ios::binary);
    if (!file) { error = "unable to open asset: " + path.string(); return {}; }
    std::ostringstream contents;
    contents << file.rdbuf();
    return contents.str();
}

std::string_view array_text(std::string_view json, std::string_view key) {
    const std::size_t key_pos = json.find('"' + std::string(key) + '"');
    if (key_pos == std::string_view::npos) return {};
    const std::size_t start = json.find('[', key_pos);
    if (start == std::string_view::npos) return {};
    int depth = 0;
    for (std::size_t i = start; i < json.size(); ++i) {
        if (json[i] == '[') ++depth;
        if (json[i] == ']' && --depth == 0) return json.substr(start + 1, i - start - 1);
    }
    return {};
}

std::vector<JsonObject> objects(std::string_view array) {
    std::vector<JsonObject> result;
    for (std::size_t i = 0; i < array.size();) {
        const std::size_t start = array.find('{', i);
        if (start == std::string_view::npos) break;
        int depth = 0;
        for (std::size_t j = start; j < array.size(); ++j) {
            if (array[j] == '{') ++depth;
            if (array[j] == '}' && --depth == 0) {
                result.push_back({array.substr(start, j - start + 1)});
                i = j + 1;
                break;
            }
        }
        if (i <= start) break;
    }
    return result;
}

std::optional<double> number(std::string_view object, std::string_view key) {
    const std::size_t key_pos = object.find('"' + std::string(key) + '"');
    if (key_pos == std::string_view::npos) return std::nullopt;
    const std::size_t colon = object.find(':', key_pos);
    if (colon == std::string_view::npos) return std::nullopt;
    std::size_t end = colon + 1;
    while (end < object.size() && (object[end] == ' ' || object[end] == '\n')) ++end;
    std::size_t consumed = 0;
    try { return std::stod(std::string(object.substr(end)), &consumed); }
    catch (...) { return std::nullopt; }
}

std::string string_value(std::string_view object, std::string_view key) {
    const std::size_t key_pos = object.find('"' + std::string(key) + '"');
    if (key_pos == std::string_view::npos) return {};
    const std::size_t start = object.find('"', object.find(':', key_pos) + 1);
    if (start == std::string_view::npos) return {};
    const std::size_t end = object.find('"', start + 1);
    return end == std::string_view::npos ? std::string{} : std::string(object.substr(start + 1, end - start - 1));
}

std::vector<std::byte> base64(std::string_view text) {
    static constexpr std::string_view chars = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::vector<std::byte> result;
    int accumulator = 0;
    int bits = 0;
    for (const char c : text) {
        const std::size_t value = chars.find(c);
        if (value == std::string_view::npos) continue;
        accumulator = (accumulator << 6) | static_cast<int>(value);
        bits += 6;
        if (bits >= 8) {
            bits -= 8;
            result.push_back(static_cast<std::byte>((accumulator >> bits) & 0xff));
        }
    }
    return result;
}

template <typename T> bool read_value(const std::vector<std::byte>& data, std::size_t offset, T& value) {
    if (offset + sizeof(T) > data.size()) return false;
    std::memcpy(&value, data.data() + offset, sizeof(T));
    return true;
}

struct Accessor { int view = -1; std::size_t offset = 0; std::size_t count = 0; int component = 0; int components = 1; };

int component_count(std::string_view type) {
    if (type == "VEC2") return 2;
    if (type == "VEC3") return 3;
    if (type == "VEC4") return 4;
    return 1;
}

std::optional<std::uint32_t> index_value(const std::vector<std::byte>& data, std::size_t offset, int component) {
    if (component == 5121) { std::uint8_t value = 0; return read_value(data, offset, value) ? std::optional{static_cast<std::uint32_t>(value)} : std::nullopt; }
    if (component == 5123) { std::uint16_t value = 0; return read_value(data, offset, value) ? std::optional{static_cast<std::uint32_t>(value)} : std::nullopt; }
    if (component == 5125) { std::uint32_t value = 0; return read_value(data, offset, value) ? std::optional{value} : std::nullopt; }
    return std::nullopt;
}

}  // namespace

AssetLoadResult load_gltf(const std::filesystem::path& path) {
    std::string error;
    const std::string file = read_file(path, error);
    if (file.empty()) return AssetLoadResult::failure(error);
    std::string json;
    std::vector<std::byte> binary;
    if (path.extension() == ".glb") {
        if (file.size() < 20) return AssetLoadResult::failure("GLB is truncated: " + path.string());
        std::uint32_t magic = 0; std::uint32_t version = 0; std::uint32_t total = 0;
        std::memcpy(&magic, file.data(), 4); std::memcpy(&version, file.data() + 4, 4); std::memcpy(&total, file.data() + 8, 4);
        if (magic != 0x46546c67U || version != 2U || total > file.size()) return AssetLoadResult::failure("invalid GLB header: " + path.string());
        std::size_t offset = 12;
        while (offset + 8 <= total) {
            std::uint32_t length = 0; std::uint32_t type = 0;
            std::memcpy(&length, file.data() + offset, 4); std::memcpy(&type, file.data() + offset + 4, 4);
            if (offset + 8U + length > total) return AssetLoadResult::failure("truncated GLB chunk: " + path.string());
            if (type == 0x4e4f534aU) json.assign(file.data() + offset + 8, length);
            if (type == 0x004e4942U) binary.assign(reinterpret_cast<const std::byte*>(file.data() + offset + 8), reinterpret_cast<const std::byte*>(file.data() + offset + 8 + length));
            offset += 8U + length;
        }
    } else {
        json = file;
    }
    if (json.empty()) return AssetLoadResult::failure("glTF JSON chunk is missing: " + path.string());
    const auto buffer_objects = objects(array_text(json, "buffers"));
    if (binary.empty() && !buffer_objects.empty()) {
        const std::string uri = string_value(buffer_objects.front().text, "uri");
        if (uri.rfind("data:application/octet-stream;base64,", 0) == 0) binary = base64(uri.substr(37));
        else if (!uri.empty()) {
            std::string buffer_error;
            const std::string external = read_file(path.parent_path() / uri, buffer_error);
            if (external.empty()) return AssetLoadResult::failure(buffer_error);
            binary.assign(reinterpret_cast<const std::byte*>(external.data()), reinterpret_cast<const std::byte*>(external.data() + external.size()));
        }
    }
    const auto view_objects = objects(array_text(json, "bufferViews"));
    const auto accessor_objects = objects(array_text(json, "accessors"));
    std::vector<Accessor> accessors;
    for (const auto object : accessor_objects) {
        Accessor accessor;
        accessor.view = static_cast<int>(number(object.text, "bufferView").value_or(-1.0));
        accessor.offset = static_cast<std::size_t>(number(object.text, "byteOffset").value_or(0));
        accessor.count = static_cast<std::size_t>(number(object.text, "count").value_or(0));
        accessor.component = static_cast<int>(number(object.text, "componentType").value_or(0));
        const std::size_t type_start = object.text.find("\"type\"");
        accessor.components = type_start == std::string_view::npos ? 1 : component_count(string_value(object.text.substr(type_start), "type"));
        accessors.push_back(accessor);
    }
    MeshAsset result;
    for (const auto primitive : objects(array_text(json, "primitives"))) {
        const auto position = number(primitive.text, "POSITION");
        const auto indices = number(primitive.text, "indices");
        if (!position || !indices || *position < 0 || *indices < 0 || static_cast<std::size_t>(*position) >= accessors.size() || static_cast<std::size_t>(*indices) >= accessors.size()) continue;
        const Accessor& positions = accessors[static_cast<std::size_t>(*position)];
        const Accessor& index_accessor = accessors[static_cast<std::size_t>(*indices)];
        if (positions.view < 0 || index_accessor.view < 0 || static_cast<std::size_t>(positions.view) >= view_objects.size() || static_cast<std::size_t>(index_accessor.view) >= view_objects.size()) continue;
        const auto view_offset = [&](const Accessor& accessor) { return static_cast<std::size_t>(number(view_objects[static_cast<std::size_t>(accessor.view)].text, "byteOffset").value_or(0)); };
        const auto view_stride = [&](const Accessor& accessor, std::size_t element_size) { return static_cast<std::size_t>(number(view_objects[static_cast<std::size_t>(accessor.view)].text, "byteStride").value_or(static_cast<double>(element_size))); };
        Mesh mesh;
        const std::size_t position_offset = view_offset(positions) + positions.offset;
        const std::size_t stride = view_stride(positions, sizeof(float) * 3U);
        for (std::size_t i = 0; i < positions.count; ++i) {
            std::array<float, 3> value{};
            if (!read_value(binary, position_offset + i * stride, value[0]) || !read_value(binary, position_offset + i * stride + sizeof(float), value[1]) || !read_value(binary, position_offset + i * stride + sizeof(float) * 2U, value[2])) break;
            mesh.vertices.push_back({{value[0], value[1], value[2]}, {}, {}, {0.0F, 1.0F, 0.0F}});
        }
        const std::size_t index_offset = view_offset(index_accessor) + index_accessor.offset;
        const std::size_t index_size = index_accessor.component == 5121 ? 1U : index_accessor.component == 5123 ? 2U : 4U;
        for (std::size_t i = 0; i < index_accessor.count; ++i) {
            const auto value = index_value(binary, index_offset + i * index_size, index_accessor.component);
            if (!value) break;
            mesh.indices.push_back(*value);
        }
        if (mesh.valid()) result.meshes.push_back(std::move(mesh));
    }
    if (!result.valid()) return AssetLoadResult::failure("no supported triangle meshes found: " + path.string());
    return {std::move(result), {}};
}

void Renderer3D::add_mesh(Mesh mesh, Transform transform, Material material, std::uint64_t id) {
    if (mesh.valid()) instances_.push_back({std::move(mesh), transform, material, id});
}

void Renderer3D::add_asset(const MeshAsset& asset, Transform transform, std::uint64_t id) {
    for (std::size_t i = 0; i < asset.meshes.size(); ++i) add_mesh(asset.meshes[i], transform, i < asset.materials.size() ? asset.materials[i] : Material{}, id);
}

void Renderer3D::flush(Renderer& renderer, const Camera& camera) {
    renderer.set_camera(camera);
    for (const auto& instance : instances_) {
        const Mat4 matrix = transform_matrix(instance.transform);
        std::vector<Vertex> vertices;
        vertices.reserve(instance.mesh.indices.size());
        for (const std::uint32_t index : instance.mesh.indices) {
            if (index >= instance.mesh.vertices.size()) continue;
            Vertex vertex = instance.mesh.vertices[index];
            vertex.position = transform_point(matrix, vertex.position);
            vertex.normal = normalize(transform_point(matrix, vertex.normal) - transform_point(matrix, {}));
            float illumination = ambient_.intensity;
            const Vec3 light_direction = normalize(directional_.direction * -1.0F);
            illumination += std::max(0.0F, dot(vertex.normal, light_direction)) * directional_.intensity;
            for (const auto& point : points_) {
                const Vec3 to_light = point.position - vertex.position;
                const float distance = length(to_light);
                if (distance < point.range && distance > 0.000001F) {
                    const float attenuation = 1.0F - distance / point.range;
                    illumination += std::max(0.0F, dot(vertex.normal, to_light * (1.0F / distance))) * point.intensity * attenuation;
                }
            }
            const auto shade = [illumination](std::uint8_t channel) {
                return static_cast<std::uint8_t>(std::clamp(illumination * static_cast<float>(channel), 0.0F, 255.0F));
            };
            vertex.color = {shade(instance.material.base_color.r), shade(instance.material.base_color.g),
                            shade(instance.material.base_color.b), instance.material.base_color.a};
            vertices.push_back(vertex);
        }
        if (!vertices.empty()) renderer.draw(renderer.create_vertex_buffer(vertices), vertices.size(), instance.material);
    }
}

Ray Renderer3D::screen_ray(ScreenPoint screen, const Camera& camera, float width, float height) const {
    const float aspect = std::max(width, 1.0F) / std::max(height, 1.0F);
    const float tan_half = std::tan(camera.projection().value[5] == 0.0F ? 0.5F : std::atan(1.0F / camera.projection().value[5]));
    const Vec3 forward = normalize(camera.target - camera.position);
    const Vec3 right = normalize(cross(forward, camera.up));
    const Vec3 up = normalize(cross(right, forward));
    const float nx = (screen.x / std::max(width, 1.0F)) * 2.0F - 1.0F;
    const float ny = 1.0F - (screen.y / std::max(height, 1.0F)) * 2.0F;
    return {camera.position, normalize(forward + right * (nx * tan_half * aspect) + up * (ny * tan_half))};
}

std::optional<RayHit> Renderer3D::raycast(ScreenPoint screen, const Camera& camera, float width, float height) const {
    const Ray ray = screen_ray(screen, camera, width, height);
    std::optional<RayHit> closest;
    for (const auto& instance : instances_) {
        const Mat4 matrix = transform_matrix(instance.transform);
        for (std::size_t i = 0; i + 2 < instance.mesh.indices.size(); i += 3) {
            const auto ia = instance.mesh.indices[i]; const auto ib = instance.mesh.indices[i + 1]; const auto ic = instance.mesh.indices[i + 2];
            if (ia >= instance.mesh.vertices.size() || ib >= instance.mesh.vertices.size() || ic >= instance.mesh.vertices.size()) continue;
            const Vec3 a = transform_point(matrix, instance.mesh.vertices[ia].position); const Vec3 b = transform_point(matrix, instance.mesh.vertices[ib].position); const Vec3 c = transform_point(matrix, instance.mesh.vertices[ic].position);
            const Vec3 edge1 = b - a; const Vec3 edge2 = c - a; const Vec3 p = cross(ray.direction, edge2); const float determinant = dot(edge1, p);
            if (std::fabs(determinant) < 0.000001F) continue;
            const float inverse = 1.0F / determinant; const Vec3 t = ray.origin - a; const float u = dot(t, p) * inverse;
            if (u < 0.0F || u > 1.0F) continue;
            const Vec3 q = cross(t, edge1); const float v = dot(ray.direction, q) * inverse;
            if (v < 0.0F || u + v > 1.0F) continue;
            const float distance = dot(edge2, q) * inverse;
            if (distance <= 0.0F || (closest && distance >= closest->distance)) continue;
            closest = RayHit{instance.id, distance, ray.origin + ray.direction * distance, normalize(cross(edge1, edge2))};
        }
    }
    return closest;
}

std::optional<std::uint64_t> Renderer3D::select(ScreenPoint screen, const Camera& camera, float width, float height) const {
    const auto hit = raycast(screen, camera, width, height);
    return hit && hit->id != 0 ? std::optional{hit->id} : std::nullopt;
}

Mesh make_room_floor(float width, float depth) {
    const float x = width * 0.5F; const float z = depth * 0.5F;
    Mesh mesh; mesh.vertices = {{{-x, 0, -z}, {}, {0, 0}, {0, 1, 0}}, {{x, 0, -z}, {}, {1, 0}, {0, 1, 0}}, {{x, 0, z}, {}, {1, 1}, {0, 1, 0}}, {{-x, 0, z}, {}, {0, 1}, {0, 1, 0}}}; mesh.indices = {0, 2, 1, 0, 3, 2}; return mesh;
}

Mesh make_room_wall(float length, float height, float thickness) {
    const float half = length * 0.5F; const float depth = thickness * 0.5F;
    Mesh mesh; mesh.vertices = {{{-half, 0, -depth}}, {{half, 0, -depth}}, {{half, height, -depth}}, {{-half, height, -depth}}, {{-half, 0, depth}}, {{half, 0, depth}}, {{half, height, depth}}, {{-half, height, depth}}};
    mesh.indices = {0, 1, 2, 0, 2, 3, 5, 4, 7, 5, 7, 6, 4, 0, 3, 4, 3, 7, 1, 5, 6, 1, 6, 2, 3, 2, 6, 3, 6, 7, 4, 5, 1, 4, 1, 0}; return mesh;
}

void populate_sample_room(Renderer3D& renderer) {
    renderer.begin();
    renderer.set_ambient_light({{210, 220, 240, 255}, 0.35F});
    renderer.set_directional_light({{-0.5F, -1.0F, -0.35F}, {255, 244, 220, 255}, 0.8F});
    renderer.add_point_light({{0.0F, 2.6F, 0.0F}, {255, 235, 205, 255}, 1.2F, 8.0F});
    renderer.add_mesh(make_room_floor(), {}, Material{{190, 190, 180, 255}, 0.0F, 0.9F}, 1);
    renderer.add_mesh(make_room_wall(8.0F, 3.0F), {{0.0F, 0.0F, -3.0F}}, Material{{225, 225, 220, 255}}, 2);
    renderer.add_mesh(make_room_wall(8.0F, 3.0F), {{0.0F, 0.0F, 3.0F}, Quaternion::from_axis_angle({0, 1, 0}, 3.14159265F)}, Material{{225, 225, 220, 255}}, 3);
    renderer.add_mesh(make_room_wall(6.0F, 3.0F), {{-4.0F, 0.0F, 0.0F}, Quaternion::from_axis_angle({0, 1, 0}, 1.5707963F)}, Material{{215, 215, 210, 255}}, 4);
    renderer.add_mesh(make_room_wall(6.0F, 3.0F), {{4.0F, 0.0F, 0.0F}, Quaternion::from_axis_angle({0, 1, 0}, -1.5707963F)}, Material{{215, 215, 210, 255}}, 5);
}

}  // namespace room_engine
