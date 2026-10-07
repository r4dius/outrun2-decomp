#include "system/title_movie.hpp"
#include <cstdio>
#include <stdexcept>
#include <limits>
#include <algorithm>
using outrun::platform::TitleMovie;
namespace {unsigned checks{};void req(bool v,const char*m){++checks;if(!v)throw std::runtime_error(m);}}
int main(int argc,char** argv){try{
    req(argc==2,"owned title movie path required");TitleMovie movie;std::string error;
    req(!movie.open("/missing/or2/TitleScreen.bik",error)&&!movie.active(),"missing file is not success");
    req(movie.open(argv[1],error),error.c_str());
    req(movie.width()==640u&&movie.height()==480u,"original movie dimensions");
    req(movie.decoded_frames()==1u&&movie.rgba().size()==640u*480u*4u,"first frame present");
    const auto first=movie.rgba();
    req(movie.has_audio(),"original Bink soundtrack found");
    std::vector<std::int16_t> audio;std::uint64_t samples{};bool nonzero=false;
    req(!movie.advance(-1.0,error),"negative clock rejected");
    req(!movie.advance(std::numeric_limits<double>::quiet_NaN(),error),"NaN clock rejected");
    // Decode past the original 78.61 second duration, exercising the rewind.
    for(unsigned tick=0;tick<=6000u;++tick){
        req(movie.advance(double(tick)/60.0,error),error.c_str());
        movie.take_audio(audio);samples+=audio.size();
        nonzero|=std::any_of(audio.begin(),audio.end(),[](auto value){return value!=0;});
    }
    req(nonzero&&samples>9500000u&&samples<9700000u,"100 seconds of real 48 kHz stereo audio including loop");
    req(movie.decoded_audio_frames()*2u==samples,"all decoded PCM delivered once");
    req(movie.decoded_frames()==2998u,"100 seconds decoded at original rational frame rate");
    req(movie.rgba()!=first,"animation is not a repeated static image");
    const auto frames=movie.decoded_frames();
    req(movie.advance(1000.0,error)&&movie.decoded_frames()==frames+8u,"catch-up bounded to eight frames");
    movie.close();req(!movie.active()&&movie.rgba().empty(),"stop releases decoded video");
    req(movie.open(argv[1],error)&&movie.rgba()==first,"re-entry restarts original first frame");
    movie.close();req(!movie.advance(0.0,error),"stopped decoder rejects advancement");
    std::printf("retail_title_movie: %u checks; original Bink playback, loop, stop and reopen verified\n",checks);
    return 0;
}catch(const std::exception& e){std::fprintf(stderr,"FAIL: %s\n",e.what());return 1;}}
