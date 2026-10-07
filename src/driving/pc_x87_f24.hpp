#pragma once
// Single24 x87 arithmetic on binary32 values as binary32 arithmetic (fast
// build only). With the precision control at 24 bits an FMUL/FADD/FSUB/FDIV
// rounds the exact result to a 24-bit significand, ties to even, keeping the
// 15-bit exponent. When both operands are binary32 numbers and the result
// lies in the binary32 normal range (or is an exact zero), that is exactly
// the binary32 operation's result; every later operand is then a binary32
// number again. The D3DX routines (pc_d3dx.cpp, pc_matrix_stack.cpp) only
// load binary32 values, so they run on F24 and fall back to the X87 model
// when an operation left the normal range (f24_bad): subnormal, overflow,
// an underflow to zero or a division by zero, where the x87 register still
// holds a value binary32 cannot. Needs -ffp-contract=off (no FMA fusion) and
// FLT_EVAL_METHOD 0 (AArch64, x86-64 SSE).
#include <cstdint>
#include <cstring>
namespace outrun::driving {
inline unsigned f24_bad=0;   // sticky: some operation left the binary32 normal range
namespace f24_detail {
inline unsigned special(float r){
    std::uint32_t u;std::memcpy(&u,&r,4);const std::uint32_t e=u&0x7f800000u;
    return unsigned(e==0x7f800000u)|unsigned(e==0u&&(u&0x7fffffu)!=0u);
}
}
struct F24 {
    float v{};
    constexpr F24()=default;
    constexpr F24(float x):v(x){}
    constexpr F24(int x):v(float(x)){}
};
inline F24 operator*(F24 a,F24 b){
    const float r=a.v*b.v;
    f24_bad|=f24_detail::special(r)|unsigned(r==0.0f&&a.v!=0.0f&&b.v!=0.0f);return F24(r);}
inline F24 operator+(F24 a,F24 b){const float r=a.v+b.v;f24_bad|=f24_detail::special(r);return F24(r);}   // a binary32 sum never underflows to 0
inline F24 operator-(F24 a,F24 b){const float r=a.v-b.v;f24_bad|=f24_detail::special(r);return F24(r);}
inline F24 operator/(F24 a,F24 b){
    const float r=a.v/b.v;
    f24_bad|=f24_detail::special(r)|unsigned(b.v==0.0f)|unsigned(r==0.0f&&a.v!=0.0f);return F24(r);}
inline F24 operator-(F24 a){return F24(-a.v);}
inline bool operator==(F24 a,F24 b){return a.v==b.v;}
inline bool operator!=(F24 a,F24 b){return a.v!=b.v;}
inline bool operator<(F24 a,F24 b){return a.v<b.v;}
inline bool operator>(F24 a,F24 b){return a.v>b.v;}
inline bool operator<=(F24 a,F24 b){return a.v<=b.v;}
inline bool operator>=(F24 a,F24 b){return a.v>=b.v;}
inline float x87_float(F24 a){return a.v;}
// Every value zero or of magnitude in [2^-30, 2^31): sums of up to four
// products of such values (and their partial sums) can neither overflow nor
// leave the binary32 normal range (each is a multiple of 2^-83 below 2^64),
// so dot products of four terms need no f24_bad checks.
inline bool f24_dot_range(const float* v,unsigned n){
    unsigned bad=0;
    for(unsigned k=0;k<n;++k){
        std::uint32_t u;std::memcpy(&u,v+k,4);
        const std::uint32_t e=(u>>23)&0xffu;
        bad|=unsigned((u&0x7fffffffu)!=0u)&unsigned(e-97u>60u);
    }
    return bad==0u;
}
}
