#include "vk_display.hpp"
#include "vk_d3d9.hpp"
#include "system/exe_image.hpp"
#include "platform/pc_vertex_shader_setup.hpp"
#include "pc_vk_shaders.hpp"
#include <cstdio>
#include <cstdlib>
#include <cmath>
#include <cstring>
#include <chrono>
#include <string>
using namespace outrun::ps5_runtime;
using namespace outrun::platform;
using namespace outrun::platform::d3d9;
namespace outrun::ps5_runtime {
struct VulkanVertexTestAccess {
    static bool native(VulkanD3D9Device& d,unsigned type,int base,unsigned start,unsigned count){
        const auto result=d.prepare_native(type,base,start,count);d.native_vertices_=false;return result;
    }
    static std::uint32_t draw(VulkanD3D9Device& d,unsigned type,int base,unsigned start,unsigned count,std::vector<float>& out){
        d.batch_.clear();const auto result=d.prepare_indexed(type,base,0,0,start,count);
        out.resize(d.batch_.size()*sizeof(VulkanD3D9Device::Vertex)/sizeof(float));
        if(!out.empty())std::memcpy(out.data(),d.batch_.data(),out.size()*sizeof(float));
        d.batch_.clear();return result;
    }
};
}
// Capture the unmodified common CPU shader/clipping output without rasterizing.
// Convert its fan into the same half-pixel-adjusted triangle list as Vulkan.
class ReferenceVertices:public PcSoftD3D9Device {
public:
    ReferenceVertices():PcSoftD3D9Device(64,48){}
    std::vector<float> captured;
private:
    void raster(const Vertex* vertices,unsigned count)override{
        auto push=[&](const Vertex& source){auto v=source;v.pos[0]+=v.pos[3]/soft_viewport_.w;v.pos[1]-=v.pos[3]/soft_viewport_.h;
            const auto at=captured.size();captured.resize(at+sizeof(Vertex)/sizeof(float));std::memcpy(captured.data()+at,&v,sizeof v);};
        for(unsigned i=1;i+1<count;++i){push(vertices[0]);push(vertices[i]);push(vertices[i+1]);}
    }
};
static unsigned checks=0;
static void check(bool ok,const char* message){++checks;if(!ok){std::fprintf(stderr,"FAIL: %s\n",message);std::exit(1);}}
struct V {float x,y,z,w;unsigned colour;};
static void quad(PcSoftD3D9Device& d,unsigned c,float z=0.5f){
    const V v[]{{4,3,z,1,c},{60,3,z,1,c},{4,45,z,1,c},{60,45,z,1,c}};
    d.set_fvf(0x44);d.draw_primitive_up(5,2,v,sizeof(V));
}
static void setup(PcSoftD3D9Device& d){
    d.set_render_state(RS_CULLMODE,CULL_NONE);
    d.set_texture_stage_state(0,1,2);d.set_texture_stage_state(0,2,0);
    d.set_texture_stage_state(0,4,2);d.set_texture_stage_state(0,5,0);
}
static void scene_quad(PcSoftD3D9Device& d,unsigned colour){
    struct G {float x,y,z;unsigned colour;};
    const G v[]{{-.8f,-.8f,.5f,colour},{.8f,-.8f,.5f,colour},{-.8f,.8f,.5f,colour},{.8f,.8f,.5f,colour}};
    d.set_render_state(RS_LIGHTING,0);d.set_fvf(0x42);d.draw_primitive_up(5,2,v,sizeof(G));
}
static void compare(VulkanD3D9Device& gpu,PcSoftD3D9Device& cpu,vulkan::Context& context){
    const auto pixels=context.readback(gpu.back_texture());
    for(unsigned y=5;y<43;++y)for(unsigned x=6;x<58;++x)for(unsigned c=0;c<4;++c){
        if(std::abs(int(pixels[(y*64+x)*4+c])-int(cpu.rgba()[(y*64+x)*4+c]))>1){
            std::fprintf(stderr,"pixel (%u,%u) channel %u: Vulkan %u CPU %u\n",x,y,c,pixels[(y*64+x)*4+c],cpu.rgba()[(y*64+x)*4+c]);
            check(false,"Vulkan differs from CPU at interior pixel");
        }++checks;
    }
    check(cpu.errors.empty()&&gpu.errors.empty(),"D3D errors absent");
}
static void compare_clipped(VulkanD3D9Device& gpu,PcSoftD3D9Device& cpu,vulkan::Context& context){
    const auto pixels=context.readback(gpu.back_texture());unsigned stable=0;
    for(unsigned y=2;y<46;++y)for(unsigned x=2;x<62;++x){
        const auto at=(y*64+x)*4;bool interior=true;
        for(int dy=-1;dy<=1;++dy)for(int dx=-1;dx<=1;++dx)if(std::memcmp(cpu.rgba().data()+at,cpu.rgba().data()+((int(y)+dy)*64+int(x)+dx)*4,4))interior=false;
        if(!interior)continue;++stable;
        for(unsigned c=0;c<4;++c)check(std::abs(int(pixels[at+c])-int(cpu.rgba()[at+c]))<=1,"GPU homogeneous clipping matches stable reference pixels");
    }check(stable>500,"Clip comparison includes substantial interior/background coverage");
}
static void indexed_cases(VulkanD3D9Device& gpu,vulkan::Context& context){
    PcSoftD3D9Device cpu(64,48);ReferenceVertices reference;std::vector<float> actual;
    struct Position {float x,y,z,w;unsigned bytes;float uv[2];};
    std::vector<Position> positions(65538);
    // Asymmetric coordinates keep pixel comparisons away from exact shared
    // edges, where CPU floating-point coverage and hardware subpixels differ.
    positions[1]={-.91f,-.87f,.5f,1,0x04030201,{.1f,.2f}};
    positions[2]={ .87f,-.87f,.5f,1,0x04030201,{.9f,.2f}};
    positions[3]={-.91f, .89f,.5f,1,0x04030201,{.1f,.8f}};
    positions[4]={ .87f, .89f,.5f,1,0x04030201,{.9f,.8f}};
    positions[65536]=positions[4];
    std::vector<unsigned> colours(positions.size(),0xff336699);
    const std::uint16_t indices[]{123,456,0,1,2,2,1,3,0,1,2,3,0,1,65535};
    PcSoftD3D9Device* devices[]{&gpu,&cpu,&reference};
    unsigned vb[3]{},colour[3]{},ib[3]{};
    const PcVertexElement elements[]{{0,0,3,0,0,0},{1,0,4,0,10,0},{0,16,5,0,2,0},{0,20,1,0,5,0},{PcDeclEndStream,0,17,0,0,0}};
    for(unsigned i=0;i<3;++i){auto& d=*devices[i];d.reset_state();d.set_fvf(0);setup(d);d.hash_draws=true;
        d.set_render_state(RS_TEXTUREFACTOR,0xff336699);d.set_texture_stage_state(0,2,3);d.set_texture_stage_state(0,5,3);
        vb[i]=d.create_vertex_buffer(unsigned(positions.size()*sizeof(Position)+8),0,0,0);
        std::memcpy(d.lock(vb[i],8,unsigned(positions.size()*sizeof(Position)),0),positions.data(),positions.size()*sizeof(Position));d.unlock(vb[i]);
        colour[i]=d.create_vertex_buffer(unsigned(colours.size()*4+4),0,0,0);
        std::memcpy(d.lock(colour[i],4,unsigned(colours.size()*4),0),colours.data(),colours.size()*4);d.unlock(colour[i]);
        ib[i]=d.create_index_buffer(sizeof indices,0,0,0);std::memcpy(d.lock(ib[i],0,sizeof indices,0),indices,sizeof indices);d.unlock(ib[i]);
        d.set_stream_source(0,vb[i],8,sizeof(Position));d.set_stream_source(1,colour[i],4,4);d.set_indices(ib[i]);
        d.set_vertex_declaration(d.create_vertex_declaration(elements));
        for(unsigned row=0;row<4;++row){float c[4]{};c[row]=1;d.set_vertex_shader_constant_f(64+row,c,1);}
        const float one[]{1,1,1,1};d.set_vertex_shader_constant_f(0,one,1);
    }
    auto capture=[&](unsigned type,int base,unsigned start,unsigned count){
        reference.captured.clear();const auto expected=reference.draw_indexed_primitive(type,base,0,0,start,count);
        check(VulkanVertexTestAccess::draw(gpu,type,base,start,count,actual)==expected,"Indexed return code matches reference");
        check(actual.size()==reference.captured.size(),"Clipped triangle list size matches reference");
        for(std::size_t i=0;i<actual.size();++i){const auto a=actual[i],b=reference.captured[i];
            check(a==b||(std::isnan(a)&&std::isnan(b))||std::abs(a-b)<=1e-6f*std::max(1.f,std::abs(b)),"Indexed attributes match reference");}
    };
    const auto native_before=gpu.native_vertex_draws;
    for(unsigned config:{8u,8u|(1u<<9),0xffffffffu}){
        const auto tokens=config==0xffffffffu?pc_shader_tokens(0x621e10):link_vertex_shader_40e140(config);unsigned shaders[3]{};
        for(auto* d:devices){d->set_texture_stage_state(0,2,config==0xffffffffu?0:3);d->set_texture_stage_state(0,5,config==0xffffffffu?0:3);
            const float one[]{1,1,1,1};d->set_vertex_shader_constant_f(8,one,1);}
        for(unsigned i=0;i<3;++i){shaders[i]=devices[i]->create_vertex_shader(tokens.data());check(shaders[i]!=0,"Linked recovered VS created");devices[i]->set_vertex_shader(shaders[i]);}
        capture(4,1,2,2);capture(5,1,8,2);capture(4,1,12,1);capture(4,0,2,2);
        // Reused indices must see new constants, streams and buffer contents.
        for(auto* d:devices){const float shift[]{1,0,0,.15f};d->set_vertex_shader_constant_f(64,shift,1);}capture(4,1,2,2);
        for(auto* d:devices){const float reset[]{1,0,0,0};d->set_vertex_shader_constant_f(64,reset,1);}
        for(float z:{-.6f,1.6f,.5f}){
            positions[1].z=z;
            for(unsigned i=0;i<3;++i){std::memcpy(devices[i]->lock(vb[i],8+sizeof(Position),sizeof(Position),0),&positions[1],sizeof(Position));devices[i]->unlock(vb[i]);}
            capture(4,1,2,2);capture(5,1,8,2);
            if(z!=.5f){
                for(PcSoftD3D9Device* d:{devices[0],devices[1]}){d->clear(7,0xff000000,1,0);check(d->draw_indexed_primitive(5,1,0,4,8,2)==0,"GPU depth clipping draw succeeds");}
                reference.count_only=true;reference.draw_indexed_primitive(5,1,0,4,8,2);reference.count_only=false;
                compare_clipped(gpu,cpu,context);
            }
        }
        for(PcSoftD3D9Device* d:{devices[0],devices[1]}){d->clear(7,0xff000000,1,0);check(d->draw_indexed_primitive(5,1,0,4,8,2)==0,"Indexed pixel draw succeeds");}
        reference.count_only=true;reference.draw_indexed_primitive(5,1,0,4,8,2);reference.count_only=false;
        compare(gpu,cpu,context);
        if(config==0xffffffffu){
            for(auto* d:devices)d->clear(7,0xff000000,1,0);
            // Two draws use the same mutable stream before a fence. The first
            // snapshot must survive the second unlock and cache replacement.
            for(unsigned half=0;half<2;++half){
                std::fill(colours.begin(),colours.end(),half?0xff00ff00:0xffff0000);
                for(unsigned i=0;i<3;++i){auto& d=*devices[i];d.set_viewport(half*32,0,32,48,0,1);
                    std::memcpy(d.lock(colour[i],4,unsigned(colours.size()*4),0),colours.data(),colours.size()*4);d.unlock(colour[i]);
                    d.count_only=i==2;check(d.draw_indexed_primitive(5,1,0,4,8,2)==0,"Mutable GPU stream draw succeeds");d.count_only=false;}
            }
            compare(gpu,cpu,context);for(auto* d:devices)d->set_viewport(0,0,64,48,0,1);
            // The same prepared VS must survive the MSAA pipeline variant.
            gpu.set_msaa(4);gpu.begin_frame();
            for(auto* d:devices){d->clear(7,0xff000000,1,0);d->count_only=d==&reference;
                check(d->draw_indexed_primitive(5,1,0,4,8,2)==0,"Prepared indexed MSAA draw succeeds");d->count_only=false;}
            gpu.end_frame();compare_clipped(gpu,cpu,context);gpu.set_msaa(1);
        }
        capture(4,1,14,2);capture(6,1,2,2);capture(4,70000,2,1);capture(4,-2,2,1);
        for(unsigned i=0;i<3;++i){devices[i]->set_vertex_shader(0);devices[i]->release(shaders[i]);}
    }
    check(gpu.native_vertex_draws-native_before==(std::getenv("OR2_PS5_CPU_VERTEX")?0u:12u),"Indexed cases use the selected GPU or CPU vertex path");
    // Hashes and state counters must remain equivalent independently of pixels.
    check(gpu.draw_hash==reference.draw_hash&&gpu.draws_hashed==reference.draws_hashed,"Indexed draw hashes preserve reference state/arguments");
    if(!std::getenv("OR2_PS5_CPU_VERTEX")){
        // A bounds-cache hit still validates a changed base, and Unlock must
        // invalidate a range even when its buffer handle and draw stay the same.
        const auto tokens=pc_shader_tokens(0x621e10);const auto shader=gpu.create_vertex_shader(tokens.data());gpu.set_vertex_shader(shader);
        check(VulkanVertexTestAccess::native(gpu,5,1,8,2),"Native index range warms");
        const auto scans=gpu.index_scan_count,hits=gpu.index_range_hits;
        check(VulkanVertexTestAccess::native(gpu,5,1,8,2),"Native index range reused");
        check(gpu.index_scan_count==scans&&gpu.index_range_hits==hits+1,"Cached range avoids repeated index scans");
        check(!VulkanVertexTestAccess::native(gpu,5,-1,8,2),"Cached range still rejects negative vertex address");
        std::uint16_t changed[sizeof(indices)/sizeof(indices[0])];std::memcpy(changed,indices,sizeof indices);changed[8]=65535;
        std::memcpy(gpu.lock(ib[0],0,sizeof changed,0),changed,sizeof changed);gpu.unlock(ib[0]);
        check(!VulkanVertexTestAccess::native(gpu,5,3,8,2),"Index unlock invalidates old bounds before stream validation");
        std::memcpy(gpu.lock(ib[0],0,sizeof indices,0),indices,sizeof indices);gpu.unlock(ib[0]);
        check(VulkanVertexTestAccess::native(gpu,5,3,8,2),"Restored index bounds see new contents");
        gpu.set_vertex_shader(0);gpu.release(shader);
    }
    for(unsigned i=0;i<3;++i){auto& d=*devices[i];d.hash_draws=false;d.set_stream_source(0,0,0,0);d.set_stream_source(1,0,0,0);d.set_indices(0);
        const auto decl=d.declaration;d.set_vertex_declaration(0);d.release(decl);d.release(vb[i]);d.release(colour[i]);d.release(ib[i]);}
    setup(gpu);
}
static void particle_cases(VulkanD3D9Device& gpu,PcSoftD3D9Device& cpu,vulkan::Context& context){
    unsigned handles[2]{},surfaces[2]{};unsigned index=0;
    for(auto* d:{static_cast<PcSoftD3D9Device*>(&gpu),&cpu}){
        setup(*d);d->set_vertex_shader(0);d->set_pixel_shader(0);
        handles[index]=d->create_texture(2,2,1,0,21,0);surfaces[index]=d->get_surface_level(handles[index],0);
        unsigned pitch=0;auto* p=d->lock_rect(surfaces[index],pitch);
        const unsigned colours[]{0xffff0000,0xff00ff00,0xff0000ff,0x80ffffff};
        for(unsigned y=0;y<2;++y)for(unsigned x=0;x<2;++x)std::memcpy(p+y*pitch+x*4,&colours[y*2+x],4);
        d->unlock_rect(surfaces[index]);d->set_texture(0,handles[index++]);
        d->set_texture_stage_state(0,1,4);d->set_texture_stage_state(0,2,0);d->set_texture_stage_state(0,3,2);
        d->set_texture_stage_state(0,4,4);d->set_texture_stage_state(0,5,2);d->set_texture_stage_state(0,6,3);
        d->set_render_state(RS_TEXTUREFACTOR,0x80ffffff);d->set_render_state(RS_ALPHABLENDENABLE,1);
        d->set_render_state(RS_SRCBLEND,5);d->set_render_state(RS_DESTBLEND,6);d->set_render_state(RS_ZWRITEENABLE,0);
    }
    struct Point {float x,y,z;unsigned colour;};struct Quad {float x,y,z,w;unsigned colour;float u,v;};
    // Exact retail FVF/state: textured colour, texture-factor alpha, distance
    // scaling, min/max clamp, centre clipping, and winding-independent sprites.
    for(unsigned test=0;test<4;++test){
        const float point_size=test==0?12.f:test==1?.2f:test==2?.001f:100.f;
        const float expected=test==0?12.f:test==1?19.2f:test==2?4.f:20.f;
        const bool sprite=test!=2;
        for(auto* d:{static_cast<PcSoftD3D9Device*>(&gpu),&cpu}){
            d->clear(7,0xff202020,1,0);d->set_render_state(154,u(point_size));d->set_render_state(155,u(4.f));d->set_render_state(166,u(20.f));
            d->set_render_state(156,sprite);d->set_render_state(157,test!=0);d->set_render_state(158,u(0.f));d->set_render_state(159,u(0.f));d->set_render_state(160,u(1.f));}
        gpu.set_render_state(RS_CULLMODE,CULL_CCW);gpu.set_fvf(0x42);
        const Point points[]{{0,0,.5f,0xffcc9966},{0,0,-.5f,0xffffffff},{2,0,.5f,0xffffffff},{0,0,1.5f,0xffffffff}};
        gpu.draw_primitive_up(1,4,points,sizeof(Point));
        const auto cull=gpu.render[RS_CULLMODE];check(cull==CULL_CCW,"Point sprites restore triangle culling state");
        cpu.set_render_state(RS_CULLMODE,CULL_NONE);cpu.set_fvf(0x144);
        const float lo_x=32-expected/2,hi_x=32+expected/2,lo_y=24-expected/2,hi_y=24+expected/2;
        const Quad q[]{{lo_x,lo_y,.5f,1,0xffcc9966,0,0},{hi_x,lo_y,.5f,1,0xffcc9966,sprite?1.f:0.f,0},
            {lo_x,hi_y,.5f,1,0xffcc9966,0,sprite?1.f:0.f},{hi_x,hi_y,.5f,1,0xffcc9966,sprite?1.f:0.f,sprite?1.f:0.f}};
        cpu.draw_primitive_up(5,2,q,sizeof(Quad));compare_clipped(gpu,cpu,context);
    }
    // World/view eye distance and perspective projection must both participate.
    std::array<float,16> world{1,0,0,0,0,1,0,0,0,0,1,0,0,0,2,1};
    std::array<float,16> projection{1,0,0,0,0,1,0,0,0,0,.5f,1,0,0,0,0};
    gpu.set_transform(0x100,world.data());gpu.set_transform(3,projection.data());gpu.set_render_state(154,u(.5f));
    gpu.set_render_state(155,u(1.f));gpu.set_render_state(166,u(64.f));gpu.set_render_state(156,1);
    gpu.clear(7,0xff202020,1,0);cpu.clear(7,0xff202020,1,0);
    const Point origin{0,0,0,0xffcc9966};gpu.draw_primitive_up(1,1,&origin,sizeof origin);
    const Quad expected[]{{26,18,.5f,1,0xffcc9966,0,0},{38,18,.5f,1,0xffcc9966,1,0},{26,30,.5f,1,0xffcc9966,0,1},{38,30,.5f,1,0xffcc9966,1,1}};
    cpu.draw_primitive_up(5,2,expected,sizeof(Quad));compare_clipped(gpu,cpu,context);
    const std::array<float,16> identity{1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1};
    gpu.set_transform(0x100,identity.data());gpu.set_transform(3,identity.data());
    index=0;for(auto* d:{static_cast<PcSoftD3D9Device*>(&gpu),&cpu}){
        d->set_texture(0,0);d->release(surfaces[index]);d->release(handles[index++]);d->set_render_state(RS_ZWRITEENABLE,1);
        d->set_render_state(RS_ALPHABLENDENABLE,0);setup(*d);}
    check(gpu.errors.empty(),"Retail particle POINTLIST no longer fails the renderer");
}
static void pipeline_cache_cases(){
    const auto path=std::string("/tmp/or2-vulkan-cache-")+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
    const auto* old=std::getenv("OR2_PS5_VK_CACHE");const bool had_old=old;const std::string previous=old?old:"";
    setenv("OR2_PS5_VK_CACHE",path.c_str(),1);
    {
        vulkan::Context context(64,48,false);VulkanD3D9Device gpu(context,64,48,false);setup(gpu);
        gpu.clear(7,0xff000000,1,0);quad(gpu,0xffff0000);auto pixels=context.readback(gpu.back_texture());
        check(pixels[(24*64+32)*4]==255,"Cold-cache pipeline renders correctly");check(context.warmed_pipelines()==0,"New cache starts empty");
        for(unsigned frame=0;frame<120;++frame)context.present();
        auto* checkpoint=std::fopen(path.c_str(),"rb");check(checkpoint!=nullptr,"Cache checkpoint survives an interrupted run");std::fclose(checkpoint);
    }
    {
        vulkan::Context context(64,48,false);check(context.warmed_pipelines()==1&&context.pipeline_count()==1,"Persisted pipeline is prewarmed on next launch");
        VulkanD3D9Device gpu(context,64,48,false);setup(gpu);gpu.clear(7,0xff000000,1,0);quad(gpu,0xff00ff00);
        const auto pixels=context.readback(gpu.back_texture());check(pixels[(24*64+32)*4+1]==255,"Warm-cache pipeline renders correctly");
        check(context.pipeline_count()==1,"Dynamic colour does not create a pipeline");
        gpu.set_render_state(RS_SRCBLEND,5);gpu.set_render_state(RS_DESTBLEND,6);gpu.set_render_state(53,3);gpu.set_render_state(59,0);
        quad(gpu,0xffff0000);context.finish();check(context.pipeline_count()==1,"Disabled blend/stencil state reuses a pipeline");
        gpu.set_render_state(RS_ALPHABLENDENABLE,1);quad(gpu,0x8000ff00);context.finish();
        check(context.pipeline_count()==2,"Enabling blending creates the required distinct pipeline");
    }
    // Truncation and a damaged key/cache checksum are rejected before any data
    // is passed to the Vulkan driver; a fresh valid pipeline still renders.
    auto* file=std::fopen(path.c_str(),"r+b");check(file!=nullptr,"Pipeline cache was saved");
    std::fseek(file,48,SEEK_SET);const auto byte=std::fgetc(file);std::fseek(file,48,SEEK_SET);std::fputc(byte^0xff,file);std::fclose(file);
    {vulkan::Context context(64,48,false);check(context.warmed_pipelines()==0,"Corrupt cached keys are ignored");}
    file=std::fopen(path.c_str(),"wb");std::fputs("OR2",file);std::fclose(file);
    {vulkan::Context context(64,48,false);VulkanD3D9Device gpu(context,64,48,false);setup(gpu);quad(gpu,0xff0000ff);
        const auto pixels=context.readback(gpu.back_texture());check(pixels[(24*64+32)*4+2]==255,"Truncated cache recovers and renders");}
    std::remove(path.c_str());std::remove((path+".tmp").c_str());
    if(had_old)setenv("OR2_PS5_VK_CACHE",previous.c_str(),1);else unsetenv("OR2_PS5_VK_CACHE");
}
int main(){try{
    pipeline_cache_cases();
    std::vector<VkDisplayModePropertiesKHR> modes(4);
    modes[0].parameters={{3840,2160},119880};modes[1].parameters={{1920,1080},119880};
    modes[2].parameters={{2560,1440},59940};modes[3].parameters={{1920,1080},59940};
    auto mode=vulkan::select_display_mode(modes,1280,720);
    check(mode.parameters.visibleRegion.width==1920&&mode.parameters.refreshRate==59940,"Unordered native modes choose 1080p60 for 720p compositor");
    mode=vulkan::select_display_mode(modes,2560,1600);
    check(mode.parameters.visibleRegion.width==3840,"Scanout must fit both compositor dimensions");
    mode=vulkan::select_display_mode(modes,7680,4320);
    check(mode.parameters.visibleRegion.width==3840,"Largest scanout fallback for oversized compositor");
    bool invalid=false;try{vulkan::select_display_mode({},1280,720);}catch(const std::exception&){invalid=true;}
    check(invalid,"Empty display mode list fails explicitly");
    VulkanDisplay display;std::string error;check(display.open(64,48,error),error.c_str());auto& context=display.context();
    VulkanD3D9Device gpu(context,64,48);PcSoftD3D9Device cpu(64,48);setup(gpu);setup(cpu);
    for(unsigned cull:{CULL_NONE,CULL_CW,CULL_CCW}){
        for(PcSoftD3D9Device* d:{static_cast<PcSoftD3D9Device*>(&gpu),&cpu}){
            d->set_render_state(RS_CULLMODE,cull);d->clear(7,0xff000000,1,0);quad(*d,0xffff0000);
        }compare(gpu,cpu,context);
    }
    for(PcSoftD3D9Device* d:{static_cast<PcSoftD3D9Device*>(&gpu),&cpu}){
        setup(*d);d->clear(7,0xff000000,1,0);quad(*d,0xffff0000,0.3f);quad(*d,0xff00ff00,0.7f);
        d->set_render_state(RS_ALPHABLENDENABLE,1);d->set_render_state(RS_SRCBLEND,5);d->set_render_state(RS_DESTBLEND,6);quad(*d,0x800000ff,0.2f);
    }compare(gpu,cpu,context);
    for(PcSoftD3D9Device* d:{static_cast<PcSoftD3D9Device*>(&gpu),&cpu}){
        d->set_render_state(RS_ALPHABLENDENABLE,0);d->clear(7,0xff000000,1,0);
        d->set_render_state(52,1);d->set_render_state(55,3);d->set_render_state(57,7);d->set_render_state(RS_COLORWRITEENABLE,0);
        quad(*d,0xffff0000);d->set_render_state(55,1);d->set_render_state(56,CMP_EQUAL);d->set_render_state(RS_COLORWRITEENABLE,15);
        quad(*d,0xff00ff00);d->set_render_state(52,0);
    }compare(gpu,cpu,context);
    for(PcSoftD3D9Device* d:{static_cast<PcSoftD3D9Device*>(&gpu),&cpu}){
        d->set_render_state(RS_ALPHATESTENABLE,1);d->set_render_state(RS_ALPHAFUNC,CMP_GREATER);d->set_render_state(RS_ALPHAREF,128);
        d->clear(7,0xff000000,1,0);quad(*d,0x7fff0000);quad(*d,0xff00ff00);
    }compare(gpu,cpu,context);
    for(PcSoftD3D9Device* d:{static_cast<PcSoftD3D9Device*>(&gpu),&cpu})d->set_render_state(RS_ALPHATESTENABLE,0);
    for(PcSoftD3D9Device* d:{static_cast<PcSoftD3D9Device*>(&gpu),&cpu}){
        d->set_render_state(RS_FOGENABLE,1);d->set_render_state(RS_FOGTABLEMODE,3);d->set_render_state(RS_FOGSTART,u(0.f));d->set_render_state(RS_FOGEND,u(2.f));
        d->set_render_state(RS_FOGCOLOR,0xff0000ff);d->clear(7,0xff000000,1,0);quad(*d,0xffff0000);d->set_render_state(RS_FOGENABLE,0);
    }compare(gpu,cpu,context);
    if(exe_image_autoload()){
        indexed_cases(gpu,context);
        // Bytecode comes only from the user's verified image. Both PS versions
        // and multi-blob specialization are compared to the recovered CPU code.
        for(unsigned version:{0xffff0101u,0xffff0104u})for(unsigned count:{1u,2u}){
            std::vector<unsigned> tokens{version};
            for(unsigned index=0;index<count;++index){const unsigned wanted=version==0xffff0101u?index:2+index;
                for(std::size_t i=0;i<PcShaderBlobCount;++i){const auto& b=PcShaderBlobs[i];if(b.version==version&&b.index==wanted){
                    tokens.insert(tokens.end(),PcShaderBlobTokens+b.token_offset,PcShaderBlobTokens+b.token_offset+b.token_count);break;}}}
            tokens.push_back(0xffff);
            for(PcSoftD3D9Device* d:{static_cast<PcSoftD3D9Device*>(&gpu),&cpu}){
                auto shader=d->create_pixel_shader(tokens.data());check(shader!=0,"Recovered pixel program creation");d->set_pixel_shader(shader);
                d->clear(7,0xff000000,1,0);quad(*d,0xff336699);d->set_pixel_shader(0);d->release(shader);
            }compare(gpu,cpu,context);
        }
        std::puts("Recovered PS 1.1/1.4 specialization: tested with verified user EXE");
    }else std::puts("Recovered PS 1.1/1.4 bytecode cases skipped: set OR2_EXE to your supported EXE/cache");
    unsigned sampled[2]{},surfaces[2]{};unsigned device_index=0;
    for(PcSoftD3D9Device* d:{static_cast<PcSoftD3D9Device*>(&gpu),&cpu}){
        sampled[device_index]=d->create_texture(2,2,2,0,21,0);surfaces[device_index]=d->get_surface_level(sampled[device_index],1);
        unsigned pitch=0;auto* p=d->lock_rect(surfaces[device_index],pitch);check(p&&pitch>=4,"Mip surface lock");
        p[0]=0x33;p[1]=0x66;p[2]=0x99;p[3]=255;d->unlock_rect(surfaces[device_index]);
        d->set_texture(0,sampled[device_index++]);d->set_texture_stage_state(0,2,2);d->set_texture_stage_state(0,5,2);
        d->set_sampler_state(0,SAMP_MAXMIPLEVEL,1);d->set_sampler_state(0,SAMP_MIPFILTER,2);
        d->clear(7,0xff000000,1,0);quad(*d,0xffffffff);
    }compare(gpu,cpu,context);
    device_index=0;
    for(PcSoftD3D9Device* d:{static_cast<PcSoftD3D9Device*>(&gpu),&cpu}){
        unsigned pitch=0;auto* p=d->lock_rect(surfaces[device_index],pitch);p[0]=255;p[1]=0;p[2]=0;p[3]=255;d->unlock_rect(surfaces[device_index]);
        d->clear(7,0xff000000,1,0);quad(*d,0xffffffff);
        d->set_texture(0,0);d->set_sampler_state(0,SAMP_MAXMIPLEVEL,0);d->set_sampler_state(0,SAMP_MIPFILTER,0);
        d->release(surfaces[device_index]);d->release(sampled[device_index++]);setup(*d);
    }compare(gpu,cpu,context);
    for(unsigned address:{5u,4u}){
        for(PcSoftD3D9Device* d:{static_cast<PcSoftD3D9Device*>(&gpu),&cpu}){
            auto t=d->create_texture(1,1,1,0,21,0);auto s=d->get_surface_level(t,0);unsigned pitch=0;auto* p=d->lock_rect(s,pitch);
            p[0]=255;p[1]=0;p[2]=0;p[3]=255;d->unlock_rect(s);d->set_texture(0,t);
            d->set_texture_stage_state(0,2,2);d->set_texture_stage_state(0,5,2);d->set_sampler_state(0,SAMP_ADDRESSU,address);d->set_sampler_state(0,SAMP_ADDRESSV,address);
            d->set_sampler_state(0,SAMP_BORDERCOLOR,0xff663399);
            struct U {float x,y,z,w;unsigned colour;float u,v;};
            const U v[]{{4,3,.5f,1,0xffffffff,-2,-2},{60,3,.5f,1,0xffffffff,-2,-2},{4,45,.5f,1,0xffffffff,-2,-2},{60,45,.5f,1,0xffffffff,-2,-2}};
            d->clear(7,0xff000000,1,0);d->set_fvf(0x144);d->draw_primitive_up(5,2,v,sizeof(U));d->set_texture(0,0);d->release(s);d->release(t);
            d->set_sampler_state(0,SAMP_ADDRESSU,1);d->set_sampler_state(0,SAMP_ADDRESSV,1);d->set_sampler_state(0,SAMP_BORDERCOLOR,0);setup(*d);
        }compare(gpu,cpu,context);
    }
    for(unsigned cycle=0;cycle<50;++cycle){
        auto t=gpu.create_cube_texture(16,2,1,21,0);auto s=gpu.get_cube_map_surface(t,cycle%6,1);
        auto depth=gpu.create_depth_stencil_surface(8,8,75,0,0,0);
        gpu.set_render_target(0,s);gpu.set_depth_stencil_surface(depth);gpu.release(s);gpu.release(depth);gpu.release(t);
        gpu.clear(7,0xff123456,1,0);gpu.set_render_target(0,PcSoftD3D9Device::BackBuffer);gpu.set_depth_stencil_surface(PcSoftD3D9Device::DepthBuffer);
        check(gpu.live_objects()==0,"Reflection targets release D3D resources before the GPU fence");
    }
    // Render a cube face, then sample it at a later draw in the same submission.
    auto cube=gpu.create_cube_texture(16,1,1,21,0);auto face=gpu.get_cube_map_surface(cube,0,0);
    auto depth=gpu.create_depth_stencil_surface(16,16,75,0,0,0);gpu.set_render_target(0,face);gpu.set_depth_stencil_surface(depth);
    gpu.clear(7,0xff336699,1,0);gpu.set_render_target(0,PcSoftD3D9Device::BackBuffer);gpu.set_depth_stencil_surface(PcSoftD3D9Device::DepthBuffer);
    gpu.set_texture(0,cube);gpu.set_texture_stage_state(0,2,2);gpu.set_texture_stage_state(0,5,2);
    struct T {float x,y,z,w;unsigned c;float u,v,t;};
    const T v[]{{4,3,.5f,1,0xffffffff,1,0,0},{60,3,.5f,1,0xffffffff,1,0,0},{4,45,.5f,1,0xffffffff,1,0,0},{60,45,.5f,1,0xffffffff,1,0,0}};
    gpu.clear(7,0xff000000,1,0);gpu.set_fvf(0x10144);gpu.draw_primitive_up(5,2,v,sizeof(T));
    auto pixels=context.readback(gpu.back_texture());check(pixels[(24*64+32)*4]==0x33&&pixels[(24*64+32)*4+1]==0x66&&pixels[(24*64+32)*4+2]==0x99,"Reflection render-to-texture barrier and cube sampling");
    gpu.set_texture(0,0);gpu.release(face);gpu.release(depth);gpu.release(cube);setup(gpu);
    // Interleaved materials reuse immutable descriptors, including sampler
    // changes. Texture writes still take effect on a cache hit; retiring the
    // command buffer invalidates all cached sets before their pool is reset.
    {
        const auto a=gpu.create_texture(1,1,1,0,21,0),b=gpu.create_texture(1,1,1,0,21,0);
        const auto sa=gpu.get_surface_level(a,0),sb=gpu.get_surface_level(b,0);
        for(auto surface:{sa,sb}){unsigned pitch=0;auto* p=gpu.lock_rect(surface,pitch);
            p[0]=0;p[1]=surface==sb?255:0;p[2]=surface==sa?255:0;p[3]=255;gpu.unlock_rect(surface);}
        gpu.set_texture_stage_state(0,2,2);gpu.set_texture_stage_state(0,5,2);
        gpu.clear(7,0xff000000,1,0);gpu.set_texture(0,a);quad(gpu,0xffffffff);
        gpu.set_texture(0,b);quad(gpu,0xffffffff);
        const auto allocated=context.descriptor_sets_allocated;
        gpu.set_texture(0,a);quad(gpu,0xffffffff);
        check(context.descriptor_sets_allocated==allocated,"Interleaved A B A textures reuse earlier descriptors");
        const auto min_filter=gpu.get_sampler_state(0,SAMP_MINFILTER);
        gpu.set_sampler_state(0,SAMP_MINFILTER,min_filter==2?1:2);quad(gpu,0xffffffff);
        const auto filtered=context.descriptor_sets_allocated;
        check(filtered==allocated+1,"Changed sampler gets a distinct immutable descriptor set");
        gpu.set_sampler_state(0,SAMP_MINFILTER,min_filter);quad(gpu,0xffffffff);
        check(context.descriptor_sets_allocated==filtered,"Returning sampler reuses its earlier descriptor set");
        // Re-upload while A's set exists. Its transition must run before reuse.
        unsigned pitch=0;auto* p=gpu.lock_rect(sa,pitch);p[0]=255;p[1]=0;p[2]=0;p[3]=255;gpu.unlock_rect(sa);
        gpu.set_texture(0,b);quad(gpu,0xffffffff);gpu.set_texture(0,a);quad(gpu,0xffffffff);
        check(context.descriptor_sets_allocated==filtered,"Updated texture content preserves descriptor identity");
        pixels=context.readback(gpu.back_texture());
        check(pixels[(24*64+32)*4+2]==255&&pixels[(24*64+32)*4]==0,"Cached material samples latest uploaded texture contents");
        quad(gpu,0xffffffff);
        check(context.descriptor_sets_allocated==filtered+1,"Retired descriptor pool is never reused in a new recording");
        context.finish();gpu.set_texture(0,0);gpu.release(sa);gpu.release(sb);gpu.release(a);gpu.release(b);setup(gpu);
    }
    // Matrices/material constants move to a new upload slice while texture,
    // buffer and range stay the same. Reuse the set, but bind the new offset.
    gpu.clear(7,0xff000000,1,0);gpu.set_texture_stage_state(0,2,3);gpu.set_texture_stage_state(0,5,3);
    gpu.set_render_state(RS_TEXTUREFACTOR,0xffff0000);quad(gpu,0xffffffff);
    const auto sets_before=context.descriptor_sets_allocated,reused_before=context.descriptor_sets_reused;
    gpu.set_render_state(RS_TEXTUREFACTOR,0xff00ff00);quad(gpu,0xffffffff);
    check(context.descriptor_sets_allocated==sets_before&&context.descriptor_sets_reused>reused_before,"Changed uniform slice reuses dynamic descriptor set");
    // Raw command-buffer users invalidate cached state even if the next D3D
    // draw requests exactly its previously recorded viewport/scissor.
    const auto raw=context.commands();const VkViewport small{0,0,2,2,0,1};const VkRect2D scissor{{0,0},{2,2}};
    vulkan::vkCmdSetViewport(raw,0,1,&small);vulkan::vkCmdSetScissor(raw,0,1,&scissor);
    gpu.set_render_state(RS_TEXTUREFACTOR,0xff0000ff);quad(gpu,0xffffffff);
    pixels=context.readback(gpu.back_texture());check(pixels[(24*64+32)*4+2]==255&&pixels[(24*64+32)*4]==0,"Raw Vulkan state changes restore viewport/scissor and latest dynamic constants");
    setup(gpu);
    gpu.begin_frame();gpu.clear(7,0xff000000,1,0);quad(gpu,0xff00ff00);gpu.end_frame();check(gpu.pixels_shaded>0,"Actual Vulkan occlusion samples counted");
    auto query=gpu.create_occlusion_query();gpu.clear(7,0xff000000,1,0);gpu.query_issue(query,2);quad(gpu,0xffff0000);gpu.query_issue(query,1);
    unsigned samples=0;check(gpu.query_get_data(query,samples)==1,"D3D occlusion results wait for their submission");
    context.finish();check(gpu.query_get_data(query,samples)==0&&samples>0,"D3D occlusion queries use GPU results");
    // Exercise independent back-buffer sizes and the scene/2D resolve boundary.
    const auto resolves_before=gpu.msaa_resolves;
    gpu.resize_back_buffer(128,96);gpu.set_msaa(4);gpu.begin_frame();gpu.clear(7,0xff000000,1,0);scene_quad(gpu,0xff00ff00);gpu.finish_scene();gpu.end_frame();
    check(gpu.msaa_resolves==resolves_before+1,"MSAA scene resolves once");pixels=context.readback(gpu.back_texture());check(pixels[(48*128+64)*4+1]==255,"MSAA resolve preserves scene colour");
    gpu.set_msaa(1);gpu.set_fxaa(true);gpu.begin_frame();gpu.clear(7,0xff000000,1,0);scene_quad(gpu,0xffff0000);gpu.end_frame();check(gpu.fxaa_passes==1,"FXAA runs over the scene");
    pixels=context.readback(gpu.back_texture());check(pixels[(48*128+64)*4]==255,"FXAA preserves a flat region");
    display.clear();display.pc_frame(gpu.back_texture(),0,0,64,48);check(display.present(error),error.c_str());
    // Asymmetric image catches vertical inversion and BGRA/RGBA mistakes.
    const unsigned char image[]{255,0,0,255,0,255,0,255,0,0,255,255,255,255,255,255};display.clear();display.rgba(image,image,2,2,0,0,64,48);
    auto output=context.readback(context.display_target());check(output[(4*64+4)*4]>240&&output[(4*64+4)*4+2]<15,"Display top-left orientation");
    check(output[(43*64+4)*4+2]>240&&output[(43*64+4)*4]<15,"Display bottom-left orientation");check(display.present(error),error.c_str());
    // Native mode omits frame-wide diagnostics while keeping requested game
    // queries functional, including overlaps and a query-reset block boundary.
    VulkanD3D9Device fast(context,64,48,false);setup(fast);setup(cpu);
    fast.begin_frame();fast.clear(7,0xff000000,1,0);quad(fast,0xff00ff00);fast.end_frame();
    cpu.clear(7,0xff000000,1,0);quad(cpu,0xff00ff00);compare(fast,cpu,context);
    check(fast.pixels_shaded==0,"Native mode does not collect frame diagnostic samples");
    fast.begin_frame();fast.clear(7,0xff000000,1,0);quad(fast,0xffff0000);fast.end_frame();
    display.clear();display.pc_frame(fast.back_texture(),0,0,64,48);check(display.present(error),error.c_str());
    output=context.readback(context.display_target());
    check(output[(24*64+32)*4]==255&&output[(24*64+32)*4+1]==0,"Deferred native scene and compositor execute in one submission");
    auto first=fast.create_occlusion_query(),second=fast.create_occlusion_query();
    fast.query_issue(first,2);fast.query_issue(second,2);quad(fast,0xffff0000);fast.query_issue(first,1);
    unsigned a=0,b=0;context.finish();check(fast.query_get_data(first,a)==0&&a>0,"Game query works with frame diagnostics disabled");
    fast.clear(7,0xff000000,1,0);quad(fast,0xff00ff00);fast.query_issue(second,1);
    context.finish();check(fast.query_get_data(second,b)==0&&b>a,"Overlapping query spans submissions");
    const auto counted=context.samples_passed();quad(fast,0xffffffff);context.finish();
    check(context.samples_passed()==counted,"Ending all game queries disables sample counting again");
    fast.query_issue(first,2);fast.query_issue(first,2);for(unsigned k=0;k<300;++k){fast.clear(7,0xff000000,1,0);quad(fast,0xff00ff00);}
    fast.query_issue(first,1);context.finish();check(fast.query_get_data(first,a)==0&&a>0,"Query reset batches span more than 256 draws");
    const auto after=context.samples_passed();quad(fast,0xffffffff);context.finish();
    check(context.samples_passed()==after,"Repeated BEGIN does not keep diagnostic sampling enabled");
    {VulkanD3D9Device particles(context,64,48,false);PcSoftD3D9Device reference(64,48);particle_cases(particles,reference,context);}
    if(std::getenv("OR2_PS5_VK_LINK_ONLY"))check(context.linked_shader_uses()>0,"Directly linked shaders execute the differential rendering cases");
    else if(!vulkan::shaders::prepared_shaders.empty())check(context.prepared_shader_uses()>0,"Offline prepared shaders execute the differential rendering cases");
    std::printf("PS5 Vulkan: differential pixels, particles, pipeline cache, depth/blend/stencil/cull, reflections, occlusion, MSAA, FXAA and display: %u checks passed\n",checks);
    return 0;
}catch(const std::exception& e){std::fprintf(stderr,"FAIL: %s\n",e.what());return 1;}}
