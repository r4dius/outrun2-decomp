#include "vehicle_preview_init.hpp"
#include <stdexcept>
#include <vector>
namespace outrun::platform {
namespace {
// PC 5C24D0: twelve 12-byte start slots indexed by 45A2B0 rank. 5C2560 follows.
constexpr std::array<driving::CourseProbe,12> RankStart{{
    {2.f,0.f,-20.f},{-2.f,0.f,-15.f},{2.f,0.f,-10.f},{-2.f,0.f,-5.f},
    {0.f,0.f,-20.f},{4.f,0.f,-18.25f},{0.f,0.f,-13.f},{4.f,0.f,-11.25f},
    {0.f,0.f,-6.f},{4.f,0.f,-4.25f},{0.f,0.f,1.f},{4.f,0.f,2.75f}}};
constexpr driving::CourseProbe SingleStart{0.f,0.f,-16.f}; // PC 5C2560
void put3(driving::Bytes b,std::size_t o,const driving::CourseProbe& p){b.putf(o,p.x);b.putf(o+4,p.y);b.putf(o+8,p.z);}
}
driving::CourseProbe vehicle_preview_start_position(std::uint8_t scene,std::uint8_t rank){
    if(scene<=1u)return SingleStart;
    if(rank>=RankStart.size())throw std::out_of_range("start rank outside 5C24D0");
    return RankStart[rank];
}
bool vehicle_preview_init_4a7270(driving::Bytes e,VehiclePreviewInit& in,VehicleParameterChoice* out){
    e.check(0,PcVehicleObjectBytes);
    VehicleParameterChoice choice{};
    if(!vehicle_construct_4a5830(e,in.queue,in.driving,in.selection,in.shared_83db30,&choice))return false;
    if(out)*out=choice;
    if(in.parameters.size()<DrivingParameterViewBytes)throw std::out_of_range("4A7270 parameter view");
    std::vector<std::uint8_t> selected(DrivingParameterViewBytes);
    if(!copy_driving_parameter_view(in.driving,choice.map,choice.base_model,selected.data(),selected.size()))
        throw std::runtime_error("4A7270 constructor selected an unavailable parameter column");
    for(std::size_t k=0;k<selected.size();++k)in.parameters.put8(k,selected[k]);
    driving::course_environment_sun_449f50(in.environment,in.primary_course,in.environment_context);
    vehicle_body_init_4a69f0(e,in.body_82e7f0,in.parameters,0x82e7f0u,in.world);
    vehicle_ground_init_519300(e,in.body_82e7f0,in.world,in.area_yaw);
    e.put8(0x13,in.transmission_830374==1u?1u:0u);
    e.put16(0x2c,0);e.put16(0x2e,0);e.put16(0x30,0);
    std::uint8_t rank=0;
    if(in.selection.loading_scene_7de418>1u){
        if(!in.rank)throw std::logic_error("4A7270 rank provider 45A2B0 unavailable");
        rank=driving::rank_provider_gateway_45a2b0(e.u8(0x10),*in.rank);
    }
    const auto start=vehicle_preview_start_position(in.selection.loading_scene_7de418,rank);
    put3(e,0x14,start);put3(e,0xd28,start);
    e.putf(0x178,0.f);e.putf(0x1c4,0.f);
    vehicle_collision_init_4f6e40(e);
    // 49A650 is a bare RET in this hash-pinned build.
    driving::set_old_param_buffer_4a2ee0(e);
    e.put16(0x2e,in.yaw_841fa0);e.put16(0x17e,in.yaw_841fa0);e.put16(0x160,in.yaw_841fa0);
    const auto flags=(e.u32(4)&~2u)|0xa1u;e.put32(4,flags);
    e.put32(0x5c,1);e.put8(0x66,3);
    e.putf(0x14,0.f);e.putf(0x18,0.f);e.putf(0x1c,0.f);
    e.put16(0x2c,0);e.put16(0x30,0);e.put16(0x162,0);
    e.putf(0x300,10000.f); // 6280A0
    e.put8(0x329,0xff);e.putf(0xd24,0.f);
    auto network=e.u32(0xc5c);float blend;
    if(flags&0x40u){network|=0x2000u;blend=0.25f;} // 628088
    else{network&=~0x2000u;blend=1.f;}
    e.put32(0xc5c,network);
    e.put16(0xb62,0);e.put16(0xb64,0);
    e.put32(0xd10,0);e.put32(0xd14,0);e.put32(0xd18,0);
    e.putf(0xb68,blend);
    put3(e,0xd28,{0.f,0.f,0.f});
    e.put32(0x68,0xe);
    return true;
}
bool vehicle_arcade_init_4a7080(driving::Bytes e,VehiclePreviewInit& in,const VehicleArcadeInit& a){
    e.check(0,PcVehicleObjectBytes);
    VehicleParameterChoice choice{};
    if(!vehicle_construct_4a5830(e,in.queue,in.driving,in.selection,in.shared_83db30,&choice))return false;
    if(in.parameters.size()<DrivingParameterViewBytes)throw std::out_of_range("4A7080 parameter view");
    std::vector<std::uint8_t> selected(DrivingParameterViewBytes);
    if(!copy_driving_parameter_view(in.driving,choice.map,choice.base_model,selected.data(),selected.size()))
        throw std::runtime_error("4A7080 constructor selected an unavailable parameter column");
    for(std::size_t k=0;k<selected.size();++k)in.parameters.put8(k,selected[k]);
    driving::course_environment_sun_449f50(in.environment,in.primary_course,in.environment_context);
    vehicle_body_init_4a69f0(e,in.body_82e7f0,in.parameters,0x82e7f0u,in.world);
    vehicle_ground_init_519300(e,in.body_82e7f0,in.world,in.area_yaw);
    e.put8(0x13,in.transmission_830374==1u?1u:0u);
    e.put16(0x2c,0);e.put16(0x2e,0);e.put16(0x30,0);
    std::uint8_t rank=0;
    if(in.selection.loading_scene_7de418>1u){
        if(!in.rank)throw std::logic_error("4A7080 rank provider 45A2B0 unavailable");
        rank=driving::rank_provider_gateway_45a2b0(e.u8(0x10),*in.rank);
    }
    const auto start=vehicle_preview_start_position(in.selection.loading_scene_7de418,rank);
    put3(e,0x14,start);put3(e,0xd28,start);
    e.putf(0x178,0.f);e.putf(0x1c4,0.f);
    vehicle_collision_init_4f6e40(e);
    if(a.shadow_46ba20)a.shadow_46ba20(e);
    // 49A650 is a bare RET in this hash-pinned build.
    driving::set_old_param_buffer_4a2ee0(e);
    if(!a.reset_46f350)throw std::logic_error("4A7080 other-car reset 46F350 unavailable");
    a.reset_46f350(e);
    e.put32(0x5c,1);e.put8(0x66,3);
    const bool demo=a.mode_78026c==3u&&(a.query_49eed0==2u||a.query_49eed0==3u||a.query_49eed0==4u);
    if(demo&&a.query_49eee0){e.putf(0x14,2.f);e.putf(0x18,0.f);e.putf(0x1c,-8.f);}   // 6280B0, 5C1C70
    else if(demo)e.putf(0x14,0.f);                                                    // only x (4A71DF)
    else{e.putf(0x18,0.f);e.putf(0x1c,0.f);e.putf(0x14,0.f);}
    const driving::CourseProbe at{e.f32(0x14),e.f32(0x18),e.f32(0x1c)};
    e.put16(0x2e,0x84f4u);e.put16(0x160,0x84f4u);                                     // -31500
    e.put32(4,(e.u32(4)&~0x42u)|0xa1u);
    e.putf(0x300,10000.f);                                                             // 6280A0
    put3(e,0xd28,at);
    e.put16(0x2c,0);e.put16(0x30,0);e.put16(0x162,0);e.put16(0xb62,0);e.put16(0xb64,0);
    e.put32(0xd10,0);e.put32(0xd14,0);e.put32(0xd18,0);
    e.put8(0x329,0xff);e.putf(0xd24,0.f);
    e.put32(0x68,0xe);
    return true;
}
bool game_pl_car_init_4a6ed0(driving::Bytes e,GamePlCarInit& in,VehicleParameterChoice* out){
    e.check(0,PcVehicleObjectBytes);
    in.global_841b50=0u;in.flag_680bd0=1u;                            // [5C3604] is 0 in the EXE
    const auto kept_1054=e.u32(0x1054);
    e.put32(0x1058,0);
    VehicleParameterChoice choice{};
    if(!vehicle_construct_4a5830(e,in.queue,in.driving,in.selection,in.shared_83db30,&choice))return false;
    if(out)*out=choice;
    if(in.parameters.size()<DrivingParameterViewBytes)throw std::out_of_range("4A6ED0 parameter view");
    std::vector<std::uint8_t> selected(DrivingParameterViewBytes);
    if(!copy_driving_parameter_view(in.driving,choice.map,choice.base_model,selected.data(),selected.size()))
        throw std::runtime_error("4A6ED0 constructor selected an unavailable parameter column");
    for(std::size_t k=0;k<selected.size();++k)in.parameters.put8(k,selected[k]);
    e.put16(0x64,0);e.put32(0x1054,kept_1054);
    vehicle_body_init_4a69f0(e,in.body_82e7f0,in.parameters,0x82e7f0u,in.world);
    vehicle_ground_init_519300(e,in.body_82e7f0,in.world,in.area_yaw);
    e.put8(0x13,in.transmission_830374==1u?1u:0u);
    e.put16(0x2c,0);e.put16(0x2e,0);e.put16(0x30,0);
    driving::CourseProbe start{};
    if(in.selection.game_mode_780258==4u){
        if(!in.grid_variant4||!in.grid_variant4(e,start))throw std::logic_error("4A6ED0 variant-4 grid 4963B0/456E00/43F730 unavailable");
    }else if(in.selection.loading_scene_7de418<=1u)start=SingleStart;
    else{
        if(!in.rank)throw std::logic_error("4A6ED0 rank provider 45A2B0 unavailable");
        const auto rank=driving::rank_provider_gateway_45a2b0(std::uint8_t(e.u32(0x1054)),*in.rank);
        if(rank>=RankStart.size())throw std::out_of_range("start rank outside 5C24D0");
        start=RankStart[rank];
    }
    if(in.selection.game_mode_780258!=4u)put3(e,0x14,start);
    put3(e,0xd28,{e.f32(0x14),e.f32(0x18),e.f32(0x1c)});
    e.putf(0x178,0.f);e.putf(0x1c4,0.f);
    vehicle_collision_init_4f6e40(e);
    if(in.shadow_46ba20)in.shadow_46ba20(e);
    if(in.envmap_46bbc0)in.envmap_46bbc0(e);
    driving::set_old_param_buffer_4a2ee0(e);
    return true;
}
}
