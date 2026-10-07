#include "platform/frontend_transmission.hpp"
namespace outrun::platform {
using driving::Bytes;
namespace {
constexpr FrontendCarouselDescriptor Records5cd868[4]{{0x440045,0,0x19},{0x440045,0x19,0x2d},{0x440045,0x2d,0x2d},{0x440045,0x5f,0x73}};
constexpr FrontendCarouselTable Table5cd868{Records5cd868,4,0x5cd868};
Bytes object(FrontendTransmission& c){return Bytes(c.object.data(),c.object.size());}
Bytes carousel(FrontendTransmission& c){return object(c).sub(0x34,0x118);}
bool fail(FrontendTransmission& c,FrontendTransmissionServices& s,unsigned pc){if(!c.fault)c.fault=pc;s.missing=pc;return false;}
bool resource(FrontendTransmission& c,FrontendTransmissionServices& s,unsigned pc,const unsigned* a=nullptr,unsigned n=0,unsigned* out=nullptr){
    unsigned result{};
    if(!s.ui.call(pc,object(c).sub(0x14c,0xa0),a,n,result)||s.ui.missing_pc)return fail(c,s,s.ui.missing_pc?s.ui.missing_pc:pc);
    if(out)*out=result;return true;
}
// 4DD300: confirmed/back animation of the selected entry, 84B210 = 5..8.
bool leave_4dd300(FrontendTransmission& c,FrontendTransmissionServices& s){
    FrontendCarouselServices api{s.ui,s.timer};
    if(!frontend_carousel_release_51bc30(carousel(c),api))return fail(c,s,api.missing);
    if(!resource(c,s,0x465250))return false;
    driving::object_transition_latch_441130(s.root);
    auto b=object(c);const bool confirmed=b.u8(0x1ec)==1u;const bool first=b.u32(0xe8)==0u;
    unsigned lo,hi;
    if(confirmed){if(first){lo=0x2e;hi=0x46;s.selection_84b210=5;}else{lo=0x74;hi=0x8a;s.selection_84b210=6;}}
    else{if(first){lo=0x4b;hi=0x5a;s.selection_84b210=7;}else{lo=0x91;hi=0xa0;s.selection_84b210=8;}}
    const unsigned a[]{0x440045,lo,hi,0xc,3,0,0,0x3f800000,0x3f800000,0x3f800000,0};
    if(!resource(c,s,0x465860,a,11)||!resource(c,s,0x465970))return false;
    b.put8(0x1ed,1);return true;
}
}
bool frontend_transmission_construct_4dd410(FrontendTransmission& c,unsigned& repeat){
    auto* p=c.object.data();const auto size=c.object.size();c.fault=0;c.constructed=false;
    if(!title_base_construct_48f480(p,size,repeat))return false;
    auto b=object(c);b.put32(0,0x5cd8d4);
    if(!title_ui_resource_construct_465160(p+0x38,size-0x38)||!title_ui_resource_construct_465160(p+0x14c,size-0x14c))return false;
    b.put32(8,0x10);b.put8(0x1ed,0);b.put8(0x1ec,0);c.constructed=true;return true;
}
bool frontend_transmission_init_4dd250(FrontendTransmission& c,FrontendTransmissionServices& s){
    if(!c.constructed)return fail(c,s,0x4dd250);
    auto b=object(c);b.put8(0x1ed,0);b.put8(0x1ec,0);b.put32(4,4);
    const unsigned a[]{0x440045,0,0x19,0xb,3,0,0,0x3f800000,0x3f800000,0x3f800000,0};
    if(!resource(c,s,0x465860,a,11)||!resource(c,s,0x465970))return false;
    FrontendCarouselServices api{s.ui,s.timer};
    auto w=carousel(c);
    if(!frontend_carousel_init_51b7b0(w,Table5cd868,0,4,api))return fail(c,s,api.missing);
    w.put32(0xb8,10);                                            // 51BE10(10): 0..0x15 accepted
    driving::object_transition_config_440f70(s.root,1,0x1e,7,s.transition);
    return true;
}
bool frontend_transmission_control_4dd4a0(FrontendTransmission& c,FrontendTransmissionServices& s,unsigned& result){
    result=0;if(!c.constructed)return fail(c,s,0x4dd4a0);
    unsigned ready{};
    if(!resource(c,s,0x4659f0)||!resource(c,s,0x4652e0,nullptr,0,&ready))return false;
    if(!ready)return true;
    auto b=object(c);
    if(b.u8(0x1ed)){
        if(b.u8(0x1ec)){driving::object_set_field4_440df0(s.root,0);result=1;return true;}
        result=2;return true;
    }
    FrontendCarouselServices api{s.ui,s.timer};auto w=carousel(c);
    if(!frontend_carousel_tick_51be30(w,Table5cd868,api))return fail(c,s,api.missing);
    unsigned action{};
    if(!frontend_input_action_48f5f0(c.object.data(),c.object.size(),s.input,1,s.repeat,&s,
        [](void* p,unsigned key,int arg){auto& x=*static_cast<FrontendTransmissionServices*>(p);return x.ui.input_feedback(x.root,key,arg);},action))
        return fail(c,s,s.ui.missing_pc?s.ui.missing_pc:0x48f5f0);
    switch(action){
    case 0:b.put8(0x1ed,1);b.put8(0x1ec,1);s.transmission_830374=b.u8(0xe8);return leave_4dd300(c,s); // 48B170
    case 1:b.put8(0x1ed,1);b.put8(0x1ec,0);return leave_4dd300(c,s);
    case 3:if(!frontend_carousel_move(w,Table5cd868,false,api))return fail(c,s,api.missing);return true;     // 51BBA0
    case 5:if(!frontend_carousel_move(w,Table5cd868,true,api))return fail(c,s,api.missing);return true;      // 51BB10
    default:return true;
    }
}
bool frontend_transmission_suspend_4dd2e0(FrontendTransmission& c,FrontendTransmissionServices& s){
    FrontendCarouselServices api{s.ui,s.timer};
    if(!frontend_carousel_release_51bc30(carousel(c),api))return fail(c,s,api.missing);
    if(!resource(c,s,0x465250))return false;
    object(c).put8(0x1ed,0);return true;
}
}
