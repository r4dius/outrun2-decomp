#include "switch_network.hpp"
#include "system/network_bsd.hpp"
#include <cstddef>
#include <vector>
#include <switch.h>
#include <arpa/inet.h>
#include <unistd.h>
namespace outrun::switch_runtime {
#if !defined(OUTRUN_SWITCH_STUB)
namespace {
std::vector<platform::PcNetworkInterface> interfaces(){
    std::vector<platform::PcNetworkInterface> out;
    static bool nifm=R_SUCCEEDED(nifmInitialize(NifmServiceType_User));
    u32 address=0,mask=0,gateway=0,dns1=0,dns2=0;
    if(nifm&&R_SUCCEEDED(nifmGetCurrentIpConfigInfo(&address,&mask,&gateway,&dns1,&dns2))&&address)
        out.push_back({address,(address&mask)|~mask,mask});
    else if(const auto id=std::uint32_t(gethostid());id&&id!=0x7f000001u&&id!=0x0100007fu){
        const std::uint32_t m=htonl(0xffffff00u);out.push_back({id,(id&m)|~m,m});
    }
    return out;
}
}
#endif
platform::PcNetworkPlatform switch_network_platform(){
    auto p=platform::pc_network_bsd_platform();
#if !defined(OUTRUN_SWITCH_STUB)
    if(p.open_udp)p.interfaces=interfaces;
#endif
    return p;
}
}
