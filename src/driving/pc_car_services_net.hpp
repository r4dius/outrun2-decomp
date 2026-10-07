#pragma once
// PC car/network services reached from GamePlCar_Ctrl (route_state 4), the
// manager forced-cruise path (0x46C390 -> 0x4FB870), the road-mu manager
// lookup behind 0x46C450 (0x4FC390) and allocate_result 0x451180.
// All PC globals are explicit inputs/views. Serialized guest pointers are
// never followed (e.g. [event+0x2B4] params is an explicit Bytes view).
#include "driving/pc_driving.hpp"
#include "driving/pc_collision.hpp"
#include <array>
#include <cstdint>

namespace outrun::driving {
struct PcRoadInfoContext;

// Result of one PC GetCsRoadInfoByCsLen 0x43E3B0 call as consumed by the
// callers below: the success flag (EAX != 0) and the 0x64-byte road info.
// Only the defined bytes [0,0x58) are read, and only when found.
struct PcRoadInfoResult {
    bool found{};
    Bytes info;
};

// ---- 0x457480 / 0x457770 -------------------------------------------------
// PC 0x457480 (edi = event). Route progress between the road-info envelope
// edges: returns the raw x87 ST0 (80-bit) result. When road info is not
// found it returns (long double)0.0f (PC constant 0x619A34).
// Reads event +0x14..+0x1F (position) and +0x260 (u16 course length).
long double pc_route_progress_457480(Bytes event,const PcRoadInfoResult& road);
// Same, performing the original GetCsRoadInfoByCsLen(event+0x5C, hint -1)
// with the verified native 0x43E3B0 port (matrix stack scratch included).
long double pc_route_progress_457480(Bytes event,PcRoadInfoContext& road);
// PC 0x457770: table_7de480 is the 0x7DE480 array (stride 0x6C) and slot is
// byte [0x7DD138]; event is [0x799D18]. Stores (float)457480() at
// table[slot*0x6C]. Nothing else is written.
void pc_route_state4_457770(Bytes event,std::uint8_t slot_7dd138,Bytes table_7de480,
                            const PcRoadInfoResult& road);
void pc_route_state4_457770(Bytes event,std::uint8_t slot_7dd138,Bytes table_7de480,
                            PcRoadInfoContext& road);

// ---- 0x4FB870 ------------------------------------------------------------
// PC thiscall 0x4FB870(manager=ecx, event, work), ret 8. Manager forced
// cruise. manager is the 0x7F9460 object (reads +0x184, +0x7668..+0x7677),
// params is the explicit [event+0x2B4] view (reads +0xB48, +0xB94, +0x10A0,
// +0x1644). Early exit (no writes) when !(manager+0x7670 > 0) (ordered).
void pc_manager_forced_cruise_4fb870(Bytes manager,Bytes event,Bytes work,Bytes params);

// ---- 0x4FC390 ------------------------------------------------------------
// PC thiscall 0x4FC390(manager=ecx, road_id, surface_flags) -> float (ST0),
// ret 8. road_id is not read. mu_764c is manager+0x764C.
float pc_manager_road_mu_4fc390(float mu_764c,std::uint32_t road_id,std::uint32_t surface_flags);
// Adapter for RoadMuSource (pc_road_mu.hpp): context points at a float
// holding manager+0x764C.
float pc_manager_road_mu_lookup(void* mu_764c,std::uint32_t road_id,std::uint32_t surface_flags);

// ---- 0x4A4440 / 0x4506B0 / 0x449B30 / 0x449C10 / 0x451180 ---------------
struct PcRoadSideOffsets { float a3{},a4{}; };
// PC 0x4A4440(hint, place, pos, &a3, &a4) given the 43E3B0 result for
// (place, hint). Writes *a3 then *a4 (returned here in that order).
PcRoadSideOffsets pc_road_side_offsets_4a4440(const PcRoadInfoResult& road,CourseProbe position);
// PC 0x4506B0 (esi = event), SSE float result in XMM0. road_5c / road_184
// are the 43E3B0 results for (event+0x5C, hint event+0x1C0) and
// (event+0x184, hint event+0x1C0). Reads event +0x14..+0x1F, +0x16C..+0x177.
float pc_lane_ratio_4506b0(Bytes event,const PcRoadInfoResult& road_5c,const PcRoadInfoResult& road_184);
float pc_lane_ratio_4506b0(Bytes event,PcRoadInfoContext& road);
// PC 0x449B30 general form (frames as u32 [pushed int], extra seconds float):
// returns {hours, minutes, seconds, milliseconds} (u16 each).
std::array<std::uint16_t,4> pc_time_split_449b30(std::uint32_t frames,float extra);
// PC 0x449C10(h,m,s,ms) (u16 each) -> ((h*60+m)*60+s)*1000+ms.
std::uint32_t pc_time_join_449c10(std::uint16_t h,std::uint16_t m,std::uint16_t s,std::uint16_t ms);

struct PcAllocateResultState {
    std::int32_t counter_656234{};   // 48B350: [0x656234] < 0x3C
    std::uint32_t frames_7d3948{};   // pushed as int to 449B30
    std::uint32_t mode_78024c{};     // 43F960: in {2,3}
    std::uint32_t result_7d3738{};
    std::uint32_t result_7d3698{};
};
// PC 0x451180(request). event is [0x799D18] (only read on the live path).
// Live path: request != 0 and counter < 0x3C. The road-info variant takes the
// two 43E3B0 results 4506B0 would obtain (ignored on the stored path).
std::uint32_t pc_allocate_result_451180(std::uint32_t request,const PcAllocateResultState& state,
                                        Bytes event,const PcRoadInfoResult& road_5c,
                                        const PcRoadInfoResult& road_184);
std::uint32_t pc_allocate_result_451180(std::uint32_t request,const PcAllocateResultState& state,
                                        Bytes event,PcRoadInfoContext& road);
} // namespace outrun::driving
