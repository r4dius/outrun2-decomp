#define SDL_MAIN_HANDLED
#include <SDL.h>
#include "sdl_backend.hpp"
#include "platform/pc_dds.hpp"
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <stdexcept>

namespace outrun::ps5_runtime {
namespace {
void require_sdl(bool ok){if(!ok)throw std::runtime_error(SDL_GetError());}
}
SdlDisplay::~SdlDisplay(){
    for(auto& [_,image]:textures_)SDL_DestroyTexture(image.texture);
    if(renderer_)SDL_DestroyRenderer(renderer_);
    if(window_)SDL_DestroyWindow(window_);
}
bool SdlDisplay::open(unsigned w,unsigned h,std::string& error){
    width_=w;height_=h;
#ifdef OR2_PS5_PAYLOAD
    // The homebrew VideoOut backend exposes a 1920x1080 display.
    const int window_width=1920,window_height=1080;
#else
    const int window_width=int(w),window_height=int(h);
#endif
    window_=SDL_CreateWindow("OutRun 2006 PS5",SDL_WINDOWPOS_CENTERED,SDL_WINDOWPOS_CENTERED,window_width,window_height,SDL_WINDOW_SHOWN);
    if(!window_){error=SDL_GetError();return false;}
    renderer_=SDL_CreateRenderer(window_,-1,SDL_RENDERER_SOFTWARE);
    if(!renderer_){error=SDL_GetError();return false;}
    if(SDL_RenderSetLogicalSize(renderer_,int(w),int(h))<0){error=SDL_GetError();return false;}
    SDL_SetRenderDrawBlendMode(renderer_,SDL_BLENDMODE_BLEND);
    std::fprintf(stdout,"PS5 display: SDL driver=%s, software D3D9 + SDL composition\n",SDL_GetCurrentVideoDriver());
    return true;
}
void SdlDisplay::clear(){require_sdl(SDL_SetRenderDrawColor(renderer_,0,0,0,255)==0);require_sdl(SDL_RenderClear(renderer_)==0);}
SDL_Texture* SdlDisplay::upload(const void* key,const std::uint8_t* pixels,unsigned w,unsigned h,bool update){
    if(!pixels||!w||!h)throw std::runtime_error("invalid display texture");
    auto& image=textures_[key];bool fresh=false;
    if(!image.texture||image.width!=w||image.height!=h){
        if(image.texture)SDL_DestroyTexture(image.texture);
        image={SDL_CreateTexture(renderer_,SDL_PIXELFORMAT_RGBA32,SDL_TEXTUREACCESS_STREAMING,int(w),int(h)),w,h};
        require_sdl(image.texture!=nullptr);
        require_sdl(SDL_SetTextureBlendMode(image.texture,SDL_BLENDMODE_BLEND)==0);
        SDL_SetTextureScaleMode(image.texture,SDL_ScaleModeLinear);fresh=true;
    }
    if(fresh||update)require_sdl(SDL_UpdateTexture(image.texture,nullptr,pixels,int(w)*4)==0);
    return image.texture;
}
SDL_Texture* SdlDisplay::texels(const void* key,platform::MeshPreviewTextureFormat f,unsigned w,unsigned h,const std::vector<std::uint8_t>& bytes){
    auto i=textures_.find(key);if(i!=textures_.end())return i->second.texture;
    platform::PcDdsTexture texture{};texture.width=w;texture.height=h;texture.levels=1;
    texture.format=f==platform::MeshPreviewTextureFormat::bc1?platform::PcDdsTexture::Format::bc1:
        f==platform::MeshPreviewTextureFormat::bc2?platform::PcDdsTexture::Format::bc2:
        f==platform::MeshPreviewTextureFormat::bc3?platform::PcDdsTexture::Format::bc3:platform::PcDdsTexture::Format::rgba8;
    texture.faces.resize(1);texture.faces[0].push_back({w,h,bytes});
    auto rgba=platform::pc_dds_rgba(texture,0,0);
    if(rgba.size()!=std::size_t(w)*h*4)throw std::runtime_error("DDS decode size mismatch");
    return upload(key,rgba.data(),w,h,false);
}
void SdlDisplay::quad(const std::array<platform::MeshPreviewVertex,4>& v,SDL_Texture* texture,unsigned,unsigned){
    SDL_Vertex vertices[4];
    for(unsigned k=0;k<4;++k){
        vertices[k].position={(v[k].position[0]+1)*0.5f*float(width_),(1-v[k].position[1])*0.5f*float(height_)};
        auto channel=[&](unsigned c){return Uint8(std::clamp(v[k].color[c],0.0f,1.0f)*255.0f+0.5f);};
        vertices[k].color={channel(0),channel(1),channel(2),channel(3)};
        vertices[k].tex_coord={std::clamp(v[k].uv[0],0.0f,1.0f),std::clamp(v[k].uv[1],0.0f,1.0f)};
    }
    const int indices[6]{0,1,2,2,3,0};
    require_sdl(SDL_RenderGeometry(renderer_,texture,vertices,4,indices,6)==0);
}
void SdlDisplay::rgba(const void* key,const std::uint8_t* bytes,unsigned w,unsigned h,int x,int y,int width,int height){
    auto* texture=upload(key,bytes,w,h,true);const SDL_Rect rect{x,y,width,height};
    require_sdl(SDL_RenderCopy(renderer_,texture,nullptr,&rect)==0);
}
bool SdlDisplay::present(std::string& error){
#ifdef OR2_PS5_HOST_TEST
    // Capture the composed image, including the native 3D and frontend layers.
    if(const char* dir=std::getenv("OR2_PS5_SCREENSHOT_DIR")){
        static unsigned frame=0;
        std::filesystem::create_directories(dir);
        auto* surface=SDL_CreateRGBSurfaceWithFormat(0,int(width_),int(height_),32,SDL_PIXELFORMAT_RGBA32);
        if(!surface){error=SDL_GetError();return false;}
        char name[48];std::snprintf(name,sizeof name,"/frame-%04u.bmp",++frame);
        const bool ok=SDL_RenderReadPixels(renderer_,nullptr,surface->format->format,surface->pixels,surface->pitch)==0&&
            SDL_SaveBMP(surface,(std::string(dir)+name).c_str())==0;
        SDL_FreeSurface(surface);if(!ok){error=SDL_GetError();return false;}
    }
#endif
    SDL_ClearError();SDL_RenderPresent(renderer_);
    if(*SDL_GetError()){error=SDL_GetError();return false;}
    return true;
}
}
