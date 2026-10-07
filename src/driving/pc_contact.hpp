#pragma once
#include "driving/pc_driving.hpp"
namespace outrun::driving {
// PC CalcContactMatrix, semantic body at 0x004A63CC.  The public entry
// 0x004A63C0 first enters a protection trampoline; r009 validates this
// body through a harness-only stack wrapper and the symbolized Lindbergh twin.
void contact_matrix(Bytes work);
// PC MaximumVelocityCheck 0x004A65C0. Returns whether it clamped/adjusted state.
bool maximum_velocity_check(Bytes work);
}
