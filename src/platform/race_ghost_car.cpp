// Time Attack ghost cars (events 9..12, car kind 59C554): 4407E0 opening,
// 4ACFC0/4ACFB0 queue 841FA8, init 4AD000 (+46F350), control 4ACE40 with
// the playback 467E30 (4661C0, 4661F0, 4F6290, 454FC0, 467800) and 466250,
// destroy 470560. Transliteration over PC addresses like race_ghosts.cpp.
#include "enhancements/frame_rate.hpp"
#include "system/exe_image.hpp"
#include "platform/race_ghosts.hpp"
#include "platform/pc_address_view.hpp"
#include "platform/vehicle_constructor.hpp"
#include "platform/vehicle_constructor_data.hpp"
#include "driving/pc_car_services_ghost.hpp"
#include "driving/pc_x87.hpp"
#include <cmath>
#include <cstring>
#include <limits>
#include <stdexcept>
namespace outrun::platform {
namespace {
using driving::X87;
std::uint32_t call(PcRaceContext& c,std::uint32_t pc,std::initializer_list<std::uint32_t> args,std::uint32_t eax=0,std::uint32_t ecx=0){
    PcRaceCall k;k.pc=pc;k.eax=eax;k.ecx=ecx;k.argc=std::uint32_t(args.size());
    std::size_t i=0;for(auto a:args)k.args[i++]=a;
    return c.service(k);
}
float bits(std::uint32_t u){float f;std::memcpy(&f,&u,4);return f;}
const float K0_5=bits(0x3f000000u),K1=bits(0x3f800000u),K2=bits(0x40000000u),K0_1=bits(0x3dcccccdu),K0_0625=bits(0x3d800000u);
const float KAngle=bits(0x38c90fdbu);          // 628254: 2pi/65536
const float KAngleInv=bits(0x4622f983u);       // 6282C0: 65536/2pi
const float KDistance=bits(0x46600000u);       // 5B041C: 14336
const float KFar=bits(0x3f400000u),KNear=bits(0x3e800000u),KStep=bits(0x3bd9ba42u);   // 6282A0, 628088, 5C3730
const float KMax=bits(0x7f7fffffu),KMin=bits(0xff7fffffu),K100000=bits(0x47c35000u);  // 59943C, 5B4370, 5B436C
// MSVC _ftol2 (582194): low dword of the 64-bit truncation.
std::uint32_t ftol2(X87 v){return std::uint32_t(std::uint64_t(driving::x87_ftol64(v)));}
// CVTTSS2SI / CVTSI2SS
std::int32_t cvtt(float v){
    if(!(v>-2147483904.0f&&v<2147483648.0f))return std::numeric_limits<std::int32_t>::min();
    return std::int32_t(v);
}
// COMISS a,b then JBE: taken when a <= b or unordered.
bool jbe(float a,float b){return std::isnan(a)||std::isnan(b)||a<=b;}
// Local stack frame of 467E30 (L = ESP after the three register pushes).
struct Frame {
    std::array<std::uint8_t,0xb0> bytes{};
    PcRaceMemory& m;std::size_t mark;std::uint32_t base;
    Frame(PcRaceMemory& mm,std::uint32_t at):m(mm),mark(mm.mark()),base(at){m.map(at,bytes.data(),bytes.size());}
    ~Frame(){m.release(mark);}
    std::uint32_t at(std::uint32_t o)const{return base+o;}
};
constexpr std::uint32_t Local467e30=0x7ffe1000u;
// 4661C0 (EAX = first packet, EDX = count): address of packet EDX.
std::uint32_t packet_4661c0(const PcRaceMemory& m,std::uint32_t p,std::int32_t count){
    for(std::int32_t i=0;i<count;++i){
        if(m.u8(p)&1u)p+=i?0x10u:0x1cu;else p+=i?0x8u:0x14u;
    }
    return p;
}
// 467800 (EAX = b, EDX = a, [esp+4] = out, XMM0 = t; VM at 40EC0F = mov ebx,3).
void angles_467800(PcRaceMemory& m,std::uint32_t b,std::uint32_t a,std::uint32_t out,float t){
    for(std::uint32_t k=0;k<3u;++k){
        const std::uint32_t wa=m.u16(a+k*2u),wb=m.u16(b+k*2u);
        const std::int32_t d=std::int16_t(std::uint16_t(wb-wa));
        const float x=float(d)*t;
        m.put16(out+k*2u,std::uint16_t(std::uint32_t(cvtt(x))+wa));
    }
}
// 4661F0 (EAX = old position, ESI = car): travelled distance words +40/+44.
void distance_4661f0(PcRaceMemory& m,std::uint32_t old,std::uint32_t car){
    const float dz=m.f32(car+0x1c)-m.f32(old+8),dy=m.f32(car+0x18)-m.f32(old+4),dx=m.f32(car+0x14)-m.f32(old);
    float s=dz*dz;const float y2=dy*dy,x2=dx*dx;s=s+y2;s=s+x2;
    const X87 r=driving::x87_sqrt(X87(s))*X87(KDistance);
    const auto ax=std::uint16_t(ftol2(r));
    m.put16(car+0x40,std::uint16_t(m.u16(car+0x40)+ax));
    m.put16(car+0x44,std::uint16_t(m.u16(car+0x44)+ax));
}
// 454FC0(spline, time): cubic a x^3 + b x^2 + c x + d, x = time - t0 (x87).
X87 spline_454fc0(const PcRaceMemory& m,std::uint32_t s,std::uint32_t time);
X87 spline_454fc0_impl(const PcRaceMemory& m,std::uint32_t s,std::uint32_t time){
    const float x=float(std::int32_t(time-m.u32(s+0x10)));
    const float x2=x*x;
    X87 r=X87(x2)*X87(x)*X87(m.f32(s));
    r=r+X87(x2)*X87(m.f32(s+4));
    r=r+X87(x)*X87(m.f32(s+8));
    return r+X87(m.f32(s+0xc));
}
// 4F6290(spline, value, time): fit the cubic through the new sample and the
// three previous ones (when one exists), then shift the history.
void spline_4f6290(PcRaceMemory& m,std::uint32_t S,float V,std::uint32_t T);
void spline_4f6290_impl(PcRaceMemory& m,std::uint32_t S,float V,std::uint32_t T){
    if(m.u32(S+0x14)){
        const float x1=float(std::int32_t(m.u32(S+0x20)-T));   // xmm4
        const float x2=float(std::int32_t(m.u32(S+0x24)-T));   // xmm5
        const float x2_2=x2*x2;                                // xmm2
        const float x0=float(std::int32_t(m.u32(S+0x10)-T));
        const float x2_3=x2_2*x2;                              // s40
        const float x0_2=x0*x0;                                // xmm3
        const float x0_3=x0_2*x0;                              // s10
        const float s04=m.f32(S+0x18)-V,s2c=m.f32(S+0x1c)-V,s0c=m.f32(S+0xc)-V;
        const float x1_2=x1*x1;                                // xmm1
        const float x1_3=x1_2*x1;                              // s30
        const float s18=x2_2*x1_3,s24=x0_2*x1_3,s1c=x0_3*x2_2,s20=x0_3*x1_2,s28=x0_2*x2_3;
        float xmm0=x2_3*x1_2;
        const float s14a=xmm0*x0;
        float det=s14a-s18*x0;
        det=det+s1c*x1;
        det=det+s24*x2;
        const float t20=s20*x2;
        const float t28=s28*x1;
        det=det-t20;det=det-t28;
        const float inv=K1/det;                                // s14
        float a=x0_2*s04;
        a=a-x1_2*s0c;
        a=a*x2;
        a=a-x2_2*x0*s04;
        const float x1_2x0=x1_2*x0;                            // xmm1
        float x2_2x1=x2_2*x1;x2_2x1=x2_2x1*s0c;
        a=a+x2_2x1;
        a=a+x1_2x0*s2c;
        a=a-x0_2*x1*s2c;
        a=a*inv;
        m.putf(S,a);
        float b=x2*s0c;
        b=b-x0*s2c;
        b=b*x1_3;
        b=b+x0_3*x1*s2c;
        const float x2_3x1=x2_3*x1;
        b=b+x2_3*x0*s04;
        b=b-x0_3*x2*s04;
        b=b-x2_3x1*s0c;
        xmm0=xmm0*s0c;
        xmm0=xmm0-s18*s0c;
        xmm0=xmm0+s1c*s04;
        xmm0=xmm0-s20*s2c;
        xmm0=xmm0+s24*s2c;
        xmm0=xmm0-s28*s04;
        b=b*inv;xmm0=xmm0*inv;
        m.putf(S+4,b);m.putf(S+8,xmm0);
    }
    m.put32(S+0x18,m.u32(S+0x1c));m.put32(S+0x1c,m.u32(S+0xc));
    m.put32(S+0x20,m.u32(S+0x24));m.put32(S+0x24,m.u32(S+0x10));
    m.put32(S+0x14,m.u32(S+0x14)+1u);
    m.putf(S+0xc,V);m.put32(S+0x10,T);
}
X87 spline_454fc0(const PcRaceMemory& m,std::uint32_t s,std::uint32_t time){return spline_454fc0_impl(m,s,time);}
void spline_4f6290(PcRaceMemory& m,std::uint32_t S,float V,std::uint32_t T){spline_4f6290_impl(m,S,V,T);}
void word_angle(PcRaceMemory& m,std::uint32_t dst,std::uint32_t word){   // 4493A0 * 628254, FSTP
    m.putf(dst,driving::x87_float(X87(std::int32_t(std::int16_t(m.u16(word))))*X87(KAngle)));
}
}

void ghost_spline_fit_4f6290(PcRaceMemory& m,std::uint32_t spline,float value,std::uint32_t time){spline_4f6290_impl(m,spline,value,time);}
float ghost_spline_454fc0(const PcRaceMemory& m,std::uint32_t spline,std::uint32_t time){return driving::x87_float(spline_454fc0_impl(m,spline,time));}

std::uint32_t ghost_play_467e30(PcRaceContext& c,std::uint32_t car,std::uint32_t slot){
    auto& m=c.m;
    const std::uint32_t mode=m.u32(0x78026cu);
    if(mode!=0x10u&&mode!=0x12u)return 1u;
    const std::uint32_t variant=m.u32(0x780258u);
    if(variant!=0u&&variant!=7u&&!(call(c,0x4962a0u,{})&0xffu))return 0u;
    if(std::int32_t(slot)>=3)return 0u;
    if(std::int16_t(call(c,0x49b2d0u,{}))>0x3c){
        m.put32(0x7f91e4u+slot*4u,0);
        const std::uint32_t g=m.u32(0x7f9224u)+slot*PcGhostRecordBytes;
        m.put8(car+0x11,m.u8(g+0x11bu));m.put8(car+0x12,m.u8(g+0x11cu));
        m.put32(car+0xc5c,m.u32(car+0xc5c)&0xffffdfffu);
        m.put16(car+0x260,1);m.put16(car+0x262,1);m.putf(car+0xb68,0.f);
        return 1u;
    }
    m.put32(car+0xc5c,m.u32(car+0xc5c)|0x2000u);
    const std::uint32_t g=slot*PcGhostRecordBytes+m.u32(0x7f9224u);
    m.put8(0x64bfecu,1);
    if(m.u32(g)!=0x544f484eu)return 0u;
    const std::uint32_t count=m.u32(g+0xf8u);
    if(!count)return 0u;
    const std::uint32_t index=m.u32(0x7f91e4u+slot*4u);
    if(index>=0xf61u||index>=count)return 1u;
    Frame F(m,Local467e30);auto L=[&](std::uint32_t o){return F.at(o);};
    const std::uint32_t prev=0x7f9390u+slot*0x30u,splines=0x7f8d88u+slot*0x78u;
    const std::uint32_t frac=m.u32(0x7f9220u)&0xfu;
    m.put32(L(0x20),frac);
    if(frac){
        const float t=driving::x87_float(X87(std::int32_t(frac))*X87(K0_0625));
        m.putf(L(0x10),t);
        const std::uint32_t packet=packet_4661c0(m,g+0x120u,std::int32_t(index));
        if(index){
            driving::pc_ghost_delta_decode_4668f0(m.bytes(L(0x30),0x30),m.bytes(packet,0x10));
            driving::pc_ghost_frame_add_466720(m.bytes(L(0x60),0x30),m.bytes(L(0x30),0x30),m.bytes(prev,0x30));
        }else driving::pc_ghost_key_decode_466af0(m.bytes(L(0x60),0x30),m.bytes(packet,0x1c));
        for(std::uint32_t k=0;k<0x30u;k+=4)m.put32(L(0x30+k),m.u32(prev+k));
        m.put32(L(0x24),m.u32(car+0x14));m.put32(L(0x28),m.u32(car+0x18));m.put32(L(0x2c),m.u32(car+0x1c));
        if(frac==1u){
            const std::uint32_t T=m.u32(0x7f9220u)+0xfu;
            spline_4f6290(m,splines,m.f32(L(0x60)),T);
            spline_4f6290(m,splines+0x28u,m.f32(L(0x64)),m.u32(0x7f9220u)+0xfu);
            spline_4f6290(m,splines+0x50u,m.f32(L(0x68)),m.u32(0x7f9220u)+0xfu);
        }
        float xmm3;
        if(std::int32_t(m.u32(splines+0x14u))>3){
            const std::uint32_t time=m.u32(0x7f9220u);
            m.putf(car+0x14,driving::x87_float(spline_454fc0(m,splines,time)));
            m.putf(car+0x18,driving::x87_float(spline_454fc0(m,splines+0x28u,time)));
            m.putf(car+0x1c,driving::x87_float(spline_454fc0(m,splines+0x50u,time)));
            xmm3=m.f32(L(0x10));
        }else{
            const float ax=m.f32(L(0x30)),bx=m.f32(L(0x60));
            float x1=m.f32(L(0x34))+m.f32(L(0x64));
            float x2=m.f32(L(0x38))+m.f32(L(0x68));
            x1=x1*K0_5;x2=x2*K0_5;
            float x0=ax+bx;x0=x0*K0_5;
            xmm3=m.f32(L(0x10));
            x0=x0*xmm3;x0=x0*K2;
            const float u=K1-xmm3;
            x0=x0+u*ax;
            x0=x0*u;
            x0=x0+bx*xmm3*xmm3;
            m.putf(car+0x14,x0);
            x1=x1*xmm3;x1=x1*K2;
            x1=x1+u*m.f32(L(0x34));
            const float by=m.f32(L(0x64))*xmm3*xmm3;
            x2=x2*xmm3;
            x1=x1*u;
            x1=x1+by;
            x2=x2*K2;
            x2=x2+u*m.f32(L(0x38));
            float bz=m.f32(L(0x68))*xmm3;
            x2=x2*u;
            bz=bz*xmm3;
            x2=x2+bz;
            m.putf(car+0x18,x1);m.putf(car+0x1c,x2);
        }
        angles_467800(m,L(0x6c),L(0x3c),car+0x2c,xmm3);
        const std::uint32_t a12=m.u16(L(0x42)),b12=m.u16(L(0x72));
        const std::uint16_t old260=m.u16(car+0x260);
        const float u=K1-xmm3;
        m.putf(L(0x20),u);
        const X87 w=X87(std::int32_t(a12))*X87(u)+X87(std::int32_t(b12))*X87(m.f32(L(0x10)));
        m.put16(car+0x262,old260);
        const std::uint16_t ax=std::uint16_t(ftol2(w));
        float speed=u*m.f32(L(0x44));
        speed=speed+m.f32(L(0x74))*t;
        m.put16(car+0x260,ax);
        m.putf(car+0x2c8,speed);
        if(!jbe(K0_1,speed))m.putf(car+0x2c8,0.f);
        // +2D8..+2E0: the same quadratic on frame +18..+20
        {
            float b18=m.f32(L(0x78));const float a18=m.f32(L(0x48));
            float v2=m.f32(L(0x7c))+m.f32(L(0x4c));
            float v3=m.f32(L(0x80))+m.f32(L(0x50));
            float v1=b18+a18;v1=v1*K0_5;
            v2=v2*K0_5;v3=v3*K0_5;
            v1=v1*t;
            float r7=u*a18;
            v1=v1*K2;
            r7=r7+v1;
            float b1c=m.f32(L(0x7c));
            b18=b18*t;b18=b18*t;
            v2=v2*t;v2=v2*K2;
            r7=r7*u;
            r7=r7+b18;
            b1c=b1c*t;b1c=b1c*t;
            float r5=u*m.f32(L(0x4c));
            r5=r5+v2;
            r5=r5*u;
            r5=r5+b1c;
            float b20=m.f32(L(0x80));
            float r2=u*m.f32(L(0x50));
            v3=v3*t;
            v3=v3*K2;
            r2=r2+v3;
            b20=b20*t;b20=b20*t;
            r2=r2*u;
            r2=r2+b20;
            m.putf(car+0x2d8,r7);m.putf(car+0x2dc,r5);m.putf(car+0x2e0,r2);
        }
        angles_467800(m,L(0x84),L(0x54),L(0x14),t);
        word_angle(m,car+0x2e4,L(0x14));word_angle(m,car+0x2e8,L(0x16));word_angle(m,car+0x2ec,L(0x18));
        angles_467800(m,L(0x8a),L(0x5a),L(0x14),t);
        m.put16(car+0x32,m.u16(L(0x16)));
        distance_4661f0(m,L(0x24),car);
    }else{
        const std::uint32_t packet=packet_4661c0(m,g+0x120u,std::int32_t(index));
        if(index){
            driving::pc_ghost_delta_decode_4668f0(m.bytes(L(0x60),0x30),m.bytes(packet,0x10));
            driving::pc_ghost_frame_add_466720(m.bytes(L(0x30),0x30),m.bytes(L(0x60),0x30),m.bytes(prev,0x30));
        }else driving::pc_ghost_key_decode_466af0(m.bytes(L(0x30),0x30),m.bytes(packet,0x1c));
        for(std::uint32_t k=0;k<0x30u;k+=4)m.put32(prev+k,m.u32(L(0x30+k)));
        m.put32(L(0x24),m.u32(car+0x14));m.put32(L(0x28),m.u32(car+0x18));m.put32(L(0x2c),m.u32(car+0x1c));
        if(m.u32(0x7f9220u)==0u){
            spline_4f6290(m,splines,m.f32(L(0x30)),0);
            spline_4f6290(m,splines+0x28u,m.f32(L(0x34)),m.u32(0x7f9220u));
            spline_4f6290(m,splines+0x50u,m.f32(L(0x38)),m.u32(0x7f9220u));
        }
        m.put32(car+0x14,m.u32(L(0x30)));m.put32(car+0x18,m.u32(L(0x34)));m.put32(car+0x1c,m.u32(L(0x38)));
        m.put16(car+0x2c,m.u16(L(0x3c)));m.put16(car+0x2e,m.u16(L(0x3e)));m.put16(car+0x30,m.u16(L(0x40)));
        m.put32(car+0x2d8,m.u32(L(0x48)));m.put16(car+0x260,m.u16(L(0x42)));
        m.put32(car+0x2dc,m.u32(L(0x4c)));m.put32(car+0x2e0,m.u32(L(0x50)));
        m.put32(car+0x2c8,m.u32(L(0x44)));
        word_angle(m,car+0x2e4,L(0x54));word_angle(m,car+0x2e8,L(0x56));word_angle(m,car+0x2ec,L(0x58));
        m.put16(car+0x32,m.u16(L(0x5c)));
        distance_4661f0(m,L(0x24),car);
        m.put32(0x7f91e4u+slot*4u,m.u32(0x7f91e4u+slot*4u)+1u);
    }
    // 468536
    const std::uint32_t flags=m.u32(car+4);
    m.put32(car+4,!jbe(m.f32(car+0x2c8),0.f)?(flags|0x80000000u):(flags&0x7fffffffu));
    if(!(std::int32_t(m.u32(0x656234u))<0x3c))return 1u;   // 48B350: [656234] < 0x3C (signed)
    const std::uint32_t player=m.u32(0x799d18u);
    if(m.u32(player+0x5c)==0u){
        for(std::uint32_t k=0;k<0x40u;k+=4)m.put32(0x7f9350u+k,m.u32(0x7d2da0u+k));   // 44BEA0
        m.put8(0x7f9340u,1);
    }
    auto& s=c.matrices;
    driving::pc_matrix_push(s);
    driving::pc_matrix_unit_rotation(s);
    driving::pc_matrix_rotate_y(s,driving::x87_float(X87(std::int32_t(std::int16_t(m.u16(car+0x2e))))*X87(KAngle)));
    driving::pc_matrix_rotate_x(s,driving::x87_float(X87(std::int32_t(std::int16_t(m.u16(car+0x2c))))*X87(KAngle)));
    driving::pc_matrix_rotate_z(s,driving::x87_float(X87(std::int32_t(std::int16_t(m.u16(car+0x30))))*X87(KAngle)));
    driving::pc_matrix_get(s,m.bytes(L(0x64),0x40));
    driving::pc_matrix_pop(s);
    driving::pc_matrix_push_load(s,m.bytes(0x7f9350u,0x40));
    {   const auto p=driving::pc_matrix_point(s,{m.f32(car+0x14),m.f32(car+0x18),m.f32(car+0x1c)});
        m.putf(L(0x18),p.x);m.putf(L(0x1c),p.y);m.putf(L(0x20),p.z);}
    driving::pc_matrix_multiply_current(s,m.bytes(L(0x64),0x40));
    driving::pc_matrix_get(s,m.bytes(L(0x64),0x40));
    {   const auto a=driving::pc_matrix_angles_449640(m.bytes(L(0x64),0x40));
        m.putf(L(0x28),a[0]);m.putf(L(0x2c),a[1]);m.putf(L(0x30),a[2]);}
    m.put16(car+0x2c,std::uint16_t(cvtt(m.f32(L(0x28))*KAngleInv)));
    m.put16(car+0x2e,std::uint16_t(cvtt(m.f32(L(0x2c))*KAngleInv)));
    m.put16(car+0x30,std::uint16_t(cvtt(m.f32(L(0x30))*KAngleInv)));
    driving::pc_matrix_pop(s);
    m.put32(car+0x14,m.u32(L(0x18)));m.put32(car+0x18,m.u32(L(0x1c)));m.put32(car+0x1c,m.u32(L(0x20)));
    return 1u;
}

// 466E50 (EAX = car) on the module block: one recorded packet.
void ghost_packet_466e50(PcRaceContext& c,std::uint32_t car){
    auto& m=c.m;
    driving::PcGhostPacketState st{m.u32(0x7f8ef8u),m.u32(0x7f8ef4u),m.u32(0x7f91f0u),m.u32(0x7f8d84u),
        m.bytes(0x7f92c8u,0x30),m.bytes(0x7f9300u,0x40)};
    const std::uint32_t work=m.u32(0x7f9228u);
    driving::PcGhostPacketBuffer buf{m.bytes(work,PcGhostRecordBytes),work};
    driving::PcGhostPacketInputs in{m.u32(m.u32(0x799d18u)+0x5c),m.i32(0x656234u),m.bytes(0x7d2da0u,0x40)};
    driving::pc_ghost_packet_write_466e50(st,buf,m.bytes(car,0x10f0),in,c.matrices);
    m.put32(0x7f8ef8u,st.overflow_7f8ef8);m.put32(0x7f8ef4u,st.count_7f8ef4);m.put32(0x7f91f0u,st.cursor_7f91f0);
}

// 467190 (race manager goal, variant 7): the last packet, once.
void ghost_goal_467190(PcRaceContext& c){
    auto& m=c.m;
    if(m.u32(0x7f91f4u))return;
    const std::uint32_t player=m.u32(0x799d18u);
    if(!(call(c,0x48b310u,{})&0xffu)||!(m.i32(0x656234u)<0x3c))ghost_packet_466e50(c,player);
    m.put32(0x7f91f4u,1);
}

// 466120: rewind the recording (work record +120, counters, splines).
void ghost_record_reset_466120(PcRaceMemory& m){
    const std::uint32_t work=m.u32(0x7f9228u);
    m.put32(0x7f91f0u,work+0x120u);m.put32(0x7f8d84u,work+0x120u);
    m.put32(0x7f91e4u,0);m.put32(0x7f91e8u,0);m.put32(0x7f8ef8u,0);m.put32(0x7f8ef4u,0);m.put32(0x7f91ecu,0);
    m.put32(0x7f9220u,0);m.put8(0x7f922cu,1);
    for(std::uint32_t pose=0x7f8db0u;pose<0x7f8f18u;pose+=0x78u)
        for(std::uint32_t p:{pose-0x28u,pose,pose+0x28u})for(std::uint32_t k=0;k<0x28u;k+=4)m.put32(p+k,0);   // 4F6250
}

// 4662F0 (VM prologue = lea ecx,[edx+120]; EDX = record): record length.
std::uint32_t ghost_length_4662f0(const PcRaceMemory& m,std::uint32_t record){
    const std::uint32_t count=m.u32(record+0xf8u);
    std::uint32_t length=0x120u,p=record+0x120u;
    for(std::uint32_t i=0;i<count;++i){
        const std::uint32_t step=(m.u8(p)&1u)?(i?0x10u:0x1cu):(i?0x8u:0x14u);
        length+=step;p+=step;
    }
    return length;
}

// 4675F0 (BX = course code word, [esp+4] = name, 0x17 bytes): finalize the
// recorded work record (header, sector times, length, CRC).
void ghost_finalize_4675f0(PcRaceContext& c,std::uint16_t code,std::uint32_t name){
    auto& m=c.m;
    if(!m.u32(0x7f8ef4u))return;
    if(std::int32_t(call(c,0x4505d0u,{}))<=0x258)return;
    if(!m.u32(0x7f92b8u)&&std::int32_t(call(c,0x4505d0u,{}))>0xf618)return;
    if(m.u32(0x7f8ef8u))return;
    const std::uint32_t work=m.u32(0x7f9228u);
    for(std::uint32_t k=0;k<0x17u;++k)m.put8(work+0x104u+k,m.u8(name+k));   // 449A60
    m.put32(work,0x544f484eu);
    if(std::int16_t(code)<0)m.put8(work+0x11du,std::uint8_t(call(c,0x450320u,{})));
    else m.put8(work+0x11du,std::uint8_t(code));
    m.put32(work+4u,call(c,0x451180u,{1u}));
    std::uint32_t level=0;
    for(std::uint32_t o=8u;o<0xf8u;++level)
        for(std::uint32_t sector=0;sector<4u;++sector,o+=4u)m.put32(work+o,call(c,0x450610u,{level,sector}));
    m.put32(work+0xf8u,m.u32(0x7f8ef4u));
    ghost_total_time_466460(m,work);
    if(m.u32(0x7f92b8u))ghost_trim_467260(m,work);
    const std::uint32_t scratch=call(c,0x580253u,{0x9e00u});
    const std::uint32_t length=ghost_length_4662f0(m,work);
    m.put32(work+0x100u,length);
    for(std::uint32_t k=0;k<0x9e00u;k+=4)m.put32(scratch+k,0);
    for(std::uint32_t k=0;k<PcGhostRecordBytes;k+=4)m.put32(scratch+k,m.u32(work+k));
    m.put32(scratch+0xfcu,0xffffu);
    const std::uint32_t crc=ghost_crc_449a80(m,scratch,std::int32_t(length));
    m.put32(scratch+0xfcu,crc);m.put32(work+0xfcu,crc);
    (void)call(c,0x580bc2u,{scratch});
}

// 467C60(slot): after a finished run the work record replaces the playback
// record of the slot when faster (new ghost car event slot+9: 4ACFC0 and
// 440180(slot+9, 0x58)); slot 2 keeps the best run. Returns BL.
std::uint8_t ghost_best_467c60(PcRaceContext& c,std::uint32_t slot){
    auto& m=c.m;
    std::uint8_t result=0;
    const std::uint32_t code=call(c,0x48b320u,{});
    ghost_finalize_4675f0(c,std::uint16_t(code),0x7c23e0u);
    std::uint32_t ghosts=m.u32(0x7f9224u),work=m.u32(0x7f9228u);
    const std::uint32_t g=slot*PcGhostRecordBytes;
    const std::uint32_t magic=m.u32(g+ghosts);
    const std::uint32_t best=magic==0x544f484eu?m.u32(g+ghosts+4u):0xffffffffu;
    if(m.u32(work+4u)<best){
        if(magic!=0x544f484eu||m.u32(g+ghosts+4u)!=0u){
            const std::uint32_t sec=slot*0xf0u;
            m.put32(0x7f91d4u,m.u32(0x7f8f00u+sec));m.put32(0x7f91d8u,m.u32(0x7f8f04u+sec));
            m.put32(0x7f91dcu,m.u32(0x7f8f08u+sec));m.put32(0x7f91e0u,m.u32(0x7f8f0cu+sec));
            const std::uint8_t b11b=m.u8(g+ghosts+0x11bu),b11c=m.u8(g+ghosts+0x11cu);
            m.put8(0x7f91d0u,b11b);m.put8(0x7f9204u,b11c);
            for(std::uint32_t k=0;k<0x17u;++k)m.put8(0x7f9208u+k,m.u8(g+ghosts+0x104u+k));
            m.put8(0x7f8ef0u,1);
            (void)call(c,0x424940u,{0xafu});
            work=m.u32(0x7f9228u);ghosts=m.u32(0x7f9224u);
        }
        for(std::uint32_t k=0;k<PcGhostRecordBytes;k+=4)m.put32(g+ghosts+k,m.u32(work+k));
        ghosts=m.u32(0x7f9224u);
        ghost_sections_4662b0(m,g+ghosts,std::int32_t(slot));
        if((m.u8(0x79fb48u+slot+9u)&3u)==2u){(void)call(c,0x440200u,{slot+9u});ghosts=m.u32(0x7f9224u);}
        ghost_car_queue_4acfc0(m,slot+9u,std::uint8_t(slot+1u),std::uint32_t(std::int32_t(std::int8_t(m.u8(g+ghosts+0x11bu)))),m.u8(g+ghosts+0x11cu));
        (void)call(c,0x440180u,{slot+9u,0x58u});
        work=m.u32(0x7f9228u);ghosts=m.u32(0x7f9224u);
        result=1;
    }
    const std::uint32_t s2=ghosts+0x13958u;
    const std::uint32_t t2=m.u32(s2)==0x544f484eu?m.u32(ghosts+0x1395cu):0xffffffffu;
    if(m.u32(work+4u)<t2)for(std::uint32_t k=0;k<PcGhostRecordBytes;k+=4)m.put32(s2+k,m.u32(work+k));
    return result;
}

// 465FA0 (mode 28 / 30 teardown): release the playback and work records.
void ghost_release_465fa0(PcRaceContext& c){
    auto& m=c.m;
    m.put32(0x7f91e4u,0);m.put32(0x7f91e8u,0);m.put32(0x7f91ecu,0);
    std::uint32_t work=m.u32(0x7f9228u);
    m.put32(0x7f91f0u,work+0x120u);m.put32(0x7f8d84u,work+0x120u);
    const std::uint32_t ghosts=m.u32(0x7f9224u);
    for(std::uint32_t a:{0x7f8ef4u,0x7f9220u,0x7f91f4u,0x7f8ef8u,0x7f9230u,0x7f92c4u,0x7f92b8u,0x7f92bcu})m.put32(a,0);
    if(ghosts){call(c,0x580bc2u,{ghosts});work=m.u32(0x7f9228u);m.put32(0x7f9224u,0);}
    if(work){call(c,0x580bc2u,{work});m.put32(0x7f9228u,0);}
}
// 467E00 (race manager, lap restart of a Time Attack run).
void ghost_restart_467e00(PcRaceContext& c){
    ghost_car_queue_reset_4acfb0(c.m);
    if(ghost_best_467c60(c,1))c.m.put32(0x7f92bcu,1);
    ghost_record_reset_466120(c.m);
}

// 4671D0 (CommonPlCar 4A8100): the player's ghost recording. During the
// countdown the work record (7F9228) is rewound and takes the car bytes
// +11/+12; afterwards one 466E50 packet every 16 updates of 7F9220.
void ghost_record_4671d0(PcRaceContext& c,std::uint32_t car){
    auto& m=c.m;
    const std::uint32_t mode=m.u32(0x78026cu);
    if(mode!=0x10u&&mode!=0x12u)return;
    if(m.u32(0x780258u)!=7u&&!(call(c,0x4962a0u,{})&0xffu))return;
    if(std::int16_t(call(c,0x49b2d0u,{}))>0x3c){
        const std::uint32_t work=m.u32(0x7f9228u);
        m.put32(0x7f91f0u,work+0x120u);m.put32(0x7f8d84u,work+0x120u);
        m.put8(work+0x11bu,m.u8(car+0x11));
        m.put32(0x7f8ef4u,0);
        m.put8(work+0x11cu,m.u8(car+0x12));
        return;
    }
    m.put8(0x64bfecu,1);
    if(m.u8(0x7f9220u)&0xfu)return;
    ghost_packet_466e50(c,car);
    m.put32(0x7f92b8u,m.i32(0x656234u)<0x3c?1u:0u);                          // 48B350; NEG AL; SBB; NEG
}

// 4666A0 (per update, 417C7B loop): the playback sub-frame counter 7F9220
// once the countdown is over; hides event 9's ghost when slot 0 and slot 1
// hold the same record (time +4 and CRC +FC equal).
void ghost_frame_4666a0(PcRaceContext& c){
    auto& m=c.m;
    if(!m.u32(0x7f9224u))return;
    if(!m.u8(0x64bfecu))return;
    if(std::int16_t(call(c,0x49b2d0u,{}))>0x3c)return;
    if(m.u8(0x7f922cu)){m.put32(0x7f9220u,0);m.put8(0x7f922cu,0);}
    else m.put32(0x7f9220u,m.u32(0x7f9220u)+1u);
    const std::uint32_t g=m.u32(0x7f9224u);
    const std::uint32_t car=m.u32(0x799d54u);
    const bool same_time=m.u32(g+4u)==m.u32(g+0x9cb0u);
    m.put8(0x64bfecu,0);
    if(!same_time)return;
    if(m.u32(g+0xfcu)!=m.u32(g+0x9da8u))return;
    m.put32(car+0xc5c,m.u32(car+0xc5c)&0xffffdfffu);
}

std::uint32_t ghost_state_466250(const PcRaceMemory& m,std::uint32_t slot){
    if(std::int32_t(slot)>=3)return 0u;
    const std::uint32_t base=m.u32(0x7f9224u);
    if(!base)return 0u;
    const std::uint32_t g=base+slot*PcGhostRecordBytes;
    if(m.u32(g)!=0x544f484eu)return 0u;
    const std::uint32_t index=m.u32(0x7f91e4u+slot*4u);
    if(!index)return 1u;
    return index>=m.u32(g+0xf8u)?3u:2u;
}

void ghost_car_control_4ace40(PcRaceContext& c,std::uint32_t car){
    auto& m=c.m;
    const std::uint32_t player=m.u32(0x799d18u);
    for(std::uint32_t k=0;k<12u;k+=4)m.put32(car+0x16c+k,m.u32(car+0x14+k));
    m.put16(car+0x17c,m.u16(car+0x2c));m.put16(car+0x17e,m.u16(car+0x2e));m.put16(car+0x180,m.u16(car+0x30));
    for(std::uint32_t k=0;k<12u;k+=4)m.put32(car+0x1040+k,m.u32(car+0x2d8+k));
    for(std::uint32_t k=0;k<12u;k+=4)m.put32(car+0x1034+k,m.u32(car+0x2e4+k));
    const std::uint32_t slot=m.u32(car)-9u;
    m.put16(car+0xc2e,m.u16(car+0xc2c));
    if(!ghost_play_467e30(c,car,slot))(void)call(c,0x4401d0u,{m.u32(car)});
    (void)call(c,0x4a2650u,{car});
    {   // 40A060: identity at car+F0
        for(std::uint32_t k=0;k<0x40u;k+=4)m.put32(car+0xf0+k,0);
        for(std::uint32_t k:{0x0u,0x14u,0x28u,0x3cu})m.put32(car+0xf0+k,0x3f800000u);}
    if(ghost_state_466250(m,slot)!=2u){m.putf(car+0xb68,KNear);return;}
    const std::int32_t gap=std::int32_t(m.u16(car+0x260))-std::int32_t(m.u16(player+0x260));
    float v=m.f32(car+0xb68);
    if(gap>2){
        if(jbe(KFar,v)){m.putf(car+0xb68,KFar);return;}
        v=v+KStep;m.putf(car+0xb68,v);
        if(!jbe(v,KFar))m.putf(car+0xb68,KFar);
    }else{
        if(jbe(v,KNear)){m.putf(car+0xb68,KNear);return;}
        v=v-KStep;m.putf(car+0xb68,v);
        if(!jbe(KNear,v))m.putf(car+0xb68,KNear);
    }
}

// 46F350: other-car state reset (4866C0 parameter +1C of car byte +11).
void car_reset_46f350(PcRaceMemory& m,std::uint32_t e){
    const float zero=0.f;
    m.put32(e+0x2f0,(m.u32(e+0x2f0)&0xfffffc04u)|4u);
    m.putf(e+0x2c8,zero);m.putf(e+0x2d0,KMax);
    for(std::uint32_t o:{0x2d8u,0x2dcu,0x2e0u,0x2e4u,0x2e8u,0x2ecu,0x2f8u})m.putf(e+o,zero);
    m.put32(e+4,0);m.put32(e+8,0);m.put32(e+0xc,0);
    m.putf(e+0x2d4,K1);
    m.putf(e+0x300,KMin);
    m.put8(e+0x329,0x7f);
    const auto model=std::int8_t(m.u8(e+0x11));
    if(model<0)throw std::runtime_error("46F350: negative 4866C0 model");
    m.put32(e+0x30c,m.u32(0x650500u+std::uint32_t(model)*0x44u+0x1cu));   // 4866C0 +1C (model table 650500, EXE data)
    m.put32(e+0x314,0);m.put32(e+0x318,0);
    m.putf(e+0x58,K1);
    m.put8(e+0xd22,0);m.put8(e+0xd23,0);m.putf(e+0xd24,zero);
    for(std::uint32_t o:{0xae8u,0xc60u,0xdd4u,0xda0u,0xda8u,0xe74u})m.put32(e+o,0x19a);
    m.put32(e+0xc5c,m.u32(e+0xc5c)&0xff3fffffu);
    for(std::uint32_t base:{0x100cu,0x101bu}){m.put32(e+base,0);m.put32(e+base+4,0);m.put32(e+base+8,0);m.put16(e+base+0xc,0);m.put8(e+base+0xe,0);}
    m.put16(e+0xb4c,0);m.put16(e+0xb4e,0);
    for(std::uint32_t k=0;k<12u;k+=4)m.put32(e+0xaf4+k,m.u32(e+0x14+k));
    for(std::uint32_t o:{0xb00u,0xb04u,0xb08u,0xb0cu,0xb10u,0xb68u,0xbb0u})m.putf(e+o,zero);
    m.put8(e+0xb52,0);m.put32(e+0xb58,0);m.put32(e+0xb5c,0);m.put16(e+0xb60,0);m.put8(e+0xb53,0);
    m.put16(e+0xb62,0);m.put16(e+0xb64,0);m.put8(e+0xb6c,0);m.put16(e+0xb66,0);m.put16(e+0xb72,0);
    m.putf(e+0xb54,K100000);
    m.put8(e+0xc30,2);m.put8(e+0xc32,0xff);m.put8(e+0xc31,0xff);
    const std::uint8_t first=m.u8(e);
    for(std::uint32_t o:{0xbb8u,0xbbcu,0xb78u,0xb7cu,0xb80u,0xb84u,0xb88u,0xb8cu,0xb90u,0xb94u,0xb98u,0xb9cu,0xba0u,0xba4u,0xba8u,0xbacu,
                         0xbc8u,0xbe4u,0xbecu,0xbd0u,0xbd4u,0xbd8u})m.putf(e+o,zero);
    m.put8(e+0xc36,0);m.put8(e+0xc37,0);m.put32(e+0xc20,0);m.put16(e+0xc24,0);m.put16(e+0xc2c,0);m.put16(e+0xc2e,0);
    m.put16(e+0xc26,0);m.put8(e+0xc33,0);m.put32(e+0xc48,0);m.put16(e+0xc2a,0);m.put32(e+0xc58,0);m.put16(e+0xc28,0);m.put8(e+0xc42,0);
    m.putf(e+0xbb4,K100000);m.putf(e+0xbc0,K1);m.putf(e+0xbe0,K1);m.putf(e+0xbe8,K1);
    m.put8(e+0xc38,0xff);
    m.put8(e+0xc34,std::uint8_t(first+2u));
    m.putf(e+0x1d4,zero);m.putf(e+0x1d0,zero);m.put32(e+0x1d8,1);
    for(std::uint32_t o:{0xbf0u,0xbf4u,0xbf8u,0xbfcu,0xc00u,0xc04u})m.putf(e+o,KMin);
    for(std::uint32_t o:{0xc08u,0xc0cu,0xc10u,0xc14u,0xc18u,0xc1cu})m.putf(e+o,KMax);
    m.put32(e+0xb18,0xb4);
}

void ghost_car_init_4ad000(PcRaceContext& c,std::uint32_t car){
    auto& m=c.m;
    if(!vehicle_clear_4874f0(m.bytes(car,PcVehicleObjectBytes),true))throw std::runtime_error("4874F0: car work too small");
    car_reset_46f350(m,car);
    m.put32(car+4,(m.u32(car+4)&0xffffffdcu)|0xc0u);
    m.put32(car+0xc5c,m.u32(car+0xc5c)|0x2000u);
    const std::uint8_t n=m.u8(0x841fa9u);
    const std::uint32_t q=0x841fb0u+std::uint32_t(std::int32_t(std::int8_t(n)))*16u;
    m.put32(car,m.u32(q));
    m.put8(car+0x10,m.u8(q+0xc));
    m.put8(0x841fa9u,std::uint8_t(n+1u));
    m.put8(car+0x11,m.u8(q+8));
    m.put8(car+0x12,m.u8(q+0xd));
    (void)call(c,0x440bd0u,{m.u32(car),0x100u});
    (void)call(c,0x440ba0u,{0u});
}

void ghost_car_queue_4acfc0(PcRaceMemory& m,std::uint32_t event,std::uint8_t k,std::uint32_t a2,std::uint8_t a3){
    const std::uint8_t n=m.u8(0x841fa8u);
    const std::uint32_t q=0x841fb0u+std::uint32_t(std::int32_t(std::int8_t(n)))*16u;
    m.put32(q,event);m.put8(q+0xc,k);m.put32(q+8,a2);m.put8(q+0xd,a3);
    m.put8(0x841fa8u,std::uint8_t(n+1u));
}
void ghost_car_queue_reset_4acfb0(PcRaceMemory& m){m.put8(0x841fa8u,0);m.put8(0x841fa9u,0);}
void ghost_car_destroy_470560(PcRaceMemory& m,std::uint32_t car){m.put32(car+0x10d0,0xffffffffu);}

// ---- record ghost cars (variant 0): 440750's queue 4AD1A0, init 4AD1F0, control 4AD080 ----
void record_car_queue_4ad1a0(PcRaceMemory& m,std::uint32_t event,std::uint8_t k,std::uint32_t a2,std::uint8_t a3){
    const std::uint8_t n=m.u8(0x841ff0u);
    const std::uint32_t q=0x841ff8u+std::uint32_t(std::int32_t(std::int8_t(n)))*16u;
    m.put32(q,event);m.put8(q+0xc,k);m.put32(q+8,a2);m.put8(q+0xd,a3);
    m.put8(0x841ff0u,std::uint8_t(n+1u));
}
// 4AD1F0: as 4AD000 on the 841FF0 queue, except that +10 is read from the entry at the queue
// count (841FF0), not at the read index.
void record_car_init_4ad1f0(PcRaceContext& c,std::uint32_t car){
    auto& m=c.m;
    if(!vehicle_clear_4874f0(m.bytes(car,PcVehicleObjectBytes),true))throw std::runtime_error("4874F0: car work too small");
    car_reset_46f350(m,car);
    m.put32(car+4,(m.u32(car+4)&0xffffffdcu)|0xc0u);
    m.put32(car+0xc5c,m.u32(car+0xc5c)|0x2000u);
    const std::uint8_t n=m.u8(0x841ff1u);
    const std::uint32_t q=0x841ff8u+std::uint32_t(std::int32_t(std::int8_t(n)))*16u;
    m.put32(car,m.u32(q));
    m.put8(car+0x10,m.u8(0x842004u+std::uint32_t(std::int32_t(std::int8_t(m.u8(0x841ff0u))))*16u));
    m.put8(0x841ff1u,std::uint8_t(n+1u));
    m.put8(car+0x11,m.u8(q+8));
    m.put8(car+0x12,m.u8(q+0xd));
    (void)call(c,0x440bd0u,{m.u32(car),0x100u});
    (void)call(c,0x440ba0u,{0u});
}
// 4AD080: in GAME (78026C = 0x10) and variant 0 only: the countdown fade (+C5C bit 0x2000,
// +B68 = 0 above 60 frames), 480220 playback at the 47F1A0 frame (4401D0 when it returns 0),
// 4A2650, the +F0 identity, then +B68 eased toward 0.75 (playing, more than two course units
// ahead of the player) or 0.25.
void record_car_control_4ad080(PcRaceContext& c,std::uint32_t car){
    auto& m=c.m;
    if(m.u32(0x78026cu)!=0x10u||m.u32(0x780258u)!=0u)return;
    const std::uint32_t player=m.u32(0x799d18u);
    const std::uint32_t slot=m.u32(car)-9u;
    if(std::int16_t(call(c,0x49b2d0u,{}))>0x3c){
        m.put32(car+0xc5c,m.u32(car+0xc5c)&0xffffdfffu);m.putf(car+0xb68,0.0f);return;
    }
    if(std::int16_t(call(c,0x49b2d0u,{}))==0x3c)m.put32(car+0xc5c,m.u32(car+0xc5c)|0x2000u);
    const std::uint32_t frame=call(c,0x47f1a0u,{slot});
    if(!call(c,0x480220u,{car,slot,frame}))(void)call(c,0x4401d0u,{m.u32(car)});
    (void)call(c,0x4a2650u,{car});
    {   // 40A060: identity at car+F0
        for(std::uint32_t k=0;k<0x40u;k+=4)m.put32(car+0xf0+k,0);
        for(std::uint32_t k:{0x0u,0x14u,0x28u,0x3cu})m.put32(car+0xf0+k,0x3f800000u);}
    float target=KNear;
    if(call(c,0x47f140u,{slot})==2u&&std::int32_t(m.u16(car+0x260))-std::int32_t(m.u16(player+0x260))>2)target=KFar;
    float v=m.f32(car+0xb68);
    if(!jbe(v,target)){
        v=v-KStep;m.putf(car+0xb68,v);
        if(!jbe(target,v))m.putf(car+0xb68,target);
    }else{
        v=v+KStep;m.putf(car+0xb68,v);
        if(!jbe(v,target))m.putf(car+0xb68,target);
    }
}
}

