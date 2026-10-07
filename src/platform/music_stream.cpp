#include "platform/music_stream.hpp"
#include <algorithm>
#include <cstring>
#if defined(OR2_HAVE_VORBIS)
#include <vorbis/vorbisfile.h>
#endif

namespace outrun::platform {
namespace {
// PC table 771AD8 (names as the EXE spells them; the retail SD tree is
// case-insensitive).
constexpr const char* Tracks771ad8[MusicTrackCount]{
    "01_Splash_wave.ogg","02_Magical_Sound_Shower.ogg","03_Passing_Breeze.ogg","04_Risky_Ride.ogg","05_Shiny_World.ogg",
    "06_Night_Flight.ogg","07_Life_was_a_Bore.ogg","08_Radiation.ogg","09_Night_Bird.ogg","10_Splash_wave_1986.ogg",
    "11_Magical_Sound_Shower_1986.ogg","12_Passing_Breeze_1986.ogg","13_Shake_the_Street_1989.ogg","14_Rush_a_Difficulty_1989.ogg",
    "15_Who_are_You_1989.ogg","16_Keep_Your_Heart_1989.ogg","17_Splash_Wave_EuroMix.ogg","18_Magical_Sound_Shower_EuroMix.ogg",
    "19_Passing_Breeze_EuroMix.ogg","20_Risky_Ride_Guitar_Mix.ogg","21_Shake_the_Street_ARRANGED.ogg","22_Who_are_you_ARRANGED.ogg",
    "23_Rush_a_difficulty_ARRANGED.ogg","24_Keep_your_Heart_ ARRANGED.ogg","25_Shiny_World_prototype.ogg","26_Night_Flight_prototype.ogg",
    "27_Life_was_a_Bore_Instrumental.ogg","28_Night_Flight_Instrumental.ogg","LAST_WAVE.ogg","SEGA441.ogg","BEACH_WAVE.ogg",
    "TITLE_PATTERN1.ogg","TITLE_01.ogg","SEL_01_Splash_Wave.ogg","SEL_02_Magical_Sound_Shower.ogg","SEL_03_Passing_Breeze.ogg",
    "SEL_04_Risky_Ride.ogg","SEL_05_Shiny_World.ogg","SEL_06_Night_Flight.ogg","SEL_07_Life_was_a_Bore.ogg","SEL_08_Radiation.ogg",
    "SEL_09_Night_Bird.ogg","SEL_10_Splash_Wave_1986.ogg","SEL_11_Magical_Sound_Shower_1986.ogg","SEL_12_Passing_Breeze_1986.ogg",
    "SEL_13_Shake_the_Street_1989.ogg","SEL_14_Rush_a_Difficulty.ogg","SEL_15_Who_are_You_1989.ogg","SEL_16_Keep_Your_Heart_1989.ogg",
    "SEL_17_Splash_Wave_EuroMix.ogg","SEL_18_Magical_Sound_Shower_EuroMix.ogg","SEL_19_Passing_Breeze_EuroMix.ogg",
    "SEL_20_Risky_Ride_Guitar.ogg","SEL_21_Shake_the_Street_ARRANGED.ogg","SEL_22_Who_are_you_ARRANGED.ogg",
    "SEL_23_Rush_a_Difficulty_ARRANGED.ogg","SEL_24_Keep_your_Heart_ARRANGED.ogg","SEL_25_Shiny_World_Prototype.ogg",
    "SEL_26_Night_Flight_Prototype.ogg","SEL_27_Life_was_a_Bore_Instrumental.ogg","SEL_28_Night_Flight_Instrumental.ogg",
    "OR2ED1.ogg","OR2ED2.ogg","OR2ED3A.ogg","OR2ED3B.ogg","OR2ED4.ogg","OR2ED5.ogg"};
bool fail(std::string& e,const char* m){e=m;return false;}
}
const char* music_track_name_771ad8(unsigned track){return track<MusicTrackCount?Tracks771ad8[track]:nullptr;}

struct MusicStream::Impl {
    std::vector<std::uint8_t> file;
    std::size_t read_at{};
    bool open{},loop{},ended{};
    std::uint32_t rate{},channels{};
    std::vector<std::int16_t> pending;   // decoded source frames (stereo) not yet consumed
    std::size_t base{};                  // index of pending[0] (frames)
    double position{};                   // source frame position of the next output frame
#if defined(OR2_HAVE_VORBIS)
    OggVorbis_File vf{};
    static std::size_t read(void* p,std::size_t size,std::size_t n,void* user){
        auto& s=*static_cast<Impl*>(user);const std::size_t want=size*n,left=s.file.size()-s.read_at,take=std::min(want,left);
        std::memcpy(p,s.file.data()+s.read_at,take);s.read_at+=take;return size?take/size:0;}
    static int seek(void* user,ogg_int64_t offset,int whence){
        auto& s=*static_cast<Impl*>(user);std::int64_t to=offset;
        if(whence==SEEK_CUR)to+=std::int64_t(s.read_at);else if(whence==SEEK_END)to+=std::int64_t(s.file.size());
        if(to<0||to>std::int64_t(s.file.size()))return -1;s.read_at=std::size_t(to);return 0;}
    static long tell(void* user){return long(static_cast<Impl*>(user)->read_at);}
#endif
    void close(){
#if defined(OR2_HAVE_VORBIS)
        if(open)ov_clear(&vf);
#endif
        open=false;ended=false;pending.clear();base=0;position=0;file.clear();read_at=0;
    }
};
MusicStream::MusicStream():impl_(new Impl){}
MusicStream::~MusicStream(){impl_->close();}
bool MusicStream::decoder_available(){
#if defined(OR2_HAVE_VORBIS)
    return true;
#else
    return false;
#endif
}
bool MusicStream::open(std::vector<std::uint8_t> file,bool loop,std::string& error){
    auto& s=*impl_;s.close();
#if defined(OR2_HAVE_VORBIS)
    s.file=std::move(file);s.read_at=0;
    const ov_callbacks cb{&Impl::read,&Impl::seek,nullptr,&Impl::tell};
    if(ov_open_callbacks(&s,&s.vf,nullptr,0,cb)!=0){s.file.clear();return fail(error,"music: not an Ogg Vorbis stream");}
    const vorbis_info* info=ov_info(&s.vf,-1);
    if(!info||info->channels<1||info->channels>2||info->rate<8000||info->rate>192000){ov_clear(&s.vf);s.file.clear();return fail(error,"music: unsupported Vorbis layout");}
    s.open=true;s.loop=loop;s.rate=std::uint32_t(info->rate);s.channels=std::uint32_t(info->channels);
    stats.rate=s.rate;stats.channels=s.channels;++stats.opens;
    return true;
#else
    (void)file;(void)loop;
    return fail(error,"music: no Vorbis decoder in this build");
#endif
}
void MusicStream::stop(){if(impl_->open)++stats.stops;impl_->close();}
bool MusicStream::playing()const{return impl_->open&&!impl_->ended;}
bool MusicStream::refill(std::string& error){
#if defined(OR2_HAVE_VORBIS)
    auto& s=*impl_;
    char raw[8192];int section=0;
    for(int attempt=0;attempt<4;++attempt){
        const long got=ov_read(&s.vf,raw,sizeof raw,0,2,1,&section);
        if(got>0){
            const std::size_t samples=std::size_t(got)/2u;const auto* p=reinterpret_cast<const std::int16_t*>(raw);
            if(s.channels==2)s.pending.insert(s.pending.end(),p,p+samples);
            else for(std::size_t i=0;i<samples;++i){s.pending.push_back(p[i]);s.pending.push_back(p[i]);}
            return true;
        }
        if(got==0){
            if(!s.loop){s.ended=true;++stats.ends;return true;}
            if(ov_raw_seek(&s.vf,0)!=0)return fail(error,"music: loop seek failed");
            ++stats.loops;continue;
        }
        if(got==OV_HOLE)continue;           // interruption in the data: skip
        return fail(error,"music: Vorbis decode error");
    }
    return fail(error,"music: no data after a loop seek");
#else
    return fail(error,"music: no Vorbis decoder in this build");
#endif
}
bool MusicStream::mix_add(double* acc,std::size_t frames,double gain,std::string& error){
    auto& s=*impl_;
    if(!s.open||s.ended)return true;
    const double step=double(s.rate)/48000.0;
    for(std::size_t f=0;f<frames;++f){
        const std::size_t index=std::size_t(s.position);
        // frames index and index+1 (relative to base) must be decoded
        while(!s.ended&&(index+2u-s.base)*2u>s.pending.size())if(!refill(error))return false;
        const std::size_t have=s.pending.size()/2u;
        if(index-s.base>=have)break;                                   // stream finished
        const std::size_t i0=index-s.base,i1=std::min(i0+1u,have-1u);
        const double fraction=s.position-double(index);
        const double l=s.pending[i0*2]+(s.pending[i1*2]-s.pending[i0*2])*fraction;
        const double r=s.pending[i0*2+1]+(s.pending[i1*2+1]-s.pending[i0*2+1])*fraction;
        acc[f*2]+=l*gain;acc[f*2+1]+=r*gain;
        s.position+=step;++stats.frames;
    }
    // drop the consumed source frames
    const std::size_t consumed=std::size_t(s.position)-s.base;
    if(consumed>4096u&&consumed*2u<=s.pending.size()){
        s.pending.erase(s.pending.begin(),s.pending.begin()+std::ptrdiff_t(consumed*2u));s.base+=consumed;
    }
    if(s.ended&&std::size_t(s.position)-s.base>=s.pending.size()/2u)s.close();
    return true;
}
}
