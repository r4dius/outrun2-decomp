#include "switch_audio.hpp"
#include <cstdio>
#include <cstdlib>
#if (defined(__SWITCH__) && !defined(OUTRUN_SWITCH_STUB)) || defined(OR2_AUDIO_DEVICE_TEST)
#include <switch.h>
#include <malloc.h>
#include <cstring>
#include <algorithm>
#include <array>
#include <cstdio>
namespace outrun::switch_runtime {
namespace {
struct Output {
    static constexpr std::size_t Bytes=8192,Count=8;
    std::array<AudioOutBuffer,Count> buffers{};
    std::array<bool,Count> busy{};
    std::vector<std::int16_t> pending;
    bool initialized{};
    ~Output(){
        if(initialized){audoutStopAudioOut();bool flushed{};audoutFlushAudioOutBuffers(&flushed);audoutExit();}
        // Close the service before freeing potentially in-flight buffers.
        for(auto& b:buffers)std::free(b.buffer);
    }
};
bool check(Result rc,const char* op,std::string& error){
    if(R_SUCCEEDED(rc))return true;char text[160];
    std::snprintf(text,sizeof(text),"%s failed: %08x",op,unsigned(rc));error=text;return false;
}
}
SwitchAudio::~SwitchAudio(){close();}
void SwitchAudio::close(){delete static_cast<Output*>(impl_);impl_=nullptr;}
bool SwitchAudio::open(std::string& error){
    close();error.clear();submitted_=0u;auto* output=new Output;impl_=output;
    if(!check(audoutInitialize(),"audoutInitialize",error)){close();return false;}
    output->initialized=true;
    if(audoutGetSampleRate()!=48000u||audoutGetChannelCount()!=2u||audoutGetPcmFormat()!=PcmFormat_Int16){
        error="audio device did not provide 48 kHz stereo S16";close();return false;
    }
    for(auto& b:output->buffers){b.buffer=memalign(0x1000u,Output::Bytes);b.buffer_size=Output::Bytes;
        if(!b.buffer){error="audio buffer allocation failed";close();return false;}}
    if(!check(audoutStartAudioOut(),"audoutStartAudioOut",error)){close();return false;}
    return true;
}
bool SwitchAudio::submit(const std::vector<std::int16_t>& pcm,std::string& error){
    auto* o=static_cast<Output*>(impl_);if(!o){error="audio output not open";return false;}
    // libnx exposes one returned tag per IPC call, not a linked list.
    for(std::size_t j=0;j<Output::Count;++j){
        AudioOutBuffer* released{};u32 count{};
        if(!check(audoutGetReleasedAudioOutBuffer(&released,&count),"audio buffer release",error))return false;
        if(!count)break;
        bool known=false;
        for(std::size_t i=0;i<Output::Count;++i)if(released==&o->buffers[i]){o->busy[i]=false;known=true;}
        if(!known){error="audio service returned an unknown buffer";return false;}
    }
    if(pcm.size()%2u||o->pending.size()+pcm.size()>192000u){error="audio output queue stalled or odd stereo sample count";return false;}
    o->pending.insert(o->pending.end(),pcm.begin(),pcm.end());
    std::size_t consumed{};
    for(std::size_t i=0;i<Output::Count&&consumed<o->pending.size();++i){
        if(o->busy[i])continue;
        auto& b=o->buffers[i];const auto samples=std::min(Output::Bytes/2u,o->pending.size()-consumed);
        std::memcpy(b.buffer,o->pending.data()+consumed,samples*2u);b.data_size=samples*2u;b.next=nullptr;
        armDCacheFlush(b.buffer,b.data_size);
        if(!check(audoutAppendAudioOutBuffer(&b),"audio buffer submit",error))return false;
        o->busy[i]=true;consumed+=samples;submitted_+=samples/2u;
    }
    o->pending.erase(o->pending.begin(),o->pending.begin()+consumed);return true;
}
std::size_t SwitchAudio::queued_frames()const{
    const auto* o=static_cast<const Output*>(impl_);if(!o)return 0u;
    std::size_t samples=o->pending.size();
    for(std::size_t i=0;i<Output::Count;++i)if(o->busy[i])samples+=o->buffers[i].data_size/2u;
    return samples/2u;
}
}
#else
namespace outrun::switch_runtime {
SwitchAudio::~SwitchAudio()=default;
void SwitchAudio::close(){impl_=nullptr;}
std::size_t SwitchAudio::queued_frames()const{return 0u;}
#if defined(OR2_HOST_NRO)
// tools host_nro: the PCM stream is produced and discarded (no device), or
// appended as raw s16le 48 kHz stereo to $OR2_HOST_PCM for listening checks.
bool SwitchAudio::open(std::string&){return true;}
bool SwitchAudio::submit(const std::vector<std::int16_t>& pcm,std::string&){
    static std::FILE* dump=[]{const char* p=std::getenv("OR2_HOST_PCM");return p&&*p?std::fopen(p,"wb"):nullptr;}();
    if(dump&&!pcm.empty()){std::fwrite(pcm.data(),2,pcm.size(),dump);std::fflush(dump);}
    return true;}
#else
bool SwitchAudio::open(std::string& error){error="native audio unavailable in host stub";return false;}
bool SwitchAudio::submit(const std::vector<std::int16_t>&,std::string& error){error="native audio unavailable in host stub";return false;}
#endif
}
#endif
