#include "support/crash_fixture.hpp"
#include <functional>
#include <iostream>
#include <limits>
using namespace outrun::testing;
namespace {
unsigned checks=0;
void require(bool ok,const char* message){++checks;if(!ok)throw std::runtime_error(message);}
void rejects_unchanged(CrashFixture& f,const std::function<void()>& fn){const auto old=f.image;bool rejected=false;try{fn();}catch(const std::out_of_range&){rejected=true;}catch(const std::invalid_argument&){rejected=true;}require(rejected,"invalid view was not rejected");require(old==f.image,"rejection changed arena");}
void poison(CrashFixture& f){auto b=f.bytes();b.put32(0x2b4,0xdeadc0de);b.put32(0x1200+0x248,1);for(unsigned k=0;k<6;++k)b.put32(0x2104+k*8,0xdeadbeef);for(unsigned n=0;n<20;++n){b.put32(0x3000+n*8,0xffffffff);for(unsigned k=0;k<6;++k)b.put32(0x5004+n*0x30+k*8,0xfdfdfdfd);}for(unsigned n=0;n<8;++n)b.put32(0x4040+n*4,0xdeadbee1);}
}
int main(){try{
 const float dt=0x1.1028d2p-6f;
 // Verify actual timer quantum, not a guessed fixed 60Hz step, and expiration.
 for(float duration:{-dt,-0.0f,0.0f,std::nextafter(dt,0.0f),dt,std::nextafter(dt,INFINITY),dt*2}){
  auto f=make_crash_fixture(1,10);auto b=f.bytes();b.putf(0x2c8,duration);b.put32(0x2f0,0);b.putf(0x2f8,dt/2);b.put32(4,0x12345678);b.put32(0x2f4,0xffffffff);b.putf(0x2d0,0);b.putf(0x2d4,1);auto tables=f.tables();const auto old=f.image;
  pc_advance_crash_state(b.sub(0,0x1100),tables);
  require((b.u32(4)&0x7fffffffu)==0x12345678u,"unrelated event flags");
  if(duration>0){require(b.u32(4)&0x80000000u,"active/crash-final flag");require(b.u32(0x2f4)==0,"unsigned frame wrap");require(b.f32(0x2d0)==dt,"sample time quantum");if(duration<=dt){require(b.f32(0x2c8)==0,"expiration clamp");require(b.f32(0x2f8)==tables.recovery.f32(0),"recovery duration");require(b.u32(0xd90)==60,"expiration counter");for(auto o:{0x2d8,0x2dc,0x2e0,0x2e4,0x2e8,0x2ec,0x1034,0x1038,0x103c,0x1040,0x1044,0x1048})require(b.u32(o)==0,"expiration zero field");}else require(b.f32(0x2c8)==duration-dt,"active countdown");}
  else{require(!(b.u32(4)&0x80000000u),"idle flag");require(b.f32(0x2f8)==-dt/2,"cooldown must not clamp");require(b.u32(0x2f4)==0xffffffffu,"idle frame touched");}
  require(std::memcmp(old.data()+0x1200,f.image.data()+0x1200,0x800)==0,"crash update touched work");require(std::memcmp(old.data()+0x1a00,f.image.data()+0x1a00,0x400)==0,"crash update touched matrices");
 }
 // Empty cursor is allowed. Empty sampler is not. Pointer arithmetic is bounded.
 {
  auto f=make_crash_fixture(1,5);auto b=f.bytes();PcCrashCursor c{{0,Bytes(nullptr,0)},23,42};pc_find_crash_segment(c,0);require(!c.lower&&!c.upper,"empty cursor");
  c={{-1,b.sub(0x2200,0x100)},0,0};auto old=c;rejects_unchanged(f,[&]{pc_find_crash_segment(c,0);});require(c.lower==old.lower&&c.upper==old.upper,"rejected cursor changed");
  c={{2,b.sub(0x2200,0x100)},std::numeric_limits<std::size_t>::max()/16+1,0};rejects_unchanged(f,[&]{pc_find_crash_segment(c,0);});
  rejects_unchanged(f,[&]{pc_interpolate_crash_keys(b.sub(0x2200,0x100),std::numeric_limits<std::size_t>::max()/16+1,0,0);});
  rejects_unchanged(f,[&]{pc_sample_crash_channel({0,Bytes(nullptr,0)},0);});
 }
 for(unsigned count=1;count<=16;++count){
  auto f=make_crash_fixture(count,5);auto b=f.bytes();auto keys=b.sub(0x2200,0x100);for(unsigned k=0;k<count;++k){keys.putf(k*16,float(k));keys.putf(k*16+4,float(k*7));}
  for(unsigned k=0;k<count;++k){PcCrashCursor c{{std::int16_t(count),keys},0,0};pc_find_crash_segment(c,float(k));require(*c.upper==k&&*c.lower==(k?k-1:0),"exact boundary segment ordering");require(pc_sample_crash_channel(c.channel,float(k))==float(k*7),"exact key value");}
  PcCrashCursor before{{std::int16_t(count),keys},0,0},after=before;pc_find_crash_segment(before,-1);pc_find_crash_segment(after,float(count+1));require(*before.lower==0&&*before.upper==0,"before range clamps index");require(*after.lower==count-1&&*after.upper==count-1,"after range clamps index");
 }
 // Six sequential outputs, overlapping destinations, and native preflight.
 {
  auto f=make_crash_fixture(17,7);auto b=f.bytes();auto p=f.pose();const float t=.03f;
  std::array<float,6> expected{};for(unsigned k=0;k<6;++k)expected[k]=float(pc_sample_crash_channel(p[k],t));
  pc_sample_crash_pose(p,t,b.sub(0x4d00,12),b.sub(0x4d04,12));require(b.f32(0x4d00)==expected[0],"overlap first X");for(unsigned k=0;k<3;++k)require(b.f32(0x4d04+k*4)==expected[k+3],"overlap second output order");
  p[5].keys=Bytes(nullptr,0);rejects_unchanged(f,[&]{pc_sample_crash_pose(p,t,b.sub(0x4d00,12),b.sub(0x4d10,12));});
  p=f.pose();rejects_unchanged(f,[&]{pc_sample_crash_pose(p,t,b.sub(0x4d00,12),Bytes(nullptr,0));});
 }
 // Full per-frame entry preflights required branch only, before the event flag.
 {
  auto f=make_crash_fixture(7,10);auto b=f.bytes();b.putf(0x2c8,dt*2);b.put32(0x2f0,4);auto t=f.tables();t.poses[1][5].keys=Bytes(nullptr,0);
  rejects_unchanged(f,[&]{pc_advance_crash_state(b.sub(0,0x1100),t);});
  t=f.tables();rejects_unchanged(f,[&]{pc_advance_crash_state(b.sub(0,0x1000),t);});
  b.putf(0x2c8,dt);t.recovery=Bytes(nullptr,0);rejects_unchanged(f,[&]{pc_advance_crash_state(b.sub(0,0x1100),t);});
  b.putf(0x2c8,0);b.putf(0x2f8,0);t.poses.clear();pc_advance_crash_state(b.sub(0,0x1100),t);require(!(b.u32(4)&0x80000000u),"idle consulted tables");
  t=f.tables();for(auto v:{128u,255u,0xffffff80u})rejects_unchanged(f,[&]{pc_crash_duration(t,v);});require(pc_crash_duration(t,0x100)==pc_crash_duration(t,0),"signed byte indexing uses low byte");
 }
 // Material priority and signed-byte timer. Disabled audio still sets cooldown.
 for(unsigned material=0;material<8;++material){auto f=make_crash_fixture(material,1);auto b=f.bytes();b.put32(0x1200+0x68c,1);b.put32(0x4408,(1u<<material)|0x80000000u);b.put8(0xd23,255);b.put32(0x4680,1);b.put32(0x4684,0);auto m=f.materials();for(unsigned k=0;k<8;++k)b.put32(0x4000+4*k,1u<<k);b.put32(0x4100+material*64+8,12345+material);auto q=f.sounds();q.control=2;pc_collision_material_sound(b.sub(0,0x1100),b.sub(0x1200,0x800),b.sub(0x4400,0x100),2,m,q);require(b.u8(0xd23)==60&&b.u32(0x4600)==12345+material,"material sound priority / signed cooldown");const auto old=f.image;pc_collision_material_sound(b.sub(0,0x1100),b.sub(0x1200,0x800),Bytes(nullptr,0),999,PcMaterialSounds{Bytes(nullptr,0),m.commands},q);require(old==f.image,"cooldown gate consulted unavailable contacts");}
 {
  auto f=make_crash_fixture(1,1);auto b=f.bytes();b.put8(0xd23,0);b.put32(0x1200+0x68c,1);b.put32(0x4408,1);auto m=f.materials();b.put32(0x4000,1);b.put32(0x4100,7);auto q=f.sounds();q.control=2;q.state=Bytes(nullptr,0);rejects_unchanged(f,[&]{pc_collision_material_sound(b.sub(0,0x1100),b.sub(0x1200,0x800),b.sub(0x4400,16),0,m,q);});q.control=0;pc_collision_material_sound(b.sub(0,0x1100),b.sub(0x1200,0x800),b.sub(0x4400,16),0,m,q);require(b.u8(0xd23)==60,"disabled sound cooldown");
 }
 // Neither primary guest pointer words nor serialized channel pointers are used.
 for(unsigned i=0;i<256;++i){auto a=make_crash_fixture(i,11),b=a;poison(b);for(unsigned k=0;k<16;++k){run_crash_native(a,10);run_crash_native(b,10);}poison(a);require(a.image==b.image,"crash native followed guest pointers");}
 // Null course header is the original zero path; a present header requires data.
 {auto f=make_crash_fixture(0,2);auto b=f.bytes();PcCourseEndView c{std::nullopt,Bytes(nullptr,0)};require(pc_course_end_position(c)==0,"absent course header");b.put32(0x480c,0);c.header=b.sub(0x4800,16);require(pc_course_end_position(c)==0,"empty course header");b.put32(0x480c,1);rejects_unchanged(f,[&]{pc_course_end_position(c);});}
 std::cout<<checks<<" crash state/keyframe/material boundary and explicit-view checks\n";return 0;
}catch(const std::exception& e){std::cerr<<"FAIL: "<<e.what()<<'\n';return 1;}}
