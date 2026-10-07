#include "platform/frontend_sprites.hpp"
#include "driving/pc_x87.hpp"
#include <algorithm>
#include <cmath>
#include <cstring>

namespace outrun::platform {
namespace {
std::uint32_t read32(const std::uint8_t* p){std::uint32_t v;std::memcpy(&v,p,4);return v;}
float read_float(const std::uint8_t* p){float v;std::memcpy(&v,p,4);return v;}
}
bool FrontendSprites::bind(std::uint32_t bank,const GameUiPack& pack){
    std::vector<FrontendSpriteTiming> timing;
    for(std::uint32_t i=0;i<pack.scene_count;++i){
        GameUiScene scene{};if(!game_ui_scene(pack,i,scene))return false;
        const auto* data=pack.animation.data();
        const auto offset=read32(data+i*4u);
        const auto root=read32(data+offset+4u)+(scene.components-1u)*36u;
        const auto size=read32(data+root);
        timing.push_back({read_float(data+root+8u),read_float(data+root+12u),
            float(static_cast<std::int16_t>(size&0xffffu)),float(static_cast<std::int16_t>(size>>16u)),read_float(data+root+4u)});
    }
    return bind_timing(bank,timing);
}
bool FrontendSprites::bind_timing(std::uint32_t bank,const std::vector<FrontendSpriteTiming>& scenes){
    if(bank<0x20u||bank>=0x4bu||scenes.empty()||scenes.size()>65536u)return false;
    for(const auto& s:scenes)if(!std::isfinite(s.frames)||s.frames<1.0f||
        s.frames>2147483520.0f||!std::isfinite(s.rate)||s.rate<0.0f||
        !std::isfinite(s.width)||!std::isfinite(s.height)||s.width<=0||s.height<=0)return false;
    banks_[bank-0x20u]=scenes;return true;
}
void FrontendSprites::reset(){instances_={};used_={};next_={};}
void FrontendSprites::clear_allocations(){used_={};next_={};for(auto& s:instances_)s.allocated=false;}
bool FrontendSprites::bank_scene(std::uint32_t token,FrontendSpriteTiming& out) const {
    const auto bank=token>>16u,index=token&0xffffu;
    if(bank<0x20u||bank>=0x4bu||index>=banks_[bank-0x20u].size())return false;
    out=banks_[bank-0x20u][index];return true;
}
std::uint32_t FrontendSprites::create(std::uint32_t token,std::uint32_t layer,
    std::uint32_t mode,std::int32_t first,std::int32_t last,bool explicit_range,
    std::uint32_t pause_domain){
    const auto bank=token>>16u,index=token&0xffffu;
    if(bank<0x20u||bank>=0x4bu||layer>=Layers||used_[layer]>=Slots)return ~0u;
    const auto& scenes=banks_[bank-0x20u];if(index>=scenes.size())return ~0u;
    // 4282B0: the cursor always moves one past the slot it examines.
    unsigned slot;
    do{slot=next_[layer];next_[layer]=(slot+1u)%Slots;}while(instances_[layer*Slots+slot].allocated);
    const auto handle=layer*Slots+slot;
    auto& s=instances_[handle];s={};s.allocated=true;s.visible=true;
    s.token=token;s.mode=mode;s.status=1;s.pause_domain=pause_domain;
    s.frame=explicit_range?float(first):0.0f;
    s.first=explicit_range&&first>=0?float(first):0.0f;
    s.last=explicit_range?(last<0?scenes[index].frames-1.0f:float(last)):
        float(static_cast<std::int32_t>(scenes[index].frames)-1);
    s.speed=1.0f;s.rate=scenes[index].rate;
    s.canvas_width=scenes[index].width;s.canvas_height=scenes[index].height;
    s.matrix[0]=s.matrix[5]=s.matrix[10]=s.matrix[15]=1.0f;
    ++used_[layer];return handle;
}
bool FrontendSprites::release(std::uint32_t handle){
    if(handle>=Count)return false;
    const bool was=instances_[handle].allocated;
    instances_[handle].allocated=false;const auto layer=handle/Slots;
    if(used_[layer])--used_[layer];
    next_[layer]=handle%Slots;return was;
}
const FrontendSprite* FrontendSprites::get(std::uint32_t handle) const {
    return handle<Count?&instances_[handle]:nullptr;
}
std::uint32_t FrontendSprites::status(std::uint32_t handle) const {
    const auto* s=get(handle);return s?s->status:0u;
}
bool FrontendSprites::set_speed(std::uint32_t handle,float speed){
    if(handle>=Count||!std::isfinite(speed))return false;
    instances_[handle].speed=speed;return true;
}
bool FrontendSprites::set_frame(std::uint32_t handle,std::int32_t frame){
    if(handle>=Count)return false;
    instances_[handle].frame=float(frame);return true;
}
bool FrontendSprites::set_matrix(std::uint32_t handle,const std::array<float,16>& matrix){
    if(handle>=Count)return false;
    for(float f:matrix)if(!std::isfinite(f))return false;
    instances_[handle].matrix=matrix;return true;
}
void FrontendSprites::tick(const FrontendSpriteClock& clock){
    for(unsigned layer=0;layer<Layers;++layer){
        unsigned processed=0;
        const auto count=used_[layer];
        for(unsigned slot=0;slot<Slots&&processed<count;++slot){
            auto& s=instances_[layer*Slots+slot];
            if(!s.allocated||s.mode==2u||(clock.paused&&s.pause_domain!=1u))continue;
            if(clock.sixty_hz||clock.advance_fifty_hz){
                // 427F70: fld rate, fmul speed, fmul step, fadd frame, fstp (x87).
                const float step=clock.sixty_hz?0.01666666753590107f:0.019999999552965164f;
                using driving::X87;
                s.frame=driving::x87_float(X87(s.rate)*X87(s.speed)*X87(step)+X87(s.frame));
            }
            const float lo=std::min(s.first,s.last),hi=std::max(s.first,s.last);
            if(s.frame>hi||s.frame<lo){
                if(s.mode==0u){s.frame=s.frame>hi?lo:hi;s.status=2;}
                else if(s.mode==1u||s.mode==5u){s.frame=std::clamp(s.frame,lo,hi);s.status=s.mode==1u?3u:4u;}
                else if(s.mode==3u){s.allocated=false;--used_[layer];}
            }
            ++processed;
        }
    }
}
void FrontendSprites::drawn(std::uint32_t handle){
    if(handle<Count&&instances_[handle].allocated&&instances_[handle].mode==4u){
        instances_[handle].allocated=false;--used_[handle/Slots];
    }
}
void frontend_sprite_transform(GameUiDraw& draw,const FrontendSprite& sprite,float width,float height){
    const auto& m=sprite.matrix;
    for(auto& p:draw.corners){
        const float x=p[0]-width*0.5f,y=p[1]-height*0.5f;
        // 429010 centres the component canvas in the fixed 640x480 viewport.
        p={{x*m[0]+y*m[4]+m[12]+320.0f,
            x*m[1]+y*m[5]+m[13]+240.0f}};
    }
}
}
