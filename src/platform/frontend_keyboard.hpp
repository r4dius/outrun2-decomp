#pragma once
#include "frontend_ui_resources.hpp"
#include <string>
#include <string_view>

namespace outrun::platform {
constexpr std::size_t PcKeyboardBytes=0x70c;
// Original 468880/469130 controller. Focus is a platform boundary: on PC it
// disables the matching raw-keyboard device; it must not be silently omitted.
struct FrontendKeyboardServices {
    void* user{};
    bool (*input)(void*,driving::Bytes,std::int32_t&){};
    bool (*sound)(void*,std::uint32_t){};
    bool (*focus)(void*,bool acquire){};
    std::uint32_t selected{}; // PC 7F9420, shared by navigation and pointer input
    bool focused{};          // native counterpart of active keyboard 95AF38
};
bool keyboard_construct_468e40(driving::Bytes,std::uint32_t& repeat);
std::string keyboard_name_468770(driving::Bytes);
bool keyboard_name_468710(driving::Bytes,std::string_view);
void keyboard_limits_468780(driving::Bytes,std::int32_t minimum,std::int32_t maximum);
bool keyboard_append_468d80(driving::Bytes,std::uint8_t);
std::uint8_t keyboard_character_468d40(driving::Bytes,unsigned cell);
bool keyboard_position_4687c0(driving::Bytes,FrontendUiResources&,float,float);
bool keyboard_init_468880(driving::Bytes,FrontendUiResources&,FrontendKeyboardServices&);
bool keyboard_move_468c00(driving::Bytes,FrontendUiResources&,FrontendKeyboardServices&,unsigned direction);
bool keyboard_suspend_468f20(driving::Bytes,FrontendUiResources&,FrontendKeyboardServices&);
bool keyboard_finish_469030(driving::Bytes,FrontendUiResources&,FrontendKeyboardServices&,bool accept);
bool keyboard_tick_469130(driving::Bytes,FrontendUiResources&,FrontendKeyboardServices&,std::uint32_t& result);
}
