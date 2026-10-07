#include "platform_api.hpp"
#include "native_window.hpp"
#include "platform/retail_asset_store.hpp"
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.Gaming.Input.h>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <thread>
namespace {
namespace wgi=winrt::Windows::Gaming::Input;
bool running=true;
std::string retail_root;
// -1..1 (y up) to the DualSense byte (0..255, 128 centre, y down).
std::uint8_t axis(double v,bool invert){v=std::clamp(invert?-v:v,-1.0,1.0);return std::uint8_t(std::clamp<long>(std::lround(128.0+v*(v<0?128.0:127.0)),0,255));}
// The first connected gamepad, in the shared DualSense layout (src/input/dual_sense.hpp).
void read_pad(outrun::ps5::PadSample& sample){
    using namespace outrun::ps5;
    sample=PadSample{};
    const auto pads=wgi::Gamepad::Gamepads();
    if(pads.Size()==0)return;
    const auto r=pads.GetAt(0).GetCurrentReading();
    sample.connected=true;
    sample.left_x=axis(r.LeftThumbstickX,false);sample.left_y=axis(r.LeftThumbstickY,true);
    sample.right_x=axis(r.RightThumbstickX,false);sample.right_y=axis(r.RightThumbstickY,true);
    sample.l2=std::uint8_t(std::lround(std::clamp(r.LeftTrigger,0.0,1.0)*255));sample.r2=std::uint8_t(std::lround(std::clamp(r.RightTrigger,0.0,1.0)*255));
    using B=wgi::GamepadButtons;
    const struct {B raw;std::uint32_t logical;} mapping[]{
        {B::A,Cross},{B::B,Circle},{B::X,Square},{B::Y,Triangle},{B::Menu,Options},{B::View,Share},
        {B::LeftShoulder,L1},{B::RightShoulder,R1},{B::DPadLeft,Left},{B::DPadRight,Right},{B::DPadUp,Up},{B::DPadDown,Down},
        {B::LeftThumbstick,StickL},{B::RightThumbstick,StickR}};
    for(auto m:mapping)if((r.Buttons&m.raw)==m.raw)sample.buttons|=m.logical;
    if(sample.l2>127)sample.buttons|=L2;if(sample.r2>127)sample.buttons|=R2;
}
bool writable(const std::string& folder){
    const auto probe=folder+"/.or2-write-test";
    std::FILE* f=std::fopen(probe.c_str(),"wb");if(!f)return false;std::fclose(f);std::remove(probe.c_str());return true;
}
}
bool application_initialize(std::string&){running=true;return true;}
void application_shutdown(){running=false;}
bool application_running(){return running;}
void platform_sleep_ns(s64 ns){if(ns>0)std::this_thread::sleep_for(std::chrono::nanoseconds(ns));else std::this_thread::yield();}
// The game folder is the home: the original data is read there and the
// cache, saves, options and logs are written there, as the PC executable
// does in its installation folder. PC build: the executable's folder (it is
// copied into the Steam game folder). Xbox: OutRun2006 at the root of a USB
// drive, then the application's LocalState\OutRun2006.
std::string application_home(int argc,char** argv){
    for(int i=1;i<argc;++i){const std::string a=argv[i]?argv[i]:"";if(a.rfind("--data=",0)==0)retail_root=a.substr(7);}
    if(retail_root.empty()){
        std::vector<std::string> candidates{outrun::xbox_runtime::launch_folder()};
        for(char drive='D';drive<='Z';++drive)candidates.push_back(std::string(1,drive)+":/OutRun2006");
        if(auto local=outrun::xbox_runtime::local_state_folder();!local.empty())candidates.push_back(local+"/OutRun2006");
        for(const auto& path:candidates)if(!path.empty()&&outrun::platform::retail_asset_store_is_game_root(path)){retail_root=path;break;}
    }
    if(retail_root.empty())throw std::runtime_error("OutRun 2006 game folder not found (USB drive OutRun2006, LocalState\\OutRun2006 or next to the executable)");
    if(!writable(retail_root))throw std::runtime_error("the game folder is read-only: "+retail_root);
    std::error_code ec;
    std::filesystem::create_directories(retail_root+"/SaveGame",ec);
    if(ec)throw std::runtime_error("cannot create SaveGame: "+ec.message());
    return retail_root;
}
std::string application_retail_root(){return retail_root;}
// stdout goes to runtime.log in the game folder (window_*.cpp); no text console.
void consoleInit(void*){}void consoleUpdate(void*){std::fflush(stdout);}void consoleExit(void*){std::fflush(stdout);}
void padConfigureInput(int,u64){}void padInitializeDefault(PadState* p){*p=PadState{};}
void padUpdate(PadState* p){
    if(!outrun::xbox_runtime::pump_window_events())running=false;
    read_pad(p->sample);
    const auto held=p->sample.connected?p->sample.buttons:0u;p->down=held&~p->previous;p->up=p->previous&~held;p->previous=held;
}
bool padIsConnected(const PadState* p){return p->sample.connected;}
u64 padGetButtons(const PadState* p){return p->sample.buttons;}u64 padGetButtonsDown(const PadState* p){return p->down;}u64 padGetButtonsUp(const PadState* p){return p->up;}
HidAnalogStickState padGetStickPos(const PadState* p,unsigned stick){
    const auto x=stick?p->sample.right_x:p->sample.left_x,y=stick?p->sample.right_y:p->sample.left_y;
    return {(int(x)-128)*256,-(int(y)-128)*256};
}
