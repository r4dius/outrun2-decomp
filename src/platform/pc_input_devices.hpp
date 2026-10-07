#pragma once
// PC input device layer (DirectInput): the device objects 8606D4[95AEC4]
// (joystick class 402640, vtable 624B30; keyboard class 403660, vtable 624B88,
// always the last device), their creation 403DE0 / 403EF0 at device setup, the
// saved assignments 4040F0 (common save 7C211C: four 0xB0 records keyed by
// the device instance GUID), and the per-frame update 406FA0 (poll of the
// selected device and the keyboard, then 407430 / 4077F0 / 407880 into the
// platform record 8999C0) that 453BB0 runs first, and the Controls
// configuration screen. The objects keep their PC layout in this layer's own
// memory; DirectInput is a fake: the Switch pad is one joystick with the layout
// of an XInput pad seen through DirectInput, the keyboard reads no keys.
#include "platform/race_input.hpp"
#include <array>
#include <cstdint>
#include <string>
namespace outrun::platform {
struct NativeRuntimeContext;
// DIJOYSTATE of the pad: lX lY lZ lRx lRy lRz slider0 slider1 (-32768..32767,
// the DIPROP_RANGE 402F80 sets), POV 0 in hundredths of a degree (-1 centred),
// buttons 0x80 = down.
struct PcDirectInputPad {
    std::array<std::int32_t,8> axes{};
    std::uint32_t pov{0xffffffffu};
    std::array<std::uint8_t,32> buttons{};
};
void native_pc_input_set_pad(NativeRuntimeContext&,const PcDirectInputPad&);
// 403DE0 (41779B, after the D3D device): DirectInput8Create, EnumDevices
// (game controllers -> 403EF0), then the keyboard. Runs once.
bool native_pc_input_create_403de0(NativeRuntimeContext&);
// 4040F0 (4C5234, after the save is read): saved assignments per device.
bool native_pc_input_profile_4040f0(NativeRuntimeContext&);
// 406FA0 at the head of 453BB0: polls and fills the 8999C0 record; `device`
// gets its +4 bits and +94 axis words.
bool native_pc_input_update_406fa0(NativeRuntimeContext&,PcInputDevice& device);
// The 8999C0 record (0x1D4 bytes) after the last update.
const std::uint8_t* native_pc_input_record(NativeRuntimeContext&);
// Options > Controls > Configuration (FrontendTitleWidgets::ConfigRunner, user =
// the NativeRuntimeContext): 4D7E00 / 4D7FB0 / 4D6A60 / 4D6E50 over the owner.
class FrontendTitleWidgets;
bool native_pc_input_config_runner(void* user,std::uint32_t pc,FrontendTitleWidgets&,std::uint32_t& result);
struct NativePcInputStats { std::uint32_t creates{},updates{},failures{}; std::string last_error; };
NativePcInputStats& native_pc_input_stats();
}
