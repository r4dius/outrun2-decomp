#define SDL_MAIN_HANDLED
#include <SDL.h>
#include "platform_api.hpp"
#include "platform/retail_asset_store.hpp"
#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <thread>
#ifdef OR2_PS5_PAYLOAD
extern "C" {
// Open-source ABI from ps5-payload-dev/SDL, retained verbatim with license.
#include "../third_party/sdl-pad/SDL_ps5joystick.h"
}
#endif
namespace {
bool initialized=false,running=true;
std::string retail_root;
unsigned input_frame=0;
#ifdef OR2_PS5_PAYLOAD
int pad_handle=-1;bool pad_owned=false;
#else
SDL_GameController* controller=nullptr;
#endif
const char* env(const char* name){const auto* p=std::getenv(name);return p&&*p?p:nullptr;}
u64 parse_buttons(const std::string& text){
    const struct {const char* name;u64 bit;} buttons[]{
        {"A",Button_A},{"B",Button_B},{"X",Button_X},{"Y",Button_Y},
        {"L",Button_L},{"R",Button_R},{"ZL",Button_ZL},{"ZR",Button_ZR},
        {"PLUS",Button_Plus},{"MINUS",Button_Minus},{"LS",Button_StickL},{"RS",Button_StickR},
        {"LEFT",Button_Left},{"RIGHT",Button_Right},{"UP",Button_Up},{"DOWN",Button_Down}};
    u64 value=0;std::size_t p=0;
    while(p<text.size()){auto e=text.find('+',p);if(e==std::string::npos)e=text.size();const auto name=text.substr(p,e-p);
        for(auto b:buttons)if(name==b.name)value|=b.bit;p=e+1;}
    return value;
}
#ifdef OR2_PS5_HOST_TEST
u64 script_buttons(const char* name,unsigned frame){
    const auto* value=env(name);if(!value)return 0;const std::string text(value);std::size_t p=0;u64 bits=0;
    while(p<text.size()){auto e=text.find(',',p);if(e==std::string::npos)e=text.size();auto item=text.substr(p,e-p);auto c=item.find(':');
        if(c!=std::string::npos){auto range=item.substr(0,c);auto dash=range.find('-');const auto a=std::strtoul(range.c_str(),nullptr,10);
            const auto b=dash==std::string::npos?a:std::strtoul(range.c_str()+dash+1,nullptr,10);
            if(frame>=a&&frame<=b)bits|=parse_buttons(item.substr(c+1));}p=e+1;}
    return bits;
}
#endif
void read_pad(outrun::ps5::PadSample& sample){
    using namespace outrun::ps5;
    sample=PadSample{};
#ifdef OR2_PS5_PAYLOAD
    if(pad_handle<0){
        int users[4]{-1,-1,-1,-1};
        if(sceUserServiceGetLoginUserIdList(users)!=0)return;
        for(int user:users)if(user!=-1){
            pad_handle=scePadOpen(user,PS5_PAD_PORT_TYPE_STANDARD,0,nullptr);
            if(pad_handle>=0){pad_owned=true;break;}
            if(std::uint32_t(pad_handle)==PS5_PAD_ERROR_ALREADY_OPENED){
                pad_handle=scePadGetHandle(user,PS5_PAD_PORT_TYPE_STANDARD,0);pad_owned=false;
                if(pad_handle>=0)break;
            }
        }
    }
    if(pad_handle<0)return;
    PS5_PadData data{};
    if(scePadReadState(pad_handle,&data)!=0||!data.connected){
        if(pad_owned)scePadClose(pad_handle);pad_handle=-1;pad_owned=false;return;
    }
    sample.connected=true;
    sample.left_x=data.leftStick.x;sample.left_y=data.leftStick.y;
    sample.right_x=data.rightStick.x;sample.right_y=data.rightStick.y;
    sample.l2=data.analogButtons.l2;sample.r2=data.analogButtons.r2;
    const struct {std::uint32_t raw,logical;} mapping[]{
        {PS5_PAD_BUTTON_CROSS,Cross},{PS5_PAD_BUTTON_CIRCLE,Circle},{PS5_PAD_BUTTON_SQUARE,Square},{PS5_PAD_BUTTON_TRIANGLE,Triangle},
        {PS5_PAD_BUTTON_OPTIONS,Options},{PS5_PAD_BUTTON_L1,L1},{PS5_PAD_BUTTON_R1,R1},{PS5_PAD_BUTTON_L2,L2},{PS5_PAD_BUTTON_R2,R2},
        {PS5_PAD_BUTTON_UP,Up},{PS5_PAD_BUTTON_DOWN,Down},{PS5_PAD_BUTTON_LEFT,Left},{PS5_PAD_BUTTON_RIGHT,Right},
        {PS5_PAD_BUTTON_L3,StickL},{PS5_PAD_BUTTON_R3,StickR},{PS5_PAD_BUTTON_TOUCH_PAD,Share}};
    for(auto m:mapping)if(data.buttons&m.raw)sample.buttons|=m.logical;
#else
    if(controller&&!SDL_GameControllerGetAttached(controller)){SDL_GameControllerClose(controller);controller=nullptr;}
    if(!controller)for(int i=0;i<SDL_NumJoysticks();++i)if(SDL_IsGameController(i)){controller=SDL_GameControllerOpen(i);if(controller)break;}
    if(controller){
        sample.connected=true;
        auto axis=[&](SDL_GameControllerAxis a){const int v=SDL_GameControllerGetAxis(controller,a);
            return std::uint8_t(v<0?128+v*128/32768:128+v*127/32767);};
        sample.left_x=axis(SDL_CONTROLLER_AXIS_LEFTX);sample.left_y=axis(SDL_CONTROLLER_AXIS_LEFTY);
        sample.right_x=axis(SDL_CONTROLLER_AXIS_RIGHTX);sample.right_y=axis(SDL_CONTROLLER_AXIS_RIGHTY);
        sample.l2=std::uint8_t(std::max(0,int(SDL_GameControllerGetAxis(controller,SDL_CONTROLLER_AXIS_TRIGGERLEFT)))*255/32767);
        sample.r2=std::uint8_t(std::max(0,int(SDL_GameControllerGetAxis(controller,SDL_CONTROLLER_AXIS_TRIGGERRIGHT)))*255/32767);
        const struct {SDL_GameControllerButton raw;std::uint32_t logical;} mapping[]{
            {SDL_CONTROLLER_BUTTON_A,Cross},{SDL_CONTROLLER_BUTTON_B,Circle},{SDL_CONTROLLER_BUTTON_X,Square},{SDL_CONTROLLER_BUTTON_Y,Triangle},
            {SDL_CONTROLLER_BUTTON_START,Options},{SDL_CONTROLLER_BUTTON_BACK,Share},{SDL_CONTROLLER_BUTTON_LEFTSHOULDER,L1},{SDL_CONTROLLER_BUTTON_RIGHTSHOULDER,R1},
            {SDL_CONTROLLER_BUTTON_DPAD_LEFT,Left},{SDL_CONTROLLER_BUTTON_DPAD_RIGHT,Right},{SDL_CONTROLLER_BUTTON_DPAD_UP,Up},{SDL_CONTROLLER_BUTTON_DPAD_DOWN,Down},
            {SDL_CONTROLLER_BUTTON_LEFTSTICK,StickL},{SDL_CONTROLLER_BUTTON_RIGHTSTICK,StickR}};
        for(auto m:mapping)if(SDL_GameControllerGetButton(controller,m.raw))sample.buttons|=m.logical;
        if(sample.l2>127)sample.buttons|=L2;if(sample.r2>127)sample.buttons|=R2;
    }
#ifdef OR2_PS5_HOST_TEST
    if(env("OR2_HOST_SCRIPT")||env("OR2_HOST_HOLD")||env("OR2_HOST_FRAMES")){
        sample.connected=true;sample.buttons|=std::uint32_t(script_buttons("OR2_HOST_SCRIPT",input_frame)|script_buttons("OR2_HOST_HOLD",input_frame));
        if(const auto* last=env("OR2_HOST_FRAMES"))if(input_frame>=std::strtoul(last,nullptr,10))sample.buttons|=Options|L1|R1;
        if(sample.buttons&R2)sample.r2=255;if(sample.buttons&L2)sample.l2=255;
    }
#endif
#endif
}
}
bool application_initialize(std::string& error){
    SDL_SetMainReady();
    auto flags=Uint32(SDL_INIT_AUDIO|SDL_INIT_EVENTS);
#if !defined(OR2_PS5_OPENGL) && !defined(OR2_PS5_VULKAN)
    flags|=SDL_INIT_VIDEO;
#endif
#ifndef OR2_PS5_PAYLOAD
    flags|=SDL_INIT_GAMECONTROLLER;
#endif
    if(SDL_Init(flags)<0){error=SDL_GetError();return false;}
    initialized=true;running=true;
#ifdef OR2_PS5_PAYLOAD
    // SDL video/audio may already own UserService; a login query below is the
    // readiness check, not an assumed successful initialization return value.
    const int users_rc=sceUserServiceInitialize(nullptr);const int pad_rc=scePadInit();
    std::fprintf(stdout,"PS5 input init: UserService=%08x Pad=%08x\n",unsigned(users_rc),unsigned(pad_rc));
#endif
    return true;
}
void application_shutdown(){
#ifdef OR2_PS5_PAYLOAD
    if(pad_owned&&pad_handle>=0)scePadClose(pad_handle);pad_handle=-1;pad_owned=false;
#else
    if(controller)SDL_GameControllerClose(controller);controller=nullptr;
#endif
    if(initialized)SDL_Quit();initialized=false;running=false;
}
bool application_running(){return running;}
void platform_sleep_ns(s64 ns){if(ns>0)std::this_thread::sleep_for(std::chrono::nanoseconds(ns));else std::this_thread::yield();}
std::string application_home(int argc,char** argv){
    std::string home=env("OR2_PS5_HOME")?env("OR2_PS5_HOME"):
#ifdef OR2_PS5_PAYLOAD
        "/data/OutRunPS5";
#else
        "./OutRunPS5-user";
#endif
    if(env("OR2_GAME_ROOT"))retail_root=env("OR2_GAME_ROOT");
    bool explicit_home=env("OR2_PS5_HOME")!=nullptr;
    for(int i=1;i<argc;++i){const std::string a=argv[i]?argv[i]:"";
        if(a.rfind("--home=",0)==0){home=a.substr(7);explicit_home=true;}
        if(a.rfind("--data=",0)==0)retail_root=a.substr(7);}
    std::error_code ec;std::filesystem::create_directories(home+"/SaveGame",ec);
#ifdef OR2_PS5_NATIVE_TITLE
    if(ec&&!explicit_home){
        home="/download0/OutRunPS5";ec.clear();std::filesystem::create_directories(home+"/SaveGame",ec);
        std::fprintf(stdout,"PS5 native home fallback: %s\n",home.c_str());
    }
#else
    (void)explicit_home;
#endif
    if(ec)throw std::runtime_error("cannot create PS5 application home: "+ec.message());
    if(retail_root.empty()){std::ifstream file(home+"/retail-root.txt");std::getline(file,retail_root);if(!retail_root.empty()&&retail_root.back()=='\r')retail_root.pop_back();}
    // The dedicated folder of the title (copied with it, or sent by FTP next to
    // an installed package), then older places.
    if(retail_root.empty())for(const char* path:{"/app0/assets/game","/data/homebrew/PPSA99106/assets/game","/app0/OutRun2006","/mnt/usb0/OutRun2006","/mnt/usb1/OutRun2006","/data/OutRun2006","/download0/OutRun2006","./OutRun2006"})
        if(outrun::platform::retail_asset_store_is_game_root(path)){retail_root=path;break;}
    return home;
}
std::string application_retail_root(){return retail_root;}
// Payload stdout is the loader's real log channel; no libnx console exists.
void consoleInit(void*){std::setvbuf(stdout,nullptr,_IOLBF,0);}void consoleUpdate(void*){std::fflush(stdout);}void consoleExit(void*){std::fflush(stdout);}
void padConfigureInput(int,u64){}void padInitializeDefault(PadState* p){*p=PadState{};}
void padUpdate(PadState* p){
    SDL_Event event;while(SDL_PollEvent(&event))if(event.type==SDL_QUIT)running=false;
    ++input_frame;read_pad(p->sample);
    const auto held=p->sample.connected?p->sample.buttons:0u;p->down=held&~p->previous;p->up=p->previous&~held;p->previous=held;
}
bool padIsConnected(const PadState* p){return p->sample.connected;}
u64 padGetButtons(const PadState* p){return p->sample.buttons;}u64 padGetButtonsDown(const PadState* p){return p->down;}u64 padGetButtonsUp(const PadState* p){return p->up;}
HidAnalogStickState padGetStickPos(const PadState* p,unsigned stick){
    const auto x=stick?p->sample.right_x:p->sample.left_x,y=stick?p->sample.right_y:p->sample.left_y;
    return {(int(x)-128)*256,-(int(y)-128)*256};
}
