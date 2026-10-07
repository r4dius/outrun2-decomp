#pragma once
#include "driving/pc_course_query.hpp"
#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace outrun::platform {

struct CourseCollisionQuad {
    std::uint32_t flags{};
    std::array<std::array<float,3>,4> vertices{};
    std::uint32_t material{};
    std::array<float,2> center_xz{};
};

// Offsets are relative to the COLI0200 header, AFTER its four-byte size
// prefix. The primary road polygon count is distinct from the total count.
struct CourseCollisionLayout {
    std::array<std::uint32_t,8> sections{};
    std::uint32_t polygon_count{};
    std::uint32_t primary_polygon_count{};
    std::uint32_t primary_length_count{};
    std::uint32_t referenced_area_lists{};
    bool primary_lengths_ordered{true};  // 0x780190 stays set (lane 0)
};

struct CourseCollisionPack {
    std::vector<CourseCollisionQuad> quads;
    // OR2COL2 retains the complete decoded PC COLI0200 tables. OR2COL1 is
    // accepted only for legacy host fixtures.
    std::vector<std::uint8_t> pc_coli0200;
    CourseCollisionLayout pc_layout{};
    // Runtime lookup derived from the monotone primary road length table.
    // Source section 6 is NOT this lookup and must never be bound as ranges.
    std::vector<std::uint8_t> primary_ranges;
    std::array<std::uint8_t,32> source_sha256{};
    std::uint32_t source_bytes{};
    std::uint32_t source_quad_offset{};
};

struct CourseGroundSample {
    float height{};
    std::array<float,3> normal{{0.0f,1.0f,0.0f}};
    std::uint32_t quad_index{};
    std::uint32_t surface_flags{};
};

constexpr std::size_t CourseCollisionHeaderSize=128u;
constexpr std::size_t CourseCollisionQuadStride=64u;

// Open the original inflated file directly (including its size prefix).
// Admission checks all grid-reachable lists, geometry, normals and primary
// run topology. Failure leaves the previous pack intact.
bool parse_pc_coli0200(const std::uint8_t* data,std::size_t size,
                       CourseCollisionPack& pack,std::string* error=nullptr);
// Short-lived, read-only views; no cached native/guest pointers survive a
// pack copy or move. Invalid/unopened packs throw before exposing any views.
driving::CourseCollisionTables course_collision_tables(
    const CourseCollisionPack& pack,std::uint32_t load_type=0u);
driving::Bytes course_collision_grid(const CourseCollisionPack& pack);

bool parse_course_collision_pack(const std::uint8_t* data,std::size_t size,
                                 CourseCollisionPack& pack,std::string* error=nullptr);
bool load_course_collision_pack_file(const char* path,CourseCollisionPack& pack,
                                     std::string* error=nullptr);
bool course_collision_ground_at(const CourseCollisionPack& pack,float x,float z,
                                float reference_height,CourseGroundSample& sample);

} // namespace outrun::platform
