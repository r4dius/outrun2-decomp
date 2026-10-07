#pragma once
// Game-side input update of the PC frame loop (0x453BB0 for player 0):
// 0x453640 switch record 7D6770 (held/previous/pressed/released feature bits)
// and 0x453860 analogue channels 7D6810 (+4 current, +8 previous, pedal
// edge flags +1C/+2C) with the steering filter 7D6884 and the digital
// steering smoother 0x4537C0. The platform device record (0x8999C0: +4 button
// bits, +94 signed 16-bit axis words) is what the Switch adapter fills; the
// configuration records are the EXE tables 5A7B30 (keyboard) / 5A7B50[n]
// (joystick): steer/accel/brake axis indices, shift-up/down/view masks, flags.
#include <array>
#include <cstdint>
namespace outrun::platform {
struct PcInputDevice {                         // 0x8999C0 view
    std::uint32_t buttons_04{};
    std::array<std::int16_t,0x30> axes_94{};   // +0x94 + index*2
};
struct PcInputConfig {                         // 0x1C-byte record
    std::uint32_t steer_axis{},accel_axis{},brake_axis{};
    std::uint32_t shift_up_mask{},shift_down_mask{},view_mask{},flags{};
};
// EXE tables: keyboard 5A7B30 and the four joystick configurations 5A7B50.
PcInputConfig pc_input_keyboard_config();
PcInputConfig pc_input_joystick_config(unsigned index);
struct PcInputSwitchRecord { std::uint32_t held{},previous{},pressed{},released{}; }; // 7D6770 + player*0x10
struct PcInputAnalog {                                                               // 7D6810 + player*0x70
    std::array<std::int32_t,7> current{},previous{};   // +4/+8 of each 0x10 channel
    std::int32_t accel_edge_1c{},brake_edge_2c{};
};
void pc_input_switch_453640(PcInputSwitchRecord&,const PcInputDevice&,const PcInputConfig&);
// filter_7d6884: per-player steering filter; option_7c24cb: steering speed
// option (race table 5A7C30 row); game_mode_78026c selects 5A7C30 (16) or 5A7CF0.
void pc_input_analog_453860(PcInputAnalog&,float& filter_7d6884,const PcInputDevice&,const PcInputConfig&,
                            std::int8_t option_7c24cb,std::int32_t game_mode_78026c);
std::int32_t pc_input_digital_steer_4537c0(std::int32_t target,std::int32_t previous,std::int8_t option_7c24cb);
}
