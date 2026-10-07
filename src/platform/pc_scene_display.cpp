#include "enhancements/frame_rate.hpp"
#include "platform/pc_scene_display.hpp"
#include "driving/pc_x87.hpp"
#include <cmath>
#include <cstring>
#include "driving/service_hole.hpp"
namespace outrun::platform {
namespace {
inline float addf(float a,float b){volatile float x=a+b;return x;}
inline float mulf(float a,float b){volatile float x=a*b;return x;}
// fld dword; fsin; fstp dword (FSIN is not affected by the precision control).
inline float x87_sin(float v){return driving::x87_float(driving::x87_sin(driving::X87(v)));}
constexpr float kPhaseStep4=0.02094395086169243f;  // 59D914
constexpr float kPhaseStep8=0.10471975058317184f;  // 59D918
constexpr float kGreenScale=0.0f;                  // 619A34
// Lanes +0x28/+0x18/+0x08 of the light <-> 7A0DBC/7A0DCC/7A0DDC.
constexpr std::uint32_t kLaneOffsets[3]{0x28u,0x18u,0x08u};
void save_light(const std::uint8_t* light,PcSceneDisplayGlobals& g){
    for(unsigned k=0;k<3;++k)std::memcpy(&g.saved_7a0dbc[k*4],light+kLaneOffsets[k],16);
}
void restore_light(std::uint8_t* light,const PcSceneDisplayGlobals& g){
    for(unsigned k=0;k<3;++k)std::memcpy(light+kLaneOffsets[k],&g.saved_7a0dbc[k*4],16);
}
void colour_light(std::uint8_t* light,float r,float gr,float b){
    for(std::uint32_t lane:{0x08u,0x18u,0x28u}){
        std::memcpy(light+lane,&r,4);std::memcpy(light+lane+4,&gr,4);std::memcpy(light+lane+8,&b,4);
    }
}
void reinstall(const PcSceneDisplayServices& s,std::uint8_t* light){
    if(s.lights_reset)s.lights_reset(s.user);else outrun::driving::service_hole("reinstall","s.lights_reset");
    if(s.light_add)s.light_add(s.user,light);else outrun::driving::service_hole("reinstall","s.light_add");
}
void call(const PcSceneDisplayServices& s,std::uint32_t callback,const driving::PcEventSlot& slot){
    if(callback&&s.display)s.display(s.user,callback,slot.work_token,slot.event_id);
}
void plain_loop(driving::PcEventControlState& state,std::uint32_t mask,std::int32_t first,std::int32_t last,
                const PcSceneDisplayServices& s){
    for(std::int32_t i=first;i<=last;++i){
        auto& slot=state.slots[std::uint32_t(i)];
        state.current_slot=std::uint32_t(i);
        if((slot.flags&0x02u)&&(slot.display_scene&mask))call(s,slot.disp_callback,slot);
    }
}
}

void event_display_43fb40(driving::PcEventControlState& state,std::uint32_t mask,
                          std::int32_t first,std::int32_t last,std::uint8_t* light,
                          PcSceneDisplayGlobals& g,const PcSceneDisplayServices& s){
    bool coloured=false;
    if(g.mode_780258==2u){
        const std::uint32_t scene=g.scene_7f2428;
        save_light(light,g);
        if(mask==0x40u&&(scene==2u||scene==6u||scene==0x14u)){
            float red,blue;
            if(scene==0x14u){red=0.0f;blue=1.0f;}else{red=1.0f;blue=0.0f;}
            coloured=true;
            if(!g.paused_780248&&enhancements::display_ticks())g.phase_7a0db8=addf(g.phase_7a0db8,kPhaseStep8);   // port: not on display-only frames
            const float x=x87_sin(g.phase_7a0db8);
            colour_light(light,mulf(x,red),mulf(x,kGreenScale),mulf(x,blue));
            reinstall(s,light);
        }
        plain_loop(state,mask,first,last,s);
    }else{
        if(!g.object_7f9460_60){plain_loop(state,mask,first,last,s);return;}
        save_light(light,g);
        if(!g.paused_780248&&enhancements::display_ticks())g.phase_7a0db4=addf(g.phase_7a0db4,kPhaseStep4);   // port: not on display-only frames
        const float x=x87_sin(g.phase_7a0db4);
        for(std::int32_t i=first;i<=last;++i){
            auto& slot=state.slots[std::uint32_t(i)];
            state.current_slot=std::uint32_t(i);
            if(!(slot.flags&0x02u)||!(slot.display_scene&mask))continue;
            const std::int32_t id=std::int32_t(slot.event_id);
            std::uint32_t kind=0;
            if(id>=8&&id<=0x1f&&mask==0x40u)kind=s.work_d10?s.work_d10(s.user,slot.work_token):(outrun::driving::service_hole("event_display_43fb40","s.work_d10"),0u);
            if(kind-1u>4u){call(s,slot.disp_callback,slot);continue;}
            float r=0.0f,gr=0.0f,b=0.0f;
            if(kind==1u||kind==3u)r=1.0f;else if(kind==4u)gr=1.0f;else b=1.0f;
            coloured=true;
            if(kind<3u){r=mulf(r,x);gr=mulf(gr,x);b=mulf(b,x);}
            colour_light(light,r,gr,b);
            if(s.flush_alpha)s.flush_alpha(s.user);else outrun::driving::service_hole("event_display_43fb40","s.flush_alpha");
            reinstall(s,light);
            call(s,slot.disp_callback,slot);
            if(s.flush_alpha)s.flush_alpha(s.user);else outrun::driving::service_hole("event_display_43fb40","s.flush_alpha");
            restore_light(light,g);
            reinstall(s,light);
        }
    }
    if(coloured)restore_light(light,g);
}

void event_shadow_display_4400c0(driving::PcEventControlState& state,const PcSceneDisplayGlobals& g,
                                 const PcSceneDisplayServices& s){
    if(!((g.shadow_79f5ec>>2)&1u))return;
    for(std::uint32_t i=0;i<driving::PcEventSlotCount;++i){
        auto& slot=state.slots[i];
        state.current_slot=i;
        if(slot.flags&0x02u)call(s,slot.shadow_callback,slot);
    }
}
}
