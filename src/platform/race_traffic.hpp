#pragma once
// PC rival racers / traffic (the "othcar" module, Lindbergh OthCar*):
//   race manager hooks 47DAC0 init, 47EC00 control, 47DC00 destroy;
//   spawning 47D8B0 (477510 ranks, 477C90 mission order, 47C8A0 / 47CA40
//   appear checks -> 47BC90 opens events of function 0x55 (car work 0x10F0)),
//   car callbacks 4AD280 init, 47E780 control, 470560 destroy (display
//   4AE5F0 / shadow 4ADAC0 are shared with the Time Attack ghosts).
// Like race_area / race_robots the port is a transliteration over PC
// addresses: every global it reads or writes is addressed by its PC address
// in a PcRaceMemory. The module .bss 800A00..80FB50 (racer table pointers
// and counters 80FB00.. are owned by RacerSetupState and mapped over it by
// the runtime) and the course appear tables 84BD00..84CE00 live in
// PcTrafficState; car works, the player car, event flags and the EXE
// tables are mapped by their owners. Every PC function outside the module
// is one service call (PcRaceCall); x87-returning services return the float
// bits in EAX.
#include "platform/race_area.hpp"
#include <cstdint>
#include <vector>
namespace outrun::platform {
// Scratch stack for the module's locals passed by address (the PC passes
// stack addresses to its services); mapped by the caller with the state.
constexpr std::uint32_t TrafficStackBase=0x7ffc0000u,TrafficStackSize=0x4000u;
struct PcTrafficState {
    static constexpr std::uint32_t BlockBase=0x800a00u,BlockEnd=0x80fb00u;   // 80FB00.. = RacerSetupState
    static constexpr std::uint32_t AppearBase=0x84bd00u,AppearEnd=0x84ce80u;   // 4EF890 copies of 6A5DF8..
    static constexpr std::uint32_t TailBase=0x80fb30u,TailEnd=0x80fb50u;       // 80FB48 (racers .bss tail)
    std::vector<std::uint8_t> block=std::vector<std::uint8_t>(BlockEnd-BlockBase);
    std::vector<std::uint8_t> appear=std::vector<std::uint8_t>(AppearEnd-AppearBase);
    std::vector<std::uint8_t> stack=std::vector<std::uint8_t>(TrafficStackSize);
    void map(PcRaceMemory& m){m.map(BlockBase,block.data(),block.size());m.map(AppearBase,appear.data(),appear.size());
        m.map(TrafficStackBase,stack.data(),stack.size());}
};
// Race manager hooks.
void traffic_init_47dac0(PcRaceContext&);
void traffic_control_47ec00(PcRaceContext&);
// Car event callbacks (event function 0x55): 4AD280 init, 47E780 control.
void traffic_car_init_4ad280(PcRaceContext&,std::uint32_t car);
// The car init callbacks of the event functions 0x55 (4AD280), 0x56 (4AD310), 0x59 (4704A0).
void traffic_car_init(PcRaceContext&,std::uint32_t callback,std::uint32_t car);
void traffic_car_control_47e780(PcRaceContext&,std::uint32_t car);
// 4A6CF0(car) (x87 result, as float bits): its course position with the fraction across the road quad.
std::uint32_t traffic_progress_4a6cf0(PcRaceContext&,std::uint32_t car);
// 4EFB90(place) / 4F0030(place, record): the appear / speed records of a course place (84CAB4 / 84C6F8).
std::uint32_t traffic_appear_rec_4efb90(PcRaceContext&,std::uint32_t place);
std::uint32_t traffic_speed_rec_4f0030(PcRaceContext&,std::uint32_t place,std::uint32_t rec);
// Parts exposed for the probe.
void traffic_spawn_47d8b0(PcRaceContext&);
// 4B0330 (variant 9): the open traffic car works of events [680AD4]+8..31 marked (476740).
void traffic_mark_4b0330(PcRaceContext&);
void traffic_ranks_477510(PcRaceContext&);
void traffic_order_477c90(PcRaceContext&);
void traffic_appear_47c8a0(PcRaceContext&);
void traffic_tables_4ef890(PcRaceContext&);
// One part of 47EC00 by its PC address (oracle probe): 47EBA0 4791B0 479500 479580 47CA40 477E00, 478B90(a0) 4787E0(a0, a1).
// a0 = the car, a1 = appear record, a2 = speed record (or the parts' own arguments).
std::uint32_t traffic_control_part(PcRaceContext&,std::uint32_t pc,std::uint32_t a0,std::uint32_t a1,std::uint32_t a2=0);
// The LAN race CommRace methods (ECX = 7DE418: 456720 / 456790 / 4568E0 / 4569D0 / 459C30) and
// the rank 45A2B0, native; false for any other pc.
bool traffic_network_call(PcRaceContext&,std::uint32_t pc,std::uint32_t ecx,const std::uint32_t* args,std::size_t n,std::uint32_t& eax);
// Course object (OSO) event callbacks (event functions 0x3D..0x42, 0x4A..0x4F, 0x5C..0x65 and
// the controls they switch to); false when `callback` is none of them. Displays record
// their draw leaves in the context draw list.
bool traffic_object_callback(PcRaceContext&,std::uint32_t callback,std::uint32_t work);
// Oracle access to the object module's internal routines (rigid body, contacts, 4A92E0, 43F3D0).
std::uint32_t traffic_object_debug(PcRaceContext&,std::uint32_t pc,std::uint32_t a0,std::uint32_t a1,std::uint32_t a2,std::uint32_t a3,std::uint32_t a4);
// Generic CRT qsort 580CB0 (the comparator is the module function at `compare`).
void traffic_qsort_580cb0(PcRaceContext&,std::uint32_t base,std::uint32_t num,std::uint32_t width,std::uint32_t compare);
}
