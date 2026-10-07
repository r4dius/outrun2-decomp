#pragma once
// RobMotion animation engine of the race robots (motion objects 82F5F0 +
// n*0xA4), ported from the PC EXE:
//   4F2280 SetMotion(id)          520750 key setup  522F70/522EE0 channels
//   4F1D30 SetFrame(f)            522BE0/522AE0 keys  522AA0 half float
//   4F1E60 SetLoop(n)             522C20 Hermite key evaluation, 519920 xyz
//   4F2320 SetMotionConnect(id, frame, length) (51B3F0/51B260 pose save,
//          519E60, 51A050, 519C30 angle wrap)
//   4F2410 connect step           4F2520 Calc (51B6D0 / 51B650 blend)
//   4F2260 UnsetBone              51B420 bone tree, 51A5F0 key TRS bone,
//          51A770 two-bone IK (519870 aim), 51A3A0, 519DD0 rotation blend
//          (519970 matrix lerp, 519CE0 matrix -> angles)
// Motion object: +04 motion id, +08 flags, +0C frame, +10 next frame, +14
// end, +18 speed, +1C last frame, +20/+24 connect ids, +28/+2C connect time,
// +30 connect frame, +34 speed, +38 connect length, +3C..+50, +54 bone
// entry, +58 bone set (0x44: +08 id, +10 count, +12 frames, +16 flags, +1C
// bone buffer, +20..+40 key cursors), +9C loop count, +A0 motion name.
// Bone record (0x350, buffer + i*0x350): +00 matrix, +40 file record, +44
// type, +45 IK kind, +46 unit rotation, +47 children, +48..+2CF nine key
// channels (0x48 each: +00/+10 keys {time, in, out, value}, +24 count,
// +26 left, +38/+3C cursors, +40 default), +2D0 IK target, +2DC/+2E8/+2F4
// T/R/S, +300/+30C/+318/+324 saved IK/T/R/S, +330 children, +338 index,
// +33C/+340 IK override.
// Globals 85B2B8..85B2EB (engine; only +30 flags, +04 factor and +1C..+24 are
// written by the EXE, the others stay 0) and 842114 (frame step, read).
#include "platform/race_area.hpp"
#include <cstdint>
#include <vector>
namespace outrun::platform {
struct PcRobMotionEngineState {
    static constexpr std::uint32_t Base=0x85b2b8u,End=0x85b2ecu;
    std::vector<std::uint8_t> block=std::vector<std::uint8_t>(End-Base);
    void map(PcRaceMemory& m){m.map(Base,block.data(),block.size());}
};
// Thiscall entries (ECX = motion object). The matrix stack is the context's.
std::uint32_t rob_motion_set_motion_4f2280(PcRaceContext&,std::uint32_t motion,std::uint32_t id);
void rob_motion_set_frame_4f1d30(PcRaceContext&,std::uint32_t motion,float frame);
std::uint32_t rob_motion_set_loop_4f1e60(PcRaceMemory&,std::uint32_t motion,std::uint32_t n);
std::uint32_t rob_motion_connect_4f2320(PcRaceContext&,std::uint32_t motion,std::uint32_t id,float frame,float length);
void rob_motion_calc_4f2520(PcRaceContext&,std::uint32_t motion);
// 519DD0 blends whose lerped matrix has a row of length <= 0.01: the PC then
// reads its uninitialised stack matrix (not modelled; the native rows are 0).
std::uint32_t rob_motion_degenerate_blends();
}
