#pragma once
#include "driving/pc_matrix_stack.hpp"
#include <array>
#include <cstdint>

namespace outrun::driving {
// Offsets are relative to the native payload AFTER the four-byte size prefix.
// They are never x86 addresses or cached host pointers. Presence is carried
// separately, as in the original root-pointer tests.
struct PcEnvironmentList {
    bool present{};
    std::uint32_t offset{};
    std::uint32_t records{};
    std::uint32_t active{};
    std::uint32_t skipped{};
};
struct PcEnvironmentLayout {
    std::array<std::array<PcEnvironmentList,3>,2> lists{}; // fog, sun
    bool spline_present{};
    std::uint32_t spline_records{};
    std::uint32_t spline_active{};
    std::uint32_t spline_skipped{};
};
using PcEnvironmentPayloads=std::array<Bytes,3>;
// Read-only bounds validation of the original sentinel walks. Empty view =
// absent source, not an unfinished load. Exact aliases/suffixes are legal;
// header overlap and incongruent overlapping records are rejected at admission.
PcEnvironmentLayout inspect_course_environment(const PcEnvironmentPayloads& payloads);
// Reset -> initialize path of 0x44A940, with its six root getters represented
// by bounded offsets. Caller owns/stages mutable payloads. No host pointer is
// serialized into the PC data. Existing matrix arithmetic supplies 0x40A7D0.
PcEnvironmentLayout course_environment_init_44a940(
    const PcEnvironmentPayloads& payloads,Bytes primary_matrix);
// Completion tail of 0x44AA80. This is the root mode (0x78026C), NOT
// the race variant (0x780258). Other modes preserve the previous six flags.
void course_environment_finish_44aa80(std::uint32_t mode_78026c,
    std::array<std::uint32_t,6>& flags_7d28b0,std::uint32_t& phase_7d28c8);
} // namespace outrun::driving
