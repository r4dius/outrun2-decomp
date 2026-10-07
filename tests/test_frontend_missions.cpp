#include "platform/frontend_missions.hpp"
#include "platform/frontend_mission_tables.hpp"
#include <cstdio>
#include <cstdlib>
using namespace outrun::platform;
using outrun::driving::Bytes;
#define CHECK(x) do {if(!(x)){std::fprintf(stderr,"line %d: %s\n",__LINE__,#x);std::exit(1);}}while(0)
int main(){
    FrontendMissionState c;FrontendCategoryState categories;FrontendSprites pool;FrontendUiResources ui{pool};
    CHECK(pool.bind_timing(0x44,std::vector<FrontendSpriteTiming>(224,{240,60})));
    ui.effect_4249f0=[](void*,unsigned id){return id==1;};
    std::array<std::uint8_t,0xe00> root{};Bytes r(root.data(),root.size());
    PcLicense profile;frontend_license_reset_4471a0(profile,1);
    LicenseProgressTables tables;tables.categories_ready=true;tables.category_counts.fill(5);
    FrontendInputSnapshot input;unsigned repeat{},result{};
    FrontendMissionServices s{{ui,r,profile,tables,input,repeat},categories};
    CHECK(frontend_missions_construct_4e9160(c,repeat));auto b=Bytes(c.object.data(),c.object.size());
    const auto tick=[&](unsigned mask){input.feature_mask=mask;CHECK(frontend_missions_control_4e9360(c,s,result));};
    for(unsigned group=0;group<4;++group){
        r.put32(0x20c,(0xabc008afu&~0x700u)|(group<<8));categories.selection_84b7f4=0;
        for(unsigned i=0;i<200;++i)profile[0x1dd+i]=7;
        CHECK(frontend_missions_init_4e92d0(c,s));
        for(unsigned node=0;node<mission_tables::counts[group];++node){bool open{};
            CHECK(frontend_mission_unlocked_4e90e0(s,node,open)&&open==(node==0));}
        tick(0x2000);tick(0);CHECK(categories.selection_84b7f4==0); // locked neighbor
        tick(4);tick(0);CHECK(b.u8(0x34)==1&&b.u32(0x38)==0);
        tick(0x2000);tick(0);CHECK(b.u32(0x38)==0); // next race needs current A
        const auto category=mission_tables::offsets[group];profile[0x1dd+category*5]=4;
        tick(0x2000);tick(0);CHECK(b.u32(0x38)==1);
        tick(0x1000);tick(0);CHECK(b.u32(0x38)==0);
        tick(8);tick(0);CHECK(b.u8(0x34)==0&&result==0);
        // All passing categories unlock the original graph; no replacement map bounds.
        for(unsigned i=0;i<200;++i)profile[0x1dd+i]=4;
        tick(0x2000);tick(0);CHECK(categories.selection_84b7f4==1);
        tick(4);tick(0);for(unsigned i=0;i<4;++i){tick(0x2000);tick(0);}CHECK(b.u32(0x38)==4);
        const auto before=r.u32(0x20c);tick(4);CHECK(result==1&&b.u32(4)==10);
        CHECK(r.u32(0x20c)==((before&~0x1ff000u)|((category+1)<<12)|(4<<18)));tick(0);
        tick(8);tick(0);tick(8);CHECK(result==2&&r.u32(0x20c)==(before&0xffe00fffu));tick(0);
        CHECK(frontend_missions_suspend_4e9bb0(c,s));for(unsigned layer=0;layer<21;++layer)CHECK(pool.used(layer)==0);
    }
    // Exact one-based matching, not the clamping helper used by a different PC routine.
    RaceAssignmentPack assignment;assignment.menu_count=40;assignment.menu_race_keys[0]=9;
    RaceAssetPack races;races.race_count=2;races.bytes.resize(2*0x44);Bytes data(races.bytes.data(),races.bytes.size());
    data.put32(0,9);data.put32(4,2);data.put32(0x20,6);
    data.put32(0x44,9);data.put32(0x48,1);data.put32(0x64,3);unsigned type=99;
    CHECK(race_menu_type_4958c0(races,assignment,0,1,type)&&type==3);
    CHECK(race_menu_type_4958c0(races,assignment,0,2,type)&&type==6);
    CHECK(!race_menu_type_4958c0(races,assignment,0,0,type));CHECK(!race_menu_type_4958c0(races,assignment,0,3,type));
    races.bytes.pop_back();CHECK(!race_menu_type_4958c0(races,assignment,0,1,type));
    r.put32(0x20c,0);categories.selection_84b7f4=0;CHECK(frontend_missions_init_4e92d0(c,s));
    CHECK(!frontend_missions_display_4e95a0(c,s)&&s.missing==0x4958c0); // not fake resource readiness
    CHECK(frontend_missions_suspend_4e9bb0(c,s));
    std::puts("mission map/race navigation, original unlock gates, selected flags, lookup and teardown pass");
}
