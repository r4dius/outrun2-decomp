#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace outrun::platform {

// Key-44 owner (PC factory 0x441B10). These are only the recovered init and
// dispatch boundaries, not a replacement for its child objects or UI.
constexpr std::size_t TitleOwnerPcSize = 0x1d90u;
struct TitleOwnerGlobals {
    std::uint32_t scene_ids[3]{}; // PC 0x692B28/2C/30
    std::uint8_t pause_flag{};    // PC 0x84B200
};
using TitleOwnerChildInit = void(*)(void* user); // +0x9BC virtual +4
using TitleOwnerPause = void(*)(void* user);     // PC 0x440930
struct TitleOwnerServices {
    void* user{};
    TitleOwnerChildInit child_init{};
    TitleOwnerPause pause{};
};

using TitleOwnerConstructChild = void(*)(void* user,std::uint32_t pc_entry,
                                         std::uint8_t* child);
using TitleOwnerConstructArray = void(*)(void* user,std::uint32_t helper_pc,
                                         std::uint8_t* first,std::uint32_t element_size,
                                         std::uint32_t count,std::uint32_t constructor_pc,
                                         std::uint32_t destructor_pc);
struct TitleOwnerConstructorServices {
    void* user{};
    TitleOwnerConstructChild child{};
    TitleOwnerConstructArray array{};
};

// Parent-owned portion of 0x4D7140. Its nested constructors are mandatory
// services, not zero-filled approximations.
bool title_owner_construct_4d7140(std::uint8_t* object,std::size_t size,
                                  std::uint8_t& game_state_780270,
                                  std::uint8_t& title_flag_95b250,
                                  const TitleOwnerConstructorServices& services);

bool title_base_construct_48f480(std::uint8_t* object,std::size_t size,
                                 std::uint32_t& global_6591e4);
// A single PC ReadIO snapshot: axes returned by 453720(3..6), feature mask
// queried by 4536F0, and the separate device +8 held bits. No Switch key IDs.
struct FrontendInputSnapshot {
    std::array<std::int16_t,4> axes{};
    std::uint32_t feature_mask{};
    std::uint32_t device_held{};
};
using FrontendInputFeedback=bool(*)(void*,std::uint32_t key,std::int32_t argument);
bool frontend_input_axes_48f4f0(std::uint8_t*,std::size_t,const FrontendInputSnapshot&);
// The argument controls feedback, not input acceptance. Idle action is 12.
// 6591E4 is shared across ALL menu objects, not an object-local debounce.
bool frontend_input_action_48f5f0(std::uint8_t*,std::size_t,const FrontendInputSnapshot&,
                                 std::int32_t argument,std::uint32_t& global_6591e4,
                                 void* user,FrontendInputFeedback,std::uint32_t& result);
bool title_ui_resource_construct_465160(std::uint8_t* object,std::size_t size);
bool title_list_construct_4ed950(std::uint8_t* object,std::size_t size);
bool title_transform_construct_48e310(std::uint8_t* object,std::size_t size);
bool title_widget_construct_48e590(std::uint8_t* object,std::size_t size,
                                   std::uint32_t& global_6591e4);
bool title_controller_construct_48c490(std::uint8_t* object,std::size_t size,
                                       std::uint32_t& global_6591e4);
bool title_controller_init_48c5b0(std::uint8_t* object,std::size_t size);
// Generic 48C490 window used by the title and license context dialogs.
// 48CC00 consumes the real owner +DA0 delta, NOT the independent UI ms clock.
bool title_controller_motion_48cc00(std::uint8_t*,std::size_t,float owner_delta);
bool title_controller_move_48cb00(std::uint8_t*,std::size_t,float x,float y,float duration);
bool title_controller_rect_48d4e0(std::uint8_t*,std::size_t,float x,float y,float width,float height);
// Virtual +8 of the title controller. The animation update, input virtual,
// sound command and two widget ticks are explicit PC service boundaries.
using TitleControllerCall = bool(*)(void* user,std::uint32_t pc_entry,
                                   std::uint8_t* object,std::size_t offset,
                                   std::int32_t argument,std::uint32_t& result);
struct TitleControllerServices {
    void* user{};
    TitleControllerCall call{};
    std::uint32_t root_state{};
};
bool title_controller_tick_48d420(std::uint8_t* object,std::size_t size,
                                  const TitleControllerServices& services,
                                  std::uint32_t& result);

// Disassembly-backed parent effects of the key-44 initial stage at 0x4D5E40.
// The original function configures the title controller/widget, appends the
// authored menu records by root state/variant/manager, resolves the
// two text records 0x295/0x296, then advances owner +0x9AC from stage 0 to 1.
// This legacy metadata trace is NOT the complete UI implementation. The native
// window/list initializer is frontend_title_init_4d5e40 in frontend_window.hpp;
// production binding must use it before claiming an initialized title menu.
struct TitleOwnerInitialUiState4d5e40 {
    std::array<std::uint32_t,7> entries{};
    std::size_t entry_count{};
    std::array<std::uint32_t,2> text_ids{{0x295u,0x296u}};
    std::uint32_t root_state{};
};
struct TitleMenuGlobals {
    std::uint32_t root_state{},variant{1};
    bool manager_present{};
    std::uint32_t manager_child_key{}; // bounded resolution of manager +5C, child +8
    std::uint32_t feature_mask{};
    float delay_692c9c{};
};
TitleOwnerInitialUiState4d5e40 title_menu_records(const TitleMenuGlobals&);
bool title_menu_control_4d7300(std::uint8_t*,std::size_t,TitleMenuGlobals&,
                              const TitleControllerServices&,std::uint32_t& result);
bool title_menu_close_4d6090(std::uint8_t*,std::size_t,const TitleControllerServices&);
bool title_owner_initial_ui_4d5e40(std::uint8_t* object,std::size_t size,
                                   std::uint32_t root_state,
                                   TitleOwnerInitialUiState4d5e40& state,
                                   std::uint32_t variant=1,bool manager_present=false,
                                   std::uint32_t manager_child_key=0);
// Fully constructs the fixed key-44 object graph. The four 0x570AC0 array
// constructors are genuine PC no-ops; no dynamic UI resources are allocated.
bool title_owner_construct_complete_4d7140(std::uint8_t* object,std::size_t size,
                                            std::uint8_t& game_state_780270,
                                            std::uint8_t& title_flag_95b250,
                                            std::uint32_t& global_6591e4);

// Original 0x4D5D00 with explicit external services. Returns false without
// modifying state when the child callback or required pause service is absent.
bool title_owner_init_4d5d00(std::uint8_t* object,std::size_t size,
                              std::uint32_t singleton_state,std::uint32_t mode,
                              TitleOwnerGlobals& globals,
                              const TitleOwnerServices& services);

// Original 0x4D7260 switch after its child virtual +0xC update. Zero means
// no owner-state handler; the caller still owes that child update.
std::uint32_t title_owner_control_target_4d7260(std::uint32_t owner_state);

// Key-44 virtual +8. Every called PC child/handler remains an explicit
// service; the dispatcher never supplies a convenient success value.
using TitleOwnerTickCall = bool(*)(void* user,std::uint32_t pc_entry,
                                  std::uint8_t* object,std::size_t offset,
                                  std::uint32_t& result);
struct TitleOwnerTickServices {
    void* user{};
    TitleOwnerTickCall call{};
    std::uint32_t root_state{}; // singleton +0x218 for the 2/4 exit branch
};
bool title_owner_tick_4d8c50(std::uint8_t* object,std::size_t size,
                             const TitleOwnerTickServices& services,
                             std::uint32_t& result);

} // namespace outrun::platform
