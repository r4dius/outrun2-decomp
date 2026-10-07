#pragma once
#include "driving/pc_matrix_stack.hpp"
#include <array>
#include <vector>
namespace outrun::platform {
// One render-leaf call issued by PC 0x46AE70, with the current matrix at the
// time of the call. pc is the original leaf: 405360/4056D0 draw an object ID,
// 4044F0 sets a render state, 404540/4052B0/4052C0/405350 are state pushes.
struct PcVehicleDrawCall {
    std::uint32_t pc{};
    std::uint32_t argc{};
    std::array<std::uint32_t,6> args{};
    std::array<std::uint8_t,64> matrix{};
};
struct PcVehicleModelDraw {
    driving::Bytes car{nullptr,0};         // shared vehicle object
    std::int32_t model{};                  // argument 1 (46C140 passes +11)
    std::int32_t variant{};                // argument 2 (0 from 46C140)
    driving::Bytes body_82e7f0{nullptr,0}; // wheel angle words at wheel +30/+32
    std::int32_t debug_alt_64d198{-1};     // negative in the shipped build
    std::int32_t game_mode_78026c{};
    std::uint8_t scene_82e7d4{};
};
// PC 0x46AE70 as a draw list. The two protected bridges are reproduced from
// black-box measurements (steering word +204, model-5 roof angle to +2FC).
// Writes the same car fields as the PC (+04 flags, +2FC, and +B0 row
// normalisation in the 449640 path).
void vehicle_model_draw_46ae70(const PcVehicleModelDraw&,driving::PcMatrixStack&,
    std::vector<PcVehicleDrawCall>& calls);
// PC 0x469600 (DispPlCarModel, the race car draw of 46BD30) as a draw list.
// Same layouts as 46AE70 but its own view-mode branches, the shadow volume
// leaves 422550(work, light+44) / 422740(work) (light = 4082B0(1,0,0), whose
// PC address is recorded) and the two protected bridges at 46989F / 469CDB
// (both compare the view mode with 1, measured against the original).
struct PcRaceModelDraw {
    driving::Bytes car{nullptr,0};         // race car (non-null; the null path faults in the PC)
    std::int32_t model{};
    std::int32_t variant{};
    driving::Bytes body_82e7f0{nullptr,0};
    std::int32_t game_mode_78026c{};
    std::uint8_t scene_82e7d4{};
};
void vehicle_race_model_draw_469600(const PcRaceModelDraw&,driving::PcMatrixStack&,
    std::vector<PcVehicleDrawCall>& calls);
// PC 0x469FF0 (the arcade ending's car, 46C060 via event 8 function 0x2D display 49F4C0) as a
// draw list: layout 5B2F68[model] (the caller passes +11), wheel steering from +32 (front) and
// spin from +40..+46, the shadow volume leaves 422550(+2BC / +2C4, light 1 +44) and 422740(+2BC),
// the model-5 roof (+2FC) and the convertible top. The protected bridge at 46A190 (between
// 4082B0(1,0,0) and 422550) passes [car+2C4] as 469600's bridge 46989F does.
void vehicle_ending_model_draw_469ff0(driving::Bytes car,std::int32_t model,driving::PcMatrixStack&,
    std::vector<PcVehicleDrawCall>& calls);
// PC 0x46A560 (the OUTRUN2SP car-select car, 46C090 via event 8 function 0x2B display 49F4D0)
// as a draw list: 46AE70's layouts and wheels without the debug alternate draws, with the
// shadow volume leaves 422550(+2BC / +2C4, light 1 +44) of view mode 1 and the model-5 roof
// angle computed in place. A null car faults in the PC (its colour bytes are read).
void vehicle_select_model_draw_46a560(const PcVehicleModelDraw&,driving::PcMatrixStack&,
    std::vector<PcVehicleDrawCall>& calls);
// 449640: normalise the three rows in place, then extract Euler angles.
std::array<float,3> matrix_angles_449640(driving::Bytes matrix);
}
