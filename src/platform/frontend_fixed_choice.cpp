#include "frontend_fixed_choice.hpp"
#include "frontend_carousel.hpp"
#include "frontend_text.hpp"
#include <cmath>
namespace outrun::platform {
using driving::Bytes;
namespace {
bool valid(Bytes b,unsigned key){return b.size()>=0x154&&(key==1||key==2);}
bool resource(Bytes b,FixedChoiceServices& s,unsigned pc,const unsigned* args=nullptr,std::size_t n=0,unsigned* output=nullptr){
    unsigned value{};const bool ok=s.ui.call(pc,b.sub(0x38,0xa0),args,n,value)&&!s.ui.missing_pc;
    if(output)*output=value;
    if(!ok)s.missing=s.ui.missing_pc?s.ui.missing_pc:pc;
    return ok;
}
bool external(FixedChoiceServices& s,unsigned pc){
    if(s.external&&s.external(s.user,pc))return true;
    s.missing=pc;return false;
}
bool commands(FixedChoiceServices& s,unsigned pc,const unsigned* args,std::size_t n){
    unsigned value{};const bool ok=s.ui.commands(pc,s.root.sub(0x51c,s.root.size()-0x51c),args,n,s.globals,value);
    if(!ok)s.missing=s.ui.missing_pc?s.ui.missing_pc:pc;
    return ok;
}
FrontendCarouselTable table(unsigned key){
    static const auto records=[](){std::array<std::array<FrontendCarouselDescriptor,10>,2> out{};
        for(unsigned k=0;k<2;++k)for(unsigned i=0;i<10;++i)out[k][i]={k?0x440017u:0x4400ddu,20*(i/2+(i&1)),20*(i/2+1)};
        return out;}();
    return {records[key-1].data(),10,key==1?0x5c9530u:0x5c95e8u};
}
}
bool fixed_choice_construct(std::uint8_t* data,std::size_t size,unsigned key,unsigned& repeat){
    if(!data||!valid(Bytes(data,size),key))return false;
    if(!title_base_construct_48f480(data,size,repeat)||!title_ui_resource_construct_465160(data+0x38,size-0x38))return false;
    Bytes b(data,size);b.put32(0,key==1?0x5c95cc:0x5c966c);b.put32(8,key);return true;
}
bool fixed_choice_init(Bytes b,unsigned key,FixedChoiceServices& s){
    if(!valid(b,key)||!std::isfinite(s.timer))return false;
    b.put8(0x14c,0);if(key==1){b.put32(0x150,0);s.root.put32(0x20c,s.root.u32(0x20c)&~0xe0u);}
    auto w=b.sub(0x34,0x118);FrontendCarouselServices carousel{s.ui,s.timer};
    if(!frontend_carousel_init_51b7b0(w,table(key),0,10,carousel)){s.missing=carousel.missing;return false;}
    const auto flags=s.root.u32(0x20c);
    if(key==1){
        if((flags&0x1c)<0x14)frontend_carousel_select_51bdb0(w,int((flags>>2)&7));
        driving::object_store_depth_pair_442f20(s.root,1,1);
        const unsigned visible[]{1,0},labels[]{4,0x295,0x8000,0x297,0x4000,0x29a,8,0x296};
        return commands(s,0x447000,visible,2)&&commands(s,0x440ea0,labels,8);
    }
    const auto mode=flags&0xe0;
    if(mode<0xe0){if(mode<0x60)frontend_carousel_select_51bdb0(w,int((flags>>5)&7));else if(mode==0x80)frontend_carousel_select_51bdb0(w,3);else if(mode==0xc0)frontend_carousel_select_51bdb0(w,4);}
    return external(s,0x4940d0);
}
bool fixed_choice_suspend(Bytes b,unsigned key,FixedChoiceServices& s){
    if(!valid(b,key)||!resource(b,s,0x465250))return false;
    if(key==1)b.put8(0x14c,0);
    return true;
}
bool fixed_choice_tick(std::uint8_t* data,std::size_t size,unsigned key,FixedChoiceServices& s,unsigned& result){
    result=0;if(!data||!valid(Bytes(data,size),key))return false;
    Bytes b(data,size);if(b.u8(0x14c)){result=b.u32(0x150);return true;}
    FrontendCarouselServices carousel{s.ui,s.timer};auto w=b.sub(0x34,0x118);
    if(!frontend_carousel_tick_51be30(w,table(key),carousel)){s.missing=carousel.missing;return false;}
    unsigned action{};
    if(!frontend_input_action_48f5f0(data,size,s.input,1,s.repeat,&s,
        [](void* p,unsigned input,int arg){auto& s=*static_cast<FixedChoiceServices*>(p);return s.ui.input_feedback(s.root,input,arg);},action)){
        s.missing=s.ui.missing_pc?s.ui.missing_pc:0x48f5f0;return false;
    }
    if(action==3||action==5){const bool ok=frontend_carousel_move(w,table(key),action==5,carousel);if(!ok)s.missing=carousel.missing;return ok;}
    if(action!=0&&action!=1)return true;
    auto flags=s.root.u32(0x20c);
    if(action==1){
        if(key==1){const unsigned visible[]{0,1};if(!commands(s,0x447000,visible,2))return false;b.put32(4,0);flags&=~0x1cu;}
        else flags&=~0xe0u; // key 2 retains +4, not a fabricated 0x53 assignment
        b.put32(0x150,key==1?3:2);
    }else{
        const auto cursor=b.u32(0xe8);if(cursor>4){s.missing=key==1?0x4c5620:0x4c5a50;return false;}
        if(key==1){
            if(cursor<3)s.root.put32(4,cursor);
            flags=(flags&~0x1cu)|(cursor<<2);s.root.put32(0x20c,flags);
            if((cursor==2||cursor==3)&&(!external(s,0x4165c0)||!external(s,0x4f3cc0)))return false;
            constexpr unsigned targets[]{2,6,0x53,0x31,0x2c};b.put32(4,targets[cursor]);b.put32(0x150,cursor==2?5:1);
        }else{
            constexpr unsigned modes[]{0,0x20,0x40,0x80,0xc0},targets[]{0x3a,0x24,0x24,0x24,0x2b};
            flags=(flags&~0xe0u)|modes[cursor];b.put32(4,targets[cursor]);b.put32(0x150,1);
        }
    }
    s.root.put32(0x20c,flags);
    if(!resource(b,s,0x465250))return false;
    b.put8(0x14c,1);return true;
}
}
namespace outrun::platform {
namespace {
FrontendCarouselTable selector_table(){
    // 5CCC00: 4400D7 frames 0-F, F, F-2A, 2A, 2A-40, 40 (transition / hold per choice).
    static const FrontendCarouselDescriptor records[6]{{0x4400d7,0,0xf},{0x4400d7,0xf,0xf},{0x4400d7,0xf,0x2a},
        {0x4400d7,0x2a,0x2a},{0x4400d7,0x2a,0x40},{0x4400d7,0x40,0x40}};
    return {records,6,0x5ccc00u};
}
bool selector_valid(Bytes b){return b.size()>=PcRankingSelectorBytes;}
// 4D8FF0 / 4D8FD0: release the carousel, +14D = leaving.
bool selector_leave(Bytes b,FixedChoiceServices& s,bool leaving){
    FrontendCarouselServices carousel{s.ui,s.timer};
    if(!frontend_carousel_release_51bc30(b.sub(0x34,0x118),carousel)){s.missing=carousel.missing;return false;}
    b.put8(0x14d,leaving?1:0);return true;
}
}
bool ranking_selector_construct_4d9010(std::uint8_t* data,std::size_t size,unsigned& repeat){
    if(!data||size<PcRankingSelectorBytes)return false;
    if(!title_base_construct_48f480(data,size,repeat)||!title_ui_resource_construct_465160(data+0x38,size-0x38)||
       !title_widget_construct_48e590(data+0x158,size-0x158,repeat))return false;
    Bytes b(data,size);b.put32(0,0x5ccc54);b.put32(8,0x31);b.put8(0x14d,0);return true;
}
bool ranking_selector_init_4d8ee0(Bytes b,FixedChoiceServices& s,RankingSelectorGlobals& g){
    if(!selector_valid(b)||!std::isfinite(s.timer))return false;
    b.put32(4,0x53);b.put8(0x14d,0);b.put8(0x154,0);g.ghost_84b0f6=0;b.put32(0x5e8,0);b.putf(0x5e4,s.timer);   // 4AF500
    // +158: 48E640, 48F2D0 with a 48D570 block (font 9, layer D, colour FF3F474A), 48EE80(""), 48E530(60, 342).
    auto w=b.sub(0x158,PcTextWidgetBytes);frontend_text_init_48e640(w);
    w.put32(0x450,9);w.put32(0x454,0xd);w.put8(0x470,0);w.put32(0x474,0xff3f474a);
    w.put8(0x4e,0);w.put32(0x458,0);w.putf(0x34,60.f);w.putf(0x38,342.f);
    auto c=b.sub(0x34,0x118);FrontendCarouselServices carousel{s.ui,s.timer};
    if(!frontend_carousel_init_51b7b0(c,selector_table(),0,6,carousel)){s.missing=carousel.missing;return false;}
    frontend_carousel_select_51bdb0(c,g.cursor_84b20c);
    return external(s,0x4940d0);
}
bool ranking_selector_control_4d90a0(Bytes b,FixedChoiceServices& s,RankingSelectorGlobals& g,unsigned& result){
    result=0;if(!selector_valid(b))return false;
    if(b.u8(0x14d)){result=b.u32(0x150);return true;}
    auto c=b.sub(0x34,0x118);FrontendCarouselServices carousel{s.ui,s.timer};
    if(!frontend_carousel_tick_51be30(c,selector_table(),carousel)){s.missing=carousel.missing;return false;}
    unsigned action{};
    if(!frontend_input_action_48f5f0(b.data(),b.size(),s.input,1,s.repeat,&s,
        [](void* p,unsigned input,int arg){auto& s=*static_cast<FixedChoiceServices*>(p);return s.ui.input_feedback(s.root,input,arg);},action)){
        s.missing=s.ui.missing_pc?s.ui.missing_pc:0x48f5f0;return false;
    }
    const auto index=b.u32(0xe8);
    switch(action){
    case 0:
        b.put32(0x150,1);
        s.root.put32(0x20c,s.root.u32(0x20c)&~3u);                            // [535EF0(4035F0())+208]
        if(index==2){
            if(g.online_7d68bc){g.cursor_84b20c=2;g.ghost_84b0f6=2;b.put32(4,0x34);return selector_leave(b,s,true);}
            g.notice_63aa6c=2;b.put32(4,0x16);result=4;return true;
        }
        g.ghost_84b0f6=index==1?1:0;g.cursor_84b20c=std::int8_t(index==1?1:0);
        b.put32(4,0x33);return selector_leave(b,s,true);
    case 1:
        b.put32(0x150,2);g.cursor_84b20c=0;g.ghost_84b0f6=0;return selector_leave(b,s,true);
    case 3:case 5:
        if(action==3?index==0:index==2)return true;
        if(!frontend_carousel_move(c,selector_table(),action==5,carousel)){s.missing=carousel.missing;return false;}
        return true;
    default:return true;
    }
}
bool ranking_selector_suspend_4d8fd0(Bytes b,FixedChoiceServices& s){return selector_valid(b)&&selector_leave(b,s,false);}
}
