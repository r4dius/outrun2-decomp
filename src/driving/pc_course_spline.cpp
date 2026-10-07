#include "driving/pc_course_spline.hpp"
#include "driving/pc_x87.hpp"
#include <cmath>
#include <stdexcept>
namespace outrun::driving {
namespace {
// Keep SSE scalar operations separated and disable contraction in CMake.
float add(float a,float b){return a+b;}
float sub(float a,float b){return a-b;}
float mul(float a,float b){return a*b;}
float div(float a,float b){return a/b;}
float store(X87 a){return static_cast<float>(a);}
CourseVec3 vsub(const CourseVec3& a,const CourseVec3& b){
    return {store(X87(a.x)-b.x),store(X87(a.y)-b.y),
            store(X87(a.z)-b.z)};
}
X87 dot(const CourseVec3& a,const CourseVec3& b){
    return (X87(a.z)*b.z+X87(a.y)*b.y)+
           X87(a.x)*b.x;
}
bool normalize_scaled(CourseVec3& out,const CourseVec3& v,float scale){
    const X87 len=x87_sqrt((course_vec3_length_squared_x87(v)));
    // PC 0x40F080 compares against the original binary64 literal, not 1e-4f.
    if(!(len>X87(0.0001)))return false;
    const X87 k=X87(scale)/len;
    out={store(k*v.x),store(k*v.y),store(k*v.z)};return true;
}
float projection_parameter(const CourseVec3& query,const CourseVec3& origin,const CourseVec3& direction){
    // PC 0x449860, null destination path used by SolveHermite.
    constexpr float tiny=0x1p-23f;
    if(std::fabs(direction.x)<tiny&&std::fabs(direction.y)<tiny&&std::fabs(direction.z)<tiny)return 0.0f;
    const CourseVec3 rel=vsub(query,origin);
    const float numerator=store(dot(rel,direction));
    return store(X87(numerator)/(course_vec3_length_squared_x87(direction)));
}
CourseVec3 weighted_sum(const CourseVec3& a,float ka,const CourseVec3& b,float kb){
    // PC 0x40F180 deliberately spills b.y, a.z, b.z to binary32 but not X.
    const X87 ax=X87(ka)*a.x,ay=X87(ka)*a.y;
    const float az=store(X87(ka)*a.z);
    const X87 bx=X87(kb)*b.x;
    const float by=store(X87(kb)*b.y),bz=store(X87(kb)*b.z);
    return {store(bx+ax),store(X87(by)+ay),store(X87(bz)+az)};
}
}
CourseQuad course_quad_vertices(Bytes r){
    r.check(0,0x30);CourseQuad q{};
    for(unsigned i=0;i<4;++i)q[i]={r.f32(i*12),r.f32(i*12+4),r.f32(i*12+8)};
    return q;
}
X87 course_vec3_length_squared_x87(const CourseVec3& v){
    return (X87(v.x)*v.x+X87(v.y)*v.y)+X87(v.z)*v.z;
}
X87 course_vec3_distance_x87(const CourseVec3& a,const CourseVec3& b){
    const X87 x=X87(a.x)-b.x,y=X87(a.y)-b.y,z=X87(a.z)-b.z;
    // PC distance sums Z, X, Y (length_squared instead sums X, Y, Z).
    return x87_sqrt((z*z+x*x)+y*y);
}
long double course_vec3_length_squared(const CourseVec3& v){return course_vec3_length_squared_x87(v).v;}
long double course_vec3_distance(const CourseVec3& a,const CourseVec3& b){return course_vec3_distance_x87(a,b).v;}
void calc_normal_to_delta(const CourseVec3& a,const CourseVec3& b,const CourseVec3& na,
                          const CourseVec3& nb,float scale,CourseVec3& da,CourseVec3& db){
    const float dx=sub(a.x,b.x),dz=sub(b.z,a.z);
    const float target=mul(mul(add(mul(dz,dz),mul(dx,dx)),scale),scale);
    const CourseVec3 va{sub(0.0f,mul(na.y,dx)),sub(mul(na.x,dx),mul(na.z,dz)),mul(na.y,dz)};
    const CourseVec3 vb{sub(0.0f,mul(nb.y,dx)),sub(mul(nb.x,dx),mul(nb.z,dz)),mul(nb.y,dz)};
    float la=store((course_vec3_length_squared_x87(va))),lb=store((course_vec3_length_squared_x87(vb)));
    if(la>0.0f)la=store(x87_sqrt(X87(div(target,la))));
    if(lb>0.0f)lb=store(x87_sqrt(X87(div(target,lb))));
    da={mul(la,va.x),mul(la,va.y),mul(la,va.z)};
    db={mul(lb,vb.x),mul(lb,vb.y),mul(lb,vb.z)};
}
void calc_hermite_direction_vector2(CourseQuad& u,CourseQuad& v,const CourseQuad& p,const CourseSplineNeighbors& n){
    v[0]=vsub(p[3],n.back?(*n.back)[0]:p[0]);
    v[1]=vsub(p[2],n.back?(*n.back)[1]:p[1]);
    v[2]=vsub(n.forward?(*n.forward)[2]:p[2],p[1]);
    v[3]=vsub(n.forward?(*n.forward)[3]:p[3],p[0]);
    u[0]=vsub(p[1],n.left?(*n.left)[0]:p[0]);
    u[3]=vsub(p[2],n.left?(*n.left)[3]:p[3]);
    u[1]=vsub(n.right?(*n.right)[1]:p[1],p[0]);
    u[2]=vsub(n.right?(*n.right)[2]:p[2],p[3]);
}
void calc_hermite_tangent(CourseTangents& out,const CourseQuad& p,const CourseQuad& normals,CourseSplineTuning k){
    calc_normal_to_delta(p[0],p[1],normals[0],normals[1],k.lateral,out[0],out[1]);
    calc_normal_to_delta(p[3],p[2],normals[3],normals[2],k.lateral,out[2],out[3]);
    calc_normal_to_delta(p[0],p[3],normals[0],normals[3],k.longitudinal,out[4],out[5]);
    calc_normal_to_delta(p[1],p[2],normals[1],normals[2],k.longitudinal,out[6],out[7]);
}
std::uint8_t calc_hermite_tangent2(CourseTangents& out,const CourseQuad& p,const CourseSplineNeighbors& n,CourseSplineTuning k){
    CourseQuad u{},v{};calc_hermite_direction_vector2(u,v,p,n);
    const float s0=store((course_vec3_distance_x87(p[0],p[1]))*k.lateral);
    const float s1=store((course_vec3_distance_x87(p[3],p[2]))*k.lateral);
    const float s2=store((course_vec3_distance_x87(p[0],p[3]))*k.longitudinal);
    const float s3=store((course_vec3_distance_x87(p[1],p[2]))*k.longitudinal);
    const std::array<CourseVec3,8> dirs{u[0],u[1],u[3],u[2],v[0],v[3],v[1],v[2]};
    const std::array<float,8> scales{s0,s0,s1,s1,s2,s2,s3,s3};std::uint8_t mask=0;
    for(unsigned i=0;i<8;++i)if(normalize_scaled(out[i],dirs[i],scales[i]))mask|=std::uint8_t(1u<<i);
    return mask;
}
std::array<float,3> calc_hermite_coefficients(const CourseVec3& a,const CourseVec3& b,const CourseVec3& ta,const CourseVec3& tb){
    const float dx=store(X87(b.x)-a.x),dz=store(X87(b.z)-a.z);
    const bool use_x=std::fabs(dx)>std::fabs(dz);
    const float inv=div(1.0f,use_x?dx:dz),c1=use_x?ta.x:ta.z,c2=use_x?tb.x:tb.z;
    return {sub(mul(add(c2,c1),inv),2.0f),sub(3.0f,mul(add(mul(c1,2.0f),c2),inv)),mul(inv,c1)};
}
float calc_carry_variable(float value,const std::array<float,3>& c){
    const float a=c[0],b=c[1],d=c[2];
    if(a==0.0f)return value;
    const float extremum=div(b,mul(a,-3.0f));
    if(0.0f>=extremum||extremum>=1.0f)return value;
    const float constant=sub(0.0f,value);
    const float check=add(mul(add(mul(add(mul(extremum,a),b),extremum),d),extremum),constant);
    if(check==0.0f)return extremum;
    for(unsigned j=0;j<3;++j){
        const float va=mul(value,a);
        const float residual=add(mul(add(mul(add(va,b),value),d),value),constant);
        if(residual==0.0f)break;
        const float slope=add(mul(add(mul(va,3.0f),mul(b,2.0f)),value),d);
        value=sub(value,div(residual,slope));
    }
    return value;
}
void solve_hermite(CourseVec3& out,const CourseVec3& point,float u,float v,const CourseQuad& p,const CourseTangents& t){
    // The PC also computes/discards two lateral coefficient triplets. They
    // produce no external writes and do not enter its result calculation.
    const auto ca=calc_hermite_coefficients(p[0],p[3],t[4],t[5]);
    const auto cb=calc_hermite_coefficients(p[1],p[2],t[6],t[7]);
    const float va=calc_carry_variable(v,ca),vb=calc_carry_variable(v,cb);
    const float s=add(mul(sub(vb,va),u),va);
    const float q=sub(mul(s,s),s),he=mul(q,s),hs=sub(he,q);
    const float pe=sub(0.0f,mul(sub(mul(q,2.0f),s),s)),ps=sub(1.0f,pe);
    CourseVec3 a{},b{};
    a.x=add(add(add(mul(t[4].x,hs),mul(p[3].x,pe)),mul(t[5].x,he)),mul(p[0].x,ps));
    a.y=add(add(add(mul(p[3].y,pe),mul(p[0].y,ps)),mul(t[4].y,hs)),mul(t[5].y,he));
    a.z=add(add(add(mul(p[3].z,pe),mul(p[0].z,ps)),mul(t[4].z,hs)),mul(t[5].z,he));
    b.x=add(add(add(mul(p[2].x,pe),mul(t[6].x,hs)),mul(p[1].x,ps)),mul(t[7].x,he));
    b.y=add(add(add(mul(p[1].y,ps),mul(p[2].y,pe)),mul(t[6].y,hs)),mul(t[7].y,he));
    b.z=add(add(add(mul(p[1].z,ps),mul(p[2].z,pe)),mul(t[6].z,hs)),mul(t[7].z,he));
    CourseVec3 direction=vsub(b,a);direction.y=0.0f;
    const float frac=projection_parameter({point.x,0.0f,point.z},a,direction);
    out=weighted_sum(a,store(X87(1.0f)-frac),b,frac);
}
float calc_hermite2(const CourseVec3& point,float u,float v,const CourseQuad& p,const CourseQuad& normals,
                    const CourseSplineNeighbors& n,CourseSplineTuning tuning){
    CourseTangents t{};
    if(n.forward&&n.back){
        if(calc_hermite_tangent2(t,p,n,tuning)!=0xffu)
            throw std::domain_error("PC Hermite neighbor path leaves undefined stack tangents for degenerate geometry");
    }else calc_hermite_tangent(t,p,normals,tuning);
    CourseVec3 out{};solve_hermite(out,point,u,v,p,t);return out.y;
}
float calc_y_pos_spl(float x,float z,const CourseQuad& p,const CourseQuad& normals,
                     const CourseSplineNeighbors& n,CourseSplineTuning tuning){
    float u=0.5f,v=0.5f;
    const float ax=sub(p[2].x,p[1].x),az=sub(p[2].z,p[1].z);
    const float bx=sub(p[3].x,p[0].x),bz=sub(p[3].z,p[0].z);
    const float px=sub(p[0].x,x),pz=sub(p[0].z,z),qx=sub(p[1].x,x),qz=sub(p[1].z,z);
    const float a=sub(mul(ax,bz),mul(az,bx));
    const float c=sub(mul(qx,pz),mul(qz,px));
    float b=sub(add(mul(pz,ax),mul(qx,bz)),add(mul(px,az),mul(qz,bx)));
    bool compute=false;
    if(std::fabs(a)>0.00001f){
        const float twice_a=mul(a,2.0f);
        const float discr=sub(mul(b,b),mul(mul(c,2.0f),twice_a));
        const float root=store(x87_sqrt(X87(discr)));
        b=b>0.0f?div(sub(sub(0.0f,b),root),twice_a):div(sub(root,b),twice_a);
        const float other=div(c,mul(b,a));
        // Jb treats unordered as taken; do not replace with <= here.
        if(!(std::fabs(sub(b,0.5f))<std::fabs(sub(other,0.5f))) &&
           !std::isnan(std::fabs(sub(b,0.5f))) && !std::isnan(std::fabs(sub(other,0.5f))))b=other;
        // PC tolerance constants are documented/tested separately below.
        if(!(b<-0.001f||b>1.001f)){
            if(b<0.0f)b=0.0f;else if(b>1.0f)b=1.0f;compute=true;
        }
    }else if(b!=0.0f){
        b=sub(0.0f,div(c,b));
        if(!(b<=-0.01f||b>=1.01f)){
            if(b<0.0f)b=0.0f;else if(b>1.0f)b=1.0f;compute=true;
        }
    }
    if(compute){
        const float ex=sub(p[1].x,p[0].x),ez=sub(p[1].z,p[0].z);
        const float dx=add(mul(sub(sub(p[2].x,p[3].x),ex),b),ex);
        const float dz=add(mul(sub(sub(p[2].z,p[3].z),ez),b),ez);
        float h;
        if(!(std::fabs(dz)<std::fabs(dx))){h=sub(0.0f,add(mul(b,bz),pz));if(h!=0.0f)h=div(h,dz);}
        else {h=sub(0.0f,add(mul(b,bx),px));if(h!=0.0f)h=div(h,dx);}
        if(h<0.0f)h=0.0f;else if(h>1.0f)h=1.0f;
        u=b;v=h;
    }
    // PC passes the lateral coordinate first, longitudinal coordinate second.
    return calc_hermite2({x,0.0f,z},v,u,p,normals,n,tuning);
}
} // namespace outrun::driving
