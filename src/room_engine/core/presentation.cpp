#include "room_engine/core/presentation.hpp"

#include "room_engine/core/placement.hpp"
#include "room_engine/renderer/renderer_3d.hpp"
#include "room_engine/renderer/math.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <limits>
#include <optional>
#include <sstream>
#include <vector>

namespace room_engine {
namespace {

std::string esc(std::string_view s) {
    std::string r = "\"";
    constexpr char hex[] = "0123456789abcdef";
    for (const char raw_character : s) {
        const auto c = static_cast<unsigned char>(raw_character);
        switch (c) {
        case '\"': r += "\\\""; break;
        case '\\': r += "\\\\"; break;
        case '\b': r += "\\b"; break;
        case '\f': r += "\\f"; break;
        case '\n': r += "\\n"; break;
        case '\r': r += "\\r"; break;
        case '\t': r += "\\t"; break;
        default:
            if (c < 0x20U) {
                r += "\\u00";
                r += hex[c >> 4U];
                r += hex[c & 0x0fU];
            } else {
                r += static_cast<char>(c);
            }
            break;
        }
    }
    return r + "\"";
}
void point(std::ostream& o, Point2 p) { o << "[" << p.x << "," << p.y << "]"; }
void vec(std::ostream& o, Vec3 v) { o << "[" << v.x << "," << v.y << "," << v.z << "]"; }
void transform(std::ostream& o, const Transform& t) { o << "{\"position\":"; vec(o,t.position); o << ",\"rotation\":[" << t.rotation.w << "," << t.rotation.x << "," << t.rotation.y << "," << t.rotation.z << "],\"scale\":"; vec(o,t.scale); o << "}"; }

// The JSON writer is intentionally explicit and deterministic. Import accepts the same
// schema plus legacy project files produced by the core archive through a compatibility note.
void write_json(const RoomDesign& d, std::ostream& o) {
    o << std::setprecision(std::numeric_limits<float>::max_digits10) << "{\"schema\":"
      << RoomDesign::current_version << ",\"materials\":[";
    for (std::size_t i=0;i<d.materials.size();++i) { if(i) o<<','; const auto&m=d.materials[i]; o<<"{\"id\":"<<esc(m.id)<<",\"name\":"<<esc(m.name)<<",\"albedo\":";vec(o,m.albedo);o<<",\"roughness\":"<<m.roughness<<"}"; }
    o << "],\"rooms\":[";
    for (std::size_t ri=0;ri<d.rooms.size();++ri) { if(ri)o<<','; const auto&r=d.rooms[ri]; o<<"{\"id\":"<<esc(r.id)<<",\"name\":"<<esc(r.name)<<",\"walls\":[";
        for(std::size_t i=0;i<r.walls.size();++i){if(i)o<<',';const auto&w=r.walls[i];o<<"{\"id\":"<<esc(w.id)<<",\"start\":";point(o,w.start);o<<",\"end\":";point(o,w.end);o<<",\"thickness\":"<<w.thickness<<",\"height\":"<<w.height<<",\"material_id\":"<<esc(w.material_id)<<"}";}
        o << "],\"floor\":";
        if (r.floor) {
            o << "{\"id\":" << esc(r.floor->id) << ",\"boundary\":[";
            for (std::size_t i = 0; i < r.floor->boundary.size(); ++i) {
                if (i) o << ',';
                point(o, r.floor->boundary[i]);
            }
            o << "],\"elevation\":" << r.floor->elevation << ",\"material_id\":"
              << esc(r.floor->material_id) << '}';
        } else {
            o << "null";
        }
        o << ",\"ceiling\":";
        if (r.ceiling) {
            o << "{\"id\":" << esc(r.ceiling->id) << ",\"boundary\":[";
            for (std::size_t i = 0; i < r.ceiling->boundary.size(); ++i) {
                if (i) o << ',';
                point(o, r.ceiling->boundary[i]);
            }
            o << "],\"elevation\":" << r.ceiling->elevation << ",\"material_id\":"
              << esc(r.ceiling->material_id) << '}';
        } else {
            o << "null";
        }
        o << ",\"doors\":[";
        for (std::size_t i = 0; i < r.doors.size(); ++i) {
            if (i) o << ',';
            const auto& door = r.doors[i];
            o << "{\"id\":" << esc(door.id) << ",\"wall_id\":" << esc(door.wall_id)
              << ",\"offset\":" << door.offset << ",\"width\":" << door.width
              << ",\"bottom\":" << door.bottom << ",\"height\":" << door.height
              << ",\"open\":" << (door.open ? "true" : "false")
              << ",\"material_id\":" << esc(door.material_id) << '}';
        }
        o << "],\"windows\":[";
        for (std::size_t i = 0; i < r.windows.size(); ++i) {
            if (i) o << ',';
            const auto& window = r.windows[i];
            o << "{\"id\":" << esc(window.id) << ",\"wall_id\":"
              << esc(window.wall_id) << ",\"offset\":" << window.offset
              << ",\"width\":" << window.width << ",\"bottom\":" << window.bottom
              << ",\"height\":" << window.height << ",\"material_id\":"
              << esc(window.material_id) << '}';
        }
        o<<"],\"furniture\":["; for(std::size_t i=0;i<r.furniture.size();++i){if(i)o<<',';const auto&f=r.furniture[i];o<<"{\"id\":"<<esc(f.id)<<",\"name\":"<<esc(f.name)<<",\"transform\":";transform(o,f.transform);o<<",\"dimensions\":";vec(o,f.dimensions);o<<",\"material_id\":"<<esc(f.material_id)<<",\"asset_id\":"<<esc(f.asset_id)<<"}";} o<<"]}";
    }
    MemoryArchive archive; serialize(d, archive);
    o << "],\"core_archive\":" << esc(archive.read_string("room_design")) << "}";
}

std::string read_all(const std::filesystem::path& path, std::string& error) {
    constexpr std::uintmax_t max_project_bytes = 64U * 1024U * 1024U;
    std::error_code size_error;
    const std::uintmax_t size = std::filesystem::file_size(path, size_error);
    if (!size_error && size > max_project_bytes) {
        error = "project file exceeds the 64 MiB safety limit";
        return {};
    }
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        error = "cannot open " + path.string();
        return {};
    }
    std::ostringstream stream;
    stream << file.rdbuf();
    if (!file.eof() && file.fail()) {
        error = "cannot read " + path.string();
        return {};
    }
    std::string contents = stream.str();
    if (contents.empty()) error = "project file is empty";
    return contents;
}

ExportResult write_file(const std::filesystem::path& path, const std::string& contents) {
    const std::filesystem::path temporary = path.string() + ".tmp";
    std::ofstream file(temporary, std::ios::binary | std::ios::trunc);
    if (!file) return {false, "cannot write " + path.string()};
    file.write(contents.data(), static_cast<std::streamsize>(contents.size()));
    file.flush();
    if (!file) {
        file.close();
        std::error_code ignored;
        std::filesystem::remove(temporary, ignored);
        return {false, "write failed"};
    }
    file.close();
    if (!file) {
        std::error_code ignored;
        std::filesystem::remove(temporary, ignored);
        return {false, "write failed"};
    }
    std::error_code rename_error;
    std::filesystem::rename(temporary, path, rename_error);
    if (rename_error) {
        std::error_code ignored;
        std::filesystem::remove(temporary, ignored);
        return {false, "cannot replace " + path.string()};
    }
    return {true, {}};
}

std::optional<std::string> json_string_field(std::string_view json, std::string_view key) {
    const std::string marker = "\"" + std::string{key} + "\"";
    const std::size_t key_position = json.find(marker);
    if (key_position == std::string_view::npos) return std::nullopt;
    const std::size_t colon = json.find(':', key_position + marker.size());
    if (colon == std::string_view::npos) return std::nullopt;
    std::size_t position = colon + 1U;
    while (position < json.size() &&
           (json[position] == ' ' || json[position] == '\n' || json[position] == '\r' ||
            json[position] == '\t'))
        ++position;
    if (position >= json.size() || json[position] != '\"') return std::nullopt;
    ++position;

    const auto hex_value = [](char value) -> int {
        if (value >= '0' && value <= '9') return value - '0';
        if (value >= 'a' && value <= 'f') return value - 'a' + 10;
        if (value >= 'A' && value <= 'F') return value - 'A' + 10;
        return -1;
    };
    std::string result;
    for (; position < json.size(); ++position) {
        const char value = json[position];
        if (value == '\"') return result;
        if (value != '\\') {
            if (static_cast<unsigned char>(value) < 0x20U) return std::nullopt;
            result += value;
            continue;
        }
        if (++position >= json.size()) return std::nullopt;
        const char escaped = json[position];
        switch (escaped) {
        case '\"': result += '\"'; break;
        case '\\': result += '\\'; break;
        case '/': result += '/'; break;
        case 'b': result += '\b'; break;
        case 'f': result += '\f'; break;
        case 'n': result += '\n'; break;
        case 'r': result += '\r'; break;
        case 't': result += '\t'; break;
        case 'u': {
            if (position + 4U >= json.size()) return std::nullopt;
            int code = 0;
            for (std::size_t i = 0; i < 4U; ++i) {
                const int digit = hex_value(json[++position]);
                if (digit < 0) return std::nullopt;
                code = code * 16 + digit;
            }
            if (code > 0xff) return std::nullopt;
            result += static_cast<char>(static_cast<unsigned char>(code));
            break;
        }
        default: return std::nullopt;
        }
    }
    return std::nullopt;
}

} // namespace

