#include "platform/frontend_stack.hpp"
#include "platform/frontend_welcome.hpp"
#include <array>
#include <cstdio>
#include <cstdlib>
#include <vector>
using namespace outrun;
static void require(bool ok,const char* why){if(!ok){std::fprintf(stderr,"FAIL %s\n",why);std::exit(1);}}
int main(){
    std::array<std::uint8_t,0x1000> bytes{};driving::Bytes owner(bytes.data(),bytes.size());
    struct Trace{std::vector<std::uint32_t> calls;bool create=true,ready=true;} trace;
    platform::FrontendStackServices s{};s.user=&trace;
    s.create=[](void* u,std::uint32_t key){auto& t=*static_cast<Trace*>(u);t.calls.push_back(0x10000u|key);return t.create?key+100u:0u;};
    s.invoke=[](void* u,std::uint32_t handle,std::uint32_t slot){auto& t=*static_cast<Trace*>(u);t.calls.push_back((slot<<16u)|handle);return t.ready?1u:0u;};
    s.release=[](void* u,std::uint32_t handle){static_cast<Trace*>(u)->calls.push_back(0x20000u|handle);};
    s.key=[](void*,std::uint32_t handle){return handle-100u;};
    s.ui=[](void* u,std::uint32_t pc,std::uint32_t key){static_cast<Trace*>(u)->calls.push_back(pc+key);return 0x440051u;};
    owner.put32(0x484u,1u);owner.put32(0x284u,100u);
    owner.put32(0x488u,122u);owner.put8(0x48cu,1u);
    require(platform::frontend_push_444fe0(owner,1u,s),"push root to first choice");
    require(trace.calls==std::vector<std::uint32_t>{0x2007au,0x10001u,0x100064u,0x40065u,0x4432b1u,0x4447d1u},"release/create/suspend/init/transition/open order");
    require(owner.u32(0x484u)==2u&&owner.u32(0x288u)==101u&&owner.u8(0x48cu)==0u&&owner.u32(0xce8u)==0x440051u&&owner.u8(0xcf0u)==1u,"PC stack and UI state");
    require(platform::frontend_push_444fe0(owner,2u,s)&&owner.u32(0x484u)==3u,"push second choice without hard-coded depth");
    trace.calls.clear();require(platform::frontend_pop_444f40(owner,s),"pop");
    require(trace.calls==std::vector<std::uint32_t>{0x20066u,0x40065u,0x444881u},"pop destroys top then reinitializes/restores prior child");
    require(owner.u32(0x484u)==2u,"pop restores depth");
    trace.create=false;trace.calls.clear();require(!platform::frontend_push_444fe0(owner,6u,s)&&owner.u32(0x484u)==2u,"failed factory preserves stack");
    require(trace.calls==std::vector<std::uint32_t>{0x10006u},"failed factory does not suspend previous child");
    trace.create=true;trace.ready=false;trace.calls.clear();
    require(!platform::frontend_push_444fe0(owner,6u,s)&&owner.u32(0x484u)==2u,"failed initialization preserves stack");
    require(trace.calls==std::vector<std::uint32_t>{0x10006u,0x100065u,0x4006au,0x2006au},"failed initialization releases created handle");
    trace.ready=true;owner.put32(0x490u,144u);owner.put8(0x494u,1u);
    require(platform::frontend_pop_444f40(owner,s)&&owner.u32(0x484u)==2u&&owner.u8(0x494u)==0u,"secondary overlay pop does not pop menu");
    require(platform::frontend_push_444fe0(owner,2u,s),"prepare unwind");
    trace.calls.clear();
    require(platform::frontend_unwind_4448c0(owner,0u,s)==0u&&owner.u32(0x484u)==1u,"unwind to root by child identity");
    require(trace.calls==std::vector<std::uint32_t>{0x20066u,0x20065u,0x40064u,0x4432b0u,0x4448c0u},"unwind releases top-down and reinitializes target once");
    trace.calls.clear();
    require(platform::frontend_unwind_4448c0(owner,99u,s)==0u&&trace.calls.empty(),"absent unwind target preserves stack");
    std::puts("PASS frontend stack lifecycle");
}
