#include "driving/pc_transmission.hpp"
#include "driving/pc_driving.hpp"
#include <cstdint>
#include <stdexcept>
namespace outrun::driving {
void auto_transmission(Bytes e,Bytes p){
    const std::int32_t pedal=e.i32(0x38);
    std::uint32_t gear=e.u32(0x208);
    // Valid game states index one of the seven gear threshold entries. Keep a
    // fail-closed bound in native code rather than reproducing an OOB guest read.
    if(gear>7u)throw std::domain_error("automatic transmission gear outside validated table");
    const float speed=e.f32(0x1c4);
    const std::size_t down_base=pedal<128?0xe2cu:0xe48u;
    if(speed>e.f32(0xe10u+std::size_t(gear)*4u) && pedal<128){
        ++gear;e.put8(0x296,1);
    }else if(e.f32(down_base+std::size_t(gear)*4u)>speed && e.u8(0x282)==0){
        --gear;e.put8(0x296,3);
    }
    const std::uint32_t maximum=p.u32(0x10a0);
    if(gear>maximum)gear=maximum;
    e.put32(0x208,gear);
}
void manual_transmission(Bytes e,Bytes w,Bytes p,bool inhibited,bool shift_up,bool shift_down){
    if(e.u32(0xe84)!=0)auto_transmission(e,p);
    if(inhibited)return;
    if(shift_up){
        e.put32(0xe84,0);
        const std::uint32_t gear=e.u32(0x208),maximum=p.u32(0x10a0);
        if(gear<maximum){e.put32(0x208,gear+1u);e.put8(0x296,1);}
        return;
    }
    if(!shift_down)return;
    std::uint32_t gear=e.u32(0x208);e.put32(0xe84,0);
    if(gear<=1u)return;
    --gear;e.put32(0x208,gear);
    const float predicted=predicted_engine_speed(e,w,p,gear);
    if(predicted>=p.f32(0x1644) && e.i32(0x38)<128 && e.i32(0x34)>16){
        if(e.i8(0xd36)<12){e.put32(0xd94,180);e.put32(0xd98,60);}
        e.put32(0xd9c,60);e.put8(0xd36,80);e.put8(0x296,0);
    }else{
        e.put8(0x296,3);
    }
}

}
