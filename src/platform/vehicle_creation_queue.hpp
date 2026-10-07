#pragma once
#include "driving/pc_driving.hpp"
#include <array>
namespace outrun::platform {
// Shared 841AC0..841F9F request/list state. The original reset deliberately
// leaves the old sixteen request records intact and resets their count only.
struct VehicleCreationQueue {
    std::array<std::uint8_t,0x4e0> object{};
    unsigned fault{};
};
bool vehicle_creation_reset_49fa60(VehicleCreationQueue&);
bool vehicle_creation_append_49fa80(VehicleCreationQueue&,unsigned event,unsigned lane,unsigned model,unsigned colour,unsigned flags);
struct VehicleCreationServices {
    void* user{};
    bool (*event_setup_440110)(void*,unsigned event,unsigned function){};
};
bool vehicle_creation_preview_4406f0(VehicleCreationQueue&,unsigned model,unsigned colour,unsigned hidden,const VehicleCreationServices&);
// The offline one-player branch only; callers must gate scene/variant first.
bool vehicle_creation_offline_440380(VehicleCreationQueue&,unsigned model,unsigned colour,unsigned player_slot,const VehicleCreationServices&);
// The LAN branch (7DE418 > 1 or variant 4): lanes 8.. for the CommRace slots (59C6B0 row of
// the local slot 7DD138), event 8 = function 0x26, the other cars 0x67 (variant 4) / 0x54.
bool vehicle_creation_network_440380(VehicleCreationQueue&,unsigned count,unsigned self,unsigned variant,
                                     const std::uint8_t* commrace_7de418,std::size_t size,const VehicleCreationServices&);
}
