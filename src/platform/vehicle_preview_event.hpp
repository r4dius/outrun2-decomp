#pragma once
#include "vehicle_constructor.hpp"
#include "driving/pc_wrecker.hpp"
namespace outrun::platform {
// Caller-owned shared globals. No independent preview yaw, random stream or
// ghost history; the native event owner must provide the original shared state.
struct VehiclePreviewControl {
    driving::CourseProbe position_680c04{};
    std::uint16_t& yaw_841fa0;
    std::uint32_t& random_state;
    driving::Bytes history_83db30;
    driving::PcMatrixStack& matrices;
    std::int32_t interpolation_override_82e7d8{};
    std::uint8_t owner_flags_79fb4e{};
    driving::Bytes owner_799ca0;
    float frame_blend_634b34{};
    std::uint8_t scene_82e7d4{};
    std::int32_t game_mode_78026c{};
    std::int32_t steering_override_82e7ec{};
    std::int16_t steering_82e7cc{};
};
void vehicle_preview_control_4a5b20(driving::Bytes event,VehiclePreviewControl&);
}
