#include "driving/pc_steering.hpp"
#include "driving/pc_wheel_dynamics.hpp"
#include <array>
#include <cmath>
#include <iostream>
#include <stdexcept>
using namespace outrun::driving;
namespace {
unsigned checks=0;
void require(bool v,const char* m){++checks;if(!v)throw std::runtime_error(m);}
struct F {
    std::array<std::uint8_t,event_size> eb{};
    std::array<std::uint8_t,work_size> wb{};
    std::array<std::uint8_t,parameter_size> pb{};
    Bytes e(){return {eb.data(),eb.size()};} Bytes w(){return {wb.data(),wb.size()};} Bytes p(){return {pb.data(),pb.size()};}
    F(){
        auto e_=e(),w_=w(),p_=p();
        e_.put32(0x2b4,0xdead0001);
        for(unsigned i=0;i<4;++i)w_.put32(0x248+i*4,0xbad00001+i);
        p_.putf(0x130,1.0f);p_.putf(0x193c,4.0f);p_.putf(0x17c,1.0f);
        p_.putf(0x18a4,1.0f);p_.putf(0x18f0,1.0f);
    }
};
}
int main(){try{
    F x;auto e=x.e(),w=x.w(),p=x.p();auto q=embedded_wheels(w);

    // Low-speed steering: no road correction path, but the original base-angle
    // calculation still updates event+0x32.
    e.put16(0x202,0x1000);e.put32(0x1f4,20);e.put16(0x32,0x7777);
    steering_operation(e,w,p);const auto low=e.i16(0x32);
    require(low!=std::int16_t(0x7777),"steering base angle written");
    e.put32(0x1f4,21);e.put16(0x16a,1);e.put8(0xd36,1);
    const std::int16_t desired_road=low>=0?std::int16_t(-1000):std::int16_t(1000);
    const std::int16_t raw_road=std::int16_t(desired_road-8);
    w.put16(0x52c,static_cast<std::uint16_t>(raw_road));w.put16(0x620,static_cast<std::uint16_t>(raw_road));
    steering_operation(e,w,p);require(e.i16(0x32)==std::int16_t(low+1),"high-speed fallback correction branch");

    // Toe sign alternates left/right and axle coefficients are independent.
    p.putf(0x1a*0x4c,0.1f);p.putf(0x1b*0x4c,0.0f); // avoid accidental overlap assumptions
    p.putf(0x1c*0x4c,0.0f);p.putf(0x1e*0x4c,0.0f);p.putf(0x20*0x4c,0.0f);p.putf(0x22*0x4c,0.0f);p.putf(0x24*0x4c,0.0f);
    p.putf(0x1b*0x4c,0.2f); // axle 1 base at (1+0x1a)*0x4c
    p.putf(0x1d*0x4c,0.0f);p.putf(0x1f*0x4c,0.0f);p.putf(0x21*0x4c,0.0f);p.putf(0x23*0x4c,0.0f);p.putf(0x25*0x4c,0.0f);
    for(auto wheel:q)wheel.putf(0xb0,0.0f);
    toe_angle(e,w,p,0);
    require(q[0].f32(0xcc)==0.1f&&q[1].f32(0xcc)==-0.1f,"front toe alternating sign");
    require(q[2].f32(0xcc)==0.2f&&q[3].f32(0xcc)==-0.2f,"rear toe alternating sign");

    // Closed CalcTireDirection prefix: front adds event steering, rear does not;
    // averages use signed truncation toward zero.
    e.put16(0x32,100);e.put16(0x4c,50);e.put16(0x4e,std::uint16_t(std::int16_t(-50)));
    for(unsigned i=0;i<4;++i){q[i].putf(0xcc,0.0f);q[i].put16(0xec,std::uint16_t(200+i*100));}
    tire_direction_angles(e,w);
    require(q[0].i16(0x32)==100&&q[1].i16(0x32)==100&&q[2].i16(0x32)==0&&q[3].i16(0x32)==0,"front/rear direction angle composition");
    require(q[0].i16(0xee)==100&&q[1].i16(0xee)==200&&q[2].i16(0xee)==400&&q[3].i16(0xee)==500,"wheel angle deltas");
    require(e.i16(0x4c)==150&&e.i16(0x4e)==450,"axle averages");
    require(e.i16(0x168)==100&&e.i16(0x16a)==500,"axle average deltas");

    // Running resistance keeps the PC mutable globals explicit. Distinct event
    // and wheel surface scales ensure the two original branches are preserved.
    RunningResistanceTuning t{1.0f,0.0f,0.0f,0.0f,2.0f,3.0f};
    e.put8(0x283,0);e.put32(0x1f4,10);e.put32(0xdf8,0);e.putf(0xdb4,0);e.put32(0x248,0x4);
    for(auto wheel:q){wheel.put16(0xee,0);wheel.put32(0x14,0);wheel.putf(0x58,1);wheel.putf(0x5c,2);wheel.putf(0x60,-1);}
    running_resistance(e,w,t);
    for(auto wheel:q){require(wheel.f32(0x94)==-200.0f&&wheel.f32(0x98)==-400.0f&&wheel.f32(0x9c)==200.0f,"event-surface resistance scale");}
    e.put32(0x248,0);q[0].put32(0x14,0x8);for(unsigned i=1;i<4;++i)q[i].put32(0x14,0);
    running_resistance(e,w,t);
    require(q[0].f32(0x94)==-300.0f,"wheel-surface resistance scale");
    require(q[1].f32(0x94)==0.0f&&q[2].f32(0x94)==0.0f&&q[3].f32(0x94)==0.0f,"no surface resistance is zero");

    require(e.u32(0x2b4)==0xdead0001,"native steering never dereferences guest parameter pointer");
    std::cout<<"steering/resistance: "<<checks<<" checks passed\n";return 0;
}catch(const std::exception& ex){std::cerr<<"FAIL after "<<checks<<" checks: "<<ex.what()<<"\n";return 1;}}
