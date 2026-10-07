#pragma once
// Owners of the race variants the retail frontend has no menu path to (the host
// reaches them with OR2_HOST_VARIANT): variant 8 (event 0x194, function 0x23:
// drift points by sector / stage into the records 4483D0) and variant 9 (Race
// Attack, event 0x192, function 0x21: RaceAttack.bin records, rivals spawned per
// stage, ranks by stage and the rank level).
#include "driving/pc_matrix_stack.hpp"
#include <cstddef>
#include <cstdint>
#include <functional>
namespace outrun::platform {
struct NativeRuntimeContext;
class PcRaceMemory;
class PcSceneRenderer;
// Event callbacks 495740 / 495790 / 4957A0 and 4B0400 / 4B0780 / 4B07A0; false when
// not one of them.
bool native_race_variant_invoke(NativeRuntimeContext&,std::uint32_t callback,driving::PcMatrixStack&);
// Race manager stage end 495610 (variant 8 route gate set).
void native_race_variant8_stage_end_495610(NativeRuntimeContext&);
// Variant 8 goal 4956C0 (race end 49D175): the multi-stage drift record (448230).
void native_race_variant8_goal_4956c0(NativeRuntimeContext&);
// Variant 9 accessors for the other modules: 4B00D0 / 4B00E0 (8421C0 != 0 / == 5),
// 4B00F0 last rank, 4B0100(stage) rank of a stage, 4B0190 level, 4B02A0 target
// segment, 4B0110(stage key, level) stage hook, 4B01A0(speed, rank) rival speed
// (float bits).
std::uint32_t native_race_attack_value(NativeRuntimeContext&,std::uint32_t pc,const std::uint32_t* args,std::size_t count);
// The variant 9 words 686254..686273 and the relocated RaceAttack.bin.
void native_race_attack_map(NativeRuntimeContext&,PcRaceMemory&);
// 4B0390: the models of the RaceAttack racers flagged in models[size] (49B870 variant 9).
bool native_race_attack_models_4b0390(NativeRuntimeContext&,std::uint8_t* models,std::size_t size);
// 4B0490 (display 4B0790): the road markers on the renderer and, in state 5, the rank
// markers through ranks (the NAVI module's 4B04D0); false when ranks failed.
bool native_race_attack_display_4b0490(NativeRuntimeContext&,PcSceneRenderer&,const std::function<bool()>& ranks);
}
