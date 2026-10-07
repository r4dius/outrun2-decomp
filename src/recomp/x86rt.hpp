#pragma once
// Runtime for code produced by tools/x86tr/translate.py: the guest register
// file, flat guest memory (PC addresses), and the integer flag arithmetic of
// the original x86 instructions. Translated functions take the CPU state and
// behave like the original instruction stream, byte for byte in memory and
// bit for bit in registers (flags included where the original reads them).
#include <cmath>
#include <cstdint>
#include <cstring>
#include <limits>
#include "driving/pc_x87.hpp"
namespace or2x86 {
using u8=std::uint8_t;using u16=std::uint16_t;using u32=std::uint32_t;using u64=std::uint64_t;
using i8=std::int8_t;using i16=std::int16_t;using i32=std::int32_t;using i64=std::int64_t;
struct Fl;
// ---- x87 register stack: values in the precision-controlled arithmetic of
// driving/pc_x87.hpp (exact long double on x86 hosts, checked double on the
// Switch).
using xv=outrun::driving::x87_value_t;
using outrun::driving::x87_add;using outrun::driving::x87_sub;using outrun::driving::x87_mul;
using outrun::driving::x87_div;using outrun::driving::x87_sqrt_op;
struct Fpu {
    xv r[8]{};unsigned top{};u16 cw{0x037f};bool c0{},c1{},c2{},c3{};
    xv& st(int i){return r[(top+unsigned(i))&7u];}
    void push(xv v){top=(top-1u)&7u;r[top]=v;}
    void pop(){top=(top+1u)&7u;}
    u32 status() const{return (c0?0x100u:0u)|(c1?0x200u:0u)|(c2?0x400u:0u)|((top&7u)<<11)|(c3?0x4000u:0u);}
    void compare(xv a,xv b){
        c1=false;
        if(std::isnan(a)||std::isnan(b)){c0=c2=c3=true;return;}
        c2=false;c3=a==b;c0=a<b;}
    inline void compare_flags(Fl& f,xv a,xv b);
    void init(){*this=Fpu{};}
    void load_cw(u16 v){
        cw=v;
#if OR2_X87_FAST
        (void)outrun::driving::x87_set_control(v);
#else
        using outrun::driving::X87Precision;
        const unsigned pc=(v>>8)&3u;
        outrun::driving::x87_precision()=pc==0?X87Precision::Single24:pc==2?X87Precision::Double53:X87Precision::Extended64;
#endif
    }
    unsigned rc() const{return (cw>>10)&3u;}
    // FPREM/FPREM1: partial remainder (always complete for the game's operands),
    // quotient bits in C0 C3 C1.
    void prem(bool ieee){
        const xv a=st(0),b=st(1);
        if(std::isnan(a)||std::isnan(b)||std::isinf(a)||b==0){st(0)=std::numeric_limits<xv>::quiet_NaN();c2=false;return;}
        const xv q=ieee?std::nearbyint(a/b):std::trunc(a/b);
        const xv r=ieee?std::remainder(a,b):std::fmod(a,b);
        const unsigned long long qi=(unsigned long long)(std::fabs(q))&7ull;
        st(0)=r;c2=false;c0=(qi>>2)&1;c3=(qi>>1)&1;c1=qi&1;}
    // FXAM: class of ST0 (empty registers are not tracked: an empty stack reads as zero).
    void examine(){
        const xv v=st(0);c1=std::signbit(v);
        if(std::isnan(v)){c3=false;c2=false;c0=true;}
        else if(std::isinf(v)){c3=false;c2=true;c0=true;}
        else if(v==0){c3=true;c2=false;c0=false;}
        else if(std::fpclassify(v)==FP_SUBNORMAL){c3=true;c2=true;c0=false;}
        else{c3=false;c2=true;c0=false;}}
};
inline xv fpu_const_pi(){return xv(3.141592653589793238462643383279502884L);}
inline xv fpu_const_l2e(){return xv(1.442695040888963407359924681001892137L);}
inline xv fpu_const_l2t(){return xv(3.321928094887362347870319429489390175L);}
inline xv fpu_const_lg2(){return xv(0.301029995663981195213738894724493027L);}
inline xv fpu_const_ln2(){return xv(0.693147180559945309417232121458176568L);}
inline xv fpu_f2xm1(xv v){return std::exp2(v)-1;}
inline xv fpu_fyl2x(xv x,xv y){return y*std::log2(x);}
inline xv fpu_sin(xv v){return outrun::driving::x87_sin(outrun::driving::X87(v)).v;}
inline xv fpu_cos(xv v){return outrun::driving::x87_cos(outrun::driving::X87(v)).v;}
inline xv fpu_tan(xv v){return outrun::driving::x87_tan(outrun::driving::X87(v)).v;}
inline xv fpu_atan2(xv y,xv x){return outrun::driving::x87_atan2(outrun::driving::X87(y),outrun::driving::X87(x)).v;}
inline xv x87_from_f32(u32 b){float f;std::memcpy(&f,&b,4);return xv(f);}
inline xv x87_from_f64(u64 b){double d;std::memcpy(&d,&b,8);return xv(d);}
inline xv x87_from_i64(i64 v){return xv(v);}
inline u32 x87_to_f32(xv v){const float f=float(v);u32 b;std::memcpy(&b,&f,4);return b;}
inline u64 x87_to_f64(xv v){const double d=double(v);u64 b;std::memcpy(&b,&d,8);return b;}
inline xv fpu_round(const Fpu& x,xv v){
    switch(x.rc()){case 0:return std::nearbyint(v);case 1:return std::floor(v);case 2:return std::ceil(v);default:return std::trunc(v);}}
// FIST/FISTP: rounded by the control word; out of range or NaN stores the
// integer indefinite (the type's minimum).
template<class T> inline T x87_to_int(const Fpu& x,xv v){
    const xv r=fpu_round(x,v);
    if(!(r>=xv(std::numeric_limits<T>::min())&&r<=xv(std::numeric_limits<T>::max())))return std::numeric_limits<T>::min();
    return T(r);}
inline xv x87_frndint(const Fpu& x,xv v){return fpu_round(x,v);}
inline xv x87_fscale(xv a,xv b){return std::ldexp(a,int(std::trunc(b)));}
struct Cpu;
using Fn=void(*)(Cpu&);
struct Cpu {
    u32 eax{},ecx{},edx{},ebx{},esp{},ebp{},esi{},edi{};
    Fpu fpu;
    u32 xmm[8][4]{};                          // SSE registers (lanes of 32 bits)
    u32 fs_base{};                            // guest address of the thread block (FS:)
    u32 mxcsr{0x1f80};                        // STMXCSR/LDMXCSR image (scalar SSE here always rounds to nearest)
    bool fsw_al{};                            // test only: AL came from FNSTSW (exception flags not modelled)
    std::uintptr_t base{};                    // host address of guest address 0
    void* user{};                             // the owner (recomp: Guest)
    // A call to code that is not translated (direct or indirect): the guest
    // stack holds the return address at [esp]; the handler runs the callee
    // and leaves esp as the original `ret` would (return address popped).
    void (*external)(Cpu&,u32 target){};
    Fn (*lookup)(u32 target){};               // translated function for an address, or null
    // Execution that cannot continue the way the original would (division
    // fault, undecoded instruction): the handler does not return.
    void (*trap)(Cpu&,u32 pc,const char* why){};   // never returns (throws); clang rejects [[noreturn]] on a pointer
    // Guest memory owned elsewhere (the native port's region-mapped PcRaceMemory):
    // when set, every access of n bytes at a goes through it instead of base + a.
    void* mem_user{};
    u8* (*mem_at)(void* user,u32 a,u32 n,bool write){};
};
inline u8* host(Cpu& c,u32 a,u32 n=1,bool write=false){
    return c.mem_at?c.mem_at(c.mem_user,a,n,write):reinterpret_cast<u8*>(c.base+a);}
inline u8 ld8(Cpu& c,u32 a){return *host(c,a,1);}
inline u16 ld16(Cpu& c,u32 a){u16 v;std::memcpy(&v,host(c,a,2),2);return v;}
inline u32 ld32(Cpu& c,u32 a){u32 v;std::memcpy(&v,host(c,a,4),4);return v;}
inline u64 ld64(Cpu& c,u32 a){u64 v;std::memcpy(&v,host(c,a,8),8);return v;}
inline void st8(Cpu& c,u32 a,u32 v){*host(c,a,1,true)=u8(v);}
inline void st16(Cpu& c,u32 a,u32 v){u16 w=u16(v);std::memcpy(host(c,a,2,true),&w,2);}
inline void st32(Cpu& c,u32 a,u32 v){std::memcpy(host(c,a,4,true),&v,4);}
inline void st64(Cpu& c,u32 a,u64 v){std::memcpy(host(c,a,8,true),&v,8);}
// ---- flags (CF ZF SF OF PF; AF is never read by the game code) ----
struct Fl { bool cf{},zf{},sf{},of{},pf{}; };
template<int W> constexpr u32 mask(){return W==32?0xffffffffu:((1u<<W)-1u);}
template<int W> inline bool msb(u32 v){return (v>>(W-1))&1u;}
inline bool parity(u32 v){return !__builtin_parity(v&0xffu);}
template<int W> inline void szp(Fl& f,u32 r){f.zf=(r&mask<W>())==0;f.sf=msb<W>(r);f.pf=parity(r);}
template<int W> inline u32 add(Fl& f,u32 a,u32 b,u32 carry=0){
    const u64 wide=u64(a&mask<W>())+u64(b&mask<W>())+carry;const u32 r=u32(wide)&mask<W>();
    f.cf=(wide>>W)&1u;f.of=msb<W>((a^r)&(b^r));szp<W>(f,r);return r;}
template<int W> inline u32 sub(Fl& f,u32 a,u32 b,u32 borrow=0){
    a&=mask<W>();b&=mask<W>();const u32 r=(a-b-borrow)&mask<W>();
    f.cf=u64(a)<u64(b)+borrow;f.of=msb<W>((a^b)&(a^r));szp<W>(f,r);return r;}
template<int W> inline u32 logic(Fl& f,u32 r){r&=mask<W>();f.cf=false;f.of=false;szp<W>(f,r);return r;}
template<int W> inline u32 inc(Fl& f,u32 a){const bool c=f.cf;const u32 r=add<W>(f,a,1);f.cf=c;return r;}
template<int W> inline u32 dec(Fl& f,u32 a){const bool c=f.cf;const u32 r=sub<W>(f,a,1);f.cf=c;return r;}
template<int W> inline u32 neg(Fl& f,u32 a){const u32 r=sub<W>(f,0,a);f.cf=(a&mask<W>())!=0;return r;}
// Shifts: count masked to 5 bits; a zero count leaves every flag unchanged.
template<int W> inline u32 shl(Fl& f,u32 a,u32 n){
    n&=31u;a&=mask<W>();if(!n)return a;
    const u32 r=n>=32?0u:(a<<n)&mask<W>();f.cf=n<=W?((a>>(W-n))&1u):false;
    f.of=msb<W>(r)!=f.cf;szp<W>(f,r);return r;}
template<int W> inline u32 shr(Fl& f,u32 a,u32 n){
    n&=31u;a&=mask<W>();if(!n)return a;
    const u32 r=n>=W?0u:a>>n;f.cf=n<=W?((a>>(n-1))&1u):false;f.of=msb<W>(a);szp<W>(f,r);return r;}
template<int W> inline u32 sar(Fl& f,u32 a,u32 n){
    n&=31u;a&=mask<W>();if(!n)return a;
    const i32 s=W==32?i32(a):W==16?i32(i16(a)):i32(i8(a));
    const u32 r=u32(s>>(n>=W?W-1:n))&mask<W>();f.cf=n>=W?(s<0):((s>>(n-1))&1);f.of=false;szp<W>(f,r);return r;}
template<int W> inline u32 rol(Fl& f,u32 a,u32 n){
    n&=31u;a&=mask<W>();if(!n)return a;const u32 k=n%W;
    const u32 r=k?((a<<k)|(a>>(W-k)))&mask<W>():a;f.cf=r&1u;f.of=msb<W>(r)!=f.cf;return r;}
template<int W> inline u32 ror(Fl& f,u32 a,u32 n){
    n&=31u;a&=mask<W>();if(!n)return a;const u32 k=n%W;
    const u32 r=k?((a>>k)|(a<<(W-k)))&mask<W>():a;f.cf=msb<W>(r);f.of=msb<W>(r)!=msb<W>(r<<1);return r;}
inline u32 rcr(Fl& f,u32 a,u32 n){
    n&=31u;if(!n)return a;
    u64 v=(u64(f.cf)<<32)|a;                      // 33-bit rotate through CF
    for(u32 k=0;k<n;++k){const u64 lo=v&1u;v=(v>>1)|(lo<<32);}
    const u32 r=u32(v);const bool c=(v>>32)&1u;f.of=msb<32>(r)!=msb<32>(r<<1);f.cf=c;return r;}
inline u32 rcl(Fl& f,u32 a,u32 n){
    n&=31u;if(!n)return a;
    u64 v=(u64(f.cf)<<32)|a;
    for(u32 k=0;k<n;++k){const u64 hi=(v>>32)&1u;v=((v<<1)|hi)&0x1ffffffffull;}
    const u32 r=u32(v);f.cf=(v>>32)&1u;f.of=msb<32>(r)!=f.cf;return r;}
inline u32 shld(Fl& f,u32 a,u32 b,u32 n){
    n&=31u;if(!n)return a;const u32 r=(a<<n)|(b>>(32-n));f.cf=(a>>(32-n))&1u;f.of=msb<32>(r)!=msb<32>(a);szp<32>(f,r);return r;}
inline u32 shrd(Fl& f,u32 a,u32 b,u32 n){
    n&=31u;if(!n)return a;const u32 r=(a>>n)|(b<<(32-n));f.cf=(a>>(n-1))&1u;f.of=msb<32>(r)!=msb<32>(a);szp<32>(f,r);return r;}
// FCOMI/FUCOMI: ZF PF CF from the comparison (unordered: all three set).
inline void Fpu::compare_flags(Fl& f,xv a,xv b){
    f.of=f.sf=false;
    if(std::isnan(a)||std::isnan(b)){f.zf=f.pf=f.cf=true;return;}
    f.pf=false;f.zf=a==b;f.cf=a<b;}
inline xv x87_from_f80(Cpu& c,u32 a);
inline void x87_to_f80(Cpu& c,u32 a,xv v);
template<int W> inline i32 sx(u32 v){return W==32?i32(v):W==16?i32(i16(v)):i32(i8(v));}
template<int W> inline u32 imul2(Fl& f,u32 a,u32 b){
    const i64 p=i64(sx<W>(a))*i64(sx<W>(b));const u32 r=u32(p)&mask<W>();
    f.cf=f.of=p!=i64(sx<W>(r));szp<W>(f,r);return r;}
// ---- SSE scalar single precision (IEEE binary32, round to nearest, no FTZ/DAZ:
// MXCSR 0x1F80 as the game runs).
inline float fb(u32 b){float f;std::memcpy(&f,&b,4);return f;}
inline u32 bf(float f){u32 b;std::memcpy(&b,&f,4);return b;}
// x86 SSE arithmetic producing a NaN from non-NaN operands returns the
// "default NaN" 0xFFC00000; a NaN operand is propagated (first source wins).
inline u32 sse_nan(u32 a,u32 b,u32 r){
    const bool na=std::isnan(fb(a)),nb=std::isnan(fb(b));
    if(na)return a|0x00400000u;
    if(nb)return b|0x00400000u;
    return std::isnan(fb(r))?0xffc00000u:r;}
inline u32 sse_add(u32 a,u32 b){return sse_nan(a,b,bf(fb(a)+fb(b)));}
inline u32 sse_sub(u32 a,u32 b){return sse_nan(a,b,bf(fb(a)-fb(b)));}
inline u32 sse_mul(u32 a,u32 b){return sse_nan(a,b,bf(fb(a)*fb(b)));}
inline u32 sse_div(u32 a,u32 b){return sse_nan(a,b,bf(fb(a)/fb(b)));}
inline u32 sse_sqrt(u32 a){return sse_nan(a,a,bf(std::sqrt(fb(a))));}
// MINSS/MAXSS: the second operand is returned when either is NaN or both are zero.
inline u32 sse_min(u32 a,u32 b){return fb(a)<fb(b)?a:b;}
inline u32 sse_max(u32 a,u32 b){return fb(a)>fb(b)?a:b;}
inline void sse_comi(Fl& f,u32 a,u32 b);
inline u32 sse_cvtsi2ss(i32 v){return bf(float(v));}
inline u32 sse_cvttss2si(u32 a){const float v=fb(a);
    if(!(v>=-2147483648.0f&&v<2147483648.0f))return 0x80000000u;return u32(i32(v));}
inline u32 sse_cvtss2si(u32 a){const float v=std::nearbyint(fb(a));
    if(!(v>=-2147483648.0f&&v<2147483648.0f))return 0x80000000u;return u32(i32(v));}
inline void sse_comi(Fl& f,u32 a,u32 b){
    f.of=f.sf=false;const float x=fb(a),y=fb(b);
    if(std::isnan(x)||std::isnan(y)){f.zf=f.pf=f.cf=true;return;}
    f.pf=false;f.zf=x==y;f.cf=x<y;}
// 80-bit memory operands (FLD/FSTP TBYTE).
inline xv x87_from_f80(Cpu& c,u32 a){
    u64 m;u16 se;std::memcpy(&m,host(c,a,8),8);std::memcpy(&se,host(c,a+8,2),2);
    const int e=se&0x7fff;const bool neg=se&0x8000u;xv v;
    if(e==0x7fff)v=(m<<1)?std::numeric_limits<xv>::quiet_NaN():std::numeric_limits<xv>::infinity();
    else v=std::ldexp(xv(m),e-16383-63);
    return neg?-v:v;}
inline void x87_to_f80(Cpu& c,u32 a,xv v){
    u64 m=0;u16 se=std::signbit(v)?0x8000u:0u;
    if(std::isnan(v)){m=0xc000000000000000ull;se|=0x7fff;}
    else if(std::isinf(v)){m=0x8000000000000000ull;se|=0x7fff;}
    else if(v!=0){int e;const xv fr=std::frexp(std::fabs(v),&e);m=u64(std::ldexp(fr,64));se|=u16(e-1+16383);}
    std::memcpy(host(c,a,8,true),&m,8);std::memcpy(host(c,a+8,2,true),&se,2);}
}
