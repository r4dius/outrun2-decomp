#pragma once
// Named layouts of the PC renderer globals kept by PcRenderGlobals.
//
// The words still live at their PC addresses (PcRenderGlobals::w, and the
// translated modules map the same storage), so every struct below overlays
// one modelled range exactly; the static_asserts pin each field to its PC
// address. All fields are 32-bit words: float fields are read and written
// with f32()/set_f32() (memcpy), never through a float member.
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
namespace outrun::platform::render_state {
using u32=std::uint32_t;
inline float f32(u32 bits){float f;std::memcpy(&f,&bits,4);return f;}
inline u32 bits(float f){u32 u;std::memcpy(&u,&f,4);return u;}
using Matrix=std::array<float,16>;
inline Matrix load(const u32 (&m)[16]){Matrix r;std::memcpy(r.data(),m,64);return r;}
inline void store(u32 (&m)[16],const Matrix& r){std::memcpy(m,r.data(),64);}
inline std::array<float,4> load4(const u32* v){std::array<float,4> r;std::memcpy(r.data(),v,16);return r;}

// 897D30: the two pass records (4044F0 / 404540) applied by 404600.
struct PassRecord {
    u32 enabled;          // +00 pass drawn at all
    u32 alpha_blend;      // +04 D3DRS_ALPHABLENDENABLE
    u32 alpha_test;       // +08 40AE80 alpha test enable
    u32 alpha_ref;        // +0C D3DRS_ALPHAREF
    u32 z_write;          // +10 D3DRS_ZWRITEENABLE
    u32 colour_write;     // +14 D3DRS_COLORWRITEENABLE mask
};
static_assert(sizeof(PassRecord)==0x18);

// 89A4F8: what the current flush has bound (408AF0 resets it, 408C80 /
// 408F90 / 409430 compare against it).
struct MaterialCache {
    u32 object_key;            // 89A4F8 colour record owner (symbolic, PcRenderGlobals::object_key)
    u32 texture_key[4];        // 89A4FC stage texture ID (-2 unknown, -1 none)
    u32 unknown_89a50c[4];     // 89A50C
    u32 saved_texture3;        // 89A51C stage-3 texture before the flush (GetTexture reference)
    u32 unknown_89a520[3];     // 89A520
    u32 saved_address3;        // 89A52C stage-3 D3DSAMP_ADDRESSU before the flush
    u32 material_flags;        // 89A530 last material +04
    u32 colour_index;          // 89A534 last material +00 (-2: ambient pass needed)
    u32 layer_flags[4];        // 89A538 last layer +00 of each stage
    u32 normal_cube;           // 89A548 420B80 normalisation cube map
    u32 saved_fog_enable;      // 89A54C D3DRS_FOGENABLE of the layer
    u32 unknown_89a550[2];     // 89A550
};
static_assert(sizeof(MaterialCache)==0x60);

// One combiner entry of a stage record (40B710 / 40B760 / 40B7B0).
struct CombinerEntry {
    u32 colour_op,colour_arg;  // +00
    u32 alpha_op,alpha_arg;    // +08
    u32 constant_a,constant_b; // +10 40B4B0 pixel constants 2s / 2s+1 (ARGB)
    u32 mask_a,mask_b;         // +18 (0xF at init)
};
static_assert(sizeof(CombinerEntry)==0x20);
struct CombinerStage {
    u32 colour_count;          // +00 colour entries in use
    u32 alpha_count;           // +04 alpha entries in use
    CombinerEntry entry[9];    // +08
};
static_assert(sizeof(CombinerStage)==0x128);
// 89B5F8: per texture stage (4 records of 0x10).
struct StageTexture {
    u32 unknown0;              // +00
    u32 bound;                 // +04 a texture is bound (40B0C0 mask 89BD18)
    u32 unknown8;              // +08
    u32 init_index;            // +0C max(stage-1, 0) at 40AC60
};
// 89BC28: the combiner block 40B0C0 builds and 40B200 hashes (0x108 bytes).
struct CombinerBlock {
    u32 alpha_op[9];           // 89BC28
    u32 unknown_89bc4c[2];     // 89BC4C (0x130C0305, 0x1C80 at init)
    u32 constant_a[9];         // 89BC54
    u32 constant_b[9];         // 89BC78
    u32 alpha_arg[9];          // 89BC9C
    u32 colour_op[9];          // 89BCC0
    u32 unknown_89bce4[3];     // 89BCE4
    u32 colour_arg[9];         // 89BCF0
    u32 slots;                 // 89BD14 slot count | 0x11100
    u32 bound_mask;            // 89BD18 stage bound flags, 5 bits each (stage 0 lowest)
    u32 unknown_89bd1c[5];     // 89BD1C
};
static_assert(sizeof(CombinerBlock)==0x108);
// 89B5B4..89BD47: pixel pipeline state (40AC60 init, 40AF80 apply).
struct PixelPipeline {
    u32 fixed_shader[12];      // 89B5B4 40AF40 fixed pixel shaders, by mode 89BC20
    u32 colour_mode[5];        // 89B5E4 40B800 mode of each stage
    StageTexture texture[4];   // 89B5F8
    CombinerStage stage[5];    // 89B638 (stage 4 = the final alpha stage)
    u32 dirty;                 // 89BC00 block and shader must be rebuilt
    u32 alpha_test;            // 89BC04 40AE80
    u32 colour_write_rgb;      // 89BC08 colour write mask without alpha
    u32 colour_write;          // 89BC0C colour write mask
    u32 alpha_ref;             // 89BC10
    u32 unknown_89bc14;        // 89BC14 (1 after 40AE80)
    u32 alpha_scale;           // 89BC18 float: material alpha factor (8A8C1C based)
    u32 applied_mode;          // 89BC1C fixed shader mode last applied
    u32 mode;                  // 89BC20 fixed shader mode (0: combiner shader)
    u32 unknown_89bc24;        // 89BC24
    CombinerBlock block;       // 89BC28
    u32 alpha_mode[5];         // 89BD30 40B7B0 alpha default mode of each stage
    u32 unknown_89bd44;        // 89BD44
};
static_assert(sizeof(PixelPipeline)==0x794);
// The stage-4 alpha constant 40B4B0 writes lands in stage 3's first entry
// (89B9CC = 89B638 + 3*0x128 + 8 + 0x14): kept as the PC does.

// 89BDB4..89EDEB: texture animation table and vertex shader switches.
struct AnimationState {
    u32 frame;                 // 89BDB4 40ECC0 integer frame (stream pair of 404700)
    struct Entry { u32 base,count,unknown; } table[0x400]; // 89BDB8
    u32 clock;                 // 89EDB8
    u32 unknown_89edbc[3];     // 89EDBC
    u32 specular_variant;      // 89EDC8 40B800 stage 0 mode 4 promoted to 5
    u32 specular_colours;      // 89EDCC 410D00 specular light colours loaded
    u32 specular_request;      // 89EDD0
    u32 unknown_89edd4[3];     // 89EDD4
    u32 unknown_89ede0[2];     // 89EDE0
    u32 unknown_89ede8;        // 89EDE8
};
static_assert(sizeof(AnimationState)==0x3038);

// D3DLIGHT9 as the light records keep it (0x68 bytes).
struct Light {
    u32 type;                  // 1 point, 2 spot, 3 directional
    u32 diffuse[4],specular[4],ambient[4];
    u32 position[3],direction[3];
    u32 range,falloff,attenuation[3],theta,phi;
};
static_assert(sizeof(Light)==0x68);
// 95D860..95DF03: transform slots, light state and texture matrices.
struct TransformState {
    u32 slot[10][16];          // 95D860 410F90 slots: 0 view, 1 projection, 2..5 texture, 6 world, 7..9 palette
    u32 view_projection[16];   // 95DAE0
    u32 world_view[16];        // 95DB20
    u32 world_view_projection[16]; // 95DB60
    u32 inverse_view[16];      // 95DBA0
    u32 inverse_world[16];     // 95DBE0
    u32 light_colour[4];       // 95DC20 environment light colour (40DD70)
    u32 ambient[4];            // 95DC30 ambient (410710 / 40DD70)
    u32 ambient_lit[4];        // 95DC40 ambient + directional ambient (410810)
    u32 eye[3];                // 95DC50 camera position (inverse view row 3)
    u32 directional_count;     // 95DC5C 4107A0 counters
    u32 spot_count;            // 95DC60
    u32 point_count;           // 95DC64
    u32 light_enabled[5];      // 95DC68 0 directional, 1..2 spot, 3..4 point
    Light light[5];            // 95DC7C
    u32 light_specular[5][4];  // 95DE84 specular scaled by the power (+0C raw)
    u32 texture_matrix_type[4];// 95DED4 0x10000 / 0x20000 / 0x30000
    u32 texture_matrix_mode[4];// 95DEE4 411230 mode of each stage
    u32 texture_matrix_eye[4]; // 95DEF4 recomputed with the world (411060)
};
static_assert(sizeof(TransformState)==0x6a4);

// 95BF40..95C057: view volume and environment matrices.
struct ViewState {
    u32 frustum[6];            // 95BF40 l, r, b, t, near, far
    u32 planes[4][4];          // 95BF58
    u32 environment[16];       // 95BF98 408A80 environment mapping matrix
    u32 inverse_view[16];      // 95BFD8
    u32 environment_base[16];  // 95C018 4089A0
};
static_assert(sizeof(ViewState)==0x118);

// 95AEE8..95AF0F: current combiner pixel shader and its switches.
struct ShaderState {
    u32 pixel_shader;          // 95AEE8 40B200 result
    u32 unknown_95aeec[2];     // 95AEEC (the PC shader cache list; host vector instead)
    u32 skip_final_word;       // 95AEF4 byte 0: final alpha stage left out of the block
    u32 unknown_95aef8[4];     // 95AEF8
    u32 filter_flags;          // 95AF08 byte 2 (95AF0A) MINFILTER, byte 3 (95AF0B) MAGFILTER anisotropic
    u32 unknown_95af0c;        // 95AF0C
    bool skip_final()const{return (skip_final_word&0xffu)!=0;}
    void set_skip_final(std::uint8_t b){skip_final_word=(skip_final_word&~0xffu)|b;}
    bool anisotropic_min()const{return ((filter_flags>>16)&0xffu)!=0;}
    bool anisotropic_mag()const{return ((filter_flags>>24)&0xffu)!=0;}
};
static_assert(sizeof(ShaderState)==0x28);

// 8A8A00..8A8C5B: car reflection state (pc_car_reflection.cpp); the flush
// reads its colour constants and the alpha scale.
struct ReflectionState {
    u32 depth_surface;         // 8A8A00
    u32 colour_constant[4][4]; // 8A8A04 408F90 vertex constant 9 by material bits 27..28
    u32 unknown_8a8a44[3];     // 8A8A44
    u32 face_matrix[6][16];    // 8A8A50
    u32 unknown_8a8bd0[19];    // 8A8BD0
    u32 alpha;                 // 8A8C1C float 40AE80 alpha scale source
    u32 unknown_8a8c20[15];    // 8A8C20
};
static_assert(sizeof(ReflectionState)==0x25c);
}
