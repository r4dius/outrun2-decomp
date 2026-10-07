// LAN layer (pc_network.hpp): the translated network tick and its Winsock bridge.
#include "platform/pc_network.hpp"
#include "platform/native_runtime.hpp"
#include "platform/race_translated.hpp"
#include "platform/translated_crt.hpp"
#include "platform/pc_address_view.hpp"
#include "platform/frontend_text.hpp"
#include "platform/race_end_runtime.hpp"
#include "platform/race_manager_runtime.hpp"
#include "platform/race_area_runtime.hpp"
#include "driving/pc_x87.hpp"
#include "platform/frontend_profiles.hpp"
#include <chrono>
#include <cstdio>
#include <random>
#include <cstdlib>
#include <functional>
#include <cstring>
#include <memory>
#include <stdexcept>
namespace outrun::platform {
extern const TranslatedFunction network_functions[];
extern const TranslatedCodeData network_code_data[];
namespace {
// The network module's private copy of the whole data region 62A000..98E000
// (.data initial values from the EXE image, .bss zero): its sessions, peers,
// lobby, screens and widgets. Shared state is mapped over it (the frontend owner,
// the active license...).
constexpr std::uint32_t DataBase=0x62a000u,DataEnd=0x98e000u;
// Frontend functions the network code and its screens call (thiscall; RET n): answered
// by native_frontend_owner_call with ECX (the owner 7B17E8, a screen object or its
// 465160 resource in the guest heap) and the n / 4 stack arguments.
// (generated from the closure's frontend externals: the owner methods 440D00..447800,
// the sprite / text / sound leaves 428000..42FFFF, the 465xxx resources, the screen base /
// input 48F480 / 48F4D0 / 48F5F0; 5816BD, the CRT array constructor, is answered in run).
constexpr TranslatedPops FrontendCalls[]{
    {0x4249f0u,0x0},{0x4294c0u,0x0},{0x429530u,0x0},{0x429810u,0x0},{0x42c360u,0x0},{0x42c370u,0x0},
    {0x42c820u,0x0},{0x42c840u,0x0},{0x42ca60u,0x0},{0x42cc00u,0x0},{0x42cc60u,0x0},{0x42cca0u,0x0},
    {0x42ccb0u,0x0},{0x42cdd0u,0x0},{0x42d280u,0x0},{0x42d300u,0x0},{0x42fc90u,0x0},{0x43fa00u,0x0},
    {0x440e00u,0x4},{0x440e10u,0x0},{0x440e60u,0x0},{0x440ea0u,0x20},{0x440f70u,0xc},{0x441130u,0x0},
    {0x441260u,0x0},{0x4412c0u,0x0},{0x4412f0u,0x0},{0x441300u,0x4},{0x442ec0u,0x0},{0x442ef0u,0x0},
    {0x442f20u,0x8},{0x443040u,0x0},{0x4440f0u,0x4},{0x444780u,0x4},{0x444b20u,0xc},{0x4459f0u,0x4},
    {0x4464f0u,0x0},{0x446fc0u,0x0},{0x465160u,0x0},{0x465250u,0x0},{0x4652e0u,0x0},{0x4653c0u,0x10},
    {0x465770u,0x4},{0x465860u,0x2c},{0x465970u,0x0},{0x4659f0u,0x0},{0x465f30u,0x0},{0x48f480u,0x0},
    {0x48f4d0u,0x0},{0x48f5f0u,0x4},{0x4c05f0u,0x0},{0x525150u,0x0},{0x525170u,0x0},{0x5251e0u,0x0},
    {0x525240u,0x0},{0x525270u,0x0},{0x5252d0u,0x0},{0x5816bdu,0x14},
    {0,0}};
// The sprite / text / sound leaves 428000..42FFFF are cdecl: their argument count is
// not in a RET n, the bridge gets the whole stack window (it reads what it needs).
bool cdecl_leaf(std::uint32_t pc){return (pc>=0x428000u&&pc<0x430000u)||pc==0x4249f0u;}
constexpr std::uint32_t HeapBase=0x0e000000u,HeapSize=0x400000u;
constexpr std::uint32_t OwnerBase=0x0ef00000u;         // [7B17E8]: the frontend owner 4035F0 returns
constexpr std::uint32_t StringBase=0x0ef80000u,StringSize=0x4000u;
constexpr std::uint32_t TibBase=0x0efa0000u;          // the thread block FS: points at (SEH frames)
// Winsock imports (WS2_32 by ordinal) 5961F8..596244: fake thunks.
constexpr std::uint32_t IatBase=0x5961f8u,IatWords=20u,ThunkBase=0x0fa10000u;
enum : std::uint32_t {WsaStartup,SendTo,RecvFrom,Ntohs,Socket,Htons,Bind,IoctlSocket,CloseSocket,Recv,Send,Connect,Listen,Select,
                      InetNtoa,GetHostByName,InetAddr,WsaIoctl,WsaGetLastError,SetSockOpt};
constexpr std::uint32_t WsaEWouldBlock=10035u;
// KERNEL32 / ADVAPI32 imports 596000..596144 used by the Demonware code: fake thunks.
constexpr std::uint32_t SystemIatBase=0x596000u,SystemIatWords=0x52u,SystemThunkBase=0x0fa20000u;
enum : std::uint32_t {CryptAcquireContext=0,CryptGenRandom=1,QueryPerformanceCounter=0x3f,QueryPerformanceFrequency=0x41,
                      ResumeThread=0x49,CreateThread=0x4a,DebugBreak=0x50};
struct VirtualSocket { bool used{},broadcast{}; int fd{-1}; std::uint16_t port{}; };
struct State {
    PcRaceMemory memory;
    bool mapped{},started{};
    std::vector<std::vector<std::uint8_t>> data;
    std::unique_ptr<GuestHeap> heap;
    std::array<std::uint32_t,IatWords> iat{};
    std::array<std::uint32_t,SystemIatWords> system_iat{};
    std::array<std::uint8_t,StringSize> strings{};std::uint32_t strings_used{};
    std::array<std::uint8_t,0x20> ntoa{};
    std::uint32_t cookie_735a00{0xbb40e64eu},last_error{};
    std::array<std::uint32_t,0x40> tib{};
    std::array<VirtualSocket,16> sockets{};
    PcNetworkPlatform platform;
    std::size_t fixed_mark{};                          // mappings after it are refreshed every outermost call
    unsigned depth{};                                  // nested run() calls (services calling back in)
    std::array<std::uint32_t,24> event_cars{};         // [799D18 + k * 0x3C]: the works of events 8..31
    std::vector<std::uint8_t> native_stack=std::vector<std::uint8_t>(0x10000);   // the native code's guest locals (0x0EFC0000)
};
State& state(){static State s;return s;}
NativeNetworkStats stats_;
bool exe_copy(std::uint32_t a,std::uint8_t* out,std::size_t n){
    bool any=false;
    for(std::size_t i=0;i<EmbeddedExeRangeCount;++i){const auto& e=EmbeddedExeRanges[i];
        const std::uint32_t lo=std::max(a,e.base),hi=std::min<std::uint32_t>(a+std::uint32_t(n),e.base+e.size);
        if(lo<hi){std::memcpy(out+(lo-a),e.data+(lo-e.base),hi-lo);any=true;}}
    return any;
}
void map_state(NativeRuntimeContext& c,State& s){
    auto& m=s.memory;
    if(!s.mapped){
        s.mapped=true;
        for(std::size_t i=0;i<EmbeddedExeRangeCount;++i)m.map_const(EmbeddedExeRanges[i].base,EmbeddedExeRanges[i].data,EmbeddedExeRanges[i].size);
        s.data.emplace_back(DataEnd-DataBase);(void)exe_copy(DataBase,s.data.back().data(),DataEnd-DataBase);
        m.map(DataBase,s.data.back().data(),DataEnd-DataBase);
        s.heap=std::make_unique<GuestHeap>(HeapBase,HeapSize);s.heap->map(m);
        for(std::uint32_t k=0;k<IatWords;++k)s.iat[k]=ThunkBase+k*4u;
        m.map(IatBase,reinterpret_cast<std::uint8_t*>(s.iat.data()),IatWords*4u);
        for(std::uint32_t k=0;k<SystemIatWords;++k)s.system_iat[k]=SystemThunkBase+k*4u;
        m.map(SystemIatBase,reinterpret_cast<std::uint8_t*>(s.system_iat.data()),SystemIatWords*4u);
        m.map(StringBase,s.strings.data(),s.strings.size());
        m.map(0x0ef90000u,s.ntoa.data(),s.ntoa.size());
        m.map(0x0efc0000u,s.native_stack.data(),s.native_stack.size());
        m.map(0x735a00u,reinterpret_cast<std::uint8_t*>(&s.cookie_735a00),4);
        s.tib[0]=0xffffffffu;                              // FS:[0] SEH chain end
        m.map(TibBase,reinterpret_cast<std::uint8_t*>(s.tib.data()),s.tib.size()*4u);
        auto& owner=c.event_function36.object;
        m.map(OwnerBase,owner.data(),owner.size());
        // The active license 7C23E0 (0x40C bytes): the frontend's own copy (name, unlocks, settings).
        auto& license=c.event_function36.frontend_profiles.active;
        m.map(0x7c23e0u,license.data(),license.size());
        // Game state the network code shares with the native game (the race start 4F5270, the
        // network race services): the race-end blocks (event-4 work 780440, goal 8369B4..,
        // 7D68BC..7D68D3 session bytes, arcade .data...), the CommRace block 7DE418 / slot
        // 7DD138, the frontend manager flags 659944, 7DF108 / 7DF10F, the course / car choices.
        auto& end=native_race_end(c);
        end.state.map(m);
        static constexpr std::uint32_t Work4=0x780440u;
        m.map_const(0x799c28u,reinterpret_cast<const std::uint8_t*>(&Work4),4);   // [799C28] = the event-4 work
        auto& world=c.race.car_world;
        m.map(0x7de418u,world.commrace_7de418.data(),world.commrace_7de418.size());
        m.map(0x7dd138u,&world.slot_7dd138,1);
        m.map(0x659944u,reinterpret_cast<std::uint8_t*>(&end.frontend_manager_659944),4);
        m.map(0x7df108u,&c.start_mode.network_mode_7df108,1);
        m.map(0x7df10fu,&c.start_mode.network_player_count_7df10f,1);
        auto& common=c.event_function36.frontend_profiles.common;m.map(0x7b17f8u,common.data(),common.size());
        m.map(0x830364u,&c.event_function36.music_globals.track_830364,1);
        m.map(0x830374u,&c.event_function36.car_select.transmission_830374,1);
        m.map(0x83036du,&c.start_mode.vehicle_variant_83036d,1);
        m.map(0x655b59u,reinterpret_cast<std::uint8_t*>(&c.start_mode.course_choice_655b59),1);
        m.map(0x655b5au,&c.start_mode.vehicle_colour_655b5a,1);
        m.map(0x780258u,reinterpret_cast<std::uint8_t*>(&c.game_mode.game_variant),4);   // the game variant (3 / 4: LAN races)
        // 45ACB0's 525340(1): Demonware's allocators registered (bdAlloc / bdFree /
        // bdRealloc, answered over the guest heap below) and 85DEB4 latched.
        m.put32(0x85de8cu,0x525150u);m.put32(0x85de90u,0x525170u);m.put32(0x85de94u,0x5251e0u);
        m.put8(0x85deb4u,1);
        m.map(0x7f1938u,reinterpret_cast<std::uint8_t*>(&world.clock_7f1938),4);   // the race clock (packet times)
        s.fixed_mark=m.mark();
    }
    // Every call: the race cars the network code reads and writes (457790 / 457A60...):
    // the event work pointers 799D18 + k * 0x3C and the works themselves (the player car
    // 7804B0, events 9.. at 7815A0 + k * 0x10F0 while the race manager exists).
    if(s.depth)return;                                 // nested: the outer call's mappings (its guest stack) stay
    m.release(s.fixed_mark);
    for(std::uint32_t k=0;k<s.event_cars.size();++k){
        s.event_cars[k]=c.event_state.slots[8u+k].work_token;
        m.map(0x799d18u+k*0x3cu,reinterpret_cast<std::uint8_t*>(&s.event_cars[k]),4);
    }
    auto& player=c.event_function36.car_select.car_799d18;m.map(0x7804b0u,player.data(),player.size());
    auto& works=c.race.manager.car_works_7815a0;if(!works.empty())m.map(0x7815a0u,works.data(),works.size());
    m.map(0x78026cu,reinterpret_cast<std::uint8_t*>(&c.mode_state.current),4);       // the mode (45A0C0 runs in 16 / 18)
    m.map(0x78024cu,reinterpret_cast<std::uint8_t*>(&c.start_mode.course_preset),4); // the course preset
}
// A KERNEL32 / ADVAPI32 import (stdcall): the crypto provider (one fake handle, random
// bytes from the platform), the performance counter in microseconds; no threads.
bool system_import(State& s,or2x86::Cpu& cpu,std::uint32_t index){
    auto& m=s.memory;
    auto a=[&](unsigned n){return translated_arg(cpu,n);};
    std::uint32_t eax=0,pop=0;
    switch(index){
    case CryptAcquireContext:pop=20;if(a(0))m.put32(a(0),1u);eax=1;break;
    case CryptGenRandom:{pop=12;static std::random_device device;
        for(std::uint32_t i=0;i<a(1);++i)m.put8(a(2)+i,std::uint8_t(device()));
        eax=1;break;}
    case QueryPerformanceCounter:{pop=4;
        const auto us=std::uint64_t(std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now().time_since_epoch()).count());
        m.put32(a(0),std::uint32_t(us));m.put32(a(0)+4,std::uint32_t(us>>32));eax=1;break;}
    case QueryPerformanceFrequency:pop=4;m.put32(a(0),1000000u);m.put32(a(0)+4,0);eax=1;break;
    default:{char t[64];std::snprintf(t,sizeof t,"system import %03X not bridged",index*4u);throw std::runtime_error(t);}
    }
    translated_return(cpu,eax,pop);
    return true;
}
// The VS2005 CRT qsort (qsort.c, CUTOFF 8: shortsort below, median-of-three
// partition above), over guest memory, the compare run translated.
void crt_qsort(PcRaceMemory& m,std::uint32_t base,std::uint32_t num,std::uint32_t width,
               const std::function<std::int32_t(std::uint32_t,std::uint32_t)>& comp){
    if(num<2||!width)return;
    auto swap=[&](std::uint32_t a,std::uint32_t b){
        if(a==b)return;
        for(std::uint32_t i=0;i<width;++i){const auto t=m.u8(a+i);m.put8(a+i,m.u8(b+i));m.put8(b+i,t);}};
    auto shortsort=[&](std::uint32_t lo,std::uint32_t hi){
        while(hi>lo){
            std::uint32_t max=lo;
            for(std::uint32_t p=lo+width;p<=hi;p+=width)if(comp(p,max)>0)max=p;
            swap(max,hi);hi-=width;
        }};
    std::uint32_t lostk[30],histk[30];int stkptr=0;
    std::uint32_t lo=base,hi=base+width*(num-1);
    for(;;){
        const std::uint32_t size=(hi-lo)/width+1;
        if(size<=8)shortsort(lo,hi);
        else{
            std::uint32_t mid=lo+(size/2)*width;
            if(comp(lo,mid)>0)swap(lo,mid);
            if(comp(lo,hi)>0)swap(lo,hi);
            if(comp(mid,hi)>0)swap(mid,hi);
            std::uint32_t loguy=lo,higuy=hi;
            for(;;){
                if(mid>loguy){do loguy+=width;while(loguy<mid&&comp(loguy,mid)<=0);}
                if(mid<=loguy){do loguy+=width;while(loguy<=hi&&comp(loguy,mid)<=0);}
                do higuy-=width;while(higuy>mid&&comp(higuy,mid)>0);
                if(higuy<loguy)break;
                swap(loguy,higuy);
                if(mid==higuy)mid=loguy;
            }
            higuy+=width;
            if(mid<higuy){do higuy-=width;while(higuy>mid&&comp(higuy,mid)==0);}
            if(mid>=higuy){do higuy-=width;while(higuy>lo&&comp(higuy,mid)==0);}
            if(higuy-lo>=hi-loguy){
                if(lo<higuy){lostk[stkptr]=lo;histk[stkptr]=higuy;++stkptr;}
                if(loguy<hi){lo=loguy;continue;}
            }else{
                if(loguy<hi){lostk[stkptr]=loguy;histk[stkptr]=hi;++stkptr;}
                if(lo<higuy){hi=higuy;continue;}
            }
        }
        if(--stkptr<0)return;
        lo=lostk[stkptr];hi=histk[stkptr];
    }
}
std::uint32_t be32(std::uint32_t v){return (v>>24)|((v>>8)&0xff00u)|((v<<8)&0xff0000u)|(v<<24);}
std::uint16_t swap16(std::uint32_t v){return std::uint16_t(((v&0xffu)<<8)|((v>>8)&0xffu));}
// A Winsock import (stdcall): EAX and the argument bytes popped.
std::uint32_t winsock_call(State& s,std::uint32_t index,const std::function<std::uint32_t(unsigned)>& a,std::uint32_t& pop);
bool winsock(State& s,or2x86::Cpu& cpu,std::uint32_t index){
    std::uint32_t pop=0;
    const std::uint32_t eax=winsock_call(s,index,[&](unsigned n){return translated_arg(cpu,n);},pop);
    translated_return(cpu,eax,pop);
    return true;
}
// The Winsock import `index` with its arguments a(0..): EAX, and the argument bytes it pops.
std::uint32_t winsock_call(State& s,std::uint32_t index,const std::function<std::uint32_t(unsigned)>& a,std::uint32_t& pop){
    auto& m=s.memory;auto& p=s.platform;
    auto socket_of=[&](std::uint32_t h)->VirtualSocket*{return h>=1u&&h<=s.sockets.size()&&s.sockets[h-1].used?&s.sockets[h-1]:nullptr;};
    auto bound=[&](VirtualSocket& v)->bool{
        if(v.fd>=0)return true;
        if(!p.open_udp)return false;
        v.fd=p.open_udp(v.port,v.broadcast);
        if(std::getenv("OR2_NET_TRACE"))std::fprintf(stderr,"[net] socket port %u broadcast %d -> fd %d\n",v.port,int(v.broadcast),v.fd);
        return v.fd>=0;};
    std::uint32_t eax=0;pop=0;
    switch(index){
    case WsaStartup:pop=8;{auto* d=m.at(a(1),0x190,true);std::memset(d,0,0x190);d[0]=2;d[1]=2;d[2]=2;d[3]=2;}eax=0;break;
    case Socket:pop=12;
        eax=0xffffffffu;
        if(a(0)==2u&&a(1)==2u)for(std::uint32_t k=0;k<s.sockets.size();++k)if(!s.sockets[k].used){s.sockets[k]={true,false,-1,0};eax=k+1u;break;}
        if(eax==0xffffffffu)s.last_error=10024u;   // WSAEMFILE / not UDP
        break;
    case SetSockOpt:pop=20;{auto* v=socket_of(a(0));
        if(v&&a(1)==0xffffu&&a(2)==0x20u)v->broadcast=m.u32(a(3))!=0u;   // SOL_SOCKET, SO_BROADCAST
        eax=v?0u:0xffffffffu;}break;
    case Bind:pop=12;{auto* v=socket_of(a(0));
        if(!v){eax=0xffffffffu;break;}
        v->port=swap16(m.u16(a(1)+2));
        eax=bound(*v)?0u:0xffffffffu;if(eax)s.last_error=10048u;}break;   // WSAEADDRINUSE
    case IoctlSocket:pop=12;eax=socket_of(a(0))?0u:0xffffffffu;break;          // FIONBIO: the platform sockets are non-blocking
    case CloseSocket:pop=4;if(auto* v=socket_of(a(0))){if(v->fd>=0&&p.close)p.close(v->fd);*v={};}eax=0;break;
    case Htons:case Ntohs:pop=4;eax=swap16(a(0));break;
    case SendTo:{pop=24;auto* v=socket_of(a(0));
        if(!v||!bound(*v)||!p.send_to){eax=0xffffffffu;s.last_error=10038u;break;}
        const std::uint32_t to=a(4);
        const int n=p.send_to(v->fd,m.at(a(1),a(2)),a(2),m.u32(to+4),std::uint16_t(m.u16(to+2)));
        eax=n<0?0xffffffffu:std::uint32_t(n);if(n>0)++stats_.sent;
        if(std::getenv("OR2_NET_TRACE")){const auto ad=m.u32(to+4);
            std::fprintf(stderr,"[net] sendto %u.%u.%u.%u:%u %u bytes from port %u\n",ad&0xffu,(ad>>8)&0xffu,(ad>>16)&0xffu,ad>>24,swap16(m.u16(to+2)),a(2),v->port);}
        break;}
    case RecvFrom:{pop=24;auto* v=socket_of(a(0));
        if(v&&std::getenv("OR2_NET_TRACE")){static std::uint32_t polled=0;const std::uint32_t bit=1u<<(v->port&31u);
            static std::uint32_t polls=0;++polls;
            if(!(polled&bit)||polls%500u==0u){polled|=bit;std::fprintf(stderr,"[net] recvfrom polls port %u (tick %u, poll %u)\n",v->port,stats_.ticks,polls);}}
        if(!v||!bound(*v)||!p.receive_from){eax=0xffffffffu;s.last_error=10038u;break;}
        std::uint32_t address=0;std::uint16_t port=0;
        const int n=p.receive_from(v->fd,m.at(a(1),a(2),true),a(2),address,port);
        if(n<=0){eax=0xffffffffu;s.last_error=n==0?WsaEWouldBlock:10054u;break;}
        if(a(4)){auto* sa=m.at(a(4),16,true);std::memset(sa,0,16);sa[0]=2;std::memcpy(sa+2,&port,2);std::memcpy(sa+4,&address,4);}
        if(a(5))m.put32(a(5),16);
        eax=std::uint32_t(n);++stats_.received;
        if(std::getenv("OR2_NET_TRACE"))std::fprintf(stderr,"[net] recvfrom %u.%u.%u.%u:%u %d bytes on port %u\n",
            address&0xffu,(address>>8)&0xffu,(address>>16)&0xffu,address>>24,swap16(port),n,v->port);
        break;}
    case Recv:case Send:case Connect:case Listen:case Select:case GetHostByName:case InetAddr:
        if(std::getenv("OR2_NET_TRACE"))std::fprintf(stderr,"[net] winsock %u\n",index);
        {char t[64];std::snprintf(t,sizeof t,"Winsock import %u not bridged",index);throw std::runtime_error(t);}
    case InetNtoa:{pop=4;const std::uint32_t v=a(0);char t[20];
        std::snprintf(t,sizeof t,"%u.%u.%u.%u",v&0xffu,(v>>8)&0xffu,(v>>16)&0xffu,v>>24);
        std::memcpy(s.ntoa.data(),t,std::strlen(t)+1);eax=0x0ef90000u;break;}
    case WsaIoctl:{pop=36;
        if(a(1)!=0x4004747fu||!p.interfaces){eax=0xffffffffu;s.last_error=10045u;break;}   // SIO_GET_INTERFACE_LIST only
        const auto list=p.interfaces();const std::uint32_t room=a(5)/0x4cu;std::uint32_t n=0;
        for(const auto& i:list){
            if(n>=room)break;
            auto* e=m.at(a(4)+n*0x4cu,0x4c,true);std::memset(e,0,0x4c);
            const std::uint32_t flags=1u|2u;std::memcpy(e,&flags,4);           // IFF_UP | IFF_BROADCAST
            auto addr=[&](std::size_t off,std::uint32_t v){e[off]=2;std::memcpy(e+off+4,&v,4);};
            addr(4,i.address);addr(0x1c,i.broadcast);addr(0x34,i.netmask);++n;
        }
        if(a(6))m.put32(a(6),n*0x4cu);
        eax=0;break;}
    case WsaGetLastError:pop=0;eax=s.last_error;break;
    default:{char t[64];std::snprintf(t,sizeof t,"Winsock import %u not bridged",index);throw std::runtime_error(t);}
    }
    return eax;
}
std::uint32_t run_list(NativeRuntimeContext& c,std::uint32_t pc,std::uint32_t ecx,const std::vector<std::uint32_t>& args);
std::uint32_t run(NativeRuntimeContext& c,std::uint32_t pc,std::uint32_t ecx=0,std::initializer_list<std::uint32_t> args={}){return run_list(c,pc,ecx,std::vector<std::uint32_t>(args));}
// A call leaving the network code (CRT, heap, frontend owner, race services).
std::uint32_t network_external(NativeRuntimeContext& c,State& s,const PcRaceCall& k){
        auto& m=s.memory;
        std::uint32_t eax=0;
        if(translated_crt_call(m,*s.heap,k,eax))return eax;
        switch(k.pc){
        case 0x4035f0u:return OwnerBase;                     // the frontend owner (7B17E8)
        // Race services of the LAN race tick 45A0C0: the mission timer [836388] and the stage
        // record 44C940 (the course records are the race's).
        case 0x495800u:return c.mission.manager.timer_836388;
        case 0x4505d0u:{std::uint32_t v;std::memcpy(&v,reinterpret_cast<const std::uint8_t*>(&c.race.manager.state)+(0x7d39dcu-RaceManagerState::base),4);return v;}   // mov eax,[7D39DC]
        case 0x44c940u:{   // cache 635F2C / 635F30, else the AREA stage record +8 (as the race end binding)
            auto& rt=c.game_mode.course_runtime;const std::uint32_t key=k.args[0];
            if(key==std::uint32_t(rt.stage_key_635f2c))return std::uint32_t(rt.stage_value_635f30);
            auto& area=native_race_area_memory(c,nullptr,true);
            std::uint32_t v=0;const std::int32_t n=area.i32(0x7d33c4u);const std::uint32_t base=area.u32(0x7d33bcu);
            for(std::int32_t i=0;i<n;++i)if(area.u32(base+std::uint32_t(i)*0x78u+4u)==key){v=area.u32(base+std::uint32_t(i)*0x78u+8u);break;}
            rt.stage_key_635f2c=std::int32_t(key);rt.stage_value_635f30=std::int32_t(v);return v;}
        case 0x4c05f0u:                                      // the arcade event-4 setup of the race (4F5150): native (arcade_attract)
            if(k.args[0]!=0x780440u||!native_race_end_event_invoke(c,0x4c05f0u))throw std::runtime_error("network: 4C05F0 on another work");
            return 0u;
        case 0x580cb0u:                                      // CRT qsort(base, count, width, compare)
            crt_qsort(m,k.args[0],k.args[1],k.args[2],[&](std::uint32_t x,std::uint32_t y){return std::int32_t(run(c,k.args[3],0,{x,y}));});
            return 0u;
        case 0x5816bdu:                                      // CRT `eh vector constructor iterator`(array, size, count, ctor, dtor)
            for(std::uint32_t i=0;i<k.args[2];++i){
                const std::uint32_t element=k.args[0]+i*k.args[1];
                bool frontend=false;for(const auto* f=FrontendCalls;f->pc;++f)frontend|=f->pc==k.args[3];
                std::uint32_t eax=0;
                if(!frontend)(void)run(c,k.args[3],element);
                else if(!native_frontend_owner_call(c,k.args[3],element,nullptr,0,eax))throw std::runtime_error("network: array constructor failed");
            }
            return 0u;
        // Demonware memory 5231D0 (bdAlloc) / 523210 (bdRealloc): the functions bd
        // registered in 85DE8C / 85DE94 (523174..523194), the CRT heap ones; 0 before.
        case 0x5231d0u:return m.u32(0x85de8cu)?s.heap->alloc(k.args[0]):0u;
        // The registered allocators themselves (bdFree 523200 calls [85DE90] = 525170).
        case 0x525150u:return s.heap->alloc(k.args[0]);
        case 0x525170u:s.heap->free(k.args[0]);return 0u;
        case 0x5251e0u:return s.heap->realloc(m,k.args[0],k.args[1]);
        case 0x523210u:return m.u32(0x85de94u)?s.heap->realloc(m,k.args[0],k.args[1]):0u;
        case 0x465eb0u:{                                     // text table entry, copied to the pool
            const auto* table=c.event_function36.frontend_text;
            const auto* t=table?table->get(k.args[0]):nullptr;
            if(!t)throw std::runtime_error("465EB0 text "+std::to_string(k.args[0]));
            const auto n=std::uint32_t(t->size()+1);
            if(s.strings_used+n>s.strings.size())s.strings_used=0;
            std::memcpy(s.strings.data()+s.strings_used,t->c_str(),n);
            const std::uint32_t a=StringBase+s.strings_used;s.strings_used+=(n+3u)&~3u;return a;}
        }
        for(const auto* f=FrontendCalls;f->pc;++f)if(f->pc==k.pc){
            std::uint32_t eax=0;
            if(!native_frontend_owner_call(c,k.pc,k.ecx,k.args.data(),cdecl_leaf(k.pc)?k.args.size():f->bytes/4u,eax)){
                char t[64];std::snprintf(t,sizeof t,"network: frontend call %06X failed",k.pc);throw std::runtime_error(t);}
            return eax;}
        char t[64];std::snprintf(t,sizeof t,"network: PC callee %08X not bridged",k.pc);throw std::runtime_error(t);
}
// The native network code (pc_network_native.inc): true when pc is ported (result in eax).
bool network_native(NativeRuntimeContext& c,State& s,std::uint32_t pc,std::uint32_t ecx,const std::uint32_t* args,std::size_t n,std::uint32_t& eax);
std::uint32_t run_list(NativeRuntimeContext& c,std::uint32_t pc,std::uint32_t ecx,const std::vector<std::uint32_t>& args){
    auto& s=state();
    map_state(c,s);
    auto& m=s.memory;
    {struct Depth{unsigned& d;explicit Depth(unsigned& x):d(x){++d;}~Depth(){--d;}} depth{s.depth};
        std::uint32_t eax=0;
        if(network_native(c,s,pc,ecx,args.data(),args.size(),eax))return eax;}
    TranslatedModule module{network_functions,FrontendCalls,{},nullptr,nullptr,nullptr,network_code_data};
    module.fs_base=TibBase;
    module.crt_random=&c.event_function36.pc_crt_random_state;   // CRT rand() 580F40: the game's seed
    const PcRaceService service=[&](const PcRaceCall& k){return network_external(c,s,k);};
    module.special=[&](or2x86::Cpu& cpu,std::uint32_t target)->bool{
        if(target>=ThunkBase&&target<ThunkBase+IatWords*4u)return winsock(s,cpu,(target-ThunkBase)/4u);
        if(target>=SystemThunkBase&&target<SystemThunkBase+SystemIatWords*4u)return system_import(s,cpu,(target-SystemThunkBase)/4u);
        return false;};
    struct Depth{unsigned& d;explicit Depth(unsigned& x):d(x){++d;}~Depth(){--d;}} depth{s.depth};
    return translated_call_list(m,service,module,pc,TranslatedRegisters{0,ecx,0,0,0,0},args);
}
#include "platform/pc_network_native.inc"
}
void native_network_map_shared(PcRaceMemory& m){
    auto& s=state();if(!s.mapped)return;
    static constexpr struct {std::uint32_t base,size;} Shared[]{
        {0x7dd139u,0x7de418u-0x7dd139u},   // the players' car states (7DD140 + slot * 0x104..)
        {0x7e0418u,0x7f1840u-0x7e0418u},   // the received position history (7EE240 + player * 0x900..)
        {0x850b00u,0x40u},                 // the race start state (4F5320 / 4F53B0 / 4F5380)
        {0x7f9460u,0x200u},                // the LAN race manager 7F9460
        {0x7d688cu,0x7dd138u-0x7d688cu},   // start counters 7D68A4 / 7D68A8, sessions 7D68AC / 7D68B0 and the session objects 7D6958..
        {HeapBase,HeapSize}};              // the sessions' objects
    // Every network region inside a window, clipped, in the network memory's order (its overlays
    // keep their precedence).
    for(const auto& r:Shared)for(const auto& g:s.memory.regions()){
        const std::uint64_t lo=std::max<std::uint64_t>(g.base,r.base),hi=std::min<std::uint64_t>(std::uint64_t(g.base)+g.size,std::uint64_t(r.base)+r.size);
        if(lo>=hi)continue;
        if(g.writable)m.map(std::uint32_t(lo),g.data+(lo-g.base),std::size_t(hi-lo));
        else m.map_const(std::uint32_t(lo),g.data+(lo-g.base),std::size_t(hi-lo));
    }
}
void native_network_set_platform(PcNetworkPlatform p){state().platform=std::move(p);}
NativeNetworkStats& native_network_stats(){return stats_;}
bool native_network_invoke(NativeRuntimeContext& c,std::uint32_t pc,std::uint32_t ecx,std::initializer_list<std::uint32_t> args,std::uint32_t* eax){
    if(stats_.failures>=8u||!state().started)return false;
    try{const auto v=run(c,pc,ecx,args);if(eax)*eax=v;return true;}
    catch(const PcRaceUnmapped& u){char t[96];std::snprintf(t,sizeof t,"%06X: unmapped PC address %08X",pc,u.address);stats_.last_error=t;}
    catch(const std::exception& e){char t[16];std::snprintf(t,sizeof t,"%06X: ",pc);stats_.last_error=t+std::string(e.what());}
    ++stats_.failures;
    if(std::getenv("OR2_NET_DEBUG"))std::fprintf(stderr,"[net] %s\n",stats_.last_error.c_str());
    return false;
}
std::uint8_t native_network_u8(std::uint32_t address){
    auto& s=state();if(!s.mapped||!s.memory.mapped(address,1))return 0;
    return s.memory.u8(address);
}
std::uint32_t native_network_u32(std::uint32_t address){
    auto& s=state();if(!s.mapped||!s.memory.mapped(address,4))return 0;
    return s.memory.u32(address);
}
void native_network_put8(std::uint32_t address,std::uint8_t value){
    auto& s=state();if(s.mapped&&s.memory.mapped(address,1))s.memory.put8(address,value);
}
std::uint8_t* native_network_view(std::uint32_t address,std::uint32_t size){
    auto& s=state();if(!s.mapped||!size||!s.memory.mapped(address,size))return nullptr;
    return s.memory.at(address,size,true);
}
std::string native_network_string(std::uint32_t address){
    auto& s=state();if(!s.mapped||!address)return {};
    try{return guest_string(s.memory,address);}catch(const std::exception&){return {};}
}
std::string native_network_format(std::uint32_t format,const std::uint32_t* args,std::size_t n){
    auto& s=state();if(!s.mapped||!format)return {};
    std::size_t next=0;
    return guest_format(s.memory,format,[&]{return next<n?args[next++]:0u;});
}
std::uint32_t native_network_size(std::uint32_t address){auto& s=state();return s.heap?s.heap->size_of(address):0u;}
std::uint32_t native_network_screen_create(NativeRuntimeContext& c,std::uint32_t factory){
    std::uint32_t object=0;
    return native_network_invoke(c,factory,0,{},&object)?object:0u;
}
bool native_network_screen_slot(NativeRuntimeContext& c,std::uint32_t object,std::uint32_t slot,std::uint32_t& result){
    auto& s=state();result=0;
    if(!s.mapped||!s.memory.mapped(object,4))return false;
    const auto vtable=s.memory.u32(object);
    if(!s.memory.mapped(vtable+slot,4))return false;
    const auto method=s.memory.u32(vtable+slot);
    // Slot 0 is the deleting destructor (argument 1: free the object).
    return slot==0?native_network_invoke(c,method,object,{1u},&result):native_network_invoke(c,method,object,{},&result);
}
bool native_network_mapped(){return state().mapped&&state().started&&stats_.failures<8u;}
void native_network_put32(std::uint32_t address,std::uint32_t value){
    auto& s=state();if(s.mapped&&s.memory.mapped(address,4))s.memory.put32(address,value);
}
void native_network_put_string(std::uint32_t address,const std::string& value){
    auto& s=state();if(!s.mapped||!s.memory.mapped(address,std::uint32_t(value.size()+1)))return;
    guest_put_string(s.memory,address,value);
}
bool native_network_tick_454670(NativeRuntimeContext& c){
    if(stats_.failures>=8u)return false;   // latched: the layer stops after repeated faults (reported once each)
    try{
        // 45ACB0 (startup, 417740): bd memory, the service lists, 494270(0) on 83612C (state 1: WSAStartup, sockets).
        if(!state().started){
            state().started=true;
            // The C++ static initializers of the network objects (the __xc table 62EAE8..
            // 62F120, in its order): sessions 7D6958 / 7DCBE0 / 7DBDC8, 830BEC, the service
            // 83612C, 85B358 / 85DED8 and the LAN session chain 988DB4..98AB70.
            {auto& s=state();map_state(c,s);N(c,s).init_objects();}
            (void)run(c,0x45acb0u);
        }
        (void)run(c,0x454670u);++stats_.ticks;
        if(std::getenv("OR2_NET_DEBUG")&&stats_.ticks%60u==1u){auto& m=state().memory;
            const auto session=m.u32(0x7d68acu);
            std::fprintf(stderr,"[net] tick %u state %u ready %u sent %u received %u session %08X active %u host %u started %u lan %u/%u start %u/%u flags %X versus %u variant %u\n",stats_.ticks,m.u32(0x836140u),m.u8(0x830c30u),
                stats_.sent,stats_.received,session,session?m.u8(session+5):0u,session?m.u8(session+7):0u,session?m.u8(session+8):0u,m.u32(0x830bf0u),m.u8(0x830c00u),
                m.u32(0x7d68a4u),m.u32(0x7d68a8u),m.u32(0x659944u),m.u32(0x8369c0u),m.u32(0x780258u));
            std::fprintf(stderr,"[net]   mode %u requested %u pending %u event4 %02X work4 %08X %08X\n",c.mode_state.current,c.mode_state.requested,c.mode_state.transition_pending,
                c.event_state.slots[4].flags,m.u32(0x780440u),m.u32(0x780444u));}
        return true;}
    catch(const PcRaceUnmapped& u){char t[96];std::snprintf(t,sizeof t,"454670: unmapped PC address %08X",u.address);stats_.last_error=t;}
    catch(const std::exception& e){stats_.last_error=std::string("454670: ")+e.what();}
    ++stats_.failures;
    if(std::getenv("OR2_NET_DEBUG")){static std::string printed;if(printed!=stats_.last_error){printed=stats_.last_error;std::fprintf(stderr,"[net] %s\n",printed.c_str());}}
    return false;
}
}
