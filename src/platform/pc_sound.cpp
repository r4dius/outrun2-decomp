#include "system/exe_image.hpp"
#include <cstdio>
#include <cstdlib>
#include <stdexcept>
#include "platform/pc_sound.hpp"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstring>
#include <sstream>

namespace outrun::platform {
namespace {
#include "platform/pc_sound_tables.inc"
constexpr std::size_t IcsDefinitionCount=sizeof(kIcsDefinitions)/sizeof(kIcsDefinitions[0]);
constexpr std::int32_t ErrNoBuffer=std::int32_t(0x800401f0u);          // 412770: buffer +4 null
// x87 constants (EXE .rdata).
constexpr double K62817c=0.0078125,K6281ac=double(1.0f/127.0f),K6281a8=2000.0,K6281a4=-300.0,K628180=-10000.0,
    K628194=double(1.0f/3.0f),K628190=341.0,K62818c=1.0/4096.0,K6281a0=0.015625,K6280a0=10000.0;
std::int16_t s16(const std::uint8_t* p){std::int16_t v;std::memcpy(&v,p,2);return v;}
std::uint32_t u32(const std::uint8_t* p){std::uint32_t v;std::memcpy(&v,p,4);return v;}
std::int32_t ftol(double v){return std::int32_t(v);}                     // 582194: truncation
double log10_or_inf(double v){return std::log10(v);}                     // fyl2x: log10(0) = -inf
// 42FB30: pitch curve (8-byte points {x, a, b}: ((a<<6)+b)<<4), 7 segments.
std::int32_t curve_pitch_42fb30(std::int32_t x,const std::uint8_t* p){
    for(int i=0;i<7;++i){
        const std::int32_t x0=s16(p+i*8),x1=s16(p+i*8+8);
        if(x<=x0||x>x1)continue;
        const std::int32_t v0=((std::int32_t(s16(p+i*8+2))<<6)+s16(p+i*8+4))<<4;
        const std::int32_t v1=((std::int32_t(s16(p+i*8+10))<<6)+s16(p+i*8+12))<<4;
        return std::int32_t(std::int64_t(v1-v0)*(x-x0)/(x1-x0))+v0;
    }
    return 0;
}
// 42FA30 curves A (volume, +4C) / B (pan, +6C): 4-byte points {x, y}.
std::int32_t curve_42fa30(std::int32_t x,const std::uint8_t* p){
    for(int i=0;i<7;++i){
        const std::int32_t x0=s16(p+i*4),x1=s16(p+i*4+4);
        if(x<=x0||x>x1)continue;
        const std::int32_t y0=s16(p+i*4+2),y1=s16(p+i*4+6);
        return std::int32_t(std::int64_t(y1-y0)*(x-x0)/(x1-x0))+y0;
    }
    return 0;
}
const std::uint8_t* layer_bytes(const PcIcsDefinition& d,std::uint32_t l){return kIcsLayers[d.first_layer+l];}
}

PcSound::PcSound(){
    index_.fill(0xffu);slot_.fill(0xffu);slot_bank_.fill(0xffu);
    // 42EBF0 audio init: layers {0, 0, 0x40, .., voice -1}, channels 0 with id -1.
    for(auto& c:channels_){c=Channel{};}
    fade_in_.reserve(32);fade_out_.reserve(32);
}
void PcSound::boot_424600(){slot_bank_.fill(0xffu);master_957be4=0x41;}
double PcSound::now_ms()const{
    static const auto origin=std::chrono::steady_clock::now();
    return std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-origin).count();
}
void PcSound::error(const std::string& e){if(errors.size()<32)errors.push_back(e);}

