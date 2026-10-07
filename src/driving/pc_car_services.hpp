#pragma once
// Leaf services of the player-car control tree (GamePlCar_Ctrl 0x4A8330 /
// CommonPlCar 0x4A8100) that had no native implementation. Every PC global is
// an explicit input, output or byte view; no guest pointer is followed.
// Verified against the original x86 code by tools/oracle/car_services_probe.inc.
#include "driving/pc_driving.hpp"
#include "driving/pc_matrix_stack.hpp"
#include <array>
#include <cstdint>
#include <string>
namespace outrun::driving {

// ---- 0x0043D130: per-lane rates -----------------------------------------
// selector == -1, or no table for the course type ([0x780160+type*4] == 0,
// passed as nullptr) -> four 1.0f. Otherwise bytes table[selector*4+k] times
// [0x6280BC] (1/255). The type table is the pointee of [0x780160+type*4].
std::array<float,4> pc_lane_rates_43d130(std::int32_t selector,const Bytes* type_table);

// ---- 0x004963E0: race-ready predicate ------------------------------------
// [0x780258]==4 && [0x83637C]!=0 && [[0x83637C]+0x10]!=0.
bool pc_race_ready_4963e0(std::uint32_t route_state_780258,bool state_present_83637c,
                          std::uint32_t state_field_10);

// ---- 0x004F53B0: versus flag (relocated snippet 0x44785A: mov al,[0x850B04])
bool pc_versus_flag_4f53b0(std::uint8_t byte_850b04);

// ---- 0x00456BB0: reference course position (thiscall ecx=0x7DE418) -------
// manager = the rank manager block 0x7DE418: +0 slot count, slot i at
// 4+i*0x6C (+0 record pointer, +4 key, +0x54 stamp, +0x5A u16 position).
// slot_records[i] = view of the pointee of slot i's +0 (>= 0x0C bytes:
// +0 u16 key, +8 dword kind); PC dereferences it for every slot.
std::uint16_t pc_reference_course_position_456bb0(Bytes manager,const Bytes* slot_records,
                                                  std::size_t slot_record_count,
                                                  std::uint32_t clock_7f1938,
                                                  std::uint8_t versus_850b04);

// ---- 0x00450130: protected gate (relocated snippet 0x4484A7) --------------
// Measured: the snippet resolves [0x103938C]=0x003E9CF8 ror (byte 0x43D1DC=0xFF)
// = 0x7D39F0 and returns ([0x7D39F0]>>1)&1.
std::uint32_t pc_protected_gate_450130(std::uint32_t flags_7d39f0);

// ---- 0x004B5FD0: owner mode (relocated snippet 0x103C899: mov al,[0x79FB4E])
// (byte&3)==2 ? [[0x799CA0]+0x1C] : 0.
std::int32_t pc_owner_mode_4b5fd0(std::uint8_t flags_79fb4e,std::int32_t owner_mode_1c);

// ---- 0x0044F0F0 protected tail (relocated snippet 0x40E705) ----------------
// Measured: mov eax,[0x01039DC4] (file value 1) keeping the CMP flags, so the
// gate returns 1 when 0x44DDC0 != -1 and 0 otherwise.
constexpr std::uint32_t PcRoadStageGateValue44f0f0=1u;
std::uint32_t pc_road_stage_gate_44f0f0(std::int32_t stage_unique,std::uint16_t course_position,
                                        std::uint16_t rolling_reference);

// ---- 0x004A3F8A protected cache selector (VM [0x1039AC0] -> 0x10428B0) -----
// Measured: EAX = dword event+0x10D0 (the cached hint); all other registers
// and memory unchanged; independent of .data.
std::int32_t pc_road_cache_selector_4a3f8a(Bytes event);

// ---- 0x004A45F9 protected selector (VM [0x10398B8] -> 0x1042610) -----------
// Measured: EDI = dword event+0x230; independent of .data.
std::int32_t pc_ofs_left_lane_selector_4a45f9(Bytes event);

// ---- 0x0045A2B0 CommRace_GetRank (VM [0x1039CCC] -> 0x10431F0) -------------
// The VM entry is `cmp dword [0x78026C],0x10` (re-measured with 78026C poked
// to 0x10: it resumes at 0x45A2C4, otherwise at 0x45A2B9).
//  * mode != 16: AL = byte [0x7DF118 + id], EAX upper 24 bits = id's.
//  * mode 16, variant 780258 not 3/4: EAX = variant with AL cleared.
//  * mode 16, variant 3/4: network path 456870/459D10/459E10 (not ported):
//    throws std::logic_error.
std::uint32_t pc_comm_race_get_rank_45a2b0(std::uint32_t id,Bytes rank_table_7df118,
                                           std::uint32_t mode_78026c,std::uint32_t variant_780258);

// ---- Course road-sample tables (0x804480 / 0x800DA8 / 0x80A670) -------------
// primary   = 6 blocks x 0x1030 at 0x804480 ("_stg" files, row 0)
// preload   = 2 blocks x 0x1030 at 0x800DA8 ("_stg" files, next row)
// secondary = 12 blocks x 0x70C at 0x80A670 ("_bra" files)
// Offsets are in bytes from the region start; the PC indexing may cross block
// boundaries, so regions are contiguous views.
struct PcRoadTableArena {
    Bytes primary;   // >= 6*0x1030
    Bytes preload;   // >= 2*0x1030
    Bytes secondary; // >= 12*0x70C
};
constexpr std::uint32_t PcRoadPrimaryBlock=0x1030u,PcRoadSecondaryBlock=0x70cu;
constexpr std::uint16_t PcRoadTableMagic=0x4f53u;

// 0x0046F7A0: lane merge flags from the primary table (event+0x5C must be 0,
// else 0). Bits 0,1,4,5 as the PC loop computes them.
std::uint8_t pc_route_flags_46f7a0(Bytes event,const PcRoadTableArena& tables);

// 0x00479D90: distance between the samples of two lane tables at the same
// sample index, transformed by the course display matrix of place+0 (0x44BED0:
// 0 -> 0x7D2DA0, else 0x7D3190). Returns the x87 ST0 (FLT_MAX [0x59943C] when
// a sample is off-road or the second table is not loaded). Uses the matrix
// stack exactly as the PC (push-load, two points, pop).
struct PcRouteWidthResult {long double value{};bool used_matrices{};};
PcRouteWidthResult pc_route_width_479d90(Bytes on_road_place,std::int32_t row,std::int8_t selector,
                                         const PcRoadTableArena& tables,Bytes matrix_7d2da0,
                                         Bytes matrix_7d3190,PcMatrixStack& matrices);

// Loader state of the course road-table streamer 0x46FAC0/0x46FC40/0x46FDE0.
struct PcRoadTableLoaderState {
    std::array<std::uint32_t,24> status{};   // 0x804390 (0 new, 1 pending, 2 done)
    std::array<std::uint32_t,24> handle{};   // 0x80A5A8 (async file slot pointer)
    std::array<std::int32_t,2> stage{};      // 0x803730 [which]
    std::array<std::int32_t,2> counter{};    // 0x800D44 [which]
    std::string path;                        // 0x800D58 filename scratch
};
enum class PcRoadTableRegion : std::uint8_t {Primary,Preload,Secondary};
struct PcRoadTableRequest {       // 0x44FD20 request {path,dest,size,0,0,0,4}
    std::string path;
    PcRoadTableRegion region{};
    std::uint32_t offset{};       // byte offset in the region
    std::uint32_t size{};
    std::uint32_t flags{4};
};
// Platform async file service (PC 0x44FD20 submit, 0x44F880 finished,
// 0x44FBE0 release). Handle 0 is the PC null slot: finished(0) must return
// nonzero and release(0) must be a no-op (the PC routines treat null so).
struct PcRoadTableFileService {
    void* user{};
    std::uint32_t (*submit)(void*,const PcRoadTableRequest&){};
    std::uint32_t (*finished)(void*,std::uint32_t handle){};
    void (*release)(void*,std::uint32_t handle){};
};
// 0x0044F880 (relocated snippet 0x44838B: mov eax,[0x01039E98]=1): 1 when
// the async slot is null, empty (+0 == 0) or its state +4 is outside 0..2;
// 0 while the slot is in flight. The reference implementation of
// PcRoadTableFileService::finished for a PC-layout slot.
std::uint32_t pc_async_slot_finished_44f880(bool slot_present,std::uint32_t slot_word0,std::int32_t slot_state);
// 0x0044C680: filename "\OCP\ocp" + group + {_stg,_bra}[which] + course +
// {_othcar,_rvlcar,_01}[row'] + {_01.._12}[slot] + "_tgt.sz".
// kind -> (group,course): >=0x3C table 0x5A3C88 (0x3C..0x41 only), 0x2D.. ->
// (_cvr,kind-0x2D), 0x1E.. -> (_olr,kind-0x1E), 0x0F.. -> (_cvt,kind-0x0F),
// else (_old,kind). row'=2 when which==0 && row==1 && [0x7D33C8]==1.
std::string pc_road_table_path_44c680(std::uint32_t which,std::uint32_t kind,std::uint32_t row,
                                      std::uint32_t slot,std::uint32_t selected_7d33c8);
// 0x0046FAC0 (via 0x46FC30): reset one table family.
void pc_road_table_reset_46fac0(PcRoadTableLoaderState&,const PcRoadTableArena&,std::uint32_t which);
// 0x0046FC40: one slot step; returns the PC EAX (1 = stay, 0 = advance).
std::uint32_t pc_road_table_slot_46fc40(PcRoadTableLoaderState&,const PcRoadTableArena&,
                                        std::int32_t counter,std::int32_t row,std::uint32_t which,
                                        std::uint32_t kind,std::uint32_t selected_7d33c8,
                                        const PcRoadTableFileService&);
// 0x0046FDE0 (via 0x46FE50(which,kind)): returns 0 when the family is loaded.
std::uint32_t pc_road_table_step_46fde0(PcRoadTableLoaderState&,const PcRoadTableArena&,
                                        std::uint32_t which,std::uint32_t kind,
                                        std::uint32_t selected_7d33c8,const PcRoadTableFileService&);
}
