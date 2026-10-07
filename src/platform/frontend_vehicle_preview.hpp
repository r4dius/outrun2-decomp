#pragma once
#include "frontend_vehicle_loader.hpp"
#include "vehicle_creation_queue.hpp"
namespace outrun::platform {
// Views of the ONE PC loader, shared vehicle and scene-list table. No private
// copy of 83039C or 799D18: selection/colour changes must reach their consumers.
struct FrontendVehiclePreviewServices {
    driving::Bytes loader{nullptr,0};
    driving::Bytes vehicle{nullptr,0};
    driving::Bytes scene_lists{nullptr,0}; // 7D26A8; entries contain PC IDs, not host pointers
    void* user{};
    bool (*call)(void*,unsigned pc,const unsigned*,std::size_t){};
    bool (*ready_448960)(void*,unsigned resource,bool& ready){};
    unsigned fault{};
    VehicleCreationQueue* creation{};
};
bool frontend_vehicle_preview_construct_48bf00(driving::Bytes);
bool frontend_vehicle_preview_init_48c170(driving::Bytes,unsigned index,bool unlocked,FrontendVehiclePreviewServices&);
bool frontend_vehicle_preview_open_48c220(driving::Bytes,unsigned model,unsigned colour,bool unlocked,FrontendVehiclePreviewServices&);
bool frontend_vehicle_preview_close_48c260(driving::Bytes,FrontendVehiclePreviewServices&);
bool frontend_vehicle_preview_select_48c290(driving::Bytes,unsigned index,unsigned colour,bool unlocked,FrontendVehiclePreviewServices&);
bool frontend_vehicle_preview_tick_48c3f0(driving::Bytes,FrontendVehiclePreviewServices&);
bool frontend_vehicle_preview_suspend_48c450(driving::Bytes,FrontendVehiclePreviewServices&);
}
