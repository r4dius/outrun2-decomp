#include "platform/native_runtime.hpp"
#include "frontend_course_fixture.hpp"
#include <array>
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <filesystem>
#include <chrono>
#include <string>
#include <utility>
#include <vector>
using namespace outrun;
static unsigned checks=0;static void req(bool v,const char* m){++checks;if(!v){std::fprintf(stderr,"FAIL: %s\n",m);std::exit(1);}}
static void put32(std::vector<unsigned char>& b,std::size_t o,std::uint32_t v){b[o]=v;b[o+1]=v>>8;b[o+2]=v>>16;b[o+3]=v>>24;}
int main(){
    std::vector<unsigned char>b(platform::EventMetadataFileSize);const unsigned char magic[8]={'O','R','2','E','V','T','2',0};for(unsigned i=0;i<8;++i)b[i]=magic[i];put32(b,8,platform::EventMetadataVersion);put32(b,12,driving::PcEventSlotCount);put32(b,16,driving::PcEventFunctionTableCount);put32(b,20,platform::NativeModeCount);
    auto desc=[&](std::size_t id,std::size_t field,std::uint32_t v){put32(b,platform::EventMetadataHeaderSize+id*24u+field*4u,v);};
    desc(405,0,0x599808u+405u*0x18u);desc(405,1,0x12345678u);desc(405,3,1u);desc(405,4,36u);desc(405,5,0x20u);
    desc(406,0,0x599808u+406u*0x18u);desc(406,1,0x87654321u);desc(406,5,0x08u);
    const auto fbase=platform::EventMetadataHeaderSize+driving::PcEventSlotCount*24u+36u*20u;put32(b,fbase+0,0x49e490u);put32(b,fbase+4,0x49e4a0u);put32(b,fbase+8,0x49e4b0u);put32(b,fbase+12,0u);put32(b,fbase+16,0x49e4c0u);
    const auto f81base=platform::EventMetadataHeaderSize+driving::PcEventSlotCount*24u+81u*20u;put32(b,f81base+0,0x414940u);put32(b,f81base+4,0x414980u);put32(b,f81base+8,0x414a40u);put32(b,f81base+12,0u);put32(b,f81base+16,0x414bb0u);
    const auto mbase=platform::LegacyEventMetadataFileSize+32u*platform::EventMetadataModeSize;put32(b,mbase+4u,0x49e380u);put32(b,mbase+8u,0x49e3e0u);put32(b,mbase+12u,0x49e410u);
    const auto gamebase=platform::LegacyEventMetadataFileSize+16u*platform::EventMetadataModeSize;put32(b,gamebase+4u,0x499d90u);put32(b,gamebase+8u,0x49c840u);put32(b,gamebase+12u,0x499e50u);
    const auto startbase=platform::LegacyEventMetadataFileSize+13u*platform::EventMetadataModeSize;put32(b,startbase+4u,0x49db20u);put32(b,startbase+8u,0x49dd40u);put32(b,startbase+12u,0x499d80u);
    const std::string path="r089-event-metadata-test.bin";{std::ofstream f(path,std::ios::binary);f.write(reinterpret_cast<const char*>(b.data()),std::streamsize(b.size()));}
    platform::NativeRuntimeContext c;std::string err;req(platform::load_event_metadata_file(path.c_str(),c,&err),"load synthetic event pack");std::remove(path.c_str());
    req(c.event_descriptors[405].startup==1u&&c.event_descriptors[405].function_id==36u,"event405 metadata");req(c.event_functions[36].ctrl_callback==0x49e4a0u,"EvFunc36 metadata");req(c.mode_descriptors[32].init_callback==0x49e380u&&c.mode_descriptors[32].control_callback==0x49e3e0u,"SUMO_FE mode metadata");
    struct Trace{std::vector<std::uint32_t> pc;}t;auto cb=[](void* u,std::uint32_t pc,std::uint32_t,std::uint32_t,std::uint32_t)->std::uint32_t{auto* t=static_cast<Trace*>(u);t->pc.push_back(pc);if(pc==0x40e470u)return 1u;if(pc==0x4177e6u)return 0x955ad800u;return 0u;};
    c.primary_system_token=0x11110000u;c.secondary_system_token=0x22220000u;c.platform_init_state.window_token=0x33330000u;c.platform_init_state.resource_token_740ca0=0x44440000u;
    struct LoopTrace{std::vector<std::uint32_t> setup_pc,frame_pc,cleanup_pc;std::uint32_t elapsed_calls{},waits{};}lt;
    auto setup=[](void* u,std::uint32_t pc,const std::uint32_t*,std::size_t)->std::uint32_t{auto& q=*static_cast<LoopTrace*>(u);q.setup_pc.push_back(pc);if(pc==0x417a6fu)return 0xa001u;if(pc==0x417a93u)return 0xa002u;if(pc==0x417aebu)return 0xa003u;return 0u;};
    auto frame=[](void* u,std::uint32_t pc,const std::uint32_t*,std::size_t)->std::uint32_t{auto& q=*static_cast<LoopTrace*>(u);q.frame_pc.push_back(pc);if(pc==0x455c20u)return 2u;return 0u;};
    auto elapsed=[](void* u,std::uint32_t)->float{auto& q=*static_cast<LoopTrace*>(u);const auto n=q.elapsed_calls++;return (n&1u)?0.010f:10.0f;};
    auto cleanup=[](void* u,std::uint32_t pc,std::uint32_t,std::uint32_t){static_cast<LoopTrace*>(u)->cleanup_pc.push_back(pc);};
    auto more=[](void*,std::uint32_t completed)->bool{return completed<3u;};
    auto wait=[](void* u){++static_cast<LoopTrace*>(u)->waits;};
    c.loop_setup_state.mode_78026c=0x10u;c.loop_setup_state.object_95b218=0x0b001u;c.frame_state.frame_counter_95af0c=4u;
    platform::NativeRuntimeServices sv{};sv.startup={&t,cb};sv.platform_init={&t,cb};sv.use_native_platform_init=true;sv.use_native_frame_loop=true;
    sv.loop.setup={&lt,setup};sv.loop.frame={&lt,frame,elapsed,nullptr,nullptr};sv.loop.cleanup={&lt,cleanup};sv.loop.user=&lt;sv.loop.continue_running=more;sv.loop.platform_wait=wait;
    const auto r=platform::run_native_runtime(c,sv);req(r==0u,"native runtime return");req(c.event_state.slots[405].flags==1u&&c.event_state.slots[405].function_id==36u,"native runtime installed event405");req(c.primary_system_token==0u&&c.secondary_system_token==0u,"native runtime owns/releases system tokens");req(!t.pc.empty()&&t.pc.front()==0x49a650u,"native runtime enters native r090 platform init");req(c.platform_init_state.thread_token==0x955ad800u,"native runtime owns platform thread token");
    req(c.completed_frames==3u&&c.frame_state.frame_counter_95af0c==7u,"r095 portable loop runs three frames");req(lt.waits==3u&&lt.elapsed_calls==6u,"r095 platform wait decision routed outside frame core");req(c.loop_cleanup_state.handle_8a89f4==0u&&c.loop_cleanup_state.handle_8a8a00==0u&&c.loop_cleanup_state.handle_89f680==0u,"r095 loop-owned handles cleaned");req(!lt.setup_pc.empty()&&lt.setup_pc.front()==0x4041e0u&&std::count(lt.frame_pc.begin(),lt.frame_pc.end(),0x453bb0u)==3,"r095 setup/frame services executed");req(lt.cleanup_pc.size()==5u,"r095 cleanup service sequence executed");

    // r096: the same native loop can now obtain QPC/QPF-equivalent timing, the
    // tiny PC wait guard and the exit policy from a concrete platform backend.
    struct FakePlatform{std::uint64_t now{1000u},freq{1000000u};std::uint32_t sleeps{},polls{};}fp;
    auto pticks=[](void* u)->std::uint64_t{return static_cast<FakePlatform*>(u)->now;};
    auto pfreq=[](void* u)->std::uint64_t{return static_cast<FakePlatform*>(u)->freq;};
    auto psleep=[](void* u,std::uint64_t ns){auto& p=*static_cast<FakePlatform*>(u);++p.sleeps;const auto add=(ns*p.freq+999999999ull)/1000000000ull;p.now+=add?add:1u;};
    // Six frames: since the 2026-10-02..04 frontend loader work the owner reaches state 2 after the
    // fifth tick (one frame later than when this fixture was written; not checked against the PC's
    // loader timing), and the sixth runs the first state-2 loop checked below.
    auto ppoll=[](void* u)->bool{auto& p=*static_cast<FakePlatform*>(u);++p.polls;return p.polls<6u;};
    platform::NativeRuntimePlatform native_platform{&fp,pticks,pfreq,psleep,ppoll};
    struct FakeInput{std::uint32_t samples{};}fi;
    auto sample=[](void* u,platform::NativeInputState& state){auto& q=*static_cast<FakeInput*>(u);++q.samples;state.connected=true;state.steering=-37;state.accelerator=255u;state.brake=0u;state.shift_up=q.samples==2u;state.raw_buttons_held=0x200u;};
    platform::NativeRuntimeInput native_input{&fi,sample};
    struct EventTrace{std::vector<std::uint32_t> callback,event;}et2;
    auto invoke2=[](void* u,std::uint32_t callback,std::uint32_t,std::uint32_t event){auto& q=*static_cast<EventTrace*>(u);q.callback.push_back(callback);q.event.push_back(event);};
    platform::NativeRuntimeContext c3;c3.event_descriptors=c.event_descriptors;c3.event_functions=c.event_functions;c3.mode_descriptors=c.mode_descriptors;c3.primary_system_token=0x11110000u;c3.secondary_system_token=0x22220000u;c3.platform_init_state.window_token=0x33330000u;c3.platform_init_state.resource_token_740ca0=0x44440000u;c3.loop_setup_state.mode_78026c=0x10u;c3.loop_setup_state.object_95b218=0x0b001u;platform::LoaderAssetPack assets111{};assets111.records.push_back({0xbau,2u,{1u,2u,3u}});assets111.records.push_back({0xbbu,2u,{4u,5u,6u,7u}});assets111.records.push_back({0x2cu,8u,{8u,9u,10u,11u,12u}});assets111.records.push_back({0x33u,8u,{13u,14u,15u,16u,17u,18u}});assets111.records.push_back({0x48u,8u,{19u,20u,21u,22u,23u,24u,25u}});assets111.records.push_back({0x44u,9u,{26u,27u,28u,29u,30u,31u,32u,33u}});assets111.records.push_back({platform::LoaderAssetSelectTableId,0u,std::vector<std::uint8_t>(platform::LoaderAssetSelectTableBytes,0x5au)});for(std::uint32_t lane=0u;lane<platform::LoaderAssetFrontendScriptCount;++lane)assets111.records.push_back({platform::LoaderAssetFrontendScriptBaseId+lane,0u,frontend_fixture::script(lane)});req(platform::native_runtime_attach_loader_assets(c3,assets111),"r111 attach all 71 loader assets");
    auto races111=frontend_fixture::races();
    req(platform::native_runtime_attach_race_assets(c3,races111),"validated Races source for native bootstrap");
    req(c3.event_function36.frontend_sprites.bind_timing(0x44u,std::vector<platform::FrontendSpriteTiming>(224,{52.0f,60.0f})),"synthetic authored sprite timing for runtime fixture");
    req(c3.event_function36.frontend_sprites.bind_timing(0x2cu,std::vector<platform::FrontendSpriteTiming>(64,{52.0f,60.0f})),"shared command sprite timing fixture");
    struct ModeTrace{std::vector<std::uint32_t> callback,mode,phase;}mt;auto mode_invoke=[](void* u,std::uint32_t callback,std::uint32_t mode,platform::NativeModePhase phase){auto& q=*static_cast<ModeTrace*>(u);q.callback.push_back(callback);q.mode.push_back(mode);q.phase.push_back(static_cast<std::uint32_t>(phase));};
    LoopTrace lt2;platform::NativeRuntimeServices sv2{};sv2.startup={&t,cb};sv2.platform_init={&t,cb};sv2.use_native_platform_init=true;sv2.use_native_frame_loop=true;sv2.loop.setup={&lt2,setup};sv2.loop.frame={&lt2,frame,elapsed,nullptr,nullptr};sv2.loop.cleanup={&lt2,cleanup};sv2.loop.user=&lt2;sv2.loop.platform_wait=wait;sv2.loop.platform=&native_platform;sv2.loop.input=&native_input;sv2.loop.event={&et2,invoke2,nullptr};sv2.loop.mode={&mt,mode_invoke};sv2.loop.use_native_event_control=true;sv2.loop.use_native_mode_control=true;req(platform::native_runtime_request_mode(c3,32u),"r105 request SUMO_FE mode");req(!platform::native_runtime_request_mode(c3,platform::NativeModeCount),"r105 reject invalid mode request");
    req(platform::run_native_runtime(c3,sv2)==0u,"r096 native platform runtime return");req(c3.completed_frames==6u,"r113 applet-style platform exit policy controls frame count");req(native_platform.wait_calls==6u&&native_platform.poll_calls==6u&&fp.sleeps>=6u,"r113 platform wait/poll routed through backend");req(lt2.waits==6u&&lt2.elapsed_calls==0u,"r113 platform timing replaces legacy elapsed callback while preserving wait observer");req(c3.timing_state.frequency_8a8c98==1000000&&c3.frame_state.slow_frame_count_8a8cc4==0u,"r096 platform QPF/QPC timing state");req(fp.now>=1051u,"r113 wait reaches PC 1/60-millisecond guard");
    req(native_input.sample_calls==6u&&fi.samples==6u,"r113 ReadIO samples native input once per update");req(c3.input_state.connected&&c3.input_state.steering==-37&&c3.input_state.accelerator==255u&&!c3.input_state.shift_up,"r113 native input snapshot retained");
    req(et2.callback.size()==14u&&std::count(et2.event.begin(),et2.event.end(),405u)==7&&std::count(et2.event.begin(),et2.event.end(),406u)==7,"r118 EventControl runs frontend owner and PC auxiliary event in native frame path");req(et2.callback[0]==0x49e490u&&et2.callback[1]==0x49e4a0u&&et2.callback[2]==0x414940u&&et2.callback[3]==0x414980u&&c3.event_state.slots[405].flags==2u&&c3.event_state.slots[406].flags==2u,"r118 event405/event406 init-to-control lifecycle");
    req(mt.callback.size()==7u&&mt.callback[6]==0x49e3e0u&&mt.callback[0]==0x49e380u&&mt.callback[1]==0x49e3e0u&&mt.callback[2]==0x49e3e0u&&mt.callback[3]==0x49e3e0u&&mt.callback[4]==0x49e3e0u&&mt.callback[5]==0x49e3e0u,"r113 ModeControl dispatches SUMO_FE init then controls");req(c3.mode_state.dispatcher_calls==6u&&c3.mode_state.init_callbacks==1u&&c3.mode_state.control_callbacks==6u&&c3.mode_state.exit_callbacks==0u,"r113 native ModeControl lifecycle counters");req(c3.mode_state.current==32u&&c3.mode_state.previous==0u&&c3.mode_state.transition_pending==0u,"r105 native mode state transition");req(c3.mode_state.sumo_fe_event_setup_calls==1u,"r118 SUMO_FE installs event406/function81");
    req(c3.event_function36.initialized&&c3.event_function36.init_calls==1u,"r104 event36 owns and initializes native object");
    req(c3.event_function36.state2_last_handle==0u,"no unsolicited profile overlay during bootstrap");
    req(c3.event_function36.control_calls==6u&&c3.event_function36.control_true_returns==6u,"r113 event36 executes native control owner per update");
    req(c3.event_function36.display_calls==6u&&c3.event_function36.destroy_calls==0u,"r113 event36 executes native display per rendered frame");
    {driving::Bytes event36(c3.event_function36.object.data(),c3.event_function36.object.size());driving::Bytes loader(c3.event_function36.loader_83039c.data(),c3.event_function36.loader_83039c.size());req(event36.u8(0xd94u)==0u&&event36.f32(0xd9cu)>0.0f,"welcome 4C5210 state 0 hides the UI (4413F0(0)) until a start press; owner timer kept");req(c3.mode_state.sumo_fe_reset_calls==1u&&!c3.mode_state.sumo_fe_reset_pending&&event36.u32(0x218u)==2u&&event36.u32(0)==14u,"r112 native loader reaches stage14 and owner state2");req(c3.event_function36.loader_begin_calls==1u&&c3.event_function36.loader_request_calls==2u&&c3.event_function36.loader_last_request==0xbbu&&c3.event_function36.loader_last_request_mode==2u,"r106 native loader request trace");req(loader.u8(0u)==1u&&loader.u32(0x10u)==11u&&loader.u32(0x14u)==30u&&loader.u32(0x18u)==0u,"r106 native loader global state");req(c3.event_function36.loader_ready_count==2u&&c3.event_function36.loader_ready_bytes==7u&&c3.event_function36.loader_resource_pending==0u&&c3.event_function36.loader_stage4_passes==1u,"r107 native BA/BB readiness and stage4 pass");req(c3.event_function36.shared_loader.stage_83db18==6u&&c3.event_function36.shared_resource_requests==3u&&c3.event_function36.shared_last_resource==0x48u&&c3.event_function36.shared_last_mode==8u&&c3.event_function36.shared_resource_pending==0u,"r108 native shared loader reaches terminal state");req(c3.event_function36.shared_ready_count==3u&&c3.event_function36.shared_ready_bytes==18u&&c3.event_function36.shared_release_calls==3u,"r108 shared group assets owned and released");req(c3.event_function36.shared_table_reset_calls==1u&&c3.event_function36.shared_finalize_calls==1u&&c3.event_function36.shared_last_result==1u,"r108 shared completion callbacks");req(c3.event_function36.frontend_resource_requests==1u&&c3.event_function36.frontend_ready_count==1u&&c3.event_function36.frontend_ready_bytes==8u&&c3.event_function36.frontend_resource_pending==0u&&c3.event_function36.frontend_release_calls==1u,"r110 frontend 44/9 asset owned and released");req(c3.event_function36.select_table_open_calls==1u&&c3.event_function36.select_table_read_calls==1u&&c3.event_function36.select_table_close_calls==1u&&c3.event_function36.select_table_bytes==platform::LoaderAssetSelectTableBytes,"r110 select table exact fread lifecycle");req(c3.event_function36.frontend_init_calls==2u&&c3.event_function36.frontend_async_polls==1u&&c3.event_function36.frontend_bulk_polls==1u&&c3.event_function36.loader_stage12_passes==1u,"r111 frontend bulk completion reaches stage12");req(c3.event_function36.frontend_bulk_requests==64u&&c3.event_function36.frontend_bulk_ready_count==64u&&c3.event_function36.frontend_bulk_ready_bytes==frontend_fixture::TotalBytes&&c3.event_function36.frontend_bulk_pending==0u&&c3.event_function36.frontend_bulk_last_lane==63u,"r111 all 64 synthetic frontend scripts ready");req(c3.event_function36.frontend_bulk_format_calls==60u&&c3.event_function36.frontend_bulk_bind_calls==4u,"r111 four fixed binds and sixty formatted paths");req(c3.event_function36.loader_stage12_body_calls==1u&&c3.event_function36.loader_stage14_passes==1u&&event36.u32(0x484u)==1u&&event36.u32(0x284u)==c3.event_function36.stage12_last_handle,"r112 stage12 callback inserted before stage14");req(c3.event_function36.stage12_global_init_calls==1u&&c3.event_function36.stage12_embedded_init_calls==1u&&c3.event_function36.stage12_embedded_reset_calls==1u&&c3.event_function36.stage12_dispatch_calls==1u,"r112 stage12 initialization order counters");req(c3.event_function36.stage12_factory_allocate_calls==1u&&c3.event_function36.stage12_factory_construct_calls==1u&&c3.event_function36.stage12_ready_checks==1u&&c3.event_function36.stage12_transition_init_calls==1u,"r112 native callback factory and transition init");req(c3.event_function36.stage12_scene_init_calls==1u&&c3.event_function36.stage12_manager_reset_calls==1u&&c3.event_function36.stage12_audio_calls==0u&&c3.event_function36.stage12_optional_calls==0u,"r112 default branch final service sequence");req(c3.event_function36.state2_frames==1u&&c3.event_function36.state2_transition_ticks==1u&&c3.event_function36.state2_ui_state_ticks==1u&&c3.event_function36.state2_ui_ticks==1u&&c3.event_function36.state2_embedded_ticks==1u,"r113 first half of active owner state2 loop");req(c3.event_function36.state2_gate_ticks==1u&&c3.event_function36.state2_runtime_transition_ticks==1u&&c3.event_function36.state2_queue_ticks==1u&&c3.event_function36.state2_shutdown_ticks==1u,"r113 second half of active owner state2 loop");req(c3.event_function36.state2_ui_open_calls==1u&&c3.event_function36.state2_ui_open_last_key==0u,"r114 enters 4447D0 with root state");req(c3.event_function36.state2_ui_valid_calls==1u&&c3.event_function36.state2_ui_invalid_calls==0u&&c3.event_function36.state2_ui_last_token==0x00440094u&&c3.event_function36.state2_ui_configure_calls==1u,"r114 valid 4447D0 configure path");req(c3.event_function36.state2_ui_create3_calls==0u&&c3.event_function36.state2_ui_create5_calls==1u&&c3.event_function36.state2_ui_property_calls==1u&&c3.event_function36.state2_ui_finalize_calls==1u&&c3.event_function36.state2_ui_release_calls==0u,"r114 resource create/property/finalize lifecycle");req(event36.u32(0xcfcu)==0xffffffffu&&c3.event_function36.frontend_welcome_scene_token==0x4400d8u,"welcome replaces the background with its authored 4400D8 resource");req(c3.event_function36.state2_callback_dispatch_calls==0u&&c3.event_function36.frontend_handle_count==1u&&event36.u32(0x488u)==0u,"idle feature gate leaves the welcome owner active");req(c3.event_function36.state2_selector_calls==1u&&c3.event_function36.state2_selector_child_state==0u&&c3.event_function36.frontend_welcome_control_calls==1u,"original welcome virtual control runs");req(c3.event_function36.state2_transition_activations==0u&&c3.event_function36.state2_gate_configure_calls==0u&&c3.event_function36.state2_gate_action_calls==0u,"r114 idle active-loop side boundaries remain quiet");}
    {
        driving::Bytes owner(c3.event_function36.object.data(),c3.event_function36.object.size());
        auto commands=owner.sub(0x51c,0x68c);
        req(c3.event_function36.frontend_ui_missing_pc==0,"welcome owner commands have no missing graphics service");
        req(commands.u8(0)==0,"welcome hides owner command icons");
        for(unsigned slot=0;slot<4;++slot){
            req(commands.u32(8+commands.u32(0x408)*0x20+slot*4)==~0u,"welcome forwards all original unset keys");
            req(commands.u32(0x414+slot*0xa0)==~0u&&commands.f32(0x470+slot*0xa0)==1,"owner uses fully constructed command resources");
        }
    }
    for(unsigned i=0;i<3u;++i)req(platform::native_runtime_event_function36_invoke(c3,0x49e4a0u,405u),"r114 repeated state2 control");
    req(c3.event_function36.state2_frames==4u&&c3.event_function36.state2_ui_valid_calls==1u&&c3.event_function36.state2_ui_open_calls==1u,"r114 committed resource suppresses duplicate UI open");
    req(c3.event_function36.state2_ui_create5_calls==1u&&c3.event_function36.state2_ui_property_calls==1u,"r114 one-shot UI creation/property");
    req(c3.event_function36.state2_ui_finalize_calls==1u&&c3.event_function36.state2_ui_release_calls==0u,"r114 live UI resource retained without premature release");
    auto root=driving::Bytes(c3.event_function36.object.data(),c3.event_function36.object.size());
    auto welcome=driving::Bytes(c3.event_function36.frontend_root_object.data(),c3.event_function36.frontend_root_object.size());
    // Runtime-only fixture. Retail PCM and actual mixed device submissions are
    // covered by frontend_menu_render and switch_audio_queue respectively.
    std::vector<unsigned> effect_ids;
    c3.event_function36.frontend_effect_user=&effect_ids;
    c3.event_function36.frontend_effect=[](void* p,unsigned id){static_cast<std::vector<unsigned>*>(p)->push_back(id);return true;};
    // Queue A while stage zero still performs its original first tick.
    welcome.put32(0x740u,0u);
    c3.input_state={};c3.input_state.menu_confirm=true;
    req(platform::native_runtime_event_function36_invoke(c3,0x49e4a0u,405u)&&
        c3.event_function36.frontend_pending_input_action==0u&&welcome.u32(0x740u)==1u,
        "welcome first tick does not discard early confirm");
    c3.input_state={};
    req(c3.event_function36.frontend_movie_request==1&&c3.event_function36.frontend_movie_generation>0u,
        "welcome original 414790 request starts the title video service");
    req(platform::native_runtime_event_function36_invoke(c3,0x49e4a0u,405u)&&
        welcome.u32(0x740u)==2u&&root.u32(0x484u)==1u,
        "welcome confirm waits for the original separate profile-load tick");
    req(c3.event_function36.frontend_movie_request==-1,"welcome confirm stops movie before profile loading");
    req(effect_ids==std::vector<unsigned>{64},"welcome dispatches the original MENU confirm sample");
    const auto history=root.u8(0x220u);
    for(unsigned i=0;i<4;++i)
        req(platform::native_runtime_event_function36_invoke(c3,0x49e4a0u,405u),"welcome profile request");
    req(welcome.u32(4u)==21u&&c3.event_function36.frontend_last_missing_key==21u&&
        root.u32(0x484u)==1u&&root.u8(0x220u)==history&&root.u32(0x488u)==0u,
        "no profiles requests original key21 without silently selecting a guest or leaking history");
    // Exercise the production file service with a private, PC-format fixture.
    // Never point this test at the user's save directory.
    const auto save_fixture=std::filesystem::temp_directory_path()/
        ("or2-native-save-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    std::filesystem::create_directories(save_fixture/"SaveGame");
    {std::vector<unsigned char> license(platform::PcLicenseBytes+4u);put32(license,0u,platform::PcLicenseBytes);
        license[4u+0x3f4u]=1u;std::ofstream file(save_fixture/"SaveGame/License1.dat",std::ios::binary);
        file.write(reinterpret_cast<const char*>(license.data()),license.size());req(bool(file),"write private PC license fixture");}
    platform::RetailAssetStore profile_store{};profile_store.root=save_fixture.string();
    c3.event_function36.retail_assets=&profile_store;
    c3.event_function36.frontend_profiles.queried=false;
    req(platform::native_runtime_event_function36_invoke(c3,0x49e4a0u,405u)&&
        c3.event_function36.frontend_first_menu_opens==1u&&root.u32(0x484u)==2u&&
        c3.race.manager.state.over_state_7d38f0==0u&&root.u32(0x488u)==0u,
        "loaded profile follows welcome into original main choice without hotkey overlay");
    req(c3.event_function36.frontend_profiles.loaded_count==1u&&c3.event_function36.frontend_profiles.loaded[0],
        "416380 adapter reads PC-format license from the production retail path");
    c3.event_function36.retail_assets=nullptr;
    std::filesystem::remove_all(save_fixture);
    auto first_menu=driving::Bytes(c3.event_function36.frontend_first_menu_object.data(),c3.event_function36.frontend_first_menu_object.size());
    c3.input_state={};c3.input_state.frontend.feature_mask=0x2000;
    req(platform::native_runtime_event_function36_invoke(c3,0x49e4a0u,405u)&&first_menu.u32(0xe8u)==1u,
        "main menu right input reaches active child");
    c3.input_state={};c3.input_state.frontend.feature_mask=0x1000;
    req(platform::native_runtime_event_function36_invoke(c3,0x49e4a0u,405u)&&first_menu.u32(0xe8u)==0u,
        "main menu left input reaches active child");
    c3.input_state={};c3.input_state.frontend.feature_mask=4;
    req(platform::native_runtime_event_function36_invoke(c3,0x49e4a0u,405u)&&root.u32(0x484u)==2u,
        "choice confirmation preserves one-tick action latch");
    c3.input_state={};
    req(platform::native_runtime_event_function36_invoke(c3,0x49e4a0u,405u)&&root.u32(0x484u)==3u&&
        c3.event_function36.frontend_second_menu_opens==1u,
        "generic owner action pushes key2");
    req(c3.event_function36.frontend_records.reset_calls==1&&c3.event_function36.frontend_records.missing_pc==0,
        "key2 invokes the connected PC record manager reset");
    c3.input_state={};c3.input_state.frontend.feature_mask=8;
    req(platform::native_runtime_event_function36_invoke(c3,0x49e4a0u,405u),"second menu cancel latch");
    c3.input_state={};
    req(platform::native_runtime_event_function36_invoke(c3,0x49e4a0u,405u)&&root.u32(0x484u)==2u&&
        first_menu.u8(0x14cu)==0u&&c3.event_function36.frontend_stack_pops==1u,
        "cancel pops original owner stack and rearms previous menu");
    c3.input_state={};c3.input_state.frontend.feature_mask=4;
    req(platform::native_runtime_event_function36_invoke(c3,0x49e4a0u,405u),"reopen second menu confirmation");
    c3.input_state={};
    req(platform::native_runtime_event_function36_invoke(c3,0x49e4a0u,405u)&&root.u32(0x484u)==3u&&
        c3.event_function36.frontend_second_menu_opens==2u,
        "repeated push/pop uses correct factory despite equal-sized menu objects");
    for(unsigned tick=0u;tick<4u;++tick){
        c3.input_state={};c3.input_state.frontend.feature_mask=(tick%2u)==0u?8u:0u;
        req(platform::native_runtime_event_function36_invoke(c3,0x49e4a0u,405u),"cancel back through main choice to welcome");
    }
    req(root.u32(0x484u)==1u&&welcome.u32(0x740u)==0u&&
        c3.event_function36.frontend_welcome_scene_token==0x4400d8u,
        "original action 3 unwinds to welcome and restores its authored resource");
    c3.input_state={};c3.input_state.menu_preview=true;req(platform::native_runtime_event_function36_invoke(c3,0x49e4a0u,405u),"r154 ignores legacy diagnostic preview input");
    req(!c3.event_function36.frontend_menu_committed&&
        c3.event_function36.frontend_menu_preview_requests==0u&&
        driving::Bytes(c3.event_function36.object.data(),c3.event_function36.object.size()).u32(0x21cu)!=16u,
        "r154 X cannot bypass the original frontend and post GAME");
    // Keep the mode-lifecycle regression independent from controller input.
    // This direct mailbox write is a host fixture, not a Switch control path.
    driving::object_set_token_440de0(
        driving::Bytes(c3.event_function36.object.data(),c3.event_function36.object.size()),16u);
    c3.input_state={};req(platform::native_runtime_mode_control(c3,{&mt,mode_invoke}),"r118 SUMO_FE control requests GAME");
    req(c3.mode_state.transition_pending==1u&&c3.mode_state.requested==16u&&c3.mode_state.sumo_fe_game_requests==1u&&c3.mode_state.sumo_fe_event_close_calls==1u&&c3.event_state.slots[406].flags==4u,"r118 SUMO_FE exits and schedules event406 destroy");
    req(c3.mode_state.sumo_fe_owner_command_polls>=1u&&c3.mode_state.sumo_fe_last_owner_command==16u&&driving::Bytes(c3.event_function36.object.data(),c3.event_function36.object.size()).u32(0x21cu)==0xffffffffu,"SUMO_FE consumes and clears PC owner mode-command mailbox");
    req(platform::native_runtime_mode_control(c3,{&mt,mode_invoke}),"r118 enter PC GAME mode");
    req(c3.mode_state.current==16u&&c3.mode_state.previous==32u&&c3.mode_state.transition_pending==0u,"r118 mode table commits SUMO_FE to GAME");
    req(c3.game_mode.active&&c3.game_mode.init_calls==1u&&c3.game_mode.control_calls==1u&&c3.game_mode.previous_mode==32u,"r118 GAME callbacks execute natively");
    req(c3.game_mode.event_resume_calls==0u&&c3.game_mode.empty_440d90_calls==1u&&c3.game_mode.global_effect_calls==1u&&c3.game_mode.global_effect_last==0x82u&&c3.game_mode.timing_reset_calls==1u,"r118 GAME init follows non-RESTART branch");
    req(c3.game_mode.owner_command_polls==1u&&c3.game_mode.menu_state_polls==1u&&c3.mode_state.init_callbacks==2u&&c3.mode_state.exit_callbacks==1u,"r118 GAME control owns transition polls");
    platform::NativeRuntimeContext c4{};c4.mode_descriptors=c.mode_descriptors;
    for(auto& slot:c4.event_state.slots)slot.flags=0x10u;
    c4.mode_state.current=15u;req(platform::native_runtime_request_mode(c4,16u),"r118 request RESTART to GAME");
    req(platform::native_runtime_mode_control(c4,{}),"r118 RESTART predecessor enters GAME");
    req(c4.game_mode.event_resume_calls==9u&&c4.game_mode.event_resume_slots==326u&&c4.game_mode.empty_440d90_calls==0u,"r118 GAME init owns all recovered resume ranges");
    req((c4.event_state.slots[0x17fu].flags&0x10u)==0u&&(c4.event_state.slots[8u].flags&0x10u)==0u&&(c4.event_state.slots[0x18du].flags&0x10u)==0u,"r118 GAME resume ranges reach boundary slots");
    // START is now a distinct PC mode.  Its owned resource may be present,
    // but the two source readiness predicates must be explicitly satisfied.
    platform::NativeRuntimeContext c5{};c5.mode_descriptors=c.mode_descriptors;
    c5.event_function36.loader_assets=&assets111;
    assets111.records.push_back({platform::LoaderAssetStartLoadingId,2u,{0x78u,0xdau,1u}});
    assets111.records.push_back({platform::LoaderAssetStartVersusId,2u,{0x78u,0xdau,2u}});
    assets111.records.push_back({platform::LoaderAssetGameSpritesId,8u,{0x78u,0xdau,3u,4u}});
    req(platform::native_runtime_request_mode(c5,13u)&&platform::native_runtime_mode_control(c5,{}),"START enters PC mode 13");
    req(c5.mode_state.current==13u&&c5.start_mode.active&&c5.start_mode.stage==0u&&c5.start_mode.event_open_count==1u&&c5.game_mode.start_countdown==0x168,"START init owns event 1 and course stage");
    c5.start_mode.stage=1u;
    req(platform::native_runtime_mode_control(c5,{})&&c5.start_mode.stage==2u&&c5.start_mode.resource_id==platform::LoaderAssetStartLoadingId&&c5.start_mode.resource_ready==1u&&c5.start_mode.event_open_count==1u,"START requests retail 2F/2 and waits for both gates");
    auto ready=[](void*,std::uint32_t pc)->bool{return pc==0x42df90u||pc==0x4299a0u;};
    req(platform::native_runtime_mode_control(c5,{nullptr,nullptr,ready})&&c5.start_mode.stage==3u&&c5.start_mode.event_open_count==2u&&(c5.event_state.slots[0x185u].flags&1u)!=0u,"START opens original event 185 only after both gates");
    req(platform::native_runtime_mode_control(c5,{nullptr,nullptr,ready})&&c5.start_mode.stage==3u&&c5.mode_state.current==13u,"unported START stage 3 cannot fake a GAME transition");
    auto start_service=[](void*,std::uint32_t pc,const std::uint32_t*,std::size_t,
                          std::uint32_t& result)->bool{
        switch(pc){
            case 0x456d60u:case 0x55a930u:case 0x427db0u:
            case 0x440380u:case 0x4962a0u:case 0x4f0d10u:result=0u;return true;
            case 0x49ba80u:case 0x4557f0u:result=1u;return true;
            default:return false;
        }
    };
    const platform::NativeModeServices start_services{nullptr,nullptr,ready,start_service};
    req(platform::native_runtime_mode_control(c5,start_services)&&c5.start_mode.stage==4u,
        "START stage 3 follows PC loading-scene branch");
    req(platform::native_runtime_mode_control(c5,start_services)&&c5.start_mode.stage==59u,
        "START stage 4/5 respects course/vehicle service predicates");
    req(platform::native_runtime_mode_control(c5,start_services)&&c5.start_mode.stage==60u&&
        c5.start_mode.stage60_phase==1u&&c5.start_mode.race_event_count==9u,
        "START stage 59/60 installs nine original race-event owners");
    req((c5.event_state.slots[0x167u].flags&1u)!=0u&&
        (c5.event_state.slots[0x181u].flags&1u)!=0u&&
        (c5.event_state.slots[0x16au].flags&1u)!=0u&&
        c5.mode_state.current==13u&&c5.mode_state.transition_pending==0u,
        "START race events are real scheduler slots without a fake GAME transition");
    auto complete_start_service=[](void*,std::uint32_t pc,const std::uint32_t* args,
                                   std::size_t count,std::uint32_t& result)->bool{
        result=0u;
        switch(pc){
            case 0x4962d0u:case 0x4999a0u:case 0x4957f0u:
            case 0x48b310u:case 0x495490u:case 0x55a930u:
            case 0x4518a0u:case 0x456d60u:case 0x44c2d0u:
            case 0x4999f0u:case 0x428600u:case 0x43f9c0u:
            case 0x43f980u:return true;
            case 0x48b140u:result=1u;return true; // authored route 12
            case 0x4871a0u:return count==1u&&args[0]==12u;
            case 0x45a920u:case 0x43fa90u:result=1u;return true;
            case 0x42dfb0u:case 0x4299c0u:return count==1u;
            case 0x44fce0u:return count==1u&&args[0]==0u;
            default:return false;
        }
    };
    const platform::NativeModeServices complete_start{nullptr,nullptr,ready,
                                                           complete_start_service};
    req(platform::native_runtime_mode_control(c5,complete_start)&&
        c5.start_mode.stage==64u&&c5.start_mode.stage60_route==12u&&
        c5.start_mode.race_event_count==11u&&
        (c5.event_state.slots[0x16bu].flags&1u)!=0u&&
        (c5.event_state.slots[0x16cu].flags&1u)!=0u,
        "START completes conditional race events and authored route selection");
    req(platform::native_runtime_mode_control(c5,start_services)&&
        c5.start_mode.stage==64u&&c5.start_mode.last_missing_service==0x45a920u&&
        c5.mode_state.transition_pending==0u,
        "START final resource predicate cannot be skipped");
    req(platform::native_runtime_mode_control(c5,complete_start)&&
        c5.start_mode.stage==65u&&c5.start_mode.game_requests==1u&&
        c5.mode_state.requested==16u&&c5.mode_state.transition_pending==1u&&
        c5.start_mode.race_event_count==11u,
        "START requests GAME only after all original event and service gates");
    c3.start_mode.resource_id=platform::LoaderAssetStartLoadingId;
    c3.start_mode.resource_ready=1u;
    c3.start_mode.resource_bytes=3u;
    req(platform::native_start_owned_resource_ready(c3,0x42df90u)&&
        platform::native_start_owned_resource_ready(c3,0x4299a0u)&&
        !platform::native_start_owned_resource_ready(c3,0x4557f0u),
        "START source-backed pool gates require the loaded retail resource and frontend bootstrap");
    platform::NativeRuntimeContext access{};
    std::uint32_t access_value=0xffffffffu;
    access.game_mode.game_variant=1u;
    access.start_mode.mode_countdown_780250=2;
    req(platform::native_start_owned_call(access,0x456d60u,nullptr,0u,access_value)&&
        access_value==0u&&
        platform::native_start_owned_call(access,0x427db0u,nullptr,0u,access_value)&&
        access.start_mode.render_mode_754b0c==4u&&
        platform::native_start_owned_call(access,0x55a930u,nullptr,0u,access_value)&&
        access_value==0u,
        "START source-backed loading byte, default scene mode and manager getter");
    req(platform::native_start_owned_call(access,0x43fa90u,nullptr,0u,access_value)&&
        access_value==0u&&
        platform::native_start_owned_call(access,0x43fa90u,nullptr,0u,access_value)&&
        access_value==1u&&access.start_mode.mode_countdown_780250==0,
        "START original mode countdown reaches its 43FA90 transition gate");
    const std::uint32_t variant7=7u;
    req(platform::native_start_owned_call(access,0x4999a0u,&variant7,1u,access_value)&&
        access_value==1u&&
        platform::native_start_owned_call(access,0x49ba80u,nullptr,0u,access_value)&&
        access_value==0u&&access.start_mode.scene_owner_stage==8u,
        "START mode classifier and stage-5 scene owner return zero until ready");
    platform::NativeRuntimeContext c6{};c6.mode_descriptors=c.mode_descriptors;
    c6.event_function36.loader_assets=&assets111;
    c6.event_function36.loader_stage14_passes=1u;
    c6.event_function36.shared_loader.stage_83db18=6u;
    c6.event_function36.shared_ready_count=3u;
    c6.event_function36.frontend_ready_count=1u;
    auto owned_ready=[](void* user,std::uint32_t pc){
        return platform::native_start_owned_resource_ready(
            *static_cast<platform::NativeRuntimeContext*>(user),pc);
    };
    auto owned_call=[](void* user,std::uint32_t pc,const std::uint32_t* args,
                       std::size_t count,std::uint32_t& value){
        return platform::native_start_owned_call(
            *static_cast<platform::NativeRuntimeContext*>(user),pc,args,count,value);
    };
    const platform::NativeModeServices owned_services{&c6,nullptr,owned_ready,owned_call};
    req(platform::native_runtime_request_mode(c6,13u)&&
        platform::native_runtime_mode_control(c6,owned_services),
        "START native adapter enters mode 13 without fabricating a missing course pack");
    c6.start_mode.stage=1u;
    req(platform::native_runtime_mode_control(c6,owned_services)&&
        c6.start_mode.stage==4u&&c6.start_mode.event_open_count==2u,
        "START native adapter passes held-resource pools and source-backed loading byte");
    // Stage 5 hands over to the START scene owner 49BA80; its stages are
    // verified against the original by the scene-owner oracle and end to end
    // by the host_nro menu-to-GAME run.
    req(platform::native_runtime_mode_control(c6,owned_services)&&
        c6.start_mode.stage==5u&&c6.start_mode.scene_owner_calls>=1u&&
        c6.mode_state.transition_pending==0u,
        "START stage 5 calls the scene owner and does not request GAME early");
    driving::object_set_token_440de0(driving::Bytes(c3.event_function36.object.data(),c3.event_function36.object.size()),32u);
    req(platform::native_runtime_mode_control(c3,{&mt,mode_invoke})&&c3.mode_state.transition_pending==1u&&c3.mode_state.requested==32u&&c3.game_mode.last_owner_command==32u,"GAME consumes PC owner command and requests frontend mode");
    req(driving::Bytes(c3.event_function36.object.data(),c3.event_function36.object.size()).u32(0x21cu)==0xffffffffu&&c3.game_mode.exit_calls==1u,"GAME owner command is one-shot and exits current mode");
    c3.mode_state.current=32u;
    c3.mode_state.transition_pending=0u; // return-to-frontend fixture
    c3.event_function36.frontend_menu_committed=false;
    c3.input_state={};c3.input_state.menu_confirm=true;
    req(platform::native_runtime_event_function36_invoke(c3,0x49e4a0u,405u)&&
        c3.start_mode.original_route_requests==0u&&
        driving::Bytes(c3.event_function36.object.data(),c3.event_function36.object.size()).u32(0x21cu)!=13u,
        "unported key-1 controller cannot silently jump to START");
    auto title_route=c3;
    title_route.mode_state.current=16u;
    title_route.input_state={};
    auto title_root=driving::Bytes(title_route.event_function36.object.data(),
                                   title_route.event_function36.object.size());
    title_root.put32(0x0218u,3u);
    title_root.put32(0x0484u,0u);
    title_root.put32(0x0488u,0u);
    title_root.put8(0x048cu,0u);
    title_root.put8(0x0494u,0u);
    title_root.put32(0x0518u,0u);
    title_route.race.manager.state.over_state_7d38f0=0u;
    req(platform::native_runtime_event_function36_invoke(title_route,0x49e4a0u,405u)&&
        title_route.event_function36.title_gate_ticks==1u&&
        title_route.event_function36.title_construct_calls==0u,
        "PC state-3 title gate stays shut without the real player feature bit");
    // In the race (mode 16) the player feature bit is the start key (frontend feature 1), not the
    // connected pad alone (native_runtime 49E4A0, 2026-10-04 pause-menu fix).
    title_route.input_state.connected=true;title_route.input_state.frontend.feature_mask=1u;
    req(platform::native_runtime_event_function36_invoke(title_route,0x49e4a0u,405u)&&
        title_route.event_function36.title_construct_calls==1u&&
        title_route.event_function36.title_init_calls==1u&&
        title_route.event_function36.title_pause_calls==2u&&   // 443EB0 (440930 at 443F66) then 4D5D00 (440930 at 4D5D9A), as in the EXE
        title_route.event_function36.frontend_handle_count==5u&&
        driving::Bytes(title_route.event_function36.title_owner_object.data(),
                       title_route.event_function36.title_owner_object.size()).u32(0u)==0x5ccbb4u,
        "PC state-3 gate constructs and initializes key 44 with scheduler pause ownership");
    req(title_route.event_state.pause_depth==2u&&   // [7A0DB0] after both 440930 calls; 4D5DB0 and the owner close path release both
        title_route.start_mode.game_flag_780248==1u&&
        title_route.event_function36.title_pause_flag_95b214==1u&&
        title_route.event_function36.title_pause_flag_7d2614==1u&&
        title_route.event_function36.title_owner_globals.pause_flag==1u&&
        title_route.event_function36.title_owner_globals.scene_ids[0]==14u,
        "key-44 variant-1 initialization propagates the original three pause flags and scene choice");
    req(title_route.event_function36.title_control_calls>0u&&
        title_route.event_function36.title_controller_ticks>0u&&
        title_route.event_function36.title_initial_ui_calls==0u&&
        title_route.event_function36.title_last_missing_service==0x4d5e40&&
        driving::Bytes(title_route.event_function36.title_owner_object.data(),
                       title_route.event_function36.title_owner_object.size()).u32(0x9acu)==0u,
        "key-44 cannot advance using metadata-only UI without bound retail widgets");
    req(platform::native_runtime_event_function36_invoke(title_route,0x49e4a0u,405u)&&
        title_route.event_function36.title_last_missing_service==0x4d5e40u,
        "missing title UI remains at its real initialization boundary");
    title_route.input_state.frontend.feature_mask=0x400;
    title_route.input_state.frontend.axes[0]=-65;
    req(platform::native_runtime_event_function36_invoke(title_route,0x49e4a0u,405u)&&
        title_route.event_function36.title_last_missing_service==0x4d5e40&&
        driving::Bytes(title_route.event_function36.title_owner_object.data(),
            title_route.event_function36.title_owner_object.size()).u32(0x9ac)==0,
        "input cannot bypass missing title UI initialization");
    req(platform::native_runtime_event_function36_invoke(c3,0x49e4c0u,405u)&&!c3.event_function36.initialized&&c3.event_function36.destroy_calls==1u,"r104 event36 native destroy lifecycle");
    req(!platform::native_runtime_event_function36_invoke(c3,0x49e490u,7u),"r104 event36 rejects unrelated event");
    auto bad=b;bad[0]='X';const std::string badp="r089-event-metadata-bad.bin";{std::ofstream f(badp,std::ios::binary);f.write(reinterpret_cast<const char*>(bad.data()),std::streamsize(bad.size()));}platform::NativeRuntimeContext c2;req(!platform::load_event_metadata_file(badp.c_str(),c2,&err),"reject bad magic");std::remove(badp.c_str());
    std::printf("native_runtime: %u checks passed\n",checks);return 0;
}
