#pragma once
#include "driving/pc_course_world.hpp"
namespace outrun::driving {
// Reconstructed PC collision geometry; not the complete CbwColiWall response.
// All views are native, bounds checked. No serialized guest pointer is followed.
// Four output vectors may overlap the selected polygon (the PC first snapshots
// its 48 vertex bytes); outputs must not alias the matrix stack/transform.
void cop_coli_point(const CourseWorldTables&,std::uint32_t polygon,std::uint32_t type,
                    PcMatrixStack&,const std::array<Bytes,4>& outputs); // 0x43D1D0
// The four mutable arguments are intentionally not an immutable polygon:
// PC can overwrite a/d before writing b=normal and c=offset point.
// Their read/write order is preserved, including overlapping vector views.
void calc_coli_wall_face(float x,float z,const std::array<Bytes,4>& points); // 0x502E20
struct WallPushResult {std::uint32_t mask;float distance;};
// FACE_WORK: normal[3], offset_plane_point[3], boundary_plane_point[3] (36 bytes).
// COLI_POINT: signed count then xyz[count]; original count clamps to 1..32.
// Uses the current matrix to probe the shape, but moves ONLY work+0x40..48.
// The matrix translation is deliberately NOT synchronized with that movement.
WallPushResult push_outpos_mat_obsolete(Bytes work,Bytes face,Bytes shape,
                                        const PcMatrixStack&); // 0x502F80
// Weighted local contact position. Original PC uses 12 local hit slots despite
// accepting a count up to 32. >12 selected negative projections are rejected
// by this native interface rather than reproducing original stack corruption.
// On well-formed inputs, Push/rotation-only Load/Pop effects are retained.
// Result must be disjoint from the input views and the matrix stack.
bool coli_set_obsolete(Bytes work,Bytes face,Bytes shape,std::uint32_t mask,
                       PcMatrixStack&,Bytes result); // 0x5030F0
}
