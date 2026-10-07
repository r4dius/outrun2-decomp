#pragma once
// Game modes 1/2/3/4/5 (the boot logos and the arcade demo route): their init,
// control and exit callbacks, and the logos they open (event 3 functions 0x2E /
// 0x2F / 0x30 / 0x31 / 0x49), over the race-end runtime's memory and services.
#include "platform/race_area.hpp"
#include <cstdint>
namespace outrun::platform {
// True when `callback` is one of the modes' callbacks (and it ran).
bool race_modes_callback(PcRaceContext&,std::uint32_t callback);
// The same for the logo callbacks; `work` is the event's work (the fade value).
bool race_logos_callback(PcRaceContext&,std::uint32_t callback,std::uint32_t work);
}
