#include "driving/pc_tire_geometry.hpp"
#include "driving/pc_transmission.hpp"
#include "driving/pc_road_mu.hpp"
#include <array>
#include <cmath>
#include <iostream>
#include <stdexcept>
using namespace outrun::driving;
namespace {
unsigned checks=0;
void require(bool v,const char* m){++checks;if(!v)throw std::runtime_error(m);}
bool near(float a,float b,float e=1e-6f){return std::fabs(a-b)<=e;}
struct F {
  std::array<std::uint8_t,event_size> eb{};
  std::array<std::uint8_t,work_size> wb{};
  std::array<std::uint8_t,parameter_size> pb{};
  Bytes e(){return {eb.data(),eb.size()};} Bytes w(){return {wb.data(),wb.size()};} Bytes p(){return {pb.data(),pb.size()};}
};
void identity(Bytes w,std::size_t off){for(unsigned r=0;r<4;++r)for(unsigned c=0;c<4;++c)w.putf(off+(r*4+c)*4,r==c?1.0f:0.0f);}
float lookup(void*,std::uint32_t road,std::uint32_t surface){return 0.25f+float((road^surface)&0xffu)*0.0625f;}
}
int main(){try{
  F x;auto e=x.e(),w=x.w(),p=x.p();auto q=embedded_wheels(w);
  for(unsigned i=0;i<4;++i)w.put32(0x248+i*4,0xdead0000u+i);

  // Identity orientation, zero angular velocity, +X linear velocity, +Y road normal.
  identity(w,0x10);identity(w,0x1e0);
  w.putf(0x50,0);w.putf(0x54,0);w.putf(0x58,0);
  w.putf(0x5c,10);w.putf(0x60,0);w.putf(0x64,0);
  w.putf(0x628,0);w.putf(0x62c,1);w.putf(0x630,0);
  for(auto wheel:q){wheel.putf(0x4,0);wheel.putf(0x0c,0);wheel.putf(0x2c,0);wheel.putf(0x40,1);wheel.putf(0x44,0);wheel.putf(0x48,0);}
  tire_velocity(w);
  for(auto wheel:q){
    require(near(wheel.f32(0xd4),10.0f),"tire velocity magnitude");
    require(near(wheel.f32(0x58),1)&&near(wheel.f32(0x5c),0)&&near(wheel.f32(0x60),0),"tire velocity direction");
    require(near(wheel.f32(0x64),0)&&near(wheel.f32(0x68),0)&&near(wheel.f32(0x6c),-1),"tire lateral axis");
    require(wheel.i16(0xec)==-16383,"tire velocity angle units");
  }

  // Zero steering/toe with identity body leaves forward at -Z and side at -X.
  e.put16(0x32,0);e.put16(0x4c,0);e.put16(0x4e,0);
  for(auto wheel:q){wheel.putf(0xcc,0);wheel.put16(0xec,0);wheel.putf(0x70,0);wheel.putf(0x74,1);wheel.putf(0x78,0);}
  tire_direction(e,w);
  for(auto wheel:q){
    require(near(wheel.f32(0x40),0)&&near(wheel.f32(0x44),0)&&near(wheel.f32(0x48),-1),"tire forward vector");
    require(near(wheel.f32(0x4c),-1)&&near(wheel.f32(0x50),0)&&near(wheel.f32(0x54),0),"tire side vector");
  }

  // Automatic transmission uses distinct low/high-pedal downshift tables and clamps max gear.
  e.put32(0x208,2);e.puti(0x38,0);e.put8(0x282,0);e.putf(0x1c4,500);e.putf(0xe10+2*4,400);p.put32(0x10a0,6);
  auto_transmission(e,p);require(e.u32(0x208)==3&&e.u8(0x296)==1,"automatic upshift");
  e.put32(0x208,3);e.puti(0x38,200);e.putf(0x1c4,10);e.putf(0xe48+3*4,20);e.put8(0x282,0);
  auto_transmission(e,p);require(e.u32(0x208)==2&&e.u8(0x296)==3,"automatic high-pedal downshift table");

  // Road Mu fallback reproduces PC constants when the service is unavailable.
  q[0].put32(0x14,1);q[1].put32(0x14,4);q[2].put32(0x14,8);q[3].put32(0x14,2);
  q[2].put16(0xee,0);q[3].put16(0xee,0);
  get_road_mu(e,w,{});
  require(q[0].f32(0xe8)==0.0f,"road mu surface 1 fallback");
  require(near(q[1].f32(0xe8),0.699999988079071f)&&near(q[2].f32(0xe8),0.699999988079071f),"road mu special fallback");
  require(q[3].f32(0xe8)==1.0f,"road mu default fallback");

  // Available service is explicit and called with the road/surface pair.
  e.put32(0xd38,0x55);for(unsigned i=0;i<4;++i){q[i].put32(0x14,0x10+i);q[i].put16(0xee,0);}
  get_road_mu(e,w,{true,nullptr,&lookup});
  require(near(q[0].f32(0xe8),lookup(nullptr,0x55,0x10)),"road mu service lookup");

  std::cout<<"r007 driving primitives: "<<checks<<" checks passed\n";return 0;
}catch(const std::exception& ex){std::cerr<<"FAIL after "<<checks<<" checks: "<<ex.what()<<"\n";return 1;}}
