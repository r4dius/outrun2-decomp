#pragma once
#include "frontend_carousel.hpp"
#include "frontend_vehicle_data.hpp"
#include "title_owner.hpp"
namespace outrun::platform {
struct FrontendVehiclePreviewServices;
struct FrontendVehicleMenu {
    std::array<std::uint8_t,0x1674> object{};
    // PC process globals: not reset by construction/destruction of the owner.
    std::int8_t cursor_84b0e8{},variant_84b0e9{};
    bool initialized_84b0ea{};
    std::array<std::int8_t,30> colours_68eca8{VehicleDefaultColours};
    unsigned fault{};
    bool constructed{};
};
struct FrontendVehicleMenuServices {
    FrontendUiResources& ui;
    driving::Bytes root;
    driving::PcUiNotifyGlobals& globals;
    const PcLicense& profile;
    const FrontendInputSnapshot& input;
    unsigned& repeat;
    float timer{};
    bool colour_held_4536c0{}; // held query, distinct from 4536F0 edge/features
    void* user{};
    // Preview, transition, committed selection and window services. Required
    // calls must execute their real provider; absence latches a fault.
    bool (*external)(void*,unsigned pc,driving::Bytes child,const unsigned*,std::size_t){};
    unsigned missing{};
    // When bound, preview calls run the actual lifecycle against shared data.
    // The external callback remains for other owners/services and oracle leaves.
    FrontendVehiclePreviewServices* preview{};
};
bool frontend_vehicle_menu_construct_4c8de0(FrontendVehicleMenu&,unsigned& repeat);
bool frontend_vehicle_menu_init_4c9010(FrontendVehicleMenu&,FrontendVehicleMenuServices&);
bool frontend_vehicle_menu_control_4c9290(FrontendVehicleMenu&,FrontendVehicleMenuServices&,unsigned& action);
bool frontend_vehicle_menu_display_4c8d70(FrontendVehicleMenu&,FrontendVehicleMenuServices&);
bool frontend_vehicle_menu_suspend_4c8d90(FrontendVehicleMenu&,FrontendVehicleMenuServices&);
}
