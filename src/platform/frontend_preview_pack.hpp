#pragma once
#include "platform/mesh_preview_pack.hpp"
#include "platform/game_ui_pack.hpp"
#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace outrun::platform {

struct FrontendPreviewTexture {
    std::uint32_t source_index{};
    std::uint32_t width{};
    std::uint32_t height{};
    MeshPreviewTextureFormat format{MeshPreviewTextureFormat::bc1};
    std::vector<std::uint8_t> bytes;
};

struct FrontendPreviewDraw {
    std::uint32_t source_texture{};
    std::array<std::uint32_t,4> crop{};
    std::array<std::array<float,2>,4> corners{};
    std::uint32_t layer_order{};
    std::uint32_t frame_index{};
};

struct FrontendPreviewScene {
    std::uint32_t token{};
    std::uint32_t width{};
    std::uint32_t height{};
    std::uint32_t first_draw{};
    std::uint32_t draw_count{};
};

struct FrontendPreviewPack {
    std::uint32_t source_texture_count{};
    std::uint32_t source_scene_count{};
    std::uint32_t source_xst_compressed_bytes{};
    std::uint32_t source_xst_decompressed_bytes{};
    std::uint32_t source_animation_compressed_bytes{};
    std::uint32_t source_animation_decompressed_bytes{};
    std::array<std::uint8_t,32> source_xst_compressed_sha256{};
    std::array<std::uint8_t,32> source_xst_decompressed_sha256{};
    std::array<std::uint8_t,32> source_animation_compressed_sha256{};
    std::array<std::uint8_t,32> source_animation_decompressed_sha256{};
    std::vector<FrontendPreviewTexture> textures;
    std::vector<FrontendPreviewScene> scenes;
    std::vector<FrontendPreviewDraw> draws;
    // Raw PC SUMO_FE graph, indexed by the low 16 bits of scene tokens.
    // Present in OR2FEP5; legacy diagnostic/START packs remain static.
    std::vector<std::uint8_t> animation;
};

constexpr std::uint32_t FrontendPreviewPackVersion=3u;
constexpr std::size_t FrontendPreviewPackHeaderSize=224u;
constexpr std::size_t FrontendPreviewPackRecordSize=32u;
constexpr std::size_t FrontendPreviewSceneRecordSize=24u;
constexpr std::size_t FrontendPreviewDrawRecordSize=64u;
constexpr std::size_t FrontendPreviewPackExpectedTextures=10u;
constexpr std::size_t FrontendPreviewPackExpectedScenes=9u;
constexpr std::size_t FrontendPreviewPackExpectedDraws=26u;
constexpr std::uint32_t FrontendPreviewInitialToken=0x00440094u;
constexpr std::uint32_t FrontendPreviewSourceTextureCount=129u;
constexpr std::uint32_t FrontendPreviewSourceSceneCount=224u;
constexpr std::uint32_t FrontendPreviewSceneWidth=640u;
constexpr std::uint32_t FrontendPreviewSceneHeight=480u;
constexpr std::array<std::uint32_t,FrontendPreviewPackExpectedScenes>
    FrontendPreviewTokens{{0x00440094u,0x0044008eu,0x00440093u,
                           0x00440092u,0x0044008bu,0x00440096u,
                           0x00440095u,0x0044008fu,0x00440090u}};
constexpr std::array<std::uint32_t,FrontendPreviewPackExpectedTextures>
    FrontendPreviewSourceIndices{{94u,96u,88u,90u,87u,125u,95u,86u,91u,89u}};

const FrontendPreviewScene* find_frontend_preview_scene(
    const FrontendPreviewPack& pack,std::uint32_t token);
bool parse_frontend_preview_pack(const std::uint8_t* data,std::size_t size,
                                 FrontendPreviewPack& pack,
                                 std::string* error=nullptr);
bool load_frontend_preview_pack_file(const char* path,
                                     FrontendPreviewPack& pack,
                                     std::string* error=nullptr);
bool make_frontend_animation_view(const FrontendPreviewPack& source,
                                  GameUiPack& animation,
                                  std::string* error=nullptr);

} // namespace outrun::platform
