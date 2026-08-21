#include "room_engine/core/presentation.hpp"

#include "room_engine/renderer/renderer_3d.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <limits>
#include <sstream>
#include <vector>

namespace room_engine {
namespace {

std::string esc(std::string_view s) {
    std::string r = "\"";
    for (char c : s) { if (c == '\\' || c == '\"') r += '\\'; if (c == '\n') r += "\\n"; else r += c; }
    return r + "\"";
}
void point(std::ostream& o, Point2 p) { o << "[" << p.x << "," << p.y << "]"; }
void vec(std::ostream& o, Vec3 v) { o << "[" << v.x << "," << v.y << "," << v.z << "]"; }
void transform(std::ostream& o, const Transform& t) { o << "{\"position\":"; vec(o,t.position); o << ",\"rotation\":[" << t.rotation.w << "," << t.rotation.x << "," << t.rotation.y << "," << t.rotation.z << "],\"scale\":"; vec(o,t.scale); o << "}"; }
template<class T> void quote_id(std::ostream& o, std::string_view key, const T& value, bool& first) { if (!first) o << ','; first=false; o << esc(key) << ':' << esc(value); }

// The JSON writer is intentionally explicit and deterministic. Import accepts the same
// schema plus legacy project files produced by the core archive through a compatibility note.
void write_json(const RoomDesign& d, std::ostream& o) {
    o << std::setprecision(std::numeric_limits<float>::max_digits10) << "{\"schema\":1,\"materials\":[";
    for (std::size_t i=0;i<d.materials.size();++i) { if(i) o<<','; const auto&m=d.materials[i]; o<<"{\"id\":"<<esc(m.id)<<",\"name\":"<<esc(m.name)<<",\"albedo\":";vec(o,m.albedo);o<<",\"roughness\":"<<m.roughness<<"}"; }
    o << "],\"rooms\":[";
    for (std::size_t ri=0;ri<d.rooms.size();++ri) { if(ri)o<<','; const auto&r=d.rooms[ri]; o<<"{\"id\":"<<esc(r.id)<<",\"name\":"<<esc(r.name)<<",\"walls\":[";
        for(std::size_t i=0;i<r.walls.size();++i){if(i)o<<',';const auto&w=r.walls[i];o<<"{\"id\":"<<esc(w.id)<<",\"start\":";point(o,w.start);o<<",\"end\":";point(o,w.end);o<<",\"thickness\":"<<w.thickness<<",\"height\":"<<w.height<<",\"material_id\":"<<esc(w.material_id)<<"}";}
        o<<"],\"furniture\":["; for(std::size_t i=0;i<r.furniture.size();++i){if(i)o<<',';const auto&f=r.furniture[i];o<<"{\"id\":"<<esc(f.id)<<",\"name\":"<<esc(f.name)<<",\"transform\":";transform(o,f.transform);o<<",\"dimensions\":";vec(o,f.dimensions);o<<",\"material_id\":"<<esc(f.material_id)<<"}";} o<<"]}";
    }
    MemoryArchive archive; serialize(d, archive);
    o << "],\"core_archive\":" << esc(archive.read_string("room_design")) << "}";
}

std::string read_all(const std::filesystem::path& p, std::string& e) { std::ifstream f(p); if(!f){e="cannot open "+p.string();return{};} std::ostringstream s;s<<f.rdbuf();return s.str(); }
ExportResult write_file(const std::filesystem::path& p, const std::string& s) { std::ofstream f(p); if(!f)return{false,"cannot write "+p.string()}; f<<s; return{static_cast<bool>(f),f?"":"write failed"}; }

} // namespace

ExportResult export_project_json(const RoomDesign& d, const std::filesystem::path& p) { if(!d.valid())return{false,"design is invalid"}; std::ostringstream o;write_json(d,o);return write_file(p,o.str()); }

ExportResult import_project_json(const std::filesystem::path& p, RoomDesign& out) {
    // Import is deliberately conservative until a JSON dependency is part of the locked build.
    // It supports JSON exported by this version by extracting the complete canonical payload
    // from the embedded compatibility field when present; malformed/foreign JSON is rejected.
    std::string e, text=read_all(p,e); if(text.empty())return{false,e};
    const auto marker=text.find("\"core_archive\":\"");
    if(marker==std::string::npos)return{false,"JSON import requires core_archive compatibility field"};
    std::size_t start=marker+std::string_view{"\"core_archive\":\""}.size(); std::string payload;
    for(;start<text.size();++start){char c=text[start];if(c=='"')break;if(c=='\\'&&start+1<text.size()){char n=text[++start];payload+=n=='n'?'\n':n;}else payload+=c;}
    MemoryArchive archive;archive.write_string("room_design",payload); auto loaded=deserialize(archive); if(!loaded||!loaded->valid())return{false,"invalid room design in JSON"};out=std::move(*loaded);return{true,{}};
}

ExportResult export_floor_plan_svg(const RoomDesign& d, const std::filesystem::path& p, const StableId& id) {
    const auto it=std::find_if(d.rooms.begin(),d.rooms.end(),[&](const Room&r){return id.empty()||r.id==id;}); if(it==d.rooms.end())return{false,"room not found"};
    if(!d.valid())return{false,"design is invalid"}; float minx=0,miny=0,maxx=1,maxy=1; bool first=true; for(const auto&w:it->walls)for(const auto&q:{w.start,w.end}){if(first){minx=maxx=q.x;miny=maxy=q.y;first=false;}else{minx=std::min(minx,q.x);maxx=std::max(maxx,q.x);miny=std::min(miny,q.y);maxy=std::max(maxy,q.y);}}
    const float pad=.5F; std::ostringstream o;o<<"<svg xmlns=\"http://www.w3.org/2000/svg\" viewBox=\""<<minx-pad<<' '<<miny-pad<<' '<<maxx-minx+2*pad<<' '<<maxy-miny+2*pad<<"\"><g fill=\"none\" stroke=\"#20242b\" stroke-width=\"0.08\">";
    for(const auto&w:it->walls)o<<"<line x1=\""<<w.start.x<<"\" y1=\""<<-w.start.y<<"\" x2=\""<<w.end.x<<"\" y2=\""<<-w.end.y<<"\"/>";o<<"</g></svg>";return write_file(p,o.str());
}

ExportResult export_room_glb(const RoomDesign& d, const std::filesystem::path& p, const StableId& id) {
    const auto it=std::find_if(d.rooms.begin(),d.rooms.end(),[&](const Room&r){return id.empty()||r.id==id;}); if(it==d.rooms.end())return{false,"room not found"}; if(!d.valid())return{false,"design is invalid"};
    std::vector<float> positions; std::vector<std::uint32_t> indices; auto append=[&](const Mesh&m){const auto base=static_cast<std::uint32_t>(positions.size()/3);for(const auto&v:m.vertices){positions.insert(positions.end(),{v.position.x,v.position.y,v.position.z});}for(auto x:m.indices)indices.push_back(base+x);};
    for(const auto&w:it->walls)append(make_room_wall(w)); if(it->floor)append(make_room_floor(*it->floor)); if(it->ceiling)append(make_room_ceiling(*it->ceiling)); if(positions.empty())return{false,"room has no exportable geometry"};
    std::vector<std::byte> bin(positions.size()*sizeof(float)+indices.size()*sizeof(std::uint32_t)); std::memcpy(bin.data(),positions.data(),positions.size()*sizeof(float));std::memcpy(bin.data()+positions.size()*sizeof(float),indices.data(),indices.size()*sizeof(std::uint32_t)); while(bin.size()%4)bin.push_back(std::byte{' '});
    const std::size_t posbytes = positions.size() * sizeof(float);
    const std::size_t iboff = (posbytes + 3U) & ~std::size_t(3);
    std::ostringstream j;
    j << "{\"asset\":{\"version\":\"2.0\",\"generator\":\"room_engine\"},\"scene\":0,\"scenes\":[{\"nodes\":[0]}],\"nodes\":[{\"mesh\":0}],\"meshes\":[{\"primitives\":[{\"attributes\":{\"POSITION\":0},\"indices\":1}]}],\"buffers\":[{\"byteLength\":" << bin.size()
      << "}],\"bufferViews\":[{\"buffer\":0,\"byteOffset\":0,\"byteLength\":" << posbytes << "},{\"buffer\":0,\"byteOffset\":" << iboff
      << ",\"byteLength\":" << indices.size() * sizeof(std::uint32_t) << "}],\"accessors\":[{\"bufferView\":0,\"componentType\":5126,\"count\":" << positions.size() / 3
      << ",\"type\":\"VEC3\"},{\"bufferView\":1,\"componentType\":5125,\"count\":" << indices.size() << ",\"type\":\"SCALAR\"}]}";
    std::string js = j.str(); while (js.size() % 4U) js += ' ';
    const std::uint32_t total = static_cast<std::uint32_t>(12U + 8U + js.size() + 8U + bin.size());
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
