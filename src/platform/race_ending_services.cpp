#include "platform/race_ending_services.hpp"
#include "driving/pc_matrix_stack.hpp"
#include <cmath>
#include <cstring>
namespace outrun::platform {
namespace {
std::uint32_t light_record(std::uint32_t a,std::uint32_t b,std::uint32_t c){
    if(b==0u)return 0x899b98u+(a+c)*0xa0u;
    if(b==1u)return 0x89a138u+(c+a*2u)*0xa0u;
    return 0x899d78u+(c+a*2u)*0xa0u;
}
float bf(std::uint32_t v){float f;std::memcpy(&f,&v,4);return f;}
}
bool ending_light_service(PcRaceMemory& m,driving::PcMatrixStack& st,const PcRaceCall& k){
    const auto* a=k.args.data();
    switch(k.pc){
    case 0x407cf0u:{const auto o=light_record(a[0],a[1],a[2]);m.put32(o+8u,a[3]);m.put32(o+0xcu,a[4]);m.put32(o+0x10u,a[5]);m.put32(o+0x14u,0x3f800000u);return true;}
    case 0x407d60u:m.put32(light_record(a[0],a[1],a[2])+0x90u,a[3]);return true;
    case 0x407dd0u:{const auto o=light_record(a[0],a[1],a[2]);m.put32(o+0x18u,a[3]);m.put32(o+0x1cu,a[4]);m.put32(o+0x20u,a[5]);m.put32(o+0x24u,0u);return true;}
    case 0x407e40u:{const auto o=light_record(a[0],a[1],a[2]);m.put32(o+0x28u,a[3]);m.put32(o+0x2cu,a[4]);m.put32(o+0x30u,a[5]);m.put32(o+0x34u,0u);return true;}
    case 0x407f40u:{   // +84 / +88 the angles; +44 = 44A430: unit, rotate y (+88), rotate x (+84), (0, 0, [6280C4]) by 40A820
        const auto o=light_record(a[0],a[1],a[2]);m.put32(o+0x84u,a[3]);m.put32(o+0x88u,a[4]);
        driving::pc_matrix_push_unit(st);driving::pc_matrix_rotate_y(st,bf(a[4]));driving::pc_matrix_rotate_x(st,bf(a[3]));
        const auto v=driving::pc_matrix_vector(st,driving::CourseProbe{0.0f,0.0f,m.f32(0x6280c4u)});
        driving::pc_matrix_pop(st);
        m.putf(o+0x44u,v.x);m.putf(o+0x48u,v.y);m.putf(o+0x4cu,v.z);return true;}
    case 0x408030u:m.put32(light_record(a[0],a[1],a[2])+0x8cu,a[3]);return true;
    case 0x4080a0u:m.put32(light_record(a[0],a[1],a[2])+0x50u,a[3]);return true;
    case 0x408390u:{const std::uint32_t o=0x899b98u+a[0]*0xa0u;m.put32(o+0x6cu,a[1]);m.put32(o+0x70u,a[2]);m.put32(o+0x74u,a[3]);return true;}
    case 0x4083c0u:{const std::uint32_t o=0x899b98u+a[0]*0xa0u;m.put32(o+0x78u,a[1]);m.put32(o+0x7cu,a[2]);m.put32(o+0x80u,a[3]);return true;}
    case 0x451800u:{const std::uint32_t o=0x7d3a10u+a[0]*0x1cu;m.put32(o+8u,a[2]);m.put32(o+0xcu,a[3]);m.put32(o+4u,a[1]);m.put32(o+0x10u,a[4]);return true;}
    default:return false;
    }
}
void camera_set_483e10(PcRaceMemory& m,std::uint32_t eye,std::uint32_t target,std::uint32_t roll,std::uint32_t fov){
    const std::uint32_t cam=m.u32(0x79f574u);
    if(eye)for(std::uint32_t q=0;q<12u;q+=4)m.put32(cam+0xe0u+q,m.u32(eye+q));
    if(target)for(std::uint32_t q=0;q<12u;q+=4)m.put32(cam+0xecu+q,m.u32(target+q));
    if(roll)m.put32(cam+0x130u,m.u32(roll));
    if(fov)m.put32(cam+0xa4u,m.u32(fov));
    for(std::uint32_t q=0;q<12u;q+=4)m.put32(cam+0xd4u+q,m.u32(cam+0xe0u+q));
}
void robot_matrix_487d10(PcRaceMemory& m,driving::PcMatrixStack& s,std::uint32_t work,std::uint32_t matrix){
    if(matrix){for(std::uint32_t q=0;q<0x40u;q+=4)m.put32(work+0x10u+q,m.u32(matrix+q));return;}
    driving::pc_matrix_get(s,m.bytes(work+0x10u,0x40));              // 40A0D0
}
void robot_flag_487d40(PcRaceMemory& m,std::uint32_t work,std::uint32_t on){
    const std::uint32_t f=m.u32(work+4u);m.put32(work+4u,on?(f|8u):(f&0xfffffff7u));
}
void rob_disp_word_514800(PcRaceMemory& m,std::uint32_t work,std::uint32_t value,std::uint32_t which){
    m.put32(m.u32(work)*0x60u+0x8577d8u+(which?0x5cu:0x58u),value);
}
void rob_disp_vector_514880(PcRaceMemory& m,std::uint32_t work,std::uint32_t vector,std::uint32_t which){
    const std::uint32_t o=m.u32(work)*0x60u+0x8577d8u+(which?0x44u:0x38u);
    for(std::uint32_t q=0;q<12u;q+=4)m.put32(o+q,m.u32(vector+q));
}
void rob_disp_model_514830(PcRaceContext& c,std::uint32_t work,std::uint32_t model,std::uint32_t frame){
    auto& m=c.m;
    const std::uint32_t d=m.u32(work)*0x60u+0x8577d8u;
    if(!((m.u32(d+0x14u)^model)&0xffff0000u)){m.put32(d+0x14u,m.u32(d+0x10u));m.put32(d+0x10u,model);m.put32(d+4u,frame);m.put32(d+8u,frame);}
    if(model==0xffffffffu)return;
    PcRaceCall k;k.argc=2;k.args[0]=model;
    k.pc=0x4066d0u;k.args[1]=2u;(void)c.service(k);
    k.pc=0x4103f0u;k.args[1]=1u;(void)c.service(k);
}
std::uint32_t navi_score_mean_45bf30(PcRaceMemory& m,std::uint32_t n){
    std::int32_t sum=0;
    for(std::int32_t i=0;i<5;++i){
        std::int32_t v=0;
        if(!(std::int32_t(n)<i)){
            std::int32_t d=std::int8_t(m.u8(0x7f2588u+std::uint32_t(i)*0x44u));if(d<=0)d=1;
            v=m.i32(0x7f2564u+std::uint32_t(i)*0x44u)/d;
            if(!(v>-1))v=0;
        }
        sum=std::int32_t(std::uint32_t(sum)+std::uint32_t(v));
    }
    return std::uint32_t(sum/5);
}
std::uint32_t rob_motion_word_48f4e0(PcRaceMemory& m,std::uint32_t motion){return m.u32(motion+4u);}
void rob_motion_speed_4ed890(PcRaceMemory& m,std::uint32_t motion,std::uint32_t speed){m.put32(motion+0x18u,speed);}
void rob_motion_frame_end_4f1e30(PcRaceMemory& m,std::uint32_t motion,float f){
    const float end=m.f32(motion+0x14u);
    if(f>end)f=end;                                                      // comiss / jbe: unordered keeps the frame
    if(!(0.0f<f)&&!std::isnan(f))f=end;                                  // comiss 0 / jb: unordered keeps it
    m.putf(motion+0x1cu,f);
}
}
