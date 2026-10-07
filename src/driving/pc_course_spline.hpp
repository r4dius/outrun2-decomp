#pragma once
// PC C2C course spline math, not Lindbergh structure layouts.
#include "driving/pc_driving.hpp"
#include "driving/pc_x87.hpp"
#include <array>
namespace outrun::driving {
struct CourseVec3 { float x{}, y{}, z{}; };
static_assert(sizeof(CourseVec3)==12, "PC synthetic vector fixture requires 3 binary32 fields");
using CourseQuad = std::array<CourseVec3,4>;
using CourseTangents = std::array<CourseVec3,8>;
struct CourseSplineTuning {
    float lateral=1.0f;       // PC 0x0067D94C
    float longitudinal=1.0f;  // PC 0x0067D950
};
// Explicit native references. None of these are serialized x86 pointers.
// r015 integration audit: legacy forward/back labels are ABI-slot labels.
// At the GetRoadCond boundary slot 'forward' receives ColiGetBack, and slot
// 'back' receives ColiGetForward. Do not pass physical query results by name.
// See pc_course_query.cpp's explicit adapter; existing r014 math is unchanged.
struct CourseSplineNeighbors {
    const CourseQuad* forward{};
    const CourseQuad* back{};
    const CourseQuad* left{};
    const CourseQuad* right{};
};
CourseQuad course_quad_vertices(Bytes pc_record);
// Extended return models an x87 ST0 value until the caller stores it.
long double course_vec3_length_squared(const CourseVec3& v); // 0x0040F110
long double course_vec3_distance(const CourseVec3& a,const CourseVec3& b); // 0x0040F140
// Same values as x87 registers (no long double; used by the ported callers).
X87 course_vec3_length_squared_x87(const CourseVec3& v);
X87 course_vec3_distance_x87(const CourseVec3& a,const CourseVec3& b);
void calc_normal_to_delta(const CourseVec3& a,const CourseVec3& b,
                          const CourseVec3& na,const CourseVec3& nb,float scale,
                          CourseVec3& da,CourseVec3& db); // 0x00494B90
void calc_hermite_direction_vector2(CourseQuad& lateral,CourseQuad& longitudinal,
                                    const CourseQuad& p,const CourseSplineNeighbors& n); // 0x00494A70
void calc_hermite_tangent(CourseTangents& out,const CourseQuad& p,const CourseQuad& normals,
                          CourseSplineTuning tuning={}); // 0x00495340
// Original normalize helper leaves a destination untouched for length <= 1e-4.
// In/out storage preserves that behavior. Return mask records vectors written;
// this mask is additional native diagnostics, not an original return value.
std::uint8_t calc_hermite_tangent2(CourseTangents& out,const CourseQuad& p,
                                  const CourseSplineNeighbors& n,CourseSplineTuning tuning={}); // 0x004951D0
std::array<float,3> calc_hermite_coefficients(const CourseVec3& a,const CourseVec3& b,
                                           const CourseVec3& ta,const CourseVec3& tb); // 0x00494E20
float calc_carry_variable(float value,const std::array<float,3>& coefficients); // 0x00494D30
void solve_hermite(CourseVec3& out,const CourseVec3& point,float u,float v,
                   const CourseQuad& p,const CourseTangents& t); // 0x00494EE0
// These use original math; no query callback or emulated flat-ground fallback.
// Degenerate neighbor tangents that leave original stack data undefined cause
// an explicit domain_error instead of inventing a native height.
float calc_hermite2(const CourseVec3& point,float u,float v,const CourseQuad& p,
                    const CourseQuad& normals,const CourseSplineNeighbors& n,
                    CourseSplineTuning tuning={}); // 0x004953F0
float calc_y_pos_spl(float x,float z,const CourseQuad& p,const CourseQuad& normals,
                     const CourseSplineNeighbors& n,CourseSplineTuning tuning={}); // 0x0043CD50
} // namespace outrun::driving
