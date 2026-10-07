#include "vehicle_constructor.hpp"
#include "vehicle_constructor_data.hpp"
#include "driving/pc_x87.hpp"
#include <cstring>
#include <cmath>
#include <limits>
namespace outrun::platform {
using driving::Bytes;
namespace {
float bits(unsigned value){float f;std::memcpy(&f,&value,4);return f;}
}
bool vehicle_parameter_choice_5051d0(const DrivingDataPack& pack,unsigned model,
    const VehicleParameterSelection& s,VehicleParameterChoice& result){
    // The PC consumes a signed byte. Only the thirty real model IDs are valid.
    model=std::uint8_t(model);
    if(model>=30||pack.parameter_arena.size()!=DrivingParameterArenaBytes)return false;
    const bool tuned=model>=15||s.variant_83036d==1;
    const bool alternate=s.game_mode_780258==4?s.flag_65a7ac!=1:s.flag_8514a0!=0;
    // Both sides of the loading-scene branch in the PC select the same maps.
    const unsigned map=tuned?(alternate?3:2):(alternate?0:1),base=model%15;
    const unsigned column=pack.selection_maps[map][base];
    if(column>=DrivingParameterColumns)return false;
    result={model,base,map,column,0x5e3140+4*column};return true;
}
bool vehicle_clear_4874f0(Bytes e,bool clear){
    if(e.size()<PcVehicleObjectBytes)return false;
    if(clear)for(unsigned o=0;o<PcVehicleObjectBytes;o+=4)e.put32(o,0);
    e.put32(0xc60,0x19a);e.put32(0xda0,0x19a);e.put32(0x300,0x461c4000);
    e.put32(4,(e.u32(4)&0xfeffffff)|0x02000000);e.put8(0x13,0);e.put8(0x329,255);
    for(unsigned o:{0xdbcu,0xdc0u,0xdc4u})e.putf(o,1.0f);
    return true;
}
bool vehicle_gear_thresholds_487570(Bytes e,Bytes p){
    if(e.size()<PcVehicleObjectBytes||p.size()<DrivingParameterViewBytes)return false;
    const unsigned gears=p.u32(0x10a0);
    // Original arrays hold seven values. Reject corrupt data, never truncate it.
    if(gears>6)return false;
    float divisor=p.f32(0x134c)*p.f32(0x10ec);
    divisor=divisor*bits(0x4270cccd);
    const float ratio=p.f32(0xb94)/divisor;
    for(unsigned o:{0xe10u,0xe2cu,0xe30u,0xe48u,0xe4cu})e.put32(o,0xc61c3c00);
    for(unsigned o:{0xe24u,0xe28u,0xe44u,0xe60u})e.put32(o,0x461c3c00);
    for(unsigned gear=1;gear<gears;++gear){
        float value=p.f32(0x14c8)/p.f32(0x1184+(gear-1)*0x4c);
        e.putf(0xe10+4*gear,value*ratio);
    }
    for(unsigned gear=2;gear<=gears;++gear){
        const unsigned source=gear==2?0x1398:gear==gears?0x1430:0x13e4;
        float value=p.f32(source)/p.f32((gear+0x39)*0x4c);
        e.putf(0xe2c+4*gear,value*ratio);
        value=p.f32(gear==2?0x1398:0x147c)/p.f32((gear+0x39)*0x4c);
        e.putf(0xe48+4*gear,value*ratio);
    }
    float value=p.f32(0x1644)/p.f32(0x1300);e.putf(0xe64,value*ratio);return true;
}
bool vehicle_reset_timer_455a90(Bytes e){
    if(e.size()<0xdbc)return false;
    e.put8(0xdb1,0);e.put16(0xdb8,0);e.put16(0xdba,0x708);return true;
}
bool vehicle_construct_4a5830(Bytes e,VehicleCreationQueue& queue,const DrivingDataPack& pack,
    const VehicleParameterSelection& selection,Bytes shared,VehicleParameterChoice* result){
    if(queue.fault)return false;
    auto fail=[&](unsigned pc){queue.fault=pc;return false;};
    if(e.size()<PcVehicleObjectBytes||shared.size()<60)return fail(0x4a5830);
    Bytes q(queue.object.data(),queue.object.size());const int index=q.i8(0x108),count=q.i8(0x109);
    if(index<0||index>=16||count<0||count>16||index>=count)return fail(0x4a5830);
    const unsigned offset=unsigned(index)*16,model=q.u8(offset+8);
    VehicleParameterChoice choice;
    if(!vehicle_parameter_choice_5051d0(pack,model,selection,choice))return fail(0x5051d0);
    // A native copy resolves the semantic PC pointer without narrowing a host address.
    std::array<std::uint8_t,DrivingParameterViewBytes> storage{};
    if(!copy_driving_parameter_view(pack,choice.map,choice.base_model,storage.data(),storage.size()))return fail(0x5051d0);
    Bytes p(storage.data(),storage.size());
    if(p.u32(0x10a0)>6)return fail(0x487570);
    // 4A58C2: fld; fmul 5C370C (rounds to the x87 precision control); _ftol2.
    const driving::X87 rpm_x87=driving::X87(p.f32(0x15f8))*driving::X87(bits(0x4118c9eb));
    const double rpm_value=double(rpm_x87.v);
    if(!std::isfinite(rpm_value)||rpm_value<double(std::numeric_limits<std::int32_t>::min())||
        rpm_value>double(std::numeric_limits<std::int32_t>::max()))return fail(0x4a5830);
    vehicle_clear_4874f0(e,true);
    e.put32(0,q.u32(offset));e.put32(4,q.u32(offset+4));e.put8(0x10,q.u8(offset+12));
    q.put8(0x108,std::uint8_t(index+1));e.put8(0x11,model);e.put8(0x12,q.u8(offset+13));
    e.put32(0x2b4,choice.pc_address);vehicle_gear_thresholds_487570(e,p);vehicle_reset_timer_455a90(e);
    for(unsigned o=0;o<60;o+=4)shared.put32(o,0);
    e.put32(0x208,1);e.put32(0x21c,p.u32(0x15f8));
    // _ftol2 truncates the x87 register product (rounded to the precision
    // control, not to a binary32 store).
    const auto rpm=std::int32_t(rpm_value);
    for(unsigned o:{0x20cu,0x210u,0x48u})e.puti(o,rpm);
    const auto& data=VehicleConstructorData[model];
    for(unsigned i=0;i<12;++i)e.put32(0x130+4*i,data[i]);
    e.put32(0x2d0,0x7f7fffff);e.put32(0x2f0,(e.u32(0x2f0)&0xfffffc04)|4);
    e.put32(0x2f4,0x7fffffff);e.putf(0x30c,bits(data[15])*0.5f);
    for(unsigned i=0;i<3;++i)e.put32(0x31c+4*i,data[12+i]);
    for(unsigned o:{0x58u,0xe9cu,0xe0cu})e.putf(o,1.0f);
    e.put32(0xe98,0x7f7fffff);e.put32(0xae8,0x19a);e.put32(0xdd4,0x19a);e.put32(0x1d8,1);
    // Remaining explicit zero writes in 4A5830 are already zero from 4874F0;
    // none overlap the gear thresholds, selected fields or timer above.
    if(result)*result=choice;
    return true;
}
}
