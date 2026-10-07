#include "enhancements/frame_rate.hpp"
#include "platform/pc_vehicle_display.hpp"
#include "platform/pc_screen.hpp"
#include "driving/pc_x87.hpp"
#include <cmath>
#include <cstring>
#include <stdexcept>
namespace outrun::platform {
using driving::Bytes;
namespace {
inline float mulf(float a,float b){volatile float x=a*b;return x;}
}
void vehicle_draw_calls_execute(PcVehicleDisplayContext& c,const std::vector<PcVehicleDrawCall>& calls){
    auto& q=c.queue;
    bool darkened=false;for(const auto& call:calls)darkened|=call.pc==0x422740u;
    for(const auto& call:calls){
        const auto& a=call.args;
        auto load_matrix=[&]{
            if(q.matrices.depth>=q.matrices.capacity)throw std::runtime_error("vehicle draw matrix stack");
            for(unsigned k=0;k<64;++k)q.matrices.current().put8(k,call.matrix[k]);
        };
        switch(call.pc){
        case 0x405360u:
            load_matrix();
            render_object_405360(q,a[0],std::int32_t(a[1]),Bytes(nullptr,0),a[3],std::int32_t(a[4]),std::int32_t(a[5]));
            break;
        case 0x4056d0u:
            load_matrix();
            render_object_override_4056d0(q,a[0],a[1],a[2],std::int32_t(a[3]));
            break;
        case 0x4044f0u:render_pass_record_4044f0(c.flush.g,a[0],a[1],a[2],a[3],a[4]);break;
        case 0x404540u:render_pass_defaults_404540(c.flush.g,c.layer_7d25f0);break;
        case 0x4052b0u:render_queue_mode_4052b0(q);break;
        case 0x4052c0u:render_queue_flush_4052c0(c.flush,q);break;
        case 0x405350u:{
            std::vector<std::uint32_t> order(q.opaque.count);
            for(std::uint32_t k=0;k<q.opaque.count;++k)order[k]=k;
            render_queue_flush_405890(c.flush,q.opaque,order);break;}
        case 0x422550u:case 0x422740u:
            if(call.pc==0x422550u&&!darkened&&g_pc_skip_unread_shadow_volumes)break;
            if(c.shadow){load_matrix();if(c.shadow(call))break;}
            if(!c.unported)throw std::runtime_error("shadow volume leaf without a report");
            c.unported(call.pc);break;
        default:throw std::runtime_error("46AE70 draw list leaf not handled");
        }
    }
}
void vehicle_display_46c140(PcVehicleDisplayContext& c,Bytes car){
    Bytes sun=c.environment.lights_899b98.sub(0x899cd8u-0x899b98u,0xa0);
    std::array<std::uint8_t,16> saved{};
    for(unsigned k=0;k<16;++k)saved[k]=sun.u8(8+k);
    const float scale=car.f32(0x58);
    sun.putf(8,mulf(sun.f32(8),scale));
    sun.putf(0xc,mulf(scale,sun.f32(0xc)));
    sun.putf(0x10,mulf(scale,sun.f32(0x10)));
    render_environment_40d840(c.flush,2,c.flags_79fcca,c.environment);
    driving::pc_matrix_push_load(c.matrices,car.sub(0xb0,64));
    PcVehicleModelDraw in{};
    in.car=car;in.model=std::int32_t(car.i8(0x11));in.variant=0;in.body_82e7f0=c.body_82e7f0;
    in.game_mode_78026c=c.game_mode_78026c;in.scene_82e7d4=c.scene_82e7d4;
    std::vector<PcVehicleDrawCall> calls;
    vehicle_model_draw_46ae70(in,c.matrices,calls);
    vehicle_draw_calls_execute(c,calls);
    driving::pc_matrix_pop(c.matrices);
    for(unsigned k=0;k<16;++k)sun.put8(8+k,saved[k]);
    render_environment_40d840(c.flush,2,c.flags_79fcca,c.environment);
}
namespace {
driving::X87 x87_sin(float v){return driving::x87_sin(driving::X87(v));} // 449360: fld dword; fsin
}
void vehicle_race_display_46bd30(PcVehicleDisplayContext& c,PcRaceCarDisplayState& st,const PcRaceCarDisplayInputs& in,
    Bytes car,const std::function<void(std::uint32_t)>& unported){
    Bytes sun=c.environment.lights_899b98.sub(0x899cd8u-0x899b98u,0xa0);
    const float brightness=car.f32(0x58);
    for(unsigned k=0;k<16;++k)st.diffuse_7f944c[k]=sun.u8(8+k);
    bool pulse=false;
    if(in.network_7f9460_60!=0u){
        for(unsigned k=0;k<16;++k){st.specular_7f943c[k]=sun.u8(0x18+k);st.ambient_7f942c[k]=sun.u8(0x28+k);}
        if(car.u32(0xd10)==1u){
            pulse=true;
            if(in.pause_780248==0u&&enhancements::display_ticks())st.phase_7f9428=st.phase_7f9428+0.188495517f;       // 5B3A2C (port: not on display-only frames)
            const float v=float(x87_sin(st.phase_7f9428));const float w=mulf(v,0.f);             // 619A34
            for(unsigned o:{8u,0x18u,0x28u}){sun.putf(o,v);sun.putf(o+4,w);sun.putf(o+8,w);}
            render_lights_reset_410740(c.flush);render_light_add_4107a0(c.flush,sun);
        }
    }
    sun.putf(8,mulf(sun.f32(8),brightness));sun.putf(0xc,mulf(sun.f32(0xc),brightness));sun.putf(0x10,mulf(sun.f32(0x10),brightness));
    render_environment_40d840(c.flush,2,c.flags_79fcca,c.environment);
    driving::pc_matrix_push_load(c.matrices,car.sub(0xb0,64));
    if(in.network_7f9460_60!=0u&&in.network_800abc!=0u){                              // 46C550
        if(car.i32(0xd18)>0){
            float x=st.phase_7f9424;
            if(c.game_mode_78026c==0x10&&enhancements::display_ticks()){x=x+1.f;st.phase_7f9424=x;}   // port: not on display-only frames
            x=mulf(mulf(x,0.0222222228f),6.28318548f);                                 // 628268, 5A29DC
            const float t=driving::x87_float(x87_sin(x)*driving::X87(0.0942477807f)); // fmul 5B3A28
            driving::pc_matrix_rotate_y(c.matrices,t);driving::pc_matrix_rotate_z(c.matrices,t);
            driving::pc_matrix_translate_vector(c.matrices,{0.f,mulf(float(car.i32(0xd18)),0.0333333351f),0.f});
        }else st.phase_7f9424=0.f;
    }
    PcRaceModelDraw draw{};
    draw.car=car;draw.model=std::int32_t(car.i8(0x11));draw.variant=0;draw.body_82e7f0=c.body_82e7f0;
    draw.game_mode_78026c=c.game_mode_78026c;draw.scene_82e7d4=c.scene_82e7d4;
    std::vector<PcVehicleDrawCall> calls;
    vehicle_race_model_draw_469600(draw,c.matrices,calls);
    c.unported=unported;
    vehicle_draw_calls_execute(c,calls);
    c.unported=nullptr;
    driving::pc_matrix_pop(c.matrices);
    for(unsigned k=0;k<16;++k)sun.put8(8+k,st.diffuse_7f944c[k]);
    render_environment_40d840(c.flush,2,c.flags_79fcca,c.environment);
    if(pulse)for(unsigned k=0;k<16;++k){sun.put8(8+k,st.diffuse_7f944c[k]);sun.put8(0x18+k,st.specular_7f943c[k]);sun.put8(0x28+k,st.ambient_7f942c[k]);}
}
}
