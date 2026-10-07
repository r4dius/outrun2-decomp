// OUTRUN2SP arcade frontend (OR2006C2C.EXE 4BEE60..4C4880): event 4 functions 1..12 (table
// 59BE78, [655B5C + k * 4] = k + 1) driven by the event-4 work 780440 (+0 sequence, +4 function,
// +8 flags, +A request bits, +15 sequence index) and mode 10 (48AE90).
// Line-by-line transliteration over PC addresses (PcRaceMemory), like race_end_modes.cpp:
// every PC function outside the module is one PcRaceService call. Register arguments of the
// module's own helpers are explicit parameters.
#include "platform/arcade_attract.hpp"
#include "platform/race_end_modes.hpp"
#include <array>
#include <cstring>
namespace outrun::platform {
namespace {
std::uint32_t call(PcRaceContext& c,std::uint32_t pc,std::initializer_list<std::uint32_t> args,std::uint32_t ecx=0){
    PcRaceCall k;k.pc=pc;k.ecx=ecx;k.argc=std::uint32_t(args.size());
    std::size_t i=0;for(auto a:args)k.args[i++]=a;
    return c.service(k);
}
std::uint32_t fb(float f){std::uint32_t v;std::memcpy(&v,&f,4);return v;}
// cvttss2si: truncation; NaN and out-of-range give 0x80000000.
std::int32_t cvtt(float f){
    if(!(f>-2147483904.0f&&f<2147483648.0f))return std::int32_t(0x80000000u);
    return std::int32_t(f);
}
constexpr std::uint32_t Object84a320=0x84a320u;        // 446xxx embedded slots (4 x 0xA0 at +40C)
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
// The 5C7170 animation rows {token, first, last}.
std::uint32_t row(std::uint32_t i){return 0x5c7170u+i*12u;}
std::uint32_t create_row(PcRaceContext& c,std::uint32_t r,std::uint32_t layer,std::uint32_t mode){
    auto& m=c.m;return call(c,0x428460u,{m.u32(r),layer,mode,m.u32(r+4u),m.u32(r+8u)});
}
}
// ---- helpers 4BEE60..4BF8D0 --------------------------------------------------------------
// 4BEE60(esi id): 424940 unless 4493C0 and 6 <= id <= 0x3F.
void arcade_sound_4bee60(PcRaceContext& c,std::uint32_t id){
    if(call(c,0x4493c0u,{})&&id>=6u&&id<=0x3fu)return;
    call(c,0x424940u,{id});
}
// 4BEE80: 4B8C00 while [84A210] == 0.
void arcade_4bee80(PcRaceContext& c){if(!c.m.u32(0x84a210u))call(c,0x4b8c00u,{});}
// 4BEE90(eax handle, cl rows, edx item, xmm2 x, xmm4 y, s0 scale, s1 word): the list layout
// 84A260..84A2A4.
void arcade_list_4bee90(PcRaceContext& c,std::uint32_t eax,std::int8_t cl,std::uint32_t edx,float x2,float x4,float s0,std::uint32_t s1){
    auto& m=c.m;
    m.putf(0x84a26cu,m.f32(0x62806cu));
    float t=m.f32(0x689aa4u)+m.f32(0x689aa0u);
    m.put32(0x84a298u,eax);m.put32(0x84a29cu,eax);
    m.put8(0x84a291u,std::uint8_t(cl));
    t=(t+x2)*m.f32(0x6280b0u);
    m.put32(0x84a2a0u,s1);
    m.putf(0x84a278u,t);
    float x1=float(std::int32_t(cl))*t;
    const float x0=t*m.f32(0x628064u)+x2;
    m.put8(0x84a260u,2u);m.put8(0x84a261u,2u);
    m.putf(0x84a27cu,x1);
    m.putf(0x84a264u,x0);m.putf(0x84a270u,x0);
    x1=x1+x4;
    m.putf(0x84a268u,0.0f);
    m.put8(0x84a28cu,1u);m.put8(0x84a28du,0);m.put8(0x84a28eu,0);m.put8(0x84a28fu,0);m.put8(0x84a290u,0);m.put8(0x84a2a4u,0);
    m.putf(0x84a274u,x2);m.putf(0x84a280u,x4);m.putf(0x84a284u,x1);m.putf(0x84a288u,0.0f);
    m.put8(0x84a293u,0);m.putf(0x84a294u,s0);m.put8(0x84a292u,0xau);
    m.put8(edx+0x19u,0);m.put8(edx+0x1au,0);
}
// 4BEF90(eax row): the list row's offset to the current scroll.
float arcade_row_4bef90(PcRaceContext& c,std::int32_t r){
    auto& m=c.m;
    if(r<0)r=0;
    else{const std::int32_t n=std::int32_t(m.i8(0x84a291u));if(!(r<n))r=n-1;}
    const float x1=m.f32(0x84a274u)*m.f32(0x6280b0u);
    float x0=float(r)*m.f32(0x84a278u);
    x0=x0+x1;x0=x0+m.f32(0x689aa4u);x0=x0+m.f32(0x689aa0u);x0=x0-m.f32(0x84a264u);
    return x0;
}
// 4BEFE0(limit, reset): while the scroll speed [84A268] is ~0, [84A270] counts up to limit.
void arcade_4befe0(PcRaceContext& c,float limit,float reset){
    auto& m=c.m;
    const float v=m.f32(0x84a268u);
    if(!(m.f32(0x5a29e0u)>v))return;
    if(!(v>m.f32(0x5c9404u)))return;
    const float t=m.f32(0x84a270u)+m.f32(0x62806cu);
    m.putf(0x84a270u,t);
    if(!(t>=limit))return;
    m.putf(0x84a270u,reset);
}
// 4BF030(eax work): the item / cursor animations once the previous ones finished (428880 == 3).
void arcade_4bf030(PcRaceContext& c,std::uint32_t w){
    auto& m=c.m;
    const std::uint32_t rec=m.u32(0x5c91dcu+std::uint32_t(std::int32_t(m.i8(w+0x19u)))*4u);
    if(m.i32(0x8449ccu)>=0&&call(c,0x428880u,{m.u32(0x8449ccu)})==3u){
        const std::uint32_t r=row(m.u32(rec+8u));
        call(c,0x4285a0u,{m.u32(0x8449ccu)});
        if(m.u32(0x844958u)){
            call(c,0x4285a0u,{m.u32(0x8449c4u)});
            const std::uint32_t v=m.u32(0x780258u);
            std::uint32_t tok;
            if(v==3u||v==4u){const std::uint32_t a=call(c,0x455ad0u,{})&0xffu;tok=m.u32(0x689aacu+(call(c,0x455b00u,{a})&0xffu)*4u);}
            else tok=0x2e0019u;
            const std::int32_t y=cvtt(m.f32(0x84a284u)),x=cvtt(m.f32(0x84a280u));
            m.put32(0x8449c4u,call(c,0x428460u,{tok,7u,2u,std::uint32_t(x),std::uint32_t(y)}));
        }
        m.put32(0x8449ccu,create_row(c,r,3u,0u));
        m.put32(0x844958u,0);
    }
    if(m.i32(0x84497cu)>=0&&call(c,0x428880u,{m.u32(0x84497cu)})==3u){
        call(c,0x4285a0u,{m.u32(0x84497cu)});
        const std::uint32_t k=m.u32(0x84a2b8u);
        m.put32(0x84497cu,0xffffffffu);
        if(k!=0xffffffffu){
            const std::uint32_t r=row(k);
            const std::uint32_t first=m.u32(r+4u),last=m.u32(r+8u);
            std::uint32_t h;
            if(m.f32(0x84a20cu)>m.f32(0x628064u))h=call(c,0x428460u,{m.u32(r),4u,1u,first,last});
            else h=call(c,0x428460u,{m.u32(r),4u,1u,last,first});
            m.put32(0x844980u,h);
            call(c,0x428800u,{h,m.u32(0x84a20cu)});
            return;
        }
        m.put32(0x844984u,create_row(c,row(m.u32(rec+0x1cu)),4u,0u));
        m.put32(0x84a2b8u,0xffffffffu);m.putf(0x84a20cu,m.f32(0x62806cu));
    }
    if(m.i32(0x844980u)>=0&&call(c,0x428880u,{m.u32(0x844980u)})==3u){
        call(c,0x4285a0u,{m.u32(0x844980u)});
        m.put32(0x844980u,0xffffffffu);
        m.put32(0x844984u,create_row(c,row(m.u32(rec+0x1cu)),4u,0u));
        m.put32(0x84a2b8u,0xffffffffu);m.putf(0x84a20cu,m.f32(0x62806cu));
    }
}
// 4BF260: the next of the ten 689AD4 models (448AD0 then 448960 until loaded; 84A2E8 states).
void arcade_models_4bf260(PcRaceContext& c){
    auto& m=c.m;
    const std::uint32_t i=m.u32(0x689b24u);
    if(i>=0xau)return;
    const std::uint32_t st=m.u32(0x84a2e8u+i*4u);
    if(st==0u){
        call(c,0x448ad0u,{call(c,0x46bbe0u,{m.u32(0x689ad4u+i*4u)}),0u});
        m.put32(0x84a2e8u+m.u32(0x689b24u)*4u,1u);
        return;
    }
    if(st!=1u)return;
    if(!call(c,0x448960u,{call(c,0x46bbe0u,{m.u32(0x689ad4u+i*4u)})}))return;
    const std::uint32_t e=m.u32(0x689b24u);
    m.put32(0x84a2e8u+e*4u,2u);m.put32(0x689b24u,e+1u);
}
// 4BF2E0(model): 40EC60(0), 44C0A0(10), 49FA60, 44C0D0, the model and the resources BA / BB.
void arcade_4bf2e0(PcRaceContext& c,std::uint32_t model){
    call(c,0x40ec60u,{0u});call(c,0x44c0a0u,{0xau});call(c,0x49fa60u,{});call(c,0x44c0d0u,{});
    call(c,0x448ad0u,{call(c,0x46bbe0u,{model}),0u});
    call(c,0x448ad0u,{0xbau,0u});call(c,0x448ad0u,{0xbbu,0u});
}
// 4BF340(esi, bl): 49FA60, 4406A0(esi, bl), event 0x181 function 0xD (the protected push at
// 4BF34E), 8449F0 = esi, 844941 = bl.
void arcade_4bf340(PcRaceContext& c,std::uint32_t esi,std::uint8_t bl){
    auto& m=c.m;
    call(c,0x49fa60u,{});
    call(c,0x4406a0u,{esi,bl});
    call(c,0x440110u,{0x181u,0xdu});
    m.put32(0x8449f0u,esi);m.put8(0x844941u,bl);
}
// 4BF370(edx key, cl kind): the 1-based index of key in the 84A208 table (+0x14 stride 0xC);
// key 4 stands for 5 for the kinds 4 / 6 / 8 / 9.
std::uint32_t arcade_index_4bf370(PcRaceContext& c,std::uint32_t key,std::uint8_t kind){
    auto& m=c.m;
    std::uint32_t n=0;
    if(key==4u&&(kind==4u||kind==6u||kind==8u||kind==9u))key=5u;
    std::uint32_t p=m.u32(0x84a208u)+8u;
    for(;;){const std::uint32_t v=m.u32(p+0xcu);p+=0xcu;++n;if(v==key)break;}
    return n;
}
// 4BF3B0 (sequence step 6): 2E004F, 4493C0, sound 0.
void arcade_step6_4bf3b0(PcRaceContext& c){
    auto& m=c.m;
    m.put32(0x844990u,call(c,0x428460u,{0x2e004fu,8u,1u,0u,0xeu}));
    call(c,0x4493c0u,{});
    call(c,0x424940u,{0u});
    m.put32(0x844958u,1u);
}
// 4BF400(edi work): the class banner 84A23C and its 2E0024 frame.
void arcade_4bf400(PcRaceContext& c,std::uint32_t w){
    auto& m=c.m;
    const std::uint32_t k=std::uint32_t(std::int32_t(m.i8(w+0x3eu)));
    call(c,0x4285a0u,{m.u32(0x8449ccu)});
    call(c,0x4285a0u,{m.u32(0x8449d8u)});
    static constexpr std::uint32_t T[5]{0x689dacu,0x689db8u,0x689dc4u,0x689dd0u,0x689ddcu};
    const std::uint32_t sel=std::uint32_t(std::int32_t(m.i8(w+0x19u)));
    const std::uint32_t r=row(m.u32((sel<=3u?T[sel]:T[4])+k*0x54u));
    m.put32(0x8449ccu,call(c,0x428460u,{m.u32(0x84a23cu),4u,0u,m.u32(r+4u),m.u32(r+8u)}));
    m.put32(0x8449d8u,call(c,0x428460u,{0x2e0024u,9u,1u,m.u32(r+4u),m.u32(r+8u)}));
}
// 4BF4D0: the four slots of 84A320 (446D90: 4, 0x295, 8, 0x296).
void arcade_slots_4bf4d0(PcRaceContext& c){
    call(c,0x446d90u,{4u,0x295u,8u,0x296u,0xffffffffu,0xffffffffu,0xffffffffu,0xffffffffu},Object84a320);
}
// 4BF500 (function 0xB control): request bit 1.
void arcade_fn11_ctrl_4bf500(PcRaceContext& c,std::uint32_t w){c.m.put8(w+0xau,c.m.u8(w+0xau)|2u);}
// 4BF510 (function 0xB destroy): 447090 on 84A320, music off; once flags 3: events / packs go
// and mode 32 (SUMO_FE) is requested.
void arcade_fn11_dest_4bf510(PcRaceContext& c,std::uint32_t w){
    auto& m=c.m;
    call(c,0x447090u,{},Object84a320);
    call(c,0x401030u,{0u});
    if((m.u8(w+8u)&3u)!=3u)return;
    call(c,0x4401d0u,{0x17fu});
    call(c,0x42dfb0u,{0x2fu});call(c,0x4299c0u,{0x2fu});call(c,0x42dfb0u,{0x15u});
    call(c,0x43f8c0u,{0x20u});
    m.put8(w+8u,m.u8(w+8u)&0xfcu);
}
// 4BF570: the player entry marks 2C00E1 / 2C00E0 (456D60 players, 455BF0 / 455B60 / 455B00).
void arcade_entries_4bf570(PcRaceContext& c){
    auto& m=c.m;
    auto count=[&]{return call(c,0x456d60u,{})&0xffu;};
    auto mark=[&](std::uint32_t tok,std::uint32_t base,std::uint32_t i){
        const std::uint32_t a=call(c,0x455b00u,{i})&0xffu;
        const float y=float(std::int32_t(a))*m.f32(0x5b4440u)+m.f32(base);
        call(c,0x429530u,{tok,0x41000000u,fb(y),5u,a});
    };
    bool all=true;
    if(count()){for(std::uint32_t i=0;;){if(!call(c,0x455bf0u,{i})){all=false;break;}++i;if(!(i<count()))break;}}
    if(!all){
        if(count())for(std::uint32_t i=0;;){if(call(c,0x455b60u,{i}))mark(0x2c00e0u,0x5c6fbcu,i);++i;if(!(i<count()))break;}
        if(count())for(std::uint32_t i=0;;){if(call(c,0x455b60u,{i}))mark(0x2c00e1u,0x5c6fb8u,i);++i;if(!(i<count()))break;}
        return;
    }
    bool a=true,b=true;
    if(count())for(std::uint32_t i=0;;){if(!call(c,0x455b60u,{i})){a=false;break;}++i;if(!(i<count()))break;}
    if(count())for(std::uint32_t i=0;;){if(!call(c,0x455b60u,{i})){b=false;break;}++i;if(!(i<count()))break;}
    if(a&&count())for(std::uint32_t i=0;;){mark(0x2c00e1u,0x5c6fb8u,i);++i;if(!(i<count()))break;}
    if(b&&count())for(std::uint32_t i=0;;){mark(0x2c00e0u,0x5c6fbcu,i);++i;if(!(i<count()))break;}
}
// 4BF780: the two 5C7194 / 5C71DC (or 5C7224 / 5C726C for [84A2BC] == 2) arrows at the
// 689ED8 + [84A2BC] * 16 positions, by the 8449F4 bits.
void arcade_arrows_4bf780(PcRaceContext& c){
    auto& m=c.m;
    const std::int8_t k=m.i8(0x84a2bcu);
    std::uint32_t a=0x5c7194u,b=0x5c71dcu;
    if(k==2){a=0x5c7224u;b=0x5c726cu;}
    const std::uint32_t e=std::uint32_t(std::int32_t(k))*16u;
    const float ax=m.f32(0x689ed8u+e),ay=m.f32(0x689edcu+e),bx=m.f32(0x689ee0u+e),by=m.f32(0x689ee4u+e);
    for(std::uint32_t h:{0x8449acu,0x8449b0u,0x8449b4u,0x8449b8u})call(c,0x4285a0u,{m.u32(h)});
    for(std::uint32_t h:{0x8449acu,0x8449b0u,0x8449b4u,0x8449b8u})m.put32(h,0xffffffffu);
    Locals l(m);
    if(m.u8(0x8449f4u)&2u){
        m.put32(0x8449b4u,create_row(c,a,0xau,1u));
        translation(m,PcRaceEndLocals,ax,ay,0.0f);
        call(c,0x4287b0u,{m.u32(0x8449b4u),PcRaceEndLocals});
    }
    if(m.u8(0x8449f4u)&1u){
        m.put32(0x8449b8u,create_row(c,b,0xau,1u));
        translation(m,PcRaceEndLocals,bx,by,0.0f);
        call(c,0x4287b0u,{m.u32(0x8449b8u),PcRaceEndLocals});
    }
}
// 4BF8D0(edx work): the hint timers 84A2B4 / 8449E4 / 84494C of the current sequence code.
void arcade_hint_4bf8d0(PcRaceContext& c,std::uint32_t w){
    auto& m=c.m;
    const std::uint32_t v=m.u32(m.u32(w)+std::uint32_t(std::int32_t(m.i8(w+0x15u)))*4u);
    if(std::int32_t(m.i16(0x8449f8u))!=std::int32_t(v)){
        m.put16(0x8449f8u,std::uint16_t(v));m.put16(0x84a2b4u,0);m.put16(0x8449e4u,0xfu);m.put16(0x84494cu,0);
    }
    std::uint32_t t;
    switch(v){case 4:case 5:case 8:t=0xfu;break;case 6:t=1u;break;default:return;}
    if(m.u8(w+0x18u))m.put16(0x84a2b4u,std::uint16_t(t-1u));
    if(m.u16(0x84a2b4u))m.put16(0x84a2b4u,std::uint16_t(m.u16(0x84a2b4u)-1u));
    if(m.u16(0x8449e4u))m.put16(0x8449e4u,std::uint16_t(m.u16(0x8449e4u)-1u));
}
// 4BF970: the music change to [844A00] (20 frames of fade 42E020, then 401000 and the new
// volume 427AA0).
void arcade_music_4bf970(PcRaceContext& c){
    auto& m=c.m;
    const std::uint32_t want=m.u32(0x844a00u);
    if(want==0xffffffffu){
        if(m.i32(0x844a04u)>=0x14)return;
        call(c,0x42e020u,{0u,0u,1u});
        m.put32(0x8449fcu,m.u32(0x844a00u));m.put32(0x844a04u,m.u32(0x844a04u)+1u);
        return;
    }
    if(m.u32(0x8449fcu)==want)return;
    if(m.i32(0x844a04u)<0x14){
        call(c,0x42e020u,{0u,0u,1u});
        m.put32(0x844a04u,m.u32(0x844a04u)+1u);
        return;
    }
    call(c,0x401000u,{0u,want,1u});
    const std::uint32_t volume=call(c,0x427aa0u,{m.u32(0x844a00u),1u});   // x87 float result
    call(c,0x42e020u,{0u,volume,1u});
    m.put32(0x8449fcu,m.u32(0x844a00u));m.put32(0x844a04u,0);
}
// 4BFAC0: the stick's repeat (-1 / 0 / 1 every 20 frames past 0x12) while no menu input.
std::int32_t arcade_repeat_4bfac0(PcRaceContext& c){
    auto& m=c.m;
    const std::uint32_t o=call(c,0x4035f0u,{});
    if(m.i32(o+0x518u)>0)return 0;
    const std::int32_t v=std::int32_t(call(c,0x453720u,{4u}));
    m.put32(0x84a9acu,m.u32(0x84a9acu)+1u);
    const std::int32_t a=v<0?std::int32_t(0u-std::uint32_t(v)):v;
    if(a<0x40){m.put32(0x84a9acu,0x12u);return 0;}
    const std::int32_t q=m.i32(0x84a9acu)%0x14;
    if(q!=0x13)return 0;
    return v>0?1:-1;
}
// 4BFB90 (function 0xC init).
void arcade_fn12_init_4bfb90(PcRaceContext& c,std::uint32_t w){
    auto& m=c.m;
    m.put8(w+8u,m.u8(w+8u)&0xfcu);m.put8(w+0xau,m.u8(w+0xau)&0xfcu);
    call(c,0x40ec60u,{0u});
    m.put16(w+0x3cu,0);m.put16(w+0x3eu,0);m.put32(w+0x40u,0xffffffffu);
    if(!m.u32(0x84a210u))call(c,0x4b88d0u,{});
}
// 4BFBD0(esi work): the choice on the 5C91DC record [+19] once confirmed (input 8 / 4BFB20 /
// flags 1): sound, the 2E001A cursor, +1D / +1E, 48B130; flags 3 when input 8, else bit 1 and
// sound 0x40. Returns 1 when chosen.
std::uint32_t arcade_choose_4bfbd0(PcRaceContext& c,std::uint32_t w){
    auto& m=c.m;
    auto input8=[&]{const std::uint32_t o=call(c,0x4035f0u,{});return !(m.i32(o+0x518u)>0)&&call(c,0x4536f0u,{8u});};
    if(!input8()&&!result_input_4bfb20(c)&&(m.u8(w+8u)&3u)!=1u)return 0;
    const std::uint8_t bl=m.u8(w+0x19u);
    const std::uint32_t k=std::uint32_t(std::int32_t(std::int8_t(bl)));
    const std::uint32_t slot=0x5c91dcu+k*4u;
    const std::uint32_t r=row(m.u32(m.u32(slot)+0x10u));
    if(!input8()){
        const std::uint32_t snd=m.u32(m.u32(0x6898d8u)+m.u32(0x5c9408u+k*4u)*4u);
        arcade_sound_4bee60(c,snd);
    }
    for(std::uint32_t h:{0x8449c4u,0x8449ccu,0x84497cu,0x844980u,0x844984u})call(c,0x4285a0u,{m.u32(h)});
    for(std::uint32_t h:{0x8449c4u,0x8449ccu,0x84497cu,0x844980u})m.put32(h,0xffffffffu);
    const std::uint32_t v=m.u32(0x780258u);
    std::uint32_t tok;
    if(v==3u||v==4u){const std::uint32_t a=call(c,0x455ad0u,{})&0xffu;tok=m.u32(0x689abcu+(call(c,0x455b00u,{a})&0xffu)*4u);}
    else tok=0x2e001au;
    m.put32(0x8449c4u,call(c,0x428460u,{tok,3u,1u,m.u32(r+4u),m.u32(r+8u)}));
    const std::uint32_t e=k+m.u32(0x84a218u)*10u;
    m.put8(w+0x1du,bl);
    m.put8(w+0x1eu,m.u8(0x5c70a8u+e*4u));
    call(c,0x48b130u,{m.u8(m.u32(slot))});
    if(input8()){
        m.put8(w+8u,m.u8(w+8u)|3u);
        m.put16(w+0x10u,0);
        return 1;
    }
    m.put16(w+8u,std::uint16_t((m.u16(w+8u)&0xfffeu)|2u));
    call(c,0x4493c0u,{});
    call(c,0x424940u,{0x40u});
    m.put16(w+0x10u,0);
    return 1;
}
// 4BFDB0 (function 6 destroy): 4556E0(1), 448FD0(0x100, 0), 84A318 = 0 and the four slots.
void arcade_fn6_dest_4bfdb0(PcRaceContext& c){
    call(c,0x4556e0u,{1u});call(c,0x448fd0u,{0x100u,0u});
    c.m.put32(0x84a318u,0);
    call(c,0x446d90u,{4u,0x295u,8u,0x296u,0xffffffffu,0xffffffffu,0xffffffffu,0xffffffffu},Object84a320);
}
// 4BFA20 (the layer-8 callback 4C1A60 registers with 448FD0(0x100, 4BFA20)): 84A318 = 2 while
// mode 10's event 4 is displayed on function 5 and the side +19 is in state 2 (84A2E8), else 0.
// 449050 draws the car-select car 49F4D0 over the 2D layer when it is 2.
void arcade_layer8_4bfa20(PcRaceContext& c){
    auto& m=c.m;
    const std::uint32_t w=m.u32(0x799c28u);
    const bool on=m.u32(0x78026cu)==0xau&&(m.u8(0x79fb4cu)&3u)==2u&&m.u32(w+4u)==5u&&
        m.u32(0x84a2e8u+std::uint32_t(std::int32_t(std::int8_t(m.u8(w+0x19u))))*4u)==2u;
    m.put32(0x84a318u,on?2u:0u);
}
// ---- sequence steps and functions 1, 2, 4, 5, 8, 12 -------------------------------------
// 4C0B70: the 689F58 (or 689F88 with [84A214]) start banner 844968, layer 2.
void arcade_banner_4c0b70(PcRaceContext& c){
    auto& m=c.m;
    const std::uint32_t i=m.u32(0x84a214u)?0xcu:0u;
    m.put32(0x844968u,create_row(c,row(m.u32(0x689f58u+i*4u)),2u,1u));
    m.put32(0x84a2c4u,0);m.put16(0x84a2c0u,0);m.put32(0x8449dcu,0);
}
// 4C2430 (protected head: eax = [844968]): the banner swap 844968 -> 844964 once played, the
// [84A214] banner, and the 2E0049 player marker for more than one player.
void arcade_banner_4c2430(PcRaceContext& c){
    auto& m=c.m;
    const std::uint32_t h=m.u32(0x844968u);
    if(std::int32_t(h)>=0&&call(c,0x428880u,{h})==3u){
        const std::uint32_t k=m.u32(0x8449dcu);
        std::uint32_t i;
        if(k==0u||k==2u||k==4u)i=9u;
        else if(k==1u)i=0xau;
        else i=(k!=3u?1u:0u)+0xbu;
        const std::uint32_t r=row(m.u32(0x689f58u+i*4u));
        call(c,0x4285a0u,{m.u32(0x844964u)});
        call(c,0x4285a0u,{m.u32(0x844968u)});
        m.put32(0x844968u,0xffffffffu);
        m.put32(0x844964u,create_row(c,r,2u,0u));
    }
    const std::uint32_t t=m.u32(0x84a214u);
    if(t){
        const std::uint32_t r=row(m.u32(0x689f58u+t*4u));
        call(c,0x4285a0u,{m.u32(0x844964u)});
        call(c,0x4285a0u,{m.u32(0x844968u)});
        m.put32(0x844964u,0xffffffffu);
        m.put32(0x844968u,create_row(c,r,2u,1u));
    }
    if(m.u16(0x84a2c0u))return;
    const std::uint32_t n=call(c,0x456d60u,{})&0xffu;
    if(n<=1u)return;
    call(c,0x455b10u,{});call(c,0x4b89e0u,{});
    const std::uint32_t p=call(c,0x455c10u,{});
    call(c,0x429530u,{0x2e0049u,0u,0u,3u,m.u32(0x689f8cu+p*4u)});
}
// 4C2570 (sequence step 5): the 2E0019 / 2E0017 title and the start banner.
void arcade_step5_4c2570(PcRaceContext& c){
    auto& m=c.m;
    const std::uint32_t v=m.u32(0x780258u);
    m.put32(0x844958u,1u);m.put32(0x84a2d8u,0);m.put32(0x84a2dcu,0);
    std::uint32_t tok;
    if(v==3u||v==4u){const std::uint32_t a=call(c,0x455ad0u,{})&0xffu;tok=m.u32(0x689aacu+(call(c,0x455b00u,{a})&0xffu)*4u);}
    else tok=0x2e0019u;
    m.put32(0x8449c4u,call(c,0x428460u,{tok,7u,1u,0u,0x59u}));
    m.put32(0x8449ccu,call(c,0x428460u,{0x2e0017u,3u,1u,0u,0x59u}));
    m.put32(0x84a214u,0xcu);
    arcade_banner_4c2430(c);
    m.put32(0x8449bcu,0xffffffffu);m.put32(0x8449c0u,0xffffffffu);
}
// 4C2610 (sequence step 8): 2E0028 / 2E0027, the banner, the name entry words 84A220.. /
// 84492C.., 48B1D0(-1).
void arcade_step8_4c2610(PcRaceContext& c){
    auto& m=c.m;
    m.put32(0x8449c4u,call(c,0x428460u,{0x2e0028u,7u,1u,0u,0x3bu}));
    m.put32(0x8449ccu,call(c,0x428460u,{0x2e0027u,4u,1u,0u,0x3bu}));
    m.put32(0x84a214u,0xcu);
    arcade_banner_4c2430(c);
    for(std::uint32_t k=0;k<5u;++k){m.put32(0x84a220u+k*4u,0);m.put32(0x84492cu+k*4u,0);}
    m.put32(0x84a234u,0);
    m.put32(0x8449bcu,0xffffffffu);m.put32(0x8449c0u,0xffffffffu);
    m.put32(0x844958u,1u);
    m.put8(0x84a220u,1u);m.put8(0x84a221u,1u);
    call(c,0x48b1d0u,{0xffffffffu});
    m.put32(0x84a2acu,1u);
}
void arcade_step2_4bffc0(PcRaceContext& c);
void arcade_step4_4c0110(PcRaceContext& c);
void arcade_step11_4c0de0(PcRaceContext& c);
// 4C2AC0(sequence, index) (protected head: eax = byte index): run the sequence codes from
// index: 1 skips on (request bit 0), 2 / 4 / 5 / 6 / 8 / 11 start their screens, 9 the 2E0050
// cursor, 3 / 7 / 10 and the others stop.
void arcade_sequence_4c2ac0(PcRaceContext& c,std::uint32_t seq,std::uint32_t index){
    auto& m=c.m;
    std::uint32_t v=m.u32(seq+(index&0xffu)*4u)-1u;
    if(v>10u)return;
    for(;;){
        switch(v){
        case 0:{
            const std::uint32_t w=m.u32(0x799c28u);
            m.put8(w+0xau,m.u8(w+0xau)|1u);
            const std::uint8_t i=std::uint8_t(m.u8(w+0x15u)+1u);
            m.put8(w+0x15u,i);
            v=m.u32(m.u32(w)+std::uint32_t(std::int32_t(std::int8_t(i)))*4u)-1u;
            if(v<=10u)continue;
            return;}
        case 1:arcade_step2_4bffc0(c);return;
        case 3:arcade_step4_4c0110(c);return;
        case 4:arcade_step5_4c2570(c);return;
        case 5:arcade_step6_4bf3b0(c);return;
        case 7:arcade_step8_4c2610(c);return;
        case 8:
            m.put32(0x8449a8u,call(c,0x428460u,{0x2e0050u,9u,1u,0u,0xdu}));
            m.put32(0x8449bcu,0xffffffffu);m.put32(0x8449c0u,0xffffffffu);
            return;
        case 10:arcade_step11_4c0de0(c);return;
        default:return;
        }
    }
}
// 4BFE00 (function 1 init): the work bytes, sequence 5C8BE0, 417F70 -> +C, the 30 handles
// 844960..8449D8 = -1, 42CCB0(4), music 0x1E, 48B130(-1).
void arcade_fn1_init_4bfe00(PcRaceContext& c,std::uint32_t w){
    auto& m=c.m;
    call(c,0x456d60u,{});
    call(c,0x40ec60u,{0u});
    for(std::uint32_t o:{0x1cu,0x1du,0x22u,0x20u,0x1fu,0x23u,0x29u})m.put8(w+o,0xffu);
    m.put32(w+0x58u,0xffffffffu);
    m.put32(w+0x2au,0);m.put8(w+0x2eu,0);
    const std::uint8_t a=m.u8(w+0xau);
    m.put8(w+8u,m.u8(w+8u)&0xfcu);
    m.put32(0x844950u,0);
    m.put8(w+0x1eu,1u);m.put8(w+0x21u,1u);m.put8(w+0x24u,0xffu);
    m.put32(w,0x5c8be0u);
    m.put8(w+0xau,a&0xfcu);
    const std::uint32_t t=call(c,0x417f70u,{});
    m.put32(w+0xcu,t);
    m.put16(w+0x10u,0);m.put16(w+0x12u,0);m.put8(w+0x14u,0);m.put8(w+0x15u,0);
    m.put32(w+4u,m.u32(m.u32(w)));
    for(std::uint32_t a2=0x844960u;a2<=0x8449d8u;a2+=4u)m.put32(a2,0xffffffffu);
    call(c,0x42ccb0u,{4u});
    m.put32(0x8449fcu,0x1eu);m.put32(0x844a00u,0x1eu);m.put32(0x844a04u,0);
    call(c,0x401000u,{0u,0x1eu,1u});
    m.put32(0x8449f4u,3u);
    call(c,0x48b130u,{0xffffffffu});
}
// 4C3510 (function 1 control): request bit 0 and the sequence from its index.
void arcade_fn1_ctrl_4c3510(PcRaceContext& c,std::uint32_t w){
    auto& m=c.m;
    m.put8(w+0xau,m.u8(w+0xau)|1u);
    arcade_sequence_4c2ac0(c,m.u32(w),m.u8(w+0x15u));
}
// 4BFF80 (function 2 init).
void arcade_fn2_init_4bff80(PcRaceContext& c,std::uint32_t w){
    auto& m=c.m;
    m.put8(w+8u,m.u8(w+8u)&0xfcu);
    m.put32(0x844950u,0);
    m.put8(w+0xau,m.u8(w+0xau)&0xfcu);
    m.put16(w+0x10u,0x258u);m.put16(w+0x12u,0x3cu);
    for(std::uint32_t o:{0x3cu,0x3du,0x3eu,0x25u,0x26u})m.put8(w+o,0);
    m.put32(0x84a310u,0);
}
// 4BFFC0 (sequence step 2): with [+1F] the sequence moves on; else the course banner of
// [+1D] (689B28 or 689C68 for [7C24BC]) 8449A0 and sound 0.
void arcade_step2_4bffc0(PcRaceContext& c){
    auto& m=c.m;
    const std::uint32_t w=m.u32(0x799c28u);
    if(m.u8(w+0x1fu)){
        const std::uint8_t a=std::uint8_t(m.u8(w+0xau)|1u);
        const std::uint8_t i=std::uint8_t(m.u8(w+0x15u)+1u);
        m.put8(w+0x15u,i);m.put8(w+0xau,a);
        arcade_sequence_4c2ac0(c,m.u32(w),i);
        return;
    }
    const std::uint32_t e=std::uint32_t(std::int32_t(m.i8(w+0x1du)))*0x20u+(m.u32(0x7c24bcu)?0x689c68u:0x689b28u);
    m.put32(0x84a240u,e);
    m.put32(0x8449a0u,create_row(c,row(m.u32(e)),0xcu,1u));
    call(c,0x4493c0u,{});
    call(c,0x424940u,{0u});
    m.put32(0x844958u,1u);
}
// 4C0060 (function 4 init).
void arcade_fn4_init_4c0060(PcRaceContext& c,std::uint32_t w){
    auto& m=c.m;
    m.put8(w+8u,m.u8(w+8u)&0xfcu);
    m.put32(0x844950u,0);
    m.put8(w+0xau,m.u8(w+0xau)&0xfcu);
    m.put16(w+0x10u,0x708u);m.put16(w+0x12u,0x3cu);
}
// 4C0090 (function 4 control): skip on 4BFB20 (sound 0x40), flags 1 when the +10 timer ran
// out, ABCD at +2A; once flags are set, +12 counts down to the next sequence index.
void arcade_fn4_ctrl_4c0090(PcRaceContext& c,std::uint32_t w){
    auto& m=c.m;
    if(m.u8(w+8u)&3u){
        m.put16(w+0x12u,std::uint16_t(m.u16(w+0x12u)-1u));
        if(m.i16(w+0x12u)>0)return;
        m.put8(w+0xau,m.u8(w+0xau)|1u);m.put8(w+0x15u,std::uint8_t(m.u8(w+0x15u)+1u));
        return;
    }
    if(result_input_4bfb20(c)){
        m.put16(w+0x10u,0);
        call(c,0x4493c0u,{});
        call(c,0x424940u,{0x40u});
    }
    if(!(m.i16(w+0x10u)>0)){
        const std::uint16_t f=m.u16(w+8u);
        if(!(f&3u))m.put16(w+8u,std::uint16_t((f&0xfffdu)|1u));
    }
    m.put8(w+0x2au,0x41u);m.put8(w+0x2bu,0x42u);m.put8(w+0x2cu,0x43u);m.put8(w+0x2du,0x44u);
}
// 4C0110 (sequence step 4): 453320 -> 8449E0 (844944 = 1 for 1..2, 0 for 0), the 689E60
// words 84A24C.., 84A23C = 2E0022, handles 8449CC / 8449D8 / 8449C4.
void arcade_step4_4c0110(PcRaceContext& c){
    auto& m=c.m;
    const std::int32_t r=std::int32_t(call(c,0x453320u,{}));
    m.put32(0x8449e0u,std::uint32_t(r));
    if(r==0)m.put32(0x844944u,0);
    else if(r>0&&r<=2)m.put32(0x844944u,1u);
    m.put32(0x84a24cu,m.u32(0x689e60u));
    m.put32(0x84a23cu,0x2e0022u);
    m.put32(0x84a250u,m.u32(0x689e64u));m.put32(0x84a254u,m.u32(0x689e68u));m.put32(0x84a258u,m.u32(0x689e6cu));
    m.put32(0x8449ccu,call(c,0x428460u,{0x2e0022u,4u,1u,0u,0x3bu}));
    m.put32(0x8449d8u,call(c,0x428460u,{0x2e0024u,9u,1u,0u,0x3bu}));
    m.put32(0x8449c4u,call(c,0x428460u,{m.u32(0x689e50u+m.u32(0x844944u)*4u),7u,1u,0u,0x3bu}));
    m.put32(0x8449bcu,0xffffffffu);m.put32(0x8449c0u,0xffffffffu);
    m.put32(0x844958u,1u);
}
// 4C01E0 (function 5 init): the list of 3 or 5 rows (4BEE90) for 8449E0 = 0 / 1..2, sound 4,
// the four slots (4, 0x295, 8, 0x296, 0x20).
void arcade_fn5_init_4c01e0(PcRaceContext& c,std::uint32_t w){
    auto& m=c.m;
    m.put8(w+8u,m.u8(w+8u)&0xfcu);
    m.put8(w+0xau,m.u8(w+0xau)&0xfcu);
    m.put16(w+0x10u,0x384u);m.put16(w+0x12u,0x3cu);m.put16(w+0x3cu,0);m.put8(w+0x1fu,0xffu);
    const std::int32_t r=m.i32(0x8449e0u);
    m.put32(0x844950u,0);
    if(r>=0&&r<=2){
        const std::uint32_t h=m.u32(0x689e50u+m.u32(0x844944u)*4u);
        arcade_list_4bee90(c,h,std::int8_t(r==0?3:5),w,m.f32(0x689e74u),m.f32(0x689e70u),1080.0f,7u);
    }
    for(std::uint32_t o:{0x3eu,0x3fu,0x40u,0x41u,0x19u,0x1au,0x27u})m.put8(w+o,0);
    call(c,0x4493c0u,{});
    call(c,0x424940u,{4u});
    call(c,0x446d90u,{4u,0x295u,8u,0x296u,0x20u,0xffffffffu,0xffffffffu,0xffffffffu},Object84a320);
}
// 4C02B0 (function 8 init).
void arcade_fn8_init_4c02b0(PcRaceContext& c,std::uint32_t w){
    auto& m=c.m;
    m.put8(w+8u,m.u8(w+8u)&0xfcu);
    m.put32(0x844950u,0);
    m.put8(w+0xau,m.u8(w+0xau)&0xfcu);
    m.put16(w+0x10u,0x258u);m.put16(w+0x12u,0x3cu);
}
// 4C02E0 (function 8 control).
void arcade_fn8_ctrl_4c02e0(PcRaceContext& c,std::uint32_t w){
    auto& m=c.m;
    if(m.u8(w+8u)&3u){
        m.put16(w+0x12u,std::uint16_t(m.u16(w+0x12u)-1u));
        if(m.i16(w+0x12u)>0)return;
        m.put8(w+0xau,m.u8(w+0xau)|1u);m.put8(w+0x15u,std::uint8_t(m.u8(w+0x15u)+1u));
        return;
    }
    if(result_input_4bfb20(c))m.put16(w+0x10u,0);
    if(!(m.i16(w+0x10u)>0)){
        const std::uint16_t f=m.u16(w+8u);
        if(!(f&3u))m.put16(w+8u,std::uint16_t((f&0xfffdu)|1u));
    }
    m.put8(w+0x1cu,0);
}
// 4C0DE0 (sequence step 11): 455C20 players, 447090 / 447000(1, 0) on 84A320, the start
// banner; online (47F110) with more than one player the race starts; otherwise the sequence
// 5C8BE0 runs on.
void arcade_step11_4c0de0(PcRaceContext& c){
    auto& m=c.m;
    const std::uint32_t w=m.u32(0x799c28u);
    const std::uint32_t players=call(c,0x455c20u,{});
    m.put32(0x84a210u,0);m.put32(0x84a244u,0);
    call(c,0x4470f0u,{},Object84a320);
    call(c,0x447000u,{1u,0u},Object84a320);
    m.put32(0x84a214u,0xcu);
    arcade_banner_4c0b70(c);
    m.put32(w+0x50u,1u);m.put32(w+0x54u,0);m.put8(w+0x28u,1u);m.put8(w+0x27u,0xffu);
    if(call(c,0x47f110u,{})&&players>1u){
        m.put8(w+0x16u,0);m.put8(w+0x17u,0);
        m.put32(0x84a2b0u,0);m.put16(0x8449f8u,0xffffu);m.putf(0x84a314u,0.0f);
        m.put32(0x84a210u,call(c,0x47f110u,{}));
        return;
    }
    const std::uint8_t a=std::uint8_t(m.u8(w+0xau)|1u);
    m.put32(0x84a218u,0);
    const std::uint8_t i=std::uint8_t(m.u8(w+0x15u)+1u);
    m.put8(w+0xau,a);
    m.put32(w,0x5c8be0u);
    m.put8(w+0x15u,i);
    arcade_sequence_4c2ac0(c,0x5c8be0u,i);
}
// 4C2B70 (function 0xC control): 455AB0; state +3C 0 waits for 455B40 (one player: sequence
// 5C8BE0 and the 2E004E prompt with its sound), 1 waits 0x78 frames (or 47F110), then
// 4B89B0, the sequence moves on (+3C = 3).
void arcade_fn12_ctrl_4c2b70(PcRaceContext& c,std::uint32_t w){
    auto& m=c.m;
    call(c,0x455ab0u,{});
    const std::uint16_t st=m.u16(w+0x3cu);
    if(st==0u){
        if(!call(c,0x455b40u,{}))return;
        m.put16(w+0x3cu,1u);
        if((call(c,0x456d60u,{})&0xffu)>1u)return;
        m.put32(0x84a218u,0);
        m.put32(w,0x5c8be0u);
        const std::uint32_t online=call(c,0x47f110u,{});
        m.put32(0x84a244u,online);
        if(m.u32(0x84a210u)||online)return;
        m.put32(w+0x40u,call(c,0x428320u,{0x2e004eu,7u,1u}));
        arcade_sound_4bee60(c,m.u32(m.u32(0x6898d8u)+0xf0u));
        return;
    }
    if(st!=1u)return;
    const std::uint32_t online=call(c,0x47f110u,{});
    m.put32(0x84a244u,online);
    if(!m.u32(0x84a210u)){
        if(!online){
            const std::uint16_t f=m.u16(w+0x3eu);
            m.put16(w+0x3eu,std::uint16_t(f+1u));
            if(f!=0x78u)return;
        }
        call(c,0x4b89b0u,{});
    }
    call(c,0x4285a0u,{m.u32(w+0x40u)});
    const std::uint8_t a=std::uint8_t(m.u8(w+0xau)|1u);
    const std::uint8_t i=std::uint8_t(m.u8(w+0x15u)+1u);
    m.put8(w+0x15u,i);
    m.put16(w+0x3cu,3u);
    m.put8(w+0xau,a);m.put8(w+0x18u,0);
    arcade_sequence_4c2ac0(c,m.u32(w),i);
}
// ---- list, stick, panel helpers ------------------------------------------------------------
// 4C0A10(edi work): hints 4BF8D0, the 84A320 slots (446CF0 / 446A50, protected head at
// 4C0A1C = ecx 84A320), and the side panel 2E0011 / 2E0016 / 2E0012 / 2E0015 / 2E0013 rows
// (y from the 5C9334 words) for the chosen items.
void arcade_panel_4c0a10(PcRaceContext& c,std::uint32_t w){
    auto& m=c.m;
    std::uint32_t ebx=0;
    arcade_hint_4bf8d0(c,w);
    call(c,0x446cf0u,{},Object84a320);
    call(c,0x446a50u,{},Object84a320);
    std::uint32_t esi=0;
    auto y=[&](std::uint32_t i){return fb(float(std::int32_t(m.u16(0x5c9334u+i*2u))));};
    if(m.u8(w+0x22u)!=0xffu){
        call(c,0x429530u,{0x2e0011u,0x44110000u,0x42b40000u,5u,std::uint32_t(std::int32_t(m.i8(w+0x1du)))});
        esi=1;
    }
    if(m.u8(w+0x22u)!=0xffu){
        call(c,0x429530u,{0x2e0016u,0x44110000u,y(esi),5u,std::uint32_t(std::int32_t(m.i8(w+0x22u)))});
        ++esi;
    }
    const std::uint8_t a=m.u8(w+0x1fu);
    if(a!=0xffu&&a!=3u&&a!=4u){
        switch(std::int8_t(a)){case 0:ebx=2u;break;case 1:ebx=0;break;case 2:ebx=1u;break;default:break;}
        if(m.u8(w+0x27u)==1u)ebx+=3u;
        call(c,0x429530u,{0x2e0012u,0x44110000u,y(esi),5u,ebx});
        ++esi;
    }
    if(m.u8(w+0x23u)!=0xffu){
        call(c,0x429530u,{0x2e0015u,0x44110000u,y(esi),5u,std::uint32_t(std::int32_t(m.i8(w+0x23u)))});
        ++esi;
    }
    if(m.u8(w+0x20u)!=0xffu)
        call(c,0x429530u,{0x2e0013u,0x44110000u,y(esi),5u,std::uint32_t(std::int32_t(m.i8(w+0x20u)))});
}
// 4C0BD0(arg): the stick's list step (1 / -1 / 0) from 453720(0) / 453750(0) against the
// 689FB4 / 689FB8 thresholds (widened after 50 frames), with the smoothed position 84A314.
std::int32_t arcade_stick_4c0bd0(PcRaceContext& c,std::uint32_t arg){
    auto& m=c.m;
    std::int32_t ebp=0;
    std::uint32_t o=call(c,0x4035f0u,{});
    const std::int32_t esi=m.i32(o+0x518u)>0?0:std::int32_t(call(c,0x453720u,{0u}));
    o=call(c,0x4035f0u,{});
    const std::int32_t other=m.i32(o+0x518u)>0?0:std::int32_t(call(c,0x453750u,{0u}));
    std::int32_t ebx=m.i32(0x689fb8u);
    const std::int32_t ecx0=std::int32_t(std::uint32_t(esi)-std::uint32_t(other));
    std::int32_t eax=m.i32(0x689fb4u);
    std::int32_t edi;
    if(esi>ebx&&esi<eax){
        const std::int32_t n=m.i32(0x84a9f8u)+1;
        m.put32(0x84a9f8u,std::uint32_t(n));
        if(n>0x32){ebx=-40;eax=40;m.put32(0x689fb8u,std::uint32_t(-40));m.put32(0x689fb4u,40u);m.put32(0x84a9f8u,0);}
        edi=0xffff;
        m.put32(0x84a9f4u,std::uint32_t(edi));
    }else{
        edi=m.i32(0x84a9f4u);
        if(edi<0xffff){++edi;m.put32(0x84a9f4u,std::uint32_t(edi));}
    }
    const std::int32_t edx=m.i32(0x689fa4u);
    float x0;
    if(esi>edx){
        if(ecx0>0)x0=float(esi-edx);
        else{const float x1=m.f32(0x84a314u);x0=float(esi-edx);x0=x0-x1;x0=x0*m.f32(0x689fa0u);x0=x0+x1;}
        const float x1=m.f32(0x5b4354u);
        m.putf(0x84a314u,x0);
        if(x1>x0){x0=x1;m.putf(0x84a314u,x0);}
    }else if(esi>=-edx){
        x0=0.0f;m.putf(0x84a314u,0.0f);
    }else{
        x0=float(edx+esi);
        if(ecx0>=0){const float x1=m.f32(0x84a314u);x0=x0-x1;x0=x0*m.f32(0x689fa0u);x0=x0+x1;}
        const float x1=m.f32(0x5b4350u);
        m.putf(0x84a314u,x0);
        if(x0>x1){x0=x1;m.putf(0x84a314u,x0);}
    }
    bool step=false;
    if(arg){
        if(!(esi<0x50)&&edi>0x28)step=true;
        else if(!(esi>-0x50)&&edi>0x28)step=true;
        else if(!(esi<eax)&&edi>0x50)step=true;
        else if(esi>ebx)step=false;
        else step=edi>0x50;
    }else{
        bool go=false;
        if(esi>0){if(float(esi)>x0)go=true;}
        else if(esi<0){if(x0>float(esi))go=true;}
        if(go){
            if(!(esi<0x50)&&edi>0)step=true;
            else if(!(esi>-0x50)&&edi>0)step=true;
            else if(!(esi<eax)&&edi>0)step=true;
            else if(esi>ebx)step=false;
            else step=edi>0;
        }
    }
    if(step){
        m.put32(0x84a9f8u,0);m.put32(0x84a9f4u,0);
        if(esi>0){
            m.put32(0x84a2b0u,1u);
            m.put32(0x689fb4u,0x1eu);m.put32(0x689fb8u,std::uint32_t(-0x5a));
            return 1;
        }
        m.put32(0x689fb8u,std::uint32_t(-0x1e));m.put32(0x689fb4u,0x5au);
        ebp=-1;
    }
    m.put32(0x84a2b0u,std::uint32_t(ebp));
    return ebp;
}
// 4C0EB0(work, offset, a2): the list scroll (84A260..84A2A4): stick / repeat steps, the
// speed 84A268 toward the row, its wrap for the looping lists, the row change of +19 / +1A /
// +18, the cursor 8449C4 (428320 of [84A298] when it changed) and its x (4288C0).
void arcade_scroll_4c0eb0(PcRaceContext& c,std::uint32_t w,std::uint32_t a1,std::uint32_t a2){
    auto& m=c.m;
    std::uint32_t o=call(c,0x4035f0u,{});
    std::int32_t r0=m.i32(o+0x518u)>0?0:std::int32_t(call(c,0x453720u,{0u}));
    const std::int32_t esi=r0<0?std::int32_t(0u-std::uint32_t(r0)):r0;
    const std::uint8_t ral=m.u8(0x84a292u);
    m.put8(w+0x18u,0);
    std::int32_t edi;
    if(ral){m.put8(0x84a292u,std::uint8_t(ral-1u));edi=0;}
    else{
        edi=arcade_stick_4c0bd0(c,0);
        if(edi==0){
            bool skip=false;
            if(a2){
                o=call(c,0x4035f0u,{});
                if(!(m.i32(o+0x518u)>0)&&std::int32_t(call(c,0x453720u,{2u}))>0x80)skip=true;
                if(!skip){const std::uint32_t v=m.u32(0x780258u);if(v==3u||v==4u)skip=true;}
            }
            if(!skip&&m.u8(0x84a2a4u))m.put8(0x84a2a4u,0);
        }
    }
    const std::int32_t ebp=std::int32_t(m.i8(w+0x19u));
    float x0,x1,x2,x3,x4,x5,x6,x7=0.0f;
    float Lm4,Lm8,Lmc,Lm10,Lm14;
    std::uint8_t B15;
    std::uint8_t cl,al,bl,dl;
    std::int32_t eax;
    x5=m.f32(0x84a274u);x2=m.f32(0x84a278u);x1=m.f32(0x689aa4u);x3=float(ebp);
    x0=x3+m.f32(0x628064u);
    x0=x0*x2;
    x1=x1+x5;
    x4=x0-x1;
    x1=x1+x0;
    Lm8=x1;
    x1=x0;
    x0=x0+x5;
    x6=x0;
    x0=m.f32(0x84a268u);
    Lm4=x4;
    x4=m.f32(0x689aa0u);
    x1=x1-x5;
    Lmc=x1;
    Lm14=x6;
    if(edi>0){
        x7=0.0f;
        if(m.u8(0x84a28cu)){
            if(x7>=x0)m.putf(0x84a26cu,m.f32(0x689fc4u));
            al=m.u8(0x84a290u);
            if(al==2u){x2=m.f32(0x84a278u);x1=x4*m.f32(0x6280b0u);x2=x2-x1;x2=x2+x0;x0=x2;m.putf(0x84a268u,x0);}
            else if(al==3u||al==1u){
                x1=m.f32(0x5a29e0u);
                if(x1>x0&&x0>m.f32(0x5c9404u)){
                    x1=m.f32(0x84a270u)+m.f32(0x62806cu);
                    m.putf(0x84a270u,x1);
                    if(x1>=x6)m.putf(0x84a270u,Lmc);
                }
            }else{x0=arcade_row_4bef90(c,ebp+1);m.putf(0x84a268u,x0);}
            m.put8(0x84a28eu,0);
            if(x0>x7)m.put8(0x84a28cu,0);
        }
        x2=m.f32(0x689aa8u);x1=float(esi);
        if(x2>x1)m.putf(0x84a288u,float(std::int32_t(m.i8(0x689fc0u))));
        else m.putf(0x84a288u,float(std::int32_t(m.i8(0x84a9fcu))));
        x2=m.f32(0x84a264u);
    }else if(edi<0){
        x7=0.0f;
        if(m.u8(0x84a28cu)){
            if(x0>=x7)m.putf(0x84a26cu,m.f32(0x689fc4u));
            al=m.u8(0x84a28fu);
            if(al==2u){x1=x4*m.f32(0x6280b0u);x0=x2;x0=x0-x1;x0=x0*m.f32(0x6280c4u);m.putf(0x84a268u,x0);}
            else if(al==3u||al==1u){arcade_4befe0(c,Lm14,Lmc);x0=m.f32(0x84a268u);x5=m.f32(0x84a274u);}
            else{x0=arcade_row_4bef90(c,ebp-1);m.putf(0x84a268u,x0);}
            m.put8(0x84a28du,0);
            if(x7>x0)m.put8(0x84a28cu,0);
        }
        x2=m.f32(0x689aa8u);x1=float(esi);
        if(x2>x1)m.putf(0x84a288u,float(std::int32_t(m.i8(0x689fc0u))));
        else m.putf(0x84a288u,float(std::int32_t(m.i8(0x84a9fcu))));
        x2=m.f32(0x84a264u);
    }else{
        x2=m.f32(0x5a29e0u);
        x7=m.f32(0x62806cu);
        m.putf(0x84a26cu,x7);
        if(x2>x0&&x0>m.f32(0x5c9404u)){
            x2=m.f32(0x84a270u)+x7;
            m.putf(0x84a270u,x2);
            if(x2>=x6)m.putf(0x84a270u,x1);
        }
        x7=0.0f;
        if(m.u8(0x84a28du)){x2=x6;x0=x7;m.putf(0x84a264u,x2);m.putf(0x84a270u,x6);m.putf(0x84a268u,x0);m.put8(0x84a28du,0);}
        else x2=m.f32(0x84a264u);
        if(m.u8(0x84a28eu)){x2=x6;x0=x7;m.putf(0x84a264u,x2);m.putf(0x84a270u,x6);m.putf(0x84a268u,x0);m.put8(0x84a28eu,0);}
    }
    // 4C1266
    al=m.u8(0x84a260u);
    if(al==0u||al==4u)x1=m.f32(0x84a26cu);
    else if(al==1u||al==3u)x1=m.f32(0x689fbcu);
    else x1=m.f32(0x62806cu);
    x3=x3*m.f32(0x84a278u);
    Lm10=x1;
    x1=x2;
    x1=x1-x3;
    m.put8(0x84a261u,al);
    auto zone=[&](std::uint8_t& out,float& x6r,float& x7r){
        // 4C12AC / 4C1350: the zone of x1 (0..4) along the list.
        out=2u;
        if(x4>x1){out=0;return;}
        x3=x4+m.f32(0x689aa4u);
        if(x3>x1){out=1u;x7r=0.0f;return;}
        x6r=m.f32(0x689aa4u);x3=x5*m.f32(0x6280b0u);x7r=x3+x4;x7r=x7r+x6r;
        if(x7r>=x1){out=2u;x6r=Lm14;x7r=0.0f;return;}
        x6r=x6r*m.f32(0x6280b0u);x6r=x6r+x3;x6r=x6r+x4;
        if(x6r>=x1){out=3u;x6r=Lm14;x7r=0.0f;return;}
        x6r=x4+m.f32(0x689aa4u);x6r=x6r*m.f32(0x6280b0u);x6r=x6r+x3;
        if(x6r>=x1)out=4u;
        x6r=Lm14;x7r=0.0f;
    };
    zone(cl,x6,x7);
    m.put8(0x84a260u,cl);
    if(x0>x7)x1=x1+Lm10;else x1=x1-Lm10;
    zone(B15,x6,x7);
    dl=m.u8(0x84a291u);
    bl=m.u8(w+0x19u);
    eax=std::int32_t(std::int8_t(dl))-1;
    m.put8(0x84a290u,ebp==eax?1u:0u);
    bl=bl==0u?1u:0u;
    m.put8(0x84a28fu,bl);
    if(x0>x7){
        bl=m.u8(0x84a28du);
        if(bl==0u){
            x1=Lm10;x0=x0-x1;m.putf(0x84a268u,x0);
            if(x7>=x0){x0=x0+x1;x0=x0+x2;x2=x0;x0=x7;m.putf(0x84a268u,x0);m.put8(0x84a28cu,1u);}
            else x2=x2+x1;
            m.putf(0x84a264u,x2);
        }
        x5=Lmc;
        if(x2>=x5&&x6>x2){
            x1=x6;x1=x1-x5;x0=x0-x1;
            const bool below=!(x7>=x0);
            x2=x2-x5;x2=x2+x6;m.putf(0x84a264u,x2);m.putf(0x84a268u,x0);
            if(!below){x0=x7;m.putf(0x84a268u,x0);m.put8(0x84a28cu,1u);}
        }
        bool store=false;
        if(m.u8(0x84a290u)==3u){
            x1=m.f32(0x84a27cu);x1=x1-x4;
            if(x2>x1){x2=x2-x1;x2=x2+x4;store=true;}
        }else if(ebp!=eax){
            bool go=false;
            if(x0>=m.f32(0x84a278u)&&cl==1u&&m.u8(0x84a261u)==0u)go=true;
            if(!go&&std::int8_t(m.u8(0x84a28du))>0)go=true;
            if(go){
                x3=Lm4;
                if(x2>=x3){
                    x1=Lm8;
                    if(x1>x2){
                        al=std::uint8_t(m.u8(0x84a28du)+1u);m.put8(0x84a28du,al);
                        x5=float(std::int32_t(std::int8_t(al)));
                        if(x5>m.f32(0x84a288u)){
                            x2=x2-x3;x2=x2+x1;m.putf(0x84a264u,x2);
                            x0=arcade_row_4bef90(c,ebp+1);m.putf(0x84a268u,x0);m.put8(0x84a28du,0);
                        }
                        x5=Lmc;
                    }
                }
            }
            x4=x4+m.f32(0x689aa4u);
            x1=m.f32(0x84a27cu);x1=x1-x4;
            if(x2>=x1){x2=x1;store=true;}
        }
        if(store)m.putf(0x84a264u,x2);
        al=m.u8(0x84a28du);bl=0;
        if(std::int8_t(al)>0){x1=m.f32(0x84a270u)+Lm10;m.putf(0x84a270u,x1);if(x1>=x6)m.putf(0x84a270u,x5);}
        else m.putf(0x84a270u,x2);
        cl=1u;
        if(B15==1u)m.put8(0x84a28cu,cl);
    }else if(x7>x0){
        al=m.u8(0x84a28eu);
        if(al==0u){
            x1=Lm10;x0=x0+x1;m.putf(0x84a268u,x0);
            if(x0>=x7){x1=x1-x0;x0=x7;m.putf(0x84a268u,x0);m.put8(0x84a28cu,1u);}
            x2=x2-x1;m.putf(0x84a264u,x2);
        }
        x1=Lmc;
        if(x2>=x1&&x6>x2){
            x3=x6;x3=x3-x2;x2=x1;x2=x2-x3;x3=x6;x3=x3-x1;x3=x3+x0;x0=x3;
            const bool below=!(x0>=x7);
            m.putf(0x84a264u,x2);m.putf(0x84a268u,x0);
            if(!below){x0=x7;m.putf(0x84a268u,x0);m.put8(0x84a28cu,1u);}
        }
        bool store=false;
        if(bl==3u){
            if(x4>x2){x1=x4;x1=x1-x2;x2=m.f32(0x84a27cu);x2=x2-x1;x2=x2-x4;store=true;}
        }else{
            bl=m.u8(w+0x19u);
            if(bl){
                x1=x7;x1=x1-m.f32(0x84a278u);
                bool go=false;
                if(x1>=x0&&cl==3u&&m.u8(0x84a261u)==4u)go=true;
                if(!go&&std::int8_t(al)>0)go=true;
                if(go&&x2>=Lm4){
                    x1=Lm8;
                    if(x1>x2){
                        al=std::uint8_t(al+1u);
                        x1=float(std::int32_t(std::int8_t(al)));
                        m.put8(0x84a28eu,al);
                        if(x1>m.f32(0x84a288u)){
                            x0=Lm8;x0=x0-x2;x2=Lm4;x2=x2-x0;m.putf(0x84a264u,x2);
                            x0=arcade_row_4bef90(c,ebp-1);al=0;m.putf(0x84a268u,x0);m.put8(0x84a28eu,0);
                        }
                    }
                }
                x5=x5*m.f32(0x6280b0u);x5=x5+x4;x5=x5+m.f32(0x689aa4u);x1=x5;
                if(x1>x2){x2=x1;store=true;}
            }
        }
        if(store)m.putf(0x84a264u,x2);
        bl=0;
        if(std::int8_t(al)>0){
            x1=m.f32(0x84a270u)-Lm10;x3=Lmc;m.putf(0x84a270u,x1);
            if(x3>x1){x6=x6-m.f32(0x62806cu);m.putf(0x84a270u,x6);}
        }else m.putf(0x84a270u,x2);
        cl=1u;
        if(B15==3u)m.put8(0x84a28cu,cl);
    }else{cl=1u;bl=0;}
    // 4C17A3
    x2=x2/m.f32(0x84a278u);
    if(cvtt(x2)!=ebp){
        if(x0>x7){
            al=m.u8(w+0x19u);m.put8(w+0x1au,al);al=std::uint8_t(al+1u);m.put8(w+0x19u,al);
            if(!(std::int8_t(al)<std::int8_t(dl))){
                if(m.u8(0x84a290u)==3u){m.put8(w+0x19u,bl);m.put8(w+0x18u,cl);}
                else{dl=std::uint8_t(dl-1u);m.put8(w+0x19u,dl);m.put8(w+0x18u,cl);}
            }else{m.put8(0x84a28fu,bl);m.put8(w+0x18u,cl);}
        }else if(x7>x0){
            al=m.u8(w+0x19u);m.put8(w+0x1au,al);al=std::uint8_t(al-1u);m.put8(w+0x19u,al);
            if(std::int8_t(al)<0){
                if(m.u8(0x84a28fu)==3u){dl=std::uint8_t(dl-1u);m.put8(w+0x19u,dl);}
                else m.put8(w+0x19u,bl);
            }else m.put8(0x84a290u,bl);
            m.put8(w+0x18u,2u);
        }
    }
    std::uint32_t h;
    if(m.u32(0x84a298u)!=m.u32(0x84a29cu)){
        call(c,0x4285a0u,{m.u32(0x8449c4u)});
        h=call(c,0x428320u,{m.u32(0x84a298u),m.u32(0x84a2a0u),2u});
        m.put32(0x8449c4u,h);m.put32(0x84a29cu,m.u32(0x84a298u));
    }else h=m.u32(0x8449c4u);
    if(!a1)call(c,0x4288c0u,{h,std::uint32_t(cvtt(m.f32(0x84a280u)+m.f32(0x84a270u)))});
    else{float v=m.f32(0x84a294u)+m.f32(0x84a280u);v=v+m.f32(0x84a270u);call(c,0x4288c0u,{h,std::uint32_t(cvtt(v))});}
}
// 4C0340(esi work): the transmission switch of the class screen (input 0x10 held 10 frames,
// then 4BFAC0 / 0x400 / 0x800 cycle 84A21C 1 -> 3 -> 2 and +21).
void arcade_gear_4c0340(PcRaceContext& c,std::uint32_t w){
    auto& m=c.m;
    if(!m.u32(0x8449e8u))return;
    m.put32(0x84a238u,0);m.put8(0x84a2a4u,0);
    std::uint32_t o=call(c,0x4035f0u,{});
    if(!(m.i32(o+0x518u)>0)&&call(c,0x4536c0u,{0x10u})){
        const std::int8_t n=std::int8_t(m.u8(0x84a248u));
        if(n<0xa){m.put8(0x84a248u,std::uint8_t(n+1));return;}
        m.put32(0x84a238u,1u);m.put8(0x84a2a4u,1u);
        const std::int32_t edi=arcade_repeat_4bfac0(c);
        o=call(c,0x4035f0u,{});
        bool up=!(m.i32(o+0x518u)>0)&&call(c,0x4536f0u,{0x400u});
        if(up||edi>0){
            switch(m.u32(0x84a21cu)){
            case 1:m.put32(0x84a21cu,3u);m.put8(w+0x21u,2u);break;
            case 2:m.put32(0x84a21cu,1u);m.put8(w+0x21u,0);break;
            case 3:m.put32(0x84a21cu,2u);m.put8(w+0x21u,1u);break;
            default:break;
            }
        }
        o=call(c,0x4035f0u,{});
        const bool down=!(m.i32(o+0x518u)>0)&&call(c,0x4536f0u,{0x800u});
        if(!down&&edi>=0)return;
        switch(m.u32(0x84a21cu)){
        case 1:m.put8(w+0x21u,1u);m.put32(0x84a21cu,2u);return;
        case 2:m.put32(0x84a21cu,3u);m.put8(w+0x21u,2u);return;
        case 3:m.put32(0x84a21cu,1u);m.put8(w+0x21u,0);return;
        default:return;
        }
    }
    const std::int8_t n=std::int8_t(m.u8(0x84a248u));
    if(n>0&&n<0xa){
        switch(m.u32(0x84a21cu)){
        case 1:m.put32(0x84a21cu,2u);m.put8(w+0x21u,1u);break;
        case 2:m.put32(0x84a21cu,3u);m.put8(w+0x21u,2u);break;
        case 3:m.put32(0x84a21cu,1u);m.put8(w+0x21u,0);break;
        default:break;
        }
    }
    m.put8(0x84a248u,0);
}
// 4C04D0 (function 9 init): first entry the 8-row list (4BEE90 2E0028), the work words and
// the 5C8D00 music; later the saved name 84492C.. back; slots 4 / 0x295 / 8 / 0x296 / 0x10 /
// 0x32E / 0x20 / 0x423; transmission switch reset.
void arcade_fn9_init_4c04d0(PcRaceContext& c,std::uint32_t w){
    auto& m=c.m;
    m.put32(0x8449f4u,3u);
    if(m.u32(w+0x54u)==0u){
        m.put32(0x844950u,0);
        arcade_list_4bee90(c,0x2e0028u,8,w,m.f32(0x689e7cu),m.f32(0x689e78u),740.0f,7u);
        m.put8(w+8u,m.u8(w+8u)&0xfcu);m.put8(w+0xau,m.u8(w+0xau)&0xfcu);
        m.put16(w+0x10u,0x4b0u);m.put16(w+0x12u,0x3cu);
        m.put8(w+0x19u,0);m.put8(w+0x1au,0);m.put8(w+0x48u,0);m.put32(w+0x3cu,0);m.put32(w+0x44u,0);
        m.put8(w+0x49u,0);m.put8(w+0x4au,0);m.put8(w+0x4bu,0);
        m.put32(0x844a00u,m.u32(m.u32(0x5c8d00u)+4u));
        m.put32(w+0x54u,1u);
    }else{
        m.put8(w+8u,m.u8(w+8u)&0xfcu);m.put8(w+0xau,m.u8(w+0xau)&0xfcu);
        m.put32(0x8449bcu,0xffffffffu);m.put32(0x8449c0u,0xffffffffu);
        m.put32(w+0x3cu,m.u32(0x84492cu));m.put32(w+0x40u,m.u32(0x844930u));
        m.put32(w+0x44u,m.u32(0x844934u));m.put32(w+0x48u,m.u32(0x844938u));
        m.put8(w+0x19u,m.u8(0x84493cu));m.put8(w+0x1au,m.u8(0x84493du));
    }
    call(c,0x446d90u,{4u,0x295u,8u,0x296u,0x10u,0x32eu,0x20u,0x423u},Object84a320);
    m.put32(0x8449e8u,0);m.put32(0x84a238u,0);m.put8(0x84a248u,0);m.put32(0x84a21cu,2u);
}
// 4C05F0 (function 0xB init): unless flags 3, the race settings: online (49B460) from the
// session (457A60, 4591F0 / 4591C0 / 455AE0, 65A7A4.. options -> 84A9B0.., 43F940(4), 4957D0,
// 495A10(84A9B0)); offline from the work bytes (48B130 / 48B150 / 48B170 / 48B190 / 48B1B0,
// the 5C8D00 music (random for 7 / 0xF), variant [+1F] (1 when unset), 47EF20, 43F940 /
// 4B6EE0, the 452ED0 / 452CD0 / 452D20 counters, the 6898C9.. and 84A2CD.. copies).
void arcade_fn11_init_4c05f0(PcRaceContext& c,std::uint32_t w){
    auto& m=c.m;
    const std::uint16_t f=m.u16(w+8u);
    if((f&3u)==3u)return;
    m.put16(w+8u,std::uint16_t(f&0xfffcu));
    m.put8(w+0xau,m.u8(w+0xau)&0xfcu);
    if(call(c,0x49b460u,{})){
        call(c,0x457a60u,{});
        std::uint32_t a=call(c,0x455ad0u,{})&0xffu;call(c,0x48b130u,{call(c,0x4591f0u,{a})});
        a=call(c,0x455ad0u,{})&0xffu;call(c,0x48b150u,{call(c,0x4591c0u,{a})});
        a=call(c,0x455ad0u,{})&0xffu;call(c,0x48b170u,{call(c,0x455ae0u,{a})});
        call(c,0x48b200u,{std::uint32_t(std::uint8_t(m.u8(0x65a7a9u)+1u))});
        call(c,0x455660u,{std::uint32_t(std::int32_t(m.i8(0x65a7adu)))});
        const std::int32_t ah=std::int32_t(m.i8(0x65a7a9u));
        switch(std::int8_t(m.u8(0x65a7a8u))){
        case 0:m.put32(0x84a9c4u,0);m.put32(0x84a9b0u,std::uint32_t(ah));break;
        case 1:m.put32(0x84a9c4u,5u);m.put32(0x84a9b0u,std::uint32_t(ah));break;
        case 2:m.put32(0x84a9c4u,3u);m.put32(0x84a9b0u,std::uint32_t(ah));break;
        case 3:m.put32(0x84a9c4u,4u);m.put32(0x84a9b0u,std::uint32_t(ah));break;
        default:break;
        }
        switch(std::uint32_t(std::int32_t(m.i8(0x65a7a6u)))){
        case 0:m.put32(0x84a9d0u,0);break;case 1:m.put32(0x84a9d0u,1u);break;case 2:m.put32(0x84a9d0u,2u);break;
        case 3:m.put32(0x84a9d0u,3u);break;case 4:case 5:m.put32(0x84a9d0u,6u);break;default:break;
        }
        const std::int32_t t=std::int32_t(m.i8(0x65a7afu));
        if(t==0)m.put32(0x84a9bcu,1u);else if(t==1)m.put32(0x84a9bcu,0);
        m.put32(0x84a9b8u,0);m.put32(0x84a9b4u,0);m.putf(0x84a9d8u,0.0f);
        m.put32(0x84a9c0u,m.u8(0x65a7adu)==0u?1u:0u);
        switch(std::uint32_t(std::int32_t(m.i8(0x65a7aau)))){
        case 1:m.put32(0x84a9dcu,5u);break;case 2:m.put32(0x84a9dcu,0xau);break;default:m.put32(0x84a9dcu,3u);break;
        }
        m.put32(0x84a9c8u,0);m.put32(0x84a9ccu,0);m.put32(0x84a9d4u,0);
        m.put8(0x84a9e0u,m.u8(0x65a7a4u)==0u?1u:0u);
        call(c,0x43f940u,{4u});
        call(c,0x4957d0u,{});
        call(c,0x495a10u,{0x84a9b0u});
        return;
    }
    const std::uint32_t k=std::uint32_t(std::int32_t(m.i8(w+0x1du)));
    call(c,0x48b130u,{m.u8(m.u32(0x5c91dcu+k*4u))});
    if((call(c,0x456d60u,{})&0xffu)<=1u&&m.u32(0x844948u))
        m.put8(w+0x1eu,m.u8(m.u32(0x5c9204u+k*4u)+m.u32(0x844954u)*12u+8u));
    const std::uint32_t kind=m.u32(m.u32(0x5c91dcu+std::uint32_t(std::int32_t(m.i8(w+0x1du)))*4u));
    if((kind==6u||kind==5u||kind==3u||kind==9u)&&m.u8(w+0x1eu)==4u)m.put8(w+0x1eu,5u);
    call(c,0x48b150u,{m.u8(w+0x1eu)});
    call(c,0x48b170u,{m.u8(w+0x22u)});
    call(c,0x48b190u,{m.u8(w+0x23u)});
    call(c,0x48b1b0u,{m.u8(w+0x21u)});
    const std::uint8_t mu=m.u8(w+0x20u);
    std::uint32_t p;
    if(mu==7u){const std::int32_t r=std::int32_t(call(c,0x580f40u,{}))%7;p=m.u32(0x5c8d00u+std::uint32_t(r)*4u);}
    else if(mu==0xfu){const std::int32_t r=std::int32_t(call(c,0x580f40u,{}))%7;p=m.u32(0x5c8d00u+std::uint32_t(r+8)*4u);}
    else p=m.u32(0x5c8d00u+std::uint32_t(std::int32_t(std::int8_t(mu)))*4u);
    call(c,0x48b1d0u,{m.u8(p)});
    if(m.u8(w+0x1fu)==0xffu)m.put8(w+0x1fu,1u);
    call(c,0x47ef20u,{m.u8(w+0x1fu)==0u?1u:0u});
    if((call(c,0x456d60u,{})&0xffu)<=1u){
        call(c,0x43f940u,{std::uint32_t(std::int32_t(m.i8(w+0x1fu)))});
        call(c,0x4b6ee0u,{m.u8(w+0x1fu)});
    }
    const std::uint32_t v=std::uint16_t(std::int16_t(m.i8(w+0x1fu))),e=std::uint16_t(std::int16_t(m.i8(w+0x27u)));
    call(c,0x452ed0u,{v,e});   // 16-bit pushes (upper halves are left-overs)
    call(c,0x452cd0u,{v,e});
    const std::int32_t d=std::int32_t(call(c,0x417f70u,{})-m.u32(w+0xcu));
    call(c,0x452d20u,{v,std::uint32_t(d<0?-d:d)});
    m.put8(0x6898cau,m.u8(w+0x22u));m.put8(0x6898c9u,m.u8(w+0x1du));
    m.put8(0x6898cbu,m.u8(w+0x20u));m.put8(0x6898ccu,m.u8(w+0x24u));
    const std::uint8_t var=m.u8(w+0x1fu);
    m.put8(0x6898cdu,var);
    m.put8(0x6898cfu,var==0u?m.u8(w+0x23u):0xffu);
    m.put8(0x84a2cdu,m.u8(w+0x25u));m.put8(0x84a2ceu,m.u8(w+0x26u));m.put8(0x84a2cfu,m.u8(w+0x27u));
    m.put8(0x6898ceu,m.u8(w+0x1eu));
    m.put32(0x84a2d0u,m.u32(0x844954u));m.put32(0x84a2d4u,m.u32(0x844948u));
}
// 4C1E80 (function 6 display): one player with [844928]: the 2E0000 bar at [844954] and the
// 5C922C label of +19; then the panel.
void arcade_fn6_disp_4c1e80(PcRaceContext& c,std::uint32_t w){
    auto& m=c.m;
    const std::int8_t bl=m.i8(w+0x19u);
    if((call(c,0x456d60u,{})&0xffu)<=1u&&m.u32(0x844928u)){
        const float f=float(m.i32(0x844954u));
        float x1=f*m.f32(0x6281e8u);
        float x0=f*m.f32(0x5c9434u);x0=x0+m.f32(0x5c9430u);
        const float y=m.f32(0x5c9438u)-x1;
        call(c,0x429530u,{0x2e0000u,fb(x0),fb(y),0xau,0u});
        call(c,0x429530u,{m.u32(0x5c922cu+std::uint32_t(std::int32_t(bl))*4u),0u,0u,9u,0u});
    }
    arcade_panel_4c0a10(c,w);
}
// 4C22C0 (function 5 display): 408880, the panel.
void arcade_fn5_disp_4c22c0(PcRaceContext& c,std::uint32_t w){call(c,0x408880u,{});arcade_panel_4c0a10(c,w);}
// 4C22E0 (function 9 display): the 2E0050 gear cursor, then the panel or (+4B >= 1) the
// player marks.
void arcade_fn9_disp_4c22e0(PcRaceContext& c,std::uint32_t w){
    auto& m=c.m;
    if(m.u32(0x8449e8u))call(c,0x429530u,{0x2e0050u,0u,0u,0xau,m.u32(0x84a238u)?m.u32(0x84a21cu):0u});
    if(std::int8_t(m.u8(w+0x4bu))<1)arcade_panel_4c0a10(c,w);
    else arcade_entries_4bf570(c);
}
// 4C2420 (functions 3 / 7 / 0xA display): the panel.
void arcade_disp_4c2420(PcRaceContext& c,std::uint32_t w){arcade_panel_4c0a10(c,w);}
// 4C2C60(esi work): once the 8449C4 animation ended, the screen closes: the next sequence
// index (+1 after a cancel, else 8), event 0x181 closed, 440330(8, 0x18), the course world
// released, 84A214 = 0xC. Returns 1 when closed.
std::uint32_t arcade_close_4c2c60(PcRaceContext& c,std::uint32_t w){
    auto& m=c.m;
    std::uint32_t h=m.u32(0x8449c4u);
    if(std::int32_t(h)>=0){
        if(call(c,0x428880u,{h})!=3u)return 0;
        h=m.u32(0x8449c4u);
        m.put16(0x84494cu,1u);
    }
    call(c,0x4285a0u,{h});
    call(c,0x4285a0u,{m.u32(0x8449ccu)});
    if((m.u8(w+8u)&3u)==2u)m.put8(w+0x15u,std::uint8_t(m.u8(w+0x15u)+1u));
    else m.put8(w+0x15u,8u);
    m.put8(w+0xau,m.u8(w+0xau)|1u);
    arcade_sequence_4c2ac0(c,m.u32(w),m.u8(w+0x15u));
    call(c,0x4401d0u,{0x181u});
    call(c,0x440330u,{8u,0x18u});
    call(c,0x44fd00u,{});call(c,0x42ebf0u,{});call(c,0x44fcc0u,{0u});
    call(c,0x4489c0u,{});call(c,0x44c3d0u,{});call(c,0x44a1a0u,{});call(c,0x4f2210u,{});
    m.put32(0x84a214u,0xcu);
    return 1;
}
// ---- function inits 3, 6, 7, 10 ----------------------------------------------------------
// 4C18C0 (function 6 init): slots (0x10, 0x32D), the course list 84A208 = 5C8EA8, the
// 10-row list (4BEE90) on the first entry ([+50] == 1), players +1B, music 0x1E, sound 4,
// the two 449FA0 texture slots on 844A08 (+ the 844A38 / 844A78 transforms), the
// sel_dl_edit0.tgt picture (4239C0 / 423CB0 / 423BD0) and 448FD0(0x100, 4BFA20).
void arcade_fn6_init_4c18c0(PcRaceContext& c,std::uint32_t w){
    auto& m=c.m;
    m.put8(w+0x1fu,0xffu);m.put8(w+0x22u,0xffu);
    m.put32(0x844950u,0);m.put32(0x844954u,0);m.put32(0x844948u,0);m.put32(0x844928u,0);
    m.put32(0x84a208u,0x5c8ea8u);m.put8(0x84494eu,0);m.put32(0x84a2e0u,0);m.put32(0x84a2e4u,0);
    call(c,0x446d90u,{4u,0x295u,8u,0x296u,0x10u,0x32du,0xffffffffu,0xffffffffu},Object84a320);
    const std::uint8_t a=m.u8(w+0xau);
    const std::uint32_t first=m.u32(w+0x50u);
    m.put8(w+8u,m.u8(w+8u)&0xfcu);
    m.put32(0x84a214u,0xcu);
    m.put8(w+0xau,a&0xfcu);
    m.put16(w+0x10u,0x4b0u);m.put16(w+0x12u,0x3cu);
    if(first==1u){
        const std::uint32_t v=m.u32(0x780258u);
        std::uint32_t tok;
        if(v==3u||v==4u){const std::uint32_t k=call(c,0x455ad0u,{})&0xffu;tok=m.u32(0x689aacu+(call(c,0x455b00u,{k})&0xffu)*4u);}
        else tok=0x2e0019u;
        arcade_list_4bee90(c,tok,0xa,w,m.f32(0x689accu),m.f32(0x689ad0u),0.0f,7u);
    }
    m.put8(w+0x1bu,std::uint8_t(call(c,0x456d60u,{})));
    call(c,0x401000u,{0u,0x1eu,1u});
    call(c,0x4493c0u,{});
    call(c,0x424940u,{4u});
    std::array<float,16> t{};
    t[0]=t[1]=t[2]=m.f32(0x62806cu);
    t[12]=t[13]=m.f32(0x5c6fdcu);
    m.put32(call(c,0x449fa0u,{1u}),0x844a08u);
    m.put32(call(c,0x449fa0u,{2u}),0x844a08u);
    for(std::uint32_t k=0;k<16u;++k){m.putf(0x844a38u+k*4u,t[k]);}
    for(std::uint32_t k=0;k<16u;++k){m.putf(0x844a78u+k*4u,t[k]);}
    const std::uint32_t file=call(c,0x4239c0u,{0x59daecu,0x62563cu});
    if(file){call(c,0x423cb0u,{0x844a08u,0xb0u,0x80u,file});call(c,0x423bd0u,{file});}
    call(c,0x448fd0u,{0x100u,0x4bfa20u});
}
// 4C1F10 (function 7 init) / 4C2240 (function 3 init): +3E / +19 / +1A from the stick (> 100
// selects 1), 844940 (7) or 84A2A8 (3) = 0, arrows both ways. Function 7 also clears
// +1F / +22. (4C1F44 is the protected cmp [eax+518], 0.)
namespace {
void arcade_side_init(PcRaceContext& c,std::uint32_t w,std::uint32_t flag){
    auto& m=c.m;
    const std::uint32_t o=call(c,0x4035f0u,{});
    std::uint8_t v=0;
    if(!(m.i32(o+0x518u)>0)&&std::int32_t(call(c,0x453720u,{0u}))>0x64)v=1u;
    m.put8(w+0x3eu,v);m.put8(w+0x19u,v);m.put8(w+0x1au,v);
    m.put8(flag,0);m.put32(0x8449f4u,3u);
}
}
void arcade_fn7_init_4c1f10(PcRaceContext& c,std::uint32_t w){
    auto& m=c.m;
    m.put8(w+8u,m.u8(w+8u)&0xfcu);
    m.put32(0x844950u,0);
    m.put8(w+0x1fu,0xffu);m.put8(w+0x22u,0xffu);
    m.put8(w+0xau,m.u8(w+0xau)&0xfcu);
    m.put16(w+0x10u,0x258u);m.put16(w+0x12u,0x3cu);
    arcade_side_init(c,w,0x844940u);
}
void arcade_fn3_init_4c2240(PcRaceContext& c,std::uint32_t w){
    auto& m=c.m;
    m.put8(w+8u,m.u8(w+8u)&0xfcu);
    m.put32(0x844950u,0);
    m.put8(w+0xau,m.u8(w+0xau)&0xfcu);
    m.put16(w+0x10u,0x258u);m.put16(w+0x12u,0x3cu);
    arcade_side_init(c,w,0x84a2a8u);
}
// 4C2330 (function 0xA init): the name entry 84A220.. copied to +3C, the stick's side
// (1 centred, 2 right, 0 left) to +3C / +3D / +19 / +1A, 84A2C8 = 0.
void arcade_fn10_init_4c2330(PcRaceContext& c,std::uint32_t w){
    auto& m=c.m;
    m.put8(w+8u,m.u8(w+8u)&0xfcu);m.put8(w+0xau,m.u8(w+0xau)&0xfcu);
    for(std::uint32_t k=0;k<5u;++k)m.put32(w+0x3cu+k*4u,m.u32(0x84a220u+k*4u));
    m.put32(0x8449f4u,3u);
    std::uint8_t v;
    std::uint32_t o=call(c,0x4035f0u,{});
    bool centre=false;
    if(m.i32(o+0x518u)>0)centre=true;
    else{const std::int32_t s=std::int32_t(call(c,0x453720u,{0u}));if(!(s>0x64)&&!(s<-0x64))centre=true;}
    if(centre)v=1u;
    else{
        o=call(c,0x4035f0u,{});
        if(m.i32(o+0x518u)>0)v=0;
        else v=std::int32_t(call(c,0x453720u,{0u}))>0x64?2u:0u;
    }
    m.put8(w+0x3cu,v);m.put8(w+0x3du,v);m.put8(w+0x19u,v);m.put8(w+0x1au,v);
    m.put32(0x84a2c8u,0);m.put32(0x844958u,1u);
}
// 4C1AC0(edi work): the course screen controls: input 0x10 held 10 frames enters the course
// (route) list 844954 of the 5C9204 table (stick / 0x400 / 0x800 step it, wrapping); the
// list scroll 4C0EB0(w, 0, 1); a row change swaps the 2E0017 frame (428800 +-1) and plays
// sound 1; the models 4BF260; the chosen route goes to the player car +12, then event 0x181
// closes and the 449FA0 slots / 4BF340 camera follow over three frames.
void arcade_course_4c1ac0(PcRaceContext& c,std::uint32_t w){
    auto& m=c.m;
    std::uint32_t o=call(c,0x4035f0u,{});
    auto table=[&]{return m.u32(0x5c9204u+std::uint32_t(std::int32_t(m.i8(w+0x19u)))*4u);};
    if(!(m.i32(o+0x518u)>0)&&call(c,0x4536c0u,{0x10u})){
        const std::int8_t al=std::int8_t(m.u8(0x84494eu));
        if(al<0xa)m.put8(0x84494eu,std::uint8_t(al+1));
        else{
            m.put32(0x844948u,1u);m.put32(0x844928u,1u);
            m.put32(0x84a208u,table());
            m.put8(0x84a2a4u,1u);
            const std::int32_t esi=arcade_repeat_4bfac0(c);
            bool act=false;
            o=call(c,0x4035f0u,{});
            if(!(m.i32(o+0x518u)>0)&&call(c,0x4536f0u,{0x400u}))act=true;
            else{
                o=call(c,0x4035f0u,{});
                if(!(m.i32(o+0x518u)>0)&&call(c,0x4536f0u,{0x800u}))act=true;
                else if(esi!=0)act=true;
            }
            if(act){
                o=call(c,0x4035f0u,{});
                bool dec=false;
                if(!(m.i32(o+0x518u)>0)&&call(c,0x4536f0u,{0x800u}))dec=true;
                else if(esi<0)dec=true;
                const std::int32_t e=dec?m.i32(0x844954u)-1:m.i32(0x844954u)+1;
                m.put32(0x844954u,std::uint32_t(e));
                if(e<0){
                    const std::uint32_t t=m.u32(0x84a208u);
                    std::int8_t cl=0;
                    do{cl=std::int8_t(cl+1);}while(m.u32(t+std::uint32_t(std::int32_t(cl))*12u)!=0u);
                    m.put32(0x844954u,std::uint32_t(std::int32_t(cl)-1));
                }else if(m.u32(m.u32(0x84a208u)+std::uint32_t(e)*12u)==0u)m.put32(0x844954u,0);
            }
        }
    }else{
        const std::int8_t al=std::int8_t(m.u8(0x84494eu));
        if(al>0&&al<0xa){
            const std::uint32_t e=m.u32(0x844954u)+1u;
            m.put32(0x844954u,e);m.put32(0x844948u,1u);
            if(m.u32(m.u32(0x84a208u)+e*12u)==0u)m.put32(0x844954u,0);
        }
        m.put32(0x844928u,0);m.put8(0x84494eu,0);
    }
    arcade_scroll_4c0eb0(c,w,0,1u);
    if(m.u8(w+0x18u)){
        const std::uint32_t rec=m.u32(0x5c91dcu+std::uint32_t(std::int32_t(m.i8(w+0x1au)))*4u);
        for(std::uint32_t h:{0x84497cu,0x844980u,0x844984u})call(c,0x4285a0u,{m.u32(h)});
        for(std::uint32_t h:{0x84497cu,0x844980u,0x844984u})m.put32(h,0xffffffffu);
        const std::int8_t s=m.i8(w+0x18u);
        if(s==1||s==2){
            const std::uint32_t r=row(m.u32(rec+(s==1?0xcu:4u)));
            m.put32(0x844954u,0);m.put32(0x84a208u,table());
            call(c,0x4285a0u,{m.u32(0x8449ccu)});
            std::uint32_t h;
            if(s==1)h=call(c,0x428460u,{0x2e0017u,3u,1u,m.u32(r+4u),m.u32(r+8u)});
            else h=call(c,0x428460u,{0x2e0017u,3u,1u,m.u32(r+8u),m.u32(r+4u)});
            m.put32(0x8449ccu,h);
            call(c,0x428800u,{h,s==1?0x3f800000u:0xbf800000u});
        }
        call(c,0x4493c0u,{});
        call(c,0x424940u,{1u});
    }
    arcade_models_4bf260(c);
    if(!m.u32(0x84a2d8u))return;
    if(m.u32(0x84a2dcu)){
        const std::uint32_t t=table();
        m.put32(0x84a208u,t);
        m.put8(m.u32(0x799d18u)+0x12u,m.u8(t+m.u32(0x844954u)*12u+8u));
    }
    if(m.u8(w+0x18u)){
        call(c,0x4401d0u,{0x181u});
        call(c,0x440330u,{8u,0x18u});
        m.put32(0x84a2dcu,0);m.put32(0x84a2e4u,1u);
        return;
    }
    const std::uint32_t st=m.u32(0x84a2e4u);
    if(st==1u){
        m.put32(0x84a2e4u,2u);
        m.put32(call(c,0x449fa0u,{1u}),0x844a08u);
        m.put32(call(c,0x449fa0u,{2u}),0x844a08u);
        return;
    }
    if(st==2u){m.put32(0x84a2e4u,3u);return;}
    if(st!=3u)return;
    const std::uint8_t cl=m.u8(w+0x19u);
    const std::uint32_t k=std::uint32_t(std::int32_t(std::int8_t(cl)));
    const std::uint32_t ebp=m.u32(0x5c91dcu+k*4u);
    const std::uint32_t e=m.u32(0x689afcu+k*4u);
    if(m.u32(0x84a2e8u+e*4u)!=2u)return;
    const std::uint32_t t=m.u32(0x5c9204u+k*4u);
    m.put32(0x84a208u,t);
    if(!m.u32(0x84a218u)){
        arcade_4bf340(c,m.u32(ebp),m.u8(t+8u));
        m.put32(0x84a2dcu,1u);m.put32(0x84a2e4u,0);
        return;
    }
    const std::uint32_t n=arcade_index_4bf370(c,m.u32(0x8449ecu),cl);
    arcade_4bf340(c,m.u32(ebp),m.u8(t+n*12u+8u));
    m.put32(0x84a2e4u,0);
}
// 4C1FA0 (function 2 display): the panel, the course picture frame 12003B, the course names
// 5C94CC / 5C94B8 of +3C, and with +3E == 1 the stage 5C9440 picture and its number (bl =
// 5C9308[+3C * 6 + +3D]: tens 12000F, units 120010 - d).
void arcade_fn2_disp_4c1fa0(PcRaceContext& c,std::uint32_t w){
    auto& m=c.m;
    const std::uint32_t esi=std::uint32_t(std::int32_t(m.i8(w+0x3cu)));
    const std::uint32_t ebp=std::uint32_t(std::int32_t(m.i8(w+0x3du)))+esi*6u;
    const std::int8_t bl=m.i8(0x5c9308u+ebp);
    arcade_panel_4c0a10(c,w);
    call(c,0x42d5c0u,{0x12003bu,0x38u,0x4fu,0x40400000u,0xffffffffu});
    call(c,0x42d5c0u,{m.u32(0x5c94ccu+esi*4u),0x38u,0x4bu,0x40800000u,0xffffffffu});
    call(c,0x42d280u,{m.u32(0x5c94b8u+esi*4u),0xccu,0x16fu,0u,0x41000000u,0xffffffffu});
    if(m.u8(w+0x3eu)!=1u)return;
    call(c,0x42d5c0u,{m.u32(0x5c9440u+ebp*4u),0x38u,0x4bu,0x40a00000u,0xffffffffu});
    call(c,0x42d280u,{0x120006u,0x1a5u,0x172u,0u,0x41000000u,0xffffffffu});
    if(bl>=0xa)call(c,0x42d280u,{0x12000fu,0x1f6u,0x172u,0u,0x41000000u,0xffffffffu});
    const std::int32_t d=std::int32_t(bl)%10;
    if(std::uint32_t(d)<=9u)call(c,0x42d280u,{0x120010u-std::uint32_t(d),0x209u,0x172u,0u,0x41000000u,0xffffffffu});
}
// 4C3530 (function 2 control): the course / stage choice: the 2E001A frames once the intro
// played, the banner, +3E 0 picks the course +3C (0..4, picture 5C79BC), 1 its stage +3D
// (5C9510 counts, arrows 4BF780); confirm (input 8 / 4BFB20 / flags 1) moves +3E on and
// finally stores +24 / +25 / +26 with the 5C9290 / 5C9308 tables; once flags 2 / 3 the
// sequence goes on (+15 + 1 / - 1).
void arcade_fn2_ctrl_4c3530(PcRaceContext& c,std::uint32_t w){
    auto& m=c.m;
    if((m.u8(w+8u)&3u)<=1u&&m.i32(0x8449d4u)>=0&&call(c,0x428880u,{m.u32(0x8449d4u)})==3u){
        call(c,0x4285a0u,{m.u32(0x8449d0u)});call(c,0x4285a0u,{m.u32(0x8449c4u)});call(c,0x4285a0u,{m.u32(0x8449d4u)});
        m.put32(0x8449d4u,0xffffffffu);
        m.put32(0x8449d0u,call(c,0x428460u,{0x2e001au,4u,0u,0x1fu,0x5au}));
        m.put32(0x8449c4u,call(c,0x428460u,{0x2e001au,7u,0u,0x1fu,0x3cu}));
        m.put32(0x844958u,0);
    }
    arcade_banner_4c2430(c);
    m.put32(0x8449bcu,0xffffffffu);m.put32(0x8449c0u,0xffffffffu);
    if(m.u32(0x844958u))return;
    if(m.i32(0x84a310u)>0)m.put32(0x84a310u,m.u32(0x84a310u)-1u);
    const std::uint16_t ax=m.u16(w+8u);
    const std::uint16_t cx=ax&3u;
    if(cx>1u){
        if(m.i32(0x8449c4u)>=0&&call(c,0x428880u,{m.u32(0x8449c4u)})!=3u)return;
        call(c,0x4285a0u,{m.u32(0x8449d0u)});call(c,0x4285a0u,{m.u32(0x8449d4u)});call(c,0x4285a0u,{m.u32(0x8449c4u)});
        m.put32(0x8449d0u,0xffffffffu);m.put32(0x8449d4u,0xffffffffu);m.put32(0x8449c4u,0xffffffffu);
        const std::uint16_t f=m.u16(w+8u)&3u;
        if(f==2u)m.put8(w+0x15u,std::uint8_t(m.u8(w+0x15u)+1u));
        else if(f==3u)m.put8(w+0x15u,std::uint8_t(m.u8(w+0x15u)-1u));
        m.put8(w+0xau,m.u8(w+0xau)|1u);
        arcade_sequence_4c2ac0(c,m.u32(w),m.u8(w+0x15u));
        return;
    }
    if(!(m.i16(w+0x10u)>0)&&cx==0u)m.put16(w+8u,std::uint16_t((ax&0xfffdu)|1u));
    std::uint32_t o=call(c,0x4035f0u,{});
    bool confirm=!(m.i32(o+0x518u)>0)&&call(c,0x4536f0u,{8u});
    if(!confirm)confirm=result_input_4bfb20(c)!=0;
    if(!confirm)confirm=(m.u8(w+8u)&3u)==1u;
    if(!confirm){
        const std::int8_t v=m.i8(w+0x3eu);
        if(v==0){
            const std::int32_t r=arcade_stick_4c0bd0(c,1u);
            std::int8_t al=m.i8(w+0x3cu);
            if(r==-1){if(al<=0)return;m.put8(w+0x3cu,std::uint8_t(al-1));}
            else if(r==1){if(al>=4)return;m.put8(w+0x3cu,std::uint8_t(al+1));}
            else return;
        }else if(v==1){
            const std::int32_t r=arcade_stick_4c0bd0(c,1u);
            std::int8_t al=m.i8(w+0x3du);
            if(r==-1){if(al<=0)return;m.put8(w+0x3du,std::uint8_t(al-1));}
            else if(r==1){
                const std::int32_t n=std::int32_t(m.u8(0x5c9510u+std::uint32_t(std::int32_t(m.i8(w+0x3cu)))))-1;
                if(std::int32_t(al)>=n)return;
                m.put8(w+0x3du,std::uint8_t(al+1));
                arcade_arrows_4bf780(c);
            }else return;
        }else return;
        if(m.u8(w+0x3eu)==0u){
            const std::uint32_t r=0x5c79bcu+std::uint32_t(std::int32_t(m.i8(w+0x3cu)))*12u;
            call(c,0x4285a0u,{m.u32(0x8449d0u)});
            m.put32(0x8449d0u,create_row(c,r,6u,0u));
        }
        call(c,0x4493c0u,{});
        call(c,0x424940u,{0x164u});
        return;
    }
    const std::int8_t v=m.i8(w+0x3eu);
    if(v==0){
        call(c,0x4493c0u,{});
        call(c,0x424940u,{0x40u});
        const std::uint8_t al=m.u8(w+0x3cu);
        m.put16(w+0x10u,0x258u);m.put16(w+0x12u,0x3cu);m.put8(w+0x3eu,1u);m.put8(w+0x25u,al);
        arcade_sound_4bee60(c,m.u32(m.u32(0x6898d8u)+std::uint32_t(std::int32_t(std::int8_t(al)))*4u+0x54u));
        const std::uint8_t a2=m.u8(w+0x25u);
        m.put32(0x84a310u,0x3cu);
        if(!(a2==0u||a2==4u))return;
    }else if(v!=1)return;
    const std::uint32_t r=row(m.u32(0x5c94fcu+std::uint32_t(std::int32_t(m.i8(w+0x3cu)))*4u));
    call(c,0x4285a0u,{m.u32(0x8449d0u)});call(c,0x4285a0u,{m.u32(0x8449c4u)});call(c,0x4285a0u,{m.u32(0x8449d4u)});
    m.put32(0x8449d0u,0xffffffffu);
    m.put32(0x8449d4u,call(c,0x428460u,{0x2e001au,4u,1u,0x1e1u,0x1feu}));
    m.put32(0x8449c4u,call(c,0x428460u,{0x2e001au,7u,1u,0x3du,0x5au}));
    call(c,0x428460u,{m.u32(r),9u,3u,m.u32(r+4u),m.u32(r+8u)});
    const std::uint8_t al=m.u8(w+0x3du);
    const std::uint32_t k=std::uint32_t(std::int32_t(m.i8(w+0x3cu)))*6u+std::uint32_t(std::int32_t(std::int8_t(al)));
    m.put8(w+0x26u,al);
    m.put8(w+0x24u,m.u8(0x5c9290u+k*4u));
    if(!(m.i32(0x84a310u)>0))
        arcade_sound_4bee60(c,m.u32(m.u32(0x6898d8u)+std::uint32_t(std::int32_t(m.i8(0x5c9308u+k)))*4u+0x64u));
    o=call(c,0x4035f0u,{});
    if(!(m.i32(o+0x518u)>0)&&call(c,0x4536f0u,{8u}))m.put8(w+8u,m.u8(w+8u)|3u);
    else m.put16(w+8u,std::uint16_t((m.u16(w+8u)&0xfffeu)|2u));
    m.put16(w+0x10u,0);
    call(c,0x4493c0u,{});
    call(c,0x424940u,{0x40u});
}
// 4C3960 (function 3 control): the two-way choice of the 84A240 record (+3E 0 / 1, frames
// +4 / +C or +8 / +10 then +14 for the switch, +18 / +1C on confirm, sounds 6898D8 +48 / +4C),
// the class banner [+58] / [+5C] / [+60]; flags 2 / 3 run the sequence on (+23 = -1 on cancel).
// (4C3A57 is the protected cmp [eax+518], 0.)
void arcade_fn3_ctrl_4c3960(PcRaceContext& c,std::uint32_t w){
    auto& m=c.m;
    std::int32_t edi=0;
    if((m.u8(w+8u)&3u)<=1u&&m.i32(0x8449a0u)>=0&&call(c,0x428880u,{m.u32(0x8449a0u)})==3u){
        call(c,0x4285a0u,{m.u32(0x8449a0u)});
        const std::uint32_t rec=m.u32(0x84a240u);
        m.put32(0x8449a0u,0xffffffffu);
        std::uint32_t a,b;
        if(m.u8(w+0x3eu)==0u){a=m.u32(rec+4u);b=m.u32(rec+0xcu);}
        else{a=m.u32(rec+8u);b=m.u32(rec+0x10u);}
        const std::uint32_t ra=row(a),rb=row(b);
        if(m.u8(0x84a2a8u)==0u){
            m.put32(0x8449a0u,create_row(c,ra,0xcu,1u));
            m.put8(0x84a2a8u,1u);
        }else{
            m.put32(0x84499cu,create_row(c,rb,0xcu,0u));
            m.put32(0x844958u,0);
        }
    }
    arcade_banner_4c2430(c);
    if(m.u32(0x844958u))return;
    if((m.u8(w+8u)&3u)>1u){
        std::uint32_t h=m.u32(0x8449a0u);
        if(std::int32_t(h)>=0){if(call(c,0x428880u,{h})!=3u)return;h=m.u32(0x8449a0u);}
        call(c,0x4285a0u,{h});
        const std::uint8_t bl=std::uint8_t(m.u8(w+0xau)|1u);
        const std::uint16_t f=m.u16(w+8u)&3u;
        m.put8(w+0xau,bl);
        if(f==2u)m.put8(w+0x15u,std::uint8_t(m.u8(w+0x15u)+1u));
        else if(f==3u){m.put8(w+0x23u,0xffu);m.put8(w+0x15u,std::uint8_t(m.u8(w+0x15u)-1u));}
        arcade_sequence_4c2ac0(c,m.u32(w),m.u8(w+0x15u));
        return;
    }
    std::uint32_t o=call(c,0x4035f0u,{});
    if(!(m.i32(o+0x518u)>0))call(c,0x453720u,{0u});
    o=call(c,0x4035f0u,{});
    if(!(m.i32(o+0x518u)>0))call(c,0x453750u,{0u});
    m.put8(w+0x18u,0);
    if(m.u32(0x8449a0u)==0xffffffffu)edi=arcade_stick_4c0bd0(c,1u);
    m.put32(0x8449bcu,0xffffffffu);m.put32(0x8449c0u,0xffffffffu);
    if(!(m.i16(w+0x10u)>0)){const std::uint16_t f=m.u16(w+8u);if(!(f&3u))m.put16(w+8u,std::uint16_t((f&0xfffdu)|1u));}
    o=call(c,0x4035f0u,{});
    bool confirm=!(m.i32(o+0x518u)>0)&&call(c,0x4536f0u,{8u});
    if(!confirm)confirm=result_input_4bfb20(c)!=0;
    if(!confirm)confirm=(m.u8(w+8u)&3u)==1u;
    if(!confirm){
        if(edi==-1){
            if(m.u8(w+0x3eu)==0u)return;
            const std::uint8_t al=m.u8(w+0x19u);
            const std::uint32_t rec=m.u32(0x84a240u);
            m.put8(w+0x3eu,0);m.put8(w+0x1au,al);m.put8(w+0x19u,0);m.put8(w+0x18u,1u);
            const std::uint32_t r=row(m.u32(rec+0x14u));
            call(c,0x4285a0u,{m.u32(0x84499cu)});call(c,0x4285a0u,{m.u32(0x8449a0u)});
            const std::uint32_t h=call(c,0x428460u,{m.u32(r),0xcu,1u,m.u32(r+8u),m.u32(r+4u)});
            m.put32(0x8449a0u,h);
            call(c,0x428800u,{h,fb(0.0f-m.f32(0x6898d0u))});
        }else if(edi==1){
            if(m.u8(w+0x3eu)==1u)return;
            const std::uint8_t cl=m.u8(w+0x19u);
            m.put8(w+0x3eu,1u);m.put8(w+0x1au,cl);m.put8(w+0x19u,1u);m.put8(w+0x18u,1u);
            const std::uint32_t r=row(m.u32(m.u32(0x84a240u)+0x14u));
            call(c,0x4285a0u,{m.u32(0x84499cu)});call(c,0x4285a0u,{m.u32(0x8449a0u)});
            const std::uint32_t h=call(c,0x428460u,{m.u32(r),0xcu,1u,m.u32(r+4u),m.u32(r+8u)});
            m.put32(0x8449a0u,h);
            call(c,0x428800u,{h,m.u32(0x6898d0u)});
        }else return;
        call(c,0x4493c0u,{});
        call(c,0x424940u,{0x164u});
        return;
    }
    const std::uint8_t al=m.u8(w+0x3eu);
    m.put8(w+0x23u,al);
    std::uint32_t k;
    if(al==0u){arcade_sound_4bee60(c,m.u32(m.u32(0x6898d8u)+0x48u));k=m.u32(m.u32(0x84a240u)+0x18u);}
    else{arcade_sound_4bee60(c,m.u32(m.u32(0x6898d8u)+0x4cu));k=m.u32(m.u32(0x84a240u)+0x1cu);}
    const std::uint32_t r=row(k);
    call(c,0x4285a0u,{m.u32(0x84499cu)});
    m.put32(0x84499cu,0xffffffffu);m.put32(0x8449a0u,0xffffffffu);
    m.put32(0x8449a0u,create_row(c,r,0xcu,1u));
    o=call(c,0x4035f0u,{});
    if(!(m.i32(o+0x518u)>0)&&call(c,0x4536f0u,{8u})){m.put8(w+8u,m.u8(w+8u)|3u);m.put8(w+0x23u,0xffu);}
    else m.put16(w+8u,std::uint16_t((m.u16(w+8u)&0xfffeu)|2u));
    m.put16(w+0x10u,0);
    call(c,0x4493c0u,{});
    call(c,0x424940u,{0x40u});
    m.put32(0x8449bcu,0xffffffffu);m.put32(0x8449c0u,0xffffffffu);
    const std::uint32_t rr=row(m.u32(w+0x58u));
    call(c,0x4285a0u,{m.u32(0x8449ccu)});call(c,0x4285a0u,{m.u32(0x8449d8u)});
    call(c,0x428460u,{m.u32(w+0x5cu),4u,3u,m.u32(rr+4u),m.u32(rr+8u)});
    call(c,0x428460u,{m.u32(w+0x60u),9u,3u,m.u32(rr+4u),m.u32(rr+8u)});
}
// 4C30B0 (function 7 control): the two-way 2E004F choice (+3E, frames 5C768C / 5C76A4 or
// 5C7698 / 5C76B0, switch 0x77 / 0x5A), confirm stores +22 (sounds 6898D8 +30 / +34,
// 5C76C8 / 5C76D4), the 2E001A course frame of the [+1D] record; flags 2 / 3 run the sequence
// on (+2 for one player). (4C3192 is the protected mov ecx, [eax+518].)
void arcade_fn7_ctrl_4c30b0(PcRaceContext& c,std::uint32_t w){
    auto& m=c.m;
    std::int32_t esi=0;
    if((m.u8(w+8u)&3u)<=1u&&m.i32(0x844990u)>=0&&call(c,0x428880u,{m.u32(0x844990u)})==3u){
        call(c,0x4285a0u,{m.u32(0x844990u)});
        m.put32(0x844990u,0xffffffffu);
        std::uint32_t a,b;
        if(m.u8(w+0x3eu)==0u){a=0x5c768cu;b=0x5c76a4u;}else{a=0x5c7698u;b=0x5c76b0u;}
        if(m.u8(0x844940u)==0u){
            m.put32(0x844990u,create_row(c,a,8u,1u));
            m.put8(0x844940u,1u);
        }else{
            m.put32(0x84498cu,create_row(c,b,8u,0u));
            m.put32(0x844958u,0);
        }
    }
    arcade_banner_4c2430(c);
    if(m.u32(0x844958u))return;
    if((m.u8(w+8u)&3u)>1u){
        std::uint32_t h=m.u32(0x844990u);
        if(std::int32_t(h)>=0){if(call(c,0x428880u,{h})!=3u)return;h=m.u32(0x844990u);}
        call(c,0x4285a0u,{h});
        m.put8(w+0xau,m.u8(w+0xau)|1u);
        std::uint8_t al;
        if((m.u8(w+8u)&3u)==2u){
            const std::uint32_t n=call(c,0x456d60u,{})&0xffu;
            al=m.u8(w+0x15u);
            al=std::uint8_t(n>1u?al+1u:al+2u);
        }else al=std::uint8_t(m.u8(w+0x15u)-1u);
        m.put8(w+0x15u,al);
        arcade_sequence_4c2ac0(c,m.u32(w),al);
        return;
    }
    std::uint32_t o=call(c,0x4035f0u,{});
    if(!(m.i32(o+0x518u)>0))call(c,0x453720u,{0u});
    o=call(c,0x4035f0u,{});
    if(!(m.i32(o+0x518u)>0))call(c,0x453750u,{0u});
    m.put8(w+0x18u,0);
    if(m.u32(0x844990u)==0xffffffffu)esi=arcade_stick_4c0bd0(c,1u);
    m.put32(0x8449bcu,0xffffffffu);m.put32(0x8449c0u,0xffffffffu);
    if(!(m.i16(w+0x10u)>0)){const std::uint16_t f=m.u16(w+8u);if(!(f&3u))m.put16(w+8u,std::uint16_t((f&0xfffdu)|1u));}
    o=call(c,0x4035f0u,{});
    bool confirm=!(m.i32(o+0x518u)>0)&&call(c,0x4536f0u,{8u});
    if(!confirm)confirm=result_input_4bfb20(c)!=0;
    if(!confirm)confirm=(m.u8(w+8u)&3u)==1u;
    if(!confirm){
        if(esi==-1){
            if(m.u8(w+0x3eu)==0u)return;
            const std::uint8_t al=m.u8(w+0x19u);
            m.put8(w+0x3eu,0);m.put8(w+0x1au,al);m.put8(w+0x19u,0);m.put8(w+0x18u,1u);
            call(c,0x4285a0u,{m.u32(0x84498cu)});call(c,0x4285a0u,{m.u32(0x844990u)});
            const std::uint32_t h=call(c,0x428460u,{0x2e004fu,8u,1u,0x77u,0x5au});
            m.put32(0x844990u,h);
            call(c,0x428800u,{h,fb(0.0f-m.f32(0x6898d0u))});
        }else if(esi==1){
            if(m.u8(w+0x3eu)==1u)return;
            const std::uint8_t al=m.u8(w+0x19u);
            m.put8(w+0x3eu,1u);m.put8(w+0x1au,al);m.put8(w+0x19u,1u);m.put8(w+0x18u,1u);
            call(c,0x4285a0u,{m.u32(0x84498cu)});call(c,0x4285a0u,{m.u32(0x844990u)});
            const std::uint32_t h=call(c,0x428460u,{0x2e004fu,8u,1u,0x5au,0x77u});
            m.put32(0x844990u,h);
            call(c,0x428800u,{h,m.u32(0x6898d0u)});
        }else return;
        call(c,0x4493c0u,{});
        call(c,0x424940u,{0x164u});
        return;
    }
    m.put8(w+0x22u,m.u8(w+0x3eu));
    call(c,0x4285a0u,{m.u32(0x84498cu)});
    m.put32(0x84498cu,0xffffffffu);m.put32(0x844990u,0xffffffffu);
    o=call(c,0x4035f0u,{});
    if(!(!(m.i32(o+0x518u)>0)&&call(c,0x4536f0u,{8u}))){
        std::uint32_t r;
        if(m.u8(w+0x22u)==0u){arcade_sound_4bee60(c,m.u32(m.u32(0x6898d8u)+0x30u));r=0x5c76c8u;}
        else{arcade_sound_4bee60(c,m.u32(m.u32(0x6898d8u)+0x34u));r=0x5c76d4u;}
        m.put32(0x844990u,create_row(c,r,8u,1u));
    }
    o=call(c,0x4035f0u,{});
    if(!(m.i32(o+0x518u)>0)&&call(c,0x4536f0u,{8u})){m.put8(w+8u,m.u8(w+8u)|3u);m.put8(w+0x22u,0xffu);}
    else m.put16(w+8u,std::uint16_t((m.u16(w+8u)&0xfffeu)|2u));
    m.put16(w+0x10u,0);
    call(c,0x4493c0u,{});
    call(c,0x424940u,{0x40u});
    m.put32(0x8449bcu,0xffffffffu);m.put32(0x8449c0u,0xffffffffu);
    const std::uint32_t r=row(m.u32(m.u32(0x5c91dcu+std::uint32_t(std::int32_t(m.i8(w+0x1du)))*4u)+0x14u));
    m.put16(0x84494cu,1u);
    call(c,0x4285a0u,{m.u32(0x8449c4u)});call(c,0x4285a0u,{m.u32(0x844984u)});
    m.put32(0x8449c4u,0xffffffffu);m.put32(0x844984u,0xffffffffu);
    const std::uint32_t v=m.u32(0x780258u);
    std::uint32_t tok;
    if(v==3u||v==4u){const std::uint32_t a=call(c,0x455ad0u,{})&0xffu;tok=m.u32(0x689abcu+(call(c,0x455b00u,{a})&0xffu)*4u);}
    else tok=0x2e001au;
    call(c,0x428460u,{tok,4u,3u,m.u32(r+4u),m.u32(r+8u)});
}
// 4C2D20 (function 6 control): load the course models (4BF2E0, then 448980 / 4F21B0), set the
// camera 4BF340 for the route; then each frame the item animations, the banner, the 2E0005 /
// 2C00E2 title, the other players' 2E0056 markers (rank among the players by 455B00, column
// 4591F0 from the 10 positions 5C2138 / 5C94F8 / 6281C8 / 5C94F4 / 5C94F0 / 5C94EC /
// 5C94E8 / 5C94E4 / 5C2134 / 5C94E0), the choice 4BFBD0 or the course controls 4C1AC0;
// flags 2 / 3 close the screen (4C2C60).
void arcade_fn6_ctrl_4c2d20(PcRaceContext& c,std::uint32_t w){
    auto& m=c.m;
    const std::uint32_t st=m.u32(0x84a2e0u);
    if(st==0u){
        arcade_4bf2e0(c,m.u32(m.u32(0x5c91dcu+std::uint32_t(std::int32_t(m.i8(w+0x19u)))*4u)));
        m.put32(0x689b24u,0);
        for(std::uint32_t a=0x84a2e8u;a<=0x84a30cu;a+=4u)m.put32(a,0);
        m.put32(0x84a2e0u,m.u32(0x84a2e0u)+1u);
        return;
    }
    if(st==1u){
        if(!call(c,0x448980u,{}))return;
        if(!call(c,0x4f21b0u,{}))return;
        m.put32(0x84a2e0u,m.u32(0x84a2e0u)+1u);
        return;
    }
    if(st==2u){
        const std::uint8_t cl=m.u8(w+0x19u);
        const std::uint32_t k=std::uint32_t(std::int32_t(std::int8_t(cl)));
        const std::uint32_t t=m.u32(0x5c9204u+k*4u),rec=m.u32(0x5c91dcu+k*4u);
        m.put32(0x84a208u,t);
        if(!m.u32(0x84a218u)){
            arcade_4bf340(c,m.u32(rec),m.u8(t+8u));
            m.put32(0x84a2dcu,1u);
        }else{
            m.put32(0x8449ecu,m.u32(0x5c70a8u+(k+m.u32(0x84a218u)*10u)*4u));
            const std::uint32_t n=arcade_index_4bf370(c,m.u32(0x8449ecu),cl);
            arcade_4bf340(c,m.u32(rec),m.u8(t+n*12u+8u));
        }
        m.put32(0x84a2d8u,1u);
        m.put32(0x84a2e0u,m.u32(0x84a2e0u)+1u);
        return;
    }
    if((m.u8(w+8u)&3u)<=1u)arcade_4bf030(c,w);
    arcade_banner_4c2430(c);
    m.put32(0x8449bcu,0xffffffffu);m.put32(0x8449c0u,0xffffffffu);
    if(m.u32(0x844958u))return;
    const std::uint8_t pl=m.u8(w+0x1bu);
    call(c,0x429530u,{(pl==0u||pl==1u)?0x2e0005u:0x2c00e2u,0u,0u,0xau,0u});
    if((call(c,0x456d60u,{})&0xffu)>1u){
        std::uint8_t b12=0;
        if(call(c,0x456d60u,{})&0xffu){
            std::uint32_t esi=0;
            for(;;){
                if(esi!=(call(c,0x455ad0u,{})&0xffu)){
                    const std::uint8_t b13=std::uint8_t(call(c,0x455b00u,{esi}));
                    std::uint8_t rank=0;
                    std::uint8_t bl=0;
                    if(call(c,0x456d60u,{})&0xffu){
                        std::uint32_t ebp=0;
                        do{
                            if(ebp!=(call(c,0x455ad0u,{})&0xffu)&&bl!=b12){
                                const std::int32_t p=std::int32_t(call(c,0x455b00u,{ebp})&0xffu);
                                if(p<std::int32_t(std::int8_t(b13)))rank=std::uint8_t(rank+1u);
                            }
                            bl=std::uint8_t(bl+1u);
                            ebp=std::uint32_t(std::int32_t(std::int8_t(bl)));
                        }while(std::int32_t(ebp)<std::int32_t(call(c,0x456d60u,{})&0xffu));
                    }
                    std::int32_t idx=std::int32_t(std::int8_t(call(c,0x4591f0u,{esi})));
                    if(idx<0||idx>=0xa)idx=0;
                    static constexpr std::uint32_t X[10]{0x5c2138u,0x5c94f8u,0x6281c8u,0x5c94f4u,0x5c94f0u,0x5c94ecu,0x5c94e8u,0x5c94e4u,0x5c2134u,0x5c94e0u};
                    float x=float(std::int32_t(std::int8_t(rank)))*m.f32(0x6281d0u);
                    x=x+m.f32(X[idx]);
                    call(c,0x429530u,{0x2e0056u,fb(x),0x43c08000u,0xau,std::uint32_t(std::int32_t(std::int8_t(b13)))});
                }
                b12=std::uint8_t(b12+1u);
                esi=std::uint32_t(std::int32_t(std::int8_t(b12)));
                if(!(std::int32_t(esi)<std::int32_t(call(c,0x456d60u,{})&0xffu)))break;
            }
        }
    }
    const std::uint16_t ax=m.u16(w+8u);
    const std::uint16_t cx=ax&3u;
    if(cx>1u){arcade_close_4c2c60(c,w);return;}
    if(!(m.i16(w+0x10u)>0)&&cx==0u)m.put16(w+8u,std::uint16_t((ax&0xfffdu)|1u));
    if(arcade_choose_4bfbd0(c,w)==1u)return;
    arcade_course_4c1ac0(c,w);
    call(c,0x48b130u,{m.u8(m.u32(0x5c91dcu+std::uint32_t(std::int32_t(m.i8(w+0x19u)))*4u))});
}
// 4C26D0 (function 0xA control): the three-way 5C9328 choice (+3C 0..2, frames +8 / +C / +10,
// arrows 8449F4), the music 4BF970, the banner; confirm (4BFB20 / 453780(2) / flags 1)
// shows the choice (or the 2E0050 back cursor for 453780(2)) and stores +21; the name entry
// copy 84A220.. on the way out.
void arcade_fn10_ctrl_4c26d0(PcRaceContext& c,std::uint32_t w){
    auto& m=c.m;
    std::int32_t ebp=-1;
    auto rec=[&](std::uint32_t k){return m.u32(0x5c9328u+std::uint32_t(std::int32_t(std::int8_t(k)))*4u);};
    if((m.u8(w+8u)&3u)<=1u&&m.i32(0x8449a8u)>=0&&call(c,0x428880u,{m.u32(0x8449a8u)})==3u){
        call(c,0x4285a0u,{m.u32(0x8449a8u)});
        m.put32(0x8449a8u,0xffffffffu);
        m.put32(0x8449a4u,create_row(c,row(m.u32(rec(m.u8(w+0x3cu))+0xcu)),9u,0u));
        const std::uint8_t al=m.u8(w+0x3cu);
        m.put32(0x844958u,0);
        if(al==0u)m.put32(0x8449f4u,1u);
        else m.put32(0x8449f4u,(al!=2u?1u:0u)+2u);
    }
    m.put32(0x8449bcu,0xffffffffu);m.put32(0x8449c0u,0xffffffffu);
    arcade_music_4bf970(c);
    arcade_banner_4c2430(c);
    if(m.u32(0x844958u))return;
    const std::uint16_t ax=m.u16(w+8u);
    const std::uint16_t cx=ax&3u;
    if(cx>1u){
        if(m.i32(0x8449a8u)<0)return;
        if(call(c,0x428880u,{m.u32(0x8449a8u)})!=3u)return;
        if(m.u32(0x84a2c8u)){
            for(std::uint32_t k=0;k<5u;++k)m.put32(0x84a220u+k*4u,m.u32(w+0x3cu+k*4u));
            m.put8(0x84a234u,m.u8(w+0x19u));m.put8(0x84a235u,m.u8(w+0x1au));
        }
        call(c,0x4285a0u,{m.u32(0x8449a8u)});
        m.put8(w+0xau,m.u8(w+0xau)|1u);
        m.put8(w+0x15u,std::uint8_t(m.u8(w+0x15u)-1u));
        return;
    }
    if(!(m.i16(w+0x10u)>0)&&cx==0u)m.put16(w+8u,std::uint16_t((ax&0xfffdu)|1u));
    bool confirm=result_input_4bfb20(c)!=0;
    if(!confirm){const std::uint32_t o=call(c,0x4035f0u,{});if(!(m.i32(o+0x518u)>0)&&call(c,0x453780u,{2u}))confirm=true;}
    if(!confirm&&(m.u8(w+8u)&3u)==1u)confirm=true;
    if(confirm){
        call(c,0x4285a0u,{m.u32(0x8449a8u)});call(c,0x4285a0u,{m.u32(0x8449a4u)});
        m.put32(0x8449a4u,0xffffffffu);
        const std::uint32_t o=call(c,0x4035f0u,{});
        if(!(m.i32(o+0x518u)>0)&&call(c,0x453780u,{2u})){
            const std::uint32_t h=call(c,0x428460u,{0x2e0050u,9u,1u,0xdu,0u});
            m.put32(0x8449a8u,h);
            call(c,0x428800u,{h,0xbf800000u});
        }else{
            m.put32(0x8449a8u,create_row(c,row(m.u32(rec(m.u8(w+0x3cu))+0x10u)),9u,1u));
            m.put8(w+0x21u,m.u8(w+0x3cu));
            call(c,0x4493c0u,{});
            call(c,0x424940u,{0x40u});
            m.put32(0x84a2c8u,1u);
        }
        m.put16(w+8u,std::uint16_t((m.u16(w+8u)&0xfffeu)|2u));
        m.put32(0x8449bcu,0xffffffffu);m.put32(0x8449c0u,0xffffffffu);
    }
    const std::uint8_t wait=m.u8(w+0x4cu);
    m.put8(w+0x18u,0);
    if(wait){m.put8(w+0x4cu,std::uint8_t(wait-1u));return;}
    const std::int32_t r=arcade_stick_4c0bd0(c,1u);
    std::uint32_t edi;
    if(r==-1){
        const std::uint8_t al=m.u8(w+0x3cu);
        if(al==0u)return;
        const std::uint8_t dl=m.u8(w+0x19u);
        m.put8(w+0x3du,al);
        edi=std::uint32_t(std::int32_t(m.i8(w+0x3du)));
        m.put8(w+0x3cu,std::uint8_t(al-1u));m.put8(w+0x1au,dl);m.put8(w+0x19u,std::uint8_t(al-1u));m.put8(w+0x18u,1u);
    }else if(r==1){
        std::uint8_t al=m.u8(w+0x3cu);
        if(al==2u)return;
        const std::uint8_t cl=m.u8(w+0x19u);
        m.put8(w+0x3du,al);
        al=std::uint8_t(al+1u);
        ebp=1;
        m.put8(w+0x3cu,al);m.put8(w+0x1au,cl);m.put8(w+0x19u,al);m.put8(w+0x18u,1u);
        edi=std::uint32_t(std::int32_t(std::int8_t(al)));
    }else return;
    call(c,0x4285a0u,{m.u32(0x8449a8u)});call(c,0x4285a0u,{m.u32(0x8449a4u)});
    m.put32(0x8449a4u,0xffffffffu);m.put32(0x8449a8u,0xffffffffu);
    if(m.u8(w+0x18u)==1u){
        const std::uint32_t rr=row(m.u32(m.u32(0x5c9328u+edi*4u)+8u));
        if(ebp<0){
            const std::uint32_t h=call(c,0x428460u,{m.u32(rr),9u,1u,m.u32(rr+8u),m.u32(rr+4u)});
            m.put32(0x8449a8u,h);
            call(c,0x428800u,{h,fb(0.0f-m.f32(0x6898d0u))});
        }else{
            const std::uint32_t h=call(c,0x428460u,{m.u32(rr),9u,1u,m.u32(rr+4u),m.u32(rr+8u)});
            m.put32(0x8449a8u,h);
            call(c,0x428800u,{h,m.u32(0x6898d0u)});
        }
    }
    call(c,0x4493c0u,{});
    call(c,0x424940u,{3u});
}
// 4C3DC0 (function 5 control): the game / class list (+19 rows 0..4 of the 689DA8 records by
// +3E, scroll 4C0EB0 with input 0x40000 flipping +40), the record banner 84A23C / 2E0024 /
// cursor, the side switch (+18 3 / 4: +3E, 0x456 / 0x473 frames); confirm sets the choice
// sound / frames and flags; leaving (+41 0 then 1) stores +58 / +5C / +60, the variant +1F
// (43F940), the preset +27 (43F950) and loads its course data (44DA00 0 / 1 / 5 / 6, 44C0B0).
void arcade_fn5_ctrl_4c3dc0(PcRaceContext& c,std::uint32_t w){
    auto& m=c.m;
    const std::uint32_t esi=std::uint32_t(std::int32_t(m.i8(w+0x3eu)));
    std::uint32_t ebp=0;
    auto cursor=[&]{return m.u32(0x689e50u+m.u32(0x844944u)*4u);};
    if(m.u32(0x8449e0u)==1u){m.put8(0x84a291u,5u);m.put32(0x84a298u,cursor());}
    if((m.u8(w+8u)&3u)<=1u&&m.i32(0x8449ccu)>=0&&call(c,0x428880u,{m.u32(0x8449ccu)})==3u){
        arcade_4bf400(c,w);
        if(m.u32(0x844958u)){
            call(c,0x4285a0u,{m.u32(0x8449c4u)});
            const std::int32_t y=cvtt(m.f32(0x84a284u)),x=cvtt(m.f32(0x84a280u));
            m.put32(0x8449c4u,call(c,0x428460u,{cursor(),7u,2u,std::uint32_t(x),std::uint32_t(y)}));
            call(c,0x48b170u,{m.u8(w+0x22u)});
        }
        m.put32(0x844958u,0);
    }
    switch(std::uint32_t(std::int32_t(m.i8(w+0x19u)))){
    case 0:case 1:case 2:m.put32(0x84a214u,0xcu);break;
    case 3:case 4:m.put32(0x84a214u,0xbu);break;
    default:break;
    }
    arcade_banner_4c2430(c);
    m.put32(0x8449bcu,0xffffffffu);m.put32(0x8449c0u,0xffffffffu);
    if(m.u32(0x844958u))return;
    const std::uint16_t ax=m.u16(w+8u);
    const std::uint16_t cx=ax&3u;
    if(cx>1u){
        const std::uint8_t st=m.u8(w+0x41u);
        if(st==0u){
            std::uint32_t h=m.u32(0x8449ccu);
            if(std::int32_t(h)>=0){if(call(c,0x428880u,{h})!=3u)return;h=m.u32(0x8449ccu);}
            std::uint8_t cl=m.u8(w+0x19u);
            m.put16(0x84494cu,1u);
            if(cl==0u||cl==1u||cl==3u){m.put32(0x8449bcu,0xffffffffu);m.put32(0x8449c0u,0xffffffffu);}
            cl=m.u8(w+0x19u);
            if(cl!=2u&&cl!=4u){
                call(c,0x4285a0u,{h});call(c,0x4285a0u,{m.u32(0x8449d8u)});
                const std::uint32_t r=row(m.u32(esi*0x54u+0x689df8u));
                m.put32(0x8449ccu,0xffffffffu);m.put32(0x8449d8u,0xffffffffu);
                m.put32(0x8449ccu,call(c,0x428460u,{m.u32(0x84a24cu+(esi+m.u32(0x844944u)*2u)*4u),4u,1u,m.u32(r+4u),m.u32(r+8u)}));
                m.put32(0x8449d8u,call(c,0x428460u,{m.u32(0x689e58u+esi*4u),9u,1u,m.u32(r+4u),m.u32(r+8u)}));
                m.put8(w+0x41u,std::uint8_t(m.u8(w+0x41u)+1u));
                return;
            }
            call(c,0x4285a0u,{h});
            const std::uint32_t e=esi*0x54u;
            m.put32(0x8449ccu,0xffffffffu);
            m.put32(w+0x58u,m.u32(e+0x689df8u));
            m.put32(w+0x5cu,m.u32(0x84a24cu+(esi+m.u32(0x844944u)*2u)*4u));
            m.put32(w+0x60u,m.u32(0x689e58u+esi*4u));
            const std::int32_t k=std::int32_t(m.i8(w+0x19u))-2;
            std::uint32_t r=ebp;
            if(k==0)r=row(m.u32(e+0x689dc4u));
            else if(k==2)r=row(m.u32(e+0x689ddcu));
            m.put32(0x8449ccu,call(c,0x428460u,{m.u32(0x84a23cu),4u,0u,m.u32(r+4u),m.u32(r+8u)}));
            m.put8(w+0x41u,std::uint8_t(m.u8(w+0x41u)+1u));
            return;
        }
        if(st!=1u)return;
        std::uint8_t al=m.u8(w+0x19u);
        std::uint32_t h;
        if(al==0u||al==1u||al==3u){
            h=m.u32(0x8449ccu);
            if(std::int32_t(h)>=0){if(call(c,0x428880u,{h})!=3u)return;h=m.u32(0x8449ccu);}
            call(c,0x4285a0u,{h});call(c,0x4285a0u,{m.u32(0x8449d8u)});
            h=0xffffffffu;m.put32(0x8449ccu,h);m.put32(0x8449d8u,0xffffffffu);
        }else h=m.u32(0x8449ccu);
        const std::int8_t v=m.i8(w+0x3eu);
        switch(m.i8(w+0x19u)){
        case 0:if(v==0){m.put8(w+0x27u,0);}else if(v==1){m.put8(w+0x27u,1u);}m.put8(w+0x1fu,1u);break;
        case 1:if(v==0){m.put8(w+0x27u,0);}else if(v==1){m.put8(w+0x27u,1u);}m.put8(w+0x1fu,2u);break;
        case 2:
            call(c,0x4285a0u,{h});call(c,0x4285a0u,{m.u32(0x8449d8u)});
            m.put32(0x8449ccu,0xffffffffu);m.put32(0x8449d8u,0xffffffffu);
            if(m.i8(w+0x3eu)==0)m.put8(w+0x27u,0);else if(m.i8(w+0x3eu)==1)m.put8(w+0x27u,1u);
            m.put8(w+0x1fu,0);
            break;
        case 3:if(v==0){m.put8(w+0x27u,2u);}else if(v==1){m.put8(w+0x27u,3u);}m.put8(w+0x1fu,1u);break;
        case 4:
            call(c,0x4285a0u,{h});call(c,0x4285a0u,{m.u32(0x8449d8u)});
            m.put32(0x8449ccu,0xffffffffu);m.put32(0x8449d8u,0xffffffffu);
            if(m.i8(w+0x3eu)==0)m.put8(w+0x27u,2u);else if(m.i8(w+0x3eu)==1)m.put8(w+0x27u,3u);
            m.put8(w+0x1fu,0);
            break;
        default:break;
        }
        m.put8(w+0xau,m.u8(w+0xau)|1u);
        const std::uint16_t f=m.u16(w+8u)&3u;
        if(f==2u)m.put8(w+0x15u,std::uint8_t(m.u8(w+0x15u)+1u));
        else if(f==3u){
            al=std::uint8_t(m.u8(w+0x15u)-1u);
            m.put8(w+0x15u,al);
            if(m.u32(m.u32(w)+std::uint32_t(std::int32_t(std::int8_t(al)))*4u)==0xffffffffu)m.put8(w+0x15u,std::uint8_t(al-1u));
        }
        arcade_sequence_4c2ac0(c,m.u32(w),m.u8(w+0x15u));
        call(c,0x43f940u,{std::uint32_t(std::int32_t(m.i8(w+0x1fu)))});
        call(c,0x43f950u,{std::uint32_t(std::int32_t(m.i8(w+0x27u)))});
        switch(std::uint32_t(std::int32_t(m.i8(w+0x27u)))){
        case 0:call(c,0x44da00u,{0u,0u});break;
        case 1:call(c,0x44da00u,{1u,0u});break;
        case 2:call(c,0x44da00u,{5u,0u});break;
        case 3:call(c,0x44da00u,{6u,0u});break;
        default:break;
        }
        call(c,0x44c0b0u,{});
        return;
    }
    if(!(m.i16(w+0x10u)>0)&&cx==0u)m.put16(w+8u,std::uint16_t((ax&0xfffdu)|1u));
    std::uint32_t o=call(c,0x4035f0u,{});
    bool confirm=!(m.i32(o+0x518u)>0)&&call(c,0x4536f0u,{8u});
    if(!confirm)confirm=result_input_4bfb20(c)!=0;
    if(!confirm)confirm=(m.u8(w+8u)&3u)==1u;
    if(confirm){
        m.put8(w+0x23u,0xffu);
        const std::uint32_t e=esi*0x54u;
        std::uint32_t snd;
        switch(std::uint32_t(std::int32_t(m.i8(w+0x19u)))){
        case 0:ebp=row(m.u32(e+0x689de4u));snd=m.u32(m.u32(0x6898d8u)+0x3cu);break;
        case 1:ebp=row(m.u32(e+0x689de8u));snd=m.u32(m.u32(0x6898d8u)+0x40u);break;
        case 2:snd=m.u32(m.u32(0x6898d8u)+0x44u);ebp=row(m.u32(e+0x689decu));break;
        case 3:ebp=row(m.u32(e+0x689df0u));snd=m.u32(m.u32(0x6898d8u)+0x3cu);break;
        case 4:ebp=row(m.u32(e+0x689df4u));snd=m.u32(m.u32(0x6898d8u)+0x44u);break;
        default:snd=w;break;
        }
        call(c,0x4285a0u,{m.u32(0x8449ccu)});call(c,0x4285a0u,{m.u32(0x8449d8u)});call(c,0x4285a0u,{m.u32(0x8449c4u)});
        m.put32(0x8449ccu,call(c,0x428460u,{m.u32(0x84a24cu+(esi+m.u32(0x844944u)*2u)*4u),4u,1u,m.u32(ebp+4u),m.u32(ebp+8u)}));
        m.put32(0x8449d8u,call(c,0x428460u,{m.u32(0x689e58u+esi*4u),9u,1u,m.u32(ebp+4u),m.u32(ebp+8u)}));
        call(c,0x4493c0u,{});
        call(c,0x424940u,{0x40u});
        o=call(c,0x4035f0u,{});
        if(!(m.i32(o+0x518u)>0)&&call(c,0x4536f0u,{8u})){
            m.put8(w+8u,m.u8(w+8u)|3u);
            m.put8(w+0x22u,0xffu);m.put8(w+0x1fu,0xffu);m.put8(w+0x27u,0xffu);
            m.put16(w+0x10u,0);
            return;
        }
        arcade_sound_4bee60(c,snd);
        m.put16(w+0x10u,0);
        m.put16(w+8u,std::uint16_t((m.u16(w+8u)&0xfffeu)|2u));
        return;
    }
    const std::uint8_t busy=m.u8(0x84a293u);
    m.put8(w+0x18u,0);
    if(busy==0u){
        arcade_scroll_4c0eb0(c,w,std::uint32_t(std::int32_t(m.i8(w+0x40u))),0);
        o=call(c,0x4035f0u,{});
        if(!(m.i32(o+0x518u)>0)&&call(c,0x4536f0u,{0x40000u})&&m.u8(0x84a292u)==0u){
            const std::uint8_t cl=std::uint8_t(m.u8(w+0x40u)^1u);
            const std::uint8_t dl=std::uint8_t((cl==0u?1u:0u)+3u);
            m.put8(0x84a292u,9u);m.put8(w+0x40u,cl);m.put8(w+0x18u,dl);
        }
    }else if(m.i32(0x8449c4u)>=0&&call(c,0x428880u,{m.u32(0x8449c4u)})==3u){
        call(c,0x4285a0u,{m.u32(0x8449c4u)});
        const std::int32_t y=cvtt(m.f32(0x84a284u)),x=cvtt(m.f32(0x84a280u));
        const std::uint32_t h=call(c,0x428460u,{m.u32(0x84a298u),7u,2u,std::uint32_t(x),std::uint32_t(y)});
        m.put32(0x8449c4u,h);
        if(m.u8(w+0x48u)==0u)call(c,0x4288c0u,{h,std::uint32_t(cvtt(m.f32(0x84a270u)+m.f32(0x84a280u)))});
        else{float v=m.f32(0x84a294u)+m.f32(0x84a270u);v=v+m.f32(0x84a280u);call(c,0x4288c0u,{h,std::uint32_t(cvtt(v))});}
        arcade_4bf400(c,w);
        m.put8(0x84a293u,0);
    }
    const std::uint8_t al=m.u8(w+0x18u);
    if(al==0u)return;
    const std::uint32_t k=std::uint32_t(std::int32_t(std::int8_t(al)))-1u;
    if(k>3u)return;
    if(k==0u||k==1u){
        call(c,0x4285a0u,{m.u32(0x8449ccu)});
        static constexpr std::uint32_t A[5]{0x689da8u,0x689db4u,0x689dc0u,0x689dccu,0x689dd8u};
        static constexpr std::uint32_t B[5]{0x689db0u,0x689dbcu,0x689dc8u,0x689dd4u,0x689de0u};
        const std::uint32_t sel=std::uint32_t(std::int32_t(m.i8(w+0x19u)));
        if(sel<=4u)ebp=row(m.u32((k==0u?A[sel]:B[sel])+esi*0x54u));
        std::uint32_t h;
        if(k==0u)h=call(c,0x428460u,{m.u32(0x84a23cu),4u,1u,m.u32(ebp+4u),m.u32(ebp+8u)});
        else h=call(c,0x428460u,{m.u32(0x84a23cu),4u,1u,m.u32(ebp+8u),m.u32(ebp+4u)});
        m.put32(0x8449ccu,h);
        call(c,0x428800u,{h,k==0u?0x3f800000u:0xbf800000u});
        call(c,0x4493c0u,{});
        call(c,0x424940u,{1u});
        return;
    }
    const bool right=k==2u;
    m.put8(w+0x3eu,right?1u:0u);
    call(c,0x4285a0u,{m.u32(0x8449ccu)});call(c,0x4285a0u,{m.u32(0x8449d8u)});call(c,0x4285a0u,{m.u32(0x8449c4u)});
    const std::uint32_t a=right?0x456u:0x473u,b=right?0x473u:0x456u;
    m.put32(0x8449ccu,call(c,0x428460u,{m.u32(0x84a23cu),4u,1u,a,b}));
    m.put32(0x8449d8u,call(c,0x428460u,{0x2e0024u,9u,1u,a,b}));
    m.put32(0x8449c4u,call(c,0x428460u,{cursor(),7u,1u,0x456u,0x473u}));
    const std::uint32_t s=right?0x3f800000u:0xbf800000u;
    call(c,0x428800u,{m.u32(0x8449ccu),s});
    call(c,0x428800u,{m.u32(0x8449d8u),s});
    call(c,0x4493c0u,{});
    call(c,0x424940u,{1u});
    m.put8(0x84a293u,1u);
}
// 4C4880 (function 9 control): the music list (5C8D00 records; +48 / +4A pick the page, the
// row frames +8 / +C / +10, the 2E0027 page switch 0x30C / 0x31F or 0x5F0 / 0x603, the track
// 844A00 for 4BF970), confirm stores +20 (sound 6898D8 + 0xAC), then +4B: the 5C7170 row 0x16F
// / 0x178 banner, the network wait (+3C 0..3: 2E004B, 45AA50) for more players, and the
// sequence on (+15 +- 2, music 0x1E); every frame the gear switch 4C0340, 48B1B0(+21) and the
// sound preview 427B10.
void arcade_fn9_ctrl_4c4880(PcRaceContext& c,std::uint32_t w){
    auto& m=c.m;
    auto music=[&]{
        const std::uint32_t k=m.u8(w+0x48u)==0u?std::uint32_t(std::int32_t(m.i8(w+0x19u)))
            :std::uint32_t(std::int32_t(m.i8(w+0x19u))+std::int32_t(m.i8(0x84a291u)));
        return m.u32(0x5c8d00u+k*4u);};
    auto tail=[&]{
        arcade_gear_4c0340(c,w);
        call(c,0x48b1b0u,{m.u8(w+0x21u)});
        const bool held=m.u32(0x84a238u)&&(m.u8(w+8u)&3u)<=1u;
        const std::uint32_t snd=call(c,0x48b1c0u,{});
        call(c,0x427b10u,{snd,m.u32(0x8449fcu),held?1u:0u});
    };
    if((m.u8(w+8u)&3u)<=1u&&m.i32(0x8449ccu)>=0&&call(c,0x428880u,{m.u32(0x8449ccu)})==3u){
        call(c,0x4285a0u,{m.u32(0x8449ccu)});
        const std::uint32_t h=create_row(c,row(m.u32(music()+0xcu)),4u,0u);
        m.put32(0x8449ccu,h);
        call(c,0x428800u,{h,0x3f800000u});
        if(m.u32(0x844958u)){
            call(c,0x4285a0u,{m.u32(0x8449c4u)});
            const std::int32_t y=cvtt(m.f32(0x84a284u)),x=cvtt(m.f32(0x84a280u));
            m.put32(0x8449c4u,call(c,0x428460u,{0x2e0028u,7u,2u,std::uint32_t(x),std::uint32_t(y)}));
        }
        m.put32(0x8449e8u,1u);m.put32(0x844958u,0);
    }
    m.put32(0x8449bcu,0xffffffffu);m.put32(0x8449c0u,0xffffffffu);
    arcade_music_4bf970(c);
    if(m.u32(0x844958u))return;
    if((m.u8(w+8u)&3u)>1u){
        const std::uint8_t st=m.u8(w+0x4bu);
        if(st==0u){
            std::uint32_t h=m.u32(0x8449c4u);
            if(std::int32_t(h)>=0){if(call(c,0x428880u,{h})!=3u)return;h=m.u32(0x8449c4u);}
            m.put32(0x8449bcu,0xffffffffu);m.put32(0x8449c0u,0xffffffffu);m.put16(0x84494cu,1u);
            call(c,0x4285a0u,{h});
            m.put32(0x8449c4u,0xffffffffu);
            const std::uint32_t idx=m.i8(w+0x19u)>=8?0x178u:0x16fu;
            m.put32(0x8449c4u,create_row(c,row(idx),4u,1u));
            m.put8(w+0x4bu,std::uint8_t(m.u8(w+0x4bu)+1u));
            tail();
            return;
        }
        if(st!=1u){tail();return;}
        std::uint32_t h=m.u32(0x8449c4u);
        if(std::int32_t(h)>=0){
            if(call(c,0x428880u,{h})!=3u)return;
            call(c,0x4285a0u,{m.u32(0x8449c4u)});
            m.put32(0x8449c4u,0xffffffffu);
        }
        if((call(c,0x456d60u,{})&0xffu)>1u){
            call(c,0x48b1d0u,{m.u8(w+0x20u)});
            if(m.u32(0x84a2acu)){
                m.put32(0x84a2acu,0);
                std::uint32_t edi=0;
                if(call(c,0x456d60u,{})&0xffu){
                    for(;;){
                        if(edi!=(call(c,0x455ad0u,{})&0xffu)){
                            if((call(c,0x48f970u,{edi})&0xffu)==0xffu)break;
                        }
                        if(edi==(call(c,0x456d60u,{})&0xffu)-1u){m.put32(w+0x3cu,2u);break;}
                        ++edi;
                        if(!(edi<(call(c,0x456d60u,{})&0xffu)))break;
                    }
                }
            }
            switch(m.u32(w+0x3cu)){
            case 0:
                m.put32(w+0x40u,call(c,0x428460u,{0x2e004bu,0xcu,1u,0u,0x1eu}));
                m.put32(w+0x3cu,m.u32(w+0x3cu)+1u);
                return;
            case 1:
                if(m.u32(w+0x44u)==0x1eu){
                    m.put32(w+0x3cu,m.u32(w+0x3cu)+1u);
                    call(c,0x4285a0u,{m.u32(w+0x40u)});
                    m.put32(w+0x40u,call(c,0x428460u,{0x2e004bu,0xcu,0u,0x1fu,0x3cu}));
                    m.put32(w+0x44u,0);
                }
                m.put32(w+0x44u,m.u32(w+0x44u)+1u);
                return;
            case 2:
                if(!call(c,0x45aa50u,{}))return;
                m.put32(w+0x44u,0);m.put32(w+0x3cu,m.u32(w+0x3cu)+1u);
                return;
            case 3:{
                const std::uint32_t t=m.u32(w+0x44u);
                if(t==0x1eu){m.put32(w+0x3cu,m.u32(w+0x3cu)+1u);m.put32(w+0x40u,0xffffffffu);}
                m.put32(w+0x44u,t+1u);
                return;}
            default:break;
            }
        }else{
            call(c,0x4285a0u,{m.u32(0x8449ccu)});
            m.put32(0x8449ccu,0xffffffffu);
        }
        m.put8(w+0xau,m.u8(w+0xau)|1u);
        std::uint8_t al=m.u8(w+0x15u);
        if((m.u8(w+8u)&3u)==3u){m.put8(w+0x20u,0xffu);m.put8(w+0x23u,0xffu);al=std::uint8_t(al-2u);}
        else al=std::uint8_t(al+2u);
        m.put8(w+0x15u,al);
        m.put32(0x8449fcu,0x1eu);
        call(c,0x401000u,{0u,0x1eu,1u});
        arcade_sequence_4c2ac0(c,m.u32(w),al);
        tail();
        return;
    }
    arcade_banner_4c2430(c);
    const std::uint8_t busy=m.u8(0x84a293u);
    m.put8(w+0x18u,0);
    if(busy==0u){
        arcade_scroll_4c0eb0(c,w,std::uint32_t(std::int32_t(m.i8(w+0x4au))),1u);
        const std::uint32_t o=call(c,0x4035f0u,{});
        if(!(m.i32(o+0x518u)>0)&&call(c,0x4536f0u,{0x40000u})&&m.u8(0x84a292u)==0u){
            const std::uint8_t al=std::uint8_t(m.u8(w+0x4au)^1u);
            m.put8(0x84a292u,9u);m.put8(w+0x4au,al);
            if(al==0u){m.put8(w+0x18u,4u);m.put8(w+0x48u,0);}
            else{m.put8(w+0x18u,3u);m.put8(w+0x48u,1u);}
        }
    }else if(m.i32(0x8449c4u)>=0&&call(c,0x428880u,{m.u32(0x8449c4u)})==3u){
        call(c,0x4285a0u,{m.u32(0x8449c4u)});
        const std::int32_t y=cvtt(m.f32(0x84a284u)),x=cvtt(m.f32(0x84a280u));
        const std::uint32_t h=call(c,0x428460u,{m.u32(0x84a298u),7u,2u,std::uint32_t(x),std::uint32_t(y)});
        m.put32(0x8449c4u,h);
        if(m.u8(w+0x48u)==0u)call(c,0x4288c0u,{h,std::uint32_t(cvtt(m.f32(0x84a270u)+m.f32(0x84a280u)))});
        else{float v=m.f32(0x84a294u)+m.f32(0x84a270u);v=v+m.f32(0x84a280u);call(c,0x4288c0u,{h,std::uint32_t(cvtt(v))});}
        m.put8(0x84a293u,0);
    }
    const std::uint8_t al=m.u8(w+0x18u);
    if(al){
        const std::uint32_t k=std::uint32_t(std::int32_t(std::int8_t(al)))-1u;
        if(k<=1u){
            const std::uint32_t r=row(m.u32(music()+8u));
            call(c,0x4285a0u,{m.u32(0x8449ccu)});
            const std::uint32_t h=create_row(c,r,4u,1u);
            m.put32(0x8449ccu,h);
            call(c,0x428800u,{h,0x3f800000u});
        }else if(k==2u||k==3u){
            const std::uint32_t a=k==2u?0x30cu:0x5f0u,b=k==2u?0x31fu:0x603u;
            call(c,0x4285a0u,{m.u32(0x8449c4u)});call(c,0x4285a0u,{m.u32(0x8449ccu)});
            m.put32(0x8449c4u,call(c,0x428460u,{m.u32(0x84a298u),7u,1u,a,b}));
            m.put32(0x8449ccu,call(c,0x428460u,{0x2e0027u,4u,1u,a,b}));
            call(c,0x428800u,{m.u32(0x8449c4u),0x3f800000u});
            if(k==2u)call(c,0x428800u,{m.u32(0x8449ccu),0x3f800000u});
            m.put8(0x84a293u,1u);m.put32(0x8449e8u,0);
        }
        m.put32(0x844a00u,m.u32(music()+4u));
        call(c,0x4493c0u,{});
        call(c,0x424940u,{3u});
    }
    if(!(m.i16(w+0x10u)>0)){const std::uint16_t f=m.u16(w+8u);if(!(f&3u))m.put16(w+8u,std::uint16_t((f&0xfffdu)|1u));}
    std::uint32_t o=call(c,0x4035f0u,{});
    bool confirm=!(m.i32(o+0x518u)>0)&&call(c,0x4536f0u,{8u});
    if(!confirm)confirm=result_input_4bfb20(c)!=0;
    if(!confirm)confirm=(m.u8(w+8u)&3u)==1u;
    if(confirm){
        call(c,0x4285a0u,{m.u32(0x8449c4u)});call(c,0x4285a0u,{m.u32(0x8449ccu)});
        m.put32(0x8449ccu,0xffffffffu);
        const std::uint8_t a=m.u8(w+0x19u);
        std::uint32_t p;
        if(m.u8(w+0x48u)==0u){m.put8(w+0x20u,a);p=m.u32(0x5c8d00u+std::uint32_t(std::int32_t(std::int8_t(a)))*4u);}
        else{
            const std::uint8_t cl=m.u8(0x84a291u);
            m.put8(w+0x20u,std::uint8_t(a+cl));
            p=m.u32(0x5c8d00u+std::uint32_t(std::int32_t(std::int8_t(a))+std::int32_t(std::int8_t(cl)))*4u);
        }
        m.put32(0x8449c4u,create_row(c,row(m.u32(p+0x10u)),4u,1u));
        m.put32(0x8449e8u,0);
        o=call(c,0x4035f0u,{});
        if(!(m.i32(o+0x518u)>0)&&call(c,0x4536f0u,{8u})){m.put8(w+8u,m.u8(w+8u)|3u);m.put32(w+0x54u,0);}
        else{
            arcade_sound_4bee60(c,m.u32(m.u32(0x6898d8u)+std::uint32_t(std::int32_t(m.i8(w+0x20u)))*4u+0xacu));
            m.put16(w+8u,std::uint16_t((m.u16(w+8u)&0xfffeu)|2u));
        }
        m.put16(w+0x10u,0);
        call(c,0x4493c0u,{});
        call(c,0x424940u,{0x40u});
        call(c,0x43f950u,{std::uint32_t(std::int32_t(m.i8(w+0x27u)))});
    }
    tail();
}
// 4BF3F0 (function 1 display): 40EC60(0).
void arcade_fn1_disp_4bf3f0(PcRaceContext& c,std::uint32_t){call(c,0x40ec60u,{0u});}
// The event-4 callbacks of functions 1..0xC (59BE78 rows 1..12); 49A650 (RET) is accepted.
bool arcade_event4_invoke(PcRaceContext& c,std::uint32_t callback,std::uint32_t w){
    switch(callback){
    case 0x4bfe00u:arcade_fn1_init_4bfe00(c,w);return true;
    case 0x4c3510u:arcade_fn1_ctrl_4c3510(c,w);return true;
    case 0x4bf3f0u:arcade_fn1_disp_4bf3f0(c,w);return true;
    case 0x4bff80u:arcade_fn2_init_4bff80(c,w);return true;
    case 0x4c3530u:arcade_fn2_ctrl_4c3530(c,w);return true;
    case 0x4c1fa0u:arcade_fn2_disp_4c1fa0(c,w);return true;
    case 0x4c2240u:arcade_fn3_init_4c2240(c,w);return true;
    case 0x4c3960u:arcade_fn3_ctrl_4c3960(c,w);return true;
    case 0x4c2420u:arcade_disp_4c2420(c,w);return true;
    case 0x4c0060u:arcade_fn4_init_4c0060(c,w);return true;
    case 0x4c0090u:arcade_fn4_ctrl_4c0090(c,w);return true;
    case 0x4c01e0u:arcade_fn5_init_4c01e0(c,w);return true;
    case 0x4c3dc0u:arcade_fn5_ctrl_4c3dc0(c,w);return true;
    case 0x4c22c0u:arcade_fn5_disp_4c22c0(c,w);return true;
    case 0x4bf4d0u:arcade_slots_4bf4d0(c);return true;
    case 0x4c18c0u:arcade_fn6_init_4c18c0(c,w);return true;
    case 0x4c2d20u:arcade_fn6_ctrl_4c2d20(c,w);return true;
    case 0x4c1e80u:arcade_fn6_disp_4c1e80(c,w);return true;
    case 0x4bfdb0u:arcade_fn6_dest_4bfdb0(c);return true;
    case 0x4c1f10u:arcade_fn7_init_4c1f10(c,w);return true;
    case 0x4c30b0u:arcade_fn7_ctrl_4c30b0(c,w);return true;
    case 0x4c02b0u:arcade_fn8_init_4c02b0(c,w);return true;
    case 0x4c02e0u:arcade_fn8_ctrl_4c02e0(c,w);return true;
    case 0x4c04d0u:arcade_fn9_init_4c04d0(c,w);return true;
    case 0x4c4880u:arcade_fn9_ctrl_4c4880(c,w);return true;
    case 0x4c22e0u:arcade_fn9_disp_4c22e0(c,w);return true;
    case 0x4c2330u:arcade_fn10_init_4c2330(c,w);return true;
    case 0x4c26d0u:arcade_fn10_ctrl_4c26d0(c,w);return true;
    case 0x4c05f0u:arcade_fn11_init_4c05f0(c,w);return true;
    case 0x4bf500u:arcade_fn11_ctrl_4bf500(c,w);return true;
    case 0x4bf510u:arcade_fn11_dest_4bf510(c,w);return true;
    case 0x4bfb90u:arcade_fn12_init_4bfb90(c,w);return true;
    case 0x4c2b70u:arcade_fn12_ctrl_4c2b70(c,w);return true;
    case 0x4bee80u:arcade_4bee80(c);return true;
    case 0x49a650u:return true;   // 49A650: ret (the empty event callback)
    default:return false;
    }
}

// ---- the OUTRUN2SP entry module (4B6E30..4B8DD2): its globals 842880 / 842884 and the
// network wait banner block 84288C..8428B7 (handles 84288C / 842890 / 8428B0, frames 842894,
// music 842898, flags 84289C, player lamps 8428A0..8428AC, stage 8428B4). -----------------------
namespace {
// 4B6EE0(al): the selected entry 842884, cleared (with 842880) outside 0 .. [84284C]-1.
void entry_select_4b6ee0(PcRaceContext& c,std::uint8_t al){
    auto& m=c.m;
    m.put8(0x842884u,al);
    if(std::int32_t(std::int8_t(al))>=m.i32(0x84284cu)){m.put8(0x842884u,0);m.put32(0x842880u,0);return;}
    if(std::int8_t(al)<0)m.put8(0x842884u,0);
    m.put32(0x842880u,0);
}
// The 687E24 x of player slot i among n players (rows of 4 from n = 2).
float lamp_x(PcRaceMemory& m,std::uint32_t i,std::uint32_t n){
    return float(m.i32(0x687e24u+(i+n*4u-8u)*4u));
}
// 4B88D0: the wait banner 2E0046, the clock 2E0054, the local player's lamp 2E004A at its slot.
void entry_wait_init_4b88d0(PcRaceContext& c){
    auto& m=c.m;
    m.put32(0x84288cu,call(c,0x428460u,{0x2e0046u,2u,0u,0u,0u}));
    m.put32(0x8428b0u,call(c,0x428460u,{0x2e0054u,3u,0u,0x1eu,0x3bu}));
    m.put32(0x842894u,0);
    if(m.u32(0x842898u)!=0x1fu)m.put32(0x842898u,0xffffffffu);
    for(std::uint32_t a=0x84289cu;a<=0x8428acu;a+=4)m.put32(a,0);
    const std::uint32_t esi=call(c,0x455c10u,{})-1u;
    const std::uint32_t ebx=call(c,0x455c20u,{});
    const std::uint32_t f=esi*0x3cu;
    m.put32(0x842890u,call(c,0x428460u,{0x2e004au,6u,0u,f,f+0x3bu}));
    Locals l(m);
    translation(m,PcRaceEndLocals,lamp_x(m,esi,ebx),340.0f,0.0f);
    call(c,0x4287b0u,{m.u32(0x842890u),PcRaceEndLocals});
}
// 4B89B0: releases the three handles.
void entry_wait_release_4b89b0(PcRaceContext& c){
    auto& m=c.m;
    call(c,0x4285a0u,{m.u32(0x84288cu)});
    call(c,0x4285a0u,{m.u32(0x8428b0u)});
    call(c,0x4285a0u,{m.u32(0x842890u)});
}
// 4B89E0: ax = OR of 1 << 455B00(i) over the 456D60 players (eax upper half: the last callee's).
std::uint32_t entry_players_4b89e0(PcRaceContext& c){
    std::uint32_t eax=call(c,0x456d60u,{});
    const std::uint32_t n=eax&0xffu;
    std::uint32_t bx=0;
    for(std::uint32_t i=0;i<n;++i){
        eax=call(c,0x455b00u,{i});
        bx|=1u<<(eax&31u);
    }
    if(n==0u)eax&=0xffff0000u;   // movzx ax,al left ah = 0
    return (eax&0xffff0000u)|(bx&0xffffu);
}
// 4B8C00 (display through 4BEE80): lamps of the joined players, the wait music 0x1F, the
// remaining time 455AB0 as two digits (tens of seconds / seconds at 60 frames), the clock
// 2E0054 restarted once more than one player is in, the other players' lamps (2E0047 lit with
// a 60-frame phase, 2E0048 off), then frames + 1.
void entry_wait_display_4b8c00(PcRaceContext& c){
    auto& m=c.m;
    std::int32_t esi=std::int16_t(call(c,0x455ab0u,{}));
    const std::uint32_t ebx=call(c,0x456d60u,{})&0xffu;
    const std::int32_t edi=std::int32_t(call(c,0x455c20u,{}));
    const std::int32_t local=std::int32_t(call(c,0x455c10u,{})-1u);
    const std::uint32_t al=entry_players_4b89e0(c);
    for(std::uint32_t b=0;b<4;++b)if(al&(1u<<b))m.put32(0x8428a0u+b*4u,1u);
    if(m.u32(0x842894u)==0u&&m.u32(0x842898u)==0xffffffffu){
        m.put32(0x842898u,0x1fu);
        call(c,0x401000u,{0u,0x1fu,1u});
    }
    if(esi==0){
        if(m.u32(0x842898u)!=0xffffffffu){call(c,0x401030u,{0u});m.put32(0x842898u,0xffffffffu);}
    }else if(esi<0)esi=0;
    call(c,0x429530u,{0x2e000du,0x43960000u,0x43670000u,5u,std::uint32_t(esi/600%10)});
    call(c,0x429530u,{0x2e000du,0x43ac8000u,0x43670000u,5u,std::uint32_t(esi/60%10)});
    if(ebx>1u&&m.u32(0x84289cu)==0u){
        m.put32(0x84289cu,1u);
        call(c,0x4285a0u,{m.u32(0x8428b0u)});
        m.put32(0x8428b0u,call(c,0x428460u,{0x2e0054u,3u,1u,0u,0u}));
    }
    for(std::int32_t i=0;i<edi;++i){
        const float x=lamp_x(m,std::uint32_t(i),std::uint32_t(edi));
        if(m.u32(0x8428a0u+std::uint32_t(i)*4u)){
            if(i==local)continue;
            const std::int32_t d=m.i32(0x842894u)%0x3c+i*0x3c;
            call(c,0x429530u,{0x2e0047u,fb(x),0x43aa0000u,5u,std::uint32_t(d)});
        }else call(c,0x429530u,{0x2e0048u,fb(x),0x43aa0000u,5u,std::uint32_t(i)});
    }
    m.put32(0x842894u,m.u32(0x842894u)+1u);
}
}
bool arcade_entry_invoke(PcRaceContext& c,const PcRaceCall& k,std::uint32_t& eax){
    switch(k.pc){
    case 0x4b6ee0u:entry_select_4b6ee0(c,std::uint8_t(k.args[0]));eax=0;return true;
    case 0x4b88d0u:entry_wait_init_4b88d0(c);eax=0;return true;
    case 0x4b89b0u:entry_wait_release_4b89b0(c);eax=0;return true;
    case 0x4b89e0u:eax=entry_players_4b89e0(c);return true;
    case 0x4b8c00u:entry_wait_display_4b8c00(c);eax=c.m.u32(0x842894u);return true;
    default:return false;
    }
}
}
