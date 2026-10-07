#include "platform/vehicle_preview_init.hpp"
#include "platform/embedded_exe_data.hpp"
#include "support/course_world_fixture.hpp"
#include <cstdio>
#include <cstdlib>
using namespace outrun::platform;
using namespace outrun::driving;
static unsigned checks;
#define CHECK(x) do{++checks;if(!(x)){std::fprintf(stderr,"preview init line %d: %s\n",__LINE__,#x);std::exit(1);}}while(0)
int main(){
    DrivingDataPack pack;CHECK(parse_driving_data_pack(EmbeddedDrivingData,EmbeddedDrivingDataSize,pack));
    CHECK(vehicle_preview_start_position(1,11).z==-16.f);
    CHECK(vehicle_preview_start_position(2,5).x==4.f&&vehicle_preview_start_position(2,5).z==-18.25f);
    bool bad_rank=false;try{vehicle_preview_start_position(2,12);}catch(const std::out_of_range&){bad_rank=true;}CHECK(bad_rank);
    struct Rank{unsigned calls{};} rank;
    PcRankProviderServices rank_services{&rank,[](void* u,std::uint8_t)->std::uint32_t{++static_cast<Rank*>(u)->calls;return 0x100u|3u;}};
    for(unsigned model=0;model<30;++model)for(std::uint8_t scene:{0,2})for(std::uint8_t transmission:{0,1}){
        outrun::testing::CourseWorldFixture world;outrun::testing::configure_world_boundary(world,0,0);world.rebuild_grids();
        auto matrices=world.matrix();auto prediction=world.prediction();auto tables=world.tables();
        CourseWorldQuery query{tables,matrices,prediction};
        VehicleCreationQueue queue;vehicle_creation_reset_49fa60(queue);
        CHECK(vehicle_creation_append_49fa80(queue,8,0,model,1,0x4081u|(transmission?0x40u:0u)));
        std::array<std::uint8_t,PcVehicleObjectBytes> event{};std::array<std::uint8_t,PcVehicleBodyBytes> body{};
        std::array<std::uint8_t,DrivingParameterViewBytes> parameters{};std::array<std::uint8_t,76> shared{};
        std::array<std::uint8_t,0x1e0> saved_sun{},sun{};std::array<std::uint8_t,0x54> saved_fog{},fog{};
        std::array<std::uint8_t,24> flags{},nearest{};std::array<std::uint8_t,6*0xa0> lights{};std::array<std::uint8_t,0x110> camera{};
        Bytes e(event.data(),event.size());
        PcEnvironmentFrame frame{};frame.vehicle=e;frame.camera=Bytes(camera.data(),camera.size());
        frame.flags_7d28b0=Bytes(flags.data(),flags.size());frame.nearest_7d2d58=Bytes(nearest.data(),nearest.size());
        frame.lights_899d78=Bytes(lights.data(),lights.size());
        PcEnvironmentBlendContext context{3,0,1,Bytes(saved_sun.data(),saved_sun.size()),Bytes(saved_fog.data(),saved_fog.size()),
            Bytes(sun.data(),sun.size()),Bytes(fog.data(),fog.size()),tables.transforms[0],matrices};
        VehicleParameterSelection selection{0,scene,4,0,0};
        VehiclePreviewInit init{queue,pack,selection,Bytes(shared.data(),shared.size()),Bytes(parameters.data(),parameters.size()),
            frame,tables.courses[0],context,Bytes(body.data(),body.size()),query,{0.f,0.f},transmission,&rank_services,0x1234};
        const auto depth=matrices.depth;VehicleParameterChoice choice;
        CHECK(vehicle_preview_init_4a7270(e,init,&choice));
        CHECK(e.u8(0x11)==model&&e.u32(0x2b4)==choice.pc_address);
        CHECK(e.u8(0x13)==transmission&&e.u32(0x68)==0xe&&e.u32(0x5c)==1&&e.u8(0x66)==3);
        CHECK(e.i16(0x2e)==0x1234&&e.i16(0x17e)==0x1234&&e.i16(0x160)==0x1234);
        CHECK((e.u32(4)&0xa1u)==0xa1u&&(e.u32(4)&2u)==0);
        CHECK(e.f32(0x14)==0&&e.f32(0x1c)==0&&e.f32(0xd28)==0&&e.f32(0x300)==10000.f&&e.u8(0x329)==0xff);
        CHECK(e.f32(0xb68)==((e.u32(4)&0x40u)?0.25f:1.f));
        CHECK(e.u8(0x334)==model&&matrices.depth==depth);
    }
    CHECK(rank.calls==60);
    // The constructor rejects an empty queue; nothing after it runs.
    {
        outrun::testing::CourseWorldFixture world;outrun::testing::configure_world_boundary(world,0,0);world.rebuild_grids();
        auto matrices=world.matrix();auto prediction=world.prediction();auto tables=world.tables();
        CourseWorldQuery query{tables,matrices,prediction};
        VehicleCreationQueue queue;vehicle_creation_reset_49fa60(queue);
        std::array<std::uint8_t,PcVehicleObjectBytes> event{};std::array<std::uint8_t,PcVehicleBodyBytes> body{};body.fill(0x77);
        std::array<std::uint8_t,DrivingParameterViewBytes> parameters{};std::array<std::uint8_t,76> shared{};
        std::array<std::uint8_t,0x1e0> sun{};std::array<std::uint8_t,0x54> fog{};std::array<std::uint8_t,24> flags{},nearest{};
        std::array<std::uint8_t,6*0xa0> lights{};std::array<std::uint8_t,0x110> camera{};
        Bytes e(event.data(),event.size());PcEnvironmentFrame frame{};frame.vehicle=e;frame.camera=Bytes(camera.data(),camera.size());
        frame.flags_7d28b0=Bytes(flags.data(),flags.size());frame.nearest_7d2d58=Bytes(nearest.data(),nearest.size());
        frame.lights_899d78=Bytes(lights.data(),lights.size());
        PcEnvironmentBlendContext context{3,0,1,Bytes(sun.data(),sun.size()),Bytes(fog.data(),fog.size()),
            Bytes(sun.data(),sun.size()),Bytes(fog.data(),fog.size()),tables.transforms[0],matrices};
        VehiclePreviewInit init{queue,pack,{},Bytes(shared.data(),shared.size()),Bytes(parameters.data(),parameters.size()),
            frame,tables.courses[0],context,Bytes(body.data(),body.size()),query,{0.f,0.f},0,nullptr,0};
        const auto before=body;
        CHECK(!vehicle_preview_init_4a7270(e,init)&&body==before);
    }
    std::printf("vehicle preview init 4A7270: %u checks\n",checks);
}
