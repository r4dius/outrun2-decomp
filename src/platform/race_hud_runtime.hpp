#pragma once
#include <array>
#include "platform/frontend_text.hpp"
// Native runtime owner of the GAME-mode race HUD (race_hud.hpp,
// race_hud_navi.hpp, mission_manager.hpp) and of the PC 2D sprite renderer
// it draws with (pc_sprite_2d.hpp):
//   event 398 SPRANI   427E30 init, 427F70 control, 428170 display (mode 16);
//                      the pool is the runtime's FrontendSprites
//                      (event_function36.frontend_sprites, the PC 7551xx pool).
//   event 389 GAD_PUB  4970F0 init, 498590 control, 4998C0 display, 497210 destroy.
//   event 388 NAVI_PUB 4BC9E0 init, 4BCC40 control, 4BEB00 display, 4B8DF0 destroy.
//   event 401 mission  4970E0 -> 496A30 (4965A0 then the race part), called
//                      from native_mission_event_invoke.
// The HUD displays record their draws (SpraniDraw / RaceHudDraw); this owner
// executes them in the recorded order through the ported 2D producers
// (429460 SPRANI renderer, 42D280 / 42D5F0 images) into the 2D queue, and
// the frame renderer's leaf 42D710(0, 0x15) flushes the queue on the
// renderer's device (pc_sprite_2d.cpp).
//
// Sprite banks (956D88 / 9568B8) are loaded from the retail tree when a
// draw first needs them and START requested them (the runtime's resource
// model): XST textures on the device, the SPRANI animation relocated as
// 429AC0 does, and the pool's bank timing (48BD20) bound for 428460.
//
// Sounds: 424940 requests (NAVI, the mission race part and the 45BD30 voice
// queue) are enqueued in the race sound queue 9563E8 the player car uses
// (pc_enqueue_sound, gated by the event-383 flags like the PC); 4249F0
// effects of the mission UI resources go to the frontend effect service.
// Unported callees are counted and reported, never replaced.
#include "platform/race_hud.hpp"
#include "platform/race_hud_navi.hpp"
#include "platform/pc_sprite_2d.hpp"
#include "platform/race_area.hpp"
#include "platform/mission_manager.hpp"
#include <map>
#include <string>
#include <vector>
namespace outrun::platform {
struct NativeRuntimeContext;
class PcSceneRenderer;
struct NativeRaceHudRuntime {
    std::array<bool,0x2b> bank_bind_tried{};std::string bank_bind_error;   // lazily bound SPRANI banks (car meters)
    std::uint32_t arcade_displays{};                       // OUTRUN2SP event-4 displays drawn
    std::array<std::uint8_t,0x330> recorder_810120{};  // 810120..81044F (zero: 480FE0 runs in variant 0 only)
    std::array<std::uint8_t,0x10> recorder_813738{};   // 813738..813747 (81373C / 813740 record pointers)
    // ---- event 398 SPRANI ----
    SpraniGlobals sprani{};
    std::vector<SpraniDraw> sprani_draws;
    std::array<float,3> sprani_blend{};
    // ---- event 389 GAD_PUB ----
    GadPubState gad{};
    float value_84bd00{};
    // ---- event 388 NAVI_PUB ----
    NaviPubGlobals navi;
    PcRaceMemory memory;
    std::vector<RaceHudDraw> navi_draws;
    std::vector<SpraniDraw> navi_scene_draws;
    const std::vector<FrontendGlyph>* navi_glyphs{};   // RaceHudGlyphRange source (race-end displays)
    std::array<float,3> navi_blend{};
    std::vector<std::uint8_t> event_records_799b30=std::vector<std::uint8_t>(410u*0x3cu);
    std::vector<std::uint8_t> event_flags_79fb48=std::vector<std::uint8_t>(410u);
    std::array<std::uint32_t,8> mirrors{};                  // read-only mode/route words (see rebuild)
    std::array<std::uint8_t,8> text_cursor_956bb4{};         // 42CC00 (written by 4BA9D0)
    std::array<std::uint8_t,0x14> race_clock_84210c{};       // copy of race.clock (read-only)
    bool navi_initialized{},navi_init_failed{};
    // ---- 496A30 race part ----
    NaviVoiceQueue voice{};
    bool mission_ui_constructed{};
    // (The 2D renderer, its banks, event 398 and the 42D710 flush belong to
    // sprite_2d_runtime: the HUD draws into its queue.)
    // ---- sounds ----
    std::vector<std::uint32_t> pending_sounds;                // scratch (per callback)
    std::map<std::uint32_t,std::uint32_t> sounds_424940;      // id -> requests
    std::uint32_t sounds_enqueued{},sounds_dropped{};
    // ---- report ----
    std::uint32_t frame{};
    std::map<std::uint32_t,std::uint32_t> missing;            // pc -> count (callbacks and 2D producers)
    std::vector<std::pair<std::uint32_t,std::uint32_t>> missing_order;  // (pc, first frame)
    std::map<std::uint32_t,std::uint32_t> frame_missing;      // pc -> count in the current frame
    std::uint32_t frame_missing_frame{};
    std::map<std::uint32_t,std::uint32_t> unmapped;           // NAVI fault address -> count
    std::map<std::uint32_t,std::uint32_t> draws_by_pc;        // recorded HUD draws executed (pc -> count)
    std::map<std::uint32_t,std::uint32_t> scene_tokens;       // SPRANI tokens rendered -> count
    std::uint32_t sprani_inits{},sprani_controls{},sprani_displays{};
    std::uint32_t gad_inits{},gad_controls{},gad_displays{},gad_destroys{};
    std::uint32_t navi_inits{},navi_controls{},navi_displays{},navi_destroys{};
    std::uint32_t mission_race_parts{};
    std::uint32_t flushes{},flush_draws{},last_flush_draws{},last_flush_sprite_draws{};
    std::string last_error;
};
NativeRaceHudRuntime& native_race_hud(NativeRuntimeContext&);
// Maps the HUD-owned Heart Attack words into another owner's memory (race_traffic).
void native_race_hud_map_heart(NativeRuntimeContext&,PcRaceMemory&);
// Event callbacks 427E30/427F70 (mode 16), 4970F0/498590/497210 and
// 4BC9E0/4BCC40/4B8DF0. False when the callback is not one of them.
bool native_race_hud_event_invoke(NativeRuntimeContext&,std::uint32_t callback,std::uint32_t work,std::uint32_t event_id,
                                  driving::PcMatrixStack& matrices,PcSceneRenderer* renderer);
// Displays 428170 (mode 16), 4998C0 and 4BEB00 from the frame renderer's
// display dispatch. False when the callback is not one of them.
bool native_race_hud_display(NativeRuntimeContext&,PcSceneRenderer&,std::uint32_t callback,std::uint32_t work,std::uint32_t event_id);
// Event 401 control 4970E0 -> 496A30 with the runtime's services. Returns
// the mission manager's result (false: a missing callee, reported).
// 45BD30(id, duration, priority) on the HUD voice queue 7F1998 (the race end and the other owners).
std::uint32_t native_race_hud_voice_45bd30(NativeRuntimeContext&,std::int32_t id,std::int16_t duration,std::int16_t priority);
bool native_race_hud_mission_control(NativeRuntimeContext&,MissionManagerServices&);
// 45C470 for the START scene owner (variant 2): quest object path of a stage.
bool native_race_hud_quest_object_45c470(NativeRuntimeContext&,std::uint32_t stage,std::uint32_t& path,std::string& why);
// navi_service (race_hud_navi.hpp) on the HUD runtime's memory: false with `why` when refused.
bool native_race_hud_service(NativeRuntimeContext&,std::uint32_t pc,const std::uint32_t* args,std::size_t count,
                             std::uint32_t& eax,std::string& why);
std::string native_race_hud_status(const NativeRuntimeContext&);
std::string native_race_hud_report(const NativeRuntimeContext&);
}
