#pragma once
// Native runtime binding of the GAME race manager (event 359: init 450790,
// control 4515B0, display 44FE00, destroy 44FE10; race_manager.hpp).
//
// NativeRaceManager (in NativeRaceState) is the single owner of the PC block
// 0x7D3650..0x7D39FF (RaceManagerState): race time 7D394C, route choices
// 7D39A0, result flags 7D39F0, game-over state 7D38F0, ... Every other native
// reader (race_player_car, the AREA owner's race_manager hook, the robots,
// the GAME mode control's 450240 poll) reads this state.
//
// RaceManagerWorld is filled from the real owners before each call (no
// copy survives the call; the 635F2C cache is written back):
//   event works 8..31: id 8 = the shared car (car_select.car_799d18, 7804B0);
//     ids 9..31 = the static PC works 7815A0 + (id-9)*0x10F0 owned here
//     (.bss; no native owner exists; their events are never opened on the
//     single-player route; an opened one is reported and latches);
//   course selection *7D3188, 7D33BC/7D33C4 stage records, 7D2E98 time
//     table, 7D33B0 = the AREA owner's memory (race_area_runtime);
//   635F2C/635F30 = game_mode.course_runtime (shared with the car and area);
//   78024C/780258/78026C, 7C24C0 = active license +0xE0, 80FB14, 7F95A8.
// RaceManagerServices are bound to the native owners (see the .cpp and the
// report). A service without a native port is counted by PC with its first
// frame; a void service whose effect lands in another unported system's own
// state (and that the manager's outputs never read back) is skipped and the
// call continues; any other missing service throws, the callback is aborted
// and the manager is fault-latched (later init/control callbacks skipped and
// counted; accessors keep answering from the state).
#include "platform/race_manager.hpp"
#include "platform/race_area.hpp"
#include "driving/pc_matrix_stack.hpp"
#include <array>
#include <functional>
#include <map>
#include <string>
#include <vector>
namespace outrun::platform {
struct NativeRuntimeContext;
class PcSceneRenderer;
struct NativeRaceManager {
    RaceManagerState state{};                                   // 7D3650..7D39FF
    // PC .bss the manager writes whose owning systems are not native yet.
    // Owned here (single native owner) until those systems land.
    static constexpr std::uint32_t CarWorkBase=0x7815a0u,CarWorkStride=0x10f0u,CarWorkFirst=9u,CarWorkCount=23u;
    std::vector<std::uint8_t> car_works_7815a0=std::vector<std::uint8_t>(std::size_t(CarWorkCount)*CarWorkStride); // events 9..31
    std::array<std::uint8_t,kVisibility84cec8Size> visibility_84cec8{}; // 4F0DD0 / 4F0E40 (race_traffic maps it)
    std::array<std::uint8_t,kScore84def8Size> score_84def8{};         // 4F2AC0 / 4F2DF0 / 4F2B20 (race_traffic_score.inc)
    std::uint16_t word_8037bc{};                                // 46FAB0
    std::uint32_t goal_6840ec{},goal_6840f0{};float goal_6840f4{};  // 4AEEF0(course, level, ratio)
    std::uint32_t clock_7de388{};                               // 458450: [7DE388] = [7F1938]
    std::uint32_t input_lock_7d6764{};                          // 453080(value)
    std::uint8_t flag_84490c{};                                 // written 1 by 450CC0
    // 2D race display (HUD 47DAC0 init / 47EC00 control / 47DC00 destroy):
    // another port owns it. Returns true when it handled the call; unset (or
    // false) = counted in `missing` and skipped (the manager never reads
    // back HUD state).
    std::function<bool(std::uint32_t pc)> hud_2d;
    // Report.
    std::map<std::uint32_t,std::uint32_t> routed,missing,skipped_void;
    std::vector<std::pair<std::uint32_t,std::uint32_t>> missing_order;   // (pc, first frame)
    std::uint32_t inits{},controls{},displays{},destroys{},skipped_callbacks{},accessor_calls{};
    std::uint32_t fault_pc{},fault_frame{};std::string fault,last_error;
    bool initialized{},latched{};
    std::uint32_t frame{};
};
// Event callbacks 450790 / 4515B0 / 44FE10; false when the callback is not
// one of them. `renderer` may be null (the AREA memory then has no 95DBA0).
bool native_race_manager_invoke(NativeRuntimeContext&,std::uint32_t callback,driving::PcMatrixStack&,PcSceneRenderer* renderer);
// Display 44FE00 (frame renderer display dispatch); false otherwise.
bool native_race_manager_display(NativeRuntimeContext&,std::uint32_t callback);
// Manager accessors and route choice for other native modules (AREA hook,
// robots): 44FDF0 44FE30 44FE40 44FE50 44FE70 44FEF0 44FF10 450110 450130
// 450140 450160 450230 450240 4502C0 4502D0 4502E0 450300 450310 450320
// 450380 4503A0 4503C0 450560..450600 450610 450630 450670..4506A0 450750
// 450780 451350 (any index), 451140. Uses the AREA memory already built by
// the caller when `area_built` (inside an AREA callback), else prepares it.
// False when `pc` is not a manager accessor.
bool native_race_manager_call(NativeRuntimeContext&,std::uint32_t pc,const std::uint32_t* args,std::size_t count,
    std::uint32_t& eax,bool area_built);
// 451350(index) / 451140(index, value) for native callers (race_player_car).
std::uint32_t native_race_route_451350(NativeRuntimeContext&,std::int32_t index);
// View of the 7D39A0.. route words for driving::PcRouteContext (0x60 bytes:
// 7D39A0..7D39FF, the block's end).
driving::Bytes native_race_route_words(NativeRuntimeContext&);
// PC 44C020 (START_Init 49DD0D): copies the 0x1C2-byte time table of the
// course preset [78024C] (0 -> row 0, 1 -> 1, 2 -> 5, 3 -> 6, else 0) from
// .rdata 5A2A20 + row*0x1C2 to 7D2E98 (area block).
void race_time_table_44c020(std::uint32_t preset_78024c,std::uint8_t* dst_7d2e98);
// One-line status / full report.
std::string native_race_manager_status(const NativeRuntimeContext&);
std::string native_race_manager_report(const NativeRuntimeContext&);
}
