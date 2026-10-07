// The portable system clock of the runtime frame loop: std::chrono ticks,
// std::this_thread sleeps (Linux host, macOS, tools).
#include "platform/runtime_platform.hpp"
#include <chrono>
#include <thread>
namespace outrun::platform {
namespace {
std::uint64_t system_ticks(void*){
    using namespace std::chrono;
    return static_cast<std::uint64_t>(duration_cast<nanoseconds>(steady_clock::now().time_since_epoch()).count());
}
std::uint64_t system_frequency(void*){return 1000000000ull;}
void system_sleep_ns(void*,std::uint64_t ns){
    std::this_thread::sleep_for(std::chrono::nanoseconds(ns));
}
bool system_main_loop(void*){return true;}
}
NativeRuntimePlatform make_system_runtime_platform(){
    NativeRuntimePlatform p{};
    p.ticks=system_ticks;
    p.frequency=system_frequency;
    p.sleep_ns=system_sleep_ns;
    p.main_loop=system_main_loop;
    return p;
}
}
