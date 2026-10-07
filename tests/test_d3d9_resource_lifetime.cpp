#include "platform/pc_soft_d3d9.hpp"
#include "platform/pc_scene_renderer.hpp"
#include <cstdio>
#include <cstdlib>
using outrun::platform::PcSoftD3D9Device;
static unsigned checks=0;
static void check(bool value,const char* message){++checks;if(!value){std::fprintf(stderr,"FAIL: %s\n",message);std::exit(1);}}
int main(){
    PcSoftD3D9Device d(64,48);
    for(unsigned cycle=0;cycle<100;++cycle){
        auto b=d.create_vertex_buffer(128,0,0,0);
        d.set_stream_source(0,b,0,16);d.release(b);
        check(d.buffer_size(b)==128,"bound buffer retains ownership");
        d.set_stream_source(0,0,0,0);
        check(d.lock(b,0,4,0)==nullptr,"released buffer cannot be locked");
        auto t=d.create_texture(32,32,1,1,21,0);
        d.set_texture(0,t);d.release(t);
        auto saved=d.get_texture(0);d.set_texture(0,0);
        check(d.live_objects()==1,"GetTexture retains texture after unbind");
        d.release(saved);check(d.live_objects()==0,"texture released after GetTexture reference");
        t=d.create_cube_texture(16,1,1,21,0);
        auto s=d.get_cube_map_surface(t,0,0),again=d.get_cube_map_surface(t,0,0);
        check(s==again,"surface identity stable");
        d.release(t);d.release(again);
        const auto back=d.get_render_target(0);
        d.set_render_target(0,s);d.release(s);
        d.clear(1u,0xff112233u,1.f,0);
        check(d.rgba().size()==16u*16*4&&d.rgba()[0]==0x11,"surface and parent retained while bound");
        d.set_render_target(0,back);d.release(back);
        check(d.live_objects()==0,"restoring target frees surface and parent texture");
        auto depth=d.create_depth_stencil_surface(64,48,21,0,0,0);
        d.set_depth_stencil_surface(depth);d.release(depth);
        saved=d.get_depth_stencil_surface();d.set_depth_stencil_surface(PcSoftD3D9Device::DepthBuffer);
        check(d.live_objects()==1,"GetDepthStencilSurface retains ownership");
        d.release(saved);check(d.live_objects()==0,"depth surface freed");
    }
    auto pending=d.create_texture(8,8,1,1,21,0);
    {
        outrun::platform::PcSceneRenderer scene(d);
        check(scene.texture_swap_406630(0x1230000,pending),"deferred swap owns reference");
        check(scene.texture_swap_406630(0x1230000,pending),"same deferred swap preserves reference");
        d.release(pending);unsigned width=0,height=0;
        check(d.texture_level_size(pending,0,width,height),"deferred swap retains producer texture");
    }
    unsigned width=0,height=0;
    check(!d.texture_level_size(pending,0,width,height),"scene shutdown releases unapplied swap");
    check(d.errors.empty(),"no D3D9 errors across repeated cycles");
    std::printf("D3D9 renderer resources: %u checks passed\n",checks);
}
