#include "runtime_adapter.hpp"

namespace outrun::ps5 {
namespace {
void sample_input(void* user,platform::NativeInputState& state){
    auto& adapter=*static_cast<RuntimeAdapter*>(user);
    PadSample pad{};
    if(!adapter.read_pad||!adapter.read_pad(adapter.backend,pad))pad=PadSample{};
    const auto mode=adapter.current_mode?adapter.current_mode(adapter.backend):0u;
    state=adapter.input.update(pad,mode);
    if(state.exit_requested)adapter.stop();
}
bool main_loop(void* user){
    return static_cast<RuntimeAdapter*>(user)->running.load(std::memory_order_relaxed);
}
}
platform::NativeRuntimeInput make_runtime_input(RuntimeAdapter& adapter){
    platform::NativeRuntimeInput input{};input.user=&adapter;input.sample=sample_input;return input;
}
platform::NativeRuntimePlatform make_runtime_platform(RuntimeAdapter& adapter){
    // PS5 is x86-64: use the existing chrono/thread timing boundary, never
    // Switch ARM timer registers. Preserve the recovered PC wait semantics.
    auto result=platform::make_system_runtime_platform();
    result.user=&adapter;result.main_loop=main_loop;return result;
}
}
