#pragma once
// PC event 386 SCN_ENV (function 0x33): init 449FC0 and control 44A890.
//   449FC0: 4518E0 fog defaults, 407940 default light table (three sun
//           lights, six local lights 899D78, six lights 89A138;
//           SetRenderState(LIGHTING, 1); 449F80 clears the sun lists 7D26A8),
//           progression flags 7D28B0..7D28C4 = 1, phase 7D28C8 = 3.
//   44A890: by the root mode 78026C (table 44A910): the 44A8DF update
//           (4517D0 fog, 449F50 sun + local lights, 44A000 phase), the
//           49B2D0 timer gates (0x12C, 0x168 for race modes 3/4) or 4518C0
//           (fog off); every path ends with 408310 (sun positions 1 and 2).
// The light table is the 0x960-byte 899B98..89A4F8 block, the fog records
// the 0x54-byte 7D3A10 block.
#include "driving/pc_environment_blend.hpp"
#include "platform/pc_d3d9.hpp"
#include <array>
#include <functional>
namespace outrun::platform {
struct PcSceneEnvironment {
    std::array<std::uint8_t,0x960> lights_899b98{};
    std::array<std::uint8_t,0x54> fog_7d3a10{};
    std::array<std::uint32_t,3> fog_lists_7d3a00{};   // list tokens, 0 = absent
    std::array<std::uint32_t,3> sun_lists_7d26a8{};   // list tokens, 0 = absent
    std::array<std::uint32_t,6> flags_7d28b0{};
    std::uint32_t phase_7d28c8{};
};
// 449FC0. matrices: the 89B564 stack used by 44A430.
void scene_environment_init_449fc0(PcSceneEnvironment&,PcD3D9Device&,driving::PcMatrixStack&);
// 407940 alone (light table defaults), 4518E0 and 4518C0 alone, 408310 alone.
void scene_environment_lights_407940(PcSceneEnvironment&,PcD3D9Device&,driving::PcMatrixStack&);
void scene_environment_fog_defaults_4518e0(PcSceneEnvironment&);
void scene_environment_fog_off_4518c0(PcSceneEnvironment&);
void scene_environment_positions_408310(PcSceneEnvironment&);
// 44A890. update runs the 44A8DF sequence (course_environment_update_44a8df
// over the owner's resolved lists); bridge_49eed0 and timer_49b2d0 are the
// protected queries of the table-0 and table-2 gates.
struct PcSceneEnvironmentControl {
    std::uint32_t mode_78026c{};
    std::uint32_t race_mode_780258{};
    std::function<std::uint32_t()> bridge_49eed0;
    std::function<std::uint16_t()> timer_49b2d0;
    std::function<void()> update_44a8df;
};
void scene_environment_control_44a890(PcSceneEnvironment&,const PcSceneEnvironmentControl&);
}