// ---- banks --------------------------------------------------------------
std::uint32_t PcSound::request_427700(std::uint32_t arg){
    if(!ready)return 0u;
    ++stats.requests;
    const std::uint32_t bank=arg&0xffu,slot=(arg>>8)&0x3fu;
    if(slot>=Slots||bank>=sizeof(kSoundBanks)/sizeof(kSoundBanks[0])){error("427700: request "+std::to_string(arg)+" outside the 12 slots / bank table");return 0u;}
    if(slot_bank_[slot]==bank)return 0u;
    if(request_state_956110==0u){
        if(slot_bank_[slot]!=0xffu)unload_4278d0(slot);
        request_state_956110=1u;return 1u;
    }
    if(request_state_956110!=1u)return 0u;
    std::string name=kSoundBanks[bank].name;
    if((arg&0x8000u)&&!(arg&0x4000u)){
        static constexpr const char* Prefix754a88[5][6]{
            {"ENGLISHC","FRENCH","GERMAN","ITALIAN","SPANISH","ENGLISHC"},{"ENGLISHH","FRENCH","GERMAN","ITALIAN","SPANISH","ENGLISHH"},
            {"ENGLISHJ","FRENCH","GERMAN","ITALIAN","SPANISH","ENGLISHJ"},{"SPHAM","FRENCH","GERMAN","ITALIAN","SPANISH","SPHAM"},
            {"SPRACE","FRENCH","GERMAN","ITALIAN","SPANISH","SPRACE"}};
        if(voice_set_754b0c<5u&&language_95b210<6u)name=std::string(Prefix754a88[voice_set_754b0c][language_95b210])+"_"+name;   // "%s_%s"
        else error("427700: voice set "+std::to_string(voice_set_754b0c)+" / language "+std::to_string(language_95b210)+" outside 754A88");
    }
    if(load_42f620(slot,name)==0)return 1u;
    slot_bank_[slot]=std::uint16_t(bank);
    const auto& b=kSoundBanks[bank];
    for(std::uint32_t k=0;k<b.count;++k){
        const std::uint32_t id=kSoundBankIds[b.first+k];
        if(id>=Ids){error("427700: bank id outside the sound table");continue;}
        slot_[id]=std::uint16_t(slot);index_[id]=std::uint16_t(k);
    }
    request_state_956110=0u;return 0u;
}
// 42F620: 0 = not finished (open, then read + 42F4C0), 1 = loaded. A file
// the retail tree does not have is reported and leaves the slot empty
// (the PC would wait on it forever).
std::int32_t PcSound::load_42f620(std::uint32_t slot,const std::string& name){
    if(!ready)return 1;
    auto& st=slot_state_[slot];
    if(st==0u){st=1u;return 0;}
    if(st==1u){
        std::vector<std::uint8_t> bytes;std::string e;
        if(!read_file||!read_file(name,bytes,e))error("sound bank "+name+": "+(e.empty()?"no reader":e));
        else{parse_42f4c0(bytes,slot);++stats.loads;stats.load_bytes+=bytes.size();}
        st=6u;return 0;
    }
    return 1;
}
// 42F4C0: {id, size, 16-bit mono PCM} until id -1; slot 5 creates the ICS
// buffers (7790F8, voices 774170), the others 7763F0 (voices 773868; id 0x240 skipped).
void PcSound::parse_42f4c0(const std::vector<std::uint8_t>& pak,std::uint32_t slot){
    std::size_t o=0;
    while(o+8u<=pak.size()){
        const std::uint32_t id=u32(pak.data()+o),size=u32(pak.data()+o+4);o+=8;
        if(id==0xffffffffu)return;
        if(size>pak.size()-o){error("sound bank: entry beyond the file");return;}
        if(id>=Ids){error("sound bank: id "+std::to_string(id)+" outside the sound table");o+=size;continue;}
        if(slot!=5u&&id==0x240u){o+=size;continue;}
        auto pcm=std::make_shared<std::vector<std::int16_t>>(size/2u);
        std::memcpy(pcm->data(),pak.data()+o,(size/2u)*2u);
        const auto& info=kSoundIds[id];
        auto& buffer=slot==5u?ics_[id]:se_[id];
        if(buffer)++stats.replaced;
        // A replaced buffer drops out of the fade lists (its voices stop).
        auto drop=[&](std::vector<FadeRef>& l){l.erase(std::remove_if(l.begin(),l.end(),[&](const FadeRef& f){return f.buffer==&buffer;}),l.end());};
        drop(fade_in_);drop(fade_out_);
        buffer=Buffer{};buffer.pcm=pcm;buffer.rate=info.rate;
        const std::uint32_t voices=slot==5u?info.voices_ics:info.voices;
        buffer.voices.assign(voices?voices:1u,Voice{});
        for(auto& v:buffer.voices)v.freq=buffer.rate;
        o+=size;
    }
}
void PcSound::stop_slot_42f160(std::uint32_t slot){
    if(!ready)return;
    for(std::uint32_t id=0;id<Ids;++id)if(slot_[id]==slot&&se_[id])stop_412880(se_[id],-1);
}
void PcSound::release_42f290(std::uint32_t slot){
    if(!ready)return;
    for(std::uint32_t id=0;id<Ids;++id){
        if(slot_[id]!=slot)continue;
        if(se_[id]){
            auto drop=[&](std::vector<FadeRef>& l){l.erase(std::remove_if(l.begin(),l.end(),[&](const FadeRef& f){return f.buffer==&se_[id];}),l.end());};
            drop(fade_in_);drop(fade_out_);se_[id]=Buffer{};
        }
        slot_[id]=0xffu;index_[id]=0xffu;
    }
    slot_state_[slot]=0u;
}
void PcSound::unload_4278d0(std::uint32_t slot){
    if(!ready)return;
    slot&=0x7fu;
    if(slot>=Slots){error("4278D0: slot outside the 12 slots");return;}
    ++stats.unloads;
    stop_slot_42f160(slot);release_42f290(slot);
    slot_bank_[slot]=0xffu;
    for(std::uint32_t id=0;id<Ids;++id)if(slot_[id]==slot){slot_[id]=0xffu;index_[id]=0;}
}
void PcSound::unload_all_4276b0(){
    for(std::uint32_t s=0;s<Slots;++s){
        if(s==5u||s==0xau||s==0xbu)continue;
        if(slot_bank_[s]!=0xffu)unload_4278d0(s);
    }
    request_state_956110=0u;
}
void PcSound::voice_set_427db0(std::uint32_t language,bool heart_attack,std::uint32_t variant,std::uint32_t profile_208){
    language_95b210=language;
    if(!heart_attack){voice_set_754b0c=variant!=2u?4u:3u;return;}
    if(variant==2u){voice_set_754b0c=0u;return;}
    if(variant!=5u){voice_set_754b0c=2u;return;}
    const std::uint32_t k=(profile_208>>8)&7u;
    voice_set_754b0c=k==4u?0u:k==5u?2u:1u;
}

