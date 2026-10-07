#pragma once
#include "crash_fixture.hpp"
#include "driving/pc_crash_entry.hpp"
namespace outrun::testing {
// Reuse the previously validated curve/sound arena. r021 occupies its unused
// 5400..5FFF region. Neither generation nor native execution uses guest pointers.
constexpr unsigned entry_selector_count=9;
constexpr const char* entry_names[]={"impact_bands","impact_feedback_refresh","impact_feedback_add",
 "crash_entry_candidate","crash_start_candidate","cw_crush_candidate",
 "impact_feedback_chain","crash_entry_update_chain","cw_crush_update_chain"};
struct CrashEntryFixture:CrashFixture {
 PcImpactFeedback feedback(){auto b=bytes();return {b.sub(0x5900,12),b.sub(0x5920,64),b.u32(0x59a0)!=0};}
 PcStageViews stages(){auto b=bytes();return {b.sub(0x5400,4*0x78),b.i32(0x59a4),
   {b.sub(0x5600,16),b.sub(0x5610,16),b.sub(0x5620,16),b.sub(0x5630,16)},b.sub(0x5640,16)};}
 PcCrashEntryContext entry_context(){auto b=bytes();return {tables(),stages(),b.sub(0x5700,256),feedback()};}
 WallResponseContext wall_context(){auto b=bytes();return {stages(),b.sub(0x5800,256),b.sub(0x5800,256),0};}
 PcCrushSelection tuning(){auto b=bytes();return {b.f32(0x59c0),b.f32(0x59c4),b.f32(0x59c8),b.f32(0x59cc),b.f32(0x59d0),b.f32(0x59d4),b.sub(0x5a00,64)};}
};
inline unsigned entry_stage_count(unsigned id){return id==6?4u:id==7||id==8?17u:1u;}
inline unsigned entry_stage_id(unsigned id,unsigned stage){return id==6?2u:id==7?(stage==0?3u:9u):id==8?(stage==0?5u:9u):id;}
inline CrashEntryFixture make_entry_fixture(unsigned i,unsigned id){
 CrashEntryFixture f;static_cast<CrashFixture&>(f)=make_crash_fixture(i,10);auto b=f.bytes();
 for(unsigned k=0;k<4;++k){b.put32(0x5400+k*0x78+4,100+k);b.put32(0x5400+k*0x78+0x14,crash_guest_base+0x5600+k*16);b.put32(0x5600+k*16,(k+1)%4);}
 b.put32(0x5640,3);b.puti(0x59a4,4);b.put32(0x68,i%7==0?999:100+i%4);
 b.put32(0x5c,(i/11)%4);b.put16(0x64,std::uint16_t(int(i%31)*50-200));
 for(unsigned s=0;s<4;++s)for(unsigned k=0;k<16;++k){
  b.put16(0x5700+s*64+k*4,k<3?std::uint16_t(s*80+k*190):0xffffu);
  b.put16(0x5702+s*64+k*4,std::uint16_t(s*80+k*190+110));
  b.put16(0x5800+s*64+k*4,k<2?std::uint16_t(s*120+k*310):0xffffu);
  b.put16(0x5802+s*64+k*4,std::uint16_t(s*120+k*310+160));
 }
 // Independent varied thresholds and increments, not one copied table row.
 b.putf(0x5900,0.10f+float(i%3)*0.025f);b.putf(0x5904,0.35f+float(i%7)*0.025f);b.putf(0x5908,0.70f+float(i%5)*0.025f);
 for(unsigned k=0;k<16;++k)b.putf(0x5920+k*4,float(int((i+k*13)%41)-10)*0.015625f);
 b.put32(0x59a0,i%9!=0?1:0);b.put32(0x59a8,i%20);b.put32(0x59ac,(i/20)%16);b.put32(0x59b0,(i/3)&1);
 b.putf(0x59b4,float(int(i%9)-2)*0.5f);b.putf(0x59b8,float(i%17)/16);b.put32(0x59bc,(i/32)%2);
 b.putf(0x59c0,-170);b.putf(0x59c4,-130);b.putf(0x59c8,216.720001220703125f);
 b.putf(0x59cc,-20);b.putf(0x59d0,-33);b.put32(0x59d4,0x3e32b8c3);
 for(unsigned k=0;k<16;++k)b.put32(0x5a00+k*4,2000+k*7);
 float value=float(int(i%151)-25)/100;
 if(i%16<9){const auto t=b.f32(0x5900+(i%3)*4);value=i%16<3?t:i%16<6?std::nextafter(t,-INFINITY):std::nextafter(t,INFINITY);}
 b.put8(0xc36,i%4==0?0:i%4==1?1:i%4==2?255:b.u8(0xc36));
 b.putf(0xbd0,value);b.putf(0x59d8,value);b.putf(0x59dc,float(int(i%17)-8)*0.25f);
 // Sweep each source flag, plus random full words, including both modifier bits.
 b.put32(0xc,i%3==0?1u<<((i/3)%32):b.u32(0xc));b.put32(4,(i%5?0x20u:0u)|((i/5)%2));
 // Do not execute the original unimplemented remorquage branch in this family.
 // Native rejection of this branch is independently tested without an oracle.
 if(((id==3||id==4||id==7)&&b.u32(0x59b0))||id==5||id==8)b.put32(4,b.u32(4)&~1u);
 b.putf(0x2c8,id==5&&i%13==0?1.0f:0.0f);b.putf(0x2f8,id==5&&i%17==0?1.0f:0.0f);
 b.put32(0xdf8,id==5&&i%19==0?1u:0u);
 b.putf(0xdb4,float(i%9)*0.125f);b.putf(0xe64,1.0f+float(i%11)*0.125f);b.put32(0x1f4,(i*37)%701);
 b.put32(0x1200+0x670,(i%4)<<29);b.puti(0xdec,int(i%250)-10);
 // Preserve inherited event kinds7/8 so the original effect dispatcher also
 // traverses its true RET callee on the applicable captured paths.
 // Exact speed and orientation boundaries; the expected result still comes
 // exclusively from original execution, not this fixture construction.
 if((id==5||id==8)&&i%7==0){
  b.put32(0x5c,1);b.putf(0xdb4,0);b.putf(0xe64,1);
  constexpr std::uint32_t speeds[]={145,146,147,195,196,197};
  b.put32(0x1f4,speeds[(i/7)%6]);
  float angle=b.f32(0x59d4);if((i/42)%3==0)angle=std::nextafter(angle,-INFINITY);else if((i/42)%3==2)angle=std::nextafter(angle,INFINITY);
  b.putf(0x59b8,angle);
 }
 if((id==3||id==4||id==7)&&i%13==0){
  b.put32(0x59a8,1);b.put32(0x5c,0);b.put32(0x68,100);b.put32(0x5600,0);
  b.put16(0x5700,100);b.put16(0x5702,200);b.put16(0x5704,0xffff);
  constexpr int positions[]={-1,99,100,150,200,201,32767,-32768};
  b.put16(0x64,std::uint16_t(positions[(i/13)%8]));
 }
 if(id==7)b.putf(0x59b4,1.0f);
 return f;
}
inline std::uint32_t run_entry_native(CrashEntryFixture& f,unsigned id){auto b=f.bytes();const auto c=f.entry_context();
 switch(id){
 case 0:pc_calc_impact_bands(b.f32(0x59d8),b.u32(0x59bc),(b.u32(0xc)>>25)&1,c.feedback.thresholds,
  {b.sub(0x5b00,4),b.sub(0x5b04,4),b.sub(0x5b08,4),b.sub(0x5b0c,4),b.sub(0x5b10,4),b.sub(0x5b14,4)});break;
 case 1:pc_refresh_impact_feedback(b.sub(0,0x1100),c.feedback);break;
 case 2:pc_add_impact_feedback(b.sub(0,0x1100),b.u32(0x59a8)%16,b.f32(0x59dc),c.feedback);break;
 case 3:pc_enter_crash_candidate(b.sub(0,0x1100),b.u32(0x59a8),b.u32(0x59ac),b.u32(0x59b0)!=0,b.f32(0x59b4),c);break;
 case 4:pc_start_crash_candidate(b.sub(0,0x1100),b.u32(0x59a8),b.u32(0x59ac),b.u32(0x59b0)!=0,c);break;
 case 5:{auto queue=f.sounds();return pc_cw_crush_status_candidate(b.sub(0,0x1100),b.sub(0x1200,0x800),b.f32(0x59b8),b.sub(0x4400,0x100),c,f.wall_context(),f.tuning(),f.materials(),queue,f.ends(),b.u32(0x4c64),b.u32(0x4c68))?1:0;}
 case 9:pc_advance_crash_state(b.sub(0,0x1100),f.tables());break;
 default:throw std::invalid_argument("crash entry selector");
 }return 0;
}
}
