#pragma once
#include <functional>
// PC 46C140 (event 0x2C display of the vehicle preview, via 49F500): the sun
// light 4082B0(2,0,0) diffuse scaled by the car brightness +58, 40D840(2),
// the car matrix +B0 pushed (409F90), the model draw 46AE70 executed
// against the renderer, the matrix popped (40A010), the light restored and
// 40D840(2) again.
#include "platform/pc_render_flush.hpp"
#include "platform/vehicle_model_draw.hpp"
namespace outrun::platform {
struct PcVehicleDisplayContext {
    PcFlushContext& flush;
    PcRenderContext& queue;
    PcEnvironmentRenderTables environment; // lights_899b98 is written (light 2 diffuse)
    std::uint8_t flags_79fcca{};
    std::uint32_t layer_7d25f0{};
    driving::PcMatrixStack& matrices;      // 409F90 / 40A010 stack
    driving::Bytes body_82e7f0{nullptr,0};
    std::int32_t game_mode_78026c{};
    std::uint8_t scene_82e7d4{};
    std::function<void(std::uint32_t pc)> unported{};  // draw-list leaves without a native renderer
    // Shadow volume leaves 422550 / 422740 (pc_shadow_volume) with the
    // current matrix loaded; false: reported through `unported`.
    std::function<bool(const PcVehicleDrawCall&)> shadow{};
};
// Executes a 46AE70 draw list: 405360, 4056D0, 4044F0, 404540, 4052B0,
// 4052C0 and 405350 (405890 on the opaque queue, not emptied).
void vehicle_draw_calls_execute(PcVehicleDisplayContext&,const std::vector<PcVehicleDrawCall>&);
void vehicle_display_46c140(PcVehicleDisplayContext&,driving::Bytes car);
// Display state of 46BD30 (DispPlCar): saved sun light words 7F942C..7F9458
// and the two phase floats 7F9424 / 7F9428.
struct PcRaceCarDisplayState {
    std::array<std::uint8_t,16> diffuse_7f944c{},specular_7f943c{},ambient_7f942c{};
    float phase_7f9424{},phase_7f9428{};
};
struct PcRaceCarDisplayInputs {
    std::uint32_t network_7f9460_60{};  // 55A930(7F9460)
    std::uint32_t network_800abc{};     // 46C550 second word
    std::uint8_t pause_780248{};        // 43F9C0
};
// PC 0x46BD30 (race car display, via 49F3E0): sun light 4082B0(2,0,0)
// diffuse scaled by +58 (and, in a network race with +D10 == 1, the pulsing
// light 410740/4107A0), 40D840(2), 409F90(+B0), the network bob (46C550),
// 469600(car, +11, 0), 40A010, light restored, 40D840(2). Unported draw-list
// leaves (shadow volumes 422550/422740) are passed to `unported`.
void vehicle_race_display_46bd30(PcVehicleDisplayContext&,PcRaceCarDisplayState&,const PcRaceCarDisplayInputs&,
    driving::Bytes car,const std::function<void(std::uint32_t pc)>& unported);
}
