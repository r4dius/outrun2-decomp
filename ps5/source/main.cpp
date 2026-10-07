// PS5 entry: the SDL/OpenGL renderer, SDL audio output and DualSense input
// around the shared game host (src/runtime/game_host); same flow as mac/source/main.cpp.
// application folders, the player options and the result log.
#include "setup_display.hpp"
#include "system/exe_image.hpp"
#include <algorithm>
#include <map>

#include "runtime/game_host.hpp"
#include "platform/pc_scene_renderer.hpp"
#include <memory>
#include "platform_api.hpp"
#include "input/runtime_adapter.hpp"

#include "driving/pc_driving.hpp"
#include "platform/embedded_exe_data.hpp"
#include "platform/frontend_preview_pack.hpp"
#include "platform/native_race_effects.hpp"   // scn-efc
#include "platform/sprite_2d_runtime.hpp"
#include "platform/race_hud_runtime.hpp"
#include "platform/race_area_runtime.hpp"
#include "platform/pc_screen.hpp"
#include "platform/race_ghosts_runtime.hpp"   // Time Attack ghosts
#include "platform/race_end_runtime.hpp"      // race end modes 19..35
#include "platform/race_course_passes.hpp"
#include "platform/pc_input_devices.hpp"
#include "system/network_bsd.hpp"           // LAN layer sockets
#include "platform/race_manager.hpp"
#include "platform/pc_vehicle_control.hpp"
#include "platform/vehicle_pose_filter.hpp"
#include "renderer.hpp"
#ifdef OR2_PS5_VULKAN
#include "vk_d3d9.hpp"
#endif
#include "enhancements/gpu_backend.hpp"
#include "enhancements/frame_rate.hpp"
#include "audio.hpp"
#include "movie_clock.hpp"
#include "frame_profile.hpp"
#include <array>
#include <cerrno>
#include <cmath>
#include <cstdlib>
#include <cstdio>
#include <cstdint>
#include <stdexcept>
#include <string>

namespace {
namespace rt=outrun::runtime;
using rt::result_printf;using rt::write_failure_log;using rt::open_result_log;

// The PS5 renderer as the game host's presenter.
class Ps5Presenter final : public rt::Presenter {
public:
    explicit Ps5Presenter(outrun::ps5_runtime::Ps5Renderer& r,rt::GameHost& host,outrun::platform::NativeRuntimeContext& context):r_(r),host_(host),context_(context){}
    bool set_start_loading_scene(std::uint32_t scene)override{return outrun::ps5_runtime::ps5_renderer_set_start_loading_scene(r_,scene);}
    bool set_start_loading_visible(bool v)override{return outrun::ps5_runtime::ps5_renderer_set_start_loading_visible(r_,v);}
    bool set_frontend_visible(bool v)override{return outrun::ps5_runtime::ps5_renderer_set_frontend_visible(r_,v);}
    bool set_frontend_glyphs(const std::vector<outrun::platform::FrontendGlyph>& g)override{return outrun::ps5_runtime::ps5_renderer_set_frontend_glyphs(r_,g);}
    bool set_frontend_images(const std::vector<outrun::platform::FrontendListImage>& i)override{return outrun::ps5_runtime::ps5_renderer_set_frontend_images(r_,i);}
    bool set_frontend_icons(const std::vector<outrun::platform::FrontendWindowIcon>& i)override{return outrun::ps5_runtime::ps5_renderer_set_frontend_icons(r_,i);}
    bool set_frontend_token(std::uint32_t t)override{return outrun::ps5_runtime::ps5_renderer_set_frontend_token(r_,t);}
    bool set_frontend_overlay(std::uint32_t t,float f)override{return outrun::ps5_runtime::ps5_renderer_set_frontend_overlay(r_,t,f);}
    bool set_frontend_game_backdrop(bool v)override{return outrun::ps5_runtime::ps5_renderer_set_frontend_game_backdrop(r_,v);}
    bool set_movie_frame(const std::uint8_t* rgba,std::uint32_t w,std::uint32_t h)override{return outrun::ps5_runtime::ps5_renderer_set_movie_frame(r_,rgba,w,h);}
    bool set_pc_scene(std::function<void(outrun::platform::PcD3D9Device&)> scene)override{return outrun::ps5_runtime::ps5_renderer_set_pc_scene(r_,std::move(scene));}
    bool draw()override{
        const auto result=outrun::ps5_runtime::ps5_renderer_draw(r_);
#if defined(OR2_PS5_VULKAN) && defined(OR2_PS5_MENU_TRACE)
        if(!rt::frame_profile().active&&++menu_frames_%120==0&&host_.pc_scene){const auto& scene=*host_.pc_scene;
            const auto& car=context_.event_function36.car_select;
            std::fprintf(stdout,"[ps5-menu-trace] mode=%u car_displays=%u select_draws=%u bank_failures=%u preview_init=%u controls=%u fault=%x %s\n",
                context_.mode_state.current,scene.car_displays,scene.select_car_draws(),scene.bank_failures,car.preview_inits,car.preview_controls,car.fault,rt::pc_scene_summary(host_).c_str());}
#endif
        return result;
    }
    bool frontend_animation_complete()const override{return outrun::ps5_runtime::ps5_renderer_stats(r_).frontend_animation_complete;}
    void scene_to_frame()override{outrun::ps5_runtime::ps5_renderer_scene_to_frame(r_);}   // MSAA resolve / FXAA before the 2D layer (Options > Settings)
    void cycle_pc_diagnostic()override{}
    void disable_pc_specialised_shaders()override{}
private:
    outrun::ps5_runtime::Ps5Renderer& r_;
    rt::GameHost& host_;outrun::platform::NativeRuntimeContext& context_;unsigned menu_frames_{};
};
// Core Audio as the game host's audio output.
class Ps5AudioOutput final : public rt::AudioOutput {
public:
    bool open(std::string& e)override{return audio_.open(e);}
    bool submit(const std::vector<std::int16_t>& pcm,std::string& e)override{return audio_.submit(pcm,e);}
    void close()override{audio_.close();}
    std::uint64_t submitted_frames()const override{return audio_.submitted_frames();}
    std::size_t queued_frames()const override{return audio_.queued_frames();}
private:
    outrun::ps5_runtime::Ps5Audio audio_;
};
// DualSense buttons <-> the shared pad layout.
constexpr std::pair<u64,std::uint64_t> PadMap[]{{Button_A,rt::PadA},{Button_B,rt::PadB},{Button_X,rt::PadX},{Button_Y,rt::PadY},
    {Button_L,rt::PadL},{Button_R,rt::PadR},{Button_ZL,rt::PadZL},{Button_ZR,rt::PadZR},{Button_Plus,rt::PadPlus},{Button_Minus,rt::PadMinus},
    {Button_StickL,rt::PadStickL},{Button_StickR,rt::PadStickR},{Button_Left,rt::PadLeft},{Button_Up,rt::PadUp},{Button_Right,rt::PadRight},{Button_Down,rt::PadDown}};
std::uint64_t to_pad(u64 b){std::uint64_t m=0;for(const auto& [ds,pad]:PadMap)if(b&ds)m|=pad;return m;}
u64 to_dualsense(std::uint64_t b){u64 m=0;for(const auto& [ds,pad]:PadMap)if(b&pad)m|=ds;return m;}
u64 dualsense_mapped(){u64 m=0;for(const auto& e:PadMap)m|=e.first;return m;}

// PS5 options live in the application home. The reference CPU backend exposes
// car reflections; Switch clock controls have no PS5 service. With the OpenGL
// backend, also set in Options > Settings:
//   widescreen=0        the PC's 4:3 instead of the 16:9 (Hor+) default
//   resolution=WxH      3D resolution: its height (480 .. 2160, default 2160); the width follows the aspect
//   antialiasing=off    off, msaa2, msaa4 or fxaa
// PS5 defaults (no options.ini key yet): 16:9 at 2160p.
struct Options {bool reflections{true};outrun::enhancements::VideoSettings video{true,{3840,2160},outrun::enhancements::Antialiasing::Off};} g_options;
const char* const OptionsDefault=
    "# OutRun 2006 PS5 options (edit and restart)\n"
    "reflections=1\n";
void load_options(){
    const std::string path=rt::home()+"/options.ini";
    std::FILE* f=std::fopen(path.c_str(),"rb");
    if(!f){if(std::FILE* w=std::fopen(path.c_str(),"wb")){std::fputs(OptionsDefault,w);std::fclose(w);}return;}
    char line[256];
    while(std::fgets(line,sizeof line,f)){
        std::string l(line);while(!l.empty()&&(l.back()==10||l.back()==13||l.back()==32))l.pop_back();
        const auto eq=l.find('=');if(l.empty()||l[0]=='#'||eq==std::string::npos)continue;
        const std::string k=l.substr(0,eq);const unsigned v=unsigned(std::strtoul(l.c_str()+eq+1,nullptr,10));
        if(k=="reflections")g_options.reflections=v!=0;
        else if(outrun::enhancements::GpuVideoBackend::parse_option(k,l.substr(eq+1),g_options.video)){}
        else std::fprintf(stdout,"PS5 options: unsupported key %s\n",k.c_str());
    }
    std::fclose(f);
    if(!g_options.reflections)rt::cube_disabled()=true;
}
// Touch-pad performance overlay, drawn by the native 2D flush (pc_screen.hpp).
struct Overlay {
    bool on{};std::uint64_t last{},sum_dt{},sum_wait{},sum_gpu_ns{},sum_record_ns{},sum_present_ns{};unsigned n{},toast_frames{};
    std::string stats,toast;
} g_overlay;
std::string video_settings(){
#if defined(OR2_PS5_OPENGL) || defined(OR2_PS5_VULKAN)
    const auto* backend=outrun::enhancements::video_backend();const auto settings=backend?backend->current():g_options.video;
#ifdef OR2_PS5_OPENGL
    return "OpenGL "+outrun::enhancements::resolution_label(settings.resolution)+(settings.widescreen?" 16:9 ":" 4:3 ")+outrun::enhancements::antialiasing_key(settings.antialiasing);
#else
    return "Vulkan "+outrun::enhancements::resolution_label(settings.resolution)+(settings.widescreen?" 16:9 ":" 4:3 ")+outrun::enhancements::antialiasing_key(settings.antialiasing);
#endif
#else
    return "CPU 640x480 4:3";
#endif
}
void overlay_frame(const rt::GameHost& s,const outrun::ps5_runtime::Ps5Renderer* renderer){
    auto& o=g_overlay;
    const std::uint64_t now=OR2_PROFILE_TICK(),freq=OR2_PROFILE_FREQ();
    const auto wait=rt::frame_wait_ticks();rt::frame_wait_ticks()=0;
    if(o.last&&now>o.last){
        o.sum_dt+=now-o.last;o.sum_wait+=std::min(wait,now-o.last);++o.n;
        if(renderer){const auto stats=outrun::ps5_runtime::ps5_renderer_stats(*renderer);o.sum_gpu_ns+=stats.last_gpu_ns;o.sum_record_ns+=stats.last_record_ns;o.sum_present_ns+=stats.last_present_ns;}
    }
    o.last=now;
    if(o.n>=20u){
        const double frame_ms=double(o.sum_dt)*1000.0/double(freq)/o.n;
        const double cpu_ms=double(o.sum_dt-o.sum_wait)*1000.0/double(freq)/o.n;
        char t[512];
#if defined(OR2_PS5_OPENGL) && defined(OR2_PS5_PAYLOAD)
        // Native GL timers use the CPU clock; do not display them as GPU load.
        std::snprintf(t,sizeof t,"FPS %.1f %.1fms\nCPU/GL %.1fms\nDRAW %.1fms SWAP %.1fms\nGPU n/d\n",
            frame_ms>0?1000.0/frame_ms:0.0,frame_ms,cpu_ms,double(o.sum_record_ns)/1e6/o.n,double(o.sum_present_ns)/1e6/o.n);
#else
        const double gpu_ms=double(o.sum_gpu_ns)/1e6/o.n;
        std::snprintf(t,sizeof t,"FPS %.1f %.1fms\nCPU %.1fms %.0f%%\nGPU %.1fms %.0f%%\n",
            frame_ms>0?1000.0/frame_ms:0.0,frame_ms,cpu_ms,frame_ms>0?cpu_ms*100.0/frame_ms:0.0,gpu_ms,frame_ms>0?gpu_ms*100.0/frame_ms:0.0);
#endif
        o.stats=t;
#ifdef OR2_PS5_VULKAN
        std::snprintf(t,sizeof t,"DRAW %.1fms SWAP %.1fms\nWAIT %.1fms\n",double(o.sum_record_ns)/1e6/o.n,double(o.sum_present_ns)/1e6/o.n,double(o.sum_wait)*1000.0/double(freq)/o.n);
        o.stats+=t;
#endif
        std::snprintf(t,sizeof t,"SND %ums skip %llu\n",unsigned((s.audio_output?s.audio_output->queued_frames():0u)/48u),(unsigned long long)s.audio_drops);
        o.stats+=t;o.sum_dt=o.sum_wait=o.sum_gpu_ns=o.sum_record_ns=o.sum_present_ns=0;o.n=0;
    }
    std::string text;
    if(o.on)text=o.stats+video_settings();
    else if(o.toast_frames){--o.toast_frames;text=o.toast;}
    outrun::platform::g_pc_overlay_text=text;
}

bool find_retail_root(outrun::platform::RetailAssetStore& store,std::string& used,std::string& error){
    used=application_retail_root();
    if(!outrun::platform::retail_asset_store_open(store,used,&error))return false;
    return true;
}

void wait_for_plus(PadState&){std::fflush(stdout);std::fflush(stderr);}
}

