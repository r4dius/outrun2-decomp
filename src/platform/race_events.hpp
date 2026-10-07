#pragma once
// Small per-frame GAME events of the race (event ids from the START event
// table): event 0 function 0x36 game clock (4AF420 init / 4AF4A0 control),
// event 1 function 0 frame counter (49ACE0 / 49ACF0), event 7 function 0x46
// camera-steering override (4866D0 / 4874A0) and event 361 function 0x52
// display (49F400). Event 360 (45A280) drives the static object 7D68AC
// (98A650, the 430xxx-435xxx audio manager) and waits for that owner.
// Callees that are not ported are reported (missing), never replaced.
#include <array>
#include <cstdint>
namespace outrun::platform {
struct RaceGameClock {                       // PC 0x84210C..0x84212C
    std::uint32_t w84210c{};
    float time_842110{},step_842114{},next_step_842118{};
    std::int32_t frames_84211c{};
    float f842120{},f842124{};
    std::uint32_t w842128{},w84212c{};
};
void race_clock_init_4af420(RaceGameClock&);
void race_clock_control_4af4a0(RaceGameClock&);
void race_frame_counter_init_49ace0(std::uint32_t& counter_8367f4);
void race_frame_counter_control_49acf0(std::uint32_t& counter_8367f4);
struct RaceCameraOverride {                  // PC 0x82E7C0..0x82E7EC
    std::uint32_t w82e7c0{};
    std::int16_t w82e7c4{};
    std::uint32_t w82e7c8{};
    std::int16_t w82e7cc{};
    std::uint32_t w82e7d0{};
    std::uint8_t scene_82e7d4{};
    std::int32_t override_82e7d8{};
    std::int16_t w82e7dc{};
    float time_82e7e0{};
    std::uint32_t w82e7e4{};
    float f82e7e8{};
    std::uint32_t steering_82e7ec{};
};
void race_camera_override_init_4866d0(RaceCameraOverride&);
// Returns the missing PC callee (486730) when 82E7D8 != 0, else 0.
std::uint32_t race_camera_override_control_4874a0(RaceCameraOverride&);
// 49F400: event 10 flags 79FB50 & 3 -> 4695C0 (returned as missing), else 0.
std::uint32_t race_display_49f400(std::uint8_t flags_79fb50);
}
