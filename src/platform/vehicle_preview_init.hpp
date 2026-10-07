#pragma once
#include <functional>
#include "vehicle_body_init.hpp"
#include "driving/pc_common_control.hpp"
#include "driving/pc_environment_blend.hpp"
namespace outrun::platform {
// Services and shared state consumed by PC 4A7270 (event8 function2C init).
// Every root is the caller's shared owner; nothing here is a preview copy.
struct VehiclePreviewInit {
    // 4A5830 queue consumer.
    VehicleCreationQueue& queue;
    const DrivingDataPack& driving;
    VehicleParameterSelection selection;
    driving::Bytes shared_83db30;
    // Parameter view the constructor selects (event +2B4); filled here.
    driving::Bytes parameters;
    // 449F50 scene environment against the shared car 799D18.
    driving::PcEnvironmentFrame& environment;
    const driving::CourseCollisionTables& primary_course;
    driving::PcEnvironmentBlendContext& environment_context;
    // 4A69F0/519300 shared body work 82E7F0 and course world.
    driving::Bytes body_82e7f0;
    driving::CourseWorldQuery& world;
    std::array<float,2> area_yaw{}; // 7D3128 / 7D317C
    std::uint8_t transmission_830374{};
    const driving::PcRankProviderServices* rank{}; // 45A2B0, used only when selection 7DE418 > 1
    std::uint16_t yaw_841fa0{};
};
// Complete 4A7270 orchestration. Returns false when the original constructor
// rejects its queue/data (the fault stays latched in the queue); malformed
// views throw before any later step runs.
bool vehicle_preview_init_4a7270(driving::Bytes event,VehiclePreviewInit&,
    VehicleParameterChoice* choice=nullptr);
// PC 4A6ED0 (GamePlCar_Init, event 8 function 0 in a race). Same constructor
// and body/ground set-up as 4A7270 without the preview environment; the start
// slot comes from 5C2560 (single) or 5C24D0[45A2B0(+1054)], and the variant-4
// grid (4963B0/456E00/43F730) is an explicit service. The shadow volume
// allocation 46BA20 (protected) and the environment map 46BBC0 are renderer
// services.
struct GamePlCarInit {
    VehicleCreationQueue& queue;
    const DrivingDataPack& driving;
    VehicleParameterSelection selection;
    driving::Bytes shared_83db30;
    driving::Bytes parameters;
    driving::Bytes body_82e7f0;
    driving::CourseWorldQuery& world;
    std::array<float,2> area_yaw{};                  // 7D3128 / 7D317C
    std::uint8_t transmission_830374{};              // 48B180
    const driving::PcRankProviderServices* rank{};   // 45A2B0
    std::uint32_t global_841b50{};                   // written from 5C3604 (0)
    std::uint32_t flag_680bd0{};                     // written 1
    // 780258 == 4: 4963B0 / 456E00 / 43F730 grid (returns false when absent).
    std::function<bool(driving::Bytes car,driving::CourseProbe& position)> grid_variant4;
    std::function<void(driving::Bytes car)> shadow_46ba20,envmap_46bbc0;
};
bool game_pl_car_init_4a6ed0(driving::Bytes event,GamePlCarInit&,VehicleParameterChoice* choice=nullptr);
// PC 4A7080 (event 8 function 0x2B init: the OUTRUN2SP car-select car). The 4A7270
// constructor / body / ground / start-slot steps, then 4F6E40, 46BA20 (shadow volume),
// 4A2EE0 and 46F350 (other-car reset, a service here), the display yaw -31500 and the
// position (mode 3 demo route: 49EED0 2/3/4 and 49EEE0 choose (2, 0, -8)).
struct VehicleArcadeInit {
    std::function<void(driving::Bytes car)> shadow_46ba20,reset_46f350;
    std::uint32_t mode_78026c{},query_49eed0{};std::uint8_t query_49eee0{};
};
bool vehicle_arcade_init_4a7080(driving::Bytes event,VehiclePreviewInit&,const VehicleArcadeInit&);
// Start positions read by 4A7270: 5C2560 (single) and 5C24D0[rank] (12 slots).
driving::CourseProbe vehicle_preview_start_position(std::uint8_t loading_scene,std::uint8_t rank);
}
