#pragma once
#include "platform/course_collision_pack.hpp"
#include "platform/course_world_runtime.hpp"
#include <array>
#include <cstdint>

namespace outrun::platform {

struct VehicleWheelRoadContact {
    bool supported{};
    std::array<float,3> world_position{};
    std::array<float,3> normal{{0.0f,1.0f,0.0f}};
    std::uint32_t quad_index{};
    std::uint32_t course_lane{};
    std::uint32_t material{};
    float suspension_delta{};
};

struct VehicleRoadContactFrame {
    std::array<VehicleWheelRoadContact,4> wheels{};
    std::array<float,3> normal{{0.0f,1.0f,0.0f}};
    float ground_height{};
    std::uint32_t hit_mask{};
    std::uint32_t hit_count{};
    std::uint32_t primary_quad{};
    std::uint32_t primary_lane{};
    bool supported{};
};

enum class VehicleRoadSweepFailure { None, InvalidInput, BudgetExceeded, Unsupported };
constexpr std::uint32_t VehicleRoadSweepMaximumSegments=256u;

struct VehicleRoadSweepStats {
    std::uint32_t sampled_poses{};
    std::uint32_t wheel_queries{};
    std::uint32_t wheel_hits{};
    VehicleRoadSweepFailure failure{VehicleRoadSweepFailure::None};
};

// Queries all four real wheel footprints.  Three contacts are sufficient to
// retain a supported chassis plane; individual wheel residuals drive the
// platform-to-PC suspension boundary without flattening sloped road geometry.
bool sample_vehicle_road_contacts(const CourseCollisionPack& pack,
                                  const std::array<float,3>& body_position,
                                  float yaw,
                                  VehicleRoadContactFrame& frame);

// Sampled (not continuous) four-wheel collision, including both endpoints.
// The step bounds translation plus the outer wheel's yaw arc. Too much motion
// is rejected before any float-to-integer conversion, never coarsened to fit
// the query budget. Gaps smaller than maximum_step are not guaranteed detected.
bool sample_vehicle_road_contacts_swept(
    const CourseCollisionPack& pack,
    const std::array<float,3>& from_position,float from_yaw,
    const std::array<float,3>& to_position,float to_yaw,
    VehicleRoadContactFrame& frame,VehicleRoadSweepStats& stats,
    float maximum_step=0.20f);

// Live-world overloads share the exact footprint/sweep implementation and
// retain the native query's prediction state between wheel contacts.
bool sample_vehicle_road_contacts(CourseWorldRuntime& world,
    const std::array<float,3>& body_position,float yaw,VehicleRoadContactFrame& frame);
bool sample_vehicle_road_contacts_swept(CourseWorldRuntime& world,
    const std::array<float,3>& from_position,float from_yaw,
    const std::array<float,3>& to_position,float to_yaw,
    VehicleRoadContactFrame& frame,VehicleRoadSweepStats& stats,float maximum_step=0.20f);

} // namespace outrun::platform
