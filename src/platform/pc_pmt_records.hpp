#pragma once
// PMT model records as the loader (42E4F0) leaves them in a bank's system
// section: little-endian words, pointer fields relocated to system offsets.
// Read by value with one bounds check per record (pmt<T>).
#include "driving/pc_driving.hpp"
#include <cstdint>
#include <cstring>
#include <vector>
namespace outrun::platform::pmt_records {
using u32=std::uint32_t;
// Object record (0x3C), the bank's object table from system +0x18.
struct PmtObject {
    u32 unknown00;
    u32 index_buffers;   // +04 IB handle per group
    u32 vertex_buffers;  // +08 four VB handles per group (animation frames)
    u32 shader_records;  // +0C PmtShaderRecord per (group, material)
    u32 matrices;        // +10 node matrices (0x40)
    u32 unknown14;
    u32 info;            // +18 +2C material count (shader stride), +30 colour count
    u32 nodes;           // +1C PmtNode
    u32 meshes;          // +20 PmtMesh
    u32 primitives;      // +24 PmtPrimitive
    u32 draws;           // +28 PmtDraw
    u32 ranges;          // +2C PmtRange
    u32 groups;          // +30 PmtGroup
    u32 materials;       // +34 PmtMaterial
    u32 colours;         // +38 0x48-byte colour records (D3DMATERIAL9 + alpha)
};
// Node of the object's tree (0x38).
struct PmtNode {
    u32 flags;           // +00 1 opaque pass, 2 blended pass, 0x10 LOD by size, 0x20 texture
                         //     animation, 0x40 LOD threshold 7, 0x200 per-primitive, 0x800 faces the camera
    float centre[3];     // +04 bounding sphere
    float radius;        // +10
    std::int32_t lod_bias;   // +14
    u32 animation;       // +18 40ECC0 animation index
    std::int32_t matrix; // +1C node matrix (-1: none)
    std::int32_t child;  // +20 first child (-1: none)
    std::int32_t sibling;// +24 next sibling (-1: none)
    std::int32_t mesh[4];// +28 mesh per LOD (-1: none)
};
struct PmtMesh { u32 first_primitive; std::int32_t primitives; };
struct PmtPrimitive { u32 group; u32 first_draw[2]; std::int32_t draws[2]; };     // [pass]
struct PmtDraw { u32 base_vertex,material,range,unknown[5]; };
struct PmtRange { u32 type,start,count,unknown; };
struct PmtGroup { std::int32_t frames; u32 unknown04[6]; u32 vertex_bytes; u32 unknown20; u32 stride; u32 unknown28; };
struct PmtShaderRecord { u32 vertex_shader,declaration,keeps_light_state,unknown[3]; };
struct PmtLayer {
    u32 flags;           // +00 409430 stage flags
    u32 unknown04;
    u32 lod_bias;        // +08 D3DSAMP_MIPMAPLODBIAS
    u32 bump_matrix;     // +0C D3DTSS_BUMPENVMAT00/11
    u32 texture;         // +10 bank texture index (-1 none)
};
struct PmtMaterial {
    u32 colour_index;    // +00 colour record
    u32 flags;           // +04 408F90 render state flags
    PmtLayer layer[3];   // +08
    u32 unknown44[4];
    u32 environment_texture; // +54 texture of the 408F90 mode-5 reflection
};
static_assert(sizeof(PmtObject)==0x3c&&sizeof(PmtNode)==0x38&&sizeof(PmtPrimitive)==0x14&&sizeof(PmtDraw)==0x20&&
              sizeof(PmtRange)==0x10&&sizeof(PmtGroup)==0x2c&&sizeof(PmtShaderRecord)==0x18&&sizeof(PmtMaterial)==0x58);
template<class T> T pmt(const std::uint8_t* data,std::size_t size,u32 offset){
    if(std::size_t(offset)+sizeof(T)>size)driving::Bytes::out_of_view(offset,sizeof(T),size);
    T r;std::memcpy(&r,data+offset,sizeof(T));return r;
}
template<class T> T pmt(const std::vector<std::uint8_t>& system,u32 offset){return pmt<T>(system.data(),system.size(),offset);}
template<class T> T pmt(const driving::Bytes& system,u32 offset){return pmt<T>(system.data(),system.size(),offset);}
inline u32 pmt_u32(const std::vector<std::uint8_t>& system,u32 offset){return pmt<u32>(system,offset);}

// Render queue entry: the first 15 words of the PC draw descriptor (405360).
struct DrawEntry {
    u32 depth;           // +00 float, 4499E0 sort key (view z of the node centre)
    u32 matrix;          // +04 matrix pool index
    u32 palette_count;   // +08 matrices of the palette (1: world only)
    u32 id_high;         // +0C object ID & 0xFFFF0000 (texture keys)
    u32 resource;        // +10 bank
    u32 object;          // +14 PmtObject offset
    u32 node;            // +18 PmtNode offset
    u32 mesh;            // +1C PmtMesh offset
    u32 pass;            // +20 0 opaque pass, 1 blended pass (written by the flush)
    u32 colour_list;     // +24 408C80 colour list (0: none)
    u32 colour;          // +28 colour list index
    u32 morph_target;    // +2C 405450 target object (0: none)
    u32 morph_weight;    // +30 float
    u32 flags;           // +34 bit 0: cull mode 3 (mirrored)
    u32 id;              // +38
};
static_assert(sizeof(DrawEntry)==60);
}
