#pragma once
#include "platform/vehicle_visual.hpp"
#include "platform/mesh_preview_pack.hpp"
#include "platform/frontend_sprites.hpp"
#include "platform/frontend_text.hpp"
#include "platform/frontend_images.hpp"
#include "platform/frontend_window.hpp"
#include <array>
#include <cstdint>
#include <functional>
#include <string>
namespace outrun::platform { class PcD3D9Device; }

namespace outrun::xbox_runtime {
// The PC's 640x480 raster, presented centred 4:3 unless 16:9 is chosen
// (Options > Settings): the 3D scene full width, the 2D layer 4:3.
inline bool g_widescreen=false;


struct XboxRendererStats {
    std::uint64_t last_gpu_ns{};           // PC scene GPU time of the last measured frame (timestamps)
    std::uint32_t frames{};
    std::uint32_t unowned_frames{};
    std::uint32_t movie_frames{};
    std::uint32_t frontend_textures{};
    std::uint32_t frontend_scenes{};
    std::uint32_t frontend_draws{};
    std::uint32_t frontend_token{};
    std::uint32_t frontend_scene_width{};
    std::uint32_t frontend_scene_height{};
    std::uint32_t frontend_frames{};
    std::uint32_t frontend_scene_changes{};
    std::uint32_t frontend_animation_updates{};
    float frontend_animation_frame{};
    float frontend_animation_last_frame{};
    std::uint32_t frontend_animation_complete{};
    std::uint32_t frontend_overlay_frames{};
    std::uint32_t frontend_overlay_token{};
    std::uint32_t frontend_overlay_draws{};
    std::uint32_t frontend_instance_frames{},frontend_instances{},frontend_instance_draws{};
    std::uint32_t loading_scenes{};
    std::uint32_t loading_textures{};
    std::uint32_t loading_draws{};
    std::uint32_t loading_frames{};
    std::uint32_t loading_animation_updates{};
    float loading_animation_frame{};
    // Resident images and the frontend descriptor set.
    std::uint32_t resident_images{};
    std::uint32_t frontend_descriptors{};
    std::uint32_t shared_ui_scenes{},font_textures{},font_draws{},font_frames{},menu_image_draws{};
    std::uint32_t menu_icons{},menu_icon_draws{};
};

struct XboxRenderer { void* impl{}; };
// Options > Settings enhancement rows: the 3D resolution (back buffer pixels),
// 16:9 and the antialiasing (MSAA 1 / 2 / 4 samples, or FXAA), applied before the next frame.
void xbox_renderer_set_video(XboxRenderer&,unsigned width,unsigned height,bool widescreen,unsigned msaa,bool fxaa);
// The 2D layer starts (PC 42D710): the 3D scene's MSAA resolve or FXAA.
void xbox_renderer_scene_to_frame(XboxRenderer&);
// Borrowed pool must outlive the renderer. Binding copies authored timing into
// the native controller pool; rendering consumes its instances, not a menu index.
bool xbox_renderer_attach_frontend_sprites(XboxRenderer& renderer,platform::FrontendSprites& sprites);
// The instance pool is drawn by the ported PC 2D renderer (428170/42D710).
void xbox_renderer_set_pc_sprites(XboxRenderer&,bool on);
// The 2D layer starts (PC 42D710): a scaled / antialiased scene is finished here so the 2D draws at full resolution.
// The frontend glyphs / raw images are drawn by the PC 2D renderer (49E4B0 lists).
void xbox_renderer_set_pc_text(XboxRenderer&,bool on);

// The frontend pack, the START loading picture and the menu graphics/fonts of
// the game data (platform/retail_gpu_cache.hpp builds the START loading picture).
bool xbox_renderer_initialize(XboxRenderer& renderer,const char* frontend_path,std::string& error,
                         const std::vector<std::uint8_t>* start_loading,const char* shared_ui_path,
                         const char* font_path,const char* image_path);
// Copies this frame's real widget submissions. No synthetic labels or menu UI.
bool xbox_renderer_set_frontend_glyphs(XboxRenderer&,const std::vector<platform::FrontendGlyph>&);
bool xbox_renderer_set_frontend_images(XboxRenderer&,const std::vector<platform::FrontendListImage>&);
// Direct 429530 window icons: no allocation in the persistent PC sprite pool.
bool xbox_renderer_set_frontend_icons(XboxRenderer&,const std::vector<platform::FrontendWindowIcon>&);
bool xbox_renderer_set_frontend_game_backdrop(XboxRenderer&,bool);
const platform::FrontendFontPack* xbox_renderer_frontend_fonts(const XboxRenderer&);
bool xbox_renderer_set_frontend_visible(XboxRenderer& renderer,bool visible);
bool xbox_renderer_set_movie_frame(XboxRenderer& renderer,const std::uint8_t* rgba,
                                     unsigned width,unsigned height);
bool xbox_renderer_set_start_loading_visible(XboxRenderer& renderer,bool visible);
bool xbox_renderer_set_start_loading_scene(XboxRenderer& renderer,std::uint32_t scene);
bool xbox_renderer_set_frontend_token(XboxRenderer& renderer,std::uint32_t token);
bool xbox_renderer_set_frontend_overlay(XboxRenderer& renderer,std::uint32_t token,float frame);
// Ported PC renderer (frame renderer 449050 and below) on the CPU device: the scene
// callback runs each draw into the 4:3 PC screen rectangle (960x720 centred),
// after the frame clear and before the frontend/UI command lists. The device
// is nullptr when it could not be created (xbox_renderer_pc_error says why).
platform::PcD3D9Device* xbox_renderer_pc_device(XboxRenderer& renderer);
bool xbox_renderer_set_pc_scene(XboxRenderer& renderer,std::function<void(platform::PcD3D9Device&)> scene);
std::string xbox_renderer_pc_error(const XboxRenderer& renderer);
bool xbox_renderer_draw(XboxRenderer& renderer);
XboxRendererStats xbox_renderer_stats(const XboxRenderer& renderer);
void xbox_renderer_shutdown(XboxRenderer& renderer);

} // namespace outrun::xbox_runtime
