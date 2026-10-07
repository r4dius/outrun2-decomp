#pragma once
#include "driving/pc_wall_response.hpp"
#include <array>
#include <cmath>
#include <cstring>
namespace outrun::testing {
using namespace outrun::driving;
constexpr std::size_t response_image_size=0x5000;
constexpr std::uint32_t response_guest_base=0x34500000;
constexpr unsigned response_selector_count=12;
constexpr const char* response_names[response_selector_count]={"stage_find_record","stage_number","check_crush_entrapment_length","check_friction_entrapment_length","collision_timer","matrix_identity","matrix_store_rotation","matrix_multiply_current","matrix_rotate_y","calc_friction_status","calc_rebound_heading","wall_response_chain"};
struct WallResponseFixture {
    std::array<std::uint8_t,response_image_size> image{};
    Bytes bytes(){return Bytes(image.data(),image.size());}
    PcMatrixStack stack(){auto b=bytes();return {b.sub(0x1800,0x400),b.i32(0x4000),b.i32(0x4004),b.i32(0x4008)};}
    WallResponseContext context(){
        auto b=bytes();PcStageViews stages{b.sub(0x1c00,4*0x78),b.i32(0x400c),{},b.sub(0x1e40,4)};
        for(unsigned k=0;k<4;++k)stages.descriptors.push_back(b.sub(0x1e00+4*k,4));
        return {std::move(stages),b.sub(0x2000,0x1000),b.sub(0x3000,0x1000),b.u8(0x401c),b.f32(0x4024),b.f32(0x4028)};
    }
};
inline WallResponseFixture make_wall_response_fixture(unsigned i,unsigned id){
    WallResponseFixture f;auto b=f.bytes();std::uint32_t r=0x4f523018u^(i*1664525u);
    auto random=[&](){r=r*1664525u+1013904223u;return r;};
    for(auto& v:f.image)v=std::uint8_t(random()>>24);
    auto scalar=[&](){return float(int(random()%20001)-10000)/256.0f;};
    auto matrix=[&](std::size_t o){for(unsigned k=0;k<16;++k)b.putf(o+k*4,scalar());};
    for(unsigned o=0x1800;o<0x1c00;o+=64)matrix(o);
    matrix(0x1010);matrix(0x4100);matrix(0x4180);
    // Physics matrices are affine; standalone multiply also exercises non-affine matrices.
    if(id==9||id==11)for(unsigned o:{0x1010u,0x1880u}){
        b.putf(o+12,0);b.putf(o+28,0);b.putf(o+44,0);b.putf(o+60,1);
    }
    b.puti(0x4000,128);b.puti(0x4004,int(i%8));b.puti(0x4008,15);
    const int count=i%17==0?-1:i%17==1?0:int(i%4+1);b.puti(0x400c,count);
    for(unsigned k=0;k<4;++k){
        b.put32(0x1c04+k*0x78,100+k);
        b.put32(0x1c14+k*0x78,response_guest_base+0x1e00+k*4);
        b.puti(0x1e00+k*4,int((i+k*13)%64));
    }
    if(i%11==0)b.put32(0x1c04+0x78,100); // first-match priority
    b.puti(0x1e40,int((i*17+7)%64));
    const auto key=i%5==4?999u:100u+i%4;b.put32(0x4010,key);b.put32(0x68,key);
    const std::uint16_t positions[]={99,100,101,199,200,201,32767,32768,65535,0,1,300};
    const auto stage=pc_stage_number(f.context().stages,key);
    const auto basepos=positions[(i/4)%12];
    const auto pos=std::uint16_t(basepos<1000?basepos+stage*256:basepos);
    b.put32(0x4014,pos);b.put16(0x64,pos);
    b.put32(0x5c,i%19==0?1u:0u);
    // The two *mutable data* tables are synthetic, never replacement code.
    for(unsigned table=0;table<2;++table)for(unsigned row=0;row<64;++row){
        const auto start=0x2000+table*0x1000+row*64;
        for(unsigned n=0;n<16;++n){b.put16(start+n*4,0xffffu);b.put16(start+n*4+2,0x55aau);}
        if((i>>table)&1u){
            const unsigned slot=(i/48)%16;
            for(unsigned n=0;n<slot;++n){b.put16(start+n*4,10);b.put16(start+n*4+2,9);}
            b.put16(start+slot*4,std::uint16_t(100+row*256));b.put16(start+slot*4+2,std::uint16_t(200+row*256));
            if(i%29==0){b.put16(start+slot*4,32767);b.put16(start+slot*4+2,32767);}
        }else if(i%31==0){ // no sentinel; exhaust all 16 disjoint/reversed ranges
            for(unsigned n=0;n<16;++n){b.put16(start+n*4,7);b.put16(start+n*4+2,6);}
        }
    }
    const std::int32_t modes[]={-2147483647-1,-1,0,1,2,3,2147483647};
    const std::int32_t timers[]={-2147483647-1,-1,0,119,120,179,180,181,2147483647};
    b.puti(0x4018,modes[i%7]);b.puti(0xdec,timers[(i/7)%9]);
    const std::uint8_t levels[]={0,1,2,3,255};b.put32(0x401c,levels[(i/3)%5]);
    float a=(float(int(random()%200001)-100000)/32768.0f);
    const float angles[]={0.0f,-0.0f,1.0f/2608.0f,-1.0f/2608.0f,3.141592741012573242f,-3.141592741012573242f,32.0f,-32.0f};
    if(i%3==0)a=angles[(i/3)%8];
    b.putf(0x4020,a);
    if((id==9||id==11)&&i%23==0){
        const std::uint32_t extremes[]={0x7fc00001,0x7f800000,0xff800000,0x4f000000,0xcf000000};
        b.put32(0x4020,extremes[(i/23)%5]);
    }
    b.putf(0x4024,i%13==0?4096.0f:2608.0f);b.putf(0x4028,i%13==0?-4096.0f:-2608.0f);
    const std::size_t current=0x1880;
    b.put32(0x402c,i%4==0?current:i%4==1?current+4:i%4==2?current-4:0x4180);
    b.put32(0x4030,i%3==0?current:i%3==1?current-4:0x4100);
    const std::int16_t differences[]={-32768,-32767,-9,-5,-4,-3,-1,0,1,3,4,5,9,32767};
    b.put16(0x4034,i%2?std::uint16_t(random()):std::uint16_t(differences[(i/2)%14]));
    b.put16(0x160,std::uint16_t(random()));b.put16(0xd4c,std::uint16_t(random()));b.put16(0xd4e,std::uint16_t(random()));
    b.put8(0x281,std::uint8_t(i%6==5?255:i%6));b.put8(0x282,std::uint8_t(i%3?random():0));
    const std::uint32_t speeds[]={0,1,1023,0x1000,0x7fffffff,0xffffffff};
    b.put32(0x1f4,i%2?random():speeds[(i/2)%6]);b.put32(0x2a8,i%3?0x10f0020u:0x400);
    const float assists[]={0,1,0.5f,0.33333334f,-1,1.125f};b.putf(0xdb4,assists[i%6]);b.put32(0xdf8,i%3==0?0u:random());
    if(i%23==0){const std::uint32_t v[]={0x7fc00001,0x7f800000,0xff800000,0x4f000000,0xcf000000};b.put32(0xdb4,v[(i/23)%5]);}
    // Explicit native views must ignore serialized guest work pointers.
    b.put32(0x2b4,0xdeadc0de);b.put32(0x1248,0xfefefefe);
    std::array<float,3> v{scalar(),scalar(),scalar()},n{1,0,0};
    switch(i%12){
      case 0:v={0,0,0};break;
      case 1:v={10,0,0};break;
      case 2:v={10,0,5};break;
      case 3:v={10,0,-5};break;
      case 4:n={0,1,0};break;
      case 5:n={0,0,1};break;
      case 6:n={0.6f,0,0.8f};break;
      case 7:n={0.8f,0.6f,0};break;
      case 8:n={0,0,0};break;
      case 9:v={1,0,1.0e-4f};break;
      case 10:v={1,0,std::nextafter(1.0e-4f,0.0f)};break;
      case 11:v={1,0,std::nextafter(1.0e-4f,1.0f)};break;
    }
    for(unsigned k=0;k<3;++k){b.putf(0x105c+4*k,v[k]);b.putf(0x164c+4*k,n[k]);}
    return f;
}
inline std::uint32_t run_wall_response_native(WallResponseFixture& f,unsigned id){
    auto b=f.bytes();auto stack=f.stack();std::uint32_t ret=0;
    switch(id){
      case 0:{auto v=pc_find_stage_record(f.context().stages,b.u32(0x4010));ret=v?std::uint32_t(*v):0xffffffffu;break;}
      case 1:ret=std::uint32_t(pc_stage_number(f.context().stages,b.u32(0x4010)));break;
      case 2:ret=check_crush_entrapment_length(b.sub(0,0x1000),std::uint16_t(b.u32(0x4014)),f.context());break;
      case 3:ret=check_friction_entrapment_length(b.sub(0,0x1000),std::uint16_t(b.u32(0x4014)),f.context());break;
      case 4:set_collision_timer(b.sub(0,0x1000),b.i32(0x4018),b.u8(0x401c));break;
      case 5:pc_matrix_identity(stack);break;
      case 6:pc_matrix_store_rotation(stack,b.sub(b.u32(0x402c),44));break;
      case 7:pc_matrix_multiply_current(stack,b.sub(b.u32(0x4030),64));break;
      case 8:pc_matrix_rotate_y(stack,b.f32(0x4020));break;
      case 9:calc_friction_status(b.sub(0,0x1000),b.sub(0x1000,0x800),b.f32(0x4020),stack,f.context());break;
      case 10:calc_rebound_heading(b.sub(0,0x1000),b.i16(0x4034));break;
      default:throw std::invalid_argument("unknown wall-response selector");
    }
    b.puti(0x4000,static_cast<std::int32_t>(stack.current_offset));b.puti(0x4004,stack.depth);b.puti(0x4008,stack.capacity);return ret;
}
}
