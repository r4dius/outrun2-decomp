#include "platform/bulk_fallback.hpp"
#include "platform/race_manager_runtime.hpp"
#include "platform/race_robots_runtime.hpp"
#include "platform/race_translated.hpp"
#include "platform/native_runtime.hpp"
#include "platform/race_area_runtime.hpp"   // race manager binding: 7B11A8 allocator stacks
#include "platform/pc_scene_renderer.hpp"
#include "platform/pc_render_flush.hpp"
#include "platform/pc_render_queue.hpp"
#include "platform/frontend_profiles.hpp"   // 580F40 rand
#include "platform/race_end_runtime.hpp"    // AUTOSCENE: the ending block 7D3A74.. and 84281C
#include "platform/race_autoscene.hpp"
#include "platform/race_ending_services.hpp"
#include "platform/race_hud_runtime.hpp"
#include "driving/pc_wall_rebound.hpp"
#include "platform/native_race_effects.hpp"
#include "platform/retail_asset_store.hpp"
#include "platform/pc_address_view.hpp"
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <cstdio>
#include <stdexcept>
namespace outrun::platform {
namespace {
constexpr std::uint32_t HeapLimit=0x100000u;   // 4F1F90 buffers (the retail files are 15 KB / 42 KB)
struct RobotMissing : std::runtime_error {
    std::uint32_t pc;
    explicit RobotMissing(std::uint32_t p):std::runtime_error("race robots: service without a native port"),pc(p){}
};
void put(std::array<std::uint8_t,4>& cell,std::uint32_t v){std::memcpy(cell.data(),&v,4);}
// Maps the runtime-owned state (see race_robots_runtime.hpp) for one call.
bool map_memory(NativeRuntimeContext& c,NativeRaceRobots& r,bool scene=false){
    auto& m=r.memory;m.clear();
    // The EXE tables (AUTOSCENE script rows 717D68, paths, names; the passenger's
    // translated code reads its .rdata/.data tables directly), shadowed below.
    (void)scene;for(std::size_t i=0;i<EmbeddedExeRangeCount;++i)m.map_const(EmbeddedExeRanges[i].base,EmbeddedExeRanges[i].data,EmbeddedExeRanges[i].size);
    auto& s=c.event_function36.car_select;
    const auto& slots=c.event_state.slots;
    r.state.map(m);race_robots_map_tables(m);
    const std::uint32_t car=slots[8].work_token,camera=slots[385].work_token;
    put(r.cell_799d18,car);put(r.cell_79f010,slots[362].work_token);put(r.cell_79f574,camera);put(r.cell_7f94c0,0);
    put(r.cell_78024c,c.start_mode.course_preset);put(r.cell_780258,c.game_mode.game_variant);put(r.cell_78026c,c.mode_state.current);
    for(std::size_t i=0;i<r.flags_79fb48.size();++i)r.flags_79fb48[i]=slots[i].flags;
    m.map(0x799d18u,r.cell_799d18.data(),4);m.map(0x79f010u,r.cell_79f010.data(),4);m.map(0x79f574u,r.cell_79f574.data(),4);
    m.map(0x7f94c0u,r.cell_7f94c0.data(),4);m.map(0x78024cu,r.cell_78024c.data(),4);m.map(0x780258u,r.cell_780258.data(),4);
    m.map(0x78026cu,r.cell_78026c.data(),4);m.map(0x79fb48u,r.flags_79fb48.data(),r.flags_79fb48.size());
    // C2C (43F860 set): 487EE0 picks the driver from the active license +0x20 (7C2400).
    m.map(0x7c23e0u,c.event_function36.frontend_profiles.active.data(),c.event_function36.frontend_profiles.active.size());
    if(car)m.map(car,s.car_799d18.data(),s.car_799d18.size());
    // Car works of events 9..31 (Time Attack ghost cars 9..12): race manager storage.
    {auto& works=c.race.manager.car_works_7815a0;m.map(NativeRaceManager::CarWorkBase,works.data(),works.size());}
    // The camera work 79FE10 is 0x3D0 bytes on the PC (the robot works follow
    // at 7A01E0); the runtime's buffer is larger: map only the PC extent.
    if(camera)m.map(camera,s.camera_79fe10.data(),(camera<PcRaceRobotState::WorkBase&&camera+s.camera_79fe10.size()>PcRaceRobotState::WorkBase)?
        PcRaceRobotState::WorkBase-camera:s.camera_79fe10.size());
    // race manager binding: 7D3650..7D39FF (450580/450160/450650 read it inline)
    m.map(RaceManagerState::base,reinterpret_cast<std::uint8_t*>(&c.race.manager.state),sizeof(RaceManagerState));
    // RobMotion tables 84D970/84D974 + their heap buffers (4F2470 owner)
    r.motion_tables.map_for_robots(m);
    r.heap.map(m);                                    // SetBone buffers
    for(auto& h:r.chr_heaps)m.map(h.base,h.bytes.data(),h.bytes.size());   // 488B80 character files
    // ROBDISPWORK: built by the static constructor 595430 before any object
    // db build (its 448CD0 lookups find nothing: every handle is -1).
    if(!r.disp_constructed){PcObjectDb startup;r.disp.construct_595430(startup);r.disp_constructed=true;}
    r.disp.map(m);rob_disp_map_rdata(m);
    r.osage.map(m);
    r.flag.map(m);
    r.engine.map(m);
    // Renderer state the robot displays write (4082B0 light records, 899B98).
    if(r.renderer)m.map(0x899b98u,r.renderer->environment().lights_899b98.data(),r.renderer->environment().lights_899b98.size());
    {const float step=c.race.clock.step_842114;std::memcpy(r.cell_842114.data(),&step,4);m.map(0x842114u,r.cell_842114.data(),4);}
    // AUTOSCENE: the event-6 work and its record word 799CA0, the robot event records
    // (799B38 + id * 0x3C: the work of events 0x16A..0x17F), the script files, the ending
    // block 7D3A74.. (7D3A7C motion set) and 842288..84281F (84281C).
    put(r.cell_799ca0,slots[6].work_token);m.map(0x799ca0u,r.cell_799ca0.data(),4);
    for(std::uint32_t i=0;i<r.cell_records_799b38.size();++i){
        put(r.cell_records_799b38[i],slots[0x16au+i].work_token);m.map(0x799b38u+(0x16au+i)*0x3cu,r.cell_records_799b38[i].data(),4);}
    m.map(NativeRaceRobots::AutosceneWorkBase,r.autoscene_work.data(),r.autoscene_work.size());
    for(auto& f:r.autoscene_files)m.map(f.base,f.bytes.data(),f.bytes.size());
    if(scene){auto& st=native_race_end(c).state;
        m.map(0x7d3a74u,st.ending_7d3a74.data(),st.ending_7d3a74.size());
        m.map(PcRaceEndState::NameBase,st.name_842288.data(),st.name_842288.size());}
    return true;
}
// Writes back what the module may have changed outside its own state (the
// event flags are read-only for it; car/camera are mapped in place).
std::uint32_t robot_index(std::uint32_t work){
    if(work<PcRaceRobotState::WorkBase||work>=PcRaceRobotState::WorkEnd||(work-PcRaceRobotState::WorkBase)%0x90u)return 0xffffffffu;
    return (work-PcRaceRobotState::WorkBase)/0x90u;
}
template<class F>
bool run(NativeRuntimeContext& c,std::uint32_t work,driving::PcMatrixStack& matrices,F&& body,std::vector<PcVehicleDrawCall>* draws=nullptr){
    auto& r=c.race.robots;
    // AUTOSCENE (work = its event work): its own fault latch; robots: per ROB01..ROB08.
    const bool scene=work==NativeRaceRobots::AutosceneWorkBase;
    const std::uint32_t index=scene?8u:robot_index(work);
    if(index==0xffffffffu){r.last_error="robot work token outside 7A01E0..7A0660";++r.skipped;return false;}
    std::uint32_t& fault=scene?r.autoscene_fault:r.fault[index];
    std::string& first_error=scene?r.autoscene_error:r.errors[index];
    if(fault){++(scene?r.autoscene_skipped:r.skipped);return false;}
    map_memory(c,r,scene);
    // ---- race manager binding: services with a native owner: begin ----
    // 440D50/440D70: the 7B11A8 allocator selector stacks owned by the AREA
    // runtime (440D10/440D30 there); 43F860 (byte 780260), 49B2D0 (8367BC),
    // 45A2B0, 44DC50 and the 44FExx/450xxx/451350 accessors: event-359 owner.
    PcRaceService service=[&r,&c,&matrices,&service](const PcRaceCall& k)->std::uint32_t{
        switch(k.pc){
        case 0x440d50u:++r.routed[k.pc];driving::push_alloc_state_b_440d50(native_race_area(c).alloc_stacks,k.args[0]);return 0u;
        case 0x440d70u:++r.routed[k.pc];return driving::pop_alloc_state_b_440d70(native_race_area(c).alloc_stacks);
        case 0x4f1cf0u:++r.routed[k.pc];rob_set_bone_4f1cf0(r.memory,r.heap,native_race_area(c).alloc_stacks,k.ecx,k.args[0],k.args[1]);return 0u;   // SetBone
        case 0x514bf0u:{++r.routed[k.pc];PcRaceContext inner{r.memory,matrices,service,nullptr};rob_disp_init_514bf0(inner,k.args[0]);return 0u;}   // rob_disp_init
        case 0x514f60u:{++r.routed[k.pc];PcRaceContext inner{r.memory,matrices,service,nullptr};rob_osage_init_514f60(inner,r.heap,k.args[0],k.args[1]);return 0u;}   // rob_osage_init
        case 0x514e80u:++r.routed[k.pc];rob_osage_reset_514e80(r.memory,r.heap,k.args[0]);return 0u;   // osage reset (destroy)
        case 0x46bc30u:{++r.routed[k.pc];PcRaceContext inner{r.memory,matrices,service,nullptr};float ang;std::memcpy(&ang,&k.args[1],4);race_char_grip_46bc30(inner,k.args[0],ang,k.args[2],k.args[3]);return 0u;}
        case 0x515040u:{++r.routed[k.pc];PcRaceContext inner{r.memory,matrices,service,nullptr};rob_osage_ctrl_515040(inner,k.args[0],k.args[1]);return 0u;}   // rob_osage_ctrl
        case 0x44bce0u:case 0x44bd20u:{   // area switch state: car +184/+5C, AREA 7D2D80 == 0x14, 7D3068 (AREA owner block)
            ++r.routed[k.pc];
            const auto& area=native_race_area(c).area.block;
            auto word=[&](std::uint32_t a){std::uint32_t v;std::memcpy(&v,area.data()+(a-PcRaceAreaState::BlockBase),4);return v;};
            const std::uint32_t car=r.memory.u32(0x799d18u);
            const bool on=r.memory.u32(car+0x184)!=0u&&r.memory.u32(car+0x5c)!=0u&&word(0x7d2d80u)==0x14u&&std::int32_t(word(0x7d3068u))>=0;
            if(k.pc==0x44bce0u)return on?1u:0u;
            return on?word(0x7d3068u):0xffffffffu;}
        case 0x5147d0u:++r.routed[k.pc];rob_disp_ctrl_5147d0(r.memory,k.args[0]);return 0u;   // rob_disp_ctrl
        case 0x417f70u:++r.routed[k.pc];return 0u;                                           // [8A8CE0]: only 417740 writes it (0)
        case 0x580f33u:++r.routed[k.pc];c.event_function36.pc_crt_random_state=k.args[0];return 0u;   // srand
        case 0x580f40u:++r.routed[k.pc];return frontend_crt_random_580f40(c.event_function36.pc_crt_random_state);   // rand
        // ---- robot display (487880 / 514E60) on the renderer ----
        case 0x4082b0u:{   // GetLightWorkAddress(a, b, c): bridge 447B4A adds 899B98 for b == 0
            ++r.routed[k.pc];
            const std::uint32_t a=k.args[0],b=k.args[1],cc=k.args[2];
            if(b==0u)return 0x899b98u+(a+cc)*0xa0u;
            if(b==1u)return 0x89a138u+(cc+a*2u)*0xa0u;
            return 0x899d78u+(cc+a*2u)*0xa0u;}
        case 0x514e60u:{++r.routed[k.pc];PcRaceContext inner{r.memory,matrices,service,nullptr};rob_disp_disp_514e60(inner,k.args[0]);return 0u;}
        case 0x40d840u:case 0x405360u:case 0x405450u:case 0x405580u:case 0x406800u:case 0x4044f0u:case 0x404540u:
        case 0x405350u:case 0x4052b0u:case 0x4052c0u:case 0x410670u:{
            if(!r.renderer){++r.missing[k.pc];throw RobotMissing(k.pc);}
            ++r.routed[k.pc];
            auto& rr=*r.renderer;auto& q=rr.queue_context();auto& fl=rr.flush_context();
            const auto* a=k.args.data();
            switch(k.pc){
            case 0x40d840u:{
                auto& env=rr.environment();
                PcEnvironmentRenderTables tables{driving::Bytes(env.lights_899b98.data(),env.lights_899b98.size()),
                    driving::Bytes(env.fog_7d3a10.data(),env.fog_7d3a10.size()),0};
                render_environment_40d840(fl,std::int32_t(a[0]),c.event_state.slots[386].flags,tables);return 0u;}
            case 0x405360u:
                if(a[0]!=0xffffffffu&&(a[0]>>16)<0x223u)(void)native_race_bank_ensure(c,rr,a[0]>>16);
                render_object_405360(q,a[0],std::int32_t(a[1]),driving::Bytes(nullptr,0),a[3],std::int32_t(a[4]),std::int32_t(a[5]));return 0u;
            case 0x405450u:
                for(unsigned i=0;i<2;++i)if(a[i]!=0xffffffffu&&(a[i]>>16)<0x223u)(void)native_race_bank_ensure(c,rr,a[i]>>16);
                render_object_morph_405450(q,a[0],a[1],std::int32_t(a[2]),a[3],std::int32_t(a[4]));return 0u;
            case 0x405580u:{
                if(a[0]!=0xffffffffu&&(a[0]>>16)<0x223u)(void)native_race_bank_ensure(c,rr,a[0]>>16);
                std::vector<std::uint8_t> palette(std::size_t(a[5])*64u);
                for(std::size_t i=0;i<palette.size();++i)palette[i]=r.memory.u8(a[4]+std::uint32_t(i));
                render_object_palette_405580(q,a[0],std::int32_t(a[1]),a[2],std::int32_t(a[3]),palette.data(),a[5]);return 0u;}
            case 0x406800u:q.morph_weight_95aecc=a[0];return 0u;
            case 0x4044f0u:render_pass_record_4044f0(fl.g,a[0],a[1],a[2],a[3],a[4]);return 0u;
            case 0x404540u:render_pass_defaults_404540(fl.g,rr.frame().layer_7d25f0);return 0u;
            case 0x405350u:{
                std::vector<std::uint32_t> order(q.opaque.count);
                for(std::uint32_t i=0;i<q.opaque.count;++i)order[i]=i;
                render_queue_flush_405890(fl,q.opaque,order);return 0u;}
            case 0x4052b0u:render_queue_mode_4052b0(q);return 0u;
            case 0x4052c0u:render_queue_flush_4052c0(fl,q);return 0u;
            default:fl.g.w(0x89ede0)=0;fl.g.w(0x89ede8)=0;return 0u;   // 410670
            }}
        // ---- flagman flag cloth (rob_flag.hpp) ----
        case 0x402100u:{++r.routed[k.pc];PcRaceContext inner{r.memory,matrices,service,nullptr};rob_flag_init_402100(inner);return 0u;}
        case 0x402140u:{++r.routed[k.pc];PcRaceContext inner{r.memory,matrices,service,nullptr};rob_flag_ctrl_402140(inner);return 0u;}
        case 0x4021a0u:++r.routed[k.pc];rob_flag_dest_4021a0(r.memory);return 0u;
        case 0x409e00u:case 0x402170u:{
            if(!r.renderer){++r.missing[k.pc];throw RobotMissing(k.pc);}
            auto& rr=*r.renderer;auto& fl=rr.flush_context();
            if(k.pc==0x409e00u){   // mxSetD3DTransform(slot): jump table 409E74, the current stack matrix
                static constexpr std::uint32_t States[10]={2,3,0x10,0x11,0x12,0x13,0x100,0x101,0x102,0x103};
                const std::uint32_t slot=k.args[0];
                float mtx[16];const auto cur=matrices.current();for(unsigned q=0;q<16;++q)mtx[q]=cur.f32(q*4);
                ++r.routed[k.pc];rr.device().set_transform(slot<10u?States[slot]:2u,mtx);return 0u;}
            // 402170 flagman_cloth_disp: 408880, 401B80(860040, 85FF20, 5, 7), 89EDE0/89EDE8 = 0, 408880.
            const std::uint32_t resource=rob_flag_texture_resource(c.start_mode.course_preset);
            if(!native_race_bank_ensure(c,rr,resource)){++r.missing[0x448810u];throw RobotMissing(0x448810u);}
            PcPmtResources* bank=rr.bank_resources(resource);
            if(!bank){++r.missing[0x448810u];throw RobotMissing(0x448810u);}
            ++r.routed[k.pc];
            render_reset_states_408880(fl,rr.frame().layer_7d25f0);
            PcRaceMemory save;auto& particles=c.race_effects.particles;
            save.map(PcParticleState::BssBase,particles.bss.data(),particles.bss.size());
            PcRobFlagDraw draw{r.memory,save,rr.device(),rob_flag_texture_401b80(*bank),
                [&fl](std::uint32_t a,std::uint32_t v){fl.g.w(a)=v;}};
            rob_flag_draw_401b80(draw,PcRobFlagState::Vertices,PcRobFlagState::Indices,5,7);
            fl.g.w(0x89ede0)=0;fl.g.w(0x89ede8)=0;
            render_reset_states_408880(fl,rr.frame().layer_7d25f0);
            return 0u;}
        // ---- the passenger's callees (race_robot_passenger_tr.cpp) ----
        case 0x424940u:{++r.routed[k.pc];   // race SE queue 9563E8 gated by event 383 (the voices)
            auto& w=c.race.car_world;
            driving::PcSoundQueue q{driving::Bytes(w.sound_entries_9563e8.data(),w.sound_entries_9563e8.size()),
                driving::Bytes(w.sound_state.data(),w.sound_state.size()),std::uint8_t(c.event_state.slots[383].flags)};
            driving::pc_enqueue_sound(q,k.args[0]);return 0u;}
        case 0x44c940u:{++r.routed[k.pc];   // 44C940: cache 635F2C/635F30, else the stage record +8
            auto& area=native_race_area_memory(c,nullptr,true);auto& rt=c.game_mode.course_runtime;const std::uint32_t key=k.args[0];
            if(key==std::uint32_t(rt.stage_key_635f2c))return std::uint32_t(rt.stage_value_635f30);
            std::uint32_t v=0;const std::int32_t n=area.i32(0x7d33c4u);const std::uint32_t base=area.u32(0x7d33bcu);
            for(std::int32_t i=0;i<n;++i)if(area.u32(base+std::uint32_t(i)*0x78u+4u)==key){v=area.u32(base+std::uint32_t(i)*0x78u+8u);break;}
            rt.stage_key_635f2c=std::int32_t(key);rt.stage_value_635f30=std::int32_t(v);return v;}
        case 0x44b7b0u:{++r.routed[k.pc];   // 44B7B0 (bridge 447F7A: al = byte [7D33D0]), as the race manager's
            auto& area=native_race_area_memory(c,nullptr,true);
            if(area.u8(0x7d33d0u)&&k.args[0]==0u)return 1u;
            const std::uint32_t sel=area.u32(0x7d3188u);
            if(area.i32(sel+0x2cu)==-1&&area.i32(sel+0x30u)==-1&&k.args[0]==0u){area.put8(0x7d33d0u,1u);return 1u;}
            return 0u;}
        case 0x455ad0u:++r.routed[k.pc];return c.race.car_world.slot_7dd138;                   // movzx [7DD138] (CommRace slot)
        case 0x455c10u:++r.routed[k.pc];return std::uint32_t(std::int32_t(std::int8_t(c.race.car_world.commrace_7de418[0x7df10bu-0x7de418u]))+1);
        case 0x45b380u:++r.routed[k.pc];return race_robot_model_flag_45b380(k.args[0]);
        case 0x45bf30u:{++r.routed[k.pc];   // the NAVI stage score mean (7F2560 block)
            PcRaceMemory nm;auto& hud=native_race_hud(c);nm.map(0x7f2560u,hud.navi.g7f1900.data()+(0x7f2560u-0x7f1900u),0x180u);
            return navi_score_mean_45bf30(nm,k.args[0]);}
        case 0x45c440u:++r.routed[k.pc];return c.race.car_world.heart_mode_7f2428;              // mov eax,[7F2428]
        case 0x46c520u:++r.routed[k.pc];if(c.start_mode.manager_state_7f94c0){++r.missing[k.pc];throw RobotMissing(k.pc);}return 0u;   // [7F94D4] (no LAN session: 0)
        case 0x46c530u:++r.routed[k.pc];if(c.start_mode.manager_state_7f94c0){++r.missing[k.pc];throw RobotMissing(k.pc);}return 0u;   // 55A930 ? [7F95C4] : 0
        case 0x487810u:{++r.routed[k.pc];PcRaceContext inner{r.memory,matrices,service,nullptr};race_robot_common_init_487810(inner,k.args[0]);return 0u;}
        case 0x487b70u:{++r.routed[k.pc];PcRaceContext inner{r.memory,matrices,service,nullptr};race_robot_motion_connect_487b70(inner,k.args[0],k.args[1],k.args[2]);return 0u;}
        case 0x488450u:{++r.routed[k.pc];PcRaceContext inner{r.memory,matrices,service,nullptr};race_robot_control_488450(inner,k.args[0]);return 0u;}
        case 0x4f1e70u:++r.routed[k.pc];return (r.memory.u32(k.ecx+8u)>>2)&1u;                  // motion end bit
        case 0x505340u:{   // [6AF268 + (3a + b)*4] (EXE .data table)
            static const PcAddressView exe=[]{PcAddressView v;v.add_exe();return v;}();
            std::uint32_t v{};if(!exe.u32(0x6af268u+(k.args[0]*3u+k.args[1])*4u,v)){++r.missing[k.pc];throw RobotMissing(k.pc);}
            ++r.routed[k.pc];return v;}
        case 0x5148d0u:{++r.routed[k.pc];PcRaceContext inner{r.memory,matrices,service,nullptr};rob_disp_hand_gu_5148d0(inner,k.args[0]);return 0u;}
        case 0x5148f0u:{++r.routed[k.pc];PcRaceContext inner{r.memory,matrices,service,nullptr};rob_disp_hand_pa_5148f0(inner,k.args[0]);return 0u;}
        // ---- RobMotion engine (rob_motion_engine.hpp): ECX = motion object ----
        case 0x4f2280u:{++r.routed[k.pc];PcRaceContext inner{r.memory,matrices,service,nullptr};return rob_motion_set_motion_4f2280(inner,k.ecx,k.args[0]);}
        case 0x4f1d30u:{++r.routed[k.pc];PcRaceContext inner{r.memory,matrices,service,nullptr};float f;std::memcpy(&f,&k.args[0],4);rob_motion_set_frame_4f1d30(inner,k.ecx,f);return 0u;}
        case 0x4f1e60u:++r.routed[k.pc];return rob_motion_set_loop_4f1e60(r.memory,k.ecx,k.args[0]);
        case 0x4f2320u:{++r.routed[k.pc];PcRaceContext inner{r.memory,matrices,service,nullptr};float fr,len;std::memcpy(&fr,&k.args[1],4);std::memcpy(&len,&k.args[2],4);
            return rob_motion_connect_4f2320(inner,k.ecx,k.args[0],fr,len);}
        case 0x4f2520u:{++r.routed[k.pc];PcRaceContext inner{r.memory,matrices,service,nullptr};rob_motion_calc_4f2520(inner,k.ecx);return 0u;}
        case 0x4f2260u:++r.routed[k.pc];rob_unset_bone_4f2260(r.memory,r.heap,native_race_area(c).alloc_stacks,k.ecx);return 0u;
        case 0x448cd0u:{   // object handle by name (object db 7CC1E0)
            if(!c.object_db.built){++r.missing[0x448b90u];throw RobotMissing(0x448b90u);}
            std::string name;for(std::uint32_t p=k.args[0];;++p){const auto ch=r.memory.u8(p);if(!ch)break;name.push_back(char(ch));if(name.size()>256u)throw std::runtime_error("race robots: 448CD0 name not terminated");}
            ++r.routed[k.pc];++c.object_db.lookups;
            const auto h=object_db_find_448b10(c.object_db,name);if(h==0xffffffffu)++c.object_db.misses;return h;}
        case 0x4066d0u:case 0x406730u:case 0x4103f0u:{   // bank objects on the renderer
            if(!r.renderer){++r.missing[k.pc];throw RobotMissing(k.pc);}
            const auto h=k.args[0];
            if(h!=0xffffffffu&&(h>>16)<0x223u)(void)native_race_bank_ensure(c,*r.renderer,h>>16);
            ++r.routed[k.pc];
            if(k.pc==0x4066d0u)return r.renderer->object_group_type_4066d0(h,k.args[1]);
            if(k.pc==0x406730u)return r.renderer->object_mesh_flags_406730(h,k.args[1],k.args[2]);
            return r.renderer->object_shaders_4103f0(h,k.args[1]);}
        // ---- AUTOSCENE (race_autoscene.hpp) ----
        case 0x440a60u:{   // MallocNowEventWork(size, heap) of event 6: its work + the {work, heap} trailer ([7A01D0]+24)
            const std::uint32_t ev=c.event_state.current_slot;
            if(ev!=6u||k.args[0]!=NativeRaceRobots::AutosceneWorkSize){++r.missing[k.pc];throw RobotMissing(k.pc);}
            ++r.routed[k.pc];
            const std::uint32_t base=NativeRaceRobots::AutosceneWorkBase,trailer=base+k.args[0];
            r.memory.put32(trailer,base);r.memory.put32(trailer+4u,k.args[1]);
            auto& sl=c.event_state.slots[6];sl.work_token=base;sl.aux24=trailer;
            put(r.cell_799ca0,base);return base;}
        case 0x440b20u:{   // 440CD0([7A01D0]+24): the current event's work handle
            const std::uint32_t ev=c.event_state.current_slot;
            if(ev!=6u){++r.missing[k.pc];throw RobotMissing(k.pc);}
            ++r.routed[k.pc];c.event_state.slots[6].aux24=0;return 0u;}
        case 0x440890u:{   // a free robot event (flags & 3 == 0) among 0x16A..0x17E: function 0x66 without init, 487810(work)
            ++r.routed[k.pc];
            auto& sl=c.event_state.slots;
            std::uint32_t id=0x16au,last=id;
            for(;id<0x17fu;++id){last=id;if(!(sl[id].flags&3u))break;}
            if(id==0x17fu)return last;                                   // none free: the last id tested (4408B2)
            auto& e=sl[id];
            e.flags=std::uint8_t((e.flags&0xe7u)|1u);r.flags_79fb48[id]=e.flags;
            const auto& f=c.event_functions[0x66];                        // 59C674..59C680
            e.ctrl_callback=f.ctrl_callback;e.disp_callback=f.disp_callback;e.shadow_callback=f.shadow_callback;e.dest_callback=f.dest_callback;
            e.init_callback=0;e.display_scene=c.event_descriptors[id].display_scene;
            PcRaceContext inner{r.memory,matrices,service,nullptr};race_robot_common_init_487810(inner,e.work_token);
            return id;}
        case 0x440cd0u:{   // free *p (the {payload, mode} handle), *p = 0
            ++r.routed[k.pc];
            const std::uint32_t h=r.memory.u32(k.args[0]);
            if(h){
                const std::uint32_t payload=r.memory.u32(h);
                if(payload>=RobotHeap::Base&&payload<RobotHeap::End)r.heap.free(payload);
                else if(!(payload>=NativeRaceRobots::AutosceneFileBase&&payload<NativeRaceRobots::AutosceneFileEnd)){++r.missing[k.pc];throw RobotMissing(k.pc);}
                // a script file stays mapped (its addresses are never reused)
                r.memory.put32(k.args[0],0);
            }
            return 0u;}
        case 0x440d10u:++r.routed[k.pc];driving::push_alloc_state_a_440d10(native_race_area(c).alloc_stacks,k.args[0]);return 0u;
        case 0x440d30u:++r.routed[k.pc];return driving::pop_alloc_state_a_440d30(native_race_area(c).alloc_stacks);
        case 0x580253u:{   // malloc on the robot heap (mapped at once)
            ++r.routed[k.pc];
            const std::uint32_t at=r.heap.malloc(k.args[0]);
            if(at)r.memory.map(at,r.heap.blocks.back().bytes.data(),r.heap.blocks.back().bytes.size());
            return at;}
        case 0x44fd80u:{   // the script file request (path, mode), read at once (44F980: payload + {payload, mode})
            ++r.routed[k.pc];
            std::string name;for(std::uint32_t q=k.args[0];name.size()<260u;++q){const auto ch=r.memory.u8(q);if(!ch)break;name.push_back(char(ch));}
            const bool sz=name.size()>3&&name.compare(name.size()-3,3,".sz")==0;
            std::string err;const std::vector<std::uint8_t>* bytes=nullptr;
            if(auto* retail=c.event_function36.retail_assets)bytes=retail_asset_guest_path(*retail,name,sz,&err);
            std::vector<std::uint8_t> image;const std::uint32_t base=r.autoscene_file_next;
            if(!bytes)r.autoscene_error="script "+name+" unreadable: "+err;
            else if(!race_area_loader_image(*bytes,sz,base,k.args[1],image,err))r.autoscene_error=name+": "+err;
            else if(base+std::uint64_t(image.size())>NativeRaceRobots::AutosceneFileEnd)r.autoscene_error="AUTOSCENE script heap exhausted";
            else{
                r.autoscene_file_next=(base+std::uint32_t(image.size())+0xfffu)&~0xfffu;
                r.autoscene_files.push_back({base,std::move(image)});
                r.memory.map(base,r.autoscene_files.back().bytes.data(),r.autoscene_files.back().bytes.size());
                r.autoscene_handles.push_back(base);
                return NativeRaceRobots::AutosceneFileHandle+std::uint32_t(r.autoscene_handles.size()-1u);
            }
            ++r.missing[0x44f990u];throw RobotMissing(0x44f990u);}
        case 0x44f880u:case 0x44fc60u:{
            const std::uint32_t i=k.args[0]-NativeRaceRobots::AutosceneFileHandle;
            if(k.args[0]<NativeRaceRobots::AutosceneFileHandle||i>=r.autoscene_handles.size()||!r.autoscene_handles[i]){++r.missing[k.pc];throw RobotMissing(k.pc);}
            ++r.routed[k.pc];
            if(k.pc==0x44f880u)return 1u;                                  // read synchronously: finished
            const std::uint32_t base=r.autoscene_handles[i];
            const auto file=std::find_if(r.autoscene_files.begin(),r.autoscene_files.end(),[base](const NativeRobMotionTables::Heap& f){return f.base==base;});
            const std::uint32_t trailer=base+std::uint32_t(file->bytes.size()-8u);
            // 44FC60: dest +24, size +28 (0: the loader sized it), the handle; the slot is freed
            if(k.args[1])r.memory.put32(k.args[1],base);
            if(k.args[2])r.memory.put32(k.args[2],0);
            if(k.args[3])r.memory.put32(k.args[3],trailer);
            r.autoscene_handles[i]=0;return 0u;}
        // 44FD00 / 42EBF0 / 4489F0 (scene end): the async slots, sprite and resource loaders hold nothing of
        // the scene's (its reads are synchronous and taken by 44FC60)
        case 0x44fd00u:case 0x42ebf0u:case 0x4489f0u:++r.routed[k.pc];return 0u;
        case 0x51b740u:++r.routed[k.pc];return 0u;                                 // the per-scene callbacks (VM): measured 0 for every scene and slot
        case 0x483e10u:++r.routed[k.pc];camera_set_483e10(r.memory,k.args[0],k.args[1],k.args[2],k.args[3]);return 0u;   // the camera work [79F574]
        case 0x4af520u:{   // [842118] = the next frame step; returns [842114] (x87)
            ++r.routed[k.pc];std::memcpy(&c.race.clock.next_step_842118,&k.args[0],4);
            std::uint32_t v;std::memcpy(&v,&c.race.clock.step_842114,4);return v;}
        case 0x42e020u:++r.routed[k.pc];return 1u;
        case 0x49a650u:++r.routed[k.pc];return 0u;                                 // RET                                 // mov eax,1
        case 0x4249f0u:{++r.routed[k.pc];auto& f=c.event_function36;               // a sound effect
            if(!f.frontend_effect||!f.frontend_effect(f.frontend_effect_user,k.args[0]))++r.autoscene_unplayed[0x4249f0u];return 0u;}
        case 0x4208a0u:{   // the smoke (PART_EFC 4208A0 on the effects runtime); a latched PART_EFC is counted
            ++r.routed[k.pc];
            if(!native_race_effects_smoke_4208a0(c,matrices,k.args[0],r.memory.u32(k.args[1]),r.memory.u32(k.args[1]+4u),r.memory.u32(k.args[1]+8u),k.args[2]))
                ++r.autoscene_unplayed[k.pc];
            return 0u;}
        case 0x420560u:++r.routed[k.pc];if(!native_race_effects_flare_420560(c,matrices))++r.autoscene_unplayed[k.pc];return 0u;   // the falling pieces (PART_EFC)
        case 0x487c30u:{++r.routed[k.pc];PcRaceContext inner{r.memory,matrices,service,nullptr};return race_robot_set_chara_487c30(inner,k.args[0],k.args[1]);}
        case 0x487d10u:++r.routed[k.pc];robot_matrix_487d10(r.memory,matrices,k.args[0],k.args[1]);return 0u;
        case 0x487d40u:++r.routed[k.pc];robot_flag_487d40(r.memory,k.args[0],k.args[1]);return 0u;
        case 0x514800u:++r.routed[k.pc];rob_disp_word_514800(r.memory,k.args[0],k.args[1],k.args[2]);return 0u;
        case 0x514880u:++r.routed[k.pc];rob_disp_vector_514880(r.memory,k.args[0],k.args[1],k.args[2]);return 0u;
        case 0x514830u:{   // its 4066D0 / 4103F0 through this service (the bank objects on the renderer)
            ++r.routed[k.pc];PcRaceContext inner{r.memory,matrices,service,nullptr};rob_disp_model_514830(inner,k.args[0],k.args[1],k.args[2]);return 0u;}
        case 0x48f4e0u:++r.routed[k.pc];return rob_motion_word_48f4e0(r.memory,k.ecx);
        case 0x47fbd0u:{   // 47FBD0 record sector time (race_hud_navi)
            std::uint32_t eax{};std::string why;
            if(!native_race_hud_service(c,k.pc,k.args.data(),k.argc,eax,why))throw RobotMissing(k.pc);
            ++r.routed[k.pc];return eax;}
        case 0x4ed890u:++r.routed[k.pc];rob_motion_speed_4ed890(r.memory,k.ecx,k.args[0]);return 0u;
        case 0x4f1e30u:{++r.routed[k.pc];float f;std::memcpy(&f,&k.args[0],4);rob_motion_frame_end_4f1e30(r.memory,k.ecx,f);return 0u;}
        default:break;
        }
        std::uint32_t eax{};
        if(native_race_manager_call(c,k.pc,k.args.data(),k.args.size(),eax,false)){++r.routed[k.pc];return eax;}
        if(bulk_translated(k.pc)){++r.routed[k.pc];return bulk_call(r.memory,service,k,&matrices,&c.event_function36.pc_crt_random_state);}
        ++r.missing[k.pc];throw RobotMissing(k.pc);};
    // ---- race manager binding: end ----
    PcRaceContext ctx{r.memory,matrices,service,draws};
    // A throw leaves the PC matrix stack where the original would have been
    // mid-function: restore it so the other displays keep a sane stack.
    const auto offset=matrices.current_offset;const auto depth=matrices.depth;
    try{body(ctx);++r.completed;}
    catch(const RobotMissing& e){fault=e.pc;char t[64];
        if(scene)std::snprintf(t,sizeof t,"AUTOSCENE: service %06X not ported",e.pc);else std::snprintf(t,sizeof t,"ROB%02u: service %06X not ported",index+1u,e.pc);
        r.last_error=t;}
    catch(const PcAutosceneUnported& e){fault=e.pc;char t[64];std::snprintf(t,sizeof t,"AUTOSCENE: scene callback %06X not ported",e.pc);r.last_error=t;}
    catch(const PcRaceUnmapped& e){++r.unmapped[e.address];fault=e.address;r.last_error=e.what();
        if(e.address>=PcRobMotionTablesState::Base&&e.address<PcRobMotionTablesState::End&&!r.motion_tables.ready)
            r.last_error+=" (RobMotion tables 4F2470: "+(r.motion_tables.init_calls?r.motion_tables.error:std::string("not run"))+")";}
    catch(const std::exception& e){fault=0xffffffffu;r.last_error=e.what();}
    if(fault&&first_error.empty())first_error=r.last_error;
    matrices.current_offset=offset;matrices.depth=depth;
    return fault==0u;
}
}
namespace {
// AUTOSCENE: one call on the robots' memory with the event-6 work as the run key.
template<class F> bool run_autoscene(NativeRuntimeContext& c,driving::PcMatrixStack& matrices,F&& body,std::vector<PcVehicleDrawCall>* draws=nullptr){
    ++c.race.robots.autoscene_calls;
    return run(c,NativeRaceRobots::AutosceneWorkBase,matrices,std::forward<F>(body),draws);
}
}
bool native_autoscene_call(NativeRuntimeContext& c,std::uint32_t pc,std::uint32_t arg,std::uint32_t& eax){
    std::array<std::uint8_t,0x40> mb{};driving::PcMatrixStack own{driving::Bytes(mb.data(),mb.size())};
    eax=0;
    switch(pc){
    case 0x4b5f60u:return run_autoscene(c,own,[&](PcRaceContext& ctx){autoscene_start_4b5f60(ctx,arg);});
    case 0x4b5fc0u:return run_autoscene(c,own,[&](PcRaceContext& ctx){eax=autoscene_frame_4b5fc0(ctx);});
    case 0x4b5fd0u:return run_autoscene(c,own,[&](PcRaceContext& ctx){eax=autoscene_state_4b5fd0(ctx);});
    default:return false;
    }
}
namespace {
// 488340 (function 0x3C init): in a LAN race (variant 3 / 4) the robot
// globals 82F0F0..82F370 back to their start values; 82F248 = 0 for the
// session types 1 and 4 (455C10), else 1.
void robots_lan_init_488340(NativeRuntimeContext& c){
    const std::uint32_t v=c.game_mode.game_variant;
    if(v!=3u&&v!=4u)return;
    auto& r=c.race.robots;
    map_memory(c,r,false);
    auto& m=r.memory;
    const std::uint32_t session=std::uint32_t(std::int32_t(std::int8_t(c.race.car_world.commrace_7de418[0x7df10bu-0x7de418u])))+1u;   // 455C10
    m.put32(0x82f248u,(session==1u||session==4u)?0u:1u);
    m.put32(0x82f24cu,0);m.put32(0x82f348u,0);
    for(std::uint32_t a=0x82f124u;a<0x82f224u;a+=0x20u){
        m.put32(a-4u,0);m.put32(a,0);m.put32(a+0xcu,0xffffffffu);m.put32(a+0x10u,0);m.put16(a+0x14u,0);m.put32(a+0x18u,0);
    }
    m.put32(0x82f254u,0xffffffffu);
    for(std::uint32_t a:{0x82f250u,0x82f224u,0x82f110u,0x82f5c8u,0x82f5c4u})m.put32(a,0);
    m.put16(0x82f230u,0);m.put16(0x82f22cu,0);m.put8(0x82f118u,0);m.put16(0x82f2f0u,0);m.put16(0x82f260u,0);
    for(std::uint32_t a:{0x82f5d0u,0x82f25cu,0x82f220u})m.put32(a,0);
    m.put32(0x82f338u,0x258u);m.put32(0x82f114u,0);
    const std::int32_t n=std::int32_t(c.mission.racers.v680ad4);                 // [680AD4] dwords from 82F0F0
    for(std::int32_t k=0;k<n;++k)m.put32(0x82f0f0u+std::uint32_t(k)*4u,0);
    m.putf(0x82f33cu,300.0f);m.put32(0x82f244u,0);m.put32(0x82f370u,0);m.putf(0x82f340u,-10.0f);
}
}
bool native_race_robots_invoke(NativeRuntimeContext& c,std::uint32_t callback,driving::PcMatrixStack& matrices){
    if(callback==0x488340u){robots_lan_init_488340(c);return true;}   // function 0x3C init
    // AUTOSCENE event 6 (function 0x19)
    if(callback==0x4b5150u){(void)run_autoscene(c,matrices,[](PcRaceContext& ctx){autoscene_init_4b5150(ctx);});return true;}
    if(callback==0x4b6cd0u||callback==0x4b6690u){
        const std::uint32_t work=c.event_state.slots[6].work_token;
        if(work!=NativeRaceRobots::AutosceneWorkBase){c.race.robots.autoscene_error="event 6 work "+std::to_string(work)+" is not the AUTOSCENE work";++c.race.robots.autoscene_skipped;return true;}
        if(callback==0x4b6cd0u)(void)run_autoscene(c,matrices,[work](PcRaceContext& ctx){autoscene_control_4b6cd0(ctx,work);});
        else (void)run_autoscene(c,matrices,[work](PcRaceContext& ctx){autoscene_dest_4b6690(ctx,work);});
        return true;}
    void (*fn)(PcRaceContext&,std::uint32_t)=nullptr;unsigned kind=0;   // 1 init 2 control 3 destroy
    switch(callback){
    case 0x488450u:fn=race_robot_control_488450;kind=2;break;          // function 0x66 (the AUTOSCENE robots of 440890)
    case 0x4884d0u:fn=race_robot_driver_init_4884d0;kind=1;break;
    case 0x488dd0u:fn=race_robot_driver_control_488dd0;kind=2;break;
    case 0x487980u:fn=race_robot_driver_destroy_487980;kind=3;break;
    case 0x488710u:fn=race_robot_flagman_init_488710;kind=1;break;
    case 0x488820u:fn=race_robot_flagman_control_488820;kind=2;break;
    case 0x487a00u:fn=race_robot_flagman_destroy_487a00;kind=3;break;
    case 0x4885a0u:fn=race_robot_passenger_init_4885a0;kind=1;break;    // function 0x39 (the passenger)
    case 0x489a90u:fn=race_robot_passenger_control_489a90;kind=2;break;
    case 0x489850u:fn=race_robot_driver_control_489850;kind=2;break;      // function 0x38
    case 0x48ad60u:fn=race_robot_passenger_control_48ad60;kind=2;break;   // function 0x3A
    default:return false;
    }
    auto& r=c.race.robots;
    if(kind==1)++r.inits;else if(kind==2)++r.controls;else ++r.destroys;
    const auto slot=c.event_state.current_slot;
    if(slot>=c.event_state.slots.size()){r.last_error="robot callback outside an event slot";++r.skipped;return true;}
    const std::uint32_t work=c.event_state.slots[slot].work_token;
    run(c,work,matrices,[&](PcRaceContext& ctx){fn(ctx,work);});
    return true;
}
// ---- RobMotion file loads (4F1F90 services) --------------------------------
namespace {
// Services of 4F1F90 for the robot loaders: the files come from the retail
// tree (else the loader pack for the two RobMotion tables); 580253 buffers
// are new heap blocks at `next` (never reallocated while mapped).
struct RobFiles {
    NativeRuntimeContext& c;
    PcRaceMemory& m;
    std::vector<NativeRobMotionTables::Heap>& heaps;
    std::uint32_t& next;
    std::uint32_t end;
    std::string& error;
    std::uint32_t& loads;
    std::vector<const std::vector<std::uint8_t>*> files;
    // Open files: handle token 0x74000001 + index (the handle is only passed
    // back to 423F10/423CB0/423BD0).
    const std::vector<std::uint8_t>& file(std::uint32_t h){
        if(h<0x74000001u||h-0x74000001u>=files.size()||!files[h-0x74000001u])throw std::runtime_error("robot files: bad file handle");
        return *files[h-0x74000001u];}
    std::uint32_t operator()(const PcRaceCall& k){
        const auto* a=k.args.data();
        switch(k.pc){
        case 0x4239c0u:{   // open(name,"rb"): the retail tree (else the loader pack), 0 when absent
            std::string name;for(std::uint32_t p=a[0];;++p){const auto ch=m.u8(p);if(!ch)break;name.push_back(char(ch));}
            const std::vector<std::uint8_t>* bytes=nullptr;std::string err;
            if(auto* retail=c.event_function36.retail_assets)bytes=retail_asset_guest_path(*retail,name,false,&err);
            if(!bytes&&c.event_function36.loader_assets){
                const std::uint32_t id=name=="\\Common\\bone.bin"?LoaderAssetBoneTableId:name=="\\Common\\motdata_table.bin"?LoaderAssetMotionTableId:0u;
                if(id)if(const auto* rec=find_loader_asset(*c.event_function36.loader_assets,id,0u))bytes=&rec->bytes;
            }
            if(!bytes){error="file "+name+" not found";return 0u;}
            files.push_back(bytes);++loads;
            return 0x74000000u+std::uint32_t(files.size());}
        case 0x423f10u:return std::uint32_t(file(a[0]).size());
        case 0x423cb0u:{   // read(buf,size,count,handle): the whole file (4F1F90 reads size x 1)
            const auto& f=file(a[3]);const std::uint64_t n=std::uint64_t(a[1])*a[2];
            if(n>f.size())throw std::runtime_error("robot files: read past the file");
            if(n)std::memcpy(m.at(a[0],std::size_t(n),true),f.data(),std::size_t(n));
            return a[2];}
        case 0x423bd0u:(void)file(a[0]);return 0u;
        case 0x580253u:{   // malloc: a new owner buffer (once per load, never per frame)
            const std::uint32_t base=next,size=a[0];
            if(size==0u||size>HeapLimit||base+std::uint64_t(size)>end)throw std::runtime_error("robot files: heap exhausted");
            next=(base+size+0xfffu)&~0xfffu;
            heaps.push_back({base,std::vector<std::uint8_t>(size)});
            m.map(base,heaps.back().bytes.data(),size);
            return base;}
        // 7B11A8 / 7B1194 allocator selector stacks: owned by the AREA runtime
        case 0x440d10u:driving::push_alloc_state_a_440d10(native_race_area(c).alloc_stacks,a[0]);return 0u;
        case 0x440d30u:return driving::pop_alloc_state_a_440d30(native_race_area(c).alloc_stacks);
        case 0x440d50u:driving::push_alloc_state_b_440d50(native_race_area(c).alloc_stacks,a[0]);return 0u;
        case 0x440d70u:return driving::pop_alloc_state_b_440d70(native_race_area(c).alloc_stacks);
        default:throw RobotMissing(k.pc);
        }
    }
};
}
// ---- RobMotion system init 4F2470 (rob_motion_tables.hpp) -------------------
void native_rob_motion_tables_init(NativeRuntimeContext& c){
    auto& t=c.race.robots.motion_tables;
    ++t.init_calls;t.ready=false;t.fault=0u;t.error.clear();
    PcRaceMemory m;
    t.state.map(m);rob_motion_map_rdata(m);
    for(auto& h:t.heaps)m.map(h.base,h.bytes.data(),h.bytes.size());
    RobFiles files{c,m,t.heaps,t.heap_next,NativeRobMotionTables::HeapEnd,t.error,t.loads,{}};
    PcRaceService service=[&files](const PcRaceCall& k){return files(k);};
    try{rob_motion_init_4f2470(m,service);t.ready=true;}
    catch(const RobotMissing& e){t.fault=e.pc;char s[64];std::snprintf(s,sizeof s,"service %06X not ported",e.pc);t.error=s;}
    catch(const PcRaceUnmapped& e){t.fault=e.address;t.error=(t.error.empty()?std::string():t.error+": ")+e.what();}
    catch(const std::exception& e){t.fault=0xffffffffu;t.error=e.what();}
}
// ---- motion groups 4F2020 / 4F2060 -------------------------------------------
bool native_rob_motion_group_request_4f2020(NativeRuntimeContext& c,std::uint32_t group,std::uint32_t mode){
    auto& t=c.race.robots.motion_tables;
    if(!t.ready){t.error="4F2020 before the RobMotion tables";return false;}
    PcRaceMemory m;
    t.state.map(m);for(auto& h:t.heaps)m.map(h.base,h.bytes.data(),h.bytes.size());
    const std::uint32_t slot=0x84d978u+group*16u;
    if(group>=0x3fu)throw std::out_of_range("4F2020: group outside the 63 slots");
    // 4F2130: once no slot is pending, the scheduler stops loading ([84DE64] = 0, 4F21B0 true).
    auto settle=[&]{for(std::uint32_t g=0;g<0x3fu;++g)if(m.u32(0x84d97cu+g*16u)!=2u)return;m.put32(0x84de64u,0);};
    // 4F2020: an idle slot (state 2) becomes a request {0, 0, 0, mode}.
    if(m.u32(slot+4)!=2u)return true;
    m.put32(slot,0);m.put32(slot+4,0);m.put32(slot+8,0);m.put32(slot+0xc,mode);m.put32(0x84de64u,1);
    // 4F2060 state 0: nothing to load past the table or when already loaded.
    if(std::int32_t(group)>=std::int32_t(m.u32(0x84de70u))||m.u32(0x84dd68u+group*4u)!=0u){m.put32(slot+4,2);settle();return true;}
    std::string name="\\Anims\\";
    for(std::uint32_t p=m.u32(m.u32(0x84d970u)+group*16u);;++p){const auto ch=m.u8(p);if(!ch)break;name.push_back(char(ch));}
    std::string err;const std::vector<std::uint8_t>* bytes=nullptr;
    if(auto* retail=c.event_function36.retail_assets)bytes=retail_asset_guest_path(*retail,name,true,&err);
    if(!bytes){t.error="motion group "+name+" unreadable: "+err;return false;}
    // 44F980: payload after the .sz size word, then the {payload, mode} handle.
    if(bytes->size()<4u){t.error=name+": short .sz";return false;}
    const std::uint32_t size=std::uint32_t(bytes->size()-4u);
    const std::uint32_t base=t.heap_next;
    if(base+std::uint64_t(size)+8u>NativeRobMotionTables::HeapEnd){t.error="RobMotion heap exhausted";return false;}
    t.heap_next=(base+size+8u+0xfffu)&~0xfffu;
    t.heaps.push_back({base,std::vector<std::uint8_t>(size+8u)});
    auto& h=t.heaps.back().bytes;std::memcpy(h.data(),bytes->data()+4,size);
    const std::uint32_t words[2]={base,mode};std::memcpy(h.data()+size,words,8);
    // 4F2060 state 1 -> 44FC60: 84DD68[g] = payload, slot +0 = handle, state 2.
    m.put32(0x84dd68u+group*4u,base);m.put32(slot,base+size);m.put32(slot+8,0);m.put32(slot+4,2);
    ++t.groups_loaded;
    settle();
    return true;
}
// 4F21C0(group): the group's buffer released (440CD0 on slot +0, 84DD68[g] =
// 0) and the slot idle again {+4 = 2, +8 = 0, +C = 4}. The heap buffer stays
// reserved (its guest range is never reused).
bool native_rob_motion_group_release_4f21c0(NativeRuntimeContext& c,std::uint32_t group){
    auto& t=c.race.robots.motion_tables;
    if(!t.ready){t.error="4F21C0 before the RobMotion tables";return false;}
    if(group>=0x3fu)throw std::out_of_range("4F21C0: group outside the 63 slots");
    PcRaceMemory m;
    t.state.map(m);
    const std::uint32_t slot=0x84d978u+group*16u;
    if(m.u32(slot)){m.put32(slot,0);m.put32(0x84dd68u+group*4u,0);}
    m.put32(slot+4,2);m.put32(slot+8,0);m.put32(slot+0xc,4);
    return true;
}
// ---- character files 488B80 (rob_disp.hpp) ------------------------------------
void native_rob_chr_load_488b80(NativeRuntimeContext& c){
    auto& r=c.race.robots;
    ++r.chr_loads;r.chr_error.clear();r.chr_fault=0u;
    PcRaceMemory m;
    r.state.map(m);race_robots_map_tables(m);rob_disp_map_rdata(m);
    for(auto& h:r.chr_heaps)m.map(h.base,h.bytes.data(),h.bytes.size());
    std::uint32_t files_loaded=0;
    RobFiles files{c,m,r.chr_heaps,r.chr_heap_next,NativeRaceRobots::ChrHeapEnd,r.chr_error,files_loaded,{}};
    PcRaceService service=[&files](const PcRaceCall& k){return files(k);};
    try{rob_chr_load_488b80(m,service);r.chr_ready=true;r.chr_files=files_loaded;}
    catch(const RobotMissing& e){r.chr_fault=e.pc;char s[64];std::snprintf(s,sizeof s,"service %06X not ported",e.pc);r.chr_error=s;}
    catch(const PcRaceUnmapped& e){r.chr_fault=e.address;r.chr_error=(r.chr_error.empty()?std::string():r.chr_error+": ")+e.what();}
    catch(const std::exception& e){r.chr_fault=0xffffffffu;r.chr_error=e.what();}
}
bool native_race_robots_display(NativeRuntimeContext& c,std::uint32_t callback,std::uint32_t work,driving::PcMatrixStack& matrices){
    void (*fn)(PcRaceContext&,std::uint32_t)=nullptr;
    if(callback==0x4b6a30u){   // AUTOSCENE display: its 405360 leaves on the renderer with their matrices
        auto& r=c.race.robots;
        if(work!=NativeRaceRobots::AutosceneWorkBase||!r.renderer){r.autoscene_error="AUTOSCENE display without its work or a renderer";++r.autoscene_skipped;return true;}
        std::vector<PcVehicleDrawCall> draws;
        if(!run_autoscene(c,matrices,[work](PcRaceContext& ctx){autoscene_display_4b6a30(ctx,work);},&draws))return true;
        auto& rr=*r.renderer;auto& q=rr.queue_context();
        driving::pc_matrix_push(q.matrices);
        for(const auto& d:draws){
            if(d.pc!=0x405360u){driving::pc_matrix_pop(q.matrices);r.autoscene_fault=d.pc;r.autoscene_error="AUTOSCENE leaf not handled";return true;}
            const auto& a=d.args;
            if(a[0]==0xffffffffu)continue;
            if((a[0]>>16)<0x223u&&!native_race_bank_ensure(c,rr,a[0]>>16))continue;          // not resident: 448810 +0C = 0
            for(unsigned b=0;b<64;++b)q.matrices.current().put8(b,d.matrix[b]);
            render_object_405360(q,a[0],std::int32_t(a[1]),driving::Bytes(nullptr,0),a[3],std::int32_t(a[4]),std::int32_t(a[5]));
            ++r.autoscene_draws;
        }
        driving::pc_matrix_pop(q.matrices);
        return true;}
    if(callback==0x4879b0u)fn=race_robot_driver_display_4879b0;
    else if(callback==0x4879d0u)fn=race_robot_flagman_display_4879d0;
    else if(callback==0x487880u)fn=race_robot_display_487880;         // function 0x66 (the AUTOSCENE robots)
    else return false;
    ++c.race.robots.displays;
    run(c,work,matrices,[&](PcRaceContext& ctx){fn(ctx,work);});
    return true;
}
// 487BE0(out, work, bone): the bone's world position (the work's matrix +10
// times the motion's bone matrix, 4F1EC0); false (out untouched) without
// such a bone.
bool native_robot_bone_position_487be0(NativeRuntimeContext& c,std::uint32_t work,std::uint32_t bone,
                                       driving::PcMatrixStack& s,driving::CourseProbe& out){
    auto& r=c.race.robots;
    map_memory(c,r,false);
    auto& m=r.memory;
    const std::uint32_t motion=PcRaceRobotState::MotionBase+m.u32(work)*PcRaceRobotState::MotionSize;
    const std::uint32_t index=bone&0x7fffu;
    if(!(std::int32_t(std::int16_t(m.u16(motion+0x68)))>std::int32_t(index))||(bone&0x8000u))return false;
    driving::pc_matrix_push_load(s,m.bytes(work+0x10,64));
    driving::pc_matrix_multiply_current(s,m.bytes(m.u32(motion+0x74)+index*0x350u,64));
    out=driving::pc_matrix_translation(s);
    driving::pc_matrix_pop(s);
    return true;
}
}
