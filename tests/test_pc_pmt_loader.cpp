// Host checks of the native PMT loader and material shader setup on a
// synthetic PMT. The retail-data comparison against the original code is the
// pmt_loader_probe oracle.
#include "platform/pc_pmt_loader.hpp"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>
using namespace outrun::platform;
using outrun::driving::Bytes;
static unsigned checks;
#define CHECK(x) do{++checks;if(!(x)){std::fprintf(stderr,"pmt loader line %d: %s\n",__LINE__,#x);std::exit(1);}}while(0)
namespace {
struct Device final:PcD3D9Device{
    std::vector<std::vector<std::uint8_t>> buffers;std::vector<std::uint32_t> kinds;std::vector<std::uint32_t> calls;
    std::vector<PcTextureFileRequest> textures;
    std::uint32_t add(std::uint32_t kind){kinds.push_back(kind);buffers.emplace_back();return std::uint32_t(kinds.size());}
    std::uint32_t create_vertex_buffer(std::uint32_t n,std::uint32_t,std::uint32_t,std::uint32_t)override{
        calls.push_back(26);const auto h=add(26);buffers[h-1].resize(n);return h;}
    std::uint32_t create_index_buffer(std::uint32_t n,std::uint32_t,std::uint32_t f,std::uint32_t)override{
        calls.push_back(27);CHECK(f==0x65);const auto h=add(27);buffers[h-1].resize(n);return h;}
    std::uint8_t* lock(std::uint32_t h,std::uint32_t o,std::uint32_t,std::uint32_t)override{calls.push_back(0x100b);return buffers.at(h-1).data()+o;}
    void unlock(std::uint32_t)override{calls.push_back(0x100c);}
    std::uint32_t create_vertex_declaration(const PcVertexElement*)override{calls.push_back(86);return add(86);}
    std::uint32_t create_vertex_shader(const std::uint32_t* t)override{calls.push_back(91);CHECK(t[0]==0xfffe0101u);return add(91);}
    void set_vertex_shader_constant_f(std::uint32_t,const float*,std::uint32_t)override{calls.push_back(94);}
    std::uint32_t create_texture_from_file(const PcTextureFileRequest& q)override{calls.push_back(0x5001);textures.push_back(q);return add(0x5001);}
    void release(std::uint32_t)override{calls.push_back(2);}
    std::uint32_t get_render_state(std::uint32_t)override{return 0;}
    void set_render_state(std::uint32_t,std::uint32_t)override{}
    std::uint32_t get_texture(std::uint32_t)override{return 0;}
    void set_texture(std::uint32_t,std::uint32_t)override{}
    std::uint32_t get_texture_stage_state(std::uint32_t,std::uint32_t)override{return 0;}
    void set_texture_stage_state(std::uint32_t,std::uint32_t,std::uint32_t)override{}
    std::uint32_t get_sampler_state(std::uint32_t,std::uint32_t)override{return 0;}
    void set_sampler_state(std::uint32_t,std::uint32_t,std::uint32_t)override{}
    std::uint32_t draw_indexed_primitive(std::uint32_t,std::int32_t,std::uint32_t,std::uint32_t,std::uint32_t,std::uint32_t)override{return 0;}
    void set_vertex_declaration(std::uint32_t)override{}
    void set_vertex_shader(std::uint32_t)override{}
    void set_stream_source(std::uint32_t,std::uint32_t,std::uint32_t,std::uint32_t)override{}
    void set_indices(std::uint32_t)override{}
    std::uint32_t create_pixel_shader(const std::uint32_t*)override{return add(106);}
    void set_pixel_shader(std::uint32_t)override{}
    void set_pixel_shader_constant_f(std::uint32_t,const float*,std::uint32_t)override{}
    std::uint32_t get_declaration(std::uint32_t,PcVertexElement*)override{return 0;}
};
}
int main(){
    PcVertexDeclaration d{};
    CHECK(d3dx_declarator_from_fvf(0x152,d)); // XYZ | NORMAL | DIFFUSE | TEX1
    CHECK(d[0].type==2&&d[0].usage==0&&d[0].offset==0);
    CHECK(d[1].type==2&&d[1].usage==3&&d[1].offset==12);
    CHECK(d[2].type==4&&d[2].usage==10&&d[2].offset==24);
    CHECK(d[3].type==1&&d[3].usage==5&&d[3].offset==28&&d[3].usage_index==0);
    CHECK(d[4].stream==PcDeclEndStream&&d[4].type==17);
    CHECK(!d3dx_declarator_from_fvf(0x1,d));
    type2_declaration_4101c0(0x152,d);
    CHECK(d[4].stream==1&&d[4].offset==0&&d[4].type==2&&d[4].usage==0&&d[4].usage_index==1);
    CHECK(d[5].stream==1&&d[5].offset==12&&d[5].usage==3&&d[5].usage_index==1);
    CHECK(d[6].stream==1&&d[6].offset==24&&d[6].type==4&&d[6].usage==10);
    CHECK(d[7].stream==1&&d[7].offset==28&&d[7].type==1&&d[7].usage==5&&d[7].usage_index==4);
    CHECK(d[8].stream==PcDeclEndStream&&d[8].type==0x11);
    const auto shader=link_vertex_shader_40e140(0);
    CHECK(shader.front()==0xfffe0101u&&shader.back()==0xffffu);
    // Shader cache: identical requests share one shader; key3 separates entries.
    {
        Device dev;PcShaderCache cache{};std::uint32_t decl=0;
        CHECK(d3dx_declarator_from_fvf(0x112,d));
        const auto a=shader_cache_40f4e0(cache,dev,1,d.data(),shader.data(),0,decl);
        const auto b=shader_cache_40f4e0(cache,dev,1,d.data(),shader.data(),0,decl);
        const auto c=shader_cache_40f4e0(cache,dev,1,d.data(),shader.data(),5,decl);
        CHECK(a==b&&a!=c&&cache.count==2&&cache.high_water==2&&cache.entries[0].references==2);
        shader_cache_release_40f650(cache,dev);
        CHECK(cache.entries[0].shader==0&&cache.entries[1].declaration==0);
    }
    // Synthetic PMT: one object, one group (1 VB, 1 IB), one material, one texture.
    PcPmtResources r{};r.system.assign(0x1b0,0);Bytes s(r.system.data(),r.system.size());
    std::vector<std::uint8_t> video(0xa0,0);for(unsigned k=0;k<12;++k)video[k]=std::uint8_t(0x40+k);
    r.video=video.data();r.video_size=video.size();
    s.put32(4,1);s.put32(8,1);
    const std::uint32_t rec=0x18;
    s.put32(rec+0x04,0x100);s.put32(rec+0x08,0xe0);s.put32(rec+0x0c,0x130);s.put32(rec+0x18,0x80);
    s.put32(rec+0x30,0xb0);s.put32(rec+0x34,0x150);
    s.put32(0x80+0x24,1);s.put32(0x80+0x2c,1);
    s.put32(0xb0,1);s.put32(0xb0+0x18,6);s.put32(0xb0+0x1c,12);s.put32(0xb0+0x20,0x2);s.put32(0xb0+0x28,0);
    s.put32(0xe0,0xf0);s.put32(0xf0+4,0);
    s.put32(0x100,0x110);s.put32(0x110+4,0x120);for(unsigned k=0;k<6;++k)s.put8(0x120+k,std::uint8_t(k+1));
    for(unsigned l=0;l<3;++l)s.puti(0x150+8+l*0x14+0x10,-1);
    s.put32(0x68+4,0x20);s.put32(0x68+0x10,0);
    Device dev;PcShaderCache cache{};PcShaderGlobals globals{};PcPmtLoadCursor cur{};
    pmt_objects_begin_42e490(r,cur);
    CHECK(s.u32(0x14)==0x18&&r.object_count_0c==1);
    CHECK(pmt_objects_step_42e4f0(r,cur,dev,cache,globals));
    CHECK(cur.object_95b230==1&&cur.offset_95b234==0x54);
    pmt_textures_begin_42e850(r,cur);
    CHECK(r.texture_table_24==0x54&&s.u32(0x54)==0x58&&r.texture_records_04==0x68);
    CHECK(pmt_textures_step_42e8c0(r,cur,dev));
    const std::vector<std::uint32_t> expected{26,0x100b,0x100c,27,0x100b,0x100c,86,91,0x5001};
    CHECK(dev.calls==expected);
    CHECK(s.u32(0xe0)==1&&s.u32(0x100)==2);
    CHECK(dev.buffers[0][0]==0x40&&dev.buffers[0][11]==0x4b&&dev.buffers[1][0]==1&&dev.buffers[1][5]==6);
    CHECK(s.u32(0x130)==4&&s.u32(0x130+4)==3&&s.u32(0x130+8)==1);
    CHECK(s.u32(0x58)==5);
    CHECK(dev.textures[0].data==video.data()+0x20&&dev.textures[0].size==0x80&&!dev.textures[0].cube&&dev.textures[0].mip_levels==4);
    std::printf("pc pmt loader: %u checks\n",checks);
    return 0;
}
