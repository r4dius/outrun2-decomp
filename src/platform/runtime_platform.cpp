#include "platform/runtime_platform.hpp"
#include <algorithm>
#include <cmath>
#include <limits>


namespace outrun::platform {
namespace {
std::uint64_t ceil_div_u128(std::uint64_t n,std::uint64_t mul,std::uint64_t d){
#if defined(__SIZEOF_INT128__)
    const auto v=static_cast<unsigned __int128>(n)*static_cast<unsigned __int128>(mul);
    return static_cast<std::uint64_t>((v+d-1u)/d);
#else
    // The real libnx/host targets used by this project provide __int128.  Keep a
    // conservative fallback for compilers that do not: quotient first, then a
    // rounded-up remainder contribution.
    const auto q=n/d;
    const auto r=n%d;
    if(q!=0u && mul>std::numeric_limits<std::uint64_t>::max()/q)
        return std::numeric_limits<std::uint64_t>::max();
    const auto a=q*mul;
    const long double b=static_cast<long double>(r)*static_cast<long double>(mul)/static_cast<long double>(d);
    const auto rb=static_cast<std::uint64_t>(std::ceil(b));
    return (a>std::numeric_limits<std::uint64_t>::max()-rb)?std::numeric_limits<std::uint64_t>::max():a+rb;
#endif
}

}

bool runtime_platform_ready(const NativeRuntimePlatform& p){
    return p.ticks!=nullptr&&p.frequency!=nullptr&&p.sleep_ns!=nullptr&&p.main_loop!=nullptr;
}

void runtime_platform_begin_frame(NativeRuntimePlatform& p){
    if(!runtime_platform_ready(p))return;
    p.cached_frequency=p.frequency(p.user);
    p.frame_start_tick=p.ticks(p.user);
}

float runtime_platform_elapsed_ms(NativeRuntimePlatform& p){
    if(!runtime_platform_ready(p))return 0.0f;
    if(p.cached_frequency==0u)p.cached_frequency=p.frequency(p.user);
    if(p.cached_frequency==0u)return 0.0f;
    const auto now=p.ticks(p.user);
    const auto delta=now-p.frame_start_tick; // intentional unsigned wrap, matching timer-tick arithmetic
    // Host timer (not PC arithmetic): double, no software-quad long double on AArch64.
    const double ms=static_cast<double>(delta)*1000.0/static_cast<double>(p.cached_frequency);
    return static_cast<float>(ms);
}

std::int64_t runtime_platform_clock_query(NativeRuntimePlatform& p,std::uint32_t import_address){
    if(!runtime_platform_ready(p))return 0;
    if(import_address==0x596104u){
        p.cached_frequency=p.frequency(p.user);
        return static_cast<std::int64_t>(p.cached_frequency);
    }
    if(import_address==0x5960fcu)return static_cast<std::int64_t>(p.ticks(p.user));
    return 0;
}

void runtime_platform_wait_pc_guard(NativeRuntimePlatform& p){
    if(!runtime_platform_ready(p))return;
    ++p.wait_calls;
    if(p.cached_frequency==0u)p.cached_frequency=p.frequency(p.user);
    if(p.cached_frequency==0u)return;

    // threshold = (1/60 ms) = 1/60000 s.  Round the tick target upward so the
    // post-wait elapsed time cannot remain below the PC comparison boundary.
    const auto guard_ticks=(p.cached_frequency+59999u)/60000u;
    const auto deadline=p.frame_start_tick+guard_ticks;
    auto now=p.ticks(p.user);
    if(static_cast<std::int64_t>(deadline-now)<=0)return;

    auto remaining=deadline-now;
    const auto ns=ceil_div_u128(remaining,1000000000ull,p.cached_frequency);
    if(ns!=0u)p.sleep_ns(p.user,ns);

    // A sleep of only a few microseconds may return slightly early on some
    // schedulers.  Yield until the exact tick boundary is reached.  The loop is
    // bounded by time progression supplied by the platform backend.
    for(unsigned spins=0;spins<64u;++spins){
        now=p.ticks(p.user);
        if(static_cast<std::int64_t>(deadline-now)<=0)break;
        p.sleep_ns(p.user,0u);
    }
}

bool runtime_platform_poll(NativeRuntimePlatform& p){
    if(!runtime_platform_ready(p))return true;
    ++p.poll_calls;
    return p.main_loop(p.user);
}


} // namespace outrun::platform
