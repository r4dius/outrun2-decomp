#include "system/dev_hooks.hpp"
#include "platform/native_runtime.hpp"
#include "enhancements/frame_rate.hpp"
#include "platform/race_variant_owners.hpp"
#include "platform/pc_input_devices.hpp"
#include "platform/pc_network.hpp"
#include "platform/race_end_runtime.hpp"
#include "platform/race_ghosts_runtime.hpp"
#include "platform/race_area_runtime.hpp"   // race AREA/SKY owner (road tables)
#include "platform/race_hud_runtime.hpp"    // 45C470 quest objects (NAVI memory)
#include "platform/scene_owner_49ba80.hpp"
#include "platform/pc_address_view.hpp"
#include "platform/frontend_welcome.hpp"
#include "platform/frontend_fixed_choice.hpp"
#include "platform/frontend_vehicle_data.hpp"
#include "platform/frontend_stack.hpp"
#include "platform/frontend_ui_resources.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <exception>
#include <initializer_list>
#include <limits>
#include <vector>
#include "driving/service_hole.hpp"
namespace outrun::platform {
// race_traffic_runtime.cpp: an entry of the C2C request manager 7F9460 (variant 5).
bool native_race_requests_call(NativeRuntimeContext&,driving::PcMatrixStack&,std::uint32_t pc,
                               std::initializer_list<std::uint32_t> args,std::uint32_t& eax,std::uint32_t ecx);
namespace {
// The request manager outside a race frame (START): over an identity matrix stack.
bool requests_call(NativeRuntimeContext& c,std::uint32_t pc,std::initializer_list<std::uint32_t> args,std::uint32_t& eax){
    std::array<std::uint8_t,0x400> stack{};
    driving::Bytes bytes(stack.data(),stack.size());
    for(std::size_t i=0;i<16u;++i)bytes.put32(i*4u,(i%5u)==0u?0x3f800000u:0u);
    driving::PcMatrixStack matrices{bytes,0,0,16};
    return native_race_requests_call(c,matrices,pc,args,eax,0u);
}
}
}

