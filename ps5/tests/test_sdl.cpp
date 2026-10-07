#define SDL_MAIN_HANDLED
#include <SDL.h>
#include "audio.hpp"
#include "sdl_backend.hpp"
#include <cstdio>
#include <cstdlib>
static unsigned checks=0;
static void check(bool v,const char* m){++checks;if(!v){std::fprintf(stderr,"FAIL: %s (%s)\n",m,SDL_GetError());std::exit(1);}}
int main(){
    SDL_SetMainReady();check(SDL_Init(SDL_INIT_AUDIO|SDL_INIT_VIDEO)==0,"SDL init");
    std::string error;
    {
        outrun::ps5_runtime::Ps5Audio a;
        for(unsigned cycle=0;cycle<20;++cycle){
            check(a.open(error),"open stereo output");
            check(a.submit(std::vector<std::int16_t>(2048,100),error),"submit PCM");
            check(!a.submit(std::vector<std::int16_t>(3),error),"odd stereo rejected");
            check(!a.submit(std::vector<std::int16_t>(192002),error),"queue bound enforced");
            a.close();check(a.queued_frames()==0,"closed queue cleared");
            check(!a.submit(std::vector<std::int16_t>(2),error),"closed output rejects PCM");
        }
    }
    for(unsigned cycle=0;cycle<5;++cycle){
        outrun::ps5_runtime::SdlDisplay d;check(d.open(64,48,error),"display lifecycle");d.clear();
        const std::vector<std::uint8_t> pixels(32u*32*4,255);
        d.rgba(&pixels,pixels.data(),32,32,0,0,64,48);check(d.present(error),"real software present");
    }
    SDL_Quit();std::printf("PS5 SDL boundaries: %u checks passed\n",checks);
}
