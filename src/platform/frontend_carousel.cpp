#include "frontend_carousel.hpp"
#include <cmath>
namespace outrun::platform {
using driving::Bytes;
namespace {
bool resource(Bytes w,FrontendCarouselServices& s,unsigned pc,const unsigned* args=nullptr,unsigned n=0,unsigned* out=nullptr){
    unsigned result{};const bool ok=s.ui.call(pc,w.sub(4,0xa0),args,n,result)&&!s.ui.missing_pc;
    if(out)*out=result;
    if(!ok)s.missing=s.ui.missing_pc?s.ui.missing_pc:pc;
    return ok;
}
bool configure(Bytes w,FrontendCarouselTable t,unsigned index,bool reverse,bool hold,FrontendCarouselServices& s){
    if(!t.records||index>=t.count){s.missing=0x51be30;return false;}
    const auto d=t.records[index];
    const unsigned args[]{d.token,reverse?d.last:d.first,reverse?d.first:d.last,w.u32(0xb8),hold?0u:3u,
        w.u32(0xbc),w.u32(0xc0),w.u32(0xe0),w.u32(0xe4),reverse?0xbf800000u:0x3f800000u,0};
    return resource(w,s,0x465250)&&resource(w,s,0x465860,args,11)&&resource(w,s,0x465970);
}
void integrate(Bytes w,unsigned flag,unsigned current,float delta){
    if(!w.u32(flag))return;
    const auto target=current+12,step=current+24;bool done=true;
    for(unsigned i=0;i<3;++i){const auto off=i*4;const float speed=w.f32(step+off),end=w.f32(target+off);
        const float value=w.f32(current+off)+speed*delta;w.putf(current+off,value);
        if(i<2)done=done&&(speed==0||(speed<0&&value<=end)||(speed>0&&value>=end));}
    if(done){for(unsigned i=0;i<3;++i)w.put32(current+i*4,w.u32(target+i*4));w.put32(flag,0);}
}
}
bool frontend_carousel_init_51b7b0(Bytes w,FrontendCarouselTable t,unsigned first,unsigned end,FrontendCarouselServices& s){
    if(w.size()<0x118||!t.records||end>t.count||first>=end||!std::isfinite(s.timer)){s.missing=0x51b7b0;return false;}
    w.put32(0,t.pc_address);if(!resource(w,s,0x465250))return false;
    w.put32(0xa4,first);w.put32(0xa8,end);w.put32(0xac,(end-first-1)/2);
    for(unsigned off=0xb0;off<=0x114;off+=4)w.put32(off,0);
    for(unsigned off:{0xe0u,0xe4u,0xecu,0xf0u,0xf4u})w.putf(off,1);
    w.putf(0x10c,s.timer);w.putf(0x110,s.timer);return true;
}
bool frontend_carousel_release_51bc30(Bytes w,FrontendCarouselServices& s){return resource(w,s,0x465250);}
void frontend_carousel_select_51bdb0(Bytes w,int index){
    if(index<0||index>w.i32(0xac))return;
    const int old=w.i32(0xb4),difference=index-old;
    w.puti(0xb0,difference>1?index-1:difference< -1?index+1:old);w.puti(0xb4,index);
}
void frontend_carousel_position_51bd00(Bytes w,float x,float y,float duration){
    w.put32(0x104,1);w.putf(0xc8,x);w.putf(0xcc,y);w.putf(0xd0,0);w.putf(0xdc,0);
    if(duration==0){w.putf(0xbc,x);w.putf(0xc0,y);w.putf(0xc4,0);w.putf(0xd4,0);w.putf(0xd8,0);}
    else{const float inv=1.f/duration;w.putf(0xd4,(x-w.f32(0xbc))*inv);w.putf(0xd8,(y-w.f32(0xc0))*inv);}
}
void frontend_carousel_scale_51bc40(Bytes w,float x,float y,float duration){
    w.put32(0x108,1);w.putf(0xec,x);w.putf(0xf0,y);w.putf(0x100,0);
    if(duration==0){w.putf(0xe0,x);w.putf(0xe4,y);w.putf(0xe8,0);w.putf(0xf4,0);w.putf(0xf8,0);w.putf(0xfc,0);}
    else{w.putf(0xf4,1);const float inv=1.f/duration;w.putf(0xf8,(x-w.f32(0xe0))*inv);w.putf(0xfc,(y-w.f32(0xe4))*inv);}
}
bool frontend_carousel_move(Bytes w,FrontendCarouselTable t,bool forward,FrontendCarouselServices& s){
    const int old=w.i32(0xb4);if(forward?old>=w.i32(0xac):old<=0)return true;
    w.puti(0xb0,old);w.puti(0xb4,old+(forward?1:-1));
    if(!configure(w,t,unsigned(old*2+(forward?2:0)),!forward,false,s))return false;
    if(!s.ui.effect_4249f0||!s.ui.effect_4249f0(s.ui.effect_user,1)){s.missing=0x4249f0;return false;}
    return true;
}
bool frontend_carousel_tick_51be30(Bytes w,FrontendCarouselTable t,FrontendCarouselServices& s){
    unsigned ready{};
    if(!resource(w,s,0x4659f0)||!resource(w,s,0x4652e0,nullptr,0,&ready))return false;
    if((ready||w.u32(0xc)==~0u)&&!configure(w,t,w.u32(0xb4)*2+1,false,true,s))return false;
    w.put32(0x10c,w.u32(0x110));w.putf(0x110,s.timer);w.putf(0x114,s.timer-w.f32(0x10c));
    integrate(w,0x104,0xbc,w.f32(0x114));integrate(w,0x108,0xe0,w.f32(0x114));return true;
}
}
