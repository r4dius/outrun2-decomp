#include "driving/pc_crash_entry.hpp"
#include "driving/pc_x87.hpp"
#include <limits>
namespace outrun::driving {
namespace {
float ssub(float a,float b){volatile float v=a-b;return v;}
float smul(float a,float b){volatile float v=a*b;return v;}
float sadd(float a,float b){volatile float v=a+b;return v;}
float sdiv(float a,float b){volatile float v=a/b;return v;}
float ratio(float n,float d){float q=sdiv(n,d);if(0.1f>q)q=0.1f;if(q>1.0f)q=1.0f;return q;}
void check_feedback(const PcImpactFeedback& c,std::uint32_t state){
 c.thresholds.check(0,12); const std::uint64_t o=std::uint64_t(state)*4;
 if(o>std::numeric_limits<std::size_t>::max())throw std::out_of_range("feedback index");
 c.increments.check(static_cast<std::size_t>(o),4);
}
std::uint32_t resolve_entry(Bytes e,std::uint32_t state,const PcCrashEntryContext& c){
 if(state!=1||e.u32(0x5c)!=0)return state;
 const auto stage=pc_stage_number(c.stages,e.u32(0x68));
 if(stage<0)throw std::out_of_range("negative crash reroute stage");
 const auto row=c.reroute_ranges.sub(std::size_t(stage)*64,64);
 const int pos=e.i16(0x64);
 for(unsigned k=0;k<16;++k){const int lo=row.i16(4*k);if(lo<0)break;if(pos>=lo&&pos<=row.i16(4*k+2))return 5;}
 return state;
}
int feedback_kind(std::uint32_t state){
 // Original switch table 4A23E4. Filled from the verified reference executable.
 constexpr signed char kinds[19]={0,0,0,0,0,0,0,0,0,0,0,0,1,1,-1,-1,2,1,1};
 return state<19?kinds[state]:-1;
}
std::int32_t trunc_x87(X87 v){
 return x87_ftol32(v); // _ftol2; out of range/NaN -> 0x80000000
}
std::int32_t trunc_sse_i32(float v){
 if(!(v>=-2147483648.0f&&v<2147483648.0f))return std::numeric_limits<std::int32_t>::min();
 return static_cast<std::int32_t>(v);
}
std::int32_t crash_wrecker_phase(float duration){
 // PC 4A2358: MULSS by 628110 (bits 4270CCCD) followed by CVTTSS2SI.
 return trunc_sse_i32(smul(duration,60.200000762939453125f));
}
struct EntryPlan {std::uint32_t state,flags;float duration;int feedback;bool wrecker;std::int32_t phase;};
EntryPlan plan_entry(Bytes e,std::uint32_t state,std::uint32_t reverse,bool tow,const PcCrashEntryContext& c){
 e.check(0,0x10f0);   // the car work (stride 10F0); the fields read end at +E68
 state=resolve_entry(e,state,c);
 const auto flags=((((reverse&7u)<<5)|(state&31u))<<2)|(e.u32(0x2f0)&0xfffffc03u)|3u;
 const auto duration=pc_crash_duration(c.tables,(flags>>2)&31u);
 const bool wrecker=tow&&(e.u32(4)&1u);
 if(wrecker&&(!c.wrecker||!c.wrecker->services))throw PcMissingWrecker();
 const int feedback=(e.u32(4)&0x20u)&&c.feedback.enabled?feedback_kind(state):-1;
 if(feedback>=0)check_feedback(c.feedback,static_cast<unsigned>(feedback));
 return {state,flags,duration,feedback,wrecker,crash_wrecker_phase(duration)};
}
void apply_entry_state(Bytes e,float rate,const EntryPlan& p){
 e.put32(0x2f0,p.flags);e.putf(0x2cc,p.duration);e.put16(0xd8c,std::uint16_t(e.i16(0x1fe)));
 e.putf(0x2c8,p.duration);e.putf(0x2d0,0);e.putf(0x2d4,rate);e.putf(0x2f8,0);e.put32(0x2f4,0);
}
void execute_entry(Bytes e,float rate,const PcCrashEntryContext& c,const EntryPlan& p){
 apply_entry_state(e,rate,p);
 if(p.wrecker){
  const auto& w=*c.wrecker;
  pc_pl_wrecker(e,w.work,w.body_params,w.wheel_block,p.phase,*w.services);
 }
 if(p.feedback>=0)pc_add_impact_feedback(e,static_cast<unsigned>(p.feedback),1.0f,c.feedback);
}
}
void pc_calc_impact_bands(float value,std::uint32_t option2,std::uint32_t option3,
 Bytes thresholds,const PcImpactBands& o){
 (void)option3;thresholds.check(0,12);
 for(auto b:{o.count,o.low,o.middle,o.middle_ratio,o.high,o.high_ratio})b.check(0,4);
 // Preserve original write/read order. Validation uses disjoint view fields.
 o.count.put32(0,0);
 if(value>=thresholds.f32(0)){o.low.put32(0,1);o.count.put32(0,o.count.u32(0)+1);}else o.low.put32(0,0);
 if(value>=thresholds.f32(4)){
  o.middle.put32(0,1);o.middle_ratio.putf(0,ratio(ssub(value,thresholds.f32(4)),ssub(thresholds.f32(8),thresholds.f32(4))));
  o.count.put32(0,o.count.u32(0)+1);
 }else{o.middle.put32(0,0);o.middle_ratio.putf(0,0);}
 if(value>=thresholds.f32(8)){
  o.high.put32(0,1);o.count.put32(0,o.count.u32(0)+1);o.middle_ratio.putf(0,1);
  o.high_ratio.putf(0,ratio(ssub(value,thresholds.f32(8)),ssub(1.0f,thresholds.f32(8))));
 }else{o.high.put32(0,0);o.high_ratio.putf(0,0);}
 if(option2==0&&thresholds.f32(8)>value){o.low.put32(0,1);o.middle.put32(0,1);if(0.5f>o.middle_ratio.f32(0))o.middle_ratio.putf(0,0.5f);}
}
void pc_refresh_impact_feedback(Bytes e,const PcImpactFeedback& c){
 e.check(0,0xc37);c.thresholds.check(0,12);
 std::uint8_t buf[24]{};Bytes b(buf,sizeof buf);
 const auto flags=e.u32(0xc);
 pc_calc_impact_bands(e.f32(0xbd0),e.u8(0xc36),(flags>>25)&1u,c.thresholds,
  {b.sub(8,4),b.sub(4,4),b.sub(16,4),b.sub(0,4),b.sub(12,4),b.sub(20,4)});
 const auto packed=((((((b.u32(4)&1u)<<4)|(b.u32(8)&3u))<<2)|(b.u32(12)&1u))<<1)|(b.u32(16)&1u);
 e.putf(0xbd4,b.f32(0));e.put32(0xc,(flags&0xfffec9ffu)|(packed<<9));e.putf(0xbd8,b.f32(20));
}
void pc_add_impact_feedback(Bytes e,std::uint32_t state,float scale,const PcImpactFeedback& c){
 if(!(e.u32(4)&0x20u)||!c.enabled)return;
 e.check(0,0xc37);check_feedback(c,state);
 float q=sadd(smul(c.increments.f32(std::size_t(state)*4),scale),e.f32(0xbd0));
 if(q>1.0f)q=1.0f;else if(0.0f>q)q=0.0f;
 e.putf(0xbd0,q);pc_refresh_impact_feedback(e,c);
}
void pc_enter_crash(Bytes e,std::uint32_t state,std::uint32_t reverse,bool tow,float rate,const PcCrashEntryContext& c){
 const auto p=plan_entry(e,state,reverse,tow,c);execute_entry(e,rate,c,p);
}
void pc_start_crash(Bytes e,std::uint32_t state,std::uint32_t reverse,bool tow,const PcCrashEntryContext& c){
 pc_enter_crash(e,state,reverse,tow,1.0f,c);
}
bool pc_cw_crush_status(Bytes e,Bytes w,float angle,Bytes contacts,
 const PcCrashEntryContext& c,const WallResponseContext& wall,const PcCrushSelection& tuning,
 const PcMaterialSounds& materials,PcSoundQueue& queue,const std::array<PcCourseEndView,4>& ends,
 std::uint32_t mode,std::uint32_t variant){
 e.check(0,0x10f0);w.check(0,0x800);   // the car work (stride 10F0); the fields read end at +E68
 if(e.f32(0x2c8)>0||e.f32(0x2f8)>0||e.u32(0xdf8))return false;
 const bool trapped=check_crush_entrapment_length(e,std::uint16_t(e.i16(0x64)),wall);
 const X87 d=e.f32(0xdb4);
 std::uint32_t low,high;
 if(trapped){low=180u-std::uint32_t(trunc_x87(d*tuning.trapped_low));high=350u-std::uint32_t(trunc_x87(d*tuning.trapped_high));}
 else{const auto n=std::uint32_t(trunc_x87(X87(e.f32(0xe64))*tuning.ordinary_speed));
 low=n-std::uint32_t(trunc_x87(d*tuning.ordinary_low))-70u;high=n-std::uint32_t(trunc_x87(d*tuning.ordinary_high))-20u;}
 const auto speed=e.u32(0x1f4);
 if(speed<=low){set_collision_timer(e,2,wall.level);return false;}
 const auto state=speed>high?1u:angle>=tuning.angle_threshold?5u:2u;
 const auto reverse=(w.u32(0x670)&0xa0000000u)?1u:0u;
 const auto plan=plan_entry(e,state,reverse,true,c); // reject missing wrecker transactionally
 // Preflight the remaining services on private scratch states. No side effects
 // escape on invalid native views; the real calls below preserve original order.
 std::uint8_t scratch_event[0x1100],scratch_queue[128],scratch_indices[8];
 for(std::size_t k=0;k<sizeof scratch_event;++k)scratch_event[k]=e.u8(k);
 queue.entries.check(0,128);queue.state.check(0,8);
 for(unsigned k=0;k<128;++k)scratch_queue[k]=queue.entries.u8(k);
 for(unsigned k=0;k<8;++k)scratch_indices[k]=queue.state.u8(k);
 Bytes se(scratch_event,sizeof scratch_event);PcSoundQueue sq{{scratch_queue,128},{scratch_indices,8},queue.control};
 const auto command=tuning.commands.u32(state*4);
 pc_crash_effect_dispatch(se,state,ends,mode,variant);
 pc_enqueue_sound(sq,command);
 pc_collision_material_sound(se,w,contacts,state,materials,sq);
 set_collision_timer(e,2,wall.level);execute_entry(e,1.0f,c,plan);
 pc_crash_effect_dispatch(e,state,ends,mode,variant);pc_enqueue_sound(queue,command);
 pc_collision_material_sound(e,w,contacts,state,materials,queue);return true;
}
}
