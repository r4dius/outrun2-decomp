#include "vehicle_preview_event.hpp"
#include "vehicle_constructor_data.hpp"
#include "frontend_profiles.hpp"
#include "driving/pc_common_control.hpp"
#include <cstring>
namespace outrun::platform {
void vehicle_preview_control_4a5b20(driving::Bytes e,VehiclePreviewControl& c){
    e.check(0,PcVehicleObjectBytes);c.history_83db30.check(0,60);c.matrices.current();
    const auto model=e.u8(0x11);
    if(model>=VehicleConstructorData.size())throw std::out_of_range("preview vehicle model");
    float blend=c.frame_blend_634b34;
    if(c.interpolation_override_82e7d8!=0)blend=1.0f;
    else if((c.owner_flags_79fb4e&3)==2&&c.owner_799ca0.i32(0x1c)==3)blend=1.0f;
    for(unsigned o:{0x14u,0x16cu}){
        e.putf(o,c.position_680c04.x);e.putf(o+4,c.position_680c04.y);e.putf(o+8,c.position_680c04.z);
    }
    const auto old_yaw=std::uint16_t(e.i16(0x2e));e.put16(0x2e,c.yaw_841fa0);
    e.put16(0x17e,old_yaw);c.yaw_841fa0=std::uint16_t(c.yaw_841fa0+100);
    driving::PcDispMatrixContext display{c.matrices,blend,c.scene_82e7d4};
    driving::pc_calc_disp_matrix(e,display);
    const auto& row=VehicleConstructorData[model];
    auto f=[&](unsigned i){float v;std::memcpy(&v,&row[i],4);return v;};
    driving::PcReverseCarInputs reverse{f(16),f(17),f(18),f(19),0,c.game_mode_78026c};
    // Consume the shared CRT stream only on the original age>100 branch.
    if(e.u32(0x1f4)>100)reverse.random_value=frontend_crt_random_580f40(c.random_state);
    driving::check_reverse_car_4a2910(e,reverse,c.matrices);
    driving::record_ghost_car_4a4710(e,c.history_83db30,
        {c.steering_override_82e7ec,c.steering_82e7cc,c.game_mode_78026c});
}
}
