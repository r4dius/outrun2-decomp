#include "system/exe_image.hpp"
#include "platform/race_end_modes.hpp"
#include "platform/sp_rankings.hpp"
#include "platform/pc_address_view.hpp"
#include "driving/pc_x87.hpp"
#include <cmath>
#include <cstdio>
#include <cstring>
#include <stdexcept>
#include <string>
namespace outrun::platform {
namespace {
#include "platform/race_end_tables.inc"
std::uint32_t call(PcRaceContext& c,std::uint32_t pc,std::initializer_list<std::uint32_t> args,std::uint32_t ecx=0){
    PcRaceCall k;k.pc=pc;k.ecx=ecx;k.argc=std::uint32_t(args.size());
    std::size_t i=0;for(auto a:args)k.args[i++]=a;
    return c.service(k);
}
std::uint32_t bits(float f){std::uint32_t v;std::memcpy(&v,&f,4);return v;}
// cvttss2si: truncation; NaN and out-of-range give 0x80000000.
std::int32_t cvtt(float f){
    if(!(f>-2147483904.0f&&f<2147483648.0f))return std::int32_t(0x80000000u);
    return std::int32_t(f);
}
constexpr float K628180=-10000.0f,K5c4bf8=-9000.0f,K62806c=1.0f,K6280c4=-1.0f;
constexpr float K628100=30.0f,K5c4bfc=23.0f,K5c6900=46.0f,K5b4434=7.0f,K5c1854=14.0f;
// Stack matrices passed by address: one page mapped for the call.
struct Locals {
    PcRaceMemory& m;std::size_t mark;std::array<std::uint8_t,0x100> bytes{};
    explicit Locals(PcRaceMemory& mem):m(mem),mark(mem.mark()){m.map(PcRaceEndLocals,bytes.data(),bytes.size());}
    ~Locals(){m.release(mark);}
};
// D3DXMatrixTranslation(out, x, y, z).
void translation(PcRaceMemory& m,std::uint32_t out,float x,float y,float z){
    for(std::uint32_t k=0;k<16;++k)m.putf(out+k*4,(k%5u)==0u?1.0f:0.0f);
    m.putf(out+0x30,x);m.putf(out+0x34,y);m.putf(out+0x38,z);
}
std::uint32_t create_428440(PcRaceContext& c,std::uint32_t token,std::uint32_t layer,std::uint32_t first,std::uint32_t last){
    return call(c,0x428460u,{token,layer,0u,first,last});
}
// The race-suspend group shared by 49A1E0, 49ADC0, 49A2E0, 49A370, 49C9C0.
void suspend_race(PcRaceContext& c,std::uint32_t player){
    call(c,0x440a10u,{8u,0x18u});call(c,0x440a10u,{0x186u,1u});call(c,0x440a10u,{0x187u,1u});
    call(c,0x440a10u,{0x20u,0xfau});call(c,0x440a10u,{0x11au,0x1au});
    if((c.m.u32(player+4)&0x80000000u)==0x80000000u)call(c,0x440a10u,{0x16au,0x15u});
}
// 4489C0 .. 4F2210: the course resources (49A4F0 / 49AF90 / 49B190).
void release_course(PcRaceContext& c){
    call(c,0x4489c0u,{});call(c,0x44c3d0u,{});call(c,0x44a1a0u,{});
    for(std::uint32_t i=0;i<4;++i)call(c,0x43de50u,{i});
    for(std::uint32_t i=0;i<3;++i)call(c,0x4f11b0u,{i});
    for(std::uint32_t i=0;i<3;++i)call(c,0x4f0600u,{i});
    call(c,0x46fc30u,{0u});call(c,0x46fc30u,{1u});
    call(c,0x4f2210u,{});
}
// 49A130: race sound stops (4278C0 requests).
void sounds_49a130(PcRaceContext& c){
    const std::uint32_t v=c.m.u32(0x780258u);
    if(v==5u||v==6u||v==4u){call(c,0x4278c0u,{0x800u});call(c,0x4278c0u,{0x700u});call(c,0x4278c0u,{0x600u});}
    for(std::uint32_t id:{0x300u,0x400u,0x100u,0u})call(c,0x4278c0u,{id});
}
// ---- GAD_PUB text and animation helpers (498670 / 499020) -------------------
constexpr std::uint32_t One=0x3f800000u;
// 5802DD sprintf of the integer formats the GAD_PUB displays use.
std::string format_5802dd(std::uint32_t format,std::initializer_list<std::int32_t> args){
    const char* f=nullptr;
    switch(format){
    case 0x5c20e4u:f="% 2d'%02d\"%03d";break;
    case 0x5c20f4u:f="%07d";break;
    case 0x5c21e0u:f="%-4d";break;
    case 0x5c2130u:f="%d";break;
    case 0x5c212cu:f="%2d";break;
    case 0x5c2120u:f="%02d";break;
    case 0x5c2104u:f="%02d%%";break;
    case 0x5c210cu:f="%08d";break;
    case 0x5b0334u:f="%03d";break;
    default:throw std::logic_error("5802DD format not modelled");
    }
    std::array<std::int32_t,3> v{};std::size_t i=0;for(auto a:args)v[i++]=a;
    char out[64];std::snprintf(out,sizeof out,f,v[0],v[1],v[2]);
    return out;
}
// 4B9200(text, x, y, step): 42CC00(x, y) then 42CCC0(c) per character.
void text_4b9200(PcRaceContext& c,const std::string& s,std::uint32_t x,std::uint32_t y,std::uint32_t step){
    for(unsigned char ch:s){call(c,0x42cc00u,{x,y});call(c,0x42ccc0u,{ch});x+=step;}
}
// 42CCB0(mode), 42CA60(3), 42CC60(1, 1), 42CCA0(colour).
void text_font3(PcRaceContext& c,std::uint32_t colour){
    call(c,0x42ccb0u,{0u});call(c,0x42ca60u,{3u});call(c,0x42cc60u,{One,One});call(c,0x42cca0u,{colour});
}
// 449AC0(hours, minutes, seconds, ms, time): 16-bit words of a time in ms
// (minutes keep the PC's "- hours * 3600").
struct TimeWords { std::uint16_t hours,minutes,seconds,ms; };
TimeWords time_words(std::uint32_t t){
    TimeWords w{};const std::uint32_t s=t/1000u;w.ms=std::uint16_t(t-s*1000u);
    const std::uint32_t h=s/3600u;w.hours=std::uint16_t(h);
    const std::uint32_t mi=s/60u-h*3600u;w.minutes=std::uint16_t(mi);
    w.seconds=std::uint16_t(mi*0xffffffc4u-std::uint32_t(std::uint16_t(w.hours*0xe10u))+s);
    return w;
}
void time_words_449ac0(PcRaceMemory& m,std::uint32_t ph,std::uint32_t pm,std::uint32_t ps,std::uint32_t pms,std::uint32_t t){
    const auto w=time_words(t);m.put16(pms,w.ms);m.put16(ph,w.hours);m.put16(pm,w.minutes);m.put16(ps,w.seconds);
}
// 4973C0(x, y, colour): the race time words 8366EC'836634"8366E4.
void gad_time_4973c0(PcRaceContext& c,std::uint32_t x,std::uint32_t y,std::uint32_t colour){
    auto& m=c.m;text_font3(c,colour);
    text_4b9200(c,format_5802dd(0x5c20e4u,{m.u16(0x8366ecu),m.u16(0x836634u),m.u16(0x8366e4u)}),x,y,0x14u);
}
// 4974E0(EAX = car, x, y, colour): the 4B99D0 score, at most 9999999.
void gad_score_4974e0(PcRaceContext& c,std::uint32_t car,std::uint32_t x,std::uint32_t y,std::uint32_t colour){
    std::int32_t score=std::int32_t(call(c,0x4b99d0u,{car}));
    if(score>0x98967f)score=0x98967f;
    text_font3(c,colour);
    text_4b9200(c,format_5802dd(0x5c20f4u,{score}),x,y,0x14u);
}
// 497430(x, y, colour): the ghost 2 time (465F70 / 466190), 0 without one.
void gad_ghost_time_497430(PcRaceContext& c,std::uint32_t x,std::uint32_t y,std::uint32_t colour){
    TimeWords w{};
    if(call(c,0x465f70u,{2u})&0xffu)w=time_words(call(c,0x466190u,{2u}));
    text_font3(c,colour);
    text_4b9200(c,format_5802dd(0x5c20e4u,{w.minutes,w.seconds,w.ms}),x,y,0x14u);
}
// 497220(EAX = p, XMM0 = a, XMM1 = b, XMM3 = t, s0..s4): a damped slide of
// p[0] towards t (p = {position, damping, speed, phase}), SSE single.
void anim_497220(PcRaceMemory& m,std::uint32_t p,float a,float b,float t,float s0,float s1,float s2,float s3,float s4){
    if(a>m.f32(p)&&m.u32(p+0xc)==0u)m.put32(p+0xc,1u);
    if(s0>m.f32(p)&&m.u32(p+0xc)==1u)m.put32(p+0xc,2u);
    const std::uint32_t phase=m.u32(p+0xc);
    const float v=m.f32(p+8);
    m.putf(p+8,phase==0u?v*s1:phase==1u?v*s2:v*s3);
    if(t>m.f32(p)&&0.0f>m.f32(p+8)){
        const float speed=0.0f-m.f32(p+8)*b,damp=s4*m.f32(p+4);
        m.putf(p+8,speed);m.putf(p+4,damp);
    }
    if(m.f32(p)>t+m.f32(p+4)&&m.f32(p+8)>0.0f){
        const float damp=s4*m.f32(p+4),speed=0.0f-m.f32(p+8)*b;
        m.putf(p+8,speed);m.putf(p+4,damp);
    }
    const float one=m.f32(0x67ee54u),d=m.f32(p+8),x=d+m.f32(p);
    m.putf(p,x);
    if(d>0.0f-one&&one>d&&x>t-one&&one+t>x){m.putf(p,t);m.putf(p+8,0.0f);}
}
// 49A190: the goal voice still playing (427700(4), then the variant's own).
bool goal_voice_49a190(PcRaceContext& c){
    if(call(c,0x427700u,{4u}))return true;
    const std::uint32_t v=c.m.u32(0x780258u);                  // VM 49A19E: mov eax,[780258]
    if(v==5u||v==2u)return call(c,0x427700u,{0x8405u})!=0u;
    if(v==6u||v==4u)return call(c,0x427700u,{0x889cu})!=0u;
    return false;
}
void mode_escape(PcRaceContext& c){if(call(c,0x43f980u,{}))call(c,0x43f990u,{0x1du});}
// 49A800 request block: 450240 == 1 or (arcade and a ranking entry) -> name entry.
void result_next(PcRaceContext& c){
    auto& m=c.m;
    const std::uint32_t v=m.u32(0x780258u);
    if(v==3u||v==4u)call(c,0x43f8c0u,{0x1bu});
    if(call(c,0x450240u,{})==1u||(m.u32(0x780258u)==0u&&m.u32(0x8367acu)!=0u))call(c,0x43f8c0u,{0x1au});
    else call(c,0x43f8c0u,{0x1bu});
}
}
PcRaceEndState::PcRaceEndState(){
    adv_7f1958[8]=2;adv_7f1958[0x14]=0xf;
    std::memcpy(camera_651758.data(),Exe651758,sizeof Exe651758);
    for(std::size_t i=0;i<EmbeddedExeRangeCount;++i){   // the arcade .data tables (EXE initial values)
        const auto& r=EmbeddedExeRanges[i];
        if(r.base==0x6898c0u&&r.size>=arcade_6898c0.size())std::memcpy(arcade_6898c0.data(),r.data,arcade_6898c0.size());
        if(r.base==0x65a7a0u&&r.size>=arcade_65a7a0.size())std::memcpy(arcade_65a7a0.data(),r.data,arcade_65a7a0.size());
        if(r.base==0x638df0u&&r.size>=ending_638df0.size())std::memcpy(ending_638df0.data(),r.data,ending_638df0.size());
    }
    // 4177B9: 453440 sets up the rankings book at boot (47F110 = 0).
    PcRaceMemory m;
    m.map(BookBase,bookkeeping.data(),bookkeeping.size());m.map(SpTablesBase,sp_tables.data(),sp_tables.size());
    m.map(0x7d6720u,stats_7d6720.data(),stats_7d6720.size());
    std::uint32_t cell=BookBase;m.map(0x63960cu,reinterpret_cast<std::uint8_t*>(&cell),4);
    for(std::size_t i=0;i<EmbeddedExeRangeCount;++i)
        if(EmbeddedExeRanges[i].base==0x5da300u)m.map_const(0x5da300u,EmbeddedExeRanges[i].data,EmbeddedExeRanges[i].size);
    sp_book_init_453440(m);
}
void race_end_map_tables(PcRaceMemory& m){
    m.map_const(0x5c4740u,Exe5c4740,sizeof Exe5c4740);
    m.map_const(0x5a7ab4u,Exe5a7ab4,sizeof Exe5a7ab4);
    m.map_const(0x5c2258u,Exe5c2258,sizeof Exe5c2258);
    m.map_const(0x653788u,Exe653788,sizeof Exe653788);
    m.map_const(0x67f618u,Exe67f618,sizeof Exe67f618);
    m.map_const(0x5c1e00u,Exe5c1e00,sizeof Exe5c1e00);
    static constexpr std::uint8_t Exe5c403c[4]{0x33,0x33,0x33,0x3f},Exe67ee54[4]{0x00,0x00,0x80,0x3f};   // 0.7f; .data 1.0f (497220)
    m.map_const(0x5c403cu,Exe5c403c,4);m.map_const(0x67ee54u,Exe67ee54,4);m.map_const(0x67ee2cu,Exe67ee2c,sizeof Exe67ee2c);
    OR2_EXE_BYTES(Exe67ee58,0x67EE58u,0x20u);   // .data record marker frames
    m.map_const(0x67ee58u,Exe67ee58,sizeof Exe67ee58);
    static constexpr std::uint8_t Exe6282cc[4]{0x00,0x00,0xc8,0x42},Exe62810c[4]{0x00,0x00,0x16,0x43};   // 100.0f, 150.0f
    m.map_const(0x6282ccu,Exe6282cc,4);m.map_const(0x62810cu,Exe62810c,4);
    OR2_EXE_BYTES(Exe5e0d94,0x5E0D94u,0x1Cu);   // Heart Attack voices (499020)
    m.map_const(0x5e0d94u,Exe5e0d94,sizeof Exe5e0d94);m.map_const(0x67f644u,Exe67f644,sizeof Exe67f644);
    m.map_const(0x5c2370u,Exe5c2370,sizeof Exe5c2370);
}

// ---- mode 20 ------------------------------------------------------------
void mode20_init_49a1e0(PcRaceContext& c){
    auto& m=c.m;
    const std::uint32_t variant=m.u32(0x780258u),player=m.u32(0x799d18u);
    m.put8(0x836ce0u,0);
    call(c,0x427630u,{});call(c,0x428600u,{});
    const std::uint16_t si=std::uint16_t(variant);
    call(c,0x43f900u,{(si==2u||si==5u)?0x258u:0xf0u});
    suspend_race(c,player);
    adv_release_45af40(c);                                   // tail 45AEF0
}
void mode20_control_49a280(PcRaceContext& c){
    if(call(c,0x43fa90u,{})){
        std::uint32_t next;
        if((call(c,0x4957f0u,{})&0xffu)||call(c,0x55a930u,{},0x7f9460u)||(call(c,0x43f860u,{})&0xffu))next=0x22u;
        else next=c.m.u32(0x780258u)==5u?0x1bu:0x19u;
        call(c,0x43f8c0u,{next});
    }
    mode_escape(c);
}
// ---- modes 21 / 22 (LAN race: TIME OVER / the other end) ----------------------
// 49A2E0 / 49A370: the race suspended (443EA0 on 4035F0, 427630, 43F900(0x294)), the race
// events paused (440A10: 8..0x1F, 0x186, 0x187, 0x20..0x119, 0x11A..0x133, 0x16A..0x17E when
// the player car +4 has bit 31), then 45AEF0; mode 21 also clears [836CE1].
void lan_end_suspend(PcRaceContext& c){
    auto& m=c.m;
    const std::uint32_t player=m.u32(0x799d18u);
    call(c,0x443ea0u,{},call(c,0x4035f0u,{}));
    call(c,0x427630u,{});
    call(c,0x43f900u,{0x294u});
    call(c,0x440a10u,{8u,0x18u});call(c,0x440a10u,{0x186u,1u});call(c,0x440a10u,{0x187u,1u});
    call(c,0x440a10u,{0x20u,0xfau});call(c,0x440a10u,{0x11au,0x1au});
    if((m.u32(player+4u)&0x80000000u)==0x80000000u)call(c,0x440a10u,{0x16au,0x15u});
    call(c,0x45aef0u,{});
}
void mode21_init_49a2e0(PcRaceContext& c){lan_end_suspend(c);c.m.put8(0x836ce1u,0);}
void mode22_init_49a370(PcRaceContext& c){lan_end_suspend(c);}
// 49D210: once ([836CE1]), when 456D70 says the race is over: a player whose rank
// (45A0B0 of 455AD0) is not set and with more than one player gets licence +104 (7C24E4)
// += 1.0; the same on the 43FA90 timeout, then 428600 and mode 0x22. LAN (variant 4,
// 456D60 > 1): 457940 (this player's finish record, sent).
void mode21_control_49d210(PcRaceContext& c){
    auto& m=c.m;
    auto rating=[&]{
        if(call(c,0x45a0b0u,{call(c,0x455ad0u,{})&0xffu})&0xffu)return;
        if(std::int32_t(call(c,0x456e10u,{}))<=1)return;
        m.putf(0x7c24e4u,m.f32(0x7c24e4u)+1.0f);              // 62806C
    };
    if(!m.u8(0x836ce1u)&&call(c,0x456d70u,{})){rating();m.put8(0x836ce1u,1u);}   // (580F92: a debug printf)
    if(call(c,0x43fa90u,{})){
        if(!m.u8(0x836ce1u))rating();
        call(c,0x428600u,{});call(c,0x43f8c0u,{0x22u});
    }
    if(m.u32(0x780258u)==4u&&(call(c,0x456d60u,{})&0xffu)>1u)call(c,0x457940u,{});
}
// 49A400: 456D40 (the remaining time) + 0x294 < 0 and 456D70: 428600, mode 0x22; 43F980:
// 43F990(0x1D); LAN: 457940.
void mode22_control_49a400(PcRaceContext& c){
    if(std::int32_t(call(c,0x456d40u,{})+0x294u)<0&&call(c,0x456d70u,{})){call(c,0x428600u,{});call(c,0x43f8c0u,{0x22u});}
    if(call(c,0x43f980u,{}))call(c,0x43f990u,{0x1du});
    if(c.m.u32(0x780258u)==4u)call(c,0x457940u,{});
}
void mode20_exit_49d1e0(PcRaceContext& c){
    auto& m=c.m;
    const std::uint32_t before=m.u32(0x7c24ecu);
    const std::uint32_t t=call(c,0x4505d0u,{});
    m.put8(0x7c27d4u,std::uint8_t(m.u8(0x7c27d4u)|2u));
    m.put32(0x7c24ecu,t+before);
    race_close_49a4f0(c);
    call(c,0x428600u,{});
}
void race_close_49a4f0(PcRaceContext& c){
    auto& m=c.m;
    if(call(c,0x43f8e0u,{})!=0x19u)return;
    call(c,0x440330u,{8u,0x18u});call(c,0x440330u,{0x16au,0x15u});
    for(std::uint32_t id:{0x18au,0x167u,0x181u,0x186u,0x187u,0x18du,0x184u,0x17fu,0x183u,0x6u})call(c,0x4401d0u,{id});
    const std::uint32_t v=m.u32(0x780258u);
    if(v==4u||v==6u)call(c,0x4401d0u,{0x191u});
    else if(call(c,0x55a930u,{},0x7f9460u))call(c,0x4401d0u,{0x190u});
    else if(v==9u)call(c,0x4401d0u,{0x192u});
    else if(v==7u)call(c,0x4401d0u,{0x193u});
    else if(v==8u)call(c,0x4401d0u,{0x194u});
    call(c,0x440a30u,{0x182u,1u});
    release_course(c);
}

// 449AC0(&hours, &minutes, &seconds, &ms, time) for callers outside this file.
void race_time_words_449ac0(PcRaceMemory& m,std::uint32_t ph,std::uint32_t pm,std::uint32_t ps,std::uint32_t pms,std::uint32_t t){
    time_words_449ac0(m,ph,pm,ps,pms,t);
}

// ---- mode 23 ------------------------------------------------------------
void mode23_init_49a450(PcRaceContext& c){
    call(c,0x43f900u,{0x258u});
    call(c,0x424ae0u,{});
    call(c,0x42e020u,{0u,0u,0x78u});
    call(c,0x401000u,{1u,0x1cu,0u});
}
void mode23_control_49a480(PcRaceContext& c){
    if(std::int32_t(call(c,0x43f910u,{}))<=0x1a4&&
       (call(c,0x4536f0u,{0xc0u})||call(c,0x453780u,{1u})||call(c,0x453780u,{2u})))call(c,0x43f920u,{0x3cu});
    if(call(c,0x43fa90u,{}))call(c,0x43f8c0u,{0x19u});
    mode_escape(c);
}

// ---- mode 29 ------------------------------------------------------------
void mode29_control_49b130(PcRaceContext& c){call(c,0x43f8c0u,{0x1cu});}
void mode29_exit_49b140(PcRaceContext& c){
    call(c,0x401030u,{0u});
    race_events_close_49ad00(c);
    call(c,0x428600u,{});
    call(c,0x487240u,{});
    call(c,0x44fcc0u,{0u});call(c,0x44fce0u,{0u});
}

// ---- mode 31 ------------------------------------------------------------
void mode31_init_49ab90(PcRaceContext& c){
    call(c,0x43f900u,{0u});
    call(c,0x42deb0u,{0x3du,9u});call(c,0x42deb0u,{0x12u,9u});call(c,0x42deb0u,{0x14u,9u});call(c,0x42deb0u,{0x15u,9u});
    call(c,0x429920u,{0x3du,9u});
    call(c,0x42deb0u,{0x36u,9u});call(c,0x429920u,{0x36u,9u});
    call(c,0x42deb0u,{0x37u,9u});call(c,0x429920u,{0x37u,9u});
    c.m.put32(0x836d0cu,0);
    common_anim_4b72f0(c,1u);
}
void mode31_control_49ac00(PcRaceContext& c){
    auto& m=c.m;
    switch(m.u32(0x836d0cu)){
    case 0:
        if(call(c,0x42df90u,{})&&call(c,0x4299a0u,{})){
            call(c,0x440110u,{0x18bu,0x1eu});call(c,0x440110u,{0x17fu,0x17u});
            common_anim_release_4b7630(c);
            m.put32(0x836d0cu,m.u32(0x836d0cu)+1u);
        }
        break;
    case 1:
        if(!call(c,0x4b0960u,{}))break;
        m.put32(0x836d0cu,m.u32(0x836d0cu)+1u);
        [[fallthrough]];
    case 2:
        if(call(c,0x43fa90u,{}))call(c,0x43f8c0u,{0x1bu});
        break;
    default:break;
    }
    mode_escape(c);
}
void mode31_exit_49ac80(PcRaceContext& c){
    call(c,0x4401d0u,{0x18bu});
    call(c,0x427630u,{});
    call(c,0x4401d0u,{0x17fu});
    for(std::uint32_t b:{0x3du,0x12u,0x14u,0x15u,0x36u,0x37u})call(c,0x42dfb0u,{b});
    for(std::uint32_t b:{0x3du,0x36u,0x37u})call(c,0x4299c0u,{b});
}

// ---- mode 25 ------------------------------------------------------------
void mode25_init_49a790(PcRaceContext& c){
    auto& m=c.m;
    call(c,0x43f900u,{0xb4u});
    call(c,0x42deb0u,{0x34u,9u});call(c,0x429920u,{0x34u,9u});
    m.put32(0x836d08u,0);
    call(c,0x401000u,{0u,0x1cu,1u});
    const std::uint32_t variant=m.u32(0x780258u),preset=m.u32(0x78024cu);
    m.put32(0x8367acu,0);
    if(variant==0u)m.put32(0x8367acu,call(c,0x47ef30u,{preset}));
    common_anim_4b72f0(c,1u);
}
void mode25_control_49a800(PcRaceContext& c){
    auto& m=c.m;
    const std::uint32_t stage=m.u32(0x836d08u);
    bool run1=false,run2=false;
    if(stage==0u){
        if(call(c,0x42df90u,{})&&call(c,0x4299a0u,{})){
            call(c,0x440110u,{0x188u,call(c,0x43f960u,{})?0x50u:0x15u});
            common_anim_release_4b7630(c);
            m.put32(0x836d08u,m.u32(0x836d08u)+1u);
            run1=true;
        }
    }else if(stage==1u)run1=true;
    else if(stage==2u)run2=true;
    if(run1){
        if(result_input_4bfb20(c))result_next(c);
        if(m.u32(0x8420fcu)){m.put32(0x836d08u,m.u32(0x836d08u)+1u);run2=true;}   // 4AEF20
    }
    if(run2&&(call(c,0x43fa90u,{})||result_input_4bfb20(c)))result_next(c);
    mode_escape(c);
}
void mode25_exit_49a920(PcRaceContext& c){
    call(c,0x4401d0u,{0x188u});
    if(call(c,0x43f8e0u,{})!=0x1bu&&call(c,0x43f8e0u,{})!=0x1au){call(c,0x42dfb0u,{0x34u});call(c,0x4299c0u,{0x34u});}
}

// ---- mode 27 ------------------------------------------------------------
void mode27_init_49adc0(PcRaceContext& c){
    auto& m=c.m;
    call(c,0x43f900u,{0xf0u});
    call(c,0x42e020u,{0u,0u,0xf0u});
    call(c,0x455710u,{});
    if(!(call(c,0x43f860u,{})&0xffu)&&(call(c,0x450240u,{})==6u||call(c,0x450240u,{})==7u)){
        const std::uint32_t player=m.u32(0x799d18u);
        m.put8(0x836ce0u,0);
        call(c,0x427630u,{});call(c,0x428600u,{});
        suspend_race(c,player);
        adv_release_45af40(c);
    }
    const std::uint32_t v=m.u32(0x780258u);
    if(v==3u||v==4u)call(c,0x4edce0u,{0x10u},0x659930u);
    m.put32(0x67ee3cu,1u);                                    // tail 498400
}
void mode27_control_49aea0(PcRaceContext& c){
    if(call(c,0x43fa90u,{}))call(c,0x43f8c0u,{0x1cu});
    mode_escape(c);
}
void mode27_exit_49aed0(PcRaceContext& c){
    auto& m=c.m;
    const std::uint32_t preset=m.u32(0x78024cu),variant=m.u32(0x780258u);
    call(c,0x401030u,{0u});call(c,0x401030u,{1u});
    const std::uint32_t book=PcRaceEndState::BookBase;       // [63960C]
    m.put16(book+0x34u,std::uint16_t(m.u16(book+0x34u)+1u));  // 452C50
    auto hi=[&](std::uint32_t v){return std::uint32_t(std::uint16_t(v));};
    const std::uint32_t k1=preset;                            // 452EF0 = ax [7D6728], 452CB0(ax, preset)
    {const std::uint32_t a=hi(m.u16(0x7d6728u)),b=hi(k1);m.put32(book+(a+b*12u)*4u+0x40u,m.u32(0x7d675cu));}
    {const std::uint32_t a=hi(m.u16(0x7d6728u));m.put32(book+a*4u+0xa0u,m.u32(0x7d6724u));}   // 452D00(452EF0()) (push 0 is 452EF0's unused argument)
    {const std::uint32_t a=hi(variant),b=hi(preset);m.put32(book+(a+b*12u)*4u+0x50u,m.u32(0x7d6760u));}   // 452D90(variant, preset)
    {const std::uint32_t a=hi(variant),b=hi(preset);m.put32(book+(a+(b+2u)*12u)*4u,m.u32(0x7d6768u));}    // 452E00(variant, preset)
    bookkeeping_453370(c);
    {const std::uint32_t add=call(c,0x48b1f0u,{})+m.u32(0x8367f4u);m.put32(book+0x3cu,m.u32(book+0x3cu)+add);}   // 452C90
    {const std::uint32_t v=m.u32(0x780258u);
     if(v==3u||v==4u){const std::uint32_t add=call(c,0x48b1f0u,{})+m.u32(0x8367f4u);m.put32(book+0xc0u,m.u32(book+0xc0u)+add);}}   // 453050
    // 453330: its body runs only when 47F110 (`xor eax,eax; ret`) is nonzero.
    race_events_close_49ad00(c);
    call(c,0x42dfb0u,{0x34u});call(c,0x4299c0u,{0x34u});
}

// ---- mode 28 ------------------------------------------------------------
void mode28_init_49b170(PcRaceContext& c){
    c.m.put32(0x836d10u,0);
    call(c,0x428600u,{});
    call(c,0x401030u,{0u});
}
void mode28_control_49af90(PcRaceContext& c){
    auto& m=c.m;
    if(m.u32(0x8a8cdcu))return;
    const std::uint32_t stage=m.u32(0x836d10u);
    if(stage>3u)return;
    if(stage==0u){
        race_events_close_49ad00(c);
        if(!(call(c,0x43f860u,{})&0xffu)&&m.u32(0x65994cu)==0u){common_anim_4b72f0(c,1u);m.put32(0x836d10u,1u);return;}
        call(c,0x4999d0u,{});m.put32(0x836d10u,1u);return;
    }
    if(stage==1u){
        call(c,0x44fd00u,{});call(c,0x42ebf0u,{});call(c,0x4489f0u,{});
        release_course(c);
        call(c,0x42dfd0u,{});call(c,0x429a10u,{});call(c,0x47ed40u,{});call(c,0x465fa0u,{});
        adv_release_45af40(c);
        call(c,0x451a00u,{});call(c,0x4276b0u,{});
        const std::uint32_t v=m.u32(0x780258u);
        if(v==6u||v==5u){call(c,0x496170u,{});call(c,0x4e85e0u,{});m.put32(0x836d10u,2u);return;}
    }else if(stage==2u){
        if(!(call(c,0x496180u,{})&0xffu))return;
        if(!(call(c,0x4e8620u,{})&0xffu))return;
        m.put32(0x836d10u,3u);return;
    }else if(stage==3u){
        std::uint32_t next;
        if(call(c,0x43f860u,{})&0xffu)next=call(c,0x499890u,{})?0x24u:0x20u;
        else next=m.u32(0x65994cu)?0x20u:0xau;
        call(c,0x43f8c0u,{next});
        call(c,0x450230u,{0u});
        call(c,0x455730u,{});
    }
    m.put32(0x836d10u,3u);
}
void race_events_close_49ad00(PcRaceContext& c){
    call(c,0x440330u,{8u,0x18u});call(c,0x440330u,{0x16au,0x15u});   // VM at 49AD0B = push 0x16A
    for(std::uint32_t id:{0x18au,0x1u,0x167u,0x181u,0x186u,0x187u,0x18du,0x184u,0x185u,0x17fu,0x183u,0x191u,0x190u,0x192u,0x193u,0x194u})
        call(c,0x4401d0u,{id});
}

// ---- event 0x188 function 0x15: route map --------------------------------
void route_init_4ae750(PcRaceContext& c){
    auto& m=c.m;
    std::uint32_t token;
    const std::int8_t bl=std::int8_t(call(c,0x48b140u,{}));
    if(bl>=0&&(call(c,0x48b160u,{})&0xffu)){
        const std::int32_t e=bl;
        const std::uint32_t row=m.u32(0x5c4b80u+std::uint32_t(e)*4u);
        token=m.u32(0x5c48b0u+(row*0x1eu+std::uint32_t(e))*4u);
    }else token=0x340017u;
    m.put32(0x8420fcu,0);
    for(std::uint32_t a=0x842054u,b=0x8420b0u;a<0x8420b4u;a+=8u,b+=4u){m.putf(a-4u,K628180);m.putf(a,K628180);m.put32(b,0);}
    const std::uint32_t x0=m.u32(0x5c4838u),y0=m.u32(0x5c483cu);
    m.put32(0x842048u,x0);m.put32(0x84204cu,y0);m.put32(0x842050u,x0);
    const std::uint32_t x1=m.u32(0x5c4848u);
    m.put32(0x842054u,y0);
    const std::uint32_t y1=m.u32(0x5c484cu);
    m.put32(0x8420e0u,0);m.put32(0x8420e4u,0);m.put32(0x8420e8u,0x28u);
    m.put32(0x842058u,x1);m.put32(0x84205cu,y1);
    m.put32(0x8420f4u,0xffffffffu);
    std::uint32_t node=0,out=0x842068u;
    for(std::uint32_t leg=0;leg<4u;++leg,out+=0x10u){
        const std::uint32_t r=call(c,0x451350u,{leg});
        if(r==1u){node=m.u32(0x5c4754u+node*16u);m.put32(0x8420b4u+leg*8u,2u);}
        else if(r==0u){node=m.u32(0x5c4750u+node*16u);m.put32(0x8420b4u+leg*8u,1u);}
        else{m.put32(0x8420b4u+leg*8u,0xffffffffu);break;}
        const std::uint32_t from=m.u32(0x5c4748u+node*16u),to=m.u32(0x5c474cu+node*16u);
        m.put32(out-8u,m.u32(0x5c4830u+from*8u));m.put32(out-4u,m.u32(0x5c4834u+from*8u));
        m.put32(out,m.u32(0x5c4830u+to*8u));m.put32(out+4u,m.u32(0x5c4834u+to*8u));
    }
    const std::uint32_t preset=m.u32(0x78024cu);
    m.put32(0x8420f0u,token);
    if(preset!=0u&&preset!=2u){call(c,0x428320u,{0x34000du,1u,0u});call(c,0x428320u,{0x340019u,5u,0u});}
    else{call(c,0x428320u,{0x34000eu,1u,0u});call(c,0x428320u,{0x34001au,5u,0u});}
    call(c,0x428320u,{0x34001du,6u,1u});
    m.put32(0x8420ecu,create_428440(c,token,4u,0x5bu,0x96u));
    Locals l(m);
    translation(m,PcRaceEndLocals,m.f32(0x842048u),m.f32(0x84204cu),0.0f);
    call(c,0x4287b0u,{m.u32(0x8420ecu),PcRaceEndLocals});
}
void route_control_4ae960(PcRaceContext& c){
    auto& m=c.m;
    if(m.u32(0x8420fcu))return;
    std::uint32_t step=m.u32(0x8420e0u);
    if(K5c4bf8>m.f32(0x842058u+step*8u)){
        m.put32(0x842058u+step*8u,m.u32(0x842050u+step*8u));
        m.put32(0x84205cu+step*8u,m.u32(0x842054u+step*8u));
        const std::uint32_t h=m.u32(0x8420f4u);
        m.put32(0x8420fcu,1u);
        call(c,0x428770u,{h,2u});
        step=m.u32(0x8420e0u);
    }
    std::int32_t level=m.i32(0x6840f0u);
    std::int32_t count=m.i32(0x8420e8u),pos=m.i32(0x8420e4u);
    if(level!=-1){
        level=m.u32(0x6840ecu)==0u?level*2:level*2+1;
        if(!(std::int32_t(step)<level)){
            const float t=float(pos)/float(count);
            if(t>=m.f32(0x6840f4u)||std::int32_t(step)>level){
                const std::uint32_t h=m.u32(0x8420ecu);
                m.put32(0x8420fcu,1u);
                call(c,0x428770u,{h,2u});
                call(c,0x428770u,{m.u32(0x8420f4u),2u});
                return;
            }
        }
    }
    Locals l(m);
    float speed=0.0f;bool speed_set=false;
    if(pos==count-0xe&&!(K5c4bf8>m.f32(0x842060u+step*8u))&&m.u32(0x8420fcu)==0u){
        std::uint32_t first,last;
        switch(m.u32(0x8420b4u+step*4u)){
        case 0:
            if(m.u32(0x8420b0u+step*4u)==2u){speed=K6280c4;first=0x97u;last=0xa5u;}
            else{speed=K62806c;first=0x3du;last=0x4bu;}
            break;
        case 1:speed=K6280c4;first=0x3du;last=0x4bu;break;
        case 2:speed=K62806c;first=0x97u;last=0xa5u;break;
        default:throw PcRaceEndUndefined{0x4aeaefu};    // reads the uninitialised [esp+0xC]
        }
        speed_set=true;
        call(c,0x4285a0u,{m.u32(0x8420ecu)});
        const std::uint32_t h=create_428440(c,m.u32(0x8420f0u),4u,first,last);
        m.put32(0x8420ecu,h);
        call(c,0x428800u,{h,bits(speed)});
        count=m.i32(0x8420e8u);pos=m.i32(0x8420e4u);step=m.u32(0x8420e0u);
    }
    (void)speed_set;
    const float t=float(pos)/float(count);
    const float dy=m.f32(0x84205cu+step*8u)-m.f32(0x842054u+step*8u);
    const float dx=m.f32(0x842058u+step*8u)-m.f32(0x842050u+step*8u);
    const float x=dx*t+m.f32(0x842050u+step*8u);
    const float y=dy*t+m.f32(0x842054u+step*8u);
    const std::uint32_t moved=step;
    translation(m,PcRaceEndLocals,x,0.0f-y,0.0f);
    call(c,0x4287b0u,{m.u32(0x8420ecu),PcRaceEndLocals});
    if(m.u32(0x8420e4u)==0u&&std::int32_t(moved)<9){
        std::uint32_t first,last;
        switch(m.u32(0x8420b0u+m.u32(0x8420e0u)*4u)){
        case 1:first=0x29u;last=0x50u;break;
        case 2:first=0x51u;last=0x78u;break;
        default:first=1u;last=0x28u;break;
        }
        const std::uint32_t h=call(c,0x428460u,{0x34001bu,2u,1u,first,last});
        call(c,0x4287b0u,{h,PcRaceEndLocals});
        m.put32(0x8420f4u,h);
        if(moved!=0u&&std::int32_t(moved)%2==0){
            const std::uint32_t mark=call(c,0x428320u,{0x34001cu,3u,0u});
            call(c,0x4287b0u,{mark,PcRaceEndLocals});
        }
    }
    const std::int32_t next=m.i32(0x8420e4u)+1;
    m.put32(0x8420e4u,std::uint32_t(next));
    if(next>=m.i32(0x8420e8u)-1){
        const std::uint32_t s=m.u32(0x8420e0u)+1u;
        m.put32(0x8420e0u,s);
        std::uint32_t first,last;
        switch(m.u32(0x8420b0u+s*4u)){
        case 0:first=0x5bu;last=0x95u;break;
        case 1:first=1u;last=0x3cu;break;
        case 2:first=0xb5u;last=0xf0u;break;
        default:return;
        }
        const std::uint32_t p=0x842050u+s*8u;
        call(c,0x4285a0u,{m.u32(0x8420ecu)});
        const std::uint32_t h=create_428440(c,m.u32(0x8420f0u),4u,first,last);
        m.put32(0x8420ecu,h);
        m.put32(0x8420e4u,0);
        translation(m,PcRaceEndLocals,m.f32(p),0.0f-m.f32(p+4u),0.0f);
        call(c,0x4287b0u,{m.u32(0x8420ecu),PcRaceEndLocals});
    }
}
void route_destroy_4aed20(PcRaceContext& c){
    auto& m=c.m;
    call(c,0x428600u,{});
    m.put32(0x8420fcu,0);
    m.put32(0x6840ecu,0xffffffffu);m.put32(0x6840f0u,0xffffffffu);m.putf(0x6840f4u,K6280c4);
}

// ---- event 0x188 function 0x50: Time Attack result ------------------------
void ta_result_init_4aed50(PcRaceContext& c){
    auto& m=c.m;
    const std::int8_t course=std::int8_t(call(c,0x48b140u,{}));
    m.put32(0x8420fcu,0);
    if(m.u32(0x78024cu)==2u){call(c,0x428320u,{0x340001u,1u,0u});call(c,0x428320u,{0x34000cu,5u,0u});}
    else{call(c,0x428320u,{0x340000u,1u,0u});call(c,0x428320u,{0x34000bu,5u,0u});}
    call(c,0x428320u,{0x34001du,6u,1u});
    std::int32_t last=0;
    if(call(c,0x4506a0u,{0xeu}))last=0x1fe;
    else{
        const std::int32_t level=m.i32(0x6840f0u);
        for(std::int32_t i=0;i<level;++i)last+=(i==5||i==10)?0x3c:0x1e;
        const std::int32_t n=level+1;
        if(m.u32(0x6840ecu)==0u){
            if(level==0xe)last+=cvtt(m.f32(0x6840f4u)*K628100);
            else last+=cvtt(m.f32(0x6840f4u)*((n==5||n==10)?K5c6900:K5c4bfc));
        }else{
            const float base=(n==5||n==10)?K5c6900:K5c4bfc;
            const float scale=(n==5||n==10)?K5c1854:K5b4434;
            last+=cvtt(m.f32(0x6840f4u)*scale+base);
        }
    }
    const std::uint32_t token=m.u32(0x5c4b08u+std::uint32_t(std::int32_t(course))*4u);
    m.put32(0x8420f8u,call(c,0x428460u,{token,4u,1u,0u,std::uint32_t(last)}));
}
void ta_result_control_4aeec0(PcRaceContext& c){
    if(call(c,0x428880u,{c.m.u32(0x8420f8u)})==3u)c.m.put32(0x8420fcu,1u);
}
void ta_result_destroy_4aeee0(PcRaceContext& c){
    call(c,0x428600u,{});
    c.m.put32(0x8420fcu,0);
}

// ---- SUMO_FE flow: mode 34 (OutRun Miles) -----------------------------------
void mode34_init_49b470(PcRaceContext& c){
    auto& m=c.m;
    call(c,0x428600u,{});
    call(c,0x43f900u,{0xf0u});
    call(c,0x440a10u,{0x16au,0x15u});
    miles_compute_4ef550(c,m.u32(0x780258u));
    if(call(c,0x450240u,{})==6u||call(c,0x450240u,{})==7u){
        const std::uint32_t player=m.u32(0x799d18u);
        m.put8(0x836ce0u,0);
        call(c,0x427630u,{});call(c,0x428600u,{});
        suspend_race(c,player);
        adv_release_45af40(c);
        sounds_49a130(c);
        call(c,0x427700u,{0x1a1u});
    }
    m.put32(0x836d1cu,0);m.put32(0x836d20u,0);
    m.put32(0x67f638u,call(c,0x428320u,{0x2b004du,1u,5u}));
    m.put8(0x836d18u,0);
}
void mode34_control_49b570(PcRaceContext& c){
    auto& m=c.m;
    std::uint32_t score=m.u32(0x84bcf4u);                      // 4EF4E0
    std::uint8_t was=m.u8(0x836d18u);
    if(call(c,0x428880u,{m.u32(0x67f638u)})==4u){
        m.put8(0x836d18u,0);
        for(std::uint32_t d=0;d<8u;++d){
            const std::uint32_t digit=score%10u;score/=10u;
            const std::uint32_t at=0x836d1cu+d;
            if(std::uint32_t(std::int32_t(std::int8_t(m.u8(at))))<digit*6u){m.put8(at,std::uint8_t(m.u8(at)+1u));m.put8(0x836d18u,1u);}
        }
    }
    const bool ended=was==1u&&m.u8(0x836d18u)==0u;          // bl
    if(!call(c,0x427700u,{0x1a1u})&&ended)call(c,0x424940u,{0x217u});
    if(call(c,0x427700u,{0x1a1u}))return;
    if(m.u32(0x780258u)==4u){if(call(c,0x43fa90u,{}))call(c,0x43f8c0u,{0x1bu});return;}
    if(!call(c,0x43fa90u,{})&&!call(c,0x4536f0u,{4u})&&!call(c,0x4536f0u,{8u}))return;
    const std::uint32_t s=call(c,0x450240u,{});
    std::uint32_t next;
    if(s==2u)next=0x23u;
    else if(s==6u)next=0x1eu;
    else if(s==7u)next=0x1bu;
    else if(call(c,0x46c500u,{}))next=0x1bu;
    else if(!(call(c,0x48b310u,{})&0xffu))next=0x23u;
    else if(!(call(c,0x48b350u,{})&0xffu))next=0x23u;
    else next=0x1bu;
    call(c,0x43f8c0u,{next});
}
void mode34_exit_49b7c0(PcRaceContext& c){
    auto& m=c.m;
    call(c,0x4285a0u,{m.u32(0x67f638u)});
    m.put32(0x67f638u,0xffffffffu);
    miles_commit_4ef510(c);
}

// ---- mode 35 (continue: retry / exit) ---------------------------------------
void mode35_init_49b7e0(PcRaceContext& c){
    auto& m=c.m;
    call(c,0x43f900u,{1u});
    std::uint32_t token=0x2b004au;
    if(call(c,0x4957f0u,{})&0xffu){
        const std::int32_t mission=std::int32_t(call(c,0x496510u,{}));
        token=0x2b0066u;
        if((call(c,0x495b20u,{})==1u||(call(c,0x495b80u,{})&0xffu))&&mission>=4&&mission!=7)token=0x2b0067u;
    }
    m.put32(0x67f63cu,call(c,0x428320u,{token,1u,5u}));
    m.put32(0x836d28u,0);
}
void mode35_control_49d9c0(PcRaceContext& c){
    auto& m=c.m;
    bool second=false;
    if((call(c,0x4957f0u,{})&0xffu)&&(call(c,0x4959e0u,{})&0xffu))second=true;
    const std::uint32_t stage=m.u32(0x836d28u);
    if(stage==0u){
        if(call(c,0x428880u,{m.u32(0x67f63cu)})!=4u)return;
        const std::uint32_t token=((call(c,0x4957f0u,{})&0xffu)&&second)?0x2b0069u:0x2b0068u;
        m.put32(0x67f640u,call(c,0x428320u,{token,1u,5u}));
        m.put32(0x836d28u,m.u32(0x836d28u)+1u);
        return;
    }
    if(stage!=1u)return;
    if(call(c,0x428880u,{m.u32(0x67f640u)})!=4u)return;
    if(m.i32(call(c,0x4035f0u,{})+0x518u)>0)return;
    bool retry;
    if((call(c,0x4957f0u,{})&0xffu)&&second){
        if(call(c,0x4536f0u,{4u})){call(c,0x495a60u,{});retry=true;}
        else retry=call(c,0x4536f0u,{0x10u})!=0u;
    }else retry=call(c,0x4536f0u,{4u})!=0u;
    if(retry){call(c,0x43f900u,{0u});call(c,0x43f8c0u,{0x1eu});call(c,0x4249f0u,{0x40u});}
    if(call(c,0x4536f0u,{8u})){call(c,0x43f900u,{0u});call(c,0x43f8c0u,{0x1cu});call(c,0x4249f0u,{0x40u});}
}
void mode35_exit_49dae0(PcRaceContext& c){
    auto& m=c.m;
    call(c,0x4285a0u,{m.u32(0x67f63cu)});
    call(c,0x4285a0u,{m.u32(0x67f640u)});
    if(m.u8(0x836ce0u)){race_close_49cdf0(c);m.put8(0x836ce0u,0);}
    mode27_exit_49aed0(c);
}

// ---- mode 36 (C2C reward, 4981A0 / 4981F0 / 4983D0; read from the Steam build) ----
namespace {
// 5C1E58: nine 0x48-byte reward records, indexed by [67EE4C] (499730): +00 picture,
// +0C animated picture {token, a, b}, +18 / +24 extra sprites, +30 / +3C text {id, y, -}.
alignas(4) static std::uint32_t RewardRecords[9][18]{}; OR2_EXE_COPY(RewardRecords,0x5C1E58u,0x288u);
}
const std::uint32_t* mode36_reward_record(std::int32_t r){return r>=0&&r<=8?RewardRecords[r]:nullptr;}
namespace {
const std::uint32_t* reward_record(PcRaceMemory& m){
    const std::int32_t r=m.i32(0x67ee4cu);
    if(r<0||r>8)throw std::out_of_range("mode 36: reward index [67EE4C] outside 5C1E58");
    return RewardRecords[r];
}
}
void mode36_init_4981a0(PcRaceContext& c){
    auto& m=c.m;
    for(std::uint32_t a:{0x8366b0u,0x8366b4u,0x8366b8u,0x8366bcu})m.put32(a,0xffffffffu);
    m.put32(0x8366acu,0xc8u);
    call(c,0x42deb0u,{0x42u,8u});call(c,0x429920u,{0x42u,9u});call(c,0x427700u,{0x80a6u});
    m.put32(0x67ee50u,0);
}
void mode36_control_4981f0(PcRaceContext& c){
    auto& m=c.m;
    const std::uint32_t stage=m.u32(0x67ee50u);
    if(stage==0u){
        if(!call(c,0x42df90u,{}))return;
        if(!call(c,0x4299a0u,{}))return;
        if(call(c,0x427700u,{0x80a6u}))return;
        call(c,0x4999f0u,{});call(c,0x440110u,{0x199u,0x53u});
        const auto* r=reward_record(m);
        m.put32(0x67ee50u,1u);
        if(std::int32_t(r[0])>=0)m.put32(0x8366b0u,call(c,0x428320u,{r[0],5u,5u}));
        if(std::int32_t(r[6])>=0)m.put32(0x8366b8u,call(c,0x428320u,{r[6],6u,5u}));
        if(std::int32_t(r[9])>=0)m.put32(0x8366bcu,call(c,0x428320u,{r[9],7u,5u}));
        call(c,0x4249f0u,{0x16bu});call(c,0x401000u,{0u,0x1cu,1u});
        return;
    }
    if(stage==1u){
        const std::int32_t n=m.i32(0x8366acu);
        if(n>0){m.put32(0x8366acu,std::uint32_t(n-1));return;}
        m.put32(0x67ee50u,2u);return;
    }
    if(stage!=2u)return;
    const auto* r=reward_record(m);
    if(std::int32_t(r[3])>=0&&m.i32(0x8366b4u)<0&&call(c,0x428880u,{m.u32(0x8366b0u)})==4u){
        call(c,0x4285a0u,{m.u32(0x8366b0u)});m.put32(0x8366b0u,0xffffffffu);
        m.put32(0x8366b4u,call(c,0x428460u,{r[3],5u,0u,r[4],r[5]}));
    }
    if(!call(c,0x4536f0u,{4u}))return;
    for(std::uint32_t a=0x8366b0u;a<0x8366c0u;a+=4u)
        if(m.i32(a)>=0){call(c,0x4285a0u,{m.u32(a)});m.put32(a,0xffffffffu);}
    call(c,0x4249f0u,{0x40u});call(c,0x4278c0u,{0x80a6u});call(c,0x401030u,{0u});call(c,0x43f8c0u,{0x20u});
    call(c,0x4999d0u,{});
}
void mode36_exit_4983d0(PcRaceContext& c){
    call(c,0x42dfd0u,{});call(c,0x429a10u,{});call(c,0x4401d0u,{0x199u});
    c.m.put32(0x67ee4cu,0xffffffffu);c.m.put32(0x67ee50u,0xffffffffu);
}

// ---- mode 30 (retry) ----------------------------------------------------------
void mode30_control_49b190(PcRaceContext& c){
    auto& m=c.m;
    if(m.u32(0x8a8cdcu))return;
    const std::uint32_t stage=m.u32(0x836d10u);
    if(stage==0u){
        race_events_close_49ad00(c);
        m.put32(0x67f614u,create_428440(c,0x480000u,3u,0x28u,0x3bu));
        m.put32(0x836d10u,1u);
    }else if(stage==1u){
        call(c,0x44fd00u,{});call(c,0x42ebf0u,{});call(c,0x4489f0u,{});
        release_course(c);
        call(c,0x42dfd0u,{});call(c,0x429a10u,{});call(c,0x47ed40u,{});call(c,0x465fa0u,{});
        adv_release_45af40(c);
        call(c,0x451a00u,{});call(c,0x4276b0u,{});
        m.put32(0x836d10u,2u);
    }else if(stage==2u){
        call(c,0x4999f0u,{});
        call(c,0x43f8c0u,{0xdu});
        call(c,0x450230u,{0u});
        call(c,0x455730u,{});
    }
}

// ---- 49CDF0: close the race after a goal --------------------------------------
void race_close_49cdf0(PcRaceContext& c){
    auto& m=c.m;
    if(m.u32(m.u32(0x8369b4u)+8u))throw PcRaceEndUndefined{0x49cdfcu};   // goal-scene exit pointer (8369C8 table: never written)
    call(c,0x440a30u,{0x182u,1u});
    call(c,0x428600u,{});
    call(c,0x42dfb0u,{0x3cu});call(c,0x4299c0u,{0x3cu});
    call(c,0x440330u,{8u,0x18u});call(c,0x440330u,{0x16au,0x15u});
    for(std::uint32_t id:{0x18au,0x167u,0x181u,0x186u,0x187u,0x18du,0x184u,0x17fu,0x183u,0x6u})call(c,0x4401d0u,{id});
    model_release_499bb0(c);
    for(std::uint32_t id:{0xeu,0xbfu,0xc4u,0xcau,0xbeu,0x1ebu,0x1ecu,0xc6u,0xc7u,0xcbu,0xc5u})call(c,0x448990u,{id});
    {const std::uint32_t p=m.u32(0x78024cu);call(c,0x448990u,{(p==1u||p==3u)?0xc3u:0x12bu});}
    call(c,0x448990u,{0x67u});
    call(c,0x448990u,{call(c,0x44dbb0u,{0xcu})});
    call(c,0x448990u,{call(c,0x44dbb0u,{0xdu})});
    call(c,0x448990u,{call(c,0x44c500u,{})});
    call(c,0x448990u,{call(c,0x44c510u,{})});
    for(std::uint32_t i=0;i<4;++i)call(c,0x43de50u,{i});
    call(c,0x44c3d0u,{});
    for(std::uint32_t i=0;i<3;++i)call(c,0x4f11b0u,{i});
    for(std::uint32_t i=0;i<3;++i)call(c,0x4f0600u,{i});
    call(c,0x46fc30u,{0u});call(c,0x46fc30u,{1u});
    call(c,0x4276b0u,{});
    adv_release_45af40(c);
}
void model_release_499bb0(PcRaceContext& c){
    auto& m=c.m;
    for(std::uint32_t id=0;id<0x2cu;++id){
        if(!m.u8(0x8367c0u+id))continue;
        std::uint32_t k=0;
        for(;;++k){
            const std::int32_t e=m.i32(0x5c2258u+k*8u);       // reading past the table raises PcRaceUnmapped
            if(e>=0&&std::uint32_t(e)==id)break;
        }
        call(c,0x448990u,{m.u32(0x5c225cu+k*8u)});
        m.put8(0x8367c0u+id,0);
    }
}

// ---- OutRun Miles --------------------------------------------------------------
void miles_init_4ef280(PcRaceContext& c){
    auto& m=c.m;
    m.put32(0x84bcb8u,1u);
    call(c,0x4f12a0u,{0x5cee0cu,0x84bcb0u,0u,0u});
    call(c,0x4f1a90u,{0x5cee04u,0x84bcb0u});
    for(std::uint32_t a=0x84b920u;a<0x84b920u+0xcfu*4u;a+=4u)m.put32(a,0);
    for(std::uint32_t a=0x84b920u,v=0;a<0x84bc5cu;a+=4u,v+=10u)m.put32(a,v);
    const std::uint32_t setup=call(c,0x4f1a90u,{0x5cee04u,0x84bcb0u});
    const std::uint32_t prices=call(c,0x4f1a90u,{0x5cedfcu,0x84bcb0u});
    const std::int32_t n=std::int32_t(call(c,0x4f1210u,{prices,0x84bcb0u}));
    for(std::int32_t i=0;i<n;++i)m.put32(0x84b920u+m.u32(prices+std::uint32_t(i)*8u)*4u,m.u32(prices+std::uint32_t(i)*8u+4u));
    static constexpr std::uint32_t Pair[7]{0x84b918u,0x84b914u,0x84b910u,0x84b90cu,0x84b908u,0x84b904u,0x84b900u};
    for(std::uint32_t k=0;k<7u;++k){const std::uint32_t v=m.u32(setup+k*4u);m.put32(Pair[k],v);m.put32(Pair[k]+0x390u,v);}
    m.put32(0x84b91cu,0);m.put32(0x84bcacu,0);
    m.put32(0x84bc60u,m.u32(setup+0x1cu));m.put32(0x84bc64u,m.u32(setup+0x24u));m.put32(0x84bc78u,m.u32(setup+0x2cu));
    m.put32(0x84bc7cu,m.u32(setup+0x3cu));m.put32(0x84bc80u,m.u32(setup+0x34u));m.put32(0x84bcc4u,m.u32(setup+0x20u));
    m.put32(0x84bcc8u,m.u32(setup+0x28u));m.put32(0x84bcdcu,m.u32(setup+0x30u));
    const std::uint32_t data=m.u32(0x84bcb4u);
    m.put32(0x84bce0u,m.u32(setup+0x40u));
    m.put32(0x84bce4u,m.u32(setup+0x38u));
    m.put32(0x84bcb0u,0);
    call(c,0x580c38u,{data});
    m.put32(0x84bcb4u,0);m.put32(0x84bcf4u,0);m.put32(0x84bcb8u,1u);
}
void miles_compute_4ef550(PcRaceContext& c,std::uint32_t variant){
    using driving::X87;
    auto& m=c.m;
    const std::uint32_t car=m.u32(0x799d18u);
    m.put32(0x84bcf4u,0);
    auto add=[&](std::uint32_t v){m.put32(0x84bcf4u,m.u32(0x84bcf4u)+v);};
    auto ftol=[](X87 v){return std::uint32_t(driving::x87_ftol64(v));};   // _ftol2, EAX
    if(call(c,0x44ff10u,{})&&variant==6u){
        const std::uint32_t a=call(c,0x496510u,{}),b=call(c,0x495890u,{});
        const std::int32_t d=m.i32(0x84b900u+a*4u)-m.i32(0x84b900u+b*4u);
        add(d<0?0u:std::uint32_t(d));
    }else if(variant==4u){
        const float v=m.f32(0x84bd00u)*50.0f;                   // 5B4358
        const float kept=100.0f>v?100.0f:v;                     // 6282CC: comiss/ja
        m.putf(0x84bd00u,0.0f);
        add(ftol(X87(kept)));
    }
    if(m.u8(0x84bcfcu)){
        add(m.u32(0x84bc5cu+variant*4u));
        if(variant==1u||variant==9u)add(std::uint32_t(std::int32_t(call(c,0x4b99d0u,{car}))/1000));
        else if(variant==2u)add(ftol(X87(std::int32_t(call(c,0x45b820u,{})))*X87(9.999999747378752e-05f)*X87(0.4000000059604645f)));
        m.put8(0x84bcfcu,0);
    }else{
        if(variant==1u||variant==9u)add(std::uint32_t(std::int32_t(call(c,0x4b99d0u,{car}))/3000));
        else if(variant==2u)add(ftol(X87(std::int32_t(call(c,0x45b820u,{})))*X87(9.999999747378752e-05f)*X87(0.10000000149011612f)));
    }
    m.put32(0x84bcf4u,m.u32(0x84bcf4u)+m.u32(0x84bcf8u));
    if(!call(c,0x55a930u,{},0x7f9460u)){
        const std::uint32_t q=m.u32(car+0xe78u)/60u;
        m.put32(0x84bcf4u,m.u32(0x84bcf4u)+q*5u);
    }
    m.put32(car+0xe78u,0);
}
void miles_commit_4ef510(PcRaceContext& c){
    using driving::X87;
    auto& m=c.m;
    m.put8(0x7c27d4u,std::uint8_t(m.u8(0x7c27d4u)|2u));
    const std::int32_t score=m.i32(0x84bcf4u);
    X87 v=X87(score);
    if(score<0)v=v+X87(4294967296.0f);                         // 628070
    v=v+X87(m.f32(0x7c2404u));
    m.put32(0x84bcf4u,0);
    m.putf(0x7c2404u,driving::x87_float(v));
    m.put32(0x84bcf8u,0);
}
// ---- mode 19 (GOAL) ------------------------------------------------------------
void bookkeeping_452dd0(PcRaceMemory& m,std::uint32_t variant,std::uint32_t preset){
    const std::uint32_t a=std::uint16_t(variant),b=std::uint16_t(preset);
    m.put32(0x7d6760u,m.u32(PcRaceEndState::BookBase+(a+b*12u)*4u+0x50u)+1u);
}
void bookkeeping_452e60(PcRaceMemory& m,std::uint32_t variant,std::uint32_t preset,std::uint32_t time){
    using driving::X87;
    auto unsigned_value=[](std::uint32_t v){X87 x=X87(std::int32_t(v));if(std::int32_t(v)<0)x=x+X87(4294967296.0f);return x;};   // fild + 628070
    const X87 count=unsigned_value(m.u32(0x7d6760u));
    const std::uint32_t a=std::uint16_t(variant),b=std::uint16_t(preset);
    const X87 average=unsigned_value(m.u32(PcRaceEndState::BookBase+(a+(b+2u)*12u)*4u));
    const X87 sum=average*(count-X87(1.0f))+unsigned_value(time);
    m.put32(0x7d6768u,std::uint32_t(driving::x87_ftol64(sum/count)));
}
// 4871A0(scene): the goal camera of the override 82E7C0.. (script 653788[scene]).
void goal_camera_4871a0(PcRaceContext& c,std::uint32_t scene){
    auto& m=c.m;
    m.put8(0x82e7d4u,std::uint8_t(scene));
    std::uint32_t rec=m.u32(0x653788u+std::uint32_t(std::uint8_t(scene))*4u);
    m.put16(0x82e7c4u,0);m.put32(0x82e7c8u,0);m.put16(0x82e7dcu,0);m.put16(0x82e7ccu,0);
    const std::uint32_t player=m.u32(0x799d18u);
    m.put32(0x82e7d8u,1u);m.putf(0x82e7e0u,0.0f);m.putf(0x82e7e8u,0.0f);m.put32(0x82e7e4u,2u);m.put32(0x82e7d0u,1u);m.put32(0x82e7c0u,1u);
    while(m.u32(rec)!=0x1fu){m.put32(rec+0x28u,1u);rec+=0x2cu;}
    m.put32(player+0x2f0u,(m.u32(player+0x2f0u)&0xffffffb9u)|0x38u);
}
void mode19_init_49c9c0(PcRaceContext& c){
    auto& m=c.m;
    const std::uint32_t variant=m.u32(0x780258u);
    call(c,0x443ea0u,{},call(c,0x4035f0u,{}));
    call(c,0x43f900u,{0x4b0u});
    call(c,0x427630u,{});
    sounds_49a130(c);
    const std::uint32_t v2=m.u32(0x780258u),preset=m.u32(0x78024cu);
    for(std::uint32_t i=0;i<5u;++i)call(c,0x4505f0u,{i});     // float results popped
    bookkeeping_452dd0(m,v2,preset);
    bookkeeping_452e60(m,v2,preset,call(c,0x451180u,{0u}));
    call(c,0x450320u,{});                                      // its result goes to 49A650 (RET)
    model_release_499bb0(c);
    call(c,0x448990u,{0xeu});
    std::uint32_t goal=0;
    if(call(c,0x44b7b0u,{0u})&0xffu)goal=call(c,0x450380u,{8u})-0xau;
    std::uint32_t scene=3u;
    bool camera=true;
    if(variant==6u){
        if(call(c,0x495b20u,{})==6u||call(c,0x495b20u,{})==1u){
            if(call(c,0x495820u,{})&0xffu){
                suspend_race(c,m.u32(0x799d18u));
                camera=false;
            }
        }
    }else if(variant!=5u&&variant!=4u&&m.u32(0x635f34u)==0x42u&&variant!=7u)
        scene=m.u8(0x5c2370u+(goal+m.u32(0x78024cu)*5u)*4u);
    if(camera){
        goal_camera_4871a0(c,scene);
        if(variant!=3u&&variant!=4u&&variant!=6u&&!(call(c,0x43f860u,{})&0xffu)&&!call(c,0x4493c0u,{}))call(c,0x4249f0u,{0x16bu});
        switch(variant){
        case 0:{
            const std::uint32_t mine=0u;                           // 47F110(1) = xor eax,eax
            call(c,0x401000u,{0u,mine>call(c,0x451180u,{0u})?0x41u:0x3du,0u});
            break;}
        case 1:call(c,0x401000u,{0u,m.u32(0x5c23d4u+goal*4u),0u});break;
        case 2:call(c,0x401000u,{0u,m.u32(0x5c23e8u+goal*4u),0u});call(c,0x43f900u,{0x258u});break;
        case 3:case 4:call(c,0x43f900u,{0x294u});m.put8(0x836ce1u,0);break;
        case 5:call(c,0x43f900u,{0x258u});break;
        default:break;
        }
    }
    const std::uint32_t kind=call(c,0x44dc50u,{call(c,0x450380u,{8u})});
    m.put32(0x8369b4u,0);m.put32(0x8369b8u,0);
    const std::uint32_t rec=0x8369c8u+kind*12u;
    m.put32(0x8369bcu,0);m.put32(0x8369b4u,rec);
    if(m.u32(rec))throw PcRaceEndUndefined{0x49cc29u};         // goal-scene init pointer (8369C8 table: never written)
}
void goal_scene_49b2e0(PcRaceContext& c){
    auto& m=c.m;
    if(m.u32(0x8369b8u)==0x24eu){
        const std::uint32_t v=m.u32(0x780258u);
        if(v==3u||v==4u||v==6u){
            for(auto r:{std::array<std::uint32_t,2>{8u,0x18u},{0x186u,1u},{0x187u,1u},{0x182u,1u},{0x20u,0xfau},{0x11au,0x1au},{0x16au,0x15u},{0x18du,1u}})
                call(c,0x440a10u,{r[0],r[1]});
            m.put32(0x8369bcu,1u);
        }
    }
    if(m.u32(m.u32(0x8369b4u)+4u))throw PcRaceEndUndefined{0x49b381u};   // goal-scene control pointer
    m.put32(0x8369b8u,m.u32(0x8369b8u)+1u);
}
void mode19_control_49cc50(PcRaceContext& c){
    auto& m=c.m;
    m.put8(0x836ce0u,1u);
    if(m.u32(0x780258u)==4u){
        call(c,0x457840u,{});
        if(!m.u8(0x836ce1u)&&call(c,0x456d70u,{})){
            if(!(call(c,0x45a0b0u,{call(c,0x455ad0u,{})&0xffu})&0xffu)&&std::int32_t(call(c,0x456e10u,{}))>1)
                m.putf(0x7c24e4u,m.f32(0x7c24e4u)+1.0f);
            m.put8(0x836ce1u,1u);
        }
    }
    if(call(c,0x427700u,{4u}))return;
    const std::uint32_t v=m.u32(0x780258u);
    if(v==5u||v==2u){if(call(c,0x427700u,{0x8405u}))return;}
    else if((v==6u||v==4u)&&call(c,0x427700u,{0x889cu}))return;
    call(c,0x43fa90u,{});
    if(!call(c,0x4872e0u,{})&&v<=10u){
        static constexpr std::uint8_t Kind[11]{0,0,0,1,1,2,0,0,0,0,0};
        if(Kind[v]==0u){
            if(std::int32_t(call(c,0x43f910u,{}))<0){
                std::uint32_t next=0x21u;
                if(call(c,0x43f860u,{})&0xffu){
                    if(call(c,0x4957f0u,{})&0xffu)call(c,0x495c10u,{});
                    const std::uint32_t w=m.u32(0x780258u);
                    if(w==6u||w==5u)next=0x22u;
                }
                call(c,0x43f8c0u,{next});
            }
        }else if(Kind[v]==1u){
            if(std::int32_t(call(c,0x456d40u,{})+0x294u)<0){
                if(!m.u8(0x836ce1u)){
                    if(!(call(c,0x45a0b0u,{call(c,0x455ad0u,{})&0xffu})&0xffu)&&std::int32_t(call(c,0x456e10u,{}))>1)
                        m.putf(0x7c24e4u,m.f32(0x7c24e4u)+1.0f);
                }
                call(c,0x428600u,{});
                call(c,0x43f8c0u,{0x22u});
            }
        }
    }
    goal_scene_49b2e0(c);
}

// 4B1670: the route number 450320 below 0x10, else 0.
std::uint32_t goal_route_4b1670(PcRaceContext& c){const std::uint32_t r=call(c,0x450320u,{});return std::int32_t(r)<0x10?r:0u;}
// 4B1680(route, level): 0 / 1..2 / 3..5 / 6..9 / 10..14 by the route bits of
// the level (tables 4B1764.. = bit counts), 15 outside.
std::uint32_t goal_index_4b1680(std::uint32_t route,std::uint32_t level){
    if(level>4u)return 0xfu;
    if(level==0u)return 0u;
    if(route>0xfu)return 0xfu;
    const std::uint32_t mask=level==1u?1u:level==2u?3u:level==3u?7u:0xfu;
    std::uint32_t bits=0;for(std::uint32_t r=route&mask;r;r&=r-1u)++bits;
    static constexpr std::uint32_t Base[5]{0u,1u,3u,6u,10u};
    return Base[level]+bits;
}
// 499960(bit) on the license (ECX = 7C23E0): +3F4 |= 2, bit of the +120 set.
void license_bit_499960(PcRaceMemory& m,std::uint32_t license,std::int32_t bit){
    m.put8(license+0x3f4u,std::uint8_t(m.u8(license+0x3f4u)|2u));
    const std::int32_t byte=bit/8,shift=bit%8;
    m.put8(std::uint32_t(std::int32_t(license+0x120u)+byte),std::uint8_t(m.u8(std::uint32_t(std::int32_t(license+0x120u)+byte))|(1u<<(shift&31))));
}
void mode19_exit_49cfd0(PcRaceContext& c){
    auto& m=c.m;
    const std::uint32_t before=m.u32(0x7c24ecu);
    const std::uint32_t t=call(c,0x4505d0u,{});
    m.put8(0x7c27d4u,std::uint8_t(m.u8(0x7c27d4u)|2u));
    m.put32(0x7c24ecu,t+before);
    if(call(c,0x43f860u,{})&0xffu){
        const std::uint32_t distance=call(c,0x4b99d0u,{m.u32(0x799d18u)});
        const std::uint32_t course=call(c,0x48b140u,{})&0xffu;
        const std::uint32_t time=call(c,0x451180u,{0u});
        const std::uint32_t a=(call(c,0x48b180u,{})&0xffu)==1u?1u:0u;   // bytes; the PC pushes their stack dwords (upper bits unset)
        const std::uint32_t b=(call(c,0x48b1a0u,{})&0xffu)==1u?1u:0u;
        const std::uint32_t goal=goal_index_4b1680(goal_route_4b1670(c),4u)-0xau;
        const std::uint32_t preset=m.u32(0x78024cu);
        const std::uint32_t arcade=(preset==0u||preset==2u)?1u:0u;
        switch(m.u32(0x780258u)){
        case 1:
            if(preset==2u||preset==3u){
                license_bit_499960(m,0x7c23e0u,arcade?0x1e:0x1f);
                call(c,0x4478f0u,{arcade,distance,time,a,b,course},0x7b17f8u);
            }else{
                license_bit_499960(m,0x7c23e0u,std::int32_t(call(c,0x450320u,{})+(arcade?0x10u:0u)));
                const std::uint32_t route=call(c,0x450320u,{});
                call(c,0x447750u,{arcade,goal,distance,time,a,b,course,route},0x7b17f8u);
            }
            break;
        case 2:{
            const std::uint32_t route=call(c,0x450320u,{});
            const std::int32_t hearts=std::int32_t(call(c,0x45b820u,{}))/10000;
            call(c,0x447a60u,{arcade,goal,std::uint32_t(hearts),time,a,b,course,route},0x7b17f8u);
            break;}
        case 6:call(c,0x495a20u,{});break;
        case 7:call(c,0x48b430u,{});break;
        case 8:call(c,0x4956c0u,{});break;
        case 9:{
            const std::uint32_t route=call(c,0x450320u,{});
            call(c,0x447c10u,{arcade,goal,distance,time,a,b,course,route},0x7b17f8u);
            break;}
        default:break;
        }
    }
    call(c,0x467880u,{});
    call(c,0x467960u,{});
    call(c,0x47ef10u,{});                                      // = jmp 416830 (GHOST save buffer close)
    if(!(call(c,0x43f860u,{})&0xffu))race_close_49cdf0(c);
    call(c,0x440a30u,{0x182u,1u});
}
// ---- mode 33 (network ranking; offline: straight to 34 / 24) ------------------
void mode33_init_49d4a0(PcRaceContext& c){
    auto& m=c.m;
    if(m.u8(0x7d68bcu)&&!m.u8(0x7d68d2u))throw PcRaceEndUndefined{0x49d4bau};   // network ranking upload (7D68BC session) not ported
    m.put32(0x836d14u,3u);
}
void mode33_control_49d850(PcRaceContext& c){
    auto& m=c.m;
    const std::uint32_t s=m.u32(0x836d14u)+1u;
    if(s>4u)return;
    if(s!=4u)throw PcRaceEndUndefined{0x49d85fu};              // states 0..2: network ranking dialogs
    call(c,0x43f8c0u,{(call(c,0x43f860u,{})&0xffu)?0x22u:0x18u});
}
// ---- GAD_PUB display 4998C0 (event 389) of the race end modes --------------
// 497960 GAME OVER (mode 27): the banner sprite and, once, its voice.
void gad_game_over_497960(PcRaceContext& c){
    auto& m=c.m;
    if(m.i32(0x836644u)>=0)return;
    std::uint32_t token;
    if(call(c,0x43f860u,{})&0xffu)token=0x2c0011u;
    else token=call(c,0x450240u,{})==7u?0x2c0011u:0x34001fu;
    m.put32(0x836644u,call(c,0x428320u,{token,0u,1u}));
    call(c,0x4b8e20u,{});
    if(m.u32(0x67ee3cu)!=1u)return;
    const std::uint32_t v=m.u32(0x780258u);
    m.put32(0x67ee3cu,0);
    call(c,0x4249f0u,{(v==6u||v==4u)?0x1e4u:0x154u});
}
// 499A30 START display (mode 13): the 490001 animation runs 836CE4 - 50
// frames, capped at 5 per player; in a network race (456D60 > 1) the capped
// frame lists each player (455AE0 >= 0) with its icons, then 4BB7C0.
void gad_start_499a30(PcRaceContext& c){
    auto& m=c.m;
    if(!(call(c,0x43f860u,{})&0xffu))return;
    const std::int32_t frames=m.i32(0x836ce4u)+1;
    m.put32(0x836ce4u,std::uint32_t(frames));
    std::int32_t shown=0;bool capped=false;
    if(frames>0x32){
        shown=frames-0x32;
        const std::int32_t cap=std::int32_t(m.u16(0x8367f0u))*5;
        if(shown>cap){shown=cap;capped=true;}
    }
    call(c,0x429530u,{0x490001u,0u,0u,4u,std::uint32_t(shown)});
    const std::uint16_t players=std::uint8_t(call(c,0x456d60u,{}));
    m.put16(0x8367f0u,players);
    if(players<=1u)return;
    if(capped){
        for(std::uint16_t i=0;i<m.u16(0x8367f0u);++i){
            const std::int8_t rank=std::int8_t(call(c,0x455ae0u,{i}));
            if(rank<0)continue;
            call(c,0x45a2b0u,{i});
            const std::int8_t icon=std::int8_t(call(c,0x4591f0u,{i}));
            const std::int8_t place=std::int8_t(std::uint8_t(call(c,0x4591c0u,{i}))-1u);
            const std::int32_t y=std::int32_t(i)*m.i32(0x67f624u)+m.i32(0x67f620u);
            call(c,0x429530u,{m.u32(0x67f648u+std::uint32_t(std::int32_t(icon))*4u),bits(float(m.i32(0x67f618u))),bits(float(y)),5u,
                             m.u32(0x67f6c0u+std::uint32_t(std::int32_t(place))*4u)});
            call(c,0x429530u,{0x490000u,bits(float(m.i32(0x67f61cu)-0x140)),bits(float(y-0xf0)),5u,std::uint32_t(std::int32_t(rank))});
        }
    }
    call(c,0x4bb7c0u,{});
}
// 497330(EAX = stage, x, y, colour): the stage's split time 4505A0.
void gad_row_time_497330(PcRaceContext& c,std::uint32_t stage,std::uint32_t x,std::uint32_t y,std::uint32_t colour){
    const auto w=time_words(call(c,0x4505a0u,{stage}));
    text_font3(c,colour);
    text_4b9200(c,format_5802dd(0x5c20e4u,{w.minutes,w.seconds,w.ms}),x,y,0x14u);
}
// 4979E0: the GOAL stage table. 8366E8 counts frames; every stage row
// (67EF68 frames each, 67EF64 rows on screen) fills its bar (42D280 /
// 42D200, percentage text; the car's +102C share for an uncleared stage),
// then shows the split time (4505A0) or, in variant 9, the 4B0100 sprite,
// the record marker (44C8D0 of 450560) and the variant 0 new-record flag
// (47EE70, handles 836658..). True once 8366E8 passed the last cleared row.
bool gad_stage_table_4979e0(PcRaceContext& c){
    auto& m=c.m;
    const float y0=m.u32(0x780258u)==1u?m.f32(0x5c20e0u):0.0f;          // [esp+4]
    const std::uint32_t car=m.u32(0x799d18u);                            // [esp+1C]
    std::int32_t rows=m.i32(0x8366e8u)/m.i32(0x67ef68u);
    std::int32_t cleared=0;
    if(std::int32_t(call(c,0x4ef710u,{}))>0)
        for(;;){if(!call(c,0x4506a0u,{std::uint32_t(cleared)}))break;++cleared;if(!(cleared<std::int32_t(call(c,0x4ef710u,{}))))break;}
    if(rows>cleared)rows=cleared;
    std::int32_t first=rows-m.i32(0x67ef64u)+1;if(first<0)first=0;
    const std::int32_t rf=m.i32(0x67ef68u),r=m.i32(0x8366e8u)%rf;
    const float share=m.f32(0x6282ccu)/float(rf);
    const std::int32_t end_frames=(cleared+1)*rf;
    const float percent=float(r)*share;
    // rows on screen: background bars and the stage names (5C2144)
    {   std::int32_t yoff=0;
        for(std::int32_t i=first;i<m.i32(0x67ef64u)+first;){
            const float y=driving::x87_float(driving::X87(yoff)+driving::X87(y0));   // fild; fadd; fstp
            call(c,0x429530u,{0x2c0013u,0u,bits(y),0u,0u});
            call(c,0x429530u,{m.u32(0x5c2144u+std::uint32_t(i)*4u),0u,bits(y),0u,0u});
            ++i;yoff+=0x26;
            if(!(i<m.i32(0x67ef64u)+first))break;
        }
    }
    auto percent_text=[&](std::int32_t p,std::uint32_t y){
        call(c,0x42ccb0u,{0u});call(c,0x42ca60u,{3u});call(c,0x42cc60u,{One,One});
        text_4b9200(c,p!=100?format_5802dd(0x5c2104u,{p}):std::string("100%"),0x1d6u,y,0x14u);
    };
    if(first<=rows){
        std::int32_t yoff=0;
        for(std::int32_t row=first;row<=rows;++row,yoff+=0x26){
            bool fresh=false;                                            // ebp
            if(m.u32(0x780258u)==0u){
                Locals l(m);call(c,0x47ee70u,{PcRaceEndLocals});
                if(m.u32(PcRaceEndLocals+std::uint32_t(row)*4u)==0u)fresh=true;
            }
            const bool clear=call(c,0x4506a0u,{std::uint32_t(row)})!=0u;
            const std::int32_t length=clear?rf:cvtt(float(rf)*m.f32(car+0x102cu));
            const std::int32_t frames=m.i32(0x8366e8u);
            if(!(rf*row>frames)&&!(frames>=rf*row+length)){               // the row filling up
                const float rowy=float(yoff)+y0;
                const std::int32_t ty=cvtt(rowy+m.f32(0x5c2140u));
                call(c,0x42d280u,{0x30002u,0x158u,std::uint32_t(ty),0u,0u,0xffffffffu});
                call(c,0x42d200u,{0x30003u,0x15au,std::uint32_t(cvtt(rowy+m.f32(0x5c213cu))),bits(percent),0u,0u,0xffffffffu});
                percent_text(cvtt(percent),std::uint32_t(ty));
                if(m.i32(0x67ef60u)==-1){m.put32(0x67ef60u,std::uint32_t(row));call(c,0x424940u,{0x8adu});}
            }
            if(!(rf*row+length>m.i32(0x8366e8u))){                       // the row complete
                if(m.i32(0x67ef60u)==row){m.put32(0x67ef60u,0xffffffffu);call(c,0x424940u,{0x80adu});}
                const std::uint32_t slot=0x836658u+std::uint32_t(row)*4u;
                if(m.i32(slot)<0&&fresh&&call(c,0x4506a0u,{std::uint32_t(row)})){
                    m.put32(slot,call(c,0x428320u,{0x2c00bdu,0u,1u}));call(c,0x424940u,{0xafu});}
                if(call(c,0x4506a0u,{std::uint32_t(row)})){
                    if(m.u32(0x780258u)==9u){
                        const std::uint32_t s=call(c,0x4b0100u,{std::uint32_t(row)});
                        call(c,0x429530u,{0x2c00fcu,bits(68.0f),bits(float(yoff)-m.f32(0x5c2138u)),9u,s});
                    }else gad_row_time_497330(c,std::uint32_t(row),0x152u,std::uint32_t(cvtt(float(yoff)+y0+m.f32(0x5c2140u))),
                                              fresh?0xff00ff00u:0xffffffffu);
                }else{
                    const float rowy=float(yoff)+y0;
                    const std::int32_t ty=cvtt(rowy+m.f32(0x5c2140u));
                    call(c,0x42d280u,{0x30002u,0x158u,std::uint32_t(ty),0u,0u,0xffffffffu});
                    call(c,0x42d200u,{0x30003u,0x15au,std::uint32_t(cvtt(rowy+m.f32(0x5c213cu))),bits(m.f32(car+0x102cu)*m.f32(0x6282ccu)),0u,0u,0xffffffffu});
                    percent_text(cvtt(m.f32(car+0x102cu)*m.f32(0x6282ccu)),std::uint32_t(ty));
                }
            }
            // record marker (497E79)
            if(const std::uint32_t rec=call(c,0x44c8d0u,{call(c,0x450560u,{std::uint32_t(row)})})){
                const std::uint32_t token=m.u32(0x67ee58u+m.u32(rec)*4u);
                const float fy=float(yoff);
                call(c,0x429530u,{0x2c004du,bits(267.0f),bits(fy+y0+m.f32(0x5c2134u)),0u,token});
                const std::uint32_t slot=0x836658u+std::uint32_t(row)*4u;
                if(m.i32(slot)>=0){
                    Locals l(m);translation(m,PcRaceEndLocals,492.0f,fy+m.f32(0x62810cu),0.0f);
                    call(c,0x4287b0u,{m.u32(slot),PcRaceEndLocals});
                }
            }
        }
    }
    for(std::uint32_t i=0;i<0xfu;++i){                                  // 497F35: handles off screen
        const std::uint32_t slot=0x836658u+i*4u;const std::int32_t h=m.i32(slot);
        if(h<0)continue;
        if(first>std::int32_t(i)||rows<std::int32_t(i)){call(c,0x4285a0u,{std::uint32_t(h)});m.put32(slot,0xffffffffu);}
    }
    const std::int32_t frames=std::int32_t(std::uint32_t(m.i32(0x8366e8u))+call(c,0x43fa00u,{}));
    m.put32(0x8366e8u,std::uint32_t(frames));
    return frames>end_frames;
}
// 497560(EAX = x, slot, name x, name y, colour; EBX = text y, the caller's
// row y - 9): a ranking row of the mission GOAL screen. The racer (476710)
// name in font 9 (42CDD0, "PLAYER" without one), then in font 3 its time
// (mission type 0: m'ss"mmm from frames / 60) or its 8-digit value (types
// 2..3). Network races (495B00) list the CommRace player of that rank instead
// (497579..: Steam build plain code; the FXT has a VM bridge at 4975BF): its name
// 457A20 (+F), and its finish time 456D90 (+40) or text 438 while it races (456DA0, +3C).
void gad_rank_row_497560(PcRaceContext& c,std::uint32_t x,std::uint32_t slot,std::uint32_t nx,std::uint32_t ny,std::uint32_t colour,std::uint32_t ty){
    auto& m=c.m;
    if(m.u32(0x780258u)==4u){
        const std::uint32_t n=call(c,0x456d60u,{})&0xffu;
        std::uint32_t i=0;
        while(i<n&&(call(c,0x45a0b0u,{i})&0xffu)!=slot)++i;
        if(i==n)return;
        const std::uint32_t e=0x7de418u+i*0x6cu;
        call(c,0x42ccb0u,{0u});call(c,0x42ca60u,{9u});call(c,0x42cc60u,{One,One});call(c,0x42cca0u,{colour});call(c,0x42cc00u,{nx,ny});
        call(c,0x42cdd0u,{e+0xfu});                                   // 457A20
        const std::uint32_t t=m.u32(e+0x40u);                          // 456D90
        call(c,0x42ca60u,{3u});
        if(!t)return;
        if(!m.u32(e+0x3cu)){                                          // 456DA0: still racing
            const std::uint32_t text=call(c,0x465eb0u,{0x438u});
            call(c,0x42cc00u,{x-call(c,0x42c370u,{text}),ty});
            call(c,0x42cdd0u,{text});
            return;
        }
        const std::uint32_t q=t/60u,rem=t%60u;                         // div 0x3C (unsigned)
        const std::int32_t minutes=std::int32_t(std::uint16_t(q))/60;
        const std::uint32_t seconds=q-std::uint32_t(minutes)*60u;
        const std::int32_t ms=std::int32_t(std::uint16_t(rem))*1000/60;
        text_4b9200(c,format_5802dd(0x5c212cu,{std::uint16_t(minutes)}),x-0x80u,ty,0x10u);
        text_4b9200(c,"'",x-0x63u,ty,6u);
        text_4b9200(c,format_5802dd(0x5c2120u,{std::uint16_t(seconds)}),x-0x5au,ty,0x10u);
        text_4b9200(c,"\"",x-0x3au,ty,0xau);
        text_4b9200(c,format_5802dd(0x5b0334u,{std::uint16_t(ms)}),x-0x30u,ty,0x10u);
        return;
    }
    const std::uint32_t r=call(c,0x476710u,{slot});
    if(!r)return;
    const std::uint32_t name=m.u32(r+0x18u),value=m.u32(r+0x14u);
    const std::int32_t t=m.i32(r+4u),q=t/60,rem=t%60;              // idiv 0x3C
    const std::int32_t ms=std::int32_t(std::uint16_t(rem))*1000/60;  // 0x88888889 magic
    const std::int32_t minutes=std::int32_t(std::uint16_t(q))/60;
    const std::uint32_t seconds=std::uint32_t(q)+std::uint32_t(minutes)*0xffc4u;
    call(c,0x42ccb0u,{0u});call(c,0x42cc60u,{One,One});call(c,0x42cca0u,{colour});call(c,0x42cc00u,{nx,ny});call(c,0x42ca60u,{9u});
    call(c,0x42cdd0u,{name?name:0x5c2114u});                        // "PLAYER"
    call(c,0x42ca60u,{3u});
    const std::int32_t kind=std::int32_t(call(c,0x495b20u,{}));
    if(kind==0){
        text_4b9200(c,format_5802dd(0x5c212cu,{std::uint16_t(minutes)}),x-0x80u,ty,0x10u);
        text_4b9200(c,"'",x-0x63u,ty,6u);
        text_4b9200(c,format_5802dd(0x5c2120u,{std::uint16_t(seconds)}),x-0x5au,ty,0x10u);
        text_4b9200(c,"\"",x-0x3au,ty,0xau);
        text_4b9200(c,format_5802dd(0x5b0334u,{std::uint16_t(ms)}),x-0x30u,ty,0x10u);
    }else if(kind>1&&kind<=3){
        const std::string s=format_5802dd(0x5c210cu,{std::int32_t(value)});
        call(c,0x42ca60u,{3u});
        text_4b9200(c,s,x-0x80u,ty,0x10u);
    }
}
// 498670 GAD_PUB GOAL display (modes 19 and 22). First call (836694 < 0,
// 67EE3A set): the GOAL banner (mission / network variants pick their own),
// the race time words 8366A8.. (449AC0 of 451180(0)), voice 0xA0. Then per
// variant: 0/1/7/8/9 the stage table 4979E0 then the time / score panels
// (497220 slides), 2 the mission result digits, 4 the network ranking
// (497560), 5 46C220, 6 the mission ranking; 836698 steps, 836630 frames.
void gad_goal_498670(PcRaceContext& c){
    auto& m=c.m;
    const std::uint16_t v=std::uint16_t(m.u32(0x780258u));   // si
    auto frame=[&]{m.put16(0x836630u,std::uint16_t(m.u16(0x836630u)+std::uint16_t(call(c,0x43fa00u,{}))));};
    auto step=[&]{m.put16(0x836698u,std::uint16_t(m.u16(0x836698u)+1u));};
    auto next=[&]{step();frame();};                            // 498F93
    auto wait=[&]{call(c,0x43f900u,{0xb4u});};                 // 49882B
    if(m.i32(0x836694u)<0&&m.u8(0x67ee3au)!=0u){
        bool banner=true;
        if(v==6u){
            const std::uint32_t r=call(c,0x495b20u,{});
            if(r==6u){m.put32(0x836694u,call(c,0x428320u,{call(c,0x495830u,{}),0u,1u}));banner=false;}
            else if(r==1u&&call(c,0x496510u,{})==0u){
                m.put32(0x836694u,call(c,0x428320u,{call(c,0x495830u,{}),0u,1u}));call(c,0x424940u,{0x1f0u});banner=false;}
        }else if(v==5u&&!(call(c,0x4957f0u,{})&0xffu))banner=false;
        if(banner){
            m.put32(0x836694u,call(c,0x428460u,{0x2c000fu,2u,1u,0u,0x5au}));
            Locals l(m);translation(m,PcRaceEndLocals,0.0f,-130.0f,0.0f);   // D3DXMatrixTranslation (4393A6)
            call(c,0x4287b0u,{m.u32(0x836694u),PcRaceEndLocals});
        }
        if(v>9u||(v!=1u&&v!=3u&&v!=4u)){if(v<=9u)call(c,0x4b8e20u,{});}   // 498FB0 / 498FB8
        time_words_449ac0(m,0x8366a8u,0x8366ecu,0x836634u,0x8366e4u,call(c,0x451180u,{0u}));
        call(c,0x451180u,{0u});
        m.put16(0x836698u,0);m.put8(0x67ef6cu,1u);
        call(c,0x424940u,{0xa0u});
        m.put8(0x67ee3au,0);
    }
    const std::uint32_t banner_handle=m.u32(0x836694u);        // EAX at the 498FC4 dispatch
    switch(v){
    case 0:case 1:case 7:case 8:case 9:{                       // 4987BA
        if(v==7u&&(call(c,0x48b350u,{})&0xffu)){
            switch(m.u16(0x836698u)){
            case 0:
                if(!(call(c,0x465f70u,{2u})&0xffu)){call(c,0x43f900u,{0u});return;}
                {const std::uint32_t h=call(c,0x428460u,{0x2b0061u,0u,1u,0u,0x46u});step();m.put16(0x836710u,std::uint16_t(h));}
                [[fallthrough]];
            case 1:
                if(call(c,0x428880u,{m.u16(0x836710u)})==1u){wait();return;}
                step();return;
            case 2:
                if(goal_voice_49a190(c)){wait();return;}
                if(m.u8(0x67ef6cu)){m.put8(0x67ef6cu,0);call(c,0x424940u,{0xb0u});}
                gad_ghost_time_497430(c,0xcdu,0xf5u,0xffffffffu);
                if(std::int32_t(call(c,0x43f910u,{}))>=0xa)return;
                step();return;
            case 3:
                call(c,0x4285a0u,{m.u16(0x836710u)});step();return;
            default:return;
            }
        }
        if(m.u16(0x836698u)!=0u){frame();return;}
        if(v==1u||v==9u){
            if(std::int32_t(call(c,0x43f910u,{}))>0x3fc)return;
            call(c,0x4b8e30u,{0u});
        }
        if((v==0u||v==7u||v==8u)&&call(c,0x4bea50u,{}))return;
        if(goal_voice_49a190(c)){wait();return;}
        if(!gad_stage_table_4979e0(c)){wait();return;}
        if(m.u8(0x67ef6cu)){m.put8(0x67ef6cu,0);call(c,0x424940u,{0xb0u});}
        if(v==1u||v==9u){
            const std::uint32_t car=m.u32(0x799d18u);
            const float y=v==1u?m.f32(0x5c20e0u):0.0f;
            call(c,0x429530u,{0x2c00b4u,0u,bits(y),0u,0u});
            gad_time_4973c0(c,0x152u,std::uint32_t(cvtt(y+m.f32(0x5c2230u))),0xffffffffu);
            anim_497220(m,0x8366c4u,m.f32(0x5c222cu),m.f32(0x5c403cu),m.f32(0x5c2228u),358.0f,1.02f,0.7f,1.0f,0.7f);
            gad_score_4974e0(c,car,std::uint32_t(cvtt(m.f32(0x8366c4u))),std::uint32_t(cvtt(y+m.f32(0x5c2224u))),0xffffff00u);
        }
        if(v==0u||v==7u||v==8u){
            call(c,0x429530u,{0x2c00b5u,0u,0u,0u,0u});
            anim_497220(m,0x8366d4u,m.f32(0x5c2220u),m.f32(0x5c403cu),m.f32(0x5c221cu),338.0f,1.02f,0.7f,1.0f,0.7f);
            gad_time_4973c0(c,std::uint32_t(cvtt(m.f32(0x8366d4u))),0x181u,0xffffff00u);
        }
        frame();return;}
    case 2:                                                    // 498AA5
        switch(m.u16(0x836698u)){
        case 0:{
            call(c,0x429530u,{0x2c006cu,0u,0u,9u,0u});
            std::int32_t limit=0x1e;std::uint32_t row=0x5c2208u;
            Locals l(m);
            for(std::uint32_t e=0x7f2588u;e<0x7f26dcu;e+=0x44u,row+=4u,limit+=0x1e){
                const std::int32_t f=m.u16(0x836630u);const std::int32_t y=m.i32(row);
                std::uint32_t token;
                if(f<limit-0x1e){if(f<limit)continue;}
                if(f>=limit-0x1e&&f<=limit)token=m.u32(0x5c21e8u+std::uint32_t((f>>1)&7)*4u);   // rolling digit
                else{
                    std::int32_t digit=0;const std::int8_t d=std::int8_t(m.u8(e));
                    if(d)digit=m.i32(e-0x24u)/d;
                    token=m.u32(0x5c21e8u+std::uint32_t(digit)*4u);
                }
                m.putf(PcRaceEndLocals,0.0f);m.putf(PcRaceEndLocals+4,0.0f);
                call(c,0x4294c0u,{token,PcRaceEndLocals+4,PcRaceEndLocals});
                call(c,0x429530u,{token,bits(m.f32(PcRaceEndLocals+4)+80.0f),bits((float(y)-240.0f)+m.f32(PcRaceEndLocals)),9u,0u});
            }
            if(m.u16(0x836630u)<0x12cu){frame();return;}
            next();return;}
        case 1:{
            call(c,0x4285a0u,{banner_handle});
            for(std::uint32_t a=0x836658u;a<0x83666cu;a+=4)call(c,0x4285a0u,{m.u32(a)});
            const std::uint32_t i=call(c,0x45bf30u,{4u});
            m.put32(0x83664cu,call(c,0x428320u,{m.u32(m.u32(0x83669cu)+i*4u),8u,5u}));
            call(c,0x424940u,{0xa8u});next();return;}
        case 2:{
            if(m.u16(0x836630u)<0x158u){frame();return;}
            step();
            const std::uint32_t i=call(c,0x45bf30u,{4u});
            call(c,0x424940u,{m.u16(0x5c1e30u+i*2u)});frame();return;}
        case 3:{
            const std::int32_t s=std::int32_t(call(c,0x45b830u,{})),d=std::int32_t(call(c,0x45b840u,{}));
            const std::string text=format_5802dd(0x5c21e0u,{s/d});
            call(c,0x42ca60u,{3u});call(c,0x42cc60u,{One,One});call(c,0x42ccb0u,{9u});
            const std::uint32_t i=call(c,0x45bf30u,{4u});
            text_4b9200(c,text,m.u32(0x5c21a0u+i*8u),m.u32(0x5c21a4u+i*8u),0x14u);
            frame();return;}
        default:frame();return;
        }
    case 4:{                                                   // 498D6C
        call(c,0x4b9c80u,{0u});
        const std::uint16_t s=m.u16(0x836698u);
        if(s==0u){if(call(c,0x456d70u,{})==1u)step();m.put16(0x836630u,0);}
        else if(s!=1u){frame();return;}
        std::int32_t n=std::int32_t(call(c,0x496440u,{}));if(n>6)n=6;
        call(c,0x429530u,{0x2c00fbu,0u,0u,0u,std::uint32_t(n-1)});
        for(std::int32_t i=0;i<n;++i){const std::uint32_t y=m.u32(0x5c1e40u+std::uint32_t(i)*4u);
            gad_rank_row_497560(c,0x22bu,std::uint32_t(i),0x80u,y,0xffffffffu,y-9u);}
        const std::uint32_t r=call(c,0x496510u,{});
        if(r&&m.u8(0x67ef6cu)==1u){m.put8(0x67ef6cu,0);call(c,0x424940u,{m.u16(0x67ee2cu+r*2u)});}
        frame();return;}
    case 5:call(c,0x46c220u,{});frame();return;                 // 498D53
    case 6:                                                    // 498E4B
        switch(m.u16(0x836698u)){
        case 0:{
            const std::uint32_t r=call(c,0x495b20u,{});
            std::int32_t n=std::int32_t(call(c,0x496440u,{}));if(n>6)n=6;
            if(r!=6u&&r!=1u){
                call(c,0x429530u,{0x2c00fbu,0u,0u,0u,std::uint32_t(n-1)});
                for(std::int32_t i=0;i<n;++i){const std::uint32_t y=m.u32(0x5c1e40u+std::uint32_t(i)*4u);
                    gad_rank_row_497560(c,0x22bu,std::uint32_t(i),0x80u,y,0xffffffffu,y-9u);}
            }
            if(m.u8(0x67ef6cu)==1u&&r!=3u&&r!=2u&&r!=6u){
                m.put8(0x67ef6cu,0);
                std::uint32_t i=call(c,0x496510u,{});
                const std::int32_t k=std::int32_t(call(c,0x496440u,{}));
                if(k<=6&&i==0u)i=std::uint32_t(7-k);
                call(c,0x424940u,{m.u16(0x67ee2cu+i*2u)});
            }
            if(m.u16(0x836630u)<0xb4u){frame();return;}
            next();return;}
        case 1:{
            call(c,0x4285a0u,{banner_handle});
            const std::uint32_t i=call(c,0x496510u,{});
            m.put32(0x836694u,call(c,0x428320u,{m.u32(0x5c2184u+i*4u),8u,5u}));
            call(c,0x424940u,{0xa8u});next();return;}
        case 2:{
            if(m.u16(0x836630u)<0xf0u){frame();return;}
            const std::uint32_t i=call(c,0x496510u,{})+1u;
            call(c,0x424940u,{m.u16(0x5c1e20u+i*2u)});next();return;}
        case 3:
            if(m.u16(0x836630u)<0x168u){frame();return;}
            call(c,0x4285a0u,{banner_handle});call(c,0x43f900u,{0x3cu});next();return;
        default:frame();return;
        }
    default:frame();return;                                    // 3, > 9
    }
}
// 499020 GAD_PUB TIME OVER display (modes 20 and 21): first call (836650 <
// 0) the TIME OVER banner (mission variant: its own banner and voice), the
// race time words; then per variant the stage table (0/1/7/8/9; the ghost
// time for TA), the mission results (2), the network or mission ranking
// rows (4, 6) and the Heart Attack voice (5); 836698 steps, 836630 frames.
void gad_time_over_499020(PcRaceContext& c){
    auto& m=c.m;
    const std::uint16_t v=std::uint16_t(m.u32(0x780258u));   // si
    auto frame=[&]{m.put16(0x836630u,std::uint16_t(m.u16(0x836630u)+std::uint16_t(call(c,0x43fa00u,{}))));};
    auto step=[&]{m.put16(0x836698u,std::uint16_t(m.u16(0x836698u)+1u));};
    auto next=[&]{step();frame();};
    if(m.i32(0x836650u)<0){
        if(v==6u){
            const std::uint32_t r=call(c,0x495b20u,{});
            const std::uint32_t token=r==1u?0x2b0054u:r==6u?0x2b004eu:0x2c0012u,voice=r==1u?0x1f0u:r==6u?0x216u:0x1e3u;
            m.put32(0x836650u,call(c,0x428320u,{token,0u,1u}));call(c,0x424940u,{voice});
        }else{
            m.put32(0x836650u,call(c,0x428320u,{0x2c0012u,0u,1u}));
            call(c,0x424940u,{m.u32(0x780258u)==4u?0x1e3u:0x155u});
        }
        {Locals l(m);translation(m,PcRaceEndLocals,0.0f,0.0f,0.0f);   // D3DXMatrixTranslation (4393A6)
         call(c,0x4287b0u,{m.u32(0x836650u),PcRaceEndLocals});}
        if((call(c,0x4957f0u,{})&0xffu)||(v<=9u&&v!=3u&&v!=4u&&v!=5u&&v!=6u))call(c,0x4b8e20u,{});   // 4996D0 / 4996D8
        time_words_449ac0(m,0x8366a8u,0x8366ecu,0x836634u,0x8366e4u,call(c,0x451180u,{0u}));
        m.put16(0x836698u,0);m.put8(0x67ef6du,1u);
    }
    if(goal_voice_49a190(c)){call(c,0x43f900u,{(v==2u||v==5u)?0x258u:0xf0u});return;}
    if(v==0xffffu||v>9u){frame();return;}
    switch(v){
    case 0:case 1:case 7:case 8:case 9:                        // 4991AD
        if(v==7u&&(call(c,0x48b350u,{})&0xffu)){
            switch(m.u16(0x836698u)){
            case 0:
                if(!(call(c,0x465f70u,{2u})&0xffu)){call(c,0x43f900u,{0u});return;}
                {const std::uint32_t h=call(c,0x428460u,{0x2b0061u,0u,1u,0u,0x46u});step();m.put16(0x836714u,std::uint16_t(h));}
                [[fallthrough]];
            case 1:
                if(call(c,0x428880u,{m.u16(0x836714u)})==1u){call(c,0x43f900u,{0xb4u});return;}
                step();return;
            case 2:
                if(goal_voice_49a190(c)){call(c,0x43f900u,{0xb4u});return;}
                if(m.u8(0x67ef6du)){m.put8(0x67ef6du,0);call(c,0x424940u,{0xb0u});}
                gad_ghost_time_497430(c,0xcdu,0xf5u,0xffffffffu);
                return;
            case 3:
                call(c,0x4285a0u,{m.u16(0x836714u)});step();return;
            default:return;
            }
        }
        if(m.u16(0x836698u)!=0u){frame();return;}
        if(gad_stage_table_4979e0(c)&&m.u16(0x836630u)>0x12bu){call(c,0x4285a0u,{m.u32(0x836650u)});next();return;}
        call(c,0x43f900u,{0xb4u});frame();return;
    case 2:{                                                   // 4992F7
        const std::uint32_t car=m.u32(0x799d18u);
        switch(m.u16(0x836698u)){
        case 0:if(m.u16(0x836630u)<0x12bu){frame();return;}next();return;
        case 1:{
            const std::uint32_t k=call(c,0x44c940u,{m.u32(car+0x68u)});
            call(c,0x4285a0u,{m.u32(0x836650u)});
            const std::uint32_t i=std::uint16_t(call(c,0x45bf30u,{k-1u}));
            m.put32(0x836650u,call(c,0x428320u,{m.u32(m.u32(0x83669cu)+i*4u),8u,1u}));
            call(c,0x424940u,{0xa8u});next();return;}
        case 2:{
            if(m.u16(0x836630u)<0x158u){frame();return;}
            const std::uint32_t i=std::uint16_t(call(c,0x45bf30u,{call(c,0x44c940u,{m.u32(car+0x68u)})-1u}));
            call(c,0x424940u,{m.u16(0x5c1e30u+i*2u)});next();return;}
        case 3:{
            const std::uint32_t k=call(c,0x44c940u,{m.u32(car+0x68u)});
            const std::int32_t s=std::int32_t(call(c,0x45b830u,{})),d=std::int32_t(call(c,0x45b840u,{}));
            const std::string text=format_5802dd(0x5c21e0u,{s/d});
            call(c,0x42ca60u,{3u});call(c,0x42cc60u,{One,One});call(c,0x42ccb0u,{9u});
            const std::uint32_t i=std::uint16_t(call(c,0x45bf30u,{k-1u}));
            text_4b9200(c,text,m.u32(0x5c21a0u+i*8u),m.u32(0x5c21a4u+i*8u),0x14u);
            frame();return;}
        default:frame();return;
        }}
    case 4:{                                                   // 4995E3
        const std::uint16_t s=m.u16(0x836698u);
        if(s==0u){step();m.put8(0x836712u,1u);}
        else if(s!=1u){frame();return;}
        std::int32_t n=std::int32_t(call(c,0x496440u,{}));if(n>6)n=6;
        call(c,0x429530u,{0x2c00fbu,0u,0u,0u,std::uint32_t(n-1)});
        for(std::int32_t i=0;i<n;++i){const std::uint32_t y=m.u32(0x5c1e40u+std::uint32_t(i)*4u);
            gad_rank_row_497560(c,0x22bu,std::uint32_t(i),0x80u,y,0xffffffffu,y-9u);}
        const std::uint32_t r=call(c,0x496510u,{});
        if(m.u16(0x836630u)>0x3cu&&r!=0u&&m.u8(0x836712u)==1u){m.put8(0x836712u,0);call(c,0x424940u,{m.u16(0x67ee2cu+r*2u)});}
        if(m.u16(0x836630u)>=0x294u){step();call(c,0x4285a0u,{m.u32(0x836650u)});}
        frame();return;}
    case 5:                                                    // 49947B
        switch(m.u16(0x836698u)){
        case 0:
            if(m.u16(0x836630u)<0x77u){frame();return;}
            call(c,0x4285a0u,{m.u32(0x836650u)});call(c,0x46c420u,{});call(c,0x424940u,{0xa8u});next();return;
        case 1:
            if(m.u16(0x836630u)<0xb4u){frame();return;}
            call(c,0x45bd30u,{m.u32(0x5e0d94u+call(c,0x46c440u,{})*4u),0x3cu,0xfu});next();return;   // 45BD30(id, 3C, F)
        default:frame();return;
        }
    case 6:{                                                   // 499515
        auto pick=[&]{std::uint32_t i=0;if(call(c,0x495b20u,{})==1u)i=call(c,0x496510u,{})+1u;return i;};
        switch(m.u16(0x836698u)){
        case 0:{
            if(m.u16(0x836630u)<0x77u){frame();return;}
            call(c,0x4285a0u,{m.u32(0x836650u)});
            const std::uint32_t i=pick();
            m.put32(0x836650u,call(c,0x428320u,{m.u32(0x5c2180u+i*4u),8u,5u}));
            call(c,0x424940u,{0xa8u});next();return;}
        case 1:{
            if(m.u16(0x836630u)<0xb4u){frame();return;}
            const std::uint32_t i=pick();
            call(c,0x424940u,{m.u16(0x5c1e20u+i*2u)});next();return;}
        default:frame();return;
        }}
    default:frame();return;                                    // 3
    }
}
// 497FA0 OUTRUN MILES digits (mode 34): the eight digits 836D1C at
// 67EE40/67EE44 (SSE: (x - 320) - i * 67EE48, y - 240).
void miles_digits_497fa0(PcRaceContext& c){
    auto& m=c.m;
    if(m.u32(0x67f638u)==0xffffffffu)return;
    for(std::uint32_t i=0;i<8u;++i){
        const std::int32_t digit=std::int8_t(m.u8(0x836d1cu+i));
        const float y=m.f32(0x67ee44u)-240.0f;
        const float x=(m.f32(0x67ee40u)-320.0f)-float(std::int32_t(i))*m.f32(0x67ee48u);
        call(c,0x429530u,{0x2b004cu,bits(x),bits(y),5u,std::uint32_t(digit)});
    }
}
// 49A060 (mode 18, the pause): [67F628] (.data 1, no writer) -> the sprite 2C00E3 on layer
// 0xC with no matrix at frame [836CF4]; the 837FE0 text rows follow only when [836D00] == 1.
// 836CF4 / 836D00 are .bss words nothing writes (0).
void gad_pause_49a060(PcRaceContext& c){
    call(c,0x4289b0u,{0x2c00e3u,0xcu,0u,0u});
}
// 497900 (mode 23 display): the full-screen image 30000 (42D5C0, layer 5.0) and the
// countdown [780250] / 60 in seconds ("%d") as 4BC990 digits (style 3, 0x11C, 0x10E).
void gad_countdown_497900(PcRaceContext& c){
    call(c,0x42d5c0u,{0x30000u,0u,0u,bits(5.0f),0xffffffffu});
    const std::int32_t t=std::int32_t(call(c,0x43f910u,{}));
    const std::string text=format_5802dd(0x5c2130u,{t/60});         // imul 0x88888889: truncation toward 0
    Locals l(c.m);
    for(std::size_t k=0;k<text.size();++k)c.m.put8(PcRaceEndLocals+std::uint32_t(k),std::uint8_t(text[k]));
    c.m.put8(PcRaceEndLocals+std::uint32_t(text.size()),0);
    call(c,0x4bc990u,{3u,0x11cu,0x10eu,PcRaceEndLocals});
}
void gad_display_4998c0(PcRaceContext& c){
    auto& m=c.m;
    const std::uint32_t mode=m.u32(0x78026cu),i=mode-13u;
    if(i>0x16u)return;
    static constexpr std::uint8_t Index[23]{0,9,9,9,1,2,3,4,4,3,5,1,1,1,6,9,9,9,9,9,7,8,1};   // 499940
    switch(Index[i]){
    case 0:gad_start_499a30(c);return;
    case 2:if(mode==0x12u)gad_pause_49a060(c);return;
    case 3:gad_goal_498670(c);return;
    case 4:gad_time_over_499020(c);return;
    case 5:gad_countdown_497900(c);return;
    case 6:gad_game_over_497960(c);return;
    case 7:if(m.u32(0x836d14u)==0u)call(c,0x48c5f0u,{},0x836d30u);return;
    case 8:miles_digits_497fa0(c);return;
    default:return;
    }
}
// ---- helpers ---------------------------------------------------------------
void common_anim_4b72f0(PcRaceContext& c,std::uint32_t kind){
    auto& m=c.m;
    if(m.i32(0x687774u)<0)m.put32(0x687774u,call(c,0x428320u,{0x330001u,1u,0u}));
    const std::int32_t h=m.i32(0x687778u);
    if(h>=0){
        if(kind==m.u32(0x842888u))return;
        call(c,0x4285a0u,{std::uint32_t(h)});
        m.put32(0x687778u,0xffffffffu);
    }
    if(std::int32_t(kind)>0&&std::int32_t(kind)<=2)m.put32(0x687778u,call(c,0x428320u,{0x330000u,1u,0u}));
    m.put32(0x842888u,kind);
}
void common_anim_release_4b7630(PcRaceContext& c){
    auto& m=c.m;
    call(c,0x4285a0u,{m.u32(0x687774u)});                  // VM at 4B7630 = mov eax,[687774]
    const std::uint32_t second=m.u32(0x687778u);
    m.put32(0x687774u,0xffffffffu);
    call(c,0x4285a0u,{second});
    m.put32(0x687778u,0xffffffffu);
}
std::uint32_t result_input_4bfb20(PcRaceContext& c){
    auto& m=c.m;
    const std::uint32_t object=call(c,0x4035f0u,{});
    if(!(m.i32(object+0x518u)>0)&&call(c,0x4536f0u,{1u}))return 1u;   // VM at 4BFB25 = mov ecx,[eax+518]
    const std::uint32_t again=call(c,0x4035f0u,{});
    if(m.i32(again+0x518u)>0)return 0u;
    return call(c,0x4536f0u,{4u})?1u:0u;
}
void adv_release_45af40(PcRaceContext& c){
    auto& m=c.m;
    if(m.u32(0x7f1964u)){call(c,0x440cd0u,{0x7f1964u});m.put32(0x7f1958u,0);}
    m.put32(0x7f195cu,0);m.put32(0x7f1960u,2u);m.put32(0x7f196cu,0xfu);
}
void bookkeeping_453370(PcRaceContext& c){
    auto& m=c.m;
    std::array<std::uint16_t,6> keys{0xffffu,0xffffu,0xffffu,0xffffu,0xffffu,0xffffu};   // [esp+0x10..]: word 0 = 0, words 1..5 = -1
    keys[0]=0;
    std::uint16_t si=1,di=0;
    for(;;){
        const std::uint32_t bx=si;
        const std::uint16_t ax=std::uint16_t(call(c,0x451350u,{bx-1u}));
        if(ax==2u)break;
        di=m.u16(0x5a7ab4u+(std::uint32_t(ax)+std::uint32_t(di)*2u)*2u);
        ++si;
        keys[bx]=di;
        if(!(si<5u))break;
    }
    if(m.u32(0x7d6764u)==0u)si=std::uint16_t(si+0xffffu);
    if(si==0u){m.put32(0x7d6764u,0);return;}
    for(std::uint32_t i=0;i<si;++i){
        const std::int32_t level=std::int32_t(call(c,0x44c940u,{std::uint32_t(std::int32_t(std::int16_t(keys[i])))}));
        call(c,0x4505a0u,{i});
        if(level>=4)call(c,0x4505e0u,{});
        else call(c,0x4505f0u,{i});
    }
    m.put32(0x7d6764u,0);
}
// ---- OUTRUN2SP route (menu mode 2 commit: [780260] = 0, +21C = 7) ----------------------
// 487240: the camera override reset (protected head: al = [82E7D4]): the script 653788[scene]
// entries lose their +28 flag up to the 0x1F terminator, the player car +2DC = 0, then the
// 4866D0 words.
void camera_reset_487240(PcRaceContext& c){
    auto& m=c.m;
    const std::uint8_t scene=m.u8(0x82e7d4u);
    if(scene!=0xffu){
        std::uint32_t p=m.u32(0x653788u+std::uint32_t(scene)*4u);
        while(m.u32(p)!=0x1fu){m.put32(p+0x28u,0);p+=0x2cu;}
    }
    m.putf(m.u32(0x799d18u)+0x2dcu,0.0f);
    m.put32(0x82e7d8u,0);m.put32(0x82e7e0u,0x7f7fffffu);                        // 59943C (FLT_MAX)
    m.put16(0x82e7c4u,0x7fffu);m.put8(0x82e7d4u,0xffu);m.put32(0x82e7c8u,0);m.put32(0x82e7ecu,0);
    m.put32(0x82e7e4u,2u);m.put16(0x82e7dcu,0);m.put32(0x82e7d0u,1u);m.put32(0x82e7c0u,1u);m.put16(0x82e7ccu,0);
}
// 4B6F80 (protected head: [842850] = 0): the arcade entry bytes.
void arcade_reset_4b6f80(PcRaceContext& c){
    auto& m=c.m;
    m.put8(0x842850u,0);m.put8(0x842874u,0);m.put8(0x84286cu,0);m.put8(0x842851u,3u);m.put8(0x842828u,0);
}
// 7 / 9 init 48AE40 = 4B72F0(1).
void mode7_init_48ae40(PcRaceContext& c){common_anim_4b72f0(c,1u);}
// 7 control 49F360 = 43F8C0(9).
void mode7_control_49f360(PcRaceContext& c){call(c,0x43f8c0u,{9u});}
// 7 exit 49F370: the SUMO_FE world and packs go.
void mode7_exit_49f370(PcRaceContext& c){
    call(c,0x44fd00u,{});call(c,0x42ebf0u,{});call(c,0x4489f0u,{});call(c,0x4489c0u,{});
    call(c,0x42dfd0u,{});call(c,0x429a10u,{});
    adv_release_45af40(c);                                   // 45AEF0
    camera_reset_487240(c);
    arcade_reset_4b6f80(c);
}
// 9 control 48AE50: from mode 7 the variant becomes -1, the goal words 4AEEF0(-1, -1, -1.0)
// and mode 10 (attract) follows; otherwise mode 3.
void mode9_control_48ae50(PcRaceContext& c){
    auto& m=c.m;
    if(call(c,0x43f8f0u,{})==7u){
        call(c,0x43f940u,{0xffffffffu});
        m.put32(0x6840ecu,0xffffffffu);m.put32(0x6840f0u,0xffffffffu);m.putf(0x6840f4u,-1.0f);
        call(c,0x43f8c0u,{0xau});
    }else call(c,0x43f8c0u,{3u});
}
// ---- 10 ATTRACT (48B210 / 48AE90 / 48B070), 11 (48B0B0), 12 (48B0F0 / 48B120 / 48B2A0) -----------
namespace {
void attract_bytes_ff(PcRaceMemory& m){
    m.put8(0x655b59u,0xffu);m.put8(0x830364u,0xffu);m.put8(0x830374u,0xffu);m.put8(0x83036du,0xffu);m.put8(0x83036cu,0xffu);
}
}
void mode10_init_48b210(PcRaceContext& c){
    auto& m=c.m;
    attract_bytes_ff(m);
    m.put32(0x830368u,0);
    call(c,0x401050u,{});call(c,0x49fa60u,{});
    m.put32(0x83037cu,0);m.put32(0x830380u,0);
    attract_bytes_ff(m);
    call(c,0x45a840u,{});call(c,0x4276b0u,{});
    call(c,0x44da00u,{0u,0u});
    call(c,0x4518c0u,{});
}
// 48AE90: packs 2E / 12 / 15 / 2F (2E and 2F also through 429920), the attract car model
// (448AD0(46BBE0(0), 8)), events 4 (function [655B5C]) and 0x17F (0x17), the scene owner reset
// 49BA50, then each frame 49BA80 while [830380], event 4's next function when its flags say
// so, and mode 11 once event 4 lets go ([79FB4C] & 3 == 0, or its work +A bit 1) and 45A920.
void mode10_control_48ae90(PcRaceContext& c){
    auto& m=c.m;
    m.put32(0x83037cu,m.u32(0x83037cu)+1u);
    const std::uint32_t state=m.u32(0x830368u);
    switch(state){
    case 0:
        call(c,0x42deb0u,{0x2eu,8u});call(c,0x42deb0u,{0x12u,8u});call(c,0x42deb0u,{0x15u,8u});call(c,0x42deb0u,{0x2fu,2u});
        m.put32(0x830368u,1u);
        [[fallthrough]];
    case 1:
        if(!call(c,0x42df90u,{}))break;
        if(call(c,0x427700u,{0u}))break;
        call(c,0x429920u,{0x2fu,2u});call(c,0x429920u,{0x2eu,9u});
        if(!call(c,0x4299a0u,{}))break;
        m.put32(0x830368u,2u);
        [[fallthrough]];
    case 2:
        call(c,0x448ad0u,{call(c,0x46bbe0u,{0u}),8u});
        m.put32(0x830368u,3u);
        [[fallthrough]];
    case 3:
        if(!call(c,0x448980u,{}))break;
        [[fallthrough]];
    case 4:case 5:
        m.put32(0x830368u,6u);
        [[fallthrough]];
    case 6:
        call(c,0x440110u,{4u,m.u32(0x655b5cu)});call(c,0x440110u,{0x17fu,0x17u});
        call(c,0x49ba50u,{});call(c,0x44c0b0u,{});
        common_anim_release_4b7630(c);
        m.put32(0x830368u,7u);
        [[fallthrough]];
    case 7:{
        if(m.u32(0x830380u))call(c,0x49ba80u,{});
        if((m.u8(0x79fb4cu)&3u)==2u){
            const std::uint32_t w=m.u32(0x799c28u);
            if(m.u8(w+0xau)&1u){
                call(c,0x440200u,{4u});
                const std::uint32_t f=m.u32(m.u32(w)+std::uint32_t(std::int32_t(m.i8(w+0x15u)))*4u);
                m.put32(w+4u,f);
                call(c,0x440110u,{4u,m.u32(0x655b5cu+f*4u)});
            }
        }
        const std::uint32_t start=call(c,0x45a920u,{});
        const std::uint8_t fl=m.u8(0x79fb4cu)&3u;
        bool leave=false;
        if(fl==0u)leave=true;
        else if(fl!=1u&&(m.u8(m.u32(0x799c28u)+0xau)&2u))leave=true;
        if(!leave||!start)break;
        m.put32(0x830368u,9u);
        call(c,0x43f8c0u,{0xbu});
        break;}
    case 8:
        m.put32(0x830368u,9u);
        [[fallthrough]];
    case 9:
        call(c,0x43f8c0u,{0xbu});
        break;
    default:break;
    }
    if(call(c,0x43f980u,{}))call(c,0x43f990u,{0xcu});
}
void mode10_exit_48b070(PcRaceContext& c){
    call(c,0x428600u,{});
    if(call(c,0x43f8e0u,{})==0x20u)call(c,0x4999d0u,{});
    call(c,0x440330u,{4u,1u});call(c,0x4401d0u,{2u});
    call(c,0x4299c0u,{0x2eu});call(c,0x42dfb0u,{0x2eu});call(c,0x42dfb0u,{0x12u});
}
// 48B0B0: after mode 12, 4532E0 == 2 -> 5, else 10; otherwise START (13).
void mode11_control_48b0b0(PcRaceContext& c){
    if(call(c,0x43f8f0u,{})==0xcu){call(c,0x43f8c0u,{call(c,0x4532e0u,{})==2u?5u:0xau});return;}
    call(c,0x43f8c0u,{0xdu});
}
void mode12_init_48b0f0(PcRaceContext& c){
    call(c,0x401030u,{0u});call(c,0x427630u,{});
    call(c,0x4401d0u,{0x17fu});call(c,0x4401d0u,{0x181u});
    call(c,0x440330u,{8u,0x18u});
}
void mode12_control_48b120(PcRaceContext& c){call(c,0x43f8c0u,{0xbu});}
void mode12_exit_48b2a0(PcRaceContext& c){
    call(c,0x428600u,{});call(c,0x44fd00u,{});call(c,0x42ebf0u,{});call(c,0x44fcc0u,{0u});
    call(c,0x4489c0u,{});call(c,0x42dfd0u,{});call(c,0x429a10u,{});
    call(c,0x43f940u,{1u});
    attract_bytes_ff(c.m);
    call(c,0x455730u,{});
}
}
