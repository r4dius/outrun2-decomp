#pragma once
// Host replacement for libnx used by tools/host/host_nro: runs the real
// switch/source/main.cpp on the PC with a scripted controller.
//   OR2_HOST_SCRIPT  "frame:buttons,frame:buttons,..." (buttons as the
//                    HidNpadButton bit names A,B,X,Y,L,R,ZL,ZR,PLUS,LEFT,UP,
//                    RIGHT,DOWN joined by '+'); each press lasts one frame.
//   OR2_HOST_HOLD    same syntax, "frame-frame:buttons" ranges held.
//   OR2_HOST_FRAMES  frame at which L+R+Plus is sent (exit), default 600.
// A frame is one padUpdate call.
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <string>
#include <thread>
using u64=std::uint64_t;
using s64=std::int64_t;
using u32=std::uint32_t;
using s32=std::int32_t;
using Result=std::uint32_t;
constexpr Result RESULT_OK=0;
#define R_SUCCEEDED(x) ((x)==RESULT_OK)
inline u64 armGetSystemTick_wall(){return u64(std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now().time_since_epoch()).count())/52u;}   // ~19.2 MHz
// Host lockstep runs (OR2_HOST_FIXEDCLOCK): ticks advance a fixed 1 ms per read.
inline u64 armGetSystemTick(){static const bool fixed=std::getenv("OR2_HOST_FIXEDCLOCK")!=nullptr;static u64 t=0;if(fixed)return t+=19200u;return armGetSystemTick_wall();}
inline u64 armGetSystemTickFreq(){return 19200000u;}
inline void svcSleepThread(s64){}
inline bool appletMainLoop(){return true;}
// Operation mode and clocks (options.ini): the host is "handheld" with system clocks.
enum AppletOperationMode{AppletOperationMode_Handheld=0,AppletOperationMode_Console=1};
inline AppletOperationMode appletGetOperationMode(){return AppletOperationMode_Handheld;}
#define R_FAILED(x) ((x)!=RESULT_OK)
enum PcvModuleId{PcvModuleId_CpuBus=0x40000001,PcvModuleId_GPU=0x40000002};
struct ClkrstSession{int unused;};
inline bool hosversionAtLeast(int,int,int){return false;}
inline Result clkrstInitialize(){return 1u;}
inline void clkrstExit(){}
inline Result clkrstOpenSession(ClkrstSession*,PcvModuleId,u32){return 1u;}
inline void clkrstCloseSession(ClkrstSession*){}
inline Result clkrstSetClockRate(ClkrstSession*,u32){return 1u;}
inline Result clkrstGetClockRate(ClkrstSession*,u32*){return 1u;}
// nxlink stdio (never active on the host: no nxlink host).
struct NxlinkHostStub { u32 s_addr{}; };
inline NxlinkHostStub __nxlink_host{};
inline Result socketInitializeDefault(){return 1u;}
inline int nxlinkStdio(){return -1;}
inline void socketExit(){}
inline void consoleInit(void*){}
inline void consoleUpdate(void*){}
inline void consoleExit(void*){}
inline Result fsdevMountSdmc(){return RESULT_OK;}
inline int fsdevUnmountDevice(const char*){return 0;}
struct FsFileSystem{};
inline FsFileSystem* fsdevGetDeviceFileSystem(const char*){static FsFileSystem fs;return &fs;}
inline Result romfsInit(){return RESULT_OK;}
inline Result romfsExit(){return RESULT_OK;}
struct PadState{};
struct HidAnalogStickState{s32 x{};s32 y{};};
constexpr u64 HidNpadStyleSet_NpadStandard=1u;
constexpr u64 HidNpadButton_A=1u<<0;
constexpr u64 HidNpadButton_B=1u<<1;
constexpr u64 HidNpadButton_X=1u<<2;
constexpr u64 HidNpadButton_Y=1u<<3;
constexpr u64 HidNpadButton_StickL=1u<<4;
constexpr u64 HidNpadButton_StickR=1u<<5;
constexpr u64 HidNpadButton_L=1u<<6;
constexpr u64 HidNpadButton_R=1u<<7;
constexpr u64 HidNpadButton_ZL=1u<<8;
constexpr u64 HidNpadButton_ZR=1u<<9;
constexpr u64 HidNpadButton_Plus=1u<<10;
constexpr u64 HidNpadButton_Minus=1u<<11;
constexpr u64 HidNpadButton_Left=1u<<12;
constexpr u64 HidNpadButton_Up=1u<<13;
constexpr u64 HidNpadButton_Right=1u<<14;
constexpr u64 HidNpadButton_Down=1u<<15;
// Host-only pseudo buttons: the left stick pushed fully (padGetStickPos).
constexpr u64 HostStickLeft=1ull<<40,HostStickRight=1ull<<41,HostStickUp=1ull<<42,HostStickDown=1ull<<43,HostStickHalf=1ull<<44;
namespace host_pad {
inline unsigned& frame(){static unsigned f=0;return f;}
inline u64 parse_buttons(const std::string& s){
    u64 m=0;std::size_t p=0;
    while(p<=s.size()){
        const auto e=s.find('+',p);const auto n=s.substr(p,e==std::string::npos?std::string::npos:e-p);
        static const struct{const char* n;u64 b;} names[]{{"A",HidNpadButton_A},{"B",HidNpadButton_B},{"X",HidNpadButton_X},{"Y",HidNpadButton_Y},
            {"L",HidNpadButton_L},{"R",HidNpadButton_R},{"ZL",HidNpadButton_ZL},{"ZR",HidNpadButton_ZR},{"PLUS",HidNpadButton_Plus},
            {"MINUS",HidNpadButton_Minus},{"LS",HidNpadButton_StickL},{"RS",HidNpadButton_StickR},{"LEFT",HidNpadButton_Left},{"UP",HidNpadButton_Up},{"RIGHT",HidNpadButton_Right},{"DOWN",HidNpadButton_Down},
            {"SLEFT",HostStickLeft},{"SHALF",HostStickHalf},{"SRIGHT",HostStickRight},{"SUP",HostStickUp},{"SDOWN",HostStickDown}};
        for(const auto& x:names)if(n==x.n)m|=x.b;
        if(e==std::string::npos)break;p=e+1;
    }
    return m;
}
// Buttons of `var` active on frame f ("a:B" single frames, "a-b:B" ranges).
inline u64 lookup(const char* var,unsigned f){
    const char* v=std::getenv(var);if(!v)return 0;const std::string s(v);u64 m=0;std::size_t p=0;
    while(p<s.size()){
        auto e=s.find(',',p);if(e==std::string::npos)e=s.size();
        const auto item=s.substr(p,e-p);const auto c=item.find(':');
        if(c!=std::string::npos){
            const auto range=item.substr(0,c);const auto d=range.find('-');
            const unsigned a=unsigned(std::strtoul(range.c_str(),nullptr,10));
            const unsigned b=d==std::string::npos?a:unsigned(std::strtoul(range.c_str()+d+1,nullptr,10));
            if(f>=a&&f<=b)m|=parse_buttons(item.substr(c+1));
        }
        p=e+1;
    }
    return m;
}
inline unsigned exit_frame(){const char* v=std::getenv("OR2_HOST_FRAMES");return v?unsigned(std::strtoul(v,nullptr,10)):600u;}
inline u64 held(){const auto f=frame();if(f>=exit_frame())return HidNpadButton_L|HidNpadButton_R|HidNpadButton_Plus;
    return lookup("OR2_HOST_SCRIPT",f)|lookup("OR2_HOST_HOLD",f);}
inline u64 down(){const auto f=frame();if(f>=exit_frame())return HidNpadButton_Plus;
    const u64 prev=f?(lookup("OR2_HOST_SCRIPT",f-1)|lookup("OR2_HOST_HOLD",f-1)):0;return held()&~prev;}
}
inline void padConfigureInput(int,u64){}
inline void padInitializeDefault(PadState*){}
// OR2_HOST_PACE=1: one pad update per 1/60 s (several instances talking over the test LAN).
inline void padUpdate(PadState*){
    ++host_pad::frame();
    if(std::getenv("OR2_HOST_PACE")){
        static const auto start=std::chrono::steady_clock::now();
        std::this_thread::sleep_until(start+std::chrono::microseconds(16667ull*host_pad::frame()));
    }
}
inline bool padIsConnected(const PadState*){return true;}
inline u64 padGetButtons(const PadState*){return host_pad::held();}
inline u64 padGetButtonsDown(const PadState*){return host_pad::down();}
inline u64 padGetButtonsUp(const PadState*){return 0u;}
inline HidAnalogStickState padGetStickPos(const PadState*,unsigned i){
    if(i!=0u)return {};
    const u64 h=host_pad::held();
    const s32 v=(h&HostStickHalf)?16384:32767;   // SHALF: half deflection
    return {(h&HostStickLeft)?-v:(h&HostStickRight)?v:0,(h&HostStickDown)?-v:(h&HostStickUp)?v:0};}
