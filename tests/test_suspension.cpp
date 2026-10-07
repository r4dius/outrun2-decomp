#include "driving/pc_suspension.hpp"
#include "driving/pc_wheel_dynamics.hpp"
#include <array>
#include <cassert>
#include <cmath>
#include <cstdint>
using namespace outrun::driving;
namespace {
using Event=std::array<std::uint8_t,event_size>;
using Work=std::array<std::uint8_t,work_size>;
using Params=std::array<std::uint8_t,parameter_size>;
void identity(Bytes w){for(int i=0;i<16;++i)w.putf(0x10+i*4,(i%5)==0?1.0f:0.0f);}
}
int main(){
    Event ee{};Work ww{};Params pp{};Bytes e(ee.data(),ee.size()),w(ww.data(),ww.size()),p(pp.data(),pp.size());
    identity(w);p.putf(0,4000.0f);
    auto q=embedded_wheels(w);
    for(auto wheel:q){wheel.putf(0x70,0);wheel.putf(0x74,1);wheel.putf(0x78,0);wheel.putf(0x24,6000);wheel.putf(0x38,1000);}
    tire_load(e,w,p);
    for(auto wheel:q)assert(wheel.f32(0x34)==1000.0f);

    q[0].putf(0x34,200);q[1].putf(0x34,100);q[2].putf(0x34,100);q[3].putf(0x34,200);
    q[0].putf(0x38,10);q[1].putf(0x38,20);q[2].putf(0x38,30);q[3].putf(0x38,40);p.putf(0,100);
    ass_diagonal_tire_load(w,p);
    assert(std::fabs(q[0].f32(0x34)-180.0f)<0.0001f);
    assert(std::fabs(q[1].f32(0x34)-140.0f)<0.0001f);
    assert(std::fabs(q[2].f32(0x34)-160.0f)<0.0001f);
    assert(std::fabs(q[3].f32(0x34)-120.0f)<0.0001f);

    // Symmetric suspension: equal wheel states must remain pairwise equal.
    pp.fill(0);p.putf(0,4000.0f);
    for(unsigned axle=0;axle<2;++axle){
        p.putf((axle+0x0e)*0x4c,0.15f);p.putf((axle+0x10)*0x4c,0.05f);p.putf((axle+0x12)*0x4c,0.30f);
        p.putf((axle+0x16)*0x4c,12000.0f);p.putf((axle+0x18)*0x4c,3000.0f);
    }
    for(auto wheel:q){wheel.putf(0x08,0.25f);wheel.putf(0x28,0.05f);wheel.putf(0x18,0.18f);}
    suspension_force(w,p);
    assert(q[0].f32(0x18)==0.20f);assert(q[0].f32(0x1c)==0.18f);
    assert(q[0].f32(0x20)==q[1].f32(0x20));assert(q[2].f32(0x20)==q[3].f32(0x20));
    assert(q[0].f32(0x24)==q[1].f32(0x24));assert(q[2].f32(0x24)==q[3].f32(0x24));
    assert(std::isfinite(q[0].f32(0x24))&&std::isfinite(q[2].f32(0x24)));
    return 0;
}
