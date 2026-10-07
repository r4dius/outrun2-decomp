#include "enhancements/frame_rate.hpp"
#include "system/exe_image.hpp"
#include "platform/frontend_text.hpp"
#include "platform/race_hud_navi.hpp"
#include "platform/race_hud.hpp"
#include "driving/pc_x87.hpp"
#include "driving/pc_d3dx.hpp"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <cstdlib>
namespace outrun::platform {
namespace {
#include "platform/race_hud_navi_tables.inc"
std::uint32_t fb(float f){std::uint32_t u;std::memcpy(&u,&f,4);return u;}
float bf(std::uint32_t u){float f;std::memcpy(&f,&u,4);return f;}
using driving::X87;
// A dword read as unsigned: FILD m32 (exact load) and, when negative,
// FADD [628070] (2^32), rounded under the x87 precision control.
X87 fild_u32(std::uint32_t v){
    const X87 r=X87(int(std::int32_t(v)));
    return std::int32_t(v)<0?r+X87(4294967296.0f):r;
}
// _ftol2 582194: 64-bit truncation of ST0; EAX is the low dword
// (indefinite -> 0).
std::uint32_t ftol(X87 v){return std::uint32_t(std::uint64_t(driving::x87_ftol64(v)));}
// cvttss2si: out of range and NaN give 0x80000000.
std::int32_t cvtt(float v){
    if(!std::isfinite(v)||v>=2147483648.0f||v<-2147483648.0f)return std::int32_t(0x80000000u);
    return std::int32_t(v);
}
std::string format_int(const char* f,std::int32_t v){char b[64];std::snprintf(b,sizeof b,f,v);return b;}
// PcRaceMemory with the accessor set used below (i8/puti) and no copies.
struct Mem {
    PcRaceMemory& r;
    std::uint8_t u8(std::uint32_t a){return r.u8(a);}
    std::int8_t i8(std::uint32_t a){return std::int8_t(r.u8(a));}
    std::uint16_t u16(std::uint32_t a){return r.u16(a);}
    std::int16_t i16(std::uint32_t a){return r.i16(a);}
    std::uint32_t u32(std::uint32_t a){return r.u32(a);}
    std::int32_t i32(std::uint32_t a){return r.i32(a);}
    float f32(std::uint32_t a){return r.f32(a);}
    void put8(std::uint32_t a,std::uint8_t v){r.put8(a,v);}
    void put16(std::uint32_t a,std::uint16_t v){r.put16(a,v);}
    void put32(std::uint32_t a,std::uint32_t v){r.put32(a,v);}
    void puti(std::uint32_t a,std::int32_t v){r.put32(a,std::uint32_t(v));}
    void putf(std::uint32_t a,float v){r.putf(a,v);}
    driving::Bytes bytes(std::uint32_t a,std::size_t n){return r.bytes(a,n);}
};
struct N {
    NaviPubServices& s;Mem m;
    N(NaviPubServices& services,PcRaceMemory& memory):s(services),m{memory}{}
    bool fail(std::uint32_t pc){if(!s.missing)s.missing=pc;return false;}
    bool faulted(){return false;}             // unmapped accesses throw PcRaceUnmapped (see guard)
    std::uint32_t u32(std::uint32_t a){return m.u32(a);}
    std::int32_t i32(std::uint32_t a){return m.i32(a);}
    void emit(const RaceHudDraw& d){
        if(s.draws)s.draws->push_back(d);
        if(!s.scene_draws||!s.sprites||!s.matrices||!s.sprani||!s.blend_9564e0)return;
        const auto* a=d.args.data();
        if(d.pc==0x429530u||d.pc==0x429580u){
            const auto t=sprani_translation(bf(a[1]),bf(a[2]));
            sprani_draw_428a10(*s.sprites,*s.sprani,*s.matrices,a[0],&t,a[3],std::int32_t(a[4]),d.pc==0x429580u?bf(a[5]):1.0f,
                               1.0f,0.0f,*s.scene_draws,*s.blend_9564e0);
        }else if(d.pc==0x4289b0u)
            sprani_draw_428a10(*s.sprites,*s.sprani,*s.matrices,a[0],a[4]?nullptr:&d.matrix,a[1],std::int32_t(a[3]),1.0f,1.0f,0.0f,
                               *s.scene_draws,*s.blend_9564e0);   // args[4]: the 4289B0 matrix pointer was NULL
        else if(d.pc==0x428980u)                                     // 428980(token, layer, NULL): 428A10 frame 1
            sprani_draw_428a10(*s.sprites,*s.sprani,*s.matrices,a[0],nullptr,a[1],1,1.0f,1.0f,0.0f,*s.scene_draws,*s.blend_9564e0);
    }
    void draw(std::uint32_t pc,std::initializer_list<std::uint32_t> a){
        RaceHudDraw d;d.pc=pc;std::size_t k=0;for(auto v:a)d.args[k++]=v;emit(d);}
    void sound(std::uint32_t id){if(s.sounds)s.sounds->push_back(id);}
    // Sprite pool (PC 428320/428460/4285A0/428800/428880 semantics).
    std::int32_t sprite_create(std::uint32_t token,std::uint32_t layer,std::uint32_t mode){
        if(!s.sprites){fail(0x428320);return -1;}
        const auto h=s.sprites->create(token,layer,mode,-1,-1,false,s.pause_95b214);return h==~0u?-1:std::int32_t(h);}
    std::int32_t sprite_create_range(std::uint32_t token,std::uint32_t layer,std::uint32_t mode,std::int32_t first,std::int32_t last){
        if(!s.sprites){fail(0x428460);return -1;}
        const auto h=s.sprites->create(token,layer,mode,first,last,true,s.pause_95b214);return h==~0u?-1:std::int32_t(h);}
    void sprite_release(std::int32_t h){if(h>=0&&s.sprites)s.sprites->release(std::uint32_t(h));}
    void sprite_speed(std::int32_t h,float v){
        if(h<0)return;if(!s.sprites){fail(0x428800);return;}
        if(!s.sprites->set_speed(std::uint32_t(h),v))fail(0x428800);}
    // Small getters.
    std::uint32_t variant(){return u32(0x780258);}
    std::uint32_t mode(){return u32(0x78026c);}
    std::uint32_t car(){return u32(0x799d18);}
    std::uint32_t mission_type(){const auto r=u32(0x83637c);return r?u32(r+0x20):0u;}     // 495B20
    bool selection(){return m.u8(0x836374)!=0;}                                          // 4957F0
    std::uint32_t racer_count(){return u32(0x80fb2c);}                                   // 4774A0
    std::uint32_t player_count_496440(){if(variant()==4u){fail(0x456e30);return 0;}return racer_count();}
    // 45A2B0 CommRace_GetRank: the protected entry is cmp [78026C],0x10. Outside
    // mode 16 it returns byte [7DF118 + id]; in mode 16 the network variants 3/4
    // continue into 456870/459D10/459E10 (not ported), the others return AL = 0.
    bool rank_45a2b0(std::uint8_t id,std::uint32_t& out){
        if(mode()!=16u){out=m.u8(0x7df118+id);return true;}
        const auto v=variant();
        if(v==3u||v==4u){if(!s.lan_rank||!s.lan_rank(s.lan_user,id,out))return fail(0x45a2d6);out&=0xffu;return true;}
        out=0;return true;}
    std::uint32_t car_stage_450380(std::uint32_t index){return u32(u32(0x799b38+index*0x3c)+0x68);}
    // 44C8D0 course record by key, 44C940 cached level [rec+8], 44DC50.
    std::uint32_t course_record(std::uint32_t key){
        const auto count=i32(0x7d33c4);if(count<=0)return 0;const auto base=u32(0x7d33bc);
        for(std::int32_t i=0;i<count;++i)if(u32(base+std::uint32_t(i)*0x78+4)==key)return base+std::uint32_t(i)*0x78;
        return 0;}
    std::uint32_t level_44c940(std::uint32_t key){
        if(key==u32(0x635f2c))return u32(0x635f30);
        const auto r=course_record(key);const auto v=r?u32(r+8):0u;
        m.put32(0x635f2c,key);m.put32(0x635f30,v);return v;}
    std::uint32_t unique_44dc50(std::uint32_t key){const auto r=course_record(key);return r?u32(u32(r+0x14)):u32(u32(0x7d2df4));}
    // 47DA80 rival racer, 477DC0(rank) racer by rank table.
    std::uint32_t racer_by_rank(std::uint8_t rank){
        if(std::int32_t(rank)>=i32(0x80fb2c))return 0;
        const auto idx=i32(u32(0x80fb1c)+std::uint32_t(rank)*20+0x10);if(idx<0)return 0;
        return u32(0x80fb00)+std::uint32_t(idx)*0xa0;}
    std::uint32_t rival_47da80(){
        if(!u32(0x80fb00)||i32(0x80fb2c)<=1)return 0;
        const auto i=i32(0x64e190);if(i>=0)return u32(0x80fb00)+std::uint32_t(i)*0xa0;
        return racer_by_rank(1);}
};
// ------------------------------------------------------------------ init
void clear_4b8e40(N& n){for(std::uint32_t a=0x844630;a<=0x8446ac;a+=4)n.m.put32(a,0xffffffffu);}
void quest_45c010(N& n){
    auto& m=n.m;
    for(auto a:{0x7f2434u,0x7f246cu,0x7f2474u})m.put32(a,0xffffffffu);
    for(std::uint32_t a=0x7f24ec;a<=0x7f2504;a+=4)m.put32(a,0);
    m.put32(0x7f2428,3);
    for(auto a:{0x7f2430u,0x7f2470u,0x7f24c8u,0x7f24d0u,0x7f24d4u,0x7f24e8u,0x7f24e4u,0x7f2478u,0x7f2508u,0x7f2438u,0x7f244cu,
                0x7f243cu,0x7f2450u,0x7f2440u,0x7f2454u,0x7f2444u,0x7f2458u,0x7f2448u,0x7f245cu,0x7f247cu,0x7f2460u,0x7f2464u,
                0x7f2468u,0x7f2524u,0x7f2528u})m.put32(a,0);
}
// 45C160: getQuestTableByLevel(level bl, stage esi).
std::uint32_t quest_table_45c160(N& n,std::uint8_t level,std::uint32_t stage){
    auto eax=n.unique_44dc50(stage);if(std::int32_t(eax)>0xe)eax-=0xf;
    if(level==1){const auto i=stage-3u;if(i<=0xbu)return i<7u?0xfu+i:0x1au+(i-7u);return eax;}
    if(level==2){const auto i=stage-6u;if(i<=8u)return i<4u?0x16u+i:0x1fu+(i-4u);return eax;}
    if(level==3){const auto i=stage-0xau;if(i<=4u)return 0x24u+i;return eax;}
    return eax;
}
// 45C2C0: getSpecialQuest(stage) on the 7F2548 entry of the stage level.
std::uint32_t special_quest_45c2c0(N& n,std::uint32_t stage){
    auto& m=n.m;const auto level=n.level_44c940(stage);const auto e=0x7f2548+level*0x44;
    auto finish=[&]{const auto v=quest_table_45c160(n,m.u8(e),stage);m.put32(e+4,v);return v;};
    m.put8(e,0);
    if(level==2u){
        for(std::uint32_t i=0;i<=4;++i)if(m.u8(0x7f2580+i)||m.u8(0x7f25c4+i))return finish();
        m.put8(e,1);return finish();
    }
    if(level==3u){
        auto one=[&]{m.put8(e,1);for(std::uint32_t i=0;i<=3;++i)if(m.u8(0x7f2608+i)){m.put8(e,0);return finish();}return finish();};
        if(m.i8(0x7f25d0)<=0)return one();
        for(std::uint32_t i=0;i<=4;++i)if(m.u8(0x7f2608+i))return one();
        m.put8(e,2);return finish();
    }
    if(level==4u){
        const auto state=m.i8(0x7f2614);
        const std::int32_t c=m.i8(0x7f264c),d=m.i8(0x7f264d),b=m.i8(0x7f264e),f=m.i8(0x7f264f),g=m.i8(0x7f2650);
        bool two;
        if(state==2){
            const std::int32_t sum=c+d+b+f+g+m.i8(0x7f2651);
            if(sum==0){m.put8(e,3);return finish();}
            two=(c+d+b+f+g)==0;                                                 // 45C355: sum of five == 0
        }else{
            if(state<=0)two=false;
            else two=(c+d+b+f+g)==0;
        }
        if(two){                                                                // 45C3D0
            m.put8(e,2);return finish();
        }
        m.put8(e,1);
        for(std::uint32_t i=0;i<=3;++i)if(m.u8(0x7f264c+i)){m.put8(e,0);return finish();}
        return finish();
    }
    return finish();
}
// 45C470 jump tables (45C6C8 / 45C730 by 78024C for quests >= 15, 45C798 by
// the 44DC50 id below 15): EXE path strings ("oso_qt_*.sz").
constexpr std::uint32_t QuestObjects45c6c8[26]{0x5b0028,0,0,0x5b0018,0,0x5b0008,0x5afff8,0,0x5affe8,0,0,0x5affd8,0x5affc8,0x5affb8,0,
    0x5affa8,0x5aff98,0x5aff88,0x5aff78,0x5aff68,0x5aff58,0x5aff48,0x5aff38,0x5aff28,0x5aff18,0x5aff08};
constexpr std::uint32_t QuestObjects45c730[26]{0,0x5afef8,0,0x5afee8,0,0x5afed8,0x5afec8,0x5afeb8,0x5afea8,0x5afe98,0x5afe88,0x5afe78,
    0,0,0,0,0,0x5afe68,0x5afe58,0,0,0x5afe48,0x5afe38,0x5afe58,0x5afe28,0x5afe18};
constexpr std::uint32_t QuestObjects45c798[15]{0,0x5afdf8,0x5afde8,0x5afdd8,0x5afdc8,0,0x5afe08,0x5afdb8,0x5afda8,0x5afd98,0,
    0x5afd88,0x5afd78,0x5afd68,0};
std::uint32_t quest_object_45c470(N& n,std::uint32_t stage){
    auto& m=n.m;
    const auto unique=n.unique_44dc50(stage);
    const auto level=n.level_44c940(stage);const auto e=0x7f2548+level*0x44;
    std::uint32_t quest;
    if(m.u8(e)!=0xff){quest=quest_table_45c160(n,m.u8(e),stage);m.put32(e+4,quest);}
    else quest=special_quest_45c2c0(n,stage);
    if(std::int32_t(quest)>=15){
        const auto k=quest-15u;if(k>0x19u)return 0;
        return m.u32(0x78024c)==0?QuestObjects45c6c8[k]:QuestObjects45c730[k];
    }
    if(m.u32(0x78024c)!=0)return 0;
    const auto k=unique-15u;return k>0xeu?0:QuestObjects45c798[k];
}
// 45C7E0(stage): the stage's quest kind: 1..3 for the quests 15..40 (byte table
// 45C858), 0 otherwise; updates the 7F2548 level entry like 45C470.
std::uint32_t quest_kind_45c7e0(N& n,std::uint32_t stage){
    auto& m=n.m;
    (void)n.level_44c940(stage);                                               // called twice on the PC
    const auto level=n.level_44c940(stage);const auto e=0x7f2548+level*0x44;
    std::uint32_t quest;
    if(m.u8(e)!=0xff){quest=quest_table_45c160(n,m.u8(e),stage);m.put32(e+4,quest);}
    else quest=special_quest_45c2c0(n,stage);
    static constexpr std::uint8_t Kind[26]{0,0,0,0,0,0,0,1,1,1,1,0,0,0,0,0,1,1,1,1,1,2,2,2,2,2};
    if(std::int32_t(quest)<15)return 0;
    const auto k=quest-15u;
    return k>0x19u?0u:Kind[k]+1u;
}
bool quest_45e050(N& n){
    auto& m=n.m;const auto stage=m.u32(n.car()+0x68);
    const auto level=n.level_44c940(stage);const auto e=0x7f2548+level*0x44;
    std::uint32_t quest;
    if(m.u8(e)!=0xff){quest=quest_table_45c160(n,m.u8(e),stage);m.put32(e+4,quest);}
    else quest=special_quest_45c2c0(n,stage);
    const std::uint8_t* table=(m.u32(0x78024c)==0?Exe5a94d8:Exe5ac818);
    const std::size_t base=std::size_t(quest)*0x140;
    bool bad=false;
    auto word=[&](std::size_t o){if(base+o+2>sizeof(Exe5a94d8)){bad=true;return std::int16_t(0);}std::int16_t v;std::memcpy(&v,table+base+o,2);return v;};
    std::uint32_t count=0;m.put32(0x7f24cc,0);
    if(word(0)>0){
        std::size_t p=4;
        for(std::uint32_t i=0;i<0x20;){
            if(word(p-2)>0){const auto k=word(p);if(k>=0&&k<0x1a)m.put32(0x7f24cc,++count);}
            p+=0xa;++i;
            if(!(word(p-4)>0))break;
        }
    }
    if(bad)return n.fail(0x45e0c5);
    return true;
}
// 45DB70 (78024C == 0): the HEART ATTACK OSO objects, events 32..307. A live object of kind
// 0A / 22 / 39 takes its course height (43F2F0); kinds 5..9 (with the 5B0078 offset), 0B, 0E
// and 16 rebuild their ground matrix (45D950, jump tables 45DCC0 / 45DCCC). Those two course
// queries are not ported: a live object of such a kind reports its PC instead of being skipped.
bool course_objects_45db70(N& n){
    alignas(4) static std::uint8_t Case[0x12]{}; OR2_EXE_COPY(Case,0x45DCCCu,0x12u);
    for(std::uint32_t id=0x20u;id<0x20u+0x114u;++id){
        if((n.m.u8(0x79fb48u+id)&3u)!=2u)continue;
        const std::uint32_t w=n.u32(0x799b38u+id*0x3cu);
        const std::uint32_t kind=n.u32(w+0x5c);
        if(kind==0xau||kind==0x22u||kind==0x39u)return n.fail(0x43f2f0u);
        if(kind-5u<=0x11u&&Case[kind-5u]!=2u)return n.fail(0x45d950u);
    }
    return true;
}
bool quest_init_45e0f0(N& n){
    auto& m=n.m;
    for(auto a:{0x7f1c94u,0x7f23dcu,0x7f1cacu,0x7f23c4u,0x7f1c80u})m.put32(a,0);
    for(std::uint32_t a=0x7f2038;a<0x7f23b8;a+=0x38){m.put32(a,0);m.put32(a+0x24,0xffffffffu);}
    for(auto a:{0x7f8b20u,0x7f8b24u,0x7f1c64u,0x7f1994u,0x7f8b28u})m.put32(a,0);m.put16(0x7f8b2c,0);
    m.put32(0x7f8b30,0);m.put32(0x7f2988,0);m.putf(0x7f1c38,0.0f);m.putf(0x7f1c40,0.0f);
    m.put32(0x7f8b4c,0);m.put32(0x7f8b50,0);m.put32(0x7f8b54,0xffffffffu);m.put32(0x7f8b58,0xffffffffu);m.put16(0x7f8b5c,0);
    m.put32(0x7f8b60,0);m.put32(0x7f8b64,0);m.putf(0x7f8b6c,0.0f);m.putf(0x7f8b70,0.0f);m.put16(0x7f8bb4,0);m.put16(0x7f8bb6,0);
    m.put32(0x7f8b68,0xffffffffu);
    quest_45c010(n);
    m.put32(0x7f1978,0);m.put32(0x7f8afc,0);m.put32(0x7f19f8,0);
    for(std::uint32_t a=0x7f1a1c;a<0x7f1c4c;a+=0x1c){m.put32(a-0x14,0);m.put16(a+4,0xffffu);m.put32(a,0xffffffffu);}
    for(auto a:{0x7f1c54u,0x7f1c58u,0x7f1c5cu})m.put32(a,0);
    m.put32(0x7f1c3c,0);m.put16(0x7f1ca8,0);m.put32(0x7f1c60,0);m.put16(0x7f1c5a,0xffffu);
    if(m.u32(0x78024c)==0){
        if(!course_objects_45db70(n))return false;
    }
    if(!quest_45e050(n))return false;
    m.put32(0x7f24b8,0);m.put32(0x7f24bc,0);
    for(std::uint32_t edx=0x7f2550;edx<0x7f294c;edx+=0x44){
        const auto e=edx-8;for(std::uint32_t k=0;k<0x44;k+=4)m.put32(e+k,0);
        m.put32(edx-4,0x29);for(std::uint32_t k=0;k<=0x10;k+=4)m.put32(edx+k,0xffffffffu);
        m.put8(e,0xff);m.put32(edx+0x30,0);m.put32(edx+0x34,0);m.put32(edx+0x14,0);m.put8(edx+0x38,0);
    }
    m.put32(0x7f8b34,0);
    return true;
}
void clear_4ba690(N& n){
    auto& m=n.m;
    for(std::uint32_t k=0;k<4;++k){m.put32(0x8446b0+k*4,0);m.put32(0x8446d4+k*4,0);m.put32(0x844894+k*4,0);}
    m.put32(0x8448a4,0);
    for(std::uint32_t k=0;k<8;++k){m.put32(0x844780+k*4,0);m.put32(0x84481c+k*4,0);m.put32(0x844710+k*4,0);}
    for(std::uint32_t k=0;k<0x10;++k)m.put32(0x844848+k*4,0);
    m.put32(0x84472c,0);m.put16(0x844840,0);m.putf(0x8448ac,0.0f);
}
// 4BC390 (end 8444CC) / 4BC5B0 (end 843AEC): per-car blocks at 842C1C.
bool car_blocks(N& n,std::uint32_t end){
    auto& m=n.m;
    const std::uint32_t cars[6]{m.u32(0x799d18),m.u32(0x799d54),m.u32(0x799d90),m.u32(0x799dcc),m.u32(0x799e08),m.u32(0x799e44)};
    std::uint32_t mask=1;
    if((m.u8(0x79fb51)&3)==2&&m.u8(cars[1]+0x12))mask=3;
    if((m.u8(0x79fb52)&3)==2&&m.u8(cars[2]+0x12))mask|=4;
    if((m.u8(0x79fb53)&3)==2&&m.u8(cars[3]+0x12))mask|=8;
    if((m.u8(0x79fb54)&3)==2&&m.u8(cars[4]+0x12))mask|=0x10;
    if((m.u8(0x79fb55)&3)==2&&m.u8(cars[5]+0x12))mask|=0x20;
    const std::int32_t bits=std::int16_t(mask);
    std::uint32_t idx=0;
    for(std::uint32_t esi=0x842c1c;esi<end;esi+=0x278,++idx){
        if(!(bits&(1<<idx)))continue;
        if(idx>=6)return n.fail(0x4bc478);                 // [esp+ebp*4+0x14] beyond the six car pointers
        const auto car=cars[idx];
        m.put32(esi-4,0);m.put32(esi,0);m.put16(esi+4,0);m.put16(esi+6,0);m.putf(esi+8,0.0f);m.put32(esi+0xc,0);
        std::uint32_t rank{};
        if(!n.rank_45a2b0(m.u8(car+0x10),rank))return false;m.put32(esi+0x10,rank);
        if(!n.rank_45a2b0(m.u8(car+0x10),rank))return false;m.put32(esi+0x14,rank);
        m.put32(esi+0x18,0);
        for(std::uint32_t k=0;k<0xf;++k){m.put32(esi+0x1c+k*4,0);m.put32(esi+0x58+k*4,0xffffffffu);}
        for(auto o:{0x258u,0x25cu,0x260u,0x264u,0x26cu,0x268u})m.putf(esi+o,0.0f);
        m.put32(esi+0x94,0);m.put32(esi+0x98,0);m.put32(esi+0x9c,0xffffffffu);m.put32(esi+0xa0,0);m.put32(esi+0xa4,0);m.put32(esi+0xa8,0);
        m.put32(esi+0xac,0xffffffffu);m.put32(esi+0xb0,0);m.put32(esi+0xbc,0);m.put32(esi+0xc4,0);m.put32(esi+0xcc,0);m.put32(esi+0xd4,0);
        m.putf(esi+0x270,-45.0f);
    }
    return true;
}
void clear_4ba850(N& n){
    auto& m=n.m;m.put8(0x8447a0,0xff);m.put8(0x8447ec,0xff);m.put8(0x844888,0xff);
    std::uint8_t k=0;
    for(std::uint32_t a=0x842aa0;a<0x842bf0;a+=0x38,++k){
        m.put8(a+0xc,k);m.put8(a+0xd,k);m.put32(a+0x14,0);m.put8(a,0);m.put8(a+1,0);m.put32(a+8,0);m.put8(a+0xe,0xff);
        m.putf(a+0x1c,0.0f);m.put8(a+0xf,0);m.put8(a+0x10,1);m.put32(a+0x18,0);m.put32(a+0x2c,0);m.put32(a+0x30,0);
        m.putf(a+0x20,0.0f);m.putf(a+0x24,0.0f);m.putf(a+0x28,0.0f);m.put32(a+0x34,0xffffffffu);
    }
}
}
bool navi_pub_init_4bc9e0_impl(NaviPubServices& s){
    if(!s.m)return false;
    N n(s,*s.m);auto& m=n.m;
    m.put32(0x84477c,0);
    float scale;
    if(m.u32(0x7c24bc)==0){scale=1.0f;m.put32(0x844778,0x2c00af);m.put32(0x842c0c,0x2c00ae);}
    else{scale=0.6215000152587891f;m.put32(0x844778,0x2c00b1);m.put32(0x842c0c,0x2c00b0);}
    m.put32(0x8447f8,1);m.put32(0x8446ec,1);m.put32(0x8446d0,1);
    for(std::uint32_t a=0x8447ac;a<=0x8447e0;a+=4)m.put32(a,0xffffffffu);
    m.putf(0x688b30,scale);m.put32(0x8446c8,0);m.put32(0x8446e4,0);m.put16(0x8446e8,0);m.put16(0x8447f4,0);
    m.put32(0x842bf0,0xffffffffu);m.put16(0x8447a4,0);m.put32(0x8447e4,0xffffffffu);m.put32(0x844844,0xffffffffu);m.put32(0x84488c,0xffffffffu);
    {const auto count=m.i32(0x680ad4);if(count>0)for(std::int32_t k=0;k<count;++k)m.put32(0x8447fc+std::uint32_t(k)*4,0);}
    clear_4b8e40(n);
    m.put16(0x844708,0);m.put16(0x844900,0);m.put32(0x8447f0,0);m.put32(0x844904,0);m.put32(0x842c10,0);
    if(!quest_init_45e0f0(n))return false;
    m.put32(0x688b34,0xffffffffu);
    clear_4ba690(n);
    if(!car_blocks(n,0x8444cc))return false;
    m.put32(0x844890,0);m.put32(0x8447a8,0xffffffffu);m.put32(0x8446cc,0xffffffffu);m.putf(0x84470c,200.0f);
    m.put32(0x844704,0xffffffffu);m.put32(0x842a9c,0xffffffffu);
    if(!car_blocks(n,0x843aec))return false;
    m.put32(0x8447a8,0xffffffffu);m.put32(0x844890,0);m.put32(0x8446cc,0xffffffffu);m.putf(0x84470c,200.0f);
    m.put32(0x844704,0xffffffffu);m.put32(0x842a9c,0xffffffffu);
    clear_4ba850(n);
    for(std::uint32_t k=0;k<0x78;++k)m.put32(0x8428b8+k*4,0);
    for(std::uint32_t a=0x8444d4;a<=0x844618;a+=0x24)m.put32(a,0xffffffffu);
    m.put32(0x8448a8,0xffffffffu);m.put32(0x842a98,0xffffffffu);m.put32(0x844700,0xffffffffu);
    m.put32(0x8446fc,0);m.put32(0x8446c0,0);
    for(std::uint32_t a=0x8448b0;a<=0x8448fc;a+=4)m.putf(a,0.0f);
    m.putf(0x8446c4,0.0f);
    return !n.faulted()&&!s.missing;
}
// ------------------------------------------------------------------ destroy
bool navi_pub_destroy_4b8df0_impl(NaviPubServices& s){
    if(!s.m)return false;
    N n(s,*s.m);auto& m=n.m;
    if(m.i32(0x844630)>=0){n.sprite_release(m.i32(0x844630));m.put32(0x844630,0xffffffffu);}
    for(std::uint32_t a=0x7f2038;a<0x7f23b8;a+=0x38)
        if(m.u32(a)){n.sprite_release(m.i32(a+0x24));m.put32(a+0x24,0xffffffffu);m.put32(a,0);}
    for(std::uint32_t a=0x7f1a1c;a<0x7f1c4c;a+=0x1c)
        if(m.u32(a-0x14)){n.sprite_release(m.i32(a));m.put32(a,0xffffffffu);}
    return !n.faulted()&&!s.missing;
}
// ------------------------------------------------------------------ control
namespace {
void timer_4b93e0(N& n){
    auto& m=n.m;
    if(m.i16(0x8446e8)>0){if(m.u8(0x780248)==0)m.put16(0x8446e8,std::uint16_t(m.i16(0x8446e8)-1));return;}
    if(((m.u32(0x7d39f0)>>1)&1u)==0)return;                                      // 450130
    m.put16(0x8446e8,0xf0);
    if(((m.u32(0x7d39f0)>>3)&1u)==0)return;                                      // 450160
    n.sound(0x4d);
    const auto v=n.variant();n.sound(v==6u||v==4u?0x1e5u:0x16cu);
}
void countdown_4b9cd0(N& n){
    auto& m=n.m;
    auto t=m.i32(0x7d394c);if(t<0)t=0;                                           // 44FE40
    if(m.u32(0x7d38f0)!=0)return;                                                 // 450240
    if(t>0x258){m.put32(0x688b38,0x3e7);return;}
    std::int32_t mask,target;
    if(t<=0xf0){mask=3;target=2;}else if(t<=0x1a4){mask=7;target=5;}else{mask=0xf;target=0xc;}
    const auto v=mask&t;m.put32(0x688b38,std::uint32_t(target));m.put32(0x844908,std::uint32_t(v));
    if(v!=target)return;
    const auto var=n.variant();
    if(var>=3u&&var<=4u&&m.u8(0x7de784)!=0)n.sound(0xf7);else n.sound(0x518d);
}
bool colour_46c840(N& n,std::uint8_t key,std::uint8_t& out){
    for(std::uint32_t i=0;i*2+1<sizeof(Exe5b3c34);++i){const auto a=std::int8_t(Exe5b3c34[i*2]);if(a>=0&&std::uint8_t(a)==key){out=Exe5b3c34[i*2+1];return true;}}
    return n.fail(0x46c850);
}
bool ranks_4ba8c0(N& n,std::uint32_t car){
    auto& m=n.m;
    if(n.mode()!=16u)return true;
    for(std::uint32_t a=0x842ab4;a<=0x842bcc;a+=0x38)m.put32(a,0);
    if(std::int32_t(n.player_count_496440())<=0)return !n.s.missing;
    std::uint32_t esi=0x842aa0;
    for(std::uint32_t k=0;std::int32_t(k)<std::int32_t(n.player_count_496440());++k,esi+=0x38){
        const auto r=n.racer_by_rank(std::uint8_t(k));
        m.put8(esi+1,m.u8(esi));m.put8(esi+0xd,m.u8(esi+0xc));m.put8(esi,std::uint8_t(k+1));
        std::uint32_t pos;
        if(r){
            m.put32(esi+8,m.u32(r+0x98));m.put8(esi+0xc,std::uint8_t(k));m.put32(esi+0x14,1);
            m.put8(esi+0xf,m.u8(r+0x72));m.put8(esi+0x10,m.u8(r+0x73));m.put32(esi+0x18,0);
            pos=m.u8(r+0x74);
        }else{
            m.put32(esi+8,m.u32(0x836388));m.put8(esi+0xc,std::uint8_t(k));m.put32(esi+0x14,1);
            std::uint8_t colour{};if(!colour_46c840(n,m.u8(car+0x11),colour))return false;m.put8(esi+0xf,colour);m.put8(esi+0x10,m.u8(car+0x12));m.put32(esi+0x18,1);
            pos=m.u8(car+0xc36);
        }
        m.put32(esi+4,pos==n.player_count_496440()-1u?1u:0u);
    }
    return !n.s.missing;
}
bool sectors_4ba3a0(N& n);                                  // variants 0 / 7, below
}
// 4BC7D0(EAX = player car) (LAN races): for the player and events 9..13 whose cars run (open,
// +12) the 842C1C records (stride 0x278): +C = 0, +0 = 44C940(+68) level, +10 = 45A2B0(+10)
// rank, and for presets 0..4 the progress +8 = (level + car +102C) * 36 (presets 2/3: 12).
bool lan_cars_4bc7d0(N& n,std::uint32_t car){
    auto& m=n.m;
    const std::uint32_t cars[6]{car,m.u32(0x799d54),m.u32(0x799d90),m.u32(0x799dcc),m.u32(0x799e08),m.u32(0x799e44)};
    std::uint32_t mask=1u;
    if((m.u8(0x79fb51)&3u)==2u&&m.u8(cars[1]+0x12))mask=3u;
    for(std::uint32_t k=2;k<6u;++k)if((m.u8(0x79fb48+8u+k)&3u)==2u&&m.u8(cars[k]+0x12))mask|=1u<<k;
    for(std::uint32_t k=0;k<6u;++k){
        if(!(mask&(1u<<k)))continue;
        const std::uint32_t c=cars[k],rec=0x842c1cu+k*0x278u;
        m.put32(rec+0xc,0);
        const auto level=n.level_44c940(m.u32(c+0x68));
        if(m.u32(rec)!=level)m.put32(rec,n.level_44c940(m.u32(c+0x68)));
        std::uint32_t rank{};if(!n.rank_45a2b0(m.u8(c+0x10),rank))return false;
        if(m.u32(rec+0x10)!=rank){if(!n.rank_45a2b0(m.u8(c+0x10),rank))return false;m.put32(rec+0x10,rank);}
        const std::uint32_t preset=m.u32(0x78024c);
        if(preset>4u)continue;
        const float scale=(preset==2u||preset==3u)?12.0f:36.0f;           // 6281E8 / 59DCCC
        m.putf(rec+8u,(float(m.i32(rec))+m.f32(c+0x102c))*scale);
    }
    return true;
}
bool navi_pub_control_4bcc40_impl(NaviPubServices& s){
    if(!s.m)return false;
    N n(s,*s.m);auto& m=n.m;
    const auto car=n.car();
    const auto stage=n.car_stage_450380(m.u32(car));
    if(m.u32(0x844844)!=stage){
        const auto level=n.level_44c940(stage);
        const auto c=m.u32(m.u32(0x7d3188)+0x74);
        m.put32(0x8447ac+level*4,c==0?stage:c-1);
        m.put32(0x84488c,m.u32(0x8447ac+level*4));m.put32(0x844844,stage);
    }
    const auto var=n.variant();
    if(n.mode()==16u){
        timer_4b93e0(n);
        if(var>7u){}
        else if(var==6u){
            const auto r=m.u32(0x83637c);
            // 4962A0 -> 4BA790 (type-4 missions): at 300 (450670 = [7D3934]) the sectors 4BA3A0 and sound 0x5D.
            if(m.u8(0x836374)&&r&&m.u32(r+0x20)==4u&&m.u32(0x7d3934)==0x12cu){if(!sectors_4ba3a0(n))return false;n.sound(0x5d);}
        }else if(var==2u){if(!s.heart||!s.heart(s.heart_user,0x464d20u))return n.fail(0x464d20);}
        else if((var==0u||var==7u)&&m.u32(0x7d3934)==0x12cu){if(!sectors_4ba3a0(n))return false;n.sound(0x5d);}
        countdown_4b9cd0(n);
    }
    if(n.selection()){const auto t=n.mission_type();if(t==1u||t==6u)if(!ranks_4ba8c0(n,car))return false;}
    if((var==3u||var==4u)&&!lan_cars_4bc7d0(n,n.car()))return false;
    return !n.faulted()&&!s.missing;
}
// ------------------------------------------------------------------ display
bool navi_digits_4ba9d0_impl(NaviPubServices& s,std::uint32_t style,std::int32_t x,std::int32_t y,const std::string& text,
                        std::uint32_t layer,float alpha){
    N n(s,*s.m);auto& m=n.m;
    // edi = 5C6DF8 + style*16 {token base, advance, x offset f32, y offset f32};
    // the protected bridge sets ecx = 688DE0 + style*40 (ten image tokens).
    const std::size_t so=(0x5c6df8-0x5c6c00)+std::size_t(style)*16;
    if(so+16>sizeof(Exe5c6c00))return n.fail(0x4ba9f0);
    auto sw=[&](std::size_t o){std::uint32_t v;std::memcpy(&v,Exe5c6c00+so+o,4);return v;};
    const std::uint32_t digits=0x688de0+style*40;
    const std::int32_t advance=std::int32_t(sw(4));std::int32_t offset=0;
    for(unsigned char ch:text){
        if(ch==0)break;
        if(ch>='0'&&ch<='9'){
            const std::uint32_t d=ch-'0';
            if(std::int32_t(sw(0))<0){
                const std::int32_t px=cvtt(float(offset+x)+bf(sw(8)));
                const std::int32_t py=cvtt(float(y)+bf(sw(0xc)));
                const auto colour=(m.u32(0x7551b4)&0xffffffu)|(ftol(X87(alpha)*X87(255.0f))<<24);
                n.draw(0x42d280,{m.u32(digits+d*4),std::uint32_t(px),std::uint32_t(py),0,fb(float(std::int32_t(layer))),colour});
            }else{
                const float px=(float(offset)+bf(sw(8)))+float(x);
                const float py=float(y)+bf(sw(0xc));
                n.draw(0x429580,{sw(0),fb(px),fb(py),layer,d,fb(alpha)});
            }
        }else{
            // 42CC30 reads the text cursor into unused locals; 42CC00(x, y + advance).
            const std::int32_t px=x,py=y+advance;
            m.put16(0x956bb8,std::uint16_t(px));m.put16(0x956bba,std::uint16_t(py));
            m.put16(0x956bb4,std::uint16_t(px));m.put16(0x956bb6,std::uint16_t(py));
            n.draw(0x42cc00,{std::uint32_t(px),std::uint32_t(py)});
        }
        offset+=advance;
    }
    return !n.faulted()&&!s.missing;
}
}
// ------------------------------------------------------------------ display pieces
namespace outrun::platform {
namespace {
// .rdata dword from the embedded 5C6C00..5C70C0 block.
bool rd32(N& n,std::uint32_t a,std::uint32_t& v){
    if(a<0x5c6c00||a+4>0x5c6c00+sizeof(Exe5c6c00))return n.fail(a);
    std::memcpy(&v,Exe5c6c00+(a-0x5c6c00),4);return true;}
bool rd8s(N& n,std::uint32_t a,std::int32_t& v){
    if(a<0x5c6c00||a+1>0x5c6c00+sizeof(Exe5c6c00))return n.fail(a);
    v=std::int8_t(Exe5c6c00[a-0x5c6c00]);return true;}
constexpr std::uint32_t F1=0x3f800000u,F4=0x40800000u,F2=0x40000000u;
bool digits(N& n,std::uint32_t style,std::int32_t x,std::int32_t y,const std::string& t){
    return navi_digits_4ba9d0_impl(n.s,style,x,y,t,9,1.0f);}
// 4B9B40(car): course icon, rival and own progress markers.
bool progress_4b9b40(N& n,std::uint32_t car){
    auto& m=n.m;
    static constexpr std::uint8_t pick[14]{0,1,2,3,5,5,5,5,5,5,5,5,5,4};
    static constexpr std::uint32_t tokens[5]{0x2c0118,0x2c0119,0x2c011a,0x2d0001,0x2d0002};
    const auto i=m.u32(0x7d33c0)-1u;
    if(i<=0xdu&&pick[i]<5)n.draw(0x429530,{tokens[pick[i]],0,0,1,0});
    const auto len=fild_u32(m.u32(0x803710));
    if(const auto r=n.rival_47da80()){
        const float v=driving::x87_float(fild_u32(m.u32(r+0x3c)));      // fstp [esp]
        n.draw(0x429530,{0x2c011b,0,0,0,ftol(X87(v)/len*X87(180.0f))});
    }
    const float mine=float(m.u16(car+0x260));
    n.draw(0x429530,{0x2c011c,0,0,0,ftol(X87(mine)/len*X87(180.0f))});
    return true;
}
// 4BD340 (mission type 6): score and rival score.
bool scores_4bd340(N& n){
    auto& m=n.m;
    const auto r=n.rival_47da80();const bool first=r&&m.u8(r+0x74)==0;
    auto buf=format_int("%04d",(m.i32(0x836388)+5)/6);
    n.draw(0x42d280,{0x2c019e,0x1e1,0x36,0,F4,0xffffffffu});
    if(!digits(n,first?0xd:1,0x227,0x49,buf))return false;
    if(r){
        buf=format_int("%04d",(m.i32(r+0x98)+5)/6);
        n.draw(0x42d280,{0x2c019e,0x1e1,0x54,0,F4,0xffffffffu});
        if(!digits(n,first?1:0xd,0x227,0x67,buf))return false;
        n.draw(0x42d280,{0x2c027b,m.u32(0x688d60)+0x1f5,m.u32(0x688d64)+0x67,0,F4,0xffffffffu});
    }
    return true;
}
std::string score7(std::int32_t v){return v>9999999?std::string("9999999"):format_int("%07d",v);}
// 4BD230 (types 2/3): scores through 4BC990/4BA9D0.
bool scores_4bd230(N& n,std::uint32_t car){
    auto& m=n.m;
    const auto buf=score7(m.i32(0x836388));
    const bool live=(m.u8(car+0x244)&0x9c)==0&&m.i8(car+0xda4)<=0&&(m.u8(car+6)&1)==0;
    if(!digits(n,live?1:0xd,0x1e1,0x49,buf))return false;
    if(const auto r=n.rival_47da80()){
        if(!digits(n,1,0x1e1,0x67,score7(m.i32(r+0x98))))return false;
        n.draw(0x42d280,{0x2c027b,m.u32(0x688d60)+0x1e1,m.u32(0x688d64)+0x67,0,F4,0xffffffffu});
    }
    return true;
}
void text_pos_42cc00(N& n,std::int32_t x,std::int32_t y);   // below (Heart Attack texts)
// 4B99E0(car) (LAN races, 456D60 > 1 players): the 2D0001 / 2D0002 header (preset 0/1/4 or
// 2/3), then for the player and events 9..13 whose cars run (flags & 3 == 2 and car +12) the
// rank of their CommRace slot (+1054): 5C6DDC[slot] at frame [7DE47C + slot * 0x6C].
void ranks_4b99e0(N& n,std::uint32_t car){
    auto& m=n.m;
    if(m.u8(0x7de418)<=1u)return;
    const std::uint32_t cars[6]{car,m.u32(0x799d54),m.u32(0x799d90),m.u32(0x799dcc),m.u32(0x799e08),m.u32(0x799e44)};
    std::uint32_t mask=1u;
    for(std::uint32_t k=1;k<6u;++k)
        if((m.u8(0x79fb48+8u+k)&3u)==2u&&m.u8(cars[k]+0x12))mask|=k==1u?3u:(1u<<k);
    const std::uint32_t preset=m.u32(0x78024c);
    if(preset<=4u)n.draw(0x429530,{preset==2u||preset==3u?0x2d0002u:0x2d0001u,0,0,1,0});
    for(std::uint32_t i=0;i<6u;++i){
        if(!(mask&(1u<<i)))continue;
        const std::uint32_t slot=m.u32(cars[i]+0x1054);
        n.draw(0x429530,{m.u32(0x5c6ddc+slot*4u),0,0,4,m.u8(0x7de47c+slot*0x6cu)});
    }
}
// 42CDD0 with the resolved text in the record (proportional, the run's 42CA60 / 42C360 /
// 42CCA0 / 42CC00 style and cursor).
void text_42cdd0(N& n,const std::string& text){
    RaceHudDraw d;d.pc=0x42cdd0;
    std::memcpy(d.record.data(),text.data(),std::min(text.size(),d.record.size()-1u));
    n.emit(d);
}
// 456540 = 4562C0(7EE070, [63E0BC], [63E0C0]): the LAN message queue (16 entries of 0x18:
// +0 text id, +4 player name) owned by the network module: the current entry ([+180]) fades
// in / out over [+188] frames, the name over the message (two lines and a shadow when
// [+18C] + [+190] > 0x80, else one "%s %s" line).
bool messages_456540(N& n){
    auto& m=n.m;
    constexpr std::uint32_t Q=0x7ee070;
    if(!m.r.mapped(Q,0x198))return true;                    // no LAN session
    if(!m.u32(Q+0x194))return true;
    const std::int32_t x=m.i32(0x63e0bc),y=m.i32(0x63e0c0);
    std::int32_t t=m.i32(Q+0x188);
    std::uint32_t alpha;
    auto cvtt=[](float f){return std::uint32_t(std::int32_t(f));};
    if(t>0xd2)alpha=cvtt((1.0f-float(t-0xd2)*0.0333333351f)*255.0f);
    else if(t>0x1e)alpha=0xff;
    else alpha=cvtt(float(t)*0.0333333351f*255.0f);
    n.draw(0x42ca60,{9});n.draw(0x42c360,{0x101});
    const std::uint32_t idx=m.u32(Q+0x180),rec=Q+idx*0x18u;
    std::string name;for(std::uint32_t k=0;k<20u;++k){const auto ch=m.u8(rec+4u+k);if(!ch)break;name+=char(ch);}
    const std::string* msg=n.s.text?n.s.text->get(m.u32(rec)):nullptr;
    if(!msg)return n.fail(0x465eb0);
    const std::int32_t w1=m.i32(Q+0x18c),w2=m.i32(Q+0x190);
    const std::uint32_t shadow=alpha<<24,front=(alpha<<24)|0xffffffu;
    if(w1+w2>0x80){
        n.draw(0x42cca0,{shadow});
        text_pos_42cc00(n,x-w1+1,y-0x23);text_42cdd0(n,name);
        text_pos_42cc00(n,x-w2+1,y-0x11);text_42cdd0(n,*msg);
        n.draw(0x42cca0,{front});
        text_pos_42cc00(n,x-w1,y-0x24);text_42cdd0(n,name);
        text_pos_42cc00(n,x-w2,y-0x12);text_42cdd0(n,*msg);
    }else{
        n.draw(0x42cca0,{shadow});
        text_pos_42cc00(n,x-w1-w2-7,y-0x11);text_42cdd0(n,name+" "+*msg);
        n.draw(0x42cca0,{front});
        text_pos_42cc00(n,x-w1-w2-8,y-0x12);text_42cdd0(n,name+" "+*msg);
    }
    t-=m.i32(0x780278);                                    // 43FA00
    m.put32(Q+0x188,std::uint32_t(t));
    if(t<=0){
        m.put32(Q+0x180,(idx+1u)&0xfu);m.put32(Q+0x188,0xf0);m.put32(Q+0x194,m.u32(Q+0x194)-1u);
    }
    return true;
}
// 4B9D80(car) (types 0/1): position and racer count.
bool position_4b9d80(N& n,std::uint32_t car){
    auto& m=n.m;
    if(m.u32(0x688b3c))return true;
    n.draw(0x429530,{0x2b005f,0,0,1,0});
    const auto count=std::uint8_t(n.player_count_496440());
    if(n.s.missing)return false;
    const auto p=std::uint8_t(m.u8(car+0xc36)+1);                                // 496410 (variant != 4)
    if(p==0)return true;
    const std::uint8_t pos=p<=count?p:count;
    if(m.u32(0x689200)==0&&m.u32(car+0x5c)==1&&m.i16(car+0x64)>100)n.sound(0x518d);
    m.put32(0x689200,1);
    std::uint32_t suffix=3;
    if(m.u32(0x7d2698)){if(pos==1)suffix=0;}
    else if(pos/10!=1)suffix=m.u32(0x688d68+(pos%10)*4);
    const std::uint32_t tens=pos/10,ones=pos%10,ctens=count/10,cones=count%10;
    std::uint32_t colour=0xffffffffu,colour2=0xffffffffu;
    if(n.mission_type()==1u&&pos==std::uint8_t(cones)){
        float v=m.f32(0x844924);
        if(m.u8(0x6891fe)){v=v+0.029999999329447746f;m.putf(0x844924,v);
            if(v>=1.0f){m.put8(0x6891fe,0);m.putf(0x844924,1.0f);}}
        else{v=v-0.029999999329447746f;m.putf(0x844924,v);
            if(0.0f>=v){m.put8(0x6891fe,1);m.putf(0x844924,0.0f);}}
        const auto a=ftol((X87(1.0f)-X87(m.f32(0x844924)))*X87(255.0f))&0xffu;
        colour=(a<<16)|0xff003030u;colour2=colour;
    }
    std::uint32_t t;std::int32_t o1,o2;
    auto tok=[&](std::uint32_t base,std::uint32_t i,std::uint32_t& v){return rd32(n,base+i*4,v);};
    if(tens>0){
        o1=std::int8_t(m.u8(0x6891f4+tens));o2=std::int8_t(m.u8(0x6891e8+ones));
        if(!tok(0x5c6cc0,tens,t))return false;
        n.draw(0x42d280,{t,std::uint32_t(o1+o2+0x1be),0x42,0,F2,colour2});
        if(!tok(0x5c6cc0,ones,t))return false;
        n.draw(0x42d280,{t,std::uint32_t(o2+0x1e6),0x42,0,F2,colour2});
    }else{
        o2=std::int8_t(m.u8(0x6891e8+ones));
        if(!tok(0x5c6cc0,ones,t))return false;
        n.draw(0x42d280,{t,std::uint32_t(o2+0x1e6),0x42,0,F2,colour2});
    }
    const auto lang=m.u32(0x7d2698);
    if(!tok(0x5c6ce8,suffix,t))return false;
    n.draw(0x42d280,{t,0x20a,(lang==4u||lang==3u)?0x38u:0x56u,0,F2,colour});
    if(ctens>0){
        if(!tok(0x5c6c98,ctens,t))return false;n.draw(0x42d280,{t,0x22c,0x46,0,F2,0xffffffffu});
        if(!tok(0x5c6c98,cones,t))return false;n.draw(0x42d280,{t,0x23c,0x46,0,F2,0xffffffffu});
    }else{
        if(!tok(0x5c6c98,cones,t))return false;n.draw(0x42d280,{t,0x22c,0x46,0,F2,0xffffffffu});
    }
    n.draw(0x42d280,{0x3000f,0x21c,0x44,0,F2,0xffffffffu});
    return true;
}
// 4BCF10(car): speedometer digits and the speed-lines sprite.
bool speed_4bcf10(N& n,std::uint32_t car){
    auto& m=n.m;
    float scale;
    if(m.u32(0x7c24bc)==0){scale=1.0f;m.put32(0x844778,0x2c00af);m.put32(0x842c0c,0x2c00ae);}
    else{scale=0.6215000152587891f;m.put32(0x844778,0x2c00b1);m.put32(0x842c0c,0x2c00b0);}
    m.putf(0x688b30,scale);
    const auto buf=format_int("%03d",std::int32_t(ftol(X87(m.f32(0x688b30))*X87(m.f32(car+0x1f8)))));
    const std::int32_t lean=m.i16(car+0x162);
    if(m.f32(car+0xe68)>0.18000000715255737f&&lean>-1500&&lean<1500){
        if(!digits(n,0xb,0x78,0x191,buf))return false;
        n.draw(0x429530,{m.u32(0x842c0c),0,0,5,0});
        auto h=m.i32(0x844700);
        if(h<0){h=n.sprite_create(0x2c00cc,5,0);m.puti(0x844700,h);}
        const bool fast=n.variant()==1u&&m.f32(car+0x1c4)>m.f32(car+0xe64);
        n.sprite_speed(h,fast?6.0f:1.0f);
        return !n.s.missing;
    }
    if(!digits(n,2,0x78,0x191,buf))return false;
    n.draw(0x429530,{m.u32(0x844778),0,0,5,0});
    if(m.i32(0x844700)>=0){n.sprite_release(m.i32(0x844700));m.put32(0x844700,0xffffffffu);}
    return true;
}
// 4B8F30(car): tachometer needle through 4289B0 with T(-219, 127).
bool needle_4b8f30(N& n,std::uint32_t car){
    auto& m=n.m;
    const std::int32_t model=m.i8(car+0x11);
    std::uint32_t token;if(!rd32(n,0x5c6c00+std::uint32_t(model*4),token))return false;
    // fild (unsigned), fmul [5A29E0] (float 1e-4), fmul [5A29E4] (180): x87 products.
    auto frame=ftol(fild_u32(m.u32(car+0x20c))*X87(9.999999747378752e-05f)*X87(180.0f));
    if(std::int32_t(frame)>=0xb4)frame=0xb3;
    RaceHudDraw d;d.pc=0x4289b0;d.args={token,2,0,frame,0,0};
    d.matrix=sprani_translation(-219.0f,127.0f);                               // D3DXMatrixTranslation
    n.emit(d);
    return true;
}
// 4B9010(car): gear indicator and the manual shift warning.
bool gear_4b9010(N& n,std::uint32_t car){
    auto& m=n.m;
    const auto g=m.u32(car+0x208);const std::uint32_t idx=g<1u?1u:(g>6u?6u:g);
    const auto spec=m.u32(car+0x2b4);
    n.draw(0x429530,{m.u32(spec+0x10a0)==5u?0x2c00a1u:0x2c00a2u,0,0,8,0});
    std::uint32_t t;if(!rd32(n,0x5c6da8+idx*4,t))return false;
    n.draw(0x429530,{t,0,0,9,0});
    if(m.u8(car+0x13)!=1)return true;
    n.draw(0x42d280,{0x2c00d3,0x95,0x132,0,F4,0xffffffffu});
    n.draw(0x42d280,{0x2c0115,0x95,0x146,0,F4,0xffffffffu});
    if((m.u32(0x95af0c)&8u)&&m.u32(car+0x48)>0x1edcu&&m.u32(car+0x208)<m.u32(spec+0x10a0))
        n.draw(0x42d5f0,{0x30002,0x28,0x12,0x40a00000u,0xffffffffu,0x8000});
    return true;
}
// 4B9100: route map markers (not drawn during a mission unless variant 4).
bool route_4b9100(N& n){
    auto& m=n.m;
    if(m.u8(0x830394)&&m.i32(0x656234)<0x3c)return true;
    if(n.selection()&&n.variant()!=4u)return true;
    const auto preset=m.u32(0x78024c);const auto ebp=m.i32(0x84488c);
    std::uint32_t a=0,b=0,c=0;
    if(preset<=4u){if(preset==2u||preset==3u){a=0x2c0074;b=0x2c0073;c=0x2c0072;}else{a=0x2c0078;b=0x2c0076;c=0x2c0075;}}
    const auto x=m.u32(0x6890d8),y=m.u32(0x6890dc);
    n.draw(0x429530,{a,x,y,2,0});
    if(ebp!=-1)n.draw(0x429530,{b,x,y,3,std::uint32_t(ebp)});
    const std::int32_t last=(preset>=2u&&preset<=3u)?0xe:4;
    for(std::int32_t i=0;i<=last;++i){
        const auto v=m.i32(0x8447ac+std::uint32_t(i)*4);
        if(v<0||ebp==v)break;
        n.draw(0x429530,{c,x,y,3,std::uint32_t(v)});
    }
    return true;
}
// 4294C0(token, &half_w, &half_h) from the sprite bank's root component.
void half_size_4294c0(N& n,std::uint32_t token,float& w,float& h){
    FrontendSpriteTiming t{};
    if(!n.s.sprites||!n.s.sprites->bank_scene(token,t))return;
    w=float(std::int16_t(t.width))*0.5f;h=float(std::int16_t(t.height))*0.5f;
}
// 4B9450: next-stage banner.
bool stage_banner_4b9450(N& n){
    auto& m=n.m;
    float x=-340.0f;const auto car=n.car();const auto on=m.u32(car+0x5c);
    if(n.selection()&&n.variant()!=4u)x=float(m.i32(0x688b4c))-340.0f;
    if(on==0){m.put16(0x844900,0);return true;}
    const auto rec=m.u32(0x7d3188);const auto have=rec?m.u32(rec+4):0xfu;
    if(have==n.car_stage_450380(m.u32(car)))return true;
    const auto state=m.u16(0x844900);const auto next=m.u32(m.u32(m.u32(0x7d3188)+0x14));
    auto icon=[&]{
        const auto token=m.u32(0x6890e0+next*4);float w=0.0f,h=0.0f;half_size_4294c0(n,token,w,h);
        const float py=(h+5.0f)-240.0f;const float px=(w+x)-320.0f;
        n.draw(0x429530,{token,fb(px),fb(py),5,0});};
    if(state==0){m.put16(0x844900,1);m.put16(0x844708,1);return true;}
    // Port: 844708 counts displayed frames; not on display-only frames (above 60 Hz).
    const std::uint16_t step=enhancements::display_ticks()?1u:0u;
    if(state==2){icon();m.put16(0x844708,std::uint16_t(m.u16(0x844708)+step));return true;}
    if(state==1){
        if(m.u8(0x844708)&0x10)icon();
        if(m.u16(0x844708)==0x78){m.put16(0x844900,2);m.put16(0x844708,0x79);return true;}
    }
    m.put16(0x844708,std::uint16_t(m.u16(0x844708)+step));return true;
}
// 4BA070(car): wrong-way style sprite 842BF0.
void warning_4ba070(N& n,std::uint32_t car){
    auto& m=n.m;
    if(m.i16(0x8446e8)>0||m.i16(0x8447f4)>0||m.u8(car+0x13)!=1||m.u32(car+0xe80)==0){
        if(m.i32(0x842bf0)>=0)n.sprite_release(m.i32(0x842bf0));
        m.put32(0x842bf0,0xffffffffu);return;
    }
    if(m.i32(0x842bf0)>=0)return;
    m.puti(0x842bf0,n.sprite_create(0x2c00a0,4,0));
}
// 4B9630: lap banner timer 8447F4.
bool lap_4b9630(N& n){
    auto& m=n.m;const auto var=n.variant();
    if(var==2u&&m.u32(0x844774)==1u)return true;
    const auto t=m.i16(0x8447f4);
    if(t>0){
        if(t==0xb4)(void)n.sprite_create_range(0x2c00b2,6,3,0,0xf0);
        if(m.u8(0x780248)==0)m.put16(0x8447f4,std::uint16_t(m.u16(0x8447f4)-std::uint16_t(m.u32(0x780278))));
        return !n.s.missing;
    }
    if(!m.u8(0x84490c))return true;
    m.put8(0x84490c,0);m.put16(0x8447f4,0xb4);
    if(var==3u||var==4u)return n.fail(0x4b96aa);
    return true;
}
// 4BDA30 / 4BD9B0: countdown seconds.
bool countdown_4bda30(N& n){
    auto& m=n.m;
    auto t=m.i32(0x7d394c);if(t<0)t=0;
    if(n.mode()!=16u)return true;
    const auto var=n.variant();
    if(var>=3u&&var<=4u&&m.u8(0x7de784))return n.fail(0x4bda5f);
    if(m.i32(0x844908)>=m.i32(0x688b38))return true;
    const auto v=cvtt(float(t)*0.016611294820904732f+0.3400000035762787f);
    std::string buf;std::int32_t x;
    if(v<100){buf=format_int("%02d",v);x=0x12d;}else{buf=format_int("%03d",v);x=0x114;}
    return digits(n,0,x,0x59,buf);
}
}
// 4BA0E0(car): camera (79F574 +0xD4) to car (+0x14) line of sight. Near cars
// (distance <= 200) test the midpoint raised by 2; far cars test ten points
// 2.5 apart along the camera-to-car direction. 43EB60 is the course query.
bool line_of_sight_4ba0e0(N& n,std::uint32_t car,bool& visible){
    using driving::x87_float;using driving::x87_sqrt;
    auto& m=n.m;visible=false;
    const auto cam=m.u32(0x79f574)+0xd4,pos=car+0x14;
    const float cx=m.f32(cam),cy=m.f32(cam+4),cz=m.f32(cam+8),px=m.f32(pos),py=m.f32(pos+4),pz=m.f32(pos+8);
    if(!n.s.ground_43eb60)return n.fail(0x43eb60);
    const X87 dx=X87(cx)-X87(px),dy=X87(cy)-X87(py),dz=X87(cz)-X87(pz);        // 40F140
    const X87 dist=x87_sqrt(((dz*dz)+(dx*dx))+(dy*dy));
    std::uint32_t flags=0;
    if(!(dist>X87(200.0f))){                                                   // fcomip; jbe (NaN: near)
        driving::CourseProbe p{x87_float(X87(cx)+X87(px)),x87_float(X87(cy)+X87(py)),x87_float(X87(cz)+X87(pz))};   // 40EF40
        p={x87_float(X87(0.5f)*X87(p.x)),x87_float(X87(0.5f)*X87(p.y)),x87_float(X87(0.5f)*X87(p.z))};        // 40F050
        const float lifted=p.y+2.0f;
        if(!n.s.ground_43eb60(n.s.user,0x100,p,flags))return n.fail(0x43eb60);
        visible=(flags&0xf00002u)==0u||(lifted-p.y)>0.0f;
        return true;
    }
    const float Dx=x87_float(X87(px)-X87(cx)),Dy=x87_float(X87(py)-X87(cy)),Dz=x87_float(X87(pz)-X87(cz));   // 40EFA0
    const X87 L=x87_sqrt(((X87(Dx)*X87(Dx))+(X87(Dy)*X87(Dy)))+(X87(Dz)*X87(Dz)));                         // 40F080
    if(!(L>X87(0.0001)))return n.fail(0x40f0ce);          // S would keep uninitialised stack words
    for(int i=1;i<=10;++i){
        const float len=float(i)*2.5f;
        const X87 k=X87(len)/L;
        const float sx=x87_float(k*X87(Dx)),sy=x87_float(k*X87(Dy)),sz=x87_float(k*X87(Dz));
        driving::CourseProbe p{x87_float(X87(cx)+X87(sx)),x87_float(X87(cy)+X87(sy)),x87_float(X87(cz)+X87(sz))};
        const float y0=p.y;
        if(!n.s.ground_43eb60(n.s.user,0x100,p,flags))return n.fail(0x43eb60);
        if((flags&0xf00002u)==0u||(y0-p.y)>0.0f){visible=true;return true;}
    }
    return true;
}
// 4BAD20(car): rival position labels projected over the other cars.
bool navi_rival_labels_4bad20_impl(NaviPubServices& s,std::uint32_t car){
    N n(s,*s.m);auto& m=n.m;
    const auto camera=m.u32(0x79f574);
    std::int32_t divisor=0xffffff;std::int32_t mine=m.u16(car+0x260);
    const auto p80fb0c=m.u32(0x80fb0c);
    if((p80fb0c?m.i32(p80fb0c+0x18):-1)>1){
        divisor=m.i16(0x804388);if(divisor==0)return n.fail(0x4bad5c);
        mine=std::int32_t(m.u16(car+0x260))%divisor;
    }
    if(std::int32_t(n.racer_count())<=1)return true;
    // The PC keeps up to 8 indices in a stack array (more would overwrite its frame).
    std::array<std::uint32_t,8> list{};std::size_t count=0;
    for(std::uint32_t i=0;std::int32_t(i)<m.i32(0x680ad4)-1;++i){
        if((m.u8(0x79fb51+i)&3)!=2)continue;
        if(m.u8(0x79fb48+i+9)&0x10)continue;
        if(m.u32(m.u32(0x799d54+i*0x3c)+8)&0x20000000u)continue;
        if(count==list.size())return n.fail(0x4badc3);
        list[count++]=i;
    }
    if(count==0)return true;
    for(bool sorted=false;!sorted;){
        sorted=true;
        for(std::size_t k=1;k<count;++k){
            const auto a=m.u8(m.u32(0x799d54+list[k]*0x3c)+0xc36),b=m.u8(m.u32(0x799d54+list[k-1]*0x3c)+0xc36);
            if(a<b){std::swap(list[k],list[k-1]);sorted=false;}
        }
    }
    if(!s.matrices)return n.fail(0x4baeaa);
    auto& st=*s.matrices;
    for(std::size_t k=0;k<count;++k){
        const auto other=m.u32(0x799d54+list[k]*0x3c);
        const std::int32_t gap=std::int32_t(m.u8(car+0xc36))-std::int32_t(m.u8(other+0xc36));
        if(gap>3)continue;
        if(gap<0)break;
        const std::int32_t rem=std::int32_t(m.u16(other+0x260))%divisor;
        // P = (0,1,0) through the car matrix, then 449940 with the camera.
        auto carm=m.bytes(other+0xb0,64),camm=m.bytes(camera+0x140,64);
        if(n.faulted())return false;
        driving::pc_matrix_push_load(st,carm);
        auto p=driving::pc_matrix_point(st,driving::CourseProbe{0.0f,1.0f,0.0f});
        driving::pc_matrix_load(st,camm);
        const auto t=driving::pc_matrix_point(st,p);
        const float a0=m.f32(camera+0xb0),a1=m.f32(camera+0xb4);
        if(1.1920928955078125e-07f>std::fabs(t.z))p={0,0,0};                  // fcomip: exact compare
        else{const float w=1.0f/(0.0f-t.z);const float wx=w*a0;const float wy=w*a1;p={wx*t.x,t.y*wy,t.z};}
        driving::pc_matrix_pop(st);
        const float z=p.z;const std::int32_t diff=rem-mine;
        if(z>200.0f||-500.0f>z||diff>0x7f||diff<-0x1a)continue;
        float A=1.0f,px=p.x,py=p.y;
        const auto md=n.mode();
        if(md!=0xdu&&md!=0xfu){
            float x1,xv;
            if(z>0.0f){x1=1.0f-z*0.004999999888241291f;xv=0.0f-px;px=xv;py=-220.0f;}
            else{x1=1.0f-z*-0.0020000000949949026f;xv=px;}
            float x4=float(diff);x4=diff>=0?x4*0.007874015718698502f:x4*-0.03846153989434242f;
            const float x2=1.0f-x4;
            A=x1;if(!(x2>=x1))A=x2;
            if(-220.0f>py)py=-220.0f;
            if(-290.0f>xv)px=-290.0f;else if(xv>290.0f)px=290.0f;
        }
        bool visible=false;
        if(!line_of_sight_4ba0e0(n,other,visible))return false;
        if(!visible)continue;
        const std::uint8_t pos=m.u8(other+0xc36);
        std::int32_t esi=cvtt(px)+0x140;
        std::int32_t ebp=cvtt((240.0f-py)-32.0f);
        const std::uint32_t rank=std::uint8_t(pos+1);
        std::int32_t height=0x18,xoff=0;std::uint32_t suffix=3;
        const auto lang=m.u32(0x7d2698);
        if(lang){if(rank==1)suffix=0;}
        else if(rank/10!=1)suffix=m.u32(0x688d68+(rank%10)*4);
        const std::uint32_t tens=rank/10,ones=rank%10;
        const std::uint32_t kind=n.mission_type();
        if(n.selection()&&(kind==3u||kind==2u||kind==6u)){
            n.draw(0x429580,{0x2c00cb,fb(float(esi)),fb(float(ebp-12)),0,0,fb(A)});continue;}
        if(pos<3){
            n.draw(0x429580,{0x2b0047u+pos,fb(float(esi)),fb(float(ebp-12)),0,0,fb(A)});continue;}
        const std::uint32_t colour=(ftol(X87(A)*X87(224.0f))<<24)|0xffffffu;
        std::int32_t ebx;std::uint32_t tok;std::int32_t o1,o2;
        if(tens>0){
            if(!rd8s(n,0x5c6f80+ones,o2)||!rd8s(n,0x5c6f74+tens,o1)||!rd32(n,0x5c6d70+tens*4,tok))return false;
            n.draw(0x42d280,{tok,std::uint32_t(o2+(o1+esi)-0x24),std::uint32_t(ebp-0x18),0,0,colour});
            if(!rd32(n,0x5c6d70+ones*4,tok))return false;
            n.draw(0x42d280,{tok,std::uint32_t(o2+esi-0x12),std::uint32_t(ebp-0x18),0,0,colour});
            ebx=0;
        }else{
            if(!rd8s(n,0x5c6f80+ones,o2)||!rd32(n,0x5c6d70+ones*4,tok))return false;
            n.draw(0x42d280,{tok,std::uint32_t(o2+esi-0x1a),std::uint32_t(ebp-0x18),0,0,colour});
            ebx=-8;
        }
        if(lang==4u||lang==3u){height=0x1c;xoff=2;}
        if(!rd32(n,0x5c6c78+suffix*4,tok))return false;
        n.draw(0x42d280,{tok,std::uint32_t(ebx+xoff+esi),std::uint32_t(ebp-height+1),0,0,colour});
        n.draw(0x42d280,{0x2b0095,std::uint32_t(esi-12),std::uint32_t(ebp),0,0,colour});
    }
    return !n.faulted()&&!s.missing;
}
// ------------------------------------------------------------------ Heart Attack (variant 2)
namespace {
// 42CC00(x, y): the text cursor 956BB4..956BBA (recorded, as in 4BA9D0).
void text_pos_42cc00(N& n,std::int32_t x,std::int32_t y){
    n.m.put16(0x956bb8,std::uint16_t(x));n.m.put16(0x956bba,std::uint16_t(y));
    n.m.put16(0x956bb4,std::uint16_t(x));n.m.put16(0x956bb6,std::uint16_t(y));
    n.draw(0x42cc00,{std::uint32_t(x),std::uint32_t(y)});
}
// 42CCE0(fmt): monospace text at the cursor; the record keeps the format address and the
// resolved text (up to 23 bytes) in args 1..6 for the renderer.
void text_42cce0(N& n,std::uint32_t at,const char* text){
    RaceHudDraw d;d.pc=0x42cce0;d.args[0]=at;
    char b[24]{};std::strncpy(b,text,sizeof b-1);std::memcpy(&d.args[1],b,sizeof b);
    n.emit(d);
}
// 4297F0(v): 7551B4 = v (the id 428A10 stamps on its submissions).
void id_4297f0(N& n,std::uint32_t v){n.m.put32(0x7551b4,v);if(n.s.sprani)n.s.sprani->current_7551b4=std::int32_t(v);}
// 4BBC70(edi target, player, slot): the marker over a tracked car (1 unit above it, through
// 449940) ahead (+260 / +25E gap) or behind, its 842BF4..842C08 flags, 8446F0 / 8446F4 / 8447E8
// and the 8448B0 + slot * 8 phase.
bool marker_4bbc70(N& n,std::uint32_t target,std::uint32_t player,std::uint32_t slot){
    auto& m=n.m;
    static constexpr std::uint32_t Ahead[15]{0x2c00d7,0x2c00d9,0x2c00db,0x2c00dd,0x2b0001,0x2b0047,0x2b0048,0x2b0049,
                                             0x2b0001,0x2b0001,0x2b0001,0x2b0001,0x2b0001,0x2b0001,0x2b0001};
    static constexpr std::uint32_t Behind[15]{0x2c00d8,0x2c00da,0x2c00dc,0x2c00de,0x2b0001,0x2b0047,0x2b0048,0x2b0049,
                                              0x2b0001,0x2b0001,0x2b0001,0x2b0001,0x2b0001,0x2b0001,0x2b0001};
    if(slot>=15u)return n.fail(0x4bc204);                  // past the two token tables of the frame
    if(!n.s.matrices)return n.fail(0x4bbd91);
    auto& st=*n.s.matrices;
    const auto camera=m.u32(0x79f574);
    auto carm=m.bytes(target+0xb0,64),camm=m.bytes(camera+0x140,64);
    driving::pc_matrix_push_load(st,carm);                 // 409F90, 40A7D0
    auto p=driving::pc_matrix_point(st,driving::CourseProbe{0.0f,1.0f,0.0f});
    driving::pc_matrix_load(st,camm);                      // 40A170, 449940
    const auto t=driving::pc_matrix_point(st,p);
    const float a0=m.f32(camera+0xb0),a1=m.f32(camera+0xb4);
    if(1.1920928955078125e-07f>std::fabs(t.z))p={0,0,0};
    else{const float w=1.0f/(0.0f-t.z);const float wx=w*a0;const float wy=w*a1;p={wx*t.x,t.y*wy,t.z};}
    driving::pc_matrix_pop(st);                            // 40A010
    const std::int32_t bp=m.u16(target+0x260),bx=m.u16(player+0x260);
    const std::int32_t d=(m.u8(target+4)&0x60)?bp-bx:std::int32_t(m.u16(player+0x25e))-std::int32_t(m.u16(target+0x25e))-bx+bp;
    float px=p.x,py=p.y;const float pz=p.z;
    std::uint32_t f4=0,f8=0,fc=0,c00=0,c04=0,c08=0;
    std::uint32_t side=0;bool clamped=false,far=false,behind_far=false;
    if(pz>100.0f){side=2;c04=2;}
    else if(pz>0.0f){side=1;c04=1;}
    else if(-500.0f>pz){f4=1;c00=1;}
    if(d>1000){far=true;fc=1;c00=1;}
    else if(d<-30){behind_far=true;c08=2;}
    else if(d<0)c08=1;
    if(-200.0f>py){clamped=true;py=-240.0f;f8=1;}
    if(-250.0f>px){px=-290.0f;clamped=true;f8=1;}
    else if(px>250.0f){px=290.0f;clamped=true;f8=1;}
    float sc,rate=0.0f;bool reset=false;
    if(side){sc=0.4000000059604645f;px=0.0f-px;py=-240.0f;reset=true;}
    else if(!far){sc=clamped?0.4000000059604645f:1.0f;rate=float(1000-d)*0.0020000000949949026f;}
    else if(!clamped){sc=0.4000000059604645f;rate=float(1000-d)*0.0020000000949949026f;}
    else{sc=0.4000000059604645f;reset=true;}
    const std::uint32_t o=slot*8u;
    if(reset)m.putf(0x8448b0+o,rate);
    m.putf(0x8448b4+o,rate);
    m.putf(0x8447e8,sc);m.putf(0x8446f0,px);m.putf(0x8446f4,py);
    m.put32(0x842bf4,f4);m.put32(0x842bf8,f8);m.put32(0x842bfc,fc);m.put32(0x842c00,c00);m.put32(0x842c04,c04);m.put32(0x842c08,c08);
    if(behind_far)return true;
    const float X=px+320.0f;const float Y=(240.0f-py)-32.0f;
    std::uint32_t rp,rt;
    if(!n.rank_45a2b0(m.u8(player+0x10),rp)||!n.rank_45a2b0(m.u8(target+0x10),rt))return false;
    if(std::uint8_t(rp)>std::uint8_t(rt)){
        int kind=0;                                        // 0 drawn, 1 blinking (half scale), 2 hidden
        if(n.mode()!=0xdu){
            const std::int32_t f=m.i32(0x7d394c)%60;       // 44FE40
            const std::int32_t r=m.u8(target+0x10),q0=r*60/4,q1=(r+1)*60/4;
            if(q0<=f&&f<q0+3)kind=1;
            else if(q0+3<=f&&f<q1-3)kind=2;
            else if(q1-3<=f&&f<q1)kind=1;
        }
        if(kind!=2)n.draw(0x429580,{Ahead[slot],fb(X),fb(Y),0,0,fb(kind?sc*0.5f:sc)});
    }else if(d>-32){
        std::uint32_t e;float a;
        if(d>-3){sc=1.0f;e=4;a=1.0f;}
        else{sc=0.30000001192092896f;if(d>-6){e=4;a=sc;}else if(d>-12){e=1;a=sc;}else{e=0;a=0.0f;}}
        std::uint32_t rk;if(!n.rank_45a2b0(m.u8(target+0x10),rk))return false;
        if(std::uint8_t(rk)>3u)return n.fail(0x4bc2c6);      // layer table {3,2,1,0} read past its end
        n.draw(0x429580,{Behind[slot],fb(X),fb(Y),3u-std::uint8_t(rk),0,fb(sc)});
        const auto var=n.variant();
        if(var!=9u&&var!=2u&&5994.0f>float(m.i32(0x7d394c))){
            const std::int32_t r=std::int32_t(std::uint32_t(m.i32(0x7d394c))*e)%30;
            n.draw(0x429580,{0x2c00df,fb(X),fb(Y),3,std::uint32_t(0x1d-r),fb(a)});
        }
    }
    float v=m.f32(0x8448b0+o)+m.f32(0x8448b4+o);
    if(v>=30.0f)v-=30.0f;
    m.putf(0x8448b0+o,v);
    return true;
}
// 45B790: in state 0x0E, the markers of the (up to) three tracked cars 7F8B00 {car, slot - 5}
// (4BDAA0 -> 4BBC70).
bool tracked_45b790(N& n){
    if(n.u32(0x7f2428)!=0xeu)return true;
    const auto pl=n.car();
    for(std::uint32_t k=0;k<3;++k){
        const std::uint32_t e=0x7f8b00u+k*8u,target=n.u32(e);
        if(target&&!marker_4bbc70(n,target,pl,n.u32(e+4)+5u))return false;
    }
    return true;
}
// 4BB9E0(row, value): a request row 2C003D + row and its value (negative: red, with a "-").
bool request_row_4bb9e0(N& n,std::uint32_t row,std::int32_t value){
    static constexpr std::uint32_t T[5]{0x2c003d,0x2c003e,0x2c003f,0x2c0040,0x2c0041};
    if(row>=5u)return n.fail(0x4bba18);                    // token table read past the frame
    n.draw(0x429530,{T[row],0x42700000u,0,1,1});
    std::uint32_t colour=0xffffffffu;std::int32_t a=value;
    if(value<0){
        a=std::int32_t(0u-std::uint32_t(value));colour=0xffff0000u;
        n.draw(0x42ca60,{3});n.draw(0x42cc60,{F1,F1});n.draw(0x42ccb0,{1});n.draw(0x42cca0,{colour});
        text_pos_42cc00(n,a>=100?0x1fe:(a<10?0x21e:0x20e),std::int32_t(row)*0x15+0x43);
        text_42cce0(n,0x5c18c4,"-");
    }
    id_4297f0(n,colour);
    if(!navi_digits_4ba9d0_impl(n.s,0xa,0x21d,std::int32_t(row)*0x15+0x53,format_int("%3d",a),1,1.0f))return false;
    id_4297f0(n,0xffffffffu);
    return true;
}
// 4BCD50: request rows 0..level-1 (7F244C) and the total (7F2464), the 7F1C80 / 10000 gauge
// 2C006D, the 2B000E alert sprite [688B34] while [7F1CAC] == -1 (mode 16), and its value.
bool requests_4bcd50(N& n){
    auto& m=n.m;const auto pl=n.car();
    std::int32_t alert=m.i32(0x7f1cac);                    // 45B850
    const std::int32_t v=m.i32(0x7f1c80)/10000;            // 45B820 / 45B840
    const auto md=n.mode();
    std::uint32_t i=0;
    if(std::int32_t(n.level_44c940(m.u32(pl+0x68)))>0){
        do{if(!request_row_4bb9e0(n,i,m.i32(0x7f244c+i*4u)))return false;++i;}   // 45DFA0
        while(std::int32_t(i)<std::int32_t(n.level_44c940(m.u32(pl+0x68))));
    }
    if(!request_row_4bb9e0(n,i,m.i32(0x7f2464)))return false;                    // 45DF80
    if(md!=16u)alert=1;
    std::int32_t fr=v/37;if(fr>0x63)fr=0x63;
    n.draw(0x429530,{0x2c006d,0,0,0,std::uint32_t(fr)});
    const std::int32_t h=m.i32(0x688b34);
    if(alert!=-1){if(h!=-1){n.sprite_release(h);m.put32(0x688b34,0xffffffffu);}}
    else{
        bool make=h==-1;
        if(!make){
            if(!n.s.sprites)return n.fail(0x428880);
            if(n.s.sprites->status(std::uint32_t(h))==3u){n.sprite_release(h);make=true;}
        }
        if(make){
            const auto nh=n.sprite_create(0x2b000e,6,1);m.puti(0x688b34,nh);
            if(nh>=0&&n.s.sprites)n.s.sprites->set_matrix(std::uint32_t(nh),sprani_translation(166.0f,182.0f));   // D3DXMatrixTranslation, 4287B0
            n.sound(0xd4);
        }
    }
    return digits(n,0xa,0x206,0x1b2,format_int("%4d",v));
}
// 4BD900: the race clock "%2d %02d %03d" ([780258] ? 7D3994 : 7D3998 or 7D39F4 set: 449AC0 of
// the 7D393C milliseconds; else 449B30 of the 7D39DC frames).
bool clock_4bd900(N& n){
    auto& m=n.m;
    const std::uint32_t t=n.variant()?m.u32(0x7d3994):m.u32(0x7d3998);   // protected 447AE3: eax = [780258]
    std::uint16_t mi,se,ms;
    if(t||m.u32(0x7d39f4)){
        const std::uint32_t v=m.u32(0x7d393c),total=v/1000u;
        ms=std::uint16_t(v-total*1000u);
        const std::uint32_t hours=total/3600u;
        const std::uint32_t mins=total/60u-hours*0xe10u;   // 449AC0 subtracts hours * 3600 (sic)
        mi=std::uint16_t(mins);
        se=std::uint16_t(mins*0xffc4u-std::uint16_t(std::uint16_t(hours)*0xe10u)+total);
    }else{                                                 // 449B30(frames, 0.0) under the x87 precision
        X87 st=fild_u32(m.u32(0x7d39dc));
        st=(st+X87(0.0f))*X87(bf(0x3c881469u));           // fadd extra; fmul [62812C]
        const float stored=driving::x87_float(st);         // fst
        std::uint32_t eax=ftol(st);
        float frac=(stored-float(std::int32_t(eax&0xffffu)))*1000.0f;
        std::uint16_t sec=std::uint16_t(eax);
        if(0.0f>frac){--eax;frac+=1000.0f;sec=std::uint16_t(eax);}
        const std::uint32_t msl=ftol(X87(frac));
        ms=std::uint16_t(msl);
        if(ms>=1000u){ms=std::uint16_t(msl-1000u);sec=std::uint16_t(sec+1u);}
        const std::uint16_t minutes=std::uint16_t(sec/60u);
        mi=std::uint16_t(minutes%60u);se=std::uint16_t(sec%60u);
    }
    char b[64];std::snprintf(b,sizeof b,"%2d %02d %03d",int(mi),int(se),int(ms));
    return digits(n,0xa,0x46,0x49,b);
}
// 4BD0A0, variant 2 (8446E8 > 0): the level badge [5C6FE0 + (level-1) * 4], its 7F2438 value
// (negative: "-" in front) and the 2B0016 mark when [7F257C + (level-1) * 0x44] > 0.
bool level_4bd0a0(N& n){
    auto& m=n.m;
    const std::uint32_t e=n.level_44c940(m.u32(n.car()+0x68))-1u;
    std::int32_t v=m.i32(0x7f2438+e*4u);                   // 45DF90
    std::uint32_t tok;if(!rd32(n,0x5c6fe0+e*4u,tok))return false;
    n.draw(0x429530,{tok,0,0,2,1});
    std::int32_t x=0x163;
    if(v<0){
        v=std::int32_t(0u-std::uint32_t(v));
        const auto s=format_int("%d",v);
        std::int32_t tx;
        if(v>=1000){tx=0x154;x=0x173;}else tx=0x184-std::int32_t(s.size())*16;
        n.draw(0x42ca60,{3});n.draw(0x42cc60,{F1,F1});n.draw(0x42ccb0,{1});
        text_pos_42cc00(n,tx,m.i32(0x688b48)-16);
        text_42cce0(n,0x5c18c4,"-");
    }
    if(!digits(n,0xa,x,m.i32(0x688b48),format_int("%4d",v)))return false;
    if(m.i32(0x7f257c+e*0x44u)>0)n.draw(0x429530,{0x2b0016,0x42200000u,0xc1200000u,2,1});
    return true;
}
}
// ------------------------------------------------------------------ OutRun (variants 0 / 1)
namespace {
// 4BABF0(eax camera, ecx car): the car is in front of the camera and no course polygon (43EB60,
// attribute bit 0 clear) rises above ten points 0.1 apart from the eye to it (raised by 2).
bool sight_4babf0(N& n,std::uint32_t cam,std::uint32_t car,bool& visible){
    using driving::x87_float;
    auto& m=n.m;visible=false;
    if(!n.s.matrices)return n.fail(0x4bac21);
    auto& st=*n.s.matrices;
    driving::pc_matrix_push_load(st,m.bytes(cam+0x140,64));
    const auto v=driving::pc_matrix_point(st,driving::CourseProbe{m.f32(car+0x14),m.f32(car+0x18),m.f32(car+0x1c)});
    driving::pc_matrix_pop(st);
    if(v.z>0.0f)return true;
    if(!n.s.ground_43eb60)return n.fail(0x43eb60);
    const std::uint32_t eye=cam+0xd4;
    const float ex=m.f32(eye),ey=m.f32(eye+4),ez=m.f32(eye+8);
    const float dx=x87_float(X87(m.f32(car+0x14))-X87(ex)),dy=x87_float(X87(m.f32(car+0x18))-X87(ey)),dz=x87_float(X87(m.f32(car+0x1c))-X87(ez));   // 40EFA0
    std::uint32_t flags=0;
    for(int i=1;i<=10;++i){
        const float s=float(i)*0.10000000149011612f;
        const float qx=x87_float(X87(dx)*X87(s)),qy=x87_float(X87(dy)*X87(s)),qz=x87_float(X87(dz)*X87(s));   // 40F050
        const float px=x87_float(X87(qx)+X87(ex)),py=x87_float(X87(qy)+X87(ey)),pz=x87_float(X87(qz)+X87(ez));   // 40EF40
        const float raised=py+2.0f;
        driving::CourseProbe p{px,raised,pz};
        if(!n.s.ground_43eb60(n.s.user,0x100,p,flags))return n.fail(0x43eb60);
        if(flags&1u)continue;
        if(p.y>raised)return true;
    }
    visible=true;return true;
}
// 4BB640: the 2C00CB marker over each visible rival car (kind 5, no +C5C 0x800000) through the
// 449940 projection, raised by [689210] - z * -2 (at least [68920C]); layer 1 for the first,
// then 2 when farther than the first, else 0.
bool rival_markers_4bb640(N& n){
    auto& m=n.m;
    const std::uint32_t cam=m.u32(0x79f574);
    bool first=true;float near=0.0f;
    for(std::uint32_t id=9;id<0x20;++id){
        if((m.u8(0x79fb48+id)&3u)!=2u)continue;
        const std::uint32_t car=m.u32(0x799b38+id*0x3c);
        if(m.u32(car+0x324)!=5u||(m.u32(car+0xc5c)&0x800000u))continue;
        bool visible;if(!sight_4babf0(n,cam,car,visible))return false;
        if(!visible)continue;
        auto& st=*n.s.matrices;
        driving::pc_matrix_push_load(st,m.bytes(car+0xb0,64));
        auto p=driving::pc_matrix_point(st,driving::CourseProbe{0.0f,0.0f,0.0f});
        driving::pc_matrix_load(st,m.bytes(cam+0x140,64));
        const auto t=driving::pc_matrix_point(st,p);
        const float a0=m.f32(cam+0xb0),a1=m.f32(cam+0xb4);
        if(1.1920928955078125e-07f>std::fabs(t.z))p={0,0,0};
        else{const float w=1.0f/(0.0f-t.z);const float wx=w*a0;const float wy=w*a1;p={wx*t.x,t.y*wy,t.z};}
        driving::pc_matrix_pop(st);
        const float z=p.z;
        std::int32_t lift=m.i32(0x689210)-cvtt(z*-2.0f);
        if(lift<m.i32(0x68920c))lift=m.i32(0x68920c);
        const float X=p.x+320.0f;const float Y=(240.0f-p.y)-float(lift);
        std::uint32_t layer;
        if(first){first=false;near=z;layer=1;}else layer=z>near?2u:0u;
        n.draw(0x429580,{0x2c00cb,fb(X),fb(Y),layer,0,0x3f4ccccdu});
    }
    return true;
}
// 4BB300 (LAN races): over each running remote car (events 9..31, kind 5, not hidden +8 bit 0)
// within 0x32 course units of the player: through the 449940 projection (z at least -500) and
// the 4BABF0 sight test, its slot marker 5C6FA0[+1054] (alpha 0.8, fading past 400 / below 150)
// and, nearer than 150, its CommRace name (7DE427 + slot * 0x6C) in the slot colour; layer 1 for
// the first, then 2 when farther than the first, else 0.
bool lan_markers_4bb300(N& n){
    auto& m=n.m;
    const std::uint32_t cam=m.u32(0x79f574),player=m.u32(0x799d18);
    static constexpr std::uint32_t Colour[6]{0xff0000,0xefcc00,0x191919,0xffffff,0x3bc729,0x29a0c7};
    bool any=false;float near=0.0f;
    for(std::uint32_t id=9;id<0x20;++id){
        if((m.u8(0x79fb48+id)&3u)!=2u)continue;
        const std::uint32_t car=m.u32(0x799b38+id*0x3c);
        if(m.u32(car+0x324)!=5u||(m.u8(car+8)&1u))continue;
        const std::int32_t gap=std::int32_t(m.u16(player+0x260))-std::int32_t(m.u16(car+0x260));
        if(gap>0x32||gap<-0x32)continue;
        if(!n.s.matrices)return n.fail(0x4bb3a2);
        auto& st=*n.s.matrices;
        driving::pc_matrix_push_load(st,m.bytes(car+0xb0,64));
        auto p=driving::pc_matrix_point(st,driving::CourseProbe{0.0f,0.0f,0.0f});
        driving::pc_matrix_load(st,m.bytes(cam+0x140,64));
        const auto t=driving::pc_matrix_point(st,p);
        const float a0=m.f32(cam+0xb0),a1=m.f32(cam+0xb4);
        if(1.1920928955078125e-07f>std::fabs(t.z))p={0,0,0};
        else{const float w=1.0f/(0.0f-t.z);const float wx=w*a0;const float wy=w*a1;p={wx*t.x,t.y*wy,t.z};}
        driving::pc_matrix_pop(st);
        const float z=p.z;
        if(-500.0f>z)continue;                                               // 5B43F4 (comiss / ja)
        bool visible;if(!sight_4babf0(n,cam,car,visible))return false;
        if(!visible)continue;
        std::int32_t lift=m.i32(0x689208)-cvtt(z*-2.0f);
        if(lift<m.i32(0x689204))lift=m.i32(0x689204);
        const float X=p.x+320.0f,Y=(240.0f-p.y)-float(lift);
        std::uint32_t layer;
        if(!any){any=true;near=z;layer=1;}else layer=z>near?2u:0u;
        const float d=0.0f-z;
        float sprite=0.8f,text=0.0f;
        if(d>400.0f)sprite=(1.0f-(d-400.0f)*0.00999999977648258f)*0.8f;
        bool skip_sprite=false;
        if(150.0f>d){
            if(100.0f>d){text=1.0f;skip_sprite=true;}
            else{const float k=1.0f-(d-100.0f)*0.0199999995529652f;text=k;sprite=(1.0f-k)*0.8f;}
        }
        const std::uint32_t slot=m.u32(car+0x1054);
        if(!skip_sprite&&sprite>0.0f)n.draw(0x429580,{m.u32(0x5c6fa0+slot*4u),fb(X),fb(Y),layer,0,fb(sprite)});
        if(!(text>0.0f))continue;
        if(slot>=6u)return n.fail(0x4bb5c5);                                 // past the six colours of the frame
        n.draw(0x42ca60,{9});n.draw(0x42cc60,{0x3f800000u,0x3f800000u});
        n.draw(0x42cca0,{(ftol(X87(text)*X87(255.0f))<<24)|Colour[slot]});
        text_pos_42cc00(n,cvtt(X),cvtt(Y));
        n.draw(0x42c360,{4});n.draw(0x42ccb0,{0});n.draw(0x42ccb0,{layer});
        std::string name;for(std::uint32_t k=0;k<0x20u;++k){const auto ch=m.u8(0x7de427+slot*0x6cu+k);if(!ch)break;name+=char(ch);}
        text_42cdd0(n,name);
    }
    return true;
}
// 4BB890(esi car): the 2B0000 "final stretch" sprite [8448A8] between +102C 0.65 and 0.72 of the
// first leg (+68 0) / 0.7 and 0.75 of the second, presets 0, 1 and 4.
void final_stretch_4bb890(N& n,std::uint32_t car){
    auto& m=n.m;
    const std::int32_t p=m.i32(0x78024c);
    if(p<0||(p>1&&p!=4))return;
    std::int32_t h=m.i32(0x8448a8);
    const float f=m.f32(car+0x102c);
    auto window=[&](float on,float off){
        if(f>=on&&h<0){h=n.sprite_create(0x2b0000,0,0);m.puti(0x8448a8,h);}            // comiss / jb: NaN skips
        if(f>=off&&h>=0){n.sprite_release(h);h=-1;m.put32(0x8448a8,0xffffffffu);}
    };
    if(m.u32(car+0x68)==0u)window(0.6499999761581421f,0.7200000286102295f);
    if(m.u32(car+0x68)==1u)window(0.699999988079071f,0.75f);
}
// 4B9250(time block, x) with the colour: the "% 2d %02d %03d" lap time (minutes, seconds,
// milliseconds; 9'59"999 past ten minutes) in font 3 mode 2 from x + 0xDA step 0x14 (4B9200:
// 42CC00 + 42CCC0 per character), and the 2C00BC frame at (x, 30).
void lap_time_4b9250(N& n,std::uint32_t blk,std::int32_t x,std::uint32_t colour){
    auto& m=n.m;
    const std::uint16_t h=m.u16(blk);std::uint16_t mi=m.u16(blk+2),se=m.u16(blk+4),ms=m.u16(blk+6);
    n.draw(0x42ca60,{3});n.draw(0x42cc60,{F1,F1});n.draw(0x42ccb0,{2});n.draw(0x42cca0,{colour});
    id_4297f0(n,colour);
    if(h>0||mi>=10){mi=9;se=59;ms=999;}
    char b[64];std::snprintf(b,sizeof b,"% 2d %02d %03d",int(mi),int(se),int(ms));
    std::int32_t px=x+0xda;
    for(const char* c=b;*c;++c){text_pos_42cc00(n,px,0xce);n.draw(0x42ccc0,{std::uint8_t(*c)});px+=0x14;}
    n.draw(0x429530,{0x2c00bc,fb(float(x)),0x41f00000u,2,1});
    id_4297f0(n,0xffffffffu);
}
// 4B9320(eax variant, esi 844738): the split time of the last section (449AC0 of [7D387C]); in
// variant 0 with [8448A5], green and the 2C00BD flash sprite once 0xF0 - [8446E8] frames passed.
void split_4b9320(N& n,std::uint32_t var){
    auto& m=n.m;
    const bool flash=var==0u&&m.u8(0x8448a5)!=0;
    const std::uint32_t colour=flash?0xff00ff00u:0xffffffffu;
    if(std::uint32_t(0xf0-std::int32_t(m.i16(0x8446e8)))<m.u32(0x780278)&&flash){   // 43FA00
        const auto h=n.sprite_create_range(0x2c00bd,2,3,0,0xf0);
        if(h>=0&&n.s.sprites)n.s.sprites->set_matrix(std::uint32_t(h),sprani_translation(387.0f,162.0f));
        n.sound(0xaf);
    }
    const std::uint32_t v=m.u32(0x7d387c),total=v/1000u,hours=total/3600u,mins=total/60u-hours*0xe10u;   // 450580, 449AC0
    m.put16(0x844738,std::uint16_t(hours));m.put16(0x84473a,std::uint16_t(mins));
    m.put16(0x84473c,std::uint16_t(mins*0xffc4u-std::uint16_t(std::uint16_t(hours)*0xe10u)+total));
    m.put16(0x84473e,std::uint16_t(v-total*1000u));
    lap_time_4b9250(n,0x844738,0,colour);
}
}
// ------------------------------------------------------------------ OutRun score (4BD480)
namespace {
// 4B96F0(car): the running distance digit (+1F8 * 0.00915 summed in 8446C4 within the course
// segment 8446C0 = +260), mod 10; 0 outside mode 16 and when the segment changes.
std::int32_t odo_4b96f0(N& n,std::uint32_t car){
    auto& m=n.m;
    if(n.mode()!=16u)return 0;
    float x;
    if(m.u8(car+0x244)&0x9cu)x=m.f32(0x8446c4);
    else{
        x=m.f32(car+0x1f8)*bf(0x3c15e9e2u)+m.f32(0x8446c4);m.putf(0x8446c4,x);
        const std::uint32_t seg=m.u16(car+0x260);
        if(m.u32(0x8446c0)!=seg){m.put32(0x8446c0,seg);m.putf(0x8446c4,0.0f);return 0;}
    }
    return cvtt(x)%10;
}
// 4BD480(eax car): the score (the car's 8447DC word plus the 4B96F0 digit, less the points of
// the messages still queued) "%07d" at (0x1E1, 0x49) in style 0xC drifting / 0xD off road / 1,
// then each queued 8444C8 message (4B9770): its stacking order and y easing, alpha in/out,
// "%7d" points, the passed car's 2B0044 frame, the 0xE / 0xF / 0x11 bonus 2B0046 sprite and
// 4249F0(0x52); outside mode 0x12 its age +C counts down.
bool score_4bd480(N& n,std::uint32_t car){
    auto& m=n.m;
    std::uint32_t v=std::uint32_t(odo_4b96f0(n,car))+m.u32(0x8447dc+m.u32(car)*4);
    for(std::uint32_t k=0;k<10;++k){const std::uint32_t r=0x8444c8+k*0x24;if(m.i32(r+0xc)>0)v-=m.u32(r+8);}
    const std::string buf=std::int32_t(v)>9999999?std::string("9999999"):format_int("%07d",std::int32_t(v));
    std::uint32_t style;
    const std::int32_t sl=m.i16(car+0x162);
    if(m.f32(car+0xe68)>bf(0x3e3851ecu)&&sl>-1500&&sl<1500&&m.f32(car+0x1c4)>m.f32(car+0xe64))style=0xc;
    else style=(m.u8(car+0x244)&0x9cu)?0xdu:1u;
    if(!digits(n,style,0x1e1,0x49,buf))return false;
    // Port: the messages ease, fade and age once per displayed frame on the PC;
    // on a display-only frame (above 60 Hz) they hold.
    const bool ticked=enhancements::display_ticks()!=0;
    for(std::uint32_t k=0;k<10;++k){
        const std::uint32_t r=0x8444c8+k*0x24;
        const std::int32_t age=m.i32(r+0xc);
        if(age<=0)continue;
        std::int32_t order=0;
        for(std::uint32_t j=0;j<10;++j){if(j==k)continue;const std::int32_t a=m.i32(0x8444d4+j*0x24);if(a>0&&a<=age)++order;}
        m.puti(r,order);
        std::int32_t y;
        if(!ticked)y=m.i32(r+0x10);
        else if(age<10)y=cvtt(float(m.i32(r+0x10))*bf(0x3f2aaaabu));
        else{const std::int32_t t=(order+1)*0x1a;y=t-cvtt(float(m.i32(r+0x10)-t)*bf(0xbf2aaaabu));}
        m.puti(r+0x10,y);
        if(age<60&&ticked){
            if(age>=50&&n.mode()!=0x12u)m.putf(r+0x14,m.f32(r+0x14)+bf(0x3dcccccdu));
            if(age<10)m.putf(r+0x14,m.f32(r+0x14)*bf(0x3f4ccccdu));
        }
        if(!navi_digits_4ba9d0_impl(n.s,m.u32(r+0x18),0x1e0,y+0x49,format_int("%7d",m.i32(r+8)),4,m.f32(r+0x14)))return false;
        const std::int32_t left=0x8c-age;
        if(left<0x5a){
            const std::int32_t kind=m.i32(r+4);
            if(kind>0&&kind<=4){
                std::uint32_t frame;
                if(kind==4)frame=std::uint32_t(left+0x113a);
                else frame=m.u32(0x688b50+std::uint32_t(m.i32(r+0x1c)*3+m.i32(r+0x20))*4)+std::uint32_t(left);
                n.draw(0x429530,{0x2b0044,0xc1200000u,fb(float(y-0x12)+20.0f),5,frame});
            }
        }
        if(n.mode()==0x12u||!ticked)continue;
        const std::uint8_t view=m.u8(m.u32(0x79f574)+0x34a);              // 483EB0
        const std::uint32_t md=n.mode(),st=m.u32(r+0x18);
        auto bonus=[&](bool sprite){
            if(sprite&&view==2&&md==16u){
                const auto h=n.sprite_create(0x2b0046,0,3);
                if(h>=0&&n.s.sprites)n.s.sprites->set_matrix(std::uint32_t(h),sprani_translation(60.0f,50.0f));
            }
            n.draw(0x4249f0,{0x52});
        };
        if(st==0xfu){if(age==5)bonus(true);}
        else if(st==0x11u){if(age==5)bonus(true);else if(age==0xf)bonus(false);}
        else if(st==0xeu){if(age==5)bonus(true);else if(age==0xf||age==0x19)bonus(false);}
        m.puti(r+0xc,age-1);
    }
    return true;
}
}
// ------------------------------------------------------------------ OutRun / Time Attack (variants 0 / 7)
namespace {
// 449AC0(h, m, s, ms, milliseconds) into four words (minutes = total / 60 - hours * 3600, sic).
void split_449ac0(Mem& m,std::uint32_t h,std::uint32_t mi,std::uint32_t se,std::uint32_t ms,std::uint32_t v){
    const std::uint32_t total=v/1000u,hours=total/3600u,mins=total/60u-hours*0xe10u;
    m.put16(ms,std::uint16_t(v-total*1000u));m.put16(h,std::uint16_t(hours));m.put16(mi,std::uint16_t(mins));
    m.put16(se,std::uint16_t(mins*0xffc4u-std::uint16_t(std::uint16_t(hours)*0xe10u)+total));
}
// 467840(a, level, sector): the Time Attack ghost's sector time ([7F8EF0]: the loaded ghost's
// 7F91D4[sector], else the 7F8F00 table).
std::uint32_t ghost_time_467840(N& n,std::uint32_t a,std::uint32_t level,std::uint32_t sector){
    auto& m=n.m;
    if(m.u8(0x7f8ef0))return m.u32(0x7f91d4+sector*4);
    return m.u32(0x7f8f00+((std::uint32_t(std::int32_t(std::int8_t(a)))*15u+level)*4u+sector)*4u);
}
// The QHOT record of (a, b) in [813740] (stride 0xFD4, version 0x132), 0 when absent.
std::uint32_t qhot_record(N& n,std::uint32_t base_index){
    auto& m=n.m;
    const std::uint32_t t=m.u32(0x813740);
    if(!t)return 0;
    const std::uint32_t rec=base_index*0xfd4u+t;
    if(!rec||m.u32(rec)!=0x544f4851u||m.u16(rec+0x24)!=0x132u)return 0;
    return rec;
}
// 47FBD0(level, sector, slot) (protected 40E9C3: eax = [81373C]): the record sector time.
std::uint32_t record_47fbd0(N& n,std::uint32_t level,std::uint32_t sector,std::uint32_t slot){
    auto& m=n.m;
    const std::uint32_t t=m.u32(0x81373c);
    if(!t){
        if(n.variant()==7u&&slot==1u)return ghost_time_467840(n,1,level,sector);
        return 0;
    }
    const std::int32_t idx=m.i8(level*0xfd4u+t+0x2f);
    const std::uint32_t rec=qhot_record(n,slot+std::uint32_t(idx)*2u);
    return rec?m.u32(rec+0x14+sector*4):0u;
}
// 4BA2C0(eax slot, ecx level, sector, time): the slot's record time 844710 (+ split) and the
// difference to it 844780 (+ split, +C 1 when ahead).
void sector_vs_record_4ba2c0(N& n,std::uint32_t slot,std::uint32_t level,std::uint32_t sector,std::uint32_t time){
    auto& m=n.m;
    const std::uint32_t r=record_47fbd0(n,level,sector,slot),o=slot*16u;
    m.put32(0x844710+o,r);m.put8(0x84471c+o,0);
    split_449ac0(m,0x844714+o,0x844716+o,0x844718+o,0x84471a+o,r);
    if(!r){
        m.put32(0x844780+o,0);for(std::uint32_t a:{0x84478au,0x844788u,0x844786u,0x844784u})m.put16(a+o,0);m.put8(0x84478c+o,0);
        return;
    }
    std::uint32_t d;
    if(time>=r){d=time-r;m.put32(0x844780+o,d);m.put8(0x84478c+o,0);}
    else{d=r-time;m.put32(0x844780+o,d);m.put8(0x84478c+o,1);}
    split_449ac0(m,0x844784+o,0x844786+o,0x844788+o,0x84478a+o,d);
}
// 4BA280(ebx stage): the two record holders 84481C.. (47FCC0 / 47FD00 / 47FC50 name).
void record_names_4ba280(N& n,std::uint32_t stage){
    auto& m=n.m;
    for(std::uint32_t i=0;i<2;++i){
        const std::uint32_t p=0x84481d+i*0x13,rec=qhot_record(n,i+stage*2u);
        m.put8(p-1,rec?m.u8(rec+0x28):0xff);
        m.put8(p,rec?m.u8(rec+0x31):0xff);
        if(!m.u32(0x813740)||!rec){m.put8(p+1,0);continue;}
        if(!m.u8(rec+0x2b)){m.put32(p+1,0x2d2d2d2du);m.put8(p+5,0);}               // [5B4490] "----"
        else{m.put32(p+1,m.u32(rec+0x2b));m.put8(p+5,0);}
    }
}
// 450750(level): the last level of the course (14 for presets 2 / 3 (43F960), else 4).
bool last_level_450750(N& n,std::uint32_t lvl){const std::uint32_t p=n.u32(0x78024c);return (p==2u||p==3u)?lvl==0xeu:lvl==4u;}
// 451350(level): the branch taken at a level (7D39A0), first decided from the course record
// [7D3188] +2C / +30 under a selection, 48B310 or [8361B4] (451140; variant 4 not ported).
bool route_451350(N& n,std::uint32_t lvl,std::uint32_t& out){
    auto& m=n.m;
    const bool gate=n.selection()||m.u8(0x830394)!=0||m.u8(0x8361b4)!=0;
    if(gate&&m.u32(0x7d39a0+lvl*4u)==2u){
        const std::uint32_t rec=m.u32(0x7d3188);const std::int32_t a=m.i32(rec+0x2c),b=m.i32(rec+0x30);
        std::int32_t v=-1;
        if(a==-1){if(b!=-1)v=1;}else if(b==-1)v=0;
        if(v>=0&&std::int32_t(lvl)<14){m.put32(0x7d39a0+lvl*4u,std::uint32_t(v));if(n.variant()==4u)return n.fail(0x456d20);}
    }
    out=std::int32_t(lvl)<14?m.u32(0x7d39a0+lvl*4u):2u;
    return true;
}
// 4BA3A0 (control, variants 0 / 7, [7D3934] == 300): the passed sector's time against the two
// records (4BA2C0), the banner state 844894, the colour flag 8448A5 / 844896+level and the sector
// list entry 844848 + sector * 16 (time and split since the previous sector).
bool sectors_4ba3a0(N& n){
    auto& m=n.m;
    const std::uint32_t car=n.car();
    const std::uint32_t lvl=n.level_44c940(m.u32(car+0x68));
    const bool last=last_level_450750(n,lvl);
    const std::uint32_t k=m.u32(0x7d3874),slvl=m.u32(0x7d3944);                 // 450650
    bool flag=false;std::int32_t lv;
    if(last&&k==2u)lv=std::int32_t(lvl);
    else{lv=std::int32_t(lvl)-1;if(k==3u)flag=true;}
    if(last&&k==2u)flag=true;
    const std::uint32_t ebx=m.u32(0x7d3650+(slvl*4u+k)*4u);                      // 450610
    const bool ta=m.u8(0x830394)!=0,early=m.i32(0x656234)<0x3c;                   // 48B310 / 48B350
    std::uint32_t esi,time;
    if(k==3u){
        if(ta&&early&&lv<0)lv=0;
        esi=m.u32(0x7d3954+std::uint32_t(lv)*4u);                                 // 4505A0
        sector_vs_record_4ba2c0(n,0,std::uint32_t(lv),k,esi);
        sector_vs_record_4ba2c0(n,1,std::uint32_t(lv),k,esi);
        time=esi;
        if(ta&&!early){sector_vs_record_4ba2c0(n,1,std::uint32_t(lv),k,ebx);esi=ebx;}
        std::uint32_t route;if(!route_451350(n,std::uint32_t(lv),route))return false;
        m.put16(0x844894,route==1u?2u:1u);
    }else{
        if(last)esi=flag?m.u32(0x7d3954+lvl*4u):m.u32(0x7d3748+(slvl*4u+k)*4u);   // 4505A0 / 450630
        else esi=m.u32(0x7d3748+(slvl*4u+k)*4u);
        sector_vs_record_4ba2c0(n,0,lvl,k,esi);
        sector_vs_record_4ba2c0(n,1,lvl,k,last?ebx:esi);
        time=esi;
        if(ta&&!early){sector_vs_record_4ba2c0(n,1,lvl,k,ebx);esi=ebx;}
        m.put16(0x844894,last?4u:3u);
    }
    if(ta)m.put16(0x844894,2);
    m.put32(0x8446b0,esi);split_449ac0(m,0x8446b4,0x8446b6,0x8446b8,0x8446ba,esi);
    m.put32(0x8446d4,ebx);split_449ac0(m,0x8446d8,0x8446da,0x8446dc,0x8446de,ebx);
    if(flag){
        std::uint32_t s=0;
        if(k==3u){if(!route_451350(n,std::uint32_t(lv),s))return false;}
        if(ta)s=1;
        const std::uint8_t a=m.u8(0x84478c+s*16u);
        m.put8(0x844896+std::uint32_t(lv),a);m.put8(0x8448a5,a);
    }
    const std::uint32_t e=0x844848+k*16u;
    std::uint32_t d;
    if(k==0u){record_names_4ba280(n,n.car_stage_450380(m.u32(car)));d=time;}
    else d=time-m.u32(e-0x10);
    m.put32(e,time);
    split_449ac0(m,e+4,e+6,e+8,e+0xa,d);
    return !n.s.missing;
}
}
// ------------------------------------------------------------------ variants 0 / 7 display
namespace {
void image_42d280(N& n,std::uint32_t tok,std::int32_t x,std::int32_t y,std::uint32_t colour){
    n.draw(0x42d280,{tok,std::uint32_t(x),std::uint32_t(y),0,0x40a00000u,colour});
}
// 4B1F40(dst, src, n): up to n characters, those outside 5C62E0 (0x20..0x5F, a..z, ~) as '-'.
void filter_4b1f40(N& n,std::uint8_t* dst,std::uint32_t src,std::uint32_t count){
    for(std::uint32_t i=0;i<count;++i){
        const std::uint8_t c=n.m.u8(src+i);
        if(!c){dst[i]=0;return;}
        dst[i]=((c>=0x20&&c<=0x5f)||(c>=0x61&&c<=0x7a)||c==0x7e)?c:std::uint8_t('-');
    }
}
// "%2d %02d %03d"-style time text of four words (h, m, s, ms) at w, clamped as given.
std::string time_text(const char* fmt,int a,int b,int c){char t[64];std::snprintf(t,sizeof t,fmt,a,b,c);return t;}
// 4BDAC0(eax cmp 844780+i*16, ecx time block, x, y-key f, R 844710+i*16, 30): the record row
// (ghost / record labels 2C0049 / 2C00D4 / 2C011A and 2C009E under 48B310), the frames 2C0033,
// the record time R+4.. and the time block, the difference C+4.. in green / red / white (2C00C1).
bool record_row_4bdac0(N& n,std::uint32_t cmp,std::uint32_t blk,std::int32_t x,float f,std::uint32_t rec){
    auto& m=n.m;
    const std::int32_t edi=30;
    if(m.u8(0x830394)){
        const std::int32_t x0=cvtt(f+44.0f);                                                   // [5B0270]
        image_42d280(n,0x2c0049,x0,edi+0xd0,0xffffffffu);image_42d280(n,0x2c00d4,x0,edi+0xe4,0xffffffffu);
        image_42d280(n,0x2c011a,x0,edi+0xf8,0xffffffffu);
        if(m.i32(0x656234)<0x3c&&m.u8(0x7f8ef0))image_42d280(n,0x2c009e,cvtt(f+100.0f),edi+0x94,0xffffffffu);
    }
    n.draw(0x429530,{0x2c0033,fb(f),fb(30.0f),5,1});
    n.draw(0x429530,{0x2c0033,fb(f),fb(50.0f),5,1});
    auto clamp=[&](std::uint32_t w,std::uint16_t& a,std::uint16_t& b,std::uint16_t& c){
        const std::uint16_t h=m.u16(w+4);a=m.u16(w+6);b=m.u16(w+8);c=m.u16(w+0xa);
        if(h>1){a=0x3b;b=0x3b;c=0x3e7;}};
    std::uint16_t a,b,c;
    clamp(rec,a,b,c);
    if(!navi_digits_4ba9d0_impl(n.s,9,x-10,edi+0xa8,time_text("%2d %02d %03d",a,b,c),9,1.0f))return false;
    clamp(blk,a,b,c);
    if(!navi_digits_4ba9d0_impl(n.s,9,x-10,edi+0xbc,time_text("%2d %02d %03d",a,b,c),9,1.0f))return false;
    std::uint32_t colour=m.u8(cmp+0xc)?0xff00ff00u:0xffff0000u;
    if(!m.u16(cmp+6)&&!m.u16(cmp+8)&&!m.u16(cmp+0xa))colour=0xffffffffu;
    id_4297f0(n,colour);
    n.draw(0x429530,{0x2c00c1,fb(float(x-0xf)),fb(float(edi+0xd0)),9,std::uint32_t(std::int32_t(std::int8_t(m.u8(cmp+0xc)))+10)});
    n.draw(0x429530,{0x2c0033,fb(f),fb(70.0f),5,1});                       // 30 + [5B4354]
    {std::uint16_t h=m.u16(cmp+4);a=m.u16(cmp+6);b=m.u16(cmp+8);c=m.u16(cmp+0xa);
     if(h>0||a>=10){a=9;b=0x3b;c=0x3e7;}}
    if(!navi_digits_4ba9d0_impl(n.s,9,x,edi+0xd0,time_text("%d %02d %03d",a,b,c),9,1.0f))return false;
    id_4297f0(n,0xffffffffu);
    return true;
}
// 4BDDB0(eax i, edi token, flag, time block): one sector banner (protected 1039DA0: edx =
// [844710 + i * 16], nothing without a record): 4BDAC0, the ghost / record holder rank badge
// and its 4-letter name (4B1F40 through 5C62E0, 4B9200 in font 7 mode 5).
bool banner_4bddb0(N& n,std::uint32_t i,std::uint32_t token,std::uint32_t flag,std::uint32_t blk){
    auto& m=n.m;
    if(!m.u32(0x844710+i*16u))return true;
    const std::uint32_t rec=0x844710+i*16u;
    float f0,f1;std::int32_t ebp,x;
    if(i==0){f0=70.0f;f1=4.0f;ebp=0x66;x=0x30;}
    else{f0=456.0f;f1=403.0f;ebp=0x1eb;x=0x1bf;if(flag){f0=477.0f;ebp=0x200;}}
    m.putf(0x688dd0,189.0f);m.putf(0x688dcc,196.0f);m.putf(0x84491c,30.0f);m.putf(0x844920,30.0f);
    if(!record_row_4bdac0(n,0x844780+i*16u,blk,x,f1,rec))return false;
    std::int8_t badge,frame;float bx;
    if(n.variant()==7u){
        // 466D60(1, &rank, &frame, 0): the loaded ghost's rank / frame
        std::uint8_t b3=0,b2=0;bool have=false;
        const std::uint32_t base=m.u32(0x7f9224);
        if(m.u32(base+0x9cac)==0x544f484eu){
            have=true;
            if(m.u8(0x7f8ef0)){b3=m.u8(0x7f91d0);b2=m.u8(0x7f9204);}
            else{b3=m.u8(base+0x9cac+0x11b);b2=m.u8(base+0x9cac+0x11c);}
        }
        bx=f0-10.0f;
        if(!have){badge=0;frame=0;}
        else{badge=std::int8_t(b3);frame=std::int8_t(b2-1);}
    }else{
        n.draw(0x429530,{token,i?m.u32(0x844918):m.u32(0x844914),i?m.u32(0x844920):m.u32(0x84491c),4,0});
        bx=f0;
        badge=std::int8_t(m.u8(0x84481c+i*0x13));frame=std::int8_t(m.u8(0x84481d+i*0x13)-1);
    }
    if(badge>=0&&frame>=0){
        std::uint32_t tok;if(!rd32(n,0x5c6cf8+std::uint32_t(std::int32_t(badge))*4u,tok))return false;
        n.draw(0x429530,{tok,fb(bx+48.0f),fb(m.f32(0x688dcc)+30.0f),5,std::uint32_t(std::int32_t(frame))});
    }
    n.draw(0x42ca60,{7});n.draw(0x42cc60,{F1,F1});n.draw(0x42ccb0,{5});
    std::uint8_t buf[12]{};std::int32_t y;
    if(n.variant()==7u){
        const std::uint32_t name=m.u8(0x7f8ef0)?0x7f9208u:(m.u32(0x7f9224)&&m.u32(m.u32(0x7f9224)+0x9cac)==0x544f484eu?m.u32(0x7f9224)+0x9cac+0x104:0u);
        if(!name)return true;
        filter_4b1f40(n,buf+4,name,4);                       // the buffer at L+8 (L+4 = buf)
        y=cvtt(m.f32(0x688dd0)+24.0f);ebp+=0x20;
    }else{
        filter_4b1f40(n,buf+4,0x84481eu+i*0x13,4);           // into L+8 as well
        y=cvtt(m.f32(0x688dd0));
    }
    buf[8]=0;
    for(std::uint32_t k=4;k<8&&buf[k];++k){text_pos_42cc00(n,ebp+std::int32_t(k-4)*0xf,y);n.draw(0x42ccc0,{buf[k]});}
    return true;
}
// 4BE020: the sector banners while [7D3934] > 0 (844894 bit 2: record 2C0032 and, with
// [8448A6], ghost 2C0036; else bit 0 record 2C0034, bit 1 ghost 2C0035).
bool banners_4be020(N& n){
    auto& m=n.m;
    if(m.i32(0x7d3934)<=0)return true;                                                // 450670
    std::uint8_t f=m.u8(0x844894);
    if(f&4u){
        if(!banner_4bddb0(n,0,0x2c0032,0,0x8446b0))return false;
        if(m.u8(0x8448a6)&&!banner_4bddb0(n,1,0x2c0036,1,0x8446d4))return false;
        return true;
    }
    if(f&1u){if(!banner_4bddb0(n,0,0x2c0034,0,0x8446b0))return false;f=m.u8(0x844894);}
    if((f&2u)&&!banner_4bddb0(n,1,0x2c0035,0,0x8446b0))return false;
    return true;
}
// 4BE150: the passed sectors split list (844848 + k * 16, k <= [7D3874]): frame
// 2C004A / 2C0047 / 2C0048 / 2C0049 (428980) and 2C004B, "%02d %03d" seconds / ms.
bool sector_list_4be150(N& n){
    auto& m=n.m;
    static constexpr std::uint32_t T[4]{0x2c004a,0x2c0047,0x2c0048,0x2c0049};
    const std::int32_t last=m.i32(0x7d3874);                                           // 450650
    float y=0.0f;std::int32_t py=0x14c;
    for(std::int32_t k=0;k<=last;++k){
        const std::uint32_t e=0x844848+std::uint32_t(k)*16u;
        if(m.u32(e)){
            if(k>=4)return n.fail(0x4be1b7);                                          // token table read past its end
            n.draw(0x428980,{T[k],5,0});
            n.draw(0x429530,{0x2c004b,0,fb(y),5,1});
            const std::uint16_t h=m.u16(e+4),mi=m.u16(e+6);std::uint16_t se=m.u16(e+8),ms=m.u16(e+0xa);
            if(h>0||mi>0){se=0x3b;ms=0x3e7;}
            char t[32];std::snprintf(t,sizeof t,"%02d %03d",int(se),int(ms));
            if(!navi_digits_4ba9d0_impl(n.s,9,0x1d7,py,t,9,1.0f))return false;
        }
        y+=17.0f;py+=0x11;
    }
    return true;
}
// 4BBAF0(eax car, ebx 8448AC): the 2C0023 marker over the car 47F120 names (+C5C 0x2000),
// fading by 0.05 to its +B68 alpha while visible (4BA0E0) and between z 10 and -700.
bool target_marker_4bbaf0(N& n,std::uint32_t t){
    auto& m=n.m;
    if(!t||!(m.u32(t+0xc5c)&0x2000u))return true;
    float alpha=m.f32(t+0xb68);
    bool visible;if(!line_of_sight_4ba0e0(n,t,visible))return false;
    if(!visible)alpha=0.0f;
    if(!n.s.matrices)return n.fail(0x4bbb51);
    auto& st=*n.s.matrices;const std::uint32_t cam=m.u32(0x79f574);
    driving::pc_matrix_push_load(st,m.bytes(t+0xb0,64));
    auto p=driving::pc_matrix_point(st,driving::CourseProbe{0.0f,0.0f,0.0f});
    driving::pc_matrix_load(st,m.bytes(cam+0x140,64));
    const auto q=driving::pc_matrix_point(st,p);
    const float a0=m.f32(cam+0xb0),a1=m.f32(cam+0xb4);
    if(1.1920928955078125e-07f>std::fabs(q.z))p={0,0,0};
    else{const float w=1.0f/(0.0f-q.z);const float wx=w*a0;const float wy=w*a1;p={wx*q.x,q.y*wy,q.z};}
    driving::pc_matrix_pop(st);
    float target;
    if(p.z>10.0f)target=0.0f;
    else{if(-700.0f>p.z)alpha=0.0f;target=alpha;}
    float py=p.y;
    if(p.z>0.0f)py=-240.0f;
    else if(-240.0f>py)py=-240.0f;
    float a=m.f32(0x8448ac);
    if(a>target){a-=0.05000000074505806f;if(target>a)a=target;}
    else{a+=0.05000000074505806f;if(a>target)a=target;}
    m.putf(0x8448ac,a);
    n.draw(0x429580,{0x2c0023,fb(p.x+320.0f),fb((240.0f-py)-32.0f),0,0,fb(a)});
    return true;
}
// 4BE4B0(car): sector banners, split list, the 47F120 target marker, the time attack / class
// labels (2C00A9..2C00AD), the stage split list (up to four rows from 5C7080: label 2C0042,
// stage digits 5C7058, the cleared mark 844895+stage, "%1d %02d %03d" 4505A0 split), the
// ghost row (467840 under 48B310 / 48B350) or the next stage label, and the running time.
bool stages_4be4b0(N& n,std::uint32_t car){
    auto& m=n.m;
    if(!banners_4be020(n)||!sector_list_4be150(n))return false;
    if(m.u8(0x8448a6)){
        const std::uint32_t t=m.u8(0x81044c)==1u?m.u32(0x810438):0u;                // 47F120
        if(!target_marker_4bbaf0(n,t))return false;
    }
    const bool ta=m.u8(0x830394)!=0;
    if(ta)n.draw(0x429530,{m.u8(car+0x13)==1?0x2c00aau:0x2c00a9u,0xc2dc0000u,0x40000000u,5,1});
    else{
        const std::int8_t v=std::int8_t(m.u8(0x83036d));                               // 48B1A0
        n.draw(0x428980,{v==0?0x2c00adu:v==1?0x2c00abu:0x2c00acu,5,0});
        n.draw(0x428980,{m.u8(car+0x13)==1?0x2c00aau:0x2c00a9u,5,0});
    }
    std::uint32_t rx[5],ry[5];
    for(std::uint32_t r=0;r<5;++r){if(!rd32(n,0x5c7080+r*8,rx[r])||!rd32(n,0x5c7084+r*8,ry[r]))return false;}
    auto stage_label=[&](std::uint32_t row,std::int32_t number)->bool{
        if(row>=5)return n.fail(0x4be5b8);
        image_42d280(n,0x2c0042,std::int32_t(rx[row]),std::int32_t(ry[row]),0xffffffffu);
        const std::int32_t tens=number/10,ones=number%10;std::uint32_t tok;
        if(tens>0){if(!rd32(n,0x5c7058+std::uint32_t(tens)*4,tok))return false;image_42d280(n,tok,std::int32_t(rx[row])+m.i32(0x689220),std::int32_t(ry[row]),0xffffffffu);}
        if(!rd32(n,0x5c7058+std::uint32_t(ones)*4,tok))return false;
        image_42d280(n,tok,std::int32_t(rx[row])+m.i32(0x68921c)+m.i32(0x689220),std::int32_t(ry[row]),0xffffffffu);
        return true;
    };
    auto split=[&](std::uint32_t v,std::uint16_t& a,std::uint16_t& b,std::uint16_t& c){
        const std::uint32_t total=v/1000u,hours=total/3600u,mins=total/60u-hours*0xe10u;
        const std::uint16_t h=std::uint16_t(hours);a=std::uint16_t(mins);
        b=std::uint16_t(mins*0xffc4u-std::uint16_t(h*0xe10u)+total);c=std::uint16_t(v-total*1000u);
        if(h>0||a>=10){a=9;b=0x3b;c=0x3e7;}
    };
    const std::uint32_t lvl=n.level_44c940(m.u32(car+0x68));
    std::int32_t first=std::int32_t(lvl)-4;if(first<0)first=0;
    std::int32_t s=first;std::int32_t py=0x53;
    if(s<std::int32_t(n.level_44c940(m.u32(car+0x68)))){
        do{
            const std::uint32_t row=std::uint32_t(s-first);
            if(!stage_label(row,s+1))return false;
            if(m.u8(0x844895+std::uint32_t(s+1))){
                id_4297f0(n,0xff00ff00u);
                image_42d280(n,0x2c0059,std::int32_t(rx[row])+0x42,std::int32_t(ry[row])-6,0xff00ff00u);
                image_42d280(n,0x2c0010,std::int32_t(rx[row])+0x74,std::int32_t(ry[row])-8,0xff00ff00u);
            }else{
                image_42d280(n,0x2c0059,std::int32_t(rx[row])+0x42,std::int32_t(ry[row])-6,0xffffffffu);
                image_42d280(n,0x2c0010,std::int32_t(rx[row])+0x74,std::int32_t(ry[row])-8,0xffffffffu);
            }
            std::uint16_t a,b,c;split(m.u32(0x7d3954+std::uint32_t(s)*4u),a,b,c);   // 4505A0
            if(!navi_digits_4ba9d0_impl(n.s,0xa,0x1cc,py,time_text("%1d %02d %03d",a,b,c),9,1.0f))return false;
            id_4297f0(n,0xffffffffu);
            py+=0x15;++s;
        }while(s<std::int32_t(n.level_44c940(m.u32(car+0x68))));
    }
    const std::uint32_t row=std::uint32_t(s-first);
    bool skip_label=false;
    if(ta&&m.i32(0x656234)<0x3c){
        const std::uint32_t t=ghost_time_467840(n,1,0,3);
        if(t){
            image_42d280(n,0x2c0049,0x193,0x5f,0xffffffffu);image_42d280(n,0x2c0059,0x1cd,0x5d,0xffffffffu);image_42d280(n,0x2c0010,0x1ff,0x5b,0xffffffffu);
            std::uint16_t a,b,c;split(t,a,b,c);
            if(!navi_digits_4ba9d0_impl(n.s,0xa,0x1cc,0x68,time_text("%1d %02d %03d",a,b,c),9,1.0f))return false;
        }
        skip_label=true;
    }
    if(!skip_label&&!stage_label(row,s+1))return false;
    if(row>=5)return n.fail(0x4be958);
    image_42d280(n,0x2c0059,std::int32_t(rx[row])+0x42,std::int32_t(ry[row])-6,0xffffffffu);
    image_42d280(n,0x2c0010,std::int32_t(rx[row])+0x74,std::int32_t(ry[row])-8,0xffffffffu);
    {   // 449B30([7D3930] frames, 0.0) under the x87 precision
        X87 st=fild_u32(m.u32(0x7d3930));
        st=(st+X87(0.0f))*X87(bf(0x3c881469u));
        const float stored=driving::x87_float(st);
        std::uint32_t eax=ftol(st);
        float frac=(stored-float(std::int32_t(eax&0xffffu)))*1000.0f;
        std::uint16_t sec=std::uint16_t(eax);
        if(0.0f>frac){--eax;frac+=1000.0f;sec=std::uint16_t(eax);}
        const std::uint32_t msl=ftol(X87(frac));std::uint16_t ms=std::uint16_t(msl);
        if(ms>=1000u){ms=std::uint16_t(msl-1000u);sec=std::uint16_t(sec+1u);}
        const std::uint16_t minutes=std::uint16_t(sec/60u);
        const std::uint16_t h=std::uint16_t(minutes/60u);std::uint16_t mi=std::uint16_t(minutes%60u),se=std::uint16_t(sec%60u);
        if(h>0||mi>=10){mi=9;se=0x3b;ms=0x3e7;}
        if(!navi_digits_4ba9d0_impl(n.s,0xa,0x1cc,std::int32_t(row)*0x15+0x53,time_text("%1d %02d %03d",mi,se,ms),9,1.0f))return false;
    }
    return true;
}
// 4BB970(esi car): the 2B005A goal-ahead sprite [842A98] once +64 passes the stage
// 44C990 length - 0x23, released when +5C is set.
void goal_ahead_4bb970(N& n,std::uint32_t car){
    auto& m=n.m;
    std::int32_t h=m.i32(0x842a98);
    if(h<0){
        if(m.u32(car+0x5c))return;
        const std::uint32_t key=n.unique_44dc50(m.u32(car+0x68));
        const std::uint16_t len=m.u16(m.u32(0x6a54e0+key*4)+0x7e);                    // 44C990
        if(std::int32_t(m.i16(car+0x64))>std::int32_t(len)-0x23){h=n.sprite_create(0x2b005a,0,0);m.puti(0x842a98,h);}
        else h=m.i32(0x842a98);
    }
    if(m.u32(car+0x5c)&&h>=0){n.sprite_release(h);m.put32(0x842a98,0xffffffffu);}
}
}
namespace {
// 4BA7B0 (variant 0): the ghost announcement 2C0025..2C0029 / 2C0024 (stage - 10, or 5 on the
// long courses 43F960; 428320 layer 4 mode 3, sound DB) once ([8448A6]) while the recorder holds
// a ghost (47EE60: [810124] != 0).
void ghost_4ba7b0(N& n){
    auto& m=n.m;
    std::int32_t st=std::int32_t(n.car_stage_450380(m.u32(n.car())));
    if(m.u8(0x8448a6))return;
    const std::uint32_t p=m.u32(0x78024c);
    if(p==2u||p==3u)st=5;
    else{st-=10;if(st<0||st>=6)return;}
    if(!m.u32(0x810124))return;
    static constexpr std::uint32_t T[6]{0x2c0025,0x2c0026,0x2c0027,0x2c0028,0x2c0029,0x2c0024};
    n.sprite_create(T[st],4,3);n.sound(0xdb);m.put8(0x8448a6,1);
}
}
bool mission_4bed13(N& n,NaviPubServices& s,std::uint32_t car);
bool navi_pub_display_4beb00_impl(NaviPubServices& s){
    if(!s.m)return false;
    N n(s,*s.m);auto& m=n.m;
    if(m.u32(0x8447f8)==0)return true;
    const auto md=n.mode();const auto car=n.car();
    auto tail=[&](bool release){
        if(release&&m.i32(0x844700)>=0){n.sprite_release(m.i32(0x844700));m.put32(0x844700,0xffffffffu);}
        m.put32(0x842c10,1);
        if(!s.heart||!s.heart(s.heart_user,0x46ed20u))return n.fail(0x46ed20);   // 46ED20 pair markers (state 9)
        return !n.faulted()&&!s.missing;};
    if(md==0xdu){m.put32(0x842c10,0);if(!s.heart||!s.heart(s.heart_user,0x46ed20u))return n.fail(0x46ed20);return !s.missing;}
    const auto var=n.variant();
    if(var!=6u&&var!=9u&&var!=2u&&var!=1u&&var!=0u&&var!=7u&&var!=3u&&var!=4u)return n.fail(var<=9u?0x4beb3f:0x4bedb8);
    if(var==1u&&m.u32(0x8446ec)){n.draw(0x429530,{0x2c002f,0,0,3,0});if(!score_4bd480(n,car))return false;}   // 4BEB46
    if(md!=16u)return tail(true);                                               // 4BEDF0 protected test: mode != 16
    if(var==0u||var==7u){                                                       // 4BEBF0
        n.draw(0x429530,{0x2c0031,0,0,3,0});
        if(!stages_4be4b0(n,car)||!clock_4bd900(n))return false;
        if(m.u8(0x830394)&&m.i32(0x656234)<0x3c)goal_ahead_4bb970(n,car);
    }else if(var==1u){                                                          // 4BEB74 OutRun
        n.draw(0x429530,{0x2c002e,0,0,3,0});
        if(!clock_4bd900(n))return false;
        if(m.u32(0x80fb14)){if(!navi_rival_labels_4bad20_impl(s,car))return false;}
        else if(!rival_markers_4bb640(n))return false;
        final_stretch_4bb890(n,car);
    }else if(var==2u){                                                          // 4BEBB9 Heart Attack
        if(!tracked_45b790(n))return false;
        n.draw(0x429530,{0x2c0030,0,0,1,0});
        if(!requests_4bcd50(n))return false;
        if(!s.heart||!s.heart(s.heart_user,0x462cd0u))return n.fail(0x462cd0);
        if(!clock_4bd900(n))return false;
    }else if(var==3u||var==4u){                                                 // 4BEC3D LAN races
        // 456D50: [7DE784] (CommRace: a player has finished) hides the 2C004C panel
        if(!(m.r.mapped(0x7de784,1)&&m.u8(0x7de784)))n.draw(0x429530,{0x2c004c,0,0,3,0});
        if(n.selection()&&n.mission_type()==0u&&m.u32(0x8446d0)){if(!position_4b9d80(n,car))return false;}   // 4957F0 / 495B20 / 8446D0
        if(!lan_markers_4bb300(n))return false;
        ranks_4b99e0(n,car);
        if(!messages_456540(n))return false;
    }else if(!mission_4bed13(n,s,car))return false;
    if(!speed_4bcf10(n,car)||!needle_4b8f30(n,car)||!gear_4b9010(n,car)||!route_4b9100(n)||!stage_banner_4b9450(n))return false;
    warning_4ba070(n,car);
    if(m.i16(0x8446e8)>0){                                                      // 4BD0A0
        if(var==2u){if(!level_4bd0a0(n))return false;}
        else if(var==1u)split_4b9320(n,1);
        else if(var==0u){
            if(std::uint32_t(std::int32_t(m.i16(0x8446e8))-0xd2)<m.u32(0x780278))ghost_4ba7b0(n);   // 43FA00
            split_4b9320(n,0);
        }
    }
    if(!lap_4b9630(n)||!countdown_4bda30(n))return false;
    return tail(false);
}
// 4BED13 (variants 6 / 9): mission panel, progress, scores and rival labels.
bool mission_4bed13(N& n,NaviPubServices& s,std::uint32_t car){
    const auto t=n.mission_type();
    n.draw(0x429530,{0x2c004c,0,0,3,0});
    if(t==3u||t==2u||t==6u)n.draw(0x429530,{0x2c002f,0,0,3,0});
    if(!progress_4b9b40(n,car))return false;
    if(n.selection()){
        if(t==3u||t==2u){if(!scores_4bd230(n,car))return false;}
        else if(t==6u){if(!scores_4bd340(n))return false;}
        else if(t==1u||t==0u){if(!position_4b9d80(n,car))return false;}
    }
    return navi_rival_labels_4bad20_impl(s,car);
}
}
namespace outrun::platform {
namespace {
template<class F> bool guarded(NaviPubServices& s,F&& f){
    try{return f();}
    catch(const PcRaceUnmapped& e){if(!s.missing)s.missing=0xfa000000u;s.fault=e.address;return false;}
}
}
bool navi_pub_init_4bc9e0(NaviPubServices& s){return s.m&&guarded(s,[&]{return navi_pub_init_4bc9e0_impl(s);});}
bool navi_pub_control_4bcc40(NaviPubServices& s){return s.m&&guarded(s,[&]{return navi_pub_control_4bcc40_impl(s);});}
bool navi_pub_display_4beb00(NaviPubServices& s){return s.m&&guarded(s,[&]{return navi_pub_display_4beb00_impl(s);});}
bool navi_pub_destroy_4b8df0(NaviPubServices& s){return s.m&&guarded(s,[&]{return navi_pub_destroy_4b8df0_impl(s);});}
bool navi_digits_4ba9d0(NaviPubServices& s,std::uint32_t style,std::int32_t x,std::int32_t y,const std::string& text,std::uint32_t layer,float alpha){
    return s.m&&guarded(s,[&]{return navi_digits_4ba9d0_impl(s,style,x,y,text,layer,alpha);});}
bool navi_sector_banners_4bea50(NaviPubServices& s){
    return s.m&&guarded(s,[&]{N n(s,*s.m);return banners_4be020(n)&&sector_list_4be150(n);});
}
bool navi_rival_labels_4bad20(NaviPubServices& s,std::uint32_t car){return s.m&&guarded(s,[&]{return navi_rival_labels_4bad20_impl(s,car);});}
// 4B04D0 (variant 9 display, attack running): for each rank 0..2 the first car of the
// event slots 9..23 at that rank gets the rank marker 4BDAA0(player, car, rank) = 4BBC70(slot rank + 5).
bool navi_rank_markers_4b04d0(NaviPubServices& s){
    return s.m&&guarded(s,[&]{
        N n(s,*s.m);const auto pl=n.car();
        for(std::uint32_t rank=0;rank<3u;++rank)
            for(std::uint32_t slot=9;slot<24u;++slot){
                const auto car=n.u32(0x799b38u+slot*0x3cu);
                if(n.m.u8(car+0xc36)!=rank)continue;
                if(!marker_4bbc70(n,car,pl,rank+5u))return false;
                break;
            }
        return true;});
}
NaviPubGlobals::NaviPubGlobals(){std::memcpy(g688b00.data(),Exe688b00,sizeof(Exe688b00));}
void NaviPubGlobals::map(PcRaceMemory& m){
    m.map(0x842800,g842800.data(),g842800.size());m.map(0x688b00,g688b00.data(),g688b00.size());
    m.map(0x7f1900,g7f1900.data(),g7f1900.size());m.map(0x7f8980,g7f8a00.data(),g7f8a00.size());
    m.map(0x7f9400,g7f9400.data(),g7f9400.size());
}
bool navi_sprite3d_4295d0(NaviPubServices& s,std::uint32_t token,const std::array<float,3>& pos,std::int32_t mode,
                          std::int32_t frame,float scale,float zbias,float angle,std::uint32_t depth,float minimum){
    return s.m&&guarded(s,[&]{
        N n(s,*s.m);auto& m=n.m;
        m.putf(0x780068,zbias);
        if(!s.matrices)return n.fail(0x4295f6);
        auto& st=*s.matrices;
        const auto cam=m.u32(0x79f574);
        auto camm=m.bytes(cam+0x140,64);
        driving::pc_matrix_push_load(st,camm);                                    // copy of the camera matrix, 449940
        const auto t=driving::pc_matrix_point(st,driving::CourseProbe{pos[0],pos[1],pos[2]});
        driving::pc_matrix_pop(st);
        const float a0=m.f32(cam+0xb0),a1=m.f32(cam+0xb4);
        float px=0.0f,py=0.0f,pz=0.0f;
        if(!(1.1920928955078125e-07f>std::fabs(t.z))){const float w=1.0f/(0.0f-t.z);const float wx=w*a0;const float wy=w*a1;px=wx*t.x;py=t.y*wy;pz=t.z;}
        const float nz=-pz;                                                        // fchs
        if(nz<m.f32(cam+0xbc))return true;                                        // behind the near plane
        float sc=0.0f;
        if(!(std::fabs(pz)<1.1920928955078125e-07f))sc=driving::x87_float(X87(scale)*X87(a1)/X87(nz)*X87(0.03125f));
        const X87 lim=X87(minimum)*X87(20.0f)*X87(0.03125f);
        if(X87(sc)<lim)sc=driving::x87_float(lim);
        float z=0.0f;
        if(mode<0){                                                                // D3DXMatrixMultiply, D3DXVec3TransformCoord
            driving::PcMatrix16 a{},b{};for(unsigned k=0;k<16;++k){a[k]=m.f32(0x95d8a0+k*4);b[k]=m.f32(0x780030+k*4);}
            z=driving::pc_d3dx_vec3_transform_coord({px,py,pz},driving::pc_d3dx_matrix_multiply(a,b))[2];
        }
        std::array<float,16> T{};T[0]=T[5]=T[10]=T[15]=1.0f;T[12]=px;T[13]=-py;T[14]=z;   // D3DXMatrixTranslation(x, fchs y, z)
        if(!s.sprites||!s.sprani||!s.scene_draws||!s.blend_9564e0)return n.fail(0x428af0);
        if(sprani_draw_428af0(*s.sprites,*s.sprani,st,token,T,frame,sc,angle,depth,*s.scene_draws,*s.blend_9564e0)&&s.draws){
            RaceHudDraw d;d.pc=0x428af0;d.args[0]=token;s.draws->push_back(d);}   // the renderer pairs it with its scene draw
        return !s.missing;});
}
bool navi_sprite_428ac0(NaviPubServices& s,std::uint32_t token,const std::array<float,16>& matrix,std::int32_t frame,
                        float sc,float angle,std::uint32_t mode){
    if(!s.sprites||!s.sprani||!s.scene_draws||!s.blend_9564e0||!s.matrices){if(!s.missing)s.missing=0x428ac0;return false;}
    if(sprani_draw_428af0(*s.sprites,*s.sprani,*s.matrices,token,matrix,frame,sc,angle,mode,*s.scene_draws,*s.blend_9564e0)&&s.draws){
        RaceHudDraw d;d.pc=0x428af0;d.args[0]=token;s.draws->push_back(d);}
    return true;
}
void navi_emit_draw(NaviPubServices& s,const RaceHudDraw& d){if(s.m){N n(s,*s.m);n.emit(d);}}
bool navi_quest_object_45c470(NaviPubServices& s,std::uint32_t stage,std::uint32_t& path){
    if(!s.m){s.missing=0x45c470;return false;}
    N n(s,*s.m);
    try{path=quest_object_45c470(n,stage);return true;}
    catch(const PcRaceUnmapped& u){s.fault=u.address;return n.fail(0x45c470);}
}
bool navi_service(NaviPubServices& s,std::uint32_t pc,const std::uint32_t* a,std::size_t count,std::uint32_t& eax){
    if(!s.m){s.missing=pc;return false;}
    N n(s,*s.m);
    auto need=[&](std::size_t k){if(count<k){s.missing=pc;return false;}return true;};
    try{
        switch(pc){
        case 0x45c470:if(!need(1))return false;eax=quest_object_45c470(n,a[0]);return true;
        case 0x45c7e0:if(!need(1))return false;eax=quest_kind_45c7e0(n,a[0]);return true;
        case 0x45dfb0:case 0x45e000:{   // the quest music / voice of a stage by its kind
            if(!need(1))return false;
            static constexpr std::uint32_t Dfb0[4]{0x8443,0x844f,0x8458,0x845d},E000[4]{0x841a,0x8426,0x842f,0x8434};
            eax=a[0]+(pc==0x45dfb0?Dfb0:E000)[quest_kind_45c7e0(n,a[0])];return true;}
        case 0x47fbd0:if(!need(3))return false;eax=record_47fbd0(n,a[0],a[1],a[2]);return true;
        default:s.missing=pc;return false;
        }
    }catch(const PcRaceUnmapped& u){s.fault=u.address;return n.fail(pc);}
}
}
