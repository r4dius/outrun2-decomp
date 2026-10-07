#pragma once
#include "driving/pc_crash.hpp"
#include "driving/pc_wrecker.hpp"
#include <stdexcept>
namespace outrun::driving {
// r021: scalar-SSE state helpers called by the crash-entry routine. Every
// pointee is an explicit bounded view; serialized PC pointers are not followed.
struct PcImpactBands {
    Bytes count, low, middle, middle_ratio, high, high_ratio; // each 4 bytes
};
void pc_calc_impact_bands(float value, std::uint32_t option2, std::uint32_t option3,
                          Bytes thresholds, const PcImpactBands&); // 46C5B0
struct PcImpactFeedback {
    Bytes thresholds; // 3 floats, pointee of 64DEEC
    Bytes increments; // float[state], pointee of 64DEF0
    bool enabled;    // 80FB14 != 0
};
void pc_refresh_impact_feedback(Bytes event, const PcImpactFeedback&); // 46C6E0
void pc_add_impact_feedback(Bytes event, std::uint32_t state, float scale,
                            const PcImpactFeedback&); // 46C780

// 4A2270 can invoke PlWrecker after publishing the new crash state.  The PC
// obtains work from global 82E7F0 and follows serialized event/work pointers to
// body/wheels.  Native code receives those views and services explicitly.
// A missing context is rejected before the first event write so callers that do
// not model recovery cannot accidentally execute a partial tow path.
class PcMissingWrecker final : public std::logic_error {
 public: PcMissingWrecker():std::logic_error("active crash entry requires an explicit PlWrecker context"){}
};
struct PcCrashWreckerContext {
    Bytes work;
    Bytes body_params;
    Bytes wheel_block;
    PcPlWreckerContext* services=nullptr;
};
struct PcCrashEntryContext {
    PcCrashTables tables;
    PcStageViews stages;
    Bytes reroute_ranges; // 5C2570, stage*64, signed16 inclusive endpoints
    PcImpactFeedback feedback;
    const PcCrashWreckerContext* wrecker=nullptr;
};
// Complete PC entries 0x004A2270 and 0x004A6EA0.  The legacy "candidate"
// aliases are retained for source compatibility with r021-r024 tests.
void pc_enter_crash(Bytes event, std::uint32_t state,
    std::uint32_t reverse, bool request_wrecker, float rate, const PcCrashEntryContext&);
void pc_start_crash(Bytes event, std::uint32_t state,
    std::uint32_t reverse, bool request_wrecker, const PcCrashEntryContext&);
inline void pc_enter_crash_candidate(Bytes event, std::uint32_t state,
    std::uint32_t reverse, bool request_wrecker, float rate, const PcCrashEntryContext& c){
    pc_enter_crash(event,state,reverse,request_wrecker,rate,c);
}
inline void pc_start_crash_candidate(Bytes event, std::uint32_t state,
    std::uint32_t reverse, bool request_wrecker, const PcCrashEntryContext& c){
    pc_start_crash(event,state,reverse,request_wrecker,c);
}
struct PcCrushSelection {
    float trapped_low=-170.0f, trapped_high=-130.0f, ordinary_speed=216.720001220703125f;
    float ordinary_low=-20.0f, ordinary_high=-33.0f, angle_threshold=0x1.657186p-3f;
    Bytes commands; // 5E0DF0, u32[state]
};
// Complete PC CwCrushStatus entry 0x005038D0.  It performs the original
// gate/threshold/state selection and, on accepted collisions, enters crash
// state through the closed StartCrash -> PlWrecker path before dispatching
// effects and material sounds.
bool pc_cw_crush_status(Bytes event, Bytes work, float angle, Bytes contacts,
    const PcCrashEntryContext&, const WallResponseContext&, const PcCrushSelection&,
    const PcMaterialSounds&, PcSoundQueue&, const std::array<PcCourseEndView,4>&,
    std::uint32_t mode, std::uint32_t variant);
inline bool pc_cw_crush_status_candidate(Bytes event, Bytes work, float angle, Bytes contacts,
    const PcCrashEntryContext& c, const WallResponseContext& w, const PcCrushSelection& s,
    const PcMaterialSounds& m, PcSoundQueue& q, const std::array<PcCourseEndView,4>& ends,
    std::uint32_t mode, std::uint32_t variant){
    return pc_cw_crush_status(event,work,angle,contacts,c,w,s,m,q,ends,mode,variant);
}
}
