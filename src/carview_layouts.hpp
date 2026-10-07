#pragma once
#include <cstddef>
#include <cstdint>
// Generated DWARF metadata, carview SHA256 ab3b658fc359d22afb200bd2b9d06615729ec44c22449a84b0e78b0a0c635120
// Not host memory overlays. Not automatically the PC/primary-game ABI.
namespace outrun::carview_layout {
// gz.cpp:1705; DIE 0x3db47
struct XPR_HEADER {
    static constexpr std::size_t byte_size = 12;
    static constexpr std::size_t dwMagic_offset = 0; // long unsigned int
    static constexpr std::size_t dwMagic_size = 4;
    static constexpr std::size_t dwTotalSize_offset = 4; // long unsigned int
    static constexpr std::size_t dwTotalSize_size = 4;
    static constexpr std::size_t dwHeaderSize_offset = 8; // long unsigned int
    static constexpr std::size_t dwHeaderSize_size = 4;
};

// gz.cpp:1891; DIE 0x3ec15
struct D3DTexture {
    static constexpr std::size_t byte_size = 20;
    static constexpr std::size_t Common_offset = 0; // long unsigned int
    static constexpr std::size_t Common_size = 4;
    static constexpr std::size_t Data_offset = 4; // long unsigned int
    static constexpr std::size_t Data_size = 4;
    static constexpr std::size_t Lock_offset = 8; // long unsigned int
    static constexpr std::size_t Lock_size = 4;
    static constexpr std::size_t n__offset = 12; // union_type@0x3ec4f
    static constexpr std::size_t n__size = 4;
    static constexpr std::size_t Size_offset = 16; // long unsigned int
    static constexpr std::size_t Size_size = 4;
};

// d3d_form.h:23; DIE 0x5a5d2
struct CullNodeFlags {
    static constexpr std::size_t byte_size = 4;
    static constexpr std::int64_t B_CA_OPAQUE = 0;
    static constexpr std::int64_t B_CA_TRANSPARENT = 1;
    static constexpr std::int64_t B_CA_INSTANCE_ORG = 2;
    static constexpr std::int64_t B_CA_INSTANCE_REF = 3;
    static constexpr std::int64_t B_CA_LOD = 4;
    static constexpr std::int64_t B_CA_TWEENING = 5;
    static constexpr std::int64_t B_CA_CULL_FAR = 6;
    static constexpr std::int64_t B_CA_KAGE = 7;
    static constexpr std::int64_t B_CA_SHADOW = 8;
    static constexpr std::int64_t B_CA_MATERIAL_SORT = 9;
    static constexpr std::int64_t B_CA_ALWAYS = 10;
    static constexpr std::int64_t B_CA_BILLBOARD = 11;
};

// d3d_form.h:39; DIE 0x5a632
struct M_CullNodeFlags {
    static constexpr std::size_t byte_size = 4;
    static constexpr std::int64_t M_CA_OPAQUE = 1;
    static constexpr std::int64_t M_CA_TRANSPARENT = 2;
    static constexpr std::int64_t M_CA_INSTANCE_ORG = 4;
    static constexpr std::int64_t M_CA_INSTANCE_REF = 8;
    static constexpr std::int64_t M_CA_LOD = 16;
    static constexpr std::int64_t M_CA_TWEENING = 32;
    static constexpr std::int64_t M_CA_CULL_FAR = 64;
    static constexpr std::int64_t M_CA_KAGE = 128;
    static constexpr std::int64_t M_CA_SHADOW = 256;
    static constexpr std::int64_t M_CA_MATERIAL_SORT = 512;
    static constexpr std::int64_t M_CA_ALWAYS = 1024;
    static constexpr std::int64_t M_CA_BILLBOARD = 2048;
};

// d3d_form.h:55; DIE 0x5a696
struct MatAttrFlags {
    static constexpr std::size_t byte_size = 4;
    static constexpr std::int64_t B_MA_SPECULAR = 0;
    static constexpr std::int64_t B_MA_DOUBLESIDE = 1;
    static constexpr std::int64_t B_MA_DOUBLESIDE_LIGHTING = 2;
    static constexpr std::int64_t B_MA_ZBIAS = 3;
    static constexpr std::int64_t B_MA_NOFOG = 4;
};

// d3d_form.h:64; DIE 0x5a6cc
struct M_MatAttrFlags {
    static constexpr std::size_t byte_size = 4;
    static constexpr std::int64_t M_MA_SPECULAR = 1;
    static constexpr std::int64_t M_MA_DOUBLESIDE = 2;
    static constexpr std::int64_t M_MA_DOUBLESIDE_LIGHTING = 4;
    static constexpr std::int64_t M_MA_ZBIAS = 8;
    static constexpr std::int64_t M_MA_NOFOG = 16;
};

// d3d_form.h:73; DIE 0x5a702
struct TexAttrFlags {
    static constexpr std::size_t byte_size = 4;
    static constexpr std::int64_t B_TA_SPECULARMAP = 0;
    static constexpr std::int64_t B_TA_ENVMAP_LIGHTING = 1;
    static constexpr std::int64_t B_TA_ENVMAP_SPHERE = 2;
    static constexpr std::int64_t B_TA_ENVMAP_CUBE = 3;
    static constexpr std::int64_t B_TA_VOLUMETEX = 4;
    static constexpr std::int64_t B_TA_COMBINE_BUMPSPHEREMAP = 5;
    static constexpr std::int64_t B_TA_PROJECTION = 6;
    static constexpr std::int64_t B_TA_FRESNEL = 7;
};

// d3d_form.h:88; DIE 0x5a74a
struct M_TexAttrFlags {
    static constexpr std::size_t byte_size = 4;
    static constexpr std::int64_t M_TA_SPECULARMAP = 1;
    static constexpr std::int64_t M_TA_ENVMAP_LIGHTING = 2;
    static constexpr std::int64_t M_TA_ENVMAP_SPHERE = 4;
    static constexpr std::int64_t M_TA_ENVMAP_CUBE = 8;
    static constexpr std::int64_t M_TA_VOLUMETEX = 16;
    static constexpr std::int64_t M_TA_COMBINE_BUMPSPHEREMAP = 32;
    static constexpr std::int64_t M_TA_PROJECTION = 64;
    static constexpr std::int64_t M_TA_FRESNEL = 128;
};

// d3d_form.h:103; DIE 0x5a792
struct PixelShaderFlags {
    static constexpr std::size_t byte_size = 4;
    static constexpr std::int64_t B_PS_NULL = 0;
    static constexpr std::int64_t B_PS_BUMPSPHEREMAP_2 = 1;
    static constexpr std::int64_t B_PS_BUMPSPHEREMAP_3 = 2;
    static constexpr std::int64_t B_PS_BUMPSPHEREMAP_4 = 3;
    static constexpr std::int64_t B_PS_BUMPDOTMAP = 4;
    static constexpr std::int64_t B_PS_BUMPCUBEMAP = 5;
    static constexpr std::int64_t B_PS_MAX = 6;
};

// d3d_form.h:115; DIE 0x5a7d4
struct AlphaClassfy {
    static constexpr std::size_t byte_size = 4;
    static constexpr std::int64_t AC_OPAQUE = 0;
    static constexpr std::int64_t AC_TRANSPARENT = 1;
    static constexpr std::int64_t ALPHA_CLASSFY_MAX = 2;
};

// d3d_form.h:123; DIE 0x5a7fe
struct VertexShaderType {
    static constexpr std::size_t byte_size = 4;
    static constexpr std::int64_t VSTYPE_FIXED_FUNCTION = 0;
    static constexpr std::int64_t VSTYPE_BUMPDOTMAP = 1;
    static constexpr std::int64_t VSTYPE_TWEENING = 2;
    static constexpr std::int64_t VSTYPE_BUMPCUBEMAP = 3;
    static constexpr std::int64_t VSTYPE_MAX = 4;
};

// d3d_form.h:148; DIE 0x5a834
struct ObjectHeader {
    static constexpr std::size_t byte_size = 52;
    static constexpr std::size_t offset_cull_nodes_offset = 0; // long unsigned int
    static constexpr std::size_t offset_cull_nodes_size = 4;
    static constexpr std::size_t offset_matrices_offset = 4; // long unsigned int
    static constexpr std::size_t offset_matrices_size = 4;
    static constexpr std::size_t offset_models_offset = 8; // long unsigned int
    static constexpr std::size_t offset_models_size = 4;
    static constexpr std::size_t offset_vtx_groups_offset = 12; // long unsigned int
    static constexpr std::size_t offset_vtx_groups_size = 4;
    static constexpr std::size_t offset_mat_groups_offset = 16; // long unsigned int
    static constexpr std::size_t offset_mat_groups_size = 4;
    static constexpr std::size_t offset_primitives_offset = 20; // long unsigned int
    static constexpr std::size_t offset_primitives_size = 4;
    static constexpr std::size_t offset_vtx_formats_offset = 24; // long unsigned int
    static constexpr std::size_t offset_vtx_formats_size = 4;
    static constexpr std::size_t offset_materials_offset = 28; // long unsigned int
    static constexpr std::size_t offset_materials_size = 4;
    static constexpr std::size_t offset_mat_colors_offset = 32; // long unsigned int
    static constexpr std::size_t offset_mat_colors_size = 4;
    static constexpr std::size_t numof_vtx_formats_offset = 36; // int
    static constexpr std::size_t numof_vtx_formats_size = 4;
    static constexpr std::size_t numof_mat_groups_offset = 40; // int
    static constexpr std::size_t numof_mat_groups_size = 4;
    static constexpr std::size_t numof_materials_offset = 44; // int
    static constexpr std::size_t numof_materials_size = 4;
    static constexpr std::size_t numof_mat_colors_offset = 48; // int
    static constexpr std::size_t numof_mat_colors_size = 4;
};

// d3d_form.h:168; DIE 0x5a962
struct CullingNode {
    static constexpr std::size_t byte_size = 56;
    static constexpr std::size_t flags_offset = 0; // long unsigned int
    static constexpr std::size_t flags_size = 4;
    static constexpr std::size_t center_offset = 4; // float[3]
    static constexpr std::size_t center_size = 12;
    static constexpr std::size_t radius_offset = 16; // float
    static constexpr std::size_t radius_size = 4;
    static constexpr std::size_t offset_lod_offset = 20; // int
    static constexpr std::size_t offset_lod_size = 4;
    static constexpr std::size_t tweening_factor_offset = 24; // long unsigned int
    static constexpr std::size_t tweening_factor_size = 4;
    static constexpr std::size_t index_matrix_offset = 28; // int
    static constexpr std::size_t index_matrix_size = 4;
    static constexpr std::size_t index_child_offset = 32; // int
    static constexpr std::size_t index_child_size = 4;
    static constexpr std::size_t index_sibling_offset = 36; // int
    static constexpr std::size_t index_sibling_size = 4;
    static constexpr std::size_t index_model_offset = 40; // int[4]
    static constexpr std::size_t index_model_size = 16;
};

// d3d_form.h:181; DIE 0x5aa68
struct ModelHeader {
    static constexpr std::size_t byte_size = 8;
    static constexpr std::size_t index_vtx_groups_offset = 0; // int
    static constexpr std::size_t index_vtx_groups_size = 4;
    static constexpr std::size_t numof_vtx_groups_offset = 4; // int
    static constexpr std::size_t numof_vtx_groups_size = 4;
};

// d3d_form.h:187; DIE 0x5aafc
struct VtxGroupInfo {
    static constexpr std::size_t byte_size = 20;
    static constexpr std::size_t index_vtxfmt_offset = 0; // int
    static constexpr std::size_t index_vtxfmt_size = 4;
    static constexpr std::size_t index_mat_groups_offset = 4; // long unsigned int[2]
    static constexpr std::size_t index_mat_groups_size = 8;
    static constexpr std::size_t numof_mat_groups_offset = 12; // int[2]
    static constexpr std::size_t numof_mat_groups_size = 8;
};

// d3d_form.h:194; DIE 0x5abae
struct MatGroupInfo {
    static constexpr std::size_t byte_size = 32;
    static constexpr std::size_t base_index_offset = 0; // long unsigned int
    static constexpr std::size_t base_index_size = 4;
    static constexpr std::size_t index_material_offset = 4; // int
    static constexpr std::size_t index_material_size = 4;
    static constexpr std::size_t index_primitives_offset = 8; // int
    static constexpr std::size_t index_primitives_size = 4;
    static constexpr std::size_t numof_primitives_offset = 12; // int
    static constexpr std::size_t numof_primitives_size = 4;
    static constexpr std::size_t center_offset = 16; // float[3]
    static constexpr std::size_t center_size = 12;
    static constexpr std::size_t radius_offset = 28; // float
    static constexpr std::size_t radius_size = 4;
};

// d3d_form.h:206; DIE 0x5ac7a
struct PrimitiveList {
    static constexpr std::size_t byte_size = 16;
    static constexpr std::size_t primitive_type_offset = 0; // long unsigned int
    static constexpr std::size_t primitive_type_size = 4;
    static constexpr std::size_t start_index_offset = 4; // long unsigned int
    static constexpr std::size_t start_index_size = 4;
    static constexpr std::size_t primitive_count_offset = 8; // long unsigned int
    static constexpr std::size_t primitive_count_size = 4;
    static constexpr std::size_t polygon_count_offset = 12; // long unsigned int
    static constexpr std::size_t polygon_count_size = 4;
};

// d3d_form.h:215; DIE 0x5ad2a
struct VtxFormatList {
    static constexpr std::size_t byte_size = 44;
    static constexpr std::size_t numof_stream_offset = 0; // int
    static constexpr std::size_t numof_stream_size = 4;
    static constexpr std::size_t offset_indices_offset = 4; // long unsigned int
    static constexpr std::size_t offset_indices_size = 4;
    static constexpr std::size_t offset_vertices_offset = 8; // long unsigned int[4]
    static constexpr std::size_t offset_vertices_size = 16;
    static constexpr std::size_t index_buffer_size_offset = 24; // long unsigned int
    static constexpr std::size_t index_buffer_size_size = 4;
    static constexpr std::size_t vertex_buffer_size_offset = 28; // long unsigned int
    static constexpr std::size_t vertex_buffer_size_size = 4;
    static constexpr std::size_t vertex_format_offset = 32; // long unsigned int
    static constexpr std::size_t vertex_format_size = 4;
    static constexpr std::size_t vertex_format_size_offset = 36; // long unsigned int
    static constexpr std::size_t vertex_format_size_size = 4;
    static constexpr std::size_t vertex_shader_type_offset = 40; // VertexShaderType
    static constexpr std::size_t vertex_shader_type_size = 4;
};

// d3d_form.h:228; DIE 0x5ae22
struct MatAttrib_member {
    static constexpr std::size_t byte_size = 4;
    static constexpr std::size_t flags_offset = 0; // unsigned int
    static constexpr std::size_t flags_size = 4;
    static constexpr unsigned flags_shift = 0;
    static constexpr std::uint32_t flags_mask = 0x000000ffu;
    static constexpr std::size_t src_blend_factor_offset = 0; // unsigned int
    static constexpr std::size_t src_blend_factor_size = 4;
    static constexpr unsigned src_blend_factor_shift = 8;
    static constexpr std::uint32_t src_blend_factor_mask = 0x00000f00u;
    static constexpr std::size_t dst_blend_factor_offset = 0; // unsigned int
    static constexpr std::size_t dst_blend_factor_size = 4;
    static constexpr unsigned dst_blend_factor_shift = 12;
    static constexpr std::uint32_t dst_blend_factor_mask = 0x0000f000u;
    static constexpr std::size_t blend_operation_offset = 0; // unsigned int
    static constexpr std::size_t blend_operation_size = 4;
    static constexpr unsigned blend_operation_shift = 16;
    static constexpr std::uint32_t blend_operation_mask = 0x00070000u;
    static constexpr std::size_t pixelshader_offset = 0; // unsigned int
    static constexpr std::size_t pixelshader_size = 4;
    static constexpr unsigned pixelshader_shift = 19;
    static constexpr std::uint32_t pixelshader_mask = 0x00780000u;
    static constexpr std::size_t zbias_offset = 0; // unsigned int
    static constexpr std::size_t zbias_size = 4;
    static constexpr unsigned zbias_shift = 23;
    static constexpr std::uint32_t zbias_mask = 0x07800000u;
    static constexpr std::size_t ftype_offset = 0; // unsigned int
    static constexpr std::size_t ftype_size = 4;
    static constexpr unsigned ftype_shift = 27;
    static constexpr std::uint32_t ftype_mask = 0x18000000u;
};

// d3d_form.h:242; DIE 0x5af11
struct MatAttrib {
    static constexpr std::size_t byte_size = 4;
    static constexpr std::size_t m_size = 4;
    static constexpr std::size_t w_size = 4;
};

// d3d_form.h:248; DIE 0x5af9c
struct TexAttrib_member {
    static constexpr std::size_t byte_size = 4;
    static constexpr std::size_t flags_offset = 0; // unsigned int
    static constexpr std::size_t flags_size = 4;
    static constexpr unsigned flags_shift = 0;
    static constexpr std::uint32_t flags_mask = 0x000003ffu;
    static constexpr std::size_t addr_u_offset = 0; // unsigned int
    static constexpr std::size_t addr_u_size = 4;
    static constexpr unsigned addr_u_shift = 10;
    static constexpr std::uint32_t addr_u_mask = 0x00001c00u;
    static constexpr std::size_t addr_v_offset = 0; // unsigned int
    static constexpr std::size_t addr_v_size = 4;
    static constexpr unsigned addr_v_shift = 13;
    static constexpr std::uint32_t addr_v_mask = 0x0000e000u;
    static constexpr std::size_t filter_offset = 0; // unsigned int
    static constexpr std::size_t filter_size = 4;
    static constexpr unsigned filter_shift = 16;
    static constexpr std::uint32_t filter_mask = 0x00070000u;
    static constexpr std::size_t mipmap_offset = 0; // unsigned int
    static constexpr std::size_t mipmap_size = 4;
    static constexpr unsigned mipmap_shift = 19;
    static constexpr std::uint32_t mipmap_mask = 0x00180000u;
    static constexpr std::size_t blend_offset = 0; // unsigned int
    static constexpr std::size_t blend_size = 4;
    static constexpr unsigned blend_shift = 21;
    static constexpr std::uint32_t blend_mask = 0x03e00000u;
    static constexpr std::size_t alpha_blend_offset = 0; // unsigned int
    static constexpr std::size_t alpha_blend_size = 4;
    static constexpr unsigned alpha_blend_shift = 26;
    static constexpr std::uint32_t alpha_blend_mask = 0x0c000000u;
    static constexpr std::size_t coord_index_offset = 0; // unsigned int
    static constexpr std::size_t coord_index_size = 4;
    static constexpr unsigned coord_index_shift = 28;
    static constexpr std::uint32_t coord_index_mask = 0xf0000000u;
};

// d3d_form.h:270; DIE 0x5b09f
struct TexAttrib {
    static constexpr std::size_t byte_size = 4;
    static constexpr std::size_t m_size = 4;
    static constexpr std::size_t w_size = 4;
};

// d3d_form.h:276; DIE 0x5b12d
struct MatTexInfo {
    static constexpr std::size_t byte_size = 20;
    static constexpr std::size_t attrib_offset = 0; // TexAttrib
    static constexpr std::size_t attrib_size = 4;
    static constexpr std::size_t blendcolor_offset = 4; // long unsigned int
    static constexpr std::size_t blendcolor_size = 4;
    static constexpr std::size_t mipmap_bias_offset = 8; // float
    static constexpr std::size_t mipmap_bias_size = 4;
    static constexpr std::size_t bump_depth_offset = 12; // float
    static constexpr std::size_t bump_depth_size = 4;
    static constexpr std::size_t index_offset = 16; // int
    static constexpr std::size_t index_size = 4;
};

// d3d_form.h:285; DIE 0x5b1f2
struct MaterialList {
    static constexpr std::size_t byte_size = 88;
    static constexpr std::size_t index_color_offset = 0; // int
    static constexpr std::size_t index_color_size = 4;
    static constexpr std::size_t attrib_offset = 4; // MatAttrib
    static constexpr std::size_t attrib_size = 4;
    static constexpr std::size_t texture_offset = 8; // MatTexInfo[4]
    static constexpr std::size_t texture_size = 80;
};

// d3d_form.h:293; DIE 0x5b2a9
struct MaterialColor {
    static constexpr std::size_t byte_size = 72;
    static constexpr std::size_t diffuse_offset = 0; // float[4]
    static constexpr std::size_t diffuse_size = 16;
    static constexpr std::size_t ambient_offset = 16; // float[4]
    static constexpr std::size_t ambient_size = 16;
    static constexpr std::size_t specular_offset = 32; // float[4]
    static constexpr std::size_t specular_size = 16;
    static constexpr std::size_t emissive_offset = 48; // float[4]
    static constexpr std::size_t emissive_size = 16;
    static constexpr std::size_t power_offset = 64; // float
    static constexpr std::size_t power_size = 4;
    static constexpr std::size_t intensity_offset = 68; // float
    static constexpr std::size_t intensity_size = 4;
};

// xmtdef.h:9; DIE 0x5b381
struct TAG_xmtset_index {
    static constexpr std::size_t byte_size = 4;
    static constexpr std::int64_t XMT_COMMON = 0;
    static constexpr std::int64_t XMT_PLCAR_F50 = 1;
    static constexpr std::int64_t XMT_PLCAR_DINO = 2;
    static constexpr std::int64_t XMT_PLCAR_GTO = 3;
    static constexpr std::int64_t XMT_PLCAR_FX = 4;
    static constexpr std::int64_t XMT_PLCAR_DAYTS = 5;
    static constexpr std::int64_t XMT_PLCAR_F355S = 6;
    static constexpr std::int64_t XMT_PLCAR_TESTA = 7;
    static constexpr std::int64_t XMT_PLCAR_360S = 8;
    static constexpr std::int64_t XMT_PLCAR_F40 = 9;
    static constexpr std::int64_t XMT_PLCAR_512BB = 10;
    static constexpr std::int64_t XMT_PLCAR_250GTO = 11;
    static constexpr std::int64_t XMT_PLCAR_FXdummy = 12;
    static constexpr std::int64_t XMT_PLCAR_360 = 13;
    static constexpr std::int64_t XMT_OTHCAR = 14;
    static constexpr std::int64_t XMT_CS_PALM = 15;
    static constexpr std::int64_t XMT_CS_LAKE = 16;
    static constexpr std::int64_t XMT_CS_INDU = 17;
    static constexpr std::int64_t XMT_CS_ALPI = 18;
    static constexpr std::int64_t XMT_CS_SNOW = 19;
    static constexpr std::int64_t XMT_CS_CLOU = 20;
    static constexpr std::int64_t XMT_CS_CAST = 21;
    static constexpr std::int64_t XMT_CS_GHOS = 22;
    static constexpr std::int64_t XMT_CS_FORE = 23;
    static constexpr std::int64_t XMT_CS_DESE = 24;
    static constexpr std::int64_t XMT_CS_TULI = 25;
    static constexpr std::int64_t XMT_CS_METR = 26;
    static constexpr std::int64_t XMT_CS_RUIN = 27;
    static constexpr std::int64_t XMT_CS_CAPE = 28;
    static constexpr std::int64_t XMT_CS_IMPE = 29;
    static constexpr std::int64_t XMT_CS31 = 30;
    static constexpr std::int64_t XMT_CS32 = 31;
    static constexpr std::int64_t XMT_CS33 = 32;
    static constexpr std::int64_t XMT_CS34 = 33;
    static constexpr std::int64_t XMT_CS35 = 34;
    static constexpr std::int64_t XMT_BK_PALM = 35;
    static constexpr std::int64_t XMT_BK_LAKE = 36;
    static constexpr std::int64_t XMT_BK_INDU = 37;
    static constexpr std::int64_t XMT_BK_ALPI = 38;
    static constexpr std::int64_t XMT_BK_SNOW = 39;
    static constexpr std::int64_t XMT_BK_CLOU = 40;
    static constexpr std::int64_t XMT_BK_CAST = 41;
    static constexpr std::int64_t XMT_BK_GHOS = 42;
    static constexpr std::int64_t XMT_BK_FORE = 43;
    static constexpr std::int64_t XMT_BK_DESE = 44;
    static constexpr std::int64_t XMT_BK_TULI = 45;
    static constexpr std::int64_t XMT_BK_METR = 46;
    static constexpr std::int64_t XMT_BK_RUIN = 47;
    static constexpr std::int64_t XMT_BK_CAPE = 48;
    static constexpr std::int64_t XMT_BK_IMPE = 49;
    static constexpr std::int64_t XMT_CHA_DR_L00 = 50;
    static constexpr std::int64_t XMT_CHA_DR_M00 = 51;
    static constexpr std::int64_t XMT_CHA_AUT01 = 52;
    static constexpr std::int64_t XMT_CHA_AUT02 = 53;
    static constexpr std::int64_t XMT_CHA_AUT03 = 54;
    static constexpr std::int64_t XMT_CHA_AUT04 = 55;
    static constexpr std::int64_t XMT_CHA_MOB_CB01 = 56;
    static constexpr std::int64_t XMT_CHA_MOB_FN01 = 57;
    static constexpr std::int64_t XMT_CHA_MOB_MN01 = 58;
    static constexpr std::int64_t XMT_CHA_AUT05 = 59;
    static constexpr std::int64_t XMT_CHA_AUT06 = 60;
    static constexpr std::int64_t XMT_CHA_AUT07 = 61;
    static constexpr std::int64_t XMT_CHA_AUT08 = 62;
    static constexpr std::int64_t XMT_CHA_AUT09 = 63;
    static constexpr std::int64_t XMT_CHA_AUT10 = 64;
    static constexpr std::int64_t XMT_CHA_AUT11 = 65;
    static constexpr std::int64_t XMT_CHA_MOB_MN02 = 66;
    static constexpr std::int64_t XMT_CHA_MOB_MN03 = 67;
    static constexpr std::int64_t XMT_CHA_MOB_MN04 = 68;
    static constexpr std::int64_t XMT_CHA_MOB_MN05 = 69;
    static constexpr std::int64_t XMT_CHA_MOB_MN06 = 70;
    static constexpr std::int64_t XMT_LRBK_PALM = 71;
    static constexpr std::int64_t XMT_LRBK_LAKE = 72;
    static constexpr std::int64_t XMT_LRBK_INDU = 73;
    static constexpr std::int64_t XMT_LRBK_ALPI = 74;
    static constexpr std::int64_t XMT_LRBK_SNOW = 75;
    static constexpr std::int64_t XMT_LRBK_CLOU = 76;
    static constexpr std::int64_t XMT_LRBK_CAST = 77;
    static constexpr std::int64_t XMT_LRBK_GHOS = 78;
    static constexpr std::int64_t XMT_LRBK_FORE = 79;
    static constexpr std::int64_t XMT_LRBK_DESE = 80;
    static constexpr std::int64_t XMT_LRBK_TULI = 81;
    static constexpr std::int64_t XMT_LRBK_METR = 82;
    static constexpr std::int64_t XMT_LRBK_RUIN = 83;
    static constexpr std::int64_t XMT_LRBK_CAPE = 84;
    static constexpr std::int64_t XMT_LRBK_IMPE = 85;
    static constexpr std::int64_t XMT_CS_SHADOW = 86;
    static constexpr std::int64_t XMT_COURSE_OBJ_COMMON = 87;
    static constexpr std::int64_t XMT_COURSE_OBJ_CS_PALM = 88;
    static constexpr std::int64_t XMT_COURSE_OBJ_CS_LAKE = 89;
    static constexpr std::int64_t XMT_COURSE_OBJ_CS_INDU = 90;
    static constexpr std::int64_t XMT_COURSE_OBJ_CS_ALPI = 91;
    static constexpr std::int64_t XMT_COURSE_OBJ_CS_SNOW = 92;
    static constexpr std::int64_t XMT_COURSE_OBJ_CS_CLOU = 93;
    static constexpr std::int64_t XMT_COURSE_OBJ_CS_CAST = 94;
    static constexpr std::int64_t XMT_COURSE_OBJ_CS_GHOS = 95;
    static constexpr std::int64_t XMT_COURSE_OBJ_CS_FORE = 96;
    static constexpr std::int64_t XMT_COURSE_OBJ_CS_DESE = 97;
    static constexpr std::int64_t XMT_COURSE_OBJ_CS_TULI = 98;
    static constexpr std::int64_t XMT_COURSE_OBJ_CS_METR = 99;
    static constexpr std::int64_t XMT_COURSE_OBJ_CS_RUIN = 100;
    static constexpr std::int64_t XMT_COURSE_OBJ_CS_CAPE = 101;
    static constexpr std::int64_t XMT_COURSE_OBJ_CS_IMPE = 102;
    static constexpr std::int64_t XMT_COURSE_OBJ_BK_PALM = 103;
    static constexpr std::int64_t XMT_COURSE_OBJ_BK_LAKE = 104;
    static constexpr std::int64_t XMT_COURSE_OBJ_BK_INDU = 105;
    static constexpr std::int64_t XMT_COURSE_OBJ_BK_ALPI = 106;
    static constexpr std::int64_t XMT_COURSE_OBJ_BK_SNOW = 107;
    static constexpr std::int64_t XMT_COURSE_OBJ_BK_CLOU = 108;
    static constexpr std::int64_t XMT_COURSE_OBJ_BK_CAST = 109;
    static constexpr std::int64_t XMT_COURSE_OBJ_BK_GHOS = 110;
    static constexpr std::int64_t XMT_COURSE_OBJ_BK_FORE = 111;
    static constexpr std::int64_t XMT_COURSE_OBJ_BK_DESE = 112;
    static constexpr std::int64_t XMT_COURSE_OBJ_SKY_PALM = 113;
    static constexpr std::int64_t XMT_COURSE_OBJ_SKY_LAKE = 114;
    static constexpr std::int64_t XMT_COURSE_OBJ_SKY_INDU = 115;
    static constexpr std::int64_t XMT_COURSE_OBJ_SKY_ALPI = 116;
    static constexpr std::int64_t XMT_COURSE_OBJ_SKY_SNOW = 117;
    static constexpr std::int64_t XMT_COURSE_OBJ_SKY_CLOU = 118;
    static constexpr std::int64_t XMT_COURSE_OBJ_SKY_CAST = 119;
    static constexpr std::int64_t XMT_COURSE_OBJ_SKY_GHOS = 120;
    static constexpr std::int64_t XMT_COURSE_OBJ_SKY_FORE = 121;
    static constexpr std::int64_t XMT_COURSE_OBJ_SKY_DESE = 122;
    static constexpr std::int64_t XMT_COURSE_OBJ_SKY_TULI = 123;
    static constexpr std::int64_t XMT_COURSE_OBJ_SKY_METR = 124;
    static constexpr std::int64_t XMT_COURSE_OBJ_SKY_RUIN = 125;
    static constexpr std::int64_t XMT_COURSE_OBJ_SKY_CAPE = 126;
    static constexpr std::int64_t XMT_COURSE_OBJ_SKY_IMPE = 127;
    static constexpr std::int64_t XMT_RC_250GTO = 128;
    static constexpr std::int64_t XMT_RC_360SP = 129;
    static constexpr std::int64_t XMT_RC_DAYTS = 130;
    static constexpr std::int64_t XMT_RC_DINO = 131;
    static constexpr std::int64_t XMT_RC_FX = 132;
    static constexpr std::int64_t XMT_RC_512BB = 133;
    static constexpr std::int64_t XMT_RC_F40 = 134;
    static constexpr std::int64_t XMT_RC_F50 = 135;
    static constexpr std::int64_t XMT_RC_GTO = 136;
    static constexpr std::int64_t XMT_RC_TESTA = 137;
    static constexpr std::int64_t XMT_ENV_PALM = 138;
    static constexpr std::int64_t XMT_ENV_LAKE = 139;
    static constexpr std::int64_t XMT_ENV_INDU = 140;
    static constexpr std::int64_t XMT_ENV_ALPI = 141;
    static constexpr std::int64_t XMT_ENV_SNOW = 142;
    static constexpr std::int64_t XMT_ENV_CLOU = 143;
    static constexpr std::int64_t XMT_ENV_CAST = 144;
    static constexpr std::int64_t XMT_ENV_GHOS = 145;
    static constexpr std::int64_t XMT_ENV_FORE = 146;
    static constexpr std::int64_t XMT_ENV_DESE = 147;
    static constexpr std::int64_t XMT_ENV_TULI = 148;
    static constexpr std::int64_t XMT_ENV_METR = 149;
    static constexpr std::int64_t XMT_ENV_RUIN = 150;
    static constexpr std::int64_t XMT_ENV_CAPE = 151;
    static constexpr std::int64_t XMT_ENV_IMPE = 152;
    static constexpr std::int64_t XMT_ENVBK_PALM = 153;
    static constexpr std::int64_t XMT_ENVBK_LAKE = 154;
    static constexpr std::int64_t XMT_ENVBK_INDU = 155;
    static constexpr std::int64_t XMT_ENVBK_ALPI = 156;
    static constexpr std::int64_t XMT_ENVBK_SNOW = 157;
    static constexpr std::int64_t XMT_ENVBK_CLOU = 158;
    static constexpr std::int64_t XMT_ENVBK_CAST = 159;
    static constexpr std::int64_t XMT_ENVBK_GHOS = 160;
    static constexpr std::int64_t XMT_ENVBK_FORE = 161;
    static constexpr std::int64_t XMT_ENVBK_DESE = 162;
    static constexpr std::int64_t XMT_END_1ST = 163;
    static constexpr std::int64_t XMT_END_2ND = 164;
    static constexpr std::int64_t XMT_END_328S = 165;
    static constexpr std::int64_t XMT_END_360SP = 166;
    static constexpr std::int64_t XMT_END_3RD = 167;
    static constexpr std::int64_t XMT_END_5A = 168;
    static constexpr std::int64_t XMT_END_5B = 169;
    static constexpr std::int64_t XMT_END_5C = 170;
    static constexpr std::int64_t XMT_END_5D = 171;
    static constexpr std::int64_t XMT_END_5E = 172;
    static constexpr std::int64_t XMT_END_DAYTS = 173;
    static constexpr std::int64_t XMT_END_DINO = 174;
    static constexpr std::int64_t XMT_END_ENZO = 175;
    static constexpr std::int64_t XMT_END_F355S = 176;
    static constexpr std::int64_t XMT_END_F40 = 177;
    static constexpr std::int64_t XMT_END_F50 = 178;
    static constexpr std::int64_t XMT_END_GTO = 179;
    static constexpr std::int64_t XMT_END_TESTA = 180;
    static constexpr std::int64_t XMT_END_STAGE_5A0 = 181;
    static constexpr std::int64_t XMT_END_STAGE_5B0 = 182;
    static constexpr std::int64_t XMT_END_STAGE_5C0 = 183;
    static constexpr std::int64_t XMT_END_STAGE_5D0 = 184;
    static constexpr std::int64_t XMT_END_STAGE_5E0 = 185;
    static constexpr std::int64_t XMT_VSHADOW = 186;
    static constexpr std::int64_t XMT_PC_COLOR = 187;
    static constexpr std::int64_t XMT_DRIVER_RIVAL = 188;
    static constexpr std::int64_t XMT_AS_CAR = 189;
    static constexpr std::int64_t XMT_CHR_DR_M00 = 190;
    static constexpr std::int64_t XMT_CHR_DR_L00 = 191;
    static constexpr std::int64_t XMT_CHR_AUT01 = 192;
    static constexpr std::int64_t XMT_CHR_AUT02 = 193;
    static constexpr std::int64_t XMT_CHR_AUT03 = 194;
    static constexpr std::int64_t XMT_CHR_AUT04 = 195;
    static constexpr std::int64_t XMT_CHR_DR_G00 = 196;
    static constexpr std::int64_t XMT_CHR_DR_MH00 = 197;
    static constexpr std::int64_t XMT_CHR_DR_LH00 = 198;
    static constexpr std::int64_t XMT_CHR_DR_GH00 = 199;
    static constexpr std::int64_t XMT_DRIVER_GAL = 200;
    static constexpr std::int64_t XMT_DRIVER_GALUSA = 201;
    static constexpr std::int64_t XMT_CHR_DR_G00_USA = 202;
    static constexpr std::int64_t XMT_CHR_DR_GH00_USA = 203;
    static constexpr std::int64_t XMT_CHR_MAL = 204;
    static constexpr std::int64_t XMT_CHR_FAL = 205;
    static constexpr std::int64_t XMT_CHR_GAL = 206;
    static constexpr std::int64_t XMT_CHR_GAL_USA = 207;
    static constexpr std::int64_t XMT_END_PROPERTY = 208;
    static constexpr std::int64_t XMT_CS_GRAN = 209;
    static constexpr std::int64_t XMT_CS_SEQU = 210;
    static constexpr std::int64_t XMT_CS_SANF = 211;
    static constexpr std::int64_t XMT_CS_NEWY = 212;
    static constexpr std::int64_t XMT_CS_MACH = 213;
    static constexpr std::int64_t XMT_CS_YOSE = 214;
    static constexpr std::int64_t XMT_CS_MAYA = 215;
    static constexpr std::int64_t XMT_CS_NIAG = 216;
    static constexpr std::int64_t XMT_CS_ALAS = 217;
    static constexpr std::int64_t XMT_CS_AMAZ = 218;
    static constexpr std::int64_t XMT_CS_BEAC = 219;
    static constexpr std::int64_t XMT_CS_LASV = 220;
    static constexpr std::int64_t XMT_CS_PRIN = 221;
    static constexpr std::int64_t XMT_CS_FLOR = 222;
    static constexpr std::int64_t XMT_CS_EAST = 223;
    static constexpr std::int64_t XMT_BK_GRAN = 224;
    static constexpr std::int64_t XMT_BK_SEQU = 225;
    static constexpr std::int64_t XMT_BK_SANF = 226;
    static constexpr std::int64_t XMT_BK_NEWY = 227;
    static constexpr std::int64_t XMT_BK_MACH = 228;
    static constexpr std::int64_t XMT_BK_YOSE = 229;
    static constexpr std::int64_t XMT_BK_MAYA = 230;
    static constexpr std::int64_t XMT_BK_NIAG = 231;
    static constexpr std::int64_t XMT_BK_ALAS = 232;
    static constexpr std::int64_t XMT_BK_AMAZ = 233;
    static constexpr std::int64_t XMT_BK_BEAC = 234;
    static constexpr std::int64_t XMT_BK_LASV = 235;
    static constexpr std::int64_t XMT_BK_PRIN = 236;
    static constexpr std::int64_t XMT_BK_FLOR = 237;
    static constexpr std::int64_t XMT_BK_EAST = 238;
    static constexpr std::int64_t XMT_BK_ENDA = 239;
    static constexpr std::int64_t XMT_BK_ENDB = 240;
    static constexpr std::int64_t XMT_BK_ENDC = 241;
    static constexpr std::int64_t XMT_BK_ENDD = 242;
    static constexpr std::int64_t XMT_BK_ENDE = 243;
    static constexpr std::int64_t XMT_COURSE_OBJ_CS_GRAN = 244;
    static constexpr std::int64_t XMT_COURSE_OBJ_CS_SEQU = 245;
    static constexpr std::int64_t XMT_COURSE_OBJ_CS_SANF = 246;
    static constexpr std::int64_t XMT_COURSE_OBJ_CS_NEWY = 247;
    static constexpr std::int64_t XMT_COURSE_OBJ_CS_MACH = 248;
    static constexpr std::int64_t XMT_COURSE_OBJ_CS_YOSE = 249;
    static constexpr std::int64_t XMT_COURSE_OBJ_CS_MAYA = 250;
    static constexpr std::int64_t XMT_COURSE_OBJ_CS_NIAG = 251;
    static constexpr std::int64_t XMT_COURSE_OBJ_CS_ALAS = 252;
    static constexpr std::int64_t XMT_COURSE_OBJ_CS_AMAZ = 253;
    static constexpr std::int64_t XMT_COURSE_OBJ_CS_BEAC = 254;
    static constexpr std::int64_t XMT_COURSE_OBJ_CS_LASV = 255;
    static constexpr std::int64_t XMT_COURSE_OBJ_CS_PRIN = 256;
    static constexpr std::int64_t XMT_COURSE_OBJ_CS_FLOR = 257;
    static constexpr std::int64_t XMT_COURSE_OBJ_CS_EAST = 258;
    static constexpr std::int64_t XMT_COURSE_OBJ_BK_ALAS = 259;
    static constexpr std::int64_t XMT_COURSE_OBJ_BK_AMAZ = 260;
    static constexpr std::int64_t XMT_COURSE_OBJ_BK_BEAC = 261;
    static constexpr std::int64_t XMT_COURSE_OBJ_BK_LASV = 262;
    static constexpr std::int64_t XMT_COURSE_OBJ_BK_MACH = 263;
    static constexpr std::int64_t XMT_COURSE_OBJ_BK_NIAG = 264;
    static constexpr std::int64_t XMT_COURSE_OBJ_BK_SANF = 265;
    static constexpr std::int64_t XMT_COURSE_OBJ_BK_SEQU = 266;
    static constexpr std::int64_t XMT_COURSE_OBJ_BK_YOSE = 267;
    static constexpr std::int64_t XMT_COURSE_OBJ_BK_GRAN = 268;
    static constexpr std::int64_t XMT_COURSE_OBJ_SKY_GRAN = 269;
    static constexpr std::int64_t XMT_COURSE_OBJ_SKY_SEQU = 270;
    static constexpr std::int64_t XMT_COURSE_OBJ_SKY_SANF = 271;
    static constexpr std::int64_t XMT_COURSE_OBJ_SKY_NEWY = 272;
    static constexpr std::int64_t XMT_COURSE_OBJ_SKY_MACH = 273;
    static constexpr std::int64_t XMT_COURSE_OBJ_SKY_YOSE = 274;
    static constexpr std::int64_t XMT_COURSE_OBJ_SKY_MAYA = 275;
    static constexpr std::int64_t XMT_COURSE_OBJ_SKY_NIAG = 276;
    static constexpr std::int64_t XMT_COURSE_OBJ_SKY_ALAS = 277;
    static constexpr std::int64_t XMT_COURSE_OBJ_SKY_AMAZ = 278;
    static constexpr std::int64_t XMT_COURSE_OBJ_SKY_BEAC = 279;
    static constexpr std::int64_t XMT_COURSE_OBJ_SKY_LASV = 280;
    static constexpr std::int64_t XMT_COURSE_OBJ_SKY_PRIN = 281;
    static constexpr std::int64_t XMT_COURSE_OBJ_SKY_FLOR = 282;
    static constexpr std::int64_t XMT_COURSE_OBJ_SKY_EAST = 283;
    static constexpr std::int64_t XMT_ENV_GRAN = 284;
    static constexpr std::int64_t XMT_ENV_SEQU = 285;
    static constexpr std::int64_t XMT_ENV_SANF = 286;
    static constexpr std::int64_t XMT_ENV_NEWY = 287;
    static constexpr std::int64_t XMT_ENV_MACH = 288;
    static constexpr std::int64_t XMT_ENV_YOSE = 289;
    static constexpr std::int64_t XMT_ENV_MAYA = 290;
    static constexpr std::int64_t XMT_ENV_NIAG = 291;
    static constexpr std::int64_t XMT_ENV_ALAS = 292;
    static constexpr std::int64_t XMT_ENV_AMAZ = 293;
    static constexpr std::int64_t XMT_ENV_BEAC = 294;
    static constexpr std::int64_t XMT_ENV_LASV = 295;
    static constexpr std::int64_t XMT_ENV_PRIN = 296;
    static constexpr std::int64_t XMT_ENV_FLOR = 297;
    static constexpr std::int64_t XMT_ENV_EAST = 298;
    static constexpr std::int64_t XMT_CHR_AUT04_CVT = 299;
    static constexpr std::int64_t XMT_COURSE_OBJ_BK_PRIN = 300;
    static constexpr std::int64_t XMT_COURSE_OBJ_BK_MAYA = 301;
    static constexpr std::int64_t XMT_COURSE_OBJ_BK_EAST = 302;
    static constexpr std::int64_t XMT_COURSE_OBJ_BK_NEWY = 303;
    static constexpr std::int64_t XMT_COURSE_OBJ_BK_FLOR = 304;
    static constexpr std::int64_t XMT_COURSE_OBJ_BK_TULI = 305;
    static constexpr std::int64_t XMT_COURSE_OBJ_BK_METR = 306;
    static constexpr std::int64_t XMT_COURSE_OBJ_BK_RUIN = 307;
    static constexpr std::int64_t XMT_COURSE_OBJ_BK_IMPE = 308;
    static constexpr std::int64_t XMT_COURSE_OBJ_BK_CAPE = 309;
    static constexpr std::int64_t XMT_BO_BALL = 310;
    static constexpr std::int64_t XMT_BO_BIRD_GROUP = 311;
    static constexpr std::int64_t XMT_BO_BIRD_MINORITY = 312;
    static constexpr std::int64_t XMT_BO_FIGH = 313;
    static constexpr std::int64_t XMT_BO_HANG = 314;
    static constexpr std::int64_t XMT_BO_JETL = 315;
    static constexpr std::int64_t XMT_BO_PANZ = 316;
    static constexpr std::int64_t XMT_BO_PEGA = 317;
    static constexpr std::int64_t XMT_BO_PROP = 318;
    static constexpr std::int64_t XMT_BO_SHIP = 319;
    static constexpr std::int64_t XMT_BO_UFOO = 320;
    static constexpr std::int64_t XMT_BO_WAIB = 321;
    static constexpr std::int64_t XMT_BO_FRAM = 322;
    static constexpr std::int64_t XMT_BO_HAWK = 323;
    static constexpr std::int64_t XMT_RC_ALL = 324;
    static constexpr std::int64_t XMT_END_FLOR = 325;
    static constexpr std::int64_t XMT_END_PRIN = 326;
    static constexpr std::int64_t XMT_END_EAST = 327;
    static constexpr std::int64_t XMT_END_MAYA = 328;
    static constexpr std::int64_t XMT_END_NEWY = 329;
    static constexpr std::int64_t XMTSET_MAX = 330;
};

// xmtset_custom.h:7; DIE 0x5bba7
struct TAG_XMTHEAD {
    static constexpr std::size_t byte_size = 20;
    static constexpr std::size_t flag_offset = 0; // long unsigned int
    static constexpr std::size_t flag_size = 4;
    static constexpr std::size_t objnum_offset = 4; // long unsigned int
    static constexpr std::size_t objnum_size = 4;
    static constexpr std::size_t texnum_offset = 8; // long unsigned int
    static constexpr std::size_t texnum_size = 4;
    static constexpr std::size_t mdldata_offset = 12; // long unsigned int*
    static constexpr std::size_t mdldata_size = 4;
    static constexpr std::size_t texdata_offset = 16; // long unsigned int*
    static constexpr std::size_t texdata_size = 4;
};

// xmtset_custom.h:18; DIE 0x5bc6b
struct TAG_XMDLHEAD {
    static constexpr std::size_t byte_size = 16;
    static constexpr std::size_t flag_offset = 0; // long unsigned int
    static constexpr std::size_t flag_size = 4;
    static constexpr std::size_t mdlnum_offset = 4; // long unsigned int
    static constexpr std::size_t mdlnum_size = 4;
    static constexpr std::size_t mdldata_table_offset = 8; // long unsigned int**
    static constexpr std::size_t mdldata_table_size = 4;
    static constexpr std::size_t mdlname_table_offset = 12; // long unsigned int**
    static constexpr std::size_t mdlname_table_size = 4;
};

// xmtset_custom.h:26; DIE 0x5bd27
struct TAG_XTEXHEAD {
    static constexpr std::size_t byte_size = 20;
    static constexpr std::size_t flag_offset = 0; // long unsigned int
    static constexpr std::size_t flag_size = 4;
    static constexpr std::size_t texnum_offset = 4; // long unsigned int
    static constexpr std::size_t texnum_size = 4;
    static constexpr std::size_t texdata_offset_offset = 8; // long unsigned int*
    static constexpr std::size_t texdata_offset_size = 4;
    static constexpr std::size_t texofst_table_offset = 12; // long unsigned int*
    static constexpr std::size_t texofst_table_size = 4;
    static constexpr std::size_t texname_table_offset = 16; // char**
    static constexpr std::size_t texname_table_size = 4;
};

// xmtset_custom.h:34; DIE 0x5bdf1
struct TAG_OBJECT {
    static constexpr std::size_t byte_size = 40;
    static constexpr std::size_t pObjectData_offset = 0; // char*
    static constexpr std::size_t pObjectData_size = 4;
    static constexpr std::size_t pObjHeader_offset = 4; // ObjectHeader*
    static constexpr std::size_t pObjHeader_size = 4;
    static constexpr std::size_t pCullingNode_offset = 8; // CullingNode*
    static constexpr std::size_t pCullingNode_size = 4;
    static constexpr std::size_t pModelHeader_offset = 12; // ModelHeader*
    static constexpr std::size_t pModelHeader_size = 4;
    static constexpr std::size_t pVtxGroup_offset = 16; // VtxGroupInfo*
    static constexpr std::size_t pVtxGroup_size = 4;
    static constexpr std::size_t pMatGroup_offset = 20; // MatGroupInfo*
    static constexpr std::size_t pMatGroup_size = 4;
    static constexpr std::size_t pPrimitive_offset = 24; // PrimitiveList*
    static constexpr std::size_t pPrimitive_size = 4;
    static constexpr std::size_t pVtxFormat_offset = 28; // VtxFormatList*
    static constexpr std::size_t pVtxFormat_size = 4;
    static constexpr std::size_t pMaterials_offset = 32; // MaterialList*
    static constexpr std::size_t pMaterials_size = 4;
    static constexpr std::size_t pMatColors_offset = 36; // MaterialColor*
    static constexpr std::size_t pMatColors_size = 4;
};

// xmtset_custom.h:57; DIE 0x5befb
struct TAG_XMTSET {
    static constexpr std::size_t byte_size = 68;
    static constexpr std::size_t head_offset = 0; // XMTHEAD*
    static constexpr std::size_t head_size = 4;
    static constexpr std::size_t pSysMemData_offset = 4; // unsigned char*
    static constexpr std::size_t pSysMemData_size = 4;
    static constexpr std::size_t pVidMemData_offset = 8; // unsigned char*
    static constexpr std::size_t pVidMemData_size = 4;
    static constexpr std::size_t objnum_offset = 12; // long unsigned int
    static constexpr std::size_t objnum_size = 4;
    static constexpr std::size_t load_flg_offset = 16; // int
    static constexpr std::size_t load_flg_size = 4;
    static constexpr std::size_t progress_offset = 20; // long unsigned int
    static constexpr std::size_t progress_size = 4;
    static constexpr std::size_t load_size_offset = 24; // size_t
    static constexpr std::size_t load_size_size = 4;
    static constexpr std::size_t head_handle_offset = 28; // Handle
    static constexpr std::size_t head_handle_size = 4;
    static constexpr std::size_t pObjTbl_offset = 32; // OBJECT*
    static constexpr std::size_t pObjTbl_size = 4;
    static constexpr std::size_t pTextures_offset = 36; // LPDIRECT3DTEXTURE8*
    static constexpr std::size_t pTextures_size = 4;
    static constexpr std::size_t ObjectHandle_offset = 40; // Handle
    static constexpr std::size_t ObjectHandle_size = 4;
    static constexpr std::size_t TexturesHandle_offset = 44; // Handle
    static constexpr std::size_t TexturesHandle_size = 4;
    static constexpr std::size_t ObjDataHandle_offset = 48; // Handle
    static constexpr std::size_t ObjDataHandle_size = 4;
    static constexpr std::size_t TexDataHandle_offset = 52; // Handle
    static constexpr std::size_t TexDataHandle_size = 4;
    static constexpr std::size_t load_tex_mode_offset = 56; // long unsigned int
    static constexpr std::size_t load_tex_mode_size = 4;
    static constexpr std::size_t dwSysMemDataSize_offset = 60; // long unsigned int
    static constexpr std::size_t dwSysMemDataSize_size = 4;
    static constexpr std::size_t dwVidMemDataSize_offset = 64; // long unsigned int
    static constexpr std::size_t dwVidMemDataSize_size = 4;
};

// xstBrg.h:39; DIE 0x5c079
struct tag_DSPDATA {
    static constexpr std::size_t byte_size = 16;
    static constexpr std::size_t scr_idx_offset = 0; // long unsigned int
    static constexpr std::size_t scr_idx_size = 4;
    static constexpr std::size_t sx_offset = 4; // short unsigned int
    static constexpr std::size_t sx_size = 2;
    static constexpr std::size_t sy_offset = 6; // short unsigned int
    static constexpr std::size_t sy_size = 2;
    static constexpr std::size_t rot_offset = 8; // short unsigned int
    static constexpr std::size_t rot_size = 2;
    static constexpr std::size_t flip_offset = 10; // short unsigned int
    static constexpr std::size_t flip_size = 2;
    static constexpr std::size_t scale_offset = 12; // float
    static constexpr std::size_t scale_size = 4;
};

// xstBrg.h:47; DIE 0x5c149
struct tag_DSPTBL {
    static constexpr std::size_t byte_size = 8;
    static constexpr std::size_t nb_dspdata_offset = 0; // long unsigned int
    static constexpr std::size_t nb_dspdata_size = 4;
    static constexpr std::size_t dspdata_offset = 4; // DSPDATA*
    static constexpr std::size_t dspdata_size = 4;
};

// xstBrg.h:54; DIE 0x5c1e9
struct tag_SCRTBL {
    static constexpr std::size_t byte_size = 28;
    static constexpr std::size_t spr_idx_offset = 0; // long unsigned int
    static constexpr std::size_t spr_idx_size = 4;
    static constexpr std::size_t su_offset = 4; // float
    static constexpr std::size_t su_size = 4;
    static constexpr std::size_t sv_offset = 8; // float
    static constexpr std::size_t sv_size = 4;
    static constexpr std::size_t eu_offset = 12; // float
    static constexpr std::size_t eu_size = 4;
    static constexpr std::size_t ev_offset = 16; // float
    static constexpr std::size_t ev_size = 4;
    static constexpr std::size_t sx_offset = 20; // short unsigned int
    static constexpr std::size_t sx_size = 2;
    static constexpr std::size_t sy_offset = 22; // short unsigned int
    static constexpr std::size_t sy_size = 2;
    static constexpr std::size_t ex_offset = 24; // short unsigned int
    static constexpr std::size_t ex_size = 2;
    static constexpr std::size_t ey_offset = 26; // short unsigned int
    static constexpr std::size_t ey_size = 2;
};

// xstBrg.h:61; DIE 0x5c2dd
struct tag_SCRTBL2 {
    static constexpr std::size_t byte_size = 32;
    static constexpr std::size_t spr_idx_offset = 0; // long unsigned int
    static constexpr std::size_t spr_idx_size = 4;
    static constexpr std::size_t rotate_offset = 4; // long unsigned int
    static constexpr std::size_t rotate_size = 4;
    static constexpr std::size_t su_offset = 8; // float
    static constexpr std::size_t su_size = 4;
    static constexpr std::size_t sv_offset = 12; // float
    static constexpr std::size_t sv_size = 4;
    static constexpr std::size_t eu_offset = 16; // float
    static constexpr std::size_t eu_size = 4;
    static constexpr std::size_t ev_offset = 20; // float
    static constexpr std::size_t ev_size = 4;
    static constexpr std::size_t sx_offset = 24; // short unsigned int
    static constexpr std::size_t sx_size = 2;
    static constexpr std::size_t sy_offset = 26; // short unsigned int
    static constexpr std::size_t sy_size = 2;
    static constexpr std::size_t ex_offset = 28; // short unsigned int
    static constexpr std::size_t ex_size = 2;
    static constexpr std::size_t ey_offset = 30; // short unsigned int
    static constexpr std::size_t ey_size = 2;
};

// xstBrg.h:70; DIE 0x5c3df
struct tag_XSTHEAD {
    static constexpr std::size_t byte_size = 32;
    static constexpr std::size_t flag_offset = 0; // long unsigned int
    static constexpr std::size_t flag_size = 4;
    static constexpr std::size_t tex_ofs_offset = 4; // long unsigned int
    static constexpr std::size_t tex_ofs_size = 4;
    static constexpr std::size_t nb_tex_offset = 8; // long unsigned int
    static constexpr std::size_t nb_tex_size = 4;
    static constexpr std::size_t dummy_offset = 12; // void*
    static constexpr std::size_t dummy_size = 4;
    static constexpr std::size_t nb_dsptbl_offset = 16; // long unsigned int
    static constexpr std::size_t nb_dsptbl_size = 4;
    static constexpr std::size_t dsptbl_offset = 20; // DSPTBL*
    static constexpr std::size_t dsptbl_size = 4;
    static constexpr std::size_t nb_scrtbl_offset = 24; // long unsigned int
    static constexpr std::size_t nb_scrtbl_size = 4;
    static constexpr std::size_t scrtbl_offset = 28; // SCRTBL*
    static constexpr std::size_t scrtbl_size = 4;
};

// xstBrg.h:81; DIE 0x5c4d9
struct tagTEXINFO {
    static constexpr std::size_t byte_size = 16;
    static constexpr std::size_t w_offset = 0; // int
    static constexpr std::size_t w_size = 4;
    static constexpr std::size_t h_offset = 4; // int
    static constexpr std::size_t h_size = 4;
    static constexpr std::size_t d_offset = 8; // int
    static constexpr std::size_t d_size = 4;
    static constexpr std::size_t format_offset = 12; // int
    static constexpr std::size_t format_size = 4;
};

// xstBrg.h:88; DIE 0x5c589
struct tagXSTSET {
    static constexpr std::size_t byte_size = 28;
    static constexpr std::size_t flag_offset = 0; // long unsigned int
    static constexpr std::size_t flag_size = 4;
    static constexpr std::size_t head_offset = 4; // XSTHEAD*
    static constexpr std::size_t head_size = 4;
    static constexpr std::size_t pSysMemData_offset = 8; // unsigned char*
    static constexpr std::size_t pSysMemData_size = 4;
    static constexpr std::size_t pVidMemData_offset = 12; // unsigned char*
    static constexpr std::size_t pVidMemData_size = 4;
    static constexpr std::size_t progress_offset = 16; // long unsigned int
    static constexpr std::size_t progress_size = 4;
    static constexpr std::size_t pTextures_offset = 20; // LPDIRECT3DTEXTURE8*
    static constexpr std::size_t pTextures_size = 4;
    static constexpr std::size_t pInfo_offset = 24; // TEXINFO*
    static constexpr std::size_t pInfo_size = 4;
};

} // namespace outrun::carview_layout
