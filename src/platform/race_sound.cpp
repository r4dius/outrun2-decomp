#include "system/exe_image.hpp"
#include "platform/race_sound.hpp"
#include "driving/pc_d3dx.hpp"
#include <cmath>
#include <limits>
// Line-by-line port of the PC race sound manager (event 383) and COMM_TRANS
// (event 360); see race_sound.hpp. Branch comments name the PC addresses.
// x87 code uses driving::X87 (24-bit precision control in game), stores
// round to float, _ftol2 truncates to the low dword of the 64-bit result.
namespace outrun::platform {
namespace {
using driving::X87;
constexpr std::uint32_t kCache=0x955ae0u,kQueue=0x955ca0u,kW=RaceSoundState::ics_write_955c78,kR=RaceSoundState::ics_read_956140;
// .rdata tables (verified against the EXE by the oracle through every path).
// 623EA4..624037: ICS codes 623EA4, voice set-up 623EF8, surfaces 623F38,
// walls 623FB0, car SE 623FF8, passing SE 624014.
alignas(4) static std::uint32_t k623ea4[101]{}; OR2_EXE_COPY(k623ea4,0x623EA4u,0x194u);
alignas(4) static std::uint32_t k624038[128]{}; OR2_EXE_COPY(k624038,0x624038u,0x200u);
// 624238: default balance (7 floats); 624258: three option balances.
alignas(4) static std::uint32_t k624238[29]{}; OR2_EXE_COPY(k624238,0x624238u,0x74u);
std::uint32_t rd623(std::uint32_t pc){
    if(pc<0x623ea4u||pc>=0x624038u||(pc&3u))throw std::out_of_range("race sound table read outside 623EA4..624037");
    return k623ea4[(pc-0x623ea4u)/4u];}
float fb(std::uint32_t u){float f;std::memcpy(&f,&u,4);return f;}
std::uint32_t bf(float f){std::uint32_t u;std::memcpy(&u,&f,4);return u;}
float rpm_threshold(std::uint32_t i){return fb(k624038[i]);}
// float constants (EXE .rdata address in the name)
const float c628100=fb(0x41f00000u),c628104=fb(0x42fe0000u),c628108=fb(0x3ccccccdu),c62810c=fb(0x43160000u),
    c628110=fb(0x4270cccdu),c628114=fb(0x427c0000u),c628118=fb(0x3f19999au),c62811c=fb(0xbf19999au),c628120=fb(0xbecccccdu),
    c628124=fb(0x3e8a3d71u),c628128=fb(0x3d4ccccdu),c62812c=fb(0x3c881469u),c628130=fb(0x3b800000u),c628134=fb(0x42700000u),
    c628138=fb(0x40a00000u),c62813c=fb(0x3dcccccdu),c628140=fb(0x3b5a740eu),c628144=fb(0x3fcb3333u),c628148=fb(0x42a00000u),
    c6280ec=fb(0x3ca3d70au),c6280f4=fb(0x428c0000u),c6280f8=fb(0x3ba3d70au),c6280fc=fb(0x438f8000u),c628070=fb(0x4f800000u),
    c62806c=fb(0x3f800000u),c628064=fb(0x3f000000u),c6280bc=fb(0x3b808081u),c6280c4=fb(0xbf800000u),c6281ac=fb(0x3c010204u),
    c6281c0=fb(0x3c23d70au),c6281d4=fb(0x42800000u),c6281d8=fb(0x3b4e69a0u),c6281dc=fb(0x3f8ccccdu),c6281e0=fb(0x44400000u),
    c6281e4=fb(0x3daaaaabu),c6281e8=fb(0x41400000u),c6281ec=fb(0x40200000u),c6281f0=fb(0x34000000u),c6281f4=fb(0x43aa0000u),
    c6281f8=fb(0x42780000u),c6281fc=fb(0x42fc0000u),c628200=fb(0x43173334u),c628204=fb(0x3a83126fu),c628208=fb(0x3f7fbe77u),
    c62820c=fb(0x42f00000u),c628218=fb(0x3d23d70au),c62821c=fb(0x3f4ccccdu),c628220=fb(0x424e0000u),c628224=fb(0x4604d000u),
    c628228=fb(0x3fb70a3du),c62822c=fb(0x3d75c28fu),c628240=fb(0x46034000u),c628244=fb(0x41200000u),c628248=fb(0x41000000u),
    c62824c=fb(0x40490fd8u),c628250=fb(0x3d0efa2eu),c6282b0=fb(0x3b83126fu),c5b0058=fb(0x3ecccccdu),c5b0068=fb(0x3e4ccccdu),
    c619a34=0.0f,c5c68e0=fb(0x42dc0000u);
double db(std::uint64_t u){double d;std::memcpy(&d,&u,8);return d;}
const double d628210=db(0x3fe99999a8000000ull),d628230=db(0x3fd0000000000000ull),d628238=db(0x3fd3333340000000ull);

std::int32_t ftol(X87 v){return std::int32_t(std::uint32_t(std::uint64_t(driving::x87_ftol64(v))));}   // 582194
float st(X87 v){return driving::x87_float(v);}

// ICS queue (955CA0) and parameter cache (955AE0).
std::uint32_t cache(std::uint32_t ch,std::uint32_t p){return kCache+(ch*6u+p)*4u;}
bool full(const RaceSoundState& s){return s.u32(kW)==s.u32(kR);}
void qpush(RaceSoundState& s,std::uint32_t ch,std::uint32_t p,std::uint32_t v){
    auto w=s.u32(kW);const auto e=kQueue+w*12u;
    s.put32(e,ch);s.put32(e+4,p);s.put32(e+8,v);++w;s.put32(kW,w==0x58u?0u:w);}
// Inline "cache differs -> store and queue" block (caller checked !full).
void setc(RaceSoundState& s,std::uint32_t ch,std::uint32_t p,std::uint32_t v){
    if(s.u32(cache(ch,p))!=v){s.put32(cache(ch,p),v);qpush(s,ch,p,v);}}

// World objects by PC address.
struct Obj {
    const std::uint8_t* p;std::size_t n;
    void chk(std::size_t o,std::size_t k)const{if(o+k>n)throw std::out_of_range("race sound object field outside view");}
    std::uint32_t u32(std::size_t o)const{chk(o,4);std::uint32_t v;std::memcpy(&v,p+o,4);return v;}
    std::int32_t i32(std::size_t o)const{return std::int32_t(u32(o));}
    float f32(std::size_t o)const{chk(o,4);float v;std::memcpy(&v,p+o,4);return v;}
    std::uint16_t u16(std::size_t o)const{chk(o,2);std::uint16_t v;std::memcpy(&v,p+o,2);return v;}
    std::int16_t i16(std::size_t o)const{return std::int16_t(u16(o));}
    std::uint8_t u8(std::size_t o)const{chk(o,1);return p[o];}
};
Obj object(const RaceSoundWorld& w,std::uint32_t pc,std::size_t need){
    for(const auto& v:w.event_work)if(v.size&&pc>=v.pc&&pc-v.pc<v.size)return {v.data+(pc-v.pc),v.size-(pc-v.pc)};
    for(const auto* v:{&w.car_82e7f0,&w.params})if(v->size&&pc>=v->pc&&pc-v->pc<v->size)return {v->data+(pc-v->pc),v->size-(pc-v->pc)};
    (void)need;throw std::out_of_range("race sound: PC object not provided by the integrator");
}
std::uint32_t player_pc(const RaceSoundWorld& w){
    if(!w.event_work[8].size)throw std::out_of_range("race sound: player work 799D18 not provided");
    return w.event_work[8].pc;}

// rpm threshold search of 4252E0 / 426B10 (624038, first entry above).
std::uint32_t rpm_index(float v){
    std::uint32_t i=0;for(;i<0x80u;++i)if(v<rpm_threshold(i))break;
    return i;}

// CRT helpers.
bool isnan3(const RaceSoundState& s,std::uint32_t a){        // 426AC0 on the state block
    return std::isnan(s.f32(a))||std::isnan(s.f32(a+4))||std::isnan(s.f32(a+8));}
X87 fsin(float v){return driving::x87_sin(X87(v));}             // 449360
X87 ln(float v);                                                // FLDLN2; FYL2X
X87 ln2_constant();                                             // FLD m64 2.0; FLDLN2; FYL2X
}

// ============================================================================
// x87 transcendental helpers (not affected by the precision control; the
// arithmetic between them is).
#if !OR2_X87_FAST && (defined(__i386__) || defined(__x86_64__))
namespace {
X87 ln(float v){long double r,x=v;__asm__("fldln2\n\tfxch %%st(1)\n\tfyl2x":"=t"(r):"0"(x));return X87(r);}
X87 ln2_constant(){long double r,x=2.0L;__asm__("fldln2\n\tfxch %%st(1)\n\tfyl2x":"=t"(r):"0"(x));return X87(r);}
}
driving::X87 race_sound_pow_5826d0(double x,double y,bool& fault){
    // 58272D (x87 path; 98BB20 == 0 as on every non-P4 Intel CPU and in the
    // oracle): CW = (CW & 0x300) | 0x7F, FYL2X, then 5888C0:
    // FLD ST; FRNDINT; FSUBR ST(1),ST; FXCH; FCHS; F2XM1; FLD1; FADDP; FSCALE.
    // (r - n) is computed here as ST - ST(1) after FXCH, the sign-symmetric
    // rounding of FSUBR + FCHS.
    fault=false;
    if(x==0.0){if(!(y>0.0))fault=true;return X87(0.0);}          // 582863 -> 5828B3 FLDZ (y > 0, not an odd integer)
    if(!(x>0.0)||!std::isfinite(x)||!std::isfinite(y)){fault=true;return X87(std::numeric_limits<double>::quiet_NaN());}
    const std::uint16_t cw=driving::x87_control_word();std::uint16_t saved;long double r;
    const long double lx=x,ly=y;
    __asm__ volatile("fnstcw %0":"=m"(saved));__asm__ volatile("fldcw %0"::"m"(cw));
    __asm__ volatile("fyl2x\n\t"
                     "fld %%st(0)\n\tfrndint\n\tfxch %%st(1)\n\tfsub %%st(1),%%st\n\t"
                     "f2xm1\n\tfld1\n\tfaddp\n\tfscale\n\tfstp %%st(1)"
                     :"=t"(r):"0"(lx),"u"(ly):"st(1)");
    __asm__ volatile("fldcw %0"::"m"(saved));
    return X87(r);
}
#else
namespace {
// Double libm: the 64-bit x87 results are only consumed by 24-bit arithmetic.
X87 ln(float v){return X87(std::log(double(v)));}
X87 ln2_constant(){return X87(0.69314718055994530942);}
}
driving::X87 race_sound_pow_5826d0(double x,double y,bool& fault){
    fault=false;
    if(x==0.0){if(!(y>0.0))fault=true;return X87(0.0);}
    if(!(x>0.0)||!std::isfinite(x)||!std::isfinite(y)){fault=true;return X87(std::numeric_limits<double>::quiet_NaN());}
    const double r=y*std::log2(x),n=std::nearbyint(r);
    const X87 f=X87(r)-X87(n);                                  // FSUBR/FCHS under the precision control
    const X87 t=X87(std::exp2(double(f))-1.0);                  // F2XM1
    const X87 u=X87(1.0)+t;                                     // FLD1; FADDP
    return X87(std::ldexp(double(u),int(n)));                   // FSCALE
}
#endif

const std::uint8_t* RaceSoundWorld::resolve(std::uint32_t pc,std::size_t n)const{
    for(const auto& v:event_work)if(v.size&&pc>=v.pc&&n<=v.size&&pc-v.pc<=v.size-n)return v.data+(pc-v.pc);
    for(const auto* v:{&car_82e7f0,&params})if(v->size&&pc>=v->pc&&n<=v->size&&pc-v->pc<=v->size-n)return v->data+(pc-v->pc);
    return nullptr;
}

// ============================================================================
// 427A20: ICS volume scale (EAX channel, [ESP+4] value).
std::uint32_t race_sound_volume_427a20(const RaceSoundState& s,std::uint32_t ch,std::int32_t value){
    static constexpr std::uint8_t idx[15]={0,0,4,1,4,4,2,4,3,3,3,3,3,3,3};
    static constexpr std::uint32_t cell[5]={0x9560f4u,0x9560f8u,0x956100u,0x9560fcu,0x956104u};
    const float f=s.f32(ch>0xeu?0x956104u:cell[idx[ch]]);
    const auto r=ftol(X87(value)*X87(f));
    return r<0?0u:(r>0x7f?0x7fu:std::uint32_t(r));
}
// 4247B0 SetIcsQueue: EDI channel, EBX parameter, EAX value (ECX = [956140]
// loaded by the protected entry).
void race_sound_set_ics_4247b0(RaceSoundState& s,std::uint32_t ch,std::uint32_t p,std::uint32_t v){
    if(s.u32(kW)==s.u32(kR))return;
    if(p==0)v=race_sound_volume_427a20(s,ch,std::int32_t(v));
    setc(s,ch,p,v);
}
// 424820 FlushIcsQueue: up to 16 entries to 42EFF0(ch, value, 623EA4[param]).
void race_sound_flush_ics_424820(RaceSoundState& s,RaceSoundServices& sv){
    auto r=s.u32(kR);
    for(unsigned n=0;n<16;++n){
        ++r;if(r==0x58u)r=0;
        if(r==s.u32(kW))return;
        const auto e=kQueue+r*12u;const auto p=s.u32(e+4),v=s.u32(e+8),ch=s.u32(e);
        sv.pc_42eff0(ch,v,rd623(0x623ea4u+p*4u));
        s.put32(kR,r);
    }
}
// 424880: SE queue 9563E8 = -1 x 32, write 0, read 0x1F.
void race_sound_init_se_queue_424880(RaceSoundState& s){
    for(std::uint32_t a=0x9563e8u;a<0x956468u;a+=4)s.put32(a,0xffffffffu);
    s.put32(0x956124u,0);s.put32(0x9560c0u,0x1fu);
}
// 424940 SetSndQueue.
void race_sound_set_se_424940(RaceSoundState& s,const RaceSoundWorld& w,std::uint32_t cmd){
    const auto f=w.flags(0x17f);
    if((f&3u)!=2u)return;
    if(f&0x10u)return;                                         // 440A50(0x17F)
    auto e=s.u32(0x956124u);
    if(e==s.u32(0x9560c0u))return;
    for(std::uint32_t a=0x9563e8u;a<0x956468u;a+=4)if(s.u32(a)==cmd)return;
    s.put32(0x9563e8u+e*4u,cmd);++e;s.put32(0x956124u,e==0x20u?0u:e);
}
// 4249A0 FlushSndQueue: up to 6 commands to 42F0D0.
void race_sound_flush_se_4249a0(RaceSoundState& s,RaceSoundServices& sv){
    auto r=s.u32(0x9560c0u);
    for(unsigned n=0;n<6;++n){
        ++r;if(r==0x20u)r=0;
        if(r==s.u32(0x956124u))return;
        sv.pc_42f0d0(s.u32(0x9563e8u+r*4u));
        s.put32(0x9563e8u+r*4u,0xffffffffu);s.put32(0x9560c0u,r);
    }
}
// 424A00 InitCarSoundWork (CarSound 956148).
void race_sound_init_car_424a00(RaceSoundState& s){
    for(std::uint32_t a:{0x956168u,0x95619cu,0x9561b4u,0x9561ccu,0x9561e4u})s.put32(a,0);
    for(std::uint32_t a=0x956218u;a<0x956378u;a+=0x2cu)s.put32(a,0);
    for(std::uint32_t a=0x956378u;a<0x9563d8u;a+=4)s.put32(a,0);
    s.put32(0x9563d8u,0xffffffffu);s.put32(0x9563dcu,0);s.put32(0x9563e0u,0);s.put32(0x9563e4u,0);
}
// 424B10: ClearAllSound, then parameter 2 of every channel = 0 in the cache.
void race_sound_pause_clear_424b10(RaceSoundState& s,RaceSoundServices& sv){
    sv.pc_427630();
    for(std::uint32_t a=0x955ae8u;a<0x955c80u;a+=0x18u)s.put32(a,0);
}
// 4279C0: balance 9560D0 from 624238 or 624258[option], copied to 9560F0.
void race_sound_balance_4279c0(RaceSoundState& s,const RaceSoundWorld& w,RaceSoundServices& sv){
    std::uint32_t first=0;
    if(w.mode_78026c==0xd||w.mode_78026c==0x10){
        auto al=std::int8_t(sv.pc_48b1c0());
        if(al<0||al>2)al=1;
        first=8u+std::uint32_t(al)*7u;
    }
    for(unsigned k=0;k<7;++k)s.put32(0x9560d0u+4*k,k624238[first+k]);
    for(unsigned k=0;k<7;++k)s.put32(0x9560f0u+4*k,s.u32(0x9560d0u+4*k));
}
// 427B10 (music select preview): the balance 9560F0 from 624258 + option*0x1C (option is
// 48B1C0, read by the caller), then channel 0/1 parameter 2 = 0x73 or 0 and the volumes
// (parameter 0) ftol([9560F4]*19.0 or *0.0) clamped to 0..7F; each stops on a full queue.
void race_sound_preview_427b10(RaceSoundState& s,std::int8_t option,bool held){
    const std::int32_t first=8+std::int32_t(option)*7;
    if(first<0||first+7>29)throw std::out_of_range("427B10 option outside 624238 table");
    for(unsigned k=0;k<7;++k)s.put32(0x9560f0u+4*k,k624238[first+k]);
    auto vol=[&](float f){const auto r=ftol(X87(s.f32(0x9560f4u))*X87(f));return r<0?0u:(r>0x7f?0x7fu:std::uint32_t(r));};
    const std::uint32_t mode=held?0x73u:0u;
    const float f0=held?19.0f:0.0f;   // 6281D0 / 619A34
    if(full(s))return;setc(s,0,2,mode);
    if(full(s))return;setc(s,1,2,mode);
    if(full(s))return;setc(s,0,0,vol(f0));
    if(full(s))return;setc(s,1,0,vol(0.0f));
}
// 427320: ICS voice selection (param 5) and silence of every channel.
void race_sound_voices_427320(RaceSoundState& s){
    const auto r=s.u32(kR);
    if(!full(s)){
        setc(s,0,5,0x200u);
        if(!full(s))setc(s,1,5,0x201u);
    }
    for(std::uint32_t ch=2;ch<17;++ch)
        if(!full(s))setc(s,ch,5,rd623(0x623ef8u+(ch-2)*4u));
    (void)r;
    for(std::uint32_t ch=0;ch<17;++ch){
        if(full(s))continue;
        setc(s,ch,0,race_sound_volume_427a20(s,ch,0));
        if(full(s))continue;
        setc(s,ch,2,0);
        if(full(s))continue;
        setc(s,ch,1,0);
        if(full(s))continue;
        setc(s,ch,4,0);
    }
}

// ============================================================================
// Stage predicates on a car work (its +0x68 stage key through 44DC50).
std::uint32_t race_sound_stage_424b50(const RaceSoundWorld& w,RaceSoundServices& sv,std::uint32_t work){
    if(sv.pc_48b310()&&sv.pc_48b350())return 1;
    const auto v=sv.pc_44dc50(object(w,work,0x6c).u32(0x68));
    return (v==0xc||v==0x19)?1u:0u;
}
std::uint32_t race_sound_stage_424b90(const RaceSoundWorld& w,RaceSoundServices& sv,std::uint32_t work){
    if(sv.pc_48b310()&&sv.pc_48b350())return 1;
    const auto v=std::uint32_t(sv.pc_44dc50(object(w,work,0x6c).u32(0x68)))-12u;
    return (v==0||v==13||v==16||v==17)?1u:0u;
}
std::uint32_t race_sound_stage_424bf0(const RaceSoundWorld& w,RaceSoundServices& sv,std::uint32_t work){
    const auto v=std::uint32_t(sv.pc_44dc50(object(w,work,0x6c).u32(0x68)));
    return (v==0||v==15||v==60||v==61)?1u:0u;
}
std::uint32_t race_sound_stage_424c60(const RaceSoundWorld& w,RaceSoundServices& sv,std::uint32_t work){
    const auto v=sv.pc_44dc50(object(w,work,0x6c).u32(0x68));
    return (v==0x1e||v==0x2d)?1u:0u;
}

// 424C80: cheer volume targets 95B208 / 95B20C.
void race_sound_cheer_targets_424c80(RaceSoundState& s,RaceSoundWorld& w,RaceSoundServices& sv){
    if((w.flags(8)&3u)!=2u)return;
    const auto mode=std::uint32_t(w.mode_78026c)-0xdu;
    if(mode>0xbu)return;
    const std::uint32_t e=player_pc(w);
    switch(mode){
    case 0:                                                     // 424CB9 mode 0x0D
        if(race_sound_stage_424bf0(w,sv,e)){
            if(s.u32(0x956114u))return;
            s.put32(0x956114u,1);s.cheer_95b208=0x7f;s.cheer_95b20c=0x50;return;}
        s.cheer_95b208=0;s.cheer_95b20c=0;return;
    case 3:{                                                    // 424D09 mode 0x10
        const Obj E=object(w,e,0x6c);
        std::int32_t ebp=0;std::uint32_t ebx=0;
        if(sv.pc_44c940(E.u32(0x68))!=0){
            // 424DF6
            if(sv.pc_44b7b0(0)&&object(w,e,0x60).u32(0x5c)==0){
                if(race_sound_stage_424b50(w,sv,e))goto done;
                ebx=0x50;
                const std::int32_t a=std::int32_t(std::int16_t(sv.pc_44dd60(object(w,e,0x6c).u32(0x68),0)))-0x32;
                const std::int32_t c=object(w,e,0x66).i16(0x64);
                if(a>=c)goto done;
                ebp=ftol((X87(c)-X87(a))*X87(c6280ec)*X87(c628104));
            }else{
                // 424E55
                if(!race_sound_stage_424c60(w,sv,e))goto done;
                ebx=0x50;
                const Obj F=object(w,e,0x6c);
                if(F.u32(0x5c)==0){
                    const std::int32_t a=std::int32_t(std::int16_t(sv.pc_44dd60(F.u32(0x68),0)))-10;
                    const std::int32_t c=F.i16(0x64);
                    if(a>=c)goto done;
                    ebp=ftol((X87(c)-X87(a))*X87(c62813c)*X87(c628104));
                }else{
                    const std::int16_t si=F.i16(0x64);                  // 424E9D
                    if(si>=5)goto done;
                    ebp=ftol((X87(c628138)-X87(std::int32_t(si)))*X87(c5b0068)*X87(c628104));
                }
            }
            goto done;
        }
        if(!race_sound_stage_424bf0(w,sv,e))goto done;
        ebx=0x50;
        {const Obj F=object(w,e,0x6c);
        if(F.u32(0x5c)==0&&F.i16(0x64)<0x50){
            const std::uint32_t frames=sv.pc_450570();
            const std::int32_t t=F.i16(0x64);
            ebp=ftol((X87(c628148)-X87(t))*X87(c628144));
            if(std::int32_t(frames)<=0x4b0||std::int32_t(frames)>=0x5dc)goto done;
            ebp=ftol(X87(std::int32_t(0x5dc-frames))*X87(c628140)*X87(ebp));
            goto done;
        }
        // 424DA4
        if(!sv.pc_44b800())goto done;
        if(object(w,e,0x60).u32(0x5c)!=1)goto done;
        {const std::int32_t a=std::int32_t(std::int16_t(sv.pc_44dd90(object(w,e,0x6c).u32(0x68),1)))-0x14;
        const std::int32_t c=object(w,e,0x66).i16(0x64);
        if(a>=c)goto done;
        ebp=ftol((X87(c)-X87(a))*X87(c628128)*X87(c628104));}}
    done:
        s.cheer_95b208=std::uint32_t(ebp);s.cheer_95b20c=ebx;return;}
    case 6:                                                     // 424EDF mode 0x13
        if(s.u32(0x95611cu))return;
        s.put32(0x95611cu,1);
        if(race_sound_stage_424b50(w,sv,e)){s.cheer_95b208=0;s.cheer_95b20c=0;return;}
        s.cheer_95b208=0x7f;s.cheer_95b20c=0x50;return;
    case 0xb:                                                   // 424F17 mode 0x18
        if(s.u32(0x956120u))return;
        s.put32(0x956120u,1);
        if(race_sound_stage_424b90(w,sv,e)){s.cheer_95b208=0;s.cheer_95b20c=0;return;}
        s.cheer_95b208=0x3f;s.cheer_95b20c=0x50;return;
    default:return;
    }
}
// 424F80 PlayCheer: channel 2 (param 2 from 95B208, volume from 95B20C).
void race_sound_cheer_424f80(RaceSoundState& s,RaceSoundWorld& w,RaceSoundServices& sv){
    std::int32_t v2=0;std::uint32_t v1=0;
    race_sound_cheer_targets_424c80(s,w,sv);
    ++s.counter_98abd0;
    if(s.counter_98abd0&1u)return;
    s.phase_98abd4=st(X87(s.phase_98abd4)+X87(c628250));
    if(s.cheer_95b208){
        const float a=st(X87(s.phase_98abd4)+X87(c62824c));
        auto r=ftol(fsin(a)*X87(c628248)+X87(std::int32_t(s.cheer_95b208)));
        v1=r<0?0u:(r>0x7f?0x7fu:std::uint32_t(r));
    }
    if(s.cheer_95b20c){
        auto r=ftol(fsin(s.phase_98abd4)*X87(c628244)+X87(std::int32_t(s.cheer_95b20c)));
        v2=r<0?0:(r>0x7f?0x7f:r);
    }
    const auto rd=s.u32(kR);
    if(s.u32(kW)==rd)return;
    if(s.u32(0x955b18u)!=v1){qpush(s,2,2,v1);s.put32(0x955b18u,v1);}
    if(s.u32(kW)==rd)return;
    const auto r=ftol(X87(v2)*X87(s.f32(0x956104u)));
    const std::uint32_t c=r<0?0u:(r>0x7f?0x7fu:std::uint32_t(r));
    if(s.u32(0x955b10u)!=c){qpush(s,2,0,c);s.put32(0x955b10u,c);}
}

// ============================================================================
// 4256F0 MakePlcarParam: ECX CarSound, EDX player work, ESI car work 82E7F0.
void race_sound_car_param_4256f0(RaceSoundState& s,const RaceSoundWorld& w,std::uint32_t c,std::uint32_t e_pc){
    const Obj e=object(w,e_pc,0xe6c);
    const Obj b=object(w,w.car_82e7f0.pc,0x624);
    for(std::uint32_t o:{0x20u,0x54u,0x6cu,0x84u,0x9cu})s.put32(c+o+4u,s.u32(c+o));
    s.put32(c+0x18,e_pc+0x70u);
    s.put32(c,e.u32(0x14));s.put32(c+4,e.u32(0x18));s.put32(c+8,e.u32(0x1c));
    s.putf(c+0xc,st(X87(e.f32(0x20))*X87(c628134)));
    s.putf(c+0x10,st(X87(e.f32(0x24))*X87(c628134)));
    s.putf(c+0x14,st(X87(e.f32(0x28))*X87(c628134)));
    {X87 r(e.i32(0x48));if(e.i32(0x48)<0)r=r+X87(c628070);s.putf(c+0x1c,st(r));}
    s.putf(c+0x20,st(X87(e.i32(0x34))*X87(c628130)));
    s.putf(c+0x28,st(X87(e.i32(0x38))*X87(c628130)));
    s.putf(c+0x30,st(X87(e.f32(0x1c4))*X87(c628134)));
    s.put32(c+0x34,e.u32(0x1cc));
    if(e.f32(0x2c8)>c619a34||e.f32(0x2f8)>c619a34)s.put32(c+0x34,0);
    s.put32(c+0x38,e.u8(0x281));
    s.put32(c+0x3c,e.u32(0x2a8));
    s.put32(c+0x40,e.u32(0x208));
    s.put32(c+0x44,object(w,e.u32(0x2b4)+0x10a0u,4).u32(0));
    s.put32(c+0x48,e.u8(0x13));
    s.put32(c+0x4c,e.u32(0xe68));
    s.put32(c+0x50,(b.u32(0x244)>>4)&1u);
    s.put16(c+0x5c,b.u16(0x346));s.put32(c+0x60,b.u32(0x338));s.put32(c+0x64,b.u32(0x33c));s.put32(c+0x68,b.u32(0x330));
    s.put32(c+0x54,e.u32(0x24c));
    s.put16(c+0x74,b.u16(0x43a));s.put32(c+0x78,b.u32(0x42c));s.put32(c+0x7c,b.u32(0x430));s.put32(c+0x80,b.u32(0x424));
    s.put32(c+0x6c,e.u32(0x250));
    s.put16(c+0x8c,b.u16(0x52e));s.put32(c+0x90,b.u32(0x520));s.put32(c+0x94,b.u32(0x524));s.put32(c+0x98,b.u32(0x518));
    s.put32(c+0x84,e.u32(0x254));
    s.put16(c+0xa4,b.u16(0x622));s.put32(c+0xa8,b.u32(0x614));s.put32(c+0xac,b.u32(0x618));s.put32(c+0xb0,b.u32(0x60c));
    s.put32(c+0x9c,e.u32(0x258));
    const auto d=e.i32(0xd9c);
    if(d>0){
        const float v=st(X87(s.f32(c+0x94))-X87(d)*X87(c62812c));
        s.putf(c+0x94,v);s.putf(c+0xac,v);
        s.putf(c+0x90,st(X87(c62806c)-X87(s.f32(c+0x94))));
        s.putf(c+0xa8,st(X87(c62806c)-X87(s.f32(c+0x94))));
    }
    s.put32(c+0xc8,0);s.put32(c+0xcc,0);
    const auto f=e.u32(0x2f0);
    if(!(f&2u))return;
    const auto k=(f>>2)&0x1fu;
    if(k!=2&&k!=5&&k!=15&&k!=12&&k!=17&&k!=18&&k!=19&&k!=13)s.put32(c+0xc8,1);
    const auto m=e.u32(0x2f4);
    switch(((e.u32(0x2f0)>>2)&0x1fu)-1u){
    case 0:if(m==0x70)s.put32(c+0xcc,1);return;
    case 1:if(m==1)s.put32(c+0xcc,3);return;
    case 2:if(m==0x29)s.put32(c+0xcc,6);return;
    case 4:if(m==0x2d)s.put32(c+0xcc,4);return;
    case 6:if(m==0x2f)s.put32(c+0xcc,5);return;
    default:return;
    }
}

// 4252E0 PlayEngine: EAX CarSound, EBX channel A, [ESP+4] channel B.
void race_sound_engine_4252e0(RaceSoundState& s,std::uint32_t c,std::uint32_t a,std::uint32_t b){
    std::uint32_t idx=rpm_index(s.f32(c+0x1c));
    if(std::int32_t(idx)<2)idx=2;else if(std::int32_t(idx)>0x7f)idx=0x7f;
    if(!full(s)){
        setc(s,a,2,idx);
        if(!full(s))setc(s,b,2,idx);
    }
    float A=st(((X87(std::int32_t(idx))*X87(c5b0058))+X87(c628148))*X87(c6281ac)),B=A;
    if(A>c62806c){A=c62806c;B=c62806c;}
    else{
        if(A<c5b0058)A=fb(0x3ecccccdu);
        if(B<c62821c)B=fb(0x3f4ccccdu);
    }
    float C=s.f32(c+0x20);
    if(s.f32(c+0x30)>c628244&&s.f32(c+0x1c)>c628240&&s.u32(c+0x40)==s.u32(c+0x44))C=c62806c;
    bool fault=false;
    A=st(race_sound_pow_5826d0(double(C),d628238,fault)*X87(A));
    if(fault)throw std::domain_error("425535 _CIpow domain (car sound +0x20 outside [0,1])");
    B=st(race_sound_pow_5826d0(double(X87(c62806c)-X87(C)),d628230,fault)*X87(B));
    if(fault)throw std::domain_error("425552 _CIpow domain (1 - car sound +0x20 outside [0,1])");
    const float v34=s.f32(c+0x34);
    X87 t;
    if(v34<c619a34)t=driving::x87_sqrt(X87(std::fabs(v34)));
    else t=X87(v34)*X87(v34);
    const auto e40=s.u32(c+0x40);
    t=t*X87(c62822c);
    if(e40<=1u)t=t*X87(c628228);
    t=t+X87(c62806c);
    if(s.u32(c+0x48)==1u&&s.f32(c+0x1c)>c628224&&e40<s.u32(c+0x44))t=t+t;
    else if(s.f32(c+0x4c)>c619a34)t=t*(X87(s.f32(c+0x4c))+X87(c62806c));
    const float D=st(X87(s.f32(0x9560ccu))*t);
    std::int32_t v=ftol(X87(D)*X87(A)*X87(c628220));if(v>0x7f)v=0x7f;
    if(!full(s))setc(s,a,0,race_sound_volume_427a20(s,a,v));
    v=ftol(X87(D)*X87(B)*X87(c628220));if(v>0x7f)v=0x7f;
    if(!full(s))setc(s,b,0,race_sound_volume_427a20(s,b,v));
}

// 425A40 PlayRoadCondition: EDI (from EAX) channel A, ECX CarSound, [ESP+4] channel B.
void race_sound_road_425a40(RaceSoundState& s,std::uint32_t c,std::uint32_t a,std::uint32_t b){
    X87 sum=x87_abs(X87(s.f32(c+0xa8)))+x87_abs(X87(s.f32(c+0x90)));
    sum=sum+x87_abs(X87(s.f32(c+0x78)));
    sum=sum+x87_abs(X87(s.f32(c+0x60)));
    float V=st(x87_abs((sum*X87(c62821c)+X87(c62806c))*X87(s.f32(c+0x30)))*X87(c628218));
    if(V>c62806c)V=c62806c;
    float g1=c62806c,g2=c62806c;
    std::int32_t snd_a=-1,snd_b=-1;
    {const auto m=s.u32(c+0x54);
    for(unsigned k=0;k<10;++k)if(rd623(0x623f38u+12*k)&m){snd_a=std::int32_t(rd623(0x623f3cu+12*k));g1=fb(rd623(0x623f40u+12*k));break;}}
    {const auto m=s.u32(c+0x6c);
    for(unsigned k=0;k<10;++k)if(rd623(0x623f38u+12*k)&m){snd_b=std::int32_t(rd623(0x623f3cu+12*k));g2=fb(rd623(0x623f40u+12*k));break;}}
    // channel A
    if(snd_a==-1){
        if(!full(s)){
            setc(s,a,0,race_sound_volume_427a20(s,a,0));
            if(!full(s)&&s.u32(cache(a,2))!=0){qpush(s,a,2,0);s.put32(cache(a,2),0);}
        }
    }else{
        if(s.surface_780028!=std::uint32_t(snd_a)){
            const bool f=full(s);s.surface_780028=std::uint32_t(snd_a);
            if(!f){
                setc(s,a,0,race_sound_volume_427a20(s,a,0));
                if(!full(s)&&s.u32(cache(a,2))!=0){qpush(s,a,2,0);s.put32(cache(a,2),0);}
            }
        }
        const float R=st(driving::x87_sqrt(X87(d628210)));
        if(!full(s)){
            setc(s,a,5,std::uint32_t(snd_a));
            if(!full(s)){
                const auto v=ftol(X87(s.f32(0x9560ccu))*X87(R)*X87(g1)*X87(c5c68e0));
                setc(s,a,0,race_sound_volume_427a20(s,a,v));
            }
        }
        const auto v=std::uint32_t(ftol(X87(V)*X87(c62820c)));
        if(!full(s)){
            setc(s,a,2,v);
            if(!full(s)&&s.u32(cache(a,1))!=0xffffffdcu){qpush(s,a,1,0xffffffdcu);s.put32(cache(a,1),0xffffffdcu);}
        }
    }
    // channel B
    if(snd_b==-1){
        if(full(s))return;
        setc(s,b,0,race_sound_volume_427a20(s,b,0));
        if(full(s)||s.u32(cache(b,2))==0)return;
        qpush(s,b,2,0);s.put32(cache(b,2),0);return;
    }
    if(s.surface_780024!=std::uint32_t(snd_b)){
        const bool f=full(s);s.surface_780024=std::uint32_t(snd_b);
        if(!f){
            setc(s,b,0,race_sound_volume_427a20(s,b,0));
            if(!full(s)&&s.u32(cache(b,2))!=0){qpush(s,b,2,0);s.put32(cache(b,2),0);}
        }
    }
    const float R=st(driving::x87_sqrt(X87(d628210)));
    if(!full(s)){
        setc(s,b,5,std::uint32_t(snd_b));
        if(!full(s)){
            const auto v=ftol(X87(s.f32(0x9560ccu))*X87(R)*X87(g2)*X87(c5c68e0));
            setc(s,b,0,race_sound_volume_427a20(s,b,v));
        }
    }
    const auto v=std::uint32_t(ftol(X87(V)*X87(c62820c)));
    if(full(s))return;
    setc(s,b,2,v);
    if(full(s)||s.u32(cache(b,1))==0x24u)return;
    qpush(s,b,1,0x24u);s.put32(cache(b,1),0x24u);
}

// 4260E0 PlaySkid: EDI (from EAX) channel, ECX CarSound.
void race_sound_skid_4260e0(RaceSoundState& s,std::uint32_t c,std::uint32_t ch){
    auto silence=[&]{                                           // 426409
        if(full(s))return;
        setc(s,ch,0,race_sound_volume_427a20(s,ch,0));
        if(full(s)||s.u32(cache(ch,2))==0)return;
        qpush(s,ch,2,0);s.put32(cache(ch,2),0);};
    if(s.f32(c+0x30)<c5b0068){silence();return;}
    if(s.u32(c+0xc8)!=0){silence();return;}
    auto grip=[&](float v){return v<c619a34?driving::x87_sqrt(-X87(v)):X87(v)*X87(c628118);};
    X87 x60=grip(s.f32(c+0x60));
    float a2=st(grip(s.f32(c+0x78)));
    float c2=st(grip(s.f32(c+0x90)));
    float b2=st(grip(s.f32(c+0xa8)));
    if(s.f32(c+0x64)<c628208)x60=x60+(X87(c62806c)-X87(s.f32(c+0x64)));else x60=X87(c619a34);
    if(s.f32(c+0x7c)<c628208)a2=st((X87(c62806c)-X87(s.f32(c+0x7c)))+X87(a2));else a2=0.0f;
    X87 c2x;
    if(s.f32(c+0x94)<c628208)c2x=(X87(c62806c)-X87(s.f32(c+0x94)))+X87(c2);else c2x=X87(c619a34);
    if(s.f32(c+0xac)<c628208)b2=st((X87(c62806c)-X87(s.f32(c+0xac)))+X87(b2));else b2=0.0f;
    float M=st(x60);
    if(x60<X87(a2))M=a2;
    if(X87(M)<c2x)M=st(c2x);
    if(M<b2)M=b2;
    if(M>c62806c)M=c62806c;
    const X87 S=x60+((X87(b2)+c2x)+X87(a2));
    float P;
    if(S<X87(c628204))P=0.0f;
    else P=st(((X87(b2)+X87(a2))/S)*X87(2.0f)-X87(c62806c));
    // (B2 + A2) / S, doubled with FADD ST(0),ST: x + x == 2x exactly.
    const std::uint32_t mask=0xf00402u;
    X87 z=X87(c619a34);
    if(s.u32(c+0x54)&mask)z=X87(c62813c);
    if(s.u32(c+0x6c)&mask)z=z+X87(c62813c);
    if(s.u32(c+0x84)&mask)z=z+X87(c62813c);
    if(s.u32(c+0x9c)&mask)z=z+X87(c62813c);
    if(z>X87(c6281c0)){
        race_sound_set_ics_4247b0(s,ch,0,std::uint32_t(ftol(driving::x87_sqrt(z+X87(c628118))*X87(s.f32(0x9560ccu))*X87(c628200))));
        race_sound_set_ics_4247b0(s,ch,2,std::uint32_t(ftol(X87(M)*X87(c6281fc))));
        race_sound_set_ics_4247b0(s,ch,1,std::uint32_t(ftol(X87(P)*X87(c6281f8))));
        return;
    }
    race_sound_set_ics_4247b0(s,ch,0,0);                        // 42639C
    if(full(s)||s.u32(cache(ch,2))==0)return;
    s.put32(cache(ch,2),0);qpush(s,ch,2,0);
}

// 4264C0 PlayWall: EDI channel, [ESP+4] CarSound.
void race_sound_wall_4264c0(RaceSoundState& s,std::uint32_t c,std::uint32_t ch){
    {const float v=st(X87(s.f32(c+0x29c))-X87(c628128));s.putf(c+0x29c,v);
    if(v<c619a34){s.put32(c+0x29c,0);s.put32(c+0x290,0xffffffffu);}}
    const auto m=s.u32(c+0x3c);
    if(m!=0&&s.f32(c+0x30)>c628124){
        for(unsigned k=0;k<6;++k){
            if(!(rd623(0x623fb0u+12*k)&m))continue;
            float pan;
            switch(s.u32(c+0x38)){
            case 0:pan=c628120;break;
            case 1:pan=c62811c;break;
            case 2:pan=c628118;break;
            case 3:pan=c5b0058;break;
            default:pan=c619a34;break;
            }
            s.put32(c+0x29c,0x3f800000u);
            if(pan<c6280c4)pan=c6280c4;
            else if(pan>c62806c)pan=c62806c;
            s.putf(c+0x298,pan);
            s.put32(c+0x290,rd623(0x623fb4u+12*k));
            s.put32(c+0x294,rd623(0x623fb8u+12*k));
            if(s.u32(c+0x50)!=0)s.put32(c+0x294,0x3f800000u);
            break;
        }
    }
    const auto snd=s.u32(c+0x290);
    if(std::int32_t(snd)<0){
        if(full(s)||s.u32(cache(ch,2))==0)return;
        s.put32(cache(ch,2),0);qpush(s,ch,2,0);return;
    }
    if(!full(s)){
        setc(s,ch,5,snd);
        if(!full(s))setc(s,ch,0,race_sound_volume_427a20(s,ch,ftol(X87(s.f32(c+0x294))*X87(c628104))));
    }
    const auto v=std::uint32_t(ftol(X87(s.f32(c+0x29c))*X87(c628104)));
    if(full(s))return;
    setc(s,ch,2,v);
    const auto p=std::uint32_t(ftol(X87(s.f32(c+0x298))*X87(c628114)));
    if(full(s))return;
    setc(s,ch,1,p);
}

// 4267C0: the two nearest cars of slot pair `pair` (0/1) at CarSound+0xD0+pair*0x58.
void race_sound_near_cars_4267c0(RaceSoundState& s,const RaceSoundWorld& w,std::uint32_t c,std::uint32_t player,std::uint32_t pair){
    std::uint32_t ids[4]={0xffffffffu,0xffffffffu,0xffffffffu,0xffffffffu};
    float dist[4]={fb(0x7f7fffffu),fb(0x7f7fffffu),fb(0x7f7fffffu),fb(0x7f7fffffu)};
    for(std::uint32_t id=9;id<32;++id){
        if((w.flags(id)&3u)!=2u)continue;
        const auto& v=w.event_work[id];
        if(!v.size)throw std::out_of_range("race sound: car event work not provided");
        const Obj car{v.data,v.size};
        if(car.u32(0x31c)!=pair||!(car.u8(4)&1u))continue;
        const auto k=car.u32(0)+object(w,player,4).u32(0)*24u;
        if(!w.distance_802af0||k>=w.distance_count)throw std::out_of_range("race sound: distance table 802AF0 not provided");
        const float d=w.distance_802af0[k];
        unsigned j=0;while(j<4&&!(d<dist[j]))++j;
        if(j==4)continue;
        for(int q=2;q>=int(j);--q){ids[q+1]=ids[q];dist[q+1]=dist[q];}
        ids[j]=v.pc;dist[j]=d;
    }
    const std::uint32_t base=c+0xd0u+pair*0x58u;
    for(unsigned k=0;k<2;++k){
        const auto slot=base+0x2cu*k;const auto car=s.u32(slot);
        unsigned j=0;while(j<2&&car!=ids[j])++j;
        if(j<2){s.putf(slot+0x1c,dist[j]);ids[j]=0xffffffffu;}
        else s.put32(slot,0);
    }
    for(unsigned k=0;k<2;++k){
        const auto slot=base+0x2cu*k;
        if(s.u32(slot)!=0)continue;
        unsigned j=0;while(j<2&&ids[j]==0xffffffffu)++j;
        if(j==2)continue;
        s.put32(slot,ids[j]);s.putf(slot+0x1c,dist[j]);ids[j]=0xffffffffu;
    }
    for(unsigned k=0;k<2;++k){
        const auto slot=base+0x2cu*k;const auto pc=s.u32(slot);
        if(!pc)continue;
        const Obj car=object(w,pc,0x4c);
        s.put32(slot+4,car.u32(0x14));s.put32(slot+8,car.u32(0x18));s.put32(slot+0xc,car.u32(0x1c));
        s.putf(slot+0x10,st(X87(car.f32(0x20))*X87(c628110)));
        s.putf(slot+0x14,st(X87(car.f32(0x24))*X87(c628110)));
        s.putf(slot+0x18,st(X87(car.f32(0x28))*X87(c628110)));
        {X87 r(car.i32(0x48));if(car.i32(0x48)<0)r=r+X87(c628070);s.putf(slot+0x24,st(r));}
        s.putf(slot+0x20,st(X87(car.i32(0x34))*X87(c6280bc)));
    }
}

// 426B10 PlayEnCar: EDI (from EAX) channel; args CarSound, slot, range, gain.
void race_sound_enemy_426b10(RaceSoundState& s,const RaceSoundWorld& w,std::uint32_t ch,std::uint32_t c,std::uint32_t slot,float range,float gain){
    if(s.u32(slot)==0||s.f32(slot+0x1c)>range){               // 426EA0
        if(full(s))return;
        setc(s,ch,0,race_sound_volume_427a20(s,ch,0));
        if(full(s)||s.u32(cache(ch,2))==0)return;
        qpush(s,ch,2,0);s.put32(cache(ch,2),0);return;
    }
    if(isnan3(s,c)||isnan3(s,c+0xc)||isnan3(s,slot+4)||isnan3(s,slot+0x10))return;
    driving::PcVec3 d{st(X87(s.f32(slot+4))-X87(s.f32(c))),st(X87(s.f32(slot+8))-X87(s.f32(c+4))),st(X87(s.f32(slot+0xc))-X87(s.f32(c+8)))};
    d=driving::pc_d3dx_vec3_normalize(d);
    const auto dot=[&](std::uint32_t v){
        return ((X87(d[1])*X87(s.f32(v+4))+X87(d[2])*X87(s.f32(v+8)))+X87(d[0])*X87(s.f32(v)))+X87(c6281f4);};
    const X87 num=dot(c+0xc),den=dot(slot+0x10);
    const float R=st(num/den);
    X87 L;
    if(x87_abs(X87(R))<X87(c6281f0))L=X87(c619a34);
    else L=ln(R)/ln2_constant();
    L=L*X87(c6281ec);
    std::int32_t v1=ftol(X87(c6281e8)*L);
    if(v1<-64)v1=-64;else if(v1>63)v1=63;
    std::int32_t v2=ftol((L-X87(v1)*X87(c6281e4))*X87(c6281e0));
    if(v2<-64)v2=-64;else if(v2>63)v2=63;
    race_sound_set_ics_4247b0(s,ch,3,std::uint32_t(v1));
    race_sound_set_ics_4247b0(s,ch,4,std::uint32_t(v2));
    std::uint32_t idx=rpm_index(s.f32(slot+0x24));
    if(std::int32_t(idx)<1)idx=1;else if(std::int32_t(idx)>0x7f)idx=0x7f;
    race_sound_set_ics_4247b0(s,ch,2,idx);
    {const Obj m=object(w,s.u32(c+0x18),12);
    std::int32_t p=ftol((((X87(d[1])*X87(m.f32(4)))+X87(d[2])*X87(m.f32(8)))+X87(d[0])*X87(m.f32(0)))*X87(c6281d4));
    if(p<-64)p=-64;else if(p>63)p=63;
    race_sound_set_ics_4247b0(s,ch,1,std::uint32_t(p));}
    X87 vol=(X87(c62806c)-X87(s.f32(slot+0x1c))/X87(range))*X87(c6281dc);
    if(vol<X87(c619a34))vol=X87(c619a34);
    else if(vol>X87(c62806c))vol=X87(c62806c);
    const X87 t=driving::x87_sqrt((X87(s.f32(slot+0x20))+X87(c62806c))*X87(c628064));
    X87 x=(t*((vol*vol)*(X87(std::int32_t(idx))*X87(c6281d8)+X87(c628118))))*X87(gain);
    if(x<X87(c6281c0))x=X87(c619a34);
    else if(x>X87(c62806c))x=X87(c62806c);
    race_sound_set_ics_4247b0(s,ch,0,std::uint32_t(ftol(x*X87(c628104))));
}

// 426F50: one passing-car SE per frame (args player work, CarSound).
void race_sound_passing_426f50(RaceSoundState& s,const RaceSoundWorld& w,RaceSoundServices& sv,std::uint32_t player,std::uint32_t c){
    for(std::uint32_t k=1;k<24;++k){
        const std::uint32_t flag=c+0x230u+k*4u;
        const auto f=w.flags(8+k)&3u;
        if(f==1u){s.put32(flag,0);continue;}
        if(f!=2u)continue;
        const auto& v=w.event_work[8+k];
        if(!v.size)throw std::out_of_range("race sound: car event work not provided");
        const Obj car{v.data,v.size};
        if(car.u8(4)&0x41u)continue;
        if(car.u8(0x2f0)&2u)continue;
        if(sv.pc_55a930()){
            const auto m=car.u32(0xd14);
            if(m==1||m==4||m==5||m==2||m==3)continue;
        }
        const Obj e=object(w,player,0x264);
        const std::int32_t d=std::int32_t(e.u16(0x260))-std::int32_t(car.u16(0x260));
        if(d<-2){s.put32(flag,0);continue;}
        if(s.u32(flag)!=0)continue;
        driving::PcVec3 rel{st(X87(car.f32(0x14))-X87(e.f32(0x14))),st(X87(car.f32(0x18))-X87(e.f32(0x18))),st(X87(car.f32(0x1c))-X87(e.f32(0x1c)))};
        driving::PcVec3 ve{e.f32(0x20),e.f32(0x24),e.f32(0x28)};
        rel=driving::pc_d3dx_vec3_normalize(rel);
        ve=driving::pc_d3dx_vec3_normalize(ve);
        const X87 dotv=(X87(rel[2])*X87(ve[2])+X87(rel[0])*X87(ve[0]))+X87(rel[1])*X87(ve[1]);
        if(dotv>X87(c619a34))continue;
        // 4270A8: 40EFF0 cross(rel, ve); only its y is used.
        const float cy=st(X87(rel[2])*X87(ve[0])-X87(ve[2])*X87(rel[0]));
        const std::uint32_t side=(cy<=c619a34)?0u:1u;
        race_sound_set_se_424940(s,w,rd623(0x624014u+(side+car.u32(0x31c)*2u)*4u));
        s.put32(flag,1);
        return;
    }
}

// 425100 PlayCarSound.
void race_sound_car_425100(RaceSoundState& s,RaceSoundWorld& w,RaceSoundServices& sv){
    if((w.flags(8)&3u)!=2u)return;
    const auto count=s.u32(0x955c9cu);
    const std::uint32_t e=player_pc(w),c=RaceSoundState::car_sound_956148;
    if(count){s.putf(0x9560ccu,st(X87(s.f32(0x9560ccu))+X87(c6282b0)));s.put32(0x955c9cu,count-1u);}
    race_sound_car_param_4256f0(s,w,c,e);
    const auto phase=s.u32(0x9560c8u);s.put32(0x9560c8u,phase+1u);
    switch(phase&3u){
    case 0:
        race_sound_road_425a40(s,c,5,4);
        race_sound_skid_4260e0(s,c,3);
        if(s.i32(0x9563d8u)<0)race_sound_wall_4264c0(s,c,6);
        break;
    case 1:
        race_sound_engine_4252e0(s,c,0,1);
        if(s.i32(0x9563d8u)<0)race_sound_wall_4264c0(s,c,6);
        race_sound_near_cars_4267c0(s,w,c,e,0);
        race_sound_enemy_426b10(s,w,0xa,c,0x956218u,fb(0x41a00000u),fb(0x3f800000u));
        break;
    case 2:
        race_sound_road_425a40(s,c,5,4);
        race_sound_skid_4260e0(s,c,3);
        race_sound_wall_4264c0(s,c,6);
        break;
    case 3:
        race_sound_engine_4252e0(s,c,0,1);
        if(s.i32(0x9563d8u)<0)race_sound_wall_4264c0(s,c,6);
        race_sound_near_cars_4267c0(s,w,c,e,0);                 // 425267 also pushes 0
        race_sound_enemy_426b10(s,w,0xb,c,0x956244u,fb(0x41a00000u),fb(0x3f800000u));
        break;
    }
    race_sound_passing_426f50(s,w,sv,e,c);
    const auto k=s.u32(0x956214u);
    if(k!=0&&k<=6)race_sound_set_se_424940(s,w,rd623(0x623ff8u+k*4u));
}

// 427110 PlayEnvironmentSound (channel 15).
void race_sound_environment_427110(RaceSoundState& s,RaceSoundWorld& w,RaceSoundServices& sv){
    if((w.flags(8)&3u)!=2u)return;
    const std::uint32_t e=player_pc(w);
    const auto v=sv.pc_44dc50(object(w,e,0x6c).u32(0x68));
    const Obj E=object(w,e,0x66);
    const std::int32_t c=E.i16(0x64);const auto flag=object(w,e,0x60).u32(0x5c);
    if(v==0x11){
        if(flag!=0)return;
        X87 t;
        if(c<0x11f)t=X87(c628100);
        else if(c<0x1e7)t=(X87(c)-X87(c6280fc))*X87(c6280f8)*X87(c6280f4)+X87(c628100);
        else{t=(X87(c62806c)-X87(c-0x1e7)*X87(c6280f8))*X87(c6280f4);if(t<X87(c619a34))t=X87(c619a34);}
        const auto r=std::uint32_t(ftol(t));
        if(!full(s)){
            if(s.u32(0x955c50u)!=r){qpush(s,15,2,r);s.put32(0x955c50u,r);}
            if(!full(s)&&s.u32(0x955c5cu)!=0xdu){qpush(s,15,5,0xdu);s.put32(0x955c5cu,0xdu);}
        }
        race_sound_set_ics_4247b0(s,15,0,0x7f);
        return;
    }
    if(v!=0x16||flag!=0)return;
    X87 t;
    if(c>=0x96&&c<0xbe)t=(X87(c)-X87(c62810c))*X87(c628108)*X87(c628104);
    else if(c>=0xbe&&c<0xe6){t=(X87(c62806c)-X87(c-0xbe)*X87(c628108))*X87(c628104);if(t<X87(c619a34))t=X87(c619a34);}
    else t=X87(c619a34);
    race_sound_set_ics_4247b0(s,15,2,std::uint32_t(ftol(t)));
    race_sound_set_ics_4247b0(s,15,5,0xc);
    race_sound_set_ics_4247b0(s,15,0,0x7f);
}

// ============================================================================
// Event 383 entries.
void race_sound_init_424650(RaceSoundState& s,RaceSoundWorld& w,RaceSoundServices& sv){
    if(!w.enabled_95b248)return;
    race_sound_balance_4279c0(s,w,sv);
    for(std::uint32_t a=0x955ae0u;a<0x955ae0u+0x66u*4u;a+=4)s.put32(a,0x7fffu);
    s.put32(kW,0);s.put32(kR,0x57u);
    race_sound_init_se_queue_424880(s);
    race_sound_init_car_424a00(s);
    s.put32(0x956144u,0xffffffffu);s.put32(0x95610cu,0);s.put32(0x9560c4u,0);s.put32(0x9560ccu,0x3db851ecu);
    s.put32(0x955c9cu,0x46u);s.put32(0x9560c8u,0);s.put32(0x955c7cu,1);
    sv.pc_427630();
    race_sound_voices_427320(s);
    s.cheer_95b208=0;s.cheer_95b20c=0;
    for(std::uint32_t a:{0x956114u,0x956118u,0x95611cu,0x956120u})s.put32(a,0);
}
void race_sound_control_424700(RaceSoundState& s,RaceSoundWorld& w,RaceSoundServices& sv){
    if(!w.enabled_95b248)return;
    const auto mode=w.mode_78026c;
    const bool game=mode==0xd||mode==0x10;
    for(unsigned k=0;k<7;++k)s.put32(0x9560f0u+4*k,game?s.u32(0x9560d0u+4*k):k624238[k]);
    if(sv.pc_43f9c0()){
        if(s.u32(0x955c7cu)){race_sound_pause_clear_424b10(s,sv);s.put32(0x955c7cu,0);}
    }else{
        if(w.mode_78026c==0x10){race_sound_car_425100(s,w,sv);race_sound_environment_427110(s,w,sv);}
        race_sound_cheer_424f80(s,w,sv);
        s.put32(0x955c7cu,1);
    }
    race_sound_flush_ics_424820(s,sv);
    race_sound_flush_se_4249a0(s,sv);
}
void race_sound_destroy_424790(RaceSoundState& s,RaceSoundServices& sv){
    for(unsigned k=0;k<7;++k)s.put32(0x9560f0u+4*k,k624238[k]);
    sv.pc_427630();
}

// ============================================================================
CommTransResult comm_trans_control_45a280(const CommTransWorld& w){
    if(!w.manager_known)return CommTransResult::Unknown;
    if(!w.manager_7d68ac||!w.session_58)return CommTransResult::Idle;
    if(w.state_8==7u||w.state_8==0u)return CommTransResult::Idle;
    return CommTransResult::Transfer45a0c0;
}
}
