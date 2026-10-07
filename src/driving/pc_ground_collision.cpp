#include "driving/pc_ground_collision.hpp"
#include "driving/pc_x87.hpp"
#include <cmath>
#include <limits>
#include <stdexcept>
namespace outrun::driving {
namespace {
CourseProbe read3(Bytes b,std::size_t o){return {b.f32(o),b.f32(o+4),b.f32(o+8)};}
void write3(Bytes b,std::size_t o,const CourseProbe& v){b.putf(o,v.x);b.putf(o+4,v.y);b.putf(o+8,v.z);}
const CourseCollisionTables& selected(const CourseWorldTables& t,std::uint32_t type){
    if(type>=t.courses.size()||t.courses[type].load_type!=type)
        throw std::out_of_range("ground collision course selector/view mismatch");
    return t.courses[type];
}
CourseProbe sub(const CourseProbe& a,const CourseProbe& b){
    return {static_cast<float>(X87(a.x)-b.x),
            static_cast<float>(X87(a.y)-b.y),
            static_cast<float>(X87(a.z)-b.z)};
}
CourseProbe cross(const CourseProbe& a,const CourseProbe& b){
    return {static_cast<float>(X87(a.y)*b.z-X87(a.z)*b.y),
            static_cast<float>(X87(a.z)*b.x-X87(a.x)*b.z),
            static_cast<float>(X87(a.x)*b.y-X87(a.y)*b.x)};
}
CourseProbe normalized(CourseProbe v){
    // PC mxNormalizeVector calls the pinned generic D3DX29 Normalize helper.
    // The square sum spills to binary32 before the epsilon/zero tests.
    X87 sum=X87(v.x)*v.x;
    sum+=X87(v.y)*v.y;
    sum+=X87(v.z)*v.z;
    const float squared=static_cast<float>(sum);
    constexpr float epsilon=1.1920928955078125e-7f;
    if(std::isfinite(squared)&&std::fabs(squared-1.0f)<=epsilon)return v;
    if(!(squared>std::numeric_limits<float>::min()))return {0,0,0};
    const X87 inv=X87(1.0f)/x87_sqrt(X87(squared));
    return {static_cast<float>(v.x*inv),static_cast<float>(v.y*inv),static_cast<float>(v.z*inv)};
}
std::int32_t midpoint(std::int32_t first,std::int32_t last){
    const auto bits=static_cast<std::uint32_t>(first)+static_cast<std::uint32_t>(last);
    std::int32_t sum;std::memcpy(&sum,&bits,4);return sum/2;
}
}
std::int32_t find_selected_course_run(const CourseWorldTables& tables,Bytes selector,
    std::int32_t hint,std::int32_t& first,std::int32_t& last){
    selector.check(0,10);const auto type=selector.u32(0);const auto& t=selected(tables,type);
    return type==0u?find_primary_course_run(t.runs,selector.i16(8),hint,first,last):
                    find_secondary_course_run(t,selector.i16(8),selector.i32(4),first,last);
}
void calc_ground_coli_face(Bytes e,Bytes w,Bytes p,const std::array<Bytes,4>& wheels,GroundCollisionContext& context){
    e.check(0,0x298);w.check(0x628,0x24);p.check(0x27u*0x4cu,4);
    for(auto wheel:wheels)wheel.check(0,0x40);
    auto& q=context.world;
    CourseProbe center=pc_matrix_translation(q.matrices),probe=center;
    const auto old_type=e.u32(0x5c);
    auto index=e.u32(0x230),special=e.u32(0x1c0),kind=e.u32(0x248);
    const auto type=get_y_position_spl_chk(q,probe,&index,&special,&kind);
    if(kind!=1u){
        center=probe;e.put32(0x230,index);e.put32(0x1c0,special);
        e.put32(0x248,kind);e.put32(0x5c,type);
    }
    const auto& course=selected(q.tables,e.u32(0x5c));
    // Unlike GetCourseLength, the inline original does not check the root.
    e.put16(0x64,course_length(course.runs.lengths,e.i32(0x230)));
    auto ext=[&](std::int32_t i){return course_collision_ext_flags(course.polygons,i,course.polygons_present);};
    if(e.i32(0x1c0)==e.i32(0x230))e.put16(0x25c,ext(e.i32(0x1c0)));
    else{
        if(e.u32(0x5c)!=old_type)e.puti(0x1c0,0);
        std::int32_t first=0,last=0;
        const auto n=find_selected_course_run(q.tables,e.sub(0x5c,10),e.i32(0x1c0),first,last);
        const auto candidate=n>0?midpoint(first,last):e.i32(0x1c0);
        const auto current_flags=ext(e.i32(0x230)),candidate_flags=ext(candidate);
        const auto difference=std::uint16_t(candidate_flags^current_flags);
        e.put16(0x25c,std::uint16_t(candidate_flags^(difference&0xc00u)));
        if(!(difference&0xc00u))e.puti(0x1c0,candidate);
    }
    write3(w,0x640,center);
    std::array<CourseProbe,4> contacts{};
    for(unsigned i=0;i<4;++i){
        auto wheel=wheels[i];
        CourseProbe point{wheel.f32(4),static_cast<float>(wheel.f32(0x28)-p.f32(((i>>1)+0x26u)*0x4cu)),wheel.f32(0x0c)};
        point=pc_matrix_point(q.matrices,point);
        auto polygon=wheel.u32(0x10),flags=wheel.u32(0x14);
        get_y_position_spl_chk(q,point,&polygon,nullptr,&flags);
        wheel.put32(0x10,polygon);wheel.put32(0x14,flags);wheel.putf(0x3c,point.y);
        contacts[i]=point;
    }
    const auto normal=normalized(cross(sub(contacts[1],contacts[2]),sub(contacts[0],contacts[3])));
    write3(w,0x628,normal);
    constexpr float offset=0.01f; // PC 0x6281C0, 0x3C23D70A.
    write3(w,0x634,{static_cast<float>(center.x+static_cast<float>(normal.x*offset)),
                   static_cast<float>(center.y+static_cast<float>(normal.y*offset)),
                   static_cast<float>(center.z+static_cast<float>(normal.z*offset))});
    const auto yaw=context.area_yaw_radians[e.u32(0x5c)==0u?0u:1u];
    e.put16(0x294,static_cast<std::uint16_t>(course_collision_offset_direction(course.polygons,e.u32(0x1c0),yaw,course.polygons_present)));
}
} // namespace outrun::driving
