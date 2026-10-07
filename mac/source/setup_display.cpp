#include "setup_display.hpp"
#include <SDL.h>
namespace outrun::mac_runtime {
void SetupDisplay::show(const platform::SetupScreen& screen){
    if(!window_){
        window_=SDL_CreateWindow("OutRun 2006 Coast 2 Coast",SDL_WINDOWPOS_CENTERED,SDL_WINDOWPOS_CENTERED,
                                 screen.width(),screen.height(),SDL_WINDOW_ALLOW_HIGHDPI);
        if(!window_)return;
        renderer_=SDL_CreateRenderer(window_,-1,SDL_RENDERER_ACCELERATED|SDL_RENDERER_PRESENTVSYNC);
        if(!renderer_)renderer_=SDL_CreateRenderer(window_,-1,0);
        if(renderer_){
            SDL_RenderSetLogicalSize(renderer_,screen.width(),screen.height());
            texture_=SDL_CreateTexture(renderer_,SDL_PIXELFORMAT_RGBA32,SDL_TEXTUREACCESS_STREAMING,screen.width(),screen.height());
        }
    }
    pump();
    if(!texture_)return;
    SDL_UpdateTexture(texture_,nullptr,screen.pixels(),screen.width()*4);
    SDL_RenderClear(renderer_);SDL_RenderCopy(renderer_,texture_,nullptr,nullptr);SDL_RenderPresent(renderer_);
}
bool SetupDisplay::pump(){
    SDL_Event e;
    while(SDL_PollEvent(&e))
        if(e.type==SDL_QUIT||(e.type==SDL_WINDOWEVENT&&e.window.event==SDL_WINDOWEVENT_CLOSE))quit_=true;
    return !quit_;
}
void SetupDisplay::wait_for_close(){
    while(!quit_){
        SDL_Event e;
        if(!SDL_WaitEventTimeout(&e,100))continue;
        if(e.type==SDL_QUIT||(e.type==SDL_WINDOWEVENT&&e.window.event==SDL_WINDOWEVENT_CLOSE)||
           e.type==SDL_KEYDOWN||e.type==SDL_CONTROLLERBUTTONDOWN)quit_=true;
    }
}
void SetupDisplay::close(){
    if(texture_)SDL_DestroyTexture(texture_);
    if(renderer_)SDL_DestroyRenderer(renderer_);
    if(window_)SDL_DestroyWindow(window_);
    texture_=nullptr;renderer_=nullptr;window_=nullptr;
}
platform::SetupPlatform SetupDisplay::platform(){
    platform::SetupPlatform p;
    p.present=[this](const platform::SetupScreen& s){show(s);};
    p.keep_running=[this]{return pump();};
    p.wait_for_exit=[this]{wait_for_close();};
    p.exit_hint="Press any key or close this window to quit.";
    return p;
}
}
