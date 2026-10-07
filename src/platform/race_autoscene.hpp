#pragma once
// AUTOSCENE: the scripted scenes of the arcade goal endings (OR2006C2C.EXE event 6,
// function 0x19 = 4B5150 init / 4B6CD0 control / 4B6A30 display / 4B6690 dest; the work
// [799CA0], 0x124 bytes), started by 4B5F60(scene) with scene ids 0x13..0x38: script rows
// 717D68 + scene * 12 {kind 1, path \AS\as_E_xx_bin.sz / as_SPE_xxx_bin.sz, extra entry}.
// The loaded script holds relocatable entries (4B6430) of kinds 0 camera track (4B5380),
// 1 node trees (4B53F0; 0x44-byte nodes, children at +3C / +40, ten 513650 channels at +38),
// 2 robot motion tracks (4B54E0 / 4B55D0), 5 fade track, 6 event lists (4B5630).
// Line-by-line transliteration over PC addresses (PcRaceMemory); every other PC function
// is one PcRaceService call:
//   events     440A60(size, heap) 440CD0(&block) 440B20(work) 440890 440CC0(dst, &src)
//              440D10(k) 440D30 440D50(k) 440D70 580253(size)
//   files      44FD80(path, 9) 44F880(handle) 44FC60(handle, &data, &size, &block)
//              44FD00 42EBF0 4489F0
//   scene      51B740(scene, k) (the protected per-scene callbacks: measured 0 for every
//              scene; a non-zero one raises PcAutosceneUnported), 483E10(&eye, &target,
//              &fov, &roll) 4AF520(fade) 42E020(a, b, c) 448CD0(name) 4249F0(sound)
//              4208A0(0x60, &pos, colour) 420560 49A650(x); 405360(token, 0, 0, 0, -1, 0)
//              is a draw leaf (PcVehicleDrawCall with the current matrix) of the context's list
//   robots     487C30(work, kind) 487D10(work, &matrix) 487D40(work, 0) 514880(work, &rot,
//              k) 514830 / 514800(work, token, frame) 514F60(work, motion) and the motion
//              slots 82F5F0 + i * 0xA4 (thiscall ECX): 48F4E0 4F2280(k) 4F1D30(f) 4F2520
//              4F1E30(f) 4ED890(f) 4F1E60(n)
// Matrix operations 409EF0 / 409F30 / 40A010 / 40A0D0 / 40A170 / 40A290 / 40A2D0 / 40A4E0
// run on the context's PcMatrixStack; 513650 is driving::pc_sample_crash_channel_x87.
#include "platform/race_area.hpp"
#include <cstdint>
namespace outrun::platform {
// A path that needs a service the port does not model (a per-scene callback).
struct PcAutosceneUnported { std::uint32_t pc; };
constexpr std::uint32_t PcAutosceneLocals=0x7ffb0000u;   // PC stack buffers passed by address
void autoscene_init_4b5150(PcRaceContext&);
void autoscene_control_4b6cd0(PcRaceContext&,std::uint32_t work);
void autoscene_display_4b6a30(PcRaceContext&,std::uint32_t work);
void autoscene_dest_4b6690(PcRaceContext&,std::uint32_t work);
void autoscene_start_4b5f60(PcRaceContext&,std::uint32_t scene);
std::uint32_t autoscene_frame_4b5fc0(PcRaceContext&);
// 4B5FD0 = (event 6 flags & 3) == 2 ? [[799CA0] + 1C] : 0 (VM head: al = [79FB4E]).
std::uint32_t autoscene_state_4b5fd0(PcRaceContext&);
}
