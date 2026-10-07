#pragma once
// Deterministic synthetic PC-layout tables, not commercial circuit data.
#include "driving/pc_course_query.hpp"
#include <array>
#include <cmath>
#include <random>
#include <limits>
namespace outrun::testing {
using namespace outrun::driving;
struct CourseQueryFixture {
    std::array<std::uint8_t,4096> image{};
    Bytes bytes(){return Bytes(image.data(),image.size());}
    CourseCollisionTables tables(){
        auto b=bytes();const auto options=b.u32(0xf04);
        return {{b.sub(0x140,0x10),b.sub(0x200,0x80),b.sub(0x280,0x100),b.i32(0xf08),bool(options&1),bool(options&2)},
                b.sub(0x180,0x20),b.sub(0x400,0x400),b.sub(0x800,0x300),b.sub(0xc00,0x200),b.u32(0xf00),bool(options&4)};
    }
    CourseSplineTuning tuning(){auto b=bytes();return {b.f32(0xf20),b.f32(0xf24)};}
};
inline CourseQueryFixture make_course_query_fixture(unsigned i,bool road=false,bool secondary=false){
    CourseQueryFixture f;std::mt19937 rng(0x4f523015u^(i*1664525u));
    for(auto& b:f.image)b=std::uint8_t(rng());
    auto m=f.bytes();auto sample=[&](float lo,float hi){return lo+(hi-lo)*(float(rng()&0xffffffu)/float(0xffffffu));};
    const unsigned mask=i&15u,sides=(mask>>2)&3u;
    unsigned cols=sides==0?1u:(sides==3?3u:2u),rows=3u;
    unsigned col=sides==1||sides==3?1u:0u,row=1u;
    const unsigned type=(i/16u)&3u;
    if(i>=256u&&!road&&i%9u==0u){cols=1;col=0;}
    const unsigned n=cols*rows,index=row*cols+col;
    m.put32(0xf00,type);m.put32(0xf04,6u|((i%7u==0u)?1u:0u));m.puti(0xf08,int(rows));
    m.puti(0xf10,int(index));m.put32(0xf14,0x400u);m.put16(0xf18,16);
    m.putf(0xf20,sample(0.5f,1.5f));m.putf(0xf24,sample(0.5f,1.5f));
    m.put32(0x140,0xdeadbeefu);m.put32(0x144,0xffffffffu); // ignored serialized pointers
    m.puti(0x148,int(n));m.puti(0x14c,int(n));
    m.put32(0x100,type);m.puti(0x104,0);m.put16(0x108,std::uint16_t(row));
    std::array<float,4> xs{};std::array<float,4> zs{};
    const float width=(i<256u)?10.f:sample(4,20),length=(i<256u)?16.f:sample(5,30);
    for(unsigned c=0;c<=cols;++c)xs[c]=float(c)*width;
    for(unsigned r=0;r<=rows;++r)zs[r]=-float(r)*length;
    const float ax=(i<256u)?0.f:sample(-0.1f,0.1f),az=(i<256u)?0.f:sample(-0.1f,0.1f);
    auto vertex=[&](unsigned r,unsigned c){return CourseVec3{xs[c],ax*xs[c]+az*zs[r],zs[r]};};
    auto putv=[&](std::size_t o,CourseVec3 v){m.putf(o,v.x);m.putf(o+4,v.y);m.putf(o+8,v.z);};
    for(unsigned r=0;r<rows;++r){
        m.put16(0x280+r*4,std::uint16_t(r*cols));m.put16(0x282+r*4,std::uint16_t(r*cols+cols-1));
        for(unsigned c=0;c<cols;++c){const auto j=r*cols+c;const auto p=0x400+j*0x40;
            putv(p,vertex(r+1,c));putv(p+12,vertex(r+1,c+1));putv(p+24,vertex(r,c+1));putv(p+36,vertex(r,c));
            m.put16(p+0x3c,2);m.put8(0x180+j,1);m.put16(0x200+j*2,std::uint16_t(r));
            for(unsigned v=0;v<4;++v)putv(0x800+j*0x30+v*12,CourseVec3{-ax,1.f,-az});
        }
    }
    // Both longitudinal checks have independent discontinuities. Exact 0.5,
    // its float neighbors, and the second vertex are exercised separately.
    const auto forward=index+cols,back=index-cols;
    if(!(mask&1u))m.putf(0x400+forward*0x40+0x24+4,m.f32(0x400+forward*0x40+0x24+4)+1.f);
    if(!(mask&2u))m.putf(0x400+back*0x40+4,m.f32(0x400+back*0x40+4)+1.f);
    if(i>=128u&&i<224u){
        static const float limits[]={0.f,std::nextafter(0.5f,0.f),0.5f,std::nextafter(0.5f,1.f),1.f,-0.5f};
        for(unsigned j=0;j<n;++j)for(unsigned v=0;v<4;++v)m.putf(0x400+j*0x40+v*12+4,0.f);
        m.putf(0x400+forward*0x40+((i/6u)&1u?0x18:0x24)+4,limits[i%6u]);
        m.putf(0x400+back*0x40+((i/12u)&1u?0x0c:0x00)+4,limits[(i/6u)%6u]);
    }
    if(i>=256u){
        const unsigned kind=(i/3u)&255u;for(unsigned j=0;j<n;++j)m.put8(0x180+j,std::uint8_t(kind));
        if(!road&&i%31u==0u)m.put32(0xf04,m.u32(0xf04)&~2u); // missing header
        if(!road&&i%29u==0u)m.put32(0xf04,m.u32(0xf04)&~4u); // missing polygon root
        if(!road&&i%37u==0u)m.puti(0x148,1); // forward sequential header +8 boundary
        if(!road&&i%41u==0u)m.puti(0xf10,0); // first polygon
        if(!road&&i%43u==0u)m.puti(0xf10,int(n-1));
    }
    m.putf(0xe00,(xs[col]+xs[col+1])*0.5f);m.putf(0xe08,(zs[row]+zs[row+1])*0.5f);
    m.putf(0x30,sample(-40,40));m.puti(0x34,-9876);m.put16(0x38,0xabcd);
    if(road){
        static const unsigned modes[]={0x400,0x100,0,1,0x500,0x401};
        const auto mode=modes[(i/64u)%6u];m.put32(0xf14,mode);
        const unsigned listoff=1u+(i%25u);m.put16(0xf18,std::uint16_t(listoff));
        const unsigned listcount=(i%47u==0u)?0u:n;
        m.put16(0xc00+listoff*2,std::uint16_t(listcount));
        for(unsigned j=0;j<listcount;++j){const auto idx=(j+(i/n))%n;m.put16(0xc02+listoff*2+j*2,std::uint16_t(idx));}
        for(unsigned j=0;j<n;++j)m.put16(0x400+j*0x40+0x3c,std::uint16_t(2u|((i/17u)&0xfff8u)));
        if(i%13u==0u){m.put16(0x400+index*0x40+0x3c,1);m.putf(0xe00,xs[col]+width*0.75f);m.putf(0xe08,zs[row]-length*0.75f);}
        // Point on either side of each exact edge/corner, and far outside.
        if(i%5u==0u){const auto edge=(i/5u)%12u;
            const float xlo=xs[col],xhi=xs[col+1],zlo=zs[row],zhi=zs[row+1];
            if(edge<6u){static const int dirs[]={0,-1,1,0,-1,1};const float x=edge<3?xlo:xhi;
                m.putf(0xe00,dirs[edge]?std::nextafter(x,dirs[edge]<0?-INFINITY:INFINITY):x);
            }else {static const int dirs[]={0,-1,1,0,-1,1};const float z=edge<9?zlo:zhi;
                m.putf(0xe08,dirs[edge-6]?std::nextafter(z,dirs[edge-6]<0?-INFINITY:INFINITY):z);}
        }
        if(i%19u==0u){m.putf(0xe00,-1000);m.putf(0xe08,-1000);}
        // Stacked surfaces: varying order, ties, explicit first-hit flags.
        // Disable neighbor classification, not the geometry query itself.
        if(i%4u==0u&&i%47u!=0u){
            auto source=m.sub(0x400+index*0x40,0x40);std::array<std::uint8_t,0x40> p{};
            for(unsigned k=0;k<0x40;++k)p[k]=source.u8(k);
            for(unsigned j=0;j<n;++j){
                for(unsigned k=0;k<0x40;++k)m.put8(0x400+j*0x40+k,p[k]);
                const float h=(i%8u==0u)?float(int(j%3u)-1)*4.f:sample(-20,20);
                for(unsigned v=0;v<4;++v)m.putf(0x400+j*0x40+v*12+4,h);
                m.put8(0x180+j,0);m.put16(0x400+j*0x40+0x3c,std::uint16_t(2u|((i%12u==0u&&j==n/2u)?4u:0u)));
            }
            m.putf(0x30,0.f);
        }
    }
    if(secondary){
        const auto count=unsigned((i*7u)%15u);m.puti(0x14c,int(count));
        static const int modes[]={0,100,101,102,103,99,104,-1};
        m.puti(0x104,modes[(i/8u)%8u]);
        const std::uint16_t wanted=(i%3u==0u)?0x0101u:std::uint16_t(i%7u);
        m.put16(0x108,(i%101u==0u)?0xffffu:wanted);
        for(unsigned j=0;j<=count+1u&&j<32u;++j)m.put16(0x200+j*2u,(i%4u==0u)?wanted:std::uint16_t((j/3u+i)%7u));
        for(unsigned j=0;j<16u;++j)m.put16(0x400+j*0x40+0x3c,std::uint16_t((j%3u?0x400u:0x800u)|((i&1)?0x400u:0u)));
        if(i%11u==0u)m.put32(0xf04,m.u32(0xf04)&~2u);
        if(i%13u==0u)m.put32(0xf04,m.u32(0xf04)&~4u);
    }
    return f;
}
inline std::uint32_t run_course_query_native(CourseQueryFixture& f,unsigned id){
    auto b=f.bytes();const auto t=f.tables();const auto index=b.i32(0xf10);std::int32_t output=b.i32(id*4u);
    if(id==0u){auto a=b.i32(0x20),z=b.i32(0x24);const auto ret=find_secondary_course_run(t,b.i16(0x108),b.i32(0x104),a,z);
        b.puti(0x20,a);b.puti(0x24,z);return static_cast<std::uint32_t>(ret);}
    using Fn=bool(*)(const CourseCollisionTables&,std::int32_t,std::int32_t&);
    static constexpr Fn functions[]={coli_get_forward_polygon_number,coli_get_back_polygon_number,coli_get_left_polygon_number,coli_get_right_polygon_number};
    if(id<=4u){const bool ret=functions[id-1u](t,index,output);b.puti(id*4u,output);return ret?1u:0u;}
    if(id==5u){const auto result=get_road_cond(t,static_cast<std::uint16_t>(b.i16(0xf18)),b.u32(0xf14),b.f32(0xe00),b.f32(0x30),b.f32(0xe08),f.tuning());
        b.putf(0x30,result.y);b.puti(0x34,result.polygon);b.put16(0x38,result.flags);return 0;}
    throw std::invalid_argument("unknown course query fixture routine");
}
}
