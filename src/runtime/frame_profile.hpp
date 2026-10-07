#pragma once
// Frame-time profile of the race modes (16 GAME and the race end modes),
// written to the result log at exit: time per event callback, per PC scene
// display callback, and in the renderer (image acquire / PC fence waits =
// GPU-bound time, PC scene command recording, audio mixing). Ticks come
// from the platform clock (profile_clock(); the ARM system counter on the
// Switch, steady_clock elsewhere).
#include <chrono>
#include <cstdint>
namespace outrun::runtime {
struct ProfileClock {
    std::uint64_t (*ticks)(){[]{return std::uint64_t(std::chrono::duration_cast<std::chrono::nanoseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count());}};
    std::uint64_t frequency{1000000000u};
};
inline ProfileClock& profile_clock(){static ProfileClock c;return c;}
}
#define OR2_PROFILE_TICK() (::outrun::runtime::profile_clock().ticks())
#define OR2_PROFILE_FREQ() (::outrun::runtime::profile_clock().frequency)
#include "system/perf.hpp"
#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <map>
#include <vector>
namespace outrun::runtime {
struct ProfileBucket { std::uint64_t ticks{},max{},calls{}; };
struct FrameProfile {
    bool active{};                                   // set per frame by main (race modes)
    std::uint64_t frames{},frame_ticks{},frame_max{},last_frame_start{};
    std::map<std::uint64_t,ProfileBucket> buckets;   // key: (kind<<56)|(event<<32)|pc
    void add_one(std::uint64_t key,std::uint64_t ticks){
        if(!active)return;
        auto& b=buckets[key];b.ticks+=ticks;b.calls+=1;b.max=std::max(b.max,ticks);
    }
    void add(std::uint64_t key,std::uint64_t ticks);
    void frame_start(bool race,std::uint32_t mode=0);
    void frame_start_one(bool race){
        const std::uint64_t now=OR2_PROFILE_TICK();
        if(active&&last_frame_start){const auto d=now-last_frame_start;++frames;frame_ticks+=d;frame_max=std::max(frame_max,d);}
        active=race;last_frame_start=race?now:0u;
    }
};
inline FrameProfile& frame_profile(){static FrameProfile p;return p;}
// The same counters over the last window only (live nxlink report).
inline FrameProfile& frame_profile_window(){static FrameProfile p;return p;}
// Called once per frame (input sample). With nxlink stdio, every 300 race
// frames a window report goes to stdout.
inline bool& frame_profile_live(){static bool live=false;return live;}
// Stutter log (live log only): every frame longer than ~18 ms is printed with
// its number, game mode, duration and the buckets that took the most time in
// that frame (ticks summed per bucket key over the frame, all modes).
struct HitchFrame {
    static constexpr unsigned Slots=96;
    std::uint64_t keys[Slots]{},ticks[Slots]{};unsigned n{};
    std::uint64_t start{},number{};std::uint32_t mode{};
    void add(std::uint64_t key,std::uint64_t t){
        for(unsigned i=0;i<n;++i)if(keys[i]==key){ticks[i]+=t;return;}
        if(n<Slots){keys[n]=key;ticks[n]=t;++n;}
    }
};
inline HitchFrame& hitch_frame(){static HitchFrame h;return h;}
void hitch_report(std::FILE*,const HitchFrame&,std::uint64_t duration);
void frame_profile_report(std::FILE*,const FrameProfile&,unsigned);
inline void FrameProfile::frame_start(bool race,std::uint32_t mode){
    if(frame_profile_live()){
        auto& h=hitch_frame();const std::uint64_t now=OR2_PROFILE_TICK();
        if(h.start&&now-h.start>OR2_PROFILE_FREQ()*18u/1000u)hitch_report(stdout,h,now-h.start);
        h.n=0;h.start=now;++h.number;h.mode=mode;
    }
    frame_start_one(race);
    platform::pc_perf_enabled()=race;
    auto& w=frame_profile_window();w.frame_start_one(race);
    if(w.frames>=300u){
        if(frame_profile_live()){frame_profile_report(stdout,w,14);platform::pc_perf_report(stdout,frames);std::fflush(stdout);}
        const bool a=w.active;const auto t=w.last_frame_start;w=FrameProfile{};w.active=a;w.last_frame_start=t;
    }
}
// Image acquire + PC fence waits of the current frame (all modes), read and
// reset by the performance overlay: frame time minus this = CPU-side time.
inline std::uint64_t& frame_wait_ticks(){static std::uint64_t t=0;return t;}
inline void FrameProfile::add(std::uint64_t key,std::uint64_t ticks){
    if((key>>56)==3u&&((key&0xffffffffu)==1u||(key&0xffffffffu)==2u))frame_wait_ticks()+=ticks;   // ProfileRenderer: ProfAcquire / ProfPcFence
    add_one(key,ticks);frame_profile_window().add_one(key,ticks);
    if(frame_profile_live())hitch_frame().add(key,ticks);}
// Kinds of the bucket key.
constexpr std::uint64_t ProfileEvent=1,ProfileDisplay=2,ProfileRenderer=3;
// Renderer bucket ids.
constexpr std::uint32_t ProfAcquire=1,ProfPcFence=2,ProfPcRecord=3,ProfPresent=4,ProfAudio=5,ProfDrawTotal=6,ProfSceneRender=7,ProfPcGpu=8,ProfDeviceDraw=9;
inline std::uint64_t profile_key(std::uint64_t kind,std::uint32_t event,std::uint32_t pc){return (kind<<56)|(std::uint64_t(event)<<32)|pc;}
struct ProfileScope {
    std::uint64_t key,t0;
    explicit ProfileScope(std::uint64_t k):key(k),t0(OR2_PROFILE_TICK()){}
    ~ProfileScope(){frame_profile().add(key,OR2_PROFILE_TICK()-t0);}
};
// Text summary (ms per race frame, sorted by total time).
inline void frame_profile_report(std::FILE* f,const FrameProfile& p=frame_profile(),unsigned top=60){
    const double tick_ms=1000.0/double(OR2_PROFILE_FREQ());
    if(!f)return;
    if(!p.frames){std::fprintf(f,"frame profile: no race frames\n");return;}
    std::fprintf(f,"frame profile (race modes): frames=%llu avg=%.2f ms (%.1f fps) max=%.2f ms\n",
        (unsigned long long)p.frames,double(p.frame_ticks)/double(p.frames)*tick_ms,
        1000.0/(double(p.frame_ticks)/double(p.frames)*tick_ms),double(p.frame_max)*tick_ms);
    std::vector<std::pair<std::uint64_t,ProfileBucket>> v(p.buckets.begin(),p.buckets.end());
    std::sort(v.begin(),v.end(),[](const auto& a,const auto& b){return a.second.ticks>b.second.ticks;});
    static const char* renderer_names[]{"?","acquire image (GPU/vsync wait)","PC fence wait (GPU)","PC scene record (CPU)","present","audio mix","renderer draw total","PC scene render_frame","PC scene GPU time (timestamps)","PC device draws (CPU, in displays)"};
    unsigned shown=0;
    for(const auto& [k,b]:v){
        if(++shown>top)break;
        const unsigned kind=unsigned(k>>56),ev=unsigned((k>>32)&0xffffffu),pc=unsigned(k&0xffffffffu);
        const double per_frame=double(b.ticks)/double(p.frames)*tick_ms;
        if(kind==ProfileRenderer)std::fprintf(f,"  %-34s %7.3f ms/frame  max %7.3f ms  calls %llu\n",
            pc<10u?renderer_names[pc]:"?",per_frame,double(b.max)*tick_ms,(unsigned long long)b.calls);
        else if(kind==ProfileDisplay&&ev>=0xfffeu)   // scene renderer hooks: whole display (inclusive) / frame leaf
            std::fprintf(f,"  %s pc %06x %20s %7.3f ms/frame  max %7.3f ms  calls %llu\n",ev==0xffffu?"leaf     ":"display  ",pc,"",
            per_frame,double(b.max)*tick_ms,(unsigned long long)b.calls);
        else std::fprintf(f,"  %s event %3u pc %06x %13s %7.3f ms/frame  max %7.3f ms  calls %llu\n",kind==ProfileEvent?"ctrl":"disp",ev,pc,"",
            per_frame,double(b.max)*tick_ms,(unsigned long long)b.calls);
    }
    if(&p==&frame_profile())platform::pc_perf_report(f,p.frames);
}
inline void hitch_report(std::FILE* f,const HitchFrame& h,std::uint64_t duration){
    if(!f)return;
    const double tick_ms=1000.0/double(OR2_PROFILE_FREQ());
    unsigned order[HitchFrame::Slots];unsigned n=0;
    for(unsigned i=0;i<h.n;++i){
        const unsigned kind=unsigned(h.keys[i]>>56),id=unsigned(h.keys[i]&0xffffffffu);
        if(kind==ProfileRenderer&&(id==ProfDrawTotal||id==ProfSceneRender))continue;   // inclusive totals
        order[n++]=i;
    }
    std::sort(order,order+n,[&](unsigned a,unsigned b){return h.ticks[a]>h.ticks[b];});
    static const char* names[]{"?","acquire","gpu-wait","record","present","audio","draw","render","gpu-time","draws"};
    std::fprintf(f,"hitch frame %llu mode %u: %.1f ms |",(unsigned long long)h.number,h.mode,double(duration)*tick_ms);
    for(unsigned k=0;k<n&&k<6;++k){
        const auto key=h.keys[order[k]];const unsigned kind=unsigned(key>>56),ev=unsigned((key>>32)&0xffffffu),pc=unsigned(key&0xffffffffu);
        const double ms=double(h.ticks[order[k]])*tick_ms;
        if(kind==ProfileRenderer)std::fprintf(f," %s %.1f",pc<10u?names[pc]:"?",ms);
        else if(kind==ProfileDisplay&&ev>=0xfffeu)std::fprintf(f," %s%06x %.1f",ev==0xffffu?"leaf":"disp",pc,ms);
        else std::fprintf(f," %s%u:%06x %.1f",kind==ProfileEvent?"ev":"de",ev,pc,ms);
    }
    std::uint64_t listed=0;for(unsigned k=0;k<n;++k)listed+=h.ticks[order[k]];
    std::fprintf(f," | unprofiled %.1f",duration>listed?double(duration-listed)*tick_ms:0.0);
    std::fputc(10,f);
}
}
