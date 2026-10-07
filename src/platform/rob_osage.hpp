#pragma once
// Race robot "osage" chains (hair / cloth strands of the driver and
// passenger), ported from the PC EXE:
//   514F60 rob_osage_init(work, motion)   514E80 osage reset (16 chains)
//   5229A0 chain build   522810 chain placement   522620 chain step
//   522450 chain matrices   5223B0 link droop   522560 sphere collision
//   401790 link length   5222B0 damping   522280 chain reset   522270 pool
// State (module .bss): 858710 + id*0x800 matrix pools (0x40 each),
// 85A710 init mask, 85A7B8 + (id*16+i)*0x20 chains, 85DE50..85DE63 globals
// (85DE58 / 85DE5C are never written by the EXE: 0). A chain: +00 record
// (character file osage entry, 0x24), +04 parameter list, +08 node count,
// +0C nodes (0x2C each: +00 length, +04 position, +10 velocity, +1C, +20
// pool matrix, +24 damping, +28 collision list).
// The work is the robot work: +00 display index, +08 kind (82F388 entry).
#include "platform/race_area.hpp"
#include "platform/rob_motion.hpp"
#include <cstdint>
#include <vector>
namespace outrun::platform {
struct PcRobOsageState {
    // 857900 sphere storage (0x70 each, 515040), 858710 pools, 85A710 words, chains.
    static constexpr std::uint32_t SphereBase=0x857900u,PoolBase=0x858710u,ChainsEnd=0x85adb8u;
    static constexpr std::uint32_t GlobalsBase=0x85de50u,GlobalsEnd=0x85de64u;
    static constexpr std::uint32_t Chains=0x85a7b8u,ChainSize=0x20u,NodeSize=0x2cu;
    std::vector<std::uint8_t> block=std::vector<std::uint8_t>(ChainsEnd-SphereBase);
    std::vector<std::uint8_t> globals=std::vector<std::uint8_t>(GlobalsEnd-GlobalsBase);
    void map(PcRaceMemory& m){m.map(SphereBase,block.data(),block.size());m.map(GlobalsBase,globals.data(),globals.size());}
};
// EXE constants read by the chains (never written): 62806C 1.0, 73771C
// gravity 0.0027222, 7134EC damping 0.002, 722D40 warm-up steps 20.
// 514F60 (cdecl: work, motion).
void rob_osage_init_514f60(PcRaceContext&,RobotHeap&,std::uint32_t work,std::uint32_t motion);
// 514E80(work): 522280 on the 16 chains of the display index.
void rob_osage_reset_514e80(PcRaceMemory&,RobotHeap&,std::uint32_t work);
// 515040 rob_osage_ctrl(work, motion): per chain the collision spheres of
// the kind (514EC0 from 5E6740/5E6900/5E6AE0, or 7147A8 when 85A714 names
// this robot), the per-node damping (5222F0) and sphere lists (522360), then
// the bone matrix, 522810 when 85A710 flags a new chain, gravity 85A720
// (522340) and one step. Needs rob_disp_map_rdata (EXE tables).
void rob_osage_ctrl_515040(PcRaceContext&,std::uint32_t work,std::uint32_t motion);
// 401790(a, b, length): a = b + (a - b) * (length / |a - b|) when longer
// (also used by the flag cloth 401720).
void pc_limit_401790(PcRaceMemory&,std::uint32_t a,std::uint32_t b,float length);
// 522620 (thiscall chain): one simulation step.
void rob_osage_step_522620(PcRaceContext&,std::uint32_t chain);
}