int main(int argc,char** argv){
    std::string backend_error;
    if(!application_initialize(backend_error)){std::fprintf(stderr,"PS5 platform: %s\n",backend_error.c_str());return 1;}
    std::atexit(application_shutdown);
    outrun::platform::native_network_set_platform(outrun::platform::pc_network_bsd_platform());
    consoleInit(nullptr);
    rt::product()={"OutRunPS5","OutRunPS5.log"};
    // An uncaught exception or abort: crash.txt in the launch folder names it and the
    // last scene display / leaf (the result log is only written at a normal exit).
    std::set_terminate([]{
        std::string what="unknown (not a std::exception)";
        try{if(auto e=std::current_exception())std::rethrow_exception(e);else what="std::terminate without an exception";}
        catch(const std::exception& e){what=e.what();}catch(...){}
        if(std::FILE* f=std::fopen((rt::home()+"/crash.txt").c_str(),"wb")){
            const auto& c=rt::crash_breadcrumbs();
            std::fprintf(f,"terminate: %s\nlast scene stage: %s %x\nmode=%u\n",what.c_str(),c.stage?c.stage:"-",unsigned(c.value),unsigned(c.mode));
            std::fclose(f);}
        std::abort();});

    std::printf("OutRunPS5 - authored frontend/title ownership\n\n");
    std::printf("Cross/Options confirm; L1+R1+Options exits.\n\n");

    // PS5-owned logs/saves/caches are separate from read-only PC data.
    try{rt::home()=application_home(argc,argv);}catch(const std::exception& e){
        std::fprintf(stderr,"PS5 application home: %s\n",e.what());return 1;}
    // FRAME RATE default: 60 (options.ini overrides; 120 in Options > Settings).
    g_options.video.frame_rate=60u;
    load_options();
    bool render_profile=false;
#ifdef OR2_PS5_PAYLOAD
    // Fine renderer zones read steady_clock thousands of times per race frame.
    // Keep the frame/wait overlay, but enable these development zones only on
    // request. The flag is read once, after application_home has been resolved.
    render_profile=std::getenv("OR2_PS5_RENDER_PROFILE")!=nullptr;
    if(auto* flag=std::fopen((rt::home()+"/profile-renderer.flag").c_str(),"rb")){
        render_profile=true;std::fclose(flag);
    }
    constexpr unsigned ClockReads=8192;
    const auto clock_begin=std::chrono::steady_clock::now();
    for(unsigned i=0;i<ClockReads;++i)(void)std::chrono::steady_clock::now();
    const auto clock_ns=std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now()-clock_begin).count();
    std::fprintf(stdout,"[ps5-cpu-profile] detailed-zones=%u steady-clock=%.1fns/read (%u reads)\n",
        unsigned(render_profile),double(clock_ns)/ClockReads,ClockReads);
#endif
    rt::GameHost diag{};
    Ps5AudioOutput audio;
    diag.audio_output=&audio;
    diag.save_directory=rt::home()+"/SaveGame";
    PadState pad{};
    bool console_live=true;
    outrun::ps5::DualSenseInput input_adapter;
    outrun::ps5::RuntimeAdapter lifecycle;
    outrun::ps5_runtime::MovieClock movie_clock;
    padConfigureInput(1,PadStandard);
    padInitializeDefault(&pad);
    outrun::ps5_runtime::Ps5Renderer renderer{};
    outrun::ps5_runtime::Ps5Renderer* active_renderer=nullptr;   // set once initialized
    diag.movie_clock_reset=[&movie_clock]{movie_clock.reset(std::chrono::steady_clock::now());};
    diag.movie_clock_read=[&movie_clock](bool& paused){
        paused=movie_clock.advance(std::chrono::steady_clock::now());
        if(paused)std::fprintf(stdout,"Title movie: resumed after window/scheduling pause\n");
        return movie_clock.elapsed();};
    diag.read_pad=[&pad]{
        padUpdate(&pad);
        rt::PadSnapshot p;
        p.held=to_pad(padGetButtons(&pad));p.down=to_pad(padGetButtonsDown(&pad));p.up=to_pad(padGetButtonsUp(&pad));
        const auto l=padGetStickPos(&pad,0u),r=padGetStickPos(&pad,1u);
        p.left={l.x,l.y};p.right={r.x,r.y};
        p.connected=padIsConnected(&pad);
        return p;};
    // The touch pad controls the overlay. Other inputs belong to the game.
    diag.platform_keys=[&diag,&active_renderer,render_profile](std::uint64_t,std::uint64_t down){
#ifdef OR2_PS5_PAYLOAD
        // GameHost sets this from the race mode just before platform_keys.
        // Disabling zones skips their clock calls; it does not alter game time,
        // CPU/GPU measurements, or the frame/fence profiling used by the overlay.
        outrun::platform::pc_perf_enabled()&=render_profile;
#endif
        diag.widescreen=outrun::ps5_runtime::g_widescreen;
        if((down&rt::PadMinus)!=0u)g_overlay.on=!g_overlay.on;
        overlay_frame(diag,active_renderer);};
    diag.map_input=[&diag,&pad,&input_adapter,&lifecycle](std::uint64_t held,std::uint64_t,const rt::PadSnapshot&,outrun::platform::NativeInputState& out){
        pad.sample.buttons=std::uint32_t((pad.sample.buttons&~dualsense_mapped())|to_dualsense(held));
        out=input_adapter.update(pad.sample,diag.runtime?diag.runtime->mode_state.current:0u);
        out.exit_requested|=diag.render_failed||!application_running();
        if(out.exit_requested)lifecycle.stop();};
    diag.sample_status=[&diag,&console_live](const outrun::platform::NativeInputState& out,std::uint64_t held,std::uint64_t down,std::uint64_t up){
        if(console_live&&((diag.input_samples&0xffu)==1u||down!=0u||up!=0u)){
            std::printf("\x1b[6;1Hinput samples=%u connected=%u                         \n",diag.input_samples,unsigned(out.connected));
            std::printf("steer=%4d accel=%3u brake=%3u shift=%c/%c             \n",out.steering,out.accelerator,out.brake,out.shift_down?'D':'-',out.shift_up?'U':'-');
            std::printf("held=%016llx  (L1+R1+Options exits)                          \n",(unsigned long long)to_dualsense(held));
            consoleUpdate(nullptr);
        }};

    std::string error,retail_root;
    outrun::platform::RetailAssetStore retail{};
    if(!find_retail_root(retail,retail_root,error)){
        std::printf("Original OutRun 2006 data tree not found.\n\n");
        std::printf("Pass the owned installation with --data=/path/to/OutRun2006.\n");
        std::printf("(started from: %s).\n\nLast error: %s\n",rt::home().c_str(),error.c_str());
        write_failure_log(2u,"retail-root",error.c_str());consoleUpdate(nullptr);wait_for_plus(pad);
        consoleExit(nullptr);return 2;
    }

    // The player's OR2006C2C.EXE: its prepared image next to the game data
    // (made by the builder kit, or by an earlier launch), or the first-launch
    // preparation in its own window (nothing of the EXE is in this application).
    {outrun::ps5_runtime::SetupDisplay display;
     auto setup=display.platform();
     setup.wait_for_exit=[&pad]{wait_for_plus(pad);};
     const bool ready=outrun::platform::exe_setup_prepare(retail_root,rt::home(),setup,&error);
     display.close();
     if(!ready){
        std::printf("OR2006C2C.EXE: %s\n",error.c_str());
        write_failure_log(18u,"exe-image",error.c_str());consoleUpdate(nullptr);
        consoleExit(nullptr);return 18;
     }}

    // Built after the EXE image is installed: the context copies EXE-owned tables.
    auto context_owner=std::make_unique<outrun::platform::NativeRuntimeContext>();
    auto& context=*context_owner;
    diag.runtime=&context;

    rt::GameData data;
    {std::uint32_t code=0;const char* phase=nullptr;
     const auto status=[&retail](const std::string& line){
         std::printf("%s",line.c_str());
         consoleUpdate(nullptr);};
     if(!rt::load_game_data(context,retail,retail_root,data,status,code,phase,error)){
        write_failure_log(code,phase,error.c_str());consoleUpdate(nullptr);wait_for_plus(pad);
        consoleExit(nullptr);return int(code);
     }}
        const auto& course_assets=data.course_assets;
    const auto& course_collision=data.course_collision;const auto& driving_data=data.driving_data;
    const std::string& metadata_path=data.metadata_path;const std::string& loader_assets_path=data.loader_assets_path;
    const std::string& course_assets_path=data.course_assets_path;const std::string& stage17_assets_path=data.stage17_assets_path;
    const std::string& race_assets_path=data.race_assets_path;const std::string& race_assignment_path=data.race_assignment_path;
    const std::string& world_source_path=data.world_source_path;const std::string& course_collision_path=data.course_collision_path;
    const std::string& driving_data_path=data.driving_data_path;

    rt::RuntimeBinding binding;
    binding.platform=outrun::ps5::make_runtime_platform(lifecycle);