// ---- display: 4AE5F0 (with 4AE0F0 and the other-car model tree) and 4ADAC0
namespace outrun::platform {
namespace {
#include "platform/race_ghost_car_tables.inc"
constexpr std::uint32_t None=0xffffffffu;
const float KPi=bits(0x40490fdbu);            // 6280C8
const float KLift=bits(0x3dcccccdu);          // 0.1 (40A290 y)
const float KGlowMin=bits(0x34000000u);       // 6281F0
struct Draw {
    PcRaceContext& c;PcRaceMemory& m;driving::PcMatrixStack& s;
    explicit Draw(PcRaceContext& cc):c(cc),m(cc.m),s(cc.matrices){}
    void leaf(std::uint32_t pc,std::initializer_list<std::uint32_t> args){
        if(!c.draws)throw std::logic_error("ghost car display without a draw list");
        PcVehicleDrawCall d{};d.pc=pc;d.argc=std::uint32_t(args.size());
        unsigned i=0;for(auto a:args)d.args[i++]=a;
        const auto cur=s.current();for(unsigned k=0;k<64;++k)d.matrix[k]=cur.u8(k);
        c.draws->push_back(d);
    }
    std::uint32_t svc(std::uint32_t pc,std::initializer_list<std::uint32_t> args={}){return call(c,pc,args);}
    void push(){driving::pc_matrix_push(s);}
    void pop(){driving::pc_matrix_pop(s);}
    void push_load(std::uint32_t a){driving::pc_matrix_push_load(s,m.bytes(a,64));}
    void push_mul(std::uint32_t a){   // 409FD0: depth+1; when it fits, current+1 = [a] * current
        const bool fits=s.depth+1<s.capacity;driving::pc_matrix_push(s);
        if(fits)driving::pc_matrix_multiply_current(s,m.bytes(a,64));
    }
    void translate(std::uint32_t v){driving::pc_matrix_translate_vector(s,{m.f32(v),m.f32(v+4),m.f32(v+8)});}   // 40A2D0
    void translate3(float x,float y,float z){driving::pc_matrix_translate_vector(s,{x,y,z});}                  // 40A290
    void scale3(float x,float y,float z){std::array<float,16> k{x,0,0,0, 0,y,0,0, 0,0,z,0, 0,0,0,1};           // 40A360
        driving::pc_matrix_multiply_current(s,driving::Bytes(k.data(),64));}
    void multiply(std::uint32_t a){driving::pc_matrix_multiply_current(s,m.bytes(a,64));}                     // 40A220
    std::uint32_t lods(std::uint32_t car){   // [4866C0(car byte +11)]
        const auto model=std::int8_t(m.u8(car+0x11));
        return m.u32(0x650500u+std::uint32_t(std::int32_t(model))*0x44u);
    }
    // 4AD470 (EDX = id, ECX = colour, AL = byte)
    void object(std::uint32_t id,std::uint32_t colour,std::uint8_t byte){
        const std::uint32_t b=std::uint32_t(std::int32_t(std::int8_t(byte)));
        if(!m.u32(0x842038u))leaf(0x405360u,{id,0,0,colour,b,0});
        else if(!m.u32(0x842040u))leaf(0x405360u,{id,1,0,colour,b,0});
        else leaf(0x4056d0u,{id,m.u32(0x84203cu),colour,b});
    }
    // 4AD410 (EAX = id; VM prologue = mov ecx,[842038]); 4ADA30 inlines it.
    void plain(std::uint32_t id){
        if(!m.u32(0x842038u))leaf(0x405360u,{id,0,0,0,None,0});
        else if(!m.u32(0x842040u))leaf(0x405360u,{id,1,0,0,None,0});
        else leaf(0x4056d0u,{id,m.u32(0x84203cu),0,None});
    }
    std::uint32_t colour(std::uint32_t car){return m.u32(0x683980u+std::uint32_t(std::int32_t(std::int8_t(m.u8(car+0x11))))*4u);}
    // 4AD4D0 (ESI = car, EDI = descriptor)
    void body_4ad4d0(std::uint32_t car,std::uint32_t d){
        if((m.u32(car+4)&0x100u)&&m.u32(d+8)!=None)object(m.u32(d+8),colour(car),m.u8(car+0x12));
        std::uint32_t id;
        if(std::int32_t(m.u32(car+0x38))>0)id=m.u32(d+0x14);
        else id=(m.u32(car+4)&0x300u)?m.u32(d+0x10):m.u32(d+0xc);
        if(id!=None)object(id,colour(car),m.u8(car+0x12));
    }
    // 4AD6C0 / 4AD9D0: lamp pair a/b by +C5C bits 0x200/0x400, blinking on +B50 % 30 > 15.
    void lamps(std::uint32_t car,std::uint32_t a,std::uint32_t b){
        if(a==None||b==None)return;
        if(!(m.u32(car+0xc5c)&0x600u))return;
        if(std::int32_t(m.u16(car+0xb50))%0x1e<=0xf)return;
        push();
        if(m.u32(car+0xc5c)&0x200u)plain(a);
        if(m.u32(car+0xc5c)&0x400u)plain(b);
        pop();
    }
    // 4AD720 (descriptor +9C list, 4AD470 objects) / 4ADA30 (+68 list, 4AD410 objects)
    void list(std::uint32_t car,std::uint32_t d,std::uint32_t list_offset,bool coloured){
        const std::uint32_t p=m.u32(d+list_offset);
        if(!p||m.u32(p)==None)return;
        for(std::uint32_t e=0;;e+=0x10){
            push();translate(p+e+4);
            if(coloured)object(m.u32(p+e),colour(car),m.u8(car+0x12));else plain(m.u32(p+e));
            pop();
            if(m.u32(p+e+0x10)==None)break;
        }
    }
    // 4AD7E0 ([esp+4] = descriptor, ESI = car)
    void lamp_4ad7e0(std::uint32_t car,std::uint32_t d){
        if(!(m.u8(car+4)&0x20u))return;
        const auto bl=std::int32_t(std::int8_t(m.u8(car+0x328)));
        push();translate(d+0x20);
        const std::uint32_t v=m.u32(0x780258u);
        if(v==3u||v==4u)object(m.u32(0x5c460cu+std::uint32_t(bl)*4u),0x683a30u,1);
        else if(m.u8(car+0xc35)==0xffu)object(m.u32(0x5c45f8u+std::uint32_t(bl)*4u),0x683a30u,m.u8(car+0x12));
        else{const std::int32_t k=std::int32_t(std::int8_t(m.u8(car+0xc35)))*5+bl;object(m.u32(0x5c460cu+std::uint32_t(k)*4u),0x683a30u,m.u8(car+0x12));}
        pop();
    }
    // 4AD880 ([esp+4] = descriptor, ESI = car; VM at 4AD8A2 = mov bl,[esi+328])
    void lamp_4ad880(std::uint32_t car,std::uint32_t d){
        if(!(m.u8(car+4)&0x20u))return;
        if(!(svc(0x4957f0u)&0xffu))return;
        if(svc(0x495b00u)&0xffu)return;
        if(!(svc(0x4962d0u)&0xffu))return;
        const auto bl=std::int32_t(std::int8_t(m.u8(car+0x328)));
        push();translate(d+0x2c);
        const std::int32_t k=std::int32_t(svc(0x495860u))*5+bl;
        object(m.u32(0x5c45f8u+std::uint32_t(k)*4u),0x683a30u,m.u8(car+0x12));
        pop();
    }
    float word_angle(std::uint32_t a){return driving::x87_float(X87(std::int32_t(std::int16_t(m.u16(a))))*X87(KAngle));}
    static float neg(float v){std::uint32_t u;std::memcpy(&u,&v,4);u^=0x80000000u;std::memcpy(&v,&u,4);return v;}   // FCHS
    // 4AD530 (EAX = car, ESI = descriptor): the four wheels
    void wheels_4ad530(std::uint32_t car,std::uint32_t d){
        const float yaw=word_angle(car+0x32);
        const float a=neg(word_angle(car+0x40));
        const float b=neg(word_angle(car+0x44));
        push();translate(d+0x3c);driving::pc_matrix_rotate_y(s,yaw);driving::pc_matrix_rotate_x(s,a);plain(m.u32(d+0x38));pop();
        push();translate(d+0x4c);
        driving::pc_matrix_rotate_y(s,driving::x87_float(X87(yaw)+X87(KPi)));
        driving::pc_matrix_rotate_x(s,neg(a));plain(m.u32(d+0x48));pop();
        push();translate(d+0x5c);driving::pc_matrix_rotate_x(s,b);plain(m.u32(d+0x58));pop();
        if(m.u32(d+0x78)!=None){push();translate(d+0x7c);driving::pc_matrix_rotate_x(s,b);plain(m.u32(d+0x78));pop();}
        const float back=neg(b);
        push();translate(d+0x6c);driving::pc_matrix_rotate_y(s,KPi);driving::pc_matrix_rotate_x(s,back);plain(m.u32(d+0x68));pop();
        if(m.u32(d+0x88)!=None){push();translate(d+0x8c);driving::pc_matrix_rotate_y(s,KPi);driving::pc_matrix_rotate_x(s,back);plain(m.u32(d+0x88));pop();}
    }
    // 4AD8F0 (EAX = car, ESI = second descriptor)
    void wheels_4ad8f0(std::uint32_t car,std::uint32_t d){
        const float a=neg(word_angle(car+0x40));const float b=neg(a);
        push();translate(d+0x28);driving::pc_matrix_rotate_x(s,a);plain(m.u32(d+0x24));pop();
        push();translate(d+0x38);driving::pc_matrix_rotate_y(s,KPi);driving::pc_matrix_rotate_x(s,b);plain(m.u32(d+0x34));pop();
        if(m.u32(d+0x44)!=None){push();translate(d+0x48);driving::pc_matrix_rotate_x(s,a);plain(m.u32(d+0x44));pop();}
        if(m.u32(d+0x54)!=None){push();translate(d+0x58);driving::pc_matrix_rotate_y(s,KPi);driving::pc_matrix_rotate_x(s,b);plain(m.u32(d+0x54));pop();}
    }
    // Brake glow (descriptor +4): 4056D0 with max(1 - +2DC, 6281F0) while +2C8 > 0.
    void glow(std::uint32_t car,std::uint32_t id){
        if(id==None)return;
        if(!jbe(m.f32(car+0x2c8),0.f)){
            float v=K1-m.f32(car+0x2dc);
            if(!(KGlowMin<v)&&!std::isnan(v))v=KGlowMin;          // COMISS 6281F0,v ; JB keeps v (also unordered)
            std::uint32_t u;std::memcpy(&u,&v,4);
            leaf(0x4056d0u,{id,u,0,None});
        }else leaf(0x405360u,{id,0,0,0,None,0});
    }
    // Second descriptor (+C90 matrix): body, lamps, (list), lift, wheels.
    void second(std::uint32_t car,std::uint32_t d2,bool near,bool lifted){
        push();multiply(car+0xc90);
        object(m.u32(d2),colour(car),m.u8(car+0x12));
        std::uint32_t id;
        if(std::int32_t(m.u32(car+0x38))>0)id=m.u32(d2+0x18);
        else id=(m.u32(car+4)&0x300u)?m.u32(d2+0x14):None;
        if(id!=None)plain(id);
        lamps(car,m.u32(d2+0x1c),m.u32(d2+0x20));
        if(near)list(car,d2,0x68,false);
        if(m.u32(d2+0x10)!=None){
            if(lifted){push();translate3(0.f,KLift,0.f);plain(m.u32(d2+0x10));pop();}
            else plain(m.u32(d2+0x10));
        }
        if(near)wheels_4ad8f0(car,d2);
        pop();
    }
    // 4ADBC0 (LOD 0) / 4ADD80 (LOD 1) / 4ADF00 (LOD 2..4 via 4AE030/4AE070/4AE0B0,
    // VM prologue at 4ADF06 = lea eax,[ebx+F0]).
    void model(std::uint32_t car,std::uint32_t lod){
        const std::uint32_t L=m.u32(lods(car)+lod*4u);
        const std::uint32_t d=m.u32(L),d2=m.u32(L+4);
        const bool near=lod<=1u;
        push_mul(car+0xf0);
        object(m.u32(d),colour(car),m.u8(car+0x12));
        body_4ad4d0(car,d);
        lamps(car,m.u32(d+0x18),m.u32(d+0x1c));
        if(near)list(car,d,0x9c,true);
        lamp_4ad7e0(car,d);
        lamp_4ad880(car,d);
        pop();
        if(lod==0u){
            if(m.u32(d+4)!=None){push();translate3(0.f,KLift,0.f);glow(car,m.u32(d+4));pop();}
        }else glow(car,m.u32(d+4));
        if(near)wheels_4ad530(car,d);
        if(d2)second(car,d2,near,lod==0u);
    }
};
}

void ghost_map_car_tables(PcRaceMemory& m){
    m.map_const(0x650500u,Exe650500,sizeof Exe650500);m.map_const(0x5bc910u,Exe5bc910,sizeof Exe5bc910);
    m.map_const(0x683980u,Exe683980,sizeof Exe683980);m.map_const(0x5c45f8u,Exe5c45f8,sizeof Exe5c45f8);
    // the traffic models 30..44: their 650500 records and model chains (embedded EXE ranges)
    for(std::size_t i=0;i<EmbeddedExeRangeCount;++i){
        const auto& r=EmbeddedExeRanges[i];
        if(r.base==0x650cf8u||r.base==0x5baa00u||r.base==0x6839f8u)m.map_const(r.base,r.data,r.size);
    }
}

// 4AE386 (Heart Attack, car +C5C 0x80000): the car is shown as the heart
// item 57002C (57002D when +2F0 & 2), turned by the angle word +B6E and
// bobbing 3 +- 0.5 over the 100-frame cycle of +B70, in the car's matrix.
void heart_item_draw_4ae386(Draw& d,std::uint32_t car){
    auto& m=d.m;
    using driving::X87;
    d.push_load(car+0xb0);                                                       // 409F90
    const std::int32_t phase=std::int32_t(std::int16_t(m.u16(car+0xb70)))%100;   // idiv: the remainder
    const float t=(float(phase)*0.01f)*6.28318548f;                              // SSE mulss / mulss
    const float y=driving::x87_float(driving::x87_sin(X87(t))*X87(0.5f)+X87(3.0f));   // 449360 (fsin)
    const float yaw=driving::x87_float(X87(std::int32_t(std::int16_t(m.u16(car+0xb6e))))*X87(9.58738019107841e-05f));   // 4493A0 (fild) * 2pi/65536
    driving::pc_matrix_rotate_y(d.s,yaw);                                        // 40A410
    d.translate3(0.0f,y,0.0f);                                                   // 40A290
    d.leaf(0x4044a0u,{});d.leaf(0x4044e0u,{8});
    d.leaf(0x405360u,{(m.u8(car+0x2f0)&2u)?0x57002du:0x57002cu,0,0,0,None,0});
    d.leaf(0x4044c0u,{});
    d.pop();                                                                     // 40A010
}
// 4AE0F0 (ESI = car), offline path (the network draws are refused).
void other_car_draw_4ae0f0(PcRaceContext& c,std::uint32_t car){
    Draw d(c);auto& m=c.m;
    if(m.u32(0x7f94c0u)!=0u)throw std::runtime_error("4AE0F0: network car draw (55A930) not ported");
    if(m.u32(0x780258u)==2u&&(m.u32(car+0xc5c)&0x80000u)){heart_item_draw_4ae386(d,car);return;}
    std::uint32_t flags=m.u32(car+4);
    float saved=0.f;std::uint32_t was40=0;
    if(flags&4u){
        saved=m.f32(car+0xb68);was40=(flags>>6)&1u;
        if(!was40){m.put32(car+4,flags|0x40u);m.putf(car+0xb68,K0_5);}
    }
    m.put32(0x842038u,0);m.put32(0x842040u,0);m.putf(0x84203cu,K1);
    if(m.u8(car+4)&0x40u){
        m.put32(0x842038u,1);
        const float alpha=m.f32(car+0xb68);                                    // 4AD1E0: FLD [+B68]
        if(!(K1<=alpha)&&!std::isnan(alpha)){m.put32(0x842040u,1);m.putf(0x84203cu,alpha);}   // FCOMIP 1.0,alpha; JBE
    }
    const std::uint32_t list=d.lods(car);
    const auto lod=std::int32_t(std::int8_t(m.u8(car+0x328)));
    d.leaf(0x4052b0u,{});
    if(lod>=0&&lod<=4&&m.u32(list+std::uint32_t(lod)*4u))d.model(car,std::uint32_t(lod));
    if(m.u32(0x842040u)){
        d.leaf(0x4044f0u,{0,0,0,8,0x10000000u});
        d.leaf(0x4044f0u,{1,0,0,8,0});
        d.leaf(0x405350u,{});
        d.leaf(0x4044f0u,{0,1,0,8,7});
        d.leaf(0x4044f0u,{1,1,0,8,7});
    }
    d.leaf(0x4052c0u,{});
    if(m.u32(0x842040u))d.leaf(0x404540u,{});
    flags=m.u32(car+4);
    if(flags&4u){m.put32(car+4,(((was40<<6)^flags)&0x40u)^flags);m.putf(car+0xb68,saved);}
    d.leaf(0x404540u,{});
}

void ghost_car_display_4ae5f0(PcRaceContext& c,std::uint32_t car){
    Draw d(c);auto& m=c.m;
    if(m.u32(car+8)&1u)return;
    if((m.u32(0x7d39f0u)>>2)&1u)return;                                       // 44FF10
    const std::uint32_t ghost=(m.u32(car+4)>>6)&1u;
    if(ghost&&!(m.u32(car+0xc5c)&0x2000u))return;
    if(ghost==1u&&m.u8(0x7c24c9u))return;
    const bool not12=m.u32(0x78026cu)!=0x12u;
    // Port: on a display-only frame (above 60 Hz) the timers and the flash hold.
    const bool ticked=enhancements::display_ticks()!=0;
    auto event_flag=[&]{return ticked&&(m.u8(0x79fb48u+m.u32(car))&0x10u)!=0u;};  // 440A50
    bool toggle=true;
    if(m.u16(car+0xb62)){
        if(event_flag())m.put16(car+0xb62,std::uint16_t(m.u16(car+0xb62)-1u));
    }else if(m.u16(car+0xb64)){
        const std::uint32_t v=m.u32(0x780258u);
        if(v==3u||v==4u||!m.u8(car+0xb6c)||!(m.u32(car+0x2f0)&2u)||std::int32_t(m.u32(car+0x2f4))>=0x1e){
            if(event_flag())m.put16(car+0xb64,std::uint16_t(m.u16(car+0xb64)-1u));
        }else toggle=false;
    }else toggle=false;
    if(toggle){
        const std::uint8_t b=std::uint8_t(m.u8(car+0xb53)^(not12&&ticked?1u:0u));m.put8(car+0xb53,b);
        if(b)return;
    }
    const std::uint32_t player=m.u32(0x799d18u);
    std::int32_t gap;
    if(m.u8(car+4)&0x40u)gap=std::int32_t(m.u16(car+0x260))-std::int32_t(m.u16(player+0x260));
    else gap=std::int32_t(call(c,0x46f990u,{car+0x5c,player+0x5c}));
    bool draw;
    if(m.u32(0x78026cu)==3u&&call(c,0x49eed0u,{})==4u)draw=true;
    else if(m.u8(car+4)&0x40u)draw=true;
    else draw=gap<0x3c&&gap>-3;
    if(!draw)return;
    d.push_load(car+0xb0);
    d.leaf(0x4044a0u,{});d.leaf(0x4044e0u,{9});
    other_car_draw_4ae0f0(c,car);
    d.leaf(0x4044c0u,{});
    d.pop();
}

void ghost_car_shadow_4adac0(PcRaceContext& c,std::uint32_t car){
    Draw d(c);auto& m=c.m;
    if(m.u8(0x7c24c9u))return;
    const std::uint32_t player=m.u32(0x799d18u);
    const std::uint32_t f=m.u32(car+4);
    if(!(f&0x80u))return;
    if(m.u32(car+8)&1u)return;
    if(f&0x40u){if(!(m.u32(car+0xc5c)&0x2000u))return;}
    else{
        std::int32_t gap;
        if(m.u32(0x78026cu)==3u&&call(c,0x49eed0u,{})==4u){
            if(m.u32(car)==8u)return;
            gap=std::int32_t(m.u16(car+0x260))-std::int32_t(m.u16(player+0x260));
        }else gap=std::int32_t(call(c,0x46f990u,{car+0x5c,player+0x5c}));
        if(gap>=0x3c||gap<=-3)return;
    }
    if(m.u32(0x780258u)==2u&&(m.u32(car+0xc5c)&0x80000u))return;
    if(m.u32(0x7f94c0u)!=0u){
        const std::uint32_t st=m.u32(car+0xd14);
        if(st==1u||st==2u||st==3u)return;
    }
    d.push_load(car+0xb0);
    d.scale3(bits(0x3f8ccccdu),bits(0x3f8ccccdu),bits(0x3f8ccccdu));
    {   // 4AD380
        const std::uint32_t L=m.u32(d.lods(car));
        const std::uint32_t d0=m.u32(L),d1=m.u32(L+4);
        if(m.u32(d0+0x98)!=None)d.leaf(0x405360u,{m.u32(d0+0x98),1,0,0,None,0});
        if(d1&&m.u32(d1+0x64)!=None){d.push();d.multiply(car+0xc90);d.leaf(0x405360u,{m.u32(d1+0x64),1,0,0,None,0});d.pop();}
    }
    d.pop();
}
}
