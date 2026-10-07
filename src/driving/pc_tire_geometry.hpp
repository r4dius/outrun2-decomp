#pragma once
#include "driving/pc_wheel_dynamics.hpp"

namespace outrun::driving {
// PC CalcTireVelocity at 0x00500230. Reconstructs the four per-wheel
// contact-plane velocity directions, lateral axes, speed magnitudes and
// velocity angles. Wheel storage is supplied explicitly through work's
// embedded wheel views; no 32-bit guest pointer is dereferenced by native code.
void tire_velocity(Bytes work);
// PC CalcTireDirection 0x500700, including the D3DX vector half.
void tire_direction(Bytes event, Bytes work);
}
