#include <cstring>
#include "driving/pc_x87.hpp"
namespace outrun::driving {
X87Precision& x87_precision(){static X87Precision p=X87Precision::Single24;return p;}
long double x87_round_bits(long double v,int bits){
    if(v==0.0L||!std::isfinite(v))return v;
    int e=0;const long double m=std::frexp(v,&e);                 // |m| in [0.5,1)
    return std::ldexp(std::nearbyint(std::ldexp(m,bits)),e-bits);
}
double x87_round_bits_d(double v,int bits){
    if(v==0.0||!std::isfinite(v))return v;
    int e=0;const double m=std::frexp(v,&e);
    return std::ldexp(std::nearbyint(std::ldexp(m,bits)),e-bits);
}
#if OR2_X87_FAST
namespace x87_detail {
// Rounds hi + err (err: exact error of the double operation, or only its
// sign) to a 24-bit significand, ties to even, keeping the exponent.
namespace {
double resolve_slow(double hi,double err){
    if(hi==0.0||!std::isfinite(hi))return hi;
    int e=0;const double m=std::frexp(hi,&e);
    const double s=std::ldexp(m,24);                                 // exact
    const double lo=std::floor(s);
    double r;
    if(s-lo==0.5&&err!=0.0)r=err>0.0?lo+1.0:lo;                      // true value off the tie
    else r=std::nearbyint(s);
    return std::ldexp(r,e-24);
}
}
// Same result on the bits for a normal double: the 24-bit significand is the
// implicit bit and the top 23 stored bits, so the rounding point is bit 29
// whatever the exponent (no frexp/ldexp; ties from float additions are frequent).
double resolve(double hi,double err){
    std::uint64_t u;std::memcpy(&u,&hi,8);
    const std::uint32_t exponent=std::uint32_t(u>>52)&0x7ffu;
    if(exponent==0u||exponent==0x7ffu)return resolve_slow(hi,err);   // zero, subnormal, inf, NaN
    const std::uint64_t low=u&0x1fffffffull;
    std::uint64_t r=u&~0x1fffffffull;
    if(low==0x10000000ull){                                            // halfway
        const bool up=err!=0.0?((err>0.0)==(hi>0.0)):((u>>29)&1u)!=0u;  // off the tie, else to even
        if(up)r+=0x20000000ull;
    }else if(low>0x10000000ull)r+=0x20000000ull;
    double out;std::memcpy(&out,&r,8);return out;
}
}
bool x87_set_control(std::uint16_t cw){return ((cw>>8)&3u)==0u;}
#else
#if defined(__i386__) || defined(__x86_64__)
std::uint16_t x87_control_word(){
    switch(x87_precision()){
    case X87Precision::Single24:return 0x007fu;
    case X87Precision::Double53:return 0x027fu;
    default:return 0x037fu;
    }
}
#endif
bool x87_set_control(std::uint16_t cw){
    switch((cw>>8)&3u){
    case 0:x87_precision()=X87Precision::Single24;break;
    case 2:x87_precision()=X87Precision::Double53;break;
    default:x87_precision()=X87Precision::Extended64;break;
    }
    return true;
}
#endif
#if !OR2_X87_FAST && (defined(__i386__) || defined(__x86_64__))
X87 x87_sin(X87 a){long double r;__asm__("fsin":"=t"(r):"0"(a.v));return X87(r);}
X87 x87_cos(X87 a){long double r;__asm__("fcos":"=t"(r):"0"(a.v));return X87(r);}
X87 x87_tan(X87 a){long double r;__asm__("fptan\n\tfstp %%st(0)":"=t"(r):"0"(a.v));return X87(r);}
X87 x87_atan2(X87 y,X87 x){long double r;__asm__("fpatan":"=t"(r):"0"(x.v),"u"(y.v):"st(1)");return X87(r);}
#else
// Hardware double libm: the 64-bit x87 results are only ever consumed by
// 24-bit arithmetic or float stores, which the 53-bit values reproduce except
// within one double ulp of a float rounding boundary (checked by the oracle
// fast build).
namespace {
// FSIN/FCOS reduce the argument with a 66-bit approximation of pi (Intel:
// 0xC90FDAA22168C234C * 2^-66), not with the true pi. For small arguments the
// difference is far below a double ulp, but for large ones (seen in the
// environment progression probe, |x| ~ 2^49..2^51) the hardware result is the
// sine/cosine of x reduced modulo pi66/2, which libm (exact reduction) does
// not reproduce. Emulate that reduction exactly with integer arithmetic on the
// rare large-argument path; |x| >= 2^63 leaves ST0 unchanged (C2=1).
constexpr std::uint64_t kPi66Hi=0x3ull;                     // pi66/2 = (kPi66Hi:kPi66Lo) * 2^-65,
constexpr std::uint64_t kPi66Lo=0x243F6A8885A308D3ull;      // 0x3243F6A8885A308D3 = 0xC90FDAA22168C234C/4
bool x87_reduce_large(double x,unsigned& quadrant,double& r_hi,double& r_lo){
    using u128=unsigned __int128;
    const u128 P=(u128(kPi66Hi)<<64)|kPi66Lo;
    int e=0;const double m=std::frexp(std::fabs(x),&e);      // |x| = m*2^e, m in [0.5,1)
    const u128 mi=u128(std::uint64_t(std::ldexp(m,53)));     // |x| = mi*2^(e-53)
    int shift=e-53+65;                                       // |x|/(pi66/2) = mi*2^shift/P
    if(shift<0)return false;
    u128 v=mi%P;unsigned q=unsigned(mi/P)&3u;
    for(;shift>0;--shift){v<<=1;q=(q<<1)&3u;if(v>=P){v-=P;q=(q+1u)&3u;}}
    if(v>(P>>1)){v=P-v;q=(q+1u)&3u;r_hi=-1.0;}else r_hi=1.0;  // nearest multiple
    const double sign=r_hi;
    const double hi=double(std::uint64_t(v>>13))*8192.0;    // v < 2^65: 52 + 13 bits, both exact
    const double lo=double(std::uint64_t(v&0x1fffu));
    r_hi=sign*std::ldexp(hi,-65);r_lo=sign*std::ldexp(lo,-65);
    if(x<0){r_hi=-r_hi;r_lo=-r_lo;q=(4u-q)&3u;}
    quadrant=q;return true;
}
double x87_sin_large(double x,bool cosine){
    unsigned q=0;double rh=0,rl=0;
    if(!x87_reduce_large(x,q,rh,rl))return cosine?std::cos(x):std::sin(x);
    if(cosine)q=(q+1u)&3u;                                   // cos(x) = sin(x+pi/2)
    const double s=std::sin(rh)+std::cos(rh)*rl,c=std::cos(rh)-std::sin(rh)*rl;
    switch(q){case 0:return s;case 1:return c;case 2:return -s;default:return -c;}
}
constexpr double kLargeTrig=256.0;   // below this the pi66 reduction error is < 2^-58
}
X87 x87_sin(X87 a){const double x=double(a.v);const double ax=std::fabs(x);
    if(OR2_X87_LIKELY(ax<kLargeTrig))return X87(x87_value_t(std::sin(x)));
    if(!(ax<0x1p63))return a;                                // out of range: ST0 unchanged
    return X87(x87_value_t(x87_sin_large(x,false)));}
X87 x87_cos(X87 a){const double x=double(a.v);const double ax=std::fabs(x);
    if(OR2_X87_LIKELY(ax<kLargeTrig))return X87(x87_value_t(std::cos(x)));
    if(!(ax<0x1p63))return a;
    return X87(x87_value_t(x87_sin_large(x,true)));}
X87 x87_tan(X87 a){return X87(x87_value_t(std::tan(double(a.v))));}
X87 x87_atan2(X87 y,X87 x){return X87(x87_value_t(std::atan2(double(y.v),double(x.v))));}
#endif
}
