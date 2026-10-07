#pragma once
#include "platform/runtime_input.hpp"
#include <cstdint>

namespace outrun::ps5 {
// Logical buttons, deliberately independent of the official/homebrew ABI.
// The SDK backend translates its pad bitfield to this enum.
enum Button : std::uint32_t {
    Cross=1u<<0, Circle=1u<<1, Square=1u<<2, Triangle=1u<<3,
    Options=1u<<4, L1=1u<<5, R1=1u<<6, L2=1u<<7, R2=1u<<8,
    Left=1u<<9, Right=1u<<10, Up=1u<<11, Down=1u<<12,
    StickL=1u<<13, StickR=1u<<14, Share=1u<<15
};
struct PadSample {
    bool connected{};
    std::uint32_t buttons{};
    std::uint8_t left_x{128},left_y{128},right_x{128},right_y{128};
    std::uint8_t l2{},r2{};
};
class DualSenseInput {
public:
    platform::NativeInputState update(const PadSample&,std::uint32_t game_mode);
    void reset(){previous_=0;}
private:
    std::uint32_t previous_{};
};
} // namespace outrun::ps5
