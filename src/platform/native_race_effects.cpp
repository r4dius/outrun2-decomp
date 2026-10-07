#include "driving/pc_wrecker.hpp"
#include "driving/pc_wall_rebound.hpp"
#include "system/dev_hooks.hpp"
#include "platform/bulk_fallback.hpp"
#include "platform/native_race_effects.hpp"
#include "platform/race_ghosts.hpp"
#include "platform/race_end_runtime.hpp"
#include "platform/native_runtime.hpp"
#include "platform/pc_scene_renderer.hpp"
#include "platform/race_area_runtime.hpp"
#include <array>
#include <cstring>
#include <string>
#include <cstdio>
#include <cstdlib>
#include <stdexcept>
namespace outrun::platform {
namespace {
constexpr std::uint32_t CarBase=0x60000000u,CameraBase=0x61000000u,Bank57Base=0x62000000u,BankD0Base=0x63000000u;
void put32(std::uint8_t* p,std::uint32_t v){std::memcpy(p,&v,4);}
// PC memory the ported events may touch, over the runtime-owned bytes.
void map_memory(NativeRuntimeContext& c,PcRaceMemory& m,bool particles){
    auto& s=c.race_effects;
    m.clear();
    // The AREA owner's PC memory (area block 7D2D80.., course records, the
    // race-manager block, EXE tables) as its last callback built it; the
    // mappings below shadow it where this module owns the words.
    for(const auto& r:native_race_area_memory(c,nullptr,false).regions()){
        if(r.writable)m.map(r.base,r.data,r.size);else m.map_const(r.base,r.data,r.size);}
    // The arcade ending's live .data 638DF0.. (4160F0 reads the column [638E9C] in mode 24).
    if(c.race_end)m.map(0x638df0u,c.race_end->state.ending_638df0.data(),c.race_end->state.ending_638df0.size());
    s.scene.work_pointer_79f5ec=c.event_state.slots[387].work_token;
    s.scene.map(m);
    for(std::uint32_t id=0;id<s.event_flags_79fb48.size();++id)s.event_flags_79fb48[id]=c.event_state.slots[id].flags;
    m.map(0x79fb48u,s.event_flags_79fb48.data(),s.event_flags_79fb48.size());
    std::memset(s.mode_78024c.data(),0,s.mode_78024c.size());
    put32(s.mode_78024c.data()+0xc,c.game_mode.game_variant);      // 780258
    put32(s.mode_78024c.data()+0x20,c.mode_state.current);         // 78026C
    m.map(0x78024cu,s.mode_78024c.data(),s.mode_78024c.size());
    auto& car=c.event_function36.car_select;
    std::memset(s.cars_799d18.data(),0,s.cars_799d18.size());
    put32(s.cars_799d18.data(),CarBase);
    m.map(0x799d18u,s.cars_799d18.data(),4);
    m.map(CarBase,car.car_799d18.data(),car.car_799d18.size());
    put32(s.camera_79f574.data(),CameraBase);
    m.map(0x79f574u,s.camera_79f574.data(),4);
    m.map(CameraBase,car.camera_79fe10.data(),car.camera_79fe10.size());
    if(s.lights_899b98)m.map(0x899b98u,s.lights_899b98,0x960);
    if(!particles)return;
    s.particles.map(m);particles_map_tables(m);
    ghost_map_car_tables(m);   // 41C190 (make_occar_param): the model table 650500 (4866C0) and its car data records
    if(s.glow_8a8c18)m.map(PcParticleState::GlowBase,s.glow_8a8c18,PcParticleState::GlowEnd-PcParticleState::GlowBase);
    put32(s.frame_95af0c.data(),c.frame_state.frame_counter_95af0c);
    m.map(0x95af0cu,s.frame_95af0c.data(),4);
    put32(s.network_7f94c0.data(),0);                               // offline: no LAN session object (7F9460+60)
    m.map(0x7f94c0u,s.network_7f94c0.data(),4);
    const auto& o=c.race.camera_override;auto* b=s.override_82e7c0.data();
    std::memset(b,0,s.override_82e7c0.size());
    std::memcpy(b+0x00,&o.w82e7c0,4);std::memcpy(b+0x04,&o.w82e7c4,2);std::memcpy(b+0x08,&o.w82e7c8,4);std::memcpy(b+0x0c,&o.w82e7cc,2);
    std::memcpy(b+0x10,&o.w82e7d0,4);b[0x14]=o.scene_82e7d4;std::memcpy(b+0x18,&o.override_82e7d8,4);std::memcpy(b+0x1c,&o.w82e7dc,2);
    std::memcpy(b+0x20,&o.time_82e7e0,4);std::memcpy(b+0x24,&o.w82e7e4,4);std::memcpy(b+0x28,&o.f82e7e8,4);
    m.map(0x82e7c0u,b,s.override_82e7c0.size());
    // 780140[type] / 780228[type]: the course world's run headers and per-polygon
    // lengths (43D470, inlined by 41BD50 and the smoke / gravel requests), at
    // synthetic bases; 0 for a lane not loaded.
    {auto& world=c.start_mode.scene_owner_course_world;
     std::memset(s.runs_780140.data(),0,16);std::memset(s.runs_780228.data(),0,16);
     bool any=false;for(std::uint32_t k=0;k<4;++k)if(world.lane_loaded(k))any=true;
     if(any){const auto tables=world.tables();
        for(std::uint32_t k=0;k<4;++k){
            const auto& runs=tables.courses[k].runs;
            if(!runs.present||!runs.header.size()||!runs.lengths.size())continue;
            const std::uint32_t h=0x5d300000u+k*0x10000u,l=0x5d400000u+k*0x100000u;
            m.map_const(h,runs.header.data(),runs.header.size());m.map_const(l,runs.lengths.data(),runs.lengths.size());
            put32(s.runs_780140.data()+k*4u,h);put32(s.runs_780228.data()+k*4u,l);
        }}
     m.map(0x780140u,s.runs_780140.data(),16);m.map(0x780228u,s.runs_780228.data(),16);}
    // The player car's body work 82E7F0 (its tyre pointers 82EA38.. and the
    // skid mask 82EE60 feed 41BD50), owned by the car-select/race car state.
    m.map(0x82e7f0u,car.body_82e7f0.data(),car.body_82e7f0.size());
    // Resource 0x57 (particle textures): the PC entry points at the system
    // section and at the texture table header, whose first word is the
    // absolute address of the handle slots (an offset in the native loader).
    PcPmtResources* r=(s.bank&&!s.bank57_failed)?s.bank(0x57):nullptr;
    if(!r)s.bank57_failed=true;
    if(r&&r->texture_table_24){
        std::memset(s.resource_57.data(),0,s.resource_57.size());
        put32(s.resource_57.data(),Bank57Base);
        put32(s.resource_57.data()+0x24,0x62fff000u);
        put32(s.texture_table_57.data(),Bank57Base+r->view().u32(r->texture_table_24));
        m.map(0x7c2800u+0x57u*0x48u,s.resource_57.data(),s.resource_57.size());
        m.map(0x62fff000u,s.texture_table_57.data(),4);
        m.map(Bank57Base,r->system.data(),r->system.size());
    }
    // Mode 24: the ending props bank 0xD0 (420560's six textures), mapped like 0x57 when resident.
    if(c.mode_state.current==24u&&s.bank)
        if(PcPmtResources* d=s.bank(0xd0);d&&d->texture_table_24){
            std::memset(s.resource_d0.data(),0,s.resource_d0.size());
            put32(s.resource_d0.data(),BankD0Base);
            put32(s.resource_d0.data()+0x24,0x63fff000u);
            put32(s.texture_table_d0.data(),BankD0Base+d->view().u32(d->texture_table_24));
            m.map(0x7c2800u+0xd0u*0x48u,s.resource_d0.data(),s.resource_d0.size());
            m.map(0x63fff000u,s.texture_table_d0.data(),4);
            m.map(BankD0Base,d->system.data(),d->system.size());
        }
}
std::uint32_t fault_pc(const std::exception& e){
    // Called inside a catch handler. The Switch build has no RTTI: the active
    // exception is rethrown and matched instead of dynamic_cast.
    (void)e;
    try{throw;}catch(const PcRaceUnmapped& u){return u.address;}catch(...){return 0;}
}
// The SPRANI pool services of the particles (41D810 spark flash 0x2C0001):
// the runtime's native pool, as the traffic and race end bindings.
bool sprite_pool(NativeRuntimeContext& c,PcRaceMemory& m,const PcRaceCall& k,std::uint32_t& eax){
    auto& pool=c.event_function36.frontend_sprites;const auto* a=k.args.data();eax=0u;
    switch(k.pc){
    case 0x428320u:{
        FrontendSpriteTiming t{};std::string err;
        if(!pool.bank_scene(a[0],t)&&!native_sprani_bind_bank(c,a[0]>>16,err))throw std::runtime_error("SPRANI bank: "+err);
        eax=pool.create(a[0],a[1],a[2],-1,-1,false,c.event_function36.title_pause_flag_95b214);return true;}
    case 0x4285a0u:if(a[0]<FrontendSprites::Count)pool.release(a[0]);return true;
    case 0x428880u:eax=std::int32_t(a[0])<0?0u:pool.status(a[0]);return true;
    case 0x4287b0u:{
        if(a[0]>=FrontendSprites::Count)return true;
        std::array<float,16> mat{};for(std::uint32_t w=0;w<16;++w)mat[w]=m.f32(a[1]+w*4);
        pool.set_matrix(a[0],mat);return true;}
    default:return false;
    }
}
// Callees the runtime does not answer: counted, then the event is latched.
PcRaceService unported_service(NativeRaceEffectsState& s){
    return [&s](const PcRaceCall& k)->std::uint32_t{
        ++s.unported[k.pc];
        char t[64];std::snprintf(t,sizeof t,"PC callee %08x is not ported natively",k.pc);
        throw std::runtime_error(t);
    };
}
// No native answer: the bulk translation over the effects memory (counted and traced
// once per address by bulk_call), else counted and the event latched.
std::uint32_t fallback(NativeRuntimeContext& c,NativeRaceEffectsState& s,PcRaceMemory& m,PcRaceContext& ctx,const PcRaceCall& k){
    if(k.pc==0x43e3b0u){   // GetCsRoadInfoByCsLen(out 0x64, place, polygon hint) (as the traffic binding)
        const auto tables=c.start_mode.scene_owner_course_world.tables_or_empty();
        driving::PcRoadInfoContext road{tables,ctx.matrices,{c.game_mode.course_runtime.zero_7d3124[1],c.game_mode.course_runtime.zero_7d3178[1]}};
        return driving::pc_get_cs_road_info_by_cs_len(m.bytes(k.args[0],0x64u),m.bytes(k.args[1],0x10u),std::int32_t(k.args[2]),road)?1u:0u;
    }
    if(bulk_translated(k.pc))return bulk_call(m,ctx.service,k,&ctx.matrices,&c.event_function36.pc_crt_random_state);
    return unported_service(s)(k);
}
// IDirect3DTexture9::GetLevelDesc(texture, 0, desc) for a texture of the
// resource 0x57 bank: the level-0 size of a texture created by
// D3DXCreateTextureFromFileInMemoryEx with D3DX_DEFAULT (0xFFFFFFFF, the
// 42E8C0 arguments) is the DDS size rounded up to powers of two.
bool texture_level_desc(NativeRaceEffectsState& s,PcRaceMemory& m,const PcRaceCall& k){
    if(k.pc!=PcParticleGetLevelDesc||k.args[1]!=0u||!s.bank)return false;
    PcPmtResources* r=s.bank(0x57);if(!r||!r->texture_table_24||!r->video)return false;
    const auto v=r->view();
    const std::uint32_t slots=v.u32(r->texture_table_24),count=v.u32(8);
    for(std::uint32_t t=0;t<count;++t){
        if(v.u32(slots+t*4u)!=k.args[0]||!k.args[0])continue;
        const std::uint32_t source=v.u32(r->texture_records_04+t*0x14u+4u);
        if(std::uint64_t(source)+0x14u>r->video_size||std::memcmp(r->video+source,"DDS ",4))return false;
        std::uint32_t h,w;std::memcpy(&h,r->video+source+0xc,4);std::memcpy(&w,r->video+source+0x10,4);
        auto pow2=[](std::uint32_t x){std::uint32_t p=1;while(p<x&&p<0x80000000u)p<<=1;return p;};
        m.put32(k.args[2]+0x18,pow2(w));m.put32(k.args[2]+0x1c,pow2(h));
        return true;
    }
    return false;
}
// PcD3D9Device adapter (SetFVF / SetTransform / DrawPrimitiveUP included).
struct DeviceAdapter:PcParticleDevice{
    PcD3D9Device& d;std::uint32_t& dropped;
    DeviceAdapter(PcD3D9Device& dd,std::uint32_t& n):d(dd),dropped(n){}
    std::uint32_t get_render_state(std::uint32_t s)override{return d.get_render_state(s);}
    void set_render_state(std::uint32_t s,std::uint32_t v)override{d.set_render_state(s,v);}
    std::uint32_t get_texture(std::uint32_t s)override{return d.get_texture(s);}
    void set_texture(std::uint32_t s,std::uint32_t t)override{d.set_texture(s,t);}
    std::uint32_t get_texture_stage_state(std::uint32_t s,std::uint32_t t)override{return d.get_texture_stage_state(s,t);}
    void set_texture_stage_state(std::uint32_t s,std::uint32_t t,std::uint32_t v)override{d.set_texture_stage_state(s,t,v);}
    std::uint32_t get_sampler_state(std::uint32_t s,std::uint32_t t)override{return d.get_sampler_state(s,t);}
    void set_sampler_state(std::uint32_t s,std::uint32_t t,std::uint32_t v)override{d.set_sampler_state(s,t,v);}
    void set_transform(std::uint32_t s,const float* m)override{d.set_transform(s,m);}
    void draw_primitive_up(std::uint32_t t,std::uint32_t n,const std::uint8_t* v,std::uint32_t stride)override{d.draw_primitive_up(t,n,v,stride);}
    void set_fvf(std::uint32_t f)override{d.set_fvf(f);}
    void set_vertex_shader(std::uint32_t v)override{d.set_vertex_shader(v);}
    void set_pixel_shader(std::uint32_t v)override{d.set_pixel_shader(v);}
    void release(std::uint32_t o)override{d.release(o);}
};
}
bool native_race_effects_invoke(NativeRuntimeContext& c,std::uint32_t callback,driving::PcMatrixStack& matrices){
    auto& s=c.race_effects;
    const bool scene=callback==0x4afbb0u||callback==0x4afdc0u||callback==0x4afc70u;
    const bool particles=callback==0x41bbf0u||callback==0x41bc60u;
    if(!scene&&!particles)return false;
    if(scene&&s.scene_fault_pc)return true;
    if(particles&&!s.particles_fault.empty())return true;
    PcRaceMemory& m=s.memory;
    try{
        map_memory(c,m,particles);
        PcRaceContext ctx{m,matrices,[&](const PcRaceCall& k)->std::uint32_t{
            if(k.pc==0x424940u){   // race SE queue 9563E8 gated by event 383 (41F5F0 backfire sound 0x51)
                auto& w=c.race.car_world;
                driving::PcSoundQueue q{driving::Bytes(w.sound_entries_9563e8.data(),w.sound_entries_9563e8.size()),
                    driving::Bytes(w.sound_state.data(),w.sound_state.size()),std::uint8_t(c.event_state.slots[383].flags)};
                driving::pc_enqueue_sound(q,k.args[0]);return 0;}
            std::uint32_t eax;if(sprite_pool(c,m,k,eax))return eax;
            if(texture_level_desc(s,m,k))return 0;return fallback(c,s,m,ctx,k);},nullptr};
        switch(callback){
        case 0x4afbb0u:scene_effects_init_4afbb0(ctx,PcSceneEffectsState::WorkBase);++s.scene_inits;break;
        case 0x4afdc0u:scene_effects_control_4afdc0(ctx,PcSceneEffectsState::WorkBase);++s.scene_controls;
            break;
        case 0x4afc70u:scene_effects_destroy_4afc70(m,PcSceneEffectsState::WorkBase);++s.scene_destroys;break;
        case 0x41bbf0u:{PcParticleContext p{ctx,c.event_function36.pc_crt_random_state};particles_init_41bbf0(p);++s.particle_inits;break;}
        default:{PcParticleContext p{ctx,c.event_function36.pc_crt_random_state};particles_control_41bc60(p);++s.particle_controls;break;}
        }
    }catch(const std::exception& e){
        const std::uint32_t pc=fault_pc(e);
        if(scene){s.scene_fault_pc=pc?pc:callback;s.scene_fault=e.what();}
        else{s.particles_fault_pc=pc?pc:callback;s.particles_fault=e.what();}
        dev_log("race effects fault cb=%x pc=%x: %s",callback,pc,e.what());
    }
    return true;
}
bool native_race_effects_smoke_4208a0(NativeRuntimeContext& c,driving::PcMatrixStack& matrices,std::uint32_t count,
                                      std::uint32_t x,std::uint32_t y,std::uint32_t z,std::uint32_t colour){
    auto& s=c.race_effects;
    if(!s.particles_fault.empty())return false;
    PcRaceMemory& m=s.memory;
    try{
        map_memory(c,m,true);
        PcRaceContext ctx{m,matrices,[&](const PcRaceCall& k)->std::uint32_t{return fallback(c,s,m,ctx,k);},nullptr};
        PcParticleContext p{ctx,c.event_function36.pc_crt_random_state};
        particles_smoke_emit_4208a0(p,count,x,y,z,colour);++s.smoke_emits;
    }catch(const std::exception& e){
        const std::uint32_t pc=fault_pc(e);
        s.particles_fault_pc=pc?pc:0x4208a0u;s.particles_fault=std::string("4208A0: ")+e.what();
        return false;
    }
    return true;
}
bool native_race_effects_tire_reset(NativeRuntimeContext& c,driving::PcMatrixStack& matrices){
    auto& s=c.race_effects;
    if(!s.particles_fault.empty())return false;
    PcRaceMemory& m=s.memory;
    try{
        map_memory(c,m,true);
        PcRaceContext ctx{m,matrices,[&](const PcRaceCall& k)->std::uint32_t{return fallback(c,s,m,ctx,k);},nullptr};
        PcParticleContext p{ctx,c.event_function36.pc_crt_random_state};
        particles_tire_reset_41fb10_41c420(p);
    }catch(const std::exception& e){
        const std::uint32_t pc=fault_pc(e);
        s.particles_fault_pc=pc?pc:0x41fb10u;s.particles_fault=std::string("41FB10/41C420: ")+e.what();
        return false;
    }
    return true;
}
bool native_race_effects_flare_420560(NativeRuntimeContext& c,driving::PcMatrixStack& matrices){
    auto& s=c.race_effects;
    if(!s.particles_fault.empty())return false;
    PcRaceMemory& m=s.memory;
    try{
        map_memory(c,m,true);
        PcRaceContext ctx{m,matrices,[&](const PcRaceCall& k)->std::uint32_t{return fallback(c,s,m,ctx,k);},nullptr};
        PcParticleContext p{ctx,c.event_function36.pc_crt_random_state};
        particles_flare_420560(p);++s.flare_emits;
    }catch(const std::exception& e){
        const std::uint32_t pc=fault_pc(e);
        s.particles_fault_pc=pc?pc:0x420560u;s.particles_fault=std::string("420560: ")+e.what();
        return false;
    }
    return true;
}
bool native_race_effects_display(NativeRuntimeContext& c,std::uint32_t callback,PcSceneRenderer& r){
    if(callback!=0x41bd10u)return false;
    auto& s=c.race_effects;
    if(!s.particles_fault.empty())return true;
    PcRaceMemory& m=s.memory;
    try{
        map_memory(c,m,true);
        auto& g=r.globals();
        for(std::uint32_t k=0;k<0x240u;k+=4)put32(s.render_95d860.data()+k,g.w(0x95d860u+k));
        m.map(0x95d860u,s.render_95d860.data(),s.render_95d860.size());
        put32(s.screen_740c8c.data(),0x44200000u);put32(s.screen_740c8c.data()+4,0x43f00000u);
        m.map_const(0x740c8cu,s.screen_740c8c.data(),8);
        m.put32(0x95af9cu,r.pixel_shader_95af9c());                // bootstrap 41784F: CreatePixelShader(623830)
        PcRaceService service=[&s,&r,&g](const PcRaceCall& k)->std::uint32_t{
            if(k.pc==PcParticleRenderGlobal){g.w(k.args[0])=k.args[1];return 0;}
            if(k.pc==0x409df0u){                                  // 409DF0(slot) -> 410F90(current, slot)
                std::array<float,16> m{};const auto top=r.matrices().current();for(unsigned q=0;q<16;++q)m[q]=top.f32(q*4);
                render_set_matrix_410f90(r.flush_context(),m,k.args[0]);return 0;}
            if(k.pc==0x408880u){render_reset_states_408880(r.flush_context(),r.frame().layer_7d25f0);return 0;}
            if(k.pc==0x411060u){                                  // current matrix into 95D9E0, then 411060
                std::array<float,16> m{};const auto top=r.matrices().current();for(unsigned q=0;q<16;++q)m[q]=top.f32(q*4);
                render_set_matrix_410f90(r.flush_context(),m,6);return 0;}
            return unported_service(s)(k);
        };
        PcRaceContext ctx{m,r.matrices(),service,nullptr};
        DeviceAdapter device(r.device(),s.display_dropped);
        PcParticleContext p{ctx,c.event_function36.pc_crt_random_state,&device,&r.device()};
        particles_display_41bd10(p);++s.particle_displays;
    }catch(const std::exception& e){
        const std::uint32_t pc=fault_pc(e);
        s.particles_fault_pc=pc?pc:callback;s.particles_fault=std::string("display: ")+e.what();
    }
    return true;
}
bool native_race_effects_setter(NativeRuntimeContext& c,const PcRaceCall& k,std::uint32_t& eax){
    PcRaceMemory& m=c.race_effects.memory;map_memory(c,m,false);
    return scene_effects_setter(m,k,eax);
}
}
