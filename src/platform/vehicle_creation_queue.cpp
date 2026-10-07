#include "vehicle_creation_queue.hpp"
#include <algorithm>
namespace outrun::platform {
using driving::Bytes;
bool vehicle_creation_reset_49fa60(VehicleCreationQueue& s){
    if(s.fault)return false;
    Bytes b(s.object.data(),s.object.size());b.put8(0x108,0);b.put8(0x109,0);
    std::fill(s.object.begin()+0x110,s.object.begin()+0x4d0,0);return true;
}
bool vehicle_creation_append_49fa80(VehicleCreationQueue& s,unsigned event,unsigned lane,unsigned model,unsigned colour,unsigned flags){
    if(s.fault)return false;
    Bytes b(s.object.data(),s.object.size());const auto index=b.i8(0x109);
    // Outside the sixteen-record table the PC would corrupt adjacent globals.
    if(index<0||index>=16){s.fault=0x49fa80;return false;}
    const auto off=unsigned(index)*16;
    b.put32(off,event);b.put32(off+4,flags);b.put8(off+12,std::uint8_t(lane));
    b.put32(off+8,model);b.put8(off+13,std::uint8_t(colour));b.put8(0x109,std::uint8_t(index+1));return true;
}
bool vehicle_creation_preview_4406f0(VehicleCreationQueue& s,unsigned model,unsigned colour,unsigned hidden,const VehicleCreationServices& api){
    if(s.fault)return false;
    const unsigned flags=0x4081|((hidden&1)<<6);
    if(!api.event_setup_440110||!api.event_setup_440110(api.user,8,0x2c)){s.fault=0x440110;return false;}
    const auto signed_colour=static_cast<std::int8_t>(std::uint8_t(colour));
    const unsigned clamped=signed_colour<1?1:signed_colour>8?8:unsigned(signed_colour);
    return vehicle_creation_append_49fa80(s,8,0,model,clamped,flags);
}
bool vehicle_creation_offline_440380(VehicleCreationQueue& s,unsigned model,unsigned colour,unsigned player_slot,const VehicleCreationServices& api){
    if(s.fault)return false;
    // 59C6B0 has eight permutations; the first byte of row N is N.
    if(player_slot>=8){s.fault=0x455ad0;return false;}
    if(!api.event_setup_440110||!api.event_setup_440110(api.user,8,0x26)){s.fault=0x440110;return false;}
    const unsigned clamped=std::clamp(unsigned(std::uint8_t(colour)),1u,8u);
    const auto signed_model=static_cast<std::int8_t>(std::uint8_t(model));
    return vehicle_creation_append_49fa80(s,8,player_slot,unsigned(std::int32_t(signed_model)),clamped,0x4081);
}
}
namespace outrun::platform {
bool vehicle_creation_network_440380(VehicleCreationQueue& s,unsigned count,unsigned self,unsigned variant,
                                     const std::uint8_t* commrace,std::size_t size,const VehicleCreationServices& api){
    if(s.fault)return false;
    if(self>=8){s.fault=0x455ad0;return false;}
    for(unsigned lane=8;lane-8<4u;++lane){                    // [680AD4] = 4
        if(lane>=count+8)continue;
        const unsigned j=lane-8;
        const unsigned slot=j==0?self:(j-1<self?j-1:j);         // 59C6B0 row [self]
        const std::size_t at=0xdu+std::size_t(slot)*0x6cu;      // 7DE425 + slot*0x6C (4579E0 / 4579F0)
        if(at+1>=size){s.fault=0x4579e0;return false;}
        const auto model=static_cast<std::int8_t>(commrace[at]);
        const unsigned colour=std::clamp(unsigned(commrace[at+1]),1u,8u);
        const unsigned function=lane==8?0x26u:variant==4?0x67u:0x54u;
        if(!api.event_setup_440110||!api.event_setup_440110(api.user,lane,function)){s.fault=0x440110;return false;}
        if(!vehicle_creation_append_49fa80(s,lane,slot,unsigned(std::int32_t(model)),colour,lane==8?0x4081u:0x40a3u))return false;
    }
    return true;
}
}
