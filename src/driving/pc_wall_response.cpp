#include "driving/pc_wall_response.hpp"
#include "driving/pc_x87.hpp"
#include <cmath>
#include <limits>
namespace outrun::driving {
namespace {
std::int32_t signed32(std::uint32_t u){std::int32_t v;std::memcpy(&v,&u,4);return v;}
std::int16_t signed16(std::uint16_t u){std::int16_t v;std::memcpy(&v,&u,2);return v;}
std::int32_t truncate_sse(float f){
    // CVTTSS2SI produces integer indefinite on NaN/out-of-range, not C++ UB.
    if(!(f>=-2147483648.0f&&f<2147483648.0f))return std::numeric_limits<std::int32_t>::min();
    return static_cast<std::int32_t>(f);
}
CourseProbe read3(Bytes b,std::size_t o){return {b.f32(o),b.f32(o+4),b.f32(o+8)};}
void write3(Bytes b,std::size_t o,CourseProbe p){b.putf(o,p.x);b.putf(o+4,p.y);b.putf(o+8,p.z);}
X87 length(CourseProbe p){
    X87 sq=X87(p.x)*p.x;
    sq+=X87(p.y)*p.y;
    sq+=X87(p.z)*p.z;
    return x87_sqrt(sq);
}
CourseProbe tangent(CourseProbe p,CourseProbe n){
    X87 dot=X87(p.y)*n.y;
    dot+=X87(p.x)*n.x;
    dot+=X87(n.z)*p.z;
    return {static_cast<float>(X87(p.x)-dot*n.x),
            static_cast<float>(X87(p.y)-dot*n.y),
            static_cast<float>(X87(p.z)-dot*n.z)};
}
bool entrapment(Bytes event,std::uint16_t position,const WallResponseContext& c,Bytes table){
    // The original resolves stage BEFORE testing event+0x5C.
    event.check(0x5c,16);
    const auto stage=pc_stage_number(c.stages,event.u32(0x68));
    if(event.u32(0x5c)!=0)return false;
    if(stage<0)throw std::out_of_range("negative wall-entrapment stage");
    const std::uint64_t offset=std::uint64_t(stage)*64;
    if(offset>std::numeric_limits<std::size_t>::max())throw std::out_of_range("wall-entrapment offset overflow");
    const auto row=table.sub(static_cast<std::size_t>(offset),64);
    for(unsigned k=0;k<16;++k){
        const int lo=row.i16(k*4);if(lo<0)return false;
        if(int(position)>=lo&&int(position)<=row.i16(k*4+2))return true;
    }
    return false;
}
}
std::optional<std::size_t> pc_find_stage_record(const PcStageViews& s,std::uint32_t id){
    if(s.count<=0)return std::nullopt;
    const auto bytes=std::uint64_t(s.count)*0x78u;
    if(bytes>std::numeric_limits<std::size_t>::max())throw std::out_of_range("stage record count overflow");
    s.records.check(0,static_cast<std::size_t>(bytes));
    for(std::size_t k=0;k<static_cast<std::size_t>(s.count);++k)
        if(s.records.u32(k*0x78+4)==id)return k;
    return std::nullopt;
}
std::int32_t pc_stage_number(const PcStageViews& s,std::uint32_t id){
    const auto index=pc_find_stage_record(s,id);
    if(!index)return s.fallback_descriptor.i32(0);
    if(*index>=s.descriptors.size())throw std::out_of_range("missing explicit stage descriptor");
    return s.descriptors[*index].i32(0);
}
bool check_crush_entrapment_length(Bytes e,std::uint16_t p,const WallResponseContext& c){return entrapment(e,p,c,c.crush_ranges);}
bool check_friction_entrapment_length(Bytes e,std::uint16_t p,const WallResponseContext& c){return entrapment(e,p,c,c.friction_ranges);}
void set_collision_timer(Bytes e,std::int32_t mode,std::uint8_t level){
    std::int32_t timer;
    if(mode==2)timer=120;
    else if((mode==0||mode==1)&&level>1)timer=180;
    else return;
    if(e.i32(0xdec)<timer)e.puti(0xdec,timer);
}
void calc_rebound_heading(Bytes e,std::int16_t difference){
    e.check(0,0xd50);
    const int quarter=-int(difference)/4;
    const auto delta=signed16(std::uint16_t(std::uint16_t(e.i16(0xd4e))-std::uint16_t(e.i16(0xd4c))));
    int half=int(delta)/2;
    if((half+quarter)*quarter<0)half=-(quarter/2);
    e.puti(0x290,1);
    e.put16(0x286,std::uint16_t(int(std::uint16_t(e.i16(0x160)))+half+quarter-int(difference)));
}
void calc_friction_status(Bytes e,Bytes w,float angle,PcMatrixStack& s,const WallResponseContext& c){
    // Resolve and validate everything which can reject BEFORE changing state.
    e.check(0,event_size);w.check(0,work_size);s.current();
    const auto position=std::uint16_t(e.i16(0x64));
    const bool crush=check_crush_entrapment_length(e,position,c);
    const bool friction=check_friction_entrapment_length(e,position,c);
    const float rotation=angle*((e.u8(0x281)&1u)?c.rotation_odd:c.rotation_even);
    const auto units=signed16(std::uint16_t(truncate_sse(rotation)));
    constexpr float angle_scale=0x1.921fb6p-14f; // bits 0x38C90FDB
    const float radians=static_cast<float>(X87(units)*angle_scale);
    set_collision_timer(e,0,c.level);
    w.put32(0x244,((w.u32(0x244)|8u)&~16u)|((crush||friction)?16u:0u));
    const auto value=e.u32(0x1f4);
    int a,b;
    if(crush&&friction){a=(value>>7)&255;b=(value>>3)&255;}
    else if(!crush&&!friction){a=(value>>8)&255;b=(value>>7)&255;}
    else {a=(value>>7)&255;b=(value>>6)&255;}
    if(!(e.u32(0x2a8)&0x010f0020u))b=a;
    const auto decay=truncate_sse(static_cast<float>(b-a)*e.f32(0xdb4));
    b=signed32(std::uint32_t(b)-std::uint32_t(decay));
    if(e.u32(0xdf8))b/=3;
    if(int(e.u8(0x282))<b)e.put8(0x282,static_cast<std::uint8_t>(b));
    // NOT Push/Pop: the caller's current matrix is overwritten and retained.
    pc_matrix_identity(s);pc_matrix_rotate_y(s,radians);
    pc_matrix_multiply_current(s,w.sub(0x10,64));
    pc_matrix_store_rotation(s,w.sub(0x10,44));
    const auto original=read3(w,0x5c),normal=read3(w,0x64c);
    const float magnitude=static_cast<float>(length(original));
    auto projected=tangent(original,normal);
    const X87 projected_length=length(projected);
    // 0x40F080 leaves output unchanged at/below the double literal 1e-4.
    if(projected_length>X87(1.0e-4)){
        const X87 scale=X87(magnitude)/projected_length;
        projected={static_cast<float>(scale*projected.x),static_cast<float>(scale*projected.y),static_cast<float>(scale*projected.z)};
    }
    const float cross_y=static_cast<float>(X87(original.z)*projected.x-X87(original.x)*projected.z);
    if(e.u8(0x281)<2?cross_y<0.0f:cross_y>0.0f)write3(w,0x5c,projected);
}
}
