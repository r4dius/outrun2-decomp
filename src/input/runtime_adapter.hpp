#pragma once
#include "dual_sense.hpp"
#include "platform/runtime_platform.hpp"
#include <atomic>

namespace outrun::ps5 {
// Backend-owned pad reader. False means a failed read/disconnected controller.
// Keep this context alive for the entire native runtime (callback user pointer).
struct RuntimeAdapter {
    void* backend{};
    bool (*read_pad)(void*,PadSample&){};
    std::uint32_t (*current_mode)(void*){};
    DualSenseInput input;
    std::atomic<bool> running{true};
    void stop(){running.store(false,std::memory_order_relaxed);}
};
platform::NativeRuntimeInput make_runtime_input(RuntimeAdapter&);
platform::NativeRuntimePlatform make_runtime_platform(RuntimeAdapter&);
}
