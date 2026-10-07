#pragma once
// The arcade goal ending (OR2006C2C.EXE mode 24 = 49A660 init / 49A710 control / 49A780 exit
// = 4527A0) and the events it opens besides the race ones:
//   event 0x187 function 0x45  451D10 init (three lights) / 4522D0 display (the ending's
//                              background model 452080 when [638E9C] > 4)
//   event 8 function 0x2D      49F410 init (the player car object for the ending) / 46C060
//                              display / 49F4E0 dest (46BB20)
// 452B10 resets the ending block, 4527F0 picks the ending ([638E98] 0..0x25 from the variant,
// the course 450380(8), the preset, 4524B0 / 45BF30(4); [638E9C] its course column, [7D3A7C]
// the motion set, [7D3A80] the car model) and requests its resources, 4524E0 runs the
// states [7D3A74] 0..6 (loads, events, AUTOSCENE 4B5F60(scene 0x13..0x38), lights 4523E0
// from 5A6AB8, the end through mode 0x19 / 0x1D).
// Line-by-line transliteration over PC addresses (PcRaceMemory: 7D3A74..7D3A8B, 638DF0..
// 638EA3, 836848, 8367A0 / 8367A4, 799D18 -> the car, 79F010 / 79F04C robots, 780258 /
// 78024C) with the EXE tables 5A4618..5A4880 and 5A6AB8; every other PC function is one
// PcRaceService call (4B5F60 / 4B5FC0 / 4B5FD0 included: the AUTOSCENE module is
// race_autoscene's). The background draw 452080 emits draw leaves (4044F0 / 4056D0 /
// 404540, the current matrix) to the context's list.
#include "platform/race_area.hpp"
#include <cstdint>
namespace outrun::platform {
void ending_reset_452b10(PcRaceContext&);
void mode24_init_49a660(PcRaceContext&);
void mode24_control_49a710(PcRaceContext&);
void mode24_exit_4527a0(PcRaceContext&);
void ending_choice_4527f0(PcRaceContext&);
void ending_states_4524e0(PcRaceContext&);
void ending_lights_4523e0(PcRaceContext&,std::uint32_t ending,std::uint32_t column);
std::uint32_t ending_record_4524b0(PcRaceContext&);
void ending_lights_init_451d10(PcRaceContext&);
void ending_background_4522d0(PcRaceContext&);
void ending_car_init_49f410(PcRaceContext&,std::uint32_t car);
void ending_car_dest_49f4e0(PcRaceContext&,std::uint32_t car);
// 452340: the ending stage model, 448CD0 of the name at 5A46A4 + ([7D3A88] + [638E9C] * 2) * 8
// (2 when absent).
std::uint32_t ending_model_452340(PcRaceContext&);
// 44B890 (event 0x186 function 0x44 display): 409F30, 4044E0(6), 405360(452340, 0, 0, 0, -1, 0),
// 4044E0(7), 40A010 as draw leaves.
void ending_area_display_44b890(PcRaceContext&);
}