ExportResult export_project_json(const RoomDesign& d, const std::filesystem::path& p) { if(!d.valid())return{false,"design is invalid"}; std::ostringstream o;write_json(d,o);return write_file(p,o.str()); }

ExportResult import_project_json(const std::filesystem::path& p, RoomDesign& out) {
    // Import is deliberately conservative until a JSON dependency is part of the locked build.
    // It supports JSON exported by this version by extracting the complete canonical payload
    // from the embedded compatibility field when present; malformed/foreign JSON is rejected.
    std::string error;
    const std::string text = read_all(p, error);
    if (text.empty()) return {false, error};
    const auto payload = json_string_field(text, "core_archive");
    if (!payload)
        return {false, "JSON import requires a valid core_archive compatibility field"};
    MemoryArchive archive;
    archive.write_string("room_design", *payload);
    auto loaded = deserialize(archive);
    if (!loaded || !loaded->valid()) return {false, "invalid room design in JSON"};
    out = std::move(*loaded);
    return {true, {}};
}

ExportResult export_floor_plan_svg(const RoomDesign& d, const std::filesystem::path& p, const StableId& id) {
    const auto it = std::find_if(d.rooms.begin(), d.rooms.end(), [&](const Room& room) {
        return id.empty() || room.id == id;
    });
    if (it == d.rooms.end()) return {false, "room not found"};
    if (!d.valid()) return {false, "design is invalid"};

    float min_x = 0.0F;
    float min_y = 0.0F;
    float max_x = 1.0F;
    float max_y = 1.0F;
    bool first = true;
    const auto include = [&](Point2 point) {
        const float svg_y = -point.y;
        if (first) {
            min_x = max_x = point.x;
            min_y = max_y = svg_y;
            first = false;
        } else {
            min_x = std::min(min_x, point.x);
            max_x = std::max(max_x, point.x);
            min_y = std::min(min_y, svg_y);
            max_y = std::max(max_y, svg_y);
        }
    };
    if (it->floor)
        for (const Point2 point : it->floor->boundary) include(point);
    for (const auto& wall : it->walls) {
        include(wall.start);
        include(wall.end);
    }
    for (const auto& furniture : it->furniture)
        for (const Point2 point : furniture_footprint_corners(furniture)) include(point);

    const auto wall_point = [](const WallSegment& wall, float offset) {
        const float length = std::hypot(wall.end.x - wall.start.x,
                                        wall.end.y - wall.start.y);
        const float ratio = offset / length;
        return Point2{wall.start.x + (wall.end.x - wall.start.x) * ratio,
                      wall.start.y + (wall.end.y - wall.start.y) * ratio};
    };
    const float padding = 0.5F;
    std::ostringstream out;
    out << std::setprecision(std::numeric_limits<float>::max_digits10)
        << "<svg xmlns=\"http://www.w3.org/2000/svg\" viewBox=\"" << min_x - padding
        << ' ' << min_y - padding << ' ' << max_x - min_x + 2.0F * padding << ' '
        << max_y - min_y + 2.0F * padding << "\">";
    if (it->floor) {
        out << "<polygon fill=\"#f3f1eb\" stroke=\"none\" points=\"";
        for (const Point2 point : it->floor->boundary)
            out << point.x << ',' << -point.y << ' ';
        out << "\"/>";
    }
    out << "<g fill=\"none\" stroke=\"#20242b\">";
    for (const auto& wall : it->walls)
        out << "<line x1=\"" << wall.start.x << "\" y1=\"" << -wall.start.y
            << "\" x2=\"" << wall.end.x << "\" y2=\"" << -wall.end.y
            << "\" stroke-width=\"" << wall.thickness << "\"/>";
    out << "</g><g fill=\"none\" stroke=\"#e4a83e\" stroke-width=\"0.08\">";
    for (const auto& door : it->doors) {
        const auto wall = std::find_if(it->walls.begin(), it->walls.end(),
                                       [&](const auto& item) {
                                           return item.id == door.wall_id;
                                       });
        if (wall == it->walls.end()) continue;
        const Point2 start = wall_point(*wall, door.offset);
        const Point2 end = wall_point(*wall, door.offset + door.width);
        out << "<line x1=\"" << start.x << "\" y1=\"" << -start.y << "\" x2=\""
            << end.x << "\" y2=\"" << -end.y << "\"/>";
    }
    for (const auto& window : it->windows) {
        const auto wall = std::find_if(it->walls.begin(), it->walls.end(),
                                       [&](const auto& item) {
                                           return item.id == window.wall_id;
                                       });
        if (wall == it->walls.end()) continue;
        const Point2 start = wall_point(*wall, window.offset);
        const Point2 end = wall_point(*wall, window.offset + window.width);
        out << "<line stroke=\"#4b9dcc\" x1=\"" << start.x << "\" y1=\"" << -start.y
            << "\" x2=\"" << end.x << "\" y2=\"" << -end.y << "\"/>";
    }
    out << "</g><g fill=\"#6e9fd0\" fill-opacity=\"0.55\" stroke=\"#315b80\" "
           "stroke-width=\"0.03\">";
    for (const auto& furniture : it->furniture) {
        out << "<polygon points=\"";
        for (const Point2 point : furniture_footprint_corners(furniture))
            out << point.x << ',' << -point.y << ' ';
        out << "\"/>";
    }
    out << "</g></svg>";
    return write_file(p, out.str());
}

