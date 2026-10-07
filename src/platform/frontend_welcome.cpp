#include "frontend_welcome.hpp"
#include <initializer_list>
#include "driving/service_hole.hpp"

namespace outrun::platform {
namespace {
std::uint32_t call(const FrontendWelcomeServices& s,std::uint32_t pc,
                   std::initializer_list<std::uint32_t> args={}){
    return s.call?s.call(s.user,pc,args.begin(),args.size()):(outrun::driving::service_hole("frontend_welcome.cpp:8","s.call"),0u);
}
}
bool frontend_welcome_init_4c5180(driving::Bytes child,const FrontendWelcomeServices& s){
    child.check(0x754u,4u);
    child.put32(4u,21u);child.put32(0x74cu,0u);child.put32(0x754u,0u);
    call(s,0x401030u,{0u});call(s,0x4afea0u,{1u});
    child.put32(0x744u,call(s,0x428320u,{0x4400d8u,0u,0u}));
    child.put32(0x740u,0u);child.put8(0x751u,0u);
    call(s,0x442f20u,{0u,0u});call(s,0x447000u,{0u,0u});
    call(s,0x440ea0u,{~0u,~0u,~0u,~0u,~0u,~0u,~0u,~0u});
    child.put8(0x750u,1u);return true;
}
std::uint32_t frontend_welcome_control_4c5210(driving::Bytes child,const FrontendWelcomeServices& s){
    child.check(0x754u,4u);
    switch(child.u32(0x740u)){
    case 0u:
        call(s,0x414790u,{1u});child.put32(0x740u,1u);
        call(s,0x4413f0u,{0u});child.put32(0x754u,0u);break;
    case 1u:
        if(const auto timer=child.u32(0x754u)){
            child.put32(0x754u,timer-1u);
            if(timer==1u){child.put8(0x751u,child.u8(0x751u)==0u);child.put32(0x740u,0u);}
        }else if(call(s,0x4536c0u,{1u})||call(s,0x4536c0u,{4u})){
            call(s,0x4285a0u,{child.u32(0x744u)});call(s,0x414790u,{~0u});
            child.put32(0x740u,2u);call(s,0x4413f0u,{1u});call(s,0x4249f0u,{0x40u});
        }else if(call(s,0x4536f0u,{2u})||call(s,0x4536f0u,{8u})){
            const auto text=call(s,0x465eb0u,{0x4b3u});
            call(s,0x492690u,{0x17u,text,~0u,10u});child.put32(4u,46u);
            call(s,0x4249f0u,{0u});return 4u;
        }
        break;
    case 2u:
        child.put32(4u,call(s,0x416380u));call(s,0x4040f0u);return 1u;
    default:break;
    }
    return 0u;
}
void frontend_welcome_suspend_4c5350(driving::Bytes child,const FrontendWelcomeServices& s){
    child.check(0x754u,4u);
    if(child.u8(0x750u)){
        call(s,0x4285a0u,{child.u32(0x744u)});call(s,0x414790u,{~0u});
        child.put32(0x740u,2u);call(s,0x4413f0u,{1u});
        call(s,0x401000u,{0u,30u,1u});child.put8(0x750u,0u);
    }
    call(s,0x442f20u,{1u,1u});
}
}
