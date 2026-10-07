#pragma once
// Native runtime binding of the race end modes (race_end_modes.hpp):
//   mode 20 TIME OVER, 25 RESULT (event 0x188 function 0x15 route map /
//   0x50 Time Attack result), 27 GAME OVER, 28 TO MENU, 36 C2C unlock notice.
// NativeRaceEnd owns the PC blocks of PcRaceEndState. The race globals are
// mapped from their native owners for each call: 780258 = game_mode.
// game_variant, 78024C = start_mode.course_preset, 799D18/7804B0 = the
// player car, 7C23E0 = the active license, 6840EC/6840F0/6840F4 = the race
// manager's 4AEEF0 words, 7D6764 = its 453080 input lock, 8A8CDC = the
// frame's update index.
// Services (see the .cpp): the mode globals on mode_state/start_mode, the
// event system on event_state, the race manager accessors, the SPRANI pool
// (event_function36.frontend_sprites; bank 0x34 ROUTE_CVT timing read from
// the retail SPRANI file at 42DEB0/429920), music (401000/401030), the
// input switch record 7D6770 (4536F0 = pressed & mask), the event-36 object
// (4035F0). Course resource releases (4489C0 ... 4276B0) are counted: the
// native course owners are rebuilt by the next START init; the object event
// ranges of 4F11B0 are closed. A service without a native port throws, the
// callback is aborted and the module latches (fault PC + reason reported).
#include "platform/race_end_modes.hpp"
#include "platform/vehicle_model_draw.hpp"
#include "platform/race_hud_navi.hpp"
#include "platform/frontend_text.hpp"
#include "platform/native_runtime.hpp"
#include "driving/pc_common_control.hpp"
#include <array>
#include <vector>
#include <map>
#include <string>
namespace outrun::platform {
class PcSceneRenderer;
struct NativeRaceEnd {
    PcRaceEndState state{};
    std::map<std::uint32_t,std::uint32_t> routed,released;
    std::uint32_t mode_calls{},event_calls{},bank_loads{},sprite_matrices{},camera_calls{},start_camera_inits{};
    std::uint32_t records_written{};
    std::uint32_t rankings_saves{};
    std::int32_t mission_reward_last{-1};std::uint32_t mission_rewards_deferred{};   // 499890 / 499730 rewards given (mode 0x24)    // 4165F0: rankings.dat written (406C50 status 0)   // 447750 / 4478F0 / 447A60 / 447C10 calls into the common save
    // 4493C0 = [7D2698], stored by 4177CA from [740CA0], the language index
    // the protected launcher writes (0 English, the XST column read here).
    std::uint32_t language_7d2698{};
    std::vector<PcVehicleDrawCall>* scene_draws{};
    std::uint32_t scene_displays{},scene_display_faults{},scene_display_fault{},scene_display_skipped{};
    std::string scene_display_error;            // 3D draw leaves of an ending display (44B890 / 4522D0)
    std::vector<RaceHudDraw>* hud_draws{};                   // 429530 sink of a GAD_PUB display call
    std::vector<FrontendGlyph>* hud_glyphs{};                // 42CCC0 glyphs of a GAD_PUB display call
    // In-race text globals 956BA0.. (42CA60 font, 42CC60 scale, 42CCA0
    // colour, 42CCB0 mode, 42CC00 cursor; 42CCC0 = 42C720 + 42C5A0).
    struct Text { const FrontendFont* font{};FrontendTextStyle style{};FrontendTextCursor cursor{};std::int16_t base_y{}; } text;
    std::uint32_t frontend_manager_659944{};                 // 4EDCE0 on 659930 (+14)
    std::uint32_t attract_frames_83037c{};                   // 48B1F0 (mode 10 counter)
    // OUTRUN2SP arcade frontend words without another owner.
    std::uint32_t word_830370{},word_813744{};               // 48B200, 47EF20
    std::array<std::uint32_t,9> callbacks_7d2620{};          // 448FD0 handlers: 7D2620 + layer * 0xC (mask bit = layer)
    std::vector<std::uint8_t> picture_file;                  // 4239C0 \common\sel_dl_edit0.tgt
    std::uint32_t picture_target{},picture_w{},picture_h{},pictures_loaded{};   // 423CB0 (not rendered: counted)
    bool arcade_control{};                                   // an event-4 control is running
    std::map<std::uint32_t,std::uint32_t> image_entries_42c2f0;
    // 440A60 / 440B20 event works of the translated event functions: guest blocks
    // from 0x7E000000 (unused PC addresses), the slot's handle per event.
    static constexpr std::uint32_t EventWorkBase=0x7e000000u;
    std::map<std::uint32_t,std::vector<std::uint8_t>> event_works;
    std::map<std::uint32_t,std::uint32_t> bulk_event_faults;   // callback -> first fault (it stays off)
    std::map<std::uint32_t,std::uint32_t> bulk_event_calls;
    std::map<std::uint32_t,driving::PcEventWorkHandle> event_work_handles;
    std::uint32_t next_event_work{EventWorkBase};     // record address -> 42C2F0 token (resolved when drawn, the bank loaded)
    std::vector<RaceHudDraw> control_draws;                  // its 429530 draws, drawn by the next event-4 display
    static constexpr std::uint32_t MilesFileBase=0x5d000000u;
    std::vector<std::uint8_t> miles_file;                    // Scripts/bin/OutrunMiles.bin (4F12A0)
    driving::PcRelocCategoryBlobR077 miles_blob{};
    std::uint32_t miles_releases{};
    static constexpr std::uint32_t TextBase=0x5ef00000u;
    std::array<std::uint8_t,0x400> text_465eb0{};            // the last 465EB0 string (localized text)
    bool miles_loaded{};
    std::uint32_t fault{};std::string error;
    std::string bank_error;
    bool bank_pending_failed{};
};
NativeRaceEnd& native_race_end(NativeRuntimeContext&);
// The OutRun2SP button services 4165C0 (rankings.dat) / 4F3CC0 (tables); false for other PCs.
bool native_sp_rankings_service(NativeRuntimeContext&,unsigned pc);
// 496170 / 4E85E0 / 496180 / 4E8620 (native_runtime.cpp): the mission data reload at a C2C race end.
std::uint32_t native_mission_reload_call(NativeRuntimeContext&,std::uint32_t pc);
// 499730: the C2C reward index of the active license (-1: none or already given); commit sets it.
std::int32_t native_mission_reward_499730(NativeRuntimeContext&,bool commit);
// Mode callbacks of modes 20, 25, 27, 28; false for other tokens.
bool native_race_end_mode(NativeRuntimeContext&,std::uint32_t mode,std::uint32_t token,NativeModePhase phase);
// Event 0x188 callbacks 4AE750/4AE960/4AED20/4AED50/4AEEC0/4AEEE0; false otherwise.
bool native_race_end_event_invoke(NativeRuntimeContext&,std::uint32_t callback);
// An event callback no native owner takes, through the translated original (bulk_tr.cpp)
// over the race-end memory: false when it is not translated. A faulting callback is
// skipped afterwards (bulk_event_faults) instead of latching the race end.
bool native_bulk_event_invoke(NativeRuntimeContext&,std::uint32_t callback,std::uint32_t work,driving::PcMatrixStack&);
// 4985E4, the LAN results branch of the GAD control: 0 = take the 4EF410(0) tail, 1 = return, -1 = not run.
int native_race_end_lan_results_4985e4(NativeRuntimeContext&,std::uint32_t& done_83670c);
// GAD_PUB display 4998C0 (gad_display_4998c0): its 429530 draws are appended
// to `draws`; false with `missing` = the first unported display callee.
bool native_race_end_gad_display(NativeRuntimeContext&,std::vector<RaceHudDraw>& draws,std::vector<FrontendGlyph>& glyphs,std::uint32_t& missing);
// OUTRUN2SP arcade screens: the event-4 display callbacks (4BF3F0, 4C1FA0, 4C2420, 4C22C0,
// 4C1E80, 4C22E0, 4BEE80), name entry 4B4F20 and C2C unlock notice 498010;
// their draws and original-font captions are appended.
bool native_arcade_display_callback(std::uint32_t callback);
bool native_race_end_arcade_display(NativeRuntimeContext&,std::uint32_t callback,std::vector<RaceHudDraw>& draws,std::vector<FrontendGlyph>& glyphs,std::uint32_t& missing);
// Event 7 control 4874A0 (camera override 82E7D8 != 0): 486730 + 486EF0.
// The arcade ending's 3D displays 44B890 (event 0x186) / 4522D0 (event 0x187): their draw
// leaves on the renderer (native_race_draw_list_execute). False: not one of them.
bool native_race_end_scene_display(NativeRuntimeContext&,PcSceneRenderer&,std::uint32_t callback);
bool native_race_end_camera_4874a0(NativeRuntimeContext&,driving::PcMatrixStack&);
// The 448FD0 layer callback of `layer` (0 when none) and, for the ones this module owns
// (4BFA20; 49A650 is a RET), its run from the frame renderer 449050: true when handled.
std::uint32_t native_race_end_layer_callback(const NativeRuntimeContext&,std::uint32_t layer);
bool native_race_end_layer_invoke(NativeRuntimeContext&,std::uint32_t callback);
// [84A318] (4BFA20 / 4BFDB0): 2 while 449050 draws the car-select car 49F4D0 last.
std::uint32_t native_race_end_84a318(const NativeRuntimeContext&);
// START 4871A0(route 10..14): the camera override of the START sequence.
bool native_race_start_camera_4871a0(NativeRuntimeContext&,std::uint32_t scene);
// True for the race end modes whose SPRANI pool is ticked by event 398.
bool native_race_end_mode_active(std::uint32_t mode);
// The SPRANI pool timing of a bank (the 42DEB0 / 429920 resource result), from the
// retail ani file; banks already bound are rebound (same data).
bool native_sprani_bind_bank(NativeRuntimeContext&,std::uint32_t bank,std::string& error);
std::string native_race_end_status(const NativeRuntimeContext&);
// 4EF280 once before the first SUMO_FE (PC boot mode 0 stage 49E747).
void native_race_end_miles_init(NativeRuntimeContext&);
// 44DBB0(n) on the selected course descriptors (native_runtime.cpp).
bool native_course_world_id_44dbb0(const NativeRuntimeContext&,std::uint32_t n,std::uint32_t& value);
// 443EA0 on the event-36 object (native_runtime.cpp).
void native_event36_close_overlays_443ea0(NativeRuntimeContext&);
}
