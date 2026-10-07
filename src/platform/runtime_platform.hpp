#pragma once
#include <cstdint>

namespace outrun::platform {

// r096: concrete platform timing / wait / exit boundary used by the native
// frame loop.  The callback shape is deliberately tiny so the same runtime can
// be exercised deterministically on the host and bound to libnx on Switch.
struct NativeRuntimePlatform {
    void* user{};
    std::uint64_t (*ticks)(void* user){};
    std::uint64_t (*frequency)(void* user){};
    void (*sleep_ns)(void* user,std::uint64_t nanoseconds){};
    bool (*main_loop)(void* user){};

    // Mutable timing state owned by the platform adapter, not by guest data.
    std::uint64_t frame_start_tick{};
    std::uint64_t cached_frequency{};
    std::uint32_t wait_calls{};
    std::uint32_t poll_calls{};
};

// The original PC loop compares its millisecond timer against 1/60.  Since the
// timer helper multiplies seconds by 1000, this is a 1/60 millisecond guard
// (~16.67 us), not a 16.67 ms frame limiter.  Preserve that exact boundary.
constexpr double PcLoopWaitThresholdMilliseconds = 1.0 / 60.0;

bool runtime_platform_ready(const NativeRuntimePlatform& platform);
void runtime_platform_begin_frame(NativeRuntimePlatform& platform);
float runtime_platform_elapsed_ms(NativeRuntimePlatform& platform);
std::int64_t runtime_platform_clock_query(NativeRuntimePlatform& platform,std::uint32_t pc_import_address);
void runtime_platform_wait_pc_guard(NativeRuntimePlatform& platform);
bool runtime_platform_poll(NativeRuntimePlatform& platform);

// std::chrono / std::thread clock (system/system_clock.cpp); the Switch has its
// own (switch/source/switch_clock.hpp). The main-loop predicate is always true;
// tests/tools should still supply a finite policy when they run more than one frame.
NativeRuntimePlatform make_system_runtime_platform();

} // namespace outrun::platform
