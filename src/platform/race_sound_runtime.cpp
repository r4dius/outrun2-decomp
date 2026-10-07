#include "platform/pc_network.hpp"
#include "platform/race_ghosts_runtime.hpp"
#include "platform/race_traffic_runtime.hpp"
#include "platform/race_sound_runtime.hpp"
#include "platform/native_runtime.hpp"
#include "platform/race_area_runtime.hpp"
#include "driving/pc_common_control.hpp"
#include <cstdio>
#include <cstring>
#include <sstream>
#include <stdexcept>
namespace outrun::platform {
namespace {
std::string hex(std::uint32_t v){char t[16];std::snprintf(t,sizeof t,"%X",v);return t;}
// A service with no native answer: the callback is aborted and the sound
// manager latched (its later callbacks are skipped and counted).
struct SoundMissing : std::runtime_error {
    std::uint32_t pc;
    SoundMissing(std::uint32_t p,const std::string& why):std::runtime_error("PC "+hex(p)+": "+why),pc(p){}
};
struct Binding final : RaceSoundServices {
    NativeRuntimeContext& c;
    NativeRaceSound& s;
    PcRaceMemory& area;                       // AREA owner memory (7D2D80.. block, course records)
    Binding(NativeRuntimeContext& ctx,NativeRaceSound& snd,PcRaceMemory& a):c(ctx),s(snd),area(a){}
    void routed(std::uint32_t pc){++s.routed[pc];}
    // 44C8D0 over the area memory (count [7D33C4], records [7D33BC]).
    std::uint32_t find_stage(std::uint32_t key){
        const std::int32_t n=area.i32(0x7d33c4u);if(n<=0)return 0u;
        const std::uint32_t base=area.u32(0x7d33bcu);
        for(std::int32_t k=0;k<n;++k)if(area.u32(base+std::uint32_t(k)*0x78u+4u)==key)return base+std::uint32_t(k)*0x78u;
        return 0u;
    }
    // 44DC50: 44C8D0(key) ? [[rec+14]] : [[7D2DF4]].
    std::int32_t pc_44dc50(std::uint32_t key)override{
        routed(0x44dc50u);const auto rec=find_stage(key);
        return area.i32(area.u32(rec?rec+0x14u:0x7d2df4u));
    }
    // 44C940: cache 635F2C/635F30 (course_runtime), else [rec+8] or 0.
    std::uint32_t pc_44c940(std::uint32_t key)override{
        routed(0x44c940u);auto& rt=c.game_mode.course_runtime;
        if(key==std::uint32_t(rt.stage_key_635f2c))return std::uint32_t(rt.stage_value_635f30);
        const auto rec=find_stage(key);const std::uint32_t v=rec?area.u32(rec+8u):0u;
        rt.stage_key_635f2c=std::int32_t(key);rt.stage_value_635f30=std::int32_t(v);return v;
    }
    // 44DD60 / 44DD90: word +7C / +7E of the side (0: [rec+14], else [rec+18]) descriptor; 0 without a record.
    std::uint16_t side_word(std::uint32_t pc,std::uint32_t key,std::uint32_t side,std::uint32_t off){
        routed(pc);const auto rec=find_stage(key);if(!rec)return 0u;
        return area.u16(area.u32(rec+(side==0u?0x14u:0x18u))+off);
    }
    std::uint16_t pc_44dd60(std::uint32_t key,std::uint32_t side)override{return side_word(0x44dd60u,key,side,0x7cu);}
    std::uint16_t pc_44dd90(std::uint32_t key,std::uint32_t side)override{return side_word(0x44dd90u,key,side,0x7eu);}
    // 44B7B0 (bridge 447F7A: al = byte [7D33D0]), as the race manager binding.
    std::uint8_t pc_44b7b0(std::uint32_t arg)override{
        routed(0x44b7b0u);
        if(area.u8(0x7d33d0u)&&arg==0u)return 1u;
        const std::uint32_t sel=area.u32(0x7d3188u);
        if(area.i32(sel+0x2cu)==-1&&area.i32(sel+0x30u)==-1&&arg==0u){area.put8(0x7d33d0u,1u);return 1u;}
        return 0u;
    }
    std::uint32_t pc_44b800()override{routed(0x44b800u);return area.u32(0x7d30d4u)==0u&&area.u32(0x7d30d8u)==0u?1u:0u;}
    std::uint32_t pc_450570()override{routed(0x450570u);return c.race.manager.state.stage_frames_7d3930;}   // mov eax,[7D3930]
    std::uint8_t pc_48b310()override{routed(0x48b310u);return c.start_mode.flag_830394;}
    std::uint8_t pc_48b350()override{routed(0x48b350u);
        return driving::platform_counter_lt_60_48b350(std::int32_t(c.start_mode.frontend_prepare.output_code_656234))?1u:0u;}
    std::uint8_t pc_48b1c0()override{routed(0x48b1c0u);return s.option_83036c;}
    std::uint8_t pc_43f9c0()override{routed(0x43f9c0u);return c.start_mode.game_flag_780248;}
    std::uint32_t pc_55a930()override{routed(0x55a930u);return c.start_mode.manager_state_7f94c0!=0u?1u:0u;}
    void pc_42eff0(std::uint32_t channel,std::uint32_t value,std::uint32_t code)override{
        ++s.ics_updates;s.ics_last[(std::uint64_t(channel)<<32)|code]=value;
        c.pc_sound.ics_42eff0(channel,value,code);
    }
    void pc_42f0d0(std::uint32_t command)override{
        if(s.effect&&s.effect(command))++s.played[command];else ++s.refused[command];
    }
    void pc_427630()override{++s.clear_alls;c.pc_sound.clear_all_427630();if(s.stop_all)s.stop_all();}
};
void latch(NativeRaceSound& s,std::uint32_t pc,const std::string& why){
    s.last_error=why;
    if(!s.latched){s.latched=true;s.fault_pc=pc;}
}
// SE queue 9563E8 and cursors 9560C0/956124 <-> the player car's queue.
void queue_in(NativeRuntimeContext& c,RaceSoundState& st){
    const auto& w=c.race.car_world;
    for(std::uint32_t k=0;k<32u;++k){std::uint32_t v;std::memcpy(&v,w.sound_entries_9563e8.data()+k*4u,4);st.put32(RaceSoundState::se_queue_9563e8+k*4u,v);}
    std::uint32_t r,wr;std::memcpy(&r,w.sound_state.data(),4);std::memcpy(&wr,w.sound_state.data()+4,4);
    st.put32(RaceSoundState::se_read_9560c0,r);st.put32(RaceSoundState::se_write_956124,wr);
}
void queue_out(NativeRuntimeContext& c,const RaceSoundState& st){
    auto& w=c.race.car_world;
    for(std::uint32_t k=0;k<32u;++k){const auto v=st.u32(RaceSoundState::se_queue_9563e8+k*4u);std::memcpy(w.sound_entries_9563e8.data()+k*4u,&v,4);}
    const auto r=st.u32(RaceSoundState::se_read_9560c0),wr=st.u32(RaceSoundState::se_write_956124);
    std::memcpy(w.sound_state.data(),&r,4);std::memcpy(w.sound_state.data()+4,&wr,4);
}
}
void native_race_sound_mode_token(NativeRuntimeContext& c,std::uint32_t token){
    if(token==0x48b210u||token==0x48b2a0u)c.race.sound.option_83036c=0xffu;   // mode-10 init / mode-12 exit
}
bool native_race_sound_invoke(NativeRuntimeContext& c,std::uint32_t callback,PcSceneRenderer* renderer){
    auto& s=c.race.sound;
    if(callback==0x45a280u){                  // event 360 COMM_TRANS
        ++s.comm_trans;
        // With a LAN session [7D68AC] the original 45A280 -> 45A0C0 race tick (the local car's
        // packets, the CommRace times, the race clock 7F1938) runs in the network module.
        if(native_network_u32(0x7d68acu)){(void)native_network_invoke(c,0x45a280u,0,{});return true;}
        CommTransWorld w{};                   // [7D68AC] CommRace manager: set only by the LAN session code; 0 offline
        w.manager_known=true;
        if(comm_trans_control_45a280(w)==CommTransResult::Unknown)++s.missing[0x45a280u];
        return true;
    }
    unsigned kind=0;
    switch(callback){case 0x424650u:kind=1;break;case 0x424700u:kind=2;break;case 0x424790u:kind=3;break;default:return false;}
    if(kind==1)++s.inits;else if(kind==2)++s.controls;else ++s.destroys;
    if(s.latched&&kind!=3){++s.skipped;return true;}
    if(kind==2&&!s.initialized){++s.skipped;s.last_error="424700 without a completed 424650";return true;}
    queue_in(c,s.state);
    try{
        auto& area=native_race_area_memory(c,renderer,true);
        Binding b{c,s,area};
        const auto& slots=c.event_state.slots;
        std::array<std::uint8_t,0x180> flags{};
        for(std::uint32_t id=0;id<flags.size();++id)flags[id]=std::uint8_t(slots[id].flags);
        RaceSoundWorld w{};
        w.enabled_95b248=s.audio_ready&&s.audio_ready()?1u:0u;
        w.mode_78026c=std::int32_t(c.mode_state.current);
        w.event_flags_79fb48=flags.data();
        auto& car=c.event_function36.car_select;
        w.event_work[8]={slots[8].work_token,car.car_799d18.data(),car.car_799d18.size()};
        auto& m=c.race.manager;
        for(std::uint32_t id=NativeRaceManager::CarWorkFirst;id<32u;++id){
            if((slots[id].flags&3u)&&!native_ghost_car_slot(slots[id])&&!native_traffic_car_slot(slots[id]))throw SoundMissing(0x799b38u+id*0x3cu,"event "+std::to_string(id)+" is open: its car work has no native owner");
            const auto off=std::size_t(id-NativeRaceManager::CarWorkFirst)*NativeRaceManager::CarWorkStride;
            w.event_work[id]={NativeRaceManager::CarWorkBase+std::uint32_t(off),m.car_works_7815a0.data()+off,NativeRaceManager::CarWorkStride};
        }
        w.car_82e7f0={0x82e7f0u,car.body_82e7f0.data(),car.body_82e7f0.size()};
        std::uint32_t params;std::memcpy(&params,car.car_799d18.data()+0x2b4,4);
        w.params={params,car.parameters.data(),car.parameters.size()};
        if(kind==1){race_sound_init_424650(s.state,w,b);s.initialized=true;}
        else if(kind==2)race_sound_control_424700(s.state,w,b);
        else{race_sound_destroy_424790(s.state,b);s.initialized=false;}
    }catch(const SoundMissing& e){latch(s,e.pc,e.what());}
    catch(const PcRaceUnmapped& e){latch(s,e.address,std::string("unmapped PC address: ")+e.what());}
    catch(const std::exception& e){latch(s,0xffffffffu,e.what());}
    queue_out(c,s.state);
    return true;
}
std::string native_race_sound_status(const NativeRuntimeContext& c){
    const auto& s=c.race.sound;std::ostringstream o;
    o<<"race sound inits="<<s.inits<<" controls="<<s.controls<<" destroys="<<s.destroys<<" skipped="<<s.skipped
     <<" ics="<<s.ics_updates<<" clear_all="<<s.clear_alls<<" comm_trans="<<s.comm_trans<<" fade9560cc="<<s.state.f32(0x9560ccu)<<" count955c9c="<<s.state.u32(0x955c9cu)<<std::hex;
    o<<" played:";for(const auto& [k,n]:s.played)o<<' '<<k<<'x'<<std::dec<<n<<std::hex;
    o<<" refused:";for(const auto& [k,n]:s.refused)o<<' '<<k<<'x'<<std::dec<<n<<std::hex;
    o<<" missing:";for(const auto& [k,n]:s.missing)o<<' '<<k<<'x'<<std::dec<<n<<std::hex;
    o<<" ics(ch/code=value):";for(const auto& [k,v]:s.ics_last)o<<' '<<(k>>32)<<'/'<<std::uint32_t(k)<<'='<<v;
    if(s.latched)o<<" fault="<<s.fault_pc<<' '<<s.last_error;
    else if(!s.last_error.empty())o<<" last="<<s.last_error;
    return o.str();
}
}
