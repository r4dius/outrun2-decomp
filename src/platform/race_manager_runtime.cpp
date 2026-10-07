#include "system/exe_image.hpp"
#include <cstring>
#include "platform/race_variant_owners.hpp"
#include "platform/race_ghosts_runtime.hpp"
#include "platform/race_manager_runtime.hpp"
#include "platform/race_asset_pack.hpp"
#include "platform/native_runtime.hpp"
#include "platform/race_area_runtime.hpp"
#include "platform/race_end_runtime.hpp"
#include "platform/race_traffic_runtime.hpp"
#include "driving/pc_car_services.hpp"
#include "driving/pc_car_services_net.hpp"
#include "driving/pc_car_services_ghost.hpp"
#include "driving/pc_d3dx.hpp"
#include "driving/pc_crash.hpp"
#include "driving/pc_wall_rebound.hpp"
#include "driving/pc_wrecker.hpp"
#include <cstdio>
#include <cstring>
#include <sstream>
#include <stdexcept>
namespace outrun::platform {
// 4962A0 (mission type 4 session predicate), native_runtime.cpp.
bool native_mission_type4_4962a0(const NativeRuntimeContext&);
namespace {
using driving::Bytes;
// .rdata 5A2A20 time tables (0x1C2 bytes = 225 words each), the rows 44C020
// selects for presets 0/1/2/3 (rows 0, 1, 5, 6), copied from the player's EXE.
alignas(16) std::int16_t kTimeTable5a2a20[4][225]{};
OR2_EXE_COPY(kTimeTable5a2a20[0],0x5A2A20u,0x1C2u);
OR2_EXE_COPY(kTimeTable5a2a20[1],0x5A2BE2u,0x1C2u);
OR2_EXE_COPY(kTimeTable5a2a20[2],0x5A32EAu,0x1C2u);
OR2_EXE_COPY(kTimeTable5a2a20[3],0x5A34ACu,0x1C2u);
std::string hex(std::uint32_t v){char t[16];std::snprintf(t,sizeof t,"%X",v);return t;}
std::uint32_t le32(const std::uint8_t* p){std::uint32_t v;std::memcpy(&v,p,4);return v;}
float lef(const std::uint8_t* p){float v;std::memcpy(&v,p,4);return v;}
// A service the manager needs a value (or a read-back state) from, with no
// native port: the callback is aborted and the manager latched.
struct ManagerMissing : std::runtime_error {
    std::uint32_t pc;
    ManagerMissing(std::uint32_t p,const std::string& why):std::runtime_error("PC "+hex(p)+": "+why),pc(p){}
};
// Services bound to the native owners (see race_bind_report.md for the
// per-service decisions).
struct Binding final : RaceManagerServices {
    NativeRuntimeContext& c;
    NativeRaceManager& m;
    PcRaceMemory& area;                       // AREA owner memory (7D2D80.. block, course records, EXE data)
    driving::PcMatrixStack* matrices;         // 43E3B0 scratch (4A4440)
    Binding(NativeRuntimeContext& ctx,NativeRaceManager& mgr,PcRaceMemory& a,driving::PcMatrixStack* mx):c(ctx),m(mgr),area(a),matrices(mx){}
    void routed(std::uint32_t pc){++m.routed[pc];}
    void count(std::uint32_t pc){if(m.missing[pc]++==0)m.missing_order.push_back({pc,m.frame});}
    [[noreturn]] void fault(std::uint32_t pc,const std::string& why){count(pc);throw ManagerMissing(pc,why);}
    // Void service whose effect lands in another unported system's own state
    // and is never read back by the manager: counted, skipped.
    void skip(std::uint32_t pc){count(pc);++m.skipped_void[pc];}
    Bytes commrace(){auto& a=c.race.car_world.commrace_7de418;return Bytes(a.data(),a.size());}
    // ---- fill / write back ------------------------------------------------
    void fill(RaceManagerWorld& w){
        auto& s=c.event_function36.car_select;
        const auto& slots=c.event_state.slots;
        if(slots[8].work_token!=0x7804b0u)throw std::runtime_error("event 8 work token "+hex(slots[8].work_token)+" is not the car work 7804B0");
        w.event_work_799b38.fill(nullptr);
        w.event_work_799b38[8]=s.car_799d18.data();
        for(std::uint32_t id=NativeRaceManager::CarWorkFirst;id<32u;++id){
            if((slots[id].flags&3u)&&!native_ghost_car_slot(slots[id])&&!native_traffic_car_slot(slots[id]))fault(0x799b38u+id*0x3cu,"event "+std::to_string(id)+" is open: its work "+hex(slots[id].work_token)+" has no native owner (callbacks "+hex(slots[id].init_callback)+"/"+hex(slots[id].ctrl_callback)+"/"+hex(slots[id].dest_callback)+", flags "+hex(slots[id].flags)+")");
            w.event_work_799b38[id]=m.car_works_7815a0.data()+std::size_t(id-NativeRaceManager::CarWorkFirst)*NativeRaceManager::CarWorkStride;
        }
        for(std::uint32_t id=0;id<32u;++id)w.event_open_79fb48[id]=std::uint8_t(slots[id].flags);
        w.player_events_680ad4=4;                                         // .data 680AD4, no writer in .text
        w.variant_780258=std::int32_t(c.game_mode.game_variant);
        w.mode_78026c=std::int32_t(c.mode_state.current);
        w.difficulty_7c24c0=std::int32_t(le32(c.event_function36.frontend_profiles.active.data()+0xe0)); // 7C23E0 license +E0
        w.time_decrement_637911=1;                                        // .data 637911, no writer in .text
        w.ranking_80fb14=c.race.car_world.gate_80fb14;
        auto& rt=c.game_mode.course_runtime;
        w.network_7f95a8=rt.force_mode_7f95a8;
        const std::uint32_t sel=area.u32(0x7d3188u);
        w.course_present_7d3188=sel!=0u;
        if(sel){
            w.course_10=area.i32(sel+0x10u);w.course_2c=area.i32(sel+0x2cu);w.course_30=area.i32(sel+0x30u);
            w.course_14_5c=area.i16(area.u32(sel+0x14u)+0x5cu);
        }else{w.course_10=0;w.course_2c=-1;w.course_30=-1;w.course_14_5c=0;}
        w.area_7d33b0=area.u32(0x7d33b0u);
        w.time_table_7d2e98=reinterpret_cast<const std::int16_t*>(area.at(0x7d2e98u,3u*15u*5u*2u));
        w.stage_count_7d33c4=area.i32(0x7d33c4u);
        w.stage_records_7d33bc=w.stage_count_7d33c4>0?area.at(area.u32(0x7d33bcu),std::size_t(w.stage_count_7d33c4)*0x78u):nullptr;
        w.stage_cache_key_635f2c=std::uint32_t(rt.stage_key_635f2c);w.stage_cache_value_635f30=std::uint32_t(rt.stage_value_635f30);
        w.flag_84490c=m.flag_84490c;
    }
    void write_back(const RaceManagerWorld& w){
        auto& rt=c.game_mode.course_runtime;
        rt.stage_key_635f2c=std::int32_t(w.stage_cache_key_635f2c);rt.stage_value_635f30=std::int32_t(w.stage_cache_value_635f30);
        m.flag_84490c=w.flag_84490c;
    }
    // 44C8D0 over the area memory (count [7D33C4], records [7D33BC]).
    std::uint32_t find_stage(std::uint32_t key){
        const std::int32_t n=area.i32(0x7d33c4u);if(n<=0)return 0u;
        const std::uint32_t base=area.u32(0x7d33bcu);
        for(std::int32_t k=0;k<n;++k)if(area.u32(base+std::uint32_t(k)*0x78u+4u)==key)return base+std::uint32_t(k)*0x78u;
        return 0u;
    }
    // ---- services -----------------------------------------------------------
    void pc_46fab0()override{routed(0x46fab0u);race_reset_8037bc_46fab0(m.word_8037bc);}
    void hud(std::uint32_t pc){if(m.hud_2d&&m.hud_2d(pc))routed(pc);else skip(pc);}
    // rival racers / traffic (race_traffic_runtime.hpp); a latched module is skipped there
    void traffic(std::uint32_t pc){if(matrices&&native_race_traffic_hook(c,pc,*matrices))routed(pc);else hud(pc);}
    void pc_47dac0()override{traffic(0x47dac0u);}
    void pc_47dc00()override{traffic(0x47dc00u);c.start_mode.scene_owner_special_driver_80fb28=0;}   // 47D970: [80FB28]=0
    void pc_47ec00()override{traffic(0x47ec00u);}
    std::uint32_t pc_44c2c0()override{routed(0x44c2c0u);return area.u32(0x7d30acu);}             // mov eax,[7D30AC]
    std::uint8_t pc_43f860()override{routed(0x43f860u);return c.start_mode.frontend_prepare.game_flag_780260;}   // measured: byte [780260]
    std::uint8_t pc_48b1a0()override{routed(0x48b1a0u);return c.start_mode.vehicle_variant_83036d;}
    std::uint32_t pc_43f960()override{routed(0x43f960u);return race_preset_43f960(c.start_mode.course_preset);}
    std::uint8_t pc_456d60()override{routed(0x456d60u);return c.race.car_world.commrace_7de418[0];}
    void pc_4f0dd0()override{routed(0x4f0dd0u);race_visibility_init_4f0dd0(m.visibility_84cec8.data());}
    // course object set-ups and visibility (race_traffic_visibility.inc), with the score system's latch
    void pc_4f0e40()override{if(matrices&&native_race_score(c,*matrices,0x4f0e40u,0))routed(0x4f0e40u);else skip(0x4f0e40u);}
    // The CommRace saves of a LAN race (variant 4): the translated originals on ECX = 7DE418
    // (race_network_cars_tr.cpp over the traffic memory, CommRace / 7DD138 / 7F1938 mapped).
    std::uint32_t commrace_save(std::uint32_t pc,std::initializer_list<std::uint32_t> args,std::uint32_t* scratch=nullptr){
        std::uint32_t eax=0;
        if(!matrices)fault(pc,"CommRace save without the matrix stack");
        if(!native_race_network_call(c,*matrices,pc,args,eax,0x7de418u,scratch))
            fault(pc,"CommRace save: "+native_race_traffic(c).error);
        routed(pc);return eax;
    }
    void pc_456720(std::uint32_t packed)override{(void)commrace_save(0x456720u,{packed});}
    void pc_4f2ac0()override{routed(0x4f2ac0u);race_score_init_4f2ac0(m.score_84def8.data());}
    // score control / stage bonus (race_traffic_score.inc); a latched score system is skipped
    void pc_4f2df0()override{if(matrices&&native_race_score(c,*matrices,0x4f2df0u,0))routed(0x4f2df0u);else skip(0x4f2df0u);}
    void pc_4f2b20(std::int32_t t)override{if(matrices&&native_race_score(c,*matrices,0x4f2b20u,std::uint32_t(t)))routed(0x4f2b20u);else skip(0x4f2b20u);}
    std::uint8_t pc_45a2b0(std::uint8_t car)override{
        if(c.game_mode.game_variant==4u)return std::uint8_t(commrace_save(0x45a2b0u,{car}));   // LAN rank: the translated original
        routed(0x45a2b0u);
        try{return std::uint8_t(driving::pc_comm_race_get_rank_45a2b0(car,commrace().sub(0x7df118u-0x7de418u,0x100),
            c.mode_state.current,c.game_mode.game_variant));}
        catch(const std::logic_error& e){fault(0x45a2b0u,e.what());}
    }
    std::uint32_t pc_55a930()override{routed(0x55a930u);return c.start_mode.manager_state_7f94c0;}   // [7F9460+60]
    std::uint8_t pc_48b350()override{routed(0x48b350u);
        return driving::platform_counter_lt_60_48b350(std::int32_t(c.start_mode.frontend_prepare.output_code_656234))?1u:0u;}
    std::uint32_t pc_46c500()override{
        if(c.start_mode.manager_state_7f94c0==0u){routed(0x46c500u);return 0u;}   // 55A930() == 0: no 7F94C8 read
        fault(0x46c500u,"network session word 7F94C8 not owned");
    }
    // The goal side test of 4513C0 (network races and Time Attack with 48B350): the area
    // matrix 7D3190 on the matrix stack, the car's position in its frame.
    driving::PcMatrixStack& goal_stack(std::uint32_t pc){if(!matrices)fault(pc,"goal side test without the matrix stack");return *matrices;}
    std::uint32_t pc_44bec0()override{routed(0x44bec0u);return 0x7d3190u;}                // mov eax,7D3190
    void pc_409f90(std::uint32_t matrix)override{routed(0x409f90u);driving::pc_matrix_push_load(goal_stack(0x409f90u),area.bytes(matrix,0x40));}
    void pc_40a240()override{routed(0x40a240u);auto& s=goal_stack(0x40a240u);driving::pc_d3dx_matrix_inverse(s.current(),nullptr,s.current());}
    RaceVec3 pc_40a7d0(const std::uint8_t* point)override{
        routed(0x40a7d0u);float v[3];std::memcpy(v,point,12);
        const auto p=driving::pc_matrix_point(goal_stack(0x40a7d0u),{v[0],v[1],v[2]});return {p.x,p.y,p.z};}
    void pc_40a010()override{routed(0x40a010u);driving::pc_matrix_pop(goal_stack(0x40a010u));}
    // 44B900(side): past stage 7, the goal matrix 7D2DA0 (kept in 7D3130) rebuilt from
    // 7D3190 with the side's offset and rotations (636048.. / 636060..), its angles 7D3124.
    void pc_44b900(std::uint32_t side)override{
        routed(0x44b900u);
        if(area.i32(0x7d2e80u)<=7)return;
        std::memcpy(area.bytes(0x7d3130u,0x40).data(),area.bytes(0x7d2da0u,0x40).data(),0x40);
        auto& s=goal_stack(0x44b900u);
        driving::pc_matrix_push_load(s,area.bytes(0x7d3190u,0x40));
        if(side<=1u){
            const std::uint32_t t=side==0u?0x636048u:0x636060u;
            driving::pc_matrix_translate_vector(s,{area.f32(t),area.f32(t+4u),area.f32(t+8u)});
            driving::pc_matrix_rotate_y(s,area.f32(t+0x10u));
            driving::pc_matrix_rotate_x(s,area.f32(t+0xcu));
            driving::pc_matrix_rotate_z(s,area.f32(t+0x14u));
        }
        driving::pc_matrix_get(s,area.bytes(0x7d2da0u,0x40));
        const auto a=driving::pc_matrix_angles_449640(area.bytes(0x7d2da0u,0x30));
        area.putf(0x7d3124u,a[0]);area.putf(0x7d3128u,a[1]);area.putf(0x7d312cu,a[2]);
        driving::pc_matrix_pop(s);
    }
    // 46C410 / 46C400: the goal words 7F95B0 (never read) / 7F95AC (read by the goal camera 46C3F0).
    void pc_46c410(std::uint32_t v)override{routed(0x46c410u);c.game_mode.course_runtime.goal_word_7f95b0=v;}
    void pc_46c400(std::uint32_t v)override{routed(0x46c400u);c.game_mode.course_runtime.goal_word_7f95ac=v;}
    // 44B7B0 (bridge 447F7A: al = byte [7D33D0]).
    std::uint8_t pc_44b7b0(std::uint32_t arg)override{
        routed(0x44b7b0u);
        if(area.u8(0x7d33d0u)&&arg==0u)return 1u;
        const std::uint32_t sel=area.u32(0x7d3188u);
        if(area.i32(sel+0x2cu)==-1&&area.i32(sel+0x30u)==-1&&arg==0u){area.put8(0x7d33d0u,1u);return 1u;}
        return 0u;
    }
    std::uint8_t pc_4957f0()override{routed(0x4957f0u);return c.start_mode.selection_active_836374?1u:0u;}
    std::uint8_t pc_48b310()override{routed(0x48b310u);return c.start_mode.flag_830394;}
    std::uint8_t pc_495490()override{routed(0x495490u);return c.race.car_world.gate_8361b4;}
    // 44BDD0 (bridge 4CC25F: eax = [ror([10393C0],[997850])] = [7D3188], measured).
    std::uint32_t pc_44bdd0()override{
        routed(0x44bdd0u);
        const std::uint32_t sel=area.u32(0x7d3188u);
        if(sel){
            if(area.i32(0x7d2e80u)>8)return area.u32(sel+4u);
            const std::uint32_t alt=area.u32(0x7d31dcu);
            if(alt)return area.u32(alt+4u);
        }
        return area.u32(0x7d30acu);
    }
    void pc_495b60()override{routed(0x495b60u);++c.mission.manager.v836378;}            // inc [836378] (mission manager)
    // The selected Races record [83637C] (mission manager): 495B10 its +14,
    // 495B30 its +2C (variant 4) or [[+1C]+18] (0 when +1C is null).
    const std::uint8_t* mission_record(std::uint32_t pc){
        const auto* races=c.start_mode.scene_owner_race_assets;const auto i=c.mission.manager.record_83637c;
        if(i<0)return nullptr;
        if(!races||std::uint32_t(i)>=races->race_count)fault(pc,"mission record 83637C outside Races.bin");
        return races->bytes.data()+races->races_offset+std::size_t(i)*0x44u;}
    static std::uint32_t le(const std::uint8_t* p){std::uint32_t v;std::memcpy(&v,p,4);return v;}
    std::uint32_t pc_495b10()override{const auto* r=mission_record(0x495b10u);routed(0x495b10u);return r?le(r+0x14):0u;}
    std::uint32_t pc_495b70()override{routed(0x495b70u);return c.mission.manager.v836378;}   // mov eax,[836378]
    std::uint32_t pc_495b30()override{
        const auto* r=mission_record(0x495b30u);
        if(!r)fault(0x495b30u,"495B30 without a mission record (the PC reads [0+2C/1C])");
        routed(0x495b30u);
        if(c.game_mode.game_variant==4u)return le(r+0x2c);
        const auto* races=c.start_mode.scene_owner_race_assets;
        const std::size_t field=std::size_t(r-races->bytes.data())+0x1cu;
        if(le(r+0x1c)==0u)return 0u;
        std::size_t target=0;
        if(!race_asset_pointer(*races,field,target)||target+0x1cu>races->bytes.size())fault(0x495b30u,"record +1C pointer not relocated");
        return le(races->bytes.data()+target+0x18u);}
    std::uint8_t pc_4b00d0()override{
        if(!c.start_mode.scene_state_8421c0_known)fault(0x4b00d0u,"8421C0 unknown (event-function 0x21 owner not ported)");
        routed(0x4b00d0u);return c.start_mode.scene_state_8421c0!=0u?1u:0u;
    }
    std::uint32_t pc_4b0190()override{routed(0x4b0190u);return native_race_attack_value(c,0x4b0190u,nullptr,0);}   // variant 9 level
    void pc_4b0110(std::uint32_t key,std::uint32_t value)override{   // variant 9 stage hook (race_variant_owners)
        routed(0x4b0110u);const std::uint32_t a[2]{key,value};(void)native_race_attack_value(c,0x4b0110u,a,2);}
    // 48B3A0 (Time Attack goal): a stage code (656234 < 3C) with 451350(0) == 1 sets 44FEF0(1); 830398 = 1.
    void pc_48b3a0()override{
        routed(0x48b3a0u);auto& st=c.start_mode;
        if(std::int32_t(st.frontend_prepare.output_code_656234)<0x3c){
            const std::uint32_t zero=0;std::uint32_t eax{};
            if(!native_race_manager_call(c,0x451350u,&zero,1,eax,true))fault(0x451350u,"48B3A0: route 451350");
            if(eax==1u)race_set_flag2_44fef0(m.state,1u);
        }
        st.ta_result_830398=1u;
    }
    void pc_495610()override{routed(0x495610u);native_race_variant8_stage_end_495610(c);}   // variant 8 stage end (race_variant_owners)
    void pc_45a0a0()override{(void)commrace_save(0x459c30u,{});}                         // jmp 459C30
    std::uint32_t pc_456d10()override{routed(0x456d10u);return commrace().u32(0x7de77cu-0x7de418u);}          // [7DE77C]
    void pc_456d20(std::uint32_t packed)override{                                                               // 456820(packed,[7F1938]) on 7DE418
        routed(0x456d20u);auto& w=c.race.car_world;
        driving::pc_save_route_progress(commrace(),w.slot_7dd138,packed,w.clock_7f1938);
    }
    // 476760 ranking (race_traffic): the work's PC address is 7804B0 (event 8) or 7815A0 + k*10F0.
    void pc_476760(RaceEventWork work)override{
        if(!matrices)fault(0x476760u,"ranking without the matrix stack");
        std::uint32_t pc;
        const auto& s=c.event_function36.car_select;
        if(work==s.car_799d18.data())pc=0x7804b0u;
        else if(work>=m.car_works_7815a0.data()&&work<m.car_works_7815a0.data()+m.car_works_7815a0.size())
            pc=NativeRaceManager::CarWorkBase+std::uint32_t(work-m.car_works_7815a0.data());
        else fault(0x476760u,"ranking work outside the car works");
        if(!native_race_traffic_ranking(c,pc,*matrices))
            fault(0x476760u,"traffic module latched: "+native_race_traffic(c).error);
        routed(0x476760u);
    }
    std::uint32_t pc_456dc0(std::uint32_t count,std::uint32_t& key,std::uint32_t flag)override{   // 4568E0(count, &key, flag) != 0
        return (commrace_save(0x4568e0u,{count,NetworkCallScratch,flag},&key)&0xffu)?1u:0u;}
    std::uint16_t pc_49b2d0()override{routed(0x49b2d0u);return std::uint16_t(c.game_mode.start_countdown);}    // word 8367BC
    std::uint32_t pc_456d50()override{routed(0x456d50u);return commrace().u8(0x7de784u-0x7de418u)!=0u?1u:0u;}  // byte [7DE784] != 0
    std::int32_t pc_456d40()override{return std::int32_t(commrace_save(0x456790u,{}));}   // jmp 456790
    std::uint32_t pc_456de0(std::int32_t time)override{return (commrace_save(0x4569d0u,{std::uint32_t(time)})&0xffu)?1u:0u;}
    std::uint16_t pc_43d470(std::uint32_t type)override{
        if(type>3u)fault(0x43d470u,"course type "+std::to_string(type)+" outside 0..3");
        routed(0x43d470u);
        const auto tables=c.start_mode.scene_owner_course_world.tables();
        driving::PcCourseEndView v{std::nullopt,tables.courses[type].runs.lengths};
        if(tables.courses[type].runs.present)v.header=tables.courses[type].runs.header;
        return driving::pc_course_end_position(v);
    }
    void pc_4aeef0(std::uint32_t a,std::uint32_t level,float ratio)override{routed(0x4aeef0u);m.goal_6840ec=a;m.goal_6840f0=level;m.goal_6840f4=ratio;}
    void pc_458450()override{routed(0x458450u);m.clock_7de388=c.race.car_world.clock_7f1938;}
    void pc_467190()override{   // Time Attack goal: the last ghost packet (race_ghosts_runtime)
        if(!matrices)fault(0x467190u,"467190 needs the race matrix stack");
        if(!native_ghost_goal_467190(c,*matrices))fault(0x467190u,native_race_ghosts(c).error);routed(0x467190u);}
    std::uint8_t pc_4962a0()override{routed(0x4962a0u);return native_mission_type4_4962a0(c)?1u:0u;}
    void pc_453080(std::uint32_t v)override{routed(0x453080u);m.input_lock_7d6764=v;}
    std::uint32_t pc_4ef710()override{routed(0x4ef710u);return (c.start_mode.course_preset==2u||c.start_mode.course_preset==3u)?14u:4u;}
    void pc_465f20()override{   // 465F20: [7F8EF0] = 0 (the loaded ghost of the Time Attack record is dropped)
        routed(0x465f20u);PcRaceMemory gm;native_race_ghosts(c).map(gm);gm.put8(0x7f8ef0u,0);}
    // 44DC50: 44C8D0(key) ? [[rec+14]] : [[7D2DF4]].
    std::int32_t pc_44dc50(std::uint32_t key)override{
        routed(0x44dc50u);
        const std::uint32_t rec=find_stage(key);
        return area.i32(area.u32(rec?rec+0x14u:0x7d2df4u));
    }
    std::uint32_t pc_48b330()override{routed(0x48b330u);return c.start_mode.ta_result_830398;}   // mov eax,[830398]
    void pc_48b340()override{routed(0x48b340u);c.start_mode.ta_result_830398=0u;}
    std::pair<float,float> pc_4a4440(std::uint32_t hint,const std::uint8_t* place,const std::uint8_t* pos)override{
        if(!matrices)fault(0x4a4440u,"no matrix stack for 43E3B0");
        routed(0x4a4440u);
        const auto tables=c.start_mode.scene_owner_course_world.tables();
        const auto& rt=c.game_mode.course_runtime;
        driving::PcRoadInfoContext road{tables,*matrices,{rt.zero_7d3124[1],rt.zero_7d3178[1]}};
        std::array<std::uint8_t,0x64> buf{};
        std::array<std::uint8_t,12> where{};std::memcpy(where.data(),place,12);
        const bool ok=driving::pc_get_cs_road_info_by_cs_len(Bytes(buf.data(),buf.size()),Bytes(where.data(),12),std::int32_t(hint),road);
        const auto r=driving::pc_road_side_offsets_4a4440({ok,Bytes(buf.data(),buf.size())},{lef(pos),lef(pos+4),lef(pos+8)});
        return {r.a3,r.a4};
    }
    // 48B3D0 (Time Attack stage record): a stage code (< 3C) inserts 450580's total time into the
    // 7B17F8 table 447F60(code, time, 48B180 == 1, 48B1A0 == 1, 48B140).
    void pc_48b3d0()override{
        routed(0x48b3d0u);auto& st=c.start_mode;
        const std::int32_t code=std::int32_t(st.frontend_prepare.output_code_656234);
        if(code>=0x3c)return;
        const std::uint32_t course=std::uint32_t(std::int32_t(std::int8_t(c.start_mode.course_choice_655b59)));      // 48B140
        const std::uint32_t a=c.event_function36.car_select.transmission_830374==1u?1u:0u;                          // 48B180
        const std::uint32_t b=st.vehicle_variant_83036d==1u?1u:0u;                                                   // 48B1A0
        std::uint32_t time{};if(!native_race_manager_call(c,0x450580u,nullptr,0,time,true))fault(0x450580u,"48B3D0: total time 450580");
        // 447F60 (thiscall 7B17F8, plain code in the Steam build; the launcher check is not reproduced):
        // ten 0x1C entries per code at +4D84 (+0 name, +10 car, +14 time, +18 flags), lowest time first.
        auto& common=c.event_function36.frontend_profiles.common;
        const std::size_t first=0x4d84u+std::size_t(std::uint32_t(code))*0x118u;
        if(first+10u*0x1cu>common.size())fault(0x447f60u,"447F60: code outside the common save");
        Bytes t(common.data()+first,10u*0x1cu);
        auto value=[&](std::int32_t k){return t.u32(std::size_t(k)*0x1cu+0x14u);};
        if(value(9)<time)return;                                                       // 447FD1: entry 9 better
        std::int32_t k=9;while(k>=0&&!(value(k)<time))--k;                             // 447FF0..447FFA
        if(k==-1)native_race_end(c).state.miles[0x84bcfcu-0x84b900u]=1u;                // 4EF430
        for(std::int32_t j=9;j>=k+2;--j)for(std::size_t o=0;o<0x1cu;++o)t.put8(std::size_t(j)*0x1cu+o,t.u8(std::size_t(j-1)*0x1cu+o));
        const std::size_t r=std::size_t(k+1)*0x1cu;
        t.put32(r+0x14u,time);
        t.put8(r+0x18u,std::uint8_t((((b&1u)|((course&0xffu)<<1))<<1)|(a&1u)));
        const auto& license=c.event_function36.frontend_profiles.active;
        t.put8(r+0x10u,license[0x1cu]);                                                 // [7C23FC]
        for(std::size_t i=0;i<0x10u;++i){const auto ch=license[i];t.put8(r+i,ch);if(!ch)break;}   // strcpy (license name)
    }
    void pc_467e00()override{   // Time Attack lap restart: new ghost, rewind (race_ghosts_runtime)
        if(!matrices)fault(0x467e00u,"467E00 needs the race matrix stack");
        if(!native_ghost_restart_467e00(c,*matrices))fault(0x467e00u,native_race_ghosts(c).error);routed(0x467e00u);}
};
void latch(NativeRaceManager& m,std::uint32_t pc,const std::string& why){
    m.last_error=why;
    if(!m.latched){m.latched=true;m.fault_pc=pc;m.fault_frame=m.frame;m.fault=why;}
}
// Runs body(binding, world) with the world filled and written back.
template<class F>
void with_manager(NativeRuntimeContext& c,PcRaceMemory& area,driving::PcMatrixStack* matrices,F&& body){
    auto& m=c.race.manager;
    Binding b{c,m,area,matrices};
    RaceManagerWorld w{};
    b.fill(w);
    try{body(b,w);}
    catch(...){b.write_back(w);throw;}
    b.write_back(w);
}
}
void race_time_table_44c020(std::uint32_t preset,std::uint8_t* dst){
    const unsigned row=preset==1u?1u:preset==2u?2u:preset==3u?3u:0u;   // jump table 44C068: 0/1/5/6, >3 -> 0
    std::memcpy(dst,kTimeTable5a2a20[row],0x1c2u);
}
bool native_race_manager_invoke(NativeRuntimeContext& c,std::uint32_t callback,driving::PcMatrixStack& matrices,PcSceneRenderer* renderer){
    unsigned kind=0;
    switch(callback){case 0x450790u:kind=1;break;case 0x4515b0u:kind=2;break;case 0x44fe10u:kind=3;break;default:return false;}
    auto& m=c.race.manager;
    m.frame=c.completed_frames;
    if(kind==1)++m.inits;else if(kind==2)++m.controls;else ++m.destroys;
    if(m.latched&&kind!=3){++m.skipped_callbacks;return true;}
    if(kind==2&&!m.initialized){++m.skipped_callbacks;m.last_error="4515B0 without a completed 450790";return true;}
    const auto depth=matrices.depth;const auto offset=matrices.current_offset;
    try{
        auto& area=native_race_area_memory(c,renderer,true);
        with_manager(c,area,&matrices,[&](Binding& b,RaceManagerWorld& w){
            if(kind==1){race_manager_init_450790(m.state,w,b);m.initialized=true;}
            else if(kind==2)race_manager_control_4515b0(m.state,w,b);
            else{race_manager_destroy_44fe10(m.state,b);m.initialized=false;}
        });
    }catch(const ManagerMissing& e){latch(m,e.pc,e.what());}
    catch(const PcRaceUnmapped& e){latch(m,e.address,std::string("unmapped PC address: ")+e.what());}
    catch(const std::exception& e){latch(m,0xffffffffu,e.what());}
    matrices.depth=depth;matrices.current_offset=offset;
    return true;
}
bool native_race_manager_display(NativeRuntimeContext& c,std::uint32_t callback){
    if(callback!=0x44fe00u)return false;
    auto& m=c.race.manager;
    ++m.displays;
    race_manager_display_44fe00(m.state);      // 7D38EC += 1 (then 49A650, an empty function)
    return true;
}
bool native_race_manager_call(NativeRuntimeContext& c,std::uint32_t pc,const std::uint32_t* a,std::size_t n,std::uint32_t& eax,bool area_built){
    auto& m=c.race.manager;auto& st=m.state;
    auto arg=[&](std::size_t i)->std::uint32_t{if(i>=n)throw std::out_of_range("race manager accessor "+hex(pc)+": missing argument");return a[i];};
    auto i32=[&](std::size_t i){return std::int32_t(arg(i));};
    switch(pc){
    case 0x44fdf0u:eax=race_countdown_frames_44fdf0(st);break;
    case 0x44fe30u:race_set_time_44fe30(st,i32(0));eax=0;break;
    case 0x44fe40u:eax=std::uint32_t(race_time_44fe40(st));break;
    case 0x44fe50u:race_set_flag0_44fe50(st,arg(0));eax=0;break;
    case 0x44fe70u:eax=race_flag0_44fe70(st);break;
    case 0x44fef0u:race_set_flag2_44fef0(st,arg(0));eax=0;break;
    case 0x44ff10u:eax=race_flag2_44ff10(st);break;
    case 0x450110u:race_set_flag1_450110(st,arg(0));eax=0;break;
    case 0x450130u:eax=race_flag1_450130(st);break;
    case 0x450140u:race_set_flag3_450140(st,arg(0));eax=0;break;
    case 0x450160u:eax=race_flag3_450160(st);break;
    case 0x450230u:race_set_over_state_450230(st,arg(0));eax=0;break;
    case 0x450240u:eax=race_over_state_450240(st);break;
    case 0x4502c0u:eax=race_flag5_4502c0(st);break;
    case 0x4502d0u:race_clear_flag5_4502d0(st);eax=0;break;
    case 0x4502e0u:race_request_lamp_4502e0(st,i32(0));eax=0;break;
    case 0x450300u:eax=race_flag6_450300(st);break;
    case 0x450310u:race_clear_flag6_450310(st);eax=0;break;
    case 0x4503c0u:eax=race_leader_event_4503c0(st);break;
    case 0x450560u:eax=race_stage_key_450560(st,i32(0));break;
    case 0x450570u:eax=race_stage_frames_450570(st);break;
    case 0x450580u:eax=std::uint32_t(race_split_450580(st));break;
    case 0x450590u:eax=std::uint32_t(race_sector_ms_450590(st));break;
    case 0x4505a0u:eax=std::uint32_t(race_stage_split_4505a0(st,i32(0)));break;
    case 0x4505b0u:eax=race_sector_new_4505b0(st);break;
    case 0x4505c0u:eax=race_sector_set_4505c0(st);break;
    case 0x4505d0u:eax=race_total_frames_4505d0(st);break;
    case 0x450600u:race_set_time_adjust_450600(st,i32(0));eax=0;break;
    case 0x450610u:eax=std::uint32_t(race_sector_time_450610(st,i32(0),i32(1)));break;
    case 0x450630u:eax=std::uint32_t(race_sector_stage_time_450630(st,i32(0),i32(1)));break;
    case 0x450670u:eax=race_sector_banner_450670(st);break;
    case 0x450680u:eax=race_display_count_450680(st);break;
    case 0x450690u:eax=race_sector_goal_450690(st);break;
    case 0x4506a0u:eax=race_stage_flag_4506a0(st,i32(0));break;
    // Accessors that need the world or services.
    case 0x450320u:case 0x450380u:case 0x4503a0u:case 0x450750u:case 0x450780u:case 0x451350u:case 0x451140u:
    case 0x45a2b0u:case 0x44dc50u:case 0x43f860u:case 0x49b2d0u:case 0x451180u:case 0x450250u:{
        auto& area=native_race_area_memory(c,nullptr,!area_built);
        with_manager(c,area,nullptr,[&](Binding& b,RaceManagerWorld& w){
            switch(pc){
            case 0x450320u:eax=race_route_number_450320(st,b);break;
            case 0x450380u:eax=race_event_stage_key_450380(w,arg(0));break;
            case 0x4503a0u:eax=race_event_position_4503a0(w,arg(0));break;
            case 0x450750u:eax=race_is_last_level_450750(b,i32(0));break;
            case 0x450780u:eax=race_last_level_450780(b);break;
            case 0x451350u:eax=race_get_route_451350(st,w,b,i32(0));break;
            case 0x451140u:race_set_route_451140(st,w,b,i32(0),arg(1));eax=0;break;
            case 0x45a2b0u:eax=b.pc_45a2b0(std::uint8_t(arg(0)));break;
            case 0x44dc50u:eax=std::uint32_t(b.pc_44dc50(arg(0)));break;
            case 0x43f860u:eax=b.pc_43f860();break;
            case 0x451180u:eax=race_time_451180(st,b,w.work(8),arg(0));break;
            case 0x450250u:eax=std::uint32_t(race_next_stage_key_450250(w,std::uint16_t(c.start_mode.course_preset),i32(0),i32(1)));break;
            default:eax=b.pc_49b2d0();break;
            }
        });
        break;}
    default:return false;
    }
    ++m.accessor_calls;
    return true;
}
std::uint32_t native_race_route_451350(NativeRuntimeContext& c,std::int32_t index){
    std::uint32_t eax{};const std::uint32_t a=std::uint32_t(index);
    native_race_manager_call(c,0x451350u,&a,1,eax,false);
    return eax;
}
driving::Bytes native_race_route_words(NativeRuntimeContext& c){
    auto* p=reinterpret_cast<std::uint8_t*>(&c.race.manager.state)+(0x7d39a0u-RaceManagerState::base);
    return driving::Bytes(p,0x7d3a00u-0x7d39a0u);
}
std::string native_race_manager_status(const NativeRuntimeContext& c){
    const auto& m=c.race.manager;const auto& s=m.state;
    std::ostringstream o;
    o<<"manager init="<<m.inits<<" ctl="<<m.controls<<" disp="<<m.displays<<" dest="<<m.destroys<<" skipped="<<m.skipped_callbacks
     <<" time7D394C="<<s.time_7d394c<<" flags7D39F0="<<std::hex<<s.flags_7d39f0<<" lamp7D38E8="<<s.branch_state_7d38e8
     <<" over7D38F0="<<s.over_state_7d38f0<<std::dec<<" total="<<s.total_frames_7d39dc<<" stage="<<s.stage_frames_7d3930
     <<" cd="<<s.countdown_frames_7d3884<<" avg="<<s.average_speed_7d3888<<" zone="<<int(s.sector_zone_7d399d)
     <<" route="<<s.route_7d39a0[0]<<s.route_7d39a0[1]<<s.route_7d39a0[2]<<s.route_7d39a0[3]<<s.route_7d39a0[4];
    if(m.latched)o<<" LATCHED@"<<m.fault_frame<<": "<<m.fault;
    return o.str();
}
std::string native_race_manager_report(const NativeRuntimeContext& c){
    const auto& m=c.race.manager;
    std::ostringstream o;
    o<<native_race_manager_status(c)<<"\n  routed:";
    for(const auto& [pc,n]:m.routed)o<<" "<<std::hex<<pc<<std::dec<<"x"<<n;
    o<<"\n  missing (first frame):";
    for(const auto& [pc,f]:m.missing_order)o<<" "<<std::hex<<pc<<std::dec<<"@"<<f<<"x"<<m.missing.at(pc)<<(m.skipped_void.count(pc)?"(skipped)":"(fault)");
    o<<"\n  accessor calls="<<m.accessor_calls<<" 6840EC="<<m.goal_6840ec<<"/"<<m.goal_6840f0<<"/"<<m.goal_6840f4
     <<" 7DE388="<<m.clock_7de388<<" 7D6764="<<m.input_lock_7d6764<<" 84490C="<<unsigned(m.flag_84490c)<<"\n";
    return o.str();
}
}
