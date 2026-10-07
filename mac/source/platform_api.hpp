// macOS adaptation of ps5/source/platform_api.hpp; reference baseline remains unchanged.
#pragma once
#include "input/dual_sense.hpp"
#include <cstdint>
#include <string>
using u64=std::uint64_t;using u32=std::uint32_t;using s64=std::int64_t;
struct PadState {outrun::ps5::PadSample sample;u64 previous{},down{},up{};};
struct HidAnalogStickState {std::int32_t x{},y{};};
constexpr u64 Button_A=outrun::ps5::Cross,Button_B=outrun::ps5::Circle;
constexpr u64 Button_X=outrun::ps5::Square,Button_Y=outrun::ps5::Triangle;
constexpr u64 Button_L=outrun::ps5::L1,Button_R=outrun::ps5::R1;
constexpr u64 Button_ZL=outrun::ps5::L2,Button_ZR=outrun::ps5::R2;
constexpr u64 Button_Left=outrun::ps5::Left,Button_Right=outrun::ps5::Right;
constexpr u64 Button_Up=outrun::ps5::Up,Button_Down=outrun::ps5::Down;
constexpr u64 Button_Plus=outrun::ps5::Options,Button_Minus=outrun::ps5::Share;
constexpr u64 Button_StickL=outrun::ps5::StickL,Button_StickR=outrun::ps5::StickR,PadStandard=1;
bool application_initialize(std::string& error);
void application_shutdown();
bool application_running();
void platform_sleep_ns(s64 ns);
std::string application_home(int argc,char** argv);
std::string application_retail_root();
void consoleInit(void*);void consoleUpdate(void*);void consoleExit(void*);
void padConfigureInput(int,u64);void padInitializeDefault(PadState*);
void padUpdate(PadState*);
bool padIsConnected(const PadState*);
u64 padGetButtons(const PadState*);u64 padGetButtonsDown(const PadState*);u64 padGetButtonsUp(const PadState*);
HidAnalogStickState padGetStickPos(const PadState*,unsigned);

std::string application_cache();
