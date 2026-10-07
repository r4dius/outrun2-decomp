#pragma once
// Event 388 NAVI_PUB (GAME HUD of a race): init 4BC9E0, control 4BCC40,
// display 4BEB00, destroy 4B8DF0, for the C2C mission route (780258 == 6)
// in mode 16. The Heart-Attack "quest" state it initialises (45E0F0, the
// 7F1938..7F8BB8 block, shared with the 45BD30 voice queue) is included.
//
// State and inputs are PC words reached through PcRaceMemory (race_area.hpp):
// the caller maps its native storage at the PC addresses listed in
// race_hud_port.md (the
// NaviPubGlobals blocks below plus the foreign words: car table 799B38..,
// the car objects the table points at, 79F574 camera, 780xxx mode words,
// 7D2xxx/7D3xxx course words, 80FBxx racers, 836374/83637C/836388 mission).
// Draw submissions are recorded (RaceHudDraw), never rendered here; the
// sprite pool calls go to FrontendSprites with the PC semantics.
#include "platform/frontend_sprites.hpp"
#include "platform/race_area.hpp"
#include "platform/race_hud.hpp"
#include "driving/pc_matrix_stack.hpp"
#include <array>
#include <cstdint>
#include <string>
#include <vector>
namespace outrun::platform {
// One recorded draw call with its raw 32-bit stack arguments (floats as
// bits), exactly as the original pushes them:
//   429530(token, x f32, y f32, layer, frame)            sprite scene at T(x,y)
//   429580(token, x f32, y f32, layer, frame, scale f32) sprite scene, scaled
//   4289B0(token, layer, matrix, frame)                  matrix copied below
//   42D280(token, x, y, flip mask, layer f32, colour)    raw image
//   42D5F0(token, x, y, scale f32, colour, flags)        raw image group
//   42CC00(x, y) text position (also written to 956BB4..956BBA)
// Race-end text: the glyphs [args[0], args[1]) of the display's glyph list,
// queued at this point of the draw sequence (42CDD0 / 42CCE0 / 42CCC0 call order).
constexpr std::uint32_t RaceHudGlyphRange=0xf0000001u;
struct RaceHudDraw {
    std::uint32_t pc{};
    std::array<std::uint32_t,7> args{};
    std::array<float,16> matrix{};
    std::array<std::uint8_t,0x48> record{};   // 42CFE0: the queued image record
};
// NAVI_PUB-owned PC globals: 842800..844A00, 688B00..689300 (EXE initial
// contents, including its static tables), 7F1900..7F2A00, 7F8980..7F8D80 and
// 7F9400..7F9600 (Heart-Attack quest state).
struct NaviPubGlobals {
    std::array<std::uint8_t,0x2200> g842800{};
    std::array<std::uint8_t,0x800> g688b00{};
    std::array<std::uint8_t,0x1100> g7f1900{};
    std::array<std::uint8_t,0x400> g7f8a00{};   // 7F8980..7F8D80 (Heart Attack: 7F89A8 race cars .. 7F8CFC)
    std::array<std::uint8_t,0x200> g7f9400{};
    NaviPubGlobals();                        // 688B00 block = EXE image
    void map(PcRaceMemory&);
};
struct NaviPubServices {
    PcRaceMemory* m{};                       // an unmapped access: missing 0xFA000000, fault = address
    const class FrontendTextTable* text{};   // 465EB0 (the LAN message queue 7EE070)
    FrontendSprites* sprites{};              // 428320/428460/4285A0/428800/428880
    std::uint32_t pause_95b214{};            // allocation pause domain
    driving::PcMatrixStack* matrices{};      // 89B564 stack (4BAD20 projection)
    std::vector<RaceHudDraw>* draws{};
    // Optional: 429530/429580/4289B0 resolved through 428A10 into SPRANI
    // submissions (final matrices), as the renderer consumes them.
    std::vector<SpraniDraw>* scene_draws{};
    SpraniGlobals* sprani{};                 // 986B28 / 7551B4
    std::array<float,3>* blend_9564e0{};
    std::vector<std::uint32_t>* sounds{};    // ordered 424940(id)
    // Course-system boundaries (plain callbacks, no per-call allocation):
    // 43EB60(mode, &point, 0, 0, &flags) ground query used by the 4BA0E0 line
    // of sight (writes point and the result flags word).
    void* user{};
    bool (*ground_43eb60)(void* user,std::uint32_t mode,driving::CourseProbe& point,std::uint32_t& flags){};
    // Heart Attack routines of race_traffic (464D20 control child, 4BEBB9 display pieces):
    // false when the routine did not run (not ported, or latched).
    void* heart_user{};
    bool (*heart)(void* user,std::uint32_t pc){};
    // LAN races (variants 3 / 4): 45A2B0 CommRace_GetRank in mode 16 (the translated
    // original); false when it did not run.
    void* lan_user{};
    bool (*lan_rank)(void* user,std::uint8_t id,std::uint32_t& rank){};
    std::uint32_t missing{},fault{};
};
bool navi_pub_init_4bc9e0(NaviPubServices&);
bool navi_pub_control_4bcc40(NaviPubServices&);
bool navi_pub_display_4beb00(NaviPubServices&);
bool navi_pub_destroy_4b8df0(NaviPubServices&);
// 45C470(stage): the HEART ATTACK quest object file of a stage (a path in EXE
// data, 0 when the quest has none); updates the 7F2548 level entry like 45E050.
bool navi_quest_object_45c470(NaviPubServices&,std::uint32_t stage,std::uint32_t& path);
// Value services of the navi module for the other runtimes: 45C470 quest object, 45C7E0
// quest kind, 45DFB0 / 45E000 the quest sound of a stage, 47FBD0 record sector time.
bool navi_service(NaviPubServices&,std::uint32_t pc,const std::uint32_t* args,std::size_t count,std::uint32_t& eax);
// 4295D0(token, &pos, mode, frame, scale, z bias, angle, depth mode, minimum): a SPRANI scene
// at the camera projection (449940) of a world point, scaled by distance, depth tested
// (428AF0); z from [95D8A0] * [780030] when mode < 0. Writes [780068] = z bias.
bool navi_sprite3d_4295d0(NaviPubServices&,std::uint32_t token,const std::array<float,3>& pos,std::int32_t mode,
                          std::int32_t frame,float scale,float zbias,float angle,std::uint32_t depth,float minimum);
// 428AC0(token, &matrix, frame, s, angle, mode): 428AF0 with that matrix (course object hoops).
bool navi_sprite_428ac0(NaviPubServices&,std::uint32_t token,const std::array<float,16>& matrix,std::int32_t frame,
                        float s,float angle,std::uint32_t mode);
// One recorded 429530 / 429580 / 4289B0 submission executed as this module's own draws.
void navi_emit_draw(NaviPubServices&,const RaceHudDraw&);
// Pieces exposed for the oracle.
bool navi_digits_4ba9d0(NaviPubServices&,std::uint32_t style,std::int32_t x,std::int32_t y,
                        const std::string& text,std::uint32_t layer,float alpha);
bool navi_rival_labels_4bad20(NaviPubServices&,std::uint32_t car_pc);
// 4B04D0: the variant 9 rank markers (4BDAA0 -> 4BBC70, slots 5..7) of the first cars at ranks 0..2.
bool navi_rank_markers_4b04d0(NaviPubServices&);
// 4BEA50 body (the GOAL display caller checks 450670 >= 0x78): 4BE020 banners, 4BE150 sector list.
bool navi_sector_banners_4bea50(NaviPubServices&);
}
