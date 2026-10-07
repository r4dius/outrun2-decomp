#include "driving/pc_wall_rebound.hpp"
#include "driving/pc_x87.hpp"
#include <cmath>
#include <limits>
namespace outrun::driving {
namespace {
std::int16_t i16(std::uint16_t v){std::int16_t r;std::memcpy(&r,&v,2);return r;}
std::int32_t i32(std::uint32_t v){std::int32_t r;std::memcpy(&r,&v,4);return r;}
std::int32_t cvtt(float v){
    if(!(v>=-2147483648.0f&&v<2147483648.0f))return std::numeric_limits<std::int32_t>::min();
    return static_cast<std::int32_t>(v);
}
X87 pc_atan2(float y,float x){
    return x87_atan2(X87(y),X87(x)); // flds y; flds x; fpatan (ST0 kept)
}
constexpr float angle_units=0x1.45f306p+13f; // 6282C0 bits4622F983
void preflight_route_set(const PcRouteContext& c,std::int32_t index){
    if(index<0)throw std::out_of_range("negative route index");
    if(index>=14)return;
    c.choices.check(std::size_t(index)*4,4);
    if(c.mode==4){c.choices.check(0,56);c.save.check(std::size_t(c.slot)*0x6c,0x3c);c.save.check(0x364,4);}
}
bool route_auto_enabled(const PcRouteContext& c){return c.enabled[0]||c.enabled[1]||c.enabled[2];}
int route_auto_value(const PcRouteContext& c,std::int32_t index){
    if(index<0)throw std::out_of_range("negative route index");
    if(!route_auto_enabled(c))return -1;
    if(c.choices.u32(std::size_t(index)*4)!=2)return -1;
    const auto a=c.selection.i32(0x2c),b=c.selection.i32(0x30);
    if(a==-1&&b!=-1)return 1;
    if(a!=-1&&b==-1)return 0;
    return -1;
}
void preflight_route_get(const PcRouteContext& c,std::int32_t index){
    const auto value=route_auto_value(c,index);
    if(value>=0)preflight_route_set(c,index);
    if(index<14)c.choices.check(std::size_t(index)*4,4);
}
std::uint32_t stage_property_value(const PcStageViews& s,std::uint32_t key,Bytes cache){
    cache.check(0,8);
    if(cache.u32(0)==key)return cache.u32(4);
    const auto pos=pc_find_stage_record(s,key);
    return pos?s.records.u32(*pos*0x78+8):0u;
}
void preflight_sound(const PcSoundQueue& q){
    if((q.control&3u)!=2||(q.control&16u))return;
    q.state.check(0,8);
    if(q.state.u32(0)==q.state.u32(4))return;
    q.entries.check(0,128);
    if(q.state.u32(4)>=32)throw std::out_of_range("invalid sound queue write cursor");
}
void simple_heading(Bytes e,std::int16_t difference){
    e.put32(0x290,0);
    e.put16(0x286,std::uint16_t(std::uint16_t(e.i16(0x160))-(int(difference)*5)/4));
}
}
std::uint32_t pc_stage_property(const PcStageViews& s,std::uint32_t key,Bytes cache){
    const auto value=stage_property_value(s,key,cache);
    if(cache.u32(0)!=key){cache.put32(0,key);cache.put32(4,value);}
    return value;
}
std::uint32_t pc_pack_route(Bytes choices){
    choices.check(0,56);std::uint32_t result=0;
    for(unsigned n=14;n!=0;--n)result=(result<<2)|(choices.u32((n-1)*4)&3u);
    return result;
}
void pc_unpack_route(Bytes choices,std::uint32_t packed){
    choices.check(0,56);
    for(unsigned n=0;n<14;++n){choices.put32(n*4,packed&3u);packed>>=2;}
}
void pc_save_route_progress(Bytes save,std::uint8_t slot,std::uint32_t packed,std::uint32_t clock){
    auto row=save.sub(std::size_t(slot)*0x6c,0x3c);
    unsigned n=0;auto tail=packed;
    while(n<14&&(tail&3u)!=2){tail>>=2;++n;}
    if(n>row.u32(0x38)){row.put32(0x38,n);row.put32(0x34,packed);row.put32(0x30,clock);}
}
void pc_set_route_choice(PcRouteContext& c,std::int32_t index,std::uint32_t value){
    preflight_route_set(c,index);if(index>=14)return;
    c.choices.put32(std::size_t(index)*4,value);
    if(c.mode==4){
        pc_save_route_progress(c.save,c.slot,pc_pack_route(c.choices),c.clock);
        pc_unpack_route(c.choices,c.save.u32(0x364));
    }
}
std::uint32_t pc_get_route_choice(PcRouteContext& c,std::int32_t index){
    preflight_route_get(c,index);
    const auto value=route_auto_value(c,index);
    if(value>=0)pc_set_route_choice(c,index,std::uint32_t(value));
    return index<14?c.choices.u32(std::size_t(index)*4):2u;
}
void pc_enqueue_sound(PcSoundQueue& q,std::uint32_t command){
    preflight_sound(q);
    if((q.control&3u)!=2||(q.control&16u))return;
    if(q.state.u32(0)==q.state.u32(4))return;
    for(unsigned n=0;n<32;++n)if(q.entries.u32(n*4)==command)return;
    auto pos=q.state.u32(4);q.entries.put32(pos*4,command);
    q.state.put32(4,pos==31?0u:pos+1u);
}
void calc_rebound_heading_float(Bytes e,std::int16_t difference){
    e.check(0,event_size);
    X87 magnitude=X87(e.f32(0xdbc))*e.f32(0x1c4);
    magnitude*=30.0f;
    const float limit=static_cast<float>(magnitude),lateral=e.f32(0x26c);
    e.put32(0x290,2);
    if(limit>x87_abs(X87(lateral))){
        const float a=limit*limit,b=lateral*lateral;
        const float root=x87_float(x87_sqrt(X87(a-b)));
        const float angle=static_cast<float>(pc_atan2(lateral,root)*angle_units);
        e.put16(0x286,std::uint16_t(std::uint16_t(e.i16(0xd4c))+std::uint16_t(cvtt(angle))));
    }else{simple_heading(e,difference);e.put8(0x282,0);}
}
void pc_direction_xz(float ax,float az,float bx,float bz,Bytes out_x,Bytes out_z){
    out_x.check(0,4);out_z.check(0,4);
    float x=ax-bx,z=az-bz;
    X87 sq=X87(x)*x;
    sq+=X87(0.0f);sq+=X87(z)*z;
    const X87 length=x87_sqrt(sq);
    if(length>X87(1.0e-4)){
        const X87 scale=X87(1.0f)/length;
        x=static_cast<float>(scale*x);z=static_cast<float>(scale*z);
    }
    // FUCOMIP/LAHF/TEST/JNP selects +1 only when length is exactly zero;
    // unordered stays on the stored-Z path. Tiny positive lengths stay raw.
    if(length==X87(0.0f))z=1.0f;
    out_x.putf(0,x);out_z.putf(0,z);
}
void cw_rebound_status(Bytes e,Bytes w,CourseProbe normal,WallReboundContext& c){
    e.check(0,event_size);w.check(0,work_size);
    const bool crush=check_crush_entrapment_length(e,std::uint16_t(e.i16(0x64)),c.response);
    const bool assisted=e.u32(0xdf8)!=0;
    const bool query_route=!assisted&&e.u32(0x1f4)>=100&&e.u8(0x284)==0&&e.u32(0x5c)!=0;
    if(query_route){const auto property=stage_property_value(c.response.stages,e.u32(0x68),c.stage_cache);preflight_route_get(c.route,i32(property));}
    if(e.i8(0xd23)<=0)preflight_sound(c.sounds);
    e.put32(8,(e.u32(8)&~64u)|256u);
    set_collision_timer(e,1,c.response.level);
    const float negative_z=0.0f-normal.z;
    const float angle=static_cast<float>(pc_atan2(negative_z,normal.x)*angle_units);
    auto heading=std::uint32_t(cvtt(angle));
    const int delta=i16(std::uint16_t(heading-std::uint16_t(e.i16(0xd4c))));
    if(std::abs(delta)>=0x4000)heading-=0x8000u;
    const auto difference=i16(std::uint16_t(std::uint16_t(e.i16(0x160))-heading));
    if(crush){
        const float t=1.0f-e.f32(0xdb4);
        const float u=t*0.25f;
        const float factor=1.0f-u;
        for(unsigned k=0;k<3;++k)w.putf(0x5c+4*k,static_cast<float>(X87(w.f32(0x5c+4*k))*factor));
        e.putf(0x1c4,e.f32(0x1c4)*factor);
    }
    if(e.u8(0x284)==0){
        const float t=1.0f-e.f32(0xdb4),u=t*24.0f;
        const auto byte=std::uint8_t(cvtt(u));
        if(e.u8(0x282)<byte)e.put8(0x282,byte);
    }
    if(assisted)calc_rebound_heading_float(e,difference);
    else{
        bool regular=e.u32(0x1f4)>=100&&e.u8(0x284)==0;
        if(regular&&e.u32(0x5c)!=0){
            const auto property=pc_stage_property(c.response.stages,e.u32(0x68),c.stage_cache);
            regular=pc_get_route_choice(c.route,i32(property))!=2;
        }
        if(regular)calc_rebound_heading(e,difference);else simple_heading(e,difference);
    }
    e.put32(0x28c,e.u32(0x1c4));
    e.put16(0x288,std::uint16_t(std::uint16_t(e.i16(0x286))-std::uint16_t(e.i16(0x2e))));
    e.put8(0x283,30);e.put8(0x284,36);
    if(e.i8(0xd23)<=0){e.put8(0xd23,60);pc_enqueue_sound(c.sounds,75);}
}
}
