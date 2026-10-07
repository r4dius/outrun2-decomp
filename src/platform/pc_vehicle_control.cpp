#include "platform/pc_vehicle_control.hpp"
#include "driving/pc_chassis.hpp"
#include "driving/pc_common_control.hpp"
#include "driving/pc_driving_control.hpp"
#include "driving/pc_matrix_stack.hpp"
#include "driving/pc_suspension.hpp"
#include "driving/pc_steering.hpp"
#include "driving/pc_transmission.hpp"
#include "driving/pc_wheel_dynamics.hpp"
#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <string>

namespace outrun::platform {
namespace {

driving::Bytes event_view(PcVehicleControlState& s){
    return {s.event.data(),s.event.size()};
}
driving::Bytes work_view(PcVehicleControlState& s){
    return {s.work.data(),s.work.size()};
}
driving::Bytes parameter_view(PcVehicleControlState& s){
    return {s.parameters.data(),s.parameters.size()};
}
driving::Tables table_views(PcVehicleControlState& s){
    return {{driving::Bytes(s.torque_tables[0].data(),s.torque_tables[0].size()),
             driving::Bytes(s.torque_tables[1].data(),s.torque_tables[1].size())},
            driving::Bytes(s.brake_table.data(),s.brake_table.size())};
}

driving::PcMatrixStack matrix_stack(PcVehicleControlState& s){
    return {driving::Bytes(s.matrix_stack_storage.data(),s.matrix_stack_storage.size()),0,0,8};
}

void put_identity(driving::Bytes bytes,std::size_t offset){
    for(unsigned column=0;column<4u;++column)for(unsigned row=0;row<4u;++row)
        bytes.putf(offset+(column*4u+row)*4u,column==row?1.0f:0.0f);
}

void initialize_physics_views(PcVehicleControlState& s){
    auto w=work_view(s),p=parameter_view(s);
    put_identity(w,0x10u);put_identity(w,0x1e0u);
    // Exact PC RIGID_BODY construction / player-car body setup.  0x516E10
    // initializes an enabled zeroed body with identity matrices and 1.0
    // placeholders.  Player-car setup 0x4A6840 then replaces mass/inertias
    // from the selected car parameter column and computes their reciprocals.
    w.put32(0x00u,1u);
    for(const auto off:{0x50u,0x5cu,0x68u,0x74u,0x80u,0x8cu,0xc0u,0xccu,0xd8u})
        for(unsigned k=0;k<3u;++k)w.putf(off+k*4u,0.0f);
    const float body_mass=p.f32(0x000u);
    const float inertia_x=body_mass*p.f32(0x04cu);
    const float inertia_y=body_mass*p.f32(0x098u);
    const float inertia_z=body_mass*p.f32(0x0e4u);
    if(!(std::isfinite(body_mass)&&body_mass>0.0f&&
         std::isfinite(inertia_x)&&inertia_x>0.0f&&
         std::isfinite(inertia_y)&&inertia_y>0.0f&&
         std::isfinite(inertia_z)&&inertia_z>0.0f))
        throw std::runtime_error("invalid retail rigid-body mass/inertia");
    w.putf(0x98u,body_mass);w.putf(0x9cu,1.0f/body_mass);
    w.putf(0xa0u,inertia_x);w.putf(0xa4u,inertia_y);w.putf(0xa8u,inertia_z);
    w.putf(0xacu,1.0f/inertia_x);w.putf(0xb0u,1.0f/inertia_y);w.putf(0xb4u,1.0f/inertia_z);
    w.putf(0xb8u,0.0f);w.putf(0xbcu,0.0f);
    w.putf(0x628u,0.0f);w.putf(0x62cu,1.0f);w.putf(0x630u,0.0f);
    const float total_load=p.f32(0u);
    const float reference_load=std::isfinite(total_load)&&total_load>0.0f?
        total_load*0.25f:1000.0f;
    const auto wheels=driving::embedded_wheels(w);
    for(std::size_t i=0;i<wheels.size();++i){
        auto wheel=wheels[i];const auto& local=VehicleVisualObjectTranslations[i+1u];
        const std::size_t axle=i/2u;
        const float center=p.f32((axle+0x0eu)*0x4cu);
        wheel.put8(0u,0u);
        wheel.putf(0x04u,local[0]);wheel.putf(0x08u,std::isfinite(center)?center:0.15f);
        wheel.putf(0x0cu,local[2]);wheel.putf(0x28u,0.0f);wheel.putf(0x2cu,0.0f);
        wheel.putf(0x38u,reference_load);
        wheel.putf(0x40u,0.0f);wheel.putf(0x44u,0.0f);wheel.putf(0x48u,-1.0f);
        wheel.putf(0x4cu,-1.0f);wheel.putf(0x50u,0.0f);wheel.putf(0x54u,0.0f);
        wheel.putf(0x70u,0.0f);wheel.putf(0x74u,1.0f);wheel.putf(0x78u,0.0f);
    }
    auto matrices=matrix_stack(s);driving::pc_matrix_identity(matrices);
    s.chassis_initialized=true;
}

float body_forward_speed(driving::Bytes w){
    // The display/world forward vector is local -Z in the retained PC matrix.
    const float fx=-w.f32(0x30u),fz=-w.f32(0x38u);
    const float vx=w.f32(0x5cu),vz=w.f32(0x64u);
    return vx*fx+vz*fz;
}

float body_yaw(driving::Bytes w){
    // Matrix set by mxRotateY(-yaw): m20=-sin(yaw), m22=cos(yaw).
    return std::atan2(-w.f32(0x30u),w.f32(0x38u));
}

void run_recovered_chassis(PcVehicleControlState& s){
    auto e=event_view(s),w=work_view(s),p=parameter_view(s);
    auto matrices=matrix_stack(s);driving::pc_matrix_identity(matrices);
    const auto wheels=driving::embedded_wheels(w);
    // CommonPlCar's force phase: suspension/drag/gravity/incline, followed by
    // the tire force vectors produced by DrivingControl/CopyPhysicalWork.
    try{driving::make_force_work(e,w,p,wheels,matrices);}
    catch(const std::exception& ex){throw std::runtime_error(std::string("MakeForceWork: ")+ex.what());}
    try{driving::make_force_work_tire_4a0000(e,w,p,wheels,matrices);}
    catch(const std::exception& ex){throw std::runtime_error(std::string("MakeForceWorkTire gear=")+std::to_string(e.u32(0x208u))+" max="+std::to_string(p.u32(0x10a0u))+": "+ex.what());}
    try{driving::action_force2(w,e.f32(0xdbcu),matrices);}
    catch(const std::exception& ex){throw std::runtime_error(std::string("ActionForce2: ")+ex.what());}
    ++s.chassis_calls;
    s.chassis_active=true;
}

void apply_road_contacts(PcVehicleControlState& s){
    auto w=work_view(s),p=parameter_view(s);
    const auto wheels=driving::embedded_wheels(w);
    for(unsigned axis=0;axis<3u;++axis)w.putf(0x628u+axis*4u,s.road_contacts.normal[axis]);
    for(std::size_t i=0;i<wheels.size();++i){
        auto wheel=wheels[i];const auto& contact=s.road_contacts.wheels[i];
        const std::size_t axle=i/2u;
        const float center=p.f32((axle+0x0eu)*0x4cu);
        wheel.put8(0u,contact.supported?0u:1u);
        wheel.putf(0x08u,std::clamp(center+contact.suspension_delta,
            p.f32((axle+0x10u)*0x4cu),p.f32((axle+0x12u)*0x4cu)));
        wheel.putf(0x28u,0.0f);
        wheel.put32(0x14u,contact.material);
        for(unsigned axis=0;axis<3u;++axis)
            wheel.putf(0x70u+axis*4u,contact.supported?contact.normal[axis]:s.road_contacts.normal[axis]);
    }
}

constexpr driving::RunningResistanceTuning RetailRunningResistance{
    0.0225f,0.2f,1.0f,0.9f,0.175f,0.175f};
constexpr float PcFixedStep=0.01661129482090473175048828125f;
constexpr float PcSpeedDisplayScale=216.720001220703125f;

std::uint16_t measure_steering_full_lock(PcVehicleControlState& s){
    // Measure through the recovered PC path itself.  This deliberately uses a
    // copy: calibration must not alter live input, wheel or counter state.
    auto event=s.event;
    auto work=s.work;
    driving::Bytes e(event.data(),event.size());
    driving::Bytes w(work.data(),work.size());
    auto p=parameter_view(s);
    driving::operation_input_49fad0(e,{16,127,0,0});
    e.put32(0x1f4u,0u);
    driving::steering_operation(e,w,p);
    const auto angle=static_cast<std::int32_t>(e.i16(0x32u));
    const auto magnitude=angle<0?-angle:angle;
    return static_cast<std::uint16_t>(std::clamp(magnitude,1,32767));
}

} // namespace

void initialize_pc_vehicle_control(PcVehicleControlState& s){
    s=PcVehicleControlState{};
    auto e=event_view(s),w=work_view(s),p=parameter_view(s);

    // Bounded defaults for synthetic tests. configure_pc_vehicle_control()
    // replaces this view with the exact selected retail parameter column.
    // These first four values also reproduce the 1.0 placeholders installed
    // by PC RIGID_BODY construction before player-car data is selected.
    p.putf(0x000,1.0f);p.putf(0x04c,1.0f);p.putf(0x098,1.0f);p.putf(0x0e4,1.0f);
    p.putf(0x130,1.0f);
    p.putf(0x193c,4.0f);
    p.putf(0x17c,1.0f);
    p.putf(0x18a4,1.0f);
    p.putf(0x18f0,1.0f);
    p.put32(0x10a0,6u);
    p.putf(0x1644,1000.0f);
    p.putf(0xb94,0.31f);
    p.putf(0x134c,3.0f);
    p.putf(0x10ec,1.0f);
    for(std::uint32_t gear=0;gear<=7u;++gear)
        p.putf((gear+0x3au)*0x4cu,3.0f/(float(gear)+1.0f));

    e.put32(0x2b4,0xdead0001u); // canary: native code never dereferences it
    e.put8(0x13,1u);            // manual-transmission PC branch
    e.put32(0x208,1u);
    e.puti(0x3c,255);           // engaged clutch input domain
    e.put8(0x296,0u);
    e.putf(0xdbcu,1.0f);        // HandicapControl default ActionForce2 scale
    e.putf(0xdc0u,1.0f);
    for(std::size_t i=0;i<4u;++i)
        w.put32(0x248u+i*4u,0xbad00001u+static_cast<std::uint32_t>(i));
    initialize_physics_views(s);
    s.steering_full_lock=measure_steering_full_lock(s);
    s.initialized=true;
}

bool configure_pc_vehicle_control(PcVehicleControlState& s,
                                  const DrivingDataPack& pack,
                                  std::uint32_t car_id,
                                  std::uint32_t selector_map){
    initialize_pc_vehicle_control(s);
    std::uint32_t column{};
    if(!copy_driving_parameter_view(pack,selector_map,car_id,
                                    s.parameters.data(),s.parameters.size(),&column)||
       pack.torque_tables[0].size()!=DrivingTorqueTableBytes||
       pack.torque_tables[1].size()!=DrivingTorqueTableBytes||
       pack.brake_table.size()!=DrivingBrakeTableBytes)return false;
    std::copy(pack.torque_tables[0].begin(),pack.torque_tables[0].end(),
              s.torque_tables[0].begin());
    std::copy(pack.torque_tables[1].begin(),pack.torque_tables[1].end(),
              s.torque_tables[1].begin());
    std::copy(pack.brake_table.begin(),pack.brake_table.end(),s.brake_table.begin());
    auto e=event_view(s),p=parameter_view(s);
    s.engine_rpm=p.f32(0x15f8u);
    e.putf(0x21cu,s.engine_rpm);
    e.putf(0x2a0u,1.0f);
    e.putf(0x22cu,0.0f);
    e.putf(0x228u,0.0f);
    s.retail_parameter_column=column;
    s.retail_car_id=car_id;
    initialize_physics_views(s);
    s.steering_full_lock=measure_steering_full_lock(s);
    s.retail_driving_data=true;
    return true;
}

PcVehicleControlOutput pc_vehicle_control_output(PcVehicleControlState& s){
    if(!s.initialized)initialize_pc_vehicle_control(s);
    auto e=event_view(s),w=work_view(s);
    const auto wheel_records=driving::embedded_wheels(w);
    PcVehicleControlOutput out{};
    out.linear_speed=s.linear_speed;
    out.steering_angle=e.i16(0x32);
    out.steering_full_lock=s.steering_full_lock;
    out.gear=e.u32(0x208);
    out.engine_rpm=s.engine_rpm;
    out.drive_torque=s.drive_torque;
    out.brake_pressure=s.brake_pressure;
    out.body_position={{w.f32(0x40u),w.f32(0x44u),w.f32(0x48u)}};
    out.body_velocity={{w.f32(0x5cu),w.f32(0x60u),w.f32(0x64u)}};
    out.body_yaw=body_yaw(w);
    out.chassis_active=s.chassis_active;
    out.road_contact_mask=s.road_contacts.hit_mask;
    out.full_driving_control=s.driving_control_calls!=0u;
    for(std::size_t i=0;i<out.wheels.size();++i){
        out.wheels[i].spin=wheel_records[i].i16(0x30);
        out.wheels[i].direction=wheel_records[i].i16(0x32);
        out.wheels[i].translation_delta[1]=s.road_contacts.wheels[i].suspension_delta;
        out.suspension_forces[i]=wheel_records[i].f32(0x24u);
        out.tire_loads[i]=wheel_records[i].f32(0x34u);
        out.road_mu[i]=wheel_records[i].f32(0xe8u);
        out.road_surfaces[i]=s.road_contacts.wheels[i].material;
    }
    return out;
}

void set_pc_vehicle_road_contacts(PcVehicleControlState& s,
                                  const VehicleRoadContactFrame& contacts){
    s.road_contacts=contacts;
    s.road_contacts_valid=contacts.supported&&contacts.hit_count>=3u;
}

void set_pc_vehicle_world_pose(PcVehicleControlState& s,
                               const std::array<float,3>& position,float yaw,
                               bool clear_velocity){
    if(!s.initialized)initialize_pc_vehicle_control(s);
    auto w=work_view(s);
    auto matrices=matrix_stack(s);
    driving::pc_matrix_identity(matrices);
    driving::pc_matrix_rotate_y(matrices,-yaw);
    driving::pc_matrix_set_translation(matrices,{position[0],position[1],position[2]});
    driving::pc_matrix_get(matrices,w.sub(0x10u,64u));
    if(clear_velocity){
        for(const auto off:{0x50u,0x5cu,0x68u,0x74u,0xc0u,0xccu})
            for(unsigned k=0;k<3u;++k)w.putf(off+k*4u,0.0f);
        s.linear_speed=0.0f;
    }
    s.chassis_pose_bound=true;
    ++s.chassis_pose_sets;
}

void set_pc_vehicle_world_position(PcVehicleControlState& s,
                                   const std::array<float,3>& position){
    if(!s.initialized)initialize_pc_vehicle_control(s);
    auto w=work_view(s);
    w.putf(0x40u,position[0]);w.putf(0x44u,position[1]);w.putf(0x48u,position[2]);
    s.chassis_pose_bound=true;
    ++s.chassis_pose_sets;
}

PcVehicleControlOutput step_pc_vehicle_control(
    PcVehicleControlState& s,const PcVehicleControlInput& input){
    if(!s.initialized)initialize_pc_vehicle_control(s);
    auto e=event_view(s),w=work_view(s),p=parameter_view(s);

    const auto steering=std::clamp(input.steering,-128,127);
    const auto accelerator=std::min(input.accelerator,255u);
    const auto brake=std::min(input.brake,255u);
    driving::operation_input_49fad0(e,{16,steering,static_cast<std::int32_t>(accelerator),
                                       static_cast<std::int32_t>(brake)});
    ++s.operation_input_calls;

    const bool full_chain=s.retail_driving_data&&s.road_contacts_valid;
    if(full_chain&&s.chassis_initialized){
        const float vx=w.f32(0x5cu),vz=w.f32(0x64u);
        s.linear_speed=std::hypot(vx,vz)*PcFixedStep;
    }
    const float speed_magnitude=std::fabs(s.linear_speed);
    const float pc_speed=full_chain?speed_magnitude:speed_magnitude*1000.0f;
    const float pc_speed_integer_scale=full_chain?PcSpeedDisplayScale:1000.0f;
    e.putf(0x1c4,pc_speed);
    e.put32(0x1f4,static_cast<std::uint32_t>(
        std::min(std::lround(speed_magnitude*pc_speed_integer_scale),100000l)));
    const auto old_gear=e.u32(0x208);
    if(full_chain){
        apply_road_contacts(s);
        try{driving::suspension_force(w,p);}catch(const std::exception& ex){
            throw std::runtime_error(std::string("live suspension_force: ")+ex.what());
        }
        ++s.suspension_force_calls;
        try{driving::tire_load(e,w,p);}catch(const std::exception& ex){
            throw std::runtime_error(std::string("live tire_load: ")+ex.what());
        }
        ++s.tire_load_calls;
        driving::DrivingControlInputs inputs{};
        inputs.analog_channel_1=static_cast<std::int32_t>(accelerator);
        inputs.shift_up=input.shift_up;inputs.shift_down=input.shift_down;
        inputs.running_resistance=RetailRunningResistance;
        inputs.reaction_blend_parameter=p.f32(0x20a8u);
        try{driving::driving_control(e,w,p,table_views(s),inputs);}catch(const std::exception& ex){
            throw std::runtime_error(std::string("live DrivingControl: ")+ex.what());
        }
        ++s.driving_control_calls;++s.road_mu_calls;++s.steering_calls;++s.transmission_calls;
        ++s.engine_torque_calls;++s.brake_pressure_calls;
        ++s.road_contact_frames;s.road_contact_wheel_hits+=s.road_contacts.hit_count;
        s.wheel_angle_updates+=4u;
        s.engine_rpm=e.f32(0x21cu);s.drive_torque=e.f32(0x214u);
        s.brake_pressure=driving::brake_pressure(e.i32(0x38u),table_views(s));
    }else{
        driving::steering_operation(e,w,p);
        driving::toe_angle(e,w,p,static_cast<std::int32_t>(accelerator));
        driving::tire_direction_angles(e,w);
        ++s.steering_calls;

        // Keep the rear velocity fields used by the original downshift predictor
        // coherent with the bridge's bounded translation state.
        w.putf(0x514,s.linear_speed*100.0f);
        w.putf(0x608,s.linear_speed*100.0f);
        driving::manual_transmission(e,w,p,false,input.shift_up,input.shift_down);
        ++s.transmission_calls;
    }
    if(input.shift_up)++s.shift_up_requests;
    if(input.shift_down)++s.shift_down_requests;
    if(e.u32(0x208)!=old_gear)++s.gear_changes;

    if(full_chain&&s.chassis_initialized&&s.chassis_pose_bound){
        try{run_recovered_chassis(s);}catch(const std::exception& ex){
            throw std::runtime_error(std::string("live recovered chassis: ")+ex.what());
        }
        const float vx=w.f32(0x5cu),vz=w.f32(0x64u);
        s.linear_speed=std::hypot(vx,vz)*PcFixedStep;
        ++s.frames;
        return pc_vehicle_control_output(s);
    }

    // Bounded fallback retained only for synthetic/no-contact callers which do
    // not provide enough state for the recovered PC rigid-body chain.
    const auto gear=std::max(e.u32(0x208),1u);
    const float throttle=float(e.i32(0x34))/255.0f;
    float drive_step=throttle*0.0045f*
                     (1.0f-0.06f*float(std::min(gear,6u)-1u));
    float brake_step=float(e.i32(0x38))/255.0f*0.012f;
    if(s.retail_driving_data&&!full_chain){
        const float predicted=driving::predicted_engine_speed(e,w,p,gear);
        const float target=std::clamp(std::max(p.f32(0x15f8u),predicted),
                                      0.0f,p.f32(0x1690u));
        s.engine_rpm+=std::clamp(target-s.engine_rpm,-24.0f,18.0f);
        e.putf(0x21cu,s.engine_rpm);
        e.put32(0x48u,static_cast<std::uint32_t>(
            std::clamp(std::lround(s.engine_rpm*9.5492965855f),0l,20000l)));
        driving::accel_operation(e,w,p);
        driving::auto_clutch_control(e,w,p);
        auto tables=table_views(s);
        driving::engine_torque(e,p,tables);
        ++s.engine_torque_calls;
        s.drive_torque=e.f32(0x214u);
        s.brake_pressure=driving::brake_pressure(e.i32(0x38u),tables);
        ++s.brake_pressure_calls;
        const float effective_throttle=float(e.i32(0x34u))/255.0f;
        drive_step=std::max(s.drive_torque,0.0f)*0.000034f*effective_throttle/
                   (1.0f+0.08f*float(std::min(gear,6u)-1u));
        brake_step=s.brake_pressure*0.012f;
    }
    s.linear_speed+=drive_step;
    s.linear_speed=std::max(0.0f,s.linear_speed-brake_step);
    s.linear_speed*=throttle>0.0f?0.999f:0.994f;
    const float gear_limit=0.10f+float(std::min(gear,6u))*0.05f;
    s.linear_speed=std::clamp(s.linear_speed,0.0f,gear_limit);

    if(!full_chain){
        const float angular_velocity=s.linear_speed*15.0f;
        for(auto wheel:driving::embedded_wheels(w)){
            wheel.putf(0xd8,angular_velocity);
            driving::advance_wheel_angle_inline(wheel,angular_velocity);
            ++s.wheel_angle_updates;
        }
    }
    ++s.frames;
    return pc_vehicle_control_output(s);
}

void reject_pc_vehicle_motion(PcVehicleControlState& s){
    if(!s.initialized)initialize_pc_vehicle_control(s);
    s.linear_speed=0.0f;
    auto w=work_view(s);
    for(const auto off:{0x50u,0x5cu,0x68u,0x74u,0xc0u,0xccu})
        for(unsigned k=0;k<3u;++k)w.putf(off+k*4u,0.0f);
    for(auto wheel:driving::embedded_wheels(work_view(s)))wheel.putf(0xd8,0.0f);
    ++s.chassis_rejects;
    ++s.rejected_motion;
}

float normalized_pc_vehicle_steering(const PcVehicleControlOutput& output){
    if(output.steering_full_lock==0u)return 0.0f;
    return std::clamp(-float(output.steering_angle)/
                      float(output.steering_full_lock),-1.0f,1.0f);
}

float pc_vehicle_yaw_step(const PcVehicleControlOutput& output){
    return normalized_pc_vehicle_steering(output)*0.014f*
           (0.15f+std::fabs(output.linear_speed));
}

} // namespace outrun::platform
