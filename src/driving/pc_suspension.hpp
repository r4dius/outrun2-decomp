#pragma once
#include "driving/pc_driving.hpp"
namespace outrun::driving {
struct SuspensionTuning {
    float press_down = 5.0f;                 // PC 0x628138 / assGetPressDown
    float velocity_scale = 60.20000076293945f; // PC 0x628110
    float travel_inv = 6.666666507720947f;  // PC 0x5c36c0
    float damping_shape = 0.4000000059604645f; // PC 0x5b0058
    float damping_gain = 19228.98828125f;   // PC 0x5c36bc
    float rebound_scale = 0.7368475794792175f; // PC 0x5c36b8
    float force_scale = 0.10197161883115768f;  // PC 0x5c36b4
};

// PC 0x518110, 0x518120, 0x518240.
float ass_get_press_down(const SuspensionTuning& = {});
void ass_specific_amount_of_tire_load(Bytes work);
void ass_diagonal_tire_load(Bytes work, Bytes parameters);

// PC 0x4A1B70 and 0x4A1A90.
void suspension_force(Bytes work, Bytes parameters, const SuspensionTuning& = {});
void tire_load(Bytes event, Bytes work, Bytes parameters, const SuspensionTuning& = {});
// Explicit stack form also performs the original initial mxLoadMatrix.
struct PcMatrixStack;
void tire_load(Bytes event,Bytes work,Bytes parameters,const SuspensionTuning&,PcMatrixStack&);
}
