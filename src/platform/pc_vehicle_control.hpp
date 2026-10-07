#pragma once
#include "driving/pc_driving.hpp"
#include "platform/driving_data_pack.hpp"
#include "platform/vehicle_constructor.hpp"
#include "platform/vehicle_body_init.hpp"
#include "platform/vehicle_road_contact.hpp"
#include "platform/vehicle_visual.hpp"
#include <array>
#include <cstdint>

namespace outrun::platform {

// Native owner for the event/work/parameter slices consumed by the recovered
// PC input, four-wheel suspension/load and complete DrivingControl chain.
// Retail sessions also retain the recovered rigid-body state used by
// ActionForce2; synthetic/unit-test sessions may still use the bounded motion
// adapter when no complete retail/contact chain is available.
struct PcVehicleControlState {
    std::array<std::uint8_t,PcVehicleObjectBytes> event{};
    std::array<std::uint8_t,PcVehicleBodyBytes> work{};
    std::array<std::uint8_t,driving::parameter_size> parameters{};
    std::array<std::array<std::uint8_t,DrivingTorqueTableBytes>,2> torque_tables{};
    std::array<std::uint8_t,DrivingBrakeTableBytes> brake_table{};
    VehicleRoadContactFrame road_contacts{};
    float linear_speed{};
    float engine_rpm{};
    float drive_torque{};
    float brake_pressure{};
    std::array<std::uint8_t,64u*8u> matrix_stack_storage{};
    std::uint32_t chassis_calls{};
    std::uint32_t chassis_pose_sets{};
    std::uint32_t chassis_rejects{};
    std::uint32_t frames{};
    std::uint32_t operation_input_calls{};
    std::uint32_t steering_calls{};
    std::uint32_t transmission_calls{};
    std::uint32_t wheel_angle_updates{};
    std::uint32_t shift_up_requests{};
    std::uint32_t shift_down_requests{};
    std::uint32_t gear_changes{};
    std::uint32_t rejected_motion{};
    std::uint32_t engine_torque_calls{};
    std::uint32_t brake_pressure_calls{};
    std::uint32_t road_contact_frames{};
    std::uint32_t road_contact_wheel_hits{};
    std::uint32_t suspension_force_calls{};
    std::uint32_t tire_load_calls{};
    std::uint32_t driving_control_calls{};
    std::uint32_t road_mu_calls{};
    std::uint32_t retail_parameter_column{};
    std::uint32_t retail_car_id{};
    std::uint16_t steering_full_lock{};
    bool retail_driving_data{};
    bool road_contacts_valid{};
    bool chassis_initialized{};
    bool chassis_pose_bound{};
    bool chassis_active{};
    bool initialized{};
};

struct PcVehicleControlInput {
    std::int32_t steering{};      // native ReadIO signed-byte domain
    std::uint32_t accelerator{}; // PC analogue volume 1, 0..255
    std::uint32_t brake{};       // PC analogue volume 2, 0..255
    bool shift_up{};
    bool shift_down{};
};

struct PcVehicleControlOutput {
    float linear_speed{};
    std::int16_t steering_angle{}; // event+0x32
    std::uint16_t steering_full_lock{}; // measured from the active PC parameter view
    std::uint32_t gear{};          // event+0x208
    float engine_rpm{};
    float drive_torque{};
    float brake_pressure{};
    std::array<float,3> body_position{};
    float body_yaw{};
    std::array<float,3> body_velocity{};
    bool chassis_active{};
    std::uint32_t road_contact_mask{};
    std::array<float,4> suspension_forces{};
    std::array<float,4> tire_loads{};
    std::array<float,4> road_mu{};
    std::array<std::uint32_t,4> road_surfaces{};
    bool full_driving_control{};
    std::array<VehicleVisualWheel,4> wheels{};
};

void initialize_pc_vehicle_control(PcVehicleControlState& state);
// Explicit low-level arithmetic adapter. Do not infer a selected game model
// from OR2DRV1's historical header; use 5051D0 for the actual event constructor.
bool configure_pc_vehicle_control(PcVehicleControlState& state,
                                  const DrivingDataPack& pack,
                                  std::uint32_t car_id,
                                  std::uint32_t selector_map);
void set_pc_vehicle_road_contacts(PcVehicleControlState& state,
                                  const VehicleRoadContactFrame& contacts);
void set_pc_vehicle_world_pose(PcVehicleControlState& state,
                               const std::array<float,3>& position,float yaw,
                               bool clear_velocity=true);
void set_pc_vehicle_world_position(PcVehicleControlState& state,
                                   const std::array<float,3>& position);
PcVehicleControlOutput step_pc_vehicle_control(
    PcVehicleControlState& state,const PcVehicleControlInput& input);
void reject_pc_vehicle_motion(PcVehicleControlState& state);
PcVehicleControlOutput pc_vehicle_control_output(PcVehicleControlState& state);
float normalized_pc_vehicle_steering(const PcVehicleControlOutput& output);
float pc_vehicle_yaw_step(const PcVehicleControlOutput& output);

} // namespace outrun::platform
