#include "frontend_vehicle_menu.hpp"
#include "frontend_vehicle_preview.hpp"
namespace outrun::platform {
using driving::Bytes;
namespace {
Bytes object(FrontendVehicleMenu& c){return {c.object.data(),c.object.size()};}
bool fail(FrontendVehicleMenu& c,FrontendVehicleMenuServices& s,unsigned pc){s.missing=c.fault=pc;return false;}
bool valid(FrontendVehicleMenu& c,FrontendVehicleMenuServices& s){
    if(c.fault){s.missing=c.fault;return false;}
    return (c.constructed&&c.cursor_84b0e8>=0&&c.cursor_84b0e8<15&&c.variant_84b0e9>=0&&c.variant_84b0e9<=1)||fail(c,s,0x4c9290);
}
unsigned selected(const FrontendVehicleMenu& c){return unsigned(c.cursor_84b0e8+15*c.variant_84b0e9);}
bool resource(FrontendVehicleMenu& c,FrontendVehicleMenuServices& s,unsigned off,unsigned pc,const unsigned* a=nullptr,unsigned n=0){
    unsigned result{};return (s.ui.call(pc,object(c).sub(off,0xa0),a,n,result)&&!s.ui.missing_pc)||fail(c,s,s.ui.missing_pc?s.ui.missing_pc:pc);
}
bool external(FrontendVehicleMenu& c,FrontendVehicleMenuServices& s,unsigned pc,unsigned off,std::initializer_list<unsigned> args={}){
    if(s.preview){
        auto b=object(c).sub(off,c.object.size()-off);auto& p=*s.preview;const auto* a=args.begin();bool ok{};
        switch(pc){
            case 0x48c170:ok=args.size()==2&&frontend_vehicle_preview_init_48c170(b,a[0],a[1]!=0,p);break;
            case 0x48c290:ok=args.size()==3&&frontend_vehicle_preview_select_48c290(b,a[0],a[1],a[2]!=0,p);break;
            case 0x48c3f0:ok=args.size()==0&&frontend_vehicle_preview_tick_48c3f0(b,p);break;
            case 0x48c450:ok=args.size()==0&&frontend_vehicle_preview_suspend_48c450(b,p);break;
            default:return (s.external&&s.external(s.user,pc,b,a,args.size()))||fail(c,s,pc);
        }
        return ok||fail(c,s,p.fault?p.fault:pc);
    }
    return (s.external&&s.external(s.user,pc,object(c).sub(off,c.object.size()-off),args.begin(),args.size()))||fail(c,s,pc);
}
bool sound(FrontendVehicleMenu& c,FrontendVehicleMenuServices& s,unsigned id){
    return (s.ui.effect_4249f0&&s.ui.effect_4249f0(s.ui.effect_user,id))||fail(c,s,0x4249f0);
}
FrontendCarouselTable table(const FrontendVehicleMenu& c){
    static const auto rows=[](){std::array<std::array<FrontendCarouselDescriptor,30>,2> out{};
        for(unsigned k=0;k<2;++k)for(unsigned i=0;i<30;++i)out[k][i]={0x4400c0+k,20*(i/2+(i&1)),20*(i/2+1)};
        return out;}();
    return {rows[unsigned(c.variant_84b0e9)].data(),30,c.variant_84b0e9?0x84ae18u:0x84af80u};
}
bool carousel_init(FrontendVehicleMenu& c,FrontendVehicleMenuServices& s){
    auto w=object(c).sub(0xc8,0x118);FrontendCarouselServices api{s.ui,s.timer};
    if(!frontend_carousel_init_51b7b0(w,table(c),0,30,api))return fail(c,s,api.missing);
    frontend_carousel_position_51bd00(w,0,0,0);frontend_carousel_scale_51bc40(w,1,1,0);
    frontend_carousel_select_51bdb0(w,c.cursor_84b0e8);return true;
}
bool preview(FrontendVehicleMenu& c,FrontendVehicleMenuServices& s){
    const auto index=selected(c);
    return external(c,s,0x48c290,0x34,{index,std::uint8_t(c.colours_68eca8[index]+1),unsigned(vehicle_unlocked_4c8f90(s.profile,index))});
}
bool caption(FrontendVehicleMenu& c,FrontendVehicleMenuServices& s){
    const unsigned a[]{0x4400db,0,0,4,0,0,0,0x3f800000,0x3f800000,0x3f800000,0};
    return resource(c,s,0x1530,0x465860,a,11)&&resource(c,s,0x1530,0x465970);
}
bool input(FrontendVehicleMenu& c,FrontendVehicleMenuServices& s,unsigned mode,unsigned& action){
    if(!frontend_input_action_48f5f0(c.object.data(),c.object.size(),s.input,int(mode),s.repeat,&s,
        [](void* p,unsigned key,int arg){auto& s=*static_cast<FrontendVehicleMenuServices*>(p);return s.ui.input_feedback(s.root,key,arg);},action))
        return fail(c,s,s.ui.missing_pc?s.ui.missing_pc:0x48f5f0);
    return true;
}
}
bool frontend_vehicle_menu_construct_4c8de0(FrontendVehicleMenu& c,unsigned& repeat){
    auto* p=c.object.data();const auto size=c.object.size();c.fault=0;c.constructed=false;
    if(!title_base_construct_48f480(p,size,repeat))return false;
    auto b=object(c);b.put32(0,0x5c9a70);
    if(!frontend_vehicle_preview_construct_48bf00(b.sub(0x34,0x94)))return false;
    if(!title_ui_resource_construct_465160(p+0xcc,size-0xcc)||!title_controller_construct_48c490(p+0x1e0,size-0x1e0,repeat))return false;
    for(unsigned off:{0x1490u,0x1530u,0x15d4u})if(!title_ui_resource_construct_465160(p+off,size-off))return false;
    b.put32(8,0xa);b.put8(0x15d0,0);b.put8(0x15d1,0);c.constructed=true;return true;
}
bool frontend_vehicle_menu_init_4c9010(FrontendVehicleMenu& c,FrontendVehicleMenuServices& s){
    if(!valid(c,s))return false;
    if(c.initialized_84b0ea)return true;
    auto b=object(c);b.put32(4,0x53);b.put8(0x15d0,0);
    if(!carousel_init(c,s))return false;
    const auto index=selected(c);const bool unlocked=vehicle_unlocked_4c8f90(s.profile,index);
    if(unlocked&&!vehicle_colour_unlocked_4c8fc0(s.profile,index,unsigned(c.colours_68eca8[index]))){
        unsigned colour=0;while(colour<8&&!vehicle_colour_unlocked_4c8fc0(s.profile,index,colour))++colour;
        c.colours_68eca8[index]=std::int8_t(colour==8?0:colour);
    }
    if(!external(c,s,0x48c170,0x34,{index,unsigned(unlocked)})||!preview(c,s))return false;
    const unsigned labels[]{4,0x295,0x10,0x29b,~0u,~0u,8,0x296};unsigned result{};
    if(!s.ui.commands(0x440ea0,s.root.sub(0x51c,s.root.size()-0x51c),labels,8,s.globals,result))return fail(c,s,s.ui.missing_pc?s.ui.missing_pc:0x440ea0);
    if(!caption(c,s))return false;
    c.initialized_84b0ea=true;return true;
}
bool frontend_vehicle_menu_control_4c9290(FrontendVehicleMenu& c,FrontendVehicleMenuServices& s,unsigned& action){
    action=0;if(!valid(c,s))return false;
    if(!external(c,s,0x48c3f0,0x34)||!external(c,s,0x4c50d0,0)||
       !external(c,s,0x4c50f0,0,{0})||!external(c,s,0x4c5100,0,{0x3f800000}))return false;
    driving::object_store_depth_pair_442f20(s.root,0,0);
    auto b=object(c),w=b.sub(0xc8,0x118);FrontendCarouselServices carousel{s.ui,s.timer};unsigned pressed{};
    if(b.u8(0x15d0)){
        if(!external(c,s,0x48d420,0x1e0)||!input(c,s,1,pressed)||!input(c,s,1,pressed))return false;
        if(pressed<=1){if(!external(c,s,0x48ca30,0x1e0))return false;b.put8(0x15d0,0);}
    }else{
        if(!input(c,s,0,pressed))return false;
        if(pressed==0){
            const auto index=selected(c);if(!vehicle_unlocked_4c8f90(s.profile,index))return sound(c,s,3);
            b.put32(4,0x10);
            if(!external(c,s,0x48b130,0,{VehicleMenuModels[index]})||
               !external(c,s,0x48b190,0,{unsigned(c.variant_84b0e9!=0)})||
               !external(c,s,0x48b150,0,{std::uint8_t(c.colours_68eca8[index]+1)})||!sound(c,s,64))return false;
            action=4;return true;
        }
        if(pressed==1){action=2;return true;}
        if(pressed==3||pressed==5){
            const auto old=w.i32(0xb4);
            if(!frontend_carousel_move(w,table(c),pressed==5,carousel))return fail(c,s,carousel.missing);
            if(old!=w.i32(0xb4)){
                c.cursor_84b0e8=std::int8_t(w.i32(0xb4));
                if(!preview(c,s)||!resource(c,s,0x1530,0x465250)||!caption(c,s))return false;
            }
        }
        if(s.input.device_held&0x2000){
            if(!frontend_carousel_release_51bc30(w,carousel))return fail(c,s,carousel.missing);
            c.variant_84b0e9=std::int8_t(1-c.variant_84b0e9);
            if(!resource(c,s,0x1530,0x465250)||!carousel_init(c,s)||!preview(c,s)||!caption(c,s))return false;
        }
        if(!s.colour_held_4536c0)b.put8(0x15d1,0);
        else if(!b.u8(0x15d1)){
            b.put8(0x15d1,1);const auto index=selected(c);
            if(vehicle_unlocked_4c8f90(s.profile,index)){
                const auto old=c.colours_68eca8[index];
                if(old<0||old>=8)return fail(c,s,0x4c9620);
                unsigned next=unsigned(old);
                do{next=(next+1)%8;}while(next!=unsigned(old)&&!vehicle_colour_unlocked_4c8fc0(s.profile,index,next));
                c.colours_68eca8[index]=std::int8_t(next);
                if(next!=unsigned(old)&&!preview(c,s))return false;
            }
        }
    }
    if(!frontend_carousel_tick_51be30(w,table(c),carousel))return fail(c,s,carousel.missing);
    if(!resource(c,s,0x15d4,0x4659f0))return false;
    const auto index=unsigned(c.variant_84b0e9)*15+w.u32(0xb4);
    if(index>=30)return fail(c,s,0x4c96c8);
    if(vehicle_unlocked_4c8f90(s.profile,index))return resource(c,s,0x15d4,0x465250);
    const unsigned locked[]{0x440047,5,5,5,1,0xc3898000,0xc1c00000,0x3f800000,0x3f800000,0x3f800000,0};
    return resource(c,s,0x15d4,0x465860,locked,11)&&resource(c,s,0x15d4,0x465970);
}
bool frontend_vehicle_menu_display_4c8d70(FrontendVehicleMenu& c,FrontendVehicleMenuServices& s){
    return valid(c,s)&&(!object(c).u8(0x15d0)||external(c,s,0x48c5f0,0x1e0));
}
bool frontend_vehicle_menu_suspend_4c8d90(FrontendVehicleMenu& c,FrontendVehicleMenuServices& s){
    c.initialized_84b0ea=false;
    if(!external(c,s,0x48c450,0x34)||!resource(c,s,0x1530,0x465250))return false;
    FrontendCarouselServices carousel{s.ui,s.timer};auto b=object(c);
    if(!frontend_carousel_release_51bc30(b.sub(0xc8,0x118),carousel))return fail(c,s,carousel.missing);
    if(b.u8(0x15d0)&&!resource(c,s,0x15d4,0x465250))return false;
    b.put8(0x15d0,0);return external(c,s,0x4c5110,0);
}
}
