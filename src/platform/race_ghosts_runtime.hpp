#pragma once
// Native owner of the Time Attack ghost module (race_ghosts.cpp) and of the
// ghost save slot of the profile bank's save object (PC [8A8C7C]).
//   * PcGhostState: .bss 7F8D80..7F9420, table 7457C8 (416610 runs once at
//     creation, as 4162D0 does at boot), 8A8C74..8A8C80, 64BFED.
//   * Synthetic PC addresses: the save object (0x13C bytes) at
//     GhostSaveObjectBase; malloc/new blocks at GhostHeapBase + k *
//     GhostHeapWindow (never reused while mapped).
//   * Services: malloc/free/new/delete on those blocks; 4239C0/423F10/
//     423CB0/423BD0 on the retail tree ("\Ghosts\gc_default_TA_%02d.rec");
//     406DB0/406E50/406C50 on the save directory (GHOST%02d.DAT, a LE
//     length word + payload like the PC); 48B320 = [656234]. 466340
//     (downloaded ghost, network only) is refused and latched.
#include "platform/race_ghosts.hpp"
#include "platform/race_records.hpp"
#include "driving/pc_common_control.hpp"
#include <map>
#include <memory>
#include <string>
#include <vector>
namespace outrun::platform {
struct NativeRuntimeContext;
constexpr std::uint32_t GhostSaveObjectBase=0x5c000000u,GhostHeapBase=0x5c100000u,GhostHeapWindow=0x00040000u;
struct NativeGhostRuntime {
    PcGhostState state;
    PcRecordState records;                                  // variant 0 record module 47EC00..481180 (race_records)
    std::array<std::uint8_t,PcGhostState::SaveObjectBytes> save_object{};
    std::map<std::uint32_t,std::vector<std::uint8_t>> heap;
    std::uint32_t next_block=GhostHeapBase;
    std::vector<const std::vector<std::uint8_t>*> files;
    std::uint32_t fault{};                // first unported PC reached
    std::string error;
    std::uint32_t init_calls{},load_calls{},releases{},save_reads{},save_writes{},file_loads{},record_inits{},record_loads{};
    std::uint32_t save_prepares{},save_stores{};   // 467880 / 467960
    std::string save_directory;
    // Ghost car works: events 9..12, the race manager's car works 7815A0 +
    // k*0x10F0 (NativeRaceManager::car_works_7815a0, the single owner).
    static constexpr std::uint32_t CarBase=0x7815a0u;
    std::uint32_t car_inits{},car_controls{},car_destroys{},cars_opened{};
    std::array<std::uint8_t,12> alpha_842038{};           // 842038 ghost, 84203C alpha, 842040 translucent
    std::array<std::uint8_t,0x48> record_queue_841ff0{};   // 4AD1A0 queue: 841FF0 count, 841FF1 read, 841FF8 + i*0x10
    std::vector<PcVehicleDrawCall> draw_list;               // reused per display
    std::uint32_t displays{},shadows{},display_faults{},objects_drawn{};
    std::string display_error;
    NativeGhostRuntime();
    // Maps the module regions and every live heap block.
    void map(PcRaceMemory&);
};
NativeGhostRuntime& native_race_ghosts(NativeRuntimeContext&);
// Scene owner entries 467AC0 / 4686C0; false when a fault was latched.
bool native_ghost_init_467ac0(NativeRuntimeContext&);
bool native_ghost_load_4686c0(NativeRuntimeContext&);
// Scene owner entry 480FE0(a, b, c) (variant 0 records); false when a fault was latched.
bool native_records_init_480fe0(NativeRuntimeContext&,std::uint32_t a,std::uint32_t b,std::uint32_t c);
// Scene owner entry 481230(a, b, c) (variant 0: the QHOT records of [813740]).
bool native_records_load_481230(NativeRuntimeContext&,std::uint32_t a,std::uint32_t b,std::uint32_t c);
// 440880 = 4407E0(4): events 9..12 with the ghost car callbacks
// (4AD000/4ACE40/4AE5F0/4ADAC0/470560) and the 4ACFC0 queue.
bool native_ghost_open_440880(NativeRuntimeContext&);
// 4ACFB0 (START_Init 49DB20).
void native_ghost_queue_reset_4acfb0(NativeRuntimeContext&);
// 4AD190 (START_Init 49DC4A): the record car queue 841FF0 / 841FF1.
void native_record_queue_reset_4ad190(NativeRuntimeContext&);
// 440870 = 440750(4): events 9..12 with the record car callbacks (4AD1F0 / 4AD080 / 4AE5F0 /
// 4ADAC0 / 470560) and the 4AD1A0 queue.
bool native_records_open_440870(NativeRuntimeContext&);
// 480F80 (every update from the frame loop 417CF9): the variant-0 record scheduling.
void native_records_frame_480f80(NativeRuntimeContext&);
// 47F780 (CommonPlCar outside TA): the player's run recorded into the variant-0 records.
bool native_records_player_47f780(NativeRuntimeContext&,driving::PcMatrixStack&);
// 47EF30(preset) / 47EE70(out) for the race end (variant 0); false when a fault was latched.
// 47EE70's words are returned with the mask of those it wrote (the rest is PC stack garbage).
bool native_records_ranking_47ef30(NativeRuntimeContext&,std::uint32_t preset,std::uint32_t& eax);
bool native_records_stage_flags_47ee70(NativeRuntimeContext&,std::array<std::uint32_t,32>& flags,std::array<bool,32>& written);
// Event callbacks of the current event (9..12); false when not a ghost car callback.
bool native_ghost_car_invoke(NativeRuntimeContext&,std::uint32_t callback,driving::PcMatrixStack&);
// Display / shadow callbacks 4AE5F0 / 4ADAC0 of the ghost car events (work
// 7815A0 + k*0x10F0): the draw list is executed on the renderer through the
// race AREA executor. False when the callback is not a ghost car's.
class PcSceneRenderer;
bool native_ghost_car_display(NativeRuntimeContext&,PcSceneRenderer&,std::uint32_t callback,std::uint32_t work);
// 4671D0 (CommonPlCar): the player's ghost recording on the player car 7804B0.
bool native_ghost_record_4671d0(NativeRuntimeContext&,driving::PcMatrixStack&);
// Race manager hooks (event 359): 467190 goal, 467E00 restart.
bool native_ghost_goal_467190(NativeRuntimeContext&,driving::PcMatrixStack&);
// Race end (mode 19 exit 49CFD0): 467880 / 467960 ghost save, 47EF10 = 416830 close.
bool native_ghost_save_467880(NativeRuntimeContext&);
bool native_ghost_save_467960(NativeRuntimeContext&);
bool native_ghost_save_close_416830(NativeRuntimeContext&);
// Variant 0 name entry: 480D00 / 481180(name) record save (record module).
bool native_records_store_480d00(NativeRuntimeContext&);
bool native_records_store_481180(NativeRuntimeContext&,std::uint32_t name);
bool native_ghost_restart_467e00(NativeRuntimeContext&,driving::PcMatrixStack&);
// 465FA0 at the race teardown (race_end_runtime).
void native_ghost_release_465fa0(NativeRuntimeContext&);
// 4666A0 per update (417C7B loop); no-op until the ghost module exists.
void native_ghost_frame_4666a0(NativeRuntimeContext&);
// Whether an open event 9..31 is a ghost car (its work is driven here).
bool native_ghost_car_slot(const driving::PcEventSlot&);
// 465F40(i) for 49B870.
std::int8_t native_ghost_car_465f40(NativeRuntimeContext&,std::uint32_t i);
}