#ifdef OR2_PS5_HOST_TEST
    // Host checks of display rates: OR2_HOST_FRAME_HZ=N makes the game clock
    // advance 1/N s per presented frame, whatever the host's speed.
    if(const char* hz=std::getenv("OR2_HOST_FRAME_HZ")){
        static double step=1e9/std::max(1.0,std::strtod(hz,nullptr));static auto* frames_of=&renderer;
        binding.platform.ticks=[](void*)->std::uint64_t{return std::uint64_t(double(outrun::ps5_runtime::ps5_renderer_stats(*frames_of).frames)*step);};
        binding.platform.frequency=[](void*)->std::uint64_t{return 1000000000ull;};
    }
#endif
    auto& native_platform=binding.platform;auto& native_input=binding.input;
    if(!rt::bind_runtime(diag,context,binding)){
        write_failure_log(6u,"mode-request");
        consoleExit(nullptr);
        return 6;
    }

    std::printf("metadata: %s\n",metadata_path.c_str());
    std::printf("course collision: %s\nloader assets: %s\ncourse assets: %s\nstage17 assets: %s\ndriving data: %s\n",course_collision_path.c_str(),loader_assets_path.c_str(),course_assets_path.c_str(),stage17_assets_path.c_str(),driving_data_path.c_str());
    std::printf("Initializing PS5 frontend; press Options to finish.\n");consoleUpdate(nullptr);platform_sleep_ns(500000000ll);consoleExit(nullptr);console_live=false;
    // Options > Settings enhancement rows: 16:9, 3D resolution, MSAA / FXAA,
    // frame rate. The scanout is chosen before the display opens.
    outrun::enhancements::GpuVideoBackend video(rt::home()+"/options.ini",g_options.video);
#ifdef OR2_PS5_VULKAN
    {const auto start=video.current().resolution;outrun::ps5_runtime::vulkan::Context::prefer_scanout(start.width,start.height);}
#endif
    if(!outrun::ps5_runtime::ps5_renderer_initialize(renderer,nullptr,error,&data.start_loading,nullptr,nullptr,nullptr)){
        consoleInit(nullptr);console_live=true;std::printf("OutRunPS5 renderer initialization failed.\n\n%s\n\nPress Options to exit.\n",error.c_str());write_failure_log(5u,"renderer",error.c_str());consoleUpdate(nullptr);wait_for_plus(pad);
        consoleExit(nullptr);
        return 5;
    }
    if(!rt::bind_frontend_timing(context.event_function36.frontend_sprites,data)||
       !outrun::ps5_runtime::ps5_renderer_attach_frontend_sprites(renderer,context.event_function36.frontend_sprites)){
        write_failure_log(5u,"frontend-instances","Invalid authored SUMO_FE timing");
        outrun::ps5_runtime::ps5_renderer_shutdown(renderer);
        return 5;
    }
    Ps5Presenter presenter(renderer,diag,context);
    diag.presenter=&presenter;active_renderer=&renderer;
    // The video rows apply to the renderer from here on.
#if defined(OR2_PS5_OPENGL) || defined(OR2_PS5_VULKAN)
    video.attach([&renderer](std::uint32_t w,std::uint32_t h,bool wide,unsigned msaa,bool fxaa){outrun::ps5_runtime::ps5_renderer_set_video(renderer,w,h,wide,msaa,fxaa);});
    outrun::enhancements::set_video_backend(&video);
#endif
#ifdef OR2_PS5_VULKAN
    // FRAME RATE 60 / 120 when the display opened at 120 Hz, applied at once
    // through the VideoOut flip rate.
    {using outrun::ps5_runtime::vulkan::Context;
     if(Context::display_refresh()>=120u)video.set_frame_rates({60,120},[](std::uint32_t fps){Context::pace_frames(fps);});
     else video.set_frame_rates({60});}
    std::fprintf(stderr,"[frame-rate] %u fps, display %u Hz\n",unsigned(outrun::enhancements::display_rate()),
        outrun::ps5_runtime::vulkan::Context::display_refresh());
#endif
    if(auto* pc_device=outrun::ps5_runtime::ps5_renderer_pc_device(renderer)){
        rt::attach_pc_scene(diag,context,retail,*pc_device);
#ifdef OR2_PS5_VULKAN
        // Bounded capture of the car-select owner's inputs and first geometry.
        // The shared renderer/decompilation and the owner's game state stay intact.
        if(diag.pc_scene){auto camera=diag.pc_scene->vehicle_camera;
            auto* vk_device=static_cast<outrun::ps5_runtime::VulkanD3D9Device*>(pc_device);
            diag.pc_scene->vehicle_camera=[&,camera,vk_device,reported=false](outrun::driving::Bytes& car,outrun::driving::Bytes& view)mutable{
                const bool ok=camera&&camera(car,view);
                const auto& owner=context.event_state.slots[8];
                if(ok&&!reported&&owner.function_id==0x2bu&&(owner.flags&3u)==2u&&!rt::frame_profile().active){
                    reported=true;vk_device->trace_menu_vertices(12);
                    std::fprintf(stdout,"[ps5-menu-pose] mode=%u control=%x model=%u flags=%x brightness=%.4g position=%.4g,%.4g,%.4g matrix=%.4g,%.4g,%.4g,%.4g translation=%.4g,%.4g,%.4g\n",
                        context.mode_state.current,owner.ctrl_callback,car.u8(0x11),car.u32(4),car.f32(0x58),car.f32(0x14),car.f32(0x18),car.f32(0x1c),
                        car.f32(0xb0),car.f32(0xc4),car.f32(0xd8),car.f32(0xec),car.f32(0xe0),car.f32(0xe4),car.f32(0xe8));
                }
                return ok;
            };
        }
#endif
        outrun::ps5_runtime::ps5_renderer_set_pc_sprites(renderer,true);
        outrun::ps5_runtime::ps5_renderer_set_pc_text(renderer,true);
    }
    rt::FrontendSession fe;
    {const char* phase=nullptr;
     if(!rt::open_frontend(diag,context,retail,&data.fonts,fe,phase,error)){
        const bool text=std::string(phase)=="frontend-text";
        write_failure_log(text?5u:6u,phase,error.c_str());
        outrun::enhancements::set_video_backend(nullptr);video.attach({});outrun::ps5_runtime::ps5_renderer_shutdown(renderer);diag.presenter=nullptr;active_renderer=nullptr;
        return text?5:6;
     }}
    auto& license_owners=*fe.license_owners;
    (void)outrun::ps5_runtime::ps5_renderer_set_frontend_visible(renderer,false);
    const auto rc=outrun::platform::run_native_runtime(context,binding.services);
    rt::close_frontend(diag,context,fe);
    const auto renderer_stats=outrun::ps5_runtime::ps5_renderer_stats(renderer);
    const auto pc_scene_error=outrun::ps5_runtime::ps5_renderer_pc_error(renderer);
    const std::string pc_scene_summary=rt::pc_scene_summary(diag);
#if defined(OR2_PS5_HOST_TEST)
    if(std::getenv("OR2_HOST_AREA"))std::fprintf(stderr,"race area report:\n%s",outrun::platform::native_race_area_report(context).c_str()); // race AREA/SKY
    if(std::getenv("OR2_HOST_RACE"))std::fprintf(stderr,"race manager report:\n%s",outrun::platform::native_race_manager_report(context).c_str()); // race manager (event 359)
