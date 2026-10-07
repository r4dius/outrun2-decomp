#include "driving/pc_contact.hpp"
#include "driving/pc_x87.hpp"
#include "driving/pc_d3dx.hpp"
#include <array>
#include <cmath>
#include <cstdint>
#include <limits>

namespace outrun::driving {
namespace {
struct Vec3 { float x,y,z; };
struct Mat4 { std::array<float,16> m{}; };
Vec3 read_vec(Bytes b,std::size_t o){ return {b.f32(o),b.f32(o+4),b.f32(o+8)}; }
void write_vec(Bytes b,std::size_t o,const Vec3& v){ b.putf(o,v.x);b.putf(o+4,v.y);b.putf(o+8,v.z); }
Mat4 read_mat(Bytes b,std::size_t o){ Mat4 r;for(std::size_t i=0;i<16;++i)r.m[i]=b.f32(o+i*4);return r; }
void write_mat(Bytes b,std::size_t o,const Mat4& m){ for(std::size_t i=0;i<16;++i)b.putf(o+i*4,m.m[i]); }
Mat4 identity(){ Mat4 r{};r.m[0]=r.m[5]=r.m[10]=r.m[15]=1.0f;return r; }

float dot_pc(const Vec3& a,const Vec3& b){
    // PC mxInnerProduct: z product, y product/add, x product/add, one f32 spill.
    const X87 z=X87(a.z)*b.z;
    const X87 y=X87(a.y)*b.y;
    const X87 x=X87(a.x)*b.x;
    return static_cast<float>((z+y)+x);
}
Vec3 cross_pc(const Vec3& a,const Vec3& b){
    // PC mxOuterProduct spills each component to f32.
    return {
      static_cast<float>(X87(a.y)*b.z-X87(a.z)*b.y),
      static_cast<float>(X87(a.z)*b.x-X87(a.x)*b.z),
      static_cast<float>(X87(a.x)*b.y-X87(a.y)*b.x)
    };
}
X87 scalar2_pc(const Vec3& v){
    const X87 z=X87(v.z)*v.z;
    const X87 y=X87(v.y)*v.y;
    const X87 x=X87(v.x)*v.x;
    return (z+y)+x;
}
float unit_vector_pc(Vec3& v){
    const X87 len=x87_sqrt(scalar2_pc(v));
    // mxUnitVector compares the x87 length against the double 1e-4.
    if(len>X87(0.0001)){
        const X87 inv=X87(1.0f)/len;
        v.x=static_cast<float>(inv*v.x);v.y=static_cast<float>(inv*v.y);v.z=static_cast<float>(inv*v.z);
    }
    return static_cast<float>(len);
}
void d3dx_normalize(Vec3& v){
    // d3dx9_29 generic D3DXVec3Normalize selected by the original dispatch slot.
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
    // PC mxCalcInvertVector: dot each matrix row with the vector.
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
    d3dx_normalize(axis);
    float sn=0.0f,cs=1.0f;sincos_pc(angle,sn,cs);
    const float t_float=static_cast<float>(X87(1.0f)-X87(cs));
    const X87 T=t_float,X=axis.x,Y=axis.y,Z=axis.z,S=sn,C=cs;
    const float txy_spill=static_cast<float>((Y*X)*T);
    const X87 yzT=(Y*Z)*T,zxT=(Z*X)*T,zs=Z*S,ys=Y*S;
    const float ys_spill=static_cast<float>(ys);const X87 xs=X*S;
    Mat4 m{};
    m.m[0]=static_cast<float>((X*X)*T+C);m.m[1]=static_cast<float>(X87(txy_spill)+zs);m.m[2]=static_cast<float>(zxT-ys);
    m.m[4]=static_cast<float>(X87(txy_spill)-zs);m.m[5]=static_cast<float>((Y*Y)*T+C);m.m[6]=static_cast<float>(yzT+xs);
    m.m[8]=static_cast<float>(X87(ys_spill)+zxT);m.m[9]=static_cast<float>(yzT-xs);m.m[10]=static_cast<float>((Z*Z)*T+C);
    m.m[15]=1.0f;return m;
}
Mat4 matrix_multiply(const Mat4& a,const Mat4& b){
    Mat4 out;out.m=pc_d3dx_matrix_multiply(a.m,b.m); // generic D3DXMatrixMultiply
    return out;
}
float atan2_pc(float y,float x){
    return x87_float(x87_atan2(X87(y),X87(x)));
}
float length_pc(const Vec3& v){
    const X87 x=v.x,y=v.y,z=v.z;
    return x87_float(x87_sqrt(((x*x)+(y*y))+(z*z)));
}
float atan2_scaled_pc(float y,float x,float scale){
    return x87_float(x87_atan2(X87(y),X87(x))*scale);
}
float integer_angle_to_radians_pc(std::int32_t value,float scale){
    return x87_float(X87(value)*scale); // FILD; FMUL m32
}
void rotate_axis(Mat4& current,const Vec3& axis,float angle){ current=matrix_multiply(rotation_axis(axis,angle),current); }

Mat4 cop_lookat(const Vec3& tangent,const Vec3& normal){
    Mat4 current=identity();
    // CopLookatv first checks that the requested tangent/up pair is non-collinear.
    if(scalar2_pc(cross_pc(tangent,normal))==X87(0.0f))return current;

    Vec3 axis=cross_pc({0.0f,0.0f,-1.0f},tangent);
    float length=unit_vector_pc(axis);
    if(length==0.0f)axis={0.0f,1.0f,0.0f};
    // PC caller uses FCHS on tangent.z, then ATan2f(length,-z).
    const float minus_z=static_cast<float>(-X87(tangent.z));
    const float angle1=atan2_pc(length,minus_z);
    rotate_axis(current,axis,angle1);

    Vec3 local_normal=inverse_vector(current,normal);
    axis=cross_pc({0.0f,1.0f,0.0f},local_normal);
    length=unit_vector_pc(axis);
    if(length==0.0f)axis={0.0f,1.0f,0.0f};
    const float angle2=atan2_pc(length,local_normal.y);
    rotate_axis(current,axis,angle2);
    return current;
}
}

void contact_matrix(Bytes work){
    const Mat4 body=read_mat(work,0x10);
    const Vec3 normal=read_vec(work,0x628);
    const Vec3 point=read_vec(work,0x640);

    // Exact PC SSE subtraction order/spill: point minus body translation.
    const Vec3 delta{
      static_cast<float>(point.x-body.m[12]),
      static_cast<float>(point.y-body.m[13]),
      static_cast<float>(point.z-body.m[14])};
    const float distance=dot_pc(delta,normal);
    Vec3 projected{
      static_cast<float>(normal.x*distance),
      static_cast<float>(normal.y*distance),
      static_cast<float>(normal.z*distance)};
    projected=inverse_vector(body,projected);

    volatile float positive_zero=0.0f;
    Vec3 tangent{
      positive_zero-body.m[8],
      positive_zero-body.m[9],
      positive_zero-body.m[10]};
    // The original reuses the *point/plane distance* here.  It does not
    // recompute dot(tangent,normal); the Lindbergh symbolized build shows the
    // same stack argument reuse.  This quirk is externally observable.
    tangent.x=static_cast<float>(tangent.x-normal.x*distance);
    tangent.y=static_cast<float>(tangent.y-normal.y*distance);
    tangent.z=static_cast<float>(tangent.z-normal.z*distance);
    d3dx_normalize(tangent);
    tangent=inverse_vector(body,tangent);
    const Vec3 local_normal=inverse_vector(body,normal);

    write_mat(work,0x1e0,cop_lookat(tangent,local_normal));
    write_vec(work,0x210,projected);
}

bool maximum_velocity_check(Bytes work){
    constexpr float max_linear=166.6666717529296875f; // PC 0x5C3714
    constexpr float max_angular=60.200000762939453125f; // PC 0x628110
    constexpr float one=1.0f;
    constexpr float angle_units=10430.3779296875f; // PC 0x6282C0
    constexpr float angle_to_radians=9.5873801910784096e-05f; // PC 0x628254
    constexpr float ceiling=10.0f; // PC 0x628244
    bool changed=false;

    Mat4 current=read_mat(work,0x10);
    auto clamp_vector=[&](std::size_t off,float limit){
        Vec3 v=read_vec(work,off);const float len=length_pc(v);
        if(len>limit){
            const float scale=limit/len;
            v.x=static_cast<float>(v.x*scale);v.y=static_cast<float>(v.y*scale);v.z=static_cast<float>(v.z*scale);
            write_vec(work,off,v);changed=true;
        }
    };
    clamp_vector(0x5c,max_linear);
    clamp_vector(0x50,max_angular);

    const Vec3 normal=read_vec(work,0x628);
    const Vec3 local_normal=inverse_vector(current,normal);
    float y=local_normal.y;if(y>one)y=one;
    const float root=static_cast<float>(x87_sqrt(X87(one-y*y)));
    const float scaled_angle=atan2_scaled_pc(root,y,angle_units);
    if(!std::isfinite(scaled_angle)||scaled_angle<float(std::numeric_limits<std::int32_t>::min())||scaled_angle>=2147483648.0f)
        throw std::domain_error("MaximumVelocityCheck angle outside validated finite domain");
    const std::int32_t angle_i=static_cast<std::int32_t>(scaled_angle);
    const std::int16_t excess16=static_cast<std::int16_t>(static_cast<std::uint16_t>(angle_i-0x900));
    if(excess16>0){
        Vec3 axis=cross_pc({0.0f,1.0f,0.0f},local_normal);
        d3dx_normalize(axis);
        const float radians=integer_angle_to_radians_pc(static_cast<std::int32_t>(excess16),angle_to_radians);
        rotate_axis(current,axis,radians);
        write_mat(work,0x10,current);
        // Original pushes the current matrix, recalculates contact orientation,
        // then pops.  The native port has no global matrix stack, so only the
        // observable work writes from CalcContactMatrix remain.
        contact_matrix(work);
        changed=true;
    }

    const float top=static_cast<float>(work.f32(0x644)+ceiling);
    if(work.f32(0x44)>top){
        work.putf(0x44,top);
        const float vy=work.f32(0x60);if(vy>0.0f)work.putf(0x60,0.0f);
        changed=true;
    }
    return changed;
}
}
