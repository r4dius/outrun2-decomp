#include <cstdio>
#include "driving/pc_driving.hpp"
#include "driving/pc_x87.hpp"
#include <algorithm>
#include <cmath>
#include <limits>
#include <string>
namespace outrun::driving {
void Bytes::out_of_view(std::size_t o,std::size_t s,std::size_t n){
    char text[96];std::snprintf(text,sizeof text,"PC field outside view (+%zx/%zx of %zx)",o,s,n);
    throw std::out_of_range(text);
}
namespace {
float bits(std::uint32_t u){float f;std::memcpy(&f,&u,4);return f;}
// Exact PC literals; notably 0x3ee971aa differs by 1 ULP from Lindbergh.
float inv255(){return bits(0x3b808081);}
float angle_unit(){return bits(0x38c90fdb);}
Bytes torque_table(Bytes e,const Tables& t){
    const std::size_t index=e.u32(0xe84)?0:std::size_t(e.u8(0x13));
    if(index>=t.torque.size())throw std::out_of_range("torque table id");
    return t.torque[index];
}
std::int32_t add32(std::int32_t a,std::int32_t b){
    auto v=static_cast<std::uint32_t>(a)+static_cast<std::uint32_t>(b);
    std::int32_t r;std::memcpy(&r,&v,4);return r;
}
std::int32_t to_int(double v){
    if(!std::isfinite(v)||v<std::numeric_limits<std::int32_t>::min()||v>=2147483648.0)
        throw std::domain_error("integer conversion outside validated driving domain");
    return static_cast<std::int32_t>(v);
}
std::int32_t to_int(X87 v){ // CRT _ftol2 (582194): truncation of the x87 value
    if(!(v.v>=-2147483648.0&&v.v<2147483648.0))
        throw std::domain_error("integer conversion outside validated driving domain");
    return x87_ftol32(v);
}
X87 fild_u32(std::uint32_t u){ // FILD (signed) + FADD 2^32 (628070) when the sign bit is set
    X87 v=X87(static_cast<std::int32_t>(u));
    if(static_cast<std::int32_t>(u)<0)v=v+bits(0x4f800000);
    return v;
}
float radians(std::int32_t a){return x87_float(X87(a)*angle_unit());} // 4493A0 fild; fmul 628254; fstp
void finish_clutch(Bytes e,Bytes p){
    float f=static_cast<float>(e.i32(0x3c))*inv255();
    e.putf(0x22c,f); e.putf(0x228,p.f32(0x1774)*f);
}
}
float engine_friction(Bytes e,Bytes p,const Tables& t,float rpm){
    float r=0.0f;
    if(!(0.0f>rpm))r=rpm>p.f32(0x1644)?p.f32(0x1644):rpm;
    float base=r*bits(0x3ee971aa);
    base=base+bits(0x430f3d4d); base=base*r;
    base=base+bits(0x47bd7400); base=base*bits(0x3dd0fac6);
    const auto table=torque_table(e,t);
    float geometry=p.f32(0x1560)*p.f32(0x1560);
    geometry=geometry*p.f32(0x15ac); geometry=geometry*p.f32(0x1514);
    geometry=geometry*bits(0x3f490fdb);
    float other=table.f32(4)*base;
    geometry=geometry*other; return geometry*bits(0x3da2f983);
}
void auto_clutch_control(Bytes e,Bytes w,Bytes p){
    if(e.u8(4)&8){
        e.puti(0x3c,0);e.put8(0x296,0);e.putf(0x22c,0);e.putf(0x228,0);e.put8(0x297,1);return;
    }
    if(e.f32(0x2f8)>0.0f){e.puti(0x3c,0);e.put8(0x297,1);finish_clutch(e,p);return;}
    const auto speed=e.u32(0x1f4);
    if((speed==0 || std::abs(int(e.i16(0x4e)))>0x7000) &&
       (e.i32(0x38)>50 || e.u32(0x48)<4500)){
        e.put32(0x3c,std::min(speed,255u));e.put8(0x297,1);finish_clutch(e,p);return;
    }
    const auto shift=e.u8(0x296);
    if(shift==4){
        float average=w.f32(0x618)+w.f32(0x524);average=average*0.5f;
        e.puti(0x3c,std::clamp(add32(e.i32(0x3c),1.0f>average?-4:4),0,255)); e.put8(0x297,4);
    }else if(shift==1 || shift==3){e.puti(0x3c,0);e.put8(0x297,shift==1?2:3);}
    else{
        std::int32_t delta=10;
        switch(e.i8(0x297)){
        case 1:delta=e.u32(0x210)<e.u32(0x48)?16:1;break;
        case 2:case 3:delta=32;break;
        case 4:delta=16;break;
        default:break;
        }
        auto c=add32(e.i32(0x3c),delta);
        if(e.i8(0x297)==1 && c>255)e.put8(0x297,0);
        e.puti(0x3c,std::min(c,255)); // No lower clamp on this original path.
    }
    finish_clutch(e,p);
}
void induce_spin(Bytes e,Bytes w){
    auto timer=e.u32(0x304);if(!timer)return;
    if(e.u32(0x308)==0)throw std::domain_error("nonzero spin timer with zero duration");
    float ratio=x87_float(fild_u32(timer)/fild_u32(e.u32(0x308)));
    w.putf(0x318,ratio*w.f32(0x318)); w.putf(0x40c,ratio*w.f32(0x40c));
    if(std::abs(int(w.i16(0x52e)))<0x2000)w.putf(0x500,ratio*w.f32(0x500));
    if(std::abs(int(w.i16(0x622)))<0x2000)w.putf(0x5f4,w.f32(0x5f4)*ratio);
    e.put32(0x304,timer-1);
}
void tire_grip(Bytes e,Bytes w,Bytes p,const std::array<Bytes,4>& wheels){
    for(std::size_t i=0;i<4;++i){
        auto wheel=wheels[i]; const auto axle=(i/2)*0x4c;
        if(wheel.f32(0x38)==0.0f)throw std::domain_error("zero tire reference load");
        float ratio=wheel.f32(0x34)/wheel.f32(0x38);
        const float maximum=p.f32(0xe40+axle);
        float squared=ratio*ratio;
        float loss=maximum-p.f32(0xed8+axle);loss=loss*squared;
        float coefficient=maximum-loss;
        if(0.0f>coefficient)coefficient=0.0f;
        if(coefficient>maximum)coefficient=maximum;
        float scaled=wheel.f32(0xe8)*coefficient;
        float load=wheel.f32((e.u32(4)&0x100000)?0x38:0x34);
        wheel.putf(0xc0,load*scaled);
        if(e.u8(0x283))wheel.putf(0xc0,0.0f);
    }
    induce_spin(e,w);
}
float brake_pressure(std::int32_t pedal,const Tables& t){
    return t.brake.f32(static_cast<std::size_t>(std::clamp(pedal,0,255))*4);
}
void engine_torque(Bytes e,Bytes p,const Tables& tables){
    float rpm=e.f32(0x21c);
    if(0.0f>rpm)rpm=0.0f;else if(rpm>p.f32(0x1690))rpm=p.f32(0x1690);
    const float friction=engine_friction(e,p,tables,rpm);
    auto table=torque_table(e,tables);
    const auto index=to_int(table.f32(0)*rpm);
    if(index<0)throw std::out_of_range("negative torque sample index");
    if(table.size()<12u)throw std::out_of_range("torque table has no samples");
    // Retail reaches a few indices past the 160-entry curve near the limiter;
    // on x86 those reads fall into adjacent static storage that is zero for the
    // owned executable. Saturating at the final (also zero) curve entry keeps
    // the native boundary deterministic without importing unrelated globals.
    const auto bounded_index=std::min<std::size_t>(
        static_cast<std::size_t>(index),(table.size()-12u)/4u);
    const auto sample_offset=8u+bounded_index*4u;
    float torque=p.f32(0x1728)*table.f32(4);
    torque=torque*table.f32(sample_offset);
    torque=torque+friction;
    auto accelerator=e.i32(0x34);
    if(accelerator<0||accelerator>255)throw std::domain_error("accelerator outside 0..255");
    float fraction=static_cast<float>(accelerator)*inv255();
    const auto angle=(accelerator*16384)/255;
    const float sine=x87_float(x87_sin(X87(radians(angle))));
    const auto gear=e.u32(0x208);
    float target=sine;
    if(gear){
        target=fraction*fraction;target=target*e.f32(0x22c);
        float other=1.0f-e.f32(0x22c);other=other*sine;target=target+other;
    }
    float limiter=e.f32(0x2a0);
    if(target>limiter){
        float mean=limiter+target;mean=mean*0.5f;
        e.puti(0x34,to_int(x87_sqrt(X87(mean))*bits(0x437f0000)));target=limiter;
    }
    torque=target*torque;
    const auto highest=p.u32(0x10a0);
    float step=e.i32(0x3c)<128?bits(0x3f4ccccd):(gear==highest?bits(0x3e4ccccd):bits(0x3ecccccd));
    float threshold=p.f32(0x1644);
    if(gear==highest){float extra=e.f32(0xe6c)*bits(0x3d851eb8);extra=extra+1.0f;threshold=extra*threshold;}
    if(rpm>=threshold){
        if(gear==highest&&e.i8(0x11)>=15)step=step*0.5f;
        limiter=limiter-step;
    }else limiter=step+limiter;
    if(0.0f>limiter)limiter=0.0f;else if(limiter>1.0f)limiter=1.0f;
    e.putf(0x2a0,limiter);
    const float idle=p.f32(0x15f8)>=rpm?engine_friction(e,p,tables,p.f32(0x15f8))*2.0f:0.0f;
    e.putf(0x220,idle);
    float out=idle+torque;out=out-friction;e.putf(0x214,out);
}
// PC 0x502070. x87 chain: two FCOS products (first spilled), FABS, FMUL 0.5,
// ratio product/quotient, single final f32 store.
float predicted_engine_speed(Bytes e,Bytes w,Bytes p,std::uint32_t gear){
    if(!gear)return e.f32(0x21c);
    if(gear>16)throw std::out_of_range("gear outside validated parameter view");
    const float first=x87_float(x87_cos(X87(radians(w.i16(0x622))))*w.f32(0x608));
    const float sum=x87_float(x87_cos(X87(radians(w.i16(0x52e))))*w.f32(0x514)+first);
    const X87 speed=x87_abs(X87(sum))*bits(0x3f000000);
    X87 ratio=p.f32((gear+0x3a)*0x4c);
    ratio*=p.f32(0x134c);ratio*=p.f32(0x10ec);
    if(p.f32(0xb94)==0)throw std::domain_error("zero wheel radius parameter");
    ratio/=p.f32(0xb94);
    return x87_float(ratio*speed);
}
void accel_operation(Bytes e,Bytes w,Bytes p){
    const float predicted=predicted_engine_speed(e,w,p,e.u32(0x208));
    switch(e.i8(0x296)){
    case 1:{
        if(e.i32(0x3c)==0&&(e.u32(0x1f4)==0||e.i32(0x29c)==0||p.f32(0x15f8)>e.f32(0x21c)))e.put8(0x296,2);
        auto v=std::max(add32(e.i32(0x29c),-64),0);e.puti(0x34,v);e.puti(0x29c,v);break;}
    case 2:
        if(e.i32(0x3c)<255){e.puti(0x34,0);e.puti(0x29c,0);}else e.put8(0x296,0);
        if(p.f32(0x15f8)>e.f32(0x21c)||e.u32(0x1f4)==0)e.put8(0x296,0);
        break;
    case 3:
        if(!(predicted>e.f32(0x21c))){e.put8(0x296,0);break;}
        e.puti(0x34,237);if(e.f32(0x21c)>=p.f32(0x1644))e.put8(0x296,4);break;
    case 4:e.puti(0x34,237);if(p.f32(0x1644)>predicted)e.put8(0x296,0);break;
    default:e.puti(0x29c,e.i32(0x34));break;
    }
}
} // namespace outrun::driving
