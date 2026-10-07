#include "frontend_vehicle_preview.hpp"
#include "frontend_vehicle_data.hpp"
#include <initializer_list>
namespace outrun::platform {
using driving::Bytes;
namespace {
bool fail(FrontendVehiclePreviewServices& s,unsigned pc){s.fault=pc;return false;}
bool valid(Bytes b,FrontendVehiclePreviewServices& s,unsigned pc){
    if(s.fault)return false;
    return (b.size()>=0x94&&s.loader.size()>=0x34)||fail(s,pc);
}
bool call(FrontendVehiclePreviewServices& s,unsigned pc,std::initializer_list<unsigned> a={}){
    if(s.creation&&(pc==0x49fa60||pc==0x4406f0)){
        const auto* args=a.begin();bool ok{};
        if(pc==0x49fa60)ok=a.size()==0&&vehicle_creation_reset_49fa60(*s.creation);
        else if(a.size()==4)ok=vehicle_creation_preview_4406f0(*s.creation,args[0],args[1],args[2],{&s,
            [](void* p,unsigned event,unsigned function){auto& s=*static_cast<FrontendVehiclePreviewServices*>(p);
                const unsigned args[]{event,function};return s.call&&s.call(s.user,0x440110,args,2);}});
        return ok||fail(s,s.creation->fault?s.creation->fault:pc);
    }
    return (s.call&&s.call(s.user,pc,a.begin(),a.size()))||fail(s,pc);
}
void neighborhood(Bytes loader,unsigned index,bool current_first){
    const unsigned previous=VehicleMenuModels[(index+29)%30],current=VehicleMenuModels[index];
    loader.put32(4,current_first?current:previous);loader.put32(8,current_first?previous:current);
    loader.put32(12,VehicleMenuModels[(index+1)%30]);
}
}
bool frontend_vehicle_preview_construct_48bf00(Bytes b){
    if(b.size()<0x94)return false;
    b.put32(0,0);b.put32(4,1);b.put8(8,1);b.put32(12,0);b.put32(16,0);b.put8(0x90,0);return true;
}
bool frontend_vehicle_preview_init_48c170(Bytes b,unsigned index,bool unlocked,FrontendVehiclePreviewServices& s){
    if(!valid(b,s,0x48c170))return false;
    if(index>=30||s.scene_lists.size()<12)return fail(s,0x48c170);
    b.put32(4,index);b.put8(0x90,std::uint8_t(unlocked));b.put8(8,1);
    if(!call(s,0x40ec60,{0})||!call(s,0x44c0a0,{10})||!call(s,0x49fa60)||
       !call(s,0x49a650)||!call(s,0x44c0d0)||!call(s,0x49a650))return false;
    neighborhood(s.loader,index,false);
    s.scene_lists.put32(4,0x844a08);s.scene_lists.put32(8,0x844a08);
    s.loader.put8(0,0);return true;
}
bool frontend_vehicle_preview_open_48c220(Bytes b,unsigned model,unsigned colour,bool unlocked,FrontendVehiclePreviewServices& s){
    if(!valid(b,s,0x48c220))return false;
    if(b.u32(0))return true;
    if(model>=30)return fail(s,0x48c220);
    b.put32(0,1);
    return call(s,0x49fa60)&&call(s,0x4406f0,{model,colour,unsigned(!unlocked),0x3e800000})&&call(s,0x440110,{0x181,13});
}
bool frontend_vehicle_preview_close_48c260(Bytes b,FrontendVehiclePreviewServices& s){
    if(!valid(b,s,0x48c260))return false;
    if(!b.u32(0))return true;
    b.put32(0,0);return call(s,0x4401d0,{0x181})&&call(s,0x440330,{8,24});
}
bool frontend_vehicle_preview_select_48c290(Bytes b,unsigned index,unsigned colour,bool unlocked,FrontendVehiclePreviewServices& s){
    if(!valid(b,s,0x48c290))return false;
    if(index>=30)return fail(s,0x48c290);
    const auto colour_byte=std::uint8_t(colour),unlocked_byte=std::uint8_t(unlocked);
    if(b.u32(4)==index){
        if(b.u8(8)==colour_byte&&b.u8(0x90)==unlocked_byte)return true;
        if(s.vehicle.size()<0x13)return fail(s,0x799d18);
        b.put8(8,colour_byte);b.put8(0x90,unlocked_byte);
        s.vehicle.put8(0x12,colour_byte);
        s.vehicle.put32(4,(s.vehicle.u32(4)&~0x40u)|(unlocked?0:0x40));
        return true;
    }
    b.put32(4,index);b.put8(8,colour_byte);b.put8(0x90,unlocked_byte);
    if(!frontend_vehicle_preview_close_48c260(b,s))return false;
    neighborhood(s.loader,index,true);return true;
}
bool frontend_vehicle_preview_tick_48c3f0(Bytes b,FrontendVehiclePreviewServices& s){
    if(!valid(b,s,0x48c3f0))return false;
    for(unsigned id:{0xbau,0xbbu}){
        bool ready{};if(!s.ready_448960||!s.ready_448960(s.user,id,ready))return fail(s,0x448960);
        if(!ready)return true;
    }
    if(b.u32(0))return true;
    const unsigned index=b.u32(4);if(index>=30)return fail(s,0x48c3f0);
    const unsigned model=VehicleMenuModels[index];
    if(!frontend_vehicle_loader_ready_48bf80(s.loader,model))return true;
    return frontend_vehicle_preview_open_48c220(b,model,b.u8(8),b.u8(0x90)!=0,s);
}
bool frontend_vehicle_preview_suspend_48c450(Bytes b,FrontendVehiclePreviewServices& s){
    if(!valid(b,s,0x48c450))return false;
    b.put32(0,0);b.put32(16,0);b.put32(12,0);
    // Unlike close48C260, suspend closes the event even if already inactive.
    if(!call(s,0x4401d0,{0x181})||!call(s,0x440330,{8,24}))return false;
    s.loader.put8(0,1);
    return call(s,0x44c3d0)&&call(s,0x44a1a0)&&call(s,0x4f2210);
}
}