#endif
    diag.pc_scene.reset();
    const outrun::driving::Bytes event36_object(context.event_function36.object.data(),context.event_function36.object.size());
    const auto event36_owner_state=event36_object.u32(0x218u);
    const auto event36_loader_stage=event36_object.u32(0x0000u);
    const auto stage12_list_count=event36_object.u32(0x0484u);
    const auto stage12_first_handle=event36_object.u32(0x0284u);
    const auto state2_gate_handle=event36_object.u32(0x0488u);
    outrun::enhancements::set_video_backend(nullptr);video.attach({});outrun::ps5_runtime::ps5_renderer_shutdown(renderer);diag.presenter=nullptr;active_renderer=nullptr;consoleInit(nullptr);console_live=true;
    const auto& ev405=context.event_state.slots[405];
    const auto& frontend=context.event_function36;
    const bool state2_children=frontend.state2_frames>0u&&
        frontend.state2_transition_ticks==frontend.state2_frames&&
        frontend.state2_ui_state_ticks==frontend.state2_frames&&
        frontend.state2_ui_ticks==frontend.state2_frames&&
        frontend.state2_embedded_ticks==frontend.state2_frames&&
        frontend.state2_gate_ticks==frontend.state2_frames&&
        frontend.state2_runtime_transition_ticks==frontend.state2_frames&&
        frontend.state2_queue_ticks==frontend.state2_frames&&
        frontend.state2_shutdown_ticks==frontend.state2_frames;
    const bool frontend_token_valid=(renderer_stats.frontend_token>>16u)==0x44u&&
        (renderer_stats.frontend_token&0xffffu)<224u;
    const bool frontend_rendered=renderer_stats.frontend_textures==129u&&
        renderer_stats.frontend_scenes==224u&&frontend_token_valid&&
        renderer_stats.frontend_scene_width==outrun::platform::FrontendPreviewSceneWidth&&
        renderer_stats.frontend_scene_height==outrun::platform::FrontendPreviewSceneHeight&&
        renderer_stats.frontend_frames>0u&&
        diag.frontend_gate_open&&
        renderer_stats.frontend_frames+renderer_stats.loading_frames+
            renderer_stats.unowned_frames==renderer_stats.frames;
    const bool game_transition_passed=context.mode_state.current==16u&&
        context.mode_state.previous==32u&&context.mode_state.transition_pending==0u&&
        context.mode_state.init_callbacks==2u&&
        context.mode_state.control_callbacks==renderer_stats.frames&&
        context.mode_state.exit_callbacks==1u&&
        context.mode_state.sumo_fe_reset_calls==1u&&
        context.mode_state.sumo_fe_event_setup_calls==1u&&
        context.mode_state.sumo_fe_event_close_calls==1u&&
        context.mode_state.sumo_fe_game_requests==1u&&
        context.mode_state.sumo_fe_owner_command_polls>0u&&
        context.mode_state.sumo_fe_last_owner_command==16u&&
        event36_object.u32(0x21cu)==0xffffffffu&&
        context.game_mode.active&&context.game_mode.init_calls==1u&&
        context.game_mode.control_calls>0u&&context.game_mode.exit_calls==0u&&
        context.game_mode.previous_mode==32u&&
        context.game_mode.empty_440d90_calls==1u&&
        context.game_mode.global_effect_calls==1u&&
        context.game_mode.global_effect_last==0x82u&&
        context.game_mode.timing_reset_calls==1u&&
        context.game_mode.owner_command_polls==context.game_mode.control_calls&&
        context.game_mode.menu_state_polls==context.game_mode.control_calls&&
        diag.event_406_init==1u&&diag.event_406_ctrl>0u&&diag.event_406_dest==1u&&
        context.event_state.slots[406].flags==0u;
    const bool course_runtime_passed=context.game_mode.course_load_calls==1u&&
        context.game_mode.course_load_success==1u&&
        context.game_mode.course_read_calls==1u&&
        context.game_mode.course_bytes==outrun::platform::CourseCvtBlobBytes&&
        context.game_mode.course_provider_loaded&&
        context.game_mode.course_loader_phase==3u&&
        context.game_mode.course_load.force_sync_7d2d8c==1u&&
        context.game_mode.course_runtime.active_count_7d33c4==outrun::platform::CourseCvtRecordCount&&
        context.game_mode.course_runtime.max_depth_7d33c0==4&&
        context.game_mode.course_runtime.selected_index==0&&
        context.game_mode.course_runtime.selected_copy_active&&
        context.game_mode.course_matrix_ready;
    const bool course_collision_passed=course_collision.quads.size()==5597u&&
        course_collision.pc_coli0200.size()==835204u&&
        context.game_mode.gameplay_ground_queries>=context.game_mode.control_calls*4u&&
        context.game_mode.gameplay_ground_hits>0u&&
        context.game_mode.gameplay_ground_hits+context.game_mode.gameplay_ground_rejects==context.game_mode.gameplay_ground_queries&&
        context.game_mode.gameplay_sweep_poses>=context.game_mode.control_calls&&
        context.game_mode.gameplay_pose_filter.initialized&&context.game_mode.gameplay_pose_filter.updates>0u&&
        context.game_mode.gameplay_pose_filter.max_applied_angle<=0.01801f&&
        context.game_mode.gameplay_pose_filter.max_applied_height<=0.02501f;
    const outrun::driving::Bytes pc_vehicle_event(
        context.game_mode.gameplay_vehicle.event.data(),context.game_mode.gameplay_vehicle.event.size());
    const bool pc_vehicle_passed=context.game_mode.gameplay_vehicle.initialized&&
        context.game_mode.gameplay_vehicle.retail_driving_data&&
        context.game_mode.gameplay_vehicle.retail_car_id==outrun::platform::DrivingPackV1LegacyCarId&&
        context.game_mode.gameplay_vehicle.retail_parameter_column==outrun::platform::DrivingPackV1LegacyColumn&&
        context.game_mode.gameplay_vehicle.frames==context.game_mode.gameplay_frames&&
        context.game_mode.gameplay_frames==context.game_mode.control_calls&&
        context.game_mode.gameplay_vehicle.operation_input_calls==context.game_mode.gameplay_vehicle.frames&&
        context.game_mode.gameplay_vehicle.steering_calls==context.game_mode.gameplay_vehicle.frames&&
        context.game_mode.gameplay_vehicle.transmission_calls==context.game_mode.gameplay_vehicle.frames&&
        context.game_mode.gameplay_vehicle.engine_torque_calls==context.game_mode.gameplay_vehicle.frames&&
        context.game_mode.gameplay_vehicle.brake_pressure_calls==context.game_mode.gameplay_vehicle.frames&&
        context.game_mode.gameplay_vehicle.suspension_force_calls==context.game_mode.gameplay_vehicle.frames&&
        context.game_mode.gameplay_vehicle.tire_load_calls==context.game_mode.gameplay_vehicle.frames&&
        context.game_mode.gameplay_vehicle.driving_control_calls==context.game_mode.gameplay_vehicle.frames&&
        context.game_mode.gameplay_vehicle.road_mu_calls==context.game_mode.gameplay_vehicle.frames&&
        context.game_mode.gameplay_vehicle.road_contact_frames==context.game_mode.gameplay_vehicle.frames&&
        context.game_mode.gameplay_vehicle.road_contact_wheel_hits>=context.game_mode.gameplay_vehicle.frames*3u&&
        context.game_mode.gameplay_vehicle.wheel_angle_updates==context.game_mode.gameplay_vehicle.frames*4u&&
        context.game_mode.gameplay_vehicle.steering_full_lock>900u&&
        context.game_mode.gameplay_vehicle.steering_full_lock<1400u&&
        context.game_mode.gameplay_vehicle_output.steering_full_lock==context.game_mode.gameplay_vehicle.steering_full_lock&&
        pc_vehicle_event.u32(0x2b4u)==0xdead0001u&&
        std::isfinite(context.game_mode.gameplay_steering_normalized)&&
        std::isfinite(context.game_mode.gameplay_yaw_step)&&
        std::fabs(context.game_mode.gameplay_steering_normalized)<=1.0f&&
        std::isfinite(context.game_mode.gameplay_vehicle_output.engine_rpm)&&
        std::isfinite(context.game_mode.gameplay_vehicle_output.drive_torque)&&
        std::isfinite(context.game_mode.gameplay_vehicle_output.brake_pressure)&&
        context.game_mode.gameplay_vehicle_output.full_driving_control&&
        (context.game_mode.gameplay_vehicle_output.road_contact_mask&0x0fu)!=0u&&
        context.game_mode.gameplay_vehicle_output.gear>=1u&&context.game_mode.gameplay_vehicle_output.gear<=6u;
    const auto asset_bytes=[&](std::uint32_t id,std::uint32_t mode){
        std::string ignored;
        const auto* asset=outrun::platform::retail_asset_lookup(retail,id,mode,&ignored);
        return asset?asset->bytes.size():std::size_t{0};
    };
    const auto expected_loader_bytes=asset_bytes(0xbau,2u)+asset_bytes(0xbbu,2u);
    const auto expected_shared_bytes=asset_bytes(0x2cu,8u)+asset_bytes(0x33u,8u)+asset_bytes(0x48u,8u);
    const auto expected_frontend_bytes=asset_bytes(0x44u,9u);
    constexpr std::array<std::uint32_t,10> meter_ids{{
        0x23u,0x24u,0x25u,0x26u,0x27u,0x28u,0x29u,0x2au,0x30u,0x31u}};
    std::uint32_t meter_archive_count=0u;
    std::size_t meter_archive_bytes=0u;
    for(const auto id:meter_ids){
        const auto bytes=asset_bytes(id,8u);
        if(bytes!=0u){++meter_archive_count;meter_archive_bytes+=bytes;}
    }
    const auto optional_effect_bytes=asset_bytes(0x3cu,9u);
    const bool scene_resource_pack_complete=meter_archive_count==meter_ids.size()&&
        optional_effect_bytes!=0u;
    const bool loader_bytes_match=expected_loader_bytes!=0u&&expected_shared_bytes!=0u&&
        expected_frontend_bytes!=0u&&context.event_function36.loader_ready_bytes==expected_loader_bytes&&
        context.event_function36.shared_ready_bytes==expected_shared_bytes&&
        context.event_function36.frontend_ready_bytes==expected_frontend_bytes;
