#include "enhancements/frame_rate.hpp"
#include "platform/race_end_runtime.hpp"
#include "platform/bulk_fallback.hpp"
#include "platform/race_ghosts_runtime.hpp"
#include "platform/native_runtime.hpp"
#include "platform/race_manager_runtime.hpp"
#include "platform/race_traffic_runtime.hpp"
#include "platform/race_area_runtime.hpp"
#include "platform/pc_scene_renderer.hpp"
#include "platform/retail_asset_store.hpp"
#include "driving/pc_wrecker.hpp"
#include "driving/pc_camera.hpp"
#include "driving/pc_wall_rebound.hpp"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
namespace outrun::platform {
bool native_mission_type4_4962a0(const NativeRuntimeContext&);
namespace {
struct GhostUnported : std::runtime_error {
    std::uint32_t pc;
    GhostUnported(std::uint32_t p,const std::string& why):std::runtime_error(why),pc(p){}
};
std::string c_string(const PcRaceMemory& m,std::uint32_t p){
    std::string s;for(std::uint32_t i=0;i<0x100u;++i){const auto ch=m.u8(p+i);if(!ch)break;s.push_back(char(ch));}return s;
}
void put32(std::uint8_t* p,std::uint32_t v){for(unsigned i=0;i<4;++i)p[i]=std::uint8_t(v>>(8*i));}
std::uint32_t get32(const std::uint8_t* p){return std::uint32_t(p[0])|(std::uint32_t(p[1])<<8)|(std::uint32_t(p[2])<<16)|(std::uint32_t(p[3])<<24);}
// 406E50: 0 read, 9 cannot open, 0xA short length read, 0xB length differs,
// 0xD short payload read.
std::uint32_t save_read(const std::string& path,std::uint8_t* out,std::uint32_t size){
    auto* f=std::fopen(path.c_str(),"rb");if(!f)return 9u;
    std::uint8_t header[4];
    if(std::fread(header,1,4,f)!=4){std::fclose(f);return 0xau;}
    if(get32(header)!=size){std::fclose(f);return 0xbu;}
    const bool ok=std::fread(out,1,size,f)==size;
    std::fclose(f);return ok?0u:0xdu;
}
// 406C50: 0 written, 1 cannot create, 2 length write failed, 7 payload write
// failed, 6 close failed.
std::uint32_t save_write(const std::string& path,const std::uint8_t* data,std::uint32_t size){
    auto* f=std::fopen(path.c_str(),"wb");if(!f)return 1u;
    std::uint8_t header[4];put32(header,size);
    if(std::fwrite(header,1,4,f)!=4){std::fclose(f);return 2u;}
    if(std::fwrite(data,1,size,f)!=size||std::fflush(f)!=0){std::fclose(f);return 7u;}
    return std::fclose(f)==0?0u:6u;
}
}
NativeGhostRuntime::NativeGhostRuntime(){
    PcRaceMemory m;state.map(m);
    ghost_save_layout_416610(m);                                     // 4162D0 at boot
    put32(state.words.data()+8,GhostSaveObjectBase);                 // [8A8C7C]
}
void NativeGhostRuntime::map(PcRaceMemory& m){
    state.map(m);records.map(m);ghost_map_tables(m);
    m.map(0x841ff0u,record_queue_841ff0.data(),record_queue_841ff0.size());
    // 842044: .bss without a writer (only 842038..842043 and 842048.. are written): 4AD1F0 reads
    // its byte as the +C of a fifth queue entry once four cars are queued.
    static const std::array<std::uint8_t,4> bss_842044{};m.map_const(0x842044u,bss_842044.data(),bss_842044.size());
    m.map(GhostSaveObjectBase,save_object.data(),save_object.size());
    for(auto& [base,bytes]:heap)m.map(base,bytes.data(),bytes.size());
}
NativeGhostRuntime& native_race_ghosts(NativeRuntimeContext& c){
    if(!c.race_ghosts)c.race_ghosts=std::make_shared<NativeGhostRuntime>();
    return *c.race_ghosts;
}
namespace {
template<class F> bool run_ghost(NativeRuntimeContext& c,F&& body,driving::PcMatrixStack* race_matrices=nullptr){
    auto& g=native_race_ghosts(c);
    if(g.fault)return false;
    if(g.save_directory.empty())g.save_directory=c.event_function36.frontend_save_directory;
    PcRaceMemory m;g.map(m);ghost_map_car_tables(m);   // 4866C0 model records 650500 (46F350, 480220)
    {auto& works=c.race.manager.car_works_7815a0;m.map(NativeGhostRuntime::CarBase,works.data(),4u*0x10f0u);}   // events 9..12
    std::uint32_t variant=c.game_mode.game_variant,download=0x54u;   // 672DF0: .data 0x54 unless a network download set it
    m.map(0x780258u,reinterpret_cast<std::uint8_t*>(&variant),4);m.map(0x672df0u,reinterpret_cast<std::uint8_t*>(&download),4);
    std::uint32_t preset=c.start_mode.course_preset;m.map_const(0x78024cu,reinterpret_cast<const std::uint8_t*>(&preset),4);
    std::array<std::uint8_t,0x40> matrix_bytes{};driving::PcMatrixStack local_matrices{driving::Bytes(matrix_bytes.data(),matrix_bytes.size())};
    driving::PcMatrixStack& matrices=race_matrices?*race_matrices:local_matrices;
    // Race globals the ghost cars read (78026C, 656234, 799D18 -> player car
    // 7804B0, area matrix 7D2DA0).
    std::uint32_t game_mode=c.mode_state.current,course_code=c.start_mode.frontend_prepare.output_code_656234,player_ptr=0x7804b0u;
    m.map(0x78026cu,reinterpret_cast<std::uint8_t*>(&game_mode),4);m.map(0x656234u,reinterpret_cast<std::uint8_t*>(&course_code),4);
    auto& player=c.event_function36.car_select.car_799d18;
    if(player.size()>=0x10f0u){m.map(0x799d18u,reinterpret_cast<std::uint8_t*>(&player_ptr),4);m.map(0x7804b0u,player.data(),0x10f0u);}
    auto& license=c.event_function36.frontend_profiles.active;m.map(0x7c23e0u,license.data(),license.size());   // 4675F0 ghost name
    m.map(0x7d2da0u,c.game_mode.course_runtime.matrix_7d2da0.data(),64);
    m.map(0x7d3190u,c.game_mode.course_runtime.matrix_7d3190.data(),64);
    // Event flags 79FB48 and records 799B30 (+8 work) as read-only snapshots (47ED00 reads them).
    std::vector<std::uint8_t> event_flags(410u),event_records(410u*0x3cu);
    for(std::uint32_t i=0;i<410u;++i){event_flags[i]=std::uint8_t(c.event_state.slots[i].flags);
        put32(event_records.data()+i*0x3cu+8u,c.event_state.slots[i].work_token);}
    m.map_const(0x79fb48u,event_flags.data(),event_flags.size());m.map_const(0x799b30u,event_records.data(),event_records.size());
    auto allocate=[&](std::uint32_t size)->std::uint32_t{
        if(!size||size>GhostHeapWindow)throw GhostUnported(0x580253u,"ghost heap: allocation size "+std::to_string(size));
        const std::uint32_t base=g.next_block;g.next_block+=GhostHeapWindow;
        auto& bytes=g.heap[base];bytes.assign(size,0);m.map(base,bytes.data(),bytes.size());
        return base;
    };
    auto release=[&](std::uint32_t p){if(p)g.heap.erase(p);};   // the block stays mapped in m until the call returns
    auto file=[&](std::uint32_t h)->const std::vector<std::uint8_t>&{
        if(h<0x74000001u||h-0x74000001u>=g.files.size()||!g.files[h-0x74000001u])throw GhostUnported(0x423f10u,"ghost files: bad handle");
        return *g.files[h-0x74000001u];};
    PcRaceContext* current=nullptr;
    PcRaceContext ctx{m,matrices,[&](const PcRaceCall& k)->std::uint32_t{
        const auto* a=k.args.data();
        switch(k.pc){
        case 0x580253u:case 0x580c33u:return allocate(a[0]);
        case 0x580bc2u:case 0x580c38u:release(a[0]);return 0u;
        case 0x48b320u:return c.start_mode.frontend_prepare.output_code_656234;
        case 0x416700u:{std::array<std::uint32_t,7> v{};for(unsigned i=0;i<7;++i)v[i]=a[i];return ghost_save_open_416700(*current,v);}
        case 0x416930u:{std::array<std::uint32_t,9> v{};for(unsigned i=0;i<9;++i)v[i]=a[i];ghost_save_read_416930(*current,v);return 0u;}
        case 0x416830u:ghost_save_close_416830(*current);return 0u;
        // record module (variant 0) services of the record ghost cars
        case 0x47f1a0u:return records_frame_47f1a0(m,a[0]);
        case 0x47f140u:return records_state_47f140(m,a[0]);
        case 0x480220u:return records_play_480220(*current,a[0],a[1],a[2]);
        case 0x43f370u:{   // 43F370(p) (VM push measured: 43EB60(0x100, &copy, &arg slot, 0, 0)): course length word
            const auto tables=c.start_mode.scene_owner_course_world.tables();
            driving::CourseProbe point{m.f32(a[0]),m.f32(a[0]+4),m.f32(a[0]+8)};
            std::uint32_t polygon=a[0];
            driving::CourseWorldQuery q{tables,matrices,c.event_function36.car_select.race_prediction};
            const std::uint32_t type=driving::get_y_position_prog(q,0x100u,point,&polygon,nullptr,nullptr);
            if(polygon==0xffffffffu)return 0u;
            if(type>3u||!tables.courses[type].runs.present)throw GhostUnported(0x43f370u,"43F370: course type without a length table");
            const auto& lengths=tables.courses[type].runs.lengths;
            if(std::size_t(polygon)*2u+2u>lengths.size())throw GhostUnported(0x43f370u,"43F370: polygon outside the length table");
            return std::uint32_t(std::uint16_t(lengths.i16(std::size_t(polygon)*2u)));}
        case 0x4239c0u:{
            const std::string name=c_string(m,a[0]);std::string err;
            const std::vector<std::uint8_t>* bytes=nullptr;
            if(auto* retail=c.event_function36.retail_assets)bytes=retail_asset_guest_path(*retail,name,false,&err);
            if(!bytes)return 0u;
            g.files.push_back(bytes);++g.file_loads;return 0x74000000u+std::uint32_t(g.files.size());}
        case 0x423f10u:return std::uint32_t(file(a[0]).size());
        case 0x423cb0u:{const auto& f=file(a[3]);const std::uint64_t n=std::uint64_t(a[1])*a[2];
            if(n>f.size())throw GhostUnported(0x423cb0u,"ghost files: read past the file");
            if(n)std::memcpy(m.at(a[0],std::size_t(n),true),f.data(),std::size_t(n));
            return a[2];}
        case 0x423bd0u:(void)file(a[0]);return 0u;
        case 0x406db0u:{
            if(g.save_directory.empty())return 0u;
            auto* f=std::fopen((g.save_directory+"/"+c_string(m,k.ecx)).c_str(),"rb");if(!f)return 0u;std::fclose(f);return 1u;}
        case 0x406e50u:
            if(g.save_directory.empty())return 1u;                   // 406B70 without a directory
            ++g.save_reads;return save_read(g.save_directory+"/"+c_string(m,k.ecx),m.at(a[0],a[1],true),a[1]);
        case 0x406c50u:
            if(g.save_directory.empty())return 1u;
            ++g.save_writes;return save_write(g.save_directory+"/"+c_string(m,k.ecx),m.at(a[0],a[1]),a[1]);
        case 0x49b2d0u:return std::uint32_t(std::uint16_t(c.game_mode.start_countdown));   // 49B2D0 = [8367BC]
        case 0x4962a0u:return native_mission_type4_4962a0(c)?1u:0u;
        case 0x4401d0u:driving::event_close_4401d0(c.event_state,a[0]);return 0u;
        case 0x440bd0u:driving::change_disp_scene_440bd0(c.event_state,a[0],a[1]);return 0u;
        case 0x440ba0u:driving::change_now_event_shadow_func_440ba0(c.event_state,a[0]);return 0u;
        case 0x48b310u:return c.start_mode.flag_830394;
        case 0x44c940u:return race_area_value_44c940(native_race_area_memory(c,nullptr,true),a[0]);
        case 0x450250u:{   // 450250(stage, result): the stage key a result leads to (as the traffic binding)
            const std::uint16_t preset=std::uint16_t(c.start_mode.course_preset);
            if(preset==2u||preset==3u)return a[0]+1u;
            const std::uint32_t lvl=race_area_value_44c940(native_race_area_memory(c,nullptr,true),a[0]);
            if(lvl>3u)return 0xffffffffu;
            const std::uint32_t r=lvl==0u?1u:a[0]+1u+lvl;
            if(lvl!=0u&&r==0xffffffffu)return r;
            return a[1]==1u?r+1u:r;}
        case 0x43f9c0u:return c.start_mode.game_flag_780248;   // mov al,[780248]
        case 0x43f960u:return (c.start_mode.course_preset==2u||c.start_mode.course_preset==3u)?1u:0u;
        case 0x48b1a0u:return c.start_mode.vehicle_variant_83036d;   // mov al,[83036D]
        case 0x4505d0u:case 0x451180u:case 0x450610u:case 0x450320u:case 0x450130u:case 0x451350u:case 0x450780u:
        case 0x44ff10u:case 0x450750u:case 0x4505a0u:case 0x450630u:{   // race manager accessors (event-359 owner)
            std::uint32_t eax{};
            if(!native_race_manager_call(c,k.pc,k.args.data(),k.argc,eax,true))throw GhostUnported(k.pc,"ghost module: race manager accessor refused");
            return eax;}
        case 0x424940u:{   // race sound request: the player car queue 9563E8 gated by event 383
            auto& w=c.race.car_world;
            driving::PcSoundQueue q{driving::Bytes(w.sound_entries_9563e8.data(),w.sound_entries_9563e8.size()),
                driving::Bytes(w.sound_state.data(),w.sound_state.size()),std::uint8_t(c.event_state.slots[383].flags)};
            driving::pc_enqueue_sound(q,a[0]);return 0u;}
        case 0x440200u:{   // immediate close: the ghost car destroy 470560 on the race manager car work
            const driving::PcEventServices services{&c,[](void* u,std::uint32_t cb,std::uint32_t work,std::uint32_t){
                auto& cc=*static_cast<NativeRuntimeContext*>(u);
                if(cb!=0x470560u)throw GhostUnported(cb,"ghost module: 440200 destroy callback not a ghost car");
                auto& works=cc.race.manager.car_works_7815a0;
                if(work<NativeGhostRuntime::CarBase||work+0x10f0u>NativeGhostRuntime::CarBase+works.size())throw GhostUnported(0x470560u,"ghost car work outside 7815A0..");
                driving::Bytes(works.data()+(work-NativeGhostRuntime::CarBase),0x10f0u).put32(0x10d0,0xffffffffu);},nullptr};
            driving::event_close_immediate_440200(c.event_state,a[0],services);return 0u;}
        case 0x440180u:{   // open event (slot+9, 0x58): init 4AD000 runs at once on this memory
            struct Open{PcRaceContext* ctx;};Open open{current};
            const driving::PcEventServices services{&open,[](void* u,std::uint32_t cb,std::uint32_t work,std::uint32_t){
                auto& o=*static_cast<Open*>(u);
                if(cb!=0x4ad000u)throw GhostUnported(cb,"ghost module: 440180 init callback not a ghost car");
                ghost_car_init_4ad000(*o.ctx,work);},nullptr};
            driving::event_open_static_440180(c.event_state,a[0],a[1],c.event_descriptors,c.event_functions,services);
            ++g.car_inits;return 0u;}
        case 0x49eed0u:{   // 49EED0: the attract demo round [83DAF4] (race end boot block 83DAE0 + 0x14)
            std::uint32_t round=0;if(c.race_end)std::memcpy(&round,c.race_end->state.boot_83dae0.data()+0x14,4);return round;}
        case 0x4a2650u:{
            // 4A2650 CalcDispMatrix as race_player_car binds it (4493E0 blend,
            // AUTOSCENE flags 79FB4E).
            const std::uint8_t autoscene=std::uint8_t(c.event_state.slots[6].flags);
            if((autoscene&3u)==2u)throw GhostUnported(0x4b5fd0u,"4B5FD0 needs the active AUTOSCENE work 799CA0");
            driving::PcCameraBlend blend{c.race.camera_override.override_82e7d8,autoscene,0,enhancements::display_blend()};
            driving::PcDispMatrixContext display{matrices,driving::camera_blend_4493e0(blend),c.race.camera_override.scene_82e7d4};
            driving::pc_calc_disp_matrix(m.bytes(a[0],0x10f0u),display);
            enhancements::display_note_car(m.bytes(a[0],0x10f0u),[&c=c]{const std::uint8_t autoscene=std::uint8_t(c.event_state.slots[6].flags);   // port: replay between ticks
            if((autoscene&3u)==2u)return 1.f;
            return driving::camera_blend_4493e0({c.race.camera_override.override_82e7d8,autoscene,0,enhancements::display_blend()});},c.race.camera_override.scene_82e7d4);
            return 0u;}
        default:
            if(bulk_translated(k.pc))return bulk_call(m,ctx.service,k,&matrices,&c.event_function36.pc_crt_random_state);
            throw GhostUnported(k.pc,"ghost module: PC service not ported");
        }
    },nullptr};
    current=&ctx;
    try{body(ctx);g.files.clear();return true;}
    catch(const GhostUnported& e){g.fault=e.pc;g.error=e.what();}
    catch(const PcRaceUnmapped& e){g.fault=e.address;g.error=std::string("ghost module: unmapped PC address: ")+e.what();}
    catch(const std::exception& e){g.fault=0x467ac0u;g.error=e.what();}
    g.files.clear();return false;
}
}
bool native_ghost_init_467ac0(NativeRuntimeContext& c){
    ++native_race_ghosts(c).init_calls;
    return run_ghost(c,[](PcRaceContext& ctx){ghost_init_467ac0(ctx);});
}
bool native_records_init_480fe0(NativeRuntimeContext& c,std::uint32_t a,std::uint32_t b,std::uint32_t k){
    ++native_race_ghosts(c).record_inits;
    return run_ghost(c,[&](PcRaceContext& ctx){records_init_480fe0(ctx,a,b,k);});
}
bool native_records_load_481230(NativeRuntimeContext& c,std::uint32_t a,std::uint32_t b,std::uint32_t k){
    ++native_race_ghosts(c).record_loads;
    return run_ghost(c,[&](PcRaceContext& ctx){records_load_481230(ctx,a,b,k);});
}
bool native_ghost_load_4686c0(NativeRuntimeContext& c){
    ++native_race_ghosts(c).load_calls;
    return run_ghost(c,[](PcRaceContext& ctx){ghost_load_4686c0(ctx);});
}
std::int8_t native_ghost_car_465f40(NativeRuntimeContext& c,std::uint32_t i){
    auto& g=native_race_ghosts(c);PcRaceMemory m;g.map(m);
    return ghost_car_465f40(m,i);
}
bool native_ghost_open_440880(NativeRuntimeContext& c){
    // 4407E0(4): events 9..12; callbacks from the car-kind table 59C558..59C568.
    auto& g=native_race_ghosts(c);
    return run_ghost(c,[&](PcRaceContext& ctx){
        for(std::uint32_t k=0;k<4u;++k){
            const std::uint32_t event=9u+k;
            auto& slot=c.event_state.slots[event];
            slot.flags=std::uint8_t((slot.flags&0xe7u)|1u);
            slot.display_scene=c.event_descriptors[event].display_scene;
            slot.init_callback=0x4ad000u;slot.ctrl_callback=0x4ace40u;slot.disp_callback=0x4ae5f0u;
            slot.shadow_callback=0x4adac0u;slot.dest_callback=0x470560u;
            ghost_car_queue_4acfc0(ctx.m,event,std::uint8_t(k+1u),0u,0u);
        }
        ++g.cars_opened;});
}
void native_records_frame_480f80(NativeRuntimeContext& c){
    if(!c.race_ghosts)return;                       // no record module state yet: [81373C] is .bss 0
    auto& g=native_race_ghosts(c);
    if(g.fault)return;
    if(!get32(g.records.tail.data()+(0x81373cu-PcRecordState::TailBase)))return;   // [81373C]: no record tables yet
    (void)run_ghost(c,[](PcRaceContext& ctx){records_frame_480f80(ctx);});
}
void native_record_queue_reset_4ad190(NativeRuntimeContext& c){
    auto& g=native_race_ghosts(c);g.record_queue_841ff0[0]=0;g.record_queue_841ff0[1]=0;
}
bool native_records_open_440870(NativeRuntimeContext& c){
    // 440750(4): events 9..12; callbacks from the car-kind table 59C544..59C554.
    auto& g=native_race_ghosts(c);
    return run_ghost(c,[&](PcRaceContext& ctx){
        for(std::uint32_t k=0;k<4u;++k){
            const std::uint32_t event=9u+k;
            auto& slot=c.event_state.slots[event];
            slot.flags=std::uint8_t((slot.flags&0xe7u)|1u);
            slot.display_scene=c.event_descriptors[event].display_scene;
            slot.init_callback=0x4ad1f0u;slot.ctrl_callback=0x4ad080u;slot.disp_callback=0x4ae5f0u;
            slot.shadow_callback=0x4adac0u;slot.dest_callback=0x470560u;
            record_car_queue_4ad1a0(ctx.m,event,std::uint8_t(k+1u),0u,0u);
        }
        ++g.cars_opened;});
}
void native_ghost_queue_reset_4acfb0(NativeRuntimeContext& c){
    auto& g=native_race_ghosts(c);PcRaceMemory m;g.map(m);ghost_car_queue_reset_4acfb0(m);
}
bool native_ghost_car_invoke(NativeRuntimeContext& c,std::uint32_t callback,driving::PcMatrixStack& matrices){
    if(callback!=0x4ad000u&&callback!=0x4ace40u&&callback!=0x470560u&&callback!=0x4ad1f0u&&callback!=0x4ad080u)return false;
    auto& g=native_race_ghosts(c);
    const std::uint32_t event=c.event_state.current_slot;
    if(event<9u||event>12u){g.fault=callback;g.error="ghost car callback outside events 9..12";return true;}
    const std::uint32_t car=NativeGhostRuntime::CarBase+(event-9u)*0x10f0u;
    if(callback==0x4ad000u){++g.car_inits;(void)run_ghost(c,[&](PcRaceContext& ctx){ghost_car_init_4ad000(ctx,car);},&matrices);}
    else if(callback==0x4ace40u){++g.car_controls;(void)run_ghost(c,[&](PcRaceContext& ctx){ghost_car_control_4ace40(ctx,car);},&matrices);}
    else if(callback==0x4ad1f0u){++g.car_inits;(void)run_ghost(c,[&](PcRaceContext& ctx){record_car_init_4ad1f0(ctx,car);},&matrices);}
    else if(callback==0x4ad080u){++g.car_controls;(void)run_ghost(c,[&](PcRaceContext& ctx){record_car_control_4ad080(ctx,car);},&matrices);}
    else{++g.car_destroys;auto& works=c.race.manager.car_works_7815a0;driving::Bytes(works.data()+(event-9u)*0x10f0u,0x10f0u).put32(0x10d0,0xffffffffu);}   // 470560
    return true;
}
bool native_records_player_47f780(NativeRuntimeContext& c,driving::PcMatrixStack& matrices){
    return run_ghost(c,[](PcRaceContext& ctx){records_player_47f780(ctx,0x7804b0u);},&matrices);
}
bool native_records_ranking_47ef30(NativeRuntimeContext& c,std::uint32_t preset,std::uint32_t& eax){
    return run_ghost(c,[&](PcRaceContext& ctx){eax=records_ranking_47ef30(ctx,preset);});
}
bool native_records_stage_flags_47ee70(NativeRuntimeContext& c,std::array<std::uint32_t,32>& flags,std::array<bool,32>& written){
    constexpr std::uint32_t Out=0x7ffe3100u,Unset=0xdeadbeefu;
    std::array<std::uint8_t,0x80> bytes{};
    for(std::uint32_t k=0;k<32u;++k)put32(bytes.data()+k*4u,Unset);
    const bool ok=run_ghost(c,[&](PcRaceContext& ctx){
        const auto mark=ctx.m.mark();ctx.m.map(Out,bytes.data(),bytes.size());
        try{records_stage_flags_47ee70(ctx,Out);}catch(...){ctx.m.release(mark);throw;}
        ctx.m.release(mark);});
    for(std::uint32_t k=0;k<32u;++k){flags[k]=get32(bytes.data()+k*4u);written[k]=flags[k]!=Unset;}
    return ok;
}
bool native_ghost_record_4671d0(NativeRuntimeContext& c,driving::PcMatrixStack& matrices){
    return run_ghost(c,[](PcRaceContext& ctx){ghost_record_4671d0(ctx,0x7804b0u);},&matrices);
}
bool native_ghost_goal_467190(NativeRuntimeContext& c,driving::PcMatrixStack& matrices){
    return run_ghost(c,[](PcRaceContext& ctx){ghost_goal_467190(ctx);},&matrices);
}
bool native_ghost_restart_467e00(NativeRuntimeContext& c,driving::PcMatrixStack& matrices){
    return run_ghost(c,[](PcRaceContext& ctx){ghost_restart_467e00(ctx);},&matrices);
}
bool native_ghost_save_467880(NativeRuntimeContext& c){
    if(!c.race_ghosts)return true;                               // no Time Attack ghost module: 7F92C4 is 0
    ++native_race_ghosts(c).save_prepares;
    return run_ghost(c,[](PcRaceContext& ctx){ghost_save_prepare_467880(ctx);});
}
bool native_ghost_save_467960(NativeRuntimeContext& c){
    if(!c.race_ghosts)return true;
    ++native_race_ghosts(c).save_stores;
    return run_ghost(c,[](PcRaceContext& ctx){ghost_save_store_467960(ctx);});
}
bool native_records_store_480d00(NativeRuntimeContext& c){
    return run_ghost(c,[](PcRaceContext& ctx){records_prepare_480d00(ctx);});
}
bool native_records_store_481180(NativeRuntimeContext& c,std::uint32_t name){
    return run_ghost(c,[name](PcRaceContext& ctx){records_store_481180(ctx,name);});
}
bool native_ghost_save_close_416830(NativeRuntimeContext& c){
    if(!c.race_ghosts)return true;
    return run_ghost(c,[](PcRaceContext& ctx){ghost_save_close_416830(ctx);});
}
void native_ghost_release_465fa0(NativeRuntimeContext& c){
    ++native_race_ghosts(c).releases;
    (void)run_ghost(c,[](PcRaceContext& ctx){ghost_release_465fa0(ctx);});
}
void native_ghost_frame_4666a0(NativeRuntimeContext& c){
    if(!c.race_ghosts)return;                                   // 7F9224 is 0 until 467AC0 allocates it
    std::uint32_t event9=c.event_state.slots[9].work_token;     // [799D54]
    (void)run_ghost(c,[&](PcRaceContext& ctx){
        ctx.m.map(0x799d54u,reinterpret_cast<std::uint8_t*>(&event9),4);
        ghost_frame_4666a0(ctx);});
}
bool native_ghost_car_slot(const driving::PcEventSlot& s){
    // TA ghost cars (4407E0: 4AD000 / 4ACE40) and variant-0 record cars (440750: 4AD1F0 / 4AD080).
    if(s.dest_callback!=0x470560u)return false;
    return (s.init_callback==0x4ad000u&&s.ctrl_callback==0x4ace40u)||(s.init_callback==0x4ad1f0u&&s.ctrl_callback==0x4ad080u);
}
bool native_ghost_car_display(NativeRuntimeContext& c,PcSceneRenderer& r,std::uint32_t callback,std::uint32_t work){
    if(callback!=0x4ae5f0u&&callback!=0x4adac0u)return false;
    // the car works of events 9..31 (TA ghost cars 9..12, traffic / racer cars 9..31)
    if(work<NativeGhostRuntime::CarBase||work>=NativeGhostRuntime::CarBase+23u*0x10f0u||(work-NativeGhostRuntime::CarBase)%0x10f0u)return false;
    auto& g=native_race_ghosts(c);
    auto& matrices=r.matrices();
    const auto depth=matrices.depth;
    g.draw_list.clear();
    try{
        PcRaceMemory m;g.map(m);ghost_map_car_tables(m);
        auto& works=c.race.manager.car_works_7815a0;m.map(NativeGhostRuntime::CarBase,works.data(),23u*0x10f0u);
        std::uint32_t variant=c.game_mode.game_variant,mode=c.mode_state.current,player_ptr=0x7804b0u,net=c.start_mode.manager_state_7f94c0;
        m.map(0x780258u,reinterpret_cast<std::uint8_t*>(&variant),4);m.map(0x78026cu,reinterpret_cast<std::uint8_t*>(&mode),4);
        m.map(0x7f94c0u,reinterpret_cast<std::uint8_t*>(&net),4);
        auto& player=c.event_function36.car_select.car_799d18;
        m.map(0x799d18u,reinterpret_cast<std::uint8_t*>(&player_ptr),4);m.map(0x7804b0u,player.data(),0x10f0u);
        m.map(RaceManagerState::base,reinterpret_cast<std::uint8_t*>(&c.race.manager.state),sizeof(RaceManagerState));   // 7D39F0 (44FF10)
        m.map(0x7c23e0u,c.event_function36.frontend_profiles.active.data(),c.event_function36.frontend_profiles.active.size());   // 7C24C9 option byte
        std::array<std::uint8_t,0x200> flags{};for(std::size_t i=0;i<flags.size();++i)flags[i]=std::uint8_t(c.event_state.slots[i].flags);
        m.map(0x79fb48u,flags.data(),flags.size());
        m.map(0x842038u,g.alpha_842038.data(),g.alpha_842038.size());
        PcRaceContext ctx{m,matrices,[&](const PcRaceCall& k)->std::uint32_t{
            if(k.pc==0x4957f0u)return c.start_mode.selection_active_836374?1u:0u;
            // the selected Races record [83637C] (mission manager)
            auto record_u32=[&](std::uint32_t off,std::uint32_t& v)->bool{
                const auto* races=c.start_mode.scene_owner_race_assets;const auto i=c.mission.manager.record_83637c;
                if(!races||i<0||std::uint32_t(i)>=races->race_count)return false;
                const std::size_t at=races->races_offset+std::size_t(i)*0x44u+off;
                if(at+4u>races->bytes.size())throw GhostUnported(0x4f1a90u,"Races record outside Races.bin");
                std::memcpy(&v,races->bytes.data()+at,4);return true;};
            if(k.pc==0x495b00u)return c.game_mode.game_variant==4u?1u:0u;                        // [780258] == 4
            if(k.pc==0x4962d0u){std::uint32_t t;return (c.start_mode.selection_active_836374&&record_u32(0x20u,t)&&t==6u)?1u:0u;}
            if(k.pc==0x495860u){std::uint32_t v;return record_u32(0x40u,v)?v:4u;}
            if(k.pc==0x46f990u){   // othcarCalcCsLenDiff on the traffic module's memory (the same PC addresses)
                std::int32_t d;
                if(native_race_traffic_cs_diff(c,matrices,k.args[0],k.args[1],d))return std::uint32_t(d);
            }
            char pcs[16];std::snprintf(pcs,sizeof pcs,"%X",k.pc);
            throw GhostUnported(k.pc,std::string("car display: PC service ")+pcs+" not ported");},&g.draw_list};
        if(callback==0x4ae5f0u){ghost_car_display_4ae5f0(ctx,work);++g.displays;}
        else{ghost_car_shadow_4adac0(ctx,work);++g.shadows;}
    }catch(const std::exception& e){
        while(matrices.depth>depth)driving::pc_matrix_pop(matrices);
        ++g.display_faults;g.display_error=e.what();g.draw_list.clear();return true;
    }
    if(std::getenv("OR2_GHOST_TRACE")){const driving::Bytes gc(c.race.manager.car_works_7815a0.data()+(work-NativeGhostRuntime::CarBase),0x10f0);
        const driving::Bytes pl(c.event_function36.car_select.car_799d18.data(),c.event_function36.car_select.car_799d18.size());
        std::fprintf(stderr,"[ghost] pos=(%.2f,%.2f,%.2f) m=(%.2f,%.2f,%.2f) player=(%.2f,%.2f,%.2f) lod=%d model=%d\n",gc.f32(0x14),gc.f32(0x18),gc.f32(0x1c),
            gc.f32(0xb0+0x30),gc.f32(0xb0+0x34),gc.f32(0xb0+0x38),pl.f32(0x14),pl.f32(0x18),pl.f32(0x1c),int(gc.i8(0x328)),int(gc.i8(0x11)));}
    if(std::getenv("OR2_GHOST_TRACE"))std::fprintf(stderr,"[ghost] %s work=%x draws=%zu flags4=%x c5c=%x b68=%f\n",callback==0x4ae5f0u?"disp":"shadow",work,g.draw_list.size(),
        driving::Bytes(c.race.manager.car_works_7815a0.data()+(work-NativeGhostRuntime::CarBase),0x10f0).u32(4),
        driving::Bytes(c.race.manager.car_works_7815a0.data()+(work-NativeGhostRuntime::CarBase),0x10f0).u32(0xc5c),
        double(driving::Bytes(c.race.manager.car_works_7815a0.data()+(work-NativeGhostRuntime::CarBase),0x10f0).f32(0xb68)));
    try{g.objects_drawn+=native_race_draw_list_execute(c,r,g.draw_list);}
    catch(const std::exception& e){++g.display_faults;g.display_error=std::string("draw list: ")+e.what();}
    if(std::getenv("OR2_GHOST_TRACE")){auto& o=native_race_area(c);std::fprintf(stderr,"[ghost]   skipped=%u bank_failures=%u first=%x last=%s\n",o.skipped_objects,o.bank_failures,
        g.draw_list.size()>1?g.draw_list[1].args[0]:0u,o.last_error.c_str());
        for(const auto& d:g.draw_list)std::fprintf(stderr," %x(%x,%x,%x)",d.pc,d.args[0],d.args[1],d.args[2]);std::fprintf(stderr,"\n");}
    return true;
}
}
