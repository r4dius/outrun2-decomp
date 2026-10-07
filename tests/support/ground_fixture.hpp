#pragma once
#include "course_world_fixture.hpp"
#include "driving/pc_ground_collision.hpp"
#include "driving/pc_suspension.hpp"
#include "driving/pc_wheel_dynamics.hpp"
namespace outrun::testing {
constexpr std::size_t ground_car_size=event_size+work_size+parameter_size;
struct GroundFixture {
    CourseWorldFixture world;
    std::array<std::uint8_t,ground_car_size> car{};
    Bytes e(){return {car.data(),event_size};}
    Bytes w(){return {car.data()+event_size,work_size};}
    Bytes p(){return {car.data()+event_size+work_size,parameter_size};}
    void rebuild_grids(){
        world.rebuild_grids();const auto b=world.bytes();
        if(b.u32(0x4710)==0x474d4153u){
            const auto mask=b.u32(0x4708);
            for(auto& grid:world.grids)grid.fill(250);
            for(unsigned k=0;k<4;++k){
                const auto cell=calc_collision_area(k&1u?1.f:-1.f,k<2u?-2.f:2.f);
                if(mask&(1u<<k))world.grids[0][cell]=16;
            }
        }
    }
    std::array<Bytes,4> wheels(){return embedded_wheels(w());}
};
inline GroundFixture make_ground_fixture(unsigned i,bool standalone=false){
    GroundFixture f;f.world=make_course_world_fixture(i,7);
    auto m=f.world.bytes(),e=f.e(),w=f.w(),p=f.p();
    std::mt19937 gen(0x4f524716u^(i*1664525u));
    for(auto& b:f.car)b=std::uint8_t(gen());
    auto sample=[&](float lo,float hi){return lo+(hi-lo)*(float(gen()&65535u)/65535.f);};
    m.put32(0x4710,0);
    e.put32(0x2b4,0x2f005000u);
    for(unsigned k=0;k<4;++k)w.put32(0x248+k*4,0x2f003000u+static_cast<std::uint32_t>(embedded_wheel_offsets[k]));
    e.put32(0x5c,(i/7u)%4u);e.puti(0x60,0);e.put32(0x230,i%9u==0u?0xffffffffu:0u);e.put32(0x1c0,0);
    e.put8(0x283,i%13u==0u?1:0);e.put32(0x248,0xabcdef01u);
    // The selection root has a shared total-length/force-range setting.
    // Each of the four courses is generated with matching run metadata.
    for(unsigned t=0;t<4;++t){
        auto b=m.sub(t*4096u,4096);const auto n=b.i32(0x14c);
        for(int k=0;k<n;++k)b.put16(0x400+std::size_t(k)*0x40+0x3e,static_cast<std::uint16_t>(gen()));
    }
    if(i>=128u&&i%5u==0u)e.puti(0x1c0,std::max(0,m.i32(0x14c)-1));
    const float sx=sample(0.5f,1.5f),sz=sample(1.2f,2.8f);
    const float angle=i<128u?0.f:sample(-0.15f,0.15f),cs=std::cos(angle),sn=std::sin(angle);
    for(unsigned k=0;k<16;++k)w.putf(0x10+k*4,0);
    w.putf(0x10,cs);w.putf(0x18,-sn);w.putf(0x24,1);w.putf(0x30,sn);w.putf(0x38,cs);w.putf(0x4c,1);
    world_put_probe(w,0x40,world_probe(m,0x4600));
    // Distinct body transform and active transform cases catch accidental work-only use.
    auto stack=f.world.matrix();pc_matrix_load(stack,w.sub(0x10,64));
    if(standalone&&i>=128u&&i%7u==0u){auto active=pc_matrix_translation(stack);active.y+=0.25f;pc_matrix_set_translation(stack,active);}
    m.puti(0x4558,15); // Overflow/underflow already exhausted in the matrix corpus.
    m.putf(0x4700,sample(-3,3));m.putf(0x4704,sample(-3,3));
    p.putf(0,sample(900,2400));
    for(unsigned axle=0;axle<2;++axle){
        p.putf((axle+0x26u)*0x4cu,sample(0.1f,0.6f));
        p.putf((axle+0x0eu)*0x4cu,0.2f);p.putf((axle+0x10u)*0x4cu,0.1f);p.putf((axle+0x12u)*0x4cu,0.4f);
        p.putf((axle+0x16u)*0x4cu,sample(3000,27000));p.putf((axle+0x18u)*0x4cu,sample(500,9500));
    }
    auto wheels=f.wheels();
    for(unsigned k=0;k<4;++k){auto a=wheels[k];
        a.putf(4,k&1u?sx:-sx);a.putf(0x0c,k<2u?-sz:sz);
        a.putf(8,sample(0.3f,0.8f));a.putf(0x28,sample(0,0.3f)); // Deliberately not the same field.
        a.putf(0x18,sample(0,0.4f));a.putf(0x38,sample(1000,6000));
        a.putf(0x3c,-9876.5f);a.put32(0x10,0xcafebabe);a.put32(0x14,0xdeadbeef);
    }
    if(i>=128u&&i<168u){
        const unsigned mask=i<160u?(i-128u)&15u:15u;const bool old_none=((i-128u)&16u)!=0;
        configure_world_boundary(f.world,0,7);m=f.world.bytes();
        m.put32(0x4648,1);m.putf(0x4434,0); // Only primary root, identity transform.
        m.putf(0x4700,0);m.putf(0x4704,0);m.put32(0x4708,mask);m.put32(0x4710,0x474d4153u);
        const CourseProbe vertices[]={{-10,0,-10},{10,0,-10},{10,0,10},{-10,0,10}};
        for(unsigned v=0;v<4;++v)world_put_probe(m,0x400+v*12,vertices[v]);
        e.put32(0x5c,0);e.puti(0x60,0);e.put32(0x230,0);e.put32(0x1c0,0);
        w.put32(0x244,(w.u32(0x244)&~0x86u)|(old_none?4u:0u));
        for(unsigned k=0;k<16;++k)w.putf(0x10+k*4,(k%5u==0u)?1.f:0.f);
        auto active=f.world.matrix();pc_matrix_load(active,w.sub(0x10,64));
        for(unsigned axle=0;axle<2;++axle)p.putf((axle+0x26u)*0x4cu,0.25f);
        for(unsigned k=0;k<4;++k){auto a=wheels[k];a.putf(4,k&1u?1.f:-1.f);a.putf(0xc,k<2u?-2.f:2.f);a.putf(8,0.5f);a.putf(0x28,0.25f);}
        if(i>=160u){
            float x=0.25f,z=0.5f;
            if(i==160u)x=z=0;
            if(i==162u)x=std::nextafter(x,0.f);
            if(i==163u)x=std::nextafter(x,1.f);
            if(i==164u)x=std::nextafter(std::nextafter(x,1.f),1.f);
            if(i==165u)x=z=1e-20f;
            if(i==166u)z=-z;
            if(i==167u)z=std::nextafter(z,1.f);
            for(unsigned k=0;k<4;++k){wheels[k].putf(4,k&1u?x:-x);wheels[k].putf(0xc,k<2u?-z:z);}
        }
        f.rebuild_grids();
    }
    return f;
}
// Stages are individual original entries, NOT an implementation of CommonPlCar.
// Repeating 0..4 retains both car and world state and compares after EACH stage.
inline void run_ground_native(GroundFixture& f,unsigned stage){
    auto tables=f.world.tables();auto s=f.world.matrix();auto pred=f.world.prediction();
    CourseWorldQuery q{tables,s,pred};GroundCollisionContext context{q,{f.world.bytes().f32(0x4700),f.world.bytes().f32(0x4704)}};
    switch(stage){
    case 0:calc_ground_coli_face(f.e(),f.w(),f.p(),f.wheels(),context);break;
    case 1:car_sus_coli_check(f.e(),f.w(),f.p(),f.wheels(),s);break;
    case 2:car_sus_bump_push(f.e(),f.w(),f.p(),f.wheels(),s);break;
    case 3:suspension_force(f.w(),f.p());break;
    case 4:tire_load(f.e(),f.w(),f.p(),{},s);break;
    default:throw std::invalid_argument("unknown ground stage");
    }
    f.world.save(s,pred);
}
}