ExportResult export_room_glb(const RoomDesign& d, const std::filesystem::path& p, const StableId& id) {
    const auto it = std::find_if(d.rooms.begin(), d.rooms.end(), [&](const Room& room) {
        return id.empty() || room.id == id;
    });
    if (it == d.rooms.end()) return {false, "room not found"};
    if (!d.valid()) return {false, "design is invalid"};

    std::vector<float> positions;
    std::vector<std::uint32_t> indices;
    const auto append = [&](const Mesh& mesh, const Transform& world = {}) {
        const std::size_t max_vertices =
            std::numeric_limits<std::uint32_t>::max();
        if (mesh.vertices.size() > max_vertices ||
            positions.size() / 3U > max_vertices - mesh.vertices.size())
            return false;
        const auto base = static_cast<std::uint32_t>(positions.size() / 3U);
        const Mat4 matrix = transform_matrix(world);
        for (const auto& vertex : mesh.vertices) {
            const Vec3 position = transform_point(matrix, vertex.position);
            positions.insert(positions.end(), {position.x, position.y, position.z});
        }
        for (const std::uint32_t index : mesh.indices) {
            if (index >= mesh.vertices.size()) return false;
            indices.push_back(base + index);
        }
        return true;
    };

    for (const auto& wall : it->walls) {
        std::vector<const Door*> doors;
        std::vector<const Window*> windows;
        for (const auto& door : it->doors)
            if (door.wall_id == wall.id) doors.push_back(&door);
        for (const auto& window : it->windows)
            if (window.wall_id == wall.id) windows.push_back(&window);
        const float wall_length = std::hypot(wall.end.x - wall.start.x,
                                             wall.end.y - wall.start.y);
        const float angle =
            std::atan2(-(wall.end.y - wall.start.y), wall.end.x - wall.start.x);
        const Transform wall_transform{{wall.start.x, 0.0F, wall.start.y},
                                       Quaternion::from_axis_angle({0, 1, 0}, angle)};
        if (!append(make_room_wall(wall, doors, windows), wall_transform))
            return {false, "room geometry is too large to export"};
        const float dx = (wall.end.x - wall.start.x) / wall_length;
        const float dz = (wall.end.y - wall.start.y) / wall_length;
        for (const Door* door : doors) {
            const float center = door->offset + door->width * 0.5F;
            const Transform opening_transform{
                {wall.start.x + dx * center, door->bottom,
                 wall.start.y + dz * center},
                Quaternion::from_axis_angle({0, 1, 0}, angle)};
            if (!append(make_room_opening(door->width, door->height,
                                          std::max(wall.thickness * 0.15F, 0.01F)),
                        opening_transform))
                return {false, "room geometry is too large to export"};
        }
        for (const Window* window : windows) {
            const float center = window->offset + window->width * 0.5F;
            const Transform opening_transform{
                {wall.start.x + dx * center, window->bottom,
                 wall.start.y + dz * center},
                Quaternion::from_axis_angle({0, 1, 0}, angle)};
            if (!append(make_room_opening(window->width, window->height,
                                          std::max(wall.thickness * 0.15F, 0.01F)),
                        opening_transform))
                return {false, "room geometry is too large to export"};
        }
    }
    if (it->floor && !append(make_room_floor(*it->floor)))
        return {false, "room geometry is too large to export"};
    if (it->ceiling && !append(make_room_ceiling(*it->ceiling)))
        return {false, "room geometry is too large to export"};
    for (const auto& furniture : it->furniture) {
        Transform furniture_transform = furniture.transform;
        furniture_transform.position.y -=
            furniture.dimensions.y * furniture.transform.scale.y * 0.5F;
        if (!append(make_room_opening(furniture.dimensions.x, furniture.dimensions.y,
                                      furniture.dimensions.z),
                    furniture_transform))
            return {false, "room geometry is too large to export"};
    }
    if (positions.empty()) return {false, "room has no exportable geometry"};

    const std::size_t posbytes = positions.size() * sizeof(float);
    const std::size_t indexbytes = indices.size() * sizeof(std::uint32_t);
    if (posbytes > std::numeric_limits<std::uint32_t>::max() ||
        indexbytes > std::numeric_limits<std::uint32_t>::max() ||
        posbytes > std::numeric_limits<std::size_t>::max() - indexbytes)
        return {false, "room geometry is too large to export"};
    std::vector<std::byte> bin(posbytes + indexbytes);
    std::memcpy(bin.data(), positions.data(), posbytes);
    std::memcpy(bin.data() + posbytes, indices.data(), indexbytes);
    while (bin.size() % 4U) bin.push_back(std::byte{0});
    const std::size_t iboff = posbytes;

    Vec3 minimum{positions[0], positions[1], positions[2]};
    Vec3 maximum = minimum;
    for (std::size_t i = 0; i + 2 < positions.size(); i += 3) {
        minimum.x = std::min(minimum.x, positions[i]);
        minimum.y = std::min(minimum.y, positions[i + 1]);
        minimum.z = std::min(minimum.z, positions[i + 2]);
        maximum.x = std::max(maximum.x, positions[i]);
        maximum.y = std::max(maximum.y, positions[i + 1]);
        maximum.z = std::max(maximum.z, positions[i + 2]);
    }
    std::ostringstream j;
    j << "{\"asset\":{\"version\":\"2.0\",\"generator\":\"room_engine\"},\"scene\":0,\"scenes\":[{\"nodes\":[0]}],\"nodes\":[{\"mesh\":0}],\"meshes\":[{\"primitives\":[{\"attributes\":{\"POSITION\":0},\"indices\":1}]}],\"buffers\":[{\"byteLength\":" << bin.size()
      << "}],\"bufferViews\":[{\"buffer\":0,\"byteOffset\":0,\"byteLength\":" << posbytes << "},{\"buffer\":0,\"byteOffset\":" << iboff
      << ",\"byteLength\":" << indices.size() * sizeof(std::uint32_t) << "}],\"accessors\":[{\"bufferView\":0,\"componentType\":5126,\"count\":" << positions.size() / 3
      << ",\"type\":\"VEC3\",\"min\":[" << minimum.x << ',' << minimum.y << ','
      << minimum.z << "],\"max\":[" << maximum.x << ',' << maximum.y << ','
      << maximum.z << "]},{\"bufferView\":1,\"componentType\":5125,\"count\":"
      << indices.size() << ",\"type\":\"SCALAR\"}]}";
    std::string js = j.str(); while (js.size() % 4U) js += ' ';
    const std::uint64_t total_size = 12U + 8U + js.size() + 8U + bin.size();
    if (total_size > std::numeric_limits<std::uint32_t>::max())
        return {false, "room geometry is too large to export"};
    const std::uint32_t total = static_cast<std::uint32_t>(total_size);
    std::ofstream f(p, std::ios::binary); if (!f) return {false, "cannot write " + p.string()};
    const std::uint32_t magic = 0x46546c67U, version = 2U, json_type = 0x4e4f534aU, bin_type = 0x004e4942U;
    const std::uint32_t json_length = static_cast<std::uint32_t>(js.size()), bin_length = static_cast<std::uint32_t>(bin.size());
    auto write_u32 = [&f](std::uint32_t value) { f.write(reinterpret_cast<const char*>(&value), static_cast<std::streamsize>(sizeof(value))); };
    write_u32(magic); write_u32(version); write_u32(total); write_u32(json_length); write_u32(json_type);
    f.write(js.data(), static_cast<std::streamsize>(js.size())); write_u32(bin_length); write_u32(bin_type);
    f.write(reinterpret_cast<const char*>(bin.data()), static_cast<std::streamsize>(bin.size()));
    return {static_cast<bool>(f), f ? "" : "write failed"};
}

ExportResult export_screenshot(Renderer& r, const std::filesystem::path& p) { const bool ok=r.save_screenshot(p); return{ok,ok?"":"renderer backend does not support screenshot readback"}; }
} // namespace room_engine
