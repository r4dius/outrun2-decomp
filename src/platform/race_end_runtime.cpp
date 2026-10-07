#include <array>
#include <cmath>
#include <cstdlib>
#include <vector>
#include "platform/bulk_fallback.hpp"
#include "system/dev_hooks.hpp"
#include "platform/race_end_runtime.hpp"
#include "platform/race_modes.hpp"
#include "platform/race_translated.hpp"
#include "platform/race_traffic_runtime.hpp"
#include "platform/translated_crt.hpp"   // the request manager's heap
#include "platform/sp_rankings.hpp"
#include "platform/race_manager_runtime.hpp"
#include "platform/race_area_runtime.hpp"
#include "platform/race_ghosts_runtime.hpp"
#include "platform/race_hud_runtime.hpp"
#include "platform/race_goal_camera.hpp"
#include "platform/sprite_2d_runtime.hpp"
#include "platform/race_manager.hpp"
#include "driving/pc_common_control.hpp"
#include "driving/pc_wall_rebound.hpp"
#include "platform/retail_asset_store.hpp"
#include "platform/arcade_attract.hpp"
#include "platform/race_variant_owners.hpp"
#include "platform/race_name_entry.hpp"
#include "platform/race_ending.hpp"
#include "platform/race_ending_services.hpp"
#include "platform/object_db.hpp"
#include "platform/vehicle_body_init.hpp"
#include "platform/pc_scene_renderer.hpp"
#include "platform/native_race_effects.hpp"
#include "platform/race_autoscene.hpp"
#include "platform/race_robots_runtime.hpp"
#include "platform/frontend_ui_resources.hpp"
#include "platform/vehicle_creation_queue.hpp"
#include "platform/pc_address_view.hpp"
#include "platform/pc_network.hpp"
#include <cstdio>
#include <cctype>
#include <cstring>
#include <vector>
#include <algorithm>
#include <sstream>
#include <stdexcept>
namespace outrun::platform {
extern const TranslatedFunction network_functions[];
extern const TranslatedCodeData network_code_data[];
}
namespace outrun::platform {
namespace {
// LAN race end (variant 4): the bulk code reaches the network set (message buffers 491900,
// 4F5xxx sends) on the same CPU and stack.
const TranslatedModule* lan_network_module(const NativeRuntimeContext& c){
    if(c.game_mode.game_variant!=4u)return nullptr;
    // Only the message buffer methods (4918xx..4919xx) and the session sends 4F5xxx: the
    // other functions of the set (events 440xxx, race globals...) stay the service's.
    static const std::vector<TranslatedFunction> functions=[]{
        std::vector<TranslatedFunction> v;
        for(const auto* f=network_functions;f->pc;++f)
            if((f->pc>=0x491800u&&f->pc<0x491c00u)||(f->pc>=0x4f5000u&&f->pc<0x4f6000u))v.push_back(*f);
        v.push_back(TranslatedFunction{0,nullptr});return v;}();
    static const TranslatedModule module=[]{TranslatedModule m{functions.data(),nullptr,{}};m.code_data=network_code_data;return m;}();
    return &module;
}
std::string hex(std::uint32_t v){char t[16];std::snprintf(t,sizeof t,"%X",v);return t;}
struct EndUnported : std::runtime_error {
    std::uint32_t pc;
    EndUnported(std::uint32_t p,const std::string& why):std::runtime_error("PC "+hex(p)+": "+why),pc(p){}
};
// The text glyphs a service added since `first`, as a RaceHudGlyphRange draw: the
// HUD owner queues them between the sprites in call order (the PC 2D queue keeps
// the submission order inside a layer: 497560's names over the 2C00FB rows).
void mark_glyphs(NativeRaceEnd& e,std::size_t first){
    if(!e.hud_draws||e.hud_glyphs->size()==first)return;
    RaceHudDraw d;d.pc=RaceHudGlyphRange;d.args[0]=std::uint32_t(first);d.args[1]=std::uint32_t(e.hud_glyphs->size());
    e.hud_draws->push_back(d);
}
// 755860[bank-0x20]: the SPRANI animations the pool banks use here.
const char* ani_path(std::uint32_t bank){return native_sprite2d_ani_path(bank);}
std::uint32_t rd32(const std::uint8_t* p){std::uint32_t v;std::memcpy(&v,p,4);return v;}
float rdf(const std::uint8_t* p){float v;std::memcpy(&v,p,4);return v;}
// The pool's per-scene timing of a bank (48BD20 root component: +0 size,
// +8 frames, +C rate), from the retail SPRANI file (the resource loader's
// 42DEB0/429920 result).
bool bind_bank(NativeRuntimeContext& c,std::uint32_t bank,std::string& error){
    const char* path=ani_path(bank);
    if(!path){error="no SPRANI path for bank "+hex(bank);return false;}
    auto* store=c.event_function36.retail_assets;
    if(!store){error="no retail store";return false;}
    const auto* bytes=retail_asset_guest_path(*store,path,true,&error);
    if(!bytes)return false;
    const auto& d=*bytes;
    if(d.size()<8||rd32(d.data())+4u!=d.size()){error=std::string(path)+": ani wrapper";return false;}
    const std::uint8_t* a=d.data()+4;const std::size_t n=d.size()-4;
    std::vector<FrontendSpriteTiming> timing;
    for(std::uint32_t off=0;;off+=4){
        if(off+4>n){error=std::string(path)+": scene table";return false;}
        const std::uint32_t sp=rd32(a+off);if(!sp)break;
        if(std::size_t(sp)+8>n){error=std::string(path)+": scene";return false;}
        const std::uint32_t cc=rd32(a+sp),cp=rd32(a+sp+4);
        if(!cc||std::size_t(cp)+std::size_t(cc)*36u>n){error=std::string(path)+": components";return false;}
        const std::uint32_t root=cp+(cc-1u)*36u;const std::uint32_t size=rd32(a+root);
        timing.push_back({rdf(a+root+8),rdf(a+root+12),float(std::int16_t(size&0xffffu)),float(std::int16_t(size>>16)),rdf(a+root+4)});
    }
    if(!c.event_function36.frontend_sprites.bind_timing(bank,timing)){error=std::string(path)+": timing rejected";return false;}
    return true;
}
// 82E7C0..82E7EC (RaceCameraOverride): copied in and back around a call.
void override_in(const RaceCameraOverride& o,std::uint8_t* b){
    std::memset(b,0,0x30);
    std::memcpy(b+0x00,&o.w82e7c0,4);std::memcpy(b+0x04,&o.w82e7c4,2);std::memcpy(b+0x08,&o.w82e7c8,4);std::memcpy(b+0x0c,&o.w82e7cc,2);
    std::memcpy(b+0x10,&o.w82e7d0,4);b[0x14]=o.scene_82e7d4;std::memcpy(b+0x18,&o.override_82e7d8,4);std::memcpy(b+0x1c,&o.w82e7dc,2);
    std::memcpy(b+0x20,&o.time_82e7e0,4);std::memcpy(b+0x24,&o.w82e7e4,4);std::memcpy(b+0x28,&o.f82e7e8,4);std::memcpy(b+0x2c,&o.steering_82e7ec,4);
}
void override_out(RaceCameraOverride& o,const std::uint8_t* b){
    std::memcpy(&o.w82e7c0,b+0x00,4);std::memcpy(&o.w82e7c4,b+0x04,2);std::memcpy(&o.w82e7c8,b+0x08,4);std::memcpy(&o.w82e7cc,b+0x0c,2);
    std::memcpy(&o.w82e7d0,b+0x10,4);o.scene_82e7d4=b[0x14];std::memcpy(&o.override_82e7d8,b+0x18,4);std::memcpy(&o.w82e7dc,b+0x1c,2);
    std::memcpy(&o.time_82e7e0,b+0x20,4);std::memcpy(&o.w82e7e4,b+0x24,4);std::memcpy(&o.f82e7e8,b+0x28,4);std::memcpy(&o.steering_82e7ec,b+0x2c,4);
}
// 42CCE0's _vsnprintf (580265, 0x100 bytes) for the formats its callers pass: text, one
// %s (a mapped string) or one %d with an optional 0 flag and width.
std::string format_42cce0(const PcRaceMemory& m,std::uint32_t format,std::uint32_t arg){
    std::string f;for(std::uint32_t p=format;f.size()<0xffu;++p){const auto ch=m.u8(p);if(!ch)break;f+=char(ch);}
    std::string out;bool used=false;
    for(std::size_t i=0;i<f.size();++i){
        if(f[i]!='%'){out+=f[i];continue;}
        if(i+1<f.size()&&f[i+1]=='%'){out+='%';++i;continue;}
        std::size_t j=i+1;bool zero=false;int width=0;
        if(j<f.size()&&f[j]=='0'){zero=true;++j;}
        while(j<f.size()&&f[j]>='0'&&f[j]<='9'){width=width*10+(f[j]-'0');++j;}
        if(used||j>=f.size()||(f[j]!='d'&&f[j]!='s'))throw std::logic_error("42CCE0 format not modelled: "+f);
        used=true;
        std::string v;
        if(f[j]=='s'){for(std::uint32_t p=arg;v.size()<0xffu;++p){const auto ch=m.u8(p);if(!ch)break;v+=char(ch);}}
        else v=std::to_string(std::int32_t(arg));
        if(f[j]=='d'&&zero&&v.size()<std::size_t(width)){const bool neg=!v.empty()&&v[0]=='-';v.insert(neg?1:0,std::size_t(width)-v.size(),'0');}
        else if(v.size()<std::size_t(width))v.insert(0,std::size_t(width)-v.size(),' ');
        out+=v;i=j;
    }
    if(out.size()>0xffu)out.resize(0xffu);
    return out;
}
// 447750 / 4478F0 / 447A60 / 447C10 (thiscall 7B17F8, read from the Steam
// build): one record into a table of ten 0x20-byte entries (+4 name, +14 car,
// +15 route, +18 score, +1C time, +20 flags) at this + table + index*0x140,
// sorted by score, highest first. The PC first checks its launcher (the
// protection's named object); that handshake has no meaning here and is not
// reproduced. Returns 0 when the score does not reach the tenth entry.
std::uint32_t record_insert_447750(PcRaceMemory& m,std::uint32_t self,std::uint32_t table,std::uint32_t index,std::uint32_t score,
    std::uint32_t time,std::uint32_t a,std::uint32_t b,std::uint32_t course,std::uint32_t route){
    const std::uint32_t base=self+table+index*0x140u;
    auto entry=[&](std::int32_t k){return base+std::uint32_t(k)*0x20u;};
    if(m.u32(entry(9)+0x18u)>score)return 0u;
    std::int32_t k=9;
    while(k>=0&&!(m.u32(entry(k)+0x18u)>score))--k;
    if(k==0)m.put8(0x84bcfcu,1);                                  // 4EF430
    for(std::int32_t j=9;j>=k+2;--j)for(std::uint32_t o=4;o<0x24u;o+=4)m.put32(entry(j)+o,m.u32(entry(j-1)+o));
    const std::uint32_t r=entry(k+1);
    m.put32(r+0x1cu,time);m.put8(r+0x15u,std::uint8_t(route));m.put32(r+0x18u,score);
    m.put8(r+0x20u,std::uint8_t((((b&1u)|((course&0xffu)<<1))<<1)|(a&1u)));
    m.put8(r+0x14u,m.u8(0x7c23fcu));
    for(std::uint32_t i=0;;++i){const auto ch=m.u8(0x7c23e0u+i);m.put8(r+4u+i,ch);if(!ch)break;}   // strcpy (license name)
    return 1u;
}
// 4480B0 / 447DC0 (Time Attack, thiscall 7B17F8): ten entries sorted by time,
// lowest first. 4480B0: 0x58-byte entries at +9154 (+0 name, +10 car, +14
// time, +18 fifteen lap times, +54 flags), table (arcade + 2*flag) of 0x370;
// 447DC0: 0x30-byte entries at +2804 (+0 name, +10 car, +14 time, +18 five
// stage times, +2C flags), table course of 0x1E0. Same launcher note as 447750.
std::uint32_t record_insert_time(PcRaceMemory& m,std::uint32_t first,std::uint32_t size,std::uint32_t time,
    const std::uint32_t* splits,std::uint32_t split_count,std::uint32_t a,std::uint32_t b,std::uint32_t course,std::uint32_t flags_at){
    auto entry=[&](std::int32_t k){return first+std::uint32_t(k)*size;};
    if(m.u32(entry(9)+0x14u)<time)return 0u;
    std::int32_t k=9;
    while(k>=0&&!(m.u32(entry(k)+0x14u)<time))--k;
    if(k==0)m.put8(0x84bcfcu,1);                                  // 4EF430
    for(std::int32_t j=9;j>=k+2;--j)for(std::uint32_t o=0;o<size;o+=4)m.put32(entry(j)+o,m.u32(entry(j-1)+o));
    const std::uint32_t r=entry(k+1);
    m.put32(r+0x14u,time);
    for(std::uint32_t i=0;i<split_count;++i)m.put32(r+0x18u+i*4u,splits[i]);
    m.put8(r+flags_at,std::uint8_t((((b&1u)|((course&0xffu)<<1))<<1)|(a&1u)));
    m.put8(r+0x10u,m.u8(0x7c23fcu));
    for(std::uint32_t i=0;;++i){const auto ch=m.u8(0x7c23e0u+i);m.put8(r+i,ch);if(!ch)break;}   // strcpy (license name)
    return 1u;
}
// soft_fault: a display call (GAD_PUB): a service without a port is
// returned there instead of latching the module.
constexpr std::uint32_t RacerTableBase=0x5c000000u;      // 80FB20 records as mapped here
// 5802DD sprintf(out, format, ...) over the guest memory: the integer, character and
// string conversions with their flags / width / precision (the formats the game's
// screens use); a floating-point or other conversion is refused.
std::string guest_cstring(PcRaceMemory& m,std::uint32_t a){std::string s;for(;m.u8(a);++a)s+=char(m.u8(a));return s;}
// The printf text of format a[first - 1] with its arguments a[first..argc).
std::string guest_format(PcRaceMemory& m,const std::uint32_t* a,std::uint32_t argc,std::uint32_t first){
    std::string out;std::uint32_t next=first;
    auto arg=[&]()->std::uint32_t{if(next>=argc)throw EndUnported(0x5802ddu,"sprintf: missing argument");return a[next++];};
    for(std::uint32_t f=a[first-1u];;++f){
        const char ch=char(m.u8(f));
        if(!ch)break;
        if(ch!='%'){out+=ch;continue;}
        std::string spec="%";
        for(;;){const char x=char(m.u8(++f));spec+=x;if(std::strchr("-+ #0",x)==nullptr||x==0)break;}
        while(std::isdigit(std::uint8_t(spec.back()))){spec+=char(m.u8(++f));}
        if(spec.back()=='.'){spec+=char(m.u8(++f));while(std::isdigit(std::uint8_t(spec.back())))spec+=char(m.u8(++f));}
        const char conv=spec.back();char buf[96];
        switch(conv){
        case '%':out+='%';continue;
        case 'd':case 'i':std::snprintf(buf,sizeof buf,spec.c_str(),std::int32_t(arg()));break;
        case 'u':case 'x':case 'X':case 'o':std::snprintf(buf,sizeof buf,spec.c_str(),arg());break;
        case 'c':std::snprintf(buf,sizeof buf,spec.c_str(),int(std::uint8_t(arg())));break;
        case 's':{std::string s;for(std::uint32_t p=arg();m.u8(p);++p)s+=char(m.u8(p));
            std::vector<char> b(s.size()+64);std::snprintf(b.data(),b.size(),spec.c_str(),s.c_str());out+=b.data();continue;}
        default:throw EndUnported(0x5802ddu,"sprintf conversion "+spec);
        }
        out+=buf;
    }
    return out;
}
std::uint32_t crt_sprintf_5802dd(PcRaceMemory& m,const std::uint32_t* a,std::uint32_t argc){
    const std::string out=guest_format(m,a,argc,2u);
    for(std::size_t k=0;k<out.size();++k)m.put8(a[0]+std::uint32_t(k),std::uint8_t(out[k]));
    m.put8(a[0]+std::uint32_t(out.size()),0);
    return std::uint32_t(out.size());
}
template<class F> bool run_end(NativeRuntimeContext& c,F&& body,driving::PcMatrixStack* shared_matrices=nullptr,std::uint32_t* soft_fault=nullptr){
    auto& e=native_race_end(c);
    if(e.fault)return false;
    PcRaceMemory m;
    // EXE read-only ranges (the arcade tables 5C6F00..5C9600 ...); everything below shadows them.
    for(std::size_t i=0;i<EmbeddedExeRangeCount;++i)m.map_const(EmbeddedExeRanges[i].base,EmbeddedExeRanges[i].data,EmbeddedExeRanges[i].size);
    native_race_requests_map(c,m);   // the C2C request manager 7F9460 (variant 5) and its heap
    // The AREA module (7D2D80 block: matrices 7D2DA0/7D3190, 7D33D0; .data
    // 635F2C..: 635F34) while its owner exists (44FD00 drops it).
    if(c.race_area)for(const auto& r:native_race_area_memory(c,nullptr,true).regions()){
        if(r.writable)m.map(r.base,r.data,r.size);else m.map_const(r.base,r.data,r.size);}
    goal_camera_map_tables(m);
    e.state.map(m);race_end_map_tables(m);
    for(auto& [at,bytes]:e.event_works)m.map(at,bytes.data(),bytes.size());
    // Race globals (copies of read-only words; written words are mapped).
    std::uint32_t variant=c.game_mode.game_variant,preset=c.start_mode.course_preset,player_ptr=0x7804b0u;
    std::uint32_t update=c.frame_state.update_index_8a8cdc;
    m.map(0x780258u,reinterpret_cast<std::uint8_t*>(&variant),4);m.map(0x78024cu,reinterpret_cast<std::uint8_t*>(&preset),4);
    m.map(0x8a8cdcu,reinterpret_cast<std::uint8_t*>(&update),4);
    m.map(0x8367f4u,reinterpret_cast<std::uint8_t*>(&c.race.frames_8367f4),4);   // event 1 frame counter (49ACE0/49ACF0)
    m.map(0x8367c0u,c.start_mode.scene_owner_models_8367c0.data(),c.start_mode.scene_owner_models_8367c0.size());   // 499BB0 flags
    m.map(0x67f614u,reinterpret_cast<std::uint8_t*>(&c.start_mode.start_system_handle_67f614),4);   // 4999D0 / 4999F0 loading anim
    float hud_84bd00=0.0f;float& value_84bd00=c.race_hud?c.race_hud->value_84bd00:hud_84bd00;m.map(0x84bd00u,reinterpret_cast<std::uint8_t*>(&value_84bd00),4);   // 4EF410 (HUD)
    auto& player=c.event_function36.car_select.car_799d18;
    m.map(0x799d18u,reinterpret_cast<std::uint8_t*>(&player_ptr),4);m.map(0x7804b0u,player.data(),0x10f0u);
    auto& license=c.event_function36.frontend_profiles.active;m.map(0x7c23e0u,license.data(),license.size());
    auto& common=c.event_function36.frontend_profiles.common;m.map(0x7b17f8u,common.data(),common.size());   // 7B17F8 record tables
    m.map(NativeRaceEnd::TextBase,e.text_465eb0.data(),e.text_465eb0.size());   // 465EB0 results
    m.map(0x84def8u,c.race.manager.score_84def8.data(),c.race.manager.score_84def8.size());   // 4F2xxx score block (name entry commit at rank -1)
    // OUTRUN2SP arcade frontend: the event-4 slot (79FB4C flags, 799C28 work 780440), the
    // texture / sun lists 7D26A8 (449FA0), the attract bytes and the 655B5C function table.
    m.map(0x79fb4cu,&c.event_state.slots[4].flags,1);
    static constexpr std::uint32_t Work4=0x780440u;m.map_const(0x799c28u,reinterpret_cast<const std::uint8_t*>(&Work4),4);
    m.map(0x7d26a8u,c.event_function36.car_select.sun_lists_7d26a8.data(),c.event_function36.car_select.sun_lists_7d26a8.size());
    m.map(0x83037cu,reinterpret_cast<std::uint8_t*>(&e.attract_frames_83037c),4);
    m.map(0x830364u,&c.event_function36.music_globals.track_830364,1);
    m.map(0x830374u,&c.event_function36.car_select.transmission_830374,1);
    m.map(0x83036du,&c.start_mode.vehicle_variant_83036d,1);
    m.map(0x83036cu,&c.race.sound.option_83036c,1);
    m.map(0x655b59u,reinterpret_cast<std::uint8_t*>(&c.start_mode.course_choice_655b59),1);
    static constexpr std::array<std::uint32_t,12> Table655b5c{1,2,3,4,5,6,7,8,9,10,11,12};
    m.map_const(0x655b5cu,reinterpret_cast<const std::uint8_t*>(Table655b5c.data()),sizeof Table655b5c);
    std::array<std::uint8_t,driving::PcEventSlotCount> event_flags{};
    if(variant==4u){   // LAN race end (mode 21 TIME OVER, bulk translated): the network blocks, CommRace, 7DD138, clock
        native_network_map_shared(m);
        for(std::size_t i=0;i<event_flags.size();++i)event_flags[i]=c.event_state.slots[i].flags;   // 79FB48.. (read-only copy)
        m.map_const(0x79fb48u,event_flags.data(),event_flags.size());
        m.map(0x79fb4cu,&c.event_state.slots[4].flags,1);
        auto& w=c.race.car_world;
        if(!m.mapped(0x7de418u,4))m.map(0x7de418u,w.commrace_7de418.data(),w.commrace_7de418.size());
        if(!m.mapped(0x7dd138u,1))m.map(0x7dd138u,&w.slot_7dd138,1);
        if(!m.mapped(0x7f1938u,4))m.map(0x7f1938u,reinterpret_cast<std::uint8_t*>(&w.clock_7f1938),4);
    }
    auto& rm=c.race.manager;
    m.map(0x6840ecu,reinterpret_cast<std::uint8_t*>(&rm.goal_6840ec),4);m.map(0x6840f0u,reinterpret_cast<std::uint8_t*>(&rm.goal_6840f0),4);
    m.map(0x6840f4u,reinterpret_cast<std::uint8_t*>(&rm.goal_6840f4),4);m.map(0x7d6764u,reinterpret_cast<std::uint8_t*>(&rm.input_lock_7d6764),4);
    // Event 7 camera override 82E7C0 (copied back after the call), the body
    // work 82E7F0 (tyre pointers 82EA38, angles 82EA78..), the CAMERA event
    // work (79F574 -> 79FE10) and the game mode 78026C.
    std::array<std::uint8_t,0x30> override_82e7c0{};override_in(c.race.camera_override,override_82e7c0.data());
    m.map(0x82e7c0u,override_82e7c0.data(),override_82e7c0.size());
    auto& shared=c.event_function36.car_select;
    m.map(0x82e7f0u,shared.body_82e7f0.data(),shared.body_82e7f0.size());
    std::uint32_t camera_79f574=0x79fe10u,mode_78026c=c.mode_state.current;
    m.map(0x79f574u,reinterpret_cast<std::uint8_t*>(&camera_79f574),4);m.map(0x79fe10u,shared.camera_79fe10.data(),shared.camera_79fe10.size());
    m.map_const(0x78026cu,reinterpret_cast<const std::uint8_t*>(&mode_78026c),4);
    // [63960C]: the rankings book pointer (453440 at boot), the book itself is PcRaceEndState's.
    const std::uint32_t book_63960c=PcRaceEndState::BookBase;
    m.map_const(0x63960cu,reinterpret_cast<const std::uint8_t*>(&book_63960c),4);
    // GAD_PUB globals (race_hud runtime): 836630..836718, 67EE38..67EE50
    // (67EE3C, the GAME OVER voice flag, is theirs).
    auto& hud=native_race_hud(c);
    m.map(GadPubState::Base,hud.gad.block.data(),hud.gad.block.size());m.map(GadPubState::DataBase,hud.gad.data.data(),hud.gad.data.size());
    m.map(GadPubState::Data2Base,hud.gad.data2.data(),hud.gad.data2.size());
    // 7F2560..7F26E0: the mission result block of the NAVI module (498670 variant 2, 45BF30).
    m.map(0x7f2560u,hud.navi.g7f1900.data()+(0x7f2560u-0x7f1900u),0x180u);
    // Racer ranking records (80FB20) and the names they point at (Races.bin,
    // EXE defaults).
    if(!c.mission.racers.table_80fb20.empty())m.map_const(RacerTableBase,c.mission.racers.table_80fb20.data(),c.mission.racers.table_80fb20.size());
    if(!c.mission.races_relocated.empty())m.map_const(NativeRacesBlobBase,c.mission.races_relocated.data(),c.mission.races_relocated.size());
    native_race_attack_map(c,m);   // variant 9: 686254.. and RaceAttack.bin (racer names)
    constexpr std::uint32_t Event36=0x5e000000u;           // [7B17E8] (4035F0)
    auto& object=c.event_function36.object;m.map(Event36,object.data(),object.size());
    auto& pool=c.event_function36.frontend_sprites;
    auto routed=[&](std::uint32_t pc){++e.routed[pc];};
    // CommRace block 7DE418.. bytes (offline network session words).
    auto& commrace_block=c.race.car_world.commrace_7de418;
    auto commrace=[&](std::uint32_t a)->std::uint32_t{
        if(a<0x7de418u||a-0x7de418u>=commrace_block.size())throw EndUnported(a,"CommRace block");return commrace_block[a-0x7de418u];};
    auto commrace_put=[&](std::uint32_t a,std::uint8_t v){
        if(a<0x7de418u||a-0x7de418u>=commrace_block.size())throw EndUnported(a,"CommRace block");commrace_block[a-0x7de418u]=v;};
    auto exe_u32=[](std::uint32_t a,std::uint32_t& v)->bool{
        for(std::size_t i=0;i<EmbeddedExeRangeCount;++i){const auto& r=EmbeddedExeRanges[i];
            if(a>=r.base&&a+4u<=r.base+r.size){std::memcpy(&v,r.data+(a-r.base),4);return true;}}
        return false;};
    auto exe_f32=[&](std::uint32_t a,float& v)->bool{std::uint32_t u;if(!exe_u32(a,u))return false;std::memcpy(&v,&u,4);return true;};
    auto cvtt_end=[](float f)->std::int32_t{if(!(f>-2147483904.0f&&f<2147483648.0f))return std::int32_t(0x80000000u);return std::int32_t(f);};
    auto released=[&](std::uint32_t pc){++e.released[pc];};
    // Mission manager and the selected Races record (variants 4 and 6).
    auto& mission=c.mission.manager;
    const RaceAssetPack* races=c.start_mode.scene_owner_race_assets;
    const std::int32_t record_index=(races&&mission.record_83637c>=0&&std::uint32_t(mission.record_83637c)<races->race_count)?mission.record_83637c:-1;
    auto race_u32=[&](std::uint32_t index,std::uint32_t off)->std::uint32_t{
        const std::size_t at=races->races_offset+std::size_t(index)*0x44u+off;
        if(at+4u>races->bytes.size())throw EndUnported(0x4f1a90u,"Races record outside Races.bin");
        std::uint32_t v;std::memcpy(&v,races->bytes.data()+at,4);return v;};
    auto record_u32=[&](std::uint32_t off)->std::uint32_t{
        if(record_index<0)throw EndUnported(0x83637cu,"no selected Races record");
        return race_u32(std::uint32_t(record_index),off);};
    auto races_with_key=[&](std::uint32_t key)->std::uint32_t{
        if(!races)throw EndUnported(0x4f1a90u,"Races.bin not loaded");
        std::uint32_t n=0;for(std::uint32_t i=0;i<races->race_count;++i)if(race_u32(i,0)==key)++n;return n;};
    auto find_race=[&](std::uint32_t key,std::uint32_t sub)->bool{
        if(!races)throw EndUnported(0x4f1a90u,"Races.bin not loaded");
        for(std::uint32_t i=0;i<races->race_count;++i)if(race_u32(i,0)==key&&race_u32(i,4)==sub)return true;return false;};
    auto manager=[&](std::uint32_t pc,const PcRaceCall& k)->std::uint32_t{
        std::uint32_t eax{};
        if(!native_race_manager_call(c,pc,k.args.data(),k.argc,eax,false))throw EndUnported(pc,"race manager accessor refused");
        return eax;};
    auto ensure_bank=[&](std::uint32_t token){
        FrontendSpriteTiming t{};if(pool.bank_scene(token,t))return;
        const std::uint32_t bank=token>>16;std::string err;
        if(!bind_bank(c,bank,err))throw EndUnported(0x42deb0u,"SPRANI bank "+hex(bank)+": "+err);
        ++e.bank_loads;};
    std::array<std::uint8_t,0x400> matrix_bytes{};driving::PcMatrixStack own_matrices{driving::Bytes(matrix_bytes.data(),matrix_bytes.size()),0,0,16};   // 407F40 (44A430) pushes one matrix
    driving::PcMatrixStack& matrices=shared_matrices?*shared_matrices:own_matrices;
    PcRaceContext ctx{m,matrices,[&](const PcRaceCall& k)->std::uint32_t{
        const auto* a=k.args.data();routed(k.pc);
        auto& ms=c.mode_state;auto& st=c.start_mode;
        switch(k.pc){
        // mode globals 7802xx
        case 0x43f900u:st.mode_countdown_780250=std::int32_t(a[0]);return 0u;
        case 0x43fa90u:if(st.game_flag_780248==0u)--st.mode_countdown_780250;return st.mode_countdown_780250<=0?1u:0u;
        case 0x43f980u:return ms.snapshot_destination;
        case 0x43f8e0u:return ms.requested;
        case 0x43f8f0u:return ms.previous;                                     // [780268]
        case 0x43f940u:c.game_mode.game_variant=a[0];return 0u;
        case 0x43f860u:return st.frontend_prepare.game_flag_780260;
        case 0x43f960u:return (st.course_preset==2u||st.course_preset==3u)?1u:0u;
        case 0x4957f0u:return st.selection_active_836374?1u:0u;
        case 0x55a930u:return st.manager_state_7f94c0;           // [7F9460+60]
        case 0x43f8c0u:{
            const std::uint32_t next=a[0];
            if(!native_runtime_request_mode(c,next))throw EndUnported(0x43f8c0u,"mode "+hex(next)+" outside the mode table");
            return 0u;}
        case 0x43f990u:{   // 43F990(mode): the pause-menu exit. [780254] (no native reader) / [780248] / [780270] = 0,
            // [780264] = 0, 4409C0 (ClrEvPauseFlag), then the mode request [78025C] = mode, [780274] = 1.
            st.game_flag_780248=0u;c.event_function36.title_game_state_780270=0u;ms.snapshot_source=0u;
            const driving::PcEventPauseServices pause{&c,[](void* u,std::uint32_t f,std::uint32_t v){
                auto& cc=*static_cast<NativeRuntimeContext*>(u);
                if(f==0x43f9d0u)cc.start_mode.game_flag_780248=std::uint8_t(v);
                else if(f==0x429810u)cc.event_function36.title_pause_flag_95b214=v;
                else if(f==0x449040u)cc.event_function36.title_pause_flag_7d2614=v;}};
            driving::clr_ev_pause_flag_4409c0(c.event_state,pause);
            ms.requested=a[0];ms.transition_pending=1u;return 0u;}
        // events
        case 0x440a10u:driving::event_suspend_440a10(c.event_state,a[0],a[1]);return 0u;
        case 0x440a30u:driving::event_resume_440a30(c.event_state,a[0],a[1]);return 0u;
        case 0x440330u:driving::event_close_serial_440330(c.event_state,a[0],a[1]);return 0u;
        case 0x4401d0u:driving::event_close_4401d0(c.event_state,a[0]);return 0u;
        case 0x440110u:driving::event_setup_440110(c.event_state,a[0],a[1],c.event_descriptors,c.event_functions);return 0u;
        case 0x440cd0u:throw EndUnported(0x440cd0u,"attract data 7F1964 held (45AE40 loader not ported)");
        // race manager
        case 0x4505d0u:case 0x450240u:case 0x450230u:case 0x451350u:case 0x4505a0u:case 0x4506a0u:return manager(k.pc,k);
        case 0x4505e0u:case 0x4505f0u:return 0u;                 // float results discarded by 453370 (pure accessors)
        case 0x44c940u:{
            // 44C940: cache 635F2C/635F30, else the stage record +8 (44C8D0 over the AREA memory).
            auto& area=native_race_area_memory(c,nullptr,true);
            auto& rt=c.game_mode.course_runtime;const std::uint32_t key=a[0];
            if(key==std::uint32_t(rt.stage_key_635f2c))return std::uint32_t(rt.stage_value_635f30);
            std::uint32_t v=0;const std::int32_t n=area.i32(0x7d33c4u);const std::uint32_t base=area.u32(0x7d33bcu);
            for(std::int32_t i=0;i<n;++i)if(area.u32(base+std::uint32_t(i)*0x78u+4u)==key){v=area.u32(base+std::uint32_t(i)*0x78u+8u);break;}
            rt.stage_key_635f2c=std::int32_t(key);rt.stage_value_635f30=std::int32_t(v);return v;}
        case 0x47ef30u:{   // variant-0 new-record test (race_records over the ghost runtime's record state)
            std::uint32_t r=0;
            if(!native_records_ranking_47ef30(c,a[0],r))throw EndUnported(0x47ef30u,"47EF30: "+native_race_ghosts(c).error);
            return r;}
        // course resources: the native owners are rebuilt by the next START
        case 0x42dfd0u:{
            // 42DFD0 releases every 956D88 resource except 0, 3, 0x17, 0x2C, 0x33, 0x48:
            // the SUMO_FE pack 0x44 is requested again by the next 445500 stage 5.
            auto& f=c.event_function36;f.frontend_ready_count=0u;f.frontend_ready_bytes=0u;f.frontend_resource_pending=0u;
            released(k.pc);return 0u;}
        case 0x43de50u:case 0x44a1a0u:case 0x4f11b0u:case 0x4f0600u:
            released(k.pc);native_race_area_teardown(c,k.pc,k.argc?a[0]:0u);return 0u;
        case 0x46fc30u:released(k.pc);return native_race_road_tables_46fc30(c,a[0]);
        case 0x44fd00u:
            // 44FD00 (first of the mode 28/30 releases): 44FBE0 on the eight
            // async slots 7D34D0.., then 42EBF0, 4489F0, 4489C0, 44C3D0 and
            // 4F2210 free every allocation of the AREA owner: the native
            // owner (slots, allocations, lanes, road tables, 7D2D80 block) is
            // dropped and rebuilt empty by the next race, as at boot.
            released(k.pc);c.race_area.reset();return 0u;
        case 0x4489c0u:case 0x44c3d0u:case 0x4f2210u:
        case 0x4276b0u:released(k.pc);c.pc_sound.unload_all_4276b0();return 0u;
        case 0x42ebf0u:case 0x4489f0u:case 0x429a10u:case 0x47ed40u:case 0x451a00u:
            released(k.pc);return 0u;
        case 0x465fa0u:native_ghost_release_465fa0(c);return 0u;
        // resources (42DEB0/429920 request, 42DF90/4299A0 all loaded)
        case 0x42deb0u:case 0x429920u:{
            if(a[0]<0x20u&&native_sprite2d_xst_path(a[0])){++e.released[k.pc];return 0u;}   // 2D XST bank (select / ranking ...): loaded by the 2D renderer on first use
            if(a[0]<0x20u||a[0]>=0x4bu)throw EndUnported(k.pc,"resource "+hex(a[0]));
            std::string err;
            if(k.pc==0x429920u&&!bind_bank(c,a[0],err)){e.bank_pending_failed=true;e.bank_error=err;}
            else if(k.pc==0x429920u)++e.bank_loads;
            return 0u;}
        case 0x42df90u:return 1u;   // 42DF90 all requested banks resident: 42DEB0 requests complete at once natively
        case 0x4299a0u:if(e.bank_pending_failed)throw EndUnported(0x4299a0u,e.bank_error);return 1u;
        case 0x42dfb0u:case 0x4299c0u:released(k.pc);return 0u;
        // SPRANI pool
        case 0x428600u:pool.clear_allocations();return 0u;
        case 0x428320u:ensure_bank(a[0]);return pool.create(a[0],a[1],a[2]);
        case 0x428460u:ensure_bank(a[0]);return pool.create(a[0],a[1],a[2],std::int32_t(a[3]),std::int32_t(a[4]),true);
        case 0x4285a0u:if(a[0]<FrontendSprites::Count)pool.release(a[0]);return 0u;
        case 0x428770u:if(auto* s=pool.get_mutable(a[0]))s->mode=a[1];return 0u;
        case 0x428730u:if(std::int32_t(a[0])>=0)if(auto* s=pool.get_mutable(a[0]))s->visible=a[1]!=0u;return 0u;   // instance +4 (428170 draws it when non-zero)
        case 0x428800u:{float f;std::memcpy(&f,&a[1],4);if(a[0]<FrontendSprites::Count)pool.set_speed(a[0],f);return 0u;}
        case 0x428880u:return std::int32_t(a[0])<0?0u:pool.status(a[0]);
        case 0x4287b0u:{
            if(a[0]>=FrontendSprites::Count)return 0u;
            std::array<float,16> mat{};for(std::uint32_t w=0;w<16;++w)mat[w]=m.f32(a[1]+w*4);
            pool.set_matrix(a[0],mat);++e.sprite_matrices;return 0u;}
        case 0x4999d0u:{
            ensure_bank(0x480000u);
            st.start_system_handle_67f614=std::int32_t(pool.create(0x480000u,3u,0u,0x28,0x3b,true));return 0u;}
        // services of the translated mode 1..5 / 8 callbacks (race_modes_tr.cpp)
        case 0x42df70u:return 1u;   // resource a[0] resident: the native loader completes 42DEB0 requests at once
        case 0x4b72f0u:common_anim_4b72f0(ctx,a[0]);return 0u;
        case 0x4b7630u:common_anim_release_4b7630(ctx);return 0u;
        case 0x487240u:camera_reset_487240(ctx);return 0u;
        case 0x4b0960u:return name_entry_done_4b0960(ctx);                  // [842804]
        case 0x4532e0u:return m.u32(0x7d6744u);                            // mov eax,[7D6744] (453210 sets 1)
        case 0x440a60u:case 0x440b20u:{                                   // malloc / free the current event's work
            struct U{NativeRaceEnd& e;PcRaceMemory& m;};U u{e,m};
            driving::PcEventWorkServices ws{&u,nullptr,
                [](void* p,std::uint32_t n)->std::uint32_t{auto& x=*static_cast<U*>(p);
                    const std::uint32_t at=x.e.next_event_work;x.e.next_event_work+=(n+0xfffu)&~0xfffu;
                    auto& b=x.e.event_works[at];b.assign(n,0);x.m.map(at,b.data(),b.size());return at;},
                [](void* p,std::uint32_t base){static_cast<U*>(p)->e.event_works.erase(base);}};
            auto& handle=e.event_work_handles[c.event_state.current_slot];
            if(k.pc==0x440a60u)return driving::malloc_now_event_work_440a60(c.event_state,handle,a[0],a[1],ws);
            driving::free_now_event_work_440b20(c.event_state,handle,ws);return 0u;}
        case 0x449ac0u:race_time_words_449ac0(m,a[0],a[1],a[2],a[3],a[4]);return 0u;
        case 0x5802ddu:return crt_sprintf_5802dd(m,a,k.argc);
        // CRT strings: 581000 strstr, 581780 strncpy, 58DD1E _stricmp (C locale)
        case 0x581000u:{
            const std::string hay=guest_cstring(m,a[0]),needle=guest_cstring(m,a[1]);
            const auto at=hay.find(needle);return at==std::string::npos?0u:a[0]+std::uint32_t(at);}
        case 0x581780u:{
            bool end=false;
            for(std::uint32_t i=0;i<a[2];++i){const std::uint8_t ch=end?0u:m.u8(a[1]+i);if(!ch)end=true;m.put8(a[0]+i,ch);}
            return a[0];}
        case 0x58dd1eu:{
            for(std::uint32_t i=0;;++i){
                const int x=std::tolower(m.u8(a[0]+i)),y=std::tolower(m.u8(a[1]+i));
                if(x!=y)return std::uint32_t(x-y);
                if(!x)return 0u;
            }}
        case 0x4021b0u:return 0u;                                 // debug print (vsnprintf to a dropped buffer)
        case 0x42c2f0u:{                                          // image table entry into the caller's record
            std::array<std::uint8_t,0x14> rec{};for(std::uint32_t i=0;i<0x14u;++i)rec[i]=m.u8(a[0]+i);
            pc_image_entry_42c2f0(native_sprite2d(c).state,rec.data(),a[1]);
            for(std::uint32_t i=0;i<0x14u;++i)m.put8(a[0]+i,rec[i]);
            if(!(rec[0]|rec[1]|rec[2]|rec[3]))e.image_entries_42c2f0[a[0]]=a[1];   // bank not loaded natively yet
            return 0u;}
        case 0x42cfe0u:{                                          // queue an image record
            if(!e.hud_draws)throw EndUnported(0x42cfe0u,"2D image record outside a display");
            RaceHudDraw d;d.pc=k.pc;d.args[0]=a[1];for(std::uint32_t i=0;i<0x48u;++i)d.record[i]=m.u8(a[0]+i);
            // A 42C2F0 entry of a bank not loaded yet here: the token, resolved when drawn.
            if(const auto it=e.image_entries_42c2f0.find(a[0]);it!=e.image_entries_42c2f0.end()){d.args[1]=it->second;d.args[2]=1u;e.image_entries_42c2f0.erase(it);}
            e.hud_draws->push_back(d);return 0u;}
        case 0x428980u:{                                          // 428980(token, layer, NULL): 428A10 frame 1
            if(!e.hud_draws)throw EndUnported(0x428980u,"sprite draw outside a display");
            RaceHudDraw d;d.pc=k.pc;d.args[0]=a[0];d.args[1]=a[1];e.hud_draws->push_back(d);return 0u;}
        case 0x4bc990u:{                                          // 4BC990(style, x, y, text): 4BA9D0(.., 9, 1.0)
            if(!e.hud_draws)throw EndUnported(0x4bc990u,"digits outside a display");
            const auto mark=m.mark();
            m.map(0x688b00u,hud.navi.g688b00.data(),hud.navi.g688b00.size());
            NaviPubServices ns;ns.m=&m;ns.sprites=&c.event_function36.frontend_sprites;ns.pause_95b214=c.event_function36.title_pause_flag_95b214;
            ns.draws=e.hud_draws;
            const bool ok=navi_digits_4ba9d0(ns,a[0],std::int32_t(a[1]),std::int32_t(a[2]),guest_cstring(m,a[3]),9u,1.0f);
            m.release(mark);
            if(!ok)throw EndUnported(0x4ba9d0u,"digits 4BA9D0");
            return 0u;}
        case 0x4b6f80u:arcade_reset_4b6f80(ctx);return 0u;
        case 0x440240u:driving::event_close_all_440240(c.event_state);return 0u;
        case 0x40ecb0u:return c.frame_state.frame_counter_95af0c;            // [95AF0C]
        case 0x4556f0u:   // [7F1930] == [7F1884]: both written only with the network manager 7DF34C
            if(st.network_manager_7df34c_present)throw EndUnported(0x4556f0u,"7F1930/7F1884 with a network manager");
            return 1u;
        case 0x4b1dd0u:return name_entry_record_4b1dd0(ctx);   // the variant-0 ending record test (4524B0)
        case 0x4bfb20u:return result_input_4bfb20(ctx);   // the result / skip input (race_end_modes)
        case 0x4b5f60u:case 0x4b5fc0u:case 0x4b5fd0u:{   // AUTOSCENE start / frame / state (race_robots_runtime)
            std::uint32_t eax=0;
            if(!native_autoscene_call(c,k.pc,k.argc?a[0]:0u,eax))throw EndUnported(k.pc,"AUTOSCENE: "+c.race.robots.autoscene_error);
            return eax;}
        case 0x4f2020u:   // motion group request (4F2130 / 4F2060 scheduler run synchronously); the result is unused
            if(!native_rob_motion_group_request_4f2020(c,a[0],a[1]))throw EndUnported(0x4f2060u,"motion group "+hex(a[0])+": "+c.race.robots.motion_tables.error);
            return 0u;
        case 0x4066d0u:case 0x406730u:case 0x4103f0u:{   // bank objects on the renderer (the robots' renderer binding)
            auto* r=c.race.robots.renderer;
            if(!r)throw EndUnported(k.pc,"no renderer for the bank objects");
            const auto h=a[0];
            if(h!=0xffffffffu&&(h>>16)<0x223u)(void)native_race_bank_ensure(c,*r,h>>16);
            if(k.pc==0x4066d0u)return r->object_group_type_4066d0(h,a[1]);
            if(k.pc==0x406730u)return r->object_mesh_flags_406730(h,a[1],a[2]);
            return r->object_shaders_4103f0(h,a[1]);}
        // the light records 899B98 (b 0: + (a + c) * 0xA0; 1: 89A138 + (c + a * 2) * 0xA0; else
        // 899D78 + ...) and the fog records 7D3A10 + i * 0x1C of the renderer's environment
        case 0x407cf0u:case 0x407d60u:case 0x407dd0u:case 0x407e40u:case 0x407f40u:case 0x408030u:case 0x4080a0u:
        case 0x408390u:case 0x4083c0u:case 0x451800u:{
            auto* r=c.race.robots.renderer;
            if(!r)throw EndUnported(k.pc,"no renderer for the light records");
            auto& env=r->environment();
            PcRaceMemory lm;
            for(std::size_t i=0;i<EmbeddedExeRangeCount;++i)lm.map_const(EmbeddedExeRanges[i].base,EmbeddedExeRanges[i].data,EmbeddedExeRanges[i].size);
            lm.map(0x899b98u,env.lights_899b98.data(),env.lights_899b98.size());lm.map(0x7d3a10u,env.fog_7d3a10.data(),env.fog_7d3a10.size());
            (void)ending_light_service(lm,matrices,k);return 0u;}
        // the arcade ending (race_ending.cpp)
        case 0x448cd0u:{   // object handle by name (object db 7CC1E0)
            if(!c.object_db.built)throw EndUnported(0x448b90u,"object db not built");
            std::string name;for(std::uint32_t q=a[0];name.size()<0x100u;++q){const auto ch=m.u8(q);if(!ch)break;name.push_back(char(ch));}
            ++c.object_db.lookups;
            const auto h=object_db_find_448b10(c.object_db,name);if(h==0xffffffffu)++c.object_db.misses;return h;}
        case 0x406780u:{   // mesh records of a bank object (the AREA owner's renderer binding)
            if(!c.race_area)throw EndUnported(k.pc,"no AREA owner for the bank objects");
            return native_race_area_call(c,c.race.robots.renderer,k);}
        case 0x4162b0u:{   // the glow level [8A8C18] * 0.25 (race_sky 452080)
            if(!c.race_effects.glow_8a8c18)throw EndUnported(k.pc,"glow record 8A8C18 not bound");
            float g;std::memcpy(&g,c.race_effects.glow_8a8c18,4);g=g*0.25f;
            std::uint32_t v;std::memcpy(&v,&g,4);return v;}
        case 0x4f6e40u:{   // the car's collision box from 520950(model) = 5E8A88 + model * 0x3C
            if(a[0]!=0x7804b0u)throw EndUnported(k.pc,"4F6E40 on another car");
            std::array<std::uint8_t,0x3c> desc{};
            const std::uint32_t d=0x5e8a88u+std::uint32_t(std::int32_t(std::int8_t(m.u8(a[0]+0x11u))))*0x3cu;
            for(std::uint32_t q=0;q<0x3cu;++q)desc[q]=m.u8(d+q);
            vehicle_collision_init_4f6e40(driving::Bytes(player.data(),player.size()),driving::Bytes(desc.data(),desc.size()));return 0u;}
        case 0x46ba20u:case 0x46bb20u:{   // the car's shadow volume (renderer)
            if(a[0]!=0x7804b0u)throw EndUnported(k.pc,"car shadow of another car");
            auto& cs=c.event_function36.car_select;
            if(!(cs.shadow_service&&cs.shadow_service(k.pc,driving::Bytes(player.data(),player.size()))))++cs.shadow_unported;
            return 0u;}
        case 0x46bbc0u:{   // 406630(5B2FE4[model], 414040): the reflection cube into the car's environment map slot
            auto* r=c.race.robots.renderer;
            if(r){native_car_reflection_init(*r);if(r->environment_map_46bbc0(driving::Bytes(player.data(),player.size())))return 0u;}
            released(0x406630u);return 0u;}
        // SCN_EFC work setters (event 387's work 79F5EC): 4AF580 / 4AFBA0 of the ending (452B10, 4524E0)
        case 0x4af550u:case 0x4af560u:case 0x4af570u:case 0x4af580u:case 0x4af590u:case 0x4afb40u:case 0x4afb50u:
        case 0x4afb70u:case 0x4afb80u:case 0x4afba0u:case 0x4afd90u:case 0x4afb60u:case 0x4af5a0u:case 0x4afb90u:{
            if(c.event_state.slots[387].work_token==0u)throw EndUnported(k.pc,"SCN_EFC (event 387) work not open");
            std::uint32_t eax=0;
            if(!native_race_effects_setter(c,k,eax))throw EndUnported(k.pc,"SCN_EFC setter");
            return eax;}
        // audio
        case 0x424ae0u:{   // 424AE0: ClearAllSound, the ICS cache 955AE8..955C80 cleared, event 0x17F suspended
            ++c.race.sound.clear_alls;c.pc_sound.clear_all_427630();if(c.race.sound.stop_all)c.race.sound.stop_all();
            for(std::uint32_t q=0x955ae8u;q<0x955c80u;q+=0x18u)c.race.sound.state.put32(q,0);
            driving::event_suspend_440a10(c.event_state,0x17fu,1u);return 0u;}
        case 0x427630u:++c.race.sound.clear_alls;c.pc_sound.clear_all_427630();if(c.race.sound.stop_all)c.race.sound.stop_all();return 0u;
        case 0x401000u:{auto& f=c.event_function36;++f.music_play_requests;f.music_last_track=a[1];
            if(f.music_play)(void)f.music_play(f.music_user,a[0],a[1],a[2]);return 1u;}
        case 0x401030u:{auto& f=c.event_function36;++f.music_stop_requests;if(f.music_stop)(void)f.music_stop(f.music_user,a[0]);return 1u;}
        // input
        case 0x4035f0u:return Event36;
        case 0x4536f0u:return c.race.car_world.switch_7d6770.pressed&a[0];   // 47F110 = 0: [7D6770+8] & mask
        // other
        case 0x42e020u:return 1u;                                  // `mov eax,1`
        case 0x455710u:if(st.network_manager_7df34c_present)throw EndUnported(0x455710u,"network manager present");return 0u;
        case 0x455730u:
            if(st.network_manager_7df34c_present)throw EndUnported(0x455730u,"network manager present");
            // Offline branch 45578F: the CommRace words (7DF198, 7DF10F, 7F1844,
            // 7DF1A0..7DF1A8, 7DF110/7DF118 per player) have no native owner;
            // 7DD138 = 0 and 450490(0xAA) on the race manager.
            c.race.car_world.slot_7dd138=0u;race_unpack_route_450490(c.race.manager.state,0xaau);released(0x455730u);return 0u;
        case 0x4edce0u:e.frontend_manager_659944=a[0];return 0u;   // [659930+14] (ECX = 659930)
        case 0x48b1f0u:return e.attract_frames_83037c;             // mode 10 control counter (attract not ported: stays 0)
        case 0x499890u:{
            const std::uint32_t v=c.game_mode.game_variant;
            if(v!=5u&&v!=6u)return 0u;
            // [67EE4C] caches 499730's result (-1 at boot, reset by the mode 36 exit 4983D0).
            if(m.i32(0x67ee4cu)==-1){
                const std::int32_t r=native_mission_reward_499730(c,true);
                m.put32(0x67ee4cu,std::uint32_t(r));
                if(r==-1)return 0u;
                ++e.mission_rewards_deferred;e.mission_reward_last=r;
            }
            return 1u;}
        case 0x496170u:case 0x4e85e0u:case 0x496180u:case 0x4e8620u:return native_mission_reload_call(c,k.pc);   // mission data reload
        // console flow (modes 30/34/35) and the goal close 49CDF0
        case 0x4278c0u:c.race.sound_commands.push_back(a[0]);c.pc_sound.unload_4278c0(a[0]);return 0u;
        case 0x427700u:return c.pc_sound.request_427700(a[0]);
        case 0x4249f0u:{auto& f=c.event_function36;
            if(!f.frontend_effect||!f.frontend_effect(f.frontend_effect_user,a[0]))++e.released[0x4249f0u];return 0u;}
        case 0x424940u:{   // race SE queue 9563E8 gated by event 383
            auto& w=c.race.car_world;
            driving::PcSoundQueue q{driving::Bytes(w.sound_entries_9563e8.data(),w.sound_entries_9563e8.size()),
                driving::Bytes(w.sound_state.data(),w.sound_state.size()),std::uint8_t(c.event_state.slots[383].flags)};
            driving::pc_enqueue_sound(q,a[0]);return 0u;}
        case 0x46c500u:return st.manager_state_7f94c0!=0u?1u:0u;          // 55A930 == 0 offline: 0 (no network)
        case 0x48b310u:return st.flag_830394;
        case 0x48b350u:return std::int32_t(st.frontend_prepare.output_code_656234)<0x3c?1u:0u;
        case 0x44ff10u:return manager(k.pc,k);
        case 0x4999f0u:{
            native_loading_animation_close_4999f0(c);return 0u;}
        case 0x448990u:released(k.pc);return 1u;                           // resource release (START rebuilds its set)
        case 0x44dbb0u:{std::uint32_t v{};if(!native_course_world_id_44dbb0(c,a[0],v))throw EndUnported(0x44dbb0u,"course world id");return v;}
        case 0x44c500u:return native_race_area_memory(c,nullptr,true).u32(0x7d2e90u);
        case 0x44c510u:return native_race_area_memory(c,nullptr,true).u32(0x7d31e4u);
        case 0x4b99d0u:{
            // [8447DC + [car]*4]: the car's score (4B9770 score add, NAVI init 4BCA86), in the
            // HUD owner's NAVI block.
            const std::uint32_t id=m.u32(a[0]);if(id>=0x20u)throw EndUnported(0x4b99d0u,"car id");
            ++e.released[0x4b99d0u];
            if(!c.race_hud)return 0u;
            std::uint32_t v;std::memcpy(&v,c.race_hud->navi.g842800.data()+(0x8447dcu-0x842800u)+id*4u,4);return v;}
        case 0x45b820u:{   // mov eax,[7F1C80] (Steam build, plain): the Heart Attack total of the NAVI block
            const std::size_t o=0x7f1c80u-0x7f1900u;auto& g=hud.navi.g7f1900;
            if(g.size()<o+4u)throw EndUnported(0x45b820u,"7F1C80 outside the NAVI block");
            std::uint32_t v;std::memcpy(&v,g.data()+o,4);return v;}
        // Mission manager accessors (836350..8363A8, 67E6A4..67E6B8, the
        // selected Races record [83637C], grades 7C25B7 + key * 5 + sub).
        case 0x496510u:                                            // VM entry: variant != 4 -> [67E6B0]
            if(c.game_mode.game_variant==4u){                    // LAN: 6 - this player's rank 45A0B0(455AD0) (0 past the 6th)
                std::uint32_t rank=0;
                if(!native_network_u32(0x7d68acu)||!native_network_invoke(c,0x45a0b0u,0,{std::uint32_t(c.race.car_world.slot_7dd138)},&rank))
                    throw EndUnported(0x45a0b0u,"variant 4 ranking: no LAN session");
                rank&=0xffu;
                return rank<6u?6u-rank:0u;
            }
            return mission.v67e6b0;
        case 0x495b20u:return record_index>=0?record_u32(0x20u):0u;
        case 0x495b10u:return record_index>=0?record_u32(0x14u):0u;
        case 0x495b80u:return mission.v836398;
        case 0x495c10u:mission.v836398=1u;return 0u;
        case 0x495820u:return mission.v8363a4;
        case 0x495890u:return mission.v67e6b4;
        case 0x495830u:return std::uint32_t(mission.v67e6b8);
        case 0x496440u:                                            // 4774A0: [80FB2C]
            if(c.game_mode.game_variant==4u){                    // LAN: 456E30, the players finished or connected
                std::uint32_t n=0;
                if(!native_network_u32(0x7d68acu)||!native_network_invoke(c,0x456e30u,0,{},&n))
                    throw EndUnported(0x456e30u,"variant 4 player count: no LAN session");
                return n;
            }
            return c.mission.racers.v80fb2c;
        case 0x4959e0u:{                                           // 495990: the key has a race `sub`
            const std::uint32_t key=st.scene_owner_race_key,sub=st.scene_owner_race_sub_key;
            if(!(sub<races_with_key(key)))return 0u;
            const std::uint32_t g=m.u8(0x7c25b7u+5u*key+sub);
            return g>=4u&&g!=7u?1u:0u;}
        case 0x495a60u:{                                           // next race of the chain once graded
            if(!mission.v836398)return 0u;
            const std::uint32_t key=st.scene_owner_race_key;
            if(std::int32_t(m.u8(0x7c25b7u+5u*key+st.scene_owner_race_sub_key))<std::int32_t(record_u32(0x24u)))return 0u;
            const std::uint32_t next=++st.scene_owner_race_sub_key;
            if(find_race(key,next))return 1u;
            --st.scene_owner_race_sub_key;return 0u;}
        case 0x495a20u:{                                           // record the mission grade 67E6B0
            const std::uint32_t key=record_u32(0u),sub=record_u32(4u),slot=0x7c25b7u+5u*key+sub;
            const std::uint32_t g=m.u8(slot);
            if(std::int32_t(mission.v67e6b0)>std::int32_t(g)||g==7u){
                m.put8(0x7c27d4u,std::uint8_t(m.u8(0x7c27d4u)|2u));m.put8(slot,std::uint8_t(mission.v67e6b0));}
            return 0u;}
        // OutRun Miles loader (4EF280): \Scripts\bin\OutrunMiles.bin, a PC category blob
        case 0x4f12a0u:{
            if(a[0]!=0x5cee0cu||a[1]!=0x84bcb0u)throw EndUnported(0x4f12a0u,"unexpected category file");
            auto* store=c.event_function36.retail_assets;if(!store)throw EndUnported(0x4f12a0u,"no retail store");
            std::string err;
            if(!retail_asset_read_relative(*store,"Scripts/bin/OutrunMiles.bin",e.miles_file,1u<<20,&err))throw EndUnported(0x4f12a0u,"OutrunMiles.bin: "+err);
            if(!driving::course_reloc_blob_open_r077(e.miles_file.data(),e.miles_file.size(),e.miles_blob))throw EndUnported(0x4f12a0u,"OutrunMiles.bin: category blob rejected");
            m.map(NativeRaceEnd::MilesFileBase,e.miles_file.data(),e.miles_file.size());
            m.put32(0x84bcb0u,NativeRaceEnd::MilesFileBase+std::uint32_t(e.miles_blob.index_offset));
            m.put32(0x84bcb4u,NativeRaceEnd::MilesFileBase);m.put32(0x84bcb8u,2u);m.put32(0x84bcbcu,std::uint32_t(e.miles_file.size()));
            return 1u;}
        case 0x4f1a90u:{
            const char* name=a[0]==0x5cee04u?"SETUP":a[0]==0x5cedfcu?"PRICES":nullptr;
            if(!name||a[1]!=0x84bcb0u||e.miles_file.empty())throw EndUnported(0x4f1a90u,"unexpected category");
            auto* p=static_cast<std::uint8_t*>(driving::runtime_category_records_4f1a90(name,e.miles_blob));
            if(!p)return 0u;
            return NativeRaceEnd::MilesFileBase+std::uint32_t(p-e.miles_file.data());}
        case 0x4f1210u:{
            if(e.miles_file.empty())throw EndUnported(0x4f1210u,"no category blob");
            for(const char* name:{"SETUP","PRICES"}){
                auto* p=static_cast<std::uint8_t*>(driving::runtime_category_records_4f1a90(name,e.miles_blob));
                if(p&&NativeRaceEnd::MilesFileBase+std::uint32_t(p-e.miles_file.data())==a[0])
                    return std::uint32_t(driving::runtime_category_count_4f1ba0(name,e.miles_blob));
            }
            return 0u;}
        case 0x580c38u:if(a[0]!=NativeRaceEnd::MilesFileBase)throw EndUnported(0x580c38u,"delete of an unknown block");++e.miles_releases;return 0u;   // the file stays mapped until the run returns
        // goal (mode 19 / 33)
        case 0x443ea0u:if(k.ecx!=Event36)throw EndUnported(0x443ea0u,"443EA0 on another object");
            native_event36_close_overlays_443ea0(c);return 0u;
        case 0x451180u:case 0x450320u:case 0x450380u:case 0x44dc50u:case 0x4502e0u:case 0x450250u:return manager(k.pc,k);
        case 0x44b7b0u:return m.u8(0x7d33d0u);                     // VM entry 44B7B0: mov al,[7D33D0]
        case 0x4493c0u:return e.language_7d2698;
        case 0x4872e0u:return m.u32(0x82e7d8u);
        case 0x43f910u:return std::uint32_t(st.mode_countdown_780250);
        case 0x43f920u:st.mode_countdown_780250-=std::int32_t(a[0]);return std::uint32_t(st.mode_countdown_780250);
        case 0x48b180u:return c.event_function36.car_select.transmission_830374;
        case 0x48b1a0u:return st.vehicle_variant_83036d;
        case 0x4954f0u:return st.frontend_prepare.alternate_code_836174;   // mov eax,[836174] (4EEC80 / 495480)
        case 0x457840u:case 0x456d70u:case 0x45a0b0u:case 0x456e10u:case 0x456d40u:case 0x45a2b0u:case 0x457a30u:case 0x457a40u:{
            // The LAN race's CommRace services (goal 457840, ranks 45A0B0 / 45A2B0, the
            // 456xxx player tests): the network module's (its memory holds the sessions).
            std::uint32_t eax=0;
            if(!native_network_u32(0x7d68acu)||!native_network_invoke(c,k.pc,k.ecx,{a[0],a[1]},&eax))
                throw EndUnported(k.pc,"variant 3/4 goal service: no LAN session");
            return eax;}
        case 0x4956c0u:native_race_variant8_goal_4956c0(c);return 0u;   // variant 8 goal record (race_variant_owners)
        // Records of 49CFD0 (7B17F8 thiscall, 48B430 for the Time Attack
        // variant 7; their 4480B0/447DC0 bodies are
        // VM black boxes) and the TA ghost save 467880/467960/47EF10
        // (4168D0 VM): not ported, counted in the status.
        case 0x447750u:if(k.ecx!=0x7b17f8u)throw EndUnported(k.pc,"record table on another object");   // OutRun (goal + 5*arcade)
            ++e.records_written;return record_insert_447750(m,k.ecx,0,(a[0]&0xffu?5u:0u)+(a[1]&0xffu),a[2],a[3],a[4]&0xffu,a[5]&0xffu,a[6],a[7]);
        case 0x4478f0u:if(k.ecx!=0x7b17f8u)throw EndUnported(k.pc,"record table on another object");   // OutRun2SP-style arcade table
            ++e.records_written;return record_insert_447750(m,k.ecx,0xc80u,a[0]&0xffu?1u:0u,a[1],a[2],a[3]&0xffu,a[4]&0xffu,a[5],0);
        case 0x447a60u:if(k.ecx!=0x7b17f8u)throw EndUnported(k.pc,"record table on another object");   // Heart Attack (hearts)
            ++e.records_written;return record_insert_447750(m,k.ecx,0xf00u,(a[0]&0xffu?5u:0u)+(a[1]&0xffu),a[2],a[3],a[4]&0xffu,a[5]&0xffu,a[6],a[7]);
        case 0x447c10u:if(k.ecx!=0x7b17f8u)throw EndUnported(k.pc,"record table on another object");   // variant 9
            ++e.records_written;return record_insert_447750(m,k.ecx,0x1b80u,(a[0]&0xffu?5u:0u)+(a[1]&0xffu),a[2],a[3],a[4]&0xffu,a[5]&0xffu,a[6],a[7]);
        case 0x48b430u:{                                           // Time Attack records (variant 7)
            const std::uint32_t code=st.frontend_prepare.output_code_656234;
            if(std::int32_t(code)<0x3c)return 0u;
            const std::uint32_t course=std::uint32_t(std::int32_t(std::int8_t(c.start_mode.course_choice_655b59)));   // 48B140
            const std::uint32_t a=c.event_function36.car_select.transmission_830374==1u?1u:0u;                     // 48B180
            const std::uint32_t b=st.vehicle_variant_83036d==1u?1u:0u;                                              // 48B1A0
            const std::uint32_t arcade=(st.course_preset==0u||st.course_preset==2u)?1u:0u;
            std::uint32_t laps[15]{},eax{};
            for(std::uint32_t i=0;i<15;++i){if(!native_race_manager_call(c,0x4505a0u,&i,1,eax,false))throw EndUnported(0x4505a0u,"lap time");laps[i]=eax;}
            const std::uint32_t zero=0;if(!native_race_manager_call(c,0x451180u,&zero,1,eax,false))throw EndUnported(0x451180u,"total time");
            const std::uint32_t time=eax;
            ++e.records_written;
            if(code>=0x50u){const std::uint32_t flag=(code==0x51u||code==0x53u)?1u:0u;                             // 4480B0
                return record_insert_time(m,0x7b17f8u+0x9154u+(arcade+flag*2u)*0x370u,0x58u,time,laps,15,a,b,course,0x54u);}
            return record_insert_time(m,0x7b17f8u+0x2804u+(code-0x3cu)*0x1e0u,0x30u,time,laps,5,a,b,course,0x2cu);   // 447DC0
        }
        case 0x467880u:(void)native_ghost_save_467880(c);return 0u;
        case 0x467960u:(void)native_ghost_save_467960(c);return 0u;
        case 0x47ef10u:(void)native_ghost_save_close_416830(c);return 0u;
        // camera override script 486730 (goal scripts; the others are reported)
        case 0x440bb0u:driving::change_ctrl_func_440bb0(c.event_state,a[0],a[1]);return 0u;
        case 0x49a650u:return 0u;                                  // RET
        case 0x46c3f0u:return c.game_mode.course_runtime.goal_word_7f95ac;   // mov eax,[7F95AC] (written by the goal side test 451514)
        case 0x48b320u:return st.frontend_prepare.output_code_656234;
        case 0x495490u:return st.route_gate_8361b4;
        case 0x46c400u:c.game_mode.course_runtime.goal_word_7f95ac=a[0];return a[0];   // mov [7F95AC],arg
        case 0x48b1e0u:return c.event_function36.music_globals.track_830364;   // mov al,[830364]
        case 0x427aa0u:{
            // 427AA0 (VM 447B0F reads [79FCC7], event 383 SOUND's flags):
            // base = running ? [9560F0] : 0.9 (6280F0), plus the per-track
            // offset 624224[t] (t 22h..30h) / 6242A8[t] (t 1..Fh), the same
            // fifteen values; clamped to 0..1 (619A34 / 62806C). Single
            // precision (PC24); the caller stores it with fstp dword.
            static constexpr float kOffset[16]{0.f,0.f,0.f,0.f,-0.15f,-0.2f,-0.2f,-0.25f,-0.25f,-0.25f,-0.25f,-0.3f,-0.3f,-0.3f,-0.3f,0.f};
            float v=(c.event_state.slots[383].flags&3u)==2u?c.race.sound.state.f32(0x9560f0u):0.9f;
            const std::int32_t t=std::int32_t(a[0]);
            if(t>=0x22&&t<=0x30)v+=kOffset[t-0x21];else if(t>=1&&t<=0xf)v+=kOffset[t];
            if(v<0.f)v=0.f;else if(v>1.f)v=1.f;   // fcom 0: test ah,5 / jp; fcom 1: test ah,41 / jne (NaN kept)
            std::uint32_t bits;std::memcpy(&bits,&v,4);return bits;}
        case 0x401050u:{auto& f=c.event_function36;++f.music_stop_requests;if(f.music_stop)(void)f.music_stop(f.music_user,0u);return 0u;}   // [95B24C] closed
        // goal camera script services (race_goal_camera op 13 / 21 / 22 / 27)
        case 0x41fb10u:case 0x41c420u:   // op 21 calls both, in this order: one reset of the particles
            if(k.pc==0x41fb10u&&!native_race_effects_tire_reset(c,matrices))throw EndUnported(0x41fb10u,"41FB10: "+c.race_effects.particles_fault);
            return 0u;
        case 0x4a7d70u:{   // 4A7D70(event, steps): the car's velocities and angles cleared, [841F70+event*4] = 0, 4A7C50 steps+1 times
            const std::uint32_t car=m.u32(0x799b38u+a[0]*0x3cu);
            for(std::uint32_t o:{0x14u,0x18u,0x1cu,0x16cu,0x170u,0x174u,0x178u,0x1c4u})m.putf(car+o,0.0f);
            for(std::uint32_t o:{0x30u,0x2eu,0x2cu})m.put16(car+o,0);
            m.put32(0x841f70u+a[0]*4u,0);
            for(std::uint32_t i=a[1]+1u;i!=0u;--i){
                PcRaceCall s{};s.pc=0x4a7c50u;s.argc=1;s.args[0]=car;
                if(!bulk_translated(s.pc))throw EndUnported(0x4a7c50u,"4A7C50 neither native nor translated");
                (void)bulk_call(m,ctx.service,s,&matrices,&c.event_function36.pc_crt_random_state,lan_network_module(c));
            }
            return 0u;}
        case 0x49f7c0u:m.put32(0x841b20u+a[0]*4u,a[1]);return 0u;   // 49F7C0(i, v): [841B20+i*4] = v
        case 0x49f7d0u:   // 49F7D0(event, record): 47F260(event, event - 8, 0, 83DB70 + record * 0xFD4)
            records_bind_47f260(m,a[0],a[0]-8u,0u,0x83db70u+a[1]*0xfd4u);return 0u;
        case 0x487b70u:race_robot_motion_connect_487b70(ctx,a[0],a[1],a[2]);return 0u;
        // GAD_PUB displays (4998C0)
        case 0x429530u:{
            if(!e.hud_draws&&e.arcade_control){RaceHudDraw d;d.pc=k.pc;for(std::uint32_t i=0;i<5;++i)d.args[i]=a[i];e.control_draws.push_back(d);return 0u;}
            if(!e.hud_draws)throw EndUnported(0x429530u,"HUD draw outside a display");
            RaceHudDraw d;d.pc=k.pc;for(std::uint32_t i=0;i<5;++i)d.args[i]=a[i];e.hud_draws->push_back(d);return 0u;}
        case 0x580f92u:return 0u;   // printf (debug output, 49D21E)
        case 0x457940u:{
            // 457940 (LAN race end, modes 21 / 22): this player's CommRace record (7DE418 + slot *
            // 0x6C) +2C / +28 / +58 = the clock 7F1938, +5C = 1, +40 (when 0) = 456790 + 4505D0,
            // then 4F5F00(+44, 0, +40): the finish message to the session (the network set).
            const std::uint32_t o=std::uint32_t(m.u8(0x7dd138u))*0x6cu,clock=m.u32(0x7f1938u);
            m.put32(0x7de444u+o,clock);m.put32(0x7de440u+o,clock);m.put32(0x7de470u+o,clock);
            std::uint32_t remaining=0;
            if(!native_race_network_call(c,matrices,0x456790u,{},remaining,0x7de418u))throw EndUnported(0x456790u,"457940: CommRace 456790: "+native_race_traffic(c).error);
            const std::uint32_t t=remaining+ctx.service(PcRaceCall{0x4505d0u});
            m.put8(0x7de474u+o,1u);
            if(m.u32(0x7de458u+o)==0u)m.put32(0x7de458u+o,t);
            if(!native_network_invoke(c,0x4f5f00u,0,{m.u32(0x7de45cu+o),0u,m.u32(0x7de458u+o)}))
                throw EndUnported(0x4f5f00u,"457940: finish message 4F5F00: "+native_network_stats().last_error);
            return 0u;}
        case 0x49a4f0u:race_close_49a4f0(ctx);return 0u;   // mode 21 / 22 exit 49D2E0: jmp 49A4F0
        case 0x45aef0u:   // = adv_release_45af40 (mode 21 init tail): 7F1964 held -> 440CD0, which the race end does not own
            if(m.u32(0x7f1964u))throw EndUnported(0x440cd0u,"attract data 7F1964 held (45AE40 loader not ported)");
            m.put32(0x7f195cu,0);m.put32(0x7f1960u,2u);m.put32(0x7f196cu,0xfu);return 0u;
        case 0x4b8e20u:{std::uint32_t z=0;std::memcpy(hud.navi.g842800.data()+(0x8447f8u-0x842800u),&z,4);return 0u;}   // VM: mov [8447F8],0
        case 0x456d60u:return c.race.car_world.commrace_7de418[0];
        case 0x4bb7c0u:return 0u;   // the 2C00E0 / 2C00E1 marks of the players with 455B60 != 0: 455B60 is always 0, nothing is drawn
        // the C2C request manager 7F9460 (race end mode 5, 49947B / 498D53; its block and heap are
        // mapped here): 46C440 = 481760 ([7F9460+5808]), 46C420 = 5000E0(0) then 4FE290, 46C220 =
        // 481A40 (the GOAL results), run as the translated originals until their native port
        case 0x46c440u:case 0x46c420u:case 0x46c220u:{
            auto run=[&](std::uint32_t pc,std::initializer_list<std::uint32_t> args)->std::uint32_t{
                PcRaceCall s{};s.pc=pc;s.ecx=0x7f9460u;for(auto v:args)s.args[s.argc++]=v;
                if(!bulk_translated(pc))throw EndUnported(pc,"7F9460 manager method neither native nor translated");
                return bulk_call(m,ctx.service,s,&matrices,&c.event_function36.pc_crt_random_state);};
            if(k.pc==0x46c440u)return run(0x481760u,{});
            if(k.pc==0x46c220u)return run(0x481a40u,{});
            (void)run(0x5000e0u,{0u});return run(0x4fe290u,{});}
        case 0x45bd30u:return native_race_hud_voice_45bd30(c,std::int32_t(a[0]),std::int16_t(a[1]),std::int16_t(a[2]));
        case 0x49a060u:case 0x497900u:case 0x48c5f0u:
            throw EndUnported(k.pc,"GAD_PUB display not ported");
        case 0x4ef710u:return (st.course_preset==2u||st.course_preset==3u)?14u:4u;
        case 0x450560u:return manager(k.pc,k);
        case 0x44c8d0u:return race_area_record_44c8d0(m,a[0]);   // AREA course records 7D33BC
        case 0x4b0100u:return native_race_attack_value(c,k.pc,a,k.argc);   // variant 9 rank of a stage (686260)
        case 0x42d280u:case 0x42d200u:{
            if(!e.hud_draws)throw EndUnported(k.pc,"2D image outside a display");
            RaceHudDraw d;d.pc=k.pc;for(std::uint32_t i=0;i<7;++i)d.args[i]=a[i];e.hud_draws->push_back(d);return 0u;}
        case 0x47ee70u:{   // the variant-0 stage flags into the caller's buffer (the words 47EE70 writes)
            std::array<std::uint32_t,32> flags{};std::array<bool,32> written{};
            if(!native_records_stage_flags_47ee70(c,flags,written))throw EndUnported(0x47ee70u,"47EE70: "+native_race_ghosts(c).error);
            for(std::uint32_t i=0;i<32u;++i)if(written[i])m.put32(a[0]+i*4u,flags[i]);
            return 0u;}
        // GOAL display 498670 services
        case 0x43fa00u:return c.frame_state.updates_95af48;      // [780278] (43FA10 of 417C7B)
        case 0x4b8e30u:std::memcpy(hud.navi.g842800.data()+(0x8446ecu-0x842800u),&a[0],4);return 0u;
        case 0x4bea50u:
            if(std::int32_t(race_sector_banner_450670(c.race.manager.state))<0x78)return 0u;
            {   // 4BE020 / 4BE150 on the NAVI module's globals (844800..844A00) and the race manager block
                if(!e.hud_draws)throw EndUnported(0x4bea50u,"sector banners outside a display");
                const auto mark=m.mark();
                m.map(0x844800u,hud.navi.g842800.data()+(0x844800u-0x842800u),0x200u);
                m.map(RaceManagerState::base,reinterpret_cast<std::uint8_t*>(&c.race.manager.state),sizeof(RaceManagerState));
                NaviPubServices ns;ns.m=&m;ns.sprites=&c.event_function36.frontend_sprites;ns.pause_95b214=c.event_function36.title_pause_flag_95b214;
                ns.draws=e.hud_draws;
                const bool ok=navi_sector_banners_4bea50(ns);
                m.release(mark);
                if(!ok)throw EndUnported(ns.missing?ns.missing:0x4be020u,"sector banners 4BE020/4BE150");
            }
            return 1u;
        case 0x465f70u:case 0x466190u:{                           // ghost record present / its time
            PcRaceMemory gm;native_race_ghosts(c).map(gm);
            const std::uint32_t base=gm.u32(0x7f9224u);
            if(k.pc==0x465f70u)return base&&gm.u32(base+a[0]*0x9cacu)==0x544f484eu?1u:0u;
            const std::uint32_t rec=a[0]*0x9cacu+base;
            return gm.u32(rec)==0x544f484eu?gm.u32(rec+4u):0xffffffffu;}
        case 0x476710u:{                                          // racer record of a ranking slot (80FB04, 80FB20)
            const auto& rs=c.mission.racers;const std::int32_t i=std::int32_t(a[0]);
            if(i<0||i>std::int32_t(rs.count_80fb04))return 0u;
            const std::size_t at=std::size_t(i)*0x1cu;
            if(at+0x1cu>rs.table_80fb20.size())throw EndUnported(0x476710u,"racer table 80FB20 shorter than its count");
            return rs.table_80fb20[at+0x10u]==0xffu?0u:RacerTableBase+std::uint32_t(at);}
        case 0x42cce0u:{                                          // printf text, monospace (42C390, 42C720 / 42C5A0)
            if(!e.text.font)throw EndUnported(0x42cce0u,"race text without a font");
            if(!e.hud_glyphs)throw EndUnported(0x42cce0u,"race text outside a display");
            const std::string text=format_42cce0(m,a[0],a[1]);
            {   // 42C390: the alignment of 956BD8 (right / centred by the 42C480 width, space 0.3; a half
                // or a whole line up), as the proportional text does
                auto& cur=e.text.cursor;const auto& st=e.text.style;
                auto wrap16=[](int v){return std::int16_t(std::uint16_t(std::uint32_t(v)));};
                if(!(st.flags&1u)){const int w=frontend_text_width_42c480(*e.text.font,st,text,0.3f);cur.x=wrap16(cur.x-((st.flags&4u)?w/2:w));}
                if(st.flags&8u)cur.y=wrap16(cur.y+int(driving::x87_ftol64(driving::X87(int(e.text.font->height))*driving::X87(-0.5f))));
                else if(st.flags&0x10u)cur.y=wrap16(cur.y-int(e.text.font->height));
            }
            const std::size_t first=e.hud_glyphs->size();
            for(unsigned char ch:text){
                // a line feed too goes through 42C720 first, then 42CD50 returns to the line start
                FrontendGlyph g;
                if(frontend_text_glyph_42c720(*e.text.font,e.text.style,e.text.cursor,ch,g))e.hud_glyphs->push_back(g);
                frontend_text_advance_42c5a0(*e.text.font,e.text.style,e.text.cursor,ch);
            }
            mark_glyphs(e,first);
            e.text.style.flags=1u;return 0u;}                       // 956BD8 = 1
        case 0x465eb0u:{                                          // localized text: copied to the mapped string slot
            const auto* table=c.event_function36.frontend_text;
            const std::string* s=table?table->get(a[0]):nullptr;
            if(!s)throw EndUnported(0x465eb0u,"text "+hex(a[0])+" not loaded");
            const std::size_t n=std::min<std::size_t>(s->size(),e.text_465eb0.size()-1u);
            std::memcpy(e.text_465eb0.data(),s->data(),n);e.text_465eb0[n]=0;
            return NativeRaceEnd::TextBase;}
        case 0x4289b0u:{                                          // sprite with a matrix (token, layer, &matrix, frame)
            if(!e.hud_draws)throw EndUnported(0x4289b0u,"sprite draw outside a display");
            RaceHudDraw d;d.pc=k.pc;d.args[0]=a[0];d.args[1]=a[1];d.args[3]=a[3];
            if(a[2])for(std::uint32_t w=0;w<16;++w)d.matrix[w]=m.f32(a[2]+w*4);
            else d.args[4]=1u;                                    // NULL matrix: 428A10 keeps the current one
            e.hud_draws->push_back(d);return 0u;}
        case 0x428840u:{                                          // the sprite's frame (x87 float result)
            const auto* sp=pool.get(a[0]);const float f=sp?sp->frame:0.0f;
            std::uint32_t bits;std::memcpy(&bits,&f,4);return bits;}
        case 0x45b830u:return hud.navi.g7f1900.size()>0x7f23c4u-0x7f1900u+3u?
            std::uint32_t(hud.navi.g7f1900[0x7f23c4u-0x7f1900u])|std::uint32_t(hud.navi.g7f1900[0x7f23c5u-0x7f1900u])<<8|
            std::uint32_t(hud.navi.g7f1900[0x7f23c6u-0x7f1900u])<<16|std::uint32_t(hud.navi.g7f1900[0x7f23c7u-0x7f1900u])<<24:0u;   // mov eax,[7F23C4]
        case 0x45b840u:return 0x2710u;                             // mov eax,10000
        // The name entry's saves. 480D00 / 481180: a ghost/record module fault stays latched
        // there (its status line); the race end goes on.
        case 0x480d00u:(void)native_records_store_480d00(c);return 0u;
        case 0x481180u:(void)native_records_store_481180(c,a[0]);return 0u;
        case 0x4165f0u:{                                          // 406C50("rankings.dat", [63960C] = 453070(), 0x2C90); al = 1
            const auto& dir=c.event_function36.frontend_save_directory;
            if(!dir.empty()){
                const std::uint32_t size=PcRaceEndState::BookSize;
                const std::uint8_t* book=m.at(m.u32(0x63960cu),size);
                if(std::FILE* f=std::fopen((dir+"/rankings.dat").c_str(),"wb")){   // 406C50: u32 length, then the payload
                    const std::uint8_t head[4]{std::uint8_t(size),std::uint8_t(size>>8),std::uint8_t(size>>16),std::uint8_t(size>>24)};
                    const bool ok=std::fwrite(head,1,4,f)==4&&std::fwrite(book,1,size,f)==size;
                    if(std::fclose(f)==0&&ok)++e.rankings_saves;
                }
            }
            return 1u;}
        case 0x42c370u:{                                          // the text's width (42C480, space 0.3), as the frontend text leaves
            if(!e.text.font)throw EndUnported(0x42c370u,"race text width without a font");
            std::string s;for(std::uint32_t p=a[0];s.size()<0xffu;++p){const auto ch=m.u8(p);if(!ch)break;s+=char(ch);}
            return std::uint32_t(frontend_text_width_42c480(*e.text.font,e.text.style,s,0.3f));}
        case 0x42cdd0u:{                                          // printf text, proportional (42C860 / 42C610)
            if(!e.text.font)throw EndUnported(0x42cdd0u,"race text without a font");
            if(!e.hud_glyphs)throw EndUnported(0x42cdd0u,"race text outside a display");
            std::string s=guest_format(m,a,k.argc,1u);   // printf(format, ...) into 956CA0
            const std::size_t first=e.hud_glyphs->size();
            if(!frontend_text_draw(*e.text.font,e.text.style,e.text.cursor,s,*e.hud_glyphs))throw EndUnported(0x42cdd0u,"race text outside the font");
            mark_glyphs(e,first);
            e.text.style.flags=1u;return 0u;}                       // 956BD8 = 1
        case 0x45bf30u:return navi_score_mean_45bf30(m,a[0]);   // the NAVI stage score mean (7F2560 block)
        case 0x4294c0u:{   // 4294C0(token, &w, &h): half the root component size; -1 without a scene
            FrontendSpriteTiming sc{};
            if(!pool.bank_scene(a[0],sc))return 0xffffffffu;
            m.putf(a[1],float(std::int16_t(sc.width))*0.5f);m.putf(a[2],float(std::int16_t(sc.height))*0.5f);return 0u;}
        case 0x4b9c80u:{   // 4B9C80(on): the 2D0000 sprite (layer 9, mode 3) at [842C14] while [8446C8] == 1 (NAVI globals)
            auto word=[&](std::uint32_t at)->std::uint8_t*{return hud.navi.g842800.data()+(at-0x842800u);};
            std::uint32_t state,handle;std::memcpy(&state,word(0x8446c8u),4);std::memcpy(&handle,word(0x842c14u),4);
            if(state==1u){
                if(a[0]!=0u)return 0u;
                if(handle<FrontendSprites::Count)pool.release(handle);
                state=0;std::memcpy(word(0x8446c8u),&state,4);return 0u;
            }
            if(a[0]!=1u)return 0u;
            ensure_bank(0x2d0000u);handle=pool.create(0x2d0000u,9u,3u);state=1;
            std::memcpy(word(0x842c14u),&handle,4);std::memcpy(word(0x8446c8u),&state,4);return 0u;}
        // in-race text (956BA0..)
        case 0x42ca60u:{
            const auto* fonts=c.event_function36.frontend_fonts;
            if(!fonts||a[0]>=10u||fonts->fonts[a[0]].token!=a[0])throw EndUnported(0x42ca60u,"race text: font not loaded");
            e.text.font=&fonts->fonts[a[0]];e.text.style.scale_x=1.0f;e.text.style.scale_y=1.0f;
            e.text.style.color=0xffffffffu;e.text.style.flags=1u;return 0u;}
        case 0x42cc60u:{                                          // scale unless 0.0 ([619A34])
            float x,y;std::memcpy(&x,&a[0],4);std::memcpy(&y,&a[1],4);
            if(!(x==0.0f))e.text.style.scale_x=x;
            if(!(y==0.0f))e.text.style.scale_y=y;
            return 0u;}
        case 0x42cca0u:e.text.style.color=a[0];return 0u;
        case 0x42ccb0u:e.text.style.mode=a[0];return 0u;
        case 0x42cc00u:
            e.text.cursor.x=std::int16_t(a[0]);e.text.cursor.y=std::int16_t(a[1]);
            e.text.cursor.origin_x=e.text.cursor.x;e.text.base_y=e.text.cursor.y;return 0u;
        case 0x42ccc0u:{
            if(!e.text.font)throw EndUnported(0x42ccc0u,"race text without a font");
            if(!e.hud_glyphs)throw EndUnported(0x42ccc0u,"race text outside a display");
            FrontendGlyph g;const std::size_t first=e.hud_glyphs->size();
            if(frontend_text_glyph_42c720(*e.text.font,e.text.style,e.text.cursor,std::uint8_t(a[0]),g))e.hud_glyphs->push_back(g);
            frontend_text_advance_42c5a0(*e.text.font,e.text.style,e.text.cursor,std::uint8_t(a[0]));
            mark_glyphs(e,first);
            return 0u;}
        case 0x48b140u:return c.start_mode.course_choice_655b59;
        case 0x48b160u:return c.start_mode.vehicle_colour_655b5a;
#include "platform/arcade_services.inc"
        default:{   // the race manager's accessors (race_manager_runtime), else not ported
            std::uint32_t eax{};
            if(native_race_manager_call(c,k.pc,k.args.data(),k.argc,eax,false))return eax;
            {auto& t=native_race_traffic(c);   // CRT new / delete of the request manager's code (variant 5)
             if(t.requests_heap&&translated_crt_call(m,*t.requests_heap,k,eax))return eax;}
            // the translated original (bulk_tr.cpp) over this memory, its ported callees here
            if(bulk_translated(k.pc))return bulk_call(m,ctx.service,k,&matrices,&c.event_function36.pc_crt_random_state,lan_network_module(c));
            throw EndUnported(k.pc,"race end: PC service not ported");}
        }
    },e.scene_draws};
    struct Back{RaceCameraOverride& o;const std::uint8_t* b;~Back(){override_out(o,b);}} back{c.race.camera_override,override_82e7c0.data()};
    if(soft_fault){
        try{body(ctx);return true;}
        catch(const EndUnported& x){*soft_fault=x.pc;}
        catch(const PcRaceUnmapped& x){*soft_fault=x.address;}
        catch(const PcRaceEndUndefined& x){*soft_fault=x.pc;}
        catch(const PcRaceEndUnreachable& x){*soft_fault=x.pc;}
        catch(const std::exception&){*soft_fault=0x4998c0u;}
        return false;
    }
    try{body(ctx);return true;}
    catch(const EndUnported& x){e.fault=x.pc;e.error=x.what();}
    catch(const PcRaceEndUndefined& x){e.fault=x.pc;e.error="PC reads an uninitialised stack slot at "+hex(x.pc);}
    catch(const PcRaceEndUnreachable& x){e.fault=x.pc;e.error="PC path no PC code reaches, taken at "+hex(x.pc);}
    catch(const PcRaceUnmapped& x){e.fault=x.address;e.error=std::string("race end: unmapped PC address: ")+x.what();}
    catch(const std::exception& x){e.fault=0x49a1e0u;e.error=x.what();}
    std::fprintf(stderr,"race end latched: %s\n",e.error.c_str());
    return false;
}
}
bool native_sprani_bind_bank(NativeRuntimeContext& c,std::uint32_t bank,std::string& error){return bind_bank(c,bank,error);}
namespace {
// LAN ratings (498410 and its 4EExxx helpers). A rating (12 bytes): +0 the rating, +4 the
// races counted, +8 its grade (the first of the 24 thresholds 5CED30 above it, -1 past them).
struct LanRating {
    PcRaceMemory& m;
    std::uint32_t grade_of(float r){
        for(std::uint32_t k=0;k<0x18u;++k)if(m.f32(0x5ced30u+k*4u)>r)return k;
        return 0xffffffffu;
    }
    // the rating's step factor by its band (5CEDF4 / 5CEDF0 thresholds)
    float factor(float r){
        if(m.f32(0x5cedf4u)>r)return m.f32(0x5b0424u);
        if(m.f32(0x5cedf0u)>r)return m.f32(0x5b4440u);
        return m.f32(0x5b018cu);
    }
    // 4EF1E0(ecx = rating, value): set (a zero step, clamped at 0) and graded.
    void set_4ef1e0(float* r,std::uint32_t* grade,float v){
        *r=v;
        const float s=factor(v)*0.0f+v;
        *r=s<0.0f?0.0f:s;
        *grade=grade_of(*r);
    }
    // 4EEF70(pairs, n): the Elo update: expected 1 / (1 + 10^((Rj - Ri) * 0.0025)), score 1 /
    // 0.5 / 0 by the ranks, each rating moved by its band's factor * (score - expected).
    void update_4eef70(std::vector<float*>& r,std::vector<std::uint32_t*>& games,std::vector<std::uint32_t*>& grade,const std::vector<std::uint32_t>& rank){
        const std::size_t n=r.size();
        if(n<=1u)return;
        std::vector<float> delta(n);
        for(std::size_t i=0;i<n;++i){
            float expect=0.0f,score=0.0f;
            for(std::size_t j=0;j<n;++j){
                if(j==i)continue;
                const long double d=(static_cast<long double>(*r[j])-*r[i])*static_cast<long double>(m.f32(0x5cede0u));
                const double p=std::pow(10.0,static_cast<double>(d));                                  // 5826D0 _CIpow
                const float e=static_cast<float>(1.0L/(1.0L+static_cast<long double>(p)));
                const std::int32_t c=std::int32_t(rank[j]-rank[i]);
                expect+=e;
                score+=c>0?1.0f:(c<0?0.0f:0.5f);
            }
            delta[i]=score-expect;
        }
        for(std::size_t i=0;i<n;++i){
            *games[i]+=std::uint32_t(n-1u);
            float v=factor(*r[i])*delta[i]+*r[i];
            *r[i]=v<0.0f?0.0f:v;
            *grade[i]=grade_of(*r[i]);
        }
    }
};
}
// 498410 (a LAN race's results, once): every player's rating (CommRace +20, the local one the
// licence 7C24F0) updated by the finish ranks (45A0B0), the new ratings stored in CommRace +24,
// the licence's change into 84BD00 (4EF410) and the licence regraded (7C27D4 bit 1).
void lan_ratings_498410(PcRaceContext& ctx){
    auto& m=ctx.m;
    if(!ctx.service(PcRaceCall{0x456d70u}))return;
    const std::uint32_t me=m.u8(m.u32(0x799d18u)+0x10u);
    const std::uint32_t n=ctx.service(PcRaceCall{0x456d60u})&0xffu;
    struct Local{float r{};std::uint32_t games{},grade{};};
    std::vector<Local> locals(n);
    LanRating lr{m};
    std::vector<float*> r(n);std::vector<std::uint32_t*> games(n),grade(n);std::vector<std::uint32_t> rank(n);
    for(std::uint32_t i=0;i<n;++i){
        lr.set_4ef1e0(&locals[i].r,&locals[i].grade,m.f32(0x5ced30u+9u*4u)-m.f32(0x5cedf8u));   // 4EF260 = 4EF150(9)
        locals[i].games=0;
    }
    float lic_r=m.f32(0x7c24f0u);std::uint32_t lic_games=m.u32(0x7c24f4u),lic_grade=m.u32(0x7c24f8u);
    for(std::uint32_t i=0;i<n;++i){
        if(i==me){r[i]=&lic_r;games[i]=&lic_games;grade[i]=&lic_grade;}
        else{
            r[i]=&locals[i].r;games[i]=&locals[i].games;grade[i]=&locals[i].grade;
            lr.set_4ef1e0(r[i],grade[i],m.f32(0x7de438u+i*0x6cu));                          // 457A30
        }
        PcRaceCall k{};k.pc=0x45a0b0u;k.argc=1;k.args[0]=i;
        rank[i]=ctx.service(k)&0xffu;
    }
    lr.update_4eef70(r,games,grade,rank);
    for(std::uint32_t i=0;i<n;++i)m.putf(0x7de43cu+i*0x6cu,*r[i]);                             // 457A40
    m.putf(0x7c24f0u,lic_r);m.put32(0x7c24f4u,lic_games);m.put32(0x7c24f8u,lic_grade);
    m.put32(0x83670cu,1);
    m.putf(0x84bd00u,lic_r-m.f32(0x7de438u+me*0x6cu));                                        // 4EF410(new - 457A30(me))
    m.put8(0x7c27d4u,m.u8(0x7c27d4u)|2u);
    float v=*r[me];std::uint32_t g=0;
    lr.set_4ef1e0(&v,&g,v);
    m.putf(0x7c24f0u,v);m.put32(0x7c24f8u,g);
}
// 4985E4 (GAD control 498590, modes 0x13 / 0x15 / 0x16 of a LAN race): with no [65A7A4] and the
// event-36 owner in state 1 (4035F0 -> 564C90), 498410 once ([83670C]: the LAN rating into
// 84BD00); the online upload 494140 ([7D68BC]) is not ported.
int native_race_end_lan_results_4985e4(NativeRuntimeContext& c,std::uint32_t& done_83670c){
    int result=-1;
    if(!run_end(c,[&](PcRaceContext& ctx){
        auto& m=ctx.m;
        if(m.u8(0x65a7a4u)){result=0;return;}
        const std::uint32_t owner=ctx.service(PcRaceCall{0x4035f0u});
        if(m.u32(owner+8u)!=1u){result=0;return;}
        if(!done_83670c){
            m.map(0x83670cu,reinterpret_cast<std::uint8_t*>(&done_83670c),4);
            lan_ratings_498410(ctx);
            if(native_network_u8(0x7d68bcu)&&done_83670c)throw EndUnported(0x49861eu,"online ranking upload 494140 not ported");
        }
        result=1;}))return -1;
    return result;
}
bool native_bulk_event_invoke(NativeRuntimeContext& c,std::uint32_t callback,std::uint32_t work,driving::PcMatrixStack& matrices){
    if(!bulk_translated(callback))return false;
    auto& e=native_race_end(c);
    if(e.bulk_event_faults.count(callback))return true;
    ++e.bulk_event_calls[callback];
    PcRaceCall k{};k.pc=callback;k.argc=1;k.args[0]=work;
    std::uint32_t fault=0;
    if(!run_end(c,[&](PcRaceContext& ctx){(void)bulk_call(ctx.m,ctx.service,k,&ctx.matrices,&c.event_function36.pc_crt_random_state);},&matrices,&fault)){
        e.bulk_event_faults[callback]=fault;
        std::fprintf(stderr,"bulk event callback %06X faulted at %08X (now skipped)\n",callback,fault);
    }
    return true;
}
// OutRun2SP button (key 1 fixed choice, cursor 2/3): 4165C0 loads rankings.dat
// (406DB0 existence test, 406E50: a u32 size header that must be 0x2C90, then
// up to 0x2C90 bytes into the book [63960C]) from the save directory, then
// 4F3CC0 copies the book into the SP ranking tables.
bool native_sp_rankings_service(NativeRuntimeContext& c,unsigned pc){
    auto& st=native_race_end(c).state;
    if(pc==0x4165c0u){
        const auto& dir=c.event_function36.frontend_save_directory;
        if(dir.empty())return true;                       // no save bank: the file does not exist
        const std::string path=dir+"/rankings.dat";
        if(std::FILE* f=std::fopen(path.c_str(),"rb")){
            std::uint8_t head[4]{};
            if(std::fread(head,1,4,f)==4&&(std::uint32_t(head[0])|std::uint32_t(head[1])<<8|std::uint32_t(head[2])<<16|std::uint32_t(head[3])<<24)==PcRaceEndState::BookSize)
                (void)std::fread(st.bookkeeping.data(),1,st.bookkeeping.size(),f);
            std::fclose(f);
        }
        return true;                                      // 4165C0 returns 1
    }
    if(pc==0x4f3cc0u){
        PcRaceMemory m;
        m.map(PcRaceEndState::BookBase,st.bookkeeping.data(),st.bookkeeping.size());
        m.map(PcRaceEndState::SpTablesBase,st.sp_tables.data(),st.sp_tables.size());
        std::uint32_t cell=PcRaceEndState::BookBase;m.map(0x63960cu,reinterpret_cast<std::uint8_t*>(&cell),4);
        sp_tables_from_book_4f3cc0(m);
        return true;
    }
    return false;
}
NativeRaceEnd& native_race_end(NativeRuntimeContext& c){
    if(!c.race_end)c.race_end=std::make_shared<NativeRaceEnd>();
    return *c.race_end;
}
bool native_race_end_mode_active(std::uint32_t mode){
    return mode==1u||mode==2u||mode==3u||mode==4u||mode==5u||mode==8u||mode==7u||mode==9u||mode==10u||mode==11u||mode==12u||mode==19u||mode==20u||mode==21u||mode==22u||mode==23u||mode==24u||mode==25u||mode==26u||mode==27u||mode==28u||mode==29u||mode==30u||mode==31u||mode==33u||mode==34u||mode==35u||mode==36u;
}
bool native_race_end_gad_display(NativeRuntimeContext& c,std::vector<RaceHudDraw>& draws,std::vector<FrontendGlyph>& glyphs,std::uint32_t& missing){
    auto& e=native_race_end(c);
    if(e.fault){missing=0x4998c0u;return false;}
    e.hud_draws=&draws;e.hud_glyphs=&glyphs;missing=0;
    const bool ok=run_end(c,[](PcRaceContext& ctx){gad_display_4998c0(ctx);},nullptr,&missing);
    e.hud_draws=nullptr;e.hud_glyphs=nullptr;
    return ok;
}
bool native_arcade_display_callback(std::uint32_t callback){
    switch(callback){
    case 0x4bf3f0u:case 0x4c1fa0u:case 0x4c2420u:case 0x4c22c0u:case 0x4c1e80u:case 0x4c22e0u:case 0x4bee80u:
    case 0x4b4f20u:case 0x4b4550u:case 0x4aef60u:case 0x4af100u:case 0x4af220u:case 0x4af280u:return true;                                    // event 0x189 (name entry) display
    case 0x498010u:return true;                                    // event 0x199 function 0x53: mode 36 reward text
    default:return false;
    }
}
namespace {
// 498010 (mode 36 reward text, stage [67EE50] == 2): a 48E590 text widget, font 3, 0x454 = 9,
// scale 1, colour -1, rect (-280, 0, 500, 1E0), +480/+484 = 104 / 1E; then each text record
// of 5C1E58 (+30, +3C) with an id: position (320, y), the localized text, 48F3C0.
bool reward_text_498010(NativeRuntimeContext& c,std::vector<FrontendGlyph>& glyphs,std::uint32_t& missing){
    auto& st=native_race_end(c).state;
    std::int32_t stage,reward;std::memcpy(&stage,st.stage_67ee50.data(),4);
    std::memcpy(&reward,native_race_hud(c).gad.data.data()+(0x67ee4cu-GadPubState::DataBase),4);
    if(stage!=2)return true;
    const auto* fonts=c.event_function36.frontend_fonts;const auto* table=c.event_function36.frontend_text;
    if(!fonts||!table){missing=0x498010u;return false;}
    const auto* r=mode36_reward_record(reward);if(!r){missing=0x498010u;return false;}
    std::array<std::uint8_t,PcTextWidgetBytes> widget{};driving::Bytes w(widget.data(),widget.size());
    frontend_text_init_48e640(w);
    w.put32(0x450,3u);w.put32(0x454,9u);w.putf(0x478,1.0f);w.putf(0x47c,1.0f);w.put32(0x474,0xffffffffu);
    const std::uint32_t rect[4]{0xfffffd80u,0u,0x500u,0x1e0u};(void)frontend_text_setter(0x48eee0u,w,rect,4);
    w.put32(0x480,0x104u);w.put32(0x484,0x1eu);
    for(unsigned k=0;k<2;++k){
        const std::uint32_t id=r[12+3*k],y=r[13+3*k];
        if(std::int32_t(id)<0)continue;
        w.putf(0x34,320.0f);w.putf(0x38,float(std::int32_t(y)));
        const std::string* text=table->get(id);if(!text){missing=0x465eb0u;return false;}
        if(!frontend_text_set_48f280(w,*text,w.u32(0x450),w.u32(0x474))){missing=0x48ee80u;return false;}
        FrontendTextLines lines;
        if(!frontend_text_display_48f3c0(w,*fonts,lines,glyphs)){missing=0x48f3c0u;return false;}
    }
    return true;
}
}
// Event function 0x1E (mode 31's letter-board name entry, event 0x18B): 4B2F00 init,
// 4B42C0 control, 4B4550 display (race_name_entry.cpp); destroy 4B0950 is the native
// name entry's.
void name_entry2_4b2f00(NativeRuntimeContext&,PcRaceContext& ctx,std::uint32_t callback){
    try{
        if(callback==0x4b2f00u)name_entry2_init_4b2f00(ctx);
        else if(callback==0x4b42c0u)name_entry2_control_4b42c0(ctx);
        else name_entry2_display_4b4550(ctx);
    }catch(const std::exception& x){
        dev_log("name entry 2 %x: %s",callback,x.what());
        throw;}
}
// Event 3 functions 0x2E / 0x2F / 0x30 / 0x31 / 0x49 (the logos of modes 1/2/5/8, which
// open them with 440110(3, fn)): race_modes.cpp, given the event's work.
void logos_event3(NativeRuntimeContext& c,PcRaceContext& ctx,std::uint32_t callback){
    const auto slot=c.event_state.current_slot;
    const std::uint32_t work=slot<c.event_state.slots.size()?c.event_state.slots[slot].work_token:0u;
    try{(void)race_logos_callback(ctx,callback,work);}
    catch(const std::exception& x){
        dev_log("logo %x: %s",callback,x.what());
        throw;}
}
bool native_race_end_arcade_display(NativeRuntimeContext& c,std::uint32_t callback,std::vector<RaceHudDraw>& draws,std::vector<FrontendGlyph>& glyphs,std::uint32_t& missing){
    if(callback==0x498010u){missing=0;return reward_text_498010(c,glyphs,missing);}
    auto& e=native_race_end(c);
    if(e.fault){missing=e.fault;return false;}
    draws.insert(draws.end(),e.control_draws.begin(),e.control_draws.end());e.control_draws.clear();
    e.hud_draws=&draws;e.hud_glyphs=&glyphs;missing=0;
    const bool ok=run_end(c,[&](PcRaceContext& ctx){
        if(callback==0x4b4f20u)name_entry_display_4b4f20(ctx);
        else if(callback==0x4b4550u)name_entry2_4b2f00(c,ctx,callback);
        else if(callback==0x4aef60u||callback==0x4af100u||callback==0x4af220u||callback==0x4af280u)logos_event3(c,ctx,callback);
        else (void)arcade_event4_invoke(ctx,callback,0x780440u);});
    e.hud_draws=nullptr;e.hud_glyphs=nullptr;
    if(!ok)missing=e.fault;
    return ok;
}
bool native_race_end_scene_display(NativeRuntimeContext& c,PcSceneRenderer& r,std::uint32_t callback){
    if(callback!=0x44b890u&&callback!=0x4522d0u)return false;
    auto& e=native_race_end(c);
    if(e.fault){++e.scene_display_skipped;return true;}
    std::vector<PcVehicleDrawCall> draws;
    e.scene_draws=&draws;
    std::uint32_t missing=0;
    const bool ok=run_end(c,[&](PcRaceContext& ctx){
        if(callback==0x44b890u)ending_area_display_44b890(ctx);else ending_background_4522d0(ctx);},&r.matrices(),&missing);
    e.scene_draws=nullptr;
    if(!ok){++e.scene_display_faults;e.scene_display_fault=missing;return true;}
    try{(void)native_race_draw_list_execute(c,r,draws);++e.scene_displays;}
    catch(const std::exception& x){++e.scene_display_faults;e.scene_display_error=x.what();}
    return true;
}
bool native_race_end_camera_4874a0(NativeRuntimeContext& c,driving::PcMatrixStack& matrices){
    ++native_race_end(c).camera_calls;
    return run_end(c,[](PcRaceContext& ctx){camera_override_control_4874a0(ctx);},&matrices);
}
std::uint32_t native_race_end_layer_callback(const NativeRuntimeContext& c,std::uint32_t layer){
    return c.race_end&&layer<c.race_end->callbacks_7d2620.size()?c.race_end->callbacks_7d2620[layer]:0u;
}
bool native_race_end_layer_invoke(NativeRuntimeContext& c,std::uint32_t callback){
    if(callback==0x49a650u)return true;                                       // RET
    if(callback!=0x4bfa20u||!c.race_end)return false;
    (void)run_end(c,[](PcRaceContext& ctx){arcade_layer8_4bfa20(ctx);});
    return true;
}
std::uint32_t native_race_end_84a318(const NativeRuntimeContext& c){
    if(!c.race_end)return 0u;
    const auto& b=c.race_end->state.arcade_84a208;std::uint32_t v;std::memcpy(&v,b.data()+(0x84a318u-0x84a208u),4);return v;
}
bool native_race_start_camera_4871a0(NativeRuntimeContext& c,std::uint32_t scene){
    ++native_race_end(c).start_camera_inits;
    return run_end(c,[scene](PcRaceContext& ctx){goal_camera_4871a0(ctx,scene);},nullptr);
}
void native_race_end_miles_init(NativeRuntimeContext& c){
    auto& e=native_race_end(c);
    if(e.miles_loaded)return;
    e.miles_loaded=true;
    (void)run_end(c,[](PcRaceContext& ctx){miles_init_4ef280(ctx);});
}
bool native_race_end_mode(NativeRuntimeContext& c,std::uint32_t mode,std::uint32_t token,NativeModePhase phase){
    void (*fn)(PcRaceContext&)=nullptr;
    switch(token){
    case 0x49a1e0u:fn=mode20_init_49a1e0;break;case 0x49a280u:fn=mode20_control_49a280;break;case 0x49d1e0u:fn=mode20_exit_49d1e0;break;
    case 0x49a2e0u:fn=mode21_init_49a2e0;break;case 0x49d210u:fn=mode21_control_49d210;break;
    case 0x49a370u:fn=mode22_init_49a370;break;case 0x49a400u:fn=mode22_control_49a400;break;
    case 0x49d2e0u:fn=race_close_49a4f0;break;                                // 49D2E0: jmp 49A4F0
    case 0x49a790u:fn=mode25_init_49a790;break;case 0x49a800u:fn=mode25_control_49a800;break;case 0x49a920u:fn=mode25_exit_49a920;break;
    case 0x49a660u:fn=mode24_init_49a660;break;case 0x49a710u:fn=mode24_control_49a710;break;case 0x49a780u:fn=mode24_exit_4527a0;break;   // 49A780: jmp 4527A0
    case 0x49a960u:fn=mode26_init_49a960;break;case 0x49aa60u:fn=mode26_control_49aa60;break;case 0x49aae0u:fn=mode26_exit_49aae0;break;
    case 0x49adc0u:fn=mode27_init_49adc0;break;case 0x49aea0u:fn=mode27_control_49aea0;break;case 0x49aed0u:fn=mode27_exit_49aed0;break;
    case 0x49b170u:fn=mode28_init_49b170;break;case 0x49af90u:fn=mode28_control_49af90;break;
    case 0x49b190u:fn=mode30_control_49b190;break;
    case 0x49a450u:fn=mode23_init_49a450;break;case 0x49a480u:fn=mode23_control_49a480;break;case 0x49a4f0u:fn=race_close_49a4f0;break;
    case 0x49b130u:fn=mode29_control_49b130;break;case 0x49b140u:fn=mode29_exit_49b140;break;
    case 0x49ab90u:fn=mode31_init_49ab90;break;case 0x49ac00u:fn=mode31_control_49ac00;break;case 0x49ac80u:fn=mode31_exit_49ac80;break;
    case 0x49b470u:fn=mode34_init_49b470;break;case 0x49b570u:fn=mode34_control_49b570;break;case 0x49b7c0u:fn=mode34_exit_49b7c0;break;
    case 0x4981a0u:fn=mode36_init_4981a0;break;case 0x4981f0u:fn=mode36_control_4981f0;break;case 0x4983d0u:fn=mode36_exit_4983d0;break;
    case 0x49b7e0u:fn=mode35_init_49b7e0;break;case 0x49d9c0u:fn=mode35_control_49d9c0;break;case 0x49dae0u:fn=mode35_exit_49dae0;break;
    case 0x49c9c0u:fn=mode19_init_49c9c0;break;case 0x49cc50u:fn=mode19_control_49cc50;break;case 0x49cfd0u:fn=mode19_exit_49cfd0;break;
    case 0x49d4a0u:fn=mode33_init_49d4a0;break;case 0x49d850u:fn=mode33_control_49d850;break;
    case 0x48ae40u:fn=mode7_init_48ae40;break;case 0x49f360u:fn=mode7_control_49f360;break;case 0x49f370u:fn=mode7_exit_49f370;break;
    case 0x48ae50u:fn=mode9_control_48ae50;break;
    case 0x48b210u:fn=mode10_init_48b210;break;case 0x48ae90u:fn=mode10_control_48ae90;break;case 0x48b070u:fn=mode10_exit_48b070;break;
    case 0x48b0b0u:fn=mode11_control_48b0b0;break;
    case 0x48b0f0u:fn=mode12_init_48b0f0;break;case 0x48b120u:fn=mode12_control_48b120;break;case 0x48b2a0u:fn=mode12_exit_48b2a0;break;
    // Modes 1/2/3/4/5/8 (boot logos, the arcade demo route): race_modes.cpp.
    case 0x49e790u:case 0x49e7b0u:case 0x49e810u:case 0x49e830u:case 0x49e8b0u:case 0x49ea10u:
    case 0x49ea20u:case 0x49eb80u:case 0x49ed10u:case 0x49efe0u:case 0x49f040u:case 0x49f1b0u:
    case 0x49f1d0u:case 0x49f1e0u:case 0x49f2d0u:case 0x49f2f0u:{
        ++native_race_end(c).mode_calls;
        (void)run_end(c,[&](PcRaceContext& ctx){
            // Offline: no network manager [7DF34C] and no network player records
            // (7DF100..7DF350 are .bss written only by the LAN session code).
            std::array<std::uint8_t,0x250> network{};const auto mark=ctx.m.mark();
            if(!c.start_mode.network_manager_7df34c_present)ctx.m.map(0x7df100u,network.data(),network.size());
            try{
                (void)race_modes_callback(ctx,token);
            }catch(const std::exception& x){
                dev_log("mode %u callback %x: %s",mode,token,x.what());
                ctx.m.release(mark);throw;}
            ctx.m.release(mark);});
        return true;}
    case 0x49a650u:return native_race_end_mode_active(mode);   // RET
    default:
        // Modes with no native callbacks (21 LAN TIME OVER 49A2E0 / 49D210 / 49D2E0...): the
        // bulk translation over this runtime's memory and services.
        if(!bulk_translated(token))return false;
        ++native_race_end(c).mode_calls;
        (void)run_end(c,[&](PcRaceContext& ctx){
            PcRaceCall k{};k.pc=token;
            (void)bulk_call(ctx.m,ctx.service,k,&ctx.matrices,&c.event_function36.pc_crt_random_state,lan_network_module(c));});
        return true;
    }
    (void)phase;
    ++native_race_end(c).mode_calls;
    (void)run_end(c,[&](PcRaceContext& ctx){fn(ctx);});
    return true;
}
bool native_race_end_event_invoke(NativeRuntimeContext& c,std::uint32_t callback){
    void (*fn)(PcRaceContext&)=nullptr;
    switch(callback){
    case 0x4ae750u:fn=route_init_4ae750;break;case 0x4ae960u:fn=route_control_4ae960;break;case 0x4aed20u:fn=route_destroy_4aed20;break;
    case 0x4aef50u:{                                                         // jmp 440B20: a work this runtime allocated
        const auto ev=c.event_state.current_slot;
        if(ev>=c.event_state.slots.size()||c.event_state.slots[ev].work_token<NativeRaceEnd::EventWorkBase)return false;   // course objects (race traffic)
        ++native_race_end(c).event_calls;
        (void)run_end(c,[&](PcRaceContext& ctx){logos_event3(c,ctx,callback);});
        return true;}
    case 0x4aef30u:case 0x4af090u:case 0x4aef80u:case 0x4af0e0u:case 0x4af120u:case 0x4af160u:case 0x4af1d0u:case 0x4af200u:
        ++native_race_end(c).event_calls;
        (void)run_end(c,[&](PcRaceContext& ctx){logos_event3(c,ctx,callback);});
        return true;
    case 0x4b2f00u:case 0x4b42c0u:
        ++native_race_end(c).event_calls;
        (void)run_end(c,[&](PcRaceContext& ctx){name_entry2_4b2f00(c,ctx,callback);});
        return true;
    case 0x4b2370u:fn=name_entry_init_4b2370;break;case 0x4b29e0u:fn=name_entry_control_4b29e0;break;case 0x4b0950u:fn=name_entry_destroy_4b0950;break;
    case 0x451d10u:fn=ending_lights_init_451d10;break;                       // event 0x187 function 0x45 init
    case 0x49f410u:case 0x49f4e0u:{                                           // event 8 function 0x2D (the ending car)
        const std::uint32_t car=c.event_state.slots[8].work_token;
        ++native_race_end(c).event_calls;
        (void)run_end(c,[&](PcRaceContext& ctx){
            if(car!=0x7804b0u)throw EndUnported(callback,"event 8 work is not the player car");
            if(callback==0x49f410u)ending_car_init_49f410(ctx,car);else ending_car_dest_49f4e0(ctx,car);});
        return true;}
    case 0x4aed50u:fn=ta_result_init_4aed50;break;case 0x4aeec0u:fn=ta_result_control_4aeec0;break;case 0x4aeee0u:fn=ta_result_destroy_4aeee0;break;
    default:
        if(native_arcade_display_callback(callback))return false;   // drawn by native_race_end_arcade_display
        {   // OUTRUN2SP arcade screens (event 4 functions 1..0xC)
            bool known=false;
            static constexpr std::uint32_t Calls[]{0x4bfe00u,0x4c3510u,0x4bff80u,0x4c3530u,0x4c2240u,0x4c3960u,0x4c0060u,0x4c0090u,0x4c01e0u,0x4c3dc0u,
                0x4bf4d0u,0x4c18c0u,0x4c2d20u,0x4bfdb0u,0x4c1f10u,0x4c30b0u,0x4c02b0u,0x4c02e0u,0x4c04d0u,0x4c4880u,0x4c2330u,0x4c26d0u,
                0x4c05f0u,0x4bf500u,0x4bf510u,0x4bfb90u,0x4c2b70u};
            for(auto k:Calls)known=known||k==callback;
            if(!known)return false;
            auto& e=native_race_end(c);++e.event_calls;
            e.arcade_control=true;   // 429530 draws of a control: queued for this frame's display
            (void)run_end(c,[&](PcRaceContext& ctx){(void)arcade_event4_invoke(ctx,callback,0x780440u);});
            e.arcade_control=false;
            return true;
        }
    }
    ++native_race_end(c).event_calls;
    (void)run_end(c,[&](PcRaceContext& ctx){fn(ctx);});
    return true;
}
std::string native_race_end_status(const NativeRuntimeContext& c){
    if(!c.race_end)return "race end: idle";
    const auto& e=*c.race_end;std::ostringstream s;
    s<<"race end: modes="<<e.mode_calls<<" events="<<e.event_calls<<" camera="<<e.camera_calls<<" start_camera="<<e.start_camera_inits<<" banks="<<e.bank_loads<<" matrices="<<e.sprite_matrices<<" records="<<e.records_written<<" rankings_saves="<<e.rankings_saves<<" rewards="<<e.mission_rewards_deferred<<"/"<<e.mission_reward_last;
    if(!e.released.empty()){s<<" released:";for(const auto& [pc,n]:e.released)s<<" "<<hex(pc)<<"x"<<n;}
    s<<" scene displays="<<e.scene_displays<<"/faults "<<e.scene_display_faults<<" "<<hex(e.scene_display_fault)<<" "<<e.scene_display_error;
    if(e.fault)s<<" FAULT "<<hex(e.fault)<<" "<<e.error;
    return s.str();
}
}
