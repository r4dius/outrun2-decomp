#pragma once
// Native runtime owner of the race AREA (event 390: 44CB00 init, 44F7C0
// control, 44F120 display, 44B7F0 destroy) and SKY (event 391: 451A30 init,
// 451B40 control, 4521C0 display) modules of race_area.cpp / race_sky.cpp.
//
// The modules address every global by its PC address through one
// PcRaceMemory. This owner builds that memory before each callback:
//   * EXE read-only data: every embedded EXE range (course descriptors
//     5CEE80..5DA040, 5D4DC8 tables, path strings, 633558 path table ...).
//   * Area block 7D2D80..7D34C8 and .data 635F2C..636BC4 (PcRaceAreaState),
//     sky words 7D3A64 / 638664..638674 (PcRaceSkyState).
//   * Overlays (later mappings shadow the block) onto the native owners of
//     the same PC words, so every reader stays consistent:
//       635F2C/635F30 (44C940 cache), 7D2DA0/7D3130/7D3190 matrices,
//       7D2DE0/7D30A8/7D33D8 records, 7D3124/7D3178, 7D33B0, 7D33C0, 7D33C4,
//       7F95A8 -> game_mode.course_runtime (44DA00/44D720 owner);
//       7D2E80/7D2E88 -> race.area_state_7d2e80/88 (GAME control 44B9C0..);
//       7F94C0 -> start_mode.manager_state_7f94c0 (55A930 with ECX=7F9460).
//   * Read-only mirrors rebuilt before every callback: 78024C (course
//     preset), 780258 (race mode), 78026C (game mode), the 410 event records
//     799B30 (+08 = work pointer: 799D18 car, 79ECC8.. stage objects, 79F6DC
//     sky work) and the flag bytes 79FB48 (79FCA4.. stage objects, 79FCCE =
//     area), and the inverse view matrix 95DBA0 (renderer globals).
//   * Event works at their static PC work addresses (descriptor column):
//     the car (event 8, 7804B0, car_select.car_799d18) and the sky work
//     (event 391, 79FCF0, owned here).
//   * Synthetic PC addresses chosen here (documented in the report):
//       RaceAreaTableBase      course record table [7D33BC] points into;
//       RaceAreaAllocationBase geometry allocations of the 44FD80 mode-7
//                              loader (one 64 MB window each).
// Every PC function outside the two modules goes through service(): routed
// to the native port when one exists, otherwise counted in `missing` and
// latched. A missing function whose EAX the modules use aborts the callback
// (PcRaceMissingService); one whose EAX is never used (void services) is
// counted and the callback continues (its side effect is reported lost).
// 450xxx race-manager functions (7D39xx words) go to `race_manager` (hook
// for the event-359 owner); unanswered they are missing.
#include "platform/race_area.hpp"
#include "platform/race_sky.hpp"
#include "platform/pc_car_reflection.hpp"
#include "driving/pc_car_services.hpp"
#include "driving/pc_common_control.hpp"
#include <functional>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>
namespace outrun::platform {
struct NativeRuntimeContext;
class PcSceneRenderer;
constexpr std::uint32_t RaceAreaTableBase=0x41000000u;
constexpr std::uint32_t RaceAreaAllocationBase=0x42000000u,RaceAreaAllocationWindow=0x04000000u;
constexpr std::uint32_t RaceAreaSlotBase=0x7d34d0u,RaceAreaSlotCount=8u; // PC async slots 7D34D0..7D3650 (0x30 each)
constexpr std::uint32_t StageObjectBase=0x28000000u,StageObjectWindow=0x01000000u;  // course-prog: stage-object lanes (below NativeRacesBlobBase 30000000, which the traffic memory maps too)
struct PcRaceMissingService : std::runtime_error {
    std::uint32_t pc;
    PcRaceMissingService(std::uint32_t p,const std::string& why):std::runtime_error(why),pc(p){}
};
// PC async slot (7D34D0 + k*0x30) fields used by 44FC60: +00 in use,
// +04 state, +14 allocation handle, +24 destination, +28 request size.
struct PcRaceAsyncSlot { std::uint32_t word0{},state{},handle_14{},dest_24{},size_28{}; };
struct NativeRaceAreaRuntime {
    std::vector<std::int32_t> lod_stack_899560;          // 4044A0/4044C0 saved 8999B8 values
    PcRaceAreaState area;
    PcRaceSkyState sky;
    std::array<std::uint8_t,0x120> sky_work{};               // event 391 work (79FCF0)
    std::vector<std::uint8_t> event_records_799b30=std::vector<std::uint8_t>(410u*0x3cu);
    std::vector<std::uint8_t> event_flags_79fb48=std::vector<std::uint8_t>(410u);
    std::array<std::uint32_t,3> globals_78024c_780258_78026c{};
    std::array<std::uint8_t,64> view_inverse_95dba0{};
    bool view_inverse_valid{};
    PcRaceMemory memory;
    // Course record table ([7D33BC] - selected*0x78). Identified at each
    // rebuild: the mission selection records (44D720 direct) or the 44DA00
    // course file records, whichever holds the selected 7D30A8 record.
    std::uint8_t* table{};std::size_t table_bytes{};std::string table_source;
    // Allocations of the 44FD80 mode-7 loader (and of START's 44C310 lanes
    // mirrored at their completion): payload then the 8-byte handle
    // {+0 payload pointer, +4 mode}; the handle is base+payload size.
    std::map<std::uint32_t,std::vector<std::uint8_t>> allocations;
    std::uint32_t next_allocation{};
    struct Slot { PcRaceAsyncSlot pc{}; std::uint32_t path{},mode{}; std::string name; };
    std::array<Slot,RaceAreaSlotCount> slots{};
    // START lanes mirrored into 7D306C/7D3070 (44C310 run by 49BA80).
    std::array<std::uint32_t,2> start_lane_crc{},start_lane_state{};
    std::uint32_t start_lane_reset_count{0xffffffffu};
    // 7B11A8 allocator selector stack (440D10/440D30/440D50/440D70).
    driving::PcAllocatorStateStacks alloc_stacks{};
    // Course road tables 804480 / 800DA8 / 80A670 (46FAC0/46FC40/46FDE0,
    // pc_car_services). Exposed read-only through road_tables().
    std::vector<std::uint8_t> road_primary=std::vector<std::uint8_t>(6u*driving::PcRoadPrimaryBlock);
    std::vector<std::uint8_t> road_preload=std::vector<std::uint8_t>(2u*driving::PcRoadPrimaryBlock);
    std::vector<std::uint8_t> road_secondary=std::vector<std::uint8_t>(12u*driving::PcRoadSecondaryBlock);
    driving::PcRoadTableLoaderState road_loader{};
    std::uint32_t road_requests{},road_bytes{};
    driving::PcRoadTableArena road_arena(){return {driving::Bytes(road_primary.data(),road_primary.size()),
        driving::Bytes(road_preload.data(),road_preload.size()),driving::Bytes(road_secondary.data(),road_secondary.size())};}
    // 4103A0 renderer words 89EDBC/89EDC0/89EDC4 (consumer 4103F0/40FD70 not wired).
    std::array<std::uint32_t,3> shader_mode_89edbc{};
    // Resources this module requested (448AD0) / released (448990).
    std::set<std::uint32_t> requested,released;
    // Race-manager hook (event 359 owner): 4502C0 4502D0 450300 450310
    // 450380 450130 44FF10 ... Return true with eax when answered.
    std::function<bool(const PcRaceCall&,std::uint32_t& eax)> race_manager;
    std::uint32_t memory_frame{};                      // race manager binding: completed_frames+1 of the last rebuild
    // ---- course-prog: stage-object lanes / area switch: begin ----
    // Stage-object lanes of module 4F0xxx (PC .bss 84CE70..84D8F0), loaded
    // by 4F10D0 (placements: state 84CE70[l], slot 84D8E4[l], handle
    // 84D6CC[l]) and 4F0430 (dynamic tables: state 84CE7C[l], slot
    // 84CE88[l], handle 84CE94[l], payload 84D6C0[l]); released by
    // 4F11B0/4F0600. Images are PC-layout allocations (payload, then the
    // {payload, mode} handle) at StageObjectBase + (kind*3+lane)*window.
    // START's 49BA80 lane-0 results (its own native loaders) are adopted
    // when they complete, as START's 44C310 lanes are (sync_start_lanes).
    struct ObjectLane {
        std::uint32_t state{},handle{},payload{},token{},mode{};
        std::vector<std::uint8_t> image;std::string path;std::uint32_t loads{};
    };
    std::array<ObjectLane,3> placements{},dynamics{};
    std::uint32_t start_objects_crc{0xffffffffu},start_objects_state{0xffffffffu},start_dynamics_state{0xffffffffu};
    // Car reflection cube (414340 face scenes): 44CD30 / 451C20 runs and their aborts.
    PcCarReflectionStats cube{};
    std::uint32_t cube_model_runs{},cube_sky_runs{},cube_aborts{};std::string cube_error;
    std::uint32_t cube_without_course{};   // 44CD30 faces before any course (frontend car select)
    std::uint32_t object_events_opened{},object_event_ranges_closed{};
    // 4EFD20 lane != 0 target 6A5DC4..6A5DDB (read by the traffic 4EFB90 fallback;
    // 4EFED0 not ported). Initial value: the EXE .data image of 6A5DC4.
    std::array<std::uint32_t,6> light_words_6a5dc4{0x00000000u,0x00001402u,0x0c640000u,0x0000ffffu,0x50000000u,0x40103040u};bool light_words_written{};
    std::uint32_t collision_releases{};
    std::vector<std::vector<std::uint8_t>> retired_images;   // released lane images, dropped at the next rebuild
    // ---- course-prog: end ----
    // Report.
    std::map<std::uint32_t,std::uint32_t> routed,missing,recorded;
    std::vector<std::pair<std::uint32_t,std::uint32_t>> routed_order,missing_order;   // (pc, first frame)
    std::vector<std::uint32_t> sound_effects_4249f0;
    std::uint32_t fault_pc{};std::string fault,last_error;
    std::map<std::string,std::uint32_t> aborts;       // callback abort reasons
    std::uint32_t frame{};
    std::uint32_t inits{},controls{},control_aborts{},displays{},display_aborts{},destroys{};
    std::uint32_t sky_inits{},sky_controls{},sky_control_aborts{},sky_displays{},sky_display_aborts{};
    std::uint32_t last_area_draws{},last_sky_draws{},total_draws{},skipped_objects{},bank_failures{};
    std::map<std::uint32_t,std::uint32_t> skipped_ids;   // resource id -> draws skipped (not resident)
    bool resource_requested{};   // 42DEB0 / 429920 asked by the AREA (44E590 on the final course): ready at once
    std::map<std::uint32_t,std::uint32_t> draws_by_pc;
    std::vector<PcVehicleDrawCall> draw_list;          // display draw list (reused)
    std::set<std::uint32_t> banks_loaded;
    bool initialized{},sky_initialized{},init_failed{},sky_init_failed{},colour_hooked{};
};
NativeRaceAreaRuntime& native_race_area(NativeRuntimeContext&);
bool native_race_area_bind_records(NativeRuntimeContext&);   // 7D33BC table + START words
// Event callbacks 44CB00 / 44F7C0 / 44B7F0 / 451A30 / 451B40. False when the
// callback is not one of them. `renderer` may be null (renderer-side
// services then report missing).
bool native_race_area_event_invoke(NativeRuntimeContext&,std::uint32_t callback,std::uint32_t work,
    std::uint32_t event_id,driving::PcMatrixStack& matrices,PcSceneRenderer* renderer);
// Display callbacks 44F120 / 4521C0 run from the frame renderer's display
// dispatch; the draw list is executed on the renderer. False when the
// callback is not one of them.
bool native_race_area_display(NativeRuntimeContext&,PcSceneRenderer&,std::uint32_t callback,
    std::uint32_t work,std::uint32_t event_id);
// 46FC30(which) / 46FE50(which, kind) on the owned road tables (used by the
// AREA and by START's scene owner 49BA80). Returns the PC EAX.
std::uint32_t native_race_road_tables_46fc30(NativeRuntimeContext&,std::uint32_t which);
// Course teardown services 43DE50(lane) / 44A1A0 / 4F11B0(lane) / 4F0600(lane).
void native_race_area_teardown(NativeRuntimeContext&,std::uint32_t pc,std::uint32_t arg);
std::uint32_t native_race_area_call(NativeRuntimeContext&,PcSceneRenderer*,const PcRaceCall&);
std::uint32_t native_race_road_tables_46fe50(NativeRuntimeContext&,std::uint32_t which,std::uint32_t kind);
// Read-only view of the road tables for 4A4010 GetRoadOfs / CalcOfsLeftLane.
driving::PcRoadTableArena native_race_road_tables(NativeRuntimeContext&);
// PC 44F990 mode-7 transfer of a file into a handle allocation: the
// payload (.sz: the inflated bytes after the 4-byte size, which must equal
// the rest; other files: the whole file) followed by {payload, mode}.
// Returns false (error set) when the .sz size word does not match.
bool race_area_loader_image(const std::vector<std::uint8_t>& file_bytes,bool sz,std::uint32_t base,
    std::uint32_t mode,std::vector<std::uint8_t>& image,std::string& error);
// PC 44FC60(slot, &a, &b, out) for a completed (state 4) or empty slot:
// a = +24, b = +28, out = +14 (440CC0), then the protected bridge at 44FCA4
// clears +14 (measured) and 44FBE0/44F950 zero the slot. Returns the handle
// 44FBE0 frees through 440CD0 (out == null), else 0.
std::uint32_t race_async_take_44fc60(PcRaceAsyncSlot&,std::uint32_t* a,std::uint32_t* b,std::uint32_t* out);
// PC 407C30 / 407E40 on the light words 899B98..89A4F8 (lights = 0x960 bytes).
std::uint32_t race_light_offset(std::uint32_t a,std::uint32_t b,std::uint32_t c);
void race_light_word_407c30(driving::Bytes lights,std::uint32_t a,std::uint32_t b,std::uint32_t c,std::uint32_t v);
void race_light_words_407e40(driving::Bytes lights,std::uint32_t a,std::uint32_t b,std::uint32_t c,
    std::uint32_t v0,std::uint32_t v1,std::uint32_t v2);
// PC 4103A0: words 89EDBC, 89EDC0, 89EDC4.
std::array<std::uint32_t,3> race_shader_mode_4103a0(std::uint32_t a0,std::uint32_t a1);
// PC 406780(object, mask) over a loaded bank (system section with object
// records at +18 whose pointer fields are system offsets, count = 448810 +0C).
std::uint32_t race_mesh_count_406780(driving::Bytes system,std::uint32_t object_count,std::uint32_t object,std::uint32_t mask);
// PC 43F960: [78024C] == 2 || == 3.
std::uint32_t race_preset_43f960(std::uint32_t preset_78024c);
// ---- course-prog: stage-object lanes / area-switch services: begin ----
// PC 4F0430 state 1 (after 44FC60): relocates the dynamic stage-object
// table loaded at payload p (84D6C0[lane]) in place.
void race_objects_relocate_4f0430(const PcRaceMemory&,std::uint32_t p);
// PC 4F0D10(lane) after its protected prologue (measured: EAX =
// [84D6CC+lane*4], TEST AX,AX): walks the 0x28-byte placement records of
// [root] while +4 >= -10000.0 and opens event [5DA050+lane*8]+k with
// function [5DA068+type*4] (440110). Returns the number opened.
std::uint32_t race_objects_open_4f0d10(const PcRaceMemory&,std::uint32_t lane,std::uint32_t root,
    void(*open)(void*,std::uint32_t event,std::uint32_t function),void* user);
// PC 4F0910(v): eax = [84D8F0], [84D8F0] = v.
std::uint32_t race_objects_frame_4f0910(const PcRaceMemory&,std::uint32_t v);
// PC 4F0CB0(id, function, data): 440110(id, function), then the record 84D6D8 + (id-15C)*34:
// +0..+27 = the 0x28 bytes at data, +28 = 1, +2C = function, +30 = [84D8F0].
void race_objects_dynamic_open_4f0cb0(const PcRaceMemory&,std::uint32_t id,std::uint32_t function,std::uint32_t data,
    void(*open)(void*,std::uint32_t event,std::uint32_t function),void* user);
// PC 4EFD20(e, lane): lane != 0 copies 6 dwords to 6A5DC4; lane 0 copies up
// to 7 12-byte entries (until u16 FFFF) to 84BD08 and terminates the list.
void race_objects_copy_4efd20(const PcRaceMemory&,std::uint32_t e,std::uint32_t lane);
// ---- course-prog: end ----
// ---- race manager binding: begin ----
// The AREA memory (PC addresses of the area block, course records, EXE data,
// the race-manager block ...) for the event-359 owner. rebuild=true syncs
// START's lanes, rebuilds the mappings (as before each AREA callback) and
// mirrors START's 44D720 words 7D33BC/7D3188 while 44CB00 has not run;
// rebuild=false returns the memory as the current AREA callback built it.
PcRaceMemory& native_race_area_memory(NativeRuntimeContext&,PcSceneRenderer* renderer,bool rebuild);
// Executes a race draw list (405360 / 4056D0 on resident banks, 4044A0 /
// 4044C0 / 4044E0 LOD threshold, 4044F0, 404540, 4052B0, 4052C0, 405350)
// on the renderer. Returns the objects drawn.
std::uint32_t native_race_draw_list_execute(NativeRuntimeContext&,PcSceneRenderer&,const std::vector<PcVehicleDrawCall>&);
// Frame layer-0 leaf 414340 (pc_car_reflection): the player car's reflection
// cube faces, their course model 44CD30 and sky 451C20 run here on the AREA
// memory; the cube is created (413B30) on the first call.
bool native_car_reflection_leaf(NativeRuntimeContext&,PcSceneRenderer&);
// 413B30 on the renderer once (the cube [8A89F4], its depth surface, the constants).
void native_car_reflection_init(PcSceneRenderer&);
// Bank of a resource on the renderer when START/AREA made it resident (the
// PC 448810 bank of a resource that is not loaded has no objects): false
// when it is not resident or cannot be loaded (counted by the AREA owner).
bool native_race_bank_ensure(NativeRuntimeContext&,PcSceneRenderer&,std::uint32_t resource);
// ---- race manager binding: end ----
// One-line status for traces.
std::string native_race_area_status(const NativeRuntimeContext&);
// Full report (services by first frame, counts).
std::string native_race_area_report(const NativeRuntimeContext&);
}
