#include "driving/pc_chassis.hpp"
#include "driving/pc_x87.hpp"
#include <cmath>

namespace outrun::driving {
namespace {
constexpr float fixed_dt=0.01661129482090473175048828125f; // PC 0x7162C4
constexpr float rotation_scale_limit=1.25f;                 // PC 0x5B43EC
constexpr float angular_epsilon=9.99999993922529029078e-09f; // PC 0x5E0AB8
constexpr X87 vector_epsilon=0.0001;                // PC 0x6282A8 (binary64)
constexpr float game_resist_scale=0.06002243235707283f;     // PC 0x5C3698
constexpr float force_floor=0.1384274661540985f;            // PC 0x5A923C

float vector_length_pc(Bytes body,std::size_t off){
    body.check(off,12);
    const X87 x=body.f32(off),y=body.f32(off+4),z=body.f32(off+8);
    return static_cast<float>(x87_sqrt(((x*x)+(y*y))+(z*z)));
}
inline float mul_add_sse(float a,float b,float c){const float p=static_cast<float>(a*b);return static_cast<float>(p+c);}
inline float mul3_sse(float a,float b,float c){const float p=static_cast<float>(a*b);return static_cast<float>(p*c);}
inline float mul_sse(float a,float b){return static_cast<float>(a*b);}

X87 vector_length_ext(CourseProbe v){
    const X87 x=v.x,y=v.y,z=v.z;
    return x87_sqrt(((x*x)+(y*y))+(z*z));
}

float normalize_pc(CourseProbe& v){
    const float ox=v.x,oy=v.y,oz=v.z;
    const X87 length=vector_length_ext(v);
    if(length>vector_epsilon){
        const X87 inv=X87(1.0f)/length;
        v.x=static_cast<float>(inv*X87(ox));
        v.y=static_cast<float>(inv*X87(oy));
        v.z=static_cast<float>(inv*X87(oz));
    }
    return static_cast<float>(length); // caller's FSTP DWORD
}

CourseProbe subtract_x87(CourseProbe a,CourseProbe b){
    return {
        static_cast<float>(X87(a.x)-b.x),
        static_cast<float>(X87(a.y)-b.y),
        static_cast<float>(X87(a.z)-b.z)};
}

CourseProbe scale_x87(CourseProbe v,float scale){
    return {
        static_cast<float>(X87(scale)*v.x),
        static_cast<float>(X87(scale)*v.y),
        static_cast<float>(X87(scale)*v.z)};
}

CourseProbe partial_vector_x87(CourseProbe source,CourseProbe direction){
    // mxVectorCalcPartialVector 0x40F1F0 has one deliberate binary32 spill for
    // x*x; preserve it before the remaining x87 dot/length arithmetic.
    const float x2=static_cast<float>(X87(direction.x)*direction.x);
    const X87 yz2=X87(direction.y)*direction.y+
                          X87(direction.z)*direction.z;
    const X87 denom=yz2+X87(x2);
    // FCOMP 6282A8 / TEST AH,5 / JP: only an ordered "less" returns zero.
    if(denom<vector_epsilon)return {0.0f,0.0f,0.0f};
    const X87 dot=(X87(source.z)*direction.z+
                           X87(source.y)*direction.y)+
                           X87(source.x)*direction.x;
    const X87 ratio=dot/denom;
    return {
        static_cast<float>(ratio*direction.x),
        static_cast<float>(ratio*direction.y),
        static_cast<float>(ratio*direction.z)};
}
}

void set_force_obsolete(Bytes body,const CourseProbe& force,const CourseProbe& point,std::uint32_t flags,PcMatrixStack& matrices){
    body.check(0,0xd8);
    constexpr float gravity=9.8066501617431640625f; // PC 0x5C3690
    CourseProbe scaled{
        static_cast<float>(force.x*gravity),
        static_cast<float>(force.y*gravity),
        static_cast<float>(force.z*gravity)};
    if(flags&1u){
        // mxAddVector (0x40EF10): one x87 add and one binary32 store per component.
        body.putf(0x74,static_cast<float>(X87(body.f32(0x74))+scaled.x));
        body.putf(0x78,static_cast<float>(X87(scaled.y)+body.f32(0x78)));
        body.putf(0x7c,static_cast<float>(X87(scaled.z)+body.f32(0x7c)));
    }
    if(flags&2u){
        pc_matrix_push_load(matrices,body.sub(0x10,64));
        const auto local_point=pc_matrix_inverse_point(matrices,point);
        const auto local_force=pc_matrix_inverse_vector(matrices,scaled);
        // mxOuterProduct output order is individually spilled to binary32.
        CourseProbe torque{
            static_cast<float>(X87(local_point.y)*local_force.z-X87(local_point.z)*local_force.y),
            static_cast<float>(X87(local_point.z)*local_force.x-X87(local_point.x)*local_force.z),
            static_cast<float>(X87(local_point.x)*local_force.y-X87(local_point.y)*local_force.x)};
        body.putf(0x68,static_cast<float>(X87(body.f32(0x68))+torque.x));
        body.putf(0x6c,static_cast<float>(X87(torque.y)+body.f32(0x6c)));
        body.putf(0x70,static_cast<float>(X87(torque.z)+body.f32(0x70)));
        pc_matrix_pop(matrices);
    }
}

void action_force2(Bytes body,float step_scale,PcMatrixStack& matrices){
    body.check(0,0xd8);
    if(body.u32(0)==0u)return;

    float rotation_scale=step_scale;
    // COMISS/JBE: unordered (NaN) takes the non-clamping branch.
    if(!std::isnan(step_scale)&&step_scale>rotation_scale_limit)rotation_scale=rotation_scale_limit;

    const float ax=static_cast<float>(body.f32(0x74)*body.f32(0x9c));
    const float ay=static_cast<float>(body.f32(0x78)*body.f32(0x9c));
    const float az=static_cast<float>(body.f32(0x7c)*body.f32(0x9c));
    const float fx=static_cast<float>(ax+body.f32(0xc0));
    const float fy=static_cast<float>(ay+body.f32(0xc4));
    const float fz=static_cast<float>(az+body.f32(0xc8));
    const float vx=mul_add_sse(fixed_dt,fx,body.f32(0x5c));
    const float vy=mul_add_sse(fixed_dt,fy,body.f32(0x60));
    const float vz=mul_add_sse(fixed_dt,fz,body.f32(0x64));
    body.putf(0x5c,vx);body.putf(0x60,vy);body.putf(0x64,vz);

    body.putf(0x40,static_cast<float>(mul3_sse(vx,fixed_dt,step_scale)+body.f32(0x40)));
    body.putf(0x44,static_cast<float>(mul3_sse(vy,fixed_dt,step_scale)+body.f32(0x44)));
    body.putf(0x48,static_cast<float>(mul3_sse(vz,fixed_dt,step_scale)+body.f32(0x48)));

    body.putf(0x8c,static_cast<float>(body.f32(0x98)*vx));
    body.putf(0x90,static_cast<float>(body.f32(0x98)*vy));
    body.putf(0x94,static_cast<float>(body.f32(0x98)*vz));

    const float tx=static_cast<float>(body.f32(0xac)*body.f32(0x68));
    const float ty=static_cast<float>(body.f32(0xb0)*body.f32(0x6c));
    const float tz=static_cast<float>(body.f32(0xb4)*body.f32(0x70));
    const float wx=mul_add_sse(fixed_dt,static_cast<float>(tx+body.f32(0xcc)),body.f32(0x50));
    const float wy=mul_add_sse(fixed_dt,static_cast<float>(ty+body.f32(0xd0)),body.f32(0x54));
    const float wz=mul_add_sse(fixed_dt,static_cast<float>(tz+body.f32(0xd4)),body.f32(0x58));
    body.putf(0x50,wx);body.putf(0x54,wy);body.putf(0x58,wz);

    const float angular_length=vector_length_pc(body,0x50);
    if(angular_length>angular_epsilon){
        const float inv=static_cast<float>(1.0f/angular_length); // DIVSS
        CourseProbe axis{static_cast<float>(inv*wx),static_cast<float>(wy*inv),static_cast<float>(wz*inv)};
        pc_matrix_push_load(matrices,body.sub(0x10,64));
        const float angle=mul3_sse(fixed_dt,angular_length,rotation_scale);
        pc_matrix_rotate_axis(matrices,axis,angle);
        pc_matrix_get(matrices,body.sub(0x10,64));
        pc_matrix_pop(matrices);
    }

    body.putf(0x80,static_cast<float>(body.f32(0xa0)*wx));
    body.putf(0x84,static_cast<float>(body.f32(0xa4)*wy));
    body.putf(0x88,static_cast<float>(body.f32(0xa8)*wz));
    for(const auto off:{0x68u,0x74u,0xc0u,0xccu}){
        body.put32(off,0);body.put32(off+4,0);body.put32(off+8,0);
    }
}

void make_force_work_sus(Bytes event,Bytes work,const std::array<Bytes,4>& suspensions,PcMatrixStack& matrices){
    event.check(0x283,1);work.check(0,0x258);
    pc_matrix_load(matrices,work.sub(0x10,64));
    pc_matrix_multiply_current(matrices,work.sub(0x1e0,64));
    for(auto suspension:suspensions){
        suspension.check(0,0x28);
        CourseProbe point{suspension.f32(0x04),suspension.f32(0x08),suspension.f32(0x0c)};
        point=pc_matrix_point(matrices,point);
        float y=suspension.f32(0x24);
        // COMISS 0,y / JBE: only an ordered negative force is clamped while
        // crash/recovery suspension suppression is active. NaN follows JBE.
        if(event.u8(0x283)>0u&&!std::isnan(y)&&y<0.0f)y=0.0f;
        CourseProbe force{0.0f,y,0.0f};
        force=pc_matrix_vector(matrices,force);
        set_force_obsolete(work,force,point,3u,matrices);
    }
}

void calc_game_resist(Bytes event,Bytes work,Bytes params,PcMatrixStack& matrices){
    event.check(0,0xe70);work.check(0,0x80);params.check(0,0x19d8);
    event.putf(0x224,1.0f);
    CourseProbe velocity{work.f32(0x5c),work.f32(0x60),work.f32(0x64)};
    const float speed=normalize_pc(velocity);
    float force=mul_sse(speed,speed);
    force=mul_sse(force,params.f32(0x19d4));
    force=mul_sse(force,params.f32(0x1988));
    force=mul_sse(force,game_resist_scale);
    const float remaining=static_cast<float>(1.0f-event.f32(0xe6c));
    force=mul_sse(force,remaining);
    // COMISS force,0 / JBE skips non-positive ordered values and NaNs.
    if(std::isnan(force)||force<=0.0f)return;
    const CourseProbe applied=scale_x87(velocity,static_cast<float>(0.0f-force));
    const CourseProbe point{work.f32(0x40),work.f32(0x44),work.f32(0x48)};
    set_force_obsolete(work,applied,point,1u,matrices);
}

void force_work_49fed0(Bytes event,Bytes work,Bytes params,PcMatrixStack& matrices){
    event.check(0,0x2b8);work.check(0,0x80);params.check(0,4);
    static constexpr float table[5]={0.0f,0.25f,0.5f,0.75f,1.0f};
    float amount=0.0f;
    const std::uint8_t counter=event.u8(0x282);
    if(counter>0u){
        event.put8(0x282,static_cast<std::uint8_t>(counter-1u));
        const unsigned slot=counter>4u?4u:counter;
        amount=mul_sse(table[slot],params.f32(0));
        amount=mul_sse(amount,4.0f);
    }
    CourseProbe delta{
        mul_sse(fixed_dt,work.f32(0x5c)),
        mul_sse(fixed_dt,work.f32(0x60)),
        mul_sse(fixed_dt,work.f32(0x64))};
    const X87 length_ext=vector_length_ext(delta);
    const float length_spill=static_cast<float>(length_ext); // FST DWORD, no pop
    const float divisor=(X87(force_floor)>length_ext)?force_floor:length_spill;
    const float ratio=static_cast<float>(amount/divisor); // DIVSS
    const float neg_ratio=static_cast<float>(0.0f-ratio);
    CourseProbe applied{
        mul_sse(neg_ratio,delta.x),
        mul_sse(neg_ratio,delta.y),
        mul_sse(neg_ratio,delta.z)};
    const CourseProbe point{work.f32(0x40),work.f32(0x44),work.f32(0x48)};
    set_force_obsolete(work,applied,point,1u,matrices);
}

void ass_cancel_incline_resistance(Bytes work,PcMatrixStack& matrices){
    work.check(0,0x634);
    const CourseProbe road{work.f32(0x628),work.f32(0x62c),work.f32(0x630)};
    CourseProbe force=subtract_x87(road,{0.0f,1.0f,0.0f});
    const float neg_mass=static_cast<float>(0.0f-work.f32(0x98));
    force=scale_x87(force,neg_mass);
    const CourseProbe velocity{work.f32(0x5c),work.f32(0x60),work.f32(0x64)};
    force=partial_vector_x87(force,velocity);
    const CourseProbe point{work.f32(0x40),work.f32(0x44),work.f32(0x48)};
    set_force_obsolete(work,force,point,3u,matrices);
}

void ass_press_down_against_road(Bytes work,PcMatrixStack& matrices){
    work.check(0,0x634);
    const float factor=mul_sse(work.f32(0x98),-5.0f);
    const CourseProbe road{work.f32(0x628),work.f32(0x62c),work.f32(0x630)};
    const CourseProbe force=scale_x87(road,factor);
    const CourseProbe point{work.f32(0x40),work.f32(0x44),work.f32(0x48)};
    set_force_obsolete(work,force,point,1u,matrices);
}

void make_force_work(Bytes event,Bytes work,Bytes params,const std::array<Bytes,4>& suspensions,PcMatrixStack& matrices){
    event.check(0,0xe70);work.check(0,0x634);params.check(0,0x19d8);
    for(const auto off:{0x68u,0x6cu,0x70u,0x74u,0x78u,0x7cu})work.put32(off,0u);
    make_force_work_sus(event,work,suspensions,matrices);
    calc_game_resist(event,work,params,matrices);
    force_work_49fed0(event,work,params,matrices);
    const CourseProbe gravity_force{0.0f,static_cast<float>(0.0f-work.f32(0x98)),0.0f};
    const CourseProbe point{work.f32(0x40),work.f32(0x44),work.f32(0x48)};
    set_force_obsolete(work,gravity_force,point,3u,matrices);
    ass_cancel_incline_resistance(work,matrices);
    ass_press_down_against_road(work,matrices);
    for(unsigned k=0;k<3;++k){
        work.put32(0x22cu+k*4u,work.u32(0x74u+k*4u));
        work.put32(0x238u+k*4u,work.u32(0x68u+k*4u));
    }
}

}

