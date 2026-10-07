#include "driving/pc_driving_control.hpp"
#include "driving/pc_steering.hpp"
#include "driving/pc_tire_geometry.hpp"
#include "driving/pc_transmission.hpp"
#include <exception>
#include <stdexcept>
#include <string>

namespace outrun::driving {

void driving_control(Bytes e, Bytes w, Bytes p, const Tables& tables,
                     const DrivingControlInputs& in) {
#define OR2_DRIVING_STAGE(label, expression) do { \
    try { expression; } \
    catch (const std::exception& ex) { \
        throw std::runtime_error(std::string("DrivingControl ") + label + ": " + ex.what()); \
    } \
} while(false)
    // Exact call order observed in PC DrivingControl 0x00502C90.
    OR2_DRIVING_STAGE("tire_velocity", tire_velocity(w));
    OR2_DRIVING_STAGE("steering_operation", steering_operation(e, w, p));
    OR2_DRIVING_STAGE("toe_angle", toe_angle(e, w, p, in.analog_channel_1));
    OR2_DRIVING_STAGE("tire_direction", tire_direction(e, w));

    if (e.u8(0x13) == 1)
        OR2_DRIVING_STAGE("manual_transmission", manual_transmission(e, w, p, in.input_inhibited, in.shift_up, in.shift_down));
    else
        OR2_DRIVING_STAGE("auto_transmission", auto_transmission(e, p));

    OR2_DRIVING_STAGE("accel_operation", accel_operation(e, w, p));
    OR2_DRIVING_STAGE("distribute_brake_torque", distribute_brake_torque(e, w, p, tables));
    OR2_DRIVING_STAGE("auto_clutch_control", auto_clutch_control(e, w, p));
    OR2_DRIVING_STAGE("engine_torque", engine_torque(e, p, tables));
    OR2_DRIVING_STAGE("get_road_mu", get_road_mu(e, w, in.road_mu));

    auto wheels = embedded_wheels(w);
    OR2_DRIVING_STAGE("tire_grip", tire_grip(e, w, p, wheels));
    OR2_DRIVING_STAGE("cornering_power", cornering_power(e, p, wheels));
    OR2_DRIVING_STAGE("side_force", side_force(wheels));
    OR2_DRIVING_STAGE("front_driving_force", front_driving_force(p, wheels));
    OR2_DRIVING_STAGE("rear_driving_force", rear_driving_force(e, w, p));
    OR2_DRIVING_STAGE("friction_circle", friction_circle(e, p, wheels));
    OR2_DRIVING_STAGE("resolve_wheel_forces", resolve_wheel_forces(wheels));
    OR2_DRIVING_STAGE("front_wheel_rotation", front_wheel_rotation(p, wheels));
    OR2_DRIVING_STAGE("rear_wheel_rotation", rear_wheel_rotation(e, w, p));
    OR2_DRIVING_STAGE("rolling_resistance", rolling_resistance(e, p, wheels));
    OR2_DRIVING_STAGE("slip_ratio", slip_ratio(p, wheels));
    OR2_DRIVING_STAGE("running_resistance", running_resistance(e, w, in.running_resistance));
    OR2_DRIVING_STAGE("copy_physical_work", copy_physical_work(e, w, in.reaction_blend_parameter));
#undef OR2_DRIVING_STAGE
}

} // namespace outrun::driving
