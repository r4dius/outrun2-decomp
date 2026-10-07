// Course passes 40BD80 / 40BE70 (race_course_passes.hpp) over the SCN_EFC
// work and the renderer state.
#include "platform/race_course_passes.hpp"
#include "platform/native_runtime.hpp"
#include "platform/pc_scene_renderer.hpp"
#include "platform/pc_vehicle_display.hpp"
#include "platform/race_manager_runtime.hpp"
#include "platform/race_area_runtime.hpp"
#include "platform/race_robots_runtime.hpp"
#include "platform/sprite_2d_runtime.hpp"
#include "platform/pc_address_view.hpp"
#include "platform/pc_render_flush.hpp"
#include "platform/race_scene_effects.hpp"
#include "driving/pc_d3dx.hpp"
#include "driving/pc_camera.hpp"
#include "driving/pc_environment_blend.hpp"
#include "platform/pc_car_reflection.hpp"
#include "platform/pc_render_queue.hpp"
#include "driving/pc_matrix_stack.hpp"
#include "driving/pc_x87.hpp"
#include "platform/retail_asset_store.hpp"
#include <algorithm>
#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
namespace outrun::platform {
namespace {
// PC statics the passes keep between frames (zero-initialised .bss on the PC
// unless noted).
struct CoursePassState {
    std::array<std::uint8_t,0x0c> targets_95afd4{};   // 95AFD4 / 95AFD8 saved render target / depth (422820 -> 422F20), 95AFDC (422AC0)
    std::array<std::uint8_t,0x18> viewport_955a44{};  // 955A44 saved viewport (422820 -> 422F20)
    std::array<std::uint8_t,0x80> data_751b2c{};      // 751B2C..751BAB shadow light records (.data, 422820 writes 751B94/751B98)
    bool data_loaded{},flare_loaded{};
    bool shadow_skipped{};                            // 422820 drew nothing: its 422F20 too (host guard)
    std::array<std::uint8_t,0x420> flare_7d2938{};       // 7D2938 \common\lens_flare_offset.bin (44A630), 7D2B18 its copy
    std::array<std::uint8_t,4> flag_7d2d74{};             // 7D2D74 (44A690)
    PcD3D9Device* query_device{};
    std::uint32_t query{};                 // 89F680 occlusion query (never released, as on the PC)
};
CoursePassState& state(){static CoursePassState s;return s;}
NativeCoursePassStats stats_;
bool exe_bytes(std::uint32_t a,std::uint8_t* out,std::size_t n){
    for(std::size_t i=0;i<EmbeddedExeRangeCount;++i){const auto& e=EmbeddedExeRanges[i];
        if(a>=e.base&&a+n<=e.base+e.size){std::memcpy(out,e.data+(a-e.base),n);return true;}}
    return false;
}
void put32(std::uint8_t* p,std::uint32_t v){std::memcpy(p,&v,4);}
}
// ---- the passes ----
// They work on the SCN_EFC work (780280) and the renderer state directly.
namespace {
using M=std::array<float,16>;
struct CoursePass {
    NativeRuntimeContext& c;
    PcSceneRenderer& r;
    PcFlushContext& fl;
    PcD3D9Device& d;
    driving::PcMatrixStack& s;
    PcRenderStageOps stages;
    CoursePass(NativeRuntimeContext& cc,PcSceneRenderer& rr)
        :c(cc),r(rr),fl(rr.flush_context()),d(rr.device()),s(rr.matrices()),stages{rr.flush_context()}{}
    // A matrix or word of the SCN_EFC work by its PC address.
    const std::uint8_t* work(std::uint32_t address,std::size_t bytes){
        const auto& w=c.race_effects.scene.work;
        const std::uint32_t at=address-PcSceneEffectsState::WorkBase;
        if(address<PcSceneEffectsState::WorkBase||std::size_t(at)+bytes>w.size())throw PcRaceUnmapped(address,bytes);
        return w.data()+at;
    }
    std::uint32_t work_u32(std::uint32_t address){std::uint32_t v;std::memcpy(&v,work(address,4),4);return v;}
    M work_matrix(std::uint32_t address){M m;std::memcpy(m.data(),work(address,64),64);return m;}
    // A sprite texture by id (bank << 16 | index; 0 when the bank is not loaded).
    std::uint32_t sprite_texture(std::uint32_t id){
        const std::uint32_t bank=(id>>16)&0xffffu;
        if(bank>=PcSpriteBankCount)return 0;
        const auto& b=native_sprite2d(c).state.banks[bank];
        if(b.state!=2u)return 0;
        const std::uint32_t index=id&0xffffu;
        if(index>=b.textures.size())throw PcRaceUnmapped(0x0fc00000u+index*4u,4);
        return b.textures[index];
    }
    M current(){M m;std::memcpy(m.data(),s.current().data(),64);return m;}
    void set_current(const M& m){std::memcpy(s.current().data(),m.data(),64);}
    // The current matrix inverted in place (unchanged when singular).
    void invert_current(){
        std::array<std::uint8_t,64> copy;std::memcpy(copy.data(),s.current().data(),64);
        (void)driving::pc_d3dx_matrix_inverse(s.current(),nullptr,driving::Bytes(copy.data(),64));
    }
    // 40C1B0 / 40C550: texture projection from the eye: inverse(view) * (m * bias).
    void projection(const M& m,const M& bias,std::uint32_t stage){
        driving::pc_matrix_push(s);
        set_current(render_state::load(fl.g.transform().slot[0]));
        invert_current();
        const M inverse_view=current();
        set_current(driving::pc_d3dx_matrix_multiply(m,bias));
        set_current(driving::pc_d3dx_matrix_multiply(inverse_view,current()));
        render_set_matrix_410f90(fl,current(),stage+2);
        driving::pc_matrix_pop(s);
    }
    void samplers(std::uint32_t stage,std::initializer_list<std::array<std::uint32_t,2>> states){
        for(const auto& v:states)stages.sampler(stage,v[0],v[1]);
    }
};
// 40BF10 (stage 2, the rain object at work+10): its sprite texture
// projected from the object's matrix (inverse of matrix * view).
void rain_40bf10(CoursePass& p,std::uint32_t stage,std::uint32_t object){
    const std::uint32_t texture=p.sprite_texture(p.work_u32(object+0x70));
    if(!texture)return;
    driving::pc_matrix_push_load(p.s,driving::Bytes(p.fl.g.transform().slot[0],64));
    p.set_current(driving::pc_d3dx_matrix_multiply(p.work_matrix(object+0x30),p.current()));
    p.invert_current();
    render_set_matrix_410f90(p.fl,p.current(),stage+2);
    driving::pc_matrix_pop(p.s);
    p.samplers(stage,{{1,1},{2,1},{5,2},{6,2},{7,1}});            // wrap, linear, point mip
    p.stages.texture_matrix_411230(2,stage,0x20000);
    p.d.set_texture(stage,texture);
    p.stages.mode_40b800(stage,0xd);
    p.stages.alpha_default3(stage);
    p.stages.bound(stage,1);
    p.r.frame().course_73e2a4=1;
}
// 40C1B0 (stage 3, the shadow light matrix at work+B0): the shadow map
// projected (scale 256 / -256, bias 256.25, depth 2^24 - 1), border
// addressing (wrap with ps_1_4), constant colour 80A0A0A0.
void shadow_40c1b0(CoursePass& p,std::uint32_t stage,std::uint32_t matrix){
    M bias{1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1};
    bias[0]=256.0f;bias[5]=-256.0f;bias[10]=16777215.0f;bias[12]=256.25f;bias[13]=256.25f;
    p.projection(p.work_matrix(matrix),bias,stage);
    const std::uint32_t address=p.fl.g.pixel_shader_14()?1u:4u;   // D3DTADDRESS_WRAP / BORDER
    p.samplers(stage,{{1,address},{2,address},{3,address},{5,2},{6,2}});
    p.stages.constants_40b4b0(0x80a0a0a0u,stage,0);
    p.d.set_texture(stage,0);
    p.r.frame().course_73e2a8=1;
    p.fl.g.shader().unknown_95aef8[1]=stage;                      // 95AEFC
}
// 40C550 (stage 3, the headlight object at work+130): its spot texture
// projected (scale and bias 0.5), modulated by the previous stage; no
// texture while the player car's +2C8 is positive.
void headlight_40c550(CoursePass& p,std::uint32_t stage,std::uint32_t object){
    driving::Bytes vehicle(nullptr,0),camera(nullptr,0);
    if(!(p.r.vehicle_camera&&p.r.vehicle_camera(vehicle,camera)&&vehicle.size()))throw PcRaceUnmapped(0x799d18u,4);  // read first on the PC
    M bias{1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1};
    bias[0]=0.5f;bias[5]=0.5f;bias[8]=0.5f;bias[9]=0.5f;
    p.projection(p.work_matrix(object+0x40),bias,stage);
    p.samplers(stage,{{1,4},{2,4},{3,4},{5,2},{6,2}});
    p.stages.constants_40b4b0(0x80808080u,stage,0);
    p.stages.colour_40b710(stage,((stage+8u)<<8)|0xc200001u,0xc00);
    p.stages.alpha_40b7b0(stage,0x1c301010u,0xc0);
    p.stages.bound(stage,1);
    p.stages.texture_matrix_411230(0x103,stage,0x20000);
    // fcomp 0.0 / test ah,41: positive (ordered) leaves the stage without texture.
    if(driving::X87(vehicle.f32(0x2c8))>driving::X87(0.0f))p.d.set_texture(stage,0);
    else p.d.set_texture(stage,p.sprite_texture(p.work_u32(object+0x80)));
}
// 40C150 / 40C4A0 / 40C8D0: the stage back to no texture and no texture
// matrix after the layer (40C150 for the rain stage, 40C4A0 / 40C8D0 for
// the shadow and headlight stage).
void rain_end_40c150(CoursePass& p,std::uint32_t stage){
    p.stages.bound(stage,0);
    p.stages.mode_40b800(stage,3);
    p.d.set_texture(stage,0);
    p.stages.texture_matrix_411230(0,stage,0);
    p.r.frame().course_73e2a4=0xffffffffu;
}
void stage_end(CoursePass& p,std::uint32_t stage){
    p.stages.mode_40b800(stage,3);
    p.stages.alpha_default3(stage);
    p.stages.bound(stage,0);
    p.d.set_texture(stage,0);
    p.stages.texture_matrix_411230(0,stage,0);
}
// ---- shadow map (layer 2): 422820 before the layer, 422F20 after it ----
using X=driving::X87;
float st(X v){return driving::x87_float(v);}
driving::Bytes slot(driving::PcMatrixStack& s,std::ptrdiff_t offset){return s.storage.sub(std::size_t(offset),64);}
void copy_matrix(driving::Bytes to,const void* from){std::memcpy(to.data(),from,64);}
// The D3DX helpers as the PC calls them (generic x87 paths).
M rotation(std::uint32_t axis,float angle){        // 4393B8 Z, 4393BE X, 4393DC Y
    const float sn=st(driving::x87_sin(X(angle))),cs=st(driving::x87_cos(X(angle)));
    M r{1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1};
    if(axis==2){r[0]=cs;r[1]=sn;r[4]=-sn;r[5]=cs;}
    else if(axis==0){r[5]=cs;r[6]=sn;r[9]=-sn;r[10]=cs;}
    else{r[0]=cs;r[2]=-sn;r[8]=sn;r[10]=cs;}
    return r;
}
M translation(float x,float y,float z){M r{1,0,0,0, 0,1,0,0, 0,0,1,0, x,y,z,1};return r;}
// 44A480: yaw = atan2(-x, -z) and pitch = atan2(y, |xz|) of a direction
// (x and z both below 2^-23 in size: z taken as 2^-23).
void yaw_pitch_44a480(const float v[3],float& pitch,float& yaw){
    const float eps=1.1920928955078125e-07f;
    const float x=v[0],y=v[1];float z=v[2];
    if(X(eps)>driving::x87_abs(X(x))&&X(eps)>driving::x87_abs(X(z)))z=eps;
    const X r=driving::x87_sqrt(X(z)*X(z)+X(x)*X(x));
    float length=st(r);
    if(X(eps)>r)length=eps;
    yaw=st(driving::x87_atan2(-X(x),-X(z)));
    pitch=st(driving::x87_atan2(X(y),X(length)));
}
// 422AC0(out, light): the shadow camera. An orthographic projection from the
// light record, turned about the view axis by the sun yaw (softened near the
// 45-degree multiples by the sun height) minus the car's heading, and a
// look-at from the light (sun direction 899C7C times the record's distance)
// to the car's origin offset; out = view * projection.
void shadow_camera_422ac0(CoursePass& p,M* out,const std::uint8_t light[0x2c]){
    auto& s=p.s;auto& tf=p.fl.g.transform();
    auto f=[&](unsigned k){float v;std::memcpy(&v,light+k*4,4);return v;};
    auto u16=[&](unsigned at){std::uint16_t v;std::memcpy(&v,light+at,2);return std::int32_t(v);};
    float sun[3];std::memcpy(sun,p.r.environment().lights_899b98.data()+(0x899c7cu-0x899b98u),12);
    const float distance=f(1);
    float eye[3]{st(-(X(sun[0])*X(distance))),st(-(X(sun[1])*X(distance))),st(-(X(sun[2])*X(distance)))};
    float at[3]{0,0,0};
    float offset[3]{0,0,f(0)};
    const float up[3]{0,1,0};
    driving::pc_matrix_push_unit(s);
    driving::pc_d3dx_ortho_off_center_rh(s.current(),st(-X(f(3))),f(3),f(2),st(-X(f(2))),f(4),f(5));
    float pitch,yaw;yaw_pitch_44a480(sun,pitch,yaw);
    const X degrees=X(57.2957763671875f);
    const float height=st(driving::x87_abs(X(st(X(pitch)*degrees))));            // 449390
    const float yaw_degrees=st(X(yaw)*degrees);
    const float within=st(driving::x87_abs(X(st(X(std::fmod(X(yaw_degrees).v,X(45.0).v))))));   // _CIfmod, 449390
    const std::int32_t octant=driving::x87_ftol32(driving::x87_abs(X(yaw_degrees))*X(0.02222222276031971f));
    const X base=(octant%2==1)?X(45.0f)-X(within):X(within);
    // fldlg2 / fyl2x: log10 of (height * 0.1 + 1), as the translator evaluates it.
    const X lg=X(X(0.301029995663981195213738894724493027L).v*std::log2((X(height)*X(0.1f)+X(1.0f)).v));
    const X soften=(X(1.0f)-lg)*base;
    const X y=X(yaw);
    bool plus;
    if(y>=X(1.5707963705062866f))plus=false;                        // fcomp: C0 clear (unordered sets C0)
    else if(!(y<X(0.0f)))plus=true;                                  // >= 0 or unordered
    else plus=y<X(-1.5707963705062866f);
    const X turned=plus?X(yaw)*degrees+soften:X(yaw)*degrees-soften;
    auto& turn=p.fl.g.shader().unknown_95aef8[2];                    // 95AFDC
    turn=render_state::bits(st(turned));
    const X t=X(render_state::f32(turn))-X(float(u16(0x26)))*X(0.0054931640625f);
    turn=render_state::bits(st(t));
    const M roll=rotation(2,st(t*X(0.01745329238474369f)));
    p.set_current(driving::pc_d3dx_matrix_multiply(roll,p.current()));
    const M projection=p.current();
    render_state::store(tf.slot[1],projection);                      // 95D8A0 (no 410F90)
    p.set_current(M{1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1});
    p.set_current(driving::pc_d3dx_matrix_multiply(translation(f(6),f(7),f(8)),p.current()));
    driving::pc_matrix_push_unit(s);
    const X angle=X(9.58738019107841e-05f);
    p.set_current(driving::pc_d3dx_matrix_multiply(rotation(1,st(X(float(u16(0x26)))*angle)),p.current()));
    p.set_current(driving::pc_d3dx_matrix_multiply(rotation(0,st(X(float(u16(0x24)))*angle)),p.current()));
    p.set_current(driving::pc_d3dx_matrix_multiply(rotation(2,st(X(float(u16(0x28)))*angle)),p.current()));
    {const auto v=driving::pc_d3dx_vec3_transform_coord({offset[0],offset[1],offset[2]},p.current());offset[0]=v[0];offset[1]=v[1];offset[2]=v[2];}
    driving::pc_matrix_pop(s);
    p.set_current(driving::pc_d3dx_matrix_multiply(translation(offset[0],offset[1],offset[2]),p.current()));
    {const auto v=driving::pc_d3dx_vec3_transform_coord({eye[0],eye[1],eye[2]},p.current());eye[0]=v[0];eye[1]=v[1];eye[2]=v[2];}
    {const auto v=driving::pc_d3dx_vec3_transform_coord({at[0],at[1],at[2]},p.current());at[0]=v[0];at[1]=v[1];at[2]=v[2];}
    driving::pc_d3dx_look_at_rh(s.current(),{eye[0],eye[1],eye[2]},{at[0],at[1],at[2]},{up[0],up[1],up[2]});
    const M view=p.current();
    render_set_matrix_410f90(p.fl,view,0);                            // 95D860, 410FF0
    p.set_current(M{1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1});
    p.set_current(driving::pc_d3dx_matrix_multiply(projection,p.current()));
    p.set_current(driving::pc_d3dx_matrix_multiply(view,p.current()));
    if(out)*out=p.current();
    driving::pc_matrix_pop(s);
}
// 422820 (light matrix out = work+B0): saves the viewport, render target
// and depth surface and keeps the view and projection under two stack
// pushes, renders into the 512x512 shadow texture (cleared), then the
// shadow camera of the player car (the light record 751B2C with the car's
// position and angles); colour writes off, cull CW.
bool shadow_map_422820(CoursePass& p,std::uint32_t out_address){
    auto& st_=state();auto& d=p.d;auto& s=p.s;auto& g=p.fl.g;
    const bool ending=p.r.current_mode()==0x18u;
    driving::Bytes vehicle(nullptr,0),camera(nullptr,0);
    if(!ending&&!(p.r.vehicle_camera&&p.r.vehicle_camera(vehicle,camera)&&vehicle.size()))return false;
    if(!st_.data_loaded)return false;
    std::uint32_t viewport[6];d.get_viewport(viewport);std::memcpy(st_.viewport_955a44.data(),viewport,0x18);
    put32(st_.targets_95afd4.data(),d.get_render_target(0));
    put32(st_.targets_95afd4.data()+4,d.get_depth_stencil_surface());
    {   // two pushes: the old slots keep the view, then the projection
        std::int32_t depth=s.depth+1;std::ptrdiff_t at=s.current_offset;
        if(depth<s.capacity){copy_matrix(slot(s,at+64),slot(s,at).data());copy_matrix(slot(s,at),g.transform().slot[0]);at+=64;s.current_offset=at;}
        ++depth;s.depth=depth;
        if(depth<s.capacity){copy_matrix(slot(s,at+64),slot(s,at).data());copy_matrix(slot(s,at),g.transform().slot[1]);at+=64;s.current_offset=at;}
    }
    d.set_render_target(0,g.w(0x95afc4));
    d.set_depth_stencil_surface(g.w(0x95afcc));
    d.clear(7,0,1.0f,0);
    const std::uint32_t shadow_viewport[6]{0,0,0x200,0x200,0,0x3f800000u};
    d.set_viewport(shadow_viewport);
    render_reset_states_408880(p.fl,p.r.frame().layer_7d25f0);
    std::uint8_t light[0x2c];
    if(ending){
        // The arcade ending: the second record 751B58 over the midpoint of
        // the two characters' bone 15 (events 0x16A / 0x16B, 487BE0).
        std::memcpy(light,st_.data_751b2c.data()+(0x751b58u-0x751b2cu),0x2c);
        driving::CourseProbe a{},b{};
        const auto& slots=p.c.event_state.slots;
        (void)native_robot_bone_position_487be0(p.c,slots[0x16a].work_token,0xf,s,a);
        (void)native_robot_bone_position_487be0(p.c,slots[0x16b].work_token,0xf,s,b);
        const float mid[3]{st((X(b.x)+X(a.x))*X(0.5f)),st((X(b.y)+X(a.y))*X(0.5f)),st(X(st(X(b.z)+X(a.z)))*X(0.5f))};
        std::memcpy(light+0x18,mid,12);
    }else{
        std::memcpy(light,st_.data_751b2c.data(),0x2c);
        std::memcpy(light+0x18,vehicle.data()+0x14,12);                   // car position
        std::memcpy(light+0x24,vehicle.data()+0x2c,6);                    // car angles
    }
    M out;
    shadow_camera_422ac0(p,out_address?&out:nullptr,light);
    if(out_address){
        const std::uint32_t at=out_address-PcSceneEffectsState::WorkBase;
        auto& w=p.c.race_effects.scene.work;
        if(out_address<PcSceneEffectsState::WorkBase||std::size_t(at)+64>w.size())throw PcRaceUnmapped(out_address,64);
        std::memcpy(w.data()+at,out.data(),64);
    }
    put32(st_.data_751b2c.data()+(0x751b94u-0x751b2cu),0);
    put32(st_.data_751b2c.data()+(0x751b98u-0x751b2cu),0x3f000000u);
    if(d.get_render_state(0xa8)!=0u)d.set_render_state(0xa8,0);
    if(d.get_render_state(0x16)!=2u)d.set_render_state(0x16,2);
    return true;
}
// 422F20: colour writes back on, the saved target, depth surface and
// viewport restored (references released), the projection and view back
// from the stack, render states reset.
void shadow_map_end_422f20(CoursePass& p){
    auto& st_=state();auto& d=p.d;auto& s=p.s;auto& g=p.fl.g;
    if(d.get_render_state(0xa8)!=0xfu)d.set_render_state(0xa8,0xf);
    std::uint32_t target,depth;std::memcpy(&target,st_.targets_95afd4.data(),4);std::memcpy(&depth,st_.targets_95afd4.data()+4,4);
    d.set_render_target(0,target);
    d.set_depth_stencil_surface(depth);
    if(target){d.release(target);put32(st_.targets_95afd4.data(),0);}
    if(depth){d.release(depth);put32(st_.targets_95afd4.data()+4,0);}
    std::uint32_t viewport[6];std::memcpy(viewport,st_.viewport_955a44.data(),0x18);d.set_viewport(viewport);
    std::ptrdiff_t at=s.current_offset;
    if(--s.depth>=0){
        at-=64;
        std::memcpy(g.transform().slot[1],slot(s,at).data(),64);        // 95D8A0
        s.current_offset=at;
        copy_matrix(slot(s,at),slot(s,at+64).data());
    }
    if(--s.depth>=0){
        at-=64;s.current_offset=at;
        M view;std::memcpy(view.data(),slot(s,at).data(),64);
        render_set_matrix_410f90(p.fl,view,0);                          // 95D860, 410FF0
        copy_matrix(slot(s,at),slot(s,at+64).data());
    }
    render_reset_states_408880(p.fl,p.r.frame().layer_7d25f0);
}
// ---- sun and lens flare (bit 1, layer 8): 40CBC0 on the object at work+90 ----
// PC statics of the pass (.data / .bss).
struct SunState {
    std::uint32_t visibility_73e2ac{0x2cec};   // 73E2AC (.data 11500): occlusion samples of a fully visible sun
    std::uint8_t first_73e2b0{1};              // 73E2B0 (.data 1): first visible frame
    float previous_89bd48[3]{};                // 89BD48 sun position 899C70 of the last direction update
    std::uint32_t samples_89f684{};            // 89F684 last query result
    std::uint32_t saved_89f670[4]{};           // 89F670 states 7 / E / F / A8 around the occlusion draw (411560)
    std::uint32_t count_89f66c{};              // 89F66C occlusion records (at most 1)
    struct Record {std::uint32_t handle{},arg{};M matrix{};} record_89f690;
};
SunState& sun_state(){static SunState s;return s;}
float fbits(std::uint32_t bits){float v;std::memcpy(&v,&bits,4);return v;}
// lens_flare_offset.bin (44A630 at device setup: the file read over a
// cleared table, then its first 0xF0 bytes copied to 7D2B18).
void load_flare_table(NativeRuntimeContext& c){
    auto& st_=state();
    if(st_.flare_loaded)return;
    st_.flare_loaded=true;
    if(auto* store=c.event_function36.retail_assets){
        std::string error;
        if(const auto* f=retail_asset_guest_path(*store,"\\common\\lens_flare_offset.bin",false,&error))
            std::memcpy(st_.flare_7d2938.data(),f->data(),std::min<std::size_t>(f->size(),0x420u));
    }
    std::memmove(st_.flare_7d2938.data()+(0x7d2b18u-0x7d2938u),st_.flare_7d2938.data(),0xf0);
}
struct Sun {
    CoursePass& p;
    driving::Bytes car,cam;
    float intensity_89bd44{};
    std::uint32_t manager(std::uint32_t pc,std::uint32_t arg){
        std::uint32_t v=0;
        if(!native_race_manager_call(p.c,pc,&arg,1,v,false))throw std::runtime_error("sun pass: race manager accessor");
        return v;
    }
    std::uint32_t stage_44dc50(std::uint32_t key){return manager(0x44dc50u,key);}
    // 44C830: the current area ([[7D3188]+4], 0xF without a course record).
    std::uint32_t area_44c830(){
        auto& m=native_race_area_memory(p.c,&p.r,false);
        const std::uint32_t record=m.u32(0x7d3188u);
        return record?m.u32(record+4):0xfu;
    }
    driving::Bytes lights(){auto& l=p.r.environment().lights_899b98;return driving::Bytes(l.data(),l.size());}
    float light(std::uint32_t address){return lights().f32(address-0x899b98u);}
    void put_light(std::uint32_t address,float v){lights().putf(address-0x899b98u,v);}
    std::uint8_t* object(std::uint32_t address,std::size_t bytes){return const_cast<std::uint8_t*>(p.work(address,bytes));}
    std::int16_t i16(std::uint32_t address){std::int16_t v;std::memcpy(&v,object(address,2),2);return v;}
    void put16(std::uint32_t address,std::int16_t v){std::memcpy(object(address,2),&v,2);}
    bool paused(){return p.r.pause_780248!=0;}                                 // 43F9C0
    // 44A690(0, out): the sun angle offsets of the stage from
    // lens_flare_offset.bin ({pitch, yaw} by stage and by camera
    // 34A < 2), the area's pair when the car's +5C is set (blended over the
    // environment transition 7D2934 / 7D28D8 in phase 2, held by 7D2D74),
    // then towards the other camera's pair by +364.
    std::array<float,2> offsets_44a690(){
        auto& st_=state();
        auto table=[&](std::uint32_t index,unsigned k){float v;std::memcpy(&v,st_.flare_7d2938.data()+index*8u+k*4u,4);return v;};
        const std::uint32_t near=cam.i8(0x34a)<2?1u:0u;
        const std::uint32_t stage=stage_44dc50(manager(0x450380u,8));
        const float ax=table(near+stage*2,0),ay=table(near+stage*2,1);
        float x=ax,y=ay;
        auto& flag=*reinterpret_cast<std::uint32_t*>(st_.flag_7d2d74.data());
        if(car.u32(0x5c)==0u){flag=0;}
        else{
            const std::uint32_t area=stage_44dc50(area_44c830());
            const float bx=table(near+area*2,0),by=table(near+area*2,1);
            const std::int32_t phase=std::int32_t(p.r.environment().phase_7d28c8);
            if(phase>=0&&phase<=1){flag=1;}
            else if(phase==2){
                const float t=float(p.r.environment_time_7d2934())/p.r.environment_duration_7d28d8();
                const float u=1.0f-t;
                x=bx*u+ax*t;y=by*u+ay*t;
            }else if(flag){x=bx;y=by;}
        }
        const float k=cam.f32(0x364);
        if(k>0.0f){
            const std::uint32_t other=cam.i8(0x34b)<2?1u:0u;
            if(near!=other){
                const float cx=table(other+stage*2,0),cy=table(other+stage*2,1);
                const float u=1.0f-k;
                x=x*u+cx*k;y=y*u+cy*k;
            }
        }
        return {x,y};
    }
    // 4056D0(handle, value, 0, -1) under the current matrix.
    void draw(std::uint32_t handle,float value){
        if(!native_race_bank_ensure(p.c,p.r,handle>>16))throw std::runtime_error("sun pass: 4056D0 bank not loaded");
        render_object_override_4056d0(p.r.queue_context(),handle,render_state::bits(value),0,-1);
    }
    // 40C980: the object scaled by the intensity.
    void draw_40c980(std::uint32_t handle,float k){draw(handle,st(X(intensity_89bd44)*X(k)));}
    // 40C9A0: one flare sprite: the point `along` the view axis at the sun
    // (the current matrix), through the camera's view (+140) into the
    // camera's screen space (+1C0), drawn there at `size`.
    void flare_40c9a0(std::uint32_t handle,float along,float size,float k){
        auto& s=p.s;
        driving::pc_matrix_push(s);
        driving::pc_matrix_translate_vector(s,{0.0f,0.0f,along});
        auto v=driving::pc_d3dx_vec3_transform_coord({0.0f,0.0f,0.0f},p.current());
        driving::pc_matrix_load(s,cam.sub(0x140,64));
        v=driving::pc_d3dx_vec3_transform_coord(v,p.current());
        driving::pc_matrix_load(s,cam.sub(0x1c0,64));
        driving::pc_matrix_translate_vector(s,{v[0],v[1],v[2]});
        M scale{};driving::pc_d3dx_matrix_scaling(driving::Bytes(scale.data(),64),size,size,1.0f);
        driving::pc_matrix_multiply_current(s,driving::Bytes(scale.data(),64));
        draw(handle,st(X(intensity_89bd44)*X(k)));
        driving::pc_matrix_pop(s);
    }
    // 40CAE0: the current matrix turned so its view axis follows `w`.
    void face_40cae0(const float w[3]){
        auto& s=p.s;
        const auto flat=driving::pc_d3dx_vec3_normalize({w[0],0.0f,w[2]});
        car_reflection_align_40a6d0(s,{flat[0],flat[1],flat[2]},{0.0f,0.0f,1.0f});
        const float length=st(driving::x87_sqrt(X(w[2])*X(w[2])+X(w[0])*X(w[0])));
        driving::pc_matrix_rotate_z(s,fbits(0x3fc90fdbu));                       // pi/2
        const auto up=driving::pc_d3dx_vec3_normalize({w[1],0.0f,length});
        car_reflection_align_40a6d0(s,{up[0],up[1],up[2]},{0.0f,0.0f,1.0f});
        driving::pc_matrix_rotate_z(s,fbits(0x4096cbe4u));                       // 3pi/2
    }
    // 406810 / 406870 on render state 7 (z enable) through the state shadow 8606F0.
    void z_save_406810(std::uint32_t want){
        auto& sh=native_sprite2d(p.c).state;
        const auto v=p.d.get_render_state(7);
        if(v!=want)p.d.set_render_state(7,want);
        sh.shadow_1c=v;sh.shadow_63f=1;
    }
    void z_restore_406870(){
        auto& sh=native_sprite2d(p.c).state;
        if(p.d.get_render_state(7)!=sh.shadow_1c)p.d.set_render_state(7,sh.shadow_1c);
        sh.shadow_63f=0;
    }
    void pass_record(std::uint32_t mode){                                      // 897D48..897D5C
        auto& g=p.fl.g;
        g.w(0x897d48)=1;g.w(0x897d58)=0;g.w(0x897d5c)=7;g.w(0x897d4c)=1;g.w(0x897d50)=0;g.w(0x897d54)=mode;
    }
    void scale(float k){
        M m{};driving::pc_d3dx_matrix_scaling(driving::Bytes(m.data(),64),k,k,k);
        driving::pc_matrix_multiply_current(p.s,driving::Bytes(m.data(),64));
    }
    // IDirect3DQuery9::GetData on the occlusion query 89F680 (no device
    // query: always S_FALSE).
    std::uint32_t query_get_data(std::uint32_t& samples){
        auto& st_=state();
        if(!st_.query)return 1u;
        return p.d.query_get_data(st_.query,samples);
    }
    // 4113E0(ECX = handle, arg): records the object and the current matrix.
    void record_4113e0(std::uint32_t handle,std::uint32_t arg){
        auto& sun=sun_state();
        if(sun.count_89f66c>=1u)return;
        sun.record_89f690.handle=handle;sun.record_89f690.arg=arg;sun.record_89f690.matrix=p.current();
        ++sun.count_89f66c;
    }
    // 4114E0(EAX = handle): the object drawn inside the occlusion query
    // once the previous result is in.
    void occlusion_draw_4114e0(std::uint32_t handle){
        auto& st_=state();auto& d=p.d;
        std::uint32_t samples=0;
        if(query_get_data(samples)==1u)return;
        if(st_.query)d.query_issue(st_.query,2);                          // D3DISSUE_BEGIN
        const std::uint32_t resource=handle>>16;
        if(!native_race_bank_ensure(p.c,p.r,resource))throw std::runtime_error("sun pass: 448810 bank not loaded");
        auto* bank=p.r.bank_resources(resource);
        if(!bank)throw std::runtime_error("sun pass: 448810 bank not loaded");
        if(pmt_object_draw_4116f0(p.fl,*bank,handle&0xffffu,p.s))d.set_pixel_shader(0);
        if(st_.query)d.query_issue(st_.query,1);                          // D3DISSUE_END
    }
    // 411420: the recorded objects drawn for the query with z on, z writes,
    // alpha blending and colour writes off, world matrix = current (411060),
    // then 4115C0 restores the four states.
    void occlusion_411420(){
        auto& sun=sun_state();auto& d=p.d;
        if(sun.count_89f66c==0u)return;
        static constexpr std::uint32_t states[4]{7,0xe,0xf,0xa8};
        for(unsigned k=0;k<4;++k)sun.saved_89f670[k]=d.get_render_state(states[k]);          // 411560
        d.set_render_state(7,1);d.set_render_state(0xe,0);d.set_render_state(0xf,0);d.set_render_state(0xa8,0);   // 4116A0
        render_set_matrix_410f90(p.fl,p.current(),6);                                         // 95D9E0, 411060
        driving::pc_matrix_push(p.s);
        for(std::uint32_t k=0;k<sun.count_89f66c;++k){
            p.set_current(sun.record_89f690.matrix);
            occlusion_draw_4114e0(sun.record_89f690.handle);
        }
        driving::pc_matrix_pop(p.s);
        sun.count_89f66c=0;
        for(unsigned k=0;k<4;++k)if(d.get_render_state(states[k])!=sun.saved_89f670[k])d.set_render_state(states[k],sun.saved_89f670[k]);   // 4115C0
    }
};
// 40CBC0 (EAX = the sun object at work+90: +C intensity, +10 flicker and
// +12 countdown words). The sun direction 899C7C (turned by the stage's
// lens_flare_offset.bin angles from stage 15 on) is refreshed into
// 899CCC whenever the sun or its offset moved; the sun at the eye plus
// 899CCC is projected through the camera; on screen, the visible part
// measured by the previous occlusion query sets 95AEF8 (calibrated
// against 73E2AC), the sun disc, its occlusion object, the glow and the
// flare sprites are drawn along the screen axis.
void sun_flare_40cbc0(CoursePass& p,std::uint32_t object){
    Sun sun{p,driving::Bytes(nullptr,0),driving::Bytes(nullptr,0)};
    if(!(p.r.vehicle_camera&&p.r.vehicle_camera(sun.car,sun.cam)))throw PcRaceUnmapped(0x79f574u,4);
    load_flare_table(p.c);
    auto& g=p.fl.g;auto& s=p.s;auto& ss=sun_state();
    const auto& cam=sun.cam;
    const std::int32_t stage=std::int32_t(sun.stage_44dc50(sun.manager(0x450380u,8)));
    sun.intensity_89bd44=fbits(p.work_u32(object+0xc));
    const float eps=fbits(0x34000000u);
    auto moved=[&](float v){return driving::x87_abs(X(v))>X(eps);};           // 449390, fcomp 6281F0
    if(moved(st(X(cam.f32(0x358))-X(cam.f32(0x35c))))
       ||moved(st(X(sun.light(0x899c70))-X(ss.previous_89bd48[0])))
       ||moved(st(X(sun.light(0x899c74))-X(ss.previous_89bd48[1])))
       ||moved(st(X(sun.light(0x899c78))-X(ss.previous_89bd48[2])))
       ||moved(sun.light(0x899ccc))||moved(sun.light(0x899cd0))||moved(sun.light(0x899cd4))){
        const float v[3]{sun.light(0x899c7c),sun.light(0x899c80),sun.light(0x899c84)};
        float pitch,yaw;yaw_pitch_44a480(v,pitch,yaw);
        if(stage>=0xf){
            const auto offset=sun.offsets_44a690();
            pitch=st(X(pitch)+X(offset[0]));
            yaw=st(X(yaw)+X(offset[1]));
        }else pitch=st(X(pitch)*X(cam.f32(0x358)));
        const auto dir=driving::environment_direction_44a430(pitch,yaw,s);
        const float range=sun.light(0x899cc4);
        sun.put_light(0x899ccc,st(-(X(range)*X(dir.x))));
        sun.put_light(0x899cd0,st(-(X(range)*X(dir.y))));
        sun.put_light(0x899cd4,st(-(X(range)*X(dir.z))));
    }
    for(unsigned k=0;k<3;++k)ss.previous_89bd48[k]=sun.light(0x899c70+k*4);
    // The sun position: the eye (4493E0 blend of +D4 and +3A8) plus 899CCC.
    driving::PcCameraBlend blend{p.r.interpolation_override_82e7d8,std::uint8_t(p.r.events()?p.r.events()->slots[6].flags:0u),0,1.0f};
    if((blend.owner_flags_79fb4e&3u)==2u&&p.r.autoscene_work_799ca0)std::memcpy(&blend.owner_mode_799ca0_1c,p.r.autoscene_work_799ca0+0x1c,4);
    const float t=driving::camera_blend_4493e0(blend);
    const X rest=X(1.0f)-X(t);
    const float a0=st(X(t)*X(cam.f32(0xd4))),a1=st(X(t)*X(cam.f32(0xd8))),a2=st(X(t)*X(cam.f32(0xdc)));
    const float b0=st(rest*X(cam.f32(0x3a8))),b1=st(rest*X(cam.f32(0x3ac)));
    const X eye_x=X(b0)+X(a0);
    const float eye_y=st(X(b1)+X(a1)),eye_z=st(rest*X(cam.f32(0x3b0))+X(a2));
    const float S[3]{st(X(sun.light(0x899ccc))+eye_x),st(X(sun.light(0x899cd0))+X(eye_y)),st(X(sun.light(0x899cd4))+X(eye_z))};
    const X fov=X(cam.f32(0xa0))*X(fbits(0x42652ee0u))*X(0.01f);                 // degrees / 100
    float size=st(fov);                                                          // F14
    float spread=st(fov*X(0.13f));                                               // F10
    driving::pc_matrix_push_load(s,cam.sub(0x140,64));                          // the view
    const auto P=driving::pc_d3dx_vec3_transform_coord({S[0],S[1],S[2]},p.current());
    if(sun.i16(object+0x12)>0&&!sun.paused())sun.put16(object+0x12,std::int16_t(sun.i16(object+0x12)-1));
    if(X(P[2])<X(0.0f)){
        // 449940 under the view: the screen position (x / 740C94, y / 740C98).
        driving::pc_matrix_push_load(s,cam.sub(0x140,64));
        float sx=0,sy=0;
        {
            const auto l=driving::pc_matrix_point(s,{S[0],S[1],S[2]});
            if(!(X(eps)>driving::x87_abs(X(l.z)))){
                const float inv=1.0f/(0.0f-l.z);
                sx=(inv*cam.f32(0xb0))*l.x;
                sy=l.y*(inv*cam.f32(0xb4));
            }
        }
        driving::pc_matrix_pop(s);
        sx=st(X(sx)/X(1.0f));                                                    // 740C94 = 1.0
        sy=st(X(sy)/X(g.f(0x740c98)));
        if(!(X(sx)>X(-470.0f)&&X(sx)<X(470.0f)&&X(sy)>X(-440.0f)&&X(sy)<X(440.0f))){
            sun.put16(object+0x10,0);
        }else{
            render_light_ambient_410710(p.fl,{sun.light(0x899c10),sun.light(0x899c14),sun.light(0x899c18),1.0f});
            render_lights_reset_410740(p.fl);
            render_light_add_4107a0(p.fl,sun.lights().sub(0x899cd8u-0x899b98u,0xa0));
            auto sub=[](const float* a,const float* b,float* o){for(unsigned k=0;k<3;++k)o[k]=st(X(a[k])-X(b[k]));};   // 40EFA0
            const float eye[3]{cam.f32(0xf8),cam.f32(0xfc),cam.f32(0x100)};
            const float target[3]{cam.f32(0x104),cam.f32(0x108),cam.f32(0x10c)};
            float view_dir[3],sun_dir[3];
            sub(target,eye,view_dir);sub(S,eye,sun_dir);
            const float glow_offset[3]{0.0f,0.0f,-0.2f};                         // F68
            float bright=fbits(0x3f7d70a4u);                                     // 0.99, F18
            const float far=stage==0x17?1300.0f:2500.0f;                         // F44
            // Fade near the screen edges (100 to 470 across, 100 to 520 down).
            if(X(sx)<X(-100.0f))bright=st((X(sx)+X(470.0f))*X(fbits(0x3b311fd4u)));
            else if(X(sx)>X(100.0f))bright=st((X(470.0f)-X(sx))*X(fbits(0x3b311fd4u)));
            if(X(sy)<X(-100.0f))bright=st((X(sy)+X(520.0f))*X(bright)*X(fbits(0x3b40c0c1u)));
            else if(X(sy)>X(100.0f))bright=st((X(520.0f)-X(sy))*X(bright)*X(fbits(0x3b40c0c1u)));
            const float edge=bright;                                             // F40 / F3C
            float flare=0.65f;                                                   // F34
            if(sun.i16(object+0x10)>0){                                          // flicker countdown: odd frames dark
                if(!sun.paused())sun.put16(object+0x10,std::int16_t(sun.i16(object+0x10)-1));
                if(*sun.object(object+0x10,1)&1u){flare=0.0f;bright=0.0f;}
            }
            flare=st(X(bright)*X(flare));
            const auto vd=driving::pc_d3dx_vec3_normalize({view_dir[0],view_dir[1],view_dir[2]});
            const auto sd=driving::pc_d3dx_vec3_normalize({sun_dir[0],sun_dir[1],sun_dir[2]});
            const float facing=st((X(vd[0])*X(sd[0])+X(vd[1])*X(sd[1]))+X(vd[2])*X(sd[2]));   // F48
            // The previous occlusion result over the calibration 73E2AC -> 95AEF8.
            std::uint32_t samples=0;
            if(sun.query_get_data(samples)==1u)samples=ss.samples_89f684;
            else ss.samples_89f684=samples;
            auto unsigned_x87=[](std::uint32_t v){X r=X(double(std::int32_t(v)));if(std::int32_t(v)<0)r=r+X(4294967296.0f);return r;};
            const X ratio=unsigned_x87(samples)/unsigned_x87(ss.visibility_73e2ac);
            g.putf(0x95aef8,st(ratio));
            if(ratio>X(0.22f)){
                if(ss.first_73e2b0){ss.first_73e2b0=0;}
                else{
                    const std::uint32_t n=std::uint32_t(driving::x87_ftol32((X(g.f(0x95aef8))-X(0.22f))*X(5000.0f)));   // 582194
                    ss.visibility_73e2ac+=n>10000u?10000u:n>500u?n:500u;
                }
                g.putf(0x95aef8,0.22f);
            }else if(X(g.f(0x95aef8))<X(0.0f))g.putf(0x95aef8,0.0f);
            const X inv_z=X(1.0f)/X(P[2]);
            const float axis[3]{st(X(P[0])*inv_z*X(-1.0f)),st(X(P[1])*inv_z*X(-1.0f)),-1.0f};   // the sun on the z = -1 plane
            float glow_dir[3];sub(glow_offset,axis,glow_dir);
            driving::pc_matrix_push_load(s,cam.sub(0x1c0,64));                  // screen space
            if(X(spread)>X(0.0f)){
                // The sun disc (z off), then its occlusion object for the next
                // frame's query and the sun objects at the far distance.
                sun.pass_record(2);
                driving::pc_matrix_push(s);
                driving::pc_matrix_translate_vector(s,{axis[0],axis[1],axis[2]});
                sun.scale(st(X(g.f(0x95aef8))*X(0.1f)));
                sun.z_save_406810(0);
                sun.draw_40c980(0x570002u,g.f(0x95aef8));
                sun.z_restore_406870();
                driving::pc_matrix_pop(s);
                p.r.queue_context().immediate_8999b0=0;
                driving::pc_matrix_push(s);
                float at[3];for(unsigned k=0;k<3;++k)at[k]=st(X(far)*X(axis[k]));   // 40F050
                driving::pc_matrix_translate_vector(s,{at[0],at[1],at[2]});
                spread=st(X(far)*X(spread));
                sun.scale(spread);
                driving::pc_matrix_push(s);
                sun.scale(0.25f);
                sun.record_4113e0(0x570010u,0);
                driving::pc_matrix_pop(s);
                sun.occlusion_411420();
                sun.draw_40c980(0x570010u,1.0f);
                sun.draw_40c980(0x57000fu,1.0f);
                driving::pc_matrix_pop(s);
                render_queue_flush_4052c0(p.fl,p.r.queue_context());
            }
            // The glow, then the flare sprites along the axis towards the sun.
            sun.z_save_406810(0);
            sun.pass_record(4);
            driving::pc_matrix_translate_vector(s,{glow_offset[0],glow_offset[1],glow_offset[2]});
            driving::pc_matrix_push(s);
            sun.scale(st(X(bright)*X(0.1f)));
            sun.draw_40c980(0x570006u,st(X(bright)*X(g.f(0x95aef8))));
            driving::pc_matrix_pop(s);
            sun.face_40cae0(glow_dir);
            spread=st(driving::x87_sqrt((X(glow_dir[0])*X(glow_dir[0])+X(glow_dir[1])*X(glow_dir[1]))+X(glow_dir[2])*X(glow_dir[2])));   // 40F0E0
            X ring;
            if(stage==0x1a||stage==0x19||stage==0x14){
                ring=X(edge)>X(0.999f)?X(0.999f):X(edge);
            }else{
                ring=X(edge)<X(0.4f)||std::isnan(edge)?X(edge):X(0.4f);
                if(X(facing)>X(0.92f)&&ring!=X(0.0f))ring=ring+(X(facing)-X(0.92f))*X(6.8f);
                if(ring>X(0.999f))ring=X(0.999f);
            }
            p.r.queue_context().immediate_8999b0=0;
            spread=st(X(spread)*X(facing)*X(0.4f));
            size=st(X(size)*X(0.4f));
            const float visible=g.f(0x95aef8);
            flare=st(X(flare)*X(visible));
            bright=st(X(bright)*X(visible));
            const float glow=st(ring*X(visible));
            sun.flare_40c9a0(0x57000eu,st(X(spread)*X(-1.7f)),st(X(size)*X(0.18f)),flare);
            const float small=st(X(size)*X(0.15f));
            sun.flare_40c9a0(0x570003u,st(X(spread)*X(-0.52f)),small,flare);
            sun.flare_40c9a0(0x570004u,st(X(spread)*X(-0.47f)),small,flare);
            sun.flare_40c9a0(0x570005u,st(X(spread)*X(-0.15f)),small,flare);
            sun.flare_40c9a0(0x570007u,st(X(spread)*X(0.12f)),st(X(size)*X(0.1f)),bright);
            sun.flare_40c9a0(0x570008u,st(X(spread)*X(0.16f)),st(X(size)*X(0.09f)),flare);
            sun.flare_40c9a0(0x57000au,st(X(spread)*X(0.17f)),st(X(size)*X(0.06f)),flare);
            sun.flare_40c9a0(0x570009u,st(X(spread)*X(0.18f)),st(X(size)*X(0.11f)),flare);
            sun.flare_40c9a0(0x57000bu,st(X(spread)*X(0.21f)),st(X(size)*X(0.12f)),flare);
            sun.flare_40c9a0(0x57000cu,st(X(spread)*X(0.27f)),st(X(size)*X(0.05f)),flare);
            sun.flare_40c9a0(0x57000du,st(X(spread)*X(0.3f)),st(X(size)*X(0.03f)),flare);
            render_queue_flush_4052c0(p.fl,p.r.queue_context());
            sun.flare_40c9a0(0x570008u,fbits(0xbb449ba6u),1.5f,glow);
            render_pass_defaults_404540(g,p.r.frame().layer_7d25f0);
            sun.z_restore_406870();
            driving::pc_matrix_pop(s);
            render_light_ambient_410710(p.fl,{sun.light(0x899d50),sun.light(0x899d54),sun.light(0x899d58),1.0f});
            render_lights_reset_410740(p.fl);
            render_light_add_4107a0(p.fl,sun.lights().sub(0x899cd8u-0x899b98u,0xa0));
        }
    }
    driving::pc_matrix_pop(s);
}
bool native_pass(NativeRuntimeContext& c,PcSceneRenderer& r,std::uint32_t pc,std::uint32_t arg){
    CoursePass p(c,r);
    switch(pc){
    case 0x40bf10u:rain_40bf10(p,2,arg);return true;
    case 0x40c1b0u:shadow_40c1b0(p,3,arg);return true;
    case 0x40c550u:headlight_40c550(p,3,arg);return true;
    case 0x40c150u:rain_end_40c150(p,2);return true;
    case 0x40c4a0u:stage_end(p,3);r.frame().course_73e2a8=0xffffffffu;return true;
    case 0x40c8d0u:stage_end(p,3);return true;
    case 0x422820u:return shadow_map_422820(p,arg);
    case 0x422f20u:shadow_map_end_422f20(p);return true;
    case 0x40cbc0u:sun_flare_40cbc0(p,arg);return true;
    default:return false;
    }
}
}
NativeCoursePassStats& native_course_pass_stats(){return stats_;}
std::uint32_t native_course_pass_work(NativeRuntimeContext& c){return c.race_effects.scene.work_pointer_79f5ec;}
std::uint32_t native_course_pass_flags(NativeRuntimeContext& c){
    if(!c.race_effects.scene.work_pointer_79f5ec)return 0u;
    std::uint32_t f;std::memcpy(&f,c.race_effects.scene.work.data(),4);return f;
}
bool native_course_pass_leaf(NativeRuntimeContext& c,PcSceneRenderer& r,std::uint32_t pc,std::uint32_t arg){
    switch(pc){
    case 0x40bf10u:case 0x40c1b0u:case 0x40c550u:case 0x422820u:
    case 0x40c150u:case 0x40c8d0u:case 0x40c4a0u:case 0x40cbc0u:case 0x422f20u:break;
    default:return false;
    }
    static const bool disabled=std::getenv("OR2_NOCOURSE")!=nullptr;   // host comparison switch
    if(disabled)return false;
    auto& st=state();
    if(!st.data_loaded)st.data_loaded=exe_bytes(0x751b2cu,st.data_751b2c.data(),st.data_751b2c.size());   // 751B2C (.data)
    // The occlusion query 89F680 (41776A creates it with the device).
    auto& dev=r.device();
    if(st.query_device!=&dev){st.query_device=&dev;st.query=dev.create_occlusion_query();}
    // 422820 without the car or the light records draws nothing: its 422F20
    // has nothing to restore either.
    if(pc==0x422f20u&&st.shadow_skipped){st.shadow_skipped=false;return true;}
    try{
        if(pc==0x422820u)st.shadow_skipped=false;
        const bool done=native_pass(c,r,pc,arg);
        if(pc==0x422820u)st.shadow_skipped=!done;
        if(done){
            ++stats_.runs;
            if(std::getenv("OR2_COURSE_DEBUG")&&(stats_.runs<12u||stats_.runs%220u==1u))
                std::fprintf(stderr,"[course] runs %u native pc %08x flags %08x\n",stats_.runs,pc,native_course_pass_flags(c));
        }
        return true;
    }catch(const PcRaceUnmapped& u){
        char t[96];std::snprintf(t,sizeof t,"course pass %08X: unmapped PC address %08X",pc,u.address);stats_.last_error=t;
    }catch(const std::exception& e){
        char t[32];std::snprintf(t,sizeof t,"course pass %08X: ",pc);stats_.last_error=t+std::string(e.what());
    }
    ++stats_.failures;
    if(std::getenv("OR2_COURSE_DEBUG")&&stats_.failures<20u)std::fprintf(stderr,"[course] %s\n",stats_.last_error.c_str());
    return true;
}
}
