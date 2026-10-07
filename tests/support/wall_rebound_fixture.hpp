#pragma once
#include "wall_response_fixture.hpp"
#include "driving/pc_wall_rebound.hpp"
namespace outrun::testing {
constexpr std::size_t rebound_image_size=0x7000;
constexpr std::uint32_t rebound_guest_base=0x34600000;
constexpr unsigned rebound_selector_count=12;
constexpr const char* rebound_names[]={"stage_property","pack_route","unpack_route","save_route_progress","set_route_choice","get_route_choice","enqueue_sound","calc_rebound_heading_float","direction_xz","cw_rebound_status","wall_rebound_chain","wall_friction_rebound_chain"};
struct WallReboundFixture {
    std::array<std::uint8_t,rebound_image_size> image{};
    Bytes bytes(){return Bytes(image.data(),image.size());}
    PcMatrixStack stack(){auto b=bytes();return {b.sub(0x1800,0x400),b.i32(0x4000),b.i32(0x4004),b.i32(0x4008)};}
    WallReboundContext context(){
        auto b=bytes();PcStageViews stages{b.sub(0x1c00,4*0x78),b.i32(0x400c),{},b.sub(0x1e40,4)};
        for(unsigned k=0;k<4;++k)stages.descriptors.push_back(b.sub(0x1e00+k*4,4));
        WallResponseContext response{stages,b.sub(0x2000,0x1000),b.sub(0x3000,0x1000),b.u8(0x5800),b.f32(0x4024),b.f32(0x4028)};
        PcRouteContext route{b.sub(0x5100,256),b.sub(0x5300,256),b.sub(0x5800,0x1000),b.u8(0x6a00),b.u32(0x6a04),b.u32(0x6a08),{b.u8(0x6a0c),b.u8(0x6a0d),b.u8(0x6a0e)}};
        return {std::move(response),b.sub(0x5000,8),route,{b.sub(0x6900,128),b.sub(0x6980,8),b.u8(0x6988)}};
    }
};
inline WallReboundFixture make_wall_rebound_fixture(unsigned i,unsigned id){
    WallReboundFixture f;auto b=f.bytes();const auto base=make_wall_response_fixture(i,9);
    std::copy(base.image.begin(),base.image.end(),f.image.begin());
    std::uint32_t r=0x4f523019u^(i*1664525u);
    auto random=[&](){r=r*1664525u+1013904223u;return r;};
    for(std::size_t k=0x5000;k<f.image.size();++k)f.image[k]=std::uint8_t(random()>>24);
    auto scalar=[&](){return float(int(random()%200001)-100000)/4096.0f;};
    for(unsigned k=0;k<4;++k){b.put32(0x1c14+k*0x78,rebound_guest_base+0x1e00+k*4);b.put32(0x1c08+k*0x78,(i+k*3)%14);}
    b.put32(0x5000,i%4==0?b.u32(0x68):0xffffffffu);b.put32(0x5004,(i+7)%14);
    for(unsigned k=0;k<64;++k)b.put32(0x5100+4*k,k<14?((i+k)%4):2u);
    b.put32(0x6b10,i%19);b.put32(0x6b14,i%7==0?random():i%3);
    b.put32(0x6b18,random());
    if(i%16<15){ // first unresolved position 0..14, high bits retained
        auto packed=b.u32(0x6b18);const auto first=i%16;
        for(unsigned n=0;n<14;++n){packed&=~(3u<<(2*n));packed|=((n==first?2u:(n&1u))<<(2*n));}
        b.put32(0x6b18,packed);
    }
    for(unsigned k=0;k<8;++k)b.put32(0x5800+k*0x6c+0x38,(i+k)%17);
    b.put8(0x5800,b.u8(0x401c));
    b.put8(0x6a00,std::uint8_t(i%8));b.put32(0x6a04,i%3==0?4u:i%3);
    b.put32(0x6a08,random());b.put32(0x5b64,random());
    b.put8(0x6a0c,std::uint8_t(i%4==1));b.put8(0x6a0d,std::uint8_t(i%4==2));b.put8(0x6a0e,std::uint8_t(i%4==3));
    b.put32(0x532c,i%4==0||i%4==1?0xffffffffu:random()%5);
    b.put32(0x5330,i%4==0||i%4==2?0xffffffffu:random()%5);
    const std::uint8_t controls[]={0,1,2,3,0x12,0x82,0xf2};b.put8(0x6988,controls[i%7]);
    for(unsigned k=0;k<32;++k)b.put32(0x6900+k*4,1000+k);
    b.put32(0x6980,(i+1)%32);b.put32(0x6984,i%32);
    if(i%9==0)b.put32(0x6980,i%32);
    b.put32(0x6b1c,i%11==0?1000+(i%32):75);
    if(i%5==0)b.put32(0x6900+4*((i/5)%32),75);
    const float assists[]={0,0.25f,0.5f,1.0f,-0.5f,1.5f};b.putf(0xdb4,assists[i%6]);
    b.putf(0xdbc, (i%9==0?-1.0f:1.0f)*float(i%5+1)/16.0f);
    b.putf(0x1c4,float(i%7+1)*0.25f);
    b.putf(0x26c,scalar());
    const float limit=static_cast<float>((static_cast<long double>(b.f32(0xdbc))*b.f32(0x1c4))*30.0f);
    switch(i%8){case 0:b.putf(0x26c,0.0f);break;case 1:b.putf(0x26c,limit);break;case 2:b.putf(0x26c,-limit);break;case 3:b.putf(0x26c,std::nextafter(limit,0.0f));break;case 4:b.putf(0x26c,std::nextafter(limit,INFINITY));break;default:break;}
    if(id==7&&i%31==0)b.put32(0x26c,0x7fc12345); // unordered selects fallback, no libm domain exception
    const unsigned speeds[]={0,99,100,101,0xffffffffu};b.put32(0x1f4,speeds[(i/3)%5]);
    b.put32(0xdf8,i%4==0?1u:0u);b.put8(0x284,i%5==0?1u:0u);
    const std::uint8_t timers[]={0,1,60,127,128,255};b.put8(0xd23,timers[(i/2)%6]);
    // Distinct horizontal normals; include signed zeros and exact quarter-turns.
    std::array<float,3> normal{scalar(),0,scalar()};
    if(i%4==0){const std::array<std::array<float,3>,8> axes{{{1,0,0},{0,0,1},{-1,0,0},{0,0,-1},{0,0,0},{-0.0f,0,-0.0f},{0.6f,0,0.8f},{0.8f,0,-0.6f}}};normal=axes[(i/4)%8];}
    for(unsigned k=0;k<3;++k)b.putf(0x6b00+k*4,normal[k]);
    for(unsigned k=0;k<4;++k)b.putf(0x6b20+k*4,scalar());
    if(i%6==0){b.putf(0x6b20,0);b.putf(0x6b24,0);b.putf(0x6b28,0);b.putf(0x6b2c,0);}
    else if(i%6<4){b.putf(0x6b20,0);b.putf(0x6b24,i%6==1?1.0e-4f:i%6==2?std::nextafter(1.0e-4f,0.0f):std::nextafter(1.0e-4f,1.0f));b.putf(0x6b28,0);b.putf(0x6b2c,0);}
    b.put32(0x6b30,0x6c00);b.put32(0x6b34,i%7==0?0x6c00:0x6c04);
    return f;
}
inline unsigned rebound_stage_count(unsigned id){return id==10?3u:id==11?4u:1u;}
inline unsigned rebound_stage_id(unsigned id,unsigned stage){return id==10?9u:id==11?(stage%2?9u:12u):id;}
inline std::uint32_t run_wall_rebound_native(WallReboundFixture& f,unsigned id){
    auto b=f.bytes();auto c=f.context();std::uint32_t ret=0;
    switch(id){
      case 0:ret=pc_stage_property(c.response.stages,b.u32(0x68),c.stage_cache);break;
      case 1:ret=pc_pack_route(c.route.choices);break;
      case 2:pc_unpack_route(c.route.choices,b.u32(0x6b18));break;
      case 3:pc_save_route_progress(c.route.save,c.route.slot,b.u32(0x6b18),c.route.clock);break;
      case 4:pc_set_route_choice(c.route,b.i32(0x6b10),b.u32(0x6b14));break;
      case 5:ret=pc_get_route_choice(c.route,b.i32(0x6b10));break;
      case 6:pc_enqueue_sound(c.sounds,b.u32(0x6b1c));break;
      case 7:calc_rebound_heading_float(b.sub(0,0x1000),b.i16(0x4034));break;
      case 8:pc_direction_xz(b.f32(0x6b20),b.f32(0x6b24),b.f32(0x6b28),b.f32(0x6b2c),b.sub(b.u32(0x6b30),4),b.sub(b.u32(0x6b34),4));break;
      case 9:cw_rebound_status(b.sub(0,0x1000),b.sub(0x1000,0x800),{b.f32(0x6b00),b.f32(0x6b04),b.f32(0x6b08)},c);break;
      case 12:{auto s=f.stack();calc_friction_status(b.sub(0,0x1000),b.sub(0x1000,0x800),b.f32(0x4020),s,c.response);b.puti(0x4000,std::int32_t(s.current_offset));b.puti(0x4004,s.depth);b.puti(0x4008,s.capacity);break;}
      default:throw std::invalid_argument("unknown rebound selector");
    }
    return ret;
}
}
