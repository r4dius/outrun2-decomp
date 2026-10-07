#include "driving/pc_car_services.hpp"
#include "driving/pc_x87.hpp"
#include "driving/pc_common_control.hpp"
#include "driving/pc_d3dx.hpp"
#include <cmath>
#include <limits>
#include <stdexcept>
namespace outrun::driving {
namespace {
inline float cs_mulf(float a,float b){volatile float x=a*b;return x;}
inline float cs_subf(float a,float b){volatile float x=a-b;return x;}
constexpr float kRateScale=0.0039215688593685627f;   // [0x6280BC] 0x3B808081
constexpr float kOne=1.0f;                           // [0x62806C]
constexpr float kSampleUnit=0.000244140625f;         // [0x62818C]
constexpr float kSampleWorld=1000.0f;                // [0x5A29EC]
constexpr float kOffRoad=-3.691162109375f;           // [0x5B437C]
constexpr float kOnRoad=1.384033203125f;             // [0x5B4378]
constexpr float kSurfaceEps=1.1920928955078125e-07f; // [0x6281F0]
constexpr float kNoWidth=std::numeric_limits<float>::max(); // [0x59943C]

struct Decoded {float x,y,z,surface;};
Decoded decode_sample(Bytes region,std::size_t offset){
    region.check(offset,6);
    auto scale=[](std::int32_t v){return cs_mulf(cs_mulf(static_cast<float>(v),kSampleUnit),kSampleWorld);};
    const std::uint16_t raw=static_cast<std::uint16_t>(region.u8(offset+4)|(std::uint16_t(region.u8(offset+5))<<8u));
    std::int32_t y=std::int32_t(raw&0x7fffu);if(y&0x4000)y|=~0x7fff;
    return {scale(region.i16(offset)),scale(y),scale(region.i16(offset+2)),
            (region.u8(offset+5)&0x80u)?kOffRoad:kOnRoad};
}
std::size_t checked_offset(std::int64_t v){
    if(v<0)throw std::out_of_range("road table offset before region");
    return static_cast<std::size_t>(v);
}
// Region and byte offset of block (sel + row*n) as 0x479D90/0x4700D0 compute it.
std::pair<const Bytes*,std::size_t> block_of(const PcRoadTableArena& t,std::int32_t type,
                                             std::int32_t row,std::int32_t sel){
    if(type==0)return {&t.primary,checked_offset((std::int64_t(sel)+std::int64_t(row)*6)*PcRoadPrimaryBlock)};
    return {&t.secondary,checked_offset((std::int64_t(sel)+std::int64_t(row)*12)*PcRoadSecondaryBlock)};
}
// 40F0E0: fld z; fld y; fld x; x*x + y*y, + z*z, fsqrt (ST0 returned).
long double length_40f0e0(CourseProbe v){
    const X87 x=v.x,y=v.y,z=v.z;
    return x87_sqrt((x*x+y*y)+z*z).v;
}
const char* const kGroup[4]{"_cvt","_old","_cvr","_olr"};            // 0x636934
const char* const kFamily[2]{"_stg","_bra"};                         // 0x636944
const char* const kCourse[15]{"_1a","_2a","_2b","_3a","_3b","_3c","_4a","_4b","_4c","_4d",
                              "_5a","_5b","_5c","_5d","_5e"};       // 0x63694C
const char* const kRow[3]{"_othcar","_rvlcar","_01"};                 // 0x636988 (+2 aliases 0x636990)
const char* const kSlot[12]{"_01","_02","_03","_04","_05","_06","_07","_08","_09","_10","_11","_12"}; // 0x636990
constexpr std::uint32_t kKindGroup[6]{1u,0u,1u,0u,3u,2u};            // 0x5A3C88[0x3C..0x41]
}

std::array<float,4> pc_lane_rates_43d130(std::int32_t selector,const Bytes* table){
    std::array<float,4> out{kOne,kOne,kOne,kOne};
    if(selector==-1||!table)return out;
    const auto base=checked_offset(std::int64_t(selector)*4);
    for(unsigned k=0;k<4;++k)out[k]=cs_mulf(static_cast<float>(table->u8(base+k)),kRateScale);
    return out;
}

bool pc_race_ready_4963e0(std::uint32_t route_state,bool present,std::uint32_t field_10){
    return route_state==4u&&present&&field_10!=0u;
}

bool pc_versus_flag_4f53b0(std::uint8_t b){return b!=0u;}

std::uint16_t pc_reference_course_position_456bb0(Bytes manager,const Bytes* records,std::size_t record_count,
                                                  std::uint32_t clock,std::uint8_t versus){
    const std::uint32_t count=manager.u32(0);
    std::uint32_t best=0;
    for(std::uint32_t i=0;i<count;++i){
        const std::size_t s=4u+std::size_t(i)*0x6cu;
        std::int32_t age=static_cast<std::int32_t>(clock-manager.u32(s+0x54));
        if(!pc_versus_flag_4f53b0(versus))age=0;
        if(i>=record_count)throw std::out_of_range("456BB0 slot record view missing");
        const Bytes& rec=records[i];
        if(std::uint32_t(static_cast<std::uint16_t>(rec.i16(0)))!=manager.u32(s+4))continue;
        const std::uint32_t kind=rec.u32(8);
        if(kind!=3u&&kind!=4u)continue;
        if(age>=0xff)continue;
        const std::uint16_t pos=static_cast<std::uint16_t>(manager.i16(s+0x5a));
        if(pos>static_cast<std::uint16_t>(best))best=pos;
    }
    return static_cast<std::uint16_t>(best);
}

std::uint32_t pc_protected_gate_450130(std::uint32_t flags){return (flags>>1)&1u;}

std::int32_t pc_owner_mode_4b5fd0(std::uint8_t flags,std::int32_t owner_mode){
    return (flags&3u)==2u?owner_mode:0;
}

std::uint32_t pc_road_stage_gate_44f0f0(std::int32_t stage_unique,std::uint16_t pos,std::uint16_t rolling){
    return road_stage_gate_44f0f0(stage_unique,pos,rolling,PcRoadStageGateValue44f0f0);
}

std::int32_t pc_road_cache_selector_4a3f8a(Bytes event){return event.i32(0x10d0);}
std::int32_t pc_ofs_left_lane_selector_4a45f9(Bytes event){return event.i32(0x230);}

std::uint32_t pc_comm_race_get_rank_45a2b0(std::uint32_t id,Bytes table,std::uint32_t mode,std::uint32_t variant){
    if(mode==0x10u){
        if(variant==3u||variant==4u)throw std::logic_error("45A2B0 network rank path (456870/459D10/459E10) not ported");
        return variant&0xffffff00u;
    }
    return (id&0xffffff00u)|table.u8(id);
}

std::uint8_t pc_route_flags_46f7a0(Bytes event,const PcRoadTableArena& t){
    if(event.u32(0x5c)!=0u)return 0u;
    const std::size_t rec=4u+std::size_t(static_cast<std::uint16_t>(event.i16(0x64)))*6u;
    auto same=[&](std::size_t a,std::size_t b){
        const std::size_t oa=a*PcRoadPrimaryBlock+rec,ob=b*PcRoadPrimaryBlock+rec;
        t.primary.check(oa,6);t.primary.check(ob,6);
        if(t.primary.i16(oa)!=t.primary.i16(ob))return false;                        // x
        if(((t.primary.u8(oa+4)^t.primary.u8(ob+4))|((std::uint32_t(t.primary.u8(oa+5)^t.primary.u8(ob+5))&0x7fu)<<8))!=0u)return false; // y&0x7FFF
        return t.primary.i16(oa+2)==t.primary.i16(ob+2);                             // z
    };
    std::uint8_t flags=0;
    for(int k=0;k<2;++k)for(int m=0;m<2-k;++m){
        const int bit_a=k+4+m,bit_b=1-k-m;
        if(!(flags&(1u<<bit_a))&&same(std::size_t(2-k),std::size_t(1-k-m)))flags|=std::uint8_t(1u<<bit_a);
        if(!(flags&(1u<<bit_b))&&same(std::size_t(3+k),std::size_t(4+k+m)))flags|=std::uint8_t(1u<<bit_b);
    }
    return flags;
}

PcRouteWidthResult pc_route_width_479d90(Bytes place,std::int32_t row,std::int8_t selector,
                                         const PcRoadTableArena& t,Bytes m0,Bytes m1,PcMatrixStack& matrices){
    const std::int32_t type=place.i32(0);
    const std::int32_t index=place.i16(8);
    auto first_block=block_of(t,type,row,place.i8(0x0a));
    const auto first=decode_sample(*first_block.first,checked_offset(std::int64_t(first_block.second)+4+std::int64_t(index)*6));
    if(!(kSurfaceEps<first.surface))return {kNoWidth,false};  // COMISS/JB
    auto second_block=block_of(t,type,row,selector);
    const Bytes& region=*second_block.first;region.check(second_block.second,2);
    if(static_cast<std::uint16_t>(region.u8(second_block.second)|(std::uint16_t(region.u8(second_block.second+1))<<8u))!=PcRoadTableMagic)
        return {kNoWidth,false};
    const std::int32_t index2=static_cast<std::int16_t>(place.u32(8)&0xffffu);
    const auto second=decode_sample(region,checked_offset(std::int64_t(second_block.second)+4+std::int64_t(index2)*6));
    if(kSurfaceEps>=second.surface)return {kNoWidth,false};
    pc_matrix_push_load(matrices,type==0?m0:m1);
    // 0x40A7D0 = D3DXVec3TransformCoord(v,v,current) (divides by w, also w==0).
    auto point=[&](const Decoded& v){
        const Bytes m=matrices.current();PcMatrix16 mm{};for(unsigned k=0;k<16;++k)mm[k]=m.f32(k*4);
        const auto r=pc_d3dx_vec3_transform_coord({v.x,v.y,v.z},mm);return CourseProbe{r[0],r[1],r[2]};};
    CourseProbe a=point(first);
    // PC re-reads place+0 and reloads only if it changed; it cannot change here.
    CourseProbe b=point(second);
    pc_matrix_pop(matrices);
    const CourseProbe d{cs_subf(a.x,b.x),cs_subf(a.y,b.y),cs_subf(a.z,b.z)};
    return {length_40f0e0(d),true};
}

std::uint32_t pc_async_slot_finished_44f880(bool present,std::uint32_t word0,std::int32_t state){
    return (present&&word0!=0u&&state>=0&&state<=2)?0u:1u;
}

std::string pc_road_table_path_44c680(std::uint32_t which,std::uint32_t kind,std::uint32_t row,
                                      std::uint32_t slot,std::uint32_t selected){
    std::uint32_t group{},course{};
    if(kind>=0x3cu){
        if(kind>0x41u)throw std::out_of_range("44C680 kind outside 0x5A3C88 table");
        group=kKindGroup[kind-0x3cu];course=0;
    }else if(kind>=0x2du){group=2;course=kind-0x2du;}
    else if(kind>=0x1eu){group=3;course=kind-0x1eu;}
    else if(kind>=0x0fu){group=0;course=kind-0x0fu;}
    else {group=1;course=kind;}
    if(which>1u||slot>=12u)throw std::out_of_range("44C680 name index");
    std::uint32_t r=row;
    if(which==0u&&row==1u&&selected==1u)r=2u;
    if(r>2u)throw std::out_of_range("44C680 row index");
    std::string s="\\OCP\\ocp";
    s+=kGroup[group];s+=kFamily[which];s+=kCourse[course];s+=kRow[r];s+=kSlot[slot];s+="_tgt.sz";
    return s;
}

void pc_road_table_reset_46fac0(PcRoadTableLoaderState& st,const PcRoadTableArena& t,std::uint32_t which){
    if(which>1u)throw std::out_of_range("46FAC0 family");
    for(unsigned k=0;k<12;++k){st.status[which*12u+k]=0;st.handle[which*12u+k]=0;}
    if(which==0u){
        for(unsigned k=0;k<6;++k)t.primary.put16(k*PcRoadPrimaryBlock,0);
        for(unsigned k=0;k<2;++k)t.preload.put16(k*PcRoadPrimaryBlock,0);
    }else{
        for(unsigned k=0;k<12;++k)t.secondary.put16(k*PcRoadSecondaryBlock,0);
    }
    st.stage[which]=0;st.counter[which]=0;
}

std::uint32_t pc_road_table_slot_46fc40(PcRoadTableLoaderState& st,const PcRoadTableArena& t,
                                        std::int32_t counter,std::int32_t row,std::uint32_t which,
                                        std::uint32_t kind,std::uint32_t selected,
                                        const PcRoadTableFileService& io){
    const std::int64_t slot=std::int64_t(counter)+(std::int64_t(row)+which)*12;
    if(slot<0||slot>=24)throw std::out_of_range("46FC40 slot");
    const auto status=st.status[std::size_t(slot)];
    if(status==1u){
        if(io.finished(io.user,st.handle[std::size_t(slot)])==0u)return 1u;
        io.release(io.user,st.handle[std::size_t(slot)]);
        st.handle[std::size_t(slot)]=0;st.status[std::size_t(slot)]=2u;return 0u;
    }
    if(status!=0u)return 0u;
    std::int32_t row_add=0,slot_sub=0;
    if(which==0u){
        if(counter<6)t.primary.put16(checked_offset((std::int64_t(counter)+std::int64_t(row)*6)*PcRoadPrimaryBlock),0);
        else if(counter<8){t.preload.put16(checked_offset(std::int64_t(counter-6)*PcRoadPrimaryBlock),0);row_add=1;slot_sub=6;}
        if(counter>=8){
            const std::int64_t done=std::int64_t(counter)+std::int64_t(row)*12;  // PC omits which here (which==0)
            if(done<0||done>=24)throw std::out_of_range("46FC40 done slot");
            st.status[std::size_t(done)]=2u;return 0u;
        }
    }else{
        t.secondary.put16(checked_offset((std::int64_t(counter)+std::int64_t(row)*12)*PcRoadSecondaryBlock),0);
    }
    st.path=pc_road_table_path_44c680(which,kind,std::uint32_t(row+row_add),std::uint32_t(counter-slot_sub),selected);
    PcRoadTableRequest rq{};rq.path=st.path;rq.flags=4u;
    if(which==0u){
        rq.size=PcRoadPrimaryBlock;
        if(st.counter[0]>=6){rq.region=PcRoadTableRegion::Preload;rq.offset=std::uint32_t(checked_offset(std::int64_t(counter-6)*PcRoadPrimaryBlock));}
        else{rq.region=PcRoadTableRegion::Primary;rq.offset=std::uint32_t(checked_offset((std::int64_t(counter)+std::int64_t(row)*6)*PcRoadPrimaryBlock));}
    }else{
        rq.size=PcRoadSecondaryBlock;rq.region=PcRoadTableRegion::Secondary;
        rq.offset=std::uint32_t(checked_offset((std::int64_t(counter)+std::int64_t(row)*12)*PcRoadSecondaryBlock));
    }
    st.handle[std::size_t(slot)]=io.submit(io.user,rq);
    st.status[std::size_t(slot)]=1u;
    return 1u;
}

std::uint32_t pc_road_table_step_46fde0(PcRoadTableLoaderState& st,const PcRoadTableArena& t,
                                        std::uint32_t which,std::uint32_t kind,std::uint32_t selected,
                                        const PcRoadTableFileService& io){
    if(which>1u)throw std::out_of_range("46FDE0 family");
    auto& stage=st.stage[which];auto& counter=st.counter[which];
    if(stage==1)return 0u;
    if(stage>1)return 1u;
    for(;;){
        if(counter<12){
            if(pc_road_table_slot_46fc40(st,t,counter,stage,which,kind,selected,io)==0u)++counter;
            return stage!=1?1u:0u;
        }
        counter=0;++stage;
        if(stage>=1)return stage!=1?1u:0u;
    }
}
}
