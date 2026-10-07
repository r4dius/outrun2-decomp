#pragma once
// Flagman flag cloth (ROB03), ported from the PC EXE:
//   402100 flagman_cloth_init: 95B270 bit 0 cleared, 401080 cloth (5 x 7
//          nodes between 73772C and 737738), 401830 strip indices 85FF20 /
//          vertices 860040
//   402140 flagman_cloth_ctrl: 401490 (row 0 from the rest pose through the
//          current matrix, rows 1.. 401530 -> 4015E0 integration, 401720
//          springs, 401790 lengths), 401990 vertex positions and normals
//          (40F2C0)
//   402170 flagman_cloth_disp: 408880, 401B80 (texture, material, 41B5C0
//          save, states, DrawIndexedPrimitiveUP FVF 1D2, 41B7A0 restore),
//          89EDE0/89EDE8 = 0, 408880
//   4021A0 flagman_cloth_dest: 95B270 bit 0 cleared
// State: cloth 95B270..95BF20 (+00 flags, +04 columns, +08 rows, +0C nodes
// 0x5C each: +00 edge flags, +04 position, +10, +1C velocity, +28 rest
// position, +34 previous position, +40 wind direction, +4C/+50/+54/+58
// rest lengths up / down / left / right; +CAC wind scale), mesh 85FF20..
// 8605B8. 95AEB8 (wind), 95AEAC (length bias) are never written (0).
#include "platform/race_area.hpp"
#include "platform/pc_d3d9.hpp"
#include <cstdint>
#include <functional>
#include <vector>
namespace outrun::platform {
struct PcRobFlagState {
    static constexpr std::uint32_t ClothBase=0x95b270u,ClothEnd=0x95bf20u;
    static constexpr std::uint32_t MeshBase=0x85ff20u,MeshEnd=0x8605b8u,Indices=0x85ff20u,Vertices=0x860040u;
    static constexpr std::uint32_t WindBase=0x95aeacu,WindEnd=0x95aec4u;   // 95AEAC bias, 95AEB8 wind vector
    std::vector<std::uint8_t> cloth=std::vector<std::uint8_t>(ClothEnd-ClothBase);
    std::vector<std::uint8_t> mesh=std::vector<std::uint8_t>(MeshEnd-MeshBase);
    std::vector<std::uint8_t> wind=std::vector<std::uint8_t>(WindEnd-WindBase);
    void map(PcRaceMemory& m){
        m.map(ClothBase,cloth.data(),cloth.size());m.map(MeshBase,mesh.data(),mesh.size());m.map(WindBase,wind.data(),wind.size());
    }
};
void rob_flag_init_402100(PcRaceContext&);
void rob_flag_ctrl_402140(PcRaceContext&);
void rob_flag_dest_4021a0(PcRaceMemory&);
// 401B80 flagman_cloth_draw(vertices, indices, columns, rows) on the device.
// `mesh` maps the vertices / indices, `save` the particle bss slots of
// 41B5C0 (ParticleSavedStates41b5c0, 8A92D0 textures); texture is the
// handle 448810 gives (rob_flag_texture_401b80); render_global performs the
// 89EDE0 / 89EDE8 writes of 41B7A0.
struct PcRobFlagDraw {
    PcRaceMemory& mesh;
    PcRaceMemory& save;
    PcD3D9Device& d;
    std::uint32_t texture{};
    std::function<void(std::uint32_t,std::uint32_t)> render_global;
};
void rob_flag_draw_401b80(PcRobFlagDraw&,std::uint32_t vertices,std::uint32_t indices,std::uint32_t cols,std::uint32_t rows);
// The texture resource of 401B80: C3 for preset [78024C] 1 or 3, else 12B.
constexpr std::uint32_t rob_flag_texture_resource(std::uint32_t preset_78024c){return preset_78024c==1u||preset_78024c==3u?0xc3u:0x12bu;}
// Handle 1 of a loaded texture resource: [[r]+8] > 1 ? [[r+24]][1] : 0 with
// the system section and the handle slot offset of the native loader.
struct PcPmtResources;
std::uint32_t rob_flag_texture_401b80(PcPmtResources&);
// Parts (probes).
void rob_flag_cloth_401080(PcRaceMemory&,std::uint32_t cloth,std::uint32_t cols,std::uint32_t rows,std::uint32_t a,std::uint32_t b);
void rob_flag_mesh_401830(PcRaceMemory&,std::uint32_t indices,std::uint32_t vertices,std::uint32_t cols,std::uint32_t rows);
void rob_flag_normals_401990(PcRaceMemory&,std::uint32_t vertices,std::uint32_t nodes,std::uint32_t rows,std::uint32_t cols);
}
