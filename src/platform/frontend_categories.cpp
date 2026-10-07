#include "frontend_categories.hpp"
#include <cstring>
namespace outrun::platform {
using driving::Bytes;
namespace {
bool resource(FrontendCategoryState& c,FrontendCategoryServices& s,unsigned off,unsigned pc,const unsigned* args=nullptr,unsigned n=0){
    unsigned result{};if(!s.ui.call(pc,Bytes(c.object.data()+off,0xa0),args,n,result)||s.ui.missing_pc){
        s.missing=s.ui.missing_pc?s.ui.missing_pc:pc;return false;}return true;
}
unsigned bits(float v){unsigned r;std::memcpy(&r,&v,4);return r;}
bool configure(FrontendCategoryState& c,FrontendCategoryServices& s,unsigned off,unsigned token,unsigned first,unsigned last,unsigned layer,unsigned mode,float x=0,float y=0){
    const unsigned args[]{token,first,last,layer,mode,bits(x),bits(y),0x3f800000,0x3f800000,0x3f800000,0};
    return resource(c,s,off,0x465250)&&resource(c,s,off,0x465860,args,11)&&resource(c,s,off,0x465970);
}
bool selection(FrontendCategoryState& c,FrontendCategoryServices& s){ // 4E8580, table5CE3A0
    if(c.selected_84b7f0>=7){s.missing=0x4e8580;return false;}
    const unsigned first=c.selected_84b7f0*30;
    return configure(c,s,0x5d4,0x4400b6,first,first+29,4,0);
}
bool unlocked(FrontendCategoryServices& s,unsigned group,bool& result){
    if(frontend_category_unlocked_4e8410(s.profile,s.progress,group,result))return true;
    s.missing=0x4e8410;return false;
}
bool sound(FrontendCategoryServices& s,unsigned id){
    if(s.ui.effect_4249f0&&s.ui.effect_4249f0(s.ui.effect_user,id))return true;
    s.missing=0x4249f0;return false;
}
bool text(FrontendCategoryState& c,FrontendCategoryServices& s,std::string_view value,unsigned color,unsigned flags,short x,short y){
    // Original 42CA60(9), 42CC60(1,1), 42CCB0(6), 42C360, 42CC00, 42CDD0("%s").
    if(!s.fonts||!frontend_text_draw(s.fonts->fonts[9],{1,1,color,6,flags},
        {x,x,y},value,c.glyphs)){s.missing=0x42cdd0;return false;}return true;
}
bool localized(FrontendCategoryState& c,FrontendCategoryServices& s,unsigned id){
    const auto* value=s.text?s.text->get(id):nullptr;
    if(!value){s.missing=0x465eb0;return false;}
    return text(c,s,*value,0xff3f474a,1,65,210);
}
}
bool frontend_categories_construct_4e8130(FrontendCategoryState& c,unsigned& repeat){
    c.fault=0;c.glyphs.clear();c.constructed=false;
    if(!title_base_construct_48f480(c.object.data(),c.object.size(),repeat))return false;
    for(unsigned off=0x34;off<0x674;off+=0xa0)
        if(!title_ui_resource_construct_465160(c.object.data()+off,c.object.size()-off))return false;
    Bytes b(c.object.data(),c.object.size());b.put32(0,0x5ce4c0);b.put32(8,0x3a);c.constructed=true;return true;
}
bool frontend_categories_init_4e8760(FrontendCategoryState& c,FrontendCategoryServices& s){
    if(!selection(c,s))return false;
    c.selection_84b7f4=0;c.selection_84b7f8=0;c.query_83639c=0;return true;
}
bool frontend_categories_control_4e8780(FrontendCategoryState& c,FrontendCategoryServices& s,unsigned& result){
    result=0;unsigned action{};
    if(c.selected_84b7f0>=7){s.missing=0x4e8780;return false;}
    if(!frontend_input_action_48f5f0(c.object.data(),c.object.size(),s.input,0,s.repeat,&s,
        [](void* p,unsigned key,int arg){auto& svc=*static_cast<FrontendCategoryServices*>(p);return svc.ui.input_feedback(svc.root,key,arg);},action)){
        s.missing=s.ui.missing_pc?s.ui.missing_pc:0x48f5f0;return false;}
    if(action==0){bool ready{};if(!unlocked(s,c.selected_84b7f0,ready)||!sound(s,ready?64:3))return false;
        if(!ready)return true;
        s.root.put32(0x20c,(s.root.u32(0x20c)&~0x700u)|(c.selected_84b7f0<<8));
        Bytes(c.object.data(),c.object.size()).put32(4,c.selected_84b7f0<4?0x3b:0x3c);result=1;
    }else if(action==1){s.root.put32(0x20c,s.root.u32(0x20c)&0xffe008ffu);result=2;
    }else if(action>=2&&action<=5){
        // 5CE400: action2=up, action3=left, action4=down, action5=right.
        constexpr int neighbors[7][4]={{-1,4,1,-1},{-1,5,2,0},{-1,6,3,1},{-1,-1,-1,2},{0,-1,5,-1},{1,-1,6,4},{2,-1,-1,5}};
        constexpr unsigned columns[]{0,3,1,2};
        const int next=neighbors[c.selected_84b7f0][columns[action-2]];
        if(next>=0){c.selected_84b7f0=unsigned(next);if(!sound(s,1))return false;}
        if(!selection(c,s))return false; // PC refreshes even for a blocked edge.
    }
    return true;
}
bool frontend_categories_suspend_4e8a30(FrontendCategoryState& c,FrontendCategoryServices& s){
    c.glyphs.clear();if(!c.constructed)return true;
    if(!resource(c,s,0x5d4,0x465250))return false;
    for(unsigned off=0x34;off<0x494;off+=0xa0)if(!resource(c,s,off,0x465250))return false;
    return resource(c,s,0x494,0x465250)&&resource(c,s,0x534,0x465250);
}
bool frontend_categories_display_4e8c60(FrontendCategoryState& c,FrontendCategoryServices& s){
    c.glyphs.clear();if(c.selected_84b7f0>=7){s.missing=0x4e8c60;return false;}
    constexpr float xs[]{87,149,212,274,87,149,212};
    for(unsigned i=0;i<7;++i){bool available{};if(!unlocked(s,i,available))return false;
        const unsigned frame=i*2+unsigned(available);
        if(!configure(c,s,0x34+0xa0*i,0x4400b7,frame,frame,available?2:1,0,xs[i]-320,(i<4?303.f:365.f)-240))return false;
    }
    unsigned picture{};if(!frontend_category_picture_4e8b20(s.profile,s.progress,c.selected_84b7f0,picture)){s.missing=0x4e8b20;return false;}
    bool available{};if(!unlocked(s,c.selected_84b7f0,available))return false;
    if(c.selected_84b7f0<4){if(!configure(c,s,0x534,0x4400b5,picture,picture,1,0))return false;}
    else {constexpr unsigned frames[]{0,6,2};const auto frame=frames[c.selected_84b7f0-4]+unsigned(!available);
        if(!configure(c,s,0x534,0x4400cb,frame,frame,1,0))return false;}
    if(available){int grade{};if(!frontend_group_grade_4e82a0(s.profile,s.progress,c.selected_84b7f0,grade)){s.missing=0x4e82a0;return false;}
        constexpr unsigned normal[]{0x382,0x383,0x384,0x385,0x38d,0x38e,0x38f};
        constexpr unsigned complete[]{0x386,0x387,0x388,0x389,0x391,0x392,0x393};
        constexpr const char* ranks[]{"E","D","C","B","A","AA","AAA",""};
        if(!localized(c,s,grade==6?complete[c.selected_84b7f0]:normal[c.selected_84b7f0])||
           !text(c,s,ranks[grade],0xff4d5559,4,520,363))return false;
    }else{constexpr int locked[]{-1,0x38a,0x38b,0x38c,-1,0x395,0x396};
        if(locked[c.selected_84b7f0]>0&&!localized(c,s,unsigned(locked[c.selected_84b7f0])))return false;}
    const unsigned frame=available?c.selected_84b7f0:c.selected_84b7f0<5?c.selected_84b7f0+8:12;
    return configure(c,s,0x494,0x4400b4,frame,frame,1,0);
}
}