// ---- voices (DirectSound layer) ------------------------------------------
std::int32_t PcSound::play_412770(Buffer& b,bool loop,std::int32_t volume,std::int32_t freq,std::int32_t pan,float arg5,float arg6){
    if(!b)return ErrNoBuffer;
    std::size_t i=0;
    while(i<b.voices.size()&&b.voices[i].playing)++i;                      // 412700: first stopped voice
    if(i==b.voices.size()){++stats.saturated;return -1;}
    auto& v=b.voices[i];
    v.fade_in_ms=arg5;v.fade_out_ms=arg6;v.fade_from=volume;
    if(arg6>0.0f){                                                          // 4122E0: fade list, then muted start
        if(fade_in_.size()<32u){v.fade_start=now_ms();fade_in_.push_back({&b,i});}
        volume=-10000;
    }
    if(volume>=-10000&&volume<=0)v.volume=volume;                           // SetVolume (DSBVOLUME_MIN..MAX)
    if(freq!=-1){if(freq==0)v.freq=b.rate;else if(freq>=100&&freq<=200000)v.freq=std::uint32_t(freq);}
    if(pan>=-10000&&pan<=10000)v.pan=pan;
    v.loop=loop;v.playing=true;
    return std::int32_t(i);
}
void PcSound::stop_412880(Buffer& b,std::int32_t voice){
    if(!b)return;
    auto stop=[](Voice& v){v.playing=false;v.pos=0;};
    if(voice>=0){if(std::size_t(voice)<b.voices.size()&&b.voices[voice].playing)stop(b.voices[voice]);return;}
    for(auto& v:b.voices)if(v.playing)stop(v);
}
void PcSound::stop_4129c0(Buffer& b,std::int32_t voice){
    if(!b)return;
    auto one=[&](std::size_t i){
        auto& v=b.voices[i];
        if(v.fade_out_ms>0.0f){                                             // 412340: fade-out list
            if(fade_out_.size()<32u){v.fade_start=now_ms();v.fade_from=v.volume;fade_out_.push_back({&b,i});}
            return;
        }
        v.playing=false;v.pos=0;
    };
    if(voice>=0){if(std::size_t(voice)<b.voices.size())one(std::size_t(voice));return;}
    for(std::size_t i=0;i<b.voices.size();++i)if(b.voices[i].playing)one(i);
}
void PcSound::fades_412120(){
    const double now=now_ms();
    for(std::size_t k=0;k<fade_in_.size();){
        auto& v=fade_in_[k].buffer->voices[fade_in_[k].voice];
        const double elapsed=now-v.fade_start;
        if(!(elapsed<=double(v.fade_in_ms))){fade_in_[k]=fade_in_.back();fade_in_.pop_back();continue;}
        const double frac=(double(v.fade_in_ms)-elapsed)/double(v.fade_in_ms);
        if(std::isfinite(frac)){const auto vol=ftol(double(v.volume+10000)*frac-K6280a0);if(vol>=-10000&&vol<=0)v.volume=vol;}
        ++stats.fades;++k;
    }
    for(std::size_t k=0;k<fade_out_.size();){
        auto& v=fade_out_[k].buffer->voices[fade_out_[k].voice];
        const double elapsed=now-v.fade_start;
        if(!(elapsed<=double(v.fade_out_ms))){v.playing=false;v.pos=0;fade_out_[k]=fade_out_.back();fade_out_.pop_back();continue;}
        const auto vol=ftol(elapsed/double(v.fade_out_ms)*double(-10000-v.fade_from)+double(v.fade_from));
        if(vol>=-10000&&vol<=0)v.volume=vol;
        ++stats.fades;++k;
    }
}

