#include "driving/pc_tire_geometry.hpp"
#include "driving/pc_x87.hpp"
#include "driving/pc_d3dx.hpp"
#include "driving/pc_steering.hpp"
#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <limits>
#include <stdexcept>

namespace outrun::driving {
namespace {
struct Vec3 {float x,y,z;};
struct Mat4 {std::array<float,16> m{};};

Vec3 read_vec(Bytes b,std::size_t o){return {b.f32(o),b.f32(o+4),b.f32(o+8)};}
void write_vec(Bytes b,std::size_t o,const Vec3& v){b.putf(o,v.x);b.putf(o+4,v.y);b.putf(o+8,v.z);}
Mat4 read_mat(Bytes b,std::size_t o){Mat4 r;for(std::size_t i=0;i<16;++i)r.m[i]=b.f32(o+i*4);return r;}

// D3DXMatrixMultiply semantics: out = a*b, every element rounded once to float.
Mat4 matrix_multiply(const Mat4& a,const Mat4& b){
    Mat4 out;out.m=pc_d3dx_matrix_multiply(a.m,b.m); // generic D3DXMatrixMultiply
    return out;
}

Vec3 cross(const Vec3& a,const Vec3& b){
    return {
        static_cast<float>(X87(a.y)*b.z-X87(a.z)*b.y),
        static_cast<float>(X87(a.z)*b.x-X87(a.x)*b.z),
        static_cast<float>(X87(a.x)*b.y-X87(a.y)*b.x)
    };
}

Vec3 transform_coord(const Vec3& v,const Mat4& m){
    const X87 x=(X87(v.x)*m.m[0]+X87(v.y)*m.m[4])+
                        (X87(v.z)*m.m[8]+m.m[12]);
    const X87 y=(X87(v.x)*m.m[1]+X87(v.y)*m.m[5])+
                        (X87(v.z)*m.m[9]+m.m[13]);
    const X87 z=(X87(v.x)*m.m[2]+X87(v.y)*m.m[6])+
                        (X87(v.z)*m.m[10]+m.m[14]);
    const float w=static_cast<float>((X87(v.x)*m.m[3]+X87(v.y)*m.m[7])+
                                     (X87(v.z)*m.m[11]+m.m[15]));
    if(w==0.0f||!std::isfinite(w))throw std::domain_error("invalid tire velocity homogeneous w");
    return {static_cast<float>(x/w),static_cast<float>(y/w),static_cast<float>(z/w)};
}

Vec3 calc_pos_velocity(Bytes work,const Vec3& point){
    Mat4 body=read_mat(work,0x10);
    body.m[12]=work.f32(0x5c);body.m[13]=work.f32(0x60);body.m[14]=work.f32(0x64);
    return transform_coord(cross(read_vec(work,0x50),point),body);
}

Vec3 projective(const Vec3& v,const Vec3& normal){
    // PC mxProjectiveVector 0x40f280: y term, x term, z term, all kept in x87.
    X87 dot=X87(v.y)*normal.y;
    dot+=X87(v.x)*normal.x;
    dot+=X87(v.z)*normal.z;
    return {
        static_cast<float>(X87(v.x)-dot*normal.x),
        static_cast<float>(X87(v.y)-dot*normal.y),
        static_cast<float>(X87(v.z)-dot*normal.z)
    };
}

float unit_with_length(Vec3& v){
    X87 len2=X87(v.x)*v.x;
    len2+=X87(v.y)*v.y;
    len2+=X87(v.z)*v.z;
    const X87 len=x87_sqrt(len2);
    // PC helper compares against the double 1e-4 before normalizing.
    if(len>X87(0.0001)){
        const X87 inv=X87(1.0f)/len;
        v.x=static_cast<float>(inv*v.x);v.y=static_cast<float>(inv*v.y);v.z=static_cast<float>(inv*v.z);
    }
    return static_cast<float>(len);
}

void d3dx_normalize(Vec3& v){
    // Generic d3dx9_29 D3DXVec3Normalize selected by its dispatch table.
    X87 sq=X87(v.x)*v.x;
    sq+=X87(v.y)*v.y;
    sq+=X87(v.z)*v.z;
    const float squared=static_cast<float>(sq);
    constexpr float eps=1.1920928955078125e-7f;
    if(std::isfinite(squared)&&std::fabs(squared-1.0f)<=eps)return;
    if(!(squared>std::numeric_limits<float>::min())){v={0.0f,0.0f,0.0f};return;}
    const X87 inv=X87(1.0f)/x87_sqrt(X87(squared));
    v.x=static_cast<float>(inv*v.x);v.y=static_cast<float>(inv*v.y);v.z=static_cast<float>(inv*v.z);
}

Vec3 inverse_vector(const Mat4& m,const Vec3& v){
    // PC mxCalcInvertVector 0x40a8a0: dot each matrix row with v.
    return {
        static_cast<float>((X87(v.z)*m.m[2]+X87(v.y)*m.m[1])+X87(v.x)*m.m[0]),
        static_cast<float>((X87(v.z)*m.m[6]+X87(v.y)*m.m[5])+X87(v.x)*m.m[4]),
        static_cast<float>((X87(v.z)*m.m[10]+X87(v.y)*m.m[9])+X87(v.x)*m.m[8])
    };
}

void sincos_pc(float angle,float& s,float& c){
    // FSINCOS: not affected by the precision control; two binary32 stores.
    s=x87_float(x87_sin(X87(angle)));
    c=x87_float(x87_cos(X87(angle)));
}
Mat4 rotation_axis(Vec3 axis,float angle){
    // d3dx9_29 generic D3DXMatrixRotationAxis (0x4415d7).  The implementation
    // keeps t=(1-cos) in x87 extended precision after loading a rounded float,
    // but deliberately spills t*x*y (and later y*sin) to float.  Preserve those
    // spill points: they are visible at the bit level in CalcTireDirection.
    d3dx_normalize(axis);
    float sn=0.0f,cs=1.0f;sincos_pc(angle,sn,cs);
    const float t_float=static_cast<float>(X87(1.0f)-X87(cs));
    const X87 T=t_float,X=axis.x,Y=axis.y,Z=axis.z,S=sn,C=cs;
    const float txy_spill=static_cast<float>((Y*X)*T);
    const X87 yzT=(Y*Z)*T;
    const X87 zxT=(Z*X)*T;
    const X87 zs=Z*S;
    const X87 ys=Y*S;
    const float ys_spill=static_cast<float>(ys);
    const X87 xs=X*S;
    Mat4 m{};
    m.m[0]=static_cast<float>((X*X)*T+C);
    m.m[1]=static_cast<float>(X87(txy_spill)+zs);
    m.m[2]=static_cast<float>(zxT-ys);
    m.m[3]=0.0f;
    m.m[4]=static_cast<float>(X87(txy_spill)-zs);
    m.m[5]=static_cast<float>((Y*Y)*T+C);
    m.m[6]=static_cast<float>(yzT+xs);
    m.m[7]=0.0f;
    m.m[8]=static_cast<float>(X87(ys_spill)+zxT);
    m.m[9]=static_cast<float>(yzT-xs);
    m.m[10]=static_cast<float>((Z*Z)*T+C);
    m.m[11]=0.0f;m.m[12]=0.0f;m.m[13]=0.0f;m.m[14]=0.0f;m.m[15]=1.0f;
    return m;
}

Vec3 transform_normal(const Vec3& v,const Mat4& m){
    return {
      static_cast<float>((X87(v.z)*m.m[8]+X87(v.y)*m.m[4])+X87(v.x)*m.m[0]),
      static_cast<float>((X87(v.z)*m.m[9]+X87(v.y)*m.m[5])+X87(v.x)*m.m[1]),
      static_cast<float>((X87(v.z)*m.m[10]+X87(v.y)*m.m[6])+X87(v.x)*m.m[2])
    };
}

float atan2_scaled_pc(float y,float x,float scale){
    // The PC helper at 0x449330 is literally FLD y; FLD x; FPATAN, followed by
    // the caller's FMULS and FSTPS (CalcTireVelocity).
    return x87_float(x87_atan2(X87(y),X87(x))*scale);
}
std::int32_t trunc_i32(float x){
    if(!std::isfinite(x)||x<float(std::numeric_limits<std::int32_t>::min())||x>=2147483648.0f)
        throw std::domain_error("tire velocity angle outside validated finite domain");
    return static_cast<std::int32_t>(x);
}
}

void tire_velocity(Bytes work){
    auto wheels=embedded_wheels(work);
    // mxLoadMatrix(work+0x10); mxMultiMatrix(work+0x1e0) => pre-multiply.
    const Mat4 current=matrix_multiply(read_mat(work,0x1e0),read_mat(work,0x10));
    const Vec3 normal=read_vec(work,0x628);
    constexpr float fallback_threshold=9.9999997473787516e-5f; // PC 0x5a29e0
    constexpr float angle_units=10430.3779296875f;              // PC 0x6282c0
    for(auto q:wheels){
        write_vec(q,0x70,normal);
        const Vec3 point{q.f32(0x4),q.f32(0x2c),q.f32(0x0c)};
        Vec3 velocity=projective(calc_pos_velocity(work,point),normal);
        const float length=unit_with_length(velocity);
        if(fallback_threshold>length)velocity=read_vec(q,0x40);
        Vec3 lateral=cross(normal,velocity);
        d3dx_normalize(lateral);
        q.putf(0xd4,length);
        write_vec(q,0x58,velocity);write_vec(q,0x64,lateral);
        const Vec3 local=inverse_vector(current,velocity);
        // Original SSE sequence is +0.0f - component, not a sign-bit unary
        // negate; that distinction matters for signed zero at atan2 axes.
        volatile float positive_zero=0.0f;
        const float angle_y=positive_zero-local.x;
        const float angle_x=positive_zero-local.z;
        const float scaled=atan2_scaled_pc(angle_y,angle_x,angle_units);
        q.put16(0xec,static_cast<std::uint16_t>(trunc_i32(scaled)));
    }
}
void tire_direction(Bytes event,Bytes work){
    // Prefix 0x500700..0x500873.
    tire_direction_angles(event,work);
    auto wheels=embedded_wheels(work);
    constexpr float angle_to_radians=9.58738019107841e-05f; // PC 0x628254
    const Mat4 body=read_mat(work,0x10);
    const Vec3 base{0.0f,0.0f,-1.0f};
    for(auto q:wheels){
        const Vec3 normal=read_vec(q,0x70);
        const auto raw_angle=q.i16(0x32);
        const float angle=static_cast<float>(X87(raw_angle)*X87(angle_to_radians));
        const Mat4 current=matrix_multiply(body,rotation_axis(normal,angle));
        Vec3 forward=transform_normal(base,current);
        forward=projective(forward,normal);
        d3dx_normalize(forward);
        Vec3 side=cross(normal,forward);
        d3dx_normalize(side);
        write_vec(q,0x40,forward);
        write_vec(q,0x4c,side);
    }
}

}
