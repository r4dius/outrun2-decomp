#pragma once
#include "driving/pc_course_world.hpp"
namespace outrun::driving {
// PC 0x43D920: selects the already reconstructed primary/secondary run lookup.
// selector holds {u32 type, i32 mode, i16 length}. Outputs stay unchanged on a
// primary no-result. This adapter follows no serialized guest pointers.
std::int32_t find_selected_course_run(const CourseWorldTables&,Bytes selector,
    std::int32_t hint,std::int32_t& first,std::int32_t& last);
struct GroundCollisionContext {
    CourseWorldQuery& world;
    // PC 0x44BEF0 -> +4: 0x7D3128 (type zero), 0x7D317C (all nonzero types).
    std::array<float,2> area_yaw_radians{};
};
// PC CalcGroundColiFace 0x519500. The caller's CURRENT matrix, not merely
// work+0x10, supplies center/point transforms. Queries execute reconstructed
// circuit code. A missing hit is the original no-hit case, not a flat floor.
// Fixed views are checked before entry; a later malformed circuit view throws
// and earlier completed query effects are retained (not an atomic frame).
void calc_ground_coli_face(Bytes event,Bytes work,Bytes parameters,
    const std::array<Bytes,4>& wheels,GroundCollisionContext&);
} // namespace outrun::driving
