#pragma once
// Native runtime binding of the GAME events 387 SCN_EFC (race_scene_effects)
// and 397 PART_EFC (race_particles). The ported functions address the PC
// memory they touch by PC address; this binding maps what the native runtime
// owns (event works, the player car 799D18 and camera 79F574 bytes, the
// scene environment light table 899B98, event flags 79FB48, mode words, the
// frame counter 95AF0C, the camera override block 82E7C0, the renderer
// matrices 95D860..) and nothing else. A read of an unmapped address or a
// call of an unported callee latches a fault for that event (first PC and
// reason kept, later callbacks of the event skipped): nothing is invented.
#include "platform/race_particles.hpp"
#include "platform/race_scene_effects.hpp"
#include "platform/pc_pmt_loader.hpp"
#include <functional>
#include <cstdint>
#include <map>
#include <string>
namespace outrun::platform {
struct NativeRuntimeContext;
class PcSceneRenderer;
struct NativeRaceEffectsState {
    PcSceneEffectsState scene{};
    PcParticleState particles{};
    std::uint8_t* glow_8a8c18{};                // bound to the renderer globals 8A8C18..8A8C5C (4160F0 record; single owner)
    std::uint8_t* lights_899b98{};              // bound to the scene environment light table (0x960 bytes)
    // Loaded PMT bank of a resource (the particle textures, resource 0x57);
    // nullptr when the runtime cannot load it.
    std::function<PcPmtResources*(std::uint32_t resource)> bank;
    std::array<std::uint8_t,0x48> resource_57{};    // 7C2800 + 0x57*0x48 entry (+0 system, +24 texture table)
    std::array<std::uint8_t,4> texture_table_57{};
    bool bank57_failed{};                          // not retried every frame
    std::array<std::uint8_t,0x48> resource_d0{};    // 7C2800 + 0xD0*0x48 (the ending props bank, 420560 in mode 24)
    std::array<std::uint8_t,4> texture_table_d0{};
    std::array<std::uint8_t,0x28> event_flags_79fb48{};
    std::array<std::uint8_t,0x24> mode_78024c{};     // 78024C..780270
    std::array<std::uint8_t,24*4> cars_799d18{};     // 799D18 + id*0x3C is only read for id 0; the other ids stay 0
    std::array<std::uint8_t,4> camera_79f574{},frame_95af0c{},network_7f94c0{};
    std::array<std::uint8_t,0x2c> override_82e7c0{};
    std::array<std::uint8_t,16> runs_780140{},runs_780228{};   // course run roots [type] (43D470: header +0C count, u16 lengths)
    std::array<std::uint8_t,0x240> render_95d860{};  // renderer matrix slots 0..8 (95D860..95DAA0)
    std::array<std::uint8_t,8> screen_740c8c{};      // EXE .data 640.0 / 480.0 (no writer in the EXE)
    std::uint32_t scene_fault_pc{},particles_fault_pc{};
    std::string scene_fault,particles_fault;
    std::uint32_t scene_inits{},scene_controls{},scene_destroys{};
    std::uint32_t particle_inits{},particle_controls{},particle_displays{},smoke_emits{},flare_emits{};
    std::uint32_t display_dropped{};             // FVF draws / SetTransform the device cannot execute (dropped, counted)
    std::map<std::uint32_t,std::uint32_t> unported;  // unported callee PC -> count
    PcRaceMemory memory;                           // region list reused every callback (no per-frame allocation)
};
// Event callbacks 4AFBB0/4AFDC0/4AFC70 (387) and 41BBF0/41BC60 (397).
bool native_race_effects_invoke(NativeRuntimeContext&,std::uint32_t callback,driving::PcMatrixStack&);
// Display callback 41BD10 (397) inside PcSceneRenderer::render_frame.
bool native_race_effects_display(NativeRuntimeContext&,std::uint32_t callback,PcSceneRenderer&);
// AREA / player car / sky setters of the SCN_EFC work (4AF550..4AFD90),
// for the owners of those events once they are integrated.
bool native_race_effects_setter(NativeRuntimeContext&,const PcRaceCall&,std::uint32_t& eax);
// 4208A0(count, &pos, colour) (the AUTOSCENE smoke): false when PART_EFC is latched or the emitter faults.
// 41FB10 + 41C420 (the goal camera script op 21) on the particles memory; false when faulted.
bool native_race_effects_tire_reset(NativeRuntimeContext&,driving::PcMatrixStack&);
bool native_race_effects_smoke_4208a0(NativeRuntimeContext&,driving::PcMatrixStack&,std::uint32_t count,
                                      std::uint32_t x,std::uint32_t y,std::uint32_t z,std::uint32_t colour);
// 420560 (the arcade ending's falling pieces): false when PART_EFC is latched or it faults.
bool native_race_effects_flare_420560(NativeRuntimeContext&,driving::PcMatrixStack&);
}