// ---- effects -------------------------------------------------------------
void PcSound::effect_42f0d0(std::uint32_t command){
    if(!ready)return;
    ++stats.effects;
    const std::uint32_t id=command&0x7ffu;
    if(id==0x4a9u)return;                                                   // the "no sound" id
    if(command&0x8000u){
        if(id<Ids&&se_[id]){stop_4129c0(se_[id],-1);++stats.effect_stops;}
        return;
    }
    if(command&0x4000u){play_42f1a0((command&0xfffu)|0x1000u);play_42f1a0(((command+1u)&0xfffu)|0x2000u);return;}
    play_42f1a0(command);
}
void PcSound::play_42f1a0(std::uint32_t command){
    const std::uint32_t id=command&0x7ffu;
    if(master_957be4<=1)return;
    if(id>=Ids){error("42F1A0: id outside the sound table");return;}
    if(slot_[id]>=0xffu||!se_[id]){++stats.unmapped;++unmapped_ids[id];return;}   // "not loaded"
    const auto vol=ftol(log10_or_inf(double(master_957be4)*K62817c)*K6281a8+double((std::int32_t(kSoundIds[id].category)-15)*300));
    const std::int32_t pan=(command&0x1000u)?1000:(command&0x2000u)?-1000:0;
    if(play_412770(se_[id],(command&0x800u)!=0u,vol,-1,pan,0.0f,0.0f)>=0)++stats.effect_plays;
}
void PcSound::clear_all_427630(){
    if(!ready)return;
    for(std::uint32_t id=0;id<Ids;++id)if(slot_[id]<0xffu&&id!=0x40u&&se_[id])stop_412880(se_[id],-1);
    for(std::uint32_t ch=0;ch<Channels;++ch)if(channels_[ch].x!=0){channels_[ch].x=0;curves_42fa30(ch);}
}

