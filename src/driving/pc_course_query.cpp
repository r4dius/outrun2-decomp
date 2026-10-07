#include "driving/pc_course_query.hpp"
#include "driving/pc_x87.hpp"
#include "driving/pc_collision.hpp"
#include <cmath>
#include <limits>
#include <stdexcept>
namespace outrun::driving {
bool course_run_geometry_43e570(const CourseCollisionTables& t,Bytes request,Bytes out,std::int32_t hint){
    request.check(0,10);out.check(0,0x58);
    unsigned maximum=0;
    if(t.runs.present){const auto count=t.runs.header.i32(0xc);if(count<0)throw std::out_of_range("negative course count");
        if(count)maximum=std::uint16_t(t.runs.lengths.i16(std::size_t(count-1)*2));}
    if(request.i16(8)>int(maximum))request.put16(8,std::uint16_t(maximum));
    for(unsigned k=0;k<3;++k)out.put32(8+k*4,0);
    std::int32_t first=out.i32(0),last=out.i32(4);
    const auto found=request.u32(0)==0?find_primary_course_run(t.runs,request.i16(8),hint,first,last):
        find_secondary_course_run(t,request.i16(8),request.i32(4),first,last);
    out.puti(0,first);out.puti(4,last);
    if(!found||!t.polygons_present)return false;
    if(first<0||last<0)throw std::out_of_range("negative environment course index");
    auto a=t.polygons.sub(std::size_t(first)*0x40,0x40),b=t.polygons.sub(std::size_t(last)*0x40,0x40);
    for(unsigned k=0;k<3;++k){out.put32(0x24+k*4,a.u32(k*4));out.put32(0x30+k*4,b.u32(12+k*4));
        out.put32(0x48+k*4,b.u32(24+k*4));out.put32(0x3c+k*4,a.u32(36+k*4));}
    for(unsigned at:{0x24u,0x30u,0x48u,0x3cu})for(unsigned k=0;k<3;++k)out.putf(8+k*4,out.f32(8+k*4)+out.f32(at+k*4));
    for(unsigned k=0;k<3;++k)out.putf(8+k*4,out.f32(8+k*4)*.25f);
    out.putf(0x54,x87_float(course_vec3_distance_x87({out.f32(0x3c),out.f32(0x40),out.f32(0x44)},
        {out.f32(0x48),out.f32(0x4c),out.f32(0x50)})));
    return true;
}
// 43E6C0: 43E570 without the refresh of the run corners: only +54 is recomputed
// from the +3C / +48 points already in the output.
bool course_run_length_43e6c0(const CourseCollisionTables& t,Bytes request,Bytes out,std::int32_t hint){
    request.check(0,10);out.check(0,0x58);
    unsigned maximum=0;
    if(t.runs.present){const auto count=t.runs.header.i32(0xc);if(count<0)throw std::out_of_range("negative course count");
        if(count)maximum=std::uint16_t(t.runs.lengths.i16(std::size_t(count-1)*2));}
    if(request.i16(8)>int(maximum))request.put16(8,std::uint16_t(maximum));
    for(unsigned k=0;k<3;++k)out.put32(8+k*4,0);
    std::int32_t first=out.i32(0),last=out.i32(4);
    const auto found=request.u32(0)==0?find_primary_course_run(t.runs,request.i16(8),hint,first,last):
        find_secondary_course_run(t,request.i16(8),request.i32(4),first,last);
    out.puti(0,first);out.puti(4,last);
    if(!found||!t.polygons_present)return false;
    out.putf(0x54,x87_float(course_vec3_distance_x87({out.f32(0x3c),out.f32(0x40),out.f32(0x44)},
        {out.f32(0x48),out.f32(0x4c),out.f32(0x50)})));
    return true;
}
namespace {
void valid_type(const CourseCollisionTables& t){
    if(t.load_type>3u)throw std::out_of_range("course load type outside four selected tables");
}
std::size_t index_offset(std::int32_t i,std::size_t stride){
    if(i<0)throw std::out_of_range("negative course polygon index");
    return static_cast<std::size_t>(i)*stride;
}
std::int16_t signed_word(std::uint16_t u){std::int16_t s;std::memcpy(&s,&u,2);return s;}
std::uint16_t length(const CourseCollisionTables& t,std::int32_t i){
    if(i==-1)return 0;
    return static_cast<std::uint16_t>(t.runs.lengths.i16(index_offset(i,2)));
}
std::uint16_t flags(const CourseCollisionTables& t,std::int32_t i){
    if(!t.polygons_present||i==-1)return 0;
    return static_cast<std::uint16_t>(t.polygons.i16(index_offset(i,0x40)+0x3c));
}
std::uint32_t kind_bit(const CourseCollisionTables& t,std::int32_t i){
    return 1u<<(t.kinds.u8(index_offset(i,1))&31u);
}
std::int32_t count(const CourseCollisionTables& t,std::size_t offset){
    const auto n=t.runs.header.i32(offset);
    if(n<0)throw std::out_of_range("negative course record count");
    return n;
}
CourseQuad polygon(const CourseCollisionTables& t,std::int32_t i){
    if(!t.polygons_present)throw std::invalid_argument("missing course polygon table");
    return course_quad_vertices(t.polygons.sub(index_offset(i,0x40),0x40));
}
std::int32_t run_at(const CourseCollisionTables& t,std::int16_t value,std::int32_t hint,
                    std::int32_t& first,std::int32_t& last){
    return t.load_type==0u?find_primary_course_run(t.runs,value,hint,first,last):
                           find_secondary_course_run(t,value,0,first,last);
}
// The PC does not subtract the products before comparing: this preserves two
// binary32 multiplications and the ordered COMISS > condition, including ties.
CourseProbe d3dx_normalized(CourseProbe v){
    return pc_normalize_vector_40ef00(v);
}
void add_spilled(CourseProbe& a,const CourseProbe& b){
    a.x=static_cast<float>(X87(a.x)+b.x);
    a.y=static_cast<float>(X87(a.y)+b.y);
    a.z=static_cast<float>(X87(a.z)+b.z);
}
bool outside_edge(const CourseVec3& a,const CourseVec3& b,float x,float z){
    const float lhs=static_cast<float>(static_cast<float>(b.z-a.z)*static_cast<float>(x-a.x));
    const float rhs=static_cast<float>(static_cast<float>(b.x-a.x)*static_cast<float>(z-a.z));
    return lhs>rhs;
}
}
CourseProbe course_collision_world_normal(const CourseCollisionTables& t,std::int32_t i,
                                                  PcMatrixStack& matrices,Bytes transform){
    valid_type(t);
    if(t.normals.size()==0)return {0.0f,1.0f,0.0f};
    transform.check(0,64);
    const auto rec=t.normals.sub(index_offset(i,0x30),0x30);
    CourseProbe n{0.0f,0.0f,0.0f};
    for(unsigned k=0;k<4;++k)add_spilled(n,{rec.f32(k*12u),rec.f32(k*12u+4u),rec.f32(k*12u+8u)});
    n=d3dx_normalized(n);
    pc_matrix_push(matrices);
    try{
        pc_matrix_load(matrices,transform);
        n=pc_matrix_vector(matrices,n);
        pc_matrix_pop(matrices);
    }catch(...){
        // Native invalid-view failures are transactional with respect to the
        // explicit stack state, unlike undefined guest-pointer faults.
        pc_matrix_pop(matrices);
        throw;
    }
    return n;
}

std::int32_t find_secondary_course_run(const CourseCollisionTables& t,std::int16_t requested,
                                      std::int32_t mode,std::int32_t& first,std::int32_t& last){
    valid_type(t);
    if(!t.runs.present)return 0;
    const auto n=count(t,0x0c);
    std::int32_t pos=0;
    if(mode!=101&&mode!=103){
        while(pos<n&&length(t,pos)!=requested)++pos;
        const auto begin=pos;
        // This +1 is a BYTE offset in the actual PC instruction, not the next
        // u16 element. Do not 'repair' it to +2 based on a guessed layout.
        while(pos<n-1&&static_cast<std::uint16_t>(t.runs.lengths.i16(index_offset(pos,2)+1))==requested)++pos;
        first=begin;last=pos;return pos-begin+1;
    }
    auto skip_spline=[&](std::int32_t p){
        if(flags(t,p)&0x400u)while(p<n&&!(flags(t,p)&0x800u))++p;
        return p;
    };
    while(pos<n){pos=skip_spline(pos);if(length(t,pos)==requested)break;++pos;}
    const auto begin=pos;
    while(pos<n-1){
        pos=skip_spline(pos);
        const auto next=pos+1;
        if(length(t,next)!=requested)break;
        pos=next;
    }
    first=begin;last=pos;return pos-begin+1;
}
bool coli_get_forward_polygon_number(const CourseCollisionTables& t,std::int32_t i,std::int32_t& out){
    valid_type(t);if(t.load_type==1u)return false;
    const auto here=signed_word(length(t,i));
    std::int32_t maximum=0;
    if(t.runs.present){const auto n=count(t,0x0c);if(n)maximum=length(t,n-1);}
    if(here>=maximum)return false;
    const auto bit=kind_bit(t,i);std::int32_t step=0;
    if(bit&0xf00002u){
        std::int32_t a=0,b=0;
        step=run_at(t,here,i,a,b);
        if(step!=run_at(t,signed_word(static_cast<std::uint16_t>(here+1)),i,a,b))return false;
    }else{
        if(!(bit&0x3b00u)||!t.runs.present)return false;
        const auto n=count(t,8);
        if(static_cast<std::uint32_t>(i)>=static_cast<std::uint32_t>(n)-1u)return false;
        if(length(t,i+1)!=static_cast<std::int32_t>(here)+1)return false;
        step=1; // Original .rld 0x103c0dc reads pinned literal 0x1039f00 == 1.
    }
    if(!t.polygons_present)return false;
    const auto target=i+step;
    const auto a=polygon(t,i),b=polygon(t,target);
    if(course_vec3_distance_x87(a[0],b[3])>X87(0.5f)||course_vec3_distance_x87(a[1],b[2])>X87(0.5f))return false;
    out=target;return true;
}
bool coli_get_back_polygon_number(const CourseCollisionTables& t,std::int32_t i,std::int32_t& out){
    valid_type(t);if(t.load_type==1u)return false;
    const auto here=signed_word(length(t,i));if(here<=0)return false;
    const auto bit=kind_bit(t,i);std::int32_t step=0;
    if(bit&0xf00002u){
        std::int32_t a=0,b=0;step=run_at(t,here,i,a,b);
        if(step!=run_at(t,signed_word(static_cast<std::uint16_t>(here-1)),i,a,b))return false;
    }else{
        if(!(bit&0x3b00u)||static_cast<std::uint32_t>(i)==0u)return false;
        if(length(t,i-1)!=static_cast<std::int32_t>(here)-1)return false;
        step=1;
    }
    if(!t.polygons_present)return false;
    const auto target=i-step;
    const auto a=polygon(t,target),b=polygon(t,i);
    if(course_vec3_distance_x87(a[0],b[3])>X87(0.5f)||course_vec3_distance_x87(a[1],b[2])>X87(0.5f))return false;
    out=target;return true;
}
bool coli_get_left_polygon_number(const CourseCollisionTables& t,std::int32_t i,std::int32_t& out){
    valid_type(t);if(!(kind_bit(t,i)&0xf00002u)||t.load_type==1u)return false;
    std::int32_t a=0,b=0;
    if(run_at(t,signed_word(length(t,i)),i,a,b)<=1||i==a)return false;
    out=i-1;return true;
}
bool coli_get_right_polygon_number(const CourseCollisionTables& t,std::int32_t i,std::int32_t& out){
    valid_type(t);if(!(kind_bit(t,i)&0xf00002u)||t.load_type==1u)return false;
    std::int32_t a=0,b=0;
    if(run_at(t,signed_word(length(t,i)),i,a,b)<=1||i==b)return false;
    out=i+1;return true;
}
RoadCondition get_road_cond(const CourseCollisionTables& t,std::uint16_t list_offset,
                            std::uint32_t mode,float x,float reference_y,float z,CourseSplineTuning tuning){
    valid_type(t);
    const auto base=static_cast<std::size_t>(list_offset)*2u;
    const auto n=static_cast<std::uint16_t>(t.area_lists.i16(base));
    t.area_lists.check(base+2u,static_cast<std::size_t>(n)*2u);
    RoadCondition best{std::numeric_limits<float>::max(),-1,0};
    for(unsigned k=0;k<n;++k){
        const auto i=static_cast<std::uint16_t>(t.area_lists.i16(base+2u+k*2u));
        const auto p=polygon(t,i);const auto f=flags(t,i);
        if(outside_edge(p[0],p[1],x,z)||outside_edge(p[1],p[2],x,z))continue;
        if(f&1u){if(outside_edge(p[2],p[0],x,z))continue;}
        else if(outside_edge(p[2],p[3],x,z)||outside_edge(p[3],p[0],x,z))continue;
        float y=p[0].y;
        if(mode==0x100u){
            const auto record=t.polygons.sub(index_offset(i,0x40),0x40);
            y=(f&2u)?course_quad_plane_y(record,x,z):course_triangle_plane_y(record,x,z);
        }else if(mode==0x400u){
            std::int32_t id=0;CourseQuad forward{},back{},left{},right{};CourseSplineNeighbors neighbors;
            // The legacy r014 field names label ABI slots, not query directions.
            // GetRoadCond passes Back, Forward, Left, Right to CalcYPosSpl.
            // Keep r014 arithmetic/fixtures unchanged; adapt only at this boundary.
            if(coli_get_forward_polygon_number(t,i,id)){forward=polygon(t,id);neighbors.back=&forward;}
            if(coli_get_back_polygon_number(t,i,id)){back=polygon(t,id);neighbors.forward=&back;}
            if(coli_get_left_polygon_number(t,i,id)){left=polygon(t,id);neighbors.left=&left;}
            if(coli_get_right_polygon_number(t,i,id)){right=polygon(t,id);neighbors.right=&right;}
            const auto normal=course_quad_vertices(t.normals.sub(index_offset(i,0x30),0x30));
            y=calc_y_pos_spl(x,z,p,normal,neighbors,tuning);
        }
        if(f&4u)return {y,i,f};
        const float new_distance=std::fabs(static_cast<float>(y-reference_y));
        const float old_distance=std::fabs(static_cast<float>(best.y-reference_y));
        if(old_distance>new_distance)best={y,i,f};
    }
    return best;
}
}
