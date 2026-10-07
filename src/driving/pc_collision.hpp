#pragma once
#include "driving/pc_driving.hpp"
#include <array>
namespace outrun::driving {
struct CourseProbe { float x{}, y{}, z{}; };
struct CourseQueryResult {
    float y{};
    std::uint32_t course_or_return{};
    std::uint32_t collision_index{};
    std::uint32_t special_index{};
    std::uint32_t flags{};
};
using CourseQueryFn = CourseQueryResult(*)(std::uint32_t mask, const CourseProbe& point, void* user);
struct CourseQuery { CourseQueryFn get_y_position_prog{}; void* user{}; };

// PC CalcCollisionArea (0x0043CB40): 24-unit X/Z cells in a 256x256 grid
// centered by +3072. The returned value is (z_cell << 8) | x_cell.
std::uint32_t calc_collision_area(float x, float z);

// PC internal polygon-height helpers used by GetRoadCond. A PC polygon record
// stores four xyz vertices at +0x00/+0x0c/+0x18/+0x24.
float course_triangle_plane_y(Bytes polygon, float x, float z); // 0x0043CC90
float course_quad_plane_y(Bytes polygon, float x, float z);     // 0x0043DE80

// PC Update_EasyLctPredictionTable (0x0043CBC0 semantic body at +5).
// The PC owns a 16-slot ring but its threshold scan intentionally considers
// only slots 0..14; preserve that quirk because GetYPositionProg consumes it.
struct EasyLctPredictionState {
    std::array<std::uint32_t,16> recent{};
    std::uint32_t cursor{};
    std::uint32_t easy{};
};
void update_easy_lct_prediction_table(EasyLctPredictionState& state, std::uint32_t load_coli_type);

// PC CopGetOfsDir (0x0043D340): combines the selected area-display yaw with
// the signed per-polygon direction offset stored at collision_record+0x3e.
std::int32_t course_collision_offset_direction(Bytes collision_table, std::uint32_t collision_index,
                                               float area_yaw_radians, bool table_present=true);
// PC GetCourseLength (0x00401810): u16 lookup by collision index in the
// course-selected length table. Index -1 and an absent table return zero.
std::uint16_t course_length(Bytes length_table, std::int32_t collision_index, bool table_present=true);
// PC course collision ext-flag lookup (0x0043D440). Table records are 0x40 bytes.
std::uint16_t course_collision_ext_flags(Bytes collision_table, std::int32_t collision_index, bool table_present=true);

// PC GetYPositionSplChk (0x00518E10), reconstructed around an explicit lower course-query boundary.
// Returns false only when the native lower query dependency is absent.
bool get_y_position_spl_chk(CourseProbe& point, std::uint32_t* collision_index,
                            std::uint32_t* special_index, std::uint32_t* flags,
                            const CourseQuery& query, std::uint32_t* course_or_return=nullptr);
// PC CarSusColiCheck (0x00518FA0). Wheel/parameter views are explicit native views;
// guest pointer fields are never dereferenced by native code.
void car_sus_coli_check(Bytes event, Bytes work, Bytes parameters, const std::array<Bytes,4>& wheels);

// PC CarSusBumpPush (0x00519170).  Wheel views correspond to work+0x248 pointers.
// Uses the body matrix stored at work+0x10 instead of a global matrix stack.
void car_sus_bump_push(Bytes event, Bytes work, Bytes parameters, const std::array<Bytes,4>& wheels);

// Stack-aware forms preserve the original temporary matrix side effects.
// The earlier overloads retain their isolated work-matrix boundary for replay.
struct PcMatrixStack;
void car_sus_coli_check(Bytes,Bytes,Bytes,const std::array<Bytes,4>&,const PcMatrixStack&);
void car_sus_bump_push(Bytes,Bytes,Bytes,const std::array<Bytes,4>&,PcMatrixStack&);

// PC ColiCar (0x00519830) is a thin ordered dispatcher around four collision stages.
// Callback dispatch only: passing work does not reproduce every temporary
// matrix-stack effect. Ground/body-wall integration still requires a stack audit.
using CollisionStage = void(*)(Bytes event, Bytes work, void* user);
struct CollisionStages {
    CollisionStage ground_face{};
    CollisionStage suspension_check{};
    CollisionStage suspension_bump_push{};
    CollisionStage body_wall{};
    void* user{};
};
// Returns false and performs no stage call when any dependency is missing.
bool coli_car(Bytes event, Bytes work, const CollisionStages& stages);
}
