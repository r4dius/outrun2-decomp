#pragma once
// PC material vertex shader setup (40FD70 and helpers): vertex declarations,
// the material configuration word, the vertex shader linker and the shader
// cache 95C058.
#include "driving/pc_driving.hpp"
#include "platform/pc_d3d9.hpp"
#include <array>
#include <vector>
namespace outrun::platform {
using PcVertexDeclaration=std::array<PcVertexElement,PcMaxFvfDeclSize>;
// D3DXDeclaratorFromFVF (d3dx9_29, import 5962A0). False for an FVF the
// library rejects.
bool d3dx_declarator_from_fvf(std::uint32_t fvf,PcVertexDeclaration& out);
// PC 4101C0 declaration part: the FVF declaration plus the stream-1
// position/normal (and colour/texcoord) elements of type-2 groups.
void type2_declaration_4101c0(std::uint32_t fvf,PcVertexDeclaration& out);
// PC 40FEE0 declaration: as 4101C0 with the stream-1 TEXCOORD usage indexes
// starting at the set count.
void type2_declaration_40fee0(std::uint32_t fvf,PcVertexDeclaration& out);
// PC 40E010: texture coordinate source of one material layer (0x14 bytes)
// as {index, kind}. `elements` may be null (the PC then accepts any set).
std::array<std::uint32_t,2> layer_texcoord_40e010(driving::Bytes layer,const PcVertexElement* elements,std::uint32_t type);
// PC 40DE10: configuration word of a material from its group (0x2C) and
// material record (0x58).
std::uint32_t material_config_40de10(driving::Bytes group,driving::Bytes material,const PcVertexElement* elements);
// PC 40E140: links the vs_1_1 shader of a configuration word from the
// EXE-owned fragments (tokens end with 0x0000FFFF).
std::vector<std::uint32_t> link_vertex_shader_40e140(std::uint32_t config);
// 43A6F0: Adler-32 (initial value 1) as used by the shader caches.
std::uint32_t pc_adler32_43a6f0(const std::uint8_t* data,std::size_t size);
// Words of the embedded EXE renderer data by PC address (throws outside).
std::uint32_t pc_shader_data_u32(std::uint32_t va);
std::vector<std::uint32_t> pc_shader_tokens(std::uint32_t va);
PcVertexDeclaration pc_shader_declaration(std::uint32_t va);
// Shader cache 95C058 (0x18-byte entries, count +1800, high water +1804).
struct PcShaderCache {
    struct Entry { std::uint32_t hash{},key{},key3{},shader{},declaration{},references{}; };
    std::array<Entry,256> entries{};
    std::uint32_t count{},high_water{};
};
// PC 40F4E0: shader (and declaration) for (tokens, key, key3), created on a
// miss; the cache key is Adler-32 of the tokens without the end token.
std::uint32_t shader_cache_40f4e0(PcShaderCache&,PcD3D9Device&,std::uint32_t key,const PcVertexElement* elements,
    const std::uint32_t* tokens,std::uint32_t key3,std::uint32_t& declaration);
// PC 40F650: releases every cached object (after SetVertexShader(NULL)).
void shader_cache_release_40f650(PcShaderCache&,PcD3D9Device&);
// Renderer globals read by the material setup.
struct PcShaderGlobals {
    std::uint32_t mode_89edbc{};      // set by 4103A0
    std::uint32_t override_89edd4{};  // set by 40BDD6/40BECE
    std::uint32_t generated_key_1039ec0{1}; // protected-section constant read by bridge 4CC208
};
// PC 40FD70: sets up the 0x18-byte shader record of one material:
// +00 shader, +04 declaration, +08 cache key, +0C configuration, +14 flag.
void material_setup_40fd70(driving::Bytes shader_record,driving::Bytes material,driving::Bytes group,
    PcD3D9Device&,PcShaderCache&,const PcShaderGlobals&);
// PC 4104D0: shader record of one material for a 4103F0 kind (1 linked,
// 2/3 fixed, 5 and 10 by FVF, the others table shader 74209C[kind]).
void material_kind_setup_4104d0(driving::Bytes shader_record,driving::Bytes material,driving::Bytes group,std::uint32_t kind,
    PcD3D9Device&,PcShaderCache&,const PcShaderGlobals&);
}
