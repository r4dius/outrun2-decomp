#include "switch_audio.hpp"
#include "platform/menu_audio.hpp"
#include <algorithm>
#include <switch.h>
#include <cstdio>
using outrun::switch_runtime::SwitchAudio;
namespace {unsigned checks{};void req(bool v,const char*m){++checks;if(!v)throw std::runtime_error(m);}}
int main(int argc,char** argv){try{
    SwitchAudio output;std::string error;std::vector<std::int16_t> pcm(48000u);
    for(std::size_t i=0;i<pcm.size();++i)pcm[i]=std::int16_t(i&0x7fffu);
    req(!output.submit({},error),"closed output rejected");
    req(output.open(error),error.c_str());audio_test::release=false;
    req(output.submit(pcm,error),error.c_str());req(audio_test::busy.size()==8u,"bounded eight-buffer pool");
    req(output.submit({},error),"pending PCM remains queued without touching in-flight buffers");
    audio_test::release=true;req(output.submit({},error),error.c_str());
    req(output.submitted_frames()==24000u&&audio_test::played==pcm,"all samples delivered in order once");
    req(!output.submit({1},error),"odd stereo sample count rejected");
    audio_test::fail_append=0x559;req(!output.submit({1,2},error)&&error.find("00000559")!=std::string::npos,"device error retained");
    output.close();req(!audio_test::initialized&&!audio_test::started&&audio_test::busy.empty(),"stop flush and close before freeing buffers");
    audio_test::fail_append=0;req(output.open(error),"movie re-entry reopens audio");
    req(output.submitted_frames()==0u,"new playback counter reset");
    req(!output.submit(std::vector<std::int16_t>(192002u),error),"unbounded buffering rejected");
    output.close();req(audio_test::closes==2&&audio_test::starts==2,"two balanced lifecycles");
    req(argc==2,"retail sound bank argument");
    outrun::platform::MenuAudio mixer;req(mixer.load_file(argv[1],error),error.c_str());
    audio_test::played.clear();req(output.open(error),error.c_str());
    const auto starts=audio_test::starts;std::vector<std::int16_t> expected;
    req(mixer.command(64,error),error.c_str()); // actual welcome sound
    for(unsigned frame=0;frame<100;++frame){
        if(frame==2)req(mixer.queue_movie({100,-100,200,-200},error),error.c_str());
        if(frame==3){mixer.stop_movie();req(mixer.active_voices()>0,"movie stop preserves welcome voice");}
        if(frame==10)req(mixer.command(1,error),error.c_str()); // actual navigation sound
        req(mixer.mix(800,pcm,error),error.c_str());expected.insert(expected.end(),pcm.begin(),pcm.end());
        req(output.submit(pcm,error),error.c_str());
    }
    req(output.submit({},error),error.c_str());
    req(audio_test::starts==starts,"one service session across movie and effect transitions");
    req(audio_test::played==expected,"retail effects reach DMA in mixed PCM order");
    req(std::any_of(expected.begin(),expected.end(),[](auto n){return n!=0;}),"actual audible PCM, not only silent counters");
    req(!mixer.active_voices()&&mixer.stats.starts==2,"both original sounds completed");
    output.close();
    std::printf("switch_audio: %u checks; queue ordering, DMA lifetime, restart and failure paths (simulated service)\n",checks);return 0;
}catch(const std::exception& e){std::fprintf(stderr,"FAIL: %s\n",e.what());return 1;}}
