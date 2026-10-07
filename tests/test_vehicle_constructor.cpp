#include "platform/vehicle_constructor.hpp"
#include "platform/vehicle_constructor_data.hpp"
#include "platform/embedded_exe_data.hpp"
#include <cstdio>
#include <cstdlib>
#include <cstring>
using namespace outrun::platform;
using outrun::driving::Bytes;
static unsigned checks;
#define CHECK(x) do{++checks;if(!(x)){std::fprintf(stderr,"constructor line %d: %s\n",__LINE__,#x);std::exit(1);}}while(0)
int main(){
    DrivingDataPack pack;CHECK(parse_driving_data_pack(EmbeddedDrivingData,EmbeddedDrivingDataSize,pack));
    std::array<std::uint8_t,PcVehicleObjectBytes+16> object{};Bytes e(object.data(),object.size());
    std::array<std::uint8_t,64> shared{};Bytes scratch(shared.data(),shared.size());
    VehicleCreationQueue queue;VehicleParameterSelection selection;VehicleParameterChoice choice;
    for(unsigned model=0;model<30;++model)for(unsigned variant:{0u,1u,2u,255u})for(unsigned flag:{0u,1u,255u}){
        selection.variant_83036d=std::uint8_t(variant);selection.flag_8514a0=flag;
        CHECK(vehicle_creation_reset_49fa60(queue));
        for(unsigned i=0;i<16;++i)CHECK(vehicle_creation_append_49fa80(queue,8+i,i%8,model,1+i%8,0x4081|(i%2)*0x40));
        for(unsigned i=0;i<16;++i){
            object.fill(0xa5);shared.fill(0xa5);
            CHECK(vehicle_construct_4a5830(e,queue,pack,selection,scratch,&choice));
            CHECK(e.u32(0)==8+i&&e.u8(0x10)==i%8&&e.u8(0x11)==model&&e.u8(0x12)==1+i%8);
            CHECK(e.u32(4)==(0x4081|(i%2)*0x40)&&queue.object[0x108]==i+1&&queue.object[0x109]==16);
            CHECK(choice.base_model==model%15&&choice.map==((model>=15||variant==1)?(flag?3:2):(flag?0:1)));
            CHECK(e.u32(0x2b4)==0x5e3140+4*pack.selection_maps[choice.map][model%15]);
            for(unsigned k=0;k<12;++k)CHECK(e.u32(0x130+k*4)==VehicleConstructorData[model][k]);
            CHECK(e.u32(PcVehicleObjectBytes)==0xa5a5a5a5&&scratch.u32(60)==0xa5a5a5a5);
            for(unsigned k=0;k<60;k+=4)CHECK(scratch.u32(k)==0);
        }
        const auto before=object;const auto qbefore=queue.object;
        CHECK(!vehicle_construct_4a5830(e,queue,pack,selection,scratch)&&queue.fault==0x4a5830);
        CHECK(object==before&&queue.object==qbefore);
        queue.fault=0;
    }
    // The historical prototype's model5/column6 is NOT model9 (250 GTO).
    selection={};selection.flag_8514a0=1;
    CHECK(vehicle_parameter_choice_5051d0(pack,9,selection,choice)&&choice.column==9&&choice.map==0);
    CHECK(vehicle_parameter_choice_5051d0(pack,24,selection,choice)&&choice.column==17&&choice.map==3);
    selection.flag_8514a0=0;
    CHECK(vehicle_parameter_choice_5051d0(pack,9,selection,choice)&&choice.column==12&&choice.map==1);
    CHECK(vehicle_parameter_choice_5051d0(pack,24,selection,choice)&&choice.column==7&&choice.map==2);
    CHECK(!vehicle_parameter_choice_5051d0(pack,30,selection,choice));
    CHECK(!vehicle_parameter_choice_5051d0(pack,255,selection,choice));
    CHECK(vehicle_parameter_choice_5051d0(pack,0x109,selection,choice)&&choice.model==9);
    auto fault_case=[&](unsigned read,unsigned count,unsigned model,unsigned event_size,unsigned scratch_size,unsigned expected){
        queue={};vehicle_creation_append_49fa80(queue,8,0,model,1,0x4081);queue.object[0x108]=std::uint8_t(read);queue.object[0x109]=std::uint8_t(count);
        object.fill(0xcd);shared.fill(0xab);const auto before=object;const auto qbefore=queue.object;const auto sbefore=shared;
        CHECK(!vehicle_construct_4a5830(Bytes(object.data(),event_size),queue,pack,selection,Bytes(shared.data(),scratch_size)));
        CHECK(queue.fault==expected&&queue.object==qbefore&&object==before&&shared==sbefore);
        queue.object[0x108]=0;queue.object[0x109]=1;
        CHECK(!vehicle_construct_4a5830(e,queue,pack,selection,scratch)); // fault is latched
    };
    fault_case(0,0,9,PcVehicleObjectBytes,60,0x4a5830);
    fault_case(16,16,9,PcVehicleObjectBytes,60,0x4a5830);
    fault_case(255,1,9,PcVehicleObjectBytes,60,0x4a5830);
    fault_case(0,17,9,PcVehicleObjectBytes,60,0x4a5830);
    fault_case(0,1,30,PcVehicleObjectBytes,60,0x5051d0);
    fault_case(0,1,9,0x1000,60,0x4a5830);
    fault_case(0,1,9,PcVehicleObjectBytes,59,0x4a5830);
    pack.selection_maps[1][9]=19;fault_case(0,1,9,PcVehicleObjectBytes,60,0x5051d0);
    pack.selection_maps[1][9]=12;
    Bytes arena(pack.parameter_arena.data(),pack.parameter_arena.size());arena.put32(12*4+0x10a0,7);
    fault_case(0,1,9,PcVehicleObjectBytes,60,0x487570);arena.put32(12*4+0x10a0,6);
    arena.put32(12*4+0x15f8,0x7fc00000);fault_case(0,1,9,PcVehicleObjectBytes,60,0x4a5830);
    std::printf("vehicle constructor: %u checks\n",checks);
}
