#include "driving/pc_crash.hpp"
#include "driving/pc_x87.hpp"
#include <cmath>
#include <limits>
namespace outrun::driving {
namespace {
X87 original_acos_nonnegative(float x,std::uint32_t mode){
    // The original CRT (582300) uses sqrt((1+x)*(1-x)), then FPATAN, not acosf.
    // 5888D5 keeps the caller's precision control ((cw&0x300)|0x7F), so the
    // FADD/FSUB/FMUL/FSQRT round to it while FPATAN does not.
    // Domain-error/errno/TLS behavior is not a native game service here.
    if(x>1.0f || !std::isfinite(x))return X87(std::numeric_limits<double>::quiet_NaN());
    if(x==1.0f)return X87(0.0f);
    const X87 a=X87(1.0f)+x,b=X87(1.0f)-x;          // fld1; fadd st,st(1) / fld1; fsub st,st(2)
    X87 v=x87_atan2(x87_sqrt(a*b),X87(x));          // fmulp; fsqrt; fxch; fpatan
    // CRT 58896B: when the saved control word is not 027F and the (sticky)
    // precision exception is set, 588877 spills/reloads ST0 as QWORD. The
    // 85FA64!=0 fast-return mode (58895E) keeps ST0.
    if(mode==0 && x87_precision()!=X87Precision::Double53)v=X87(x87_double(v));
    return v;
}
std::size_t crash_index(std::uint32_t s){
    const auto u=std::uint8_t(s);std::int8_t i;std::memcpy(&i,&u,1);
    if(i<0)throw std::out_of_range("negative signed-byte crash-table index");
    return std::size_t(i);
}
void sound_preflight(const PcSoundQueue& q){
    if((q.control&3u)!=2u||(q.control&16u))return;
    q.state.check(0,8);if(q.state.u32(0)==q.state.u32(4))return;
    q.entries.check(0,128);
    if(q.state.u32(4)>=32u)throw std::out_of_range("invalid sound write cursor");
}
void validate_channel(const PcCrashChannel& c){
    if(c.count<=0)throw std::out_of_range("crash sampler needs at least one key");
    c.keys.check(0,std::size_t(c.count)*16);
}
std::uint32_t encoded_state(std::uint32_t flags){return (flags>>2)&0xffffff1fu;}
constexpr float crash_dt=0x1.1028d2p-6f; // PC62812C bits3C881469, NOT 1/60
}
X87 calc_wall_impact_angle_x87(CourseVec3 motion,CourseVec3 wall,const PcMatrixStack& s,std::uint32_t crt_math_mode){
    const float sq=x87_float(course_vec3_length_squared_x87(motion));
    CourseVec3 v;
    if(sq>0.0f){
        const X87 scale=X87(1.0f)/x87_sqrt(X87(sq));
        v={static_cast<float>(scale*motion.x),static_cast<float>(scale*motion.y),static_cast<float>(scale*motion.z)};
    }else{
        const auto m=s.current();v={m.f32(0x20),m.f32(0x24),m.f32(0x28)};
    }
    const float dot=static_cast<float>((X87(v.z)*wall.z+
      X87(v.y)*wall.y)+X87(v.x)*wall.x);
    const float magnitude=std::fabs(dot);
    return X87(0x1.921fb6p+0f)-original_acos_nonnegative(magnitude,crt_math_mode);
}
long double calc_wall_impact_angle(CourseVec3 motion,CourseVec3 wall,const PcMatrixStack& s,std::uint32_t crt_math_mode){
    return calc_wall_impact_angle_x87(motion,wall,s,crt_math_mode).v;
}
void pc_collision_material_sound(Bytes e,Bytes w,Bytes contacts,std::uint32_t state,
                                  const PcMaterialSounds& tables,PcSoundQueue& q){
    // PC reads count before the cooldown gate, but reads no contact on an early exit.
    const auto count=w.i32(0x68c);e.check(0xd23,1);
    if(e.i8(0xd23)>0)return;
    std::uint32_t flags=0;
    if(count>0){contacts.check(0,std::size_t(count)*16);for(std::int32_t i=0;i<count;++i)flags|=contacts.u32(std::size_t(i)*16+8);}
    tables.priority_masks.check(0,32);
    unsigned material=0;
    for(unsigned i=0;i<8;++i)if(flags&tables.priority_masks.u32(4*i)){material=i;break;}
    const auto command=tables.commands[material].u32(std::size_t(state)*4);
    if(command)sound_preflight(q);
    e.put8(0xd23,60);
    if(command)pc_enqueue_sound(q,command);
}
std::uint16_t pc_course_end_position(const PcCourseEndView& c){
    if(!c.header)return 0;
    const auto count=c.header->u32(0xc);
    if(count==0)return 0;
    return std::uint16_t(c.positions.i16(std::size_t(count-1)*2));
}
void pc_crash_effect_dispatch(Bytes e,std::uint32_t state,const std::array<PcCourseEndView,4>& courses,
                              std::uint32_t mode,std::uint32_t variant){
    if(e.i8(0xd23)>0||e.u32(0)!=8u)return;
    const auto type=e.u32(0x5c);if(type>=courses.size())throw std::out_of_range("effect course type");
    const auto end=pc_course_end_position(courses[type]);
    volatile float progress=static_cast<float>(e.i16(0x64))/static_cast<float>(end);
    progress=progress*100.0f;
    // 49A650 is RET in the pinned PC file. The original dispatcher issues
    // either one or two calls with identical arguments. No replacement effect.
    (void)progress;(void)state;(void)mode;(void)variant;
}
void pc_find_crash_segment(PcCrashCursor& c,float t){
    if(c.channel.count<0)throw std::out_of_range("negative crash-key count");
    if(c.channel.count==0){c.lower.reset();c.upper.reset();return;}
    if(!c.lower||!c.upper)throw std::invalid_argument("missing crash-key cursor");
    auto lo=*c.lower,hi=*c.upper;const auto n=std::size_t(c.channel.count);
    const auto capacity=c.channel.keys.size()/16;
    if(lo>=capacity || hi>capacity || n>capacity-hi)throw std::out_of_range("crash cursor outside keys");
    c.channel.keys.check(lo*16,16);c.channel.keys.check(hi*16,n*16);
    auto remaining=n-1;
    // Equivalent ordered scan to the PC four-way unrolled loop. Unordered
    // time comparisons do not take JAE and therefore advance through keys.
    while(remaining && !(c.channel.keys.f32(hi*16)>=t)){lo=hi;++hi;--remaining;}
    if(!remaining && t>c.channel.keys.f32(hi*16))lo=hi;
    c.lower=lo;c.upper=hi;
}
X87 pc_interpolate_crash_keys_x87(Bytes keys,std::size_t lo,std::size_t hi,float t){
    if(lo>=keys.size()/16 || hi>=keys.size()/16)throw std::out_of_range("crash key index");
    const auto a=keys.sub(lo*16,16),b=keys.sub(hi*16,16);
    if(lo==hi)return a.f32(4);
    const float d=t-a.f32(0), span=b.f32(0)-a.f32(0), inv=1.0f/span;
    const float d2=d*d, inv2=inv*inv, q=d2*inv2;
    const float three=q*3.0f, qd=q*d, d2inv=d2*inv, outgoing=qd-d2inv;
    const float cubic=qd*inv, twice=cubic*2.0f;
    X87 v=((X87(outgoing)-d2inv)+d)*a.f32(12);
    v+=((X87(twice)-three)+1.0f)*a.f32(4);
    v+=(X87(three)-twice)*b.f32(4);
    v+=X87(outgoing)*b.f32(8);
    return v;
}
long double pc_interpolate_crash_keys(Bytes keys,std::size_t lo,std::size_t hi,float t){return pc_interpolate_crash_keys_x87(keys,lo,hi,t).v;}
X87 pc_sample_crash_channel_x87(const PcCrashChannel& c,float t){
    validate_channel(c);PcCrashCursor cursor{c,0,0};pc_find_crash_segment(cursor,t);
    return pc_interpolate_crash_keys_x87(c.keys,*cursor.lower,*cursor.upper,t);
}
long double pc_sample_crash_channel(const PcCrashChannel& c,float t){return pc_sample_crash_channel_x87(c,t).v;}
void pc_sample_crash_pose(const PcCrashPose& pose,float t,Bytes translation,Bytes rotation){
    translation.check(0,12);rotation.check(0,12);for(const auto& c:pose)validate_channel(c);
    for(unsigned i=0;i<6;++i){auto out=i<3?translation:rotation;out.putf((i%3)*4,x87_float(pc_sample_crash_channel_x87(pose[i],t)));}
}
float pc_crash_duration(const PcCrashTables& c,std::uint32_t s){return c.primary.f32(crash_index(s)*8+4);}
float pc_crash_recovery_duration(const PcCrashTables& c,std::uint32_t s){return c.recovery.f32(crash_index(s)*12);}
void pc_advance_crash_state(Bytes e,const PcCrashTables& c){
    e.check(0,0x104c);
    // Preflight the branch-dependent data before the first state write.
    const float old=e.f32(0x2c8);const float next=old-crash_dt;
    const PcCrashPose* pose=nullptr;float cooldown=0;
    if(old>0.0f){
        if(!(0.0f<next))cooldown=pc_crash_recovery_duration(c,encoded_state(e.u32(0x2f0)&0xfffffc7du));
        else{const auto i=crash_index(encoded_state(e.u32(0x2f0)));if(i>=c.poses.size())throw std::out_of_range("missing crash pose");pose=&c.poses[i];for(const auto& ch:*pose)validate_channel(ch);}
    }
    e.put32(4,e.u32(4)&0x7fffffffu);
    if(old>0.0f){
        e.putf(0x2c8,next);
        if(e.f32(0x2d0)>0.0f)e.put32(0x2f0,e.u32(0x2f0)&~1u);
        e.put32(0x2f4,e.u32(0x2f4)+1u);
        const float step=e.f32(0x2d4)*crash_dt;
        e.putf(0x2d0,e.f32(0x2d0)+step);
        if(!(0.0f<next)){
            e.put32(0x2f0,e.u32(0x2f0)&0xfffffc7du);
            for(auto o:{0x2c8,0x2d8,0x2dc,0x2e0,0x2e4,0x2e8,0x2ec,0x1034,0x1038,0x103c,0x1040,0x1044,0x1048})e.putf(o,0.0f);
            e.putf(0x2f8,cooldown);e.put32(0xd90,60);
        }
        e.put32(4,e.u32(4)|0x80000000u);
    }else if(e.f32(0x2f8)>0.0f)e.putf(0x2f8,e.f32(0x2f8)-crash_dt);
    if(e.f32(0x2c8)>0.0f){
        pc_sample_crash_pose(*pose,e.f32(0x2d0),e.sub(0x2d8,12),e.sub(0x2e4,12));
        const auto flip=(e.u32(0x2f0)>>7)&7u;
        if(flip==1){e.putf(0x2e8,0.0f-e.f32(0x2e8));e.putf(0x2ec,0.0f-e.f32(0x2ec));}
        else if(flip==2){e.putf(0x2e4,0.0f-e.f32(0x2e4));e.putf(0x2e8,0.0f-e.f32(0x2e8));}
    }
}
}
