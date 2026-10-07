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

namespace outrun::switch_runtime {
// Widescreen (Y in the game): the PC 3D scene fills the 16:9 framebuffer with
// a 16:9 camera (Hor+), the pretransformed 2D (HUD, sprites) stays in the
// centred 4:3 rectangle.
inline bool g_widescreen=false;
inline unsigned g_render_scale_percent=100;
// Runtime antialiasing (right stick click): 0 off, 1 MSAA 2x, 2 MSAA 4x,
// 3 FXAA (post-process over the PC frame). Seeded from options.ini at start.
inline unsigned g_aa_mode=0;
constexpr unsigned AaModeCount=4;
inline unsigned g_msaa_samples=0;            // options.ini antialiasing: 0, 2 or 4 (read before the renderer starts)   // 3D render resolution (options.ini), upscaled to the frame

struct SwitchRendererStats {
    std::uint64_t last_gpu_ns{};           // PC scene GPU time of the last measured frame (timestamps)
    std::uint32_t msaa_switches{},fxaa_frames{},fxaa_available{};
    std::uint32_t scaled_scene_frames{};   // frames whose 3D went through the render scale / antialiasing path
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
    // Resident images and the frontend descriptor set (at most 256 bound at once).
    std::uint32_t resident_images{};
    std::uint32_t frontend_descriptors{};
    std::uint32_t shared_ui_scenes{},font_textures{},font_draws{},font_frames{},menu_image_draws{};
    std::uint32_t menu_icons{},menu_icon_draws{};
};

struct SwitchRenderer { void* impl{}; };
// Borrowed pool must outlive the renderer. Binding copies authored timing into
// the native controller pool; rendering consumes its instances, not a menu index.
bool switch_renderer_attach_frontend_sprites(SwitchRenderer& renderer,platform::FrontendSprites& sprites);
// The instance pool is drawn by the ported PC 2D renderer (428170/42D710).
void switch_renderer_set_pc_sprites(SwitchRenderer&,bool on);
// The 2D layer starts (PC 42D710): a scaled / antialiased scene is finished here so the 2D draws at full resolution.
void switch_renderer_scene_to_frame(SwitchRenderer&);
// The frontend glyphs / raw images are drawn by the PC 2D renderer (49E4B0 lists).
void switch_renderer_set_pc_text(SwitchRenderer&,bool on);

// The frontend pack, the START loading picture and the menu graphics/fonts of
// the game data (platform/retail_gpu_cache.hpp builds the START loading picture).
bool switch_renderer_initialize(SwitchRenderer& renderer,const char* frontend_path,std::string& error,
                                const std::vector<std::uint8_t>* start_loading,const char* shared_ui_path,
                                const char* font_path,const char* image_path);
// Copies this frame's real widget submissions. No synthetic labels or menu UI.
bool switch_renderer_set_frontend_glyphs(SwitchRenderer&,const std::vector<platform::FrontendGlyph>&);
bool switch_renderer_set_frontend_images(SwitchRenderer&,const std::vector<platform::FrontendListImage>&);
// Direct 429530 window icons: no allocation in the persistent PC sprite pool.
bool switch_renderer_set_frontend_icons(SwitchRenderer&,const std::vector<platform::FrontendWindowIcon>&);
bool switch_renderer_set_frontend_game_backdrop(SwitchRenderer&,bool);
const platform::FrontendFontPack* switch_renderer_frontend_fonts(const SwitchRenderer&);
bool switch_renderer_set_frontend_visible(SwitchRenderer& renderer,bool visible);
bool switch_renderer_set_movie_frame(SwitchRenderer& renderer,const std::uint8_t* rgba,
                                     unsigned width,unsigned height);
bool switch_renderer_set_start_loading_visible(SwitchRenderer& renderer,bool visible);
bool switch_renderer_set_start_loading_scene(SwitchRenderer& renderer,std::uint32_t scene);
bool switch_renderer_set_frontend_token(SwitchRenderer& renderer,std::uint32_t token);
bool switch_renderer_set_frontend_overlay(SwitchRenderer& renderer,std::uint32_t token,float frame);
// Ported PC renderer (frame renderer 449050 and below) on deko3d: the scene
// callback runs each draw into the 4:3 PC screen rectangle (960x720 centred),
// after the frame clear and before the frontend/UI command lists. The device
// is nullptr when it could not be created (switch_renderer_pc_error says why).
platform::PcD3D9Device* switch_renderer_pc_device(SwitchRenderer& renderer);
bool switch_renderer_set_pc_scene(SwitchRenderer& renderer,std::function<void(platform::PcD3D9Device&)> scene);
std::string switch_renderer_pc_error(const SwitchRenderer& renderer);
// Console diagnostic: next uber pixel shader mode (Minus button).
void switch_renderer_cycle_pc_diag(SwitchRenderer& renderer);
// Development (autoplay "nospec=1"): the PC device uses only its uber shaders.
void switch_renderer_disable_pc_spec();
bool switch_renderer_draw(SwitchRenderer& renderer);
SwitchRendererStats switch_renderer_stats(const SwitchRenderer& renderer);
void switch_renderer_shutdown(SwitchRenderer& renderer);

} // namespace outrun::switch_runtime
