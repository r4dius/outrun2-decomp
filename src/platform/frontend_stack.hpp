#pragma once
#include "driving/pc_common_control.hpp"
namespace outrun::platform {
struct FrontendStackServices {
    void* user{};
    std::uint32_t (*create)(void*,std::uint32_t){};
    std::uint32_t (*invoke)(void*,std::uint32_t,std::uint32_t){};
    void (*release)(void*,std::uint32_t){};
    std::uint32_t (*key)(void*,std::uint32_t){};
    // 4432B0 (return transition token), 4447D0 (open), 444880 (restore).
    std::uint32_t (*ui)(void*,std::uint32_t,std::uint32_t){};
};
void frontend_close_overlays_4430b0(driving::Bytes,const FrontendStackServices&);
bool frontend_push_444fe0(driving::Bytes,std::uint32_t,const FrontendStackServices&);
bool frontend_pop_444f40(driving::Bytes,const FrontendStackServices&);
// Target has already been resolved by 48F4E0. Returns the original status:
// 1 only if a restored child's initializer fails; 0 otherwise.
std::uint32_t frontend_unwind_4448c0(driving::Bytes,std::uint32_t,const FrontendStackServices&);
}
