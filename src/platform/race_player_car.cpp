#include "enhancements/frame_rate.hpp"
#include "platform/race_player_car.hpp"
#include "system/dev_hooks.hpp"
#include "platform/race_ghosts_runtime.hpp"
#include "platform/frontend_profiles.hpp"
#include "platform/vehicle_constructor_data.hpp"
#include "driving/pc_car_services.hpp"
#include "platform/native_runtime.hpp"
#include "platform/pc_address_view.hpp"
#include "driving/pc_driving_control.hpp"
#include "driving/pc_wheel_dynamics.hpp"
#include "driving/pc_chassis.hpp"
#include "driving/pc_suspension.hpp"
#include "driving/pc_contact.hpp"
#include "driving/pc_coli_car.hpp"
#include "driving/pc_wrecker.hpp"
#include "driving/pc_camera.hpp"
#include "driving/pc_crash_entry.hpp"
#include "driving/pc_body_wall.hpp"
#include "driving/pc_wall_controller.hpp"
#include "platform/course_world_runtime.hpp"
#include "platform/race_goal_camera.hpp"
#include "platform/pc_network.hpp"
#include "platform/race_end_runtime.hpp"
#include "platform/race_robots.hpp"
#include "driving/pc_course_query.hpp"
#include "driving/pc_ground_collision.hpp"
#include "driving/pc_collision.hpp"
#include "platform/race_traffic_runtime.hpp"
#include "platform/race_area_runtime.hpp"
#include <cstring>
#include <cstdio>
#include <stdexcept>
namespace outrun::platform {
bool native_mission_type4_4962a0(const NativeRuntimeContext&);   // native_runtime.cpp (4962A0)
namespace {
using driving::Bytes;
using driving::CourseProbe;
// Running resistance constants 6AE4E4..6AE4F8 (.data, no writer).
constexpr driving::RunningResistanceTuning RaceRunningResistance{0.0225f,0.2f,1.0f,0.9f,0.175f,0.175f};
struct Frame {
    NativeRuntimeContext& c;
    RaceCarWorld& w;
    driving::PcMatrixStack& m;
    Bytes e,W,P;
    std::array<Bytes,4> wheels;
    driving::CourseWorldTables tables;
    std::array<driving::PcCourseEndView,4> ends{{{std::nullopt,Bytes(nullptr,0)},{std::nullopt,Bytes(nullptr,0)},{std::nullopt,Bytes(nullptr,0)},{std::nullopt,Bytes(nullptr,0)}}};
    std::vector<Bytes> descriptors;
    std::array<std::uint8_t,4> fallback_descriptor{};
    driving::PcStageViews stages{Bytes(fallback_descriptor.data(),4),0,{},Bytes(fallback_descriptor.data(),4)};
    std::array<std::uint8_t,0x78> selection{};
    // GamePlCar parent call-site state.
    std::uint32_t translation_calls{};
    CourseProbe local{};
    std::array<std::uint8_t,0x900> snapshot{};
    bool snapshot_valid{};
    std::uint32_t stage_level{};
    driving::PcGamePlCarParentInputs parent{};
    driving::PcGamePlCarParentState parent_state{};
    bool failed{};
    void miss(std::uint32_t pc){++w.missing[pc];}
    void fail(std::uint32_t pc,const char* why){if(!failed){failed=true;w.last_error_pc=pc;w.last_error=why;}}
    Bytes stage_cache(){return Bytes(reinterpret_cast<std::uint8_t*>(&c.game_mode.course_runtime.stage_key_635f2c),8);}
    std::array<float,2> area_yaw()const{return {c.game_mode.course_runtime.zero_7d3124[1],c.game_mode.course_runtime.zero_7d3178[1]};}
    std::int32_t game_mode()const{return std::int32_t(c.mode_state.current);}
    std::int32_t variant()const{return std::int32_t(c.game_mode.game_variant);}
    std::int32_t volume(unsigned ch)const{return w.analog_7d6810.current[ch];}                // 453720
    std::int32_t old_volume(unsigned ch)const{return w.analog_7d6810.previous[ch];}           // 453750
    std::uint32_t pressed(std::uint32_t mask)const{return w.switch_7d6770.pressed&mask;}      // 4536F0
    driving::PcRouteContext route(){
        return {native_race_route_words(c),Bytes(selection.data(),selection.size()),   // race manager 7D39A0..
            Bytes(w.commrace_7de418.data(),w.commrace_7de418.size()),w.slot_7dd138,std::uint32_t(variant()),w.clock_7f1938,
            {std::uint8_t(c.start_mode.selection_active_836374?1u:0u),c.start_mode.flag_830394,w.gate_8361b4}};
    }
    std::uint32_t stage_property(std::uint32_t key){return driving::pc_stage_property(stages,key,stage_cache());} // 44C940
    std::uint16_t course_end(unsigned type){return driving::pc_course_end_position(ends[type]);}           // 43D470
    // 780170[type] = COLI0200 header + [header+24] (one surface byte per polygon);
    // 780160[type] = header + [header+30] (lane rates, 4 bytes per selector).
    Bytes coli_section(unsigned type,std::size_t field,std::size_t element){
        auto& world=c.start_mode.scene_owner_course_world;
        if(!world.lane_loaded(type))return Bytes(nullptr,0);
        const auto& pack=world.lane(type);
        if(pack.pc_coli0200.size()<0x40)return Bytes(nullptr,0);
        const Bytes h(const_cast<std::uint8_t*>(pack.pc_coli0200.data()),pack.pc_coli0200.size());
        const std::size_t off=h.u32(field);
        const std::size_t n=std::size_t(pack.pc_layout.polygon_count)*element;
        if(off>h.size()||n>h.size()-off)return Bytes(nullptr,0);
        return h.sub(off,n);
    }
    Bytes surface_codes(unsigned type){return coli_section(type,0x24,1);}
};
// EXE data tables of the wall/crash paths (embedded ranges, read-only).
struct ExeTables {
    PcAddressView exe;
    Bytes view(std::uint32_t address,std::size_t size)const{
        const auto* p=exe.at(address,size);
        if(!p)throw std::out_of_range("EXE table outside the embedded ranges");
        return Bytes(const_cast<std::uint8_t*>(p),size);
    }
    driving::PcCrashTables crash;
    ExeTables():crash{Bytes(nullptr,0),Bytes(nullptr,0),{}}{
        exe.add_exe();
        crash.primary=view(0x5e08e8u,20u*8u);crash.recovery=view(0x5e0988u,20u*12u);
        for(unsigned k=0;k<20u;++k){
            const auto pose=crash.primary.u32(k*8u);
            driving::PcCrashPose p{{{0,Bytes(nullptr,0)},{0,Bytes(nullptr,0)},{0,Bytes(nullptr,0)},{0,Bytes(nullptr,0)},{0,Bytes(nullptr,0)},{0,Bytes(nullptr,0)}}};
            if(pose!=0u)for(unsigned ch=0;ch<6u;++ch){
                const Bytes c=view(pose+ch*8u,8);const auto count=c.i16(0);
                p[ch]=driving::PcCrashChannel{count,count>0?view(c.u32(4),std::size_t(count)*16u):Bytes(nullptr,0)};
            }
            crash.poses.push_back(p);
        }
    }
};
const ExeTables& exe_tables(){static const ExeTables t;return t;}
// 5E0EE0: eight pointers to u32[state] sound command lists.
std::array<Bytes,8> material_commands(const ExeTables& x){
    auto at=[&](unsigned k){std::uint32_t p{};x.exe.u32(0x5e0ee0u+k*4u,p);return x.view(p,16u*4u);};
    return {at(0),at(1),at(2),at(3),at(4),at(5),at(6),at(7)};
}
// 4963E0: 780258 == 4 and [[83637C]+10] != 0 (the variant-4 manager state).
bool race_ready_4963e0(const NativeRuntimeContext& c){
    if(c.game_mode.game_variant!=4u)return false;
    const auto& m=c.mission;const auto* races=c.start_mode.scene_owner_race_assets;
    if(m.manager.network_record_83637c){   // LAN race: [83637C] = the 84A9B0 settings block (+10 = 84A9C0)
        if(!c.race_end)return false;
        const auto& block=c.race_end->state.arcade_84a208;const std::uint32_t at=m.manager.network_record_83637c+0x10u-0x84a208u;
        if(at+4u>block.size())throw std::logic_error("4963E0 network record outside 84A208..");
        std::uint32_t v;std::memcpy(&v,block.data()+at,4);return v!=0u;}
    if(m.manager.record_83637c<0||!m.view_ready||!races)return false;
    const auto record=NativeRacesBlobBase+std::uint32_t(races->races_offset)+std::uint32_t(m.manager.record_83637c)*0x44u;
    std::uint32_t v{};if(!m.view.u32(record+0x10u,v))throw std::logic_error("4963E0 record outside the Races view");
    return v!=0u;
}
// 456BB0 (ECX = 7DE418): the highest course position +5A among the CommRace
// entries whose session player ([+0] -> u16 id == +4) is racing (state +8 = 3 / 4) and
// whose last update (+54) is younger than 0xFF frames of 7F1938 (age 0 before the network
// start 4F53B0); 0 without entries. The players live in the network module's memory.
std::uint16_t reference_position_456bb0(const RaceCarWorld& w){
    const Bytes b(const_cast<std::uint8_t*>(w.commrace_7de418.data()),w.commrace_7de418.size());
    const std::uint32_t n=b.u32(0);
    std::uint16_t best=0;
    const bool started=native_network_u8(0x850b04u)!=0u;
    for(std::uint32_t i=0;i<n;++i){
        const std::uint32_t e=4u+i*0x6cu;
        const std::int32_t age=started?std::int32_t(w.clock_7f1938-b.u32(e+0x54u)):0;
        const std::uint32_t p=b.u32(e);
        if(std::uint32_t(native_network_u8(p)|native_network_u8(p+1u)<<8)!=b.u32(e+4u))continue;
        const std::uint32_t state=native_network_u32(p+8u);
        if(state!=3u&&state!=4u)continue;
        if(age>=0xff)continue;
        const std::uint16_t v=std::uint16_t(b.i16(e+0x5au));
        if(v>best)best=v;
    }
    return best;
}
// Stage records 7D33BC: the course set records of the selected category,
// counted by 7D33C4; +14 points at the stage descriptor (EXE data).
void bind_stages(Frame& f){
    auto& rt=f.c.game_mode.course_runtime;
    const std::int32_t count=rt.active_count_7d33c4;
    // 7D33BC: the table 44D720 walked (mission direct records or 44DA00 file).
    std::uint8_t* data=rt.records_fallback?rt.fallback_record_7d33d8.data():rt.records_7d33bc;
    const std::size_t size=rt.records_fallback?rt.fallback_record_7d33d8.size():(data?rt.records_bytes:0u);
    const Bytes records(data,size);
    f.descriptors.clear();
    PcAddressView exe;exe.add_exe();
    for(std::int32_t k=0;k<count&&std::size_t(k+1)*0x78u<=size;++k){
        const auto token=records.u32(std::size_t(k)*0x78u+0x14u);
        const auto* p=exe.at(token,4);
        f.descriptors.push_back(p?Bytes(const_cast<std::uint8_t*>(p),4):Bytes(f.fallback_descriptor.data(),4));
    }
    std::memcpy(f.selection.data(),rt.selected_7d30a8.data(),f.selection.size());
    f.stages=driving::PcStageViews{records,count,f.descriptors,Bytes(f.fallback_descriptor.data(),4)};
}
void bind_ends(Frame& f){
    for(unsigned t=0;t<4;++t){
        f.ends[t]=driving::PcCourseEndView{std::nullopt,f.tables.courses[t].runs.lengths};
        if(f.tables.courses[t].runs.present)f.ends[t].header=f.tables.courses[t].runs.header;
    }
}
// ---- CommonPlCar 0x4A8100 children -------------------------------------
void common_service(void* user,std::uint32_t pc){
    auto& f=*static_cast<Frame*>(user);
    if(f.failed)return;
    auto& c=f.c;auto& w=f.w;auto& m=f.m;Bytes e=f.e,W=f.W,P=f.P;
    driving::CourseWorldQuery query{f.tables,m,c.event_function36.car_select.race_prediction};
    driving::PcRoadInfoContext road{f.tables,m,f.area_yaw()};
    auto route=f.route();
    driving::PcCourseAdvanceContext advance{f.ends,f.stages,f.stage_cache(),route,0u,0u,false};
    try{
    switch(pc){
    case 0x0049b2d0u:return;                                                     // timer: parent input
    case 0x004a4010u:{                                                          // GetRoadOfs (race_traffic_runtime, shared with the traffic cars)
        std::string error;
        try{if(!native_get_road_ofs_4a4010(c,native_race_area_memory(c,nullptr,false),e,m,w.gate_80fb14!=0u,error))f.miss(0x479d90u);}
        catch(const std::exception& x){f.miss(0x4a4010u);w.last_error_pc=0x4a4010u;w.last_error=x.what();}
        return;}
    case 0x004a61f0u:driving::make_force_work(e,W,P,f.wheels,m);return;
    case 0x004a0000u:driving::make_force_work_tire_4a0000(e,W,P,f.wheels,m);return;
    case 0x00517410u:driving::action_force2(W.sub(0xf0,0x1f0),e.f32(0xdbc),m);return;
    case 0x004a2fa0u:driving::rear_grip_ctrl(e,W,P,{f.volume(1),f.volume(2),f.old_volume(1),f.old_volume(2)});return;
    case 0x004a3310u:driving::slip_angle_ctrl(e,W,P,{c.start_mode.manager_state_7f94c0,w.slip_assist_800aac});return;
    case 0x004a3950u:driving::cornering_ctrl(e,W,P,m);return;
    case 0x004a7ec0u:{
        advance.transition_allowed=c.race.area_state_7d2e80>9u;                 // 44BE50(0)
        driving::PcAssCompulsiveMoveContext ass{advance,road,c.race.area_state_7d2e80>9u,c.race.area_state_7d2e88==20u};
        driving::PcDispMatrixContext display{m,1.f,c.race.camera_override.scene_82e7d4};
        driving::PcPlWreckerContext wrecker{advance,road,query,display};
        driving::PcCalcPlBody2Context ctx{ass,road,f.stages,wrecker};
        driving::calc_pl_body_2nd_4a7ec0(e,W,P,f.wheels,W.sub(0x258,4*0xf4),ctx);return;}
    case 0x00519830u:{                                                          // ColiCar
        const auto& x=exe_tables();
        driving::GroundCollisionContext ground{query,f.area_yaw()};
        driving::PcBkQueryContext bk{};bk.game_mode=std::uint32_t(f.variant());
        bk.branch_record=native_race_route_451350(c,std::int32_t(f.stage_property(e.u32(0x68))));   // 451350(44C940(450380(8))) on the race manager
        bk.mode4_course_gate=e.u32(0x5c);bk.gate_4957f0=c.start_mode.selection_active_836374;bk.gate_48b310=c.start_mode.flag_830394!=0u;
        bk.gate_495490=w.gate_8361b4!=0u;
        driving::PcDispMatrixContext display{m,1.f,c.race.camera_override.scene_82e7d4};
        driving::PcPlWreckerContext wrecker{advance,road,query,display};
        driving::PcCrashWreckerContext crash_wrecker{W,P,W.sub(0x258,4*0xf4),&wrecker};
        driving::PcWallControllerContext wall{f.tables,{f.surface_codes(0),f.surface_codes(1),f.surface_codes(2),f.surface_codes(3)},x.view(0x5e0de0u,16u),
            driving::WallReboundContext{driving::WallResponseContext{f.stages,x.view(0x5e0f20u,0x5e1fa0u-0x5e0f20u),x.view(0x5e1fa0u,0x5e302cu-0x5e1fa0u),
                w.commrace_7de418[0],w.rotation_5e302c,w.rotation_5e3030},f.stage_cache(),route,
                driving::PcSoundQueue{Bytes(w.sound_entries_9563e8.data(),128),Bytes(w.sound_state.data(),8),std::uint8_t(c.event_state.slots[0x17fu].flags)}},
            driving::PcCrashEntryContext{x.crash,f.stages,x.view(0x5c2570u,0x5c2f80u-0x5c2570u),
                driving::PcImpactFeedback{x.view(0x5b3a30u,12u),x.view(0x5b3a3cu,20u*4u),w.gate_80fb14!=0u},&crash_wrecker},
            driving::PcCrushSelection{-170.0f,-130.0f,216.720001220703125f,-20.0f,-33.0f,0x1.657186p-3f,x.view(0x5e0df0u,16u*4u)},
            driving::PcMaterialSounds{x.view(0x5e0f00u,32u),material_commands(x)},f.ends,std::uint32_t(f.game_mode()),std::uint32_t(f.variant())};
        driving::PcBodyWallContext body{query,bk,wall,P,W.sub(0x258,4*0xf4),wrecker};
        driving::PcColiCarContext ctx{ground,body,P,f.wheels};
        driving::coli_car(e,W,ctx);
        w.rotation_5e302c=wall.rebound.response.rotation_odd;w.rotation_5e3030=wall.rebound.response.rotation_even;
        return;}
    case 0x004a1b70u:driving::suspension_force(W,P);return;
    case 0x004a1a90u:driving::tire_load(e,W,P,driving::SuspensionTuning{},m);return;
    case 0x004a63c0u:driving::contact_matrix(W);return;
    case 0x004a65c0u:(void)driving::maximum_velocity_check(W);return;
    case 0x004a1140u:driving::copy_car_work_4a1140(e,W,P,f.wheels,m);return;
    case 0x004a1680u:driving::set_car_camera_4a1680(e,W,m);return;
    case 0x00458e40u:{
        driving::PcHandicapInputs in{};
        in.active_nodes=c.start_mode.network_player_count_7df10f;                // 7DF10F
        in.local_car_id=w.slot_7dd138;                                          // 7DD138
        in.race_ready=race_ready_4963e0(c);
        in.reference_course_position=reference_position_456bb0(w);
        in.handicap_table_index=w.commrace_7de418[0];
        in.stage_current=std::int32_t(f.stage_property(e.u32(0x68)));           // 44C940
        in.stage_reference=c.game_mode.course_runtime.max_depth_7d33c0;         // 44C980
        in.max_cs_len0=f.course_end(0);in.max_cs_len1=f.course_end(1);
        driving::handicap_control_458e40(e,in);return;}
    case 0x004a4830u:driving::check_driving_skill_4a4830(e,{std::int32_t(f.stage_property(e.u32(0x68))),
        std::uint8_t(w.commrace_7de418[0])});return;
    case 0x004a4900u:{                                                          // CheckChickenDriver
        // 43D920 (GetColiPnumByCsLen) on the car's place, cs+8 (clamped to 43D470) and cs-8
        // (clamped to 0); 43D340 (CopGetOfsDir) of each first polygon; 453720 volume 1.
        driving::PcChickenDriverInputs in{};
        const std::uint32_t type=e.u32(0x5c);
        if(type>=f.tables.courses.size()){f.miss(0x4a4900u);return;}
        std::array<std::array<std::uint8_t,0x10>,3> place{};
        for(auto& p:place)for(unsigned k=0;k<0x10;++k)p[k]=e.u8(0x5c+k);
        {std::int16_t up;std::memcpy(&up,&place[1][8],2);up=std::int16_t(up+8);
         const std::int16_t end=std::int16_t(f.course_end(type));if(up>end)up=end;std::memcpy(&place[1][8],&up,2);}
        {std::int16_t dn;std::memcpy(&dn,&place[2][8],2);dn=std::int16_t(dn-8);if(dn<0)dn=0;std::memcpy(&place[2][8],&dn,2);}
        std::array<std::int32_t,3> first{};std::int32_t last{};
        for(unsigned k=0;k<3;++k){
            in.query_status[k]=driving::find_selected_course_run(f.tables,Bytes(place[k].data(),0x10),e.i32(0x1c0),first[k],last);
            if(in.query_status[k]<=0)break;                                     // 4A499B / 4A49C1 / 4A49E7
        }
        if(in.query_status[0]>0&&in.query_status[1]>0&&in.query_status[2]>0){
            const auto& course=f.tables.courses[type];
            const float yaw=f.area_yaw()[type?1:0];
            for(unsigned k=0;k<3;++k)in.offset_direction[k]=driving::course_collision_offset_direction(course.polygons,std::uint32_t(first[k]),yaw,course.polygons_present);
        }
        in.volume1=f.volume(1);
        driving::check_chicken_driver_4a4900(e,in);
        return;}
    case 0x004a4ba0u:driving::assist_chicken_driver_4a4ba0(e,{std::uint8_t(w.commrace_7de418[0]),f.volume(1),f.volume(2)});return;
    case 0x004a2400u:driving::pc_advance_crash_state(e,exe_tables().crash);return;
    case 0x004a25f0u:{                                                          // CheckNightAndTunnel
        // night 4AFB60 / tunnel 4AFB90 bits of *[79F5EC] (the SCN_EFC work 780280), course flags 44BDB0
        const auto& fx=c.race_effects.scene;
        if(c.event_state.slots[387].work_token!=PcSceneEffectsState::WorkBase){f.miss(0x4afb60u);return;}   // event 387 closed
        const std::uint8_t bits=fx.work[0];
        auto& area=native_race_area_memory(c,nullptr,false);
        const std::uint32_t sel=area.u32(0x7d3188u);
        const std::uint32_t flags=sel?area.u32(area.u32(sel+0x14u)+0x3cu):0u;
        driving::check_night_and_tunnel_4a25f0(e,{((bits>>3)&1u)!=0u,((bits>>4)&1u)!=0u,std::uint8_t(flags)});
        return;}
    case 0x004a2650u:{
        // 4493E0: 634B34 is a .data constant 1.0f (its only reference in the
        // image is this load); 79FB4E is the AUTOSCENE (event 6) flag byte.
        const std::uint8_t autoscene=std::uint8_t(c.event_state.slots[6].flags);
        if((autoscene&3u)==2u){f.fail(pc,"4B5FD0 needs the active AUTOSCENE work 799CA0");return;}
        driving::PcCameraBlend blend{c.race.camera_override.override_82e7d8,autoscene,0,enhancements::display_blend()};
        driving::PcDispMatrixContext display{m,driving::camera_blend_4493e0(blend),c.race.camera_override.scene_82e7d4};
        driving::pc_calc_disp_matrix(e,display);
        enhancements::display_note_car(e,[&c=c]{const std::uint8_t autoscene=std::uint8_t(c.event_state.slots[6].flags);   // port: replay between ticks
            if((autoscene&3u)==2u)return 1.f;
            return driving::camera_blend_4493e0({c.race.camera_override.override_82e7d8,autoscene,0,enhancements::display_blend()});},c.race.camera_override.scene_82e7d4);
        return;}
    case 0x004a2d70u:driving::calc_vibrate_matrix_4a2d70(e,Bytes(w.vibrate_841bd0.data(),w.vibrate_841bd0.size()));return;
    case 0x004a2910u:{                                                          // CheckReverseCar (as the preview 4A5B20 binds it)
        const auto model=e.u8(0x11);
        if(model>=VehicleConstructorData.size()){f.fail(pc,"4866C0 model outside the constructor table");return;}
        const auto& row=VehicleConstructorData[model];
        auto param=[&](unsigned i){float v;std::memcpy(&v,&row[i],4);return v;};
        driving::PcReverseCarInputs reverse{param(16),param(17),param(18),param(19),0,f.game_mode()};
        if(e.u32(0x1f4)>100)reverse.random_value=frontend_crt_random_580f40(c.event_function36.pc_crt_random_state);   // 580F40
        driving::check_reverse_car_4a2910(e,reverse,m);return;}
    case 0x004a3d40u:driving::calc_light_rate_4a3d40(e,m);return;
    case 0x004a45f0u:{                                                          // CalcOfsLeftLane
        // selector +230 (4A45F9, measured); 43D130 lane rates 780160[type]; 43D1D0 the four
        // corners of polygon `selector` (780110[type]) through 44BED0(type) (409EF0 / 40A170 / 40A7D0 x4 / 40A010).
        driving::PcOfsLeftLaneInputs in{};
        in.selector=driving::pc_ofs_left_lane_selector_4a45f9(e);
        if(in.selector!=-1){
            const std::uint32_t type=e.u32(0x5c);
            if(type>=f.tables.courses.size()){f.miss(0x4a45f0u);return;}
            const Bytes rates=f.coli_section(type,0x30,4);
            in.rates=driving::pc_lane_rates_43d130(in.selector,rates.size()?&rates:nullptr);
            const auto& poly=f.tables.courses[type].polygons;
            const Bytes transform=f.tables.transforms[type?1u:0u];
            const std::size_t at=std::size_t(std::uint32_t(in.selector))*0x40u;
            if(poly.size()<at+0x30u||transform.size()<64u){f.miss(0x43d1d0u);return;}
            driving::pc_matrix_push_load(m,transform);
            for(unsigned k=0;k<4;++k)in.points[k]=driving::pc_matrix_point(m,{poly.f32(at+k*12u),poly.f32(at+k*12u+4u),poly.f32(at+k*12u+8u)});
            driving::pc_matrix_pop(m);
        }
        driving::calc_ofs_left_lane_4a45f0(e,in);
        return;}
    case 0x0046eb40u:driving::calc_disp_steering_angle_46eb40(e);return;
    case 0x004a4710u:driving::record_ghost_car_4a4710(e,Bytes(w.ghost_83db30.data(),w.ghost_83db30.size()),
        {std::int32_t(c.race.camera_override.steering_82e7ec),c.race.camera_override.w82e7cc,f.game_mode()});return;
    case 0x004962a0u:return;                                                    // parent input
    case 0x0047f780u:if(!native_records_player_47f780(c,m))f.miss(0x47f780u);return;   // variant-0 recording (race_ghosts_runtime; a latched fault is reported there)
    case 0x004671d0u:if(!native_ghost_record_4671d0(c,m))f.miss(0x4671d0u);return;   // TA ghost recording (race_ghosts_runtime; a latched ghost fault is reported there)
    case 0x00479670u:                                                           // othcarGetR (race_traffic radius_479670)
        if(!native_race_traffic_car_service(c,0x479670u,c.event_state.slots[8].work_token,m))f.miss(0x479670u);
        return;
    default:f.fail(pc,"unknown CommonPlCar child");return;
    }
    }catch(const std::exception& ex){f.fail(pc,ex.what());}
}
// ---- GamePlCar_Ctrl 0x4A8330 children ----------------------------------
void game_service(void* user,std::uint32_t pc){
    auto& f=*static_cast<Frame*>(user);
    if(f.failed)return;
    auto& c=f.c;auto& w=f.w;auto& m=f.m;Bytes e=f.e,W=f.W,P=f.P;
    try{
    switch(pc){
    case 0x0043f9f0u:f.parent.game_state_byte=c.event_function36.title_game_state_780270;return;
    case 0x0043f9c0u:f.parent.game_flag=c.start_mode.game_flag_780248;return;
    case 0x0044ff10u:return;                                                    // result unused
    case 0x0043cc20u:{
        auto& lct=c.event_function36.car_select.race_prediction;                // 7801A8.. / 780240 / 78023C
        const auto v=e.u32(0x5c);for(unsigned k=0;k<15;++k)lct.recent[k]=v;lct.cursor=0u;lct.easy=v;
        f.snapshot_valid=(e.u32(4)&0x800000u)!=0u;
        if(f.snapshot_valid)std::memcpy(f.snapshot.data(),c.event_function36.car_select.body_82e7f0.data(),0x900);
        return;}
    case 0x0044c940u:f.stage_level=f.stage_property(e.u32(0x68));return;
    case 0x00451350u:f.parent.entry_mode=native_race_route_451350(c,std::int32_t(f.stage_level));return;   // race manager 451350
    case 0x00409ef0u:driving::pc_matrix_push(m);return;
    case 0x00487740u:driving::pc_matrix_load_rotation(m,e.sub(0x70,0x40));return;
    case 0x0040a270u:
        if(f.translation_calls++==0u)driving::pc_matrix_set_translation(m,{e.f32(0x14),e.f32(0x18),e.f32(0x1c)});
        else driving::pc_matrix_set_translation(m,f.local);
        return;
    case 0x0040a7d0u:f.local=driving::pc_matrix_point(m,{W.f32(0x220),W.f32(0x224),W.f32(0x228)});return;
    case 0x0040a0d0u:driving::pc_matrix_get(m,W.sub(0x10,64));return;
    case 0x0049fad0u:
        // 4A8586..4A85A6: work velocity = E+20..28 * (1/[7162C4]), computed by the parent.
        W.putf(0x5c,f.parent_state.world_position[0]);W.putf(0x60,f.parent_state.world_position[1]);W.putf(0x64,f.parent_state.world_position[2]);
        driving::operation_input_49fad0(e,{f.game_mode(),f.volume(0),f.volume(1),f.volume(2)});return;
    case 0x0049fb70u:driving::control_timeup_braking_49fb70(e,W,P,race_time_44fe40(c.race.manager.state));return;   // 44FE40: race manager time 7D394C
    case 0x004a4d20u:{
        driving::PcCheckSlipStreamInputs in{};in.network_session_active=c.start_mode.manager_state_7f94c0!=0u;
        // Events 9..31: the rival/traffic car works of the race manager block 7815A0 (stride 10F0).
        auto& works=c.race.manager.car_works_7815a0;
        std::array<std::uint32_t,23> at{};
        for(unsigned id=9;id<=31;++id){
            auto& k=in.candidates[id-9];const auto& slot=c.event_state.slots[id];
            k.open_state=std::uint8_t(slot.flags&3u);
            if((k.open_state&3u)!=2u)continue;
            const std::uint32_t base=NativeRaceManager::CarWorkBase+(id-9u)*NativeRaceManager::CarWorkStride;
            if(slot.work_token!=base||std::size_t(id-9u+1u)*NativeRaceManager::CarWorkStride>works.size()){f.miss(0x4a4d20u);return;}
            const Bytes o(works.data()+std::size_t(id-9u)*NativeRaceManager::CarWorkStride,NativeRaceManager::CarWorkStride);
            at[id-9]=1u;
            k.flags=o.u32(4);k.network_state=o.u32(0xd14);
            k.position={o.f32(0x14),o.f32(0x18),o.f32(0x1c)};k.direction={o.f32(0x20),o.f32(0x24),o.f32(0x28)};
            k.speed=o.f32(0x1c4);k.cooldown=o.i16(0xb72);
        }
        driving::check_slipstream_4a4d20(e,in);
        for(unsigned id=9;id<=31;++id)if(at[id-9]){
            Bytes o(works.data()+std::size_t(id-9u)*NativeRaceManager::CarWorkStride,NativeRaceManager::CarWorkStride);
            o.put16(0xb72,std::uint16_t(in.candidates[id-9].cooldown));}
        return;}
    case 0x004a50f0u:driving::check_shift_warning_4a50f0(e,P,{f.volume(1),f.volume(2)});return;
    case 0x004a5260u:driving::check_wanderer_4a5260(e,W,{f.variant(),std::int32_t(f.stage_property(e.u32(0x68)))});return;
    case 0x00502c90u:{
        const auto* data=c.game_mode.driving_data;
        if(!data){f.fail(pc,"no driving data");return;}
        driving::Tables t{{Bytes(const_cast<std::uint8_t*>(data->torque_tables[0].data()),data->torque_tables[0].size()),
            Bytes(const_cast<std::uint8_t*>(data->torque_tables[1].data()),data->torque_tables[1].size())},
            Bytes(const_cast<std::uint8_t*>(data->brake_table.data()),data->brake_table.size())};
        driving::DrivingControlInputs in{};
        in.analog_channel_1=f.volume(1);in.input_inhibited=c.event_function36.title_game_state_780270!=0u;
        in.shift_up=f.pressed(0x80u)!=0u;in.shift_down=f.pressed(0x40u)!=0u;
        in.road_mu.service_available=c.start_mode.manager_state_7f94c0!=0u;      // offline: constants 1.0 / 0.7
        in.running_resistance=RaceRunningResistance;in.reaction_blend_parameter=P.f32(0x20a8);
        driving::driving_control(e,W,P,t,in);return;}
    case 0x004a8100u:{
        driving::PcCommonPlCarParentInputs in{};in.game_mode=f.game_mode();in.timer=std::int16_t(c.game_mode.start_countdown);
        in.route_state=f.variant();in.session_mode4=native_mission_type4_4962a0(c);   // 4962A0: mission type 4
        driving::common_pl_car_4a8100(e,W,P,f.wheels,in,{&f,common_service});return;}
    case 0x0040a010u:driving::pc_matrix_pop(m);return;
    case 0x00455f50u:driving::car_calc_total_cs_len_455f50(e,Bytes(w.commrace_7de418.data()+(0x7df350u-0x7de418u),64u*0x24u),w.clock_7f1938);return;   // 7DF350 inside the CommRace block (the network sends it)
    case 0x004a2130u:driving::car_calc_current_stage_progress_4a2130(e,{f.course_end(0),f.course_end(1),w.gate_80fb14!=0u,
        c.start_mode.selection_active_836374},w.progress_680bd0);return;
    case 0x0043d470u:f.parent.course_end=f.course_end(0);return;
    case 0x0045a2b0u:                                                           // CommRace rank (protected VM: mode 16 -> AL 0 / network)
        if(c.mode_state.current==16u&&(f.variant()==3u||f.variant()==4u)){      // LAN: 456870 / 459D10 / 459E10 (translated)
            std::uint32_t eax{};
            if(!native_race_network_call(c,m,0x45a2b0u,{e.u8(0x10)},eax)){f.fail(pc,"45A2B0 LAN rank (race traffic latched)");return;}
            f.parent.rank=std::uint8_t(eax);return;}
        f.parent.rank=std::uint8_t(driving::pc_comm_race_get_rank_45a2b0(e.u8(0x10),
            Bytes(w.commrace_7de418.data()+(0x7df118u-0x7de418u),0x100),std::uint32_t(f.game_mode()),std::uint32_t(f.variant())));return;
    case 0x0045c440u:f.parent.heart_mode=w.heart_mode_7f2428;return;
    case 0x004a5650u:driving::ham_nos_set_speed_4a5650(e,W,P,w.nos_speed_7f8abc);return;
    case 0x0055a930u:f.parent.network_tail_active=c.start_mode.manager_state_7f94c0!=0u;return;
    case 0x0046c390u:f.miss(0x4fb870u);return;
    case 0x004a2ee0u:driving::set_old_param_buffer_4a2ee0(e);return;
    case 0x00457770u:{                                                          // LAN: (float)457480() to 7DE480 + slot * 0x6C
        std::uint32_t eax{};
        if(!native_race_network_call(c,m,0x457770u,{},eax))f.fail(pc,"457770 LAN progress (race traffic latched)");
        return;}
    default:f.fail(pc,"unknown GamePlCar child");return;
    }
    }catch(const std::exception& ex){f.fail(pc,ex.what());}
}
}
const driving::PcCrashTables& race_crash_tables(){return exe_tables().crash;}
// 4A6EA0 / 4A2270 crash entry for another car's event (the traffic cars): the
// entry context ColiCar (519830) builds, with the impact feedback views of
// the caller's memory; a requested tow (PlWrecker) uses the player's body
// work 82E7F0 as the PC does.
bool race_car_crash_entry(NativeRuntimeContext& c,driving::PcMatrixStack& m,Bytes e,std::uint32_t pc,std::uint32_t state,
    std::uint32_t reverse,bool tow,float rate,const driving::PcImpactFeedback& feedback,std::string& error){
    auto& s=c.event_function36.car_select;auto& w=c.race.car_world;
    const Bytes W(s.body_82e7f0.data(),s.body_82e7f0.size()),P(s.parameters.data(),s.parameters.size());
    Frame f{c,w,m,e,W,P,driving::embedded_wheels(W),c.start_mode.scene_owner_course_world.tables()};
    bind_ends(f);bind_stages(f);
    try{
        const auto& x=exe_tables();
        driving::CourseWorldQuery query{f.tables,m,c.event_function36.car_select.race_prediction};
        driving::PcRoadInfoContext road{f.tables,m,f.area_yaw()};
        auto route=f.route();
        driving::PcCourseAdvanceContext advance{f.ends,f.stages,f.stage_cache(),route,0u,0u,false};
        driving::PcDispMatrixContext display{m,1.f,c.race.camera_override.scene_82e7d4};
        driving::PcPlWreckerContext wrecker{advance,road,query,display};
        driving::PcCrashWreckerContext crash_wrecker{W,P,W.sub(0x258,4*0xf4),&wrecker};
        const driving::PcCrashEntryContext entry{x.crash,f.stages,x.view(0x5c2570u,0x5c2f80u-0x5c2570u),feedback,&crash_wrecker};
        if(pc==0x4a6ea0u)driving::pc_start_crash(e,state,reverse,tow,entry);
        else driving::pc_enter_crash(e,state,reverse,tow,rate,entry);
        return true;
    }catch(const std::exception& ex){error=ex.what();return false;}
}
// 5041B0 CbwColiWall for another car's event (the traffic cars' 504E70): the
// wall controller context ColiCar (519830) builds for the player car, over the
// given event, wall work and parameter view.
bool race_car_cbw_coli_wall_5041b0(NativeRuntimeContext& c,driving::PcMatrixStack& m,Bytes e,Bytes W,Bytes P,Bytes contacts,std::string& error){
    auto& w=c.race.car_world;
    Frame f{c,w,m,e,W,P,driving::embedded_wheels(W),c.start_mode.scene_owner_course_world.tables()};
    bind_ends(f);bind_stages(f);
    try{
        const auto& x=exe_tables();
        driving::CourseWorldQuery query{f.tables,m,c.event_function36.car_select.race_prediction};
        driving::PcRoadInfoContext road{f.tables,m,f.area_yaw()};
        auto route=f.route();
        driving::PcCourseAdvanceContext advance{f.ends,f.stages,f.stage_cache(),route,0u,0u,false};
        driving::PcDispMatrixContext display{m,1.f,c.race.camera_override.scene_82e7d4};
        driving::PcPlWreckerContext wrecker{advance,road,query,display};
        driving::PcCrashWreckerContext crash_wrecker{W,P,W.sub(0x258,4*0xf4),&wrecker};
        driving::PcWallControllerContext wall{f.tables,{f.surface_codes(0),f.surface_codes(1),f.surface_codes(2),f.surface_codes(3)},x.view(0x5e0de0u,16u),
            driving::WallReboundContext{driving::WallResponseContext{f.stages,x.view(0x5e0f20u,0x5e1fa0u-0x5e0f20u),x.view(0x5e1fa0u,0x5e302cu-0x5e1fa0u),
                w.commrace_7de418[0],w.rotation_5e302c,w.rotation_5e3030},f.stage_cache(),route,
                driving::PcSoundQueue{Bytes(w.sound_entries_9563e8.data(),128),Bytes(w.sound_state.data(),8),std::uint8_t(c.event_state.slots[0x17fu].flags)}},
            driving::PcCrashEntryContext{x.crash,f.stages,x.view(0x5c2570u,0x5c2f80u-0x5c2570u),
                driving::PcImpactFeedback{x.view(0x5b3a30u,12u),x.view(0x5b3a3cu,20u*4u),w.gate_80fb14!=0u},&crash_wrecker},
            driving::PcCrushSelection{-170.0f,-130.0f,216.720001220703125f,-20.0f,-33.0f,0x1.657186p-3f,x.view(0x5e0df0u,16u*4u)},
            driving::PcMaterialSounds{x.view(0x5e0f00u,32u),material_commands(x)},f.ends,std::uint32_t(f.game_mode()),std::uint32_t(f.variant())};
        driving::cbw_coli_wall(e,W,contacts,m,wall);
        w.rotation_5e302c=wall.rebound.response.rotation_odd;w.rotation_5e3030=wall.rebound.response.rotation_even;
        return true;
    }catch(const std::exception& ex){error=ex.what();return false;}
}
bool race_player_car_control_4a8330(NativeRuntimeContext& c,driving::PcMatrixStack& matrices){
    auto& s=c.event_function36.car_select;auto& w=c.race.car_world;
    Frame f{c,w,matrices,Bytes(s.car_799d18.data(),s.car_799d18.size()),Bytes(s.body_82e7f0.data(),s.body_82e7f0.size()),
        Bytes(s.parameters.data(),s.parameters.size()),driving::embedded_wheels(Bytes(s.body_82e7f0.data(),s.body_82e7f0.size())),
        c.start_mode.scene_owner_course_world.tables()};
    bind_ends(f);bind_stages(f);
    f.parent.route_state=f.variant();f.parent.stage_denominator_base=w.stage_base_841fa4;
    f.parent.world_scale_divisor=0.0166112948f;                                  // 7162C4 = 1/60.2
    try{driving::game_pl_car_ctrl_4a8330(f.e,f.parent,f.parent_state,{&f,game_service});}
    catch(const std::exception& ex){f.fail(0x4a8330u,ex.what());}
    w.stage_base_841fa4=f.parent_state.stage_denominator_base;
    if(!f.failed&&(f.e.u32(4)&0x800000u)){
        // 4A86B2..4A8703: six wheel words from the snapshot taken after 43CC20.
        const Bytes snap(f.snapshot.data(),f.snapshot.size());
        for(std::size_t o:{0x28au,0x37eu,0x288u,0x37cu,0x470u,0x564u})f.W.put16(o,std::uint16_t(snap.i16(o)));
    }
    ++w.frames;
    return !f.failed;
}
// 475720 PasPlCar_Ctrl (event 8 while the camera script's 486942 selects
// it): race_goal_camera's transliteration over the player car (7804B0 via
// 799D18), the body work 82E7F0 (tyre records 82EA38..), 82E7D4 and the
// 5B2F68 model table; its children run on the native car work as in
// GamePlCar_Ctrl (4A2650, 4A2EE0, 4A4710, 4A8330, 43EB60, 43D390).
bool race_player_car_pas_475720(NativeRuntimeContext& c,driving::PcMatrixStack& matrices){
    auto& s=c.event_function36.car_select;auto& w=c.race.car_world;
    Frame f{c,w,matrices,Bytes(s.car_799d18.data(),s.car_799d18.size()),Bytes(s.body_82e7f0.data(),s.body_82e7f0.size()),
        Bytes(s.parameters.data(),s.parameters.size()),driving::embedded_wheels(Bytes(s.body_82e7f0.data(),s.body_82e7f0.size())),
        c.start_mode.scene_owner_course_world.tables()};
    bind_ends(f);bind_stages(f);
    PcRaceMemory m;race_robots_map_tables(m);
    constexpr std::uint32_t Car=0x7804b0u;
    std::uint32_t car_799d18=Car;const std::uint8_t scene=c.race.camera_override.scene_82e7d4;
    m.map(0x799d18u,reinterpret_cast<std::uint8_t*>(&car_799d18),4);m.map(Car,s.car_799d18.data(),s.car_799d18.size());
    m.map(0x82e7f0u,s.body_82e7f0.data(),s.body_82e7f0.size());m.map_const(0x82e7d4u,&scene,1);
    auto get3=[&](std::uint32_t a){return CourseProbe{m.f32(a),m.f32(a+4),m.f32(a+8)};};
    auto put3=[&](std::uint32_t a,const CourseProbe& v){m.putf(a,v.x);m.putf(a+4,v.y);m.putf(a+8,v.z);};
    PcRaceContext ctx{m,matrices,[&](const PcRaceCall& k)->std::uint32_t{
        const auto* a=k.args.data();
        switch(k.pc){
        case 0x4a2650u:case 0x4a4710u:common_service(&f,k.pc);return 0u;
        case 0x4a2ee0u:driving::set_old_param_buffer_4a2ee0(f.e);return 0u;
        case 0x4a8330u:if(!race_player_car_control_4a8330(c,matrices)){
            if(dev_hooks().log_faults){static std::string last;if(last!=w.last_error){last=w.last_error;dev_log("4a8330: %06X %s",unsigned(w.last_error_pc),last.c_str());}}
            f.fail(0x4a8330u,"GamePlCar_Ctrl under PasPlCar");}return 0u;
        case 0x43eb60u:{                                                    // GetYPositionProg(mode, point, polygon, 0, kind)
            if(a[3]!=0u)throw std::logic_error("43EB60 special output");
            CourseProbe p=get3(a[1]);std::uint32_t polygon=m.u32(a[2]),kind=m.u32(a[4]);
            driving::CourseWorldQuery q{f.tables,matrices,c.event_function36.car_select.race_prediction};
            (void)driving::get_y_position_prog(q,a[0],p,&polygon,nullptr,&kind);
            put3(a[1],p);m.put32(a[2],polygon);m.put32(a[4],kind);return 0u;}
        case 0x43d390u:{                                                    // GetPolNormal(polygon, type, out)
            if(a[1]>=f.tables.courses.size())throw std::out_of_range("43D390 course type");
            put3(a[2],driving::course_collision_world_normal(f.tables.courses[a[1]],std::int32_t(a[0]),matrices,f.tables.transforms[a[1]!=0u?1:0]));
            return 0u;}
        default:throw std::logic_error("unknown PasPlCar child");
        }
    },nullptr};
    try{pas_pl_car_475720(ctx,Car);}catch(const std::exception& ex){f.fail(0x475720u,ex.what());}
    ++w.frames;
    return !f.failed;
}
bool arcade_475670_body(NativeRuntimeContext&,driving::PcMatrixStack&);
// 475670 (event 8 function 0x2B control, the OUTRUN2SP car-select car): placed at
// 5B4420 (0, 2.5, -11) with +16C, yaw +2E turning by 0x64 per frame (+17E keeps the
// previous), +04 bit 0x20000 cleared, 504E70, then 46E4B0 (when +5C is 0 and the
// car is above -100 or +320 is 0/1) or 46E740, and 4A2650 / 4A2910 / 4A4710.
bool race_player_car_arcade_475670(NativeRuntimeContext& c,driving::PcMatrixStack& matrices){
    auto& w=c.race.car_world;
    try{return arcade_475670_body(c,matrices);}
    catch(const std::exception& x){w.last_error_pc=0x475670u;w.last_error=x.what();
        dev_log("475670: %s",x.what());
        return false;}
}
bool arcade_475670_body(NativeRuntimeContext& c,driving::PcMatrixStack& matrices){
    auto& s=c.event_function36.car_select;auto& w=c.race.car_world;
    Frame f{c,w,matrices,Bytes(s.car_799d18.data(),s.car_799d18.size()),Bytes(s.body_82e7f0.data(),s.body_82e7f0.size()),
        Bytes(s.parameters.data(),s.parameters.size()),driving::embedded_wheels(Bytes(s.body_82e7f0.data(),s.body_82e7f0.size())),
        c.start_mode.scene_owner_course_world.tables_or_empty()};        // the frontend has no course lane
    bind_ends(f);bind_stages(f);
    Bytes e=f.e;
    for(std::size_t o:{std::size_t(0x14),std::size_t(0x16c)}){e.putf(o,0.f);e.putf(o+4,2.5f);e.putf(o+8,-11.f);}
    e.put16(0x17e,std::uint16_t(e.i16(0x2e)));e.put16(0x2e,std::uint16_t(std::uint16_t(e.i16(0x2e))+0x64u));
    e.put32(4,e.u32(4)&0xfffdffffu);
    constexpr std::uint32_t Car=0x7804b0u;
    std::uint32_t r=0;std::string error;
    if(!native_race_traffic_car_call(c,0x504e70u,Car,matrices,r,error)){f.fail(0x504e70u,error.c_str());return false;}
    bool tilt=true;
    if(e.u32(0x5c)==0u){
        const auto k=e.u32(0x320);
        if(!(e.f32(0x300)<-100.f)||k==0u||k==1u){                                  // comiss 628324: jae (NaN falls through)
            if(!native_race_traffic_car_call(c,0x46e4b0u,Car,matrices,r,error)){f.fail(0x46e4b0u,error.c_str());return false;}
            tilt=r==0u;
        }
    }
    if(tilt&&!native_race_traffic_car_call(c,0x46e740u,Car,matrices,r,error)){f.fail(0x46e740u,error.c_str());return false;}
    common_service(&f,0x4a2650u);common_service(&f,0x4a2910u);common_service(&f,0x4a4710u);
    ++w.frames;
    return !f.failed;
}
void race_input_update_453bb0(NativeRuntimeContext& c,const PcInputDevice& device,bool keyboard_table){
    auto& w=c.race.car_world;
    // 453BB9: [8999C0 + 1D0] (no code writes it) selects 5A7B50[7D6880]; 0 takes
    // 5A7B30[7C24CA], whose only record is index 0 (gear down 0x4, no analogue flag).
    const auto cfg=keyboard_table?pc_input_keyboard_config():pc_input_joystick_config(w.joystick_config_7d6880);
    // 7C24CB: steering speed byte of the active license (Options > Controls 4D8890).
    w.steer_option_7c24cb=std::int8_t(c.event_function36.frontend_profiles.active[0xeb]);
    pc_input_switch_453640(w.switch_7d6770,device,cfg);
    pc_input_analog_453860(w.analog_7d6810,w.steer_filter_7d6884,device,cfg,w.steer_option_7c24cb,std::int32_t(c.mode_state.current));
}
}
