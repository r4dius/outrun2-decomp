#pragma once
#include "driving/pc_course_world.hpp"
#include "driving/pc_crash.hpp"
#include <array>
namespace outrun::driving {
// Explicit dependencies for the road-info service consumed by PlWrecker.
// area_yaw_radians corresponds to GetAreaDisp()->+4 for primary/secondary.
struct PcRoadInfoContext {
    const CourseWorldTables& tables;
    PcMatrixStack& matrices;
    std::array<float,2> area_yaw_radians{};
};
// PC GetCsRoadInfoByCsLen 0x0043E3B0. The 0x64-byte destination contains
// undefined/padding bytes above the documented fields; this routine writes only
// the bytes the PC function itself defines. Serialized guest pointers are never
// followed. Returns the original success boolean.
bool pc_get_cs_road_info_by_cs_len(Bytes road_info,Bytes on_road_place,
                                   std::int32_t polygon_hint,PcRoadInfoContext&);
// PC PlWrecker_Sub 0x00503780. Defined road-info bytes [0,0x58) are
// published; the original copies 12 bytes of uninitialised stack padding after
// them, which native code deliberately leaves untouched.
void pc_pl_wrecker_sub(Bytes event,Bytes on_road_place,Bytes road_info,PcRoadInfoContext&);

// PC 0x0046FE70. Advances an OnRoadPlace by a signed step and performs the
// original primary/secondary route transition when the current course end is
// crossed. The four service values below are explicit native stand-ins for
// read-only globals/getters used by the PC routine; stage-cache and route
// side effects remain visible and are not hidden behind callbacks.
struct PcCourseAdvanceContext {
    const std::array<PcCourseEndView,4>& course_ends;
    const PcStageViews& stages;
    Bytes stage_cache;
    PcRouteContext& route;
    std::uint32_t current_stage_key; // value returned by PC 0x44BDD0
    std::uint8_t stage_limit;        // low byte returned by PC 0x44BE00
    bool transition_allowed;        // PC 0x44BE50(0)
};
bool pc_advance_on_road_place(Bytes on_road_place,std::int32_t step,PcCourseAdvanceContext&);


// PC ReconstructPostureMatrixAndFaceWork 0x00503DD0.  The PC stores guest
// pointers to body parameters at event+0x2B4 and the contiguous four-wheel
// block at work+0x248.  Native code receives both views explicitly and never
// follows those serialized pointers.  wheel_block is four 0xF4-byte records.
void pc_reconstruct_posture_matrix_and_face_work(Bytes event,Bytes work,
                                                  Bytes body_params,Bytes wheel_block,
                                                  CourseWorldQuery&);

// PC CalcDispMatrix 0x004A2650.  The original obtains blend from 0x4493E0
// and the petty-auto scene code from 0x4872F0.  They are read-only services,
// represented explicitly so native code never calls back into x86.
struct PcDispMatrixContext {
    PcMatrixStack& matrices;
    float blend=1.0f;
    std::uint8_t scene_code=0;
};
void pc_calc_disp_matrix(Bytes event,PcDispMatrixContext&);

// Immediate branch (phase <= 1) of PC PlWrecker 0x00504900. Serialized PC
// pointers event+0x2B4 and work+0x248..0x254 are never followed; body_params
// and the contiguous four-wheel native block are supplied explicitly.
void pc_pl_wrecker_immediate(Bytes event,Bytes work,Bytes body_params,Bytes wheel_block,
                             std::int32_t phase,PcRoadInfoContext&,CourseWorldQuery&,
                             PcDispMatrixContext&);

// Validated delayed/retry branch of PC PlWrecker 0x00504900 (third argument > 1).
// This wrapper performs no hidden callback substitution: it uses the
// reconstructed advance and road-info paths.
void pc_pl_wrecker_delayed(Bytes event,std::int32_t phase,
                           PcCourseAdvanceContext&,PcRoadInfoContext&);

// Complete public PC PlWrecker 0x00504900 dispatch.  The two original phase
// domains were differential-tested independently against this same public x86
// entry: phase<=1 uses the immediate posture reset; phase>1 uses the delayed
// retry path.  All native views/services remain explicit.
struct PcPlWreckerContext {
    PcCourseAdvanceContext& advance;
    PcRoadInfoContext& road;
    CourseWorldQuery& query;
    PcDispMatrixContext& display;
};
void pc_pl_wrecker(Bytes event,Bytes work,Bytes body_params,Bytes wheel_block,
                   std::int32_t phase,PcPlWreckerContext&);
}
