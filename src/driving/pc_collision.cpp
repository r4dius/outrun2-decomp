#include "driving/pc_collision.hpp"
#include "driving/pc_x87.hpp"
#include "driving/pc_matrix_stack.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
namespace outrun::driving {
namespace {
struct Vec3{float x,y,z;};
struct Mat4{std::array<float,16> m{};};
Vec3 rv(Bytes b,std::size_t o){return {b.f32(o),b.f32(o+4),b.f32(o+8)};}
void wv(Bytes b,std::size_t o,const Vec3& v){b.putf(o,v.x);b.putf(o+4,v.y);b.putf(o+8,v.z);}
Mat4 rm(Bytes b,std::size_t o){Mat4 r;for(unsigned i=0;i<16;++i)r.m[i]=b.f32(o+i*4);return r;}
Vec3 calc_point_pc(const Vec3& v,const Mat4& m){
    // PC mxCalcPoint / D3D row-vector affine path. CommonPlCar has loaded work+0x10.
    return {
      static_cast<float>((X87(v.x)*m.m[0]+X87(v.y)*m.m[4])+
                         (X87(v.z)*m.m[8]+m.m[12])),
      static_cast<float>((X87(v.x)*m.m[1]+X87(v.y)*m.m[5])+
                         (X87(v.z)*m.m[9]+m.m[13])),
      static_cast<float>((X87(v.x)*m.m[2]+X87(v.y)*m.m[6])+
                         (X87(v.z)*m.m[10]+m.m[14]))};
}
float inner_pc(const Vec3& a,const Vec3& b){
    // mxInnerProduct order used by the PC helper: z, y, x in x87.
    return static_cast<float>((X87(a.z)*b.z+
                               X87(a.y)*b.y)+
                               X87(a.x)*b.x);
}
std::int32_t cvttss2si_pc(float value){
    // SSE CVTTSS2SI returns INT_MIN for NaN/out-of-range. Keep this explicit so
    // the reconstructed helper has defined behavior on ARM64 as well.
    constexpr float pos_limit=2147483648.0f;
    constexpr float neg_limit=-2147483648.0f;
    if(!std::isfinite(value)||value>=pos_limit||value<neg_limit)return std::numeric_limits<std::int32_t>::min();
    return static_cast<std::int32_t>(value); // C++ truncates finite in-range float toward zero.
}
std::int32_t add_i32_wrap(std::int32_t a,std::int32_t b){
    const std::uint32_t bits=static_cast<std::uint32_t>(a)+static_cast<std::uint32_t>(b);
    std::int32_t out{};std::memcpy(&out,&bits,sizeof(out));return out;
}
float x87_store(float a,float b,char op){
    const X87 aa=X87(a),bb=X87(b);
    switch(op){case '-':return static_cast<float>(aa-bb);case '*':return static_cast<float>(aa*bb);default:return 0.0f;}
}
float x87_mul_sub(float a,float b,float c,float d){
    const X87 left=X87(a)*X87(b);
    const X87 right=X87(c)*X87(d);
    return static_cast<float>(left-right);
}
}

std::uint32_t calc_collision_area(float x,float z){
    constexpr float origin=3072.0f;
    const float shifted_x=static_cast<float>(x+origin);
    const float shifted_z=static_cast<float>(z+origin);
    std::int32_t cell_x=cvttss2si_pc(shifted_x)/24;
    std::int32_t cell_z=cvttss2si_pc(shifted_z)/24;
    cell_x=std::clamp(cell_x,0,255);
    cell_z=std::clamp(cell_z,0,255);
    return (static_cast<std::uint32_t>(cell_z)<<8)|static_cast<std::uint32_t>(cell_x);
}

float course_triangle_plane_y(Bytes polygon,float x,float z){
    const Vec3 p0=rv(polygon,0x00),p1=rv(polygon,0x0c),p2=rv(polygon,0x18);
    // PC 0x43CC90 forms two float-stored edge vectors with x87, crosses
    // them with x87, then performs the plane solve with scalar SSE.
    const Vec3 a{x87_store(p1.x,p0.x,'-'),x87_store(p1.y,p0.y,'-'),x87_store(p1.z,p0.z,'-')};
    const Vec3 b{x87_store(p2.x,p0.x,'-'),x87_store(p2.y,p0.y,'-'),x87_store(p2.z,p0.z,'-')};
    const float nx=x87_mul_sub(a.y,b.z,a.z,b.y);
    const float ny=x87_mul_sub(a.z,b.x,a.x,b.z);
    const float nz=x87_mul_sub(a.x,b.y,a.y,b.x);
    const float dz=static_cast<float>(z-p0.z);
    const float dx=static_cast<float>(x-p0.x);
    const float nz_dz=static_cast<float>(nz*dz);
    const float nx_dx=static_cast<float>(nx*dx);
    const float sum=static_cast<float>(nz_dz+nx_dx);
    const float slope=static_cast<float>(sum/ny);
    return static_cast<float>(p0.y-slope);
}

float course_quad_plane_y(Bytes polygon,float x,float z){
    const Vec3 p0=rv(polygon,0x00),p2=rv(polygon,0x18);
    const float ax=static_cast<float>(p2.x-p0.x);
    const float dz=static_cast<float>(z-p0.z);
    const float dx=static_cast<float>(x-p0.x);
    const float az=static_cast<float>(p2.z-p0.z);
    const float left=static_cast<float>(ax*dz);
    const float right=static_cast<float>(az*dx);
    const float side=static_cast<float>(left-right);
    if(side>=0.0f)return course_triangle_plane_y(polygon,x,z);
    std::array<std::uint8_t,0x24> tri{};Bytes t(tri.data(),tri.size());
    const Vec3 p3=rv(polygon,0x24);
    wv(t,0x00,p0);wv(t,0x0c,p2);wv(t,0x18,p3);
    return course_triangle_plane_y(t,x,z);
}

void update_easy_lct_prediction_table(EasyLctPredictionState& state,std::uint32_t load_coli_type){
    state.cursor=(state.cursor+1u)&0x0fu;
    state.recent[state.cursor]=load_coli_type;
    unsigned nonzero=0;
    // Exact PC loop bound: slot 15 is in the ring but excluded from this count.
    for(unsigned i=0;i<15;++i)if(state.recent[i]!=0u)++nonzero;
    state.easy=nonzero>=8u?1u:0u;
}

std::int32_t course_collision_offset_direction(Bytes table,std::uint32_t index,float area_yaw_radians,bool table_present){
    if(!table_present)return 0;
    constexpr float angle_scale=10430.3779296875f; // 65536/(2*pi), PC 0x006282c0.
    const float scaled=static_cast<float>(area_yaw_radians*angle_scale);
    const std::int32_t base=cvttss2si_pc(scaled);
    const std::int32_t local=static_cast<std::int32_t>(table.i16(static_cast<std::size_t>(index)*0x40u+0x3eu));
    return add_i32_wrap(base,local);
}

std::uint16_t course_length(Bytes table,std::int32_t index,bool table_present){
    if(!table_present||index==-1)return 0;
    const auto off=static_cast<std::size_t>(static_cast<std::uint32_t>(index))*2u;
    return static_cast<std::uint16_t>(table.u8(off)|(std::uint16_t(table.u8(off+1))<<8));
}

std::uint16_t course_collision_ext_flags(Bytes table,std::int32_t index,bool table_present){
    if(!table_present||index==-1)return 0;
    const auto off=static_cast<std::size_t>(static_cast<std::uint32_t>(index))*0x40u+0x3cu;
    return static_cast<std::uint16_t>(table.u8(off)|std::uint16_t(table.u8(off+1))<<8);
}

bool get_y_position_spl_chk(CourseProbe& point,std::uint32_t* collision_index,
                            std::uint32_t* special_index,std::uint32_t* flags,
                            const CourseQuery& query,std::uint32_t* course_or_return){
    if(!query.get_y_position_prog)return false;
    constexpr float epsilon=0.001f; // PC rdata 0x628204 == 0x3a83126f.
    const CourseProbe original=point;
    CourseQueryResult r{};
    CourseProbe probe=original;
    for(unsigned attempt=0;attempt<4;++attempt){
        if(attempt==1){probe=original;probe.x=static_cast<float>(probe.x+epsilon);}
        else if(attempt==2){probe=original;probe.z=static_cast<float>(probe.z+epsilon);}
        else if(attempt==3){probe=original;probe.x=static_cast<float>(probe.x+epsilon);probe.z=static_cast<float>(probe.z+epsilon);}
        r=query.get_y_position_prog(0x400u,probe,query.user);
        probe.y=r.y;
        if((r.flags&1u)==0)break;
    }
    point.y=probe.y;
    if(collision_index)*collision_index=r.collision_index;
    if(flags)*flags=r.flags;
    if(special_index && (r.flags&0x00f00002u)!=0)*special_index=r.collision_index;
    if(course_or_return)*course_or_return=r.course_or_return;
    return true;
}

static void sus_check_impl(Bytes event,Bytes work,Bytes parameters,const std::array<Bytes,4>& wheels,const PcMatrixStack* stack){
    (void)event;
    const Mat4 body=stack?rm(stack->current(),0):rm(work,0x10);
    const Vec3 normal=rv(work,0x628);
    const Vec3 up{body.m[4],body.m[5],body.m[6]};
    const float orientation=inner_pc(normal,up);
    bool any_no_contact=false;
    bool all_no_contact=true;
    for(unsigned i=0;i<4;++i){
        auto q=wheels[i];
        const unsigned axle=i>>1;
        const float axle_param=parameters.f32((axle+0x26u)*0x4cu);
        Vec3 point{q.f32(0x04),static_cast<float>(q.f32(0x28)-axle_param),q.f32(0x0c)};
        if(stack){const auto v=pc_matrix_point(*stack,{point.x,point.y,point.z});point={v.x,v.y,v.z};}
        else point=calc_point_pc(point,body);
        std::uint32_t flags=q.u32(0);
        const float limit=q.f32(0x3c);
        const bool contact=(limit>=point.y) && (orientation>=0.0f);
        if(contact){
            q.putf(0x28,static_cast<float>((limit-point.y)+q.f32(0x28)));
            flags&=~1u;
        }else flags|=1u;
        q.put32(0,flags);
        q.putf(0x2c,static_cast<float>(q.f32(0x28)-axle_param));
        wv(q,0x70,normal);
        any_no_contact |= (flags&1u)!=0;
        all_no_contact &= (flags&1u)!=0;
    }
    const std::uint32_t old=work.u32(0x244);
    std::uint32_t next=old & 0xffffff79u;
    if(!any_no_contact) next|=0x2u;
    if(all_no_contact) next|=0x4u;
    if(all_no_contact && (old&0x4u)==0) next|=0x80u;
    work.put32(0x244,next);
}

static void bump_push_impl(Bytes event,Bytes work,Bytes parameters,const std::array<Bytes,4>& wheels,PcMatrixStack* stack){
    (void)event; // event is retained in the boundary because the original obtains parameters through it.
    const Mat4 body=stack?rm(stack->current(),0):rm(work,0x10);
    const Vec3 normal=rv(work,0x628);
    const Vec3 contact=rv(work,0x640);
    float push=0.0f;
    for(unsigned i=0;i<4;++i){
        auto q=wheels[i]; const unsigned axle=i>>1;
        Vec3 p=rv(q,0x04);
        // Exact PC offsets: two per-axle vertical geometry terms are removed before world transform.
        const float a=parameters.f32((axle+0x26u)*0x4cu);
        const float b=parameters.f32((axle+0x10u)*0x4cu);
        p.y=static_cast<float>(p.y-static_cast<float>(a+b));
        if(stack){const auto v=pc_matrix_point(*stack,{p.x,p.y,p.z});p={v.x,v.y,v.z};}
        else p=calc_point_pc(p,body);
        p.x=static_cast<float>(p.x-contact.x);
        p.y=static_cast<float>(p.y-contact.y);
        p.z=static_cast<float>(p.z-contact.z);
        const float penetration=static_cast<float>(-X87(inner_pc(p,normal)));
        const float nonnegative=penetration>0.0f?penetration:0.0f;
        // PC MAXSS keeps the prior accumulator on equality/NaN in this finite-domain port.
        if(nonnegative>push)push=nonnegative;
    }
    Vec3 pos=rv(work,0x40);
    pos.x=static_cast<float>(pos.x+push*normal.x);
    pos.y=static_cast<float>(pos.y+push*normal.y);
    pos.z=static_cast<float>(pos.z+push*normal.z);
    wv(work,0x40,pos);
    // work+0x40 is ALSO the translation of the work+0x10 matrix. In addition,
    // the original updates the separate current stack slot for the next stage.
    if(stack)pc_matrix_set_translation(*stack,{pos.x,pos.y,pos.z});
}
void car_sus_coli_check(Bytes e,Bytes w,Bytes p,const std::array<Bytes,4>& wheels){sus_check_impl(e,w,p,wheels,nullptr);}
void car_sus_coli_check(Bytes e,Bytes w,Bytes p,const std::array<Bytes,4>& wheels,const PcMatrixStack& s){sus_check_impl(e,w,p,wheels,&s);}
void car_sus_bump_push(Bytes e,Bytes w,Bytes p,const std::array<Bytes,4>& wheels){bump_push_impl(e,w,p,wheels,nullptr);}
void car_sus_bump_push(Bytes e,Bytes w,Bytes p,const std::array<Bytes,4>& wheels,PcMatrixStack& s){bump_push_impl(e,w,p,wheels,&s);}
bool coli_car(Bytes event,Bytes work,const CollisionStages& stages){
    if(!stages.ground_face||!stages.suspension_check||!stages.suspension_bump_push||!stages.body_wall)return false;
    stages.ground_face(event,work,stages.user);
    stages.suspension_check(event,work,stages.user);
    stages.suspension_bump_push(event,work,stages.user);
    stages.body_wall(event,work,stages.user);
    return true;
}
}
