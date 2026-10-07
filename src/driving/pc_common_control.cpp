#include "system/exe_image.hpp"
#include <cstdio>
#include "driving/pc_common_control.hpp"
#include "driving/pc_chassis.hpp"
#include "driving/pc_wrecker.hpp"
#include "driving/pc_wall_response.hpp"
#include "driving/pc_x87.hpp"
#include <cmath>
#include <cstdint>
#include <cstring>
#include <initializer_list>
#include <limits>
#include "driving/service_hole.hpp"
namespace outrun::driving {
namespace {
constexpr float kOne=1.0f;
constexpr float kTwo=2.0f;
constexpr float kInv255=0.003921568859368563f;
constexpr float kPosInputScale=1.537893695058301e-05f;
constexpr float kNegInputScale=-1.537893695058301e-05f;
constexpr float kAngleUnit=9.58738019107841e-05f;
constexpr float kTimerScale=0.11074196547269821f;
constexpr float kQuarter=0.25f;
constexpr float kThreeQuarter=0.75f;
constexpr float kTwenty=20.0f;
inline float addf(float a,float b){volatile float x=a+b;return x;}
inline float subf(float a,float b){volatile float x=a-b;return x;}
inline float mulf(float a,float b){volatile float x=a*b;return x;}
inline float i32f(std::int32_t v){volatile float x=static_cast<float>(v);return x;}
// x87 arithmetic below uses X87 (pc_x87.hpp): every FADD/FSUB/FMUL/FDIV/
// FSQRT rounds to the active precision control (24 bits in the game).
// FLD m32; FSIN; FSTP m32.
inline float x87_sin_spilled(float v){return x87_float(x87_sin(X87(v)));}
// FILD m32; FMUL m32 (angle unit); FSTP m32.
inline float x87_int_angle_spilled(std::int32_t v){return x87_float(X87(v)*kAngleUnit);}
// x86 CVTTSS2SI (SSE): truncation, 0x80000000 for NaN/out-of-range.
inline std::int32_t sse_cvttss2si(float v){
    if(!(v>-2147483904.0f&&v<2147483648.0f))return std::numeric_limits<std::int32_t>::min();
    return static_cast<std::int32_t>(v);
}
// MSVC _ftol2 (0x582194): FISTP m64 plus truncation fix-up; callers keep EAX,
// the low dword of the 64-bit truncation.
inline std::int32_t x87_ftol2_low32(X87 v){
    return static_cast<std::int32_t>(static_cast<std::uint32_t>(static_cast<std::uint64_t>(x87_ftol64(v))));
}
// FLD z; FLD y; FLD x; x*x; +y*y; +z*z; FSQRT (mxLength/mxUnitVector order).
inline X87 x87_length3(float x,float y,float z){return x87_sqrt((X87(x)*x+X87(y)*y)+X87(z)*z);}
inline float x87_abs_sin_i32(std::int32_t v){
    const float angle=x87_int_angle_spilled(v);
    float s=x87_sin_spilled(angle);
    std::uint32_t u{};std::memcpy(&u,&s,4);u&=0x7fffffffu;std::memcpy(&s,&u,4);return s;
}
inline float clamp_pc_0_1(float v){
    // COMISS leaves unordered values unchanged in this exact sequence.
    if(!std::isnan(v) && v<0.0f)v=0.0f;
    if(!std::isnan(v) && v>1.0f)v=1.0f;
    return v;
}
}
float rear_grip_curve_4a3260(Bytes event,Bytes params,std::int32_t index){
    if(index<0)throw std::out_of_range("rear grip curve negative index");
    const std::size_t base_off=std::size_t(index+0x32)*0x4cu;
    const std::size_t target_off=std::size_t(index+0x36)*0x4cu;
    float base=addf(params.f32(base_off),kTwo);
    if(event.i8(0xd36)<=0)return base;
    float t=mulf(i32f(event.i32(0xd94)),kTimerScale);
    if(!std::isnan(t)&&t<0.0f)t=0.0f;
    if(!std::isnan(t)&&t>kTwenty)t=kTwenty;
    const float table=params.f32(target_off);
    const float target=x87_float(X87(t)+table);                   // FLD; FADD; FSTP
    const float blend=x87_abs_sin_i32(event.i16(0xd46));
    // FLD blend; FLD target; FSUB base; FMULP; FADD base; FSTP.
    return x87_float(X87(blend)*(X87(target)-base)+base);
}
void rear_grip_ctrl(Bytes event,Bytes work,Bytes params,const RearGripInputs& in){
    event.check(0xd9c,4);work.check(0x244,1);params.check(0x1d64,4);
    auto timer=event.i8(0xd36);if(timer>0)event.put8(0xd36,std::uint8_t(timer-1));
    if(in.volume2>10 && in.old_volume2<=10 && in.volume1<250)event.put8(0xd36,40);
    if(in.volume1<250 && in.old_volume1>=250 && in.volume2>10)event.put8(0xd36,40);
    if(event.u8(0x13)==1u){
        for(auto off:{0xd94u,0xd98u,0xd9cu}){auto v=event.i32(off);if(v>0)event.puti(off,v-1);}
    }
    event.putf(0xd38,kOne);
    float zero=0.0f;
    if(event.u32(0x1f4)!=0u){
        const std::int16_t steer=event.i16(0x4e);const std::int32_t si=steer;
        const std::int32_t abssteer=si<0?-si:si;
        if(abssteer>=0x300 && event.i8(0xd36)>0){
            if(abssteer>0x600)event.put8(0xd36,12);
            const std::int32_t prod=std::int32_t(event.i16(0x16a))*si;
            event.putf(0xd38,prod<0?params.f32(0x1c34):(prod>0?params.f32(0x1ccc):params.f32(0x1c80)));
            float x{};
            if(steer>0)x=mulf(i32f(std::int32_t(event.i16(0x202))-0x7f00),kNegInputScale);
            else x=mulf(i32f(std::int32_t(event.i16(0x202))+0x7f00),kPosInputScale);
            x=clamp_pc_0_1(x);
            const float v1=mulf(i32f(in.volume1),kInv255);
            const float q=subf(kOne,v1);
            const float span=subf(params.f32(0x1d64),params.f32(0x1d18));
            const float qx=mulf(q,x);
            const float inv=subf(kOne,qx);
            const float amount=addf(mulf(inv,span),params.f32(0x1d18));
            event.putf(0xd38,subf(event.f32(0xd38),amount));
        }
    }
    if((work.u8(0x244)&8u)!=0u){
        const float s=x87_abs_sin_i32(event.i16(0x4e));
        float candidate=addf(mulf(s,kQuarter),kThreeQuarter);
        const float current=event.f32(0xd38);
        if(!std::isnan(candidate)&&!std::isnan(current)&&candidate>current)event.putf(0xd38,candidate);
    }
    const std::int16_t steer=event.i16(0x4e);const std::int32_t si=steer;const std::int32_t abssteer=si<0?-si:si;
    if(abssteer>0x3800){
        const float side=event.f32(0x26c);
        if((steer>0 && side>zero)||(steer<0 && side<zero))event.putf(0xd38,kOne);
    }
    event.putf(0xd38,clamp_pc_0_1(event.f32(0xd38)));
}
namespace {
constexpr float kSlipLow=0.9228497743606567f;
constexpr float kSlipHigh=1.8456995487213135f;
constexpr float kSlipRangeInv=1.0835999250411987f;
constexpr float kDotScale0=0.016611294820904732f;
constexpr float kAngleScale=10430.3779296875f;
constexpr float kSteerParamUnit=6.103515625e-05f;
constexpr float kHalf=0.5f;
constexpr float kAssistThreshold=0.949999988079071f;
constexpr float kAssistHi=0.15000000596046448f;
constexpr float kAssistLo=0.029999999329447746f;
constexpr float kAssistSteerScale=0.00010557432688074186f;
constexpr float kSlipForceScale=0.05098580941557884f;
constexpr float kHz=60.20000076293945f;
constexpr float kCurveScale=0.10197161883115768f;
constexpr float kOppositeScale=3.075787390116602e-05f;
constexpr float kD3cBlend=0.10000000149011612f;
constexpr float kD48BlendScale=0.00390625f;
inline float divf(float a,float b){volatile float x=a/b;return x;}
inline std::int32_t truncf32(float x){return static_cast<std::int32_t>(x);}
inline std::int16_t low16(std::int32_t x){return static_cast<std::int16_t>(static_cast<std::uint16_t>(x));}
float x87_mul_spilled(float a,float b){return x87_float(X87(a)*b);} // FLD; FMUL; FSTP
// FLD 1; FPATAN = atan(v/1), unaffected by the precision control.
inline X87 x87_atan(X87 v){return x87_atan2(v,X87(1.0f));}
std::int16_t x87_dot_scaled_i16(Bytes work){
    const float ax=work.f32(0x50),ay=work.f32(0x54),az=work.f32(0x58);
    const float bx=work.f32(0x628),by=work.f32(0x62c),bz=work.f32(0x630);
    // (az*bz + ay*by) + ax*bx, then *c0 *c1, one FSTP m32.
    const float v=x87_float(((X87(az)*bz+X87(ay)*by)+X87(ax)*bx)*kDotScale0*kAngleScale);
    return low16(sse_cvttss2si(v));
}
std::int32_t x87_cos_param_int_trunc(std::int32_t angle,float param,std::int32_t factor){
    const float af=x87_int_angle_spilled(angle);
    // FLD af; FCOS; FMUL param; FIMUL factor; FISTTP m32.
    return x87_ftol32(x87_cos(X87(af))*param*X87(factor));
}
std::int16_t x87_atan_scaled_i16(float v){
    const float q=x87_float(x87_atan(X87(v))*kAngleScale);
    return low16(sse_cvttss2si(q));
}
std::int16_t x87_dynamic_angle_i16(std::int32_t angle,float ratio,float param130){
    const float af=x87_int_angle_spilled(angle);
    const float cf=x87_float(x87_cos(X87(af)));
    const float base=x87_float(X87(ratio)/param130);
    // FLD af; FSIN; FSUBR base; FDIVR cf; FSTP.
    const float arg=x87_float(X87(cf)/(X87(base)-x87_sin(X87(af))));
    const float q=x87_float(x87_atan(X87(arg))*kAngleScale);
    return low16(sse_cvttss2si(q));
}
float abs_event202_scaled(std::int16_t v){
    const float fv=i32f(v);float a=fv;std::uint32_t u{};std::memcpy(&u,&a,4);u&=0x7fffffffu;std::memcpy(&a,&u,4);
    return x87_mul_spilled(a,kOppositeScale);
}
}

void slip_angle_ctrl(Bytes event,Bytes work,Bytes params,const SlipAngleInputs& inputs){
    event.check(0xd4a,2);work.check(0x630,4);params.check(0x23ec,4);
    float speed=event.f32(0x1c4),clamped{};
    if(speed<kSlipLow)clamped=kSlipLow;else if(speed>kSlipHigh)clamped=kSlipHigh;else clamped=speed;
    const float t=mulf(subf(clamped,kSlipLow),kSlipRangeInv);
    float a0,a1,b0,b1;
    if(event.i8(0xd36)>=12){a0=params.f32(0x1db0);b0=params.f32(0x1e48);a1=params.f32(0x1dfc);b1=params.f32(0x1e94);}
    else {a0=params.f32(0x1ee0);b0=params.f32(0x1f78);a1=params.f32(0x1f2c);b1=params.f32(0x1fc4);}
    const float coef0=addf(mulf(subf(b0,a0),t),a0);
    const float coef1=addf(mulf(subf(b1,a1),t),a1);
    const std::int16_t dot_i=x87_dot_scaled_i16(work);
    const std::int32_t cos_term=x87_cos_param_int_trunc(event.i16(0xd46),params.f32(0x2010),dot_i);
    std::int32_t edi=-std::int32_t(low16(cos_term));
    const float curve1=rear_grip_curve_4a3260(event,params,1);
    const std::int16_t d4a=event.i16(0xd4a),d46=event.i16(0xd46);
    float adjust{};
    if(d4a<0){
        if(d46<d4a){float q=mulf(i32f(std::int32_t(d46)-d4a),params.f32(0x2308));q=divf(q,i32f(-0x4000-std::int32_t(d4a)));adjust=addf(q,params.f32(0x22bc));}
        else if(d46>0){adjust=addf(mulf(mulf(i32f(d46),params.f32(0x23a0)),kSteerParamUnit),params.f32(0x2354));}
        else adjust=params.f32(0x23ec);
    }else{
        if(d46>d4a){float q=mulf(i32f(std::int32_t(d46)-d4a),params.f32(0x2308));q=divf(q,i32f(0x4000-std::int32_t(d4a)));adjust=addf(q,params.f32(0x22bc));}
        else if(d46<0){adjust=subf(params.f32(0x2354),mulf(mulf(i32f(d46),params.f32(0x23a0)),kSteerParamUnit));}
        else adjust=params.f32(0x23ec);
    }
    float grip=addf(event.f32(0xd38),adjust);
    if(!std::isnan(grip)&&grip<0.0f)grip=0.0f;else if(!std::isnan(grip)&&grip>1.0f)grip=1.0f;
    float force=mulf(mulf(grip,curve1),params.f32(0));force=mulf(force,kHalf);
    if(inputs.assist_gate!=0u && !std::isnan(inputs.assist_state) && inputs.assist_state<kAssistThreshold && (event.u8(0x2f0)&2u)==0u){
        const std::int32_t steer=event.i16(0x4e),abssteer=steer<0?-steer:steer;
        if(abssteer>0x250){float m=mulf(i32f(abssteer),kAssistSteerScale);if(!std::isnan(m)&&m>kAssistHi)m=kAssistHi;else if(!std::isnan(m)&&m<kAssistLo)m=kAssistLo;force=mulf(m,force);}
    }
    float q=mulf(params.f32(0),kSlipForceScale);
    float sp=mulf(event.f32(0x1c4),kHz);
    force=divf(force,q);force=divf(force,mulf(sp,sp));force=mulf(force,params.f32(0x130));
    const std::int16_t target=x87_atan_scaled_i16(force);

    const std::int16_t old=event.i16(0xd46);const std::int32_t oldi=old;
    std::int32_t d=truncf32(mulf(i32f(target),coef1));
    const std::int32_t negold=-oldi;
    if(negold<0){d=-d;if(d<negold)d=negold;}else if(d>negold)d=negold;
    const std::int32_t old_delta=event.i16(0xd48);
    std::int32_t delta=truncf32(divf(i32f(d),coef1));
    delta=delta-old_delta+edi;
    delta/=4;
    std::int32_t next_delta=old_delta+delta;
    const std::int32_t max_delta=truncf32(params.f32(0x205c));
    if(next_delta<-max_delta)next_delta=-max_delta;else if(next_delta>max_delta)next_delta=max_delta;
    next_delta=low16(next_delta);
    if(oldi+next_delta<-0x3400)next_delta=-0x3400-oldi;
    if(oldi+next_delta>0x3400)next_delta=0x3400-oldi;
    const std::int16_t new_delta=low16(next_delta);
    const std::int16_t new_angle=low16(oldi+std::int32_t(new_delta));
    event.put16(0xd48,std::uint16_t(new_delta));event.put16(0xd46,std::uint16_t(new_angle));

    float ratio=divf(params.f32(0),addf(divf(1.0f,params.f32(0x214)),1.0f));
    const float curve0=rear_grip_curve_4a3260(event,params,0);
    const float speed_hz=mulf(event.f32(0x1c4),kHz);
    const float curve_ratio=mulf(curve0,ratio);
    ratio=mulf(ratio,kCurveScale);ratio=divf(ratio,curve_ratio);ratio=mulf(ratio,mulf(speed_hz,speed_hz));
    edi=x87_dynamic_angle_i16(event.i16(0xd46),ratio,params.f32(0x130));

    const std::int32_t input=event.i16(0x32),angle=event.i16(0xd46);
    float opposite=0.0f;
    if((input-angle)*input<0){opposite=abs_event202_scaled(event.i16(0x202));if(!std::isnan(opposite)&&opposite<0.0f)opposite=0.0f;if(!std::isnan(opposite)&&opposite>1.0f)opposite=1.0f;}
    float d3c=event.f32(0xd3c);d3c=addf(d3c,mulf(subf(opposite,d3c),kD3cBlend));event.putf(0xd3c,d3c);
    const float fade=subf(1.0f,d3c);
    std::int32_t a=truncf32(mulf(fade,i32f(edi)));
    const std::int32_t input_diff=input-angle;
    std::int32_t signed_a=a;
    if(input_diff<0)signed_a=-signed_a;
    std::int32_t scaled=truncf32(mulf(i32f(signed_a),coef0));
    if(input_diff<0){if(scaled<input_diff)scaled=input_diff;}else if(scaled>input_diff)scaled=input_diff;
    const float neg_inv_coef=divf(-1.0f,coef0);
    std::int32_t ebx=truncf32(mulf(i32f(scaled),neg_inv_coef));ebx=input_diff+ebx;ebx=-ebx;
    if(angle*input>0){const std::int32_t aa=angle<0?-angle:angle,ii=input<0?-input:input;if(aa>ii)a=0;}
    if(input<0)a=-a;
    std::int32_t e=truncf32(mulf(i32f(a),coef0));
    if(input<0){if(e<input)e=input;}else if(e>input)e=input;
    e=truncf32(mulf(i32f(e),neg_inv_coef));
    std::int32_t edi2=angle-e-input;
    const std::int32_t d48=event.i16(0xd48);
    float mix=0.0f;
    if(d48*angle>=0){mix=mulf(i32f(d48<0?-d48:d48),kD48BlendScale);if(!std::isnan(mix)&&mix<0.0f)mix=0.0f;if(!std::isnan(mix)&&mix>1.0f)mix=1.0f;}
    const float blended=addf(mulf(subf(1.0f,mix),i32f(ebx)),mulf(i32f(edi2),mix));
    event.put16(0xd44,std::uint16_t(low16(truncf32(blended))));
    if(event.u8(0x283)!=0u){event.put16(0xd44,0);event.put16(0xd46,0);}
}
namespace {
float x87_vec_length_spilled(Bytes b,std::size_t off){
    return x87_float(x87_length3(b.f32(off),b.f32(off+4),b.f32(off+8)));
}
bool x87_float_gt_i32_angle_extended(float left,std::int32_t right){
    // FILD right; FMUL unit; FLD left; FCOMIP: the product stays unspilled.
    return X87(left)>X87(right)*kAngleUnit;
}
float x87_i32_angle(std::int32_t v){return x87_int_angle_spilled(v);}
float x87_sin_mul_spill(float angle,float factor){return x87_float(x87_sin(X87(angle))*factor);}
float x87_cos_mul_spill(float angle,float factor){return x87_float(x87_cos(X87(angle))*factor);}
// FLD angle; FSIN; FDIVR numerator; FSTP.
float x87_num_div_sin_spill(float numerator,float angle){return x87_float(X87(numerator)/x87_sin(X87(angle)));}
// FLD p214; FADD 1; FDIVR p130; FSTP.
float x87_param_ratio(float p130,float p214){return x87_float(X87(p130)/(X87(p214)+1.0f));}
float x87_cos_mul_add_spill(float angle,float factor,float add){return x87_float(x87_cos(X87(angle))*factor+add);}
// FLD y; FLD x; FPATAN; FSTP.
float x87_atan2_spilled(float y,float x){return x87_float(x87_atan2(X87(y),X87(x)));}
// FLD q; FABS; FMUL sign; FDIVR speed; FSTP.
float x87_speed_over_abs_ratio(float speed,float quotient,float sign){
    return x87_float(X87(speed)/(x87_abs(X87(quotient))*sign));
}
CourseProbe sub_vec_x87(CourseProbe a,CourseProbe b){
    return {x87_float(X87(a.x)-b.x),x87_float(X87(a.y)-b.y),x87_float(X87(a.z)-b.z)};
}
// mxProjectiveVector 0x40F280: dot=(ay*by+ax*bx)+bz*az kept live; a-dot*axis.
CourseProbe projective_vec_x87(CourseProbe a,CourseProbe axis){
    const X87 dot=(X87(a.y)*axis.y+X87(a.x)*axis.x)+X87(axis.z)*a.z;
    return {x87_float(X87(a.x)-dot*axis.x),x87_float(X87(a.y)-dot*axis.y),x87_float(X87(a.z)-dot*axis.z)};
}
CourseProbe scale_vec_x87(CourseProbe a,float factor){
    return {x87_float(X87(factor)*a.x),x87_float(X87(factor)*a.y),x87_float(X87(factor)*a.z)};
}
}

void cornering_ctrl(Bytes event,Bytes work,Bytes params,PcMatrixStack& matrices){
    event.check(0,0xd44+4);work.check(0,0x634);params.check(0,0x218);
    const std::int16_t steer=event.i16(0x4e);
    if(steer<std::int16_t(-0x4000)||steer>std::int16_t(0x4000)||event.u32(0x1f4)==0u)return;
    pc_matrix_push(matrices);
    const float speed=x87_vec_length_spilled(work,0x14c);
    const float theta0=x87_i32_angle(std::int32_t(event.i16(0xd44))+std::int32_t(event.i16(0x32)));
    const float theta1=x87_i32_angle(event.i16(0xd46));
    // PC compares spilled theta0 against theta1 still live in x87 after an
    // FST (not FSTP).  Rounded-equal floats can therefore select opposite
    // half-pi offsets depending on the hidden x87 remainder.
    float sign=x87_float_gt_i32_angle_extended(theta0,event.i16(0xd46))?1.0f:-1.0f;
    float abs_theta1=theta1;std::uint32_t au{};std::memcpy(&au,&abs_theta1,4);au&=0x7fffffffu;std::memcpy(&abs_theta1,&au,4);
    constexpr float halfpi=1.5707963705062866f;
    if(abs_theta1>halfpi)sign=mulf(sign,-1.0f);
    const float offset=mulf(sign,halfpi);
    const float A=addf(offset,theta0);
    const float B=addf(offset,theta1);
    float angle{};
    float velocity{};
    if(A==B){
        angle=subf(A,offset);
        velocity=0.0f;
    }else{
        const float diff=subf(A,B);
        const float sinB_p=x87_sin_mul_spill(B,params.f32(0x130));
        float q=x87_num_div_sin_spill(sinB_p,diff);
        std::uint32_t qu{};std::memcpy(&qu,&q,4);qu&=0x7fffffffu;std::memcpy(&q,&qu,4);
        // The PC also evaluates fabs(cos(B)*p130 + cos(diff)*q) here; its
        // result is discarded before the values below are formed.
        const float k=x87_param_ratio(params.f32(0x130),params.f32(0x214));
        const float x=x87_cos_mul_add_spill(A,q,k);
        const float y=x87_sin_mul_spill(A,q);
        const float phi=x87_atan2_spilled(y,x);
        const float num=x87_sin_mul_spill(A,q);
        const float quotient=x87_num_div_sin_spill(num,phi);
        velocity=x87_speed_over_abs_ratio(speed,quotient,sign);
        angle=subf(phi,offset);
    }
    float rate=subf(angle,event.f32(0xd40));
    rate=mulf(rate,kHz);
    velocity=subf(velocity,rate);
    velocity=subf(velocity,work.f32(0x144));
    velocity=mulf(velocity,kHz);
    work.put32(0xcc,0);work.putf(0xd0,velocity);work.put32(0xd4,0);
    event.putf(0xd40,angle);

    pc_matrix_identity(matrices);
    CourseProbe axis{work.f32(0x628),work.f32(0x62c),work.f32(0x630)};
    pc_matrix_rotate_axis(matrices,axis,angle);
    pc_matrix_multiply_current(matrices,work.sub(0x100,64));
    pc_matrix_multiply_current(matrices,work.sub(0x1e0,64));
    CourseProbe v{0.0f,0.0f,subf(0.0f,speed)};
    v=pc_matrix_vector(matrices,v);
    CourseProbe current{work.f32(0x14c),work.f32(0x150),work.f32(0x154)};
    v=sub_vec_x87(v,current);
    v=projective_vec_x87(v,axis);
    v=scale_vec_x87(v,kHz);
    work.putf(0xc0,v.x);work.putf(0xc4,v.y);work.putf(0xc8,v.z);
    pc_matrix_pop(matrices);
}

namespace {
CourseProbe d3dx_normalized_vec(CourseProbe v){
    return pc_normalize_vector_40ef00(v);
}
// mxUnitVector 0x40EEB0: length (x*x+y*y)+z*z; FSQRT; FCOM qword 0.0001
// (C0|C3 skip: <=, unordered); FLD1; FDIV len; component FMUL/FSTP.
// Returns the unspilled x87 length.
X87 unit_vector_ext(CourseProbe& v){
    const X87 len=x87_length3(v.x,v.y,v.z);
    if(len>X87(0.0001)){
        const X87 inv=X87(1.0f)/len;
        v.x=x87_float(inv*v.x);v.y=x87_float(inv*v.y);v.z=x87_float(inv*v.z);
    }
    return len;
}
CourseProbe add_vec_x87(CourseProbe a,CourseProbe b){
    return {x87_float(X87(a.x)+b.x),x87_float(X87(b.y)+a.y),x87_float(X87(b.z)+a.z)};
}
}
// Public signature (pc_matrix_stack.hpp) still returns long double.
long double pc_unit_vector_40eeb0(CourseProbe& v){return static_cast<long double>(unit_vector_ext(v).v);}
void calc_pl_body_force_limit_4a3c60(Bytes work,float threshold){
    work.check(0,0x158);
    constexpr float dt=0.016611294820904732f;
    CourseProbe velocity{work.f32(0x5c),work.f32(0x60),work.f32(0x64)};
    CourseProbe force{work.f32(0xc0),work.f32(0xc4),work.f32(0xc8)};
    CourseProbe predicted=scale_vec_x87(force,dt);
    predicted=add_vec_x87(predicted,{work.f32(0x14c),work.f32(0x150),work.f32(0x154)});
    CourseProbe delta=sub_vec_x87(predicted,velocity);
    delta=scale_vec_x87(delta,kHz);
    const CourseProbe direction=d3dx_normalized_vec(velocity);
    delta=projective_vec_x87(delta,direction);
    const X87 length=unit_vector_ext(delta);
    const X87 excess=length-threshold;
    float selected{};
    // PC compares zero with the unspilled x87 excess, but loads the rounded
    // float spill when the excess is non-negative (unordered also loads it).
    if(!(X87(0.0f)>excess))selected=x87_float(excess);
    selected=subf(0.0f,selected);
    delta=scale_vec_x87(delta,selected);
    force=add_vec_x87(force,delta);
    work.putf(0xc0,force.x);work.putf(0xc4,force.y);work.putf(0xc8,force.z);
}

}


namespace outrun::driving {
namespace {
float r032_vec_length_spill(CourseProbe v){return x87_float(x87_length3(v.x,v.y,v.z));} // mxLength 0x40F0E0
CourseProbe r032_projective(CourseProbe a,CourseProbe axis){return projective_vec_x87(a,axis);}
// 0x40F080: length; FCOM qword 0.0001; FDIVR target; component FMUL/FSTP.
CourseProbe r032_normalize_scale(CourseProbe v,float target){
    const float ox=v.x,oy=v.y,oz=v.z;
    const X87 len=x87_length3(ox,oy,oz);
    if(len>X87(0.0001)){const X87 scale=X87(target)/len;v={x87_float(scale*ox),x87_float(scale*oy),x87_float(scale*oz)};}
    return v;
}
float r032_x87_sin(float a){return x87_sin_spilled(a);}
float r032_x87_cos(float a){return x87_float(x87_cos(X87(a)));}
}
void assist_wanderer_4a5470(Bytes event,Bytes work,PcMatrixStack& matrices){
    event.check(0,0xe9c);work.check(0,0x634);
    if(event.u32(0xe90)!=0u)return;
    const auto remaining=event.i32(0xe88);if(remaining<=0)return;
    const auto duration=event.i32(0xe8c);if(duration<30)return;
    const auto half=(duration-(duration>>31))/2;
    float ratio{};
    if(remaining>=half)ratio=subf(2.0f,static_cast<float>(static_cast<float>(remaining)/static_cast<float>(half)));
    else ratio=static_cast<float>(static_cast<float>(remaining)/static_cast<float>(half));
    ratio=static_cast<float>(ratio/static_cast<float>(event.f32(0xe98)*0.13333334028720856f));
    ratio=clamp_pc_0_1(ratio);
    std::int16_t target=event.i16(0xd4c);
    const std::int16_t rawdiff=static_cast<std::int16_t>(std::uint16_t(std::uint16_t(event.i16(0xd4e))-std::uint16_t(event.i16(0xd4c))));
    const std::int32_t diff=rawdiff;
    const std::int32_t ad=diff<0?-diff:diff;
    if(ad>0x400){
        const float prod=static_cast<float>(static_cast<float>(diff)*event.f32(0xe94));
        if(!std::isnan(prod)&&prod>=0.0f){
            const auto h=(diff-(diff>>31))>>1;
            target=static_cast<std::int16_t>(std::uint16_t(target)+std::uint16_t(h));
        }
    }
    CourseProbe velocity{work.f32(0x5c),work.f32(0x60),work.f32(0x64)};
    const float speed=r032_vec_length_spill(velocity);
    pc_matrix_push(matrices);pc_matrix_unit_rotation(matrices);
    auto scaled_angle=[&](std::int16_t d){
        const float f=static_cast<float>(static_cast<float>(d)*ratio);
        const auto q=static_cast<std::int32_t>(f);
        const auto lo=static_cast<std::int16_t>(static_cast<std::uint16_t>(q));
        return x87_i32_angle(lo);
    };
    const std::int16_t d1=static_cast<std::int16_t>(std::uint16_t(target)-std::uint16_t(event.i16(0x160)));
    pc_matrix_rotate_y(matrices,scaled_angle(d1));
    velocity=pc_matrix_vector(matrices,velocity);
    const CourseProbe axis{work.f32(0x628),work.f32(0x62c),work.f32(0x630)};
    velocity=r032_projective(velocity,axis);
    velocity=r032_normalize_scale(velocity,speed);
    work.putf(0x5c,velocity.x);work.putf(0x60,velocity.y);work.putf(0x64,velocity.z);
    pc_matrix_unit_rotation(matrices);
    const std::int16_t d2=static_cast<std::int16_t>(std::uint16_t(target)-std::uint16_t(event.i16(0x2e)));
    pc_matrix_rotate_y(matrices,scaled_angle(d2));
    pc_matrix_multiply_current(matrices,work.sub(0x10,64));
    pc_matrix_store_rotation(matrices,work.sub(0x10,64));
    pc_matrix_pop(matrices);
}

void reb_pl_body_base_4a0200(Bytes event,Bytes work,PcMatrixStack& matrices){
    event.check(0,0x298);work.check(0,0x68);
    const float vx=work.f32(0x5c),vy=work.f32(0x60),vz=work.f32(0x64);
    const float sum=static_cast<float>(static_cast<float>(vz*vz)+static_cast<float>(vx*vx));
    const float speed=x87_float(x87_sqrt(X87(sum)));             // FLD; FSQRT; FSTP
    const float a=x87_i32_angle(event.i16(0x286));
    work.putf(0x5c,-x87_sin_mul_spill(a,speed));
    work.putf(0x60,vy);
    work.putf(0x64,-x87_cos_mul_spill(a,speed));
    bool rotate=true;
    if((event.u32(0x244)&0x00f00002u)!=0u){
        const std::int32_t delta=std::int32_t(event.i16(0x2e))-std::int32_t(event.i16(0x294));
        const std::int32_t ad=delta<0?-delta:delta;if(ad<0x800)rotate=false;
    }
    if(rotate){
        pc_matrix_unit_rotation(matrices);
        const std::int32_t q=std::int32_t(event.i16(0x288))/30;
        const float angle=static_cast<float>(static_cast<float>(q)*kAngleUnit);
        pc_matrix_rotate_y(matrices,angle);
        pc_matrix_multiply_current(matrices,work.sub(0x10,64));
        pc_matrix_store_rotation(matrices,work.sub(0x10,64));
    }
}
namespace {
#if defined(__GNUC__) || defined(__clang__)
#define OR2_R033_NOINLINE __attribute__((noinline))
#else
#define OR2_R033_NOINLINE
#endif
OR2_R033_NOINLINE CourseProbe r033_read3(Bytes b,std::size_t o){return {b.f32(o),b.f32(o+4),b.f32(o+8)};}
OR2_R033_NOINLINE void r033_write3(Bytes b,std::size_t o,const CourseProbe& v){b.putf(o,v.x);b.putf(o+4,v.y);b.putf(o+8,v.z);}
OR2_R033_NOINLINE CourseProbe r033_sub(CourseProbe a,CourseProbe b){return sub_vec_x87(a,b);}
OR2_R033_NOINLINE CourseProbe r033_add(CourseProbe a,CourseProbe b){return add_vec_x87(a,b);}
OR2_R033_NOINLINE CourseProbe r033_scale(CourseProbe a,float k){return scale_vec_x87(a,k);}
OR2_R033_NOINLINE CourseProbe r033_projective(CourseProbe a,CourseProbe axis){return r032_projective(a,axis);}
OR2_R033_NOINLINE CourseProbe r033_void_unit(CourseProbe v){return d3dx_normalized_vec(v);}
OR2_R033_NOINLINE float r033_unit(CourseProbe& v){
    // PC mxUnitVector returns the x87 length while normalising the input.
    // Keeping this behind a call boundary mirrors the original vector helper
    // and prevents unrelated long-double temporaries from occupying x87 regs.
    return x87_float(unit_vector_ext(v));
}
OR2_R033_NOINLINE CourseProbe r033_heading(std::int16_t a){
    const float angle=x87_i32_angle(a);
    return {-x87_sin_mul_spill(angle,1.0f),0.0f,-x87_cos_mul_spill(angle,1.0f)};
}
float r033_sqrt_spill(float v){return x87_float(x87_sqrt(X87(v)));}
CourseProbe r033_choose_rebound_delta(Bytes event,PcRoadInfoContext& road,CourseProbe fallback){
    if(event.u32(0x5c)==0u || event.i16(0x64)>=50)return fallback;
    std::array<std::uint8_t,16> pa{},pb{};
    for(unsigned k=0;k<16;++k)pa[k]=pb[k]=event.u8(0x5c+k);
    Bytes aplace(pa.data(),pa.size()),bplace(pb.data(),pb.size());
    aplace.puti(4,100);bplace.puti(4,101);
    std::array<std::uint8_t,0x64> ra{},rb{};Bytes a(ra.data(),ra.size()),b(rb.data(),rb.size());
    pc_get_cs_road_info_by_cs_len(a,aplace,event.i32(0x1c0),road);
    pc_get_cs_road_info_by_cs_len(b,bplace,event.i32(0x1c0),road);
    const auto pos=r033_read3(event,0x14),ca=r033_read3(a,0x08),cb=r033_read3(b,0x08);
    const float da=static_cast<float>(course_vec3_distance({pos.x,pos.y,pos.z},{ca.x,ca.y,ca.z}));
    const X87 db=X87(course_vec3_distance({pos.x,pos.y,pos.z},{cb.x,cb.y,cb.z}));
    const auto chosen=X87(da)<=db?ca:cb;
    return r033_sub(chosen,pos);
}
}

void reb_pl_body_sub3_4a08f0(Bytes event,Bytes work,PcRoadInfoContext& road){
    event.check(0,0xd68);work.check(0,0x634);
    if((event.u8(0x06)&1u)!=0u){
        reb_pl_body_base_4a0200(event,work,road.matrices);
        event.put32(0x290,0u);
        return;
    }
    const CourseProbe axis=r033_read3(work,0x628);
    CourseProbe current=r033_projective(r033_heading(event.i16(0xd4c)),axis);current=r033_void_unit(current);
    CourseProbe target=r033_projective(r033_heading(event.i16(0xd4e)),axis);target=r033_void_unit(target);
    CourseProbe delta=r033_sub(r033_read3(event,0xd5c),r033_read3(event,0x14));
    delta=r033_choose_rebound_delta(event,road,delta);
    delta=r033_projective(delta,current);
    delta=r033_projective(delta,target);
    float lateral=r033_unit(delta);
    const std::uint8_t count=event.u8(0x283);
    if(count>1u)lateral=static_cast<float>(static_cast<float>(lateral*1.5f)/static_cast<float>(count));
    delta=r033_scale(delta,lateral);
    float forward=0.0f;
    const float target_speed=event.f32(0x28c);
    if(!std::isnan(target_speed)&&!std::isnan(lateral)&&target_speed>lateral){
        const float q=static_cast<float>(static_cast<float>(target_speed*target_speed)-static_cast<float>(lateral*lateral));
        forward=r033_sqrt_spill(q);
    }
    CourseProbe velocity=r033_add(delta,r033_scale(current,forward));
    velocity=r033_scale(velocity,60.20000076293945f);
    velocity=r033_projective(velocity,axis);
    r033_write3(work,0x5c,velocity);

    const std::int16_t raw=static_cast<std::int16_t>(std::uint16_t(event.i16(0xd4e))-std::uint16_t(event.i16(0x2e)));
    const std::int32_t q1=std::int32_t(raw)/std::int32_t(count);
    const std::int16_t remain=static_cast<std::int16_t>(std::uint16_t(raw)-std::uint16_t(q1));
    const std::int32_t q2=std::int32_t(remain)/std::int32_t(count);
    const std::int16_t step=static_cast<std::int16_t>(std::uint16_t(q1+q2));
    pc_matrix_unit_rotation(road.matrices);
    pc_matrix_rotate_y(road.matrices,x87_i32_angle(step));
    pc_matrix_multiply_current(road.matrices,work.sub(0x10,64));
    pc_matrix_store_rotation(road.matrices,work.sub(0x10,64));
}


namespace {
OR2_R033_NOINLINE X87 r033_length_ext(CourseProbe v){return x87_length3(v.x,v.y,v.z);}
OR2_R033_NOINLINE float r033_length_spill(CourseProbe v){return x87_float(r033_length_ext(v));}
OR2_R033_NOINLINE CourseProbe r033_scale_unit(CourseProbe v,float target){return r032_normalize_scale(v,target);}
OR2_R033_NOINLINE X87 r033_cos_ext(float a){return x87_cos(X87(a));}
OR2_R033_NOINLINE float r033_sqrt_xz(float x,float z){
    const float zz=static_cast<float>(z*z);const float xx=static_cast<float>(x*x);const float sum=static_cast<float>(zz+xx);return r033_sqrt_spill(sum);
}
std::int16_t r033_s16_sub(std::int16_t a,std::int16_t b){return static_cast<std::int16_t>(std::uint16_t(a)-std::uint16_t(b));}
}

void reb_pl_body_sub2_4a0320(Bytes event,Bytes work,PcRoadInfoContext& road,const PcStageViews& stages){
    event.check(0,0xd68);work.check(0,0x634);
    if((event.u8(0x06)&1u)!=0u){
        reb_pl_body_base_4a0200(event,work,road.matrices);
        event.put32(0x290,0u);
        return;
    }
    std::int16_t target=event.i16(0xd4e);
    const std::int16_t current_angle=event.i16(0xd4c);
    std::int16_t rawdiff=r033_s16_sub(target,current_angle);
    const std::int32_t diff=rawdiff;
    const std::int32_t ad=diff<0?-diff:diff;
    if(ad>0x400){
        const float prod=static_cast<float>(static_cast<float>(diff)*event.f32(0x26c));
        if(!std::isnan(prod)&&prod<0.0f)target=current_angle;
    }
    const std::uint8_t count=event.u8(0x283);
    const std::int16_t old286=event.i16(0x286);
    rawdiff=r033_s16_sub(target,current_angle);
    const std::int32_t half=std::int32_t(rawdiff)/2;
    const std::uint16_t midpoint=static_cast<std::uint16_t>(std::uint16_t(target)-std::uint16_t(half));
    const std::int16_t to_mid=static_cast<std::int16_t>(midpoint-std::uint16_t(old286));
    const std::int32_t step286=std::int32_t(to_mid)/std::int32_t(count);
    event.put16(0x286,static_cast<std::uint16_t>(std::uint16_t(old286)+std::uint16_t(step286)));

    CourseProbe velocity=r033_read3(work,0x5c);
    const float planar=r033_sqrt_xz(velocity.x,velocity.z);
    const float steer=x87_i32_angle(event.i16(0x286));
    velocity.x=-x87_sin_mul_spill(steer,planar);
    velocity.z=-x87_cos_mul_spill(steer,planar);
    const float speed=r033_length_spill(velocity);
    const CourseProbe axis=r033_read3(work,0x628);
    velocity=r033_projective(velocity,axis);
    velocity=r033_scale_unit(velocity,speed);
    r033_write3(work,0x5c,velocity);

    const auto stage=pc_stage_number(stages,event.u32(0x68));
    bool skip_correction=false;
    if((stage==0x1c||stage==0x12) && event.u32(0x5c)==0u && (event.u32(0x04)&0x00020000u)!=0u && count<=0x16u){
        event.put32(0x290,0u);skip_correction=true;
    }
    if(!skip_correction){
        CourseProbe direction=r033_projective(r033_heading(current_angle),axis);
        direction=r033_void_unit(direction);
        CourseProbe delta=r033_sub(r033_read3(event,0xd5c),r033_read3(event,0x14));
        delta=r033_choose_rebound_delta(event,road,delta);

        const float denom=event.f32(0x26c);
        const float absden=std::fabs(denom);
        constexpr float eps=1.1920928955078125e-7f;
        if(!(eps>absden)){
            const float r1=static_cast<float>(static_cast<float>(event.f32(0x264)+2.0f)/denom);
            const float r2=static_cast<float>(static_cast<float>(event.f32(0x268)-2.0f)/denom);
            CourseProbe a=r033_scale(delta,r1),b=r033_scale(delta,r2);
            const float la=r033_length_spill(a);const X87 lb=r033_length_ext(b);
            if(X87(la)>lb)a=b;
            const float ld=r033_length_spill(delta);const X87 lc=r033_length_ext(a);
            if(X87(ld)>lc)delta=a;
        }

        CourseProbe damp=r033_scale(r033_read3(work,0x5c),0.016611294820904732f);
        const float countf=static_cast<float>(count);
        const float damp_ratio=static_cast<float>(1.0f-static_cast<float>(countf*0.03333333507180214f));
        damp=r033_scale(damp,damp_ratio);
        delta=r033_sub(delta,damp);
        delta=r033_projective(delta,direction);
        const float lateral=r033_unit(delta);
        const float narg=static_cast<float>(countf*0.05235987901687622f);
        const float factor_n=x87_float(X87(1.0f)-r033_cos_ext(narg));
        const float prevf=static_cast<float>(std::int32_t(count)-1);
        const float parg=static_cast<float>(prevf*0.05235987901687622f);
        const X87 factor_prev=X87(1.0f)-r033_cos_ext(parg);
        const X87 ratio=X87(1.0f)-factor_prev/factor_n;
        const float correction=x87_float(ratio*lateral);
        delta=r033_scale(delta,correction);
        delta=r033_projective(delta,axis);
        const X87 correction_len=r033_length_ext(delta);
        const X87 max_step=X87(speed)*0.016611294820904732f;
        if(!(correction_len>max_step)){
            pc_matrix_identity(road.matrices);
            pc_matrix_translate_vector(road.matrices,delta);
            pc_matrix_multiply_current(road.matrices,work.sub(0x10,64));
            pc_matrix_get(road.matrices,work.sub(0x10,64));
        }
    }

    const std::int16_t raw=r033_s16_sub(target,event.i16(0x2e));
    const std::int32_t q1=std::int32_t(raw)/std::int32_t(count);
    const std::int16_t remain=static_cast<std::int16_t>(std::uint16_t(raw)-std::uint16_t(q1));
    const std::int32_t q2=std::int32_t(remain)/std::int32_t(count);
    const std::int16_t rot=static_cast<std::int16_t>(std::uint16_t(q1+q2));
    pc_matrix_unit_rotation(road.matrices);
    pc_matrix_rotate_y(road.matrices,x87_i32_angle(rot));
    pc_matrix_multiply_current(road.matrices,work.sub(0x10,64));
    pc_matrix_store_rotation(road.matrices,work.sub(0x10,64));
}


void reb_pl_body_4a62e0(Bytes event,Bytes work,PcRoadInfoContext& road,const PcStageViews& stages){
    event.check(0,0xd94);work.check(0,0x634);
    const auto count=event.u8(0x283);
    if(count==0u){const auto delay=event.u8(0x284);if(delay>0u)event.put8(0x284,std::uint8_t(delay-1u));return;}
    pc_matrix_push(road.matrices);
    const auto state=event.u32(0x290);
    if(state==1u)reb_pl_body_sub2_4a0320(event,work,road,stages);
    else if(state==2u)reb_pl_body_sub3_4a08f0(event,work,road);
    else reb_pl_body_base_4a0200(event,work,road.matrices);
    event.put16(0x4e,0u);event.put16(0x4c,0u);
    const auto next=std::uint8_t(event.u8(0x283)-1u);event.put8(0x283,next);if(next==0u)event.puti(0xd90,60);
    work.putf(0xc0,0.0f);work.putf(0xc4,0.0f);work.putf(0xc8,0.0f);work.putf(0xcc,0.0f);work.putf(0xd0,0.0f);work.putf(0xd4,0.0f);
    CourseProbe v=r033_projective(r033_read3(work,0x68),r033_read3(work,0x628));r033_write3(work,0x68,v);
    pc_matrix_pop(road.matrices);
}

namespace {
OR2_R033_NOINLINE std::int32_t r034_get_max_road_pnum(const CourseWorldTables& tables,std::uint32_t type){
    if(type>=tables.courses.size())return 0;
    const auto& runs=tables.courses[type].runs;
    if(!runs.present || runs.header.size()==0)return 0;
    runs.header.check(0x0c,4);
    return runs.header.i32(0x0c)-1;
}
OR2_R033_NOINLINE float r034_wrec_angular_velocity(std::int32_t quotient){
    constexpr float kAccel=3624.0400390625f;
    return x87_float(X87(quotient)*kAngleUnit*kAccel);            // FILD; FMUL; FMUL; FSTP
}
}


namespace {
OR2_R033_NOINLINE std::int32_t r034_cvttss2si(float v){return sse_cvttss2si(v);}
OR2_R033_NOINLINE float r034_one_minus_x87(float r){return x87_float(X87(1.0f)-r);} // FLD1; FSUB; FSTP
OR2_R033_NOINLINE CourseProbe r034_linear_combo(CourseProbe a,float ka,CourseProbe b,float kb){
    // X: ka*ax + kb*bx kept live.  Y: kb*by spilled, then ka*ay + spill.
    // Z: both products spilled, then FLD bz; FADD az.
    const float by=x87_float(X87(kb)*b.y);
    const float az=x87_float(X87(ka)*a.z);
    const float bz=x87_float(X87(kb)*b.z);
    return {x87_float(X87(ka)*a.x+X87(kb)*b.x),
            x87_float(X87(ka)*a.y+by),
            x87_float(X87(bz)+az)};
}
// mxOuterProduct: st1-st0 per component, each spilled.
OR2_R033_NOINLINE CourseProbe r034_cross(CourseProbe a,CourseProbe b){
    return {x87_float(X87(b.z)*a.y-X87(a.z)*b.y),
            x87_float(X87(a.z)*b.x-X87(b.z)*a.x),
            x87_float(X87(a.x)*b.y-X87(b.x)*a.y)};
}
OR2_R033_NOINLINE float r034_wrap_angle(float d){
    constexpr float npi=-3.1415927410125732f,pi=3.1415927410125732f,twopi=6.2831854820251465f;
    if(!std::isnan(d)&&d<npi)d=addf(d,twopi);else if(!std::isnan(d)&&d>pi)d=subf(d,twopi);return d;
}
OR2_R033_NOINLINE float r034_x87_abs_spill(float x){return x87_float(x87_abs(X87(x)));} // FLD; FABS; FSTP
struct R034SinSquare { float sin{};float abs_sin{};float base{}; };
OR2_R033_NOINLINE R034SinSquare r034_sin_square_base(std::int32_t v,float param){
    R034SinSquare o{};
    const float angle=x87_int_angle_spilled(v);
    // FLD angle; FSIN; FST s (live value kept); FMUL s; FMUL param; FSTP.
    const X87 s=x87_sin(X87(angle));
    o.sin=x87_float(s);
    o.base=x87_float(s*o.sin*param);
    o.abs_sin=r034_x87_abs_spill(o.sin);return o;
}
OR2_R033_NOINLINE float r034_abs_sin_squared(std::int32_t v){
    const float angle=x87_i32_angle(v);const float s=x87_sin_spilled(angle);float a=r034_x87_abs_spill(s);return x87_float(X87(a)*a);
}
OR2_R033_NOINLINE float r034_path_angle(CourseProbe a,CourseProbe b){
    const float dx=subf(b.x,a.x),dz=subf(b.z,a.z);return x87_atan2_spilled(subf(0.0f,dx),subf(0.0f,dz));
}
OR2_R033_NOINLINE float r034_speed(Bytes work){return r033_length_spill(r033_read3(work,0x14c));}
OR2_R033_NOINLINE bool r034_course_gate(std::uint32_t type,const PcAssCompulsiveMoveContext& c){return type==0u?c.primary_course_gate:c.secondary_course_gate;}
OR2_R033_NOINLINE std::array<std::uint8_t,16> r034_place_from_event(Bytes event){std::array<std::uint8_t,16> p{};for(unsigned k=0;k<16;++k)p[k]=event.u8(0x5c+k);return p;}
OR2_R033_NOINLINE std::array<std::uint8_t,0x64> r034_road(Bytes place,std::int32_t hint,PcRoadInfoContext& c){std::array<std::uint8_t,0x64> r{};pc_get_cs_road_info_by_cs_len(Bytes(r.data(),r.size()),place,hint,c);return r;}
OR2_R033_NOINLINE std::array<std::uint8_t,0x64> r034_next_road(std::array<std::uint8_t,16>& place,std::int32_t step,const std::array<std::uint8_t,0x64>& fallback,std::int32_t hint,PcAssCompulsiveMoveContext& c){
    Bytes p(place.data(),place.size());pc_advance_on_road_place(p,step,c.advance);if(!r034_course_gate(p.u32(0),c))return fallback;return r034_road(p,hint,c.road);
}
OR2_R033_NOINLINE void r034_apply_velocity_correction(Bytes work,CourseProbe heading,CourseProbe predicted,float speed,float coeff){
    CourseProbe corr=r033_scale(r033_sub(heading,predicted),coeff);predicted=r033_add(predicted,corr);r033_unit(predicted);predicted=r033_scale(predicted,speed);
    CourseProbe diff=r033_sub(predicted,r033_read3(work,0x14c));diff=r033_projective(diff,r033_read3(work,0x628));r033_write3(work,0xc0,r033_scale(diff,60.20000076293945f));
}
}

void wrec_pl_body_4a0c70(Bytes event,Bytes work,Bytes body_params,Bytes wheel_block,PcPlWreckerContext& context){
    event.check(0,0xd8c);work.check(0,0xd8);
    std::int32_t countdown=event.i16(0xd68);
    if(countdown<=0)return;
    --countdown;
    event.put16(0xd68,static_cast<std::uint16_t>(static_cast<std::int16_t>(countdown)));
    event.put16(0xd50,0u);event.put16(0xd44,0u);event.put16(0xd46,0u);
    work.putf(0x74,0.0f);work.putf(0x7c,0.0f);work.putf(0x6c,0.0f);
    const CourseProbe zero{0.0f,0.0f,0.0f};
    if(countdown<=0){
        r033_write3(work,0x5c,zero);r033_write3(work,0x50,zero);r033_write3(work,0xc0,zero);r033_write3(work,0xcc,zero);
        pc_pl_wrecker(event,work,body_params,wheel_block,1,context);
        return;
    }

    const auto stage=pc_stage_number(context.advance.stages,event.u32(0x68));
    if((stage==0x1c||stage==0x12) && event.u32(0x5c)==0u && (event.u32(0x04)&0x00020000u)!=0u){
        const std::int32_t initial=event.i16(0xd6a);
        const std::int32_t threshold=(initial*3)/4;
        if(countdown<=threshold){
            std::array<std::uint8_t,16> place_mem{};for(unsigned k=0;k<16;++k)place_mem[k]=event.u8(0xd7c+k);Bytes place(place_mem.data(),place_mem.size());
            std::array<std::uint8_t,0x64> a_mem{},b_mem{};Bytes a(a_mem.data(),a_mem.size()),b(b_mem.data(),b_mem.size());
            pc_get_cs_road_info_by_cs_len(a,place,0,context.road);
            const std::int32_t max_pnum=r034_get_max_road_pnum(context.road.tables,place.u32(0));
            pc_get_cs_road_info_by_cs_len(b,place,max_pnum,context.road);
            const auto pos=r033_read3(event,0x14),ca=r033_read3(a,0x08),cb=r033_read3(b,0x08);
            const float da=static_cast<float>(course_vec3_distance({pos.x,pos.y,pos.z},{ca.x,ca.y,ca.z}));
            const X87 db=X87(course_vec3_distance({pos.x,pos.y,pos.z},{cb.x,cb.y,cb.z}));
            const Bytes chosen=X87(da)>=db?b:a;
            r033_write3(event,0xd6c,r033_read3(chosen,0x08));
            const float scaled=static_cast<float>(chosen.f32(0x14)*10430.3779296875f);
            const std::int32_t angle_i=static_cast<std::int32_t>(std::trunc(scaled));
            event.put16(0xd78,static_cast<std::uint16_t>(static_cast<std::int16_t>(angle_i)));
        }
    }

    CourseProbe delta=r033_sub(r033_read3(event,0xd6c),r033_read3(event,0x14));
    const float inv=1.0f/static_cast<float>(countdown);
    delta=r033_scale(delta,inv);
    delta=r033_scale(delta,3624.0400390625f);
    r033_write3(work,0xc0,delta);
    const std::uint32_t state=(event.u32(0x2f0)>>2)&0x1fu;
    if(state!=2u&&state!=5u)r033_write3(work,0x5c,zero);
    else {work.putf(0x5c,0.0f);work.putf(0x64,0.0f);work.putf(0xc4,0.0f);}
    work.putf(0x54,0.0f);work.putf(0xcc,0.0f);work.putf(0xd4,0.0f);
    const std::int16_t raw=static_cast<std::int16_t>(std::uint16_t(event.i16(0xd78))-std::uint16_t(event.i16(0x2e)));
    const std::int32_t q=std::int32_t(raw)/countdown;
    const std::int16_t q16=static_cast<std::int16_t>(q);
    work.putf(0xd0,r034_wrec_angular_velocity(q16));
}


void ass_compulsive_move_5184b0(Bytes event,Bytes work,Bytes body_params,PcAssCompulsiveMoveContext& context){
    event.check(0,0xd5c+12);work.check(0,0x634);body_params.check(0,0x2274);
    const float speed=r034_speed(work);
    auto place=r034_place_from_event(event);Bytes pl(place.data(),place.size());
    auto r0=r034_road(pl,event.i32(0x1c0),context.road);
    auto r1=r034_next_road(place,1,r0,event.i32(0x1c0),context);
    auto r2=r034_next_road(place,1,r1,event.i32(0x1c0),context);
    auto r3=r034_next_road(place,14,r2,event.i32(0x1c0),context);
    auto r4=r034_next_road(place,1,r3,event.i32(0x1c0),context);
    Bytes b0(r0.data(),r0.size()),b1(r1.data(),r1.size()),b2(r2.data(),r2.size()),b3(r3.data(),r3.size()),b4(r4.data(),r4.size());

    const float denominator=subf(event.f32(0x274),event.f32(0x270));
    const float absden=r034_x87_abs_spill(denominator);
    constexpr float eps=9.9999997473787516e-05f;
    float ratio{};if(!std::isnan(absden)&&eps>absden)ratio=0.0f;else ratio=event.f32(0x274)/denominator;

    float d01=r034_wrap_angle(subf(b1.f32(0x14),b0.f32(0x14)));
    float d02=r034_wrap_angle(subf(b2.f32(0x14),b0.f32(0x14)));
    float d12=r034_wrap_angle(subf(b2.f32(0x14),b1.f32(0x14)));
    const float a01=r034_x87_abs_spill(d01),a02=r034_x87_abs_spill(d02);
    float selected{};
    if(std::isnan(a02)||std::isnan(a01)||a02<a01)selected=addf(b0.f32(0x14),mulf(d02,ratio));
    else{
        const float a12=r034_x87_abs_spill(d12);const float twice=x87_float(X87(a12)+a12);
        if(std::isnan(twice)||std::isnan(a01)||twice<a01)selected=b1.f32(0x14);else selected=addf(b0.f32(0x14),mulf(d01,ratio));
    }
    const std::int32_t d4c_i=r034_cvttss2si(mulf(selected,10430.3779296875f));event.put16(0xd4c,static_cast<std::uint16_t>(static_cast<std::int16_t>(d4c_i)));
    const float one_minus=r034_one_minus_x87(ratio);
    const std::int16_t raw160=r033_s16_sub(event.i16(0x160),event.i16(0xd4c));const std::int32_t rawi=raw160;const std::int32_t absdiff=rawi<0?-rawi:rawi;
    r033_write3(event,0xd5c,r034_linear_combo(r033_read3(b1,0x08),ratio,r033_read3(b0,0x08),one_minus));

    CourseProbe heading=r033_heading(event.i16(0xd4c));heading=r033_projective(heading,r033_read3(work,0x628));r033_unit(heading);
    const CourseProbe ca=r034_linear_combo(r033_read3(b0,0x08),one_minus,r033_read3(b1,0x08),ratio);
    const CourseProbe cb=r034_linear_combo(r033_read3(b3,0x08),one_minus,r033_read3(b4,0x08),ratio);
    const float path=r034_path_angle(ca,cb);event.put16(0xd4e,static_cast<std::uint16_t>(static_cast<std::int16_t>(r034_cvttss2si(mulf(path,10430.3779296875f)))));

    if(static_cast<std::int16_t>(static_cast<std::uint16_t>(absdiff))<std::int16_t(0x4000)){
        const float a=event.f32(0x264),b=event.f32(0x268);const bool equal=(!std::isnan(a)&&!std::isnan(b)&&a==b);
        if(!equal){
            CourseProbe predicted=r033_scale(r033_read3(work,0xc0),0.016611294820904732f);predicted=r033_add(predicted,r033_read3(work,0x14c));r033_unit(predicted);
            const CourseProbe cross=r034_cross(heading,predicted);
            const auto ss=r034_sin_square_base(event.i16(0xd46),body_params.f32(0x20f4));
            const X87 prod=X87(cross.y)*ss.sin;
            float coeff=(std::isnan(prod.v)||!(prod<X87(0.0f)))?body_params.f32(0x218c):body_params.f32(0x2140);
            coeff=mulf(coeff,ss.abs_sin);if(event.i8(0xd36)<=0)coeff=ss.base;
            r034_apply_velocity_correction(work,heading,predicted,speed,coeff);
        }
        if(!equal){
            CourseProbe predicted=r033_scale(r033_read3(work,0xc0),0.016611294820904732f);predicted=r033_add(predicted,r033_read3(work,0x14c));r033_unit(predicted);
            const CourseProbe cross=r034_cross(heading,predicted);const float cy=cross.y;
            const std::int32_t delta=std::int32_t(event.i16(0xd4e))-std::int32_t(event.i16(0xd4c));const std::int32_t ad=delta<0?-delta:delta;
            bool skip=false;if(ad>0x100){if(!std::isnan(cy)&&cy>0.0f&&delta<0&&event.i16(0x4e)<0)skip=true;if(!std::isnan(cy)&&cy<0.0f&&delta>0&&event.i16(0x4e)>0)skip=true;}
            if(!skip){
                float side=0.0f;const float s=event.f32(0x26c);
                if(!std::isnan(s)&&s<0.0f&&!std::isnan(cy)&&cy>0.0f){const float den=subf(s,event.f32(0x268));const float test=subf(0.0f,den);if(!std::isnan(test)&&test>eps)side=s/den;}
                else if(!std::isnan(s)&&s>0.0f&&!std::isnan(cy)&&cy<0.0f){const float den=subf(s,event.f32(0x264));if(!std::isnan(den)&&den>eps)side=s/den;}
                std::int32_t steer=event.i16(0xd46),target=std::int32_t(event.i16(0x4e))*2;if(target>0){if(steer>target)steer=target;}else if(steer<target)steer=target;
                const float sin2=r034_abs_sin_squared(steer);
                float timer=mulf(static_cast<float>(event.i8(0xd36)),0.0833333358168602f);if(timer<0.0f)timer=0.0f;else if(timer>1.0f)timer=1.0f;
                const float p1=mulf(body_params.f32(0x2224),sin2);const float p2=mulf(body_params.f32(0x21d8),side);float strength=mulf(mulf(body_params.f32(0x2270),timer),p1);strength=mulf(strength,p2);
                if(!std::isnan(strength)&&strength<0.0f)strength=0.0f;else if(!std::isnan(strength)&&strength>1.0f)strength=1.0f;
                if(!std::isnan(p2)&&p2>1.0f&&event.i8(0xd36)>0){float ds=subf(strength,event.f32(0xd58));constexpr float neg=-0.009999999776482582f;if(!std::isnan(ds)&&ds<neg)ds=neg;else if(!std::isnan(ds)&&ds>1.0f)ds=1.0f;strength=addf(event.f32(0xd58),ds);}
                event.putf(0xd58,strength);r034_apply_velocity_correction(work,heading,predicted,speed,strength);
            }
        }
    }
    const std::int16_t rd=r033_s16_sub(event.i16(0xd4e),event.i16(0xd4c));std::int32_t target=-5*std::int32_t(rd);if(target<-16383)target=-16383;else if(target>16383)target=16383;
    const std::int32_t current=event.i16(0xd4a);std::int32_t q=(target-current)/10;if(q<-256)q=-256;else if(q>256)q=256;event.put16(0xd4a,static_cast<std::uint16_t>(static_cast<std::int16_t>(current+q)));
}


void calc_pl_body_2nd_4a7ec0(Bytes event,Bytes work,Bytes body_params,
                             const std::array<Bytes,4>& tires,Bytes wheel_block,
                             PcCalcPlBody2Context& context){
    event.check(0,0xdc8);work.check(0,0x634);body_params.check(0,0x2274);wheel_block.check(0,4u*0xf4u);

    // PC 4A7EC7..4A7F50: |sin(steer)| and the half-step smoothing gate.
    const float steer_angle=x87_i32_angle(event.i16(0x4e));
    const float abs_sin=r034_x87_abs_spill(x87_sin_spilled(steer_angle));
    const float target=event.u32(0x1f4)!=0u?abs_sin:0.0f;
    const float old=event.f32(0xd54);
    float blend=addf(mulf(subf(target,old),0.5f),old);
    blend=clamp_pc_0_1(blend);
    event.putf(0xd54,blend);
    r033_write3(work,0xc0,r033_scale(r033_read3(work,0xc0),blend));
    r033_write3(work,0xcc,r033_scale(r033_read3(work,0xcc),blend));

    // PC 4A7F71..4A7FC1: force limiter is disabled during recovery.
    if(event.i8(0xd36)<=0){
        float threshold=addf(body_params.f32(0xf24),body_params.f32(0xed8));
        threshold=mulf(threshold,0.5f);
        threshold=mulf(threshold,event.f32(0xdc4));
        threshold=addf(threshold,2.0f);
        threshold=mulf(threshold,9.806650161743164f);
        calc_pl_body_force_limit_4a3c60(work,threshold);
    }

    ass_compulsive_move_5184b0(event,work,body_params,context.compulsive);

    // PC uses raw dword moves here: preserve NaN payloads/sign bits exactly.
    for(unsigned k=0;k<3;++k)work.put32(0x74u+k*4u,work.u32(0x22cu+k*4u));
    for(unsigned k=0;k<3;++k)work.put32(0x68u+k*4u,work.u32(0x238u+k*4u));

    make_force_work_tire_4a0000(event,work,body_params,tires,context.road.matrices);

    bool suppress=false;
    constexpr float kMoveEps=9.9999997473787516e-05f;
    const float gate=event.f32(0x2c8);
    if(!std::isnan(gate)&&gate>kMoveEps){
        const std::uint32_t state=(event.u32(0x2f0)>>2)&0x1fu;
        if(state!=3u&&state!=6u){
            const CourseProbe zero{0.0f,0.0f,0.0f};
            r033_write3(work,0xc0,zero);r033_write3(work,0xcc,zero);
            suppress=true;
        }
    }
    if(!suppress){
        constexpr float kSpeedGate=0.6921373009681702f;
        const float speed_state=event.f32(0x1c4);
        if(!std::isnan(speed_state)&&kSpeedGate>speed_state){
            float scale=mulf(subf(speed_state,0.23071244359016418f),2.1671998500823975f);
            scale=clamp_pc_0_1(scale);
            r033_write3(work,0xc0,r033_scale(r033_read3(work,0xc0),scale));
            r033_write3(work,0xcc,r033_scale(r033_read3(work,0xcc),scale));
        }
    }

    assist_wanderer_4a5470(event,work,context.road.matrices);
    reb_pl_body_4a62e0(event,work,context.road,context.stages);
    wrec_pl_body_4a0c70(event,work,body_params,wheel_block,context.wrecker);
    action_force2(work,event.f32(0xdbc),context.road.matrices);
}


void check_night_and_tunnel_4a25f0(Bytes event,const PcNightTunnelInputs& inputs){
    event.check(4,4);
    auto flags=event.u32(4);
    flags=inputs.night?(flags|0x100u):(flags&~0x100u);
    flags=inputs.tunnel?(flags|0x200u):(flags&~0x200u);
    flags=(inputs.course_flags&0x08u)?(flags|0x400u):(flags&~0x400u);
    event.put32(4,flags);
}
void calc_disp_steering_angle_46eb40(Bytes event){
    event.check(0x280,1);
    if(event.u32(0x27c)!=0u){event.put8(0x280,0xffu);return;}
    const float input=event.f32(0x268);
    // x86 CVTTSS2SI returns INT_MIN for NaN/out-of-range.  Only AL is
    // consumed by the original, so preserve that architectural result.
    const std::int32_t raw=sse_cvttss2si(input);
    const auto signed_low=static_cast<std::int32_t>(static_cast<std::int8_t>(raw&0xff));
    std::int32_t q=signed_low/4; // same truncation toward zero as cdq/and/add/sar.
    if(q>5)q=5;
    event.put8(0x280,static_cast<std::uint8_t>(static_cast<std::int8_t>(q)));
}
namespace {
OR2_R033_NOINLINE std::int16_t r037_atan_scaled_i16(float y,float x){
    constexpr float scale=10430.3779296875f;
    // FLD y; FLD x; FPATAN; FMUL scale; FSTP m32; CVTTSS2SI.
    const float spilled=x87_float(x87_atan2(X87(y),X87(x))*scale);
    return static_cast<std::int16_t>(sse_cvttss2si(spilled));
}
// mxInnerProduct 0x40EFD0: (az*bz + ay*by) + ax*bx.
inline X87 x87_inner3(CourseProbe a,CourseProbe b){return (X87(a.z)*b.z+X87(a.y)*b.y)+X87(a.x)*b.x;}
OR2_R033_NOINLINE float r037_dot_spill(CourseProbe a,CourseProbe b){return x87_float(x87_inner3(a,b));}
OR2_R033_NOINLINE X87 r037_length_ext(CourseProbe v){return x87_length3(v.x,v.y,v.z);}
OR2_R033_NOINLINE std::int32_t r037_positive_ftol(X87 v){
    // CopyCarWork feeds this helper a non-negative vector length * 216.72.
    // MSVC's 0x582194 helper truncates the x87 value toward zero.
    return x87_ftol2_low32(v);
}
}

void copy_car_work_4a1140(Bytes event,Bytes work,Bytes body_params,
                           const std::array<Bytes,4>& tires,PcMatrixStack& matrices){
    event.check(0,0x2b4);work.check(0,0x634);body_params.check(0,0x243c);
    for(auto t:tires)t.check(0,0x2c);
    constexpr float angle_unit=9.58738019107841e-05f;
    constexpr float velocity_scale=0.016611294820904732f;
    constexpr float body_scale=216.720001220703125f;
    constexpr float speed_history_scale=0.0010000000474974513f;
    constexpr float low_speed=0.018456995487213135f;
    constexpr float speed_limit=300.0f;
    constexpr float correction=1.0101009607315063f;

    const bool special=(event.u8(4)&8u)!=0u;
    pc_matrix_load(matrices,work.sub(0x10,64));
    if(special){
        pc_matrix_unit_rotation(matrices);
        const auto angle=[&](std::int16_t a){
            return x87_float(X87(std::int32_t(a))*angle_unit);  // FILD; FMUL; FSTP
        };
        pc_matrix_rotate_y(matrices,angle(event.i16(0x2e)));
        pc_matrix_rotate_x(matrices,angle(event.i16(0x2c)));
        pc_matrix_rotate_z(matrices,angle(event.i16(0x30)));
    }
    pc_matrix_store_rotation(matrices,event.sub(0x70,44));
    std::array<std::uint8_t,64> matrix_copy{};Bytes matrix(matrix_copy.data(),matrix_copy.size());
    pc_matrix_get(matrices,matrix);
    if(!special){
        const CourseProbe offset{work.f32(0x220),work.f32(0x224),work.f32(0x228)};
        const CourseProbe transformed=pc_matrix_vector(matrices,offset);
        event.putf(0x14,subf(matrix.f32(0x30),transformed.x));
        event.putf(0x18,subf(matrix.f32(0x34),transformed.y));
        event.putf(0x1c,subf(matrix.f32(0x38),transformed.z));
        event.put16(0x2c,static_cast<std::uint16_t>(r037_atan_scaled_i16(subf(0.0f,matrix.f32(0x24)),1.0f)));
        event.put16(0x2e,static_cast<std::uint16_t>(r037_atan_scaled_i16(matrix.f32(0x20),matrix.f32(0x28))));
        event.put16(0x30,static_cast<std::uint16_t>(r037_atan_scaled_i16(matrix.f32(0x04),matrix.f32(0x14))));
    }

    CourseProbe raw{work.f32(0x5c),work.f32(0x60),work.f32(0x64)};
    if(special){raw={0.0f,0.0f,0.0f};work.put32(0x5c,0);work.put32(0x60,0);work.put32(0x64,0);}
    CourseProbe velocity{mulf(raw.x,velocity_scale),mulf(raw.y,velocity_scale),mulf(raw.z,velocity_scale)};
    event.putf(0x20,velocity.x);event.putf(0x24,velocity.y);event.putf(0x28,velocity.z);

    const CourseProbe normal{work.f32(0x628),work.f32(0x62c),work.f32(0x630)};
    const float dot=r037_dot_spill(normal,velocity);
    CourseProbe planar{subf(velocity.x,mulf(normal.x,dot)),subf(velocity.y,mulf(normal.y,dot)),subf(velocity.z,mulf(normal.z,dot))};
    const X87 speed_ext=r037_length_ext(planar);
    const float speed=x87_float(speed_ext);
    const float old_speed=event.f32(0x1c4);
    event.putf(0x1c8,x87_float(speed_ext-old_speed));
    event.put32(0x178,event.u32(0x1c4));
    event.putf(0x1c4,speed);
    event.puti(0x1f4,r037_positive_ftol(speed_ext*body_scale));

    float scaled=mulf(mulf(body_params.f32(0x2438),speed),body_scale);
    event.putf(0x1f8,scaled);
    const float gate=body_params.f32(0x20a8);
    if(!(0.0f<gate) && speed_limit>scaled){
        scaled=mulf(scaled,correction);event.putf(0x1f8,scaled);
        if(0.0f>scaled)scaled=0.0f;else if(scaled>speed_limit)scaled=speed_limit;
        event.putf(0x1f8,scaled);
    }
    const float history=mulf(speed,speed_history_scale);
    event.putf(0x2ac,addf(event.f32(0x2ac),history));event.putf(0x2b0,addf(event.f32(0x2b0),history));

    if(special)event.put16(0x160,static_cast<std::uint16_t>(event.i16(0x2e)));
    else{
        CourseProbe direction=velocity;
        if(!(low_speed<speed))direction={subf(0.0f,matrix.f32(0x20)),subf(0.0f,matrix.f32(0x24)),subf(0.0f,matrix.f32(0x28))};
        direction=d3dx_normalized_vec(direction);
        event.put16(0x160,static_cast<std::uint16_t>(r037_atan_scaled_i16(subf(0.0f,direction.x),subf(0.0f,direction.z))));
    }

    for(unsigned i=0;i<4;++i){
        float y=subf(tires[i].f32(0x08),body_params.f32(i<2?0x4c0:0x50c));
        const float cap=tires[i].f32(0x28);if(y>cap)y=cap;
        event.putf(0x134+i*0x0c,addf(work.f32(0x224),y));
    }
    event.put32(0x244,tires[0].u32(0x14)|tires[1].u32(0x14)|tires[2].u32(0x14)|tires[3].u32(0x14));
    constexpr std::array<std::size_t,8> wo{0x268,0x35c,0x450,0x544,0x26c,0x360,0x454,0x548};
    constexpr std::array<std::size_t,8> eo{0x234,0x238,0x23c,0x240,0x24c,0x250,0x254,0x258};
    for(unsigned i=0;i<8;++i)event.put32(eo[i],work.u32(wo[i]));
}

namespace {
OR2_R033_NOINLINE CourseProbe r038_project_keep_dot_ext(CourseProbe v,CourseProbe n){
    // SetCarCamera's first projection keeps mxInnerProduct's x87 result live
    // while all three n*dot subtractions are performed, spilling only XYZ.
    const X87 dot=x87_inner3(v,n);
    return {x87_float(X87(v.x)-X87(n.x)*dot),
            x87_float(X87(v.y)-X87(n.y)*dot),
            x87_float(X87(v.z)-X87(n.z)*dot)};
}
OR2_R033_NOINLINE CourseProbe r038_project_spilled_dot(CourseProbe v,CourseProbe n){
    const float dot=r037_dot_spill(v,n);
    return {subf(v.x,mulf(n.x,dot)),subf(v.y,mulf(n.y,dot)),subf(v.z,mulf(n.z,dot))};
}
OR2_R033_NOINLINE float r038_blend_from_length(CourseProbe v){
    constexpr float kBlendRange=0.014999999664723873f;
    float out=x87_float(X87(kBlendRange)-r037_length_ext(v));
    // Original stores binary32, then compares 0 against it with FCOMIP.
    // Unordered is kept; only a strictly negative finite value becomes zero.
    if(!std::isnan(out)&&out<0.0f)out=0.0f;
    return out;
}
OR2_R033_NOINLINE float r038_i32_angle_radians(std::int32_t v){
    constexpr float kAngleUnit=9.58738019107841e-05f;
    return x87_float(X87(v)*kAngleUnit);
}
OR2_R033_NOINLINE float r038_cos_spilled(float v){return x87_float(x87_cos(X87(v)));}
}

void set_car_camera_4a1680(Bytes event,Bytes work,PcMatrixStack& matrices){
    event.check(0,0x202);work.check(0,0x634);
    constexpr float kDiagThreshold=0.7070000171661377f;

    pc_matrix_identity(matrices);
    const CourseProbe normal{work.f32(0x628),work.f32(0x62c),work.f32(0x630)};
    const CourseProbe original{event.f32(0x90),event.f32(0x94),event.f32(0x98)};

    CourseProbe heading=r038_project_keep_dot_ext(original,normal);
    heading=d3dx_normalized_vec(heading);

    CourseProbe velocity{subf(0.0f,event.f32(0x20)),
                         subf(0.0f,event.f32(0x24)),
                         subf(0.0f,event.f32(0x28))};
    velocity=r038_project_spilled_dot(velocity,normal);
    const float blend=r038_blend_from_length(velocity);

    CourseProbe fallback=r038_project_spilled_dot(original,normal);
    fallback=d3dx_normalized_vec(fallback);
    velocity={addf(velocity.x,mulf(fallback.x,blend)),
              addf(velocity.y,mulf(fallback.y,blend)),
              addf(velocity.z,mulf(fallback.z,blend))};
    velocity=d3dx_normalized_vec(velocity);

    const CourseProbe side=r034_cross(normal,velocity);
    std::array<float,16> frame{
        side.x,side.y,side.z,0.0f,
        normal.x,normal.y,normal.z,0.0f,
        velocity.x,velocity.y,velocity.z,0.0f,
        0.0f,0.0f,0.0f,1.0f};
    pc_matrix_load(matrices,Bytes(frame.data(),64));

    const CourseProbe local=pc_matrix_inverse_vector(matrices,original);
    const std::int16_t raw_half_source=r037_atan_scaled_i16(local.x,local.z);
    const std::int32_t half=static_cast<std::int32_t>(raw_half_source)/2;
    pc_matrix_rotate_y(matrices,r038_i32_angle_radians(half));

    std::array<float,16> out{};pc_matrix_get(matrices,Bytes(out.data(),64));
    Bytes m(out.data(),64);
    event.put16(0x1fe,static_cast<std::uint16_t>(r037_atan_scaled_i16(m.f32(0x20),m.f32(0x28))));
    const std::int16_t pitch=r037_atan_scaled_i16(m.f32(0x04),m.f32(0x14));
    event.put16(0x200,static_cast<std::uint16_t>(pitch));

    const float radians=r038_i32_angle_radians(static_cast<std::int32_t>(pitch));
    const float sn=x87_sin_spilled(radians);
    const float cs=r038_cos_spilled(radians);
    float ratio{};
    if(std::fabs(sn)>kDiagThreshold)ratio=divf(m.f32(0x04),sn);
    else ratio=divf(m.f32(0x14),cs);
    event.put16(0x1fc,static_cast<std::uint16_t>(r037_atan_scaled_i16(subf(0.0f,m.f32(0x24)),ratio)));
    event.put16(0x162,static_cast<std::uint16_t>(static_cast<std::int16_t>(event.i16(0x160)-event.i16(0x2e))));
}

void check_driving_skill_4a4830(Bytes event,const PcDrivingSkillInputs& inputs){
    event.check(0xdb4,0x54);
    alignas(4) static std::int32_t promotion[8]{}; OR2_EXE_COPY(promotion,0x680BD4u,0x20u);
    constexpr float kInvSeven=0.1428571492433548f;
    constexpr float kHalf=0.5f;

    auto timer=event.i32(0xdec);
    if(timer>0){
        --timer;event.puti(0xdec,timer);
        if(timer==0)event.puti(0xdf0,event.i32(0xdf0)+1);
    }

    const std::int32_t divisor=inputs.stage_level+1;
    // The original game service returns a non-negative stage level. Preserve
    // that contract explicitly rather than manufacturing an x86 IDIV fault.
    if(divisor==0)throw std::domain_error("CheckDrivingSkill stage level -1");
    const std::int32_t quotient=event.i32(0xdf0)/divisor;
    std::int32_t level=event.i32(0xdf4);
    if(level<0 || level>7)throw std::out_of_range("CheckDrivingSkill level outside 0..7");
    if(promotion[level]<=quotient){
        ++level;
        if(level<0)level=0;
        if(level>7)level=7;
        event.puti(0xdf4,level);
    }

    const float level_ratio=mulf(i32f(level),kInvSeven);
    event.puti(0xdf8,0);
    if(inputs.entry_nodes<=1u){
        event.putf(0xdb4,0.0f);
        return;
    }

    const float progress=event.f32(0xdb4);
    // COMISS/JB treats unordered as a failed threshold in this body.
    if(std::isnan(progress) || progress<kHalf)return;
    const float combined=addf(mulf(event.f32(0xe04),kInvSeven),level_ratio);
    if(std::isnan(combined) || combined<kHalf)return;
    event.puti(0xdf8,1);
}

namespace {
constexpr float kChickenDirScale=0.00017755682347342372f;
constexpr float kChickenDeltaMin=-0.10000000149011612f;
constexpr float kChickenDeltaMax=0.009999999776482582f;
constexpr float kChickenGate=0.8999999761581421f;
constexpr float kChickenRatioBase=0.10000000149011612f;
constexpr float kChickenRatioScale=4.999999523162842f;
constexpr float kChickenWarmupScale=0.0010000000474974513f;
inline std::int16_t r039_s16_from_u16(std::uint16_t v){std::int16_t out{};std::memcpy(&out,&v,2);return out;}
inline std::int16_t r039_low16_sub(std::int32_t a,std::int32_t b){
    return r039_s16_from_u16(static_cast<std::uint16_t>(static_cast<std::uint32_t>(a)-static_cast<std::uint32_t>(b)));
}
inline std::int16_t r039_abs_s16_wrap(std::int16_t v){
    if(v>=0)return v;
    return r039_s16_from_u16(static_cast<std::uint16_t>(0u-static_cast<std::uint16_t>(v)));
}
inline std::int16_t r039_add_s16_wrap(std::int16_t a,std::int16_t b){
    return r039_s16_from_u16(static_cast<std::uint16_t>(static_cast<std::uint16_t>(a)+static_cast<std::uint16_t>(b)));
}
}

void check_chicken_driver_4a4900(Bytes event,const PcChickenDriverInputs& inputs){
    event.check(0x5c,0xdac);
    float target=1.0f;
    if(inputs.query_status[0]>0 && inputs.query_status[1]>0 && inputs.query_status[2]>0){
        const std::int16_t d1=r039_low16_sub(inputs.offset_direction[1],inputs.offset_direction[0]);
        const std::int16_t d2=r039_low16_sub(inputs.offset_direction[2],inputs.offset_direction[0]);
        std::int16_t metric{};
        if(event.u32(0x5c)==0u){
            metric=r039_add_s16_wrap(r039_abs_s16_wrap(d1),r039_abs_s16_wrap(d2));
        }else{
            metric=r039_abs_s16_wrap(r039_s16_from_u16(static_cast<std::uint16_t>(static_cast<std::uint16_t>(d1)-static_cast<std::uint16_t>(d2))));
        }
        if(metric<0x0800)metric=0x0800;
        else if(metric>0x1e00)metric=0x1e00;
        target=subf(1.0f,mulf(i32f(static_cast<std::int32_t>(metric)-0x0800),kChickenDirScale));
    }

    float delta=subf(target,event.f32(0xe0c));
    if(!std::isnan(delta) && delta<kChickenDeltaMin)delta=kChickenDeltaMin;
    if(!std::isnan(delta) && delta>kChickenDeltaMax)delta=kChickenDeltaMax;
    const float current=addf(delta,event.f32(0xe0c));
    event.putf(0xe0c,current);

    if(event.u8(0x283)==0u &&
       !( !std::isnan(event.f32(0x2c8)) && event.f32(0x2c8)>0.0f ) &&
       !( !std::isnan(event.f32(0x2f8)) && event.f32(0x2f8)>0.0f ) &&
       event.i8(0xd36)<=0 && static_cast<std::uint16_t>(event.i16(0x260))>static_cast<std::uint16_t>(event.i16(0x262)) &&
       !( !std::isnan(current) && current>kChickenGate )){
        if(inputs.volume1<0xc0)event.puti(0xdfc,event.i32(0xdfc)+1);
        event.puti(0xe00,event.i32(0xe00)+1);
    }

    const std::int32_t total=event.i32(0xe00);
    float score=0.0f;
    if(total>=10)score=divf(i32f(event.i32(0xdfc)),i32f(total));
    score=mulf(subf(score,kChickenRatioBase),kChickenRatioScale);
    if(!std::isnan(score)&&score<0.0f)score=0.0f;
    if(!std::isnan(score)&&score>1.0f)score=1.0f;
    event.putf(0xe04,score);
    if(total<1000){
        score=mulf(mulf(i32f(total),score),kChickenWarmupScale);
        event.putf(0xe04,score);
    }
}

namespace {
constexpr float kAssistChickenSpeedMin=9.999999747378752e-05f;
constexpr float kAssistChickenCapBase=2.0f;
constexpr float kAssistChickenDeltaMin=-0.019999999552965164f;
constexpr float kAssistChickenDeltaMax=0.003000000026077032f;
}

void assist_chicken_driver_4a4ba0(Bytes event,const PcAssistChickenInputs& inputs){
    event.check(0x06,0xe0c);
    if(inputs.entry_nodes<=1u){
        event.putf(0xe08,0.0f);
        return;
    }

    float assist=0.0f;
    const float speed=event.f32(0x1c4);
    const bool active=
        !(!std::isnan(speed)&&speed<=kAssistChickenSpeedMin) &&
        event.u8(0x282)==0u && event.u8(0x283)==0u &&
        !(!std::isnan(event.f32(0x2c8))&&event.f32(0x2c8)>0.0f) &&
        !(!std::isnan(event.f32(0x2f8))&&event.f32(0x2f8)>0.0f) &&
        (event.u8(0x06)&1u)==0u;
    if(active){
        const float handicap=event.f32(0xdbc);
        float gap=subf(mulf(event.f32(0xe64),handicap),speed);
        gap=divf(gap,speed);
        gap=addf(gap,1.0f);
        gap=subf(gap,handicap);
        if(!std::isnan(gap)&&gap<0.0f)gap=0.0f;
        else{
            const float cap=subf(kAssistChickenCapBase,handicap);
            if(!std::isnan(gap)&&!std::isnan(cap)&&gap>cap)gap=cap;
        }

        float target=mulf(mulf(event.f32(0xdb4),gap),event.f32(0xe04));
        target=mulf(mulf(i32f(inputs.volume1),target),kInv255);
        const float volume2=mulf(i32f(inputs.volume2),kInv255);
        target=mulf(subf(1.0f,volume2),target);

        float delta=subf(target,event.f32(0xe08));
        if(!std::isnan(delta)&&delta<kAssistChickenDeltaMin)delta=kAssistChickenDeltaMin;
        if(!std::isnan(delta)&&delta>kAssistChickenDeltaMax)delta=kAssistChickenDeltaMax;
        assist=addf(event.f32(0xe08),delta);
    }
    event.putf(0xe08,assist);
    event.putf(0xdbc,addf(assist,event.f32(0xdbc)));
}

namespace {
constexpr float kVibrateInputScale=1000.0f;
constexpr float kVibrateAverageScale=0.03333333507180214f;
constexpr std::size_t kVibrateHistoryStride=0x78u;
}

void calc_vibrate_matrix_4a2d70(Bytes event,Bytes histories){
    event.check(0,0x1d0);
    if((event.u8(0x04)&1u)==0u)return;
    const std::uint32_t car_id=event.u32(0);
    if(car_id<8u)throw std::out_of_range("CalcVibrateMatrix car id below history base");
    const std::size_t slot=std::size_t(car_id-8u)*kVibrateHistoryStride;
    Bytes h=histories.sub(slot,kVibrateHistoryStride);

    float total=0.0f;
    // The PC walks the 29 retained samples from newest/highest slot to oldest,
    // shifting each one upward as it accumulates. Preserve that ADDSS order.
    for(int i=28;i>=0;--i){
        const std::size_t src=std::size_t(i)*4u;
        const float v=h.f32(src);
        h.put32(src+4u,h.u32(src));
        total=addf(v,total);
    }
    const float sample=mulf(event.f32(0x1c8),kVibrateInputScale);
    h.putf(0,sample);
    total=addf(sample,total);
    event.putf(0x1cc,mulf(total,kVibrateAverageScale));
}

namespace {
constexpr float kReverseTimeScale=0.009999999776482582f;
constexpr float kReversePhaseMax=4.0f;
constexpr float kReversePhaseWrap=255.0f;
constexpr float kReversePhaseUnits=4096.0f;
constexpr float kReverseRandScale=3.0517578125e-05f;
constexpr float kReverseRandBias=0.5f;
constexpr float kReverseRandGain=0.6666666865348816f;
constexpr float kReverseHalf=0.5f;
constexpr float kReversePitchLow=-1.5707963705062866f;
constexpr float kReversePitchHigh=1.5707963705062866f;
constexpr float kReverseAgeScale=0.0020833334419876337f;
constexpr float kReversePitchShape=0.40528470277786255f;

// PC: FILD signed dword, FADD 2^32 for the negative half (rounded like any
// FADD), then FMUL 0.01 / FSUB 1.0 with a single final binary32 spill.
X87 r041_u32_load(std::uint32_t value){
    const std::int32_t signed_value=static_cast<std::int32_t>(value);
    X87 v=X87(signed_value);
    if(signed_value<0)v=v+4294967296.0f;
    return v;
}
float r041_u32_age_phase(std::uint32_t value){
    // FMUL 0.01; FSUB 1.0; single final binary32 spill.
    return x87_float(r041_u32_load(value)*kReverseTimeScale-1.0f);
}
float r041_u32_age_scaled(std::uint32_t value){
    return x87_float(r041_u32_load(value)*kReverseAgeScale);
}
float r041_reverse_bob(std::uint32_t random_value,float sine,float age,float base){
    const std::int32_t r=static_cast<std::int32_t>(random_value&0x7fffu);
    // FILD r; FMUL rs; FADD rb; FMUL rg; FMUL sine; FLD age; FMUL half;
    // FMULP; FMUL base; FADD base; FSTP.
    const X87 q=((X87(r)*kReverseRandScale+kReverseRandBias)*kReverseRandGain)*sine;
    return x87_float((q*(X87(age)*kReverseHalf))*base+base);
}
}

void check_reverse_car_4a2910(Bytes event,const PcReverseCarInputs& in,PcMatrixStack& matrices){
    event.check(0,0x330);
    float translate_y=0.0f,pitch=0.0f,roll=0.0f;
    const std::uint32_t age_u=event.u32(0x1f4);
    if(age_u>100u){
        float age=r041_u32_age_phase(age_u);
        if(age>kReversePhaseMax)age=kReversePhaseMax;
        float phase=addf(event.f32(0x310),age);
        event.putf(0x310,phase);
        if(!std::isnan(phase)&&phase>kReversePhaseWrap){phase=0.0f;event.putf(0x310,phase);}
        const float scaled=mulf(phase,kReversePhaseUnits);
        std::int32_t angle_i{};
        if(!std::isfinite(scaled)||scaled<-2147483648.0f||scaled>=2147483648.0f)angle_i=std::numeric_limits<std::int32_t>::min();
        else angle_i=static_cast<std::int32_t>(scaled);
        const float sine=x87_sin_spilled(x87_i32_angle(angle_i));
        translate_y=r041_reverse_bob(in.random_value,sine,age,event.f32(0x30c));
    }

    const std::uint32_t active=event.u32(0x04)&1u;
    if(active){
        const float raw=event.f32(0x1cc);
        float x{};
        // Mirrors COMISS/Jcc ordering: unordered takes the upper clamp.
        if(!std::isnan(raw)&&raw<kReversePitchLow)x=kReversePitchLow;
        else if(std::isnan(raw)||raw>=kReversePitchHigh)x=kReversePitchHigh;
        else x=raw;
        const float t=r041_u32_age_scaled(age_u);
        float v{};
        if(!std::isnan(x)&&x<0.0f)v=mulf(in.param20,t);
        else v=mulf(subf(1.0f,t),in.param24);
        v=mulf(v,x);v=mulf(v,x);v=mulf(v,kReversePitchShape);
        if(!std::isnan(in.param20)&&!std::isnan(v)&&in.param20>v)v=in.param20;
        else if(!std::isnan(v)&&!std::isnan(in.param24)&&v>in.param24)v=in.param24;
        pitch=v;
    }

    if(in.game_mode==3 || active!=1u){
        const float a=x87_i32_angle(event.i16(0xd34));
        float clamped{};
        if(!std::isnan(a)&&!std::isnan(in.param2c)&&a>in.param2c)clamped=in.param2c;
        else {
            const float neg=subf(0.0f,in.param2c);
            if(!std::isnan(neg)&&!std::isnan(a)&&neg>a)clamped=neg;
            else clamped=a;
        }
        roll=mulf(divf(in.param28,in.param2c),clamped);
    }

    pc_matrix_push_unit(matrices);
    pc_matrix_translate_vector(matrices,{0.0f,translate_y,0.0f});
    pc_matrix_rotate_y(matrices,0.0f);
    pc_matrix_rotate_x(matrices,pitch);
    pc_matrix_rotate_z(matrices,roll);
    pc_matrix_get(matrices,event.sub(0xf0,64));
    pc_matrix_pop(matrices);
}

namespace {
constexpr float kHandicapIndexScale=0.06666667014360428f;
constexpr float kHandicapDb1A=0.013333333656191826f;
constexpr float kHandicapDb1B=0.01133333332836628f;
constexpr float kHandicapDb8Scale=2.499999936844688e-05f;
constexpr float kHandicapTimerScale=0.0008333333535119891f;
constexpr float kHandicapSpeedDeltaScale=-123.18307495117188f;
constexpr float kHandicapSpeedCap=0.1384274661540985f;
constexpr float kHandicapSpeedScale=7.223999977111816f;
alignas(4) static float kHandicapNodeScale[9]{}; OR2_EXE_COPY(kHandicapNodeScale,0x5A9098u,0x24u);
inline float r039_clamp_0_1_unordered(float v){
    if(!std::isnan(v)&&v<0.0f)v=0.0f;
    if(!std::isnan(v)&&v>1.0f)v=1.0f;
    return v;
}
inline std::uint16_t r039_step_counter_u16(std::uint16_t old,bool up){
    const std::uint16_t bits=static_cast<std::uint16_t>(old+(up?1u:0xffffu));
    const std::int16_t signed_bits=r039_s16_from_u16(bits);
    if(signed_bits<0)return 0;
    if(signed_bits>0x4b0)return 0x4b0;
    return static_cast<std::uint16_t>(signed_bits);
}
inline float r039_course_progress(std::int16_t position,std::uint16_t max_len,std::int32_t tail){
    const std::int32_t threshold=static_cast<std::int32_t>(max_len)-tail;
    const std::int32_t numerator=static_cast<std::int32_t>(position)-threshold;
    const std::int32_t denominator=static_cast<std::int32_t>(max_len)-threshold;
    return r039_clamp_0_1_unordered(divf(i32f(numerator),i32f(denominator)));
}
}

namespace {
X87 r041_vector_length_ext(CourseProbe v){return x87_length3(v.x,v.y,v.z);}
}
namespace {
inline std::int16_t r042_s16_bits(std::uint16_t u){std::int16_t v{};std::memcpy(&v,&u,2);return v;}
inline std::int16_t r042_s16_sub(std::int16_t a,std::int16_t b){
    return r042_s16_bits(static_cast<std::uint16_t>(static_cast<std::uint16_t>(a)-static_cast<std::uint16_t>(b)));
}
inline std::int32_t r042_abs_s16(std::int16_t v){const auto q=static_cast<std::int32_t>(v);return q<0?-q:q;}
}

void calc_light_rate_4a3d40(Bytes event,PcMatrixStack& matrices){
    event.check(0,0xd54);
    if(event.u8(0x283)>0u)event.put16(0xd50,0u);

    const std::uint16_t count=static_cast<std::uint16_t>(event.i16(0xd50));
    const std::uint16_t raw_threshold=static_cast<std::uint16_t>((std::uint32_t(0x1c0u)-count)<<5u);
    std::int32_t threshold=static_cast<std::int32_t>(r042_s16_bits(raw_threshold));
    if(threshold<0x2800)threshold=0x2800;

    const std::int16_t reference=event.i16(0xd4c);
    const std::int16_t light_delta=r042_s16_sub(event.i16(0x160),reference);
    const std::int16_t body_delta=r042_s16_sub(event.i16(0x2e),reference);
    if(r042_abs_s16(light_delta)<=threshold){event.put16(0xd50,0u);return;}
    event.put16(0xd50,static_cast<std::uint16_t>(count+1u));

    // PC mxUnitVector returns an extended x87 length, immediately spilled by
    // the caller. The normalized vector itself is written component-by-component.
    CourseProbe v{event.f32(0x20),event.f32(0x24),event.f32(0x28)};
    const float original_length=x87_float(unit_vector_ext(v));
    const float heading_angle=x87_i32_angle(reference);
    CourseProbe heading{-r032_x87_sin(heading_angle),0.0f,-r032_x87_cos(heading_angle)};
    v=projective_vec_x87(v,heading);
    v=scale_vec_x87(v,original_length);

    // IDIV-by-four lowering used by the PC compiler: C++ signed / has the same
    // truncation-toward-zero rule for this bounded range.
    const std::int32_t quarter=(0x4000-threshold)/4;
    const std::int32_t limit_angle=0x4000-quarter;
    const float a=x87_i32_angle(limit_angle);
    heading=scale_vec_x87(heading,r032_x87_cos(a));
    v=scale_vec_x87(v,r032_x87_sin(a));
    v=add_vec_x87(v,heading);
    event.putf(0x20,v.x);event.putf(0x24,v.y);event.putf(0x28,v.z);

    const std::int32_t body_abs=r042_abs_s16(body_delta);
    if(body_abs<=threshold)return;
    std::int32_t clamped=static_cast<std::int32_t>(body_delta);
    if(clamped < -threshold)clamped=-threshold;
    else if(clamped > threshold)clamped=threshold;
    const std::int32_t correction=clamped-static_cast<std::int32_t>(body_delta);

    pc_matrix_push_unit(matrices);
    pc_matrix_rotate_y(matrices,x87_i32_angle(correction));
    pc_matrix_multiply_current(matrices,event.sub(0x70,64));
    pc_matrix_store_rotation(matrices,event.sub(0x70,64));
    pc_matrix_pop(matrices);
}

void calc_ofs_left_lane_4a45f0(Bytes event,const PcOfsLeftLaneInputs& in){
    event.check(0,0x1f8);
    if(in.selector==-1){event.putf(0x58,1.0f);return;}
    const CourseProbe position{event.f32(0x14),event.f32(0x18),event.f32(0x1c)};
    float best=100000.0f;
    unsigned best_index=0;
    for(unsigned i=0;i<4;++i){
        const CourseProbe delta=sub_vec_x87(in.points[i],position);
        const X87 length=r041_vector_length_ext(delta);
        // Original: FST (keep x87 sqrt live), FLD best, FCOMIP best,length.
        // Unordered takes the no-update path just like JBE after FCOMIP.
        if(!std::isnan(length.v) && X87(best)>length){
            best=x87_float(length);
            best_index=i;
        }
    }
    const float target=in.rates[best_index];
    if(event.u32(0x1f4)>200u){event.putf(0x58,target);return;}
    const float old=event.f32(0x58);
    event.putf(0x58,addf(old,mulf(subf(target,old),0.4000000059604645f)));
}

namespace {
inline std::int16_t r042_neg_s16(std::int16_t v){
    return r042_s16_bits(static_cast<std::uint16_t>(0u-static_cast<std::uint16_t>(v)));
}
}
void record_ghost_car_4a4710(Bytes event,Bytes history,const PcRecordGhostInputs& in){
    event.check(0,0x206);history.check(0,0x3c);
    std::int32_t sum=0;
    // Original shift walks high-to-low so every source is consumed before overwrite.
    for(int i=29;i>=1;--i){
        const auto v=history.i16(std::size_t(i-1)*2u);
        history.put16(std::size_t(i)*2u,static_cast<std::uint16_t>(v));
        sum+=static_cast<std::int32_t>(v);
    }

    std::int16_t sample{};
    if(in.steering_override_active!=0){
        sample=r042_neg_s16(in.steering_override);
    }else if(in.game_mode==0x10){
        sample=event.i16(0x202);
    }else{
        sample=r042_neg_s16(event.i16(0x32));
    }

    if(in.game_mode!=0x10 || in.steering_override_active!=0){
        std::int32_t q=static_cast<std::int32_t>(sample);
        if(q<-5461)q=-5461;else if(q>5461)q=5461;
        sample=static_cast<std::int16_t>(q*6);
    }
    history.put16(0,static_cast<std::uint16_t>(sample));
    sum+=static_cast<std::int32_t>(sample);
    event.put16(0x204,static_cast<std::uint16_t>(static_cast<std::int16_t>(sum/30)));
}

void handicap_control_458e40(Bytes event,const PcHandicapInputs& inputs){
    event.check(0x04,0xdc0);
    event.putf(0xdb4,0.0f);
    event.put8(0xdb1,0);
    event.putf(0xdbc,1.0f);
    event.putf(0xdc0,1.0f);
    if(inputs.active_nodes<=1u || event.u8(0x10)!=inputs.local_car_id || !inputs.race_ready)return;
    if(inputs.handicap_table_index>8u)throw std::out_of_range("HandicapControl node table index");

    std::int32_t course_delta=0;
    if((event.u32(0x04)&0x00200000u)==0u){
        course_delta=static_cast<std::int32_t>(inputs.reference_course_position)-
                     static_cast<std::int32_t>(static_cast<std::uint16_t>(event.i16(0x260)));
        const float scaled=mulf(i32f(course_delta),kHandicapNodeScale[inputs.handicap_table_index]);
        std::int32_t index=truncf32(scaled);
        if(index<0)index=0;else if(index>15)index=15;
        event.put8(0xdb1,static_cast<std::uint8_t>(index));
        event.putf(0xdb4,mulf(i32f(index),kHandicapIndexScale));
    }
    event.put16(0xdb8,r039_step_counter_u16(static_cast<std::uint16_t>(event.i16(0xdb8)),course_delta>0));

    float progress=0.0f;
    if(inputs.stage_current==inputs.stage_reference){
        const std::int32_t max=inputs.max_cs_len0;
        const std::int32_t half=max/2;
        progress=r039_clamp_0_1_unordered(divf(i32f(static_cast<std::int32_t>(event.i16(0x64))-half),i32f(max-half)));
    }

    const std::int32_t db1=event.u8(0xdb1);
    const std::int32_t db8=event.i16(0xdb8);
    float hi=addf(mulf(i32f(db1),kHandicapDb1A),1.0f);
    const float db1_term=mulf(i32f(db1),kHandicapDb1B);
    float lo=addf(addf(mulf(i32f(db8),kHandicapDb8Scale),db1_term),1.0f);
    float factor=addf(mulf(subf(hi,lo),progress),lo);

    std::int16_t timer=event.i16(0xdba);
    if(timer>0){
        if(timer>0x4b0)factor=1.0f;
        else factor=subf(factor,mulf(mulf(subf(factor,1.0f),i32f(timer)),kHandicapTimerScale));
        timer=r039_s16_from_u16(static_cast<std::uint16_t>(static_cast<std::uint16_t>(timer)-1u));
        event.put16(0xdba,static_cast<std::uint16_t>(timer));
    }
    event.putf(0xdbc,factor);
    event.putf(0xdc0,subf(factor,mulf(subf(factor,1.0f),progress)));

    float speed_delta=mulf(subf(event.f32(0x1c4),event.f32(0x178)),kHandicapSpeedDeltaScale);
    speed_delta=r039_clamp_0_1_unordered(speed_delta);
    const float retain=subf(1.0f,speed_delta);
    float speed=event.f32(0x1c4);
    if(!std::isnan(speed)&&speed>kHandicapSpeedCap)speed=kHandicapSpeedCap;
    if(!std::isnan(speed)&&speed<0.0f)speed=0.0f;
    speed=mulf(speed,kHandicapSpeedScale);
    factor=addf(mulf(mulf(speed,subf(factor,1.0f)),retain),1.0f);
    event.putf(0xdbc,factor);

    if(inputs.stage_current==inputs.stage_reference)return;
    if(event.u32(0x5c)==0u){
        const float p=r039_course_progress(event.i16(0x64),inputs.max_cs_len0,10);
        event.putf(0xdbc,subf(factor,mulf(subf(factor,1.0f),p)));
    }else{
        const float p=r039_course_progress(event.i16(0x64),inputs.max_cs_len1,25);
        event.putf(0xdbc,addf(mulf(subf(factor,1.0f),p),1.0f));
    }
}

}

// r043 post-ghost CommonPlCar helpers.
namespace outrun::driving {
bool session_mode4_4962a0(const PcSessionModeInputs& in){
    return in.manager_active && in.state_present && in.state_code==4u;
}
bool session_mode6_4962d0(const PcSessionModeInputs& in){
    return in.manager_active && in.state_present && in.state_code==6u;
}

namespace {
inline float r043_sub(float a,float b){volatile float q=a-b;return q;}
inline float r043_mul(float a,float b){volatile float q=a*b;return q;}
inline float r043_add(float a,float b){volatile float q=a+b;return q;}
// PC 0x46F0A3..0x46F0D1: FLD dx_cb; FMUL dz_ab; FLD dz_cb; FMUL dx_ab;
// FSUBP st(1),st (st1-st0); FSTP m32.
float r043_det_spill(float dx_cb,float dz_ab,float dz_cb,float dx_ab){
    return x87_float(X87(dx_cb)*dz_ab-X87(dz_cb)*dx_ab);
}
float r043_sqrt_spill(float v){return x87_float(x87_sqrt(X87(v)));}   // 0x449380 + FSTP
// sqrt(len_ab_squared) (0x449380, live) * len_cb; FDIVR det; FSTP.
float r043_ratio(float det,float len_cb,float len_ab_squared){
    return x87_float(X87(det)/(x87_sqrt(X87(len_ab_squared))*len_cb));
}
// sqrt(len_ca_squared); FDIV ratio; FMUL 0.5; FSTP.
float r043_radius_from_ratio(float len_ca_squared,float ratio){
    constexpr float half=0.5f;
    return x87_float((x87_sqrt(X87(len_ca_squared))/ratio)*half);
}
float r043_othcar_ratio(CourseProbe a,CourseProbe b,CourseProbe c,float& len_ca_squared){
    const float dx_ca=r043_sub(c.x,a.x);
    const float dz_cb=r043_sub(c.z,b.z);
    const float dz_ca=r043_sub(c.z,a.z);
    const float dx_ab=r043_sub(a.x,b.x);
    const float dz_ab=r043_sub(a.z,b.z);
    const float dx_cb=r043_sub(c.x,b.x);
    const float det=r043_det_spill(dx_cb,dz_ab,dz_cb,dx_ab);
    const float len_cb_squared=r043_add(r043_mul(dz_cb,dz_cb),r043_mul(dx_cb,dx_cb));
    const float len_cb=r043_sqrt_spill(len_cb_squared);
    const float len_ab_squared=r043_add(r043_mul(dz_ab,dz_ab),r043_mul(dx_ab,dx_ab));
    len_ca_squared=r043_add(r043_mul(dz_ca,dz_ca),r043_mul(dx_ca,dx_ca));
    return r043_ratio(det,len_cb,len_ab_squared);
}
}

float othcar_calc_r_46f040(CourseProbe a,CourseProbe b,CourseProbe c){
    constexpr float tiny=9.999999747378752e-06f;
    constexpr float huge=100000.0f;
    float len_ca_squared{};
    const float ratio=r043_othcar_ratio(a,b,c,len_ca_squared);
    const float ar=std::fabs(ratio);
    // FCOMIP unordered takes the normal-calculation branch.  COMISS unordered
    // in the small-ratio sign test takes JBE and therefore returns -huge.
    if(!std::isnan(ar) && ar<tiny)return (!std::isnan(ratio)&&ratio>0.0f)?huge:-huge;
    return r043_radius_from_ratio(len_ca_squared,ratio);
}

float othcar_calc_r_alt_46f190(CourseProbe a,CourseProbe b,CourseProbe c){
    constexpr float tiny=9.999999747378752e-06f;
    constexpr float huge=100000.0f;
    float len_ca_squared{};
    const float ratio=r043_othcar_ratio(a,b,c,len_ca_squared);
    const float ar=std::fabs(ratio);
    if(!std::isnan(ar) && ar<tiny)return huge;
    float radius=r043_radius_from_ratio(len_ca_squared,ratio);
    const float abs_radius=std::fabs(radius);
    // The PC FCOMIP + JBE preserves NaNs and values <= 100000, otherwise the
    // original signed radius is overwritten by positive +100000.
    if(!std::isnan(abs_radius) && abs_radius>huge)radius=huge;
    return radius;
}

void othcar_get_r_479670(Bytes event,const PcOthcarGetRInputs& in){
    event.check(0,0xb58);
    const auto throttle=static_cast<std::int8_t>(event.u8(0xb52));
    if(throttle>0){event.put8(0xb52,static_cast<std::uint8_t>(throttle-1));return;}
    const std::int32_t type=event.i32(0x5c);
    const std::int16_t cs=event.i16(0x64);
    if(static_cast<std::int32_t>(cs)+3>static_cast<std::int32_t>(in.max_cs_len)){
        event.putf(0xb54,100000.0f);return;
    }
    if(type!=0 && (cs<10 || cs>170)){event.putf(0xb54,100000.0f);return;}
    if(!in.road_ok[0])return;
    if(!in.next_ok[0])return;
    if(!in.road_ok[1])return;
    if(!in.next_ok[1])return;
    if(!in.road_ok[2])return;
    const bool alternate=in.alternate_radius && ((event.u8(0x04)&0x20u)!=0u);
    const float radius=alternate?othcar_calc_r_alt_46f190(in.centers[0],in.centers[1],in.centers[2])
                                :othcar_calc_r_46f040(in.centers[0],in.centers[1],in.centers[2]);
    event.putf(0xb54,radius);
    event.put8(0xb52,4u);
}

bool platform_flag_bit2_44ff10(std::uint32_t flags){
    return ((flags>>2u)&1u)!=0u;
}

std::uint32_t platform_index_value_4505a0(Bytes table,std::uint32_t index){
    return table.u32(std::size_t(index)*4u);
}

std::uint32_t platform_pair_value_450630(Bytes table,std::uint32_t row,std::uint32_t column){
    const std::uint64_t index=std::uint64_t(column)+std::uint64_t(row)*4u;
    if(index>std::uint64_t(std::numeric_limits<std::size_t>::max()/4u))throw std::out_of_range("platform pair index");
    return table.u32(std::size_t(index)*4u);
}

bool platform_slot_kind_450750(std::int32_t slot,bool external_gate){
    return external_gate?slot==14:slot==4;
}

bool platform_counter_lt_60_48b350(std::int32_t counter){
    return counter<60;
}

void platform_ghost_route_4671d0(Bytes event,const PcPlatformGhost4671Inputs& in,
                                 PcPlatformGhost4671State& state){
    event.check(0,0x13);
    if(in.game_mode!=0x10 && in.game_mode!=0x12)return;
    if(in.route_state!=7 && !session_mode4_4962a0(in.session))return;
    if(in.timer>60){
        state.writer_offset=0x120u;
        state.reader_offset=0x120u;
        state.packet_11b=event.u8(0x11);
        state.stream_index=0u;
        state.packet_11c=event.u8(0x12);
        return;
    }
    state.service_pending=1u;
    if((in.packet_flags&0x0fu)!=0u)return;
    state.serialize_called=true;
    state.short_window=platform_counter_lt_60_48b350(in.frame_counter)?1u:0u;
}

void platform_ghost_record_47f780(Bytes event,Bytes records,Bytes index_table,Bytes pair_table,
                                  const PcPlatformGhost47f780Inputs& in,
                                  PcPlatformGhost47f780State& state){
    constexpr std::size_t stride=0xfd4u;
    constexpr std::uint32_t reset_bits=0xc7c34fffu; // PC [0x5b4488] == -99999.9921875f
    event.check(0,0x69);
    auto rec_off=[&](std::int32_t slot,std::size_t field)->std::size_t{
        if(slot<0)throw std::out_of_range("platform ghost record slot");
        const auto off=std::size_t(slot)*stride+field;records.check(off,1);return off;
    };
    const bool flag=platform_flag_bit2_44ff10(in.flags);
    const std::int32_t slot=in.entry_slot;
    if(in.game_mode!=0x10 || in.route_state!=0)return;
    if(in.protected_gate||flag){
        std::int32_t target{};
        if(flag){
            target=slot;
            records.put32(rec_off(slot,0x08),in.allocate_result);
        }else{
            target=slot-1;
            if(platform_slot_kind_450750(slot,in.slot_external_gate))
                records.put32(rec_off(slot,0x10),in.slot_token);
            records.put8(rec_off(slot,0x2f),event.u8(0x68));
        }
        if(state.init_state==1u){
            records.put32(rec_off(target,0x00),0x544f4851u);
            records.put32(rec_off(target,0x0c),state.sequence);
            records.put32(rec_off(target,0x04),platform_index_value_4505a0(index_table,std::uint32_t(target)));
        }
        for(std::uint32_t col=0;col<4u;++col)
            records.put32(rec_off(target,0x14u+std::size_t(col)*4u),
                          platform_pair_value_450630(pair_table,std::uint32_t(target),col));
        records.put32(rec_off(target,0x20),platform_index_value_4505a0(index_table,std::uint32_t(target)));
        if(platform_slot_kind_450750(target,in.slot_external_gate))
            records.put32(rec_off(target,0x1c),platform_index_value_4505a0(index_table,std::uint32_t(target)));
        state.frame_counter=0u;
        state.reset114_bits=reset_bits;state.reset118_bits=reset_bits;state.reset11c_bits=reset_bits;
        state.marker121=0u;state.marker123=0u;state.sequence=0u;state.init_state=1u;
    }
    if(flag)return;
    if(in.timer>60){state.reset_service_called=true;return;}
    state.record_ready=1u;
    state.record_service_called=true;
}

std::uint32_t course_nested_marker_44bdb0(bool manager_present,std::uint32_t marker){
    return manager_present?marker:0u;
}
std::uint32_t course_stage_limit_44be00(std::uint32_t value){return value;}
std::uint32_t course_active_slot_44be10(bool descriptor_present,std::uint32_t slot){
    return descriptor_present?slot:15u;
}
bool course_primary_ready_44be30(std::int32_t state){return state>17;}
bool course_secondary_ready_44be40(std::int32_t state){return state!=20;}
bool course_type_gate_44be50(std::int32_t type,std::int32_t primary_state,std::int32_t secondary_state){
    return type==0?primary_state>9:secondary_state==20;
}
std::uint32_t course_index_lookup_44be80(Bytes table,std::int32_t index){
    if(index<0)throw std::out_of_range("negative course lookup index");
    const std::size_t selected=index<66?static_cast<std::size_t>(index):0u;
    return table.u32(selected*4u);
}
PcCourseMatrixChoice course_disp_matrix_choice_44bed0(bool secondary){
    return secondary?PcCourseMatrixChoice::Secondary:PcCourseMatrixChoice::Primary;
}
PcCourseMatrixChoice course_area_matrix_choice_44bef0(bool secondary){
    return secondary?PcCourseMatrixChoice::Secondary:PcCourseMatrixChoice::Primary;
}
void course_clear_service_state_44bf10(PcCourseServiceState& state){
    state.clear_bc=0u;state.clear_c4=0u;state.manager_ptr=0u;
}
void course_mark_ready_44c080(PcCourseServiceState& state){state.ready=1u;}
std::uint8_t course_get_mode_byte_44c090(const PcCourseServiceState& state){return state.mode_byte;}
void course_set_mode_byte_44c0a0(PcCourseServiceState& state,std::uint8_t value){state.mode_byte=value;}
void course_copy_snapshot_44c0b0(Bytes destination,Bytes source){
    destination.check(0,120u);source.check(0,120u);
    for(std::size_t off=0;off<120u;off+=4u)destination.put32(off,source.u32(off));
}

void common_pl_car_inline_tail_4a82c4(Bytes event,Bytes work){
    event.check(0,0xda8);work.check(0,0x566);
    const auto d22=event.i8(0xd22);if(d22>0)event.put8(0xd22,std::uint8_t(d22-1));
    event.put16(0x40,static_cast<std::uint16_t>(work.i16(0x288)));
    event.put16(0x42,static_cast<std::uint16_t>(work.i16(0x37c)));
    event.put16(0x44,static_cast<std::uint16_t>(work.i16(0x470)));
    event.put16(0x46,static_cast<std::uint16_t>(work.i16(0x564)));
    const auto da4=event.i8(0xda4);if(da4>0)event.put8(0xda4,std::uint8_t(da4-1));
    const auto d90=event.i32(0xd90);if(d90>0)event.puti(0xd90,d90-1);
}


std::int32_t course_stage_unique_44dc50(bool descriptor_present,
                                        std::uint32_t descriptor_value,
                                        std::uint32_t fallback_value){
    return static_cast<std::int32_t>(descriptor_present?descriptor_value:fallback_value);
}

std::int32_t road_stage_window_44ddc0(std::int32_t stage_unique,
                                      std::uint16_t course_position,
                                      std::uint16_t rolling_reference){
    bool inside=false;
    if(stage_unique==0x1c){
        inside=course_position>=0x60u && course_position<=0x23eu;
    }else if(stage_unique==0x3a){
        const std::uint16_t center=static_cast<std::uint16_t>(rolling_reference+1u);
        const std::uint16_t lower=static_cast<std::uint16_t>(center-0x23eu);
        const std::uint16_t upper=static_cast<std::uint16_t>(center-0x60u);
        inside=course_position>=lower && course_position<=upper;
    }
    return inside?0x4fb:-1;
}

std::uint32_t road_stage_gate_44f0f0(std::int32_t stage_unique,
                                     std::uint16_t course_position,
                                     std::uint16_t rolling_reference,
                                     std::uint32_t protected_gate_value){
    return road_stage_window_44ddc0(stage_unique,course_position,rolling_reference)==-1
        ?0u:protected_gate_value;
}

void road_decode_sample_46ffc0(Bytes packed_sample,Bytes output){
    packed_sample.check(0,6);output.check(0,0x14);
    constexpr float kPacked=0.000244140625f;
    constexpr float kWorld=1000.0f;
    auto scale=[](std::int32_t v){
        float f=static_cast<float>(v);f=mulf(f,kPacked);return mulf(f,kWorld);
    };
    const auto x=packed_sample.i16(0);
    const auto z=packed_sample.i16(2);
    const std::uint16_t raw_y=static_cast<std::uint16_t>(packed_sample.u8(4)) |
                              (std::uint16_t(packed_sample.u8(5))<<8u);
    std::int32_t y=std::int32_t(raw_y&0x7fffu);
    if((y&0x4000)!=0)y|=~0x7fff;
    output.putf(0,scale(x));output.putf(4,scale(y));output.putf(8,scale(z));
    output.putf(0x0c,(packed_sample.u8(5)&0x80u)?-3.691162109375f:1.384033203125f);
    output.put32(0x10,0u);
}

PcRoadTableChoice road_table_choice_4700d0(std::int32_t type,
                                           std::int32_t row,
                                           std::int32_t selector){
    PcRoadTableChoice out{};
    if(type==0){
        if(selector<0 || selector>=6 || row<0)return out;
        const std::int64_t index=std::int64_t(selector)+std::int64_t(row)*6;
        if(index>std::numeric_limits<std::int32_t>::max())return out;
        out.valid=true;out.secondary=false;out.index=static_cast<std::int32_t>(index);
        out.byte_offset=std::size_t(index)*0x1030u;return out;
    }
    if(selector<0 || row<0)return out;
    const std::int64_t index=std::int64_t(selector)+std::int64_t(row)*12;
    if(index>std::numeric_limits<std::int32_t>::max())return out;
    out.valid=true;out.secondary=true;out.index=static_cast<std::int32_t>(index);
    out.byte_offset=std::size_t(index)*0x70cu;return out;
}

namespace {
const Bytes* r046_block(const PcRoadSampleTables& tables,const PcRoadTableChoice& choice){
    if(!choice.valid)return nullptr;
    if(choice.secondary){
        if(!tables.secondary_blocks || std::size_t(choice.index)>=tables.secondary_count)return nullptr;
        return &tables.secondary_blocks[std::size_t(choice.index)];
    }
    if(!tables.primary_blocks || std::size_t(choice.index)>=tables.primary_count)return nullptr;
    return &tables.primary_blocks[std::size_t(choice.index)];
}
float r046_sample_coord(std::int16_t v){
    float f=static_cast<float>(v);f=mulf(f,0.000244140625f);return mulf(f,1000.0f);
}
}

bool road_side_test_479a70(CourseProbe point,Bytes on_road_place,
                            std::int8_t current_selector,
                            std::int8_t alternate_selector,
                            const PcRoadSampleTables& tables){
    on_road_place.check(0,0x0c);
    const std::int32_t type=on_road_place.i32(0);
    const auto current_choice=road_table_choice_4700d0(type,0,current_selector);
    const Bytes* current=r046_block(tables,current_choice);
    if(!current)throw std::out_of_range("road current sample block");
    current->check(0,2);if(static_cast<std::uint16_t>(current->u8(0)|(std::uint16_t(current->u8(1))<<8u))!=0x4f53u)return false;
    const auto cs=on_road_place.i16(8);if(cs<0)throw std::out_of_range("negative road sample index");
    const std::size_t rec=4u+std::size_t(cs)*6u;current->check(rec,6);
    const float first_x=r046_sample_coord(current->i16(rec));
    const float first_z=r046_sample_coord(current->i16(rec+2));
    if((current->u8(rec+5)&0x80u)!=0u)return false;
    const auto alternate_choice=road_table_choice_4700d0(type,0,alternate_selector);
    const Bytes* alternate=r046_block(tables,alternate_choice);
    if(!alternate)throw std::out_of_range("road alternate sample block");
    alternate->check(0,2);
    if(static_cast<std::uint16_t>(alternate->u8(0)|(std::uint16_t(alternate->u8(1))<<8u))!=0x4f53u)return true;
    alternate->check(rec,6);
    std::array<std::uint8_t,0x14> decoded_bytes{};Bytes decoded(decoded_bytes.data(),decoded_bytes.size());
    road_decode_sample_46ffc0(alternate->sub(rec,6),decoded);
    constexpr float eps=1.1920928955078125e-07f;
    const float surface=decoded.f32(0x0c);
    if(!std::isnan(surface) && eps>=surface)return true;
    const float dz1=subf(first_z,point.z),dx1=subf(first_x,point.x);
    const float dz2=subf(decoded.f32(8),point.z),dx2=subf(decoded.f32(0),point.x);
    const float d1=addf(mulf(dz1,dz1),mulf(dx1,dx1));
    const float d2=addf(mulf(dz2,dz2),mulf(dx2,dx2));
    return !(d1>d2); // COMISS/JBE: unordered follows the true branch.
}

void road_lane_classify_47b890(Bytes event,const PcRoadLaneInputs& in){
    event.check(0,0xc34);
    std::int32_t lane{};
    if((event.u8(4)&1u)==0u){
        float v=mulf(event.f32(0xb08),0.25f);
        v=addf(v,static_cast<float>(event.i8(0x66)));
        v=addf(v,0.5f);lane=r034_cvttss2si(v);
    }else if(event.i32(0x5c)==1){
        lane=r034_cvttss2si(mulf(event.f32(0x268),0.25f));
        if(event.i32(0x60)==101)lane+=3;
    }else{
        float width=in.route_width;
        if(!std::isnan(width)&&1.0f>width)width=1.0f;
        else if(!std::isnan(width)&&width>4.0f)width=4.0f;
        lane=r034_cvttss2si(event.f32(0x268)/width);
        event.put8(0xc33,in.route_flags);
        if((in.route_flags&0x20u)!=0u)++lane;
        if((in.route_flags&0x10u)!=0u)++lane;
    }
    if(lane<0)lane=0;
    if(lane>5)lane=5;
    event.put8(0xc32,static_cast<std::uint8_t>(lane));
    std::int32_t low=0,high=5;
    if(event.i32(0x5c)!=0){
        if(event.i32(0x60)==100){low=0;high=2;}else{low=3;high=5;}
    }else{
        const auto gate=road_stage_gate_44f0f0(in.stage_unique,
                                               static_cast<std::uint16_t>(event.i16(0x64)),
                                               in.rolling_reference,in.protected_gate_value);
        if(gate!=0u && event.i8(0x66)>=3){low=3;high=5;}
        else if(gate!=0u){low=0;high=2;}
    }
    lane=event.i8(0xc32);if(lane<low)lane=low;if(lane>high)lane=high;
    event.put8(0xc32,static_cast<std::uint8_t>(lane));
}

bool refresh_cached_road_4a3f80(Bytes event,std::int32_t polygon_hint,
                                 const PcRoadCacheRefreshInputs& in){
    event.check(0,0x10d4);
    if(in.selector_result>=0){
        const bool same=event.u32(0x5c)==event.u32(0x10c0) &&
                        event.i16(0x64)==event.i16(0x10c8) &&
                        event.u32(0x68)==event.u32(0x10cc) &&
                        polygon_hint==in.selector_result;
        if(same)return true;
        event.puti(0x10d0,-1);
    }
    for(std::size_t off=0;off<in.query_output.size();++off)event.put8(0x105c+off,in.query_output[off]);
    if(!in.query_success)return false;
    event.puti(0x10d0,polygon_hint);
    for(std::size_t off=0;off<16u;++off)event.put8(0x10c0+off,event.u8(0x5c+off));
    return true;
}

namespace {
float r046_x87_half_sum(float a,float b){
    // FLD/FADD/FMUL 0.5/FSTP: both binary32 inputs are summed exactly in the
    // original x87 precision modes used by the oracle, then spilled once.
    return x87_float((X87(a)+b)*0.5f);
}
float r046_cross_distance(float point_z,float origin_z,float direction_x,
                          float point_x,float origin_x,float direction_z){
    const float z=subf(point_z,origin_z),x=subf(point_x,origin_x);
    return subf(mulf(z,direction_x),mulf(x,direction_z));
}
float r046_negative_dot(float point_x,float origin_x,float direction_x,
                        float point_z,float origin_z,float direction_z){
    const float x=mulf(subf(point_x,origin_x),direction_x);
    const float z=mulf(subf(point_z,origin_z),direction_z);
    return subf(0.0f,addf(x,z));
}
}

void get_road_ofs_4a4010(Bytes event,const PcGetRoadOfsInputs& in,
                         Bytes primary_display_matrix,
                         const PcRoadSampleTables& tables){
    event.check(0,0x10d4);primary_display_matrix.check(0,64);
    const auto hint=event.i32(0x1c0);
    if(!refresh_cached_road_4a3f80(event,hint,in.cache))return;

    const CourseProbe position{event.f32(0x14),event.f32(0x18),event.f32(0x1c)};
    const CourseProbe center{event.f32(0x1064),event.f32(0x1068),event.f32(0x106c)};
    const CourseProbe a{event.f32(0x1080),event.f32(0x1084),event.f32(0x1088)};
    const CourseProbe b{event.f32(0x108c),event.f32(0x1090),event.f32(0x1094)};
    const CourseProbe c{event.f32(0x1098),event.f32(0x109c),event.f32(0x10a0)};
    const CourseProbe d{event.f32(0x10a4),event.f32(0x10a8),event.f32(0x10ac)};

    const CourseProbe ab{r046_x87_half_sum(b.x,a.x),0.0f,r046_x87_half_sum(b.z,a.z)};
    const CourseProbe cd{r046_x87_half_sum(c.x,d.x),0.0f,r046_x87_half_sum(c.z,d.z)};
    std::array<std::uint8_t,8> dir_bytes{};Bytes dir(dir_bytes.data(),dir_bytes.size());
    pc_direction_xz(cd.x,cd.z,ab.x,ab.z,dir.sub(0,4),dir.sub(4,4));
    float dx=dir.f32(0),dz=dir.f32(4);
    event.putf(0x26c,r046_cross_distance(center.z,position.z,dx,center.x,position.x,dz));
    event.putf(0x270,r046_negative_dot(b.x,position.x,dx,b.z,position.z,dz));
    event.putf(0x274,r046_negative_dot(d.x,position.x,dx,d.z,position.z,dz));

    pc_direction_xz(c.x,c.z,a.x,a.z,dir.sub(0,4),dir.sub(4,4));dx=dir.f32(0);dz=dir.f32(4);
    event.putf(0x268,r046_cross_distance(a.z,position.z,dx,a.x,position.x,dz));
    pc_direction_xz(d.x,d.z,b.x,b.z,dir.sub(0,4),dir.sub(4,4));dx=dir.f32(0);dz=dir.f32(4);
    event.putf(0x264,r046_cross_distance(b.z,position.z,dx,b.x,position.x,dz));

    constexpr float eps=1.1920928955078125e-07f;
    auto stage_gate=[&](){return road_stage_gate_44f0f0(in.lane.stage_unique,
        static_cast<std::uint16_t>(event.i16(0x64)),in.lane.rolling_reference,
        in.lane.protected_gate_value);};
    auto side=[&](std::int8_t current,std::int8_t alternate){
        const auto local=pc_inverse_point(primary_display_matrix,position);
        return road_side_test_479a70(local,event.sub(0x5c,0x10),current,alternate,tables);
    };
    const float right=event.f32(0x264),left=event.f32(0x268);
    if(!std::isnan(right)&&right>eps){
        event.puti(0x27c,2);event.put8(0xc32,5);
        if(event.i32(0x5c)==1&&event.i32(0x60)==100)event.put8(0xc32,2);
        if(stage_gate()!=0u)event.put8(0xc32,side(2,5)?2u:5u);
    }else if(!std::isnan(left)&&eps>left){
        event.puti(0x27c,1);event.put8(0xc32,0);
        if(event.i32(0x5c)==1&&event.i32(0x60)==101)event.put8(0xc32,3);
        if(stage_gate()!=0u)event.put8(0xc32,side(0,3)?0u:3u);
    }else{
        event.puti(0x27c,0);
        if(in.use_lane_classifier)road_lane_classify_47b890(event,in.lane);
        else{
            auto lane=r034_cvttss2si(mulf(event.f32(0x268),0.25f));
            if(event.i32(0x60)==101)lane+=3;
            event.put8(0xc32,static_cast<std::uint8_t>(lane));
        }
    }
    auto lane=event.i8(0xc32);if(lane<0)event.put8(0xc32,0);else if(lane>5)event.put8(0xc32,5);
}

void common_pl_car_4a8100(Bytes event,Bytes work,Bytes body_params,
                          const std::array<Bytes,4>& wheels,
                          const PcCommonPlCarParentInputs& in,
                          const PcCommonPlCarServices& services){
    event.check(0,0xda8);work.check(0,work_size);body_params.check(0,0x5a8);
    for(const auto& wheel:wheels)wheel.check(0,0x56);
    auto call=[&](std::uint32_t entry){if(services.callback)services.callback(services.user,entry);else outrun::driving::service_hole("common_pl_car_4a8100","services.callback");};

    // CommonPlCar always queries the timer before applying the game-mode gate.
    call(0x0049b2d0u);
    const bool active=(in.game_mode==0x0d) ||
                      (in.game_mode==0x10 && in.timer>60);
    if(active){event.put32(4,event.u32(4)|8u);work.put32(0,0u);}
    else{event.put32(4,event.u32(4)&~8u);work.put32(0,1u);}

    call(0x004a4010u); // GetRoadOfs
    call(0x004a61f0u); // force-work wrapper
    call(0x004a0000u); // MakeForceWorkTire

    // REP MOVSD 0x3c dwords: work[0..0xef] -> work[0xf0..0x1df].
    for(std::size_t off=0;off<0xf0u;off+=4u)work.put32(0xf0u+off,work.u32(off));
    call(0x00517410u); // ActionForce2(work+0xf0,event+0xdbc)
    call(0x004a2fa0u); // RearGripCtrl
    call(0x004a3310u); // SlipAngleCtrl
    call(0x004a3950u); // CorneringCtrl
    call(0x004a7ec0u); // CalcPlBody_2nd

    // Four parent-owned suspension rest offsets are written immediately before
    // ColiCar, using the body-parameter axle values.
    wheels[0].putf(0x28,subf(wheels[0].f32(0x08),body_params.f32(0x558)));
    wheels[1].putf(0x28,subf(wheels[1].f32(0x08),body_params.f32(0x558)));
    wheels[2].putf(0x28,subf(wheels[2].f32(0x08),body_params.f32(0x5a4)));
    wheels[3].putf(0x28,subf(wheels[3].f32(0x08),body_params.f32(0x5a4)));

    call(0x00519830u); // ColiCar
    call(0x004a1b70u); // CalcSuspensionForce
    call(0x004a1a90u); // CalcTireLoad
    call(0x004a63c0u); // CalcContactMatrix
    call(0x004a65c0u); // MaximumVelocityCheck
    call(0x004a1140u); // CopyCarWork
    call(0x004a1680u); // SetCarCamera
    call(0x00458e40u); // HandicapControl
    call(0x004a4830u); // CheckDrivingSkill
    call(0x004a4900u); // CheckChickenDriver
    call(0x004a4ba0u); // AssistChickenDriver
    call(0x004a2400u); // CalcCrushMotion
    call(0x004a25f0u); // CheckNightAndTunnel
    call(0x004a2650u); // CalcDispMatrix
    call(0x004a2d70u); // CalcVibrateMatrix
    call(0x004a2910u); // CheckReverseCar
    call(0x004a3d40u); // CalcLightRate
    call(0x004a45f0u); // CalcOfsLeftLane
    call(0x0046eb40u); // CalcDispSteeringAngle
    call(0x004a4710u); // RecordGhostCar

    if(in.route_state==7){
        call(0x004671d0u);
    }else{
        call(0x004962a0u);
        if(in.session_mode4)call(0x004671d0u);
        else call(0x0047f780u);
    }
    call(0x00479670u); // othcarGetR
    common_pl_car_inline_tail_4a82c4(event,work);
}



std::uint8_t game_flag_43f9c0(const PcGameControlGlobals& state){return state.flag_780248;}
void set_game_flag_43f9d0(PcGameControlGlobals& state,std::uint8_t value){state.flag_780248=value;}
void set_game_state_byte_43f9e0(PcGameControlGlobals& state,std::uint8_t value){state.flag_780270=value;}
std::uint8_t game_state_byte_43f9f0(const PcGameControlGlobals& state){return state.flag_780270;}
std::uint32_t game_state_dword_43fa00(const PcGameControlGlobals& state){return state.value_780278;}
void set_game_state_dword_43fa10(PcGameControlGlobals& state,std::uint32_t value){state.value_780278=value;}

void game_broadcast_43cc20(PcGameBroadcastState& state,std::uint32_t value){
    state.slots.fill(value);state.state_780240=0;state.value_78023c=value;
}

void operation_input_49fad0(Bytes event,const PcOperationInputInputs& in){
    event.check(0,0xd52);
    if(in.game_mode==0x0d||in.game_mode==0x0f||in.game_mode==0x12){
        event.puti(0x34,0);event.puti(0x38,0);event.put16(0x202,0);return;
    }
    if(event.i16(0xd50)>120){event.puti(0x34,0);event.puti(0x38,255);return;}
    event.puti(0x34,in.volume1);
    const float slip=event.f32(0x2f8);
    if(!std::isnan(slip)&&slip>0.0f){event.puti(0x38,255);event.put16(0x202,std::uint16_t(std::uint32_t(in.volume0)<<8));return;}
    event.puti(0x38,in.volume2);event.put16(0x202,std::uint16_t(std::uint32_t(in.volume0)<<8));
}

void check_shift_warning_4a50f0(Bytes event,Bytes params,const PcShiftWarningInputs& in){
    event.check(0,0xe88);params.check(0,0x10a4);
    if(event.u8(0x13)!=1u)return;
    if(event.u8(0x282)>0u||event.u8(0x283)>0u)return;
    const float a=event.f32(0x2c8);if(!std::isnan(a)&&a>0.0f)return;
    const float b=event.f32(0x2f8);if(!std::isnan(b)&&b>0.0f)return;
    if(event.i32(0xe84)!=0)return;
    const std::uint32_t gear=event.u32(0x208),max_gear=params.u32(0x10a0);
    bool active=gear<max_gear;
    const float rpm=event.f32(0x1c4),threshold=event.f32(0xe10u+std::size_t(gear)*4u);
    active=active && (!std::isnan(rpm)&&!std::isnan(threshold)&&rpm>threshold);
    active=active && event.i32(0x3c)>=255;
    active=active && in.volume1>=175;
    active=active && in.volume2<=0;
    active=active && event.i8(0xd36)<=0;
    std::int32_t counter=event.i32(0xe7c);
    if(active)counter=std::int32_t(std::uint32_t(counter)+1u);
    else{counter=std::int32_t(std::uint32_t(counter)-1u);if(counter>180)counter=180;}
    event.puti(0xe7c,counter);
    if(event.u8(0x296)==1u){counter=0;event.puti(0xe7c,0);}
    if(counter>=480){counter=0;event.puti(0xe7c,0);event.puti(0xe84,1);event.puti(0xe80,0);}
    else{event.puti(0xe84,0);event.puti(0xe80,counter>=180?1:0);}
    counter=event.i32(0xe7c);if(counter<0)counter=0;if(counter>480)counter=480;event.puti(0xe7c,counter);
}

void car_calc_total_cs_len_455f50(Bytes event,Bytes history,std::uint32_t sample_counter){
    event.check(0,0x264);history.check(0,64u*0x24u);
    const std::size_t rec=std::size_t(sample_counter&0x3fu)*0x24u;
    history.put32(rec+0x00,sample_counter);history.put32(rec+0x04,event.u32(0x14));history.put32(rec+0x08,event.u32(0x1c));
    history.putf(rec+0x0c,subf(event.f32(0x14),event.f32(0x16c)));
    history.putf(rec+0x10,subf(event.f32(0x1c),event.f32(0x174)));
    history.putf(rec+0x14,i32f(event.i16(0x2e)));
    history.putf(rec+0x18,i32f(std::int32_t(event.i16(0x2e))-std::int32_t(event.i16(0x17e))));
    history.put32(rec+0x1c,(event.u32(4)>>18u)&1u);
    std::uint32_t v=event.u32(0x260)&0xffffu;v|=event.u32(0x5c)<<22u;v<<=8u;v|=event.u8(0x10);history.put32(rec+0x20,v);
}

void car_calc_current_stage_progress_4a2130(Bytes event,const PcStageProgressInputs& in,PcStageProgressHistory& history){
    event.check(0,0x264);
    const std::uint16_t previous=std::uint16_t(event.u32(0x260));
    const std::uint16_t current=std::uint16_t(event.u32(0x64));
    std::uint16_t offset=std::uint16_t(event.u32(0x25e));
    // The PC keeps the subtraction in ESI, zero-extends the existing lane offset
    // into EDI, adds the two as 32-bit integers, and only then sign-extends SI.
    // Consequently the branch discriminator is signed16(current-previous+offset),
    // not merely signed16(current-previous).
    const std::int16_t delta=static_cast<std::int16_t>(
        std::uint16_t(std::uint16_t(current-previous)+offset));
    bool wrapped=false;
    const auto backward=[&](std::uint16_t end){
        if(end==0)return false;
        const std::int32_t half=std::int32_t(static_cast<std::int16_t>(end))/2;
        if(std::int32_t(delta)<-half){offset=std::uint16_t(offset+end+1u);return true;}
        return false;
    };
    const auto forward=[&](std::uint16_t end){
        if(end==0)return false;
        const std::int32_t half=std::int32_t(static_cast<std::int16_t>(end))/2;
        if(std::int32_t(delta)>half){offset=std::uint16_t(offset+std::uint16_t(0xffffu-end));return true;}
        return false;
    };
    const std::uint32_t mode=event.u32(0x5c);
    if(mode==1u){wrapped=backward(in.course_end0);if(!wrapped)wrapped=forward(in.course_end1);}
    else if(mode==0u){wrapped=backward(in.course_end1);if(!wrapped)wrapped=forward(in.course_end0);}
    if(wrapped){
        const std::uint16_t probe=std::uint16_t(current+offset);
        if(probe<previous)return;
        event.put16(0x25e,offset);
    }
    const std::uint16_t effective=std::uint16_t(event.u32(0x25e));
    event.put16(0x262,previous);
    event.put16(0x260,std::uint16_t(current+effective));
    if((event.u8(4)&1u)!=0u&&wrapped&&(in.global_gate||in.record_gate)&&history.count<30u){
        history.offset[history.count]=std::uint16_t(event.u32(0x25e));
        history.position[history.count]=std::uint16_t(event.u32(0x260));
        ++history.count;
    }
}

std::uint32_t get_now_heart_calc_mode_45c440(std::uint32_t value){return value;}

namespace {
inline float r047_x87_accumulate_angle(std::int32_t v,float sum){
    return x87_float(X87(v)*kAngleUnit+sum);                      // FILD; FMUL; FADD; FSTP
}
inline float r047_wrap_pi(float x){
    constexpr float pi=3.1415927410125732f;
    constexpr float two_pi=6.2831854820251465f;
    constexpr float neg_pi=-3.1415927410125732f;
    if(std::isnan(x)) return x;
    while(x>pi) x=subf(x,two_pi);
    while(x<neg_pi) x=addf(x,two_pi);
    return x;
}
}
void set_old_param_buffer_4a2ee0(Bytes event){
    event.check(0,0xd38);float sum=0.0f;
    for(unsigned n=0;n<20;++n){const std::size_t dst=0x1bcu-n*2u;const auto v=event.i16(dst-2u);event.put16(dst,std::uint16_t(v));sum=r047_x87_accumulate_angle(v,sum);}
    const std::uint16_t raw=std::uint16_t(std::uint16_t(event.u32(0xc2c))+std::uint16_t(event.u32(0x162)));
    event.put16(0x194,raw);const std::int32_t sv=static_cast<std::int16_t>(raw);
    constexpr float kAverage=0.0476190485060215f,kAngleScale=10430.3779296875f;
    float angle=x87_float((X87(sv)*kAngleUnit+sum)*kAverage);    // FILD; FMUL; FADD; FMUL; FSTP
    angle=r047_wrap_pi(angle);
    const float scaled=x87_float(X87(angle)*kAngleScale);         // FLD; FMUL; FSTP
    const auto out=static_cast<std::int32_t>(std::trunc(scaled));event.put16(0xd34,std::uint16_t(out));
    if((event.u8(4)&1u)!=0u){auto flags=event.u32(0x0c);if(event.i8(0xd36)>0)flags|=0x00100000u;else flags&=~0x00100000u;event.put32(0x0c,flags);}
}

}

namespace outrun::driving {
std::uint32_t race_counter_44fdf0(std::uint32_t value){return value;}
void set_timeup_counter_44fe30(std::uint32_t& value,std::uint32_t input){value=input;}
std::uint32_t get_timeup_counter_44fe40(std::uint32_t value){return value;}
void set_race_flag0_44fe50(std::uint32_t& flags,bool enabled){flags=(flags&~1u)|(enabled?1u:0u);}
bool get_race_flag0_44fe70(std::uint32_t flags){return (flags&1u)!=0u;}
void set_race_flag2_44fef0(std::uint32_t& flags,bool enabled){flags=(flags&~4u)|(enabled?4u:0u);}
float ham_nos_speed_45d0e0(float value){return value;}

void game_pl_car_ctrl_4a8330(Bytes e,const PcGamePlCarParentInputs& in,
                             PcGamePlCarParentState& st,
                             const PcGamePlCarServices& svc){
    e.check(0,0x104cu);
    auto call=[&](std::uint32_t pc){if(svc.callback)svc.callback(svc.user,pc);else outrun::driving::service_hole("game_pl_car_ctrl_4a8330","svc.callback");};
    call(0x43f9f0u);
    if(in.game_state_byte==0u){call(0x43f9c0u);if(in.game_flag==0u)call(0x44ff10u);}

    std::uint32_t x=e.u32(8);std::uint32_t a=((x+x)^x)&0x80u;a^=x;
    const float speed_delta=subf(e.f32(0x1c4),e.f32(0x178));
    std::uint32_t flags=e.u32(4);std::uint32_t d=((flags>>2u)&0x8000u)|(flags&0x10000u);
    d>>=4u;d|=((a&0x100u)<<1u);a&=0xffffc5ffu;d|=a;e.put32(8,d);
    e.put32(0x1d8,e.u32(0x208));e.put32(0x1d4,e.u32(0x1d0));e.putf(0x1d0,speed_delta);
    const auto old_1dc=e.u32(0x1dc);e.put32(0x1e8,e.u32(0x20));e.put32(0x1e4,e.u32(0x1e0));
    float old_1dc_f{};std::memcpy(&old_1dc_f,&old_1dc,4);e.put32(0x1dc,e.u32(0x2dc));e.putf(0x1e0,subf(e.f32(0x2dc),old_1dc_f));
    e.put32(0x1ec,e.u32(0x24));e.put32(0x1f0,e.u32(0x28));
    call(0x43cc20u);

    auto c=e.i8(0xd23);if(c>0)e.put8(0xd23,std::uint8_t(c-1));
    for(unsigned off=0;off<12;off+=4)e.put32(0x16c+off,e.u32(0x14+off));
    e.put16(0x17c,std::uint16_t(e.i16(0x2c)));e.put16(0x17e,std::uint16_t(e.i16(0x2e)));e.put16(0x180,std::uint16_t(e.i16(0x30)));
    for(unsigned off=0;off<16;off+=4)e.put32(0x184+off,e.u32(0x5c+off));
    for(unsigned off=0;off<12;off+=4)e.put32(0x1040+off,e.u32(0x2d8+off));
    for(unsigned off=0;off<12;off+=4)e.put32(0x1034+off,e.u32(0x2e4+off));
    e.put32(0x2f0,e.u32(0x2f0)&~1u);
    call(0x44c940u);call(0x451350u);
    flags=e.u32(4);flags=(flags&~0x03000000u)|((in.entry_mode&3u)<<24u);e.put32(4,flags);
    if(e.i32(0x5c)==0)e.puti(0x60,0);
    else {const auto m=(e.u32(4)>>24u)&3u;e.puti(0x60,m==0u?100:(m==1u?101:-1));}

    call(0x409ef0u);call(0x487740u);call(0x40a270u);call(0x40a7d0u);call(0x40a270u);call(0x40a0d0u);
    const float divisor=in.world_scale_divisor;const float scale=(divisor==0.0f)?0.0f:(1.0f/divisor);
    st.world_position={mulf(e.f32(0x20),scale),mulf(e.f32(0x24),scale),mulf(e.f32(0x28),scale)};
    call(0x49fad0u);call(0x49fb70u);call(0x4a4d20u);call(0x4a50f0u);call(0x4a5260u);call(0x502c90u);call(0x4a8100u);call(0x40a010u);
    call(0x455f50u);call(0x4a2130u);
    std::int32_t numerator=std::int16_t(e.i16(0x64));std::uint32_t base=in.stage_denominator_base;
    if(e.i32(0x5c)==0){call(0x43d470u);base=std::uint32_t(in.course_end)+1u;st.stage_denominator_base=base;}
    else {st.stage_denominator_base=base;numerator+=std::int32_t(base);}
    const float denom=float(base+0xf2u);e.putf(0x102c,denom!=0.0f?float(numerator)/denom:0.0f);e.put8(0x66,0u);
    call(0x45a2b0u);e.put8(0xdb0,in.rank);
    if(in.route_state==2){call(0x45c440u);if(in.heart_mode==0x12u)call(0x4a5650u);else {call(0x55a930u);if(in.network_tail_active)call(0x46c390u);}}
    else {call(0x55a930u);if(in.network_tail_active)call(0x46c390u);}
    call(0x4a2ee0u);
    if((e.u32(4)&0x800000u)!=0u)st.capture_words=in.capture_words;
    if(in.route_state==4)call(0x457770u);
}
}

namespace outrun::driving {
namespace {
OR2_R033_NOINLINE std::int32_t r049_ftol2_low32(float source){
    constexpr float kRadToDeg=9.54929637908935546875f;
    // FLD; FMUL; CALL 0x582194 (MSVC _ftol2): low dword of the signed
    // 64-bit truncation toward zero of the live x87 product.
    return x87_ftol2_low32(X87(source)*kRadToDeg);
}
inline bool pc_comiss_above(float left,float right){
    return !std::isnan(left)&&!std::isnan(right)&&left>right;
}
}

void control_timeup_braking_49fb70(Bytes event,Bytes work,Bytes params,std::int32_t counter){
    event.check(0,0xd4e);work.check(0,0x634);params.check(0,0x17c4);
    if(counter>60)return;
    if(counter<=10)event.put16(0x202,0u);

    if(counter>0){
        const float countf=i32f(counter);
        const float ratio=mulf(divf(event.f32(0x1c4),countf),3624.0400390625f);
        const float threshold=mulf(params.f32(0x17c0),9.8066501617431640625f);
        if(pc_comiss_above(ratio,threshold))event.puti(0x38,255);
        else if(event.i32(0x38)<10)event.puti(0x38,10);
    }else{
        event.puti(0x38,255);
    }
    event.puti(0x34,0);

    if(counter>0){
        CourseProbe velocity{work.f32(0x5c),work.f32(0x60),work.f32(0x64)};
        const X87 length=unit_vector_ext(velocity);
        const float braking=addf(x87_float(length/X87(counter)),
                                 mulf(event.f32(0x1c8),60.200000762939453125f));
        if(pc_comiss_above(braking,0.0f)){
            const CourseProbe delta=scale_vec_x87(velocity,braking);
            CourseProbe current{work.f32(0x5c),work.f32(0x60),work.f32(0x64)};
            current=sub_vec_x87(current,delta);
            work.putf(0x5c,current.x);work.putf(0x60,current.y);work.putf(0x64,current.z);
        }
    }
    if(counter<=1)return;

    const float angle=x87_i32_angle(static_cast<std::int32_t>(event.i16(0xd4c)));
    CourseProbe heading{-r032_x87_sin(angle),0.0f,-r032_x87_cos(angle)};
    const CourseProbe road_normal{work.f32(0x628),work.f32(0x62c),work.f32(0x630)};
    heading=projective_vec_x87(heading,road_normal);
    (void)unit_vector_ext(heading);

    CourseProbe lateral{work.f32(0x5c),work.f32(0x60),work.f32(0x64)};
    lateral=projective_vec_x87(lateral,heading);
    const X87 lateral_length=unit_vector_ext(lateral);
    const float amount=x87_float(lateral_length/X87(counter));
    lateral=scale_vec_x87(lateral,amount);
    CourseProbe current{work.f32(0x5c),work.f32(0x60),work.f32(0x64)};
    current=sub_vec_x87(current,lateral);
    work.putf(0x5c,current.x);work.putf(0x60,current.y);work.putf(0x64,current.z);
}

void check_wanderer_4a5260(Bytes event,Bytes work,const PcCheckWandererInputs& in){
    event.check(0,0xe9c);work.check(0,0x634);
    if(in.route_state==0)return;

    const std::uint32_t allowed_base=(in.stage_level!=0)?0x801cu:0x809cu;
    const std::uint32_t disallowed_mask=~allowed_base;
    const std::uint32_t previous=event.u32(0xe90);
    if((event.u32(0x244)&disallowed_mask)==0u){
        event.put32(0xe90,1u);
        event.put32(0xe94,event.u32(0x26c));
    }else{
        event.put32(0xe90,0u);
        if(previous!=0u){
            const float angle=x87_i32_angle(static_cast<std::int32_t>(event.i16(0xd4c)));
            CourseProbe heading{-r032_x87_sin(angle),0.0f,-r032_x87_cos(angle)};
            const CourseProbe road_normal{work.f32(0x628),work.f32(0x62c),work.f32(0x630)};
            heading=projective_vec_x87(heading,road_normal);
            heading=d3dx_normalized_vec(heading);

            CourseProbe lateral{work.f32(0x5c),work.f32(0x60),work.f32(0x64)};
            lateral=projective_vec_x87(lateral,heading);
            const float lateral_speed=r032_vec_length_spill(lateral);
            event.putf(0xe98,std::numeric_limits<float>::max());
            constexpr float kEpsilon=1.1920928955078125e-7f;
            if(!(kEpsilon>std::fabs(lateral_speed))){
                const CourseProbe cross=r034_cross(lateral,heading);
                float value{};
                if(!std::isnan(cross.y)&&cross.y>=0.0f)
                    value=mulf(divf(event.f32(0x264),lateral_speed),-60.200000762939453125f);
                else
                    value=mulf(divf(event.f32(0x268),lateral_speed),60.200000762939453125f);
                event.putf(0xe98,value);
            }
        }
    }

    if(event.u32(0xe90)!=0u){
        std::int32_t timer=event.i32(0xe88)+2;event.puti(0xe88,timer);
        if(timer>120){timer=120;event.puti(0xe88,timer);}
        event.puti(0xe8c,timer);
    }else{
        std::int32_t timer=event.i32(0xe88)-1;event.puti(0xe88,timer);
        if(timer<0)event.puti(0xe88,0);
    }

    const bool cancel=event.u8(0x283)>0u || pc_comiss_above(event.f32(0x2c8),0.0f) ||
                      pc_comiss_above(event.f32(0x2f8),0.0f) || event.i8(0xd36)>0;
    if(cancel)event.puti(0xe88,0);
}

void ham_nos_set_speed_4a5650(Bytes event,Bytes work,Bytes params,float nos_speed){
    event.check(0,0x2b8);work.check(0,0x610);params.check(0,0x1648);
    if(!std::isnan(nos_speed)&&nos_speed<=0.0f)return;

    const float target=mulf(nos_speed,2.7685492038726806640625f);
    event.putf(0x1c4,target);
    const float current=event.f32(0x178);
    if(pc_comiss_above(current,target))event.put32(0x1c4,event.u32(0x178));

    if(event.u32(0x1f4)!=0u){
        CourseProbe velocity{event.f32(0x20),event.f32(0x24),event.f32(0x28)};
        (void)unit_vector_ext(velocity);
        velocity=scale_vec_x87(velocity,event.f32(0x1c4));
        event.putf(0x20,velocity.x);event.putf(0x24,velocity.y);event.putf(0x28,velocity.z);
    }else{
        const float angle=x87_i32_angle(static_cast<std::int32_t>(event.i16(0x2e)));
        event.putf(0x20,-x87_sin_mul_spill(angle,event.f32(0x1c4)));
        event.putf(0x24,0.0f);
        event.putf(0x28,-x87_cos_mul_spill(angle,event.f32(0x1c4)));
    }

    event.put32(0x208,params.u32(0x10a0));
    event.put32(0x21c,params.u32(0x1644));
    event.puti(0x3c,255);
    const std::int32_t converted=r049_ftol2_low32(event.f32(0x21c));
    event.puti(0x20c,converted);event.puti(0x210,converted);event.puti(0x48,converted);

    constexpr float kHz49=60.200000762939453125f;
    const float speed=event.f32(0x1c4);
    const float all=mulf(speed,kHz49);
    for(auto off:{0x608u,0x514u,0x420u,0x32cu})work.putf(off,all);
    const float front=mulf(divf(speed,params.f32(0xb48)),kHz49);
    work.putf(0x424,front);work.putf(0x330,front);
    const float rear=mulf(divf(speed,params.f32(0xb94)),kHz49);
    work.putf(0x60c,rear);work.putf(0x518,rear);
}

std::uint32_t network_tail_state_55a930(Bytes network_object){
    network_object.check(0x60,4);return network_object.u32(0x60);
}
void network_tail_forward_46c390(Bytes first,Bytes second,const PcNetworkTailServices& services){
    if(services.callback)services.callback(services.user,first,second);else outrun::driving::service_hole("network_tail_forward_46c390","services.callback");
}
std::uint8_t rank_provider_gateway_45a2b0(std::uint8_t player_id,const PcRankProviderServices& services){
    if(services.game_mode_78026c!=0x10u){services.ranks_7df118.check(player_id,1);return services.ranks_7df118.u8(player_id);}
    if(services.game_variant_780258!=3u&&services.game_variant_780258!=4u)return 0u;
    const std::uint32_t value=services.callback?services.callback(services.user,player_id):(outrun::driving::service_hole("rank_provider_gateway_45a2b0","services.callback"),0u);
    return static_cast<std::uint8_t>(value&0xffu);
}

namespace {
// Exact mxLength 0x40F140: three FSUBs, then (dz*dz+dx*dx)+dy*dy; FSQRT.
X87 r050_distance_ext(CourseProbe a,CourseProbe b){
    const X87 dx=X87(a.x)-b.x,dy=X87(a.y)-b.y,dz=X87(a.z)-b.z;
    return x87_sqrt((dz*dz+dx*dx)+dy*dy);
}
X87 r050_inner_ext(CourseProbe a,CourseProbe b){return x87_inner3(a,b);}
CourseProbe r050_sub_vec_exact(CourseProbe a,CourseProbe b){return sub_vec_x87(a,b);}
float r050_clamp_distance(X87 extended){
    const float spilled=x87_float(extended);
    // First compare uses the unspilled x87 value, second compare uses the f32 spill.
    if(!std::isnan(extended.v) && X87(10.0f)>extended)return 10.0f;
    if(!std::isnan(spilled) && spilled>50.0f)return 50.0f;
    return spilled;
}
float r050_clamp_inner(X87 extended){
    const float spilled=x87_float(extended);
    if(!std::isnan(extended.v) && X87(0.0f)>extended)return 0.0f;
    if(!std::isnan(spilled) && spilled>1.0f)return 1.0f;
    return spilled;
}
float r050_cos_spill(float angle){return x87_float(x87_cos(X87(angle)));}
float r050_base_target(std::int32_t brake,float best,std::int32_t accel){
    constexpr float inv255=0.003921568859368563f;
    // FILD brake; FMUL inv; FSUBR 1; FMUL best; FIMUL accel; FMUL inv; FSTP.
    return x87_float(((X87(1.0f)-X87(brake)*inv255)*best*X87(accel))*inv255);
}
bool r050_ramp_candidate_above(std::int32_t count_plus_one,float target,float current){
    constexpr float inv180=0.0055555556900799274f;
    // FILD n; FMUL target; FMUL inv180; FLD current; FXCH; FCOMIP (unordered: false).
    const X87 probe=X87(count_plus_one)*target*inv180;
    return probe>X87(current);
}
}

void check_slipstream_4a4d20(Bytes event,PcCheckSlipStreamInputs& in){
    event.check(0,0xe7c);
    event.put32(0x0c,event.u32(0x0c)&0xfffbffffu);

    float best=0.0f;
    std::int32_t best_id=-1;
    const CourseProbe self_pos{event.f32(0x14),event.f32(0x18),event.f32(0x1c)};

    for(std::size_t index=0;index<in.candidates.size();++index){
        auto& c=in.candidates[index];
        if((c.open_state&3u)!=2u)continue;
        if((c.flags&0x40u)!=0u)continue;
        if(in.network_session_active && c.network_state!=0u)continue;

        const X87 dist_ext=r050_distance_ext(self_pos,c.position);
        const float clamped_distance=r050_clamp_distance(dist_ext);
        const float distance_factor=subf(1.0f,mulf(subf(clamped_distance,10.0f),0.02500000037252903f));

        const float scaled_speed=mulf(c.speed,216.720001220703125f);
        float speed_factor=1.0f;
        if(!std::isnan(scaled_speed)&&300.0f>scaled_speed)
            speed_factor=mulf(scaled_speed,0.0033333334140479565f);

        CourseProbe self_dir{event.f32(0x20),event.f32(0x24),event.f32(0x28)};
        CourseProbe candidate_dir=c.direction;
        (void)unit_vector_ext(self_dir);
        (void)unit_vector_ext(candidate_dir);
        const float alignment=r050_clamp_inner(r050_inner_ext(candidate_dir,self_dir));

        CourseProbe to_candidate=r050_sub_vec_exact(c.position,self_pos);
        (void)unit_vector_ext(to_candidate);
        const float facing=x87_float(r050_inner_ext(candidate_dir,to_candidate));
        constexpr float a5=0.087266467511653900146484375f;
        constexpr float a20=0.3490658700466156005859375f;
        const float cos5=r050_cos_spill(a5),cos20=r050_cos_spill(a20);
        float angle_factor=divf(subf(facing,cos20),subf(cos5,cos20));
        angle_factor=clamp_pc_0_1(angle_factor);

        float score=mulf(angle_factor,alignment);
        score=mulf(score,speed_factor);
        score=mulf(score,distance_factor);
        if(pc_comiss_above(score,best)){
            best=score;
            best_id=static_cast<std::int32_t>(index+9u);
            if(pc_comiss_above(score,0.180000007152557373046875f)){
                const std::int32_t angle=static_cast<std::int32_t>(event.i16(0x162));
                const std::int32_t abs_angle=angle<0?-angle:angle;
                if(abs_angle<1500){
                    c.cooldown=180;
                    event.put32(0x0c,event.u32(0x0c)|0x00040000u);
                    event.put32(0xe78,event.u32(0xe78)+1u);
                }
            }
        }
        if(c.cooldown>0)c.cooldown=static_cast<std::int16_t>(c.cooldown-1);
    }

    event.putf(0xe68,best);
    event.puti(0xe74,best_id);

    const float base=r050_base_target(event.i32(0x38),best,event.i32(0x34));
    const float angle=x87_i32_angle(static_cast<std::int32_t>(event.i16(0xd46)));
    const float target=x87_cos_mul_spill(angle,base);
    std::uint8_t count=event.u8(0xe70);
    const std::int32_t next=static_cast<std::int32_t>(count)+1;
    const float current=event.f32(0xe6c);
    constexpr float inv180=0.0055555556900799274f;
    if(r050_ramp_candidate_above(next,target,current)){
        if(count<180u)++count;
        event.put8(0xe70,count);
        const float q=mulf(i32f(static_cast<std::int32_t>(count)),target);
        event.putf(0xe6c,mulf(q,inv180));
    }else if(count>0u){
        const float step=divf(current,i32f(static_cast<std::int32_t>(count)));
        --count;event.put8(0xe70,count);event.putf(0xe6c,subf(current,step));
    }else{
        event.put8(0xe70,0u);event.putf(0xe6c,0.0f);
    }
}
}

namespace outrun::driving {
void pas_pl_car_ctrl_475720(Bytes event,const std::array<Bytes,4>& road_records,
                            const PcPasPlCarInputs& in,const PcPasPlCarServices& svc){
    event.check(0,0x25cu);
    for(auto record:road_records)record.check(0,0xf0u);
    auto call=[&](std::uint32_t pc,std::uint32_t wheel=0xffffffffu){
        if(svc.callback)svc.callback(svc.user,pc,wheel);else outrun::driving::service_hole("pas_pl_car_ctrl_475720","svc.callback");
    };

    // PC helper at 0x4755C0 is a direct child boundary in this parent oracle.
    call(0x4755c0u);
    event.put32(0x04,event.u32(0x04)|0x00800000u);
    call(0x4872f0u);

    const std::uint8_t scene=in.petty_auto_scene_list;
    const bool petty_path=scene!=0u&&(scene<=7u||scene>14u);
    if(petty_path){
        call(0x4a2650u); // CalcDispMatrix-aligned child
        call(0x409f30u); // mxPushUnitMatrix
        call(0x40a0d0u); // mxGetMatrix(event+0xF0)
        call(0x40a010u); // mxPopMatrix
        call(0x4a2ee0u); // SetOldParamBuffer
        call(0x4a4710u); // display-steering/record child in this PC build
    }else{
        call(0x4a8330u); // GamePlCar_Ctrl
    }

    call(0x409f90u); // mxPushLoadMatrix(event+0xB0)
    for(std::uint32_t i=0;i<4u;++i){
        auto record=road_records[i];
        const auto& w=in.wheel[i];
        call(0x40a7d0u,i); // mxCalcPoint
        CourseProbe point=w.transformed_point;
        const CourseProbe original=point;

        call(0x43eb60u,i); // GetYPositionProg(0x400,...)
        point.y=w.road_y;
        record.put32(0x10,w.road_polygon);
        if(w.road_result==1u)continue;

        event.put32(0x24cu+i*4u,w.road_result);
        record.putf(0x3c,point.y);
        record.putf(0xe0,0.0f);
        record.putf(0xe4,1.0f);
        record.put16(0xee,0u);

        call(0x46bbf0u,i); // GetTirePosition
        float corrected=addf(w.tire_position.y,point.y);
        corrected=subf(corrected,original.y);
        const std::size_t yoff=0x134u+std::size_t(i)*12u;
        corrected=addf(corrected,event.f32(yoff));
        event.putf(yoff,corrected);

        call(0x43d390u,i); // GetPolNormal(record+0x10,event+0x5C,record+0x70)
        record.putf(0x70,w.road_normal.x);
        record.putf(0x74,w.road_normal.y);
        record.putf(0x78,w.road_normal.z);
    }
    call(0x40a010u); // mxPopMatrix
}
}

namespace outrun::driving {
namespace {
inline PcEventSlot* event_slot(PcEventControlState& state,std::uint32_t id){
    return id<PcEventSlotCount?&state.slots[id]:nullptr;
}
inline const PcEventSlot* event_slot(const PcEventControlState& state,std::uint32_t id){
    return id<PcEventSlotCount?&state.slots[id]:nullptr;
}
inline void event_invoke(const PcEventServices& services,const PcEventSlot& slot,std::uint32_t callback){
    if(callback!=0u&&services.invoke)services.invoke(services.user,callback,slot.work_token,slot.event_id);
}
inline void event_close_transition(PcEventSlot& slot){
    if((slot.flags&0x01u)!=0u)slot.flags=static_cast<std::uint8_t>(slot.flags&~0x01u);
    else if((slot.flags&0x02u)!=0u){
        slot.flags=static_cast<std::uint8_t>(slot.flags&~0x02u);
        slot.flags=static_cast<std::uint8_t>(slot.flags|0x04u);
    }
}
}

void event_control_43fab0(PcEventControlState& state,const PcEventServices& services){
    for(std::uint32_t id=0;id<PcEventSlotCount;++id){
        state.current_slot=id;
        auto& slot=state.slots[id];
        if((slot.flags&0x04u)!=0u){
            event_invoke(services,slot,slot.dest_callback);
            slot.flags=static_cast<std::uint8_t>(slot.flags&~0x04u);
        }
        if((slot.flags&0x01u)!=0u){
            event_invoke(services,slot,slot.init_callback);
            slot.flags=static_cast<std::uint8_t>((slot.flags&~0x01u)|0x02u);
        }
        if((slot.flags&0x18u)==0u&&(slot.flags&0x02u)!=0u)
            event_invoke(services,slot,slot.ctrl_callback);
    }
}

void event_setup_440110(
    PcEventControlState& state,std::uint32_t event_id,std::uint32_t function_id,
    const std::array<PcEventInitDescriptor,PcEventSlotCount>& descriptors,
    const std::array<PcEventFunctionDescriptor,PcEventFunctionTableCount>& functions){
    auto* slot=event_slot(state,event_id);
    if(!slot||function_id>=PcEventFunctionTableCount)return;
    // The protected prefix produces CL from the slot's current flags before the
    // visible AND/OR sequence at 0x44011A. Keep the same low-byte semantics.
    slot->flags=static_cast<std::uint8_t>((slot->flags&0xe7u)|0x01u);
    const auto& f=functions[function_id];
    slot->init_callback=f.init_callback;
    slot->ctrl_callback=f.ctrl_callback;
    slot->disp_callback=f.disp_callback;
    slot->shadow_callback=f.shadow_callback;
    slot->dest_callback=f.dest_callback;
    slot->display_scene=descriptors[event_id].display_scene;
}

void event_open_static_440180(
    PcEventControlState& state,std::uint32_t event_id,std::uint32_t function_id,
    const std::array<PcEventInitDescriptor,PcEventSlotCount>& descriptors,
    const std::array<PcEventFunctionDescriptor,PcEventFunctionTableCount>& functions,
    const PcEventServices& services){
    auto* slot=event_slot(state,event_id);
    if(!slot)return;
    event_setup_440110(state,event_id,function_id,descriptors,functions);
    slot=event_slot(state,event_id);
    if((slot->flags&0x01u)!=0u){
        event_invoke(services,*slot,slot->init_callback);
        slot->flags=static_cast<std::uint8_t>((slot->flags&~0x01u)|0x02u);
    }
}

void event_open_440180(PcEventControlState& state,std::uint32_t event_id,
                       std::uint32_t function_id,const PcEventServices& services){
    auto* slot=event_slot(state,event_id);
    if(!slot)return;
    if(services.setup)services.setup(services.user,state,event_id,function_id);else outrun::driving::service_hole("event_open_440180","services.setup");
    // Provider 0x440110 may rewrite the slot, so resolve it again after setup.
    slot=event_slot(state,event_id);
    if(!slot)return;
    if((slot->flags&0x01u)!=0u){
        event_invoke(services,*slot,slot->init_callback);
        slot->flags=static_cast<std::uint8_t>((slot->flags&~0x01u)|0x02u);
    }
}

void event_close_4401d0(PcEventControlState& state,std::uint32_t event_id){
    if(auto* slot=event_slot(state,event_id))event_close_transition(*slot);
}

void event_close_immediate_440200(PcEventControlState& state,std::uint32_t event_id,
                                  const PcEventServices& services){
    auto* slot=event_slot(state,event_id);if(!slot)return;
    if((slot->flags&0x01u)!=0u){
        slot->flags=static_cast<std::uint8_t>(slot->flags&~0x01u);
    }else if((slot->flags&0x02u)!=0u){
        slot->flags=0u;
        event_invoke(services,*slot,slot->dest_callback);
    }
}

void event_close_all_440240(PcEventControlState& state){
    for(auto& slot:state.slots){
        if(slot.close_guard!=0u)continue;
        event_close_transition(slot);
    }
}

void event_close_serial_440330(PcEventControlState& state,std::uint32_t first,std::uint32_t count){
    const std::uint64_t end=static_cast<std::uint64_t>(first)+count;
    for(std::uint64_t id=first;id<end&&id<PcEventSlotCount;++id)
        event_close_transition(state.slots[static_cast<std::size_t>(id)]);
}

bool check_event_destructing_440370(const PcEventControlState& state,std::uint32_t event_id){
    const auto* slot=event_slot(state,event_id);return slot&&(slot->flags&0x04u)!=0u;
}

std::uint32_t get_event_id_440b30(const std::array<std::uint32_t,PcEventSlotCount>& defaults,
                                 std::uint32_t token){
    for(std::uint32_t id=0;id<PcEventSlotCount;++id)if(defaults[id]==token)return id;
    return static_cast<std::uint32_t>(PcEventSlotCount);
}

std::uint32_t get_now_event_id_440b80(const PcEventControlState& state){
    if(state.current_slot>=PcEventSlotCount)return PcEventInvalidSlot;
    return state.slots[state.current_slot].event_id;
}
void change_now_event_ctrl_func_440b90(PcEventControlState& state,std::uint32_t token){
    if(state.current_slot<PcEventSlotCount)state.slots[state.current_slot].ctrl_callback=token;
}
void change_now_event_shadow_func_440ba0(PcEventControlState& state,std::uint32_t token){
    if(state.current_slot<PcEventSlotCount)state.slots[state.current_slot].shadow_callback=token;
}
void change_ctrl_func_440bb0(PcEventControlState& state,std::uint32_t id,std::uint32_t token){
    if(auto* slot=event_slot(state,id))slot->ctrl_callback=token;
}
void change_disp_scene_440bd0(PcEventControlState& state,std::uint32_t id,std::uint32_t scene){
    if(auto* slot=event_slot(state,id))slot->display_scene=scene;
}

void init_event_control_440bf0(
    PcEventControlState& state,
    const std::array<PcEventInitDescriptor,PcEventSlotCount>& descriptors,
    const std::array<PcEventFunctionDescriptor,PcEventFunctionTableCount>& functions){
    for(std::uint32_t id=0;id<PcEventSlotCount;++id){
        const auto& d=descriptors[id];
        auto& q=state.slots[id];
        q=PcEventSlot{};
        q.descriptor_token=d.descriptor_token;
        q.event_id=id;
        q.work_token=d.work_token;
        q.display_scene=d.display_scene;
        q.aux28=d.aux28;
        q.close_guard=d.startup;
        q.function_id=d.function_id;
        if(d.startup!=0u){
            if(d.function_id<PcEventFunctionTableCount){
                const auto& f=functions[d.function_id];
                q.init_callback=f.init_callback;
                q.ctrl_callback=f.ctrl_callback;
                q.disp_callback=f.disp_callback;
                q.shadow_callback=f.shadow_callback;
                q.dest_callback=f.dest_callback;
            }
            q.flags=0x01u;
            q.display_scene=d.display_scene;
        }
    }
}

void event_suspend_440a10(PcEventControlState& state,std::uint32_t first,std::uint32_t count){
    const std::uint64_t end=static_cast<std::uint64_t>(first)+count;
    for(std::uint64_t id=first;id<end&&id<PcEventSlotCount;++id)
        state.slots[static_cast<std::size_t>(id)].flags=static_cast<std::uint8_t>(state.slots[static_cast<std::size_t>(id)].flags|0x10u);
}
void event_resume_440a30(PcEventControlState& state,std::uint32_t first,std::uint32_t count){
    const std::uint64_t end=static_cast<std::uint64_t>(first)+count;
    for(std::uint64_t id=first;id<end&&id<PcEventSlotCount;++id)
        state.slots[static_cast<std::size_t>(id)].flags=static_cast<std::uint8_t>(state.slots[static_cast<std::size_t>(id)].flags&0xefu);
}
std::uint32_t check_event_suspend_440a50(const PcEventControlState& state,std::uint32_t event_id){
    const auto* q=event_slot(state,event_id);
    return q?std::uint32_t(q->flags&0x10u):0u;
}

namespace {
inline void event_boundary(const PcEventBoundaryCallback cb,void* user,std::uint32_t entry,std::uint32_t arg){
    if(cb)cb(user,entry,arg);
}
inline bool event_heap_type_a(std::uint32_t type){return type==2u||type==3u;}
inline bool event_heap_type_b(std::uint32_t type){
    return type==1u||type==3u||type==4u||type==5u||type==7u||type==9u;
}
}

void set_ev_pause_flag_440930(PcEventControlState& state,const PcEventPauseServices& services){
    event_boundary(services.boundary,services.user,0x43f9d0u,1u);
    event_boundary(services.boundary,services.user,0x429810u,1u);
    event_boundary(services.boundary,services.user,0x449040u,1u);
    // 44094A: record +28 (aux28, the descriptor column +08 "pausable"), not the work +08.
    for(auto& q:state.slots)if(q.aux28!=0u)q.flags=static_cast<std::uint8_t>(q.flags|0x08u);
    if(std::getenv("OR2_PAUSE_DEBUG")){for(std::uint32_t i=0;i<PcEventSlotCount;++i){const auto& q=state.slots[i];if(q.flags&3u)std::fprintf(stderr,"pause ev %x flags %x aux28 %x work %x ctrl %x\n",i,q.flags,q.aux28,q.work_token,q.ctrl_callback);}}
    state.pause_depth=state.pause_depth+1u;
}

void clr_ev_pause_flag_4409c0(PcEventControlState& state,const PcEventPauseServices& services){
    state.pause_depth=state.pause_depth-1u;
    if(static_cast<std::int32_t>(state.pause_depth)>0)return;
    state.pause_depth=0u;
    event_boundary(services.boundary,services.user,0x429810u,0u);
    event_boundary(services.boundary,services.user,0x43f9d0u,0u);
    event_boundary(services.boundary,services.user,0x449040u,0u);
    for(auto& q:state.slots)q.flags=static_cast<std::uint8_t>(q.flags&0xf7u);
}

std::uint32_t malloc_now_event_work_440a60(PcEventControlState& state,PcEventWorkHandle& handle,
                                           std::uint32_t requested_size,std::uint32_t heap_type,
                                           const PcEventWorkServices& services){
    if(state.current_slot>=PcEventSlotCount)return 0u;
    auto& slot=state.slots[state.current_slot];
    event_boundary(services.boundary,services.user,0x440d90u,slot.descriptor_token);
    event_boundary(services.boundary,services.user,0x440d10u,event_heap_type_a(heap_type)?1u:0u);
    event_boundary(services.boundary,services.user,0x440d50u,event_heap_type_b(heap_type)?1u:0u);
    const std::uint32_t total=requested_size+8u;
    const std::uint32_t base=services.allocate?services.allocate(services.user,total):(outrun::driving::service_hole("malloc_now_event_work_440a60","services.allocate"),0u);
    if(base==0u)return 0u;
    handle.wrapper_token=base+requested_size;
    handle.base_token=base;
    handle.heap_type=heap_type;
    handle.live=true;
    slot.aux24=handle.wrapper_token;
    event_boundary(services.boundary,services.user,0x440d30u,0u);
    event_boundary(services.boundary,services.user,0x440d70u,0u);
    event_boundary(services.boundary,services.user,0x49a650u,handle.wrapper_token);
    event_boundary(services.boundary,services.user,0x49a650u,handle.wrapper_token);
    event_boundary(services.boundary,services.user,0x440d90u,0u);
    slot.work_token=base;
    return base;
}

void free_event_work_handle_440cd0(PcEventControlState& state,PcEventWorkHandle& handle,
                                   const PcEventWorkServices& services){
    if(state.current_slot>=PcEventSlotCount)return;
    auto& slot=state.slots[state.current_slot];
    if(slot.aux24==0u||!handle.live)return;
    event_boundary(services.boundary,services.user,0x440d10u,event_heap_type_a(handle.heap_type)?1u:0u);
    if(services.release)services.release(services.user,handle.base_token);else outrun::driving::service_hole("free_event_work_handle_440cd0","services.release");
    event_boundary(services.boundary,services.user,0x440d30u,0u);
    slot.aux24=0u;
    handle=PcEventWorkHandle{};
}

void free_now_event_work_440b20(PcEventControlState& state,PcEventWorkHandle& handle,
                                const PcEventWorkServices& services){
    free_event_work_handle_440cd0(state,handle,services);
}

void push_alloc_state_a_440d10(PcAllocatorStateStacks& state,std::uint32_t value){
    if(state.depth_a>=4u)return;
    state.stack_a[state.depth_a]=value;
    state.depth_a=state.depth_a+1u;
}
std::uint32_t pop_alloc_state_a_440d30(PcAllocatorStateStacks& state){
    const std::uint32_t value=state.stack_a[state.depth_a<state.stack_a.size()?state.depth_a:0u];
    state.depth_a=state.depth_a-1u;
    return value;
}
void push_alloc_state_b_440d50(PcAllocatorStateStacks& state,std::uint32_t value){
    if(state.depth_b>=4u)return;
    state.stack_b[state.depth_b]=value;
    state.depth_b=state.depth_b+1u;
}
std::uint32_t pop_alloc_state_b_440d70(PcAllocatorStateStacks& state){
    const std::uint32_t value=state.stack_b[state.depth_b<state.stack_b.size()?state.depth_b:0u];
    state.depth_b=state.depth_b-1u;
    return value;
}
void hmm_handle_move_440cc0(std::uint32_t& destination,std::uint32_t source){destination=source;}
void masked_service_440ca0(std::uint32_t bit_index,void* user,PcMaskedServiceCallback callback){
    const std::uint32_t mask=std::uint32_t(1u<<(bit_index&31u));
    if(callback)callback(user,mask,0u,0x199u);
}

std::uint32_t object_take_token_440dc0(Bytes object){
    object.check(0x21c,4);const auto value=object.u32(0x21c);object.put32(0x21c,0xffffffffu);return value;
}
void object_set_token_440de0(Bytes object,std::uint32_t value){object.check(0x21c,4);object.put32(0x21c,value);}
void object_set_field4_440df0(Bytes object,std::uint32_t value){object.check(0x04,4);object.put32(0x04,value);}
void object_set_field8_440e00(Bytes object,std::uint32_t value){object.check(0x08,4);object.put32(0x08,value);}
namespace {
void r056_child_void(const PcObjectChildServices& s,std::uint32_t pc){if(s.call_void)s.call_void(s.user,pc,0x51cu);else outrun::driving::service_hole("r056_child_void","s.call_void");}
std::size_t r056_signed_stack_offset(std::uint32_t base,std::int32_t index){
    const auto off=std::int32_t(base)+index;if(off<0)throw std::out_of_range("r056 signed object offset");return std::size_t(off);
}
}
void object_push_byte_state_440e10(Bytes object,const PcObjectChildServices& services){
    object.check(0x220,1);auto depth=object.i8(0x220);if(depth>=0x20)return;
    const auto wrapped=std::uint8_t(std::uint8_t(depth)+1u);object.put8(0x220,wrapped);
    const auto index=std::int32_t(static_cast<std::int8_t>(wrapped));
    const auto src0=r056_signed_stack_offset(0x220u,index),dst0=r056_signed_stack_offset(0x221u,index);
    const auto src1=r056_signed_stack_offset(0x240u,index),dst1=r056_signed_stack_offset(0x241u,index);
    object.put8(dst0,object.u8(src0));object.put8(dst1,object.u8(src1));
    r056_child_void(services,0x446fc0u);
}
void object_pop_byte_state_440e60(Bytes object,const PcObjectChildServices& services){
    object.check(0x220,1);const auto depth=object.i8(0x220);if(depth<=0)return;
    const auto index=std::int32_t(depth);
    object.put8(r056_signed_stack_offset(0x221u,index),0u);
    object.put8(r056_signed_stack_offset(0x241u,index),0u);
    object.put8(0x220,std::uint8_t(std::uint8_t(depth)-1u));
    r056_child_void(services,0x4464f0u);
}
void object_child_call_a_440ea0(Bytes object,const PcObjectChildServices& services){object.check(0x51c,1);r056_child_void(services,0x446d90u);}
void object_child_call_b_440eb0(Bytes object,const PcObjectChildServices& services){object.check(0x51c,1);r056_child_void(services,0x4464b0u);}
void object_child_call_c_440ec0(Bytes object,const PcObjectChildServices& services){object.check(0x51c,1);r056_child_void(services,0x4464d0u);}
std::uint8_t object_child_conditional_440ed0(Bytes object,std::uint32_t arg0,std::uint32_t arg1,
                                            const PcObjectChildServices& services){
    object.check(0x218,4);object.check(0x51c,1);if(object.u32(0x218)!=2u)return 0u;
    return services.call_bool2?services.call_bool2(services.user,0x446f30u,0x51cu,arg0,arg1):(outrun::driving::service_hole("object_child_conditional_440ed0","services.call_bool2"),0u);
}

std::uint8_t transition_global_get_active_4c50c0(const PcFloatTransitionGlobals& globals){return globals.active;}
void transition_global_set_token_4c50e0(PcFloatTransitionGlobals& globals,std::uint32_t value){globals.token=value;}
void transition_global_set_primary_4c50f0(PcFloatTransitionGlobals& globals,float value){globals.primary=value;}
void transition_global_set_secondary_4c5100(PcFloatTransitionGlobals& globals,float value){globals.secondary=value;}
void transition_global_clear_active_4c5110(PcFloatTransitionGlobals& globals){globals.active=0u;}
void transition_global_set_active_4c50d0(PcFloatTransitionGlobals& globals){globals.active=1u;}

void object_transition_init_440ef0(Bytes object,PcFloatTransitionGlobals& globals){
    object.check(0x280,4);
    constexpr float one=1.0f;
    object.putf(0x268,one);object.putf(0x264,one);object.putf(0x274,one);object.putf(0x270,one);
    object.putf(0x26c,one);object.putf(0x278,one);
    object.put32(0x27c,0u);object.put32(0x280,0u);object.put8(0x262,0u);object.put8(0x261,0u);
    transition_global_set_primary_4c50f0(globals,one);transition_global_set_secondary_4c5100(globals,one);
    transition_global_clear_active_4c5110(globals);
}
void object_transition_config_440f70(Bytes object,std::uint8_t mode,std::uint32_t frames,std::uint32_t unused,
                                     PcFloatTransitionGlobals& globals){
    (void)unused;object.check(0x280,4);constexpr float one=1.0f;
    object.putf(0x268,one);object.putf(0x264,one);object.putf(0x274,one);object.putf(0x270,one);
    object.putf(0x26c,3.0f);object.putf(0x278,0.75f);object.put32(0x280,frames);object.put32(0x27c,0u);
    object.put8(0x261,mode);object.put8(0x262,0u);
    if(mode==0u){transition_global_set_primary_4c50f0(globals,3.0f);transition_global_set_secondary_4c5100(globals,0.75f);}
    else{transition_global_set_primary_4c50f0(globals,one);transition_global_set_secondary_4c5100(globals,one);}
}
void object_transition_update_441020(Bytes object,PcFloatTransitionGlobals& globals,
                                     const PcFloatTransitionServices& services){
    object.check(0x280,4);
    const float current_a=object.f32(0x264),target_a=object.f32(0x26c);
    const float current_b=object.f32(0x270),target_b=object.f32(0x278);
    if(current_a==target_a && current_b==target_b)return;
    const std::int32_t next=static_cast<std::int32_t>(object.u32(0x27c)+1u);
    const float ratio=static_cast<float>(next)/static_cast<float>(object.i32(0x280));
    const float inv=1.0f-ratio;
    const float new_a=object.f32(0x268)*inv+target_a*ratio;
    const float new_b=object.f32(0x274)*inv+target_b*ratio;
    object.putf(0x264,new_a);object.putf(0x270,new_b);object.puti(0x27c,next);
    transition_global_set_primary_4c50f0(globals,new_a);transition_global_set_secondary_4c5100(globals,new_b);
    if(new_a==1.0f)transition_global_clear_active_4c5110(globals);
    if(!(new_a>1.0f))return;
    if(transition_global_get_active_4c50c0(globals)!=0u)return;
    if(services.activate)services.activate(services.user);else outrun::driving::service_hole("object_transition_update_441020","services.activate");
}
void object_transition_latch_441130(Bytes object){
    object.check(0x280,4);
    if(object.u8(0x262)!=0u && object.u8(0x261)==0u)return;
    object.put32(0x268,object.u32(0x264));object.put32(0x274,object.u32(0x270));
    object.putf(0x26c,1.0f);object.putf(0x278,1.0f);object.put32(0x280,object.u32(0x27c));
    object.put32(0x27c,0u);object.put8(0x262,1u);object.put8(0x261,0u);
}

namespace {
void r058_void(const PcRuntimeControlServices& s,std::uint32_t pc,std::uint32_t h){if(s.call_void)s.call_void(s.user,pc,h);else outrun::driving::service_hole("r058_void","s.call_void");}
void r058_void_arg(const PcRuntimeControlServices& s,std::uint32_t pc,std::uint32_t h,std::uint32_t a){if(s.call_void_arg)s.call_void_arg(s.user,pc,h,a);else outrun::driving::service_hole("r058_void_arg","s.call_void_arg");}
std::uint8_t r058_u8(const PcRuntimeControlServices& s,std::uint32_t pc,std::uint32_t h){return s.call_u8?s.call_u8(s.user,pc,h):(outrun::driving::service_hole("r058_u8","s.call_u8"),0u);}
std::uint32_t r058_u32(const PcRuntimeControlServices& s,std::uint32_t pc,std::uint32_t h){return s.call_u32?s.call_u32(s.user,pc,h):(outrun::driving::service_hole("r058_u32","s.call_u32"),0u);}
std::uint8_t r058_pair(const PcRuntimeControlServices& s,std::uint32_t pc,std::uint32_t a,std::uint32_t b){return s.call_bool_pair?s.call_bool_pair(s.user,pc,a,b):(outrun::driving::service_hole("r058_pair","s.call_bool_pair"),0u);}
std::int8_t r058_i8(const PcRuntimeControlServices& s,std::uint32_t pc,std::uint32_t h){return s.call_i8?s.call_i8(s.user,pc,h):(outrun::driving::service_hole("r058_i8","s.call_i8"),0);}
}
void runtime_manager_reset_454200(Bytes manager,PcRuntimeControlGlobals& globals){
    manager.check(0x08,1);globals.reset_word_659930=0u;manager.put8(0x05,0u);manager.put8(0x07,0u);manager.put8(0x08,0u);
}
void runtime_set_gate_454220(PcRuntimeControlGlobals& globals,std::uint8_t value){globals.gate_d4=value;}
std::uint8_t runtime_get_gate_454230(const PcRuntimeControlGlobals& globals){return globals.gate_d4;}
std::uint8_t runtime_finish_454240(PcRuntimeControlGlobals& globals,const PcRuntimeControlServices& services){
    globals.started_d1=1u;return r058_u8(services,0x4946e0u,0x0083612cu);
}
void runtime_child_counter_dec_4464f0(Bytes child){child.check(0x408,4);const auto n=child.u32(0x408);if(n)child.put32(0x408,n-1u);}
std::uint32_t runtime_child_state_564c90(Bytes child){child.check(0x08,4);return child.u32(0x08);}
void runtime_shutdown_4411a0(PcRuntimeControlGlobals& globals,Bytes current_object,const PcRuntimeControlServices& services){
    if(globals.enabled_d2==0u||globals.blocked_bf!=0u)return;
    if(globals.current_ac!=0u){
        current_object.check(0x08,1);r058_void(services,0x453c40u,globals.current_ac);
        r058_void_arg(services,0x490530u,0x00830becu,0u);
        runtime_manager_reset_454200(current_object,globals);
        r058_void(services,0x4411e0u,globals.current_ac);
    }
    if(runtime_finish_454240(globals,services)==0u)globals.enabled_d2=0u;
}
void runtime_release_handle_441200(Bytes object,std::uint32_t handle,const PcRuntimeControlServices& services){
    if(handle==0u)return;
    object.check(0x924,4);
    r058_void(services,0x441210u,handle);
    r058_void_arg(services,0x441219u,handle,1u);
    const auto index=std::int32_t(r058_i8(services,0x01039ba0u,handle));
    if(index<=0)return;
    const auto a=std::int32_t(0x221)+index;
    if(a<0)throw std::out_of_range("r058 release index");
    object.put8(std::size_t(a),0u);
    const auto depth=std::int32_t(object.i8(0x220));
    const auto b=std::int32_t(0x241)+depth;
    if(b<0)throw std::out_of_range("r058 release depth");
    object.put8(std::size_t(b),0u);
    object.put8(0x220,std::uint8_t(object.u8(0x220)-1u));
    runtime_child_counter_dec_4464f0(object.sub(0x51c,object.size()-0x51c));
}
std::uint8_t runtime_ready_441260(Bytes object,const PcRuntimeControlGlobals& globals,const PcRuntimeControlServices& services){
    object.check(0x48c,1);
    const auto child=object.u32(0x488);
    if(child!=0u&&r058_u32(services,0x564c90u,child)==0x0eu)return 0u;
    if(globals.alternate_b0!=0u&&globals.alternate_b0!=globals.current_ac&&r058_pair(services,0x453ca0u,globals.alternate_b0,globals.current_ac)!=0u)return 1u;
    if(globals.current_ac!=0u&&r058_pair(services,0x453ca0u,globals.current_ac,globals.current_ac)!=0u)return 1u;
    if(globals.mode_836130==1u)return 1u;
    if(runtime_get_gate_454230(globals)!=0u)return 1u;
    return 0u;
}
std::uint32_t runtime_status_4412c0(Bytes object,const PcRuntimeControlServices& services){
    object.check(0x48c,1);const auto child=object.u32(0x488);if(child!=0u&&object.u8(0x48c)!=0u)return r058_u32(services,0x564c90u,child);return 0x53u;
}
std::uint8_t runtime_has_handle_4412f0(Bytes object){object.check(0x48c,1);return object.u8(0x48c);}
std::uint8_t runtime_close_if_status_441300(Bytes object,std::uint32_t requested,const PcRuntimeControlServices& services){
    object.check(0x48c,1);const auto current=runtime_status_4412c0(object,services);
    if(current!=requested&&requested!=0x53u)return 0u;
    if(object.u8(0x48c)!=0u){const auto child=object.u32(0x488);if(child!=0u){runtime_release_handle_441200(object,child,services);object.put32(0x488,0u);}object.put8(0x48c,0u);}return 1u;
}

namespace {
void r059_handle_void(const PcUiNotifyServices& s,std::uint32_t pc,std::uint32_t h){if(s.handle_void)s.handle_void(s.user,pc,h);else outrun::driving::service_hole("r059_handle_void","s.handle_void");}
std::int32_t r059_handle_i32(const PcUiNotifyServices& s,std::uint32_t pc,std::uint32_t h){return s.handle_i32?s.handle_i32(s.user,pc,h):(outrun::driving::service_hole("r059_handle_i32","s.handle_i32"),0);}
void r059_value_void(const PcUiNotifyServices& s,std::uint32_t pc,std::uint32_t v){if(s.value_void)s.value_void(s.user,pc,v);else outrun::driving::service_hole("r059_value_void","s.value_void");}
std::uint32_t r059_lookup(const PcUiNotifyServices& s,std::uint32_t pc,std::uint32_t id){return s.lookup?s.lookup(s.user,pc,id):(outrun::driving::service_hole("r059_lookup","s.lookup"),0u);}
void r059_draw(const PcUiNotifyServices& s,std::uint32_t pc,std::uint32_t fmt,std::uint32_t arg){if(s.draw)s.draw(s.user,pc,fmt,arg);else outrun::driving::service_hole("r059_draw","s.draw");}
}
void ui_resource_reset_465250(Bytes object,const PcUiNotifyServices& services){
    object.check(0x3c,4);const auto handle=object.u32(0x08);if(handle==0xffffffffu)return;
    if(object.u32(0x24)==0u)r059_handle_void(services,0x4285a0u,handle);
    object.put32(0x08,0xffffffffu);object.put32(0x2c,0u);object.put32(0x28,0u);
}
std::uint32_t ui_resource_ready_4652e0(Bytes object,const PcUiNotifyServices& services){
    object.check(0x24,4);if(object.i32(0x24)>0)return 0u;const auto handle=object.u32(0x08);
    if(handle!=0xffffffffu&&r059_handle_i32(services,0x428880u,handle)==1)return 0u;
    return 1u;
}
void ui_text_set_xy_42cc00(PcUiNotifyGlobals& globals,std::uint32_t x,std::uint32_t y){
    globals.x=static_cast<std::int16_t>(x);globals.y=static_cast<std::int16_t>(y);globals.base_x=globals.x;globals.base_y=globals.y;
}
void ui_text_set_color_42cca0(PcUiNotifyGlobals& globals,std::uint32_t color){globals.color=color;}
void ui_text_set_mode_42ccb0(PcUiNotifyGlobals& globals,std::uint32_t mode){globals.mode=mode;}
std::uint32_t ui_table_lookup_465eb0(Bytes table,std::uint32_t index){table.check(std::size_t(index)*4u,4);return table.u32(std::size_t(index)*4u);}
void ui_notify_441370(Bytes object,PcUiNotifyGlobals& globals,const PcUiNotifyServices& services){
    object.check(0xd94,1);if(object.u32(0xc50)==0xffffffffu)return;
    auto resource=object.sub(0xc48,object.size()-0xc48);if(ui_resource_ready_4652e0(resource,services)==0u)return;
    r059_value_void(services,0x42ca60u,9u);ui_text_set_mode_42ccb0(globals,7u);ui_text_set_color_42cca0(globals,0xffffffffu);
    ui_text_set_xy_42cc00(globals,globals.source_x,globals.source_y);
    const std::uint32_t id=globals.alternate?0x476u:0x474u;const auto fmt=r059_lookup(services,0x465eb0u,id);
    r059_draw(services,0x42cdd0u,fmt,globals.alternate?0x00830c20u:id);
}
void ui_set_active_4413f0(Bytes object,std::uint8_t active,const PcUiNotifyServices& services){
    object.check(0xd94,1);object.put8(0xd94,active);if(active==0u)ui_resource_reset_465250(object.sub(0xcf4,object.size()-0xcf4),services);
}
void compact_u32_list_441410(Bytes object,std::int32_t index){
    object.check(0x80,4);const std::int32_t count=object.i32(0x80)-1;object.puti(0x80,count);if(index>=count)return;
    if(index<0)throw std::out_of_range("r059 negative compact index");
    for(std::int32_t i=index;i<count;++i){object.check(std::size_t(i+1)*4u,4);object.put32(std::size_t(i)*4u,object.u32(std::size_t(i+1)*4u));}
}

namespace {
std::uint32_t r060_factory(std::uint32_t bytes,std::uint32_t ctor,const PcFactoryServices& services){
    const std::uint32_t object=services.allocate?services.allocate(services.user,0x5802cfu,bytes):(outrun::driving::service_hole("r060_factory","services.allocate"),0u);
    if(object==0u)return 0u;
    return services.construct?services.construct(services.user,ctor,object):(outrun::driving::service_hole("r060_factory","services.construct"),object);
}
}
std::uint32_t object_factory_441450(const PcFactoryServices& s){return r060_factory(0x758u,0x4c5120u,s);}
std::uint32_t object_factory_4414b0(const PcFactoryServices& s){return r060_factory(0x154u,0x4c5550u,s);}
std::uint32_t object_factory_441510(const PcFactoryServices& s){return r060_factory(0x154u,0x4c5980u,s);}
std::uint32_t object_factory_441570(const PcFactoryServices& s){return r060_factory(0x14cu,0x4c5cd0u,s);}
std::uint32_t object_factory_4415d0(const PcFactoryServices& s){return r060_factory(0x14cu,0x4c5ef0u,s);}
std::uint32_t object_factory_441630(const PcFactoryServices& s){return r060_factory(0x1f4u,0x4c60b0u,s);}
std::uint32_t object_factory_441690(const PcFactoryServices& s){return r060_factory(0x2200u,0x4c6300u,s);}
std::uint32_t object_factory_4416f0(const PcFactoryServices& s){return r060_factory(0x31c8u,0x4c6fd0u,s);}
std::uint32_t object_factory_441750(const PcFactoryServices& s){return r060_factory(0x1f10u,0x4c8030u,s);}
std::uint32_t object_factory_4417b0(const PcFactoryServices& s){return r060_factory(0x1674u,0x4c8de0u,s);}
std::uint32_t object_factory_441810(const PcFactoryServices& s){return r060_factory(0x474u,0x4c9a90u,s);}
std::uint32_t object_factory_441870(const PcFactoryServices& s){return r060_factory(0x27f9cu,0x4cb300u,s);}
std::uint32_t object_factory_4418d0(const PcFactoryServices& s){return r060_factory(0x27f9cu,0x4ccfc0u,s);}

// r061 factory continuation.  Semantics and service boundaries are identical
// to r060; constructor bodies remain separate reconstruction targets.
std::uint32_t object_factory_441930(const PcFactoryServices& s){return r060_factory(0x279c4u,0x4ce170u,s);}
std::uint32_t object_factory_441990(const PcFactoryServices& s){return r060_factory(0x27a68u,0x4cebf0u,s);}
std::uint32_t object_factory_4419f0(const PcFactoryServices& s){return r060_factory(0x27ba8u,0x4d05e0u,s);}
std::uint32_t object_factory_441a50(const PcFactoryServices& s){return r060_factory(0x27ce8u,0x4d2710u,s);}
std::uint32_t object_factory_441ab0(const PcFactoryServices& s){return r060_factory(0x28fcu,0x4d4230u,s);}
std::uint32_t object_factory_441b10(const PcFactoryServices& s){return r060_factory(0x1d90u,0x4d7140u,s);}
std::uint32_t object_factory_441b70(const PcFactoryServices& s){return r060_factory(0x5ecu,0x4d9010u,s);}
std::uint32_t object_factory_441bd0(const PcFactoryServices& s){return r060_factory(0x27b08u,0x4da2f0u,s);}
std::uint32_t object_factory_441c30(const PcFactoryServices& s){return r060_factory(0x275d8u,0x4db680u,s);}
std::uint32_t object_factory_441c90(const PcFactoryServices& s){return r060_factory(0x1324u,0x4926f0u,s);}
std::uint32_t object_factory_441cf0(const PcFactoryServices& s){return r060_factory(0x12b0u,0x48c490u,s);}
std::uint32_t object_factory_441d50(const PcFactoryServices& s){return r060_factory(0xd8u,0x4dc080u,s);}
std::uint32_t object_factory_441db0(const PcFactoryServices& s){return r060_factory(0x12e8u,0x4dcc00u,s);}
std::uint32_t object_factory_441e10(const PcFactoryServices& s){return r060_factory(0x1f4u,0x4dd410u,s);}
std::uint32_t object_factory_441e70(const PcFactoryServices& s){return r060_factory(0x11a4u,0x4dd5c0u,s);}
std::uint32_t object_factory_441ed0(const PcFactoryServices& s){return r060_factory(0x12ecu,0x4de6f0u,s);}
std::uint32_t object_factory_441f30(const PcFactoryServices& s){return r060_factory(0x17cu,0x4de9d0u,s);}
std::uint32_t object_factory_441f90(const PcFactoryServices& s){return r060_factory(0x17b8u,0x4dec60u,s);}
std::uint32_t object_factory_441ff0(const PcFactoryServices& s){return r060_factory(0x271cu,0x4df820u,s);}
std::uint32_t object_factory_442050(const PcFactoryServices& s){return r060_factory(0x1b68u,0x4dfaf0u,s);}
std::uint32_t object_factory_4420b0(const PcFactoryServices& s){return r060_factory(0x1444u,0x4e0680u,s);}
std::uint32_t object_factory_442110(const PcFactoryServices& s){return r060_factory(0xc768u,0x4e1890u,s);}
std::uint32_t object_factory_442170(const PcFactoryServices& s){return r060_factory(0x448cu,0x4e3ac0u,s);}
std::uint32_t object_factory_4421d0(const PcFactoryServices& s){return r060_factory(0x449cu,0x4e4c60u,s);}
std::uint32_t object_factory_442230(const PcFactoryServices& s){return r060_factory(0x20b0u,0x4e5590u,s);}
std::uint32_t object_factory_442290(const PcFactoryServices& s){return r060_factory(0x1768u,0x4e5720u,s);}
std::uint32_t object_factory_4422f0(const PcFactoryServices& s){return r060_factory(0x1decu,0x4e5b20u,s);}
std::uint32_t object_factory_442350(const PcFactoryServices& s){return r060_factory(0x1b48u,0x4e60c0u,s);}
std::uint32_t object_factory_4423b0(const PcFactoryServices& s){return r060_factory(0x26f8u,0x4e6220u,s);}
std::uint32_t object_factory_442410(const PcFactoryServices& s){return r060_factory(0x22d4u,0x493020u,s);}
std::uint32_t object_factory_442470(const PcFactoryServices& s){return r060_factory(0x130e8u,0x4e7600u,s);}
std::uint32_t object_factory_4424d0(const PcFactoryServices& s){return r060_factory(0x88a8u,0x4e7a90u,s);}
std::uint32_t object_factory_442530(const PcFactoryServices& s){return r060_factory(0x132cu,0x4e7d60u,s);}
std::uint32_t object_factory_442590(const PcFactoryServices& s){return r060_factory(0x674u,0x4e8130u,s);}
std::uint32_t object_factory_4425f0(const PcFactoryServices& s){return r060_factory(0x4488u,0x4e9160u,s);}
std::uint32_t object_factory_442650(const PcFactoryServices& s){return r060_factory(0x2408u,0x4e9e50u,s);}
std::uint32_t object_factory_4426b0(const PcFactoryServices& s){return r060_factory(0x1c4cu,0x4eb100u,s);}
std::uint32_t object_factory_442710(const PcFactoryServices& s){return r060_factory(0x1c10u,0x4302c0u,s);}
std::uint32_t object_factory_442770(const PcFactoryServices& s){return r060_factory(0x1c10u,0x4afe30u,s);}
std::uint32_t object_factory_4427d0(const PcFactoryServices& s){return r060_factory(0x1c14u,0x4eb2e0u,s);}
std::uint32_t object_factory_442830(const PcFactoryServices& s){return r060_factory(0x1c10u,0x4eb5b0u,s);}
std::uint32_t object_factory_442890(const PcFactoryServices& s){return r060_factory(0x1c10u,0x4eb8d0u,s);}
std::uint32_t object_factory_4428f0(const PcFactoryServices& s){return r060_factory(0x1c10u,0x4eb9e0u,s);}
std::uint32_t object_factory_442950(const PcFactoryServices& s){return r060_factory(0x1c10u,0x4ebd10u,s);}

namespace {
void r062_this(const PcObjectInitServices& s,std::uint32_t pc,Bytes object,std::size_t off){
    object.check(off,1);if(s.call_this)s.call_this(s.user,pc,object,off);else outrun::driving::service_hole("r062_this","s.call_this");
}
void r062_this_arg(const PcObjectInitServices& s,std::uint32_t pc,Bytes object,std::size_t off,std::uint32_t arg0){
    object.check(off,1);if(s.call_this_arg)s.call_this_arg(s.user,pc,object,off,arg0);else outrun::driving::service_hole("r062_this_arg","s.call_this_arg");
}
void r062_array(const PcObjectInitServices& s,Bytes object,std::size_t off,std::uint32_t elem,std::uint32_t count,
                std::uint32_t ctor,std::uint32_t dtor){
    const std::size_t bytes=std::size_t(elem)*std::size_t(count);object.check(off,bytes);
    if(s.construct_array)s.construct_array(s.user,0x5816bdu,object,off,elem,count,ctor,dtor);else outrun::driving::service_hole("r062_array","s.construct_array");
}
}

std::uintptr_t object_ctor_4429b0(Bytes object,std::uintptr_t self_token,const PcObjectInitServices& services){
    object.check(0,0x297cu);
    r062_this(services,0x48f480u,object,0x0000u);
    object.put32(0x0000u,0x0059dabcu);
    r062_this(services,0x4ed950u,object,0x01a8u);
    r062_this(services,0x48c490u,object,0x01e4u);
    r062_array(services,object,0x1494u,0x8cu,4u,0x570ac0u,0x49a650u);
    r062_this_arg(services,0x490f10u,object,0x16c4u,0u);
    r062_array(services,object,0x174cu,0x48cu,4u,0x48e590u,0x48e630u);
    return self_token;
}

std::uintptr_t object_ctor_442a60(Bytes object,std::uintptr_t self_token,const PcObjectInitServices& services){
    object.check(0,0x74u);
    r062_this(services,0x48f480u,object,0x0000u);
    object.put32(0x0000u,0x0059dad4u);
    r062_this(services,0x4ed950u,object,0x0034u);
    r062_this(services,0x48c490u,object,0x0070u);
    return self_token;
}

std::uintptr_t object_block_init_442ac0(Bytes object,std::uintptr_t self_token,const PcObjectInitServices& services){
    object.check(0,0x68cu);
    object.put8(0x0000u,0u);
    for(std::size_t row=0;row<32u;++row){
        const std::size_t base=0x0008u+row*0x20u;
        for(std::size_t k=0;k<8u;++k)object.put32(base+k*4u,0xffffffffu);
    }
    object.put32(0x0408u,0u);
    r062_array(services,object,0x040cu,0xa0u,4u,0x465160u,0x465250u);
    return self_token;
}

std::uintptr_t object_state_ctor_442b20(Bytes object,std::uintptr_t self_token,std::uint8_t mode_global_6319a1,
                                       const PcObjectInitServices& services){
    object.check(0,0x0da4u);
    object.put32(0x0484u,0u);
    object.put32(0x0518u,0u);
    r062_this(services,0x442ac0u,object,0x051cu);
    r062_this(services,0x465160u,object,0x0ba8u);
    r062_this(services,0x465160u,object,0x0c48u);
    r062_this(services,0x465160u,object,0x0cf4u);
    object.put32(0x0218u,0u);
    object.put32(0x0214u,0u);
    object.put32(0x0004u,5u);
    object.put32(0x0008u,0u);
    object.put32(0x020cu,0u);
    for(std::size_t k=0;k<0x80u;++k)object.put32(0x000cu+k*4u,0x53u);
    object.put8(0x0220u,0u);
    object.put32(0x0000u,mode_global_6319a1?0u:3u);
    for(std::size_t k=0;k<0x20u;++k){object.put8(0x0221u+k,0u);object.put8(0x0241u+k,0u);}
    object.put32(0x021cu,0xffffffffu);
    object.putf(0x0d98u,0.0f);object.putf(0x0d9cu,0.0f);object.putf(0x0da0u,0.0f);
    return self_token;
}

float timer_value_4af500(float timer_global_842110){return timer_global_842110;}

std::uint8_t object_state_reset_442c20(Bytes object,std::uint8_t mode_global_6319a1,float scale_62812c,
                                      const PcObjectResetServices& services){
    object.check(0,0x0da4u);
    object.put32(0x0214u,0u);
    object.put32(0x0218u,1u);
    if(mode_global_6319a1!=0u){
        object.put32(0x0000u,0u);
        object.putf(0x0d98u,0.0f);
        object.putf(0x0d9cu,0.0f);
        object.putf(0x0da0u,0.0f);
    }else{
        object.put32(0x0000u,3u);
        const float timer=services.call_float?services.call_float(services.user,0x4af500u):(outrun::driving::service_hole("object_state_reset_442c20","services.call_float"),0.0f);
        const float scaled=timer*scale_62812c;
        object.putf(0x0d9cu,scaled);
        object.putf(0x0d98u,scaled);
        object.putf(0x0da0u,scale_62812c);
    }
    object.put32(0x021cu,0xffffffffu);
    object.put8(0x0220u,0u);
    for(std::size_t k=0;k<0x20u;++k){object.put8(0x0221u+k,0u);object.put8(0x0241u+k,0u);}
    return 1u;
}

namespace {
std::uint32_t r063_virtual(const PcObjectSelectorServices& s,std::uint32_t handle){
    return s.call_virtual_u32?s.call_virtual_u32(s.user,handle,0x08u):(outrun::driving::service_hole("r063_virtual","s.call_virtual_u32"),0u);
}
void r063_void(const PcObjectSelectorServices& s,std::uint32_t pc,std::uint32_t handle){
    if(s.call_void_handle)s.call_void_handle(s.user,pc,handle);else outrun::driving::service_hole("r063_void","s.call_void_handle");
}
}

std::uint32_t object_select_primary_442cb0(Bytes object,std::int32_t& selected_kind,
                                           PcObjectSelectorGlobals& globals,
                                           const PcObjectSelectorServices& services){
    object.check(0x0518u,4u);
    selected_kind=-1;
    std::uint32_t result=0u;
    if(object.i32(0x0518u)>0){
        selected_kind=3;
        result=r063_virtual(services,object.u32(0x0498u));
    }else if(object.u8(0x0494u)!=0u){
        selected_kind=2;
        result=r063_virtual(services,object.u32(0x0490u));
    }else if(object.u8(0x048cu)!=0u){
        selected_kind=1;
        result=r063_virtual(services,object.u32(0x0488u));
    }else if(object.i32(0x0484u)>0){
        selected_kind=0;
        const std::size_t index=static_cast<std::size_t>(object.u32(0x0484u));
        result=r063_virtual(services,object.u32(0x0280u+index*4u));
    }
    if(object.u8(0x0494u)==0u&&globals.manager_handle!=0u&&globals.manager_flag5!=0u&&globals.manager_flag8!=0u){
        r063_void(services,0x4ee930u,globals.manager_handle);
        globals.manager_flag8=0u;
        return 5u;
    }
    return result;
}

std::uint32_t object_select_secondary_442d70(Bytes object,std::int32_t& selected_kind,
                                             const PcObjectSelectorServices& services){
    object.check(0x0518u,4u);
    selected_kind=-1;
    if(object.i32(0x0518u)>0){
        selected_kind=3;
        return r063_virtual(services,object.u32(0x0498u));
    }
    if(object.u8(0x0494u)!=0u){
        const auto handle=object.u32(0x0490u);
        if(handle!=0u){selected_kind=2;return r063_virtual(services,handle);}
    }
    if(object.u8(0x048cu)!=0u){
        const auto handle=object.u32(0x0488u);
        if(handle!=0u){selected_kind=1;return r063_virtual(services,handle);}
    }
    return 0u;
}

std::uint32_t frontend_gate_choice_class_4def50(std::int32_t item_count,
                                                 std::int32_t cursor){
    // The PC compares signed EAX/ECX here. Its real item count is small;
    // widen the arithmetic so malformed host data cannot overflow count+4.
    const auto count=static_cast<std::int64_t>(item_count);
    const auto selected=static_cast<std::int64_t>(cursor);
    if(selected==0)return 0u;
    if(count>0&&selected>1&&selected<count+2)return 1u;
    if(count<=0){
        if(selected==1)return 2u;
        if(selected==2)return 3u;
        return 0u;
    }
    if(count>8){
        if(selected==count+2)return 3u;
        if(selected==count+3)return 4u;
        return 0u;
    }
    if(selected==count+2)return 2u;
    if(selected==count+3)return 3u;
    if(selected==count+4)return 4u;
    return 0u;
}

void frontend_gate_list_initialize_4ded80(Bytes child,std::uint32_t profile_count,
                                           PcFrontendGateList& list){
    child.check(0x3cu,4u);
    // PC 0x4DED80 appends item 0, optionally item 1, one entry per saved
    // profile, then items 2, 8 and 3. Profile storage is a separate service.
    // The retail profile table is bounded to eight slots.
    if(profile_count>8u)throw std::out_of_range("frontend profile count");
    list={};
    list.count=profile_count==0u?4u:profile_count+5u;
    for(std::uint32_t i=0u;i<list.count;++i)list.enabled[i]=1u;
    if(profile_count==0u)list.enabled[3u]=0u; // 0x4DEE84
    else if(profile_count>=8u)list.enabled[profile_count+2u]=0u; // 0x4DEE5B
    child.put32(0x34u,profile_count);
    child.put32(0x38u,0u);
    child.put32(0x3cu,list.count);
}

bool frontend_gate_list_move_4ed250_4ed2a0(Bytes child,
                                             const PcFrontendGateList& list,
                                             bool forward){
    child.check(0x38u,4u);
    if(list.count==0u||list.count>list.enabled.size()||
       child.u32(0x3cu)!=list.count)return false;
    const auto original=child.u32(0x38u);
    if(original>=list.count)return false;
    auto cursor=original;
    // Source loops over linked nodes and skips disabled entries. A full
    // revolution is enough to reject a corrupt/all-disabled native list.
    for(std::uint32_t step=0u;step<list.count;++step){
        cursor=forward?(cursor+1u)%list.count:
            (cursor==0u?list.count-1u:cursor-1u);
        if(list.enabled[cursor]!=0u){child.put32(0x38u,cursor);return cursor!=original;}
    }
    return false;
}


std::uint32_t frontend_gate_control_4df120(Bytes child,
                                           std::uint32_t input_action,
                                           bool animation_ready,
                                           bool owner_busy,
                                           std::uint32_t menu_variant,
                                           const PcFrontendGateList* list){
    child.check(0x17b4u,4u);
    // 0x48CBB0 checks the animation object's +0x12A4 byte. The host owns
    // that timeline; an action latch by itself is not an animation finish.
    if(!animation_ready)return 0u;
    if(child.u8(0x17b0u)!=0u){
        const auto latched=child.u32(0x17b4u);
        if(latched!=0u)return latched;
        const auto choice=frontend_gate_choice_class_4def50(
            child.i32(0x34u),child.i32(0x38u));
        if(choice==1u){child.put32(0x17b4u,2u);return 2u;}
        if(choice==2u||choice==3u){
            child.put32(0x04u,30u);child.put32(0x17b4u,4u);return 4u;
        }
        if(choice==4u){
            child.put32(0x04u,29u);child.put32(0x17b4u,4u);return 4u;
        }
        return 0u;
    }
    // 0x4DF252 checks the owner's active overlay before dispatching the
    // virtual input selector. 0x48F5F0 maps 0=confirm, 1=cancel, 2/4=cursor.
    if(owner_busy)return 0u;
    if(input_action==0u){
        const auto choice=frontend_gate_choice_class_4def50(
            child.i32(0x34u),child.i32(0x38u));
        if(choice==0u){
            if(menu_variant==0u){child.put32(0x04u,1u);child.put32(0x17b4u,1u);}
            else if(menu_variant==1u){child.put32(0x04u,6u);child.put32(0x17b4u,2u);}
            else if(menu_variant==2u)child.put32(0x17b4u,2u);
        }
        // Other classes depend on the PC's profile/manager readiness globals
        // at 0x830C30 and 0x7D68CA; do not guess them here.
    }else if(input_action==1u){
        if(menu_variant==0u)child.put32(0x17b4u,2u);
        else if(menu_variant==1u){child.put32(0x04u,6u);child.put32(0x17b4u,2u);}
        else if(menu_variant==2u)child.put32(0x17b4u,2u);
    }else if((input_action==2u||input_action==4u)&&list){
        (void)frontend_gate_list_move_4ed250_4ed2a0(
            child,*list,input_action==4u);
    }
    return 0u;
}

namespace {
void r064_virtual_void(const PcObjectStateServices& s,std::uint32_t h){if(s.call_virtual_void)s.call_virtual_void(s.user,h,0x0cu);else outrun::driving::service_hole("r064_virtual_void","s.call_virtual_void");}
void r064_embedded_void(const PcObjectStateServices& s,Bytes object,std::size_t off){if(s.call_embedded_void)s.call_embedded_void(s.user,0x446a50u,object,off);else outrun::driving::service_hole("r064_embedded_void","s.call_embedded_void");}
std::uint32_t r064_u32(const PcObjectStateServices& s,std::uint32_t h){return s.call_u32?s.call_u32(s.user,0x564c90u,h):(outrun::driving::service_hole("r064_u32","s.call_u32"),0u);}
}

void object_dispatch_state_442e00(Bytes object,const PcObjectStateServices& services){
    object.check(0x051cu,1u);
    const auto indexed=object.i32(0x0484u);
    if(indexed>0){
        const auto off=0x0280u+std::size_t(static_cast<std::uint32_t>(indexed))*4u;
        object.check(off,4u);
        r064_virtual_void(services,object.u32(off));
    }
    if(object.i32(0x0518u)>0)r064_virtual_void(services,object.u32(0x0498u));
    if(object.u8(0x0494u)!=0u){const auto h=object.u32(0x0490u);if(h!=0u)r064_virtual_void(services,h);}
    if(object.u8(0x048cu)!=0u){const auto h=object.u32(0x0488u);if(h!=0u)r064_virtual_void(services,h);}
    r064_embedded_void(services,object,0x051cu);
}

void object_refresh_state_table_442e70(Bytes object,const PcObjectStateServices& services){
    object.check(0x000cu,0x200u);
    for(std::size_t k=0;k<0x80u;++k)object.put32(0x000cu+k*4u,0x53u);
    const auto count=object.i32(0x0484u);
    if(count<=0)return;
    if(count>0x80)throw std::out_of_range("r064 state-table count");
    for(std::int32_t i=0;i<count;++i){
        const auto handle_off=0x0284u+std::size_t(i)*4u;
        const auto state=r064_u32(services,object.u32(handle_off));
        if(state!=4u&&state!=10u&&state!=21u)object.put32(0x000cu+std::size_t(i)*4u,state);
    }
}

std::uint32_t object_query_previous_state_442ec0(Bytes object,const PcObjectStateServices& services){
    object.check(0x0484u,4u);
    const auto count=object.i32(0x0484u);
    if(count<2)return 0x53u;
    if(count>0x80)throw std::out_of_range("r064 previous-state count");
    const auto off=0x027cu+std::size_t(static_cast<std::uint32_t>(count))*4u;
    return r064_u32(services,object.u32(off));
}

void object_store_depth_pair_442f20(Bytes object,std::uint8_t first,std::uint8_t second){
    object.check(0x0220u,1u);
    const auto depth=std::int32_t(object.i8(0x0220u));
    object.put8(std::size_t(std::int64_t(0x0221u)+depth),first);
    object.put8(std::size_t(std::int64_t(0x0241u)+depth),second);
}

std::uint32_t object_query_last_state_443040(Bytes object,const PcObjectStateServices& services){
    object.check(0x0484u,4u);
    const auto count=object.i32(0x0484u);
    if(count<=0)return 0x53u;
    if(count>0x80)throw std::out_of_range("r064 last-state count");
    const auto off=0x0280u+std::size_t(static_cast<std::uint32_t>(count))*4u;
    return r064_u32(services,object.u32(off));
}

std::uint8_t object_has_active_state_443060(Bytes object){
    object.check(0x0518u,4u);
    if(object.i32(0x0518u)>0)return 1u;
    if(object.u8(0x048cu)!=0u||object.u8(0x0494u)!=0u)return 1u;
    if(object.u32(0x0218u)==2u&&object.i32(0x0484u)>0)return 1u;
    return 0u;
}

const std::array<PcObjectStatePairEntry,PcObjectStatePairCount>& pc_object_state_pair_table_r065(){
    static constexpr std::array<PcObjectStatePairEntry,PcObjectStatePairCount> table={{
        {0u,0u,0u},{1u,1u,1u},{2u,1u,1u},{3u,1u,1u},{4u,0u,0u},{5u,1u,1u},
        {6u,1u,1u},{7u,1u,1u},{8u,1u,1u},{10u,0u,0u},{11u,1u,0u},{12u,1u,0u},
        {13u,1u,0u},{14u,0u,0u},{15u,0u,0u},{16u,0u,0u},{17u,0u,0u},{19u,0u,0u},
        {20u,0u,0u},{21u,0u,0u},{22u,0u,0u},{23u,0u,0u},{24u,0u,0u},{25u,0u,0u},
        {26u,0u,0u},{29u,0u,0u},{30u,0u,0u},{31u,0u,0u},{32u,0u,0u},{34u,0u,0u},
        {36u,0u,0u},{57u,0u,0u},{37u,0u,0u},{38u,0u,0u},{39u,0u,0u},{40u,0u,0u},
        {41u,0u,0u},{42u,0u,0u},{43u,1u,1u},{44u,0u,0u},{46u,0u,0u},{47u,0u,0u},
        {54u,0u,0u},{55u,1u,1u},{56u,1u,1u},{58u,1u,1u},{59u,1u,1u},{60u,1u,1u}
    }};
    return table;
}

void object_store_state_pair_442f50(Bytes object,std::uint32_t key,const PcObjectStatePairTable& table){
    object.check(0x0260u,1u);
    const auto depth=std::int32_t(object.i8(0x0220u));
    std::uint8_t first=1u,second=1u;
    if(key!=0x53u){
        for(std::size_t i=0;i<table.count;++i){
            if(table.entries==nullptr)throw std::invalid_argument("r065 null state-pair table");
            const auto& e=table.entries[i];
            if(e.key==key){first=e.first;second=e.second;break;}
        }
    }
    object.put8(std::size_t(std::int64_t(0x0221u)+depth),first);
    object.put8(std::size_t(std::int64_t(0x0241u)+depth),second);
}

namespace {
std::uint32_t r065_u32(const PcObjectStateUpdateServices& s,std::uint32_t h){
    return s.call_u32?s.call_u32(s.user,0x564c90u,h):(outrun::driving::service_hole("r065_u32","s.call_u32"),0u);
}
void r065_embedded_u32(const PcObjectStateUpdateServices& s,Bytes object,std::uint32_t value){
    if(s.call_embedded_u32)s.call_embedded_u32(s.user,0x446ea0u,object,0x051cu,value);else outrun::driving::service_hole("r065_embedded_u32","s.call_embedded_u32");
}
}

void object_update_state_442fd0(Bytes object,const PcObjectStatePairTable& table,
                                const PcObjectStateUpdateServices& services){
    object.check(0x051cu,1u);
    std::uint32_t state=0x53u;
    bool selected=false;
    if(object.i32(0x0518u)>0){
        state=r065_u32(services,object.u32(0x0498u));
        selected=true;
    }else{
        if(object.u8(0x0494u)!=0u){
            const auto h=object.u32(0x0490u);
            if(h!=0u){state=r065_u32(services,h);selected=true;}
        }
        if(!selected){
            if(object.u8(0x048cu)!=0u){
                state=r065_u32(services,object.u32(0x0488u));
                selected=true;
            }else{
                const auto count=object.i32(0x0484u);
                if(count>0){
                    if(count>0x80)throw std::out_of_range("r065 indexed state count");
                    const auto off=0x0280u+std::size_t(static_cast<std::uint32_t>(count))*4u;
                    state=r065_u32(services,object.u32(off));
                    selected=true;
                }
            }
        }
    }
    (void)selected;
    r065_embedded_u32(services,object,state);
    object_store_state_pair_442f50(object,state,table);
}

void object_release_flagged_handles_4430b0(Bytes object,const PcRuntimeControlServices& services){
    object.check(0x0495u,1u);
    if(object.u8(0x0494u)!=0u){
        const auto h=object.u32(0x0490u);
        if(h!=0u){runtime_release_handle_441200(object,h,services);object.put32(0x0490u,0u);}
        object.put8(0x0494u,0u);
    }
    if(object.u8(0x048cu)!=0u){
        const auto h=object.u32(0x0488u);
        if(h!=0u){runtime_release_handle_441200(object,h,services);object.put32(0x0488u,0u);}
        object.put8(0x048cu,0u);
    }
}

namespace {
void r066_open_void(const PcObjectUiOpenServices& s,std::uint32_t site,std::uint32_t pc,Bytes object,std::size_t off){
    if(s.call_void)s.call_void(s.user,site,pc,object,off);else outrun::driving::service_hole("r066_open_void","s.call_void");
}
void r066_open_config(const PcObjectUiOpenServices& s,std::uint32_t site,Bytes object,std::size_t off,
                      const std::array<std::uint32_t,11>& args){
    if(s.call_config)s.call_config(s.user,site,0x465860u,object,off,args);else outrun::driving::service_hole("r066_open_config","s.call_config");
}
constexpr std::array<std::uint8_t,61> R066EventClass={{
    0,1,2,3,4,5,3,3,3,17,6,7,7,7,17,17,17,17,17,17,17,8,17,17,8,17,17,17,17,17,17,
    17,17,17,17,17,9,10,11,12,17,17,17,13,14,17,15,3,17,16,16,16,16,16,17,5,17,16,5,5,5
}};
}

void object_ui_state_update_443110(Bytes object,PcUiNotifyGlobals& globals,
                                   const PcUiNotifyServices& ui_services,
                                   const PcObjectUiOpenServices& open_services){
    object.check(0x0cf1u,1u);
    auto first=object.sub(0x0ba8u,object.size()-0x0ba8u);
    auto second=object.sub(0x0c48u,object.size()-0x0c48u);
    if(object.u8(0x0cf0u)==0u){
        ui_resource_reset_465250(first,ui_services);
        ui_resource_reset_465250(second,ui_services);
        return;
    }
    if(object.u32(0x0ce8u)!=0xffffffffu&&object.u8(0x0cf1u)==0u){
        ui_resource_reset_465250(first,ui_services);
        const std::array<std::uint32_t,11> a={{object.u32(0x0cecu),20u,0u,6u,3u,0u,0u,0x3f800000u,0x3f800000u,0xbf800000u,0u}};
        r066_open_config(open_services,0x443168u,object,0x0ba8u,a);
        r066_open_void(open_services,0x44316fu,0x465970u,object,0x0ba8u);
        object.put8(0x0cf1u,1u);
    }
    r066_open_void(open_services,0x443184u,0x4659f0u,object,0x0ba8u);
    r066_open_void(open_services,0x443191u,0x4659f0u,object,0x0c48u);
    ui_notify_441370(object,globals,ui_services);
    if(ui_resource_ready_4652e0(first,ui_services)==0u)return;
    if(object.u8(0x0cf1u)!=0u){
        ui_resource_reset_465250(second,ui_services);
        ui_resource_reset_465250(first,ui_services);
        const std::array<std::uint32_t,11> a={{object.u32(0x0ce8u),0u,20u,6u,3u,0u,0u,0x3f800000u,0x3f800000u,0x3f800000u,0u}};
        r066_open_config(open_services,0x4431e6u,object,0x0ba8u,a);
        r066_open_void(open_services,0x4431edu,0x465970u,object,0x0ba8u);
        object.put32(0x0cecu,object.u32(0x0ce8u));
        object.put32(0x0ce8u,0xffffffffu);
        object.put8(0x0cf1u,0u);
        return;
    }
    ui_resource_reset_465250(first,ui_services);
    const std::array<std::uint32_t,11> a={{object.u32(0x0cecu),25u,150u,6u,0u,0u,0u,0x3f800000u,0x3f800000u,0x3f800000u,0u}};
    r066_open_config(open_services,0x443243u,object,0x0ba8u,a);
    r066_open_void(open_services,0x44324au,0x465970u,object,0x0ba8u);
    if(object.u32(0x0c50u)!=0xffffffffu)return;
    const std::array<std::uint32_t,11> b={{0x004400d3u,0u,15u,6u,5u,0u,0u,0x3f800000u,0x3f800000u,0x3f800000u,0u}};
    r066_open_config(open_services,0x44327cu,object,0x0c48u,b);
    r066_open_void(open_services,0x443286u,0x465970u,object,0x0c48u);
}

std::uint32_t object_event_dispatch_4432b0(Bytes object,std::uint32_t state,
                                           const PcObjectStateServices& services){
    object.check(0x020cu,4u);
    for(;;){
        if(state>60u)return 0xffffffffu;
        switch(R066EventClass[state]){
            case 1:return 0x00440051u;
            case 2:return 0x00440058u;
            case 3:return 0x00440050u;
            case 4:return 0x0044004fu;
            case 5:return 0x00440054u;
            case 6:return 0x00440053u;
            case 7:return 0x00440052u;
            case 8:return 0x0044004au;
            case 9:return 0x00440040u;
            case 10:return (object.u32(0x020cu)&3u)!=0u?0x00440048u:0x00440049u;
            case 11:return (object.u32(0x020cu)&3u)!=0u?0x0044004bu:0x0044004cu;
            case 12:return (object.u32(0x020cu)&3u)!=0u?0x0044005au:0x0044005bu;
            case 13:return 0x00440059u;
            case 14:return 0x0044004eu;
            case 15:state=object_query_previous_state_442ec0(object,services);break;
            case 16:return 0x00440056u;
            default:return 0xffffffffu;
        }
    }
}

namespace {
void r067_embedded_void(const PcObjectEventDispatchServices& s,Bytes object){
    if(s.call_embedded_void)s.call_embedded_void(s.user,0x446fc0u,object,0x051cu);else outrun::driving::service_hole("r067_embedded_void","s.call_embedded_void");
}
void r067_embedded_u32(const PcObjectEventDispatchServices& s,Bytes object,std::uint32_t key){
    if(s.call_embedded_u32)s.call_embedded_u32(s.user,0x446ea0u,object,0x051cu,key);else outrun::driving::service_hole("r067_embedded_u32","s.call_embedded_u32");
}
}

std::uint32_t object_dispatch_callback_443420(Bytes object,std::uint32_t key,
                                               const PcObjectStatePairTable& pair_table,
                                               const PcObjectEventCallbackTable& callback_table,
                                               const PcObjectEventDispatchServices& services){
    if(callback_table.entries==nullptr||callback_table.count<PcObjectEventCallbackCount)
        throw std::invalid_argument("r067 callback table must expose 83 entries");
    std::size_t found=PcObjectEventCallbackCount;
    for(std::size_t i=0;i<PcObjectEventCallbackCount;++i){
        if(callback_table.entries[i].key==key){found=i;break;}
    }
    if(found==PcObjectEventCallbackCount)return 0u;
    object.check(0x051cu,1u);
    const auto depth=std::int32_t(object.i8(0x0220u));
    if(depth<0x20){
        const auto next=depth+1;
        object.put8(0x0220u,std::uint8_t(std::int8_t(next)));
        const auto first_src=std::size_t(std::int64_t(0x0220u)+next);
        const auto first_dst=std::size_t(std::int64_t(0x0221u)+next);
        const auto second_src=std::size_t(std::int64_t(0x0240u)+next);
        const auto second_dst=std::size_t(std::int64_t(0x0241u)+next);
        object.put8(first_dst,object.u8(first_src));
        object.put8(second_dst,object.u8(second_src));
        r067_embedded_void(services,object);
    }
    r067_embedded_u32(services,object,key);
    object_store_state_pair_442f50(object,key,pair_table);
    return services.call_callback?services.call_callback(services.user,callback_table.entries[found].callback_token,found):(outrun::driving::service_hole("pc_common_control.cpp:3779","services.call_callback"),0u);
}

std::uint32_t object_factory_4434c0(const PcFactoryServices& s){return r060_factory(0x2980u,0x4429b0u,s);}
std::uint32_t object_factory_443520(const PcFactoryServices& s){return r060_factory(0x1320u,0x442a60u,s);}

namespace {
void r068_this(const PcObjectDestroyServices& s,std::uint32_t pc,Bytes object,std::size_t off){
    if(s.destroy_this)s.destroy_this(s.user,pc,object,off);else outrun::driving::service_hole("r068_this","s.destroy_this");
}
void r068_vector(const PcObjectDestroyServices& s,Bytes object,std::size_t off,std::uint32_t elem,
                 std::uint32_t count,std::uint32_t dtor){
    if(s.destroy_vector)s.destroy_vector(s.user,0x58165du,object,off,elem,count,dtor);else outrun::driving::service_hole("r068_vector","s.destroy_vector");
}
void r068_free(const PcObjectDestroyServices& s,std::uint32_t token){
    if(s.release)s.release(s.user,0x5801a7u,token);else outrun::driving::service_hole("r068_free","s.release");
}
}

void object_destroy_body_4435a0(Bytes object,const PcObjectDestroyServices& services){
    object.check(0x174cu,0x48cu*4u);
    r068_vector(services,object,0x174cu,0x48cu,4u,0x48e630u);
    r068_vector(services,object,0x1494u,0x8cu,4u,0x49a650u);
    r068_this(services,0x48c520u,object,0x1e4u);
    r068_this(services,0x4edab0u,object,0x1a8u);
    r068_this(services,0x48f4d0u,object,0u);
}

std::uint32_t object_destroy_443580(Bytes object,std::uint32_t object_token,std::uint8_t flags,
                                    const PcObjectDestroyServices& services){
    object_destroy_body_4435a0(object,services);
    if((flags&1u)!=0u)r068_free(services,object_token);
    return object_token;
}

void object_destroy_body_443660(Bytes object,const PcObjectDestroyServices& services){
    object.check(0x70u,1u);
    r068_this(services,0x48c520u,object,0x70u);
    r068_this(services,0x4edab0u,object,0x34u);
    r068_this(services,0x48f4d0u,object,0u);
}

std::uint32_t object_destroy_443640(Bytes object,std::uint32_t object_token,std::uint8_t flags,
                                    const PcObjectDestroyServices& services){
    object_destroy_body_443660(object,services);
    if((flags&1u)!=0u)r068_free(services,object_token);
    return object_token;
}

namespace {
struct R069CallbackInit { std::uint8_t index; std::uint32_t callback; };
constexpr std::array<R069CallbackInit,61> R069CallbackInitTable={{
    {0,0x441450u},{1,0x4414b0u},{2,0x441510u},{4,0x441810u},
    {6,0x441570u},{7,0x4415d0u},{8,0x441630u},{10,0x4417b0u},
    {11,0x441690u},{12,0x4416f0u},{13,0x441750u},{14,0x441d50u},
    {15,0x441db0u},{16,0x441e10u},{17,0x4434c0u},{19,0x443520u},
    {20,0x441f30u},{21,0x442110u},{22,0x441f90u},{24,0x441e70u},
    {25,0x4423b0u},{26,0x442410u},{27,0x442470u},{28,0x4424d0u},
    {29,0x441ff0u},{30,0x442050u},{31,0x4420b0u},{32,0x442170u},
    {33,0x4421d0u},{34,0x442530u},{36,0x441870u},{37,0x441990u},
    {38,0x4419f0u},{39,0x441a50u},{40,0x442230u},{41,0x442290u},
    {42,0x4422f0u},{43,0x441ab0u},{44,0x441b10u},{45,0x441cf0u},
    {46,0x441c90u},{47,0x442350u},{48,0x4418d0u},{49,0x441b70u},
    {50,0x441bd0u},{51,0x441930u},{52,0x441c30u},{53,0x441bd0u},
    {57,0x4418d0u},{58,0x442590u},{59,0x4425f0u},{60,0x442650u},
    {61,0x441ed0u},{65,0x4426b0u},{66,0x442710u},{68,0x442770u},
    {70,0x4427d0u},{71,0x442830u},{72,0x442890u},{73,0x4428f0u},
    {74,0x442950u}
}};
}

std::uint32_t object_runtime_init_4436c0(
    Bytes object,
    std::array<PcObjectEventCallbackEntry,PcObjectEventCallbackCount>& callback_table,
    std::uint8_t global_mode_6319a1,
    const PcObjectRuntimeInitServices& services){
    object.check(0x0d94u,1u);
    for(const auto& e:R069CallbackInitTable){
        callback_table[e.index].key=e.index;
        callback_table[e.index].callback_token=e.callback;
    }

    object.put8(0x0220u,0u);
    for(std::size_t i=0;i<0x20u;++i){
        object.put8(0x0221u+i,0u);
        object.put8(0x0241u+i,0u);
    }
    if(services.init_embedded)
        services.init_embedded(services.user,0x4470f0u,object,0x051cu);else outrun::driving::service_hole("object_runtime_init_4436c0","services.init_embedded");

    object.put32(0x0214u,0u);
    object.put32(0x0004u,5u);
    object.put32(0x0008u,0u);
    for(std::size_t i=0;i<0x80u;++i)object.put32(0x000cu+i*4u,0x53u);

    object.put8(0x048cu,0u);
    object.put32(0x0488u,0u);
    object.put8(0x0494u,0u);
    object.put32(0x0490u,0u);
    object.put32(0x0000u,global_mode_6319a1!=0u?0u:3u);

    ui_resource_reset_465250(object.sub(0x0ba8u,object.size()-0x0ba8u),services.ui_services);
    object.put32(0x0ce8u,0xffffffffu);
    object.put32(0x0cecu,0xffffffffu);
    object.put8(0x0cf0u,0u);
    object.put8(0x0cf1u,0u);
    object.put32(0x0218u,0u);
    object.put8(0x0d94u,1u);
    return 0xffffff01u;
}

namespace {
std::uint32_t r070_handle_virtual(const PcObjectRuntimeTeardownServices& s,std::uint32_t slot,std::uint32_t handle,std::uint32_t arg){
    return s.handle_virtual?s.handle_virtual(s.user,slot,handle,arg):(outrun::driving::service_hole("r070_handle_virtual","s.handle_virtual"),0u);
}
void r070_release_one(Bytes object,std::uint32_t handle,const PcObjectRuntimeTeardownServices& services){
    (void)r070_handle_virtual(services,0x10u,handle,0u);
    (void)r070_handle_virtual(services,0x00u,handle,1u);
    const auto depth=std::int32_t(object.i8(0x0220u));
    if(depth>0){
        const auto d=std::size_t(depth);
        object.put8(0x0221u+d,0u);
        object.put8(0x0241u+d,0u);
        object.put8(0x0220u,std::uint8_t(std::int8_t(depth-1)));
        runtime_child_counter_dec_4464f0(object.sub(0x051cu,object.size()-0x051cu));
    }
}
std::uint32_t r070_global(const PcObjectRuntimeTeardownServices& s,std::uint32_t pc,std::uint32_t arg=0u,bool has_arg=false){
    return s.global_call?s.global_call(s.user,pc,arg,has_arg):(outrun::driving::service_hole("r070_global","s.global_call"),0u);
}
}

std::uint32_t object_runtime_teardown_443c30(Bytes object,const PcObjectRuntimeTeardownServices& services){
    object.check(0x0d34u,4u);
    if(services.reset_runtime)services.reset_runtime(services.user,0x447090u,object,0u);else outrun::driving::service_hole("object_runtime_teardown_443c30","services.reset_runtime");

    object.put8(0x0220u,0u);
    for(std::size_t i=0;i<0x20u;++i){object.put8(0x0221u+i,0u);object.put8(0x0241u+i,0u);}

    const auto first_count=object.i32(0x0484u);
    if(first_count>0){
        if(first_count>128)throw std::out_of_range("r070 first runtime handle count");
        for(std::int32_t i=0;i<first_count;++i){
            const auto h=object.u32(0x0284u+std::size_t(i)*4u);
            if(h!=0u)r070_release_one(object,h,services);
        }
    }
    object.put32(0x0484u,0u);

    const auto second_count=object.i32(0x0518u);
    if(second_count>0){
        if(second_count>127)throw std::out_of_range("r070 second runtime handle count");
        for(std::int32_t i=0;i<second_count;++i){
            const auto h=object.u32(0x0498u+std::size_t(i)*4u);
            if(h!=0u)r070_release_one(object,h,services);
        }
    }
    object.put32(0x0518u,0u);

    if(object.u8(0x0494u)!=0u){
        const auto h=object.u32(0x0490u);
        if(h!=0u){r070_release_one(object,h,services);object.put32(0x0490u,0u);}
        object.put8(0x0494u,0u);
    }
    if(object.u8(0x048cu)!=0u){
        const auto h=object.u32(0x0488u);
        if(h!=0u){r070_release_one(object,h,services);object.put32(0x0488u,0u);}
        object.put8(0x048cu,0u);
    }

    ui_resource_reset_465250(object.sub(0x0ba8u,object.size()-0x0ba8u),services.ui_services);
    ui_resource_reset_465250(object.sub(0x0cf4u,object.size()-0x0cf4u),services.ui_services);
    object.put32(0x0218u,4u);
    object.put32(0x0214u,0u);

    (void)r070_global(services,0x428600u);
    (void)r070_global(services,0x4299c0u,0x44u,true);
    (void)r070_global(services,0x4299c0u,0x47u,true);
    (void)r070_global(services,0x42dfb0u,0x44u,true);
    return r070_global(services,0x42dfb0u,0x47u,true);
}

namespace {
std::size_t r071_find_callback(const PcObjectEventCallbackTable& table,std::uint32_t key){
    if(table.entries==nullptr||table.count<PcObjectEventCallbackCount)
        throw std::invalid_argument("r071 callback table must expose 83 entries");
    for(std::size_t i=0;i<PcObjectEventCallbackCount;++i)if(table.entries[i].key==key)return i;
    return PcObjectEventCallbackCount;
}
void r071_embedded_void(const PcObjectEventDispatch443eb0Services& s,Bytes object){
    if(s.call_embedded_void)s.call_embedded_void(s.user,0x446fc0u,object,0x051cu);else outrun::driving::service_hole("r071_embedded_void","s.call_embedded_void");
}
void r071_embedded_u32(const PcObjectEventDispatch443eb0Services& s,Bytes object,std::uint32_t key){
    if(s.call_embedded_u32)s.call_embedded_u32(s.user,0x446ea0u,object,0x051cu,key);else outrun::driving::service_hole("r071_embedded_u32","s.call_embedded_u32");
}
void r071_global(const PcObjectEventDispatch443eb0Services& s,std::uint32_t pc,std::uint32_t arg=0u,bool has=false){
    if(s.call_global)s.call_global(s.user,pc,arg,has);else outrun::driving::service_hole("r071_global","s.call_global");
}
constexpr std::array<std::uint8_t,61> R071StateClass={{
    0,0,1,2,3,4,2,2,2,10,5,6,6,6,10,10,10,10,10,10,10,0,10,10,0,10,10,10,10,10,10,
    10,10,10,10,10,7,7,7,7,10,10,10,5,4,10,8,10,9,9,9,9,9,9,10,4,10,9,4,4,4
}};
}

std::uint32_t object_dispatch_callback_443eb0(
    Bytes object,std::uint32_t key,
    const PcObjectStatePairTable& pair_table,
    const PcObjectEventCallbackTable& callback_table,
    const PcObjectEventModeInputs& inputs,
    const PcObjectEventDispatch443eb0Services& services){
    const auto found=r071_find_callback(callback_table,key);
    if(found==PcObjectEventCallbackCount)return 0u;
    object.check(0x051cu,1u);
    const auto depth=std::int32_t(object.i8(0x0220u));
    if(depth<0x20){
        const auto next=depth+1;
        object.put8(0x0220u,std::uint8_t(std::int8_t(next)));
        const auto a0=std::size_t(std::int64_t(0x0220u)+next);
        const auto a1=std::size_t(std::int64_t(0x0221u)+next);
        const auto b0=std::size_t(std::int64_t(0x0240u)+next);
        const auto b1=std::size_t(std::int64_t(0x0241u)+next);
        object.put8(a1,object.u8(a0));
        object.put8(b1,object.u8(b0));
        r071_embedded_void(services,object);
    }
    r071_embedded_u32(services,object,key);
    object_store_state_pair_442f50(object,key,pair_table);
    if(object.u32(0x0218u)==3u && inputs.game_flag_780248==0u && object_has_active_state_443060(object)==0u){
        if(inputs.route_state_780258!=4u){
            r071_global(services,0x43f9e0u,1u,true);
            r071_global(services,0x440930u,0u,false);
        }
        if(inputs.game_mode_78026c==0x10u || inputs.fallback_state_78025c==0x10u)
            object.put32(0x021cu,0x12u);
    }
    return services.call_callback?services.call_callback(services.user,callback_table.entries[found].callback_token,found):(outrun::driving::service_hole("object_dispatch_callback_443eb0","services.call_callback"),0u);
}

void object_open_callback_443fa0(
    Bytes object,std::uint32_t key,
    const PcObjectStatePairTable& pair_table,
    const PcObjectEventCallbackTable& callback_table,
    const PcObjectEventModeInputs& inputs,
    const PcObjectEventDispatch443eb0Services& dispatch_services,
    const PcRuntimeControlServices& runtime_services,
    const PcObjectOpenCallbackServices& open_services){
    object.check(0x048cu,1u);
    object_release_flagged_handles_4430b0(object,runtime_services);
    const auto handle=object_dispatch_callback_443eb0(object,key,pair_table,callback_table,inputs,dispatch_services);
    if(handle==0u)return;
    object.put32(0x0488u,handle);
    const auto ready=open_services.handle_ready?open_services.handle_ready(open_services.user,handle,4u):(outrun::driving::service_hole("pc_common_control.cpp:4030","open_services.handle_ready"),0u);
    if(std::uint8_t(ready)==0u){
        runtime_release_handle_441200(object,handle,runtime_services);
        object.put32(0x0488u,0u);
        return;
    }
    object.put8(0x048cu,1u);
}

std::uint32_t object_state_code_443ff0(Bytes object,std::uint32_t state,
                                       Bytes related,const PcObjectStateServices& related_services){
    object.check(0x0d94u,1u);
    for(;;){
        if(object.u8(0x0d94u)==0u||state>60u)return 0xffffffffu;
        switch(R071StateClass[state]){
            case 0:return 0x00440094u;
            case 1:return 0x0044008eu;
            case 2:return 0x00440093u;
            case 3:return 0x00440092u;
            case 4:return 0x0044008bu;
            case 5:return 0x00440096u;
            case 6:return 0x00440095u;
            case 7:return 0x0044008fu;
            case 8:state=object_query_previous_state_442ec0(related,related_services);break;
            case 9:return 0x00440090u;
            default:return 0xffffffffu;
        }
    }
}


namespace {
std::uint32_t r072_get(const PcObjectListServices& s,std::uint32_t handle){
    return s.get_u32?s.get_u32(s.user,0x48f4e0u,handle):(outrun::driving::service_hole("r072_get","s.get_u32"),0u);
}
std::uint32_t r072_state(const PcObjectListServices& s,std::uint32_t handle){
    return s.get_u32?s.get_u32(s.user,0x564c90u,handle):(outrun::driving::service_hole("r072_state","s.get_u32"),0u);
}
std::uint32_t r072_virtual(const PcObjectListServices& s,std::uint32_t handle,std::uint32_t slot,
                           std::uint32_t argument=0u,bool has_argument=false){
    return s.call_virtual?s.call_virtual(s.user,handle,slot,argument,has_argument):(outrun::driving::service_hole("r072_virtual","s.call_virtual"),0u);
}
void r072_pop_history(Bytes object){
    const auto depth=std::int32_t(object.i8(0x0220u));
    if(depth<=0)return;
    const auto d=std::size_t(depth);
    object.put8(0x0221u+d,0u);
    object.put8(0x0241u+d,0u);
    object.put8(0x0220u,std::uint8_t(std::int8_t(depth-1)));
    runtime_child_counter_dec_4464f0(object.sub(0x051cu,object.size()-0x051cu));
}
void r072_reject_created(Bytes object,std::uint32_t handle,const PcObjectListServices& s){
    (void)r072_virtual(s,handle,0x10u);
    (void)r072_virtual(s,handle,0x00u,1u,true);
    r072_pop_history(object);
}
}

std::uint32_t object_insert_callback_4440f0(
    Bytes object,std::uint32_t key,
    const PcObjectStatePairTable& pair_table,
    const PcObjectEventCallbackTable& callback_table,
    const PcObjectEventDispatchServices& dispatch_services,
    const PcObjectListServices& list_services){
    object.check(0x0518u,4u);
    auto count=object.i32(0x0518u);
    if(count<0||count>32)throw std::out_of_range("r072 callback-list count");
    for(std::int32_t i=0;i<count;++i){
        const auto h=object.u32(0x0498u+std::size_t(i)*4u);
        if(r072_state(list_services,h)==key)return 0u;
    }
    for(std::int32_t i=0;i<count;++i){
        const auto h=object.u32(0x0498u+std::size_t(i)*4u);
        if(std::uint8_t(r072_virtual(list_services,h,0x1cu,key,true))!=0u)return 0u;
    }
    std::uint32_t raw=0xffffffffu;
    for(std::int32_t i=0;i<count;++i){
        const auto h=object.u32(0x0498u+std::size_t(i)*4u);
        raw=r072_virtual(list_services,h,0x18u,key,true);
    }
    const auto mode=raw+1u;
    if(mode>3u)return 0u;

    std::uint32_t created=0u;
    if(mode<=1u){
        if(count>=32)throw std::out_of_range("r072 callback-list append overflow");
        created=object_dispatch_callback_443420(object,key,pair_table,callback_table,dispatch_services);
        object.put32(0x0498u+std::size_t(count)*4u,created);
        object.puti(0x0518u,count+1);
    }else if(mode==2u){
        if(count<=0||count>=32)throw std::out_of_range("r072 callback-list insert count");
        created=object_dispatch_callback_443420(object,key,pair_table,callback_table,dispatch_services);
        for(std::int32_t i=count;i>count-1;--i)
            object.put32(0x0498u+std::size_t(i)*4u,object.u32(0x0498u+std::size_t(i-1)*4u));
        object.put32(0x0498u+std::size_t(count-1)*4u,created);
        object.puti(0x0518u,count+1);
    }else{
        if(count<=0)throw std::out_of_range("r072 callback-list replace count");
        created=object_dispatch_callback_443420(object,key,pair_table,callback_table,dispatch_services);
        const auto existing=object.u32(0x0498u+std::size_t(count-1)*4u);
        if(list_services.link_handle)list_services.link_handle(list_services.user,0x48d870u,existing,created);else outrun::driving::service_hole("object_insert_callback_4440f0","list_services.link_handle");
    }
    if(created==0u)return 0u;
    if(std::uint8_t(r072_virtual(list_services,created,0x04u))!=0u)return created;
    r072_reject_created(object,created,list_services);
    return 0u;
}

std::uint32_t object_reopen_callback_4442a0(
    Bytes object,std::uint32_t mode,std::uint32_t unused,
    const PcObjectStatePairTable& pair_table,
    const PcObjectEventCallbackTable& callback_table,
    const PcObjectEventModeInputs& inputs,
    const PcObjectEventDispatch443eb0Services& dispatch_services,
    const PcRuntimeControlServices& runtime_services,
    const PcObjectOpenCallbackServices& open_services,
    const PcObjectListServices& list_services){
    (void)unused;
    if(mode>3u)return 0u;
    object.check(0x0518u,4u);
    std::uint32_t source=0u;
    if(mode==0u){
        const auto count=object.u32(0x0484u);
        if(count>0x80u)throw std::out_of_range("r072 reopen indexed count");
        source=object.u32(0x0280u+std::size_t(count)*4u);
    }else if(mode==1u)source=object.u32(0x0488u);
    else if(mode==2u)source=object.u32(0x0490u);
    else source=object.u32(0x0498u);
    const auto key=r072_get(list_services,source);
    object_open_callback_443fa0(object,key,pair_table,callback_table,inputs,
                                dispatch_services,runtime_services,open_services);
    if(mode==3u){
        runtime_release_handle_441200(object,source,runtime_services);
        compact_u32_list_441410(object.sub(0x0498u,object.size()-0x0498u),0);
    }
    return 0u;
}

namespace {
void r072_commit_global(const PcObjectRuntimeCommitServices& s,std::uint32_t pc,
                        std::uint32_t a0=0u,std::uint32_t a1=0u,std::uint32_t argc=0u){
    if(s.global_call)s.global_call(s.user,pc,a0,a1,argc);else outrun::driving::service_hole("r072_commit_global","s.global_call");
}
std::uint32_t r072_commit_virtual(const PcObjectRuntimeCommitServices& s,std::uint32_t h,
                                  std::uint32_t slot,std::uint32_t arg=0u,bool has=false){
    return s.call_virtual?s.call_virtual(s.user,h,slot,arg,has):(outrun::driving::service_hole("r072_commit_virtual","s.call_virtual"),0u);
}
}

void object_runtime_commit_444350(
    Bytes object,PcObjectRuntimeSnapshot444350& snapshot,
    const PcObjectStateServices& state_services,
    const PcRuntimeControlServices& runtime_services,
    const PcObjectRuntimeCommitServices& commit_services){
    object.check(0x0518u,4u);
    if(commit_services.prepare)commit_services.prepare(commit_services.user,0x4eec80u,object,4u);else outrun::driving::service_hole("object_runtime_commit_444350","commit_services.prepare");
    object_refresh_state_table_442e70(object,state_services);
    for(std::size_t i=0;i<snapshot.bytes.size();++i)snapshot.bytes[i]=object.u8(4u+i);
    if(object.i32(0x0484u)>0)snapshot.active=1u;

    const auto mode=object.u32(0x0004u);
    if(mode<=1u)object.put32(0x021cu,0x0du);
    else if(mode==2u){r072_commit_global(commit_services,0x43f870u,0u,0u,1u);object.put32(0x021cu,7u);}
    else if(mode==3u){r072_commit_global(commit_services,0x4957e0u);r072_commit_global(commit_services,0x44da00u,0u,0u,2u);object.put32(0x021cu,0x0du);}

    object_release_flagged_handles_4430b0(object,runtime_services);
    const auto count=object.i32(0x0484u);
    if(count>0){
        if(count>0x80)throw std::out_of_range("r072 commit indexed count");
        for(std::int32_t i=0;i<count;++i){
            const auto h=object.u32(0x0284u+std::size_t(i)*4u);
            // PC 0x4443E4 first calls virtual +0x10 unconditionally, then for
            // a non-null handle repeats +0x10, invokes deleting virtual +0(1),
            // and pops the *current depth*.  This is intentionally not the
            // 0x441200 helper: that helper clears 0x221+resolver_index instead.
            (void)r072_commit_virtual(commit_services,h,0x10u);
            if(h!=0u){
                (void)r072_commit_virtual(commit_services,h,0x10u);
                (void)r072_commit_virtual(commit_services,h,0x00u,1u,true);
                r072_pop_history(object);
            }
        }
        object.put32(0x0484u,0u);
    }
}




bool runtime_primary_mode_4eea70(std::uint32_t selector,PcRuntimePrepareState& state){
    static constexpr std::array<std::uint32_t,4> map{{1u,0u,3u,2u}};
    if(selector>=map.size())return false;
    state.primary_mode_78024c=map[selector];
    return true;
}

bool runtime_route_mode_4eead0(std::uint32_t selector,PcRuntimePrepareState& state){
    if(selector>6u)return false;
    if(selector==0u)return true;
    static constexpr std::array<std::uint32_t,6> map{{1u,2u,9u,7u,8u,10u}};
    state.route_state_780258=map[selector-1u];
    return true;
}

bool runtime_route_config_4eeb50(std::uint32_t selector,std::uint32_t arg2,std::uint32_t arg3,
                                 PcRuntimePrepareState& state,
                                 const PcRuntimePrepareServices& services){
    if(selector>6u)return false;
    if(selector<=3u){
        const auto count=services.count_entries?services.count_entries(services.user,arg2):(outrun::driving::service_hole("runtime_route_config_4eeb50","services.count_entries"),0u);
        const auto max_index=count-1u; // original DEC wraps when count==0
        auto index=arg3;
        if(index>max_index)index=max_index;
        state.selection_active_836374=1u;
        ++index;
        if(services.select_entry)services.select_entry(services.user,arg2,index);else outrun::driving::service_hole("runtime_route_config_4eeb50","services.select_entry");
        state.route_state_780258=6u;
        return true;
    }
    std::array<std::uint32_t,5> cfg{};
    if(selector==4u){cfg[0]=1u;cfg[4]=1u;}
    else if(selector==6u){cfg[0]=3u;cfg[4]=2u;}
    if(arg2!=0u){cfg[1]=1u;cfg[2]=arg2-1u;}
    cfg[3]=(arg3<4u)?arg3:0u;
    state.event_config_7f94c4=cfg;
    state.primary_mode_78024c=(selector==5u)?1u:0u;
    state.route_state_780258=5u;
    return true;
}

void runtime_prepare_4eec80(Bytes runtime,PcRuntimePrepareState& state,
                            const PcRuntimePrepareServices& services){
    runtime.check(0x020cu,4u);
    const auto packed=runtime.u32(0x0208u);
    (void)runtime_primary_mode_4eea70(packed&3u,state);
    const auto branch=(packed>>2u)&7u;
    if(branch!=0u){
        if(branch==1u&&services.select_entry)services.select_entry(services.user,0u,1u);
        return;
    }
    const auto route=(packed>>5u)&7u;
    (void)runtime_route_mode_4eead0(route,state);
    if(route==0u){
        const auto selector=(packed>>8u)&7u;
        const auto arg2=(packed>>12u)&0x3fu;
        const auto arg3=(packed>>18u)&7u;
        (void)runtime_route_config_4eeb50(selector,arg2,arg3,state,services);
        return;
    }
    if(route!=4u&&route!=5u)return;
    auto code=(packed>>12u)&0x3fu;
    const bool alternate=((packed>>11u)&1u)!=0u;
    if(!alternate){
        if(code>=15u)code+=15u;
        if(state.primary_mode_78024c==0u)code+=15u;
        if(state.route_state_780258==7u){
            state.output_code_656234=code;
            state.output_flag_830395=std::uint8_t(runtime.u32(0x020cu)&1u);
        }else state.alternate_code_836174=code;
        return;
    }
    if(state.primary_mode_78024c==0u){
        if(state.route_state_780258==7u){
            if(code==5u){state.primary_mode_78024c=2u;code=0x52u;}
            else if(code==11u){state.primary_mode_78024c=2u;code=0x53u;}
            else code+=(code>5u)?0x45u:0x41u;
        }else code+=(code>4u)?0x46u:0x41u;
    }else{
        if(state.route_state_780258==7u){
            if(code==5u){state.primary_mode_78024c=3u;code=0x50u;}
            else if(code==11u){state.primary_mode_78024c=3u;code=0x51u;}
            else code+=(code>5u)?0x40u:0x3cu;
        }else code+=(code>4u)?0x41u:0x3cu;
    }
    if(state.route_state_780258==7u){
        state.output_code_656234=code;
        state.output_flag_830395=0u;
    }else state.alternate_code_836174=code;
}

std::uint32_t callback_key_48f4e0(Bytes callback_object){
    return callback_object.u32(0x04u);
}

bool callback_append_48d870(Bytes callback_object,std::uint32_t new_handle){
    if(new_handle==0u)return false;
    const auto count=callback_object.u32(0x04d0u);
    const auto off=0x04b0u+std::size_t(count)*4u;
    callback_object.put32(off,new_handle);
    callback_object.put32(0x04d0u,count+1u);
    return true;
}



void runtime_select_entry_4958a0(PcRuntimeCategoryState& state,
                                  std::uint32_t category,std::uint32_t index){
    if(state.category_keys==nullptr||category>=state.category_count)
        throw std::out_of_range("r074 category selector");
    state.selected_key_67e6a4=state.category_keys[category];
    state.selected_index_67e6a8=index;
}

std::uint32_t runtime_count_entries_495930(const PcRuntimeCategoryState& state,
                                           std::uint32_t category){
    if(state.mode_836358!=3u)return 0u;
    if(state.category_keys==nullptr||category>=state.category_count)
        throw std::out_of_range("r074 category count selector");
    const auto key=state.category_keys[category];
    std::uint32_t count=0u;
    if(state.records==nullptr&&state.record_count!=0u)
        throw std::invalid_argument("r074 null record table");
    for(std::size_t i=0;i<state.record_count;++i)if(state.records[i].key==key)++count;
    return count;
}
std::uint32_t runtime_count_entries_495930_service(void* user,std::uint32_t category){
    if(user==nullptr)return 0u;
    return runtime_count_entries_495930(*static_cast<PcRuntimeCategoryState*>(user),category);
}
void runtime_select_entry_4958a0_service(void* user,std::uint32_t category,std::uint32_t index){
    if(user==nullptr)return;
    runtime_select_entry_4958a0(*static_cast<PcRuntimeCategoryState*>(user),category,index);
}

void runtime_game_flag_43f870(PcRuntimePrepareState& state,std::uint8_t value){
    state.game_flag_780260=value;
}
void runtime_selection_disable_4957e0(PcRuntimePrepareState& state){state.selection_active_836374=0u;}
std::uint32_t runtime_external_block_4872e0(std::uint32_t value_82e7d8){return value_82e7d8;}
bool runtime_system_handle_active_4999c0(std::uint32_t handle_67f614){return handle_67f614!=0xffffffffu;}
std::uint32_t runtime_menu_state_450240(std::uint32_t state_7d38f0){return state_7d38f0;}
std::int32_t runtime_current_player_47f110(){return 0;}
std::uint32_t runtime_feature_mask_4536f0(std::uint32_t player0_mask,std::uint32_t requested_mask){
    // The pinned PC body calls 0x47F110, which is the closed constant-zero
    // current-player selector, then masks slot-0 flags with the argument.
    (void)runtime_current_player_47f110();
    return player0_mask&requested_mask;
}

const PcNativeHandleBinding* native_handle_find(const PcNativeHandleResolver& resolver,
                                                std::uint32_t token){
    if(token==0u)return nullptr;
    if(resolver.bindings==nullptr&&resolver.count!=0u)
        throw std::invalid_argument("r074 null handle registry");
    for(std::size_t i=0;i<resolver.count;++i)if(resolver.bindings[i].token==token)return &resolver.bindings[i];
    return nullptr;
}
std::uint32_t native_handle_state_564c90(const PcNativeHandleResolver& resolver,
                                         std::uint32_t token){
    const auto* b=native_handle_find(resolver,token);
    if(b==nullptr)throw std::out_of_range("r074 unresolved handle state");
    return Bytes(b->object,b->size).u32(0x08u);
}
std::uint32_t native_handle_key_48f4e0(const PcNativeHandleResolver& resolver,
                                       std::uint32_t token){
    const auto* b=native_handle_find(resolver,token);
    if(b==nullptr)throw std::out_of_range("r074 unresolved handle key");
    return callback_key_48f4e0(Bytes(b->object,b->size));
}

void object_runtime_gate_444470(
    Bytes object,Bytes current_object,
    const PcNativeHandleResolver& handles,
    const PcRuntimeGate444470Inputs& gate_inputs,
    const PcObjectStatePairTable& pair_table,
    const PcObjectEventCallbackTable& callback_table,
    const PcObjectEventModeInputs& event_inputs,
    const PcObjectEventDispatch443eb0Services& dispatch_services,
    const PcRuntimeControlServices& runtime_services,
    const PcObjectOpenCallbackServices& open_services,
    const PcRuntimeGate444470Services& gate_services){
    object.check(0x051cu,1u);
    current_object.check(0x048cu,1u);
    if(gate_inputs.game_mode_78026c!=0x10u)return;
    if(runtime_external_block_4872e0(gate_inputs.external_block_82e7d8)!=0u)return;

    const auto current_handle=current_object.u32(0x0488u);
    if(current_handle!=0u&&current_object.u8(0x048cu)!=0u&&
       native_handle_state_564c90(handles,current_handle)==0x1bu)return;
    if(object_has_active_state_443060(object)!=0u)return;
    if(runtime_system_handle_active_4999c0(gate_inputs.system_handle_67f614))return;

    const auto menu=runtime_menu_state_450240(gate_inputs.menu_state_7d38f0);
    if(menu==7u||menu==6u||menu!=0u)return;
    if(runtime_feature_mask_4536f0(gate_inputs.player0_feature_mask,1u)==0u)return;

    if(object.u32(0x0218u)==2u&&gate_services.configure_446f30)
        gate_services.configure_446f30(gate_services.user,
                                      object.sub(0x051cu,object.size()-0x051cu),1u,1u);

    const auto own_handle=object.u32(0x0488u);
    if(own_handle!=0u&&native_handle_state_564c90(handles,own_handle)==0x2cu)return;
    object_open_callback_443fa0(object,0x2cu,pair_table,callback_table,event_inputs,
                                dispatch_services,runtime_services,open_services);
}

namespace {
constexpr std::array<std::pair<std::uint32_t,std::uint32_t>,8> R075EmbeddedEffectMap{{
    {0x04u,0x002c013bu},{0x08u,0x002c013au},{0x10u,0x002c0139u},{0x20u,0x002c0138u},
    {0x01u,0x002c013bu},{0x02u,0x002c013au},{0x8000u,0x002c0139u},{0x4000u,0x002c0138u}}};
constexpr std::array<std::pair<std::uint32_t,std::uint32_t>,4> R075EmbeddedCoords{{
    {0xc37c0000u,0x433c0000u},{0xc3180000u,0x433c0000u},
    {0x43160000u,0x433c0000u},{0x437a0000u,0x433c0000u}}};
const char* r075_course_name(std::uint32_t mode){
    switch(mode){
        case 1u:case 4u:return "csc_data_2";
        case 0u:case 3u:return "csc_data_cvt";
        case 5u:case 7u:case 8u:case 10u:return "csc_data_cvt_ren";
        case 6u:case 9u:return "csc_data_2_ren";
        case 2u:return "csc_data_easy";
        default:return nullptr;
    }
}
}

bool runtime_course_load_44da00(std::uint32_t mode,std::uint8_t alternate,
                                 PcCourseLoadState44da00& state,
                                 const PcCourseLoadServices44da00& services){
    const char* data=r075_course_name(mode);
    if(data==nullptr)return false; // PC callers only use the defined 0..10 modes.
    std::array<char,32> course{};
    const auto n=std::snprintf(course.data(),course.size(),"%s_course",data);
    if(n<0||std::size_t(n)>=course.size())throw std::runtime_error("r075 course name overflow");
    const auto applied=services.apply_44d720?
        services.apply_44d720(services.user,data,course.data(),alternate):(outrun::driving::service_hole("runtime_course_load_44da00","services.apply_44d720"),0u);
    if(alternate!=0u)return applied!=0u;
    state.force_sync_7d2d8c=1u;
    return true;
}

void embedded_slot_refresh_446bb0(Bytes embedded,std::uint32_t slot,
                                  const PcEmbeddedConfigServices446f30& services){
    if(slot>=4u)throw std::out_of_range("r075 embedded slot");
    const auto child_off=0x040cu+std::size_t(slot)*0xa0u;
    embedded.check(child_off,0xa0u);
    Bytes child=embedded.sub(child_off,0xa0u);
    if(child.u32(0x08u)==0xffffffffu)return;
    ui_resource_reset_465250(child,services.ui_reset_services);
    const auto depth=embedded.u32(0x0408u);
    const auto hist_off=0x08u+std::size_t(depth)*0x20u+std::size_t(slot)*4u;
    embedded.check(hist_off,4u);
    const auto key=embedded.u32(hist_off);
    if(key==0xffffffffu)return;
    std::uint32_t effect=0xffffffffu;
    for(const auto& e:R075EmbeddedEffectMap)if(e.first==key){effect=e.second;break;}
    if(effect==0xffffffffu)return;
    const auto [x,y]=R075EmbeddedCoords[slot];
    if(services.configure_ui)services.configure_ui(services.user,child,effect,slot,x,y);else outrun::driving::service_hole("embedded_slot_refresh_446bb0","services.configure_ui");
    if(services.finalize_ui)services.finalize_ui(services.user,child);else outrun::driving::service_hole("embedded_slot_refresh_446bb0","services.finalize_ui");
}

bool embedded_configure_446f30(Bytes embedded,std::uint32_t selector,std::int8_t flag,
                               const PcEmbeddedConfigServices446f30& services){
    embedded.check(0x068cu,1u);
    const auto depth=embedded.u32(0x0408u);
    for(std::uint32_t slot=0;slot<4u;++slot){
        const auto hist_off=0x08u+std::size_t(depth)*0x20u+std::size_t(slot)*4u;
        embedded.check(hist_off,4u);
        if(embedded.u32(hist_off)!=selector)continue;
        embedded_slot_refresh_446bb0(embedded,slot,services);
        bool emit=false;std::uint32_t value=0x40u;
        if(selector==8u&&flag>=0){emit=true;value=0u;}
        else if(selector==4u&&flag==1){emit=true;}
        else if(selector==0x10u||selector==0x20u||selector==0x4000u||selector==0x8000u){emit=true;}
        if(emit&&services.global_effect)services.global_effect(services.user,value);
    }
    return true;
}

PcNativeHandleResolver native_handle_resolver(const PcNativeHandleRegistry& registry){
    return {registry.bindings,registry.count};
}
std::uint32_t native_handle_register(PcNativeHandleRegistry& registry,void* object,
                                     std::size_t size,std::uint32_t preferred_token){
    if(object==nullptr||size==0u)throw std::invalid_argument("r075 invalid native handle object");
    if(registry.bindings==nullptr||registry.count>=registry.capacity)throw std::length_error("r075 native handle registry full");
    auto used=[&](std::uint32_t token){for(std::size_t i=0;i<registry.count;++i)if(registry.bindings[i].token==token)return true;return false;};
    std::uint32_t token=preferred_token;
    if(token==0u){
        token=registry.next_token==0u?1u:registry.next_token;
        while(token==0u||used(token)){++token;if(token==0u)token=1u;}
        registry.next_token=token+1u;if(registry.next_token==0u)registry.next_token=1u;
    }else if(used(token))throw std::invalid_argument("r075 duplicate native handle token");
    registry.bindings[registry.count++]={token,object,size};
    return token;
}
bool native_handle_unregister(PcNativeHandleRegistry& registry,std::uint32_t token){
    for(std::size_t i=0;i<registry.count;++i)if(registry.bindings[i].token==token){
        for(std::size_t j=i+1u;j<registry.count;++j)registry.bindings[j-1u]=registry.bindings[j];
        --registry.count;return true;
    }
    return false;
}

namespace {
void copy_course_record(Bytes src,std::array<std::uint8_t,PcCourseRecord44d720Size>& dst){
    src.check(0,PcCourseRecord44d720Size);
    for(std::size_t i=0;i<PcCourseRecord44d720Size;++i)dst[i]=src.u8(i);
}
void copy_matrix64(Bytes src,std::array<std::uint8_t,64>& dst){
    src.check(0,64u);for(std::size_t i=0;i<64u;++i)dst[i]=src.u8(i);
}
void course_tree_impl(Bytes records,std::int32_t count,std::int32_t index,
                      std::int32_t depth,std::int32_t branch,std::int32_t& max_depth){
    if(index<0||index>=count)throw std::out_of_range("r076 course tree index");
    const auto off=std::size_t(index)*PcCourseRecord44d720Size;
    Bytes rec=records.sub(off,PcCourseRecord44d720Size);
    rec.puti(0x08u,depth);rec.put8(0x0cu,std::uint8_t(branch));
    const auto own=rec.i32(0x04u);
    const auto descend=[&](std::size_t key_off,std::size_t index_off,std::int32_t next_branch){
        const auto key=rec.i32(key_off);if(key==-1||key<=own)return;
        const auto child=rec.i32(index_off);course_tree_impl(records,count,child,depth+1,next_branch,max_depth);
    };
    descend(0x2cu,0x24u,branch);
    descend(0x30u,0x28u,branch+1);
    if(depth>max_depth)max_depth=depth;
}
const PcCourseDescriptor44d720& primary_descriptor(const PcCourseDescriptorTables44d720& tables,
                                                    std::uint32_t index){
    if(tables.primary==nullptr||index>=tables.primary_count)throw std::out_of_range("r076 primary descriptor index");
    return tables.primary[index];
}
std::uint32_t secondary_descriptor(const PcCourseDescriptorTables44d720& tables,std::uint32_t index){
    if(tables.secondary_tokens==nullptr||index>=tables.secondary_count)throw std::out_of_range("r076 secondary descriptor index");
    return tables.secondary_tokens[index];
}
}

void runtime_course_shuffle_44bf30(Bytes records,std::int32_t count,
                                   void* user,PcCourseRandom44bf30 random_value){
    if(count<=1)return;
    if(random_value==nullptr)throw std::invalid_argument("r076 missing course RNG");
    const auto needed=std::size_t(count)*PcCourseRecord44d720Size;records.check(0,needed);
    for(std::int32_t remaining=count;remaining>1;--remaining){
        const auto r=random_value(user);const auto pick=r%remaining;
        if(pick<0||pick>=remaining)throw std::out_of_range("r076 negative course RNG remainder");
        Bytes a=records.sub(std::size_t(pick)*PcCourseRecord44d720Size,PcCourseRecord44d720Size);
        Bytes b=records.sub(std::size_t(remaining-1)*PcCourseRecord44d720Size,PcCourseRecord44d720Size);
        const auto a1=a.u32(0x1cu),a2=a.u32(0x20u);
        a.put32(0x1cu,b.u32(0x1cu));a.put32(0x20u,b.u32(0x20u));
        b.put32(0x1cu,a1);b.put32(0x20u,a2);
    }
}

void runtime_course_tree_44c850(Bytes records,std::int32_t count,std::int32_t root_index,
                                std::int32_t depth,std::int32_t branch,std::int32_t& max_depth){
    if(count<=0)throw std::invalid_argument("r076 empty course tree");
    records.check(0,std::size_t(count)*PcCourseRecord44d720Size);
    course_tree_impl(records,count,root_index,depth,branch,max_depth);
}

void runtime_course_matrix_44c0d0(const PcCourseDescriptor44d720& descriptor,
                                  PcMatrixStack& matrices,PcCourseRuntimeState44d720& state){
    if(descriptor.token==0u)return;
    if(descriptor.data==nullptr||descriptor.size<0x98u)throw std::out_of_range("r076 course descriptor");
    Bytes d(descriptor.data,descriptor.size);
    pc_matrix_push(matrices);pc_matrix_identity(matrices);
    copy_matrix64(matrices.current(),state.matrix_7d2da0);
    copy_matrix64(matrices.current(),state.matrix_7d3130);
    pc_matrix_translate_vector(matrices,{d.f32(0x80u),d.f32(0x84u),d.f32(0x88u)});
    pc_matrix_rotate_y(matrices,d.f32(0x90u));
    pc_matrix_rotate_x(matrices,d.f32(0x8cu));
    pc_matrix_rotate_z(matrices,d.f32(0x94u));
    copy_matrix64(matrices.current(),state.matrix_7d3190);
    pc_matrix_pop(matrices);
    state.zero_7d3124={0.0f,0.0f,0.0f};state.zero_7d3178={0.0f,0.0f,0.0f};
}

void runtime_course_force_mode_46c360(PcCourseRuntimeState44d720& state,std::uint32_t value){
    state.force_mode_7f95a8=value;
}

bool runtime_apply_course_data_44d720(const PcCourseApplyRequest44d720& request,
                                      const PcCourseDescriptorTables44d720& tables,
                                      PcCourseRuntimeState44d720& state,
                                      PcMatrixStack& matrices,
                                      const PcCourseRuntimeServices44d720& services){
    if(request.data_name==nullptr||request.category_name==nullptr)throw std::invalid_argument("r076 course names");
    state.stage_key_635f2c=-1;state.stage_value_635f30=-1;
    const bool direct=request.direct_records!=nullptr;
    if(!direct){
        if(!request.loader.present||request.loader.phase>=3)return true;
        std::array<char,96> path{};const auto n=std::snprintf(path.data(),path.size(),"\\Scripts\\bin\\%s.bin",request.data_name);
        if(n<0||std::size_t(n)>=path.size())throw std::runtime_error("r076 course path overflow");
        if(services.load_binary==nullptr)return false;
        if(!services.load_binary(services.user,path.data(),request.loader_mode,request.required,state))return false;
    }

    void* record_ptr=nullptr;std::size_t record_bytes=0u;std::int32_t count=0;
    if(state.resource_type_635f34!=0x42u){
        state.source_7d33f8=state.resource_source_635f38;state.type_7d33f4=state.resource_type_635f34;state.fixed_7d33e8=0x5au;
        const auto sentinel=state.fallback_gate_7d33b0==0u?0:-1;state.fallback_left_7d3404=sentinel;state.fallback_right_7d3408=sentinel;
        state.reset_7d3448=0u;state.reset_7d3444=0u;state.reset_7d343c=0u;state.reset_7d3440=0u;state.reset_7d33dc=0u;
        // 0x7D33D8 is both the fallback record and the storage behind the named
        // globals above; preserve that aliasing before the common record pass.
        Bytes fallback(state.fallback_record_7d33d8.data(),state.fallback_record_7d33d8.size());
        fallback.put32(0x20u,state.source_7d33f8);fallback.put32(0x1cu,state.type_7d33f4);fallback.put32(0x10u,state.fixed_7d33e8);
        fallback.puti(0x2cu,state.fallback_left_7d3404);fallback.puti(0x30u,state.fallback_right_7d3408);
        fallback.put32(0x70u,0u);fallback.put32(0x6cu,0u);fallback.put32(0x64u,0u);fallback.put32(0x68u,0u);fallback.put32(0x04u,0u);
        record_ptr=state.fallback_record_7d33d8.data();record_bytes=state.fallback_record_7d33d8.size();count=1;
    }else if(direct){
        record_ptr=request.direct_records;record_bytes=request.direct_bytes;count=request.direct_count;
    }else{
        if(services.lookup_records==nullptr)return true;
        const auto source=services.lookup_records(services.user,request.category_name);
        if(source.records==nullptr)return true;
        record_ptr=source.records;record_bytes=source.bytes;count=source.count;
    }
    if(record_ptr==nullptr)return true;
    if(count<0)throw std::invalid_argument("r076 negative course record count");
    const auto min_records=std::max<std::int32_t>(count,1);
    if(record_bytes<std::size_t(min_records)*PcCourseRecord44d720Size)throw std::out_of_range("r076 course record buffer");
    Bytes records(record_ptr,record_bytes);

    if(state.resource_type_635f34==0x42u&&request.shuffle_groups&&count==15){
        runtime_course_shuffle_44bf30(records.sub(PcCourseRecord44d720Size,9u*PcCourseRecord44d720Size),9,services.user,services.random_value);
        runtime_course_shuffle_44bf30(records.sub(10u*PcCourseRecord44d720Size,5u*PcCourseRecord44d720Size),5,services.user,services.random_value);
    }
    for(std::int32_t i=0;i<count;++i){
        Bytes rec=records.sub(std::size_t(i)*PcCourseRecord44d720Size,PcCourseRecord44d720Size);
        const auto& primary=primary_descriptor(tables,rec.u32(0x1cu));
        const auto secondary=secondary_descriptor(tables,rec.u32(0x20u));
        if(primary.token!=0u&&(primary.data==nullptr||primary.size<4u))throw std::out_of_range("r076 primary descriptor data");
        rec.put32(0x18u,secondary);rec.put32(0x14u,primary.token);
        rec.put32(0x00u,primary.token==0u?0u:Bytes(primary.data,primary.size).u32(0));
        for(const auto& pair:std::array<std::pair<std::size_t,std::size_t>,2>{{{0x2cu,0x24u},{0x30u,0x28u}}}){
            rec.put32(pair.second,0xffffffffu);const auto key=rec.u32(pair.first);
            for(std::int32_t j=0;j<count;++j){Bytes candidate=records.sub(std::size_t(j)*PcCourseRecord44d720Size,PcCourseRecord44d720Size);if(candidate.u32(0x04u)==key){rec.puti(pair.second,j);break;}}
        }
    }

    std::int32_t selected=0;
    bool force_match=false;
    if(request.force_selected){
        for(std::int32_t i=0;i<count;++i){
            Bytes rec=records.sub(std::size_t(i)*PcCourseRecord44d720Size,PcCourseRecord44d720Size);
            if(rec.u32(0x04u)!=request.selected_key)continue;
            rec.put32(0x24u,0u);rec.put32(0x28u,0xffffffffu);rec.put32(0x2cu,rec.u32(0x04u));rec.put32(0x30u,0xffffffffu);
            rec.put32(0x20u,0u);rec.put32(0x18u,secondary_descriptor(tables,0u));rec.put32(0x10u,99u);selected=i;force_match=true;
        }
        state.active_count_7d33c4=1;runtime_course_force_mode_46c360(state,1u);
    }else state.active_count_7d33c4=count;

    state.selected_index=selected;state.max_depth_7d33c0=0;
    state.records_fallback=record_ptr==static_cast<void*>(state.fallback_record_7d33d8.data());
    state.records_7d33bc=state.records_fallback?nullptr:static_cast<std::uint8_t*>(record_ptr);state.records_bytes=record_bytes;
    Bytes selected_records=records.sub(std::size_t(selected)*PcCourseRecord44d720Size,
                                       record_bytes-std::size_t(selected)*PcCourseRecord44d720Size);
    // A matching forced selection deliberately turns the chosen record into a
    // leaf. If the requested key is absent, the original keeps record 0 as the
    // root and still walks the complete table even though active_count is 1.
    const auto tree_count=request.force_selected&&force_match?1:count;
    runtime_course_tree_44c850(selected_records,std::max(tree_count,1),0,0,0,state.max_depth_7d33c0);
    Bytes selected_rec=selected_records.sub(0,PcCourseRecord44d720Size);
    copy_course_record(selected_rec,state.selected_7d30a8);copy_course_record(selected_rec,state.selected_7d2de0);state.selected_copy_active=true;
    const auto& descriptor=primary_descriptor(tables,selected_rec.u32(0x1cu));
    runtime_course_matrix_44c0d0(descriptor,matrices,state);
    return true;
}


namespace {
bool r077_category_entry(const char* name,const PcRelocCategoryBlobR077& blob,
                         std::size_t& entry_offset){
    if(name==nullptr)throw std::invalid_argument("r077 null category name");
    if(blob.data==nullptr||blob.size<8u)return false;
    Bytes bytes(blob.data,blob.size);
    const auto target=runtime_category_hash_4f1260(name);
    std::int64_t low=-1;
    std::int64_t high=static_cast<std::int64_t>(blob.category_count);
    while(high-low>1){
        const auto mid=(high+low)/2;
        const auto off=blob.index_offset+std::size_t(mid)*12u;
        const auto key=bytes.u32(off);
        if(target<=key)high=mid;else low=mid;
    }
    entry_offset=blob.index_offset+std::size_t(high)*12u;
    bytes.check(entry_offset,12u);
    return bytes.u32(entry_offset)==target;
}
}

bool course_reloc_blob_open_r077(void* data,std::size_t size,PcRelocCategoryBlobR077& out){
    out={};
    if(data==nullptr||size<8u)return false;
    Bytes bytes(data,size);
    const auto categories=bytes.u32(0u);
    const auto relocs=bytes.u32(4u);
    if(std::size_t(relocs)>(size-8u)/8u)return false;
    const auto index_offset=8u+std::size_t(relocs)*8u;
    if(std::size_t(categories)>(std::numeric_limits<std::size_t>::max()/12u)-1u)return false;
    const auto index_bytes=(std::size_t(categories)+1u)*12u; // original lower_bound reads sentinel N.
    if(index_offset>size||index_bytes>size-index_offset)return false;

    for(std::size_t i=0;i<relocs;++i){
        const auto off=8u+i*8u;
        const auto patch=std::size_t(bytes.u32(off));
        const auto target=std::size_t(bytes.u32(off+4u));
        if(patch>size||size-patch<4u||target>size)return false;
    }
    std::uint32_t previous=0u;
    for(std::size_t i=0;i<categories;++i){
        const auto off=index_offset+i*12u;
        const auto key=bytes.u32(off);
        const auto data_offset=std::size_t(bytes.u32(off+8u));
        if(i!=0u&&key<previous)return false;
        if(data_offset>size)return false;
        previous=key;
    }
    out={data,size,categories,relocs,index_offset};
    return true;
}

std::uint32_t runtime_category_hash_4f1260(const char* name){
    if(name==nullptr)throw std::invalid_argument("r077 null category hash name");
    std::uint32_t hash=0u;
    for(const auto* p=reinterpret_cast<const unsigned char*>(name);*p!=0u;++p){
        unsigned char c=*p;
        if(c>='a'&&c<='z')c=static_cast<unsigned char>(c-('a'-'A'));
        const auto signed_c=static_cast<std::int32_t>(static_cast<std::int8_t>(c));
        hash=hash*0x83u+static_cast<std::uint32_t>(signed_c);
    }
    return hash;
}

void* runtime_category_records_4f1a90(const char* category_name,const PcRelocCategoryBlobR077& blob){
    std::size_t entry=0u;if(!r077_category_entry(category_name,blob,entry))return nullptr;
    Bytes bytes(blob.data,blob.size);const auto data_offset=std::size_t(bytes.u32(entry+8u));
    if(data_offset>blob.size)throw std::out_of_range("r077 category record offset");
    return static_cast<std::uint8_t*>(blob.data)+data_offset;
}

std::int32_t runtime_category_count_4f1ba0(const char* category_name,const PcRelocCategoryBlobR077& blob){
    std::size_t entry=0u;if(!r077_category_entry(category_name,blob,entry))return 0;
    const auto count=Bytes(blob.data,blob.size).u32(entry+4u);
    std::int32_t out{};std::memcpy(&out,&count,sizeof(out));return out;
}

bool course_provider_load_binary_r077(void* user,const char* path,
                                      std::uint32_t mode,std::uint32_t required,
                                      PcCourseRuntimeState44d720& state){
    (void)state;
    if(user==nullptr||path==nullptr)return false;
    auto& provider=*static_cast<PcCourseProviderR077*>(user);
    provider.loaded=false;provider.size=0u;provider.blob={};
    if(provider.storage==nullptr||provider.capacity==0u||provider.read_file==nullptr)return false;
    std::size_t bytes_read=0u;
    if(!provider.read_file(provider.read_user,path,mode,required,
                           provider.storage,provider.capacity,bytes_read))return false;
    if(bytes_read>provider.capacity)return false;
    PcRelocCategoryBlobR077 parsed{};
    if(!course_reloc_blob_open_r077(provider.storage,bytes_read,parsed))return false;
    provider.size=bytes_read;provider.blob=parsed;provider.loaded=true;return true;
}

PcCourseRecordSource44d720 course_provider_lookup_records_r077(void* user,const char* category_name){
    if(user==nullptr||category_name==nullptr)return {};
    auto& provider=*static_cast<PcCourseProviderR077*>(user);
    if(!provider.loaded)return {};
    auto* records=runtime_category_records_4f1a90(category_name,provider.blob);
    const auto count=runtime_category_count_4f1ba0(category_name,provider.blob);
    if(records==nullptr||count<=0)return {};
    const auto bytes_needed=std::size_t(count)*PcCourseRecord44d720Size;
    if(std::size_t(count)>std::numeric_limits<std::size_t>::max()/PcCourseRecord44d720Size)
        throw std::out_of_range("r077 course record count overflow");
    const auto base=static_cast<std::uint8_t*>(provider.blob.data);
    const auto ptr=static_cast<std::uint8_t*>(records);
    if(ptr<base||std::size_t(ptr-base)>provider.blob.size||bytes_needed>provider.blob.size-std::size_t(ptr-base))
        throw std::out_of_range("r077 course category records");
    return {records,bytes_needed,count};
}

PcCourseRuntimeServices44d720 course_provider_services_r077(PcCourseProviderR077& provider,
                                                             PcCourseRandom44bf30 random_value){
    return {&provider,course_provider_load_binary_r077,course_provider_lookup_records_r077,random_value};
}

bool course_provider_stdio_read_r077(void* user,const char* path,
                                     std::uint32_t mode,std::uint32_t required,
                                     void* destination,std::size_t capacity,
                                     std::size_t& bytes_read){
    (void)mode;(void)required;bytes_read=0u;
    if(path==nullptr||destination==nullptr||capacity==0u)return false;
    const auto* cfg=static_cast<const PcCourseStdioRootR077*>(user);
    const char* root=(cfg&&cfg->root)?cfg->root:"";
    std::array<char,512> native{};std::size_t n=0u;
    for(const char* p=root;*p!=0;++p){if(n+1u>=native.size())return false;native[n++]=*p;}
    if(n!=0u&&native[n-1u]!='/'&&native[n-1u]!='\\'){if(n+1u>=native.size())return false;native[n++]='/';}
    const char* p=path;while(*p=='/'||*p=='\\')++p;
    for(;*p!=0;++p){if(n+1u>=native.size())return false;native[n++]=(*p=='\\')?'/':*p;}
    native[n]=0;
    std::FILE* f=std::fopen(native.data(),"rb");if(f==nullptr)return false;
    if(std::fseek(f,0,SEEK_END)!=0){std::fclose(f);return false;}
    const auto end=std::ftell(f);if(end<0||static_cast<unsigned long long>(end)>capacity){std::fclose(f);return false;}
    if(std::fseek(f,0,SEEK_SET)!=0){std::fclose(f);return false;}
    const auto wanted=static_cast<std::size_t>(end);
    if(wanted!=0u&&std::fread(destination,1u,wanted,f)!=wanted){std::fclose(f);return false;}
    std::fclose(f);bytes_read=wanted;return true;
}

std::uint32_t runtime_course_apply_bridge_r077(void* user,const char* data_name,
                                               const char* course_name,std::uint8_t alternate){
    if(user==nullptr)return 0u;
    auto& bridge=*static_cast<PcCourseRuntimeBridgeR077*>(user);
    if(bridge.state==nullptr||bridge.matrices==nullptr)return 0u;
    PcCourseApplyRequest44d720 request{};request.data_name=data_name;request.category_name=course_name;
    request.loader=bridge.loader;request.loader_mode=alternate!=0u?1u:0u;request.required=1u;
    const auto ok=runtime_apply_course_data_44d720(request,bridge.tables,*bridge.state,*bridge.matrices,bridge.services);
    if(ok&&bridge.loader.present&&bridge.loader.phase<3)bridge.loader.phase=3;
    return ok?1u:0u;
}

PcCourseLoadServices44da00 runtime_course_load_services_r077(PcCourseRuntimeBridgeR077& bridge){
    return {&bridge,runtime_course_apply_bridge_r077};
}


bool course_descriptor_pack_open_r078(void* data,std::size_t size,PcCourseDescriptorPackR078& out){
    out={};
    constexpr std::size_t Header=24u;
    constexpr std::array<std::uint8_t,8> Magic{{'O','R','C','7','8','T','B','L'}};
    if(data==nullptr||size<Header)return false;
    Bytes b(data,size);
    for(std::size_t i=0;i<Magic.size();++i)if(b.u8(i)!=Magic[i])return false;
    const auto version=b.u32(8u);
    if(version!=1u&&version!=2u&&version!=3u)return false;
    const std::size_t Entry=version==3u?4u+PcCourseDescriptorBytesR078:
                            version==2u?44u:32u;
    const auto primary_count=std::size_t(b.u32(12u));
    const auto secondary_count=std::size_t(b.u32(16u));
    if(primary_count>PcCoursePrimaryDescriptorCountR078||secondary_count>PcCourseSecondaryDescriptorCountR078)return false;
    if(b.u32(20u)!=0u)return false;
    if(primary_count>(std::numeric_limits<std::size_t>::max()-Header)/Entry)return false;
    const auto secondary_off=Header+primary_count*Entry;
    if(secondary_count>(std::numeric_limits<std::size_t>::max()-secondary_off)/4u)return false;
    const auto expected=secondary_off+secondary_count*4u;
    if(expected!=size)return false;
    for(std::size_t i=0;i<primary_count;++i){
        const auto off=Header+i*Entry;
        auto& raw=out.storage[i];Bytes d(raw.data(),raw.size());
        if(version==3u){
            for(std::size_t j=0;j<raw.size();++j)raw[j]=b.u8(off+4u+j);
        }else{
            d.put32(0x00u,b.u32(off+4u));
            d.put32(0x80u,b.u32(off+8u));d.put32(0x84u,b.u32(off+12u));d.put32(0x88u,b.u32(off+16u));
            d.put32(0x8cu,b.u32(off+20u));d.put32(0x90u,b.u32(off+24u));d.put32(0x94u,b.u32(off+28u));
            if(version==2u){
                d.put32(0x04u,b.u32(off+32u));
                d.put32(0x38u,b.u32(off+36u));
                d.put32(0x60u,b.u32(off+40u));
            }
        }
        out.primary[i]={b.u32(off),raw.data(),raw.size()};
    }
    for(std::size_t i=0;i<secondary_count;++i)out.secondary_tokens[i]=b.u32(secondary_off+i*4u);
    out.primary_count=primary_count;out.secondary_count=secondary_count;
    out.world_resource_fields_present=version>=2u;
    return true;
}

PcCourseDescriptorTables44d720 course_descriptor_tables_r078(const PcCourseDescriptorPackR078& pack){
    return {pack.primary.data(),pack.primary_count,pack.secondary_tokens.data(),pack.secondary_count};
}

void runtime_commit_prepare_bridge_r078(void* user,std::uint32_t pc_entry,Bytes object,
                                        std::size_t object_offset){
    if(user==nullptr)return;
    auto& bridge=*static_cast<PcRuntimeCommitBridgeR078*>(user);
    if(pc_entry!=0x004eec80u)throw std::invalid_argument("r078 unexpected commit prepare entry");
    if(bridge.prepare_state==nullptr)return;
    if(object_offset>object.size())throw std::out_of_range("r078 commit prepare object offset");
    runtime_prepare_4eec80(object.sub(object_offset,object.size()-object_offset),
                           *bridge.prepare_state,bridge.prepare_services);
}

void runtime_commit_global_bridge_r078(void* user,std::uint32_t pc_entry,
                                       std::uint32_t arg0,std::uint32_t arg1,
                                       std::uint32_t argument_count){
    if(user==nullptr)return;
    auto& bridge=*static_cast<PcRuntimeCommitBridgeR078*>(user);
    switch(pc_entry){
        case 0x0043f870u:
            if(argument_count!=1u)throw std::invalid_argument("r078 43f870 argc");
            if(bridge.prepare_state)runtime_game_flag_43f870(*bridge.prepare_state,std::uint8_t(arg0));
            return;
        case 0x004957e0u:
            if(argument_count!=0u)throw std::invalid_argument("r078 4957e0 argc");
            if(bridge.prepare_state)runtime_selection_disable_4957e0(*bridge.prepare_state);
            return;
        case 0x0044da00u:
            if(argument_count!=2u)throw std::invalid_argument("r078 44da00 argc");
            if(bridge.course_load_state) (void)runtime_course_load_44da00(arg0,std::uint8_t(arg1),
                                            *bridge.course_load_state,bridge.course_services);
            return;
        default: throw std::invalid_argument("r078 unexpected commit global entry");
    }
}

std::uint32_t runtime_commit_virtual_bridge_r078(void* user,std::uint32_t handle,
                                                  std::uint32_t slot,std::uint32_t arg,
                                                  bool has_arg){
    if(user==nullptr)return 0u;
    auto& bridge=*static_cast<PcRuntimeCommitBridgeR078*>(user);
    return bridge.virtual_call?bridge.virtual_call(bridge.virtual_user,handle,slot,arg,has_arg):(outrun::driving::service_hole("runtime_commit_virtual_bridge_r078","bridge.virtual_call"),0u);
}

PcObjectRuntimeCommitServices runtime_commit_services_r078(PcRuntimeCommitBridgeR078& bridge){
    return {&bridge,runtime_commit_prepare_bridge_r078,runtime_commit_global_bridge_r078,
            runtime_commit_virtual_bridge_r078};
}

namespace {
void r079_action(const PcRuntimeTransitionServicesR079& s,std::uint32_t pc,Bytes object,
                 std::int32_t selected,std::uint32_t variant){
    if(s.call_action)s.call_action(s.user,pc,object,selected,variant);else outrun::driving::service_hole("r079_action","s.call_action");
}
void r079_this(const PcRuntimeOwnerServices445be0& s,std::uint32_t pc,Bytes object,
               std::size_t offset=0u){
    if(s.call_this)s.call_this(s.user,pc,object,offset);else outrun::driving::service_hole("r079_this","s.call_this");
}
void r079_timing(float timer,float scale,float old,float& now,float& delta){
    // PC 0x445BF0..0x445C18 keeps the x87 product live after FST stores the
    // rounded float to +0xD9C.  The subsequent FSUB therefore uses that live
    // register value rather than reloading the rounded +0xD9C value.
    const X87 product=X87(timer)*scale;                            // FLD; FMUL
    now=x87_float(product);                                        // FST m32
    delta=x87_float(product-old);                                  // FSUB; FSTP
}
}

bool object_runtime_dispatch_primary_445430(Bytes object,std::uint32_t result,
                                            std::int32_t selected_kind,
                                            const PcRuntimeTransitionServicesR079& services){
    switch(result){
        case 5u:return true;
        case 1u:r079_action(services,0x004450a0u,object,selected_kind,0u);break;
        case 2u:r079_action(services,0x00445160u,object,selected_kind,0u);break;
        case 3u:r079_action(services,0x004448c0u,object,selected_kind,0u);break;
        case 4u:r079_action(services,0x004442a0u,object,selected_kind,0u);break;
        case 6u:r079_action(services,0x00445310u,object,selected_kind,0u);break;
        default:break;
    }
    return false;
}

bool object_runtime_dispatch_secondary_4454b0(Bytes object,std::uint32_t result,
                                              std::int32_t selected_kind,
                                              const PcRuntimeTransitionServicesR079& services){
    switch(result){
        case 5u:return true;
        case 1u:case 2u:case 3u:
            r079_action(services,0x00445160u,object,selected_kind,1u);break;
        case 4u:r079_action(services,0x004442a0u,object,selected_kind,1u);break;
        default:break;
    }
    return false;
}

void runtime_loader_begin_48bf20(Bytes loader,
                                 const PcRuntimeLoaderServices48bf20& services){
    loader.check(0u,PcRuntimeLoaderState48bf20Size);
    loader.put32(0x10u,11u);
    loader.put32(0x18u,0u);
    loader.put32(0x14u,30u);
    loader.put32(0x1cu,11u);
    loader.put32(0x24u,0u);
    loader.put32(0x20u,30u);
    loader.put32(0x28u,11u);
    loader.put32(0x30u,0u);
    loader.put32(0x2cu,30u);
    if(services.request){
        services.request(services.user,0x00448ad0u,0xbau,2u);
        services.request(services.user,0x00448ad0u,0xbbu,2u);
    }else outrun::driving::service_hole("runtime_loader_begin_48bf20","services.request");
    loader.put32(0x04u,30u);
    loader.put32(0x08u,30u);
    loader.put32(0x0cu,30u);
    loader.put8(0x00u,1u);
}

bool object_runtime_loader_stage3_4455df(
    Bytes object,Bytes loader,const PcRuntimeLoaderServices48bf20& services){
    object.check(0x0518u,4u);
    if(object.u32(0x0000u)!=3u||object.i32(0x0518u)>0)return false;
    runtime_loader_begin_48bf20(loader,services);
    object.put32(0x0000u,4u);
    return true;
}

bool runtime_resource_request_448ad0(PcRuntimeResourceEntry448ad0& entry,
                                     std::uint32_t resource_id,
                                     std::uint32_t request_mode,
                                     std::uint32_t& pending_7cc1d8){
    if(resource_id>=0x223u||entry.ownership_10==1u||entry.status_14!=7u)return false;
    pending_7cc1d8=1u;
    entry.status_14=0u;
    entry.request_mode_30=request_mode;
    return true;
}

bool runtime_resource_ready_448980(std::uint32_t pending_7cc1d8){
    return pending_7cc1d8==0u;
}

bool object_runtime_loader_stage4_4455f2(Bytes object,
                                         std::uint32_t pending_7cc1d8){
    object.check(0x0518u,4u);
    if(object.u32(0x0000u)!=4u||object.i32(0x0518u)>0||
       !runtime_resource_ready_448980(pending_7cc1d8))return false;
    object.put32(0x0000u,5u);
    return true;
}

std::uint8_t runtime_shared_loader_49e580(
    PcRuntimeSharedLoaderState49e580& state,
    const PcRuntimeSharedLoaderServices49e580& services){
    const auto call=[&](std::uint32_t pc,std::initializer_list<std::uint32_t> args={}){
        return services.call?services.call(services.user,pc,args.begin(),args.size()):(outrun::driving::service_hole("runtime_shared_loader_49e580","services.call"),0u);
    };
    switch(state.stage_83db18){
        case 0u:
            (void)call(0x004f2470u);
            state.stage_83db18=1u;
            [[fallthrough]];
        case 1u:
            if(call(0x00427700u,{0x45a7u})!=0u||
               call(0x00427700u,{0x0aa4u})!=0u||
               call(0x00427700u,{0x4ba8u})!=0u)return 0u;
            state.stage_83db18=2u;
            [[fallthrough]];
        case 2u:
            (void)call(0x0042deb0u,{0x2cu,8u});
            (void)call(0x0042deb0u,{0x33u,8u});
            (void)call(0x0042deb0u,{0x48u,8u});
            state.stage_83db18=3u;
            [[fallthrough]];
        case 3u:
            if(call(0x0042df90u)==0u)return 0u;
            (void)call(0x00429920u,{0x2cu,8u});
            (void)call(0x00429920u,{0x33u,8u});
            (void)call(0x00429920u,{0x48u,8u});
            (void)call(0x00448ab0u);
            state.stage_83db18=4u;
            [[fallthrough]];
        case 4u:
            if(call(0x004299a0u)==0u||call(0x00448b90u)!=0u)return 0u;
            state.stage_83db18=5u;
            [[fallthrough]];
        case 5u:
            (void)call(0x0040ed70u);
            state.stage_83db18=6u;
            [[fallthrough]];
        case 6u:return 1u;
        default:return 0u;
    }
}

bool object_runtime_loader_stages6_to11_445627(
    Bytes object,const PcRuntimeTopLoaderServicesR110& services){
    object.check(0x0000u,4u);
    const auto call=[&](std::uint32_t pc,std::initializer_list<std::uint32_t> args={}){
        return services.call?services.call(services.user,pc,args.begin(),args.size()):(outrun::driving::service_hole("object_runtime_loader_stages6_to11_445627","services.call"),0u);
    };
    switch(object.u32(0x0000u)){
        case 6u:{
            if(call(0x0042df90u)==0u)return false;
            const auto file=call(0x004239c0u,{0x0059daecu,0x0062563cu});
            if(file!=0u){
                (void)call(0x00423cb0u,{0x00844a08u,0xb0u,0x80u,file});
                (void)call(0x00423bd0u,{file});
            }
            if(call(0x00427700u,{0u})!=0u)return false;
            (void)call(0x00496170u);
            (void)call(0x004e85e0u);
            (void)call(0x00429920u,{0x44u,9u});
            object.put32(0x0000u,7u);
            [[fallthrough]];
        }
        case 7u:
            if(call(0x004299a0u)==0u||call(0x00496180u)==0u||
               call(0x004e8620u)==0u)return false;
            object.put32(0x0000u,8u);
            [[fallthrough]];
        case 8u:
            if(call(0x00448980u)==0u)return false;
            object.put32(0x0000u,9u);
            [[fallthrough]];
        case 9u:
            object.put32(0x0000u,10u);
            [[fallthrough]];
        case 10u:
            object.put32(0x0000u,11u);
            [[fallthrough]];
        case 11u:
            object.put32(0x0000u,12u);
            return true;
        default:return false;
    }
}

void frontend_bulk_loader_initialize_4e85e0(
    PcFrontendBulkLoaderState4e8620& state){
    state.special_status.fill(1u);
    state.script_status.fill(1u);
}

bool frontend_bulk_loader_4e8620(
    PcFrontendBulkLoaderState4e8620& state,
    const PcFrontendBulkLoaderServicesR111& services){
    constexpr std::array<std::uint32_t,4> special_paths{{
        0x005ce138u,0x005ce114u,0x005ce0f0u,0x005ce0ccu}};
    constexpr std::array<std::uint32_t,4> special_keys{{
        0x005b457cu,0x005b4590u,0x005b4554u,0x005b4568u}};
    constexpr std::array<std::uint32_t,60> script_names{{
        0x005b4a14u,0x005b4a04u,0x005b49ecu,0x005b49e0u,0x005b49ccu,
        0x005b49b8u,0x005b49a8u,0x005b4994u,0x005b497cu,0x005b4970u,
        0x005b495cu,0x005b494cu,0x005b4938u,0x005b4928u,0x005b4914u,
        0x005b4908u,0x005b48fcu,0x005b48f0u,0x005b48e0u,0x005b48d4u,
        0x005b48c0u,0x005b48acu,0x005b48a0u,0x005b4890u,0x005b4884u,
        0x005b4878u,0x005b4868u,0x005b4854u,0x005b4848u,0x005b4834u,
        0x005b4820u,0x005b4810u,0x005b47f4u,0x005b47e4u,0x005b47d0u,
        0x005b47b8u,0x005b47a4u,0x005b4790u,0x005b4778u,0x005b4768u,
        0x005b4754u,0x005b4740u,0x005b472cu,0x005b471cu,0x005b4704u,
        0x005b46f8u,0x005b46e8u,0x005b46d8u,0x005b46c8u,0x005b46b8u,
        0x005b46a4u,0x005b4690u,0x005b4680u,0x005b466cu,0x005b4660u,
        0x005b4654u,0x005b4644u,0x005b4630u,0x005b4620u,0x005b460cu}};
    const auto call=[&](std::uint32_t pc,std::initializer_list<std::uint32_t> args={}){
        return services.call?services.call(services.user,pc,args.begin(),args.size()):(outrun::driving::service_hole("frontend_bulk_loader_4e8620","services.call"),0u);
    };
    for(std::size_t i=0;i<state.special_status.size();++i){
        if(state.special_status[i]>=3u)continue;
        const auto record=0x0084b3c0u+static_cast<std::uint32_t>(i*0x10u);
        if(call(0x004f12a0u,{special_paths[i],record,1u,1u})==0u)return false;
        state.special_status[i]=3u;
        state.special_handles[i]=call(0x004f1a90u,{special_keys[i],record});
    }
    for(std::size_t i=0;i<script_names.size();++i){
        const auto record=0x0084b400u+static_cast<std::uint32_t>(i*0x10u);
        (void)call(0x005802ddu,{record,0x005a4310u,script_names[i]});
        if(state.script_status[i]>=3u)continue;
        if(call(0x004f12a0u,{script_names[i],record,1u,1u})==0u)return false;
        state.script_status[i]=3u;
    }
    return true;
}

bool object_runtime_loader_stage12_4456ea(
    Bytes object,PcRuntimeTopLoaderStage12StateR112& state,
    const PcRuntimeTopLoaderStage12ServicesR112& services){
    object.check(0x051cu,1u);
    if(object.u32(0x0000u)!=12u)return false;
    const auto global=[&](std::uint32_t pc,std::initializer_list<std::uint32_t> args={}){
        return services.global_call?
            services.global_call(services.user,pc,args.begin(),args.size()):(outrun::driving::service_hole("object_runtime_loader_stage12_4456ea","services.global_call"),0u);
    };
    const auto call_this=[&](std::uint32_t pc,std::size_t offset,
                             std::initializer_list<std::uint32_t> args={}){
        return services.this_call?
            services.this_call(services.user,pc,object,offset,args.begin(),args.size()):(outrun::driving::service_hole("object_runtime_loader_stage12_4456ea","services.this_call"),0u);
    };

    if(state.mode_6319a1!=0u){
        if(global(0x00428880u,{state.optional_resource_631b38})!=3u)return false;
        (void)global(0x004285a0u,{state.optional_resource_631b38});
    }
    (void)global(0x004999f0u);
    (void)call_this(0x004470f0u,0x051cu);
    (void)call_this(0x00447000u,0x051cu,{0u,0u});

    if(state.alternate_7b17ec!=0u){
        (void)call_this(0x00444e10u,0u);
        (void)global(0x00401000u,{0u,0x1eu,1u});
    }else{
        const auto handle=call_this(0x00443eb0u,0u,{0u});
        if(handle==0u)return false;
        const auto ready=services.handle_virtual?
            services.handle_virtual(services.user,handle,0x04u):(outrun::driving::service_hole("object_runtime_loader_stage12_4456ea","services.handle_virtual"),0u);
        if(std::uint8_t(ready)==0u){
            (void)call_this(0x00441200u,0u,{handle});
            (void)call_this(0x00443c30u,0u);
            return false;
        }
        const auto count=object.u32(0x0484u);
        if(count>=128u)throw std::out_of_range("r112 top-loader callback list capacity");
        object.put32(0x0284u+std::size_t(count)*4u,handle);
        object.put32(0x0484u,count+1u);
    }

    state.mode_6319a1=0u;
    object_transition_init_440ef0(object,state.transition_globals);
    (void)global(0x0048c370u);
    (void)global(0x004edce0u,{0x00659930u,2u});
    object.put32(0x0000u,13u);
    if(state.optional_index_7b17f8!=-1&&(state.optional_flags_7c27d4&2u)!=0u)
        (void)global(0x00416420u);
    object.put32(0x0000u,14u);
    return true;
}

std::uint8_t object_runtime_owner_445be0(Bytes object,
                                         PcObjectRuntimeSnapshot444350& snapshot,
                                         PcObjectSelectorGlobals& selector_globals,
                                         const PcRuntimeOwnerInputs445be0& inputs,
                                         const PcRuntimeOwnerServices445be0& services){
    object.check(0x0da0u,4u);
    const float old=object.f32(0x0d9cu);
    object.putf(0x0d98u,old);
    float now{},delta{};
    r079_timing(inputs.timer_value_842110,inputs.scale_62812c,old,now,delta);
    object.putf(0x0d9cu,now);
    object.putf(0x0da0u,delta);

    const auto state=object.u32(0x0218u);
    switch(state){
        case 0u:return 1u;
        case 1u:
            r079_this(services,0x00445500u,object);
            if(object.u32(0x0000u)==0x0eu)object.put32(0x0218u,2u);
            return 1u;
        case 2u:{
            r079_this(services,0x00441020u,object);
            r079_this(services,0x00443110u,object);
            r079_this(services,0x00444840u,object);
            r079_this(services,0x00446cf0u,object,0x051cu);
            r079_this(services,0x00445810u,object);
            r079_this(services,0x00444530u,object);
            r079_this(services,0x00445a50u,object);
            r079_this(services,0x004411a0u,object);
            std::int32_t selected=-1;
            const auto result=object_select_primary_442cb0(object,selected,selector_globals,
                                                            services.selector_services);
            if(!object_runtime_dispatch_primary_445430(object,result,selected,
                                                        services.transition_services))return 1u;
            object_runtime_commit_444350(object,snapshot,services.state_services,
                                         services.runtime_services,services.commit_services);
            return 0u;
        }
        case 3u:{
            r079_this(services,0x00444470u,object);
            r079_this(services,0x00444650u,object);
            r079_this(services,0x00446cf0u,object,0x051cu);
            std::int32_t selected=-1;
            const auto result=object_select_secondary_442d70(object,selected,
                                                              services.selector_services);
            if(!object_runtime_dispatch_secondary_4454b0(object,result,selected,
                                                          services.transition_services))return 1u;
            object_release_flagged_handles_4430b0(object,services.runtime_services);
            return 0u;
        }
        case 4u:{
            const auto depth=object.i8(0x0220u);
            object.put32(0x0214u,0u);
            if(depth<0||depth>=32)throw std::out_of_range("r079 history depth");
            object.put8(0x0221u+std::size_t(depth),1u);
            object.put8(0x0241u+std::size_t(depth),0u);
            object.put32(0x0218u,3u);
            return 1u;
        }
        default:return 1u;
    }
}

namespace {
void r080_tick_call(const PcUiResourceTickServices4659f0& s,std::uint32_t pc,Bytes resource){
    if(s.call)s.call(s.user,pc,resource);else outrun::driving::service_hole("r080_tick_call","s.call");
}
}

void ui_resource_tick_4659f0(Bytes resource,
                             const PcUiNotifyServices& ui_services,
                             const PcUiResourceTickServices4659f0& services){
    resource.check(0x9cu,4u);
    if(resource.u32(0x08u)==0xffffffffu)return;

    if(resource.u32(0x24u)==0u&&resource.u32(0x18u)==1u&&resource.u32(0x1cu)!=0u&&
       ui_resource_ready_4652e0(resource,ui_services)!=0u){
        const auto saved_2c=resource.u32(0x2cu);
        ui_resource_reset_465250(resource,ui_services);
        if(saved_2c!=0u){
            resource.put32(0x10u,resource.u32(0x34u));
            resource.put32(0x18u,0u);
            resource.put32(0x0cu,resource.u32(0x30u));
            resource.put32(0x3cu,resource.u32(0x38u));
            r080_tick_call(services,0x00465970u,resource);
        }
    }

    if(resource.u32(0x20u)!=0u)return;
    if(resource.u32(0x9cu)==0u&&resource.u32(0x88u)==0u&&resource.u32(0x8cu)==0u)return;
    if(resource.u32(0x88u)!=0u)r080_tick_call(services,0x00465590u,resource);
    if(resource.u32(0x8cu)!=0u)r080_tick_call(services,0x00465460u,resource);
    r080_tick_call(services,0x004656b0u,resource);
}

void object_runtime_ui_tick_444840(Bytes object,
                                   const PcNativeHandleResolver& handles,
                                   const PcUiNotifyServices& ui_services,
                                   const PcUiResourceTickServices4659f0& tick_services,
                                   const PcRuntimeUiTickServices444840& services){
    object.check(0x0d94u,1u);
    object.check(0x0cfcu,4u);
    auto resource=object.sub(0x0cf4u,object.size()-0x0cf4u);
    ui_resource_tick_4659f0(resource,ui_services,tick_services);
    if(object.u32(0x0cfcu)!=0xffffffffu||object.u8(0x0d94u)==0u)return;
    const auto index=std::size_t(object.u32(0x0484u));
    if(index>(std::numeric_limits<std::size_t>::max()-0x0280u)/4u)
        throw std::out_of_range("r080 handle index overflow");
    const auto off=0x0280u+index*4u;
    object.check(off,4u);
    const auto token=object.u32(off);
    const auto key=native_handle_state_564c90(handles,token);
    if(services.open_state)services.open_state(services.user,0x004447d0u,object,key);else outrun::driving::service_hole("pc_common_control.cpp:5341","services.open_state");
}

void ui_resource_configure_465860(Bytes resource,
                                  const std::array<std::uint32_t,11>& a,
                                  const PcUiNotifyServices& ui_services){
    resource.check(0x09cu,4u);
    ui_resource_reset_465250(resource,ui_services);
    resource.put32(0x00u,a[0]);
    resource.put32(0x04u,0xffffffffu);
    resource.put32(0x0cu,a[1]);
    resource.put32(0x10u,a[2]);
    resource.put32(0x14u,a[3]);
    resource.put32(0x18u,a[4]);
    resource.put32(0x1cu,0u);
    resource.put32(0x20u,0u);
    resource.put32(0x24u,a[10]);
    resource.put32(0x28u,0u);
    resource.put32(0x2cu,0u);
    resource.put32(0x30u,0u);
    resource.put32(0x34u,0u);
    if(a[4]==3u){
        resource.put32(0x18u,1u);
        resource.put32(0x1cu,1u);
    }
    resource.put32(0x40u,a[5]);
    resource.put32(0x44u,a[6]);
    resource.put32(0x48u,0u);
    resource.put32(0x4cu,resource.u32(0x40u));
    resource.put32(0x50u,resource.u32(0x44u));
    resource.put32(0x54u,resource.u32(0x48u));
    resource.put32(0x64u,a[7]);
    resource.put32(0x68u,a[8]);
    resource.put32(0x6cu,0x3f800000u);
    resource.put32(0x70u,resource.u32(0x64u));
    resource.put32(0x74u,resource.u32(0x68u));
    resource.put32(0x78u,resource.u32(0x6cu));
    resource.put32(0x3cu,a[9]);
    resource.put32(0x88u,0u);
    resource.put32(0x8cu,0u);
    resource.put32(0x98u,0u);
    resource.put32(0x9cu,0u);
}

namespace {
std::uint32_t r114_resource_call(const PcUiResourceCommitServices465970& s,
                                 std::uint32_t pc,
                                 std::initializer_list<std::uint32_t> args){
    if(!s.call){outrun::driving::service_hole("r114_resource_call","s.call");return 0u;}
    return s.call(s.user,pc,args.begin(),args.size());
}
void r114_resource_finalize(const PcUiResourceCommitServices465970& s,Bytes resource){
    if(s.finalize)s.finalize(s.user,0x004656b0u,resource);else outrun::driving::service_hole("r114_resource_finalize","s.finalize");
}
}

void ui_resource_commit_465970(Bytes resource,
                               const PcUiResourceCommitServices465970& services){
    resource.check(0x3cu,4u);
    if(resource.u32(0x24u)!=0u){
        resource.put32(0x08u,0u);
        r114_resource_finalize(services,resource);
        return;
    }

    std::uint32_t handle{};
    if(resource.u32(0x0cu)!=0xffffffffu&&resource.u32(0x10u)!=0xffffffffu){
        handle=r114_resource_call(services,0x00428460u,
            {resource.u32(0x00u),resource.u32(0x14u),resource.u32(0x18u),
             resource.u32(0x0cu),resource.u32(0x10u)});
    }else{
        handle=r114_resource_call(services,0x00428320u,
            {resource.u32(0x00u),resource.u32(0x14u),resource.u32(0x18u)});
    }
    resource.put32(0x08u,handle);
    if(handle==0xffffffffu)return;
    (void)r114_resource_call(services,0x00428800u,
                             {handle,resource.u32(0x3cu)});
    r114_resource_finalize(services,resource);
}

bool object_runtime_ui_open_known_4447d0(
    Bytes object,std::uint32_t key,Bytes related,
    const PcObjectStateServices& related_services,
    const PcRuntimeUiOpenServices4447d0& services){
    const auto token=object_state_code_443ff0(object,key,related,related_services);
    if(token==0xffffffffu){
        if(services.invalid_tail)
            services.invalid_tail(services.invalid_user,0x01039cc4u,object,key);else outrun::driving::service_hole("object_runtime_ui_open_known_4447d0","services.invalid_tail");
        return false;
    }
    object.check(0x0cf4u,0xa0u);
    auto resource=object.sub(0x0cf4u,0xa0u);
    ui_resource_reset_465250(resource,services.ui_services);
    const std::array<std::uint32_t,11> args{{
        token,0u,0u,0u,0u,0u,0u,
        0x3f800000u,0x3f800000u,0x3f800000u,0u}};
    ui_resource_configure_465860(resource,args,services.ui_services);
    ui_resource_commit_465970(resource,services.commit_services);
    return true;
}

namespace {
std::uint32_t r081_embedded_effect(std::uint32_t key){
    for(const auto& e:R075EmbeddedEffectMap)if(e.first==key)return e.second;
    return 0xffffffffu;
}
void r081_embedded_slot_open(Bytes embedded,std::uint32_t slot,std::uint32_t pc_entry,
                             bool clear_primary,
                             const PcEmbeddedSlotsServices446cf0& services){
    if(slot>=4u)throw std::out_of_range("r081 embedded slot");
    const auto child_off=0x040cu+std::size_t(slot)*0xa0u;
    embedded.check(child_off,0xa0u);
    Bytes child=embedded.sub(child_off,0xa0u);

    // Both original bodies call 0x465250 before looking up the current entry.
    ui_resource_reset_465250(child,services.ui_services);

    const auto depth=std::size_t(embedded.u32(0x0408u));
    if(depth>(std::numeric_limits<std::size_t>::max()-0x08u)/0x20u)
        throw std::out_of_range("r081 embedded depth overflow");
    const auto key_off=0x08u+depth*0x20u+std::size_t(slot)*4u;
    embedded.check(key_off,4u);
    const auto key=embedded.u32(key_off);
    if(key==0xffffffffu)return;
    const auto effect=r081_embedded_effect(key);
    if(effect==0xffffffffu)return;
    const auto [x,y]=R075EmbeddedCoords[slot];
    if(services.configure_ui)
        services.configure_ui(services.user,pc_entry,child,effect,slot,x,y);else outrun::driving::service_hole("r081_embedded_slot_open","services.configure_ui");
    if(services.finalize_ui)
        services.finalize_ui(services.user,0x00465970u,child);else outrun::driving::service_hole("r081_embedded_slot_open","services.finalize_ui");
    if(clear_primary)embedded.put8(0x01u+std::size_t(slot),0u);
}
}

void embedded_slot_open_primary_446a80(Bytes embedded,std::uint32_t slot,
                                       const PcEmbeddedSlotsServices446cf0& services){
    r081_embedded_slot_open(embedded,slot,0x00446a80u,true,services);
}

void embedded_slot_open_secondary_446b20(Bytes embedded,std::uint32_t slot,
                                         const PcEmbeddedSlotsServices446cf0& services){
    r081_embedded_slot_open(embedded,slot,0x00446b20u,false,services);
}
void embedded_slot_close_446c50(Bytes b,std::uint32_t slot,const PcEmbeddedSlotsServices446cf0& s){
    if(slot>=4||b.u32(0x408)>=32)throw std::out_of_range("embedded close slot/depth");
    auto child=b.sub(0x40c+slot*0xa0,0xa0);if(child.u32(8)==~0u)return;
    ui_resource_reset_465250(child,s.ui_services);
    const auto effect=r081_embedded_effect(b.u32(8+b.u32(0x408)*0x20+slot*4));if(effect==~0u)return;
    const auto [x,y]=R075EmbeddedCoords[slot];
    if(s.configure_ui)s.configure_ui(s.user,0x446c50,child,effect,slot,x,y);else outrun::driving::service_hole("embedded_slot_close_446c50","s.configure_ui");
    if(s.finalize_ui)s.finalize_ui(s.user,0x465970,child);else outrun::driving::service_hole("embedded_slot_close_446c50","s.finalize_ui");
}
bool embedded_slots_set_446d90(Bytes b,const std::array<std::uint32_t,8>& args,PcUiNotifyGlobals& g,const PcEmbeddedSlotsServices446cf0& s){
    b.check(0,0x68c);const auto depth=b.u32(0x408);if(depth>=32)throw std::out_of_range("embedded set depth");
    const auto at=8+depth*0x20;
    for(unsigned i=0;i<4;++i)if(b.u32(at+i*4)!=args[2*i]){
        embedded_slot_close_446c50(b,i,s);b.put8(1+i,1);b.put32(at+i*4,args[2*i]);
    }
    for(unsigned i=0;i<4;++i)b.put32(at+16+i*4,args[2*i+1]);
    embedded_slots_tick_446cf0(b,g,s);return true;
}
void embedded_slots_push_446fc0(Bytes b,PcUiNotifyGlobals& g,const PcEmbeddedSlotsServices446cf0& s){
    const auto depth=b.u32(0x408);if(depth>=32)throw std::out_of_range("embedded push depth");if(depth==31)return;
    std::array<std::uint32_t,8> args;const auto at=8+depth*0x20;
    for(unsigned i=0;i<4;++i){args[2*i]=b.u32(at+i*4);args[2*i+1]=b.u32(at+16+i*4);}
    b.put32(0x408,depth+1);embedded_slots_set_446d90(b,args,g,s);
}
bool embedded_slots_select_446ea0(Bytes b,std::uint32_t key,PcUiNotifyGlobals& g,const PcEmbeddedSlotsServices446cf0& s){
    // Original 0x632228 table, grouped only where all eight arguments match.
    // Absent keys (including 21) must not receive a synthetic default command.
    constexpr auto none=0xffffffffu;
    std::array<std::uint32_t,8> a{4,0x295,none,none,none,none,8,0x296};
    switch(key){
    case 0:case 0x0e:case 0x2c:case 0x2e:case 0x2f:a.fill(none);break;
    case 1:case 2:case 3:case 5:case 6:case 7:case 8:
    case 0x36:case 0x37:case 0x38:case 0x3a:case 0x3b:case 0x3c:
        a={4,0x295,0x8000,0x297,0x4000,0x29a,8,0x296};break;
    case 4:a[2]=0x10;a[3]=0x29c;break;
    case 0x0a:a[2]=0x10;a[3]=0x29b;break;
    case 0x0f:a[6]=none;a[7]=none;break;
    case 0x2b:a[4]=0x4000;a[5]=0x29a;break;
    case 0x10:case 0x11:case 0x13:case 0x14:case 0x16:case 0x17:
    case 0x18:case 0x19:case 0x1a:case 0x1d:case 0x1e:case 0x1f:
    case 0x20:case 0x22:case 0x28:case 0x29:case 0x2a:break;
    default:return false;
    }
    return embedded_slots_set_446d90(b,a,g,s);
}
void embedded_slots_visible_447000(Bytes b,std::uint8_t visible,std::uint8_t immediate,const PcEmbeddedSlotsServices446cf0& s){
    b.check(0,0x68c);
    if(immediate){for(unsigned i=0;i<4;++i){ui_resource_reset_465250(b.sub(0x40c+i*0xa0,0xa0),s.ui_services);b.put8(1+i,1);}}
    else if(visible!=b.u8(0)){for(unsigned i=0;i<4;++i){if(!visible)embedded_slot_close_446c50(b,i,s);b.put8(1+i,visible?1:0);}}
    b.put8(0,visible);
}
void embedded_slots_clear_447090(Bytes b,PcUiNotifyGlobals& g,const PcEmbeddedSlotsServices446cf0& s){
    std::array<std::uint32_t,8> args;args.fill(~0u);embedded_slots_set_446d90(b,args,g,s);
    for(unsigned i=0;i<4;++i){ui_resource_reset_465250(b.sub(0x40c+i*0xa0,0xa0),s.ui_services);b.put8(1+i,0);}
    b.put32(0x408,0);b.put8(0,0);
}

void embedded_slots_tick_446cf0(Bytes embedded,PcUiNotifyGlobals& globals,
                                const PcEmbeddedSlotsServices446cf0& services){
    // The largest directly accessed field is the tail of slot 3 at +0x68B.
    embedded.check(0x068bu,1u);
    for(std::uint32_t slot=0;slot<4u;++slot){
        const auto depth=std::size_t(embedded.u32(0x0408u));
        if(depth>(std::numeric_limits<std::size_t>::max()-0x18u)/0x20u)
            throw std::out_of_range("r081 embedded history overflow");
        const auto hist_off=0x18u+depth*0x20u+std::size_t(slot)*4u;
        embedded.check(hist_off,4u);
        const auto marker=embedded.u32(hist_off);
        if(marker==0x297u||marker==0x298u)
            embedded.put32(hist_off,globals.alternate!=0u?0x297u:0x298u);

        const auto child_off=0x040cu+std::size_t(slot)*0xa0u;
        Bytes child=embedded.sub(child_off,0xa0u);
        ui_resource_tick_4659f0(child,services.ui_services,services.tick_services);

        if(embedded.u8(0x00u)==0u)continue;
        if(child.u32(0x08u)!=0xffffffffu&&
           ui_resource_ready_4652e0(child,services.ui_services)==0u)continue;
        if(embedded.u8(0x01u+std::size_t(slot))!=0u)
            embedded_slot_open_primary_446a80(embedded,slot,services);
        else
            embedded_slot_open_secondary_446b20(embedded,slot,services);
    }
}

void object_runtime_gate_445810(
    Bytes object,Bytes current_object,
    const PcNativeHandleResolver& handles,
    const PcRuntimeGate445810Inputs& gate_inputs,
    const PcObjectStatePairTable& pair_table,
    const PcObjectEventCallbackTable& callback_table,
    const PcObjectEventModeInputs& event_inputs,
    const PcObjectEventDispatch443eb0Services& dispatch_services,
    const PcRuntimeControlServices& runtime_services,
    const PcObjectOpenCallbackServices& open_services,
    const PcRuntimeGate445810Services& gate_services){
    object.check(0x051cu,1u);
    current_object.check(0x048cu,1u);

    // PC 0x445817 calls the lazy current-object accessor and then crosses an
    // obfuscated transfer.  At the re-entry point the value consumed is the
    // current object's handle at +0x488; make that dependency explicit here.
    const auto current_handle=current_object.u32(0x0488u);
    if(current_handle!=0u&&current_object.u8(0x048cu)!=0u&&
       native_handle_state_564c90(handles,current_handle)==0x1bu)return;
    if(object.i32(0x0518u)>0)return;
    if(runtime_system_handle_active_4999c0(gate_inputs.system_handle_67f614))return;
    const auto menu=runtime_menu_state_450240(gate_inputs.menu_state_7d38f0);
    if(menu==7u||menu==6u)return;

    std::uint32_t selector=0xffffffffu;
    const auto state=object.u32(0x0218u);
    if(runtime_feature_mask_4536f0(gate_inputs.player0_feature_mask,0x20u)!=0u){
        if(state==2u&&gate_services.configure_446f30)
            gate_services.configure_446f30(gate_services.user,
                                           object.sub(0x051cu,object.size()-0x051cu),0x4000u,1u);
        selector=0x4000u;
    }else if(runtime_feature_mask_4536f0(gate_inputs.player0_feature_mask,0x10u)!=0u){
        if(state==2u&&gate_services.configure_446f30)
            gate_services.configure_446f30(gate_services.user,
                                           object.sub(0x051cu,object.size()-0x051cu),0x8000u,1u);
        selector=0x8000u;
    }

    const auto depth=std::int32_t(object.i8(0x0220u));
    if(depth<0||depth>=32)throw std::out_of_range("r082 history depth");
    if(state==2u&&object.u8(0x0241u+std::size_t(depth))!=0u&&selector==0x4000u){
        const auto index=std::size_t(object.u32(0x0484u));
        if(index>(std::numeric_limits<std::size_t>::max()-0x0280u)/4u)
            throw std::out_of_range("r082 handle index overflow");
        object.check(0x0280u+index*4u,4u);
        const auto handle=object.u32(0x0280u+index*4u);
        if(native_handle_state_564c90(handles,handle)!=0x15u){
            if(gate_services.action_444fe0)
                gate_services.action_444fe0(gate_services.user,0x00444fe0u,object,0x15u);else outrun::driving::service_hole("pc_common_control.cpp:5619","gate_services.action_444fe0");
            return;
        }
    }

    // Both branches in the retail EXE (445938 and 445944) target the
    // epilogue at 4459EB, NOT the overlay-open block at 445994.
    if(object.u8(0x0221u+std::size_t(depth))==0u||selector!=0x8000u)return;
    if(state==3u){
        if(gate_inputs.game_mode_78026c!=0x10u&&gate_inputs.game_mode_78026c!=0x0au)return;
        // Original calls 0x43F9E0(1), then conditionally 0x440930, before
        // normalizing object+0x21C. Their externally visible state is modeled
        // by the explicit inputs; the owner-side globals stay outside this body.
        if(gate_inputs.game_mode_78026c!=0x0au&&gate_inputs.game_mode_78026c!=0x12u)
            object.put32(0x021cu,0x12u);
    }

    {
        const auto own_handle=object.u32(0x0488u);
        if(own_handle!=0u){
            const auto hs=native_handle_state_564c90(handles,own_handle);
            if(hs==0x20u||hs==0x16u||hs==0x21u)return;
        }
        const auto key=gate_inputs.alternate_7d68bc?0x21u:0x16u;
        object_open_callback_443fa0(object,key,pair_table,callback_table,event_inputs,
                                    dispatch_services,runtime_services,open_services);
    }
}


void object_runtime_transition_444530(
    Bytes object,
    PcRuntimeTransitionFlags444530& flags,
    const PcRuntimeControlGlobals& runtime_globals,
    const PcNativeHandleResolver& handles,
    const PcObjectStatePairTable& pair_table,
    const PcObjectEventCallbackTable& callback_table,
    const PcObjectEventModeInputs& event_inputs,
    const PcObjectEventDispatch443eb0Services& dispatch_services,
    const PcRuntimeControlServices& runtime_services,
    const PcObjectOpenCallbackServices& open_services){
    object.check(0x0495u,1u);

    const auto ready=runtime_ready_441260(object,runtime_globals,runtime_services)!=0u;
    std::uint32_t desired_state=0u;
    if(flags.request_7d68cb!=0u){
        flags.request_7d68cb=0u;
        flags.active_7d68d0=1u;
        desired_state=0x0fu;
    }else{
        if(!ready)return;
        desired_state=0x0eu;
    }

    const auto previous=object.u32(0x0490u);
    if(previous!=0u){
        if(native_handle_state_564c90(handles,previous)==desired_state)return;
        if(native_handle_state_564c90(handles,previous)==0x0fu)return;
    }

    const auto created=object_dispatch_callback_443eb0(
        object,desired_state,pair_table,callback_table,event_inputs,dispatch_services);
    if(created==0u)return;

    if(object.u8(0x0494u)!=0u){
        if(native_handle_state_564c90(handles,created)!=0x0fu){
            runtime_release_handle_441200(object,created,runtime_services);
            return;
        }
        // The original asks the newly-created handle for its state a second
        // time before comparing it with the current +0x490 handle. Preserve
        // that call shape even when a resolver would normally be immutable.
        const auto created_state=native_handle_state_564c90(handles,created);
        const auto previous_state=native_handle_state_564c90(handles,object.u32(0x0490u));
        if(created_state==previous_state)return;
        if(object.u8(0x0494u)!=0u||object.u8(0x048cu)!=0u)
            object_release_flagged_handles_4430b0(object,runtime_services);
    }

    object.put32(0x0490u,created);
    const auto keep=open_services.handle_ready?
        open_services.handle_ready(open_services.user,created,4u):(outrun::driving::service_hole("pc_common_control.cpp:5700","open_services.handle_ready"),0u);
    if(std::uint8_t(keep)==0u){
        runtime_release_handle_441200(object,object.u32(0x0490u),runtime_services);
        object.put32(0x0490u,0u);
        return;
    }
    object.put8(0x0494u,1u);
}

std::uint8_t runtime_pair_idle_434de0(Bytes manager){
    manager.check(0x16d8u,4u);
    return (manager.u32(0x16d4u)==0u&&manager.u32(0x16d8u)==0u)?1u:0u;
}

void object_runtime_open_4459f0(
    Bytes object,std::uint8_t direct_action,
    const PcObjectStatePairTable& pair_table,
    const PcObjectEventCallbackTable& callback_table,
    const PcObjectEventModeInputs& event_inputs,
    const PcObjectEventDispatch443eb0Services& dispatch_services,
    const PcRuntimeControlServices& runtime_services,
    const PcObjectOpenCallbackServices& open_services,
    const PcRuntimeOpen4459f0Services& action_services){
    if(direct_action!=0u){
        if(action_services.action_444fe0)
            action_services.action_444fe0(action_services.user,0x00444fe0u,object,0x2eu);else outrun::driving::service_hole("pc_common_control.cpp:5725","action_services.action_444fe0");
        return;
    }
    object_open_callback_443fa0(object,0x2eu,pair_table,callback_table,event_inputs,
                                dispatch_services,runtime_services,open_services);
}

void object_runtime_queue_445a50(
    Bytes object,Bytes current_object,Bytes manager_988f40,Bytes ui_lookup_table,
    PcRuntimeQueueState445a50& queue,
    const PcRuntimeControlGlobals& runtime_globals,
    const PcObjectStatePairTable& pair_table,
    const PcObjectEventCallbackTable& callback_table,
    const PcObjectEventModeInputs& event_inputs,
    const PcObjectEventDispatch443eb0Services& dispatch_services,
    const PcRuntimeControlServices& runtime_services,
    const PcObjectOpenCallbackServices& open_services,
    const PcRuntimeOpen4459f0Services& action_services,
    const PcRuntimeQueueServices445a50& queue_services){
    object.check(0x0495u,1u);
    current_object.check(0x0218u,4u);
    if(queue.gate_7d68d2!=0u)return;
    if(queue.alternate_7d68bc==0u)return;
    if(current_object.u32(0x0218u)!=2u)return;
    if(runtime_pair_idle_434de0(manager_988f40)==0u)return;
    if(object.u8(0x0494u)!=0u)return;
    if(runtime_ready_441260(object,runtime_globals,runtime_services)!=0u)return;
    if(queue.request_7d68cb!=0u)return;
    if(queue.gate_988f44==1u)return;
    if(queue.active_989318!=0u)return;
    if(queue.busy_98a5f4!=0u)return;

    for(std::size_t i=0;i<PcRuntimeQueueSlotCount445a50;++i){
        Bytes slot(queue.slots[i].data(),queue.slots[i].size());
        if(slot.u8(0x29u)!=0u)continue;
        slot.put8(0x29u,1u);
        queue.busy_98a5f4=1u;
        const auto type=slot.u32(0x00u);
        std::uint32_t format_index=0u;
        std::uint32_t mode=0u;
        if(type==5u){
            queue.out_989320=slot.u32(0x08u);
            queue.out_989324=slot.u32(0x0cu);
            queue.out_989328=slot.u32(0x21u);
            queue.out_98932c=slot.u32(0x25u);
            format_index=0x341u;
            mode=1u;
        }else if(type==1u){
            queue.out_989320=slot.u32(0x08u);
            queue.out_989324=slot.u32(0x0cu);
            format_index=0x340u;
            mode=0u;
        }else{
            continue;
        }
        const auto format_token=ui_table_lookup_465eb0(ui_lookup_table,format_index);
        if(queue_services.format_emit)
            queue_services.format_emit(queue_services.user,0x00492690u,mode,format_token,
                                       slot,0x10u,-1,0x0au);else outrun::driving::service_hole("pc_common_control.cpp:5782","queue_services.format_emit");
        object_runtime_open_4459f0(object,0u,pair_table,callback_table,event_inputs,
                                   dispatch_services,runtime_services,open_services,
                                   action_services);
        return;
    }
}

std::uint8_t runtime_owner_entry_49e4a0(
    Bytes current_object,
    PcObjectRuntimeSnapshot444350& snapshot,
    PcObjectSelectorGlobals& selector_globals,
    const PcRuntimeOwnerInputs445be0& inputs,
    const PcRuntimeOwnerServices445be0& services){
    return object_runtime_owner_445be0(current_object,snapshot,selector_globals,inputs,services);
}

std::uint32_t runtime_init_entry_49e490(
    Bytes current_object,
    std::array<PcObjectEventCallbackEntry,PcObjectEventCallbackCount>& callback_table,
    std::uint8_t global_mode_6319a1,const PcObjectRuntimeInitServices& services){
    return object_runtime_init_4436c0(current_object,callback_table,global_mode_6319a1,services);
}

void runtime_display_entry_49e4b0(Bytes current_object,const PcObjectStateServices& services){
    object_dispatch_state_442e00(current_object,services);
}

std::uint32_t runtime_teardown_alias_443e90(
    Bytes current_object,const PcObjectRuntimeTeardownServices& services){
    return object_runtime_teardown_443c30(current_object,services);
}

std::uint32_t runtime_destroy_entry_49e4c0(
    Bytes current_object,const PcObjectRuntimeTeardownServices& services){
    return runtime_teardown_alias_443e90(current_object,services);
}

void select_pl_car_ctrl_486942(Bytes event,PcEventControlState& state,std::int32_t command){
    event.check(0x04,4);
    if(command==0){
        change_ctrl_func_440bb0(state,8u,0x004a8330u);
        event.put32(0x04,event.u32(0x04)&~0x00800000u);
    }else if(command==1){
        change_ctrl_func_440bb0(state,8u,0x00475720u);
        event.put32(0x04,event.u32(0x04)|0x00800000u);
    }
}

namespace {
inline std::uint32_t bootstrap_call_417810(
    const PcBootstrapServices417810& services,std::uint32_t entry,
    std::uint32_t a0=0u,std::uint32_t a1=0u,std::uint32_t a2=0u){
    return services.call?services.call(services.user,entry,a0,a1,a2):(outrun::driving::service_hole("bootstrap_call_417810","services.call"),0u);
}
}

std::uint32_t runtime_bootstrap_417810(
    PcEventControlState& event_state,
    const std::array<PcEventInitDescriptor,PcEventSlotCount>& descriptors,
    const std::array<PcEventFunctionDescriptor,PcEventFunctionTableCount>& functions,
    const PcBootstrapServices417810& services){
    bootstrap_call_417810(services,0x4487d0u);
    bootstrap_call_417810(services,0x429b60u);
    bootstrap_call_417810(services,0x413b30u);
    bootstrap_call_417810(services,0x43f880u);
    init_event_control_440bf0(event_state,descriptors,functions);
    bootstrap_call_417810(services,0x448ce0u);
    bootstrap_call_417810(services,0x44f7d0u,0u);
    bootstrap_call_417810(services,0x49a650u);
    // Virtual call at 0x41784F: this, 0x623830, 0x95AF9C.
    bootstrap_call_417810(services,0x41784fu,services.system_token,0x623830u,0x95af9cu);
    bootstrap_call_417810(services,0x414ce0u);
    // Virtual call at 0x41786C: this, 0x623E38, 0x955A40.
    bootstrap_call_417810(services,0x41786cu,services.system_token,0x623e38u,0x955a40u);
    bootstrap_call_417810(services,0x49a650u);
    bootstrap_call_417810(services,0x45ae10u);
    bootstrap_call_417810(services,0x44a630u);
    bootstrap_call_417810(services,0x416a40u);
    // Original tail-jump at 0x417886; preserve the callee return value.
    return bootstrap_call_417810(services,0x448730u);
}

std::uint32_t runtime_platform_init_417740(
    PcPlatformInitState417740& state,
    std::uint32_t primary_system_token,
    const PcPlatformInitServices417740& services){
    const auto call=[&](std::uint32_t pc,std::uint32_t a0=0u,std::uint32_t a1=0u,std::uint32_t a2=0u){
        return services.call?services.call(services.user,pc,a0,a1,a2):(outrun::driving::service_hole("runtime_platform_init_417740","services.call"),0u);
    };
    state.global_8a8ce0=0u;
    state.global_8a8cac=0u;
    call(0x49a650u);
    if(call(0x40e470u,state.window_token)==0u)return 0u;
    call(0x409bb0u);
    // Virtual system call at 0x417779: this, 9, &global 0x89F680.
    call(0x417779u,primary_system_token,9u,0x89f680u);
    state.global_89f684=0u;
    state.global_89f66c=0u;
    call(0x4231e0u,2u,1u,0x20u);
    call(0x42ed00u);
    call(0x403de0u);
    state.scratch_8999c0.fill(0u);
    call(0x4535f0u);
    call(0x49a650u,0u);
    call(0x453440u);
    call(0x45acb0u);
    call(0x4493d0u,state.resource_token_740ca0);
    call(0x465df0u);
    call(0x427db0u);
    // CreateThread(NULL,0,0x424090,NULL,0,NULL) then SetThreadPriority(h,0).
    state.thread_token=call(0x4177e6u,0x424090u,0u,0u);
    call(0x4177f3u,state.thread_token,0u,0u);
    return 1u;
}

std::uint32_t runtime_frame_ticks_417890(
    PcRuntimeTimingState417890& state,
    std::int32_t target_rate,
    const PcRuntimeTimingServices417890& services){
    auto from_bits=[](std::uint64_t u){std::int64_t v{};std::memcpy(&v,&u,sizeof(v));return v;};
    auto to_bits=[](std::int64_t v){std::uint64_t u{};std::memcpy(&u,&v,sizeof(u));return u;};
    const auto wrap_add=[&](std::int64_t a,std::int64_t b){return from_bits(to_bits(a)+to_bits(b));};
    const auto wrap_sub=[&](std::int64_t a,std::int64_t b){return from_bits(to_bits(a)-to_bits(b));};
    const auto wrap_mul=[&](std::int64_t a,std::int64_t b){return from_bits(to_bits(a)*to_bits(b));};
    if(services.query){
        state.frequency_8a8c98=services.query(services.user,0x596104u);
        state.current_counter_8a8ca0=services.query(services.user,0x5960fcu);
    }
    if(target_rate==0||state.frequency_8a8c98==0)throw std::invalid_argument("runtime_frame_ticks_417890 zero divisor");
    const auto delta=wrap_sub(state.current_counter_8a8ca0,state.previous_counter_8a8c90);
    state.accumulator_8a8cd0=wrap_add(state.accumulator_8a8cd0,delta);
    state.previous_counter_8a8c90=state.current_counter_8a8ca0;
    const auto frames=wrap_mul(static_cast<std::int64_t>(target_rate),state.accumulator_8a8cd0)/state.frequency_8a8c98;
    state.frame_scale_95af40=1.0f;
    const auto consumed=wrap_mul(frames,state.frequency_8a8c98)/static_cast<std::int64_t>(target_rate);
    state.accumulator_8a8cd0=wrap_sub(state.accumulator_8a8cd0,consumed);
    if(state.gate_8a8cc8==0u){state.accumulator_8a8cd0=0;return 1u;}
    return static_cast<std::uint32_t>(frames);
}

PcRuntimeFrameResult417c7b runtime_frame_step_417c7b(
    PcRuntimeFrameState417c7b& state,
    const PcRuntimeFrameServices417c7b& services){
    const auto call=[&](std::uint32_t pc,std::initializer_list<std::uint32_t> args={}){
        return services.call?services.call(services.user,pc,args.begin(),args.size()):(outrun::driving::service_hole("runtime_frame_step_417c7b","services.call"),0u);
    };
    call(0x449430u);
    const auto cooperative=call(0x455c20u);
    std::uint32_t quantized=1u;
    if(services.timing_state&&services.timing_services)
        quantized=runtime_frame_ticks_417890(*services.timing_state,60,*services.timing_services);
    // Port enhancement: above 60 Hz the menu / loading modes also keep 60 Hz
    // ticks (the PC runs them once per frame, faster on a faster display).
    const bool display_only=services.ticks&&services.ticks->display_frames;
    const bool force_one=cooperative>1u||(!display_only&&(state.mode_78026c==0x20u||state.mode_78026c==0x24u||state.mode_78026c==0x18u));
    state.updates_95af48=(force_one||(quantized==0u&&!display_only))?1u:quantized;
    call(0x43fa10u,{state.updates_95af48});
    state.update_index_8a8cdc=0u;
    if(static_cast<std::int32_t>(state.updates_95af48)>0){
        while(static_cast<std::int32_t>(state.update_index_8a8cdc) <
              static_cast<std::int32_t>(state.updates_95af48)){
            if(services.ticks&&services.ticks->before_tick)services.ticks->before_tick();
            call(0x453bb0u);
            call(0x42f330u);
            call(0x455130u);
            call(0x43fa20u);
            call(0x43fab0u);
            call(0x480f80u);
            call(0x4666a0u);
            ++state.update_index_8a8cdc;
        }
    }
    state.update_index_8a8cdc=0u;
    if(services.ticks&&services.ticks->after_ticks)services.ticks->after_ticks(services.timing_state,state.updates_95af48);
    call(0x454670u);
    call(0x417d2bu,{state.primary_system_token,0xa4u});
    call(0x449050u);
    if(call(0x55a930u,{0x7f9460u})!=0u)call(0x4819c0u,{0x7f9460u});
    call(0x417d56u,{state.primary_system_token,0xa8u});
    const auto present=call(0x417d68u,{state.primary_system_token,0u,0u,0u,0u,0x44u});
    if(present==0x88760868u)call(0x4021b0u,{0x624d08u});
    ++state.frame_counter_95af0c;
    call(0x44fba0u,{0u});
    state.elapsed_8a8cb4=services.elapsed?services.elapsed(services.user,0x449df0u):(outrun::driving::service_hole("runtime_frame_step_417c7b","services.elapsed"),0.0f);
    state.slow_frame_flag_8a8cc0=0u;
    if(state.elapsed_8a8cb4>17.0f){++state.slow_frame_count_8a8cc4;state.slow_frame_flag_8a8cc0=1u;}
    const float deadline_elapsed=services.elapsed?services.elapsed(services.user,0x449df0u):(outrun::driving::service_hole("runtime_frame_step_417c7b","services.elapsed"),0.0f);
    return {state.updates_95af48,deadline_elapsed<(1.0f/60.0f)};
}

std::uint32_t runtime_loop_setup_417a20(
    PcRuntimeLoopSetupState417a20& state,
    const PcRuntimeLoopSetupServices417a20& services){
    const auto call=[&](std::uint32_t pc,std::initializer_list<std::uint32_t> args={}){
        return services.call?services.call(services.user,pc,args.begin(),args.size()):(outrun::driving::service_hole("runtime_loop_setup_417a20","services.call"),0u);
    };
    call(0x4041e0u);
    call(0x417a36u,{state.system_token,0u,0x625668u,1u});
    call(0x417a4du,{state.system_token,1u,0x625658u,1u});
    state.handle_8a89f4=call(0x417a6fu,{state.system_token,0x80u,1u,1u,0x15u,0u,0x8a89f4u,0u});
    state.handle_8a8a00=call(0x417a93u,{state.system_token,0x80u,0x80u,0x4bu,0u,0u,0u,0x8a8a00u,0u});
    if((state.feature_79fb50&3u)!=0u&&state.mode_78026c!=0x20u&&state.mode_78026c!=0x0au){
        call(0x46bb90u);
        call(0x477ed0u);
    }
    call(0x414c00u);
    call(0x4227c0u);
    call(0x417acau,{state.object_95b218,0x34u});
    if(state.optional_7f94e8!=0u)call(0x417ad9u,{state.optional_7f94e8,0x24u});
    state.handle_89f680=call(0x417aebu,{state.system_token,9u,0x89f680u});
    state.global_89f684=0u;
    state.global_89f66c=0u;
    state.flag_73e2b0=1u;
    return call(0x42fd90u);
}

void runtime_loop_cleanup_417970(
    PcRuntimeLoopCleanupState417970& state,
    const PcRuntimeLoopCleanupServices417970& services){
    const auto call=[&](std::uint32_t pc,std::uint32_t token=0u,std::uint32_t slot=0u){
        if(services.call)services.call(services.user,pc,token,slot);else outrun::driving::service_hole("runtime_loop_cleanup_417970","services.call");
    };
    if(state.handle_8a89f4!=0u){call(0x41797fu,state.handle_8a89f4,0x08u);state.handle_8a89f4=0u;}
    if(state.handle_8a8a00!=0u){call(0x417994u,state.handle_8a8a00,0x08u);state.handle_8a8a00=0u;}
    call(0x414c80u);
    if(state.handle_95afcc!=0u){call(0x4179aeu,state.handle_95afcc,0x08u);state.handle_95afcc=0u;}
    if(state.handle_95afc4!=0u){call(0x4179c3u,state.handle_95afc4,0x08u);state.handle_95afc4=0u;}
    if(state.handle_95afc8!=0u){call(0x4179d8u,state.handle_95afc8,0x08u);state.handle_95afc8=0u;}
    // object_95b218 is unconditional in the original body.
    call(0x4179e9u,state.object_95b218,0x30u);
    if(state.optional_7f94e8!=0u)call(0x4179f8u,state.optional_7f94e8,0x28u);
    if(state.handle_89f680!=0u){call(0x417a07u,state.handle_89f680,0x08u);state.handle_89f680=0u;}
}

std::uint32_t runtime_startup_owner_4176e0(
    PcEventControlState& event_state,
    const std::array<PcEventInitDescriptor,PcEventSlotCount>& descriptors,
    const std::array<PcEventFunctionDescriptor,PcEventFunctionTableCount>& functions,
    std::uint32_t& primary_system_token,
    std::uint32_t& secondary_system_token,
    const PcStartupOwnerServices4176e0& services){
    const auto call=[&](std::uint32_t pc,std::uint32_t a0=0u,std::uint32_t a1=0u,std::uint32_t a2=0u){
        return services.call?services.call(services.user,pc,a0,a1,a2):(outrun::driving::service_hole("pc_common_control.cpp:6022","services.call"),0u);
    };
    const auto gate=(services.platform_init_state&&services.platform_init_services)
        ?runtime_platform_init_417740(*services.platform_init_state,primary_system_token,*services.platform_init_services)
        :call(0x417740u);
    if(gate!=0u){
        runtime_bootstrap_417810(event_state,descriptors,functions,
            {services.user,services.call,primary_system_token});
        call(0x417b20u);
        call(0x417e30u);
    }
    call(0x49a650u);
    call(0x40e3b0u);
    call(0x403fb0u);
    if(primary_system_token!=0u){
        call(0x417713u,primary_system_token);
        primary_system_token=0u;
    }
    if(secondary_system_token!=0u){
        call(0x41772cu,secondary_system_token);
        secondary_system_token=0u;
    }
    return 0u;
}


}
