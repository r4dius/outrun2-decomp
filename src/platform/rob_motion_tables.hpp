#pragma once
// PC RobMotion system tables (Lindbergh RobMotion "motion data table" and
// "bone table"), .bss 84D970..84DE74:
//   84D970  motion table   = [[84DE6C]] (Common\motdata_table.bin, relocated)
//   84D974  bone table     = [[84DE68]] (Common\bone.bin, raw)
//   84D978  63 x 16-byte motion-group slots {0,2,0,4} (4F2020/4F2060 state)
//   84DD68  63 motion-group data pointers (filled by 4F2060 / 44FC60)
//   84DE64  scheduler pending flag (4F2020/4F2130)
//   84DE68/84DE6C  4F1F90 file headers {buffer, 0} (at buffer + size)
//   84DE70  motion-table entry count (4F1F30)
// The only writer of 84D970/84D974 is 4F2470 (RobMotion system init), called
// once by the shared loader 49E580 stage 0 (the native owner runs it from
// the same call: native_runtime.cpp native_shared_loader_call 0x4F2470).
//
// Line-by-line port over PC addresses (race_area.hpp PcRaceMemory /
// PcRaceCall conventions). Ported: 4F2470, 4F1F90 (file loader), 4F1F30
// (motion-table relocation), 440D90 (empty: push ebp/mov ebp,esp/pop ebp/
// ret, no effect) and the CRT strrchr 581090. Services (PcRaceCall):
//   4239C0 open(name, "rb") -> handle or 0 (protected entry, jmp 103C973)
//   423F10 size(handle)      423CB0 read(buf,size,1,handle)
//   423BD0 close(handle)     580253 malloc(n) (heap 85FC0C)
//   440D10/440D30 (7B11A8) and 440D50/440D70 (7B1194) allocator selector
//   stacks push(0)/pop.
#include "platform/race_area.hpp"
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>
namespace outrun::platform {
struct PcRobMotionTablesState {
    static constexpr std::uint32_t Base=0x84d970u,End=0x84de74u;
    std::vector<std::uint8_t> block=std::vector<std::uint8_t>(End-Base);   // .bss: zero
    void map(PcRaceMemory& m){m.map(Base,block.data(),block.size());}
};
// EXE .rdata strings the init passes to its services (5DA304 "\Common\bone.bin",
// 5DA2E8 "\Common\motdata_table.bin", 62563C "rb").
struct PcRobMotionRdata { std::uint32_t base; const std::uint8_t* data; std::size_t size; };
extern const PcRobMotionRdata RobMotionRdata[3];
void rob_motion_map_rdata(PcRaceMemory&);
constexpr std::uint32_t PcRobMotionServices[]={
    0x4239c0u,0x423f10u,0x440d10u,0x440d50u,0x580253u,0x440d30u,0x440d70u,0x423cb0u,0x423bd0u};
void rob_motion_init_4f2470(PcRaceMemory&,const PcRaceService&);
std::uint32_t rob_motion_load_file_4f1f90(PcRaceMemory&,const PcRaceService&,std::uint32_t name,std::uint32_t out);
void rob_motion_relocate_4f1f30(PcRaceMemory&,std::uint32_t table);
std::uint32_t pc_crt_strrchr_581090(const PcRaceMemory&,std::uint32_t s,std::uint8_t c);

// Native owner of the RobMotion system tables: the .bss block and the two
// 580253 heap buffers 4F1F90 allocates. The PC heap address is not a
// property of the game (it varies per run); the owner's buffers get guest
// addresses from a bump range at HeapBase (the relocated table pointers are
// consistent with them). A re-run of 4F2470 allocates new buffers and keeps
// the old ones alive, as the PC leaks them. Filled once by
// native_rob_motion_tables_init (race_robots_runtime.hpp), never per frame.
struct NativeRobMotionTables {
    static constexpr std::uint32_t HeapBase=0x70000000u,HeapEnd=0x71000000u;
    struct Heap { std::uint32_t base; std::vector<std::uint8_t> bytes; };
    PcRobMotionTablesState state;
    std::vector<Heap> heaps;                 // reserved once; never reallocated while mapped
    std::uint32_t heap_next{HeapBase};
    std::uint32_t init_calls{},loads{},fault{}; // fault: first missing service PC / unmapped address
    std::uint32_t groups_loaded{};           // 4F2060 motion groups (\Anims\mot_*_bin.sz)
    bool ready{};                            // 4F2470 completed
    std::string error;
    // Maps what the robots read: 84D970/84D974 and the heap buffers. The rest
    // of the block (84D978 slots, 84DD68 group pointers, 84DE64) is written
    // by the 4F2020/4F2060/4F21C0 motion scheduler, which has no native
    // owner, so it is not mapped (a read is reported, not answered).
    void map_for_robots(PcRaceMemory& m){
        if(!ready)return;
        m.map(PcRobMotionTablesState::Base,state.block.data(),state.block.size());   // tables, group data 84DD68 (4F2280)
        for(auto& h:heaps)m.map(h.base,h.bytes.data(),h.bytes.size());
    }
};
}
