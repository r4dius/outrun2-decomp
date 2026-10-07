#include "platform/race_events.hpp"
namespace outrun::platform {
namespace {
std::int32_t cvttss2si(float v){ // SSE truncation; out of range/NaN -> 0x80000000
    if(!(v>-2147483904.f&&v<2147483648.f))return std::int32_t(0x80000000u);
    return std::int32_t(v);
}
}
void race_clock_init_4af420(RaceGameClock& c){
    c=RaceGameClock{};
    c.time_842110=0.f;c.step_842114=1.f;c.next_step_842118=-1.f;             // 62806C / 6280C4
    c.frames_84211c=1;c.f842120=1.f;c.f842124=-1.f;
}
void race_clock_control_4af4a0(RaceGameClock& c){
    if(c.next_step_842118>=0.f){c.step_842114=c.next_step_842118;c.next_step_842118=-1.f;}
    const float before=c.time_842110;
    const float after=before+c.step_842114;
    c.frames_84211c=std::int32_t(std::uint32_t(cvttss2si(after))-std::uint32_t(cvttss2si(before)));                 // CVTTSS2SI
    c.time_842110=after;
}
void race_frame_counter_init_49ace0(std::uint32_t& n){n=0u;}
void race_frame_counter_control_49acf0(std::uint32_t& n){++n;}
void race_camera_override_init_4866d0(RaceCameraOverride& o){
    o.override_82e7d8=0;o.time_82e7e0=3.40282347e+38f;                          // 59943C (FLT_MAX)
    o.w82e7c4=0x7fff;o.scene_82e7d4=0xffu;o.w82e7c8=0u;o.steering_82e7ec=0u;o.w82e7e4=2u;
    o.w82e7dc=0;o.w82e7d0=1u;o.w82e7c0=1u;o.w82e7cc=0;
}
std::uint32_t race_camera_override_control_4874a0(RaceCameraOverride& o){
    if(o.override_82e7d8==0)return 0u;
    return 0x486730u;                                                           // 486730 / 486EF0 replay camera: not ported
}
std::uint32_t race_display_49f400(std::uint8_t flags){return (flags&3u)?0x4695c0u:0u;}
}
