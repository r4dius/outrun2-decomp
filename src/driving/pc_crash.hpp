#pragma once
#include "driving/pc_wall_rebound.hpp"
#include <optional>
#include <utility>
#include <vector>
namespace outrun::driving {
// r020, original PC 5034C0. Normalize motion if its spilled squared length is
// positive; otherwise use current-matrix row +20. The wall vector is NOT
// normalized by this routine. Non-x86 transcendental rounding is unvalidated.
long double calc_wall_impact_angle(CourseVec3 motion, CourseVec3 wall, const PcMatrixStack&,std::uint32_t crt_math_mode=0);
X87 calc_wall_impact_angle_x87(CourseVec3 motion, CourseVec3 wall, const PcMatrixStack&,std::uint32_t crt_math_mode=0);

struct PcMaterialSounds {
    Bytes priority_masks; // eight u32, original 5E0F00
    std::array<Bytes,8> commands; // explicit pointees of 5E0EE0; u32[state]
};
// PC 503570: OR record+8 flags, first matching material mask wins (default0),
// signed-byte cooldown, and the already reconstructed sound command queue.
// Required views are preflighted before cooldown/queue writes. No x86 pointers.
void pc_collision_material_sound(Bytes event, Bytes work, Bytes contacts,
                                  std::uint32_t state, const PcMaterialSounds&, PcSoundQueue&);
struct PcCourseEndView {
    std::optional<Bytes> header; // explicit optional 780140[type], count at +0C
    Bytes positions;            // explicit 780228[type], u16 array
};
std::uint16_t pc_course_end_position(const PcCourseEndView&); // 43D470, meaningful AX only
// 5035E0 calls 49A650, whose actual PC entry is RET. Preserves reads/arithmetic
// on active paths, but there is no rendering/effect state to synthesize.
void pc_crash_effect_dispatch(Bytes event,std::uint32_t state,
    const std::array<PcCourseEndView,4>&,std::uint32_t mode,std::uint32_t variant);

// Original channel keys: stride10, float time/value/incoming/outgoing at 0/4/8/C.
// The native pointer view is authoritative, never serialized descriptor+4.
struct PcCrashChannel { std::int16_t count; Bytes keys; };
struct PcCrashCursor {
    PcCrashChannel channel;
    std::optional<std::size_t> lower, upper; // original descriptor +0C / +10
};
void pc_find_crash_segment(PcCrashCursor&,float time); // 513590
long double pc_interpolate_crash_keys(Bytes keys,std::size_t lower,std::size_t upper,float time); // 5134C0
long double pc_sample_crash_channel(const PcCrashChannel&,float time); // 513650
X87 pc_interpolate_crash_keys_x87(Bytes keys,std::size_t lower,std::size_t upper,float time);
X87 pc_sample_crash_channel_x87(const PcCrashChannel&,float time);
using PcCrashPose=std::array<PcCrashChannel,6>;
// Original 5136C0 stores XYZ of the first output, then XYZ of the second.
// The two outputs may overlap. Inputs must not overlap either output.
void pc_sample_crash_pose(const PcCrashPose&,float time,Bytes translation,Bytes rotation);
struct PcCrashTables {
    Bytes primary;   // original 5E08E8, 8-byte rows: guest pointer (ignored), duration
    Bytes recovery;  // original 5E0988, 12-byte rows: recovery duration at +0
    std::vector<PcCrashPose> poses; // explicit pointees of primary[i]+0
};
float pc_crash_duration(const PcCrashTables&,std::uint32_t state); // 4F6580
float pc_crash_recovery_duration(const PcCrashTables&,std::uint32_t state); // 4F6590
// PC 4A2400, full state/timer and six-channel pose update. Needs event through
// +104B; do not increase unrelated legacy event_size or overlap it with work.
// This is not the state-entry routine 4A2270 (which can call PlWrecker).
void pc_advance_crash_state(Bytes event,const PcCrashTables&);
}
