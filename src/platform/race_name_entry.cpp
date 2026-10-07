#include "platform/race_name_entry.hpp"
#include "platform/sp_rankings.hpp"
#include <array>
#include <cstdio>
#include <cstring>
#include <string>
namespace outrun::platform {
namespace {
std::uint32_t call(PcRaceContext& c,std::uint32_t pc,std::initializer_list<std::uint32_t> args){
    PcRaceCall k;k.pc=pc;k.argc=std::uint32_t(args.size());
    std::size_t i=0;for(auto a:args)k.args[i++]=a;
    return c.service(k);
}
std::uint32_t fb(float f){std::uint32_t v;std::memcpy(&v,&f,4);return v;}
std::int32_t cvtt(float f){
    if(!(f>-2147483904.0f&&f<2147483648.0f))return std::int32_t(0x80000000u);
    return std::int32_t(f);
}
// The PC stack buffers passed by address: one page mapped for the call.
struct Locals {
    PcRaceMemory& m;std::size_t mark;std::array<std::uint8_t,0x100> bytes{};
    static constexpr std::uint32_t Text=PcNameEntryLocals,Matrix=PcNameEntryLocals+0x40u,
        Record=PcNameEntryLocals+0x80u,Rank=PcNameEntryLocals+0x90u;
    explicit Locals(PcRaceMemory& mem):m(mem),mark(mem.mark()){m.map(PcNameEntryLocals,bytes.data(),bytes.size());}
    ~Locals(){m.release(mark);}
};
// D3DXMatrixTranslation(out, x, y, z).
void translation(PcRaceMemory& m,std::uint32_t out,float x,float y,float z){
    for(std::uint32_t k=0;k<16;++k)m.putf(out+k*4,(k%5u)==0u?1.0f:0.0f);
    m.putf(out+0x30,x);m.putf(out+0x34,y);m.putf(out+0x38,z);
}
std::string cstring(PcRaceMemory& m,std::uint32_t a){std::string s;for(;;++a){const auto ch=m.u8(a);if(!ch)break;s+=char(ch);}return s;}
void put_string(PcRaceMemory& m,std::uint32_t a,const std::string& s){for(std::size_t i=0;i<s.size();++i)m.put8(a+std::uint32_t(i),std::uint8_t(s[i]));m.put8(a+std::uint32_t(s.size()),0);}
// strstr(set, one character string): the empty string (the terminator) is always found.
bool in_set(const std::string& set,char ch){return ch==0||set.find(ch)!=std::string::npos;}
// 4B07B0(buffer, set): the characters of set removed.
std::string strip_4b07b0(const std::string& s,const std::string& set){
    std::string out;for(char ch:s)if(!in_set(set,ch))out+=ch;return out;
}
// 4B0830(EAX = buffer, set): the leading characters of set removed.
std::string trim_lead_4b0830(const std::string& s,const std::string& set){
    std::size_t i=0;while(i<s.size()&&in_set(set,s[i]))++i;return s.substr(i);
}
// 4B08C0(buffer, set): the trailing characters of set removed.
std::string trim_trail_4b08c0(const std::string& s,const std::string& set){
    std::size_t n=s.size();while(n>0&&in_set(set,s[n-1]))--n;return s.substr(0,n);
}
// __ascii_stricmp (58DD1E in the C locale).
bool same_nocase(const std::string& a,const std::string& b){
    if(a.size()!=b.size())return false;
    auto low=[](char ch){return (ch>='A'&&ch<='Z')?char(ch-'A'+'a'):ch;};
    for(std::size_t i=0;i<a.size();++i)if(low(a[i])!=low(b[i]))return false;
    return true;
}
// 5802DD sprintf of the integer formats used here.
std::string format_int(std::uint32_t format,std::int32_t v){
    const char* f=nullptr;
    switch(format){
    case 0x5c212cu:f="%2d";break;
    case 0x5c2120u:f="%02d";break;
    case 0x5c2130u:f="%d";break;
    case 0x5b0334u:f="%03d";break;
    default:throw PcRaceEndUnreachable{format};
    }
    char out[32];std::snprintf(out,sizeof out,f,v);return out;
}
// 581780 (strncpy(dst, src, 4)) as a word: the bytes up to the terminator, zeros after it.
std::uint32_t strncpy4(std::uint32_t v){
    std::uint32_t out=0;
    for(unsigned k=0;k<4u;++k){const std::uint32_t b=(v>>(k*8))&0xffu;if(!b)break;out|=b<<(k*8);}
    return out;
}
// The 8427E0 / 842720 bit counts: 4B298C / 4B29B0 jump tables over 4B29A0 (route 0..15).
std::uint32_t route_bits(std::int32_t route){
    if(std::uint32_t(route)>0xfu)return 5u;
    std::uint32_t n=0;for(std::uint32_t r=std::uint32_t(route);r;r&=r-1u)++n;return n;
}
// 449AC0(hours, minutes, seconds, ms, time) as race_end_modes' time words: the minutes,
// seconds and milliseconds this module prints.
struct TimeWords { std::uint16_t minutes,seconds,ms; };
TimeWords time_words(std::uint32_t t){
    const std::uint32_t s=t/1000u;const std::uint16_t ms=std::uint16_t(t-s*1000u);
    const std::uint32_t h=s/3600u;const std::uint16_t hours=std::uint16_t(h);
    const std::uint32_t mi=s/60u-h*3600u;
    const std::uint16_t seconds=std::uint16_t(mi*0xffffffc4u-std::uint32_t(std::uint16_t(hours*0xe10u))+s);
    return {std::uint16_t(mi),seconds,ms};
}
// 4B2080 / 4B2150 / 4B2220 (EAX = 8423C0, EBX = count): the ten rows of the ranking table,
// 0x48 bytes each: +0 = 1, +4 = i, +8 = +0 & FFFFFF, +C = +4, +10 = +8 & FFF, +14 = the name
// (strncpy, 4 bytes), +18 = 0, +1A = +0 >> 28, +1B / +1C = +8 bits 17 / 18, +20 = +0 bits 24..27,
// +3C = +40 = -1, +44 = 1 / 2 / 0.
void rows(PcRaceContext& c,std::uint32_t kind,std::uint32_t count,std::uint32_t preset,std::uint32_t a,std::uint32_t b){
    auto& m=c.m;
    for(std::uint32_t i=0;i<10u;++i){
        const std::uint32_t rec=kind==1u?sp_record_4f3810(preset,count,i):kind==2u?sp_record_4f3910(preset,count,i):sp_record_4f3870(preset,a,b,count,i);
        const std::uint32_t r0=m.u32(rec),r4=m.u32(rec+4u),r8=m.u32(rec+8u),rc=m.u32(rec+0xcu);
        const std::uint32_t e=0x8423c0u+i*0x48u;
        m.put32(e+8u,r0&0xffffffu);m.put32(e+0x10u,r8&0xfffu);m.put32(e,1u);m.put32(e+4u,i);
        m.put32(e+0x3cu,0xffffffffu);m.put32(e+0x40u,0xffffffffu);
        m.put32(e+0x14u,strncpy4(rc));
        m.put32(e+0x20u,(r0>>24)&0xfu);m.put8(e+0x1au,std::uint8_t((r0>>28)&0xfu));
        m.put8(e+0x18u,0);m.put32(e+0xcu,r4);
        m.put8(e+0x1bu,std::uint8_t((r8>>17)&1u));m.put8(e+0x1cu,std::uint8_t((r8>>18)&1u));
        m.put32(e+0x44u,kind==0u?0u:kind);
    }
}
// 4B22F0(EAX = style, ECX = text, x, y, layer): one 429530 digit per decimal character of
// text, every character advancing x by [5C6348 + style*8]; the token is [5C6344 + style*8]
// (the protected VM entry 4B22F1: mov ebx,[eax*8+5C6344]).
void digits_4b22f0(PcRaceContext& c,std::uint32_t style,const std::string& text,std::int32_t x,std::int32_t y,std::uint32_t layer){
    auto& m=c.m;
    const std::uint32_t token=m.u32(0x5c6344u+style*8u);const std::int32_t step=m.i32(0x5c6348u+style*8u);
    std::int32_t at=0;
    for(char ch:text){
        if(ch>='0'&&ch<='9'){
            const float px=float(at)+float(x);
            call(c,0x429530u,{token,fb(px),fb(float(y)),layer,std::uint32_t(ch-'0')});
        }
        at+=step;
    }
}
// 4B1F40(dst, src, n): at most n characters of src, those outside the 5C62E0 set (0x5B
// characters) as '-'; the terminator copied when src ends first.
void filter_4b1f40(PcRaceMemory& m,std::uint32_t dst,std::uint32_t src,std::uint32_t n){
    for(std::uint32_t i=0;i<n;++i){
        const std::uint8_t ch=m.u8(src+i);
        if(!ch){m.put8(dst+i,0);return;}
        bool known=false;for(std::uint32_t k=0;k<0x5bu&&!known;++k)known=m.u8(0x5c62e0u+k)==ch;
        m.put8(dst+i,known?ch:std::uint8_t('-'));
    }
}
// 4B17E0(x, y, layer, route, token): the route sprite at (x, y) and, unless the token is
// 360009 / 3D0011, its five goal marks (frames 4B1680(route, level) + 0x20 below 0x2F).
void route_marks_4b17e0(PcRaceContext& c,std::int32_t x,std::int32_t y,std::uint32_t layer,std::uint32_t route,std::uint32_t token){
    auto& m=c.m;Locals l(m);
    translation(m,Locals::Matrix,float(x),float(y),0.0f);
    call(c,0x4289b0u,{token,layer,Locals::Matrix,0u});
    if(token==0x360009u||token==0x3d0011u)return;
    for(std::uint32_t level=0;level<=4u;++level){
        const std::int32_t g=std::int32_t(goal_index_4b1680(route,level));
        if(g<0xf&&g>=0)call(c,0x4289b0u,{token,layer+1u,Locals::Matrix,std::uint32_t(g+0x20)});
    }
}
// 4B1FA0(EAX = x, ESI = y, row): the time box 3D001A and the row's time +C as
// minutes'seconds"ms in font 2.
void time_4b1fa0(PcRaceContext& c,std::int32_t x,std::int32_t y,std::uint32_t row){
    auto& m=c.m;
    float fy=float(y);fy=fy-240.0f;fy=fy+24.0f;                  // 6281CC, 5B4440
    call(c,0x429530u,{0x3d001au,0u,fb(fy),4u,0u});
    call(c,0x42ccb0u,{4u});call(c,0x42ca60u,{2u});call(c,0x49a650u,{0x14u});call(c,0x42cc60u,{0x3f800000u,0x3f800000u});
    const auto t=time_words(m.u32(row+0xcu));
    call(c,0x42cc00u,{std::uint32_t(x),std::uint32_t(y)});
    call(c,0x42cce0u,{0x5c212cu,t.minutes});
    call(c,0x42cc00u,{std::uint32_t(x+0x32),std::uint32_t(y)});
    call(c,0x42cce0u,{0x5c2120u,t.seconds});
    call(c,0x42cc00u,{std::uint32_t(x+0x6a),std::uint32_t(y)});
    call(c,0x42cce0u,{0x5b0334u,t.ms});
    call(c,0x49a650u,{0u});
}
// The digits and colons shared by 4B4010 / 4B4170 after their first line.
void time_digits(PcRaceContext& c,std::int32_t x,std::int32_t y,std::uint32_t time){
    const auto t=time_words(time);
    const std::int32_t ty=y+0x1c;
    digits_4b22f0(c,4u,format_int(0x5c212cu,t.minutes),x+0x33,ty,4u);
    digits_4b22f0(c,4u,format_int(0x5c2120u,t.seconds),x+0x55,ty,4u);
    digits_4b22f0(c,4u,format_int(0x5b0334u,t.ms),x+0x7c,ty,4u);
    call(c,0x429530u,{0x3d0025u,fb(float(x+0x4c)),fb(float(ty)),4u,0xau});
    call(c,0x429530u,{0x3d0025u,fb(float(x+0x70)),fb(float(ty)),4u,0xbu});
}
void text_header(PcRaceContext& c,std::int32_t x,std::int32_t y){
    call(c,0x42ccb0u,{4u});call(c,0x42ca60u,{2u});call(c,0x49a650u,{0x14u});call(c,0x42cc60u,{0x3f800000u,0x3f800000u});
    call(c,0x42cc00u,{std::uint32_t(x),std::uint32_t(y)});
}
// 4B4010(EAX = y, ECX = x, EBX = row): the score +8 ("%7d", "9999999" above 9999999), then the
// time +C in 3D0025 digits.
void score_4b4010(PcRaceContext& c,std::int32_t y,std::int32_t x,std::uint32_t row){
    auto& m=c.m;
    text_header(c,x,y);
    const std::int32_t score=m.i32(row+8u);
    if(score<=9999999)call(c,0x42cce0u,{0x5c63a8u,std::uint32_t(score)});
    else call(c,0x42cce0u,{0x5c63a0u});
    call(c,0x49a650u,{0u});
    time_digits(c,x,y,m.u32(row+0xcu));
}
// 4B4170(EAX = y, ECX = x, EBX = row): the count +10 ("%d"), then the time +C.
void count_4b4170(PcRaceContext& c,std::int32_t y,std::int32_t x,std::uint32_t row){
    auto& m=c.m;
    text_header(c,x,y);
    call(c,0x42cce0u,{0x5c2130u,m.u32(row+0x10u)});
    call(c,0x49a650u,{0u});
    time_digits(c,x,y,m.u32(row+0xcu));
}
}

// ---- event 0x189 function 0x16 ----------------------------------------------
void name_entry_init_4b2370(PcRaceContext& c){
    auto& m=c.m;
    std::int32_t route=std::int32_t(call(c,0x450320u,{}));
    if(!(route<0x10))route=0;
    const std::uint32_t variant=m.u32(0x780258u);
    const std::uint32_t course=call(c,0x44dc50u,{call(c,0x450560u,{4u})});
    m.put32(0x8427f8u,0x36000bu);m.put32(0x842804u,0);
    (void)call(c,0x428320u,{m.u32(0x5c5458u+course*4u),0u,0u});      // the handle is not kept
    m.put32(0x8426f4u,0xffffffffu);m.put32(0x8426ecu,variant);m.put32(0x8426e0u,course);m.put32(0x842708u,m.u32(0x78024cu));
    m.put32(0x8427e0u,route_bits(route));m.put32(0x8427e4u,0);m.put32(0x8427f0u,0);
    for(std::uint32_t a=0x842710u;a<=0x84271cu;a+=4)m.put32(a,0xffffffffu);
    m.put32(0x842720u,route_bits(route));
    m.put32(0x842724u,0xffffffffu);m.put32(0x8427f4u,0xffffffffu);m.put32(0x8426e4u,0x78u);
    const std::uint32_t score=call(c,0x4b99d0u,{m.u32(0x799d18u)});
    const std::int32_t choice=std::int8_t(call(c,0x48b140u,{}));
    const std::uint32_t time=call(c,0x451180u,{0u});
    const std::int32_t gear=std::int8_t(call(c,0x48b180u,{}));
    std::int32_t colour=std::int8_t(call(c,0x48b1a0u,{}));
    if(colour<0)colour=0;
    // The record on the PC stack: +0 = score | route nibble << 24 | choice << 28, +4 = time,
    // +8 = the Heart Attack ratio (variant 2) | gear << 17 | colour << 18, +C = name byte 0.
    // Its other +8 bits and name bytes 1..3 are uninitialised PC stack: taken as 0.
    std::uint32_t r8=0;
    if(variant==2u){
        const std::int32_t n=std::int32_t(call(c,0x45b830u,{})),d=std::int32_t(call(c,0x45b840u,{}));
        r8=std::uint32_t(n/d)&0xfffu;
    }
    const std::uint32_t r0=((score&0xffffffu)|(std::uint32_t(choice)<<28))^((std::uint32_t(route)&0xfu)<<24);
    r8=((((std::uint32_t(colour)&1u)<<1)|(std::uint32_t(gear)&1u))<<17)|(r8&0xfff9ffffu);
    std::int32_t rank=-1;
    if(variant!=0u||time!=0u){
        Locals l(m);
        m.put32(Locals::Record,r0);m.put32(Locals::Record+4u,time);m.put32(Locals::Record+8u,r8);m.put32(Locals::Record+0xcu,0);
        m.put32(Locals::Rank,0);
        (void)sp_rank_insert_4f3010(m,1,Locals::Record,Locals::Rank,variant,m.u32(0x842708u),m.u32(0x8427e0u),std::uint32_t(gear),std::uint32_t(colour));
        rank=m.i32(Locals::Rank);
        if(!(rank<10))rank=-1;
    }
    m.put32(0x8426d8u,std::uint32_t(rank));m.put32(0x8426dcu,std::uint32_t(route));
    m.put32(0x8427e8u,std::uint32_t(gear));m.put32(0x8427ecu,std::uint32_t(colour));
    m.put32(0x84270cu,m.u32(0x5c62acu+std::uint32_t(gear+(colour+std::int32_t(m.u32(0x8426ecu))*2)*2)*4u));
    call(c,0x47ee70u,{0x842728u});
    bool missing=false;
    m.put32(0x8427dcu,0);
    std::uint32_t preset=m.u32(0x842708u);
    if(preset!=2u&&preset!=3u){
        for(std::uint32_t level=0;level<=4u;++level){
            const std::int32_t g=std::int32_t(goal_index_4b1680(std::uint32_t(route),level));
            if(m.u32(0x842728u+level*4u))continue;
            missing=true;
            const std::uint32_t k=m.u32(0x8427dcu);
            m.put32(0x842764u+k*4u,call(c,0x44dc50u,{std::uint32_t(g)}));
            if(level==4u)m.put32(0x8427a0u+m.u32(0x8427dcu)*4u,2u);
            else{
                const std::int32_t next=std::int32_t(goal_index_4b1680(std::uint32_t(route),level+1u))-m.i32(0x5c6378u+level*4u);
                const std::int32_t here=g-m.i32(0x5c6374u+level*4u);
                m.put32(0x8427a0u+m.u32(0x8427dcu)*4u,next<=here?1u:0u);
            }
            m.put32(0x8427dcu,m.u32(0x8427dcu)+1u);
        }
    }else{
        for(std::uint32_t s=0;s<=0xfu;++s){
            if(m.u32(0x842728u+s*4u))continue;
            missing=true;
            const std::uint32_t k=m.u32(0x8427dcu);
            m.put32(0x842764u+k*4u,call(c,0x44dc50u,{s}));
            m.put32(0x8427a0u+m.u32(0x8427dcu)*4u,2u);
            m.put32(0x8427dcu,m.u32(0x8427dcu)+1u);
        }
    }
    preset=m.u32(0x842708u);
    std::uint32_t sel;
    if(rank<5&&rank>=0)sel=rank<3?std::uint32_t(rank):3u;
    else sel=(missing&&variant==0u)?5u:4u;
    m.put32(0x842704u,sel);
    m.put32(0x842700u,m.u32(0x5c5548u+(sel+course*6u)*4u));
    switch(variant){
    case 2:rows(c,2u,m.u32(0x8427e0u),preset,0u,0u);break;
    case 1:rows(c,1u,m.u32(0x8427e0u),preset,0u,0u);break;
    case 0:rows(c,0u,m.u32(0x8427e0u),preset,m.u32(0x8427e8u),m.u32(0x8427ecu));break;
    default:break;
    }
    const std::uint32_t e=rank>=0?0x8423c0u+std::uint32_t(rank)*0x48u:0x842690u;
    m.put32(e+0x14u,m.u32(0x5c636cu));m.put16(e+0x18u,m.u16(0x5c6370u));
    m.put8(e+0x1au,std::uint8_t(choice));m.put32(e+8u,score);
    if(variant==2u){
        const std::int32_t n=std::int32_t(call(c,0x45b830u,{})),d=std::int32_t(call(c,0x45b840u,{}));
        m.put32(e+0x10u,std::uint32_t(std::int32_t(std::int16_t(n/d))));
    }else m.put32(e+0x10u,0);
    m.put32(e+0x20u,std::uint32_t(route));m.put32(e+0x44u,variant);m.put32(e+0x3cu,0xffffffffu);
    m.put32(e+0xcu,time);m.put8(e+0x1bu,std::uint8_t(gear));m.put8(e+0x1cu,std::uint8_t(colour));
    const std::int32_t r=m.i32(0x8426d8u);
    m.put32(0x8426fcu,0xffffffffu);
    m.put32(0x8426f0u,std::uint32_t(r<0?5:r/5*5));
    m.put32(0x8426f8u,0);
    for(std::uint32_t b=0;b<5u;++b){
        const std::uint32_t row=0x8423c0u+(m.u32(0x8426f0u)+b)*0x48u;
        const std::uint32_t h=m.u32(row+0x3cu);
        m.put32(row+0x40u,h!=0xffffffffu?call(c,0x428320u,{h,2u,0u}):0xffffffffu);
        const float x=m.f32(0x5c5210u+b*8u),y=m.f32(0x5c5214u+b*8u);
        const float top=y-480.0f;                                   // 6280DC
        m.putf(row+0x24u,x);m.putf(row+0x28u,top);m.putf(row+0x30u,top);m.putf(row+0x34u,x);m.putf(row+0x2cu,x);m.putf(row+0x38u,y);
        Locals l(m);
        translation(m,Locals::Matrix,x,0.0f-top,0.0f);
        call(c,0x4287b0u,{m.u32(row+0x40u),Locals::Matrix});
    }
    m.put32(0x8422a8u,0xffffffffu);m.put32(0x8422a4u,0);m.put32(0x8423b0u,0);
    m.put32(0x8423b4u,m.u32(0x5c633cu));m.put8(0x8423b8u,m.u8(0x5c6340u));
    m.put32(0x8427fcu,1u);m.put32(0x842800u,0);
    call(c,0x427700u,{0x80a2u});
    m.put8(0x84280cu,1u);
}
// 4B0970: the slide of the five shown rows (+24/+28 from +2C/+30 to +34/+38 over 15 frames,
// [8426F8] * 1/15 (5A9254)) of [8426FC] (or [8426F0] while 8426FC < 0); 8426F8 -1 outside 0..15.
void name_entry_slide_4b0970(PcRaceContext& c){
    auto& m=c.m;
    const std::int32_t step=m.i32(0x8426f8u);
    if(step<0||step>0xf){m.put32(0x8426f8u,0xffffffffu);return;}
    float t=float(step);t=t*m.f32(0x5a9254u);
    const std::int32_t from=m.i32(0x8426fcu);
    const std::uint32_t first=from>=0?std::uint32_t(from):m.u32(0x8426f0u);
    for(std::uint32_t k=0;k<5u;++k){
        const std::uint32_t row=0x8423c0u+(first+k)*0x48u;
        for(std::uint32_t o:{0u,4u}){
            float v=m.f32(row+0x34u+o)-m.f32(row+0x2cu+o);v=v*t;v=v+m.f32(row+0x2cu+o);
            m.putf(row+0x24u+o,v);
        }
    }
    if(from>=0&&m.i32(0x8426f8u)==0xf){m.put32(0x8426f8u,1u);m.put32(0x8426fcu,0xffffffffu);return;}
    m.put32(0x8426f8u,m.u32(0x8426f8u)+1u);
}
void name_entry_control_4b29e0(PcRaceContext& c){
    auto& m=c.m;
    name_entry_slide_4b0970(c);
    if(call(c,0x427700u,{0x80a2u})==0u&&m.u8(0x84280cu)){
        m.put8(0x84280cu,0);
        if(call(c,0x4493c0u,{})==0u)call(c,0x4249f0u,{0x158u});
    }
    if(m.u32(0x8427fcu)){
        std::int32_t rank=m.i32(0x8426d8u);if(rank<0)rank=10;
        const std::uint32_t e=0x8423c0u+std::uint32_t(rank)*0x48u;
        for(std::uint32_t k=0;k<6u;++k)m.put8(e+0x14u+k,m.u8(0x7c23e0u+k));   // 449A60 (memcpy)
        m.put8(e+0x18u,0);
        std::string name=cstring(m,e+0x14u);
        const std::string set=cstring(m,0x5c6388u);
        name=strip_4b07b0(name,cstring(m,0x62646cu));
        name=trim_lead_4b0830(name,set);
        name=trim_trail_4b08c0(name,set);
        if(!name.empty()){
            // 68694C: words stored +1 per character (the first four decoded).
            std::uint32_t i=0;
            for(;m.u32(0x68694cu+i*4u);++i){
                std::string word=cstring(m,m.u32(0x68694cu+i*4u));
                for(std::size_t k=0;k<4u&&k<word.size();++k)word[k]=char(word[k]-1);
                if(same_nocase(name,word))break;
            }
            if(m.u32(0x68694cu+i*4u)){m.put32(e+0x14u,m.u32(0x5b4490u));m.put8(e+0x18u,m.u8(0x5b4494u));}
        }
        put_string(m,0x8423b4u,cstring(m,0x7c23e0u));
        const std::uint32_t preset=m.u32(0x842708u),count=m.u32(0x8427e0u);const std::int32_t r=m.i32(0x8426d8u);
        auto commit=[&](std::uint32_t at){
            std::array<std::uint32_t,4> rec{m.u32(at),m.u32(at+4u),m.u32(at+8u),m.u32(at+0xcu)};
            rec[3]=strncpy4(m.u32(0x7c23e0u));                                 // 581780 (strncpy, 4)
            sp_store_record(m,at,rec);
        };
        switch(m.u32(0x8426ecu)){
        case 2:commit(sp_record_4f3910(preset,count,std::uint32_t(r)));break;   // 4F3910 / 4F3A80
        case 1:commit(sp_record_4f3810(preset,count,std::uint32_t(r)));break;   // 4F3810 / 4F3950
        case 0:commit(sp_record_4f3870(preset,m.u32(0x8427e8u),m.u32(0x8427ecu),count,std::uint32_t(r)));break;   // 4F3870 / 4F39E0
        default:break;
        }
        sp_book_from_tables_4f3ad0(m);
        m.put32(0x8427fcu,0);
        return;
    }
    switch(m.u32(0x842800u)){
    case 0:case 1:m.put32(0x842800u,2u);break;
    case 2:m.put32(0x842800u,5u);break;
    case 5:call(c,0x4165f0u,{});m.put32(0x842800u,m.u32(0x842800u)+1u);break;
    case 6:m.put32(0x842800u,7u);break;
    case 7:
        if(m.u32(0x780258u)==0u){call(c,0x480d00u,{0x7c23e0u});call(c,0x481180u,{0x7c23e0u});call(c,0x47ef10u,{0x7c23e0u});}
        m.put32(0x8426e4u,0xf0u);m.put32(0x842800u,m.u32(0x842800u)+1u);break;
    case 8:{
        const std::int32_t left=m.i32(0x8426e4u)-1;m.put32(0x8426e4u,std::uint32_t(left));
        if(left<=0||call(c,0x4536f0u,{1u}))m.put32(0x842800u,m.u32(0x842800u)+1u);
        break;}
    case 9:m.put32(0x842800u,0xau);break;
    case 0xa:m.put32(0x842800u,0xbu);break;
    case 0xb:m.put32(0x842800u,0xcu);break;
    case 0xc:m.put32(0x842800u,0xdu);break;
    case 0xd:m.put32(0x842804u,1u);break;
    default:break;
    }
}
// 4B46F0: the ranking board. The letter-input layout (8427E4 != 0) is unreachable on the PC.
void name_entry_display_4b46f0(PcRaceContext& c){
    auto& m=c.m;
    const std::uint32_t preset=m.u32(0x842708u);
    const std::uint32_t t=0x5c5d18u+(m.u32(0x84270cu)+preset*6u)*16u;   // {back, title, row tokens, title tokens}
    if(preset!=2u&&preset!=3u){
        if(m.i32(0x842714u)<0)m.put32(0x842714u,call(c,0x428460u,{m.u32(t+4u),4u,1u,0u,0x1eu}));
        if(m.i32(0x842710u)<0)m.put32(0x842710u,call(c,0x428320u,{m.u32(t),4u,1u}));
        if(call(c,0x428880u,{m.u32(0x842714u)})!=3u)return;
    }else{
        if(m.i32(0x842714u)<0)m.put32(0x842714u,call(c,0x428460u,{m.u32(0x8427e4u)?0x3d001bu:m.u32(t+4u),4u,1u,0u,0x1eu}));
        if(m.i32(0x842710u)<0)
            m.put32(0x842710u,call(c,0x428460u,{preset==2u?0x3d001du:preset==3u?0x3d001eu:m.u32(t),4u,1u,0u,0x14u}));
        if(call(c,0x428880u,{m.u32(0x842714u)})!=3u)return;
        if(m.i32(0x8427f4u)<0)m.put32(0x8427f4u,0);
        m.put32(0x842720u,6u);
    }
    if(m.u32(0x8427e4u))throw PcRaceEndUnreachable{0x4b4830u};
    const std::int32_t line=m.i32(0x842720u);
    if(line==6){
        if(preset==2u)call(c,0x429530u,{0x3d0000u,0x42100000u,0x41000000u,2u,0u});
        else if(preset==3u)call(c,0x429530u,{0x3d0001u,0x42100000u,0x41000000u,2u,0u});
    }else call(c,0x429530u,{m.u32(m.u32(t+0xcu)+std::uint32_t(line)*4u),0u,0u,2u,0u});
    if(m.i32(0x8426f0u)<5)call(c,0x429530u,{0x3d0015u,0u,0u,2u,0x12au});
    else for(std::uint32_t p=0x5c6298u;p<0x5c62acu;p+=4)call(c,0x429530u,{0x3d0016u,0u,m.u32(p),2u,0x12au});   // 8427E4 = 0: the 5C6298 list
    if(m.i32(0x842724u)<0&&m.i32(0x8426d8u)>=0)
        m.put32(0x842724u,call(c,0x428320u,{m.u32(0x5c5c98u+std::uint32_t(m.i32(0x8426d8u)%5)*4u),3u,0u}));
    if(m.i32(0x842720u)<0)return;
    (void)call(c,0x428840u,{m.u32(0x842718u)});                       // its frame is only read with 8427E4 set
    const std::uint32_t table=m.i32(0x8426f0u)<5?0x5c5ef8u:0x5c6028u;
    for(std::uint32_t i=0;i<5u;++i){
        const std::uint32_t base=0x8423c0u+std::uint32_t(m.i32(0x8426f0u)/5)*0x168u;
        const std::uint32_t row=base+(4u-i)*0x48u;
        const std::uint32_t at=table+i*0x3cu;
        call(c,0x42ccb0u,{4u});call(c,0x42ca60u,{2u});call(c,0x42cca0u,{0xffffff00u});call(c,0x49a650u,{0x1au});
        call(c,0x42cc00u,{std::uint32_t(cvtt(m.f32(at+4u))),std::uint32_t(cvtt(m.f32(at+8u)))});
        {Locals l(m);
         filter_4b1f40(m,Locals::Text,row+0x14u,6u);
         call(c,0x42cce0u,{0x626468u,Locals::Text});}
        call(c,0x49a650u,{0u});call(c,0x42cca0u,{0xffffffffu});
        if(m.i32(0x8426f0u)>=5){
            const std::int32_t n=m.i32(0x8426f0u)+std::int32_t(i)+1;
            const std::uint32_t mirror=table-i*0x3cu;                    // ebp - esi*0x3C
            digits_4b22f0(c,3u,format_int(0x5c212cu,n),cvtt(m.f32(mirror+0x11cu)),cvtt(m.f32(mirror+0x120u)),3u);
            float x=m.f32(mirror+0x11cu);x=x+46.0f;                       // 5C6900
            call(c,0x429530u,{0x3d0026u,fb(x),m.u32(mirror+0x120u),3u,0xau});
        }
        call(c,0x429530u,{0x3d0024u,m.u32(at+0x14u),m.u32(at+0x18u),3u,m.u32(0x5c5c20u+std::uint32_t(std::int32_t(m.i8(row+0x1au)))*4u)});
        call(c,0x429530u,{0x3d0027u,m.u32(at+0xcu),m.u32(at+0x10u),3u,std::uint32_t(std::int32_t(m.i8(row+0x1bu)))});
        if(preset!=2u&&preset!=3u)route_marks_4b17e0(c,cvtt(m.f32(at+0x1cu)),cvtt(m.f32(at+0x20u)),3u,m.u32(row+0x20u),0x3d0014u);
        else{
            Locals l(m);
            translation(m,Locals::Matrix,float(cvtt(m.f32(at+0x1cu))),float(cvtt(m.f32(at+0x20u))),0.0f);
            call(c,0x4289b0u,{0x3d0011u,3u,Locals::Matrix,0u});
        }
        switch(m.u32(row+0x44u)){
        case 0:
            call(c,0x429530u,{0x3d0028u,m.u32(at+0x34u),m.u32(at+0x38u),3u,m.u32(0x5c68f8u+std::uint32_t(std::int32_t(m.i8(row+0x1cu)))*4u)});
            time_4b1fa0(c,cvtt(m.f32(at+0x24u)),cvtt(m.f32(at+0x28u)),row);break;
        case 1:score_4b4010(c,cvtt(m.f32(at+0x28u)),cvtt(m.f32(at+0x24u)),row);break;
        case 2:count_4b4170(c,cvtt(m.f32(at+0x28u)),cvtt(m.f32(at+0x24u)),row);break;
        default:break;
        }
    }
    if(m.i32(0x8426d8u)>=0)return;
    // Outside the ranking: the "no entry" banner and the result in the 842690 row.
    call(c,0x42d5c0u,{0x150010u,0u,0x193u,0x3f800000u,0xffffffffu});
    call(c,0x42ccb0u,{4u});call(c,0x42ca60u,{3u});
    switch(m.u32(0x8426d4u)){
    case 0:
        time_4b1fa0(c,0x144,0x1a3,0x842690u);
        call(c,0x42cc00u,{0x32u,0x1a3u});
        call(c,0x42cce0u,{call(c,0x465eb0u,{0x444u}),0x444u});break;
    case 1:
        score_4b4010(c,0x1a3,0x144,0x842690u);
        call(c,0x42cc00u,{0x32u,0x1a3u});
        call(c,0x42cce0u,{call(c,0x465eb0u,{0x445u}),0x445u});break;
    case 2:
        count_4b4170(c,0x1a3,0x144,0x842690u);
        call(c,0x42cc00u,{0x32u,0x1a3u});
        call(c,0x42cce0u,{call(c,0x465eb0u,{0x446u}),0x446u});break;
    default:break;
    }
}
void name_entry_display_4b4f20(PcRaceContext& c){
    name_entry_display_4b46f0(c);
    if(c.m.u32(0x8423b0u))throw PcRaceEndUnreachable{0x4b4f35u};       // letter input board
}
// 4B0950: 428600, [842804] = 0.
void name_entry_destroy_4b0950(PcRaceContext& c){call(c,0x428600u,{});c.m.put32(0x842804u,0);}
std::uint32_t name_entry_done_4b0960(PcRaceContext& c){return c.m.u32(0x842804u);}

// ---- mode 26 -----------------------------------------------------------------
void mode26_init_49a960(PcRaceContext& c){
    call(c,0x43f900u,{0u});
    call(c,0x42deb0u,{0x3du,9u});call(c,0x42deb0u,{0x12u,9u});call(c,0x42deb0u,{0x14u,9u});call(c,0x42deb0u,{0x15u,9u});
    call(c,0x429920u,{0x3du,9u});
    call(c,0x42deb0u,{0x36u,9u});call(c,0x429920u,{0x36u,9u});
    const std::uint32_t course=call(c,0x44dc50u,{call(c,0x450560u,{4u})});
    // 49AA4C / 49AA34 over course - 10 (0..0x13): the course's ranking pack 37..3B.
    static constexpr std::uint8_t Pick[20]{0,1,2,3,4,5,5,5,5,5,5,5,5,5,5,1,3,2,4,0};
    static constexpr std::uint32_t Pack[6]{0x37u,0x38u,0x39u,0x3au,0x3bu,0x37u};
    const std::uint32_t k=course-10u;
    const std::uint32_t id=k>0x13u?0x37u:Pack[Pick[k]];
    call(c,0x42deb0u,{id,9u});call(c,0x429920u,{id,9u});
    c.m.put32(0x836d0cu,0);
    common_anim_4b72f0(c,1u);
}
void mode26_control_49aa60(PcRaceContext& c){
    auto& m=c.m;
    switch(m.u32(0x836d0cu)){
    case 0:
        if(call(c,0x42df90u,{})&&call(c,0x4299a0u,{})){
            call(c,0x440110u,{0x189u,0x16u});call(c,0x440110u,{0x17fu,0x17u});
            common_anim_release_4b7630(c);
            m.put32(0x836d0cu,m.u32(0x836d0cu)+1u);
        }
        break;
    case 1:
        if(!name_entry_done_4b0960(c))break;
        m.put32(0x836d08u,m.u32(0x836d08u)+1u);
        [[fallthrough]];
    case 2:
        if(call(c,0x43fa90u,{}))call(c,0x43f8c0u,{0x1bu});
        break;
    default:break;
    }
    if(call(c,0x43f980u,{}))call(c,0x43f990u,{0x1du});
}
void mode26_exit_49aae0(PcRaceContext& c){
    const std::uint32_t variant=c.m.u32(0x780258u);
    call(c,0x4401d0u,{0x189u});
    if(!(variant&0xffffu))return;
    call(c,0x427630u,{});
    call(c,0x4401d0u,{0x17fu});
    for(std::uint32_t id:{0x3du,0x12u,0x14u,0x15u,0x36u,0x37u,0x38u,0x39u,0x3au,0x3bu})call(c,0x42dfb0u,{id});
    for(std::uint32_t id:{0x3du,0x36u,0x37u,0x38u,0x39u,0x3au,0x3bu})call(c,0x4299c0u,{id});
}
std::uint32_t name_entry_record_4b1dd0(PcRaceContext& c){
    auto& m=c.m;
    auto call=[&](std::uint32_t pc,std::initializer_list<std::uint32_t> args){
        PcRaceCall k;k.pc=pc;k.argc=std::uint32_t(args.size());std::size_t i=0;for(auto a:args)k.args[i++]=a;return c.service(k);};
    const std::uint32_t score=call(0x4b99d0u,{m.u32(0x799d18u)});
    const std::int32_t course=std::int8_t(call(0x48b140u,{}));
    const std::uint32_t time=call(0x451180u,{0u});
    const std::int32_t transmission=std::int8_t(call(0x48b180u,{}));
    std::int32_t vehicle=std::int8_t(call(0x48b1a0u,{}));
    std::uint32_t stage=call(0x450320u,{});
    if(!(std::int32_t(stage)<0x10))stage=0;
    if(m.u32(0x780258u)!=0u)return 0;
    if(vehicle<0)vehicle=0;
    const std::uint32_t w0=(score&0xffffffu)|(std::uint32_t(course)<<28)|((stage&0xfu)<<24);
    const std::uint32_t w2=(std::uint32_t(((vehicle&1)<<1)|(transmission&1)))<<17;
    static constexpr std::uint8_t Column[16]{0,1,1,2,1,2,2,3,1,2,2,3,2,3,3,4};   // 4B1F28 -> 0xA..0xE (default 0xF)
    const std::uint32_t count=stage<16u?Column[stage]:5u;
    std::vector<std::int32_t> ranks;
    (void)sp_rank_query_4f3440(m,{{w0,time,w2,0u}},ranks,0u,m.u32(0x78024cu),count,std::uint32_t(transmission),std::uint32_t(vehicle));
    if(ranks.empty())return 0;                                        // 4F3440 returned -1: the rank slot keeps its 0
    return (ranks[0]>=0&&ranks[0]<=4)?1u:0u;
}

// ---- event 0x18B function 0x1E: the letter-board name entry of mode 31 --------
// 4B2F00 init / 4B42C0 control / 4B4550 display. The board: letters 0..0x2F of the
// 5C5250 alphabet (two bytes each), 0x30 "back", 0x31 "end"; the cursor 84229C
// moves with the stick / the wheel (4B0F10) or the d-pad repeat, A types (4BFB20),
// B erases (4BFB70); four letters at most (842288..842294, 842298 typed, 0x2F =
// none); 842394 the countdown (1800 frames). The name is committed (4B3A50) into
// the row of the entry or the 842690 row, bad words as "----".
namespace {
// The 0x48-byte image record of 42A070 / 42C2F0 / 42CFE0, passed by address.
constexpr std::uint32_t NameEntry2Image=PcNameEntryLocals+0x100u;
// sprintf of the integer formats of the board (the guest format string).
std::string guest_format_int(PcRaceMemory& m,std::uint32_t format,std::int32_t v){
    const std::string f=cstring(m,format);
    // Only "%d" with flags / width: the formats 5C212C, 5C2120, 5B0334, 5C63A8, 5B4A28.
    std::size_t at=f.find('%');
    if(at==std::string::npos||f.find('%',at+1)!=std::string::npos||f.back()!='d')throw PcRaceEndUnreachable{format};
    char out[32];std::snprintf(out,sizeof out,f.c_str(),v);return out;
}
// 4B0F10: the stick / wheel (453720) as cursor steps: -1 / 1 when it is past a
// threshold for long enough (120: 3 frames, 60: 10, the moving dead zone
// 686980..68697C: 45 frames); the zone widens to the pushed side; 0 otherwise.
std::int32_t board_stick_4b0f10(PcRaceContext& c){
    auto& m=c.m;
    const std::int32_t x=std::int32_t(call(c,0x453720u,{0u}));
    (void)call(c,0x453750u,{0u});
    std::int32_t low=m.i32(0x686980u),high=m.i32(0x68697cu);
    std::int32_t held;
    if(x>low&&x<high){
        const std::int32_t idle=m.i32(0x842814u)+1;m.put32(0x842814u,std::uint32_t(idle));
        if(idle>0x32){low=-15;high=15;m.put32(0x686980u,std::uint32_t(low));m.put32(0x68697cu,std::uint32_t(high));m.put32(0x842814u,0);}
        held=0xffff;m.put32(0x842810u,0xffffu);
    }else{
        held=m.i32(0x842810u);
        if(held<0xffff){++held;m.put32(0x842810u,std::uint32_t(held));}
    }
    const bool step=(x>=0x78&&held>3)||(x<=-0x78&&held>3)||(x>=0x3c&&held>0xa)||(x<=-0x3c&&held>0xa)||
                    (x>=high&&held>0x2d)||(x<=low&&held>0x2d);
    if(!step)return 0;
    m.put32(0x842814u,0);m.put32(0x842810u,0);
    if(x>0){m.put32(0x68697cu,0xfu);m.put32(0x686980u,0xffffffbfu);return 1;}
    m.put32(0x686980u,0xfffffff1u);m.put32(0x68697cu,0x41u);return -1;
}
// The typed-letter slot sprite (360007, frames by the letters typed).
void board_slots(PcRaceContext& c,std::uint32_t typed){
    static constexpr std::uint32_t Frames[6][2]{{0x33,0x34},{0x3d,0x3e},{0x47,0x48},{0x51,0x52},{0x5b,0x5c},{0x79,0x7a}};
    if(typed>5u)throw PcRaceEndUnreachable{0x4b12f0u};   // the PC frames come from an uninitialised local
    c.m.put32(0x8422b0u,call(c,0x428460u,{0x360007u,5u,0u,Frames[typed][0],Frames[typed][1]}));
}
// 4B0CC0: the board appears: no letters, the cursor on 'A'.
void board_open_4b0cc0(PcRaceContext& c){
    auto& m=c.m;
    (void)call(c,0x44dc50u,{call(c,0x450560u,{4u})});
    const std::uint32_t e=m.u32(0x842704u)*24u;
    for(std::uint32_t a=0x842288u;a<=0x8422a0u;a+=4)m.put32(a,0);
    m.put32(0x8422acu,0xffffffffu);
    const std::uint32_t board=call(c,0x428460u,{m.u32(0x842700u),5u,1u,m.u32(0x5c5b78u+e),m.u32(0x5c5b7cu+e)});
    for(std::uint32_t a:{0x8422b4u,0x8422bcu,0x8422c0u})m.put32(a,0xffffffffu);
    m.put32(0x8423a0u,0);m.put16(0x8423a4u,0);m.put16(0x8423a6u,0);m.put32(0x8423acu,0);m.put32(0x8423a8u,0);
    m.put32(0x8422b8u,board);
    m.put32(0x842390u,0x32u);
}
// 4B0D80: the board's sprites (slots, cursor, arrows), the letter positions 5C53E0..5C5437
// into 842308, the countdown 1800 frames, the letter wheel 360008.
void board_sprites_4b0d80(PcRaceContext& c){
    auto& m=c.m;
    (void)call(c,0x44dc50u,{call(c,0x450560u,{4u})});
    m.put32(0x8422b0u,call(c,0x428460u,{0x360007u,6u,1u,1u,0x1eu}));
    m.put32(0x8422b4u,call(c,0x428460u,{m.u32(0x8427f8u),0xbu,1u,1u,0x1eu}));
    m.put32(0x8422bcu,call(c,0x428460u,{0x360006u,7u,1u,1u,0x1eu}));
    for(std::uint32_t k=0;k<0x58u;k+=4)m.put32(0x842308u+k,m.u32(0x5c53e0u+k));
    m.put32(0x842394u,0x708u);m.put32(0x842808u,0);
    const std::uint32_t wheel=call(c,0x428320u,{0x360008u,9u,2u});
    m.put32(0x8422a8u,wheel);
    call(c,0x4288c0u,{wheel,0u});
}
// 4BFB70: B (button 8), unless the title owner's +518 is positive.
bool board_erase_4bfb70(PcRaceContext& c){
    const std::uint32_t owner=call(c,0x4035f0u,{});
    if(c.m.i32(owner+0x518u)>0)return false;
    return call(c,0x4536f0u,{8u})!=0u;
}
// 4B1010: one frame of the board; 1 when the name is done (time up, button 1, "end",
// or B with no letters).
std::uint32_t board_input_4b1010(PcRaceContext& c){
    auto& m=c.m;
    const std::int32_t stick=std::int32_t(call(c,0x453720u,{0u}));
    (void)call(c,0x44dc50u,{call(c,0x450560u,{4u})});
    auto end_jingle=[&]{if(m.i32(0x842394u)>=0x5dc&&m.u32(0x842704u)<=2u)call(c,0x4249f0u,{0x80aeu});};
    if(m.i32(0x842394u)>=0){
        const std::int32_t left=m.i32(0x842394u)-1;m.put32(0x842394u,std::uint32_t(left));
        if((left==0||left==0x5dc||call(c,0x4536f0u,{1u}))&&m.u32(0x842704u)<=2u)call(c,0x4249f0u,{0x80aeu});
        if(m.u32(0x842394u)==0u||call(c,0x4536f0u,{1u})){
            // Time up / button 1: the letters not typed are blanks (0x28 when none was).
            std::uint32_t typed=m.u32(0x842298u);
            if(typed==0u){
                m.put32(0x84229cu,0x28u);call(c,0x4285a0u,{m.u32(0x8422b0u)});m.put32(0x8422b0u,0xffffffffu);
                for(typed=m.u32(0x842298u);std::int32_t(typed)<4;++typed){m.put32(0x842288u+typed*4u,m.u32(0x84229cu));m.put32(0x842298u,typed+1u);}
            }else for(;std::int32_t(typed)<4;++typed){m.put32(0x842288u+typed*4u,0x2fu);m.put32(0x842298u,typed+1u);}
            end_jingle();
            return 1;
        }
        const std::int32_t t=m.i32(0x842394u);
        if(t/60<=10&&t%60==0)call(c,0x424940u,{0x518du});                  // the last ten seconds
    }
    if(call(c,0x428880u,{m.u32(0x8422bcu)})==3u){
        call(c,0x4285a0u,{m.u32(0x8422bcu)});
        m.put32(0x8422bcu,call(c,0x428460u,{0x360006u,7u,0u,1u,0x1eu}));
    }
    bool erase=false;
    if(call(c,0x4bfb20u,{})){                                            // A
        call(c,0x424940u,{0xa4u});
        const std::uint32_t at=m.u32(0x84229cu);
        if(at==0x31u){                                                   // "end": the rest blank
            for(std::uint32_t typed=m.u32(0x842298u);std::int32_t(typed)<4;++typed){m.put32(0x842288u+typed*4u,0x2fu);m.put32(0x842298u,typed+1u);}
            end_jingle();
            return 1;
        }
        if(at==0x30u)erase=true;                                         // "back"
        else{
            m.put32(0x842288u+m.u32(0x842298u)*4u,at);
            m.put32(0x842298u,m.u32(0x842298u)+1u);
            call(c,0x4285a0u,{m.u32(0x8422b0u)});
            board_slots(c,m.u32(0x842298u));
            if(std::int32_t(m.u32(0x842298u))>=4){                         // full: the cursor to "end"
                m.put32(0x8422a0u,0x31u);m.put32(0x84229cu,0x31u);m.put32(0x842808u,1u);
                return 0;
            }
        }
    }
    if(board_erase_4bfb70(c)||erase){                                    // B / "back"
        m.put32(0x842808u,0);
        if(m.i32(0x842298u)<=0){m.put32(0x842288u,0);return 1;}
        call(c,0x424940u,{0x165u});
        m.put32(0x842298u,m.u32(0x842298u)-1u);
        call(c,0x4285a0u,{m.u32(0x8422b0u)});
        const std::uint32_t typed=m.u32(0x842298u);
        if(typed>4u)throw PcRaceEndUnreachable{0x4b1377u};
        board_slots(c,typed);
    }
    // The cursor: the stick, else the d-pad (right 0x80 / left 0x40; held: first step,
    // then every 3 frames after 20).
    std::int32_t step=board_stick_4b0f10(c);
    if(step){m.put32(0x84239cu,m.u32(0x842398u));m.put32(0x842398u,std::uint32_t(step));}
    else if(m.u32(0x8423acu)){
        if(m.u32(0x8423a8u)==0u){
            if(call(c,0x4536c0u,{0x80u})){m.put32(0x84239cu,m.u32(0x842398u));step=1;m.put32(0x842398u,1u);m.put32(0x8423a8u,3u);}
            else if(call(c,0x4536c0u,{0x40u})){m.put32(0x84239cu,m.u32(0x842398u));step=-1;m.put32(0x842398u,0xffffffffu);m.put32(0x8423a8u,3u);}
            else m.put32(0x8423acu,0);
        }else if(call(c,0x4536c0u,{0xc0u}))m.put32(0x8423a8u,m.u32(0x8423a8u)-1u);
        else{m.put32(0x8423acu,0);m.put32(0x8423a8u,0);}
    }else{
        if(call(c,0x4536f0u,{0x80u})){m.put32(0x84239cu,m.u32(0x842398u));step=1;m.put32(0x842398u,1u);m.put32(0x8423a8u,0x14u);m.put32(0x8423acu,1u);}
        else if(call(c,0x4536f0u,{0x40u})){m.put32(0x84239cu,m.u32(0x842398u));step=-1;m.put32(0x842398u,0xffffffffu);m.put32(0x8423a8u,0x14u);m.put32(0x8423acu,1u);}
        else{m.put32(0x8423acu,0);m.put32(0x8423a8u,0);}
    }
    if(m.u32(0x842808u)==1u){                                            // on "end" after the fourth letter
        if(step==1&&m.u32(0x84229cu)==0x31u)return 0;
        if(step==-1&&m.u32(0x84229cu)==0x30u)return 0;
    }
    if(step==-1){
        call(c,0x424940u,{0x164u});
        m.put16(0x8423a6u,0xffffu);
        const std::uint32_t speed=stick< -0x78?3u:stick< -0x3c?0xau:0x2du;
        m.put32(0x8423a0u,speed);m.put16(0x8423a4u,std::uint16_t(speed));
        call(c,0x4285a0u,{m.u32(0x8422bcu)});
        m.put32(0x8422bcu,call(c,0x428460u,{0x360006u,7u,1u,0x3cu,0x58u}));
        const std::uint32_t at=m.u32(0x84229cu);m.put32(0x8422a0u,at);
        m.put32(0x84229cu,std::int32_t(at)-1<0?0x31u:at-1u);
    }else if(step==1){
        call(c,0x424940u,{0x164u});
        m.put16(0x8423a6u,1u);
        const std::uint32_t speed=stick>0x78?3u:stick>0x3c?0xau:0x2du;
        m.put32(0x8423a0u,speed);m.put16(0x8423a4u,std::uint16_t(speed));
        call(c,0x4285a0u,{m.u32(0x8422bcu)});
        m.put32(0x8422bcu,call(c,0x428460u,{0x360006u,7u,1u,0x1fu,0x3cu}));
        const std::uint32_t at=m.u32(0x84229cu);m.put32(0x8422a0u,at);
        m.put32(0x84229cu,at+1u>=0x32u?0u:at+1u);
    }
    return 0;
}
// The time box of a row: minutes / seconds / ms of +C in digit style `style`.
void board_time(PcRaceContext& c,std::uint32_t style,std::uint32_t time,std::int32_t x0,std::int32_t x1,std::int32_t x2,std::int32_t y){
    const auto t=time_words(time);
    digits_4b22f0(c,style,format_int(0x5c212cu,t.minutes),x0,y,0xau);
    digits_4b22f0(c,style,format_int(0x5c2120u,t.seconds),x1,y,0xau);
    digits_4b22f0(c,style,format_int(0x5b0334u,t.ms),x2,y,0xau);
}
// The route of a row (or the C2C / Heart Attack badge) and the row's frame sprite.
void board_route(PcRaceContext& c,std::uint32_t row,std::uint32_t frame_token){
    auto& m=c.m;
    const std::uint32_t preset=m.u32(0x842708u);
    if(preset!=2u&&preset!=3u)route_marks_4b17e0(c,0xdc,-2,9u,m.u32(row+0x20u),0x36000au);
    else{
        Locals l(m);
        translation(m,Locals::Matrix,220.0f,-2.0f,0.0f);
        call(c,0x4289b0u,{0x360009u,9u,Locals::Matrix,0u});
    }
    call(c,0x428980u,{frame_token,0xau,0u});
}
// 4B3410 (ESI = row): the entry's score row.
void board_score_4b3410(PcRaceContext& c,std::uint32_t row){
    auto& m=c.m;
    board_time(c,2u,m.u32(row+0xcu),0x9d,0xc5,0xed,0xe7);
    const std::int32_t score=m.i32(row+8u);
    digits_4b22f0(c,1u,score>9999999?cstring(m,0x5c63a0u):guest_format_int(m,0x5c63a8u,score),0xdc,0xc1,0xau);
    call(c,0x429530u,{0x360001u,0x43bc8000u,0x43650000u,0xau,m.u32(0x5c5c20u+std::uint32_t(std::int32_t(m.i8(row+0x1au)))*4u)});
    call(c,0x429530u,{0x360001u,0x43a60000u,0x43620000u,0xau,std::uint32_t(std::int32_t(m.i8(row+0x1bu))+0xa)});
    board_route(c,row,0x36000cu);
}
// 4B35D0 (ESI = row): the entry's count row.
void board_count_4b35d0(PcRaceContext& c,std::uint32_t row){
    auto& m=c.m;
    board_time(c,2u,m.u32(row+0xcu),0x9d,0xc5,0xed,0xe7);
    digits_4b22f0(c,1u,guest_format_int(m,0x5b4a28u,m.i32(row+0x10u)),0x10a,0xc1,0xau);
    call(c,0x429530u,{0x360001u,0x43bc8000u,0x43650000u,0xau,m.u32(0x5c5c20u+std::uint32_t(std::int32_t(m.i8(row+0x1au)))*4u)});
    call(c,0x429530u,{0x360001u,0x43a60000u,0x43620000u,0xau,std::uint32_t(std::int32_t(m.i8(row+0x1bu))+0xa)});
    board_route(c,row,0x36000du);
}
// 4B3770 (EAX = row): the entry's time row, then the goals not reached yet.
void board_time_4b3770(PcRaceContext& c,std::uint32_t row){
    auto& m=c.m;
    board_time(c,1u,m.u32(row+0xcu),0xbf,0x107,0x157,0xc1);
    call(c,0x429530u,{0x360001u,0x43b70000u,0x43650000u,0xau,m.u32(0x5c5c20u+std::uint32_t(std::int32_t(m.i8(row+0x1au)))*4u)});
    call(c,0x429530u,{0x360001u,0x43a08000u,0x43620000u,0xau,std::uint32_t(std::int32_t(m.i8(row+0x1bu))+0xa)});
    call(c,0x429530u,{0x360001u,0x43300000u,0x43640000u,0xau,std::uint32_t(std::int32_t(m.i8(row+0x1cu))+0xc)});
    board_route(c,row,0x36000eu);
    const std::uint32_t sel=m.u32(0x842704u);
    const std::int32_t missing=m.i32(0x8427dcu);
    if(!(sel==5u||(missing>0&&m.i32(0x8426d8u)>=0&&m.i32(0x8426d8u)<=4)))return;
    const float y=m.f32(sel==5u?0x5c68e0u:0x5cdcc4u);
    const std::uint32_t preset=m.u32(0x78024cu);
    for(std::int32_t i=0;i<m.i32(0x8427dcu);++i){
        const std::int32_t n=m.i32(0x8427dcu);
        const std::uint32_t token=m.u32(0x5c67d8u+m.u32(0x842764u+std::uint32_t(i)*4u)*4u);
        if(preset!=2u&&preset!=3u){
            const std::uint32_t at=0x5c675cu+std::uint32_t(i+n*5)*4u;
            const float x=m.f32(at)-m.f32(0x5cdb5cu);
            call(c,0x429530u,{0x360002u,fb(x),fb(y),0xau,token});
            const std::int32_t mark=m.i32(0x8427a0u+std::uint32_t(i)*4u);
            if(mark>=0)call(c,0x429530u,{0x360002u,m.u32(0x5c675cu+std::uint32_t(i+m.i32(0x8427dcu)*5)*4u),fb(y),0xau,std::uint32_t(mark+0x1e)});
        }else{
            const float x=(m.f32(0x5c63acu+std::uint32_t(n*15+i)*4u)+m.f32(0x5c63a8u+std::uint32_t(n)*4u))-m.f32(0x628074u);
            call(c,0x429530u,{0x360002u,fb(x),fb(y),0xau,token});
        }
    }
}
// 4B3A50: the name typed into the entry's row (5C5250 letters, filtered, bad words as
// "----"), copied to 8423B4, and stored in its SP ranking table.
void board_commit_4b3a50(PcRaceContext& c){
    auto& m=c.m;
    (void)call(c,0x44dc50u,{call(c,0x450560u,{4u})});
    for(std::uint32_t a:{0x8422bcu,0x8422acu,0x8422b4u})call(c,0x4285a0u,{m.u32(a)});
    for(std::uint32_t a:{0x8422bcu,0x8422acu,0x8422b4u})m.put32(a,0xffffffffu);
    call(c,0x4285a0u,{m.u32(0x8422b8u)});
    const std::uint32_t e=m.u32(0x842704u)*24u;
    m.put32(0x8422b8u,call(c,0x428460u,{m.u32(0x842700u),4u,1u,m.u32(0x5c5b88u+e),m.u32(0x5c5b8cu+e)}));
    call(c,0x4285a0u,{m.u32(0x8422b0u)});
    std::int32_t rank=m.i32(0x8426d8u);if(rank<0)rank=10;
    const std::uint32_t name=0x8423c0u+std::uint32_t(rank)*0x48u+0x14u;
    for(std::uint32_t k=0;k<4u;++k)m.put8(name+k,m.u8(0x5c5250u+m.u32(0x842288u+k*4u)*2u));
    m.put8(name+4u,0);
    const std::string set=cstring(m,0x5c6388u);
    std::string typed=strip_4b07b0(cstring(m,name),cstring(m,0x62646cu));
    typed=trim_lead_4b0830(typed,set);
    typed=trim_trail_4b08c0(typed,set);
    if(!typed.empty()){
        std::uint32_t i=0;                                              // 68694C: words stored +1 per character
        for(;m.u32(0x68694cu+i*4u);++i){
            std::string word=cstring(m,m.u32(0x68694cu+i*4u));
            for(std::size_t k=0;k<4u&&k<word.size();++k)word[k]=char(word[k]-1);
            if(same_nocase(typed,word))break;
        }
        if(m.u32(0x68694cu+i*4u)){m.put32(name,m.u32(0x5b4490u));m.put8(name+4u,m.u8(0x5b4494u));}
    }
    put_string(m,0x8423b4u,cstring(m,name));
    const std::int32_t r=m.i32(0x8426d8u);
    if(r<0)return;
    const std::uint32_t preset=m.u32(0x842708u),count=m.u32(0x8427e0u);
    auto commit=[&](std::uint32_t at){
        std::array<std::uint32_t,4> rec{m.u32(at),m.u32(at+4u),m.u32(at+8u),m.u32(at+0xcu)};
        rec[3]=strncpy4(m.u32(name));                                   // 581780 (strncpy, 4)
        sp_store_record(m,at,rec);
        sp_book_from_tables_4f3ad0(m);
    };
    switch(m.u32(0x8426ecu)){
    case 2:commit(sp_record_4f3910(preset,count,std::uint32_t(r)));break;   // 4F3910 / 4F3A80
    case 1:commit(sp_record_4f3810(preset,count,std::uint32_t(r)));break;   // 4F3810 / 4F3950
    case 0:commit(sp_record_4f3870(preset,m.u32(0x8427e8u),m.u32(0x8427ecu),count,std::uint32_t(r)));break;   // 4F3870 / 4F39E0
    default:sp_book_from_tables_4f3ad0(m);break;
    }
}
}
void name_entry2_init_4b2f00(PcRaceContext& c){
    auto& m=c.m;
    std::int32_t route=std::int32_t(call(c,0x450320u,{}));
    if(!(route<0x10))route=0;
    const std::uint32_t variant=m.u32(0x780258u);
    const std::uint32_t course=call(c,0x44dc50u,{call(c,0x450560u,{4u})});
    m.put32(0x8427f8u,0x36000bu);m.put32(0x842804u,0);
    (void)call(c,0x428320u,{0x37000du,0u,0u});                          // the handle is not kept
    m.put32(0x8426f4u,0xffffffffu);m.put32(0x8426ecu,variant);m.put32(0x8426e0u,course);m.put32(0x842708u,m.u32(0x78024cu));
    m.put32(0x8427e0u,route_bits(route));m.put32(0x8427e4u,0);m.put32(0x8427f0u,0);
    for(std::uint32_t a=0x842710u;a<=0x84271cu;a+=4)m.put32(a,0xffffffffu);
    m.put32(0x842720u,route_bits(route));
    m.put32(0x842724u,0xffffffffu);m.put32(0x8426e4u,0x78u);
    const std::uint32_t score=call(c,0x4b99d0u,{m.u32(0x799d18u)});
    const std::int32_t choice=std::int8_t(call(c,0x48b140u,{}));
    const std::uint32_t time=call(c,0x451180u,{0u});
    const std::int8_t gear=std::int8_t(call(c,0x48b180u,{}));
    std::int8_t colour=std::int8_t(call(c,0x48b1a0u,{}));
    if(colour<0)colour=0;
    m.put32(0x8427ecu,std::uint32_t(std::int32_t(colour)));m.put32(0x8427e8u,std::uint32_t(std::int32_t(gear)));
    m.put32(0x84270cu,m.u32(0x5c62acu+std::uint32_t(gear+(colour+std::int32_t(m.u32(0x8426ecu))*2)*2)*4u));
    m.put32(0x8426d8u,0xffffffffu);m.put32(0x8426dcu,std::uint32_t(route));
    call(c,0x47ee70u,{0x842728u});
    bool missing=false;
    m.put32(0x8427dcu,0);
    const std::uint32_t preset=m.u32(0x842708u);
    if(preset!=2u&&preset!=3u){
        // The five goals of the route: their stage (450250) and whether the stage after
        // is the left (1) or right (-1) branch, 0 for the first, 2 for the last.
        std::uint32_t stage=0;
        for(std::uint32_t level=0;level<=4u;++level){
            const std::uint32_t side=call(c,0x451350u,{level});
            const std::uint32_t from=level?stage:0u;
            stage=call(c,0x450250u,{from,side});
            if(m.u32(0x842728u+level*4u))continue;
            missing=true;
            const std::uint32_t k=m.u32(0x8427dcu);
            m.put32(0x842764u+k*4u,call(c,0x44dc50u,{from}));
            const std::uint32_t at=0x8427a0u+m.u32(0x8427dcu)*4u;
            if(level==4u)m.put32(at,2u);
            else if(side==1u)m.put32(at,0);
            else m.put32(at,side==0u?1u:0xffffffffu);
            m.put32(0x8427dcu,m.u32(0x8427dcu)+1u);
        }
    }else{
        for(std::uint32_t s=0;s<=0xfu;++s){
            if(m.u32(0x842728u+s*4u))continue;
            missing=true;
            const std::uint32_t k=m.u32(0x8427dcu);
            m.put32(0x842764u+k*4u,call(c,0x44dc50u,{s}));
            m.put32(0x8427a0u+m.u32(0x8427dcu)*4u,2u);
            m.put32(0x8427dcu,m.u32(0x8427dcu)+1u);
        }
    }
    if(!missing){m.put32(0x842804u,1u);return;}                         // every goal reached: no entry
    // The 842690 row (outside the table): the default name, the run.
    m.put32(0x8426a4u,m.u32(0x5c636cu));m.put32(0x842704u,5u);m.put32(0x842700u,0x370004u);
    m.put16(0x8426a8u,m.u16(0x5c6370u));m.put8(0x8426aau,std::uint8_t(choice));m.put32(0x842698u,score);
    if(variant==2u){
        const std::int32_t n=std::int32_t(call(c,0x45b830u,{})),d=std::int32_t(call(c,0x45b840u,{}));
        m.put32(0x8426a0u,std::uint32_t(std::int32_t(std::int16_t(n/d))));
    }else m.put32(0x8426a0u,0);
    m.put32(0x84269cu,time);m.put8(0x8426acu,std::uint8_t(colour));
    const std::int32_t rank=m.i32(0x8426d8u);
    m.put32(0x8426ccu,0xffffffffu);m.put32(0x8426b0u,std::uint32_t(route));m.put8(0x8426abu,std::uint8_t(gear));m.put32(0x8426d4u,variant);
    const std::int32_t first=rank<0?6:rank<5?0:rank==5?2:5;
    m.put32(0x8426fcu,0xffffffffu);m.put32(0x8422a8u,0xffffffffu);m.put32(0x8426f0u,std::uint32_t(first));
    m.put32(0x8426f8u,0);m.put32(0x8422a4u,0);m.put32(0x8423b0u,0);
    m.put32(0x8423b4u,m.u32(0x5c633cu));m.put8(0x8423b8u,m.u8(0x5c6340u));
    m.put32(0x8427fcu,0);m.put32(0x842800u,0);
    call(c,0x4249f0u,{0x158u});
    const std::uint32_t p=m.u32(0x842708u);
    const std::uint32_t t=0x5c5d18u+(m.u32(0x84270cu)+p*6u)*16u;     // {back, title, ...}
    if(p!=2u&&p!=3u){
        if(m.i32(0x842714u)<0)m.put32(0x842714u,call(c,0x428460u,{m.u32(t+4u),4u,1u,0u,0x1eu}));
        if(m.i32(0x842710u)<0)m.put32(0x842710u,call(c,0x428320u,{m.u32(t),4u,1u}));
    }else{
        if(m.u32(0x8427e4u)==0u&&m.i32(0x842714u)<0)m.put32(0x842714u,call(c,0x428460u,{m.u32(t+4u),4u,1u,0u,0x1eu}));
        if(m.i32(0x842710u)<0)m.put32(0x842710u,call(c,0x428460u,{m.u32(t),4u,1u,0u,0x14u}));
    }
}
void name_entry2_control_4b42c0(PcRaceContext& c){
    auto& m=c.m;
    const std::uint32_t e=m.u32(0x842704u)*24u;
    switch(m.u32(0x8427fcu)){
    case 0:                                                              // the background in
        if(call(c,0x428880u,{m.u32(0x842714u)})!=3u)return;
        m.put32(0x8427fcu,1u);
        [[fallthrough]];
    case 1:
        board_open_4b0cc0(c);
        m.put32(0x8427fcu,m.u32(0x8427fcu)+1u);
        [[fallthrough]];
    case 2:{                                                             // the board opening
        std::uint32_t frame=call(c,0x428840u,{m.u32(0x8422b8u)});      // x87 result as float bits
        if(call(c,0x428880u,{m.u32(0x8422b8u)})!=3u){
            float f;std::memcpy(&f,&frame,4);
            if(f==m.f32(0x5c5c08u+m.u32(0x842704u)*4u)){board_sprites_4b0d80(c);return;}
            if(m.i32(0x8422b4u)>=0&&call(c,0x428880u,{m.u32(0x8422b4u)})==3u){
                call(c,0x4285a0u,{m.u32(0x8422b4u)});
                m.put32(0x8422b4u,call(c,0x428460u,{m.u32(0x8427f8u),0xbu,0u,0x1fu,0x96u}));
            }
            return;
        }
        call(c,0x4285a0u,{m.u32(0x8422b8u)});
        m.put32(0x8422b8u,call(c,0x428460u,{m.u32(0x842700u),4u,0u,m.u32(0x5c5b80u+e),m.u32(0x5c5b84u+e)}));
        call(c,0x4285a0u,{m.u32(0x8422b0u)});
        m.put32(0x8422b0u,call(c,0x428460u,{0x360007u,6u,0u,0x33u,0x34u}));
        m.put32(0x8427fcu,m.u32(0x8427fcu)+1u);
        m.put32(0x8423b0u,1u);
        [[fallthrough]];}
    case 3:                                                              // typing
        if(!board_input_4b1010(c))return;
        call(c,0x4285a0u,{m.u32(0x8422a8u)});call(c,0x4285a0u,{m.u32(0x8422b8u)});
        m.put32(0x8422b8u,call(c,0x428460u,{m.u32(0x842700u),4u,1u,m.u32(0x5c5b80u+e),m.u32(0x5c5b84u+e)}));
        call(c,0x4285a0u,{m.u32(0x8422b4u)});
        m.put32(0x8422b4u,call(c,0x428460u,{m.u32(0x8427f8u),0xbu,1u,0x96u,0xb3u}));
        m.put32(0x8427fcu,m.u32(0x8427fcu)+1u);
        [[fallthrough]];
    case 4:                                                              // the board closing
        if(call(c,0x428880u,{m.u32(0x8422b8u)})!=3u)return;
        m.put32(0x8423b0u,0);m.put32(0x8427fcu,m.u32(0x8427fcu)+1u);
        return;
    case 5:
        board_commit_4b3a50(c);
        m.put32(0x8427fcu,m.u32(0x8427fcu)+1u);
        if(m.u32(0x780258u)==0u)call(c,0x480d00u,{0x7c23e0u});
        [[fallthrough]];
    case 6:
        if(call(c,0x428880u,{m.u32(0x8422b8u)})!=3u)return;
        call(c,0x4285a0u,{m.u32(0x8422b8u)});
        m.put32(0x8427fcu,m.u32(0x8427fcu)+1u);m.put32(0x8422b8u,0xffffffffu);
        return;
    case 7:m.put32(0x842804u,1u);return;
    default:return;
    }
}
void name_entry2_display_4b4550(PcRaceContext& c){
    auto& m=c.m;
    if(!m.u32(0x8423b0u))return;
    // The letter wheel: turning 4 frames toward the cursor's letter (5C5378 frames).
    const std::uint32_t at=m.u32(0x84229cu);
    std::uint32_t turn;
    if(at!=m.u32(0x8422a0u)){turn=4u;m.put32(0x8422a4u,4u);m.put32(0x8422a0u,at);}
    else turn=m.u32(0x8422a4u);
    if(turn){
        std::int32_t frame=std::int32_t(m.u16(0x5c5378u+at*2u))-m.i32(0x842398u)*std::int32_t(turn);
        if(frame<0)frame+=0xfa;
        call(c,0x4288c0u,{m.u32(0x8422a8u),std::uint32_t(frame)});
        m.put32(0x8422a4u,m.u32(0x8422a4u)-1u);
    }else call(c,0x4288c0u,{m.u32(0x8422a8u),m.u16(0x5c5378u+at*2u)});
    // The typed letters.
    for(std::int32_t i=0;i<m.i32(0x842298u);++i){
        const std::uint32_t letter=m.u32(0x842288u+std::uint32_t(i)*4u);
        if(letter==0x2fu)continue;
        const std::uint32_t token=m.u32(0x5c52b8u+letter*4u);
        const float x=m.f32(0x5c5438u+std::uint32_t(i)*8u),y=m.f32(0x5c543cu+std::uint32_t(i)*8u);
        call(c,0x42cc00u,{std::uint32_t(cvtt(x)),std::uint32_t(cvtt(y))});
        std::array<std::uint8_t,0x48> rec{};                              // 42A070
        const float one=1.0f;std::memcpy(rec.data()+0x14,&one,4);std::memcpy(rec.data()+0x18,&one,4);
        std::memset(rec.data()+0x2c,0xff,4);
        const auto mark=m.mark();m.map(NameEntry2Image,rec.data(),rec.size());
        call(c,0x42c2f0u,{NameEntry2Image,token});
        m.putf(NameEntry2Image+0x24u,x);m.putf(NameEntry2Image+0x28u,y);
        call(c,0x42cfe0u,{NameEntry2Image,0x40e00000u});
        m.release(mark);
    }
    std::int32_t rank=m.i32(0x8426d8u);if(rank<0)rank=10;
    const std::uint32_t row=0x8423c0u+std::uint32_t(rank)*0x48u;
    switch(m.u32(row+0x44u)){
    case 0:board_time_4b3770(c,row);break;
    case 1:board_score_4b3410(c,row);break;
    case 2:board_count_4b35d0(c,row);break;
    default:break;
    }
    // The countdown in seconds.
    Locals l(m);
    put_string(m,Locals::Text,format_int(0x5c2120u,m.i32(0x842394u)/60));
    call(c,0x4bc990u,{0u,0x230u,0x24u,Locals::Text});
    call(c,0x42ccb0u,{4u});
}
}
