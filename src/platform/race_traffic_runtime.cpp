#include "enhancements/frame_rate.hpp"
#include "platform/bulk_fallback.hpp"
#include "platform/race_traffic_runtime.hpp"
#include "platform/translated_crt.hpp"   // the request manager's heap
#include "system/exe_image.hpp"           // the request manager's .data 64F000
#include "platform/title_owner.hpp"          // 465160 sprite resources (request objects)
#include "platform/frontend_ui_resources.hpp"
#include "platform/native_runtime.hpp"
#include "platform/race_area_runtime.hpp"
#include "platform/race_manager_runtime.hpp"
#include "platform/native_race_effects.hpp"
#include "platform/race_player_car.hpp"
#include "platform/vehicle_constructor.hpp"
#include "platform/vehicle_body_init.hpp"
#include "platform/frontend_profiles.hpp"
#include "driving/pc_transmission.hpp"
#include "driving/pc_car_services.hpp"
#include "driving/pc_camera.hpp"
#include "platform/race_ghosts.hpp"
#include "platform/race_hud_navi.hpp"
#include "platform/race_hud_runtime.hpp"
#include "platform/race_end_runtime.hpp"   // 84BCF8 miles word (4F2B20)
#include "platform/pc_address_view.hpp"
#include "platform/retail_asset_store.hpp"
#include "driving/pc_common_control.hpp"
#include "driving/pc_x87.hpp"
#include "driving/pc_crash.hpp"
#include "driving/pc_wrecker.hpp"
#include "driving/pc_course_query.hpp"
#include "driving/pc_course_world.hpp"
#include "driving/pc_wall_geometry.hpp"
#include "platform/pc_scene_renderer.hpp"
#include "platform/pc_render_flush.hpp"
#include <cmath>
#include <cstdio>
#include <cstring>
#include <sstream>
#include <cstddef>
#include "system/perf.hpp"
#include "platform/race_translated.hpp"
#include "platform/pc_network.hpp"
#include "platform/race_variant_owners.hpp"
#include <cstdlib>
#include <stdexcept>
#include "driving/service_hole.hpp"
namespace outrun::platform {
bool native_mission_type4_4962a0(const NativeRuntimeContext&);
extern const TranslatedFunction network_cars_functions[];
extern const TranslatedCodeData network_cars_code_data[];
namespace {
// Synthetic PC addresses of the racer setup tables ([80FB00] / [80FB1C] / [80FB20] / [80FB30]).
constexpr std::uint32_t RacersBase=0x5d000000u,Table1cBase=0x5d100000u,Table20Base=0x5d200000u,Table30Base=0x5d300000u;
std::string hex(std::uint32_t v){char t[16];std::snprintf(t,sizeof t,"%X",v);return t;}
struct TrafficUnported : std::runtime_error {
    std::uint32_t pc;
    TrafficUnported(std::uint32_t p,const std::string& why):std::runtime_error("PC "+hex(p)+": "+why),pc(p){}
};
void put32(std::uint8_t* p,std::uint32_t v){std::memcpy(p,&v,4);}
std::uint32_t get32(const std::uint8_t* p){std::uint32_t v;std::memcpy(&v,p,4);return v;}
// Event records 799B30 (+0x3C each) and flags 79FB48 as the PC lays them out.
void mirror_events(NativeRuntimeContext& c,NativeRaceTraffic& t){
    OR2_PERF_ZONE("traffic mirror_events");
    for(std::uint32_t id=0;id<410u;++id){
        const auto& s=c.event_state.slots[id];auto* b=t.events_799b30.data()+id*0x3cu;
        // descriptor_token..aux38: fifteen consecutive words in record order (+00..+38)
        static_assert(offsetof(driving::PcEventSlot,aux38)-offsetof(driving::PcEventSlot,descriptor_token)==0x38,"PcEventSlot record words");
        std::memcpy(b,&s.descriptor_token,0x3c);
        if(id<t.flags_79fb48.size())t.flags_79fb48[id]=s.flags;
    }
}
// The racer setup words 80FB00..80FB4F <-> RacerSetupState.
void racers_in(NativeRuntimeContext& c,NativeRaceTraffic& t){
    auto& r=c.mission.racers;auto* b=t.racers_80fb00.data();
    put32(b,r.racers_80fb00.empty()?0u:RacersBase);put32(b+4,r.count_80fb04);b[8]=r.flag_80fb08;b[9]=r.flag_80fb09;
    put32(b+0xc,r.config_80fb0c);put32(b+0x14,c.race.car_world.gate_80fb14);b[0x18]=r.v80fb18;
    put32(b+0x1c,r.table_80fb1c.empty()?0u:Table1cBase);put32(b+0x20,r.table_80fb20.empty()?0u:Table20Base);
    b[0x24]=r.v80fb24;put32(b+0x28,r.special_80fb28);put32(b+0x2c,r.v80fb2c);put32(b+0x30,r.table_80fb30.empty()?0u:Table30Base);
}
void racers_out(NativeRuntimeContext& c,NativeRaceTraffic& t){
    auto& r=c.mission.racers;const auto* b=t.racers_80fb00.data();
    auto same=[&](std::uint32_t off,std::uint32_t expect){if(get32(b+off)!=expect)throw TrafficUnported(0x80fb00u+off,"racer table pointer replaced");};
    same(0,r.racers_80fb00.empty()?0u:RacersBase);same(0x1c,r.table_80fb1c.empty()?0u:Table1cBase);
    same(0x20,r.table_80fb20.empty()?0u:Table20Base);same(0x30,r.table_80fb30.empty()?0u:Table30Base);
    r.count_80fb04=get32(b+4);r.flag_80fb08=b[8];r.flag_80fb09=b[9];r.config_80fb0c=get32(b+0xc);
    c.race.car_world.gate_80fb14=get32(b+0x14);r.v80fb18=b[0x18];r.v80fb24=b[0x24];
    r.special_80fb28=get32(b+0x28);r.v80fb2c=get32(b+0x2c);
    c.start_mode.scene_owner_special_driver_80fb28=std::int32_t(r.special_80fb28);
}
void map_memory(NativeRuntimeContext& c,NativeRaceTraffic& t){
    OR2_PERF_ZONE("traffic map_memory");
    auto& m=t.memory;m.clear();
    native_race_requests_map(c,m);   // first: the area regions and the cells below shadow its words
    // The AREA owner's PC memory as its last callback built it: EXE ranges, the
    // area block 7D2D80.., the race manager block, the player car, the car works.
    for(const auto& r:native_race_area_memory(c,nullptr,false).regions()){
        if(r.writable)m.map(r.base,r.data,r.size);else m.map_const(r.base,r.data,r.size);}
    ghost_map_car_tables(m);                                   // model table 650500 and the car data tables
    if(!t.data_64de00.empty())m.map(0x64de00u,t.data_64de00.data(),t.data_64de00.size());
    if(!t.data_6a5df8.empty())m.map(0x6a5df8u,t.data_6a5df8.data(),t.data_6a5df8.size());
    {auto& r=c.mission.racers;   // racer setup words inside 64DE00..
        m.map(0x64deecu,reinterpret_cast<std::uint8_t*>(&r.names_64deec),4);m.map(0x64def0u,reinterpret_cast<std::uint8_t*>(&r.names_64def0),4);
        m.map(0x64df64u,reinterpret_cast<std::uint8_t*>(&r.profile_64df64),4);}
    t.state.map(m);
    auto& r=c.mission.racers;
    m.map(0x80fb00u,t.racers_80fb00.data(),t.racers_80fb00.size());
    if(!r.racers_80fb00.empty())m.map(RacersBase,r.racers_80fb00.data(),r.racers_80fb00.size());
    if(!r.table_80fb1c.empty())m.map(Table1cBase,r.table_80fb1c.data(),r.table_80fb1c.size());
    if(!r.table_80fb20.empty())m.map(Table20Base,r.table_80fb20.data(),r.table_80fb20.size());
    if(!r.table_80fb30.empty())m.map(Table30Base,r.table_80fb30.data(),r.table_80fb30.size());
    // [80FB0C]: the racer config of the selected Races record (Races.bin relocated at 30000000)
    if(!c.mission.races_relocated.empty())m.map_const(NativeRacesBlobBase,c.mission.races_relocated.data(),c.mission.races_relocated.size());
    native_race_attack_map(c,m);   // variant 9: 686254.. and RaceAttack.bin
    // words of other owners inside the module block 800A00..80FB00
    m.map(0x8037b8u,reinterpret_cast<std::uint8_t*>(&r.v8037b8),4);
    m.map(0x803710u,reinterpret_cast<std::uint8_t*>(&r.length_803710),4);
    m.map(0x804388u,reinterpret_cast<std::uint8_t*>(&r.per_lap_804388),2);
    m.map(0x8037bcu,reinterpret_cast<std::uint8_t*>(&c.race.manager.word_8037bc),2);
    put32(t.cell_680ad4.data(),r.v680ad4);put32(t.cell_680ad8.data(),r.v680ad8);
    if(std::getenv("OR2_680AD4_TRACE")){static std::uint32_t last=~0u;if(last!=r.v680ad4){last=r.v680ad4;std::fprintf(stderr,"[680ad4] traffic %u/%u\n",r.v680ad4,r.v680ad8);}}
    m.map(0x680ad4u,t.cell_680ad4.data(),4);m.map(0x680ad8u,t.cell_680ad8.data(),4);
    put32(t.seeds_6a4e2c.data(),r.seed_6a4e2c);m.map(0x6a4e2cu,t.seeds_6a4e2c.data(),t.seeds_6a4e2c.size());
    put32(t.cell_64e190.data(),std::uint32_t(r.v64e190));put32(t.cell_64e194.data(),std::uint32_t(r.v64e194));
    m.map(0x64e190u,t.cell_64e190.data(),4);m.map(0x64e194u,t.cell_64e194.data(),4);
    mirror_events(c,t);
    m.map(0x799b30u,t.events_799b30.data(),t.events_799b30.size());
    m.map(0x79fb48u,t.flags_79fb48.data(),t.flags_79fb48.size());
    put32(t.cell_799d18.data(),c.event_state.slots[8].work_token);m.map(0x799d18u,t.cell_799d18.data(),4);
    if(const auto car=c.event_state.slots[8].work_token)m.map(car,c.event_function36.car_select.car_799d18.data(),c.event_function36.car_select.car_799d18.size());
    put32(t.cell_79f574.data(),c.event_state.slots[385].work_token);m.map(0x79f574u,t.cell_79f574.data(),4);
    if(const auto cam=c.event_state.slots[385].work_token)m.map(cam,c.event_function36.car_select.camera_79fe10.data(),0x3d0u);
    m.map(0x8367c0u,c.start_mode.scene_owner_models_8367c0.data(),c.start_mode.scene_owner_models_8367c0.size());
    // owners of words inside the module block 84BD00..84CE80: START's 4EFB50/4EFAF0 tables
    // (84BD08 appear table, 84BF60) and the AREA's 4EFD20 lane words 6A5DC4
    m.map(0x84bd08u,c.start_mode.scene_owner_table68.data(),c.start_mode.scene_owner_table68.size());
    m.map(0x84bf60u,c.start_mode.scene_owner_table6c.data(),c.start_mode.scene_owner_table6c.size());
    m.map(0x6a5dc4u,reinterpret_cast<std::uint8_t*>(native_race_area(c).light_words_6a5dc4.data()),24u);
    // 780228[type]: the course world's per-polygon length tables (u16), at synthetic bases;
    // every cell 0 once all four lanes are released (43DB00 clears each lane's root words).
    auto& world=c.start_mode.scene_owner_course_world;
    bool released=true;for(std::uint32_t k=0;k<4;++k)if(world.lane_loaded(k))released=false;
    if(released){t.cells_780228.fill(0);m.map(0x780228u,t.cells_780228.data(),t.cells_780228.size());}
    else{const auto tables=world.tables();
        for(std::uint32_t k=0;k<4;++k){
            const auto& L=tables.courses[k].runs.lengths;std::uint32_t at=0;
            if(tables.courses[k].runs.present&&L.size()){at=0x5d400000u+k*0x100000u;m.map_const(at,L.data(),L.size());}
            put32(t.cells_780228.data()+k*4u,at);
        }
        m.map(0x780228u,t.cells_780228.data(),t.cells_780228.size());}
    // 5E3140: the driving parameter arena (5051D0 stores 5E3140 + 4 * column in car +2B4)
    // (a copy: the native leaves take mutable views; nothing writes it)
    if(c.game_mode.driving_data&&c.game_mode.driving_data->parameter_arena.size()==DrivingParameterArenaBytes){
        if(t.parameters_5e3140.empty())t.parameters_5e3140=c.game_mode.driving_data->parameter_arena;
        m.map(0x5e3140u,t.parameters_5e3140.data(),t.parameters_5e3140.size());}
    // SCN_EFC (event 387): [79F5EC] and its work 780280 (4A25F0 night / tunnel bits), as native_race_effects maps them
    {auto& fx=c.race_effects;fx.scene.work_pointer_79f5ec=c.event_state.slots[387].work_token;fx.scene.map(m);}
    m.map(0x850ba0u,t.wall_850ba0.data(),t.wall_850ba0.size());   // 504E70 wall work (module .bss 850BA0..8514A0)
    // course objects: their works (440A60), the writable .data block 681200.. (ball shapes),
    // the renderer matrix slots 95D860.. as the last display left them (4AC440 / 411210(3))
    m.map(NativeRaceTraffic::ObjectWorkBase,t.object_works.data(),t.object_works.size());
    {auto& area=native_race_area(c);
        for(std::uint32_t l=0;l<3;++l){put32(t.cells_84d6c0.data()+l*4u,area.dynamics[l].payload);put32(t.cells_84d6c0.data()+0xcu+l*4u,area.placements[l].handle);}
        m.map(0x84d6c0u,t.cells_84d6c0.data(),t.cells_84d6c0.size());
        // the race manager's 4F0DD0 block 84CEC8..84D8F4 around the lane cells: object records
        // (4F0BD0 / 4F0E40), the 84D8E0 frame and 84D8F0; the first record's head 84CEA0 here
        auto& vis=c.race.manager.visibility_84cec8;
        m.map(0x84cea0u,t.record_head_84cea0.data(),t.record_head_84cea0.size());
        m.map(0x84cec8u,vis.data(),0x84d6c0u-0x84cec8u);
        m.map(0x84d6d8u,vis.data()+(0x84d6d8u-0x84cec8u),vis.size()-(0x84d6d8u-0x84cec8u));}
    if(!t.data_681200.empty())m.map(0x681200u,t.data_681200.data(),t.data_681200.size());
    m.map_const(0x95d860u,c.race_effects.render_95d860.data(),c.race_effects.render_95d860.size());
    if(c.game_mode.game_variant==2u)native_race_hud_map_heart(c,m);   // Heart Attack: NAVI words, voice, robots (45C450, 46D250..)
    // the AREA's course road tables (46FC40 streamer): 804480 (6 x 1030), 800DA8 (2 x 1030), 80A670 (12 x 70C)
    {auto& o=native_race_area(c);
        m.map(0x804480u,o.road_primary.data(),o.road_primary.size());
        m.map(0x800da8u,o.road_preload.data(),o.road_preload.size());
        m.map(0x80a670u,o.road_secondary.data(),o.road_secondary.size());}
    if(c.game_mode.game_variant==4u){   // LAN race (479130 traffic, the CommRace saves): the network blocks, CommRace, 7DD138, clock
        native_network_map_shared(m);
        auto& w=c.race.car_world;
        if(!m.mapped(0x7de418u,4))m.map(0x7de418u,w.commrace_7de418.data(),w.commrace_7de418.size());
        if(!m.mapped(0x7dd138u,1))m.map(0x7dd138u,&w.slot_7dd138,1);
        m.map(0x7f1938u,reinterpret_cast<std::uint8_t*>(&w.clock_7f1938),4);
    }
}
void ints_out(NativeRuntimeContext& c,NativeRaceTraffic& t){
    auto& r=c.mission.racers;
    r.v680ad4=get32(t.cell_680ad4.data());r.v680ad8=get32(t.cell_680ad8.data());
    r.seed_6a4e2c=get32(t.seeds_6a4e2c.data());
    r.v64e190=std::int32_t(get32(t.cell_64e190.data()));r.v64e194=std::int32_t(get32(t.cell_64e194.data()));
    c.start_mode.manager_state_7f94c0=get32(t.requests_7f9460.data()+0x60u);   // 7F94C0 (481640)
}
}  // namespace
// 4A4010 GetRoadOfs on the native port (traffic cars and the player car): the service
// results it consumes are computed here (4A3F8A selector = +10D0 (measured), 43E3B0 road
// info when the cache is stale, 44DC50, 44DDC0's rolling word [[6A55C8]+7E], 479D90 /
// 46F7A0 for the lane classifier). `mem` maps the AREA block (7D2DA0, 7D33BC, 6A55C8).
// False when 47B890's route width failed on the lane path (the event is still updated).
bool native_get_road_ofs_4a4010(NativeRuntimeContext& c,PcRaceMemory& mem,driving::Bytes e,
                                driving::PcMatrixStack& matrices,bool gate_80fb14,std::string& error){
    driving::PcGetRoadOfsInputs in{};
    in.cache.selector_result=driving::pc_road_cache_selector_4a3f8a(e);
    const std::int32_t hint=e.i32(0x1c0);
    const bool same=in.cache.selector_result>=0&&e.u32(0x5c)==e.u32(0x10c0)&&e.i16(0x64)==e.i16(0x10c8)&&
        e.u32(0x68)==e.u32(0x10cc)&&hint==in.cache.selector_result;
    const auto tables=c.start_mode.scene_owner_course_world.tables_or_empty();
    if(!same){
        driving::PcRoadInfoContext road{tables,matrices,{c.game_mode.course_runtime.zero_7d3124[1],c.game_mode.course_runtime.zero_7d3178[1]}};
        in.cache.query_success=driving::pc_get_cs_road_info_by_cs_len(e.sub(0x105c,0x64),e.sub(0x5c,0x10),hint,road);
        for(std::size_t k=0;k<in.cache.query_output.size();++k)in.cache.query_output[k]=e.u8(0x105c+k);
    }
    in.use_lane_classifier=gate_80fb14;
    {   // 44DC50: 44C8D0(key) ? [[rec+14]] : [[7D2DF4]]
        std::uint32_t rec=0;const std::int32_t n=mem.i32(0x7d33c4u);const std::uint32_t base=mem.u32(0x7d33bcu);
        for(std::int32_t k=0;k<n;++k)if(mem.u32(base+std::uint32_t(k)*0x78u+4u)==e.u32(0x68)){rec=base+std::uint32_t(k)*0x78u;break;}
        in.lane.stage_unique=mem.i32(mem.u32(rec?rec+0x14u:0x7d2df4u));
    }
    in.lane.rolling_reference=mem.u16(mem.u32(0x6a55c8u)+0x7eu);
    in.lane.protected_gate_value=driving::PcRoadStageGateValue44f0f0;
    auto& area=native_race_area(c);
    const auto arena=area.road_arena();
    std::vector<driving::Bytes> prim,sec;
    for(unsigned k=0;k<6;++k)prim.push_back(arena.primary.sub(k*driving::PcRoadPrimaryBlock,driving::PcRoadPrimaryBlock));
    for(unsigned k=0;k<12;++k)sec.push_back(arena.secondary.sub(k*driving::PcRoadSecondaryBlock,driving::PcRoadSecondaryBlock));
    const driving::PcRoadSampleTables samples{prim.data(),prim.size(),sec.data(),sec.size()};
    // 47B890's 479D90 / 46F7A0 inputs (used only on its +4 bit 0 / +5C != 1 path)
    bool width_failed=false;
    const bool classifier_width=in.use_lane_classifier&&(e.u8(4)&1u)&&e.i32(0x5c)!=1;
    if(classifier_width){
        try{
            std::array<std::uint8_t,0x10> place{};for(unsigned k=0;k<0x10;++k)place[k]=e.u8(0x5c+k);place[0xa]=2;
            const auto r=driving::pc_route_width_479d90(driving::Bytes(place.data(),place.size()),0,3,arena,mem.bytes(0x7d2da0u,64),mem.bytes(0x7d3190u,64),matrices);
            in.lane.route_width=float(r.value);
            in.lane.route_flags=driving::pc_route_flags_46f7a0(e,arena);
        }catch(const std::exception& x){width_failed=true;error=x.what();}
    }
    driving::get_road_ofs_4a4010(e,in,mem.bytes(0x7d2da0u,64),samples);
    return !(width_failed&&e.i32(0x27c)==0);
}

namespace {
struct Run {
    NativeRuntimeContext& c;
    NativeRaceTraffic& t;
    driving::PcMatrixStack& matrices;
    PcRaceContext* ctx{};
    NativeHeartBinding* heart{};
    PcSceneRenderer* renderer{};   // course object displays (4082B0 / 410740 / 4107A0)
    void routed(std::uint32_t pc){++t.routed[pc];t.last_routed_pc=pc;}
    [[noreturn]] void missing(std::uint32_t pc,const std::string& why){++t.missing[pc];throw TrafficUnported(pc,why);}
    PcRaceMemory& m(){return t.memory;}
    driving::Bytes ev(std::uint32_t w){return m().bytes(w,0x10f0u);}
    driving::Bytes params(std::uint32_t w){return m().bytes(m().u32(w+0x2b4u),DrivingParameterViewBytes);}
    // The two feedback tables are read only (their default pointees are .rdata 5B3A30 / 5B3A3C):
    // copied into the views the crash leaves take.
    std::array<std::uint8_t,12> feedback_thresholds{};std::array<std::uint8_t,80> feedback_increments{};
    driving::PcImpactFeedback feedback(){
        const std::uint32_t a=m().u32(0x64deecu),b=m().u32(0x64def0u);
        for(std::uint32_t k=0;k<12;++k)feedback_thresholds[k]=m().u8(a+k);
        for(std::uint32_t k=0;k<80;++k)feedback_increments[k]=m().u8(b+k);
        return {driving::Bytes(feedback_thresholds.data(),12u),driving::Bytes(feedback_increments.data(),80u),m().u32(0x80fb14u)!=0u};}
    // 4A4010 GetRoadOfs: native_get_road_ofs_4a4010 (shared with the player car).
    void get_road_ofs(std::uint32_t w){
        std::string error;
        if(!native_get_road_ofs_4a4010(c,m(),ev(w),matrices,m().u32(0x80fb14u)!=0u,error))missing(0x479d90u,"47B890 route width: "+error);
    }
    // the selected Races record [83637C] (mission manager), -1 when none
    std::int32_t record(){
        const auto* races=c.start_mode.scene_owner_race_assets;const auto i=c.mission.manager.record_83637c;
        return (races&&i>=0&&std::uint32_t(i)<races->race_count)?i:-1;}
    std::uint32_t record_u32(std::uint32_t off){
        const auto* races=c.start_mode.scene_owner_race_assets;
        const std::size_t at=races->races_offset+std::size_t(record())*0x44u+off;
        if(at+4u>races->bytes.size())missing(0x4f1a90u,"Races record outside Races.bin");
        std::uint32_t v;std::memcpy(&v,races->bytes.data()+at,4);return v;}
    // events: the native event state, then the image refreshed for the module
    driving::PcEventServices car_services(){
        return driving::PcEventServices{this,[](void* u,std::uint32_t cb,std::uint32_t work,std::uint32_t){
            auto& self=*static_cast<Run*>(u);
            if(cb==0x4ad280u||cb==0x4704a0u||cb==0x4ad310u){traffic_car_init(*self.ctx,cb,work);++self.t.car_inits;return;}
            if(cb==0x470560u){self.m().put32(work+0x10d0u,0xffffffffu);++self.t.car_destroys;return;}
            throw TrafficUnported(cb,"traffic event callback outside the car function 0x55");},nullptr};
    }
    std::uint32_t operator()(const PcRaceCall& k){
        const auto pc=k.pc;const auto& a=k.args;
        switch(pc){
        // ---- event control ----
        case 0x440180u:routed(pc);driving::event_open_static_440180(c.event_state,a[0],a[1],c.event_descriptors,c.event_functions,car_services());mirror_events(c,t);return 0u;
        case 0x440200u:routed(pc);driving::event_close_immediate_440200(c.event_state,a[0],car_services());mirror_events(c,t);return 0u;
        case 0x440110u:routed(pc);driving::event_setup_440110(c.event_state,a[0],a[1],c.event_descriptors,c.event_functions);mirror_events(c,t);return 0u;
        case 0x4401d0u:routed(pc);driving::event_close_4401d0(c.event_state,a[0]);mirror_events(c,t);return 0u;
        case 0x440bb0u:routed(pc);driving::change_ctrl_func_440bb0(c.event_state,a[0],a[1]);mirror_events(c,t);return 0u;
        case 0x440bd0u:routed(pc);driving::change_disp_scene_440bd0(c.event_state,a[0],a[1]);mirror_events(c,t);return 0u;
        // ---- AREA block words (mapped) ----
        case 0x44be00u:routed(pc);return m().u32(0x7d33acu);
        case 0x44be10u:{routed(pc);const std::uint32_t p=m().u32(0x7d31dcu);return p?m().u32(p+4u):0xfu;}
        case 0x44be50u:routed(pc);return a[0]?(m().i32(0x7d2e88u)==0x14?1u:0u):(m().i32(0x7d2e80u)>9?1u:0u);
        case 0x44bed0u:routed(pc);return a[0]?0x7d3190u:0x7d2da0u;
        case 0x44bea0u:routed(pc);return 0x7d2da0u;   // mov eax,7D2DA0
        case 0x44bec0u:routed(pc);return 0x7d3190u;   // mov eax,7D3190
        case 0x44c520u:routed(pc);return m().u32(0x7d31d4u);
        case 0x44c990u:routed(pc);return (a[0]&0xffff0000u)|m().u16(m().u32(0x6a54e0u+a[0]*4u)+0x7eu);   // mov ax: EAX keeps the argument's upper bits

        case 0x44c940u:{routed(pc);   // 44C940: cache 635F2C/635F30, else the stage record +8 (as the race end binding)
            auto& rt=c.game_mode.course_runtime;
            if(std::int32_t(a[0])==rt.stage_key_635f2c)return std::uint32_t(rt.stage_value_635f30);
            const std::int32_t n=m().i32(0x7d33c4u);std::uint32_t v=0;
            if(n>0){const std::uint32_t base=m().u32(0x7d33bcu);
                for(std::int32_t i=0;i<n;++i)if(m().u32(base+std::uint32_t(i)*0x78u+4u)==a[0]){v=m().u32(base+std::uint32_t(i)*0x78u+8u);break;}}
            rt.stage_key_635f2c=std::int32_t(a[0]);rt.stage_value_635f30=std::int32_t(v);return v;}
        // ---- START / scene owner words ----
        case 0x456d60u:routed(pc);return c.race.car_world.commrace_7de418[0];
        case 0x499ba0u:routed(pc);return m().u8(0x8367c0u+std::uint32_t(std::int32_t(std::int8_t(a[0]))));
        case 0x4b00e0u:case 0x4b00d0u:   // [8421C0] == 5 / != 0 (as the race manager binding)
            if(!c.start_mode.scene_state_8421c0_known)missing(pc,"8421C0 unknown");
            routed(pc);return pc==0x4b00e0u?(c.start_mode.scene_state_8421c0==5u?1u:0u):(c.start_mode.scene_state_8421c0!=0u?1u:0u);
        case 0x4b01a0u:case 0x4b00f0u:   // variant 9 rival speed / last rank (race_variant_owners)
            routed(pc);return native_race_attack_value(c,pc,a.data(),k.argc);
        // ---- mission manager / START selection (as the race end binding) ----
        case 0x4962a0u:routed(pc);return native_mission_type4_4962a0(c)?1u:0u;
        case 0x4957f0u:routed(pc);return c.start_mode.selection_active_836374?1u:0u;   // mov al,[836374]
        case 0x495b00u:routed(pc);return c.game_mode.game_variant==4u?1u:0u;
        case 0x495800u:routed(pc);return c.mission.manager.timer_836388;
        case 0x495b20u:routed(pc);return record()>=0?record_u32(0x20u):0u;
        case 0x495880u:routed(pc);return record()>=0?record_u32(0x34u):0u;
        case 0x496390u:routed(pc);return (c.game_mode.game_variant!=4u&&record()>=0)?record_u32(8u):0u;
        case 0x4963b0u:routed(pc);return (c.game_mode.game_variant==4u&&record()>=0)?(record_u32(0xcu)?1u:0u):1u;
        case 0x49b2d0u:routed(pc);return std::uint16_t(c.game_mode.start_countdown);   // word 8367BC
        case 0x43f960u:routed(pc);return race_preset_43f960(c.start_mode.course_preset);
        case 0x55a930u:routed(pc);return c.start_mode.manager_state_7f94c0;            // [7F9460+60]
        case 0x43d470u:{routed(pc);if(a[0]>3u)missing(pc,"course type outside 0..3");
            const auto tables=c.start_mode.scene_owner_course_world.tables_or_empty();
            driving::PcCourseEndView v{std::nullopt,tables.courses[a[0]].runs.lengths};
            if(tables.courses[a[0]].runs.present)v.header=tables.courses[a[0]].runs.header;
            return driving::pc_course_end_position(v);}
        // ---- offline network globals (no LAN session: 7F94C0 = 0) ----
        case 0x45b510u:case 0x45d820u:case 0x45aff0u:case 0x45b810u:
            if(c.start_mode.manager_state_7f94c0)missing(pc,"network session globals");
            routed(pc);return 0u;
        // ---- renderer: 414040 = mov eax,[8A89F4] (measured: the 128x128 texture of the loop
        // setup); 406630(token, texture) swaps that texture into a bank's texture slot
        // (PcSceneRenderer::texture_swap_406630; a swap it cannot apply is counted).
        case 0x414040u:{routed(pc);auto* r=c.race.robots.renderer;   // mov eax,[8A89F4]: the renderer's reflection cube
            if(r)native_car_reflection_init(*r);return r?r->globals().w(0x8a89f4):c.loop_setup_state.handle_8a89f4;}
        case 0x406630u:{routed(pc);auto* r=c.race.robots.renderer;
            if(!r||!r->texture_swap_406630(a[0],a[1]))++t.skipped_texture_swaps;return 0u;}
        // ---- course queries ----
        case 0x43eb60u:{   // GetYPositionProg(mode, point, &polygon, special, &kind): returns the last type
            routed(pc);
            const auto tables=c.start_mode.scene_owner_course_world.tables_or_empty();
            driving::CourseProbe p{m().f32(a[1]),m().f32(a[1]+4),m().f32(a[1]+8)};
            std::uint32_t polygon=a[2]?m().u32(a[2]):0xffffffffu,kind=a[4]?m().u32(a[4]):1u,special=a[3]?m().u32(a[3]):0u;
            driving::CourseWorldQuery q{tables,matrices,c.event_function36.car_select.race_prediction};
            const std::uint32_t r=driving::get_y_position_prog(q,a[0],p,a[2]?&polygon:nullptr,a[3]?&special:nullptr,a[4]?&kind:nullptr);
            m().putf(a[1],p.x);m().putf(a[1]+4,p.y);m().putf(a[1]+8,p.z);
            if(a[2])m().put32(a[2],polygon);if(a[3])m().put32(a[3],special);if(a[4])m().put32(a[4],kind);
            return r;}
        case 0x43f110u:{   // 43F110(-, mode, point, &polygon, special, &kind): the racer-branch course query
            routed(pc);
            auto svc=[&](std::uint32_t f,std::uint32_t arg){PcRaceCall q{};q.pc=f;q.argc=1;q.args[0]=arg;return (*this)(q);};
            const std::uint32_t branch=svc(0x451350u,svc(0x44c940u,svc(0x450380u,8u)));
            const auto tables=c.start_mode.scene_owner_course_world.tables_or_empty();
            driving::CourseProbe p{m().f32(a[2]),m().f32(a[2]+4),m().f32(a[2]+8)};
            std::uint32_t polygon=a[3]?m().u32(a[3]):0xffffffffu,kind=a[5]?m().u32(a[5]):1u,special=a[4]?m().u32(a[4]):0u;
            driving::CourseWorldQuery q{tables,matrices,c.event_function36.car_select.race_prediction};
            const std::uint32_t r=driving::get_y_position_branch_43f110(q,branch,a[1],p,a[3]?&polygon:nullptr,a[4]?&special:nullptr,a[5]?&kind:nullptr);
            m().putf(a[2],p.x);m().putf(a[2]+4,p.y);m().putf(a[2]+8,p.z);
            if(a[3])m().put32(a[3],polygon);if(a[4])m().put32(a[4],special);if(a[5])m().put32(a[5],kind);
            return r;}
        case 0x43ed20u:{   // 43ED20(type, mode, point, &polygon, &special, &kind): the place query of 43F730
            routed(pc);
            const auto tables=c.start_mode.scene_owner_course_world.tables_or_empty();
            driving::CourseProbe p{m().f32(a[2]),m().f32(a[2]+4),m().f32(a[2]+8)};
            std::uint32_t polygon=a[3]?m().u32(a[3]):0xffffffffu,kind=a[5]?m().u32(a[5]):1u,special=a[4]?m().u32(a[4]):0u;
            driving::CourseWorldQuery q{tables,matrices,c.event_function36.car_select.race_prediction};
            const std::uint32_t r=driving::get_y_position_43ed20(q,a[0],a[1],p,a[3]?&polygon:nullptr,a[4]?&special:nullptr,a[5]?&kind:nullptr,m().f32(0x599440u));
            m().putf(a[2]+4,p.y);
            if(a[3])m().put32(a[3],polygon);if(a[4])m().put32(a[4],special);if(a[5])m().put32(a[5],kind);
            return r;}
        case 0x43e3b0u:{   // GetCsRoadInfoByCsLen(out 0x64, place, polygon hint)
            routed(pc);
            const auto tables=c.start_mode.scene_owner_course_world.tables_or_empty();
            driving::PcRoadInfoContext road{tables,matrices,{c.game_mode.course_runtime.zero_7d3124[1],c.game_mode.course_runtime.zero_7d3178[1]}};
            return driving::pc_get_cs_road_info_by_cs_len(m().bytes(a[0],0x64u),m().bytes(a[1],0x10u),std::int32_t(a[2]),road)?1u:0u;}
        case 0x4a5830u:{   // the car constructor (queue 841AC0) over the car work a0
            routed(pc);
            if(!c.game_mode.driving_data)missing(pc,"driving data pack not loaded");
            VehicleParameterSelection sel{};
            sel.variant_83036d=c.start_mode.vehicle_variant_83036d;sel.loading_scene_7de418=c.race.car_world.commrace_7de418[0];
            sel.game_mode_780258=c.game_mode.game_variant;sel.flag_65a7ac=0;sel.flag_8514a0=c.event_function36.car_select.flag_8514a0;
            auto& g=c.race.car_world.ghost_83db30;
            if(!vehicle_construct_4a5830(ev(a[0]),c.vehicle_creation,*c.game_mode.driving_data,sel,driving::Bytes(g.data(),g.size())))
                missing(pc,"4A5830 rejected its queue / data");
            return 0u;}
        case 0x43e570u:{   // 43E570(out, place, polygon hint): run geometry of the place (out 0x58 bytes)
            routed(pc);const std::uint32_t kind=m().u32(a[1]);if(kind>3u)missing(pc,"course type outside 0..3");
            const auto tables=c.start_mode.scene_owner_course_world.tables_or_empty();
            return driving::course_run_geometry_43e570(tables.courses[kind],m().bytes(a[1],0xc),m().bytes(a[0],0x58),std::int32_t(a[2]))?1u:0u;}
        case 0x43e6c0u:{   // 43E6C0(out, place, polygon hint): 43E570 without the corner refresh
            routed(pc);const std::uint32_t kind=m().u32(a[1]);if(kind>3u)missing(pc,"course type outside 0..3");
            const auto tables=c.start_mode.scene_owner_course_world.tables_or_empty();
            return driving::course_run_length_43e6c0(tables.courses[kind],m().bytes(a[1],0xc),m().bytes(a[0],0x58),std::int32_t(a[2]))?1u:0u;}
        case 0x43d390u:{   // GetPolNormal(polygon, type, out)
            routed(pc);if(a[1]>3u)missing(pc,"course type outside 0..3");
            const auto tables=c.start_mode.scene_owner_course_world.tables_or_empty();
            const auto n=driving::course_collision_world_normal(tables.courses[a[1]],std::int32_t(a[0]),matrices,tables.transforms[a[1]!=0u?1:0]);
            m().putf(a[2],n.x);m().putf(a[2]+4,n.y);m().putf(a[2]+8,n.z);return 0u;}
        case 0x4a6cf0u:routed(pc);return traffic_progress_4a6cf0(*ctx,a[0]);
        case 0x4efb90u:routed(pc);return traffic_appear_rec_4efb90(*ctx,a[0]);
        case 0x4f0030u:routed(pc);return traffic_speed_rec_4f0030(*ctx,a[0],a[1]);
        case 0x44be30u:routed(pc);return m().i32(0x7d2e80u)>0x11?1u:0u;
        case 0x44b7b0u:{routed(pc);   // 44B7B0 (bridge 447F7A: al = [7D33D0]), as the race manager binding
            if(m().u8(0x7d33d0u)&&a[0]==0u)return 1u;
            const std::uint32_t sel=m().u32(0x7d3188u);
            if(m().i32(sel+0x2cu)==-1&&m().i32(sel+0x30u)==-1&&a[0]==0u){m().put8(0x7d33d0u,1u);return 1u;}
            return 0u;}
        case 0x45c440u:routed(pc);return c.race.car_world.heart_mode_7f2428;
        case 0x45c450u:case 0x45c460u:{routed(pc);   // mov eax,[7F2430]; mov ax,[eax(+2)]: the request record's words
            const std::uint32_t p=m().u32(0x7f2430u);return (p&0xffff0000u)|m().u16(p+(pc==0x45c460u?2u:0u));}
        case 0x44c8d0u:routed(pc);return race_area_record_44c8d0(m(),a[0]);
        case 0x4a3f80u:{
            // 4A3F80(car, polygon): the cached road quad of the car (+105C road info, +10C0 place,
            // +10D0 polygon). The protected 4A3F8A load is taken as EAX = [car+10D0]: the routine
            // stores the polygon there on success and the car inits / 470560 write -1 (inferred,
            // the at-mode harness reads 0 for every input).
            routed(pc);const std::uint32_t w=a[0];const std::int32_t hint=std::int32_t(a[1]);
            const std::int32_t cached=m().i32(w+0x10d0u);
            if(cached>=0){
                if(m().u32(w+0x5c)==m().u32(w+0x10c0)&&m().i16(w+0x64)==m().i16(w+0x10c8)&&m().u32(w+0x68)==m().u32(w+0x10cc)&&hint==cached)return 1u;
                m().put32(w+0x10d0u,0xffffffffu);
            }
            const auto tables=c.start_mode.scene_owner_course_world.tables_or_empty();
            driving::PcRoadInfoContext road{tables,matrices,{c.game_mode.course_runtime.zero_7d3124[1],c.game_mode.course_runtime.zero_7d3178[1]}};
            if(!driving::pc_get_cs_road_info_by_cs_len(m().bytes(w+0x105cu,0x64u),m().bytes(w+0x5cu,0x10u),hint,road))return 0u;
            m().put32(w+0x10d0u,std::uint32_t(hint));
            for(std::uint32_t q=0;q<0x10;q+=4)m().put32(w+0x10c0u+q,m().u32(w+0x5cu+q));
            return 1u;}
        // ---- traffic car set-up (SetOthcarWork 46E830 / racer cars) ----
        case 0x4874f0u:routed(pc);if(!vehicle_clear_4874f0(ev(a[0]),a[1]!=0u))missing(pc,"4874F0 car work view");return 0u;
        case 0x455c10u:routed(pc);   // movsx [7DF10B] + 1 (CommRace block 7DE418)
            return std::uint32_t(std::int32_t(std::int8_t(c.race.car_world.commrace_7de418[0x7df10bu-0x7de418u]))+1);
        case 0x5051d0u:{routed(pc);  // 2B4 = 5E3140 + 4 * column of the model's selection map
            if(!c.game_mode.driving_data)missing(pc,"driving data pack not loaded");
            VehicleParameterSelection sel{};
            sel.variant_83036d=c.start_mode.vehicle_variant_83036d;sel.loading_scene_7de418=c.race.car_world.commrace_7de418[0];
            sel.game_mode_780258=c.game_mode.game_variant;sel.flag_65a7ac=0;sel.flag_8514a0=c.event_function36.car_select.flag_8514a0;
            VehicleParameterChoice choice{};
            if(!vehicle_parameter_choice_5051d0(*c.game_mode.driving_data,a[1]&0xffu,sel,choice))missing(pc,"model outside the parameter maps");
            m().put32(a[0]+0x2b4u,choice.pc_address);return 0u;}
        case 0x487570u:routed(pc);if(!vehicle_gear_thresholds_487570(ev(a[0]),params(a[0])))missing(pc,"487570 gear count above 6");return 0u;
        case 0x4f6e40u:{routed(pc);   // 520950(model) = 5E8A88 + movsx(model)*0x3C (the traffic models' descriptors are EXE data)
            const std::uint32_t d=0x5e8a88u+std::uint32_t(std::int32_t(std::int8_t(m().u8(a[0]+0x11u))))*0x3cu;
            const auto* p=m().at(d,0x3c);std::array<std::uint8_t,0x3c> desc{};std::memcpy(desc.data(),p,desc.size());
            vehicle_collision_init_4f6e40(ev(a[0]),driving::Bytes(desc.data(),desc.size()));return 0u;}
        case 0x43d440u:{routed(pc);   // polygon +3C flags of type a0 (AX; EAX keeps the loaded upper bits)
            if(a[0]>3u)missing(pc,"course type outside 0..3");
            const auto tables=c.start_mode.scene_owner_course_world.tables_or_empty();
            const auto& t0=tables.courses[a[0]];
            if(!t0.polygons_present)return a[0]&0xffff0000u;
            if(a[1]==0xffffffffu)return 0xffff0000u;
            return ((a[1]<<6)&0xffff0000u)|std::uint16_t(t0.polygons.i16(std::size_t(a[1])*0x40u+0x3cu));}
        case 0x4960a0u:{routed(pc);   // 4960A0(car): speed above the record's limit (bridge 447C1F: EAX = Races record or 0)
            const std::uint32_t w=a[0];
            const float speed=m().f32(w+0x1c4)*m().f32(0x5a460cu);
            float limit=(m().f32(m().u32(w+0x2b4)+0x134cu)-m().f32(0x5b4454u))*m().f32(0x5b4450u)+m().f32(0x5b444cu);
            const bool type3=record()>=0&&record_u32(0x20u)==3u;
            if(!type3)limit=limit*m().f32(0x5b43c8u);
            return speed>limit?1u:0u;}
        // ---- per-car control children (47E780) on the native CommonCar ports ----
        case 0x502420u:routed(pc);driving::auto_transmission(ev(a[0]),params(a[0]));return 0u;
        case 0x4a2ee0u:routed(pc);driving::set_old_param_buffer_4a2ee0(ev(a[0]));return 0u;
        case 0x4a45f0u:{routed(pc);   // CalcOfsLeftLane of a crashed car (as race_player_car binds it)
            const driving::Bytes e=ev(a[0]);
            driving::PcOfsLeftLaneInputs in{};
            in.selector=driving::pc_ofs_left_lane_selector_4a45f9(e);
            if(in.selector!=-1){
                const std::uint32_t type=e.u32(0x5c);
                auto& world=c.start_mode.scene_owner_course_world;
                const auto tables=world.tables_or_empty();
                if(type>=tables.courses.size())missing(pc,"course type outside the tables");
                driving::Bytes rates(nullptr,0);
                if(world.lane_loaded(type)){   // 43D130: the lane rates, section +30 of the COLI0200 header (4 bytes per polygon)
                    const auto& pack=world.lane(type);
                    if(pack.pc_coli0200.size()>=0x40){
                        const driving::Bytes h(const_cast<std::uint8_t*>(pack.pc_coli0200.data()),pack.pc_coli0200.size());
                        const std::size_t off=h.u32(0x30),n=std::size_t(pack.pc_layout.polygon_count)*4u;
                        if(off<=h.size()&&n<=h.size()-off)rates=h.sub(off,n);
                    }
                }
                in.rates=driving::pc_lane_rates_43d130(in.selector,rates.size()?&rates:nullptr);
                const auto& poly=tables.courses[type].polygons;
                const driving::Bytes transform=tables.transforms[type?1u:0u];
                const std::size_t at=std::size_t(std::uint32_t(in.selector))*0x40u;
                if(poly.size()<at+0x30u||transform.size()<64u)missing(0x43d1d0u,"polygon corners outside the course");
                driving::pc_matrix_push_load(matrices,transform);
                for(unsigned k=0;k<4;++k)in.points[k]=driving::pc_matrix_point(matrices,{poly.f32(at+k*12u),poly.f32(at+k*12u+4u),poly.f32(at+k*12u+8u)});
                driving::pc_matrix_pop(matrices);
            }
            driving::calc_ofs_left_lane_4a45f0(e,in);return 0u;}
        case 0x4a2400u:routed(pc);driving::pc_advance_crash_state(ev(a[0]),race_crash_tables());return 0u;
        case 0x4a25f0u:{routed(pc);   // night 4AFB60 / tunnel 4AFB90 bits of *[79F5EC], course flags 44BDB0
            const std::uint8_t fx=m().u8(m().u32(0x79f5ecu));
            const std::uint32_t sel=m().u32(0x7d3188u);
            const std::uint32_t flags=sel?m().u32(m().u32(sel+0x14u)+0x3cu):0u;
            driving::check_night_and_tunnel_4a25f0(ev(a[0]),{((fx>>3)&1u)!=0u,((fx>>4)&1u)!=0u,std::uint8_t(flags)});return 0u;}
        case 0x4a2130u:{routed(pc);
            const auto tables=c.start_mode.scene_owner_course_world.tables_or_empty();
            auto end=[&](unsigned k){driving::PcCourseEndView v{std::nullopt,tables.courses[k].runs.lengths};
                if(tables.courses[k].runs.present)v.header=tables.courses[k].runs.header;return driving::pc_course_end_position(v);};
            driving::PcStageProgressInputs in{end(0),end(1),m().u32(0x80fb14u)!=0u,c.start_mode.selection_active_836374!=0u};
            driving::car_calc_current_stage_progress_4a2130(ev(a[0]),in,c.race.car_world.progress_680bd0);return 0u;}
        // 4A20F0(word position): the offset word of the history entry before the first entry whose
        // position is above it, -1 when none is; 4A2120: the offset word before entry [680BD0]
        // (841B4E + i*4 = offset of entry i-1; entry 0 never matches: its position is 0).
        case 0x4a20f0u:case 0x4a2120u:{routed(pc);
            const auto& h=c.race.car_world.progress_680bd0;
            if(h.count>h.position.size())missing(pc,"stage progress history count past 30");
            std::uint32_t i=h.count;
            if(pc==0x4a20f0u){
                const std::uint16_t at=std::uint16_t(a[0]);
                for(i=0;i<h.count&&!(at<h.position[i]);++i){}
                if(i==h.count)return 0xffffffffu;
            }
            if(i==0u)missing(pc,"stage progress word 841B4E (before the history)");
            return h.offset[i-1u];}
        case 0x4a2650u:{routed(pc);   // CalcDispMatrix as the ghost cars / player car bind it
            const std::uint8_t autoscene=std::uint8_t(c.event_state.slots[6].flags);
            if((autoscene&3u)==2u)missing(0x4b5fd0u,"4B5FD0 needs the active AUTOSCENE work 799CA0");
            driving::PcCameraBlend blend{c.race.camera_override.override_82e7d8,autoscene,0,enhancements::display_blend()};
            driving::PcDispMatrixContext display{matrices,driving::camera_blend_4493e0(blend),c.race.camera_override.scene_82e7d4};
            driving::pc_calc_disp_matrix(ev(a[0]),display);
            enhancements::display_note_car(ev(a[0]),[&c=c]{const std::uint8_t autoscene=std::uint8_t(c.event_state.slots[6].flags);   // port: replay between ticks
            if((autoscene&3u)==2u)return 1.f;
            return driving::camera_blend_4493e0({c.race.camera_override.override_82e7d8,autoscene,0,enhancements::display_blend()});},c.race.camera_override.scene_82e7d4);
            return 0u;}
        case 0x4a2910u:{routed(pc);   // CheckReverseCar: 4866C0(model) words +20..+2C, 580F40 when +1F4 > 100
            const std::uint32_t w=a[0];const std::uint32_t md=0x650500u+std::uint32_t(std::int32_t(std::int8_t(m().u8(w+0x11))))*0x44u;
            driving::PcReverseCarInputs in{m().f32(md+0x20u),m().f32(md+0x24u),m().f32(md+0x28u),m().f32(md+0x2cu),0,std::int32_t(c.mode_state.current)};
            if(m().u32(w+0x1f4u)>100u)in.random_value=frontend_crt_random_580f40(c.event_function36.pc_crt_random_state);
            driving::check_reverse_car_4a2910(ev(w),in,matrices);return 0u;}
        case 0x4a4010u:{routed(pc);get_road_ofs(a[0]);return 0u;}
        case 0x424940u:{routed(pc);   // race SE queue 9563E8 gated by event 383 (as the race end binding)
            auto& w=c.race.car_world;
            driving::PcSoundQueue q{driving::Bytes(w.sound_entries_9563e8.data(),w.sound_entries_9563e8.size()),
                driving::Bytes(w.sound_state.data(),w.sound_state.size()),std::uint8_t(c.event_state.slots[383].flags)};
            driving::pc_enqueue_sound(q,a[0]);return 0u;}
        case 0x46c780u:{routed(pc);   // impact feedback on +BD0 from the 64DEEC / 64DEF0 tables
            float scale;std::memcpy(&scale,&a[2],4);
            driving::pc_add_impact_feedback(ev(a[0]),a[1],scale,feedback());return 0u;}
        case 0x4a6ea0u:case 0x4a2270u:{routed(pc);
            float rate=1.0f;if(pc==0x4a2270u)std::memcpy(&rate,&a[4],4);
            std::string err;
            if(!race_car_crash_entry(c,matrices,ev(a[0]),pc,a[1],a[2],a[3]!=0u,rate,feedback(),err))missing(pc,err);
            return 0u;}
        case 0x5041b0u:{routed(pc);   // CbwColiWall(car, wall work 850BA0, four contact records)
            // An OthCar has no parameter record (+2B4 = 0, only 5051D0 sets one): 5041B0 itself
            // never reads +2B4, so its view is empty and any use of it faults (a PC null read).
            std::string err;
            const driving::Bytes P=m().u32(a[0]+0x2b4u)?params(a[0]):driving::Bytes(nullptr,0);
            if(!race_car_cbw_coli_wall_5041b0(c,matrices,ev(a[0]),m().bytes(a[1],0x900u),P,m().bytes(a[2],0x40u),err))missing(pc,"5041B0: "+err);
            return 0u;}
        case 0x44f0f0u:{routed(pc);   // 1 when 44DDC0(stage, cs) != -1 (measured tail, pc_car_services)
            const std::uint16_t rolling=m().u16(m().u32(0x6a55c8u)+0x7eu);
            return driving::pc_road_stage_gate_44f0f0(std::int32_t(a[0]),std::uint16_t(a[1]),rolling);}
        // ---- course objects (OSO): the current event, its work, its control ----
        case 0x440b80u:routed(pc);return c.event_state.current_slot;
        case 0x440a60u:{   // MallocNowEventWork(size, heap): the current event's slot of the object works
            const std::uint32_t ev=c.event_state.current_slot;
            if(ev>=410u||a[0]+8u>NativeRaceTraffic::ObjectWorkSlot)missing(pc,"440A60 work of event "+std::to_string(ev)+" size "+std::to_string(a[0]));
            routed(pc);
            const std::uint32_t base=NativeRaceTraffic::ObjectWorkBase+ev*NativeRaceTraffic::ObjectWorkSlot;
            auto& sl=c.event_state.slots[ev];sl.work_token=base;sl.aux24=base+a[0];
            mirror_events(c,t);return base;}
        case 0x440b20u:{routed(pc);   // free the current event's work (handle [7A01D0]+24)
            const std::uint32_t ev=c.event_state.current_slot;if(ev<410u)c.event_state.slots[ev].aux24=0;
            mirror_events(c,t);return 0u;}
        case 0x440b90u:routed(pc);driving::change_ctrl_func_440bb0(c.event_state,c.event_state.current_slot,a[0]);mirror_events(c,t);return 0u;
        case 0x43d1d0u:{routed(pc);if(a[1]>3u)missing(pc,"course type outside 0..3");
            const auto tables=c.start_mode.scene_owner_course_world.tables_or_empty();
            driving::cop_coli_point(tables,a[0],a[1],matrices,{m().bytes(a[2],12),m().bytes(a[3],12),m().bytes(a[4],12),m().bytes(a[5],12)});
            return 0u;}
        case 0x502e20u:{routed(pc);float x,z;std::memcpy(&x,&a[0],4);std::memcpy(&z,&a[1],4);
            driving::calc_coli_wall_face(x,z,{m().bytes(a[2],12),m().bytes(a[3],12),m().bytes(a[4],12),m().bytes(a[5],12)});return 0u;}
        case 0x4493e0u:{routed(pc);   // camera blend rate (as 4A2650 binds it)
            const std::uint8_t autoscene=std::uint8_t(c.event_state.slots[6].flags);
            if((autoscene&3u)==2u)missing(0x4b5fd0u,"4B5FD0 needs the active AUTOSCENE work 799CA0");
            driving::PcCameraBlend blend{c.race.camera_override.override_82e7d8,autoscene,0,1.0f};
            const float f=driving::camera_blend_4493e0(blend);std::uint32_t u;std::memcpy(&u,&f,4);return u;}
        case 0x44bdd0u:{routed(pc);   // bridge 4CC25F: [7D3188] (as the race manager binds it)
            const std::uint32_t sel=m().u32(0x7d3188u);
            if(sel){if(m().i32(0x7d2e80u)>8)return m().u32(sel+4u);if(const std::uint32_t alt=m().u32(0x7d31dcu))return m().u32(alt+4u);}
            return m().u32(0x7d30acu);}
        case 0x4082b0u:{   // GetLightWorkAddress(a, b, c) (bridge 447B4A, as the robots bind it)
            if(!renderer)missing(pc,"4082B0 outside a display");
            routed(pc);
            if(a[1]==0u)return 0x899b98u+(a[0]+a[2])*0xa0u;
            if(a[1]==1u)return 0x89a138u+(a[2]+a[0]*2u)*0xa0u;
            return 0x899d78u+(a[2]+a[0]*2u)*0xa0u;}
        case 0x410740u:if(!renderer)missing(pc,"410740 outside a display");routed(pc);render_lights_reset_410740(renderer->flush_context());return 0u;
        case 0x4107a0u:if(!renderer)missing(pc,"4107A0 outside a display");routed(pc);render_light_add_4107a0(renderer->flush_context(),m().bytes(a[0],0x94));return 0u;
        case 0x580f33u:routed(pc);c.event_function36.pc_crt_random_state=a[0];return 0u;   // srand
        // 417F70 = [8A8CE0] (protected 4D4A66, measured). Its only writer is the game init 417740
        // (0): no other plain store, and the protected [A]-[B] address stubs only read it.
        case 0x417f70u:routed(pc);return 0u;
        // ---- Heart Attack display (462CD0) ----
        case 0x4294c0u:{routed(pc);   // 4294C0(token, &w, &h): half the root component size; -1 without a scene
            FrontendSpriteTiming s{};
            if(!c.event_function36.frontend_sprites.bank_scene(a[0],s))return 0xffffffffu;
            m().putf(a[1],float(std::int16_t(s.width))*0.5f);m().putf(a[2],float(std::int16_t(s.height))*0.5f);return 0u;}
        case 0x4295d0u:case 0x4bc990u:{   // 3D sprite / NAVI digits: replayed by the NAVI display in order
            if(!heart||!heart->display||!heart->draws)missing(pc,"Heart Attack 2D submission outside its display");
            routed(pc);RaceHudDraw d;d.pc=pc;
            if(pc==0x4295d0u){
                d.args={a[0],a[2],a[3],a[4],a[5],a[6],a[7]};
                for(unsigned q=0;q<3;++q)d.matrix[q]=m().f32(a[1]+q*4u);
                std::memcpy(&d.matrix[3],&a[8],4);
            }else{
                d.args[0]=a[0];d.args[1]=a[1];d.args[2]=a[2];
                char text[16]{};for(unsigned q=0;q<15;++q){text[q]=char(m().u8(a[3]+q));if(!text[q])break;}
                if(m().u8(a[3]+15u)&&text[14])missing(pc,"4BC990 text longer than 15 characters");
                std::memcpy(&d.args[3],text,16);
            }
            heart->draws->push_back(d);return 0u;}
        case 0x43fa00u:routed(pc);return c.frame_state.updates_95af48;      // [780278]
        case 0x450250u:{routed(pc);   // 450250(stage, result): the stage key a result leads to
            const std::uint16_t preset=std::uint16_t(m().u32(0x78024cu));
            if(preset==2u||preset==3u)return a[0]+1u;
            PcRaceCall q{};q.pc=0x44c940u;q.argc=1;q.args[0]=a[0];
            const std::uint32_t lvl=(*this)(q);
            if(lvl>3u)return 0xffffffffu;
            std::uint32_t r=lvl==0u?1u:a[0]+1u+lvl;
            if(lvl!=0u&&r==0xffffffffu)return r;
            return a[1]==1u?r+1u:r;}
        case 0x429530u:case 0x4289e0u:case 0x428ac0u:{   // object sprites submitted by a control: drawn by the HUD display
            routed(pc);RaceHudDraw d;d.pc=pc;for(unsigned q=0;q<6;++q)d.args[q]=a[q];
            const std::uint32_t mp=pc==0x428ac0u?a[1]:pc==0x4289e0u?a[2]:0u;
            if(mp)for(unsigned q=0;q<16;++q)d.matrix[q]=m().f32(mp+q*4u);
            if(heart&&heart->draws&&(pc==0x4289e0u||(heart->display&&pc==0x429530u)))heart->draws->push_back(d);   // a Heart Attack display (45CA70, 462CD0)
            else if(t.object_draws.size()<4096u)t.object_draws.push_back(d);
            return 0u;}
        // ---- Heart Attack (464D20 and its display callbacks) ----
        case 0x440370u:routed(pc);return m().u8(0x79fb48u+a[0])&4u;            // CheckEventDestructing
        case 0x44ddc0u:routed(pc);
            return std::uint32_t(driving::road_stage_window_44ddc0(std::int32_t(a[0]),std::uint16_t(a[1]),m().u16(m().u32(0x6a55c8u)+0x7eu)));
        case 0x580f40u:routed(pc);return frontend_crt_random_580f40(c.event_function36.pc_crt_random_state);
        // SPRANI pool (95E028 + handle * 0x7C), the race end binding's semantics
        case 0x428320u:routed(pc);return c.event_function36.frontend_sprites.create(a[0],a[1],a[2],-1,-1,false,c.event_function36.title_pause_flag_95b214);
        case 0x428460u:routed(pc);return c.event_function36.frontend_sprites.create(a[0],a[1],a[2],std::int32_t(a[3]),std::int32_t(a[4]),true,c.event_function36.title_pause_flag_95b214);
        case 0x4285a0u:routed(pc);if(a[0]<FrontendSprites::Count)c.event_function36.frontend_sprites.release(a[0]);return 0u;
        case 0x428770u:routed(pc);if(auto* s=c.event_function36.frontend_sprites.get_mutable(a[0]))s->mode=a[1];return 0u;
        case 0x428800u:{routed(pc);float f;std::memcpy(&f,&a[1],4);if(a[0]<FrontendSprites::Count)c.event_function36.frontend_sprites.set_speed(a[0],f);return 0u;}
        case 0x428880u:routed(pc);return std::int32_t(a[0])<0?0u:c.event_function36.frontend_sprites.status(a[0]);
        case 0x4287b0u:{routed(pc);if(a[0]>=FrontendSprites::Count)return 0u;
            std::array<float,16> mat{};for(std::uint32_t w=0;w<16;++w)mat[w]=m().f32(a[1]+w*4);
            c.event_function36.frontend_sprites.set_matrix(a[0],mat);return 0u;}
        case 0x428940u:routed(pc);   // instance +2C (the 7551B4 id 428170 copies) = a1 when the handle is >= 0
            if(std::int32_t(a[0])>=0)if(auto* s=c.event_function36.frontend_sprites.get_mutable(a[0]))s->id_2c=std::int32_t(a[1]);
            return 0u;
        case 0x429780u:{routed(pc);   // 429780(token, &a, &b): the scene's root component (48BD20): a = ftol(+4), b = ftol(+8) - 1
            FrontendSpriteTiming t{};
            if(!c.event_function36.frontend_sprites.bank_scene(a[0],t))return 0u;
            auto ftol=[](float v){return (v>-2147483649.0f&&v<2147483648.0f)?std::uint32_t(std::int32_t(v)):0x80000000u;};
            if(a[1])m().put32(a[1],ftol(t.root4));
            if(a[2])m().put32(a[2],ftol(t.frames)-1u);
            return 0u;}
        // 440D10 / 440D30 / 440D50 / 440D70: the heap selection stacks 7B11A8 / 7B1194 of the
        // CRT allocations around them (the request manager's heap is a single block here).
        case 0x440d10u:case 0x440d50u:case 0x440d30u:case 0x440d70u:routed(pc);return 0u;
        case 0x43f940u:routed(pc);c.game_mode.game_variant=a[0];return 0u;   // 43F940: [780258] = variant (46C2D0: 5)
        case 0x49a650u:routed(pc);return 0u;   // ret (the empty callback / virtual)
        // 4F12A0(path, loader, async, required) / 4F1A90(category, loader) / 4F1BA0(category, loader)
        // on the request manager's guest loaders (+A0: the stage's Req_*.bin; loader +0 category
        // index, +4 relocated blob, +8 phase 1 -> 3, +C handle): the file read at once from the
        // retail tree into its heap, relocated (4F13F0); the category lookup of 4F1AB1.
        case 0x4f12a0u:{routed(pc);
            const std::uint32_t loader=a[1];
            if(!loader||m().u32(loader+8u)!=1u)return 0u;   // phase 2 does not exist here; 3: done (0)
            std::string relative=guest_string(m(),a[0]);
            while(!relative.empty()&&(relative[0]=='\\'||relative[0]=='/'))relative.erase(0,1);
            for(auto& ch:relative)if(ch=='\\')ch='/';
            if(const auto old=m().u32(loader+4u)){t.requests_heap->free(old);m().put32(loader+4u,0u);}
            std::vector<std::uint8_t> bytes;
            auto* store=c.event_function36.retail_assets;
            if(!store||!retail_asset_read_relative(*store,relative,bytes,8u<<20)||bytes.size()<8u){
                if(a[2])return 0u;                               // 4F12F8: the async open failed
                m().put32(loader,0u);m().put32(loader+4u,0u);m().put32(loader+8u,3u);return 1u;}
            const std::uint32_t base=t.requests_heap->alloc(std::uint32_t(bytes.size()));
            if(!base)missing(pc,"request manager heap full ("+relative+")");
            for(std::size_t k=0;k<bytes.size();++k)m().put8(base+std::uint32_t(k),bytes[k]);
            const std::uint32_t relocs=m().u32(base+4u);
            for(std::uint32_t k=0;k<relocs;++k){
                const std::uint32_t patch=m().u32(base+8u+k*8u),target=m().u32(base+12u+k*8u);
                if(patch+4u>bytes.size()||target>bytes.size())missing(pc,relative+": relocation outside the file");
                m().put32(base+patch,base+target);}
            m().put32(loader+4u,base);m().put32(loader,base+8u+relocs*8u);m().put32(loader+8u,3u);
            return 1u;}
        case 0x4f1a90u:case 0x4f1ba0u:{
            const std::uint32_t loader=a[1];
            if(!loader||!m().u32(loader)||!m().u32(loader+4u))break;   // the global tables 84D91C..: not here
            routed(pc);
            const std::uint32_t hash=driving::runtime_category_hash_4f1260(guest_string(m(),a[0]).c_str());
            const std::uint32_t index=m().u32(loader),base=m().u32(loader+4u);
            std::int32_t hi=std::int32_t(m().u32(base)),lo=-1;
            while(hi-lo>1){const std::int32_t mid=(hi+lo)>>1;if(hash<=m().u32(index+std::uint32_t(mid)*12u))hi=mid;else lo=mid;}
            const std::uint32_t entry=index+std::uint32_t(hi)*12u;
            if(m().u32(entry)!=hash)return 0u;
            return pc==0x4f1a90u?m().u32(entry+8u)+base:m().u32(entry+4u);}
        case 0x44c830u:{routed(pc);   // [7D3188] (the selected course record 7D30A8 while active) +4, else 0xF
            const auto& rt=c.game_mode.course_runtime;
            if(!rt.selected_copy_active)return 0xfu;
            std::uint32_t v;std::memcpy(&v,rt.selected_7d30a8.data()+4u,4);return v;}
        // CRT `eh vector constructor / destructor iterator`(array, size, count, ctor[, dtor]): the
        // request manager's sprite resource arrays (465160 / 465250)
        case 0x5816bdu:case 0x58165du:{routed(pc);
            const std::uint32_t array=a[0],size=a[1],count=a[2],fn=a[3];
            for(std::uint32_t i=0;i<count;++i){
                const std::uint32_t at=pc==0x5816bdu?array+i*size:array+(count-1u-i)*size;   // destruction from the last
                PcRaceCall q{};q.pc=fn;q.ecx=at;(*this)(q);}
            return 0u;}
        // ---- 44B750: the LCG seeds 6A4E2C ----
        case 0x46c500u:if(c.start_mode.manager_state_7f94c0==0u){routed(pc);return 0u;}break;   // 55A930() == 0: no 7F94C8 read
        case 0x455670u:{routed(pc);std::uint32_t v;   // mov eax,[7DF1A4] (CommRace block)
            std::memcpy(&v,c.race.car_world.commrace_7de418.data()+(0x7df1a4u-0x7de418u),4);return v;}
        case 0x44b750u:{routed(pc);const std::uint32_t at=0x6a4e2cu+a[0]*4u;
            const std::uint32_t v=std::uint16_t(std::uint16_t(m().u16(at)*0x5e5u)+0x29u);m().put32(at,v);return v;}
        // ---- 483E90: the camera eye (camera +F8) ----
        case 0x483e90u:{routed(pc);const std::uint32_t cam=m().u32(0x79f574u)+0xf8u;
            for(std::uint32_t q=0;q<0xc;q+=4)m().put32(a[0]+q,m().u32(cam+q));return 0u;}
        default:break;
        }
        std::uint32_t eax=0;
        if(native_race_manager_call(c,pc,k.args.data(),k.argc,eax,true)){routed(pc);return eax;}
        if(c.game_mode.game_variant==4u&&ctx){   // LAN race: the native traffic parts (46F990...), then the translated LAN set (matrix leaves 409EF0.., 4F53B0...)
            try{const std::uint32_t r=traffic_control_part(*ctx,pc,a[0],a[1],a[2]);routed(pc);return r;}
            catch(const std::logic_error& e){if(std::string(e.what())!="race traffic: unknown control part")throw;}
            {std::uint32_t r=0;if(traffic_network_call(*ctx,pc,k.ecx,k.args.data(),std::min<std::size_t>(k.argc,k.args.size()),r)){routed(pc);return r;}}
            const TranslatedModule nm{network_cars_functions,nullptr,{},nullptr,&matrices,&c.event_function36.pc_crt_random_state,network_cars_code_data};
            if(translated_has(nm,pc)){routed(pc);
                const std::vector<std::uint32_t> args(k.args.begin(),k.args.begin()+std::min<std::size_t>(k.argc,k.args.size()));
                return translated_call_list(m(),ctx->service,nm,pc,TranslatedRegisters{k.eax,k.ecx,0,0,0,0},args);}
        }
        if(t.requests_heap&&translated_crt_call(m(),*t.requests_heap,k,eax)){routed(pc);return eax;}   // the request manager's new / delete
        // 465160..4659F0: sprite resources (0xA0) inside the request manager's objects, on the
        // frontend sprite pool (as the network screens' resources)
        if(pc>=0x465160u&&pc<=0x4659f0u){
            auto& st=c.event_function36;
            auto bytes=m().bytes(k.ecx,0xa0u);
            if(pc==0x465160u){routed(pc);if(!title_ui_resource_construct_465160(bytes.data(),0xa0u))missing(pc,"465160 resource construct");return k.ecx;}
            FrontendUiResources ui{st.frontend_sprites};ui.motion_step=st.frontend_ui_motion_step;
            ui.effect_user=st.frontend_effect_user;ui.effect_4249f0=st.frontend_effect;
            std::uint32_t value{};
            if(!ui.call(pc,bytes,k.args.data(),std::min<std::size_t>(k.argc,k.args.size()),value)||ui.missing_pc)
                missing(ui.missing_pc?ui.missing_pc:pc,"sprite resource service");
            routed(pc);return value;}
        if(bulk_translated(pc)&&ctx){routed(pc);return bulk_call(m(),ctx->service,k,&matrices,&c.event_function36.pc_crt_random_state);}
        missing(pc,"traffic service not ported");
    }
};
template<class F> bool run_traffic(NativeRuntimeContext& c,driving::PcMatrixStack& matrices,F&& body){
    auto& t=native_race_traffic(c);
    if(t.latched){++t.skipped;return true;}
    const auto depth=matrices.depth;
    Run run{c,t,matrices};
    try{
        map_memory(c,t);racers_in(c,t);
        PcRaceContext ctx{t.memory,matrices,[&](const PcRaceCall& k){return run(k);},nullptr};
        run.ctx=&ctx;
        body(ctx);
        racers_out(c,t);ints_out(c,t);
        return true;
    }catch(const TrafficUnported& e){t.fault=e.pc;t.error=e.what();}
    catch(const PcRaceUnmapped& e){t.fault=e.address;t.error=std::string("unmapped PC address: ")+e.what();}
    catch(const std::exception& e){t.fault=0x47ec00u;t.error=e.what();}
    while(matrices.depth>depth)driving::pc_matrix_pop(matrices);
    t.latched=true;
    std::fprintf(stderr,"race traffic latched: %s\n",t.error.c_str());
    return false;
}
}
NativeRaceTraffic::NativeRaceTraffic(){
    for(std::uint32_t k=0;k<3;++k)put32(seeds_6a4e2c.data()+k*4u,0xa5deu);   // .data 6A4E2C..6A4E37 (44B750 LCG seeds)
    for(std::size_t i=0;i<EmbeddedExeRangeCount;++i)
        if(EmbeddedExeRanges[i].base==0x64de00u)data_64de00.assign(EmbeddedExeRanges[i].data,EmbeddedExeRanges[i].data+EmbeddedExeRanges[i].size);
        else if(EmbeddedExeRanges[i].base==0x6a5df8u)data_6a5df8.assign(EmbeddedExeRanges[i].data,EmbeddedExeRanges[i].data+EmbeddedExeRanges[i].size);
        else if(EmbeddedExeRanges[i].base==0x681200u)data_681200.assign(EmbeddedExeRanges[i].data,EmbeddedExeRanges[i].data+EmbeddedExeRanges[i].size);
}
NativeRaceTraffic::~NativeRaceTraffic()=default;
NativeRaceTraffic& native_race_traffic(NativeRuntimeContext& c){
    if(!c.race_traffic)c.race_traffic=std::make_shared<NativeRaceTraffic>();
    return *c.race_traffic;
}
void native_race_requests_map(NativeRuntimeContext& c,PcRaceMemory& m){
    auto& t=native_race_traffic(c);
    put32(t.requests_7f9460.data()+0x60u,c.start_mode.manager_state_7f94c0);   // +60 = 7F94C0 (55A930)
    m.map(NativeRaceTraffic::RequestsBase,t.requests_7f9460.data(),t.requests_7f9460.size());
    if(!t.requests_heap)t.requests_heap=std::make_unique<GuestHeap>(NativeRaceTraffic::RequestsHeapBase,NativeRaceTraffic::RequestsHeapSize);
    t.requests_heap->map(m);
    if(t.requests_cells.empty()){
        static constexpr std::pair<std::uint32_t,std::uint32_t> Cells[]{{0x84d900u,0x80u},{0x850b80u,0x20u},{0x854d00u,0x40u},
            {0x856940u,0x10u},{0x8572a0u,0x28u},{0x85b340u,0x180u}};
        std::vector<std::uint8_t> data(0x100u);
        std::memcpy(data.data(),exe_image_bytes(0x64f000u,0x100u),0x100u);
        t.requests_cells.emplace_back(0x64f000u,std::move(data));
        for(const auto& [base,size]:Cells)t.requests_cells.emplace_back(base,std::vector<std::uint8_t>(size));
    }
    for(auto& [base,bytes]:t.requests_cells)m.map(base,bytes.data(),bytes.size());
}
namespace {
// LAN race cars (440380: events 9.., function 0x67 in variant 4, 0x54 in variant 3):
// init 4A5F10 / 4A5B90, control 4A60D0 / 4A5C70, display 4AE5F0 / 4ADAC0, destroy RET.
bool network_car_callback(std::uint32_t cb){return cb==0x4a5f10u||cb==0x4a60d0u||cb==0x4a5b90u||cb==0x4a5c70u;}
// The course collision roots 780100.. in PC layout for translated course queries
// (43ED20 / 43E7E0 / 43D4D0...): the selected course world's tables (views of the
// loaded COLI data) at synthetic bases 0x60000000 + table * 0x1000000 + type * 0x400000.
void map_course_world_pc(PcRaceMemory& m,const CourseWorldRuntime& world){
    const auto w=world.tables();
    static std::array<std::uint8_t,0x140> roots{};roots.fill(0);
    auto cell=[&](std::uint32_t root,std::uint32_t type,std::uint32_t table,const driving::Bytes& b){
        if(!b.size())return;
        const std::uint32_t at=0x60000000u+table*0x1000000u+type*0x400000u;
        if(b.size()>0x400000u)throw std::length_error("course table larger than its synthetic window");
        m.map(at,b.data(),b.size());put32(roots.data()+(root-0x780100u)+type*4u,at);};
    for(std::uint32_t k=0;k<4;++k){
        const auto& c=w.courses[k];
        cell(0x780100u,k,0,c.kinds);cell(0x780110u,k,1,c.polygons);cell(0x780120u,k,2,c.normals);
        cell(0x780140u,k,3,c.runs.header);cell(0x780218u,k,4,c.runs.ranges);cell(0x7801f8u,k,5,c.area_lists);
        if(w.grids_present[k])cell(0x7801e8u,k,6,w.grids[k]);
    }
    roots[0x780190u-0x780100u]=w.courses[0].runs.force_ranges?1u:0u;
    put32(roots.data()+(0x780238u-0x780100u),std::uint32_t(w.courses[0].runs.total_length));
    for(std::uint32_t k=0;k<4;++k)put32(roots.data()+(0x780208u-0x780100u)+k*4u,std::uint32_t(world.length_offset(k)));
    for(const std::uint32_t a:{0x780100u,0x780110u,0x780120u,0x780140u,0x7801e8u,0x7801f8u,0x780208u,0x780218u})
        m.map(a,roots.data()+(a-0x780100u),16);
    m.map(0x780190u,roots.data()+(0x780190u-0x780100u),1);m.map(0x780238u,roots.data()+(0x780238u-0x780100u),4);
}
bool network_car_slot(const driving::PcEventSlot& s){
    return ((s.init_callback==0x4a5f10u&&s.ctrl_callback==0x4a60d0u)||(s.init_callback==0x4a5b90u&&s.ctrl_callback==0x4a5c70u))&&s.dest_callback==0x49a650u;
}
}
bool native_traffic_car_slot(const driving::PcEventSlot& s){
    if(network_car_slot(s))return true;
    // a LAN car whose control the race switched to the traffic driver (47E780, after a leave)
    if((s.init_callback==0x4a5f10u||s.init_callback==0x4a5b90u)&&s.ctrl_callback==0x47e780u&&s.dest_callback==0x49a650u)return true;
    return (s.init_callback==0x4ad280u||s.init_callback==0x4ad310u||s.init_callback==0x4704a0u)&&s.ctrl_callback==0x47e780u&&s.dest_callback==0x470560u;
}
bool native_race_traffic_hook(NativeRuntimeContext& c,std::uint32_t pc,driving::PcMatrixStack& matrices){
    auto& t=native_race_traffic(c);
    switch(pc){
    case 0x47dac0u:++t.inits;t.latched=false;t.fault=0;t.error.clear();
        t.heart_latched=false;t.heart_fault=0;t.heart_error.clear();
        t.score_latched=false;t.score_fault=0;t.score_error.clear();
        t.objects_latched=false;t.object_fault=0;t.object_error.clear();t.object_draws.clear();
        t.state=PcTrafficState{};
        return run_traffic(c,matrices,[](PcRaceContext& ctx){traffic_init_47dac0(ctx);}),true;
    case 0x47ec00u:++t.controls;return run_traffic(c,matrices,[](PcRaceContext& ctx){traffic_control_47ec00(ctx);}),true;
    case 0x47dc00u:++t.destroys;return true;   // 47DC00 destroy: 47D970 [80FB28] = 0 (the race manager binding does it)
    // variant 9 (race_variant_owners): 47D8B0 the racers set up by 47CF40 put on the course, 4B0330
    case 0x47d8b0u:return run_traffic(c,matrices,[](PcRaceContext& ctx){traffic_spawn_47d8b0(ctx);});
    case 0x4b0330u:return run_traffic(c,matrices,[](PcRaceContext& ctx){traffic_mark_4b0330(ctx);});
    default:return false;
    }
}
bool native_race_score(NativeRuntimeContext& c,driving::PcMatrixStack& matrices,std::uint32_t pc,std::uint32_t arg){
    auto& t=native_race_traffic(c);
    if(t.score_latched){++t.score_skipped;return false;}
    ++t.score_runs;
    const auto depth=matrices.depth;
    Run run{c,t,matrices};
    try{
        map_memory(c,t);racers_in(c,t);
        native_race_hud_map_heart(c,t.memory);
        t.memory.map(0x84bcf8u,native_race_end(c).state.miles.data()+(0x84bcf8u-PcRaceEndState::MilesBase),4);   // 4EF460 / 4EF440 miles score
        PcRaceContext ctx{t.memory,matrices,[&](const PcRaceCall& k){return run(k);},nullptr};
        run.ctx=&ctx;
        traffic_control_part(ctx,pc,arg,0);
        racers_out(c,t);ints_out(c,t);
        return true;
    }catch(const TrafficUnported& e){t.score_fault=e.pc;t.score_error=e.what();}
    catch(const PcRaceUnmapped& e){t.score_fault=e.address;t.score_error=std::string("unmapped PC address: ")+e.what();}
    catch(const std::exception& e){t.score_fault=pc;t.score_error=e.what();}
    while(matrices.depth>depth)driving::pc_matrix_pop(matrices);
    t.score_latched=true;
    std::fprintf(stderr,"race score latched: %s (pc %x arg %x, mode %u variant %u, last service %x)\n",t.score_error.c_str(),
        unsigned(pc),unsigned(arg),unsigned(c.mode_state.current),unsigned(c.game_mode.game_variant),unsigned(t.last_routed_pc));
    return false;
}
bool native_race_traffic_heart(NativeRuntimeContext& c,driving::PcMatrixStack& matrices,std::uint32_t pc,NativeHeartBinding& heart){
    auto& t=native_race_traffic(c);
    if(t.heart_latched){++t.heart_skipped;return false;}
    ++t.heart_runs;
    const auto depth=matrices.depth;
    Run run{c,t,matrices};run.heart=&heart;run.renderer=heart.renderer;
    try{
        map_memory(c,t);racers_in(c,t);
        if(heart.map)heart.map(heart.user,t.memory);else outrun::driving::service_hole("native_race_traffic_heart","heart.map");
        if(heart.renderer)t.memory.map(0x899b98u,heart.renderer->environment().lights_899b98.data(),heart.renderer->environment().lights_899b98.size());
        PcRaceContext ctx{t.memory,matrices,[&](const PcRaceCall& k){return run(k);},heart.display?heart.models:nullptr};
        run.ctx=&ctx;
        traffic_control_part(ctx,pc,0,0);
        racers_out(c,t);ints_out(c,t);
        return true;
    }catch(const TrafficUnported& e){t.heart_fault=e.pc;t.heart_error=e.what();}
    catch(const PcRaceUnmapped& e){t.heart_fault=e.address;t.heart_error=std::string("unmapped PC address: ")+e.what();}
    catch(const std::exception& e){t.heart_fault=pc;t.heart_error=e.what();}
    while(matrices.depth>depth)driving::pc_matrix_pop(matrices);
    t.heart_latched=true;
    std::fprintf(stderr,"heart attack latched: %s\n",t.heart_error.c_str());
    if(std::getenv("OR2_HEART_DEBUG"))for(std::uint32_t id=0x10;id<410u;++id){const auto& q=c.event_state.slots[id];
        if(q.flags&3u)std::fprintf(stderr,"  event %x flags %x work %x init %x ctrl %x fn %x\n",unsigned(id),unsigned(q.flags),unsigned(q.work_token),unsigned(q.init_callback),unsigned(q.ctrl_callback),unsigned(q.function_id));}
    return false;
}
// ---- course objects (OSO) ----
namespace {
bool object_control_callback(std::uint32_t cb){
    switch(cb){
    case 0x4a8a60u:case 0x4a8a70u:case 0x4aada0u:case 0x4a9010u:case 0x4a9c50u:case 0x4ab2b0u:case 0x4abcd0u:case 0x4ab160u:
    case 0x4ab760u:case 0x4aadc0u:case 0x4ab9b0u:case 0x4aa560u:case 0x4aa7d0u:case 0x4a8c20u:case 0x4a9c70u:case 0x4a9f40u:
    case 0x4a9f80u:case 0x4a8b50u:case 0x4a8ba0u:case 0x4a8c60u:case 0x4a9c80u:case 0x4aa070u:case 0x4abe40u:case 0x4acd50u:
    case 0x4acbc0u:case 0x4ac790u:case 0x4ab1f0u:case 0x4acb00u:case 0x4ab960u:case 0x4acde0u:case 0x4abf20u:case 0x4aa620u:
    case 0x4aafc0u:case 0x4ac200u:case 0x4ac8c0u:case 0x4aa920u:case 0x4ab2e0u:case 0x4ac440u:case 0x4aef50u:return true;   // membership list, not an answer
    default:return false;
    }
}
bool object_display_callback(std::uint32_t cb){return cb==0x4a8a90u||cb==0x4a9030u||cb==0x4a9df0u||cb==0x4a8d80u;}
template<class F> bool run_objects(NativeRuntimeContext& c,driving::PcMatrixStack& matrices,PcSceneRenderer* renderer,std::vector<PcVehicleDrawCall>* draws,F&& body){
    auto& t=native_race_traffic(c);
    if(t.objects_latched){++t.object_skipped;return false;}
    const auto depth=matrices.depth;
    Run run{c,t,matrices};run.renderer=renderer;
    try{
        map_memory(c,t);racers_in(c,t);
        if(renderer)t.memory.map(0x899b98u,renderer->environment().lights_899b98.data(),renderer->environment().lights_899b98.size());
        PcRaceContext ctx{t.memory,matrices,[&](const PcRaceCall& k){return run(k);},draws};
        run.ctx=&ctx;
        body(ctx);
        racers_out(c,t);ints_out(c,t);
        return true;
    }catch(const TrafficUnported& e){t.object_fault=e.pc;t.object_error=e.what();}
    catch(const PcRaceUnmapped& e){t.object_fault=e.address;t.object_error=std::string("unmapped PC address: ")+e.what();}
    catch(const std::exception& e){t.object_fault=0x4a8a90u;t.object_error=e.what();}
    while(matrices.depth>depth)driving::pc_matrix_pop(matrices);
    t.objects_latched=true;
    std::fprintf(stderr,"course objects latched: %s (mode %u variant %u, last service %x)\n",t.object_error.c_str(),
        unsigned(c.mode_state.current),unsigned(c.game_mode.game_variant),unsigned(t.last_routed_pc));
    return false;
}
}
bool native_race_traffic_object_invoke(NativeRuntimeContext& c,std::uint32_t callback,driving::PcMatrixStack& matrices){
    if(!object_control_callback(callback)||std::getenv("OR2_NO_OBJECTS"))return false;
    auto& t=native_race_traffic(c);
    const std::uint32_t ev=c.event_state.current_slot;
    const std::uint32_t work=ev<410u?c.event_state.slots[ev].work_token:0u;
    if(std::getenv("OR2_OBJECTS_TRACE"))std::fprintf(stderr,"[objects] frame %u event %x callback %x work %x flags %x\n",c.completed_frames,ev,callback,work,ev<410u?unsigned(c.event_state.slots[ev].flags):0u);
    if(callback==0x4aef50u)++t.object_dests;
    else if(ev<410u&&callback==c.event_state.slots[ev].init_callback)++t.object_inits;
    else ++t.object_controls;
    run_objects(c,matrices,nullptr,nullptr,[&](PcRaceContext& ctx){
        if(!traffic_object_callback(ctx,callback,work))throw TrafficUnported(callback,"course object callback");});
    return true;
}
bool native_race_objects_display(NativeRuntimeContext& c,PcSceneRenderer& r,std::uint32_t callback,std::uint32_t work){
    if(!object_display_callback(callback))return false;
    auto& t=native_race_traffic(c);
    if(std::getenv("OR2_OBJECTS_TRACE"))std::fprintf(stderr,"[objects] frame %u display %x work %x\n",c.completed_frames,callback,work);
    std::vector<PcVehicleDrawCall> draws;
    if(!run_objects(c,r.matrices(),&r,&draws,[&](PcRaceContext& ctx){
        if(!traffic_object_callback(ctx,callback,work))throw TrafficUnported(callback,"course object display");}))return true;
    ++t.object_displays;
    try{(void)native_race_draw_list_execute(c,r,draws);}
    catch(const std::exception& e){++t.object_draw_faults;if(t.object_error.empty())t.object_error=std::string("object draw list: ")+e.what();}
    return true;
}
bool native_race_traffic_cs_diff(NativeRuntimeContext& c,driving::PcMatrixStack& matrices,std::uint32_t a,std::uint32_t b,std::int32_t& out){
    auto& t=native_race_traffic(c);
    const auto depth=matrices.depth;
    Run run{c,t,matrices};
    try{
        map_memory(c,t);racers_in(c,t);
        PcRaceContext ctx{t.memory,matrices,[&](const PcRaceCall& k){return run(k);},nullptr};
        run.ctx=&ctx;
        out=std::int32_t(traffic_control_part(ctx,0x46f990u,a,b));
        racers_out(c,t);ints_out(c,t);
        return true;
    }catch(const std::exception&){}
    while(matrices.depth>depth)driving::pc_matrix_pop(matrices);
    return false;
}
// Car services the player car's CommonPlCar (4A8100) shares with the traffic module:
// 479670 othcarGetR. The player car work is mapped at its event-8 token.
bool native_race_traffic_car_service(NativeRuntimeContext& c,std::uint32_t pc,std::uint32_t car,driving::PcMatrixStack& matrices){
    if(pc!=0x479670u||car==0u)return false;
    auto& t=native_race_traffic(c);
    if(t.latched)return false;
    return run_traffic(c,matrices,[&](PcRaceContext& ctx){traffic_control_part(ctx,pc,car,0,0);});
}
// One traffic-module routine on a car outside the race (504E70 / 46E4B0 / 46E740 for the
// OUTRUN2SP car-select car, 475670): a failure is reported, the module is not latched.
bool native_race_traffic_car_call(NativeRuntimeContext& c,std::uint32_t pc,std::uint32_t car,driving::PcMatrixStack& matrices,std::uint32_t& result,std::string& error){
    if(pc!=0x504e70u&&pc!=0x46e4b0u&&pc!=0x46e740u)return false;
    auto& t=native_race_traffic(c);
    const auto depth=matrices.depth;
    Run run{c,t,matrices};
    try{
        map_memory(c,t);racers_in(c,t);
        PcRaceContext ctx{t.memory,matrices,[&](const PcRaceCall& k){return run(k);},nullptr};
        run.ctx=&ctx;
        result=traffic_control_part(ctx,pc,car,0,0);
        racers_out(c,t);ints_out(c,t);
        return true;
    }catch(const std::exception& e){error=e.what();}
    while(matrices.depth>depth)driving::pc_matrix_pop(matrices);
    return false;
}
bool native_race_traffic_ranking(NativeRuntimeContext& c,std::uint32_t car,driving::PcMatrixStack& matrices){
    auto& t=native_race_traffic(c);
    if(t.latched)return false;
    ++t.rankings;
    return run_traffic(c,matrices,[&](PcRaceContext& ctx){traffic_control_part(ctx,0x476760u,car,0,0);});
}
namespace {
// The translated LAN car code (race_network_cars_tr.cpp) over the traffic module's memory,
// with the network module's blocks (remote players' states) and the course roots mapped.
template<class F> void network_cars_call(NativeRuntimeContext& c,driving::PcMatrixStack& matrices,PcRaceContext& ctx,F&& body){
    native_network_map_shared(ctx.m);
    auto& w=c.race.car_world;   // CommRace 7DE418.. (7DF334 history indices) and the car request list 841AC0 (4A5830)
    if(!ctx.m.mapped(0x7de418u,4))ctx.m.map(0x7de418u,w.commrace_7de418.data(),w.commrace_7de418.size());
    if(!ctx.m.mapped(0x7dd138u,1))ctx.m.map(0x7dd138u,&w.slot_7dd138,1);
    ctx.m.map(0x7f1938u,reinterpret_cast<std::uint8_t*>(&w.clock_7f1938),4);   // over the NAVI block copy
    map_course_world_pc(ctx.m,c.start_mode.scene_owner_course_world);
    if(!ctx.m.mapped(0x83db30u,4))ctx.m.map(0x83db30u,w.ghost_83db30.data(),w.ghost_83db30.size());
    if(!ctx.m.mapped(0x83036du,1))ctx.m.map(0x83036du,&c.start_mode.vehicle_variant_83036d,1);
    if(!ctx.m.mapped(0x841bc8u,4))ctx.m.map(0x841ac0u,c.vehicle_creation.object.data(),0x110u);   // records + 841BC8 count (841BD0.. is car_world.vibrate)
    const PcRaceService service=[&](const PcRaceCall& k)->std::uint32_t{
        switch(k.pc){
        case 0x504e70u:case 0x46e4b0u:return traffic_control_part(ctx,k.pc,k.args[0],k.args[1],k.args[2]);
        case 0x580f92u:return 0u;                     // printf (debug output)
        case 0x44c2c0u:return ctx.m.u32(0x7d30acu);   // mov eax,[7D30AC]
        default:return ctx.service(k);
        }};
    const TranslatedModule module{network_cars_functions,nullptr,{},nullptr,&matrices,
        &c.event_function36.pc_crt_random_state,network_cars_code_data};
    body(service,module);
}
}
bool native_race_network_call(NativeRuntimeContext& c,driving::PcMatrixStack& matrices,std::uint32_t pc,
                              std::initializer_list<std::uint32_t> args,std::uint32_t& eax,
                              std::uint32_t ecx,std::uint32_t* scratch){
    auto& t=native_race_traffic(c);
    if(t.latched)return false;
    return run_traffic(c,matrices,[&](PcRaceContext& ctx){
        network_cars_call(c,matrices,ctx,[&](const PcRaceService& service,const TranslatedModule& module){
            if(scratch)ctx.m.map(NetworkCallScratch,reinterpret_cast<std::uint8_t*>(scratch),4);
            const std::vector<std::uint32_t> a(args);
            if(traffic_network_call(ctx,pc,ecx,a.data(),a.size(),eax))return;
            eax=translated_call(ctx.m,service,module,pc,ecx,args);});});
}
namespace {
// 4829D0 (46C240, ECX = 7F9460): the request script of the current stage. +A8 >= 3: already
// loaded. +168 == -1: the stage key +13C = 44C830 (the selected record 7D30A8 +4; 0xF without a
// selection: the record 7D2DE0) and its course id +140 = [descriptor +14] +0; else +13C = +168
// and +16C = 44C940(+168) - 1. Then 4F12A0("\Scripts\bin\" 64EF38[+140] ".bin", +A0, 1, 1); once
// read: +170 += 1, +168 = -1, +100[+170] = +140, +5810 = +70 when +68, 482820, +170 -= 1 when +148.
// The records' descriptor words are the native course tables' tokens (course_descriptor_tables_r078).
// A model resource (448AD0 id, 7): the AREA module's resident set, the file 633558[id] read from
// the retail tree; 448960: resident.
bool requests_model_request(NativeRuntimeContext& c,std::uint32_t id){
    if(id>=0x223u)return false;
    std::string path;const std::uint32_t token=exe_image_u32(0x633558u+id*4u);
    for(std::uint32_t i=0;token&&i<200u;++i){const char ch=char(*exe_image_bytes(token+i,1));if(!ch)break;path.push_back(ch);}
    auto* retail=c.event_function36.retail_assets;std::string error;
    if(!retail||path.empty()||!retail_asset_guest_path(*retail,path,true,&error))return false;
    auto& area=native_race_area(c);area.requested.insert(id);area.released.erase(id);return true;
}
// 481590 (46C4D0, ECX = 7F9460): +E8 > 1 -> 1; state 0 requests (448AD0 mode 7) the 64F090
// resources whose +B0 flag is 1, state 1, 0; state 1 -> 2 and 1 once they are all resident.
std::uint32_t requests_resources_481590(NativeRuntimeContext& c,PcRaceMemory& m){
    constexpr std::uint32_t B=NativeRaceTraffic::RequestsBase;
    const std::int32_t st=m.i32(B+0xe8u);
    if(st>1)return 1u;
    if(st==0){
        for(std::uint32_t k=0;k<14u;++k)if(m.u32(B+0xb0u+k*4u)==1u&&!requests_model_request(c,m.u32(0x64f090u+k*4u)))
            throw TrafficUnported(0x448ad0u,"request object model unavailable");
        m.put32(B+0xe8u,1u);return 0u;
    }
    if(st==1){
        auto& area=native_race_area(c);bool ready=true;
        for(std::uint32_t k=0;k<14u;++k)if(m.u32(B+0xb0u+k*4u)==1u&&!area.requested.count(m.u32(0x64f090u+k*4u)))ready=false;
        if(ready){m.put32(B+0xe8u,2u);return 1u;}
    }
    return 0u;
}
std::uint32_t requests_stage_script_4829d0(NativeRuntimeContext& c,PcRaceContext& ctx,driving::PcMatrixStack& matrices){
    auto& m=ctx.m;constexpr std::uint32_t B=NativeRaceTraffic::RequestsBase;
    auto service=[&](std::uint32_t pc,std::initializer_list<std::uint32_t> args){
        PcRaceCall k{};k.pc=pc;for(auto v:args)k.args[k.argc++]=v;return ctx.service(k);};
    if(m.i32(B+0xa8u)>=3)return 1u;
    const auto& rt=c.game_mode.course_runtime;
    auto course_id=[&](const std::array<std::uint8_t,driving::PcCourseRecord44d720Size>& record)->std::uint32_t{
        std::uint32_t token;std::memcpy(&token,record.data()+0x14u,4);
        if(!c.game_mode.course_assets)throw TrafficUnported(0x4829d0u,"no course descriptors");
        const auto tables=driving::course_descriptor_tables_r078(c.game_mode.course_assets->descriptors);
        for(std::size_t i=0;i<tables.primary_count;++i)if(tables.primary[i].token==token&&tables.primary[i].size>=4u){
            std::uint32_t id;std::memcpy(&id,tables.primary[i].data,4);return id;}
        throw TrafficUnported(0x4829d0u,"stage descriptor token not in the course tables");};
    if(m.u32(B+0x168u)==0xffffffffu){
        std::uint32_t key=0xfu;if(rt.selected_copy_active)std::memcpy(&key,rt.selected_7d30a8.data()+4u,4);   // 44C830
        m.put32(B+0x13cu,key);
        if(key==0xfu){std::uint32_t k2;std::memcpy(&k2,rt.selected_7d2de0.data()+4u,4);m.put32(B+0x13cu,k2);m.put32(B+0x140u,course_id(rt.selected_7d2de0));}
        else m.put32(B+0x140u,course_id(rt.selected_7d30a8));
    }else{
        const std::uint32_t v=m.u32(B+0x168u);
        m.put32(B+0x13cu,v);m.put32(B+0x16cu,service(0x44c940u,{v})-1u);
    }
    const std::uint32_t id=m.u32(B+0x140u);
    if(id>=0x42u)throw TrafficUnported(0x4829d0u,"course id outside the 64EF38 script names");
    const std::uint32_t name=m.u32(0x64ef38u+id*4u);
    std::string path="\\Scripts\\bin\\"+guest_string(m,name)+".bin";                  // 5802DD(buf, 5A4310, name)
    constexpr std::uint32_t Scratch=NativeRaceTraffic::RequestsHeapBase+NativeRaceTraffic::RequestsHeapSize-0x100u;
    guest_put_string(m,Scratch,path);
    if(!service(0x4f12a0u,{Scratch,B+0xa0u,1u,1u}))return 0u;
    const std::uint32_t n=m.u32(B+0x170u)+1u;
    m.put32(B+0x168u,0xffffffffu);m.put32(B+0x170u,n);m.put32(B+0x100u+n*4u,m.u32(B+0x140u));
    if(m.u32(B+0x68u))m.put32(B+0x5810u,m.u32(B+0x70u));
    {PcRaceCall k{};k.pc=0x482820u;k.ecx=B;(void)bulk_call(m,ctx.service,k,&matrices,&c.event_function36.pc_crt_random_state);}
    if(m.u32(B+0x148u))m.put32(B+0x170u,m.u32(B+0x170u)-1u);
    return 1u;
}
}
// The C2C request manager 7F9460 (variant 5): one of its entries (46C2xx..46C5xx, the manager
// methods 481xxx / 482xxx / 4Fxxxx / 50xxxx with ECX = 7F9460) over the traffic module's memory
// (its block and heap mapped there). The translated originals until the native port.
bool native_race_requests_call(NativeRuntimeContext& c,driving::PcMatrixStack& matrices,std::uint32_t pc,
                               std::initializer_list<std::uint32_t> args,std::uint32_t& eax,std::uint32_t ecx){
    auto& t=native_race_traffic(c);
    if(t.latched)return false;
    bool done=false;
    const bool ok=run_traffic(c,matrices,[&](PcRaceContext& ctx){
        if(pc==0x46c240u){eax=requests_stage_script_4829d0(c,ctx,matrices);done=true;return;}
        if(pc==0x46c4d0u){eax=requests_resources_481590(c,ctx.m);done=true;return;}
        PcRaceCall k{};k.pc=pc;k.ecx=ecx;k.argc=std::uint32_t(args.size());
        unsigned i=0;for(auto a:args)k.args.at(i++)=a;
        // 46C2xx..46C4xx wrappers outside the bulk translation: their method (ECX = 7F9460)
        static constexpr std::pair<std::uint32_t,std::uint32_t> Methods[]{
            {0x46c200u,0x482140u},{0x46c210u,0x481810u},{0x46c220u,0x481a40u},{0x46c230u,0x482cf0u},
            {0x46c240u,0x4829d0u},{0x46c250u,0x481bc0u},{0x46c260u,0x481490u},{0x46c270u,0x481500u},
            {0x46c2b0u,0x481640u},{0x46c2c0u,0x482af0u},{0x46c380u,0x481650u},{0x46c4c0u,0x481540u},
            {0x46c4d0u,0x481590u},{0x46c440u,0x481760u}};
        if(!bulk_translated(k.pc))for(const auto& [w,method]:Methods)if(w==k.pc){k.pc=method;k.ecx=NativeRaceTraffic::RequestsBase;break;}
        if(k.pc==0x46c1f0u&&!bulk_translated(k.pc)){k.pc=0x481250u;k.ecx=NativeRaceTraffic::RequestsBase;k.argc=1;k.args[0]=0u;}
        if(!bulk_translated(k.pc))throw TrafficUnported(pc,"request manager entry neither native nor translated");
        eax=bulk_call(ctx.m,ctx.service,k,&matrices,&c.event_function36.pc_crt_random_state);done=true;});
    return ok&&done;
}
bool native_race_requests_event(NativeRuntimeContext& c,std::uint32_t callback,std::uint32_t work,driving::PcMatrixStack& matrices){
    if(callback!=0x46c1f0u&&callback!=0x46c200u&&callback!=0x46c210u&&callback!=0x46c230u)return false;
    std::uint32_t eax{};
    (void)native_race_requests_call(c,matrices,callback,{work},eax,0u);   // a fault latches the traffic module (reported)
    return true;
}
bool native_race_network_grid_4a6ed0(NativeRuntimeContext& c,driving::PcMatrixStack& matrices,std::uint32_t car){
    // 4A6F50..4A6FCB (GamePlCar init, variant 4): the grid slot 5C2500[456E00(+1054)] when
    // 4963B0, else slot 0; X mirrored on presets 0 / 2; then 43F730(+14, +5C, [+5C]).
    return run_traffic(c,matrices,[&](PcRaceContext& ctx){
        network_cars_call(c,matrices,ctx,[&](const PcRaceService& service,const TranslatedModule& module){
            auto& m=ctx.m;
            std::uint32_t slot=0;
            if(service(PcRaceCall{0x4963b0u}))slot=m.u32(0x7de478u+m.u32(car+0x1054u)*0x6cu);   // 456E00
            for(std::uint32_t k=0;k<12u;k+=4)m.put32(car+0x14u+k,m.u32(0x5c2500u+slot*12u+k));
            const auto preset=c.start_mode.course_preset;
            if(preset==0u||preset==2u)m.putf(car+0x14u,0.0f-m.f32(car+0x14u));
            const std::uint32_t arg[3]{car+0x14u,car+0x5cu,m.u32(car+0x5cu)};std::uint32_t r=0;
            (void)traffic_network_call(ctx,0x43f730u,0,arg,3,r);});});
}
bool native_race_traffic_car_invoke(NativeRuntimeContext& c,std::uint32_t callback,driving::PcMatrixStack& matrices){
    if(network_car_callback(callback)){
        // The LAN cars through the translated original (race_network_cars_tr.cpp) over the
        // traffic module's memory; the remote players' states are the network module's.
        const std::uint32_t event=c.event_state.current_slot;
        if(event>=c.event_state.slots.size()||!network_car_slot(c.event_state.slots[event]))return false;
        const std::uint32_t work=c.event_state.slots[event].work_token;
        auto& t=native_race_traffic(c);
        if(callback==0x4a5f10u||callback==0x4a5b90u)++t.car_inits;else ++t.car_controls;
        run_traffic(c,matrices,[&](PcRaceContext& ctx){
            network_cars_call(c,matrices,ctx,[&](const PcRaceService& service,const TranslatedModule& module){
                const std::uint32_t arg[1]{work};std::uint32_t r=0;
                if(std::getenv("OR2_NET_DIFF")){   // debug: the translated original against the native port
                    const std::pair<std::uint32_t,std::uint32_t> spans[]{{work,0x10f0u},{0x7dd140u,0x7de418u-0x7dd140u},{0x7df334u,0x20u},{0x7de480u,0x6cu*8u},{0x841ac0u,0x110u}};
                    auto snap=[&]{std::vector<std::uint8_t> v;for(auto [a,n]:spans){const auto* p=ctx.m.at(a,n);v.insert(v.end(),p,p+n);}return v;};
                    auto load=[&](const std::vector<std::uint8_t>& v){std::size_t o=0;for(auto [a,n]:spans){std::memcpy(ctx.m.at(a,n,true),v.data()+o,n);o+=n;}};
                    const auto s0=snap();
                    (void)translated_call(ctx.m,service,module,callback,0,{work});
                    const auto s1=snap();load(s0);
                    (void)traffic_network_call(ctx,callback,0,arg,1,r);
                    const auto s2=snap();
                    std::size_t o=0;unsigned shown=0;
                    for(auto [a,n]:spans){for(std::uint32_t k=0;k<n;++k)if(s1[o+k]!=s2[o+k]&&shown<12){
                        if(!shown)std::fprintf(stderr,"[netdiff] %06X frame %u:",callback,ctx.m.u32(0x7f1938u));
                        std::fprintf(stderr," %08X:%02X/%02X",a+k,s1[o+k],s2[o+k]);++shown;}o+=n;}
                    if(shown)std::fprintf(stderr,"\n");
                }else if(!traffic_network_call(ctx,callback,0,arg,1,r))(void)translated_call(ctx.m,service,module,callback,0,{work});
                if(std::getenv("OR2_NET_DEBUG")&&(t.car_controls%60u)==1u){
                    const std::uint32_t sl=ctx.m.u32(work+0x1054u);
                    std::fprintf(stderr,"[netcar] clock %u index %u hist",ctx.m.u32(0x7f1938u),ctx.m.u32(0x7df334u+sl*4u));
                    for(std::uint32_t k=0;k<6;++k)std::fprintf(stderr," %d",ctx.m.i32(0x7ee2acu+sl*0x900u+k*0x90u));
                    std::fprintf(stderr," | 7DD140 %08X\n",ctx.m.u32(0x7dd140u+sl*0x30cu));}
                if(std::getenv("OR2_NET_DEBUG")&&(t.car_controls%60u)==1u)
                    std::fprintf(stderr,"[netcar] %06X work %08X slot %u pos %.2f %.2f %.2f speed %.3f\n",callback,work,ctx.m.u32(work+0x1054u),
                        double(ctx.m.f32(work+0x14u)),double(ctx.m.f32(work+0x18u)),double(ctx.m.f32(work+0x1cu)),double(ctx.m.f32(work+0x1c4u)));});});
        return true;
    }
    if(callback!=0x4ad280u&&callback!=0x4ad310u&&callback!=0x4704a0u&&callback!=0x47e780u)return false;
    const std::uint32_t event=c.event_state.current_slot;
    if(event>=c.event_state.slots.size()||!native_traffic_car_slot(c.event_state.slots[event]))return false;
    const std::uint32_t work=c.event_state.slots[event].work_token;
    auto& t=native_race_traffic(c);
    if(callback!=0x47e780u){++t.car_inits;run_traffic(c,matrices,[&](PcRaceContext& ctx){traffic_car_init(ctx,callback,work);});}
    else{++t.car_controls;run_traffic(c,matrices,[&](PcRaceContext& ctx){traffic_car_control_47e780(ctx,work);});}
    return true;
}
std::string native_race_traffic_status(const NativeRuntimeContext& c){
    std::ostringstream o;
    if(!c.race_traffic){o<<"race traffic: not started";return o.str();}
    const auto& t=*c.race_traffic;
    o<<"race traffic inits="<<t.inits<<" controls="<<t.controls<<" destroys="<<t.destroys<<" cars: init="<<t.car_inits
     <<" ctrl="<<t.car_controls<<" dest="<<t.car_destroys<<" rankings="<<t.rankings<<" skipped="<<t.skipped<<" texture_swaps_not_applied="<<t.skipped_texture_swaps<<" latched="<<t.latched;
    if(t.latched)o<<" fault="<<hex(t.fault)<<" ("<<t.error<<")";
    if(t.object_inits||t.object_controls||t.object_displays||t.objects_latched){
        o<<" objects init="<<t.object_inits<<" ctrl="<<t.object_controls<<" disp="<<t.object_displays<<" dest="<<t.object_dests<<" skipped="<<t.object_skipped
         <<" draw_faults="<<t.object_draw_faults<<" sprites_pending="<<t.object_draws.size();
        if(t.objects_latched)o<<" OBJECTS LATCHED fault="<<hex(t.object_fault)<<" ("<<t.object_error<<")";}
    if(t.heart_runs||t.heart_latched){o<<" heart runs="<<t.heart_runs<<" skipped="<<t.heart_skipped<<" mode7f2428="<<c.race.car_world.heart_mode_7f2428;
        if(t.heart_latched)o<<" HEART LATCHED fault="<<hex(t.heart_fault)<<" ("<<t.heart_error<<")";}
    if(t.score_runs||t.score_latched){o<<" score runs="<<t.score_runs<<" skipped="<<t.score_skipped;
        if(t.score_latched)o<<" SCORE LATCHED fault="<<hex(t.score_fault)<<" ("<<t.score_error<<")";}
    o<<" routed:";for(const auto& [pc,n]:t.routed)o<<' '<<hex(pc)<<'x'<<n;
    o<<" missing:";for(const auto& [pc,n]:t.missing)o<<' '<<hex(pc)<<'x'<<n;
    return o.str();
}
}
