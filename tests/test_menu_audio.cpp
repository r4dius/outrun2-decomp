#include "platform/menu_audio.hpp"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iterator>
#include <filesystem>
using outrun::platform::MenuAudio;
static unsigned checks;
#define CHECK(x) do{++checks;if(!(x)){std::fprintf(stderr,"menu audio line %d: %s\n",__LINE__,#x);std::exit(1);}}while(0)
static std::vector<std::uint8_t> read(const char* path){std::ifstream f(path,std::ios::binary);CHECK(bool(f));return {std::istreambuf_iterator<char>(f),{}};}
static unsigned u16(const std::vector<std::uint8_t>& b,std::size_t p){CHECK(p+2<=b.size());return b[p]|unsigned(b[p+1])<<8;}
static unsigned u32(const std::vector<std::uint8_t>& b,std::size_t p){return u16(b,p)|u16(b,p+2)<<16;}
static void put(std::vector<std::uint8_t>& b,unsigned v){for(unsigned j=0;j<4;++j)b.push_back(std::uint8_t(v>>(8*j)));}
int main(int argc,char** argv){
    CHECK(argc==3);const auto retail=read(argv[1]),exe=read(argv[2]);
    const auto pe=u32(exe,0x3c),sections=u16(exe,pe+6),optional=u16(exe,pe+20),image=u32(exe,pe+24+28);
    auto offset=[&](unsigned va){for(unsigned i=0;i<sections;++i){const auto p=pe+24+optional+i*40,rva=u32(exe,p+12),size=u32(exe,p+16),raw=u32(exe,p+20);if(va>=image+rva&&va-image-rva<size)return std::size_t(raw+va-image-rva);}CHECK(false);return std::size_t(0);};
    // Verify every copied metadata field against the user's actual PC image.
    const unsigned ids[]{0,1,64},sizes[]{47512,29016,47460},rates[]{30000,22050,30000},voices[]{6,7,7};
    for(unsigned i=0;i<3;++i){const auto p=offset(0x7763e0+ids[i]*20);CHECK(u32(exe,p+4)==sizes[i]);CHECK(u16(exe,p+8)==rates[i]);CHECK(u16(exe,p+12)==14);CHECK(u32(exe,offset(0x773868+ids[i]*4))==voices[i]);}
    CHECK(u32(exe,offset(0x62817c))==0x3c000000);CHECK(u32(exe,offset(0x6281a8))==0x44fa0000);
    MenuAudio audio;std::string error;std::vector<std::int16_t> pcm;
    CHECK(!audio.command(1,error));CHECK(!audio.mix(1,pcm,error));CHECK(audio.load(retail,error));
    CHECK(audio.command(64,error));CHECK(audio.mix(40000,pcm,error));CHECK(!audio.active_voices());
    CHECK(std::any_of(pcm.begin(),pcm.end(),[](auto n){return n!=0;}));
    for(std::size_t i=0;i<pcm.size();i+=2)CHECK(pcm[i]==pcm[i+1]);
    // Uniform fixtures isolate the source-rate/voice/gain/pan behavior from
    // waveform details. File layout and all metadata still match retail.
    std::vector<std::uint8_t> pak;
    for(unsigned i=0;i<3;++i){put(pak,ids[i]);put(pak,sizes[i]);for(unsigned j=0;j<sizes[i]/2;++j){pak.push_back(0x10);pak.push_back(0x27);}}
    put(pak,~0u);put(pak,~0u);CHECK(audio.load(pak,error));
    const int db=int(2000.L*std::log10(65.L/128.L)-300);const auto expected=std::lround(10000*std::pow(10.,db/2000.));
    CHECK(audio.command(1,error));CHECK(audio.mix(1,pcm,error));CHECK(pcm[0]==expected&&pcm[1]==expected);
    const auto duration=(std::uint64_t(sizes[1]/2)*48000+rates[1]-1)/rates[1];
    CHECK(audio.mix(duration-2,pcm,error)&&audio.active_voices()==1);CHECK(audio.mix(1,pcm,error)&&!audio.active_voices());
    CHECK(audio.load(pak,error));for(unsigned i=0;i<8;++i)CHECK(audio.command(1,error));
    CHECK(audio.active_voices()==7&&audio.stats.starts==7&&audio.stats.saturated==1);
    CHECK(audio.command(0x8001,error)&&!audio.active_voices());CHECK(audio.command(0x4a9,error));
    CHECK(!audio.command(2,error)&&!audio.active_voices());CHECK(!audio.command(0x4040,error)&&!audio.active_voices());
    CHECK(audio.command(0x4000,error)&&audio.active_voices()==2);CHECK(audio.mix(1,pcm,error));
    CHECK(pcm[0]==pcm[1]&&pcm[0]>expected);CHECK(audio.command(0x8000,error)&&audio.active_voices()==1);
    CHECK(audio.command(0x8001,error));CHECK(audio.command(0x1000,error));CHECK(audio.mix(1,pcm,error));CHECK(pcm[0]<pcm[1]&&pcm[1]==expected);
    CHECK(audio.command(0x8000,error));CHECK(audio.command(0x2000,error));CHECK(audio.mix(1,pcm,error));CHECK(pcm[0]==expected&&pcm[1]<pcm[0]);
    CHECK(audio.load(pak,error));CHECK(audio.command(0x801,error));CHECK(audio.mix(96000,pcm,error)&&audio.active_voices()==1);
    // Movie and effects share one sample clock. Stopping movie never destroys
    // a sound voice; stopping sound never discards queued movie samples.
    CHECK(audio.queue_movie({30000,-30000,123,-123},error));CHECK(audio.mix(1,pcm,error));CHECK(pcm[0]==32767&&pcm[1]==-30000+expected);
    audio.stop_movie();CHECK(audio.active_voices()==1);CHECK(audio.mix(1,pcm,error)&&pcm[0]==expected&&pcm[1]==expected);
    CHECK(audio.queue_movie({11,-11,22,-22},error));CHECK(audio.command(0x8001,error));CHECK(audio.mix(3,pcm,error));CHECK((pcm==std::vector<std::int16_t>{11,-11,22,-22,0,0}));
    CHECK(!audio.queue_movie({1},error));CHECK(!audio.queue_movie(std::vector<std::int16_t>(192002),error));CHECK(!audio.mix(96001,pcm,error));
    // Chunk boundaries cannot change a voice's duration or resampling phase.
    MenuAudio whole,chunked;CHECK(whole.load(retail,error)&&chunked.load(retail,error));CHECK(whole.command(1,error)&&chunked.command(1,error));
    std::vector<std::int16_t> contiguous,joined;CHECK(whole.mix(40000,contiguous,error));
    for(unsigned remaining=40000;remaining;){const auto n=std::min(remaining,137u);CHECK(chunked.mix(n,pcm,error));joined.insert(joined.end(),pcm.begin(),pcm.end());remaining-=n;}
    CHECK(joined==contiguous&&!chunked.active_voices());
    for(std::size_t size:{0u,1u,7u,8u,47519u,47520u,124012u,124019u}){auto bad=retail;bad.resize(size);CHECK(!audio.load(bad,error)&&!audio.loaded());}
    auto bad=pak;bad.push_back(0);CHECK(!audio.load(bad,error));bad=pak;bad[0]=2;CHECK(!audio.load(bad,error));bad=pak;bad[47520]=0;CHECK(!audio.load(bad,error));
    CHECK(audio.load_file(argv[1],error));CHECK(!audio.load_file("missing-menu.pak",error)&&!audio.loaded());
    const auto fe_path=(std::filesystem::path(argv[1]).parent_path()/"FE.PAK").string();
    const auto fe=read(fe_path.c_str());CHECK(audio.load(retail,error));CHECK(audio.load_frontend_file(fe_path,error));
    unsigned fe_count=0;
    for(std::size_t p=0;u32(fe,p)!=~0u;){const auto id=u32(fe,p),size=u32(fe,p+4);p+=8;++fe_count;
        const auto metadata=offset(0x7763e0+id*20);
        const auto rate=u16(exe,metadata+8),category=u16(exe,metadata+12);
        CHECK(size==u32(exe,metadata+4));CHECK(rate>0);
        const auto length=(std::uint64_t(size/2)*48000+rate-1)/rate;
        CHECK(audio.command(id,error));CHECK(audio.mix(256,pcm,error));
        const int attenuation=int(2000.L*std::log10(65.L/128.L)+(int(category)-15)*300);
        const double gain=std::pow(10.,attenuation/2000.);
        for(unsigned sample=0;sample<256;++sample){const std::uint64_t phase=std::uint64_t(sample)*rate;
            const auto index=phase/48000;const double a=std::int16_t(u16(fe,p+index*2)),b=std::int16_t(u16(fe,p+(index+1)*2));
            CHECK(pcm[sample*2]==std::lround((a+(b-a)*double(phase%48000)/48000.)*gain)&&pcm[sample*2]==pcm[sample*2+1]);}
        for(auto remaining=length-257;remaining;){const auto chunk=std::min<std::uint64_t>(remaining,48000);
            CHECK(audio.mix(chunk,pcm,error));remaining-=chunk;}
        CHECK(audio.active_voices()==1);
        CHECK(audio.mix(1,pcm,error)&&audio.active_voices()==0);
        const auto limit=std::max(1u,u32(exe,offset(0x773868+4*id)));
        for(unsigned i=0;i<limit+1;++i)CHECK(audio.command(id,error));
        CHECK(audio.active_voices()==limit);CHECK(audio.command(id|0x8000,error)&&!audio.active_voices());p+=size;
    }
    CHECK(fe_count==64);CHECK(audio.command(0x801,error));const auto retained=audio.active_voices();
    CHECK(!audio.load_frontend_file("missing-fe.pak",error)&&audio.active_voices()==retained&&audio.loaded());
    CHECK(audio.load_frontend_file(fe_path,error)&&audio.active_voices()==retained); // MENU voice survives FE reload
    CHECK(audio.load(retail,error)&&!audio.command(3,error));
    std::printf("menu_audio: %u checks; MENU/FE metadata, voices, sample clock, movie mixing and failures\n",checks);
}
