#include "platform/race_input.hpp"
#include <cmath>
#include <cstring>
#include <stdexcept>
namespace outrun::platform {
namespace {
struct SteerRow {std::int32_t step,div,step2,div2;float in,out;};
// 5A7C30: nine rows; row 8 is 5A7CF0 (outside the race).
constexpr SteerRow SteerRows[9]{
    {11,4,16,3,0.0599999987f,0.0710000023f},{12,4,17,3,0.0700000003f,0.0825000033f},
    {14,4,18,3,0.0799999982f,0.0939999968f},{15,4,19,3,0.0900000036f,0.105499998f},
    {17,4,20,3,0.100000001f,0.116999999f},{18,4,21,3,0.109999999f,0.129000008f},
    {19,4,22,3,0.120000005f,0.141000003f},{20,4,23,3,0.129999995f,0.152999997f},
    {21,4,25,3,0.140000001f,0.165000007f}};
// 5A7BC0: seven {mask, bits, mask, bits} feature mappings.
constexpr std::uint32_t FeatureMap[7][4]{
    {0x1,0x1,0x200,0x2},{0x2,0x4,0x4,0x8},{0x8,0x10,0x10,0x20},{0x8000000,0x200,0x100000,0x100},
    {0x40,0x400,0x20,0x800},{0x100,0x1000,0x80,0x2000},{0x800,0x4000,0x400,0x8000}};
const SteerRow& row(std::int8_t option){
    if(option<0||option>=9)throw std::out_of_range("7C24CB steering option outside 5A7C30");
    return SteerRows[option];
}
std::int32_t cvttss2si(float v){
    if(!(v>-2147483904.f&&v<2147483648.f))return std::int32_t(0x80000000u);
    return std::int32_t(v);
}
// Axis word -> [-1,1] with the dead zone (x87 fild * 1/32768, SSE rest).
float axis(std::int16_t word,float dz){
    const long double v=(long double)std::int32_t(word)*(long double)3.0517578125e-05f;
    const float vf=float(v);
    if((long double)dz>std::fabs(v))return 0.f;
    float x=vf;x=vf>0.f?x-dz:x+dz;
    return x/(1.f-dz);
}
std::int16_t word_at(const PcInputDevice& d,std::uint32_t index){
    if(index>=d.axes_94.size())throw std::out_of_range("input axis index outside the device record");
    return d.axes_94[index];
}
}
PcInputConfig pc_input_keyboard_config(){return {0xe,0xd,0xc,0x2,0x4,0x10,0x0};}
PcInputConfig pc_input_joystick_config(unsigned index){
    static constexpr PcInputConfig t[4]{{0xe,0xd,0xc,0x2,0x8,0x10,0x1},{0xe,0xd,0xc,0x8,0x2,0x10,0x1},
                                        {0xe,0x17,0x18,0x2000,0x1000,0x10,0x1},{0xe,0x1,0x3,0x2000,0x1000,0x10,0x1}};
    if(index>=4)throw std::out_of_range("7D6880 joystick configuration outside 5A7B50");
    return t[index];
}
void pc_input_switch_453640(PcInputSwitchRecord& r,const PcInputDevice& d,const PcInputConfig& cfg){
    const std::uint32_t b=d.buttons_04;std::uint32_t v=0;
    for(const auto& e:FeatureMap){if(e[0]&b)v|=e[1];if(e[2]&b)v|=e[3];}
    if(cfg.shift_up_mask&b)v|=0x80u;
    if(cfg.shift_down_mask&b)v|=0x40u;
    if(cfg.view_mask&b)v|=0x40000u;
    const std::uint32_t old=r.held;
    r.held=v;r.previous=old;r.pressed=v&~old;r.released=old&~v;
}
std::int32_t pc_input_digital_steer_4537c0(std::int32_t target,std::int32_t prev,std::int8_t option){
    const auto& t=row(option);
    const std::int32_t d=target-prev;
    if(d==0)return target;
    if(std::int32_t(std::uint32_t(target)*std::uint32_t(prev))>=0&&std::abs(target)>std::abs(prev)){
        std::int32_t step=d;
        if(d<-t.step)step=-t.step;else if(d>t.step)step=t.step;
        if(prev!=0)return step+prev;
        const std::int32_t q=step/t.div;                                        // IDIV replaces EAX
        if(q!=0)return q+prev;
        return (d>0?1:-1)+prev;
    }
    std::int32_t step=d/t.div2;
    if(step==0)step=d>0?1:-1;
    if(step<-t.step2)step=-t.step2;else if(step>t.step2)step=t.step2;
    return step+prev;
}
void pc_input_analog_453860(PcInputAnalog& a,float& filter,const PcInputDevice& d,const PcInputConfig& cfg,
                            std::int8_t option,std::int32_t game_mode){
    for(unsigned k=0;k<7;++k)a.previous[k]=a.current[k];
    a.current[1]=std::int32_t(std::uint16_t(word_at(d,cfg.accel_axis)));
    a.current[2]=std::int32_t(std::uint16_t(word_at(d,cfg.brake_axis)));
    a.accel_edge_1c=((a.previous[1]^a.current[1])&a.current[1]&0xffffff80)!=0?1:0;
    a.brake_edge_2c=((a.previous[2]^a.current[2])&a.current[2]&0xffffff80)!=0?1:0;
    const float dz=(cfg.flags&1u)?0.0078125f:0.200000003f;                       // 62817C / 5B0068
    const float analog=axis(word_at(d,cfg.steer_axis),dz);
    const auto& t=game_mode==16?row(option):SteerRows[8];
    float x=filter;
    if(d.buttons_04&0x80u){x=t.in+filter;if(x>1.f)x=1.f;}
    else if(d.buttons_04&0x100u){x=filter-t.in;if(-1.f>x)x=-1.f;}
    else if(filter>0.f){x=filter-t.out;if(0.f>x)x=0.f;}
    else if(0.f>filter){x=t.out+filter;if(x>0.f)x=0.f;}
    filter=x;
    float steer=analog;
    if(std::fabs((long double)x)>std::fabs((long double)analog))steer=x;
    a.current[0]=cvttss2si(steer*127.f);
    a.current[3]=cvttss2si(axis(d.axes_94[(0xb0 - 0x94)/2],dz)*127.f);
    a.current[4]=cvttss2si(axis(d.axes_94[(0xb2 - 0x94)/2],dz)*127.f);
    a.current[5]=cvttss2si(axis(d.axes_94[(0xbe - 0x94)/2],dz)*127.f);
    a.current[6]=cvttss2si(axis(d.axes_94[(0xc0 - 0x94)/2],dz)*127.f);
    if(!(cfg.flags&1u))a.current[0]=pc_input_digital_steer_4537c0(a.current[0],a.previous[0],option);
}
}
