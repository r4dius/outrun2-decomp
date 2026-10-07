// Host census of the vertex shader programs the retail PMT banks create:
// every *_pmt.sz under the retail data directory (or the files given) is
// inflated and loaded with the native loader (42E490/42E4F0 objects, the
// 40F4E0 shader cache) into a software D3D9 device, and the blob list of
// every created vertex shader (at load, 40FD70, and through 4103F0 for every
// object and shader kind) is printed by the software device's OR2_BLOB_DEBUG
// trace ("[vs] h=N blobs=K: b0 b1 ..."): sort -u them into
// tools/pc_spec_programs.txt lines for tools/generate_pc_spec_shaders.py.
//
// Usage: pc_shader_census <retail data dir> [pmt.sz ...]
#include "platform/pc_pmt_loader.hpp"
#include "platform/pc_render_flush.hpp"
#include "platform/pc_soft_d3d9.hpp"
#include "platform/pc_vertex_shader_setup.hpp"
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <cstdlib>
#include <vector>
#include <zlib.h>
using namespace outrun;
using namespace outrun::platform;
using driving::Bytes;
namespace {
bool inflate(const std::string& path,std::vector<std::uint8_t>& out){
    std::ifstream in(path,std::ios::binary);if(!in)return false;
    std::vector<std::uint8_t> packed((std::istreambuf_iterator<char>(in)),std::istreambuf_iterator<char>());
    out.assign(64u*1024u*1024u,0);uLongf size=uLongf(out.size());
    if(uncompress(out.data(),&size,packed.data(),uLong(packed.size()))!=Z_OK)return false;
    out.resize(size);return true;
}
}
int main(int argc,char** argv){
    if(argc<2){std::fprintf(stderr,"usage: pc_shader_census <retail data dir> [pmt.sz ...]\n");return 2;}
    std::vector<std::string> files;
    for(int i=2;i<argc;++i)files.push_back(argv[i]);
    if(files.empty())for(const auto& e:std::filesystem::recursive_directory_iterator(argv[1])){
        const auto name=e.path().filename().string();
        if(e.is_regular_file()&&name.size()>7&&name.compare(name.size()-7,7,"_pmt.sz")==0)files.push_back(e.path().string());
    }
    setenv("OR2_BLOB_DEBUG","1",1);
    unsigned loaded=0,failed=0,kinds=0;
    {   // Programs created outside the banks: the shadow volume VS (41786C,
        // 623E38 -> 955A40) and the particles PS (623830 -> 95AF9C).
        PcSoftD3D9Device dev(640,480);
        const auto vs=pc_shader_tokens(0x623e38u);(void)dev.create_vertex_shader(vs.data());
        const auto ps=pc_shader_tokens(0x623830u);(void)dev.create_pixel_shader(ps.data());
    }
    for(const auto& f:files){
        PcSoftD3D9Device dev(640,480);                         // per bank: its textures are freed with it
        std::vector<std::uint8_t> pmt;
        if(!inflate(f,pmt)||pmt.size()<16){++failed;std::fprintf(stderr,"skip %s (inflate)\n",f.c_str());continue;}
        try{
            Bytes pb(pmt.data(),pmt.size());const auto ss=pb.u32(8),vs=pb.u32(12);
            if(std::uint64_t(ss)+vs+16u>pmt.size())throw std::runtime_error("sections");
            PcPmtResources r;r.system.assign(pmt.begin()+16,pmt.begin()+16+ss);r.video=pmt.data()+16+ss;r.video_size=vs;
            PcShaderCache cache{};PcShaderGlobals globals{};PcPmtLoadCursor cur{};
            pmt_objects_begin_42e490(r,cur);while(!pmt_objects_step_42e4f0(r,cur,dev,cache,globals)){}
            ++loaded;
            for(std::uint32_t i=0;i<r.object_count_0c;++i)for(std::uint32_t kind=0;kind<14u;++kind){
                try{pmt_object_shaders_4103f0(r,i,kind,dev,cache,globals);++kinds;}catch(const std::exception&){}
            }
            // Pixel programs: every material through 408C80 / 40B200.
            pmt_textures_begin_42e850(r,cur);while(!pmt_textures_step_42e8c0(r,cur,dev)){}
            PcRenderGlobals g;
            PcFlushContext fc{dev,g,[&](std::uint32_t res)->PcPmtResources*{return res==1u?&r:nullptr;},pc_colour_list_alt_408c80,1};
            render_state_init_40f6b0(fc);render_pixel_state_init_40ac60(fc);
            render_material_census(fc,1u);
        }catch(const std::exception& e){++failed;std::fprintf(stderr,"skip %s (%s)\n",f.c_str(),e.what());}
    }
    std::fprintf(stderr,"banks loaded=%u failed=%u object kinds=%u\n",loaded,failed,kinds);
    return 0;
}
