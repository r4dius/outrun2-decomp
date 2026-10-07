#pragma once
// PC race SKY event (391): init 451A30, control 451B40, display 4521C0
// (452080 = DrawSkyObject), and the two sky transition helpers the AREA
// control calls: 451DD0 (reset for a new area) and 451E90 (cross-fade step,
// its protected entry bridge measured as EAX = [7D33B4]).
// Same conventions as race_area.hpp: globals by PC address in a
// PcRaceMemory, other modules through the service, render leaves (4044F0,
// 4056D0, 404540, device SetRenderState) in the draw list.
// The sky work W (event work, 0x120 bytes: two 0x90-byte layers) is mapped by
// its owner at its PC address; [79F6DC] holds it for 451DD0/451E90.
#include "platform/race_area.hpp"
namespace outrun::platform {
// Sky module globals: 7D3A64 transition state and the .data words
// 638664..638674 (638664 step count word, 638668 default layer vector).
struct PcRaceSkyState {
    std::array<std::uint8_t,4> state_7d3a64{};
    std::vector<std::uint8_t> data;
    PcRaceSkyState();  // data = EXE image
    void map(PcRaceMemory& m){m.map(0x7d3a64u,state_7d3a64.data(),4);m.map(0x638664u,data.data(),data.size());}
};
extern std::uint8_t RaceSkyDataImage[0x10];   // EXE .data 638664..638674
void race_sky_init_451a30(PcRaceContext&,std::uint32_t work);
// Throws std::logic_error when 7D3188/7D31DC is null: 44C420 then leaves two
// of the words 451B40 copies unwritten (PC stack contents).
void race_sky_control_451b40(PcRaceContext&,std::uint32_t work);
void race_sky_display_4521c0(PcRaceContext&,std::uint32_t work);
void race_sky_object_452080(PcRaceContext&,std::uint32_t object,float alpha);
void race_sky_reset_451dd0(PcRaceContext&);
std::uint32_t race_sky_step_451e90(PcRaceContext&);
}
