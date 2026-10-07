#include "support/crash_entry_fixture.hpp"
#include <iostream>
using namespace outrun::testing;
namespace {
unsigned checks=0;
void require(bool v,const char* m){++checks;if(!v)throw std::runtime_error(m);}
template<class F>void rejects_unchanged(CrashEntryFixture& f,F action,bool wrecker=false){
 const auto before=f.image;bool rejected=false,missing=false;
 try{action();}catch(const PcMissingWrecker&){rejected=true;missing=true;}catch(const std::exception&){rejected=true;}
 require(rejected,"invalid input was not rejected");require(!wrecker||missing,"wrong missing dependency error");require(before==f.image,"rejection modified caller memory");
}
void poison(CrashEntryFixture& f){auto b=f.bytes();for(unsigned k=0;k<4;++k)b.put32(0x5414+k*0x78,0xdead0001+k);for(unsigned k=0;k<20;++k){b.put32(0x3000+k*8,0xffff0001+k);for(unsigned n=0;n<6;++n)b.put32(0x5004+k*48+n*8,0xbad00001+n);}for(unsigned n=0;n<6;++n)b.put32(0x2104+n*8,0xdeadffff);for(unsigned n=0;n<8;++n)b.put32(0x4040+n*4,0xffffffff);}
}
int main(){try{
 // Forced middle/low flags do NOT change the unforced threshold count.
 {auto f=make_entry_fixture(0,0);auto b=f.bytes();b.putf(0x59d8,0);b.put32(0x59bc,0);run_entry_native(f,0);require(b.u32(0x5b00)==0,"forced count changed");require(b.u32(0x5b04)==1&&b.u32(0x5b08)==1&&b.f32(0x5b0c)==0.5f,"forced bands");}
 // C36 is a BYTE, not an event+C bit. Nonzero values are all non-forced.
 {auto a=make_entry_fixture(1,1);auto b=a;auto c=a;a.bytes().put8(0xc36,0);b.bytes().put8(0xc36,1);c.bytes().put8(0xc36,255);
 for(auto* f:{&a,&b,&c}){f->bytes().putf(0xbd0,0);run_entry_native(*f,1);}
 require(a.bytes().f32(0xbd4)==0.5f&&b.bytes().f32(0xbd4)==0&&c.bytes().f32(0xbd4)==0,"modifier byte semantics");}
 for(unsigned id=0;id<3;++id){auto f=make_entry_fixture(1,id);auto c=f.feedback();auto b=f.bytes();c.thresholds=Bytes(nullptr,0);
 rejects_unchanged(f,[&]{if(id==0)pc_calc_impact_bands(0,0,0,c.thresholds,{b.sub(0x5b00,4),b.sub(0x5b04,4),b.sub(0x5b08,4),b.sub(0x5b0c,4),b.sub(0x5b10,4),b.sub(0x5b14,4)});else if(id==1)pc_refresh_impact_feedback(b,c);else pc_add_impact_feedback(b,0,1,c);});}
 {auto f=make_entry_fixture(1,2);auto b=f.bytes();auto c=f.feedback();c.enabled=false;c.thresholds=Bytes(nullptr,0);c.increments=Bytes(nullptr,0);const auto old=f.image;pc_add_impact_feedback(b,0xffffffff,1,c);require(old==f.image,"disabled feedback consulted missing views");c.enabled=true;b.put32(4,0);const auto old2=f.image;pc_add_impact_feedback(b,0xffffffff,1,c);require(old2==f.image,"inactive feedback consulted missing views");}
 // Without an explicit PlWrecker context, parents must reject before writes rather than silently turning remorquage into a no-op.
 for(unsigned id:{3u,4u,5u})for(unsigned i=0;i<64;++i){auto f=make_entry_fixture(i,id);auto b=f.bytes();b.put32(4,b.u32(4)|1u);b.put32(0x59b0,1);b.putf(0x2c8,0);b.putf(0x2f8,0);b.put32(0xdf8,0);b.put32(0x1f4,1000);rejects_unchanged(f,[&]{run_entry_native(f,id);},true);}
 // Parent gate still permits bit0=1 when there is no crash entry to execute.
 {auto f=make_entry_fixture(1,5);auto b=f.bytes();b.put32(4,1);b.putf(0x2c8,1);auto before=f.image;require(run_entry_native(f,5)==0&&f.image==before,"active crash gate");
 b.putf(0x2c8,0);b.putf(0x2f8,0);b.put32(0xdf8,0);b.put32(0x1f4,0);b.puti(0xdec,0);require(run_entry_native(f,5)==0&&b.i32(0xdec)==120,"speed rejection must retain timer update");}
 // Stage routing is signed16, and exactly inclusive at both endpoints.
 for(int pos:{99,100,150,200,201,-1}){auto f=make_entry_fixture(1,3);auto b=f.bytes();b.put32(0x5c,0);b.put32(0x68,100);b.put32(0x5600,0);b.put16(0x64,std::uint16_t(pos));b.put16(0x5700,100);b.put16(0x5702,200);b.put16(0x5704,0xffff);b.put32(0x59a8,1);run_entry_native(f,3);require(((b.u32(0x2f0)>>2)&31u)==(pos>=100&&pos<=200?5u:1u),"signed inclusive reroute");}
 // A missing native table rejects before event/sound/global writes.
 {auto f=make_entry_fixture(1,3);auto b=f.bytes();auto c=f.entry_context();b.put32(0x5c,0);c.reroute_ranges=Bytes(nullptr,0);rejects_unchanged(f,[&]{pc_enter_crash_candidate(b,1,0,false,1,c);});c=f.entry_context();c.tables.primary=Bytes(nullptr,0);rejects_unchanged(f,[&]{pc_enter_crash_candidate(b,2,0,false,1,c);});}
 // Serialized addresses are deliberately poisonous; explicit views win.
 for(unsigned id:{3u,4u,5u,7u,8u})for(unsigned i=0;i<128;++i){auto a=make_entry_fixture(i,id),b=a;poison(b);for(unsigned s=0;s<entry_stage_count(id);++s){auto actual=entry_stage_id(id,s);require(run_entry_native(a,actual)==run_entry_native(b,actual),"poisoned pointer return");}poison(a);require(a.image==b.image,"native followed serialized pointer");}
 std::cout<<checks<<" feedback/crash candidate bounds, state and missing-dependency checks\n";return 0;
}catch(const std::exception& e){std::cerr<<"FAIL: "<<e.what()<<'\n';return 1;}}