namespace outrun::driving {
void make_force_work_tire_4a0000(Bytes event,Bytes work,Bytes params,const std::array<Bytes,4>& tires,PcMatrixStack& matrices){
    event.check(0,0xdc4);work.check(0,0x538);params.check(0,0x2000);
    const float gate=event.f32(0x2c8);
    bool proceed=(!std::isnan(gate)&&0.0f>=gate);
    if(!proceed){
        const auto status=(event.u32(0x2f0)>>2)&0x1fu;
        proceed=status==3u||status==6u||status==7u;
    }
    if(!proceed)return;
    pc_matrix_load(matrices,work.sub(0x10,64));
    constexpr float special_limit=0.9750000238418579f;
    for(unsigned idx=0;idx<4;++idx){
        auto tire=tires[idx];tire.check(0,0xac);
        if((tire.u8(0)&3u)!=0u)continue;
        if(idx<2u&&(((work.u8(0x440)&1u)!=0u)||((work.u8(0x534)&1u)!=0u)))continue;
        CourseProbe point{tire.f32(0x04),tire.f32(0x2c),tire.f32(0x0c)};
        point=pc_matrix_point(matrices,point);
        auto vec=[&](std::size_t o){return CourseProbe{tire.f32(o),tire.f32(o+4),tire.f32(o+8)};};
        set_force_obsolete(work,vec(0x88),point,3u,matrices);
        set_force_obsolete(work,vec(0x7c),point,3u,matrices);
        set_force_obsolete(work,vec(0xa0),point,1u,matrices);
        set_force_obsolete(work,vec(0x94),point,1u,matrices);
        CourseProbe temp=vec(0x7c);
        const CourseProbe velocity{work.f32(0x5c),work.f32(0x60),work.f32(0x64)};
        const X87 dot=(X87(temp.y)*velocity.y+X87(temp.x)*velocity.x)+X87(velocity.z)*temp.z;
        if(!(dot>X87(0.0f)))continue;
        const std::uint32_t mode=event.u32(0x208);
        if(mode>0u){
            float source{};
            const bool special=mode==params.u32(0x10a0)&&event.i8(0x11)>=15&&!std::isnan(event.f32(0x2a0))&&event.f32(0x2a0)<special_limit;
            if(special)source=event.f32(0xdc0);
            else {
                const std::size_t off=std::size_t(mode+0x7bu)*0x4cu;
                params.check(off,4);source=static_cast<float>(params.f32(off)*event.f32(0xdc0));
            }
            float factor=static_cast<float>(source-1.0f);
            if(!std::isnan(factor)&&factor<0.0f)factor=0.0f;
            temp=scale_x87(temp,factor);
        }
        temp=partial_vector_x87(temp,velocity);
        set_force_obsolete(work,temp,point,1u,matrices);
    }
}
}
