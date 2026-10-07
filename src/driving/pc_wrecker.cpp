#include "driving/pc_wrecker.hpp"
#include "driving/pc_x87.hpp"
#include "driving/pc_collision.hpp"
#include "driving/pc_wall_geometry.hpp"
#include <cmath>
#include <stdexcept>
#include <limits>
#include <string>
namespace outrun::driving {
namespace {
CourseProbe read3(Bytes b,std::size_t o){return {b.f32(o),b.f32(o+4),b.f32(o+8)};}
void write3(Bytes b,std::size_t o,const CourseProbe& v){b.putf(o,v.x);b.putf(o+4,v.y);b.putf(o+8,v.z);}
void add_spilled(CourseProbe& a,const CourseProbe& b){
    a.x=static_cast<float>(X87(a.x)+b.x);
    a.y=static_cast<float>(X87(a.y)+b.y);
    a.z=static_cast<float>(X87(a.z)+b.z);
}
CourseProbe poly_v(Bytes p,unsigned v){const auto o=std::size_t(v)*12u;return read3(p,o);}
CourseProbe sub_spilled(const CourseProbe& a,const CourseProbe& b){
    return {static_cast<float>(X87(a.x)-b.x),
            static_cast<float>(X87(a.y)-b.y),
            static_cast<float>(X87(a.z)-b.z)};
}
CourseProbe cross_spilled(const CourseProbe& a,const CourseProbe& b){
    return {static_cast<float>(X87(a.y)*b.z-X87(a.z)*b.y),
            static_cast<float>(X87(a.z)*b.x-X87(a.x)*b.z),
            static_cast<float>(X87(a.x)*b.y-X87(a.y)*b.x)};
}
CourseProbe d3dx_normalized_wrecker(CourseProbe v){
    X87 sum=X87(v.x)*v.x;
    sum+=X87(v.y)*v.y;
    sum+=X87(v.z)*v.z;
    const float squared=static_cast<float>(sum);
    constexpr float eps=1.1920928955078125e-7f;
    if(std::isfinite(squared)&&std::fabs(squared-1.0f)<=eps)return v;
    if(!(squared>std::numeric_limits<float>::min()))return {0.0f,0.0f,0.0f};
    const X87 inv=X87(1.0f)/x87_sqrt(X87(squared));
    return {static_cast<float>(inv*v.x),static_cast<float>(inv*v.y),static_cast<float>(inv*v.z)};
}
CourseProbe projective(const CourseProbe& v,const CourseProbe& n){
    X87 dot=X87(v.y)*n.y;
    dot+=X87(v.x)*n.x;
    dot+=X87(v.z)*n.z;
    return {static_cast<float>(X87(v.x)-dot*n.x),
            static_cast<float>(X87(v.y)-dot*n.y),
            static_cast<float>(X87(v.z)-dot*n.z)};
}
float angle_from_s16(std::int16_t a){
    constexpr float angle_unit=9.58738019107841e-05f;
    return static_cast<float>(X87(a)*angle_unit);
}
float x87_sin_f32(float a){
    return x87_float(x87_sin(X87(a))); // flds; fsin; fstps
}
float x87_cos_f32(float a){
    return x87_float(x87_cos(X87(a))); // flds; fcos; fstps
}
CourseProbe weighted_sum_40f180(const CourseProbe& a,float ka,const CourseProbe& b,float kb){
    // PC 0x40F180 keeps A.x/A.y and B.x products in x87 extended precision,
    // but spills A.z, B.y and B.z before the final additions.
    const X87 ax=X87(ka)*a.x;
    const X87 ay=X87(ka)*a.y;
    const float az=static_cast<float>(X87(ka)*a.z);
    const X87 bx=X87(kb)*b.x;
    const float by=static_cast<float>(X87(kb)*b.y);
    const float bz=static_cast<float>(X87(kb)*b.z);
    return {static_cast<float>(bx+ax),
            static_cast<float>(X87(by)+ay),
            static_cast<float>(X87(bz)+az)};
}
float wrap_pi_sse(float v){
    constexpr float pi=3.1415927410125732f;
    constexpr float neg_pi=-3.1415927410125732f;
    constexpr float two_pi=6.2831854820251465f;
    while(v>pi)v=v-two_pi;
    while(v<neg_pi)v=v+two_pi;
    return v;
}
float angle_lerp_449580(float a,float b,float t){
    a=wrap_pi_sse(a);b=wrap_pi_sse(b);
    float d=b-a;d=wrap_pi_sse(d);
    return static_cast<float>(X87(d)*t+a);
}
CourseProbe angle_vec_lerp_40f480(const CourseProbe& a,const CourseProbe& b,float t){
    return {angle_lerp_449580(a.x,b.x,t),angle_lerp_449580(a.y,b.y,t),angle_lerp_449580(a.z,b.z,t)};
}
float x87_neg_sin_mul(float angle,float scale){
    return x87_float(-(x87_sin(X87(angle))*scale)); // fsin; fmuls; fchs; fstps
}
float x87_neg_cos_mul(float angle,float scale){
    return x87_float(-(x87_cos(X87(angle))*scale)); // fcos; fmuls; fchs; fstps
}
}
bool pc_get_cs_road_info_by_cs_len(Bytes out,Bytes place,std::int32_t hint,PcRoadInfoContext& c){
    out.check(0,0x58);place.check(0,0x0c);
    const auto type=place.u32(0);if(type>=4u)throw std::out_of_range("road-info course type");
    const auto& t=c.tables.courses[type];if(t.load_type!=type)throw std::invalid_argument("road-info course view mismatch");
    // Original clears only the accumulated center before searching.
    out.putf(0x08,0.0f);out.putf(0x0c,0.0f);out.putf(0x10,0.0f);
    std::int32_t first=out.i32(0),last=out.i32(4),count=0;
    if(type==0u)count=find_primary_course_run(t.runs,place.i16(8),hint,first,last);
    else count=find_secondary_course_run(t,place.i16(8),place.i32(4),first,last);
    // The lookup routines publish their index outputs directly in the PC ABI.
    out.puti(0,first);out.puti(4,last);
    if(count<=0||!t.polygons_present)return false;
    if(first<0||last<0)throw std::out_of_range("negative road-info polygon");
    const auto a=t.polygons.sub(std::size_t(first)*0x40u,0x40);
    const auto b=t.polygons.sub(std::size_t(last)*0x40u,0x40);
    const auto transform=c.tables.transforms[type==0u?0u:1u];transform.check(0,64);
    pc_matrix_push_load(c.matrices,transform);
    // The run's envelope uses first.v0, last.v1, last.v2, first.v3.
    const CourseProbe local[4]={poly_v(a,0),poly_v(b,1),poly_v(b,2),poly_v(a,3)};
    constexpr std::size_t dst[4]={0x24,0x30,0x48,0x3c};
    for(unsigned k=0;k<4;++k)write3(out,dst[k],pc_matrix_point(c.matrices,local[k]));
    CourseProbe center{0,0,0};
    for(unsigned k=0;k<4;++k)add_spilled(center,read3(out,dst[k]));
    constexpr float quarter=0.25f;
    center.x=static_cast<float>(X87(center.x)*quarter);
    center.y=static_cast<float>(X87(center.y)*quarter);
    center.z=static_cast<float>(X87(center.z)*quarter);
    write3(out,0x08,center);
    constexpr float angle_unit=9.58738019107841e-05f; // PC 0x628254, 2*pi/65536.
    const auto full_dir=course_collision_offset_direction(t.polygons,static_cast<std::uint32_t>(first),
                                                          c.area_yaw_radians[type==0u?0u:1u],t.polygons_present);
    const auto wrapped=static_cast<std::int16_t>(static_cast<std::uint16_t>(full_dir));
    out.putf(0x14,static_cast<float>(X87(wrapped)*angle_unit));
    {const auto p0=read3(out,0x48),p1=read3(out,0x3c);
        out.putf(0x54,x87_float(course_vec3_distance_x87({p0.x,p0.y,p0.z},{p1.x,p1.y,p1.z})));}
    write3(out,0x18,course_collision_world_normal(t,first,c.matrices,transform));
    pc_matrix_pop(c.matrices);
    return true;
}
void pc_pl_wrecker_sub(Bytes e,Bytes place,Bytes out,PcRoadInfoContext& c){
    e.check(0,0x1c4);place.check(0,16);out.check(0,0x64);
    const auto hint=e.i32(0x1c0);
    auto copy_defined=[&](Bytes dst,Bytes src){for(unsigned k=0;k<0x58u;k+=4)dst.put32(k,src.u32(k));};
    if(e.u32(0x5c)==0u||place.i16(8)>40){
        std::array<std::uint8_t,0x64> tmp{};Bytes t(tmp.data(),tmp.size());
        pc_get_cs_road_info_by_cs_len(t,place,hint,c);copy_defined(out,t);return;
    }
    std::array<std::uint8_t,16> pa{},pb{};
    for(unsigned k=0;k<16;++k)pa[k]=pb[k]=place.u8(k);
    Bytes a_place(pa.data(),pa.size()),b_place(pb.data(),pb.size());
    a_place.puti(4,100);b_place.puti(4,101);
    std::array<std::uint8_t,0x64> ra{},rb{};Bytes a(ra.data(),ra.size()),b(rb.data(),rb.size());
    pc_get_cs_road_info_by_cs_len(a,a_place,hint,c);
    pc_get_cs_road_info_by_cs_len(b,b_place,hint,c);
    const auto pos=read3(e,0x14),ca=read3(a,0x08),cb=read3(b,0x08);
    // PC stores the first mxLength result to f32, while the second remains in
    // x87 extended precision for FCOMIP.  This asymmetry can decide exact-looking ties.
    const float da=x87_float(course_vec3_distance_x87({pos.x,pos.y,pos.z},{ca.x,ca.y,ca.z}));
    const X87 db=course_vec3_distance_x87({pos.x,pos.y,pos.z},{cb.x,cb.y,cb.z});
    const bool choose_a=X87(da)<=db;
    Bytes chosen_place=choose_a?a_place:b_place;Bytes chosen=choose_a?a:b;
    for(unsigned k=0;k<16;++k)place.put8(k,chosen_place.u8(k));
    copy_defined(out,chosen);
}

bool pc_advance_on_road_place(Bytes place,std::int32_t step,PcCourseAdvanceContext& c){
    place.check(0,16);
    const auto initial_type=place.u32(0);
    if(initial_type>=c.course_ends.size())throw std::out_of_range("advance course type");
    const auto end=pc_course_end_position(c.course_ends[initial_type]);

    // ADD word ptr [place+8], DX followed by a signed-negative clamp.
    const auto delta=static_cast<std::uint16_t>(step);
    const auto sum=static_cast<std::uint16_t>(static_cast<std::uint16_t>(place.i16(8))+delta);
    place.put16(8,sum);
    if(static_cast<std::int16_t>(sum)<0)place.put16(8,0);
    if(static_cast<std::int32_t>(place.i16(8))<=static_cast<std::uint16_t>(end))return true;

    bool result=true; // protector preamble pins EBP to one.
    auto wrap_length=[&]{
        const auto adjust=static_cast<std::uint16_t>(0xffffu-static_cast<std::uint16_t>(end));
        place.put16(8,static_cast<std::uint16_t>(static_cast<std::uint16_t>(place.i16(8))+adjust));
    };

    // The PC resolves the current stage property before branching on course type.
    // On secondary -> primary transitions its value is not consumed, but the
    // cache write is still observable before the old-stage lookup below.
    const auto current_property=pc_stage_property(c.stages,c.current_stage_key,c.stage_cache);
    if(initial_type==0u){
        const auto branch=static_cast<std::int8_t>(static_cast<std::uint8_t>(current_property));
        const auto limit=static_cast<std::int8_t>(c.stage_limit);
        place.put32(0,1u);
        auto lane=place.u8(0x0a);
        if(lane>=6u)place.put8(0x0a,static_cast<std::uint8_t>(lane-6u));
        if(branch>limit)result=false;
        wrap_length();
        return result;
    }

    // Secondary -> primary transition.  The PC flips type and updates the
    // remembered stage even when route selection later rejects continuation.
    place.put32(0,0u);
    if(!c.transition_allowed)result=false;
    const auto route_index=pc_stage_property(c.stages,place.u32(0x0c),c.stage_cache);
    const auto choice=pc_get_route_choice(c.route,static_cast<std::int32_t>(route_index));
    place.put32(0x0c,c.current_stage_key);
    auto lane=place.u8(0x0a);
    bool matched=false;
    if(lane<=2u){if(choice==0u){place.put8(0x0a,static_cast<std::uint8_t>(lane+3u));matched=true;}}
    else if(lane<=5u){if(choice==1u){place.put8(0x0a,static_cast<std::uint8_t>(lane-3u));matched=true;}}
    else if(lane<=8u){if(choice==0u){place.put8(0x0a,static_cast<std::uint8_t>(lane-6u));matched=true;}}
    else {if(choice==1u){place.put8(0x0a,static_cast<std::uint8_t>(lane-6u));matched=true;}}
    if(!matched)result=false;
    wrap_length();
    return result;
}

void pc_reconstruct_posture_matrix_and_face_work(Bytes e,Bytes w,Bytes params,Bytes wheels,
                                                  CourseWorldQuery& q){
    const char* stage="input validation";
    try{
        e.check(0,0x2b8);w.check(0,0x64c);params.check(0,0xb98);wheels.check(0,4u*0xf4u);
        q.matrices.current();
        const auto next=static_cast<std::uint32_t>(q.matrices.depth)+1u;
        std::int32_t next_depth;std::memcpy(&next_depth,&next,4);
        if(next_depth<q.matrices.capacity){
            if(q.matrices.current_offset>std::numeric_limits<std::ptrdiff_t>::max()-64)
                throw std::out_of_range("matrix push overflow");
            const auto off=q.matrices.current_offset+64;
            if(off<0)throw std::out_of_range("matrix push before storage");
            q.matrices.storage.check(static_cast<std::size_t>(off),64);
        }

        stage="initial posture matrix";
        pc_matrix_push(q.matrices);
        pc_matrix_unit_rotation(q.matrices);
        pc_matrix_rotate_y(q.matrices,angle_from_s16(e.i16(0x2e)));
        pc_matrix_rotate_x(q.matrices,angle_from_s16(e.i16(0x2c)));
        pc_matrix_rotate_z(q.matrices,angle_from_s16(e.i16(0x30)));
        pc_matrix_store_rotation(q.matrices,e.sub(0x70,44));
        pc_matrix_store_rotation(q.matrices,w.sub(0x10,44));

        stage="GetYPositionProg";
        CourseProbe point=read3(e,0x14);std::uint32_t polygon=0;
        const auto type=get_y_position_prog(q,0x400u,point,&polygon,nullptr,nullptr);
        write3(e,0x14,point);
        write3(w,0x640,point);write3(w,0x40,point);
        w.putf(0x44,static_cast<float>(w.f32(0x44)+params.f32(0x260)));

        stage="CopColiPoint";
        if(type>=q.tables.courses.size())throw std::runtime_error("course type="+std::to_string(type));
        const auto polygon_bytes=q.tables.courses[type].polygons.size();
        if(polygon==0xffffffffu || std::uint64_t(polygon)*0x40u+48u>polygon_bytes)
            throw std::runtime_error("polygon="+std::to_string(polygon)+" type="+std::to_string(type)+" polygon_bytes="+std::to_string(polygon_bytes));
        std::array<std::array<std::uint8_t,12>,4> storage{};
        std::array<Bytes,4> out{Bytes(storage[0].data(),12),Bytes(storage[1].data(),12),
                                Bytes(storage[2].data(),12),Bytes(storage[3].data(),12)};
        cop_coli_point(q.tables,polygon,type,q.matrices,out);
        const auto p0=read3(out[0],0),p1=read3(out[1],0),p2=read3(out[2],0);

        stage="face normal";
        auto edge_a=sub_spilled(p1,p2);
        auto edge_b=sub_spilled(p0,p2);
        auto normal=d3dx_normalized_wrecker(cross_spilled(edge_a,edge_b));
        write3(w,0x628,normal);
        constexpr float face_offset=0.009999999776482582f;
        const auto base=read3(w,0x640);
        CourseProbe face{
            static_cast<float>(normal.x*face_offset+base.x),
            static_cast<float>(normal.y*face_offset+base.y),
            static_cast<float>(normal.z*face_offset+base.z)};
        write3(w,0x634,face);

        stage="heading basis";
        const float heading=angle_from_s16(e.i16(0x160));
        CourseProbe forward{x87_sin_f32(heading),0.0f,x87_cos_f32(heading)};
        forward=d3dx_normalized_wrecker(projective(forward,normal));
        const auto side=cross_spilled(normal,forward);
        write3(w,0x10,side);write3(w,0x20,normal);write3(w,0x30,forward);
        constexpr float velocity_scale=-1.2999999523162842f;
        w.putf(0x5c,forward.x*velocity_scale);
        w.putf(0x60,forward.y*velocity_scale);
        w.putf(0x64,forward.z*velocity_scale);

        stage="suspension reinit";
        const float body=w.f32(0x224);
        const float front=params.f32(0xb48)-body;
        const float rear=params.f32(0xb94)-body;
        const float neg_body=0.0f-body;
        for(unsigned i=0;i<4;++i){
            auto wheel=wheels.sub(std::size_t(i)*0xf4u,0xf4u);
            const float target=i<2?front:rear;
            const float displacement=wheel.f32(0x08)-target;
            wheel.putf(0x28,target);wheel.putf(0x2c,neg_body);wheel.putf(0x18,displacement);
        }
        stage="matrix pop";
        pc_matrix_pop(q.matrices);
    }catch(const std::exception& ex){
        throw std::runtime_error(std::string("ReconstructPostureMatrixAndFaceWork/")+stage+": "+ex.what());
    }
}

void pc_calc_disp_matrix(Bytes e,PcDispMatrixContext& c){
    e.check(0,0x104c);
    auto& s=c.matrices;s.current();
    const float t=c.blend;
    const float inv=static_cast<float>(X87(1.0f)-t);
    const CourseProbe position=weighted_sum_40f180(read3(e,0x14),t,read3(e,0x16c),inv);

    pc_matrix_push_unit(s);
    pc_matrix_translate_vector(s,position);

    auto angle_i=[](std::int32_t v){
        constexpr float unit=9.58738019107841e-05f;
        return static_cast<float>(X87(v)*unit);
    };
    CourseProbe a{
        angle_i(e.i16(0x2c)),
        angle_i(static_cast<std::int32_t>(e.i16(0xc2c))+e.i16(0x2e)),
        angle_i(e.i16(0x30))};
    CourseProbe b{
        angle_i(e.i16(0x17c)),
        angle_i(static_cast<std::int32_t>(e.i16(0xc2e))+e.i16(0x17e)),
        angle_i(e.i16(0x180))};
    auto angles=angle_vec_lerp_40f480(b,a,t);
    pc_matrix_rotate_y(s,angles.y);
    pc_matrix_rotate_x(s,angles.x);
    pc_matrix_rotate_z(s,angles.z);

    constexpr float epsilon=1.1920928955078125e-7f;
    if(e.f32(0xd24)>epsilon){
        CourseProbe offset{0.0f,0.0f,e.f32(0xd24)};
        offset=pc_matrix_point(s,offset);
        pc_matrix_set_translation(s,offset);
        write3(e,0xd28,offset);
    }else{
        write3(e,0xd28,position);
    }

    const auto flags=e.u32(4);
    auto apply_extra=[&](bool scene_dependent){
        const auto delta=weighted_sum_40f180(read3(e,0x2d8),t,read3(e,0x1040),inv);
        pc_matrix_translate_vector(s,delta);
        const auto extra=angle_vec_lerp_40f480(read3(e,0x1034),read3(e,0x2e4),t);
        if(scene_dependent && c.scene_code!=6u && c.scene_code!=15u){
            pc_matrix_rotate_y(s,extra.y);
            pc_matrix_rotate_z(s,extra.z);
            pc_matrix_rotate_x(s,extra.x);
        }else{
            pc_matrix_rotate_z(s,extra.z);
            pc_matrix_rotate_y(s,extra.y);
            pc_matrix_rotate_x(s,extra.x);
        }
    };
    if(flags&0x00800000u)apply_extra(true);
    else if(flags&0x80000000u)apply_extra(false);

    pc_matrix_get(s,e.sub(0xb0,64));
    pc_matrix_pop(s);
}

void pc_pl_wrecker_immediate(Bytes e,Bytes w,Bytes params,Bytes wheels,std::int32_t phase,
                                     PcRoadInfoContext& road,CourseWorldQuery& q,PcDispMatrixContext& disp){
    if(phase>1)throw std::invalid_argument("PlWrecker delayed branch requested through immediate entry");
    e.check(0,0x104c);w.check(0,0x64c);params.check(0,0x15fc);wheels.check(0,4u*0xf4u);
    if(&road.matrices!=&q.matrices||&disp.matrices!=&q.matrices)
        throw std::invalid_argument("PlWrecker contexts must share one matrix stack");

    const float old_speed=e.f32(0x1c4);
    const auto state=(e.u32(0x2f0)>>2u)&0x1fu;
    if(state!=2u&&state!=5u){
        e.putf(0x1c4,0.0f);
    }else{
        float reduced=e.f32(0x1c4)*0.6700000166893005f;
        constexpr float cap=0.7382798194885254f;
        if(reduced>cap)reduced=cap;
        e.putf(0x1c4,reduced);
    }
    float speed=old_speed-e.f32(0x1c4);
    speed=speed*e.f32(0xdb4);
    speed=speed+e.f32(0x1c4);
    e.putf(0x1c4,speed);e.putf(0x178,speed);

    std::array<std::uint8_t,16> place_storage{};
    for(unsigned k=0;k<16;++k)place_storage[k]=e.u8(0x5c+k);
    std::array<std::uint8_t,0x64> road_storage{};
    Bytes place(place_storage.data(),place_storage.size()),ri(road_storage.data(),road_storage.size());
    pc_pl_wrecker_sub(e,place,ri,road);

    constexpr float angle_units=10430.3779296875f;
    const float heading_scaled=ri.f32(0x14)*angle_units;
    std::int32_t heading32;
    if(!(heading_scaled>=-2147483648.0f&&heading_scaled<2147483648.0f))heading32=std::numeric_limits<std::int32_t>::min();
    else heading32=static_cast<std::int32_t>(heading_scaled);
    const auto heading=static_cast<std::uint16_t>(heading32);

    const auto center=read3(ri,0x08);write3(e,0x14,center);write3(e,0x16c,center);
    e.put32(0x08,e.u32(0x08)&0xfffffebfu);
    const auto neg16=[&](std::size_t off){return static_cast<std::uint16_t>(0u-static_cast<std::uint16_t>(e.i16(off)));};
    const auto pitch=neg16(0x2c),roll=neg16(0x30);
    e.put16(0x2c,pitch);e.put16(0x17c,pitch);e.put16(0x30,roll);e.put16(0x180,roll);
    e.put16(0x160,heading);e.put16(0x17e,heading);e.put16(0x2e,heading);

    const float heading_angle=angle_from_s16(static_cast<std::int16_t>(heading));
    e.putf(0x20,x87_neg_sin_mul(heading_angle,speed));
    e.putf(0x24,0.0f);
    e.putf(0x28,x87_neg_cos_mul(heading_angle,speed));
    e.put16(0x4c,0);e.put16(0x4e,0);e.put16(0xd44,0);e.put16(0xd46,0);

    pc_calc_disp_matrix(e,disp);
    for(const auto off:{0x50u,0xc0u,0xccu})write3(w,off,{0.0f,0.0f,0.0f});
    pc_reconstruct_posture_matrix_and_face_work(e,w,params,wheels,q);

    constexpr float velocity_scale=60.20000076293945f;
    w.putf(0x5c,e.f32(0x20)*velocity_scale);
    w.putf(0x60,e.f32(0x24)*velocity_scale);
    w.putf(0x64,e.f32(0x28)*velocity_scale);
    e.put32(0x21c,params.u32(0x15f8));
    const float wheel_velocity=e.f32(0x1c4)*velocity_scale;
    for(unsigned k=0;k<4;++k){
        auto wheel=wheels.sub(std::size_t(k)*0xf4u,0xf4u);
        wheel.put16(0xee,0);wheel.putf(0xd4,wheel_velocity);
        const float denom=params.f32(k<2?0xb48:0xb94);
        wheel.putf(0xd8,wheel_velocity/denom);
    }
}

void pc_pl_wrecker_delayed(Bytes e,std::int32_t phase,PcCourseAdvanceContext& advance,PcRoadInfoContext& road){
    // Closed arg3>1 branch of 0x504900. Reject the immediate phase here so
    // each partition keeps its original pre-mutation contract; pc_pl_wrecker
    // dispatches both validated domains through one public native entry.
    if(phase<=1)throw std::invalid_argument("PlWrecker immediate branch not reconstructed");
    e.check(0,0xd8c);
    for(unsigned k=0;k<16;++k)e.put8(0xd7c+k,e.u8(0x5c+k));
    auto place=e.sub(0xd7c,16);
    (void)pc_advance_on_road_place(place,10,advance); // PC ignores the boolean.
    std::array<std::uint8_t,0x64> info{};Bytes r(info.data(),info.size());
    pc_pl_wrecker_sub(e,place,r,road);
    constexpr float angle_units=10430.3779296875f; // PC 0x6282C0.
    const float scaled=r.f32(0x14)*angle_units;
    std::int32_t heading;
    if(!(scaled>=-2147483648.0f&&scaled<2147483648.0f))heading=std::numeric_limits<std::int32_t>::min();
    else heading=static_cast<std::int32_t>(scaled); // CVTTSS2SI.
    e.put16(0xd78,static_cast<std::uint16_t>(heading));
    for(unsigned k=0;k<12;++k)e.put8(0xd6c+k,r.u8(0x08+k));
    e.put32(0x08,(e.u32(0x08)&~0x100u)|0x40u);
    const auto next=static_cast<std::uint16_t>(static_cast<std::uint32_t>(phase)+1u);
    e.put16(0xd6a,next);e.put16(0xd68,next);
}

void pc_pl_wrecker(Bytes e,Bytes w,Bytes params,Bytes wheels,std::int32_t phase,PcPlWreckerContext& ctx){
    if(phase<=1)pc_pl_wrecker_immediate(e,w,params,wheels,phase,ctx.road,ctx.query,ctx.display);
    else pc_pl_wrecker_delayed(e,phase,ctx.advance,ctx.road);
}

}
