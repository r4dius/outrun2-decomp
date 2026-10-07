#pragma once
#include <functional>
// Native backing of the PC objects shared by the car select (parent key 10,
// owner 4C8DE0), its preview events (event 8 CAR01 function 0x2C, event 385
// CAMERA function 0x0D) and the renderer:
//   car 799D18 (event 8 work, 0x10F0), body 82E7F0, camera 79FE10 (event 385
//   work, 79F574 points to it), 83DB30, the constructor parameter view, the
//   sun list words 7D26A8 (shared with SCN_ENV), the select table 844A08
//   (common/sel_dl_edit0.tgt), the resource table 7C2800 used by the model
//   preloader (448AD0 request / 448960 ready / 448990 release), the clear
//   colour 89BD5C (40EC60) and the course mode byte 7D31D0 (44C0A0).
// The functions taking the runtime are declared in native_runtime.hpp.
#include "platform/driving_data_pack.hpp"
#include "platform/frontend_vehicle_menu.hpp"
#include "platform/frontend_vehicle_preview.hpp"
#include "platform/vehicle_body_init.hpp"
#include "platform/vehicle_constructor.hpp"
#include "driving/pc_common_control.hpp"
#include <array>
#include <vector>
namespace outrun::platform {
struct FrontendCarSelect {
    FrontendVehicleMenu menu;
    std::array<std::uint8_t,PcVehicleObjectBytes> car_799d18{};
    std::array<std::uint8_t,PcVehicleBodyBytes> body_82e7f0{};
    std::array<std::uint8_t,0x400> camera_79fe10{};
    std::array<std::uint8_t,0x3c> shared_83db30{};
    std::array<std::uint8_t,DrivingParameterViewBytes> parameters{};
    std::array<std::uint8_t,12> sun_lists_7d26a8{};
    std::vector<std::uint8_t> select_table_844a08;
    std::array<driving::PcRuntimeResourceEntry448ad0,0x223> resources_7c2800{};
    std::uint32_t pending_7cc1d8{};
    std::uint16_t yaw_841fa0{};
    std::uint32_t random_state{};
    std::uint32_t clear_colour_89bd5c{};
    std::uint8_t course_mode_7d31d0{};
    std::uint32_t flag_8514a0{};      // set by START 49DBAB (5051C0); 0 until a START ran
    std::uint8_t transmission_830374{};
    unsigned repeat{};
    unsigned fault{};                 // latched failing PC (0 = none)
    std::uint32_t scene_releases{};   // 44C3D0/44A1A0/4F2210: nothing is allocated natively yet
    std::uint32_t preview_inits{},preview_controls{};
    // Race (event 8 function 0, GamePlCar): 4A6ED0 globals and counters.
    std::uint32_t global_841b50{},flag_680bd0{};
    std::uint32_t race_inits{},race_displays{},race_dests{};
    std::uint32_t shadow_unported{},envmap_unported{};   // 46BA20/46BB20 without a renderer / 46BBC0 (not ported)
    // Car shadow volumes 46BA20 / 46BB20 on the renderer (pc, car work);
    // false when the renderer could not serve them.
    std::function<bool(std::uint32_t pc,driving::Bytes car)> shadow_service;
    driving::EasyLctPredictionState race_prediction{};
    std::array<std::uint32_t,5> slot_calls{};   // virtual slots 0,4,8,12,16
    FrontendCarSelect(){for(auto& r:resources_7c2800)r.status_14=7u;}
};
}
