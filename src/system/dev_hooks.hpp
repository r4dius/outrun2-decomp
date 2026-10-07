#pragma once
// Development hooks of the port (not original code): switches the host
// runners turn on (runtime/game_host.cpp reads them from the environment of
// host_nro / macOS); everything is off in a release build, where the game
// code below behaves as if the hooks did not exist.
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <optional>
namespace outrun::platform {
struct DevHooks {
    bool log_faults{};          // caught faults of ported / translated code go to stderr
    bool fixed_clock{};         // OR2_HOST_FIXEDCLOCK: wall-clock budgets advance 1 ms per call (lockstep runs)
    bool trace_glyphs{};        // OR2_HOST_GLYPHS: the 49E4B0 frontend glyph list every frame
    std::optional<std::uint32_t> force_variant;   // OR2_HOST_VARIANT: the race variant [780258] START takes (modes without a menu path)
};
inline DevHooks& dev_hooks(){static DevHooks h;return h;}
// One line on stderr when log_faults is on.
inline void dev_log(const char* format,...){
    if(!dev_hooks().log_faults)return;
    std::va_list args;va_start(args,format);std::vfprintf(stderr,format,args);va_end(args);
    std::fputc(10,stderr);
}
}
