#pragma once
// PC frame renderer 449050: nine layers, each with its render targets
// (448ED0), pass defaults (410670, 404540), environment (40D840, 4089A0 when
// the 5A27B4 entry changes), course pre/post passes (40BD80/40BE70), the
// event displays of the layer (414340, 4400C0, 440CA0 -> 43FB40), the layer-6
// glare passes and the layer callback 7D2620; 448DB0/448E40 around the
// loop, then 42D710(0, 0x15) and the player car overlay 49F4D0.
// Subsystems not ported yet are leaf services (pc, argument); everything
// else runs natively on the PcD3D9Device.
#include "platform/pc_render_flush.hpp"
#include "platform/pc_scene_display.hpp"
#include <functional>
namespace outrun::platform {
struct PcFrameLayer { std::uint32_t colour{},depth{},callback{}; }; // 7D2618 + layer*0xC
struct PcFrameState {
    std::uint32_t device_ready_7d2614{};
    std::uint32_t current_colour_7d25e8{},backbuffer_7d25ec{},layer_7d25f0{},depth_7d25f4{};
    std::array<std::uint32_t,6> viewport_7d25f8{};
    std::uint32_t current_depth_7d2610{};
    std::array<PcFrameLayer,9> layers{};
    std::uint32_t course_73e2a4{},course_73e2a8{};   // set to -1 by 40BD80
    std::uint32_t clear_colour_89bd5c{};              // 40EC70 clear colour
};
struct PcFrameInputs {
    std::uint32_t game_mode_78026c{};
    float screen_740c94{},screen_740c98{};
    std::uint8_t glare_95af09{};
    // 79FCCA / 79FCCB / 79FCCF are the flag bytes of events 386 / 387 (the
    // course, work 79F5EC) / 391: they are read from PcFrameServices::events.
    std::uint32_t mode_84a318{};
    std::uint32_t course_work_79f5ec{};  // work token of event 387 (course)
    std::uint32_t course_flags{};        // its first dword ([[79F5EC]])
    std::array<float,16> matrix_7d2da0{};// 44BEA0 (environment 1 inverse)
};
// Leaf pcs: 414340 (layer 0), 4C50A0, 40BF10/40C1B0/40C550/422820 (40BD80),
// 40C150/40C8D0/40C4A0/40CBC0/422F20 (40BE70), 414E50/414F00, the layer
// callback (its address), 42D710 (argument 0x15), 49F4D0 (event 8 work).
// arg is the PC argument (course work + offset, layer...), 0 when none.
using PcFrameLeaf=std::function<void(std::uint32_t pc,std::uint32_t arg)>;
struct PcFrameServices {
    PcFrameLeaf leaf;
    driving::PcEventControlState& events;
    PcSceneDisplayGlobals& scene;
    std::uint8_t* sun_light;             // 4082B0(2,0,0), inside environment.lights_899b98
    PcSceneDisplayServices display;
    PcEnvironmentRenderTables environment;
    // [84A318] as read after the layers (a layer callback, 4BFA20, writes it); unset: in.mode_84a318.
    std::function<std::uint32_t()> mode_84a318{};
};
void frame_render_449050(PcFlushContext&,PcFrameState&,const PcFrameInputs&,PcFrameServices&);
// Pieces, exposed for the tests.
void frame_begin_448db0(PcFlushContext&,PcFrameState&,const PcFrameInputs&);
void frame_targets_448ed0(PcFlushContext&,PcFrameState&,std::uint8_t flags_79fccf,std::uint32_t layer);
void frame_end_448e40(PcFlushContext&,PcFrameState&);
void frame_clear_40ec70(PcFlushContext&,const PcFrameState&,std::uint32_t target);
}
