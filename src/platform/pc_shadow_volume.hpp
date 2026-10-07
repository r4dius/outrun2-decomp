#pragma once
// Car stencil shadow volumes, ported from the PC EXE:
//   46BA20 shadow_alloc (protected: jmp [1039898] = edi = [5B2F68 + model*4],
//          measured by shadow_alloc_observe), per layout word +08 / +10 a
//          0xA8-byte shadow object (tail [+A0] = self, [+A4] = 0) into car
//          +2BC / +2C4 (+2B8 / +2C0 the tails) and 420E50(id, object, 3)
//   46BB20 shadow_free: 4211D0 (440CD0 on +98) and 440CD0 on the car tail
//   420E50 build: model object (448810 +0C count, [[+20]] + index*0x3C),
//          buffer sizes from its first draw of every primitive group (type 6
//          ranges become 5 in the model), one block (edges +0C, group ranges
//          +14, volume vertices +10, tail +98), the darkening quad +20..+97
//          (77FF30.. x 740C94 / 740C98, colour 0x5F000000), 4211F0 edges
//   4211F0 edges: every triangle of every draw/range (strip 5 with parity and
//          degenerate skip, list 4) through 421680 with its face normal
//          (D3DXVec3Normalize); 421680 matches reversed edges by position
//          (second face +14, flag +20) or appends (0x24-byte edge)
//   422550 per frame: light direction (arg, x -25 at 6282D0) into model space
//          by the 3x3 of the current matrix (a push that leaves the rotation
//          only matrix in the next slot, then a pop), 421740 silhouette
//          quads (421850 per edge, 6 vertices of 12 bytes) into +10 with the
//          vertex count x 4 in +18, stage textures saved in 955A30..955A3C,
//          streams 0/1 cleared, 421A90 (two stencil passes: INCRSAT with
//          CCW culling then DECRSAT with CW, vertex shader [955A40],
//          DrawPrimitiveUP list of +18/2 triangles), textures restored, 408880,
//          +04 = (+04 + 1) mod +08
//   422740 darkening: textures saved, 4220F0 (stencil LESS ref 0, alpha
//          blend, FVF 44 strip of the +20 quad), restored, 408880
// All shadow objects, their blocks, the car work and the bank system
// section live in PcRaceMemory at PC-shaped addresses; pointer fields of the
// model (object +04..+38) are system offsets natively and are relocated by
// PcShadowServices::reloc when read.
#include "platform/race_area.hpp"
#include "platform/pc_d3d9.hpp"
#include "driving/pc_matrix_stack.hpp"
#include <cstdint>
#include <functional>
namespace outrun::platform {
struct PcFlushContext;
struct PcShadowServices {
    PcRaceMemory& m;
    PcD3D9Device& d;
    // 580253 malloc (PC-shaped address of a zeroed-or-not new block).
    std::function<std::uint32_t(std::uint32_t bytes)> malloc;
    // 440CD0 free of a tail record ([tail] = block).
    std::function<void(std::uint32_t tail)> free_tail;
    // 448810(resource): object count (+0C) and the absolute object table
    // ([[+20]]); false when the resource is not loaded (count 0 on the PC).
    std::function<bool(std::uint32_t resource,std::uint32_t& count,std::uint32_t& table)> bank;
    // Relocation of a model pointer field read at `field` (0 on the PC,
    // the system base natively).
    std::function<std::uint32_t(std::uint32_t field)> reloc;
    // IDirect3D{Vertex,Index}Buffer9::Lock(0, 0, &p, flags): pointer and size.
    std::function<const std::uint8_t*(std::uint32_t buffer,std::uint32_t flags,std::uint32_t& size)> lock;
    float screen_740c94{1.f},screen_740c98{1.f};
};
// 46BA20 / 46BB20 on the car work (model byte +11, 5B2F68 layouts).
void shadow_alloc_46ba20(PcShadowServices&,std::uint32_t car);
void shadow_free_46bb20(PcShadowServices&,std::uint32_t car);
// 420E50 / 4211F0 / 421680 (probes).
std::uint32_t shadow_build_420e50(PcShadowServices&,std::uint32_t id,std::uint32_t object,std::uint32_t kind);
void shadow_edges_4211f0(PcShadowServices&,std::uint32_t object);
// 421740 silhouette with the model-space light (x, y, z).
void shadow_silhouette_421740(PcShadowServices&,std::uint32_t object,const float light[3]);
// Frame part: device state of 422550/421A90/422740/4220F0.
struct PcShadowFrame {
    PcFlushContext& fl;
    driving::PcMatrixStack& matrices;
    std::uint32_t layer_7d25f0{};
    std::uint32_t vertex_shader_955a40{};
    std::uint32_t saved_955a30[4]{};
};
void shadow_draw_422550(PcShadowServices&,PcShadowFrame&,std::uint32_t object,std::uint32_t light);
void shadow_darken_422740(PcShadowServices&,PcShadowFrame&,std::uint32_t object);
}
