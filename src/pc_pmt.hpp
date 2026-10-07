#pragma once
#include "asset_views.hpp"
#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace outrun::assets {
// Explicit PC PMT disk layout. NEVER reinterpret these offsets as host pointers.
// Inputs are decompressed .pmt bytes; lifetime remains owned by the caller.
struct DdsInfo {
    std::uint32_t width=0,height=0,mips=0,faces=0,bits=0,fourcc=0;
    std::size_t file_bytes=0;
};
DdsInfo inspect_dds(ByteView data);
struct TextureView {
    std::uint32_t data_offset=0,format_word=0,size_word=0;
    std::size_t available_bytes=0;
    bool is_dds=false;
    DdsInfo dds{};
};
struct PmtFormat {
    std::uint32_t streams=0,index_offset=0,index_bytes=0,vertex_bytes=0,fvf=0,stride=0,shader=0;
    std::array<std::uint32_t,4> vertex_offsets{};
};
struct PmtPrimitive {std::uint32_t type=0,start=0,count=0,stored_polygon_count=0;};
struct PmtMaterialGroup {
    std::uint32_t base_vertex=0,material=0;
    std::vector<PmtPrimitive> primitives;
};
struct PmtVertexGroup {
    std::uint32_t format=0;
    std::array<std::uint32_t,2> first_material_group{},material_group_count{};
};
struct PmtMaterial {
    std::uint32_t attrib=0;
    std::array<float,4> diffuse{1,1,1,1};
    std::array<std::int32_t,4> texture_indices{-1,-1,-1,-1};
    std::array<std::uint32_t,4> texture_attribs{};
};
struct PmtObject {
    bool header_relative=false;
    std::uint32_t header_offset=0;
    std::vector<PmtFormat> formats;
    std::vector<PmtVertexGroup> vertex_groups;
    std::vector<PmtMaterialGroup> material_groups;
    std::vector<PmtMaterial> materials;
};
struct PmtView {
    ByteView system,video;
    std::vector<PmtObject> objects;
    std::vector<TextureView> textures;
    std::uint64_t checked_index_references=0;
};
PmtView inspect_pc_pmt(ByteView input);
struct PreviewVertex {std::array<float,3> position{},normal{};std::array<float,2> uv{};std::array<float,4> color{1,1,1,1};};
struct PreviewMesh {
    std::uint32_t object=0,group=0,material=0,alpha_class=0;
    std::vector<PreviewVertex> vertices;
    std::vector<std::uint32_t> indices;
    std::uint64_t degenerate_triangles=0;
};
// Requires an unmodified PmtView returned by inspect_pc_pmt, with live input bytes.
// Native local coordinates, no node transforms/LOD choice/animation/material
// shader reconstruction. Unsupported packed/skinned/tweening formats throw.
std::vector<PreviewMesh> decode_static_object(const PmtView& pmt,std::size_t object);
// Conversion preserves strip parity even while removing repeated-index triangles.
std::vector<std::uint32_t> triangulate(const std::vector<std::uint32_t>& indices,
                                     std::uint32_t type,std::uint64_t* removed=nullptr);
} // namespace outrun::assets
