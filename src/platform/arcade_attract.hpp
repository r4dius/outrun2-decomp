#pragma once
// OUTRUN2SP arcade frontend (OR2006C2C.EXE 4BEE60..4C4880): the event-4 functions 1..0xC of the
// attract / arcade selection screens, transliterated over PC addresses like race_end_modes.
// Blocks they own: the event-4 work 780440, 844928..844A7C, 84A208..84A330 (with the 446xxx
// embedded slots object 84A320), 84A9AC..84AA00, and the .data tables 6898C0..68A000 and
// 65A7A0..65A7C0 (PcRaceEndState).
#include "platform/race_area.hpp"
#include <cstdint>
namespace outrun::platform {
// One event-4 callback (init / control / display / destroy of functions 1..0xC); false when
// the callback is not one of them. w is the event's work (780440).
bool arcade_event4_invoke(PcRaceContext&,std::uint32_t callback,std::uint32_t w);
// The entry module services 4B6EE0 / 4B88D0 / 4B89B0 / 4B89E0 / 4B8C00 (eax result); false for
// any other PC.
bool arcade_entry_invoke(PcRaceContext&,const PcRaceCall&,std::uint32_t& eax);
// 4BFA20: the layer-8 callback (84A318 = 2 while the car-select car is shown, else 0).
void arcade_layer8_4bfa20(PcRaceContext&);
}
