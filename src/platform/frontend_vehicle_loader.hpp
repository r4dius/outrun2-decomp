#pragma once
#include "driving/pc_driving.hpp"
#include <array>
namespace outrun::platform {
// Byte-exact original three-slot object at 83039C. 30 is the empty model,
// not a fourth slot. Resource ownership belongs to the supplied loader.
struct FrontendVehicleLoader {
    std::array<std::uint8_t,0x34> object{};
    unsigned fault{};
};
struct FrontendVehicleLoaderServices {
    void* user{};
    bool (*request_448ad0)(void*,unsigned resource,unsigned mode){};
    bool (*ready_448960)(void*,unsigned resource,bool& ready){};
    bool (*release_448990)(void*,unsigned resource){};
};
bool frontend_vehicle_loader_init_48bf20(FrontendVehicleLoader&,const FrontendVehicleLoaderServices&);
bool frontend_vehicle_loader_tick_48bfe0(FrontendVehicleLoader&,const FrontendVehicleLoaderServices&);
bool frontend_vehicle_loader_ready_48bf80(const FrontendVehicleLoader&,unsigned model);
bool frontend_vehicle_loader_ready_48bf80(driving::Bytes shared_loader,unsigned model);
bool frontend_vehicle_loader_empty_48bfc0(const FrontendVehicleLoader&);
}
