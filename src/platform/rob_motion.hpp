#pragma once
// RobMotion bone set-up of the race robots (driver / flagman motion objects
// 82F5F0 + n*0xA4), ported from the PC EXE:
//   4F1CF0 SetBone(this = motion, entry, bone file)  thiscall, ret 8
//   5206E0 bone-set release   520620 bone-set build   522DB0 extra words
//   522DD0 per-bone records (0x350 bytes each)
// The bone set lives at motion+0x58 (0x44 bytes): +08 id, +10 count (int16),
// +14 u16, +18 bone file records (0x38 each), +1C heap buffer of count
// records then the extra words. The heap buffer comes from 580253 malloc
// under the 440D10(0)/440D50(0) allocator selectors; 580BC2 frees it.
// Heap: the PC heap address is not a game property; the owner hands out
// guest addresses from its own range (RobotHeap), mapped for the robots.
#include "platform/race_area.hpp"
#include "driving/pc_common_control.hpp"
#include <cstdint>
#include <vector>
namespace outrun::platform {
struct RobotHeap {
    static constexpr std::uint32_t Base=0x72000000u,End=0x73000000u;
    struct Block { std::uint32_t base{}; std::vector<std::uint8_t> bytes; bool live{}; };
    std::vector<Block> blocks;        // reserved; a block is never moved while mapped
    std::uint32_t next{Base};
    std::uint32_t mallocs{},frees{};
    // 580253: 0 when the range is exhausted (the PC malloc's NULL).
    std::uint32_t malloc(std::uint32_t n);
    // 580BC2: the block stays mapped (its address is not reused), marked free.
    void free(std::uint32_t address);
    void map(PcRaceMemory&);
};
// 4F1CF0 SetBone. Returns false (and the PC address) on a guest fault.
void rob_set_bone_4f1cf0(PcRaceMemory&,RobotHeap&,driving::PcAllocatorStateStacks&,std::uint32_t motion,std::uint32_t entry,std::uint32_t bones);
// 4F2260 UnsetBone: 5206E0 release of the bone set, +54 = 0.
void rob_unset_bone_4f2260(PcRaceMemory&,RobotHeap&,driving::PcAllocatorStateStacks&,std::uint32_t motion);
}
