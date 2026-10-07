#pragma once
// PC event display passes: 43FB40 (display callbacks of the events whose
// display-scene mask matches the layer, with the temporary colouring of the
// sun light 4082B0(2,0,0)) and 4400C0 (layer-2 shadow callbacks).
// Callbacks, the light API (410740/4107A0), the alpha flush 405830 and the
// work word +0xD10 read are services, so the same state can be compared with
// the original i386 code.
#include "driving/pc_common_control.hpp"
#include <array>
#include <cstdint>
namespace outrun::platform {
struct PcSceneDisplayGlobals {
    std::uint32_t mode_780258{};      // game mode, 2 = frontend branch
    std::uint32_t scene_7f2428{};     // 45C440
    std::uint8_t paused_780248{};     // 43F9C0
    std::uint32_t object_7f9460_60{}; // 55A930(7F9460): +0x60
    // 4AF5A0: bit 2 of the first byte of the work of slot 387 (0x183, the
    // course event; 79F5EC is that slot's +8 work pointer).
    std::uint8_t shadow_79f5ec{};
    float phase_7a0db4{};             // advanced by 59D914 (else branch)
    float phase_7a0db8{};             // advanced by 59D918 (mode 2 branch)
    // 7A0DBC..7A0DE8: saved light lanes +0x28, +0x18, +0x08 (4 words each).
    std::array<std::uint32_t,12> saved_7a0dbc{};
};
using PcSceneDisplayCallback=void(*)(void* user,std::uint32_t callback,std::uint32_t work,std::uint32_t event_id);
using PcSceneDisplayWork=std::uint32_t(*)(void* user,std::uint32_t work);
using PcSceneDisplayVoid=void(*)(void* user);
using PcSceneDisplayLight=void(*)(void* user,std::uint8_t* light);
struct PcSceneDisplayServices {
    void* user{};
    PcSceneDisplayCallback display{};
    PcSceneDisplayWork work_d10{};     // [work+0xD10]
    PcSceneDisplayVoid flush_alpha{};  // 405830(0)
    PcSceneDisplayVoid lights_reset{}; // 410740
    PcSceneDisplayLight light_add{};   // 4107A0(light)
};
// light: the 0xA0-byte record 4082B0(2,0,0) returns (lanes +8/+0x18/+0x28).
void event_display_43fb40(driving::PcEventControlState& state,std::uint32_t mask,
                          std::int32_t first,std::int32_t last,std::uint8_t* light,
                          PcSceneDisplayGlobals& g,const PcSceneDisplayServices& s);
void event_shadow_display_4400c0(driving::PcEventControlState& state,const PcSceneDisplayGlobals& g,
                                 const PcSceneDisplayServices& s);
}
