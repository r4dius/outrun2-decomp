#include "driving/pc_matrix_stack.hpp"
#include "driving/pc_x87.hpp"
#include "driving/pc_x87_f24.hpp"
#include <array>
#include <cmath>
#include <cstring>
#include <limits>
#include <stdexcept>
#include <string>
namespace outrun::driving {
namespace {
std::int32_t step(std::int32_t n,std::uint32_t delta){
    const auto u=static_cast<std::uint32_t>(n)+delta;
    std::int32_t v;std::memcpy(&v,&u,4);return v;
}
// Binary32 fast path of the x87 routines below (pc_x87_f24.hpp): body<F24>,
// and body<X87> again when an operation left the binary32 normal range.
#if OR2_X87_FAST && !defined(OR2_F24_OFF)
template<class Body> auto run24(Body&& body){f24_bad=0;auto r=body(F24{});if(f24_bad)r=body(X87{});return r;}
#else
template<class Body> auto run24(Body&& body){return body(X87{});}
#endif
Bytes slot(const PcMatrixStack& s,std::ptrdiff_t offset){
    if(offset<0)throw std::out_of_range("matrix pointer precedes explicit native arena");
    return s.storage.sub(static_cast<std::size_t>(offset),64);
}
}
Bytes PcMatrixStack::current() const {return slot(*this,current_offset);}
CourseProbe pc_normalize_vector_40ef00(CourseProbe v){
    X87 sum=X87(v.x)*v.x;
    sum+=X87(v.y)*v.y;
    sum+=X87(v.z)*v.z;
    const float squared=x87_float(sum);
    constexpr float epsilon=1.1920928955078125e-7f;
    if(std::isfinite(squared)&&std::fabs(squared-1.0f)<=epsilon)return v;
    if(!(squared>std::numeric_limits<float>::min()))return {0.0f,0.0f,0.0f};
    const X87 inv=X87(1.0f)/x87_sqrt(X87(squared));
    return {x87_float(inv*v.x),x87_float(inv*v.y),x87_float(inv*v.z)};
}
CourseProbe pc_matrix_translation(const PcMatrixStack& s){auto b=s.current();return {b.f32(0x30),b.f32(0x34),b.f32(0x38)};}
void pc_matrix_set_translation(PcMatrixStack& s,const CourseProbe& p){auto b=s.current();b.putf(0x30,p.x);b.putf(0x34,p.y);b.putf(0x38,p.z);}
void pc_matrix_push(PcMatrixStack& s){
    const auto next=step(s.depth,1u);
    if(next<s.capacity){
        if(s.current_offset>std::numeric_limits<std::ptrdiff_t>::max()-64)
            throw std::out_of_range("matrix pointer offset overflow");
        auto from=s.current(),to=slot(s,s.current_offset+64);
        std::memcpy(to.data(),from.data(),64);          // adjacent 64-byte slots: no overlap
        s.current_offset+=64;
    }
    s.depth=next; // An overflowing PC push still increments depth.
}
void pc_matrix_push_unit(PcMatrixStack& s){
    const auto next=step(s.depth,1u);
    if(next<s.capacity){
        if(s.current_offset>std::numeric_limits<std::ptrdiff_t>::max()-64)
            throw std::out_of_range("matrix pointer offset overflow");
        auto to=slot(s,s.current_offset+64);
        for(unsigned k=0;k<16;++k)to.put32(k*4,k%5==0?0x3f800000u:0u);
        s.current_offset+=64;
    }
    s.depth=next; // Overflow increments depth but leaves pointer/data untouched.
}
void pc_matrix_push_load(PcMatrixStack& s,Bytes matrix){
    matrix.check(0,64);
    const auto next=step(s.depth,1u);
    if(next<s.capacity){
        if(s.current_offset>std::numeric_limits<std::ptrdiff_t>::max()-64)
            throw std::out_of_range("matrix pointer offset overflow");
        auto to=slot(s,s.current_offset+64);
        // Original REP MOVSD copies forward from the explicit source.  Read/write
        // in that order so overlapping native views retain the PC propagation.
        for(unsigned k=0;k<64;k+=4)to.put32(k,matrix.u32(k));
        s.current_offset+=64;
    }
    s.depth=next;
}
void pc_matrix_pop(PcMatrixStack& s){
    const auto next=step(s.depth,0xffffffffu);
    if(next>=0){
        if(s.current_offset<std::numeric_limits<std::ptrdiff_t>::min()+64)
            throw std::out_of_range("matrix pointer offset underflow");
        s.current_offset-=64;
    }
    s.depth=next; // Underflow leaves the pointer unchanged, not depth clamped.
}
void pc_matrix_load(PcMatrixStack& s,Bytes matrix){
    matrix.check(0,64);auto to=s.current();
    // REP MOVSD copies forwards even if the explicit views overlap.
    for(unsigned k=0;k<64;k+=4)to.put32(k,matrix.u32(k));
}
void pc_matrix_load_rotation(PcMatrixStack& s,Bytes matrix){
    matrix.check(0,44);auto to=s.current();
    // Original forward scalar copies: do not replace by a temporary when views overlap.
    for(unsigned row=0;row<3;++row){
        for(unsigned col=0;col<3;++col)to.put32(row*16+col*4,matrix.u32(row*16+col*4));
        to.put32(row*16+12,0);
    }
}
CourseProbe pc_inverse_vector(Bytes m,const CourseProbe& p){
    m.check(0,44);
    float f[11];std::memcpy(f,m.data(),44);
#if OR2_X87_FAST && !defined(OR2_F24_OFF)
    {const float v[3]{p.x,p.y,p.z};
     if(f24_dot_range(f,11)&&f24_dot_range(v,3))   // three-term dot products in range (pc_x87_f24.hpp)
        return CourseProbe{(p.z*f[2]+p.y*f[1])+p.x*f[0],(p.z*f[6]+p.y*f[5])+p.x*f[4],(p.z*f[10]+p.y*f[9])+p.x*f[8]};}
#endif
    return run24([&](auto t){
        using T=decltype(t);
        const T x=p.x,y=p.y,z=p.z;
        return CourseProbe{x87_float((z*T(f[2])+y*T(f[1]))+x*T(f[0])),
            x87_float((z*T(f[6])+y*T(f[5]))+x*T(f[4])),
            x87_float((z*T(f[10])+y*T(f[9]))+x*T(f[8]))};});
}
CourseProbe pc_matrix_inverse_vector(const PcMatrixStack& s,const CourseProbe& p){return pc_inverse_vector(s.current(),p);}
CourseProbe pc_inverse_point(Bytes m,const CourseProbe& p){
    m.check(0,64);
    float f[16];std::memcpy(f,m.data(),64);
#if OR2_X87_FAST && !defined(OR2_F24_OFF)
    {const float v[3]{p.x,p.y,p.z};
     if(f24_dot_range(f,16)&&f24_dot_range(v,3)){   // differences then three-term dot products: in range (multiples of 2^-106)
        const float x=p.x-f[12],y=p.y-f[13],z=p.z-f[14];
        return CourseProbe{(z*f[2]+y*f[1])+x*f[0],(z*f[6]+y*f[5])+x*f[4],(z*f[10]+y*f[9])+x*f[8]};}}
#endif
    return run24([&](auto t){
        using T=decltype(t);
        // No float spill between the original x87 subtractions and dot products.
        const T x=T(p.x)-T(f[12]);
        const T y=T(p.y)-T(f[13]);
        const T z=T(p.z)-T(f[14]);
        return CourseProbe{x87_float((z*T(f[2])+y*T(f[1]))+x*T(f[0])),
            x87_float((z*T(f[6])+y*T(f[5]))+x*T(f[4])),
            x87_float((z*T(f[10])+y*T(f[9]))+x*T(f[8]))};});
}
CourseProbe pc_transform_point(Bytes m,const CourseProbe& p){
    m.check(0,64);
#if OR2_X87_FAST && !defined(OR2_F24_OFF)
    {   // binary32 path (pc_x87_f24.hpp); the x87 model below when it leaves the normal range
        float f[16];std::memcpy(f,m.data(),64);
        f24_bad=0;std::array<float,4> q{};
        const float v[3]{p.x,p.y,p.z};
        if(f24_dot_range(f,16)&&f24_dot_range(v,3))   // (x*m + y*m + z*m) + t: within the dot-product range
            for(unsigned c=0;c<4;++c)q[c]=((p.x*f[c]+p.y*f[4+c])+p.z*f[8+c])+f[12+c];
        else for(unsigned c=0;c<4;++c)q[c]=x87_float(((F24(p.x)*F24(f[c])+F24(p.y)*F24(f[4+c]))+F24(p.z)*F24(f[8+c]))+F24(f[12+c]));
        const F24 difference=F24(q[3])-F24(1.0f);
        if(!(difference.v>=-1.1920928955078125e-7f&&difference.v<=1.1920928955078125e-7f)){
            const F24 inv=F24(1.0f)/F24(q[3]);
            for(unsigned c=0;c<3;++c)q[c]=x87_float(inv*F24(q[c]));
        }
        if(!f24_bad)return {q[0],q[1],q[2]};
    }
#endif
    std::array<float,4> q{};
    for(unsigned c=0;c<4;++c){
        X87 v=X87(p.x)*m.f32(c*4);
        v+=X87(p.y)*m.f32(0x10+c*4);
        v+=X87(p.z)*m.f32(0x20+c*4);
        v+=X87(m.f32(0x30+c*4));
        q[c]=x87_float(v); // Generic D3DX29 spills XYZ before dividing.
    }
    // D3DX29 43F7DF: |w - 1| against FLT_EPSILON after the x87 FSUB.
    constexpr double epsilon=1.1920928955078125e-7;
    const X87 difference=X87(q[3])-X87(1.0f);
    if(!(difference>=-epsilon&&difference<=epsilon)){
        // D3DX29 generic 443AA8: fld1; fdiv w with the x87 divide-by-zero
        // exception masked, so w == 0 yields +/-inf (and NaN propagates).
        const X87 inv=q[3]==0.0f?X87(std::copysign(std::numeric_limits<double>::infinity(),double(q[3])))
                                :X87(1.0f)/X87(q[3]);
        for(unsigned c=0;c<3;++c)q[c]=x87_float(inv*q[c]);
    }
    return {q[0],q[1],q[2]};
}
CourseProbe pc_matrix_inverse_point(const PcMatrixStack& s,const CourseProbe& p){return pc_inverse_point(s.current(),p);}
CourseProbe pc_matrix_point(const PcMatrixStack& s,const CourseProbe& p){return pc_transform_point(s.current(),p);}
CourseProbe pc_matrix_vector(const PcMatrixStack& s,const CourseProbe& p){
    auto m=s.current();m.check(0,44);
    float f[11];std::memcpy(f,m.data(),44);
#if OR2_X87_FAST && !defined(OR2_F24_OFF)
    {const float v[3]{p.x,p.y,p.z};
     if(f24_dot_range(f,11)&&f24_dot_range(v,3))
        return CourseProbe{(p.z*f[8]+p.y*f[4])+p.x*f[0],(p.z*f[9]+p.y*f[5])+p.x*f[1],(p.z*f[10]+p.y*f[6])+p.x*f[2]};}
#endif
    // Generic D3DXVec3TransformNormal: XYZ are each accumulated in extended
    // precision by the mapped x87 helper and spilled once to binary32.
    return run24([&](auto t){
        using T=decltype(t);
        return CourseProbe{x87_float((T(p.z)*T(f[8])+T(p.y)*T(f[4]))+T(p.x)*T(f[0])),
            x87_float((T(p.z)*T(f[9])+T(p.y)*T(f[5]))+T(p.x)*T(f[1])),
            x87_float((T(p.z)*T(f[10])+T(p.y)*T(f[6]))+T(p.x)*T(f[2]))};});
}
}
namespace outrun::driving {
namespace {
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
void multiply_into(Bytes out,Bytes a,Bytes b){
    a.check(0,64);b.check(0,64);out.check(0,64);
    std::array<float,16> temp{}; // D3DX computes into temporary storage for aliasing.
    float fa[16],fb[16];         // every input is read before any output is written
    std::memcpy(fa,a.data(),64);std::memcpy(fb,b.data(),64);
#if OR2_X87_FAST && !defined(OR2_F24_OFF)
    if(f24_dot_range(fa,16)&&f24_dot_range(fb,16)){   // binary32, no range checks needed (pc_x87_f24.hpp)
        for(unsigned e=0;e<16;++e){
            const auto& o=D3dxMultiplyOrder[e];
            temp[e]=((fa[o[0][0]]*fb[o[0][1]]+fa[o[1][0]]*fb[o[1][1]])+fa[o[2][0]]*fb[o[2][1]])+fa[o[3][0]]*fb[o[3][1]];
        }
        std::memcpy(out.data(),temp.data(),64);return;
    }
    {   // binary32 path (pc_x87_f24.hpp)
        f24_bad=0;
        for(unsigned e=0;e<16;++e){
            const auto& o=D3dxMultiplyOrder[e];
            const F24 p0=F24(fa[o[0][0]])*F24(fb[o[0][1]]),p1=F24(fa[o[1][0]])*F24(fb[o[1][1]]);
            const F24 p2=F24(fa[o[2][0]])*F24(fb[o[2][1]]),p3=F24(fa[o[3][0]])*F24(fb[o[3][1]]);
            temp[e]=x87_float(((p0+p1)+p2)+p3);
        }
        if(!f24_bad){std::memcpy(out.data(),temp.data(),64);return;}
    }
#endif
    for(unsigned e=0;e<16;++e){
        const auto& o=D3dxMultiplyOrder[e];
        const X87 p0=X87(fa[o[0][0]])*fb[o[0][1]],p1=X87(fa[o[1][0]])*fb[o[1][1]];
        const X87 p2=X87(fa[o[2][0]])*fb[o[2][1]],p3=X87(fa[o[3][0]])*fb[o[3][1]];
        temp[e]=x87_float(((p0+p1)+p2)+p3);
    }
    std::memcpy(out.data(),temp.data(),64);
}
}
void pc_matrix_identity(PcMatrixStack& s){
    auto m=s.current();
    for(unsigned k=0;k<16;++k)m.put32(k*4,k%5==0?0x3f800000u:0u);
}
void pc_matrix_unit_rotation(PcMatrixStack& s){
    auto m=s.current();
    m.put32(0x00,0x3f800000u);m.put32(0x04,0);m.put32(0x08,0);
    m.put32(0x10,0);m.put32(0x14,0x3f800000u);m.put32(0x18,0);
    m.put32(0x20,0);m.put32(0x24,0);m.put32(0x28,0x3f800000u);
}
void pc_matrix_get(const PcMatrixStack& s,Bytes out){
    out.check(0,64);auto from=s.current();
    // The PC uses forward REP MOVSD. Keep scalar forward order so overlapping
    // bounded views propagate exactly like the original.
    for(unsigned k=0;k<64;k+=4)out.put32(k,from.u32(k));
}
void pc_matrix_store_rotation(const PcMatrixStack& s,Bytes out){
    auto m=s.current();out.check(0,44);
    for(unsigned r=0;r<3;++r)for(unsigned c=0;c<3;++c){const auto k=r*16+c*4;out.put32(k,m.u32(k));}
}
void pc_matrix_multiply_current(PcMatrixStack& s,Bytes left){multiply_into(s.current(),left,s.current());}
namespace {
void pc_sincos_matrix(float angle,float& sn,float& cs,const char* axis){
    // A NaN angle (e.g. the osage chain of a fresh bone set: 514F60 places it on the zeroed
    // bone matrices, whose inverse is not finite) propagates through FSINCOS as on the PC;
    // infinities and |x| >= 2^63 (FSINCOS leaves ST0 unchanged) are refused.
    if(std::isinf(angle)||(!std::isnan(angle)&&std::fabs(angle)>=0x1p63f))
        throw std::domain_error(std::string(axis)+" rotation outside finite original FSINCOS domain");
    // FSINCOS: not affected by the precision control; two binary32 stores.
    sn=x87_float(x87_sin(X87(angle)));
    cs=x87_float(x87_cos(X87(angle)));
}
}
void pc_matrix_rotate_x(PcMatrixStack& s,float angle){
    s.current();float sn,cs;pc_sincos_matrix(angle,sn,cs,"X");
    std::array<float,16> rotation{1.0f,0.0f,0.0f,0.0f,0.0f,cs,sn,0.0f,0.0f,-sn,cs,0.0f,0.0f,0.0f,0.0f,1.0f};
    pc_matrix_multiply_current(s,Bytes(rotation.data(),64));
}
void pc_matrix_rotate_y(PcMatrixStack& s,float angle){
    s.current();float sn,cs;pc_sincos_matrix(angle,sn,cs,"Y");
    std::array<float,16> rotation{cs,0.0f,-sn,0.0f,0.0f,1.0f,0.0f,0.0f,sn,0.0f,cs,0.0f,0.0f,0.0f,0.0f,1.0f};
    pc_matrix_multiply_current(s,Bytes(rotation.data(),64));
}
void pc_matrix_rotate_z(PcMatrixStack& s,float angle){
    s.current();float sn,cs;pc_sincos_matrix(angle,sn,cs,"Z");
    std::array<float,16> rotation{cs,sn,0.0f,0.0f,-sn,cs,0.0f,0.0f,0.0f,0.0f,1.0f,0.0f,0.0f,0.0f,0.0f,1.0f};
    pc_matrix_multiply_current(s,Bytes(rotation.data(),64));
}
void pc_matrix_rotate_axis(PcMatrixStack& s,const CourseProbe& input,float angle){
    // PC mxRotateAxe (0x40A550) delegates to the generic d3dx9_29
    // D3DXMatrixRotationAxis and then left-multiplies the current matrix.
    X87 sq=X87(input.x)*input.x;
    sq+=X87(input.y)*input.y;
    sq+=X87(input.z)*input.z;
    const float squared=x87_float(sq);
    CourseProbe axis=input;
    constexpr float eps=1.1920928955078125e-7f;
    if(!(std::isfinite(squared)&&std::fabs(squared-1.0f)<=eps)){
        if(!(squared>std::numeric_limits<float>::min()))axis={0.0f,0.0f,0.0f};
        else {
            const X87 inv=X87(1.0f)/x87_sqrt(X87(squared));
            axis.x=x87_float(inv*axis.x);
            axis.y=x87_float(inv*axis.y);
            axis.z=x87_float(inv*axis.z);
        }
    }
    float sn,cs;pc_sincos_matrix(angle,sn,cs,"axis");
    const float tf=x87_float(X87(1.0f)-X87(cs));
    const X87 T=tf,X=axis.x,Y=axis.y,Z=axis.z,S=sn,C=cs;
    const float txy=x87_float((Y*X)*T);
    const X87 yzT=(Y*Z)*T,zxT=(Z*X)*T,zs=Z*S,ys=Y*S;
    const float ys_spill=x87_float(ys);const X87 xs=X*S;
    std::array<float,16> r{};
    r[0]=x87_float((X*X)*T+C);r[1]=x87_float(X87(txy)+zs);r[2]=x87_float(zxT-ys);
    r[4]=x87_float(X87(txy)-zs);r[5]=x87_float((Y*Y)*T+C);r[6]=x87_float(yzT+xs);
    r[8]=x87_float(X87(ys_spill)+zxT);r[9]=x87_float(yzT-xs);r[10]=x87_float((Z*Z)*T+C);
    r[15]=1.0f;
    pc_matrix_multiply_current(s,Bytes(r.data(),64));
}
void pc_matrix_translate_vector(PcMatrixStack& s,const CourseProbe& v){
    std::array<float,16> translation{1.0f,0.0f,0.0f,0.0f,0.0f,1.0f,0.0f,0.0f,0.0f,0.0f,1.0f,0.0f,v.x,v.y,v.z,1.0f};
    multiply_into(s.current(),Bytes(translation.data(),64),s.current());
}
}
