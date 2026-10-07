#pragma once
#include "driving/pc_road_mu.hpp"
#include "driving/pc_wheel_dynamics.hpp"

namespace outrun::driving {

// Explicit platform/data dependencies that the PC DrivingControl reached via
// globals, DirectInput helpers, and the PC road service.  Keeping them explicit
// is the boundary the Switch frontend/backend must implement.
struct DrivingControlInputs {
    std::int32_t analog_channel_1{};
    bool input_inhibited{};
    bool shift_up{};
    bool shift_down{};
    RoadMuSource road_mu{};
    RunningResistanceTuning running_resistance{};
    float reaction_blend_parameter{}; // PC car parameters +0x20A8
};

// Complete recovered PC DrivingControl 0x00502C90 call order.  All game-side
// arithmetic/cases are native; only platform/service inputs above are injected.
void driving_control(Bytes event, Bytes work, Bytes parameters,
                     const Tables& tables, const DrivingControlInputs& inputs);

} // namespace outrun::driving
