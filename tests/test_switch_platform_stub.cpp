#include "switch_clock.hpp"
#include <cstdio>
int main(){
    auto p=outrun::switch_runtime::make_switch_runtime_platform();
    if(!outrun::platform::runtime_platform_ready(p))return 1;
    if(outrun::platform::runtime_platform_clock_query(p,0x596104u)!=19200000)return 2;
    if(outrun::platform::runtime_platform_clock_query(p,0x5960fcu)!=123456)return 3;
    outrun::platform::runtime_platform_begin_frame(p);
    if(!outrun::platform::runtime_platform_poll(p))return 4;
    std::printf("switch_platform_stub: ok freq=%llu tick=%llu\n",(unsigned long long)p.cached_frequency,(unsigned long long)p.frame_start_tick);
    return 0;
}