namespace outrun::platform {
// 448B90 (object name database, object_db.hpp). State 0: the async request of
// \Common\object_db_bin.sz (read synchronously), 1: the build, then 0. A missing file is
// reported in object_db.error and every later lookup throws (the loader continues: the PC
// would wait).
std::uint32_t native_object_db_step_448b90(NativeRuntimeContext& c){
    auto& d=c.object_db;
    if(d.state_7c27f4==0u){d.state_7c27f4=1u;d.slot_7c27f0=1u;return 1u;}
    if(d.state_7c27f4!=1u)return 0u;
    std::string error;const std::vector<std::uint8_t>* bytes=nullptr;
    if(auto* retail=c.event_function36.retail_assets)bytes=retail_asset_guest_path(*retail,"\\Common\\object_db_bin.sz",true,&error);
    try{
        if(!bytes)throw std::runtime_error("\\Common\\object_db_bin.sz unreadable: "+error);
        if(bytes->size()<4u||std::uint32_t((*bytes)[0]|((*bytes)[1]<<8)|((*bytes)[2]<<16)|(std::uint32_t((*bytes)[3])<<24))!=bytes->size()-4u)
            throw std::runtime_error("object_db_bin.sz size word");
        object_db_build_448b90(d,driving::Bytes(const_cast<std::uint8_t*>(bytes->data())+4,bytes->size()-4u));
        ++d.builds;
    }catch(const std::exception& e){d.error=e.what();d.slot_7c27f0=0u;d.state_7c27f4=2u;}
    return 0u;
}
namespace {
constexpr std::array<unsigned char,8> LegacyMagic{{'O','R','2','E','V','T','1',0}};
constexpr std::array<unsigned char,8> Magic{{'O','R','2','E','V','T','2',0}};
constexpr float Scale62812c=0x1.1028d2p-6f;
std::uint32_t le32(const unsigned char* p){return std::uint32_t(p[0])|(std::uint32_t(p[1])<<8u)|(std::uint32_t(p[2])<<16u)|(std::uint32_t(p[3])<<24u);}
void fail(std::string* e,const char* s){if(e)*e=s;}

const LoaderAssetRecord* runtime_asset(const NativeEventFunction36State& state,
                                       std::uint32_t resource_id,
                                       std::uint32_t request_mode){
    if(state.retail_assets){
        std::string ignored;
        if(const auto* retail=retail_asset_lookup(*state.retail_assets,resource_id,request_mode,&ignored))
            return retail;
    }
    return state.loader_assets?find_loader_asset(*state.loader_assets,resource_id,request_mode):nullptr;
}


float native_event_timer(void* user,std::uint32_t pc_entry){
    if(!user||pc_entry!=0x004af500u)return 0.0f;
    const auto& c=*static_cast<const NativeRuntimeContext*>(user);
    return static_cast<float>(c.frame_state.frame_counter_95af0c);
}

bool apply_sumo_fe_reset(NativeRuntimeContext& c){
    auto& event=c.event_function36;
    if(!event.initialized)return false;
    const driving::PcObjectResetServices services{&c,native_event_timer};
    (void)driving::object_state_reset_442c20(
        driving::Bytes(event.object.data(),event.object.size()),
        event.global_mode_6319a1,Scale62812c,services);
    ++c.mode_state.sumo_fe_reset_calls;
    c.mode_state.sumo_fe_reset_pending=false;
    return true;
}

void native_loader_request(void* user,std::uint32_t pc_entry,
                           std::uint32_t resource_id,std::uint32_t request_mode){
    if(!user||pc_entry!=0x00448ad0u)return;
    auto& state=*static_cast<NativeEventFunction36State*>(user);
    ++state.loader_request_calls;
    state.loader_last_request=resource_id;
    state.loader_last_request_mode=request_mode;
    const auto index=resource_id==0xbau?0u:resource_id==0xbbu?1u:2u;
    if(index>=state.loader_resource_entries.size())return;
    auto& entry=state.loader_resource_entries[index];
    if(!driving::runtime_resource_request_448ad0(
           entry,resource_id,request_mode,state.loader_resource_pending))return;
    const auto* asset=runtime_asset(state,resource_id,request_mode);
    if(asset){
        entry.status_14=7u;
        ++state.loader_ready_count;
        state.loader_ready_bytes+=static_cast<std::uint32_t>(asset->bytes.size());
    }
    state.loader_resource_pending=
        state.loader_ready_count==state.loader_request_calls?0u:1u;
}

std::uint32_t native_frontend_bulk_call(void* user,std::uint32_t pc_entry,
                                        const std::uint32_t* args,std::size_t count){
    if(!user)return 0u;
    auto& state=*static_cast<NativeEventFunction36State*>(user);
    if(pc_entry==0x005802ddu&&count==3u){
        ++state.frontend_bulk_format_calls;
        return 0u;
    }
    if(pc_entry==0x004f1a90u&&count==2u){
        ++state.frontend_bulk_bind_calls;
        const auto record=args[1];
        if(record<0x0084b3c0u||record>=0x0084b400u||
           ((record-0x0084b3c0u)&0x0fu)!=0u)return 0u;
        const auto lane=(record-0x0084b3c0u)/0x10u;
        constexpr std::array<std::uint32_t,4> keys{{0x5b457cu,0x5b4590u,0x5b4554u,0x5b4568u}};
        if(args[0]!=keys[lane]||state.frontend_course_tables[lane].count==0u){
            state.frontend_bulk_missing_pc=0x4f1a90u;return 0u;
        }
        // Opaque native token resolves to owned, validated course records.
        // No pointer truncation and no successful binding of mere file bytes.
        return 0x72000000u+lane;
    }
    if(pc_entry!=0x004f12a0u||count!=4u)return 0u;
    const auto record=args[1];
    std::uint32_t lane=LoaderAssetFrontendScriptCount;
    if(record>=0x0084b3c0u&&record<0x0084b400u&&
       ((record-0x0084b3c0u)&0x0fu)==0u)
        lane=(record-0x0084b3c0u)/0x10u;
    else if(record>=0x0084b400u&&record<0x0084b7c0u&&
            ((record-0x0084b400u)&0x0fu)==0u)
        lane=4u+(record-0x0084b400u)/0x10u;
    if(lane>=LoaderAssetFrontendScriptCount)return 0u;
    ++state.frontend_bulk_requests;
    state.frontend_bulk_last_lane=lane;
    state.frontend_bulk_pending=1u;
    const auto* asset=runtime_asset(state,LoaderAssetFrontendScriptBaseId+lane,0u);
    state.frontend_bulk_missing_pc=0x4f12a0u;
    if(!asset)return 0u;
    driving::PcRelocCategoryBlobR077 blob{};
    if(!driving::course_reloc_blob_open_r077(
           const_cast<std::uint8_t*>(asset->bytes.data()),asset->bytes.size(),blob))return 0u;
    if(lane<state.frontend_course_tables.size()&&
       !parse_frontend_course_table(lane,asset->bytes.data(),asset->bytes.size(),
                                    state.frontend_course_tables[lane]))return 0u;
    state.frontend_bulk_missing_pc=0u;
    ++state.frontend_bulk_ready_count;
    state.frontend_bulk_ready_bytes+=static_cast<std::uint32_t>(asset->bytes.size());
    state.frontend_bulk_pending=
        state.frontend_bulk_ready_count==LoaderAssetFrontendScriptCount?0u:1u;
    return 1u;
}

std::uint32_t native_shared_loader_call(void* user,std::uint32_t pc_entry,
                                        const std::uint32_t* args,std::size_t count){
    if(!user)return 0u;
    auto& state=*static_cast<NativeEventFunction36State*>(user);
    if(pc_entry==0x00427700u&&count==1u&&state.runtime)return state.runtime->pc_sound.request_427700(args[0]);
    if(pc_entry==0x0042deb0u&&count==2u){
        if(args[0]==0x44u&&args[1]==9u){
            ++state.frontend_resource_requests;
            state.frontend_resource_pending=1u;
            const auto* asset=runtime_asset(state,0x44u,9u);
            if(asset&&state.frontend_ready_count==0u){
                state.frontend_ready_count=1u;
                state.frontend_ready_bytes=static_cast<std::uint32_t>(asset->bytes.size());
                state.frontend_resource_pending=0u;
            }
            return 0u;
        }
        ++state.shared_resource_requests;
        state.shared_last_resource=args[0];
        state.shared_last_mode=args[1];
        const auto index=args[0]==0x2cu?0u:args[0]==0x33u?1u:args[0]==0x48u?2u:3u;
        if(index<state.shared_resource_ready.size()&&args[1]==8u&&
           state.shared_resource_ready[index]==0u){
            const auto* asset=runtime_asset(state,args[0],args[1]);
            if(asset){
                state.shared_resource_ready[index]=1u;
                ++state.shared_ready_count;
                state.shared_ready_bytes+=static_cast<std::uint32_t>(asset->bytes.size());
            }
        }
        state.shared_resource_pending=
            state.shared_ready_count==state.shared_resource_ready.size()?0u:1u;
        return 0u;
    }
    if(pc_entry==0x0042df90u){
        if(state.frontend_resource_requests!=0u)
            return state.frontend_resource_pending==0u&&state.frontend_ready_count==1u?1u:0u;
        return state.shared_resource_pending==0u&&
               state.shared_ready_count==state.shared_resource_ready.size()?1u:0u;
    }
    if(pc_entry==0x00429920u&&count==2u){
        if(args[0]==0x44u&&args[1]==9u)++state.frontend_release_calls;
        else ++state.shared_release_calls;
        return 0u;
    }
    if(pc_entry==0x00448ab0u){
        ++state.shared_table_reset_calls;
        if(state.runtime)object_db_reset_448ab0(state.runtime->object_db);
        return 0u;
    }
    if(pc_entry==0x004299a0u)return 1u;
    if(pc_entry==0x00448b90u){
        // Object name database (object_db.hpp): native_object_db_step_448b90.
        if(!state.runtime)return 0u;
        return native_object_db_step_448b90(*state.runtime);
    }
    if(pc_entry==0x0040ed70u){
        // 40ED70: the morph variant table 89BDB8 (1024 x {base, count, values}) from
        // \media\morph_var.dat: N groups {index, k, m, m floats}; k entries from index share the
        // m floats with bases 0, m/k, 2m/k...; 95AF10 = 1. 40EE20 returning no file leaves it empty.
        ++state.shared_finalize_calls;
        state.morph_table_89bdb8={};state.morph_values.clear();state.morph_loaded=true;
        std::vector<std::uint8_t> file;std::string err;
        if(!state.retail_assets||!retail_asset_read_relative(*state.retail_assets,"Media/morph_var.dat",file,1u<<20,&err)){
            outrun::driving::service_hole("40ED70","\\media\\morph_var.dat");return 1u;}
        auto rd=[&](std::size_t at){if(at+4u>file.size())throw std::out_of_range("40ED70: morph_var.dat truncated");
            std::uint32_t v;std::memcpy(&v,file.data()+at,4);return v;};
        state.morph_values.resize(file.size()/4u);
        for(std::size_t i=0;i<state.morph_values.size();++i)std::memcpy(&state.morph_values[i],file.data()+i*4u,4);
        const std::uint32_t groups=rd(0);std::size_t at=4;
        for(std::uint32_t g=0;g<groups;++g){
            const std::uint32_t index=rd(at),k=rd(at+4),m=rd(at+8);at+=12;
            const std::uint32_t values=std::uint32_t(at/4u);
            at+=std::size_t(m)*4u;
            if(!k)continue;
            const std::uint32_t step=m/k;std::uint32_t base=0;
            for(std::uint32_t j=0;j<k;++j){
                if(index+j>=0x400u)throw std::out_of_range("40ED70: morph entry outside 89BDB8");
                state.morph_table_89bdb8[index+j]={base,m,values};base+=step;
            }
        }
        return 1u;
    }
    if(pc_entry==0x004239c0u&&count==2u){
        const auto* asset=runtime_asset(state,LoaderAssetSelectTableId,0u);
        if(!asset)return 0u;
        ++state.select_table_open_calls;
        return 0x71000001u;
    }
    if(pc_entry==0x00423cb0u&&count==4u&&args[0]==0x00844a08u&&
       args[1]==0xb0u&&args[2]==0x80u&&args[3]==0x71000001u){
        const auto* asset=runtime_asset(state,LoaderAssetSelectTableId,0u);
        if(asset&&asset->bytes.size()==LoaderAssetSelectTableBytes){
            ++state.select_table_read_calls;
            state.select_table_bytes=static_cast<std::uint32_t>(asset->bytes.size());
            state.car_select.select_table_844a08=asset->bytes; // 844A08 = common/sel_dl_edit0.tgt
            return 0x80u;
        }
        return 0u;
    }
    if(pc_entry==0x00423bd0u&&count==1u&&args[0]==0x71000001u){
        ++state.select_table_close_calls;
        return 0u;
    }
    if(pc_entry==0x00496170u||pc_entry==0x004e85e0u){
        ++state.frontend_init_calls;
        if(pc_entry==0x00496170u&&state.runtime)
            state.runtime->mission.manager.status_836358=1u;
        if(pc_entry==0x004e85e0u){
            state.frontend_bulk_loader={};
            state.frontend_course_tables={};
            state.frontend_category_progress.categories_ready=false;
            state.frontend_bulk_missing_pc=0u;
            state.frontend_bulk_requests=state.frontend_bulk_ready_count=0u;
            state.frontend_bulk_ready_bytes=state.frontend_bulk_format_calls=0u;
            state.frontend_bulk_bind_calls=state.frontend_bulk_last_lane=0u;
            state.frontend_bulk_pending=1u;
            driving::frontend_bulk_loader_initialize_4e85e0(
                state.frontend_bulk_loader);
        }
        return 0u;
    }
    if(pc_entry==0x00496180u){
        ++state.frontend_async_polls;
        if(!state.runtime)return 0u;
        auto& mission=state.runtime->mission;
        auto& manager=mission.manager;
        manager.selection_836374=0u;
        state.runtime->start_mode.selection_active_836374=false;
        if(std::int32_t(manager.status_836358)>=3)return 1u;
        const auto* source=state.runtime->start_mode.scene_owner_race_assets;
        RaceAssetPack validated;std::vector<std::uint8_t> relocated;
        if(!source||!parse_race_asset_pack(source->bytes.data(),source->bytes.size(),validated)||
           !pc_relocate_blob(validated.bytes,NativeRacesBlobBase,relocated))return 0u;
        mission.races_relocated=std::move(relocated);
        mission.view=PcAddressView{};
        mission.view.add(NativeRacesBlobBase,mission.races_relocated.data(),mission.races_relocated.size());
        mission.view.add_exe();mission.view_ready=true;
        manager.loader_836350=NativeRacesBlobBase;
        manager.buffer_836354=NativeRacesBlobBase;
        manager.status_836358=3u;
        return 1u;
    }
    if(pc_entry==0x004e8620u){
        ++state.frontend_bulk_polls;
        return driving::frontend_bulk_loader_4e8620(
            state.frontend_bulk_loader,{&state,native_frontend_bulk_call})?1u:0u;
    }
    if(pc_entry==0x00448980u)return state.loader_resource_pending==0u?1u:0u;
    // ---- robot motion tables (course-prog): begin ----
    // 49E580 stage 0: RobMotion system init 4F2470 (84D970/84D974 owner).
    if(pc_entry==0x004f2470u){if(state.runtime)native_rob_motion_tables_init(*state.runtime);return 0u;}
    // ---- robot motion tables (course-prog): end ----
    return 0u;
}

// Parent key 36 (4CB300 family): slots 0 destroy, 4 init, 8 control,
// 12 display, 16 suspend. Faults latch with their PC address.
std::uint32_t native_rankings_virtual(NativeEventFunction36State& state,std::uint32_t slot){
    auto& child=state.rankings;
    if(child.fault&&slot!=0&&slot!=16&&slot!=0x4cb300)return 0;
    FrontendUiResources ui{state.frontend_sprites};ui.motion_step=state.frontend_ui_motion_step;
    ui.effect_user=state.frontend_effect_user;ui.effect_4249f0=state.frontend_effect;
    FrontendRankingServices svc{ui,driving::Bytes(state.object.data(),state.object.size()),state.frontend_ui_globals,
        state.frontend_profiles.active,state.frontend_profiles.common,state.frontend_input,state.title_base_global_6591e4,
        state.ranking_globals,state.frontend_fonts,state.frontend_text,state.frontend_choice_timer};
    bool ok=false;unsigned result{};
    if(slot==0x4cb300){ok=frontend_rankings_construct_4cb300(child,svc);result=ok;}
    else if(slot==4){ok=frontend_rankings_init_4caf80(child,svc);result=ok;}
    else if(slot==8)ok=frontend_rankings_control_4cb480(child,svc,result);
    else if(slot==12)ok=frontend_rankings_display_4cc390(child,svc);
    else if(slot==16)ok=frontend_rankings_suspend_4ca6f0(child,svc);
    else if(slot==0)ok=frontend_rankings_destroy_4caeb0(child,svc);        // 4CB460 -> 4CAEB0
    if(!ok){if(!child.fault)child.fault=svc.missing?svc.missing:0x4cb480;state.frontend_ui_missing_pc=child.fault;return 0;}
    return result;
}

// Parent key 37 (4CEBF0 Time Attack board).
std::uint32_t native_ghosts_virtual(NativeEventFunction36State& state,std::uint32_t slot){
    auto& child=state.ghost_board;
    if(child.fault&&slot!=0&&slot!=16&&slot!=0x4cebf0)return 0;
    FrontendUiResources ui{state.frontend_sprites};ui.motion_step=state.frontend_ui_motion_step;
    ui.effect_user=state.frontend_effect_user;ui.effect_4249f0=state.frontend_effect;
    FrontendRankingServices svc{ui,driving::Bytes(state.object.data(),state.object.size()),state.frontend_ui_globals,
        state.frontend_profiles.active,state.frontend_profiles.common,state.frontend_input,state.title_base_global_6591e4,
        state.ranking_globals,state.frontend_fonts,state.frontend_text,state.frontend_choice_timer};
    bool ok=false;unsigned result{};
    if(slot==0x4cebf0){ok=frontend_ghosts_construct_4cebf0(child,svc);result=ok;}
    else if(slot==4){ok=frontend_ghosts_init_4cece0(child,svc);result=ok;}
    else if(slot==8)ok=frontend_ghosts_control_4cef30(child,svc,result);
    else if(slot==12)ok=frontend_ghosts_display_4ce3b0(child,svc);
    else if(slot==16)ok=frontend_ghosts_suspend_4ce3e0(child,svc);
    else if(slot==0)ok=frontend_ghosts_destroy_4ce830(child,svc);         // 4CECC0 -> 4CE830
    if(!ok){if(!child.fault)child.fault=svc.missing?svc.missing:0x4cef30;state.frontend_ui_missing_pc=child.fault;return 0;}
    return result;
}

// Parent key 38 (4D05E0 Time Attack course board).
std::uint32_t native_courses_virtual(NativeEventFunction36State& state,std::uint32_t slot){
    auto& child=state.course_board;
    if(child.fault&&slot!=0&&slot!=16&&slot!=0x4d05e0)return 0;
    FrontendUiResources ui{state.frontend_sprites};ui.motion_step=state.frontend_ui_motion_step;
    ui.effect_user=state.frontend_effect_user;ui.effect_4249f0=state.frontend_effect;
    FrontendRankingServices svc{ui,driving::Bytes(state.object.data(),state.object.size()),state.frontend_ui_globals,
        state.frontend_profiles.active,state.frontend_profiles.common,state.frontend_input,state.title_base_global_6591e4,
        state.ranking_globals,state.frontend_fonts,state.frontend_text,state.frontend_choice_timer};
    bool ok=false;unsigned result{};
    if(slot==0x4d05e0){ok=frontend_courses_construct_4d05e0(child,svc);result=ok;}
    else if(slot==4){ok=frontend_courses_init_4d01a0(child,svc);result=ok;}
    else if(slot==8)ok=frontend_courses_control_4d06d0(child,svc,result);
    else if(slot==12)ok=frontend_courses_display_4cfb10(child,svc);
    else if(slot==16)ok=frontend_courses_suspend_4cfb40(child,svc);
    else if(slot==0)ok=frontend_courses_destroy_4d00d0(child,svc);         // 4D06B0 -> 4D00D0
    if(!ok){if(!child.fault)child.fault=svc.missing?svc.missing:0x4d06d0;state.frontend_ui_missing_pc=child.fault;return 0;}
    return result;
}

// Rankings menu: key 51 (4CE170) and keys 48/57 (4CCFC0), same slots as key 36.
// Keys 50/53 (4DA2F0): rankings list.
std::uint32_t native_rankings_list_virtual(NativeEventFunction36State& state,std::uint32_t slot){
    auto& child=state.board50;
    if(child.fault&&slot!=0&&slot!=16&&slot!=0x4da2f0)return 0;
    FrontendUiResources ui{state.frontend_sprites};ui.motion_step=state.frontend_ui_motion_step;
    ui.effect_user=state.frontend_effect_user;ui.effect_4249f0=state.frontend_effect;
    FrontendRankingServices svc{ui,driving::Bytes(state.object.data(),state.object.size()),state.frontend_ui_globals,
        state.frontend_profiles.active,state.frontend_profiles.common,state.frontend_input,state.title_base_global_6591e4,
        state.ranking_globals,state.frontend_fonts,state.frontend_text,state.frontend_choice_timer};
    svc.records_65f0c0=state.frontend_records.records.data();
    if(state.runtime){auto& sp=native_race_end(*state.runtime).state.sp_tables;svc.sp_tables_84df40=sp.data();svc.sp_tables_size=sp.size();}
    bool ok=false;unsigned result{};
    if(slot==0x4da2f0){ok=frontend_board50_construct_4da2f0(child,svc);result=ok;}
    else if(slot==4){ok=frontend_board50_init_4d9d70(child,svc);result=ok;}
    else if(slot==8)ok=frontend_board50_control_4da3f0(child,svc,result);
    else if(slot==12)ok=frontend_board50_display_4d9430(child,svc);
    else if(slot==16)ok=frontend_board50_suspend_4d9460(child,svc);
    else if(slot==0)ok=frontend_board50_destroy_4d9cb0(child,svc);
    if(!ok){if(!child.fault)child.fault=svc.missing?svc.missing:0x4da3f0;state.frontend_ui_missing_pc=child.fault;return 0;}
    return result;
}
std::uint32_t native_rankings_menu_virtual(NativeEventFunction36State& state,bool board,std::uint32_t slot){
    auto& child=board?state.board48:state.mode_board;
    const unsigned ctor=board?0x4ccfc0:0x4ce170,control=board?0x4cd140:0x4cdf40;
    if(child.fault&&slot!=0&&slot!=16&&slot!=ctor)return 0;
    FrontendUiResources ui{state.frontend_sprites};ui.motion_step=state.frontend_ui_motion_step;
    ui.effect_user=state.frontend_effect_user;ui.effect_4249f0=state.frontend_effect;
    FrontendRankingServices svc{ui,driving::Bytes(state.object.data(),state.object.size()),state.frontend_ui_globals,
        state.frontend_profiles.active,state.frontend_profiles.common,state.frontend_input,state.title_base_global_6591e4,
        state.ranking_globals,state.frontend_fonts,state.frontend_text,state.frontend_choice_timer};
    svc.records_65f0c0=state.frontend_records.records.data();
    if(state.runtime){auto& sp=native_race_end(*state.runtime).state.sp_tables;svc.sp_tables_84df40=sp.data();svc.sp_tables_size=sp.size();}
    bool ok=false;unsigned result{};
    if(slot==ctor){ok=board?frontend_board48_construct_4ccfc0(child,svc):frontend_modes_construct_4ce170(child,svc);result=ok;}
    else if(slot==4){ok=board?frontend_board48_init_4ccbc0(child,svc):frontend_modes_init_4cdc10(child,svc);result=ok;}
    else if(slot==8)ok=board?frontend_board48_control_4cd140(child,svc,result):frontend_modes_control_4cdf40(child,svc,result);
    else if(slot==12)ok=board?frontend_rankings_display_4cc390(child,svc):true;     // 4CC390 / 49A650
    else if(slot==16)ok=board?frontend_board48_suspend_4cc3c0(child,svc):frontend_modes_suspend_4cdd50(child,svc);
    else if(slot==0)ok=board?frontend_board48_destroy_4ccaf0(child,svc):frontend_modes_destroy_4cdea0(child,svc);
    if(!ok){if(!child.fault)child.fault=svc.missing?svc.missing:control;state.frontend_ui_missing_pc=child.fault;return 0;}
    return result;
}

std::uint32_t native_stage12_factory_allocate(void* user,std::uint32_t pc_entry,
                                               std::uint32_t bytes){
    if(!user||pc_entry!=0x005802cfu)return 0u;
    auto& state=*static_cast<NativeEventFunction36State*>(user);
    if(bytes==state.frontend_root_object.size()){
        ++state.stage12_factory_allocate_calls;
        state.stage12_last_handle=0x73000001u;
        return state.stage12_last_handle;
    }
    if(bytes==state.frontend_gate_object.size()){
        ++state.state2_factory_allocate_calls;
        state.state2_last_handle=0x73000002u;
        return state.state2_last_handle;
    }
    if(bytes==state.frontend_first_menu_object.size()){
        if(state.frontend_factory_key==1u){
            state.frontend_first_menu_handle=0x73000003u;
            return state.frontend_first_menu_handle;
        }
        state.frontend_second_menu_handle=0x73000004u;
        return state.frontend_second_menu_handle;
    }
    if(bytes==state.title_owner_object.size()){
        state.title_last_handle=0x73000005u;
        return state.title_last_handle;
    }
    if(bytes==PcNetworkMenuBytes&&state.frontend_factory_key==6u)return 0x73000015;
    if(bytes==PcNetworkMenuBytes&&state.frontend_factory_key==7u)return 0x73000017;
    if(bytes==PcLanMenuBytes&&state.frontend_factory_key==8u)return 0x73000016;
    if(state.license_owners&&bytes==PcLicenseChooserBytes)return 0x73000006;
    if(state.license_owners&&bytes==PcLicenseEditorBytes)return 0x73000007;
    if(bytes==PcCategoryOwnerBytes&&state.frontend_factory_key==0x3a)return 0x73000008;
    if(bytes==PcMissionOwnerBytes&&state.frontend_factory_key==0x3b)return 0x73000009;
    if(bytes==PcRequestOwnerBytes&&state.frontend_factory_key==0x3c)return 0x73000018;
    if(bytes==state.car_select.menu.object.size()&&state.frontend_factory_key==10u&&state.runtime)return 0x7300000a;
    if(bytes==state.transmission.object.size()&&state.frontend_factory_key==16u&&state.runtime)return 0x7300000b;
    if(bytes==state.music.object.size()&&state.frontend_factory_key==4u&&state.runtime)return 0x7300000c;
    if(bytes==PcRankingOwnerBytes&&state.frontend_factory_key==36u)return 0x7300000d;
    if(bytes==PcGhostBoardBytes&&state.frontend_factory_key==37u)return 0x7300000e;
    if(bytes==PcCourseBoardBytes&&state.frontend_factory_key==38u)return 0x7300000f;
    if(bytes==PcShowroomBytes&&state.frontend_factory_key==43u&&state.runtime)return 0x73000010;
    if(bytes==PcRankingSelectorBytes&&state.frontend_factory_key==49u)return 0x73000011;
    if(bytes==PcModeBoardBytes&&state.frontend_factory_key==51u)return 0x73000012;
    if(bytes==PcBoard50Bytes&&(state.frontend_factory_key==50u||state.frontend_factory_key==53u))return 0x73000014;
    if(bytes==PcRankingOwnerBytes&&(state.frontend_factory_key==48u||state.frontend_factory_key==57u))return 0x73000013;
    return 0u;
}

std::uint32_t native_stage12_factory_construct(void* user,std::uint32_t pc_entry,
                                                std::uint32_t object_token){
    if(!user||object_token==0u)return 0u;
    auto& state=*static_cast<NativeEventFunction36State*>(user);
    if((pc_entry==0x4c5cd0&&object_token==0x73000015)||(pc_entry==0x4c60b0&&object_token==0x73000016)||(pc_entry==0x4c5ef0&&object_token==0x73000017)){
        auto& menu=object_token==0x73000015?state.network_menu6:object_token==0x73000017?state.online_menu7:state.lan_menu8;
        std::fill(menu.object.begin(),menu.object.end(),0);menu.fault=0;
        menu.constructed=network_menu_construct(menu.object.data(),menu.object.size(),object_token==0x73000015?6:object_token==0x73000017?7:8,state.title_base_global_6591e4);
        return menu.constructed?object_token:0;
    }
    if(pc_entry==0x4e8130&&object_token==0x73000008)
        return frontend_categories_construct_4e8130(state.frontend_categories,state.title_base_global_6591e4)?object_token:0;
    if(pc_entry==0x4cb300&&object_token==0x7300000d)
        return native_rankings_virtual(state,0x4cb300)?object_token:0;
    if(pc_entry==0x4cebf0&&object_token==0x7300000e)
        return native_ghosts_virtual(state,0x4cebf0)?object_token:0;
    if(pc_entry==0x4d05e0&&object_token==0x7300000f)
        return native_courses_virtual(state,0x4d05e0)?object_token:0;
    if(pc_entry==0x4da2f0&&object_token==0x73000014)return native_rankings_list_virtual(state,0x4da2f0)?object_token:0;
    if(pc_entry==0x4ce170&&object_token==0x73000012)return native_rankings_menu_virtual(state,false,0x4ce170)?object_token:0;
    if(pc_entry==0x4ccfc0&&object_token==0x73000013)return native_rankings_menu_virtual(state,true,0x4ccfc0)?object_token:0;
    if(pc_entry==0x4d9010&&object_token==0x73000011){
        state.ranking_selector_fault=0;
        return ranking_selector_construct_4d9010(state.ranking_selector.data(),state.ranking_selector.size(),state.title_base_global_6591e4)?object_token:0;
    }
    if(pc_entry==0x4d4230&&object_token==0x73000010&&state.runtime)
        return native_showroom_virtual(*state.runtime,0x4d4230u)?object_token:0;
    if(pc_entry==0x4c9a90&&object_token==0x7300000c&&state.runtime)
        return native_music_virtual(*state.runtime,0x4c9a90u)?object_token:0;
    if(pc_entry==0x4dd410&&object_token==0x7300000b&&state.runtime)
        return native_transmission_virtual(*state.runtime,0x4dd410u)?object_token:0;
    if(pc_entry==0x4c8de0&&object_token==0x7300000a&&state.runtime)
        return native_car_select_virtual(*state.runtime,0x4c8de0u)?object_token:0;
    if(pc_entry==0x4e9160&&object_token==0x73000009)
        return frontend_missions_construct_4e9160(state.frontend_missions,state.title_base_global_6591e4)?object_token:0;
    if(pc_entry==0x4e9e50&&object_token==0x73000018)
        return frontend_requests_construct_4e9e50(state.frontend_requests,state.title_base_global_6591e4)?object_token:0;
    if(state.license_owners&&((pc_entry==0x4e1890&&object_token==0x73000006)||
       (pc_entry==0x4dd5c0&&object_token==0x73000007)))
        return state.license_owners->construct(pc_entry==0x4e1890?21:24)?object_token:0;
    if(pc_entry==0x004c5120u&&object_token==state.stage12_last_handle){
        ++state.stage12_factory_construct_calls;
        return object_token;
    }
    if(pc_entry==0x004dec60u&&object_token==state.state2_last_handle){
        ++state.state2_factory_construct_calls;
        return object_token;
    }
    if(pc_entry==0x004c5550u&&object_token==state.frontend_first_menu_handle)
        return object_token;
    if(pc_entry==0x004c5980u&&object_token==state.frontend_second_menu_handle)
        return object_token;
    if(pc_entry==0x004d7140u&&object_token==state.title_last_handle){
        if(state.title_widgets)state.title_widgets->reset();
        if(!title_owner_construct_complete_4d7140(
               state.title_owner_object.data(),state.title_owner_object.size(),
               state.title_game_state_780270,state.title_flag_95b250,
               state.title_base_global_6591e4))return 0u;
        ++state.title_construct_calls;
        return object_token;
    }
    return 0u;
}

std::uint32_t native_frontend_commands(NativeEventFunction36State& state,std::uint32_t pc,
    driving::Bytes object,std::size_t offset,const std::uint32_t* args,std::size_t count){
    FrontendUiResources ui{state.frontend_sprites};ui.motion_step=state.frontend_ui_motion_step;
    std::uint32_t result{};
    if(!ui.commands(pc,object.sub(offset,object.size()-offset),args,count,state.frontend_ui_globals,result))
        state.frontend_ui_missing_pc=ui.missing_pc;
    return result;
}
void native_stage12_embedded_void(void* user,std::uint32_t pc_entry,
                                  driving::Bytes object,std::size_t offset){
    if(!user)return;
    native_frontend_commands(*static_cast<NativeEventFunction36State*>(user),pc_entry,object,offset,nullptr,0);
}

void native_stage12_embedded_u32(void* user,std::uint32_t pc_entry,
                                 driving::Bytes object,std::size_t offset,std::uint32_t arg){
    if(!user)return;
    native_frontend_commands(*static_cast<NativeEventFunction36State*>(user),pc_entry,object,offset,&arg,1);
}

// Keys whose screens run in the network layer (reports/decomp/network.md).
constexpr std::uint32_t NetworkScreenKeys[]{11,12,13,14,15,17,19,25,26,27,28,34,46,61,65,70,71,72,73,74};
bool native_network_screen_key(std::uint32_t key){for(const auto k:NetworkScreenKeys)if(k==key)return true;return false;}
std::uint32_t native_network_screen_index(std::uint32_t key){
    for(std::uint32_t i=0;i<std::size(NetworkScreenKeys);++i)if(NetworkScreenKeys[i]==key)return i;return 0;}
bool native_network_screen_handle(std::uint32_t handle){
    return handle>=0x73000100u&&handle<0x73000160u&&native_network_screen_key(handle-0x73000100u);}
std::uint32_t native_stage12_callback(void* user,std::uint32_t callback_token,
                                      std::size_t table_index){
    if(!user)return 0u;
    auto& state=*static_cast<NativeEventFunction36State*>(user);
    const driving::PcFactoryServices services{
        user,native_stage12_factory_allocate,native_stage12_factory_construct};
    state.frontend_factory_key=static_cast<std::uint32_t>(table_index);
    std::uint32_t handle{};
    void* storage{};
    std::size_t storage_size{};
    std::size_t binding_index{};
    if(callback_token==0x00441450u&&table_index==0u){
        handle=driving::object_factory_441450(services);
        storage=state.frontend_root_object.data();
        storage_size=state.frontend_root_object.size();
        binding_index=0u;
    }else if(callback_token==0x00441f90u&&table_index==22u){
        ++state.state2_callback_dispatch_calls;
        handle=driving::object_factory_441f90(services);
        storage=state.frontend_gate_object.data();
        storage_size=state.frontend_gate_object.size();
        binding_index=1u;
    }else if(callback_token==0x004414b0u&&table_index==1u){
        handle=driving::object_factory_4414b0(services);
        storage=state.frontend_first_menu_object.data();
        storage_size=state.frontend_first_menu_object.size();
        binding_index=2u;
    }else if(callback_token==0x00441510u&&table_index==2u){
        handle=driving::object_factory_441510(services);
        storage=state.frontend_second_menu_object.data();
        storage_size=state.frontend_second_menu_object.size();
        binding_index=3u;
    }else if(callback_token==0x00441b10u&&table_index==44u){
        handle=driving::object_factory_441b10(services);
        storage=state.title_owner_object.data();
        storage_size=state.title_owner_object.size();
        binding_index=4u;
    }else if(state.license_owners&&callback_token==0x442110&&table_index==21){
        handle=driving::object_factory_442110(services);
        storage=state.license_owners->storage(21);storage_size=state.license_owners->size(21);binding_index=5;
    }else if(state.license_owners&&callback_token==0x441e70&&table_index==24){
        handle=driving::object_factory_441e70(services);
        storage=state.license_owners->storage(24);storage_size=state.license_owners->size(24);binding_index=6;
    }else if(callback_token==0x442590&&table_index==0x3a){
        handle=driving::object_factory_442590(services);
        storage=state.frontend_categories.object.data();storage_size=PcCategoryOwnerBytes;binding_index=7;
    }else if(callback_token==0x441810&&table_index==4&&state.runtime){
        handle=driving::object_factory_441810(services);
        storage=state.music.object.data();storage_size=state.music.object.size();binding_index=11;
    }else if(callback_token==0x441e10&&table_index==16&&state.runtime){
        handle=driving::object_factory_441e10(services);
        storage=state.transmission.object.data();storage_size=state.transmission.object.size();binding_index=10;
    }else if(callback_token==0x4417b0&&table_index==10&&state.runtime){
        handle=driving::object_factory_4417b0(services);
        storage=state.car_select.menu.object.data();storage_size=state.car_select.menu.object.size();binding_index=9;
    }else if(callback_token==0x441870&&table_index==36){
        handle=driving::object_factory_441870(services);
        storage=state.rankings.object.data();storage_size=state.rankings.object.size();binding_index=12;
    }else if(callback_token==0x441990&&table_index==37){
        state.ghost_board.object.assign(PcGhostBoardBytes,0);   // before 4CEBF0 constructs it
        handle=driving::object_factory_441990(services);
        storage=state.ghost_board.object.data();storage_size=state.ghost_board.object.size();binding_index=13;
    }else if(callback_token==0x4419f0&&table_index==38){
        state.course_board.object.assign(PcCourseBoardBytes,0);   // before 4D05E0 constructs it
        handle=driving::object_factory_4419f0(services);
        storage=state.course_board.object.data();storage_size=state.course_board.object.size();binding_index=14;
    }else if(callback_token==0x441bd0&&(table_index==50||table_index==53)){
        state.board50.object.assign(PcBoard50Bytes,0);   // before 4DA2F0 constructs it
        handle=driving::object_factory_441bd0(services);
        storage=state.board50.object.data();storage_size=state.board50.object.size();binding_index=19;
    }else if(callback_token==0x441930&&table_index==51){
        state.mode_board.object.assign(PcModeBoardBytes,0);   // before 4CE170 constructs it
        handle=driving::object_factory_441930(services);
        storage=state.mode_board.object.data();storage_size=state.mode_board.object.size();binding_index=17;
    }else if(callback_token==0x4418d0&&(table_index==48||table_index==57)){
        handle=driving::object_factory_4418d0(services);
        storage=state.board48.object.data();storage_size=state.board48.object.size();binding_index=18;
    }else if(callback_token==0x441b70&&table_index==49){
        handle=driving::object_factory_441b70(services);
        storage=state.ranking_selector.data();storage_size=state.ranking_selector.size();binding_index=16;
    }else if(callback_token==0x441ab0&&table_index==43&&state.runtime){
        handle=driving::object_factory_441ab0(services);
        storage=state.showroom.object.data();storage_size=state.showroom.object.size();binding_index=15;
    }else if(native_network_screen_key(std::uint32_t(table_index))&&state.runtime&&native_network_mapped()){
        // Network screens run translated in the network layer (their PC factory and vtable).
        const auto key=std::uint32_t(table_index);
        const auto object=native_network_screen_create(*state.runtime,callback_token);
        const auto size=object?native_network_size(object):0u;
        storage=object?native_network_view(object,size):nullptr;
        if(!storage){state.frontend_last_missing_key=key;return 0u;}
        storage_size=size;handle=0x73000100u+key;binding_index=22u+native_network_screen_index(key);
        state.network_screens[key]=object;
    }else if(callback_token==0x441570&&table_index==6){
        handle=driving::object_factory_441570(services);
        storage=state.network_menu6.object.data();storage_size=state.network_menu6.object.size();binding_index=20;
    }else if(callback_token==0x4415d0&&table_index==7){
        handle=driving::object_factory_4415d0(services);
        storage=state.online_menu7.object.data();storage_size=state.online_menu7.object.size();binding_index=42;
    }else if(callback_token==0x441630&&table_index==8){
        handle=driving::object_factory_441630(services);
        storage=state.lan_menu8.object.data();storage_size=state.lan_menu8.object.size();binding_index=21;
    }else if(callback_token==0x4425f0&&table_index==0x3b){
        handle=driving::object_factory_4425f0(services);
        storage=state.frontend_missions.object.data();storage_size=PcMissionOwnerBytes;binding_index=8;
    }else if(callback_token==0x442650&&table_index==0x3c){
        handle=driving::object_factory_442650(services);
        storage=state.frontend_requests.object.data();storage_size=PcRequestOwnerBytes;binding_index=43;
    }else {state.frontend_last_missing_key=static_cast<std::uint32_t>(table_index);return 0u;}
    if(handle==0u)return 0u;
    if(binding_index<4u){
        std::memset(storage,0,storage_size);
        driving::Bytes(storage,storage_size).put32(0x08u,
                                                   static_cast<std::uint32_t>(table_index));
    }
    if(binding_index==1u){
        // 0x4DEC60 constructor defaults.  The source's +0x17B4 value is an
        // animation-exit action latch, not a ready-to-dispatch owner action.
        driving::Bytes(storage,storage_size).put32(0x17b4u,2u);
        driving::frontend_gate_list_initialize_4ded80(
            driving::Bytes(storage,storage_size),state.frontend_profile_count,
            state.frontend_gate_list);
    }else if(binding_index==2u||binding_index==3u){
        state.frontend_choice_faults[binding_index-2]=0;
        if(!fixed_choice_construct(static_cast<std::uint8_t*>(storage),storage_size,
                binding_index==2u?1u:2u,state.title_base_global_6591e4))return 0;
    }
    state.frontend_handle_bindings[binding_index]={handle,storage,storage_size};
    if(state.frontend_handle_count<binding_index+1u)
        state.frontend_handle_count=binding_index+1u;
    return handle;
}

// 443EB0 globals: 43F9E0(1) ([780270] = 1) and 440930 (the event pause: 43F9D0(1) [780248],
// 429810(1) [95B214], 449040(1) [7D2614]) when the race owner opens the pause menu.
void native_stage12_dispatch_global(void* user,std::uint32_t pc,std::uint32_t arg,bool){
    if(!user)return;
    auto& state=*static_cast<NativeEventFunction36State*>(user);
    if(pc==0x43f9e0u){state.title_game_state_780270=std::uint8_t(arg);return;}
    if(pc==0x440930u&&state.title_event_state&&state.title_game_flag_780248){
        driving::set_ev_pause_flag_440930(*state.title_event_state,{&state,
            [](void* u,std::uint32_t f,std::uint32_t a){
                auto& st=*static_cast<NativeEventFunction36State*>(u);
                if(f==0x43f9d0u)*st.title_game_flag_780248=std::uint8_t(a);
                else if(f==0x429810u)st.title_pause_flag_95b214=a;
                else if(f==0x449040u)st.title_pause_flag_7d2614=a;}});
        ++state.title_pause_calls;
    }
}

// 401000 (channel, track, loop) / 401030 (channel) / 401050: the streamed BGM
// requests, recorded and handed to the platform player when one is bound.
static std::uint32_t native_music_call(NativeEventFunction36State& f,std::uint32_t pc,const std::uint32_t* args,std::size_t count){
    if(pc==0x401000u&&count==3u){
        ++f.music_play_requests;f.music_last_track=args[1];
        if(f.music_play)(void)f.music_play(f.music_user,args[0],args[1],args[2]);return 1u;}
    if(pc==0x401030u||pc==0x401050u){
        ++f.music_stop_requests;if(f.music_stop)(void)f.music_stop(f.music_user,count?args[0]:0u);return pc==0x401030u?1u:0u;}
    return 0u;
}
std::uint32_t native_stage12_global_call(void* user,std::uint32_t pc_entry,
                                         const std::uint32_t* args,std::size_t count){
    if(!user)return 0u;
    auto& state=*static_cast<NativeEventFunction36State*>(user);
    switch(pc_entry){
        case 0x00428880u:return count==1u?state.frontend_sprites.status(args[0]):0u;
        case 0x004285a0u:if(count==1u)state.frontend_sprites.release(args[0]);return 0u;
        case 0x004999f0u:{
            // 4999F0 (445719, frontend stage 0xC): the "Loading" icon 480000 that mode 28
            // created through 4999D0 is released (428900 token check, 4285A0), [67F614] = -1.
            ++state.stage12_global_init_calls;
            if(state.runtime)native_loading_animation_close_4999f0(*state.runtime);
            return 0u;}
        case 0x00401000u:
            if(count==3u&&args[0]==0u&&args[1]==0x1eu&&args[2]==1u)
                ++state.stage12_audio_calls;
            return native_music_call(state,pc_entry,args,count);
        case 0x0048c370u:++state.stage12_scene_init_calls;return 0u;
        case 0x004edce0u:
            if(count==2u&&args[0]==0x00659930u&&args[1]==2u)
                ++state.stage12_manager_reset_calls;
            // [659930+14] (the network layer reads bit 2: the frontend is up).
            if(count==2u&&args[0]==0x00659930u&&state.runtime)native_race_end(*state.runtime).frontend_manager_659944=args[1];
            return 0u;
        case 0x00416420u:++state.stage12_optional_calls;
            if(state.license_owners)(void)state.license_owners->save_active();   // licence + common.dat
            return 0u;
        default:return 0u;
    }
}

std::uint32_t native_stage12_handle_virtual(void* user,std::uint32_t handle,std::uint32_t slot);
driving::PcNativeHandleResolver native_frontend_handles(const NativeEventFunction36State& state);
std::uint32_t native_state2_runtime_u32(void* user,std::uint32_t pc_entry,std::uint32_t handle);
std::uint32_t native_state2_ui_resource_call(void* user,std::uint32_t pc_entry,const std::uint32_t* args,std::size_t count);
void native_state2_ui_finalize(void* user,std::uint32_t pc_entry,driving::Bytes resource);
driving::PcUiNotifyServices native_state2_ui_services(NativeEventFunction36State& state);
std::uint32_t native_stage12_this_call(void* user,std::uint32_t pc_entry,
                                       driving::Bytes object,std::size_t object_offset,
                                       const std::uint32_t* args,std::size_t count){
    if(!user)return 0u;
    auto& state=*static_cast<NativeEventFunction36State*>(user);
    if(pc_entry==0x004470f0u&&object_offset==0x051cu&&count==0u){
        ++state.stage12_embedded_init_calls;
        return native_frontend_commands(state,pc_entry,object,object_offset,args,count);
    }
    if(pc_entry==0x00447000u&&object_offset==0x051cu&&count==2u&&
       args[0]==0u&&args[1]==0u){
        ++state.stage12_embedded_reset_calls;
        return native_frontend_commands(state,pc_entry,object,object_offset,args,count);
    }
    if(pc_entry==0x00443eb0u&&object_offset==0u&&count==1u&&args[0]==0u){
        ++state.stage12_dispatch_calls;
        const auto& pairs=driving::pc_object_state_pair_table_r065();
        const driving::PcObjectStatePairTable pair_view{pairs.data(),pairs.size()};
        const driving::PcObjectEventCallbackTable callback_view{
            state.callback_table.data(),state.callback_table.size()};
        const driving::PcObjectEventModeInputs inputs{};
        const driving::PcObjectEventDispatch443eb0Services services{
            &state,native_stage12_embedded_void,native_stage12_embedded_u32,
            native_stage12_callback,native_stage12_dispatch_global};
        return driving::object_dispatch_callback_443eb0(
            object,0u,pair_view,callback_view,inputs,services);
    }
    if(pc_entry==0x00444e10u&&object_offset==0u&&count==0u){
        // 444E10 (stage 12 with 7B17EC set, i.e. back from a race): the
        // menu history saved by 444350 at the commit (7B15D8, 0x210 bytes)
        // is copied to +4 over a 0x53 fill of +C..+20B, every recorded key
        // (>= 0, != 0x53) of +C.. is reopened through 443EB0 and pushed, then
        // the top handle's +4, 447000(0,1)/(1,0) on +51C, 4432B0 into
        // +CE8/+CF0/+CF1 and the scene token 443FF0 into the +CF4 UI resource.
        ++state.stage12_restore_calls;
        for(std::uint32_t k=0;k<0x80u;++k)object.put32(0xcu+k*4u,0x53u);
        for(std::size_t i=0;i<state.snapshot.bytes.size();++i)object.put8(4u+i,state.snapshot.bytes[i]);
        const auto& pairs=driving::pc_object_state_pair_table_r065();
        for(std::uint32_t k=0;k<0x80u;++k){
            const auto key=object.u32(0xcu+k*4u);
            if(std::int32_t(key)<0||key==0x53u)continue;
            const auto handle=driving::object_dispatch_callback_443eb0(object,key,{pairs.data(),pairs.size()},
                {state.callback_table.data(),state.callback_table.size()},{},
                {&state,native_stage12_embedded_void,native_stage12_embedded_u32,native_stage12_callback,native_stage12_dispatch_global});
            if(handle==0u){state.frontend_ui_missing_pc=0x444e4eu;return 0u;}
            const auto n=object.u32(0x484u);
            if(n>=128u)throw std::out_of_range("444E10 callback list capacity");
            object.put32(0x284u+n*4u,handle);object.put32(0x484u,n+1u);
        }
        const auto top=object.u32(0x280u+object.u32(0x484u)*4u);
        (void)native_stage12_handle_virtual(&state,top,4u);
        {const std::uint32_t a[]{0u,1u};(void)native_frontend_commands(state,0x447000u,object,0x51cu,a,2u);}
        {const std::uint32_t a[]{1u,0u};(void)native_frontend_commands(state,0x447000u,object,0x51cu,a,2u);}
        const auto key=driving::native_handle_state_564c90(native_frontend_handles(state),top);
        const auto transition=driving::object_event_dispatch_4432b0(object,key,{&state,nullptr,nullptr,native_state2_runtime_u32});
        object.put32(0xce8u,transition);object.put8(0xcf0u,transition!=~0u);object.put8(0xcf1u,0u);
        const auto token=driving::object_state_code_443ff0(object,key,object,{&state,nullptr,nullptr,native_state2_runtime_u32});
        const auto ui=native_state2_ui_services(state);
        auto resource=object.sub(0xcf4u,0xa0u);
        driving::ui_resource_reset_465250(resource,ui);
        if(token!=~0u){
            const std::array<std::uint32_t,11> args{{token,0u,0u,0u,0u,0u,0u,0x3f800000u,0x3f800000u,0x3f800000u,0u}};
            driving::ui_resource_configure_465860(resource,args,ui);
            driving::ui_resource_commit_465970(resource,{&state,native_state2_ui_resource_call,native_state2_ui_finalize});
            state.frontend_menu_token=token;
        }
        return 0u;
    }
    return 0u;
}

std::uint32_t native_welcome_call(void* user,std::uint32_t pc,
                                  const std::uint32_t* args,std::size_t count){
    auto& state=*static_cast<NativeEventFunction36State*>(user);
    auto owner=driving::Bytes(state.object.data(),state.object.size());
    switch(pc){
    case 0x4249f0u:
        if(count!=1||!state.frontend_effect||!state.frontend_effect(state.frontend_effect_user,args[0]))
            state.frontend_ui_missing_pc=pc;
        return 0;
    case 0x440ea0u:case 0x447000u:
        return native_frontend_commands(state,pc,owner,0x51c,args,count);
    case 0x414790u:
        if(count==1u){state.frontend_movie_request=static_cast<std::int32_t>(args[0]);++state.frontend_movie_generation;}
        return 0u;
    case 0x428320u:
        if(count!=3u)return 0u;
        state.frontend_welcome_scene_token=args[0];
        state.frontend_welcome_resource=state.frontend_sprites.create(args[0],args[1],args[2]);
        return state.frontend_welcome_resource;
    case 0x4285a0u:
        if(count==1u)state.frontend_sprites.release(args[0]);
        if(count==1u&&args[0]==state.frontend_welcome_resource){
            state.frontend_welcome_scene_token=0u;state.frontend_welcome_resource=0u;
        }
        return 0u;
    case 0x442f20u:
        driving::object_store_depth_pair_442f20(owner,std::uint8_t(args[0]),std::uint8_t(args[1]));return 0u;
    case 0x4413f0u:{
        FrontendUiResources ui{state.frontend_sprites};
        driving::ui_set_active_4413f0(owner,std::uint8_t(args[0]),ui.notify());return 0u;
    }
    case 0x4536c0u:return state.frontend_gate_input_action==0u&&args[0]==1u?1u:0u;
    case 0x4536f0u:return state.frontend_gate_input_action==1u&&args[0]==2u?2u:0u;
    case 0x401000u:case 0x401030u:case 0x401050u:return native_music_call(state,pc,args,count);
    case 0x4040f0u:if(state.runtime)(void)native_pc_input_profile_4040f0(*state.runtime);return 0u;   // saved device assignments
    case 0x416380u:
        if((state.retail_assets||!state.frontend_save_directory.empty())&&!state.frontend_profiles.queried){
            frontend_profiles_reload_416380(state.frontend_profiles,
                state.frontend_save_directory.empty()?retail_asset_path(*state.retail_assets,"SaveGame"):
                    state.frontend_save_directory);
            state.frontend_profile_count=state.frontend_profiles.loaded_count;
        }
        return state.frontend_profile_count==0u?21u:1u;
    default:return 0u; // audio/text effects have no scalar control dependency
    }
}

// The licence's LAN rating 7C24F0 (licence +110: f32 rating, +114 races, +118 rank).
namespace {
alignas(16) std::array<float,24> RatingRanks5ced30{}; OR2_EXE_COPY(RatingRanks5ced30,0x5ced30u,0x60u);
// 4EEE60(this = rating, mult, races): races += n; rating += mult * (32 below 2100, 24
// below 2400, else 16), at least 0; rank = the first of the 24 limits above it (-1 none).
void rating_4eee60(driving::Bytes r,float mult,std::int32_t races){
    r.put32(4,r.u32(4)+std::uint32_t(races));
    const float v=r.f32(0);
    const float scale=2100.0f>v?32.0f:(2400.0f>v?24.0f:16.0f);         // 5CEDF4 5B0424 / 5CEDF0 5B4440 / 5B018C
    float x=scale*mult+v;if(0.0f>x)x=0.0f;r.putf(0,x);
    std::uint32_t rank=0xffffffffu;
    for(std::uint32_t i=0;i<24u;++i)if(RatingRanks5ced30[i]>x){rank=i;break;}
    r.put32(8,rank);
}
}
// Race services of the pause confirmations (frontend_title_widgets.hpp:
// 4D7B80 Retry, 4D6630 Quit, 4D7DA0 B close).
static bool native_title_pause_service(void* user,std::uint32_t pc,std::uint32_t a0,std::uint32_t a1,std::uint32_t& out){
    auto& state=*static_cast<NativeEventFunction36State*>(user);out=0u;
    if(pc==0x416420u){if(state.license_owners)(void)state.license_owners->save_active();return true;}   // licence + common.dat
    if(pc==0x440de0u){driving::object_set_token_440de0(driving::Bytes(state.object.data(),state.object.size()),a0);return true;}   // on 4035F0
    if(!state.runtime)return false;
    auto& c=*state.runtime;
    switch(pc){
    case 0x4505d0u:return native_race_manager_call(c,pc,nullptr,0,out,false);
    case 0x450230u:return native_race_manager_call(c,pc,&a0,1,out,false);
    case 0x43f860u:out=c.start_mode.frontend_prepare.game_flag_780260;return true;
    case 0x4857c0u:if(c.camera_reset_4857c0)c.camera_reset_4857c0();else outrun::driving::service_hole("native_title_pause_service","c.camera_reset_4857c0");return true;   // camera view 0x1A (renderer side)
    case 0x456e10u:                                                              // LAN players of 7DE418: 0 offline
        if(native_network_u32(0x7d68acu))return native_network_invoke(c,pc,0,{},&out);
        return !c.start_mode.network_manager_7df34c_present;
    case 0x4964b0u:{                                                             // al = [[83637C]+30] (0 without a record)
        const auto r=c.mission.manager.network_record_83637c;
        if(!r||!c.race_end)return true;
        const auto& block=c.race_end->state.arcade_84a208;const std::uint32_t at=r+0x30u-0x84a208u;
        if(at>=block.size())return false;
        out=block[at];return true;}
    // The LAN leave of Quit (4D7CF3..4D7D45): 444B20(ECX = owner, 0, 1, 0), 454220(1), [7D68D8]
    // = 0x14, the session's +4 method (0) when [session+5], then 450230(7); a1 = 1 here.
    case 0x4d7d05u:{
        const std::uint32_t args444b20[3]{0,1,0};std::uint32_t eax{};
        if(!native_frontend_owner_call(c,0x444b20u,NetworkOwnerBase,args444b20,3,eax))return false;
        if(!native_network_invoke(c,0x454220u,0,{1u}))return false;
        native_network_put32(0x7d68d8u,0x14u);
        if(const std::uint32_t s=native_network_u32(0x7d68acu);s&&native_network_u8(s+5u)){
            const std::uint32_t method=native_network_u32(native_network_u32(s)+4u);
            if(!native_network_invoke(c,method,s,{0u}))return false;
        }
        const std::uint32_t seven=7u;
        return native_race_manager_call(c,0x450230u,&seven,1,out,false);}
    // Quit step 2 in a LAN race: busy while [830C00] or 441260 (owner) answers nonzero.
    case 0x4d7c9au:{
        if(native_network_u8(0x830c00u)){out=1;return true;}
        std::uint32_t eax{};
        if(!native_frontend_owner_call(c,0x441260u,NetworkOwnerBase,nullptr,0,eax))return false;
        out=eax&0xffu;
        if(std::getenv("OR2_LEAVE_DEBUG")){static unsigned k=0;if((k++%60u)==0u)std::fprintf(stderr,"[leave] 830c00=%u 441260=%u\n",native_network_u8(0x830c00u),eax);}
        return true;}
    case 0x4eef20u:{                                                             // 4EEF20(ECX = 7C24F0, n): rating -= n * 32 when the owner is in state 1
        auto& lic=state.frontend_profiles.active;if(lic.size()<0x11cu)return false;
        const driving::Bytes owner(state.object.data(),state.object.size());
        if(owner.u32(8)!=1u)return true;
        driving::Bytes r(lic.data()+0x110u,0xcu);
        float x=r.f32(0)-float(std::int32_t(a0))*32.0f;if(0.0f>x)x=0.0f;r.putf(0,x);
        rating_4eee60(r,0.0f,0);return true;}
    case 0x440a10u:driving::event_suspend_440a10(c.event_state,a0,a1);return true;
    case 0x4edce0u:native_race_end(c).frontend_manager_659944=a0;return true;    // [659930+14]
    default:return false;
    }
}

std::uint32_t native_stage12_handle_virtual(void* user,std::uint32_t handle,
                                             std::uint32_t slot){
    if(!user)return 0u;
    auto& state=*static_cast<NativeEventFunction36State*>(user);
    if(handle==0x7300000a&&state.runtime)return native_car_select_virtual(*state.runtime,slot);
    if(handle==0x7300000b&&state.runtime)return native_transmission_virtual(*state.runtime,slot);
    if(handle==0x7300000c&&state.runtime)return native_music_virtual(*state.runtime,slot);
    if(handle==0x7300000d)return native_rankings_virtual(state,slot);
    if(handle==0x7300000e)return native_ghosts_virtual(state,slot);
    if(handle==0x7300000f)return native_courses_virtual(state,slot);
    if(handle==0x73000010&&state.runtime)return native_showroom_virtual(*state.runtime,slot);
    if(handle==0x73000014)return native_rankings_list_virtual(state,slot);
    if(handle==0x73000012)return native_rankings_menu_virtual(state,false,slot);
    if(handle==0x73000013)return native_rankings_menu_virtual(state,true,slot);
    if(handle==0x73000011){
        auto child=driving::Bytes(state.ranking_selector.data(),state.ranking_selector.size());
        if(state.ranking_selector_fault&&slot!=0&&slot!=16)return 0;
        if(slot==12)return 1;                       // 4D8FC0: the +158 text is always empty
        FrontendUiResources ui{state.frontend_sprites};ui.motion_step=state.frontend_ui_motion_step;
        ui.effect_user=state.frontend_effect_user;ui.effect_4249f0=state.frontend_effect;
        FixedChoiceServices svc{ui,driving::Bytes(state.object.data(),state.object.size()),
            state.frontend_ui_globals,state.frontend_input,state.title_base_global_6591e4,
            state.frontend_choice_timer,&state,[](void* p,unsigned pc){
                auto& owner=*static_cast<NativeEventFunction36State*>(p);
                return pc==0x4940d0&&owner.frontend_records.reset();
            }};
        RankingSelectorGlobals g{state.ranking_globals.ghost_84b0f6,state.ranking_selector_84b20c,state.ranking_notice_63aa6c,
            state.ranking_globals.online_7d68bc!=0};
        unsigned result{};bool ok=true;
        if(slot==4){ok=ranking_selector_init_4d8ee0(child,svc,g);result=ok;}
        else if(slot==8)ok=ranking_selector_control_4d90a0(child,svc,g,result);
        else if(slot==16||slot==0)ok=ranking_selector_suspend_4d8fd0(child,svc);
        if(!ok){state.ranking_selector_fault=state.frontend_ui_missing_pc=svc.missing?svc.missing:0x4d90a0;return 0;}
        return result;
    }
    if(native_network_screen_handle(handle)){
        const auto key=handle-0x73000100u;
        if(std::getenv("OR2_NET_SCREENS")){static std::uint32_t seen[0x60]{};
            if(!(seen[key]&(1u<<(slot&31u)))){seen[key]|=1u<<(slot&31u);std::fprintf(stderr,"[screen] key %u slot %u\n",key,slot);}}
        const auto object=state.network_screens[key];
        std::uint32_t result{};
        if(!object||!state.runtime||!native_network_screen_slot(*state.runtime,object,slot,result)){
            state.frontend_ui_missing_pc=0x73000100u+key;return 0;}
        if(slot==0)state.network_screens[key]=0;
        if(std::getenv("OR2_NET_SCREENS")&&slot==8&&result)std::fprintf(stderr,"[screen] key %u control -> %u (+4 = %u)\n",key,result,
            native_network_u32(object+4));
        return result;
    }
    if(handle==0x73000015||handle==0x73000016||handle==0x73000017){
        auto& menu=handle==0x73000015?state.network_menu6:handle==0x73000017?state.online_menu7:state.lan_menu8;
        const unsigned key=handle==0x73000015?6:handle==0x73000017?7:8;
        if(menu.fault&&slot!=0&&slot!=16)return 0;
        FrontendUiResources ui{state.frontend_sprites};ui.motion_step=state.frontend_ui_motion_step;
        ui.effect_user=state.frontend_effect_user;ui.effect_4249f0=state.frontend_effect;
        NetworkMenuServices svc{ui,driving::Bytes(state.object.data(),state.object.size()),state.frontend_input,
            state.title_base_global_6591e4,state.frontend_choice_timer,state.ranking_notice_63aa6c,
            state.ranking_globals.online_7d68bc!=0};
        svc.user=&state;
        svc.ready_830c30=[](void*){return native_network_u8(0x830c30u)!=0;};
        svc.player_name=[](void* p){
            auto& st=*static_cast<NativeEventFunction36State*>(p);
            const auto& lic=st.frontend_profiles.active;std::string name;
            for(std::size_t i=0;i<0x20&&i<lic.size()&&lic[i];++i)name+=char(lic[i]);
            native_network_put_string(0x830c10u,name);};
        svc.session=[](void* p,std::uint32_t pc,std::uint32_t a0,std::uint32_t a1){
            auto& st=*static_cast<NativeEventFunction36State*>(p);
            return st.runtime&&native_network_invoke(*st.runtime,pc,0,{a0,a1});};
        svc.put32=[](void*,std::uint32_t a,std::uint32_t v){native_network_put32(a,v);};
        svc.put8=[](void*,std::uint32_t a,std::uint8_t v){native_network_put8(a,v);};
        svc.copy_name=[](void*){for(std::uint32_t i=0;i<0x10u;++i){const auto ch=native_network_u8(0x830c20u+i);native_network_put8(0x830c10u+i,ch);if(!ch)break;}};
        bool ok=false;unsigned result{};
        if(menu.constructed)ok=network_menu_slot(menu.object.data(),menu.object.size(),key,slot,svc,result);
        if(slot==0)menu.constructed=false;
        if(!ok){menu.fault=svc.missing?svc.missing:(key==6?0x4c5bf0:key==7?0x4c5df0:0x4c61a0);state.frontend_ui_missing_pc=menu.fault;return 0;}
        return result;
    }
    if(handle==0x73000018){
        auto& child=state.frontend_requests;
        if(child.fault&&slot!=0&&slot!=16)return 0;
        FrontendUiResources ui{state.frontend_sprites};ui.motion_step=state.frontend_ui_motion_step;
        ui.effect_user=state.frontend_effect_user;ui.effect_4249f0=state.frontend_effect;
        // The course scripts 84B400 + type * 0x10 (Scripts/bin/Req_*.bin, loader lanes 4..).
        std::vector<std::uint8_t> copy;
        FrontendRequestServices svc{{ui,driving::Bytes(state.object.data(),state.object.size()),
            state.frontend_profiles.active,state.frontend_category_progress,state.frontend_input,
            state.title_base_global_6591e4,state.frontend_fonts,state.frontend_text},
            state.frontend_categories,
            [&state,&copy](unsigned type,driving::PcRelocCategoryBlobR077& blob){
                if(4u+type>=LoaderAssetFrontendScriptCount)return false;
                const auto* asset=runtime_asset(state,LoaderAssetFrontendScriptBaseId+4u+type,0u);
                if(!asset)return false;
                copy=asset->bytes;
                return driving::course_reloc_blob_open_r077(copy.data(),copy.size(),blob);}};
        bool ok=false;unsigned result{};
        if(slot==4){ok=frontend_requests_init_4ea330(child,svc);result=ok;}
        else if(slot==8)ok=frontend_requests_control_4ea3c0(child,svc,result);
        else if(slot==12)ok=frontend_requests_display_4ea610(child,svc);
        else if(slot==0||slot==16){ok=frontend_requests_suspend_4eaeb0(child,svc);if(slot==0)child.constructed=false;}
        if(!ok){child.fault=svc.missing?svc.missing:0x4ea3c0;state.frontend_ui_missing_pc=child.fault;return 0;}
        return result;
    }
    if(handle==0x73000009){
        auto& child=state.frontend_missions;
        if(child.fault&&slot!=0&&slot!=16)return 0;
        FrontendUiResources ui{state.frontend_sprites};ui.motion_step=state.frontend_ui_motion_step;
        ui.effect_user=state.frontend_effect_user;ui.effect_4249f0=state.frontend_effect;
        FrontendMissionServices svc{{ui,driving::Bytes(state.object.data(),state.object.size()),
            state.frontend_profiles.active,state.frontend_category_progress,state.frontend_input,
            state.title_base_global_6591e4,state.frontend_fonts,state.frontend_text},
            state.frontend_categories,state.frontend_races,state.frontend_assignment};
        bool ok=false;unsigned result{};
        if(slot==4){ok=frontend_missions_init_4e92d0(child,svc);result=ok;}
        else if(slot==8)ok=frontend_missions_control_4e9360(child,svc,result);
        else if(slot==12)ok=frontend_missions_display_4e95a0(child,svc);
        else if(slot==0||slot==16){ok=frontend_missions_suspend_4e9bb0(child,svc);if(slot==0)child.constructed=false;}
        if(!ok){child.fault=svc.missing?svc.missing:0x4e9360;state.frontend_ui_missing_pc=child.fault;return 0;}
        return result;
    }
    if(handle==0x73000008){
        auto& child=state.frontend_categories;
        if(child.fault&&slot!=0&&slot!=16)return 0;
        FrontendUiResources ui{state.frontend_sprites};ui.motion_step=state.frontend_ui_motion_step;
        ui.effect_user=state.frontend_effect_user;ui.effect_4249f0=state.frontend_effect;
        FrontendCategoryServices svc{ui,driving::Bytes(state.object.data(),state.object.size()),
            state.frontend_profiles.active,state.frontend_category_progress,state.frontend_input,
            state.title_base_global_6591e4,state.frontend_fonts,state.frontend_text};
        bool ok=false;unsigned result{};
        if(slot==4){ok=frontend_categories_init_4e8760(child,svc);result=ok;}
        else if(slot==8)ok=frontend_categories_control_4e8780(child,svc,result);
        else if(slot==12)ok=frontend_categories_display_4e8c60(child,svc);
        else if(slot==0||slot==16){ok=frontend_categories_suspend_4e8a30(child,svc);if(slot==0)child.constructed=false;}
        if(!ok){child.fault=svc.missing?svc.missing:0x4e8780;state.frontend_ui_missing_pc=child.fault;return 0;}
        return result;
    }
    if(handle==0x73000006||handle==0x73000007){
        const unsigned key=handle==0x73000006?21:24;unsigned result{};
        if(!state.license_owners||!state.license_owners->invoke(key,slot,result)){
            state.frontend_ui_missing_pc=state.license_owners?state.license_owners->missing(key):0x443eb0;
            return 0;
        }
        return result;
    }
    if(handle!=0u&&handle==state.stage12_last_handle){
        // A failed welcome sound cannot be silently retried after its input
        // already changed the child state. Teardown/reinit clears the fault.
        if(slot==0x08u&&state.frontend_ui_missing_pc==0x4249f0)return 0u;
        auto child=driving::Bytes(state.frontend_root_object.data(),state.frontend_root_object.size());
        const FrontendWelcomeServices services{&state,native_welcome_call};
        if(slot==0x04u){++state.stage12_ready_checks;state.frontend_profiles.queried=false;return frontend_welcome_init_4c5180(child,services)?1u:0u;}
        if(slot==0x10u)frontend_welcome_suspend_4c5350(child,services);
        if(slot==0x08u){++state.frontend_welcome_control_calls;return frontend_welcome_control_4c5210(child,services);}
        return 0u;
    }
    if(handle!=0u&&handle==state.state2_last_handle){
        ++state.state2_ready_checks;
        return slot==0x04u?1u:0u;
    }
    if(handle!=0u&&(handle==state.frontend_first_menu_handle||handle==state.frontend_second_menu_handle)){
        const bool first=handle==state.frontend_first_menu_handle;
        auto& storage=first?state.frontend_first_menu_object:state.frontend_second_menu_object;
        auto child=driving::Bytes(storage.data(),storage.size());const unsigned key=first?1:2;
        if(state.frontend_choice_faults[key-1]&&slot!=0&&slot!=16)return 0;
        FrontendUiResources ui{state.frontend_sprites};ui.motion_step=state.frontend_ui_motion_step;
        ui.effect_user=state.frontend_effect_user;ui.effect_4249f0=state.frontend_effect;
        FixedChoiceServices svc{ui,driving::Bytes(state.object.data(),state.object.size()),
            state.frontend_ui_globals,state.frontend_input,state.title_base_global_6591e4,
            state.frontend_choice_timer,&state,[](void* p,unsigned pc){
                auto& owner=*static_cast<NativeEventFunction36State*>(p);
                if(pc==0x4940d0)return owner.frontend_records.reset();
                return owner.frontend_choice_service&&owner.frontend_choice_service(owner.frontend_choice_service_user,pc);
            }};
        unsigned result{};bool ok=true;
        if(slot==4){ok=fixed_choice_init(child,key,svc);result=ok;}
        else if(slot==8)ok=fixed_choice_tick(storage.data(),storage.size(),key,svc,result);
        else if(slot==16||slot==0)ok=fixed_choice_suspend(child,key,svc);
        if(!ok){state.frontend_ui_missing_pc=svc.missing?svc.missing:(first?0x4c5620:0x4c5a50);
            if(svc.missing==0x4940d0&&state.frontend_records.missing_pc)
                state.frontend_ui_missing_pc=state.frontend_records.missing_pc;
            state.frontend_choice_faults[key-1]=state.frontend_ui_missing_pc;return 0;}
        return result;
    }
    if(handle!=0u&&handle==state.title_last_handle&&slot==0x04u){
        if(state.title_widgets&&!state.title_widgets->clear())return 0u;
        auto child_init=[](void* user){
            auto& owner=*static_cast<NativeEventFunction36State*>(user);
            (void)title_controller_init_48c5b0(
                owner.title_owner_object.data()+0x9bcu,
                owner.title_owner_object.size()-0x9bcu);
        };
        auto pause=[](void* user){
            auto& owner=*static_cast<NativeEventFunction36State*>(user);
            driving::set_ev_pause_flag_440930(
                *owner.title_event_state,{&owner,
                    [](void* u,std::uint32_t pc,std::uint32_t arg){
                        auto& state=*static_cast<NativeEventFunction36State*>(u);
                        if(pc==0x43f9d0u)*state.title_game_flag_780248=std::uint8_t(arg);
                        else if(pc==0x429810u)state.title_pause_flag_95b214=arg;
                        else if(pc==0x449040u)state.title_pause_flag_7d2614=arg;
                    }});
            ++owner.title_pause_calls;
        };
        const auto pause_service=state.title_event_state&&state.title_game_flag_780248?
            static_cast<TitleOwnerPause>(pause):nullptr;
        if(!title_owner_init_4d5d00(
               state.title_owner_object.data(),state.title_owner_object.size(),
               driving::Bytes(state.object.data(),state.object.size()).u32(0x0218u),
               state.title_variant_780258,
               state.title_owner_globals,{&state,child_init,pause_service}))return 0u;
        ++state.title_init_calls;
        return 1u;
    }
    if(handle!=0u&&handle==state.title_last_handle&&slot==0x08u){
        ++state.title_control_calls;
        if(state.title_widgets){
            state.title_menu_globals.root_state=driving::Bytes(state.object.data(),state.object.size()).u32(0x218);
            state.title_menu_globals.variant=state.title_variant_780258;
            state.title_menu_globals.feature_mask=state.frontend_input.feature_mask;
            if(state.title_variant_780258==4u){   // LAN pause menu: the session [7D68AC] and its [+5C] child +8 (network module)
                const std::uint32_t session=native_network_u32(0x7d68acu);
                const std::uint32_t child=session?native_network_u32(session+0x5cu):0u;
                state.title_menu_globals.manager_present=session!=0u;
                state.title_menu_globals.manager_child_key=child?native_network_u32(child+8u):0u;
                state.title_manager_resolved=true;
            }
            state.title_widgets->frame(state.title_menu_globals,state.title_owner_globals,
                driving::Bytes(state.object.data(),state.object.size()).f32(0xda0),0,
                state.title_pause_flag_95b214,state.frontend_ui_motion_step);
            // Options screens (stages 3..11) edit the active license 7C23E0.
            state.title_widgets->options(driving::Bytes(state.frontend_profiles.active.data(),state.frontend_profiles.active.size()),
                state.runtime?native_race_end(*state.runtime).language_7d2698:0u);
            state.title_widgets->pause(&state,native_title_pause_service);
            // Options > Controls > Configuration over the PC input device layer.
            if(state.runtime&&state.runtime->input_state.pc_input_layer)state.title_widgets->config(state.runtime,native_pc_input_config_runner);
        }
        const auto owner_call=[](void* user,std::uint32_t pc,std::uint8_t* object,
                                 std::size_t offset,std::uint32_t& value)->bool{
            auto& state=*static_cast<NativeEventFunction36State*>(user);
            // 4D8C50 in the race state 3 (the pause menu closing): 55A930 = [7F9460+60], the LAN
            // session manager state (0 offline; 46C270 is only reached with a session).
            if(pc==0x55a930u){value=state.runtime?state.runtime->start_mode.manager_state_7f94c0:0u;
                if(value!=0u){state.title_last_missing_service=0x46c270u;return false;}
                return true;}
            if(state.title_widgets){
                if(state.title_menu_globals.root_state==3&&state.title_menu_globals.variant==4&&!state.title_manager_resolved){
                    state.title_last_missing_service=0x4d5e40;return false;
                }
                const bool ok=state.title_widgets->invoke(pc,offset,value);
                if(pc==0x48d420)++state.title_controller_ticks;
                if(ok&&pc==0x4d5e40){++state.title_initial_ui_calls;state.title_initial_ui=title_menu_records(state.title_menu_globals);}
                state.title_last_missing_service=ok?0:state.title_widgets->missing_pc();
                return ok;
            }
            if(pc==0x48d420u&&offset==0x9bcu){
                ++state.title_controller_ticks;
                return title_controller_tick_48d420(
                    object+offset,TitleOwnerPcSize-offset,
                    {&state,[](void* user,std::uint32_t pc,std::uint8_t* controller,
                                std::size_t,std::int32_t argument,std::uint32_t& out)->bool{
                        auto& owner=*static_cast<NativeEventFunction36State*>(user);
                        switch(pc){
                        case 0x48cc00u:
                            ++owner.title_timeline_service_calls;out=0u;
                            return title_controller_motion_48cc00(controller,0x12b0,
                                driving::Bytes(owner.object.data(),owner.object.size()).f32(0xda0));
                        case 0x48f5f0u:
                            return frontend_input_action_48f5f0(controller,0x34,owner.frontend_input,
                                argument,owner.title_base_global_6591e4,&owner,
                                [](void* p,unsigned key,int arg){
                                    auto& state=*static_cast<NativeEventFunction36State*>(p);
                                    FrontendUiResources ui{state.frontend_sprites};
                                    const bool ok=ui.input_feedback(driving::Bytes(state.object.data(),state.object.size()),key,arg);
                                    if(!ok){state.title_last_missing_service=ui.missing_pc;state.frontend_ui_missing_pc=ui.missing_pc;}
                                    return ok;
                                },out);
                        case 0x47f110u:
                            out=driving::runtime_current_player_47f110();return true;
                        default:
                            owner.title_last_missing_service=pc;return false;
                        }
                    },driving::Bytes(state.object.data(),state.object.size())
                            .u32(0x0218u)},value);
            }
            if(pc==0x4d5e40u&&offset==0u){
                // Do not advance to stage 1 with an empty list/window. The
                // legacy metadata-only adapter is no longer a production path.
                state.title_last_missing_service=pc;return false;
            }
            state.title_last_missing_service=pc;
            return false;
        };
        std::uint32_t value{};
        if(!title_owner_tick_4d8c50(
               state.title_owner_object.data(),state.title_owner_object.size(),
               {&state,owner_call,
                driving::Bytes(state.object.data(),state.object.size()).u32(0x0218u)},
               value))return 0u;
        return value;
    }
    return 0u;
}

driving::PcNativeHandleResolver native_frontend_handles(
    const NativeEventFunction36State& state){
    return {state.frontend_handle_bindings.data(),state.frontend_handle_count};
}

// 442CB0's tail: 4EE930 (the lobby settings to the race globals) in the network module.
void native_state2_selector_void(void* user,std::uint32_t pc,std::uint32_t){
    auto& state=*static_cast<NativeEventFunction36State*>(user);
    if(state.runtime)(void)native_network_invoke(*state.runtime,pc,0,{});
}
std::uint32_t native_state2_selector_virtual(void* user,std::uint32_t handle,
                                              std::uint32_t slot){
    if(!user||slot!=0x08u)return 0u;
    auto& state=*static_cast<NativeEventFunction36State*>(user);
    ++state.state2_selector_calls;
    state.frontend_choice_scene_token=0u;
    state.state2_selector_child_state=driving::native_handle_state_564c90(
        native_frontend_handles(state),handle);
    // 0x564C90 is only the state key. Slot +8 is the independent virtual
    // control call; key 22 dispatches 0x4DF120 in the pinned PC vtable.
    std::uint32_t result=0u;
    if(state.state2_selector_child_state==0u&&handle==state.stage12_last_handle){
        result=native_stage12_handle_virtual(&state,handle,slot);
    }else if(state.state2_selector_child_state==22u&&handle==state.state2_last_handle){
        auto child=driving::Bytes(state.frontend_gate_object.data(),
                                   state.frontend_gate_object.size());
        const auto owner=driving::Bytes(state.object.data(),state.object.size());
        const auto input_action=state.frontend_gate_input_action;
        result=driving::frontend_gate_control_4df120(
            child,input_action,child.u8(0x17a4u)==0u,
            owner.i32(0x0518u)>0,0u,&state.frontend_gate_list);
        ++state.frontend_gate_control_calls;
        if(input_action==2u||input_action==4u){
            const auto cursor=child.u32(0x38u);
            if(state.frontend_menu_index!=cursor)++state.frontend_menu_navigation;
            state.frontend_menu_index=cursor;
        }
        if(child.u8(0x17b0u)==0u&&input_action<=1u&&
           child.u32(0x17b4u)!=0u)
            state.frontend_gate_animation_pending=1u;
    }else if(state.state2_selector_child_state==1u&&
             handle==state.frontend_first_menu_handle){
        auto child=driving::Bytes(state.frontend_first_menu_object.data(),
                                   state.frontend_first_menu_object.size());
        const auto before=child.u32(0xe8u);
        result=native_stage12_handle_virtual(&state,handle,slot);
        if(before!=child.u32(0xe8u))++state.frontend_menu_navigation;
        state.frontend_menu_index=child.u32(0xe8u);
        // The original 51BE30 resource now owns its live sprite/range.
        // No separate static preview overlay is submitted.
    }else if(state.state2_selector_child_state==2u&&
             handle==state.frontend_second_menu_handle){
        auto child=driving::Bytes(state.frontend_second_menu_object.data(),
                                   state.frontend_second_menu_object.size());
        const auto before=child.u32(0xe8u);
        result=native_stage12_handle_virtual(&state,handle,slot);
        if(before!=child.u32(0xe8u))++state.frontend_menu_navigation;
        state.frontend_menu_index=child.u32(0xe8u);
    }else if(native_network_screen_handle(handle)&&state.state2_selector_child_state==handle-0x73000100u){
        result=native_stage12_handle_virtual(&state,handle,slot);
    }else if((state.state2_selector_child_state==6u&&handle==0x73000015u)||(state.state2_selector_child_state==8u&&handle==0x73000016u)||
             (state.state2_selector_child_state==7u&&handle==0x73000017u)){
        result=native_stage12_handle_virtual(&state,handle,slot);
    }else if(state.state2_selector_child_state==0x3a&&handle==0x73000008){
        result=native_stage12_handle_virtual(&state,handle,slot);
    }else if(state.state2_selector_child_state==0x3b&&handle==0x73000009){
        result=native_stage12_handle_virtual(&state,handle,slot);
    }else if(state.state2_selector_child_state==0x3c&&handle==0x73000018){
        result=native_stage12_handle_virtual(&state,handle,slot);
    }else if(state.state2_selector_child_state==36u&&handle==0x7300000du){
        result=native_stage12_handle_virtual(&state,handle,slot);
    }else if(state.state2_selector_child_state==37u&&handle==0x7300000eu){
        result=native_stage12_handle_virtual(&state,handle,slot);
    }else if(state.state2_selector_child_state==38u&&handle==0x7300000fu){
        result=native_stage12_handle_virtual(&state,handle,slot);
    }else if((state.state2_selector_child_state==50u||state.state2_selector_child_state==53u)&&handle==0x73000014u){
        result=native_stage12_handle_virtual(&state,handle,slot);
    }else if(state.state2_selector_child_state==51u&&handle==0x73000012u){
        result=native_stage12_handle_virtual(&state,handle,slot);
    }else if((state.state2_selector_child_state==48u||state.state2_selector_child_state==57u)&&handle==0x73000013u){
        result=native_stage12_handle_virtual(&state,handle,slot);
    }else if(state.state2_selector_child_state==49u&&handle==0x73000011u){
        result=native_stage12_handle_virtual(&state,handle,slot);
    }else if(state.state2_selector_child_state==43u&&handle==0x73000010u){
        result=native_stage12_handle_virtual(&state,handle,slot);
    }else if(state.state2_selector_child_state==10u&&handle==0x7300000au){
        result=native_stage12_handle_virtual(&state,handle,slot);
    }else if(state.state2_selector_child_state==16u&&handle==0x7300000bu){
        result=native_stage12_handle_virtual(&state,handle,slot);
    }else if(state.state2_selector_child_state==4u&&handle==0x7300000cu){
        result=native_stage12_handle_virtual(&state,handle,slot);
    }else if((state.state2_selector_child_state==21&&handle==0x73000006)||
             (state.state2_selector_child_state==24&&handle==0x73000007)){
        result=native_stage12_handle_virtual(&state,handle,slot);
    }else if(state.state2_selector_child_state==44u&&
             handle==state.title_last_handle){
        result=native_stage12_handle_virtual(&state,handle,0x08u);
    }
    state.state2_selector_last_result=result;
    state.frontend_gate_input_action=0xffffffffu;
    return result;
}

std::uint32_t native_state2_runtime_u32(void* user,std::uint32_t pc_entry,
                                        std::uint32_t handle){
    if(!user||pc_entry!=0x00564c90u)return 0u;
    auto& state=*static_cast<NativeEventFunction36State*>(user);
    return driving::native_handle_state_564c90(native_frontend_handles(state),handle);
}

void native_state2_transition_activate(void* user){
    if(user)++static_cast<NativeEventFunction36State*>(user)->state2_transition_activations;
}

void native_state2_ui_release(void* user,std::uint32_t pc_entry,
                              std::uint32_t handle){
    if(!user||pc_entry!=0x004285a0u)return;
    auto& state=*static_cast<NativeEventFunction36State*>(user);
    if(state.frontend_sprites.release(handle))
        ++state.state2_ui_release_calls;
}

std::uint32_t native_state2_ui_resource_call(void* user,std::uint32_t pc_entry,
                                              const std::uint32_t* args,
                                              std::size_t count){
    if(!user)return 0xffffffffu;
    auto& state=*static_cast<NativeEventFunction36State*>(user);
    FrontendUiResources ui{state.frontend_sprites,0u,state.title_pause_flag_95b214};
    const auto result=ui.sprite_call(pc_entry,args,count);
    if(ui.missing_pc)state.frontend_ui_missing_pc=ui.missing_pc;
    if(pc_entry==0x428320u&&count==3u){++state.state2_ui_create3_calls;state.state2_ui_resource_handle=result;}
    if(pc_entry==0x428460u&&count==5u){++state.state2_ui_create5_calls;state.state2_ui_resource_handle=result;}
    if(pc_entry==0x428800u&&count==2u)++state.state2_ui_property_calls;
    return result;
}

void native_state2_ui_finalize(void* user,std::uint32_t pc_entry,
                               driving::Bytes resource){
    if(!user||pc_entry!=0x004656b0u)return;
    auto& state=*static_cast<NativeEventFunction36State*>(user);
    ++state.state2_ui_finalize_calls;
    FrontendUiResources ui{state.frontend_sprites};
    if(!ui.finalize(resource))state.frontend_ui_missing_pc=ui.missing_pc;
}

void native_state2_ui_invalid(void* user,std::uint32_t pc_entry,
                              driving::Bytes,std::uint32_t){
    if(!user||pc_entry!=0x01039cc4u)return;
    ++static_cast<NativeEventFunction36State*>(user)->state2_ui_invalid_calls;
}

driving::PcUiNotifyServices native_state2_ui_services(
    NativeEventFunction36State& state){
    driving::PcUiNotifyServices services{};
    services.user=&state;
    services.handle_void=native_state2_ui_release;
    services.handle_i32=[](void* p,std::uint32_t pc,std::uint32_t handle){
        auto& st=*static_cast<NativeEventFunction36State*>(p);
        return pc==0x428880u?static_cast<std::int32_t>(st.frontend_sprites.status(handle)):0;
    };
    // 441370 (the 4400D3 frame at +C48 ready): 42CA60(9), 465EB0(474 / 476), 42CDD0(text, arg)
    // into the frontend text layer (network_widgets) at the 956BB8 cursor, 956BD0 mode and
    // 956BCC colour that 441370 set. The text index stands for the 465EB0 string pointer.
    services.value_void=[](void* p,std::uint32_t pc,std::uint32_t v){
        auto& st=*static_cast<NativeEventFunction36State*>(p);
        if(pc!=0x42ca60u||!st.frontend_fonts||v>=st.frontend_fonts->fonts.size()){
            outrun::driving::service_hole("native_state2_ui_services","value_void (42CA60 font)");return;}
        auto& w=st.network_widgets;w.font=&st.frontend_fonts->fonts[v];
        w.style.scale_x=1.0f;w.style.scale_y=1.0f;w.style.color=0xffffffffu;w.style.flags=1u;
    };
    services.lookup=[](void*,std::uint32_t pc,std::uint32_t id)->std::uint32_t{
        if(pc!=0x465eb0u)outrun::driving::service_hole("native_state2_ui_services","lookup");
        return id;
    };
    services.draw=[](void* p,std::uint32_t pc,std::uint32_t text,std::uint32_t arg){
        auto& st=*static_cast<NativeEventFunction36State*>(p);
        auto& w=st.network_widgets;const auto& g=st.frontend_ui_globals;
        const std::string* fmt=st.frontend_text?st.frontend_text->get(text):nullptr;
        if(pc!=0x42cdd0u||!w.font||!fmt){outrun::driving::service_hole("native_state2_ui_services","draw (42CDD0 text)");return;}
        std::string out=*fmt;   // printf: %s takes the 830C20 name (7D68BC set), %d the argument
        if(const auto at=out.find("%s");at!=std::string::npos)out.replace(at,2,g.alternate?native_network_string(arg):std::string());
        else if(const auto d=out.find("%d");d!=std::string::npos)out.replace(d,2,std::to_string(std::int32_t(arg)));
        w.style.mode=g.mode;w.style.color=g.color;
        w.cursor.x=g.x;w.cursor.y=g.y;w.cursor.origin_x=g.x;
        if(!frontend_text_draw(*w.font,w.style,w.cursor,out,w.glyphs_))
            outrun::driving::service_hole("native_state2_ui_services","draw (text outside the font)");
        w.style.flags=1u;
    };
    return services;
}

void native_state2_ui_open(void* user,std::uint32_t pc_entry,
                           driving::Bytes object,std::uint32_t key){
    if(!user||pc_entry!=0x004447d0u)return;
    auto& state=*static_cast<NativeEventFunction36State*>(user);
    ++state.state2_ui_open_calls;
    state.state2_ui_open_last_key=key;
    const auto ui_services=native_state2_ui_services(state);
    driving::PcRuntimeUiOpenServices4447d0 services{};
    services.ui_services=ui_services;
    services.commit_services={&state,native_state2_ui_resource_call,
                              native_state2_ui_finalize};
    services.invalid_user=&state;
    services.invalid_tail=native_state2_ui_invalid;
    if(driving::object_runtime_ui_open_known_4447d0(
           object,key,object,{&state,nullptr,nullptr,native_state2_runtime_u32},services)){
        ++state.state2_ui_valid_calls;
        ++state.state2_ui_configure_calls;
        state.state2_ui_last_token=object.u32(0x0cf4u);
        state.frontend_menu_token=state.state2_ui_last_token;
    }
}

// SUMO_FE commit 444350 services (49E4A0 state 2, owner result 5).
// 4EEC80 decodes the packed root +0x20C selection into the START globals;
// 495930/4958A0 read Races.bin through the RACE_MAPPING table (836370).
void native_commit_prepare(void* user,std::uint32_t pc_entry,driving::Bytes object,std::size_t offset){
    auto& st=*static_cast<NativeEventFunction36State*>(user);
    if(!st.runtime||pc_entry!=0x4eec80u||offset>object.size())return;
    auto& c=*st.runtime;auto& start=c.start_mode;
    auto ps=start.frontend_prepare;
    ps.primary_mode_78024c=start.course_preset;ps.route_state_780258=c.game_mode.game_variant;
    ps.selection_active_836374=start.selection_active_836374?1u:0u;
    driving::PcRuntimePrepareServices services{};services.user=&c;
    services.count_entries=[](void* u,std::uint32_t category)->std::uint32_t{      // 495930
        auto& s=static_cast<NativeRuntimeContext*>(u)->start_mode;
        if(!s.scene_owner_race_assets||!s.scene_owner_assignment_assets)return 0u;
        const auto& races=*s.scene_owner_race_assets;const auto& map=*s.scene_owner_assignment_assets;
        if(category>=map.menu_count||category>=map.menu_race_keys.size())return 0u;
        return race_count_with_key_495930(races,map.menu_race_keys[category]);
    };
    services.select_entry=[](void* u,std::uint32_t category,std::uint32_t index){  // 4958A0
        auto& s=static_cast<NativeRuntimeContext*>(u)->start_mode;
        if(!s.scene_owner_assignment_assets||category>=s.scene_owner_assignment_assets->menu_count||
           category>=s.scene_owner_assignment_assets->menu_race_keys.size()){s.scene_owner_race_key_known=false;return;}
        s.scene_owner_race_key=s.scene_owner_assignment_assets->menu_race_keys[category];
        s.scene_owner_race_sub_key=index;s.scene_owner_race_key_known=true;
    };
    driving::runtime_prepare_4eec80(object.sub(offset,object.size()-offset),ps,services);
    if(const auto v=dev_hooks().force_variant){   // host test hook: a Time Attack course code goes to 836174 for the other variants
        if(ps.route_state_780258==7u&&*v!=7u)ps.alternate_code_836174=ps.output_code_656234;
        ps.route_state_780258=*v;}
    start.course_preset=ps.primary_mode_78024c;c.game_mode.game_variant=ps.route_state_780258;
    start.selection_active_836374=ps.selection_active_836374!=0u;
    start.frontend_prepare=ps;++start.frontend_prepare_calls;
}
void native_commit_global(void* user,std::uint32_t pc_entry,std::uint32_t a0,std::uint32_t,std::uint32_t){
    auto& st=*static_cast<NativeEventFunction36State*>(user);
    if(pc_entry==0x4957e0u&&st.runtime){st.runtime->start_mode.selection_active_836374=false;return;}  // 836374 = 0
    // 43F870(0) (mode 2, OUTRUN2SP): [780260] = 0, the PC game rather than SUMO_FE.
    if(pc_entry==0x43f870u&&st.runtime){st.runtime->start_mode.frontend_prepare.game_flag_780260=std::uint8_t(a0);return;}
    // 44DA00 (mode 3) commit is not bound here.
    st.frontend_last_missing_action=pc_entry;
}
std::uint32_t native_commit_virtual(void* user,std::uint32_t handle,std::uint32_t slot,std::uint32_t,bool has){
    // 4443E4: +0x10 suspend, then +0 (1) deleting destructor of each indexed child.
    if(slot==0x10u)return native_stage12_handle_virtual(user,handle,0x10u);
    if(slot==0u&&has&&(handle==0x73000006u||handle==0x73000007u))return native_stage12_handle_virtual(user,handle,0u);
    return 0u;
}

// 441200 on a frontend handle: suspend (+10), delete (+0) for the two
// embedded overlays, then the object's byte-state pop.
void native_frontend_release_441200(void* user,std::uint32_t handle){
    auto& st=*static_cast<NativeEventFunction36State*>(user);
    native_stage12_handle_virtual(user,handle,0x10u);
    if(handle==0x73000006||handle==0x73000007)
        native_stage12_handle_virtual(user,handle,0u);
    const driving::PcObjectChildServices child{&st,[](void* u,std::uint32_t pc,std::uint32_t off){
        auto& s=*static_cast<NativeEventFunction36State*>(u);
        if(pc!=0x4464f0u||off>s.object.size()){outrun::driving::service_hole("native_frontend_release_441200","child call");return;}
        driving::runtime_child_counter_dec_4464f0(driving::Bytes(s.object.data()+off,s.object.size()-off));},nullptr};
    driving::object_pop_byte_state_440e60(driving::Bytes(st.object.data(),st.object.size()),child);
}
bool native_network_screen_key(std::uint32_t key);
// The owner screen stack (443EB0 create, slots, 441200 release, 564C90 key, 4432B0 / 4447D0 / 444880 UI).
FrontendStackServices native_frontend_stack(NativeEventFunction36State& state){
    FrontendStackServices stack{};stack.user=&state;
    stack.create=[](void* user,std::uint32_t key){
        auto& st=*static_cast<NativeEventFunction36State*>(user);
        auto root=driving::Bytes(st.object.data(),st.object.size());
        const auto& pairs=driving::pc_object_state_pair_table_r065();
        return driving::object_dispatch_callback_443eb0(root,key,
            {pairs.data(),pairs.size()},{st.callback_table.data(),st.callback_table.size()},
            {},{&st,native_stage12_embedded_void,native_stage12_embedded_u32,
                native_stage12_callback,native_stage12_dispatch_global});
    };
    stack.invoke=native_stage12_handle_virtual;
    stack.key=[](void* user,std::uint32_t handle){
        return driving::native_handle_state_564c90(native_frontend_handles(
            *static_cast<NativeEventFunction36State*>(user)),handle);
    };
    stack.release=native_frontend_release_441200;
    stack.ui=[](void* user,std::uint32_t pc,std::uint32_t key){
        auto& st=*static_cast<NativeEventFunction36State*>(user);
        auto root=driving::Bytes(st.object.data(),st.object.size());
        if(pc==0x4432b0u)return driving::object_event_dispatch_4432b0(root,key,
            {&st,nullptr,nullptr,native_state2_runtime_u32});
        if(pc==0x4448c0u){
            const auto token=driving::object_state_code_443ff0(root,key,root,{&st,nullptr,nullptr,native_state2_runtime_u32});
            const auto ui=native_state2_ui_services(st);
            auto resource=root.sub(0xcf4u,0xa0u);
            driving::ui_resource_reset_465250(resource,ui);
            if(token!=~0u){
                const std::array<std::uint32_t,11> args{{token,0u,0u,0u,0u,0u,0u,
                    0x3f800000u,0x3f800000u,0x3f800000u,0u}};
                driving::ui_resource_configure_465860(resource,args,ui);
                driving::ui_resource_commit_465970(resource,{&st,native_state2_ui_resource_call,native_state2_ui_finalize});
                st.frontend_menu_token=token;
            }
            const auto& pairs=driving::pc_object_state_pair_table_r065();
            driving::object_update_state_442fd0(root,{pairs.data(),pairs.size()},
                {&st,native_state2_runtime_u32,native_stage12_embedded_u32});
            return 0u;
        }
        if(pc==0x444880u){
            const auto transition=driving::object_event_dispatch_4432b0(root,key,
                {&st,nullptr,nullptr,native_state2_runtime_u32});
            root.put32(0xce8u,transition);root.put8(0xcf0u,transition!=~0u);root.put8(0xcf1u,0u);
        }
        native_state2_ui_open(user,0x4447d0u,root,key);return 0u;
    };
    return stack;
}
void native_network_globals_in(NativeEventFunction36State& state);
driving::PcRuntimeControlServices native_state2_runtime_services(NativeEventFunction36State& state);
// Owner (7B17E8) methods the network layer calls (pc_network.cpp).
bool frontend_owner_call_impl(NativeRuntimeContext& c,std::uint32_t pc,std::uint32_t ecx,const std::uint32_t* a,std::size_t n,std::uint32_t& eax){
    auto& st=c.event_function36;
    auto owner=driving::Bytes(st.object.data(),st.object.size());
    const auto stack=native_frontend_stack(st);
    eax=0;
    // 465160 resources (0xA0 bytes) and screen bases of the network module's screens.
    if(pc>=0x465160u&&pc<=0x4659f0u){
        auto* r=native_network_view(ecx,0xa0);
        if(!r)return false;
        if(pc==0x465160u)return title_ui_resource_construct_465160(r,0xa0);
        if(pc==0x465770u){                                  // 428940 on the resource's sprite +8: its +2C id
            const auto sprite=driving::Bytes(r,0xa0).i32(8);
            if(n>=1&&sprite>=0)if(auto* s=st.frontend_sprites.get_mutable(std::uint32_t(sprite)))s->id_2c=std::int32_t(a[0]);
            return true;}
        FrontendUiResources ui{st.frontend_sprites};ui.motion_step=st.frontend_ui_motion_step;ui.effect_user=st.frontend_effect_user;ui.effect_4249f0=st.frontend_effect;
        unsigned value{};
        const bool ok=ui.call(pc,driving::Bytes(r,0xa0),a,n,value)&&!ui.missing_pc;
        if(!ok)st.frontend_ui_missing_pc=ui.missing_pc?ui.missing_pc:pc;
        eax=value;return ok;
    }
    // The owner's embedded UI objects (440EA0 -> +51C, the 446xxx methods on +51C)
    // through the native command set.
    if(ecx>=NetworkOwnerBase&&ecx<NetworkOwnerBase+owner.size()&&(pc==0x440ea0u||pc==0x442ac0u||(pc>=0x446a80u&&pc<=0x4470f0u))){
        const std::size_t offset=pc==0x440ea0u?0x51cu:ecx-NetworkOwnerBase;
        const auto before=st.frontend_ui_missing_pc;st.frontend_ui_missing_pc=0;
        eax=native_frontend_commands(st,pc,owner,offset,a,n);
        const bool ok=st.frontend_ui_missing_pc==0;
        if(ok)st.frontend_ui_missing_pc=before;
        return ok;
    }
    if(pc==0x48f480u||pc==0x48f4d0u||pc==0x48f5f0u){
        auto* o=native_network_view(ecx,0x34);
        if(!o)return false;
        if(pc==0x48f480u)return title_base_construct_48f480(o,0x34,st.title_base_global_6591e4);
        if(pc==0x48f4d0u){driving::Bytes(o,0x34).put32(0,0x5c18c8u);return true;}
        FrontendUiResources ui{st.frontend_sprites};ui.motion_step=st.frontend_ui_motion_step;ui.effect_user=st.frontend_effect_user;ui.effect_4249f0=st.frontend_effect;
        struct Feedback{FrontendUiResources& ui;driving::Bytes root;} fb{ui,owner};
        std::uint32_t action{};
        const bool ok=frontend_input_action_48f5f0(o,0x34,st.frontend_input,std::int32_t(n?a[0]:0),st.title_base_global_6591e4,&fb,
            [](void* p,unsigned key,int arg){auto& f=*static_cast<Feedback*>(p);return f.ui.input_feedback(f.root,key,arg);},action);
        if(std::getenv("OR2_NET_SCREENS")&&action!=12u)std::fprintf(stderr,"[screen] input %08X action %u ok %d\n",ecx,action,int(ok));
        eax=action;return ok;
    }
    // 444780(key): the overlay +488 replaced by `key` (+48C when its init succeeds).
    auto overlay=[&](std::uint32_t key){
        frontend_close_overlays_4430b0(owner,stack);
        const auto created=stack.create(&st,key);
        owner.put32(0x488,created);
        if(created&&std::uint8_t(stack.invoke(&st,created,4)))owner.put8(0x48c,1);
        else{if(created)stack.release(&st,created);owner.put32(0x488,0);}
    };
    switch(pc){
    case 0x43fa00u:eax=enhancements::display_frames()?c.frame_state.updates_95af48:1u;return true;   // [780278] = [95AF48]: ticks of this frame (417B20; 0 on a display-only frame)
    case 0x4412c0u:                                         // the +488 overlay's key, 0x53 when none
        eax=owner.u32(0x488)&&owner.u8(0x48c)?stack.key(&st,owner.u32(0x488)):0x53u;return true;
    case 0x444780u:if(n<1)return false;overlay(a[0]);return true;
    case 0x4459f0u:                                         // the key 46 (2E) notice: pushed, or as the overlay
        if(n<1)return false;
        if(a[0]&0xffu){eax=frontend_push_444fe0(owner,0x2e,stack);return true;}
        overlay(0x2e);return true;
    case 0x440e00u:if(n<1)return false;driving::object_set_field8_440e00(owner,a[0]);return true;
    case 0x441130u:driving::object_transition_latch_441130(owner);return true;
    case 0x440f70u:if(n<3)return false;
        driving::object_transition_config_440f70(owner,std::uint8_t(a[0]),a[1],a[2],st.loader_stage12_state.transition_globals);return true;
    case 0x442ec0u:case 0x443040u:{
        driving::PcObjectStateServices services{};services.user=&st;
        services.call_u32=[](void* p,unsigned,unsigned token){auto& s=*static_cast<NativeEventFunction36State*>(p);
            return driving::native_handle_state_564c90({s.frontend_handle_bindings.data(),s.frontend_handle_count},token);};
        eax=pc==0x442ec0u?driving::object_query_previous_state_442ec0(owner,services):driving::object_query_last_state_443040(owner,services);
        return true;}
    case 0x4412f0u:eax=owner.u8(0x48c);return true;          // an overlay is open
    case 0x440e10u:{                                        // history depth +220 pushed (copy of the top pair), 446FC0 on +51C
        const auto d=std::int8_t(owner.u8(0x220));
        if(d>=0x20)return true;
        const auto e=std::size_t(d+1);owner.put8(0x220,std::uint8_t(e));
        owner.put8(0x221+e,owner.u8(0x220+e));owner.put8(0x241+e,owner.u8(0x240+e));
        const auto before=st.frontend_ui_missing_pc;st.frontend_ui_missing_pc=0;
        (void)native_frontend_commands(st,0x446fc0u,owner,0x51c,nullptr,0);
        const bool ok=st.frontend_ui_missing_pc==0;if(ok)st.frontend_ui_missing_pc=before;return ok;}
    case 0x444b20u:{
        // 444B20(a0, a1, a2) (Quit from a LAN race, 4D7D12: 0, 1, 0): every child of the stack
        // released (history popped per child), the overlay too when a2, then the stack rebuilt
        // as keys 0 / 1 / 6 (+ 8, or 7 with [+8], unless a0), +20C bit 2, +4 = 1; a1: 442E70 and
        // the state table +C..+20C saved at 7B15E0 (the 444350 snapshot +8); else the top's
        // init, 4432B0 and 4447D0; then the history pushed (446FC0).
        if(n<3)return false;
        const bool a0=(a[0]&0xffu)!=0u,a1=(a[1]&0xffu)!=0u,a2=(a[2]&0xffu)!=0u;
        auto depth=owner.u32(0x484);
        if(depth>0x80u)return false;
        if(depth==0u&&a1){
            // In a race the frontend stack holds no screen (the frontend comes back through the
            // 444350 snapshot, 444E10): the rebuilt stack only reaches the snapshot's state table
            // (442E70 of keys 0 / 1 / 6 / 8|7: their 564C90 states), +4 = 1 and +20C bit 2.
            if(a2&&owner.u8(0x48c)){
                if(const auto h=owner.u32(0x488)){stack.release(&st,h);owner.put32(0x488,0);}
                owner.put8(0x48c,0);driving::object_transition_latch_441130(owner);
            }
            for(std::size_t k=0;k<0x80u;++k)owner.put32(0xc+k*4u,0x53u);
            std::uint32_t keys[4]{0,1,6,owner.u32(8)==0u?8u:7u};const std::uint32_t count=a0?3u:4u;
            for(std::uint32_t i=0;i<count;++i)owner.put32(0xc+i*4u,keys[i]);
            owner.put32(0x20c,(owner.u32(0x20c)&0xffffffe7u)|4u);owner.put32(4,1);
            for(std::size_t i=0;i<0x200u;++i)st.snapshot.bytes[8u+i]=owner.u8(0xc+i);
            return true;
        }
        for(;depth>0u;owner.put32(0x484,--depth)){
            const auto handle=owner.u32(0x280+depth*4u);
            if(!handle)continue;
            stack.release(&st,handle);
            const auto d=std::int8_t(owner.u8(0x220));
            if(d>0){owner.put8(0x221+std::size_t(d),0);owner.put8(0x241+std::size_t(d),0);owner.put8(0x220,std::uint8_t(d-1));
                driving::runtime_child_counter_dec_4464f0(owner.sub(0x51c,owner.size()-0x51c));}
        }
        if(a2&&owner.u8(0x48c)){
            if(const auto h=owner.u32(0x488)){stack.release(&st,h);owner.put32(0x488,0);}
            owner.put8(0x48c,0);driving::object_transition_latch_441130(owner);
        }
        auto push=[&](std::uint32_t key,std::uint8_t flag){
            const auto h=stack.create(&st,key);
            owner.put32(0x284+depth*4u,h);owner.put32(0x484,++depth);
            const auto d=std::size_t(std::int8_t(owner.u8(0x220)));owner.put8(0x221+d,flag);owner.put8(0x241+d,flag);};
        push(0,0);push(1,1);push(6,1);
        owner.put32(0x20c,(owner.u32(0x20c)&0xffffffe7u)|4u);owner.put32(4,1);
        if(!a0)push(owner.u32(8)==0u?8u:7u,1);
        if(a1){
            driving::PcObjectStateServices services{};services.user=&st;
            services.call_u32=[](void* p,unsigned,unsigned token){auto& s=*static_cast<NativeEventFunction36State*>(p);
                return driving::native_handle_state_564c90({s.frontend_handle_bindings.data(),s.frontend_handle_count},token);};
            driving::object_refresh_state_table_442e70(owner,services);
            for(std::size_t i=0;i<0x200u;++i)st.snapshot.bytes[8u+i]=owner.u8(0xc+i);
        }else{
            const auto top=owner.u32(0x280+depth*4u);
            (void)stack.invoke(&st,top,4u);
            const auto key=stack.key(&st,top);
            const auto transition=stack.ui(&st,0x4432b0u,key);
            owner.put32(0xce8,transition);owner.put8(0xcf0,transition!=~0u);owner.put8(0xcf1,0);
            (void)stack.ui(&st,0x4447d0u,key);
        }
        const auto d=std::int8_t(owner.u8(0x220));
        if(d>=0x20)return true;
        const auto e=std::size_t(d+1);owner.put8(0x220,std::uint8_t(e));
        owner.put8(0x221+e,owner.u8(0x220+e));owner.put8(0x241+e,owner.u8(0x240+e));
        const auto before=st.frontend_ui_missing_pc;st.frontend_ui_missing_pc=0;
        (void)native_frontend_commands(st,0x446fc0u,owner,0x51c,nullptr,0);
        const bool ok=st.frontend_ui_missing_pc==0;if(ok)st.frontend_ui_missing_pc=before;return ok;}
    case 0x440e60u:{                                        // history depth popped, 4464F0 on +51C
        const auto d=std::int8_t(owner.u8(0x220));
        if(d<=0)return true;
        owner.put8(0x221+std::size_t(d),0);owner.put8(0x241+std::size_t(d),0);owner.put8(0x220,std::uint8_t(d-1));
        driving::runtime_child_counter_dec_4464f0(owner.sub(0x51c,owner.size()-0x51c));return true;}
    // Text (956BA0.. print state) of the network screens into network_widgets.
    case 0x42ca60u:{
        auto& w=st.network_widgets;
        if(n<1||!st.frontend_fonts||a[0]>=st.frontend_fonts->fonts.size())return false;
        w.font=&st.frontend_fonts->fonts[a[0]];w.style.scale_x=1.0f;w.style.scale_y=1.0f;w.style.color=0xffffffffu;w.style.flags=1u;return true;}
    case 0x42cc60u:{float x,y;std::memcpy(&x,&a[0],4);std::memcpy(&y,&a[1],4);
        if(!(x==0.0f))st.network_widgets.style.scale_x=x;
        if(!(y==0.0f))st.network_widgets.style.scale_y=y;
        return true;}
    case 0x42cca0u:st.network_widgets.style.color=a[0];return true;
    case 0x42ccb0u:st.network_widgets.style.mode=a[0];return true;
    case 0x42c360u:st.network_widgets.style.flags=a[0];return true;
    case 0x42c820u:st.network_widgets.style.clip_left=std::int32_t(a[0]);st.network_widgets.style.clip_right=std::int32_t(a[1]);return true;
    case 0x42c840u:st.network_widgets.style.clip_left=0;st.network_widgets.style.clip_right=0x280;return true;
    case 0x42cc00u:{auto& cu=st.network_widgets.cursor;cu.x=std::int16_t(a[0]);cu.y=std::int16_t(a[1]);cu.origin_x=cu.x;return true;}
    case 0x42c370u:{                                        // the text's width (42C480, space 0.3)
        auto& w=st.network_widgets;if(!w.font)return false;
        eax=std::uint32_t(frontend_text_width_42c480(*w.font,w.style,native_network_string(a[0]),0.3f));return true;}
    case 0x42cdd0u:{                                        // printf text, proportional (42C860 / 42C610)
        auto& w=st.network_widgets;if(!w.font)return false;
        const auto s=native_network_format(a[0],a+1,n?n-1:0);
        if(std::getenv("OR2_NET_TEXT")){static unsigned k=0;if((k++%200u)==0u)std::fprintf(stderr,"[nettext] '%s' at %d,%d colour %08X mode %u clip %d..%d scale %.2f\n",s.c_str(),
            w.cursor.x,w.cursor.y,w.style.color,w.style.mode,w.style.clip_left,w.style.clip_right,double(w.style.scale_x));}
        if(!frontend_text_draw(*w.font,w.style,w.cursor,s,w.glyphs_))return false;
        w.style.flags=1u;return true;}
    case 0x4294c0u:{                                        // 4294C0(token, &w, &h): half the root component size; -1 without a scene
        FrontendSpriteTiming sc{};
        if(!st.frontend_sprites.bank_scene(a[0],sc)){eax=0xffffffffu;return true;}
        const float w=float(std::int16_t(sc.width))*0.5f,h=float(std::int16_t(sc.height))*0.5f;
        std::uint32_t wb,hb;std::memcpy(&wb,&w,4);std::memcpy(&hb,&h,4);
        native_network_put32(a[1],wb);native_network_put32(a[2],hb);return true;}
    case 0x4249f0u:                                         // the frontend sound effect (id)
        return n>=1&&st.frontend_effect&&st.frontend_effect(st.frontend_effect_user,a[0]);
    case 0x42d280u:                                         // image (token, x, y, frame, layer, colour), as the title widgets
        if(n<6)return false;
        st.network_widgets.images_.push_back({0x42d280,0,a[0],a[5],std::int32_t(a[1]),std::int32_t(a[2]),std::int32_t(a[3]),0,0,float(std::int32_t(a[4]))});
        return true;
    case 0x42d300u:{                                        // image (mode, token, x, y, width, height, frame, layer, colour)
        if(n<9)return false;
        auto f=[&](unsigned k){float v;std::memcpy(&v,&a[k],4);return v;};
        st.network_widgets.images_.push_back({0x42d300,a[0],a[1],a[8],std::int32_t(a[2]),std::int32_t(a[3]),std::int32_t(a[6]),f(4),f(5),f(7)});
        return true;}
    case 0x442f20u:if(n<2)return false;driving::object_store_depth_pair_442f20(owner,std::uint8_t(a[0]),std::uint8_t(a[1]));return true;
    case 0x429810u:if(n<1)return false;st.title_pause_flag_95b214=a[0];return true;
    case 0x42fc90u:case 0x42efa0u:return n>=1&&st.title_widgets&&st.title_widgets->call_volume(pc,a[0]);   // BGM / SE master volume
    case 0x441260u:                                         // the network is busy (reads the network globals)
        native_network_globals_in(st);
        eax=driving::runtime_ready_441260(owner,st.frontend_runtime_globals,native_state2_runtime_services(st));return true;
    case 0x441300u:{                                        // close the overlay when it is `key` (0x53: any)
        if(n<1)return false;
        const auto current=owner.u32(0x488)&&owner.u8(0x48c)?stack.key(&st,owner.u32(0x488)):0x53u;
        if(current!=a[0]&&a[0]!=0x53u)return true;
        if(owner.u8(0x48c)){if(const auto child=owner.u32(0x488)){stack.release(&st,child);owner.put32(0x488,0);}owner.put8(0x48c,0);}
        eax=1;return true;}
    case 0x442ef0u:{                                        // the current screen's key (0x53 at the root)
        const auto depth=owner.u32(0x484);
        eax=depth>=1u?stack.key(&st,owner.u32(0x280+depth*4)):0x53u;return true;}
    case 0x4440f0u:{                                        // a root screen (+498 list, +518 count)
        if(n<1)return false;
        const auto count=owner.u32(0x518);
        for(std::uint32_t i=0;i<count;++i)if(stack.key(&st,owner.u32(0x498+i*4))==a[0])return true;
        // 444140 / 444170: the listed screens' slots +1C / +18 are not ported; only an empty list is.
        if(count){st.frontend_last_missing_action=0x4440f0;return false;}
        const auto created=stack.create(&st,a[0]);
        if(!created)return true;
        owner.put32(0x498,created);owner.put32(0x518,1);
        if(!std::uint8_t(stack.invoke(&st,created,4))){
            stack.invoke(&st,created,0x10);stack.release(&st,created);
            owner.put32(0x498,0);owner.put32(0x518,0);return true;
        }
        eax=created;return true;}
    }
    return false;
}
void native_state2_owner_action(void* user,std::uint32_t pc_entry,
                                driving::Bytes owner,std::int32_t selected_kind,
                                std::uint32_t variant){
    if(!user)return;
    auto& state=*static_cast<NativeEventFunction36State*>(user);
    const FrontendStackServices stack=native_frontend_stack(state);
    if(pc_entry==0x4442a0u&&selected_kind>=0&&selected_kind<=3){
        // 4442A0 obtains the target (+4) from the selected owner, then
        // 443FA0 opens a flagged overlay. It does not push the primary stack.
        const auto source=selected_kind==0?owner.u32(0x280u+owner.u32(0x484u)*4u):
            owner.u32(selected_kind==1?0x488u:selected_kind==2?0x490u:0x498u);
        const auto* binding=driving::native_handle_find(native_frontend_handles(state),source);
        if(!binding)return;
        const auto key=driving::Bytes(binding->object,binding->size).u32(4);
        // 443FA0 overlays: key 24 (license editor) and key 16 (transmission).
        if(!((key==24&&state.license_owners)||(key==16&&state.runtime)||native_network_screen_key(key))){
            state.frontend_last_missing_key=key;state.frontend_last_missing_action=pc_entry;return;
        }
        frontend_close_overlays_4430b0(owner,stack);
        const auto created=stack.create(&state,key);
        if(created){
            owner.put32(0x488,created);
            if(std::uint8_t(stack.invoke(&state,created,4)))owner.put8(0x48c,1);
            else{stack.release(&state,created);owner.put32(0x488,0);}
        }
        if(selected_kind==3){stack.release(&state,source);driving::compact_u32_list_441410(owner.sub(0x498,0x84),0);}
    }else if(pc_entry==0x445160u&&variant==1u&&selected_kind>=1&&selected_kind<=3){
        // 445160 secondary-owner branch: the child closes, then in the race state 3
        // (the pause menu's Return): 4409C0 until [780248] clears (events resumed),
        // 43F9E0(0), and in mode 18 the +21C token [780268] (0x10 when it is 0x12).
        if(selected_kind==1)frontend_close_overlays_4430b0(owner,stack);
        else if(selected_kind==2){stack.release(&state,owner.u32(0x490));owner.put32(0x490,0);owner.put8(0x494,0);}
        else{stack.release(&state,owner.u32(0x498));driving::compact_u32_list_441410(owner.sub(0x498,0x84),0);}
        if(owner.u32(0x218)==3u&&driving::object_has_active_state_443060(owner)==0u&&state.title_event_state&&state.title_game_flag_780248){
            const driving::PcEventPauseServices pause{&state,[](void* u,std::uint32_t f,std::uint32_t a){
                auto& st=*static_cast<NativeEventFunction36State*>(u);
                if(f==0x43f9d0u)*st.title_game_flag_780248=std::uint8_t(a);
                else if(f==0x429810u)st.title_pause_flag_95b214=a;
                else if(f==0x449040u)st.title_pause_flag_7d2614=a;}};
            for(unsigned guard=0;*state.title_game_flag_780248==1u&&guard<256u;++guard)
                driving::clr_ev_pause_flag_4409c0(*state.title_event_state,pause);
            state.title_game_state_780270=0u;
            if(state.title_game_mode_78026c==0x12u&&owner.u32(0x21c)==0xffffffffu&&state.runtime){
                const std::uint32_t previous=state.runtime->mode_state.previous;      // 43F8F0: [780268]
                owner.put32(0x21c,previous==0x12u?0x10u:previous);
            }
            ++state.pause_resumes;
        }
    }else if(pc_entry==0x4450a0u&&variant==0u&&selected_kind>=0&&selected_kind<=3){
        const auto source=selected_kind==0?owner.u32(0x280u+owner.u32(0x484u)*4u):
            owner.u32(selected_kind==1?0x488u:selected_kind==2?0x490u:0x498u);
        const auto* binding=driving::native_handle_find(native_frontend_handles(state),source);
        if(!binding)return;
        const auto key=driving::Bytes(binding->object,binding->size).u32(4u);
        // Unimplemented constructors must not mutate the PC history every
        // frame or pretend that the requested screen has opened.
        if(key!=0u&&key!=1u&&key!=2u&&key!=4u&&key!=6u&&key!=8u&&!native_network_screen_key(key)&&key!=10u&&key!=16u&&key!=22u&&key!=36u&&key!=37u&&key!=38u&&key!=43u&&key!=44u&&key!=48u&&key!=49u&&key!=50u&&key!=51u&&key!=53u&&key!=57u&&key!=0x3au&&key!=0x3bu&&key!=0x3cu&&
           !((key==21u||key==24u)&&state.license_owners)){
            state.frontend_last_missing_key=key;state.frontend_last_missing_action=pc_entry;return;
        }
        if(selected_kind==3){stack.release(&state,source);driving::compact_u32_list_441410(owner.sub(0x498u,0x84u),0u);}
        if(frontend_push_444fe0(owner,key,stack)){
            ++state.frontend_gate_owner_actions;++state.frontend_stack_pushes;
            if(key==1u)++state.frontend_first_menu_opens;
            if(key==2u)++state.frontend_second_menu_opens;
            state.frontend_last_missing_key=~0u;state.frontend_last_missing_action=0u;
        }
    }else if(pc_entry==0x4448c0u&&variant==0u&&selected_kind>=0&&selected_kind<=3){
        const auto source=selected_kind==0?owner.u32(0x280u+owner.u32(0x484u)*4u):
            owner.u32(selected_kind==1?0x488u:selected_kind==2?0x490u:0x498u);
        const auto* binding=driving::native_handle_find(native_frontend_handles(state),source);
        if(!binding)return;
        const auto target=driving::Bytes(binding->object,binding->size).u32(4u);
        if(selected_kind==1)frontend_close_overlays_4430b0(owner,stack);
        if(selected_kind==2){
            stack.release(&state,source);owner.put32(0x490u,0u);owner.put8(0x494u,0u);
            frontend_close_overlays_4430b0(owner,stack);
        }
        if(selected_kind==3){stack.release(&state,source);driving::compact_u32_list_441410(owner.sub(0x498u,0x84u),0u);}
        (void)frontend_unwind_4448c0(owner,target,stack);
        ++state.frontend_stack_pops;
    }else if(pc_entry==0x445160u&&variant==0u&&selected_kind>=0&&selected_kind<=2){
        if(selected_kind==0)frontend_pop_444f40(owner,stack);
        else if(selected_kind==1)frontend_close_overlays_4430b0(owner,stack);
        else {stack.release(&state,owner.u32(0x490u));owner.put32(0x490u,0u);owner.put8(0x494u,0u);}
        const auto& pairs=driving::pc_object_state_pair_table_r065();
        driving::object_update_state_442fd0(owner,{pairs.data(),pairs.size()},
            {&state,native_state2_runtime_u32,native_stage12_embedded_u32});
        ++state.frontend_stack_pops;
    }else state.frontend_last_missing_action=pc_entry;
}

void native_state2_gate_configure(void* user,driving::Bytes,
                                  std::uint32_t,std::uint8_t){
    if(user)++static_cast<NativeEventFunction36State*>(user)->state2_gate_configure_calls;
}

void native_state2_gate_action(void* user,std::uint32_t pc_entry,
                               driving::Bytes,std::uint32_t){
    if(!user||pc_entry!=0x00444fe0u)return;
    ++static_cast<NativeEventFunction36State*>(user)->state2_gate_action_calls;
}

// 445A50's message: sprintf(buffer, text 0x340 / 0x341, the slot's name) then the network
// layer's pending message 492690(mode, buffer, lifetime, limit). The text and the
// formatting go through 492690's own buffer 65D00C (it copies the result over itself).
void native_state2_queue_format(void* user,std::uint32_t,std::uint32_t mode,std::uint32_t,
                                driving::Bytes slot,std::size_t text_offset,std::int32_t lifetime,std::uint32_t limit){
    auto& state=*static_cast<NativeEventFunction36State*>(user);
    const auto* format=state.frontend_text?state.frontend_text->get(mode?0x341u:0x340u):nullptr;
    if(!state.runtime||!format||!native_network_mapped()){driving::service_hole("native_runtime.cpp:445A50","the network layer (492690)");return;}
    std::string name;
    for(std::size_t k=text_offset;k<slot.size()&&slot.u8(k);++k)name.push_back(char(slot.u8(k)));
    constexpr std::uint32_t Buffer=0x65d00cu;
    native_network_put_string(Buffer+0x800u,*format);
    native_network_put_string(Buffer+0xc00u,name);
    const std::uint32_t arg=Buffer+0xc00u;
    std::string text=native_network_format(Buffer+0x800u,&arg,1);
    if(text.size()>0x3ffu)text.resize(0x3ffu);
    native_network_put_string(Buffer+0x400u,text);
    (void)native_network_invoke(*state.runtime,0x492690u,0,{mode,Buffer+0x400u,std::uint32_t(lifetime),limit});
}

driving::PcObjectEventDispatch443eb0Services native_state2_dispatch_services(
    NativeEventFunction36State& state){
    return {&state,native_stage12_embedded_void,native_stage12_embedded_u32,
            native_stage12_callback,native_stage12_dispatch_global};
}

driving::PcRuntimeControlServices native_state2_runtime_services(
    NativeEventFunction36State& state){
    driving::PcRuntimeControlServices services{};
    services.user=&state;
    services.call_u32=native_state2_runtime_u32;
    // 441260: 453CA0 on the network sessions [7D68B0] / [7D68AC] (one of their parts active).
    services.call_bool_pair=[](void* user,std::uint32_t pc,std::uint32_t session,std::uint32_t)->std::uint8_t{
        auto& st=*static_cast<NativeEventFunction36State*>(user);
        std::uint32_t eax{};
        if(pc==0x453ca0u&&st.runtime&&native_network_invoke(*st.runtime,0x453ca0u,session,{},&eax))return std::uint8_t(eax);
        return 0u;};
    return services;
}
// The network layer's globals the owner tick reads (441260 / 444530 / 445A50 /
// 4411A0): 7D68D4 gate, 7D68AC / 7D68B0 sessions, 836130, 7D68D2 / 7D68BF /
// 7D68D1, 659930, the 7D68CB request and 7D68D0. They live in the network
// module's memory once it runs; before that the frontend keeps its own values.
void native_network_globals_in(NativeEventFunction36State& state){
    if(!native_network_mapped())return;
    auto& g=state.frontend_runtime_globals;
    g.gate_d4=native_network_u8(0x7d68d4u);g.current_ac=native_network_u32(0x7d68acu);g.alternate_b0=native_network_u32(0x7d68b0u);
    g.mode_836130=native_network_u32(0x836130u);g.enabled_d2=native_network_u8(0x7d68d2u);g.blocked_bf=native_network_u8(0x7d68bfu);
    g.started_d1=native_network_u8(0x7d68d1u);g.reset_word_659930=native_network_u32(0x659930u);
    state.frontend_transition_flags.request_7d68cb=native_network_u8(0x7d68cbu);
    state.frontend_transition_flags.active_7d68d0=native_network_u8(0x7d68d0u);
}
void native_network_globals_out(NativeEventFunction36State& state){
    if(!native_network_mapped())return;
    native_network_put8(0x7d68cbu,state.frontend_transition_flags.request_7d68cb);
    native_network_put8(0x7d68d0u,state.frontend_transition_flags.active_7d68d0);
}

void native_event36_state2_call(NativeEventFunction36State& state,
                                std::uint32_t pc_entry,driving::Bytes object,
                                std::size_t object_offset){
    const auto handles=native_frontend_handles(state);
    const auto& pairs=driving::pc_object_state_pair_table_r065();
    const driving::PcObjectStatePairTable pair_view{pairs.data(),pairs.size()};
    const driving::PcObjectEventCallbackTable callback_view{
        state.callback_table.data(),state.callback_table.size()};
    const driving::PcObjectEventModeInputs event_inputs{};
    const auto dispatch_services=native_state2_dispatch_services(state);
    const auto runtime_services=native_state2_runtime_services(state);
    const driving::PcObjectOpenCallbackServices open_services{
        &state,native_stage12_handle_virtual};

    switch(pc_entry){
        case 0x00441020u:
            if(object_offset!=0u)return;
            ++state.state2_frames;
            ++state.state2_transition_ticks;
            driving::object_transition_update_441020(
                object,state.loader_stage12_state.transition_globals,
                {&state,native_state2_transition_activate});
            return;
        case 0x00443110u:
            if(object_offset!=0u)return;
            ++state.state2_ui_state_ticks;
            {   // +0xBA8: the screen frame (the 4432B0 scene: red Ferrari band, help bar),
                // +0xC48: 4400D3; both opened through the frontend sprite pool.
                FrontendUiResources ui{state.frontend_sprites};
                ui.motion_step=state.frontend_ui_motion_step;
                ui.effect_user=state.frontend_effect_user;ui.effect_4249f0=state.frontend_effect;
                const driving::PcObjectUiOpenServices open{&ui,
                    [](void* p,std::uint32_t,std::uint32_t pc,driving::Bytes o,std::size_t off){
                        std::uint32_t r{};(void)static_cast<FrontendUiResources*>(p)->call(pc,o.sub(off,0xa0u),nullptr,0,r);},
                    [](void* p,std::uint32_t,std::uint32_t pc,driving::Bytes o,std::size_t off,const std::array<std::uint32_t,11>& a){
                        std::uint32_t r{};(void)static_cast<FrontendUiResources*>(p)->call(pc,o.sub(off,0xa0u),a.data(),a.size(),r);}};
                driving::object_ui_state_update_443110(
                    object,state.frontend_ui_globals,native_state2_ui_services(state),open);
                if(ui.missing_pc)state.frontend_ui_missing_pc=ui.missing_pc;
            }
            return;
        case 0x00444840u:{
            if(object_offset!=0u)return;
            ++state.state2_ui_ticks;
            FrontendUiResources ui{state.frontend_sprites};
            ui.motion_step=state.frontend_ui_motion_step;
            driving::object_runtime_ui_tick_444840(
                object,handles,native_state2_ui_services(state),ui.ticks(),
                {&state,native_state2_ui_open});
            if(ui.missing_pc)state.frontend_ui_missing_pc=ui.missing_pc;
            return;
        }
        case 0x00446cf0u:
            if(object_offset!=0x051cu)return;
            ++state.state2_embedded_ticks;
            native_frontend_commands(state,pc_entry,object,object_offset,nullptr,0);
            return;
        case 0x00445810u:{
            if(object_offset!=0u)return;
            ++state.state2_gate_ticks;
            driving::PcRuntimeGate445810Inputs inputs{};
            inputs.alternate_7d68bc=state.frontend_queue.alternate_7d68bc;
            if(state.runtime)inputs.system_handle_67f614=std::uint32_t(state.runtime->start_mode.start_system_handle_67f614);   // 4999C0
            inputs.menu_state_7d38f0=state.runtime?race_over_state_450240(state.runtime->race.manager.state):0u;   // race manager binding: 7D38F0
            const driving::PcRuntimeGate445810Services gate_services{
                &state,native_state2_gate_configure,native_state2_gate_action};
            driving::object_runtime_gate_445810(
                object,object,handles,inputs,pair_view,callback_view,event_inputs,
                dispatch_services,runtime_services,open_services,gate_services);
            return;
        }
        case 0x00444650u:   // the race state (+218 = 3): [7B17ED] (never set: no 4440F0(0x4B)), then the 444530 body
        case 0x00444530u:
            if(object_offset!=0u)return;
            ++state.state2_runtime_transition_ticks;
            native_network_globals_in(state);
            driving::object_runtime_transition_444530(
                object,state.frontend_transition_flags,state.frontend_runtime_globals,
                handles,pair_view,callback_view,event_inputs,dispatch_services,
                runtime_services,open_services);
            native_network_globals_out(state);
            return;
        case 0x00445a50u:
            if(object_offset!=0u)return;
            ++state.state2_queue_ticks;
            native_network_globals_in(state);
            driving::object_runtime_queue_445a50(
                object,object,
                driving::Bytes(state.frontend_manager.data(),state.frontend_manager.size()),
                driving::Bytes(state.frontend_ui_lookup.data(),state.frontend_ui_lookup.size()),
                state.frontend_queue,state.frontend_runtime_globals,pair_view,callback_view,
                event_inputs,dispatch_services,runtime_services,open_services,
                {&state,native_state2_gate_action},{&state,native_state2_queue_format});
            return;
        case 0x004411a0u:
            if(object_offset!=0u)return;
            ++state.state2_shutdown_ticks;
            native_network_globals_in(state);
            driving::runtime_shutdown_4411a0(
                state.frontend_runtime_globals,object,runtime_services);
            return;
        default:return;
    }
}

void native_event36_owner_call(void* user,std::uint32_t pc_entry,
                               driving::Bytes object,std::size_t object_offset){
    if(!user)return;
    auto& state=*static_cast<NativeEventFunction36State*>(user);
    if(pc_entry==0x00444470u&&object_offset==0u){
        ++state.title_gate_ticks;
        const auto& pairs=driving::pc_object_state_pair_table_r065();
        const driving::PcObjectStatePairTable pair_view{pairs.data(),pairs.size()};
        const driving::PcObjectEventCallbackTable callback_view{
            state.callback_table.data(),state.callback_table.size()};
        const driving::PcRuntimeGate444470Inputs inputs{
            state.title_game_mode_78026c,0u,state.runtime?std::uint32_t(state.runtime->start_mode.start_system_handle_67f614):0xffffffffu,   // 4999C0
            state.runtime?race_over_state_450240(state.runtime->race.manager.state):0u,state.title_player0_feature_mask};   // race manager binding: 7D38F0
        // 443EB0 inside 443FA0: the race (78026C = 16) gets the +21C token 0x12 (mode 18).
        const driving::PcObjectEventModeInputs mode_inputs{
            state.title_game_flag_780248?*state.title_game_flag_780248:std::uint8_t(0),state.title_variant_780258,0u,state.title_game_mode_78026c};
        driving::object_runtime_gate_444470(
            object,object,native_frontend_handles(state),inputs,pair_view,
            callback_view,mode_inputs,native_state2_dispatch_services(state),
            native_state2_runtime_services(state),
            {&state,native_stage12_handle_virtual},
            {&state,native_state2_gate_configure});
        return;
    }
    if(pc_entry!=0x00445500u){
        native_event36_state2_call(state,pc_entry,object,object_offset);
        return;
    }
    if(object_offset!=0u)return;
    if(object.u32(0u)==3u){
        const driving::PcRuntimeLoaderServices48bf20 loader_services{
            &state,native_loader_request};
        if(driving::object_runtime_loader_stage3_4455df(
               object,driving::Bytes(state.loader_83039c.data(),state.loader_83039c.size()),
               loader_services))
            ++state.loader_begin_calls;
        return;
    }
    if(object.u32(0u)==4u){
        if(!driving::object_runtime_loader_stage4_4455f2(
               object,state.loader_resource_pending))return;
        ++state.loader_stage4_passes;
    }
    if(object.u32(0u)==5u){
        ++state.shared_loader_calls;
        state.shared_last_result=driving::runtime_shared_loader_49e580(
            state.shared_loader,{&state,native_shared_loader_call});
        if(state.shared_last_result!=0u){
            const std::uint32_t args[]{0x44u,9u};
            (void)native_shared_loader_call(&state,0x0042deb0u,args,2u);
            object.put32(0u,6u);
        }
        return;
    }
    if(object.u32(0u)>=6u&&object.u32(0u)<=11u){
        if(driving::object_runtime_loader_stages6_to11_445627(
               object,{&state,native_shared_loader_call}))
            ++state.loader_stage12_passes;
        return;
    }
    if(object.u32(0u)==12u){
        ++state.loader_stage12_body_calls;
        state.loader_stage12_state.mode_6319a1=state.global_mode_6319a1;
        state.loader_stage12_state.alternate_7b17ec=state.snapshot.active;
        // 4457B7: [7B17F8] (the common save's selected license, -1 when none) and the
        // licence's dirty bits [7C27D4] decide the 416420 save on return to the menus.
        state.loader_stage12_state.optional_index_7b17f8=
            state.frontend_profiles.selected==~0u?-1:std::int32_t(state.frontend_profiles.selected);
        state.loader_stage12_state.optional_flags_7c27d4=state.frontend_profiles.active[0x3f4];
        if(driving::object_runtime_loader_stage12_4456ea(
               object,state.loader_stage12_state,
               {&state,native_stage12_global_call,native_stage12_this_call,
                native_stage12_handle_virtual})){
            ++state.loader_stage14_passes;
            ++state.stage12_transition_init_calls;
            state.global_mode_6319a1=state.loader_stage12_state.mode_6319a1;
        }
    }
}

void native_frontend_menu_update(NativeRuntimeContext& context){
    auto& state=context.event_function36;
    driving::Bytes owner(state.object.data(),state.object.size());
    if(context.mode_state.current!=32u)return;
    const auto& input=context.input_state;
    const auto depth=owner.u32(0x484u);
    const auto current=depth&&depth<=128?owner.u32(0x280u+depth*4u):0u;
    const auto license=[](std::uint32_t handle){return handle==0x73000006||handle==0x73000007;};
    const bool fixed=current&&(current==state.frontend_first_menu_handle||current==state.frontend_second_menu_handle);
    if(fixed||license(current)||(owner.u8(0x48c)&&license(owner.u32(0x488)))||
       (owner.u8(0x494)&&license(owner.u32(0x490)))){
        // These owners consume the real shared 48F5F0 snapshot/repeat state.
        // Do not retain a second legacy A/B edge for the screen they return to.
        state.frontend_pending_input_action=state.frontend_gate_input_action=~0u;
        return;
    }

    // The PC frontend owns the confirm/cancel edge, not the loader.  On real
    // hardware the retail loader can finish after the first visible SUMO_FE
    // frame, so dropping the edge here makes A appear dead even though key22
    // becomes valid immediately afterwards. Queue only A/B; directional input
    // remains edge-driven and is never replayed later.
    if(!state.frontend_menu_committed&&state.frontend_pending_input_action==0xffffffffu){
        if(input.menu_confirm)state.frontend_pending_input_action=0u;
        else if(input.menu_cancel)state.frontend_pending_input_action=1u;
    }
    if(owner.u32(0x0218u)!=2u||state.frontend_handle_count<1u)return;
    // depth was validated above for the original callback list.
    const auto indexed=depth>=1u&&depth<0x80u?
        owner.u32(0x0280u+depth*4u):0u;
    const bool active_choice=indexed!=0u&&(indexed==state.frontend_first_menu_handle||
        indexed==state.frontend_second_menu_handle);
    const bool active_welcome=indexed!=0u&&indexed==state.stage12_last_handle&&
        driving::Bytes(state.frontend_root_object.data(),state.frontend_root_object.size()).u32(0x740u)==1u;
    if(state.frontend_gate_animation_pending!=0u&&
       state.frontend_authored_animation_ready!=0u){
        // r155: completion is owned by the authored SUMO_FE timeline rendered
        // on the previous frame.  Do not manufacture a one-frame completion.
        driving::Bytes(state.frontend_gate_object.data(),state.frontend_gate_object.size())
            .put8(0x17b0u,1u);
        state.frontend_gate_animation_pending=0u;
        state.frontend_authored_animation_ready=0u;
    }
    const bool can_accept=owner.u8(0x048cu)!=0u||active_choice||active_welcome;
    if(!state.frontend_menu_committed&&can_accept&&
       state.frontend_pending_input_action<=1u){
        state.frontend_gate_input_action=state.frontend_pending_input_action;
        if(state.frontend_pending_input_action==0u)++state.frontend_menu_confirms;
        else ++state.frontend_menu_cancels;
        state.frontend_pending_input_action=0xffffffffu;
    }else if(!state.frontend_menu_committed&&can_accept&&input.menu_left){
        state.frontend_gate_input_action=2u;
    }else if(!state.frontend_menu_committed&&can_accept&&input.menu_right){
        state.frontend_gate_input_action=4u;
    }else if(!state.frontend_menu_committed&&active_choice&&input.menu_up){
        state.frontend_gate_input_action=3u;
    }else if(!state.frontend_menu_committed&&active_choice&&input.menu_down){
        state.frontend_gate_input_action=5u;
    }
    if(input.menu_start_probe&&!state.frontend_menu_committed){
        // Diagnostic trigger only. The selected values themselves now come
        // from the retail RACE_MAPPING_ARRAY and Races categories, exactly
        // as PC 0x4EEB50 -> 0x4958A0, not from a fixed Cape Town guess.
        auto& start=context.start_mode;
        if(start.scene_owner_race_assets&&start.scene_owner_assignment_assets){
            std::uint32_t race_key=0u,sub_key=0u;
            if(race_menu_choice_4eeb50(*start.scene_owner_race_assets,
                    *start.scene_owner_assignment_assets,
                    state.frontend_menu_index,0u,race_key,sub_key)){
                start.scene_owner_race_key=race_key;
                start.scene_owner_race_sub_key=sub_key;
                start.scene_owner_race_key_known=true;
                start.selection_active_836374=true;
            }
        }
        state.frontend_menu_committed=true;
        ++context.start_mode.original_route_requests;
        driving::object_set_token_440de0(owner,13u);
    }
}

bool native_course_pack_read(void* user,const char* path,
                             std::uint32_t mode,std::uint32_t required,
                             void* destination,std::size_t capacity,
                             std::size_t& bytes_read){
    bytes_read=0u;
    if(!user)return false;
    auto& game=*static_cast<NativeGameModeState*>(user);
    ++game.course_read_calls;
    if(!game.course_assets||!path||!destination||mode>1u||required>1u)return false;   // required 0: 46C2F0's request courses
    game.course_last_path=path;
    if(std::strcmp(path,"\\Scripts\\bin\\csc_data_cvt.bin")==0){
        const auto& source=game.course_assets->course_blob;
        if(source.empty()||source.size()>capacity)return false;
        std::memcpy(destination,source.data(),source.size());
        bytes_read=source.size();
        return true;
    }
    if(!game.course_retail||std::strncmp(path,"\\Scripts\\bin\\",13)!=0)return false;
    std::vector<std::uint8_t> bytes;std::string relative="Scripts/bin/";relative+=path+13;
    if(!retail_asset_read_relative(*game.course_retail,relative,bytes,capacity)||bytes.empty()||bytes.size()>capacity)return false;
    std::memcpy(destination,bytes.data(),bytes.size());
    bytes_read=bytes.size();
    return true;
}

void native_game_load_course(NativeGameModeState& game,std::uint32_t course_mode=0u,
                             std::uint8_t alternate=0u){
    if(!game.course_assets)return;
    ++game.course_load_calls;
    game.course_load={};
    game.course_runtime={};
    // Capacity for any \\Scripts\\bin\\csc_data_*.bin course file.
    game.course_work.assign(std::max<std::size_t>(game.course_assets->course_blob.size(),0x10000u),0u);
    game.course_matrix_stack.fill(0u);
    driving::Bytes matrix_bytes(game.course_matrix_stack.data(),game.course_matrix_stack.size());
    for(std::size_t i=0;i<16u;++i)
        matrix_bytes.put32(i*4u,(i%5u)==0u?0x3f800000u:0u);
    driving::PcMatrixStack matrices{matrix_bytes,0,0,2};
    driving::PcCourseProviderR077 provider{};
    provider.storage=game.course_work.data();
    provider.capacity=game.course_work.size();
    provider.read_user=&game;
    provider.read_file=native_course_pack_read;
    driving::PcCourseRuntimeBridgeR077 bridge{};
    bridge.tables=driving::course_descriptor_tables_r078(game.course_assets->descriptors);
    bridge.state=&game.course_runtime;
    bridge.matrices=&matrices;
    bridge.services=driving::course_provider_services_r077(provider);
    const bool invoked=driving::runtime_course_load_44da00(
        course_mode,alternate,game.course_load,
        driving::runtime_course_load_services_r077(bridge));
    game.course_bytes=static_cast<std::uint32_t>(provider.size);
    game.course_loader_phase=static_cast<std::uint32_t>(bridge.loader.phase);
    game.course_provider_loaded=provider.loaded;
    game.course_matrix_ready=game.course_runtime.selected_copy_active&&
        std::any_of(game.course_runtime.matrix_7d3190.begin(),
                    game.course_runtime.matrix_7d3190.end(),
                    [](std::uint8_t value){return value!=0u;});
    if(invoked&&provider.loaded&&bridge.loader.phase==3&&
       game.course_runtime.active_count_7d33c4>0&&
       game.course_runtime.selected_copy_active&&game.course_matrix_ready)
        ++game.course_load_success;
}

// 46C2F0 (variant 5, START 49DF05): 44D720(name, name, 7F94F0, key, force, 1, 0, 0, 0, 0) with the
// C2C request manager's course config 7F94C4..7F94D4 (46C280 / 4EEB50): the course data
// 64F040[cfg0] (Request_Course_OR2 ...), the selected stage cfg2 forced when cfg1; its loader
// state 7F94F0 (+8 phase) lives in the manager block (+90).
bool native_game_load_request_course(NativeRuntimeContext& c){
    auto& game=c.game_mode;
    if(!game.course_assets)return false;
    static constexpr const char* Names[16]{"Request_Course_OR2","Request_Course_SP","Req_Course_MIX_PSP","Req_Course_MIX_PS2",
        "Test_Course","csc_data_cvt","csc_data_2","csc_data_easy","csc_data_cvt","csc_data_2","csc_data_cvt_ren",
        "csc_data_2_ren","csc_data_cvt_ren","csc_data_cvt_ren","csc_data_2_ren","csc_data_cvt_ren"};   // 64F040[]
    const auto& cfg=c.start_mode.frontend_prepare.event_config_7f94c4;
    if(cfg[0]>=16u)return false;
    auto& block=native_race_traffic(c).requests_7f9460;
    ++game.course_load_calls;
    game.course_load={};game.course_runtime={};
    game.course_work.assign(std::max<std::size_t>(game.course_assets->course_blob.size(),0x10000u),0u);
    game.course_matrix_stack.fill(0u);
    driving::Bytes matrix_bytes(game.course_matrix_stack.data(),game.course_matrix_stack.size());
    for(std::size_t i=0;i<16u;++i)matrix_bytes.put32(i*4u,(i%5u)==0u?0x3f800000u:0u);
    driving::PcMatrixStack matrices{matrix_bytes,0,0,2};
    driving::PcCourseProviderR077 provider{};
    provider.storage=game.course_work.data();provider.capacity=game.course_work.size();
    provider.read_user=&game;provider.read_file=native_course_pack_read;
    driving::PcCourseApplyRequest44d720 request{};
    request.data_name=Names[cfg[0]];request.category_name=Names[cfg[0]];
    request.loader={true,std::int32_t(le32(block.data()+0x98u))};
    request.selected_key=cfg[1]?cfg[2]:0u;request.force_selected=cfg[1]!=0u;
    request.loader_mode=1u;request.required=0u;
    const bool ok=driving::runtime_apply_course_data_44d720(request,driving::course_descriptor_tables_r078(game.course_assets->descriptors),
        game.course_runtime,matrices,driving::course_provider_services_r077(provider));
    std::fprintf(stderr,"[dbg] reqcourse ok=%d loaded=%d phase=%d sel=%d prim=%x cfg=%u/%u/%u size=%zu\n",int(ok),int(provider.loaded),int(request.loader.phase),int(game.course_runtime.selected_copy_active),le32(game.course_runtime.selected_7d30a8.data()+0x14u),cfg[0],cfg[1],cfg[2],provider.size);
    if(ok&&request.loader.phase<3){const std::uint32_t three=3u;std::memcpy(block.data()+0x98u,&three,4);}
    game.course_bytes=static_cast<std::uint32_t>(provider.size);game.course_provider_loaded=provider.loaded;
    game.course_matrix_ready=game.course_runtime.selected_copy_active&&
        std::any_of(game.course_runtime.matrix_7d3190.begin(),game.course_runtime.matrix_7d3190.end(),[](std::uint8_t v){return v!=0u;});
    if(ok&&provider.loaded&&game.course_runtime.active_count_7d33c4>0&&game.course_runtime.selected_copy_active&&game.course_matrix_ready)
        ++game.course_load_success;
    return ok;
}
}  // namespace
namespace {
// 43D950(stage, branch, second half): the stage resource of a single-stage Time Attack course.
std::uint32_t stage_resource_43d950(std::uint32_t stage,bool branch,bool second){
    static constexpr std::uint8_t left[15]{0,1,3,6,8,9,5,2,4,7,0xa,0xb,0xc,0xe,0xd};
    static constexpr std::uint8_t right[15]{0xf,0x15,0x18,0x11,0x10,0x14,0x17,0x12,0x13,0x16,0x1d,0x19,0x1b,0x1c,0x1a};
    std::uint32_t r=stage<=14u?(branch?right[stage]:left[stage]):(branch?1u:0u);   // past the table: the branch argument
    return second?r+0x1eu:r;
}
}
// 48B550: the goal / stage course of a Time Attack code (656234).
bool native_goal_course_48b550(NativeRuntimeContext& c,std::uint32_t code,std::uint8_t flag){
    return native_goal_course_48b550(c,code,flag,c.start_mode.ta_phase_83038c);
}
bool native_goal_course_48b550(NativeRuntimeContext& c,std::uint32_t code,std::uint8_t flag,std::uint32_t& phase){
    auto& st=c.start_mode;auto& game=c.game_mode;
    const bool ta=st.frontend_prepare.game_flag_780260!=0u&&st.flag_830394!=0u;     // 43F860 && [830394]
    static constexpr const char* Goals[5]{"GOAL_A","GOAL_B","GOAL_C","GOAL_D","GOAL_E"};
    static constexpr const char* GoalsTa[5]{"GOAL_A","GOAL_B_TA","GOAL_C_TA","GOAL_D_TA","GOAL_E"};
    const char* data=nullptr;const char* category=nullptr;std::int32_t resource=-1;
    const std::int32_t k=std::int32_t(code);
    if(k<0x3c){
        // 48B55F: second half (30..59), branch (15..29 of each half), stage 0..14.
        std::int32_t i=k;bool second=false,branch=false;
        if(i>=0x1e){second=true;i-=0x1e;}
        if(i>=0xf){branch=true;i-=0xf;}
        std::uint32_t r=stage_resource_43d950(std::uint32_t(i),branch,second);
        if(r==0u)r=0x3cu;else if(r==0xfu)r=0x3du;
        if(flag&&r<=0x3du){                                                  // 48B8AC / 48B898
            switch(r){case 0x3cu:r=0x3eu;break;case 0x3du:r=0x3fu;break;case 0x1eu:r=0x40u;break;case 0x2du:r=0x41u;break;default:break;}
        }
        resource=std::int32_t(r);data="OR2_goals";category="GOAL_A";
    }else if(k<=0x40){data="OR2_goals";category=(ta?GoalsTa:Goals)[k-0x3c];}
    else if(k<=0x45){data="SP_goals";category=(ta?GoalsTa:Goals)[k-0x41];}
    else if(k<=0x4a){data="OR2_reverse_goals";category=(ta?GoalsTa:Goals)[k-0x46];}
    else if(k<=0x4f){data="SP_reverse_goals";category=(ta?GoalsTa:Goals)[k-0x4b];}
    else if(k==0x50){data="csc_data_2_ren";category=ta?"csc_data_2_ren_course_TA":"csc_data_2_ren_course";}
    else if(k==0x51){data="csc_data_2_ren";category=ta?"csc_data_2_reverse_ren_course_TA":"csc_data_2_reverse_ren_course";}
    else if(k==0x52){data="csc_data_cvt_ren";category=ta?"csc_data_cvt_ren_course_TA":"csc_data_cvt_ren_course";}
    else if(k==0x53){data="csc_data_cvt_ren";category=ta?"csc_data_cvt_reverse_ren_course_TA":"csc_data_cvt_reverse_ren_course";}
    else return false;                                                       // 48B892: xor al,al
    // 44D720(data, category, &830384, 0, 0, 1, 0, 0, 0, 0): a loader at phase >= 3 does not load again.
    if(phase>=3u)return true;
    const bool ok=native_course_load_named(c,data,category,resource,phase);
    if(std::getenv("OR2_FE_DUMP"))std::fprintf(stderr,"48B550 code=%x %s/%s resource=%d ok=%d bytes=%u path=%s\n",code,data,category,resource,int(ok),game.course_bytes,game.course_last_path.c_str());
    if(!ok)return false;
    phase=3u;++game.course_load_success;
    return true;
}
// 44D720(data, category, loader, 0, 0, 1, 0, 0, 0, 0) of a named course script (csc_data_*,
// *_goals) onto the live course state; resource >= 0: the single-stage resource 635F34.
bool native_course_load_named(NativeRuntimeContext& c,const char* data,const char* category,std::int32_t resource,std::uint32_t phase){
    auto& game=c.game_mode;
    if(!game.course_assets)return false;
    ++game.course_load_calls;
    game.course_load={};game.course_runtime={};
    if(resource>=0){                                                         // 48B592 / 48B598 / 48B5A8
        game.course_runtime.resource_source_635f38=0u;game.course_runtime.fallback_gate_7d33b0=0u;
        game.course_runtime.resource_type_635f34=std::uint32_t(resource);
    }
    game.course_work.assign(std::max<std::size_t>(game.course_assets->course_blob.size(),0x10000u),0u);
    game.course_matrix_stack.fill(0u);
    driving::Bytes matrix_bytes(game.course_matrix_stack.data(),game.course_matrix_stack.size());
    for(std::size_t i=0;i<16u;++i)matrix_bytes.put32(i*4u,(i%5u)==0u?0x3f800000u:0u);
    driving::PcMatrixStack matrices{matrix_bytes,0,0,2};
    driving::PcCourseProviderR077 provider{};
    provider.storage=game.course_work.data();provider.capacity=game.course_work.size();
    provider.read_user=&game;provider.read_file=native_course_pack_read;
    driving::PcCourseRuntimeBridgeR077 bridge{};
    bridge.tables=driving::course_descriptor_tables_r078(game.course_assets->descriptors);
    bridge.state=&game.course_runtime;bridge.matrices=&matrices;
    bridge.services=driving::course_provider_services_r077(provider);
    bridge.loader={true,std::int32_t(phase)};
    // The PC read is asynchronous (arg 10 = 0); the native read completes in this call.
    const bool ok=driving::runtime_course_apply_bridge_r077(&bridge,data,category,1u)!=0u;
    game.course_bytes=static_cast<std::uint32_t>(provider.size);
    game.course_loader_phase=static_cast<std::uint32_t>(bridge.loader.phase);
    game.course_provider_loaded=provider.loaded;
    game.course_matrix_ready=game.course_runtime.selected_copy_active&&
        std::any_of(game.course_runtime.matrix_7d3190.begin(),game.course_runtime.matrix_7d3190.end(),[](std::uint8_t v){return v!=0u;});
    return ok;
}

namespace {
void native_start_mode_init(NativeRuntimeContext& context){
    auto& start=context.start_mode;
    start.active=true;
    start.stage=0u;
    start.resource_id=0u;
    start.resource_bytes=0u;
    start.resource_ready=0u;       // the START pack is requested again (42DEB0) by every START
    start.stage60_phase=0u;
    start.stage60_route=0u;
    start.stage64_cleanup_step=0u;
    start.game_requests=0u;
    start.race_event_count=0u;
    start.pending_1ee=0u;
    start.stage3_counter=0u;
    // [7DE418] (the CommRace player count) is not touched by START_Init.
    // [8369C0] (49B450: the LAN race start 4F5270 sets it): START's versus path (the 0x49
    // loading screen, the 459210 / 4F5360 / 4F53B0 / 4557F0 / 4F5150 start synchronisation).
    start.use_versus_resource=native_network_u32(0x8369c0u)!=0u;
    start.manager_state_7f94c0=0u;
    start.render_mode_754b0c=0u;
    start.first_race_flags_7d3a10=0u;
    // 836374 is not touched by START_Init 49DB20: it keeps the value the
    // SUMO_FE commit (4EEC80 -> 4957D0) or the mission manager left.
    start.game_flag_780248=0u;
    start.last_missing_service=0u;
    start.scene_owner_calls=0u;
    start.scene_owner_stage=5u;
    start.scene_owner_missing_service=0u;
    start.scene_owner_sprite_requests=0u;
    start.scene_owner_sprite_bytes=0u;
    start.scene_owner_resource_ids={};
    start.scene_owner_resource_modes={};
    start.scene_owner_resource_count=0u;
    start.scene_owner_resource_bytes=0u;
    start.scene_owner_release_count=0u;
    start.scene_owner_meter_id=0u;
    start.scene_owner_audio_channels_active=0u;
    start.scene_owner_audio_command=0u;
    start.scene_owner_audio_phase=0u;
    start.scene_owner_audio_resets=0u;
    start.scene_owner_scheduler_passes=0u;
    start.scene_owner_sound_checks=0u;
    start.scene_owner_stage22_requests=0u;
    start.scene_owner_stage23_requests=0u;
    start.scene_owner_stage23_dynamic_id=0u;
    start.scene_owner_motion_requests=0u;
    start.scene_owner_motion_bytes=0u;
    start.scene_owner_stage48_reset_driver=0u;
    start.scene_owner_stage48_character_id=0u;
    start.scene_owner_stage48_motion_group=0u;
    start.scene_owner_stage48_reset_common=0u;
    start.scene_owner_route_manager_ready=false;
    start.scene_owner_route_manager_kind=-1;
    start.scene_owner_race_record_index=0u;
    start.scene_owner_race_course_count=0u;
    start.scene_owner_world_ids={};
    start.scene_owner_world_ids_ready=false;
    start.scene_owner_world_descriptor_index=0u;
    start.scene_owner_stage51_requests=0u;
    start.scene_owner_collision_state=0u;
    start.scene_owner_collision_requests=0u;
    start.scene_owner_collision_bytes=0u;
    start.scene_owner_course_world.reset();
    start.scene_owner_collision_error.clear();
    start.scene_owner_world_reset_count=0u;
    start.scene_owner_environment_states={};
    start.scene_owner_environment_requests=0u;
    start.scene_owner_environment_ready=0u;
    start.scene_owner_environment_bytes=0u;
    start.scene_owner_environment.reset();
    start.scene_owner_environment_error.clear();
    start.scene_owner_environment_fixups=0u;
    // 0x44A080 does not clear 0x7D28B0 flags. Preserve those module globals;
    // 0x44AA80 sets their special-mode values only after successful fixup.
    start.scene_owner_course_objects_reset_count=0u;
    start.scene_owner_course_object_states={};
    start.scene_owner_course_objects={};
    start.scene_owner_course_object_identities={};
    start.scene_owner_course_object_requests=0u;
    start.scene_owner_course_object_ready=0u;
    start.scene_owner_course_object_bytes=0u;
    start.scene_owner_course_object_error.clear();
    start.scene_owner_actor_reset_4f0380={};
    start.scene_owner_event_reset_4f0400={};
    start.scene_owner_global_reset_4ef860=0u;
    start.scene_owner_global_reset_4ef850=0u;
    start.scene_owner_object_states={};
    start.scene_owner_object_files={};
    start.scene_owner_object_identities={};
    start.scene_owner_object_requests=0u;
    start.scene_owner_object_ready=0u;
    start.scene_owner_object_bytes=0u;
    start.scene_owner_object_error.clear();
    // 0x46FAC0/0x46FDD0 family resets are part of a fresh scene owner.  r145
    // accidentally carried these values across a new START transition.
    start.scene_owner_target_outer_state=0u;
    start.scene_owner_target_index=0u;
    start.scene_owner_target_states={};
    start.scene_owner_target_requests=0u;
    start.scene_owner_target_path.clear();
    start.scene_owner_table68={};
    start.scene_owner_table6c={};
    start.scene_owner_table68_token=0u;
    start.scene_owner_table6c_token=0u;
    start.scene_owner_stage17_table_bytes=0u;
    start.scene_owner_table68_ready=false;
    start.scene_owner_table6c_ready=false;
    start.scene_owner_selected64_token=0u;
    start.scene_owner_loader_reset_44fcc0=0u;
    start.scene_owner_stage17_resource_id=0u;
    start.scene_owner_stage17_resource_mode=0u;
    start.scene_owner_stage17_resource_requests=0u;
    start.scene_owner_stage17_resource_ready=0u;
    start.scene_owner_stage17_resource_bytes=0u;
    start.scene_owner_stage50_scheduler_passes=0u;
    start.scene_owner_stage53_noop_49a650=0u;
    start.scene_owner_stage54_noop_47f110=0u;
    start.scene_owner_stage55_restore_452e30=0u;
    start.scene_owner_stage55_restore_452db0=0u;
    start.scene_owner_stage55_variant=0u;
    start.scene_owner_stage55_course_preset=0u;
    start.scene_owner_stage63_sound_ids={};
    start.scene_owner_stage63_sound_checks=0u;
    start.scene_owner_complete_returns=0u;
    // 49DBEA..49DC36: the player car events [680AD4] and the traffic events [680AD8] (0x18
    // together): 8 / 0x10 with the ranking word 80FB14, 6 / 0x12 in variant 4 (LAN), else
    // 4 / 0x14. The racer setup 47CF40 (traffic init) overrides them when racers exist.
    if(!std::getenv("OR2_680AD4_OLD")){auto& r=context.mission.racers;const auto before=r.v680ad4;
        r.v680ad4=context.race.car_world.gate_80fb14?8u:context.game_mode.game_variant==4u?6u:4u;r.v680ad8=0x18u-r.v680ad4;
        if(std::getenv("OR2_680AD4_TRACE"))std::fprintf(stderr,"[680ad4] START %u -> %u\n",before,r.v680ad4);}
    native_record_queue_reset_4ad190(context);  // 49DC4A: 4AD190 clears the record car queue 841FF0/841FF1
    native_ghost_queue_reset_4acfb0(context);   // 49DC54: 4ACFB0 clears the ghost car queue 841FA8/841FA9
    start.network_mode_7df108=0u;
    start.network_player_count_7df10f=0u;
    start.network_manager_7df34c_present=false;
    start.route_aux_roots_84d6cc={};
    start.route_suppress_830394=0u;
    start.route_gate_8361b4=0u;
    start.start_ready_4557f0_checks=0u;
    start.start_bootstrap_440380_calls=0u;
    start.start_bootstrap_event8_setups=0u;
    start.start_bootstrap_records_49fa80=0u;
    start.start_route_aux_4f0d10_calls={};
    start.start_route_init_4871a0_calls=0u;
    start.start_route_entry_count=0u;
    start.start_route_player_and_mask=0xffffffb9u;
    start.start_route_player_or_bits=0x38u;
    start.start_route_scene_82e7d4=0u;
    start.start_route_mode_82e7d8=0u;
    start.start_route_state_82e7e4=0u;
    start.start_gate_45a920_checks=0u;
    start.start_system_handle_67f614=-1;
    start.start_cleanup_4999f0_calls=0u;
    start.start_cleanup_428600_calls=0u;
    start.start_cleanup_42dfb0_ids={};
    start.start_cleanup_42dfb0_count=0u;
    start.start_cleanup_4299c0_ids={};
    start.start_cleanup_4299c0_count=0u;
    start.start_loader_mode_7d34c0=0u;
    start.start_loader_mode_writes=0u;
    start.scene_owner_stage26_requests=0u;
    start.scene_owner_stage28_requests=0u;
    start.scene_owner_stage24_car_id=0u;
    // 0x8421C0 belongs to the PC scene-owner module, not to START_Init.
    // Preserve its value/knowledge across a START mode transition.
    start.scene_owner_course_marker=-1;
    // PC START_Init calls 49FA60 at 49DC40, before installing event1.
    // The queue is shared with the frontend, not reset by copying a new owner.
    if(!vehicle_creation_reset_49fa60(context.vehicle_creation)){
        start.last_missing_service=context.vehicle_creation.fault;start.active=false;return;
    }
    ++start.init_calls;
    // START_Init 0x49DB20 installs the race-start owner before loading the
    // course.  Other initialization calls remain unported service boundaries.
    driving::event_setup_440110(context.event_state,1u,0x1cu,
                                context.event_descriptors,context.event_functions);
    ++start.event_open_count;
    const auto variant=context.game_mode.game_variant;
    start.mode_countdown_780250=
        (variant==3u||variant==4u)?0xb4:0x78;
    std::uint32_t extra_event=0u,extra_function=0u;
    if(variant==4u||variant==6u){extra_event=0x191u;extra_function=0x20u;}
    else if(variant==9u){extra_event=0x192u;extra_function=0x21u;start.scene_state_8421c0_known=false;}
    else if(variant==7u){extra_event=0x193u;extra_function=0x22u;}
    else if(variant==8u){extra_event=0x194u;extra_function=0x23u;}
    if(extra_event!=0u){
        driving::event_setup_440110(context.event_state,extra_event,extra_function,
                                    context.event_descriptors,context.event_functions);
        ++start.event_open_count;
    }
    // 49DB79..49DBDF (a SUMO_FE game, [780260]): 46C2B0(variant == 5) sets the C2C request
    // manager's +60 (7F94C0, 55A930); when set, 46C2D0(-1) (variant 5, 482B90) resets it.
    if(start.frontend_prepare.game_flag_780260){
        start.manager_state_7f94c0=variant==5u?1u:0u;
        if(start.manager_state_7f94c0){
            std::uint32_t eax{};
            if(!requests_call(context,0x46c2d0u,{0xffffffffu},eax)){start.last_missing_service=0x46c2d0u;start.active=false;return;}
        }
    }
    context.game_mode.start_countdown=0x168;
    // 49DC5E: 44C0B0 copies the selected course record 7D2DE0 over the
    // current one 7D30A8 (0x1E dwords): a RETRY (mode 30) starts again from
    // the first stage, not from the stage the last race reached.
    {auto& rt=context.game_mode.course_runtime;rt.selected_7d30a8=rt.selected_7d2de0;}
    // 49DCF8: word 8367F0 = 456D60 (players), read by the START display 499A30.
    {const std::uint16_t players=context.race.car_world.commrace_7de418[0];
     std::memcpy(native_race_end(context).state.players_8367f0.data(),&players,2);}
    // ---- race manager binding: START_Init 49DD0D calls 44C020 (time table 7D2E98 of preset [78024C]): begin ----
    race_time_table_44c020(start.course_preset,native_race_area(context).area.block.data()+(0x7d2e98u-PcRaceAreaState::BlockBase));
    // ---- race manager binding: end ----
}

void native_start_mode_control(NativeRuntimeContext& context,
                               const NativeModeServices& services){
    auto& start=context.start_mode;
    if(!start.active)return;
    ++start.control_calls;
    const auto call=[&](std::uint32_t pc,std::initializer_list<std::uint32_t> args,
                        std::uint32_t& result)->bool{
        if(services.start_call&&services.start_call(
               services.user,pc,args.begin(),args.size(),result))return true;
        start.last_missing_service=pc;
        ++start.gate_waits;
        return false;
    };
    std::uint32_t result{};
    // The PC falls through stages 0, 1 and 2 within one control call.  A
    // missing owned file or an unported readiness service stops here rather
    // than manufacturing a transition to GAME.
    if(start.stage==0u){
        const auto variant=context.game_mode.game_variant;
        if(variant<=2u){
            std::uint32_t mode=0u;
            std::uint8_t alternate=1u;
            switch(start.course_preset){
                case 0u:mode=0u;alternate=1u;break;
                case 1u:mode=1u;alternate=1u;break;
                case 2u:mode=5u;alternate=1u;break;
                case 3u:mode=6u;alternate=1u;break;
                default:++start.gate_waits;return;
            }
            ++start.course_load_attempts;
            const auto success_before=context.game_mode.course_load_success;
            native_game_load_course(context.game_mode,mode,alternate);
            if(context.game_mode.course_load_success==success_before){
                ++start.gate_waits;return;
            }
            ++start.course_load_success;
        }
        start.stage=1u;
    }
    if(start.stage==1u){
        start.resource_id=start.use_versus_resource?
            LoaderAssetStartVersusId:LoaderAssetStartLoadingId;
        ++start.resource_requests;
        start.stage=2u;
    }
    if(start.stage==2u){
        const auto* asset=runtime_asset(context.event_function36,start.resource_id,2u);
        if(!asset||asset->bytes.empty()){
            ++start.gate_waits;return;
        }
        if(start.resource_ready==0u){
            start.resource_bytes=static_cast<std::uint32_t>(asset->bytes.size());
            ++start.resource_ready;
        }
        // 0x42DF90 and 0x4299A0 are distinct PC readiness gates; owning
        // their file bytes alone is insufficient to declare either complete.
        if(!services.start_ready||
           !services.start_ready(services.user,0x0042df90u)||
           !services.start_ready(services.user,0x004299a0u)){
            ++start.gate_waits;return;
        }
        native_race_end(context).state.frames_836ce4.fill(0u);   // 49DE3C
        driving::event_setup_440110(context.event_state,0x185u,0x0fu,
                                    context.event_descriptors,context.event_functions);
        ++start.event_open_count;
        // 49DE4A -> 499CC0: outside the 2SP game (780260) and the LAN variants 3..5, the
        // loading picture of the course preset (2F0002 / 2F0003 / 2F0000 / 2F0001), a 428460
        // range sprite (layer 2, mode 0, frames 0..0x77) of the loading bank 0x2F.
        {
            static constexpr std::uint32_t Picture[4]{0x2f0002u,0x2f0003u,0x2f0000u,0x2f0001u};
            const auto v=context.game_mode.game_variant;
            if(!start.frontend_prepare.game_flag_780260&&v<=9u&&!(v>=3u&&v<=5u)){
                if(start.course_preset<4u){
                    const auto token=Picture[start.course_preset];
                    auto& pool=context.event_function36.frontend_sprites;
                    FrontendSpriteTiming timing{};std::string err;
                    if(!pool.bank_scene(token,timing)&&!native_sprani_bind_bank(context,token>>16,err))
                        outrun::driving::service_hole("START 499CC0","the loading bank 0x2F (428460 picture)");
                    else (void)pool.create(token,2u,0u,0,0x77,true,context.event_function36.title_pause_flag_95b214);
                }else outrun::driving::service_hole("START 499CC0","preset above 3 (the PC pushes an undefined token)");
            }
        }
        start.stage=3u;
    }
    if(start.stage==3u){
        // PC 0x49DE65 reads the loading-scene byte at 0x7DE418.  For the
        // ordinary variant (1) values 0/1 advance immediately; variant 4
        // retains its special counter branch.
        if(!call(0x00456d60u,{},result))return;
        if(result>1u||context.game_mode.game_variant==4u){
            const auto previous=start.stage3_counter++;
            if(previous==0x78u)return;
        }
        if(!call(0x0055a930u,{},result))return;
        start.stage=4u;
        if(result!=0u){
            if(!call(0x0046c2c0u,{0u},result)||
               !call(0x00448ad0u,{0x1eeu,7u},result))return;
            start.pending_1ee=1u;
        }
        return; // the stage-4 dispatch is on the next PC control tick
    }
    if(start.stage==4u){
        if(!call(0x00427db0u,{},result))return;
        start.stage=5u;
        // The PC falls through to stage 5 in this same call.
    }
    if(start.stage==5u){
        if(!call(0x0055a930u,{},result))return;
        if(result!=0u){
            if(!call(0x00448960u,{0x1eeu},result))return;
            if(result==0u){++start.gate_waits;return;}
        }
        for(const auto pc:{0x0046c2f0u,0x0046c240u,0x0046c4d0u}){
            if(!call(0x0055a930u,{},result))return;
            if(result==0u)continue;
            if(!call(pc,{},result))return;
            if(result==0u){++start.gate_waits;return;}
        }
        if(!call(0x0049ba80u,{},result))return;
        if(result==0u){++start.gate_waits;return;}
        driving::event_close_4401d0(context.event_state,0x17fu);
        // 49DF67 cmp [780258],esi: ESI is 5 in stage 5 (49DD62 / 49DECE), the area's variant-5
        // 46C380 / 4F03A0 pair (44E940 tests the same variant).
        if(context.game_mode.game_variant==5u){
            if(!call(0x0046c380u,{},result))return;
            if(result!=0u){++start.gate_waits;return;}
            if(!call(0x004f03a0u,{0u},result))return;
        }
        if(start.use_versus_resource){
            if(!call(0x00459210u,{},result)||
               !call(0x004f5360u,{},result))return;
        }
        start.stage=59u;
        return;
    }
    if(start.stage==59u){
        if(start.use_versus_resource){
            if(!call(0x004f53b0u,{},result))return;
            if(result==0u){
                if(!call(0x004f53a0u,{},result))return;
                if(result==0u){++start.gate_waits;return;}
            }
        }
        if(!call(0x004557f0u,{},result))return;
        if(result==0u){++start.gate_waits;return;}
        if(start.use_versus_resource&&!call(0x004f5150u,{},result))return;
        start.stage=60u;
        // The PC falls through to stage 60 after the last readiness gate.
    }
    if(start.stage==60u){
        if(start.stage60_phase==0u){
            if(!call(0x00440380u,{},result))return;
            const auto variant=context.game_mode.game_variant;
            if(variant==0u){
                if(!call(0x00440870u,{},result))return;
            }else if(variant==7u){
                if(!call(0x00440880u,{},result))return;
            }else{
                if(!call(0x004962a0u,{},result))return;
                if(result!=0u&&!call(0x00440880u,{},result))return;
            }
            if(!call(0x004f0d10u,{0u},result)||
               !call(0x004f0d10u,{1u},result))return;
            constexpr std::array<std::array<std::uint32_t,2>,9> first{{
                {{0x167u,0x32u}},{{0x181u,0x0du}},{{0x186u,0x43u}},
                {{0x187u,0x14u}},{{0x18du,0x1bu}},{{0x184u,0x0eu}},
                {{0x17fu,0x17u}},{{0x183u,0x34u}},{{0x16au,0x37u}}
            }};
            for(const auto& entry:first){
                driving::event_setup_440110(context.event_state,entry[0],entry[1],
                                            context.event_descriptors,context.event_functions);
                ++start.race_event_count;
            }
            start.stage60_phase=1u;
        }
        if(start.stage60_phase==1u){
            bool include_16b=false;
            if(!call(0x004962d0u,{},result))return;
            if(result!=0u)include_16b=true;
            else{
                if(!call(0x004999a0u,{context.game_mode.game_variant},result))return;
                if(result==0u){
                    bool suppressed=false;
                    for(const auto pc:{0x004957f0u,0x0048b310u,0x00495490u}){
                        if(!call(pc,{},result))return;
                        if(result!=0u){suppressed=true;break;}
                    }
                    include_16b=!suppressed;
                }
            }
            if(!call(0x0055a930u,{},result))return;
            const bool include_190=result!=0u;
            const auto open=[&](std::uint32_t event,std::uint32_t function){
                driving::event_setup_440110(context.event_state,event,function,
                                            context.event_descriptors,context.event_functions);
                ++start.race_event_count;
            };
            if(include_16b)open(0x16bu,0x39u);
            if(context.game_mode.game_variant==3u||context.game_mode.game_variant==4u)
                open(0x18au,0x3cu);
            open(0x16cu,0x3bu);
            if(include_190)open(0x190u,0x1fu);
            start.stage60_phase=2u;
        }
        if(start.stage60_phase==2u){
            if(!call(0x004518a0u,{},result))return;
            start.stage60_phase=3u;
        }
        if(start.stage60_phase==3u){
            if(!call(0x00456d60u,{},result))return;
            std::uint32_t route=10u;
            if(result<=1u){
                if(!call(0x0048b140u,{},result))return;
                result&=0xffu;
                constexpr std::array<std::uint8_t,30> routes{{
                    11,12,14,11,13,14,13,12,11,11,11,12,11,11,11,
                    11,12,14,11,13,14,13,12,11,11,11,12,11,11,11
                }};
                if(result>=routes.size()){
                    ++start.gate_waits;return;
                }
                route=routes[result];
                if(route==12u||route==13u){
                    if(!call(0x0044c2d0u,{},result))return;
                    if(result!=0u&&result!=15u)route=11u;
                }
            }
            if(!call(0x004871a0u,{route},result))return;
            start.stage60_route=route;
            start.stage=64u;
            return;
        }
    }
    if(start.stage==64u){
        if(start.stage64_cleanup_step==0u){
            if(!call(0x0045a920u,{},result))return;
            if(result==0u){++start.gate_waits;return;}
            start.stage64_cleanup_step=1u;
        }
        constexpr std::array<std::uint32_t,2> pre_cleanup{{
            0x004999f0u,0x00428600u
        }};
        while(start.stage64_cleanup_step<=8u){
            const auto step=start.stage64_cleanup_step;
            std::uint32_t pc=0u,arg=0u;
            if(step<=2u)pc=pre_cleanup[step-1u];
            else if(step==3u){pc=0x0042dfb0u;arg=0x15u;}
            else if(step==4u){pc=0x0042dfb0u;arg=0x2fu;}
            else if(step==5u){pc=0x004299c0u;arg=0x2fu;}
            else if(step==6u){pc=0x0042dfb0u;arg=0x49u;}
            else if(step==7u){pc=0x004299c0u;arg=0x49u;}
            else{start.stage=65u;break;}
            if(step<=2u){if(!call(pc,{},result))return;}
            else if(!call(pc,{arg},result))return;
            ++start.stage64_cleanup_step;
        }
    }
    if(start.stage==65u&&start.game_requests==0u){
        if(!call(0x0043fa90u,{},result))return;
        const bool game_ready=result!=0u;
        // These boundaries still require real owners before a production
        // START can request GAME. The test harness supplies them explicitly.
        if(game_ready&&
           (context.game_mode.game_variant==3u||context.game_mode.game_variant==4u)&&
           !call(0x004edce0u,{1u},result))return;
        if(!call(0x0043f9c0u,{},result))return;
        const bool timer_held=result!=0u;
        if(!call(0x0044fce0u,{0u},result))return;
        if(!call(0x0043f980u,{},result))return;
        const bool pause_requested=result!=0u;
        if(game_ready){
            if(!native_runtime_request_mode(context,16u)){
                ++start.gate_waits;return;
            }
            context.game_mode.start_countdown=0xb4;
            ++start.game_requests;
        }else ++start.gate_waits;
        if(!timer_held)--context.game_mode.start_countdown;
        if(pause_requested){
            if(!native_runtime_request_mode(context,29u))++start.gate_waits;
        }
    }
}

void native_start_mode_exit(NativeRuntimeContext& context){
    ++context.start_mode.exit_calls;
    context.start_mode.active=false;
}


bool initialize_direct_gameplay(NativeGameModeState& game,const CourseWorldRuntime& source){
    game.gameplay_ready=false;
    game.gameplay_world_from_start=false;
    game.gameplay_direct_fallback=false;
    game.gameplay_frames=0u;
    game.gameplay_ground_queries=0u;
    game.gameplay_ground_hits=0u;
    game.gameplay_ground_rejects=0u;
    game.gameplay_sweep_poses=0u;
    game.gameplay_spawn_attempts=0u;
    game.gameplay_last_quad=0u;
    game.gameplay_yaw=0.0f;
    game.gameplay_speed=0.0f;
    game.gameplay_steering_normalized=0.0f;
    game.gameplay_yaw_step=0.0f;
    game.gameplay_steering_peak=0.0f;
    game.gameplay_yaw_step_peak=0.0f;
    game.gameplay_chase_height=3.3f;
    game.gameplay_pose_filter=VehiclePoseFilterState{};
    game.gameplay_vehicle=PcVehicleControlState{};
    game.gameplay_vehicle_output=PcVehicleControlOutput{};
    if(!game.driving_data||!source.query_ready())return false;
    game.gameplay_world=source;
    // Legacy direct-course diagnostic only, not the original event constructor.
    if(!configure_pc_vehicle_control(game.gameplay_vehicle,*game.driving_data,DrivingPackV1LegacyCarId,0))return false;
    if(!game.gameplay_world.lane_loaded(0u))return false;
    const auto& lane=game.gameplay_world.lane(0u);
    const auto count=std::min<std::size_t>(lane.pc_layout.primary_polygon_count,lane.quads.size());
    constexpr std::array<float,4> Yaws{{0.0f,1.57079632679f,-1.57079632679f,3.14159265359f}};
    VehicleRoadContactFrame contacts{};
    bool found=false;
    for(std::size_t i=0u;i<count&&!found;++i){
        const auto& q=lane.quads[i];
        for(float yaw:Yaws){
            ++game.gameplay_spawn_attempts;
            std::array<float,3> pos{{q.center_xz[0],q.vertices[0][1]+0.5f,q.center_xz[1]}};
            if(!sample_vehicle_road_contacts(game.gameplay_world,pos,yaw,contacts)||contacts.hit_count!=4u)
                continue;
            game.gameplay_position=pos;
            game.gameplay_position[1]=contacts.ground_height+0.02f;
            game.gameplay_ground_normal=contacts.normal;
            game.gameplay_yaw=yaw;
            game.gameplay_spawn_quad=contacts.primary_quad;
            game.gameplay_last_quad=contacts.primary_quad;
            found=true;
            break;
        }
    }
    if(!found)return false;
    if(!initialize_vehicle_pose_filter(game.gameplay_pose_filter,contacts.ground_height,contacts.normal))return false;
    set_pc_vehicle_road_contacts(game.gameplay_vehicle,contacts);
    set_pc_vehicle_world_pose(game.gameplay_vehicle,game.gameplay_position,game.gameplay_yaw,true);
    game.gameplay_vehicle_output=pc_vehicle_control_output(game.gameplay_vehicle);
    const float sx=std::sin(game.gameplay_yaw),cz=std::cos(game.gameplay_yaw);
    const float fx=sx,fz=-cz;
    const auto& up=game.gameplay_ground_normal;
    game.gameplay_camera_eye={{
        game.gameplay_position[0]-fx*7.0f+up[0]*game.gameplay_chase_height,
        game.gameplay_position[1]+up[1]*game.gameplay_chase_height,
        game.gameplay_position[2]-fz*7.0f+up[2]*game.gameplay_chase_height,
    }};
    game.gameplay_camera_target={{
        game.gameplay_position[0]+fx*10.0f+up[0]*0.9f,
        game.gameplay_position[1]+up[1]*0.9f,
        game.gameplay_position[2]+fz*10.0f+up[2]*0.9f,
    }};
    game.gameplay_direct_fallback=true;
    game.gameplay_ready=true;
    return true;
}

void step_native_gameplay(NativeRuntimeContext& context){
    auto& game=context.game_mode;
    if(!game.gameplay_ready)return;
    const auto& in=context.input_state;
    const float old_yaw=game.gameplay_yaw;
    const auto old_position=game.gameplay_position;
    game.gameplay_vehicle_output=step_pc_vehicle_control(game.gameplay_vehicle,
        {in.steering,in.accelerator,in.brake,in.shift_up,in.shift_down});
    game.gameplay_speed=game.gameplay_vehicle_output.linear_speed;
    game.gameplay_steering_normalized=normalized_pc_vehicle_steering(game.gameplay_vehicle_output);
    game.gameplay_yaw_step=pc_vehicle_yaw_step(game.gameplay_vehicle_output);
    game.gameplay_steering_peak=std::max(game.gameplay_steering_peak,
        std::fabs(game.gameplay_steering_normalized));
    game.gameplay_yaw_step_peak=std::max(game.gameplay_yaw_step_peak,
        std::fabs(game.gameplay_yaw_step));
    if(game.gameplay_vehicle_output.chassis_active){
        game.gameplay_position=game.gameplay_vehicle_output.body_position;
        game.gameplay_yaw=game.gameplay_vehicle_output.body_yaw;
        // The synthetic yaw metric remains an observability field only once
        // ActionForce2 owns the actual world orientation.
        game.gameplay_yaw_step=game.gameplay_yaw-old_yaw;
    }else{
        game.gameplay_yaw+=game.gameplay_yaw_step;
        const float fx=std::sin(game.gameplay_yaw),fz=-std::cos(game.gameplay_yaw);
        game.gameplay_position[0]+=fx*game.gameplay_speed;
        game.gameplay_position[2]+=fz*game.gameplay_speed;
    }
    game.gameplay_chase_height=std::clamp(
        game.gameplay_chase_height+float(in.right_stick_y)/32768.0f*0.025f,2.0f,8.0f);
    VehicleRoadContactFrame contacts{};
    VehicleRoadSweepStats sweep{};
    if(sample_vehicle_road_contacts_swept(game.gameplay_world,old_position,old_yaw,
                                          game.gameplay_position,game.gameplay_yaw,
                                          contacts,sweep)){
        game.gameplay_ground_queries+=sweep.wheel_queries;
        game.gameplay_ground_hits+=sweep.wheel_hits;
        game.gameplay_ground_rejects+=sweep.wheel_queries-sweep.wheel_hits;
        game.gameplay_sweep_poses+=sweep.sampled_poses;
        game.gameplay_last_quad=contacts.primary_quad;
        if(update_vehicle_pose_filter(game.gameplay_pose_filter,contacts.ground_height,contacts.normal)){
            game.gameplay_position[1]=game.gameplay_pose_filter.ground_height+0.02f;
            game.gameplay_ground_normal=game.gameplay_pose_filter.normal;
            if(game.gameplay_vehicle_output.chassis_active)
                set_pc_vehicle_world_position(game.gameplay_vehicle,game.gameplay_position);
            set_pc_vehicle_road_contacts(game.gameplay_vehicle,contacts);
            game.gameplay_vehicle_output=pc_vehicle_control_output(game.gameplay_vehicle);
        }else{
            game.gameplay_position=old_position;
            game.gameplay_yaw=old_yaw;
            if(game.gameplay_vehicle_output.chassis_active)
                set_pc_vehicle_world_pose(game.gameplay_vehicle,old_position,old_yaw,false);
            reject_pc_vehicle_motion(game.gameplay_vehicle);
            game.gameplay_vehicle_output=pc_vehicle_control_output(game.gameplay_vehicle);
            game.gameplay_speed=game.gameplay_vehicle_output.linear_speed;
        }
    }else{
        game.gameplay_ground_queries+=sweep.wheel_queries;
        game.gameplay_ground_hits+=sweep.wheel_hits;
        game.gameplay_ground_rejects+=sweep.wheel_queries-sweep.wheel_hits;
        game.gameplay_sweep_poses+=sweep.sampled_poses;
        game.gameplay_position=old_position;
        game.gameplay_yaw=old_yaw;
        if(game.gameplay_vehicle_output.chassis_active)
            set_pc_vehicle_world_pose(game.gameplay_vehicle,old_position,old_yaw,false);
        reject_pc_vehicle_motion(game.gameplay_vehicle);
        game.gameplay_vehicle_output=pc_vehicle_control_output(game.gameplay_vehicle);
        game.gameplay_speed=game.gameplay_vehicle_output.linear_speed;
    }
    const float cfx=std::sin(game.gameplay_yaw),cfz=-std::cos(game.gameplay_yaw);
    const auto& up=game.gameplay_ground_normal;
    game.gameplay_camera_eye={{
        game.gameplay_position[0]-cfx*7.0f+up[0]*game.gameplay_chase_height,
        game.gameplay_position[1]+up[1]*game.gameplay_chase_height,
        game.gameplay_position[2]-cfz*7.0f+up[2]*game.gameplay_chase_height,
    }};
    game.gameplay_camera_target={{
        game.gameplay_position[0]+cfx*10.0f+up[0]*0.9f,
        game.gameplay_position[1]+up[1]*0.9f,
        game.gameplay_position[2]+cfz*10.0f+up[2]*0.9f,
    }};
    ++game.gameplay_frames;
}

void native_game_mode_init(NativeRuntimeContext& context){
    auto& game=context.game_mode;
    game.transition_latched=0u;
    game.previous_mode=context.mode_state.previous;
    game.active=true;
    ++game.init_calls;

    // PC 0x499D97: only RESTART -> GAME resumes the complete gameplay event
    // groups here.  Other predecessors take the empty 0x440D90 boundary.
    if(game.previous_mode==15u){
        constexpr std::array<std::array<std::uint32_t,2>,9> ranges{{
            {{0x17fu,1u}},{{0x008u,0x18u}},{{0x18du,1u}},
            {{0x186u,1u}},{{0x187u,1u}},{{0x182u,1u}},
            {{0x020u,0xfau}},{{0x11au,0x1au}},{{0x16au,0x15u}}
        }};
        for(const auto& range:ranges){
            driving::event_resume_440a30(context.event_state,range[0],range[1]);
            ++game.event_resume_calls;
            game.event_resume_slots+=range[1];
        }
    }else ++game.empty_440d90_calls;

    // PC 0x499E0D..0x499E42.  The global effect and timing reset are retained
    // as explicit, observable boundaries until their owning systems land.
    if(game.game_variant!=4u&&game.game_variant!=6u){
        ++game.global_effect_calls;
        game.global_effect_last=0x82u;
    }
    if(game.previous_mode!=17u&&game.previous_mode!=18u)
        ++game.timing_reset_calls;
    // Back from the pause (18) or 17: the PC init ends here (0x499E47); the race, its
    // course and world stay as they were.
    if(game.previous_mode==17u||game.previous_mode==18u)return;

    // The diagnostic SUMO_FE -> GAME path still loads here.  The original
    // START -> GAME path reuses the course established in START stage 0.
    if(game.previous_mode!=13u||game.course_load_success==0u)
        native_game_load_course(game);

    // The natural START path owns a selected/transformed course world, but its
    // exact original grid spawn is not reconstructed yet. Keep it fail-closed.
    // The explicit SUMO_FE -> GAME diagnostic may use the attached BEAC world
    // and a deterministic road-supported spawn so GAME itself can be tested.
    game.gameplay_ready=false;
    game.gameplay_world_from_start=false;
    game.gameplay_direct_fallback=false;
    if(game.previous_mode==13u&&context.start_mode.scene_owner_course_world.query_ready())
        game.gameplay_world_from_start=true;
    else if(game.gameplay_fallback_world_ready)
        (void)initialize_direct_gameplay(game,game.gameplay_fallback_world);
}

// PC 0x49C840 (GAME control): start countdown 8367BC, time-out 836CEC,
// then the owner command / menu-state mode requests.
void native_game_mode_control(NativeRuntimeContext& context){
    auto& game=context.game_mode;
    if(!game.active)return;
    ++game.control_calls;
    auto& race=context.race;
    if(game.start_countdown>0){
        if(context.start_mode.game_flag_780248==0u)--game.start_countdown;       // 43F9C0
        if(game.start_countdown==0)
            game.timeout_countdown=(game.game_variant==6u||game.game_variant==4u)?0x1a4:0x78;
    }else if(game.timeout_countdown>0){
        if(--game.timeout_countdown==0){
            if(race.area_state_7d2e88!=0x14u)game.timeout_countdown=1;          // 44BE40
            else{
                race.sound_commands.push_back(0x207u);context.pc_sound.unload_4278c0(0x207u);   // 4278C0
                if(game.game_variant==2u)race.area_state_7d2e80=0x16u;          // 44B9C0
                else if(game.game_variant==5u)race.area_state_7d2e80=0x17u;     // 44B9D0
                else if(game.game_variant==6u||game.game_variant==4u){
                    race.sound_commands.push_back(0x869bu);context.pc_sound.unload_4278c0(0x869bu);race.area_state_7d2e80=0x19u; // 44B9E0 (0x19: TRC.PAK)
                }else race.area_state_7d2e80=0x18u;                              // 44B9F0
            }
        }
    }
    // 0x49C902: 440DC0 on the active owner (4035F0), then 450240 (7D38F0).
    ++game.owner_command_polls;
    std::uint32_t command=0xffffffffu;
    if(context.event_function36.initialized){
        auto owner=driving::Bytes(context.event_function36.object.data(),context.event_function36.object.size());
        command=driving::object_take_token_440dc0(owner);
    }
    game.last_owner_command=command;
    if(command!=0xffffffffu){
        game.transition_latched=1u;                                             // 836CE8
        if(command<NativeModeCount)(void)native_runtime_request_mode(context,command);
        return;
    }
    ++game.menu_state_polls;
    static constexpr std::uint32_t Requests[5]{0x13u,0x14u,0x13u,0x16u,0x15u}; // 49C960
    const std::uint32_t state=race_over_state_450240(context.race.manager.state);   // race manager binding: 450240 (7D38F0)
    if(state>=1u&&state<=5u)(void)native_runtime_request_mode(context,Requests[state-1u]);
}

void native_game_mode_exit(NativeRuntimeContext& context){
    auto& game=context.game_mode;
    ++game.exit_calls;
    game.active=false;
}
}

bool native_start_owned_resource_ready(const NativeRuntimeContext& context,
                                       std::uint32_t pc_entry){
    if(pc_entry!=0x0042df90u&&pc_entry!=0x004299a0u)return false;
    const auto& frontend=context.event_function36;
    const auto& start=context.start_mode;
    const auto* asset=runtime_asset(frontend,start.resource_id,2u);
    if(!asset||asset->bytes.empty()||start.resource_ready==0u||
       start.resource_bytes!=asset->bytes.size())return false;
    // Both PC pools have already reached their terminal states during the
    // owner-405 frontend bootstrap. START's new resource is held in the same
    // synchronous native pack, so no pending platform read remains.
    const bool base_ready=frontend.loader_stage14_passes!=0u&&
        frontend.shared_loader.stage_83db18==6u&&
        frontend.shared_resource_pending==0u&&
        frontend.frontend_resource_pending==0u&&
        frontend.frontend_bulk_pending==0u;
    if(!base_ready)return false;
    // 4299A0: every sprite bank entry idle (state 2). The SUMO_FE bank is
    // either resident or released by 429A10/42DFD0 after a race (RETRY goes
    // back to START without reloading it): no request in flight either way.
    return pc_entry==0x0042df90u?
        frontend.shared_ready_count==frontend.shared_resource_ready.size():
        frontend.frontend_resource_pending==0u;
}

namespace {
// FXT 0x5C6B88, indexed by the 0x48B140 course/car selection byte. The
// 0x4BABE0 caller does not bound the lookup; only the 30 authored entries
// are accepted here. Entries 10..29 deliberately repeat the F50 resource.
constexpr std::array<std::uint32_t,30> SceneMeterIds{{
    0x28u,0x23u,0x24u,0x30u,0x26u,0x29u,0x2au,0x25u,0x27u,0x31u,
    0x28u,0x28u,0x28u,0x28u,0x28u,0x28u,0x28u,0x28u,0x28u,0x28u,
    0x28u,0x28u,0x28u,0x28u,0x28u,0x28u,0x28u,0x28u,0x28u,0x28u}};

bool scene_owner_request(NativeRuntimeContext& context,std::uint32_t id,
                         std::uint32_t mode){
    auto& start=context.start_mode;
    const auto* asset=runtime_asset(context.event_function36,id,mode);
    if(!asset||asset->bytes.empty()||
       start.scene_owner_resource_count>=start.scene_owner_resource_ids.size()){
        start.scene_owner_missing_service=0x0049b390u;
        return false;
    }
    const auto i=start.scene_owner_resource_count++;
    start.scene_owner_resource_ids[i]=id;
    start.scene_owner_resource_modes[i]=mode;
    start.scene_owner_resource_bytes+=static_cast<std::uint32_t>(asset->bytes.size());
    return true;
}

bool scene_owner_resources_ready(const NativeRuntimeContext& context){
    const auto& start=context.start_mode;
    if((!context.event_function36.loader_assets&&!context.event_function36.retail_assets)||
       start.scene_owner_resource_count==0u)return false;
    for(std::size_t i=0;i<start.scene_owner_resource_count;++i){
        const auto* asset=runtime_asset(context.event_function36,start.scene_owner_resource_ids[i],
                                                  start.scene_owner_resource_modes[i]);
        if(!asset||asset->bytes.empty())return false;
    }
    return true;
}

bool motion_group_descriptor_valid(const LoaderAssetRecord& table,
                                   std::size_t group,const char* expected,
                                   std::uint32_t expected_count){
    const auto& b=table.bytes;
    const auto off=group*16u;
    if(b.size()!=15360u||off+16u>b.size())return false;
    const auto name=le32(b.data()+off);
    const auto list=le32(b.data()+off+4u);
    const auto count=le32(b.data()+off+8u);
    const auto length=std::strlen(expected);
    if(count!=expected_count||le32(b.data()+off+12u)!=0u||
       name>b.size()||length+1u>b.size()-name||
       std::memcmp(b.data()+name,expected,length+1u)!=0||
       list>b.size()||count*4u>b.size()-list)return false;
    for(std::uint32_t i=0u;i<count;++i){
        const auto item=le32(b.data()+list+i*4u);
        if(item>=b.size()||!std::memchr(b.data()+item,0,b.size()-item))return false;
    }
    return true;
}
// Same check with the group's own name/count read from the 4F2020 table.
bool scene_owner_motion_group_ready(const NativeRuntimeContext& context,
                                    std::uint32_t group,const char* name,
                                    std::uint32_t count,std::uint32_t& bytes);
bool scene_owner_motion_group_ready_table(const NativeRuntimeContext& context,
                                          std::uint32_t group,std::uint32_t& bytes){
    const auto* table=runtime_asset(context.event_function36,LoaderAssetMotionTableId,0u);
    if(!table||table->bytes.size()!=15360u||group*16u+16u>table->bytes.size())return false;
    const auto& b=table->bytes;const auto name=le32(b.data()+group*16u);
    if(name>=b.size()||!std::memchr(b.data()+name,0,b.size()-name))return false;
    const std::string text(reinterpret_cast<const char*>(b.data()+name));
    return scene_owner_motion_group_ready(context,group,text.c_str(),le32(b.data()+group*16u+8u),bytes);
}
bool scene_owner_motion_group_ready(const NativeRuntimeContext& context,
                                    std::uint32_t group,const char* name,
                                    std::uint32_t count,std::uint32_t& bytes){
    const auto* table=runtime_asset(context.event_function36,LoaderAssetMotionTableId,0u);
    const auto* data=runtime_asset(context.event_function36,loader_motion_group_id(group),0u);
    if(!table||!data||!motion_group_descriptor_valid(*table,group,name,count)||
       data->bytes.size()<8u||data->bytes.size()>1024u*1024u||
       le32(data->bytes.data())!=data->bytes.size()-4u)return false;
    bytes=static_cast<std::uint32_t>(data->bytes.size());
    return true;
}
}

static bool stage17_target_path_44c680(std::uint32_t course_marker,
                                      std::uint32_t lane,
                                      std::uint32_t family,
                                      std::uint32_t index,
                                      std::string& path){
    // Exact string composition used by 0x44C680 for markers 0..59.  The
    // retail BEAC marker is 15: region 0 (_cvt), section 0 (_1a).  START's
    // ordinary 0x46FE50 call uses lane 0 (_stg), family 0 (_othcar), and
    // indexes 0..11. Keep the full mapping so later lanes can reuse it.
    static constexpr const char* region[4]={"_cvt","_old","_cvr","_olr"};
    static constexpr const char* lane_name[2]={"_stg","_bra"};
    static constexpr const char* section[15]={
        "_1a","_2a","_2b","_3a","_3b","_3c","_4a","_4b","_4c","_4d",
        "_5a","_5b","_5c","_5d","_5e"};
    static constexpr const char* family_name[2]={"_othcar","_rvlcar"};
    static constexpr const char* number[12]={
        "_01","_02","_03","_04","_05","_06","_07","_08","_09","_10","_11","_12"};
    if(course_marker>=60u||lane>=2u||family>=2u||index>=12u)return false;
    std::uint32_t r=1u,section_index=course_marker;
    if(course_marker>=45u){r=2u;section_index=course_marker-45u;}
    else if(course_marker>=30u){r=3u;section_index=course_marker-30u;}
    else if(course_marker>=15u){r=0u;section_index=course_marker-15u;}
    if(section_index>=15u)return false;
    path="\\OCP\\ocp";
    path+=region[r];path+=lane_name[lane];path+=section[section_index];
    path+=family_name[family];path+=number[index];path+="_tgt.sz";
    return true;
}

// ---- 49BA80 services (scene_owner_49ba80.hpp) -------------------------------
// Each case is one PC callee with its original return contract. A callee
// whose original behaviour is not available natively latches
// scene_owner_fault: the owner then stops advancing (no invented success).
namespace {
const PcAddressView& exe_view(){
    static const PcAddressView view=[]{PcAddressView v;v.add_exe();return v;}();
    return view;
}
// 7D30BC / 7D30C0: the selected course copy's primary and secondary descriptor tokens.
std::uint32_t selected_primary(const NativeRuntimeContext& c){return le32(c.game_mode.course_runtime.selected_7d30a8.data()+0x14u);}
std::uint32_t selected_secondary(const NativeRuntimeContext& c){return le32(c.game_mode.course_runtime.selected_7d30a8.data()+0x18u);}
bool read_descriptor(std::uint32_t token,std::uint32_t offset,std::uint32_t& value){
    if(token==0u)return false;
    return exe_view().u32(token+offset,value);
}
// 44C220(n): primary/secondary descriptor field (the measured dispatch table).
bool course_field(const NativeRuntimeContext& c,std::uint32_t n,std::uint32_t& value){
    static constexpr std::array<std::uint8_t,16> offsets{{0x0c,0x0c,0x18,0x1c,0x20,0x24,0x28,0x2c,0x30,0x30,0x34,0x34,0x64,0x68,0x68,0x6c}};
    static constexpr std::array<bool,16> secondary{{false,true,true,true,false,false,false,false,false,true,false,true,false,false,true,false}};
    if(n>=16u){value=0u;return true;}
    return read_descriptor(secondary[n]?selected_secondary(c):selected_primary(c),offsets[n],value);
}
// 44DBB0(n) (jump table 44DC14); 12/13 go through 44C9A0/44C9C0 (not bound).
bool course_world_id(const NativeRuntimeContext& c,std::uint32_t n,std::uint32_t& value){
    const auto p=selected_primary(c),s=selected_secondary(c);
    switch(n){
    case 0:return read_descriptor(p,4,value);
    case 1:return read_descriptor(s,4,value);
    case 2:return read_descriptor(p,0x38,value);
    case 3:return read_descriptor(s,0x38,value);
    case 6:return read_descriptor(p,0x60,value);
    case 7:return read_descriptor(s,0x60,value);
    case 10:return read_descriptor(p,0x44,value);
    case 11:return read_descriptor(p,0,value);
    case 12:case 13:return false;
    default:value=0u;return true;
    }
}
bool scene_owner_classified_4999a0(std::uint32_t variant){return variant==0u||variant==7u||variant==8u;}
bool scene_owner_mission_type4_4962a0(const NativeRuntimeContext& c);
}
// 44DBB0(n) for the race end modes (race_end_runtime): 12/13 go through
// 44C9A0/44C9C0 (next-stage index of the selected course 7D3188 +24/+28,
// -1 or for 13 with 48B310 && 48B350 -> code 0x42, else [7D33BC + i*0x78])
// then 5D4FD8[code*16].
// 443EA0 (= 4430B0, ECX = the event-36 object of 4035F0): releases the
// flagged overlays +490/+488 (mode 19 init).
void native_event36_close_overlays_443ea0(NativeRuntimeContext& c){
    auto& st=c.event_function36;
    FrontendStackServices stack{};stack.user=&st;stack.release=native_frontend_release_441200;
    frontend_close_overlays_4430b0(driving::Bytes(st.object.data(),st.object.size()),stack);
}
bool native_course_world_id_44dbb0(const NativeRuntimeContext& c,std::uint32_t n,std::uint32_t& value){
    if(n!=12u&&n!=13u)return course_world_id(c,n,value);
    auto& area=native_race_area_memory(const_cast<NativeRuntimeContext&>(c),nullptr,true);
    std::uint32_t code=0x42u;
    const bool skip=n==13u&&c.start_mode.flag_830394!=0u&&std::int32_t(c.start_mode.frontend_prepare.output_code_656234)<0x3c;
    if(!skip){
        const std::int32_t i=area.i32(area.u32(0x7d3188u)+(n==12u?0x24u:0x28u));
        if(i!=-1)code=area.u32(area.u32(0x7d33bcu)+std::uint32_t(i)*0x78u);
    }
    return exe_view().u32(0x5d4fd8u+code*16u,value);
}
// ---- race manager binding: 4962A0 for the event-359 owner: begin ----
bool native_mission_type4_4962a0(const NativeRuntimeContext& c){return scene_owner_mission_type4_4962a0(c);}
// ---- race manager binding: end ----
namespace {
bool scene_owner_mission_type4_4962a0(const NativeRuntimeContext& c){
    const auto& start=c.start_mode;
    if(!start.selection_active_836374||c.mission.manager.record_83637c<0||!start.scene_owner_race_assets)return false;
    std::uint32_t type{};
    return race_record_type(*start.scene_owner_race_assets,start.scene_owner_race_key,start.scene_owner_race_sub_key,type)&&type==4u;
}
}

std::uint32_t native_scene_owner_collision_43dba0(NativeRuntimeContext&,std::uint32_t,std::uint32_t);
std::uint32_t native_scene_owner_environment_reset_44a080(NativeRuntimeContext&);
std::uint32_t native_scene_owner_environment_44aa80(NativeRuntimeContext&,std::uint32_t,std::uint32_t,std::uint32_t);
std::uint32_t native_scene_owner_geometry_reset_44c2e0(NativeRuntimeContext&);
std::uint32_t native_scene_owner_geometry_44c310(NativeRuntimeContext&,std::uint32_t,std::uint32_t);
std::uint32_t native_scene_owner_object_4f10d0(NativeRuntimeContext&,std::uint32_t);
std::uint32_t native_scene_owner_object_4f0430(NativeRuntimeContext&,std::uint32_t,std::uint32_t);
std::uint32_t native_scene_owner_targets_46fe50(NativeRuntimeContext&,std::uint32_t);
std::uint32_t native_scene_owner_static_tables(NativeRuntimeContext&,std::uint32_t,std::uint32_t);
bool native_scene_owner_models_49b870(NativeRuntimeContext&);
// ---- 49BA80 world loaders: lane state machines on the selected course -------
// Return contract of the PC loaders: nonzero while the lane is still loading.
// Files are the descriptor's own path strings (EXE data), read from the
// retail tree like the PC async loader does.
namespace {
std::uint32_t scene_owner_fault_at(NativeStartModeState& start,std::uint32_t at,const char* why){
    if(start.scene_owner_fault==0u)start.scene_owner_fault=at;
    start.scene_owner_missing_service=at;
    if(why&&start.scene_owner_fault_reason.empty())start.scene_owner_fault_reason=why;
    return 1u;
}
struct WorldFile {const std::vector<std::uint8_t>* bytes{};std::string path;std::uint32_t crc{};};
std::uint32_t world_crc32(const std::uint8_t* p,std::size_t n){
    std::uint32_t c=0xffffffffu;
    for(std::size_t i=0;i<n;++i){c^=p[i];for(int k=0;k<8;++k)c=(c>>1)^(0xedb88320u&(0u-(c&1u)));}
    return ~c;
}
bool world_file(NativeRuntimeContext& c,std::uint32_t token,WorldFile& out){
    const auto* p=exe_view().at(token,1);if(!p)return false;
    std::string path;for(std::uint32_t i=0;i<200u;++i){std::uint8_t ch;if(!exe_view().u8(token+i,ch))return false;if(!ch)break;path.push_back(char(ch));}
    auto* retail=c.event_function36.retail_assets;if(!retail||path.empty())return false;
    std::string error;
    const bool sz=path.size()>3u&&(path.compare(path.size()-3u,3u,".sz")==0||path.compare(path.size()-3u,3u,".SZ")==0);
    const auto* bytes=retail_asset_guest_path(*retail,path,sz,&error);
    if(!bytes||bytes->empty())return false;
    out.bytes=bytes;out.path=path;out.crc=world_crc32(bytes->data(),bytes->size());return true;
}
std::uint32_t selected_index(const NativeRuntimeContext& c){return le32(c.game_mode.course_runtime.selected_7d30a8.data()+0x1cu);}
}

// 43DBA0(path, lane): one of the four collision roots (lane 0 = primary
// +0x0C, lanes 2/3 = secondary +0x18/+0x1C). 0 once admitted.
std::uint32_t native_scene_owner_collision_43dba0(NativeRuntimeContext& c,std::uint32_t path,std::uint32_t lane){
    auto& start=c.start_mode;
    if(lane>3u)return scene_owner_fault_at(start,0x43dba0u,"collision lane outside 0..3");
    auto& state=start.scene_owner_collision_lane_state[lane];
    if(state==2u)return 0u;
    WorldFile file;
    if(!world_file(c,path,file))return scene_owner_fault_at(start,0x43dba0u,"collision file missing");
    if(state==0u){state=1u;++start.scene_owner_collision_requests;start.scene_owner_missing_service=0x0044f880u;return 1u;}
    const auto index=selected_index(c);
    start.scene_owner_world_descriptor_index=index;start.scene_owner_world_ids_ready=true;
    const CourseWorldSourceIdentity identity{index,lane<2u?selected_primary(c):selected_secondary(c),path,file.crc};
    const auto& selected=c.game_mode.course_runtime;
    std::array<float,16> primary{},secondary{};
    std::memcpy(primary.data(),selected.matrix_7d2da0.data(),64u);
    std::memcpy(secondary.data(),selected.matrix_7d3190.data(),64u);
    auto& world=start.scene_owner_course_world;
    if(!world.admit_lane(lane,file.bytes->data(),file.bytes->size(),identity,&start.scene_owner_collision_error)||
       !world.set_transform(0u,primary,&start.scene_owner_collision_error)||
       !world.set_transform(1u,secondary,&start.scene_owner_collision_error))
        return scene_owner_fault_at(start,0x43dba0u,"collision root rejected");
    state=2u;start.scene_owner_collision_state=2u;start.scene_owner_collision_bytes+=std::uint32_t(file.bytes->size());
    return 0u;
}
// 44A080: clears the three environment slots.
std::uint32_t native_scene_owner_environment_reset_44a080(NativeRuntimeContext& c){
    auto& start=c.start_mode;
    ++start.scene_owner_world_reset_count;
    start.scene_owner_environment_states={};start.scene_owner_environment.reset();
    start.scene_owner_environment_error.clear();start.scene_owner_environment_ready=0u;start.scene_owner_environment_bytes=0u;
    return 0u;
}
// 44AA80(+24, +28, +2C): sun/fog/course environment lanes, then 44A940 initialisation.
std::uint32_t native_scene_owner_environment_44aa80(NativeRuntimeContext& c,std::uint32_t t0,std::uint32_t t1,std::uint32_t t2){
    auto& start=c.start_mode;
    const auto index=selected_index(c);
    const std::array<std::uint32_t,3> tokens{{t0,t1,t2}};
    for(std::size_t lane=0;lane<3u;++lane){
        const auto token=tokens[lane];auto& state=start.scene_owner_environment_states[lane];
        if(token==0u){
            if(state==0u){start.scene_owner_environment.admit_absent_lane(std::uint32_t(lane));state=2u;}
            continue;
        }
        WorldFile file;
        if(!world_file(c,token,file))return scene_owner_fault_at(start,0x44a0f0u,"environment file missing");
        if(state==0u){state=1u;++start.scene_owner_environment_requests;start.scene_owner_missing_service=0x0044f880u;return 1u;}
        if(state==1u){
            const CourseWorldSourceIdentity identity{index,selected_primary(c),token,file.crc};
            if(!start.scene_owner_environment.admit_lane(std::uint32_t(lane),file.bytes->data(),file.bytes->size(),
                   identity,&start.scene_owner_environment_error))
                return scene_owner_fault_at(start,0x44a940u,"environment file rejected");
            state=2u;++start.scene_owner_environment_ready;start.scene_owner_environment_bytes+=std::uint32_t(file.bytes->size());
        }
    }
    if(!start.scene_owner_environment.initialized()){
        if(!start.scene_owner_environment.initialize(c.game_mode.course_runtime.matrix_7d2da0,&start.scene_owner_environment_error))
            return scene_owner_fault_at(start,0x44a940u,"environment initialisation failed");
        ++start.scene_owner_environment_fixups;
        driving::course_environment_finish_44aa80(c.mode_state.current,start.environment_flags_7d28b0,start.environment_phase_7d28c8);
    }
    return 0u;
}
// 44C2E0: resets both geometry allocations.
std::uint32_t native_scene_owner_geometry_reset_44c2e0(NativeRuntimeContext& c){
    auto& start=c.start_mode;
    start.scene_owner_course_object_states={};start.scene_owner_course_objects={};start.scene_owner_course_object_identities={};
    start.scene_owner_course_object_ready=0u;start.scene_owner_course_object_bytes=0u;start.scene_owner_course_object_error.clear();
    ++start.scene_owner_course_objects_reset_count;
    return 0u;
}
// 44C310(token, lane): course geometry allocation (lane 0 = +0x20, lane 1 = +0x64).
std::uint32_t native_scene_owner_geometry_44c310(NativeRuntimeContext& c,std::uint32_t token,std::uint32_t lane){
    auto& start=c.start_mode;
    if(lane>1u)return scene_owner_fault_at(start,0x44c310u,"geometry lane outside 0/1");
    auto& state=start.scene_owner_course_object_states[lane];auto& bytes=start.scene_owner_course_objects[lane];
    auto& identity=start.scene_owner_course_object_identities[lane];
    if(token==0u){if(state==0u){state=2u;bytes.clear();identity={};}return 0u;}
    if(state==2u)return 0u;
    WorldFile file;
    if(!world_file(c,token,file))return scene_owner_fault_at(start,0x44c310u,"geometry file missing");
    if(state==0u){state=1u;++start.scene_owner_course_object_requests;start.scene_owner_missing_service=0x0044f880u;return 1u;}
    const auto& raw=*file.bytes;
    if(raw.size()<=4u||le32(raw.data())!=raw.size()-4u)return scene_owner_fault_at(start,0x44fc60u,"geometry allocation size prefix mismatch");
    bytes=raw;identity={selected_index(c),selected_primary(c),token,file.crc};
    state=2u;++start.scene_owner_course_object_ready;start.scene_owner_course_object_bytes+=std::uint32_t(raw.size());
    return 0u;
}
// 4F10D0(+0x30, 0): stage object allocation.
std::uint32_t native_scene_owner_object_4f10d0(NativeRuntimeContext& c,std::uint32_t token){
    auto& start=c.start_mode;
    auto& state=start.scene_owner_object_states[0];
    if(token==0u){if(state==0u)state=2u;return 0u;}
    if(state==2u)return 0u;
    WorldFile file;
    if(!world_file(c,token,file))return scene_owner_fault_at(start,0x4f10d0u,"stage object file missing");
    if(state==0u){state=1u;++start.scene_owner_object_requests;start.scene_owner_missing_service=0x0044f880u;return 1u;}
    const auto& raw=*file.bytes;
    if(raw.size()<=4u||le32(raw.data())!=raw.size()-4u)return scene_owner_fault_at(start,0x44fc60u,"stage object size prefix mismatch");
    start.scene_owner_object_files[0]=raw;
    start.scene_owner_object_identities[0]={selected_index(c),selected_primary(c),token,file.crc};
    state=2u;++start.scene_owner_object_ready;start.scene_owner_object_bytes+=std::uint32_t(raw.size());
    return 0u;
}
// 4F0430(path, lane): state 84CE7C+lane*4 (measured bridge 4F0437:
// mov eax,[edi*4+84CE7C]); 44FD80 request (mode 7 for lane 0), 44F880 poll,
// 44FC60 into 84D6C0+lane*4, then every 8-byte table entry and the ten
// pointer fields (+4,+C,..,+4C) of its record are relocated by the buffer
// base. The raw file is kept; its offsets are validated here, the consumers
// read them as offsets.
std::uint32_t native_scene_owner_object_4f0430(NativeRuntimeContext& c,std::uint32_t token,std::uint32_t lane){
    auto& start=c.start_mode;
    if(lane!=0u)return scene_owner_fault_at(start,0x4f0430u,"object table lane above 0");
    auto& state=start.scene_owner_object_states[1];
    if(state==0u){
        if(token==0u){state=2u;return 0u;}
        WorldFile file;if(!world_file(c,token,file))return scene_owner_fault_at(start,0x4f0430u,"course object table file missing");
        state=1u;++start.scene_owner_object_requests;return 1u;
    }
    if(state!=1u)return 0u;
    WorldFile file;if(!world_file(c,token,file))return scene_owner_fault_at(start,0x4f0430u,"course object table file missing");
    const auto& raw=*file.bytes;
    if(raw.size()<=4u||le32(raw.data())!=raw.size()-4u)return scene_owner_fault_at(start,0x44fc60u,"course object table size prefix mismatch");
    const std::uint8_t* base=raw.data()+4;const std::size_t size=raw.size()-4u;
    for(std::size_t e=0;;e+=8){
        if(e+4>size)return scene_owner_fault_at(start,0x4f04b0u,"course object table unterminated");
        const auto rec=le32(base+e);if(!rec)break;
        if(rec+0x50u>size)return scene_owner_fault_at(start,0x4f04b0u,"course object record outside file");
        for(unsigned f=4;f<0x50;f+=8){const auto v=le32(base+rec+f);if(v&&v>=size)return scene_owner_fault_at(start,0x4f04b0u,"course object pointer outside file");}
    }
    start.scene_owner_object_files[1]=raw;
    start.scene_owner_object_identities[1]={selected_index(c),selected_primary(c),token,file.crc};
    state=2u;++start.scene_owner_object_ready;start.scene_owner_object_bytes+=std::uint32_t(raw.size());
    return 0u;
}
// 46FE50(0, marker, 0xB): the 12 other-car target slots (0..7 read OCP files).
std::uint32_t native_scene_owner_targets_46fe50(NativeRuntimeContext& c,std::uint32_t marker){
    auto& start=c.start_mode;
    if(start.scene_owner_target_outer_state==1u)return 0u;
    if(start.scene_owner_target_index>=12u){start.scene_owner_target_outer_state=1u;start.scene_owner_target_index=0u;return 0u;}
    auto& target_state=start.scene_owner_target_states[start.scene_owner_target_index];
    if(target_state==0u){
        if(start.scene_owner_target_index>=8u){target_state=2u;++start.scene_owner_target_index;return 1u;}
        // 46FC40: slots 0..5 are othcar_01..06, slots 6..7 rvlcar_01..02 (family 1).
        const auto slot=start.scene_owner_target_index;
        if(!stage17_target_path_44c680(marker,0u,slot<6u?0u:1u,slot<6u?slot:slot-6u,start.scene_owner_target_path))
            return scene_owner_fault_at(start,0x44c680u,"target path parameters outside the original table");
        target_state=1u;++start.scene_owner_target_requests;
        if(!c.event_function36.retail_assets)return 1u;
    }
    if(target_state==1u){
        auto* retail=c.event_function36.retail_assets;
        if(!retail)return 1u;
        std::string retail_error;
        const auto* bytes=retail_asset_guest_path(*retail,start.scene_owner_target_path,false,&retail_error);
        if(!bytes||bytes->empty()){
            start.scene_owner_fault_reason="other-car target file missing: "+start.scene_owner_target_path;
            return scene_owner_fault_at(start,0x44fd20u,nullptr);
        }
        target_state=2u;return 1u;
    }
    if(target_state==2u){++start.scene_owner_target_index;return 1u;}
    return scene_owner_fault_at(start,0x46fe50u,"target state outside 0/1/2");
}
// 4EFB50(+0x68, 0xD) / 4EFAF0(+0x6C, 0xF): immutable EXE tables (stage17 pack).
std::uint32_t native_scene_owner_static_tables(NativeRuntimeContext& c,std::uint32_t pc,std::uint32_t token){
    auto& start=c.start_mode;
    const auto index=le32(c.game_mode.course_runtime.selected_7d30a8.data()+0x1cu);
    const auto* stage17=c.stage17_assets;
    if(!stage17||index>=stage17->records.size())return scene_owner_fault_at(start,pc,"stage17 table pack missing");
    const auto& tables=stage17->records[index];
    if(pc==0x4efb50u){
        if(token!=tables.token68)return scene_owner_fault_at(start,pc,"stage17 table 68 identity mismatch");
        if(!start.scene_owner_table68_ready){start.scene_owner_table68=tables.table68;start.scene_owner_table68_token=token;start.scene_owner_table68_ready=true;
            start.scene_owner_stage17_table_bytes+=std::uint32_t(tables.table68.size());}
    }else{
        if(token!=tables.token6c)return scene_owner_fault_at(start,pc,"stage17 table 6C identity mismatch");
        if(!start.scene_owner_table6c_ready){start.scene_owner_table6c=tables.table6c;start.scene_owner_table6c_token=token;start.scene_owner_table6c_ready=true;
            start.scene_owner_stage17_table_bytes+=std::uint32_t(tables.table6c.size());}
    }
    return 0u;
}
// 49B870: model set by variant, then 448AD0(resource, 0) for every family.
bool native_scene_owner_models_49b870(NativeRuntimeContext& c){
    auto& start=c.start_mode;const auto variant=c.game_mode.game_variant;const auto& view=exe_view();
    std::array<std::uint8_t,44> models{},families{};
    auto flag=[&](std::uint32_t m){if(m<44u)models[m]=1u;};
    switch(variant){
    case 0:for(std::uint32_t m=0;m<10u;++m)flag(m);break;
    case 1:for(std::uint32_t m:{4u,2u,6u,8u,0u,7u,5u,3u,9u})flag(m);break;
    case 2:for(std::uint32_t m:{2u,8u,5u})flag(m);break;
    case 5:case 8:break;
    case 6:case 7:{
        // 49B92C: variant 6 outside mission type 4 lists the mission racers (4961E0 -> 477250);
        // mission type 4 (4962A0) and variant 7 (49B944) flag the player car and the three
        // playback records (465F40).
        if(variant==6u&&!scene_owner_mission_type4_4962a0(c)){
            const auto& r=c.mission.racers;
            for(std::uint32_t i=0;i<r.count_80fb04;++i){
                const auto* p=r.racers_80fb00.data()+std::size_t(i)*RacerRecordBytes;
                if(le32(p+0x54)!=0u&&le32(p+0x5c)==0u)flag(p[0x72]);
            }
            break;
        }
        std::uint8_t colour{};auto model=[&](std::uint8_t car)->bool{
            for(std::uint32_t i=0;i<0x80u;++i){std::uint8_t k,v;if(!view.u8(0x5b3c34u+i*2u,k)||!view.u8(0x5b3c35u+i*2u,v))return false;
                if(std::int8_t(k)>=0&&k==car){colour=v;return true;}}
            return false;};
        if(!model(std::uint8_t(start.course_choice_655b59))){scene_owner_fault_at(start,0x46c840u,"46C840 car without a model entry");return false;}
        flag(std::uint32_t(std::int32_t(std::int8_t(colour))));
        for(std::uint32_t i=0;i<3u;++i){const auto car=native_ghost_car_465f40(c,i);if(car<0)continue;
            if(!model(std::uint8_t(car))){scene_owner_fault_at(start,0x46c840u,"46C840 ghost car without a model entry");return false;}
            flag(std::uint32_t(std::int32_t(std::int8_t(colour))));}
        break;
    }
    case 3:case 4:{
        // 49B8A5 (LAN races): the other players' cars, 46C840(4591F0(i)) for every CommRace
        // slot i < 456D60 except the local one (455AD0); the player car is loaded by START.
        const auto& w=c.race.car_world;
        const bool session=native_network_u32(0x7d68acu)!=0u;
        for(std::uint32_t i=0;i<w.commrace_7de418[0];++i){
            if(i==w.slot_7dd138)continue;
            const std::uint8_t car=session?w.commrace_7de418[0x0du+i*0x6cu]:0u;   // 7DE425 + i*0x6C
            std::uint8_t colour{};
            for(std::uint32_t k=0;k<0x80u;++k){std::uint8_t key,v;if(!view.u8(0x5b3c34u+k*2u,key)||!view.u8(0x5b3c35u+k*2u,v))return false;
                if(std::int8_t(key)>=0&&key==car){colour=v;break;}
                if(k==0x7fu){scene_owner_fault_at(start,0x46c840u,"46C840 network car without a model entry");return false;}}
            flag(std::uint32_t(std::int32_t(std::int8_t(colour))));
        }
        break;
    }
    case 9:                                                            // 49B91D: 4B0390 (the RaceAttack records)
        if(!native_race_attack_models_4b0390(c,models.data(),models.size())){scene_owner_fault_at(start,0x4b0390u,"4B0390 model outside the tables");return false;}
        break;
    case 10:break;                                                     // outside the 49BA1C table
    default:scene_owner_fault_at(start,0x49b870u,"49B870 model list for this variant not ported");return false;
    }
    for(std::uint32_t m=0;m<44u;++m)if(models[m]){
        start.scene_owner_models_8367c0[m]=1u;std::uint8_t f;if(!view.u8(0x5c2350u+m,f))return false;
        if(f<44u)families[f]=1u;
    }
    for(std::uint32_t f=0;f<44u;++f)if(families[f]){
        std::uint32_t dl=0x1e;
        for(std::uint32_t idx=0;idx<30u;++idx){std::uint8_t v{};if(view.u8(0x5c2350u+idx,v)&&v==f){dl=idx;break;}}
        for(std::uint32_t e=0;;++e){
            std::uint32_t key,id;if(!view.u32(0x5c2258u+e*8u,key)||!view.u32(0x5c225cu+e*8u,id))return false;
            if(std::int32_t(key)>=0&&key==dl){start.scene_owner_list_836850.push_back(id);++start.scene_owner_rearm_448ad0;break;}
            if(e>64u)return false;
        }
    }
    return true;
}

std::uint32_t native_scene_owner_service(NativeRuntimeContext& context,std::uint32_t pc,std::uint32_t eax,
                                         const std::array<std::uint32_t,3>& a){
    auto& start=context.start_mode;
    const auto variant=context.game_mode.game_variant;
    auto fault=[&](std::uint32_t at)->std::uint32_t{
        if(start.scene_owner_fault==0u)start.scene_owner_fault=at;
        start.scene_owner_missing_service=at;return 0u;
    };
    auto waiting=[&](std::uint32_t at,std::uint32_t busy)->std::uint32_t{start.scene_owner_missing_service=at;return busy;};
    const auto& frontend=context.event_function36;
    switch(pc){
    case 0x440d90u:++start.scene_owner_marker_440d90;return 0u;          // empty profiling marker
    case 0x48b140u:return start.course_choice_655b59&0xffu;
    case 0x48b180u:return frontend.car_select.transmission_830374;
    case 0x48b1a0u:return start.vehicle_variant_83036d;
    case 0x48b310u:return start.flag_830394;
    case 0x4957f0u:return start.selection_active_836374?1u:0u;
    case 0x4b00d0u:
        if(!start.scene_state_8421c0_known)return fault(pc);
        return start.scene_state_8421c0!=0u?1u:0u;
    case 0x55a930u:return start.manager_state_7f94c0!=0u?1u:0u;
    case 0x46c520u:return start.frontend_prepare.event_config_7f94c4[4];   // mov eax,[7F94D4] (46C280 / 4EEB50)
    case 0x4babe0u:
        if(a[0]>=SceneMeterIds.size())return fault(pc);
        return SceneMeterIds[a[0]];
    case 0x44c2d0u:{std::uint32_t v;if(!read_descriptor(selected_primary(context),0,v))return fault(pc);return v;}
    case 0x4999a0u:return scene_owner_classified_4999a0(eax)?1u:0u;
    case 0x4962a0u:return scene_owner_mission_type4_4962a0(context)?1u:0u;
    case 0x477210u:
        if(start.scene_owner_special_driver_80fb28<0)return fault(pc);
        return std::uint32_t(start.scene_owner_special_driver_80fb28);
    case 0x46bbe0u:{unsigned id{};if(!vehicle_resource_46bbe0(a[0],id))return fault(pc);return id;}
    case 0x488090u:{
        // 487EE0 (driver) / 487F50 (passenger). With 43F860 ([780260], set by
        // the SUMO_FE init 49E3C6) the license's [7C2400] picks the pair:
        // stack list {0,1,2,4,3,5}, 6549B0[] for the driver, 6AF268[k*3+n]
        // (n: variant 2 -> 1, 9 -> 2, else 0; variant 5 -> 46C520) then
        // 6549C8[] for the passenger.
        const bool console=start.frontend_prepare.game_flag_780260!=0u;
        const auto& license=frontend.frontend_profiles.active;
        const std::uint32_t pick=le32(license.data()+0x20u);                          // [7C2400]
        static constexpr std::uint32_t list[6]{0u,1u,2u,4u,3u,5u};
        std::uint32_t index;
        if(a[0]==0u){
            if(context.mode_state.current==3u)index=12u;
            else if(!console)index=11u;
            else{if(pick>=6u||!exe_view().u32(0x6549b0u+list[pick]*4u,index))return fault(pc);}
        }else if(!console){
            index=context.mode_state.current==3u?8u:
                (variant==1u||variant==3u||variant==4u||variant==5u)?9u:6u;
        }else{
            if(pick>=6u)return fault(pc);
            const std::uint32_t k=list[pick];
            std::uint32_t slot=6u;
            if(variant==5u){
                slot=3u+start.frontend_prepare.event_config_7f94c4[4];               // 46C520: [7F94D4]
            }else{
                const std::uint32_t n=variant==2u?1u:variant==9u?2u:0u;
                if(!exe_view().u32(0x6af268u+(k*3u+n)*4u,slot))return fault(0x505340u);
            }
            if(!exe_view().u32(0x6549c8u+slot*4u,index))return fault(pc);
        }
        std::uint32_t id;if(!exe_view().u32(0x654868u+index*16u,id))return fault(pc);
        return id;
    }
    case 0x44c220u:{std::uint32_t v;if(!course_field(context,a[0],v))return fault(pc);return v;}
    case 0x44dbb0u:{std::uint32_t v;if(!course_world_id(context,a[0],v))return fault(pc);return v;}
    // Resource lists and requests (49B390/49B3C0/49B3F0/49B420 record the id).
    case 0x49b390u:start.scene_owner_list_836958.push_back(eax);
        if(!scene_owner_request(context,eax,a[0])){
            if(start.scene_owner_fault_reason.empty())start.scene_owner_fault_reason="retail resource "+std::to_string(eax)+"/"+std::to_string(a[0])+" unavailable";
            return fault(0x42deb0u);
        }
        return 0u;
    case 0x49b3c0u:start.scene_owner_list_8367f8.push_back(eax);++start.scene_owner_release_count;return 0u;   // 429920
    case 0x49b3f0u:
        start.scene_owner_list_836850.push_back(eax);
        if(a[0]==0u){++start.scene_owner_rearm_448ad0;return 0u;}                   // 448AD0(id,0) rearms a resident entry
        if(!runtime_asset(context.event_function36,eax,a[0])&&eax<0x223u){
            // 448AD0 model manager: path 633558[id], read from the retail tree.
            std::uint32_t token{};std::string path;
            if(exe_view().u32(0x633558u+eax*4u,token)&&token)
                for(std::uint32_t i=0;i<200u;++i){std::uint8_t ch;if(!exe_view().u8(token+i,ch)||!ch)break;path.push_back(char(ch));}
            auto* retail=context.event_function36.retail_assets;std::string error;
            if(retail&&!path.empty()&&retail_asset_guest_path(*retail,path,false,&error)){
                start.scene_owner_model_paths.push_back(path);return 0u;
            }
        }
        if(!scene_owner_request(context,eax,a[0])){
            if(start.scene_owner_fault_reason.empty())start.scene_owner_fault_reason="retail resource "+std::to_string(eax)+"/"+std::to_string(a[0])+" unavailable";
            return fault(0x448ad0u);
        }
        return 0u;
    case 0x49b420u:{
        start.scene_owner_list_836720.push_back(eax);
        std::uint32_t bytes{};
        if(!scene_owner_motion_group_ready_table(context,eax,bytes))return fault(0x4f2020u);
        if(!native_rob_motion_group_request_4f2020(context,eax,a[0])){
            if(start.scene_owner_fault_reason.empty())start.scene_owner_fault_reason=context.race.robots.motion_tables.error;
            return fault(0x4f2060u);
        }
        ++start.scene_owner_motion_requests;start.scene_owner_motion_bytes+=bytes;
        return 0u;
    }
    case 0x448ad0u:++start.scene_owner_rearm_448ad0;return 0u;
    // Readiness gates (1 = ready).
    case 0x42df90u:case 0x4299a0u:
        if(!native_start_owned_resource_ready(context,pc)||!scene_owner_resources_ready(context))return waiting(pc,0u);
        return 1u;
    case 0x448980u:{
        const auto pending=frontend.loader_resource_pending|frontend.shared_resource_pending|
            frontend.frontend_resource_pending|frontend.frontend_bulk_pending;
        if(!driving::runtime_resource_ready_448980(pending)||!scene_owner_resources_ready(context))return waiting(pc,0u);
        return 1u;
    }
    case 0x4f21b0u:return 1u;                                      // motion groups are read synchronously (49B420)
    case 0x4276b0u:
        start.scene_owner_audio_command=0u;start.scene_owner_audio_phase=0u;++start.scene_owner_audio_resets;
        context.pc_sound.unload_all_4276b0();return 0u;
    case 0x427700u:++start.scene_owner_sound_checks;return context.pc_sound.request_427700(a[0]);
    // Loader-slot and lane resets.
    case 0x44fce0u:case 0x44fcc0u:++start.scene_owner_loader_reset_44fcc0;return 0u;   // no asynchronous slot is pending natively
    case 0x43db00u:++start.scene_owner_collision_releases;return 0u;
    case 0x49a650u:return 0u;   // 49A650: ret (the empty event callback)
    case 0x47f110u:return 0u;                                                          // xor eax,eax; ret
    case 0x452e30u:{   // 452E30(variant, preset): [7D6768] = book[(variant + (preset+2)*12)*4] (the average time)
        ++start.scene_owner_stage55_restore_452e30;start.scene_owner_stage55_variant=a[0];start.scene_owner_stage55_course_preset=a[1];
        auto& rs=native_race_end(context).state;
        const std::size_t at=(std::size_t(std::uint16_t(a[0]))+(std::size_t(std::uint16_t(a[1]))+2u)*12u)*4u;
        if(at+4u>rs.bookkeeping.size())return fault(0x452e48u);
        std::memcpy(rs.stats_7d6768.data(),rs.bookkeeping.data()+at,4);return 0u;}
    case 0x452db0u:{   // 452DB0(variant, preset): [7D6760] = book[(variant + preset*12)*4 + 0x50] (the play count)
        ++start.scene_owner_stage55_restore_452db0;
        auto& rs=native_race_end(context).state;
        const std::size_t at=(std::size_t(std::uint16_t(a[0]))+std::size_t(std::uint16_t(a[1]))*12u)*4u+0x50u;
        if(at+4u>rs.bookkeeping.size())return fault(0x452dc5u);
        std::memcpy(rs.stats_7d6720.data()+(0x7d6760u-0x7d6720u),rs.bookkeeping.data()+at,4);return 0u;}
    case 0x49b870u:return native_scene_owner_models_49b870(context)?0u:fault(pc);
    case 0x467ac0u:   // ghost module init (variant 7: 416700 save file request)
        if(!native_ghost_init_467ac0(context)){auto& g=native_race_ghosts(context);
            if(start.scene_owner_fault_reason.empty())start.scene_owner_fault_reason=g.error;return fault(g.fault);}
        return 0u;
    case 0x480fe0u:   // record module init (variant 0: 47ECA0 tables, 480AD0 save slot request)
        if(!native_records_init_480fe0(context,a[0],a[1],a[2])){auto& g=native_race_ghosts(context);
            if(start.scene_owner_fault_reason.empty())start.scene_owner_fault_reason=g.error;return fault(g.fault);}
        return 0u;
    case 0x481230u:   // record load (variant 0: 4810A0 = 480BC0 x 30, 416830)
        if(!native_records_load_481230(context,a[0],a[1],a[2])){auto& g=native_race_ghosts(context);
            if(start.scene_owner_fault_reason.empty())start.scene_owner_fault_reason=g.error;return fault(g.fault);}
        return 0u;
    case 0x4686c0u:   // ghost load (467340 slot 1, 416830)
        if(!native_ghost_load_4686c0(context)){auto& g=native_race_ghosts(context);
            if(start.scene_owner_fault_reason.empty())start.scene_owner_fault_reason=g.error;return fault(g.fault);}
        return 0u;
    case 0x4f0380u:++start.scene_owner_actor_reset_4f0380[a[0]&1u];return 0u;
    case 0x4f0400u:++start.scene_owner_event_reset_4f0400[a[0]&1u];return 0u;
    // ---- race road tables (race_area_runtime: 46FAC0/46FDE0 on the owned arena): begin ----
    case 0x46fc30u:++start.scene_owner_target_reset_46fc30;return native_race_road_tables_46fc30(context,a[0]);
    // ---- race road tables: end ----
    case 0x4ef860u:++start.scene_owner_global_reset_4ef860;start.scene_owner_table68_ready=false;return 0u;   // [84CE68] = 0: 4EFB50 reloads table68
    case 0x4ef850u:++start.scene_owner_global_reset_4ef850;start.scene_owner_table6c_ready=false;return 0u;   // [84CE6C] = 0: 4EFAF0 reloads table6c
    // World loaders (lane state machines; nonzero = still loading).
    case 0x43dba0u:return native_scene_owner_collision_43dba0(context,a[0],a[1]);
    case 0x44a080u:return native_scene_owner_environment_reset_44a080(context);
    case 0x44aa80u:return native_scene_owner_environment_44aa80(context,a[0],a[1],a[2]);
    case 0x44c2e0u:return native_scene_owner_geometry_reset_44c2e0(context);
    case 0x44c310u:return native_scene_owner_geometry_44c310(context,a[0],a[1]);
    case 0x4f10d0u:return native_scene_owner_object_4f10d0(context,a[0]);
    case 0x4f0430u:return native_scene_owner_object_4f0430(context,a[0],a[1]);
    // ---- race road tables: begin ----
    case 0x46fe50u:
        try{return native_race_road_tables_46fe50(context,a[0],a[1]);}
        catch(const std::exception& e){return scene_owner_fault_at(start,pc,e.what());}
    // ---- race road tables: end ----
    case 0x4efb50u:case 0x4efaf0u:return native_scene_owner_static_tables(context,pc,a[0]);
    case 0x4efd20u:return fault(pc);
    case 0x495490u:return start.route_gate_8361b4;   // mov al,[8361B4] (4D4AA6); written only by 495745/4957A0
    case 0x44c2c0u:return le32(context.game_mode.course_runtime.selected_7d30a8.data()+4u);   // mov eax,[7D30AC]
    case 0x45c470u:{std::uint32_t path{};std::string why;
        if(!native_race_hud_quest_object_45c470(context,a[0],path,why)){if(start.scene_owner_fault_reason.empty())start.scene_owner_fault_reason=why;return fault(pc);}
        return path;}
    default:return fault(pc);          // 480FE0, 481230, 467AC0, 4686C0, 44C2C0, 45C470 ...
    }
}

bool native_start_owned_call(NativeRuntimeContext& context,std::uint32_t pc_entry,
                             const std::uint32_t* args,std::size_t count,
                             std::uint32_t& result){
    auto& start=context.start_mode;
    switch(pc_entry){
        // 448AD0(id, mode) / 448960(id) of START's variant-5 steps (49DEBC / 49DEE8: the tour
        // object model 0x1EE): the AREA module's model requests (its resident set), the file read
        // from the retail tree 633558[id].
        case 0x00448ad0u:case 0x00448960u:{
            if(count!=(pc_entry==0x00448ad0u?2u:1u)||args[0]>=0x223u)return false;
            auto& area=native_race_area(context);
            if(pc_entry==0x00448960u){result=area.requested.count(args[0])?1u:0u;return true;}
            std::uint32_t token{};std::string path;
            if(exe_view().u32(0x633558u+args[0]*4u,token)&&token)
                for(std::uint32_t i=0;i<200u;++i){std::uint8_t ch;if(!exe_view().u8(token+i,ch)||!ch)break;path.push_back(char(ch));}
            auto* retail=context.event_function36.retail_assets;std::string error;
            if(!retail||path.empty()||!retail_asset_guest_path(*retail,path,true,&error))return false;
            area.requested.insert(args[0]);area.released.erase(args[0]);result=0u;return true;}
        // START's C2C request manager steps (49DEB0 46C2C0(0), 49DF05.. 46C2F0 / 46C240 / 46C4D0)
        case 0x0046c2f0u:if(count!=0u)return false;result=native_game_load_request_course(context)?1u:0u;return true;
        case 0x0046c2c0u:case 0x0046c240u:case 0x0046c4d0u:{
            if(count>1u)return false;
            if(count==1u)return requests_call(context,pc_entry,{args[0]},result);
            return requests_call(context,pc_entry,{},result);}
        case 0x0049ba80u:{
            if(count!=0u)return false;
            ++start.scene_owner_calls;
            result=0u;
            // A callee without a native equivalent latched a fault: the owner
            // stays where the original would still be waiting.
            if(start.scene_owner_fault!=0u){start.scene_owner_missing_service=start.scene_owner_fault;return true;}
            SceneOwnerInputs inputs{};
            inputs.variant_780258=context.game_mode.game_variant;
            inputs.preset_78024c=start.course_preset;
            const auto& course=context.game_mode.course_runtime;
            inputs.course_7d3188=course.selected_copy_active;
            if(inputs.course_7d3188){
                const auto secondary=le32(course.selected_7d30a8.data()+0x18u);
                (void)read_descriptor(secondary,0x18u,inputs.course_18_18);
                (void)read_descriptor(secondary,0x1cu,inputs.course_18_1c);
                inputs.course_64=le32(course.selected_7d30a8.data()+0x64u);
            }
            start.scene_owner_missing_service=0u;
            result=scene_owner_49ba80(start.scene_owner_stage,inputs,
                [&context](std::uint32_t pc,std::uint32_t eax,const std::array<std::uint32_t,3>& args){
                    return native_scene_owner_service(context,pc,eax,args);});
            if(result==1u){++start.scene_owner_complete_returns;start.scene_owner_missing_service=0u;}
            return true;
        }
        case 0x0044c2d0u:{
            // 44C2D0 = first dword of the selected primary descriptor (7D30BC).
            if(count!=0u)return false;
            return read_descriptor(selected_primary(context),0u,result);
        }
        case 0x00459210u:case 0x004f5360u:case 0x004f53b0u:case 0x004f53a0u:case 0x004f5150u:{
            // The LAN race start (START stages 5 / 59 when [8369C0]): the session's start
            // synchronisation in the network module (459210 / 4F5360 / 4F53B0 / 4F53A0 / 4F5150).
            if(count!=0u||!native_network_u32(0x7d68acu))return false;
            std::uint32_t eax=0;
            if(!native_network_invoke(context,pc_entry,0,{},&eax))return false;
            result=eax;return true;
        }
        case 0x004557f0u:{
            // PC 0x4557F0.  The pinned executable returns ready immediately
            // when the network mode byte is nonzero or fewer than two players
            // are registered.  Multiplayer synchronization itself is not yet
            // reconstructed, so only those exact early exits are owned here.
            if(count!=0u)return false;
            ++start.start_ready_4557f0_checks;
            if(start.network_mode_7df108!=0u||start.network_player_count_7df10f<2u){
                result=1u;return true;
            }
            return false;
        }
        case 0x00440870u:{
            // 440870 = 440750(4): the four variant-0 record car events (race_records / race_ghost_car).
            if(count!=0u)return false;
            if(!native_records_open_440870(context)){auto& g=native_race_ghosts(context);
                if(start.scene_owner_fault_reason.empty())start.scene_owner_fault_reason=g.error;return false;}
            result=0u;return true;
        }
        case 0x00440880u:{
            // 440880 = 4407E0(4): the four Time Attack ghost car events.
            if(count!=0u)return false;
            if(!native_ghost_open_440880(context)){auto& g=native_race_ghosts(context);
                if(start.scene_owner_fault_reason.empty())start.scene_owner_fault_reason=g.error;return false;}
            result=0u;return true;
        }
        case 0x004edce0u:   // 49E2AB (variants 3 / 4): the frontend manager 659930 flags +14 = 1 (racing)
            if(count!=1u||!args)return false;
            native_race_end(context).frontend_manager_659944=args[0];
            result=0u;return true;
        case 0x00440380u:{
            // Ordinary offline branch of PC 0x440380.  0x680AD4 is the
            // immutable value 4 on the pinned EXE, but for loading-scene 0/1
            // and non-variant-4 only ESI==8 survives the branch at 0x4403C9.
            // That path opens event 8/function 0x26 and records one 0x49FA80
            // descriptor.  Network/multi-lane branches remain external.
            if(count!=0u)return false;
            ++start.start_bootstrap_440380_calls;
            if(context.race.car_world.commrace_7de418[0]>1u||context.game_mode.game_variant==4u){
                const auto& w=context.race.car_world;
                if(!vehicle_creation_network_440380(context.vehicle_creation,w.commrace_7de418[0],w.slot_7dd138,
                        context.game_mode.game_variant,w.commrace_7de418.data(),w.commrace_7de418.size(),{&context,
                        [](void* p,unsigned event,unsigned function){auto& c=*static_cast<NativeRuntimeContext*>(p);
                            driving::event_setup_440110(c.event_state,event,function,c.event_descriptors,c.event_functions);
                            if(event==8u)++c.start_mode.start_bootstrap_event8_setups;return true;}}))return false;
                ++start.start_bootstrap_records_49fa80;
                result=0u;return true;
            }
            if(!vehicle_creation_offline_440380(context.vehicle_creation,
                    start.course_choice_655b59,start.vehicle_colour_655b5a,context.race.car_world.slot_7dd138,{&context,
                    [](void* p,unsigned event,unsigned function){auto& c=*static_cast<NativeRuntimeContext*>(p);
                        driving::event_setup_440110(c.event_state,event,function,c.event_descriptors,c.event_functions);
                        ++c.start_mode.start_bootstrap_event8_setups;return true;}}))return false;
            ++start.start_bootstrap_records_49fa80;
            result=0u;return true;
        }
        case 0x004f0d10u:{
            // The protected prologue resolves one of two optional route roots.
            // With the original BSS roots 0x84D6CC/0x84D6D0 null, the pinned
            // code returns without opening events.  This is the exact offline
            // state used by the native first-playable path.
            if(count!=1u||!args||args[0]>1u)return false;
            const auto lane=args[0];
            if(start.route_aux_roots_84d6cc[lane])return false;
            ++start.start_route_aux_4f0d10_calls[lane];
            result=0u;return true;
        }
        case 0x0048b310u:
            if(count!=0u)return false;
            result=start.route_suppress_830394;return true;
        case 0x00495490u:
            if(count!=0u)return false;
            result=start.route_gate_8361b4;return true;
        case 0x004871a0u:{
            // 0x4871A0 supports the five authored START route codes selected
            // by 0x49DD40.  The route tables contain 17 entries for code 10
            // and 24 entries for codes 11..14 before the 0x1F sentinel.
            if(count!=1u||!args||args[0]<10u||args[0]>14u)return false;
            const auto route=args[0];
            ++start.start_route_init_4871a0_calls;
            start.start_route_scene_82e7d4=static_cast<std::uint8_t>(route);
            start.start_route_mode_82e7d8=1u;
            start.start_route_state_82e7e4=2u;
            start.start_route_entry_count=route==10u?17u:24u;
            start.start_route_player_and_mask=0xffffffb9u; // & -0x47
            start.start_route_player_or_bits=0x38u;
            // The override itself (82E7C0.., script 653788[route], player
            // +2F0): event 7 (4874A0) then runs the START script (camera
            // fly-in 5E61D0[route], START sprites, signal SE, BGM 401000,
            // PasPlCar until GO). A fault latches the race end module.
            (void)native_race_start_camera_4871a0(context,route);
            result=0u;return true;
        }
        case 0x0045a920u:{
            // PC 0x45A920 returns 1 before touching any network timing state
            // when 0x7DF34C is null.  Native offline START never constructs
            // that manager; the online branch remains deliberately unowned.
            if(count!=0u)return false;
            ++start.start_gate_45a920_checks;
            if(!start.network_manager_7df34c_present){result=1u;return true;}
            return false;
        }
        case 0x004999f0u:{
            // 4999F0: the "Loading" icon of mode 28/30 (4999D0, token 480000) is released
            // (428900 token check, 4285A0), then [67F614] = -1; -1 already is a no-op.
            if(count!=0u)return false;
            native_loading_animation_close_4999f0(context);
            ++start.start_cleanup_4999f0_calls;result=0u;return true;
        }
        case 0x00428600u:{
            if(count!=0u)return false;
            ++start.start_cleanup_428600_calls;
            // 428600: the SPRANI instance pool is emptied (the 21 layer
            // cursors 956500 and counts 956558, then the +0 allocated word
            // of every 95E028 instance), so no frontend scene outlives START.
            context.event_function36.frontend_sprites.clear_allocations();
            result=0u;return true;
        }
        case 0x0042dfb0u:{
            if(count!=1u||!args||args[0]>=0x4bu||
               start.start_cleanup_42dfb0_count>=start.start_cleanup_42dfb0_ids.size())
                return false;
            start.start_cleanup_42dfb0_ids[start.start_cleanup_42dfb0_count++]=args[0];
            result=0u;return true;
        }
        case 0x004299c0u:{
            if(count!=1u||!args||args[0]<0x20u||
               start.start_cleanup_4299c0_count>=start.start_cleanup_4299c0_ids.size())
                return false;
            start.start_cleanup_4299c0_ids[start.start_cleanup_4299c0_count++]=args[0];
            result=0u;return true;
        }
        case 0x0044fce0u:{
            if(count!=1u||!args)return false;
            // 0x44FB60 reports idle iff no live 0x30-byte async records are
            // in state 0..2.  Every native pack used here is synchronous; the
            // explicit pending counters are the corresponding fail-closed gate.
            const auto& f=context.event_function36;
            const bool idle=f.loader_resource_pending==0u&&
                f.shared_resource_pending==0u&&f.frontend_resource_pending==0u&&
                f.frontend_bulk_pending==0u;
            if(idle){start.start_loader_mode_7d34c0=args[0];++start.start_loader_mode_writes;}
            result=0u;return true;
        }
        case 0x00456d60u: // byte getter 0x7DE418
            if(count!=0u)return false;
            result=context.race.car_world.commrace_7de418[0];return true;
        case 0x0055a930u: // manager +0x60 getter
            if(count!=0u)return false;
            result=start.manager_state_7f94c0;return true;
        case 0x00427db0u: // the voice set / render mode (versus or not)
            if(count!=0u)return false;
            // 427DB0: 95B210 = 4493C0 (English), 754B0C from 43F860 ([780260]),
            // the variant 780258 and, for variant 5, the license +208 (535EF0).
            context.pc_sound.voice_set_427db0(0u,start.frontend_prepare.game_flag_780260!=0u,context.game_mode.game_variant,
                le32(context.event_function36.frontend_profiles.active.data()+0x208u));
            result=start.render_mode_754b0c=context.pc_sound.voice_set_754b0c;
            return true;
        case 0x004962a0u:case 0x004962d0u:
            if(count!=0u||start.session_manager_active_836374)return false;
            result=0u;return true;
        case 0x004999a0u:
            if(count!=1u||!args)return false;
            result=(args[0]==0u||args[0]==7u||args[0]==8u)?1u:0u;
            return true;
        case 0x004957f0u:
            if(count!=0u)return false;
            result=start.selection_active_836374?1u:0u;return true;
        case 0x0048b140u:
            if(count!=0u)return false;
            result=start.course_choice_655b59&0xffu;return true;
        case 0x0048b130u:
            if(count!=1u||!args)return false;
            start.course_choice_655b59=std::uint8_t(args[0]);result=0;return true;
        case 0x0048b150u:
            if(count!=1u||!args)return false;
            start.vehicle_colour_655b5a=std::uint8_t(args[0]);result=0;return true;
        case 0x0048b160u:
            if(count!=0u)return false;
            result=start.vehicle_colour_655b5a;return true;
        case 0x0048b190u:
            if(count!=1u||!args)return false;
            start.vehicle_variant_83036d=std::uint8_t(args[0]);result=0;return true;
        case 0x0048b1a0u:
            if(count!=0u)return false;
            result=start.vehicle_variant_83036d;return true;
        case 0x004518a0u:
            if(count!=0u)return false;
            result=start.first_race_flags_7d3a10=1u;return true;
        case 0x0043fa90u:
            if(count!=0u)return false;
            if(start.game_flag_780248==0u)--start.mode_countdown_780250;
            result=start.mode_countdown_780250<=0?1u:0u;return true;
        case 0x0043f9c0u:
            if(count!=0u)return false;
            result=start.game_flag_780248;return true;
        case 0x0043f980u:
            if(count!=0u)return false;
            result=context.mode_state.snapshot_destination;return true;
        default:return false;
    }
}

bool parse_event_metadata(const std::uint8_t* data,std::size_t n,NativeRuntimeContext& c,std::string* error){
    if(error)error->clear();
    if((!data&&n!=0u)||(n!=LegacyEventMetadataFileSize&&n!=EventMetadataFileSize)){
        fail(error,"event metadata size mismatch");return false;
    }
    const bool legacy=n==LegacyEventMetadataFileSize;
    const auto& expected_magic=legacy?LegacyMagic:Magic;
    if(std::memcmp(data,expected_magic.data(),expected_magic.size())!=0){fail(error,"event metadata magic mismatch");return false;}
    const auto expected_version=legacy?LegacyEventMetadataVersion:EventMetadataVersion;
    const auto expected_modes=legacy?0u:static_cast<std::uint32_t>(NativeModeCount);
    if(le32(data+8u)!=expected_version||le32(data+12u)!=driving::PcEventSlotCount||
       le32(data+16u)!=driving::PcEventFunctionTableCount||le32(data+20u)!=expected_modes){
        fail(error,"event metadata header mismatch");return false;
    }
    std::size_t o=EventMetadataHeaderSize;
    for(auto& d:c.event_descriptors){d.descriptor_token=le32(data+o);d.work_token=le32(data+o+4);d.aux28=le32(data+o+8);d.startup=le32(data+o+12);d.function_id=le32(data+o+16);d.display_scene=le32(data+o+20);o+=EventMetadataDescriptorSize;}
    for(auto& q:c.event_functions){q.init_callback=le32(data+o);q.ctrl_callback=le32(data+o+4);q.disp_callback=le32(data+o+8);q.shadow_callback=le32(data+o+12);q.dest_callback=le32(data+o+16);o+=EventMetadataFunctionSize;}
    c.mode_descriptors={};
    if(!legacy)for(auto& q:c.mode_descriptors){q.unknown_0=le32(data+o);q.init_callback=le32(data+o+4);q.control_callback=le32(data+o+8);q.exit_callback=le32(data+o+12);o+=EventMetadataModeSize;}
    return true;
}

bool load_event_metadata_file(const char* path,NativeRuntimeContext& c,std::string* error){
    if(error)error->clear();
    if(!path){fail(error,"null event metadata path");return false;}
    std::FILE* f=std::fopen(path,"rb");if(!f){fail(error,"cannot open event metadata file");return false;}
    std::vector<unsigned char> b(EventMetadataFileSize+1u);const auto n=std::fread(b.data(),1,b.size(),f);const int extra=std::fgetc(f);std::fclose(f);
    if(extra!=EOF){fail(error,"event metadata size mismatch");return false;}
    return parse_event_metadata(b.data(),n,c,error);
}

bool native_runtime_event_function36_invoke(
    NativeRuntimeContext& c,std::uint32_t callback_token,std::uint32_t event_id){
    if(event_id!=405u)return false;
    auto& state=c.event_function36;
    driving::Bytes object(state.object.data(),state.object.size());
    switch(callback_token){
        case 0x0049e490u:{
            if(state.frontend_categories.constructed)(void)native_stage12_handle_virtual(&state,0x73000008,0);
            if(state.frontend_missions.constructed)(void)native_stage12_handle_virtual(&state,0x73000009,0);
            if(state.frontend_requests.constructed)(void)native_stage12_handle_virtual(&state,0x73000018,0);
            if(state.rankings.constructed)(void)native_stage12_handle_virtual(&state,0x7300000d,0);
            if(state.ghost_board.constructed)(void)native_stage12_handle_virtual(&state,0x7300000e,0);
            if(state.course_board.constructed)(void)native_stage12_handle_virtual(&state,0x7300000f,0);
            if(state.showroom.constructed)(void)native_stage12_handle_virtual(&state,0x73000010,0);
            if(state.mode_board.constructed)(void)native_stage12_handle_virtual(&state,0x73000012,0);
            if(state.board48.constructed)(void)native_stage12_handle_virtual(&state,0x73000013,0);
            if(state.board50.constructed)(void)native_stage12_handle_virtual(&state,0x73000014,0);
            if(state.network_menu6.constructed)(void)native_stage12_handle_virtual(&state,0x73000015,0);
            if(state.lan_menu8.constructed)(void)native_stage12_handle_virtual(&state,0x73000016,0);
            if(state.online_menu7.constructed)(void)native_stage12_handle_virtual(&state,0x73000017,0);
            for(std::uint32_t k=0;k<state.network_screens.size();++k)if(state.network_screens[k])(void)native_stage12_handle_virtual(&state,0x73000100u+k,0);
            state.frontend_category_progress={};
            if(state.title_widgets)state.title_widgets->reset();
            if(state.license_owners)state.license_owners->reset();
            state.frontend_course_tables={};
            state.frontend_bulk_missing_pc=0u;
            state.frontend_bulk_pending=1u;
            state.frontend_root_object.fill(0u);
            state.frontend_gate_object.fill(0u);
            state.frontend_gate_list={};
            state.frontend_first_menu_object.fill(0u);
            state.frontend_second_menu_object.fill(0u);
            state.frontend_choice_faults={};state.frontend_choice_timer=0;
            state.title_owner_object.fill(0u);
            state.title_owner_globals={};
            state.title_last_handle=0u;
            state.title_construct_calls=0u;
            state.title_init_calls=0u;
            state.title_gate_ticks=0u;
            state.title_pause_calls=0u;
            state.title_pause_flag_95b214=0u;
            state.title_pause_flag_7d2614=0u;
            state.title_initial_ui_calls=0u;
            state.title_initial_ui={};
            state.frontend_handle_bindings={};
            state.frontend_handle_count=0u;
            state.frontend_welcome_control_calls=0u;
            state.frontend_welcome_scene_token=0u;
            state.frontend_welcome_resource=0u;
            state.frontend_movie_request=-1;++state.frontend_movie_generation;
            if(!state.frontend_profiles_initialized){
                std::array<std::uint32_t,4> random_values{};
                for(auto& value:random_values)value=frontend_crt_random_580f40(state.pc_crt_random_state);
                frontend_profiles_initialize_bank_4162d0(state.frontend_profiles,random_values);
                state.frontend_profiles_initialized=true;
            }
            state.frontend_profiles.queried=false;
            state.frontend_sprites.reset();state.frontend_ui_missing_pc=0u;
            state.frontend_choice_scene_token=0u;state.frontend_choice_scene_frame=0.0f;
            state.frontend_last_missing_key=~0u;
            state.frontend_last_missing_action=0u;
            state.frontend_stack_pushes=0u;state.frontend_stack_pops=0u;
            state.state2_ui_resource_handle=0u;
            state.frontend_menu_index=0u;
            state.frontend_menu_token=0x00440094u;
            state.frontend_menu_navigation=0u;
            state.frontend_menu_confirms=0u;
            state.frontend_menu_preview_requests=0u;
            state.frontend_menu_cancels=0u;
            state.frontend_menu_committed=false;
            state.frontend_gate_input_action=0xffffffffu;
            state.frontend_pending_input_action=0xffffffffu;
            state.frontend_gate_animation_pending=0u;
            state.frontend_authored_animation_ready=0u;
            state.frontend_gate_control_calls=0u;
            state.frontend_gate_owner_actions=0u;
            state.frontend_first_menu_opens=0u;
            state.frontend_first_menu_handle=0u;
            state.frontend_second_menu_handle=0u;
            state.frontend_second_menu_opens=0u;
            // The PC singleton is already constructed before 0x49E490.  Its
            // three resource members therefore start with the canonical
            // inactive handle rather than the zero-filled host value.
            object.put32(0x0c50u,0xffffffffu);
            object.put32(0x0cfcu,0xffffffffu);
            native_frontend_commands(state,0x442ac0,object,0x51c,nullptr,0);
            FrontendUiResources ui{state.frontend_sprites};
            driving::PcObjectRuntimeInitServices services{};
            services.user=&state;services.init_embedded=native_stage12_embedded_void;
            services.ui_services=ui.notify();
            (void)driving::runtime_init_entry_49e490(
                object,state.callback_table,state.global_mode_6319a1,services);
            state.initialized=true;
            state.loader_resource_entries[0]={0u,7u,0u};
            state.loader_resource_entries[1]={0u,7u,0u};
            ++state.init_calls;
            if(c.mode_state.sumo_fe_reset_pending)(void)apply_sumo_fe_reset(c);
            return true;
        }
        case 0x0049e4a0u:{
            if(!state.initialized)return false;
            state.frontend_categories.glyphs.clear();
            state.frontend_missions.glyphs.clear();
            state.frontend_requests.glyphs.clear();
            state.rankings.glyphs.clear();state.rankings.images.clear();
            state.ghost_board.glyphs.clear();state.ghost_board.images.clear();
            state.course_board.glyphs.clear();state.course_board.images.clear();
            state.showroom.glyphs.clear();state.showroom.images.clear();state.showroom.icons.clear();
            state.board48.glyphs.clear();state.board48.images.clear();
            state.board50.glyphs.clear();state.board50.images.clear();
            if(!state.frontend_category_progress.categories_ready)
                (void)frontend_license_progress_tables(c.start_mode.scene_owner_race_assets,
                    c.start_mode.scene_owner_assignment_assets,state.frontend_course_tables,
                    c.mode_state.current,state.frontend_category_progress);
            if(state.title_widgets)state.title_widgets->begin_frame();
            state.network_widgets.clear();
            if(state.license_owners)state.license_owners->begin_frame();
            state.title_game_mode_78026c=c.mode_state.current;
            state.title_variant_780258=c.game_mode.game_variant;
            state.frontend_input=c.input_state.frontend;
            state.frontend_choice_timer+=state.frontend_ui_motion_step;
            // PC 0x453640 obtains a per-player feature mask from the active
            // input device before 0x444470 tests bit 0. The exact PC device
            // bitfield is platform-specific; a connected native controller is
            // the Switch equivalent producer for this single recovered bit.
            // In the race (modes 16 / 18) bit 0 is the start key's trigger (444470 opens the
            // pause menu, key 0x2C, on it): the frontend feature bit 1 of this frame.
            if(c.mode_state.current==16u||c.mode_state.current==18u){
                if(c.input_state.frontend.feature_mask&1u)state.title_player0_feature_mask|=1u;
                else state.title_player0_feature_mask&=~1u;
            }else if(c.input_state.connected)state.title_player0_feature_mask|=1u;
            else state.title_player0_feature_mask&=~1u;
            state.title_event_state=&c.event_state;
            state.runtime=&c;
            state.title_game_flag_780248=&c.start_mode.game_flag_780248;
            // In GAME the pool is ticked by event 398's 427F70 (sprite_2d_runtime).
            if(c.mode_state.current!=16u)state.frontend_sprites.tick({true,true,state.title_pause_flag_95b214!=0u});
            native_frontend_menu_update(c);
            driving::PcRuntimeOwnerServices445be0 services{};
            services.user=&state;
            services.call_this=native_event36_owner_call;
            services.selector_services={&state,native_state2_selector_virtual,native_state2_selector_void};
            // 442CB0: the network session 7D68AC (+5 in a session, +8 started) commits the race.
            auto& sel=state.selector_globals;
            sel.manager_handle=native_network_u32(0x7d68acu);
            sel.manager_flag5=sel.manager_handle?native_network_u8(sel.manager_handle+5u):0u;
            sel.manager_flag8=sel.manager_handle?native_network_u8(sel.manager_handle+8u):0u;
            const std::uint8_t started=sel.manager_flag8;
            services.transition_services={&state,native_state2_owner_action};
            services.state_services={&state,nullptr,nullptr,native_state2_runtime_u32};
            services.runtime_services=native_state2_runtime_services(state);
            services.commit_services={&state,native_commit_prepare,native_commit_global,native_commit_virtual};
            const driving::PcRuntimeOwnerInputs445be0 inputs{
                static_cast<float>(c.frame_state.frame_counter_95af0c),Scale62812c};
            state.last_control_result=driving::runtime_owner_entry_49e4a0(
                object,state.snapshot,state.selector_globals,inputs,services);
            if(sel.manager_handle&&started&&!sel.manager_flag8)native_network_put8(sel.manager_handle+8u,0);
            state.control_true_returns+=state.last_control_result!=0u?1u:0u;
            ++state.control_calls;
            return true;
        }
        case 0x0049e4c0u:{
            if(!state.initialized)return false;
            if(state.frontend_categories.constructed)(void)native_stage12_handle_virtual(&state,0x73000008,0);
            if(state.frontend_missions.constructed)(void)native_stage12_handle_virtual(&state,0x73000009,0);
            if(state.frontend_requests.constructed)(void)native_stage12_handle_virtual(&state,0x73000018,0);
            if(state.rankings.constructed)(void)native_stage12_handle_virtual(&state,0x7300000d,0);
            if(state.ghost_board.constructed)(void)native_stage12_handle_virtual(&state,0x7300000e,0);
            if(state.course_board.constructed)(void)native_stage12_handle_virtual(&state,0x7300000f,0);
            if(state.showroom.constructed)(void)native_stage12_handle_virtual(&state,0x73000010,0);
            if(state.mode_board.constructed)(void)native_stage12_handle_virtual(&state,0x73000012,0);
            if(state.board48.constructed)(void)native_stage12_handle_virtual(&state,0x73000013,0);
            if(state.board50.constructed)(void)native_stage12_handle_virtual(&state,0x73000014,0);
            if(state.network_menu6.constructed)(void)native_stage12_handle_virtual(&state,0x73000015,0);
            if(state.lan_menu8.constructed)(void)native_stage12_handle_virtual(&state,0x73000016,0);
            if(state.online_menu7.constructed)(void)native_stage12_handle_virtual(&state,0x73000017,0);
            for(std::uint32_t k=0;k<state.network_screens.size();++k)if(state.network_screens[k])(void)native_stage12_handle_virtual(&state,0x73000100u+k,0);
            if(state.title_widgets)state.title_widgets->reset();
            if(state.license_owners)state.license_owners->reset();
            driving::PcObjectRuntimeTeardownServices services{};
            (void)driving::runtime_destroy_entry_49e4c0(object,services);
            state.frontend_course_tables={};
            state.frontend_bulk_loader.special_handles.fill(0u);
            state.frontend_bulk_pending=1u;
            state.initialized=false;
            ++state.destroy_calls;
            return true;
        }
        default:return false;
    }
}

bool native_runtime_attach_loader_assets(NativeRuntimeContext& c,
                                         const LoaderAssetPack& pack){
    if(!find_loader_asset(pack,0xbau,2u)||!find_loader_asset(pack,0xbbu,2u)||
       !find_loader_asset(pack,0x2cu,8u)||!find_loader_asset(pack,0x33u,8u)||
       !find_loader_asset(pack,0x48u,8u)||!find_loader_asset(pack,0x44u,9u)||
       !find_loader_asset(pack,LoaderAssetSelectTableId,0u))return false;
    for(std::uint32_t lane=0u;lane<LoaderAssetFrontendScriptCount;++lane)
        if(!find_loader_asset(pack,LoaderAssetFrontendScriptBaseId+lane,0u))
            return false;
    c.event_function36.loader_assets=&pack;
    c.event_function36.retail_assets=nullptr;
    return true;
}

bool native_runtime_attach_retail_assets(NativeRuntimeContext& c,
                                         RetailAssetStore& store){
    if(store.root.empty())return false;
    constexpr std::array<std::pair<std::uint32_t,std::uint32_t>,7> core{{
        {0xbau,2u},{0xbbu,2u},{0x2cu,8u},{0x33u,8u},{0x48u,8u},{0x44u,9u},
        {LoaderAssetSelectTableId,0u}}};
    for(const auto& item:core){
        std::string ignored;
        if(!retail_asset_lookup(store,item.first,item.second,&ignored))return false;
    }
    c.event_function36.retail_assets=&store;
    c.game_mode.course_retail=&store;
    return true;
}

bool native_runtime_attach_course_assets(NativeRuntimeContext& c,
                                         const CourseAssetPack& pack){
    if(pack.descriptors.primary_count!=driving::PcCoursePrimaryDescriptorCountR078||
       pack.descriptors.secondary_count!=driving::PcCourseSecondaryDescriptorCountR078||
       pack.course_blob.size()!=CourseCvtBlobBytes)return false;
    driving::PcRelocCategoryBlobR077 blob{};
    if(!driving::course_reloc_blob_open_r077(
           const_cast<std::uint8_t*>(pack.course_blob.data()),pack.course_blob.size(),blob)||
       driving::runtime_category_count_4f1ba0("csc_data_cvt_course",blob)!=CourseCvtRecordCount||
       driving::runtime_category_records_4f1a90("csc_data_cvt_course",blob)==nullptr)
        return false;
    c.game_mode.course_assets=&pack;
    return true;
}

bool native_runtime_attach_stage17_assets(NativeRuntimeContext& c,
                                          const Stage17AssetPack& pack){
    const auto* course=c.game_mode.course_assets;
    if(!course||course->descriptors.primary_count!=Stage17AssetRecordCount)return false;
    for(std::size_t i=0;i<Stage17AssetRecordCount;++i){
        const auto& descriptor=course->descriptors.storage[i];
        if(le32(descriptor.data()+0x68u)!=pack.records[i].token68||
           le32(descriptor.data()+0x6cu)!=pack.records[i].token6c)
            return false;
    }
    c.stage17_assets=&pack;
    return true;
}

bool native_runtime_attach_race_assets(NativeRuntimeContext& c,
                                       const RaceAssetPack& pack){
    if(pack.race_count!=94u||pack.bytes.empty())return false;
    c.start_mode.scene_owner_race_assets=&pack;
    c.event_function36.frontend_races=&pack;
    c.event_function36.frontend_category_progress.categories_ready=false;
    return true;
}

bool native_runtime_attach_race_assignment(NativeRuntimeContext& c,
                                           const RaceAssignmentPack& pack){
    if(pack.menu_count!=40u)return false;
    c.start_mode.scene_owner_assignment_assets=&pack;
    c.event_function36.frontend_assignment=&pack;
    c.event_function36.frontend_category_progress.categories_ready=false;
    return true;
}

bool native_runtime_license_completion(const NativeRuntimeContext& c,
                                        const PcLicense& license,double& result){
    LicenseProgressTables tables{};
    if(!frontend_license_progress_tables(c.start_mode.scene_owner_race_assets,
        c.start_mode.scene_owner_assignment_assets,c.event_function36.frontend_course_tables,
        c.mode_state.current,tables))return false;
    return frontend_license_completion_447400(license,tables,result);
}

void native_loading_animation_close_4999f0(NativeRuntimeContext& c){
    auto& handle=c.start_mode.start_system_handle_67f614;
    if(handle==-1)return;
    auto& pool=c.event_function36.frontend_sprites;
    // PC 428900 reads the token even if the slot has since been recycled.
    if(const auto* sprite=pool.get(std::uint32_t(handle));sprite&&sprite->token==0x480000u)
        pool.release(std::uint32_t(handle));
    handle=-1;
}

bool native_frontend_loader_call(NativeRuntimeContext& c,std::uint32_t pc,std::uint32_t& result){
    auto& state=c.event_function36;
    switch(pc){
    case 0x496170u:case 0x496180u:case 0x4e85e0u:case 0x4e8620u:
        state.runtime=&c;
        result=native_shared_loader_call(&state,pc,nullptr,0u);return true;
    case 0x4e86e0u:
        state.frontend_bulk_loader={};
        driving::frontend_bulk_loader_initialize_4e85e0(state.frontend_bulk_loader);
        state.frontend_course_tables={};
        state.frontend_category_progress.categories_ready=false;
        state.frontend_bulk_pending=1u;result=0u;return true;
    default:return false;
    }
}

bool native_runtime_attach_world_source(NativeRuntimeContext& c,
                                        const WorldSourcePack& pack){
    if(pack.descriptor_index!=15u||pack.bytes.empty()||
       pack.entries[0].descriptor_field!=0x0cu||
       !world_source_bytes(pack,pack.entries[0]))return false;
    c.start_mode.scene_owner_world_source=&pack;
    return true;
}

bool native_runtime_attach_gameplay_assets(NativeRuntimeContext& c,
                                           const CourseWorldRuntime& world,
                                           const DrivingDataPack& driving){
    if(!world.query_ready())return false;
    PcVehicleControlState probe{};
    if(!configure_pc_vehicle_control(probe,driving,DrivingPackV1LegacyCarId,0))return false;
    c.game_mode.driving_data=&driving;
    c.game_mode.gameplay_fallback_world=world;
    c.game_mode.gameplay_fallback_world_ready=true;
    return true;
}

bool native_runtime_event_function36_display(NativeRuntimeContext& c){
    auto& state=c.event_function36;
    const auto& slot=c.event_state.slots[405];
    if(!state.initialized||(slot.flags&0x02u)==0u||slot.disp_callback!=0x0049e4b0u)
        return false;
    driving::PcObjectStateServices services{};
    services.user=&state;
    services.call_virtual_void=[](void* p,unsigned handle,unsigned slot){
        auto& state=*static_cast<NativeEventFunction36State*>(p);
        if(slot==12&&handle&&handle==state.title_last_handle){
            unsigned result{};
            if(!state.title_widgets||!state.title_widgets->invoke(0x4d7260,0,result))
                state.frontend_ui_missing_pc=state.title_widgets?state.title_widgets->missing_pc():0x4d7260;
        }
        if(slot==12&&native_network_screen_handle(handle))(void)native_stage12_handle_virtual(&state,handle,slot);
        if(slot==12&&(handle==0x73000006||handle==0x73000007||handle==0x73000008||handle==0x73000009||handle==0x73000018||handle==0x7300000a||handle==0x7300000b||handle==0x7300000c||handle==0x7300000d||handle==0x7300000e||handle==0x7300000f||handle==0x73000010||handle==0x73000013||handle==0x73000014))
            (void)native_stage12_handle_virtual(&state,handle,slot);
    };
    // 442E00 ends with 446A50 on +0x51C: the slot captions are drawn by the host's frontend
    // frame (frontend_slot_captions_446a50 into its glyph layer), not here.
    services.call_embedded_void=[](void* p,std::uint32_t pc,driving::Bytes,std::size_t off){
        if(pc!=0x446a50u||off!=0x51cu)outrun::driving::service_hole("native_runtime_event_function36_display","call_embedded_void");
        else ++static_cast<NativeEventFunction36State*>(p)->display_captions_446a50;};
    driving::runtime_display_entry_49e4b0(
        driving::Bytes(state.object.data(),state.object.size()),services);
    ++state.display_calls;
    return true;
}

bool native_runtime_request_mode(NativeRuntimeContext& c,std::uint32_t mode_index){
    if(mode_index>=NativeModeCount)return false;
    c.mode_state.requested=mode_index;
    c.mode_state.transition_pending=1u;
    return true;
}

bool native_runtime_mode_control(NativeRuntimeContext& c,const NativeModeServices& services){
    auto& state=c.mode_state;
    const auto invoke=[&](std::uint32_t mode,NativeModePhase phase)->bool{
        if(mode>=NativeModeCount)return false;
        const auto& descriptor=c.mode_descriptors[mode];
        const auto token=phase==NativeModePhase::Init?descriptor.init_callback:
                         phase==NativeModePhase::Control?descriptor.control_callback:
                         descriptor.exit_callback;
        if(token==0u)return false;
        state.last_callback=token;state.last_mode=mode;
        native_race_sound_mode_token(c,token);          // 48B210 / 48B2A0 write 83036C
        if(native_race_end_mode_active(mode))(void)native_race_end_mode(c,mode,token,phase);   // modes 20/25/27/28
        if(phase==NativeModePhase::Init){
            ++state.init_callbacks;
            if(mode==13u&&token==0x0049db20u){
                native_start_mode_init(c);
                if(!c.start_mode.active)return false;
            }else if(mode==32u&&token==0x0049e380u){
                // Boot mode 0 (49E4F0 / 49E6A0) is not run natively: its
                // stage 0x0C (49E73D) character loader 488B80 runs once here,
                // before the first SUMO_FE init as on the PC.
                if(c.race.robots.chr_loads==0u)native_rob_chr_load_488b80(c);
                native_race_end_miles_init(c);   // 49E747: 4EF280 OutRun Miles tables (once)
                // 0x49E39D: 44DA00(1, 0) loads the course data for the frontend
                // (the variant-6 mission manager applies its courses onto it).
                native_game_load_course(c.game_mode,1u,0u);
                // PC SUMO_FE init 0x49E3A9 installs event 0x196/function 0x51
                // before resetting the event-405 owner at 0x49E3BF. Its
                // 0x414940/0x414980 callbacks are a separate PC auxiliary
                // subsystem; they are not the frontend menu input handler.
                driving::event_setup_440110(c.event_state,0x196u,0x51u,
                                            c.event_descriptors,c.event_functions);
                ++state.sumo_fe_event_setup_calls;
                state.sumo_fe_reset_pending=true;
                (void)apply_sumo_fe_reset(c);
                // 0x49E3C4: 43F870(1): [780260] = 1 (the SUMO_FE game; a
                // commit of menu mode 2 clears it), then tail 4518C0 clears
                // the three fog flags 7D3A10/7D3A2C/7D3A48 (START rebuilds them).
                c.start_mode.frontend_prepare.game_flag_780260=1u;
                c.start_mode.first_race_flags_7d3a10=0u;
            }else if(mode==16u&&token==0x00499d90u)
                native_game_mode_init(c);
        }else if(phase==NativeModePhase::Control){
            ++state.control_callbacks;
            if(mode==13u&&token==0x0049dd40u){
                native_start_mode_control(c,services);
            }else if(mode==32u&&token==0x0049e3e0u){
                // 0x49E3E0 first ticks the model preloader 48BFE0 on 83039C.
                if(c.event_function36.initialized)(void)native_car_loader_tick_48bfe0(c);
                // 0x49E3F4..0x49E406: consume the singleton's +0x21C
                // command and forward a valid mode to 0x43F8C0. Input still
                // originates from the diagnostic controller adapter above.
                ++state.sumo_fe_owner_command_polls;
                state.sumo_fe_last_owner_command=0xffffffffu;
                if(c.event_function36.initialized){
                    auto owner=driving::Bytes(c.event_function36.object.data(),
                                              c.event_function36.object.size());
                    state.sumo_fe_last_owner_command=
                        driving::object_take_token_440dc0(owner);
                }
                if(state.sumo_fe_last_owner_command!=0xffffffffu&&
                   state.sumo_fe_last_owner_command<NativeModeCount&&
                   state.transition_pending==0u){
                    state.requested=state.sumo_fe_last_owner_command;
                    state.transition_pending=1u;
                    if(state.requested==16u)++state.sumo_fe_game_requests;
                }
            }else if(mode==16u&&token==0x0049c840u)
                native_game_mode_control(c);
            else if(mode==18u&&token==0x0049c980u){
                // Mode 18 (the race pause, entered by the event-405 owner's +21C token 0x12):
                // 49C980 forwards the owner's next token (the pause menu's choice), runs the
                // GAME control 49C840 in variant 4 only, then 42CC00(1E0,168) (debug text
                // cursor, no visible effect).
                std::uint32_t command=0xffffffffu;
                if(c.event_function36.initialized){
                    auto owner=driving::Bytes(c.event_function36.object.data(),c.event_function36.object.size());
                    command=driving::object_take_token_440dc0(owner);
                }
                if(command!=0xffffffffu&&command<NativeModeCount)(void)native_runtime_request_mode(c,command);
                if(c.game_mode.game_variant==4u)native_game_mode_control(c);
                ++c.game_mode.pause_controls;
            }
        }else{
            ++state.exit_callbacks;
            if(mode==13u&&token==0x00499d80u){
                native_start_mode_exit(c);
            }else if(mode==32u&&token==0x0049e410u){
                driving::event_close_4401d0(c.event_state,0x196u);
                ++state.sumo_fe_event_close_calls;
                // 0x49E42C: 4035F0 -> 443C30, the event-405 owner leaves the menus: its
                // runtime handles released, both UI resources reset, state 4 (445BE0 then
                // enters the race state 3, where the pause key opens the pause menu).
                if(c.event_function36.initialized){
                    auto& st=c.event_function36;
                    driving::PcObjectRuntimeTeardownServices ts{};
                    ts.user=&st;
                    ts.reset_runtime=[](void* u,std::uint32_t pc,driving::Bytes object,std::size_t offset){
                        native_frontend_commands(*static_cast<NativeEventFunction36State*>(u),pc,object,offset,nullptr,0);};
                    ts.handle_virtual=[](void* u,std::uint32_t slot,std::uint32_t handle,std::uint32_t)->std::uint32_t{
                        return native_stage12_handle_virtual(u,handle,slot);};
                    ts.global_call=[](void* u,std::uint32_t pc,std::uint32_t arg,bool)->std::uint32_t{
                        // 428600 / 4299C0(44, 47) / 42DFB0(44, 47): the frontend 2D pool and
                        // banks 44/47; the native 2D owner keeps them (counted, not applied).
                        ++static_cast<NativeEventFunction36State*>(u)->teardown_globals_skipped[pc|(arg<<24)];return 0u;};
                    ts.ui_services=native_state2_ui_services(st);
                    (void)driving::object_runtime_teardown_443c30(driving::Bytes(st.object.data(),st.object.size()),ts);
                    ++st.owner_teardowns;
                }
            }else if(mode==16u&&token==0x00499e50u)
                native_game_mode_exit(c);
            else if(mode==18u&&token==0x0049a050u)
                c.game_mode.transition_latched=0u;                                   // 49A050: [836CE8] = 0
        }
        if(services.invoke)services.invoke(services.user,token,mode,phase);else outrun::driving::service_hole("native_runtime_mode_control","services.invoke");
        return true;
    };

    ++state.dispatcher_calls;
    state.snapshot_destination=state.snapshot_source;
    if(state.transition_pending!=0u){
        const auto requested=state.requested;
        if(requested>=NativeModeCount)return false;
        const auto previous=state.current;
        state.current=requested;state.transition_pending=0u;state.previous=previous;
        if(!invoke(requested,NativeModePhase::Init))return false;
    }
    if(!invoke(state.current,NativeModePhase::Control))return false;
    if(state.transition_pending!=0u){
        if(!invoke(state.current,NativeModePhase::Exit))return false;
        state.exit_byte_unknown=0u;
    }
    return true;
}

namespace {
struct NativeEventCallbackBridge {
    NativeRuntimeContext* context{};
    driving::PcEventServices upstream{};
};
void native_event_callback_invoke(void* user,std::uint32_t callback_token,
                                  std::uint32_t work_token,std::uint32_t event_id){
    auto& bridge=*static_cast<NativeEventCallbackBridge*>(user);
    if(bridge.context)
        (void)native_runtime_event_function36_invoke(*bridge.context,callback_token,event_id);
    if(bridge.upstream.invoke)
        bridge.upstream.invoke(bridge.upstream.user,callback_token,work_token,event_id);else outrun::driving::service_hole("native_event_callback_invoke","bridge.upstream.invoke");
}
struct NativeFramePlatformBridge {
    NativeRuntimeContext* context{};
    NativeRuntimePlatform* platform{};
    NativeRuntimeInput* input{};
    driving::PcEventServices event{};
    NativeModeServices mode{};
    bool use_native_event_control{};
    bool use_native_mode_control{};
    driving::PcRuntimeFrameServices417c7b upstream{};
};
std::uint32_t native_frame_platform_call(void* user,std::uint32_t pc,const std::uint32_t* args,std::size_t count){
    auto& b=*static_cast<NativeFramePlatformBridge*>(user);
    if(pc==0x449430u&&b.platform){
        const auto previous=b.platform->frame_start_tick;
        runtime_platform_begin_frame(*b.platform);
        if(b.context){
            const auto frequency=b.platform->cached_frequency;
            const auto ticks=b.platform->frame_start_tick-previous;
            const float milliseconds=previous&&frequency?
                float(static_cast<long double>(ticks)*1000.0L/frequency):0.0f;
            b.context->event_function36.frontend_ui_motion_step=
                float(static_cast<long double>(milliseconds)*0.001f);
            // Port: above 60 Hz, 449E30 is the time of the ticks run: one
            // tick for the controls, then this frame's ticks for the displays.
            if(enhancements::display_frames())b.context->event_function36.frontend_ui_motion_step=float(static_cast<long double>(1000.0f/60.0f)*0.001f);
        }
    }
    if(pc==0x454670u&&b.context&&enhancements::display_frames())   // after the ticks (417C7B)
        b.context->event_function36.frontend_ui_motion_step=float(static_cast<long double>(1000.0f/60.0f*float(enhancements::display_ticks()))*0.001f);
    if(pc==0x453bb0u&&b.context&&b.input){
        runtime_input_sample(*b.input,b.context->input_state);
        auto& in=b.context->input_state;
        // 406FA0 (the PC device layer) every frame; the race (modes 16 / 18) reads its
        // record, the other modes keep the direct mapping (the menus' A / B).
        if(in.pc_input_layer){
            native_pc_input_set_pad(*b.context,in.pc_pad);
            PcInputDevice d{};const auto m=b.context->mode_state.current;
            in.pc_record_used=native_pc_input_update_406fa0(*b.context,d)&&(m==16u||m==18u);
            if(in.pc_record_used)in.pc_device=d;
            if(std::getenv("OR2_INPUT_DEBUG")){static std::string last;auto& st=native_pc_input_stats();
                if(st.last_error!=last){last=st.last_error;std::fprintf(stderr,"[input] %s\n",last.c_str());}
                if(st.updates%60u==1u)std::fprintf(stderr,"[input] updates %u bits %08x steer %d accel %d brake %d\n",
                    st.updates,d.buttons_04,d.axes_94[0xe],d.axes_94[0xd],d.axes_94[0xc]);}
        }
        race_input_update_453bb0(*b.context,in.pc_device,in.pc_input_layer&&in.pc_record_used&&driving::Bytes(const_cast<std::uint8_t*>(native_pc_input_record(*b.context)),0x1d4).u32(0x1d0)==0u);
    }
    if(pc==0x43fa20u&&b.context&&b.use_native_mode_control)
        (void)native_runtime_mode_control(*b.context,b.mode);
    if(pc==0x43fab0u&&b.context&&b.use_native_event_control)
    {
        NativeEventCallbackBridge event_bridge{b.context,b.event};
        const driving::PcEventServices event_services{&event_bridge,native_event_callback_invoke,nullptr};
        driving::event_control_43fab0(b.context->event_state,event_services);
    }
    if(pc==0x454670u&&b.context)(void)native_network_tick_454670(*b.context);   // LAN layer tick (pc_network.hpp)
    if(pc==0x480f80u&&b.context)native_records_frame_480f80(*b.context);  // variant-0 record scheduling (480F80)
    if(pc==0x4666a0u&&b.context)native_ghost_frame_4666a0(*b.context);   // TA ghost sub-frame counter 7F9220
    if(pc==0x449050u&&b.context)
        (void)native_runtime_event_function36_display(*b.context);
    return b.upstream.call?b.upstream.call(b.upstream.user,pc,args,count):(outrun::driving::service_hole("native_frame_platform_call","b.upstream.call"),0u);
}
float native_frame_platform_elapsed(void* user,std::uint32_t pc){
    auto& b=*static_cast<NativeFramePlatformBridge*>(user);
    if(b.platform)return runtime_platform_elapsed_ms(*b.platform);
    return b.upstream.elapsed?b.upstream.elapsed(b.upstream.user,pc):(outrun::driving::service_hole("native_frame_platform_elapsed","b.upstream.elapsed"),0.0f);
}
std::int64_t native_frame_platform_clock_query(void* user,std::uint32_t import_address){
    auto& b=*static_cast<NativeFramePlatformBridge*>(user);
    return b.platform?runtime_platform_clock_query(*b.platform,import_address):0;
}
}

std::uint32_t run_native_frame_loop(NativeRuntimeContext& c,const NativeRuntimeLoopServices& s){
    c.loop_setup_state.system_token=c.primary_system_token;
    driving::runtime_loop_setup_417a20(c.loop_setup_state,s.setup);
    c.loop_cleanup_state.handle_8a89f4=c.loop_setup_state.handle_8a89f4;
    c.loop_cleanup_state.handle_8a8a00=c.loop_setup_state.handle_8a8a00;
    c.loop_cleanup_state.object_95b218=c.loop_setup_state.object_95b218;
    c.loop_cleanup_state.optional_7f94e8=c.loop_setup_state.optional_7f94e8;
    c.loop_cleanup_state.handle_89f680=c.loop_setup_state.handle_89f680;
    c.frame_state.primary_system_token=c.primary_system_token;
    c.frame_state.mode_78026c=c.loop_setup_state.mode_78026c;
    c.completed_frames=0u;

    // PC 0x417B4F..0x417BC5 seeds QPF/QPC before entering the loop.  Do the
    // equivalent once when a concrete native platform backend is attached.
    if(s.platform&&runtime_platform_ready(*s.platform)){
        c.timing_state.frequency_8a8c98=runtime_platform_clock_query(*s.platform,0x596104u);
        c.timing_state.current_counter_8a8ca0=runtime_platform_clock_query(*s.platform,0x5960fcu);
        c.timing_state.previous_counter_8a8c90=c.timing_state.current_counter_8a8ca0;
        c.timing_state.accumulator_8a8cd0=0;
        c.timing_state.gate_8a8cc8=1u;
    }

    for(;;){
        auto fs=s.frame;
        driving::PcRuntimeTimingServices417890 native_timing{};
        NativeFramePlatformBridge frame_bridge{};
        const bool have_platform=s.platform&&runtime_platform_ready(*s.platform);
        const bool have_input=s.input&&runtime_input_ready(*s.input);
        if(have_platform||have_input||s.use_native_event_control||s.use_native_mode_control){
            frame_bridge.context=&c;
            frame_bridge.platform=s.platform;
            frame_bridge.input=s.input;
            frame_bridge.event=s.event;
            frame_bridge.mode=s.mode;
            frame_bridge.use_native_event_control=s.use_native_event_control;
            frame_bridge.use_native_mode_control=s.use_native_mode_control;
            frame_bridge.upstream=s.frame;
            fs.user=&frame_bridge;
            fs.call=native_frame_platform_call;
            fs.elapsed=native_frame_platform_elapsed;
        }
        if(have_platform){
            native_timing={&frame_bridge,native_frame_platform_clock_query};
            fs.timing_services=&native_timing;
        }
        fs.timing_state=&c.timing_state;
        // Port enhancement: frames without a tick above 60 Hz (enhancements/frame_rate.hpp).
        static const driving::PcRuntimeTickHooks417c7b display_hooks{true,enhancements::display_before_tick,
            [](const driving::PcRuntimeTimingState417890* t,std::uint32_t updates){
                // The sub-tick time left in the 417890 accumulator, as a blend between the last two ticks.
                const double f=t&&t->frequency_8a8c98>0?double(t->accumulator_8a8cd0)*60.0/double(t->frequency_8a8c98):1.0;
                enhancements::display_after_ticks(float(f),updates);}};
        // 417C7B tests the live [78026C] for its one-tick modes (0x18 / 0x20 /
        // 0x24); this loop keeps the mode of its setup there, which suits one
        // tick per frame but would keep a race at one tick per frame.
        if(enhancements::display_frames()){fs.ticks=&display_hooks;c.frame_state.mode_78026c=c.mode_state.current;}
        const auto result=driving::runtime_frame_step_417c7b(c.frame_state,fs);
        ++c.completed_frames;
        if(result.platform_wait_before_next_frame){
            if(s.platform&&runtime_platform_ready(*s.platform))runtime_platform_wait_pc_guard(*s.platform);
            if(s.platform_wait)s.platform_wait(s.user); // optional observer/legacy hook
        }
        bool keep_running=true;
        if(s.platform&&runtime_platform_ready(*s.platform))keep_running=runtime_platform_poll(*s.platform);
        if(keep_running&&s.continue_running)keep_running=s.continue_running(s.user,c.completed_frames);
        if(!keep_running||(!s.platform&&!s.continue_running))break;
    }
    driving::runtime_loop_cleanup_417970(c.loop_cleanup_state,s.cleanup);
    return 0u;
}

namespace {
struct NativeStartupBridge { NativeRuntimeContext* context{}; const NativeRuntimeServices* services{}; };
std::uint32_t native_startup_bridge_call(void* user,std::uint32_t pc,std::uint32_t a0,std::uint32_t a1,std::uint32_t a2){
    auto& b=*static_cast<NativeStartupBridge*>(user);
    if(pc==0x417b20u&&b.services->use_native_frame_loop)
        return run_native_frame_loop(*b.context,b.services->loop);
    const auto& out=b.services->startup;
    return out.call?out.call(out.user,pc,a0,a1,a2):(outrun::driving::service_hole("native_startup_bridge_call","out.call"),0u);
}
}

std::uint32_t run_native_runtime(NativeRuntimeContext& c,const NativeRuntimeServices& s){
    auto startup=s.startup;
    NativeStartupBridge bridge{&c,&s};
    if(s.use_native_frame_loop){startup.user=&bridge;startup.call=native_startup_bridge_call;}
    if(s.use_native_platform_init){
        startup.platform_init_state=&c.platform_init_state;
        startup.platform_init_services=&s.platform_init;
    }
    return driving::runtime_startup_owner_4176e0(c.event_state,c.event_descriptors,c.event_functions,c.primary_system_token,c.secondary_system_token,startup);
}

// 44DA00(mode, alternate) for the race end / arcade modes.
void native_runtime_load_course(NativeRuntimeContext& c,std::uint32_t mode,std::uint8_t alternate){
    native_game_load_course(c.game_mode,mode,alternate);
}
// 49BA50 (mode 10 attract): the scene owner models 8367C0 and stage 836718 = 5. Its words
// 83679C / 8369AC / 8369B0 / 836798 have no native owner (only 49BA80 reads them, which the
// attract never runs: [830380] is never set).
void native_scene_owner_reset_49ba50(NativeRuntimeContext& c){
    auto& start=c.start_mode;
    start.scene_owner_models_8367c0.fill(0);
    start.scene_owner_stage=5u;
}
// Race end (mode 19 exit 49CFD0, C2C / Heart Attack variants 5/6): the mission data reload
// 496170 / 4E85E0 and its polls 496180 / 4E8620, on the same owners as the frontend boot.
std::uint32_t native_mission_reload_call(NativeRuntimeContext& c,std::uint32_t pc){
    return native_shared_loader_call(&c.event_function36,pc,nullptr,0u);
}
// 499730 (read from the Steam build): the C2C reward of the active license. The mean
// of the seven group grades (4E82A0, 7 = none) reaching 7 gives reward 8; else the
// highest group graded >= 4. A reward whose bit is already in license +11C ([7C24FC])
// returns -1; a new one sets the bit and +3F4 |= 2 ([7C27D4], save needed). 4E86E0 /
// 4961B0 (release the mission data) have no native owner state to free.
std::int32_t native_mission_reward_499730(NativeRuntimeContext& c,bool commit){
    auto& lic=c.event_function36.frontend_profiles.active;
    LicenseProgressTables tables{};
    if(!frontend_license_progress_tables(c.start_mode.scene_owner_race_assets,c.start_mode.scene_owner_assignment_assets,
        c.event_function36.frontend_course_tables,c.mode_state.current,tables))return -1;
    auto grade=[&](unsigned g){int v=7;if(!frontend_group_grade_4e82a0(lic,tables,g,v))v=7;return v;};
    std::int32_t reward=-1;int sum=0;
    for(unsigned g=0;g<7;++g){const int v=grade(g);if(v!=7)sum+=v+1;}
    const int mean=sum/7;
    if(mean!=0&&mean-1==6)reward=8;
    else for(int g=6;g>=0;--g){const int v=grade(unsigned(g));if(v!=7&&v>=4){reward=g;break;}}
    const std::uint32_t mask=1u<<(std::uint32_t(reward)&31u);
    std::uint32_t bits;std::memcpy(&bits,lic.data()+0x11c,4);
    if(bits&mask)return -1;
    if(commit){bits|=mask;std::memcpy(lic.data()+0x11c,&bits,4);lic[0x3f4]|=2u;}
    return reward;
}
bool native_frontend_owner_call(NativeRuntimeContext& c,std::uint32_t pc,std::uint32_t ecx,const std::uint32_t* a,std::size_t n,std::uint32_t& eax){
    return frontend_owner_call_impl(c,pc,ecx,a,n,eax);
}
} // namespace outrun::platform
