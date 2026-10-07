#pragma once
// Native owner of the PC 2D sprite renderer (pc_sprite_2d.hpp) for the
// frontend and the race: the 2D queue state, the sprite banks and the two
// frame-renderer entries that use it:
//   display 428170   the SPRANI instance pool of event 398 (the runtime's
//                    FrontendSprites, event_function36.frontend_sprites);
//   leaf 42D710      the queue flush (0, 0x15) at the end of 449050.
// Sprite banks: when an instance of a bank is first drawn, its SPRANI
// animation (755860[bank-0x20] path) is relocated as 429AC0/429B00 do and
// its XST sprite file is loaded on the renderer's device. The XST is the
// English file the PC resource table 639CB8[bank] names, read from the
// retail tree: a resource-system boundary, not a transliteration of the
// asynchronous loader. A bank whose
// files are missing is reported and its instances are counted, not drawn.
#include "platform/pc_sprite_2d.hpp"
#include "platform/frontend_text.hpp"
#include "platform/frontend_list.hpp"
#include "platform/frontend_window.hpp"
#include <map>
#include <string>
namespace outrun::platform {
struct NativeRuntimeContext;
class PcSceneRenderer;
struct NativeSprite2d {
    PcSprite2dState state;
    std::map<std::uint32_t,std::string> bank_status;       // bank -> "loaded ..." or why not
    std::uint32_t displays{},flushes{},last_flush_draws{},frames_drawn{},inits_398{},controls_398{},last_init_mode{};
    std::string last_error;
    // The frontend UI's text glyphs (42C860 -> 42CFE0) and raw images (42D280 /
    // 42D300) of the current frame, queued at the event-405 display 49E4B0.
    std::vector<FrontendGlyph> glyphs;
    std::vector<FrontendListImage> images;
    std::vector<FrontendWindowIcon> icons;                   // 429530 immediate scene draws
    std::uint32_t ui_displays{},ui_glyphs{},ui_images{},ui_icons{};
};
NativeSprite2d& native_sprite2d(NativeRuntimeContext&);
// 755860[bank-0x20]: the SPRANI animation path of a pool bank (null outside 0x20..0x4A).
const char* native_sprite2d_ani_path(std::uint32_t bank);
// Loads a sprite bank (SPRANI animation / XST) on `device` if it is not yet.
void native_sprite2d_ensure_bank(NativeRuntimeContext&,PcD3D9Device& device,std::uint32_t bank);
// 639CB8[bank]: the English XST of a 2D bank, nullptr when the bank has none.
const char* native_sprite2d_xst_path(std::uint32_t bank);
// Event 398 SPRANI: init 427E30 (the pool allocations are cleared,
// race_hud.hpp sprani_init_427e30, whenever the event is set up) and, in
// GAME (mode 16), control 427F70 (the pool tick with the 427F70 clock);
// outside GAME the frontend owner's pool tick (event 405) runs the pool.
// False for other callbacks.
bool native_sprite2d_event(NativeRuntimeContext&,std::uint32_t callback);
// The frontend UI draw lists of this frame (empty when the UI is not shown).
// In-race text (GAD_PUB 42CCC0 glyphs): their 42CFE0 records into the 2D queue.
void native_sprite2d_glyph_records(NativeRuntimeContext&,PcD3D9Device&,const std::vector<FrontendGlyph>&);
void native_sprite2d_set_frontend(NativeRuntimeContext&,const std::vector<FrontendGlyph>&,const std::vector<FrontendListImage>&,
                                  const std::vector<FrontendWindowIcon>&);
// Frame renderer displays 428170 (pool) and 49E4B0 (frontend UI lists): false
// for other callbacks.
bool native_sprite2d_display(NativeRuntimeContext&,PcSceneRenderer&,std::uint32_t callback);
// Frame renderer leaf 42D710(0, arg): false for other leaves.
bool native_sprite2d_leaf(NativeRuntimeContext&,PcSceneRenderer&,std::uint32_t pc,std::uint32_t arg);
std::string native_sprite2d_status(const NativeRuntimeContext&);
}
