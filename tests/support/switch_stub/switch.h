#pragma once
#include <cstdint>
using u64=std::uint64_t;
using s64=std::int64_t;
using u32=std::uint32_t;
using s32=std::int32_t;
using Handle=std::uint32_t;
inline Handle threadGetCurHandle(){return 0u;}
using Result=std::uint32_t;
constexpr Result RESULT_OK=0;
inline Result svcSetThreadCoreMask(Handle,s32,u32){return 0u;}
#define R_SUCCEEDED(x) ((x)==RESULT_OK)
inline u64 armGetSystemTick(){return 123456u;}
inline u64 armGetSystemTickFreq(){return 19200000u;}
inline void svcSleepThread(s64){}
struct Thread{int unused;};
typedef void (*ThreadFunc)(void*);
inline Result threadCreate(Thread*,ThreadFunc,void*,void*,size_t,int,int){return 1u;}
inline Result threadStart(Thread*){return 1u;}
inline Result threadWaitForExit(Thread*){return 1u;}
inline Result threadClose(Thread*){return 1u;}
inline bool appletMainLoop(){return true;}
// nxlink stdio (never active in the stub: no nxlink host).
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
inline void padConfigureInput(int,u64){}
inline void padInitializeDefault(PadState*){}
inline void padUpdate(PadState*){}
inline bool padIsConnected(const PadState*){return true;}
inline u64 padGetButtons(const PadState*){return 0u;}
inline u64 padGetButtonsDown(const PadState*){static unsigned calls=0;return ++calls>2u?HidNpadButton_Plus:0u;}
inline u64 padGetButtonsUp(const PadState*){return 0u;}
inline HidAnalogStickState padGetStickPos(const PadState*,unsigned i){return i==0u?HidAnalogStickState{16384,-8192}:HidAnalogStickState{};}
