#include "platform/pc_scene_environment.hpp"
#include "driving/pc_x87.hpp"
#include <cstring>
#include <stdexcept>
namespace outrun::platform {
using driving::Bytes;
namespace {
// x87 register value: every arithmetic step rounds to the precision control
// (24-bit after the Direct3D device is created without FPU_PRESERVE).
using X=driving::X87;
constexpr std::uint32_t kOne=0x3f800000u;
// 44A910: gate class of the root modes 3..0x24 (0 bridge, 1 update, 2 timer, 3 fog off).
constexpr std::uint8_t kModeGate44a910[34]{0,3,3,3,3,3,3,1,3,3,2,2,1,1,1,1,1,1,1,3,1,1,3,3,1,1,3,3,3,1,1,1,1,1};
Bytes lights(PcSceneEnvironment& e){return Bytes(e.lights_899b98.data(),e.lights_899b98.size());}
Bytes record(PcSceneEnvironment& e,std::uint32_t address){return lights(e).sub(address-0x899b98u,0xa0);}
void direction(Bytes r,float pitch,float yaw,driving::PcMatrixStack& m){
    r.putf(0x84,pitch);r.putf(0x88,yaw);
    const auto v=driving::environment_direction_44a430(pitch,yaw,m);
    r.putf(0x44,v.x);r.putf(0x48,v.y);r.putf(0x4c,v.z);
}
float bits(std::uint32_t u){float f;std::memcpy(&f,&u,4);return f;}
}
void scene_environment_fog_defaults_4518e0(PcSceneEnvironment& e){
    Bytes fog(e.fog_7d3a10.data(),e.fog_7d3a10.size());
    for(unsigned r=0;r<3;++r){
        fog.put32(r*0x1c,0);fog.put32(r*0x1c+0x14,0x00ffffffu);
        for(unsigned k=0;k<3;++k)fog.put8(r*0x1c+0x18+k,0xff);
        e.fog_lists_7d3a00[r]=0;
    }
}
void scene_environment_fog_off_4518c0(PcSceneEnvironment& e){
    Bytes fog(e.fog_7d3a10.data(),e.fog_7d3a10.size());
    for(unsigned r=0;r<3;++r)fog.put32(r*0x1c,0);
}
void scene_environment_lights_407940(PcSceneEnvironment& e,PcD3D9Device& device,driving::PcMatrixStack& m){
    // Three sun lights (directional, enabled, white diffuse/specular).
    for(std::uint32_t a=0x899b98;a<0x899d78;a+=0xa0){
        Bytes r=record(e,a);
        r.put32(0,1);r.put32(4,3);
        direction(r,bits(0x3f490fdbu),bits(0xc016cbe4u),m);
        r.put32(0x8c,0x43960000u);
        for(unsigned o=0x08;o<=0x20;o+=4)r.put32(o,kOne);
        for(unsigned o=0x24;o<=0x34;o+=4)r.put32(o,0);
        r.put32(0x90,kOne);for(unsigned o=0x94;o<=0x9c;o+=4)r.put32(o,0);
    }
    // Six local point lights (899D78): disabled, attenuation 1, 1, 2^-23.
    for(std::uint32_t a=0x899d78;a<0x89a118;a+=0xa0){
        Bytes r=record(e,a);
        r.put32(0,0);r.put32(4,1);r.put32(0x50,kOne);r.put32(0x58,kOne);r.put32(0x5c,kOne);r.put32(0x60,0x34000000u);
        for(unsigned o=0x18;o<=0x34;o+=4)r.put32(o,0);
        r.put32(0x90,kOne);for(unsigned o=0x94;o<=0x9c;o+=4)r.put32(o,0);
    }
    // Six further lights (89A138): disabled, pointing down.
    for(std::uint32_t a=0x89a138;a<0x89a4b8;a+=0xa0){
        Bytes r=record(e,a);
        r.put32(0,0);direction(r,bits(0x3fc90fdbu),0.f,m);
        r.put32(0x90,kOne);for(unsigned o=0x94;o<=0x9c;o+=4)r.put32(o,0);
    }
    // Colours of the three sun records (40DD70 inputs).
    for(std::uint32_t a:{0x899b98u,0x899c38u,0x899cd8u})for(unsigned o=0x6c;o<=0x80;o+=4)record(e,a).put32(o,kOne);
    device.set_render_state(0x89,1);
    e.sun_lists_7d26a8={0,0,0};                                  // 449F80
}
void scene_environment_positions_408310(PcSceneEnvironment& e){
    for(std::uint32_t a:{0x899c38u,0x899cd8u}){
        Bytes r=record(e,a);
        for(unsigned k=0;k<3;++k)r.putf(0x38+k*4,static_cast<float>(-(X(r.f32(0x44+k*4))*X(r.f32(0x8c)))));
    }
}
void scene_environment_init_449fc0(PcSceneEnvironment& e,PcD3D9Device& device,driving::PcMatrixStack& m){
    scene_environment_fog_defaults_4518e0(e);
    scene_environment_lights_407940(e,device,m);
    e.flags_7d28b0={1,1,1,1,1,1};e.phase_7d28c8=3;
}
void scene_environment_control_44a890(PcSceneEnvironment& e,const PcSceneEnvironmentControl& c){
    const std::uint32_t index=c.mode_78026c-3u;
    auto update=[&]{
        if(!c.update_44a8df)throw std::runtime_error("44A890 update without an owner");
        c.update_44a8df();scene_environment_positions_408310(e);};
    auto fog_off=[&]{scene_environment_fog_off_4518c0(e);scene_environment_positions_408310(e);};
    std::uint8_t gate=index>0x21u?3u:kModeGate44a910[index];
    if(gate==0u){
        if(!c.bridge_49eed0)throw std::runtime_error("44A890 needs 49EED0");
        if(c.bridge_49eed0()==1u){fog_off();return;}
        gate=2u;
    }
    if(gate==2u){
        if(!c.timer_49b2d0)throw std::runtime_error("44A890 needs 49B2D0");
        const std::uint16_t t=c.timer_49b2d0();
        if(c.race_mode_780258==3u||c.race_mode_780258==4u){
            if(t==0x168u){scene_environment_positions_408310(e);return;}
            update();return;
        }
        if(t!=0x12cu){update();return;}
        scene_environment_positions_408310(e);return;
    }
    if(gate==1u){update();return;}
    fog_off();
}
}
