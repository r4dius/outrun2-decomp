#include "support/ground_fixture.hpp"
#include <iostream>
#include <stdexcept>
using namespace outrun::driving;
using namespace outrun::testing;
namespace {
void require(bool b,const char* message){if(!b)throw std::runtime_error(message);}
template<class F> void rejects(F&& f){bool caught=false;try{f();}catch(const std::exception&){caught=true;}require(caught,"missing explicit view rejection");}
void test_contact_masks(){
    for(unsigned old=0;old<2;++old)for(unsigned mask=0;mask<16;++mask){
        auto f=make_ground_fixture(128u+old*16u+mask);const auto canary=f.car;
        run_ground_native(f,0);
        for(unsigned k=0;k<4;++k){auto w=f.wheels()[k];
            require(w.f32(0x3c)==((mask&(1u<<k))?0.f:-0.1f),"height must come from cell list hit/miss");
            require(w.u32(0x14)==((mask&(1u<<k))?2u:1u),"queried wheel kind");
        }
        run_ground_native(f,1);
        for(unsigned k=0;k<4;++k)require((f.wheels()[k].u32(0)&1u)==((mask&(1u<<k))?0u:1u),"queried contact mask");
        const auto flags=f.w().u32(0x244);
        require(bool(flags&2u)==(mask==15u)&&bool(flags&4u)==(mask==0u),"all/none contact flags");
        require(bool(flags&0x80u)==(mask==0u&&!old),"transition-to-none pulse");
        run_ground_native(f,1);require((f.w().u32(0x244)&0x80u)==0u,"pulse must clear on second suspension check");
        auto source=Bytes(const_cast<std::uint8_t*>(canary.data()),canary.size());
        require(f.e().u32(0x2b4)==source.u32(0x2b4),"guest parameter pointer overwritten");
    }
}
void test_stack_side_effects(){
    auto f=make_ground_fixture(143);auto s=f.world.matrix();pc_matrix_set_translation(s,{3,9,5});
    run_ground_native(f,0);require(f.w().f32(0x640)==3&&f.w().f32(0x648)==5,"center read work instead of CURRENT matrix");
    f=make_ground_fixture(143);run_ground_native(f,0);
    for(auto wheel:f.wheels())wheel.putf(8,-1.f);
    run_ground_native(f,2);auto active=pc_matrix_translation(f.world.matrix());
    require(active.y>0&&active.y==f.w().f32(0x44),"bump must update work translation AND current slot");
    s=f.world.matrix();pc_matrix_set_translation(s,{999,777,555});
    for(auto wheel:f.wheels())wheel.put32(0,wheel.u32(0)|1u);
    run_ground_native(f,4);auto matrix=f.world.matrix().current();
    for(unsigned k=0;k<64;++k)require(matrix.u8(k)==f.w().u8(0x10+k),"tire_load must reload current matrix even with no contact");
    f=make_ground_fixture(143);run_ground_native(f,0);s=f.world.matrix();s.current().putf(0x14,-1.f);
    for(auto wheel:f.wheels())wheel.putf(0x3c,100.f);
    run_ground_native(f,1);require((f.w().u32(0x244)&4u)!=0,"orientation must use CURRENT matrix up, not work up");
}
void test_views_and_pointer_poison(){
    for(unsigned fault=0;fault<4;++fault){
        auto f=make_ground_fixture(143);auto tables=f.world.tables();auto s=f.world.matrix();auto pred=f.world.prediction();
        CourseWorldQuery query{tables,s,pred};GroundCollisionContext context{query,{0,0}};
        auto e=f.e(),w=f.w(),p=f.p();auto wheels=f.wheels();
        if(fault==0)e=e.sub(0,0x297);if(fault==1)w=w.sub(0,0x64b);if(fault==2)p=p.sub(0,0x27u*0x4cu+3u);if(fault==3)wheels[3]=wheels[3].sub(0,0x3f);
        const auto before=f.car;const auto matrix_before=f.world.image;
        rejects([&]{calc_ground_coli_face(e,w,p,wheels,context);});
        require(f.car==before&&f.world.image==matrix_before,"fixed-view preflight must precede all writes");
    }
    auto normal=make_ground_fixture(143),poison=normal;
    poison.e().put32(0x2b4,0xdeadc001u);for(unsigned k=0;k<4;++k)poison.w().put32(0x248+k*4,0xffff0001u+k);
    for(unsigned stage=0;stage<10;++stage){
        run_ground_native(normal,stage%5);run_ground_native(poison,stage%5);
        require(poison.e().u32(0x2b4)==0xdeadc001u,"native followed or rewrote guest pointer");
        // Normalize ONLY the deliberately different pointer bytes in a comparison
        // copy. The native retained state is never resynchronised or overwritten.
        auto image=poison.car;Bytes view(image.data(),image.size());view.put32(0x2b4,normal.e().u32(0x2b4));
        for(unsigned k=0;k<4;++k){require(poison.w().u32(0x248+k*4)==0xffff0001u+k,"wheel guest pointer rewritten");view.put32(event_size+0x248+k*4,normal.w().u32(0x248+k*4));}
        require(image==normal.car&&poison.world.image==normal.world.image,"explicit-view chain depends on serialized pointers");
    }
}
}
int main(){try{test_contact_masks();test_stack_side_effects();test_views_and_pointer_poison();std::cout<<"queried contact masks, retained flags, matrix side effects and explicit views passed\n";return 0;}catch(const std::exception& ex){std::cerr<<ex.what()<<'\n';return 1;}}
