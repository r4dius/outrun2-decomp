#pragma once
// OUTRUN2SP name entry (OR2006C2C.EXE mode 26 and event 0x189 function 0x16):
//   mode 26    49A960 init / 49AA60 control / 49AAE0 exit (resources 3D, 12, 14,
//              15, 36 and the course's 37..3B; event 0x189 function 0x16 and the
//              sound event 0x17F function 0x17)
//   fn 0x16    4B2370 init / 4B29E0 control / 4B4F20 display / 4B0950 destroy
// Line-by-line transliteration over PC addresses (PcRaceMemory): the caller maps
// the module's .bss 842288..842820 (PcRaceEndState::name_842288, with 842820..
// the arcade entry module's), the SP ranking tables 84DF40..850B00, the active
// license 7C23E0 (name 7C23E0, car 7C23FC), the race globals 780258 / 78024C /
// 799D18 / 836D08 / 836D0C and the EXE ranges (5C5200..5C6400 tables, the
// 68694C bad-word list and its strings 5C1B70.., the 5C2120 / 5B0334 / 626468
// formats). The PC on Windows has no letter input: the license name is
// committed on the first control frame (filtered, bad words -> "----").
// Every other PC function is one PcRaceService call:
//   450320 450560(4) 44DC50(id) 428320(token,layer,mode) 428460(token,layer,
//   mode,first,last) 428880(h) 428840(h) (float bits, unused by the PC) 4287B0(h,
//   &matrix) 4289B0(token,layer,&matrix,frame) 429530(token,x,y,layer,frame)
//   4B99D0(car) 48B140 451180(0) 48B180 48B1A0 45B830 45B840 47EE70(842728)
//   427700(id) 4493C0 4249F0(id) 4165F0 480D00/481180/47EF10(7C23E0) 4536F0(1)
//   text 42CCB0(mode) 42CA60(font) 42CCA0(colour) 42CC60(sx,sy) 42CC00(x,y)
//   42CCE0(format,arg) 42D5C0(token,x,y,scale,colour) 465EB0(id) 49A650(n)
//   mode 26 43F900(n) 42DEB0(id,mode) 429920(id,mode) 42DF90 4299A0 440110(id,
//   fn) 4401D0(id) 43FA90 43F8C0(m) 43F980 43F990(m) 427630 42DFB0(id)
//   4299C0(id)
// The 4B72F0 / 4B7630 common animation helpers are race_end_modes'. The SP
// record accessors 4F3010..4F3A80 and 4F3AD0 run natively (sp_rankings).
// Paths the PC never reaches (letter input: 8427E4 / 8426D4 / 8423B0 are only
// set by code nothing calls) raise PcRaceEndUnreachable.
#include "platform/race_end_modes.hpp"
#include <cstdint>
namespace outrun::platform {
// Scratch page for the PC stack buffers passed by address (strings, matrices,
// the 4F3010 record and rank).
constexpr std::uint32_t PcNameEntryLocals=0x7ffc0000u;
void mode26_init_49a960(PcRaceContext&);
void mode26_control_49aa60(PcRaceContext&);
void mode26_exit_49aae0(PcRaceContext&);
void name_entry_init_4b2370(PcRaceContext&);
void name_entry_control_4b29e0(PcRaceContext&);
void name_entry_display_4b4f20(PcRaceContext&);
void name_entry_destroy_4b0950(PcRaceContext&);
std::uint32_t name_entry_done_4b0960(PcRaceContext&);
// Helpers exposed for the oracle probe.
void name_entry_slide_4b0970(PcRaceContext&);
// 4B1DD0 (the arcade ending's 4524B0, variant 0): 1 when the run's record {score 4B99D0 | course
// 48B140 << 28 | stage 450320 << 24, time 451180(0), transmission / vehicle bits} ranks 0..4 in the
// 4F3440 query (variant 0, the preset, column popcount(stage)); 0 otherwise or outside variant 0.
// The record words 4F3440 does not compare in variant 0 are PC stack garbage (taken as 0 here).
std::uint32_t name_entry_record_4b1dd0(PcRaceContext&);
void name_entry_display_4b46f0(PcRaceContext&);
// Event 0x18B function 0x1E (mode 31): the letter-board name entry, 4B2F00 init / 4B42C0
// control / 4B4550 display. Services as above, and 453720(0) / 453750(0) (stick), 4536C0 /
// 4536F0(button) (held / pressed), 4BFB20 (A), 4035F0 (title owner), 424940(se), 4288C0(h,
// frame), 42C2F0(&record, token) / 42CFE0(&record, scale) (the letters), 4BC990 (countdown).
void name_entry2_init_4b2f00(PcRaceContext&);
void name_entry2_control_4b42c0(PcRaceContext&);
void name_entry2_display_4b4550(PcRaceContext&);
}
