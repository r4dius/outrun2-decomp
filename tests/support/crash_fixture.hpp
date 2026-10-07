#pragma once
#include "driving/pc_crash.hpp"
#include <algorithm>
#include <cmath>
#include <cstring>
namespace outrun::testing {
using namespace outrun::driving;
constexpr std::uint32_t crash_guest_base=0x34800000;
constexpr std::size_t crash_image_size=0x6000;
constexpr unsigned crash_selector_count=13;
constexpr const char* crash_names[]={"wall_impact_angle","collision_material_sound","course_end_position","crash_effect_dispatch",
 "crash_key_interpolation","crash_segment_search","crash_channel_sample","crash_pose_sample","crash_duration","crash_recovery_duration",
 "advance_crash_state","crash_state_chain","collision_material_sound_chain"};
struct CrashFixture {
    std::array<std::uint8_t,crash_image_size> image{};
    Bytes bytes(){return {image.data(),image.size()};}
    PcMatrixStack stack(){auto b=bytes();return {b.sub(0x1a00,0x400),b.i32(0x4c40),b.i32(0x4c44),b.i32(0x4c48)};}
    PcSoundQueue sounds(){auto b=bytes();return {b.sub(0x4600,128),b.sub(0x4680,8),b.u8(0x4688)};}
    PcMaterialSounds materials(){auto b=bytes();PcMaterialSounds m{b.sub(0x4000,32),{b.sub(0x4100,64),b.sub(0x4140,64),b.sub(0x4180,64),b.sub(0x41c0,64),b.sub(0x4200,64),b.sub(0x4240,64),b.sub(0x4280,64),b.sub(0x42c0,64)}};return m;}
    std::array<PcCourseEndView,4> ends(){auto b=bytes();return {{
      {b.u32(0x4c60)&1u?std::optional<Bytes>(b.sub(0x4800,16)):std::nullopt,b.sub(0x4a00,32)},
      {b.u32(0x4c60)&2u?std::optional<Bytes>(b.sub(0x4810,16)):std::nullopt,b.sub(0x4a20,32)},
      {b.u32(0x4c60)&4u?std::optional<Bytes>(b.sub(0x4820,16)):std::nullopt,b.sub(0x4a40,32)},
      {b.u32(0x4c60)&8u?std::optional<Bytes>(b.sub(0x4830,16)):std::nullopt,b.sub(0x4a60,32)}}};}
    PcCrashChannel channel(unsigned n){auto b=bytes();return {b.i16(0x2100+n*8),b.sub(0x2200+n*0x100,0x100)};}
    PcCrashPose pose(){return {channel(0),channel(1),channel(2),channel(3),channel(4),channel(5)};}
    PcCrashTables tables(){auto b=bytes();PcCrashTables t{b.sub(0x3000,20*8),b.sub(0x3200,20*12),{}};
        for(unsigned n=0;n<20;++n){PcCrashPose p=pose();for(unsigned k=0;k<6;++k){p[k]=channel((n+k)%6);p[k].count=b.i16(0x5000+n*0x30+k*8);}t.poses.push_back(p);}return t;}
};
inline unsigned crash_stage_count(unsigned id){return id==11?16u:id==12?3u:1u;}
inline unsigned crash_stage_id(unsigned id,unsigned){return id==11?10u:id==12?1u:id;}
inline CrashFixture make_crash_fixture(unsigned i,unsigned id){
    CrashFixture f;auto b=f.bytes();std::uint32_t rng=0x4f523020u^(i*1664525u);
    auto random=[&](){rng=rng*1664525u+1013904223u;return rng;};
    for(auto& v:f.image)v=std::uint8_t(random()>>24);
    auto scalar=[&](){return float(int(random()%20001)-10000)/1024.0f;};
    // Preserve varied canaries in untouched bytes; pointer words are guest addresses only.
    b.put32(0x4c40,(i%4)*64);b.put32(0x4c44,i%4);b.put32(0x4c48,16);
    for(unsigned j=0;j<16;++j){auto m=b.sub(0x1a00+64*j,64);for(unsigned k=0;k<16;++k)m.putf(4*k,0);for(unsigned k=0;k<4;++k)m.putf(k*20,1);m.putf(0x30,scalar());m.putf(0x34,scalar());m.putf(0x38,scalar());}
    // Four exact orientations, so a zero-length fallback is not always +Z.
    {auto m=b.sub(0x1a00+b.u32(0x4c40),64);const unsigned axis=(i/4)%3;
     m.putf(0x20,axis==0?1.0f:0.0f);m.putf(0x24,axis==1?-1.0f:0.0f);m.putf(0x28,axis==2?1.0f:0.0f);}
    b.put32(0x4c6c,(i/37)%2);
    std::array<float,3> motion{scalar(),scalar(),scalar()};
    if(i%8==0)motion={0.0f,0.0f,0.0f};
    if(i%8==1)motion={1.0f,0.0f,0.0f};
    if(i%8==2)motion={0.0f,-0.0f,-0.0f};
    if(i%8==3)motion={0x1p-75f,0.0f,0.0f}; // square spill becomes zero
    std::array<float,3> wall{0.0f,0.0f,0.0f};
    wall[(i/8)%3]=float((i/24)%17)/16.0f*((i&1)?-1.0f:1.0f);
    if(i%11==0)wall={0.25f,-0.125f,0.5f}; // deliberately not unit, must not renormalize
    for(unsigned k=0;k<3;++k){b.putf(0x4c00+k*4,motion[k]);b.putf(0x4c10+k*4,wall[k]);}
    for(unsigned k=0;k<8;++k){b.put32(0x4000+4*k,(1u<<k)|((i%9==0&&k>0)?1u:0u));b.put32(0x4040+4*k,crash_guest_base+0x4100+k*64);
        for(unsigned s=0;s<16;++s)b.put32(0x4100+k*64+s*4,(s+k+i)%13==0?0u:100u+k*32+s);}
    b.puti(0x1200+0x68c,int(i%19)-2);b.put32(0x4c30,i%16);
    for(unsigned k=0;k<16;++k){b.put32(0x4400+k*16+8,1u<<((i+k)%12));if(i%17==0)b.put32(0x4400+k*16+8,0);}
    const std::uint8_t cooldown[]={0,1,60,127,128,255};b.put8(0xd23,cooldown[(i/3)%6]);
    const std::uint8_t controls[]={2,0,1,3,0x12,0x82,0xf2};b.put8(0x4688,controls[(i/7)%7]);
    for(unsigned k=0;k<32;++k)b.put32(0x4600+k*4,1000+k);
    b.put32(0x4680,(i+1)%32);b.put32(0x4684,i%32);if(i%11==0)b.put32(0x4680,i%32);
    if(i%5==0){const unsigned material=i%8;b.put32(0x4600+((i/5)%32)*4,b.u32(0x4100+material*64+(i%16)*4));}
    b.put32(0x4c60,i%16);b.put32(0x5c,i%4);b.put32(0,i%3==0?8u:7u);b.put16(0x64,std::uint16_t(random()));b.put16(0x68,std::uint16_t(random()));
    for(unsigned t=0;t<4;++t){const auto count=(i+t)%17;b.put32(0x480c+t*16,count);
       for(unsigned k=0;k<16;++k)b.put16(0x4a00+t*32+k*2,std::uint16_t(k*127+t*701+(i%101)));}
    b.put32(0x4c64,i%6);b.put32(0x4c68,(i/6)%4);
    // Six independent 16-byte key arrays with differing values and asymmetric tangents.
    for(unsigned k=0;k<6;++k){const unsigned count=1+(i+k)%16;b.put16(0x2100+k*8,std::uint16_t(count));b.put32(0x2104+k*8,crash_guest_base+0x2200+k*0x100);
        float time=-0.0625f*float((i+k)%3);
        for(unsigned n=0;n<16;++n){const auto o=0x2200+k*0x100+n*16;b.putf(o,time);b.putf(o+4,scalar());b.putf(o+8,scalar());b.putf(o+12,scalar());time+=0.015625f*float(1+(n+k)%5);}}
    // Search cursor: 0 keys is valid only for this cursor helper; sampler rejects it.
    const auto count=unsigned(b.i16(0x2100));b.put16(0x2002,id==5&&i%13==0?0:std::uint16_t(count));
    b.put32(0x2008,crash_guest_base+0x2200);b.put32(0x200c,crash_guest_base+0x2200);b.put32(0x2010,crash_guest_base+0x2200);
    const auto index=(i/16)%count;float t=b.f32(0x2200+index*16);
    switch(i%8){case 0:t-=1.0f;break;case 1:t+=1.0f;break;case 2:t=std::nextafter(t,-INFINITY);break;case 3:t=std::nextafter(t,INFINITY);break;case 4:t+=0.0078125f;break;default:break;}
    b.putf(0x4c24,t);b.put32(0x4c28,index);b.put32(0x4c2c,i%7==0?index:(index+1)%16);
    b.put32(0x4c38,0x4d00);b.put32(0x4c3c,i%9==0?0x4d00:i%9==1?0x4d04:0x4d10);
    for(unsigned n=0;n<20;++n){b.put32(0x3000+n*8,crash_guest_base+0x5000+n*0x30);b.putf(0x3004+n*8,float(n+1)/8);b.putf(0x3200+n*12,float(n+1)/16);
      for(unsigned k=0;k<6;++k){b.put16(0x5000+n*0x30+k*8,std::uint16_t(b.i16(0x2100+((n+k)%6)*8)));b.put32(0x5004+n*0x30+k*8,crash_guest_base+0x2200+((n+k)%6)*0x100);}}
    const float dt=0x1.1028d2p-6f;
    const float durations[]={-0.0f,0.0f,-dt,dt,std::nextafter(dt,0.0f),std::nextafter(dt,INFINITY),dt*2,dt*3,dt*8,0.25f,1.0f,INFINITY};
    b.putf(0x2c8,durations[i%12]);b.putf(0x2cc,scalar());b.putf(0x2f8,((i/5)%3)?dt*0.5f:-dt);
    b.putf(0x2d0,i%4==0?0.0f:i%4==1?-0.02f:i%4==2?0.05f:0.75f);
    b.putf(0x2d4,float(int(i%7)-2)*0.5f);
    b.put32(0x2f4,i%5==0?0xffffffffu:random());
    b.put32(0x2f0,(random()&~0x3fcu)|((i%20)<<2)|(((i/20)%8)<<7));b.put32(0x4c34,i%20);
    if(id==11){b.putf(0x2c8,dt*float(1+i%15));b.putf(0x2d4,1.0f);}
    return f;
}
inline std::uint32_t crash_float_bits(float v){std::uint32_t r;std::memcpy(&r,&v,4);return r;}
inline std::uint32_t run_crash_native(CrashFixture& f,unsigned id,long double* extended=nullptr){
    auto b=f.bytes();std::uint32_t ret=0;long double fp=0;bool returns_fp=false;
    switch(id){
      case 0:fp=calc_wall_impact_angle({b.f32(0x4c00),b.f32(0x4c04),b.f32(0x4c08)},{b.f32(0x4c10),b.f32(0x4c14),b.f32(0x4c18)},f.stack(),b.u32(0x4c6c));returns_fp=true;break;
      case 1:{auto q=f.sounds();pc_collision_material_sound(b.sub(0,0x1100),b.sub(0x1200,0x800),b.sub(0x4400,0x100),b.u32(0x4c30),f.materials(),q);break;}
      case 2:ret=pc_course_end_position(f.ends()[b.u32(0x5c)]);break;
      case 3:pc_crash_effect_dispatch(b.sub(0,0x1100),b.u32(0x4c30),f.ends(),b.u32(0x4c64),b.u32(0x4c68));break;
      case 4:fp=pc_interpolate_crash_keys(b.sub(0x2200,0x100),b.u32(0x4c28),b.u32(0x4c2c),b.f32(0x4c24));returns_fp=true;break;
      case 5:{PcCrashCursor c{{b.i16(0x2002),b.sub(0x2200,0x100)},b.u32(0x200c)?std::optional<std::size_t>((b.u32(0x200c)-crash_guest_base-0x2200)/16):std::nullopt,b.u32(0x2010)?std::optional<std::size_t>((b.u32(0x2010)-crash_guest_base-0x2200)/16):std::nullopt};pc_find_crash_segment(c,b.f32(0x4c24));b.put32(0x200c,c.lower?crash_guest_base+0x2200+std::uint32_t(*c.lower)*16:0);b.put32(0x2010,c.upper?crash_guest_base+0x2200+std::uint32_t(*c.upper)*16:0);break;}
      case 6:fp=pc_sample_crash_channel(f.channel(0),b.f32(0x4c24));returns_fp=true;break;
      case 7:pc_sample_crash_pose(f.pose(),b.f32(0x4c24),b.sub(b.u32(0x4c38),12),b.sub(b.u32(0x4c3c),12));break;
      case 8:fp=pc_crash_duration(f.tables(),b.u32(0x4c34));returns_fp=true;break;
      case 9:fp=pc_crash_recovery_duration(f.tables(),b.u32(0x4c34));returns_fp=true;break;
      case 10:pc_advance_crash_state(b.sub(0,0x1100),f.tables());break;
      default:throw std::invalid_argument("crash fixture selector");
    }
    if(returns_fp){if(extended)*extended=fp;ret=crash_float_bits(static_cast<float>(fp));}return ret;
}
}
