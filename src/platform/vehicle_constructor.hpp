#pragma once
#include "vehicle_creation_queue.hpp"
#include "driving_data_pack.hpp"
namespace outrun::platform {
// Full PC vehicle allocation, not the smaller arithmetic-test view (0x1000).
inline constexpr std::size_t PcVehicleObjectBytes=0x10f0;
struct VehicleParameterSelection {
    std::uint8_t variant_83036d{};
    std::uint8_t loading_scene_7de418{};
    std::uint32_t game_mode_780258{};
    std::uint8_t flag_65a7ac{};
    std::uint32_t flag_8514a0{};
};
struct VehicleParameterChoice {
    unsigned model{},base_model{},map{},column{},pc_address{};
};
// Resolves original 5051D0. The PC address is an ID only; never dereference it.
bool vehicle_parameter_choice_5051d0(const DrivingDataPack&,unsigned model,
    const VehicleParameterSelection&,VehicleParameterChoice&);
bool vehicle_clear_4874f0(driving::Bytes event,bool clear);
bool vehicle_gear_thresholds_487570(driving::Bytes event,driving::Bytes parameters);
bool vehicle_reset_timer_455a90(driving::Bytes event);
// Original shared constructor, including queue consumption and all its leaves.
// Uses the actual model tables and the caller's shared 83DB30 buffer. No fixtures.
bool vehicle_construct_4a5830(driving::Bytes event,VehicleCreationQueue&,
    const DrivingDataPack&,const VehicleParameterSelection&,
    driving::Bytes shared_83db30,VehicleParameterChoice* result=nullptr);
}
