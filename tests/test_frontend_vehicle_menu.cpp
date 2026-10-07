#include "platform/frontend_vehicle_menu.hpp"
#include <cstdio>
#include <cstdlib>
#include <vector>
using namespace outrun::platform;
using outrun::driving::Bytes;
static unsigned checks;
#define CHECK(x) do{++checks;if(!(x)){std::fprintf(stderr,"vehicle menu line %d: %s\n",__LINE__,#x);std::exit(1);}}while(0)
struct Fixture {
    FrontendVehicleMenu menu;
    std::array<std::uint8_t,0xe00> owner{};
    Bytes b{menu.object.data(),menu.object.size()},root{owner.data(),owner.size()};
    FrontendSprites sprites;FrontendUiResources ui{sprites};
    outrun::driving::PcUiNotifyGlobals globals;
    PcLicense profile{};FrontendInputSnapshot input;unsigned repeat{},result{},fail_pc{};
    std::vector<std::array<unsigned,4>> calls;
    FrontendVehicleMenuServices s{ui,root,globals,profile,input,repeat};
    Fixture(){
        CHECK(sprites.bind_timing(0x44,std::vector<FrontendSpriteTiming>(224,{400,60})));
        CHECK(sprites.bind_timing(0x2c,std::vector<FrontendSpriteTiming>(64,{52,60})));
        CHECK(ui.commands(0x442ac0,root.sub(0x51c,owner.size()-0x51c),nullptr,0,globals,result));
        ui.effect_user=this;ui.effect_4249f0=[](void* p,unsigned id){auto& f=*static_cast<Fixture*>(p);f.calls.push_back({0x4249f0,id,0,0});return f.fail_pc!=0x4249f0;};
        s.user=this;s.external=[](void* p,unsigned pc,Bytes,const unsigned* a,std::size_t n){auto& f=*static_cast<Fixture*>(p);
            std::array<unsigned,4> row{pc,0,0,0};for(std::size_t i=0;i<n&&i<3;++i)row[i+1]=a[i];f.calls.push_back(row);return pc!=f.fail_pc;};
        CHECK(frontend_vehicle_menu_construct_4c8de0(menu,repeat));
    }
    bool tick(unsigned keys=0,unsigned held=0,bool colour=false){input.feature_mask=keys;input.device_held=held;s.colour_held_4536c0=colour;s.timer+=1;return frontend_vehicle_menu_control_4c9290(menu,s,result);}
    const FrontendSprite& image(unsigned offset){const auto* p=sprites.get(b.u32(offset+8));CHECK(p);return *p;}
};
int main(){
    for(unsigned unlocked=0;unlocked<2;++unlocked)for(unsigned index=0;index<30;++index)for(unsigned colour=0;colour<8;++colour){
        Fixture f;f.profile.fill(unlocked?255:0);f.menu.cursor_84b0e8=std::int8_t(index%15);f.menu.variant_84b0e9=std::int8_t(index/15);
        f.menu.colours_68eca8[index]=std::int8_t(colour);
        CHECK(frontend_vehicle_menu_init_4c9010(f.menu,f.s));CHECK(f.menu.initialized_84b0ea);
        const bool available=vehicle_unlocked_4c8f90(f.profile,index);
        if(available)CHECK(vehicle_colour_unlocked_4c8fc0(f.profile,index,unsigned(f.menu.colours_68eca8[index])));
        else CHECK(f.menu.colours_68eca8[index]==int(colour));
        CHECK(f.tick());CHECK(f.image(0xcc).token==0x4400c0+index/15);CHECK(f.image(0xcc).first==20*(index%15+1));
        if(!available){CHECK(f.image(0x15d4).token==0x440047);CHECK(f.image(0x15d4).first==5);}
        CHECK(f.tick(4));CHECK(f.result==(available?4u:0u));
        if(available){CHECK(f.b.u32(4)==0x10);const auto n=f.calls.size();
            CHECK((f.calls[n-4]==std::array<unsigned,4>{0x48b130,VehicleMenuModels[index],0,0}));
            CHECK((f.calls[n-3]==std::array<unsigned,4>{0x48b190,index/15,0,0}));
            CHECK((f.calls[n-2]==std::array<unsigned,4>{0x48b150,unsigned(f.menu.colours_68eca8[index]+1),0,0}));
        }else CHECK(f.calls.back()[0]==0x4249f0&&f.calls.back()[1]==3);
        CHECK(f.tick(8)&&f.result==2);
    }
    {Fixture f;f.profile.fill(255);CHECK(frontend_vehicle_menu_init_4c9010(f.menu,f.s));
        for(unsigned cycle=0;cycle<3;++cycle){
            for(unsigned i=0;i<18;++i)CHECK(f.tick(0x2000));CHECK(f.menu.cursor_84b0e8==14);
            CHECK(f.tick(0,0x2000));CHECK(f.menu.variant_84b0e9==int((cycle+1)%2)&&f.menu.cursor_84b0e8==14);
            const auto index=unsigned(f.menu.variant_84b0e9*15+14);const auto old=f.menu.colours_68eca8[index];
            CHECK(f.tick(0,0,true));const auto next=f.menu.colours_68eca8[index];CHECK(next!=old);
            for(unsigned j=0;j<20;++j)CHECK(f.tick(0,0,true)&&f.menu.colours_68eca8[index]==next);
            CHECK(f.tick());CHECK(f.tick(0,0,true));CHECK(f.menu.colours_68eca8[index]!=next);
            for(unsigned i=0;i<18;++i)CHECK(f.tick(0x1000));CHECK(f.menu.cursor_84b0e8==0);
            CHECK(frontend_vehicle_menu_suspend_4c8d90(f.menu,f.s));CHECK(!f.menu.initialized_84b0ea);
            CHECK(frontend_vehicle_menu_init_4c9010(f.menu,f.s));
        }
    }
    {Fixture f;CHECK(frontend_vehicle_menu_init_4c9010(f.menu,f.s));f.b.put8(0x15d0,1);CHECK(f.tick(8));CHECK(!f.b.u8(0x15d0));CHECK(f.result==0);
        f.b.put8(0x15d0,1);CHECK(frontend_vehicle_menu_display_4c8d70(f.menu,f.s));CHECK(f.calls.back()[0]==0x48c5f0);}
    for(unsigned pc:{0x48c170u,0x48c290u}){Fixture f;f.fail_pc=pc;CHECK(!frontend_vehicle_menu_init_4c9010(f.menu,f.s));CHECK(f.menu.fault==pc&&!f.menu.initialized_84b0ea);}
    for(unsigned pc:{0x48c3f0u,0x4c50d0u,0x4c50f0u,0x4c5100u,0x48b130u,0x48b190u,0x48b150u,0x4249f0u}){
        Fixture f;CHECK(frontend_vehicle_menu_init_4c9010(f.menu,f.s));f.fail_pc=pc;CHECK(!f.tick(4));CHECK(f.menu.fault==pc&&f.result==0);
        f.fail_pc=0;CHECK(!f.tick()&&f.menu.fault==pc);
    }
    {Fixture f;f.s.external=nullptr;CHECK(!frontend_vehicle_menu_init_4c9010(f.menu,f.s)&&f.menu.fault==0x48c170);}
    std::printf("vehicle menu: %u checks; original sprites/input, all choices/colours, retained navigation and failures\n",checks);
}
