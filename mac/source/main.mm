#include "metal_display.hpp"
#include "mac_paths.hpp"
#include "platform/retail_asset_store.hpp"
#include "platform/pc_soft_d3d9.hpp"
#ifdef OR2_MAC_SDL
#include "sdl_services.hpp"
#endif
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <thread>
#include <chrono>
int main(int argc,char** argv){
    // Diagnostic application until the recovered runtime services are wired.
    bool diagnostic=false;std::string root;unsigned frame_limit=0;
    for(int i=1;i<argc;++i){
        if(std::strcmp(argv[i],"--metal-check")==0)diagnostic=true;
        else if(std::strcmp(argv[i],"--assets")==0&&i+1<argc)root=argv[++i];
        else if(std::strcmp(argv[i],"--frames")==0&&i+1<argc){
            char* end=nullptr;const long value=std::strtol(argv[++i],&end,10);
            if(!end||*end||value<1||value>10000)return 2;
            frame_limit=unsigned(value);
        }
        else {std::fprintf(stderr,"usage: OutRunMac --metal-check | --assets <PC installation>\n");return 2;}
    }
    std::string error;
    if(!diagnostic){
        outrun::platform::RetailAssetStore assets;
        if(root.empty()||!outrun::platform::retail_asset_store_open(assets,root,&error)){
            std::fprintf(stderr,"PC installation required: %s\n",error.c_str());return 2;
        }
        std::fprintf(stderr,"Retail directory validated. macOS game runtime orchestration and hardware scene renderer are not yet connected.\n");
        return 3;
    }
    outrun::mac::MetalDisplay display;
    if(!display.open(960,720,true,error)){std::fprintf(stderr,"%s\n",error.c_str());return 1;}
#ifdef OR2_MAC_SDL
    outrun::mac::SdlServices media;
    if(!media.open(error,display.native_window())){std::fprintf(stderr,"SDL: %s\n",error.c_str());return 1;}
#endif
    outrun::platform::PcSoftD3D9Device scene(640,480);
    scene.clear(0xff203040u,1.f);
    struct Vertex {float x,y,z,rhw;std::uint32_t colour;};
    const Vertex triangle[]={{100,380,0,1,0xffff0000u},{320,80,0,1,0xff00ff00u},{540,380,0,1,0xff0000ffu}};
    scene.set_fvf(0x44u);scene.set_render_state(22u,1u);
    scene.draw_primitive_up(4u,1u,triangle,sizeof(Vertex));
    if(!scene.pixels_shaded){std::fprintf(stderr,"Reference scene produced no pixels\n");return 1;}
    std::printf("Metal presentation on %s; scene rasterised by portable CPU reference. Escape closes window.\n",display.device_name().c_str());
    unsigned frames=0;
    while(display.running()){
#ifdef OR2_MAC_SDL
        if(media.sample(0).exit_requested)break;
#endif
        if(!display.present(scene.rgba().data(),scene.width(),scene.height(),error)){std::fprintf(stderr,"%s\n",error.c_str());return 1;}
        if(frame_limit&&++frames>=frame_limit)break;
        std::this_thread::sleep_for(std::chrono::milliseconds(16));
    }
    return 0;
}
