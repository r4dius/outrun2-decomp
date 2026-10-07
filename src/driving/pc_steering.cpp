#include "driving/pc_steering.hpp"
#include "driving/pc_x87.hpp"
#include <algorithm>
#include <cmath>
#include <limits>

namespace outrun::driving {
namespace {
float literal(std::uint32_t u){float f;std::memcpy(&f,&u,4);return f;}
std::int16_t as_i16(std::uint16_t u){std::int16_t s;std::memcpy(&s,&u,2);return s;}
std::int32_t trunc_i32(double v){
    if(!std::isfinite(v)||v<double(std::numeric_limits<std::int32_t>::min())||v>=2147483648.0)
        throw std::domain_error("steering integer conversion outside validated finite domain");
    return static_cast<std::int32_t>(v);
}
std::int32_t add_wrap(std::int32_t a,std::int32_t b){
    const auto u=static_cast<std::uint32_t>(a)+static_cast<std::uint32_t>(b);
    std::int32_t r;std::memcpy(&r,&u,4);return r;
}
std::int32_t mul_wrap(std::int32_t a,std::int32_t b){
    const auto u=static_cast<std::uint32_t>(a)*static_cast<std::uint32_t>(b);
    std::int32_t r;std::memcpy(&r,&u,4);return r;
}
std::int16_t avg_trunc_zero(std::int16_t a,std::int16_t b){
    const std::int32_t sum=std::int32_t(a)+std::int32_t(b);
    return static_cast<std::int16_t>(sum/2);
}
}

void steering_operation(Bytes e,Bytes w,Bytes p){
    // Geometry-derived scale. The original performs fsqrt/fpatan with x87 and
    // rounds only when storing the atan input and the final scalar.
    const float half_track=p.f32(0x130);
    const float outer=p.f32(0x193c);
    float squared=outer*outer;
    squared=squared-half_track*half_track;
    if(!(squared>=0.0f))throw std::domain_error("negative steering geometry radicand");
    const X87 root=x87_sqrt(X87(squared));                    // 449380
    const X87 denominator=root-X87(p.f32(0x17c))*0.5f;         // fld; fmul 628064; fsubp
    if(denominator==0.0f||!std::isfinite(denominator.v))throw std::domain_error("invalid steering geometry denominator");
    const float ratio=x87_float(X87(half_track)/denominator);   // fdivr; fstp before atan
    const X87 angle=x87_atan2(X87(ratio),X87(1.0f));           // 449350 fld; fld1; fpatan
    const float scaled=x87_float(angle*literal(0x4622f983));
    const std::int32_t geometry_angle=trunc_i32(scaled);
    if(geometry_angle==0)throw std::domain_error("zero steering geometry angle");

    // Keep the exact single-precision operation ordering used by SSE here.
    float inv=1.0f/static_cast<float>(geometry_angle);
    float a=p.f32(0x18a4)*inv;
    a=a*32768.0f;
    float b=p.f32(0x18f0)*inv;
    b=b*32768.0f;
    b=b-a;
    b=b+a; // seemingly redundant in source terms, but preserves PC rounding.
    if(b==0.0f||!std::isfinite(b))throw std::domain_error("invalid steering response denominator");
    float response=-1.0f/b;
    response=response*static_cast<float>(e.i16(0x202));
    response=response*p.f32(0x18a4);
    std::int32_t base=trunc_i32(response);
    e.put16(0x32,std::uint16_t(base));

    const auto speed=e.u32(0x1f4);
    if(speed<=20u)return;

    std::int32_t road=avg_trunc_zero(w.i16(0x52c),w.i16(0x620));
    road=add_wrap(road,std::int32_t(e.u32(0x16a)&0xffffu)*8);
    std::int16_t road16=as_i16(std::uint16_t(road));
    if(road16<-0x2000)road16=-0x2000;
    else if(road16>0x2000)road16=0x2000;

    const auto base16=as_i16(std::uint16_t(base));
    const auto product=mul_wrap(std::int32_t(base16),std::int32_t(road16));
    if(product>0){
        const std::int32_t difference=std::int32_t(road16)-std::int32_t(base16);
        // The PC code loads the command into BX, tests AX, then NEG EAX but
        // subsequently compares/sign-extends AX.  This is effectively a 16-bit
        // wrapping abs: -32768 stays -32768 instead of becoming +32768.
        std::int16_t input16=e.i16(0x202);
        if(input16<0) input16=as_i16(static_cast<std::uint16_t>(0u-static_cast<std::uint16_t>(input16)));
        if(input16>0x7000)input16=0x7000;
        const std::int32_t correction=mul_wrap(std::int32_t(input16),difference)/0x7000;
        const auto direction_product=mul_wrap(std::int32_t(road16),correction);
        if(direction_product>0){
            base=add_wrap(base,correction);
            e.put16(0x32,std::uint16_t(base));
            return;
        }
    }
    if(e.i8(0xd36)>0){
        base=add_wrap(base,std::int32_t(e.u32(0x16a)&0xffffu));
        e.put16(0x32,std::uint16_t(base));
    }
}

void toe_angle(Bytes e,Bytes w,Bytes p,std::int32_t analog_channel_1){
    auto wheels=embedded_wheels(w);
    float sign=1.0f;
    for(std::size_t i=0;i<4;++i){
        auto q=wheels[i];const std::size_t axle=i/2;
        q.putf(0xcc,p.f32((axle+0x1a)*0x4c)*sign);
        const float next_sign=0.0f-sign;
        const float drive=q.f32(0xb0);
        const float positive=drive>0.0f?drive:0.0f;
        const float negative=drive>0.0f?0.0f:drive;

        float normalized=std::abs(static_cast<float>(e.i16(0x202)))*literal(0x38010204);
        if(0.0f>normalized)normalized=0.0f;else if(normalized>1.0f)normalized=1.0f;

        float remaining=1.0f;
        float channel=static_cast<float>(analog_channel_1);
        channel=channel*literal(0x3b808081);
        remaining=remaining-channel;

        float out=p.f32((axle+0x1e)*0x4c)*positive;
        out=out+p.f32((axle+0x1c)*0x4c);
        float term=p.f32((axle+0x20)*0x4c)*negative;
        out=out-term;
        term=p.f32((axle+0x24)*0x4c)*remaining;
        out=out+term;
        term=p.f32((axle+0x22)*0x4c)*normalized;
        out=out+term;
        q.putf(0xd0,out);
        sign=next_sign;
    }
}

void tire_direction_angles(Bytes e,Bytes w){
    constexpr float angle_units=10430.3779296875f; // PC 0x6282c0
    auto wheels=embedded_wheels(w);
    for(std::size_t i=0;i<4;++i){
        const float scaled=wheels[i].f32(0xcc)*angle_units;
        const std::int32_t converted=trunc_i32(scaled);
        std::uint16_t angle=static_cast<std::uint16_t>(converted);
        if(i<2) angle=static_cast<std::uint16_t>(angle+static_cast<std::uint16_t>(e.i16(0x32)));
        wheels[i].put16(0x32,angle);
    }
    for(auto q:wheels){
        const std::uint16_t delta=static_cast<std::uint16_t>(
            static_cast<std::uint16_t>(q.i16(0xec))-static_cast<std::uint16_t>(q.i16(0x32)));
        q.put16(0xee,delta);
    }
    const std::int16_t old_front=e.i16(0x4c),old_rear=e.i16(0x4e);
    const std::int16_t front=avg_trunc_zero(wheels[0].i16(0xee),wheels[1].i16(0xee));
    const std::int16_t rear=avg_trunc_zero(wheels[2].i16(0xee),wheels[3].i16(0xee));
    e.put16(0x4c,static_cast<std::uint16_t>(front));
    e.put16(0x4e,static_cast<std::uint16_t>(rear));
    e.put16(0x168,static_cast<std::uint16_t>(std::uint16_t(front)-std::uint16_t(old_front)));
    e.put16(0x16a,static_cast<std::uint16_t>(std::uint16_t(rear)-std::uint16_t(old_rear)));
}

void steering_stage_from_inputs(Bytes e,Bytes w,Bytes p,std::int32_t analog_channel_1){
    steering_operation(e,w,p);
    toe_angle(e,w,p,analog_channel_1);
}
}
