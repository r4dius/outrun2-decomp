#pragma once
// Parent key 16 (factory 441E10, 0x1F4 bytes): the transmission choice shown
// after a car is confirmed. Owner 4DD410, vtable 5CD8D4: init 4DD250,
// control 4DD4A0, display 49A650 (RET), suspend 4DD2E0, input 48F5F0.
// Two-entry carousel at +34 over table 5CD868 (four 440045 animation
// ranges), caption resource at +14C, latches +1EC (confirmed) / +1ED (leaving).
#include "platform/frontend_carousel.hpp"
#include "platform/title_owner.hpp"
namespace outrun::platform {
struct FrontendTransmission {
    std::array<std::uint8_t,0x1f4> object{};
    unsigned fault{};
    bool constructed{};
};
struct FrontendTransmissionServices {
    FrontendUiResources& ui;
    driving::Bytes root;                          // owner 4035F0 (440F70/441130/440DF0)
    driving::PcFloatTransitionGlobals& transition;
    const FrontendInputSnapshot& input;
    unsigned& repeat;
    std::uint32_t& selection_84b210;              // 5..8: confirmed/back x first/second entry
    std::uint8_t& transmission_830374;            // 48B170
    float timer{};
    unsigned missing{};
};
bool frontend_transmission_construct_4dd410(FrontendTransmission&,unsigned& repeat);
bool frontend_transmission_init_4dd250(FrontendTransmission&,FrontendTransmissionServices&);
// result: 0 stay, 1 proceed (owner +4 cleared by 440DF0), 2 back.
bool frontend_transmission_control_4dd4a0(FrontendTransmission&,FrontendTransmissionServices&,unsigned& result);
bool frontend_transmission_suspend_4dd2e0(FrontendTransmission&,FrontendTransmissionServices&);
}
