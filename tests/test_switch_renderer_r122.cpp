#include "switch_renderer.hpp"
#include <array>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

namespace {
unsigned checks{};
void require(bool value,const char* message){++checks;if(!value){std::fprintf(stderr,"FAIL: %s\n",message);std::exit(1);}}
std::vector<std::uint8_t> read_file(const char* path){
    std::vector<std::uint8_t> bytes;
    if(std::FILE* f=std::fopen(path,"rb")){int c;while((c=std::fgetc(f))!=EOF)bytes.push_back(std::uint8_t(c));std::fclose(f);}
    return bytes;
}
}

int main(int argc,char** argv){
    if(argc!=2&&argc!=3){std::fprintf(stderr,"usage: test_switch_renderer_r122 FRONTEND [START_LOADING]\n");return 2;}
    outrun::switch_runtime::SwitchRenderer renderer{};std::string error;
    const auto loading=argc==3?read_file(argv[2]):std::vector<std::uint8_t>{};
    require(outrun::switch_runtime::switch_renderer_initialize(renderer,argv[1],error,argc==3?&loading:nullptr,nullptr,nullptr,nullptr),error.c_str());
    if(argc==3){
        const auto stats=outrun::switch_runtime::switch_renderer_stats(renderer);
        require(stats.loading_scenes==9u&&stats.loading_textures==5u&&stats.loading_draws==171u,"retail START loading assets resident");
        require(outrun::switch_runtime::switch_renderer_set_start_loading_scene(renderer,8u),"select last authored START scene");
        require(!outrun::switch_runtime::switch_renderer_set_start_loading_scene(renderer,9u),"reject non-authored START scene");
        require(outrun::switch_runtime::switch_renderer_set_start_loading_scene(renderer,0u),"restore PC loading scene byte zero");
        require(outrun::switch_runtime::switch_renderer_set_start_loading_visible(renderer,true),"show START loading scene");
        require(outrun::switch_runtime::switch_renderer_draw(renderer),"START loading draw");
        require(outrun::switch_runtime::switch_renderer_stats(renderer).loading_frames==1u,"START loading drawn");
        require(outrun::switch_runtime::switch_renderer_set_start_loading_visible(renderer,false),"hide START loading");
    }
    using namespace outrun::switch_runtime;
    const auto before=switch_renderer_stats(renderer);
    require(switch_renderer_set_frontend_visible(renderer,false),"bootstrap has no frontend yet");
    require(switch_renderer_draw(renderer),"bootstrap clear: the PC scene only");
    const auto bootstrap=switch_renderer_stats(renderer);
    require(bootstrap.unowned_frames==before.unowned_frames+1u,"the PC scene owns the frame without a frontend");
    const std::array<std::uint8_t,16> rgba{};
    require(!switch_renderer_set_movie_frame(renderer,rgba.data(),0u,2u),"invalid movie frame rejected");
    require(switch_renderer_set_frontend_visible(renderer,true),"welcome frontend owns output");
    require(switch_renderer_set_movie_frame(renderer,rgba.data(),2u,2u),"decoded movie accepted");
    require(switch_renderer_draw(renderer),"movie plus frontend frame");
    require(switch_renderer_stats(renderer).movie_frames==before.movie_frames+1u,"movie displayed once");
    require(switch_renderer_set_movie_frame(renderer,nullptr,0u,0u),"original stop command hides movie");
    require(switch_renderer_draw(renderer),"menu after movie release");
    require(switch_renderer_stats(renderer).movie_frames==before.movie_frames+1u,"stopped video not redrawn");
    outrun::platform::FrontendSprites sprites;
    if(switch_renderer_stats(renderer).frontend_scenes==224u){
        require(switch_renderer_attach_frontend_sprites(renderer,sprites),"bind complete authored timing into controller instances");
        const auto first=sprites.create(0x440073u,5,1,0,51,true);
        const auto second=sprites.create(0x440073u,5,1,51,0,true);
        const auto label=sprites.create(0x440081u,4,4,0,0,true);
        require(first!=~0u&&second!=~0u&&label!=~0u,"independent instances of one scene and another layer");
        sprites.set_speed(second,-1.0f);sprites.tick();
        require(switch_renderer_draw(renderer),"multiple authored instances composed");
        auto composed=switch_renderer_stats(renderer);
        require(composed.frontend_instances==3u&&composed.frontend_instance_draws>0u,"all instances reach the draw path");
        require(!sprites.get(label)->allocated,"one-draw mode released after composition");
        require(sprites.get(first)->frame!=sprites.get(second)->frame,"same token keeps independent cursors");
        sprites.release(first);sprites.release(second);
        require(switch_renderer_draw(renderer),"empty instance pool is legal");
        require(switch_renderer_stats(renderer).frontend_instances==0u,"released sprites do not fall back to a diagnostic menu");
    }
    outrun::switch_runtime::switch_renderer_shutdown(renderer);
    std::printf("switch_renderer_r122: %u checks passed\n",checks);return 0;
}
