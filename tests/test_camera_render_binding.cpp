#include "platform/camera_render_binding.hpp"
#include "platform/embedded_camera_data.hpp"
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>
using namespace outrun;
using namespace outrun::driving;
static unsigned checks;
#define CHECK(x) do{++checks;if(!(x)){std::fprintf(stderr,"camera binding line %d: %s\n",__LINE__,#x);std::exit(1);}}while(0)
namespace {
float at(const std::array<std::uint8_t,64>& m,unsigned r,unsigned c){float v;std::memcpy(&v,m.data()+(r*4+c)*4,4);return v;}
std::array<double,4> d3d(const std::array<double,4>& v,const std::array<std::uint8_t,64>& m){
    std::array<double,4> o{};for(unsigned c=0;c<4;++c)for(unsigned r=0;r<4;++r)o[c]+=v[r]*at(m,r,c);return o;}
std::array<double,4> column(const platform::MeshPreviewTransform& t,const std::array<double,4>& v){
    std::array<double,4> o{};for(unsigned row=0;row<4;++row)for(unsigned col=0;col<4;++col)o[row]+=double(t.position[col*4+row])*v[col];return o;}
}
int main(){
    // Real frontend camera: original init then the mode-32 preset control.
    std::vector<std::uint8_t> cam(0x400),live_fe8(0x64c),live_bb0(0x438),live_750(0x5460),arena(8*64);
    Bytes c(cam.data(),cam.size());
    PcMatrixStack stack{Bytes(arena.data(),arena.size()),0,0,8};pc_matrix_identity(stack);
    PcCameraTables tables{Bytes(const_cast<std::uint8_t*>(platform::EmbeddedCameraTable5b4a30),0x64c),
        Bytes(const_cast<std::uint8_t*>(platform::EmbeddedCameraTable5ba4e0),0x438),
        Bytes(const_cast<std::uint8_t*>(platform::EmbeddedCameraTable5b5080),0x5460),
        Bytes(live_fe8.data(),live_fe8.size()),Bytes(live_bb0.data(),live_bb0.size()),Bytes(live_750.data(),live_750.size())};
    const PcCameraScreen screen{};const PcCameraBlend blend{0,0,0,1.f};
    PcCameraDevice device;
    camera_init_484ee0(c,stack,device,screen,blend,0,tables);
    std::vector<std::uint8_t> car(0xf00);Bytes v(car.data(),car.size());v.put8(0x11,9);
    for(int preset:{0,1}){
        PcCameraControl control{0,32,preset,0,v,Bytes(const_cast<std::uint8_t*>(platform::EmbeddedCameraTable5ba4e0),0x438)};
        device=PcCameraDevice{};
        CHECK(camera_control_485fe0(c,stack,device,screen,blend,control));
        CHECK(device.stores.size()==2&&device.stores[0].slot==1&&device.stores[1].slot==0);
        const auto& view=device.slots_95d860[0];const auto& proj=device.slots_95d860[1];
        platform::MeshPreviewTransform t{};
        CHECK(platform::camera_scene_transform(view,proj,t));
        // The renderer's column-major product equals Direct3D v*View*Proj.
        for(double x:{-3.0,0.0,4.5})for(double z:{-20.0,-2.0,6.0}){
            const std::array<double,4> p{x,1.25,z,1.0};
            const auto ref=d3d(d3d(p,view),proj),got=column(t,p);
            for(unsigned k=0;k<4;++k)CHECK(std::fabs(ref[k]-got[k])<=1e-3*(1.0+std::fabs(ref[k])));
        }
        // The eye maps to w == 0 (RH projection: w = -z_view); a point on the
        // look direction is in front of the camera with depth inside [0,1].
        const std::array<double,4> eye{c.f32(0xf8),c.f32(0xfc),c.f32(0x100),1.0};
        CHECK(std::fabs(column(t,eye)[3])<1e-3);
        const float pitch=c.f32(0x128),yaw=c.f32(0x12c);
        const std::array<double,4> ahead{eye[0]-10.0*std::sin(yaw)*std::cos(pitch),eye[1]+10.0*std::sin(pitch),
                                         eye[2]-10.0*std::cos(yaw)*std::cos(pitch),1.0};
        const auto clip=column(t,ahead);
        CHECK(clip[3]>0.0&&clip[2]/clip[3]>=0.0&&clip[2]/clip[3]<=1.0);
        CHECK(std::fabs(clip[0]/clip[3])<0.05&&std::fabs(clip[1]/clip[3])<0.05);
        // Normals use the view rotation only.
        CHECK(t.normal[0]==at(view,0,0)&&t.normal[6]==at(view,1,2)&&t.normal[12]==0.f&&t.normal[15]==1.f);
        std::array<std::uint8_t,64> world{};Bytes w(world.data(),64);
        for(unsigned k=0;k<4;++k)w.putf(k*20,1.f);w.putf(0x30,2.f);
        platform::MeshPreviewTransform o{};
        CHECK(platform::camera_object_transform(world,view,proj,o));
        const auto moved=column(o,{0,0,0,1}),direct=column(t,{2,0,0,1});
        for(unsigned k=0;k<4;++k)CHECK(std::fabs(moved[k]-direct[k])<=1e-4*(1.0+std::fabs(direct[k])));
    }
    std::array<std::uint8_t,64> bad{};Bytes(bad.data(),64).put32(0,0x7fc00000u);
    platform::MeshPreviewTransform keep{};keep.position[0]=42.f;
    CHECK(!platform::camera_scene_transform(bad,bad,keep)&&keep.position[0]==42.f);
    // 46C140 draw command.
    v.putf(0x58,0.5f);for(unsigned k=0;k<16;++k)v.putf(0xb0+k*4,float(k));
    const auto display=platform::vehicle_display_46c140(v);
    CHECK(display.model==9&&display.colour_scale==0.5f&&at(display.world,3,3)==15.f);
    std::printf("camera render binding: %u checks\n",checks);
}
