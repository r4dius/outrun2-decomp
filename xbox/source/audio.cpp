#include "audio.hpp"
#include <xaudio2.h>
#include <wrl/client.h>
#include <deque>
#include <mutex>

namespace outrun::xbox_runtime {
namespace {
// XAudio2 plays the submitted buffers in order; a buffer's memory stays
// owned here until the voice reports it consumed.
struct Output {
    Microsoft::WRL::ComPtr<IXAudio2> engine;
    IXAudio2MasteringVoice* master{};
    IXAudio2SourceVoice* voice{};
    std::deque<std::vector<std::int16_t>> buffers;
    std::uint64_t submitted{};
    void reclaim(){XAUDIO2_VOICE_STATE s{};voice->GetState(&s,XAUDIO2_VOICE_NOSAMPLESPLAYED);while(buffers.size()>s.BuffersQueued)buffers.pop_front();}
};
}
XboxAudio::~XboxAudio(){close();}
void XboxAudio::close(){
    auto* o=static_cast<Output*>(impl_);
    if(o){if(o->voice){o->voice->Stop();o->voice->FlushSourceBuffers();o->voice->DestroyVoice();}
        if(o->master)o->master->DestroyVoice();delete o;}
    impl_=nullptr;
}
bool XboxAudio::open(std::string& error){
    close();error.clear();submitted_=0;
    auto* o=new Output;
    if(FAILED(XAudio2Create(&o->engine,0,XAUDIO2_DEFAULT_PROCESSOR))){error="XAudio2Create failed";delete o;return false;}
    if(FAILED(o->engine->CreateMasteringVoice(&o->master,2,48000))){error="XAudio2 mastering voice failed";delete o;return false;}
    WAVEFORMATEX f{};f.wFormatTag=WAVE_FORMAT_PCM;f.nChannels=2;f.nSamplesPerSec=48000;f.wBitsPerSample=16;
    f.nBlockAlign=4;f.nAvgBytesPerSec=48000*4;
    if(FAILED(o->engine->CreateSourceVoice(&o->voice,&f))){error="XAudio2 source voice failed";o->master->DestroyVoice();delete o;return false;}
    o->voice->Start();impl_=o;return true;
}
bool XboxAudio::submit(const std::vector<std::int16_t>& pcm,std::string& error){
    auto* o=static_cast<Output*>(impl_);if(!o){error="Xbox audio output not open";return false;}
    if(pcm.size()%2){error="odd stereo sample count";return false;}
    if(pcm.empty())return true;
    constexpr std::size_t MaxQueuedFrames=96000;   // 2 s, as the PS5 SDL queue cap
    o->reclaim();
    if(queued_frames()+pcm.size()/2>MaxQueuedFrames){error="Xbox audio queue stalled";return false;}
    o->buffers.push_back(pcm);const auto& b=o->buffers.back();
    XAUDIO2_BUFFER buffer{};buffer.AudioBytes=UINT32(b.size()*2);buffer.pAudioData=reinterpret_cast<const BYTE*>(b.data());
    if(FAILED(o->voice->SubmitSourceBuffer(&buffer))){o->buffers.pop_back();error="XAudio2 SubmitSourceBuffer failed";return false;}
    o->submitted+=pcm.size()/2;submitted_+=pcm.size()/2;return true;
}
std::size_t XboxAudio::queued_frames()const{
    const auto* o=static_cast<const Output*>(impl_);if(!o)return 0;
    XAUDIO2_VOICE_STATE s{};o->voice->GetState(&s);
    return o->submitted>s.SamplesPlayed?std::size_t(o->submitted-s.SamplesPlayed):0;
}
}
