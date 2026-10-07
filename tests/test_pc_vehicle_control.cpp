#include "platform/pc_vehicle_control.hpp"
#include "driving/pc_wheel_dynamics.hpp"
#include <cstring>
#include <cmath>
#include <iostream>
#include <stdexcept>

using namespace outrun;
namespace {
unsigned checks{};
void require(bool value,const char* message){
    ++checks;
    if(!value)throw std::runtime_error(message);
}
void put32(std::vector<std::uint8_t>& bytes,std::size_t offset,std::uint32_t value){
    for(unsigned i=0;i<4u;++i)bytes[offset+i]=std::uint8_t(value>>(i*8u));
}
void putf(std::vector<std::uint8_t>& bytes,std::size_t offset,float value){
    std::uint32_t bits{};std::memcpy(&bits,&value,4);put32(bytes,offset,bits);
}
platform::DrivingDataPack retail_fixture(float torque_sample=30.0f){
    platform::DrivingDataPack pack{};
    pack.parameter_arena.resize(platform::DrivingParameterArenaBytes);
    pack.selection_maps[0][platform::DrivingPackV1LegacyCarId]=
        platform::DrivingPackV1LegacyColumn;
    const auto column=std::size_t(platform::DrivingPackV1LegacyColumn)*4u;
    const auto paramf=[&](std::size_t offset,float value){
        putf(pack.parameter_arena,column+offset,value);
    };
    const auto param32=[&](std::size_t offset,std::uint32_t value){
        put32(pack.parameter_arena,column+offset,value);
    };
    // Exact player-car rigid-body setup inputs used by PC 0x4A6840.
    paramf(0x000u,1.07f);paramf(0x04cu,1.37f);paramf(0x098u,0.43f);paramf(0x0e4u,2.58f);
    paramf(0x130u,2.58f);paramf(0x17cu,1.62f);paramf(0x18a4u,0.66f);
    paramf(0x18f0u,3.2f);paramf(0x193cu,5.8f);
    paramf(0xb94u,0.3291f);paramf(0x134cu,2.4375f);paramf(0x10ecu,1.0f);
    param32(0x10a0u,6u);paramf(0x1514u,12.0f);paramf(0x1560u,0.085f);
    paramf(0x15acu,0.069f);paramf(0x15f8u,157.07964f);paramf(0x1644u,890.1179f);
    paramf(0x1690u,1308.9969f);paramf(0x1728u,1.1f);paramf(0x1774u,250.0f);
    for(std::uint32_t gear=0;gear<=7u;++gear)
        paramf((gear+0x3au)*0x4cu,3.0f/(float(gear)+1.0f));
    for(auto& table:pack.torque_tables){
        table.resize(platform::DrivingTorqueTableBytes);
        putf(table,0u,0.1273239553f);putf(table,4u,4.5f);
        for(unsigned sample=0;sample<160u;++sample)putf(table,8u+sample*4u,torque_sample);
    }
    pack.brake_table.resize(platform::DrivingBrakeTableBytes);
    for(unsigned pedal=0;pedal<256u;++pedal)
        putf(pack.brake_table,pedal*4u,float(pedal)/255.0f);
    return pack;
}
platform::VehicleRoadContactFrame flat_contacts(){
    platform::VehicleRoadContactFrame contacts{};
    contacts.supported=true;contacts.hit_count=4u;contacts.hit_mask=15u;
    contacts.normal={{0.0f,1.0f,0.0f}};
    for(auto& wheel:contacts.wheels){
        wheel.supported=true;wheel.normal=contacts.normal;wheel.material=0u;
    }
    return contacts;
}
}

