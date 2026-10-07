// macOS adaptation of ps5/source/audio.cpp; reference baseline remains unchanged.
#define SDL_MAIN_HANDLED
#include <SDL.h>
#include "audio.hpp"

namespace outrun::mac_runtime {
namespace {struct Output {SDL_AudioDeviceID device{};};}
MacAudio::~MacAudio(){close();}
void MacAudio::close(){
    auto* o=static_cast<Output*>(impl_);if(o){SDL_ClearQueuedAudio(o->device);SDL_CloseAudioDevice(o->device);delete o;}
    impl_=nullptr;
}
bool MacAudio::open(std::string& error){
    close();error.clear();submitted_=0;
    SDL_AudioSpec wanted{},actual{};wanted.freq=48000;wanted.format=AUDIO_S16SYS;wanted.channels=2;wanted.samples=1024;
    auto* o=new Output;o->device=SDL_OpenAudioDevice(nullptr,0,&wanted,&actual,0);
    if(!o->device){error=SDL_GetError();delete o;return false;}
    if(actual.freq!=48000||actual.format!=AUDIO_S16SYS||actual.channels!=2){
        error="macOS audio requires 48 kHz stereo S16";SDL_CloseAudioDevice(o->device);delete o;return false;}
    impl_=o;SDL_PauseAudioDevice(o->device,0);return true;
}
bool MacAudio::submit(const std::vector<std::int16_t>& pcm,std::string& error){
    auto* o=static_cast<Output*>(impl_);if(!o){error="macOS audio output not open";return false;}
    if(pcm.size()%2){error="odd stereo sample count";return false;}
    constexpr std::size_t MaxQueuedBytes=384000;
    if(pcm.size()>MaxQueuedBytes/2||SDL_GetQueuedAudioSize(o->device)+pcm.size()*2>MaxQueuedBytes){error="macOS audio queue stalled";return false;}
    if(!pcm.empty()&&SDL_QueueAudio(o->device,pcm.data(),Uint32(pcm.size()*2))<0){error=SDL_GetError();return false;}
    submitted_+=pcm.size()/2;return true;
}
std::size_t MacAudio::queued_frames()const{const auto* o=static_cast<const Output*>(impl_);return o?SDL_GetQueuedAudioSize(o->device)/4:0;}
}
