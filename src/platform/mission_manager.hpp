#pragma once
// Event function 0x20 (event 0x191, opened by START for variants 4 and 6):
// the mission/race manager. init 495B90, control 496A30 (its START-time part
// is 4965A0, the race record and course selection), display 49A650 (RET),
// destroy 4964D0. Globals 836350..8363A8 and 67E6AC..67E6B8.
#include "platform/race_asset_pack.hpp"
#include "platform/frontend_ui_resources.hpp"
#include "platform/race_hud.hpp"
#include "platform/racer_setup.hpp"
#include "driving/pc_matrix_stack.hpp"
#include <array>
#include <cstdint>
#include <functional>
#include <vector>
namespace outrun::platform {
struct MissionManagerState {
    std::uint32_t loader_836350{},buffer_836354{},status_836358{};
    std::uint32_t v836378{},v836380{},stage_836384{},timer_836388{},v83638c{};
    std::uint32_t countdown_836390{},v836394{},v83639c{},v8363a0{},v8363a8{};
    std::uint8_t v836398{},v8363a4{};
    std::uint8_t flag_67e6ac{};
    std::uint32_t v67e6b0{},v67e6b4{};
    std::int32_t v67e6b8{-1};
    std::uint32_t v688b3c{};                 // 4B8DE0
    std::int32_t record_83637c{-1};          // selected Races record index (PC pointer)
    std::uint32_t network_record_83637c{};   // 495A10 of a LAN race: the 84A9B0 settings block (0: none)
    std::uint8_t selection_836374{};         // 4964D0 sets it
    RaceCourseSelection selection{};         // record +0x18 course list (4F1210 count)
    // Static UI resources (0xA0 each, constructed by 465160 at program start,
    // reset by 465250 at exit): 8363B0 grade, 836450 tag, 8364F0 marker,
    // 836590 banner. Their sprites live in the shared FrontendSprites pool.
    std::array<std::uint8_t,0xa0> ui_8363b0{},ui_836450{},ui_8364f0{},ui_836590{};
};
struct MissionManagerServices {
    const RaceAssetPack* races{};
    const PcLicense* profile{};              // 7C23E0
    std::uint32_t race_key_67e6a4{},race_sub_67e6a8{};
    std::uint32_t variant_780258{};
    // 44D720("Race_Courses", "", 836350, 0, 0, 1, record+0x14 == 4, record+0x18, count, 0).
    std::function<bool(RaceCourseSelection&,bool kind4)> apply_44d720;
    // 47CF40(record+0x1C, 0): traffic set-up of the selected record.
    std::function<bool(std::uint32_t race_index)> traffic_47cf40;
    // LAN race (variant 4): the session settings record [83637C] (network memory 84A9B0):
    // +0 course index, +14 course type, +20 race kind, +28 time (seconds); 456D60 players.
    struct LanRecord {bool present{};std::uint32_t index{},type{},kind{},players{};float seconds{};} lan;
    // 44D720(data, category, 836350, 0, 0, 1, 0, 0, 0, 0) of a named course script; 43F950 course preset.
    std::function<bool(const char* data,const char* category)> apply_named_44d720;
    std::function<void(std::uint32_t)> preset_43f950;
    std::uint32_t missing{};
};
// 44B820(laps): course length. laps > 0: (primary+secondary +0x7E of the
// selected copy 7D30A8, 16-bit sum) + 2, times laps. Otherwise the sum of
// every primary (+1) and secondary (+1) length walking from 7D30A8 through the
// +0x24 (or, when +0x2C is -1, +0x28) child links into the records at 7D33BC
// until a record with +0x2C and +0x30 both -1. Descriptors are resolved by
// their PC tokens (+0x14 primary, +0x18 secondary).
bool course_length_44b820(const std::uint8_t* selected_7d30a8,const std::uint8_t* records_7d33bc,std::size_t record_bytes,
                          const driving::PcCourseDescriptorPackR078& descriptors,std::int32_t laps,std::uint32_t& length);
// 495B90 (and 4B8DE0(0)).
void mission_manager_init_495b90(MissionManagerState&);
// 4964D0.
void mission_manager_destroy_4964d0(MissionManagerState&);
// 4961F0 for variants other than 4: record +0x28 seconds * 60 (cvttss2si).
bool mission_manager_time_4961f0(const MissionManagerState&,const MissionManagerServices&,std::int32_t&);
// 4965A0, variants other than 4. result: the original AL. False from the
// function itself means a missing service (s.missing).
bool mission_manager_select_4965a0(MissionManagerState&,MissionManagerServices&,bool& result);

// 465160 on the four static UI resources (program start).
void mission_manager_construct_ui_465160(MissionManagerState&,FrontendUiResources&);
// Inputs and boundaries of the GAME (mode 16) part of 496A30.
struct MissionRaceServices {
    FrontendUiResources* ui{};               // 465xxx on the UI resources (and the sprite pool)
    driving::Bytes car{nullptr,0};           // [799D18] player car object (>= 0xDC0 bytes)
    float car_spec_134c{};                   // [[car+0x2B4]+0x134C] (4960A0)
    driving::Bytes camera{nullptr,0};        // [79F574] camera object (>= 0x358 bytes)
    driving::PcMatrixStack* matrices{};      // 89B564 stack (495D90 projection)
    std::array<float,3> model_offset_5e0ac8{}; // 5E0AC8[(int8)[655B59]] (495D90)
    RacerSetupState* racers{};               // 80FB00/80FB04/80FB1C/80FB2C/64E190/64E194
    PcLicense* profile{};                    // 7C23E0 (495A20 writes)
    NaviVoiceQueue* voice{};                 // 45BD30/45BCD0
    NaviVoiceServices voice_services{};      // race clock, 46C530, 424940 record
    std::uint32_t mode_78026c{};
    std::uint32_t race_end_7d38f0{};         // 450240 / 450230 (in/out)
    std::int16_t pause_8367bc{};             // 49B2D0 (protected getter: word [8367BC])
    std::vector<std::uint32_t>* sounds_424940{}; // caller-owned: ordered 424940(id) requests (also the voice's)
    // 4763D0 (esi = racer +0x4C car index): releases a traffic car and its racer
    // link (80FB00 +0x4C/+0x50/+0x20, car objects 799B38, 440200). Boundary.
    void* user{};
    bool (*release_car_4763d0)(void* user,std::uint32_t car_index){};
    std::uint32_t missing{};
};
// 496A30: 4965A0, then (when it returned 1) the four 4659F0 ticks and, in mode
// 16 while 7D38F0 == 0 and word [8367BC] <= 0, the countdown banners, the
// mission-type scoring switch (types 1..6), the rival timers (variant != 4),
// 477B60, 495C20, 495E90 and the 45BCD0 voice pump. Variant 4 is not ported.
bool mission_manager_control_496a30(MissionManagerState&,MissionManagerServices&,MissionRaceServices&);
// Pieces (exposed for the oracle): 495C20 rank, 495E90 marker, 495D90 projection.
bool mission_rank_495c20(const MissionManagerState&,const MissionManagerServices&,MissionRaceServices&);
bool mission_marker_495e90(MissionManagerState&,const MissionManagerServices&,MissionRaceServices&);
bool mission_project_495d90(MissionRaceServices&,bool flag,float& x,float& y);
}
