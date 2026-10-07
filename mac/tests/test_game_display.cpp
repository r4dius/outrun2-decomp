#define SDL_MAIN_HANDLED
#include <SDL.h>
#include "audio.hpp"
#include "sdl_backend.hpp"
#include "movie_clock.hpp"
#include "system/title_movie.hpp"
#include "platform/menu_audio.hpp"
#include <cstdio>
#include <cstdlib>
static unsigned checks=0;
static void check(bool v,const char* m){++checks;if(!v){std::fprintf(stderr,"FAIL: %s (%s)\n",m,SDL_GetError());std::exit(1);}}
int main(int argc,char** argv){
    using namespace std::chrono;
    outrun::mac_runtime::MovieClock movie_clock;
    auto now=outrun::mac_runtime::MovieClock::Clock::time_point{};
    movie_clock.reset(now);
    for(unsigned frame=0;frame<60;++frame){now+=milliseconds(16);check(!movie_clock.advance(now),"normal movie timing");}
    const double before=movie_clock.elapsed();now+=seconds(3);
    check(movie_clock.advance(now),"fullscreen pause detected");
    check(movie_clock.elapsed()-before<0.02,"pause does not cause audio catch-up");
    now+=milliseconds(16);check(!movie_clock.advance(now),"movie resumes normal timing");
    if(argc==3){
        outrun::platform::TitleMovie movie;
        outrun::platform::MenuAudio mixer;
        std::string error;
        check(movie.open(argv[1],error),"owned Bink intro opens");
        check(movie.has_audio(),"owned Bink soundtrack");
        check(mixer.load_file(argv[2],error),"owned menu mixer opens");
        now={};movie_clock.reset(now);
        std::vector<std::int16_t> pcm,mixed;
        for(unsigned frame=0;frame<600;++frame){
            now+=milliseconds(frame==180||frame==360?3000:16);
            if(movie_clock.advance(now))mixer.stop_movie();
            check(movie.advance(movie_clock.elapsed(),error),"intro decodes across window pauses");
            movie.take_audio(pcm);
            check(mixer.queue_movie(pcm,error),"intro audio queue stays bounded after pause");
            check(mixer.mix(768,mixed,error),"intro soundtrack continues mixing");
        }
        check(mixer.stats.movie_frames>400000,"real movie PCM mixed after repeated pauses");
    }
    SDL_SetMainReady();check(SDL_Init(SDL_INIT_AUDIO|SDL_INIT_VIDEO)==0,"SDL init");
    std::string error;
    {
        outrun::mac_runtime::MacAudio a;
        for(unsigned cycle=0;cycle<2;++cycle){
            check(a.open(error),"open stereo output");
            check(a.submit(std::vector<std::int16_t>(2048,100),error),"submit PCM");
            check(!a.submit(std::vector<std::int16_t>(3),error),"odd stereo rejected");
            check(!a.submit(std::vector<std::int16_t>(192002),error),"queue bound enforced");
            a.close();check(a.queued_frames()==0,"closed queue cleared");
            check(!a.submit(std::vector<std::int16_t>(2),error),"closed output rejects PCM");
        }
    }
    for(unsigned cycle=0;cycle<2;++cycle){
        outrun::mac_runtime::SdlDisplay d;check(d.open(64,48,error),"display lifecycle");d.clear();
        const std::vector<std::uint8_t> pixels(32u*32*4,255);
        d.rgba(&pixels,pixels.data(),32,32,0,0,64,48);check(d.present(error),"Metal present");
        auto* window=SDL_GetWindowFromID(cycle+1);check(window!=nullptr,"test window");
        SDL_MaximizeWindow(window);SDL_PumpEvents();
        d.clear();d.rgba(&pixels,pixels.data(),32,32,0,0,64,48);check(d.present(error),"present maximized window");
        check(SDL_SetWindowFullscreen(window,SDL_WINDOW_FULLSCREEN_DESKTOP)==0,"enter fullscreen");
        SDL_PumpEvents();d.clear();d.rgba(&pixels,pixels.data(),32,32,0,0,64,48);
        check(d.present(error),"present fullscreen window");
        check(SDL_SetWindowFullscreen(window,0)==0,"leave fullscreen");
        SDL_RestoreWindow(window);SDL_SetWindowSize(window,64,48);SDL_PumpEvents();
        // The PC menu flush preserves zero framebuffer alpha and writes RGB.
        // Presentation must still show its pixels, like D3D9 Present.
        auto menu=pixels;for(std::size_t i=3;i<menu.size();i+=4)menu[i]=0;
        d.clear();d.rgba(&menu,menu.data(),32,32,0,0,64,48,true);
        std::vector<std::uint8_t> output;check(d.read_pixels(output,error),"read composed menu");
        check(!output.empty()&&output[0]==255&&output[1]==255&&output[2]==255,"RGB menu visible with zero alpha");
        d.clear();d.rgba(&menu,menu.data(),32,32,0,0,64,48);
        check(d.read_pixels(output,error),"read transparent overlay");
        check(output[0]==0&&output[1]==0&&output[2]==0,"overlay transparency retained");
    }
    SDL_Quit();std::printf("macOS SDL Metal boundaries: %u checks passed\n",checks);
}
