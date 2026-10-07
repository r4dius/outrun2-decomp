#include "driving/pc_wall_geometry.hpp"
#include "driving/pc_x87.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <stdexcept>
namespace outrun::driving {
namespace {
CourseProbe read(Bytes b,std::size_t o=0){return {b.f32(o),b.f32(o+4),b.f32(o+8)};}
void write(Bytes b,const CourseProbe& p){b.putf(0,p.x);b.putf(4,p.y);b.putf(8,p.z);}
void copy3(Bytes to,Bytes from){for(unsigned k=0;k<12;k+=4)to.put32(k,from.u32(k));}
X87 dot(const CourseProbe& a,const CourseProbe& b){
    const X87 z=X87(a.z)*b.z;
    const X87 y=X87(a.y)*b.y;
    const X87 x=X87(a.x)*b.x;
    return (z+y)+x; // mxInnerProduct returns x87 without an implicit float spill.
}
CourseProbe sub(const CourseProbe& a,const CourseProbe& b){return {
    static_cast<float>(X87(a.x)-b.x),
    static_cast<float>(X87(a.y)-b.y),
    static_cast<float>(X87(a.z)-b.z)};}
unsigned shape_count(Bytes shape){
    const auto n=static_cast<unsigned>(std::clamp(shape.i32(0),1,32));
    shape.check(4,std::size_t(n)*12);return n;
}
void check_push(const PcMatrixStack& s){
    s.current();const auto u=static_cast<std::uint32_t>(s.depth)+1u;
    std::int32_t next;std::memcpy(&next,&u,4);
    if(next<s.capacity){
        if(s.current_offset>std::numeric_limits<std::ptrdiff_t>::max()-64)
            throw std::out_of_range("wall matrix push offset overflow");
        const auto off=s.current_offset+64;
        if(off<0)throw std::out_of_range("wall matrix push precedes storage");
        s.storage.check(static_cast<std::size_t>(off),64);
    }
}
}
void cop_coli_point(const CourseWorldTables& w,std::uint32_t polygon,std::uint32_t type,
                    PcMatrixStack& stack,const std::array<Bytes,4>& out){
    if(type>=w.courses.size())throw std::out_of_range("wall course type outside explicit table");
    const auto& c=w.courses[type];
    if(!c.polygons_present)throw std::invalid_argument("CopColiPoint needs the selected polygon table");
    const auto offset=std::uint64_t(polygon)*64u;
    if(offset>std::numeric_limits<std::size_t>::max())throw std::out_of_range("wall polygon offset overflow");
    auto rec=c.polygons.sub(static_cast<std::size_t>(offset),48);
    auto transform=w.transforms[type?1:0];transform.check(0,64);
    for(auto v:out)v.check(0,12);
    check_push(stack);
    std::array<CourseProbe,4> p{};
    // Original protected public entry loads the selected polygon root, copies
    // every vertex before publishing ANY output, then Push/Load/4*Point/Pop.
    // Precompute from the same explicit matrix to reject malformed views first.
    for(unsigned k=0;k<4;++k)p[k]=pc_transform_point(transform,read(rec,k*12));
    pc_matrix_push(stack);pc_matrix_load(stack,transform);
    for(unsigned k=0;k<4;++k)write(out[k],p[k]);
    pc_matrix_pop(stack);
}
void calc_coli_wall_face(float x,float z,const std::array<Bytes,4>& p){
    for(auto v:p)v.check(0,12);
    const auto a=p[0],b=p[1],c=p[2],d=p[3];
    // Scalar SSE order in the PC. Do not widen/reassociate the side test.
    float bz=c.f32(8)+d.f32(8), bx=c.f32(0)+d.f32(0);
    float az=a.f32(8)+b.f32(8), ax=a.f32(0)+b.f32(0);
    float t0=(bz-az)*x,t1=(bx-ax)*z;
    float cross=bz*ax;cross=cross-az*bx;cross=cross*0.5f;
    float side=t1-t0;side=side+cross;
    if(side>=0.0f){copy3(d,b);copy3(a,c);}
    b.putf(0,a.f32(0)-d.f32(0));
    const float dy=a.f32(4)-d.f32(4),dx=b.f32(0);b.putf(4,dy);
    const float dz=a.f32(8)-d.f32(8);b.putf(8,dz);
    float sq=dz*dz;sq=sq+dx*dx;
    const float length=x87_float(x87_sqrt(X87(sq)));
    const float inv=1.0f/length;
    b.putf(8,b.f32(0)*inv);b.putf(4,0.0f);b.putf(0,0.0f-inv*dz);
    c.putf(0,b.f32(0)*0.01f+a.f32(0));
    c.put32(4,b.u32(4)); // Y=0, not a.y. This is an observed PC quirk.
    c.putf(8,b.f32(8)*0.01f+a.f32(8));
}
WallPushResult push_outpos_mat_obsolete(Bytes work,Bytes face,Bytes shape,const PcMatrixStack& s){
    work.check(0x40,12);face.check(0,36);const auto n=shape_count(shape);s.current();
    const auto normal=read(face),inside=read(face,12),outside=read(face,24);
    float minimum=0.0f;std::uint32_t mask=0;
    for(unsigned k=0;k<n;++k){
        const auto world=pc_matrix_point(s,read(shape,4+k*12));
        // Both mxSubVector results spill; both dot results also spill before comparison.
        const float inner=static_cast<float>(dot(sub(world,inside),normal));
        const float outer=static_cast<float>(dot(sub(world,outside),normal));
        if(inner<0.0f)mask|=0x80000000u>>k;
        if(minimum>outer)minimum=outer;
    }
    const X87 distance=x87_abs(X87(minimum));
    const auto previous=read(work,0x40);
    const CourseProbe next={
        static_cast<float>(X87(normal.x)*distance+previous.x),
        static_cast<float>(X87(normal.y)*distance+previous.y),
        static_cast<float>(X87(normal.z)*distance+previous.z)};
    write(work.sub(0x40,12),next);
    return {mask,static_cast<float>(distance)};
}
bool coli_set_obsolete(Bytes work,Bytes face,Bytes shape,std::uint32_t mask,PcMatrixStack& s,Bytes result){
    work.check(0x10,44);face.check(0,12);result.check(0,12);
    const auto n=shape_count(shape);check_push(s);
    const auto normal=pc_inverse_vector(work.sub(0x10,44),read(face));
    struct Hit{unsigned index;float projection;};std::array<Hit,12> hits{};
    unsigned used=0;float sum=0.0f;
    for(unsigned k=0;k<n;++k){
        if(!(mask&(0x80000000u>>k)))continue;
        const X87 raw=dot(normal,read(shape,4+k*12));
        const float projected=static_cast<float>(raw);
        // PC compares the STILL-EXTENDED dot, not its stored float weight.
        if(raw<X87(0.0f)){
            if(used==hits.size())throw std::out_of_range("ColiSetObsolete exceeds original 12-hit local storage");
            sum=projected+sum;hits[used++]={k,projected};
        }
    }
    CourseProbe out{};
    if(used){
        for(unsigned j=0;j<used;++j){
            const auto point=read(shape,4+hits[j].index*12);const float weight=hits[j].projection;
            out.x=point.x*weight+out.x;out.y=point.y*weight+out.y;out.z=point.z*weight+out.z;
        }
        const float inv=1.0f/sum;out.x=inv*out.x;out.y=inv*out.y;out.z=inv*out.z;
    }
    pc_matrix_push(s);pc_matrix_load_rotation(s,work.sub(0x10,44));
    result.putf(0,out.x);result.putf(8,out.z);result.putf(4,out.y);
    pc_matrix_pop(s);return used!=0;
}
}
