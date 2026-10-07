#include "sdl_services.hpp"
#define SDL_MAIN_HANDLED
#include <SDL.h>
#include <cstdio>
int main(){
    std::string error;outrun::mac::SdlServices services;
    if(services.submit({0,0},error))return 1;
    if(!services.open(error)){std::fprintf(stderr,"%s\n",error.c_str());return 1;}
    if(!services.submit(std::vector<std::int16_t>(960,0),error))return 1;
    if(services.submit({0},error))return 1;
    if(services.submit(std::vector<std::int16_t>(192002,0),error))return 1;
    auto input=services.sample(16);
    if(input.accelerator||input.brake||input.steering||input.shift_up||input.shift_down||input.exit_requested)return 1;
    SDL_Event quit{};quit.type=SDL_QUIT;SDL_PushEvent(&quit);
    if(!services.sample(16).exit_requested)return 1;
    std::puts("SDL stereo queue validation, neutral input and quit verified (dummy audio)");return 0;
}
