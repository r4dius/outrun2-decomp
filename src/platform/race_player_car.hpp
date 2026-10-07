#pragma once
#include "driving/pc_crash.hpp"
#include "driving/pc_crash_entry.hpp"
// Event 8 GamePlCar_Ctrl 0x4A8330 on the native runtime: the complete player
// car update (OperationInput, time-up braking, slipstream, shift warning,
// wanderer, DrivingControl 0x502C90, CommonPlCar 0x4A8100 with all its
// children, course progress, rank, NOS, SetOldParamBuffer). The verified
// modular parents (game_pl_car_ctrl_4a8330, common_pl_car_4a8100) own their
// inline logic; every child call is executed here on the runtime's shared
// car 799D18 / work 82E7F0 and on the PC globals below, with the original
// arguments of each call site.
// A child whose inputs have no native provider yet is still executed with the
// values the offline PC state implies when those are known, and otherwise is
// recorded in `missing` (never replaced by an invented result).
#include "driving/pc_matrix_stack.hpp"
#include "driving/pc_collision.hpp"
#include "driving/pc_common_control.hpp"
#include "platform/race_input.hpp"
#include <array>
#include <cstdint>
#include <map>
#include <string>
#include <vector>
namespace outrun::platform {
struct NativeRuntimeContext;
// PC globals of the race car tree that no other native owner holds.
struct RaceCarWorld {
    // Input records (453640/453860): player-0 switch words 7D6770 +0 held,
    // +4 previous, +8 pressed, +C released; analogue channels 7D6810+ch*0x10
    // (+4 current, +8 previous).
    PcInputSwitchRecord switch_7d6770{};
    PcInputAnalog analog_7d6810{};
    float steer_filter_7d6884{};
    std::uint32_t joystick_config_7d6880{};      // .bss 0: joystick configuration 5A7B50[0]
    std::int8_t steer_option_7c24cb{};           // options steering speed (.bss 0 until the options owner exists)
    // CommRace block 7DE418, slot 7DD138, clock 7F1938 (also the 455F50
    // sample counter), gate byte 8361B4 (495490). The route choices 7D39A0,
    // time 7D394C and flags 7D39F0 are the race manager's (race.manager).
    std::array<std::uint8_t,0x2000> commrace_7de418{};
    std::uint8_t slot_7dd138{};
    std::uint32_t clock_7f1938{};
    std::uint8_t gate_8361b4{};
    float slip_assist_800aac{};              // 46C470
    std::array<std::uint8_t,8*0x78> vibrate_841bd0{};
    std::array<std::uint8_t,0x3c> ghost_83db30{};
    std::uint32_t stage_base_841fa4{};
    driving::PcStageProgressHistory progress_680bd0{};
    std::uint32_t gate_80fb14{};
    std::uint32_t heart_mode_7f2428{};
    float nos_speed_7f8abc{};
    // Sound queue 9563E8 (32 entries), cursors 9560C0/956124 (the race sound
    // manager, event 383, flushes it); its gate byte 79FCC7 is event 383's flags.
    std::array<std::uint8_t,128> sound_entries_9563e8{};
    std::array<std::uint8_t,8> sound_state{};
    // Wall rotation globals 5E302C / 5E3030 (mutable .data).
    float rotation_5e302c{2608.0f},rotation_5e3030{-2608.0f};
    // Ghost record / route state (47F780 / 4671D0).
    driving::PcPlatformGhost47f780State ghost_record{};
    driving::PcPlatformGhost4671State ghost_route{};
    std::uint32_t frames{};
    std::map<std::uint32_t,std::uint32_t> missing;   // PC service -> count
    std::uint32_t last_error_pc{};
    std::string last_error;
};
// One GamePlCar_Ctrl frame on c's car/work. Returns false (and records the
// failing PC entry) when a child rejected its inputs.
bool race_player_car_control_4a8330(NativeRuntimeContext& c,driving::PcMatrixStack& matrices);
// EXE crash tables 5E08E8 / 5E0988 (4A2400 AdvanceCrashState), shared with the traffic cars.
const driving::PcCrashTables& race_crash_tables();
// 4A6EA0 (pc) / 4A2270 crash entry over another car's event (traffic cars).
bool race_car_crash_entry(NativeRuntimeContext&,driving::PcMatrixStack&,driving::Bytes event,std::uint32_t pc,std::uint32_t state,
    std::uint32_t reverse,bool tow,float rate,const driving::PcImpactFeedback& feedback,std::string& error);
// 5041B0 CbwColiWall over another car's event / wall work / parameters (traffic 504E70).
bool race_car_cbw_coli_wall_5041b0(NativeRuntimeContext&,driving::PcMatrixStack&,driving::Bytes event,driving::Bytes work,
    driving::Bytes parameters,driving::Bytes contacts,std::string& error);
// 475720 PasPlCar_Ctrl (goal/start camera scripts, 486942 selection).
bool race_player_car_pas_475720(NativeRuntimeContext&,driving::PcMatrixStack&);
bool race_player_car_arcade_475670(NativeRuntimeContext&,driving::PcMatrixStack&);   // event 8 function 0x2B control
// PC 0x453BB0 for player 0 with the joystick configuration (a Switch pad is a
// joystick): 453640 switch record then 453860 analogue channels.
// keyboard_table: the 5A7B30 configuration the PC layer's record selects (+1D0 = 0);
// false: 5A7B50[7D6880], the direct Switch mapping's layout.
void race_input_update_453bb0(NativeRuntimeContext& c,const PcInputDevice& device,bool keyboard_table=false);
}
