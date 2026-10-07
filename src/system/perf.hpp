#pragma once
// Port timing zones (development profile, not original code): named zones of
// the ported renderer sum their time and calls while pc_perf_enabled() is set
// (main sets it on race frames, like the frame profile). The report gives
// ms per race frame; zones nest, so a zone includes the zones it calls.
#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <vector>
namespace outrun::platform {
struct PcPerfZone {
    const char* name;std::uint64_t ticks{},calls{};
    explicit PcPerfZone(const char* n);
};
inline bool& pc_perf_enabled(){static bool on=false;return on;}
inline std::vector<PcPerfZone*>& pc_perf_zones(){static std::vector<PcPerfZone*> z;return z;}
inline PcPerfZone::PcPerfZone(const char* n):name(n){pc_perf_zones().push_back(this);}
#if defined(__aarch64__)
inline std::uint64_t pc_perf_now(){std::uint64_t t;__asm__ volatile("mrs %0, cntpct_el0":"=r"(t));return t;}
inline std::uint64_t pc_perf_freq(){std::uint64_t f;__asm__ volatile("mrs %0, cntfrq_el0":"=r"(f));return f;}
#else
inline std::uint64_t pc_perf_now(){return std::uint64_t(std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now().time_since_epoch()).count());}
inline std::uint64_t pc_perf_freq(){return 1000000000u;}
#endif
struct PcPerfScope {
    PcPerfZone* z;std::uint64_t t0;
    explicit PcPerfScope(PcPerfZone& zone):z(pc_perf_enabled()?&zone:nullptr),t0(z?pc_perf_now():0u){}
    ~PcPerfScope(){if(z){z->ticks+=pc_perf_now()-t0;++z->calls;}}
    PcPerfScope(const PcPerfScope&)=delete;PcPerfScope& operator=(const PcPerfScope&)=delete;
};
inline void pc_perf_report(std::FILE* f,std::uint64_t frames,bool reset=false){
    if(!f||!frames)return;
    auto zones=pc_perf_zones();
    std::sort(zones.begin(),zones.end(),[](const PcPerfZone* a,const PcPerfZone* b){return a->ticks>b->ticks;});
    const double ms=1000.0/double(pc_perf_freq());
    for(auto* z:zones){
        if(z->calls)std::fprintf(f,"  zone %-30s %7.3f ms/frame  calls/frame %8.1f\n",z->name,
            double(z->ticks)*ms/double(frames),double(z->calls)/double(frames));
        if(reset){z->ticks=0;z->calls=0;}
    }
}
}
#define OR2_PERF_CAT2(a,b) a##b
#define OR2_PERF_CAT(a,b) OR2_PERF_CAT2(a,b)
#define OR2_PERF_ZONE(name) static ::outrun::platform::PcPerfZone OR2_PERF_CAT(or2_zone_,__LINE__)(name); \
    const ::outrun::platform::PcPerfScope OR2_PERF_CAT(or2_scope_,__LINE__)(OR2_PERF_CAT(or2_zone_,__LINE__))