// ---- ICS engine channels -------------------------------------------------
void PcSound::ics_42eff0(std::uint32_t ch,std::uint32_t value,std::uint32_t code){
    if(!ready)return;
    if(ch>=Channels){error("42EFF0: channel outside the 17 ICS channels");return;}
    ++stats.ics_params;
    auto& c=channels_[ch];
    switch(code){
    case 0xa5000000u:definition_42f790(ch,value);break;
    case 0xa5100000u:c.volume=std::int32_t(value);break;
    case 0xa5200000u:c.pan=std::int32_t(value);break;
    case 0xa6000000u:if(c.x!=std::int32_t(value)){c.x=std::int32_t(value);curves_42fa30(ch);}break;
    case 0xa7000000u:c.pitch=std::int32_t((std::uint32_t(c.pitch)&0x3ffu)|(value<<10));break;
    case 0xa7100000u:c.pitch=std::int32_t((std::uint32_t(c.pitch)&0xfffffc00u)|(value<<4));break;
    default:break;
    }
}
void PcSound::definition_42f790(std::uint32_t ch,std::uint32_t id){
    auto& c=channels_[ch];
    if(c.id==id)return;
    if(c.def){
        for(std::uint32_t l=0;l<Layers;++l){
            auto& s=c.layers[l];
            if(s.voice>=0){
                if(l<c.def->layers){const auto sample=u32(layer_bytes(*c.def,l));if(sample<Ids)stop_4129c0(ics_[sample],s.voice);}
                s.voice=-1;
            }
            s.volume=0;
        }
    }
    for(std::size_t k=0;k<IcsDefinitionCount;++k)
        if(kIcsDefinitions[k].id==id){c.def=&kIcsDefinitions[k];c.id=id;return;}
}
// 42FA30 (its first instruction is behind bridge 1039A9C: the channel index scaled by 0x260).
void PcSound::curves_42fa30(std::uint32_t ch){
    auto& c=channels_[ch];
    if(!c.def)return;
    for(std::uint32_t l=0;l<c.def->layers&&l<Layers;++l){
        auto& s=c.layers[l];const auto* ly=layer_bytes(*c.def,l);
        s.prev_pitch=s.pitch;s.prev_volume=s.volume;s.prev_pan=s.pan;
        s.volume=curve_42fa30(c.x,ly+0x4c);
        s.pitch=curve_pitch_42fb30(c.x,ly+0xc);
        s.pan=curve_42fa30(c.x,ly+0x6c);
    }
}
void PcSound::volume_pan_42f850(std::uint32_t ch,std::uint32_t l){
    auto& c=channels_[ch];auto& s=c.layers[l];const auto* ly=layer_bytes(*c.def,l);
    if(s.voice<0)return;
    const auto sample=u32(ly);if(sample>=Ids||!ics_[sample])return;
    auto& voice=ics_[sample].voices[std::size_t(s.voice)];
    double v=log10_or_inf(double(c.volume)*K6281ac)*K6281a8+log10_or_inf(double(s.volume)*K6281ac)*K6281a8;
    v+=log10_or_inf(double(master_957be4)*K62817c)*K6281a8;
    v+=double(15-std::int32_t(s16(ly+0xa)))*K6281a4;
    if(v>0.0)v=0.0;else if(v<K628180)v=K628180;                           // fcom 0 / fcom -10000
    if(s.volume!=s.prev_volume&&!std::isnan(v))voice.volume=ftol(v);       // 412B60
    if(s.pan!=s.prev_pan){
        const std::int32_t pan=ftol(double(c.pan+s.pan)*K6281a0)*10000;   // 412C60
        if(pan>=-10000&&pan<=10000)voice.pan=pan;
    }
}
void PcSound::frequency_42f980(std::uint32_t ch,std::uint32_t l){
    auto& c=channels_[ch];auto& s=c.layers[l];const auto* ly=layer_bytes(*c.def,l);
    if(s.voice<0)return;
    if(s.pitch==s.prev_pitch)return;
    const auto sample=u32(ly);if(sample>=Ids||!ics_[sample])return;
    const double e=(double(s.pitch)*K628194+double(c.pitch)*K628194-double(std::int32_t(s16(ly+8))-0x3c)*K628194*K628190)*K62818c;
    const std::int32_t f=ftol(std::pow(2.0,e)*double(kSoundIds[sample].ics_rate));   // 5826D0 pow, fimul 7790FE
    if(f==0)ics_[sample].voices[std::size_t(s.voice)].freq=ics_[sample].rate;
    else if(f>=100&&f<=200000)ics_[sample].voices[std::size_t(s.voice)].freq=std::uint32_t(f);   // 412BE0
}
void PcSound::tick_42f330(){
    if(!ready)return;
    if((++tick_98abd8)&3u)return;
    ++stats.ticks;
    for(std::uint32_t ch=0;ch<Channels;++ch){
        auto& c=channels_[ch];
        if(!c.def)continue;
        for(std::uint32_t l=0;l<c.def->layers&&l<Layers;++l){
            auto& s=c.layers[l];const auto* ly=layer_bytes(*c.def,l);
            std::int32_t gain=s.volume*c.volume;
            if(mute_95b250)gain=0;
            const auto sample=u32(ly);
            Buffer* buffer=sample<Ids?&ics_[sample]:nullptr;
            if(gain!=0&&master_957be4>1&&s.voice<=-1){
                s.voice=buffer?play_412770(*buffer,true,-10000,-1,0,float(s16(ly+4)),float(s16(ly+6))):ErrNoBuffer;
                s.prev_pitch=s.prev_volume=s.prev_pan=std::int32_t(0xefffffffu);
                if(s.voice>=0)++stats.ics_starts;
                volume_pan_42f850(ch,l);frequency_42f980(ch,l);
            }else if(s.voice>=0){
                volume_pan_42f850(ch,l);frequency_42f980(ch,l);
                if(gain==0){if(buffer)stop_4129c0(*buffer,s.voice);s.voice=-1;++stats.ics_stops;}
            }
        }
    }
    fades_412120();
    // OR2_ICS_TRACE=<ticks between dumps> (1 tick = 4 frames; empty or 0 = 60).
    static const unsigned trace=[]{const char* e=std::getenv("OR2_ICS_TRACE");if(!e)return 0u;const auto n=unsigned(std::strtoul(e,nullptr,10));return n?n:60u;}();
    if(trace&&(stats.ticks%trace)==0u)for(std::uint32_t ch=0;ch<Channels;++ch){
        const auto& c=channels_[ch];if(!c.def||c.volume==0)continue;
        std::fprintf(stderr,"[ics] ch%u id=%x x=%d vol=%d:",ch,c.id,c.x,c.volume);
        for(std::uint32_t l=0;l<c.def->layers&&l<Layers;++l){const auto& s=c.layers[l];const auto sample=u32(layer_bytes(*c.def,l));
            std::fprintf(stderr," [l%u s%x lv%d v%d",l,sample,s.volume,s.voice);
            if(s.voice>=0&&sample<Ids&&ics_[sample]){const auto& v=ics_[sample].voices[std::size_t(s.voice)];
                std::fprintf(stderr," mb%d f%u play%d n%zu",v.volume,v.freq,int(v.playing),ics_[sample].pcm?ics_[sample].pcm->size():0u);}
            std::fprintf(stderr,"]");}
        std::fprintf(stderr,"\n");}
}

