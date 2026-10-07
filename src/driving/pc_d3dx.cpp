#include "driving/pc_d3dx.hpp"
#include "driving/pc_x87.hpp"
#include "driving/pc_x87_f24.hpp"
#include <array>
#include <cmath>
#include <cstring>
#include <limits>
namespace outrun::driving {
namespace {
using X=X87; // x87 register value (precision-controlled, pc_x87.hpp)
float st(X v){return x87_float(v);} // fstp DWORD
float st(F24 v){return v.v;}
// Runs body<F24> (binary32 arithmetic, pc_x87_f24.hpp) and, when an operation
// left the binary32 normal range, body<X87> again; the exact model only.
#if OR2_X87_FAST && !defined(OR2_F24_OFF)
template<class Body> auto run24(Body&& body){f24_bad=0;auto r=body(F24{});if(f24_bad)r=body(X{});return r;}
#else
template<class Body> auto run24(Body&& body){return body(X{});}
#endif
}
namespace {
template<class T> bool inverse_impl(const std::array<float,16>& m,std::array<float,16>& out,float& det_f){
    // 2x2 minors of the first two rows (rounded to float by the DLL).
    const float t_c =st(T(m[5])*m[0]-T(m[4])*m[1]);
    const float t_8 =st(T(m[9])*m[0]-T(m[8])*m[1]);
    const float t_2c=st(T(m[13])*m[0]-T(m[12])*m[1]);
    const float t_38=st(T(m[9])*m[4]-T(m[8])*m[5]);
    const float t_34=st(T(m[13])*m[4]-T(m[12])*m[5]);
    const float t_30=st(T(m[13])*m[8]-T(m[12])*m[9]);
    // Cofactors of rows 2/3 (last column block of the result).
    const float r54=st((T(m[2])*t_38-T(m[6])*t_8)+T(m[10])*t_c);
    const float r50=st((T(m[6])*t_2c-T(m[14])*t_c)-T(m[2])*t_34);
    const float r4c=st((T(m[2])*t_30-T(m[10])*t_2c)+T(m[14])*t_8);
    const float r48=st((T(m[10])*t_34-T(m[14])*t_38)-T(m[6])*t_30);
    const float r44=st((T(t_8)*m[7]-T(t_c)*m[11])-T(t_38)*m[3]);
    const float r40=st((T(t_34)*m[3]-T(t_2c)*m[7])+T(t_c)*m[15]);
    const float r3c=st((T(t_2c)*m[11]-T(t_8)*m[15])-T(t_30)*m[3]);
    const float r2c=st((T(t_30)*m[7]-T(t_34)*m[11])+T(t_38)*m[15]);
    // Minors of the last two rows; four of them stay in x87 registers.
    const float u_c=st(T(m[2])*m[7]-T(m[6])*m[3]);
    const float u_8=st(T(m[2])*m[11]-T(m[10])*m[3]);
    const T x=T(m[2])*m[15]-T(m[14])*m[3];
    const T w=T(m[6])*m[11]-T(m[10])*m[7];
    const T y=T(m[6])*m[15]-T(m[14])*m[7];
    const T z=T(m[10])*m[15]-T(m[14])*m[11];
    const float r10=st((T(u_8)*m[5]-T(u_c)*m[9])-w*m[1]);
    const float r24=st((y*m[1]-x*m[5])+T(u_c)*m[13]);
    const float r28=st((x*m[9]-T(u_8)*m[13])-z*m[1]);
    const float r34=st((z*m[5]-y*m[9])+w*m[13]);
    const float p10=st((w*m[0]-T(u_8)*m[4])+T(u_c)*m[8]);
    const float p4 =st((x*m[4]-T(u_c)*m[12])-y*m[0]);
    const float p20=st((T(m[0])*z-x*m[8])+T(u_8)*m[12]);
    const float p30=st((y*m[8]-w*m[12])-z*m[4]);
    const T det=((T(r10)*m[12]+T(r24)*m[8])+T(r28)*m[4])+T(r34)*m[0];
    det_f=st(det);
    if(det==T(0))return false; // FUCOMPP equal: NULL result
    const float s=st(T(1)/det);
    if(!std::isfinite(s))return false; // _finite((double)s)
    const std::array<float,16> r{r34,r28,r24,r10,p30,p20,p4,p10,r2c,r3c,r40,r44,r48,r4c,r50,r54};
    for(unsigned k=0;k<16;++k)out[k]=st(T(r[k])*s);
    return true;
}
}
bool pc_d3dx_matrix_inverse(Bytes out,float* det_out,Bytes in){
    in.check(0,64);out.check(0,64);
    std::array<float,16> m{};
    for(unsigned k=0;k<16;++k)m[k]=in.f32(k*4);
    std::array<float,16> r{};float det=0.f;
    const bool ok=run24([&](auto t){return inverse_impl<decltype(t)>(m,r,det);});
    if(det_out)*det_out=det;
    if(!ok)return false;
    for(unsigned k=0;k<16;++k)out.putf(k*4,r[k]);
    return true;
}
void pc_d3dx_perspective_off_center_rh(Bytes out,float l,float r,float b,float t,float zn,float zf){
    out.check(0,64);
    const X a=X(1)/(X(r)-l);   // x87 fsub then fdivr 1.0, kept in a register
    const X c=X(1)/(X(t)-b);
    out.putf(0x2c,-1.f);
    out.putf(0x00,st(a*zn*2));
    for(unsigned o:{0x04u,0x08u,0x0cu,0x10u})out.putf(o,0.f);
    out.putf(0x14,st(c*zn*2));
    out.putf(0x18,0.f);out.putf(0x1c,0.f);
    out.putf(0x20,st((X(l)+r)*a));
    out.putf(0x24,st((X(b)+t)*c));
    const X q=X(zf)/(X(zn)-zf);
    out.putf(0x28,st(q));
    out.putf(0x30,0.f);out.putf(0x34,0.f);
    out.putf(0x38,st(q*zn));
    out.putf(0x3c,0.f);
}
// D3DXMatrixOrthoOffCenterRH (d3dx9_29 44220C): 1/(r-l) kept in a register,
// 1/(t-b) and 1/(zn-zf) stored as floats before their later uses.
void pc_d3dx_ortho_off_center_rh(Bytes out,float l,float r,float b,float t,float zn,float zf){
    out.check(0,64);
    const X a=X(1)/(X(r)-l);
    const float c=st(X(1)/(X(t)-b));
    out.putf(0x00,st(a*2));
    for(unsigned o:{0x04u,0x08u,0x0cu,0x10u})out.putf(o,0.f);
    out.putf(0x14,st(X(c)*2));
    for(unsigned o:{0x18u,0x1cu,0x20u,0x24u})out.putf(o,0.f);
    const float q=st(X(1)/(X(zn)-zf));
    out.putf(0x28,q);
    out.putf(0x2c,0.f);
    out.putf(0x30,st(-((X(l)+r)*a)));
    out.putf(0x34,st(-((X(b)+t)*c)));
    out.putf(0x38,st(X(q)*zn));
    out.putf(0x3c,1.f);
}
// D3DXMatrixMultiply generic x87 body (d3dx9_29 440943): per output element,
// ((p0+p1)+p2)+p3 with p = a[i]*b[j] in this order (it matters at 24-bit precision).
constexpr std::uint8_t D3dxMultiplyOrder[16][4][2]{
    {{3,12},{0,0},{2,8},{1,4}},
    {{1,5},{3,13},{2,9},{0,1}},
    {{3,14},{0,2},{2,10},{1,6}},
    {{3,15},{2,11},{0,3},{1,7}},
    {{7,12},{4,0},{5,4},{6,8}},
    {{6,9},{7,13},{5,5},{4,1}},
    {{5,6},{7,14},{4,2},{6,10}},
    {{5,7},{7,15},{6,11},{4,3}},
    {{11,12},{8,0},{9,4},{10,8}},
    {{11,13},{10,9},{9,5},{8,1}},
    {{9,6},{8,2},{11,14},{10,10}},
    {{11,15},{10,11},{9,7},{8,3}},
    {{15,12},{14,8},{12,0},{13,4}},
    {{15,13},{13,5},{14,9},{12,1}},
    {{14,10},{13,6},{12,2},{15,14}},
    {{15,15},{13,7},{12,3},{14,11}}};
PcMatrix16 pc_d3dx_matrix_multiply(const PcMatrix16& a,const PcMatrix16& b){
#if OR2_X87_FAST && !defined(OR2_F24_OFF)
    if(f24_dot_range(a.data(),16)&&f24_dot_range(b.data(),16)){   // binary32, no range checks needed (pc_x87_f24.hpp)
        PcMatrix16 out{};
        for(unsigned e=0;e<16;++e){
            const auto& o=D3dxMultiplyOrder[e];
            out[e]=((a[o[0][0]]*b[o[0][1]]+a[o[1][0]]*b[o[1][1]])+a[o[2][0]]*b[o[2][1]])+a[o[3][0]]*b[o[3][1]];
        }
        return out;
    }
#endif
    return run24([&](auto t){
        using T=decltype(t);
        PcMatrix16 out{};
        for(unsigned e=0;e<16;++e){
            const auto& o=D3dxMultiplyOrder[e];
            const T p0=T(a[o[0][0]])*T(b[o[0][1]]),p1=T(a[o[1][0]])*T(b[o[1][1]]),p2=T(a[o[2][0]])*T(b[o[2][1]]),p3=T(a[o[3][0]])*T(b[o[3][1]]);
            out[e]=st(((p0+p1)+p2)+p3);
        }
        return out;});
}
PcMatrix16 pc_d3dx_matrix_transpose(const PcMatrix16& m){
    PcMatrix16 t{};for(unsigned r=0;r<4;++r)for(unsigned c=0;c<4;++c)t[c*4+r]=m[r*4+c];return t;
}
bool pc_d3dx_matrix_inverse(PcMatrix16& out,const PcMatrix16& in){
    PcMatrix16 copy=in,result=out;
    if(!pc_d3dx_matrix_inverse(Bytes(result.data(),64),nullptr,Bytes(copy.data(),64)))return false;
    out=result;return true;
}
PcVec3 pc_d3dx_vec3_transform_coord(const PcVec3& v,const PcMatrix16& m){
#if OR2_X87_FAST && !defined(OR2_F24_OFF)
    if(f24_dot_range(m.data(),16)&&f24_dot_range(v.data(),3)){
        std::array<float,4> q{};
        for(unsigned c=0;c<4;++c)q[c]=((v[0]*m[c]+v[1]*m[4+c])+v[2]*m[8+c])+m[12+c];
        const float difference=q[3]-1.0f;   // in range too
        if(difference>=-1.1920928955078125e-7f&&difference<=1.1920928955078125e-7f)return PcVec3{q[0],q[1],q[2]};
    }
#endif
    return run24([&](auto t){
        using T=decltype(t);
        std::array<float,4> q{};
        for(unsigned c=0;c<4;++c)q[c]=st(((T(v[0])*T(m[c])+T(v[1])*T(m[4+c]))+T(v[2])*T(m[8+c]))+T(m[12+c]));
        const T difference=T(q[3])-T(1.0f);const T epsilon=T(1.1920928955078125e-7f);   // 2^-23, exact in binary32
        if(!(difference>=-epsilon&&difference<=epsilon)){
            const T inv=T(1.0f)/T(q[3]);
            for(unsigned c=0;c<3;++c)q[c]=st(inv*T(q[c]));
        }
        return PcVec3{q[0],q[1],q[2]};});
}
PcVec3 pc_d3dx_vec3_transform_normal(const PcVec3& v,const PcMatrix16& m){
#if OR2_X87_FAST && !defined(OR2_F24_OFF)
    if(f24_dot_range(m.data(),12)&&f24_dot_range(v.data(),3))
        return PcVec3{(v[2]*m[8]+v[1]*m[4])+v[0]*m[0],(v[2]*m[9]+v[1]*m[5])+v[0]*m[1],(v[2]*m[10]+v[1]*m[6])+v[0]*m[2]};
#endif
    return run24([&](auto t){
        using T=decltype(t);
        PcVec3 r{};
        for(unsigned c=0;c<3;++c)r[c]=st((T(v[2])*T(m[8+c])+T(v[1])*T(m[4+c]))+T(v[0])*T(m[c]));
        return r;});
}
PcVec3 pc_d3dx_vec3_normalize(const PcVec3& v){
    X sq=X(v[0])*v[0];sq+=X(v[1])*v[1];sq+=X(v[2])*v[2];
    const float squared=st(sq);
    constexpr float eps=1.1920928955078125e-7f;
    if(std::isfinite(squared)&&std::fabs(squared-1.0f)<=eps)return v;
    if(!(squared>std::numeric_limits<float>::min()))return {0.f,0.f,0.f};
    const X inv=X(1.0f)/x87_sqrt(X(squared));
    return {st(inv*v[0]),st(inv*v[1]),st(inv*v[2])};
}
void pc_d3dx_matrix_scaling(Bytes out,float sx,float sy,float sz){
    out.check(0,64);
    for(unsigned k=0;k<16;++k)out.putf(k*4,0.f);
    out.putf(0x00,sx);out.putf(0x14,sy);out.putf(0x28,sz);out.putf(0x3c,1.f);
}
void pc_d3dx_perspective_fov_rh(Bytes out,float fovy,float aspect,float zn,float zf){
    out.check(0,64);
    const float half=st(X(fovy)*X(0.5f));                 // fmul [4013CC]; fstp
    float c,s;                                            // FSINCOS: cos -> [ebp+0C], sin -> [ebp-04]
    constexpr std::uint32_t indefinite=0xffc00000u;       // x87 real indefinite
    if(std::isnan(half)){c=half;s=half;}                  // QNaN operand propagates to both
    else if(std::isinf(half)){std::memcpy(&c,&indefinite,4);s=c;}          // #IA masked
    else if(std::fabs(half)>=0x1p63f){c=half;std::memcpy(&s,&indefinite,4);} // C2 set: operand kept, second FSTP underflows
    else{c=st(x87_cos(X(half)));s=st(x87_sin(X(half)));}
    const X y=X(c)/X(s);                                  // stays in ST0
    out.putf(0x2c,-1.f);
    out.putf(0x00,st(y/X(aspect)));                       // fdivr st,st(1)
    for(unsigned o:{0x04u,0x08u,0x0cu,0x10u})out.putf(o,0.f);
    out.putf(0x14,st(y));
    for(unsigned o:{0x18u,0x1cu,0x20u,0x24u})out.putf(o,0.f);
    const X q=X(zf)/(X(zn)-X(zf));                        // fsub zf; fdivr zf
    out.putf(0x28,st(q));
    out.putf(0x30,0.f);out.putf(0x34,0.f);
    out.putf(0x38,st(q*X(zn)));
    out.putf(0x3c,0.f);
}
void pc_d3dx_look_at_rh(Bytes out,const PcVec3& eye,const PcVec3& at,const PcVec3& up){
    out.check(0,64);
    PcVec3 z{st(X(eye[0])-X(at[0])),st(X(eye[1])-X(at[1])),st(X(eye[2])-X(at[2]))};
    z=pc_d3dx_vec3_normalize(z);
    // cross(up, z), rounded to float, then normalized (copy at [ebp-18]).
    const PcVec3 cr{st(X(z[2])*X(up[1])-X(z[1])*X(up[2])),
                    st(X(z[0])*X(up[2])-X(z[2])*X(up[0])),
                    st(X(z[1])*X(up[0])-X(z[0])*X(up[1]))};
    const PcVec3 x=pc_d3dx_vec3_normalize(cr);
    const PcVec3 y{st(X(z[1])*X(x[2])-X(z[2])*X(x[1])),
                   st(X(z[2])*X(x[0])-X(z[0])*X(x[2])),
                   st(X(z[0])*X(x[1])-X(z[1])*X(x[0]))};
    const float tx=st(-((X(x[0])*X(eye[0])+X(x[1])*X(eye[1]))+X(x[2])*X(eye[2])));
    const float ty=st(-((X(y[0])*X(eye[0])+X(y[1])*X(eye[1]))+X(y[2])*X(eye[2])));
    const float tz=st(-((X(z[0])*X(eye[0])+X(z[1])*X(eye[1]))+X(z[2])*X(eye[2])));
    out.putf(0x00,x[0]);out.putf(0x10,x[1]);out.putf(0x20,x[2]);
    out.putf(0x04,y[0]);out.putf(0x14,y[1]);out.putf(0x24,y[2]);
    out.putf(0x08,z[0]);out.putf(0x18,z[1]);out.putf(0x28,z[2]);
    out.putf(0x30,tx);out.putf(0x34,ty);out.putf(0x38,tz);
    out.putf(0x0c,0.f);out.putf(0x1c,0.f);out.putf(0x2c,0.f);out.putf(0x3c,1.f);
}
}
