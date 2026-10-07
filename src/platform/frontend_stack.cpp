#include "frontend_stack.hpp"
#include <stdexcept>
#include "driving/service_hole.hpp"
namespace outrun::platform {
std::uint32_t frontend_unwind_4448c0(driving::Bytes owner,std::uint32_t target,const FrontendStackServices& s){
    if(target==~0u)return 0u;
    auto depth=owner.u32(0x484u);
    if(depth>0x80u)throw std::out_of_range("frontend unwind depth");
    std::uint32_t selected=0u;
    for(;selected<depth;++selected){
        if(s.key&&s.key(s.user,owner.u32(0x284u+selected*4u))==target)break;
    }
    if(selected==depth)return 0u;
    const bool removed=selected+1u<depth;
    while(depth>selected+1u){
        const auto handle=owner.u32(0x280u+depth*4u);
        if(handle&&s.release)s.release(s.user,handle);
        owner.put32(0x484u,--depth);
    }
    const auto handle=owner.u32(0x280u+depth*4u);
    if(removed&&(!s.invoke||std::uint8_t(s.invoke(s.user,handle,4u))==0u))return 1u;
    const auto key=s.key?s.key(s.user,handle):(outrun::driving::service_hole("frontend_unwind_4448c0","s.key"),target);
    const auto transition=s.ui?s.ui(s.user,0x4432b0u,key):(outrun::driving::service_hole("frontend_unwind_4448c0","s.ui"),~0u);
    owner.put32(0xce8u,transition);owner.put8(0xcf0u,transition!=~0u);owner.put8(0xcf1u,0u);
    if(s.ui)s.ui(s.user,0x4448c0u,key);else outrun::driving::service_hole("frontend_unwind_4448c0","s.ui");
    return 0u;
}
void frontend_close_overlays_4430b0(driving::Bytes owner,const FrontendStackServices& s){
    for(const auto offset:{0x490u,0x488u}){
        if(!owner.u8(offset+4u))continue;
        if(const auto handle=owner.u32(offset)){
            if(s.release)s.release(s.user,handle);else outrun::driving::service_hole("frontend_close_overlays_4430b0","s.release");
            owner.put32(offset,0u);
        }
        owner.put8(offset+4u,0u);
    }
}
bool frontend_push_444fe0(driving::Bytes owner,std::uint32_t key,const FrontendStackServices& s){
    const auto depth=owner.u32(0x484u);
    if(depth==0u||depth>=0x80u)throw std::out_of_range("frontend push depth");
    frontend_close_overlays_4430b0(owner,s);
    const auto created=s.create?s.create(s.user,key):(outrun::driving::service_hole("frontend_push_444fe0","s.create"),0u);
    if(!created)return false;
    if(s.invoke)s.invoke(s.user,owner.u32(0x280u+depth*4u),0x10u);else outrun::driving::service_hole("frontend_push_444fe0","s.invoke");
    if(!s.invoke||std::uint8_t(s.invoke(s.user,created,4u))==0u){
        if(s.release)s.release(s.user,created);else outrun::driving::service_hole("frontend_push_444fe0","s.release");
        return false;
    }
    owner.put32(0x284u+depth*4u,created);owner.put32(0x484u,depth+1u);
    const auto created_key=s.key?s.key(s.user,created):(outrun::driving::service_hole("frontend_push_444fe0","s.key"),key);
    const auto transition=s.ui?s.ui(s.user,0x4432b0u,created_key):(outrun::driving::service_hole("frontend_push_444fe0","s.ui"),~0u);
    owner.put32(0xce8u,transition);owner.put8(0xcf0u,transition!=~0u);owner.put8(0xcf1u,0u);
    if(s.ui)s.ui(s.user,0x4447d0u,created_key);else outrun::driving::service_hole("frontend_push_444fe0","s.ui");
    return true;
}
bool frontend_pop_444f40(driving::Bytes owner,const FrontendStackServices& s){
    if(owner.u8(0x494u)){
        if(s.release)s.release(s.user,owner.u32(0x490u));else outrun::driving::service_hole("frontend_pop_444f40","s.release");
        owner.put32(0x490u,0u);owner.put8(0x494u,0u);return true;
    }
    if(owner.u8(0x48cu)){frontend_close_overlays_4430b0(owner,s);return true;}
    auto depth=owner.u32(0x484u);
    if(depth==0u||depth>0x80u)throw std::out_of_range("frontend pop depth");
    if(depth>1u){
        if(s.release)s.release(s.user,owner.u32(0x280u+depth*4u));else outrun::driving::service_hole("frontend_pop_444f40","s.release");
        --depth;owner.put32(0x484u,depth);
        if(!s.invoke||std::uint8_t(s.invoke(s.user,owner.u32(0x280u+depth*4u),4u))==0u)return false;
    }
    const auto handle=owner.u32(0x280u+depth*4u);
    if(s.ui&&s.key)s.ui(s.user,0x444880u,s.key(s.user,handle));
    return true;
}
}
