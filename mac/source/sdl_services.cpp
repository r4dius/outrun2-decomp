#define SDL_MAIN_HANDLED
#include <SDL.h>
#include "sdl_services.hpp"
#include <algorithm>
namespace outrun::mac {
struct SdlServices::Impl {
    SDL_AudioDeviceID audio{};
    SDL_GameController* controller{};
    SDL_Window* window{};
    ps5::DualSenseInput input;
    bool initialized{},quit{};
    ~Impl(){
        if(audio){SDL_ClearQueuedAudio(audio);SDL_CloseAudioDevice(audio);}
        if(controller)SDL_GameControllerClose(controller);
        if(window)SDL_DestroyWindow(window);
        if(initialized)SDL_QuitSubSystem(SDL_INIT_VIDEO|SDL_INIT_AUDIO|SDL_INIT_GAMECONTROLLER|SDL_INIT_EVENTS);
    }
};
SdlServices::SdlServices():impl_(std::make_unique<Impl>()){}
SdlServices::~SdlServices()=default;
bool SdlServices::open(std::string& error,void* native_window){
    auto& p=*impl_;
    if(p.initialized){error="SDL services already open";return false;}
    SDL_SetMainReady();
    if(SDL_InitSubSystem(SDL_INIT_VIDEO|SDL_INIT_AUDIO|SDL_INIT_GAMECONTROLLER|SDL_INIT_EVENTS)<0){error=SDL_GetError();return false;}
    p.initialized=true;
    if(native_window){
        p.window=SDL_CreateWindowFrom(native_window);
        if(!p.window){error=SDL_GetError();return false;}
    }
    SDL_AudioSpec requested{},actual{};requested.freq=48000;requested.format=AUDIO_S16SYS;requested.channels=2;requested.samples=1024;
    p.audio=SDL_OpenAudioDevice(nullptr,0,&requested,&actual,0);
    if(!p.audio){error=SDL_GetError();return false;}
    if(actual.freq!=48000||actual.format!=AUDIO_S16SYS||actual.channels!=2){error="48 kHz stereo S16 required";SDL_CloseAudioDevice(p.audio);p.audio=0;return false;}
    SDL_PauseAudioDevice(p.audio,0);return true;
}
platform::NativeInputState SdlServices::sample(std::uint32_t mode){
    auto& p=*impl_;
    if(!p.initialized)return {};
    SDL_Event event;while(SDL_PollEvent(&event))if(event.type==SDL_QUIT)p.quit=true;
    if(p.controller&&!SDL_GameControllerGetAttached(p.controller)){SDL_GameControllerClose(p.controller);p.controller=nullptr;}
    if(!p.controller)for(int i=0;i<SDL_NumJoysticks();++i)if(SDL_IsGameController(i)){p.controller=SDL_GameControllerOpen(i);if(p.controller)break;}
    ps5::PadSample pad{};pad.connected=true;pad.left_x=pad.left_y=pad.right_x=pad.right_y=128;
    const auto* keys=SDL_GetKeyboardState(nullptr);
    const struct {SDL_Scancode key;std::uint64_t button;} bindings[]={
        {SDL_SCANCODE_LEFT,ps5::Left},{SDL_SCANCODE_RIGHT,ps5::Right},{SDL_SCANCODE_UP,ps5::Up},{SDL_SCANCODE_DOWN,ps5::Down},
        {SDL_SCANCODE_SPACE,ps5::Cross},{SDL_SCANCODE_BACKSPACE,ps5::Circle},{SDL_SCANCODE_RETURN,ps5::Options},
        {SDL_SCANCODE_Q,ps5::L1},{SDL_SCANCODE_E,ps5::R1}};
    for(const auto& b:bindings)if(keys[b.key])pad.buttons|=b.button;
    if(p.controller){
        const struct {SDL_GameControllerButton key;std::uint64_t button;} buttons[]={
            {SDL_CONTROLLER_BUTTON_A,ps5::Cross},{SDL_CONTROLLER_BUTTON_B,ps5::Circle},
            {SDL_CONTROLLER_BUTTON_X,ps5::Square},{SDL_CONTROLLER_BUTTON_Y,ps5::Triangle},
            {SDL_CONTROLLER_BUTTON_START,ps5::Options},{SDL_CONTROLLER_BUTTON_BACK,ps5::Share},
            {SDL_CONTROLLER_BUTTON_LEFTSHOULDER,ps5::L1},{SDL_CONTROLLER_BUTTON_RIGHTSHOULDER,ps5::R1},
            {SDL_CONTROLLER_BUTTON_DPAD_UP,ps5::Up},{SDL_CONTROLLER_BUTTON_DPAD_DOWN,ps5::Down},
            {SDL_CONTROLLER_BUTTON_DPAD_LEFT,ps5::Left},{SDL_CONTROLLER_BUTTON_DPAD_RIGHT,ps5::Right}};
        for(const auto& b:buttons)if(SDL_GameControllerGetButton(p.controller,b.key))pad.buttons|=b.button;
        auto axis=[&](SDL_GameControllerAxis a){return std::uint8_t((std::int32_t(SDL_GameControllerGetAxis(p.controller,a))+32768)/256);};
        pad.left_x=axis(SDL_CONTROLLER_AXIS_LEFTX);pad.left_y=axis(SDL_CONTROLLER_AXIS_LEFTY);
        pad.right_x=axis(SDL_CONTROLLER_AXIS_RIGHTX);pad.right_y=axis(SDL_CONTROLLER_AXIS_RIGHTY);
        pad.l2=std::uint8_t(std::max(0,int(SDL_GameControllerGetAxis(p.controller,SDL_CONTROLLER_AXIS_TRIGGERLEFT)))*255/32767);
        pad.r2=std::uint8_t(std::max(0,int(SDL_GameControllerGetAxis(p.controller,SDL_CONTROLLER_AXIS_TRIGGERRIGHT)))*255/32767);
    }
    if(keys[SDL_SCANCODE_W])pad.r2=255;
    if(keys[SDL_SCANCODE_S])pad.l2=255;
    auto result=p.input.update(pad,mode);result.exit_requested|=p.quit||keys[SDL_SCANCODE_ESCAPE];return result;
}
bool SdlServices::submit(const std::vector<std::int16_t>& pcm,std::string& error){
    auto& p=*impl_;
    if(!p.audio){error="Audio device not open";return false;}
    constexpr std::size_t limit=384000;
    if(pcm.size()%2||pcm.size()>limit/2||SDL_GetQueuedAudioSize(p.audio)+pcm.size()*2>limit){error="Invalid stereo PCM or audio queue full";return false;}
    if(!pcm.empty()&&SDL_QueueAudio(p.audio,pcm.data(),Uint32(pcm.size()*2))<0){error=SDL_GetError();return false;}
    return true;
}
}
