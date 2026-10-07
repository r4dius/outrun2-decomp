#include "driving/pc_wheel_dynamics.hpp"
#include "driving/pc_x87.hpp"
#include <algorithm>
#include <cmath>
#include <limits>

namespace outrun::driving {
namespace {
float literal(std::uint32_t u) { float f; std::memcpy(&f, &u, 4); return f; }
float dt() { return literal(0x3c881469); } // Original 1/60.2, not rounded to 1/60.
float inverse_dt() { return literal(0x4270cccd); }
float radians(std::int16_t a) { return x87_float(X87(std::int32_t(a)) * literal(0x38c90fdb)); } // 4493A0 fild; fmul 628254; fstp
std::int16_t signed16(std::uint16_t u) { std::int16_t s; std::memcpy(&s,&u,2); return s; }
std::int32_t signed32(std::uint32_t u) { std::int32_t s; std::memcpy(&s,&u,4); return s; }
std::int32_t integer(double x) {
    if (!std::isfinite(x) || x < -2147483648.0 || x >= 2147483648.0)
        throw std::domain_error("wheel integer conversion outside validated finite domain");
    return static_cast<std::int32_t>(x);
}
std::int32_t integer(X87 x) { // CRT _ftol2 (582194): truncation of the x87 value
    if (!(x.v >= -2147483648.0 && x.v < 2147483648.0))
        throw std::domain_error("wheel integer conversion outside validated finite domain");
    return x87_ftol32(x);
}
void nonzero(float x, const char* what) {
    if (!std::isfinite(x) || x == 0.0f) throw std::domain_error(what);
}
float gear_ratio(Bytes e, Bytes p) {
    const auto gear=e.u32(0x208);
    if (gear>16) throw std::out_of_range("gear outside validated parameter view");
    float r=p.f32((gear+0x3a)*0x4c);
    r=r*p.f32(0x134c); r=r*p.f32(0x10ec);
    nonzero(r,"zero/nonfinite selected transmission ratio"); return r;
}
float tire_mass_term(float radius, float reference_load) {
    float x=radius*radius; x=x*reference_load;
    return x*literal(0x3dd0d67f);
}
float projected_omega(Bytes wheel,float radius) {
    nonzero(radius,"zero/nonfinite wheel radius");
    // x87 holds the division and cosine product until the final f32 store.
    const X87 cosine=x87_cos(X87(radians(wheel.i16(0xee))));
    return x87_float((X87(wheel.f32(0xd4))/radius)*cosine);
}
float signed_brake(float omega, float brake) { return omega>=0.0f ? brake : 0.0f-brake; }
float apply_brake(float omega, float delta) {
    return std::abs(omega)>std::abs(delta) ? omega-delta : 0.0f;
}
void advance_angle(Bytes wheel,float omega) {
    const float a=omega*literal(0xc32d4318);
    const auto delta=integer(a);
    wheel.put16(0x30,static_cast<std::uint16_t>(std::uint32_t(std::uint16_t(wheel.i16(0x30)))+
                                                 static_cast<std::uint32_t>(delta)));
}
float softened_limit(float magnitude,float grip) {
    if(grip==0.0f) return grip;
    float ratio=magnitude/grip;
    float factor=1.0f;
    if(ratio>0.25f && 4.0f>ratio) {
        ratio=ratio-0.25f; ratio=ratio*literal(0x3e888889);
        float u=1.0f-ratio; float cube=u*u; cube=cube*u;
        factor=1.0f-cube; factor=factor*0.75f; factor=factor+0.25f;
    }
    return factor*grip;
}
} // namespace

WheelViews embedded_wheels(Bytes w) {
    return {w.sub(0x258,0xf4),w.sub(0x34c,0xf4),w.sub(0x440,0xf4),w.sub(0x534,0xf4)};
}

void advance_wheel_angle_inline(Bytes wheel,float angular_velocity) {
    advance_angle(wheel,angular_velocity);
}

// PC 0x500ea0. Low-speed special case and per-axle parameters are retained.
void cornering_power(Bytes e, Bytes p, const WheelViews& wheels) {
    for(std::size_t i=0;i<4;++i) {
        float x=p.f32(0xf70+(i/2)*0x4c);
        if(e.u32(0x1f4)<20 && e.i32(0xd90)>0) x=x*literal(0x3c23d70a);
        nonzero(x,"zero/nonfinite cornering parameter");
        x=1.0f/x; x=x+wheels[i].f32(0xd0);
        if(0.0f>x) x=0.0f;
        wheels[i].putf(0xc8,x);
    }
}

// PC 0x5019c0. The signed 16-bit folding must precede float conversion.
void side_force(const WheelViews& wheels) {
    for(auto q:wheels) {
        auto a=signed16(static_cast<std::uint16_t>(std::uint16_t(q.i16(0x32))-
                                                 std::uint16_t(q.i16(0xec))));
        if(a>0x4000 || a< -0x4000) a=signed16(static_cast<std::uint16_t>(-32768-int(a)));
        float load=q.f32(0xd4)*q.f32(0x38); load=load*literal(0x40c47029);
        float angle=float(a)*literal(0x38c90fdb);
        if(literal(0x38d1b717)>load) {q.putf(0xac,0.0f);continue;}
        float correction=q.f32(0xc8)*angle;
        float denominator=1.0f/load; denominator=denominator+q.f32(0xc8);
        nonzero(denominator,"zero/nonfinite side-force denominator");
        correction=correction/denominator;
        float force=angle-correction; force=force*load;
        // The original compares an unrounded x87 ratio, but stores it as f32.
        const X87 scale=X87(load)/x87_abs(X87(force)); // 449390 fabs; fdivr
        if(X87(1.0f)>scale) force=x87_float(scale)*force; // FST spill, MULSS
        q.putf(0xac,force);
    }
}

// PC 0x500ff0. Front axle uses a rounded reciprocal, then an x87 brake product.
void front_driving_force(Bytes p,const WheelViews& wheels) {
    const float radius=p.f32(0xb48), inertia=p.f32(0xc78);
    nonzero(radius,"zero/nonfinite front radius");
    for(unsigned i=0;i<2;++i) {
        auto q=wheels[i];
        float mass=tire_mass_term(radius,q.f32(0x38));
        float sum=mass+inertia; nonzero(sum,"zero/nonfinite front force denominator");
        float inv=1.0f/sum;
        float projected=projected_omega(q,radius);
        float omega=projected*mass;
        float previous=q.f32(0xd8)*inertia; omega=omega+previous; omega=omega*inv;
        float brake=signed_brake(omega,q.f32(0xc4)); q.putf(0xc4,brake);
        float delta=x87_float(X87(inv)*brake*dt());
        omega=apply_brake(omega,delta); q.putf(0xdc,omega);
        float force=omega-projected; force=force/radius; force=force*mass; force=force*inverse_dt();
        q.putf(0xb0,force);
    }
}

// PC 0x501240. Both rear wheels are coupled through the engine and clutch.
void rear_driving_force(Bytes e,Bytes w,Bytes p) {
    const float radius=p.f32(0xb94), inertia=p.f32(0xcc4), engine_inertia=p.f32(0x16dc);
    nonzero(radius,"zero/nonfinite rear radius"); nonzero(engine_inertia,"zero/nonfinite engine inertia");
    const auto wheels=embedded_wheels(w);auto l=wheels[2],r=wheels[3];
    // PC multiplies reference_load * (radius * radius), rather than reversing the operands.
    float radius_sq=radius*radius;
    float ml=l.f32(0x38)*radius_sq;ml=ml*literal(0x3dd0d67f);
    float mr=r.f32(0x38)*radius_sq;mr=mr*literal(0x3dd0d67f);
    const float pl=projected_omega(l,radius),pr=projected_omega(r,radius);
    float engine=e.f32(0x214)/engine_inertia;engine=engine*dt();engine=engine+e.f32(0x21c);
    float ratio=0.0f,effective=0.0f;
    if(e.u32(0x208)) {ratio=gear_ratio(e,p);effective=ratio*ratio;effective=effective*engine_inertia;engine=engine/ratio;}
    float il=ml+inertia,ir=mr+inertia;nonzero(il,"zero left effective inertia");nonzero(ir,"zero right effective inertia");
    float invl=1.0f/il,invr=1.0f/ir;
    float ol=pl*ml,orr=pr*mr;
    float prev=l.f32(0xd8)*inertia;ol=ol+prev;ol=ol*invl;
    prev=r.f32(0xd8)*inertia;orr=orr+prev;orr=orr*invr;
    float bl=signed_brake(ol,l.f32(0xc4)),br=signed_brake(orr,r.f32(0xc4));
    l.putf(0xc4,bl);r.putf(0xc4,br);
    float dl=x87_float(X87(invl)*bl*dt());
    float dr=invr*br;dr=dr*dt(); // Intentional PC left/right precision asymmetry.
    ol=apply_brake(ol,dl);orr=apply_brake(orr,dr);
    float average=orr*ir;float term=ol*il;average=average+term;
    float sum=ir+il;nonzero(sum,"zero rear force inertia sum");average=average/sum;
    float delta=engine-average;
    float limit=0.0f;
    if(effective!=0.0f){limit=e.f32(0x228)*ratio;limit=limit/effective;limit=limit*dt();}
    if(delta>limit)delta=limit;else if(0.0f-limit>delta)delta=0.0f-limit;
    float el=e.f32(0x2a4)+1.0f;el=el*effective;el=el*0.5f;
    float er=effective-el;
    float omega_l=delta+ol;omega_l=omega_l*el;
    term=il*ol;omega_l=omega_l+term;float den=el+il;nonzero(den,"zero left clutch denominator");omega_l=omega_l/den;
    float omega_r=delta+orr;omega_r=omega_r*er;
    term=ir*orr;omega_r=omega_r+term;den=er+ir;nonzero(den,"zero right clutch denominator");omega_r=omega_r/den;
    l.putf(0xdc,omega_l);r.putf(0xdc,omega_r);
    float force=omega_l-pl;force=force/radius;force=force*ml;force=force*inverse_dt();l.putf(0xb0,force);
    force=omega_r-pr;force=force/radius;force=force*mr;force=force*inverse_dt();r.putf(0xb0,force);
}

// PC 0x5026f0. Lateral and longitudinal force limits intentionally differ.
void friction_circle(Bytes e,Bytes p,const WheelViews& wheels) {
    for(std::size_t i=0;i<4;++i){
        auto q=wheels[i];float radius=p.f32(0xb48+(i/2)*0x4c);
        float rolling=x87_float(X87(radius)*q.f32(0xdc)),speed=q.f32(0xd4);
        const float angle=radians(q.i16(0xee));
        const float longitudinal=x87_float(x87_cos(X87(angle))*speed-rolling);
        const float lateral=x87_float(x87_sin(X87(angle))*speed);
        const float angle_units=x87_float(x87_atan2(X87(lateral),X87(longitudinal))*literal(0x4622f983));
        q.put16(0xf0,static_cast<std::uint16_t>(integer(angle_units)));
        float fx=q.f32(0xb0),fy=q.f32(0xac),grip=q.f32(0xc0);
        float sum=fx*fx;float side_sq=fy*fy;sum=sum+side_sq;
        const float magnitude=x87_float(x87_sqrt(X87(sum)));
        float limit=e.u32(0x208)>1?softened_limit(magnitude,grip):grip;
        float scale=magnitude>limit?limit/magnitude:1.0f;
        q.putf(0xe4,scale);
        if(fx>=0.0f)fx=scale*fx;
        else {float u=1.0f-scale;float cube=u*u;cube=cube*u;float factor=1.0f-cube;fx=factor*fx;}
        q.putf(0xb0,fx);
        const float side_magnitude=e.i32(0x38)>250?x87_float(x87_sqrt(X87(sum))):x87_float(x87_abs(X87(fy)));
        limit=softened_limit(side_magnitude,grip);
        scale=side_magnitude>limit?limit/side_magnitude:1.0f;
        q.putf(0xac,scale*fy);
    }
}

// PC 0x501ad0, semantic name chosen here; not claimed to be a recovered PC symbol.
void resolve_wheel_forces(const WheelViews& wheels) {
    for(auto q:wheels){
        const float a=radians(q.i16(0xee));const float c=x87_float(x87_cos(X87(a))),s=x87_float(x87_sin(X87(a)));
        const float side=q.f32(0xac),drive=q.f32(0xb0);
        float z=side*s;float term=drive*c;z=z+term;
        float x=side*c;term=drive*s;x=x-term;
        q.putf(0xb8,z);q.putf(0xb4,x);
    }
}

// PC 0x501190. Braking does not reverse a front wheel through zero.
void front_wheel_rotation(Bytes p,const WheelViews& wheels) {
    nonzero(p.f32(0xc78),"zero/nonfinite front inertia");
    for(unsigned i=0;i<2;++i){auto q=wheels[i];
        float inv=1.0f/p.f32(0xc78);
        float delta=p.f32(0xb48)*q.f32(0xb0);delta=delta*inv;delta=delta*dt();
        float before_brake=q.f32(0xd8)-delta;
        float brake=q.f32(0xc4)*inv;brake=brake*dt();float omega=before_brake-brake;
        float product=before_brake*omega;if(0.0f>=product)omega=0.0f;
        q.putf(0xd8,omega);advance_angle(q,omega);
    }
}

// PC 0x501680. Updates rear angular velocity AND engine state; no chassis motion.
void rear_wheel_rotation(Bytes e,Bytes w,Bytes p) {
    const float engine_inertia=p.f32(0x16dc),inertia=p.f32(0xcc4),radius=p.f32(0xb94);
    nonzero(engine_inertia,"zero/nonfinite engine inertia");nonzero(inertia,"zero/nonfinite rear inertia");
    auto wheels=embedded_wheels(w);auto l=wheels[2],r=wheels[3];
    float free_engine=e.f32(0x214)/engine_inertia;free_engine=free_engine*dt();free_engine=free_engine+e.f32(0x21c);
    float engine=free_engine,ratio=1.0f,effective=0.0f,clutch=0.0f;
    if(e.u32(0x208)){ratio=gear_ratio(e,p);effective=ratio*ratio;effective=effective*engine_inertia;
        clutch=e.f32(0x228)*ratio;engine=engine/ratio;}
    float inv=1.0f/inertia;
    auto freely=[&](Bytes q){float d=q.f32(0xb0)*inv;d=d*radius;d=d*dt();float o=q.f32(0xd8)-d;
        float b=q.f32(0xc4)*inv;b=b*dt();return o-b;};
    const float ol=freely(l),orr=freely(r);
    float pair=inertia*2.0f;nonzero(pair,"zero/nonfinite rear pair inertia");float invpair=1.0f/pair;
    float mean=orr+ol;mean=mean*inertia;mean=mean*invpair;
    float equilibrium=mean*pair;float term=engine*effective;equilibrium=equilibrium+term;
    float combined=pair+effective;nonzero(combined,"zero/nonfinite coupled inertia");equilibrium=equilibrium/combined;
    float limit=0.0f;if(clutch!=0.0f){nonzero(effective,"zero inertia with nonzero clutch");limit=clutch/effective;limit=limit*dt();}
    float change=equilibrium-engine;
    if(change>limit)change=limit;else if(0.0f-limit>change)change=0.0f-limit;
    engine=change+engine;
    float correction=equilibrium*combined;term=engine*effective;correction=correction-term;
    correction=correction*invpair;correction=correction-mean;
    float omega=correction+ol;l.putf(0xd8,omega);advance_angle(l,omega);
    omega=correction+orr;r.putf(0xd8,omega);advance_angle(r,omega);
    float candidate=e.f32(0x21c);
    if(e.u32(0x208)){float ratio_sq=ratio*ratio;effective=effective/ratio_sq;candidate=ratio*engine;}
    float engine_delta=candidate-free_engine;engine_delta=engine_delta/engine_inertia;engine_delta=engine_delta*effective;
    float result=free_engine+engine_delta;
    if(0.0f>result)result=0.0f;else if(result>p.f32(0x1690))result=p.f32(0x1690);
    e.putf(0x21c,result);
    float transfer=p.f32(0x16dc)*engine_delta;transfer=transfer*inverse_dt();e.putf(0x218,transfer);
    e.put32(0x210,e.u32(0x48));
    const std::int32_t rpm=integer(X87(result)*literal(0x4118c9eb));e.puti(0x48,rpm);
    const auto old=e.i32(0x20c);
    const auto difference=signed32(static_cast<std::uint32_t>(rpm)-static_cast<std::uint32_t>(old));
    const auto fraction=difference/(p.f32(0x1644)>result?8:4);
    const auto residual=signed32(static_cast<std::uint32_t>(difference)-static_cast<std::uint32_t>(fraction));
    const auto clamped=std::clamp(residual,-1500,1500);
    e.put32(0x20c,static_cast<std::uint32_t>(old)-static_cast<std::uint32_t>(clamped)+static_cast<std::uint32_t>(difference));
}

// PC 0x501c90. The original includes a first-gear correction, not generic drag.
void rolling_resistance(Bytes e,Bytes p,const WheelViews& wheels) {
    for(std::size_t i=0;i<4;++i){auto q=wheels[i];const auto axle=(i/2)*0x4c;
        nonzero(p.f32(0xda8+axle),"zero/nonfinite rolling parameter");
        float inv=1.0f/p.f32(0xda8+axle);float fixed=p.f32(0xd10+axle)*q.f32(0x34);
        const float drive=q.f32(0xb0);float loss=drive*drive;loss=loss*inv;loss=loss+fixed;
        if(e.u32(0x208)==1){nonzero(p.f32(0x1184),"zero first gear ratio");
            float candidate=p.f32(0x11d0)/p.f32(0x1184);candidate=candidate*drive;
            float candidate_loss=candidate*candidate;candidate_loss=candidate_loss*inv;candidate_loss=candidate_loss+fixed;
            candidate=candidate-candidate_loss;float unadjusted=drive-loss;
            if(candidate>unadjusted)loss=drive-candidate;
        }
        float resistance=0.0f-loss;
        if(e.f32(0xdc0)>1.0f){float divisor=e.f32(0xdc0)*e.f32(0xdc0);resistance=resistance/divisor;}
        q.putf(0xbc,resistance);
    }
}

// PC 0x501ba0. Division is rounded before cosine multiplication here (unlike force prediction).
void slip_ratio(Bytes p,const WheelViews& wheels) {
    for(std::size_t i=0;i<4;++i){auto q=wheels[i];float radius=p.f32(0xb48+(i/2)*0x4c);
        nonzero(radius,"zero/nonfinite slip radius");
        float projected=x87_float(X87(q.f32(0xd4))/radius);
        projected=x87_float(x87_cos(X87(radians(q.i16(0xee))))*projected);
        projected=x87_float(x87_abs(X87(projected)));const float rotation=x87_float(x87_abs(X87(q.f32(0xd8))));
        float ratio=0.0f;
        if(projected>rotation){ratio=projected-rotation;ratio=ratio/projected;}
        else if(rotation>projected){ratio=projected-rotation;ratio=ratio/rotation;}
        q.putf(0xe0,ratio);
    }
}

// Original inline block 0x502ce2..0x502d9d, isolated without running DrivingControl.
void distribute_brake_torque(Bytes e,Bytes w,Bytes p,const Tables& tables) {
    const float pressure=brake_pressure(e.i32(0x38),tables);
    float first=p.f32(0x180c)+1.0f,second=p.f32(0x1858)+1.0f;
    nonzero(first,"zero first brake-distribution denominator");nonzero(second,"zero second brake-distribution denominator");
    first=1.0f/first;second=1.0f/second;
    float bias=second-first;bias=bias*pressure;bias=bias+first;
    const float total=p.f32(0x17c0)*p.f32(0);
    float rear=p.f32(0xb94)*total;rear=rear*bias;rear=rear*pressure;rear=rear*0.5f;
    float front=1.0f-bias;front=front*p.f32(0xb48);front=front*total;front=front*pressure;front=front*0.5f;
    auto wheels=embedded_wheels(w);wheels[0].putf(0xc4,front);wheels[1].putf(0xc4,front);
    wheels[2].putf(0xc4,rear);wheels[3].putf(0xc4,rear);
}

void drivetrain_step_from_contacts(Bytes e,Bytes w,Bytes p,const Tables& tables) {
    auto wheels=embedded_wheels(w);
    accel_operation(e,w,p);
    distribute_brake_torque(e,w,p,tables);
    auto_clutch_control(e,w,p);
    engine_torque(e,p,tables);
    // GetRoadMu and all upstream contact/direction work remain external.
    tire_grip(e,w,p,wheels);
    cornering_power(e,p,wheels);side_force(wheels);
    front_driving_force(p,wheels);rear_driving_force(e,w,p);
    friction_circle(e,p,wheels);resolve_wheel_forces(wheels);
    front_wheel_rotation(p,wheels);rear_wheel_rotation(e,w,p);
    rolling_resistance(e,p,wheels);slip_ratio(p,wheels);
}

// PC 0x502120. Guest wheel pointers and mutable PC globals are explicit.
void running_resistance(Bytes e,Bytes w,const RunningResistanceTuning& t) {
    float base=0.0f;
    if(e.u8(0x283)==0){
        // FILD (signed) + FADD 2^32 for the unsigned high half, then x87 square.
        X87 speed=X87(static_cast<std::int32_t>(e.u32(0x1f4)));
        if(static_cast<std::int32_t>(e.u32(0x1f4))<0) speed=speed+literal(0x4f800000);
        base=x87_float(-((speed*speed)*t.speed_square_scale));
    }
    float factor;
    if(e.u32(0xdf8)!=0) factor=e.u8(0x282)!=0?t.state_alt_factor:t.state_factor;
    else factor=t.no_state_factor;
    float reduction=factor*base;
    reduction=reduction*e.f32(0xdb4);
    base=base-reduction;

    const auto wheels=embedded_wheels(w);
    const bool event_surface=(e.u32(0x248)&0x801cu)!=0;
    for(auto q:wheels){
        float scalar=0.0f;
        float angle_scale=0.0f;
        if(event_surface) angle_scale=t.event_surface_angle_scale;
        else if((q.u32(0x14)&0x801cu)!=0) angle_scale=t.wheel_surface_angle_scale;
        if(angle_scale!=0.0f){
            const float angle=radians(q.i16(0xee));
            const float cosine=x87_float(x87_cos(X87(angle))); // PC stores fcos to f32 before fabs.
            scalar=x87_float((x87_abs(X87(cosine))*angle_scale)*base);
        }
        for(std::size_t k=0;k<3;++k) q.putf(0x94+k*4,q.f32(0x58+k*4)*scalar);
    }
}


namespace {
struct V3 { float x,y,z; };
V3 getv(Bytes q,std::size_t o){return {q.f32(o),q.f32(o+4),q.f32(o+8)};}
void putv(Bytes q,std::size_t o,V3 v){q.putf(o,v.x);q.putf(o+4,v.y);q.putf(o+8,v.z);}
V3 scalev(V3 v,float s){return {v.x*s,v.y*s,v.z*s};}
V3 addv(V3 a,V3 b){return {a.x+b.x,a.y+b.y,a.z+b.z};}
V3 subv(V3 a,V3 b){return {a.x-b.x,a.y-b.y,a.z-b.z};}
V3 partial_vector(V3 a,V3 axis){
    // mxVectorCalcPartialVector: x^2 is explicitly rounded to f32 before
    // the y/z terms are accumulated in the original x87 implementation.
    const float x2=x87_float(X87(axis.x)*axis.x);
    const X87 den=(X87(axis.y)*axis.y + X87(axis.z)*axis.z) + x2;
    // FCOMP 6282A8 / TEST AH,5 / JP: only an ordered "less" returns zero.
    if(den<X87(0.0001))return {0,0,0};
    const X87 num=(X87(a.z)*axis.z + X87(a.y)*axis.y) + X87(a.x)*axis.x;
    const X87 k=num/den;
    return {x87_float(k*axis.x),x87_float(k*axis.y),x87_float(k*axis.z)};
}
V3 scale_unit(V3 v,float target){
    // PC 0x40F080: sqrt((x*x+y*y)+z*z); FCOM 1e-4 / TEST AH,41h skips <= and NaN.
    const X87 len=x87_sqrt((X87(v.x)*v.x+X87(v.y)*v.y)+X87(v.z)*v.z);
    if(!(len>X87(0.0001)))return v;
    const X87 k=X87(target)/len;
    return {x87_float(k*v.x),x87_float(k*v.y),x87_float(k*v.z)};
}
}

// PC 0x502270. Symbol identity comes from the same post-SetRunningResistance
// position in the symbolized Lindbergh DrivingControl (CopyPhysicalWork).
void copy_physical_work(Bytes e,Bytes w,float reaction_blend_parameter){
    float blend=1.0f-static_cast<float>(e.i8(0xd36))*literal(0x3daaaaab);
    if(0.0f>blend)blend=0.0f;else if(blend>1.0f)blend=1.0f;
    blend=reaction_blend_parameter*blend;
    for(auto q:embedded_wheels(w)){
        float scalar=q.f32(0xbc)+q.f32(0xb8);
        putv(q,0x7c,scalev(getv(q,0x58),scalar));
        putv(q,0x88,scalev(getv(q,0x64),q.f32(0xb4)));
        const V3 applied=scalev(getv(q,0x4c),q.f32(0xac));
        const V3 projected=partial_vector(applied,getv(q,0x58));
        const V3 perpendicular=subv(applied,projected);
        const V3 blended=scalev(projected,blend);
        V3 adjusted=addv(perpendicular,blended);
        adjusted=scale_unit(adjusted,std::abs(q.f32(0xac)));
        putv(q,0xa0,subv(adjusted,applied));
    }
    float front=w.f32(0x42c)+w.f32(0x338);front=front*0.5f;e.putf(0x50,front);
    float rear=w.f32(0x614)+w.f32(0x520);rear=rear*0.5f;e.putf(0x54,rear);
}

void known_driving_tail_from_contacts(Bytes e,Bytes w,Bytes p,const Tables& tables,
                                      const RunningResistanceTuning& tuning,
                                      float reaction_blend_parameter){
    // This starts at the first currently closed step after transmission/contact
    // setup and preserves the observed PC order through CopyPhysicalWork.
    drivetrain_step_from_contacts(e,w,p,tables);
    running_resistance(e,w,tuning);
    copy_physical_work(e,w,reaction_blend_parameter);
}

} // namespace outrun::driving
