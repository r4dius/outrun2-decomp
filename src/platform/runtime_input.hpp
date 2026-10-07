#pragma once
#include "platform/race_input.hpp"
#include "platform/pc_input_devices.hpp"
#include <cstdint>
#include "platform/title_owner.hpp"

namespace outrun::platform {

// Platform-neutral controller sample consumed at the recovered PC ReadIO
// boundary (0x453BB0).  Pedals retain the game's 0..255 domain and steering
// retains the signed byte domain later shifted into event+0x202.
struct NativeInputState {
    FrontendInputSnapshot frontend{};
    // Platform device record 0x8999C0 for the game-side 453BB0 update (a
    // Switch pad presented as the PC joystick of configuration 5A7B50[0]).
    PcInputDevice pc_device{};
    // The pad as the PC DirectInput joystick (pc_input_devices.hpp) and whether
    // the race reads it through the PC layer (406FA0) instead of pc_device.
    PcDirectInputPad pc_pad{};
    bool pc_input_layer{},pc_record_used{};
    bool connected{};
    std::int32_t steering{};       // -128..127
    std::uint32_t accelerator{};   // 0..255
    std::uint32_t brake{};         // 0..255
    bool shift_up{};
    bool shift_down{};
    bool menu_left{};
    bool menu_right{};
    bool menu_up{};
    bool menu_down{};
    bool menu_confirm{};
    bool menu_cancel{};
    bool menu_preview{}; // explicit diagnostic GAME shortcut
    bool menu_start_probe{}; // explicit START probe, not the original menu path
    bool start{};
    bool exit_requested{};
    std::uint64_t raw_buttons_held{};
    std::uint64_t raw_buttons_down{};
    std::uint64_t raw_buttons_up{};
    std::int32_t left_stick_x{};
    std::int32_t left_stick_y{};
    std::int32_t right_stick_x{};
    std::int32_t right_stick_y{};
};

using NativeInputSample = void(*)(void* user,NativeInputState& state);

struct NativeRuntimeInput {
    void* user{};
    NativeInputSample sample{};
    std::uint32_t sample_calls{};
};

inline bool runtime_input_ready(const NativeRuntimeInput& input){
    return input.sample!=nullptr;
}

inline void runtime_input_sample(NativeRuntimeInput& input,NativeInputState& state){
    if(!runtime_input_ready(input)){state=NativeInputState{};return;}
    NativeInputState next{};
    input.sample(input.user,next);
    state=next;
    ++input.sample_calls;
}

} // namespace outrun::platform