// ---- mixer -------------------------------------------------------------
void PcSound::mix_add(double* acc,std::size_t frames){
    auto mix=[&](Buffer& b){
        if(!b)return;
        const auto& pcm=*b.pcm;const std::size_t n=pcm.size();
        if(n==0){for(auto& v:b.voices)v.playing=false;return;}
        for(auto& v:b.voices){
            if(!v.playing)continue;
            const double gain=v.volume<=-10000?0.0:std::pow(10.0,double(v.volume)/2000.0);
            const double left=gain*(v.pan>0?std::pow(10.0,-double(v.pan)/2000.0):1.0);
            const double right=gain*(v.pan<0?std::pow(10.0,double(v.pan)/2000.0):1.0);
            const double step=double(v.freq)/48000.0;
            for(std::size_t f=0;f<frames;++f){
                std::size_t i=std::size_t(v.pos);
                if(i>=n){
                    if(!v.loop){v.playing=false;v.pos=0;break;}
                    v.pos=std::fmod(v.pos,double(n));i=std::size_t(v.pos);
                }
                const std::size_t j=i+1<n?i+1:(v.loop?0:i);
                const double fr=v.pos-double(i);
                const double s=pcm[i]+(pcm[j]-pcm[i])*fr;
                acc[f*2]+=s*left;acc[f*2+1]+=s*right;
                v.pos+=step;
            }
        }
    };
    for(auto& b:se_)mix(b);
    for(auto& b:ics_)mix(b);
    stats.mixed_frames+=frames;
}
std::string PcSound::status()const{
    std::ostringstream o;
    o<<"pc sound ready="<<ready<<" requests="<<stats.requests<<" loads="<<stats.loads<<" bytes="<<stats.load_bytes<<" unloads="<<stats.unloads
     <<" effects="<<stats.effects<<" plays="<<stats.effect_plays<<" stops="<<stats.effect_stops<<" unmapped="<<stats.unmapped
     <<" saturated="<<stats.saturated<<" ics_params="<<stats.ics_params<<" ics_starts="<<stats.ics_starts<<" ics_stops="<<stats.ics_stops
     <<" ticks="<<stats.ticks<<" fades="<<stats.fades<<" replaced="<<stats.replaced<<" slots:";
    for(std::uint32_t s=0;s<Slots;++s)if(slot_bank_[s]!=0xffu)o<<' '<<s<<'='<<kSoundBanks[slot_bank_[s]].name;
    o<<" voice_set="<<voice_set_754b0c<<" channels:";
    for(std::uint32_t ch=0;ch<Channels;++ch){
        const auto& c=channels_[ch];if(!c.def)continue;
        unsigned playing=0;for(std::uint32_t l=0;l<c.def->layers&&l<Layers;++l)playing+=c.layers[l].voice>=0;
        o<<' '<<ch<<":"<<std::hex<<c.id<<std::dec<<"/x"<<c.x<<"/v"<<c.volume<<"/p"<<playing;
    }
    if(!unmapped_ids.empty()){o<<" unmapped:";for(const auto& [id,n]:unmapped_ids)o<<' '<<std::hex<<id<<std::dec<<'x'<<n;}
    for(const auto& e:errors)o<<" error: "<<e<<';';
    return o.str();
}
}
