#include "frontend_missions.hpp"
#include "frontend_mission_tables.hpp"
#include <cstring>
namespace outrun::platform {
using driving::Bytes;
namespace {
using namespace mission_tables;
bool fail(FrontendMissionServices& s,unsigned pc){s.missing=pc;return false;}
Bytes object(FrontendMissionState& c){return {c.object.data(),c.object.size()};}
bool location(FrontendMissionServices& s,unsigned& group,unsigned& category){
    group=(s.root.u32(0x20c)>>8)&7;
    if(group>=4||s.categories.selection_84b7f4>=counts[group])return fail(s,0x4e9360);
    category=offsets[group]+s.categories.selection_84b7f4;return true;
}
bool resource(FrontendMissionState& c,FrontendMissionServices& s,unsigned off,unsigned pc,const unsigned* a=nullptr,unsigned n=0){
    unsigned r{};
    if(!s.ui.call(pc,object(c).sub(off,0xa0),a,n,r)||s.ui.missing_pc)
        return fail(s,s.ui.missing_pc?s.ui.missing_pc:pc);
    return true;
}
unsigned bits(float f){unsigned r;std::memcpy(&r,&f,4);return r;}
bool configure(FrontendMissionState& c,FrontendMissionServices& s,unsigned off,Descriptor d,unsigned layer,Point p={320,240}){
    const unsigned a[]{d.token,d.first,d.last,layer,0,bits(p.x-320),bits(p.y-240),0x3f800000,0x3f800000,0x3f800000,0};
    return resource(c,s,off,0x465250)&&resource(c,s,off,0x465860,a,11)&&resource(c,s,off,0x465970);
}
bool selection(FrontendMissionState& c,FrontendMissionServices& s){ // 4E8F80
    unsigned group{},category{};if(!location(s,group,category))return false;
    const auto b=object(c);const bool detail=b.u8(0x34)!=0;const auto cursor=b.u32(0x38);
    if(detail&&cursor>=5)return fail(s,0x4e8f80);
    if(!resource(c,s,0x21c,0x465250)||!configure(c,s,0x17c,{0x4400dc,0,52},detail?5:4,
        detail?race_positions[cursor]:highlights[group][s.categories.selection_84b7f4]))return false;
    return !detail||configure(c,s,0x21c,selected[cursor],4);
}
bool sound(FrontendMissionServices& s){
    return (s.ui.effect_4249f0&&s.ui.effect_4249f0(s.ui.effect_user,1))||fail(s,0x4249f0);
}
bool count(FrontendMissionServices& s,unsigned category,unsigned& n){
    if(!s.progress.categories_ready||category>=40)return fail(s,0x495930);
    n=s.progress.category_counts[category];return n<=5||fail(s,0x495930);
}
bool type(FrontendMissionServices& s,unsigned category,unsigned race,unsigned& value){
    return (s.races&&s.assignment&&race_menu_type_4958c0(*s.races,*s.assignment,category,race,value)&&value<7)||fail(s,0x4958c0);
}
}
bool frontend_mission_unlocked_4e90e0(FrontendMissionServices& s,unsigned node,bool& unlocked){
    unsigned group{},category{};if(!location(s,group,category)||node>=mission_tables::counts[group])return fail(s,0x4e90e0);
    unlocked=node==0;if(unlocked)return true;
    for(int neighbor:mission_tables::neighbors[group][node])if(neighbor>=0){int grade{};
        if(!frontend_category_grade_4e81d0(s.profile,s.progress,mission_tables::offsets[group]+unsigned(neighbor),grade))return fail(s,0x4e81d0);
        if(grade!=7&&grade>=4){unlocked=true;break;}}
    return true;
}
bool frontend_missions_construct_4e9160(FrontendMissionState& c,unsigned& repeat){
    c.fault=0;c.constructed=false;c.glyphs.clear();
    if(!title_base_construct_48f480(c.object.data(),c.object.size(),repeat))return false;
    for(unsigned off=0x3c;off<0x3ffc;off+=0xa0)
        if(!title_ui_resource_construct_465160(c.object.data()+off,c.object.size()-off))return false;
    if(!title_widget_construct_48e590(c.object.data()+0x3ffc,PcTextWidgetBytes,repeat))return false;
    object(c).put32(0,0x5ce71c);object(c).put32(8,0x3b);c.constructed=true;return true;
}
bool frontend_missions_init_4e92d0(FrontendMissionState& c,FrontendMissionServices& s){
    object(c).put8(0x34,0);if(!selection(c,s))return false;
    auto w=object(c).sub(0x3ffc,PcTextWidgetBytes);
    w.put32(0x450,9);w.put32(0x454,5);w.put32(0x474,0xff3f474a);w.put32(0x480,1);
    w.putf(0x34,300);w.putf(0x38,330);
    const unsigned rect[]{300,330,570,370};
    if(!frontend_text_setter(0x48eee0,w,rect,4))return fail(s,0x48eee0);
    w.put8(0x470,1);return true;
}
bool frontend_missions_control_4e9360(FrontendMissionState& c,FrontendMissionServices& s,unsigned& result){
    result=0;unsigned group{},category{},action{};if(!location(s,group,category))return false;
    if(!frontend_input_action_48f5f0(c.object.data(),c.object.size(),s.input,1,s.repeat,&s,
        [](void* p,unsigned key,int arg){auto& v=*static_cast<FrontendMissionServices*>(p);return v.ui.input_feedback(v.root,key,arg);},action))
        return fail(s,s.ui.missing_pc?s.ui.missing_pc:0x48f5f0);
    auto b=object(c);const bool detail=b.u8(0x34)!=0;const auto cursor=b.u32(0x38);
    if(detail&&cursor>=5)return fail(s,0x4e9360);
    if(action==0){
        if(!detail){b.put8(0x34,1);b.put32(0x38,0);return selection(c,s);}
        s.root.put32(0x20c,(s.root.u32(0x20c)&~0x1ff000u)|(category<<12)|(cursor<<18));
        b.put32(4,0xa);result=1;
    }else if(action==1){
        if(detail){b.put8(0x34,0);b.put32(0x38,0);return selection(c,s);}
        s.root.put32(0x20c,s.root.u32(0x20c)&0xffe00fffu);result=2;
    }else if(action>=2&&action<=5){
        if(detail){
            if(action!=3&&action!=5)return true;
            unsigned n{};if(!count(s,category,n))return false;
            if(action==3){if(!cursor)return true;b.put32(0x38,cursor-1);}
            else {const auto grade=s.profile[0x1dd+category*5+cursor];
                if(!n||cursor>=n-1||grade==7||grade<4)return true;
                b.put32(0x38,cursor+1);}
        }else{
            constexpr unsigned columns[]{0,3,1,2};
            const int next=mission_tables::neighbors[group][s.categories.selection_84b7f4][columns[action-2]];
            if(next<0)return true;
            bool available{};
            if(!frontend_mission_unlocked_4e90e0(s,unsigned(next),available))return false;
            if(!available)return true;
            s.categories.selection_84b7f4=unsigned(next);
        }
        if(!selection(c,s)||!sound(s))return false;
    }
    return true;
}
bool frontend_missions_display_4e95a0(FrontendMissionState& c,FrontendMissionServices& s){
    using namespace mission_tables;c.glyphs.clear();unsigned group{},category{},n{};
    if(!location(s,group,category)||!count(s,category,n))return false;
    if(!configure(c,s,0x3c,backgrounds[group],1)||!configure(c,s,0xdc,overlays[group],3))return false;
    for(unsigned i=0;i<40;++i)if(!resource(c,s,0xc1c+i*0xa0,0x465250)||!resource(c,s,0x251c+i*0xa0,0x465250))return false;
    for(unsigned i=0;i<counts[group];++i){bool available{};if(!frontend_mission_unlocked_4e90e0(s,i,available))return false;
        if(!available)continue;
        const auto p=positions[group][i];
        if(!configure(c,s,0xc1c+i*0xa0,nodes[group],2,p))return false;
        int grade{};if(!frontend_category_grade_4e81d0(s.profile,s.progress,offsets[group]+i,grade))return fail(s,0x4e81d0);
        if(grade!=7&&!configure(c,s,0x251c+i*0xa0,grades[grade],3,p))return false;
    }
    for(unsigned i=0;i<5;++i)for(unsigned off:{0x2bcu,0x5dcu,0x8fcu})if(!resource(c,s,off+i*0xa0,0x465250))return false;
    for(unsigned i=0;i<n;++i){const auto grade=s.profile[0x1dd+category*5+i];
        const auto previous=i?s.profile[0x1dc+category*5+i]:7;
        if(grade>7||previous>7)return fail(s,0x4e95a0);
        const bool locked=i&&(previous==7||previous<4);auto d=buttons[group];if(locked)d.first=d.last=5;
        if(!configure(c,s,0x2bc+i*0xa0,d,2,race_positions[i]))return false;
        unsigned t{};if(!type(s,category,i+1,t))return false;
        if(!locked&&(!configure(c,s,0x5dc+i*0xa0,types[t],3,race_positions[i])||
            !configure(c,s,0x8fc+i*0xa0,grades[grade],4,grade_positions[i])))return false;
    }
    if(!configure(c,s,0x3ebc,{0x4400a7,category+30,category+30},2,{237,388})||
       !configure(c,s,0x3e1c,panels[group],2)||!configure(c,s,0x3f5c,footers[group],1))return false;
    unsigned t{};if(!type(s,category,1,t))return false;
    const auto* caption=s.text?s.text->get(labels[t]):nullptr;
    if(!caption)return fail(s,0x465eb0);
    auto w=object(c).sub(0x3ffc,PcTextWidgetBytes);
    if(!frontend_text_set_48f280(w,*caption,w.u32(0x450),w.u32(0x474)))return fail(s,0x48ee80);
    return (s.fonts&&frontend_text_display_48f3c0(w,*s.fonts,c.lines,c.glyphs))||fail(s,0x48f3c0);
}
bool frontend_missions_suspend_4e9bb0(FrontendMissionState& c,FrontendMissionServices& s){
    c.glyphs.clear();if(!c.constructed)return true;
    // Preserve PC release order too: the shared sprite allocator remembers
    // the last freed slot in each layer, affecting subsequent draw order.
    for(unsigned off:{0x3cu,0xdcu})if(!resource(c,s,off,0x465250))return false;
    for(unsigned i=0;i<5;++i)for(unsigned off:{0x2bcu,0x5dcu,0x8fcu})
        if(!resource(c,s,off+i*0xa0,0x465250))return false;
    for(unsigned i=0;i<40;++i)for(unsigned off:{0xc1cu,0x251cu})
        if(!resource(c,s,off+i*0xa0,0x465250))return false;
    for(unsigned off:{0x17cu,0x21cu,0x3e1cu,0x3ebcu,0x3f5cu})
        if(!resource(c,s,off,0x465250))return false;
    return true;
}
}
