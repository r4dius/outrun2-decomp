#include <cstddef>
#include "system/dev_hooks.hpp"
#include "system/perf.hpp"
#include <cstdlib>
#include "platform/bulk_fallback.hpp"
#include "platform/race_area_runtime.hpp"
#include "platform/race_robots_runtime.hpp"
#include "platform/race_hud_runtime.hpp"
#include "platform/sprite_2d_runtime.hpp"
#include <chrono>
#include "platform/race_end_runtime.hpp"
#include "platform/race_traffic_runtime.hpp"
#include "platform/translated_crt.hpp"   // the request manager's heap
#include "platform/native_race_effects.hpp"
#include "platform/native_runtime.hpp"
#include "platform/pc_scene_renderer.hpp"
#include "platform/pc_address_view.hpp"
#include "platform/retail_asset_store.hpp"
#include "platform/race_manager_runtime.hpp"   // event-359 owner (race_manager hook)
#include "driving/pc_crash.hpp"
#include "driving/pc_wall_rebound.hpp"
#include <cstdio>
#include <cstring>
#include <sstream>
namespace outrun::platform {
// START scene-owner world loaders (native_runtime.cpp): the same PC
// functions, owned by the runtime (lane state machines on START state).
std::uint32_t native_scene_owner_collision_43dba0(NativeRuntimeContext&,std::uint32_t,std::uint32_t);
std::uint32_t native_scene_owner_environment_44aa80(NativeRuntimeContext&,std::uint32_t,std::uint32_t,std::uint32_t);
std::uint32_t native_scene_owner_object_4f10d0(NativeRuntimeContext&,std::uint32_t);
std::uint32_t native_scene_owner_static_tables(NativeRuntimeContext&,std::uint32_t,std::uint32_t);
std::uint32_t native_scene_owner_environment_reset_44a080(NativeRuntimeContext&);   // course-prog: 44A1A0 tail
namespace {
using driving::Bytes;
constexpr std::uint32_t None=0xffffffffu;
std::string hex(std::uint32_t v){char t[16];std::snprintf(t,sizeof t,"%x",v);return t;}
std::uint32_t le32(const std::uint8_t* p){std::uint32_t v;std::memcpy(&v,p,4);return v;}
// Services whose EAX the AREA/SKY modules never use (every call site is a
// statement): when not ported they are counted and the callback continues.
bool void_service(std::uint32_t pc){
    switch(pc){
    case 0x407c30:case 0x407e40:case 0x4103a0:case 0x4103f0:case 0x4249f0:case 0x4278c0:case 0x429920:case 0x4299c0:
    case 0x42deb0:case 0x42dfb0:case 0x43de50:case 0x4401d0:case 0x440cd0:case 0x440d10:case 0x448990:case 0x448ad0:
    case 0x44a1a0:case 0x44fc60:case 0x4502d0:case 0x450310:case 0x451dd0:case 0x46c260:case 0x46c2c0:case 0x46c360:
    case 0x46c4c0:case 0x46fc30:case 0x4af550:case 0x4af560:case 0x4af570:case 0x4af580:case 0x4af590:case 0x4afb40:
    case 0x4afb50:case 0x4afb70:case 0x4afb80:case 0x4afd90:case 0x4ef850:case 0x4ef860:case 0x4efaf0:case 0x4efb50:
    case 0x4efd20:case 0x4f03a0:case 0x4f0600:case 0x4f0d10:case 0x4f11b0:case 0x4f21c0:
        return true;
    default:return false;
    }
}
// 450xxx / 44FF10: race-manager words 7D39xx (event 359 owner).
bool race_manager_service(std::uint32_t pc){
    switch(pc){
    case 0x4502c0:case 0x4502d0:case 0x450300:case 0x450310:case 0x450380:case 0x450130:case 0x44ff10:case 0x450630:
        return true;
    default:return false;
    }
}
struct Service {
    NativeRuntimeContext& c;
    NativeRaceAreaRuntime& o;
    PcSceneRenderer* r;
    PcRaceContext* ctx{};
    void routed(std::uint32_t pc){if(o.routed[pc]++==0)o.routed_order.push_back({pc,o.frame});}
    [[noreturn]] void missing_value(std::uint32_t pc,const std::string& why){
        count_missing(pc,why);
        throw PcRaceMissingService(pc,"PC "+hex(pc)+": "+why);
    }
    void count_missing(std::uint32_t pc,const std::string& why){
        if(o.missing[pc]++==0)o.missing_order.push_back({pc,o.frame});
        if(!o.fault_pc){o.fault_pc=pc;o.fault="PC "+hex(pc)+": "+why;}
    }
    // 4249F0: a sound effect through the runtime's effect player (and recorded).
    void effect_4249f0(std::uint32_t id){
        o.sound_effects_4249f0.push_back(id);++o.recorded[0x4249f0];
        auto& f=c.event_function36;
        if(f.frontend_effect)(void)f.frontend_effect(f.frontend_effect_user,id);
    }
    // Void service without a port: counted, the module continues.
    std::uint32_t missing(std::uint32_t pc,const std::string& why){
        if(!void_service(pc))missing_value(pc,why);
        count_missing(pc,why);return 0u;
    }
    std::string exe_string(std::uint32_t a){
        std::string s;for(std::uint32_t i=0;i<260u;++i){const auto ch=o.memory.u8(a+i);if(!ch)return s;s.push_back(char(ch));}
        throw std::runtime_error("PC string at "+hex(a)+" not terminated");
    }
    RetailAssetStore* retail(){return c.event_function36.retail_assets;}
    // Resources: START's requests (49B390 mode 8/9/10, 49B3F0 448AD0) and
    // this module's 448AD0 requests, read synchronously from the retail
    // tree (the runtime's resource model, as START's 448980 gate).
    bool start_resident(std::uint32_t id){
        const auto& s=c.start_mode;
        for(std::uint32_t k=0;k<s.scene_owner_resource_count&&k<s.scene_owner_resource_ids.size();++k)if(s.scene_owner_resource_ids[k]==id)return true;
        for(auto v:s.scene_owner_list_836850)if(v==id)return true;
        return false;
    }
    bool resident(std::uint32_t id){
        if(o.released.count(id))return false;
        return start_resident(id)||o.requested.count(id)!=0u;
    }
    std::string resource_path(std::uint32_t id){
        if(id>=0x223u)return {};
        const auto p=o.memory.u32(0x633558u+id*4u);
        return p?exe_string(p):std::string();
    }
    // 4F0910 / 4F0CB0 / 43D470 helpers
    std::uint16_t course_end(unsigned type){
        const auto tables=c.start_mode.scene_owner_course_world.tables();
        driving::PcCourseEndView v{std::nullopt,tables.courses[type].runs.lengths};
        if(tables.courses[type].runs.present)v.header=tables.courses[type].runs.header;
        return driving::pc_course_end_position(v);
    }
    // 44FD80(path, mode): an async slot for the file; the transfer is done
    // at once (the retail tree is read synchronously), state 4.
    std::uint32_t submit_44fd80(std::uint32_t path,std::uint32_t mode){
        const auto name=exe_string(path);
        for(std::uint32_t k=0;k<RaceAreaSlotCount;++k)if(o.slots[k].pc.word0&&o.slots[k].path==path)return RaceAreaSlotBase+k*0x30u; // 44F8B0
        std::uint32_t k=0;while(k<RaceAreaSlotCount&&o.slots[k].pc.word0)++k;
        if(k==RaceAreaSlotCount)missing_value(0x44fd20u,"no free async slot (44F920 returns 0: the PC then returns a null slot)");
        auto* store=retail();if(!store)missing_value(0x44fd80u,"no retail store for "+name);
        const bool sz=name.size()>3&&name.compare(name.size()-3,3,".sz")==0;
        std::string error;
        const auto* bytes=retail_asset_guest_path(*store,name,sz,&error);
        if(!bytes)missing_value(0x44f990u,"file "+name+" unreadable: "+error);
        const auto base=RaceAreaAllocationBase+(o.next_allocation++%0x1eu)*RaceAreaAllocationWindow;
        std::vector<std::uint8_t> image;
        if(!race_area_loader_image(*bytes,sz,base,mode,image,error))missing_value(0x44f990u,name+": "+error);
        if(image.size()>RaceAreaAllocationWindow)missing_value(0x44f990u,name+": larger than the synthetic window");
        o.allocations[base]=std::move(image);
        auto& s=o.slots[k];s=NativeRaceAreaRuntime::Slot{};
        s.pc.word0=1;s.pc.state=4;s.path=path;s.mode=mode;s.pc.dest_24=base;s.pc.size_28=0;s.name=name;   // request {path,0,0,..,mode}
        s.pc.handle_14=base+std::uint32_t(o.allocations[base].size()-8u);
        return RaceAreaSlotBase+k*0x30u;
    }
    NativeRaceAreaRuntime::Slot* slot(std::uint32_t a){
        if(a<RaceAreaSlotBase||a>=RaceAreaSlotBase+RaceAreaSlotCount*0x30u||(a-RaceAreaSlotBase)%0x30u)return nullptr;
        auto& s=o.slots[(a-RaceAreaSlotBase)/0x30u];return s.pc.word0?&s:nullptr;
    }
    std::uint32_t operator()(const PcRaceCall& k){
        const auto pc=k.pc;const auto& a=k.args;
        auto& start=c.start_mode;
        switch(pc){
        // ---- SCN_EFC work setters (44BD50 weather flags): event 387's work 79F5EC
        case 0x4af550:case 0x4af560:case 0x4af570:case 0x4af580:case 0x4af590:case 0x4afb40:case 0x4afb50:
        case 0x4afb70:case 0x4afb80:case 0x4afba0:case 0x4afd90:case 0x4afb60:case 0x4af5a0:case 0x4afb90:{
            if(c.event_state.slots[387].work_token==0u)return missing(pc,"SCN_EFC (event 387) work not open");
            std::uint32_t eax=0;
            try{if(native_race_effects_setter(c,k,eax)){routed(pc);return eax;}}
            catch(const std::exception& e){return missing(pc,std::string("SCN_EFC setter: ")+e.what());}
            return missing(pc,"SCN_EFC setter");}
        // ---- sky helpers (race_sky.cpp) ---------------------------------
        case 0x451dd0:routed(pc);race_sky_reset_451dd0(*ctx);return 0u;
        case 0x451e90:routed(pc);return race_sky_step_451e90(*ctx);
        // ---- globals / tiny leaves ----------------------------------------
        case 0x40ecb0:routed(pc);return c.frame_state.frame_counter_95af0c;          // bridge 40E926: EAX=[95AF0C] (measured)
        // 45AEE0: [7F1960] == 2, the async loader 7F1958 (45AE40) idle. Bootstrap 45AE10 sets
        // state 2; its starters (45AF80/45AF90 of 481947/4B0514, 45AE40 state 0) are not
        // reached natively in GAME, so the state is the race end owner's copy (2) or 2.
        case 0x45aee0:{routed(pc);std::uint32_t st=2u;
            if(c.race_end)std::memcpy(&st,c.race_end->state.adv_7f1958.data()+8,4);
            return st==2u?1u:0u;}
        case 0x49b2d0:routed(pc);return std::uint16_t(c.game_mode.start_countdown);    // word 8367BC
        case 0x4872e0:routed(pc);return driving::runtime_external_block_4872e0(std::uint32_t(c.race.camera_override.override_82e7d8));
        case 0x487320:{
            // Course section seen by the camera override (82E7E4): 0 the player
            // car (44B8D0), 1 the camera eye (79F574 +F8: 43EB60 polygon, else the
            // nearest polygon centre of type 0), 3 the scripted 82E7DC; else FFFF.
            routed(pc);
            const auto& co=c.race.camera_override;
            const auto& car=c.event_function36.car_select.car_799d18;
            switch(co.w82e7e4){
            case 0:{
                std::uint32_t kind;std::memcpy(&kind,car.data()+0x5c,4);
                if(kind)return o.memory.u32(0x7d3064u)?0u:course_end(0);
                std::uint16_t cs;std::memcpy(&cs,car.data()+0x64,2);return cs;}
            case 1:{
                if(!r)missing_value(pc,"camera course query needs the renderer matrices");
                const auto& cam=c.event_function36.car_select.camera_79fe10;
                float e[3];std::memcpy(e,cam.data()+0xf8,12);
                const auto tables=c.start_mode.scene_owner_course_world.tables();
                driving::CourseWorldQuery q{tables,r->matrices(),c.event_function36.car_select.race_prediction};
                driving::CourseProbe p{e[0],e[1],e[2]};
                std::uint32_t poly=0,kind=0;
                (void)driving::get_y_position_prog(q,0x100u,p,&poly,nullptr,&kind);
                const auto& t0=tables.courses[0];
                auto length=[&](std::uint32_t i)->std::uint32_t{return i==0xffffffffu?0u:std::uint16_t(t0.runs.lengths.i16(std::size_t(i)*2u));};   // 401810(0, i)
                if(kind!=1u)return length(poly);
                if(!t0.polygons_present||t0.polygons.size()==0||!t0.runs.present||t0.runs.header.size()==0)return 0xffffu;
                const std::int32_t n=t0.runs.header.i32(0xc);
                std::uint32_t best=0xffffu;float min=3.40282347e+38f;
                for(std::int32_t i=0;i<n;++i){
                    const std::size_t at=std::size_t(i)*0x40u+0x30u;
                    const float dx=t0.polygons.f32(at)-e[0],dy=t0.polygons.f32(at+4)-e[1],dz=t0.polygons.f32(at+8)-e[2];
                    const float d=(dz*dz+dy*dy)+dx*dx;
                    if(!(min<d)&&!(d!=d)){best=length(std::uint32_t(i));min=d;}
                }
                return best;}
            case 3:return std::uint32_t(std::uint16_t(co.w82e7dc));
            default:return 0xffffu;
            }}
        case 0x43d470:routed(pc);if(a[0]>3u)missing_value(pc,"course type outside 0..3");return course_end(a[0]);
        case 0x451350:{
            // ---- race manager binding: 451350 (any index, incl. <0 = 7D399C..) on the event-359 owner: begin ----
            std::uint32_t eax{};
            if(o.race_manager&&o.race_manager(k,eax)){routed(pc);return eax;}
            missing_value(pc,"451350 index "+std::to_string(std::int32_t(a[0]))+": race manager hook unset");}
            // ---- race manager binding: end ----
        case 0x48b310:routed(pc);return start.flag_830394;
        case 0x48b350:routed(pc);return driving::platform_counter_lt_60_48b350(std::int32_t(start.frontend_prepare.output_code_656234))?1u:0u; // [656234] (4EEC80 output)
        case 0x4957f0:routed(pc);return start.selection_active_836374?1u:0u;
        case 0x495490:routed(pc);return c.race.car_world.gate_8361b4;
        case 0x43f960:routed(pc);return race_preset_43f960(start.course_preset);
        case 0x46c360:routed(pc);driving::runtime_course_force_mode_46c360(c.game_mode.course_runtime,a[0]);return 0u;
        case 0x4401d0:routed(pc);driving::event_close_4401d0(c.event_state,a[0]);return 0u;
        case 0x440d10:routed(pc);driving::push_alloc_state_a_440d10(o.alloc_stacks,a[0]);return 0u;
        case 0x440d30:routed(pc);return driving::pop_alloc_state_a_440d30(o.alloc_stacks);
        case 0x440d50:routed(pc);driving::push_alloc_state_b_440d50(o.alloc_stacks,a[0]);return 0u;
        case 0x440d70:routed(pc);return driving::pop_alloc_state_b_440d70(o.alloc_stacks);
        case 0x4249f0:routed(pc);effect_4249f0(a[0]);return 0u;
        case 0x44bc60:{   // 44BC60(n): below 0x154, the sound 0x88 every 100 and 0x87 every 160 (4249F0)
            routed(pc);const std::int32_t n=std::int32_t(a[0]);
            if(n<0x154){
                if(n%100==0)effect_4249f0(0x88u);
                if(n%160==0)effect_4249f0(0x87u);
            }
            return 0u;}
        case 0x45c470:case 0x45dfb0:case 0x45e000:case 0x47fbd0:{   // the navi module's values (race_hud_navi)
            std::uint32_t eax{};std::string why;
            if(!native_race_hud_service(c,pc,a.data(),k.argc,eax,why))missing_value(pc,why);
            routed(pc);return eax;}
        case 0x46c500:   // 46C500: 55A930 (LAN session) and [7F94C8] == 1; offline 0 without the read
            routed(pc);
            if(c.start_mode.manager_state_7f94c0==0u)return 0u;
            if(!o.memory.mapped(0x7f94c8u,4))missing_value(pc,"LAN session word 7F94C8 not mapped");
            return o.memory.u32(0x7f94c8u)==1u?1u:0u;
        case 0x4278c0:routed(pc);c.race.sound_commands.push_back(a[0]);c.pc_sound.unload_4278c0(a[0]);return 0u;
        case 0x427700:routed(pc);return c.pc_sound.request_427700(a[0]);
        case 0x4103a0:{  // 89EDBC = a0, 89EDC0/89EDC4 from the flag bits
            routed(pc);o.shader_mode_89edbc=race_shader_mode_4103a0(a[0],a[1]);
            if(r)r->shader_globals.mode_89edbc=o.shader_mode_89edbc[0];
            return 0u;}
        case 0x4103f0:{  // material shader records of one bank object (4104D0 per material)
            if(!r)return missing(pc,"no renderer (bank objects)");
            if((a[0]>>16)<0x223u)(void)ensure_bank(a[0]>>16);   // not resident: 448810 +0C = 0, 4103F0 returns 0
            routed(pc);return r->object_shaders_4103f0(a[0],a[1]);}
        case 0x407c30:case 0x407e40:{  // sky init: light words of 899B98 / 89A138 / 899D78
            if(!r)return missing(pc,"no renderer (light tables 899B98)");
            routed(pc);
            auto& lights=r->environment().lights_899b98;
            Bytes l(lights.data(),lights.size());
            if(pc==0x407c30u)race_light_word_407c30(l,a[0],a[1],a[2],a[3]);
            else race_light_words_407e40(l,a[0],a[1],a[2],a[3],a[4],a[5]);
            return 0u;}
        case 0x406780:{  // mesh records of the object with (flags & mask)
            if(a[0]==None){routed(pc);return 0u;}
            if(!r)missing_value(pc,"no renderer (bank objects)");
            if(!ensure_bank(a[0]>>16)){routed(pc);return 0u;}                 // 448810 +0C = 0: loop skipped
            const auto* bank=r->queue_context().bank(a[0]>>16);
            const auto index=a[0]&0xffffu;
            if(!bank||index>=bank->object_count){routed(pc);return 0u;}
            routed(pc);
            return race_mesh_count_406780(bank->system,bank->object_count,a[0],a[1]);}
        // ---- resources (7C2800 model) ------------------------------------
        case 0x448ad0:{
            routed(pc);
            if(a[0]>=0x223u)return 0u;
            const auto path=resource_path(a[0]);
            std::string error;
            if(path.empty()||!retail()||!retail_asset_guest_path(*retail(),path,true,&error))
                return missing(0x448ad0u,"resource "+hex(a[0])+" path "+path+" unreadable");
            o.requested.insert(a[0]);o.released.erase(a[0]);
            queue_bank(a[0]);return 0u;}
        case 0x4299c0:case 0x42dfb0:routed(pc);return 0u;   // 2D / model bank release (area end): the runtime keeps its banks resident
        case 0x4f21c0:routed(pc);if(!native_rob_motion_group_release_4f21c0(c,a[0]))missing_value(pc,c.race.robots.motion_tables.error);return 0u;
        case 0x448990:routed(pc);if(a[0]<0x223u){o.released.insert(a[0]);o.requested.erase(a[0]);}return 0u;
        case 0x448960:routed(pc);return resident(a[0])?1u:0u;
        case 0x448980:routed(pc);return 1u;                                 // requests complete synchronously
        case 0x448820:routed(pc);return resident(a[0])?1u:0u;               // +10 == 1 && +14 == 7
        case 0x448810:{
            if(!r)missing_value(pc,"no renderer (bank object count)");
            routed(pc);if(!ensure_bank(a[0]))return 0u;
            const auto* bank=r->queue_context().bank(a[0]);return bank?bank->object_count:0u;}
        // ---- async loader (44FD80 mode 7 / 44F880 / 44FC60) ------------------
        case 0x44fd80:routed(pc);return submit_44fd80(a[0],a[1]);
        case 0x44f880:{routed(pc);const auto* s=slot(a[0]);
            return driving::pc_async_slot_finished_44f880(s!=nullptr,s?s->pc.word0:0u,s?std::int32_t(s->pc.state):0);}
        case 0x44fc60:{
            routed(pc);auto* s=slot(a[0]);
            if(!s)return 0u;                                               // null / free slot: 44FBE0 returns
            std::uint32_t x{},y{},h{};
            const auto freed=race_async_take_44fc60(s->pc,a[1]?&x:nullptr,a[2]?&y:nullptr,a[3]?&h:nullptr);
            if(freed){const auto p=o.memory.u32(freed);o.allocations.erase(p);}   // 440CD0 of the untaken handle
            if(a[1])o.memory.put32(a[1],x);if(a[2])o.memory.put32(a[2],y);if(a[3])o.memory.put32(a[3],h);
            *s=NativeRaceAreaRuntime::Slot{};
            return 0u;}
        case 0x440cd0:{   // free *p (handle {payload, mode}), *p = 0
            routed(pc);
            const auto h=o.memory.u32(a[0]);
            if(h){
                const auto mode=o.memory.u32(h+4),payload=o.memory.u32(h);
                driving::push_alloc_state_a_440d10(o.alloc_stacks,(mode==2u||mode==3u)?1u:0u);
                o.allocations.erase(payload);
                (void)driving::pop_alloc_state_a_440d30(o.alloc_stacks);
                o.memory.put32(a[0],0u);
            }
            return 0u;}
        // ---- START world lanes (same native owners as 49BA80) --------------
        case 0x43dba0:routed(pc);return native_scene_owner_collision_43dba0(c,a[0],a[1]);
        case 0x44aa80:routed(pc);return native_scene_owner_environment_44aa80(c,a[0],a[1],a[2]);
        // ---- course-prog: stage-object lanes (84CE70.. owner below; START's lane-0 results adopted): begin ----
        case 0x4f10d0:routed(pc);return object_load(false,a[0],a[1]);
        case 0x4f0430:routed(pc);return object_load(true,a[0],a[1]);
        case 0x4f11b0:routed(pc);object_release(false,a[0]);return 0u;
        case 0x4f0600:routed(pc);object_release(true,a[0]);return 0u;
        case 0x4f0d10:routed(pc);return object_open(a[0]);
        case 0x4f0910:routed(pc);return visibility_frame_4f0910(a[0]);
        case 0x4f0cb0:routed(pc);dynamic_open_4f0cb0(a[0],a[1],a[2]);return 0u;
        case 0x4efb50:case 0x4efaf0:return static_table(pc,a[0]);
        case 0x4efd20:return copy_4efd20(a[0],a[1]);
        case 0x4ef860:routed(pc);start.scene_owner_table68_ready=false;return 0u;   // [84CE68] = 0
        case 0x4ef850:routed(pc);start.scene_owner_table6c_ready=false;return 0u;   // [84CE6C] = 0
        // ---- course-prog: area switch services: ----
        case 0x4556f0:
            // [7F1930] == [7F1884] (CommRace focus / local player records).
            // Every writer (4552F0, 455730 via 447FA6 = [7DF34C], 458A60,
            // 45A310, 45A840) runs only with the network manager 7DF34C;
            // offline both words stay .bss 0.
            if(start.network_manager_7df34c_present)missing_value(pc,"7F1930/7F1884 with a network manager");
            routed(pc);return 1u;
        // 455B80 / 455BA0 / 455BD0(e): bits of the focused player's record [7F1930] (+5E + e,
        // +6C + e); 0 when [7DF34C] is clear, as it always is here (the manager is never made).
        case 0x455b80:case 0x455ba0:case 0x455bd0:
            if(start.network_manager_7df34c_present)missing_value(pc,"7F1930 with a network manager");
            routed(pc);return 0u;
        // 46C240 / 46C250 / 46C380 / 46C260 / 46C2C0 / 46C4C0: the C2C request manager 7F9460 of
        // variant 5 (its block and heap are mapped here): the translated originals below.
        case 0x43de50:routed(pc);release_collision_43de50(a[0]);return 0u;
        case 0x44a1a0:routed(pc);native_scene_owner_environment_reset_44a080(c);return 0u;   // 440CD0 x3 then 44A080
        case 0x46c4d0:routed(pc);return resources_481590();
        // 4F03A0(&table, lane) (variant 5, after 46C380): 440CD0 of the lane's handle, then the
        // handle = [table] (the request manager's copy of the lane 0 placements, in its heap).
        case 0x4f03a0:{
            routed(pc);auto& L=object_lane(false,a[1],pc);
            if(L.handle){retire(L.image);L.handle=0u;}
            if(const auto h=o.memory.u32(a[0]))L.handle=h;
            return 0u;}
        // ---- course-prog: end ----
        // 42DEB0 / 429920(resource): request a 2D / SPRANI bank (the final course's flower
        // effect 0x3C): the 2D XST banks below 0x20 load on first use, SPRANI 0x20..0x4A
        // bind their timing at once (synchronous: the next readiness test passes).
        case 0x42deb0:case 0x429920:{
            routed(pc);
            if(a[0]<0x20u&&native_sprite2d_xst_path(a[0])){o.resource_requested=true;return 0u;}
            if(a[0]<0x20u||a[0]>=0x4bu)missing_value(pc,"resource "+hex(a[0]));
            std::string err;
            if(pc==0x429920&&!native_sprani_bind_bank(c,a[0],err))missing_value(pc,err);
            o.resource_requested=true;return 0u;}
        case 0x42df90:case 0x4299a0:
            if(!o.resource_requested&&!native_start_owned_resource_ready(c,pc))missing_value(pc,"readiness not owned");
            routed(pc);return 1u;
        // ---- road tables (46FAC0 / 46FDE0, pc_car_services) ----------------
        case 0x46fc30:routed(pc);return native_race_road_tables_46fc30(c,a[0]);
        case 0x46fe50:routed(pc);return native_race_road_tables_46fe50(c,a[0],a[1]);
        default:break;
        }
        if(race_manager_service(pc)){
            std::uint32_t eax{};
            if(o.race_manager&&o.race_manager(k,eax)){routed(pc);return eax;}
            return missing(pc,"race manager (event 359, 7D39xx) not ported: hook unset");
        }
        {std::uint32_t eax{};auto& t=native_race_traffic(c);   // CRT new / delete of the request manager's code
         if(t.requests_heap&&translated_crt_call(o.memory,*t.requests_heap,k,eax)){routed(pc);return eax;}}
        // No native answer: the bulk translation of the function over the AREA memory
        // (counted and traced once per address by bulk_call).
        if(ctx&&bulk_translated(pc)){routed(pc);return bulk_call(o.memory,ctx->service,k,&ctx->matrices,&c.event_function36.pc_crt_random_state);}
        return missing(pc,"not ported");
    }
    // ---- course-prog: stage-object lanes / area switch: begin ----
    NativeRaceAreaRuntime::ObjectLane& object_lane(bool dynamic,std::uint32_t lane,std::uint32_t pc){
        if(lane>2u)missing_value(pc,"stage-object lane "+std::to_string(lane)+" outside 0..2");
        return dynamic?o.dynamics[lane]:o.placements[lane];
    }
    static std::uint32_t object_base(bool dynamic,std::uint32_t lane){return StageObjectBase+((dynamic?3u:0u)+lane)*StageObjectWindow;}
    // A replaced/released image stays alive until the next memory rebuild
    // (the current callback's mapping may still point at it).
    void retire(std::vector<std::uint8_t>& image){if(!image.empty()){o.retired_images.push_back(std::move(image));image.clear();}}
    // 4F10D0(token, lane) / 4F0430(token, lane) (4F0430 prologue measured:
    // EBX = &84CE7C[lane], EAX = [EBX], flags of EAX-0). State 0: token 0 ->
    // state 2; else 44FD80(token, lane ? 2 : 7) (4F10D0 lane 0 then 4F0BA0
    // closes the 0x134..0x15B events in state 2), state 1, return 1. State
    // 1: 44F880 (the runtime's transfer is synchronous: complete), 44FC60
    // -> handle (84D6CC / 84CE94) and, for 4F0430, payload 84D6C0
    // relocated in place; state 2, return 0. State 2: return 0.
    std::uint32_t object_load(bool dynamic,std::uint32_t token,std::uint32_t lane){
        const std::uint32_t pc=dynamic?0x4f0430u:0x4f10d0u;
        auto& L=object_lane(dynamic,lane,pc);
        if(L.state==0u){
            if(token==0u){L.state=2u;return 0u;}
            const auto name=exe_string(token);
            auto* store=retail();if(!store)missing_value(0x44fd80u,"no retail store for "+name);
            const bool sz=name.size()>3&&(name.compare(name.size()-3,3,".sz")==0||name.compare(name.size()-3,3,".SZ")==0);
            std::string error;
            const auto* bytes=retail_asset_guest_path(*store,name,sz,&error);
            if(!bytes)missing_value(0x44f990u,"file "+name+" unreadable: "+error);
            const std::uint32_t mode=lane==0u?7u:2u,base=object_base(dynamic,lane);
            std::vector<std::uint8_t> image;
            if(!race_area_loader_image(*bytes,sz,base,mode,image,error))missing_value(0x44f990u,name+": "+error);
            if(image.size()>StageObjectWindow)missing_value(0x44f990u,name+": larger than the synthetic window");
            retire(L.image);L.image=std::move(image);L.token=token;L.path=name;L.mode=mode;L.handle=0u;L.payload=0u;
            o.memory.map(base,L.image.data(),L.image.size());
            if(!dynamic&&lane==0u){        // 4F0BA0
                for(std::uint32_t id=0x134u;id<0x15cu;++id)if((c.event_state.slots[id].flags&3u)==2u)driving::event_close_4401d0(c.event_state,id);
            }
            L.state=1u;return 1u;
        }
        if(L.state!=1u)return 0u;
        const std::uint32_t base=object_base(dynamic,lane);
        L.handle=base+std::uint32_t(L.image.size()-8u);
        if(dynamic){
            L.payload=base;
            race_objects_relocate_4f0430(o.memory,L.payload);
        }
        L.state=2u;++L.loads;return 0u;
    }
    // 4F11B0(lane) (prologue measured: EAX = [84D6CC+lane*4]) / 4F0600(lane):
    // 440CD0 of the handle when set, then handle/state/slot (and 84D6C0) = 0;
    // 4F11B0 then closes events [5DA050+lane*8] .. +[5DA054+lane*8] (440330).
    void object_release(bool dynamic,std::uint32_t lane){
        auto& L=object_lane(dynamic,lane,dynamic?0x4f0600u:0x4f11b0u);
        retire(L.image);L.handle=0u;L.payload=0u;L.state=0u;
        if(!dynamic){
            driving::event_close_serial_440330(c.event_state,o.memory.u32(0x5da050u+lane*8u),o.memory.u32(0x5da054u+lane*8u));
            ++o.object_event_ranges_closed;
        }
    }
    static void open_event(void* u,std::uint32_t event,std::uint32_t function){
        auto& sv=*static_cast<Service*>(u);
        if(event>=driving::PcEventSlotCount||function>=driving::PcEventFunctionTableCount)
            sv.missing_value(0x440110u,"event "+std::to_string(event)+" function "+std::to_string(function)+" outside the tables");
        driving::event_setup_440110(sv.c.event_state,event,function,sv.c.event_descriptors,sv.c.event_functions);
        ++sv.o.object_events_opened;
    }
    // 4F0910 / 4F0CB0 over the race manager's 4F0DD0 block (84D6D8..84D8F3: records 84D6D8 +
    // (id-15C)*34, 84D8E0, frame 84D8F0), mapped for the call.
    template<class F> auto with_visibility(F&& f){
        auto& v=c.race.manager.visibility_84cec8;
        const auto mark=o.memory.mark();
        o.memory.map(0x84d6d8u,v.data()+(0x84d6d8u-0x84cec8u),v.size()-(0x84d6d8u-0x84cec8u));
        try{auto r=f();o.memory.release(mark);return r;}catch(...){o.memory.release(mark);throw;}
    }
    std::uint32_t visibility_frame_4f0910(std::uint32_t v){
        return with_visibility([&]{return race_objects_frame_4f0910(o.memory,v);});
    }
    void dynamic_open_4f0cb0(std::uint32_t id,std::uint32_t function,std::uint32_t data){
        if(id<0x15cu||id>=0x15cu+10u)missing_value(0x4f0cb0u,"dynamic object event "+hex(id)+" outside 15C..165");
        (void)with_visibility([&]{race_objects_dynamic_open_4f0cb0(o.memory,id,function,data,&Service::open_event,this);return 0;});
    }
    std::uint32_t object_open(std::uint32_t lane){
        auto& L=object_lane(false,lane,0x4f0d10u);
        race_objects_open_4f0d10(o.memory,lane,L.handle,&Service::open_event,this);
        return 0u;
    }
    // 4EFB50(token) / 4EFAF0(token) (4EFB50 prologue measured: EAX =
    // [84CE68]): once per area, 449A60 copies 0x60 / 0x15E bytes from token
    // to 84BD08 / 84BF60 (START's table68/6c: the same immutable EXE rows,
    // found by token in the stage17 pack) and sets 84CE68 / 84CE6C.
    std::uint32_t static_table(std::uint32_t pc,std::uint32_t token){
        auto& start=c.start_mode;
        const bool t68=pc==0x4efb50u;
        if(t68?start.scene_owner_table68_ready:start.scene_owner_table6c_ready){routed(pc);return 0u;}
        if(token==0u)return missing(pc,t68?"4EF800 default table not ported":"4EF730 default table not ported");
        const auto* pack=c.stage17_assets;
        if(pack)for(const auto& rec:pack->records){
            if(t68&&rec.token68==token){start.scene_owner_table68=rec.table68;start.scene_owner_table68_token=token;start.scene_owner_table68_ready=true;routed(pc);return 0u;}
            if(!t68&&rec.token6c==token){start.scene_owner_table6c=rec.table6c;start.scene_owner_table6c_token=token;start.scene_owner_table6c_ready=true;routed(pc);return 0u;}
        }
        return missing(pc,"table "+hex(token)+" not in the stage17 pack");
    }
    // 4EFD20(e, lane): source rows are EXE .data tables (found by token in
    // the stage17 pack); lane 0 targets 84BD08 (START's table68), other
    // lanes 6A5DC4 (owned words; readers 4EFB90/4EFED0 not ported).
    std::uint32_t copy_4efd20(std::uint32_t e,std::uint32_t lane){
        const auto* pack=c.stage17_assets;
        const std::uint8_t* src=nullptr;std::size_t n=0;
        if(pack)for(const auto& rec:pack->records){
            if(rec.token68==e){src=rec.table68.data();n=rec.table68.size();break;}
            if(rec.token6c==e){src=rec.table6c.data();n=rec.table6c.size();break;}
        }
        if(!src)return missing(0x4efd20u,"row table "+hex(e)+" not in the stage17 pack");
        const auto mark=o.memory.mark();
        o.memory.map_const(e,src,n);
        o.memory.map(0x84bd08u,c.start_mode.scene_owner_table68.data(),c.start_mode.scene_owner_table68.size());
        o.memory.map(0x6a5dc4u,reinterpret_cast<std::uint8_t*>(o.light_words_6a5dc4.data()),24u);
        try{race_objects_copy_4efd20(o.memory,e,lane);}catch(...){o.memory.release(mark);throw;}
        o.memory.release(mark);
        if(lane!=0u)o.light_words_written=true;
        routed(0x4efd20u);return 0u;
    }
    // 43DE50(lane): 440CD0(&780180[lane]) when loaded, then 43DB00(lane)
    // (prologue measured: EAX = [780218+lane*4], ZF): every root word of the
    // lane = 0 (state 780194[lane] included), lane 0 frees 780218 and clears
    // 780190, then 43CC20(lane) resets the query ring 7801A8.. (the car's).
    void release_collision_43de50(std::uint32_t lane){
        if(lane>3u)missing_value(0x43de50u,"collision lane outside 0..3");
        c.start_mode.scene_owner_collision_lane_state[lane]=0u;
        c.start_mode.scene_owner_course_world.release_lane(lane);
        auto& lct=c.event_function36.car_select.race_prediction;       // 7801A8..7801E0 / 780240 / 78023C
        for(unsigned k=0;k<15u;++k)lct.recent[k]=lane;
        lct.cursor=0u;lct.easy=lane;
        ++o.collision_releases;
    }
    // 46C4D0 -> 481590 (ECX = 7F9460, the C2C request manager): +E8 > 1 -> 1; state 0 requests
    // (448AD0 mode 7) the 64F090 resources whose +B0 flag is 1 (set by 482AF0 in variant 5),
    // state 1, return 0; state 1 -> state 2 and 1 once they are all resident (448960).
    std::uint32_t resources_481590(){
        auto& m=o.memory;constexpr std::uint32_t B=0x7f9460u;
        const std::int32_t st=m.i32(B+0xe8u);
        if(st>1)return 1u;
        auto service=[&](std::uint32_t pc,std::uint32_t a0,std::uint32_t a1,std::uint32_t argc){
            PcRaceCall q{};q.pc=pc;q.argc=argc;q.args[0]=a0;q.args[1]=a1;return (*this)(q);};
        if(st==0){
            for(std::uint32_t k=0;k<14u;++k)if(m.u32(B+0xb0u+k*4u)==1u)service(0x448ad0u,m.u32(0x64f090u+k*4u),7u,2u);
            m.put32(B+0xe8u,1u);return 0u;
        }
        if(st==1){
            bool ready=true;
            for(std::uint32_t k=0;k<14u;++k)if(m.u32(B+0xb0u+k*4u)==1u&&!service(0x448960u,m.u32(0x64f090u+k*4u),0u,1u))ready=false;
            if(ready){m.put32(B+0xe8u,2u);return 1u;}
        }
        return 0u;
    }
    // ---- course-prog: end ----
    // Bank of a resource on the renderer: only resident resources (the
    // PC 448810 count of a resource that is not loaded is 0).
    bool bank_bytes(std::uint32_t id,std::vector<std::uint8_t>& pmt,std::string& error){
        if(r->bank_source&&r->bank_source(id,pmt))return true;
        const auto path=resource_path(id);
        const auto* bytes=path.empty()||!retail()?nullptr:retail_asset_guest_path(*retail(),path,true,&error);
        if(bytes){pmt=*bytes;return true;}
        return false;
    }
    // A 448AD0 request starts loading its bank onto the device a little each
    // frame (PcSceneRenderer::step_banks), as the PC loads it in the background.
    void queue_bank(std::uint32_t id){
        if(!r||r->bank_loaded(id)||r->bank_queued(id))return;
        std::vector<std::uint8_t> pmt;std::string error;
        if(bank_bytes(id,pmt,error))(void)r->queue_bank(id,std::move(pmt),error);
    }
    bool ensure_bank(std::uint32_t id){
        if(!r)return false;
        if(r->bank_loaded(id))return true;
        if(!resident(id)){++o.skipped_objects;++o.skipped_ids[id];return false;}
        std::vector<std::uint8_t> pmt;std::string error;
        const bool ok=r->bank_queued(id)||bank_bytes(id,pmt,error);
        if(!ok||!r->load_bank(id,std::move(pmt),error)){
            ++o.bank_failures;o.last_error="bank "+hex(id)+": "+error;
            if(o.missing[0x448810u]++==0)o.missing_order.push_back({0x448810u,o.frame});
            return false;
        }
        o.banks_loaded.insert(id);return true;
    }
};
// ---- memory -------------------------------------------------------------
void map_word(PcRaceMemory& m,std::uint32_t a,void* p,std::size_t n){m.map(a,static_cast<std::uint8_t*>(p),n);}
bool find_table(NativeRuntimeContext& c,NativeRaceAreaRuntime& o){
    OR2_PERF_ZONE("area find_table");
    const auto& rt=c.game_mode.course_runtime;
    o.table=nullptr;o.table_bytes=0;o.table_source.clear();
    if(!rt.selected_copy_active||rt.active_count_7d33c4<=0)return false;
    auto try_source=[&](std::uint8_t* data,std::size_t size,const char* name){
        if(o.table||!data)return;
        const auto off=std::size_t(rt.selected_index)*0x78u;
        if(off+0x78u>size)return;
        // 7D30A8 is a copy of [7D33BC] taken by 44D720; the records it points
        // to are those whose selected entry still carries the same id/links.
        if(std::memcmp(data+off+4,rt.selected_7d30a8.data()+4,4)!=0||std::memcmp(data+off+0x14,rt.selected_7d30a8.data()+0x14,8)!=0)return;
        o.table=data;o.table_bytes=size;o.table_source=name;
    };
    if(rt.records_7d33bc)try_source(rt.records_7d33bc,rt.records_bytes,"44D720 record table (7D33BC)");
    auto& sel=c.mission.manager.selection.course_records;
    try_source(sel.data(),sel.size(),"mission selection (44D720 direct records)");
    if(!o.table){
        // 44DA00 file records: search the course work for the selected record.
        auto& work=c.game_mode.course_work;
        for(std::size_t off=0;!o.table&&off+0x78u<=work.size();off+=4){
            if(std::memcmp(work.data()+off,rt.selected_7d30a8.data(),0x78u)==0){
                const auto start=off-std::size_t(rt.selected_index)*0x78u;
                if(off>=std::size_t(rt.selected_index)*0x78u){o.table=work.data()+start;o.table_bytes=work.size()-start;o.table_source="44DA00 course file";}
            }
        }
    }
    return o.table!=nullptr;
}
void mirror_events(NativeRuntimeContext& c,NativeRaceAreaRuntime& o){
    OR2_PERF_ZONE("area mirror_events");
    Bytes rec(o.event_records_799b30.data(),o.event_records_799b30.size());
    rec.check(0,410u*0x3cu);
    for(std::uint32_t id=0;id<410u;++id){
        const auto& s=c.event_state.slots[id];const auto b=id*0x3cu;
        // descriptor_token..aux38: fifteen consecutive words in record order (+00..+38)
        static_assert(offsetof(driving::PcEventSlot,aux38)-offsetof(driving::PcEventSlot,descriptor_token)==0x38,"PcEventSlot record words");
        std::memcpy(rec.data()+b,&s.descriptor_token,0x3c);
        o.event_flags_79fb48[id]=s.flags;
    }
}
// START's 49BA80 ran 44C2E0/44C310 on the shared block: mirror its lanes
// into 7D306C/7D3070 (handle), 7D2E64/7D2E68 (slot, 0 once done) and
// 7D2E70/7D2E74 (state) whenever START's lane state changes.
void sync_start_lanes(NativeRuntimeContext& c,NativeRaceAreaRuntime& o){
    auto& s=c.start_mode;
    bool changed=o.start_lane_reset_count!=s.scene_owner_course_objects_reset_count;
    for(unsigned l=0;l<2;++l)
        if(o.start_lane_state[l]!=s.scene_owner_course_object_states[l]||o.start_lane_crc[l]!=s.scene_owner_course_object_identities[l].payload_crc32)changed=true;
    if(!changed)return;
    Bytes b(o.area.block.data(),o.area.block.size());
    auto put=[&](std::uint32_t a,std::uint32_t v){b.put32(a-PcRaceAreaState::BlockBase,v);};
    if(o.start_lane_reset_count!=s.scene_owner_course_objects_reset_count){    // 44C2E0
        for(std::uint32_t a:{0x7d306cu,0x7d2e64u,0x7d2e70u,0x7d3070u,0x7d2e68u,0x7d2e74u})put(a,0u);
    }
    for(unsigned l=0;l<2;++l){
        const auto st=s.scene_owner_course_object_states[l];
        put(0x7d2e70u+l*4u,st);
        if(st==2u&&!s.scene_owner_course_objects[l].empty()){
            const auto& raw=s.scene_owner_course_objects[l];
            const auto base=RaceAreaAllocationBase+(o.next_allocation++%0x1eu)*RaceAreaAllocationWindow;
            std::vector<std::uint8_t> image;std::string error;
            if(race_area_loader_image(raw,true,base,7u,image,error)){
                const auto handle=base+std::uint32_t(image.size()-8u);
                o.allocations[base]=std::move(image);
                put(0x7d306cu+l*4u,handle);put(0x7d2e64u+l*4u,0u);
            }else{o.last_error="START lane "+std::to_string(l)+": "+error;}
        }else if(st==2u){put(0x7d306cu+l*4u,0u);put(0x7d2e64u+l*4u,0u);}      // token 0: state 2, no file
        o.start_lane_state[l]=st;o.start_lane_crc[l]=s.scene_owner_course_object_identities[l].payload_crc32;
    }
    o.start_lane_reset_count=s.scene_owner_course_objects_reset_count;
}
void rebuild(NativeRuntimeContext& c,NativeRaceAreaRuntime& o,PcSceneRenderer* r){
    OR2_PERF_ZONE("area rebuild");
    auto& m=o.memory;m.clear();
    o.memory_frame=c.completed_frames+1u;   // race manager binding: rebuilt this frame
    for(std::size_t i=0;i<EmbeddedExeRangeCount;++i)m.map_const(EmbeddedExeRanges[i].base,EmbeddedExeRanges[i].data,EmbeddedExeRanges[i].size);
    native_race_requests_map(c,m);   // the C2C request manager 7F9460 (variant 5) and its heap; the words below shadow it
    o.area.map(m);o.sky.map(m);
    auto& rt=c.game_mode.course_runtime;
    map_word(m,0x635f2cu,&rt.stage_key_635f2c,4);map_word(m,0x635f30u,&rt.stage_value_635f30,4);
    m.map(0x7d2da0u,rt.matrix_7d2da0.data(),64);m.map(0x7d3130u,rt.matrix_7d3130.data(),64);m.map(0x7d3190u,rt.matrix_7d3190.data(),64);
    m.map(0x7d2de0u,rt.selected_7d2de0.data(),0x78);m.map(0x7d30a8u,rt.selected_7d30a8.data(),0x78);m.map(0x7d33d8u,rt.fallback_record_7d33d8.data(),0x78);
    map_word(m,0x7d3124u,rt.zero_7d3124.data(),12);map_word(m,0x7d3178u,rt.zero_7d3178.data(),12);
    map_word(m,0x7d33b0u,&rt.fallback_gate_7d33b0,4);map_word(m,0x7d33c0u,&rt.max_depth_7d33c0,4);map_word(m,0x7d33c4u,&rt.active_count_7d33c4,4);
    map_word(m,0x7f95a8u,&rt.force_mode_7f95a8,4);
    map_word(m,0x7d2e80u,&c.race.area_state_7d2e80,4);map_word(m,0x7d2e88u,&c.race.area_state_7d2e88,4);
    map_word(m,0x7f94c0u,&c.start_mode.manager_state_7f94c0,4);
    m.map(RaceManagerState::base,reinterpret_cast<std::uint8_t*>(&c.race.manager.state),sizeof(RaceManagerState)); // race manager block 7D3650..7D39FF (event-359 owner)
    o.globals_78024c_780258_78026c={c.start_mode.course_preset,c.game_mode.game_variant,c.mode_state.current};
    m.map_const(0x78024cu,reinterpret_cast<const std::uint8_t*>(&o.globals_78024c_780258_78026c[0]),4);
    m.map_const(0x780258u,reinterpret_cast<const std::uint8_t*>(&o.globals_78024c_780258_78026c[1]),4);
    m.map_const(0x78026cu,reinterpret_cast<const std::uint8_t*>(&o.globals_78024c_780258_78026c[2]),4);
    mirror_events(c,o);
    m.map_const(0x799b30u,o.event_records_799b30.data(),o.event_records_799b30.size());
    m.map_const(0x79fb48u,o.event_flags_79fb48.data(),o.event_flags_79fb48.size());
    if(const auto car=c.event_state.slots[8].work_token)m.map(car,c.event_function36.car_select.car_799d18.data(),c.event_function36.car_select.car_799d18.size());
    // Car works of events 9..31 (7815A0 + k*0x10F0): the race manager's
    // storage (Time Attack ghost cars 9..12 drive theirs, race_ghosts_runtime).
    {auto& works=c.race.manager.car_works_7815a0;m.map(NativeRaceManager::CarWorkBase,works.data(),works.size());}
    // Object works of events 0..409 (the traffic module's storage): 44F190 places the dynamic
    // stage objects of events 15C.. (4F0CB0) through their works.
    {auto& t=native_race_traffic(c);m.map(NativeRaceTraffic::ObjectWorkBase,t.object_works.data(),t.object_works.size());}
    if(const auto w=c.event_state.slots[391].work_token)m.map(w,o.sky_work.data(),o.sky_work.size());
    if(find_table(c,o))m.map(RaceAreaTableBase,o.table,o.table_bytes);
    for(auto& [base,bytes]:o.allocations)m.map(base,bytes.data(),bytes.size());
    // 8A8C18..8A8C5C: the 4160F0 glow record (renderer globals, written by
    // PART_EFC 41BC60), read by the sky display 4521C0 (4162B0).
    if(r)m.map(0x8a8c18u,reinterpret_cast<std::uint8_t*>(&r->globals().w(0x8a8c18u)),0x44);
    else if(c.race_effects.glow_8a8c18)m.map(0x8a8c18u,c.race_effects.glow_8a8c18,0x44);
    // ---- course-prog: stage-object lane images: begin ----
    o.retired_images.clear();
    for(unsigned l=0;l<3u;++l){
        for(const auto* L:{&o.placements[l],&o.dynamics[l]})
            if(!L->image.empty())m.map(StageObjectBase+((L==&o.dynamics[l]?3u:0u)+l)*StageObjectWindow,const_cast<std::uint8_t*>(L->image.data()),L->image.size());
    }
    // ---- course-prog: end ----
    if(r){
        const auto v=r->flush_context().g.matrix(0x95dba0u);
        std::memcpy(o.view_inverse_95dba0.data(),v.data(),64);o.view_inverse_valid=true;
        m.map(0x95dba0u,o.view_inverse_95dba0.data(),64);       // copy (409F90 push-load takes a writable view)
    }
}
std::string what_of(const std::exception& e){return e.what();}
// 408C80 colour lists held in the race memory (area .data 63687C/6368B0,
// descriptor +70 lists): NULL-terminated pointer lists of {key, alt...}.
bool race_colour_alt(const PcRaceMemory& m,std::uint32_t list,std::uint32_t key,std::uint32_t index,std::uint32_t& alt){
    for(std::uint32_t p=list;;p+=4){
        const auto entry=m.u32(p);
        if(!entry)return false;
        if(m.u32(entry)==key){alt=m.u32(entry+index*4u);return true;}
    }
}
void hook_colours(NativeRuntimeContext& c,NativeRaceAreaRuntime& o,PcSceneRenderer& r){
    // Installed once per renderer: the hooks reach the AREA owner through
    // the context at call time (the owner is dropped at a race teardown and
    // rebuilt by the next race; a captured owner would dangle).
    o.colour_hooked=true;
    if(c.race_area_hooked_renderer==&r)return;
    {   // 40ED70's morph table into the renderer's 89BDB8 (read by 40ECC0)
        auto& g=r.flush_context().g;auto& anim=g.animation();const auto& st=c.event_function36;
        for(std::size_t i=0;i<0x400u;++i){const auto& e=st.morph_table_89bdb8[i];anim.table[i].base=e[0];anim.table[i].count=e[1];anim.table[i].unknown=e[2];}
        g.animation_values=st.morph_values;
    }
    auto& f=r.flush_context();
    auto previous=f.colour_alt;
    NativeRuntimeContext* ctx=&c;PcSceneRenderer* renderer=&r;
    f.colour_alt=[ctx,previous](std::uint32_t list,std::uint32_t key,std::uint32_t index,std::uint32_t& alt){
        if(auto* a=ctx->race_area.get();a&&a->memory.mapped(list,4))return race_colour_alt(a->memory,list,key,index,alt);
        return previous?previous(list,key,index,alt):false;
    };
    // 408C80 alternative textures of a colour list come from another bank
    // (448810 of alt>>16): load it on the renderer when it is resident.
    auto previous_bank=f.bank;
    f.bank=[ctx,renderer,previous_bank](std::uint32_t id)->PcPmtResources*{
        if(auto* b=previous_bank?previous_bank(id):nullptr)return b;
        if(!ctx->race_area)return nullptr;
        Service sv{*ctx,*ctx->race_area,renderer};
        if(!sv.ensure_bank(id))return nullptr;
        return previous_bank?previous_bank(id):nullptr;
    };
    c.race_area_hooked_renderer=&r;
}
// Draw list of 44F120 / 4521C0 on the renderer (as vehicle_draw_calls_execute).
std::uint32_t execute(NativeRaceAreaRuntime& o,Service& sv,PcSceneRenderer& r,const std::vector<PcVehicleDrawCall>& calls){
    auto& q=r.queue_context();auto& f=r.flush_context();
    std::uint32_t objects=0;
    driving::pc_matrix_push(q.matrices);
    auto load=[&](const PcVehicleDrawCall& call){for(unsigned k=0;k<64;++k)q.matrices.current().put8(k,call.matrix[k]);};
    for(const auto& call:calls){
        const auto& a=call.args;
        ++o.draws_by_pc[call.pc];
        switch(call.pc){
        case 0x405360u:{
            if(a[0]==None)break;
            if(!sv.ensure_bank(a[0]>>16))break;                      // not resident: 448810 +0C = 0
            load(call);
            Bytes nodes(nullptr,0);
            if(a[2]){
                std::size_t n=0;while(o.memory.u16(a[2]+std::uint32_t(n))!=0xffffu)n+=2;
                nodes=Bytes(o.memory.at(a[2],n+2,true),n+2);
            }
            render_object_405360(q,a[0],std::int32_t(a[1]),nodes,a[3],std::int32_t(a[4]),std::int32_t(a[5]));++objects;
            break;}
        case 0x4056d0u:
            if(a[0]==None)break;
            if(!sv.ensure_bank(a[0]>>16))break;
            load(call);render_object_override_4056d0(q,a[0],a[1],a[2],std::int32_t(a[3]));++objects;break;
        case 0x4044e0u:q.lod_threshold_8999b8=std::int32_t(a[0]);break;                 // mov [8999B8],arg
        case 0x4044a0u:o.lod_stack_899560.push_back(q.lod_threshold_8999b8);break;           // [899560+[8999A0]++*4] = [8999B8]
        case 0x4044c0u:
            if(o.lod_stack_899560.empty()){driving::pc_matrix_pop(q.matrices);throw std::runtime_error("4044C0 without a 4044A0");}
            q.lod_threshold_8999b8=o.lod_stack_899560.back();o.lod_stack_899560.pop_back();break;
        case 0x405350u:{
            std::vector<std::uint32_t> order(q.opaque.count);
            for(std::uint32_t k=0;k<q.opaque.count;++k)order[k]=k;
            render_queue_flush_405890(f,q.opaque,order);break;}
        case 0x4044f0u:render_pass_record_4044f0(f.g,a[0],a[1],a[2],a[3],a[4]);break;
        case 0x404540u:render_pass_defaults_404540(f.g,r.frame().layer_7d25f0);break;
        case 0x4052b0u:render_queue_mode_4052b0(q);break;
        case 0x4052c0u:render_queue_flush_4052c0(f,q);break;
        case 0x409df0u:{                                                               // 409DF0(slot): 410F90(current, slot)
            std::array<float,16> mt{};for(unsigned k=0;k<16;++k)std::memcpy(&mt[k],call.matrix.data()+k*4,4);
            render_set_matrix_410f90(f,mt,a[0]);break;}
        case PcRaceSetRenderState:f.device.set_render_state(a[0],a[1]);break;           // device 89BD60 vtable +E4
        default:driving::pc_matrix_pop(q.matrices);throw std::runtime_error("race draw list leaf "+hex(call.pc)+" not handled");
        }
    }
    driving::pc_matrix_pop(q.matrices);
    o.total_draws+=std::uint32_t(calls.size());
    return objects;
}
}
NativeRaceAreaRuntime& native_race_area(NativeRuntimeContext& c){
    if(!c.race_area){
        c.race_area=std::make_shared<NativeRaceAreaRuntime>();
        // ---- race manager binding: the event-359 owner answers 450xxx / 451350: begin ----
        NativeRuntimeContext* ctx=&c;
        c.race_area->race_manager=[ctx](const PcRaceCall& k,std::uint32_t& eax){
            return native_race_manager_call(*ctx,k.pc,k.args.data(),k.args.size(),eax,true);};
        // ---- race manager binding: end ----
    }
    return *c.race_area;
}
driving::PcRoadTableArena native_race_road_tables(NativeRuntimeContext& c){return native_race_area(c).road_arena();}
namespace {
struct RoadIo {
    NativeRuntimeContext* c;NativeRaceAreaRuntime* o;std::string error;
    static std::uint32_t submit(void* u,const driving::PcRoadTableRequest& rq){
        auto& io=*static_cast<RoadIo*>(u);auto& o=*io.o;
        auto* store=io.c->event_function36.retail_assets;
        if(!store)throw std::runtime_error("road table "+rq.path+": no retail store");
        std::string error;
        const bool sz=rq.path.size()>3&&rq.path.compare(rq.path.size()-3,3,".sz")==0;
        const auto* bytes=retail_asset_guest_path(*store,rq.path,sz,&error);
        if(!bytes)throw std::runtime_error("road table "+rq.path+": "+error);
        std::vector<std::uint8_t> image;
        if(!race_area_loader_image(*bytes,sz,0u,rq.flags,image,error))throw std::runtime_error("road table "+rq.path+": "+error);
        const std::size_t n=image.size()-8u;
        auto& region=rq.region==driving::PcRoadTableRegion::Primary?o.road_primary:rq.region==driving::PcRoadTableRegion::Preload?o.road_preload:o.road_secondary;
        // 44F990 with a destination reads the file's size (not the request
        // size) there; a larger file would overrun the PC table.
        if(std::size_t(rq.offset)+n>region.size())throw std::runtime_error("road table "+rq.path+" ("+std::to_string(n)+" bytes) overruns its region");
        std::memcpy(region.data()+rq.offset,image.data(),n);
        ++o.road_requests;o.road_bytes+=std::uint32_t(n);
        return 1u+o.road_requests;
    }
    static std::uint32_t finished(void*,std::uint32_t){return 1u;}   // synchronous transfer: state 4
    static void release(void*,std::uint32_t){}
};
}
std::uint32_t native_race_road_tables_46fc30(NativeRuntimeContext& c,std::uint32_t which){
    auto& o=native_race_area(c);
    driving::pc_road_table_reset_46fac0(o.road_loader,o.road_arena(),which);
    return 0u;
}
std::uint32_t native_race_road_tables_46fe50(NativeRuntimeContext& c,std::uint32_t which,std::uint32_t kind){
    auto& o=native_race_area(c);
    RoadIo io{&c,&o,{}};
    const driving::PcRoadTableFileService svc{&io,RoadIo::submit,RoadIo::finished,RoadIo::release};
    const std::uint32_t selected=le32(o.area.block.data()+(0x7d33c8u-PcRaceAreaState::BlockBase));
    return driving::pc_road_table_step_46fde0(o.road_loader,o.road_arena(),which,kind,selected,svc);
}
// Course teardown (race end 49A4F0 / mode 28 49AF90): 43DE50(lane),
// 44A1A0, 4F11B0(lane), 4F0600(lane) on the same owners as course-prog.
void native_race_area_teardown(NativeRuntimeContext& c,std::uint32_t pc,std::uint32_t arg){
    auto& o=native_race_area(c);
    rebuild(c,o,nullptr);
    Service sv{c,o,nullptr};
    std::array<std::uint8_t,0x40> mb{};driving::PcMatrixStack st{Bytes(mb.data(),mb.size())};
    PcRaceContext ctx{o.memory,st,[&sv](const PcRaceCall& k){return sv(k);},nullptr};
    sv.ctx=&ctx;
    PcRaceCall k;k.pc=pc;k.argc=1;k.args[0]=arg;
    (void)sv(k);
}
// One AREA owner service for another module (the arcade ending's 448AD0 requests): the
// call runs on the AREA owner's own model; a missing service throws.
std::uint32_t native_race_area_call(NativeRuntimeContext& c,PcSceneRenderer* r,const PcRaceCall& k){
    auto& o=native_race_area(c);
    rebuild(c,o,r);
    Service sv{c,o,r};
    std::array<std::uint8_t,0x40> mb{};driving::PcMatrixStack st{Bytes(mb.data(),mb.size())};
    PcRaceContext ctx{o.memory,st,[&sv](const PcRaceCall& q){return sv(q);},nullptr};
    sv.ctx=&ctx;
    return sv(k);
}
// ---- race manager binding: AREA memory for the event-359 owner: begin ----
bool native_race_bank_ensure(NativeRuntimeContext& c,PcSceneRenderer& r,std::uint32_t resource){
    if(resource>=0x223u)return false;
    Service sv{c,native_race_area(c),&r};
    return sv.ensure_bank(resource);
}
PcRaceMemory& native_race_area_memory(NativeRuntimeContext& c,PcSceneRenderer* renderer,bool rebuild_now){
    auto& o=native_race_area(c);
    if(!rebuild_now)return o.memory;
    // Once per frame: the mapping table only changes with the AREA's own
    // allocations/lanes (every AREA callback rebuilds it first) and START's
    // course selection; the mapped storage itself is live (no copies are
    // read by the manager: 7D2D80 block, course_runtime words, records,
    // EXE data, the car work, the manager block).
    if(o.memory_frame!=c.completed_frames+1u){
        o.frame=c.completed_frames;
        sync_start_lanes(c,o);
        rebuild(c,o,renderer);
    }
    // START's 44D720 (44D9A4/44D9C5) stores 7D33BC = records + selected*0x78
    // and 7D3188 = 7D30A8 in this block; the native 44D720 port keeps them as
    // course_runtime.selected_index / selected_copy_active. Until 44CB00
    // (which stores the same values) has run, mirror them here so the race
    // manager (event 359, initialized before event 390) reads START's words.
    const auto& rt=c.game_mode.course_runtime;
    if(!o.initialized&&rt.selected_copy_active&&o.table){
        Bytes b(o.area.block.data(),o.area.block.size());
        b.put32(0x7d33bcu-PcRaceAreaState::BlockBase,RaceAreaTableBase+std::uint32_t(rt.selected_index)*0x78u);
        b.put32(0x7d3188u-PcRaceAreaState::BlockBase,0x7d30a8u);
    }
    return o.memory;
}
// START-time readers of the course records (44C8D0 through the NAVI memory):
// identify the 44D720 table and mirror START's 7D33BC / 7D3188 words until
// 44CB00 has run, exactly like the race manager binding above.
bool native_race_area_bind_records(NativeRuntimeContext& c){
    auto& o=native_race_area(c);
    if(!find_table(c,o))return false;
    const auto& rt=c.game_mode.course_runtime;
    if(!o.initialized&&rt.selected_copy_active){
        Bytes b(o.area.block.data(),o.area.block.size());
        b.put32(0x7d33bcu-PcRaceAreaState::BlockBase,RaceAreaTableBase+std::uint32_t(rt.selected_index)*0x78u);
        b.put32(0x7d3188u-PcRaceAreaState::BlockBase,0x7d30a8u);
    }
    return true;
}
// ---- race manager binding: end ----
bool native_race_area_event_invoke(NativeRuntimeContext& c,std::uint32_t callback,std::uint32_t work,std::uint32_t,
    driving::PcMatrixStack& matrices,PcSceneRenderer* renderer){
    switch(callback){case 0x44cb00u:case 0x44f7c0u:case 0x44b7f0u:case 0x451a30u:case 0x451b40u:break;default:return false;}
    auto& o=native_race_area(c);
    o.frame=c.completed_frames;
    if(renderer)hook_colours(c,o,*renderer);
    sync_start_lanes(c,o);
    rebuild(c,o,renderer);
    Service sv{c,o,renderer};
    PcRaceContext ctx{o.memory,matrices,[&sv](const PcRaceCall& k){return sv(k);},nullptr};
    sv.ctx=&ctx;
    const auto depth=matrices.depth;
    try{
        switch(callback){
        case 0x44cb00u:
            // 7D33BC: the selected record of the table (44D720 stores
            // records + selected*0x78); 7D3188 as 44D720 left it.
            if(!o.table)throw std::runtime_error("course record table not identified (7D33BC)");
            Bytes(o.area.block.data(),o.area.block.size()).put32(0x7d33bcu-PcRaceAreaState::BlockBase,
                RaceAreaTableBase+std::uint32_t(c.game_mode.course_runtime.selected_index)*0x78u);
            race_area_init_44cb00(ctx);o.initialized=true;o.init_failed=false;++o.inits;break;
        case 0x44f7c0u:
            if(o.init_failed||!o.initialized){++o.aborts["44F7C0 without a completed 44CB00"];++o.control_aborts;break;}
            // 44E590 state 0 uses the callback's ECX (never an entry state).
            if(o.memory.u32(0x7d2e80u)==0u)throw std::runtime_error("44E590 entered in state 0: needs the PC ECX");
            ++o.controls;race_area_control_44f7c0(ctx,0u);break;
        case 0x44b7f0u:race_area_destroy_44b7f0(ctx);o.initialized=false;++o.destroys;break;
        case 0x451a30u:
            if(work!=c.event_state.slots[391].work_token||!work)throw std::runtime_error("sky work token mismatch");
            race_sky_init_451a30(ctx,work);o.sky_initialized=true;o.sky_init_failed=false;++o.sky_inits;break;
        case 0x451b40u:
            if(o.sky_init_failed||!o.sky_initialized){++o.aborts["451B40 without a completed 451A30"];++o.sky_control_aborts;break;}
            ++o.sky_controls;race_sky_control_451b40(ctx,work);break;
        }
    }catch(const std::exception& e){
        const std::string why=(callback==0x44cb00u?"44CB00: ":callback==0x44f7c0u?"44F7C0: ":callback==0x451a30u?"451A30: ":callback==0x451b40u?"451B40: ":"44B7F0: ")+what_of(e);
        ++o.aborts[why];o.last_error=why;
        if(!o.fault_pc&&o.fault.empty())o.fault=why;
        if(callback==0x44cb00u){o.init_failed=true;o.initialized=false;}
        if(callback==0x451a30u){o.sky_init_failed=true;o.sky_initialized=false;}
        if(callback==0x44f7c0u)++o.control_aborts;
        if(callback==0x451b40u)++o.sky_control_aborts;
        while(matrices.depth>depth)driving::pc_matrix_pop(matrices);   // an aborted callback leaves the stack as it was entered
    }
    // Host diagnostic (OR2_HOST_AREA_TRACE): one line per change of the area / stage states.
    if(static const bool trace=std::getenv("OR2_HOST_AREA_TRACE")!=nullptr;trace&&callback==0x44f7c0u){
        static std::string last;
        auto w=[&](std::uint32_t a){std::uint32_t v=0;try{v=o.memory.u32(a);}catch(...){}return v;};
        std::ostringstream t;t<<std::hex<<"2E80="<<w(0x7d2e80u)<<" 2E88="<<w(0x7d2e88u)<<" 2D80="<<w(0x7d2d80u)<<" 2E5C="<<w(0x7d2e5cu)
            <<" 33B4="<<w(0x7d33b4u)<<" 3188="<<w(0x7d3188u)<<" 2E7C="<<w(0x7d2e7cu)<<" 306C="<<w(0x7d306cu)<<" 3070="<<w(0x7d3070u)
            <<" lanes="<<c.start_mode.scene_owner_collision_lane_state[0]<<c.start_mode.scene_owner_collision_lane_state[1]
            <<c.start_mode.scene_owner_collision_lane_state[2]<<c.start_mode.scene_owner_collision_lane_state[3];
        if(!o.last_error.empty())t<<" err="<<o.last_error;
        if(t.str()!=last){last=t.str();std::fprintf(stderr,"AREA f%u %s\n",unsigned(c.completed_frames),last.c_str());}
    }
    return true;
}
bool native_race_area_display(NativeRuntimeContext& c,PcSceneRenderer& r,std::uint32_t callback,std::uint32_t work,std::uint32_t){
    if(callback==0x44f830u){   // event 399 display: "LOADING" (debug font 42CA60..42CCE0) while [7D34C4] != 0
        // Every 44FCC0 caller of the build passes 0: the text is never drawn.
        auto& o=native_race_area(c);
        if(!o.memory.mapped(0x7d34c4u,4)||o.memory.u32(0x7d34c4u)==0u)return true;
        ++o.aborts["44F830 LOADING text (debug font not ported)"];return true;}
    if(callback!=0x44f120u&&callback!=0x4521c0u)return false;
    auto& o=native_race_area(c);
    const bool area=callback==0x44f120u;
    if(area?!o.initialized:!o.sky_initialized){++o.aborts[area?"44F120 without a completed 44CB00":"4521C0 without a completed 451A30"];
        ++(area?o.display_aborts:o.sky_display_aborts);return true;}
    o.frame=c.completed_frames;
    if(area)r.step_banks(1.5);                              // the queued 448AD0 banks (port)
    hook_colours(c,o,r);
    sync_start_lanes(c,o);
    rebuild(c,o,&r);
    Service sv{c,o,&r};
    auto& draws=o.draw_list;draws.clear();                  // reused: no per-frame allocation
    auto& matrices=r.matrices();
    PcRaceContext ctx{o.memory,matrices,[&sv](const PcRaceCall& k){return sv(k);},&draws};
    sv.ctx=&ctx;
    const auto depth=matrices.depth;
    try{
        if(area){race_area_display_44f120(ctx);++o.displays;}
        else{race_sky_display_4521c0(ctx,work);++o.sky_displays;}
    }catch(const std::exception& e){
        // A display stopped by a missing service is dropped whole: executing
        // the recorded prefix would leave device state the PC restores later
        // in the same callback (4521C0 turns ZENABLE off before the sky
        // objects and back on after them), which then leaked into every
        // later draw of the frame (car and wheels without depth test).
        const std::string why=std::string(area?"44F120: ":"4521C0: ")+e.what();
        ++o.aborts[why];o.last_error=why;++(area?o.display_aborts:o.sky_display_aborts);
        while(matrices.depth>depth)driving::pc_matrix_pop(matrices);
        draws.clear();
    }
    try{
        const auto n=execute(o,sv,r,draws);(void)n;
        (area?o.last_area_draws:o.last_sky_draws)=std::uint32_t(draws.size());
    }catch(const std::exception& e){
        const std::string why=std::string(area?"44F120 draw list: ":"4521C0 draw list: ")+e.what();
        ++o.aborts[why];o.last_error=why;++(area?o.display_aborts:o.sky_display_aborts);
    }
    return true;
}
std::uint32_t native_race_draw_list_execute(NativeRuntimeContext& c,PcSceneRenderer& r,const std::vector<PcVehicleDrawCall>& calls){
    auto& o=native_race_area(c);
    Service sv{c,o,&r};
    return execute(o,sv,r,calls);
}
// ---- car reflection cube (414340): begin ----
namespace {
// 44CD30 (which 0) / 451C20 (which 1) of one cube face on the renderer's
// matrix stack, then their draw list. The AREA memory is rebuilt once per
// 414340 (rebuild_memory); later faces only refresh the 95DBA0 copy (the
// face's inverse view, set by 410FF0).
bool cube_scene(NativeRuntimeContext& c,PcSceneRenderer& r,std::uint32_t which,bool rebuild_memory){
    auto& o=native_race_area(c);
    if(which==0u&&!o.initialized&&o.destroys==0u){
        // No course loaded yet (the frontend car select: 7D2E80 < 0xE, 7D3070 = 0): 44CD30 draws
        // nothing, its exit leaves the LOD threshold 4044E0(7) (the 409EF0 / 40A010 pair cancels).
        r.queue_context().lod_threshold_8999b8=7;++o.cube_without_course;return true;
    }
    if(which==0u?!o.initialized:!o.sky_initialized){++o.aborts[which==0u?"44CD30 without a completed 44CB00":"451C20 without a completed 451A30"];++o.cube_aborts;return false;}
    if(rebuild_memory){hook_colours(c,o,r);sync_start_lanes(c,o);rebuild(c,o,&r);}
    else{const auto v=r.flush_context().g.matrix(0x95dba0u);std::memcpy(o.view_inverse_95dba0.data(),v.data(),64);}
    Service sv{c,o,&r};
    auto& draws=o.draw_list;draws.clear();
    auto& matrices=r.matrices();
    PcRaceContext ctx{o.memory,matrices,[&sv](const PcRaceCall& k){return sv(k);},&draws};
    sv.ctx=&ctx;
    const auto depth=matrices.depth;const auto offset=matrices.current_offset;
    try{
        if(which==0u){race_env_model_44cd30(ctx);++o.cube_model_runs;}
        else{race_env_sky_451c20(ctx,c.event_state.slots[391].work_token);++o.cube_sky_runs;}
    }catch(const std::exception& e){
        const std::string why=std::string(which==0u?"44CD30: ":"451C20: ")+e.what();
        ++o.aborts[why];o.cube_error=why;++o.cube_aborts;
        matrices.depth=depth;matrices.current_offset=offset;draws.clear();return false;
    }
    try{execute(o,sv,r,draws);}
    catch(const std::exception& e){const std::string why=std::string("cube draw list: ")+e.what();++o.aborts[why];o.cube_error=why;++o.cube_aborts;return false;}
    return true;
}
}
void native_car_reflection_init(PcSceneRenderer& r){
    auto& f=r.flush_context();
    if(f.g.w(0x8a89f0)&1u)return;
    // 413B30 (bootstrap 41781A): the face matrices are built in a scratch stack, not
    // in the renderer's current matrix (the PC leaves RotationY(pi) there at boot).
    std::array<std::uint8_t,0x80> scratch{};driving::PcMatrixStack st{driving::Bytes(scratch.data(),scratch.size()),0,0,2};
    car_reflection_init_413b30(f,st);
}
bool native_car_reflection_leaf(NativeRuntimeContext& c,PcSceneRenderer& r){
    auto& f=r.flush_context();
    if(!(f.g.w(0x8a89f0)&1u)){native_car_reflection_init(r);++native_race_area(c).cube.inits;}
    auto& o=native_race_area(c);
    auto& car=c.event_function36.car_select;
    PcCarReflectionInputs in;
    in.player_flags_79fb50=std::uint8_t(c.event_state.slots[8].flags);
    in.final_stage_7d33d0=o.area.block[0x7d33d0u-PcRaceAreaState::BlockBase];
    in.car=driving::Bytes(car.car_799d18.data(),car.car_799d18.size());
    in.camera=driving::Bytes(car.camera_79fe10.data(),car.camera_79fe10.size());
    in.light_899c38=driving::Bytes(r.environment().lights_899b98.data()+0xa0,0xa0);
    if(c.event_state.slots[387].work_token)in.scn_efc=driving::Bytes(c.race_effects.scene.work.data(),c.race_effects.scene.work.size());
    in.layer_7d25f0=r.frame().layer_7d25f0;
    bool first=true;
    PcCarReflectionServices sv;
    sv.env_model_44cd30=[&]{cube_scene(c,r,0u,first);first=false;};
    sv.env_sky_451c20=[&]{cube_scene(c,r,1u,first);first=false;};
    sv.flush_alpha_405830=[&]{r.flush_alpha_405830();};
    sv.draw_4056d0=[&](std::uint32_t object,std::uint32_t value,std::uint32_t colour,std::uint32_t colour_byte){
        PcVehicleDrawCall call{};call.pc=0x4056d0u;call.argc=4;call.args[0]=object;call.args[1]=value;call.args[2]=colour;call.args[3]=colour_byte;
        const auto m=r.matrices().current();for(unsigned k=0;k<64;++k)call.matrix[k]=m.u8(k);
        Service s{c,o,&r};
        try{execute(o,s,r,{call});}catch(const std::exception& e){o.cube_error=std::string("4056D0: ")+e.what();++o.cube_aborts;}
    };
    static const auto t0=std::chrono::steady_clock::now();
    sv.elapsed_449df0=[]{
        // Host lockstep runs (fixed clock): a fixed 1 ms per call keeps the
        // reflection face budget, hence the draws, deterministic.
        static float tick=0.f;
        if(dev_hooks().fixed_clock)return tick+=1.f;
        return float(std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-t0).count());};
    car_reflection_frame_414340(f,r.matrices(),r.render_view(),in,sv,o.cube);
    return true;
}
// ---- car reflection cube: end ----
std::string native_race_area_status(const NativeRuntimeContext& c){
    if(!c.race_area)return "area: no owner";
    const auto& o=*c.race_area;
    auto w=[&](std::uint32_t a){return le32(o.area.block.data()+(a-PcRaceAreaState::BlockBase));};
    std::ostringstream s;
    s<<std::hex<<"area 7D2E80="<<c.race.area_state_7d2e80<<" 7D2E88="<<c.race.area_state_7d2e88<<" 7D2D80="<<w(0x7d2d80u)
     <<" 7D306C="<<w(0x7d306cu)<<" 7D3070="<<w(0x7d3070u)<<std::dec<<" init="<<o.inits<<(o.init_failed?"(failed)":"")<<" object_events_opened="<<o.object_events_opened<<" object_ranges_closed="<<o.object_event_ranges_closed
     <<" ctl="<<o.controls<<"/abort "<<o.control_aborts<<" disp="<<o.displays<<"/abort "<<o.display_aborts
     <<" draws="<<o.last_area_draws<<" | sky init="<<o.sky_inits<<(o.sky_init_failed?"(failed)":"")<<" ctl="<<o.sky_controls
     <<"/abort "<<o.sky_control_aborts<<" disp="<<o.sky_displays<<"/abort "<<o.sky_display_aborts<<" draws="<<o.last_sky_draws
     <<" banks="<<o.banks_loaded.size()<<" skipped="<<o.skipped_objects;
    if(!o.skipped_ids.empty()){s<<" (not resident:"<<std::hex;for(const auto& [id,n]:o.skipped_ids)s<<" "<<id<<"x"<<std::dec<<n<<std::hex;s<<std::dec<<")";}
    if(!o.last_error.empty())s<<" last="<<o.last_error;
    return s.str();
}
std::string native_race_area_report(const NativeRuntimeContext& c){
    if(!c.race_area)return "area: no owner\n";
    const auto& o=*c.race_area;
    std::ostringstream s;
    s<<native_race_area_status(c)<<"\n  table: "<<o.table_source<<"\n  routed (first frame):";
    for(const auto& [pc,f]:o.routed_order)s<<" "<<std::hex<<pc<<std::dec<<"@"<<f<<"x"<<o.routed.at(pc);
    s<<"\n  missing (first frame):";
    for(const auto& [pc,f]:o.missing_order)s<<" "<<std::hex<<pc<<std::dec<<"@"<<f<<"x"<<o.missing.at(pc);
    s<<"\n  draw leaves:";for(const auto& [pc,n]:o.draws_by_pc)s<<" "<<std::hex<<pc<<std::dec<<"x"<<n;
    s<<"\n  banks:";for(auto b:o.banks_loaded)s<<" "<<std::hex<<b<<std::dec;
    s<<"\n  road tables: requests="<<o.road_requests<<" bytes="<<o.road_bytes<<" sound 4249F0="<<o.sound_effects_4249f0.size();
    s<<"\n  aborts:";for(const auto& [k,n]:o.aborts)s<<"\n    x"<<n<<" "<<k;
    s<<"\n  first fault: "<<o.fault<<"\n";
    return s.str();
}
}
