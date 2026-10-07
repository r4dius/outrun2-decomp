#include "menu_audio.hpp"
#include <algorithm>
#include <cmath>
#include <fstream>
#include <iterator>

namespace outrun::platform {
namespace {
struct Definition {unsigned id,bytes,rate,category,voices;};
// EXE 7763E0 + id*20 (+4/+8/+C) and 773868 + id*4.
constexpr Definition definitions[]={{0,47512,30000,14,6},{1,29016,22050,14,7},{64,47460,30000,14,7}};
// FE.PAK uses the same PCM format; high words in the EXE rate/category fields
// are other metadata, not part of sample rate or 42F1A0's volume category.
const auto frontend_definitions=[](){
    constexpr unsigned sizes[]={44930,34538,75400,32106,25542,24874,23884,21698,18888,53406,28912,24728,60030,26770,37008,33636,34596,35374,28778,22748,25950,26760,22562,22146,23028,26186,17820,30774,22670,23250,25046,22254,23382,24680,24176,22356,24690,25170,28950,24900,28784,29218,30192,30450,32090,28136,40510,27048,25174,25672,21252,28354,36252,33796,24244,29624,19988,20690,15884,17030,13334};
    static_assert(std::size(sizes)==61);
    std::vector<Definition> result;
    for(unsigned i=0;i<std::size(sizes);++i)result.push_back({i+3,sizes[i],i<3?44100u:12000u,i==2?14u:15u,i==0?5u:1u});
    result.push_back({356,15364,44100,12,1});result.push_back({397,11156,22050,15,14});result.push_back({398,11156,22050,15,14});return result;
}();
unsigned u32(const std::uint8_t* p){return unsigned(p[0])|(unsigned(p[1])<<8)|(unsigned(p[2])<<16)|(unsigned(p[3])<<24);}
bool fail(std::string& error,const char* text){error=text;return false;}
}
void MenuAudio::clear(){sounds_={};movie_.clear();movie_offset_=0;loaded_=false;stats={};}
bool MenuAudio::load_file(const std::string& path,std::string& error){
    std::ifstream file(path,std::ios::binary|std::ios::ate);
    if(!file){clear();return fail(error,"cannot open MENU.pak");}
    const auto size=file.tellg();
    if(size<0||size>4*1024*1024){clear();return fail(error,"invalid MENU.pak size");}
    std::vector<std::uint8_t> bytes(static_cast<std::size_t>(size));file.seekg(0);
    if(!file.read(reinterpret_cast<char*>(bytes.data()),std::streamsize(bytes.size()))){clear();return fail(error,"cannot read MENU.pak");}
    return load(bytes,error);
}
bool MenuAudio::load(const std::vector<std::uint8_t>& bytes,std::string& error){
    clear();return load_bank(bytes,false,error);
}
bool MenuAudio::load_frontend_file(const std::string& path,std::string& error){
    if(!loaded_)return fail(error,"load MENU bank before FE bank");
    std::ifstream file(path,std::ios::binary|std::ios::ate);
    if(!file)return fail(error,"cannot open FE.PAK");
    const auto n=file.tellg();if(n<0||n>4*1024*1024)return fail(error,"invalid FE.PAK size");
    std::vector<std::uint8_t> bytes(static_cast<std::size_t>(n));file.seekg(0);
    if(!file.read(reinterpret_cast<char*>(bytes.data()),std::streamsize(bytes.size())))return fail(error,"cannot read FE.PAK");
    return load_bank(bytes,true,error);
}
bool MenuAudio::load_bank(const std::vector<std::uint8_t>& bytes,bool frontend,std::string& error){
    error.clear();const auto* first=frontend?frontend_definitions.data():std::begin(definitions);
    const auto count=frontend?frontend_definitions.size():std::size(definitions);const auto* last=first+count;
    std::vector<Sound> parsed(count);std::vector<bool> seen(count);
    std::size_t offset=0;bool terminated=false;
    while(bytes.size()-offset>=8){
        const auto id=u32(bytes.data()+offset),size=u32(bytes.data()+offset+4);offset+=8;
        if(id==~0u){if(size!=~0u||offset!=bytes.size())return fail(error,"invalid MENU.pak terminator");terminated=true;break;}
        const auto* def=std::find_if(first,last,[&](auto d){return d.id==id;});
        if(def==last||size!=def->bytes||size>bytes.size()-offset)return fail(error,"audio bank entry does not match original sound table");
        const auto index=std::size_t(def-first);
        if(seen[index])return fail(error,"duplicate MENU.pak sound");
        seen[index]=true;
        auto& s=parsed[index];s.id=id;s.rate=def->rate;s.category=def->category;s.voices.resize(def->voices);s.pcm.resize(size/2);
        for(std::size_t j=0;j<s.pcm.size();++j){const unsigned v=unsigned(bytes[offset+j*2])|(unsigned(bytes[offset+j*2+1])<<8);s.pcm[j]=std::int16_t(v<32768?int(v):int(v)-65536);}
        offset+=size;
    }
    if(!terminated||!std::all_of(seen.begin(),seen.end(),[](bool b){return b;}))return fail(error,"incomplete MENU.pak bank");
    if(frontend){sounds_.erase(std::remove_if(sounds_.begin(),sounds_.end(),[&](const Sound& s){return std::any_of(first,last,[&](auto d){return d.id==s.id;});}),sounds_.end());
        sounds_.insert(sounds_.end(),std::make_move_iterator(parsed.begin()),std::make_move_iterator(parsed.end()));
    }else sounds_=std::move(parsed);
    loaded_=true;return true;
}
bool MenuAudio::play(unsigned command,std::string& error){
    const unsigned id=command&0x7ff;
    auto sound=std::find_if(sounds_.begin(),sounds_.end(),[&](const Sound& s){return s.id==id;});
    if(sound==sounds_.end())return fail(error,"sound command requires a bank other than MENU.pak");
    auto voice=std::find_if(sound->voices.begin(),sound->voices.end(),[](const Voice& v){return !v.playing;});
    if(voice==sound->voices.end()){++stats.saturated;return true;} // 412700 returns -1; no stealing/restart
    // PC 957BE4 (65 at boot, 42EFA0 options SE level); constants 62817C=1/128,
    // 6281A8=2000. 42F1A0 starts nothing at a master of 1 or less.
    const int master=driver_?driver_->master_957be4:65;
    if(master<=1)return true;
    const int db=int(2000.L*std::log10((long double)master/128.L)+(int(sound->category)-15)*300);
    const double gain=std::pow(10.,double(db)/2000.);
    const int pan=(command&0x1000)?1000:(command&0x2000)?-1000:0;
    *voice={0,true,(command&0x800)!=0,gain*std::pow(10.,-double(std::max(pan,0))/2000.),gain*std::pow(10.,double(std::min(pan,0))/2000.)};
    ++stats.starts;return true;
}
bool MenuAudio::command(unsigned command,std::string& error){
    error.clear();if(!loaded_)return fail(error,"MENU sound service is not loaded");++stats.commands;
    const auto id=command&0x7ff;if(id==0x4a9)return true; // original sentinel, not an absent asset
    if(command&0x8000){
        auto sound=std::find_if(sounds_.begin(),sounds_.end(),[&](const Sound& s){return s.id==id;});
        if(sound==sounds_.end())return fail(error,"stop command requires a bank other than MENU.pak");
        for(auto& v:sound->voices)v={};
        ++stats.stops;return true;
    }
    if(command&0x4000){
        // Preflight both resources: a native missing-bank fault must not partly
        // modify playback and then be retried as though no command was issued.
        for(auto candidate:{command&0x7ff,(command+1)&0x7ff})
            if(std::none_of(sounds_.begin(),sounds_.end(),[&](const Sound& s){return s.id==candidate;}))return fail(error,"stereo sound pair requires another bank");
        return play((command&0xfff)|0x1000,error)&&play(((command+1)&0xfff)|0x2000,error);
    }
    return play(command,error);
}
bool MenuAudio::queue_movie(const std::vector<std::int16_t>& pcm,std::string& error){
    if(pcm.size()%2||pcm.size()>192000||movie_.size()-movie_offset_>192000-pcm.size())return fail(error,"movie mixer queue stalled or odd stereo count");
    if(movie_offset_){movie_.erase(movie_.begin(),movie_.begin()+movie_offset_);movie_offset_=0;}
    movie_.insert(movie_.end(),pcm.begin(),pcm.end());return true;
}
void MenuAudio::stop_movie(){movie_.clear();movie_offset_=0;}
void MenuAudio::stop_effects(){for(auto& s:sounds_)for(auto& v:s.voices)if(v.playing){v={};++stats.stops;}}
void MenuAudio::music_volume_42fc90(float level){
    // fcomp 0 with C0|C3: zero, negative and NaN levels are silent.
    if(!(level>0.f)){music_volume_95b240_=-10000;return;}
    const float mb=float(std::log10((long double)level)*3000.L-600.L);   // fyl2x, fmul, fsub, fstp float
    music_volume_95b240_=std::int32_t(mb);                               // 582194 (_ftol)
}
std::size_t MenuAudio::active_voices()const{
    std::size_t result=0;for(const auto& s:sounds_)for(const auto& v:s.voices)result+=v.playing;return result;
}
bool MenuAudio::mix(std::size_t frames,std::vector<std::int16_t>& out,std::string& error){
    if(!loaded_||frames>96000)return fail(error,"menu mixer unavailable or scheduling stall");
    out.resize(frames*2);
    // The playing voices only (the bank holds every MENU sound): voices start
    // between mix calls, so the set is fixed for this call.
    active_.clear();
    for(auto& s:sounds_)for(auto& v:s.voices)if(v.playing)active_.push_back({&s,&v});
    music_mix_.assign(frames*2,0.0);
    // A broken stream ends the music (reported), never the shared output.
    // 95B240 = -10000 stops the stream buffer (412930) until a louder level
    // plays it again (412770): the stream does not advance meanwhile.
    if(music_volume_95b240_>-10000){std::string e;
        if(!music_.mix_add(music_mix_.data(),frames,std::pow(10.,double(music_volume_95b240_)/2000.),e)){
            music_error_=e;music_.stop();music_mix_.assign(frames*2,0.0);}}
    if(driver_)driver_->mix_add(music_mix_.data(),frames);
    for(std::size_t frame=0;frame<frames;++frame){
        double left=music_mix_[frame*2],right=music_mix_[frame*2+1];
        if(movie_offset_<movie_.size()){left=movie_[movie_offset_++];right=movie_[movie_offset_++];++stats.movie_frames;}
        for(auto [sp,vp]:active_){auto& s=*sp;auto& v=*vp;
            if(!v.playing)continue;
            const auto index=std::size_t(v.phase/48000),next=index+1<s.pcm.size()?index+1:v.loop?0:index;
            const double fraction=double(v.phase%48000)/48000.;
            const double value=s.pcm[index]+(s.pcm[next]-s.pcm[index])*fraction;
            left+=value*v.left;right+=value*v.right;v.phase+=s.rate;
            const std::uint64_t length=s.pcm.size()*48000ull;
            if(v.phase>=length){if(v.loop)v.phase%=length;else v.playing=false;}
        }
        out[frame*2]=std::int16_t(std::clamp(std::lround(left),-32768L,32767L));
        out[frame*2+1]=std::int16_t(std::clamp(std::lround(right),-32768L,32767L));
    }
    if(movie_offset_==movie_.size())stop_movie();
    stats.mixed_frames+=frames;return true;
}
}
