#pragma once
#include "platform/course_collision_pack.hpp"
#include "driving/pc_course_world.hpp"
#include <array>
#include <cstddef>
#include <cstdint>
#include <string>

namespace outrun::platform {

// Guest addresses are identifiers only. They are never dereferenced as host
// pointers, and the owner's byte storage outlives all per-call native views.
struct CourseWorldSourceIdentity {
    std::uint32_t descriptor_index{};
    std::uint32_t descriptor_token{};
    std::uint32_t path_token{};
    std::uint32_t payload_crc32{};
};
struct CourseWorldGroundSample {
    CourseGroundSample ground{};
    std::uint32_t lane{};
    std::uint32_t material{};
};

// Owns the four original collision roots and the two area transforms. Loading
// bytes alone does NOT establish the original transform/scene initializer.
// Source-local diagnostics explicitly install identity; START binds the two
// matrices produced by 0x44C0D0 for its verified selected course instead.
// This is single-threaded runtime state, not a shared concurrent query cache.
class CourseWorldRuntime {
public:
    CourseWorldRuntime()=default;
    CourseWorldRuntime(const CourseWorldRuntime&)=default;
    CourseWorldRuntime& operator=(const CourseWorldRuntime&)=default;
    CourseWorldRuntime(CourseWorldRuntime&& other) noexcept;
    CourseWorldRuntime& operator=(CourseWorldRuntime&& other) noexcept;

    bool admit_lane(std::uint32_t lane,const std::uint8_t* data,std::size_t size,
                    CourseWorldSourceIdentity identity,std::string* error=nullptr);
    bool set_transform(std::uint32_t group,const std::array<float,16>& matrix,
                       std::string* error=nullptr);
    void reset();
    // PC 0x43DB00 (via 0x43DE50): drops one collision root; the transform
    // groups (7D2DA0/7D3190, AREA-owned words) are kept (course-prog).
    void release_lane(std::uint32_t lane);
    bool lane_loaded(std::uint32_t lane) const;
    bool query_ready() const;
    const CourseCollisionPack& lane(std::uint32_t lane) const;
    const CourseWorldSourceIdentity& source_identity(std::uint32_t lane) const;
    std::uint64_t generation() const { return generation_; }
    const driving::EasyLctPredictionState& prediction() const { return prediction_; }
    // Views expire on admission, transform replacement, reset or destruction.
    // They are rebuilt against this owner's storage after every copy/move.
    driving::CourseWorldTables tables() const;
    // No lane admitted (frontend modes before a course loads): the EXE queries an empty world.
    driving::CourseWorldTables tables_or_empty() const {
        if(query_ready())return tables();
        driving::CourseWorldTables t{};
        for(std::uint32_t i=0;i<t.courses.size();++i){t.courses[i].load_type=i;t.courses[i].polygons_present=false;}
        return t;
    }
    bool ground_at(float x,float z,float reference_height,CourseWorldGroundSample& sample);
    // 780208[lane]: the lanes' course length offsets (43DBA0 tail: lane 0 after lane 1's last
    // record, lane 1 (and 780210) after lane 0's; 43DB00 zeroes the released lane's).
    std::int32_t length_offset(std::uint32_t lane) const {return lane<4u?length_offsets_[lane]:0;}
private:
    std::array<std::int32_t,4> length_offsets_{};
    std::array<CourseCollisionPack,4> lanes_{};
    std::array<CourseWorldSourceIdentity,4> identities_{};
    std::array<bool,4> loaded_{};
    std::array<std::array<std::uint8_t,64>,2> transforms_{};
    std::array<bool,2> transform_known_{};
    driving::EasyLctPredictionState prediction_{};
    std::uint64_t generation_{};
};

std::array<float,16> course_world_identity_transform();
} // namespace outrun::platform
