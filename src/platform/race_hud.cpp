#include "platform/race_hud.hpp"
#include "driving/pc_d3dx.hpp"
#include "driving/pc_x87.hpp"
#include <cmath>
#include <cstring>
#include <stdexcept>
namespace outrun::platform {
std::uint32_t pc_ftol2_582194(float value){
    // fld m32 (exact), _ftol2: 64-bit truncation, EAX = low dword.
    return std::uint32_t(std::uint64_t(driving::x87_ftol64(driving::X87(value))));
}
std::uint32_t navi_voice_request_45bd30(NaviVoiceQueue& q,const NaviVoiceServices& s,std::int32_t id,
                                        std::int16_t duration,std::int16_t priority){
    if(s.busy_7f94c0&&s.busy_7f95c4)return 0u;                              // 46C530
    const auto now=pc_ftol2_582194(s.time_842110);
    auto& head=q.entries_7f1998[std::uint32_t(q.head_7f1c64)&7u];
    if(now-q.last_7f8b40>std::uint32_t(head.duration)){
        q.last_7f8b40=now;head.duration=duration;head.priority=priority;head.id=id;
        if(id!=-1&&s.sounds_424940)s.sounds_424940->push_back(std::uint32_t(id));
        return 1u;
    }
    if(q.entries_7f1998[std::uint32_t(q.tail_7f1994)&7u].priority>=priority)return 0u;
    q.tail_7f1994=(q.tail_7f1994+1)&7;auto& e=q.entries_7f1998[std::uint32_t(q.tail_7f1994)];
    e.priority=priority;e.id=id;e.duration=duration;return 1u;
}
void navi_voice_pump_45bcd0(NaviVoiceQueue& q,const NaviVoiceServices& s){
    if(q.head_7f1c64==q.tail_7f1994)return;
    const auto now=pc_ftol2_582194(s.time_842110);
    if(now-q.last_7f8b40<=std::uint32_t(q.entries_7f1998[std::uint32_t(q.head_7f1c64)&7u].duration))return;
    q.last_7f8b40=now;q.head_7f1c64=(q.head_7f1c64+1)&7;
    const auto id=q.entries_7f1998[std::uint32_t(q.head_7f1c64)].id;
    if(id!=-1&&s.sounds_424940)s.sounds_424940->push_back(std::uint32_t(id));
}
namespace {
std::array<float,16> translation(float x,float y){   // D3DXMatrixTranslation(x,y,0)
    std::array<float,16> m{};m[0]=m[5]=m[10]=m[15]=1.0f;m[12]=x;m[13]=y;m[14]=0.0f;return m;
}
driving::Bytes view(std::array<float,16>& m){return driving::Bytes(m.data(),64);}
}
// ---------------------------------------------------------------- event 398
void sprani_init_427e30(FrontendSprites& pool,SpraniGlobals& g){
    pool.clear_allocations();g.current_7551b4=-1;g.paused_95b214=0;
}
void sprani_matrix_429010(driving::PcMatrixStack& stack,float w,float h,const std::array<float,16>& instance,
                          std::array<float,3>& blend){
    // fild word * 0.5 (exact), 320/240 minus it as binary32 stores.
    const float wk=w*0.5f,hk=h*0.5f;
    auto top=stack.current();
    for(unsigned k=0;k<16;++k)top.putf(k*4,k%5==0?1.0f:0.0f);
    auto t=translation(320.0f-wk,240.0f-hk);driving::pc_matrix_multiply_current(stack,view(t));
    t=translation(wk,hk);driving::pc_matrix_multiply_current(stack,view(t));
    auto inst=instance;driving::pc_matrix_multiply_current(stack,view(inst));
    t=translation(-wk,-hk);driving::pc_matrix_multiply_current(stack,view(t));
    blend={0.0f,1.0f,1.0f};
}
void sprani_display_428170(FrontendSprites& pool,SpraniGlobals& g,driving::PcMatrixStack& stack,
                           std::vector<SpraniDraw>& draws,std::array<float,3>& blend){
    for(unsigned layer=0;layer<FrontendSprites::Layers;++layer){
        const unsigned count=pool.used(layer);unsigned drawn=0;
        for(unsigned slot=0;slot<FrontendSprites::Slots&&count>drawn;++slot){
            const auto handle=layer*FrontendSprites::Slots+slot;
            auto* s=pool.get_mutable(handle);
            if(!s->allocated||!s->visible)continue;
            g.current_7551b4=s->id_2c;g.flag_986b28=0;
            driving::pc_matrix_push(stack);
            sprani_matrix_429010(stack,s->canvas_width,s->canvas_height,s->matrix,blend);
            SpraniDraw d;d.handle=handle;d.token=s->token;d.layer=layer;d.frame=s->frame;d.id_7551b4=s->id_2c;
            auto top=stack.current();for(unsigned k=0;k<16;++k)d.matrix[k]=top.f32(k*4);
            draws.push_back(d);                                                 // 429460
            driving::pc_matrix_pop(stack);
            pool.drawn(handle);
            ++drawn;
        }
    }
}
std::array<float,16> sprani_translation(float x,float y){return translation(x,y);}
bool sprani_draw_428a10(const FrontendSprites& pool,SpraniGlobals& g,driving::PcMatrixStack& stack,std::uint32_t token,
                        const std::array<float,16>* matrix,std::uint32_t layer,std::int32_t frame,float scale,
                        float s,float angle,std::vector<SpraniDraw>& draws,std::array<float,3>& blend){
    using driving::X87;using driving::x87_float;
    FrontendSpriteTiming t{};
    if(!pool.bank_scene(token,t))return false;                               // bank/scene/root missing: no draw
    g.flag_986b28=0;
    driving::pc_matrix_push(stack);                                           // 409EF0
    const float wk=float(std::int16_t(t.width))*0.5f,hk=float(std::int16_t(t.height))*0.5f;
    auto top=stack.current();
    for(unsigned k=0;k<16;++k)top.putf(k*4,k%5==0?1.0f:0.0f);
    auto m=translation(x87_float(X87(320.0f)-X87(wk)),x87_float(X87(240.0f)-X87(hk)));driving::pc_matrix_multiply_current(stack,view(m));
    m=translation(wk,hk);driving::pc_matrix_multiply_current(stack,view(m));
    driving::pc_matrix_rotate_z(stack,angle);                                 // D3DXMatrixRotationZ, then top = R * top
    m={};m[0]=s;m[5]=s;m[10]=1.0f;m[15]=1.0f;driving::pc_matrix_multiply_current(stack,view(m));   // D3DXMatrixScaling(s,s,1)
    m=translation(-wk,-hk);driving::pc_matrix_multiply_current(stack,view(m));
    blend={angle,s,s};
    if(matrix){                                                               // D3DXMatrixMultiply(top, top, matrix)
        driving::PcMatrix16 a{};for(unsigned k=0;k<16;++k)a[k]=top.f32(k*4);
        const auto r=driving::pc_d3dx_matrix_multiply(a,*matrix);for(unsigned k=0;k<16;++k)top.putf(k*4,r[k]);}
    SpraniDraw d;d.handle=~0u;d.token=token;d.layer=layer;d.frame=float(frame);d.scale=scale;d.id_7551b4=g.current_7551b4;
    top=stack.current();for(unsigned k=0;k<16;++k)d.matrix[k]=top.f32(k*4);
    draws.push_back(d);                                                       // 429460
    driving::pc_matrix_pop(stack);
    return true;
}
bool sprani_draw_428af0(const FrontendSprites& pool,SpraniGlobals& g,driving::PcMatrixStack& stack,std::uint32_t token,
                        const std::array<float,16>& matrix,std::int32_t frame,float s,float angle,std::uint32_t mode,
                        std::vector<SpraniDraw>& draws,std::array<float,3>& blend){
    const auto at=draws.size();
    if(!sprani_draw_428a10(pool,g,stack,token,&matrix,0,frame,1.0f,s,angle,draws,blend))return false;
    draws[at].depth=mode?2:1;
    return true;
}
// ---------------------------------------------------------------- event 389
GadPubState::GadPubState(){
    for(std::uint32_t a=0x836640u;a<0x836654u;a+=4)put32(a,0xffffffffu);
    put32(0x836690u,0xffffffffu);put32(0x836694u,0xffffffffu);
}
std::uint8_t* GadPubState::at(std::uint32_t a,std::size_t n){
    if(a>=Base&&a-Base+n<=Size)return block.data()+(a-Base);
    if(a>=DataBase&&a-DataBase+n<=DataSize)return data.data()+(a-DataBase);
    if(a>=Data2Base&&a-Data2Base+n<=Data2Size)return data2.data()+(a-Data2Base);
    throw std::out_of_range("GAD_PUB address outside its blocks");
}
std::uint32_t GadPubState::u32(std::uint32_t a)const{std::uint32_t v;std::memcpy(&v,at(a,4),4);return v;}
void GadPubState::put32(std::uint32_t a,std::uint32_t v){std::memcpy(at(a,4),&v,4);}
void GadPubState::put16(std::uint32_t a,std::uint16_t v){std::memcpy(at(a,2),&v,2);}
void gad_pub_init_4970f0(GadPubState& s){
    auto f=[](float v){std::uint32_t u;std::memcpy(&u,&v,4);return u;};
    for(std::uint32_t a=0x836658u;a<0x836690u;a+=4)s.put32(a,0xffffffffu);
    s.put32(0x8366c4u,f(520.0f));                                           // 62831C
    s.put32(0x83669cu,0x5c1e00u);s.put16(0x836630u,0);s.put16(0x836698u,0);
    for(std::uint32_t a=0x836640u;a<0x836654u;a+=4)s.put32(a,0xffffffffu);
    s.put32(0x836690u,0xffffffffu);s.put32(0x8366e8u,0);
    s.put32(0x8366ccu,f(-20.0f));s.put32(0x8366c8u,f(3.0f));s.put32(0x8366d0u,0);  // 5C20E0, 6282A4
    s.put32(0x8366d4u,f(500.0f));s.put32(0x8366dcu,f(-20.0f));s.put32(0x8366d8u,f(3.0f));   // 5A29F0
    s.put32(0x8366e0u,0);s.put32(0x8366a0u,0);s.put32(0x836694u,0xffffffffu);s.put32(0x836654u,0);
    s.put32(0x8366a4u,0);s.put32(0x836638u,0);s.put8(0x67ee3au,1);s.put32(0x83670cu,0);
}
bool gad_pub_control_498590(GadPubState& s,FrontendSprites& pool,std::uint32_t mode,std::uint32_t variant,float& value,std::uint32_t& missing,
                            int (*lan_results)(void*),void* lan_user){
    for(std::uint32_t a=0x836640u;a<0x836654u;a+=4){
        const std::int32_t h=s.i32(a);
        if(h<0)continue;
        if(pool.status(std::uint32_t(h))==3u){pool.release(std::uint32_t(h));s.put32(a,0xffffffffu);}   // 428880 / 4285A0
    }
    // 4985DB (modes 19/21/22): 495B00 = (780258 == 4): the variant-4 ranking upload.
    if((mode==0x13u||mode==0x15u||mode==0x16u)&&variant==4u){
        const int r=lan_results?lan_results(lan_user):-1;
        if(r<0){missing=0x4985e4u;return false;}
        if(r>0)return true;                                                 // 498653: ret
    }
    value=0.0f;return true;                                                 // 4EF410(0)
}
bool gad_pub_display_4998c0(std::uint32_t mode,std::uint32_t& missing){
    static constexpr std::uint8_t index[23]{0,9,9,9,1,2,3,4,4,3,5,1,1,1,6,9,9,9,9,9,7,8,1};
    static constexpr std::uint32_t target[10]{0x499a30u,0,0x49a060u,0x498670u,0x499020u,0x497900u,0x497960u,0x48c5f0u,0x497fa0u,0};
    const auto i=mode-13u;
    if(i>0x16u)return true;
    const auto t=index[i];
    if(t==9u||t==1u)return true;
    if(t==2u&&mode!=0x12u)return true;
    missing=target[t];return false;
}
}
