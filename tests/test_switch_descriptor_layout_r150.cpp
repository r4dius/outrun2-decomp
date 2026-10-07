#include "switch_renderer.hpp"
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
    if(argc!=3){std::fprintf(stderr,"usage: test_switch_descriptor_layout_r150 FRONTEND LOADING\n");return 2;}
    outrun::switch_runtime::SwitchRenderer renderer{};std::string error;
    const auto loading=read_file(argv[2]);
    require(outrun::switch_runtime::switch_renderer_initialize(renderer,argv[1],error,&loading,nullptr,nullptr,nullptr),error.c_str());
    const auto stats=outrun::switch_runtime::switch_renderer_stats(renderer);
    require(stats.frontend_textures==82u,"primary frontend textures");
    require(stats.loading_textures==5u,"START loading textures");
    require(stats.resident_images==88u,"white + frontend + START loading images");
    require(stats.frontend_descriptors==88u,"frontend/loading descriptor set");
    require(stats.frontend_descriptors<=256u,"GPU API descriptor bound respected");
    outrun::switch_runtime::switch_renderer_shutdown(renderer);
    std::printf("switch_descriptor_layout_r150: %u checks passed; resident=%u frontend=%u\n",checks,stats.resident_images,stats.frontend_descriptors);
    return 0;
}
