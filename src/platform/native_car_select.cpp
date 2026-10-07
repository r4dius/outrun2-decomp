// Car select (parent key 10) and its preview events on the native runtime.
// See frontend_car_select.hpp for the shared objects; the leaves below are the
// PC functions the owner/preview/loader call, executed on the runtime state.
#include "platform/race_variant_owners.hpp"
#include "platform/race_ghosts_runtime.hpp"
#include "system/dev_hooks.hpp"
#include "platform/race_ghosts.hpp"
#include "platform/pc_scene_renderer.hpp"
#include "platform/race_area_runtime.hpp"
#include "platform/race_hud_runtime.hpp"
#include "platform/race_end_runtime.hpp"
#include "platform/race_player_car.hpp"
#include "platform/frontend_text.hpp"
#include "platform/native_runtime.hpp"
#include "platform/vehicle_preview_event.hpp"
#include "platform/vehicle_preview_init.hpp"
#include "platform/frontend_window.hpp"
#include "platform/frontend_license_owners.hpp"
#include <cstring>
#include <cstdio>
#include <cstdlib>
#include "driving/service_hole.hpp"
namespace outrun::platform {
namespace {
using driving::Bytes;
FrontendCarSelect& car(NativeRuntimeContext& c){return c.event_function36.car_select;}
bool latch(NativeRuntimeContext& c,unsigned pc){auto& s=car(c);if(!s.fault)s.fault=pc;return false;}
// 448AD0 / 448960 / 448990 over the 7C2800 table. BA/BB belong to the
// bootstrap loader entries; model archives are read from the retail store
// synchronously, so a successful request is ready at once (status 7).
bool resource_request(void* user,unsigned id,unsigned mode){
    auto& c=*static_cast<NativeRuntimeContext*>(user);auto& s=car(c);
    if(id>=s.resources_7c2800.size())return latch(c,0x448ad0);
    auto& e=s.resources_7c2800[id];
    if(!driving::runtime_resource_request_448ad0(e,id,mode,s.pending_7cc1d8))return true;
    auto* store=c.event_function36.retail_assets;
    if(!store||!retail_asset_lookup(*store,id,mode))return latch(c,0x448ad0);
    e.status_14=7u;s.pending_7cc1d8=0u;return true;
}
bool resource_ready(void* user,unsigned id,bool& ready){
    auto& c=*static_cast<NativeRuntimeContext*>(user);auto& s=car(c);
    if(id==0xbau||id==0xbbu){ready=c.event_function36.loader_resource_entries[id==0xbau?0:1].status_14==7u;return true;}
    if(id>=s.resources_7c2800.size())return latch(c,0x448960);
    ready=s.resources_7c2800[id].status_14==7u;return true;
}
bool resource_release(void* user,unsigned id){
    auto& c=*static_cast<NativeRuntimeContext*>(user);auto& s=car(c);
    if(id>=s.resources_7c2800.size())return true; // 448990: ids >= 0x223 return at once
    s.resources_7c2800[id].ownership_10=0u;return true;
}
FrontendVehicleLoaderServices loader_io(NativeRuntimeContext& c){return {&c,resource_request,resource_ready,resource_release};}
// Leaves of the preview 48C170..48C450 other than 49FA60/4406F0 (queue).
bool preview_call(void* user,unsigned pc,const unsigned* a,std::size_t n){
    auto& c=*static_cast<NativeRuntimeContext*>(user);auto& s=car(c);
    switch(pc){
    case 0x440110:if(n!=2)return false;driving::event_setup_440110(c.event_state,a[0],a[1],c.event_descriptors,c.event_functions);return true;
    case 0x4401d0:if(n!=1)return false;driving::event_close_4401d0(c.event_state,a[0]);return true;
    case 0x440330:if(n!=2)return false;driving::event_close_serial_440330(c.event_state,a[0],a[1]);return true;
    case 0x40ec60:if(n!=1)return false;s.clear_colour_89bd5c=a[0];return true;
    case 0x44c0a0:if(n!=1)return false;s.course_mode_7d31d0=std::uint8_t(a[0]);return true;
    case 0x49a650:return n==0;
    case 0x44c0d0:return n==0; // no course descriptor 7D30BC in the frontend: the original null path
    case 0x44c3d0:case 0x44a1a0:case 0x4f2210:
        // Scene handle releases: nothing of theirs is allocated natively in the
        // frontend (all handles zero), so only their resets would run.
        ++s.scene_releases;return n==0;
    default:return false;
    }
}
FrontendVehiclePreviewServices preview_services(NativeRuntimeContext& c){
    auto& s=car(c);
    FrontendVehiclePreviewServices p{Bytes(c.event_function36.loader_83039c.data(),c.event_function36.loader_83039c.size()),
        Bytes(s.car_799d18.data(),s.car_799d18.size()),Bytes(s.sun_lists_7d26a8.data(),s.sun_lists_7d26a8.size())};
    p.user=&c;p.call=preview_call;p.ready_448960=resource_ready;p.creation=&c.vehicle_creation;
    return p;
}
bool menu_external(void* user,unsigned pc,Bytes window,const unsigned* a,std::size_t n){
    auto& c=*static_cast<NativeRuntimeContext*>(user);
    auto& t=c.event_function36.loader_stage12_state.transition_globals;
    switch(pc){
    case 0x4c50d0:driving::transition_global_set_active_4c50d0(t);return n==0;
    case 0x4c50f0:if(n!=1)return false;{float f;std::memcpy(&f,&a[0],4);driving::transition_global_set_primary_4c50f0(t,f);}return true;
    case 0x4c5100:if(n!=1)return false;{float f;std::memcpy(&f,&a[0],4);driving::transition_global_set_secondary_4c5100(t,f);}return true;
    case 0x4c5110:driving::transition_global_clear_active_4c5110(t);return n==0;
    case 0x48b130:case 0x48b150:case 0x48b190:{unsigned r{};return native_start_owned_call(c,pc,a,n,r);}
    case 0x48ca30:frontend_window_suspend_48ca30(window.sub(0,PcFrontendWindowBytes));return n==0;
    default:return false; // 48D420/48C5F0 transmission dialog: not bound yet
    }
}
}
bool native_frontend_resource_request(NativeRuntimeContext& c,unsigned id,unsigned mode){return resource_request(&c,id,mode);}
bool native_frontend_resource_ready(NativeRuntimeContext& c,unsigned id,bool& ready){return resource_ready(&c,id,ready);}
std::uint32_t native_car_select_virtual(NativeRuntimeContext& c,std::uint32_t slot){
    auto& st=c.event_function36;auto& s=st.car_select;
    if(slot==0x4c8de0u){s.fault=0;return frontend_vehicle_menu_construct_4c8de0(s.menu,s.repeat)?1u:0u;}
    if(slot<=16u&&(slot%4u)==0u)++s.slot_calls[slot/4u];
    if(s.fault&&slot!=0u&&slot!=16u)return 0u;
    FrontendUiResources ui{st.frontend_sprites};ui.motion_step=st.frontend_ui_motion_step;
    ui.effect_user=st.frontend_effect_user;ui.effect_4249f0=st.frontend_effect;
    auto preview=preview_services(c);
    FrontendVehicleMenuServices api{ui,Bytes(st.object.data(),st.object.size()),st.frontend_ui_globals,
        st.frontend_profiles.active,st.frontend_input,s.repeat};
    api.timer=st.frontend_choice_timer;
    api.colour_held_4536c0=(st.frontend_input.device_held&0x10u)!=0u;   // 4536C0(0x10)
    api.user=&c;api.external=menu_external;api.preview=&preview;
    bool ok=false;unsigned result{};
    if(slot==4u){ok=frontend_vehicle_menu_init_4c9010(s.menu,api);result=ok;}
    else if(slot==8u)ok=frontend_vehicle_menu_control_4c9290(s.menu,api,result);
    else if(slot==12u)ok=frontend_vehicle_menu_display_4c8d70(s.menu,api);
    else if(slot==16u||slot==0u)ok=frontend_vehicle_menu_suspend_4c8d90(s.menu,api);
    if(!ok){
        s.fault=preview.fault?preview.fault:(api.missing?api.missing:(s.menu.fault?s.menu.fault:0x4c9290));
        st.frontend_ui_missing_pc=s.fault;return 0u;
    }
    return result;
}
bool native_car_loader_tick_48bfe0(NativeRuntimeContext& c){
    FrontendVehicleLoader view;auto& bytes=c.event_function36.loader_83039c;
    std::memcpy(view.object.data(),bytes.data(),view.object.size());
    const bool ok=frontend_vehicle_loader_tick_48bfe0(view,loader_io(c));
    std::memcpy(bytes.data(),view.object.data(),view.object.size());
    if(!ok)latch(c,0x48bfe0);
    return ok;
}
bool native_car_event_invoke(NativeRuntimeContext& c,std::uint32_t callback,const NativeEnvironmentHook& environment,
                             driving::PcMatrixStack& matrices){
    auto& s=car(c);
    if(native_race_robots_invoke(c,callback,matrices))return true;          // ROB01/ROB03 (race_robots_runtime.hpp)
    if(native_race_variant_invoke(c,callback,matrices))return true;         // variants 8 / 9 owners (race_variant_owners.hpp)
    if(native_ghost_car_invoke(c,callback,matrices))return true;            // TA ghost cars 9..12 (race_ghosts_runtime.hpp)
    if(native_race_traffic_car_invoke(c,callback,matrices))return true;     // rival / traffic cars (race_traffic_runtime.hpp)
    if(native_race_traffic_object_invoke(c,callback,matrices))return true;  // course objects (OSO, race_traffic_runtime.hpp)
    if(native_race_effects_invoke(c,callback,matrices))return true;          // scn-efc: 4AFBB0/4AFDC0/4AFC70, 41BBF0/41BC60
    if(callback==0x4a7270u){
        if(!c.game_mode.driving_data){latch(c,0x4a7270);return true;}
        driving::PcEnvironmentFrame frame{};driving::PcEnvironmentBlendContext* ctx{};(void)ctx;
        driving::CourseWorldTables world_tables{};driving::EasyLctPredictionState prediction{};
        driving::CourseWorldQuery world{world_tables,matrices,prediction};
        bool ok=false;
        environment(Bytes(s.car_799d18.data(),s.car_799d18.size()),Bytes(s.camera_79fe10.data(),s.camera_79fe10.size()),
            [&](driving::PcEnvironmentFrame& f,const driving::CourseCollisionTables& primary,driving::PcEnvironmentBlendContext& ec){
                VehicleParameterSelection selection{};
                selection.variant_83036d=c.start_mode.vehicle_variant_83036d;
                selection.loading_scene_7de418=c.race.car_world.commrace_7de418[0];
                selection.game_mode_780258=c.game_mode.game_variant;
                selection.flag_65a7ac=0;            // .data byte 65A7AC
                selection.flag_8514a0=s.flag_8514a0;
                VehiclePreviewInit in{c.vehicle_creation,*c.game_mode.driving_data,selection,
                    Bytes(s.shared_83db30.data(),s.shared_83db30.size()),Bytes(s.parameters.data(),s.parameters.size()),
                    f,primary,ec,Bytes(s.body_82e7f0.data(),s.body_82e7f0.size()),world};
                in.transmission_830374=s.transmission_830374;in.yaw_841fa0=s.yaw_841fa0;
                ok=vehicle_preview_init_4a7270(Bytes(s.car_799d18.data(),s.car_799d18.size()),in);});
        if(!ok)latch(c,0x4a7270);else ++s.preview_inits;
        return true;
    }
    if(callback==0x4a7080u){                                                  // event 8 function 0x2B init: the OUTRUN2SP car-select car
        if(!c.game_mode.driving_data){latch(c,0x4a7080);return true;}
        bool ok=false;
        try{
            environment(Bytes(s.car_799d18.data(),s.car_799d18.size()),Bytes(s.camera_79fe10.data(),s.camera_79fe10.size()),
                [&](driving::PcEnvironmentFrame& f,const driving::CourseCollisionTables& primary,driving::PcEnvironmentBlendContext& ec){
                    VehicleParameterSelection selection{};
                    selection.variant_83036d=c.start_mode.vehicle_variant_83036d;
                    selection.loading_scene_7de418=c.race.car_world.commrace_7de418[0];
                    selection.game_mode_780258=c.game_mode.game_variant;
                    selection.flag_65a7ac=0;selection.flag_8514a0=s.flag_8514a0;
                    const auto tables=c.start_mode.scene_owner_course_world.tables_or_empty();
                    driving::CourseWorldQuery world{tables,matrices,s.race_prediction};
                    VehiclePreviewInit in{c.vehicle_creation,*c.game_mode.driving_data,selection,
                        Bytes(s.shared_83db30.data(),s.shared_83db30.size()),Bytes(s.parameters.data(),s.parameters.size()),
                        f,primary,ec,Bytes(s.body_82e7f0.data(),s.body_82e7f0.size()),world};
                    in.transmission_830374=s.transmission_830374;
                    VehicleArcadeInit a{};
                    a.mode_78026c=c.mode_state.current;
                    if(c.race_end){const auto& b=c.race_end->state.boot_83dae0;   // 49EED0 = [83DAF4], 49EEE0 = [83DAE9]
                        std::memcpy(&a.query_49eed0,b.data()+0x14,4);a.query_49eee0=b[0x09];}
                    a.shadow_46ba20=[&s](Bytes car){if(!(s.shadow_service&&s.shadow_service(0x46ba20u,car)))++s.shadow_unported;};
                    a.reset_46f350=[](Bytes car){                                      // 46F350 over the car at 7804B0
                        PcRaceMemory m;ghost_map_car_tables(m);m.map(0x7804b0u,car.data(),car.size());car_reset_46f350(m,0x7804b0u);};
                    ok=vehicle_arcade_init_4a7080(Bytes(s.car_799d18.data(),s.car_799d18.size()),in,a);});
        }catch(const std::exception& x){ok=false;
            dev_log("4a7080: %s",x.what());
        }
        if(!ok)latch(c,0x4a7080);else ++s.preview_inits;
        return true;
    }
    if(callback==0x4a6ed0u){
        if(!c.game_mode.driving_data){latch(c,0x4a6ed0);return true;}
        const auto tables=c.start_mode.scene_owner_course_world.tables();
        driving::CourseWorldQuery world{tables,matrices,s.race_prediction};
        VehicleParameterSelection selection{};
        selection.variant_83036d=c.start_mode.vehicle_variant_83036d;
        selection.loading_scene_7de418=c.race.car_world.commrace_7de418[0];
        selection.game_mode_780258=c.game_mode.game_variant;
        selection.flag_65a7ac=0;
        selection.flag_8514a0=s.flag_8514a0;
        const auto& runtime=c.game_mode.course_runtime;
        GamePlCarInit in{c.vehicle_creation,*c.game_mode.driving_data,selection,
            Bytes(s.shared_83db30.data(),s.shared_83db30.size()),Bytes(s.parameters.data(),s.parameters.size()),
            Bytes(s.body_82e7f0.data(),s.body_82e7f0.size()),world};
        in.area_yaw={runtime.zero_7d3124[1],runtime.zero_7d3178[1]};
        in.transmission_830374=s.transmission_830374;
        in.grid_variant4=[&](Bytes car,driving::CourseProbe& at){   // LAN grid (race_traffic_runtime)
            if(!native_race_network_grid_4a6ed0(c,matrices,c.event_state.slots[8].work_token))return false;
            at={car.f32(0x14),car.f32(0x18),car.f32(0x1c)};return true;};
        in.shadow_46ba20=[&s](Bytes car){if(!(s.shadow_service&&s.shadow_service(0x46ba20u,car)))++s.shadow_unported;};
        in.envmap_46bbc0=[&s,&c](Bytes car){
            auto* r=c.race.robots.renderer;
            if(r){native_car_reflection_init(*r);if(r->environment_map_46bbc0(car))return;}
            ++s.envmap_unported;};
        bool ok=false;
        try{ok=game_pl_car_init_4a6ed0(Bytes(s.car_799d18.data(),s.car_799d18.size()),in);}
        catch(const std::exception& x){ok=false;
            dev_log("4a6ed0: %s",x.what());
        }
        s.global_841b50=in.global_841b50;s.flag_680bd0=in.flag_680bd0;
        // 4A6EDB / 4A6EE9: the stage progress history (841B50 entries, count 680BD0) restarts
        // with one entry, the dword [5C3604] as its position/offset words.
        {auto& h=c.race.car_world.progress_680bd0;h=driving::PcStageProgressHistory{};h.count=in.flag_680bd0;
         h.position[0]=std::uint16_t(in.global_841b50);h.offset[0]=std::uint16_t(in.global_841b50>>16);}
        if(!ok)latch(c,0x4a6ed0);else ++s.race_inits;
        return true;
    }
    if(callback==0x49f4e0u){                                                  // GamePlCar dest: 49A650 (RET), 46BB20
        if(!(s.shadow_service&&s.shadow_service(0x46bb20u,Bytes(s.car_799d18.data(),s.car_799d18.size()))))++s.shadow_unported;
        ++s.race_dests;
        return true;
    }
    if(callback==0x4874a0u){                                                  // event 7 control: camera override 82E7D8
        ++c.race.override_controls;
        if(c.race.camera_override.override_82e7d8!=0)(void)native_race_end_camera_4874a0(c,matrices);   // a fault latches the race end module
        return true;
    }
    if(callback==0x475670u){                                                  // event 8 function 0x2B control
        if(!race_player_car_arcade_475670(c,matrices))latch(c,0x475670);
        return true;
    }
    if(callback==0x475720u){                                                  // PasPlCar_Ctrl (486942)
        if(!race_player_car_pas_475720(c,matrices))latch(c,0x475720);
        return true;
    }
    if(callback==0x4a8330u){                                                  // GamePlCar_Ctrl
        if(!race_player_car_control_4a8330(c,matrices))latch(c,0x4a8330);
        return true;
    }
    if(callback==0x4a5b20u){
        // 680C04 and 634B34 are .data constants; AUTOSCENE (event 6) owns
        // 79FB4E (its flags) and 799CA0 (its work, unused while inactive).
        VehiclePreviewControl ctl{{-1.3f,3.4f,-8.0f},s.yaw_841fa0,c.event_function36.pc_crt_random_state, // one CRT rand stream (580F40)
            
            Bytes(s.shared_83db30.data(),s.shared_83db30.size()),matrices,0,c.event_state.slots[6].flags,
            Bytes(nullptr,0),1.f,0,std::int32_t(c.mode_state.current),0,0};
        vehicle_preview_control_4a5b20(Bytes(s.car_799d18.data(),s.car_799d18.size()),ctl);
        ++s.preview_controls;
        return true;
    }
    return false;
}
std::uint32_t native_transmission_virtual(NativeRuntimeContext& c,std::uint32_t slot){
    auto& st=c.event_function36;auto& t=st.transmission;
    if(slot==0x4dd410u)return frontend_transmission_construct_4dd410(t,st.car_select.repeat)?1u:0u;
    if(t.fault&&slot!=0u&&slot!=16u)return 0u;
    if(slot==12u)return 1u;                                            // 49A650
    FrontendUiResources ui{st.frontend_sprites};ui.motion_step=st.frontend_ui_motion_step;
    ui.effect_user=st.frontend_effect_user;ui.effect_4249f0=st.frontend_effect;
    FrontendTransmissionServices s{ui,Bytes(st.object.data(),st.object.size()),st.loader_stage12_state.transition_globals,
        st.frontend_input,st.car_select.repeat,st.transmission_selection_84b210,st.car_select.transmission_830374,st.frontend_choice_timer};
    bool ok=false;unsigned result{};
    if(slot==4u){ok=frontend_transmission_init_4dd250(t,s);result=ok;}
    else if(slot==8u)ok=frontend_transmission_control_4dd4a0(t,s,result);
    else if(slot==16u||slot==0u)ok=frontend_transmission_suspend_4dd2e0(t,s);
    if(!ok){st.frontend_ui_missing_pc=t.fault?t.fault:0x4dd4a0;return 0u;}
    return result;
}
std::uint32_t native_music_virtual(NativeRuntimeContext& c,std::uint32_t slot){
    auto& st=c.event_function36;auto& m=st.music;
    if(slot==0x4c9a90u)return frontend_music_construct_4c9a90(m,st.car_select.repeat)?1u:0u;
    if(m.fault&&slot!=0u&&slot!=16u)return 0u;
    if(slot==12u)return 1u;                                            // 49A650
    FrontendUiResources ui{st.frontend_sprites};ui.motion_step=st.frontend_ui_motion_step;
    ui.effect_user=st.frontend_effect_user;ui.effect_4249f0=st.frontend_effect;
    FrontendMusicServices s{ui,Bytes(st.object.data(),st.object.size()),st.frontend_ui_globals,st.frontend_profiles.active,
        st.frontend_input,st.car_select.repeat,st.music_globals,st.pc_crt_random_state};
    s.random_held_4536c0=(st.frontend_input.device_held&0x10u)!=0u;   // 4536C0(0x10)
    s.timer=st.frontend_choice_timer;s.user=&st;
    s.play_401000=[](void* p,unsigned channel,unsigned track,unsigned loop){
        auto& x=*static_cast<NativeEventFunction36State*>(p);++x.music_play_requests;x.music_last_track=track;
        return x.music_play?x.music_play(x.music_user,channel,track,loop):true;};
    s.stop_401030=[](void* p,unsigned channel){
        auto& x=*static_cast<NativeEventFunction36State*>(p);++x.music_stop_requests;
        return x.music_stop?x.music_stop(x.music_user,channel):true;};
    bool ok=false;unsigned result{};
    if(slot==4u){ok=frontend_music_init_4c9790(m,s);result=ok;}
    else if(slot==8u)ok=frontend_music_control_4c9c20(m,s,result);
    else if(slot==16u||slot==0u)ok=frontend_music_suspend_4c9910(m,s);
    if(!ok){st.frontend_ui_missing_pc=m.fault?m.fault:0x4c9c20;return 0u;}
    return result;
}
// Parent key 43 (Showroom). Its preview is the car select's (one loader 83039C,
// one car 799D18); prices are the 4EF280 table 84B920 of the race end module.
namespace {
bool showroom_external(void* user,unsigned pc,const unsigned* a,std::size_t n){
    auto& c=*static_cast<NativeRuntimeContext*>(user);auto& st=c.event_function36;
    switch(pc){
    case 0x4c50d0:case 0x4c50f0:case 0x4c5100:case 0x4c5110:return menu_external(user,pc,Bytes(nullptr,0),a,n);
    case 0x401000:if(n!=3)return false;++st.music_play_requests;st.music_last_track=a[1];
        return st.music_play?st.music_play(st.music_user,a[0],a[1],a[2]):true;
    case 0x401030:if(n!=1)return false;++st.music_stop_requests;
        return st.music_stop?st.music_stop(st.music_user,a[0]):true;
    case 0x42e020:return n==3;                       // BGM channel volume: not modelled by the platform player
    case 0x416420:return n==0&&(!st.license_owners||st.license_owners->save_active());
    default:return false;
    }
}
}
std::uint32_t native_showroom_virtual(NativeRuntimeContext& c,std::uint32_t slot){
    auto& st=c.event_function36;auto& sr=st.showroom;
    if(slot==0x4d4230u)return frontend_showroom_construct_4d4230(sr,st.frontend_sprites,st.car_select.repeat)?1u:0u;
    if(!sr.constructed||!st.frontend_fonts||!st.frontend_text)return slot==0u||slot==16u?1u:0u;
    if(sr.fault&&slot!=0u&&slot!=16u)return 0u;
    if(!sr.ui&&sr.pool)sr.ui=std::make_unique<FrontendUiResources>(FrontendUiResources{*sr.pool});
    if(!sr.ui){outrun::driving::service_hole("native_showroom_virtual","sr.ui");return 0u;}
    auto& ui=*sr.ui;ui.motion_step=st.frontend_ui_motion_step;ui.pause_domain=st.title_pause_flag_95b214;
    ui.effect_user=st.frontend_effect_user;ui.effect_4249f0=st.frontend_effect;
    auto preview=preview_services(c);
    Bytes root(st.object.data(),st.object.size());
    FrontendShowroomServices s{root,st.frontend_ui_globals,st.frontend_profiles.active,st.frontend_input,st.car_select.repeat,
        *st.frontend_fonts,*st.frontend_text,native_race_end(c).state.miles.data()+0x20,&preview,st.camera_preset_819634};
    s.timer=st.frontend_choice_timer;s.owner_delta=root.f32(0xda0);s.root_state=root.u32(0x218);
    s.user=&c;s.external=showroom_external;
    bool ok=false;unsigned result{};
    if(slot==4u){ok=frontend_showroom_init_4d4a30(sr,s);result=ok;}
    else if(slot==8u)ok=frontend_showroom_control_4d5880(sr,s,result);
    else if(slot==12u){ok=frontend_showroom_display_4d4350(sr,s);result=ok;}
    else if(slot==16u)ok=frontend_showroom_suspend_4d4b70(sr,s);
    else if(slot==0u)ok=frontend_showroom_destroy_4d5760(sr,s);
    if(!ok){if(!sr.fault)sr.fault=preview.fault?preview.fault:(s.missing?s.missing:0x4d5880);st.frontend_ui_missing_pc=sr.fault;return 0u;}
    return result;
}
namespace {
MissionManagerServices mission_services(NativeRuntimeContext& c){
    auto& start=c.start_mode;auto& m=c.mission;
    MissionManagerServices s;
    s.races=start.scene_owner_race_assets;s.profile=&c.event_function36.frontend_profiles.active;
    s.race_key_67e6a4=start.scene_owner_race_key;s.race_sub_67e6a8=start.scene_owner_race_sub_key;
    s.variant_780258=c.game_mode.game_variant;
    if(c.game_mode.game_variant==4u&&m.manager.network_record_83637c&&c.race_end){
        // LAN race: the session settings record [83637C] = 84A9B0, inside the arcade block 84A208..
        const auto& block=c.race_end->state.arcade_84a208;const std::uint32_t at=m.manager.network_record_83637c-0x84a208u;
        if(at+0x2cu<=block.size()){
            auto rd=[&](std::uint32_t o){std::uint32_t v;std::memcpy(&v,block.data()+at+o,4);return v;};
            s.lan.present=true;s.lan.index=rd(0);s.lan.type=rd(0x14);s.lan.kind=rd(0x20);
            const std::uint32_t t=rd(0x28);std::memcpy(&s.lan.seconds,&t,4);
            s.lan.players=c.race.car_world.commrace_7de418[0];   // 456D60
        }
    }
    s.apply_named_44d720=[&c](const char* data,const char* category){return native_course_load_named(c,data,category,-1,0u);};
    s.preset_43f950=[&c](std::uint32_t v){c.start_mode.course_preset=v;};
    s.apply_44d720=[&c](RaceCourseSelection& sel,bool kind4){
        // 44D720("Race_Courses", "", Races, 0, 0, 1, kind4, list, count, 0) onto the live course state.
        const auto* assets=c.game_mode.course_assets;if(!assets)return false;
        driving::PcMatrixStack matrices{driving::Bytes(c.game_mode.course_matrix_stack.data(),c.game_mode.course_matrix_stack.size()),0,0,2};
        driving::PcCourseApplyRequest44d720 request{};
        request.data_name="Race_Courses";request.category_name="";request.loader={true,3};
        request.shuffle_groups=kind4;
        request.direct_records=sel.course_records.data();request.direct_bytes=sel.course_records.size();
        request.direct_count=static_cast<std::int32_t>(sel.course_count);
        try{return driving::runtime_apply_course_data_44d720(request,driving::course_descriptor_tables_r078(assets->descriptors),
            c.game_mode.course_runtime,matrices,{});}catch(const std::exception&){return false;}
    };
    s.traffic_47cf40=[&c](std::uint32_t race_index){
        auto& m=c.mission;auto& start=c.start_mode;
        if(!m.view_ready){
            if(!start.scene_owner_race_assets||!pc_relocate_blob(start.scene_owner_race_assets->bytes,NativeRacesBlobBase,m.races_relocated))return false;
            m.view=PcAddressView{};m.view.add(NativeRacesBlobBase,m.races_relocated.data(),m.races_relocated.size());m.view.add_exe();m.view_ready=true;
        }
        const auto record=NativeRacesBlobBase+std::uint32_t(start.scene_owner_race_assets->races_offset)+race_index*0x44u;
        std::uint32_t cfg{};if(!m.view.u32(record+0x1c,cfg))return false;
        RacerSetupServices rs;rs.view=&m.view;
        const auto& car=c.event_function36.car_select.car_799d18;
        rs.car_present=car.size()>0x260;if(rs.car_present){rs.car_byte11=std::int8_t(car[0x11]);std::memcpy(&rs.car_word25e,car.data()+0x25e,2);}
        rs.network_686258=0xffffffffu;
        rs.course_length_44b820=[&c](std::int32_t laps,std::uint32_t& length){
            const auto& sel=c.mission.manager.selection;const auto& rt=c.game_mode.course_runtime;
            if(!c.game_mode.course_assets||sel.course_records.empty())return false;
            const auto offset=std::size_t(rt.selected_index)*0x78u;if(offset>=sel.course_records.size())return false;
            return course_length_44b820(rt.selected_7d30a8.data(),sel.course_records.data()+offset,sel.course_records.size()-offset,
                                        c.game_mode.course_assets->descriptors,laps,length);
        };
        rs.selection_836374=start.selection_active_836374;
        std::uint32_t type{};race_record_type(*start.scene_owner_race_assets,start.scene_owner_race_key,start.scene_owner_race_sub_key,type);
        rs.mission_type_495b20=type;
        MissionManagerServices ms;ms.races=start.scene_owner_race_assets;std::int32_t t{};mission_manager_time_4961f0(m.manager,ms,t);rs.time_4961f0=t;
        const auto* text=c.event_function36.frontend_text;{const auto* t=text?text->get(0x3d7):nullptr;rs.text_3d7=t?*t:std::string();}
        if(!racer_setup_47cf40(m.racers,rs,cfg,false)){m.missing=rs.missing;return false;}
        start.scene_owner_special_driver_80fb28=std::int32_t(m.racers.special_80fb28);
        return true;
    };
    (void)m;return s;
}
}
bool native_mission_event_invoke(NativeRuntimeContext& c,std::uint32_t callback){
    auto& m=c.mission;
    switch(callback){
    case 0x4964c0u:mission_manager_init_495b90(m.manager);++m.init_calls;return true;
    case 0x4964d0u:mission_manager_destroy_4964d0(m.manager);c.start_mode.selection_active_836374=true;++m.destroy_calls;return true;
    case 0x4970e0u:{
        ++m.control_calls;
        // 496A30: 4965A0, the four 4659F0 ticks and the mode-16 race part
        // (race_hud_runtime, which owns the UI resources and voice queue).
        auto s=mission_services(c);
        if(c.mode_state.current==16u)++m.game_part_calls;
        (void)native_race_hud_mission_control(c,s);
        return true;
    }
    // Time Attack manager (event 0x193, function 0x22).
    case 0x48b8f0u:{auto& st=c.start_mode;
        st.flag_830394=1u;st.ta_result_830398=0u;st.ta_phase_83038c=1u;st.ta_loader_830384=0u;st.ta_loader_830388=0u;return true;}
    case 0x48b950u:                                                    // 48B550(&830384, [656234], [830395])
        (void)native_goal_course_48b550(c,c.start_mode.frontend_prepare.output_code_656234,c.start_mode.frontend_prepare.output_flag_830395);
        return true;
    case 0x48b920u:{auto& st=c.start_mode;
        st.flag_830394=0u;
        // 4F1860: the course script lists (84D90C..84D968) of the frontend are released; the
        // next frontend init requests them again (49E490 sets the bulk loader pending).
        c.event_function36.frontend_course_tables={};
        st.ta_phase_83038c=1u;st.ta_loader_830384=0u;st.ta_loader_830388=0u;return true;}
    // Small race events (race_events.hpp).
    case 0x4af420u:race_clock_init_4af420(c.race.clock);return true;
    case 0x4af4a0u:race_clock_control_4af4a0(c.race.clock);++c.race.clock_controls;return true;
    case 0x49ace0u:race_frame_counter_init_49ace0(c.race.frames_8367f4);return true;
    case 0x49acf0u:race_frame_counter_control_49acf0(c.race.frames_8367f4);++c.race.frame_controls;return true;
    case 0x4866d0u:race_camera_override_init_4866d0(c.race.camera_override);return true;
    default:return false;
    }
}
}
