#pragma once
#include <array>
#include <cstdint>

namespace outrun::platform {

struct VehiclePoseFilterState {
    std::array<float,3> normal{{0.0f,1.0f,0.0f}};
    float ground_height{};
    float max_target_angle{};
    float max_applied_angle{};
    float max_applied_height{};
    std::uint32_t updates{};
    std::uint32_t normal_limited{};
    std::uint32_t height_limited{};
    bool initialized{};
};

bool initialize_vehicle_pose_filter(VehiclePoseFilterState& state,
                                    float ground_height,
                                    const std::array<float,3>& normal);
bool update_vehicle_pose_filter(VehiclePoseFilterState& state,
                                float target_ground_height,
                                const std::array<float,3>& target_normal);

} // namespace outrun::platform
