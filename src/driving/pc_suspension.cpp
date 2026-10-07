#include "driving/pc_suspension.hpp"
#include "driving/pc_x87.hpp"
#include "driving/pc_matrix_stack.hpp"
#include "driving/pc_wheel_dynamics.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <limits>
#include <stdexcept>

namespace outrun::driving {
namespace {
float sqrt_blend_pc(float x) {
    // Caller sequence after Sqrtf: FSQRT result remains in x87, then FADD x
    // and FMUL 0.5 occur before the single final f32 spill.
    return x87_float((x87_sqrt(X87(x))+x)*0.5f);
}
float abs_pc(float x) {
    return x87_float(x87_abs(X87(x))); // flds; fabs; fstps
}
struct Vec3 { float x,y,z; };
struct Mat4 { std::array<float,16> m{}; };
Mat4 read_mat(Bytes b,std::size_t o){Mat4 r;for(std::size_t i=0;i<16;++i)r.m[i]=b.f32(o+i*4);return r;}
Vec3 transform_normal(const Vec3& v,const Mat4& m){
    // d3dx9_29 D3DXVec3TransformNormal uses row-vector D3DX semantics.
    return {
      static_cast<float>((X87(v.z)*m.m[8]+X87(v.y)*m.m[4])+X87(v.x)*m.m[0]),
      static_cast<float>((X87(v.z)*m.m[9]+X87(v.y)*m.m[5])+X87(v.x)*m.m[1]),
      static_cast<float>((X87(v.z)*m.m[10]+X87(v.y)*m.m[6])+X87(v.x)*m.m[2])
    };
}
float dot_pc(const Vec3& a,const Vec3& b){
    // PC mxInnerProduct 0x40EFD0: z*z, y*y, add, x*x, add, all in x87.
    const X87 z=X87(a.z)*b.z;
    const X87 y=X87(a.y)*b.y;
    const X87 x=X87(a.x)*b.x;
    return static_cast<float>((z+y)+x);
}
void require_finite_nonzero(float x,const char* what){if(!std::isfinite(x)||x==0.0f)throw std::domain_error(what);}
}

float ass_get_press_down(const SuspensionTuning& t){ return t.press_down; }

void ass_specific_amount_of_tire_load(Bytes work){
    auto q=embedded_wheels(work);
    float actual=q[1].f32(0x34)+q[0].f32(0x34);
    float reference=q[1].f32(0x38)+q[0].f32(0x38);
    reference=q[2].f32(0x38)+reference;
    actual=q[2].f32(0x34)+actual;
    reference=q[3].f32(0x38)+reference;
    actual=q[3].f32(0x34)+actual;
    require_finite_nonzero(reference,"zero/nonfinite total reference tire load");
    float inv=1.0f/reference;
    float delta=actual-reference;
    for(auto wheel:q){
        float correction=wheel.f32(0x38)*inv;
        correction=correction*delta;
        float load=wheel.f32(0x34)-correction;
        wheel.putf(0x34,load);
        if(0.0f>load)wheel.putf(0x34,0.0f);
    }
}

void ass_diagonal_tire_load(Bytes work,Bytes parameters){
    auto q=embedded_wheels(work);
    const float denominator=parameters.f32(0);
    require_finite_nonzero(denominator,"zero/nonfinite diagonal-load parameter");
    float left=q[0].f32(0x34)+q[3].f32(0x34);
    float right=q[1].f32(0x34)+q[2].f32(0x34);
    float delta=left-right;
    float c=q[0].f32(0x38)/denominator;c=c*delta;q[0].putf(0x34,q[0].f32(0x34)-c);
    c=q[1].f32(0x38)/denominator;c=c*delta;q[1].putf(0x34,q[1].f32(0x34)+c);
    c=q[2].f32(0x38)/denominator;c=c*delta;q[2].putf(0x34,q[2].f32(0x34)+c);
    c=q[3].f32(0x38)/denominator;c=c*delta;q[3].putf(0x34,q[3].f32(0x34)-c);
    for(auto wheel:q)if(0.0f>wheel.f32(0x34))wheel.putf(0x34,0.0f);
}

void suspension_force(Bytes work,Bytes parameters,const SuspensionTuning& tuning){
    auto q=embedded_wheels(work);
    const float press=tuning.press_down+1.0f;
    for(auto wheel:q){
        float compression=wheel.f32(0x08)-wheel.f32(0x28);
        if(0.0f>compression)compression=0.0f;
        const float old=wheel.f32(0x18);
        wheel.putf(0x18,compression);
        float velocity=compression-old;velocity=velocity*tuning.velocity_scale;
        wheel.putf(0x20,velocity);wheel.putf(0x1c,old);
    }
    for(unsigned i=0;i<4;++i){
        auto wheel=q[i];const unsigned axle=i>>1;auto other=q[i^1u];
        const std::size_t a=(axle+0x0e)*0x4c;
        const std::size_t spring=(axle+0x16)*0x4c;
        const std::size_t antiroll=(axle+0x18)*0x4c;
        const std::size_t lower=(axle+0x10)*0x4c;
        const std::size_t upper=(axle+0x12)*0x4c;
        const float comp=wheel.f32(0x18);
        float x=comp-parameters.f32(a);
        x=x*parameters.f32(spring);x=x*press;
        float force=0.0f-x;
        x=comp-other.f32(0x18);x=x*parameters.f32(antiroll);x=x*press;force=force-x;
        x=comp-parameters.f32(lower);
        if(0.0f>x){float t=parameters.f32(spring)*x;t=t*press;force=force-t;}
        x=comp-parameters.f32(upper);
        if(x>0.0f){float t=parameters.f32(spring)*x;t=t*press;force=force-t;}

        const float absvel=abs_pc(wheel.f32(0x20));
        // Exact PC SSE spill order: (v^3 + 1) * v * 0.5.
        float d=absvel*absvel;d=d*absvel;d=d+1.0f;d=d*absvel;d=d*0.5f;
        // If the quartic shaping exceeds 1 the original replaces it with
        // 0.5*(sqrt(v)+v), not 0.5*(sqrt(v)+quartic).
        if(d>1.0f)d=sqrt_blend_pc(absvel);
        float ratio=comp-parameters.f32(a);ratio=ratio*tuning.travel_inv;ratio=abs_pc(ratio);
        if(0.0f>ratio)ratio=0.0f;else if(ratio>1.0f)ratio=1.0f;
        ratio=ratio*tuning.damping_shape;
        float damping=1.0f-ratio;damping=damping*tuning.damping_gain;damping=damping*d;
        float signed_damping=0.0f;
        if(wheel.f32(0x20)>=0.0f)signed_damping=0.0f-damping;
        else{damping=damping*tuning.rebound_scale;signed_damping=damping;}
        signed_damping=signed_damping+force;signed_damping=signed_damping*tuning.force_scale;
        wheel.putf(0x24,signed_damping);
    }
}

void tire_load(Bytes event,Bytes work,Bytes parameters,const SuspensionTuning& tuning){
    auto q=embedded_wheels(work);const Mat4 body=read_mat(work,0x10);
    for(auto wheel:q){
        float projected=0.0f;
        if((wheel.u8(0)&1u)==0 && event.u8(0x283)==0){
            float y=wheel.f32(0x24);if(0.0f>y)y=0.0f;
            const Vec3 v=transform_normal({0.0f,y,0.0f},body);
            const Vec3 normal{wheel.f32(0x70),wheel.f32(0x74),wheel.f32(0x78)};
            projected=dot_pc(v,normal);
            if(0.0f>projected)projected=0.0f;
        }
        // FLT_MAX is the original sentinel. Finite validated inputs always take this path.
        if(std::isfinite(projected) && projected<std::numeric_limits<float>::max()){
            float denom=tuning.press_down+1.0f;
            wheel.putf(0x34,static_cast<float>(X87(projected)/denom));
        }
    }
    ass_specific_amount_of_tire_load(work);
    ass_diagonal_tire_load(work,parameters);
}
void tire_load(Bytes event,Bytes work,Bytes parameters,const SuspensionTuning& tuning,PcMatrixStack& matrices){
    pc_matrix_load(matrices,work.sub(0x10,64));
    tire_load(event,work,parameters,tuning);
}
}
