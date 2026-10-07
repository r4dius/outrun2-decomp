// Network menus of the frontend, keys 6 and 8 (frontend_network_menus.hpp).
#include "frontend_network_menus.hpp"
#include "frontend_carousel.hpp"
#include "driving/pc_common_control.hpp"
#include "driving/service_hole.hpp"
namespace outrun::platform {
using driving::Bytes;
namespace {
bool valid(Bytes b,unsigned key){return ((key==6||key==7)&&b.size()>=PcNetworkMenuBytes)||(key==8&&b.size()>=PcLanMenuBytes);}
FrontendCarouselTable table(unsigned key){
    // 5C9684 (key 6, 4400D5) / 5C9760 (key 8, 4400DF): two choices, transition / hold records.
    static const FrontendCarouselDescriptor online[4]{{0x4400d5,0,0xe},{0x4400d5,0xe,0xe},{0x4400d5,0xe,0x1d},{0x4400d5,0x1d,0x1d}};
    static const FrontendCarouselDescriptor lan[4]{{0x4400df,0,0xe},{0x4400df,0xe,0xe},{0x4400df,0xe,0x29},{0x4400df,0x29,0x29}};
    // 5C96D8 (key 7, 4400D6): three choices.
    static const FrontendCarouselDescriptor online7[6]{{0x4400d6,0,0xe},{0x4400d6,0xe,0xe},{0x4400d6,0xe,0x26},{0x4400d6,0x26,0x26},
        {0x4400d6,0x26,0x40},{0x4400d6,0x40,0x40}};
    if(key==7)return FrontendCarouselTable{online7,6,0x5c96d8u};
    return key==6?FrontendCarouselTable{online,4,0x5c9684u}:FrontendCarouselTable{lan,4,0x5c9760u};
}
// Key 8's +14C resource (465160): the 4400DF frames 0..E (enter) or E..0 (leave).
bool overlay(Bytes b,NetworkMenuServices& s,unsigned pc,const unsigned* args=nullptr,std::size_t n=0,unsigned* out=nullptr){
    unsigned value{};const bool ok=s.ui.call(pc,b.sub(0x14c,0xa0),args,n,value)&&!s.ui.missing_pc;
    if(out)*out=value;
    if(!ok)s.missing=s.ui.missing_pc?s.ui.missing_pc:pc;
    return ok;
}
bool overlay_play(Bytes b,NetworkMenuServices& s,bool leave){
    constexpr unsigned one=0x3f800000u,minus_one=0xbf800000u;
    const unsigned args[11]{0x4400df,leave?0xeu:0u,leave?0u:0xeu,4,3,0,0,one,one,leave?minus_one:one,0};
    return overlay(b,s,0x465860,args,11)&&overlay(b,s,0x465970);
}
// 4C6050: release the carousel, the leaving animation, +1EC = leaving.
bool lan_leave(Bytes b,NetworkMenuServices& s){
    FrontendCarouselServices carousel{s.ui,s.timer};
    if(!frontend_carousel_release_51bc30(b.sub(0x34,0x118),carousel)){s.missing=carousel.missing;return false;}
    if(!overlay(b,s,0x465250)||!overlay_play(b,s,true))return false;
    b.put8(0x1ec,1);return true;
}
bool input(std::uint8_t* data,std::size_t size,NetworkMenuServices& s,unsigned& action){
    if(frontend_input_action_48f5f0(data,size,s.input,1,s.repeat,&s,
        [](void* p,unsigned key,int arg){auto& s=*static_cast<NetworkMenuServices*>(p);return s.ui.input_feedback(s.root,key,arg);},action))return true;
    s.missing=s.ui.missing_pc?s.ui.missing_pc:0x48f5f0;return false;
}
// 4C5BF0 (key 6) / 4C61A0 (key 8).
bool control(std::uint8_t* data,std::size_t size,unsigned key,NetworkMenuServices& s,unsigned& result){
    Bytes b(data,size);result=0;
    if(key==8){
        unsigned ready{};
        if(!overlay(b,s,0x4659f0)||!overlay(b,s,0x4652e0,nullptr,0,&ready))return false;
        if(!ready&&b.u32(0x154)!=~0u)return true;
        if(b.u8(0x1ec)){result=b.u32(0x1f0);return true;}
    }
    FrontendCarouselServices carousel{s.ui,s.timer};auto w=b.sub(0x34,0x118);
    if(!frontend_carousel_tick_51be30(w,table(key),carousel)){s.missing=carousel.missing;return false;}
    if(key==7&&s.put8)s.put8(s.user,0x84aa00,0);
    unsigned action{};
    if(!input(data,size,s,action))return false;
    if(action==3||action==5){const bool ok=frontend_carousel_move(w,table(key),action==5,carousel);if(!ok)s.missing=carousel.missing;return ok;}
    const auto cursor=b.u32(0xe8);
    if(key==7){                                           // 4C5DF0 (84AA00 = 0 after the tick)
        if(action==1){result=2;return true;}
        if(action!=0)return true;
        driving::object_set_field8_440e00(s.root,1);
        if(s.copy_name)s.copy_name(s.user);else outrun::driving::service_hole("control","s.copy_name");               // 830C20 -> 830C10
        if(!s.put32||!s.put8||!s.session){s.missing=0x4c5e1e;return false;}
        if(cursor==1){s.put32(s.user,0x68c81c,1);s.put8(s.user,0x84aa00,1);b.put32(4,0x1a);result=4;return true;}
        s.put32(s.user,0x68c81c,cursor==0?0u:2u);s.put32(s.user,0x7d68d8,0x16);
        const bool ok=cursor==0?s.session(s.user,0x454140,1,1):s.session(s.user,0x454100,1,0);
        if(!ok)s.missing=cursor==0?0x454140:0x454100;
        return ok;
    }
    if(key==6){
        if(action==1){result=2;return true;}
        if(action!=0)return true;
        constexpr unsigned next[2]{7,8};                  // 68C164: ONLINE, LAN
        if(cursor>1){s.missing=0x4c5c17;return false;}
        b.put32(4,next[cursor]);
        if(next[cursor]==8){driving::object_set_field8_440e00(s.root,0);b.put32(4,8);result=1;return true;}
        driving::object_set_field8_440e00(s.root,1);
        if(!s.online_7d68bc){s.notice_63aa6c=1;b.put32(4,0x16);result=4;return true;}
        b.put32(4,7);result=1;return true;
    }
    if(action==1){b.put32(0x1f0,2);return lan_leave(b,s);}
    if(action!=0)return true;
    driving::object_set_field8_440e00(s.root,0);
    if(s.player_name)s.player_name(s.user);else outrun::driving::service_hole("control","s.player_name");               // 7C23E0 -> 830C10; 4164F0 is a no-op
    if(!s.ready_830c30||!s.ready_830c30(s.user)){
        if(!s.notice||!s.notice(s.user)){s.missing=0x48fc70;return false;}
        return true;
    }
    if(cursor>1)return lan_leave(b,s);
    if(!s.session){s.missing=cursor==0?0x454140:0x454100;return false;}
    if(!s.put32){s.missing=0x7d68d8;return false;}
    s.put32(s.user,0x7d68d8,0x16);                        // the requested network state
    const bool ok=cursor==0?s.session(s.user,0x454140,0,1):s.session(s.user,0x454220,1,0)&&s.session(s.user,0x454100,0,0);
    if(!ok)s.missing=cursor==0?0x454140:0x454100;
    return ok;
}
}
bool network_menu_construct(std::uint8_t* data,std::size_t size,unsigned key,unsigned& repeat){
    if(!data||!valid(Bytes(data,size),key))return false;
    if(!title_base_construct_48f480(data,size,repeat)||!title_ui_resource_construct_465160(data+0x38,size-0x38))return false;
    if(key==8&&!title_ui_resource_construct_465160(data+0x14c,size-0x14c))return false;
    Bytes b(data,size);b.put32(0,key==6?0x5c96c0:key==7?0x5c9744:0x5c97a8);b.put32(8,key);return true;
}
bool network_menu_slot(std::uint8_t* data,std::size_t size,unsigned key,unsigned slot,NetworkMenuServices& s,unsigned& result){
    result=0;if(!data||!valid(Bytes(data,size),key))return false;
    Bytes b(data,size);FrontendCarouselServices carousel{s.ui,s.timer};auto w=b.sub(0x34,0x118);
    switch(slot){
    case 4:                                               // 4C5BD0 / 4C5DC0 / 4C5FE0
        b.put32(4,0x53);
        if(key==8){b.put8(0x1ec,0);b.put32(0x1f0,0);}
        if(!frontend_carousel_init_51b7b0(w,table(key),0,key==7?6:4,carousel)){s.missing=carousel.missing;return false;}
        if(key==7){if(!s.put8||!s.put32){s.missing=0x4c5dd8;return false;}s.put8(s.user,0x84aa00,0);s.put32(s.user,0x68c81c,3);}
        if(key==8&&!overlay_play(b,s,false))return false;
        result=1;return true;
    case 8:return control(data,size,key,s,result);
    case 12:return true;                                  // 49A650
    case 16:                                              // 4C5CC0
        if(!frontend_carousel_release_51bc30(w,carousel)){s.missing=carousel.missing;return false;}
        return true;
    case 0:                                               // 4C5D30 / 4C6120
        if(!frontend_carousel_release_51bc30(w,carousel)){s.missing=carousel.missing;return false;}
        if(key==8&&!overlay(b,s,0x465250))return false;
        return true;
    }
    return false;
}
}
