#include "platform/race_traffic.hpp"
#include "platform/pc_screen.hpp"
#include "driving/pc_matrix_stack.hpp"
#include "driving/pc_d3dx.hpp"
#include "driving/pc_x87.hpp"
#include "driving/pc_common_control.hpp"
#include "driving/pc_car_services_ghost.hpp"
#include "platform/race_ghosts.hpp"   // 46F350 car work reset (shared with the ghost cars)
#include "platform/vehicle_body_init.hpp"   // 516E10 rigid body (course objects)
#include "driving/pc_crash.hpp"            // 513650 motion channels (course objects)
#include <array>
#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <functional>
#include <stdexcept>
namespace outrun::platform {
namespace {
using driving::X87;
std::uint32_t bits(float f){std::uint32_t u;std::memcpy(&u,&f,4);return u;}
float from_bits(std::uint32_t u){float f;std::memcpy(&f,&u,4);return f;}
// CVTTSS2SI: truncation, the integer indefinite value when out of range/NaN.
std::int32_t cvtt(float f){
    if(!(f>-2147483904.0f&&f<2147483648.0f))return std::int32_t(0x80000000u);
    return std::int32_t(f);
}
// 582194 _ftol2: EAX = low dword of the 64-bit truncation.
std::uint32_t ftol(X87 v){return std::uint32_t(std::uint64_t(driving::x87_ftol64(v)));}
// FILD m32 of an unsigned value as the PC does it: fild + [628070] (2^32) when negative.
X87 fild_u32(std::uint32_t v){X87 r=X87(std::int32_t(v));if(std::int32_t(v)<0)r=r+X87(4294967296.0f);return r;}
// SSE scalar arithmetic (float, round to nearest).
float sadd(float a,float b){return a+b;}
float ssub(float a,float b){return a-b;}
float smul(float a,float b){return a*b;}
float sdiv(float a,float b){return a/b;}
// COMISS a,b followed by a conditional jump: ja = a > b, jae = a >= b (both
// false when unordered); jb = a < b or unordered, jbe = a <= b or unordered.
bool gt(float a,float b){return a>b;}
bool jb(float a,float b){return !(a>=b);}
bool jbe(float a,float b){return !(a>b);}
// .rdata constants.
constexpr float K619a34=0.0f,K5b4440=24.0f,K5b4358=50.0f,K628070=4294967296.0f;

struct T {
    PcRaceContext& c;
    PcRaceMemory& m;
    explicit T(PcRaceContext& cc):c(cc),m(cc.m){}
    std::uint32_t call(std::uint32_t pc,std::initializer_list<std::uint32_t> args,std::uint32_t eax=0,std::uint32_t ecx=0){
        PcRaceCall k{};k.pc=pc;k.eax=eax;k.ecx=ecx;k.argc=std::uint32_t(args.size());
        if(args.size()>k.args.size())throw std::logic_error("race traffic: too many PC call arguments");
        unsigned i=0;for(auto a:args)k.args[i++]=a;
        if(!c.service)throw std::logic_error("race traffic: no service for PC call");
        return c.service(k);
    }
    // x87-returning service: the service returns the float bits.
    X87 callf(std::uint32_t pc,std::initializer_list<std::uint32_t> args){return X87(from_bits(call(pc,args)));}
    std::uint8_t u8(std::uint32_t a){return m.u8(a);}
    std::int8_t i8(std::uint32_t a){return std::int8_t(m.u8(a));}
    std::uint16_t u16(std::uint32_t a){return m.u16(a);}
    std::int16_t i16(std::uint32_t a){return m.i16(a);}
    std::uint32_t u32(std::uint32_t a){return m.u32(a);}
    std::int32_t i32(std::uint32_t a){return m.i32(a);}
    float f32(std::uint32_t a){return m.f32(a);}
    void p8(std::uint32_t a,std::uint32_t v){m.put8(a,std::uint8_t(v));}
    void p16(std::uint32_t a,std::uint32_t v){m.put16(a,std::uint16_t(v));}
    void p32(std::uint32_t a,std::uint32_t v){m.put32(a,v);}
    void pf(std::uint32_t a,float v){m.putf(a,v);}
    std::uint32_t work(std::uint32_t id){return u32(0x799b38u+id*0x3cu);}   // event record +8: work
    std::uint32_t player(){return u32(0x799d18u);}
    std::uint32_t racers(){return u32(0x80fb00u);}
    std::uint32_t racer(std::uint32_t i){return racers()+i*0xa0u;}
    bool flag_44ff10(){return call(0x44ff10u,{})!=0u;}

    // ---- small leaves -------------------------------------------------------------
    // 476300 / 476320: config 80FB0C +30 bits 3 / 4.
    std::uint32_t cfg_bit_476300(){const auto c0=u32(0x80fb0cu);return (c0&&(u8(c0+0x30)&0x08u))?1u:0u;}
    std::uint32_t cfg_bit_476320(){const auto c0=u32(0x80fb0cu);return (c0&&(u8(c0+0x30)&0x10u))?1u:0u;}
    // 476350: first free car event 9..[680AD4]+7 (0x19A when none).
    std::uint32_t free_event_476350(){
        const std::uint32_t end=u32(0x680ad4u)+8u;
        if(!(std::int32_t(end)>9))return 0x19au;
        for(std::uint32_t id=9;std::int32_t(id)<std::int32_t(end);++id)if(!(u8(0x79fb48u+id)&3u))return id;
        return 0x19au;   // falls out with eax = 0x19A
    }
    // 477370: the leading racer record (64E190) or 0.
    std::uint32_t leader_477370(){
        const std::uint32_t r=racers();if(!r)return 0;
        const std::int32_t i=i32(0x64e190u);if(i<0)return 0;
        return std::uint32_t(i)*0xa0u+r;
    }
    // 46C870(model): 5B3C34 pairs {family, model}: the family of `model`.
    std::uint32_t family_46c870(std::uint8_t model){
        for(std::uint32_t i=0;;++i){
            if(i>0x400)throw std::runtime_error("46C870: model without a family entry");
            const std::int8_t v=i8(0x5b3c35u+i*2u);
            if(v>=0&&std::uint8_t(v)==model)return std::uint32_t(std::int32_t(i8(0x5b3c34u+i*2u)));
        }
    }
    // 46C570(index, scale): 16-bit LCG in 6A4E2C[index]; returns x87 (n * scale) * (1/65536).
    X87 random_46c570(std::uint32_t index,float scale){
        const std::uint16_t lo=std::uint16_t(u16(0x6a4e2cu+index*4u)*0x5e5u+0x29u);
        p32(0x6a4e2cu+index*4u,lo);
        return (X87(std::int32_t(lo))*X87(scale))*X87(from_bits(0x37800000u));   // [5B4324] 1/65536
    }
    // 476480(car eax, racer esi, x): pull the racer's catch-up distance down.
    void catchup_476480(std::uint32_t car,std::uint32_t r,float x){
        float v=float(std::int32_t(u8(car+0xc36))-std::int32_t(u8(r+0x74)));
        v=sadd(smul(v,K619a34),x);v=sadd(v,61.0f);
        if(gt(v,f32(r+0x40))){pf(r+0x40,v);p32(r+0x3c,ftol(X87(v)));}
    }
    // 4764D0(car eax, racer esi, xmm0 = x).
    void catchup_4764d0(std::uint32_t car,std::uint32_t r,float x){
        float v=ssub(ssub(x,10.0f),1.0f);
        const float d=smul(float(std::int32_t(u8(r+0x74))-std::int32_t(u8(car+0xc36))),0.0f);
        v=ssub(v,d);
        if(!gt(v,0.0f))return;
        const float cur=f32(r+0x40);
        if(!gt(cur,v))return;
        const std::int16_t lap=i16(0x804388u);
        if(lap>0){
            const float half=float(std::int32_t(std::int16_t(lap>>1)));
            if(!gt(half,ssub(cur,v)))return;
        }
        pf(r+0x40,v);p32(r+0x3c,ftol(X87(v))&0xffffu);
    }
    // 46E460: the 799D18 chain through +314 into 803750.. (zero terminated).
    void chain_46e460(){
        std::uint32_t e=u32(player()+0x314);std::uint32_t i=0;
        while(e){p32(0x803750u+i*4u,e);e=u32(e+0x314);++i;}
        p32(0x803750u+i*4u,0);
    }
    // ---- init (47DAC0) -------------------------------------------------------------
    void car_ids_46c9d0(){
        const std::uint32_t n=u32(0x680ad4u);
        if(std::int32_t(n)<0x18){
            std::uint32_t id=n+8u;
            for(std::uint32_t rec=0x799d18u+n*0x3cu;rec<0x79a2b8u;rec+=0x3cu,++id){
                const std::uint32_t w=u32(rec);p32(w,id);p8(w+0xdc8,7);p8(w+0xdc9,7);
            }
        }
        for(std::uint32_t a=0x802e10u;a<0x802e10u+0x900u;a+=4)p32(a,0);
        clear_46c950();clear_46c8d0();
        const std::uint32_t v=call(0x456d60u,{})&0xffu;
        p8(0x80373au,v);p8(0x800d4du,v);
    }
    void clear_46c950(){
        for(std::uint32_t k=0;k<0x2cu;k+=4){p32(0x804358u+k,0);p32(0x804450u+k,0);}
        for(std::uint32_t a:{0x803718u,0x80371cu,0x803720u,0x803724u})p32(a,0);
        p16(0x800d50u,0);p32(0x800d9cu,0);p8(0x800d38u,0);p32(0x800d3cu,0);p32(0x803728u,0);p8(0x800d40u,0);p32(0x80372cu,0);
        p8(0x80a668u,0x78);p8(0x80a669u,0x67);p8(0x80a66au,0x69);p8(0x80a66bu,0x5e);p8(0x80a66cu,0xd4);
    }
    void clear_46c8d0(){
        for(std::uint32_t a=0x800af8u;a<=0x800cf0u;a+=0x38u){p32(a,0x19au);p32(a+4,0x19au);}
        p16(0x8037b0u,0);p32(0x804384u,0);
    }
    // 4EF890: the 31 appear tables 6A5DF8.. -> 84BD68.. and their course fix-up.
    void tables_4ef890(){
        static constexpr std::uint32_t copies[][3]{
            {0x6a5df8u,0x84c380u,0x14},{0x6a5e48u,0x84cd50u,0x46},{0x6a5f60u,0x84c500u,0x5f},{0x6a60e0u,0x84ca50u,0x19},
            {0x6a6148u,0x84c948u,0x41},{0x6a6250u,0x84c7f8u,0x14},{0x6a62a0u,0x84c0c0u,0x14},{0x6a62f0u,0x84cb10u,0x0f},
            {0x6a6330u,0x84bd68u,0x19},{0x6a6398u,0x84c680u,0x1e},{0x6a6410u,0x84cc20u,0x19},{0x6a6478u,0x84cc88u,0x32},
            {0x6a6540u,0x84c848u,0x14},{0x6a6590u,0x84c1b0u,0x14},{0x6a65e0u,0x84bec0u,0x14},{0x6a6630u,0x84c7a8u,0x14},
            {0x6a6680u,0x84cac0u,0x14},{0x6a66d0u,0x84cb50u,0x1e},{0x6a6748u,0x84c3d0u,0x32},{0x6a6810u,0x84bdd0u,0x28},
            {0x6a68b0u,0x84bf10u,0x14},{0x6a6900u,0x84c250u,0x37},{0x6a69e0u,0x84c200u,0x14},{0x6a6a30u,0x84cbc8u,0x14},
            {0x6a6a80u,0x84c110u,0x28},{0x6a6b20u,0x84c898u,0x14},{0x6a6b70u,0x84c498u,0x19},{0x6a6bd8u,0x84c330u,0x14},
            {0x6a6c28u,0x84be70u,0x14},{0x6a6c78u,0x84c740u,0x19}};
        for(const auto& c0:copies)for(std::uint32_t k=0;k<c0[2]*4u;k+=4)p32(c0[1]+k,u32(c0[0]+k));
        for(std::uint32_t i=0;i<0x42u;++i){
            const std::uint32_t len=call(0x44c990u,{i});
            for(std::uint32_t rec=u32(0x6a6ce0u+i*4u);i32(rec)<0x42;rec+=0x14u){
                p8(rec+0x11,0);
                if(i<0x3cu&&i>=0x1eu){
                    p16(rec+8,std::uint16_t(std::uint16_t(len)-u16(rec+8)+1u));
                    p8(rec+0xa,std::uint8_t(5u-u8(rec+0xa)));
                }
            }
        }
    }
    void init_47dac0(){
        p32(0x802e08u,0);
        car_ids_46c9d0();
        pf(0x803714u,K5b4440);
        p8(0x803739u,0);p8(0x80373bu,0);p8(0x800d28u,0);p8(0x800d41u,0x2c);p8(0x800d4cu,0xff);p8(0x803744u,0);p8(0x800d52u,0);
        p32(0x64e08cu,1);pf(0x80434cu,K5b4358);
        p32(0x84cc18u,0);                                   // 4EF870(0)
        tables_4ef890();
        if(u32(0x78026cu)==3u)return;
        const std::uint32_t variant=u32(0x780258u);
        for(std::uint32_t model=0;model<0x2cu;++model){
            if(!call(0x499ba0u,{model}))continue;
            std::uint32_t k=0;
            for(;;++k){const std::int32_t v=i32(0x5b3cb8u+k*8u);if(v>=0&&std::int32_t(std::int8_t(model))==v)break;}
            const std::uint32_t token=u32(0x5b3cbcu+k*8u);
            call(0x406630u,{token,call(0x414040u,{})});
        }
        if(variant&&!(call(0x4962a0u,{})&0xffu)&&variant!=7u&&variant!=8u&&variant!=9u)call(0x406630u,{0xe0014u,call(0x414040u,{})});
        const std::uint32_t gate=u32(0x80fb14u);
        p32(0x804350u,0);
        if(gate){spawn_47d8b0();return;}
        if(!(call(0x4957f0u,{})&0xffu))return;
        if(call(0x495b00u,{})&0xffu)return;
        if(call(0x495b20u,{})==4u)return;
        spawn_47d8b0();
    }
    // ---- spawn (47D8B0) ------------------------------------------------------------
    void spawn_47d8b0(){
        ranks_477510();order_477c90();appear_47c8a0();
        ranks_477510();order_477c90();ranks_477510();order_477c90();
        std::uint32_t ebx=1;
        const std::uint32_t end=u32(0x680ad4u)+8u;
        if(std::int32_t(end)>9){
            for(std::uint32_t id=9;std::int32_t(id)<std::int32_t(u32(0x680ad4u)+8u);++id){
                if(!(u8(0x79fb48u+id)&3u))continue;
                const std::uint32_t w=u32(0x799b38u+id*0x3cu);
                pf(w+0x1c4,0.0f);p32(w+0x1f4,0);
                if(!(call(0x4b00d0u,{})&0xffu)){p32(w+0x208,ebx);p32(w+0x1d8,ebx);}
            }
            p32(0x80fb14u,ebx);
            return;
        }
        p32(0x80fb14u,1);
    }
    // ---- ranks (477510) ------------------------------------------------------------
    // Entry (0x14 bytes): +0 course position, +4 fraction, +8 position+fraction,
    // +C position - player position, +10 racer index (-1 = player).
    void entry_set(std::uint32_t e,std::uint32_t pos,float f){
        p32(e,pos);pf(e+4,f);if(gt(0.0f,f))pf(e+4,0.0f);
        const float x=f32(e+4);const float frac=ssub(x,float(cvtt(x)));
        pf(e+4,frac);pf(e+8,sadd(float(i32(e)),frac));
    }
    void ranks_477510(){
        if(flag_44ff10())return;
        const std::uint32_t list=racers();if(!list)return;
        std::uint32_t table=u32(0x80fb1cu);if(!table)return;
        std::uint32_t n=0;const std::uint32_t pl=player();
        std::uint32_t e;
        if(u8(0x80fb18u)){
            for(std::uint32_t i=0;std::int32_t(i)<i32(0x80fb04u);++i){
                const std::uint32_t r=list+i*0xa0u;
                if(!u32(r+0x54))continue;
                e=table+std::uint32_t(u8(r+0x74))*0x14u;
                entry_set(e,u32(r+0x3c),f32(r+0x40));
                p32(e+0xc,u32(r+0x3c)-std::uint32_t(u16(pl+0x260)));
                p32(e+0x10,i);++n;
            }
            e=table+std::uint32_t(u8(pl+0xc36))*0x14u;
            player_entry(e,pl);
        }else{
            const std::int32_t count=i32(0x80fb04u);
            for(std::int32_t i=count-1;i>=0;--i){
                const std::uint32_t r=list+std::uint32_t(i)*0xa0u;
                if(!u32(r+0x54))continue;
                entry_set(table,u32(r+0x3c),f32(r+0x40));
                p32(table+0xc,u32(r+0x3c)-std::uint32_t(u16(pl+0x260)));
                p32(table+0x10,std::uint32_t(i));table+=0x14u;++n;
            }
            e=table;
            player_entry(e,pl);
            p8(0x80fb18u,1);
        }
        p32(e,u16(pl+0x260));++n;p32(e+0xc,0);p32(e+0x10,0xffffffffu);
        if(n>1)qsort_580cb0(u32(0x80fb1cu),n,0x14,0x4772d0u);
        const std::uint32_t t=u32(0x80fb1cu);
        p32(0x64e190u,u32(t+0x10));p32(0x64e194u,u32(t+n*0x14u-4u));p32(0x80fb2cu,n);
        for(std::uint32_t i=0;i<n;++i){
            const std::uint32_t en=t+i*0x14u;
            const std::int32_t idx=i32(en+0x10);
            auto rank_car=[&](std::uint32_t car){
                p8(car+0xc37,u8(car+0xc36));p8(car+0xc36,i);
                p32(car+0xc20,u32(en+0xc));p32(car+0xbc4,u32(en+8));p32(car+0xbdc,u32(en+4));
            };
            if(idx<0){rank_car(pl);continue;}
            const std::uint32_t r=list+std::uint32_t(idx)*0xa0u;
            p8(r+0x74,i);p32(r+0x48,u32(en+0xc));
            if(i32(r+0x4c)>=0)rank_car(work(u32(r+0x4c)));
        }
        p32(0x8037b8u,0);
        const std::uint32_t lead=list?(i32(0x64e190u)>=0?u32(0x64e190u)*0xa0u+list:0u):0u;
        if(lead)return;
        if(std::int32_t(n)>=2&&i32(u32(0x80fb1cu)+0x20)<-0x19)p32(0x8037b8u,1);
    }
    void player_entry(std::uint32_t e,std::uint32_t pl){
        const X87 st=callf(0x4a6cf0u,{pl});
        pf(e+4,driving::x87_float(st));
        if(X87(K619a34)>st)pf(e+4,0.0f);
        const float x=f32(e+4);const float frac=ssub(x,float(cvtt(x)));
        pf(e+4,frac);
        pf(e+8,sadd(float(std::int32_t(u16(pl+0x260))),frac));
    }
    // ---- 477C90: mission order of the racers (80FB30 pairs) -------------------------
    void order_477c90(){
        if(flag_44ff10())return;
        if(!racers())return;                               // bridge 477C9D: EAX = [80FB00]
        if(!u32(0x80fb30u))return;
        if(!call(0x495b20u,{}))return;
        const std::uint32_t pl=player();
        std::uint32_t out=u32(0x80fb30u);
        p32(out,call(0x495800u,{}));p32(out+4,0xffffffffu);out+=8;
        std::uint32_t n=1;const std::uint32_t list=racers();
        for(std::uint32_t i=0;std::int32_t(i)<i32(0x80fb04u);++i){
            const std::uint32_t r=list+i*0xa0u;
            if(!u32(r+0x54))continue;
            p32(out,u32(r+0x98));p32(out+4,i);out+=8;++n;
        }
        if(n>1)qsort_580cb0(u32(0x80fb30u),n,8,0x477310u);
        const std::uint32_t l=racers();
        for(std::uint32_t i=0;i<n;++i){
            const std::uint32_t en=u32(0x80fb30u)+i*8u;
            const std::int32_t idx=i32(en+4);
            if(idx<0){p8(pl+0xc41,u8(pl+0xc40));p8(pl+0xc40,i);continue;}
            const std::uint32_t r=l+std::uint32_t(idx)*0xa0u;
            p8(r+0x75,i);
            if(i32(r+0x4c)<0)continue;
            const std::uint32_t w=work(u32(r+0x4c));
            p8(w+0xc41,u8(w+0xc40));p8(w+0xc40,i);
        }
    }
    // ---- ranking table 80FB20 (476760 and its callees) ------------------------------
    // 476380: racer record of car id EBX (racers - 0xA0 when absent, as the PC).
    std::uint32_t racer_by_id_476380(std::uint32_t id){
        const std::int32_t n=i32(0x80fb04u);
        std::int32_t idx=-1;
        for(std::int32_t i=0;i<n;++i)if(u32(racer(std::uint32_t(i))+0x4c)==id){idx=std::int32_t(u8(racer(std::uint32_t(i))+0x70));break;}
        return racers()+std::uint32_t(idx)*0xa0u;
    }
    // 4765F0: first entry with lane byte a0 keeps it, else the first free (+10 == FF) gets the record.
    void insert_4765f0(std::uint32_t a0,std::uint32_t a1,std::uint32_t a2,std::uint32_t a3,std::uint32_t a4,
                       std::uint32_t a5,std::uint32_t a6,std::uint32_t a7,std::uint32_t a8){
        const std::int32_t n=i32(0x80fb04u)+1;
        const std::uint32_t key=a0&0xffu,table=u32(0x80fb20u);
        for(std::int32_t i=0;i<n;++i){
            const std::uint32_t e=table+std::uint32_t(i)*0x1cu;
            const std::uint8_t lane=u8(e+0x10);
            if(std::uint32_t(std::int32_t(std::int8_t(lane)))==key)return;
            if(lane!=0xffu)continue;
            p8(e+0x10,a0);p32(e,a4);p8(e+0x11,a1);p32(e+4,a5);p8(e+0x12,a2);p8(e+0x13,a3);
            p32(e+8,a8);p32(e+0x14,a6);p32(e+0x18,a7);
            return;
        }
    }
    // 476740 / inline copies: C5C bit 0 with B64 = 0x3C.
    void mark_476740(std::uint32_t car){
        const std::uint32_t f=u32(car+0xc5c);
        if(f&1u)return;
        p16(car+0xb64,0x3cu);p32(car+0xc5c,f|1u);
    }
    // Course position <<8 minus the fraction of a car work / racer record.
    std::uint32_t car_place(std::uint32_t car){
        return (std::uint32_t(u16(car+0x260))<<8)-ftol(X87(f32(car+0xbdc))*X87(K(0x5b4430u)));
    }
    std::uint32_t racer_place(std::uint32_t r){
        const float f=f32(r+0x40);
        return (u32(r+0x3c)<<8)-ftol((X87(f)-X87(cvtt(f)))*X87(K(0x5b4430u)));
    }
    // ftol(unsigned d / unsigned base * t).
    std::uint32_t share(std::uint32_t d,std::uint32_t base,std::uint32_t t){
        return ftol(fild_u32(d)/fild_u32(base)*X87(std::int32_t(t)));
    }
    void racer_entry(std::uint32_t rec,std::uint32_t place,std::uint32_t time){
        insert_4765f0(u8(rec+0x70),u8(rec+0x76),u8(rec+0x72),u8(rec+0x73),place,time,u32(rec+0x98),rec+0x78,0);
        p32(rec+0x6c,1);
    }
    void player_entry(std::uint32_t car,std::uint32_t place,std::uint32_t time){
        const std::uint32_t a=call(0x495800u,{0x7c23e0u,1u});
        insert_4765f0(u8(0x80fb04u),u8(car+0xc35),u8(car+0x11),u8(car+0x12),place,time,a,0x7c23e0u,1);
    }
    // 4B0330: marks the open traffic car works (events [680AD4]+8..31).
    void mark_traffic_4b0330(){
        for(std::uint32_t id=u32(0x680ad4u)+8u;std::int32_t(id)<0x20;++id){
            if(!(u8(0x79fb48u+id)&3u))continue;
            mark_476740(u32(0x799b38u+id*0x3cu));
        }
    }
    // 476680: sorts the filled entries, latches 80FB24 once the table is full.
    void sort_476680(){
        const std::int32_t n=i32(0x80fb04u)+1;
        std::int32_t count=0;
        if(n>0){
            const std::uint32_t table=u32(0x80fb20u);
            while(count<n&&u8(table+std::uint32_t(count)*0x1cu+0x10)!=0xffu)++count;
        }
        if(count<=1||u8(0x80fb24u))return;
        const std::int32_t mode=std::int32_t(call(0x495b20u,{}));
        qsort_580cb0(u32(0x80fb20u),std::uint32_t(count),0x1c,(mode>=2&&mode<=3)?0x4765d0u:0x4765a0u);
        if(u8(0x80fb24u))return;
        if(count>=i32(0x80fb04u)+1)p8(0x80fb24u,1);
    }
    // 476760: ranking entries of the cars that crossed the player's position (goal).
    void ranking_476760(std::uint32_t player){
        if(!(u16(player+0x25c)&0x110u))return;
        if(!u32(player+0x5c))return;
        if(!(call(0x44b7b0u,{0u})&0xffu))return;
        const std::uint32_t P=car_place(player);
        const std::uint32_t t=call(0x4505d0u,{});
        auto cars=[&](auto&& body){
            for(std::uint32_t id=8;std::int32_t(id)<std::int32_t(u32(0x680ad4u)+8u);++id){
                if((u8(0x79fb48u+id)&3u)!=2u)continue;
                const std::uint32_t car=u32(0x799b38u+id*0x3cu);
                if(car==player)continue;
                body(car);
            }
        };
        cars([&](std::uint32_t car){
            const std::uint32_t Q=car_place(car);
            if(Q<P)return;
            const std::uint32_t r=share(Q-P,P,t);
            if(u8(car+4)&1u){player_entry(car,Q,t-r);return;}
            racer_entry(racer_by_id_476380(u32(car)),Q,t-r);
            mark_476740(car);
        });
        for(std::int32_t k=0;k<i32(0x80fb04u);++k){
            const std::uint32_t rec=racer(std::uint32_t(k));
            const std::uint32_t Q=racer_place(rec);
            if(Q<P)continue;
            racer_entry(rec,Q,t-share(Q-P,P,t));
        }
        if(u8(player+4)&1u)player_entry(player,P,t);
        else{
            racer_entry(racer_by_id_476380(u32(player)),P,t);
            mark_476740(player);
        }
        if(u8(player+4)&1u){
            cars([&](std::uint32_t car){
                const std::uint32_t Q=car_place(car);
                if(Q>P)return;
                racer_entry(racer_by_id_476380(u32(car)),Q,share(P-Q,P,t)+t);
                mark_476740(car);
            });
            for(std::int32_t k=0;k<i32(0x80fb04u);++k){
                const std::uint32_t rec=racer(std::uint32_t(k));
                const std::uint32_t Q=racer_place(rec);
                if(Q>P)continue;
                racer_entry(rec,Q,share(P-Q,P,t)+t);
            }
            mark_traffic_4b0330();
        }
        sort_476680();
    }
    // ---- CRT qsort 580CB0 ------------------------------------------------------------
    int compare(std::uint32_t fn,std::uint32_t a,std::uint32_t b){
        switch(fn){
        case 0x4772d0u:{   // entries: position descending, then fraction descending
            const std::uint32_t pa=u32(a),pb=u32(b);
            if(pa!=pb)return std::int32_t(pb-pa);
            const float fa=f32(a+4),fb=f32(b+4);
            if(fa==fb)return 0;                            // ordered equal
            return gt(fb,fa)?1:-1;}
        case 0x477310u:return std::int32_t(u32(b)-u32(a));
        case 0x46e420u:return gt(f32(work(u32(a))+0x300),f32(work(u32(b))+0x300))?1:-1;
        case 0x479360u:{   // course order, then 46E250 along the leader
            const std::uint32_t wa=work(u32(a)),wb=work(u32(b));
            const std::int32_t d=diff_46f990(wa+0x5c,wb+0x5c);
            if(d<0)return 1;
            if(d>0)return -1;
            return X87(0.0f)>ahead_46e250(wb,wa)?1:-1;}
        case 0x4793c0u:{
            const std::uint32_t wa=work(u32(a)),wb=work(u32(b));
            const float v=ssub(sadd(float(diff_46f990(wa+0x5c,wb+0x5c)),f32(wa+0x1050)),f32(wb+0x1050));
            return gt(0.0f,v)?1:-1;}
        case 0x4765a0u:{   // ranking entries: +4 ascending (signed)
            const std::int32_t va=i32(a+4),vb=i32(b+4);
            if(va>vb)return 1;
            return va==vb?0:-1;}
        case 0x4765d0u:{   // ranking entries: +14 descending (unsigned)
            const std::uint32_t va=u32(a+0x14),vb=u32(b+0x14);
            if(va>vb)return -1;
            return va!=vb?1:0;}
        default:throw std::logic_error("race traffic: qsort comparator not ported");
        }
    }
    void qsort_580cb0(std::uint32_t base,std::uint32_t num,std::uint32_t width,std::uint32_t fn){
        auto comp=[&](std::uint32_t a,std::uint32_t b){return compare(fn,a,b);};
        auto swap=[&](std::uint32_t a,std::uint32_t b){
            if(a==b)return;
            for(std::uint32_t k=0;k<width;++k){const std::uint8_t x=u8(a+k),y=u8(b+k);p8(b+k,x);p8(a+k,y);}
        };
        if(num<2u||width==0u)return;
        std::array<std::uint32_t,30> lostk{},histk{};int stkptr=0;
        std::uint32_t lo=base,hi=base+(num-1u)*width;
        for(;;){
            const std::uint32_t size=(hi-lo)/width+1u;
            if(size<=8u){
                std::uint32_t h=hi;
                while(h>lo){
                    std::uint32_t max=lo;
                    for(std::uint32_t p=lo+width;p<=h;p+=width)if(comp(p,max)>0)max=p;
                    swap(max,h);
                    h-=width;
                }
            }else{
                std::uint32_t mid=lo+(size/2u)*width;
                if(comp(lo,mid)>0)swap(lo,mid);
                if(comp(lo,hi)>0)swap(lo,hi);
                if(comp(mid,hi)>0)swap(mid,hi);
                std::uint32_t loguy=lo,higuy=hi;
                for(;;){
                    if(mid>loguy){do{loguy+=width;}while(loguy<mid&&comp(loguy,mid)<=0);}
                    if(mid<=loguy){do{loguy+=width;}while(loguy<=hi&&comp(loguy,mid)<=0);}
                    do{higuy-=width;}while(higuy>mid&&comp(higuy,mid)>0);
                    if(higuy<loguy)break;
                    swap(loguy,higuy);
                    if(mid==higuy)mid=loguy;
                }
                higuy+=width;
                if(mid<higuy){do{higuy-=width;}while(higuy>mid&&comp(higuy,mid)==0);}
                if(mid>=higuy){do{higuy-=width;}while(higuy>lo&&comp(higuy,mid)==0);}
                if(std::int32_t(higuy-lo)>=std::int32_t(hi-loguy)){
                    if(lo<higuy){lostk.at(std::size_t(stkptr))=lo;histk.at(std::size_t(stkptr))=higuy;++stkptr;}
                    if(loguy<hi){lo=loguy;continue;}
                }else{
                    if(loguy<hi){lostk.at(std::size_t(stkptr))=loguy;histk.at(std::size_t(stkptr))=hi;++stkptr;}
                    if(lo<higuy){hi=higuy;continue;}
                }
            }
            --stkptr;
            if(stkptr<0)return;
            lo=lostk[std::size_t(stkptr)];hi=histk[std::size_t(stkptr)];
        }
    }
    // ---- scratch stack: module locals passed by address --------------------------
    std::uint32_t sp{TrafficStackBase+TrafficStackSize};
    struct Frame {
        T& t;std::uint32_t saved,base;
        Frame(T& tt,std::uint32_t n):t(tt),saved(tt.sp){
            t.sp=(t.sp-n)&~0xfu;base=t.sp;
            if(base<TrafficStackBase)throw std::logic_error("race traffic: scratch stack overflow");
            for(std::uint32_t k=0;k<n;k+=4)t.m.put32(base+k,0);
        }
        ~Frame(){t.sp=saved;}
        std::uint32_t operator()(std::uint32_t off)const{return base+off;}
    };
    float K(std::uint32_t a){return f32(a);}            // .rdata constant
    // ---- math leaves (x87, PC 24-bit precision) -------------------------------------
    X87 x(std::uint32_t a){return X87(f32(a));}
    void sub_40efa0(std::uint32_t o,std::uint32_t a,std::uint32_t b){
        const float r0=driving::x87_float(x(a)-x(b)),r1=driving::x87_float(x(a+4)-x(b+4)),r2=driving::x87_float(x(a+8)-x(b+8));
        pf(o,r0);pf(o+4,r1);pf(o+8,r2);
    }
    void sub_40ef70(std::uint32_t a,std::uint32_t b){
        pf(a,driving::x87_float(x(a)-x(b)));pf(a+4,driving::x87_float(x(a+4)-x(b+4)));pf(a+8,driving::x87_float(x(a+8)-x(b+8)));
    }
    X87 unit_40eeb0(std::uint32_t v){
        const X87 len=driving::x87_sqrt((x(v)*x(v)+x(v+4)*x(v+4))+x(v+8)*x(v+8));
        if(len.v>0.0001){
            const X87 inv=X87(1.0f)/len;
            pf(v,driving::x87_float(inv*x(v)));pf(v+4,driving::x87_float(inv*x(v+4)));pf(v+8,driving::x87_float(inv*x(v+8)));
        }
        return len;
    }
    X87 dot_40efd0(std::uint32_t a,std::uint32_t b){return (x(a+8)*x(b+8)+x(a+4)*x(b+4))+x(a)*x(b);}
    void scale_40f050(std::uint32_t o,std::uint32_t v,float s){
        const float r0=driving::x87_float(X87(s)*x(v)),r1=driving::x87_float(X87(s)*x(v+4)),r2=driving::x87_float(X87(s)*x(v+8));
        pf(o,r0);pf(o+4,r1);pf(o+8,r2);
    }
    // 40F180(out, a, sa, b, sb): out = a*sa + b*sb.
    void blend_40f180(std::uint32_t o,std::uint32_t a,float sa,std::uint32_t b,float sb){
        const X87 p0=X87(sa)*x(a),p1=X87(sa)*x(a+4);const float p2=driving::x87_float(X87(sa)*x(a+8));
        const X87 r0=X87(sb)*x(b);const float r1=driving::x87_float(X87(sb)*x(b+4)),r2=driving::x87_float(X87(sb)*x(b+8));
        const float o0=driving::x87_float(r0+p0),o1=driving::x87_float(X87(r1)+p1),o2=driving::x87_float(X87(r2)+X87(p2));
        pf(o,o0);pf(o+4,o1);pf(o+8,o2);
    }
    void cross_40eff0(std::uint32_t o,std::uint32_t a,std::uint32_t b){
        const float r0=driving::x87_float(x(b+8)*x(a+4)-x(a+8)*x(b+4));
        const float r1=driving::x87_float(x(a+8)*x(b)-x(b+8)*x(a));
        const float r2=driving::x87_float(x(a)*x(b+4)-x(b)*x(a+4));
        pf(o,r0);pf(o+4,r1);pf(o+8,r2);
    }
    // 449800(out, v): out = {-atan2(y, sqrt(z*z + x*x)), atan2(x, z), 0}.
    void angles_449800(std::uint32_t o,std::uint32_t v){
        const float yaw=driving::x87_float(driving::x87_atan2(x(v),x(v+8)));
        pf(o+4,yaw);
        const float zz=smul(f32(v+8),f32(v+8)),xx=smul(f32(v),f32(v));
        const float hyp=driving::x87_float(driving::x87_sqrt(X87(sadd(zz,xx))));
        pf(o,driving::x87_float(-driving::x87_atan2(x(v+4),X87(hyp))));
        pf(o+8,0.0f);
    }
    // ---- matrix stack -----------------------------------------------------------------
    driving::CourseProbe vec(std::uint32_t a){return {f32(a),f32(a+4),f32(a+8)};}
    void put_vec(std::uint32_t a,const driving::CourseProbe& v){pf(a,v.x);pf(a+4,v.y);pf(a+8,v.z);}
    void push_unit(){driving::pc_matrix_push_unit(c.matrices);}
    void push_load(std::uint32_t a){driving::pc_matrix_push_load(c.matrices,m.bytes(a,64));}
    void load(std::uint32_t a){driving::pc_matrix_load(c.matrices,m.bytes(a,64));}
    void pop(){driving::pc_matrix_pop(c.matrices);}
    void rotate_x(float r){driving::pc_matrix_rotate_x(c.matrices,r);}
    void rotate_y(float r){driving::pc_matrix_rotate_y(c.matrices,r);}
    void rotate_z(float r){driving::pc_matrix_rotate_z(c.matrices,r);}
    void point_40a7d0(std::uint32_t o,std::uint32_t i){put_vec(o,driving::pc_matrix_point(c.matrices,vec(i)));}
    void vector_40a820(std::uint32_t o,std::uint32_t i){put_vec(o,driving::pc_matrix_vector(c.matrices,vec(i)));}
    void translate_40a2d0(std::uint32_t v){driving::pc_matrix_translate_vector(c.matrices,vec(v));}
    void invert_40a240(){const auto cur=c.matrices.current();(void)driving::pc_d3dx_matrix_inverse(cur,nullptr,cur);}   // D3DXMatrixInverse(cur, NULL, cur)
    // angle word * (2pi/65536) as the PC computes it: fild + fmul [628254], fstp m32.
    float word_radians(std::int32_t w){return driving::x87_float(X87(w)*X87(K(0x628254u)));}
    // ---- road places (OnRoadPlace: +0 kind, +4 branch, +8 cs word, +A lane, +C stage) -----
    // 46C500: network race (55A930 on 7F9460 and [7F94C8] == 1).
    std::uint32_t net_46c500(){
        if(!call(0x55a930u,{},0,0x7f9460u))return 0;
        return u32(0x7f94c8u)==1u?1u:0u;
    }
    std::uint32_t end_raw(std::uint32_t kind){return call(0x43d470u,{kind});}                       // full EAX
    std::int32_t end_43d470(std::uint32_t kind){return std::int32_t(end_raw(kind)&0xffffu);}           // movzx ax
    // 46F920(a, b): course distance a - b across the two course kinds.
    std::int32_t diff_46f920(std::uint32_t a,std::uint32_t b){
        if(u32(a)==u32(b))return std::int32_t(i16(a+8))-std::int32_t(i16(b+8));
        const std::int32_t e1=end_43d470(u32(b));
        const std::int32_t d1=e1+(std::int32_t(i16(a+8))-std::int32_t(i16(b+8)))+1;
        const std::int32_t e2=end_43d470(u32(a));
        const std::int32_t d2=e2+(std::int32_t(i16(b+8))-std::int32_t(i16(a+8)))+1;
        return d1<d2?d1:-d2;
    }
    // 46F990(a, b): othcarCalcCsLenDiff.
    std::int32_t diff_46f990(std::uint32_t a,std::uint32_t b){
        const std::uint32_t gate=u32(0x80fb14u);                       // bridge 46F990: EAX = [80FB14]
        if((gate&&i16(0x804388u)>0)||net_46c500())return diff_46f920(a,b);
        const std::int32_t sa=std::int32_t(call(0x44c940u,{u32(a+0xc)}));
        const std::int32_t sb=std::int32_t(call(0x44c940u,{u32(b+0xc)}));
        const std::int32_t d=std::int32_t(std::uint32_t(sa)-std::uint32_t(sb));
        if(d>1)return 100000;
        if(d<-1)return -100000;
        const std::uint32_t ka=u32(a),kb=u32(b);
        const std::int32_t ca=i16(a+8),cb=i16(b+8);
        if(ka==kb){
            if(u32(a+0xc)==u32(b+0xc))return ca-cb;
            return sa<=sb?-100000:100000;
        }
        if(u32(a+0xc)==u32(b+0xc)){
            if(ka==0u)return ca-cb-end_43d470(0)-1;
            return end_43d470(kb)+(ca-cb)+1;
        }
        if(u32(0x80fb14u)&&ka==1u)return ca-cb-end_43d470(1)-1;
        return end_43d470(kb)+(ca-cb)+1;
    }
    // 46FE70(place, delta): advance along the course, across the course kinds.
    std::uint32_t advance_46fe70(std::uint32_t pl,std::int32_t delta){
        std::uint32_t ebp=1;                                           // bridge 46FE79: EBP = 1
        const std::uint32_t end=std::uint32_t(end_43d470(u32(pl)));
        const std::uint16_t cs=std::uint16_t(u16(pl+8)+std::uint16_t(delta));
        p16(pl+8,cs);
        if(std::int16_t(cs)<0)p16(pl+8,0);
        if(std::int32_t(i16(pl+8))<=std::int32_t(end))return 1;
        auto back=[&](std::uint32_t r){p16(pl+8,std::uint16_t(u16(pl+8)+std::uint16_t(0xffffffffu-end)));return r;};
        const std::uint32_t st=call(0x44bdd0u,{});
        const std::int8_t bl=std::int8_t(call(0x44c940u,{st}));
        const std::int8_t al=std::int8_t(call(0x44be00u,{}));
        if(u32(pl)==0u){
            const std::int8_t lane=i8(pl+0xa);
            p32(pl,1);
            if(lane>=6)p8(pl+0xa,std::uint8_t(lane-6));
            if(bl<=al)return back(ebp);
            return back(0);
        }
        p32(pl,0);
        if(!call(0x44be50u,{0}))ebp=0;
        const std::uint32_t route=call(0x451350u,{call(0x44c940u,{u32(pl+0xc)})});
        p32(pl+0xc,call(0x44bdd0u,{}));
        const std::int8_t lane=i8(pl+0xa);
        if(lane<=2){if(route!=0u)return back(0);p8(pl+0xa,std::uint8_t(lane+3));return back(ebp);}
        if(lane<=5){if(route!=1u)return back(0);p8(pl+0xa,std::uint8_t(lane-3));return back(ebp);}
        if(lane<=8){if(route!=0u)return back(0);}
        else if(route!=1u)return back(0);
        p8(pl+0xa,std::uint8_t(lane-6));
        return back(ebp);
    }
    // 470120(kind, row, lane, sample, out): road sample {x, y, z, side, 0}; 1 when side > FLT_EPSILON.
    bool sample_470120(std::uint32_t kind,std::int32_t row,std::int32_t lane,std::int32_t index,std::uint32_t out){
        std::uint32_t t;
        if(kind==0u){
            if(lane>=6)return false;
            t=std::uint32_t((lane+row*6)*0x1030)+0x804480u;
        }else t=std::uint32_t((lane+row*12)*0x70c)+0x80a670u;
        if(u16(t)!=0x4f53u)return false;
        const std::uint32_t r=t+std::uint32_t(index*6)+4u;
        const float a=K(0x62818cu),b=K(0x5a29ecu);
        pf(out,smul(smul(float(std::int32_t(i16(r))),a),b));
        const std::int16_t y=std::int16_t(std::uint16_t(u16(r+4)<<1))>>1;
        pf(out+4,smul(smul(float(std::int32_t(y)),a),b));
        pf(out+8,smul(smul(float(std::int32_t(i16(r+2))),a),b));
        pf(out+0xc,(u8(r+5)&0x80u)?K(0x5b437cu):K(0x5b4378u));
        p32(out+0x10,0);
        return K(0x6281f0u)<f32(out+0xc);
    }
    // 470240(place, row, pos out, angles out, side out): position/heading of a road place.
    std::uint32_t place_pose_470240(std::uint32_t pl,std::int32_t row,std::uint32_t pos,std::uint32_t ang,std::uint32_t side){
        Frame f(*this,0x60);
        const std::uint32_t L08=f(0x08),L14=f(0x14),L24=f(0x24),L38=f(0x38);
        if(!sample_470120(u32(pl),row,i8(pl+0xa),i16(pl+8),L24))return 0;
        for(std::uint32_t k=0;k<0x10;k+=4)p32(L14+k,u32(pl+k));
        (void)advance_46fe70(L14,1);
        const std::int32_t index=i16(L14+8);                           // bridge 470295: movsx word [esp+0x24]
        const std::uint32_t kind2=u32(L14);
        if(!sample_470120(kind2,row,i8(L14+0xa),index,L38))return 0;
        push_load(call(0x44bed0u,{u32(pl)}));
        point_40a7d0(L24,L24);
        if(u32(pl)!=kind2)load(call(0x44bed0u,{kind2}));
        point_40a7d0(L38,L38);
        pop();
        sub_40efa0(L08,L24,L38);
        angles_449800(ang,L08);
        p32(pos,u32(L24));p32(pos+4,u32(L24+4));p32(pos+8,u32(L24+8));
        p32(side,u32(L24+0xc));
        return 1;
    }
    // 46D060(angles esi, speed, out): out = speed * (rotY(a.y) rotX(a.x) (0,0,-1)).
    void velocity_46d060(std::uint32_t ang,float speed,std::uint32_t out){
        Frame f(*this,0x10);
        pf(f(0),0.0f);pf(f(4),0.0f);pf(f(8),K(0x6280c4u));
        push_unit();rotate_y(f32(ang+4));rotate_x(f32(ang));
        vector_40a820(f(0),f(0));pop();
        scale_40f050(out,f(0),speed);
    }
    // 470930(esi = car): which side of the course the car is on (+4 bits 24/25, +60, lane).
    void side_470930(std::uint32_t car){
        std::uint32_t e=(u32(car+4)&0xfeffffffu)|0x2000000u;          // bridge 470933: and eax,0xFEFFFFFF
        p32(car+4,e);
        if(!u32(car+0x5c))return;
        if(i16(car+0x64)<=7&&!(e&0x20000u)&&i8(car+0x280)>=0)return;
        Frame f(*this,0x10);
        push_load(call(0x44bec0u,{}));
        invert_40a240();
        point_40a7d0(f(0),car+0x14);
        pop();
        const float z=0.0f;
        if(!jb(z,f32(f(0)))){                                          // comiss 0,[esp]: jb when 0 < x (or unordered)
            const std::uint32_t v=u32(car+4)&0xfcffffffu;p32(car+4,v);p32(car+0x60,0x64);
            const std::int8_t lane=i8(car+0x66);
            if(lane>=0&&lane<=2)return;
            if(lane>=6&&lane<=8)return;
            if((v&0x20u)&&u32(0x80fb14u))return;
            p8(car+0x66,2);
        }else{
            const std::uint32_t v=(u32(car+4)&0xfdffffffu)|0x1000000u;p32(car+4,v);p32(car+0x60,0x65);
            const std::int8_t lane=i8(car+0x66);
            if(lane>=3&&lane<=5)return;
            if(lane>=9&&lane<=0xb)return;
            if((v&0x20u)&&u32(0x80fb14u))return;
            p8(car+0x66,3);
        }
        pf(car+0xb08,z);pf(car+0xb00,z);pf(car+0xb04,z);
    }
    // 449640(out, m): Euler angles of the three normalised rows of m.
    void euler_449640(std::uint32_t o,std::uint32_t mt){
        (void)unit_40eeb0(mt);(void)unit_40eeb0(mt+0x10);(void)unit_40eeb0(mt+0x20);
        const X87 eps=X87(K(0x5a29e0u));
        auto atan2f_=[&](float y,float xx){return driving::x87_float(driving::x87_atan2(X87(y),X87(xx)));};
        if(driving::x87_abs(x(mt+0x14))>eps){
            const float a=atan2f_(f32(mt+4),f32(mt+0x14));
            pf(o+8,a);
            const float d=driving::x87_float(X87(f32(mt+0x14))/driving::x87_cos(X87(a)));
            const float p=atan2f_(ssub(0.0f,f32(mt+0x24)),d);
            pf(o,p);
            if(!(X87(K(0x628260u))<driving::x87_abs(X87(p))||std::isnan(p)))pf(o+4,atan2f_(f32(mt+0x20),f32(mt+0x28)));
            else pf(o+4,atan2f_(ssub(0.0f,f32(mt+0x20)),ssub(0.0f,f32(mt+0x28))));
            return;
        }
        if(driving::x87_abs(x(mt+4))>eps){
            pf(o+8,K(0x628260u));
            pf(o,atan2f_(ssub(0.0f,f32(mt+0x24)),f32(mt+4)));
            pf(o+4,atan2f_(f32(mt+0x18),ssub(0.0f,f32(mt+0x10))));
            return;
        }
        pf(o+8,0.0f);
        pf(o,gt(f32(mt+0x24),0.0f)?K(0x62825cu):K(0x628260u));
        pf(o+4,atan2f_(ssub(0.0f,f32(mt+8)),f32(mt)));
    }
    // 4866C0(model): the 0x44-byte model record (bridge 4866C7: + 0x650500).
    static std::uint32_t model_4866c0(std::int32_t model){return std::uint32_t(model*0x44)+0x650500u;}
    // ---- lights (46C6E0 / 46C5B0) ---------------------------------------------------
    // 46C5B0(t, mode, flag, *count, *a, *b, *fa, *c, *fb): the time-of-day light
    // thresholds of 64DEEC.
    void lights_46c5b0(float t,std::uint32_t mode,std::uint32_t count,std::uint32_t pa,std::uint32_t pb,std::uint32_t fa,std::uint32_t pc,std::uint32_t fb){
        const std::uint32_t tab=u32(0x64deecu);
        p32(count,0);
        if(jb(t,f32(tab)))p32(pa,0);else{p32(pa,1);p32(count,u32(count)+1);}
        const float one=1.0f,tenth=K(0x62813cu);
        if(!jb(t,f32(tab+4))){
            p32(pb,1);
            const float v=sdiv(ssub(t,f32(tab+4)),ssub(f32(tab+8),f32(tab+4)));
            pf(fa,v);
            if(gt(tenth,v)){pf(fa,tenth);p32(count,u32(count)+1);}
            else{if(gt(v,one))pf(fa,one);p32(count,u32(count)+1);}
        }else{p32(pb,0);pf(fa,0.0f);}
        if(!jb(t,f32(tab+8))){
            p32(pc,1);p32(count,u32(count)+1);pf(fa,one);
            const float v=sdiv(ssub(t,f32(tab+8)),ssub(one,f32(tab+8)));
            pf(fb,v);
            if(gt(tenth,v))pf(fb,tenth);
            else if(gt(v,one))pf(fb,one);
        }else{p32(pc,0);pf(fb,0.0f);}
        if(mode)return;
        if(gt(f32(tab+8),t)){
            p32(pa,1);p32(pb,1);
            if(gt(0.5f,f32(fa)))pf(fa,0.5f);
        }
    }
    // 46C6E0(esi = car): lights state into +C bits 9/10/12-13/16, +BD4, +BD8.
    void lights_46c6e0(std::uint32_t car){
        Frame f(*this,0x18);
        lights_46c5b0(f32(car+0xbd0),u8(car+0xc36),f(0x08),f(0x04),f(0x10),f(0x00),f(0x0c),f(0x14));   // bridge 46C6FF: movzx ecx,[esi+C36]
        std::uint32_t e=((u32(f(0x04))&1u)<<4)|(u32(f(0x08))&3u);
        e=(e<<2)|(u32(f(0x0c))&1u);
        e=(e<<1)|(u32(f(0x10))&1u);
        e<<=9;
        pf(car+0xbd4,f32(f(0x00)));
        p32(car+0xc,e|(u32(car+0xc)&0xfffec9ffu));
        pf(car+0xbd8,f32(f(0x14)));
    }
    // ---- lanes ------------------------------------------------------------------------
    // 46CDA0(kind; ecx = row, dl = lane): the first loaded road table from `lane`
    // stepping by 6 (mod 12); -1 when none.
    std::int32_t lane_46cda0(std::uint32_t kind,std::int32_t row,std::int8_t lane){
        std::int8_t al=lane;const std::int32_t start=lane;
        if(kind==0u){
            for(unsigned guard=0;;++guard){
                if(guard>64)throw std::runtime_error("46CDA0: endless lane walk (lane outside 0..11)");
                if(al<6&&u16(std::uint32_t((al+row*6)*0x1030)+0x804480u)==0x4f53u)return al;
                al=std::int8_t(al+6);if(al>=0xc)al=std::int8_t(al-12);
                if(al==start)return -1;
            }
        }
        const std::int32_t base=row*12;
        if(u16(std::uint32_t((start+base)*0x70c)+0x80a670u)==0x4f53u)return al;
        for(unsigned guard=0;;++guard){
            if(guard>64)throw std::runtime_error("46CDA0: endless lane walk (lane outside 0..11)");
            al=std::int8_t(al+6);if(al>=0xc)al=std::int8_t(al-12);
            if(al==start)return -1;
            if(u16(std::uint32_t((al+base)*0x70c)+0x80a670u)==0x4f53u)return al;
        }
    }
    static std::int32_t bucket_46efc0(std::int32_t a,std::int32_t b){
        switch(a){
        case 1:return 1;
        case 2:return b>3?2:1;
        case 3:return b<=3?1:(b>=5?3:2);
        case 4:return b<=2?1:(b>=5?4:b-1);
        case 5:return b<=2?1:b-1;
        case 6:return b;
        default:return a;
        }
    }
    // 46CD60: traffic density scale (variants 3/4 by 456D60 from 5B3DF0).
    float density_46cd60(){
        const std::uint32_t v=u32(0x780258u);
        if(v!=3u&&v!=4u)return 1.0f;
        const std::int8_t n=std::int8_t(call(0x456d60u,{}));
        if(n<=1)return 1.0f;
        return f32(0x5b3df0u+std::uint32_t(std::int32_t(n))*4u);
    }
    // 478430(eax = appear record, place): the least used lane among the record's
    // lane groups for the current traffic level.
    std::int8_t lane_478430(std::uint32_t rec,std::uint32_t pl){
        Frame f(*this,0x100);
        const std::uint32_t B=f(0x10);                 // PC frame: B = esp after push ebx/esi
        p32(B+0x20,0);p32(B+0x24,0);p32(B+0x28,0);
        for(std::uint32_t k=0;k<6;++k)if((u8(rec+0xb)>>k)&1u)p16(0x800d2cu+k*2u,0x2a);
        call(0x43e570u,{B+0x2c,pl,0xffffffffu});
        const std::int32_t v=cvtt(smul(f32(B+0x80),K(0x628088u)));
        std::int8_t n=std::int8_t(v);
        if(n<1)n=1;else if(n>6)n=6;
        p32(B+0x18,std::uint32_t(std::int32_t(n)));
        for(std::int32_t k=1;k<=6;++k){
            const std::int32_t r=bucket_46efc0(n,k)-1;
            const std::uint32_t w=B+0x20+std::uint32_t(r*2);
            p16(w,std::uint16_t(u16(w)+u16(0x800d2cu+std::uint32_t((k-1)*2))));
        }
        p32(B+0xc,0xffffffffu);p32(B+0x10,0xffffffffu);p32(B+0x14,0xffffffffu);
        std::uint32_t ecx=0xff;p32(B+0x1c,ecx);
        std::uint32_t di=0;
        for(std::int32_t bucket=0;bucket<n;++bucket){
            const std::uint32_t ptr=B+0x20+std::uint32_t(bucket*2);
            const std::uint16_t ax=u16(ptr);
            if(std::uint16_t(ecx)<ax)continue;
            if(std::uint16_t(ecx)>ax){
                ecx=0xffffffffu;p32(B+0xc,ecx);p32(B+0x10,ecx);p32(B+0x14,ecx);di=0;
                p32(B+0x1c,(ptr&0xffff0000u)|ax);
            }
            for(std::int32_t dl=0,s=1;dl<6;++dl,++s){
                if(bucket==bucket_46efc0(std::int32_t(u32(B+0x18)),s)-1){
                    p16(B+0xc+(di&0xffffu)*2u,std::uint16_t(std::int16_t(dl)));++di;
                }
            }
            ecx=u32(B+0x1c);
        }
        std::uint32_t most=0xffffffffu,top=0;
        for(std::int32_t bl=0;bl<6;++bl){
            const std::uint16_t w=u16(B+0xc+std::uint32_t(bl)*2u);
            if(w==0xffffu)break;
            const std::uint32_t cnt=u32(0x803718u+std::uint32_t(std::int32_t(std::int16_t(w)))*4u);
            if(cnt>top){most=w;top=cnt;}
        }
        std::uint32_t b8=0xffffffffu,low=0xffffffffu;                  // [B+8]
        for(std::int32_t bl=0;bl<6;++bl){
            const std::uint16_t w=u16(B+0xc+std::uint32_t(bl)*2u);
            if(w==0xffffu)break;
            const std::int32_t lane=std::int16_t(w);
            const std::uint32_t cnt=u32(0x803718u+std::uint32_t(lane)*4u);
            if(cnt>=low)continue;
            std::int32_t d=std::int32_t(std::int16_t(std::uint16_t(most)))-lane;if(d<0)d=-d;
            if(d<=1)continue;
            b8=w;low=cnt;
        }
        if(std::int16_t(b8)>=0)return std::int8_t(b8);
        std::uint32_t lowest=0xffffffffu;
        for(std::int32_t bl=0;bl<6;++bl){
            const std::int32_t w=std::int16_t(u16(B+0xc+std::uint32_t(bl)*2u));
            if(std::uint16_t(w)==0xffffu)break;
            const std::uint32_t cnt=u32(0x803718u+std::uint32_t(w)*4u);
            if(cnt<lowest){b8=std::uint32_t(w);lowest=cnt;}
        }
        return std::int8_t(b8);
    }
    // 478640(edx = player, esi = place, edi = appear record; arg row): choose the lane of a
    // new car at `place`; 1 when the lane exists.
    std::uint32_t lane_478640(std::uint32_t player_,std::uint32_t pl,std::uint32_t rec,std::int32_t row){
        p32(0x803748u,pl);                                             // bridge 478641: [803748] = esi
        const std::uint8_t blocked=0xff;
        if(u32(pl)!=0u){
            std::uint8_t cl=u8(0x800d38u);p8(pl+0xa,cl);++cl;
            p8(0x800d38u,std::int8_t(cl)>=0xc?0:cl);
            const std::uint8_t al=std::uint8_t(u8(0x803744u)+1u);p8(0x803744u,al);
            const std::uint32_t branch=u32(player_+0x60);
            auto bit=[&](std::uint32_t mask){if(u8(rec+0xb)&mask)p8(pl+0xa,blocked);};
            if(branch==0x64u){
                const std::int8_t l=i8(pl+0xa);
                if((l>=3&&l<=5)||(l>=9&&l<=0xb))p8(pl+0xa,std::uint8_t(l-3));
                if((al&0xfu)<0xeu&&i8(pl+0xa)<=2)p8(pl+0xa,std::uint8_t(i8(pl+0xa)+6));
                const std::uint32_t e=std::uint32_t(std::int32_t(i8(pl+0xa)));
                static constexpr std::uint8_t mask64[9]{0x4,0x2,0x1,0,0,0,0x20,0x10,0x8};
                if(e>8u||!mask64[e])p8(pl+0xa,blocked);else bit(mask64[e]);
            }else if(branch==0x65u){
                const std::int8_t l=i8(pl+0xa);
                if((l>=0&&l<=2)||(l>=6&&l<=8))p8(pl+0xa,std::uint8_t(l+3));
                if((al&0xfu)<0xeu&&i8(pl+0xa)<=5)p8(pl+0xa,std::uint8_t(i8(pl+0xa)+6));
                const std::uint32_t e=std::uint32_t(std::int32_t(i8(pl+0xa))-3);
                static constexpr std::uint8_t mask65[9]{0x1,0x2,0x4,0,0,0,0x8,0x10,0x20};
                if(e>8u||!mask65[e])p8(pl+0xa,blocked);else bit(mask65[e]);
            }else{
                if(u32(player_+0x5c))p8(pl+0xa,blocked);
                else if((1u<<(u8(pl+0xa)&31u))&u8(rec+0xb))p8(pl+0xa,blocked);
            }
            if(u8(pl+0xa)==blocked)return 0;
        }else p8(pl+0xa,std::uint8_t(lane_478430(rec,pl)));
        const std::int32_t l=lane_46cda0(u32(pl),std::int8_t(row),i8(pl+0xa));
        p8(pl+0xa,std::uint8_t(l));
        return std::uint8_t(l)!=blocked?1u:0u;
    }
    // ---- car set-up -------------------------------------------------------------------
    // The model data of a car model: [[[4866C0(model)]]].
    std::uint32_t model_data(std::int32_t model){return u32(u32(u32(model_4866c0(model))));}
    // 46E4B0(car): sit the car on the road: y and the +2C/+30 angles from three
    // model contact points dropped on the ground (43EB60 kind 0x400).
    std::uint32_t settle_46e4b0(std::uint32_t car){
        Frame f(*this,0xc0);const std::uint32_t A=f(0);
        const std::uint32_t md=model_data(i8(car+0x11));
        p32(A+0x18,u32(md+0x3c));p32(A+0x1c,u32(md+0x40));p32(A+0x20,u32(md+0x44));pf(A+0x18,0.0f);
        p32(A+0x24,u32(md+0x5c));p32(A+0x28,u32(md+0x60));p32(A+0x2c,u32(md+0x64));
        p32(A+0x30,u32(md+0x6c));p32(A+0x34,u32(md+0x70));p32(A+0x38,u32(md+0x74));
        push_unit();translate_40a2d0(car+0x14);
        rotate_y(word_radians(std::int32_t(i16(car+0xc2c))+std::int32_t(i16(car+0x2e))));
        std::uint32_t n=0;
        for(std::uint32_t p=A+0x18;n<3;++n,p+=0xc){
            point_40a7d0(p,p);
            call(0x43eb60u,{0x400u,p,0,0,A+0x3c});
            if(u32(A+0x3c)==1u)break;
        }
        pop();
        if(n<3)return 0;
        pf(car+0x18,smul(sadd(smul(sadd(f32(A+0x34),f32(A+0x28)),0.5f),f32(A+0x1c)),0.5f));
        sub_40efa0(A+0x40,A+0x30,A+0x24);
        blend_40f180(A+0x58,A+0x24,0.5f,A+0x30,0.5f);
        sub_40ef70(A+0x58,A+0x18);
        (void)unit_40eeb0(A+0x40);(void)unit_40eeb0(A+0x58);
        cross_40eff0(A+0x4c,A+0x58,A+0x40);
        for(std::uint32_t k=0;k<3;++k){p32(A+0x70+k*4,u32(A+0x40+k*4));p32(A+0x80+k*4,u32(A+0x4c+k*4));p32(A+0x90+k*4,u32(A+0x58+k*4));}
        euler_449640(A+0x64,A+0x70);
        const std::int32_t ax=cvtt(smul(f32(A+0x64),K(0x6282c0u)));
        const std::int32_t az=cvtt(smul(f32(A+0x6c),K(0x6282c0u)));
        p16(car+0x2c,std::uint16_t(ax));p16(car+0x30,std::uint16_t(az));
        return 1;
    }
    // 46E830(work, id, model, colour, pos, angles, speed, place, unused, cs, unused):
    // SetOthcarWork.
    void set_work_46e830(std::uint32_t w,std::uint32_t id,std::int8_t model,std::uint8_t colour,std::uint32_t pos,std::uint32_t ang,
                         float speed,std::uint32_t pl,std::uint32_t row,std::uint32_t cs,std::uint32_t a14){
        Frame f(*this,0x10);
        call(0x4874f0u,{w,a14});
        p32(w,id);p8(w+0x11,std::uint8_t(model));p8(w+0x12,colour);
        p32(w+0x31c,u32(model_4866c0(model)+0xc));
        p32(w+0x320,u32(model_4866c0(i8(w+0x11))+0x10));
        p32(w+0x324,u32(model_4866c0(i8(w+0x11))+0x14));
        p32(w+0x14,u32(pos));p32(w+0x18,u32(pos+4));
        p32(w+0xb14,0);p16(w+0xb50,0);p32(w+0xc5c,2);
        p32(w+0x1c,u32(pos+8));
        p16(w+0x2c,std::uint16_t(cvtt(smul(f32(ang),K(0x6282c0u)))));
        p16(w+0x2e,std::uint16_t(cvtt(smul(f32(ang+4),K(0x6282c0u)))));
        p16(w+0x30,std::uint16_t(cvtt(smul(f32(ang+8),K(0x6282c0u)))));
        p16(w+0x160,u16(w+0x2e));p16(w+0x162,0);
        pf(w+0x1c4,speed);
        p32(w+0x1f4,ftol(X87(speed)*X87(K(0x5a460cu))));
        // velocity: speed * rotY(a.y) rotX(a.x) (0,0,-1)
        pf(f(0),0.0f);pf(f(4),0.0f);pf(f(8),K(0x6280c4u));
        push_unit();rotate_y(f32(ang+4));rotate_x(f32(ang));vector_40a820(f(0),f(0));pop();
        scale_40f050(w+0x20,f(0),speed);
        (void)settle_46e4b0(w);
        if(u32(u32(u32(model_4866c0(i8(w+0x11))))+4)){
            Frame g(*this,0x10);
            push_unit();translate_40a2d0(w+0x14);
            rotate_y(word_radians(i16(w+0x2e)));rotate_x(word_radians(i16(w+0x2c)));rotate_z(word_radians(i16(w+0x30)));
            driving::pc_matrix_translate_vector(c.matrices,{0.0f,0.0f,12.0f});   // 40A290(0, 0, 12)
            put_vec(w+0xc70,driving::pc_matrix_translation(c.matrices));
            pop();
            call(0x43eb60u,{0x100u,w+0xc70,0,0,0});
        }
        p16(w+0x260,std::uint16_t(cs));
        p16(w+0x25e,std::uint16_t(std::uint16_t(cs)-u16(pl+8)));
        for(std::uint32_t k=0;k<0x10;k+=4)p32(w+0x5c+k,u32(pl+k));
        p8(w+0xd20,std::uint8_t(row));p8(w+0xd21,u8(pl+0xa));
        const std::uint8_t al=std::uint8_t(call(0x455c10u,{})-1u);
        p8(w+0xdc8,al);p8(w+0xdc9,al);
        p32(w+0xdcc,0);
        const std::uint32_t mdl=std::uint32_t(std::int32_t(model));       // the pushed movsx of +72
        std::uint32_t e=std::uint32_t(al)<<5;
        e|=mdl&0x1fu;e<<=2;e|=std::uint32_t(std::uint16_t(std::int16_t(std::int8_t(colour&3u))));
        e<<=4;e|=u8(w+0xd21)&0xfu;e<<=3;e|=u8(0x804450u+mdl)&7u;
        p16(w+0xdd0,std::uint16_t(e));
        call(0x4f6e40u,{w});
        call(0x4a4010u,{w});
    }
    // ---- racer bookkeeping --------------------------------------------------------------
    // 4763D0(esi = event id): release a racer's car; the racer keeps its state.
    void release_4763d0(std::uint32_t id){
        const std::uint32_t list=racers();if(!list)return;
        const std::int32_t n=i32(0x80fb04u);
        std::uint32_t r=list;std::int32_t i=0;
        for(;i<n;++i,r+=0xa0u)if(u32(r+0x4c)==id)break;
        if(i>=n||n<=0)return;
        const std::int32_t d=std::int32_t(u32(r+0x3c)-std::uint32_t(u16(player()+0x260)));
        const std::uint32_t w=work(id);
        p32(r+0x50,d>0?1u:0u);
        p32(r+0x20,u32(w+0xbd0));
        const std::uint32_t e=u32(w+0xc);
        if(e&0x40000u){p32(w+0xc3c,0x19a);p32(w+0xc,e&0xfffbffffu);}
        p32(r+0x4c,0xffffffffu);
        call(0x440200u,{id});
        p32(0x800d98u,0);p32(0x80a5a0u,0);
    }
    // Racer catch-up parameters: (a, b, c) from 80FB0C +44/+48 (zero words keep the defaults).
    void params3(std::uint32_t tab,float& a,float& b,float& c2){
        auto take=[&](std::uint32_t at,float& v){const float t=f32(at);if(!(t==0.0f))v=t;};   // ucomiss: NaN taken
        take(tab,a);take(tab+4,b);take(tab+8,c2);
    }
    void wobble_476ca0(std::uint32_t r){
        if(u32(r+0x6c)||u32(r+0x3c)>u32(0x803710u)){p8(r+0x71,u8(r+0x71)&0xfeu);return;}   // bridge 476CB1: cmp eax,[803710]
        float a=K(0x628194u),b=1.0f,c2=K(0x5b4434u);
        const std::uint32_t cfg=u32(0x80fb0cu);
        if(cfg&&u32(cfg+0x44))params3(u32(cfg+0x44),a,b,c2);
        if(u32(r+0x64)){
            const float cur=f32(r+0x28);
            if(gt(cur,0.0f)){pf(r+0x28,ssub(cur,2.0f));p8(r+0x71,u8(r+0x71)|1u);return;}
            pf(r+0x28,0.0f);
            const X87 rnd=random_46c570(0,ssub(c2,b));
            p32(r+0x64,0);
            pf(r+0x24,driving::x87_float((rnd+X87(b))*X87(K(0x628134u))));
        }else if(!jb(f32(r+0x28),f32(r+0x24))){p32(r+0x64,1);return;}
        if(jb(a,0.5f)){pf(r+0x28,sadd(smul(a,2.0f),f32(r+0x28)));p8(r+0x71,u8(r+0x71)&0xfeu);return;}
        pf(r+0x28,sadd(sdiv(2.0f,a),f32(r+0x28)));p8(r+0x71,u8(r+0x71)&0xfeu);
    }
    void wobble_476df0(std::uint32_t r){
        if(u32(r+0x6c)||u32(r+0x3c)>u32(0x803710u)){p8(r+0x71,u8(r+0x71)&0xfdu);return;}
        float a=K(0x5b0058u),b=2.0f,c2=K(0x5b4438u);
        const std::uint32_t cfg=u32(0x80fb0cu);
        if(cfg&&u32(cfg+0x44))params3(u32(cfg+0x44)+4u,a,b,c2);
        if(u32(r+0x64)){
            const float cur=f32(r+0x28);
            if(gt(cur,0.0f)){pf(r+0x28,ssub(cur,2.0f));p8(r+0x71,u8(r+0x71)|2u);return;}
            pf(r+0x28,0.0f);
            const X87 rnd=random_46c570(0,ssub(c2,b));
            p32(r+0x64,0);
            pf(r+0x24,driving::x87_float((rnd+X87(b))*X87(K(0x628134u))));
        }else if(!jb(f32(r+0x28),f32(r+0x24))){p32(r+0x64,1);return;}
        if(jb(a,0.5f)){pf(r+0x28,sadd(smul(a,2.0f),f32(r+0x28)));p8(r+0x71,u8(r+0x71)&0xfdu);return;}
        pf(r+0x28,sadd(sdiv(2.0f,a),f32(r+0x28)));p8(r+0x71,u8(r+0x71)&0xfdu);
    }
    void wobble_476f40(std::uint32_t r){
        if(u32(r+0x6c)||u32(r+0x3c)>u32(0x803710u)){p8(r+0x71,u8(r+0x71)&0xf7u);return;}
        const std::uint32_t cfg=u32(0x80fb0cu);
        const std::uint32_t tab=u32(cfg+0x48);
        if(!tab)return;
        float a=K(0x5b0058u),b=2.0f,c2=K(0x5b4438u);
        if(u32(cfg+0x44))params3(tab,a,b,c2);
        if(u32(r+0x68)){
            const float cur=f32(r+0x30);
            if(gt(cur,0.0f)){pf(r+0x30,ssub(cur,2.0f));p8(r+0x71,u8(r+0x71)|8u);return;}
            pf(r+0x30,0.0f);
            const X87 rnd=random_46c570(0,ssub(c2,b));
            p32(r+0x68,0);
            pf(r+0x24,driving::x87_float((rnd+X87(b))*X87(K(0x628134u))));
        }else if(!jb(f32(r+0x28),f32(r+0x24))){p32(r+0x68,1);return;}
        if(jb(a,0.5f)){pf(r+0x30,sadd(smul(a,2.0f),f32(r+0x30)));p8(r+0x71,u8(r+0x71)&0xf7u);return;}
        pf(r+0x30,sadd(sdiv(2.0f,a),f32(r+0x30)));p8(r+0x71,u8(r+0x71)&0xf7u);
    }
    // 477090(racer with a car): follow the car's course position and lights.
    void follow_477090(std::uint32_t r){
        const std::uint32_t w=work(u32(r+0x4c));
        if(u8(w+8)&1u){release_4763d0(u32(w));return;}
        const X87 st=callf(0x4a6cf0u,{w});
        const float sf=driving::x87_float(st);
        const std::int32_t whole=std::int32_t(ftol(st));
        const float v=sadd(ssub(sf,float(whole)),float(std::int32_t(u16(w+0x260))));
        if(!jb(v,f32(r+0x40))){pf(r+0x40,v);p32(r+0x3c,u16(w+0x260));}
        const std::uint8_t dr=std::uint8_t(u8(w+0xc36)-u8(w+0xc37));
        const std::uint32_t tab=u32(0x64def0u);
        auto clamp01=[&](float v2){if(gt(v2,1.0f))return 1.0f;if(gt(0.0f,v2))return 0.0f;return v2;};
        if(dr&&(u8(w+4)&0x20u)&&u32(0x80fb14u)){
            pf(w+0xbd0,clamp01(sadd(smul(float(std::int32_t(std::int8_t(dr))),f32(tab+0x18)),f32(w+0xbd0))));
            lights_46c6e0(w);
        }
        if(i16(w+0xb72)>0&&(u8(w+4)&0x20u)&&u32(0x80fb14u)){
            pf(w+0xbd0,clamp01(sadd(f32(tab+0x1c),f32(w+0xbd0))));
            lights_46c6e0(w);
        }
        p32(r+0x20,u32(w+0xbd0));
        std::uint8_t al=u8(r+0x71);
        const std::uint32_t e=u32(w+0xc);
        al=std::uint8_t((al&~2u)|(((e>>20)&1u)<<1));
        al=std::uint8_t((al&~1u)|((e>>18)&1u));
        al=std::uint8_t((al&~4u)|(((e>>25)&1u)<<2));
        p8(r+0x71,al);
        if(!(call(0x4957f0u,{})&0xffu))return;
        const bool on=(call(0x4960a0u,{w})&0xffu)!=0u;
        p8(r+0x71,on?std::uint8_t(u8(r+0x71)|8u):std::uint8_t(u8(r+0x71)&0xf7u));
    }
    // ---- appear (47C8A0 / 47BC90) ---------------------------------------------------------
    void appear_47c8a0(){
        if(!racers())return;
        if(u32(0x800d98u))p32(0x800d98u,u32(0x800d98u)-1u);
        if(u32(0x80a5a0u))p32(0x80a5a0u,u32(0x80a5a0u)-1u);
        const std::uint32_t pl=player();
        float scale=2.0f;
        if(u32(0x8037b8u))scale=smul(f32(u32(0x64df64u)+0x14),2.0f);
        else if(gt(K(0x5b4408u),f32(pl+0x1c4))&&call(0x44fdf0u,{})>=0x5e1u)scale=smul(f32(u32(0x64df64u)+0x10),2.0f);
        float speed=1.0f;
        const std::uint32_t t=call(0x44fdf0u,{});
        if(t<0x5e1u)speed=driving::x87_float(fild_u32(t)*X87(K(0x5b4460u)));
        const std::uint32_t cfg=u32(0x80fb0cu);
        const bool bit3=cfg&&(u8(cfg+0x30)&0x08u);
        const bool bit4=cfg&&(u8(cfg+0x30)&0x10u);
        if(i32(0x80fb04u)<=0)return;
        std::uint32_t r=racers();
        for(std::int32_t i=0;i<i32(0x80fb04u);++i,r+=0xa0u){
            if(i32(r+0x4c)>=0){follow_477090(r);continue;}
            if(!u32(r+0x54))continue;
            if(bit3)wobble_476ca0(r);
            if(bit4)wobble_476df0(r);
            wobble_476f40(r);
            const float f=smul(sadd(smul(smul(float(i32(r+0x9c)),K(0x5b4328u)),K(0x6282e8u)),1.0f),scale);
            appear_47bc90(r,pl,f,speed);
        }
    }
    // 47BC90(racer, player, rate, speed): catch-up of a racer without a car; open its car
    // (event function 0x55) when it is near enough ahead of / behind the player.
    // F(x) = the PC frame [esp+x] after the prologue.
    void appear_47bc90(std::uint32_t r,std::uint32_t pl,float rate,float speed){
        Frame F(*this,0x70);
        const std::uint32_t game=u32(0x78026cu);                        // bridge 47BC90: EAX = [78026C]
        if(game==0x10u&&std::int16_t(call(0x49b2d0u,{}))<0x3c){
            const float v=sadd(smul(smul(f32(r+0x44),rate),speed),f32(r+0x40));
            pf(F(0x24),v);pf(r+0x40,v);p32(r+0x3c,ftol(X87(v))&0xffffu);
        }else{p32(0x800d98u,0);p32(0x80a5a0u,0);}
        if(!pl||u32(r+0x6c))return;
        const std::int16_t lap=i16(0x804388u);
        std::int32_t esi=u16(pl+0x260);const std::int32_t edi=i32(r+0x3c);
        p32(F(0x14),std::uint32_t(esi));p32(F(0x24),std::uint32_t(edi));
        std::int32_t ebx;
        if(lap>0){
            const std::int32_t L=lap;
            esi=(edi/L)*L+esi%L;ebx=edi-esi;p32(F(0x14),std::uint32_t(esi));
            if(ebx>0){
                if(ebx>(L>>1)){ebx-=L;esi=edi-ebx;p32(r+0x50,0);p32(F(0x14),std::uint32_t(esi));}
                else p32(r+0x50,1);
            }else{
                if(ebx<-(L>>1)){ebx+=L;esi=edi-ebx;p32(r+0x50,1);p32(F(0x14),std::uint32_t(esi));}
                else p32(r+0x50,0);
            }
        }else ebx=edi-esi;
        p32(F(0x18),std::uint32_t(ebx));
        const std::uint32_t place=F(0x5c);
        if(ebx>0){
            auto fail_esi=[&](){catchup_476480(pl,r,float(esi));};
            auto fail=[&](){catchup_476480(pl,r,float(i32(F(0x14))));};
            if(!u32(r+0x50)){fail_esi();return;}
            if(ebx>=0x3c)return;
            const std::uint32_t id=free_event_476350();
            p32(F(0x28),id);
            if(u32(0x800d98u)||id==0x19au){fail_esi();return;}
            const std::uint32_t pp=pl+0x5c;p32(F(0x1c),pp);
            for(std::uint32_t k=0;k<0x10;k+=4)p32(place+k,u32(pp+k));
            p32(F(0x20),0);
            esi=ebx+std::int32_t(u32(place+8));
            const std::int32_t end=end_43d470(u32(place));
            if(std::int32_t(std::int16_t(esi))>end){
                p32(F(0x18),call(0x44bdd0u,{}));
                p8(F(0x13),std::uint8_t(call(0x44be00u,{})));
                p8(F(0x12),std::uint8_t(call(0x44c940u,{u32(F(0x18))})));
                if(u32(place)==1u){
                    p32(place,0);
                    if(!(i8(F(0x12))>i8(F(0x13)))&&!(i16(0x804388u)>0)){fail();return;}
                    esi+=-1-end;
                    const std::int32_t e0=end_43d470(0);
                    if(e0==0||std::int32_t(std::int16_t(esi))>e0){fail();return;}
                    p16(place+8,std::uint16_t(esi));p32(place+0xc,u32(F(0x18)));p32(F(0x18),0);
                }else{
                    p32(place,1);
                    if(i8(F(0x12))!=i8(F(0x13))){fail();return;}
                    esi+=-1-end;
                    const std::int32_t e1=end_43d470(1);
                    if(e1==0||std::int32_t(std::int16_t(esi))>e1){fail();return;}
                    p16(place+8,std::uint16_t(esi));p32(place+0xc,u32(F(0x18)));p32(F(0x18),0);
                    const std::uint32_t route=call(0x451350u,{std::uint32_t(std::int32_t(i8(F(0x13))))});
                    std::uint8_t lane;
                    if(route==1u)lane=4;
                    else if(route==0u)lane=1;
                    else{std::int32_t v=i8(0x800d38u);if(v>5)v-=6;p32(F(0x20),1);lane=std::uint8_t(v);}
                    p8(place+0xa,lane);p8(0x800d38u,lane);
                }
            }
            if(!advance_46fe70(place,i32(F(0x18)))){fail();return;}
            const std::uint32_t rec=call(0x4efb90u,{u32(F(0x1c))});
            if(!lane_478640(pl,place,rec,0)){fail();return;}
            if(game!=0x10u){
                const std::uint8_t lane=std::uint8_t((u8(r+0x77)&1u)+2u);
                p8(0x800d38u,lane);p8(place+0xa,lane);
            }else if(i8(place+0xa)>5){fail();return;}
            if(!place_pose_470240(place,0,F(0x2c),F(0x38),F(0x18))){fail();return;}
            for(std::uint32_t k=0;k<0xc;k+=4)p32(F(0x44)+k,u32(F(0x2c)+k));
            call(0x43eb60u,{0x100u,F(0x44),0,0,F(0x1c)});
            if(u32(F(0x1c))==1u){fail();return;}
            const std::uint32_t w=open_car(r,pl,u32(F(0x28)),place,F(0x2c),F(0x38),f32(F(0x18)),std::uint32_t(i32(F(0x14))+ebx),true,F);
            if(!u32(F(0x20)))return;
            const std::int8_t lane=i8(w+0x66);
            const std::uint32_t arg=std::uint32_t(std::int32_t(i8(F(0x12))));
            call(0x451140u,{arg,(lane<3||(lane>=6&&lane<9))?0u:1u});
            return;
        }
        // racer behind (or level with) the player
        auto fail_esi=[&](){catchup_4764d0(pl,r,float(esi));};
        auto fail=[&](){catchup_4764d0(pl,r,float(i32(F(0x14))));};
        if(u32(r+0x50)){fail_esi();return;}
        if(ebx<-10)return;
        const std::uint32_t id=free_event_476350();
        p32(F(0x20),id);
        if(u32(0x80a5a0u)||id==0x19au){
            if(!(i16(0x804388u)>0)){fail_esi();return;}
            const float d=smul(float(std::int32_t(u8(r+0x74))-std::int32_t(u8(pl+0xc36))),0.0f);
            const float v=ssub(ssub(ssub(ssub(float(edi),float(ebx)),10.0f),1.0f),d);
            pf(F(0x28),v);
            if(gt(v,0.0f)&&gt(f32(r+0x40),v)){pf(r+0x40,v);p32(r+0x3c,ftol(X87(v))&0xffffu);return;}
            fail_esi();return;
        }
        const std::uint32_t pp=pl+0x5c;p32(F(0x1c),pp);
        for(std::uint32_t k=0;k<0x10;k+=4)p32(place+k,u32(pp+k));
        std::uint32_t di=u32(place+8)+std::uint32_t(ebx);
        if(std::int16_t(di)<0){
            const std::uint32_t st=call(0x44bdd0u,{});p32(F(0x28),st);
            p8(F(0x12),std::uint8_t(call(0x44c940u,{st})));
            p8(F(0x13),std::uint8_t(call(0x44be00u,{})));
            if(u32(place)==1u){
                p32(place,0);
                if(i8(F(0x12))!=i8(F(0x13))){fail_esi();return;}
                const std::int32_t e0=end_43d470(0);
                if(e0==0){fail_esi();return;}
                const std::uint32_t e=std::uint32_t(e0)+1u;
                if(std::int32_t(e&0xffffu)<-std::int32_t(std::int16_t(di))){fail_esi();return;}
                p16(place+8,std::uint16_t(e+di));p32(place+0xc,u32(F(0x28)));di=0;
            }else{
                p32(place,1);
                if(!(i8(F(0x12))>i8(F(0x13)))&&!(i16(0x804388u)>0)){fail_esi();return;}
                const std::int32_t e1=end_43d470(1);
                if(e1==0){fail_esi();return;}
                const std::uint32_t e=std::uint32_t(e1)+1u;
                if(std::int32_t(e&0xffffu)<-std::int32_t(std::int16_t(di))){fail_esi();return;}
                p16(place+8,std::uint16_t(e+di));
                p32(place+0xc,call(0x44be10u,{}));
                di=0;
                const std::uint32_t route=call(0x451350u,{std::uint32_t(std::int32_t(i8(F(0x13))))});
                const std::uint8_t lane=route==1u?4:1;
                p8(place+0xa,lane);p8(0x800d38u,lane);
            }
        }else di=u32(F(0x18));
        if(!advance_46fe70(place,std::int32_t(di))){fail_esi();return;}
        const std::uint32_t rec=call(0x4efb90u,{u32(F(0x1c))});
        if(!lane_478640(pl,place,rec,0)){fail();return;}
        std::uint8_t lane=u8(place+0xa);
        if(u32(place)==1u){
            if(u32(place+4)==0x64u){lane&=3u;p8(place+0xa,lane);}
            else if(u32(place+4)==0x65u){lane=std::uint8_t((lane&3u)+3u);p8(place+0xa,lane);}
        }
        if(std::int8_t(lane)>=6)p8(place+0xa,std::uint8_t(lane-6));
        if(!place_pose_470240(place,0,F(0x2c),F(0x38),F(0x1c))){fail();return;}
        for(std::uint32_t k=0;k<0xc;k+=4)p32(F(0x50)+k,u32(F(0x2c)+k));
        call(0x43eb60u,{0x100u,F(0x50),0,0,F(0x28)});
        if(u32(F(0x28))==1u){fail();return;}
        (void)open_car(r,pl,u32(F(0x20)),place,F(0x2c),F(0x38),f32(F(0x1c)),std::uint32_t(i32(F(0x14))+ebx),false,F);
    }
    // The car-opening tail shared by both 47BC90 paths.
    std::uint32_t open_car(std::uint32_t r,std::uint32_t pl,std::uint32_t id,std::uint32_t place,std::uint32_t pos,std::uint32_t ang,float f18,
                  std::uint32_t cs,bool ahead,const Frame& F){
        p32(r+0x4c,id);
        const std::uint32_t w=work(id);
        call(0x440180u,{id,0x55u});
        set_work_46e830(w,id,i8(r+0x72),u8(r+0x73),pos,ang,f18,place,0,cs,0);
        p32(w+0xb78,u32(r+0x0));p32(w+0xb88,u32(r+0x4));p32(w+0xb7c,u32(r+0x8));p32(w+0xb80,u32(r+0xc));
        p32(w+0xb84,u32(r+0x10));p32(w+0xb8c,u32(r+0x14));p32(w+0xb90,u32(r+0x18));p32(w+0xb94,u32(r+0x1c));
        p8(w+0xc38,u8(r+0x70));p32(w+0xbd0,u32(r+0x20));
        p8(w+0xc36,u8(r+0x74));p8(w+0xc37,u8(r+0x74));
        if(ahead){
            const std::uint32_t c5c=u32(w+0xc5c);
            p32(w+0xbcc,u32(r+0x38));p32(w+0xc5c,c5c&0xffffffefu);
            p32(0x800d98u,(call(0x4b00d0u,{})&0xffu)?0u:0x5au);
        }else{
            p32(w+0xbcc,u32(r+0x38));
            p32(0x80a5a0u,0x5a);
        }
        const std::uint32_t rec=call(0x4f0030u,{w+0x5c,0});
        float spd=smul(smul(smul(float(std::int32_t(i16(rec+0x26))),K(0x5b43a4u)),f32(w+0xbc0)),f32(w+0xb78));
        pf(w+0x1c4,spd);
        if(ahead){
            const float ps=f32(pl+0x1c4);
            if(gt(ps,1.0f)&&gt(spd,ps)){spd=smul(ps,K(0x6282a0u));pf(w+0x1c4,spd);}
        }
        p32(w+0x1f4,ftol(X87(f32(w+0x1c4))*X87(K(0x5a460cu))));
        std::uint32_t e=u32(w+0xc);
        if(u32(r+0x60)){p8(w+0xc35,u8(r+0x76));e|=0x100u;}
        else{p8(w+0xc35,0xff);e&=0xfffffeffu;}
        p32(w+0xc,e);
        velocity_46d060(ang,f32(w+0x1c4),F(0x50));
        p32(w+0x20,u32(F(0x50)));p32(w+0x24,u32(F(0x54)));p32(w+0x28,u32(F(0x58)));
        std::uint32_t ax;
        if(ahead)ax=u32(F(0x24))-u32(F(0x64));
        else{
            const std::uint32_t v=call(0x4a20f0u,{u16(r+0x3c)});
            if(std::int32_t(v)>=0)ax=v;
            else if(i16(0x804388u)>0)ax=u32(F(0x24))-u32(F(0x64));
            else ax=call(0x4a2120u,{});
        }
        const std::uint32_t f4=u32(w+4)|0xa0u,f8=u32(w+8)&0xfffffffau;
        p16(w+0x25e,std::uint16_t(ax));p32(w+4,f4);p32(w+8,f8);
        const std::uint32_t fam=family_46c870(u8(w+0x11));
        call(0x5051d0u,{w,fam});
        call(0x487570u,{w});
        side_470930(w);
        lights_46c6e0(w);
        p32(w+0xc,cfg_bit_476300()?(u32(w+0xc)|0x20000u):(u32(w+0xc)&0xfffdffffu));
        p32(w+8,cfg_bit_476320()?(u32(w+8)|0x80000000u):(u32(w+8)&0x7fffffffu));
        std::uint32_t c=u32(w+0xc)&0xfdebffffu;
        c=(((c>>4)^c)&0x200000u)^c;
        p32(w+0xc,c);
        return w;
    }
    // ---- control (47EC00) ------------------------------------------------------------------
    void control_47ec00(){
        if((u8(0x79fcceu)&3u)!=2u)return;                                // event 390 (AREA) running
        p32(0x802e08u,u32(0x802e08u)+1u);
        if(!u32(0x780258u))return;
        if(call(0x4962a0u,{})&0xffu)return;
        const std::uint32_t v=u32(0x780258u);
        if(v==7u||v==8u)return;
        const std::uint32_t g=u32(0x78026cu);
        if(g==0x13u||g==3u)return;
        bool racers_=true;
        if(!u32(0x80fb14u)){
            if(!(call(0x4b00d0u,{})&0xffu))racers_=false;
            else if(!(call(0x4b00e0u,{})&0xffu))racers_=false;
        }
        if(racers_){
            if(u8(0x802e08u)&1u)appear_47cf00();
            else{ranks_477510();order_477c90();}
            leader_477e00();
        }
        traffic_47eba0();
        pass_4791b0();
        lod_479500();
        gap_479580();
        chain_46e460();
    }
    // 47CF00: catch-up on even frames (47CA40 walks out from the player's rank).
    void appear_47cf00(){
        if(u32(0x78026cu)==0x10u&&std::int16_t(call(0x49b2d0u,{}))<0x3c)p8(0x80fb09u,1);
        if(!(i16(0x804388u)>0)&&u8(0x80fb18u)){appear_47ca40();return;}
        appear_47c8a0();
    }
    float racer_rate(std::uint32_t r){return sadd(smul(smul(float(i32(r+0x9c)),K(0x5b4328u)),K(0x6282e8u)),1.0f);}
    void bulk_step(std::uint32_t r,float combined){
        const float x=sadd(smul(smul(racer_rate(r),f32(r+0x44)),combined),f32(r+0x40));
        pf(r+0x40,x);p32(r+0x3c,ftol(X87(x))&0xffffu);
    }
    void appear_47ca40(){
        if(!racers())return;
        if(u32(0x800d98u))p32(0x800d98u,u32(0x800d98u)-1u);
        if(u32(0x80a5a0u))p32(0x80a5a0u,u32(0x80a5a0u)-1u);
        const std::uint32_t pl=player();
        float scale=2.0f;
        if(u32(0x8037b8u))scale=smul(f32(u32(0x64df64u)+0x14),2.0f);
        else if(gt(K(0x5b4408u),f32(pl+0x1c4))&&call(0x44fdf0u,{})>=0x5e1u)scale=smul(f32(u32(0x64df64u)+0x10),2.0f);
        float speed=1.0f;
        const std::uint32_t t=call(0x44fdf0u,{});
        if(t<0x5e1u)speed=driving::x87_float(fild_u32(t)*X87(K(0x5b4460u)));
        const std::uint32_t cfg=u32(0x80fb0cu);
        const bool b3=cfg&&(u8(cfg+0x30)&0x08u),b4=cfg&&(u8(cfg+0x30)&0x10u);
        const std::uint32_t c36=u8(pl+0xc36);
        // forward: the ranks behind the player
        std::uint32_t list=racers();
        std::uint32_t e=u32(0x80fb1cu)+(c36+1u)*0x14u;
        for(std::int32_t i=std::int32_t(c36)+1;i<=i32(0x80fb04u);++i,e+=0x14u){
            const std::uint32_t r=list+u32(e+0x10)*0xa0u;
            if(i32(r+0x4c)>=0){follow_477090(r);list=racers();continue;}
            if(!u32(r+0x54))continue;
            if(i32(r+0x48)<-0x14&&u8(0x80fb09u)){
                const float combined=smul(speed,scale);scale=combined;
                for(;i<=i32(0x80fb04u);++i,e+=0x14u){
                    const std::uint32_t rr=list+u32(e+0x10)*0xa0u;
                    if(!u8(0x80fb08u)){if(b3){wobble_476ca0(rr);wobble_476f40(rr);}if(b4)wobble_476df0(rr);}
                    bulk_step(rr,combined);
                }
                break;
            }
            if(b3){wobble_476ca0(r);wobble_476f40(r);}
            if(b4)wobble_476df0(r);
            appear_47bc90(r,pl,smul(racer_rate(r),scale),speed);
            list=racers();
        }
        // backward: the ranks ahead of the player
        std::int32_t k=std::int32_t(c36)-1;
        e=u32(0x80fb1cu)+std::uint32_t(k)*0x14u;
        for(;k>=0;--k,e-=0x14u){
            const std::uint32_t r=list+u32(e+0x10)*0xa0u;
            if(i32(r+0x4c)>=0){follow_477090(r);list=racers();continue;}
            if(!u32(r+0x54))continue;
            if(i32(r+0x48)>0x50&&u8(0x80fb09u)){
                const float combined=smul(speed,scale);
                for(;k>=0;--k,e-=0x14u){
                    const std::uint32_t rr=list+u32(e+0x10)*0xa0u;
                    if(!u8(0x80fb08u)){if(b3){wobble_476ca0(rr);wobble_476f40(rr);}if(b4)wobble_476df0(rr);}
                    bulk_step(rr,combined);
                }
                return;
            }
            if(b3)wobble_476ca0(r);
            if(b4)wobble_476df0(r);
            wobble_476f40(r);
            appear_47bc90(r,pl,smul(racer_rate(r),scale),speed);
            list=racers();
        }
    }
    // 477E00: the leading racer reaching the branch of the player's stage.
    void leader_477e00(){
        const std::uint32_t pl=player();
        if(u32(pl+0x5c))return;
        const std::uint32_t lvl=call(0x44c940u,{call(0x450380u,{u32(pl)})});
        if(lvl==4u){if(!call(0x43f960u,{}))return;}
        else if(lvl==0xeu){if(call(0x43f960u,{}))return;}
        const std::int32_t st=std::int8_t(call(0x44be00u,{}));
        if(std::uint32_t(st)!=lvl)return;
        if(call(0x451350u,{std::uint32_t(st)})!=2u)return;
        const std::uint32_t lead=leader_477370();if(!lead)return;
        const std::uint32_t cs=std::uint32_t(std::int32_t(i16(pl+0x64)))+u32(lead+0x48);
        const std::uint32_t lim=std::uint32_t(end_43d470(0))+1u;
        if(cs<lim)return;
        if(i32(lead+0x4c)>=0)return;
        if(call(0x4b00d0u,{})&0xffu)return;
        call(0x451140u,{std::uint32_t(st),(call(0x44b750u,{1})&0x20u)?0u:1u});
    }
    // ---- traffic (47EBA0) ------------------------------------------------------------------
    void traffic_47eba0(){
        net_switch_47eb20();
        const std::uint32_t pl=player();
        if(u32(0x780258u)!=4u)traffic_479080(pl);
        else traffic_479130(pl);
        if(u32(0x780258u)==4u){
            if(call(0x4963b0u,{}))collide_4f9ea0();
        }else collide_4f9450();
        distances_46dd00();
        p32(0x804350u,u32(0x804350u)+1u);
    }
    // 47EB20: on a 456D60 change, cars with +4 bit 1 get the control 47E780.
    void net_switch_47eb20(){
        const std::uint8_t al=std::uint8_t(call(0x456d60u,{}));
        const bool same=al==u8(0x80373au);
        p8(0x800d4du,al);
        if(!same){
            for(std::uint32_t id=9;id<0x20u;++id){
                if((u8(0x79fb48u+id)&3u)!=2u)continue;
                const std::uint32_t w=work(id);
                std::uint32_t f=u32(w+4);
                if(!(f&2u))continue;
                if(f&1u){f=(f&~1u)|0x20u;p8(w+0x66,3);p32(w+4,f);}
                call(0x440bb0u,{u32(w),0x47e780u});
                p32(w+4,u32(w+4)&0xfffffffdu);
            }
        }
        p8(0x80373au,al);
    }
    // 479080(edi = player): arcade traffic: close far cars, count lanes, appear.
    // 479130 (esi = player car, variant 4): 4780D0 / 46CBA0 / 46CC10, then in the race
    // (78026C == 0x10) with no 455670 word: 478FF0 after a 4-frame countdown in [804348]
    // (rearmed while 4503C0 != 8), a spawn (478B90 / 4EFB90 / 4787E0) when it runs out;
    // always ending in 4551E0 (tail jump).
    // 4551C0(k): the LAN traffic record k (7E16F8 + k*0x85C) +4 bit 0: this machine drives it.
    std::uint32_t lan_owned_4551c0(std::uint32_t k){return u8(0x7e16fcu+k*0x85cu)&1u;}
    // 46CBA0: count the traffic cars into [800D28] (car +4 bit 5) / [800D52] (the others):
    // the ones parked (+C5C bit 14) or open and not closing (+C5C bit 0).
    void lane_totals_46cba0(){
        p8(0x800d28u,0);p8(0x800d52u,0);
        for(std::uint32_t i=u32(0x680ad4u);std::int32_t(i)<0x18;++i){
            const std::uint32_t car=u32(0x799d18u+i*0x3cu);
            const std::uint32_t f=u32(car+0xc5c);
            if(!(f&0x4000u)&&((u8(0x79fb50u+i)&3u)!=2u||(f&1u)))continue;
            const std::uint32_t at=(u8(car+4)&0x20u)?0x800d28u:0x800d52u;
            p8(at,std::uint8_t(u8(at)+1u));
        }
    }
    // 4780D0 (LAN): each open traffic car this machine does not drive is closed (+C5C bit 0,
    // owner +DC9 = 7, 4401D0). A car it drives (owner +DC8 == 455C10 - 1) stays its own while
    // near player 0 (-2..60 course units ahead, same road); otherwise its +DC9 goes to the
    // first other player car near it (455B00 of the player's +10 byte), or it closes.
    void lan_owners_4780d0(){
        const std::uint32_t n=u32(0x680ad4u);
        std::uint32_t table[6]={};                                     // [esp+0x1C]: player cars
        for(std::uint32_t i=0;std::int32_t(i)<std::int32_t(n);++i)
            table[i]=(u8(0x79fb50u+i)&3u)==2u?u32(0x799d18u+i*0x3cu):0u;
        // The close test of 4781C5 / 47825B: 0 = far, 1 = near, 2 = near but a no-op for player 0.
        auto near=[&](std::uint32_t car,std::uint32_t p,std::int32_t& d)->int{
            d=diff_46f990(car+0x5c,p+0x5c);
            const std::int32_t e=std::int32_t(std::uint16_t(end_raw(u32(p+0x5c))));
            if(d<-2||d>0x3c)return 0;
            if(!u32(p+0x5c))return 2;
            const std::int16_t cs=i16(p+0x64);
            if(cs<=0x3c||std::int32_t(cs)>=e-0x32)return 2;
            if(u32(p+0x68)!=u32(car+0x68))return 0;
            const std::uint32_t road=u32(p+0x60);
            if((road==0x64u||road==0x65u)&&u32(car+0x60)==road)return 1;
            return 0;
        };
        for(std::uint32_t i=n;std::int32_t(i)<0x18;++i){
            if(!(u8(0x79fb50u+i)&3u))continue;
            const std::uint32_t car=u32(0x799d18u+i*0x3cu);
            if(!lan_owned_4551c0(i-u32(0x680ad4u))){
                p32(car+0xc5c,u32(car+0xc5c)&~1u);p8(car+0xdc9,7);call(0x4401d0u,{i+8u});
                continue;
            }
            if(std::int32_t(u8(car+0xdc8))!=std::int32_t(call(0x455c10u,{})&0xffu)-1)continue;
            if(u8(car+0xb6c)&&i16(car+0xb64)<=0){p32(car+0xc5c,u32(car+0xc5c)|1u);continue;}
            std::int32_t d=0;
            if(!flag_44ff10()){
                const int r=near(car,table[0],d);
                if(r)continue;                                         // near player 0: kept
            }
            bool done=false;
            for(std::uint32_t k=1;std::int32_t(k)<std::int32_t(u32(0x680ad4u));++k){
                const std::uint32_t p=table[k];
                if(!p||!near(car,p,d))continue;
                const std::uint32_t b=u8(p+0x10);
                if(b==0xffu)p32(car+0xc5c,u32(car+0xc5c)|1u);
                else p8(car+0xdc9,u8(0x7df110u+b));                    // 455B00
                done=true;break;
            }
            if(!done)p32(car+0xc5c,u32(car+0xc5c)|1u);
        }
    }
    // 478FF0: the LAN traffic records this machine drives: the car leaves the parked state
    // (+C5C bit 14); a closed event is reopened (440110, function 0x5A), an open one is
    // kept unless 42E020(k, +DD0) says so; then 46EB80 and +DC9 = +DC8.
    void lan_reopen_478ff0(){
        for(std::uint32_t k=0;std::int32_t(k)<std::int32_t(u32(0x680ad8u));++k){
            if(!lan_owned_4551c0(k))continue;
            const std::uint32_t id=u32(0x680ad4u)+k+8u;
            const std::uint32_t car=work(id);
            p32(car+0xc5c,u32(car+0xc5c)&0xffffbfffu);
            if(u8(0x79fb48u+id)&3u)continue;                       // 42E020: mov eax,1
            else call(0x440110u,{id,0x5au});
            lan_reset_46eb80(car,id);
            p8(car+0xdc9,u8(car+0xdc8));
        }
    }
    // 4551E0: each traffic car follows its LAN record (7E16F8 + (event - 8)*0x85C) +4: owner =
    // bits 7..10 when bits 3..6 and 7..10 differ (also copied to +DC8 / +DC9), else bits
    // 3..6; this machine ([7DF10B]) runs 47E780, the others 4758C0 with car +4 bit 1.
    void lan_ctrl_4551e0(){
        for(std::uint32_t id=u32(0x680ad4u)+8u;id<0x20u;++id){
            const std::uint32_t car=work(id);
            const std::uint32_t eid=u32(car);
            const std::uint32_t v=u32(0x7e16f8u+(eid-8u)*0x85cu+4u);
            const bool moving=(((v>>4)^v)&0x78u)!=0u;
            const bool mine=moving?std::int32_t((v>>7)&0xfu)==std::int32_t(i8(0x7df10bu))
                                  :std::uint8_t((v>>3)&0xfu)==u8(0x7df10bu);
            call(0x440bb0u,{eid,mine?0x47e780u:0x4758c0u});
            p32(car+4,mine?(u32(car+4)&~2u):(u32(car+4)|2u));
            if(moving){const std::uint8_t o=std::uint8_t((u32(0x7e16f8u+(eid-8u)*0x85cu+4u)>>7)&0xfu);p8(car+0xdc9,o);p8(car+0xdc8,o);}
        }
    }
    void traffic_479130(std::uint32_t pl){
        lan_owners_4780d0();lane_totals_46cba0();lane_counts_46cc10();
        if(u32(0x78026cu)==0x10u&&!call(0x455670u,{})){
            if(call(0x4503c0u,{})!=8u){p8(0x804348u,4u);lan_reopen_478ff0();}
            else if(const std::int8_t n=i8(0x804348u);n>0){p8(0x804348u,std::uint8_t(n-1));lan_reopen_478ff0();}
            else{
                if(!(fixed_478b90(pl)&0xffu))spawn_4787e0(pl,call(0x4efb90u,{pl+0x5cu}));
                lan_reopen_478ff0();
            }
        }
        lan_ctrl_4551e0();
    }
    void traffic_479080(std::uint32_t pl){
        const std::uint32_t v=u32(0x780258u);
        if(!v)return;
        if(call(0x4962a0u,{})&0xffu)return;
        if(v==7u||v==8u)return;
        const std::uint32_t g=u32(0x78026cu);
        if(g==0x13u||g==3u)return;
        const std::uint32_t place=pl+0x5c;
        close_far_477f60(place);
        close_variant2_46ca60(place);
        release_478340();
        lane_counts_46cc10();
        if(u32(0x780258u)==2u){ha_pairs_46d250();ha_markers_46db60();}
        if((call(0x4957f0u,{})&0xffu)&&!call(0x496390u,{}))return;
        if(!u32(0x64e08cu))return;
        const std::uint32_t rec=call(0x4efb90u,{place});
        if(!(fixed_478b90(pl)&0xffu))spawn_4787e0(pl,rec);
    }
    // 477F60(ebx = player place): mark far traffic cars for closing (+C5C bit 0).
    void close_far_477f60(std::uint32_t place){
        const std::uint32_t end=std::uint32_t(end_43d470(u32(place)));
        for(std::uint32_t i=0;i<0x18u;++i){
            if((u8(0x79fb50u+i)&3u)!=2u)continue;
            const std::uint32_t w=u32(0x799d18u+i*0x3cu);
            const std::uint32_t f4=u32(w+4);
            if(f4&0x41u)continue;
            if(u32(w+0x324)==6u)continue;
            std::int32_t hi,lo;
            if(u32(w+0xc5c)&0x40000u){hi=0x270f;lo=-2;}
            else if(f4&0x20u){if(u32(0x80fb14u)){hi=0x46;lo=-15;}else{hi=0x3c;lo=-4;}}
            else{hi=0x3c;lo=-2;}
            if(call(0x45c440u,{})==9u||u32(0x7f9600u))lo=-10;
            const std::int32_t d=diff_46f990(w+0x5c,place);
            bool close=d<lo||d>hi;
            if(!close){
                if(!u32(place))continue;
                if(u32(place+0xc)!=u32(w+0x68))continue;
                const std::int16_t cs=i16(place+8);
                if(cs<=0x3c)continue;
                if(std::int32_t(cs)>=std::int32_t(end)-0x32)continue;
                const std::uint32_t b=u32(place+4);
                if(b==0x64u&&u32(w+0x60)==0x65u)close=true;
                else if(b==0x65u&&u32(w+0x60)==0x64u)close=true;
                if(!close)continue;
            }
            p32(w+0xc5c,u32(w+0xc5c)|1u);p16(w+0xb64,0);
            if((u8(w+4)&0x20u)&&d>0x3c){p8(0x800d41u,u8(w+0x11));p8(0x800d4cu,u8(w+0x12));}
        }
    }
    // 46CA60(place): variant 2 closing rules.
    void close_variant2_46ca60(std::uint32_t place){
        if(u32(0x780258u)!=2u)return;
        if(u32(place))return;
        if(!u32(0x64e08cu)){
            for(std::uint32_t i=1;i<0x18u;++i){
                if((u8(0x79fb50u+i)&3u)!=2u)continue;
                const std::uint32_t w=u32(0x799d18u+i*0x3cu);
                if(u8(w+4)&1u)continue;
                if(u32(w+0x5c))continue;
                if(u32(w+0x324)==6u)continue;
                const std::uint32_t c=u32(w+0xc5c);
                if(c&0x40000u)continue;
                if(c&1u)continue;
                p16(w+0xb64,0x3c);p32(w+0xc5c,c|1u);
            }
            return;
        }
        const std::uint32_t k=call(0x45c440u,{});
        if(k!=2u&&k!=6u)return;
        const std::int16_t lo=std::int16_t(call(0x45c450u,{}));
        const std::int16_t hi=std::int16_t(call(0x45c460u,{}));
        const std::uint32_t n=u32(0x680ad4u);
        if(std::int32_t(n)>=0x18)return;
        for(std::uint32_t rec=0x799d18u+n*0x3cu,fl=0x79fb50u+n;rec<0x79a2b8u;rec+=0x3cu,++fl){
            if((u8(fl)&3u)!=2u)continue;
            const std::uint32_t w=u32(rec);
            if(u8(w+4)&0x21u)continue;
            if(u32(w+0x5c))continue;
            const std::uint32_t c=u32(w+0xc5c);
            if(c&0x40000u)continue;
            if(u32(w+0x324)==6u)continue;
            const std::int16_t a=i16(w+0x18c);
            if(a<lo||a>hi)continue;
            const std::int16_t b=i16(w+0x64);
            if(b>=lo&&b<=hi)continue;
            p16(w+0xb64,0x3c);p32(w+0xc5c,c|1u);
        }
    }
    // ---- Heart Attack (variant 2): 46D250 pair records, 46DB60 marker ring ------------------
    // Pair records 800AF8 (10 x 0x38): +0 / +4 the two event ids (19A = free), +8 / +14 their
    // positions (y + [62806C]), +20 the angles of first - second (449800), +2C distance, +30 score,
    // +34 class pair (5B3DB0), +35 distance class, +36 heading class (46D180).
    std::uint32_t live_work(std::uint32_t id){return (u8(0x79fb48u+id)&3u)==2u?work(id):0u;}
    float dist_802af0(std::uint32_t a,std::uint32_t b){return f32(0x802af0u+(a*24u+b)*4u);}
    std::uint8_t dist_class(float d){
        static constexpr std::uint32_t lim[]{0x6280c0u,0x5b4338u,0x5b4334u,0x5b4330u,0x5b432cu,0x5b018cu};
        for(unsigned k=0;k<6;++k)if(K(lim[k])>=d)return std::uint8_t(6u-k);
        return 0;
    }
    // 46D180(car, angles): how square the car's yaw (+160 word) is to the pair's yaw (angles +4).
    // Its first instructions are a protected bridge, measured: ECX = (short)[car+160].
    std::uint8_t heading_46d180(std::uint32_t car,std::uint32_t ang){
        const float d=driving::x87_float(X87(std::int32_t(i16(car+0x160)))*X87(K(0x628254u))-x(ang+4));
        const float a=driving::x87_float(driving::x87_abs(X87(wrap_449470(d))));
        const X87 s=driving::x87_sin(X87(a));
        if(X87(K(0x5b4348u))>=s)return 6;
        const float sf=driving::x87_float(s);
        static constexpr std::uint32_t lim[]{0x5b4344u,0x5b4340u,0x628064u,0x5b433cu,0x6282c8u};
        for(unsigned k=0;k<5;++k)if(K(lim[k])>=sf)return std::uint8_t(5u-k);
        return 0;
    }
    // Shared tail: copy both positions, the angles of a - b, distance and its class, heading class.
    void ha_pair_fill(std::uint32_t r,std::uint32_t a,std::uint32_t b,float d){
        Frame f(*this,0x10);
        const float k=K(0x62806cu);
        for(std::uint32_t q=0;q<12;q+=4)p32(r+8+q,u32(a+0x14+q));
        pf(r+0xc,sadd(f32(r+0xc),k));
        for(std::uint32_t q=0;q<12;q+=4)p32(r+0x14+q,u32(b+0x14+q));
        pf(r+0x18,sadd(f32(r+0x18),k));
        sub_40efa0(f(0),a+0x14,b+0x14);
        angles_449800(r+0x20,f(0));
        pf(r+0x2c,d);
        p8(r+0x35,dist_class(d));
    }
    // 46D250: rescore the open pairs (state 9 only), pair new cars along the player's chain, spin 8037B0.
    void ha_pairs_46d250(){
        const std::uint32_t pl=player();
        std::uint8_t left=10;
        for(std::uint32_t r=0x800af8u;r<0x800d28u;r+=0x38u){
            if(u32(r)==0x19au&&u32(r+4)==0x19au)continue;
            const std::uint32_t a=live_work(u32(r)),b=live_work(u32(r+4));
            auto drop=[&]{if(a)p32(a+0xc5c,u32(a+0xc5c)&0xffff7fffu);if(b)p32(b+0xc5c,u32(b+0xc5c)&0xffff7fffu);p32(r,0x19au);p32(r+4,0x19au);};
            if(!a||!b){drop();continue;}
            const float d=dist_802af0(u32(a),u32(b));
            if(call(0x45c440u,{})!=9u||gt(d,K(0x628148u))){drop();continue;}
            ha_pair_fill(r,a,b,d);
            const std::uint8_t h=heading_46d180(pl,r+0x20);p8(r+0x36,h);
            const std::int32_t sum=std::int32_t(i8(r+0x34))+std::int32_t(i8(r+0x35))+std::int32_t(std::int8_t(h));
            const X87 t=X87(sum)*X87(K(0x628194u));
            const float tf=driving::x87_float(t),cur=f32(r+0x30),step=K(0x62813cu);
            if(driving::x87_abs(t-X87(cur))>X87(step))pf(r+0x30,gt(tf,cur)?sadd(cur,step):ssub(cur,step));
            else pf(r+0x30,tf);
            --left;
        }
        if(call(0x45c440u,{})!=9u)return;
        const std::int16_t lo=std::int16_t(call(0x45c450u,{})),hi=std::int16_t(call(0x45c460u,{}));
        for(std::uint32_t cur=u32(pl+0x314);left&&cur;cur=u32(cur+0x314)){
            const std::uint32_t ci=u32(cur);
            if(!gt(K(0x5b0074u),dist_802af0(u32(pl),ci)))break;
            const std::int16_t c64=i16(cur+0x64);
            if(c64<lo||c64>hi||(u32(cur+0xc5c)&0x18000u))continue;
            std::uint32_t best=0x19au;float bestd=K(0x59943cu);
            for(std::uint32_t j=9;j<0x20u;++j){
                if(ci==j||(u8(0x79fb48u+j)&3u)!=2u)continue;
                const float d=dist_802af0(ci,j);
                if(gt(d,K(0x628148u))||d>=bestd)continue;
                const std::uint32_t w=work(j);
                const std::int16_t w64=i16(w+0x64);
                if(w64<lo||w64>hi||(u8(w+0xc5e)&1u))continue;
                best=j;bestd=d;
            }
            if(best==0x19au)continue;
            const std::uint32_t pw=work(best);
            p32(pw+0xc5c,u32(pw+0xc5c)|0x8000u);p32(cur+0xc5c,u32(cur+0xc5c)|0x8000u);
            std::uint32_t r=0x800af8u;
            while(r<0x800d28u&&!(u32(r)==0x19au&&u32(r+4)==0x19au))r+=0x38u;
            if(r>=0x800d28u)continue;
            p32(r,ci);p32(r+4,u32(pw));
            ha_pair_fill(r,cur,pw,bestd);
            p8(r+0x34,u8(0x5b3db0u+u32(pw+0x320)+u32(cur+0x320)*4u));
            const std::uint8_t h=heading_46d180(pl,r+0x20);p8(r+0x36,h);
            const std::int32_t sum=std::int32_t(i8(r+0x34))+std::int32_t(i8(r+0x35))+std::int32_t(std::int8_t(h));
            pf(r+0x30,smul(float(sum),K(0x628194u)));
            --left;
        }
        const std::uint16_t w=std::uint16_t(u16(0x8037b0u)+std::uint16_t(cvtt(from_bits(0x44088889u))));
        p16(0x8037b0u,w);
        const float v=driving::x87_float((X87(std::int32_t(std::int16_t(w)))*X87(K(0x628254u)))*X87(K(0x5b434cu))+x(0x804384u));
        pf(0x804384u,v);
        pf(0x804384u,wrap_449470(v));
    }
    // 46DB10: a free marker record of the ring 8037C8 (32 x 0x5C), from the cursor 804354.
    std::uint32_t ring_46db10(){
        if(i32(0x8037b4u)>=0x20)return 0;
        std::uint32_t c=u32(0x804354u);
        for(std::uint32_t n=0;n<0x20u;++n){
            const std::uint32_t rec=0x8037c8u+c*0x5cu;++c;
            if(u32(rec)==0x19au){p32(0x804354u,c);p32(0x8037b4u,u32(0x8037b4u)+1u);return rec;}
            if(std::int32_t(c)>=0x20)c=0;
        }
        return 0;
    }
    // 46DB60 (state 11): one marker per live car (+0 id, +4 two local points 5B3DC0, +28 their world
    // points, +4C two flags, +58 age); markers whose car is gone or whose points are all off are freed.
    void ha_markers_46db60(){
        if(call(0x45c440u,{})!=0xbu)return;
        for(std::uint32_t id=9;id<0x20u;++id){
            if((u8(0x79fb48u+id)&3u)!=2u)continue;
            const std::uint32_t w=work(id);
            if(u32(w+0xc5c)&0x20000u)continue;
            const std::uint32_t rec=ring_46db10();
            if(!rec)continue;
            p32(rec,id);
            for(std::uint32_t k=0;k<2;++k){for(std::uint32_t q=0;q<12;q+=4)p32(rec+4+k*0xc+q,u32(0x5b3dc0u+k*0xc+q));p32(rec+0x4c+k*4,1);}
            p32(w+0xc5c,u32(w+0xc5c)|0x20000u);
        }
        const std::int32_t total=i32(0x8037b4u);
        std::int32_t done=0;
        for(std::uint32_t rec=0x8037c8u;rec<0x804348u;rec+=0x5cu){
            const std::uint32_t id=u32(rec);
            if(id==0x19au)continue;
            if((u8(0x79fb48u+id)&3u)!=2u){p32(0x8037b4u,u32(0x8037b4u)-1u);p32(rec,0x19au);continue;}
            push_load(work(id)+0xb0);
            std::uint32_t pts=0;
            for(std::uint32_t k=0;k<2;++k)if(u32(rec+0x4c+k*4)){point_40a7d0(rec+0x28+k*0xc,rec+4+k*0xc);++pts;}
            pop();
            if(!pts){p32(0x8037b4u,u32(0x8037b4u)-1u);p32(rec,0x19au);}
            p16(rec+0x58,std::uint16_t(u16(rec+0x58)+1u));
            if(++done>=total)break;
        }
    }
    // 478340: close the marked cars whose timer ran out; count player-side cars.
    void release_478340(){
        const std::uint32_t pl=player();
        p8(0x800d28u,0);p8(0x800d52u,0);
        for(std::uint32_t id=8,rec=0x799d18u;rec<0x79a2b8u;rec+=0x3cu,++id){
            const std::uint32_t w=u32(rec);
            if((u8(0x79fb48u+id)&3u)!=2u)continue;
            const std::uint32_t f4=u32(w+4);
            if(f4&1u)continue;
            const std::uint32_t f8=u32(w+8);
            if(f8&1u)continue;
            if(u8(w+0xc5c)&1u){
                if(i16(w+0xb64)>0)continue;
                const std::uint32_t racer=(f4>>5)&1u;
                if(racer)p32(w+8,f8&0xbfffffffu);
                if(racer&&u32(0x80fb14u))release_4763d0(u32(w));
                else call(0x4401d0u,{id});
                p32(w+0xc5c,u32(w+0xc5c)&0xffffbffeu);
                continue;
            }
            if(f4&0x20u){p8(0x800d28u,u8(0x800d28u)+1u);continue;}
            const std::uint32_t side=u8(pl+7)&3u;
            if(side==2u||side==((f4>>24)&3u))p8(0x800d52u,u8(0x800d52u)+1u);
        }
    }
    // 46CC10: per-lane traffic weights 800D2C (5B3DE4 by car class +324).
    void lane_counts_46cc10(){
        p32(0x800d2cu,0);p32(0x800d30u,0);p32(0x800d34u,0);
        for(std::uint32_t i=0;i<0x18u;++i){
            const std::uint32_t w=u32(0x799d18u+i*0x3cu);
            if((u8(0x79fb50u+i)&3u)!=2u&&!(u32(w+0xc5c)&0x4000u))continue;
            if(u32(w+0x5c))continue;
            std::int8_t lane=i8(w+0x66);
            if(lane>=6)lane=std::int8_t(lane-6);
            const std::uint32_t a=0x800d2cu+std::uint32_t(std::int32_t(lane)*2);
            p16(a,std::uint16_t(u16(a)+u16(0x5b3de4u+u32(w+0x324)*2u)));
        }
    }
    // 46DD00: the distance table 802AF0[a*24+b] between the cars 8..31 (XZ).
    void distances_46dd00(){
        for(std::uint32_t i=0;i<0x18u;++i){
            if((u8(0x79fb50u+i)&3u)!=2u)continue;
            const std::uint32_t a=u32(0x799d18u+i*0x3cu);
            const std::uint32_t f=u32(a+4);
            p32(a+4,((((f<<1)|f)<<1)&0x40000u)|(f&~0x40000u));
            for(std::uint32_t j=i;j<0x18u;++j){
                if((u8(0x79fb50u+j)&3u)!=2u)continue;
                const std::uint32_t b=u32(0x799d18u+j*0x3cu);
                if(i==j){pf(0x802af0u+(u32(a)*24u+u32(b))*4u,0.0f);continue;}
                const float dx=ssub(f32(a+0x14),f32(b+0x14)),dz=ssub(f32(a+0x1c),f32(b+0x1c));
                const float d=driving::x87_float(driving::x87_sqrt(X87(sadd(smul(dx,dx),smul(dz,dz)))));
                pf(0x802af0u+(u32(a)*24u+u32(b))*4u,d);
                pf(0x802af0u+(u32(b)*24u+u32(a))*4u,d);
            }
        }
    }
    // ---- 4791B0: overtakes between the player and the traffic --------------------------------
    void pass_4791b0(){
        const std::uint32_t pl=player();
        for(std::uint32_t i=0;std::int32_t(i)<i32(0x680ad8u);++i){
            const std::uint32_t id=call(0x55a930u,{},0,0x7f9460u)?u32(0x680ad4u)+i+8u:i+8u;
            if((u8(0x79fb48u+id)&3u)!=2u)continue;
            const std::uint32_t w=work(id);
            if(pl==w)continue;
            const std::uint32_t pw=w+0x5c,pp=pl+0x5c;
            const std::int32_t d=diff_46f990(pp,pw);
            if(!(d<1&&d>-1))p32(w+0xc5c,u32(w+0xc5c)&0xfffffff7u);
            if(u8(w+0xc5c)&2u){
                if(diff_46f990(pl+0x184,w+0x184)>=1)continue;
                if(diff_46f990(pp,pw)<1)continue;
                bool mark=true;
                if(u32(pp)&&i16(pl+0x64)>0x3c){
                    const std::int32_t end=end_43d470(u32(pw));
                    if(std::int32_t(i16(pl+0x64))<=end-5){
                        const std::uint32_t b=u32(pl+0x60);
                        if(b==0x64u){if(u32(w+0x60)!=0x64u){if(b!=0x65u)mark=false;}}
                        else if(b!=0x65u||u32(w+0x60)!=0x65u)mark=false;
                    }
                }
                if(!mark)continue;
                p32(w+0xc5c,u32(w+0xc5c)|8u);
                if(u8(pl+0x32a)<0xffu)p8(pl+0x32a,u8(pl+0x32a)+1u);
                if(i8(pl+0x32c)<0x7f)p8(pl+0x32c,u8(pl+0x32c)+1u);
                p32(w+0xc5c,u32(w+0xc5c)&0xfffffffdu);
            }else{
                if(diff_46f990(pl+0x184,w+0x184)<=-1)continue;
                if(diff_46f990(pp,pw)>-1)continue;
                if(u8(pl+0x32a))p8(pl+0x32a,u8(pl+0x32a)-1u);
                p32(w+0xc5c,u32(w+0xc5c)|2u);
            }
        }
    }
    // ---- 479500: level of detail and the draw order of the cars -------------------------------
    // 46E250(eax = a, esi = b): along-b distance of a (x87).
    X87 ahead_46e250(std::uint32_t a,std::uint32_t b){
        Frame T(*this,0x40);
        const X87 len=driving::x87_sqrt([&]{const X87 dx=x(b+0x14)-x(a+0x14),dy=x(b+0x18)-x(a+0x18),dz=x(b+0x1c)-x(a+0x1c);
            return (dz*dz+dx*dx)+dy*dy;}());
        const float d=driving::x87_float(len);
        const std::int32_t cls=std::int32_t(u16(0x5b3de4u+u32(b+0x324)*2u))-1;
        const float dd=ssub(d,smul(float(cls),K(0x6282d4u)));
        if(gt(K(0x62813cu),dd))return X87(dd);
        pf(T(0xc),0.0f);pf(T(0x10),0.0f);pf(T(0x14),K(0x6280c4u));
        push_unit();rotate_y(word_radians(i16(b+0x2e)));rotate_x(word_radians(i16(b+0x2c)));rotate_z(word_radians(i16(b+0x30)));
        vector_40a820(T(0xc),T(0xc));pop();
        sub_40efa0(T(0x18),b+0x14,a+0x14);
        scale_40f050(T(0x18),T(0x18),sdiv(1.0f,dd));
        return dot_40efd0(T(0xc),T(0x18))*X87(dd);
    }
    // 504E70(car): the traffic car's wall collision. Four probe points (body
    // corners +338 / +344 mirrored in x, raised by +D24) through the car matrix,
    // the course kind under each (43EB60, or 43F110 on the racer branch), then
    // 5041B0 CbwColiWall over the module wall work 850BA0 when all four found a
    // wall kind; the car takes the corrected position and heading.
    void wall_504e70(std::uint32_t car){
        if(!u32(car+0x5c)&&!gt(K(0x5b005cu),f32(car+0x268))&&!gt(f32(car+0x264),K(0x5b4444u)))return;
        Frame f(*this,0xa0);
        const std::uint32_t W=0x850ba0u;
        for(std::uint32_t k=0;k<0x900;k+=4)p32(W+k,0);
        p32(0x85122cu,4);
        for(std::uint32_t k=0;k<0xc;k+=4){p32(0x85123cu+k,u32(car+0x344+k));p32(0x851230u+k,u32(car+0x344+k));}
        pf(0x85123cu,ssub(0.0f,f32(0x85123cu)));
        for(std::uint32_t k=0;k<0xc;k+=4){p32(0x851254u+k,u32(car+0x338+k));p32(0x851248u+k,u32(car+0x338+k));}
        pf(0x851248u,ssub(0.0f,f32(0x851248u)));
        pf(0x851258u,0.0f);pf(0x85124cu,0.0f);
        if(gt(f32(car+0xd24),K(0x6281f0u))){
            for(std::uint32_t a:{0x851238u,0x851244u,0x851250u,0x85125cu})pf(a,sadd(f32(car+0xd24),f32(a)));
        }
        push_unit();translate_40a2d0(car+0x14);
        rotate_y(word_radians(i16(car+0x2e)));
        if(gt(f32(car+0x2c8),K(0x619a34u)))rotate_y(f32(car+0x2e8));
        // frame: +C accumulated kinds, +10 count, +14 angles, +20 four {polygon, -, kind, type}, +64 points
        p32(f(0xc),0);p32(f(0x10),0);
        for(std::uint32_t i=0;i<4;++i){
            const std::uint32_t out=f(0x64+i*0xc),rec=f(0x20+i*0x10);
            point_40a7d0(out,0x851230u+i*0xc);
            std::uint32_t r;
            if(u32(0x80fb14u)&&(u8(car+4)&0x20u)&&u32(car+0x5c)==1u&&i16(car+0x64)>=0x29&&i16(car+0x64)<=0x38)
                r=call(0x43f110u,{1u,0u,out,rec,0u,rec+8});
            else r=call(0x43eb60u,{0u,out,rec,0u,rec+8});
            p32(rec+0xc,r);
            const std::uint32_t kind=u32(rec+8);
            if(kind==1u)break;
            p32(f(0xc),u32(f(0xc))|kind);p32(f(0x10),u32(f(0x10))+1u);
        }
        const std::uint32_t acc=u32(f(0xc));
        p32(car+0x2a8,acc);
        get_40a0d0(W+0x10);
        pop();
        if(std::int32_t(u32(f(0x10)))<4||!(acc&0x10f4060u))return;
        const float scale=sdiv(K(0x62806cu),f32(0x7162c4u));
        pf(0x850bfcu,smul(f32(car+0x20),scale));pf(0x850c00u,smul(f32(car+0x24),scale));pf(0x850c04u,smul(f32(car+0x28),scale));
        call(0x5041b0u,{car,W,f(0x20)});
        p32(car+4,u32(car+4)|0x20000u);
        p32(car+0x14,u32(0x850be0u));p32(car+0x18,u32(0x850be4u));p32(car+0x1c,u32(0x850be8u));
        const auto ang=driving::pc_matrix_angles_449640(m.bytes(W+0x10,64));
        pf(f(0x14),ang[0]);pf(f(0x18),ang[1]);pf(f(0x1c),ang[2]);
        const std::uint16_t h=std::uint16_t(cvtt(smul(ssub(f32(f(0x18)),f32(car+0x2e8)),K(0x6282c0u))));
        p16(car+0x160,h);p16(car+0x2e,h);
    }
    // ==== car-car collisions 4F9450 (module 4F65A0..4F9E94) ==========================================
    // Box records: car +338 + k*1EC (k < +335): corners +0 (x0,y0,z0) / +C (x1,y1,z1) (x0 >= x1,
    // z0 >= z1), the hit table +36C + k*1EC: per other box j a 0xD4 record {+0 any, 4 x 0x34 edges
    // {+0 car, +4 hit, +8 edge id, +C s, +10 normal, +1C point, +28 corner}}.
    bool alt_mode_78024c(){const std::uint32_t v=u32(0x78024cu);return v==1u||v==3u;}
    X87 thr_6ac898(){return fild_u32(u32(alt_mode_78024c()?0x6ac8a0u:0x6ac898u))*X87(K(0x5b43a4u));}
    X87 thr_6ac89c(){return fild_u32(u32(alt_mode_78024c()?0x6ac8a4u:0x6ac89cu))*X87(K(0x5b43a4u));}
    // mass factor [6AC8E0 + i*4]: 0 for the player car, 2 for classes 0..2, else 1
    float mass_6ac8e0(std::uint32_t car){
        std::uint32_t i;
        if(u8(car+4)&1u)i=0;
        else{const std::int32_t t=i32(car+0x324);i=(t<0)?1u:(t<=2?2u:1u);}
        return f32(0x6ac8e0u+i*4u);
    }
    // +1C4 = sqrt((z*z + y*y) + x*x) of the velocity (SSE sums, x87 sqrt 449380)
    void speed_1c4(std::uint32_t car){
        const float z=f32(car+0x28),y=f32(car+0x24),x0=f32(car+0x20);
        const float s=sadd(sadd(smul(z,z),smul(y,y)),smul(x0,x0));
        pf(car+0x1c4,driving::x87_float(driving::x87_sqrt(X87(s))));
    }
    void normalize_40ef00(std::uint32_t v){put_vec(v,driving::pc_normalize_vector_40ef00(vec(v)));}
    float neg_word_radians(std::int32_t w){return driving::x87_float(-(X87(w)*X87(K(0x628254u))));}   // fmul, fchs, fstp
    void multiply_40a220(std::uint32_t a){driving::pc_matrix_multiply_current(c.matrices,m.bytes(a,64));}
    void identity_40a020(){driving::pc_matrix_identity(c.matrices);}
    // 4F6B90(eax = record): clears the any flag and the four edges' car / hit / id.
    void rec_clear_4f6b90(std::uint32_t r){
        p32(r,0);p32(r+8,0);p32(r+0xc,0xffffffffu);p32(r+4,0);
        p32(r+0x3c,0);p32(r+0x40,0xffffffffu);p32(r+0x38,0);
        p32(r+0x70,0);p32(r+0x74,0xffffffffu);p32(r+0x6c,0);
        p32(r+0xa4,0);p32(r+0xa8,0xffffffffu);p32(r+0xa0,0);
    }
    // 4F86C0(out, B, dir, mode, clamp; edi = A, esi = box): the segment A->B against the four
    // box edges (x = x0 / x1, z = z0 / z1); the nearest accepted crossing goes to out (+8 edge id,
    // +C edge parameter, +1C point).
    void edge_hit_4f86c0(std::uint32_t out,std::uint32_t A,std::uint32_t B,std::uint32_t box,std::uint32_t dir,std::uint32_t mode,std::uint32_t clamp){
        struct Cand{std::int32_t id=-1;float s=0,px=0,py=0,pz=0,dist=0;};
        std::array<Cand,4> cand{};
        // u: crossing axis offset (0 x / 8 z), plane: the edge value, ebx sense, the dir component
        // test (+1: dir.u < 0 required, -1: dir.u > 0 required), v: the other axis.
        auto block=[&](unsigned slot,std::int32_t id,std::uint32_t u,std::uint32_t plane_at,bool a_ge_b,int dir_sign){
            const std::uint32_t v=u==0?8u:0u;
            const bool ebx=a_ge_b?!jb(f32(A+u),f32(B+u)):!jb(f32(B+u),f32(A+u));
            const float d=driving::x87_float(x(B+u)-x(A+u));
            if(X87(K(0x5e0ab8u))>driving::x87_abs(X87(d)))return;
            const float plane=f32(plane_at);
            const float t=sdiv(ssub(plane,f32(A+u)),d);
            const float zero=K(0x619a34u),one=K(0x62806cu);
            if(ebx){
                if(!mode&&jb(t,zero))return;
                if(jb(one,t))return;
            }else if(mode){
                if(jb(t,zero))return;
            }else{
                if(jb(t,zero))return;
                if(jb(one,t))return;
            }
            const float dv=f32(dir+u);
            if(dir_sign>0){if(!jb(dv,0.0f))return;}      // comiss dir, 0; jae reject
            else{if(!jb(0.0f,dv))return;}                // comiss 0, dir; jae reject
            float pv=sadd(smul(ssub(one,t),f32(A+v)),smul(f32(B+v),t));
            Frame P(*this,0x10);
            if(u==0){pf(P(0),plane);pf(P(4),0.0f);pf(P(8),pv);}
            else{pf(P(0),pv);pf(P(4),0.0f);pf(P(8),plane);}
            const float dist=driving::x87_float(dist_40f140(A,P(0)));
            const float lo=u==0?f32(box+0x14):f32(box+0xc),hi=u==0?f32(box+8):f32(box);
            if(clamp){
                if(gt(lo,pv))pv=lo;
                if(gt(pv,hi))pv=hi;
            }else{
                if(gt(lo,pv))return;
                if(gt(pv,hi))return;
            }
            if(u==0)pf(P(8),pv);else pf(P(0),pv);
            Cand& c0=cand[slot];
            c0.id=id;c0.s=sdiv(ssub(hi,pv),ssub(hi,lo));
            c0.px=f32(P(0));c0.py=f32(P(4));c0.pz=f32(P(8));c0.dist=dist;
        };
        block(0,0,0,box,true,1);          // x = x0
        block(1,2,0,box+0xc,false,-1);    // x = x1
        block(2,3,8,box+8,true,1);        // z = z0
        block(3,1,8,box+0x14,false,-1);   // z = z1
        float best=K(0x59943cu);
        p32(out+8,0xffffffffu);
        for(const auto& c0:cand){
            if(c0.id==-1||!gt(best,c0.dist))continue;
            p32(out+8,std::uint32_t(c0.id));
            pf(out+0x1c,c0.px);pf(out+0x20,c0.py);pf(out+0x24,c0.pz);
            best=c0.dist;
            pf(out+0xc,c0.s);
        }
    }
    // 4F8E70(out, boxB, M1, M2; eax = boxA): the four corners of box B through M1 (start) and
    // M2 (end) against box A; per corner an edge record out+4+i*0x34 (+4 hit, +28 end point).
    std::uint32_t corners_4f8e70(std::uint32_t out,std::uint32_t boxA,std::uint32_t boxB,std::uint32_t M1,std::uint32_t M2){
        Frame L(*this,0x40);
        const std::uint32_t corner=L(0),P1=L(0x10),P2=L(0x20),dir=L(0x30);
        std::uint32_t any=0;
        for(std::uint32_t i=0;i<4;++i){
            const std::uint32_t rec=out+4+i*0x34;
            pf(corner,i<2?f32(boxB):f32(boxB+0xc));
            pf(corner+8,(i==0||i==3)?f32(boxB+8):f32(boxB+0x14));
            pf(corner+4,0.0f);
            pf(dir,i>=2?K(0x6280c4u):K(0x62806cu));
            pf(dir+4,K(0x62806cu));
            pf(dir+8,(i==0||i==3)?K(0x62806cu):K(0x6280c4u));
            load_40a170(M1);point_40a7d0(P1,corner);
            load_40a170(M2);point_40a7d0(P2,corner);
            vector_40a820(dir,dir);
            const bool inside=!gt(f32(P2),f32(boxA))&&!gt(f32(boxA+0xc),f32(P2))&&!gt(f32(P2+8),f32(boxA+8))&&!gt(f32(boxA+0x14),f32(P2+8));
            edge_hit_4f86c0(rec,P1,P2,boxA,dir,inside?1u:0u,inside?1u:0u);
            if(i32(rec+8)!=-1){
                p32(rec+0x28,u32(P2));p32(rec+0x2c,u32(P2+4));p32(rec+0x30,u32(P2+8));
                p32(rec+4,1);any=1;
            }else p32(rec+4,0);
        }
        return any;
    }
    // the car's display matrix steps of 4F9000 (local frame of car with its +C2E/+17E yaw words etc.)
    void car_into_4f9000(std::uint32_t a,std::uint32_t ia,std::uint32_t mat,std::uint32_t yaw2,std::uint32_t pitch,std::uint32_t roll,std::uint32_t yaw,std::uint32_t pos){
        identity_40a020();
        if(ia==1u){load_40a170(a+mat);invert_40a240();}
        rotate_y(ssub(0.0f,f32(a+0x2e8)));
        rotate_z(neg_word_radians(i16(a+roll)));
        rotate_x(neg_word_radians(i16(a+pitch)));
        rotate_y(neg_word_radians(std::int32_t(i16(a+yaw2))+std::int32_t(i16(a+yaw))));
        untranslate_40a310(a+pos);
    }
    void car_out_4f9000(std::uint32_t b,std::uint32_t ib,std::uint32_t mat,std::uint32_t yaw2,std::uint32_t pitch,std::uint32_t roll,std::uint32_t yaw,std::uint32_t pos){
        translate_40a2d0(b+pos);
        rotate_y(word_radians(std::int32_t(i16(b+yaw2))+std::int32_t(i16(b+yaw))));
        rotate_x(word_radians(i16(b+pitch)));
        rotate_z(word_radians(i16(b+roll)));
        rotate_y(f32(b+0x2e8));
        if(ib==1u)multiply_40a220(b+mat);
    }
    // 4F9000(out, ia, ib; edi = A, ebx = B): box ib of B (previous pose +16C.. and current pose
    // +14..) seen from box ia of A; hits get their normal / points in the world and mark
    // B +514 + ib*0x1EC + edge*4.
    std::uint32_t boxes_4f9000(std::uint32_t out,std::uint32_t A,std::uint32_t B,std::uint32_t ia,std::uint32_t ib){
        Frame L(*this,0x80);
        const std::uint32_t M1=L(0),M2=L(0x40);
        car_into_4f9000(A,ia,0xcd0,0xc2e,0x17c,0x180,0x17e,0x16c);
        car_out_4f9000(B,ib,0xcd0,0xc2e,0x17c,0x180,0x17e,0x16c);
        get_40a0d0(M1);
        car_into_4f9000(A,ia,0xc90,0xc2c,0x2c,0x30,0x2e,0x14);
        car_out_4f9000(B,ib,0xc90,0xc2c,0x2c,0x30,0x2e,0x14);
        get_40a0d0(M2);
        const std::uint32_t r=corners_4f8e70(out,A+0x338+ia*0x1ecu,B+0x338+ib*0x1ecu,M1,M2);
        for(std::uint32_t i=0;i<4;++i){
            const std::uint32_t e=out+4+i*0x34;
            if(!u32(e+4))continue;
            p32(out,1);
            identity_40a020();
            translate_40a2d0(A+0x14);
            rotate_y(word_radians(std::int32_t(i16(A+0xc2c))+std::int32_t(i16(A+0x2e))));
            rotate_x(word_radians(i16(A+0x2c)));
            rotate_z(word_radians(i16(A+0x30)));
            rotate_y(f32(A+0x2e8));
            if(ia==1u)multiply_40a220(A+0xc90);
            point_40a7d0(e+0x1c,e+0x1c);
            point_40a7d0(e+0x28,e+0x28);
            float nx=0.0f,nz=0.0f;
            switch(i32(e+8)){
            case 0:nz=K(0x6280c4u);break;
            case 1:nx=K(0x6280c4u);break;
            case 2:nz=K(0x62806cu);break;
            case 3:nx=K(0x62806cu);break;
            default:break;
            }
            pf(e+0x10,nx);pf(e+0x14,0.0f);pf(e+0x18,nz);
            vector_40a820(e+0x10,e+0x10);
            p32(e,B);
            p32(B+0x514+(ib*0x7bu+i)*4u,1);
        }
        return r;
    }
    // 4F6AC0(eax = record, edi = V, [esp+4] = W): the longest hit (P - P2) of the record's four
    // edges into V and its normal into W; 1 when any edge hit.
    std::uint32_t longest_4f6ac0(std::uint32_t rec,std::uint32_t V,std::uint32_t W){
        Frame L(*this,0x10);
        const std::uint32_t T=L(0);
        pf(V+8,0.0f);pf(V+4,0.0f);pf(V,0.0f);
        std::uint32_t any=0;
        for(std::uint32_t i=0;i<4;++i){
            const std::uint32_t e=rec+4+i*0x34;
            if(!u32(e+4))continue;
            if(!any){sub_40efa0(V,e+0x1c,e+0x28);any=1;}
            else{
                sub_40efa0(T,e+0x1c,e+0x28);
                const float lt=driving::x87_float(len_40f0e0(T));
                if(!(X87(lt)>len_40f0e0(V)))continue;
                p32(V,u32(T));p32(V+4,u32(T+4));p32(V+8,u32(T+8));
            }
            p32(W,u32(e+0x10));p32(W+4,u32(e+0x14));p32(W+8,u32(e+0x18));
        }
        return any;
    }
    // ground snap of a pushed car (43EB60 kind 0x100 at its position) unless +4 bit 1
    void snap_car(std::uint32_t car){
        if(u8(car+4)&2u)return;
        Frame L(*this,0x10);
        const std::uint32_t Q=L(0),kind=L(0xc);
        p32(Q,u32(car+0x14));p32(Q+4,u32(car+0x18));p32(Q+8,u32(car+0x1c));
        call(0x43eb60u,{0x100u,Q,0u,0u,kind});
        if(u32(kind)==1u)return;
        p32(car+0x14,u32(Q));p32(car+0x18,u32(Q+4));p32(car+0x1c,u32(Q+8));
    }
    // 4F8490(A, B, recA, recB, ratio): separate the two cars along the longest overlap.
    std::uint32_t separate_4f8490(std::uint32_t A,std::uint32_t B,std::uint32_t recA,std::uint32_t recB,float ratio){
        Frame L(*this,0x50);
        const std::uint32_t V1=L(0x28-0x8),V2=L(0x34-0x8),W1=L(0x40-0x8),W2=L(0x4c-0x8),T=L(0x1c-0x8);
        const std::uint32_t r1=longest_4f6ac0(recA,V1,W1);
        const std::uint32_t r2=longest_4f6ac0(recB,V2,W2);
        std::uint32_t sel;
        if(!r1){
            if(!r2)return 0;
            sel=1;scale_40f050(V2,V2,K(0x6280c4u));
        }else if(!r2)sel=0;
        else{
            const float l1=driving::x87_float(len_40f0e0(V1));
            if(X87(l1)>len_40f0e0(V2))sel=0;
            else{sel=1;scale_40f050(V2,V2,K(0x6280c4u));}
        }
        const std::uint32_t V=sel?V2:V1,W=sel?W2:W1;
        scale_40f050(T,V,ssub(K(0x62806cu),ratio));
        add_40ef10(B+0x14,T);
        project_40f1f0(T,T,W);
        sub_40ef70(B+0x14,T);
        snap_car(B);
        call(0x4a2650u,{B});
        scale_40f050(T,V,ratio);
        sub_40ef70(A+0x14,T);
        project_40f1f0(T,T,W);
        add_40ef10(A+0x14,T);
        snap_car(A);
        call(0x4a2650u,{A});
        return 1;
    }
    // 4F8210(esi = A, edi = B): velocity exchange of two racer-player contacts.
    void exchange_4f8210(std::uint32_t A,std::uint32_t B){
        Frame L(*this,0x30);
        const std::uint32_t J=L(4),dv=L(0x10),dp=L(0x1c);
        pf(dv,ssub(f32(A+0x20),f32(B+0x20)));pf(dv+4,ssub(f32(A+0x24),f32(B+0x24)));pf(dv+8,ssub(f32(A+0x28),f32(B+0x28)));
        sub_40efa0(dp,B+0x14,A+0x14);
        project_40f1f0(J,dv,dp);
        const X87 lj=len_40f0e0(J);
        float e;
        if(lj>thr_6ac89c())e=K(0x5b0068u);
        else if(lj>thr_6ac898())e=K(0x628064u);
        else e=K(0x5c403cu);
        if((u32(B+4)&0x400000u)&&u32(A+0xda0)==u32(B))return;
        const float a=mass_6ac8e0(A),b=mass_6ac8e0(B);
        const float jx=f32(J),jy=f32(J+4),jz=f32(J+8);
        pf(A+0x20,ssub(f32(A+0x20),smul(smul(jx,a),e)));
        pf(A+0x24,ssub(f32(A+0x24),smul(smul(jy,a),e)));
        pf(A+0x28,ssub(f32(A+0x28),smul(smul(jz,a),e)));
        pf(B+0x20,sadd(smul(smul(jx,b),e),f32(B+0x20)));
        pf(B+0x24,sadd(smul(smul(jy,b),e),f32(B+0x24)));
        pf(B+0x28,sadd(smul(smul(jz,b),e),f32(B+0x28)));
        p32(A+4,u32(A+4)|0x4000000u);p32(B+4,u32(B+4)|0x4000000u);
        speed_1c4(A);speed_1c4(B);
    }
    // 4F6820(esi = car): the player car's skid boost on a contact (+2F0 contact kind 7).
    void boost_4f6820(std::uint32_t car){
        if(!(u8(car+4)&1u))return;
        const std::uint32_t f=u32(car+0x2f0);
        if(!(f&1u))return;
        if((f&0x7cu)!=0x1cu)return;
        if(!jb(K(0x5afd28u),f32(car+0x1c4)))return;      // comiss K, +1C4; jae return
        Frame L(*this,0x10);
        const std::uint32_t V=L(0);
        p32(V,u32(car+0x20));p32(V+4,u32(car+0x24));p32(V+8,u32(car+0x28));
        normalize_40ef00(V);
        scale_40f050(V,V,from_bits(0x3fb12fe9u));
        const float vx=f32(V),vy=f32(V+4),vz=f32(V+8);
        pf(car+0x28,vz);pf(car+0x24,vy);pf(car+0x20,vx);
        pf(car+0x1c4,driving::x87_float(driving::x87_sqrt(X87(sadd(sadd(smul(vz,vz),smul(vy,vy)),smul(vx,vx))))));
    }
    // 4F7BF0(eax = A, edi = B): the general velocity exchange (A against B).
    void exchange_4f7bf0(std::uint32_t A,std::uint32_t B){
        Frame L(*this,0x58);
        const std::uint32_t Ev=L(0x30),N=L(0x30),dp=L(0x3c),J=L(0x48),Av=L(0x18),Bv=L(0x24);
        pf(Ev,ssub(f32(A+0x20),f32(B+0x20)));pf(Ev+4,ssub(f32(A+0x24),f32(B+0x24)));pf(Ev+8,ssub(f32(A+0x28),f32(B+0x28)));
        sub_40efa0(dp,B+0x14,A+0x14);
        project_40f1f0(J,Ev,dp);
        const float lj=driving::x87_float(len_40f0e0(J));
        const bool alt=alt_mode_78024c();
        float e;
        if(X87(lj)>thr_6ac89c())e=K(alt?0x6ac8bcu:0x6ac8b0u);
        else if(X87(lj)>thr_6ac898())e=K(alt?0x6ac8b8u:0x6ac8acu);
        else e=K(alt?0x6ac8b4u:0x6ac8a8u);
        if((u8(B+4)&0x20u)&&(u8(A+4)&1u)&&u32(0x80fb14u)&&gt(K(0x6281f0u),f32(B+0x2c8))){
            std::uint32_t kind;
            if(gt(lj,K(0x5c4044u)))kind=3;
            else if(gt(lj,K(0x6280e4u)))kind=4;
            else kind=5;
            call(0x46c780u,{B,kind,0x3f800000u});
        }
        const float a=mass_6ac8e0(A),b=mass_6ac8e0(B);
        for(std::uint32_t k=0;k<0xc;k+=4){p32(Av+k,u32(A+0x20+k));p32(Bv+k,u32(B+0x20+k));}
        auto push_car=[&](std::uint32_t S,std::uint32_t O,std::uint32_t Sv,std::uint32_t Ov,float ms){
            if(!(X87(lj)>thr_6ac898())){    // complex: along the unit relative velocity
                sub_40efa0(N,Ov,Sv);
                normalize_40ef00(N);
                float s;
                const X87 t=thr_6ac898();
                if(t>X87(f32(S+0x1c4)))s=f32(S+0x1c4);
                else s=driving::x87_float(thr_6ac898());
                scale_40f050(N,N,s);
                pf(S+0x20,sadd(f32(N),f32(S+0x20)));
                pf(S+0x24,sadd(smul(smul(ssub(f32(Ov+4),f32(Sv+4)),ms),e),f32(S+0x24)));
                pf(S+0x28,sadd(f32(S+0x28),f32(N+8)));
            }else{
                pf(S+0x20,sadd(smul(smul(ssub(f32(Ov),f32(Sv)),ms),e),f32(S+0x20)));
                pf(S+0x24,sadd(smul(smul(ssub(f32(Ov+4),f32(Sv+4)),ms),e),f32(S+0x24)));
                pf(S+0x28,sadd(smul(smul(ssub(f32(Ov+8),f32(Sv+8)),ms),e),f32(S+0x28)));
            }
            p32(S+4,u32(S+4)|0x4000000u);
        };
        if(!(u32(A+4)&0x400000u))push_car(A,B,Av,Bv,a);
        speed_1c4(A);boost_4f6820(A);
        if(!(u32(B+4)&0x400000u))push_car(B,A,Bv,Av,b);
        speed_1c4(B);boost_4f6820(B);
    }
    // 4F65A0(byte): [5E0990 + movsx(byte)*12]
    std::uint8_t class_4f65a0(std::uint32_t v){return u8(0x5e0990u+std::uint32_t(std::int32_t(std::int8_t(std::uint8_t(v))))*12u);}
    // 4F65B0(eax = i): [6AC898 / 6AC8A0 + i*4]
    std::uint32_t base_4f65b0(std::uint32_t i){return u32((alt_mode_78024c()?0x6ac8a0u:0x6ac898u)+i*4u);}
    bool net_on(){return call(0x55a930u,{},0,0x7f9460u)!=0u;}
    // 4F65D0(eax = car, edi = A, kind): the impact SE of car's model family, once per 60 frames of A.
    void impact_se_4f65d0(std::uint32_t car,std::uint32_t A,std::uint32_t kind){
        const std::int8_t mdl=std::int8_t(u8(car+0x11));
        std::uint32_t s;
        if(mdl==0x1f)s=2;
        else if(mdl>0x26&&mdl<=0x2b)s=1;
        else s=0;
        if(net_on()){const std::uint32_t t=u32(A+0xd14);if(t==3u||t==2u)return;}
        if(std::int8_t(u8(A+0xd22))>0)return;
        call(0x424940u,{u32(0x5e0a78u+(s+kind*4u)*4u)});
        p8(A+0xd22,0x3c);
    }
    // 4F67A0(esi = car, kind): the car's own impact SE (by +31C) once per 60 frames.
    void impact_se_4f67a0(std::uint32_t car,std::uint32_t kind){
        if(std::int8_t(u8(car+0xd22))>0)return;
        if(gt(K(0x628324u),f32(car+0x300)))return;
        const std::uint32_t t=u32(car+0x31c);
        const std::uint32_t s=t==1u?2u:(t==2u?1u:0u);
        if(net_on()){const std::uint32_t d=u32(car+0xd14);if(d==3u||d==2u)return;}
        call(0x424940u,{u32(0x5e0a78u+(s+kind*4u)*4u)});
        p8(car+0xd22,0x3c);
    }
    // 4F69F0(esi = car): re-place the car on the course after a push; +B4E on a hit of edge 3.
    void replace_4f69f0(std::uint32_t car){
        if(u8(car+4)&2u)return;
        const std::uint32_t old=u32(car+0x5c);
        const std::uint32_t r=call(0x43eb60u,{0x100u,car+0x14,car+0x230,0u,0u});
        const std::uint32_t poly=u32(car+0x230);
        const std::uint16_t cs=poly==0xffffffffu?0u:u16(u32(0x780228u+r*4u)+poly*2u);
        p32(car+0x5c,r);
        p16(car+0x64,cs);
        if(r!=old)p32(car+0x60,r?0x64u:0u);
        if(u16(car+0xb4e))return;
        const std::uint32_t n=u8(car+0x335);
        for(std::uint32_t k=0;k<n;++k)for(std::uint32_t j=0;j<4;++j){
            const std::uint32_t e=car+0x3ac+k*0x1ecu+j*0x34u;
            if(u32(e-4)&&u32(e)==3u)p16(car+0xb4e,0x3c);
            if(u32(e+0xd0)&&u32(e+0xd4)==3u)p16(car+0xb4e,0x3c);
        }
    }
    // 4F6640(esi = car; ebx = X, arg0 = Y, arg1 = Z): the side / direction bytes of the car's
    // first hit (box 0, records 0..1).
    void side_4f6640(std::uint32_t car,std::uint32_t X,std::uint32_t Y,std::uint32_t Z){
        auto sgn=[](bool b){return b?1u:0xffu;};
        if(u32(car+0x514)||u32(car+0x518)){p8(Y,0xff);p8(X,0xff);p8(Z,sgn(u32(car+0x514)!=0u));return;}
        if(u32(car+0x51c)||u32(car+0x520)){p8(Y,1);p8(X,1);p8(Z,sgn(u32(car+0x520)!=0u));return;}
        for(std::uint32_t r=0;r<2;++r)for(std::uint32_t j=0;j<4;++j){
            const std::uint32_t at=car+r*0xd4u+j*0x34u;
            const std::uint32_t id=u32(at+0x378);
            if(id==0xffffffffu)continue;
            const bool small=!jb(K(0x628064u),f32(at+0x37c));   // 0.5 >= s
            if(id==0){p8(Y,0xff);p8(X,0xff);p8(Z,small?1u:0xffu);return;}
            if(id==2){p8(Y,1);p8(X,1);p8(Z,small?1u:0xffu);return;}
            p8(Y,0);
            p8(X,small?0xffu:1u);
            p8(Z,sgn(id==3u));
            return;
        }
        p8(Y,0);p8(X,1);p8(Z,1);
    }
    // 4F6960(eax = A, B, out1, out2): the heading of A's velocity (y left as the stale local:
    // 0 here) against the direction to B.
    void heading_4f6960(std::uint32_t A,std::uint32_t B,std::uint32_t o1,std::uint32_t o2){
        Frame L(*this,0x20);
        const std::uint32_t V=L(0),D=L(0x10);
        p32(V,u32(A+0x20));p32(V+8,u32(A+0x28));      // V.y: the uninitialised local (zeroed frame)
        sub_40efa0(D,B+0x14,A+0x14);
        (void)unit_40eeb0(V);(void)unit_40eeb0(D);
        const float vx=f32(V),vz=f32(V+8),dx=f32(D),dz=f32(D+8);
        pf(o1,sadd(smul(dz,vz),smul(dx,vx)));
        pf(o2,ssub(smul(dx,vz),smul(dz,vx)));
    }
    // 4F68F0(eax = B, ecx = A): |projection of (A.v - B.v) on (B.pos - A.pos)|.
    X87 closing_4f68f0(std::uint32_t B,std::uint32_t A){
        Frame L(*this,0x30);
        const std::uint32_t V=L(0),D=L(0x10),W=L(0x20);
        pf(V,ssub(f32(A+0x20),f32(B+0x20)));pf(V+4,ssub(f32(A+0x24),f32(B+0x24)));pf(V+8,ssub(f32(A+0x28),f32(B+0x28)));
        sub_40efa0(D,B+0x14,A+0x14);
        project_40f1f0(W,V,D);
        return len_40f0e0(W);
    }
    // 4F6FE0(eax = A, B): the contact reaction of A (player / racer) against B.
    void react_4f6fe0(std::uint32_t A,std::uint32_t B){
        Frame F(*this,0x20);
        const std::uint32_t X=F(8),kind=F(9),Z=F(0xa),Y=F(0xb),flag=F(0xc),o2=F(0x10);
        p8(kind,0);p32(flag,0);
        if(i16(A+0x1030)>0){
            p32(flag,1);
            if(call(0x45c440u,{})!=0x14u){
                if(!net_on())return;
                if(!g_46c3c0())return;
            }
        }
        const bool same=u32(A+0xda0)==u32(B)&&std::int8_t(u8(A+0xda4))>0;
        const float k29e0=K(0x5a29e0u);
        bool tail=false;   // L4F7237
        if(u32(B+0xb58)==0x12u||gt(f32(B+0x2c8),k29e0)||gt(f32(B+0x2f8),k29e0))tail=true;
        if(!tail){
            p32(A+4,u32(A+4)|0x400000u);p32(A+0xda0,u32(B));p8(A+0xda4,0x78);
            if(same)return;
            bool skip=false;   // -> L4F71DF
            if(u32(0x780258u)==2u&&call(0x45c440u,{})==0x14u)skip=true;
            else if(net_on()&&g_46c3c0())skip=true;
            else if(u32(flag))skip=true;
            const std::uint32_t nB=u8(B+0x335);
            if(!skip){
                if(u32(A+0x514))return;
                if(u32(A+0x520))return;
                for(std::uint32_t j=0;j<4;++j)for(std::uint32_t k=0;k<nB;++k){
                    const std::uint32_t id=u32(A+0x378+j*0x34u+k*0xd4u);
                    if(id==0||id==2)return;
                    if((j==1||j==2)&&id==1)return;
                }
                for(std::uint32_t k=0;k<nB;++k)for(std::uint32_t j=0;j<4;++j){
                    const std::uint32_t e=B+0x378+k*0x1ecu+j*0x34u;
                    const std::uint32_t id=u32(e);
                    if((id==0||id==2)&&!jb(f32(e+4),K(0x628128u)))return;
                    if((j==1||j==2)&&id==1)return;
                }
            }
            if(u32(B+0x320)==1u){
                for(std::uint32_t j=0;j<4;++j){
                    if(u32(A+0x378+j*0x34u)==1u)return;
                    const std::uint32_t id=u32(B+0x378+j*0x34u);
                    if(id==3||id==0||id==2)return;
                }
            }
        }
        // L4F7237
        p32(A+0xda0,0x19a);p8(A+0xda4,0);p32(A+4,u32(A+4)&0xffbfffffu);
        if(gt(f32(A+0x2c8),k29e0))return;
        if(gt(f32(A+0x2f8),k29e0))return;
        {const std::uint32_t n=u8(A+0x335);
            for(std::uint32_t k=0;k<n;++k)for(std::uint32_t j=0;j<0xd0;j+=0x34)for(std::uint32_t i=0;i<u8(A+0x335);++i)
                if(u32(A+0x378+k*0x1ecu+j+i*0xd4u)==3u)return;}
        const std::uint32_t b4=u32(B+4);
        bool se=false;   // L4F7322
        if((b4&1u)&&(b4&2u))se=true;
        else if(gt(f32(A+0xdb4),K(0x628064u))){
            if(!(u32(0x780258u)==2u&&call(0x45c440u,{})==0x14u)){
                if(!net_on()||!g_46c3c0())se=true;
            }
        }
        if(se){impact_se_4f65d0(B,A,0);return;}
        // L4F7336
        bool to7368=false;
        if(u32(0x780258u)==2u&&call(0x45c440u,{})==0x14u)to7368=true;
        else if(net_on()&&g_46c3c0())to7368=true;
        bool hit=false;   // L4F73EF
        if(to7368){
            bool to738e=false;
            if(net_on()&&g_46c3c0())to738e=true;
            else{const std::int32_t t=i32(B+0x324);to738e=!(t>=0&&t<=2);}
            if(to738e){
                if(net_on()&&g_46c3d0()){p32(B+0xc5c,u32(B+0xc5c)|1u);p16(B+0xb64,0x5a);pf(B+0x1c4,smul(f32(A+0x1c4),K(0x5b43ecu)));}
                else{p32(B+0xc5c,u32(B+0xc5c)|1u);p16(B+0xb64,0x3c);pf(B+0x1c4,smul(f32(A+0x1c4),K(0x6280b0u)));}
                hit=true;
            }
        }
        if(!hit){
            // L4F7401
            if((u8(A+4)&1u)&&u32(flag))return;
            if(gt(f32(B+0x2c8),k29e0)||gt(f32(B+0x2f8),k29e0)||gt(f32(B+0x2c8),K(0x619a34u)))hit=true;
        }
        if(hit){hit_476260(A,B);return;}
        side_4f6640(A,X,Y,Z);
        std::uint32_t side=u8(X)!=0xffu?1u:0u;
        heading_4f6960(A,B,flag,o2);
        if(gt(0.0f,f32(flag)))pf(flag,0.0f);
        else pf(flag,driving::x87_float(closing_4f68f0(B,A)));
        if(u32(B+4)&0x80000u){
            const X87 t=fild_u32(base_4f65b0(0))*X87(K(0x5b43a4u));
            if(!(t<X87(f32(flag))))pf(flag,driving::x87_float(fild_u32(base_4f65b0(0)+1u)*X87(K(0x5b43a4u))));
        }
        std::uint8_t cl;
        const float lim=K(0x5b440cu);
        if(gt(f32(flag),lim))cl=1;
        else if(u32(B+0x320)!=3u&&!jb(f32(A+0x1c4),lim))cl=1;
        else cl=u32(B+0xb58)==0x12u?3:2;
        p8(X,cl);
        std::uint8_t bl;
        if(cl==1||cl==3){
            if(cl==3)side=jb(f32(B+0x26c),0.0f)?1u:0u;
            call(0x4a6ea0u,{A,7u,side,0u});p8(kind,7);bl=0;
            p32(A+4,u32(A+4)|0x80000u);
        }else bl=0;
        if(cl==1||cl==2){
            if(!jb(f32(flag),K(0x5a923cu))){
                const std::uint32_t code=hit_kind_475f70(A,B);
                const std::uint32_t v=u32(0x780258u);
                if(v!=3u&&v!=4u)hit_476260(A,B);
                p32(A+0xda8,u32(B));p32(A+0xdac,code);
            }
        }
        const std::uint8_t al=std::uint8_t(ftol((X87(K(0x62806cu))-X87(f32(A+0xdb4)))*X87(std::int32_t(bl))));
        if(u8(A+0x282)<al)p8(A+0x282,al);
        impact_se_4f65d0(B,A,3);
        (void)call(0x43d470u,{u32(A+0x5c)});   // the rest only feeds 49A650 (an empty function)
    }
    // 4F76E0(eax = A, B): the contact reaction of a traffic car A against B (bridge 4F76EC:
    // EAX = [A+2F0], measured).
    void react_4f76e0(std::uint32_t A,std::uint32_t B){
        Frame H(*this,0x40);
        const std::uint32_t Z=H(0x11),X=H(0x12),Y=H(0x13);
        {const std::uint32_t f=u32(A+0x2f0);
            if((f&2u)&&((f&0x7cu)==0x18u))p32(A+0xc60,u32(B));}
        const bool flagE=i16(A+0xb4c)>0&&u32(A+0xae8)!=0x19au;
        const float k29e0=K(0x5a29e0u);
        if(!(u32(A+0xb5c)==0x12u||gt(f32(A+0x2c8),k29e0)||gt(f32(A+0x2f8),k29e0)||(u32(B+4)&0x80000u))){
            p32(A+4,u32(A+4)|0x400000u);
            const std::uint32_t nA=u8(A+0x335),nB=u8(B+0x335);
            for(std::uint32_t k=0;k<nA;++k)for(std::uint32_t j=0;j<0xd0;j+=0x34)for(std::uint32_t i=0;i<nB;++i){
                const std::uint32_t id=u32(A+0x378+k*0x1ecu+j+i*0xd4u);
                if(id==0||id==2)return;
                if((j==0x34||j==0x68)&&id==1)return;
            }
            const std::uint32_t nB2=u8(B+0x335);
            for(std::uint32_t k=0;k<nB2;++k)for(std::uint32_t j=0;j<0xd0;j+=0x34)for(std::uint32_t i=0;i<u8(A+0x335);++i){
                const std::uint32_t id=u32(B+0x378+k*0x1ecu+j+i*0xd4u);
                if(id==0||id==2)return;
                if((j==0x34||j==0x68)&&id==1)return;
            }
        }
        // L4F7882
        const std::uint32_t a4=u32(A+4)&0xffbfffffu;
        p32(A+4,a4);
        if(a4&2u){impact_se_4f67a0(A,0);return;}
        side_4f6640(A,X,Y,Z);
        std::uint32_t flag14=u8(X)!=0xffu?1u:0u;
        X87 L;
        bool computed;
        {const std::uint32_t r=u32(B+0x5c);
            if(r==1u)computed=false;
            else if(r==0u&&std::int32_t(i16(B+0x64))+0x28>std::int32_t(std::uint16_t(call(0x43d470u,{0u}))))computed=false;
            else computed=true;}
        if(computed){
            Frame G(*this,0x30);
            const std::uint32_t dv=G(0),dp=G(0x10),J=G(0x20);
            pf(dv,ssub(f32(A+0x20),f32(B+0x20)));pf(dv+4,ssub(f32(A+0x24),f32(B+0x24)));pf(dv+8,ssub(f32(A+0x28),f32(B+0x28)));
            sub_40efa0(dp,B+0x14,A+0x14);
            project_40f1f0(J,dv,dp);
            L=len_40f0e0(J);
        }else L=fild_u32(u32(alt_mode_78024c()?0x6ac8a0u:0x6ac898u)-1u)*X87(K(0x5b43a4u));
        const std::uint32_t cls=(i32(A+0x324)>=0&&i32(A+0x324)<=2)?1u:0u;
        const std::uint32_t af=u32(A+4);
        if(!(af&1u)){impact_se_4f67a0(A,0);return;}
        std::uint32_t edi,ebp;std::uint8_t bl;
        if(L>thr_6ac89c()){edi=u32(0x6ac8d0u+cls*4u);bl=0x40;p8(Z,0x40);ebp=3;}
        else if(L>thr_6ac898()){
            bool far=true;
            if(!(u8(B+4)&1u)){
                const std::uint32_t idx=u32(A)-8u;
                if(jb(f32(0x802e10u+idx*0x60u),K(0x628148u)))far=false;
            }
            if(!far){edi=u32(0x6ac8d0u+cls*4u);bl=0x40;ebp=3;p8(Z,0x40);}
            else{edi=u32(0x6ac8c8u+cls*4u);bl=0x20;ebp=1;p8(Z,0x20);}
        }else if(u32(A+0xb5c)==0x12u){
            const std::uint8_t z=u8(Z);
            flag14=(u8(X)==0xffu)?(z!=0xffu?1u:0u):(z==0xffu?1u:0u);
            edi=u32(0x6ac8d8u+cls*4u);bl=0x20;p8(Z,0x20);ebp=1;
        }else{
            const std::uint32_t b4=u32(B+4);
            if((b4&1u)&&(b4&0x80000u)){
                const std::uint32_t k=(u32(B+0x2f0)>>2)&0x1fu;
                edi=u32(((k==3u||k==6u)?0x6ac8c0u:0x6ac8c8u)+cls*4u);
            }else{
                if(!flagE){impact_se_4f67a0(A,0);return;}
                edi=u32(0x6ac8c0u+cls*4u);
            }
            bl=0x20;p8(Z,0x20);ebp=1;
        }
        // L4F7A17
        p32(A+4,af|0x80000u);
        if(edi!=0xeu){
            bool go=true;
            if(gt(f32(A+0x2c8),k29e0)||gt(f32(A+0x2f8),k29e0)){
                const std::int8_t own=std::int8_t(class_4f65a0((u32(A+0x2f0)>>2)&0xffffff1fu));
                const std::int8_t nw=std::int8_t(class_4f65a0(edi));
                if(nw<=own)go=false;
                else bl=u8(Z);
            }
            if(go){
                call(0x4a6ea0u,{A,edi,flag14,0u});
                if(u8(A+0x282)<bl)p8(A+0x282,bl);
            }
        }
        impact_se_4f67a0(A,ebp);
    }
    // 4F9450: the car-car collision pass of the frame (traffic control 47EBA0).
    void collide_4f9450(){
        Frame E(*this,0x220);
        const std::uint32_t recB=E(0x68),recA=E(0x13c);
        const std::uint32_t pl=player();
        {
            const std::int32_t w=i16(pl+0x4e);
            const std::uint32_t ax=std::uint32_t(w<0?-w:w);
            const std::uint32_t sp=u32(pl+0x1f4);
            p32(pl+0xda8,0);
            if(sp>=0x64u&&ax>0x400u&&ax<0x4000u)p16(pl+0x1030,0x3c);
            else if(net_on())p16(pl+0x1030,0);
            else{const std::int16_t v=i16(pl+0x1030);if(v>0)p16(pl+0x1030,std::uint16_t(v-1));}
        }
        auto live=[&](std::uint32_t id){return (u8(0x79fb48u+id)&3u)==2u;};
        // the pre-pass: timers, last-hit car, the box hit tables cleared, the pose saved
        for(std::uint32_t id=8;id<0x20u;++id){
            if(!live(id))continue;
            const std::uint32_t car=work(id);
            if(u8(car+8)&1u)continue;
            {const std::uint32_t t=u32(car+0xd14);if(t==4u||t==3u)continue;}
            p32(car+4,u32(car+4)&0xfba6ffffu);
            p32(car+0xae8,0x19a);p32(car+0xdd4,0x19a);
            if(i16(car+0xb4c)>0)p16(car+0xb4c,u16(car+0xb4c)-1u);
            if(i16(car+0xb4e)>0)p16(car+0xb4e,u16(car+0xb4e)-1u);
            {const std::uint32_t o=u32(car+0xc60);
                if(o!=0x19au){
                    if(!live(o))p32(car+0xc60,0x19a);
                    else if(!jb(K(0x5a29e0u),f32(work(o)+0x2c8)))p32(car+0xc60,0x19a);
                }}
            const std::uint32_t n=u8(car+0x335);
            for(std::uint32_t k=0;k<n;++k){
                const std::uint32_t bp=car+0x44c+k*0x1ecu;
                p32(bp-0xe0,0);
                for(std::uint32_t j=0;j<4;++j){const std::uint32_t q=bp-0xd4+j*0x34u;p32(q-4,0);p32(q,0xffffffffu);p32(q-8,0);}
                p32(bp-0xc,0);
                for(std::uint32_t j=0;j<4;++j){const std::uint32_t q=bp+j*0x34u;p32(q-4,0);p32(q,0xffffffffu);p32(q-8,0);}
                for(std::uint32_t q=0;q<0x10;q+=4)p32(bp+0xc8+q,0);
                for(std::uint32_t q=0;q<0x1c;q+=4)p32(bp+0x2c4+q,u32(bp-0x114+q));
                for(std::uint32_t q=0;q<0xc;q+=4)p32(bp+0x2e0+q,u32(bp-0xf8+q));
                for(std::uint32_t q=0;q<0xc;q+=4)p32(bp+0x2ec+q,u32(bp-0xec+q));
                if(gt(f32(car+0xd24),K(0x6281f0u))){
                    const float lo=sadd(f32(bp-0x10c),f32(car+0xd24));pf(bp-0x10c,lo);
                    const float hi=sadd(f32(bp-0x100),f32(car+0xd24));pf(bp-0x100,hi);
                    const float r=f32(bp-0xfc);
                    const float d=smul(ssub(lo,hi),K(0x628064u));
                    const float q=ssub(smul(r,r),smul(d,d));
                    const X87 root=driving::x87_sqrt(X87(driving::x87_float(driving::x87_abs(X87(q)))));   // 449390, fstp m32, 449380
                    pf(bp-0xf8,driving::x87_float(X87(f32(bp-0x114))-root));
                    pf(bp-0xec,driving::x87_float(root+X87(f32(bp-0x108))));
                }
            }
        }
        // the pairs
        for(std::uint32_t id=8;id<0x20u;++id){
            if(!live(id))continue;
            const std::uint32_t A=work(id);
            if(u8(A+8)&1u)continue;
            const std::uint32_t a4=u32(A+4);
            if(a4&0x40u)continue;
            if(gt(K(0x5e0ac0u),f32(A+0x300)))continue;
            if(std::int8_t(u8(A+0x329))>0xa)continue;
            if(i16(A+0xb64)>0)continue;
            if((a4&1u)&&(a4&2u)&&(a4&0x800000u))continue;
            {const std::uint32_t t=u32(A+0xd14);if(t==4u||t==3u)continue;}
            push_409ef0();
            p32(E(0x4c),0);p32(E(0x34),0);
            const float radA=driving::x87_float(driving::x87_abs(X87(f32(A+0xd24)))+X87(f32(model_4866c0(i8(A+0x11))+0x18)));
            pf(E(0x28),radA);
            for(std::uint32_t idB=id+1;idB<0x20u;++idB){
                if(!live(idB))continue;
                const std::uint32_t B=work(idB);
                if(u8(B+8)&1u)continue;
                if(u8(B+4)&0x40u)continue;
                if(gt(K(0x5e0ac0u),f32(B+0x300)))continue;
                if(std::int8_t(u8(B+0x329))>0xa)continue;
                if(i16(B+0xb64)>0)continue;
                {const std::uint32_t t=u32(B+0xd14);if(t==4u||t==3u)continue;}
                const float radB=driving::x87_float(driving::x87_abs(X87(f32(B+0xd24)))+X87(f32(model_4866c0(i8(B+0x11))+0x18)));
                pf(E(0x24),radB);
                const X87 dist=dist_40f140(B+0x14,A+0x14);
                if(dist>=X87(radB)+X87(radA))continue;   // fcomip; jae (unordered falls through)
                {const std::int32_t d=diff_46f990(B+0x5c,A+0x5c);if(d>=0x3c||d<=-3)continue;}
                const std::uint32_t b4=u32(B+4);
                if((b4&1u)&&(b4&2u)&&(b4&0x800000u))continue;
                float ratio=(b4&0x20000u)?K(0x62806cu):sdiv(f32(B+0x1c4),sadd(f32(A+0x1c4),f32(B+0x1c4)));
                {const float tb=f32(0x6ac8f0u+(u32(A+0x324)*7u+u32(B+0x324))*4u);
                    if(!jb(tb,ratio))ratio=tb;}
                pf(E(0x48),ratio);
                const std::uint32_t nA=u8(A+0x335);
                const float halfB=smul(radB,K(0x628064u));
                for(std::uint32_t kA=0;kA<nA;++kA){
                    if(gt(ssub(f32(B+0x2dc),halfB),f32(A+0x33c+kA*0x1ecu)))continue;
                    rec_clear_4f6b90(recA);
                    const std::uint32_t nB=u8(B+0x335);
                    const float halfA=smul(radA,K(0x628064u));
                    for(std::uint32_t kB=0;kB<nB;++kB){
                        if(gt(ssub(f32(A+0x2dc),halfA),f32(B+0x33c+kB*0x1ecu)))continue;
                        rec_clear_4f6b90(recB);
                        bool hit=false;
                        if(boxes_4f9000(recB,B,A,kB,kA)){
                            const std::uint32_t dst=B+0x36c+kB*0x1ecu+kA*0xd4u;
                            for(std::uint32_t q=0;q<0xd4;q+=4)p32(dst+q,u32(recB+q));
                            hit=true;
                        }
                        if(boxes_4f9000(recA,A,B,kA,kB)){
                            const std::uint32_t dst=A+0x36c+kA*0x1ecu+kB*0xd4u;
                            for(std::uint32_t q=0;q<0xd4;q+=4)p32(dst+q,u32(recA+q));
                            hit=true;
                        }
                        if(!hit)continue;
                        if(u8(B+4)&1u){if(i16(A+0xb4c)>0)p32(A+0xae8,u32(B));p16(A+0xb4c,0x3c);}
                        if(u8(A+4)&1u){if(i16(B+0xb4c)>0)p32(B+0xae8,u32(A));p16(B+0xb4c,0x3c);}
                        p32(B+0xdd4,u32(A));p32(A+0xdd4,u32(B));
                        p32(A+4,u32(A+4)|0x100000u);
                        if(separate_4f8490(A,B,recA,recB,ratio)){replace_4f69f0(B);p32(B+4,u32(B+4)|0x10000u);}
                        p32(E(0x4c),1);p32(E(0x34),B);
                    }
                }
            }
            if(u32(E(0x4c))==1u&&u32(E(0x34))){
                const std::uint32_t B=u32(E(0x34));
                const float b2c8=f32(B+0x2c8);
                replace_4f69f0(A);
                p32(A+4,u32(A+4)|0x10000u);
                p32(B+0xdd4,u32(A));p32(A+0xdd4,u32(B));
                if(u32(0x78026cu)==0x10u){
                    const float a2c8=f32(A+0x2c8);
                    if(u32(0x780258u)==4u){
                        if(u8(A+0x10)<u8(B+0x10)){react_4f6fe0(A,B);react_4f6fe0(B,A);}
                        else{react_4f6fe0(B,A);react_4f6fe0(A,B);}
                    }else{
                        if(u32(A)==8u)react_4f6fe0(A,B);else react_4f76e0(A,B);
                        if(u32(B)==8u)react_4f6fe0(B,A);else react_4f76e0(B,A);
                    }
                    bool skip=false;
                    if(u32(0x780258u)==2u&&call(0x45c440u,{})==0x14u)skip=true;
                    else if(net_on()&&g_46c3c0())skip=true;
                    if(!skip&&!jb(K(0x6281f0u),a2c8)){
                        const std::uint32_t ab=u32(A+4)&1u;
                        const std::uint32_t bb=u32(B+4);
                        if(ab&&(bb&1u)&&(bb&2u))exchange_4f8210(A,B);
                        else if(i16(A+0x1030)>0)impact_se_4f65d0(B,A,0);
                        else if(!ab)exchange_4f7bf0(A,B);
                        else if(gt(b2c8,K(0x619a34u)))impact_se_4f65d0(B,A,0);
                        else exchange_4f7bf0(A,B);
                    }
                }
            }
            pop();
        }
        // restore the boxes saved by the pre-pass
        for(std::uint32_t id=8;id<0x20u;++id){
            if(!live(id))continue;
            const std::uint32_t car=work(id);
            {const std::uint32_t t=u32(car+0xd14);if(t==4u||t==3u)continue;}
            const std::uint32_t n=u8(car+0x335);
            for(std::uint32_t k=0;k<n;++k){
                const std::uint32_t b=car+0x338+k*0x1ecu;
                for(std::uint32_t q=0;q<0x1c;q+=4)p32(b+q,u32(b+0x3d8+q));
                for(std::uint32_t q=0;q<0xc;q+=4)p32(b+0x1c+q,u32(b+0x3f4+q));
                for(std::uint32_t q=0;q<0xc;q+=4)p32(b+0x28+q,u32(b+0x400+q));
            }
        }
    }
    // 487750(x1, z1, x2, z2): the heading word of (x1 - x2, z1 - z2): fpatan (449330), * [6282C0], cvttss2si (4493B0).
    std::uint32_t heading_487750(float x1,float z1,float x2,float z2){
        const float dz=ssub(z1,z2),dx=ssub(x1,x2);
        return std::uint32_t(cvtt(driving::x87_float(driving::x87_atan2(X87(dx),X87(dz))*X87(K(0x6282c0u)))));
    }
    // 449E70(focal, in, out): the point in (xyz, radius at +C) through the current
    // matrix, projected (out xyz) with the projected radius (out +C); returns the scale.
    X87 project_449e70(float focal,std::uint32_t in,std::uint32_t out){
        const auto b=driving::pc_matrix_point(c.matrices,vec(in));
        float x=0.0f,y=0.0f,z=0.0f,scale=0.0f;
        if(!(X87(K(0x6281f0u))>driving::x87_abs(X87(b.z)))){
            scale=sdiv(focal,ssub(0.0f,b.z));
            x=smul(scale,b.x);y=smul(b.y,scale);z=b.z;
        }
        pf(out,x);pf(out+4,y);pf(out+8,z);
        pf(out+0xc,driving::x87_float(driving::x87_abs(X87(smul(f32(in+0xc),scale)))));
        return X87(scale);
    }
    // 4F02D0(camera, sphere, out): the projected sphere inside the depth range and the screen bounds.
    std::uint32_t view_4f02d0(std::uint32_t cam,std::uint32_t in,std::uint32_t out){
        project_449e70(f32(cam+0xb4),in,out);
        if(gt(f32(cam+0xbc),ssub(f32(in+0xc),f32(out+8))))return 0;
        if(gt(ssub(ssub(0.0f,f32(out+8)),f32(in+0xc)),f32(cam+0xc0)))return 0;
        // x bounds 5DA2D4 / 6281C8 (-320 / 320): the port's widescreen view widens them.
        const float half=g_pc_view_half_width==320.0f?K(0x6281c8u):g_pc_view_half_width;
        const float low=g_pc_view_half_width==320.0f?K(0x5da2d4u):-g_pc_view_half_width;
        if(gt(low,sadd(f32(out+0xc),f32(out))))return 0;
        if(gt(ssub(f32(out),f32(out+0xc)),half))return 0;
        if(gt(K(0x5c6fc8u),sadd(f32(out+4),f32(out+0xc))))return 0;
        if(gt(ssub(f32(out+4),f32(out+0xc)),K(0x6281ccu)))return 0;
        return 1;
    }
    // 46DE10: camera-space depth +300 and the view test (+4 bit 7) of every car.
    void visible_46de10(){
        Frame Q(*this,0x40);
        const std::uint32_t cam=u32(0x79f574u);
        push_load(cam+0x140);
        std::uint32_t best=0x19a;
        for(std::uint32_t id=8,rec=0x799d18u;rec<0x79a2b8u;rec+=0x3cu,++id){
            if((u8(0x79fb48u+id)&3u)!=2u)continue;
            const std::uint32_t w=u32(rec);
            point_40a7d0(Q(0x18),w+0x14);
            pf(w+0x300,f32(Q(0x20)));
            p32(Q(0x24),u32(w+0x14));p32(Q(0x28),u32(w+0x18));p32(Q(0x2c),u32(w+0x1c));
            p32(Q(0x30),u32(model_4866c0(i8(w+0x11))+0x18));
            if(!view_4f02d0(cam,Q(0x24),Q(0x34))){p32(w+4,u32(w+4)&0xffffff7fu);continue;}
            const std::uint32_t f=u32(w+4)|0x80u;p32(w+4,f);
            if(!(f&0x20u))continue;
            if(best==0x19au||!jb(f32(w+0x300),f32(work(best)+0x300)))best=id;
        }
        if(best!=0x19au){
            const std::uint32_t w=work(best);
            if(gt(f32(w+0x300),K(0x5b4350u)))pf(w+0x300,K(0x628138u));
        }
        pop();
    }
    // 46DF50(list, &n; edi = &sum): LOD by depth, polygon sum.
    void lod_46df50(std::uint32_t list,std::uint32_t pn,std::uint32_t psum){
        p32(psum,0);p32(pn,0);
        std::uint8_t al=std::uint8_t(call(0x456d60u,{}));
        if(al<1)al=1;else if(al>4)al=4;
        const std::uint32_t level=std::uint8_t(al-1u);
        for(std::uint32_t id=8,rec=0x799d18u;rec<0x79a2b8u;rec+=0x3cu,++id){
            if((u8(0x79fb48u+id)&3u)!=2u)continue;
            const std::uint32_t w=u32(rec);
            const std::uint8_t f=u8(w+4);
            if((f&1u)&&!(f&2u))continue;
            p8(w+0x329,0x7f);
            if(!(f&0x80u))continue;
            const std::int32_t model=i8(w+0x11);
            float base=0.0f;
            if(model==0x1f||model==0x27)base=K(0x5b4364u);
            else if(model==0x28)base=K(0x6282d0u);
            base=ssub(base,K(0x628100u));
            const float z=f32(w+0x300);
            std::uint8_t lod;
            if(ssub(base,K(0x5b4360u))>=z)lod=4;
            else if(!jb(ssub(base,K(0x5b435cu)),z))lod=3;
            else if(!jb(ssub(base,K(0x5b4358u)),z))lod=2;
            else if(!jb(ssub(base,K(0x5b4354u)),z))lod=1;
            else if(!jb(K(0x628138u),z))lod=0;
            else lod=4;
            p8(w+0x328,lod);
            p32(list+u32(pn)*4u,id);p32(pn,u32(pn)+1u);
            const std::uint32_t mr=model_4866c0(model);
            std::uint32_t sum=u32(psum)+std::uint32_t(std::int32_t(i16(mr+std::uint32_t(lod)*4u+0x30)));
            p32(psum,sum);
            if(u32(w+4)&0x100u){sum+=std::uint32_t(std::int32_t(i16(mr+std::uint32_t(lod)*4u+0x32)));p32(psum,sum);}
            const std::uint32_t idx=(level*5u+lod)*4u;
            p32(psum,u32(psum)+u32(0x5b4028u+idx)+u32(0x5b3fb0u+idx));
        }
    }
    // 46E110: the polygon budget.
    std::uint32_t budget_46e110(){
        std::uint32_t esi=0x2ee0;
        if((u8(0x79fb50u)&3u)==2u){
            const std::uint32_t k=(u32(player()+4)>>14)&3u;
            if(k<=2u)esi=0x2ee0u-u32(0x5b40a0u+k*4u);
        }
        return call(0x44c520u,{})+esi+0x1388u;
    }
    // 46E170(list, &n; edi = &sum): lower the LOD of the nearest cars until the sum fits.
    void fit_46e170(std::uint32_t list,std::uint32_t pn,std::uint32_t psum){
        const std::uint32_t budget=budget_46e110();
        for(std::int32_t lv=1;lv<=4;++lv){
            for(std::int32_t i=0;i<std::int32_t(u32(pn))-lv;++i){
                if(!(u32(psum)>budget))break;
                const std::uint32_t w=work(u32(list+std::uint32_t(i)*4u));
                if(std::int32_t(i8(w+0x328))!=lv-1)continue;
                const std::uint32_t mr=model_4866c0(i8(w+0x11));
                std::uint32_t s=u32(psum)-std::uint32_t(std::int32_t(i16(mr+std::uint32_t(std::int32_t(i8(w+0x328)))*4u+0x30)));
                p32(psum,s);
                if(u32(w+4)&0x100u){s-=std::uint32_t(std::int32_t(i16(mr+std::uint32_t(std::int32_t(i8(w+0x328)))*4u+0x32)));p32(psum,s);}
                const std::int8_t lod=std::int8_t(i8(w+0x328)+1);
                p8(w+0x328,std::uint8_t(lod));
                s=u32(psum)+std::uint32_t(std::int32_t(i16(mr+std::uint32_t(std::int32_t(lod))*4u+0x30)));
                p32(psum,s);
                if(u32(w+4)&0x100u){s+=std::uint32_t(std::int32_t(i16(mr+std::uint32_t(std::int32_t(i8(w+0x328)))*4u+0x32)));p32(psum,s);}
            }
        }
    }
    void lod_479500(){
        Frame B(*this,0x70);
        visible_46de10();
        lod_46df50(B(0x10),B(0x8),B(0xc));
        const std::uint32_t n=u32(B(0x8));
        qsort_580cb0(B(0x10),n,4,0x46e420u);
        fit_46e170(B(0x10),B(0x8),B(0xc));
        for(std::uint32_t i=0;std::int32_t(i)<std::int32_t(n);++i)p8(work(u32(B(0x10)+i*4u))+0x329,std::uint8_t(n-i-1u));
        links_479420();
    }
    // 479420: prev/next car links +314/+318 in course order.
    void links_479420(){
        Frame L(*this,0x70);
        std::uint32_t n=0;
        for(std::uint32_t id=8;id<0x20u;++id)if((u8(0x79fb48u+id)&3u)==2u){p32(L(0x8)+n*4u,id);++n;}
        qsort_580cb0(L(0x8),n,4,u32(0x780258u)==4u?0x4793c0u:0x479360u);
        for(std::uint32_t i=0;i<n;++i){
            const std::uint32_t w=work(u32(L(0x8)+i*4u));
            if(i==0){
                p32(w+0x314,0);
                p32(w+0x318,n==1u?0u:work(u32(L(0x8)+4u)));
            }else if(i==n-1u){
                p32(w+0x314,work(u32(L(0x8)+(i-1u)*4u)));p32(w+0x318,0);
            }else{
                p32(w+0x314,work(u32(L(0x8)+(i-1u)*4u)));p32(w+0x318,work(u32(L(0x8)+(i+1u)*4u)));
            }
        }
    }
    // 479580: the shortest road width ahead of the player (803714).
    void gap_479580(){
        Frame B(*this,0x80);
        const std::uint32_t pl=player();
        for(std::uint32_t k=0;k<0x10;k+=4)p32(B(0xc)+k,u32(pl+0x5c+k));
        pf(0x803714u,K(0x59943cu));
        std::uint32_t ebx=0;
        for(std::uint32_t i=0;i<0x32u;++i){
            float v;
            if(u32(B(0xc))){
                const std::int16_t cs=i16(B(0x14));
                v=(cs<0xae&&cs>=8)?K(0x6281e8u):K(0x5b4440u);
            }else{
                if(!call(0x43e6c0u,{B(0x1c),B(0xc),u32(pl+0x1c0)}))break;
                v=f32(B(0x70));
            }
            if(gt(f32(0x803714u),v))pf(0x803714u,v);
            ++ebx;
            if(!advance_46fe70(B(0xc),1))break;
        }
        if(!ebx)pf(0x803714u,K(0x5b4440u));
    }
    // ---- arcade traffic appear (478B90 fixed points, 4787E0 random) ----------------------------
    // 46EF40: a free traffic event (456D60 > 1: also closing/parked works).
    std::uint32_t free_event_46ef40(){
        std::uint32_t edi=u32(0x680ad4u);
        if(std::int32_t(edi)>=0x18)return 0x19a;
        for(std::uint32_t rec=0x799d18u+edi*0x3cu;rec<0x79a2b8u;rec+=0x3cu,++edi){
            if(u8(0x79fb50u+edi)&3u)continue;
            if((call(0x456d60u,{})&0xffu)<=1u)return edi+8u;
            if(lan_owned_4551c0(edi-u32(0x680ad4u)))continue;
            const std::uint32_t c=u32(u32(rec)+0xc5c);
            if(c&0x4000u)continue;
            if(!(c&1u))return edi+8u;
        }
        return 0x19a;
    }
    // 46D0C0(ebx = car; edi, esi = out bytes): lane range of the car's road.
    void lane_range_46d0c0(std::uint32_t car,std::uint32_t lo,std::uint32_t hi){
        if(u32(car+0x5c)){
            if(u32(car+0x60)==0x64u){p8(lo,0);p8(hi,2);}else{p8(lo,3);p8(hi,5);}
            return;
        }
        const std::uint32_t v=call(0x44f0f0u,{call(0x44dc50u,{u32(car+0x68)}),u16(car+0x64)});
        if(!v){p8(lo,0);p8(hi,5);return;}
        if(i8(car+0x66)>=3){p8(lo,3);p8(hi,5);}else{p8(lo,0);p8(hi,2);}
    }
    // 46CE30(edi = appear record): the traffic model of the next car.
    std::uint32_t pick_model_46ce30(std::uint32_t rec){
        std::uint32_t k=0;
        while(k<5u&&!u8(rec+6u+k))++k;
        std::uint32_t c;
        auto roll=[&](std::uint32_t i){
            const std::uint8_t al=std::uint8_t(std::uint8_t(std::int8_t(i8(0x80a668u+i))*5)+1u);
            p8(0x80a668u+i,al);
            const std::int32_t v=std::int32_t(al)*100;
            return v/255;
        };
        if(k==5u){c=4;p32(0x800d9cu,c);}
        else{
            c=u32(0x800d9cu);
            if(!(roll(c)<i8(rec+6u+c))){
                do{++c;if(std::int32_t(c)>=5)c=0;}while(!(roll(c)<i8(rec+6u+c)));
                p32(0x800d9cu,c);
            }
        }
        const std::uint32_t cnt=0x800d3cu+c;
        p8(cnt,u8(cnt)+1u);
        const std::uint32_t v=u32(0x780258u);
        if((v==3u||v==4u)&&std::int32_t(c)>=0&&std::int32_t(c)<=3){c=4;p32(0x800d9cu,c);}
        if(v==2u){
            const std::uint32_t z=call(0x45c440u,{});
            c=u32(0x800d9cu);
            if(z!=0x16u&&c==0u){c=4;p32(0x800d9cu,c);}
        }
        if(c>4u)return 0x21;
        const std::uint32_t preset=u32(0x78024cu);
        switch(c){
        case 0:
            if(i8(cnt)>=2)p8(cnt,0);
            if(preset==1u||preset==3u)return u8(cnt)?0x29u:0x27u;
            return u8(cnt)?0x29u:0x2bu;
        case 1:case 2:
            p8(cnt,0);return (preset==1u||preset==3u)?0x28u:0x2bu;
        case 3:p8(cnt,0);return 0x1f;
        default:break;
        }
        if(preset&&preset!=2u&&!call(0x55a930u,{},0,0x7f9460u)&&!u32(0x80fb14u)){
            const std::int8_t a=i8(cnt);
            if(a>=6){p8(cnt,0);return 0x1e;}
            if(a==0)return 0x1e;
            if(a==1)return 0x20;
            if(a==2)return 0x21;
            if(a==3)return 0x22;
            return a!=4?0x24u:0x23u;
        }
        const std::int32_t stage=std::int32_t(call(0x44dc50u,{u32(player()+0x68)}));
        const std::int32_t a=i8(cnt);
        if(stage==0x1a){const std::int8_t r=std::int8_t(a%9);p8(cnt,std::uint8_t(r));return u32(0x64e1b4u+std::uint32_t(std::int32_t(r))*4u);}
        const std::int8_t r=std::int8_t(a%7);p8(cnt,std::uint8_t(r));return u32(0x64e198u+std::uint32_t(std::int32_t(r))*4u);
    }
    // Common tail of 478B90 / 4787E0: open the car `id` (function `fn`) at `place`.
    // F(x) = the caller's PC frame [esp+x].
    void open_traffic(std::uint32_t pl,std::uint32_t id,std::uint32_t fn,std::uint32_t model,std::uint32_t colour,std::uint32_t rec2arg,
                      std::uint32_t place,std::uint32_t pos,std::uint32_t ang,std::uint32_t fslot,std::uint32_t dist,std::uint32_t vel){
        call(0x440110u,{id,fn});
        const std::uint32_t w=work(id);
        if(gt(K(0x5b443cu),f32(pl+0x1c4)))pf(fslot,0.0f);
        const std::uint32_t cs=std::uint16_t(u16(pl+0x260)+std::uint16_t(dist));
        set_work_46e830(w,id,std::int8_t(model),std::uint8_t(colour),pos,ang,f32(fslot),place,0,cs,1);
        const std::uint32_t rec=call(0x4f0030u,{w+0x5c,rec2arg});
        const float spd=smul(float(std::int32_t(i16(rec+0xc))),K(0x5b43a4u));
        pf(w+0x1c4,spd);
        p32(w+0x1f4,ftol(X87(spd)*X87(K(0x5a460cu))));
        velocity_46d060(ang,f32(w+0x1c4),vel);
        p32(w+0x20,u32(vel));p32(w+0x24,u32(vel+4));p32(w+0x28,u32(vel+8));
        if(!u32(place))p32(0x803718u+std::uint32_t(std::int32_t(i8(place+0xa)))*4u,u32(0x803718u+std::uint32_t(std::int32_t(i8(place+0xa)))*4u)+1u);
        std::uint32_t n=u32(0x800d9cu)+1u;p32(0x800d9cu,std::int32_t(n)>=5?0u:n);
        p8(0x804450u+model,u8(0x804450u+model)+1u);
        p16(0x800d50u,u16(pl+0x64));
    }
    // 478B90(player): a car at the next fixed appear point of the course.
    std::uint32_t fixed_478b90(std::uint32_t pl){
        Frame F(*this,0x80);
        p8(F(0x12),0);
        if(call(0x55a930u,{},0,0x7f9460u)&&u32(0x800ab0u))return 0;
        if((call(0x4b00d0u,{})&0xffu)&&(call(0x4b00e0u,{})&0xffu))return 0;
        if(call(0x55a930u,{},0,0x7f9460u)||u32(0x80fb14u)){
            const std::uint32_t ar=call(0x44c8d0u,{u32(pl+0x68)});
            if(u32(ar+u32(pl+0x5c)*4u+0x64))return 0;
            const std::uint32_t fl=u32(ar+0xc);
            if(fl&0x100u)return 0;
            if(fl&0x200u)p8(F(0x12),1);
        }
        const std::uint32_t stage=call(0x44dc50u,{u32(pl+0x68)});
        std::uint32_t e=u32(0x6a6ce0u+stage*4u);                       // 4EF880
        if(i32(e)<0x42){
            for(;;){
                if((u8(F(0x12))||u32(e)==stage)&&u32(e+4)==u32(pl+0x5c)&&u16(e+8)==u16(pl+0x64)&&!u8(e+0x11))break;
                const std::int32_t nx=i32(e+0x14);e+=0x14u;
                if(nx>=0x42)break;
            }
        }
        if(u32(e)==0x42u)return 0;
        std::int32_t kind=i32(e+0xc);
        p8(F(0x30),u8(e+0x10));
        std::uint32_t fn;
        if(kind>=0&&kind<0x1e){
            if(call(0x55a930u,{},0,0x7f9460u)||u32(0x80fb14u)){kind=0x21;fn=0x59;}
            else{
                const std::uint32_t v=u32(0x780258u);
                if((v==3u||v==4u)&&(call(0x456d60u,{})&0xffu)>1u)return 0;
                if(u32(0x780258u)==2u)return 0;
                if(i8(0x800d28u)>=2)return 0;
                fn=0x55;
            }
        }else{
            if(u32(0x780258u)==2u&&kind>=0x27&&kind<=0x2b)return 0;
            fn=0x59;
        }
        const std::uint32_t id=free_event_46ef40();
        if(id==0x19au)return 0;
        const std::uint32_t place=F(0x64);
        for(std::uint32_t k=0;k<0x10;k+=4)p32(place+k,u32(pl+0x5c+k));
        std::uint32_t dist=0x32;
        if(u32(place)&&i16(pl+0x64)>=0x78&&std::int32_t(i16(pl+0x64))<=end_43d470(u32(place))-0xf)dist=0x23;
        if(!advance_46fe70(place,std::int32_t(dist)))return 0;
        lane_range_46d0c0(pl,F(0x12),F(0x13));
        const std::int8_t want=i8(e+0xa),lo=i8(F(0x12)),hi=i8(F(0x13));
        p8(place+0xa,std::uint8_t(want<lo?lo:(want>hi?hi:want)));
        if(!place_pose_470240(place,0,F(0x40),F(0x34),F(0x20)))return 0;
        for(std::uint32_t k=0;k<0xc;k+=4)p32(F(0x4c)+k,u32(F(0x40)+k));
        call(0x43eb60u,{0x100u,F(0x4c),0,0,F(0x2c)});
        if(u32(F(0x2c))==1u)return 0;
        open_traffic(pl,id,fn,std::uint32_t(kind),u8(F(0x30)),0,place,F(0x40),F(0x34),F(0x20),dist,F(0x58));
        if((call(0x456d60u,{})&0xffu)>1u){
            const std::uint32_t w=work(id);
            p32(w+0xc5c,u32(w+0xc5c)|0x4000u);
            call(0x4401d0u,{id});
        }
        p8(e+0x11,1);
        return 1;
    }
    // 4787E0(player, appear record): a random traffic car ahead of the player.
    void spawn_4787e0(std::uint32_t pl,std::uint32_t rec){
        Frame F(*this,0x80);
        if(call(0x55a930u,{},0,0x7f9460u)&&u32(0x800ab0u))return;
        if((call(0x4b00d0u,{})&0xffu)&&(call(0x4b00e0u,{})&0xffu))return;
        if(u32(pl+0x184)!=u32(pl+0x5c))clear_46c950();
        std::int32_t gap=i8(rec+5);if(gap<3)gap=3;
        if(std::int32_t(i16(pl+0x64))-std::int32_t(i16(0x800d50u))<gap)return;
        if(i8(0x800d52u)>=0x14)return;
        const std::int8_t bl=i8(rec+4);
        std::int32_t want=cvtt(smul(density_46cd60(),float(std::int32_t(bl))));
        if(want<0)want=0;else if(want>0x14)want=0x14;
        if(bl>0&&want==0)want=1;
        if(std::int32_t(i8(0x800d52u))>=want)return;
        const std::uint32_t id=free_event_46ef40();
        p32(F(0x20),id);
        if(id==0x19au)return;
        if(u32(0x780258u)==4u)return;
        const std::uint32_t place=F(0x54);
        for(std::uint32_t k=0;k<0x10;k+=4)p32(place+k,u32(pl+0x5c+k));
        std::uint32_t dist=0x32;
        if(u32(place)&&i16(pl+0x64)>=0x78&&std::int32_t(i16(pl+0x64))<=end_43d470(u32(place))-0xf)dist=0x23;
        p32(F(0x10),dist);
        if(!advance_46fe70(place,std::int32_t(dist)))return;
        if(!lane_478640(pl,place,rec,0))return;
        if(!place_pose_470240(place,0,F(0x24),F(0x30),F(0x14)))return;
        for(std::uint32_t k=0;k<0xc;k+=4)p32(F(0x3c)+k,u32(F(0x24)+k));
        call(0x43eb60u,{0x100u,F(0x3c),0,0,F(0x18)});
        if(u32(F(0x18))==1u)return;
        const std::uint32_t model=pick_model_46ce30(rec);
        const std::uint32_t c=0x804358u+model;
        switch(model){
        case 0x26u:if(i8(c)>=2)p8(c,0);break;
        case 0x2bu:if(call(0x45c440u,{})==0x16u)p8(c,2);else if(i8(c)>=2)p8(c,0);break;
        case 0x27u:case 0x29u:p8(c,0);break;
        default:if(i8(c)>=3)p8(c,0);break;
        }
        const std::uint8_t colour=u8(c);
        p8(F(0x1c),colour);
        p8(c,std::uint8_t(colour+1u));
        open_traffic(pl,id,0x59,model,colour,rec,place,F(0x24),F(0x30),F(0x14),dist,F(0x48));
    }
    // ==== per-car control 47E780 (traffic / racers, event functions 0x55..0x59) ====================
    // ---- math / matrix leaves ---------------------------------------------------------------
    // 40F0E0(v): |v| (x87).
    X87 len_40f0e0(std::uint32_t v){return driving::x87_sqrt((x(v)*x(v)+x(v+4)*x(v+4))+x(v+8)*x(v+8));}
    // 40F1F0(o, v, axis): projection of v on axis; 0 when |axis|^2 < [6282A8] (double).
    void project_40f1f0(std::uint32_t o,std::uint32_t v,std::uint32_t ax){
        const X87 a0=X87(driving::x87_float(x(ax)*x(ax)));                  // fstp m32 of axis.x^2
        const X87 n2=(x(ax+4)*x(ax+4)+x(ax+8)*x(ax+8))+a0;
        double lim;{const std::uint32_t lo=u32(0x6282a8u),hi=u32(0x6282acu);const std::uint64_t q=(std::uint64_t(hi)<<32)|lo;std::memcpy(&lim,&q,8);}
        if(n2.v<X87(lim).v){p32(o,0);p32(o+4,0);p32(o+8,0);return;}
        const X87 dot=(x(v+8)*x(ax+8)+x(v+4)*x(ax+4))+x(v)*x(ax);
        const X87 q=dot/n2;
        pf(o,driving::x87_float(q*x(ax)));pf(o+4,driving::x87_float(q*x(ax+4)));pf(o+8,driving::x87_float(q*x(ax+8)));
    }
    // 449470(a): a wrapped into [-pi, pi] (SSE loops), returned on the x87 stack.
    float wrap_449470(float a){
        const float hi=K(0x6280c8u),step=K(0x5a29dcu),lo=K(0x5a29d8u);
        if(gt(a,hi)){do a=ssub(a,step);while(gt(a,hi));}
        if(gt(lo,a)){do a=sadd(a,step);while(gt(lo,a));}
        return a;
    }
    void push_409ef0(){driving::pc_matrix_push(c.matrices);}
    void get_40a0d0(std::uint32_t o){driving::pc_matrix_get(c.matrices,m.bytes(o,64));}
    void load_40a170(std::uint32_t a){driving::pc_matrix_load(c.matrices,m.bytes(a,64));}
    void translation_40a250(std::uint32_t o){put_vec(o,driving::pc_matrix_translation(c.matrices));}
    void translate3_40a290(float x0,float y0,float z0){driving::pc_matrix_translate_vector(c.matrices,{x0,y0,z0});}
    void untranslate_40a310(std::uint32_t v){driving::pc_matrix_translate_vector(c.matrices,{-f32(v),-f32(v+4),-f32(v+8)});}
    // ---- module leaves -----------------------------------------------------------------------
    std::uint32_t g_46c3b0(){return u32(0x800aa0u);}
    std::uint32_t g_46c3c0(){return u32(0x7f95f4u);}
    std::uint32_t g_46c3d0(){return u32(0x7f95f8u);}
    std::uint32_t g_46c3e0(){return u32(0x800aa4u);}
    std::uint32_t net_slot_46c4e0(std::int32_t i){return i<0x18?u32(0x7f9460u+std::uint32_t(i)*4u):0u;}
    // 46C7E0(eax = car, rec, lane): lane bit of the appear record (+B), 0 for racers.
    std::uint32_t lane_bit_46c7e0(std::uint32_t w,std::uint32_t rec,std::uint32_t lane){
        if(w&&u32(0x80fb14u)&&(u8(w+4)&0x20u))return 0;
        return (1u<<(lane&31u))&u8(rec+0xb);
    }
    // 46CA40(eax = car): racer with +8 bit 30.
    std::uint8_t racer_bit30_46ca40(std::uint32_t w){return (u8(w+4)&0x20u)?std::uint8_t((u32(w+8)>>30)&1u):0u;}
    // 470050(eax = out, ecx = packed road sample): position and width.
    void decode_470050(std::uint32_t o,std::uint32_t p){
        const float k0=K(0x62818cu),k1=K(0x5a29ecu);
        pf(o,smul(smul(float(i16(p)),k0),k1));
        const std::int16_t y=std::int16_t(std::int16_t(u16(p+4)<<4)>>4);
        pf(o+4,smul(smul(float(y),k0),k1));
        pf(o+8,smul(smul(float(i16(p+2)),k0),k1));
        const bool wide=(u8(p+5)&0x10u)!=0;
        p32(o+0x10,0);
        pf(o+0xc,wide?K(0x5b437cu):K(0x5b4378u));
    }
    std::uint32_t cfg_mode3_4762e0(){const std::uint32_t c0=u32(0x80fb0cu);return (c0&&(u32(c0+0x30)&3u)==3u)?1u:0u;}
    // 477390: the car of the first racer in rank order (player when the rank names it).
    std::uint32_t first_racer_car_477390(){
        const std::int32_t n=i32(0x80fb2cu);
        if(n<=0)return 0;
        const std::uint32_t list=racers();
        std::uint32_t e=u32(0x80fb1cu)+0x10u;
        for(std::int32_t i=0;i<n;++i,e+=0x14u){
            const std::int32_t idx=i32(e);
            if(idx<0)return player();
            const std::uint32_t r=std::uint32_t(idx)*0xa0u+list;
            if(i32(r+0x4c)>=0)return work(u32(r+0x4c));
        }
        return 0;
    }
    // 4773F0: the leader's car (player when no leader record).
    std::uint32_t leader_car_4773f0(){
        const std::uint32_t list=racers();
        if(list){
            const std::int32_t i=i32(0x64e190u);
            if(i>=0){
                const std::uint32_t r=std::uint32_t(i)*0xa0u+list;
                if(r){const std::int32_t id=i32(r+0x4c);return id<0?0u:work(std::uint32_t(id));}
            }
        }
        return player();
    }
    // 4722C0: the event car (+4 bit 0) with the smallest course distance +260.
    std::uint32_t last_car_4722c0(){
        std::uint32_t best=player(),lo=0x7fff;
        const std::int32_t n=i32(0x680ad4u);
        for(std::int32_t i=0;i<n;++i){
            if((u8(0x79fb50u+std::uint32_t(i))&3u)!=2u)continue;
            const std::uint32_t w=u32(0x799d18u+std::uint32_t(i)*0x3cu);
            if(!(u8(w+4)&1u))continue;
            const std::uint32_t d=u16(w+0x260);
            if(d<std::uint16_t(lo)){best=w;lo=d;}
        }
        return best;
    }
    // 471480(a, b, dist): frames for b to close dist on a (7FFF when not closing).
    std::int32_t time_471480(std::uint32_t a,std::uint32_t b,float d){
        const float dv=ssub(f32(b+0x1c4),f32(a+0x1c4));
        if(gt(K(0x6281f0u),dv))return 0x7fff;
        std::int32_t r=cvtt(sdiv(d,dv));
        const std::uint32_t v=u32(0x780258u);
        if(v==3u||v==4u)r-=0x1e;
        return r;
    }
    // 470A30 / 470AB0(eax = car, edx = value, rec): start a lane change left / right.
    void lane_left_470a30(std::uint32_t w,std::uint32_t val,std::uint32_t rec){
        if(!gt(K(0x62806cu),f32(w+0xb08))){
            if(u32(0x80fb14u)&&(u8(w+4)&0x20u))return;
            if(!((1u<<(u8(w+0x66)&31u))&u8(rec+0xb)))return;
        }
        p32(w+0xc5c,(u32(w+0xc5c)&0xfffffbffu)|0x200u);
        p32(w+0xb14,val);p32(w+0xb18,val);p16(w+0xb50,0);p32(0x804350u,0);
    }
    void lane_right_470ab0(std::uint32_t w,std::uint32_t val,std::uint32_t rec){
        if(!gt(f32(w+0xb08),K(0x6280c4u))){
            if(u32(0x80fb14u)&&(u8(w+4)&0x20u))return;
            if(!((1u<<(u8(w+0x66)&31u))&u8(rec+0xb)))return;
        }
        p32(w+0xc5c,(u32(w+0xc5c)&0xfffffdffu)|0x400u);
        p32(w+0xb14,val);p32(w+0xb18,val);p16(w+0xb50,0);p32(0x804350u,0);
    }
    // 476280(edx = road table 0/1, cx = sample, eax = out or 0, kind): lane of a preload sample, FF when none.
    std::uint8_t preload_lane_476280(std::uint32_t kind,std::int32_t table,std::uint32_t sample,std::uint32_t out){
        if(kind||table<0||table>=2)return 0xff;
        const std::uint32_t t=0x800da8u+std::uint32_t(table)*0x1030u;     // 476292 bridge: imul edx,edx,0x1030
        if(u16(t)!=0x4f53u)return 0xff;
        if(std::uint16_t(sample)>=u16(t+2))return 0xff;
        const std::uint32_t s=t+std::uint32_t(std::uint16_t(sample))*6u+4u;
        const std::uint16_t f=u16(s+4);
        if(f&0x1000u)return 0xff;
        if(out)decode_470050(out,s);
        return std::uint8_t((f>>13)&7u);
    }
    // 4755C0(eax = car): previous-frame copies.
    void save_prev_4755c0(std::uint32_t w){
        for(std::uint32_t k=0;k<0xc;k+=4)p32(w+0x16c+k,u32(w+0x14+k));     // 4755C6 bridge: lea edx,[eax+0x16C]
        p32(w+0x178,u32(w+0x1c4));
        p16(w+0x17c,u16(w+0x2c));p16(w+0x17e,u16(w+0x2e));p16(w+0x180,u16(w+0x30));
        for(std::uint32_t k=0;k<0x10;k+=4)p32(w+0x184+k,u32(w+0x5c+k));
        for(std::uint32_t k=0;k<0xc;k+=4)p32(w+0x1040+k,u32(w+0x2d8+k));
        for(std::uint32_t k=0;k<0xc;k+=4)p32(w+0x1034+k,u32(w+0x2e4+k));
        p16(w+0xc2e,u16(w+0xc2c));
    }
    // ---- hits ---------------------------------------------------------------------------------
    // 475F70(a, b): hit sound kind from the closing speed along a -> b.
    std::uint32_t hit_kind_475f70(std::uint32_t a,std::uint32_t b){
        Frame F(*this,0x30);
        pf(F(4),ssub(f32(a+0x20),f32(b+0x20)));pf(F(8),ssub(f32(a+0x24),f32(b+0x24)));pf(F(0xc),ssub(f32(a+0x28),f32(b+0x28)));
        sub_40efa0(F(0x10),b+0x14,a+0x14);
        project_40f1f0(F(0x1c),F(4),F(0x10));
        const X87 len=len_40f0e0(F(0x1c));
        const std::uint32_t cls=u32(b+0x324);
        const bool small=cls==0u||cls==1u||cls==2u;
        if(!(len>=X87(K(0x5b440cu))))return small?0x12u:0x11u;
        return small?0x13u:0x10u;
    }
    // 476040(other, self, kind): self was hit by other.
    void hit_476040(std::uint32_t o,std::uint32_t s,std::uint32_t k){
        p32(s+0xb58,gt(f32(o+0x26c),K619a34)?0xbu:0xcu);
        const std::uint8_t n=std::uint8_t(u8(s+0xb6d)+1u);
        p8(s+0xb6c,1);p8(s+0xb6d,n);
        if(u32(s+0x324)!=6u){
            if(!(call(0x55a930u,{},0,0x7f9460u)&&g_46c3d0()))pf(s+0x1c4,smul(f32(o+0x1c4),K(0x5b005cu)));
            p16(s+0xb60,0x78);
        }
        auto sound=[&](std::uint32_t kind){
            switch(kind){case 0x10u:call(0x424940u,{0x4fu});break;case 0x11u:call(0x424940u,{0x5eu});break;
                case 0x12u:call(0x424940u,{0x64u});break;case 0x13u:call(0x424940u,{0x65u});break;default:break;}
        };
        bool wreck=call(0x45c440u,{})==0x14u;
        if(!wreck){
            if(call(0x55a930u,{},0,0x7f9460u)&&g_46c3c0()){
                const std::uint32_t mode=u32(s+0xd14);
                if(mode==2u)return;
                if(mode!=1u){
                    std::uint32_t kind=k;
                    if(g_46c3d0()){kind=0xf;call(0x4a2270u,{s,0xfu,0,0,0x3fc00000u});}
                    else call(0x4a6ea0u,{s,k,0,0});
                    sound(kind);
                    return;
                }
                wreck=true;
            }
        }
        if(wreck){
            call(0x4a6ea0u,{s,0x10u,0,0});call(0x424940u,{0xceu});
            const std::int32_t r=std::int32_t(call(0x44fe40u,{}));
            call(0x424940u,{(r%2)==0?0xcfu:0xd0u});
            return;
        }
        call(0x4a6ea0u,{s,k,0,0});
        sound(k);
        if((u8(s+4)&0x20u)&&u32(0x80fb14u))return;
        if(call(0x45c440u,{})==0xeu)return;
        p16(s+0xb64,0x78);
        const std::uint32_t v=u32(0x780258u);
        if(v==3u||v==4u)return;
        p32(s+0xc5c,u32(s+0xc5c)|1u);
    }
    // 476260(a, b): b hit by a with the kind of 475F70.
    void hit_476260(std::uint32_t a,std::uint32_t b){hit_476040(a,b,hit_kind_475f70(a,b));}
    // ---- racer steering trims (474D30 / 474990) ------------------------------------------------
    void trim_474990(std::uint32_t w){
        Frame L(*this,0x30);
        const float one=K(0x62806cu);
        pf(L(0x18),one);pf(L(0xc),one);pf(L(0x10),one);pf(L(0x20),one);pf(L(0x1c),one);pf(L(0x14),0.0f);
        bool trim=false;
        if(i32(w+8)<0){
            const std::uint32_t cfg=u32(0x80fb0cu);
            std::uint32_t t,list;
            if((u32(w+0xc)&0x8000000u)&&cfg&&(t=u32(cfg+0x44))!=0u&&(list=u32(t))!=0u){
                std::uint32_t found=0;
                for(;;){
                    if(i32(list)<0)break;
                    if(call(0x44dc50u,{u32(w+0x68)})==u32(list)){
                        std::uint32_t r=u32(list+4);
                        std::uint16_t lo=u16(r);
                        while(lo!=0xffffu){
                            if(u32(r+4)==u32(w+0x5c)){
                                const std::int32_t cs=i16(w+0x64);
                                if(cs>=std::int32_t(lo)&&cs<=std::int32_t(u16(r+2))){found=r;break;}
                            }
                            lo=u16(r+0x20);r+=0x20u;
                        }
                    }
                    list+=8u;
                    if(found)break;
                }
                if(found){
                    static constexpr std::uint32_t slot[]{0x18,0xc,0x20,0x10,0x1c,0x14};
                    for(std::uint32_t k=0;k<6;++k){const float v=f32(found+8u+k*4u);if(!(v==0.0f))pf(L(slot[k]),v);}
                }
            }
            trim=i32(w+8)<0&&(u32(w+0xc)&0x8000000u);
        }
        if(trim){
            const std::int32_t a=cvtt(smul(float(i16(w+0x162)),f32(L(0xc))));
            p32(L(0x24),std::uint32_t(a));
            std::uint32_t edi=std::uint32_t(std::abs(std::int32_t(std::int16_t(a))));
            pf(L(0xc),driving::x87_float(driving::x87_abs(x(w+0xbbc))*x(w+0x1c4)));
            float s=gt(0.0f,f32(w+0xbbc))?K(0x6280c4u):K(0x62806cu);
            const std::int32_t t=cvtt(smul(f32(L(0x14)),K(0x5b4400u)));
            std::uint32_t ebp=std::uint32_t(cvtt(smul(smul(float(std::int16_t(t)),s),f32(L(0xc)))));
            std::uint32_t ebx=std::uint32_t(std::abs(std::int32_t(std::int16_t(ebp))));
            pf(L(0xc),driving::x87_float(driving::x87_abs(x(w+0xbb8))*x(w+0x1c4)));
            s=gt(0.0f,f32(w+0xbb8))?K(0x6280c4u):K(0x62806cu);
            const std::uint32_t ecx=std::uint32_t(cvtt(smul(s,f32(L(0xc)))));
            const std::uint32_t eax=std::uint32_t(std::abs(std::int32_t(std::int16_t(ecx))));
            if(std::int16_t(eax)>std::int16_t(ebx)){ebp=ecx;ebx=eax;}
            if(std::int16_t(ebx)>std::int16_t(edi))edi=ebx;
            else ebp=u32(L(0x24));
            const std::int32_t cap=cvtt(smul(f32(L(0x18)),K(0x5b43fcu)));
            if(std::int16_t(edi)>std::int16_t(cap)){
                const std::uint32_t edx=std::uint32_t(cvtt(smul(f32(L(0x1c)),K(0x5b43f8u))));
                std::uint32_t ax=u16(w+0xc2c);
                edi=std::uint32_t(-std::int32_t(edi))-edx;
                if(std::int16_t(ebp)>0){
                    edi=std::uint32_t(-std::int32_t(edi));
                    if(std::int16_t(ax)<std::int16_t(edi))p16(w+0xc2c,edi);
                    else{
                        ax-=std::uint32_t(cvtt(smul(f32(L(0x10)),K(0x5a29f0u))));p16(w+0xc2c,ax);
                        if(std::int16_t(ax)<std::int16_t(edi))p16(w+0xc2c,edi);
                    }
                }else{
                    if(std::int16_t(ax)>std::int16_t(edi))p16(w+0xc2c,edi);
                    else{
                        ax-=std::uint32_t(cvtt(smul(f32(L(0x10)),K(0x5b43f4u))));p16(w+0xc2c,ax);
                        if(std::int16_t(ax)>std::int16_t(edi))p16(w+0xc2c,edi);
                    }
                }
                trim_flag_474cdb(w);
                return;
            }
        }
        {   // 474C92: return the trim to 0
            const std::uint32_t step=std::uint32_t(cvtt(smul(f32(L(0x20)),K(0x5afd30u))));
            std::uint32_t ax=u16(w+0xc2c);
            if(std::int16_t(ax)!=0){
                if(std::int16_t(ax)<0){ax+=step;p16(w+0xc2c,ax);if(std::int16_t(ax)>0)p16(w+0xc2c,0);}
                else{ax-=step;p16(w+0xc2c,ax);if(std::int16_t(ax)<0)p16(w+0xc2c,0);}
            }
        }
        trim_flag_474cdb(w);
    }
    void trim_flag_474cdb(std::uint32_t w){
        if(i16(w+0xc2c)==0){
            const X87 v=driving::x87_abs(X87(driving::x87_float(X87(std::int32_t(i16(w+0x162)))*X87(K(0x628258u)))));
            if(v>X87(K(0x5b43f0u))){p32(w+0xc,u32(w+0xc)|0x100000u);return;}
            p32(w+0xc,u32(w+0xc)&0xffefffffu);
            return;
        }
        p32(w+0xc,u32(w+0xc)|0x100000u);
    }
    void trim_474d30(std::uint32_t w){
        if(u16(w+0xc26)){p16(w+0xc26,u16(w+0xc26)-1u);trim_474990(w);return;}
        if(u32(w+0x208)>u32(w+0x1d8)&&call(0x44fdf0u,{})>0x384u){
            const std::uint32_t seed=(u32(0x6a4e2cu)*0x5e5u+0x29u)&0xffffu;
            p32(0x6a4e2cu,seed);
            if(gt(f32(w+0xbc8),smul(float(std::int32_t(seed)),K(0x5b4320u)))&&!(u8(w+0xa)&1u)){
                p16(w+0xc26,0x78);trim_474990(w);return;
            }
        }
        p32(w+0x1d8,u32(w+0x208));
        call(0x502420u,{w});
        trim_474990(w);
    }
    // ---- display matrices -------------------------------------------------------------------
    // 470360(car): shadow position/angles (+C70/+C7C) and the shadow matrix (+C90, previous +CD0).
    void shadow_470360(std::uint32_t w){
        Frame F(*this,0x20);
        const std::uint32_t v=u32(u32(u32(model_4866c0(i8(w+0x11))))+4);
        const std::uint32_t edi=w+0xc70,ebx=w+0xc7c;
        push_unit();translate_40a2d0(w+0x14);
        rotate_y(word_radians(i16(w+0x2e)));rotate_x(word_radians(i16(w+0x2c)));rotate_z(word_radians(i16(w+0x30)));
        push_409ef0();invert_40a240();
        point_40a7d0(F(0x10),edi);
        pop();
        angles_449800(ebx,F(0x10));
        rotate_y(f32(ebx+4));rotate_x(f32(ebx));rotate_z(f32(ebx+8));
        translate3_40a290(0.0f,0.0f,12.0f);
        translation_40a250(edi);
        pop();
        push_unit();
        for(std::uint32_t k=0;k<0x40;k+=4)p32(w+0xcd0+k,u32(w+0xc90+k));
        translate_40a2d0(v+4);
        rotate_y(f32(ebx+4));rotate_x(f32(ebx));rotate_z(f32(ebx+8));
        get_40a0d0(w+0xc90);
        pop();
    }
    // 470570(ebx = car): is the camera inside one of the car's boxes (+335 boxes at +338, 0x1EC each)?
    void inside_470570(std::uint32_t w){
        if(u32(w+0x320)==3u)return;
        const std::uint32_t id=u32(w);
        const std::int32_t par=std::int32_t(id)%2;
        if(std::int32_t(call(0x450680u,{})&1u)!=par)return;
        p32(w+4,u32(w+4)&0xfffffffbu);
        call(0x440bd0u,{id,0x40});
        if(!u8(w+0x335))return;
        Frame L(*this,0x40);
        for(std::uint32_t box=0;box<u8(w+0x335);++box){
            for(std::uint32_t k=0;k<0x1c;k+=4)p32(L(0x18)+k,u32(w+0x338+box*0x1ecu+k));
            if(gt(f32(w+0xd24),K(0x6281f0u))){
                pf(L(0x20),sadd(f32(w+0xd24),f32(L(0x20))));
                pf(L(0x2c),sadd(f32(w+0xd24),f32(L(0x2c))));
            }
            push_unit();
            if(box==1u){load_40a170(w+0xc90);invert_40a240();}
            rotate_y(ssub(0.0f,f32(w+0x2e8)));
            rotate_z(-word_radians(i16(w+0x30)));
            rotate_x(-word_radians(i16(w+0x2c)));
            rotate_y(-word_radians(std::int32_t(i16(w+0xc2c))+std::int32_t(i16(w+0x2e))));
            untranslate_40a310(w+0x14);
            call(0x483e90u,{L(0xc)});
            point_40a7d0(L(0xc),L(0xc));
            pop();
            const float px=f32(L(0xc)),py=f32(L(0x10)),pz=f32(L(0x14));
            if(gt(py,f32(L(0x1c)))||gt(f32(L(0x28)),py))continue;
            if(gt(px,f32(L(0x18)))||gt(f32(L(0x24)),px))continue;
            if(gt(pz,f32(L(0x20))))continue;
            if(!jbe(f32(L(0x2c)),pz))continue;
            p32(w+4,u32(w+4)|4u);
            call(0x440bd0u,{id,0x100});
            return;
        }
    }
    // 470760(esi = car): pitch/roll of the body from the yaw rate (+32 roll, +202 steer).
    void body_470760(std::uint32_t w){
        const float a=word_radians(i16(w+0x2e));
        const float d=driving::x87_float(X87(a)-X87(std::int32_t(i16(w+0x17e)))*X87(K(0x628254u)));
        float l=wrap_449470(d);
        const std::uint32_t cls=u32(w+0x324);
        if(cls!=5u&&cls!=6u){
            l=smul(l,K(0x6282b8u));
            if(gt(l,K(0x5b43a0u)))l=K(0x5b43a0u);
            else if(gt(K(0x5b439cu),l))l=K(0x5b439cu);
        }else{
            bool fast=!jb(f32(w+0x1c4),K(0x5b4398u));
            if(fast){
                const X87 s=driving::x87_abs(X87(driving::x87_float(X87(std::int32_t(i16(w+0x162)))*X87(K(0x628258u)))));
                fast=!(!(s>=X87(K(0x628244u))));
            }
            l=smul(l,fast?K(0x5b4394u):K(0x628100u));
            if(gt(l,K(0x5b4390u)))l=K(0x5b4390u);
            else if(gt(K(0x5b438cu),l))l=K(0x5b438cu);
        }
        {
            const X87 r=X87(std::int32_t(i16(w+0x32)))*X87(K(0x628254u));
            const float wv=wrap_449470(driving::x87_float(X87(l)-r));
            l=driving::x87_float(X87(wv)*X87(K(0x628064u)));
        }
        if(X87(l)>X87(K(0x5b4388u)))l=K(0x5b4388u);
        else if(gt(K(0x5b4384u),l))l=K(0x5b4384u);
        {
            const X87 r=X87(std::int32_t(i16(w+0x32)))*X87(K(0x628254u));
            const float wv=wrap_449470(driving::x87_float(r+X87(l)));
            l=wv;
            p16(w+0x32,std::uint32_t(cvtt(driving::x87_float(X87(wv)*X87(K(0x6282c0u))))));
        }
        float v=smul(l,K(0x5b4380u));
        const float lo=K(0x5a29d8u),hi=K(0x6280c8u);
        if(gt(lo,v))v=lo;
        else if(jbe(hi,v))v=hi;
        else if(gt(lo,v))v=lo;
        p16(w+0x202,std::uint32_t(cvtt(smul(v,K(0x6282c0u)))));
    }
    // ---- course progress (services the runtime answers with the module's own leaves) ---------
    // 449860(out, query, origin, dir): projection parameter of query on the line origin + t*dir
    // (0 and out = origin for a direction under 6281F0 on every axis); out = origin + t*dir.
    X87 project_449860(std::uint32_t out,std::uint32_t q,std::uint32_t o,std::uint32_t dir){
        const X87 tiny=X87(K(0x6281f0u));
        if(!(tiny>driving::x87_abs(x(dir)))||!(tiny>driving::x87_abs(x(dir+4)))||!(tiny>driving::x87_abs(x(dir+8)))){
            Frame E(*this,0x10);
            sub_40efa0(E(0),q,o);
            const float num=driving::x87_float(dot_40efd0(E(0),dir));
            const X87 len2=(x(dir)*x(dir)+x(dir+4)*x(dir+4))+x(dir+8)*x(dir+8);   // 40F110
            const float t=driving::x87_float(X87(num)/len2);
            if(out)blend_40f180(out,o,1.0f,dir,t);
            return X87(t);
        }
        if(out)for(std::uint32_t k=0;k<0xc;k+=4)p32(out+k,u32(o+k));
        return X87(0.0f);
    }
    // 4A6CF0(car): fractional course position between the cached road quad's two edges.
    X87 progress_4a6cf0(std::uint32_t w){
        Frame E(*this,0x60);
        if(!call(0x4a3f80u,{w,u32(w+0x230)}))return X87(K(0x6280c4u));
        const std::uint32_t P=E(0x18),C=E(0xc),D=E(0x24),Q1=E(0x30),Q2=E(0x3c),V48=E(0x48),V54=E(0x54);
        for(std::uint32_t k=0;k<0xc;k+=4){p32(P+k,u32(w+0x14+k));p32(C+k,u32(w+0x1098+k));}
        pf(P+4,0.0f);
        sub_40efa0(D,w+0x10a4,w+0x1098);
        pf(D+4,0.0f);pf(C+4,0.0f);
        (void)project_449860(Q1,P,C,D);
        for(std::uint32_t k=0;k<0xc;k+=4)p32(C+k,u32(w+0x1080+k));
        sub_40efa0(D,w+0x108c,w+0x1080);
        pf(D+4,0.0f);pf(C+4,0.0f);
        (void)project_449860(Q2,P,C,D);
        const float l4=driving::x87_float(dist_40f140(P,Q1));
        const float l8=driving::x87_float(dist_40f140(P,Q2));
        sub_40efa0(V48,P,Q1);
        sub_40efa0(V54,P,Q2);
        const X87 pos=X87(std::int32_t(i16(w+0x64)));
        if(dot_40efd0(V48,V54)>X87(K619a34))return pos-X87(l4)/(X87(l8)-X87(l4));
        return X87(l4)/(X87(l8)+X87(l4))+pos;
    }
    // ---- appear records (4EF890 tables at 84BD00) ------------------------------------------
    // 477220: the speed scale of the racer config [80FB0C]+20 (1.0 when absent or 0).
    X87 speed_scale_477220(){
        const std::uint32_t cfg=u32(0x80fb0cu);
        if(cfg&&!(f32(cfg+0x20)==0.0f))return x(cfg+0x20);
        return X87(K(0x62806cu));
    }
    // 4EFB90(place): the 12-byte appear record of the place in 84CAB4.
    std::uint32_t appear_rec_4efb90(std::uint32_t pl){
        bool fallback=false;
        auto copy12=[&](std::uint32_t src){for(std::uint32_t k=0;k<0xc;k+=4)p32(0x84cab4u+k,u32(src+k));};
        if(u32(pl)!=0u){
            if(i16(pl+8)>=0x78&&call(0x44be30u,{}))copy12(0x84bd08u);
            else{copy12(0x6a5dc4u);fallback=true;}
        }else{
            const std::uint16_t si=u16(pl+8);
            for(std::uint32_t r=0x84bd08u;r<0x84bd68u;r+=0xcu){
                const std::uint16_t cx=u16(r);
                if(cx==0xffffu)break;
                if(!(cx>si)&&u16(r+0xc)>si){copy12(r);break;}
            }
        }
        p8(0x84cab6u,0);p8(0x84cab7u,0);
        bool tail=true;
        if(call(0x55a930u,{},0,0x7f9460u)||u32(0x80fb14u)){
            const std::uint32_t st=call(0x44c8d0u,{u32(pl+0xc)});
            if(u32(st+u32(pl)*4u+0x64u))return 0x84cab4u;
            if(!fallback){
                const float f=driving::x87_float(speed_scale_477220());
                p8(0x84cab8u,std::uint8_t(ftol(X87(std::int32_t(i8(0x84cab8u)))*X87(f))));
                if(X87(f)>X87(K619a34)){
                    float v=sdiv(float(std::int32_t(i8(0x84cab9u))),f);
                    if(gt(v,K(0x628104u)))v=K(0x628104u);
                    p8(0x84cab9u,std::uint8_t(cvtt(v)));
                }
            }
        }
        if(tail){p8(0x84cabau,0);p8(0x84cabbu,0);p8(0x84cabcu,0);p8(0x84cabdu,0);p8(0x84cabeu,0x64);}
        return 0x84cab4u;
    }
    // 4EF730(eax = record): the fixed appear parameters (Heart Attack stage 0x0E).
    void appear_fixed_4ef730(std::uint32_t r){
        p8(r+2,2);p8(r+3,2);
        for(std::uint32_t k=0xe;k<=0x18;k+=2)p16(r+k,0xfa);
        p16(r+0x22,0x1ae);p16(r+0x24,0x1ae);p16(r+0x26,0x1ae);
        p16(r+0x34,0x5f);p16(r+0x36,0x5f);p16(r+0x40,0x5f);
        p16(r,0);p16(r+4,0xa);p16(r+6,1);p16(r+8,0x32);p16(r+0xa,6);p16(r+0xc,0x64);
        p16(r+0x1a,0x12c);p16(r+0x1c,0x140);p16(r+0x1e,0x15e);p16(r+0x20,0x17c);
        p16(r+0x28,0x208);p16(r+0x2a,0x46);p16(r+0x2c,0x4b);p16(r+0x2e,0x50);p16(r+0x30,0x55);p16(r+0x32,0x5a);
        p16(r+0x38,0x4b);p16(r+0x3a,0x50);p16(r+0x3c,0x55);p16(r+0x3e,0x5a);p16(r+0x42,0x62);p16(r+0x44,0x62);
    }
    // 4F0030(place, record): the 0x46-byte speed record of the place in 84C6F8 (bridge 4F0030:
    // eax = [84CC18], measured: a forced record is returned as is).
    std::uint32_t speed_rec_4f0030(std::uint32_t pl,std::uint32_t rec){
        if(const std::uint32_t forced=u32(0x84cc18u))return forced;
        const std::uint32_t variant=u32(0x780258u);
        if(variant==2u&&call(0x45c440u,{})==0xeu){appear_fixed_4ef730(0x84c6f8u);return 0x84c6f8u;}
        auto copy46=[&](std::uint32_t src){for(std::uint32_t k=0;k<0x44;k+=4)p32(0x84c6f8u+k,u32(src+k));p16(0x84c73cu,u16(src+0x44));};
        if(u32(pl)!=0u){
            if(i16(pl+8)>=0x78&&call(0x44be30u,{}))copy46(0x84bf60u);
            else{
                for(std::int16_t ax=0;ax<5;++ax)if(u16(0x84bf60u+std::uint32_t(ax)*0x46u)==0xffffu){copy46(0x84c078u);break;}
            }
        }else{
            const std::int32_t bx=i16(pl+8);
            copy46(0x84bf60u);
            for(std::int16_t dx=0;dx<5;++dx){
                const std::uint32_t r=0x84bf60u+std::uint32_t(dx)*0x46u;
                const std::uint16_t cx=u16(r);
                if(cx==0xffffu)break;
                if(std::int32_t(cx)>bx)continue;
                if(std::int32_t(u16(r+0x46))>bx){copy46(r);break;}
            }
        }
        const std::uint32_t ar=rec?rec:appear_rec_4efb90(pl);
        p16(0x84c6fcu,std::int8_t(u8(ar+4))>=0xa?7u:10u);
        const float k=(variant==3u||variant==4u)?K(0x62821cu):K(0x62806cu);
        const float T[6]{K(0x5da03cu),K(0x5da038u),K(0x5da038u),K(0x5da040u),K(0x5da044u),K(0x5da048u)};
        for(std::uint32_t i=0;i<6;++i)p16(0x84c714u+i*2u,std::uint16_t(cvtt(smul(T[i],k))));
        p16(0x84c706u,0xfa);p16(0x84c708u,0xfa);p16(0x84c70au,0xfa);p16(0x84c70cu,0x122);p16(0x84c70eu,0x122);
        p16(0x84c722u,0x23);p16(0x84c726u,0x23);p16(0x84c710u,0x136);p16(0x84c6feu,1);p16(0x84c724u,0x28);
        p16(0x84c728u,0x32);p16(0x84c72au,0x3c);p16(0x84c72cu,0x5e);p16(0x84c730u,0x46);p16(0x84c734u,0x4b);p16(0x84c732u,0x4b);
        p16(0x84c736u,0x5d);p16(0x84c738u,0x5f);p16(0x84c73au,0x61);
        if(variant==2u){p8(0x84c6fau,0xa);p8(0x84c6fbu,0xa);p16(0x84c6fcu,8);}
        return 0x84c6f8u;
    }
    // ---- 4AD280: traffic / racer car init (event function 0x55) ---------------------------------
    void car_init_4ad280(std::uint32_t w){
        p32(w+0x10d0,0xffffffffu);pf(w+0x10e0,K(0x6280c4u));p32(w+0x10d4,0);
        car_reset_46f350(m,w);
        p32(w+4,(u32(w+4)&0xffffffbcu)|0xa0u);
        pf(w+0xd24,0.0f);
        p16(w+0xb64,0);
        p16(w+0xb62,u32(0x780258u)==4u?0u:0x3cu);
        for(std::uint32_t k=0;k<0xc;k+=4)p32(w+0xd28+k,u32(w+0x14+k));
        call(0x440bd0u,{u32(w),0x40u});
    }
    void unit_40a060(std::uint32_t a){
        for(std::uint32_t k:{0x38u,0x34u,0x30u,0x2cu,0x24u,0x20u,0x1cu,0x18u,0x10u,0x0cu,0x08u,0x04u})p32(a+k,0);
        for(std::uint32_t k:{0x3cu,0x28u,0x14u,0x00u})p32(a+k,0x3f800000u);
    }
    // 4704A0(car): init of the traffic cars (event function 0x59).
    void car_init_4704a0(std::uint32_t w){
        p32(w+0x10d0,0xffffffffu);pf(w+0x10e0,K(0x6280c4u));p32(w+0x10d4,0);
        car_reset_46f350(m,w);
        p32(w+4,(u32(w+4)&0xffffff9cu)|0x80u);
        const std::uint32_t c5c=u32(w+0xc5c)&0xfffe7ffeu;
        p16(w+0xb64,0);p8(w+0xb6c,0);p8(w+0xb6d,0);
        p16(w+0xb62,u32(0x780258u)==4u?0u:0x3cu);
        p32(w+0xc5c,c5c);
        unit_40a060(w+0xc90);unit_40a060(w+0xcd0);
        for(std::uint32_t k=0;k<0xc;k+=4)p32(w+0xd28+k,u32(w+0x14+k));
        call(0x440bd0u,{u32(w),0x40u});
    }
    // 4AD310(car): init of the event function 0x56 cars.
    void car_init_4ad310(std::uint32_t w){
        car_reset_46f350(m,w);
        p32(w+4,(u32(w+4)&0xfffffffcu)|0xe0u);
        p32(w+0xc5c,u32(w+0xc5c)|0x2000u);
        for(std::uint32_t k=0;k<0xc;k+=4)p32(w+0xd28+k,u32(w+0x14+k));
        pf(w+0xd24,0.0f);p16(w+0xb62,0x3c);p16(w+0xb64,0);
        call(0x440bd0u,{u32(w),0x100u});
    }
    // ---- 47E780 ----------------------------------------------------------------------------
    void control_47e780(std::uint32_t w){
        {
            const std::uint32_t flag=((u32(w)^u32(0x802e08u))&3u)?1u:0u;
            const std::uint32_t c0=u32(w+0xc);
            std::uint32_t nw=c0^(((flag<<26)^c0)&0x4000000u);
            p32(w+0xc,nw);
            if(!(nw&0x4000000u)||jb(f32(w+0x10e0),K619a34)){nw&=0xfffbffffu;p32(w+0xc,nw);}
        }
        if(u8(w+8)&1u)return;
        p32(w+0xc,u32(w+0xc)&0xffff7fffu);
        save_prev_4755c0(w);
        p32(w+0xc5c,u32(w+0xc5c)&0xffffffcfu);
        if(u32(0x78026cu)==0x10u&&std::uint8_t(call(0x456d60u,{}))>1u)lan_record_455150(w);
        {
            const std::uint32_t v=u32(0x780258u);
            if((v==3u||v==4u)&&u32(0x78026cu)==0x10u&&i32(0x680ad4u)>0){
                std::uint32_t rec=0x799d18u;
                for(std::int32_t i=0;i<i32(0x680ad4u);++i,rec+=0x3cu){
                    if((u8(0x79fb50u+std::uint32_t(i))&3u)!=2u)continue;
                    const std::uint32_t o=u32(rec);
                    if((u8(o+4)&1u)&&u32(o+0xda8)==u32(w))hit_476040(o,w,u32(o+0xdac));
                }
            }
        }
        if(!call(0x44be50u,{u32(w+0x5c)})){
            p32(w+8,u32(w+8)|1u);
            p32(w+0xc5c,u32(w+0xc5c)|0x11u);
            return;
        }
        call(0x4a4010u,{w});
        ai_47e6c0(w);
        if((call(0x4b00d0u,{})&0xffu)&&i32(w)<i32(0x680ad4u)+8)
            pf(w+0x1c4,driving::x87_float(callf(0x4b01a0u,{u32(w+0x1c4),u8(w+0xc36)})));
        if(!move_47bc00(w)){p32(w+0xc5c,u32(w+0xc5c)|0x11u);return;}
        const std::uint32_t mr=model_4866c0(i8(w+0x11));
        {
            const X87 t=fild_u32(u32(w+0x1f4))/X87(f32(0x5b3e08u+u32(w+0x324)*0x14u));
            const std::uint32_t r=ftol(t*fild_u32(u32(mr+4)));
            p32(w+0x48,r);
            if(r<0x5dcu)p32(w+0x48,0x5dc);
            if(u32(w+0x48)>0x2710u)p32(w+0x48,0x2710);
        }
        p32(w+0x34,((u8(w+4)&0x20u)&&!(u32(w+0xc5c)&0x1000u))?0x80u:0x100u);
        if(i8(w+0xd22)>0)p8(w+0xd22,u8(w+0xd22)-1u);
        if(i8(w+0xd23)>0)p8(w+0xd23,u8(w+0xd23)-1u);
        p16(w+0x40,u16(w+0x40)+std::uint16_t(cvtt(smul(sdiv(f32(w+0x1c4),f32(mr+8)),K(0x6282c0u)))));
        p16(w+0x44,u16(w+0x44)+std::uint16_t(cvtt(smul(sdiv(f32(w+0x1c4),f32(mr+8)),K(0x6282c0u)))));
        call(0x4a25f0u,{w});
        if(u32(u32(u32(model_4866c0(i8(w+0x11))))+4))shadow_470360(w);
        p32(w+0x1f4,ftol(x(w+0x1c4)*X87(K(0x5a460cu))));
        inside_470570(w);
        driving::calc_disp_steering_angle_46eb40(m.bytes(w,0x10f0));
        body_470760(w);
        call(0x4a2400u,{w});call(0x4a2650u,{w});call(0x4a2910u,{w});call(0x4a2130u,{w});call(0x4a2ee0u,{w});
        if(u8(w+4)&1u)call(0x4a45f0u,{w});
        if(!(u32(0x78026cu)==0x10u&&u32(0x780258u)==4u&&!(u8(w+4)&1u))){
            push_unit();
            rotate_y(word_radians(i16(w+0x2e)));rotate_x(word_radians(i16(w+0x2c)));rotate_z(word_radians(i16(w+0x30)));
            get_40a0d0(w+0x70);
            pop();
        }
        if(i16(w+0xb62)>0)p16(w+0xb62,u16(w+0xb62)-1u);
        if(i16(w+0xb64)>0)p16(w+0xb64,u16(w+0xb64)-1u);
    }
    // 47E6C0(eax = car): drive the car (AI) unless parked.
    void ai_47e6c0(std::uint32_t w){
        if(u32(0x80fb14u)&&std::int16_t(call(0x49b2d0u,{}))>0x3c&&(u8(w+4)&0x20u))return;
        const std::uint32_t v=u32(0x780258u);
        if(v==5u){
            if((u32(w+0xc5c)&0x40000u)&&!g_46c3b0())return;
        }else if(v==2u){
            if(u32(w+0x324)==6u&&!call(0x45b510u,{}))return;
            if((u32(w+0xc5c)&0x40000u)&&!call(0x45d820u,{}))return;
        }
        const std::uint32_t f=u32(w+4);
        if((f&0x40u)&&(f&0x20u)){
            const std::uint32_t pl=player();
            if(u32(w+0x68)!=u32(pl+0x68))return;
            if(u32(w+0x5c)!=u32(pl+0x5c))return;
            if(!u32(w+0x5c)&&std::int32_t(i16(w+0x64))>=end_43d470(0))return;
        }
        side_470930(w);
        drive_47e4a0(w);
    }
    // 47E4A0(car): the driving decisions of one car.
    void drive_47e4a0(std::uint32_t w){
        if(u32(0x780258u)==4u)return;
        {
            const std::uint32_t pl=player();
            std::uint8_t lane=u8(pl+0x280);p8(pl+0x66,lane);
            if(u32(pl+0x5c)&&u32(pl+0x60)==0x65u&&gt(K(0x6282d4u),ssub(f32(pl+0x268),f32(pl+0x264))))p8(pl+0x66,std::uint8_t(lane+3u));
        }
        const std::uint32_t kind=u32(0x780258u)==2u?call(0x45c440u,{}):0x1au;
        const std::uint32_t rec=call(0x4efb90u,{w+0x5c});
        const std::uint32_t rec2=call(0x4f0030u,{w+0x5c,rec});
        if(u32(0x80fb14u)&&(u8(w+4)&0x20u))trim_474d30(w);
        if(kind==0x14u){chase_4739f0(w,rec2,rec);return;}
        if(call(0x55a930u,{},0,0x7f9460u)&&g_46c3c0()){chase_4739f0(w,rec2,rec);return;}
        bool stop=kind==0x16u||kind==9u;
        if(!stop&&u32(0x7f95fcu)&&!(u32(w+4)&0x84000000u)&&!u8(w+0xb6c))stop=true;
        if(stop){
            stop_473460(rec2,w);
            if(u8(w+0xb6c))recover_473f30(w);
            return;
        }
        const bool scripted=(u32(w+0xc)&0x4000000u)&&!jb(f32(w+0x10e0),K619a34);
        if(u32(0x80fb14u)&&(u8(w+4)&0x20u)){
            if(scripted)script_472090(u32(w+0x10e0),u32(w+0x10d4),w+0x10d8,w);
            else racer_47a7c0(w,rec2);
        }else if(u32(w+0x324)==6u){
            if(scripted)script_472090(u32(w+0x10e0),u32(w+0x10d4),w+0x10d8,w);
            else bus_472fe0(w,rec2);
        }else{
            if(scripted)script_472090(u32(w+0x10e0),u32(w+0x10d4),w+0x10d8,w);
            else traffic_472450(w,rec,rec2);
        }
        if(u8(w+0xb6c)){recover_473f30(w);return;}
        lanes_47e110(w,rec,rec2);
    }
    // ---- speed and lane changes ----------------------------------------------------------------
    // 472090(rate; eax = accel table, edi = target {flag, flag, -, speed}, esi = car): move +1C4 toward the target speed.
    void script_472090(std::uint32_t rate_bits,std::uint32_t tab,std::uint32_t tgt,std::uint32_t w){
        const float rate=from_bits(rate_bits);
        auto racer_accel=[&]()->bool{
            if(!(u32(0x80fb14u)&&(u8(w+4)&0x20u)&&u32(w+0x208)))return false;
            const float xm=call(0x44fdf0u,{})<0x5e1u?f32(w+0xbcc):K(0x62806cu);
            pf(w+0x1c4,sadd(smul(smul(smul(f32(0x5b40c0u+u32(w+0x208)*4u),xm),rate),K(0x5b40dcu)),f32(w+0x1c4)));
            return true;
        };
        const float s=f32(w+0x1c4);
        bool accel=gt(f32(tgt+4),smul(s,K(0x5b43b4u)));
        if(!accel&&gt(f32(tgt+4),s)&&u8(tgt)==1u)accel=true;
        if(accel){
            if(!racer_accel())pf(w+0x1c4,sadd(smul(f32(tab+0xc),rate),f32(w+0x1c4)));
            if(gt(f32(w+0x1c4),f32(tgt+4)))p32(w+0x1c4,u32(tgt+4));
            return;
        }
        if(gt(f32(tgt+4),s)){
            if(!racer_accel())pf(w+0x1c4,sadd(smul(f32(tab+4),rate),f32(w+0x1c4)));
            if(gt(f32(w+0x1c4),f32(tgt+4)))p32(w+0x1c4,u32(tgt+4));
            return;
        }
        const bool hard=(gt(smul(s,K(0x62821cu)),f32(tgt+4))&&!u8(tgt+1))||u8(tgt)==1u;
        const float v=ssub(f32(w+0x1c4),smul(f32(tab+(hard?0x10u:8u)),rate));
        pf(w+0x1c4,v);
        if(gt(f32(tgt+4),v))p32(w+0x1c4,u32(tgt+4));
    }
    // 473460(ecx = speed record, esi = car): the record speed of the car class (+ the network bonus).
    void stop_473460(std::uint32_t rec2,std::uint32_t w){
        p32(w+0x38,0);
        pf(w+0x1c4,smul(float(i16(rec2+u32(w+0x324)*2u+0xe)),K(0x5b43a4u)));
        if(u32(0x7f95fcu))pf(w+0x1c4,sadd(smul(float(std::int32_t(net_slot_46c4e0(i32(w)-8))),K(0x5b43a4u)),f32(w+0x1c4)));
    }
    // 473620(esi = car): the lateral offset of the car's lane sample in the car frame.
    float offset_473620(std::uint32_t w){
        Frame E(*this,0x30);
        const std::uint32_t kind=u32(w+0x5c);
        if(!sample_470120(kind,u8(w+0xd20),i8(w+0x66),i16(w+0x64),E(0x10)))return 0.0f;   // 473634 bridge: movzx eax,byte [esi+0xD20]
        push_load(call(0x44bed0u,{kind}));
        point_40a7d0(E(0x10),E(0x10));
        pop();
        sub_40efa0(E(4),w+0x14,E(0x10));
        push_unit();
        rotate_y(word_radians(-(std::int32_t(i16(w+0xc2c))+std::int32_t(i16(w+0x2e)))));
        point_40a7d0(E(4),E(4));
        pop();
        return f32(E(4));
    }
    // 473F30(eax = car): recover after a hit (+B58 0xB / 0xC), then back to a lane.
    void recover_473f30(std::uint32_t w){
        const std::uint32_t k=u32(w+0xb58);
        if(k==0xbu){
            if(gt(f32(w+0x268),K619a34)&&gt(f32(w+0xb08),K(0x5b43dcu)))pf(w+0xb08,ssub(f32(w+0xb08),K(0x5b0068u)));
        }else if(k==0xcu){
            if(gt(0.0f,f32(w+0x264))&&gt(K(0x6281e8u),f32(w+0xb08)))pf(w+0xb08,sadd(f32(w+0xb08),K(0x5b0068u)));
        }
        p16(w+0xb60,u16(w+0xb60)-1u);
        if(i16(w+0xb60)>0)return;
        p16(w+0xb60,0);p32(w+0xb58,0);p8(w+0xb6c,0);
        const std::uint32_t cls=u32(w+0x324);
        if(cls!=0u&&cls!=1u&&cls!=2u&&u8(0x800d4du)<=1u)return;
        p8(w+0x66,std::uint8_t(u8(w+0x66)+std::uint8_t(cvtt(smul(f32(w+0xb08),K(0x628088u))))));
        std::int8_t lo,hi;
        if(u32(w+0x5c)){
            if(u32(w+0x60)==0x64u){lo=0;hi=2;}else{lo=3;hi=5;}
        }else{
            const std::uint32_t v=call(0x44f0f0u,{call(0x44dc50u,{u32(w+0x68)}),u16(w+0x64)});
            if(!v){lo=0;hi=5;}
            else if(i8(w+0x66)>=3){lo=3;hi=5;}
            else{lo=0;hi=2;}
        }
        if(i8(w+0x66)<lo)p8(w+0x66,std::uint8_t(lo));
        if(i8(w+0x66)>hi)p8(w+0x66,std::uint8_t(hi));
        pf(w+0xb08,offset_473620(w));
    }
    // 471380(ecx = car, al = lane, speed record): 0 when the player blocks the lane change.
    std::uint32_t clear_471380(std::uint32_t w,std::int8_t lane,std::uint32_t rec2){
        Frame F(*this,0x10);
        float l=smul(float(lane),K(0x6280c0u));
        if(u32(w+0x5c)&&i8(w+0x66)>=3&&gt(K(0x6282d4u),ssub(f32(w+0x268),f32(w+0x264))))l=ssub(l,K(0x6281e8u));
        const std::uint32_t pl=player();
        if(i8(pl+0x66)<0)return 1;
        if(std::int32_t(ftol(driving::x87_abs(X87(l)-x(pl+0x268))*X87(K(0x628088u))))>1)return 1;
        const X87 a=driving::x87_abs(X87(l)-x(pl+0x268));
        const X87 b=driving::x87_abs(x(w+0x268)-x(pl+0x268));
        if(!(b>a))return 1;
        l=driving::x87_float(ahead_46e250(pl,w));
        float lim=float(i16(rec2+8));
        if((u8(w+4)&0x20u)&&u32(0x80fb14u))lim=K(0x628248u);
        if(gt(l,K(0x6280c0u))&&gt(lim,l))return 0;
        return 1;
    }
    // 4714D0(esi = car, eax = other, lo, hi): the next lane one step away from other.
    std::uint8_t step_4714d0(std::uint32_t w,std::uint32_t o,std::int8_t lo,std::int8_t hi){
        std::int8_t bl=i8(w+0x66);
        auto apart=[&](std::uint32_t c){return ftol(driving::x87_abs(x(w+0x268)-x(c+0x268))*X87(K(0x628088u)));};
        if(apart(o)==0u){
            const std::uint32_t pl=player();
            if(apart(pl)==0u){
                const std::int32_t mid=(std::int32_t(lo)+std::int32_t(hi))/2;
                return std::uint8_t(std::int32_t(bl)>mid?bl-1:bl+1);
            }
            if(gt(f32(w+0x268),f32(pl+0x268)))return std::uint8_t(bl>=hi?bl-1:bl+1);
            return std::uint8_t(bl>lo?bl-1:bl+1);
        }
        if(gt(f32(w+0x268),f32(o+0x268)))return std::uint8_t(bl>=hi?bl:bl+1);
        return std::uint8_t(bl<=lo?bl:bl-1);
    }
    // 471590(esi = car, edi = appear record, lo, hi): the nearest open lane (FF: none).
    std::uint8_t open_lane_471590(std::uint32_t w,std::uint32_t rec,std::int8_t lo,std::int8_t hi){
        std::int8_t dl=lo,cl=i8(w+0x66),bl=hi,a=-1,al=-1;
        if(cl>lo)dl=std::int8_t(cl-1);
        if(cl<hi){cl=std::int8_t(cl+1);bl=cl;}
        const bool racer=u32(0x80fb14u)&&(u8(w+4)&0x20u);
        for(unsigned guard=0;;++guard){
            if(guard>0x200)throw std::logic_error("race traffic: 471590 found no open lane (the original loops forever)");
            if(dl<lo&&bl>hi){cl=a;break;}
            if(racer||!((1u<<(std::uint8_t(dl)&31u))&u8(rec+0xb)))al=dl;
            if(!racer&&((1u<<(std::uint8_t(bl)&31u))&u8(rec+0xb)))cl=a;
            else{cl=bl;a=cl;}
            if(al>=0||cl>=0)break;
            if(dl>lo)--dl;
            if(bl<hi)++bl;
        }
        if(al>=0){
            if(cl<0)return std::uint8_t(al);
            if(gt(f32(player()+0x268),f32(w+0x268)))return std::uint8_t(al);
            return std::uint8_t(cl);
        }
        return std::uint8_t(cl>=0?cl:dl);
    }
    // 4736D0(eax = car, appear record, speed record): run the lane change (+B14 frames, +B08 offset).
    void lane_change_4736d0(std::uint32_t w,std::uint32_t rec,std::uint32_t rec2){
        auto clear=[&]{p32(w+0xb14,0);p16(w+0xb50,0);p32(w+0xc5c,u32(w+0xc5c)&0xfffff9ffu);};
        if(u32(0x780258u)==2u){
            const std::uint32_t k=call(0x45c440u,{});
            if(k==2u||k==6u||k==0xbu){clear();return;}
        }else if(call(0x55a930u,{},0,0x7f9460u)&&g_46c3e0()&&!(u32(w+4)&0x84000000u)&&!u8(w+0xb6c)){clear();return;}
        const std::int32_t total=i32(w+0xb18),left=i32(w+0xb14);
        const float t=sdiv(float(total-left),float(total));
        p16(w+0xb50,u16(w+0xb50)+1u);
        const std::uint32_t f=u32(w+0xc5c);
        std::int8_t bl=i8(w+0x66);
        const float k0=smul(t,K(0x5b43ccu));
        p32(w+0xb14,std::uint32_t(left-1));
        if(f&0x400u){
            ++bl;
            pf(w+0xb08,sadd(smul(ssub(K(0x62806cu),t),f32(w+0xb08)),smul(k0,t)));
            if(bl>=6)bl=5;
        }else if(f&0x200u){
            --bl;
            pf(w+0xb08,ssub(smul(ssub(K(0x62806cu),t),f32(w+0xb08)),smul(k0,t)));
            if(bl<0)bl=0;
        }
        bool tail_only=false;
        if(u32(0x780258u)==2u&&call(0x45c440u,{})==0x14u)tail_only=true;
        else if(call(0x55a930u,{},0,0x7f9460u)&&g_46c3c0())tail_only=true;
        if(!tail_only){
            if(i8(w+0x280)<0){
                if(!(u32(0x80fb14u)&&!(u8(w+4)&0x20u)))clear();
            }else{
                const bool racer=u32(0x80fb14u)&&(u8(w+4)&0x20u);
                if(!racer&&((1u<<(std::uint8_t(bl)&31u))&u8(rec+0xb)))clear();
                else if(!lane_bit_46c7e0(w,rec,u8(w+0x66))){
                    if(!clear_471380(w,bl,rec2))p32(w+0xb14,u32(w+0xb14)+1u);
                    else if(u8(w+6)&1u){
                        const std::uint32_t o=work(u32(w+0xdd4));
                        const std::uint32_t g=u32(w+0xc5c);
                        if(((g&0x200u)&&gt(f32(w+0x268),f32(o+0x268)))||((g&0x400u)&&gt(f32(o+0x268),f32(w+0x268)))){
                            p32(w+0xb14,0);p16(w+0xb50,0);p32(w+0xc5c,g&0xfffff9ffu);
                        }
                    }else{
                        const std::uint32_t kind=u32(w+0x5c);
                        bool done=false;
                        if(!kind&&i16(w+0x64)<0xa)done=true;
                        else if(kind&&std::int32_t(i16(w+0x64))>=end_43d470(kind)-5)done=true;
                        if(done)clear();
                    }
                }
            }
        }
        if(i32(w+0xb14)>0)return;
        {
            const std::uint32_t g=u32(w+0xc5c);
            if(g&0x600u){p8(w+0x66,std::uint8_t(bl));p32(w+0xc5c,g&0xfffff9ffu);}
        }
        p32(w+0xb14,0);p16(w+0xb50,0);
        const float v=offset_473620(w);
        pf(w+0xb08,v);
        if((u8(w+4)&0x20u)&&u32(0x80fb14u))return;
        if(gt(v,K(0x6280c0u)))pf(w+0xb08,K(0x6280c0u));
        if(gt(K(0x5b43a8u),f32(w+0xb08)))pf(w+0xb08,K(0x5b43a8u));
    }
    // 4739F0(eax = car, edi = speed record, ebx = appear record): weave between the lanes (variant event 0x14).
    void chase_4739f0(std::uint32_t w,std::uint32_t rec2,std::uint32_t rec){
        if(f32(w+0x2c8)==K619a34){
            if(u32(0x7f95fcu)&&!(u32(w+4)&0x84000000u)&&!u8(w+0xb6c))stop_473460(rec2,w);
            else pf(w+0x1c4,K(0x5b43d0u));
        }
        const std::uint32_t left=u32(w+0xb14);
        if(!left){
            const std::int16_t c0=i16(w+0xb6e);
            if(c0){
                if(std::abs(std::int32_t(c0))<0x266)p16(w+0xb6e,0);
                else p16(w+0xb6e,std::uint16_t(c0+(c0>0?-0x266:0x266)));
            }
            if(std::int32_t(i16(w+0xb70))%0xb4!=0){p16(w+0xb70,u16(w+0xb70)+1u);return;}
            if(!jb(f32(w+0x264),K(0x5b43a8u))){lane_left_470a30(w,0xb4,rec);p32(w+0xb58,0x14);}
            else if(!jb(K(0x6280c0u),f32(w+0x268))){lane_right_470ab0(w,0xb4,rec);p32(w+0xb58,0x15);}
            else if(u32(w+0xb58)==0x14u)lane_left_470a30(w,0xb4,rec);
            else lane_right_470ab0(w,0xb4,rec);
            p16(w+0xb70,u16(w+0xb70)+1u);
            return;
        }
        const std::uint32_t f=u32(w+0xc5c);
        bool done=false;
        if(f&0x200u){
            const std::uint16_t a=u16(w+0xb6e);
            if(std::int16_t(a)>std::int16_t(0xc000)){p16(w+0xb6e,a-0x266u);done=true;}
        }
        if(!done&&(f&0x400u)){
            const std::uint16_t a=u16(w+0xb6e);
            if(std::int16_t(a)<0x4000)p16(w+0xb6e,a+0x266u);
        }
        p32(w+0xb14,left-1u);
        lane_change_4736d0(w,rec,rec2);
        p16(w+0xb70,u16(w+0xb70)+1u);
    }
    // ---- lateral offset (+B08) control -------------------------------------------------------------
    // 473B90(edi = car): move away from a close neighbour (+314 / +318); 1 when it did.
    std::uint32_t keep_473b90(std::uint32_t w){
        Frame F(*this,0x10);
        pf(F(0xc),K(0x6282ccu));
        std::uint32_t near=0;
        float d=f32(F(8));                                            // uninitialised local when there is no +314 car
        const std::uint32_t prev=u32(w+0x314);
        if(prev){
            d=f32(0x802af0u+(u32(prev)+u32(w)*24u)*4u);
            if(gt(K(0x628100u),d)){pf(F(8),driving::x87_float(ahead_46e250(w,prev)));d=f32(F(8));}
            if(gt(0.0f,d))d=smul(d,K(0x6280c4u));
            near=prev;pf(F(0xc),d);
        }
        const std::uint32_t next=u32(w+0x318);
        if(next){
            d=f32(0x802af0u+(u32(next)+u32(w)*24u)*4u);
            if(gt(K(0x628100u),d)){pf(F(8),driving::x87_float(ahead_46e250(next,w)));d=f32(F(8));}
            if(gt(0.0f,d))d=smul(d,K(0x6280c4u));
            if(gt(f32(F(0xc)),d))near=next;
        }
        if(!near)return 0;
        if(!gt(K(0x6280c0u),d))return 0;
        if(!(X87(K(0x6280b0u))>driving::x87_abs(x(w+0x268)-x(near+0x268))))return 0;
        if(gt(f32(w+0x268),f32(near+0x268))){
            if(gt(K(0x62806cu),f32(w+0xb08)))pf(w+0xb08,sadd(f32(w+0xb08),K(0x628128u)));
            return 1;
        }
        if(gt(f32(w+0xb08),K(0x6280c4u)))pf(w+0xb08,ssub(f32(w+0xb08),K(0x628128u)));
        return 1;
    }
    // 473D10(eax = car, player, speed record): keep clear of the player ahead.
    void dodge_473d10(std::uint32_t w,std::uint32_t pl,std::uint32_t rec2){
        Frame E(*this,0x20);
        if(diff_46f990(w+0x5c,pl+0x5c)>=std::int32_t(i16(rec2+4)))return;
        pf(E(0x1c),driving::x87_float(ahead_46e250(pl,w)));
        float d=ssub(f32(E(0x1c)),float(i16(rec2+8)));
        if(gt(0.0f,d))d=0.0f;
        else if(gt(d,K(0x5afd30u)))return;
        const X87 v=driving::x87_abs(x(w+0x268)-x(pl+0x268));
        float push=smul(ssub(K(0x5afd30u),d),K(0x6280f8u));
        pf(E(0x1c),driving::x87_float(v));
        const float s=X87(f32(E(0x1c)))>X87(K(0x62806cu))?K(0x62806cu):f32(E(0x1c));
        push=smul(push,s);
        pf(E(0x10),push);
        const std::uint32_t k=u32(w+0x5c);
        bool slow=false;
        if(k){if(!(i16(w+0x64)>0xf))slow=true;}
        else if(std::int32_t(i16(w+0x64))>=end_43d470(0)-0xf)slow=true;
        if(!slow&&i8(w+0x280)<0)slow=true;
        if(slow){pf(w+0xb08,smul(f32(w+0xb08),K(0x5b43d8u)));return;}
        lane_range_46d0c0(w,E(0x1c),E(0x18));
        const float k3=K(0x5b43d8u),k2=K(0x628128u);
        if(gt(f32(w+0x268),f32(pl+0x268))){
            const float b=f32(w+0xb08);
            bool add=false;
            if(gt(b,K(0x5b43d4u))){
                if(i8(w+0x66)<i8(E(0x18))&&gt(push,b))add=true;
                else if(gt(smul(push,K(0x628064u)),f32(w+0xb08)))add=true;
            }
            pf(w+0xb08,add?sadd(f32(w+0xb08),k2):smul(f32(w+0xb08),k3));
        }
        if(!gt(f32(pl+0x268),f32(w+0x268)))return;
        bool sub=false;
        if(gt(k2,f32(w+0xb08))){
            if(i8(w+0x66)>i8(E(0x1c))&&gt(f32(w+0xb08),ssub(0.0f,push)))sub=true;
            else if(gt(f32(w+0xb08),smul(push,K(0x6281b8u))))sub=true;
        }
        pf(w+0xb08,sub?ssub(f32(w+0xb08),k2):smul(f32(w+0xb08),k3));
    }
    // 47E110(ecx = car, eax = appear record, esi = speed record): lane changes and the +B08 offset.
    void lanes_47e110(std::uint32_t w,std::uint32_t rec,std::uint32_t rec2){
        Frame E(*this,0x20);
        const std::uint32_t pl=player();
        const float b0=f32(w+0xb08);
        pf(E(0xc),b0);
        const std::int8_t lane0=i8(w+0x66);
        if(i32(w+0xb14)>0){
            lane_change_4736d0(w,rec,rec2);
            if(!(u32(w+0xc)&0x4000000u))keep_473b90(w);
        }else if(!(u32(w+0xc)&0x4000000u)){
            decide_47dc10(w,rec,rec2);
            if(!keep_473b90(w))dodge_473d10(w,pl,rec2);
        }
        const float one=K(0x62806cu);
        if(u32(0x80fb14u)&&!(u8(w+4)&0x20u)){
            if(gt(one,f32(w+0x268)))pf(w+0xb08,sadd(ssub(one,f32(w+0x268)),f32(w+0xb08)));
            else if(gt(f32(w+0x264),K(0x6280c4u)))pf(w+0xb08,ssub(f32(w+0xb08),sadd(f32(w+0x264),one)));
        }
        if((u8(w+4)&0x20u)&&u32(0x80fb14u)){
            if(lane0!=i8(w+0x66))return;
            if(i32(w+0xb14)>0)return;
            const float target=sadd(f32(w+0xbe4),f32(w+0xb10));
            pf(E(0x10),target);pf(w+0xb10,target);
            if(target!=0.0f){
                const X87 v=((X87(K(0x5b436cu))-driving::x87_abs(x(w+0xb54)))*X87(K(0x599434u)))*X87(K(0x62813cu));
                pf(E(0xc),driving::x87_float(v));
                if(X87(K(0x5b4484u))>v)pf(E(0xc),K(0x5b4484u));
                else if(gt(f32(E(0xc)),one))pf(E(0xc),one);
                const float t=driving::x87_float((driving::x87_abs(x(w+0xb08)-X87(target))*x(E(0xc)))*x(w+0xbe8));
                pf(E(0xc),t);
                if(t!=f32(w+0xbec)){
                    if(gt(t,f32(w+0xbec))){
                        const float v2=sadd(smul(f32(w+0xbe8),K(0x6281c0u)),f32(w+0xbec));
                        pf(w+0xbec,v2);if(gt(v2,t))pf(w+0xbec,t);
                    }else{
                        const float v2=ssub(f32(w+0xbec),smul(f32(w+0xbe8),K(0x6281c0u)));
                        pf(w+0xbec,v2);if(gt(t,v2))pf(w+0xbec,t);
                    }
                }
                if(gt(f32(w+0xbec),K(0x628064u)))pf(w+0xbec,K(0x628064u));
                const float b=f32(w+0xb08);
                if(target==b)return;
                if(gt(target,b)){
                    const float n=sadd(b,f32(w+0xbec));pf(w+0xb08,n);
                    if(gt(n,target))pf(w+0xb08,target);
                }else{
                    const float n=ssub(b,f32(w+0xbec));pf(w+0xb08,n);
                    if(gt(target,n))pf(w+0xb08,target);
                }
                return;
            }
            if(0.0f!=f32(w+0xbec)){
                if(gt(0.0f,f32(w+0xbec))){
                    const float v2=sadd(smul(f32(w+0xbe8),K(0x6281c0u)),f32(w+0xbec));
                    pf(w+0xbec,v2);if(gt(v2,0.0f))pf(w+0xbec,0.0f);
                }else{
                    const float v2=ssub(f32(w+0xbec),smul(f32(w+0xbe8),K(0x6281c0u)));
                    pf(w+0xbec,v2);if(gt(0.0f,v2))pf(w+0xbec,0.0f);
                }
            }
            float s=sdiv(K(0x5b4480u),f32(w+0xbe8));
            if(gt(s,K(0x5a29e8u)))s=K(0x5a29e8u);
            pf(w+0xb08,smul(s,b0));
            return;
        }
        if(lane0!=i8(w+0x66))return;
        if(i8(w+0x280)<0)return;
        const float k=K(0x6280ecu);
        if(gt(b0,0.0f)){const float v=ssub(b0,k);if(gt(v,f32(w+0xb08)))pf(w+0xb08,v);}
        if(gt(0.0f,b0)){const float v=sadd(b0,k);if(gt(f32(w+0xb08),v))pf(w+0xb08,v);}
    }
    // ---- lane decisions (47DC10) ---------------------------------------------------------------------
    // lateral position of a car's lane centre as 471010 / 47DC10 read it (+268, shifted on the right road half).
    float lane_pos(std::uint32_t c){
        float v=f32(c+0x268);
        if(u32(c+0x5c)&&i8(c+0x66)>=3&&gt(K(0x6282d4u),ssub(f32(c+0x268),f32(c+0x264))))v=sadd(v,K(0x6281e8u));
        return v;
    }
    // 4734C0(al = lane): -1 when the lanes right of `lane` carry more traffic (800D2C weights) than the left ones, else 1.
    std::uint8_t busier_4734c0(std::int8_t lane){
        std::uint32_t right=0,left=0;
        if(lane<6)for(std::uint8_t n=std::uint8_t(6-lane),k=0;k<n;++k)right+=u16(0x800d2cu+std::uint32_t(std::int32_t(lane)+k)*2u);
        const float a=sdiv(float(std::int32_t(right)),float(6-std::int32_t(lane)));
        if(lane>=0)for(std::uint8_t n=std::uint8_t(lane+1),k=0;k<n;++k)left+=u16(0x800d2cu+std::uint32_t(std::int32_t(lane)-k)*2u);
        const float b=sdiv(float(std::int32_t(left)),float(std::int32_t(lane)+1));
        return gt(a,b)?0xffu:1u;
    }
    // 473540(eax = course distance, ecx = speed record, edi = place copy, esi = car; player, lo, hi): bus lane choice.
    void bus_lane_473540(std::int32_t d,std::uint32_t rec2,std::uint32_t place,std::uint32_t w,std::uint32_t pl,std::int8_t lo,std::int8_t hi){
        if(d>std::int32_t(i16(rec2+6))){
            const float l=(std::int32_t(u32(w))%2)!=0?K(0x6280c0u):K(0x628248u);
            if(!(X87(K(0x6282bcu))>driving::x87_abs(x(w+0xb54))))return;
            if(gt(0.0f,f32(w+0xb54))&&!jb(f32(w+0x268),l)){p8(place+0xa,u8(place+0xa)-1u);return;}
            if(gt(f32(w+0xb54),0.0f)&&!jb(ssub(0.0f,l),f32(w+0x264)))p8(place+0xa,u8(place+0xa)+1u);
            return;
        }
        const bool free=d>=0&&!(u8(w+6)&1u);
        if(gt(f32(w+0x268),f32(pl+0x268))){
            if(free){p8(place+0xa,u8(place+0xa)+1u);return;}
            const std::int8_t a=i8(w+0x66);
            p8(w+0x66,std::uint8_t(a<hi?a+1:a-1));
            return;
        }
        if(free){p8(place+0xa,u8(place+0xa)-1u);return;}
        const std::int8_t a=i8(w+0x66);
        p8(w+0x66,std::uint8_t(a>lo?a-1:a+1));
    }
    // 471010(edi = car, dl = lane; appear record, speed record): 1 when moving to `lane` is safe.
    std::uint32_t safe_471010(std::uint32_t w,std::int8_t lane,std::uint32_t rec,std::uint32_t rec2){
        Frame L(*this,0x20);
        const std::uint32_t kind=u32(w+0x5c);
        float l0=smul(float(lane),K(0x6280c0u));
        if(kind&&i8(w+0x66)>=3&&gt(K(0x6282d4u),ssub(f32(w+0x268),f32(w+0x264))))l0=ssub(l0,K(0x6281e8u));
        pf(L(0xc),l0);
        const bool racer=u32(0x80fb14u)&&(u8(w+4)&0x20u);
        auto blocked=[&](std::int32_t k){return ((1u<<(std::uint32_t(k)&31u))&u8(rec+0xb))!=0u;};
        if(!racer&&blocked(u8(w+0x66))){
            std::int8_t lo,hi;
            if(kind){if(u32(w+0x60)==0x64u){lo=0;hi=2;}else{lo=3;hi=5;}}
            else{
                const std::uint32_t v=call(0x44f0f0u,{call(0x44dc50u,{u32(w+0x68)}),u16(w+0x64)});
                if(!v){lo=0;hi=5;}
                else if(i8(w+0x66)>=3){lo=3;hi=5;}
                else{lo=0;hi=2;}
            }
            std::int8_t cl=i8(w+0x66);
            const bool racer2=u32(0x80fb14u)&&(u8(w+4)&0x20u);
            if(gt(l0,f32(w+0x268))){
                for(++cl;cl<=hi;++cl){if(racer2||!blocked(cl))return 1;}
                return 0;
            }
            for(--cl;cl>=lo;--cl){if(racer2||!blocked(cl))return 1;}
            return 0;
        }
        if(!racer&&blocked(lane))return 0;
        pf(L(0x14),K(0x5a29ecu));
        const std::uint32_t pl=player();
        for(std::uint32_t nx=u32(w+0x318);nx;nx=u32(nx+0x318)){
            const std::uint32_t k=u32(w+0x5c);
            if(k&&(std::int32_t(i8(nx+0x66))*2)/6!=(std::int32_t(i8(w+0x66))*2)/6){
                if(std::int32_t(i16(w+0x64))<end_43d470(k)-0xf)continue;
            }
            const std::int32_t flag=(nx==pl||u32(w+0x324)==0u)?1:0;
            const X87 a=driving::x87_abs(x(L(0xc))-x(nx+0x268));
            if(X87(flag)*X87(K(0x6280c0u))>=a){
                const X87 b=driving::x87_abs(x(w+0x268)-x(nx+0x268));
                if(b>a){
                    const X87 t=ahead_46e250(nx,w);
                    pf(L(0x10),driving::x87_float(t));
                    if(X87(f32(L(0x14)))>t&&gt(f32(nx+0x1c4),f32(w+0x1c4)))pf(L(0x14),f32(L(0x10)));
                }
            }
            if(nx==pl){const std::uint32_t v=u32(0x780258u);if(v!=3u&&v!=4u)break;}
        }
        for(std::uint32_t pv=u32(w+0x314);pv;pv=u32(pv+0x314)){
            if(ftol(driving::x87_abs(x(L(0xc))-x(pv+0x268))*X87(K(0x628088u)))!=0u)continue;
            pf(L(0x10),driving::x87_float(ahead_46e250(w,pv)));
            if(X87(f32(L(0x10)))>X87(std::int32_t(i16(rec2+8))))break;
            if(gt(f32(L(0x14)),f32(L(0x10)))&&gt(f32(w+0x1c4),f32(pv+0x1c4)))pf(L(0x14),f32(L(0x10)));
        }
        const float g=f32(L(0x14));
        if(gt(g,K(0x6280c0u))&&gt(float(i16(rec2+8)),g))return 0;
        return 1;
    }
    // 47DC10(eax = car; appear record, speed record): pick a lane change for a traffic car.
    void decide_47dc10(std::uint32_t w,std::uint32_t rec,std::uint32_t rec2){
        Frame E(*this,0x40);
        const std::uint32_t pl=player();
        const std::uint32_t P=E(0x20);
        for(std::uint32_t k=0;k<0x10;k+=4)p32(P+k,u32(w+0x5c+k));
        if(u32(0x780258u)==2u){
            const std::uint32_t k=call(0x45c440u,{});
            if(k==2u||k==6u||k==0xbu)return;
        }
        const std::int32_t d=diff_46f990(P,pl+0x5c);
        if(d<std::int32_t(i16(rec2+6))){
            const std::uint32_t v=u32(0x780258u);
            if((v==3u||v==4u)&&std::uint8_t(call(0x456d60u,{}))>1u)return;
        }
        if(u32(0x80fb14u)&&(u8(w+4)&0x20u)){
            if(u32(w+0xc)&0x4000000u)return;
            racer_lanes_47ad30(w);
            return;
        }
        std::int8_t lo,hi;
        if(!u32(P)){
            const std::uint32_t v=call(0x44f0f0u,{call(0x44dc50u,{u32(w+0x68)}),u16(w+0x64)});
            if(!v){lo=0;hi=5;}
            else if(i8(w+0x66)>=3){lo=3;hi=5;}
            else{lo=0;hi=2;}
        }else if(u32(w+0x60)==0x64u){lo=0;hi=2;}
        else{lo=3;hi=5;}
        if(!(u32(0x80fb14u)&&(u8(w+4)&0x20u))){
            const std::int8_t bl=i8(w+0x66);
            if((1u<<(std::uint8_t(bl)&31u))&u8(rec+0xb)){
                const std::int8_t al=std::int8_t(open_lane_471590(w,rec,lo,hi));
                if(bl<al)lane_right_470ab0(w,0xb4,rec);
                else lane_left_470a30(w,0xb4,rec);
                return;
            }
        }
        std::int8_t cl;
        const std::uint32_t cls=u32(w+0x324);
        auto faster=[&](std::uint32_t o){return o==pl||gt(f32(w+0x1c4),f32(o+0x1c4));};
        if(u32(P)){
            cl=i8(P+0xa);
            if(i16(w+0x64)>0xf&&std::int32_t(i16(w+0x64))<end_43d470(u32(w+0x5c))-0xf&&(cls==0u||cls==1u||cls==2u)){
                if(std::int32_t(cl)%3==1){
                    const std::uint32_t nx=u32(w+0x318);
                    if(nx&&faster(nx))cl=u32(w+0x60)!=0x64u?5:2;
                }
            }
        }else if(cls==6u){
            bus_lane_473540(d,rec2,P,w,pl,lo,hi);
            cl=i8(P+0xa);
        }else{
            cl=i8(P+0xa);
            const std::uint32_t nx=u32(w+0x318);
            if(nx&&i16(w+0x64)>0xf&&faster(nx)&&(cls==4u||cls==3u)&&i32(0x804350u)>0x3c){
                const std::int8_t bl=i8(w+0x66);
                if(bl>=0){
                    const std::int8_t r=std::int8_t(busier_4734c0(bl));
                    if(r>0){if(bl<hi)cl=std::int8_t(i8(P+0xa)+1);}
                    else if(r<0&&bl>0)cl=std::int8_t(i8(P+0xa)-1);
                }
            }
        }
        if(cl<lo)cl=lo;
        else if(cl>hi)cl=hi;
        pf(E(0x14),lane_pos(w));
        p32(E(0x18),std::uint32_t(std::int32_t(cl)));
        pf(E(0x10),driving::x87_float(X87(std::int32_t(cl))*X87(K(0x6280c0u))));
        if(!(driving::x87_abs(x(E(0x14))-x(E(0x10)))>X87(K(0x6280c0u))))return;
        const std::uint32_t r=safe_471010(w,cl,rec,rec2);
        pf(E(0x18),K(0x5a29ecu));
        const std::uint32_t pv=u32(w+0x314);
        if(pv){
            pf(E(0x14),lane_pos(pv));
            if(ftol(driving::x87_abs(x(E(0x10))-x(E(0x14)))*X87(K(0x628088u)))==0u&&gt(f32(w+0x1c4),f32(pv+0x1c4)))
                pf(E(0x18),driving::x87_float(dist_40f140(w+0x14,pv+0x14)));
        }
        if(!gt(f32(E(0x18)),float(i16(rec2+8))))return;
        if(r!=1u)return;
        const float pos=lane_pos(w),tgt=f32(E(0x10));
        if(gt(tgt,pos)){
            if(gt(K(0x5b43a8u),f32(w+0x264))){lane_right_470ab0(w,0xb4,rec);return;}
        }
        if(gt(pos,tgt)&&gt(f32(w+0x268),K(0x6280c0u)))lane_left_470a30(w,0xb4,rec);
    }
    // 40F140(a, b): |a - b| (x87).
    X87 dist_40f140(std::uint32_t a,std::uint32_t b){
        const X87 dx=x(a)-x(b),dy=x(a+4)-x(b+4),dz=x(a+8)-x(b+8);
        return driving::x87_sqrt((dz*dz+dx*dx)+dy*dy);
    }
    // ---- traffic speed (472450) -----------------------------------------------------------------------
    float dist_table(std::uint32_t a,std::uint32_t b){return f32(0x802af0u+(u32(a)+u32(b)*24u)*4u);}   // 802AF0[b][a]
    std::int32_t third(std::uint32_t c){return (std::int32_t(i8(c+0x66))*2)/6;}
    std::int32_t apart_x(std::uint32_t a,std::uint32_t b){return std::int32_t(ftol(driving::x87_abs(x(a+0x268)-x(b+0x268))*X87(K(0x628088u))));}
    // 470B30(ecx = car, eax = speed record, edx = course distance, edi = other): the target speed behind other.
    float speed_470b30(std::uint32_t w,std::uint32_t rec2,std::int32_t d,std::uint32_t o){
        Frame L(*this,0x10);
        std::int32_t dd=d-std::int32_t(i16(rec2+0xa));
        if(dd<0)dd=0;
        const std::int32_t span=std::int32_t(i16(rec2+4))-std::int32_t(i16(rec2+0xa));
        float t=sdiv(float(span-dd),float(span));
        pf(L(4),t);
        if(gt(0.0f,t)){t=0.0f;pf(L(4),t);}
        const std::uint32_t cls=u32(w+0x324);
        float base=float(i16(rec2+cls*2u+0x1c));
        if(!(cls==0u||cls==1u||cls==2u))base=sadd(base,smul(f32(0x80434cu),t));
        const float k3=K(0x5b43a4u);
        const float mn=smul(float(i16(rec2+cls*2u+0xe)),k3);
        const float sp=sadd(smul(ssub(smul(base,k3),mn),t),mn);
        const X87 v=(X87(K(0x6281dcu))-driving::x87_abs(x(w+0x268)-x(o+0x268))*X87(K(0x6281e4u)))*x(L(4));
        pf(L(4),driving::x87_float(v));
        float f=0.0f;
        if(!(X87(K619a34)>v)){f=f32(L(4));if(gt(f,K(0x62806cu)))f=K(0x62806cu);}
        float s2=f32(o+0x1c4);
        if((u8(w+4)&0x20u)&&u32(0x80fb14u))s2=K(0x6280b0u);
        const float k4=K(0x6281c0u);
        const float a=smul(smul(float(i16(rec2+cls*2u+0x2a)),s2),k4);
        const float b=smul(smul(float(i16(rec2+cls*2u+0x38)),s2),k4);
        float r=sadd(smul(ssub(b,a),f),a);
        if(gt(mn,r))r=smul(float(i16(rec2+cls*2u+0xe)),k3);
        return gt(sp,r)?r:sp;
    }
    // 470CB0(eax = car; course distance, speed record): the next car to slow down for (0 = none).
    std::uint32_t blocker_470cb0(std::uint32_t w,std::int32_t d,std::uint32_t rec2){
        const std::uint32_t pl=player();
        std::int32_t n;
        if(u32(w+0x324)==0u)n=2;
        else n=gt(smul(sadd(f32(0x803714u),K(0x628064u)),K(0x628088u)),K(0x6282a4u))?1:0;
        const float sp=speed_470b30(w,rec2,d,pl);
        for(std::int32_t lane=0;lane<=n;++lane){
            for(std::uint32_t nb=u32(w+0x318);nb;nb=u32(nb+0x318)){
                if(u8(nb+0xc5c)&1u)continue;
                if(u8(nb+4)&1u){
                    if(nb!=pl)continue;
                    const std::uint32_t v=u32(0x780258u);
                    if(v==3u||v==4u)continue;
                    break;
                }
                const std::uint32_t k=u32(w+0x5c);
                if(k&&third(nb)!=third(w)&&std::int32_t(i16(w+0x64))<end_43d470(k)-0xf)continue;
                const std::int32_t a=apart_x(w,nb);
                p32(0x80373cu,std::uint32_t(a));
                if(a>lane)continue;
                if(!gt(f32(nb+0x1c4),f32(w+0x1c4))&&!gt(f32(w+0x1c4),smul(sp,K(0x5c403cu))))continue;
                float dist=dist_table(nb,w);
                if(gt(K(0x628100u),dist))dist=driving::x87_float(ahead_46e250(nb,w));
                pf(0x800d54u,dist);
                if(a!=0&&!gt(dist,K(0x6280c0u)))continue;
                const float lim=smul(float(d/50+1),float(i16(rec2+8)));
                if(gt(lim,dist))return nb;
            }
        }
        return 0;
    }
    // 470ED0(eax = car, ecx = speed record; course distance): a slower car within two segments ahead (0 = none).
    std::uint32_t blocker_470ed0(std::uint32_t w,std::uint32_t rec2,std::int32_t d){
        const std::uint32_t pl=player();
        if(d>=std::int32_t(i16(rec2+4)))return 0;
        for(std::uint32_t nb=u32(w+0x318);nb;nb=u32(nb+0x318)){
            if(u8(nb+0xc5c)&1u)continue;
            if(u8(nb+4)&1u){
                if(nb!=pl)continue;
                const std::uint32_t v=u32(0x780258u);
                if(v==3u||v==4u)continue;
                return 0;
            }
            const std::uint32_t k=u32(w+0x5c);
            if(k&&third(nb)!=third(w)&&std::int32_t(i16(w+0x64))<end_43d470(k)-0xf)continue;
            if((u8(nb+4)&0x20u)&&(u32(nb+8)&0x40000000u))continue;
            const std::int32_t dd=diff_46f990(w+0x5c,nb+0x5c);
            float dist=dist_table(nb,w);
            if(gt(K(0x628100u),dist))dist=driving::x87_float(ahead_46e250(nb,w));
            if(gt(dist,K(0x6280c0u))&&dd<=2)return nb;
        }
        return 0;
    }
    // 471E70(ebx = car, esi = speed record; course distance, target): queue speed behind the player's group.
    void queue_471e70(std::uint32_t w,std::uint32_t rec2,std::int32_t d,std::uint32_t tgt){
        const std::uint32_t pl=player();
        std::int32_t cnt=0;
        if(i8(rec2+2)>0){
            for(std::int32_t i=0;i<i8(rec2+2);++i){
                const std::uint32_t c=u32(0x803750u+std::uint32_t(i)*4u);
                if(!c||w==c)break;
                if(diff_46f990(w+0x5c,pl+0x5c)>=std::int32_t(i16(rec2+6)))++cnt;
            }
        }
        const float k=K(0x5b43a4u);
        const std::uint32_t cls=u32(w+0x324);
        const float mn=smul(float(i16(rec2+cls*2u+0xe)),k);
        if(cnt<std::int32_t(i8(rec2+2))&&gt(f32(pl+0x1c4),mn)){
            const float v=sdiv(smul(float(i16(rec2+4)),mn),float(d));
            pf(tgt+4,v);
            const float m2=smul(float(i16(rec2+0xc)),k);
            if(gt(m2,v))pf(tgt+4,m2);
            if(gt(f32(tgt+4),f32(w+0x1c4))){p8(tgt,1);p8(tgt+1,0);}
            else{p8(tgt,0);p8(tgt+1,1);}
        }else{
            pf(tgt+4,smul(float(i16(rec2+u32(w+0x324)*2u+0xe)),k));
            p8(tgt+1,1);
        }
        if(u32(w+0x324)==5u&&gt(f32(w+0x1c4),f32(tgt+4)))p32(tgt+4,u32(w+0x1c4));
    }
    // 471F90(ebx = speed record, esi = car; course distance, target): follow the player's speed (0 = not in the group).
    std::uint32_t group_471f90(std::uint32_t rec2,std::uint32_t w,std::int32_t d,std::uint32_t tgt){
        const std::int32_t n=i8(rec2+3);
        std::int32_t cnt=0;
        for(std::int32_t i=0;i<n;++i){
            const std::uint32_t c=u32(0x803750u+std::uint32_t(i)*4u);
            if(!c)break;
            if(u8(c+4)&1u)continue;
            if(w==c)break;
            ++cnt;
        }
        const std::uint32_t pl=player();
        bool check=true;
        if(cnt==n&&w!=u32(pl+0x318)&&apart_x(w,pl)>1)check=false;
        if(check&&i32(pl+0x34)>=0xcc)return 0;
        const std::uint32_t nx=u32(w+0x318);
        if(nx&&gt(f32(nx+0x1c4),f32(pl+0x1c4)))pf(tgt+4,smul(f32(nx+0x1c4),K(0x5b43b0u)));
        else pf(tgt+4,smul(f32(pl+0x1c4),K(0x5b43b0u)));
        const float r=speed_470b30(w,rec2,d,pl);
        if(gt(f32(tgt+4),r))pf(tgt+4,r);
        const float mn=smul(float(i16(rec2+u32(w+0x324)*2u+0xe)),K(0x5b43a4u));
        if(gt(mn,f32(tgt+4)))pf(tgt+4,mn);
        return 1;
    }
    // 471680(eax = car; next car, appear record, speed record): start an overtake of next (1 = started).
    std::uint32_t overtake_471680(std::uint32_t w,std::uint32_t nb,std::uint32_t rec,std::uint32_t rec2){
        if(u32(0x780258u)==2u){
            const std::uint32_t k=call(0x45c440u,{});
            if(k==2u||k==6u||k==0xbu)return 0;
        }
        std::int8_t lo,hi;
        if(u32(w+0x5c)){
            if(u32(w+0x60)==0x64u){lo=0;hi=2;}else{lo=3;hi=5;}
        }else{
            const std::uint32_t v=call(0x44f0f0u,{call(0x44dc50u,{u32(w+0x68)}),u16(w+0x64)});
            if(!v){lo=0;hi=5;}
            else if(i8(w+0x66)>=3){lo=3;hi=5;}
            else{lo=0;hi=2;}
        }
        std::int8_t bl=std::int8_t(step_4714d0(w,nb,lo,hi));
        float pos=smul(float(bl),K(0x6280c0u));
        if(u32(w+0x5c)&&i8(w+0x66)>=3&&gt(K(0x6282d4u),ssub(f32(w+0x268),f32(w+0x264))))pos=ssub(pos,K(0x6281e8u));
        auto go=[&](float p)->bool{
            if(gt(p,f32(w+0x268))&&gt(K(0x5b43a8u),f32(w+0x264))){lane_right_470ab0(w,0xb4,rec);return true;}
            if(gt(f32(w+0x268),p)&&gt(f32(w+0x268),K(0x6280c0u))){lane_left_470a30(w,0xb4,rec);return true;}
            return false;
        };
        if(safe_471010(w,bl,rec,rec2)==1u&&go(pos))return 1;
        if(gt(pos,f32(w+0x268))&&i8(w+0x66)>lo)bl=std::int8_t(i8(w+0x66)-1);
        else if(gt(f32(w+0x268),pos)&&i8(w+0x66)<hi)bl=std::int8_t(i8(w+0x66)+1);
        const float p2=driving::x87_float(X87(std::int32_t(bl))*X87(K(0x6280c0u)));
        const X87 a=driving::x87_abs(x(w+0x268)-x(nb+0x268));
        const X87 b=driving::x87_abs(X87(p2)-x(nb+0x268));
        if(!(b>a))return 0;
        if(safe_471010(w,bl,rec,rec2)!=1u)return 0;
        return go(p2)?1u:0u;
    }
    // 4718D0(eax = car; next car, course distance, appear record, speed record, target, speed or <0): close on next.
    void close_4718d0(std::uint32_t w,std::uint32_t nb,std::int32_t d,std::uint32_t rec,std::uint32_t rec2,std::uint32_t tgt,float sp_in){
        Frame L(*this,0x10);
        const float k3=K(0x5b43a4u);
        pf(tgt+4,f32(nb+0x1c4));
        const float mn=smul(float(i16(rec2+u32(w+0x324)*2u+0xe)),k3);
        if(gt(mn,f32(nb+0x1c4)))pf(tgt+4,mn);
        float lim=float(i16(rec2+8));
        float dist=dist_table(nb,w);
        if(gt(lim,dist))dist=driving::x87_float(ahead_46e250(nb,w));
        float sp=sp_in;
        if(!(sp_in>=K619a34))sp=speed_470b30(w,rec2,d,player());
        {const std::uint32_t v=u32(0x780258u);if(v==3u||v==4u)lim=smul(lim,K(0x5b43b4u));}
        std::int8_t lo,hi;
        if(u32(nb+0x5c)){
            if(u32(nb+0x60)==0x64u){lo=0;hi=2;}else{lo=3;hi=5;}
        }else{
            const std::uint32_t v=call(0x44f0f0u,{call(0x44dc50u,{u32(nb+0x68)}),u16(nb+0x64)});
            if(!v){lo=0;hi=5;}
            else if(i8(nb+0x66)>=3){lo=3;hi=5;}
            else{lo=0;hi=2;}
        }
        const bool rb=(u8(nb+4)&0x20u)?((u32(nb+8)>>30)&1u)!=0u:false;
        auto lane_for_nb=[&](){
            const std::uint32_t f=u32(w+0xc5c);
            if((f&0x200u)&&i8(nb+0x66)<hi){lane_right_470ab0(nb,0xb4,rec);return;}
            if((f&0x400u)&&i8(nb+0x66)>lo)lane_left_470a30(nb,0xb4,rec);
        };
        auto unwind=[&](){
            const bool l=gt(f32(w+0x268),f32(nb+0x268))&&(u32(w+0xc5c)&0x200u);
            const bool r=gt(f32(nb+0x268),f32(w+0x268))&&(u32(w+0xc5c)&0x400u);
            if(!(l||r))return;
            const std::uint32_t f=u32(w+0xc5c);
            if(lane_bit_46c7e0(w,rec,u8(w+0x66)))return;
            p32(w+0xc5c,f&0xfffff9ffu);p32(w+0xb14,0);p16(w+0xb50,0);
        };
        if(gt(dist,K(0x6280c0u))&&gt(lim,dist)&&gt(f32(nb+0x1c4),f32(w+0x1c4))&&apart_x(w,nb)==0){
            std::int32_t t=time_471480(w,nb,dist);
            if(t<1)t=1;
            const float s=sadd(sdiv(ssub(smul(f32(nb+0x1c4),K(0x5b43b0u)),f32(w+0x1c4)),float(t)),f32(w+0x1c4));
            const float cap=sadd(smul(f32(0x80434cu),k3),K(0x5b43acu));
            pf(w+0x1c4,s);
            if(gt(s,cap))pf(w+0x1c4,cap);
            if(!gt(f32(w+0x1c4),smul(sp,K(0x5c403cu)))&&!rb)return;
            if(u32(nb+0xb14))return;
            if(!u32(w+0xb14)){
                overtake_471680(w,nb,rec,rec2);
                if(!rb)return;
                lane_for_nb();
                return;
            }
            unwind();
            return;
        }
        pf(tgt+4,smul(f32(tgt+4),K(0x5b43b0u)));
        const float gap=smul(float((u32(w+0x324)==0u?1:0)+1),K(0x6280c0u));
        bool go=gt(f32(nb+0x1c4),f32(tgt+4))||gt(f32(tgt+4),smul(sp,K(0x5c403cu)))||rb;
        if(go){
            bool close=true;
            if(apart_x(w,nb)!=0){
                close=false;
                if(gt(f32(w+0x268),f32(nb+0x268))&&!jb(sadd(gap,f32(nb+0x268)),f32(w+0x268))&&
                   ((u32(w+0xc5c)&0x200u)||(u32(nb+0xc5c)&0x400u)))close=true;
                else if(gt(f32(nb+0x268),f32(w+0x268))&&!jb(f32(w+0x268),ssub(f32(nb+0x268),gap))&&
                   ((u32(w+0xc5c)&0x400u)||(u32(nb+0xc5c)&0x200u)))close=true;
                if(!close){
                    pf(tgt+4,sp);
                    const float c=smul(f32(nb+0x1c4),K(0x5b43b0u));
                    if(gt(sp,c))pf(tgt+4,c);
                    p8(tgt,1);
                }
            }
            if(close&&!u32(nb+0xb14)){
                if(!u32(w+0xb14)){
                    if(overtake_471680(w,nb,rec,rec2)!=1u)pf(tgt+4,smul(f32(nb+0x1c4),K(0x5b43b0u)));
                    else if(rb)lane_for_nb();
                }else unwind();
            }
        }
        if(u8(w+6)&4u)p32(w+0x38,1);                                   // 471E3E bridge: test byte [esi+6],4
        if(gt(f32(tgt+4),f32(w+0x1c4)))p8(tgt,1);
    }
    // 472320(eax = car; course distance, appear record, speed record, target): 1 when a car ahead sets the speed.
    std::uint32_t behind_472320(std::uint32_t w,std::int32_t d,std::uint32_t rec,std::uint32_t rec2,std::uint32_t tgt){
        float sp=K(0x6280c4u);
        std::uint32_t nb=blocker_470cb0(w,d,rec2);
        if(!nb){
            nb=blocker_470ed0(w,rec2,d);
            const std::int32_t l2=u32(w+0x324)==0u?1:0;
            if(!nb)return 0;
            float dist=dist_table(nb,w);
            if(gt(K(0x628100u),dist))dist=driving::x87_float(ahead_46e250(nb,w));
            sp=speed_470b30(w,rec2,d,player());
            if(gt(K(0x6280c0u),dist))return 0;
            if(apart_x(w,nb)>l2){
                if(gt(f32(nb+0x1c4),f32(w+0x1c4)))return 0;
                if(gt(f32(w+0x1c4),smul(sp,K(0x5c403cu))))return 0;
            }
        }
        close_4718d0(w,nb,d,rec,rec2,tgt,sp);
        return 1;
    }
    // 472450(car, appear record, speed record): the traffic car's target speed (+10D8) and its rate (+10E0).
    void traffic_472450(std::uint32_t w,std::uint32_t rec,std::uint32_t rec2){
        pf(w+0x10e0,0.0f);
        const std::uint32_t pl=player();
        std::uint8_t stopped=0;
        std::int32_t d=diff_46f990(w+0x5c,pl+0x5c);
        bool net=false;
        {
            const std::uint32_t v=u32(0x780258u);
            if((v==3u||v==4u)&&std::uint8_t(call(0x456d60u,{}))>1u){(void)last_car_4722c0();net=true;}
        }
        p32(0x800da0u,std::uint32_t(d));
        const std::uint32_t tgt=w+0x10d8;
        p32(0x8037c0u,tgt);
        p32(w+0x10dc,u32(w+0x1c4));p8(w+0x10d9,0);p8(tgt,0);p32(w+0x38,0);
        const std::uint32_t cls0=u32(w+0x324);
        const float k3=K(0x5b43a4u);
        if(u8(w+0xb6c)&&i16(w+0xb60)<=0&&cls0!=0u&&cls0!=1u&&cls0!=2u&&u8(0x800d4du)<=1u){
            p8(tgt,0);p8(w+0x10d9,1);pf(w+0x10dc,0.0f);stopped=1;
        }else{
            const std::uint32_t racer=(u32(w+4)>>5)&1u;
            if(racer&&(u32(w+4)&0x10000u)&&u32(w+0xdd4)==8u)pf(w+0x10dc,smul(float(i16(rec2+cls0*2u+0x1c)),k3));
            else if(racer&&(u32(w+8)&0x40000000u))pf(w+0x10dc,smul(float(std::int32_t(i16(rec2+cls0*2u+0x1c))+0x14),k3));
            else{
                enum{Lead,Racer,Plain}path=Lead;
                if(!net){
                    if(w==u32(pl+0x318))path=d<0?Racer:Plain;
                    else if(d<0)path=Racer;
                    else if(d<std::int32_t(i16(rec2+6))&&apart_x(w,pl)>1)path=Plain;
                }
                if(path==Racer&&!(racer&&u32(0x80fb14u)))path=Plain;
                if(path==Racer){
                    if(apart_x(w,pl)<=1)pf(w+0x10dc,smul(float(i16(rec2+0xc)),k3));
                    else{
                        const std::int32_t base=i16(rec2+cls0*2u+0x1c);
                        pf(w+0x10dc,smul(float(racer_bit30_46ca40(w)?base+0x14:base),k3));
                        d=3;
                    }
                }else if(path==Plain){
                    pf(w+0x10dc,smul(float(i16(rec2+0xc)),k3));
                }else if(u8(pl+0x2f0)&2u){
                    pf(w+0x10dc,smul(float(i16(rec2+cls0*2u+0x1c)),k3));
                    if(racer)p32(w+8,u32(w+8)|0x40000000u);
                }else if(d<std::int32_t(i16(rec2+4))){
                    if(!behind_472320(w,d,rec,rec2,tgt)){
                        if(!group_471f90(rec2,w,d,tgt))pf(w+0x10dc,0.0f);
                    }
                    const float s=speed_470b30(w,rec2,d,pl);
                    p8(w+0x10d9,1);p8(w+0x10d8,0);
                    const float a=smul(float(i16(rec2+0xc)),k3);
                    if(gt(a,f32(w+0x10dc)))pf(w+0x10dc,a);
                    if(gt(s,f32(w+0x10dc)))pf(w+0x10dc,s);
                }else if(!behind_472320(w,d,rec,rec2,tgt))queue_471e70(w,rec2,d,tgt);
            }
        }
        // 472696 bridge: ecx = [ebp+0x324]
        const std::uint32_t cls=u32(w+0x324);
        const float one=K(0x62806cu);
        const std::uint32_t tab=0x5b3e08u+cls*0x14u;
        p32(w+0x10d4,tab);
        const std::int32_t far=i16(rec2+4);
        bool full=false;
        if(d>far||stopped)full=true;
        else if(d>=0&&d<=3&&gt(f32(w+0x10dc),f32(w+0x1c4))){
            const float t=sadd(sdiv(smul(float(d),K(0x5b0068u)),float(far)),K(0x62821cu));
            pf(w+0x10e0,t);
            const X87 v=(X87(K(0x6281dcu))-driving::x87_abs(x(w+0x268)-x(pl+0x268))*X87(K(0x6281e4u)))*X87(t);
            pf(w+0x10e0,driving::x87_float(v));
            if(X87(K619a34)>v)pf(w+0x10e0,0.0f);
            if(gt(f32(w+0x10e0),one))full=true;
        }else if(d<std::int32_t(i16(rec2+6))&&d>0&&gt(f32(w+0x1c4),f32(w+0x10dc))&&!net)full=true;
        if(full)pf(w+0x10e0,one);
        if(!(d>far)&&gt(K(0x628088u),f32(w+0x10e0))){
            const float s=smul(float(i16(rec2+cls*2u+0xe)),k3);
            if(gt(s,f32(w+0x1c4))&&gt(f32(w+0x10dc),f32(w+0x1c4))){
                const float q=sdiv(f32(w+0x1c4),s);
                pf(w+0x10e0,smul(q,q));
            }else pf(w+0x10e0,one);
            if(d<0)d=0;
            pf(w+0x10e0,smul(sadd(sdiv(smul(float(d),K(0x6282a0u)),float(far)),K(0x628088u)),f32(w+0x10e0)));
        }
        if(apart_x(w,pl)<1&&gt(f32(w+0x1c4),f32(w+0x10dc))&&d>=0&&d<=3){
            p8(w+0x10d9,1);p8(w+0x10d8,0);pf(w+0x10e0,K(0x628128u));
        }
        if(u32(0x80fb14u)&&!(u8(w+4)&0x20u))pf(w+0x10dc,smul(f32(u32(0x64df64u)+0x18),f32(w+0x10dc)));
        script_472090(u32(w+0x10e0),tab,tgt,w);
        const std::uint32_t v=u32(0x780258u);
        if((v==3u||v==4u)&&std::uint8_t(call(0x456d60u,{}))>1u&&gt(K(0x5b4398u),f32(w+0x1c4)))pf(w+0x1c4,K(0x5b4398u));
    }
    // ---- bus (472FE0) and racer (47A7C0) speeds ----------------------------------------------------------
    // 472A50(edi = car, ebx = speed record; &speed): not faster than the two cars ahead that are close.
    void follow_472a50(std::uint32_t w,std::uint32_t rec2,std::uint32_t ps){
        float a=f32(ps),b=a;
        const std::uint32_t pv=u32(w+0x314);
        if(pv){
            const X87 d=ahead_46e250(w,pv);
            if(X87(std::int32_t(i16(rec2+8)))>d&&gt(f32(w+0x1c4),f32(pv+0x1c4)))b=f32(pv+0x1c4);
            const std::uint32_t pp=u32(pv+0x314);
            if(pp){
                const X87 d2=ahead_46e250(pv,pp);
                if(X87(std::int32_t(i16(rec2+8)))>d2&&gt(f32(pv+0x1c4),f32(pp+0x1c4)))a=f32(pp+0x1c4);
            }
        }
        if(gt(a,b)){if(gt(f32(ps),b))pf(ps,b);return;}
        if(gt(f32(ps),a))pf(ps,a);
    }
    float sine_word(std::int16_t v){
        const float r=driving::x87_float((X87(std::int32_t(v))*X87(K(0x628254u)))*X87(K(0x628064u)));
        return driving::x87_float(driving::x87_sin(X87(r)));
    }
    // 472B60(esi = car; other, speed, out or 0): speed limit behind / beside other (+BE4 side push).
    float pace_472b60(std::uint32_t w,std::uint32_t o,std::uint32_t sp_bits,std::uint32_t out){
        Frame F(*this,0x20);
        const float d=dist_table(o,w);
        const X87 t=fild_u32(call(0x44fdf0u,{}));
        if(t>X87(K(0x5b43c4u))){
            bool side=gt(K(0x628100u),d)&&!(u8(w+8)&8u);
            if(side&&(u8(o+4)&1u)&&(u8(w+0xe)&1u))side=false;
            if(side&&gt(d,K(0x6282d4u))&&(u8(o+8)&8u))side=false;
            if(side){
                const float lc=driving::x87_float(x(o+0x268)-x(w+0x268));
                const X87 v=driving::x87_abs(X87(lc));
                const float vf=driving::x87_float(v);
                if(X87(K(0x5b43c0u))>v){
                    const float k=ssub(K(0x5b43c0u),vf);
                    pf(w+0xbe4,gt(lc,0.0f)?ssub(0.0f,k):k);
                    p32(w+8,u32(w+8)|8u);
                }
            }
            if(gt(f32(o+0x1c4),f32(w+0x1c4)))return from_bits(sp_bits);
        }
        const std::uint32_t cls=u32(o+0x320);
        std::uint32_t tab;
        X87 prod;
        if(u8(o+4)&0x20u){
            if(!jb(d,f32(0x5b423cu+cls*0x48u)))return from_bits(sp_bits);
            tab=0x5b4200u+cls*0x48u;
            const X87 a=driving::x87_abs(X87(sine_word(i16(w+0xc2c))));
            const float af=driving::x87_float(a);
            prod=driving::x87_abs(X87(sine_word(i16(o+0xc2c))))*X87(af);
        }else{
            if(!jb(d,f32(0x5b411cu+cls*0x48u)))return from_bits(sp_bits);
            tab=0x5b40e0u+cls*0x48u;
            prod=driving::x87_abs(X87(sine_word(i16(w+0xc2c))));
        }
        const float f=driving::x87_float(prod);
        const std::uint32_t ang=heading_487750(f32(w+0x14),f32(w+0x1c),f32(o+0x14),f32(o+0x1c));
        const std::uint32_t dif=std::uint32_t(std::abs(std::int32_t(i16(w+0xc2c))+std::int32_t(i16(w+0x2e))-std::int32_t(std::int16_t(ang))))&0xffffu;
        std::uint32_t ebx=ftol(X87(std::int32_t(dif))*(X87(K(0x62806cu))-X87(f)));
        if(gt(0.0f,smul(ssub(f32(w+0x268),f32(o+0x268)),f32(w+0xbb8))))ebx+=ftol(driving::x87_abs(x(w+0xbb8))*X87(K(0x5b43bcu)));
        for(std::uint32_t i=0;i<6;++i,tab+=0xcu){
            if(!gt(f32(tab),d))continue;
            if(std::int32_t(ebx&0xffffu)>=std::int32_t(i16(tab+4)))continue;
            if(out)pf(out,d);
            if(gt(K(0x62806cu),f32(tab+8))){
                p32(w+0xc,u32(w+0xc)|0x8000u);
                return smul(f32(o+0x1c4),(u8(o+4)&1u)?K(0x5b43b8u):K(0x628208u));
            }
            return smul(f32(o+0x1c4),f32(tab+8));
        }
        return from_bits(sp_bits);
    }
    // 472E60(eax = car, edi = &speed; out or 0): racer pace from the cars around (+BE4 side push).
    void pace_472e60(std::uint32_t w,std::uint32_t ps,std::uint32_t out){
        Frame F(*this,0x10);
        pf(w+0xbe4,0.0f);
        p32(w+8,u32(w+8)&0xfffffff7u);
        const std::uint32_t nx=u32(w+0x318);
        if(nx&&!((u8(nx+4)&1u)&&(u32(w+0xc)&0x10600u))){
            if(fild_u32(call(0x44fdf0u,{}))>X87(K(0x5b43c4u))&&gt(K(0x628248u),dist_table(nx,w))){
                const float b=driving::x87_float(x(w+0x268)-x(nx+0x268));
                const X87 v=driving::x87_abs(X87(b));
                const float vf=driving::x87_float(v);
                if(X87(K(0x5b43c0u))>v){
                    const float k=ssub(K(0x5b43c0u),vf);
                    pf(w+0xbe4,gt(b,0.0f)?k:ssub(0.0f,k));
                    p32(w+8,u32(w+8)|8u);
                }
            }
        }
        float r0,r1;
        const std::uint32_t pv=u32(w+0x314);
        if(pv){
            if((u8(pv+4)&1u)&&(u32(w+0xc)&0x600u))return;
            r1=pace_472b60(w,pv,u32(ps),out);
            const std::uint32_t pp=u32(pv+0x314);
            if(pp)r0=pace_472b60(w,pp,u32(ps),out);
            else r0=f32(ps);
        }else{r0=f32(ps);r1=r0;}
        if(!jb(r0,r1)){if(gt(f32(ps),r1))pf(ps,r1);}
        else if(gt(f32(ps),r0))pf(ps,r0);
    }
    // 474080(esi = car): racer slipstream factor (+C08 table of the cars behind, +C3C target).
    float draft_474080(std::uint32_t w){
        float l=K(0x62806cu);
        if(!(u32(w+0xc)&0x20000u))return l;
        const X87 v=driving::x87_abs(X87(driving::x87_float(X87(std::int32_t(i16(w+0x162)))*X87(K(0x628258u)))));
        if(v>X87(K(0x6282b8u)))return K(0x62806cu);
        std::uint32_t pv=u32(w+0x314);
        const float far=K(0x59943cu);
        for(std::uint32_t k=0;k<0x18;k+=4)pf(w+0xc08+k,far);
        p32(w+0xc,u32(w+0xc)&0xff7fffffu);
        for(std::uint32_t n=9;pv&&n<0x20u;++n){
            const float dd=dist_table(pv,w);
            if(gt(dd,K(0x628134u)))return l;
            if(!(u8(pv+4)&1u)){
                const float s=smul(f32(w+0x1c4),K(0x5c403cu));
                if(gt(s,K(0x62806cu))&&gt(f32(pv+0x1c4),s)){
                    if(X87(K(0x628138u))>driving::x87_abs(x(pv+0x268)-x(w+0x268))){
                        if(gt(dd,K(0x628244u))){p32(w+0xc,u32(w+0xc)|0x40000u);p32(w+0xc3c,u32(pv));l=s;}
                        return l;
                    }
                    if(gt(dd,K(0x6282b8u))){
                        pf(w+0xc08+std::uint32_t(std::int32_t(i8(pv+0xc32))*4),dd);   // 4743B7-style +C32 lane byte
                        p32(w+0xc,u32(w+0xc)|0x800000u);
                    }
                }
            }
            pv=u32(pv+0x314);
        }
        return l;
    }
    // 47A7C0(eax = car; speed record): a racer's target speed.
    void racer_47a7c0(std::uint32_t w,std::uint32_t rec2){
        Frame F(*this,0x20);
        const std::uint32_t pl=player();
        const std::uint32_t ps=w+0x10dc;
        p32(ps,u32(w+0x1c4));p8(w+0x10d9,0);p8(w+0x10d8,0);p32(w+0x38,0);p32(w+0x10d4,0x5b40b0u);
        const float b=driving::x87_float(driving::x87_abs(x(cfg_mode3_4762e0()?w+0xbb4:w+0xb54)));
        pf(w+0x10e0,K(0x62806cu));
        const float k3=K(0x5b43a4u);
        const std::uint32_t cls=u32(w+0x324);
        if(u8(w+0xb6c)&&i16(w+0xb60)<=0&&u8(0x800d4du)<=1u){p8(w+0x10d8,0);p8(w+0x10d9,1);pf(ps,0.0f);}
        else if((u8(pl+0x2f0)&2u)||i8(pl+0x66)<0){
            pf(ps,smul(float(i16(rec2+cls*2u+0x1c)),k3));
            pf(w+0x10e0,(u8(pl+0x2f0)&2u)?K(0x628064u):K(0x5b0068u));
            if(gt(f32(w+0x1c4),f32(ps)))p32(ps,u32(w+0x1c4));
        }else{
            const float v=smul(smul(smul(float(i16(rec2+cls*2u+0x1c)),k3),f32(w+0xb78)),f32(w+0xbc0));
            pf(ps,v);
            if(u32(0x8037b8u))pf(ps,smul(f32(u32(0x64df64u)+0x14),v));
            else if(gt(K(0x5b4408u),f32(pl+0x1c4))&&call(0x44fdf0u,{})>=0x5e1u)pf(ps,smul(f32(u32(0x64df64u)+0x10),f32(ps)));
            pf(ps,smul(racer_rate(racers()+std::uint32_t(std::int32_t(i8(w+0xc38))*0xa0)),f32(ps)));
            {
                const X87 t=fild_u32(call(0x44fdf0u,{}));
                const float ang=driving::x87_float(((t+x(w+0xb84))/x(w+0xb80))*X87(K(0x5a29dcu)));
                const float r=driving::x87_float((driving::x87_cos(X87(ang))-X87(K(0x62806cu)))*x(w+0xb7c)+x(ps));
                pf(ps,r);pf(F(0x10),r);
            }
            for(std::uint32_t i=0;i<5;++i){
                const float lo=f32(0x5b3f80u+i*8u),hi=f32(0x5b3f88u+i*8u);
                if(jb(b,lo)||!gt(hi,b))continue;
                const float t=sdiv(ssub(hi,b),ssub(hi,lo));
                const float v2=smul(smul(smul(sadd(smul(ssub(K(0x62806cu),t),f32(0x5b3f8cu+i*8u)),smul(f32(0x5b3f84u+i*8u),t)),k3),f32(w+0xb88)),f32(w+0xbc0));
                pf(F(0x10),v2);
                const X87 t2=fild_u32(call(0x44fdf0u,{}));
                const float ang=driving::x87_float(((t2+x(w+0xb94))/x(w+0xb90))*X87(K(0x5a29dcu)));
                pf(F(0x10),driving::x87_float(driving::x87_cos(X87(ang))*x(w+0xb8c)+X87(v2)));
                break;
            }
            if(gt(f32(ps),f32(F(0x10)))){pf(ps,f32(F(0x10)));p8(w+0x10d9,0);p8(w+0x10d8,1);}
            pace_472e60(w,ps,0);
            const float s=smul(f32(pl+0x1c4),K(0x5c403cu));
            bool done=false;
            if(gt(s,K(0x62806cu))&&gt(f32(w+0x1c4),s)){
                const float bb=driving::x87_float(driving::x87_abs(x(w+0x268)-x(pl+0x268)));
                const float g=(u32(w+0xc)&0x400u)?0.0f:K(0x6280c0u);
                if(gt(K(0x6281e8u),bb)&&gt(bb,g)){
                    float d0=dist_table(pl,w);
                    if(gt(K(0x6282ccu),d0))d0=driving::x87_float(ahead_46e250(pl,w));
                    if(gt(K(0x5b43a8u),d0)&&gt(d0,K(0x5b4458u))){
                        pf(ps,smul(f32(ps),s));
                        p32(w+0xc,u32(w+0xc)|0x40000u);
                        p32(w+0xc3c,u32(pl));
                        done=true;
                    }
                }
            }
            if(!done)pf(ps,smul(draft_474080(w),f32(ps)));
        }
        {
            const float mn=smul(float(i16(rec2+0xc)),k3);
            if(gt(mn,f32(ps)))pf(ps,mn);
            else{
                const std::uint32_t p=u32(w+0x2b4);
                const float cap=sdiv(smul(sadd(smul(ssub(f32(p+0x134c),K(0x5b4454u)),K(0x5b4450u)),K(0x5b444cu)),K(0x5b4448u)),
                                     smul(f32(p+0x2438),K(0x5a460cu)));
                if(gt(f32(ps),cap))pf(ps,cap);
            }
        }
        if(u8(w+0xf)&1u)pf(ps,smul(f32(ps),K(0x628064u)));
        const std::int32_t d=diff_46f990(w+0x5c,pl+0x5c);
        if(d>=0&&d<std::int32_t(i16(rec2+4))&&gt(f32(w+0x1c4),f32(ps))){
            if(apart_x(w,pl)<1&&d<=3){
                const float v=smul(f32(w+0x10e0),K(0x628128u));
                p8(w+0x10d9,1);p8(w+0x10d8,0);pf(w+0x10e0,v);
            }else{
                const float v=smul(smul(float(d),f32(w+0x10e0)),K(0x628194u));
                pf(w+0x10e0,v);
                if(gt(v,K(0x62806cu)))pf(w+0x10e0,K(0x62806cu));
            }
        }
        script_472090(u32(w+0x10e0),u32(w+0x10d4),w+0x10d8,w);
    }
    // 472FE0(eax = car; speed record): a bus's target speed.
    void bus_472fe0(std::uint32_t w,std::uint32_t rec2){
        std::int8_t zone;
        if(call(0x45c440u,{})!=0xeu)zone=2;
        else{
            const std::uint32_t p=call(0x45aff0u,{});
            const std::uint16_t cx=std::uint16_t(u16(w+0x64)-u16(p+4));
            const std::uint32_t idx=std::uint32_t(std::int32_t(i8(w+0xb74))*4);
            if(cx<=u16(0x5b3f10u+idx))zone=2;
            else zone=u16(0x5b3f12u+idx)<cx?0:1;
        }
        const float lb=driving::x87_float(driving::x87_abs(x(w+0xb54)));
        const std::uint32_t pl=player();
        const std::uint32_t ps=w+0x10dc;
        p32(ps,u32(w+0x1c4));p8(w+0x10d9,0);p8(w+0x10d8,0);p32(w+0x38,0);
        const std::uint32_t r=call(0x45b810u,{});
        p32(w+0x10d4,(std::uint8_t(r)==1u?0x5b3ed0u:0x5b3e94u)+std::uint32_t(std::int32_t(zone)*0x14));
        const float one=K(0x62806cu),k3=K(0x5b43a4u);
        pf(w+0x10e0,one);
        const std::uint32_t cls=u32(w+0x324);
        if(u8(w+0xb6c)&&i16(w+0xb60)<=0&&u8(0x800d4du)<=1u){p8(w+0x10d8,0);p8(w+0x10d9,1);pf(ps,0.0f);}
        else if((u8(pl+0x2f0)&2u)||i8(pl+0x66)<0){
            pf(ps,smul(float(i16(rec2+cls*2u+0x1c)),k3));
            pf(w+0x10e0,(u8(pl+0x2f0)&2u)?K(0x628064u):K(0x5b0068u));
            if(gt(f32(w+0x1c4),f32(ps)))p32(ps,u32(w+0x1c4));
        }else if(zone>=1){
            const float v=smul(float(i16(rec2+cls*2u+0x1c)),k3);
            pf(ps,v);
            if(gt(f32(w+0x1c4),v))p32(ps,u32(w+0x1c4));
        }else{
            const float mn=smul(float(i16(rec2+cls*2u+0xe)),k3);
            pf(ps,mn);
            float v=mn;
            for(std::uint32_t i=0;i<5;++i){
                const float lo=f32(0x5b3f50u+i*8u),hi=f32(0x5b3f58u+i*8u);
                if(jb(lb,lo)||!gt(hi,lb))continue;
                const float t=sdiv(ssub(hi,lb),ssub(hi,lo));
                v=sadd(smul(ssub(one,t),f32(0x5b3f5cu+i*8u)),smul(f32(0x5b3f54u+i*8u),t));
                break;
            }
            if(gt(mn,v)){pf(ps,v);p8(w+0x10d9,0);p8(w+0x10d8,1);}
            follow_472a50(w,rec2,ps);
            const X87 dd=ahead_46e250(pl,w);
            const float df=driving::x87_float(dd);
            if(X87(K(0x5b43a8u))>dd){
                if(gt(K(0x5b4394u),df))pf(ps,smul(f32(ps),K(0x5b43b4u)));
                else{
                    const float k=K(0x5b43c8u);
                    if(gt(f32(w+0x1c4),smul(f32(pl+0x1c4),k))&&X87(K(0x6280c0u))>driving::x87_abs(x(w+0x268)-x(pl+0x268)))
                        pf(ps,smul(smul(f32(ps),f32(pl+0x1c4)),k));
                }
            }
        }
        {
            const float a=smul(float(i16(rec2+0xc)),k3);
            if(gt(a,f32(ps)))pf(ps,a);
        }
        const std::int32_t d=diff_46f990(w+0x5c,pl+0x5c);
        if(d>=0&&d<std::int32_t(i16(rec2+4))&&zone<1){
            if(gt(f32(w+0x1c4),f32(ps))){
                if(apart_x(w,pl)<1&&d<=3){
                    const float v=smul(f32(w+0x10e0),K(0x628128u));
                    p8(w+0x10d9,1);p8(w+0x10d8,0);pf(w+0x10e0,v);
                }else{
                    const float v=smul(smul(float(d),f32(w+0x10e0)),K(0x628194u));
                    pf(w+0x10e0,v);
                    if(gt(v,one))pf(w+0x10e0,one);
                }
            }else if(gt(f32(pl+0x1c4),K(0x6281c0u))){
                const float v=smul(ssub(K(0x5b005cu),sdiv(f32(w+0x1c4),f32(pl+0x1c4))),f32(w+0x10e0));
                pf(w+0x10e0,v);
                if(gt(v,one))pf(w+0x10e0,one);
                if(gt(K(0x628088u),f32(w+0x10e0)))pf(w+0x10e0,K(0x628088u));
            }
        }
        script_472090(u32(w+0x10e0),u32(w+0x10d4),w+0x10d8,w);
    }
    // ---- not ported yet ------------------------------------------------------------------------
    // ---- racer lanes (47AD30) ------------------------------------------------------------------
    // lane sample table of a course kind / row / lane (0 for a kind-0 lane >= 6, as the PC computes it).
    std::uint32_t lane_table(std::uint32_t kind,std::uint32_t row,std::int32_t lane){
        if(kind==0u){
            if(lane>=6)return 0;
            return std::uint32_t((lane+std::int32_t(row)*6)*0x1030)+0x804480u;
        }
        return std::uint32_t((lane+std::int32_t(row)*12)*0x70c)+0x80a670u;
    }
    // two lane tables share the sample point at pos (+4, +6 words, +8 without its top bit).
    bool same_point(std::uint32_t a,std::uint32_t b,std::int32_t pos){
        const std::uint32_t i=std::uint32_t(pos*6);
        if(u16(a+i+4)!=u16(b+i+4))return false;
        if((u16(a+i+8)^u16(b+i+8))&0x7fffu)return false;
        return u16(a+i+6)==u16(b+i+6);
    }
    // 46F730(car, lane): a free lane next to a blocked one (+C33 bits 5-lane) on the kind-0 course.
    std::uint8_t lane_pref_46f730(std::uint32_t w,std::uint8_t lane){
        if(u32(w+0x5c))return lane;
        const std::uint32_t blocked=u8(w+0xc33);
        std::uint32_t al=lane;
        auto bit=[&](std::uint32_t l){return (blocked&(1u<<((5u-l)&31u)))!=0u;};
        if(!bit(al))return std::uint8_t(al);
        if(al<2u){do{al=(al+1u)&0xffu;}while(bit(al));return std::uint8_t(al);}
        if(al<=3u)return std::uint8_t(al);
        do{al=(al-1u)&0xffu;}while(bit(al));
        return std::uint8_t(al);
    }
    // 46F7A0(car): lanes of the kind-0 course merged at the car's sample (bits 4+i / bits i of the two lane pairs).
    std::uint8_t route_flags_46f7a0(std::uint32_t w){
        const std::uint16_t pos=u16(w+0x64);
        if(u32(w+0x5c))return 0;
        std::uint8_t mask=0;
        std::int32_t A=2,B=2,I=0;std::uint32_t P=0x808540u,Q=0x8054b0u;
        const std::uint32_t idx=std::uint32_t(pos)*6u;
        do{
            if(A>0){
                std::int32_t J=A-1,n=A;std::uint32_t R=P,S=Q;std::uint32_t bit=std::uint32_t(I)+4u;
                do{
                    if(!(mask&std::uint8_t(1u<<(bit&31u)))){
                        if(u16(Q+idx+0x1034)==u16(S+idx+4)&&!((u16(S+idx+8)^u16(Q+idx+0x1038))&0x7fffu)&&u16(Q+idx+0x1036)==u16(S+idx+6))
                            mask|=std::uint8_t(std::uint8_t(1u)<<(bit&31u));
                    }
                    if(!(mask&std::uint8_t(1u<<(std::uint32_t(J)&31u)))){
                        if(u16(P+idx-0x102cu)==u16(R+idx+4)&&!((u16(R+idx+8)^u16(P+idx-0x1028u))&0x7fffu)&&u16(P+idx-0x102au)==u16(R+idx+6))
                            mask|=std::uint8_t(std::uint8_t(1u)<<(std::uint32_t(J)&31u));
                    }
                    R+=0x1030u;S-=0x1030u;++bit;--J;--n;
                }while(n!=0);
            }
            ++I;--A;P+=0x1030u;Q-=0x1030u;--B;
        }while(B!=0);
        return mask;
    }
    // 474200(esi = car, al = lane or <0): the +C30 lane of a blocked overtake (+C08 free lanes), 1 when set.
    std::uint32_t lane_474200(std::uint32_t w,std::int8_t al){
        const std::uint32_t f=u32(w+0xc);
        if(f&0x40000u){
            const std::uint32_t o=u32(0x799b38u+u32(w+0xc3c)*0x3cu);
            if(u8(o+4)&1u)return 0;
            p8(w+0xc30,u8(o+0xc32));return 1;
        }
        if(!(f&0x800000u))return 0;
        const std::int8_t c32=i8(w+0xc32);
        std::int8_t bl=c32,dl=-1;
        if(al<0)al=i8(w+0xc30);
        const std::int8_t d=std::int8_t(al-c32);
        const float MAX=K(0x59943cu);
        auto free=[&](std::int8_t l){return !(f32(w+0xc08+std::uint32_t(std::int32_t(l))*4u)==MAX);};   // ucomiss + jp: not equal or unordered
        bool found=false;
        if(d==0){
            for(std::int32_t e=0;e<6&&!found;++e){
                std::int8_t cl=std::int8_t(e+bl);
                if(cl<6&&free(cl)){dl=cl;found=true;break;}
                cl=std::int8_t(bl-e);
                if(cl>=0&&free(cl)){dl=cl;found=true;break;}
            }
            if(!found)return 0;
        }else if(d>0){
            std::int8_t cl=c32;
            for(;cl<6;++cl)if(free(cl))break;
            if(cl<6){dl=cl;found=true;}
            else{
                --bl;cl=bl;
                for(;cl>=0;--cl)if(free(cl)){dl=cl;found=true;break;}
            }
            if(!found&&dl<0)return 0;
        }else{
            std::int8_t cl=c32;
            for(;cl>=0;--cl)if(free(cl))break;
            if(cl>=0){dl=cl;found=true;}
            else{
                ++bl;cl=bl;
                for(;cl<6;++cl)if(free(cl)){dl=cl;found=true;break;}
            }
            if(!found&&dl<0)return 0;
        }
        std::int32_t a=std::int32_t(i8(w+0xc32))-std::int32_t(dl);
        p8(w+0xc30,std::uint8_t(dl));
        a=a<0?-a:a;
        pf(w+0xbe8,sadd(smul(float(std::int32_t(std::int8_t(a))),K(0x628128u)),K(0x62806cu)));
        return 1;
    }
    // 4743B0(esi = car; other, lane, range, keep, speed, weave): move +C30 toward/away from the other car's lane.
    // Bridge 4743B7: cl = byte [other+C32].
    std::uint32_t tactic_4743b0(std::uint32_t w,std::uint32_t other,std::int8_t lane,std::int8_t range,std::uint32_t keep,std::uint32_t speed,std::uint32_t weave){
        std::int8_t cl=i8(other+0xc32);
        const std::int8_t al=i8(w+0xc32);
        const std::uint32_t kind=u32(w+0x5c);
        std::int8_t bl=std::int8_t(al-cl);
        std::int8_t lo=0,hi=5,near=2;
        if(kind==1u){
            if(al>=3){if(cl<3)return 0;}
            else if(cl>=3)return 0;
            if(range>2)range=2;
            if(al>2)lo=3;else hi=2;
            near=1;
        }
        {std::int32_t a=bl;a=a<0?-a:a;if(a>=std::int32_t(range))return 0;}
        bool up;
        if(bl!=0)up=bl>0;
        else if(lane<0)up=gt(f32(w+0x268),f32(other+0x268));
        else{
            const std::int8_t d=std::int8_t(lane-i8(w+0xc32));
            if(d!=0)up=d>0;
            else up=lane<3;
        }
        if(!up){
            bl=std::int8_t(cl-range);
            if(bl<lo){
                bl=lo;
                if(keep&&std::int8_t(cl-lo)<near){bl=std::int8_t(cl+range);if(bl>hi)bl=hi;}
            }
        }else{
            bl=std::int8_t(cl+range);
            if(bl>hi){
                bl=hi;
                if(keep&&std::int8_t(hi-cl)<near){bl=std::int8_t(cl-range);if(bl<lo)bl=lo;}
            }
        }
        if(bl<0)return 0;
        if(speed&&gt(ssub(f32(other+0x1c4),f32(w+0x1c4)),K(0x6281c0u)))p32(w+0xc,u32(w+0xc)|0x1000000u);
        float add=0.0f;
        if(weave&&!(u8(w+0xf)&1u)){
            const float p=smul(float(std::int32_t(i16(w+0x64))),K(0x62813cu));
            const std::int32_t q=cvtt(smul(p,K(0x5b43e4u)));
            const X87 s=driving::x87_sin(X87(p)-X87(q)*X87(K(0x5a29dcu)));
            std::int8_t a=std::int8_t(ftol(s*X87(K(0x6281ecu))));
            if(a!=0){
                a=std::int8_t(a+bl);
                if(kind==1u){
                    if(i8(w+0xc32)>=3){
                        if(a>=3){if(a<=5)bl=a;else bl=std::int8_t(10-a);}
                        else bl=std::int8_t(6-a);
                    }else{
                        if(a<0)bl=std::int8_t(-a);
                        else if(a<=2)bl=a;
                        else bl=std::int8_t(4-a);
                    }
                }else{
                    if(a>=0){if(a<=5)bl=a;else bl=std::int8_t(10-a);}
                    else bl=std::int8_t(-a);
                }
            }
            add=K(0x628108u);
        }
        std::int32_t a=std::int32_t(bl)-std::int32_t(i8(w+0xc32));a=a<0?-a:a;
        pf(w+0xbe8,sadd(sadd(smul(float(std::int32_t(std::int8_t(a))),K(0x5b43e0u)),add),K(0x62806cu)));
        p8(w+0xc30,std::uint8_t(bl));
        return 1;
    }
    // 46FFC0(rec, out): a lane sample point {x, y (15 bits), z, width, 0}.
    void decode_46ffc0(std::uint32_t r,std::uint32_t o){
        const float k0=K(0x62818cu),k1=K(0x5a29ecu);
        pf(o,smul(smul(float(std::int32_t(i16(r))),k0),k1));
        const std::int16_t y=std::int16_t(std::int16_t(std::uint16_t(u16(r+4)<<1))>>1);
        pf(o+4,smul(smul(float(std::int32_t(y)),k0),k1));
        pf(o+8,smul(smul(float(std::int32_t(i16(r+2))),k0),k1));
        p32(o+0x10,0);
        pf(o+0xc,(u8(r+5)&0x80u)?K(0x5b437cu):K(0x5b4378u));
    }
    // 479BA0(place, point, row, lane): signed distance of point from the lane segment at the place
    // (FLT_MAX when the lane has no usable sample pair).
    X87 lane_offset_479ba0(std::uint32_t place,std::uint32_t point,std::uint32_t row,std::uint32_t lane){
        Frame E(*this,0x30);
        const std::uint32_t A=E(8),Bp=E(0x1c);
        push_load(call(0x44bed0u,{u32(place)}));
        const std::uint32_t kind=u32(place);
        const std::uint32_t r8=row&0xffu,l8=lane&0xffu;
        std::uint32_t t;
        if(kind==0u)t=l8>=6u?0u:(l8+r8*6u)*0x1030u+0x804480u;
        else t=(l8+r8*12u)*0x70cu+0x80a670u;
        auto fail=[&]{pop();return X87(K(0x59943cu));};
        if(u16(t)!=0x4f53u)return fail();
        const std::uint32_t rec=t+std::uint32_t(std::int32_t(i16(place+8))*6)+4u;
        decode_46ffc0(rec,A);
        if(!gt(f32(A+0xc),K(0x6281f0u)))return fail();
        point_40a7d0(A,A);
        decode_46ffc0(rec+6u,Bp);
        if(!gt(f32(Bp+0xc),K(0x6281f0u)))return fail();
        point_40a7d0(Bp,Bp);
        const float x1=f32(A),z1=f32(A+8),x2=f32(Bp),z2=f32(Bp+8);
        const float dx=ssub(x2,x1),dz=ssub(z2,z1);
        const float cross=ssub(smul(ssub(f32(point),x1),dz),smul(ssub(f32(point+8),z1),dx));
        const float len=driving::x87_float(driving::x87_sqrt(X87(sadd(smul(dz,dz),smul(dx,dx)))));
        if(!(len>0.0f))return fail();
        pop();
        return -(X87(cross)/X87(len));
    }
    // 474640(eax = car, ebx = lane): tactic toward the player when it is just ahead.
    std::uint32_t tactic_474640(std::uint32_t w,std::int8_t lane){
        const std::uint32_t pl=u32(0x799d18u);
        const std::int32_t d=diff_46f990(w+0x5c,pl+0x5c);
        if(d<-2)return tactic_4743b0(w,pl,lane,4,1,0,0);
        if(d>=0xa)return 0;
        if(d>=5)return tactic_4743b0(w,pl,lane,2,1,0,0);
        const float a=driving::x87_float(ahead_46e250(pl,w));
        if(X87(a)>X87(K(0x6280c0u)))return tactic_4743b0(w,pl,lane,4,1,0,1);         // fcomip; jbe
        if(gt(a,K619a34)||gt(a,K(0x5b43a8u)))return tactic_4743b0(w,pl,lane,4,0,1,0); // 4746AE
        return tactic_4743b0(w,pl,lane,4,1,0,0);
    }
    // 474700(eax = car): follow / pass the player when it is close behind or beside.
    std::uint32_t tactic_474700(std::uint32_t w){
        const std::uint32_t pl=u32(0x799d18u);
        const std::int32_t d=diff_46f990(w+0x5c,pl+0x5c);
        if(d<=-2||d>=8)return 0;
        if(!gt(f32(w+0x1c4),K(0x5b43ecu)))return 0;
        const float a=driving::x87_float(ahead_46e250(pl,w));
        auto absdiff=[&](){std::int32_t v=std::int32_t(i8(w+0xc32))-std::int32_t(i8(pl+0xc32));v=v<0?-v:v;return std::int8_t(v);};
        if(X87(a)>X87(K(0x6280c0u))){
            const float dv=ssub(f32(pl+0x1c4),f32(w+0x1c4));
            if(!gt(dv,K(0x5b43e8u)))return 0;
            if(!gt(K(0x628064u),dv))return 0;
            p8(w+0xc30,u8(pl+0xc32));
            const std::int8_t al=absdiff();
            float v=smul(float(8-d),float(std::int32_t(al)));
            v=smul(smul(smul(v,K(0x628108u)),f32(w+0xbd4)),K(0x628088u));
            pf(w+0xbe8,sadd(v,K(0x62806cu)));
            if(al==0)pf(w+0xb10,ssub(f32(pl+0x268),f32(w+0x268)));
            p32(w+0xc,u32(w+0xc)|0x800u);
            return 1;
        }
        if(!gt(a,K(0x5b43a8u)))return 0;
        std::uint32_t f=u32(w+0xc);
        if(f&0x400u){
            p8(w+0xc30,u8(pl+0xc32));
            const std::int8_t al=absdiff();
            float v=smul(float(d+8),float(std::int32_t(al)));
            v=smul(smul(smul(v,K(0x628108u)),f32(w+0xbd8)),K(0x6280f0u));
            f|=0x4000u;
            pf(w+0xbe8,sadd(v,K(0x62806cu)));
            p32(w+0xc,f);
            if(al==0)pf(w+0xb10,ssub(f32(pl+0x268),f32(w+0x268)));
            return 1;
        }
        std::int8_t cl=i8(pl+0xc32);
        const std::int8_t al=absdiff();
        if(al<0)--cl;
        else if(al>0)++cl;
        else if(gt(f32(w+0x268),f32(pl+0x268)))++cl;
        else --cl;
        p8(w+0xc30,std::uint8_t(cl));
        if(cl<0)p8(w+0xc30,0);
        else if(cl>5)p8(w+0xc30,5);
        std::int32_t aa=al;aa=aa<0?-aa:aa;
        float v=smul(float(aa),float(8-d));
        v=smul(smul(smul(v,K(0x628108u)),f32(w+0xbd4)),K(0x628088u));
        pf(w+0xbe8,sadd(v,K(0x62806cu)));
        p32(w+0xc,f|0x800u);
        return 1;
    }
    // 479F50(car, lane): the lane to take from the cost of the six lanes (+BF0..C04) given the cars
    // around (course order links +318/+314, distance matrix 802AF0, lanes merged at their sample).
    std::uint8_t lane_choice_479f50(std::uint32_t w,std::int8_t lane){
        Frame E(*this,0x58);
        auto T=[&](std::int32_t l){return E(0x34)+std::uint32_t(l*4);};
        const std::uint32_t f=u32(w+0xc);
        float lim;
        if(f&0x200000u)lim=K(0x5b43c0u);
        else lim=((f&0x20000u)&&(f&0x8000000u))?K(0x6280b0u):K(0x5b4444u);
        const std::int8_t c32=i8(w+0xc32);
        const std::uint32_t blocked=u8(w+0xc33);
        const float open=K(0x5b4370u),MAX=K(0x59943cu);
        for(std::uint32_t k=0;k<6;++k)pf(T(std::int32_t(k)),(blocked&(0x20u>>k))?MAX:open);
        std::int32_t seen=0,prev_back=1;
        std::uint32_t o=u32(w+0x318);
        std::int32_t back;
        if(o){const std::uint32_t n=u32(o+0x318);if(n)o=n;back=1;}
        else{o=u32(w+0x314);back=0;}
        std::uint32_t steps=9;
        for(;;){
            if(!o)break;
            bool next=false;
            if(o==w){back=0;next=true;}
            else{
                float g=f32(0x802af0u+(u32(o)+u32(w)*24u)*4u);
                if(back){if(gt(g,K(0x6281e8u)))next=true;}
                else if(gt(g,K(0x6282ccu)))break;
                if(!next){
                    float dv=ssub(f32(w+0x1c4),f32(o+0x1c4));
                    if(back==1||(back==0&&prev_back==1)){
                        if((u8(o+4)&1u)&&(u32(w+0xc)&0x600u)){
                            if(X87(K(0x628064u))>driving::x87_abs(X87(dv)))break;
                        }
                    }
                    ++seen;prev_back=back;
                    if(back)dv=ssub(0.0f,dv);
                    if(!jb(dv,0.0f))g=ssub(K(0x6282ccu),g);
                    if(gt(K(0x5b4438u),g)){dv=gt(0.0f,dv)?K(0x6280c4u):K(0x62806cu);g=K(0x6282ccu);}
                    const std::int32_t cls=i32(o+0x324);
                    float cost=smul(dv,g);
                    if(cls>=0&&cls<=2&&gt(cost,0.0f))cost=smul(cost,K(0x628244u));
                    const std::int32_t ol=i8(o+0xc32);
                    if(gt(cost,f32(T(ol))))pf(T(ol),cost);
                    const std::uint32_t kind=u32(o+0x5c),row=u8(o+0xd20);
                    const std::int32_t pos=i16(o+0x64);
                    {   const std::uint32_t ta=lane_table(kind,row,ol);
                        for(std::int32_t l=0;l<6;++l){
                            if(l==ol)continue;
                            if(same_point(lane_table(kind,row,l),ta,pos)&&gt(cost,f32(T(l))))pf(T(l),cost);
                        }
                    }
                    const std::int8_t tl=i8(o+0x66);
                    if(tl>=0&&tl!=i8(o+0xc32)){
                        std::int32_t dd=std::int32_t(tl)-ol;dd=dd<0?-dd:dd;
                        if(dd==1){
                            if(gt(cost,f32(T(tl))))pf(T(tl),cost);
                            const std::uint32_t ta=lane_table(kind,row,tl);
                            for(std::int32_t l=0;l<6;++l){
                                if(l==tl)continue;
                                if(same_point(lane_table(kind,row,l),ta,pos)&&gt(cost,f32(T(l))))pf(T(l),cost);
                            }
                        }
                    }
                    next=true;
                }
            }
            if(!next)break;
            o=u32(o+0x314);
            if(++steps>=0x20u)break;
        }
        for(std::uint32_t k=0;k<6;++k)p32(w+0xbf0+k*4,u32(T(std::int32_t(k))));
        if(seen<=0)return std::uint8_t(lane);
        // choose
        std::int8_t bl=lane;
        auto t=[&](std::int32_t l){return f32(T(l));};
        do{
            const std::int32_t s=lane;
            float x0=t(s);
            if(!gt(x0,lim))break;
            const std::int8_t dl=c32;
            const std::int32_t dir=(std::int32_t(c32)-s)>=0?1:-1;
            std::int8_t best=lane;
            for(;;){
                bl=std::int8_t(bl+dir);
                if(bl>=0&&bl<=5&&gt(x0,t(bl))){x0=t(bl);best=bl;}
                if(bl==dl||bl<0||bl>5)break;
            }
            bl=best;
            if(!gt(t(bl),lim))break;
            float x1=t(s);
            std::int8_t cl=lane,d2=lane;
            for(;;){
                cl=std::int8_t(cl-dir);
                if(cl<0||cl>5)break;
                if(gt(x1,t(cl))){x1=t(cl);d2=cl;}
            }
            if(d2!=bl&&gt(x0,x1))bl=d2;
            else d2=bl;
            if(!gt(t(bl),lim))break;
            float x2=t(c32);
            std::int8_t b2=c32;cl=c32;
            for(;;){
                cl=std::int8_t(cl+dir);
                if(cl<0||cl>5)break;
                if(gt(x2,t(cl))){x2=t(cl);b2=cl;}
            }
            bl=(b2!=d2&&gt(x1,x2))?b2:d2;
            if(gt(t(bl),t(s)))bl=lane;
            if(jb(t(bl),t(c32)))break;
            bl=c32;
            if(!gt(t(c32),lim))break;
            bool stop=false;
            for(std::int32_t k=1;k<=3&&!stop;++k){
                std::int8_t lo=std::int8_t(c32-k);if(lo<0)lo=0;
                std::int8_t hi=std::int8_t(c32+k);if(hi>5)hi=5;
                std::int8_t pick;
                if(gt(t(hi),t(lo)))pick=lo;
                else if(!(t(lo)==t(hi)))pick=hi;
                else{
                    std::int32_t dh=std::int32_t(c32)-hi;dh=dh<0?-dh:dh;
                    std::int32_t dlo=std::int32_t(c32)-lo;dlo=dlo<0?-dlo:dlo;
                    pick=dlo<dh?lo:hi;
                }
                if(gt(t(c32),t(pick)))bl=pick;
                if(k<3){
                    if(jb(t(bl),t(c32))){stop=true;break;}
                    bl=c32;
                    if(!gt(t(c32),lim)){stop=true;break;}
                }else if(gt(t(c32),t(bl)))bl=c32;
            }
        }while(false);
        // walk from the current lane toward the choice, stopping before a lane over 5B4438
        if(bl!=c32){
            const std::int8_t dir=std::int8_t(bl>c32?1:-1);
            std::int8_t al=c32;
            for(;;){
                al=std::int8_t(al+dir);
                if(gt(t(al),K(0x5b4438u))){al=std::int8_t(al-dir);break;}
                if(al==bl)break;
            }
            bl=al;
        }
        if(bl<0)return 0;
        return std::uint8_t(bl>5?5:bl);
    }
    // 47AD30(eax = car): racer lane selection +C30 (start / course split / overtakes / player tactics)
    // and the lateral target +B08 toward it. Locals at B+k (B = esp after push ebx/esi): E(k).
    void racer_lanes_47ad30(std::uint32_t w){
        Frame E(*this,0x50);
        const std::uint32_t frames=call(0x44fdf0u,{});
        p8(E(0xb),0xff);
        const std::uint32_t stage=call(0x450380u,{u32(w)});
        p32(E(0xc),frames);p32(E(0x10),stage);
        if(X87(K(0x5b43c4u))>fild_u32(frames))return;                      // 47B5FA
        pf(w+0xb10,0.0f);pf(w+0xbe8,K(0x62806cu));p8(w+0xc33,0);
        p32(w+0xc,u32(w+0xc)&0xfeffb7ffu);
        const std::uint32_t P=E(0x14);
        for(std::uint32_t k=0;k<0x10;k+=4)p32(P+k,u32(w+0x5c+k));
        const std::uint32_t adv=advance_46fe70(P,0x1e);
        enum class Go{Limits,Choose,Tail} go;
        auto start_zone=[&](std::int16_t dx,std::uint8_t near_lane,std::uint8_t far_lane){   // 47AFCF / 47B09E tails
            if(dx<0x26){p8(w+0xc30,near_lane);return;}
            p8(w+0xc30,far_lane);
            pf(w+0xbe8,K(0x5b445cu));                                            // 47AFEB
            if(!(i16(w+0x64)>0x35))p32(w+8,u32(w+8)|0x20u);
        };
        if(adv&&u32(w+0x5c)==0u&&u32(P)==1u&&!(u8(w+8)&2u)){
            // 47ADFF: the course split ahead
            const std::uint32_t route=call(0x451350u,{call(0x44c940u,{u32(E(0x10))})});
            if(route!=2u)p8(w+0xc30,route?4u:1u);
            else if(!u8(w+0xc36))p8(w+0xc30,(frames&1u)?1u:4u);
            else{
                const std::uint32_t lead=leader_car_4773f0();
                if(lead){if(!(u8(lead+4)&1u))p8(w+0xc30,u8(lead+0xc30));}
                else{
                    const std::uint32_t first=first_racer_car_477390();
                    if(first&&!(u8(first+4)&1u)&&first!=w)p8(w+0xc30,u8(first+0xc30));
                }
            }
            p32(w+8,u32(w+8)|2u);
            go=Go::Limits;
        }else{
            // 47AEA6
            p32(w+8,u32(w+8)&0xffffffdbu);
            bool af0b=true;
            if(u32(w+0x5c)==1u){
                const std::uint32_t route=call(0x451350u,{call(0x44c940u,{u32(E(0x10))})});
                const std::uint32_t ecx=u32(w+8)&0xfffffffdu;
                const std::int16_t dx=i16(w+0x64);
                p32(w+8,ecx);
                enum class At{Af0b,Afcf,B08d} at;
                if(dx<0x35){
                    if(u8(w+7)&3u)at=((u8(w+7)&3u)==1u)?At::B08d:At::Af0b;      // 47B06C / 47B080
                    else at=route?At::Afcf:At::Af0b;                              // 47AEFC
                    if(at==At::Af0b&&!(u8(w+7)&3u))p8(w+0xc30,1);
                }else if(u32(w+0x60)==0x64u){
                    at=route?At::Afcf:At::Af0b;
                    if(at==At::Af0b)p8(w+0xc30,1);
                }else at=(u32(w+0x60)==0x65u)?At::B08d:At::Af0b;                  // 47B071
                if(at==At::B08d){
                    if(route==1u)p8(w+0xc30,4);
                    else{p32(w+8,ecx|4u);start_zone(dx,3,1);af0b=false;}
                }else if(at==At::Afcf){p32(w+8,ecx|4u);start_zone(dx,2,4);af0b=false;}
            }
            if(!af0b)go=Go::Limits;
            else{
                // 47AF0B: a lane-change zone point (+C48) ahead
                p32(w+8,u32(w+8)&0xffffffefu);
                go=Go::Choose;
                const std::uint32_t pt=u32(w+0xc48);
                if(pt){
                    sub_40efa0(E(0x14),pt,w+0x14);
                    if(dot_40efd0(E(0x14),w+0xc4c)>X87(K619a34)){
                        const float v=driving::x87_float(lane_offset_479ba0(w+0x5c,pt,u8(w+0xd20),u8(w+0x66)));
                        pf(E(0xc),v);
                        if(!(f32(E(0xc))==K(0x59943cu))&&X87(K(0x628100u))>driving::x87_abs(X87(v))){
                            pf(w+0xb04,v);pf(w+0xb00,0.0f);p32(w+8,u32(w+8)|0x10u);
                            go=Go::Tail;
                        }
                    }
                }
            }
        }
        if(go==Go::Choose){
            // 47B0C2
            std::int8_t bl;
            if(u8(w+8)&0x22u)bl=i8(E(0xb));
            else{
                const std::uint32_t cfg=u32(0x80fb0cu);
                bool chosen=false;
                if(cfg&&(u8(cfg+0x30)&1u)){
                    const std::uint32_t ahead=ftol(x(w+0x1c4)*X87(K(0x628244u))+X87(K(0x628138u)));
                    const std::uint32_t sec=call(0x44dc50u,{u32(E(0x10))});
                    const std::uint32_t at=std::uint16_t(u16(w+0x64)+std::uint16_t(ahead));
                    const std::uint32_t r1=call(0x44f0f0u,{sec,at});
                    const std::uint32_t r0=call(0x44f0f0u,{sec,u16(w+0x64)});
                    p8(w+0xc42,((r0|r1)&&i8(w+0xc32)>=3)?1u:0u);
                    const std::uint8_t l=preload_lane_476280(u32(w+0x5c),u8(w+0xc42),at,0);
                    bl=std::int8_t(l);p8(E(0xb),l);
                    if(bl>=0){p8(w+0xc30,l);chosen=true;}
                }else bl=i8(E(0xb));
                if(!chosen){
                    // 47B17E: the bend radius ahead
                    const float a=driving::x87_float(driving::x87_abs(x(w+0xbb4)));
                    pf(E(0x10),a);
                    if(X87(K(0x5a29ecu))>X87(a))p8(w+0xc30,gt(0.0f,f32(w+0xbb4))?0u:5u);
                    else if(gt(K(0x628178u),f32(E(0x10)))){
                        if(gt(0.0f,f32(w+0xbb4)))p8(w+0xc30,i8(w+0xc30)>2?1u:0u);
                        else p8(w+0xc30,std::uint8_t((i8(w+0xc30)>=3?1u:0u)+4u));
                    }else{
                        const std::int8_t c=i8(w+0xc30);
                        if(c==0)p8(w+0xc30,2);else if(c==5)p8(w+0xc30,3);
                    }
                }
            }
            // 47B220
            p32(w+0xc,u32(w+0xc)|0x8000000u);
            {   const std::uint32_t cfg=u32(0x80fb0cu);
                if(cfg&&(u8(cfg+0x30)&0x18u)){
                    const std::uint32_t r=call(0x495880u,{});
                    if(r&&i32(std::uint32_t(std::int32_t(i8(w+0xc38))*0xa0)+u32(0x80fb00u)+0x98u)>std::int32_t(r))
                        p32(w+0xc,u32(w+0xc)&0xf7ffffffu);
                }
            }
            p8(w+0xc33,route_flags_46f7a0(w));
            const std::uint8_t pick=lane_choice_479f50(w,i8(w+0xc30));
            if(pick!=u8(w+0xc30))p8(w+0xc30,pick);
            else{
                std::uint32_t r=0;
                bool done=false;
                if(u32(w+0xc)&0x200000u){r=tactic_474640(w,bl);bl=i8(E(0xb));if(r)done=true;}
                if(!done){const std::uint32_t f=u32(w+0xc);if((f&0x20000u)&&(f&0x8000000u)){r=lane_474200(w,bl);if(r)done=true;}}
                if(!done){const std::uint32_t f=u32(w+0xc);if((f&0x200u)&&!(f&0x200000u))r=tactic_474700(w);}
                if(bl>=0&&!r&&cfg_mode3_4762e0()){
                    const std::uint32_t kind=u32(w+0x5c);
                    const std::uint8_t l1=preload_lane_476280(kind,u8(w+0xc42),std::uint16_t(u16(w+0x64)+1u),E(0x14));
                    if(std::int8_t(l1)>=0){
                        push_load(call(0x44bed0u,{kind}));
                        const std::uint8_t l2=preload_lane_476280(u32(w+0x5c),u8(w+0xc42),std::uint16_t(u16(w+0x64)+2u),E(0x28));
                        if(std::int8_t(l2)<0)point_40a7d0(E(0x3c),E(0x14));
                        else{
                            point_40a7d0(E(0x14),E(0x14));
                            point_40a7d0(E(0x28),E(0x28));
                            const float k=f32(w+0xbdc);
                            blend_40f180(E(0x3c),E(0x14),k,E(0x28),ssub(K(0x62806cu),k));
                        }
                        pop();
                        const float v=driving::x87_float(lane_offset_479ba0(w+0x5c,E(0x3c),u8(w+0xd20),u8(w+0x66)));
                        pf(E(0x10),v);
                        if(!(f32(E(0x10))==K(0x59943cu)))pf(w+0xb10,v);
                    }
                }
            }
            // 47B408: the second course's lane groups
            if(u32(w+0x5c)==1u&&i32(w+0x60)>=0x64){
                const std::uint32_t s=u8(w+7)&3u;
                const std::int8_t c=i8(w+0xc30);
                if(s==1u){if(c<3)p8(w+0xc30,std::uint8_t(c+3));}
                else if(s==0u){if(c>=3)p8(w+0xc30,std::uint8_t(c-3));}
            }
            // 47B445: no lane change against the current drift
            if((u8(w+8)&8u)&&!(u32(w+0xc)&0x600u)){
                const std::int8_t cl=i8(w+0xc30),al=i8(w+0xc32);
                const bool fix=gt(0.0f,f32(w+0xbe4))?(cl>al):(cl<al);
                if(fix){p8(w+0xc30,std::uint8_t(al));pf(w+0xb10,0.0f);}
            }
            go=Go::Limits;
        }
        if(go==Go::Limits){
            // 47B00D: the ends of the courses
            const std::uint32_t kind=u32(w+0x5c);
            const std::int16_t pos=i16(w+0x64);
            if((kind==0u&&pos<5)||(kind==1u&&pos>0xe6)){
                const std::int8_t c=i8(w+0xc30);
                if(c<1){p8(w+0xc30,1);pf(w+0xb04,0.0f);pf(w+0xb00,0.0f);p32(w+8,(u32(w+8)&0xffdfffffu)|0x100000u);}
                else if(c>4){p8(w+0xc30,4);pf(w+0xb04,0.0f);pf(w+0xb00,0.0f);p32(w+8,(u32(w+8)&0xffefffffu)|0x200000u);}
                if(u32(w+0x5c)==0u&&i16(w+0x64)<8){
                    const std::int8_t a=i8(w+0xc30);
                    if(a>3){p8(w+0xc30,3);p32(w+8,(u32(w+8)&0xffefffffu)|0x200000u);}
                    else if(a<2){p8(w+0xc30,2);p32(w+8,(u32(w+8)&0xffdfffffu)|0x100000u);}
                    pf(w+0xb04,0.0f);pf(w+0xb00,0.0f);
                }
            }else if(kind==1u&&pos<5){
                const std::int8_t a=i8(w+0xc30);
                if(a==2)p8(w+0xc30,1);else if(a==3)p8(w+0xc30,4);
                pf(w+0xb00,0.0f);p32(w+8,u32(w+8)|0x80000u);
            }else p32(w+8,u32(w+8)&0xffc7ffffu);
            // 47B573: the lateral target one lane toward the choice
            const std::uint8_t al=lane_pref_46f730(w,u8(w+0xc30));
            p8(w+0xc30,al);
            if(std::int8_t(al)>=0){
                std::int8_t cl=i8(w+0x66);
                if(cl!=std::int8_t(al)){
                    cl=cl>=std::int8_t(al)?std::int8_t(cl-1):std::int8_t(cl+1);
                    p8(E(0x10),std::uint8_t(cl));
                    const float v=driving::x87_float(lane_offset_479ba0(w+0x5c,w+0x14,u8(w+0xd20),u32(E(0x10))));
                    pf(E(0x10),v);
                    if(!(f32(E(0x10))==K(0x59943cu))){pf(w+0xb08,v);p8(w+0x66,std::uint8_t(cl));}
                }
            }
        }
        // 47B5EB
        const std::uint32_t f8=u32(w+8);
        if(f8&2u)p32(w+8,f8|0x10u);
    }
    // ---- car motion (47BC00) -----------------------------------------------------------------------
    // 40EF10(a, b): a += b.
    void add_40ef10(std::uint32_t a,std::uint32_t b){
        pf(a,driving::x87_float(x(a)+x(b)));
        pf(a+4,driving::x87_float(x(b+4)+x(a+4)));
        pf(a+8,driving::x87_float(x(b+8)+x(a+8)));
    }
    // 46F2F0(f): signed weight 0..1 of a radius below [6281A8].
    float bend_46f2f0(float f){
        Frame L(*this,0x10);
        pf(L(0),driving::x87_float(driving::x87_abs(X87(f))));
        if(X87(f32(L(0)))>X87(K(0x6281a8u)))return 0.0f;
        const float s=gt(0.0f,f)?K(0x6280c4u):K(0x62806cu);
        return smul(smul(ssub(K(0x6281a8u),f32(L(0))),K(0x599438u)),s);
    }
    // three road points ahead of the car (43E570 + 46FE70 steps) for the radius helpers; 43E570 leaves the point at Q+8.
    bool three_points(std::uint32_t w,std::uint32_t hint,std::int32_t step,std::uint32_t P,std::uint32_t Q,std::uint32_t A,std::uint32_t B,std::uint32_t C){
        for(std::uint32_t k=0;k<0x10;k+=4)p32(P+k,u32(w+0x5c+k));
        if(!call(0x43e570u,{Q,P,hint}))return false;
        {const std::uint32_t q0=u32(Q+8),q1=u32(Q+0xc),q2=u32(Q+0x10);p32(A,q0);p32(A+4,q1);p32(A+8,q2);}
        if(!advance_46fe70(P,step))return false;
        if(!call(0x43e570u,{Q,P,hint}))return false;
        {const std::uint32_t q0=u32(Q+8),q1=u32(Q+0xc),q2=u32(Q+0x10);p32(B,q0);p32(B+4,q1);p32(B+8,q2);}
        if(!advance_46fe70(P,step))return false;
        if(!call(0x43e570u,{Q,P,hint}))return false;
        const std::uint32_t q0=u32(Q+8),q1=u32(Q+0xc),q2=u32(Q+0x10);
        p32(C,q0);p32(C+4,q1);p32(C+8,q2);
        return true;
    }
    // 479670(car): road radius ahead +B54 (every 5th frame, +B52 countdown).
    void radius_479670(std::uint32_t w){
        if(i8(w+0xb52)>0){p8(w+0xb52,u8(w+0xb52)-1u);return;}
        const std::int16_t cs=i16(w+0x64);
        const std::uint32_t e=std::uint32_t(end_43d470(u32(w+0x5c)));
        if(std::int32_t(cs)+3>std::int32_t(e)||(u32(w+0x5c)&&(cs<0xa||cs>0xaa))){pf(w+0xb54,K(0x5b436cu));return;}
        Frame E(*this,0xa0);
        if(!three_points(w,u32(w+0x1c0),1,E(8),E(0x3c),E(0x18),E(0x24),E(0x30)))return;
        const float r=(u32(0x80fb14u)&&(u8(w+4)&0x20u))?driving::othcar_calc_r_alt_46f190(vec(E(0x18)),vec(E(0x24)),vec(E(0x30)))
                                                       :driving::othcar_calc_r_46f040(vec(E(0x18)),vec(E(0x24)),vec(E(0x30)));
        pf(w+0xb54,r);
        p8(w+0xb52,4);
    }
    // 479830(car): racer look-ahead radius +BB4 and the bend weights +BB8/+BBC (+C34 countdown).
    void radius_479830(std::uint32_t w){
        const std::int8_t c0=i8(w+0xc34);
        if(c0>0){p8(w+0xc34,u8(w+0xc34)-1u);return;}
        if(c0==1)return;
        const std::int16_t cs=i16(w+0x64);
        const std::uint32_t e=std::uint32_t(end_43d470(u32(w+0x5c)));
        if(std::int32_t(cs)+3>std::int32_t(e)){pf(w+0xbb4,K(0x5b436cu));return;}
        const std::uint32_t step=ftol(x(w+0x1c4)*X87(K(0x6282b8u)));
        if(u32(w+0x5c)==1u&&(cs<0xa||cs>0xaa)){pf(w+0xbb4,K(0x5b436cu));return;}
        const std::uint16_t di=std::uint16_t(step);
        if(di<1u)return;
        if(di==1u){
            const float v=f32(w+0xb54);
            pf(w+0xbb4,v);
            const float r=bend_46f2f0(v);
            pf(w+0xbb8,r);pf(w+0xbbc,r);
            return;
        }
        Frame E(*this,0xa0);
        if(!three_points(w,u32(w+0x230),std::int32_t(step),E(0xc),E(0x44),E(0x38),E(0x20),E(0x2c)))return;
        pf(w+0xbb4,driving::othcar_calc_r_alt_46f190(vec(E(0x38)),vec(E(0x20)),vec(E(0x2c))));
        pf(w+0xbb8,bend_46f2f0(f32(w+0xb54)));
        pf(w+0xbbc,bend_46f2f0(f32(w+0xbb4)));
        p8(w+0xc34,8);
    }
    // 4754F0(esi = car): the body roll +D24 from the steering.
    void roll_4754f0(std::uint32_t w){
        if(u8(w+0x2f0)&2u){pf(w+0xd24,0.0f);return;}
        if(jb(f32(w+0x1c4),K(0x5b4404u)))return;
        const float l=driving::x87_float(driving::x87_abs(X87(driving::x87_float(X87(std::int32_t(i16(w+0xd34)))*X87(K(0x628258u))))));
        const float m=X87(l)>=X87(K(0x628138u))?K(0x628138u):l;
        float d=ssub(smul(m,K(0x5b441cu)),f32(w+0xd24));
        if(gt(d,K(0x6281c0u)))d=K(0x6281c0u);
        else if(gt(K(0x599428u),d))d=K(0x599428u);
        const float n=sadd(f32(w+0xd24),d);
        pf(w+0xd24,n);
        if(gt(n,K(0x5b005cu)))pf(w+0xd24,K(0x5b005cu));
        else if(gt(0.0f,n))pf(w+0xd24,0.0f);
    }
    // 46E740(esi = car): pitch +2C and roll +30 from the road normal (+1074, 43D390).
    void tilt_46e740(std::uint32_t w){
        if(!call(0x4a3f80u,{w,u32(w+0x1c0)}))return;
        Frame E(*this,0x20);
        const std::uint32_t n=w+0x1074;
        call(0x43d390u,{u32(w+0x230),u32(w+0x5c),n});
        push_unit();
        rotate_y(word_radians(-(std::int32_t(i16(w+0xc2c))+std::int32_t(i16(w+0x2e)))));
        vector_40a820(E(4),n);
        pop();
        const float s=sadd(smul(f32(E(8)),f32(E(8))),smul(f32(E(4)),f32(E(4))));
        const float r=driving::x87_float(driving::x87_sqrt(X87(s)));
        p16(w+0x2c,std::uint32_t(cvtt(driving::x87_float(driving::x87_atan2(x(E(0xc)),X87(r))*X87(K(0x6282c0u))))));
        p16(w+0x30,std::uint32_t(cvtt(driving::x87_float(-driving::x87_atan2(x(E(4)),x(E(8)))*X87(K(0x6282c0u))))));
    }
    // 474F10(eax = car+AF0, esi = car): bus look-ahead steps from the speed.
    std::int32_t steps_474f10(std::uint32_t w){
        Frame L(*this,0x10);
        pf(L(0),driving::x87_float(driving::x87_abs(x(w+0xb54))));
        const float s=f32(w+0x1c4);
        if(gt(K(0x5b4410u),s))return 2;
        if(gt(K(0x5b440cu),s))return 3;
        if(gt(K(0x5b4408u),s))return 4;
        if(gt(K(0x5b4404u),s))return 5;
        pf(L(4),smul(smul(ssub(s,K(0x5b4404u)),K(0x5a460cu)),K(0x5b0068u)));
        const float b=f32(L(0));
        float f;
        if(!jb(K(0x5a29e4u),b))f=K(0x62806cu);
        else if(!jb(b,K(0x5afd2cu)))f=0.0f;
        else f=smul(ssub(K(0x6282ccu),ssub(b,K(0x5a29e4u))),K(0x6281c0u));
        pf(L(0),f);
        return std::int32_t(ftol(x(L(0))*x(L(4))))+5;
    }
    // 474DD0(eax = target, esi = pos; &x, &z, scale): pos moved toward target by scale (XZ).
    void toward_474dd0(std::uint32_t tg,std::uint32_t pos,std::uint32_t ox,std::uint32_t oz,float scale){
        Frame L(*this,0x10);
        pf(L(0),ssub(f32(tg+8),f32(pos+8)));
        pf(L(4),0.0f);
        pf(L(8),ssub(f32(pos),f32(tg)));
        const X87 len=unit_40eeb0(L(0));
        if(len==X87(K619a34))pf(L(0),scale);
        else scale_40f050(L(0),L(0),scale);
        add_40ef10(L(0),pos);
        pf(ox,f32(L(0)));
        pf(oz,f32(L(8)));
    }
    // 474E70(eax = pos, edi = car; lateral offset, place): shift pos sideways by the lane offset.
    void shift_474e70(std::uint32_t pos,std::uint32_t w,float off,std::uint32_t place){
        Frame E(*this,0x30);
        if(X87(K(0x6281f0u))>driving::x87_abs(X87(off)))return;
        if(!sample_470120(u32(w+0x5c),u8(w+0xd20),i8(place+0xa),i16(w+0x64),E(0x14)))return;
        push_load(call(0x44bed0u,{u32(place)}));
        point_40a7d0(E(0x14),E(0x14));
        pop();
        toward_474dd0(E(0x14),pos,E(0xc),E(0x10),off);
        pf(pos,f32(E(0xc)));
        pf(pos+8,f32(E(0x10)));
    }
    // 46E380(esi = a, edi = b): cosine of the angle between a and b (0 when either is short).
    X87 cosine_46e380(std::uint32_t a,std::uint32_t b){
        Frame E(*this,0x20);
        const X87 la=len_40f0e0(a);
        pf(E(0),driving::x87_float(la));
        if(X87(K(0x6281c0u))>la)return X87(K619a34);
        scale_40f050(E(0x10),a,sdiv(1.0f,f32(E(0))));
        const X87 lb=len_40f0e0(b);
        pf(E(0),driving::x87_float(lb));
        if(X87(K(0x6281c0u))>lb)return X87(K619a34);
        scale_40f050(E(4),b,sdiv(1.0f,f32(E(0))));
        return dot_40efd0(E(0x10),E(4));
    }
    // 47B600(ecx = car, eax = place out; pos out, sample out): the point the car heads to.
    std::uint32_t target_47b600(std::uint32_t w,std::uint32_t out,std::uint32_t pos,std::uint32_t smp){
        Frame E(*this,0x30);
        std::int32_t back=1,step=2;
        if((u8(w+4)&0x20u)&&u32(0x80fb14u)&&!u8(w+0xb6c)&&!u32(w+0x5c)){
            const std::uint32_t cfg=u32(0x80fb0cu);
            if(!(cfg&&(u8(cfg+0x30)&0x10u))){
                if(gt(K(0x6280b0u),f32(w+0x268))||gt(f32(w+0x264),K(0x5a4608u))){step=1;back=0;}
            }
        }
        for(std::uint32_t k=0;k<0x10;k+=4)p32(out+k,u32(w+0x5c+k));
        if(!advance_46fe70(out,step))return 0;
        std::uint32_t si;
        for(;;){
            si=std::uint32_t(std::uint16_t(u16(out+8)-std::uint16_t(back)))+std::uint32_t(step);
            {const std::uint32_t e=end_raw(u32(out));if(!(std::uint16_t(si)<std::uint16_t(e)))si=e-1u;}
            if(!sample_470120(u32(out),u8(w+0xd20),i8(out+0xa),std::uint16_t(si),pos))return 0;
            push_load(call(0x44bed0u,{u32(out)}));
            point_40a7d0(pos,pos);
            pop();
            const X87 d=dist_40f140(w+0x14,pos);
            si=std::uint32_t(step);
            if(!(x(w+0x1c4)>=d))break;
            if(std::uint16_t(si)>=0x7fffu)break;
            for(std::uint32_t k=0;k<0x10;k+=4)p32(out+k,u32(w+0x5c+k));
            ++si;step=std::int32_t(si);
            if(!advance_46fe70(out,std::int32_t(si)))return 0;
        }
        for(std::uint32_t k=0;k<0xc;k+=4)p32(w+0xaf4+k,u32(pos+k));
        shift_474e70(pos,w,f32(w+0xb08),out);
        const std::uint32_t Q=E(0x18);
        for(std::uint32_t k=0;k<0x10;k+=4)p32(Q+k,u32(out+k));
        for(std::uint32_t k=0;k<0x14;k+=4)p32(smp+k,u32(pos+k));
        if(u8(w+0x2f0)&2u)return 1;
        const std::uint32_t cls=u32(w+0x324);
        if(cls==5u||cls==6u){step=steps_474f10(w);si=std::uint32_t(step);}
        if(!advance_46fe70(Q,std::int32_t(si)))return 1;
        const std::uint32_t kind=u32(Q);
        si=u32(Q+8)+si-1u;
        {const std::uint32_t e=end_raw(kind);if(!(std::uint16_t(si)<std::uint16_t(e)))si=e-1u;}
        if(!sample_470120(kind,u8(w+0xd20),i8(Q+0xa),std::uint16_t(si),smp))return 1;
        push_load(call(0x44bed0u,{kind}));
        point_40a7d0(smp,smp);
        pop();
        shift_474e70(smp,w,f32(w+0xb08),Q);
        return 1;
    }
    // 475040(ebx = car; target, sample): steer toward the target, new velocity +20 / angles / position.
    void steer_475040(std::uint32_t w,std::uint32_t A,std::uint32_t B){
        Frame E(*this,0x40);
        const std::uint32_t pos=w+0x14,D=E(0x10),R=E(0x1c),V=E(0x10),W=E(0x28);
        sub_40efa0(D,pos,A);
        angles_449800(R,D);
        p16(w+0x160,std::uint32_t(cvtt(smul(f32(R+4),K(0x6282c0u)))));
        pf(V+4,0.0f);pf(V,0.0f);pf(V+8,K(0x6280c4u));
        push_unit();rotate_y(f32(R+4));rotate_x(f32(R));
        vector_40a820(V,V);
        pop();
        scale_40f050(V,V,f32(w+0x1c4));
        bool turn=false;
        if(i16(w+0x64)<0xa&&call(0x44c940u,{u32(w+0x68)}))turn=true;
        else if(!(std::int32_t(i16(w+0x64))<end_43d470(u32(w+0x5c))-5))turn=true;
        std::uint32_t esi=A;
        if(turn){
            for(std::uint32_t k=0;k<0xc;k+=4)p32(W+k,u32(w+0x20+k));
            const float o=ssub(offset_473620(w),f32(w+0xb08));
            pf(E(0xc),o);
            if(!u32(w+0xb14)&&gt(f32(w+0x268),0.0f)&&gt(0.0f,f32(w+0x264))){
                const float n=sadd(smul(o,K(0x6280ecu)),f32(w+0xb08));
                pf(w+0xb08,n);
                if(gt(n,K(0x6280c0u)))pf(w+0xb08,K(0x6280c0u));
                else if(gt(K(0x5b43a8u),n))pf(w+0xb08,K(0x5b43a8u));
            }
            pf(E(0xc),driving::x87_float(driving::x87_abs(x(E(0xc)))));
            if(X87(f32(E(0xc)))>X87(K(0x6280c0u)))pf(E(0xc),K(0x6280c0u));
            const X87 c0=cosine_46e380(V,W);
            pf(E(8),driving::x87_float(c0));
            if(X87(K619a34)>c0)pf(E(8),0.0f);
            const float s=driving::x87_float(driving::x87_sqrt(x(E(8))));
            pf(E(8),s);
            float k=sadd(smul(smul(smul(ssub(K(0x62806cu),s),f32(E(0xc))),f32(w+0x1c4)),K(0x628218u)),smul(s,K(0x6280ecu)));
            if(gt(k,K(0x62806cu)))k=K(0x62806cu);
            pf(E(8),k);
            scale_40f050(V,V,k);
            scale_40f050(W,W,ssub(K(0x62806cu),f32(E(8))));
            add_40ef10(V,W);
            pf(E(0xc),driving::x87_float(len_40f0e0(V)));
            if(X87(f32(E(0xc)))>X87(K(0x6281f0u)))scale_40f050(V,V,sdiv(f32(w+0x1c4),f32(E(0xc))));
            angles_449800(R,V);
            const float y=f32(R+4);
            pf(R+4,gt(y,K619a34)?ssub(y,K(0x6280c8u)):sadd(y,K(0x6280c8u)));
            p16(w+0x160,std::uint32_t(cvtt(smul(f32(R+4),K(0x6282c0u)))));
            sub_40efa0(A,pos,V);
            esi=A;
        }else pf(E(8),K(0x62806cu));
        add_40ef10(pos,V);
        pf(w+0x20,f32(V));pf(w+0x24,f32(V+4));pf(w+0x28,f32(V+8));
        p16(w+0x2c,std::uint32_t(cvtt(smul(f32(R),K(0x6282c0u)))));
        p16(w+0x30,std::uint32_t(cvtt(smul(f32(R+8),K(0x6282c0u)))));
        float t=0.0f;
        if(driving::x87_abs(x(w+0x1c4))>X87(K(0x6281f0u))){
            sub_40efa0(V,esi,B);
            const float r2=sadd(smul(f32(V+8),f32(V+8)),smul(f32(V),f32(V)));
            if(gt(r2,K(0x6281f0u)))pf(E(0xc),driving::x87_float(driving::x87_atan2(x(V),x(V+8))));
            else pf(E(0xc),f32(R+4));
            const float a=wrap_449470(driving::x87_float(x(E(0xc))-X87(std::int32_t(i16(w+0x2e)))*X87(K(0x628254u))));
            pf(E(0xc),driving::x87_float(X87(a)*x(E(8))));
            pf(E(8),driving::x87_float(dist_40f140(pos,esi)/x(w+0x1c4)));
            t=f32(E(0xc));
            if(X87(f32(E(8)))>X87(K(0x62806cu)))t=sdiv(t,f32(E(8)));
        }
        if(gt(t,K(0x5b4418u)))t=K(0x5b4418u);
        else if(gt(K(0x5b4414u),t))t=K(0x5b4414u);
        p16(w+0x2e,u16(w+0x2e)+std::uint16_t(cvtt(smul(smul(t,K(0x62821cu)),K(0x6282c0u)))));
        p16(w+0x162,std::uint16_t(u16(w+0x160)-u16(w+0x2e)));
    }
    // 47BA30(eax = car; place): put the car on the ground at its new position and update the course place.
    void land_47ba30(std::uint32_t w,std::uint32_t place){
        Frame E(*this,0x30);
        const std::uint32_t L0=E(0xc),P=E(0x10),R=E(0x20);
        p32(P,u32(w+0x14));p32(P+4,u32(w+0x18));p32(P+8,u32(w+0x1c));
        p32(L0,u32(w+0x230));
        p32(R,u32(w+0x248));
        const std::uint32_t kind=call(0x43eb60u,{0x100,P,L0,0,R});
        const std::uint32_t res=u32(R);
        if(res!=1u){
            p32(w+0x14,u32(P));p32(w+0x18,u32(P+4));p32(w+0x1c,u32(P+8));
            p32(w+0x230,u32(L0));p32(w+0x248,res);p32(w+0x5c,kind);
            if(res&0xf00002u)p32(w+0x1c0,u32(L0));
        }
        if((u8(w+4)&0x20u)&&u32(0x80fb14u))p16(w+0x25c,call(0x43d440u,{u32(w+0x5c),u32(w+0x230)}));
        {
            const std::uint32_t poly=u32(w+0x230);
            p16(w+0x64,poly==0xffffffffu?0u:u16(u32(0x780228u+u32(w+0x5c)*4u)+poly*2u));
        }
        const std::uint32_t k=u32(w+0x5c);
        if(k==u32(place))p8(w+0x66,u8(place+0xa));
        {
            const std::int8_t l=i8(w+0x66);
            std::uint32_t b;
            if(!k)b=0;
            else if(l<3)b=0x64;
            else if(l<6)b=0x65;
            else b=l>=9?0x67u:0x66u;
            p32(w+0x60,b);
        }
        if(u32(w+0x184)&&!k&&u32(w+0x190)==u32(w+0x68))p32(w+0x68,call(0x44bdd0u,{}));
        if(i32(w+0x190)>i32(w+0x68))p32(w+0x68,u32(w+0x190));
        p32(w+4,u32(w+4)&0xfffdffffu);
        wall_504e70(w);
        bool tilt=true;
        if(!u32(w+0x5c)){
            const std::uint32_t c=u32(w+0x320);
            if(!jb(f32(w+0x300),K(0x628324u))||c==0u||c==1u)tilt=!settle_46e4b0(w);
        }
        if(tilt)tilt_46e740(w);
        if((u8(w+4)&0x20u)&&u32(0x80fb14u))radius_479830(w);
        radius_479670(w);
        roll_4754f0(w);
    }
    // 47BC00(eax = car): one motion step; 0 when no target point (the caller marks the car closing).
    std::uint32_t move_47bc00(std::uint32_t w){
        Frame E(*this,0x40);
        const std::uint32_t f=u32(w+4);
        if((f&0x40u)&&(f&0x20u)){
            const std::uint32_t pl=player();
            if(u32(w+0x68)!=u32(pl+0x68))return 1;
            if(u32(w+0x5c)!=u32(pl+0x5c))return 1;
            if(!u32(w+0x5c)&&std::int32_t(i16(w+0x64))>=end_43d470(0))return 1;
        }
        if(!target_47b600(w,E(4),E(0x28),E(0x14)))return 0;
        steer_475040(w,E(0x28),E(0x14));
        land_47ba30(w,E(4));
        return 1;
    }
#include "platform/race_traffic_heart.inc"
#include "platform/race_traffic_heart_meter.inc"
#include "platform/race_traffic_heart_setup.inc"
#include "platform/race_traffic_heart_eval.inc"
#include "platform/race_traffic_heart_spawn.inc"
#include "platform/race_traffic_heart_main.inc"
#include "platform/race_traffic_objects.inc"
#include "platform/race_traffic_heart_display.inc"
#include "platform/race_traffic_score.inc"
#include "platform/race_traffic_visibility.inc"
#include "platform/race_traffic_network.inc"
};
}
void traffic_init_47dac0(PcRaceContext& c){T t(c);t.init_47dac0();}
void traffic_spawn_47d8b0(PcRaceContext& c){T t(c);t.spawn_47d8b0();}
void traffic_mark_4b0330(PcRaceContext& c){T t(c);t.mark_traffic_4b0330();}
void traffic_ranks_477510(PcRaceContext& c){T t(c);t.ranks_477510();}
void traffic_order_477c90(PcRaceContext& c){T t(c);t.order_477c90();}
void traffic_appear_47c8a0(PcRaceContext& c){T t(c);t.appear_47c8a0();}
void traffic_tables_4ef890(PcRaceContext& c){T t(c);t.tables_4ef890();}
void traffic_control_47ec00(PcRaceContext& c){T t(c);t.control_47ec00();}
void traffic_car_init_4ad280(PcRaceContext& c,std::uint32_t car){T t(c);t.car_init_4ad280(car);}
void traffic_car_init(PcRaceContext& c,std::uint32_t callback,std::uint32_t car){
    T t(c);
    if(callback==0x4ad280u)t.car_init_4ad280(car);
    else if(callback==0x4704a0u)t.car_init_4704a0(car);
    else if(callback==0x4ad310u)t.car_init_4ad310(car);
    else throw std::logic_error("race traffic: car init callback outside 4AD280 / 4704A0 / 4AD310");
}
std::uint32_t traffic_appear_rec_4efb90(PcRaceContext& c,std::uint32_t place){T t(c);return t.appear_rec_4efb90(place);}
std::uint32_t traffic_speed_rec_4f0030(PcRaceContext& c,std::uint32_t place,std::uint32_t rec){T t(c);return t.speed_rec_4f0030(place,rec);}
std::uint32_t traffic_progress_4a6cf0(PcRaceContext& c,std::uint32_t car){T t(c);return bits(driving::x87_float(t.progress_4a6cf0(car)));}
void traffic_car_control_47e780(PcRaceContext& c,std::uint32_t car){T t(c);t.control_47e780(car);}
bool traffic_network_call(PcRaceContext& c,std::uint32_t pc,std::uint32_t ecx,const std::uint32_t* a,std::size_t n,std::uint32_t& eax){
    T t(c);
    auto arg=[&](std::size_t k){return k<n?a[k]:0u;};
    switch(pc){
    case 0x45a2b0u:eax=t.rank_45a2b0(arg(0));return true;
    case 0x456790u:eax=t.remaining_456790(ecx);return true;
    case 0x456720u:t.reset_456720(ecx,arg(0));eax=0;return true;
    case 0x4568e0u:eax=t.best_4568e0(ecx,arg(0),arg(1),arg(2));return true;
    case 0x4569d0u:eax=t.late_4569d0(ecx,arg(0));return true;
    case 0x459c30u:t.next_459c30(ecx);eax=0;return true;
    case 0x457770u:t.progress_457770();eax=0;return true;
    case 0x456e00u:eax=t.u32(0x7de478u+arg(0)*0x6cu);return true;
    case 0x43f730u:eax=t.place_43f730(arg(0),arg(1),arg(2));return true;
    case 0x4a5f10u:t.lan_car_init_4a5f10(arg(0));eax=0;return true;
    case 0x4a60d0u:t.lan_car_control_4a60d0(arg(0));eax=0;return true;
    case 0x4f9ea0u:t.collide_4f9ea0();eax=0;return true;
    default:return false;
    }
}
std::uint32_t traffic_control_part(PcRaceContext& c,std::uint32_t pc,std::uint32_t a0,std::uint32_t a1,std::uint32_t a2){
    T t(c);
    switch(pc){
    case 0x47eba0u:t.traffic_47eba0();return 0;
    case 0x4791b0u:t.pass_4791b0();return 0;
    case 0x479500u:t.lod_479500();return 0;
    case 0x479580u:t.gap_479580();return 0;
    case 0x47ca40u:t.appear_47ca40();return 0;
    case 0x477e00u:t.leader_477e00();return 0;
    case 0x478b90u:return t.fixed_478b90(a0);
    case 0x4787e0u:t.spawn_4787e0(a0,a1);return 0;
    case 0x47e6c0u:t.ai_47e6c0(a0);return 0;
    case 0x476260u:t.hit_476260(a0,a1);return 0;
    case 0x474d30u:t.trim_474d30(a0);return 0;
    case 0x470360u:t.shadow_470360(a0);return 0;
    case 0x470570u:t.inside_470570(a0);return 0;
    case 0x470760u:t.body_470760(a0);return 0;
    case 0x472450u:t.traffic_472450(a0,a1,a2);return 0;
    case 0x472fe0u:t.bus_472fe0(a0,a2);return 0;
    case 0x47a7c0u:t.racer_47a7c0(a0,a2);return 0;
    case 0x47e110u:t.lanes_47e110(a0,a1,a2);return 0;
    case 0x47bc00u:return t.move_47bc00(a0);
    case 0x47ad30u:t.racer_lanes_47ad30(a0);return 0;
    case 0x476760u:t.ranking_476760(a0);return 0;
    case 0x504e70u:t.wall_504e70(a0);return 0;
    case 0x46e4b0u:return t.settle_46e4b0(a0);
    case 0x46e740u:t.tilt_46e740(a0);return 0;
    case 0x4f9450u:t.collide_4f9450();return 0;
    case 0x4704a0u:t.car_init_4704a0(a0);return 0;
    case 0x46f990u:return std::uint32_t(t.diff_46f990(a0,a1));
    case 0x4ad310u:t.car_init_4ad310(a0);return 0;
    case 0x46d250u:t.ha_pairs_46d250();return 0;
    case 0x45e610u:t.request_45e610();return 0;
    case 0x45f3d0u:t.request_45f3d0();return 0;
    case 0x45f540u:t.request_45f540();return 0;
    case 0x45e8a0u:t.request_45e8a0();return 0;
    case 0x45eeb0u:t.request_45eeb0();return 0;
    case 0x45efb0u:t.request_45efb0();return 0;
    case 0x45f180u:t.request_45f180();return 0;
    case 0x4617a0u:t.request_4617a0();return 0;
    case 0x461850u:t.request_461850();return 0;
    case 0x462410u:t.request_462410();return 0;
    case 0x4626e0u:t.request_4626e0();return 0;
    case 0x45cca0u:t.request_45cca0();return 0;
    case 0x461940u:t.request_461940();return 0;
    case 0x45fb80u:t.heart_meter_45fb80();return 0;
    case 0x45b950u:t.effects_45b950();return 0;
    case 0x460ad0u:t.banner_460ad0();return 0;
    case 0x464d20u:t.heart_control_464d20();return 0;
    case 0x462cd0u:t.heart_display_462cd0();return 0;
    case 0x46ed20u:t.pairs_46ed20();return 0;
    case 0x4f2df0u:t.score_4f2df0();return 0;
    case 0x4763d0u:t.release_4763d0(a0);return 0;
    case 0x479670u:t.radius_479670(a0);return 0;   // othcarGetR for the player car (CommonPlCar 4A8100)   // the mission manager releases a racer car (496A30)
    case 0x4f2b20u:t.stage_bonus_4f2b20(std::int32_t(a0));return 0;
    case 0x4f2860u:t.pass_cars(true);return 0;
    case 0x4f2660u:t.pass_cars(false);return 0;
    case 0x4f2cd0u:t.stats_4f2cd0();return 0;
    case 0x4f0e40u:t.objects_4f0e40();return 0;
    case 0x4603f0u:t.result_sheet_4603f0();return 0;
    case 0x45b3d0u:t.markers_45b3d0();return 0;
    case 0x45d190u:t.car_hearts_45d190();return 0;
    case 0x45bdf0u:t.face_45bdf0();return 0;
    case 0x45b2a0u:t.road_props_45b2a0();return 0;
    case 0x45eca0u:t.goal_models(false);return 0;
    case 0x45e9b0u:t.goal_models(true);return 0;
    case 0x45c7e0u:return t.quest_class_45c7e0(a0);
    case 0x464350u:t.requests_464350();return 0;
    case 0x45f7f0u:t.request_45f7f0();return 0;
    case 0x45d2a0u:t.request_45d2a0();return 0;
    case 0x46db60u:t.ha_markers_46db60();return 0;
    case 0x4739f0u:t.chase_4739f0(a0,a2,a1);return 0;
    case 0x473f30u:t.recover_473f30(a0);return 0;
    case 0x47dc10u:t.decide_47dc10(a0,a1,a2);return 0;
    default:throw std::logic_error("race traffic: unknown control part");
    }
}
bool traffic_object_callback(PcRaceContext& c,std::uint32_t callback,std::uint32_t work){T t(c);return t.object_callback(callback,work);}
std::uint32_t traffic_object_debug(PcRaceContext& c,std::uint32_t pc,std::uint32_t a0,std::uint32_t a1,std::uint32_t a2,std::uint32_t a3,std::uint32_t a4){T t(c);return t.object_debug(pc,a0,a1,a2,a3,a4);}
void traffic_qsort_580cb0(PcRaceContext& c,std::uint32_t base,std::uint32_t num,std::uint32_t width,std::uint32_t compare){T t(c);t.qsort_580cb0(base,num,width,compare);}
}
