#pragma once
#include <cstdint>
#include <deque>
#include <vector>
#include <stdexcept>
using u32=std::uint32_t;using u64=std::uint64_t;using Result=u32;
#define R_SUCCEEDED(x) ((x)==0u)
enum PcmFormat{PcmFormat_Int16=2};
struct AudioOutBuffer{AudioOutBuffer* next{};void* buffer{};u64 buffer_size{},data_size{},data_offset{};};
namespace audio_test {
inline bool initialized{},started{},release=true;inline unsigned closes{},starts{};
inline Result fail_append{};inline std::deque<AudioOutBuffer*> busy;
inline std::vector<std::int16_t> played;
}
inline Result audoutInitialize(){audio_test::initialized=true;return 0;}
inline void audoutExit(){audio_test::initialized=false;++audio_test::closes;}
inline unsigned audoutGetSampleRate(){return 48000;}
inline unsigned audoutGetChannelCount(){return 2;}
inline PcmFormat audoutGetPcmFormat(){return PcmFormat_Int16;}
inline Result audoutStartAudioOut(){audio_test::started=true;++audio_test::starts;return 0;}
inline Result audoutStopAudioOut(){audio_test::started=false;return 0;}
inline Result audoutFlushAudioOutBuffers(bool* flushed){audio_test::busy.clear();*flushed=true;return 0;}
inline void armDCacheFlush(void*,std::size_t){}
inline Result audoutAppendAudioOutBuffer(AudioOutBuffer* b){
    if(audio_test::fail_append)return audio_test::fail_append;
    if(!audio_test::initialized||!audio_test::started||reinterpret_cast<std::uintptr_t>(b->buffer)%4096u||
       b->buffer_size%4096u||b->data_size>b->buffer_size||b->data_size%4u)throw std::runtime_error("invalid device buffer");
    for(auto* existing:audio_test::busy)if(existing==b)throw std::runtime_error("DMA buffer reused before release");
    const auto* s=static_cast<const std::int16_t*>(b->buffer);
    audio_test::played.insert(audio_test::played.end(),s,s+b->data_size/2u);audio_test::busy.push_back(b);return 0;
}
inline Result audoutGetReleasedAudioOutBuffer(AudioOutBuffer** b,u32* count){
    *b=nullptr;*count=0;
    if(audio_test::release&&!audio_test::busy.empty()){*b=audio_test::busy.front();audio_test::busy.pop_front();*count=1;}
    return 0;
}
