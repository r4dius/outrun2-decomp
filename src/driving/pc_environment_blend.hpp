#pragma once
#include "pc_course_environment.hpp"
#include "pc_course_query.hpp"
namespace outrun::driving {
// Shared data views, not renderer callbacks. Record IDs have already been
// resolved to bounded source views; absent roots have size zero.
struct PcEnvironmentBlendContext {
    std::uint32_t phase_7d28c8{};    // retained by the caller between frames
    std::int16_t time_7d2934{};
    float duration_7d28d8{};
    Bytes saved_sun_7d26d0;
    Bytes saved_fog_7d28e0;
    Bytes live_sun_899b98;
    Bytes live_fog_7d3a10;
    Bytes primary_matrix_7d2da0;
    PcMatrixStack& matrices;
};
struct PcEnvironmentAngles {float pitch{},yaw{};};
PcEnvironmentAngles environment_angles_44a480(CourseProbe);
CourseProbe environment_direction_44a430(float pitch,float yaw,PcMatrixStack&);
// All original phase branches: snapshot(0), interpolate(2), otherwise no-op.
// lane0=fog, lane1=sun. Unknown lane values perform no stores as on PC.
void course_environment_blend_44ab10(const std::array<Bytes,3>& records,
    unsigned lane,PcEnvironmentBlendContext&);
// Camera-relative original local-light selection. nearest contains all three
// distance/index pairs (including the spill pair); lights contains six A0
// records from 899D78. An absent list is an empty view.
void course_environment_lights_44a1d0(Bytes list, Bytes camera,
    Bytes nearest_7d2d58, Bytes lights_899d78);
// PC 44A520 (register argument): resolve record +00/+10 course runs of the
// PRIMARY collision type through 43E570, transform by 7D2DA0 and publish
// start +04, end +14, reciprocal length +20 and unit start-end +24.
void course_environment_record_44a520(Bytes record,const CourseCollisionTables& primary,PcEnvironmentBlendContext&);
void course_environment_progress_44b020(const std::array<Bytes,3>& lists,unsigned lane,
    Bytes vehicle,Bytes flags_7d28b0,const CourseCollisionTables& primary,PcEnvironmentBlendContext&);

// Getters 44C610/44C640. The PC reads 79FCCE&3 and, when it is 2, the course
// record [[[7D3188]+14]+58]: u16 trigger segment +0 and signed duration byte +2.
// An absent record is an empty view and yields the PC defaults 100 and 2.
struct PcEnvironmentTransition {
    std::uint8_t mode_79fcce{};
    Bytes record{nullptr,0};
};
std::int16_t environment_trigger_44c610(const PcEnvironmentTransition&);
std::int8_t environment_duration_44c640(const PcEnvironmentTransition&);
// PC 44A000: 0 arms time/duration, 1 waits for vehicle +64 to reach the trigger,
// 2 counts down, 3 is steady-state course progression. Mutates the context.
void course_environment_phase_44a000(Bytes vehicle,const PcEnvironmentTransition&,
    PcEnvironmentBlendContext&);

// Per-frame roots used by 4517D0 and 449F50. The lists are the fog 7D3A00 and
// sun 7D26A8 pointer arrays resolved to bounded list views; light_list is 7D26B4.
struct PcEnvironmentFrame {
    std::array<Bytes,3> fog_lists{Bytes(nullptr,0),Bytes(nullptr,0),Bytes(nullptr,0)};
    std::array<Bytes,3> sun_lists{Bytes(nullptr,0),Bytes(nullptr,0),Bytes(nullptr,0)};
    Bytes light_list{nullptr,0};
    Bytes vehicle{nullptr,0};        // shared car 799D18: +14 position, +5C, +64 segment
    Bytes camera{nullptr,0};         // 79F574: +F8 eye, +104 target
    Bytes flags_7d28b0{nullptr,0};   // six u32 progression-refresh flags
    Bytes nearest_7d2d58{nullptr,0}; // three distance/index pairs
    Bytes lights_899d78{nullptr,0};  // six local-light records
};
// PC 4517D0: fog lane. Vehicle +5C selects interpolation over progression.
void course_environment_fog_4517d0(PcEnvironmentFrame&,const CourseCollisionTables& primary,PcEnvironmentBlendContext&);
// PC 449F50: sun lane then camera local lights (tail jump to 44A1D0).
void course_environment_sun_449f50(PcEnvironmentFrame&,const CourseCollisionTables& primary,PcEnvironmentBlendContext&);
// Call sequence at 44A8DF: 4517D0, 449F50, 44A000. The preceding mode and
// event gates of 44A890 remain the caller's responsibility.
void course_environment_update_44a8df(PcEnvironmentFrame&,const CourseCollisionTables& primary,
    const PcEnvironmentTransition&,PcEnvironmentBlendContext&);
}
