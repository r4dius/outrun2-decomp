#include "system/perf.hpp"
#include <chrono>
#include <cstdio>
#include "platform/pc_scene_renderer.hpp"
#include "enhancements/frame_rate.hpp"
#include "driving/pc_d3dx.hpp"
#include <memory>
#include "platform/pc_headlight_pool.hpp"
#include "platform/pc_address_view.hpp"
#include "platform/pc_shadow_volume.hpp"
#include "platform/pc_vertex_shader_setup.hpp"
#include "platform/embedded_camera_data.hpp"
#include <cstring>
#include <cstdlib>
#include <stdexcept>
namespace outrun::platform {
using driving::Bytes;
PcSceneRenderer::PcSceneRenderer(PcD3D9Device& device):device_(device),matrix_arena_(64u*64u),
    matrices_{Bytes(matrix_arena_.data(),matrix_arena_.size()),0,0,64}{
    flush_=std::make_unique<PcFlushContext>(PcFlushContext{device_,globals_,
        [this](std::uint32_t r)->PcPmtResources*{auto it=banks_.find(r);return it==banks_.end()?nullptr:it->second;},
        pc_colour_list_alt_408c80,1});
    driving::pc_matrix_identity(matrices_);
    queue_=std::make_unique<PcRenderContext>(PcRenderContext{opaque_,alpha_,view_,matrices_,
        [this](std::uint32_t r)->const PcModelBank*{auto it=owned_banks_.find(r);return it==owned_banks_.end()?nullptr:&it->second->model;},7});
    camera_live_fe8_.resize(0x64c);camera_live_bb0_.resize(0x438);camera_live_750_.resize(0x5460);
    queue_->immediate_8999b0=1;
    queue_->flush_4052c0=[this]{render_queue_flush_4052c0(*flush_,*queue_);};
    // 40E4B0 -> 404250 once the device exists (render/stage defaults, 40F6B0,
    // 405160, fixed pixel shaders, normalisation cube map, shadow targets).
    renderer_init_gaps=renderer_init_404250(*flush_,*queue_,frame_.layer_7d25f0);
    frame_.device_ready_7d2614=1;
}
PcSceneRenderer::~PcSceneRenderer(){
    for(const auto& swap:pending_swaps_406630)device_.release(swap.second);
}
void PcSceneRenderer::flush_alpha(){
    OR2_PERF_ZONE("405830 alpha flush");
    std::vector<std::uint32_t> order(alpha_.count);
    for(std::uint32_t k=0;k<alpha_.count;++k)order[k]=k;
    if(alpha_.count){OR2_PERF_ZONE("4499E0 alpha sort");render_queue_sort_4499e0(alpha_,order,0,std::int32_t(alpha_.count)-1);}
    render_queue_flush_405890(*flush_,alpha_,order);
    alpha_.count=0;alpha_.matrix_count=0;++alpha_flushes;
}
std::unique_ptr<PcSceneRenderer::Bank> PcSceneRenderer::open_bank(std::vector<std::uint8_t> pmt,std::string& error){
    if(pmt.size()<16){error="PMT too small";return nullptr;}
    auto bank=std::make_unique<Bank>();bank->pmt=std::move(pmt);
    Bytes pb(bank->pmt.data(),bank->pmt.size());
    const auto system=pb.u32(8),video=pb.u32(12);
    if(std::uint64_t(system)+video+16u>bank->pmt.size()){error="PMT sections outside the archive";return nullptr;}
    auto& r=bank->resources;
    r.system.assign(bank->pmt.begin()+16,bank->pmt.begin()+16+system);
    r.video=bank->pmt.data()+16+system;r.video_size=video;
    return bank;
}
bool PcSceneRenderer::step_bank(PendingBank& p){
    auto& r=p.bank->resources;
    if(!p.textures){
        OR2_PERF_ZONE("bank object");
        if(!pmt_objects_step_42e4f0(r,p.cursor,device_,shader_cache_,shader_globals))return false;
        pmt_textures_begin_42e850(r,p.cursor);p.textures=true;return false;
    }
    OR2_PERF_ZONE("bank texture");
    return pmt_textures_step_42e8c0(r,p.cursor,device_);
}
bool PcSceneRenderer::queue_bank(std::uint32_t resource,std::vector<std::uint8_t> pmt,std::string& error){
    if(owned_banks_.count(resource)||bank_queued(resource)){error.clear();return true;}
    auto bank=open_bank(std::move(pmt),error);
    if(!bank)return false;
    PendingBank p;p.resource=resource;p.bank=std::move(bank);
    pmt_objects_begin_42e490(p.bank->resources,p.cursor);
    pending_banks_.push_back(std::move(p));
    error.clear();return true;
}
bool PcSceneRenderer::bank_queued(std::uint32_t resource)const{
    for(const auto& p:pending_banks_)if(p.resource==resource)return true;
    return false;
}
void PcSceneRenderer::step_banks(double budget_ms){
    if(pending_banks_.empty())return;
    OR2_PERF_ZONE("bank queue");
    const auto start=std::chrono::steady_clock::now();
    while(!pending_banks_.empty()){
        auto& p=pending_banks_.front();
        bool done=false;
        try{done=step_bank(p);}
        catch(const std::exception& e){++bank_failures;last_error="bank "+std::to_string(p.resource)+": PMT loader: "+e.what();
            pending_banks_.erase(pending_banks_.begin());continue;}
        if(done){const auto resource=p.resource;auto bank=std::move(p.bank);pending_banks_.erase(pending_banks_.begin());install_bank(resource,std::move(bank));}
        if(std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count()>=budget_ms)break;
    }
}
bool PcSceneRenderer::load_bank(std::uint32_t resource,std::vector<std::uint8_t> pmt,std::string& error){
    if(owned_banks_.count(resource)){error.clear();return true;}
    OR2_PERF_ZONE("bank load");
    PendingBank p;
    for(auto it=pending_banks_.begin();it!=pending_banks_.end();++it)
        if(it->resource==resource){p=std::move(*it);pending_banks_.erase(it);break;}   // queued: finish it now
    if(!p.bank){
        p.resource=resource;p.bank=open_bank(std::move(pmt),error);
        if(!p.bank)return false;
        pmt_objects_begin_42e490(p.bank->resources,p.cursor);
    }
    try{while(!step_bank(p)){}}
    catch(const std::exception& e){error=std::string("PMT loader: ")+e.what();return false;}
    install_bank(resource,std::move(p.bank));
    error.clear();return true;
}
void PcSceneRenderer::install_bank(std::uint32_t resource,std::unique_ptr<Bank> bank){
    auto& r=bank->resources;
    bank->model=PcModelBank{resource,r.view(),r.object_count_0c};
    banks_[resource]=&bank->resources;
    owned_banks_[resource]=std::move(bank);
    for(std::size_t i=0;i<pending_swaps_406630.size();){   // 406630 swaps asked before this bank was resident
        const auto [token,texture]=pending_swaps_406630[i];
        if((token>>16)==resource){pending_swaps_406630.erase(pending_swaps_406630.begin()+std::ptrdiff_t(i));(void)texture_swap_406630(token,texture);device_.release(texture);}
        else ++i;
    }
}
std::uint32_t PcSceneRenderer::object_shaders_4103f0(std::uint32_t handle,std::uint32_t kind){
    const auto resource=handle>>16;
    if(resource>=0x223u)throw std::out_of_range("4103F0: resource outside the 7C2800 bank table");
    auto* r=bank_resources(resource);
    if(!r){device_.set_vertex_shader(0);return 0u;}                   // 448810 +0C = 0
    return pmt_object_shaders_4103f0(*r,handle&0xffffu,kind,device_,shader_cache_,shader_globals);
}
std::uint32_t PcSceneRenderer::object_group_type_4066d0(std::uint32_t handle,std::uint32_t type){
    if(handle==0xffffffffu)return 0u;
    auto* r=bank_resources(handle>>16);
    return r?pmt_object_group_type_4066d0(*r,handle&0xffffu,type):0u;
}
std::uint32_t PcSceneRenderer::object_mesh_flags_406730(std::uint32_t handle,std::uint32_t and_mask,std::uint32_t or_mask){
    if(handle==0xffffffffu)return 0u;
    auto* r=bank_resources(handle>>16);
    return r?pmt_object_mesh_flags_406730(*r,handle&0xffffu,and_mask,or_mask):0u;
}
void PcSceneRenderer::apply_camera(const driving::PcCameraDevice& d,Bytes camera){
    for(const auto& st:d.stores){
        std::array<float,16> m{};std::memcpy(m.data(),st.matrix.data(),64);
        render_set_matrix_410f90(*flush_,m,st.slot);
    }
    // 409E00(slot): SetTransform of the same stack matrix (jump table 409E74:
    // 0 VIEW, 1 PROJECTION, 2..5 TEXTURE0..3, 6..9 WORLD..WORLD3).
    for(const auto slot:d.transforms){
        static constexpr std::uint32_t State[10]{2,3,0x10,0x11,0x12,0x13,0x100,0x101,0x102,0x103};
        if(slot>=10||slot>=d.slots_95d860.size())continue;
        std::array<float,16> m{};std::memcpy(m.data(),d.slots_95d860[slot].data(),64);
        device_.set_transform(State[slot],m.data());
    }
    view_.view_95d860=d.slots_95d860[0];
    view_.frustum_95bf40=d.frustum_95bf40;view_.planes_95bf58=d.planes_95bf58;
    for(unsigned k=0;k<3;++k){view_.eye[k]=camera.f32(0xf8+k*4);view_.target[k]=camera.f32(0x104+k*4);}
}
void PcSceneRenderer::note_camera_replay(Bytes camera,const std::vector<std::uint8_t>& before){
    auto after=std::make_shared<std::vector<std::uint8_t>>(camera.data(),camera.data()+0x3b4);
    enhancements::display_note(this,[this,camera,before,after]{
        const std::uint8_t autoscene=std::uint8_t(events_?events_->slots[6].flags:0u);
        float t=(autoscene&3u)==2u?1.f:driving::camera_blend_4493e0({interpolation_override_82e7d8,autoscene,0,enhancements::display_blend()});
        const Bytes prev(const_cast<std::uint8_t*>(before.data()),before.size()),cur(after->data(),after->size());
        auto delta=[&](std::size_t o){const float x=cur.f32(o)-prev.f32(o),y=cur.f32(o+4)-prev.f32(o+4),z=cur.f32(o+8)-prev.f32(o+8);return x*x+y*y+z*z;};
        if(delta(0xf8)>100.f||delta(0x104)>100.f)t=1.f;   // a camera cut: no blend across it
        for(std::size_t o:{0xf8u,0xfcu,0x100u,0x104u,0x108u,0x10cu})camera.putf(o,prev.f32(o)+(cur.f32(o)-prev.f32(o))*t);
        for(std::size_t o:{0x128u,0x12cu,0x130u})camera.putf(o,driving::camera_angle_lerp_449580(prev.f32(o),cur.f32(o),t));
        // 482F20 with blend 1 builds the view from these values, then the
        // inverse at +1C0 as the tail of 484BD0.
        driving::PcCameraDevice d;
        driving::camera_view_482f20(camera,matrices_,d,1.f);
        driving::pc_matrix_push_load(matrices_,camera.sub(0x140,64));
        driving::pc_d3dx_matrix_inverse(matrices_.current(),nullptr,matrices_.current());
        driving::pc_matrix_get(matrices_,camera.sub(0x1c0,64));
        driving::pc_matrix_pop(matrices_);
        apply_camera(d,camera);
    },[camera,after]{std::memcpy(camera.data(),after->data(),0x3b4);});   // the tick's camera back for the next tick
}
void PcSceneRenderer::camera_reset_4857c0(){
    Bytes vehicle(nullptr,0),camera(nullptr,0);
    if(!vehicle_camera||!vehicle_camera(vehicle,camera)){++camera_unanswered;last_error="4857C0 without the shared car/camera";return;}
    if(camera.i8(0x34a)>=2)return;
    driving::PcRaceCameraInputs race{};
    race.car=vehicle;
    race.live_818fe8=Bytes(camera_live_fe8_.data(),camera_live_fe8_.size());
    race.live_818bb0=Bytes(camera_live_bb0_.data(),camera_live_bb0_.size());
    race.live_813750=Bytes(camera_live_750_.data(),camera_live_750_.size());
    if(!race_camera_inputs||!race_camera_inputs(race)){++camera_unanswered;last_error="4857C0 without the race camera inputs";return;}
    driving::PcCameraBlend blend{interpolation_override_82e7d8,std::uint8_t(events_?events_->slots[6].flags:0u),0,1.0f};
    if((blend.owner_flags_79fb4e&3u)==2u){
        if(!autoscene_work_799ca0||!events_||events_->slots[6].work_token==0u){++camera_unanswered;last_error="4857C0 needs the active AUTOSCENE work 799CA0";return;}
        std::memcpy(&blend.owner_mode_799ca0_1c,autoscene_work_799ca0+0x1c,4);}
    try{
        driving::race_camera_save_4833e0(camera,0);
        camera.put8(0x34a,0x1a);   // 49A650(0x16A, 1) / 49A650(0x16B, 1) are RET entries
        driving::race_camera_eye_look_485590(vehicle,camera,race.live_818fe8.sub(0x1a*0x34,0x34),30,matrices_,race);
        driving::race_camera_angles_484df0(camera);
        // 484BD0's device stores are not applied here: the next 485FE0 control
        // (the goal view of the requested mode) sets them before any draw.
        driving::PcCameraDevice d;
        driving::camera_project_484bd0(camera,matrices_,d,screen_,blend);
    }catch(const std::exception& e){++camera_unanswered;last_error=std::string("4857C0 failed: ")+e.what();}
}
bool PcSceneRenderer::invoke(std::uint32_t callback,std::uint32_t,std::uint32_t,std::uint32_t mode_78026c,
                             std::uint32_t race_mode_780258){
    if(callback==0x49a650u||callback==0x41bd40u)return true;          // RET entries
    if(callback==0x484ee0u||callback==0x485fe0u){
        Bytes vehicle(nullptr,0),camera(nullptr,0);
        if(!vehicle_camera||!vehicle_camera(vehicle,camera)){++camera_unanswered;last_error="CAMERA event without the shared car/camera";return true;}
        driving::PcCameraTables tables{Bytes(const_cast<std::uint8_t*>(EmbeddedCameraTable5b4a30),0x64c),
            Bytes(const_cast<std::uint8_t*>(EmbeddedCameraTable5ba4e0),0x438),Bytes(const_cast<std::uint8_t*>(EmbeddedCameraTable5b5080),0x5460),
            Bytes(camera_live_fe8_.data(),camera_live_fe8_.size()),Bytes(camera_live_bb0_.data(),camera_live_bb0_.size()),
            Bytes(camera_live_750_.data(),camera_live_750_.size())};
        // 4493E0 inputs: 82E7D8 (owner: CAMERA override event 7), 79FB4E =
        // AUTOSCENE (event 6) flags, 634B34 = .data constant 1.0f.
        driving::PcCameraBlend blend{interpolation_override_82e7d8,std::uint8_t(events_?events_->slots[6].flags:0u),0,1.0f};
        if((blend.owner_flags_79fb4e&3u)==2u){   // 4B5FD0: [[799CA0]+1C]
            if(!autoscene_work_799ca0||!events_||events_->slots[6].work_token==0u){++camera_unanswered;last_error="4B5FD0 needs the active AUTOSCENE work 799CA0";return true;}
            std::memcpy(&blend.owner_mode_799ca0_1c,autoscene_work_799ca0+0x1c,4);}
        driving::PcCameraDevice d;
        if(callback==0x484ee0u){
            driving::camera_init_484ee0(camera,matrices_,d,screen_,blend,0,tables);++camera_inits;
        }else{
            const std::uint8_t display=events_?events_->slots[385].flags:0u;
            driving::PcCameraControl control{pause_780248,std::int32_t(mode_78026c),camera_preset_819634,display,vehicle,
                Bytes(const_cast<std::uint8_t*>(EmbeddedCameraTable5ba4e0),0x438)};
            driving::PcRaceCameraInputs race{};
            const bool goal_view=mode_78026c==0x13u||mode_78026c==0x1bu||mode_78026c==0x22u||mode_78026c==0x23u||   // 4853D0
                mode_78026c==20u||mode_78026c==21u||mode_78026c==22u;                                   // 485830 (Time Over)
            if((driving::race_camera_mode_48618d(std::int32_t(mode_78026c))||goal_view||mode_78026c==3u)&&race_camera_inputs){
                race.car=vehicle;
                race.live_818fe8=Bytes(camera_live_fe8_.data(),camera_live_fe8_.size());
                race.live_818bb0=Bytes(camera_live_bb0_.data(),camera_live_bb0_.size());
                race.live_813750=Bytes(camera_live_750_.data(),camera_live_750_.size());
                race.game_variant_780258=std::int32_t(race_mode_780258);
                if(race_camera_inputs(race))control.race=&race;
            }
            bool answered=false;
            std::vector<std::uint8_t> before;
            if(enhancements::display_frames()&&camera.size()>=0x3b4u)before.assign(camera.data(),camera.data()+0x3b4);
            try{answered=driving::camera_control_485fe0(camera,matrices_,d,screen_,blend,control);}
            catch(const std::exception& e){
                ++camera_unanswered;last_error=std::string("485FE0 failed (view ")+std::to_string(camera.u8(0x34a))+"): "+e.what();return true;}
            if(!answered){
                ++camera_unanswered;last_error="485FE0 debug camera or race mode without inputs: mode "+std::to_string(mode_78026c)+" (view "+std::to_string(camera.u8(0x34a))+", +94/+98/+9C "+std::to_string(camera.u32(0x94))+"/"+std::to_string(camera.u32(0x98))+"/"+std::to_string(camera.u32(0x9c))+")";return true;}
            ++camera_controls;
            apply_camera(d,camera);
            if(!before.empty())note_camera_replay(camera,before);
            return true;
        }
        apply_camera(d,camera);
        return true;
    }
    if(callback==0x449fc0u){pull_lists();scene_environment_init_449fc0(environment_,device_,matrices_);push_lists();++environment_inits;return true;}
    if(callback==0x44a890u){
        PcSceneEnvironmentControl c{};c.mode_78026c=mode_78026c;c.race_mode_780258=race_mode_780258;
        // No course environment is bound to the native runtime yet: the lists
        // stay absent (as after 449F80). The 44A8DF update still reads the
        // shared car 799D18 and the camera 79F574; without them (or for the
        // gates needing the protected 49EED0/49B2D0 queries) the control is
        // reported as unanswered instead of guessed.
        c.update_44a8df=[this]{
            driving::Bytes vehicle(nullptr,0),camera(nullptr,0);
            if(!vehicle_camera||!vehicle_camera(vehicle,camera))throw std::runtime_error("44A8DF needs the shared car and camera");
            update_environment(vehicle,camera);};
        c.bridge_49eed0=[]()->std::uint32_t{throw std::runtime_error("44A890 table-0 gate needs the 49EED0 bridge");};
        c.timer_49b2d0=[this]()->std::uint16_t{
            if(!timer_49b2d0)throw std::runtime_error("44A890 table-2 gate needs the 49B2D0 timer");
            return timer_49b2d0();};
        try{pull_lists();scene_environment_control_44a890(environment_,c);push_lists();++environment_controls;}
        catch(const std::runtime_error& e){++environment_unanswered;last_error=e.what();}
        return true;
    }
    return false;
}
Bytes PcSceneRenderer::environment_list(std::uint32_t token){
    if(token==0u)return Bytes(nullptr,0);
    if(token==0x844a08u&&select_table_&&!select_table_->empty())
        return Bytes(const_cast<std::uint8_t*>(select_table_->data()),select_table_->size());
    char text[80];std::snprintf(text,sizeof text,"environment list token %08x not resolved natively",token);
    throw std::runtime_error(text);
}
void PcSceneRenderer::pull_lists(){
    if(!sun_lists_)return;
    for(unsigned k=0;k<3;++k)environment_.sun_lists_7d26a8[k]=Bytes(sun_lists_->data(),12).u32(k*4);
}
void PcSceneRenderer::push_lists(){
    if(!sun_lists_)return;
    for(unsigned k=0;k<3;++k)Bytes(sun_lists_->data(),12).put32(k*4,environment_.sun_lists_7d26a8[k]);
}
void PcSceneRenderer::with_environment(Bytes vehicle,Bytes camera,
    const std::function<void(driving::PcEnvironmentFrame&,const driving::CourseCollisionTables&,driving::PcEnvironmentBlendContext&)>& fn){
    pull_lists();
    driving::Bytes flags(environment_.flags_7d28b0.data(),24);
    std::array<std::uint8_t,64> identity{};
    for(unsigned k=0;k<16;++k)driving::Bytes(identity.data(),64).putf(k*4,(k%5u)==0u?1.f:0.f);
    driving::PcEnvironmentBlendContext ec{environment_.phase_7d28c8,time_7d2934_,duration_7d28d8_,
        driving::Bytes(saved_sun_7d26d0_.data(),saved_sun_7d26d0_.size()),driving::Bytes(saved_fog_7d28e0_.data(),saved_fog_7d28e0_.size()),
        driving::Bytes(environment_.lights_899b98.data(),3*0xa0),driving::Bytes(environment_.fog_7d3a10.data(),environment_.fog_7d3a10.size()),
        driving::Bytes(identity.data(),64),matrices_};
    driving::PcEnvironmentFrame f{};
    for(unsigned k=0;k<3;++k){f.fog_lists[k]=environment_list(environment_.fog_lists_7d3a00[k]);f.sun_lists[k]=environment_list(environment_.sun_lists_7d26a8[k]);}
    f.vehicle=vehicle;f.camera=camera;f.flags_7d28b0=flags;f.nearest_7d2d58=driving::Bytes(nearest_7d2d58_.data(),nearest_7d2d58_.size());
    f.lights_899d78=driving::Bytes(environment_.lights_899b98.data()+0x1e0,6*0xa0);
    const driving::Bytes none(nullptr,0);
    const driving::CourseCollisionTables no_course{driving::CourseRunTables{none,none,none,0,false,false},none,none,none,none,0,false};
    fn(f,no_course,ec);
    time_7d2934_=ec.time_7d2934;duration_7d28d8_=ec.duration_7d28d8;environment_.phase_7d28c8=ec.phase_7d28c8;
    push_lists();
}
void PcSceneRenderer::update_environment(driving::Bytes vehicle,driving::Bytes camera){
    with_environment(vehicle,camera,[](driving::PcEnvironmentFrame& f,const driving::CourseCollisionTables& primary,driving::PcEnvironmentBlendContext& ec){
        driving::course_environment_update_44a8df(f,primary,driving::PcEnvironmentTransition{},ec);});
}
bool PcSceneRenderer::display_car(){
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
    vehicle_display_46c140(c,vehicle);++car_displays;return true;
}
// 49F3E0 (GamePlCar display): 46BD30 then 49A650 (RET).
bool PcSceneRenderer::display_race_car(){
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
    // Offline race: the network session word 7F9460+60 is 0 (no LAN session is ported).
    c.shadow=[this,vehicle](const PcVehicleDrawCall& call){return shadow_leaf(call,vehicle);};
    vehicle_race_display_46bd30(c,race_display_,PcRaceCarDisplayInputs{0u,0u,pause_780248},vehicle,
        [this](std::uint32_t pc){++unported_leaves[pc];});
    ++race_car_displays;return true;
}
bool PcSceneRenderer::display_headlight_49f400(){
    // 49F400: [79FB50] & 3 (event 8 flags: 79FB4E + id) gate, then 4695C0(car).
    if(!events_||!(events_->slots[8].flags&3u))return true;
    Bytes vehicle(nullptr,0),camera(nullptr,0);
    if(!vehicle_camera||!vehicle_camera(vehicle,camera))return false;
    if(headlight_vb_.empty())headlight_vb_=pc_headlight_vertices_4171f0();
    const std::uint32_t texture=headlight_texture?headlight_texture():0u;
    if(pc_headlight_display_4695c0(vehicle,*flush_,device_,matrices_,headlight_vb_,texture))++headlight_draws_;
    return true;
}
bool PcSceneRenderer::display_ending_car(){
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
    std::vector<PcVehicleDrawCall> calls;
    driving::pc_matrix_push_load(matrices_,vehicle.sub(0xb0,64));                 // 409F90
    try{vehicle_ending_model_draw_469ff0(vehicle,std::int32_t(std::int8_t(model)),matrices_,calls);}
    catch(...){driving::pc_matrix_pop(matrices_);throw;}
    driving::pc_matrix_pop(matrices_);                                            // 40A010
    vehicle_draw_calls_execute(c,calls);++ending_car_displays;return true;
}
// ---- car shadow volumes -----------------------------------------------------
namespace {
constexpr std::uint32_t ShadowCar=0x7f000000u;   // the shared car work inside the shadow memory
}
template<class F> bool PcSceneRenderer::with_shadow(Bytes car,F&& body){
    PcRaceMemory m;
    m.map(ShadowCar,car.data(),car.size());
    m.map(0x899b98u,environment_.lights_899b98.data(),environment_.lights_899b98.size());
    for(auto& [at,bytes]:shadow_.blocks)m.map(at,bytes.data(),bytes.size());
    for(const auto& [resource,base]:shadow_.bank_base)if(auto* b=bank_resources(resource))m.map(base,b->system.data(),b->system.size());
    PcShadowServices sv{m,device_,
        [this,&m](std::uint32_t n){
            const std::uint32_t at=shadow_.next;shadow_.next+=(n+15u)&~15u;
            auto& bytes=shadow_.blocks[at];bytes.assign(n,0);m.map(at,bytes.data(),bytes.size());return at;},
        [this](std::uint32_t){},
        [this,&m](std::uint32_t resource,std::uint32_t& count,std::uint32_t& table){
            if(!bank_loaded(resource)){
                std::vector<std::uint8_t> pmt;std::string error;
                if(!bank_source||!bank_source(resource,pmt)||!load_bank(resource,std::move(pmt),error)){shadow_error="bank "+std::to_string(resource)+": "+error;return false;}
            }
            auto* b=bank_resources(resource);if(!b)return false;
            auto it=shadow_.bank_base.find(resource);
            if(it==shadow_.bank_base.end()){
                it=shadow_.bank_base.emplace(resource,shadow_.next_bank).first;
                shadow_.next_bank+=(std::uint32_t(b->system.size())+0xfffffu)&~0xfffffu;
                m.map(it->second,b->system.data(),b->system.size());
            }
            count=b->object_count_0c;table=it->second+b->view().u32(b->object_table_20);return true;},
        [this](std::uint32_t field)->std::uint32_t{
            for(const auto& [resource,base]:shadow_.bank_base)if(auto* b=bank_resources(resource))
                if(field>=base&&field-base<b->system.size())return base;
            return 0u;},
        [this](std::uint32_t buffer,std::uint32_t flags,std::uint32_t& size)->const std::uint8_t*{
            size=device_.buffer_size(buffer);return size?device_.lock(buffer,0,0,flags):nullptr;},
        1.f,1.f};
    try{body(sv,m);return true;}
    catch(const std::exception& e){++shadow_failures;shadow_error=e.what();return false;}
}
std::uint32_t PcSceneRenderer::pixel_shader_95af9c(){
    if(!ps_95af9c_created_){ps_95af9c_created_=true;const auto tokens=pc_shader_tokens(0x623830u);ps_95af9c_=device_.create_pixel_shader(tokens.data());}
    return ps_95af9c_;
}
bool PcSceneRenderer::shadow_alloc(Bytes car){
    const bool ok=with_shadow(car,[&](PcShadowServices& sv,PcRaceMemory&){shadow_alloc_46ba20(sv,ShadowCar);});
    if(ok)++shadow_allocs;
    return ok;
}
bool PcSceneRenderer::shadow_free(Bytes car){
    return with_shadow(car,[&](PcShadowServices& sv,PcRaceMemory& m){
        // 440CD0 on the tails: the blocks are released after 46BB20 cleared the words.
        std::vector<std::uint32_t> tails;
        for(const std::uint32_t slot:{0u,8u})if(m.u32(ShadowCar+0x2bc+slot)){tails.push_back(m.u32(m.u32(ShadowCar+0x2bc+slot)+0x98));tails.push_back(m.u32(ShadowCar+0x2b8+slot));}
        shadow_free_46bb20(sv,ShadowCar);
        for(const std::uint32_t t:tails){const std::uint32_t block=m.u32(t);shadow_.blocks.erase(block);}
    });
}
bool PcSceneRenderer::shadow_leaf(const PcVehicleDrawCall& call,Bytes car){
    if(!shadow_draw_enabled)return false;                               // leaves reported as unported
    if(!shadow_.vs_created){
        shadow_.vs_created=true;
        const auto tokens=pc_shader_tokens(0x623e38u);                  // 41786C: CreateVertexShader(623E38, 955A40)
        shadow_.vs_955a40=device_.create_vertex_shader(tokens.data());
    }
    return with_shadow(car,[&](PcShadowServices& sv,PcRaceMemory&){
        PcShadowFrame f{*flush_,queue_->matrices,frame_.layer_7d25f0,shadow_.vs_955a40,{}};
        if(call.pc==0x422550u){shadow_draw_422550(sv,f,call.args[0],call.args[1]);++shadow_draws;
            if(std::getenv("OR2_SHADOW_DEBUG"))std::fprintf(stderr,"[shadow] obj=%08x verts=%u light=(%f,%f,%f) vs=%u\n",call.args[0],sv.m.u32(call.args[0]+0x18),
                sv.m.f32(call.args[1]),sv.m.f32(call.args[1]+4),sv.m.f32(call.args[1]+8),shadow_.vs_955a40);
            if(std::getenv("OR2_SHADOW_DEBUG")){const auto b=device_.debug_stencil_box();std::fprintf(stderr,"[shadow] stencil nonzero=%zu box=%d,%d..%d,%d\n",device_.debug_stencil_nonzero(),b[0],b[1],b[2],b[3]);}
            }
        else{
            if(std::getenv("OR2_SHADOW_DEBUG")){const auto t=device_.get_texture(0);std::fprintf(stderr,"[shadow] darken tex0=%u stencil=%zu shaded=%zu quad:",t,device_.debug_stencil_nonzero(),device_.debug_shaded());if(t)device_.release(t);
                for(std::uint32_t v=0;v<6u;++v){const auto p=call.args[0]+0x20u+v*0x14u;std::fprintf(stderr," (%.0f,%.0f,%.3f,%.3f %08x)",sv.m.f32(p),sv.m.f32(p+4),sv.m.f32(p+8),sv.m.f32(p+12),sv.m.u32(p+16));}
                std::fprintf(stderr,"\n");}
            const bool dbg=std::getenv("OR2_SHADOW_DEBUG")!=nullptr;const auto sum0=dbg?device_.debug_colour_sum():0u;
            shadow_darken_422740(sv,f,call.args[0]);
            if(dbg)std::fprintf(stderr,"[shadow] colour sum %llu -> %llu\n",(unsigned long long)sum0,(unsigned long long)device_.debug_colour_sum());
            if(std::getenv("OR2_SHADOW_DEBUG")){std::fprintf(stderr,"[shadow] after darken shaded=%zu stencil=%zu rs:",device_.debug_shaded(),device_.debug_stencil_nonzero());
                for(std::uint32_t r:{7u,14u,15u,19u,20u,22u,24u,25u,27u,52u,53u,54u,55u,56u,57u,58u,59u,168u})std::fprintf(stderr," %u=%x",r,device_.get_render_state(r));
                std::fprintf(stderr,"\n");}
        }
    });
}
bool PcSceneRenderer::texture_swap_406630(std::uint32_t token,std::uint32_t texture){
    auto* r=bank_resources(token>>16);
    if(!r){   // not loaded yet here (the PC has it resident): applied when load_bank brings it
        for(auto& p:pending_swaps_406630)if(p.first==token){device_.add_ref(texture);device_.release(p.second);p.second=texture;return true;}
        device_.add_ref(texture);pending_swaps_406630.push_back({token,texture});++texture_swaps_deferred;return true;}
    if(!r->texture_table_24){++texture_swap_failures;return false;}
    auto v=r->view();const std::uint32_t index=token&0xffffu;
    if(index>=v.u32(8)){++texture_swaps;return true;}
    const std::uint32_t slot=v.u32(r->texture_table_24)+index*4u;
    // The slot owns a separate reference from the reflection producer. Acquire
    // it before releasing the old slot, including when both handles are equal.
    device_.add_ref(texture);
    if(const auto old=v.u32(slot))device_.release(old);
    v.put32(slot,texture);++texture_swaps;return true;
}
bool PcSceneRenderer::environment_map_46bbc0(driving::Bytes car){
    const std::int32_t model=std::int8_t(car.u8(0x11));
    std::uint32_t token=0;bool found=false;
    for(std::size_t i=0;i<EmbeddedExeRangeCount&&!found;++i){const auto& e=EmbeddedExeRanges[i];
        const std::uint32_t a=0x5b2fe4u+std::uint32_t(model)*8u;
        if(a>=e.base&&a+4u<=e.base+e.size){std::memcpy(&token,e.data+(a-e.base),4);found=true;}}
    if(!found){++texture_swap_failures;return false;}
    return texture_swap_406630(token,globals_.w(0x8a89f4));
}
void PcSceneRenderer::render_frame(driving::PcEventControlState& events,std::uint32_t mode_78026c){
    events_=&events;current_mode_=mode_78026c;
    if(clear_colour_89bd5c)frame_.clear_colour_89bd5c=*clear_colour_89bd5c;
    PcEnvironmentRenderTables tables{Bytes(environment_.lights_899b98.data(),environment_.lights_899b98.size()),
        Bytes(environment_.fog_7d3a10.data(),environment_.fog_7d3a10.size()),0};
    struct Ctx{PcSceneRenderer* self;};
    Ctx ctx{this};
    PcSceneDisplayServices display{&ctx,
        [](void* u,std::uint32_t callback,std::uint32_t work,std::uint32_t event){
            auto& r=*static_cast<Ctx*>(u)->self;
            if(r.trace)r.trace("display",callback);
            struct Timer{PcSceneRenderer& r;std::uint32_t pc;std::chrono::steady_clock::time_point t0;
                ~Timer(){if(r.profile)r.profile(0u,pc,std::uint64_t(std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now()-t0).count()));}};
            const Timer timer{r,callback,r.profile?std::chrono::steady_clock::now():std::chrono::steady_clock::time_point{}};
            try{
            if(r.race_area_display&&r.race_area_display(callback,work,event)){}   // race AREA/SKY hook (events 390/391)
            else if(callback==0x405830u)r.flush_alpha();
            else if(callback==0x49a650u){}
            else if(callback==0x414a40u){}   // event 0x196 (function 0x51) Bink quad: the platform movie layer draws the open movie; with none ([95AF18] = 0) 414A40 returns
            else if(callback==0x49f500u){if(!r.display_car())++r.unported_displays[callback];}
            else if(callback==0x49f3e0u){if(!r.display_race_car())++r.unported_displays[callback];}
            else if(callback==0x49f400u){(void)r.display_headlight_49f400();}   // drawn only with the lights on
            else if(callback==0x49f4c0u){if(!r.display_ending_car())++r.unported_displays[callback];}
            else if(callback==0x49f4d0u){if(!r.display_select_car())++r.unported_displays[callback];}
            else if(r.ghost_display&&r.ghost_display(callback,work)){}          // TA ghost cars 4AE5F0/4ADAC0
            else if(r.robot_display&&r.robot_display(callback,work)){}          // ROB01/ROB03 displays
            else if(r.external_display&&r.external_display(callback)){}                 // scn-efc: 41BD10
            else if(r.race_manager_display&&r.race_manager_display(callback)){} // race manager 44FE00 (event 359)
            else if(r.sprite2d_display&&r.sprite2d_display(callback)){}         // 2D sprite pool 428170
            else ++r.unported_displays[callback];
            }catch(const std::exception& e){char t[48];std::snprintf(t,sizeof t,"display %08x: ",callback);throw std::runtime_error(t+std::string(e.what()));}},
        [](void*,std::uint32_t)->std::uint32_t{throw std::runtime_error("43FB40 kind query needs the event work");},
        [](void* u){static_cast<Ctx*>(u)->self->flush_alpha();},
        [](void* u){render_lights_reset_410740(*static_cast<Ctx*>(u)->self->flush_);},
        [](void* u,std::uint8_t* l){render_light_add_4107a0(*static_cast<Ctx*>(u)->self->flush_,Bytes(l,0x94));}};
    PcFrameInputs in{};in.game_mode_78026c=mode_78026c;in.screen_740c94=1.f;in.screen_740c98=1.f;
    for(unsigned k=0;k<16;++k)in.matrix_7d2da0[k]=(k%5u)==0u?1.f:0.f;
    if(course_effects)course_effects(in.course_work_79f5ec,in.course_flags);
    PcFrameServices sv{[this](std::uint32_t pc,std::uint32_t arg){if(trace)trace("leaf",pc);
        static const bool cube_debug=std::getenv("OR2_CUBE_DEBUG")!=nullptr;
        if(pc==0x49f4d0u){if(!display_select_car())++unported_displays[pc];return;}   // 449050 end: the car-select car over the 2D layer
        if(pc==0x414340u&&cube_debug)std::fprintf(stderr,"[cube] frame %u depth %d offset %td\n",frames,matrices_.depth,matrices_.current_offset);
        const auto t0=profile?std::chrono::steady_clock::now():std::chrono::steady_clock::time_point{};
        if(!(leaf_hook&&leaf_hook(pc,arg)))++unported_leaves[pc];
        if(profile)profile(1u,pc,std::uint64_t(std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now()-t0).count()));},events,scene_,
        environment_.lights_899b98.data()+0x140,display,tables};
    sv.mode_84a318=mode_84a318;
    if(layer_callback)for(std::uint32_t k=0;k<frame_.layers.size();++k)frame_.layers[k].callback=layer_callback(k);
    frame_render_449050(*flush_,frame_,in,sv);
    ++frames;
}
}