const bool core_passed=loader_bytes_match&&rc==0u&&ev405.function_id==36u&&ev405.ctrl_callback==0x49e4a0u&&renderer_stats.frames>=4u&&course_collision_passed&&pc_vehicle_passed&&frontend_rendered&&game_transition_passed&&course_runtime_passed&&context.event_function36.init_calls==1u&&context.event_function36.control_calls==renderer_stats.frames&&context.event_function36.display_calls==renderer_stats.frames&&event36_owner_state==2u&&event36_loader_stage==14u&&context.event_function36.loader_begin_calls==1u&&context.event_function36.loader_request_calls==2u&&context.event_function36.loader_ready_count==2u&&context.event_function36.loader_ready_bytes==expected_loader_bytes&&context.event_function36.loader_stage4_passes==1u&&context.event_function36.loader_resource_pending==0u&&context.event_function36.shared_loader.stage_83db18==6u&&context.event_function36.shared_loader_calls==1u&&context.event_function36.shared_last_result==1u&&context.event_function36.shared_resource_requests==3u&&context.event_function36.shared_last_resource==0x48u&&context.event_function36.shared_last_mode==8u&&context.event_function36.shared_ready_count==3u&&context.event_function36.shared_ready_bytes==expected_shared_bytes&&context.event_function36.shared_resource_pending==0u&&context.event_function36.shared_release_calls==3u&&context.event_function36.shared_table_reset_calls==1u&&context.event_function36.shared_finalize_calls==1u&&context.event_function36.frontend_resource_requests==1u&&context.event_function36.frontend_ready_count==1u&&context.event_function36.frontend_ready_bytes==expected_frontend_bytes&&context.event_function36.frontend_resource_pending==0u&&context.event_function36.frontend_release_calls==1u&&context.event_function36.select_table_open_calls==1u&&context.event_function36.select_table_read_calls==1u&&context.event_function36.select_table_close_calls==1u&&context.event_function36.select_table_bytes==outrun::platform::LoaderAssetSelectTableBytes&&context.event_function36.frontend_init_calls==2u&&context.event_function36.frontend_async_polls==1u&&context.event_function36.frontend_bulk_polls==1u&&context.event_function36.frontend_bulk_requests==outrun::platform::LoaderAssetFrontendScriptCount&&context.event_function36.frontend_bulk_ready_count==outrun::platform::LoaderAssetFrontendScriptCount&&context.event_function36.frontend_bulk_ready_bytes==outrun::platform::LoaderAssetFrontendScriptBytes&&context.event_function36.frontend_bulk_pending==0u&&context.event_function36.frontend_bulk_format_calls==60u&&context.event_function36.frontend_bulk_bind_calls==4u&&context.event_function36.frontend_bulk_last_lane==63u&&context.event_function36.loader_stage12_passes==1u&&context.event_function36.loader_stage12_body_calls==1u&&context.event_function36.loader_stage14_passes==1u&&stage12_list_count>=1u&&stage12_first_handle!=0u&&stage12_first_handle==context.event_function36.stage12_last_handle&&context.event_function36.stage12_global_init_calls==1u&&context.event_function36.stage12_embedded_init_calls==1u&&context.event_function36.stage12_embedded_reset_calls==1u&&context.event_function36.stage12_dispatch_calls==1u&&context.event_function36.stage12_factory_allocate_calls==1u&&context.event_function36.stage12_factory_construct_calls==1u&&context.event_function36.stage12_ready_checks>=1u&&context.event_function36.stage12_transition_init_calls==1u&&context.event_function36.stage12_scene_init_calls==1u&&context.event_function36.stage12_manager_reset_calls==1u&&context.event_function36.stage12_audio_calls==0u&&context.event_function36.stage12_optional_calls==0u&&state2_children&&frontend.state2_ui_invalid_calls==0u&&frontend.frontend_handle_count>=1u&&frontend.frontend_welcome_control_calls>0u&&frontend.state2_selector_calls==frontend.state2_frames&&!diag.render_failed;
    const char* result_log_path=nullptr;int result_log_error=0;
    std::FILE* result_log=open_result_log(result_log_path,result_log_error);
    const bool passed=core_passed&&scene_resource_pack_complete&&result_log!=nullptr;
    const bool start_attempted=context.start_mode.original_route_requests!=0u;
    const bool start_probe=start_attempted&&context.start_mode.init_calls!=0u&&
        context.start_mode.stage==5u&&
        context.start_mode.scene_owner_stage==61u&&
        context.start_mode.scene_owner_sprite_requests==1u&&
        context.start_mode.scene_owner_sprite_bytes==asset_bytes(0x2bu,8u)&&
        context.start_mode.scene_owner_missing_service==0x004b00d0u&&
        renderer_stats.loading_scenes==9u&&renderer_stats.loading_textures==5u&&
        renderer_stats.loading_draws==171u&&renderer_stats.loading_frames>0u&&
        rc==0u&&!diag.render_failed&&result_log!=nullptr;
    const bool menu_partial=!start_attempted&&context.mode_state.current==32u&&
        rc==0u&&renderer_stats.frontend_frames>0u&&
        frontend.state2_frames>0u&&frontend.frontend_welcome_control_calls>0u&&
        frontend.state2_ui_open_calls>0u&&
        frontend.state2_ui_valid_calls==frontend.state2_ui_open_calls&&
        !diag.render_failed&&result_log!=nullptr;
    result_printf(result_log,"OutRunPS5 - original welcome and frontend stack integration\n\n");
    result_printf(result_log,"verdict=%s\n",
        start_probe||menu_partial||passed?"PARTIAL":"FAIL");
    result_printf(result_log,"checks: collision=%u pc_vehicle=%u frontend=%u game_transition=%u course_runtime=%u loader_bytes=%u log=%u\n",
        unsigned(course_collision_passed),unsigned(pc_vehicle_passed),
        unsigned(frontend_rendered),unsigned(game_transition_passed),
        unsigned(course_runtime_passed),unsigned(loader_bytes_match),
        unsigned(result_log!=nullptr));
    result_printf(result_log,"scope=%s; retail assets/cache verified, natural frontend/title route remains fail-closed\n",
                  start_attempted?"source-backed START probe":"asset/bootstrap/render diagnostic");
    result_printf(result_log,"START loading: %zu bytes built in memory\n",data.start_loading.size());
    result_printf(result_log,"scene-owner archives: meters=%u/10 bytes=%u optional_effect_3C/9=%u pack_complete=%u\n",
        meter_archive_count,unsigned(meter_archive_bytes),unsigned(optional_effect_bytes),
        unsigned(scene_resource_pack_complete));
    result_printf(result_log,"controls: A=PC frontend confirm, B=cancel, D-pad=navigate, X=race view, +=PC confirm/START, L1+R1+Options=exit\n");
    result_printf(result_log,"route: START_probe=%u X/Y_bypasses=disabled; original menu controller=partial\n",
        context.start_mode.original_route_requests);
    result_printf(result_log,"welcome controls=%u scene=%08x choice=%08x frame=%.1f stack push=%u pop=%u missing_key=%u missing_action=%08x\n",
        frontend.frontend_welcome_control_calls,frontend.frontend_welcome_scene_token,
        frontend.frontend_choice_scene_token,double(frontend.frontend_choice_scene_frame),
        frontend.frontend_stack_pushes,frontend.frontend_stack_pops,
        frontend.frontend_last_missing_key,frontend.frontend_last_missing_action);
    result_printf(result_log,"frontend overlay token=%08x draws=%u frames=%u\n",
        renderer_stats.frontend_overlay_token,renderer_stats.frontend_overlay_draws,
        renderer_stats.frontend_overlay_frames);
    result_printf(result_log,"title movie request=%d decoded=%llu displayed=%u boot_clear=%u error=%s\n",
        frontend.frontend_movie_request,(unsigned long long)diag.movie_decoded_total,
        renderer_stats.movie_frames,renderer_stats.unowned_frames,diag.movie_error.c_str());
    outrun::ps5_runtime::frame_profile_report(result_log);
    result_printf(result_log,"shared audio submitted=%llu stereo frames at 48000 Hz latency_cap_drops=%llu dropped_frames=%llu mixer_thread_pumps=%llu\n",
        (unsigned long long)audio.submitted_frames(),(unsigned long long)diag.audio_drops,(unsigned long long)diag.audio_dropped,(unsigned long long)rt::audio_thread().pumps.load());
    result_printf(result_log,"MENU PCM commands=%llu starts=%llu saturated=%llu stops=%llu mixed=%llu movie=%llu\n",
        (unsigned long long)diag.menu_audio.stats.commands,(unsigned long long)diag.menu_audio.stats.starts,
        (unsigned long long)diag.menu_audio.stats.saturated,(unsigned long long)diag.menu_audio.stats.stops,
        (unsigned long long)diag.menu_audio.stats.mixed_frames,(unsigned long long)diag.menu_audio.stats.movie_frames);
    result_printf(result_log,"%s\n",context.pc_sound.status().c_str());
    result_printf(result_log,"%s\n",outrun::platform::native_race_traffic_status(context).c_str());
    {const auto& g=outrun::platform::native_race_ghosts(context);
        result_printf(result_log,"car displays (ghost/traffic 4AE5F0/4ADAC0) displays=%u shadows=%u objects=%u faults=%u error=%s\n",g.displays,g.shadows,g.objects_drawn,g.display_faults,g.display_error.c_str());}
    result_printf(result_log,"music decoder=%u requests=%u stops=%u started=%u failed=%u last_track=%u frames=%llu loops=%llu rate=%u error=%s mix_error=%s\n",
        unsigned(outrun::platform::MusicStream::decoder_available()),frontend.music_play_requests,frontend.music_stop_requests,
        diag.music_starts,diag.music_failures,diag.music_track,(unsigned long long)diag.menu_audio.music().stats.frames,
        (unsigned long long)diag.menu_audio.music().stats.loops,diag.menu_audio.music().stats.rate,diag.music_error.c_str(),diag.menu_audio.music_error().c_str());
    result_printf(result_log,"PC profiles queried=%u loaded=%u invalid=%u selected=%u active=%u next_key=%u\n",
        unsigned(frontend.frontend_profiles.queried),frontend.frontend_profiles.loaded_count,
        frontend.frontend_profiles.invalid_files,frontend.frontend_profiles.selected,
        unsigned(frontend.frontend_profiles.active_loaded),
        outrun::platform::frontend_profiles_next_key(frontend.frontend_profiles));
    result_printf(result_log,"START requested=%u init=%u ctrl=%u stage=%u resource=%02x/2 ready=%u bytes=%u course=%u/%u events=%u race=%u route=%u countdown=%d game_requests=%u missing=%08x waits=%u\n",
                  context.start_mode.original_route_requests,context.start_mode.init_calls,
                  context.start_mode.control_calls,context.start_mode.stage,
                  context.start_mode.resource_id,context.start_mode.resource_ready,
                  context.start_mode.resource_bytes,
                  context.start_mode.course_load_success,context.start_mode.course_load_attempts,
                  context.start_mode.event_open_count,context.start_mode.race_event_count,
                  context.start_mode.stage60_route,context.start_mode.mode_countdown_780250,
                  context.start_mode.game_requests,context.start_mode.last_missing_service,
                  context.start_mode.gate_waits);
    result_printf(result_log,"START scene owner 49BA80 calls=%u stage=%u sprite 2B/8 requests=%u bytes=%u next service=%08x\n",
        context.start_mode.scene_owner_calls,context.start_mode.scene_owner_stage,
        context.start_mode.scene_owner_sprite_requests,
        context.start_mode.scene_owner_sprite_bytes,
        context.start_mode.scene_owner_missing_service);
    result_printf(result_log,"START scene audio reset=%u channels_active=%u command=%u phase=%u scheduler_passes=%u sound_checks=%u stage22_requests=%u\n",
        context.start_mode.scene_owner_audio_resets,
        context.start_mode.scene_owner_audio_channels_active,
        context.start_mode.scene_owner_audio_command,
        context.start_mode.scene_owner_audio_phase,
        context.start_mode.scene_owner_scheduler_passes,
        context.start_mode.scene_owner_sound_checks,
        context.start_mode.scene_owner_stage22_requests);
    result_printf(result_log,"START scene resources requests=%u bytes=%u releases=%u selected_meter=%02x/8 scene_state=%u known=%u marker=%d\n",
        context.start_mode.scene_owner_resource_count,
        context.start_mode.scene_owner_resource_bytes,
        context.start_mode.scene_owner_release_count,
        context.start_mode.scene_owner_meter_id,
        context.start_mode.scene_state_8421c0,
        context.start_mode.scene_state_8421c0_known?1u:0u,
        context.start_mode.scene_owner_course_marker);
    result_printf(result_log,"START world descriptor=%u ids=%03x/%03x/%03x ready=%u stage51_requests=%u\n",
        context.start_mode.scene_owner_world_descriptor_index,
        context.start_mode.scene_owner_world_ids[0],
        context.start_mode.scene_owner_world_ids[1],
        context.start_mode.scene_owner_world_ids[2],
        context.start_mode.scene_owner_world_ids_ready?1u:0u,
        context.start_mode.scene_owner_stage51_requests);
    result_printf(result_log,"START world source=%u collision state=%u requests=%u bytes=%u reset=%u env=%u/%u bytes=%u\n",
        world_source_path.empty()?0u:1u,
        context.start_mode.scene_owner_collision_state,
        context.start_mode.scene_owner_collision_requests,
        context.start_mode.scene_owner_collision_bytes,
        context.start_mode.scene_owner_world_reset_count,
        context.start_mode.scene_owner_environment_ready,
        context.start_mode.scene_owner_environment_requests,
        context.start_mode.scene_owner_environment_bytes);
    const auto& environment=context.start_mode.scene_owner_environment;
    result_printf(result_log,"START native environment inputs=%u initialized=%u fixups=%u phase=%u spline=%u active=%u error=%s\n",
        environment.inputs_ready()?1u:0u,environment.initialized()?1u:0u,
        context.start_mode.scene_owner_environment_fixups,
        context.start_mode.environment_phase_7d28c8,
        environment.layout().spline_records,environment.layout().spline_active,
        context.start_mode.scene_owner_environment_error.empty()?"none":
            context.start_mode.scene_owner_environment_error.c_str());
    result_printf(result_log,"PC renderer device=%s callbacks=%u %s\n",
        pc_scene_error.empty()?"ok":pc_scene_error.c_str(),diag.pc_scene_callbacks,pc_scene_summary.c_str());
    result_printf(result_log,"%s\n",outrun::platform::native_race_sound_status(context).c_str());
    result_printf(result_log,"%s\n",outrun::platform::native_sprite2d_status(context).c_str());
    result_printf(result_log,"%s",outrun::platform::native_race_hud_report(context).c_str());
    result_printf(result_log,"START geometry allocations=%u/%u bytes=%u states=%u/%u resets=%u error=%s\n",
        context.start_mode.scene_owner_course_object_ready,
        context.start_mode.scene_owner_course_object_requests,
        context.start_mode.scene_owner_course_object_bytes,
        context.start_mode.scene_owner_course_object_states[0],
        context.start_mode.scene_owner_course_object_states[1],
        context.start_mode.scene_owner_course_objects_reset_count,
        context.start_mode.scene_owner_course_object_error.empty()?"none":
            context.start_mode.scene_owner_course_object_error.c_str());
    const auto& selected_world=context.start_mode.scene_owner_course_world;
    const bool selected_root=selected_world.lane_loaded(0u);
    const auto selected_layout=selected_root?selected_world.lane(0u).pc_layout:
        outrun::platform::CourseCollisionLayout{};
    result_printf(result_log,"START native collision root=%u query_ready=%u polygons=%u primary=%u lengths=%u error=%s\n",
        selected_root?1u:0u,selected_world.query_ready()?1u:0u,
        selected_layout.polygon_count,selected_layout.primary_polygon_count,
        selected_layout.primary_length_count,
        context.start_mode.scene_owner_collision_error.empty()?"none":
            context.start_mode.scene_owner_collision_error.c_str());
    result_printf(result_log,"START race mapping=%u/%u key=%u/%u known=%u record=%u course_records=%u\n",
        race_assets_path.empty()?0u:1u,race_assignment_path.empty()?0u:1u,
        context.start_mode.scene_owner_race_key,
        context.start_mode.scene_owner_race_sub_key,
        context.start_mode.scene_owner_race_key_known?1u:0u,
        context.start_mode.scene_owner_race_record_index,
        context.start_mode.scene_owner_race_course_count);
    result_printf(result_log,"SUMO_FE source=224 scenes; full-screen static first-frame pack=124 scenes/2428 draws/82 textures\n");
    result_printf(result_log,"metadata: %s\ncourse collision: %s\nSTART loading: %s\nloader assets: %s\ncourse assets: %s\nstage17 assets: %s\ndriving data: %s\n",metadata_path.c_str(),course_collision_path.c_str(),"in memory",loader_assets_path.c_str(),course_assets_path.c_str(),stage17_assets_path.c_str(),driving_data_path.c_str());
    result_printf(result_log,"race routes: %s\nrace assignment: %s\n",
        race_assets_path.empty()?"unavailable":race_assets_path.c_str(),
        race_assignment_path.empty()?"unavailable":race_assignment_path.c_str());
    result_printf(result_log,"world source: %s\n",
        world_source_path.empty()?"unavailable":world_source_path.c_str());
    result_printf(result_log,"START loading authored scenes=%u textures=%u draws=%u GPU frames=%u selected=%u (static first frame; canvas-clipped, vertical DDS)\n",
        renderer_stats.loading_scenes,renderer_stats.loading_textures,
        renderer_stats.loading_draws,renderer_stats.loading_frames,
        0u);
    result_printf(result_log,"asset byte verification=%u loader=%u/%u shared=%u/%u frontend=%u/%u\n",
        unsigned(loader_bytes_match),context.event_function36.loader_ready_bytes,
        unsigned(expected_loader_bytes),context.event_function36.shared_ready_bytes,
        unsigned(expected_shared_bytes),context.event_function36.frontend_ready_bytes,
        unsigned(expected_frontend_bytes));
    result_printf(result_log,"runtime rc=%u frames=%u calls=%u cleanup=%u\n",rc,context.completed_frames,diag.calls,diag.cleanup_calls);
    result_printf(result_log,"event405 function=%u ctrl=%08x flags=%u\n",ev405.function_id,ev405.ctrl_callback,unsigned(ev405.flags));
    result_printf(result_log,"events=%u event405 init=%u ctrl=%u last=%u/%08x\n",diag.event_callbacks,diag.event_405_init,diag.event_405_ctrl,diag.last_event,diag.last_callback);
    result_printf(result_log,"event406 PC auxiliary function51 init=%u ctrl=%u dest=%u flags=%u (callbacks not ported)\n",
        diag.event_406_init,diag.event_406_ctrl,diag.event_406_dest,
        unsigned(context.event_state.slots[406].flags));
    result_printf(result_log,"input polls=%u seen=%016llx\n",native_input.sample_calls,(unsigned long long)diag.seen_buttons);
    result_printf(result_log,"PS5 renderer frames=%u failed=%u\n",renderer_stats.frames,unsigned(diag.render_failed));
    result_printf(result_log,"frontend scene token=%08x size=%ux%u textures=%u scenes=%u draws=%u frames=%u\n",
        renderer_stats.frontend_token,renderer_stats.frontend_scene_width,
        renderer_stats.frontend_scene_height,renderer_stats.frontend_textures,
        renderer_stats.frontend_scenes,renderer_stats.frontend_draws,renderer_stats.frontend_frames);
    result_printf(result_log,"authored frontend animation updates=%u frame=%.1f last=%.1f complete=%u START updates=%u frame=%.1f\n",
        renderer_stats.frontend_animation_updates,
        renderer_stats.frontend_animation_frame,
        renderer_stats.frontend_animation_last_frame,
        renderer_stats.frontend_animation_complete,
        renderer_stats.loading_animation_updates,
        renderer_stats.loading_animation_frame);
    result_printf(result_log,"frontend PC sprite instances=%u draws=%u frames=%u missing_ui=%08x\n",
        renderer_stats.frontend_instances,renderer_stats.frontend_instance_draws,
        renderer_stats.frontend_instance_frames,context.event_function36.frontend_ui_missing_pc);
    result_printf(result_log,"diagnostic frontend view transitions=%u visible=%u gate=%u scene_changes=%u\n",
        diag.frontend_view_toggles,
        unsigned(diag.frontend_visible),unsigned(diag.frontend_gate_open),
        renderer_stats.frontend_scene_changes);
    result_printf(result_log,"frontend owner index=%u token=%08x diagnostic_navigation=%u PC_confirm=%u preview=%u cancel=%u committed=%u\n",
        frontend.frontend_menu_index,frontend.frontend_menu_token,
        frontend.frontend_menu_navigation,frontend.frontend_menu_confirms,
        frontend.frontend_menu_preview_requests,
        frontend.frontend_menu_cancels,unsigned(frontend.frontend_menu_committed));
    result_printf(result_log,"course camera eye=%.2f/%.2f/%.2f speed=%.3f yaw=%.3f\n",
        context.game_mode.gameplay_camera_eye[0],
        context.game_mode.gameplay_camera_eye[1],context.game_mode.gameplay_camera_eye[2],
        context.game_mode.gameplay_speed,context.game_mode.gameplay_yaw);
    result_printf(result_log,"vehicle position=%.2f/%.2f/%.2f normal=%.3f/%.3f/%.3f spin=%d\n",
        context.game_mode.gameplay_position[0],context.game_mode.gameplay_position[1],context.game_mode.gameplay_position[2],
        context.game_mode.gameplay_ground_normal[0],context.game_mode.gameplay_ground_normal[1],context.game_mode.gameplay_ground_normal[2],
        static_cast<int>(context.game_mode.gameplay_vehicle_output.wheels[0].spin));
    result_printf(result_log,"pc vehicle frames=%u input=%u steer=%u transmission=%u wheel_angles=%u gear=%u angle=%d\n",
        context.game_mode.gameplay_vehicle.frames,context.game_mode.gameplay_vehicle.operation_input_calls,
        context.game_mode.gameplay_vehicle.steering_calls,context.game_mode.gameplay_vehicle.transmission_calls,
        context.game_mode.gameplay_vehicle.wheel_angle_updates,context.game_mode.gameplay_vehicle_output.gear,
        static_cast<int>(context.game_mode.gameplay_vehicle_output.steering_angle));
    result_printf(result_log,"pc steering full_lock=%u normalized=%.4f yaw_step=%.6f peak=%.4f/%.6f\n",
        static_cast<unsigned>(context.game_mode.gameplay_vehicle.steering_full_lock),
        context.game_mode.gameplay_steering_normalized,context.game_mode.gameplay_yaw_step,
        context.game_mode.gameplay_steering_peak,context.game_mode.gameplay_yaw_step_peak);
    result_printf(result_log,"pc vehicle shifts up=%u down=%u changes=%u collision_halts=%u event_ptr=%08x\n",
        context.game_mode.gameplay_vehicle.shift_up_requests,context.game_mode.gameplay_vehicle.shift_down_requests,
        context.game_mode.gameplay_vehicle.gear_changes,context.game_mode.gameplay_vehicle.rejected_motion,
        pc_vehicle_event.u32(0x2b4u));
    result_printf(result_log,"retail driving car=%u column=%u params=%u torque_tables=%u/%u brake_samples=%u\n",
        context.game_mode.gameplay_vehicle.retail_car_id,context.game_mode.gameplay_vehicle.retail_parameter_column,
        static_cast<unsigned>(driving_data.parameter_arena.size()),
        static_cast<unsigned>(driving_data.torque_tables[0].size()),
        static_cast<unsigned>(driving_data.torque_tables[1].size()),
        static_cast<unsigned>(driving_data.brake_table.size()/4u));
    result_printf(result_log,"retail drivetrain torque_calls=%u brake_calls=%u rpm=%.3f torque=%.3f pressure=%.6f\n",
        context.game_mode.gameplay_vehicle.engine_torque_calls,context.game_mode.gameplay_vehicle.brake_pressure_calls,
        context.game_mode.gameplay_vehicle_output.engine_rpm,context.game_mode.gameplay_vehicle_output.drive_torque,
        context.game_mode.gameplay_vehicle_output.brake_pressure);
    const outrun::driving::Bytes retail_parameters(
        context.game_mode.gameplay_vehicle.parameters.data(),context.game_mode.gameplay_vehicle.parameters.size());
    result_printf(result_log,"retail tach idle=%.1f redline=%.1f ceiling=%.1f rpm\n",
        retail_parameters.f32(0x15f8u)*9.5492965855f,
        retail_parameters.f32(0x1644u)*9.5492965855f,
        retail_parameters.f32(0x1690u)*9.5492965855f);
    result_printf(result_log,"four-wheel contacts frames=%u hits=%u mask=%x suspension=%u tire_load=%u driving_control=%u\n",
        context.game_mode.gameplay_vehicle.road_contact_frames,context.game_mode.gameplay_vehicle.road_contact_wheel_hits,
        context.game_mode.gameplay_vehicle_output.road_contact_mask,context.game_mode.gameplay_vehicle.suspension_force_calls,
        context.game_mode.gameplay_vehicle.tire_load_calls,context.game_mode.gameplay_vehicle.driving_control_calls);
    result_printf(result_log,"wheel forces=%.3f/%.3f/%.3f/%.3f loads=%.3f/%.3f/%.3f/%.3f\n",
        context.game_mode.gameplay_vehicle_output.suspension_forces[0],context.game_mode.gameplay_vehicle_output.suspension_forces[1],
        context.game_mode.gameplay_vehicle_output.suspension_forces[2],context.game_mode.gameplay_vehicle_output.suspension_forces[3],
        context.game_mode.gameplay_vehicle_output.tire_loads[0],context.game_mode.gameplay_vehicle_output.tire_loads[1],
        context.game_mode.gameplay_vehicle_output.tire_loads[2],context.game_mode.gameplay_vehicle_output.tire_loads[3]);
    result_printf(result_log,"road surfaces=%x/%x/%x/%x mu=%.3f/%.3f/%.3f/%.3f calls=%u\n",
        context.game_mode.gameplay_vehicle_output.road_surfaces[0],context.game_mode.gameplay_vehicle_output.road_surfaces[1],
        context.game_mode.gameplay_vehicle_output.road_surfaces[2],context.game_mode.gameplay_vehicle_output.road_surfaces[3],
        context.game_mode.gameplay_vehicle_output.road_mu[0],context.game_mode.gameplay_vehicle_output.road_mu[1],
        context.game_mode.gameplay_vehicle_output.road_mu[2],context.game_mode.gameplay_vehicle_output.road_mu[3],
        context.game_mode.gameplay_vehicle.road_mu_calls);
    result_printf(result_log,"original COLI0200 bytes=%u grid=65536 kinds=%u polygons=%u query=GetYPositionSplChk\n",
        static_cast<unsigned>(course_collision.pc_coli0200.size()),
        static_cast<unsigned>(course_collision.quads.size()),
        static_cast<unsigned>(course_collision.quads.size()));
    result_printf(result_log,"course collision quads=%u queries=%u hits=%u rejects=%u last=%u ground=%.2f chase=%.2f\n",
        static_cast<unsigned>(course_collision.quads.size()),context.game_mode.gameplay_ground_queries,
        context.game_mode.gameplay_ground_hits,context.game_mode.gameplay_ground_rejects,context.game_mode.gameplay_last_quad,
        context.game_mode.gameplay_position[1]-0.02f,context.game_mode.gameplay_chase_height);
    result_printf(result_log,"swept contacts poses=%u pose_filter=%u limited=%u/%u angle=%.5f/%.5f height_step=%.5f\n",
        context.game_mode.gameplay_sweep_poses,context.game_mode.gameplay_pose_filter.updates,
        context.game_mode.gameplay_pose_filter.normal_limited,context.game_mode.gameplay_pose_filter.height_limited,
        context.game_mode.gameplay_pose_filter.max_target_angle,context.game_mode.gameplay_pose_filter.max_applied_angle,
        context.game_mode.gameplay_pose_filter.max_applied_height);
    result_printf(result_log,"native ev36 init=%u ctrl=%u disp=%u destroy=%u\n",
        context.event_function36.init_calls,context.event_function36.control_calls,
        context.event_function36.display_calls,context.event_function36.destroy_calls);
    result_printf(result_log,"mode lifecycle current=%u previous=%u init=%u ctrl=%u exit=%u callbacks=%u\n",
        context.mode_state.current,context.mode_state.previous,
        context.mode_state.init_callbacks,context.mode_state.control_callbacks,
        context.mode_state.exit_callbacks,diag.mode_callbacks);
    result_printf(result_log,"mode32 reset=%u event406 setup=%u close=%u game_requests=%u\n",
        context.mode_state.sumo_fe_reset_calls,
        context.mode_state.sumo_fe_event_setup_calls,
        context.mode_state.sumo_fe_event_close_calls,
        context.mode_state.sumo_fe_game_requests);
    result_printf(result_log,"mode32 PC owner command polls=%u last=%08x mailbox=%08x (producer still diagnostic)\n",
        context.mode_state.sumo_fe_owner_command_polls,
        context.mode_state.sumo_fe_last_owner_command,event36_object.u32(0x21cu));
    result_printf(result_log,"game16 active=%u init=%u ctrl=%u exit=%u previous=%u\n",
        unsigned(context.game_mode.active),context.game_mode.init_calls,
        context.game_mode.control_calls,context.game_mode.exit_calls,
        context.game_mode.previous_mode);
    result_printf(result_log,"game16 resume=%u/%u empty440d90=%u effect=%u/%x timing_reset=%u polls=%u/%u\n",
        context.game_mode.event_resume_calls,context.game_mode.event_resume_slots,
        context.game_mode.empty_440d90_calls,context.game_mode.global_effect_calls,
        context.game_mode.global_effect_last,context.game_mode.timing_reset_calls,
        context.game_mode.owner_command_polls,context.game_mode.menu_state_polls);
    result_printf(result_log,"game16 PC owner command last=%08x\n",
        context.game_mode.last_owner_command);
    result_printf(result_log,"course load=%u success=%u reads=%u bytes=%u provider=%u phase=%u sync=%u\n",
        context.game_mode.course_load_calls,context.game_mode.course_load_success,
        context.game_mode.course_read_calls,context.game_mode.course_bytes,
        unsigned(context.game_mode.course_provider_loaded),context.game_mode.course_loader_phase,
        context.game_mode.course_load.force_sync_7d2d8c);
    result_printf(result_log,"course descriptors=%u/%u records=%d depth=%d selected=%d copy=%u matrix=%u\n",
        static_cast<unsigned>(course_assets.descriptors.primary_count),
        static_cast<unsigned>(course_assets.descriptors.secondary_count),
        context.game_mode.course_runtime.active_count_7d33c4,
        context.game_mode.course_runtime.max_depth_7d33c0,
        context.game_mode.course_runtime.selected_index,
        unsigned(context.game_mode.course_runtime.selected_copy_active),
        unsigned(context.game_mode.course_matrix_ready));
    result_printf(result_log,"ev36 owner state=%u loader stage=%u\n",event36_owner_state,event36_loader_stage);
    result_printf(result_log,"loader begin=%u requests=%u last=%x/%u\n",
        context.event_function36.loader_begin_calls,
        context.event_function36.loader_request_calls,
        context.event_function36.loader_last_request,
        context.event_function36.loader_last_request_mode);
    result_printf(result_log,"loader ready=%u bytes=%u gate=%u pending=%u\n",
        context.event_function36.loader_ready_count,
        context.event_function36.loader_ready_bytes,
        context.event_function36.loader_stage4_passes,
        context.event_function36.loader_resource_pending);
    result_printf(result_log,"shared loader stage=%u calls=%u requests=%u last=%x/%u\n",
        context.event_function36.shared_loader.stage_83db18,
        context.event_function36.shared_loader_calls,
        context.event_function36.shared_resource_requests,
        context.event_function36.shared_last_resource,
        context.event_function36.shared_last_mode);
    result_printf(result_log,"shared ready=%u bytes=%u pending=%u release=%u reset=%u final=%u\n",
        context.event_function36.shared_ready_count,
        context.event_function36.shared_ready_bytes,
        context.event_function36.shared_resource_pending,
        context.event_function36.shared_release_calls,
        context.event_function36.shared_table_reset_calls,
        context.event_function36.shared_finalize_calls);
    result_printf(result_log,"frontend request=%u ready=%u bytes=%u pending=%u release=%u\n",
        context.event_function36.frontend_resource_requests,
        context.event_function36.frontend_ready_count,
        context.event_function36.frontend_ready_bytes,
        context.event_function36.frontend_resource_pending,
        context.event_function36.frontend_release_calls);
    result_printf(result_log,"select table open=%u read=%u close=%u bytes=%u\n",
        context.event_function36.select_table_open_calls,
        context.event_function36.select_table_read_calls,
        context.event_function36.select_table_close_calls,
        context.event_function36.select_table_bytes);
    result_printf(result_log,"frontend init=%u async=%u bulk=%u stage12=%u\n",
        context.event_function36.frontend_init_calls,
        context.event_function36.frontend_async_polls,
        context.event_function36.frontend_bulk_polls,
        context.event_function36.loader_stage12_passes);
    result_printf(result_log,"frontend scripts requests=%u ready=%u bytes=%u pending=%u last=%u\n",
        context.event_function36.frontend_bulk_requests,
        context.event_function36.frontend_bulk_ready_count,
        context.event_function36.frontend_bulk_ready_bytes,
        context.event_function36.frontend_bulk_pending,
        context.event_function36.frontend_bulk_last_lane);
    result_printf(result_log,"frontend scripts format=%u binds=%u\n",
        context.event_function36.frontend_bulk_format_calls,
        context.event_function36.frontend_bulk_bind_calls);
    result_printf(result_log,"rankings menu selector=%08x modes=%u/%08x board48=%u/%08x board50=%u/%08x\n",frontend.ranking_selector_fault,
        unsigned(frontend.mode_board.constructed),frontend.mode_board.fault,unsigned(frontend.board48.constructed),frontend.board48.fault,
        unsigned(frontend.board50.constructed),frontend.board50.fault);
    {const auto& sr=frontend.showroom;const outrun::driving::Bytes o(const_cast<std::uint8_t*>(sr.object.data()),sr.object.size());
     result_printf(result_log,"showroom constructed=%u fault=%08x state=%u item_base=%d rows=%u glyphs=%u\n",
        unsigned(sr.constructed),sr.fault,o.u32(0x37c),o.i32(0xbe0),unsigned(sr.list?sr.list->size():0u),unsigned(sr.glyphs.size()));}
    result_printf(result_log,"frontend course records=%u/%u/%u/%u script_missing=%08x\n",
        frontend.frontend_course_tables[0].count,frontend.frontend_course_tables[1].count,
        frontend.frontend_course_tables[2].count,frontend.frontend_course_tables[3].count,
        frontend.frontend_bulk_missing_pc);
    result_printf(result_log,"license bank initialized=%u files=%u invalid=%u selected=%u active=%u save_path=%s\n",
        unsigned(frontend.frontend_profiles_initialized),frontend.frontend_profiles.loaded_count,
        frontend.frontend_profiles.invalid_files,frontend.frontend_profiles.selected,
        unsigned(frontend.frontend_profiles.active_loaded),frontend.frontend_save_directory.c_str());
    result_printf(result_log,"license owners key21=%u key24=%u shared_missing=%08x (effect provider pending)\n",
        outrun::driving::Bytes(license_owners.storage(21),license_owners.size(21)).u32(0x38),
        outrun::driving::Bytes(license_owners.storage(24),license_owners.size(24)).u32(0x38),
        frontend.frontend_ui_missing_pc);
    result_printf(result_log,"license persistence saves=%u deletes=%u error=%d\n",
        license_owners.save_calls(),license_owners.delete_calls(),license_owners.persistence_error());
    result_printf(result_log,"stage12 body calls=%u stage14=%u list=%u handle=%08x\n",
        context.event_function36.loader_stage12_body_calls,
        context.event_function36.loader_stage14_passes,
        stage12_list_count,stage12_first_handle);
    result_printf(result_log,"stage12 init global=%u embedded=%u/%u dispatch=%u factory=%u/%u ready=%u\n",
        context.event_function36.stage12_global_init_calls,
        context.event_function36.stage12_embedded_init_calls,
        context.event_function36.stage12_embedded_reset_calls,
        context.event_function36.stage12_dispatch_calls,
        context.event_function36.stage12_factory_allocate_calls,
        context.event_function36.stage12_factory_construct_calls,
        context.event_function36.stage12_ready_checks);
    result_printf(result_log,"stage12 final transition=%u scene=%u manager=%u audio=%u optional=%u\n",
        context.event_function36.stage12_transition_init_calls,
        context.event_function36.stage12_scene_init_calls,
        context.event_function36.stage12_manager_reset_calls,
        context.event_function36.stage12_audio_calls,
        context.event_function36.stage12_optional_calls);
    result_printf(result_log,"state2 frames=%u transition=%u ui_state=%u ui_tick=%u embedded=%u\n",
        frontend.state2_frames,frontend.state2_transition_ticks,
        frontend.state2_ui_state_ticks,frontend.state2_ui_ticks,
        frontend.state2_embedded_ticks);
    result_printf(result_log,"state2 gate=%u runtime_transition=%u queue=%u shutdown=%u selector polls=%u action=%u child_state=%u\n",
        frontend.state2_gate_ticks,frontend.state2_runtime_transition_ticks,
        frontend.state2_queue_ticks,frontend.state2_shutdown_ticks,
        frontend.state2_selector_calls,frontend.state2_selector_last_result,
        frontend.state2_selector_child_state);
    result_printf(result_log,"PC frontend key22 ctrl=%u owner_actions=%u key1_open=%u key2_open=%u gate_anim_pending=%u handle_count=%u\n",
        frontend.frontend_gate_control_calls,frontend.frontend_gate_owner_actions,
        frontend.frontend_first_menu_opens,frontend.frontend_second_menu_opens,
        frontend.frontend_gate_animation_pending,
        static_cast<unsigned>(frontend.frontend_handle_count));
    result_printf(result_log,"title key44 gate=%u construct=%u init=%u ctrl=%u controller=%u initial_ui=%u stage=%u entries=%u texts=%03x/%03x next_missing=%08x feature_mask=%08x\n",
        frontend.title_gate_ticks,frontend.title_construct_calls,frontend.title_init_calls,
        frontend.title_control_calls,frontend.title_controller_ticks,
        frontend.title_initial_ui_calls,
        (std::uint32_t(frontend.title_owner_object[0x9acu])|
         (std::uint32_t(frontend.title_owner_object[0x9adu])<<8u)|
         (std::uint32_t(frontend.title_owner_object[0x9aeu])<<16u)|
         (std::uint32_t(frontend.title_owner_object[0x9afu])<<24u)),
        static_cast<unsigned>(frontend.title_initial_ui.entry_count),
        frontend.title_initial_ui.text_ids[0],frontend.title_initial_ui.text_ids[1],
        frontend.title_last_missing_service,frontend.title_player0_feature_mask);
    result_printf(result_log,"state2 ui4447d0=%u valid=%u invalid=%u key=%u token=%08x\n",
        frontend.state2_ui_open_calls,frontend.state2_ui_valid_calls,
        frontend.state2_ui_invalid_calls,frontend.state2_ui_open_last_key,
        frontend.state2_ui_last_token);
    result_printf(result_log,"state2 ui465860=%u create=%u/%u property=%u final=%u release=%u resource=%08x\n",
        frontend.state2_ui_configure_calls,frontend.state2_ui_create3_calls,
        frontend.state2_ui_create5_calls,frontend.state2_ui_property_calls,
        frontend.state2_ui_finalize_calls,frontend.state2_ui_release_calls,
        frontend.state2_ui_resource_handle);
    result_printf(result_log,"state2 handles=%u active=%08x factory22=%u/%u/%u\n",
        static_cast<unsigned>(frontend.frontend_handle_count),state2_gate_handle,
        frontend.state2_callback_dispatch_calls,frontend.state2_factory_allocate_calls,
        frontend.state2_factory_construct_calls);
    result_printf(result_log,"state2 ready=%u quiet transition=%u configure=%u action=%u\n",
        frontend.state2_ready_checks,frontend.state2_transition_activations,
        frontend.state2_gate_configure_calls,frontend.state2_gate_action_calls);
    result_printf(result_log,"platform waits=%u polls=%u freq=%llu\n",native_platform.wait_calls,native_platform.poll_calls,(unsigned long long)native_platform.cached_frequency);
    result_printf(result_log,"\nOriginal welcome and license owners/persistence are connected; effect audio and subsequent menu handlers remain incomplete. START/race are not certified playable. Missing controllers are logged, never replaced with a diagnostic drive.\n");
    if(result_log)result_printf(result_log,"PS5 result log: %s\n",result_log_path);
    else result_printf(nullptr,"PS5 result log unavailable: errno=%d\n",result_log_error);
    result_printf(result_log,"Use L1+R1+Options to exit.\n");
    if(result_log){std::fflush(result_log);std::fclose(result_log);}
    consoleUpdate(nullptr);
    wait_for_plus(pad);

    
    
    consoleExit(nullptr);

    // Diagnostic coverage is reported above; application errors determine exit.
    return rc==0u&&!diag.render_failed?0:3;
}
