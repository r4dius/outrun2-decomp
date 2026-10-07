#pragma once
// x87 arithmetic as the PC game performs it. The game creates its Direct3D 9
// device with behaviour flags 0x40 only (no D3DCREATE_FPU_PRESERVE, read in
// the protected device set-up reached from 0x40E8F0), so from then on the x87
// precision control is 24 bits (control word 0x007F): every FADD/FSUB/FMUL/
// FDIV/FSQRT (and their integer/reverse forms) rounds its result to a 24-bit
// significand while keeping the 15-bit exponent. Loads (FLD/FILD) are exact,
// stores round to the destination format, and the transcendental instructions
// (FSIN/FCOS/FPATAN/FPTAN) are not affected by the precision control.
//
// Two implementations of the same contract:
//  * exact (OR2_X87_FAST=0, default on x86 hosts / the oracle): the register
//    is a long double and every operation rounds to the selected precision
//    (Single24 0x007F, Double53 0x027F, Extended64 0x037F).
//  * fast (OR2_X87_FAST=1, default elsewhere, i.e. the Switch build): the
//    register is a double and the precision is fixed to Single24. Each
//    operation is one hardware double operation plus a float conversion; the
//    result is the correctly rounded 24-bit value of the exact result for
//    operands of up to 53 bits: double rounding can only differ when the
//    double result lies exactly on a float midpoint, which is detected with
//    one mask test and resolved with the exact error term (FMA/TwoSum). No
//    long double (software quad on AArch64) is used on this path.
// The fast path is validated against the original with the oracle built with
// -DOR2_X87_FAST=ON (probes at 0x007f).
#include <cfloat>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <limits>
#ifndef OR2_X87_FAST
#  if defined(__i386__) || defined(__x86_64__)
#    define OR2_X87_FAST 0
#  else
#    define OR2_X87_FAST 1
#  endif
#endif
#if defined(__GNUC__)
#  define OR2_X87_LIKELY(x) __builtin_expect(!!(x),1)
#  define OR2_X87_INLINE inline __attribute__((always_inline))
#else
#  define OR2_X87_LIKELY(x) (x)
#  define OR2_X87_INLINE inline
#endif
namespace outrun::driving {
enum class X87Precision : std::uint8_t { Single24, Double53, Extended64 };
#if OR2_X87_FAST
using x87_value_t=double;
#else
using x87_value_t=long double;
#endif
// Process-wide precision control (the game logic is single-threaded). Fixed
// to Single24 in the fast build.
X87Precision& x87_precision();
long double x87_round_bits(long double v,int bits);
double x87_round_bits_d(double v,int bits);
#if OR2_X87_FAST
namespace x87_detail {
OR2_X87_INLINE std::uint64_t bits(double d){std::uint64_t u;std::memcpy(&u,&d,8);return u;}
OR2_X87_INLINE std::uint32_t bits(float f){std::uint32_t u;std::memcpy(&u,&f,4);return u;}
// A double sitting exactly halfway between two floats (29 low mantissa bits
// = 1 followed by zeros).
OR2_X87_INLINE bool midpoint(double d){return (bits(d)&0x1fffffffull)==0x10000000ull;}
// Rare path: the double result is a float midpoint (decide with the sign of
// the exact error `err`, true value = hi + err) or outside the float normal
// range (round to 24 bits keeping the exponent, like the x87 register).
double resolve(double hi,double err);
OR2_X87_INLINE bool plain(double hi,float f){
    const std::uint32_t e=(bits(f)>>23)&0xffu;
    // Normal float, not a tie; or an exact zero (resolve returns it as is:
    // matrices multiply by 0 all the time, keep that off the call).
    return (e-1u)<0xfeu?!midpoint(hi):hi==0.0;
}
}
OR2_X87_INLINE double x87_add(double a,double b){
    const double s=a+b;const float f=float(s);
    if(OR2_X87_LIKELY(x87_detail::plain(s,f)))return f;
    const double bb=s-a;return x87_detail::resolve(s,(a-(s-bb))+(b-bb));   // TwoSum
}
OR2_X87_INLINE double x87_sub(double a,double b){return x87_add(a,-b);}
OR2_X87_INLINE double x87_mul(double a,double b){
    const double p=a*b;const float f=float(p);
    if(OR2_X87_LIKELY(x87_detail::plain(p,f)))return f;
    return x87_detail::resolve(p,std::fma(a,b,-p));
}
OR2_X87_INLINE double x87_div(double a,double b){
    const double q=a/b;const float f=float(q);
    if(OR2_X87_LIKELY(x87_detail::plain(q,f)))return f;
    if(!std::isfinite(q)||q==0.0)return x87_detail::resolve(q,0.0);
    const double r=std::fma(-q,b,a);                       // exact remainder
    return x87_detail::resolve(q,(r==0.0)?0.0:((r>0.0)==(b>0.0)?1.0:-1.0)*std::fabs(q)*0x1p-80);
}
OR2_X87_INLINE double x87_sqrt_op(double a){
    const double s=std::sqrt(a);const float f=float(s);
    if(OR2_X87_LIKELY(x87_detail::plain(s,f)))return f;
    if(!(s>0.0)||!std::isfinite(s))return x87_detail::resolve(s,0.0);
    const double r=std::fma(-s,s,a);
    return x87_detail::resolve(s,(r==0.0)?0.0:(r>0.0?1.0:-1.0)*s*0x1p-80);
}
inline long double x87_round(long double v){return x87_round_bits(v,24);}
#else
inline long double x87_round(long double v){
    switch(x87_precision()){
    case X87Precision::Single24:return x87_round_bits(v,24);
    case X87Precision::Double53:return x87_round_bits(v,53);
    default:return v;
    }
}
#if defined(__i386__) || defined(__x86_64__)
// Exact reference: the host x87 executes the instruction under the modelled
// precision control, so the result is rounded once from the exact value
// (a long double operation followed by a second rounding would double-round
// operands wider than 24/53 bits, e.g. FILD m32 or FLD m64 values).
std::uint16_t x87_control_word();
#define OR2_X87_HW_OP(name,insn) OR2_X87_INLINE long double name(long double a,long double b){     std::uint16_t saved,cw=x87_control_word();long double r;     __asm__ volatile("fnstcw %0":"=m"(saved));__asm__ volatile("fldcw %0"::"m"(cw));     __asm__ volatile(insn:"=t"(r):"0"(a),"u"(b):"st(1)");     __asm__ volatile("fldcw %0"::"m"(saved));     return r; }
OR2_X87_HW_OP(x87_add,"faddp")
OR2_X87_HW_OP(x87_sub,"fsubp")
OR2_X87_HW_OP(x87_mul,"fmulp")
OR2_X87_HW_OP(x87_div,"fdivp")
#undef OR2_X87_HW_OP
OR2_X87_INLINE long double x87_sqrt_op(long double a){
    std::uint16_t saved,cw=x87_control_word();long double r;
    __asm__ volatile("fnstcw %0":"=m"(saved));__asm__ volatile("fldcw %0"::"m"(cw));
    __asm__ volatile("fsqrt":"=t"(r):"0"(a));
    __asm__ volatile("fldcw %0"::"m"(saved));
    return r;
}
#else
OR2_X87_INLINE long double x87_add(long double a,long double b){return x87_round(a+b);}
OR2_X87_INLINE long double x87_sub(long double a,long double b){return x87_round(a-b);}
OR2_X87_INLINE long double x87_mul(long double a,long double b){return x87_round(a*b);}
OR2_X87_INLINE long double x87_div(long double a,long double b){return x87_round(a/b);}
OR2_X87_INLINE long double x87_sqrt_op(long double a){return x87_round(std::sqrt(a));}
#endif
#endif
struct X87 {
    x87_value_t v{};
    constexpr X87()=default;
    constexpr X87(long double x):v(x_value(x)){}
    constexpr X87(double x):v(x){}
    constexpr X87(float x):v(x){}
    constexpr X87(int x):v(x){}
    constexpr X87(unsigned x):v(x){}
    constexpr X87(long x):v(x_value(x)){}
    constexpr X87(long long x):v(x_value(x)){}
    explicit operator float()const{return float(v);}
    explicit operator double()const{return double(v);}
    explicit operator long double()const{return v;}
    X87& operator+=(X87 b){v=x87_add(v,b.v);return *this;}
    X87& operator-=(X87 b){v=x87_sub(v,b.v);return *this;}
    X87& operator*=(X87 b){v=x87_mul(v,b.v);return *this;}
    X87& operator/=(X87 b){v=x87_div(v,b.v);return *this;}
private:
    template<class T> static constexpr x87_value_t x_value(T x){return static_cast<x87_value_t>(x);}
};
OR2_X87_INLINE X87 operator+(X87 a,X87 b){return X87(x87_add(a.v,b.v));}
OR2_X87_INLINE X87 operator-(X87 a,X87 b){return X87(x87_sub(a.v,b.v));}
OR2_X87_INLINE X87 operator*(X87 a,X87 b){return X87(x87_mul(a.v,b.v));}
OR2_X87_INLINE X87 operator/(X87 a,X87 b){return X87(x87_div(a.v,b.v));}
OR2_X87_INLINE X87 operator-(X87 a){return X87(-a.v);}                 // FCHS: exact
OR2_X87_INLINE bool operator<(X87 a,X87 b){return a.v<b.v;}
OR2_X87_INLINE bool operator>(X87 a,X87 b){return a.v>b.v;}
OR2_X87_INLINE bool operator<=(X87 a,X87 b){return a.v<=b.v;}
OR2_X87_INLINE bool operator>=(X87 a,X87 b){return a.v>=b.v;}
OR2_X87_INLINE bool operator==(X87 a,X87 b){return a.v==b.v;}
OR2_X87_INLINE bool operator!=(X87 a,X87 b){return a.v!=b.v;}
OR2_X87_INLINE X87 x87_abs(X87 a){return X87(std::fabs(a.v));}          // FABS: exact
OR2_X87_INLINE X87 x87_sqrt(X87 a){return X87(x87_sqrt_op(a.v));}
// Stores (FST/FSTP m32/m64): round to the destination format.
OR2_X87_INLINE float x87_float(X87 a){return float(a.v);}
OR2_X87_INLINE double x87_double(X87 a){return double(a.v);}
// FISTP m32/m64 under the CRT's _ftol2 (truncation); out of range or NaN
// gives the integer indefinite value.
OR2_X87_INLINE std::int32_t x87_ftol32(X87 a){
    if(!(a.v>-2147483649.0&&a.v<2147483648.0))return std::numeric_limits<std::int32_t>::min();
    return static_cast<std::int32_t>(a.v);
}
OR2_X87_INLINE std::int64_t x87_ftol64(X87 a){
    if(!(a.v>=-9223372036854775808.0&&a.v<9223372036854775808.0))return std::numeric_limits<std::int64_t>::min();
    return static_cast<std::int64_t>(a.v);
}
// Transcendentals: the precision control does not apply.
X87 x87_sin(X87);
X87 x87_cos(X87);
X87 x87_tan(X87);                 // FPTAN result (ST1)
X87 x87_atan2(X87 y,X87 x);       // FPATAN: atan(ST1/ST0)
// Selects the precision matching an x87 control word (0x007F/0x027F/0x037F).
// The fast build only models 0x007F and returns false for the others.
bool x87_set_control(std::uint16_t control_word);
}
