#pragma once
#include "vehicle_constructor.hpp"
#include "driving/pc_course_world.hpp"
namespace outrun::platform {
// 4A69F0's protected preamble clears exactly 0x900 bytes, including the tail
// beyond the fields used by the earlier 0x800 arithmetic-only view.
inline constexpr std::size_t PcVehicleBodyBytes=0x900;
void vehicle_rigid_body_516e10(driving::Bytes work,driving::PcMatrixStack&);
void vehicle_wheel_positions_4a1e80(driving::Bytes work,driving::Bytes parameters);
void vehicle_body_corners_4a1fc0(driving::Bytes work);
void vehicle_body_parameters_4a6840(driving::Bytes work,driving::Bytes parameters);
// Complete shared physical setup; real world/grid query, not a ground callback.
// work_address is the semantic PC base for embedded wheel IDs, not a host pointer.
void vehicle_body_init_4a69f0(driving::Bytes event,driving::Bytes work,
    driving::Bytes parameters,std::uint32_t work_address,driving::CourseWorldQuery&);
// Original three-probe ground setup. Area yaw is from the shared primary and
// secondary area records (7D3128/7D317C), not reconstructed from the car angle.
void vehicle_ground_init_519300(driving::Bytes event,driving::Bytes work,
    driving::CourseWorldQuery&,const std::array<float,2>& area_yaw);
// Uses the original model-specific 520950 descriptor, not render-mesh bounds.
void vehicle_collision_init_4f6e40(driving::Bytes event);
// The same over the 520950 descriptor of any model ([5E8A88 + model*0x3C], e.g. the traffic models).
void vehicle_collision_init_4f6e40(driving::Bytes event,driving::Bytes descriptor);
}
