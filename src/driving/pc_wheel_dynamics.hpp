#pragma once
#include "driving/pc_driving.hpp"

namespace outrun::driving {
using WheelViews = std::array<Bytes, 4>;
// PC wheel records are 0xf4 bytes. These views never dereference guest pointers.
WheelViews embedded_wheels(Bytes work);

// Exact wheel-angle accumulation used inline by the original front/rear wheel
// routines. This is exposed for native presentation bridges that already own
// a validated angular velocity; it is not counted as a separate PC routine.
void advance_wheel_angle_inline(Bytes wheel, float angular_velocity);

// Closed original PC routines (addresses and exact input hash in the profile).
void cornering_power(Bytes event, Bytes parameters, const WheelViews& wheels);
void side_force(const WheelViews& wheels);
void front_driving_force(Bytes parameters, const WheelViews& wheels);
void rear_driving_force(Bytes event, Bytes work, Bytes parameters);
void friction_circle(Bytes event, Bytes parameters, const WheelViews& wheels);
void resolve_wheel_forces(const WheelViews& wheels); // PC 0x501ad0, name descriptive
void front_wheel_rotation(Bytes parameters, const WheelViews& wheels);
void rear_wheel_rotation(Bytes event, Bytes work, Bytes parameters);
void rolling_resistance(Bytes event, Bytes parameters, const WheelViews& wheels);
void slip_ratio(Bytes parameters, const WheelViews& wheels);

// Inline block in DrivingControl, not a separate original function.
void distribute_brake_torque(Bytes event, Bytes work, Bytes parameters, const Tables& tables);

// A deliberately bounded stage, NOT the complete DrivingControl function.
// Requires already-computed contact loads, surface coefficients, wheel speeds
// and directions. Does not invent road contacts, transmission selection,
// chassis displacement or collisions. Existing wheel/engine state is retained.
void drivetrain_step_from_contacts(Bytes event, Bytes work, Bytes parameters, const Tables& tables);
struct RunningResistanceTuning {
    float speed_square_scale;
    float no_state_factor;
    float state_factor;
    float state_alt_factor;
    float event_surface_angle_scale;
    float wheel_surface_angle_scale;
};
// PC 0x502120. The original reads six mutable globals at 0x6ae4e4..f8;
// they are explicit inputs here rather than hidden PC globals.
void running_resistance(Bytes event, Bytes work, const RunningResistanceTuning& tuning);

// PC 0x502270 = CopyPhysicalWork, identified from the Lindbergh symbols/call order.
// reaction_blend_parameter is the PC car-parameter field at +0x20a8, kept
// explicit until the native car-parameter view is expanded beyond 0x2000.
void copy_physical_work(Bytes event, Bytes work, float reaction_blend_parameter);

// Ordered native subset from AccelOperation through the original end of
// DrivingControl, with GetRoadMu/contact inputs and transmission selection still
// supplied externally. This is deliberately NOT named DrivingControl.
void known_driving_tail_from_contacts(Bytes event, Bytes work, Bytes parameters,
                                      const Tables& tables,
                                      const RunningResistanceTuning& tuning,
                                      float reaction_blend_parameter);

} // namespace outrun::driving
