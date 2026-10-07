#pragma once
#include <initializer_list>
// Native runtime binding of the rival racers / traffic module (race_traffic.hpp):
//   race manager hooks 47DAC0 init / 47EC00 control / 47DC00 destroy and the
//   car events it opens (event function 0x55: 4AD280 init, 47E780 control,
//   470560 destroy; display 4AE5F0 / shadow 4ADAC0 as the ghost cars).
// The module runs over a PcRaceMemory built per call: the AREA owner's
// regions (EXE ranges, area block, race manager block, player car, the car
// works 7815A0..), then what this binding owns on top of them: the module
// state (PcTrafficState), the racer setup words 80FB00..80FB50 and their
// tables (RacerSetupState, copied in and back), a writable image of the event
// records 799B30 and flags 79FB48 (refreshed after every event service),
// the course appear tables and the cells listed in race_traffic_runtime.cpp.
// A service without a native port (or an unmapped address) latches the
// module: traffic stops for the race, the fault is reported, nothing is
// invented.
#include "platform/race_traffic.hpp"
#include "driving/pc_matrix_stack.hpp"
#include "driving/pc_common_control.hpp"
#include "platform/race_hud_navi.hpp"   // RaceHudDraw
#include <memory>
#include <array>
#include <map>
#include <string>
#include <vector>
namespace outrun::platform {
struct NativeRuntimeContext;
class GuestHeap;   // translated_crt.hpp
struct NativeRaceTraffic {
    PcTrafficState state;
    PcRaceMemory memory;
    std::array<std::uint8_t,0x50> racers_80fb00{};      // 80FB00..80FB4F image (pointers rebased)
    std::vector<std::uint8_t> events_799b30=std::vector<std::uint8_t>(410u*0x3cu);
    std::array<std::uint8_t,0x200> flags_79fb48{};
    std::vector<std::uint8_t> data_64de00;               // .data 64DE00..64E200 (rival configs / class table), written by the module
    std::vector<std::uint8_t> wall_850ba0=std::vector<std::uint8_t>(0x900);
    std::vector<std::uint8_t> parameters_5e3140;           // driving parameter arena (copy of the OR2DRV1 pack)
    std::vector<std::uint8_t> data_6a5df8;               // .data 6A5DF8..6A6DE8 (course appear source tables), written by the module
    std::array<std::uint8_t,0xc> seeds_6a4e2c{};         // 44B750 random seeds (.data, three LCGs)
    std::array<std::uint8_t,4> cell_799d18{},cell_79f574{},cell_680ad4{},cell_680ad8{},cell_64e190{},cell_64e194{};
    std::array<std::uint8_t,0x10> cells_780228{};       // 780228[type] -> course length tables (synthetic bases 5D400000+)
    std::array<std::uint8_t,4> cell_8421c0{},cell_836388{},cell_83637c{};
    // The C2C request manager 7F9460 (Clarissa's traffic requests, game variant 5; +60 = 7F94C0
    // its "active" word, 55A930): its .bss block and the guest heap of its lists and request
    // objects (CRT new 5802CF / delete 5801A7 of its code).
    static constexpr std::uint32_t RequestsBase=0x7f9460u,RequestsBytes=0x7700u;
    static constexpr std::uint32_t RequestsHeapBase=0x0d000000u,RequestsHeapSize=0x800000u;
    std::vector<std::uint8_t> requests_7f9460=std::vector<std::uint8_t>(RequestsBytes);
    std::unique_ptr<GuestHeap> requests_heap;
    // its .data 64F000..64F0FF (resource ids, REQUEST names, 64F0CE) and the .bss cells of its
    // modules 4F1860 / 4FC9A0.. / 5064E0 / 5099C0 / 50E9F0 / 508490.. (zero at start)
    std::vector<std::pair<std::uint32_t,std::vector<std::uint8_t>>> requests_cells;
    std::uint32_t fault{};                // first unported PC / unmapped address (latched)
    std::string error;
    bool latched{};
    std::uint32_t inits{},controls{},destroys{},car_inits{},car_controls{},car_destroys{},rankings{},skipped{};
    std::uint32_t skipped_texture_swaps{};
    std::uint32_t last_routed_pc{};          // the last PC service a traffic run answered (latch reports)   // 406630 (no native producer for the [8A89F4] texture)
    std::map<std::uint32_t,std::uint32_t> routed,missing;
    // Course objects (OSO): their works (440A60, one 0x500 slot per event at ObjectWorkBase),
    // the writable .data block 681200..681E00 (ball shapes, kind / spark tables), the 2D draws
    // their controls submit (429530 / 4289E0 / 428AC0, executed by the HUD display) and an
    // own latch.
    static constexpr std::uint32_t ObjectWorkBase=0x5f000000u,ObjectWorkSlot=0x500u;
    std::vector<std::uint8_t> object_works=std::vector<std::uint8_t>(410u*ObjectWorkSlot);
    std::vector<std::uint8_t> data_681200;
    std::vector<RaceHudDraw> object_draws;
    std::array<std::uint8_t,0x18> cells_84d6c0{};   // AREA object lanes: 84D6C0[3] dynamic payloads, 84D6CC[3] placement handles
    std::array<std::uint8_t,0x28> record_head_84cea0{};   // 84CEA0..84CEC7: the first 4F0BD0 record (before the manager's 84CEC8 block)
    std::uint32_t object_inits{},object_controls{},object_displays{},object_dests{},object_skipped{},object_fault{},object_draw_faults{};
    std::string object_error;
    bool objects_latched{};
    // Heart Attack mission logic (464D20 and the NAVI display callbacks): latched on its own.
    std::uint32_t heart_runs{},heart_skipped{},heart_fault{};
    std::uint32_t score_runs{},score_skipped{},score_fault{};std::string score_error;bool score_latched{};   // 4F2DF0 / 4F2B20
    std::string heart_error;
    bool heart_latched{};
    NativeRaceTraffic();
    ~NativeRaceTraffic();
};
NativeRaceTraffic& native_race_traffic(NativeRuntimeContext&);
// Race manager hooks 47DAC0 / 47EC00 / 47DC00; false when `pc` is none of them.
bool native_race_traffic_hook(NativeRuntimeContext&,std::uint32_t pc,driving::PcMatrixStack&);
// Callbacks of the traffic car events (4AD280 / 47E780 / 470560); false when not one.
bool native_race_traffic_car_invoke(NativeRuntimeContext&,std::uint32_t callback,driving::PcMatrixStack&);
// GamePlCar init 4A6ED0 in variant 4: the LAN grid position of the player car (work `car`,
// written at +14 / +5C) through the translated 456E00 / 43F730.
bool native_race_network_grid_4a6ed0(NativeRuntimeContext&,driving::PcMatrixStack&,std::uint32_t car);
// One LAN race function of the translated set (45A2B0 rank, 457770 progress, the CommRace
// saves 456720 / 4568E0 / 4569D0 / 459C30 / 456790 on ECX = 7DE418...): false when the
// traffic module is latched or the call faulted (it latches then). `scratch` (when given)
// is mapped at NetworkCallScratch for the duration of the call (an in/out argument).
constexpr std::uint32_t NetworkCallScratch=0x0dff0000u;
// Event 0x190 (function 0x1F, variant 5): the request manager's callbacks 46C1F0 init / 46C200
// control / 46C210 display / 46C230 destroy; false for another callback.
bool native_race_requests_event(NativeRuntimeContext&,std::uint32_t callback,std::uint32_t work,driving::PcMatrixStack&);
// Maps the C2C request manager's block 7F9460 and its heap (before the words that shadow them).
void native_race_requests_map(NativeRuntimeContext&,PcRaceMemory&);
// The C2C request manager 7F9460 (variant 5): one of its entries; false when the traffic
// module is latched or the call faulted (latched then).
bool native_race_requests_call(NativeRuntimeContext&,driving::PcMatrixStack&,std::uint32_t pc,
                               std::initializer_list<std::uint32_t> args,std::uint32_t& eax,std::uint32_t ecx=0);
bool native_race_network_call(NativeRuntimeContext&,driving::PcMatrixStack&,std::uint32_t pc,
                              std::initializer_list<std::uint32_t> args,std::uint32_t& eax,
                              std::uint32_t ecx=0,std::uint32_t* scratch=nullptr);
// 476760 ranking update for the car work at PC address `car` (race manager
// control); false when the module is latched (then nothing ran).
bool native_race_traffic_ranking(NativeRuntimeContext&,std::uint32_t car,driving::PcMatrixStack&);
// 46F990 othcarCalcCsLenDiff(a, b) of two OnRoadPlace PC addresses (the car displays' LOD gap);
// false when a service it needs is not available (nothing latched).
bool native_race_traffic_cs_diff(NativeRuntimeContext&,driving::PcMatrixStack&,std::uint32_t a,std::uint32_t b,std::int32_t& out);
// Heart Attack routines of race_traffic (464D20 NAVI control child, display callbacks) on the
// traffic memory plus what the HUD owner maps through `map` (NAVI globals 7F1900 / 7F8980,
// the voice queue, the race clock); 4289E0 submissions go to `draws`. A fault latches the
// Heart Attack logic only (heart_fault / heart_error); false when it did not run.
class PcSceneRenderer;
struct NativeHeartBinding {
    void* user{};
    void (*map)(void* user,PcRaceMemory&){};
    std::vector<RaceHudDraw>* draws{};
    // A display run (462CD0): its 429530 / 4295D0 / 4BC990 submissions go to draws (replayed in
    // order by the NAVI display), its model leaves to models (executed with the renderer).
    bool display{};
    std::vector<PcVehicleDrawCall>* models{};
    PcSceneRenderer* renderer{};
};
bool native_race_traffic_heart(NativeRuntimeContext&,driving::PcMatrixStack&,std::uint32_t pc,NativeHeartBinding&);
// Race manager score control 4F2DF0 (per frame) and stage bonus 4F2B20(seconds) over the traffic
// memory with the HUD words (NAVI 842800 block, score block 84DEF8) and the miles word 84BCF8.
// False when latched (own latch, reported in the status line).
bool native_race_score(NativeRuntimeContext&,driving::PcMatrixStack&,std::uint32_t pc,std::uint32_t arg);
// Course object (OSO) event callbacks (inits / controls / 4AEF50 destroy): false when
// `callback` is none of them.
bool native_race_traffic_object_invoke(NativeRuntimeContext&,std::uint32_t callback,driving::PcMatrixStack&);
// Course object displays (4A8A90 model, 4A9030 shadow, 4A9DF0 animated, 4A8D80 lamp) on the
// renderer; false when `callback` is none of them.
class PcSceneRenderer;
bool native_race_objects_display(NativeRuntimeContext&,PcSceneRenderer&,std::uint32_t callback,std::uint32_t work);
// The event slot is a traffic / racer car (event function 0x55 callbacks).
bool native_traffic_car_slot(const driving::PcEventSlot&);
// Player car services run by the traffic module on the player's work (event 8): 479670.
bool native_race_traffic_car_service(NativeRuntimeContext&,std::uint32_t pc,std::uint32_t car,driving::PcMatrixStack&);
bool native_race_traffic_car_call(NativeRuntimeContext&,std::uint32_t pc,std::uint32_t car,driving::PcMatrixStack&,std::uint32_t& result,std::string& error);
// 4A4010 GetRoadOfs (traffic cars and the player car); `mem` maps the AREA block,
// gate_80fb14 selects 47B890 (lane classifier).
// False when 47B890's route width failed (the message is in `error`).
bool native_get_road_ofs_4a4010(NativeRuntimeContext&,PcRaceMemory& mem,driving::Bytes event,
                                driving::PcMatrixStack&,bool gate_80fb14,std::string& error);
std::string native_race_traffic_status(const NativeRuntimeContext&);
}
