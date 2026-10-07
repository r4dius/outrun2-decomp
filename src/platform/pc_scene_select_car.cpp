// 49F4D0 = 46C090 (event 8 function 0x2B display: the OUTRUN2SP car-select car).
// 46C090 is 46C140 with the model draw 46A560 instead of 46AE70: the sun light
// 4082B0(2,0,0) diffuse scaled by the car brightness +58, 40D840(2), the car
// matrix +B0 pushed (409F90), 46A560(car, +11, 0), 40A010, the light restored
// and 40D840(2) again. 46A560 is the draw list of vehicle_model_draw.cpp; its render
// leaves 405360 / 4044F0 / 404540 / 4052B0 / 4052C0 and the shadow volume 422550
// run with the matrix recorded at each call.
#include "platform/pc_scene_renderer.hpp"
#include "platform/vehicle_model_draw.hpp"
#include <array>
#include <cstdio>
#include <cstring>
#include <stdexcept>
namespace outrun::platform {
using driving::Bytes;
namespace {
inline float mulf(float a,float b){volatile float x=a*b;return x;}
}
bool PcSceneRenderer::display_select_car(){
    Bytes vehicle(nullptr,0),camera(nullptr,0),body(nullptr,0);
    if(!vehicle_camera||!vehicle_camera(vehicle,camera)||!body_view||!body_view(body))return false;
    const auto model=vehicle.u8(0x11);
    if(model>=VehicleResourceIds.size())return false;
    for(const std::uint32_t id:{VehicleResourceIds[model],0xbbu}){
        if(bank_loaded(id))continue;
        std::vector<std::uint8_t> pmt;std::string error;
        if(!bank_source||!bank_source(id,pmt)||!load_bank(id,std::move(pmt),error)){++bank_failures;last_error="bank "+std::to_string(id)+": "+error;return false;}
    }
    PcEnvironmentRenderTables tables{Bytes(environment_.lights_899b98.data(),environment_.lights_899b98.size()),
        Bytes(environment_.fog_7d3a10.data(),environment_.fog_7d3a10.size()),0};
    PcVehicleDisplayContext c{*flush_,*queue_,tables,events_?events_->slots[386].flags:std::uint8_t(0),frame_.layer_7d25f0,matrices_,body,
        std::int32_t(current_mode_),scene_82e7d4};
    c.shadow=[this,vehicle](const PcVehicleDrawCall& call){return shadow_leaf(call,vehicle);};
    c.unported=[this](std::uint32_t pc){++unported_leaves[pc];};
    Bytes sun=tables.lights_899b98.sub(0x899cd8u-0x899b98u,0xa0);
    std::array<std::uint8_t,16> saved{};
    for(unsigned k=0;k<16;++k)saved[k]=sun.u8(8+k);
    const float scale=vehicle.f32(0x58);
    sun.putf(8,mulf(sun.f32(8),scale));
    sun.putf(0xc,mulf(scale,sun.f32(0xc)));
    sun.putf(0x10,mulf(scale,sun.f32(0x10)));
    render_environment_40d840(c.flush,2,c.flags_79fcca,c.environment);
    const auto depth=matrices_.depth;
    bool ok=true;
    try{
        driving::pc_matrix_push_load(matrices_,vehicle.sub(0xb0,64));             // 409F90
        PcVehicleModelDraw in{vehicle,std::int32_t(vehicle.i8(0x11)),0,body,-1,std::int32_t(current_mode_),scene_82e7d4};
        std::vector<PcVehicleDrawCall> calls;
        vehicle_select_model_draw_46a560(in,matrices_,calls);
        vehicle_draw_calls_execute(c,calls);
    }catch(const std::exception& e){last_error=std::string("46A560: ")+e.what();ok=false;}
    while(matrices_.depth>depth)driving::pc_matrix_pop(matrices_);               // 40A010
    for(unsigned k=0;k<16;++k)sun.put8(8+k,saved[k]);
    render_environment_40d840(c.flush,2,c.flags_79fcca,c.environment);
    if(ok)++select_car_displays;
    return ok;
}
}
