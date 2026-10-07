#include "platform/vehicle_body_init.hpp"
#include "platform/embedded_exe_data.hpp"
#include "platform/pc_vehicle_control.hpp"
#include "platform/vehicle_preview_event.hpp"
#include "platform/frontend_profiles.hpp"
#include "support/course_world_fixture.hpp"
#include <cstdio>
#include <cstdlib>
#include <cmath>
using namespace outrun::platform;
using namespace outrun::driving;
static unsigned checks;
#define CHECK(x) do{++checks;if(!(x)){std::fprintf(stderr,"body init line %d: %s\n",__LINE__,#x);std::exit(1);}}while(0)
int main(){
    DrivingDataPack pack;CHECK(parse_driving_data_pack(EmbeddedDrivingData,EmbeddedDrivingDataSize,pack));
    PcVehicleControlState control;CHECK(control.work.size()>=PcVehicleBodyBytes);
    std::array<std::uint8_t,PcVehicleObjectBytes+16> object{};
    std::array<std::uint8_t,PcVehicleBodyBytes+16> body{};
    std::array<std::uint8_t,DrivingParameterViewBytes> parameters{};
    std::array<std::uint8_t,60> history{};
    Bytes e(object.data(),object.size()),w(body.data(),body.size()),p(parameters.data(),parameters.size());
    // Connect the real queue consumer, real parameter selection and physical
    // initializer. Only the course geometry is synthetic in this unit test.
    for(unsigned model=0;model<30;++model)for(unsigned variant:{0u,1u})for(unsigned alternate:{0u,1u}){
        VehicleCreationQueue queue;VehicleParameterSelection selection;VehicleParameterChoice choice;
        selection.variant_83036d=std::uint8_t(variant);selection.flag_8514a0=alternate;
        object.fill(0xa5);body.fill(0xcd);
        CHECK(vehicle_creation_append_49fa80(queue,8,0,model,1+model%8,0x4081));
        CHECK(vehicle_construct_4a5830(e,queue,pack,selection,Bytes(history.data(),history.size()),&choice));
        CHECK(copy_driving_parameter_view(pack,choice.map,choice.base_model,parameters.data(),parameters.size()));
        outrun::testing::CourseWorldFixture world;
        outrun::testing::configure_world_boundary(world,alternate?4:0,0);world.rebuild_grids();
        auto matrices=world.matrix();auto prediction=world.prediction();auto tables=world.tables();
        CourseWorldQuery query{tables,matrices,prediction};
        const auto old_offset=matrices.current_offset;const auto old_depth=matrices.depth;
        std::array<std::uint8_t,64> old_matrix{};
        for(unsigned i=0;i<64;++i)old_matrix[i]=matrices.current().u8(i);
        e.putf(0x14,5);e.putf(0x18,123);e.putf(0x1c,5);e.putf(0x1c4,0.125f);
        vehicle_body_init_4a69f0(e,w,p,0x82e7f0,query);
        CHECK(e.u8(0x11)==model&&e.u8(0x12)==1+model%8&&e.u32(0x2b4)==choice.pc_address);
        CHECK(e.f32(0x18)==(alternate?-0.1f:3.f));
        CHECK(w.f32(0x40)==5&&w.f32(0x44)==e.f32(0x18)&&w.f32(0x48)==5);
        CHECK(w.f32(0x98)==p.f32(0)&&w.f32(0x9c)==1.f/p.f32(0));
        for(unsigned i=0;i<3;++i)CHECK(w.f32(0xac+4*i)==(1.f/p.f32(0))/p.f32(0x4c*(i+1)));
        CHECK(w.f32(0x678)==std::fabs(p.f32(0x1ab8))&&w.f32(0x674)==-std::fabs(p.f32(0x1ab8)));
        CHECK(w.u32(0x68c)==4);
        for(unsigned i=0;i<4;++i){
            const auto off=embedded_wheel_offsets[i];
            CHECK(w.u32(0x248+4*i)==0x82e7f0+off);
            CHECK(w.f32(off+0xe4)==1&&w.f32(off+0x48)==-1);
            CHECK(w.f32(off+0xd4)==e.f32(0x1c4)*60.2f);
            CHECK(w.f32(off+0x24)==w.f32(off+0x34)&&w.f32(off+0x24)==w.f32(off+0x38));
        }
        for(unsigned i=0x800;i<PcVehicleBodyBytes;++i)CHECK(w.u8(i)==0);
        for(unsigned i=0;i<16;++i){CHECK(w.u8(PcVehicleBodyBytes+i)==0xcd);CHECK(e.u8(PcVehicleObjectBytes+i)==0xa5);}
        CHECK(matrices.current_offset==old_offset&&matrices.depth==old_depth);
        for(unsigned i=0;i<64;++i)CHECK(matrices.current().u8(i)==old_matrix[i]);
        // Invalid view/address errors must occur before clearing either object.
        auto invalid=[&](Bytes event_view,Bytes body_view,Bytes param_view,unsigned address){
            const auto before_e=object;const auto before_w=body;bool rejected=false;
            try{vehicle_body_init_4a69f0(event_view,body_view,param_view,address,query);}
            catch(const std::out_of_range&){rejected=true;}
            CHECK(rejected&&object==before_e&&body==before_w);
        };
        invalid(e,Bytes(body.data(),0x800),p,0x82e7f0);
        invalid(Bytes(object.data(),0x1000),w,p,0x82e7f0);
        invalid(e,w,Bytes(parameters.data(),parameters.size()-1),0x82e7f0);
        invalid(e,w,p,0xfffffff0);
        const auto prior_height=e.f32(0x18);
        vehicle_ground_init_519300(e,w,query,{0.25f,-0.5f});
        CHECK(e.f32(0x18)==prior_height&&w.f32(0x644)==prior_height);
        CHECK(w.f32(0x628)==0&&w.f32(0x62c)==1&&w.f32(0x630)==0);
        // Preserve the PC's unusual X base on all three offset components.
        CHECK(w.f32(0x634)==5&&w.f32(0x638)==5.01f&&w.f32(0x63c)==5);
        CHECK(e.i16(0x25c)==0&&e.i16(0x64)==0);
        CHECK(e.i16(0x294)==std::int16_t((alternate?-0.5f:0.25f)*10430.3779296875f));
        vehicle_collision_init_4f6e40(e);
        CHECK(e.u8(0x334)==model&&e.u8(0x335)==1);
        CHECK(e.u32(0x378)==0xffffffff&&e.u32(0x44c)==0xffffffff);
        std::uint16_t yaw=65500;unsigned seed=12345;Bytes h(history.data(),history.size());
        std::array<std::uint8_t,32> owner{};Bytes o(owner.data(),owner.size());o.put32(0x1c,3);
        VehiclePreviewControl preview{{3,2,1},yaw,seed,h,matrices,0,2,o,0.5f,0,3,0,0};
        for(unsigned frame=0;frame<128;++frame){
            e.put32(0x1f4,frame);auto expected_seed=seed;
            if(frame>100)frontend_crt_random_580f40(expected_seed);
            const auto old_yaw=e.i16(0x2e),requested=std::int16_t(yaw);
            vehicle_preview_control_4a5b20(e,preview);
            CHECK(e.i16(0x17e)==old_yaw&&e.i16(0x2e)==requested);
            CHECK(yaw==std::uint16_t(requested+100)&&seed==expected_seed);
            CHECK(e.f32(0x14)==3&&e.f32(0x18)==2&&e.f32(0x1c)==1);
            CHECK(e.f32(0x16c)==3&&e.f32(0x170)==2&&e.f32(0x174)==1);
            CHECK(e.u8(0x11)==model&&e.u32(0x2b4)==choice.pc_address);
            CHECK(matrices.current_offset==old_offset&&matrices.depth==old_depth);
        }
        // An unresolved owner used by interpolation must not manufacture ready.
        preview.owner_799ca0=Bytes(nullptr,0);const auto before=object;const auto seed_before=seed;const auto yaw_before=yaw;
        bool rejected=false;try{vehicle_preview_control_4a5b20(e,preview);}catch(const std::out_of_range&){rejected=true;}
        CHECK(rejected&&object==before&&seed==seed_before&&yaw==yaw_before);
    }
    std::printf("vehicle body init: %u checks\n",checks);
}
