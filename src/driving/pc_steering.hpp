#pragma once
#include "driving/pc_wheel_dynamics.hpp"

namespace outrun::driving {
// PC 0x5003c0, called with ESI=event and EAX=work from DrivingControl.
// Writes the steering angle at event+0x32. Parameters are supplied explicitly;
// the guest pointer at event+0x2b4 is never dereferenced by native code.
void steering_operation(Bytes event, Bytes work, Bytes parameters);

// PC 0x500580. The original queries analog channel 1 through GetVolume(1).
// Native code receives that mutable platform input explicitly so the future
// Switch input layer can provide it without depending on PC globals.
void toe_angle(Bytes event, Bytes work, Bytes parameters, std::int32_t analog_channel_1);

// Reconstructed closed prefix of PC CalcTireDirection (0x500700..0x500873).
// Converts toe/steering radians into the original 16-bit angle units, derives
// wheel slip-direction deltas, and updates event front/rear averages. The
// remaining D3DX-based world-direction portion of CalcTireDirection is not
// included here.
void tire_direction_angles(Bytes event, Bytes work);

// Ordered pre-force steering stage only. This does NOT compute tire velocity
// or tire direction and is not a complete DrivingControl replacement.
void steering_stage_from_inputs(Bytes event, Bytes work, Bytes parameters, std::int32_t analog_channel_1);
}
