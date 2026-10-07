#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace outrun::platform {

struct MeshPreviewVertex {
    std::array<float,3> position{};
    std::array<float,3> normal{};
    std::array<float,2> uv{};
    std::array<float,4> color{};
    float transform_index{}; // Runtime-only vertex attribute; not part of the 48-byte disk record.
};
static_assert(sizeof(MeshPreviewVertex)==52,"r102 mesh vertex GPU stride");

struct MeshPreviewBatch {
    std::uint32_t first_index{};
    std::uint32_t index_count{};
    std::int32_t texture_index{-1};
    std::uint32_t flags{};
    std::uint32_t material{};
    std::uint32_t texture_attrib{};
    std::uint32_t transform_index{};
};

struct MeshPreviewTransform {
    // Column-major matrices, matching GLSL std140 mat4 storage.  Position maps
    // object-local PMT coordinates to clip space; normal maps local normals to
    // the preview view space without projection scaling.
    std::array<float,16> position{};
    std::array<float,16> normal{};
};

enum class MeshPreviewTextureFormat : std::uint32_t {
    bc1=1u,
    bc2=2u,
    bc3=3u,
};

struct MeshPreviewTexture {
    std::uint32_t source_index{};
    std::uint32_t width{};
    std::uint32_t height{};
    MeshPreviewTextureFormat format{MeshPreviewTextureFormat::bc1};
    std::uint32_t mip_levels{1u};
    std::vector<std::uint8_t> bytes;
};

struct MeshPreviewPack {
    std::uint32_t format_version{};
    std::vector<MeshPreviewVertex> vertices;
    std::vector<std::uint32_t> indices;
    std::vector<MeshPreviewBatch> batches;
    std::vector<MeshPreviewTexture> textures;
    std::vector<MeshPreviewTransform> transforms;
    std::array<float,3> source_min{};
    std::array<float,3> source_max{};
    std::array<float,3> packed_min{};
    std::array<float,3> packed_max{};
    std::array<std::uint8_t,32> source_sha256{};
};

constexpr std::uint32_t MeshPreviewPackVersion2=2u;
constexpr std::uint32_t MeshPreviewPackVersion3=3u;
constexpr std::size_t MeshPreviewPackHeaderSize=160u;
constexpr std::size_t MeshPreviewVertexStride=48u;
constexpr std::size_t MeshPreviewBatchStride=32u;
constexpr std::size_t MeshPreviewTextureStride=32u;
constexpr std::size_t MeshPreviewTransformStride=128u;

bool parse_mesh_preview_pack(const std::uint8_t* data,std::size_t size,
                             MeshPreviewPack& pack,std::string* error=nullptr);
bool load_mesh_preview_pack_file(const char* path,MeshPreviewPack& pack,
                                 std::string* error=nullptr);

} // namespace outrun::platform
