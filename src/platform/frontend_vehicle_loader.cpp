#include "frontend_vehicle_loader.hpp"
#include "frontend_vehicle_data.hpp"
#include "driving/pc_common_control.hpp"
namespace outrun::platform {
using driving::Bytes;
namespace {
bool fault(FrontendVehicleLoader& s,unsigned pc){s.fault=pc;return false;}
bool resource(FrontendVehicleLoader& s,unsigned model,unsigned& id){
    return vehicle_resource_46bbe0(model,id)||fault(s,0x46bbe0);
}
bool release(FrontendVehicleLoader& s,const FrontendVehicleLoaderServices& api,unsigned id){
    return (api.release_448990&&api.release_448990(api.user,id))||fault(s,0x448990);
}
bool request(FrontendVehicleLoader& s,const FrontendVehicleLoaderServices& api,unsigned id,unsigned mode){
    return (api.request_448ad0&&api.request_448ad0(api.user,id,mode))||fault(s,0x448ad0);
}
}
bool frontend_vehicle_loader_init_48bf20(FrontendVehicleLoader& s,const FrontendVehicleLoaderServices& api){
    if(s.fault)return false;
    // Share the initializer already used by 445500 stage 3. The wrapper adds
    // observable native I/O failures without a second copy of the PC layout.
    struct Adapter {FrontendVehicleLoader& state;const FrontendVehicleLoaderServices& api;} adapter{s,api};
    driving::runtime_loader_begin_48bf20(Bytes(s.object.data(),s.object.size()),{&adapter,
        [](void* p,unsigned,unsigned id,unsigned mode){auto& a=*static_cast<Adapter*>(p);
            if(!a.state.fault)(void)request(a.state,a.api,id,mode);}});
    return !s.fault;
}
bool frontend_vehicle_loader_ready_48bf80(const FrontendVehicleLoader& s,unsigned model){
    // The original read does not mutate the object.
    auto u32=[&](unsigned o){const auto* p=s.object.data()+o;return unsigned(p[0])|(unsigned(p[1])<<8)|(unsigned(p[2])<<16)|(unsigned(p[3])<<24);};
    for(unsigned slot=0;slot<3;++slot)if(u32(0x14+12*slot)==model)return u32(0x18+12*slot)==3;
    return false;
}
bool frontend_vehicle_loader_ready_48bf80(Bytes b,unsigned model){
    if(b.size()<0x34)return false;
    for(unsigned slot=0;slot<3;++slot)if(b.u32(0x14+12*slot)==model)return b.u32(0x18+12*slot)==3;
    return false;
}
bool frontend_vehicle_loader_empty_48bfc0(const FrontendVehicleLoader& s){
    for(unsigned slot=0;slot<3;++slot)for(unsigned j=0;j<4;++j)if(s.object[0x18+12*slot+j])return false;
    return true;
}
bool frontend_vehicle_loader_tick_48bfe0(FrontendVehicleLoader& s,const FrontendVehicleLoaderServices& api){
    if(s.fault)return false;
    Bytes b(s.object.data(),s.object.size());
    for(unsigned slot=0;slot<3;++slot){
        const auto o=0x10+12*slot;
        if(b.u8(0)&&b.u32(o+8)==3){
            for(unsigned i=0;i<3;++i)if(b.u32(4+4*i)==b.u32(o+4))b.put32(4+4*i,30);
            unsigned id{};if(!resource(s,b.u32(o+4),id)||!release(s,api,id))return false;
            b.put32(o+4,30);b.put32(o+8,0);
        }
        const auto state=b.u32(o+8);
        if(state==1){
            unsigned id{};if(!resource(s,b.u32(o+4),id)||!request(s,api,id,b.u32(o)))return false;
            b.put32(o+8,2);
        }else if(state==2){
            unsigned id{};bool ready{};if(!resource(s,b.u32(o+4),id))return false;
            if(!api.ready_448960||!api.ready_448960(api.user,id,ready))return fault(s,0x448960);
            if(ready)b.put32(o+8,3);
        }
    }
    // The original does not evict or schedule while any slot is loading.
    for(unsigned slot=0;slot<3;++slot)if(b.u32(0x18+12*slot)==2)return true;
    for(unsigned i=0;i<3;++i){
        const auto model=b.u32(4+4*i);
        if(model==30||frontend_vehicle_loader_ready_48bf80(s,model))continue;
        for(unsigned slot=0;slot<3;++slot){
            const auto o=0x10+12*slot,old=b.u32(o+4);
            bool desired=false;for(unsigned j=0;j<3;++j)desired=desired||old==b.u32(4+4*j);
            if(desired||b.u32(o+8)==2)continue;
            if(b.u32(o+8)){
                unsigned id{};if(!resource(s,old,id)||!release(s,api,id))return false;
                b.put32(o+4,30);b.put32(o+8,0);
            }
            b.put32(o+4,model);b.put32(o+8,1);return true;
        }
    }
    return true;
}
}
