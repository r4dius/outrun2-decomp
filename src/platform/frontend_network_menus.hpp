#pragma once
// The network menus of the frontend (reports/decomp/network.md): parent key 6
// (factory 441570, 0x14C bytes, 4C5CD0, vtable 5C96C0) chooses ONLINE (key 7,
// or the sign-in gate 22) or LAN (key 8); key 8 (factory 441630, 0x1F4 bytes,
// 4C60B0, vtable 5C97A8): JOIN GAME searches (454140(0, 1): the 988E8C broadcast) and
// CREATE GAME hosts (454220(1), 454100(0)) a LAN session in the network layer (pc_network.cpp).
// Key 7 (factory 4415D0, 0x14C bytes, 4C5EF0, vtable 5C9744): the ONLINE menu, three choices
// (68C81C = 0 / 1 / 2: 454140(1, 1), the sign-in screen 0x1A, 454100(1)).
#include "frontend_ui_resources.hpp"
#include "title_owner.hpp"
#include <string>
namespace outrun::platform {
constexpr std::size_t PcNetworkMenuBytes=0x14c,PcLanMenuBytes=0x1f4;
struct NetworkMenuServices {
    FrontendUiResources& ui;
    driving::Bytes root;                       // the frontend owner 7B17E8 (4035F0)
    const FrontendInputSnapshot& input;
    unsigned& repeat;                          // 6591E4
    float timer{};
    std::uint32_t& notice_63aa6c;              // the sign-in gate's caller (1: ONLINE)
    bool online_7d68bc{};                      // signed in to the online service
    // Network layer: 830C30 (service ready), the 830C10 player name (copied from the
    // active license 7C23E0) and the session calls 454100 / 454140 / 454220 (arguments).
    void* user{};
    bool (*ready_830c30)(void*){};
    void (*player_name)(void*){};
    bool (*session)(void*,std::uint32_t pc,std::uint32_t a0,std::uint32_t a1){};
    void (*put32)(void*,std::uint32_t address,std::uint32_t value){};   // network memory (7D68D8)
    // 48FC70(0) on 659930: the network error notice (the service is not ready).
    bool (*notice)(void*){};
    // Key 7: byte stores in the network memory (84AA00) and its 830C20 -> 830C10 name copy.
    void (*put8)(void*,std::uint32_t address,std::uint8_t value){};
    void (*copy_name)(void*){};
    unsigned missing{};
};
// key 6 / 7 / 8: 0 destroy, 4 init, 8 control, 12 display (49A650, nothing), 16 suspend.
bool network_menu_construct(std::uint8_t*,std::size_t,unsigned key,unsigned& repeat);
bool network_menu_slot(std::uint8_t*,std::size_t,unsigned key,unsigned slot,NetworkMenuServices&,unsigned& result);
}
