#pragma once
#include "driving/pc_driving.hpp"
namespace outrun::driving {
// PC AutoTransmission 0x502420. The retail PC function hides only its signed
// pedal comparison behind the protection section; the remainder is normal
// .text. Parameters are explicit instead of dereferencing event+0x2b4.
void auto_transmission(Bytes event, Bytes parameters);

// PC ManuTransmission 0x5024c0. The two PC input queries and the one global
// inhibit query are explicit platform inputs for the future Switch frontend.
void manual_transmission(Bytes event, Bytes work, Bytes parameters,
                         bool input_inhibited, bool shift_up, bool shift_down);
}
