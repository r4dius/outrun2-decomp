#include "platform/frontend_fixed_choice.hpp"
#include <array>
#include <cstdio>
#include <cstdlib>
#include <vector>
using namespace outrun::platform;
using outrun::driving::Bytes;
#define CHECK(x) do {if(!(x)){std::fprintf(stderr,"line %d: %s\n",__LINE__,#x);std::exit(1);}}while(0)
// Resource/input implementations are real; authored timing, audio and manager
// leaves are explicit fixtures. This does not validate the external managers.
struct Fixture {
    std::array<std::uint8_t,0x154> child{};
    std::array<std::uint8_t,0xe00> owner{};
    Bytes b{child.data(),child.size()},root{owner.data(),owner.size()};
    FrontendSprites sprites;
    FrontendUiResources ui{sprites};
    outrun::driving::PcUiNotifyGlobals globals;
    FrontendInputSnapshot input;
    unsigned repeat{},result{},fail_pc{};
    std::vector<unsigned> effects,services;
    FixedChoiceServices s{ui,root,globals,input,repeat};
    Fixture(){
        CHECK(sprites.bind_timing(0x44,std::vector<FrontendSpriteTiming>(224,{120,60})));
        CHECK(sprites.bind_timing(0x2c,std::vector<FrontendSpriteTiming>(64,{52,60})));
        CHECK(ui.commands(0x442ac0,root.sub(0x51c,owner.size()-0x51c),nullptr,0,globals,result));
        ui.effect_user=this;ui.effect_4249f0=[](void* p,unsigned id){static_cast<Fixture*>(p)->effects.push_back(id);return true;};
        s.user=this;s.external=[](void* p,unsigned pc){auto& f=*static_cast<Fixture*>(p);f.services.push_back(pc);return pc!=f.fail_pc;};
    }
    void init(unsigned key,unsigned flags=0){
        root.put32(0x20c,flags);CHECK(fixed_choice_construct(child.data(),child.size(),key,repeat));
        CHECK(fixed_choice_init(b,key,s));CHECK(b.u32(0xe0)==4);
    }
    bool tick(unsigned key,unsigned mask=0){input.feature_mask=mask;s.timer+=1.0f/60; sprites.tick();return fixed_choice_tick(child.data(),child.size(),key,s,result);}
    const FrontendSprite& image(){const auto* image=sprites.get(b.u32(0x40));CHECK(image);return *image;}
};
int main(){
    for(unsigned key:{1u,2u}){
        Fixture f;f.init(key);CHECK(f.tick(key));CHECK(f.image().token==(key==1?0x4400dd:0x440017));
        CHECK(f.image().first==20&&f.image().last==20&&f.image().mode==0);
        CHECK(f.tick(key,0x400)&&f.b.u32(0xe8)==0); // up is not horizontal selection
        for(unsigned i=1;i<=4;++i){
            CHECK(f.tick(key,0x2000));CHECK(f.b.u32(0xe8)==i);CHECK(f.b.u32(0xe4)==i-1);
            CHECK(f.image().first==20*i&&f.image().last==20*(i+1)&&f.image().speed==1);
            CHECK(f.tick(key));
        }
        CHECK(f.tick(key,0x2000)&&f.b.u32(0xe8)==4); // no wrapping
        CHECK(f.tick(key,0x1000)&&f.b.u32(0xe8)==3);
        CHECK(f.image().first==100&&f.image().last==80&&f.image().speed==-1);
        for(unsigned i=0;i<25;++i)CHECK(f.tick(key));
        CHECK(f.image().first==80&&f.image().last==80&&f.image().mode==0);
        const unsigned sprite=f.b.u32(0x40);CHECK(fixed_choice_suspend(f.b,key,f.s));CHECK(!f.sprites.get(sprite)->allocated);
    }
    constexpr unsigned destinations[2][5]{{2,6,0x53,0x31,0x2c},{0x3a,0x24,0x24,0x24,0x2b}};
    constexpr unsigned modes[]{0,0x20,0x40,0x80,0xc0};
    for(unsigned key:{1u,2u})for(unsigned choice=0;choice<5;++choice){
        Fixture f;f.init(key,0x1000);for(unsigned i=0;i<choice;++i)CHECK(f.tick(key,0x2000));
        CHECK(f.tick(key,4)&&f.result==0&&f.b.u8(0x14c)==1);
        CHECK(f.b.u32(4)==destinations[key-1][choice]);CHECK(f.b.u32(0x40)==~0u);
        CHECK(f.root.u32(0x20c)==(0x1000|(key==1?choice*4:modes[choice])));
        CHECK(f.tick(key)&&f.result==(key==1&&choice==2?5u:1u));
        if(key==1&&choice<3)CHECK(f.root.u32(4)==choice);
        if(key==1&&(choice==2||choice==3))CHECK((f.services==std::vector<unsigned>{0x4165c0,0x4f3cc0}));
        if(key==2)CHECK((f.services==std::vector<unsigned>{0x4940d0}));
    }
    for(unsigned key:{1u,2u})for(unsigned choice=0;choice<5;++choice){
        Fixture f;f.init(key,0x1000|(key==1?choice*4:modes[choice]));CHECK(f.b.u32(0xe8)==choice);
        CHECK(f.tick(key));CHECK(f.image().first==20*(choice+1));
        f.b.put32(4,0xdead);CHECK(f.tick(key,8));CHECK(f.root.u32(0x20c)==0x1000);
        CHECK(f.b.u32(4)==(key==1?0u:0xdeadu));CHECK(f.tick(key)&&f.result==(key==1?3u:2u));
    }
    {Fixture f;f.s.external=nullptr;CHECK(fixed_choice_construct(f.child.data(),f.child.size(),2,f.repeat));
     CHECK(!fixed_choice_init(f.b,2,f.s)&&f.s.missing==0x4940d0);}
    for(unsigned missing:{0x4165c0u,0x4f3cc0u}){
        Fixture f;f.init(1);f.fail_pc=missing;CHECK(f.tick(1,0x2000));CHECK(f.tick(1,0x2000));
        CHECK(!f.tick(1,4)&&f.s.missing==missing&&!f.b.u8(0x14c));
    }
    {Fixture f;f.init(1);f.ui.effect_4249f0=nullptr;CHECK(!f.tick(1,0x2000)&&f.s.missing==0x4249f0);}
    {Fixture f;CHECK(!fixed_choice_construct(f.child.data(),f.child.size()-1,1,f.repeat));
     CHECK(!fixed_choice_construct(f.child.data(),f.child.size(),3,f.repeat));}
    std::puts("fixed choice: resource lifecycle, all ten targets, settings, original input and missing leaves checked");
}