int main(int argc,char** argv){
try{
    platform::PcVehicleControlState state{};
    platform::initialize_pc_vehicle_control(state);
    driving::Bytes e(state.event.data(),state.event.size());
    driving::Bytes w(state.work.data(),state.work.size());
    require(state.initialized&&e.u32(0x208)==1u,"initial manual first gear");
    require(e.u32(0x2b4)==0xdead0001u,"guest parameter pointer remains canary");

    auto out=platform::step_pc_vehicle_control(state,{127,200u,17u,false,false});
    require(e.i32(0x34)==200&&e.i32(0x38)==17&&
            std::uint16_t(e.i16(0x202))==0x7f00u,"PC OperationInput owns analogue fields");
    require(out.steering_angle!=0&&out.wheels[0].direction==out.steering_angle&&
            out.wheels[1].direction==out.steering_angle,"PC steering feeds both front wheels");
    require(out.steering_full_lock>0u&&
            std::fabs(platform::normalized_pc_vehicle_steering(out)-1.0f)<0.001f,
            "active PC parameter view calibrates full-lock steering");
    require(platform::pc_vehicle_yaw_step(out)>0.0f,
            "full right lock produces a positive bounded yaw step");
    require(out.wheels[2].direction==0&&out.wheels[3].direction==0,
            "rear wheels retain zero toe direction");
    const auto right=out.steering_angle;
    out=platform::step_pc_vehicle_control(state,{-128,0u,0u,false,false});
    require((right<0&&out.steering_angle>0)||(right>0&&out.steering_angle<0),
            "opposite input produces opposite PC steering");

    out=platform::step_pc_vehicle_control(state,{0,0u,0u,true,false});
    require(out.gear==2u&&state.shift_up_requests==1u&&state.gear_changes==1u,
            "PC manual upshift commits second gear");
    out=platform::step_pc_vehicle_control(state,{0,0u,0u,false,true});
    require(out.gear==1u&&state.shift_down_requests==1u&&state.gear_changes==2u,
            "PC manual downshift returns to first");

    const auto initial_spin=out.wheels[0].spin;
    for(unsigned frame=0;frame<240u;++frame)
        out=platform::step_pc_vehicle_control(state,{0,255u,0u,false,false});
    require(out.linear_speed>0.0f&&out.linear_speed<=0.150001f,
            "bounded first-gear translation accelerates and caps");
    require(out.wheels[0].spin!=initial_spin,"PC inline wheel angle advances");
    require(out.wheels[0].spin==out.wheels[1].spin&&
            out.wheels[0].spin==out.wheels[2].spin&&
            out.wheels[0].spin==out.wheels[3].spin,"four wheel records stay synchronized");

    for(unsigned frame=0;frame<40u;++frame)
        out=platform::step_pc_vehicle_control(state,{0,0u,255u,false,false});
    require(out.linear_speed==0.0f,"brake stops without fabricating reverse");
    platform::reject_pc_vehicle_motion(state);
    out=platform::pc_vehicle_control_output(state);
    require(out.linear_speed==0.0f&&state.rejected_motion==1u,
            "collision rejection halts bridge state");
    require(state.frames==284u&&state.operation_input_calls==state.frames&&
            state.steering_calls==state.frames&&state.transmission_calls==state.frames,
            "all PC control stages run once per bridge frame");
    require(state.wheel_angle_updates==state.frames*4u,
            "all wheel angle records update once per bridge frame");
    require(e.u32(0x2b4)==0xdead0001u,"PC routines never dereference guest canary");
    for(auto wheel:driving::embedded_wheels(w))
        require(std::isfinite(wheel.f32(0xd8)),"wheel angular velocity remains finite");

    auto pack=retail_fixture();platform::PcVehicleControlState retail{};
    require(platform::configure_pc_vehicle_control(retail,pack,5,0),
            "legacy column data configures arithmetic bridge");
    platform::PcVehicleControlState retail_turn{};
    require(platform::configure_pc_vehicle_control(retail_turn,pack,5,0),
            "retail steering calibration configures");
    const auto retail_full_right=platform::step_pc_vehicle_control(
        retail_turn,{127,0u,0u,false,false});
    require(retail_full_right.steering_full_lock>900u&&
            retail_full_right.steering_full_lock<1400u,
            "legacy column full lock is measured in its PC angle domain");
    require(std::fabs(platform::normalized_pc_vehicle_steering(retail_full_right)-1.0f)<0.001f&&
            platform::pc_vehicle_yaw_step(retail_full_right)>0.0020f,
            "retail full lock preserves world turning authority");
    auto retail_out=platform::step_pc_vehicle_control(retail,{0,255u,0u,false,false});
    require(retail.retail_driving_data&&retail.retail_car_id==5u&&
            retail.retail_parameter_column==6u,"retail selector provenance retained");
    require(retail.engine_torque_calls==1u&&retail.brake_pressure_calls==1u,
            "exact torque and brake functions run once per frame");
    require(std::isfinite(retail_out.engine_rpm)&&retail_out.engine_rpm>0.0f&&
            std::isfinite(retail_out.drive_torque)&&retail_out.drive_torque>0.0f,
            "retail engine state is finite and produces torque");
    require(retail_out.brake_pressure==0.0f&&retail_out.linear_speed>0.0f,
            "retail torque accelerates through translation boundary");
    const float initial_retail_torque=retail_out.drive_torque;
    retail_out=platform::step_pc_vehicle_control(retail,{0,0u,255u,false,false});
    require(retail_out.brake_pressure==1.0f,"retail brake curve reaches full pressure");
    auto stronger_pack=retail_fixture(60.0f);platform::PcVehicleControlState stronger{};
    require(platform::configure_pc_vehicle_control(stronger,stronger_pack,5,0),
            "second retail fixture configures");
    const auto stronger_out=platform::step_pc_vehicle_control(
        stronger,{0,255u,0u,false,false});
    require(stronger_out.drive_torque>initial_retail_torque,
            "loaded torque curve changes recovered torque output");

    if(argc==2){
        platform::DrivingDataPack actual{};std::string error;
        require(platform::load_driving_data_pack_file(argv[1],actual,&error),
                "generated owned retail pack loads");
        platform::PcVehicleControlState actual_state{};
        require(platform::configure_pc_vehicle_control(actual_state,actual,5,0),
                "generated owned legacy column data configures");
        require(actual_state.steering_full_lock>900u&&actual_state.steering_full_lock<1400u,
                "generated owned legacy column full-lock calibration is plausible");
        platform::PcVehicleControlOutput actual_out{};
        for(unsigned frame=0;frame<300u;++frame)
            actual_out=platform::step_pc_vehicle_control(
                actual_state,{32,255u,0u,false,false});
        require(actual_out.linear_speed>0.0f&&std::isfinite(actual_out.engine_rpm)&&
                std::isfinite(actual_out.drive_torque),
                "exact retail legacy column data drives 300 stable frames");
        for(unsigned frame=0;frame<80u;++frame)
            actual_out=platform::step_pc_vehicle_control(
                actual_state,{0,0u,255u,false,false});
        require(actual_out.linear_speed==0.0f&&actual_out.brake_pressure==1.0f,
                "exact retail brake curve stops without reverse");
        require(actual_state.engine_torque_calls==380u&&
                actual_state.brake_pressure_calls==380u,
                "exact retail functions cover every integration frame");
        platform::PcVehicleControlState integrated{};
        require(platform::configure_pc_vehicle_control(integrated,actual,5,0),
                "actual data configures full contact chain");
        platform::set_pc_vehicle_road_contacts(integrated,flat_contacts());
        platform::PcVehicleControlOutput integrated_out{};
        float integrated_peak_rpm=0.0f;
        constexpr unsigned IntegratedFrames=5000u;
        for(unsigned frame=0;frame<IntegratedFrames;++frame)
        {
            integrated_out=platform::step_pc_vehicle_control(
                integrated,{((frame/300u)&1u)?64:-64,
                            (frame%1000u)<800u?220u:0u,
                            (frame%1000u)>=800u?180u:0u,
                            frame==900u||frame==1800u||frame==2700u,
                            frame==3600u});
            integrated_peak_rpm=std::max(integrated_peak_rpm,integrated_out.engine_rpm);
        }
        require(integrated_out.full_driving_control&&
                integrated.driving_control_calls==IntegratedFrames&&
                integrated.road_mu_calls==IntegratedFrames&&
                integrated.suspension_force_calls==IntegratedFrames&&
                integrated.tire_load_calls==IntegratedFrames,
                "contacts drive suspension, tire load and complete DrivingControl each frame");
        require(integrated.road_contact_frames==IntegratedFrames&&
                integrated.road_contact_wheel_hits==IntegratedFrames*4u&&
                integrated.wheel_angle_updates==IntegratedFrames*4u,
                "four live contacts and wheel rotations cover every integrated frame");
        for(unsigned wheel=0;wheel<4u;++wheel)
            require(std::isfinite(integrated_out.suspension_forces[wheel])&&
                    std::isfinite(integrated_out.tire_loads[wheel])&&
                    std::isfinite(integrated_out.road_mu[wheel])&&
                    integrated_out.tire_loads[wheel]>=0.0f,
                    "integrated suspension force and tire load remain finite");
        std::cout<<"retail legacy column rpm="<<actual_out.engine_rpm
                 <<" torque="<<actual_out.drive_torque
                 <<" brake="<<actual_out.brake_pressure
                 <<" steering_full_lock="<<actual_state.steering_full_lock
                 <<" integrated_peak_rpm="<<integrated_peak_rpm*9.5492965855f
                 <<" integrated_final_rpm="<<integrated_out.engine_rpm*9.5492965855f
                 <<" integrated_speed="<<integrated_out.linear_speed<<"\n";
    }else if(argc!=1)throw std::runtime_error("usage: test_pc_vehicle_control [OR2DRV1]");

    std::cout<<"pc vehicle control bridge: "<<checks<<" checks passed\n";
    return 0;
}catch(const std::exception& ex){
    std::cerr<<"FAIL after "<<checks<<" checks: "<<ex.what()<<"\n";
    return 1;
}
}
