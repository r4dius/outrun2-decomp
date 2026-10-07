#pragma once
// The runtime frame loop's clock on the Switch: the ARM system counter,
// svcSleepThread and appletMainLoop (libnx).
#include "platform/runtime_platform.hpp"
namespace outrun::switch_runtime {
platform::NativeRuntimePlatform make_switch_runtime_platform();
}
