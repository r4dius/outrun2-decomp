#pragma once
// Race robot display works (ROBDISPWORK 8577D8 + id*0x60, three entries
// built by the static constructor 595430 -> 513750) and their set-up,
// ported from the PC EXE:
//   514BF0 rob_disp_init(work) -> 514910(disp, work)
//   513840 hand objects by kind (+58/+5C)   514680 mesh flag clear (406730)
// Work (robot work 7A01E0 + n*0x90): +00 display index, +08 kind.
// Display work: +00 flag, +04/+08 0, +0C face table word, +10/+14 face,
// +18/+1C eyes, +20..+4F four vectors, +50/+54 words, +58/+5C hands.
// Services (PcRaceCall.pc): 448CD0(name) object handle, 4066D0(handle, 2),
// 4103F0(handle, 1), 406730(handle, and, or).
#include "platform/race_area.hpp"
#include "platform/object_db.hpp"
#include <cstdint>
#include <vector>
namespace outrun::platform {
extern std::uint8_t RobDispRdata[0x850];               // EXE .rdata 5E62F0..5E6B40 (also osage spheres)
constexpr std::uint32_t RobDispRdataBase=0x5e62f0u;
extern std::uint8_t RobChrPaths[0x1c0];                // EXE .rdata 5C09C0..5C0B80 (character file names)
constexpr std::uint32_t RobChrPathsBase=0x5c09c0u;
extern std::uint8_t RobOsageData[0x2c8];               // EXE .data 7134E0..7137A8 (read-only in practice)
constexpr std::uint32_t RobOsageDataBase=0x7134e0u;
struct PcRobDispState {
    static constexpr std::uint32_t Base=0x8573d0u,End=0x857900u,Works=0x8577d8u,WorkSize=0x60u,WorkCount=3u;
    std::vector<std::uint8_t> block=std::vector<std::uint8_t>(End-Base);
    // 595430: 513750 on the three works over the object db of that time
    // (the static constructors run before any 448B90 build).
    void construct_595430(PcObjectDb& db_at_startup);
    void map(PcRaceMemory& m){m.map(Base,block.data(),block.size());}
};
void rob_disp_map_rdata(PcRaceMemory&);
// 514BF0 (cdecl, one argument: the robot work).
void rob_disp_init_514bf0(PcRaceContext&,std::uint32_t work);
// 513840(kind) on a display work (thiscall).
void rob_disp_hands_513840(PcRaceContext&,std::uint32_t disp,std::uint32_t kind);
// 488B80: the character files of the records 654860 (path, 0, resource, 0;
// 0x10 each, a null path ends): 4F1F90(path, &82F488[i]); on success
// 82F388[i] = the loaded data and 487E10 relocates it for the resource.
void rob_chr_load_488b80(PcRaceMemory&,const PcRaceService&);
// 487E10(eax = data, ecx = resource): offsets +08..+1C (and +1C's +08/+0C/
// +10) made absolute; resource << 16 into the handles of both lists.
void rob_chr_relocate_487e10(PcRaceMemory&,std::uint32_t data,std::uint32_t resource);
// 5148D0 set_hand_gu(work) -> 513A30 / 5148F0 set_hand_pa(work) -> 513BA0:
// hand objects +58/+5C of the display work by kind (5148D0 reaches 513A30
// through the bridge 1039BE8).
void rob_disp_hand_gu_5148d0(PcRaceContext&,std::uint32_t work);
void rob_disp_hand_pa_5148f0(PcRaceContext&,std::uint32_t work);
// 5147D0 rob_disp_ctrl(work): display work +08 counts down to 0.
void rob_disp_ctrl_5147d0(PcRaceMemory&,std::uint32_t work);
// 514E60 rob_disp_disp(work) -> 514C30: the robot's draw under the current
// matrix: 513D10 face (405450 morph, 406800 weight, eyes), 514280 body
// parts (405360 rigid, 405580 with a bone palette), 4044F0/405350/404540
// passes, 410670, the kind 3/4 ground disc, 513E40 hands, 514B60 prop.
// EXE .data 712E30 = 1, 712E34 = 1, 712E38 = 3 (never written). Draw
// leaves go to the service: 4052B0 4052C0 405350 404540 4044F0 410670
// 405360 405450 405580 406800 (and 448CD0 for 514B60).
void rob_disp_disp_514e60(PcRaceContext&,std::uint32_t work);
// 514680(list): 406730(handle, ~0x200, 0) on both handle arrays of a list.
void rob_disp_mesh_flags_514680(PcRaceContext&,std::uint32_t list);
}
