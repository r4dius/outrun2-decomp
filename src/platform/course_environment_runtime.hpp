#pragma once
#include "driving/pc_course_environment.hpp"
#include "driving/pc_environment_blend.hpp"
#include "platform/course_world_runtime.hpp"
#include <array>
#include <string>
#include <vector>

namespace outrun::platform {
// Retained PC globals consumed by the per-frame environment update. The flags
// and phase stay with the START owner (0x44AA80 writes them there).
struct CourseEnvironmentLiveState {
    std::int16_t time_7d2934{};
    float duration_7d28d8{};
    std::array<std::uint8_t,0x1e0> saved_sun_7d26d0{};
    std::array<std::uint8_t,0x54> saved_fog_7d28e0{};
    std::array<std::uint8_t,0x54> fog_7d3a10{};
    std::array<std::uint8_t,9*0xa0> lights_899b98{}; // 3 sun + 6 local (899D78)
    std::array<std::uint8_t,24> nearest_7d2d58{};
};
// Mutable original environment records owned independently of transport bytes.
// initialize() always starts from pristine source, preventing repeated transforms.
class CourseEnvironmentRuntime {
public:
    CourseEnvironmentRuntime()=default;
    CourseEnvironmentRuntime(const CourseEnvironmentRuntime&)=default;
    CourseEnvironmentRuntime& operator=(const CourseEnvironmentRuntime&)=default;
    CourseEnvironmentRuntime(CourseEnvironmentRuntime&& other) noexcept;
    CourseEnvironmentRuntime& operator=(CourseEnvironmentRuntime&& other) noexcept;
    bool admit_lane(std::uint32_t lane,const std::uint8_t* raw,std::size_t size,
                    CourseWorldSourceIdentity identity,std::string* error=nullptr);
    bool admit_absent_lane(std::uint32_t lane,std::string* error=nullptr);
    bool initialize(const std::array<std::uint8_t,64>& primary_matrix,
                    std::string* error=nullptr);
    void reset();
    bool inputs_ready() const;
    bool initialized() const {return initialized_;}
    bool lane_admitted(std::uint32_t lane) const;
    const std::vector<std::uint8_t>& payload(std::uint32_t lane) const;
    const CourseWorldSourceIdentity& source_identity(std::uint32_t lane) const;
    const driving::PcEnvironmentLayout& layout() const {return layout_;}
    // 44A8DF sequence (4517D0, 449F50, 44A000) over the initialized records.
    // vehicle is the shared PC car view, camera the 79F574 view. Road runs are
    // resolved against the world's primary collision type and matrix 7D2DA0.
    // Inputs are validated before any retained state is touched.
    bool update_frame(driving::Bytes vehicle,driving::Bytes camera,
                      const CourseWorldRuntime& world,
                      const driving::PcEnvironmentTransition& transition,
                      std::array<std::uint32_t,6>& flags_7d28b0,
                      std::uint32_t& phase_7d28c8,CourseEnvironmentLiveState& live,
                      std::string* error=nullptr);
private:
    std::array<std::vector<std::uint8_t>,3> source_{},runtime_{};
    std::array<CourseWorldSourceIdentity,3> identities_{};
    std::array<bool,3> admitted_{};
    driving::PcEnvironmentLayout layout_{};
    bool initialized_{};
};
} // namespace outrun::platform
