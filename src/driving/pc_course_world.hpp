#pragma once
#include "driving/pc_course_query.hpp"
#include "driving/pc_matrix_stack.hpp"
#include <array>
namespace outrun::driving {
inline CourseCollisionTables empty_world_course(){
    const Bytes none(nullptr,0);
    return {{none,none,none,0,false,false},none,none,none,none,0,false};
}
struct CourseWorldTables {
    std::array<CourseCollisionTables,4> courses{empty_world_course(),empty_world_course(),empty_world_course(),empty_world_course()};
    // Root 0x7801E8[type], 256*256 little-endian u16 word offsets into lists.
    std::array<Bytes,4> grids{Bytes(nullptr,0),Bytes(nullptr,0),Bytes(nullptr,0),Bytes(nullptr,0)};
    std::array<bool,4> grids_present{};
    // Primary 0x7D2DA0; any nonzero type selects secondary 0x7D3190.
    std::array<Bytes,2> transforms{Bytes(nullptr,0),Bytes(nullptr,0)};
    CourseSplineTuning tuning{};
};
struct CourseWorldQuery {
    const CourseWorldTables& tables;
    PcMatrixStack& matrices;
    EasyLctPredictionState& prediction;
};
// PC 0x43EB60: original world/grid/list/neighbors/Hermite calculation.
// Only point.y changes. Failure: Y=-0.1f, polygon=-1, kind=1; special preserved.
// Return is last selected type, NOT a hit Boolean. Optional outputs must be
// mutually disjoint and must not alias point or tables. Malformed views throw
// without publishing outputs or changing prediction/matrix state.
std::uint32_t get_y_position_prog(CourseWorldQuery&,std::uint32_t mode,CourseProbe&,
    std::uint32_t* polygon=nullptr,std::uint32_t* special=nullptr,std::uint32_t* kind=nullptr);

// PC 0x43EEE0 GetYPositionProg_BK. The C2C PC build selects the ordinary
// GetYPositionProg path unless a small set of game-state gates enables the
// special back-course lookup. Those external getters/globals are represented
// explicitly here; the actual world/grid/query arithmetic remains native and
// is shared with the closed 0x43EB60 implementation.
struct PcBkQueryContext {
    std::uint32_t game_mode{};          // PC global 0x780258
    std::uint32_t branch_record{};      // result of GetBranchRecord(...)
    std::uint32_t mode4_course_gate{};  // resolved [0x799D18]+0x5C when game_mode==4
    bool gate_4957f0{};                 // only consulted outside modes 3/4
    bool gate_48b310{};
    bool gate_495490{};
};
std::uint32_t get_y_position_prog_bk(CourseWorldQuery&,const PcBkQueryContext&,
    std::uint32_t mode,CourseProbe&,std::uint32_t* polygon=nullptr,
    std::uint32_t* special=nullptr,std::uint32_t* kind=nullptr);
// PC 0x43ED20(first type, mode, point, ...): GetYPositionProg on the given type, then
// the other of types 0 / 1, without the prediction update; a miss sets point.y to
// miss_y ([599440]) and returns the second type.
std::uint32_t get_y_position_43ed20(CourseWorldQuery&,std::uint32_t first,std::uint32_t mode,CourseProbe&,
    std::uint32_t* polygon,std::uint32_t* special,std::uint32_t* kind,float miss_y);
// PC 0x43F110 (the racer-branch wall probes of 504E70; argument 0 is never
// read): with branch kind 2 (451350(44C940(450380(8)))) GetYPositionProg on
// `mode`; otherwise type 3 (kind 1) or 2, then the type-0 retry, returning 0
// on a miss: 43EEE0's special path without its game-state gates.
std::uint32_t get_y_position_branch_43f110(CourseWorldQuery&,std::uint32_t branch_kind,
    std::uint32_t mode,CourseProbe&,std::uint32_t* polygon=nullptr,
    std::uint32_t* special=nullptr,std::uint32_t* kind=nullptr);
// PC 0x518E10, closed lower query. No substitution callback in this overload.
// Earlier completed query effects remain if a later native retry rejects.
std::uint32_t get_y_position_spl_chk(CourseWorldQuery&,CourseProbe&,
    std::uint32_t* polygon=nullptr,std::uint32_t* special=nullptr,std::uint32_t* kind=nullptr);
}
