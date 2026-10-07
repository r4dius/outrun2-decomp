#include "switch_clock.hpp"
#include <algorithm>
#include <cstddef>
#include <limits>
#include <switch.h>
namespace outrun::switch_runtime {
namespace {
std::uint64_t system_ticks(void*){return static_cast<std::uint64_t>(armGetSystemTick());}
std::uint64_t system_frequency(void*){return static_cast<std::uint64_t>(armGetSystemTickFreq());}
void system_sleep_ns(void*,std::uint64_t ns){
    const auto cap=static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max());
    svcSleepThread(static_cast<std::int64_t>(std::min(ns,cap)));
}
bool system_main_loop(void*){return appletMainLoop();}
}
platform::NativeRuntimePlatform make_switch_runtime_platform(){
    platform::NativeRuntimePlatform p{};
    p.ticks=system_ticks;
    p.frequency=system_frequency;
    p.sleep_ns=system_sleep_ns;
    p.main_loop=system_main_loop;
    return p;
}
}
