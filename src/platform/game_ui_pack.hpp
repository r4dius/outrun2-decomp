#pragma once
#include "platform/mesh_preview_pack.hpp"
#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace outrun::platform {
struct GameUiTexture {
    std::uint32_t width{},height{};
    MeshPreviewTextureFormat format{};
    std::vector<std::uint8_t> bytes;
};
struct GameUiScene {
    std::uint32_t width{},height{},components{},footage{};
};
// Source SPRANI layers, resolved to atlas rectangles and native scene-space
// corners. This describes the authored pose; keyframed transforms are retained
// in GameUiPack::animation and must be evaluated before drawing a moving layer.
struct GameUiDraw {
    std::uint32_t texture{};
    std::array<std::uint32_t,4> crop{};
    std::array<std::array<float,2>,4> corners{};
    std::int16_t first_frame{},last_frame{};
    bool has_keyframes{};
    bool visible{true};
    float opacity{1.0f};
};
struct GameUiPack {
    std::vector<GameUiTexture> textures;
    std::vector<std::uint8_t> animation;
    std::uint32_t scene_count{};
};
bool parse_game_ui_pack(const std::uint8_t* data,std::size_t size,
                        GameUiPack& pack,std::string* error=nullptr);
bool load_game_ui_pack_file(const char* path,GameUiPack& pack,
                            std::string* error=nullptr);
// ETC/2C shares the container layout but not GAME's fixed scene/texture counts.
bool parse_shared_ui_pack(const std::uint8_t*,std::size_t,GameUiPack&,std::string* error=nullptr);
bool load_shared_ui_pack_file(const char*,GameUiPack&,std::string* error=nullptr);
bool game_ui_scene(const GameUiPack& pack,std::uint32_t index,
                   GameUiScene& scene);
// Traverse all authored component/footage layers in PC painter order. This
// validates references, crops, transforms and recursion, but deliberately does
// not invent a value for the original animation's keyframed channels.
bool game_ui_scene_base_draws(const GameUiPack& pack,std::uint32_t index,
                              std::vector<GameUiDraw>& draws,
                              std::string* error=nullptr);
// Evaluates the six authored SPRANI channels (x, y, scale x/y, rotation,
// opacity) at a scene frame. Exact authored key values and layer lifetimes
// are retained; intervals use the source knot tangents and their encoded
// fixed-point scale as cubic Hermite curves.
bool game_ui_scene_frame_draws(const GameUiPack& pack,std::uint32_t index,
                              float frame,std::vector<GameUiDraw>& draws,
                              std::string* error=nullptr);
}
