#include "platform/pc_network.hpp"
#include "platform/race_hud_runtime.hpp"
#include "platform/race_end_runtime.hpp"
#include "platform/sprite_2d_runtime.hpp"
#include "platform/native_runtime.hpp"
#include "platform/pc_scene_renderer.hpp"
#include "platform/race_area_runtime.hpp"
#include "platform/race_manager_runtime.hpp"
#include "platform/race_traffic_runtime.hpp"
#include "platform/race_ghosts_runtime.hpp"
#include "platform/retail_asset_store.hpp"
#include "platform/frontend_ui_resources.hpp"
#include "platform/race_variant_owners.hpp"
#include "driving/pc_wall_rebound.hpp"
#include <cstdio>
#include <cstring>
#include <sstream>
namespace outrun::platform {
namespace {
using driving::Bytes;
std::string hex(std::uint32_t v){char t[16];std::snprintf(t,sizeof t,"%x",v);return t;}
float bf(std::uint32_t u){float f;std::memcpy(&f,&u,4);return f;}
// 5E0AC8[(int8)[655B59]]: {x, y, z} model offsets of 495D90 (first 32 rows of the EXE table).
constexpr std::uint32_t ModelOffset5e0ac8[32][3]={
    {0x3eb0a3d7u,0x3fa51eb8u,0x3d8f5c29u},{0x3ea5e354u,0x3fa45a1du,0x3e3f7ceeu},{0x3eb0a3d7u,0x3fa51eb8u,0x3d8f5c29u},
    {0x3eb9db23u,0x3fa26e98u,0x3e353f7du},{0x3eb0a3d7u,0x3fb020c5u,0x3ee45a1du},{0x3eb0a3d7u,0x3fa51eb8u,0x3d8f5c29u},
    {0x3eb0a3d7u,0x3fa51eb8u,0x3e000000u},{0x3eb0a3d7u,0x3fa51eb8u,0x3d8f5c29u},{0x3eb0a3d7u,0x3fa51eb8u,0x3d8f5c29u},
    {0x3eb0a3d7u,0x3fa51eb8u,0x3d8f5c29u},{0x3eb0a3d7u,0x3fa51eb8u,0x3d8f5c29u},{0x3eb0a3d7u,0x3fa51eb8u,0x3d8f5c29u},
    {0x3eb0a3d7u,0x3fa51eb8u,0x3d8f5c29u},{0x3eb0a3d7u,0x3fa51eb8u,0x3d8f5c29u},{0x3eb0a3d7u,0x3fa51eb8u,0x3d8f5c29u},
    {0x3eb0a3d7u,0x3fa51eb8u,0x3d8f5c29u},{0x3eb0a3d7u,0x3fa51eb8u,0x3d8f5c29u},{0x3eb0a3d7u,0x3fa51eb8u,0x3d8f5c29u},
    {0x3eb0a3d7u,0x3fa51eb8u,0x3d8f5c29u},{0x3eb0a3d7u,0x3fa51eb8u,0x3d8f5c29u}};
constexpr std::uint32_t ModelOffsetRows=20u;                 // rows verified above (0..19)
constexpr std::uint32_t HudRacersBase=0x44000000u,HudRankBase=0x44100000u;   // synthetic: [80FB00], [80FB1C]
void note(NativeRaceHudRuntime& o,std::uint32_t pc){
    if(!pc)return;
    if(o.missing[pc]++==0)o.missing_order.push_back({pc,o.frame});
    if(o.frame_missing_frame!=o.frame){o.frame_missing.clear();o.frame_missing_frame=o.frame;}
    ++o.frame_missing[pc];
}
void begin(NativeRuntimeContext& c,NativeRaceHudRuntime& o){
    o.frame=c.completed_frames;
    native_sprite2d(c).state.update_index_8a8cdc=c.frame_state.update_index_8a8cdc;
}
// Report the 2D producers' latched faults of this call into the owner.
void collect_2d(NativeRuntimeContext& c,NativeRaceHudRuntime& o){
    auto& s=native_sprite2d(c).state;
    for(const auto& [pc,n]:s.missing_counts)for(std::uint32_t k=0;k<n;++k)note(o,pc);
    if(s.missing&&o.last_error.empty())o.last_error="2D "+hex(s.missing)+" at "+hex(s.fault);
    s.missing_counts.clear();s.clear_report();
}
// 424940 requests into the race sound queue 9563E8 (player car's queue),
// gated by the event-383 flag byte 79FCC7 as 424940 does.
void drain_sounds(NativeRuntimeContext& c,NativeRaceHudRuntime& o){
    if(o.pending_sounds.empty())return;
    auto& w=c.race.car_world;
    driving::PcSoundQueue q{Bytes(w.sound_entries_9563e8.data(),w.sound_entries_9563e8.size()),
                            Bytes(w.sound_state.data(),w.sound_state.size()),c.event_state.slots[383].flags};
    for(const auto id:o.pending_sounds){
        ++o.sounds_424940[id];
        const auto before=w.sound_state;
        driving::pc_enqueue_sound(q,id);
        if(before!=w.sound_state)++o.sounds_enqueued;else ++o.sounds_dropped;
    }
    o.pending_sounds.clear();
}
// ---- NAVI memory -----------------------------------------------------------------
void map_word(PcRaceMemory& m,std::uint32_t a,void* p,std::size_t n){m.map(a,static_cast<std::uint8_t*>(p),n);}
void rebuild(NativeRuntimeContext& c,NativeRaceHudRuntime& o){
    auto& m=o.memory;m.clear();
    for(std::size_t i=0;i<EmbeddedExeRangeCount;++i)m.map_const(EmbeddedExeRanges[i].base,EmbeddedExeRanges[i].data,EmbeddedExeRanges[i].size);
    // The AREA module's memory (course tables 6A54E0.. for 44C990), shadowed by what follows.
    if(c.race_area)for(const auto& r:native_race_area_memory(c,nullptr,true).regions()){if(r.writable)m.map(r.base,r.data,r.size);else m.map_const(r.base,r.data,r.size);}
    // The Time Attack ghost module 7F8D80.. (variant 7 sector records 467840 / 466D60), under the
    // NAVI blocks mapped below.
    if(c.race_ghosts)native_race_ghosts(c).map(m);
    // Record module words 810124 / 810438 / 81044C / 81373C / 813740 and its QHOT tables: the
    // ghost runtime's race_records state (480FE0 runs in variant 0 only; the .bss keeps what a
    // previous variant-0 race left, as on the PC), .bss zero before any ghost runtime exists.
    if(!c.race_ghosts){
        m.map_const(0x810120u,o.recorder_810120.data(),o.recorder_810120.size());
        m.map_const(0x813738u,o.recorder_813738.data(),o.recorder_813738.size());
    }
    auto& area=native_race_area(c);
    area.area.map(m);                                           // 7D2D80..7D34C8, 635F2C..636BC4
    auto& rt=c.game_mode.course_runtime;                         // the same overlays as the area owner
    map_word(m,0x635f2cu,&rt.stage_key_635f2c,4);map_word(m,0x635f30u,&rt.stage_value_635f30,4);
    m.map(0x7d2da0u,rt.matrix_7d2da0.data(),64);m.map(0x7d3130u,rt.matrix_7d3130.data(),64);m.map(0x7d3190u,rt.matrix_7d3190.data(),64);
    m.map(0x7d2de0u,rt.selected_7d2de0.data(),0x78);m.map(0x7d30a8u,rt.selected_7d30a8.data(),0x78);m.map(0x7d33d8u,rt.fallback_record_7d33d8.data(),0x78);
    map_word(m,0x7d3124u,rt.zero_7d3124.data(),12);map_word(m,0x7d3178u,rt.zero_7d3178.data(),12);
    map_word(m,0x7d33b0u,&rt.fallback_gate_7d33b0,4);map_word(m,0x7d33c0u,&rt.max_depth_7d33c0,4);map_word(m,0x7d33c4u,&rt.active_count_7d33c4,4);
    map_word(m,0x7d2e80u,&c.race.area_state_7d2e80,4);map_word(m,0x7d2e88u,&c.race.area_state_7d2e88,4);
    if(area.table)m.map(RaceAreaTableBase,area.table,area.table_bytes);
    // Read-only mode/route words.
    auto& mr=o.mirrors;
    mr[0]=c.start_mode.course_preset;mr[1]=c.game_mode.game_variant;mr[2]=c.mode_state.current;
    mr[3]=HudRacersBase;mr[4]=HudRankBase;
    const auto& mission=c.mission.manager;
    const auto* races=c.start_mode.scene_owner_race_assets;
    mr[5]=(mission.record_83637c>=0&&races)?NativeRacesBlobBase+std::uint32_t(races->races_offset)+std::uint32_t(mission.record_83637c)*0x44u:0u;
    mr[6]=c.start_mode.selection_active_836374?1u:0u;
    m.map_const(0x78024cu,reinterpret_cast<const std::uint8_t*>(&mr[0]),4);
    m.map_const(0x780258u,reinterpret_cast<const std::uint8_t*>(&mr[1]),4);
    m.map_const(0x78026cu,reinterpret_cast<const std::uint8_t*>(&mr[2]),4);
    m.map(0x780248u,&c.start_mode.game_flag_780248,1);
    // Event records 799B30 (+08 work pointers: 799D18 car, 79F574 camera) and flags 79FB48.
    Bytes rec(o.event_records_799b30.data(),o.event_records_799b30.size());
    for(std::uint32_t id=0;id<410u;++id){
        const auto& s=c.event_state.slots[id];const auto b=id*0x3cu;
        rec.put32(b,s.descriptor_token);rec.put32(b+4,s.event_id);rec.put32(b+8,s.work_token);rec.put32(b+0xc,s.display_scene);
        rec.put32(b+0x10,s.init_callback);rec.put32(b+0x14,s.ctrl_callback);rec.put32(b+0x18,s.disp_callback);rec.put32(b+0x1c,s.shadow_callback);
        rec.put32(b+0x20,s.dest_callback);rec.put32(b+0x24,s.aux24);rec.put32(b+0x28,s.aux28);rec.put32(b+0x2c,s.close_guard);
        rec.put32(b+0x30,s.function_id);rec.put32(b+0x34,s.aux34);rec.put32(b+0x38,s.aux38);
        o.event_flags_79fb48[id]=s.flags;
    }
    m.map_const(0x799b30u,o.event_records_799b30.data(),o.event_records_799b30.size());
    m.map_const(0x79fb48u,o.event_flags_79fb48.data(),o.event_flags_79fb48.size());
    auto& cs=c.event_function36.car_select;
    if(const auto car=c.event_state.slots[8].work_token){
        m.map(car,cs.car_799d18.data(),cs.car_799d18.size());
        std::uint32_t params;std::memcpy(&params,cs.car_799d18.data()+0x2b4,4);
        if(params)m.map(params,cs.parameters.data(),cs.parameters.size());     // [car+2B4] parameter view
    }
    if(const auto cam=c.event_state.slots[385].work_token)m.map(cam,cs.camera_79fe10.data(),cs.camera_79fe10.size());
    // Car works of events 9..31 (7815A0 + k*0x10F0, the race manager's storage): 4BC9E0 reads
    // the rival flags +12, 4BAD20 the rival labels.
    {auto& works=c.race.manager.car_works_7815a0;m.map(NativeRaceManager::CarWorkBase,works.data(),works.size());}
    // Racers (47CF40 owner) and the Races blob (80FB0C / 83637C point into it).
    auto& r=c.mission.racers;
    if(!r.racers_80fb00.empty())m.map(HudRacersBase,r.racers_80fb00.data(),r.racers_80fb00.size());
    if(!r.table_80fb1c.empty())m.map(HudRankBase,r.table_80fb1c.data(),r.table_80fb1c.size());
    m.map_const(0x80fb00u,reinterpret_cast<const std::uint8_t*>(&mr[3]),4);
    map_word(m,0x80fb04u,&r.count_80fb04,4);map_word(m,0x80fb0cu,&r.config_80fb0c,4);
    map_word(m,0x80fb14u,&c.race.car_world.gate_80fb14,4);   // racers gate (4BEB8E: rival labels / markers)
    m.map_const(0x80fb1cu,reinterpret_cast<const std::uint8_t*>(&mr[4]),4);map_word(m,0x80fb2cu,&r.v80fb2c,4);
    map_word(m,0x64e190u,&r.v64e190,4);map_word(m,0x64e194u,&r.v64e194,4);map_word(m,0x680ad4u,&r.v680ad4,4);
    map_word(m,0x803710u,&r.length_803710,4);map_word(m,0x804388u,&r.per_lap_804388,2);
    if(!c.mission.races_relocated.empty())m.map_const(NativeRacesBlobBase,c.mission.races_relocated.data(),c.mission.races_relocated.size());
    else if(races)m.map_const(NativeRacesBlobBase,races->bytes.data(),races->bytes.size());
    native_race_attack_map(c,m);   // variant 9: 686254.. and RaceAttack.bin (racer names)
    m.map_const(0x83637cu,reinterpret_cast<const std::uint8_t*>(&mr[5]),4);
    m.map_const(0x836374u,reinterpret_cast<const std::uint8_t*>(&mr[6]),1);
    map_word(m,0x836388u,&c.mission.manager.timer_836388,4);
    m.map(0x7c23e0u,c.event_function36.frontend_profiles.active.data(),c.event_function36.frontend_profiles.active.size());
    m.map(RaceManagerState::base,reinterpret_cast<std::uint8_t*>(&c.race.manager.state),sizeof(RaceManagerState));   // 7D3650..7D39FF
    map_word(m,0x83036du,&c.start_mode.vehicle_variant_83036d,1);   // 48B1A0
    map_word(m,0x8361b4u,&c.start_mode.route_gate_8361b4,1);         // 495490
    map_word(m,0x830394u,&c.start_mode.flag_830394,1);
    map_word(m,0x7d2698u,&native_race_end(c).language_7d2698,4);   // 4493C0 language index (rank suffixes, units)
    map_word(m,0x95af0cu,&c.frame_state.frame_counter_95af0c,4);
    map_word(m,0x656234u,&c.start_mode.frontend_prepare.output_code_656234,4);
    map_word(m,0x7551b4u,&o.sprani.current_7551b4,4);
    map_word(m,0x95b214u,&c.event_function36.title_pause_flag_95b214,4);
    map_word(m,0x84210cu,&c.race.clock,sizeof(c.race.clock));
    m.map(0x956bb4u,o.text_cursor_956bb4.data(),o.text_cursor_956bb4.size());
    // NAVI_PUB globals, then the words of other owners inside those blocks.
    o.navi.map(m);
    // CommRace block 7DE418 (the ranks table 7DF118 is inside it).
    m.map(0x7de418u,c.race.car_world.commrace_7de418.data(),c.race.car_world.commrace_7de418.size());
    native_network_map_shared(m);                                     // LAN: the message queue 7EE070, the players' states
    map_word(m,0x7f1938u,&c.race.car_world.clock_7f1938,4);
    map_word(m,0x7f2428u,&c.race.car_world.heart_mode_7f2428,4);
    map_word(m,0x7f8abcu,&c.race.car_world.nos_speed_7f8abc,4);
    map_word(m,0x7f94c0u,&c.start_mode.manager_state_7f94c0,4);
    map_word(m,0x7f95a8u,&rt.force_mode_7f95a8,4);
    map_word(m,0x7f1994u,&o.voice.tail_7f1994,4);map_word(m,0x7f1998u,o.voice.entries_7f1998.data(),sizeof(o.voice.entries_7f1998));
    map_word(m,0x7f1c64u,&o.voice.head_7f1c64,4);map_word(m,0x7f8b40u,&o.voice.last_7f8b40,4);
}
// 43EB60(0x100, &point, 0, 0, &flags): the course ground query (4BA0E0 line of sight); flags is
// its kind output, the attribute word tested against F00002.
struct GroundUser { NativeRuntimeContext* c; PcSceneRenderer* r; };
bool ground_43eb60(void* user,std::uint32_t mode,driving::CourseProbe& p,std::uint32_t& flags){
    auto& u=*static_cast<GroundUser*>(user);
    if(!u.r||mode!=0x100u)return false;
    const auto tables=u.c->start_mode.scene_owner_course_world.tables();
    driving::CourseWorldQuery q{tables,u.r->matrices(),u.c->event_function36.car_select.race_prediction};
    std::uint32_t kind=flags;                       // [arg4]: the attribute word of the selected polygon
    driving::get_y_position_prog(q,0x100u,p,nullptr,nullptr,&kind);
    flags=kind;return true;
}
// Heart Attack routines (race_traffic: 464D20, the display callbacks) run on the traffic
// memory; the HUD owner adds its blocks there (the same storage this owner maps), then
// rebuilds its own images (events opened / works written by the routine).
struct HeartUser { NativeRuntimeContext* c; NativeRaceHudRuntime* o; driving::PcMatrixStack* st; NaviPubServices* navi{}; PcSceneRenderer* renderer{}; };
void heart_map(void* user,PcRaceMemory& m){
    auto& h=*static_cast<HeartUser*>(user);
    native_race_hud_map_heart(*h.c,m);
}
bool heart_run(void* user,std::uint32_t pc){
    auto& h=*static_cast<HeartUser*>(user);auto& o=*h.o;
    if(pc==0x46ed20u&&h.c->race.car_world.heart_mode_7f2428!=9u)return true;   // 46ED20 draws only in state 9 (45C440)
    std::vector<RaceHudDraw> draws;std::vector<PcVehicleDrawCall> models;
    NativeHeartBinding b{user,heart_map,&draws};
    b.display=pc==0x462cd0u||pc==0x46ed20u;b.models=&models;b.renderer=h.renderer;
    const bool ok=native_race_traffic_heart(*h.c,*h.st,pc,b);
    rebuild(*h.c,o);
    auto& pool=h.c->event_function36.frontend_sprites;
    for(const auto& d:draws){
        if(d.pc==0x4289e0u){   // 428A10 (scale 1, s = argument 4, angle 0) like 4289B0
            o.navi_draws.push_back(d);
            sprani_draw_428a10(pool,o.sprani,*h.st,d.args[0],&d.matrix,d.args[1],std::int32_t(d.args[3]),1.0f,bf(d.args[4]),0.0f,o.navi_scene_draws,o.navi_blend);
            continue;
        }
        if(!h.navi){note(o,d.pc);continue;}
        if(d.pc==0x429530u)navi_emit_draw(*h.navi,d);
        else if(d.pc==0x4295d0u){
            std::uint32_t mn;std::memcpy(&mn,&d.matrix[3],4);
            if(!navi_sprite3d_4295d0(*h.navi,d.args[0],{d.matrix[0],d.matrix[1],d.matrix[2]},std::int32_t(d.args[1]),std::int32_t(d.args[2]),
                                     bf(d.args[3]),bf(d.args[4]),bf(d.args[5]),d.args[6],bf(mn)))note(o,0x4295d0u);
        }else if(d.pc==0x4bc990u){
            char text[17]{};std::memcpy(text,&d.args[3],16);
            if(!navi_digits_4ba9d0(*h.navi,d.args[0],std::int32_t(d.args[1]),std::int32_t(d.args[2]),text,9,1.0f))note(o,0x4bc990u);
        }else note(o,d.pc);
    }
    if(!models.empty()){
        if(!h.renderer)note(o,0x405360u);
        else try{(void)native_race_draw_list_execute(*h.c,*h.renderer,models);}catch(const std::exception& e){note(o,0x405360u);if(o.last_error.empty())o.last_error=std::string("heart models: ")+e.what();}
    }
    if(!ok){const auto& t=native_race_traffic(*h.c);if(t.heart_latched&&o.last_error.empty())o.last_error="heart attack: "+t.heart_error;}
    return ok;
}
NaviPubServices navi_services(NativeRuntimeContext& c,NativeRaceHudRuntime& o,driving::PcMatrixStack& st,GroundUser& ground){
    NaviPubServices s;
    s.m=&o.memory;s.sprites=&c.event_function36.frontend_sprites;s.pause_95b214=c.event_function36.title_pause_flag_95b214;
    s.matrices=&st;s.draws=&o.navi_draws;s.scene_draws=&o.navi_scene_draws;s.sprani=&o.sprani;s.blend_9564e0=&o.navi_blend;
    s.sounds=&o.pending_sounds;s.user=&ground;s.ground_43eb60=ground_43eb60;
    s.text=c.event_function36.frontend_text;
    s.lan_user=&c;
    s.lan_rank=[](void* u,std::uint8_t id,std::uint32_t& rank){
        std::array<std::uint8_t,0x40> mb{};driving::PcMatrixStack mst{driving::Bytes(mb.data(),mb.size())};
        return native_race_network_call(*static_cast<NativeRuntimeContext*>(u),mst,0x45a2b0u,{id},rank);};
    return s;
}
// ---- 2D execution of recorded HUD draws ------------------------------------------
void render_scene_draw(NativeRuntimeContext& c,NativeRaceHudRuntime& o,PcD3D9Device& device,driving::PcMatrixStack& st,const SpraniDraw& d){
    native_sprite2d_ensure_bank(c,device,d.token>>16);
    const auto root=pc_sprani_scene_root(native_sprite2d(c).state,d.token);
    if(!root){note(o,0x429460u);return;}               // the bank animation is not loaded natively
    ++o.scene_tokens[d.token];
    driving::pc_matrix_push(st);
    auto top=st.current();for(unsigned k=0;k<16;++k)top.putf(k*4,d.matrix[k]);
    auto& s=native_sprite2d(c).state;
    s.mask_986b28=0;s.id_7551b4=d.id_7551b4;
    s.flag_9564f4=d.depth==1u?1u:0u;s.flag_9564f8=d.depth==2u?1u:0u;   // 428AF0 (3D sprites)
    pc_sprani_render_429460(s,device,st,root,d.frame,d.scale,d.layer);
    s.flag_9564f4=0;s.flag_9564f8=0;
    driving::pc_matrix_pop(st);
}
// Glyphs no RaceHudGlyphRange draw covers (text a service produced directly, such as
// the 446A50 slot captions) are queued after the display's draws.
void queue_unmarked_glyphs(NativeRuntimeContext& c,PcD3D9Device& device,const std::vector<RaceHudDraw>& draws,
                           const std::vector<FrontendGlyph>& glyphs){
    std::vector<bool> covered(glyphs.size(),false);
    for(const auto& d:draws)if(d.pc==RaceHudGlyphRange)
        for(std::uint32_t k=d.args[0];k<d.args[1]&&k<glyphs.size();++k)covered[k]=true;
    std::vector<FrontendGlyph> rest;
    for(std::size_t k=0;k<glyphs.size();++k)if(!covered[k])rest.push_back(glyphs[k]);
    native_sprite2d_glyph_records(c,device,rest);
}
void run_navi_draws(NativeRuntimeContext& c,NativeRaceHudRuntime& o,PcD3D9Device& device,driving::PcMatrixStack& st){
    std::size_t scene=0;
    auto& pool=c.event_function36.frontend_sprites;
    // In-race text 956BA0.. (42CA60 font, 42CC60 scale, 42CCA0 colour, 42CCB0 mode, 42CC00 cursor)
    // and 42CCE0 monospace text: 42C390 placement, then 42C720 glyphs / 42C5A0 advances.
    const FrontendFont* font{};FrontendTextStyle style{};FrontendTextCursor cursor{};
    for(const auto& d:o.navi_draws){
        ++o.draws_by_pc[d.pc];
        const auto* a=d.args.data();
        switch(d.pc){
        case 0x42d280u:native_sprite2d_ensure_bank(c,device,(a[0]>>16)&0xffffu);
            pc_image_42d280(native_sprite2d(c).state,a[0],std::int32_t(a[1]),std::int32_t(a[2]),a[3],bf(a[4]),a[5]);break;
        case RaceHudGlyphRange:
            if(o.navi_glyphs&&a[0]<a[1]&&a[1]<=o.navi_glyphs->size())
                native_sprite2d_glyph_records(c,device,std::vector<FrontendGlyph>(o.navi_glyphs->begin()+a[0],o.navi_glyphs->begin()+a[1]));
            break;
        case 0x42cfe0u:{
            auto record=d.record;
            if(a[2]){native_sprite2d_ensure_bank(c,device,(a[1]>>16)&0xffffu);                 // its 42C2F0 entry, now that the bank is loaded
                pc_image_entry_42c2f0(native_sprite2d(c).state,record.data(),a[1]);}
            std::uint32_t t;std::memcpy(&t,record.data(),4);native_sprite2d_ensure_bank(c,device,(t>>16)&0xffffu);
            pc_image_record_42cfe0(native_sprite2d(c).state,record,bf(a[0]));break;}
        case 0x42d200u:native_sprite2d_ensure_bank(c,device,(a[0]>>16)&0xffffu);
            pc_image_42d200(native_sprite2d(c).state,a[0],std::int32_t(a[1]),std::int32_t(a[2]),bf(a[3]),a[4],bf(a[5]),a[6]);break;
        case 0x42d5c0u:native_sprite2d_ensure_bank(c,device,(a[0]>>16)&0xffffu);      // 42D5C0 = 42D5F0(.., 0)
            pc_image_group_42d5f0(native_sprite2d(c).state,a[0],std::int32_t(a[1]),std::int32_t(a[2]),bf(a[3]),a[4],0u);break;
        case 0x42d5f0u:native_sprite2d_ensure_bank(c,device,(a[0]>>16)&0xffffu);
            pc_image_group_42d5f0(native_sprite2d(c).state,a[0],std::int32_t(a[1]),std::int32_t(a[2]),bf(a[3]),a[4],a[5]);break;
        case 0x42cc00u:cursor.x=std::int16_t(a[0]);cursor.y=std::int16_t(a[1]);cursor.origin_x=cursor.x;break;
        case 0x42ca60u:{
            const auto* fonts=c.event_function36.frontend_fonts;
            if(!fonts||a[0]>=10u||fonts->fonts[a[0]].token!=a[0]){font=nullptr;note(o,0x42ca60u);break;}
            font=&fonts->fonts[a[0]];style.scale_x=1.0f;style.scale_y=1.0f;style.color=0xffffffffu;style.flags=1u;break;}
        case 0x42cc60u:{const float x=bf(a[0]),y=bf(a[1]);if(!(x==0.0f))style.scale_x=x;if(!(y==0.0f))style.scale_y=y;break;}
        case 0x42cca0u:style.color=a[0];break;
        case 0x42c360u:style.flags=a[0];break;
        case 0x42cdd0u:{                                            // proportional text (456540 LAN messages)
            if(!font){note(o,0x42cdd0u);break;}
            const std::string text(reinterpret_cast<const char*>(d.record.data()));
            std::vector<FrontendGlyph> glyphs;
            if(!frontend_text_draw(*font,style,cursor,text,glyphs))note(o,0x42cdd0u);
            style.flags=1u;
            native_sprite2d_glyph_records(c,device,glyphs);
            break;}
        case 0x42ccb0u:style.mode=a[0];break;
        case 0x42cce0u:{
            if(!font){note(o,0x42cce0u);break;}
            if(!(style.flags&1u)){note(o,0x42c390u);break;}             // aligned text (42C480 width) is not used by the HUD
            char text[25]{};std::memcpy(text,&a[1],24);
            std::vector<FrontendGlyph> glyphs;
            for(const char* p=text;*p;++p){
                FrontendGlyph g;if(frontend_text_glyph_42c720(*font,style,cursor,std::uint8_t(*p),g))glyphs.push_back(g);
                frontend_text_advance_42c5a0(*font,style,cursor,std::uint8_t(*p));
            }
            style.flags=1u;
            native_sprite2d_glyph_records(c,device,glyphs);
            break;}
        case 0x42ccc0u:{                                            // one monospace glyph (4B9200)
            if(!font){note(o,0x42ccc0u);break;}
            std::vector<FrontendGlyph> glyphs;FrontendGlyph g;
            if(frontend_text_glyph_42c720(*font,style,cursor,std::uint8_t(a[0]),g))glyphs.push_back(g);
            frontend_text_advance_42c5a0(*font,style,cursor,std::uint8_t(a[0]));
            native_sprite2d_glyph_records(c,device,glyphs);
            break;}
        case 0x4249f0u:                                             // effect sound (4BD480 bonus)
            if(c.event_function36.frontend_effect)(void)c.event_function36.frontend_effect(c.event_function36.frontend_effect_user,a[0]);
            else note(o,0x4249f0u);
            break;
        case 0x429530u:case 0x429580u:case 0x4289b0u:case 0x4289e0u:case 0x428af0u:case 0x428980u:{
            FrontendSpriteTiming t{};
            // The car meter banks 23..31 (5C6C00[model], 4B8F30 needle) are loaded by
            // the race resources the native START does not route to the pool: bind the
            // bank's timing on first use, as the resource system would have.
            if(!pool.bank_scene(a[0],t)){
                const auto bank=(a[0]>>16)&0xffffu;std::string err;
                if(bank>=0x20u&&bank<0x4bu&&!o.bank_bind_tried[bank-0x20u]){o.bank_bind_tried[bank-0x20u]=true;
                    if(!native_sprani_bind_bank(c,bank,err))o.bank_bind_error=err;}
                if(!pool.bank_scene(a[0],t))break;                 // 428A10: no scene, no draw
            }
            if(scene<o.navi_scene_draws.size()){native_sprite2d(c).state.blend_9564e0={0.0f,1.0f,1.0f};render_scene_draw(c,o,device,st,o.navi_scene_draws[scene]);}
            ++scene;break;}
        default:note(o,d.pc);break;
        }
    }
    native_sprite2d(c).state.blend_9564e0=o.navi_blend;
}
std::string what(const std::exception& e){return e.what();}
}
// The Heart Attack words (NAVI globals 7F1900 / 7F8980 and their overlays, the voice queue,
// the race clock, 655B59, 7D2698, the score block 84DEF8, the robots' works) for the
// race_traffic memory (variant-2 traffic and the Heart Attack routines read them).
void native_race_hud_map_heart(NativeRuntimeContext& c,PcRaceMemory& m){
    auto& o=native_race_hud(c);
    m.map(0x842800u,o.navi.g842800.data(),o.navi.g842800.size());m.map(0x688b00u,o.navi.g688b00.data(),o.navi.g688b00.size());
    m.map(0x7f1900u,o.navi.g7f1900.data(),o.navi.g7f1900.size());m.map(0x7f8980u,o.navi.g7f8a00.data(),o.navi.g7f8a00.size());
    map_word(m,0x84210cu,&c.race.clock,sizeof(c.race.clock));
    map_word(m,0x7f1938u,&c.race.car_world.clock_7f1938,4);
    map_word(m,0x7f2428u,&c.race.car_world.heart_mode_7f2428,4);
    map_word(m,0x7f8abcu,&c.race.car_world.nos_speed_7f8abc,4);
    map_word(m,0x655b59u,&c.start_mode.course_choice_655b59,1);   // byte [655B59] (low byte, little endian)
    map_word(m,0x7d2698u,&native_race_end(c).language_7d2698,4);   // 4493C0 language index
    // 84DEF8.. score block (4F2AC0 init; 4F2DF0, its per-frame control, is not ported: the
    // rank-change words 84DF00 / 84DF0C stay as 4F2AC0 left them)
    m.map(0x84def8u,c.race.manager.score_84def8.data(),c.race.manager.score_84def8.size());
    c.race.robots.state.map(m);   // 7A01E0.. robot works, 82F11C.. (RobMotion 82F5F0), 654568..
    map_word(m,0x7f1994u,&o.voice.tail_7f1994,4);map_word(m,0x7f1998u,o.voice.entries_7f1998.data(),sizeof(o.voice.entries_7f1998));
    map_word(m,0x7f1c64u,&o.voice.head_7f1c64,4);map_word(m,0x7f8b40u,&o.voice.last_7f8b40,4);
}
NativeRaceHudRuntime& native_race_hud(NativeRuntimeContext& c){
    if(!c.race_hud)c.race_hud=std::make_shared<NativeRaceHudRuntime>();
    return *c.race_hud;
}
bool native_race_hud_event_invoke(NativeRuntimeContext& c,std::uint32_t callback,std::uint32_t,std::uint32_t,
                                  driving::PcMatrixStack& st,PcSceneRenderer* renderer){
    switch(callback){
    case 0x4970f0u:case 0x498590u:case 0x497210u:
    case 0x4bc9e0u:case 0x4bcc40u:case 0x4b8df0u:break;
    default:return false;
    }
    auto& o=native_race_hud(c);begin(c,o);
    auto& pool=c.event_function36.frontend_sprites;
    try{
        switch(callback){
        case 0x4970f0u:gad_pub_init_4970f0(o.gad);++o.gad_inits;break;
        case 0x498590u:{
            std::uint32_t missing{};
            // LAN results (4985E4): the race end's memory and services ([83670C] stays in the GAD block)
            struct Lan{NativeRuntimeContext* c;GadPubState* g;} lan{&c,&o.gad};
            auto lan_results=[](void* u)->int{auto& l=*static_cast<Lan*>(u);
                std::uint32_t done=l.g->u32(0x83670cu);const int r=native_race_end_lan_results_4985e4(*l.c,done);
                l.g->put32(0x83670cu,done);return r;};
            if(!gad_pub_control_498590(o.gad,pool,c.mode_state.current,c.game_mode.game_variant,o.value_84bd00,missing,lan_results,&lan))note(o,missing);
            ++o.gad_controls;break;}
        case 0x497210u:gad_pub_destroy_497210(pool);++o.gad_destroys;break;
        case 0x4bc9e0u:case 0x4bcc40u:case 0x4b8df0u:{
            if(callback!=0x4bc9e0u&&(!o.navi_initialized||o.navi_init_failed)){note(o,callback);break;}
            rebuild(c,o);
            GroundUser g{&c,renderer};
            o.navi_draws.clear();o.navi_scene_draws.clear();o.navi_blend=native_sprite2d(c).state.blend_9564e0;
            auto s=navi_services(c,o,st,g);
            HeartUser hu{&c,&o,&st,&s,renderer};s.heart_user=&hu;s.heart=heart_run;
            const auto depth=st.depth;const auto offset=st.current_offset;
            bool ok=false;
            if(callback==0x4bc9e0u){ok=navi_pub_init_4bc9e0(s);++o.navi_inits;o.navi_initialized=ok;o.navi_init_failed=!ok;}
            else if(callback==0x4bcc40u){ok=navi_pub_control_4bcc40(s);++o.navi_controls;}
            else{ok=navi_pub_destroy_4b8df0(s);++o.navi_destroys;o.navi_initialized=false;}
            if(!ok){note(o,s.missing?s.missing:callback);if(s.fault)++o.unmapped[s.fault];st.depth=depth;st.current_offset=offset;}
            if(!o.navi_draws.empty())note(o,0x42d710u);             // control-time draws are not queued by this owner
            break;}
        }
    }catch(const std::exception& e){o.last_error=hex(callback)+": "+what(e);note(o,callback);}
    drain_sounds(c,o);
    return true;
}
bool native_race_hud_display(NativeRuntimeContext& c,PcSceneRenderer& r,std::uint32_t callback,std::uint32_t,std::uint32_t){
    switch(callback){
    case 0x4998c0u:case 0x4beb00u:case 0x4b0790u:break;
    default:if(!native_arcade_display_callback(callback))return false;break;
    }
    auto& o=native_race_hud(c);begin(c,o);
    auto& st=r.matrices();auto& device=r.flush_context().device;
    const auto depth=st.depth;const auto offset=st.current_offset;
    try{
        switch(callback){
        case 0x4bf3f0u:case 0x4c1fa0u:case 0x4c2420u:case 0x4c22c0u:case 0x4c1e80u:case 0x4c22e0u:case 0x4bee80u:case 0x4b4f20u:case 0x4b4550u:case 0x498010u:
        case 0x4aef60u:case 0x4af100u:case 0x4af220u:case 0x4af280u:{
            // OUTRUN2SP arcade screens (arcade_attract.cpp) and the name entry board
            // (race_name_entry.cpp), drawn like the GAD_PUB displays.
            ++o.arcade_displays;std::uint32_t missing{};
            std::vector<RaceHudDraw> draws;std::vector<FrontendGlyph> glyphs;
            if(!native_race_end_arcade_display(c,callback,draws,glyphs,missing))note(o,missing?missing:callback);
            if(draws.empty()){native_sprite2d_glyph_records(c,device,glyphs);break;}
            o.navi_draws.clear();o.navi_scene_draws.clear();o.navi_blend=native_sprite2d(c).state.blend_9564e0;
            auto& pool=c.event_function36.frontend_sprites;
            for(const auto& d:draws){
                o.navi_draws.push_back(d);
                if(d.pc==0x4289b0u){                                 // (token, layer, &matrix, frame)
                    sprani_draw_428a10(pool,o.sprani,st,d.args[0],d.args[4]?nullptr:&d.matrix,d.args[1],std::int32_t(d.args[3]),1.0f,1.0f,0.0f,o.navi_scene_draws,o.navi_blend);
                    continue;}   // args[4]: the 4289B0 matrix pointer was NULL
                if(d.pc!=0x429530u)continue;
                const auto t=sprani_translation(bf(d.args[1]),bf(d.args[2]));
                sprani_draw_428a10(pool,o.sprani,st,d.args[0],&t,d.args[3],std::int32_t(d.args[4]),1.0f,1.0f,0.0f,o.navi_scene_draws,o.navi_blend);
            }
            o.navi_glyphs=&glyphs;run_navi_draws(c,o,device,st);o.navi_glyphs=nullptr;
            queue_unmarked_glyphs(c,device,draws,glyphs);
            break;}
        case 0x4998c0u:{
            // race_end_modes gad_display_4998c0: its 429530 draws are resolved
            // through 428A10 and executed like the NAVI ones.
            ++o.gad_displays;std::uint32_t missing{};
            std::vector<RaceHudDraw> draws;std::vector<FrontendGlyph> glyphs;
            if(!native_race_end_gad_display(c,draws,glyphs,missing))note(o,missing?missing:0x4998c0u);
            if(draws.empty()){native_sprite2d_glyph_records(c,device,glyphs);break;}   // 42CCC0 -> 42CFE0 into the 2D queue
            o.navi_draws.clear();o.navi_scene_draws.clear();o.navi_blend=native_sprite2d(c).state.blend_9564e0;
            auto& pool=c.event_function36.frontend_sprites;
            for(const auto& d:draws){
                o.navi_draws.push_back(d);
                if(d.pc==0x4289b0u){                                 // 49A060 pause sprite (NULL matrix)
                    sprani_draw_428a10(pool,o.sprani,st,d.args[0],d.args[4]?nullptr:&d.matrix,d.args[1],std::int32_t(d.args[3]),1.0f,1.0f,0.0f,o.navi_scene_draws,o.navi_blend);
                    continue;}
                if(d.pc!=0x429530u)continue;                         // 42D280 / 42D200 images
                const auto t=sprani_translation(bf(d.args[1]),bf(d.args[2]));
                sprani_draw_428a10(pool,o.sprani,st,d.args[0],&t,d.args[3],std::int32_t(d.args[4]),1.0f,1.0f,0.0f,o.navi_scene_draws,o.navi_blend);
            }
            o.navi_glyphs=&glyphs;run_navi_draws(c,o,device,st);o.navi_glyphs=nullptr;
            queue_unmarked_glyphs(c,device,draws,glyphs);
            break;}
        case 0x4b0790u:{   // variant 9 display (race_variant_owners): road markers, rank markers on the NAVI state
            const bool navi=o.navi_initialized&&!o.navi_init_failed;
            GroundUser g{&c,&r};
            o.navi_draws.clear();o.navi_scene_draws.clear();o.navi_blend=native_sprite2d(c).state.blend_9564e0;
            std::uint32_t missing=0,fault=0;
            const bool ok=native_race_attack_display_4b0490(c,r,[&]{
                if(!navi){missing=0x4b04d0u;return false;}   // the rank markers need the NAVI state (event 388)
                rebuild(c,o);auto s=navi_services(c,o,st,g);
                const bool done=navi_rank_markers_4b04d0(s);
                if(!done){missing=s.missing;fault=s.fault;}
                return done;});
            if(!ok){note(o,missing?missing:0x4b0490u);if(fault)++o.unmapped[fault];st.depth=depth;st.current_offset=offset;}
            run_navi_draws(c,o,device,st);
            break;}
        case 0x4beb00u:{
            ++o.navi_displays;
            if(!o.navi_initialized||o.navi_init_failed){note(o,0x4beb00u);break;}
            rebuild(c,o);
            GroundUser g{&c,&r};
            o.navi_draws.clear();o.navi_scene_draws.clear();o.navi_blend=native_sprite2d(c).state.blend_9564e0;
            auto s=navi_services(c,o,st,g);
            {   // sprites the course objects submitted during their controls (429530 / 4289E0 / 428AC0)
                auto& tr=native_race_traffic(c);auto& pool=c.event_function36.frontend_sprites;
                for(const auto& d:tr.object_draws){
                    if(d.pc==0x428ac0u){
                        if(!navi_sprite_428ac0(s,d.args[0],d.matrix,std::int32_t(d.args[2]),bf(d.args[3]),bf(d.args[4]),d.args[5]))note(o,0x428ac0u);
                        continue;}
                    o.navi_draws.push_back(d);
                    if(d.pc==0x429530u){const auto t=sprani_translation(bf(d.args[1]),bf(d.args[2]));
                        sprani_draw_428a10(pool,o.sprani,st,d.args[0],&t,d.args[3],std::int32_t(d.args[4]),1.0f,1.0f,0.0f,o.navi_scene_draws,o.navi_blend);}
                    else sprani_draw_428a10(pool,o.sprani,st,d.args[0],&d.matrix,d.args[1],std::int32_t(d.args[3]),1.0f,bf(d.args[4]),0.0f,o.navi_scene_draws,o.navi_blend);
                }
                tr.object_draws.clear();
            }
            HeartUser hu{&c,&o,&st,&s,&r};s.heart_user=&hu;s.heart=heart_run;
            const bool ok=navi_pub_display_4beb00(s);
            if(!ok){note(o,s.missing?s.missing:0x4beb00u);if(s.fault)++o.unmapped[s.fault];st.depth=depth;st.current_offset=offset;}
            // The draws issued before a missing callee are executed; the rest of the display is what it stops.
            run_navi_draws(c,o,device,st);
            o.sprani.flag_986b28=native_sprite2d(c).state.mask_986b28;
            break;}
        }
    }catch(const std::exception& e){o.last_error=hex(callback)+": "+what(e);note(o,callback);st.depth=depth;st.current_offset=offset;}
    collect_2d(c,o);
    drain_sounds(c,o);
    return true;
}
bool native_race_hud_quest_object_45c470(NativeRuntimeContext& c,std::uint32_t stage,std::uint32_t& path,std::string& why){
    auto& o=native_race_hud(c);begin(c,o);
    try{
        if(!native_race_area_bind_records(c)){why="45C470: course record table (7D33BC) not identified";return false;}
        rebuild(c,o);
        NaviPubServices s;s.m=&o.memory;
        if(navi_quest_object_45c470(s,stage,path))return true;
        note(o,s.missing?s.missing:0x45c470u);if(s.fault)++o.unmapped[s.fault];
        why="45C470 stage "+hex(stage)+": missing "+hex(s.missing)+" unmapped "+hex(s.fault);
    }catch(const std::exception& e){o.last_error="45c470: "+what(e);note(o,0x45c470u);why=o.last_error;}
    return false;
}
bool native_race_hud_service(NativeRuntimeContext& c,std::uint32_t pc,const std::uint32_t* args,std::size_t count,
                             std::uint32_t& eax,std::string& why){
    auto& o=native_race_hud(c);begin(c,o);
    try{
        if(!native_race_area_bind_records(c)){why=hex(pc)+": course record table (7D33BC) not identified";return false;}
        rebuild(c,o);
        NaviPubServices s;s.m=&o.memory;
        if(navi_service(s,pc,args,count,eax))return true;
        note(o,s.missing?s.missing:pc);if(s.fault)++o.unmapped[s.fault];
        why=hex(pc)+": missing "+hex(s.missing)+" unmapped "+hex(s.fault);
    }catch(const std::exception& e){o.last_error=hex(pc)+": "+what(e);note(o,pc);why=o.last_error;}
    return false;
}
std::uint32_t native_race_hud_voice_45bd30(NativeRuntimeContext& c,std::int32_t id,std::int16_t duration,std::int16_t priority){
    auto& o=native_race_hud(c);
    NaviVoiceServices v;v.time_842110=c.race.clock.time_842110;v.busy_7f94c0=c.start_mode.manager_state_7f94c0;
    v.busy_7f95c4=0;v.sounds_424940=&o.pending_sounds;   // 7F95C4 is only read when 7F94C0 != 0 (46C530)
    if(v.busy_7f94c0)note(o,0x46c530u);
    const auto r=navi_voice_request_45bd30(o.voice,v,id,duration,priority);
    drain_sounds(c,o);
    return r;
}
bool native_race_hud_mission_control(NativeRuntimeContext& c,MissionManagerServices& ms){
    auto& o=native_race_hud(c);begin(c,o);
    auto& m=c.mission;auto& st=c.event_function36;
    FrontendUiResources ui{st.frontend_sprites};
    ui.motion_step=st.frontend_ui_motion_step;ui.pause_domain=st.title_pause_flag_95b214;
    ui.effect_user=st.frontend_effect_user;ui.effect_4249f0=st.frontend_effect;
    if(!o.mission_ui_constructed){mission_manager_construct_ui_465160(m.manager,ui);o.mission_ui_constructed=true;}
    MissionRaceServices r;
    r.ui=&ui;
    auto& cs=st.car_select;
    r.car=Bytes(cs.car_799d18.data(),cs.car_799d18.size());
    std::memcpy(&r.car_spec_134c,cs.parameters.data()+0x134c,4);
    r.camera=Bytes(cs.camera_79fe10.data(),cs.camera_79fe10.size());
    r.racers=&m.racers;r.profile=&st.frontend_profiles.active;r.voice=&o.voice;
    r.voice_services.time_842110=c.race.clock.time_842110;
    r.voice_services.busy_7f94c0=c.start_mode.manager_state_7f94c0;
    r.voice_services.busy_7f95c4=0;                          // 7F95C4 is only read when 7F94C0 != 0 (46C530)
    r.mode_78026c=c.mode_state.current;
    r.race_end_7d38f0=c.race.manager.state.over_state_7d38f0;
    r.pause_8367bc=c.game_mode.start_countdown;
    r.sounds_424940=&o.pending_sounds;
    const auto choice=std::int8_t(c.start_mode.course_choice_655b59&0xffu);
    const bool row=choice>=0&&std::uint32_t(choice)<ModelOffsetRows;
    if(row)for(unsigned k=0;k<3;++k)r.model_offset_5e0ac8[k]=bf(ModelOffset5e0ac8[std::uint32_t(choice)][k]);
    r.user=&c;
    r.release_car_4763d0=[](void* u,std::uint32_t id){   // 4763D0 on the traffic module (its racer table 80FB00)
        std::array<std::uint8_t,0x40> mb{};driving::PcMatrixStack st{driving::Bytes(mb.data(),mb.size())};
        return native_race_score(*static_cast<NativeRuntimeContext*>(u),st,0x4763d0u,id);};
    if(r.voice_services.busy_7f94c0){note(o,0x46c530u);}
    // The race part's 495D90 projection runs on the renderer's matrix stack;
    // without it (no PC renderer) the mission manager reports it.
    r.matrices=nullptr;                                      // not bound yet: 495D90 reports it (see report)
    bool ok=false;
    try{ok=mission_manager_control_496a30(m.manager,ms,r);}
    catch(const std::exception& e){o.last_error="496A30: "+what(e);}
    if(!ok){
        const auto pc=r.missing?r.missing:(ms.missing?ms.missing:0x496a30u);
        note(o,pc);if(!m.missing)m.missing=pc;
    }
    c.race.manager.state.over_state_7d38f0=r.race_end_7d38f0;
    if(ui.missing_pc)note(o,ui.missing_pc);
    if(!row)note(o,0x495db9u);
    ++o.mission_race_parts;
    drain_sounds(c,o);
    return ok;
}
std::string native_race_hud_status(const NativeRuntimeContext& c){
    if(!c.race_hud)return "hud: no owner";
    const auto& o=*c.race_hud;
    std::ostringstream s;
    s<<"hud variant="<<c.game_mode.game_variant<<" preset="<<c.start_mode.course_preset<<" sprani "<<o.sprani_inits<<"/"<<o.sprani_controls<<"/"<<o.sprani_displays<<" draws="<<o.sprani_draws.size()
     <<" gad "<<o.gad_inits<<"/"<<o.gad_controls<<"/"<<o.gad_displays<<" navi "<<o.navi_inits<<"/"<<o.navi_controls<<"/"<<o.navi_displays
     <<(o.navi_init_failed?"(init failed)":"")<<" navi_draws="<<o.navi_draws.size()<<" scene="<<o.navi_scene_draws.size()
     <<" mission="<<o.mission_race_parts;
    if(o.frame_missing_frame==o.frame&&!o.frame_missing.empty()){s<<" missing@"<<o.frame<<":";for(const auto& [pc,n]:o.frame_missing)s<<" "<<std::hex<<pc<<std::dec<<"x"<<n;}
    if(!o.last_error.empty())s<<" last="<<o.last_error;
    return s.str();
}
std::string native_race_hud_report(const NativeRuntimeContext& c){
    if(!c.race_hud)return "hud: no owner\n";
    const auto& o=*c.race_hud;
    std::ostringstream s;
    s<<native_race_hud_status(c)<<"\n  missing (first frame):";
    for(const auto& [pc,f]:o.missing_order)s<<" "<<std::hex<<pc<<std::dec<<"@"<<f<<"x"<<o.missing.at(pc);
    s<<"\n  unmapped NAVI reads:";for(const auto& [a,n]:o.unmapped)s<<" "<<std::hex<<a<<std::dec<<"x"<<n;
    s<<"\n  HUD draws executed:";for(const auto& [pc,n]:o.draws_by_pc)s<<" "<<std::hex<<pc<<std::dec<<"x"<<n;
    s<<"\n  SPRANI scenes rendered:";for(const auto& [t,n]:o.scene_tokens)s<<" "<<std::hex<<t<<std::dec<<"x"<<n;
    s<<"\n  sounds 424940 (requests, enqueued "<<o.sounds_enqueued<<", dropped by the 9563E8 gate "<<o.sounds_dropped<<"):";
    for(const auto& [id,n]:o.sounds_424940)s<<" "<<std::hex<<id<<std::dec<<"x"<<n;
    s<<"\n";
    return s.str();
}
}
