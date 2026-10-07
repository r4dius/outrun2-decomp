#include "platform/frontend_categories.hpp"
#include <cstdio>
#include <cstdlib>
using namespace outrun::platform;
using outrun::driving::Bytes;
#define CHECK(x) do {if(!(x)){std::fprintf(stderr,"line %d: %s\n",__LINE__,#x);std::exit(1);}}while(0)
int main(){
    FrontendCategoryState c;FrontendSprites pool;FrontendUiResources ui{pool};
    CHECK(pool.bind_timing(0x44,std::vector<FrontendSpriteTiming>(224,{240,60})));
    std::array<std::uint8_t,0xe00> root{};Bytes r(root.data(),root.size());
    PcLicense profile;frontend_license_reset_4471a0(profile,1);
    LicenseProgressTables tables;tables.categories_ready=true;tables.category_counts.fill(5);
    FrontendInputSnapshot input;unsigned repeat{},result{};std::vector<unsigned> effects;
    ui.effect_user=&effects;ui.effect_4249f0=[](void* p,unsigned id){static_cast<std::vector<unsigned>*>(p)->push_back(id);return true;};
    FrontendCategoryServices s{ui,r,profile,tables,input,repeat};
    CHECK(frontend_categories_construct_4e8130(c,repeat));
    c.selection_84b7f4=6;c.selection_84b7f8=7;c.query_83639c=8;
    CHECK(frontend_categories_init_4e8760(c,s));CHECK(c.selection_84b7f4==0&&c.selection_84b7f8==0&&c.query_83639c==0);
    const auto tick=[&](unsigned mask){input.feature_mask=mask;CHECK(frontend_categories_control_4e8780(c,s,result));};
    tick(0x2000);tick(0);CHECK(c.selected_84b7f0==1);
    tick(4);CHECK(result==0&&effects.back()==3);tick(0);
    for(unsigned group=0;group<7;++group){bool open{};CHECK(frontend_category_unlocked_4e8410(profile,tables,group,open));CHECK(open==(group==0||group==4));}
    tick(8);CHECK(result==2);tick(0);
    for(unsigned grade=0;grade<8;++grade){
        for(unsigned i=0;i<200;++i)profile[0x1dd+i]=std::uint8_t(grade);
        tables.championship_present.fill(true);for(auto& bank:tables.championship_types)for(unsigned i=0;i<15;++i)bank[i]=i;
        for(unsigned i=0;i<132;++i)profile[0x36e + i]=std::uint8_t(grade|(grade<<4));
        for(unsigned group=0;group<7;++group){int got{};CHECK(frontend_group_grade_4e82a0(profile,tables,group,got)&&got==int(grade));
            c.selected_84b7f0=group;r.put32(0x20c,0xffffffff);tick(4);
            const bool open=group==0||group==4||(grade>=4&&grade!=7);
            CHECK(result==unsigned(open));
            if(open){CHECK(Bytes(c.object.data(),c.object.size()).u32(4)==(group<4?0x3b:0x3c));CHECK(r.u32(0x20c)==(0xfffff8ff|(group<<8)));}
            tick(0);tick(8);CHECK(result==2&&r.u32(0x20c)==0xffe008ff);tick(0);
        }
    }
    CHECK(frontend_categories_suspend_4e8a30(c,s));for(unsigned layer=0;layer<21;++layer)CHECK(pool.used(layer)==0);
    c.selected_84b7f0=0;CHECK(frontend_categories_construct_4e8130(c,repeat));CHECK(frontend_categories_init_4e8760(c,s));
    ui.effect_4249f0=nullptr;input.feature_mask=0x2000;
    CHECK(!frontend_categories_control_4e8780(c,s,result)&&s.missing==0x4249f0);
    std::puts("category unlocks, selected mode bits, navigation, rejection sound and resource lifecycle pass");
}
