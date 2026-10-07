#include "metal_d3d9.hpp"
#include <cstdio>
#include <cstdlib>
#include <cmath>
#include <cstring>
using namespace outrun::mac;
using namespace outrun::platform;
using namespace outrun::platform::d3d9;
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
static void compare(MetalD3D9Device& gpu,PcSoftD3D9Device& cpu){
    gpu.readback();const auto& pixels=gpu.rgba();
    for(unsigned y=5;y<43;++y)for(unsigned x=6;x<58;++x)for(unsigned c=0;c<4;++c){
        const auto p=(y*64+x)*4+c;
        if(std::abs(int(pixels[p])-int(cpu.rgba()[p]))>1)std::fprintf(stderr,"pixel (%u,%u) channel %u: Metal=%u CPU=%u checks=%u\n",x,y,c,pixels[p],cpu.rgba()[p],checks);
        check(std::abs(int(pixels[p])-int(cpu.rgba()[p]))<=1,"GPU differs from CPU at interior pixel");
    }
    check(cpu.errors.empty()&&gpu.errors.empty(),"D3D errors absent");
}
int main(){
    
    MetalD3D9Device gpu(64,48);PcSoftD3D9Device cpu(64,48);setup(gpu);setup(cpu);
    for(unsigned cull:{CULL_NONE,CULL_CW,CULL_CCW}){
        for(PcSoftD3D9Device* d:{static_cast<PcSoftD3D9Device*>(&gpu),&cpu}){
            d->set_render_state(RS_CULLMODE,cull);d->clear(7,0xff000000,1,0);quad(*d,0xffff0000);
        }compare(gpu,cpu);
    }
    for(PcSoftD3D9Device* d:{static_cast<PcSoftD3D9Device*>(&gpu),&cpu}){
        setup(*d);d->clear(7,0xff000000,1,0);quad(*d,0xffff0000,0.3f);quad(*d,0xff00ff00,0.7f);
        d->set_render_state(RS_ALPHABLENDENABLE,1);d->set_render_state(RS_SRCBLEND,5);d->set_render_state(RS_DESTBLEND,6);
        quad(*d,0x800000ff,0.2f);
    }compare(gpu,cpu);
    for(PcSoftD3D9Device* d:{static_cast<PcSoftD3D9Device*>(&gpu),&cpu}){
        d->set_render_state(RS_ALPHABLENDENABLE,0);d->clear(7,0xff000000,1,0);
        d->set_render_state(52,1);d->set_render_state(55,3);d->set_render_state(57,7);d->set_render_state(RS_COLORWRITEENABLE,0);
        quad(*d,0xffff0000);d->set_render_state(55,1);d->set_render_state(56,CMP_EQUAL);d->set_render_state(RS_COLORWRITEENABLE,15);
        quad(*d,0xff00ff00);d->set_render_state(52,0);
    }compare(gpu,cpu);
    for(unsigned cycle=0;cycle<50;++cycle){
        auto t=gpu.create_cube_texture(16,1,1,21,0);auto s=gpu.get_cube_map_surface(t,cycle%6,0);
        auto depth=gpu.create_depth_stencil_surface(16,16,75,0,0,0);
        gpu.set_render_target(0,s);gpu.set_depth_stencil_surface(depth);gpu.release(s);gpu.release(depth);gpu.release(t);
        gpu.clear(7,0xff123456,1,0);gpu.set_render_target(0,PcSoftD3D9Device::BackBuffer);gpu.set_depth_stencil_surface(PcSoftD3D9Device::DepthBuffer);
        check(gpu.live_objects()==0,"reflection target cycle releases D3D resources");
    }
    gpu.begin_frame();quad(gpu,0xff00ff00);gpu.end_frame();check(gpu.pixels_shaded>0,"actual GPU samples counted");
    // Indexed geometry uses the retained VS blob with native Metal clipping.
    const auto& blob=PcShaderBlobs[0];
    std::vector<std::uint32_t> code{0xfffe0101u};
    code.insert(code.end(),PcShaderBlobTokens+blob.token_offset,PcShaderBlobTokens+blob.token_offset+blob.token_count);
    const auto& inputs=PcShaderBlobs[1];
    code.insert(code.end(),PcShaderBlobTokens+inputs.token_offset,PcShaderBlobTokens+inputs.token_offset+inputs.token_count);
    code.push_back(0xffffu);
    const PcVertexElement declaration[]{{0,0,3,0,0,0},{PcDeclEndStream,0,17,0,0,0}};
    const float identity[16]{1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1};
    const std::uint16_t indices[]{0,1,2,2,1,3};
    for(float z:{0.5f,-0.1f}){
        const float vertices[]{-.91f,.89f,z,1,.87f,.89f,z,1,-.91f,-.87f,z,1,.87f,-.87f,z,1};
        for(PcSoftD3D9Device* d:{static_cast<PcSoftD3D9Device*>(&gpu),&cpu}){
            d->set_render_state(RS_TEXTUREFACTOR,0xff3090c0u);
            d->set_texture_stage_state(0,2,3);d->set_texture_stage_state(0,5,3);
            const auto vb=d->create_vertex_buffer(sizeof vertices,0,0,0),ib=d->create_index_buffer(sizeof indices,0,101,0);
            std::memcpy(d->lock(vb,0,sizeof vertices,0),vertices,sizeof vertices);d->unlock(vb);
            std::memcpy(d->lock(ib,0,sizeof indices,0),indices,sizeof indices);d->unlock(ib);
            const auto vs=d->create_vertex_shader(code.data()),decl=d->create_vertex_declaration(declaration);
            check(vs&&decl,"retained indexed vertex shader and declaration");
            d->set_vertex_shader(vs);d->set_vertex_declaration(decl);d->set_stream_source(0,vb,0,16);d->set_indices(ib);
            d->set_vertex_shader_constant_f(64,identity,4);d->clear(7,0xff000000u,1,0);
            check(d->draw_indexed_primitive(4,0,0,4,0,2)==0,"indexed draw succeeds");
            d->set_vertex_shader(0);d->set_vertex_declaration(0);d->set_stream_source(0,0,0,0);d->set_indices(0);
            for(auto h:{vb,ib,vs,decl})d->release(h);
        }compare(gpu,cpu);
    }
    MetalD3D9Device scaled(64,48,2);setup(scaled);scaled.clear(7,0xff000000,1,0);quad(scaled,0xffff0000);scaled.readback();
    check(scaled.presentation_width()==128&&scaled.presentation_height()==96,"native 2x target dimensions");
    const auto centre=(48u*128+64)*4;
    check(scaled.rgba().size()==128u*96*4&&scaled.rgba()[centre]==255&&scaled.rgba()[centre+1]==0,"2x render preserves logical geometry");
    // Options > Settings enhancement rows: any back buffer size (the logical target is
    // stretched), the centred 4:3 rectangle of pretransformed draws in a 16:9
    // back buffer, and FXAA over the scene once per frame.
    {
        MetalD3D9Device wide(64,48);setup(wide);wide.resize_back_buffer(128,72);
        check(wide.presentation_width()==128&&wide.presentation_height()==72,"resized back buffer");
        const auto red=[&](unsigned x,unsigned y){return wide.rgba()[(std::size_t(y)*128+x)*4];};
        wide.clear(7,0xff000000,1,0);quad(wide,0xffff0000);wide.readback();
        check(red(10,8)==255&&red(118,64)==255&&red(4,30)==0,"stretched draw covers the wide back buffer");
        wide.set_ui_rect(true);wide.clear(7,0xff000000,1,0);quad(wide,0xffff0000);wide.readback();
        check(red(10,30)==0&&red(24,30)==255&&red(104,30)==255&&red(118,30)==0,"pretransformed draws keep the centred 4:3 rectangle");
        wide.set_ui_rect(false);
        const V triangle[]{{4,3,.5f,1,0xffff0000},{60,3,.5f,1,0xffff0000},{4,45,.5f,1,0xffff0000}};
        auto blended=[&]{unsigned n=0;for(unsigned y=0;y<72;++y)for(unsigned x=0;x<128;++x)n+=red(x,y)>0&&red(x,y)<255;return n;};
        wide.begin_frame();wide.clear(7,0xff000000,1,0);wide.set_fvf(0x44);wide.draw_primitive_up(4,1,triangle,sizeof(V));wide.end_frame();
        check(blended()==0,"hard edges without FXAA");
        wide.set_fxaa(true);
        wide.begin_frame();wide.clear(7,0xff000000,1,0);wide.set_fvf(0x44);wide.draw_primitive_up(4,1,triangle,sizeof(V));
        wide.finish_scene();wide.finish_scene();wide.end_frame();
        check(wide.fxaa_passes==1&&blended()>0&&red(20,10)==255,"FXAA smooths the scene edges once per frame");
        wide.set_fxaa(false);
        for(unsigned samples:{2u,4u}){
            wide.set_msaa(samples);
            wide.begin_frame();wide.clear(7,0xff000000,1,0);wide.set_fvf(0x44);wide.draw_primitive_up(4,1,triangle,sizeof(V));
            const V corner[]{{50,34,.1f,1,0xff00ff00},{62,34,.1f,1,0xff00ff00},{50,46,.1f,1,0xff00ff00},{62,46,.1f,1,0xff00ff00}};
            wide.finish_scene();wide.draw_primitive_up(5,2,corner,sizeof(V));wide.end_frame();   // 2D after the resolve: single-sampled back buffer
            check(blended()>0&&red(20,10)==255&&wide.rgba()[(std::size_t(64)*128+115)*4+1]==255,"MSAA scene resolved before the 2D layer");
            wide.begin_frame();wide.clear(7,0xff000000,1,0);wide.set_fvf(0x44);wide.draw_primitive_up(4,1,triangle,sizeof(V));wide.end_frame();
            check(blended()>0&&red(20,10)==255,"MSAA resolved at the end of a frame without a 2D layer");
        }
        check(wide.msaa_resolves==4,"one MSAA resolve per frame");
        wide.set_msaa(1);
        check(wide.errors.empty(),"enhancement paths without D3D errors");
    }
    // Retail point-sprite particles: compare real GPU output against an
    // explicitly sized screen quad, including eye-distance scaling/clamping.
    struct Point {float x,y,z;unsigned colour;};
    const Point point{.01f,.023f,.5f,0xffff0000};
    for(unsigned mode=0;mode<4;++mode){
        const float size=mode==2?4.f:8.f;
        setup(gpu);setup(cpu);gpu.clear(7,0xff000000,1,0);cpu.clear(7,0xff000000,1,0);
        for(auto* d:{static_cast<PcSoftD3D9Device*>(&gpu),&cpu}){
            d->set_texture_stage_state(0,2,0);d->set_texture_stage_state(0,5,0);
            d->set_transform(0x100,identity);d->set_transform(2,identity);d->set_transform(3,identity);
        }
        gpu.set_fvf(0x42);gpu.set_render_state(156,1);gpu.set_render_state(157,mode==1);
        const float distance=std::sqrt(point.x*point.x+point.y*point.y+point.z*point.z);
        gpu.set_render_state(154,u(mode==1?8.f*distance/48.f:8.f));
        gpu.set_render_state(155,u(2.f));gpu.set_render_state(166,u(mode==2?4.f:64.f));
        gpu.set_render_state(158,u(0.f));gpu.set_render_state(159,u(0.f));gpu.set_render_state(160,u(1.f));
        gpu.draw_primitive_up(1,1,&point,sizeof point);
        const float x=(point.x+1)*32,y=(1-point.y)*24;
        const V q[]{{x-size/2,y-size/2,.5f,1,point.colour},{x+size/2,y-size/2,.5f,1,point.colour},
                    {x-size/2,y+size/2,.5f,1,point.colour},{x+size/2,y+size/2,.5f,1,point.colour}};
        cpu.set_fvf(0x44);cpu.draw_primitive_up(5,2,q,sizeof(V));compare(gpu,cpu);
        check(gpu.get_render_state(RS_CULLMODE)==CULL_NONE,"point draw preserves culling state");
        if(mode==3){
            struct Textured {V vertex;float u,v;};
            const Textured textured[]{{q[0],0,0},{q[1],1,0},{q[2],0,1},{q[3],1,1}};
            std::uint32_t handles[2]{};unsigned k=0;
            for(auto* d:{static_cast<PcSoftD3D9Device*>(&gpu),&cpu}){
                const auto texture=d->create_texture(2,2,1,0,21,0);handles[k++]=texture;
                const auto surface=d->get_surface_level(texture,0);std::uint32_t pitch;
                auto* p=d->lock_rect(surface,pitch);check(p&&pitch==8,"particle texture storage");
                const unsigned colours[]{0x80ffffff,0x8000ffff,0x80ff00ff,0x80ffff00};
                std::memcpy(p,colours,sizeof colours);d->unlock_rect(surface);d->release(surface);
                d->set_texture(0,texture);d->set_texture_stage_state(0,1,4);d->set_texture_stage_state(0,2,2);d->set_texture_stage_state(0,3,0);
                d->set_texture_stage_state(0,4,4);d->set_texture_stage_state(0,5,2);d->set_texture_stage_state(0,6,0);
                d->set_render_state(RS_ALPHABLENDENABLE,1);d->set_render_state(RS_SRCBLEND,5);d->set_render_state(RS_DESTBLEND,6);
                d->clear(7,0xff000000,1,0);
            }
            gpu.draw_primitive_up(1,1,&point,sizeof point);
            cpu.set_fvf(0x144);cpu.draw_primitive_up(5,2,textured,sizeof(Textured));compare(gpu,cpu);
            gpu.set_texture(0,0);cpu.set_texture(0,0);gpu.release(handles[0]);cpu.release(handles[1]);
        }
    }
    
    std::printf("macOS Metal differential pixels, depth/blend/stencil, cull and 50 reflection lifecycle cycles: %u checks passed\n",checks);
}
