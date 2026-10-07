#pragma once
// Synthetic PC-format geometry, never commercial assets or executable bytes.
#include "driving/pc_wall_geometry.hpp"
#include <array>
#include <cmath>
#include <random>
#include <limits>
namespace outrun::testing {
using namespace outrun::driving;
constexpr std::size_t wall_image_size=0x1800;
constexpr unsigned wall_sequence[]={2,3,6,4,5,6,4,5};
struct WallGeometryFixture {
    std::array<std::uint8_t,wall_image_size> image{};
    Bytes bytes(){return {image.data(),image.size()};}
    PcMatrixStack matrix(){auto b=bytes();return {b.sub(0xf00,0x400),b.i32(0x1300),b.i32(0x1304),b.i32(0x1308)};}
    void save(const PcMatrixStack& s){auto b=bytes();b.puti(0x1300,static_cast<std::int32_t>(s.current_offset));b.puti(0x1304,s.depth);b.puti(0x1308,s.capacity);}
    CourseWorldTables tables(){
        auto b=bytes();CourseWorldTables w;
        for(auto& c:w.courses){c.polygons=b.sub(0,0x400);c.polygons_present=true;}
        w.transforms={b.sub(0x400,64),b.sub(0x440,64)};return w;
    }
};
inline CourseProbe wall_read(Bytes b,std::size_t o){return {b.f32(o),b.f32(o+4),b.f32(o+8)};}
inline void wall_write(Bytes b,std::size_t o,CourseProbe p){b.putf(o,p.x);b.putf(o+4,p.y);b.putf(o+8,p.z);}
inline void wall_matrix(Bytes m,float yaw,float x,float y,float z){
    for(unsigned k=0;k<64;k+=4)m.put32(k,0);
    const float s=std::sin(yaw),c=std::cos(yaw);
    m.putf(0,c);m.putf(8,-s);m.putf(0x14,1);m.putf(0x20,s);m.putf(0x28,c);
    m.putf(0x30,x);m.putf(0x34,y);m.putf(0x38,z);m.putf(0x3c,1);
}
inline void wall_prepare_response(Bytes b){
    // Actual CbwColiWall mapping after its CalcColiWallFace call:
    // FACE_WORK = {b normal, c offset plane, a original boundary plane}.
    for(unsigned k=0;k<12;k+=4){
        b.put32(0x4c0+k,b.u32(0x48c+k));
        b.put32(0x4cc+k,b.u32(0x498+k));
        b.put32(0x4d8+k,b.u32(0x480+k));
    }
}
inline WallGeometryFixture make_wall_geometry_fixture(unsigned i,unsigned id){
    WallGeometryFixture f;std::mt19937 rng(0x4f523017u^(i*1664525u));
    for(auto& c:f.image)c=static_cast<std::uint8_t>(rng());
    auto b=f.bytes();
    auto sample=[&](float a,float z){return a+(z-a)*(static_cast<float>(rng()&65535u)/65535.f);};
    for(unsigned j=0;j<16;++j){
        const float x=sample(-20,20),z=sample(-20,20),dx=sample(.05f,12),dz=sample(.05f,12);
        wall_write(b,j*64,{x,sample(-5,5),z});wall_write(b,j*64+12,{x+dx,sample(-5,5),z});
        wall_write(b,j*64+24,{x+dx,sample(-5,5),z+dz});wall_write(b,j*64+36,{x,sample(-5,5),z+dz});
    }
    wall_matrix(b.sub(0x400,64),sample(-1,1),sample(-10,10),sample(-5,5),sample(-10,10));
    wall_matrix(b.sub(0x440,64),sample(-1,1),sample(-10,10),sample(-5,5),sample(-10,10));
    wall_matrix(b.sub(0x710,64),sample(-.3f,.3f),sample(-10,10),sample(-5,5),sample(-10,10));
    b.put32(0x1300,128);b.put32(0x1304,2);b.put32(0x1308,15);b.put32(0x1310,i%4);b.put32(0x1314,i%16);
    b.putf(0x1318,sample(-25,25));b.putf(0x131c,sample(-25,25));b.put32(0x1340,0x710);
    // Intentionally DIFFERENT matrix translation from work in isolated tests.
    wall_matrix(b.sub(0xf80,64),sample(-.3f,.3f),sample(-10,10),sample(-5,5),sample(-10,10));
    wall_write(b,0x1360,{sample(-15,15),sample(-15,15),sample(-15,15)});
    for(unsigned k=0;k<48;k+=4)b.put32(0x480+k,b.u32((i%16)*64+k));
    const float a=sample(-3.14f,3.14f);
    wall_write(b,0x4c0,{std::sin(a),id==4u?sample(-.1f,.1f):0.f,std::cos(a)});
    wall_write(b,0x4cc,{sample(-10,10),sample(-5,5),sample(-10,10)});
    wall_write(b,0x4d8,{sample(-10,10),sample(-5,5),sample(-10,10)});
    static constexpr int counts[]={-7,0,1,2,3,4,8,12,16,31,32,33,0x7fffffff};
    const int declared=id==7u?4:counts[i%13];b.puti(0x500,declared);
    for(unsigned k=0;k<32;++k)wall_write(b,0x504+k*12,{sample(-4,4),sample(-2,2),sample(-4,4)});
    // <=12 selected points: beyond that the ORIGINAL ColiSet local array overflows.
    static constexpr unsigned masks[]={0u,0x80000000u,0x00000001u,0xf0000000u,0xaaaaaaaau,0x55555555u,0xfff00000u};
    auto mask=masks[i%7];if(declared>12&&(i%7==4||i%7==5))mask&=0xfff00000u;
    b.put32(0x1324,mask);
    // 128 structured cases exercise exact zero/boundary signs, count clamps,
    // independently sloped normals, both side branches and matrix side effects.
    if(i<128u){
        wall_matrix(b.sub(0x400,64),0,0,3,0);wall_matrix(b.sub(0x440,64),0,2,-4,-1);
        wall_matrix(b.sub(0x710,64),0,0,2,0);wall_matrix(b.sub(0xf80,64),0,0,0,0);
        for(unsigned j=0;j<16;++j){
            wall_write(b,j*64,{0,1,0});wall_write(b,j*64+12,{8,2,0});
            wall_write(b,j*64+24,{8,3,2});wall_write(b,j*64+36,{0,4,2});
        }
        for(unsigned k=0;k<48;k+=4)b.put32(0x480+k,b.u32((i%16)*64+k));
        static constexpr float sides[]={3.999999761581421875f,4.f,4.000000476837158203125f,-10,20,0,-0.f,8};
        b.putf(0x1318,sides[i%8]);b.putf(0x131c,1);
        wall_write(b,0x4c0,{1,0,0});wall_write(b,0x4cc,{.01f,0,0});wall_write(b,0x4d8,{0,0,0});
        const unsigned hitmask=(i/8u)%16u;
        for(unsigned k=0;k<32;++k)wall_write(b,0x504+k*12,{(hitmask&(1u<<(k%4)))?-1.f:1.f,float(k%3)-1.f,float(k)-2.f});
        if(id==7u){b.putf(0x740,i&1u?8.25f:-.25f);b.putf(0x1318,b.f32(0x740));}
        // Exact dot zero, signed zero, two neighboring finite projections.
        if(id==4u||id==5u){
            static constexpr float near[]={0.f,-0.f,.01f,.0099999988451600074768f,.010000000707805156708f,-.01f,-1.f,1.f};
            b.putf(0x504,near[i%8]);
        }
    }
    if(id==5u && i==124u){
        // A negative extended dot spills to -0.0f; the PC must STILL select it.
        wall_matrix(b.sub(0x710,64),0,0,0,0);b.puti(0x500,1);b.put32(0x1324,0x80000000u);
        wall_write(b,0x4c0,{std::numeric_limits<float>::min(),0,0});
        wall_write(b,0x504,{-std::numeric_limits<float>::denorm_min(),0,0});
    }
    if(id==5u && i==125u){
        // Exercise ALL 12 original stack hit slots, not only typical four-point cars.
        wall_matrix(b.sub(0x710,64),0,0,0,0);b.puti(0x500,32);b.put32(0x1324,0xfff00000u);
        wall_write(b,0x4c0,{1,0,0});
        for(unsigned k=0;k<12;++k)wall_write(b,0x504+k*12,{-float(k+1),float(k),2});
    }
    if(id==3u && i==126u){
        // Original degenerate face produces non-finite outputs; no epsilon repair.
        for(unsigned k=0;k<4;++k)wall_write(b,0x480+k*12,{0,0,0});
    }
    for(unsigned k=0;k<4;++k)b.put32(0x1380+k*4,0x480+k*12);
    if(id==2u && i%29u==1u){
        const unsigned reorder[]={12,0,36,24};
        for(unsigned k=0;k<4;++k)b.put32(0x1380+k*4,(i%16)*64+reorder[k]);
    }
    if(id==3u && i%37u==5u){
        switch((i/37u)%3u){case 0:b.put32(0x1384,0x480);break;case 1:b.put32(0x138c,0x498);break;case 2:b.put32(0x1388,0x48c);break;}
    }
    if(id==0u){
        if(i%3)b.put32(0x1340,0xf80u+(i%3==1?4u:0xfffffffcu));
        // Source and destination share storage in 2/3 cases; compare forward copies.
    }
    if(id==2u&&i%31u==0u){b.puti(0x1308,3);} // Push overflow still changes depth, then Pop moves pointer.
    // Poison guest fields which the native path must never follow.
    for(unsigned k=0;k<4;++k)b.put32(0x700+0x248+k*4,0xf0bad000u+k*0x100u);
    return f;
}
inline std::uint32_t run_wall_geometry_native(WallGeometryFixture& f,unsigned id){
    auto b=f.bytes();auto s=f.matrix();std::uint32_t ret=0;
    if(id==0u)pc_matrix_load_rotation(s,b.sub(b.u32(0x1340),64));
    else if(id==1u)wall_write(b,0x1370,pc_matrix_inverse_vector(s,wall_read(b,0x1360)));
    else if(id==2u){auto w=f.tables();cop_coli_point(w,b.u32(0x1314),b.u32(0x1310),s,
        {b.sub(b.u32(0x1380),12),b.sub(b.u32(0x1384),12),b.sub(b.u32(0x1388),12),b.sub(b.u32(0x138c),12)});}
    else if(id==3u)calc_coli_wall_face(b.f32(0x1318),b.f32(0x131c),
        {b.sub(b.u32(0x1380),12),b.sub(b.u32(0x1384),12),b.sub(b.u32(0x1388),12),b.sub(b.u32(0x138c),12)});
    else if(id==4u){auto r=push_outpos_mat_obsolete(b.sub(0x700,0x800),b.sub(0x4c0,36),b.sub(0x500,388),s);
        b.put32(0x1324,r.mask);b.putf(0x1328,r.distance);}
    else if(id==5u)ret=coli_set_obsolete(b.sub(0x700,0x800),b.sub(0x4c0,36),b.sub(0x500,388),b.u32(0x1324),s,b.sub(0x132c,12))?1u:0u;
    else if(id==6u){wall_prepare_response(b);pc_matrix_load(s,b.sub(0x710,64));}
    else throw std::invalid_argument("unknown wall native fixture id");
    f.save(s);return ret;
}
}
