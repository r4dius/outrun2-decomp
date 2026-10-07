#pragma once
// Scripted race cameras of the CAMERA override event 7 (82E7C0 block):
//   4874A0 control: 486730 script + 486EF0 motion, then the 82E7E0 clock.
//   486730: the timed script 653788[scene] (0x2C-byte records, 31 opcodes;
//           sprites, music, effects, event suspends, the player-car control
//           switch 486942, the goal placement, the camera words).
//   486EF0: the camera motion 5E61D0[scene] (20 key channels, 513650):
//           car +2D8..+2EC, the body angles 82EA78.., the camera eye/target
//           +E0..+F4, +130 and the field of view +A4/+AC.
//   475720 PasPlCar_Ctrl (event 8 while 486942 selects it) and its 4755C0
//   (VM bridge at 4755C6 measured: lea edx,[eax+0x16C]).
// Line-by-line transliterations over PC addresses (PcRaceMemory), like
// race_end_modes: every function outside these is a service call. The
// matrix leaves run on PcRaceContext::matrices. The motion data (5E61A0..
// 5E62F0, 6BB3C8..712788) is the embedded EXE image; its channel-18 keys
// at 8572EC.. are .bss words no .text instruction writes (zero).
#include "platform/race_area.hpp"
#include <cstdint>
namespace outrun::platform {
constexpr std::uint32_t PcGoalLocals=0x7ffe0000u;   // 486730 stack frame (X = ESP after the prologue)
// 5E61A0..5E62F0 and 6BB3C8..712788 (const), 8572EC..8573BC (zero .bss).
void goal_camera_map_tables(PcRaceMemory&);
void goal_script_486730(PcRaceContext&);
std::uint32_t goal_motion_486ef0(PcRaceContext&);
void camera_override_control_4874a0(PcRaceContext&);
void pas_save_4755c0(PcRaceMemory&,std::uint32_t car);
// 475720(car). Wheel records: [82EA38 + i*4].
void pas_pl_car_475720(PcRaceContext&,std::uint32_t car);
}
