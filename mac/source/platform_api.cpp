// macOS platform adapter; uses the portable input mapping from the reference.
#define SDL_MAIN_HANDLED
#include <SDL.h>
#include "platform_api.hpp"
#include "mac_paths.hpp"
#include "system/files.hpp"
#include <chrono>
#include <cstdlib>
#include <cstdio>
#include <filesystem>
#include <stdexcept>
#include <thread>
#include <algorithm>
namespace {
bool initialized=false,running=true;
std::string retail,cache;
SDL_GameController* controller=nullptr;
void read_pad(outrun::ps5::PadSample& p){
    using namespace outrun::ps5;
    p=PadSample{};p.connected=true;
    if(controller&&!SDL_GameControllerGetAttached(controller)){SDL_GameControllerClose(controller);controller=nullptr;}
    if(!controller)for(int i=0;i<SDL_NumJoysticks();++i)if(SDL_IsGameController(i)){controller=SDL_GameControllerOpen(i);if(controller)break;}
    if(controller){
        auto axis=[&](SDL_GameControllerAxis a){const int v=SDL_GameControllerGetAxis(controller,a);return std::uint8_t(v<0?128+v*128/32768:128+v*127/32767);};
        p.left_x=axis(SDL_CONTROLLER_AXIS_LEFTX);p.left_y=axis(SDL_CONTROLLER_AXIS_LEFTY);
        p.right_x=axis(SDL_CONTROLLER_AXIS_RIGHTX);p.right_y=axis(SDL_CONTROLLER_AXIS_RIGHTY);
        p.l2=std::uint8_t(std::max(0,int(SDL_GameControllerGetAxis(controller,SDL_CONTROLLER_AXIS_TRIGGERLEFT)))*255/32767);
        p.r2=std::uint8_t(std::max(0,int(SDL_GameControllerGetAxis(controller,SDL_CONTROLLER_AXIS_TRIGGERRIGHT)))*255/32767);
        const struct {SDL_GameControllerButton key;u64 bit;} bindings[]={
            {SDL_CONTROLLER_BUTTON_A,Cross},{SDL_CONTROLLER_BUTTON_B,Circle},{SDL_CONTROLLER_BUTTON_X,Square},{SDL_CONTROLLER_BUTTON_Y,Triangle},
            {SDL_CONTROLLER_BUTTON_START,Options},{SDL_CONTROLLER_BUTTON_BACK,Share},{SDL_CONTROLLER_BUTTON_LEFTSHOULDER,L1},{SDL_CONTROLLER_BUTTON_RIGHTSHOULDER,R1},
            {SDL_CONTROLLER_BUTTON_DPAD_LEFT,Left},{SDL_CONTROLLER_BUTTON_DPAD_RIGHT,Right},{SDL_CONTROLLER_BUTTON_DPAD_UP,Up},{SDL_CONTROLLER_BUTTON_DPAD_DOWN,Down}};
        for(auto b:bindings)if(SDL_GameControllerGetButton(controller,b.key))p.buttons|=b.bit;
    }
    const auto* keys=SDL_GetKeyboardState(nullptr);
    const struct {SDL_Scancode key;u64 bit;} bindings[]={
        {SDL_SCANCODE_LEFT,Left},{SDL_SCANCODE_RIGHT,Right},{SDL_SCANCODE_UP,Up},{SDL_SCANCODE_DOWN,Down},
        {SDL_SCANCODE_SPACE,Cross},{SDL_SCANCODE_RETURN,Options},{SDL_SCANCODE_BACKSPACE,Circle},{SDL_SCANCODE_Q,L1},{SDL_SCANCODE_E,R1}};
    for(auto b:bindings)if(keys[b.key])p.buttons|=b.bit;
    if(keys[SDL_SCANCODE_LEFT])p.left_x=0;
    if(keys[SDL_SCANCODE_RIGHT])p.left_x=255;
    if(keys[SDL_SCANCODE_W])p.r2=255;if(keys[SDL_SCANCODE_S])p.l2=255;
    if(p.l2>127)p.buttons|=L2;if(p.r2>127)p.buttons|=R2;
    if(keys[SDL_SCANCODE_ESCAPE])running=false;
}
}
bool application_initialize(std::string& error){
    SDL_SetMainReady();
    if(SDL_Init(SDL_INIT_VIDEO|SDL_INIT_AUDIO|SDL_INIT_GAMECONTROLLER|SDL_INIT_EVENTS)<0){error=SDL_GetError();return false;}
    initialized=true;running=true;return true;
}
void application_shutdown(){if(controller)SDL_GameControllerClose(controller);controller=nullptr;if(initialized)SDL_Quit();initialized=false;running=false;}
bool application_running(){return running;}
void platform_sleep_ns(s64 ns){if(ns>0)std::this_thread::sleep_for(std::chrono::nanoseconds(ns));else std::this_thread::yield();}
std::string application_home(int argc,char** argv){
    std::string override;
    if(const char* value=std::getenv("OR2_GAME_ROOT"))retail=value;
    for(int i=1;i<argc;++i){const std::string a=argv[i];if(a.rfind("--data=",0)==0)retail=a.substr(7);if(a.rfind("--home=",0)==0)override=a.substr(7);}
    if(override.empty()){
        outrun::mac::Paths paths;std::string error;
        if(!outrun::mac::user_paths(paths,error))throw std::runtime_error(error);
        // The EXE image goes next to the game, as on every platform; this
        // folder only takes it when the game folder is read-only.
        if(retail.empty())throw std::runtime_error("game folder not set");
        cache=paths.saves;
        return paths.saves;
    }
    std::error_code ec;const auto saves=override+"/SaveGame";cache=override;
    std::filesystem::create_directories(saves,ec);if(ec)throw std::runtime_error(ec.message());
    return saves;
}
std::string application_retail_root(){return retail;}
std::string application_cache(){return cache;}
void consoleInit(void*){std::setvbuf(stdout,nullptr,_IOLBF,0);}void consoleUpdate(void*){std::fflush(stdout);}void consoleExit(void*){std::fflush(stdout);}
void padConfigureInput(int,u64){}void padInitializeDefault(PadState* p){*p=PadState{};}
void padUpdate(PadState* p){SDL_Event event;while(SDL_PollEvent(&event))if(event.type==SDL_QUIT)running=false;read_pad(p->sample);const u64 held=p->sample.buttons;p->down=held&~p->previous;p->up=p->previous&~held;p->previous=held;}
bool padIsConnected(const PadState* p){return p->sample.connected;}
u64 padGetButtons(const PadState* p){return p->sample.buttons;}u64 padGetButtonsDown(const PadState* p){return p->down;}u64 padGetButtonsUp(const PadState* p){return p->up;}
HidAnalogStickState padGetStickPos(const PadState* p,unsigned stick){const int x=stick?p->sample.right_x:p->sample.left_x,y=stick?p->sample.right_y:p->sample.left_y;return {(x-128)*256,-(y-128)*256};}
