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

namespace outrun::ps5_runtime {
// The PC's 640x480 raster, presented centred 4:3 unless 16:9 is chosen.
// 16:9 (Options > Settings, OpenGL backend): the 3D scene full width, the 2D layer 4:3.
inline bool g_widescreen=false;


struct Ps5RendererStats {
    std::uint64_t last_gpu_ns{};           // PC scene GPU time of the last measured frame (timestamps)
    std::uint64_t last_record_ns{},last_present_ns{}; // CPU wall times; may include waits inside the driver
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

struct Ps5Renderer { void* impl{}; };
// Options > Settings enhancement rows (OpenGL backend): the 3D resolution (back
// buffer pixels), 16:9 and the antialiasing (MSAA 1 / 2 / 4 samples, or FXAA),
// applied before the next frame.
void ps5_renderer_set_video(Ps5Renderer&,unsigned width,unsigned height,bool widescreen,unsigned msaa,bool fxaa);
// The 2D layer starts (PC 42D710): the 3D scene's MSAA resolve or FXAA.
void ps5_renderer_scene_to_frame(Ps5Renderer&);
// Borrowed pool must outlive the renderer. Binding copies authored timing into
// the native controller pool; rendering consumes its instances, not a menu index.
bool ps5_renderer_attach_frontend_sprites(Ps5Renderer& renderer,platform::FrontendSprites& sprites);
// The instance pool is drawn by the ported PC 2D renderer (428170/42D710).
void ps5_renderer_set_pc_sprites(Ps5Renderer&,bool on);
// The 2D layer starts (PC 42D710): a scaled / antialiased scene is finished here so the 2D draws at full resolution.
// The frontend glyphs / raw images are drawn by the PC 2D renderer (49E4B0 lists).
void ps5_renderer_set_pc_text(Ps5Renderer&,bool on);

// The frontend pack, the START loading picture and the menu graphics/fonts of
// the game data (platform/retail_gpu_cache.hpp builds the START loading picture).
bool ps5_renderer_initialize(Ps5Renderer& renderer,const char* frontend_path,std::string& error,
                         const std::vector<std::uint8_t>* start_loading,const char* shared_ui_path,
                         const char* font_path,const char* image_path);
// Copies this frame's real widget submissions. No synthetic labels or menu UI.
bool ps5_renderer_set_frontend_glyphs(Ps5Renderer&,const std::vector<platform::FrontendGlyph>&);
bool ps5_renderer_set_frontend_images(Ps5Renderer&,const std::vector<platform::FrontendListImage>&);
// Direct 429530 window icons: no allocation in the persistent PC sprite pool.
bool ps5_renderer_set_frontend_icons(Ps5Renderer&,const std::vector<platform::FrontendWindowIcon>&);
bool ps5_renderer_set_frontend_game_backdrop(Ps5Renderer&,bool);
const platform::FrontendFontPack* ps5_renderer_frontend_fonts(const Ps5Renderer&);
bool ps5_renderer_set_frontend_visible(Ps5Renderer& renderer,bool visible);
bool ps5_renderer_set_movie_frame(Ps5Renderer& renderer,const std::uint8_t* rgba,
                                     unsigned width,unsigned height);
bool ps5_renderer_set_start_loading_visible(Ps5Renderer& renderer,bool visible);
bool ps5_renderer_set_start_loading_scene(Ps5Renderer& renderer,std::uint32_t scene);
bool ps5_renderer_set_frontend_token(Ps5Renderer& renderer,std::uint32_t token);
bool ps5_renderer_set_frontend_overlay(Ps5Renderer& renderer,std::uint32_t token,float frame);
// Ported PC renderer (frame renderer 449050 and below) on the CPU device: the scene
// callback runs each draw into the 4:3 PC screen rectangle (960x720 centred),
// after the frame clear and before the frontend/UI command lists. The device
// is nullptr when it could not be created (ps5_renderer_pc_error says why).
platform::PcD3D9Device* ps5_renderer_pc_device(Ps5Renderer& renderer);
bool ps5_renderer_set_pc_scene(Ps5Renderer& renderer,std::function<void(platform::PcD3D9Device&)> scene);
std::string ps5_renderer_pc_error(const Ps5Renderer& renderer);
bool ps5_renderer_draw(Ps5Renderer& renderer);
Ps5RendererStats ps5_renderer_stats(const Ps5Renderer& renderer);
void ps5_renderer_shutdown(Ps5Renderer& renderer);

} // namespace outrun::ps5_runtime
