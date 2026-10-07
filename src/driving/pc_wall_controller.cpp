#include "driving/pc_wall_controller.hpp"
#include "driving/pc_x87.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <stdexcept>
namespace outrun::driving {
namespace {
CourseProbe read3(Bytes b,std::size_t o=0){return {b.f32(o),b.f32(o+4),b.f32(o+8)};}
void write3(Bytes b,std::size_t o,CourseProbe p){b.putf(o,p.x);b.putf(o+4,p.y);b.putf(o+8,p.z);}
void copy3(Bytes to,Bytes from){for(unsigned k=0;k<12;k+=4)to.put32(k,from.u32(k));}
float sse_mul(float a,float b){volatile float r=a*b;return r;}
float sse_add(float a,float b){volatile float r=a+b;return r;}
float sse_sub(float a,float b){volatile float r=a-b;return r;}
std::int16_t wrapped_delta(Bytes e){
    const std::uint16_t u=std::uint16_t(e.i16(0xd4e))-std::uint16_t(e.i16(0xd4c));
    std::int16_t v;std::memcpy(&v,&u,2);return v;
}
int magnitude16(std::int16_t v){const int n=int(v);return n<0?-n:n;}
CourseProbe cross_spilled(CourseProbe a,CourseProbe b){
    return {
      static_cast<float>(X87(a.y)*b.z-X87(a.z)*b.y),
      static_cast<float>(X87(a.z)*b.x-X87(a.x)*b.z),
      static_cast<float>(X87(a.x)*b.y-X87(a.y)*b.x)};
}
void regular_special_face(const std::array<Bytes,4>& p){
    // 487790 receives p1-p0, but Cbw deliberately swaps its output pointers:
    // out_x -> p0.z, out_z -> p0.x.  This directly constructs the X/Z normal.
    pc_direction_xz(p[1].f32(0),p[1].f32(8),p[0].f32(0),p[0].f32(8),
                    p[0].sub(8,4),p[0].sub(0,4));
    p[0].putf(4,0.0f);
    p[0].putf(8,sse_sub(0.0f,p[0].f32(8)));
    constexpr float epsilon=0x1.47ae14p-7f; // 0x3C23D70A, PC 0x6281C0 = 0.01f
    p[3].putf(0,sse_add(sse_mul(p[0].f32(0),epsilon),p[1].f32(0)));
    p[3].put32(4,p[1].u32(4));
    p[3].putf(8,sse_add(sse_mul(p[0].f32(8),epsilon),p[1].f32(8)));
}
void alternate_special_face(const std::array<Bytes,4>& p){
    // Observed PC quirk for material bits 8|2|1: out_z aliases p1.z while
    // p0.x is intentionally left as the old vertex coordinate.
    pc_direction_xz(p[0].f32(0),p[0].f32(8),p[1].f32(0),p[1].f32(8),
                    p[0].sub(8,4),p[1].sub(8,4));
    std::array<std::uint8_t,12> old2{};Bytes q(old2.data(),old2.size());copy3(q,p[2]);
    p[0].putf(4,0.0f);p[0].putf(8,sse_sub(0.0f,p[0].f32(8)));
    constexpr float epsilon=0x1.47ae14p-7f;
    p[3].putf(0,sse_add(sse_mul(p[0].f32(0),epsilon),q.f32(0)));
    p[3].put32(4,q.u32(4));
    p[3].putf(8,sse_add(sse_mul(p[0].f32(8),epsilon),q.f32(8)));
    copy3(p[1],q);
}
void validate_contacts(Bytes work,Bytes contacts,const PcWallControllerContext& c,std::int32_t count){
    if(count<=0||count>32)throw std::out_of_range("CbwColiWall contact count outside documented 1..32 domain");
    contacts.check(0,std::size_t(count)*16);
    work.check(0,work_size);c.material_modes.check(0,16);
    bool qualifying=false;
    for(std::int32_t i=0;i<count;++i){
        auto r=contacts.sub(std::size_t(i)*16,16);
        if(!(r.u32(8)&0x010f4060u))continue;
        qualifying=true;const auto polygon=r.u32(0),type=r.u32(12);
        if(type>=4u)throw std::out_of_range("CbwColiWall course type outside explicit table");
        c.surface_codes[type].check(polygon,1);
        const auto& course=c.world.courses[type];
        if(!course.polygons_present)throw std::invalid_argument("CbwColiWall needs selected polygon table");
        const std::uint64_t off=std::uint64_t(polygon)*64u;
        if(off>std::numeric_limits<std::size_t>::max())throw std::out_of_range("CbwColiWall polygon offset overflow");
        course.polygons.check(static_cast<std::size_t>(off),48);
    }
    if(!qualifying)throw std::invalid_argument("CbwColiWall original has undefined first-face state with no qualifying wall contact");
}
float rebound_threshold(Bytes e,Bytes w){
    constexpr float deg5=0x1.657186p-4f;  // 0x3DB2B8C3
    constexpr float deg10=0x1.657186p-3f; // 0x3E32B8C3
    constexpr float deg25=0x1.becde6p-2f; // 0x3EDF66F3
    if(e.u32(0x1f4)>=50u && !(w.u32(0x244)&8u) && e.u8(0x282)==0u){
        float threshold=deg5;const auto delta=wrapped_delta(e);
        if(magnitude16(delta)>0x400){
            const float product=sse_mul(static_cast<float>(int(delta)),e.f32(0x26c));
            if(product<0.0f)threshold=deg10;
        }
        return threshold;
    }
    return deg25;
}
float crush_threshold(Bytes e,PcWallControllerContext& c){
    constexpr float deg45=0x1.921fb6p-1f; // 0x3F490FDB
    constexpr float deg25=0x1.becde6p-2f;
    constexpr float deg20=0x1.657186p-2f; // 0x3EB2B8C3
    constexpr float deg18=0x1.41b2f8p-2f; // 0x3EA0D97C
    constexpr float units=0x1.921fb6p-14f; // PC 0x628254
    if(pc_stage_property(c.rebound.response.stages,e.u32(0x68),c.rebound.stage_cache)==0u)return deg45;
    const auto delta=wrapped_delta(e);const auto mag=magnitude16(delta);
    const X87 angle=X87(mag)*units;
    return angle<=X87(deg20)?deg25:deg18;
}
}
void cbw_coli_wall(Bytes e,Bytes w,Bytes contacts,PcMatrixStack& matrix,PcWallControllerContext& c){
    const auto count=w.i32(0x68c);validate_contacts(w,contacts,c,count);e.check(0,event_size);matrix.current();
    std::uint32_t aggregate=0,selected_mask=0;float max_distance=0.0f;bool first=false;
    std::array<std::uint8_t,36> saved_face{};
    auto face=w.sub(0x64c,36);auto shape=w.sub(0x68c,work_size-0x68c);
    for(std::int32_t i=0;i<count;++i){
        auto r=contacts.sub(std::size_t(i)*16,16);if(!(r.u32(8)&0x010f4060u))continue;
        const auto polygon=r.u32(0),type=r.u32(12);
        std::array<std::uint8_t,48> raw{};std::array<Bytes,4> p{
            Bytes(raw.data()+0,12),Bytes(raw.data()+12,12),Bytes(raw.data()+24,12),Bytes(raw.data()+36,12)};
        cop_coli_point(c.world,polygon,type,matrix,p);
        const auto material=std::uint8_t(c.surface_codes[type].u8(polygon)&0x0fu);
        const bool equal=(p[3].f32(0)==p[2].f32(0))&&(p[3].f32(8)==p[2].f32(8));
        bool special=false;
        if(equal){
            const auto mode=c.material_modes.u8(material);
            if((mode&0x0fu)==2u){
                const auto high=mode&0xf0u;
                std::array<std::uint8_t,24> temp{};Bytes a(temp.data(),12),b(temp.data()+12,12);
                if(high==0x40u){copy3(a,p[0]);copy3(b,p[3]);copy3(p[1],a);copy3(p[0],b);}
                else if(high==0x80u){copy3(a,p[3]);copy3(b,p[2]);copy3(p[1],a);copy3(p[0],b);}
                else if(high==0xc0u){copy3(a,p[1]);copy3(b,p[2]);copy3(p[0],a);copy3(p[1],b);}
                regular_special_face(p);special=true;
            }else if((material&0x0au)==0x0au){
                if(material&1u)alternate_special_face(p);else regular_special_face(p);
                special=true;
            }
        }
        if(!special){
            // Cbw pushes p1,p0,p3,p2 as CalcColiWallFace's four pointer
            // arguments; its standalone native API intentionally exposes that
            // exact argument order.
            const std::array<Bytes,4> ordered{p[1],p[0],p[3],p[2]};
            calc_coli_wall_face(w.f32(0x40),w.f32(0x48),ordered);
        }
        // Common Cbw face layout after either path: p0=normal, p3=offset,
        // p1=boundary.  Copy in the original forward DWORD order.
        copy3(face.sub(0,12),p[0]);copy3(face.sub(12,12),p[3]);copy3(face.sub(24,12),p[1]);
        pc_matrix_load(matrix,w.sub(0x10,64));
        if(e.u32(4)&0x80000000u)pc_matrix_rotate_y(matrix,e.f32(0x2e8));
        const auto pushed=push_outpos_mat_obsolete(w,face,shape,matrix);
        if(pushed.distance>max_distance)max_distance=pushed.distance;
        aggregate|=pushed.mask;if(pushed.mask)w.put32(0x244,w.u32(0x244)|0x20u);
        if(!first){for(unsigned k=0;k<36;k+=4){std::uint32_t v=face.u32(k);std::memcpy(saved_face.data()+k,&v,4);}selected_mask=pushed.mask;first=true;}
    }
    w.put32(0x670,aggregate);
    for(unsigned k=0;k<36;k+=4){std::uint32_t v;std::memcpy(&v,saved_face.data()+k,4);face.put32(k,v);}
    std::array<std::uint8_t,12> contact_raw{};Bytes contact(contact_raw.data(),contact_raw.size());
    if(!coli_set_obsolete(w,face,shape,selected_mask,matrix,contact))return;
    const float x=contact.f32(0),z=contact.f32(8);
    e.put8(0x281,x>0.0f?(z>0.0f?2u:3u):(z>0.0f?1u:0u));
    const auto old_normal=read3(face),axis=read3(w,0x628);
    write3(face,0,cross_spilled({old_normal.z,old_normal.y,sse_sub(0.0f,old_normal.x)},axis));
    if(e.u32(0x1f4)==0u || !(e.u32(4)&1u) || (e.u32(4)&2u) || !(e.u32(0x2a8)&0x010f0020u)){
        calc_friction_status(e,w,max_distance,matrix,c.rebound.response);return;
    }
    const auto motion=read3(w,0x5c),normal=read3(face);
    const float impact=x87_float(calc_wall_impact_angle_x87({motion.x,motion.y,motion.z},{normal.x,normal.y,normal.z},matrix));
    const float rebound_limit=rebound_threshold(e,w);
    const float crush_limit=crush_threshold(e,c);
    if(impact>=rebound_limit){
        cw_rebound_status(e,w,read3(face),c.rebound);
        if(impact>=crush_limit)
            (void)pc_cw_crush_status(e,w,impact,contacts,c.crash,c.rebound.response,c.crush,
                                     c.material_sounds,c.rebound.sounds,c.course_ends,c.mode,c.variant);
        return;
    }
    if(impact>=crush_limit){
        const bool accepted=pc_cw_crush_status(e,w,impact,contacts,c.crash,c.rebound.response,c.crush,
                                               c.material_sounds,c.rebound.sounds,c.course_ends,c.mode,c.variant);
        if(accepted){cw_rebound_status(e,w,read3(face),c.rebound);return;}
    }
    calc_friction_status(e,w,max_distance,matrix,c.rebound.response);
}
}
