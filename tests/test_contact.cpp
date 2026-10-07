#include "driving/pc_contact.hpp"
#include <array>
#include <cassert>
#include <cmath>
using namespace outrun::driving;
namespace { using Work=std::array<std::uint8_t,work_size>;
void identity(Bytes w){for(int i=0;i<16;++i)w.putf(0x10+i*4,(i%5)==0?1.0f:0.0f);}
}
int main(){
  Work ww{};Bytes w(ww.data(),ww.size());identity(w);
  w.putf(0x10+12*4,10.0f);w.putf(0x10+13*4,2.0f);w.putf(0x10+14*4,-5.0f);
  w.putf(0x628,0.0f);w.putf(0x62c,1.0f);w.putf(0x630,0.0f);
  w.putf(0x640,13.0f);w.putf(0x644,4.0f);w.putf(0x648,-9.0f);
  contact_matrix(w);
  // Projection of point-translation onto +Y is (0,2,0), identity inverse.
  assert(w.f32(0x210)==0.0f);assert(w.f32(0x214)==2.0f);assert(w.f32(0x218)==0.0f);
  for(int i=0;i<16;++i)assert(std::isfinite(w.f32(0x1e0+i*4)));
  // Result is an affine rotation matrix.
  assert(w.f32(0x1e0+3*4)==0.0f);assert(w.f32(0x1e0+7*4)==0.0f);
  assert(w.f32(0x1e0+11*4)==0.0f);assert(w.f32(0x1e0+15*4)==1.0f);

  // A level contact frame: cap linear/angular speed and enforce the +10 ceiling.
  ww.fill(0);identity(w);w.putf(0x628,0);w.putf(0x62c,1);w.putf(0x630,0);
  w.putf(0x5c,300);w.putf(0x50,100);w.putf(0x644,5);w.putf(0x44,20);w.putf(0x60,2);
  const bool changed=maximum_velocity_check(w);assert(changed);assert(w.f32(0x5c)<167.0f);assert(w.f32(0x50)<61.0f);
  assert(w.f32(0x44)==15.0f);assert(w.f32(0x60)==0.0f);
  return 0;
}
