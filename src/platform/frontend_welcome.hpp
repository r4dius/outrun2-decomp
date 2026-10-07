#pragma once
#include "driving/pc_common_control.hpp"

namespace outrun::platform {
// Calls preserve PC argument order. Owner methods refer to the frontend
// singleton; 447000 refers to its embedded object at +51C.
struct FrontendWelcomeServices {
    void* user{};
    std::uint32_t (*call)(void*,std::uint32_t,const std::uint32_t*,std::size_t){};
};
bool frontend_welcome_init_4c5180(driving::Bytes,const FrontendWelcomeServices&);
std::uint32_t frontend_welcome_control_4c5210(driving::Bytes,const FrontendWelcomeServices&);
void frontend_welcome_suspend_4c5350(driving::Bytes,const FrontendWelcomeServices&);
}
