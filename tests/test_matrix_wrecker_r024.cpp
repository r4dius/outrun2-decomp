#include "driving/pc_wrecker.hpp"
#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <stdexcept>
using namespace outrun::driving;
namespace {
void req(bool v,const char* m){if(!v)throw std::runtime_error(m);}
std::uint32_t bits(float f){std::uint32_t u;std::memcpy(&u,&f,4);return u;}
void identity(Bytes m){for(unsigned k=0;k<16;++k)m.putf(k*4,(k%5u)==0u?1.0f:0.0f);}
}
int main(){try{
    // The new r024 D3DX-backed primitives must keep exact identity behaviour
    // for zero rotations, then apply a vector translation to the current slot.
    std::array<std::uint8_t,64*4> arena{};Bytes storage(arena.data(),arena.size());
    identity(storage.sub(64,64));
    PcMatrixStack stack{storage,64,1,4};
    const auto before=arena;
    pc_matrix_rotate_x(stack,0.0f);pc_matrix_rotate_z(stack,0.0f);
    req(std::memcmp(arena.data()+64,before.data()+64,64)==0,"zero X/Z rotation changed identity");
    pc_matrix_translate_vector(stack,{1.25f,-2.5f,3.75f});
    auto m=stack.current();
    req(bits(m.f32(0x30))==bits(1.25f)&&bits(m.f32(0x34))==bits(-2.5f)&&bits(m.f32(0x38))==bits(3.75f),"translate-vector row");
    req(bits(m.f32(0x3c))==bits(1.0f),"translate-vector w");

    // CalcDispMatrix with an all-zero pose and t=1 must publish the current
    // identity matrix to event+B0 and balance its internal push/pop.
    std::array<std::uint8_t,0x1100> event{};Bytes e(event.data(),event.size());
    identity(stack.current()); const auto depth=stack.depth; const auto off=stack.current_offset;
    PcDispMatrixContext disp{stack,1.0f,0};pc_calc_disp_matrix(e,disp);
    for(unsigned k=0;k<16;++k)req(e.u32(0xb0+k*4)==(k%5u==0u?0x3f800000u:0u),"CalcDispMatrix identity output");
    req(stack.depth==depth&&stack.current_offset==off,"CalcDispMatrix unbalanced stack");

    // Immediate wrapper must reject the delayed branch before any view access
    // or mutation. This is the fail-closed boundary used by the split r022/r024
    // validation of public PlWrecker.
    CourseWorldTables tables{};EasyLctPredictionState pred{};
    CourseWorldQuery q{tables,stack,pred};PcRoadInfoContext road{tables,stack,{0.0f,0.0f}};PcDispMatrixContext d2{stack,1.0f,0};
    const auto snap=arena;bool rejected=false;
    try{pc_pl_wrecker_immediate(Bytes(nullptr,0),Bytes(nullptr,0),Bytes(nullptr,0),Bytes(nullptr,0),2,road,q,d2);}catch(const std::invalid_argument&){rejected=true;}
    req(rejected,"immediate wrapper accepted delayed phase");req(arena==snap,"rejected immediate wrapper mutated stack");

    std::cout<<"r024 matrix/wrecker primitives: OK\n";return 0;
}catch(const std::exception& ex){std::cerr<<"FAIL: "<<ex.what()<<"\n";return 1;}}
