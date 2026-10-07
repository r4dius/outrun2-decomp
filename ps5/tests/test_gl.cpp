#include "gl_display.hpp"
#include "gl_d3d9.hpp"
#include <cstdio>
#include <cstdlib>
#include <cmath>
using namespace outrun::ps5_runtime;
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
static void compare(GlD3D9Device& gpu,PcSoftD3D9Device& cpu){
    std::vector<unsigned char> pixels(64*48*4);glGetTextureImage(gpu.back_texture(),0,GL_RGBA,GL_UNSIGNED_BYTE,int(pixels.size()),pixels.data());
    for(unsigned y=5;y<43;++y)for(unsigned x=6;x<58;++x)for(unsigned c=0;c<4;++c)
        check(std::abs(int(pixels[(y*64+x)*4+c])-int(cpu.rgba()[(y*64+x)*4+c]))<=1,"GPU differs from CPU at interior pixel");
    check(cpu.errors.empty()&&gpu.errors.empty(),"D3D errors absent");gl_require("differential readback");
}
int main(){
    GlDisplay display;std::string error;check(display.open(64,48,error),error.c_str());
    GlD3D9Device gpu(64,48);PcSoftD3D9Device cpu(64,48);setup(gpu);setup(cpu);
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
    {   // Console mode: removing diagnostic queries must preserve the pixels.
        GlD3D9Device fast(64,48,false);PcSoftD3D9Device reference(64,48);setup(fast);setup(reference);
        fast.begin_frame();
        for(PcSoftD3D9Device* d:{static_cast<PcSoftD3D9Device*>(&fast),&reference}){
            d->clear(7,0xff000000,1,0);quad(*d,0xffff0000,0.3f);quad(*d,0xff00ff00,0.7f);
            d->set_render_state(RS_ALPHABLENDENABLE,1);d->set_render_state(RS_SRCBLEND,5);d->set_render_state(RS_DESTBLEND,6);
            quad(*d,0x800000ff,0.2f);
        }
        fast.end_frame();compare(fast,reference);
        check(fast.pixels_shaded==0&&fast.gpu_nanoseconds()==0,"console mode does not run diagnostic queries");
    }
    check(display.present(error),error.c_str());
    std::printf("PS5 OpenGL differential pixels, depth/blend/stencil, cull and 50 reflection lifecycle cycles: %u checks passed\n",checks);
}
