#include "enhancements/frame_rate.hpp"
#include "runtime/game_host.hpp"
#include "platform/bulk_fallback.hpp"
#include "driving/pc_driving.hpp"
#include "platform/embedded_exe_data.hpp"
#include "platform/frontend_preview_pack.hpp"
#include "platform/native_race_effects.hpp"   // scn-efc
#include "platform/sprite_2d_runtime.hpp"
#include "platform/race_hud_runtime.hpp"
#include "platform/race_area_runtime.hpp"
#include "platform/frontend_ui_resources.hpp"
#include "platform/pc_screen.hpp"   // race AREA/SKY (events 390/391)
#include "platform/race_ghosts_runtime.hpp"   // Time Attack ghosts
#include "platform/race_end_runtime.hpp"      // race end modes 19..35
#include "platform/race_traffic_runtime.hpp"  // the C2C request manager (event 0x190)
#include "platform/race_course_passes.hpp"
#include "platform/pc_input_devices.hpp"     // 40BD80 / 40BE70 course passes
#include "platform/race_manager.hpp"
#include "platform/pc_vehicle_control.hpp"
#include "platform/vehicle_pose_filter.hpp"
#include <algorithm>
#include <array>
#include <cerrno>
#include <cmath>
#include <cstdarg>
#include <cstdlib>
#include <cstring>
#include <optional>
#include <set>
#include <utility>
#include "system/files.hpp"
#include "system/dev_hooks.hpp"
#ifdef _WIN32
#include <filesystem>
#endif
#include <cctype>

// Development hooks and traces of the host runners (Linux host_nro, macOS).
#if defined(OR2_HOST_NRO)||defined(OR2_MAC_HOST)
#define OR2_HOST_HOOKS 1
#endif

namespace outrun::runtime {
namespace pf=outrun::platform;
ProductNames& product(){static ProductNames n;return n;}

std::string& home(){static std::string h=".";return h;}
namespace {
std::string parent_dir(std::string path){
    while(!path.empty()&&(path.back()=='/'||path.back()=='\\'))path.pop_back();
    const auto pos=path.find_last_of("/\\");return pos==std::string::npos?std::string{}:path.substr(0,pos);
}
}
void set_home(int argc,char** argv){
    if(argc>0&&argv&&argv[0]){const auto exe=parent_dir(argv[0]);if(!exe.empty())home()=exe;}
    // nxlink --args "home=sdmc:/switch/outrun2": the data folder when the NRO was sent elsewhere.
    for(int i=1;i<argc;++i)if(argv[i]&&std::string(argv[i]).rfind("home=",0)==0)home()=std::string(argv[i]).substr(5);
}
CrashBreadcrumbs& crash_breadcrumbs(){static CrashBreadcrumbs c;return c;}
bool& cube_disabled(){static bool off=std::getenv("OR2_NOCUBE")!=nullptr;return off;}

// ---------------------------------------------------------------- autoplay
Autoplay& autoplay(){static Autoplay a;return a;}
std::uint64_t Autoplay::buttons(const std::string& n){
    static const struct{const char* n;std::uint64_t b;} names[]{{"A",PadA},{"B",PadB},{"X",PadX},{"Y",PadY},
        {"L",PadL},{"R",PadR},{"ZL",PadZL},{"ZR",PadZR},{"PLUS",PadPlus},
        {"MINUS",PadMinus},{"LS",PadStickL},{"RS",PadStickR},{"LEFT",PadLeft},{"UP",PadUp},{"RIGHT",PadRight},{"DOWN",PadDown}};
    std::uint64_t m=0;std::size_t p=0;
    while(p<=n.size()){const auto e=n.find('+',p);const auto t=n.substr(p,e==std::string::npos?std::string::npos:e-p);
        for(const auto& x:names)if(t==x.n)m|=x.b;if(e==std::string::npos)break;p=e+1;}
    return m;
}
std::uint64_t Autoplay::at(unsigned f)const{
    std::uint64_t m=0;std::size_t p=0;
    while(p<script.size()){
        auto e=script.find(',',p);if(e==std::string::npos)e=script.size();
        const auto item=script.substr(p,e-p);const auto c=item.find(':');
        if(c!=std::string::npos){const auto r=item.substr(0,c);const auto d=r.find('-');
            const unsigned a=unsigned(std::strtoul(r.c_str(),nullptr,10)),b=d==std::string::npos?a:unsigned(std::strtoul(r.c_str()+d+1,nullptr,10));
            if(f>=a&&f<=b)m|=buttons(item.substr(c+1));}
        p=e+1;}
    return m;
}
void Autoplay::load(){
    loaded=true;std::FILE* f=std::fopen((home()+"/autoplay.txt").c_str(),"rb");if(!f)return;
    char line[4096];
    while(std::fgets(line,sizeof line,f)){std::string l(line);while(!l.empty()&&(l.back()==10||l.back()==13||l.back()==32))l.pop_back();
        if(l.rfind("script=",0)==0)script+=(script.empty()?"":",")+l.substr(7);
        else if(l.rfind("nospec=1",0)==0)no_specialised_shaders=true;
        else if(l.rfind("nocube=1",0)==0)cube_disabled()=true;
        else if(l.rfind("trace=1",0)==0)trace=std::fopen((home()+"/trace.txt").c_str(),"wb");
        else if(l.rfind("frames=",0)==0)frames=unsigned(std::strtoul(l.c_str()+7,nullptr,10));
        else if(l.rfind("diag=",0)==0){diag_frame=unsigned(std::strtoul(l.c_str()+5,nullptr,10));const auto c=l.find(':');if(c!=std::string::npos)diag_mode=unsigned(std::strtoul(l.c_str()+c+1,nullptr,10));}}
    std::fclose(f);on=true;
}
void ensure_autoplay(GameHost& s){
    auto& a=autoplay();if(a.loaded)return;
    a.load();
    if(a.no_specialised_shaders&&s.presenter)s.presenter->disable_pc_specialised_shaders();
}
// ---------------------------------------------------------------- input replay
InputReplay& input_replay(){static InputReplay r;return r;}
namespace {
// One frame on file: connected, held, down, up, left x/y, right x/y (little-endian).
constexpr std::size_t ReplayFrameBytes=1+8*3+4*4;
void put_le(std::uint8_t* p,std::uint64_t v,unsigned n){for(unsigned k=0;k<n;++k)p[k]=std::uint8_t(v>>(8*k));}
std::uint64_t get_le(const std::uint8_t* p,unsigned n){std::uint64_t v=0;for(unsigned k=0;k<n;++k)v|=std::uint64_t(p[k])<<(8*k);return v;}
}
void InputReplay::load(){
    loaded=true;
    std::FILE* f=std::fopen((home()+"/replay.txt").c_str(),"rb");if(!f)return;
    char line[256]{};std::string what,name="replay.bin";
    if(std::fgets(line,sizeof line,f))what=line;
    if(std::fgets(line,sizeof line,f)){name=line;while(!name.empty()&&(name.back()==10||name.back()==13||name.back()==32))name.pop_back();}
    std::fclose(f);
    while(!what.empty()&&(what.back()==10||what.back()==13||what.back()==32))what.pop_back();
    const std::string path=home()+"/"+(name.empty()?std::string("replay.bin"):name);
    if(what=="record"){file=std::fopen(path.c_str(),"wb");if(file)mode=Mode::Record;}
    else if(what=="play"){file=std::fopen(path.c_str(),"rb");if(file)mode=Mode::Play;}
    std::fprintf(stderr,"input replay: %s %s\n",mode==Mode::Record?"recording to":mode==Mode::Play?"playing":"off,",path.c_str());
}
PadSnapshot InputReplay::filter(const PadSnapshot& live){
    if(!loaded)load();
    std::uint8_t b[ReplayFrameBytes];
    if(mode==Mode::Record){
        b[0]=live.connected?1u:0u;
        put_le(b+1,live.held,8);put_le(b+9,live.down,8);put_le(b+17,live.up,8);
        put_le(b+25,std::uint32_t(live.left.x),4);put_le(b+29,std::uint32_t(live.left.y),4);
        put_le(b+33,std::uint32_t(live.right.x),4);put_le(b+37,std::uint32_t(live.right.y),4);
        if(std::fwrite(b,1,sizeof b,file)!=sizeof b){close();return live;}
        if(++frames%600u==0u)std::fflush(file);
        return live;
    }
    if(mode==Mode::Play){
        if(std::fread(b,1,sizeof b,file)!=sizeof b){
            std::fprintf(stderr,"input replay: end after %llu frames, live pad\n",(unsigned long long)frames);
            close();return live;}
        ++frames;
        PadSnapshot p;
        p.connected=b[0]!=0;
        p.held=get_le(b+1,8);p.down=get_le(b+9,8);p.up=get_le(b+17,8);
        p.left={std::int32_t(get_le(b+25,4)),std::int32_t(get_le(b+29,4))};
        p.right={std::int32_t(get_le(b+33,4)),std::int32_t(get_le(b+37,4))};
        return p;
    }
    return live;
}
void InputReplay::close(){
    if(file){std::fclose(file);file=nullptr;}
    mode=Mode::Off;
}
void trace_mark(const char* tag){auto& a=autoplay();if(a.trace){std::fprintf(a.trace," %s",tag);std::fputc(10,a.trace);std::fflush(a.trace);}}

// ---------------------------------------------------------------- audio
namespace { bool g_audio_main_held=false; }
std::recursive_mutex& audio_mutex(){static std::recursive_mutex m;return m;}
void audio_main_acquire(){if(!g_audio_main_held){audio_mutex().lock();g_audio_main_held=true;}}
void audio_main_release(){if(g_audio_main_held){g_audio_main_held=false;audio_mutex().unlock();}}
AudioThread& audio_thread(){static AudioThread t;return t;}
void AudioThread::start(GameHost& s){
    if(run)return;
    run=true;failed=false;audio_main_acquire();
    auto body=[this,&s]{
        if(s.audio_thread_setup)s.audio_thread_setup();
        while(run){
            {std::lock_guard<std::recursive_mutex> lock(audio_mutex());
             if(run&&s.audio_open&&!failed&&!pump_audio(s))failed=true;}
            ++pumps;
            std::this_thread::sleep_for(std::chrono::milliseconds(4));
        }
    };
    if(s.spawn_audio_thread)join=s.spawn_audio_thread(body);else thread=std::thread(body);
}
void AudioThread::stop(){
    if(!run)return;
    const bool held=g_audio_main_held;audio_main_release();
    run=false;if(join){join();join=nullptr;}else if(thread.joinable())thread.join();
    if(held)audio_main_acquire();
}
bool pump_audio(GameHost& s){
    std::lock_guard<std::recursive_mutex> audio_lock(audio_mutex());
    // The frame profile is main-thread only (its map is not shared): the mixer
    // thread's own time is not part of the game frame.
    std::optional<ProfileScope> prof;
    if(std::this_thread::get_id()!=audio_thread().thread.get_id())
        prof.emplace(profile_key(ProfileRenderer,0,ProfAudio));
    if(!s.audio_open||!s.audio_output)return false;
    if(!s.audio_clock_started){s.audio_clock=std::chrono::steady_clock::now();s.audio_clock_started=true;}
    const auto elapsed=std::chrono::steady_clock::now()-s.audio_clock;
    auto target=std::uint64_t(std::chrono::duration_cast<std::chrono::microseconds>(elapsed).count())*48u/1000u;
#if defined(OR2_HOST_HOOKS)
    // Host listening checks ($OR2_HOST_PCM): 800 frames per game frame (60 Hz), not the wall clock.
    static const bool frame_clock=std::getenv("OR2_HOST_PCM")!=nullptr;
    if(frame_clock)target=std::max<std::uint64_t>(s.menu_audio.stats.mixed_frames,s.game_frames*800u);
#endif
    target-=std::min(target,s.audio_dropped);
    const auto generated=s.menu_audio.stats.mixed_frames;
    if(target<generated){s.movie_error="audio clock moved backwards";return false;}
    // Latency cap: the output plays at the wall-clock rate, so every stall (a
    // 200 ms load hitch) would otherwise stay queued for good and delay every
    // later sound. Time beyond ~100 ms of queued output is skipped, not mixed.
    constexpr std::uint64_t MaxQueuedFrames=4800u;
    auto need=target-generated;const auto queued=std::uint64_t(s.audio_output->queued_frames());
    if(queued+need>MaxQueuedFrames){
        const auto drop=std::min<std::uint64_t>(need,queued+need-MaxQueuedFrames);
        s.audio_dropped+=drop;need-=drop;++s.audio_drops;
    }
    if(!s.menu_audio.mix(std::size_t(need),s.mixed_pcm,s.movie_error))return false;
    return s.audio_output->submit(s.mixed_pcm,s.movie_error);
}
namespace {
bool menu_effect(void* user,unsigned id){
    auto& s=*static_cast<GameHost*>(user);
    std::lock_guard<std::recursive_mutex> audio_lock(audio_mutex());
    // Drain elapsed time before starting a voice: input events must not be
    // retroactively mixed into samples belonging to the preceding frame.
    if(!pump_audio(s)){s.render_failed=true;return false;}
    if(s.runtime&&s.runtime->pc_sound.mapped(id)){s.runtime->pc_sound.effect_42f0d0(id);return true;}
    if(s.menu_audio.command(id,s.movie_error))return true;
    // Outside the frontend (race / race end modes) 4249F0 (42F0D0) also
    // plays the race SE banks, which the native audio does not load yet:
    // counted and reported, the mode continues.
    if(s.runtime&&s.runtime->mode_state.current!=32u&&s.movie_error.find("bank other than MENU.pak")!=std::string::npos){
        ++s.race_effects_unplayed[id&0x7ffu];s.movie_error.clear();return false;}
    s.render_failed=true;return false;
}
// 401000 (channel, track, loop): one stream (95B24C); a new request closes
// the previous one. The volume 95B240 is .bss 0 (> -10000: always started).
bool menu_music_play(void* user,unsigned,unsigned track,unsigned loop){
    auto& s=*static_cast<GameHost*>(user);
    std::lock_guard<std::recursive_mutex> audio_lock(audio_mutex());
    if(s.audio_open&&!pump_audio(s)){s.render_failed=true;return false;}   // mix the elapsed time with the old stream
    s.menu_audio.stop_music();
    const char* name=pf::music_track_name_771ad8(track);
    std::vector<std::uint8_t> bytes;std::string e;
    if(!name){e="music: track "+std::to_string(track)+" outside 771AD8";}
    else if(s.retail&&read_sound_file(s,name,bytes,e)&&s.menu_audio.play_music(std::move(bytes),loop!=0u,e)){
        ++s.music_starts;s.music_track=track;return true;}
    ++s.music_failures;s.music_error=e;return false;
}
bool menu_music_stop(void* user,unsigned){
    auto& s=*static_cast<GameHost*>(user);
    std::lock_guard<std::recursive_mutex> audio_lock(audio_mutex());
    if(s.audio_open&&!pump_audio(s)){s.render_failed=true;return false;}
    if(s.menu_audio.music().playing())++s.music_stops;
    s.menu_audio.stop_music();return true;
}
// Options > Audio: 42EFA0 (SE master, integer level) and 42FC90 (BGM, float bits).
bool menu_volume(void* user,unsigned pc,unsigned arg){
    auto& s=*static_cast<GameHost*>(user);
    std::lock_guard<std::recursive_mutex> audio_lock(audio_mutex());
    if(s.audio_open&&!pump_audio(s)){s.render_failed=true;return false;}   // mix the elapsed time at the old level
    if(pc==0x42efa0u&&s.runtime){s.runtime->pc_sound.master_volume_42efa0(arg);return true;}
    if(pc==0x42fc90u){float level;std::memcpy(&level,&arg,4);s.menu_audio.music_volume_42fc90(level);return true;}
    return false;
}
bool menu_platform(void* user,unsigned pc,const unsigned* args,std::size_t count,unsigned& result){
    if(pc!=0x4249f0||count!=1||!args)return false;
    result=0;return menu_effect(user,args[0]);
}
}
// Retail file names differ in case from the 771AD8 table (Beach_wave.ogg,
// or2ed1.ogg ...): exact name first, then a case-insensitive match in Sound/.
bool read_sound_file(GameHost& s,const char* name,std::vector<std::uint8_t>& bytes,std::string& error){
    if(pf::retail_asset_read_relative(*s.retail,std::string("Sound/")+name,bytes,64u*1024u*1024u,&error))return true;
    const auto dir=pf::retail_asset_path(*s.retail,"Sound");
    std::vector<std::string> names;
#ifdef _WIN32
    std::error_code ec;
    for(const auto& e:std::filesystem::directory_iterator(dir,ec))names.push_back(e.path().filename().string());
    if(ec){error="Sound folder missing";return false;}
#else
    bool opened=false;names=pf::directory_names(dir,&opened);
    if(!opened){error="Sound folder missing";return false;}
#endif
    std::string found;
    for(const auto& n:names){
        const char* a=n.c_str();const char* b=name;
        while(*a&&*b&&std::tolower((unsigned char)*a)==std::tolower((unsigned char)*b)){++a;++b;}
        if(!*a&&!*b){found=n;break;}
    }
    if(found.empty()){error=std::string("Sound/")+name+" missing";return false;}
    return pf::retail_asset_read_relative(*s.retail,"Sound/"+found,bytes,64u*1024u*1024u,&error);
}

// ---------------------------------------------------------------- runtime callbacks
namespace {
std::uint32_t startup_call(void* user,std::uint32_t pc,std::uint32_t,std::uint32_t,std::uint32_t){
    auto& s=*static_cast<GameHost*>(user);
    ++s.calls;
    if(pc==0x40e470u)return 1u;          // r090 platform-init gate succeeds.
    if(pc==0x4177e6u)return s.next_token++; // worker-thread semantic token.
    return 0u;
}

std::uint32_t setup_call(void* user,std::uint32_t pc,const std::uint32_t*,std::size_t){
    auto& s=*static_cast<GameHost*>(user);
    ++s.calls;
    if(pc==0x417a6fu||pc==0x417a93u||pc==0x417aebu)return s.next_token++;
    return 0u;
}

// The frontend lists of every owner, for the presenter and the PC 2D renderer.
void frontend_frame(GameHost& s){
    auto& p=*s.presenter;
    const auto& frontend=s.runtime->event_function36;
    std::vector<pf::FrontendGlyph> glyphs;
    std::vector<pf::FrontendListImage> images;
    std::vector<pf::FrontendWindowIcon> icons;
    const auto append=[&](const auto& widgets){
        glyphs.insert(glyphs.end(),widgets.glyphs().begin(),widgets.glyphs().end());
        images.insert(images.end(),widgets.images().begin(),widgets.images().end());
        icons.insert(icons.end(),widgets.icons().begin(),widgets.icons().end());
    };
    const auto append_board=[&](const auto& board){
        glyphs.insert(glyphs.end(),board.glyphs.begin(),board.glyphs.end());
        images.insert(images.end(),board.images.begin(),board.images.end());
    };
    if(frontend.title_widgets)append(*frontend.title_widgets);
    append(frontend.network_widgets);
    if(frontend.license_owners)for(unsigned key:{21u,24u})
        if(const auto* widgets=frontend.license_owners->widgets(key))append(*widgets);
    glyphs.insert(glyphs.end(),frontend.frontend_categories.glyphs.begin(),frontend.frontend_categories.glyphs.end());
    glyphs.insert(glyphs.end(),frontend.frontend_missions.glyphs.begin(),frontend.frontend_missions.glyphs.end());
    glyphs.insert(glyphs.end(),frontend.frontend_requests.glyphs.begin(),frontend.frontend_requests.glyphs.end());
    append_board(frontend.rankings);
    append_board(frontend.ghost_board);
    append_board(frontend.course_board);
    append_board(frontend.board50);
    append_board(frontend.board48);
    append_board(frontend.showroom);
    icons.insert(icons.end(),frontend.showroom.icons.begin(),frontend.showroom.icons.end());
    // 49E4B0 ends with 442E00 -> 446A50: the help bar captions of the owner's button slots (+0x51C).
    if(frontend.object.size()>=0x51cu+0x68cu)
        (void)pf::frontend_slot_captions_446a50(outrun::driving::Bytes(const_cast<std::uint8_t*>(frontend.object.data())+0x51c,0x68c),
                                                frontend.frontend_fonts,frontend.frontend_text,glyphs);
#if defined(OR2_HOST_HOOKS)
    if(std::getenv("OR2_FE_DUMP")&&s.game_frames%100u==0u){
        std::fprintf(stderr,"fe dump frame %u: glyphs=%zu images=%zu icons=%zu missions=%zu cats=%zu title=%zu visible=%d token=%08x overlay=%08x\n",
            unsigned(s.game_frames),glyphs.size(),images.size(),icons.size(),frontend.frontend_missions.glyphs.size(),
            frontend.frontend_categories.glyphs.size(),frontend.title_widgets?frontend.title_widgets->glyphs().size():0u,
            int(s.frontend_visible),frontend.frontend_menu_token,frontend.frontend_choice_scene_token);
        std::map<std::uint32_t,unsigned> tokens;
        for(const auto& sp:frontend.frontend_sprites.instances())if(sp.allocated)++tokens[sp.token];
        {const auto& e=s.runtime->event_state.slots[405];std::fprintf(stderr,"fe 405: flags %02x scene %x disp %x ctrl %x\n",e.flags,e.display_scene,e.disp_callback,e.ctrl_callback);}
        std::fprintf(stderr,"fe sprites:");
        for(const auto& [t,n]:tokens)std::fprintf(stderr," %x%s",t,n>1?("x"+std::to_string(n)).c_str():"");
        std::fprintf(stderr,"\n");
    }
#endif
    // The same lists go to the PC 2D renderer (queued at the 49E4B0 display).
    if(s.frontend_visible)pf::native_sprite2d_set_frontend(*s.runtime,glyphs,images,icons);
    else pf::native_sprite2d_set_frontend(*s.runtime,{},{},{});
    std::lock_guard<std::recursive_mutex> movie_audio_lock(audio_mutex());
    if(s.movie_generation!=frontend.frontend_movie_generation){
        s.movie_generation=frontend.frontend_movie_generation;
        if(frontend.frontend_movie_request==0||frontend.frontend_movie_request==1){
            // A movie that cannot be opened stays inactive: the error is kept
            // for the log and the frontend continues without it.
            (void)s.title_movie.open(s.movie_path,s.movie_error);
            s.menu_audio.stop_movie();
            s.movie_decoded_total+=s.title_movie.decoded_frames();s.movie_uploaded_frame=~0ull;
            if(s.movie_clock_reset)s.movie_clock_reset();
            else s.movie_clock=std::chrono::steady_clock::now();
        }else{
            s.title_movie.close();
            s.menu_audio.stop_movie();s.movie_pcm.clear();
            if(!p.set_movie_frame(nullptr,0u,0u))s.render_failed=true;
        }
    }
    if(s.title_movie.active()&&!s.render_failed){
        const auto before=s.title_movie.decoded_frames();
        double elapsed;
        if(s.movie_clock_read){bool paused=false;elapsed=s.movie_clock_read(paused);if(paused)s.menu_audio.stop_movie();}
        else elapsed=std::chrono::duration<double>(std::chrono::steady_clock::now()-s.movie_clock).count();
        if(!s.title_movie.advance(elapsed,s.movie_error))s.render_failed=true;
        s.movie_decoded_total+=s.title_movie.decoded_frames()-before;
        s.title_movie.take_audio(s.movie_pcm);
        if(!s.render_failed&&s.title_movie.has_audio()&&!s.menu_audio.queue_movie(s.movie_pcm,s.movie_error))s.render_failed=true;
        if(!s.render_failed&&s.movie_uploaded_frame!=s.title_movie.decoded_frames()){
            if(!p.set_movie_frame(s.title_movie.rgba().data(),s.title_movie.width(),s.title_movie.height()))s.render_failed=true;
            else s.movie_uploaded_frame=s.title_movie.decoded_frames();
        }
    }
    if(!p.set_frontend_token(frontend.frontend_welcome_scene_token?
                                 frontend.frontend_welcome_scene_token:frontend.frontend_menu_token))s.render_failed=true;
    if(!p.set_frontend_overlay(frontend.frontend_choice_scene_token,frontend.frontend_choice_scene_frame))s.render_failed=true;
    const bool title_visible=frontend.title_widgets&&frontend.title_widgets->visible();
    if(!p.set_frontend_game_backdrop(title_visible))s.render_failed=true;
    const bool visible=(s.runtime->mode_state.current==32u&&!frontend.frontend_menu_committed)||title_visible;
    if(visible!=s.frontend_visible){s.frontend_visible=visible;++s.frontend_view_toggles;
        if(!p.set_frontend_visible(visible))s.render_failed=true;}
}

std::uint32_t frame_call(void* user,std::uint32_t pc,const std::uint32_t*,std::size_t){
    auto& s=*static_cast<GameHost*>(user);
    ++s.calls;
    // Force the single-update path while the real input/audio/render services
    // are still explicit boundaries.
    // Above 60 Hz (enhancements/frame_rate.hpp) frames run 0, 1 or more ticks.
    if(pc==0x455c20u)return enhancements::display_frames()?1u:2u;
    if(pc!=0x449050u||!s.presenter)return 0u;
    auto& p=*s.presenter;
    auto& mixer=audio_thread();
    audio_main_release();   // the mixer thread runs while the frame is recorded
    if(mixer.failed)s.render_failed=true;
    // The START loading picture covers START's loading stages only: from stage 65 the
    // race is loaded and START runs the start camera (4871A0) and the countdown
    // until GAME, which the PC shows over the race scene.
    const bool start_loading=s.runtime&&s.runtime->mode_state.current==13u&&s.runtime->start_mode.stage<65u;
    if(start_loading&&!p.set_start_loading_scene(0u))s.render_failed=true;
    if(!p.set_start_loading_visible(start_loading))s.render_failed=true;
    if(!s.frontend_gate_open&&s.runtime&&s.runtime->event_function36.state2_ui_finalize_calls!=0u){
        s.frontend_gate_open=true;s.frontend_visible=true;
        if(!p.set_frontend_visible(true))s.render_failed=true;
    }
    if(s.frontend_gate_open&&s.runtime)frontend_frame(s);
    {std::lock_guard<std::recursive_mutex> audio_lock(audio_mutex());
     ++s.game_frames;if(s.runtime)s.runtime->pc_sound.tick_42f330();
     if(!mixer.run&&!s.render_failed&&!pump_audio(s))s.render_failed=true;}
    trace_mark("pc_scene");
    // Widescreen: the PC screen the camera sees becomes 16:9 (its FOV 483C10
    // widens, Hor+); the camera's aspect +B8 (set by 484EE0 at init) follows.
    if(s.pc_scene&&s.runtime){
        const float width=s.widescreen?480.f*16.f/9.f:640.f;
        if(s.pc_scene->screen_width()!=width){
            s.pc_scene->set_screen(width,480.f);
            pf::g_pc_view_half_width=width*0.5f;
            auto& cam=s.runtime->event_function36.car_select.camera_79fe10;
            if(cam.size()>=0xbcu){const float aspect=width/480.f;std::memcpy(cam.data()+0xb8,&aspect,4);}
        }
    }
    if(s.pc_scene&&s.runtime&&!p.set_pc_scene([&s](pf::PcD3D9Device&){
           // 82E7D4/82E7D8: event 7 (init 4866D0, control 4874A0, goal 4871A0).
           s.pc_scene->scene_82e7d4=s.runtime->race.camera_override.scene_82e7d4;s.pc_scene->interpolation_override_82e7d8=s.runtime->race.camera_override.override_82e7d8;
           s.pc_scene->camera_preset_819634=s.runtime->event_function36.camera_preset_819634;   // 482E70 (Showroom)
           ProfileScope prof(profile_key(ProfileRenderer,0,ProfSceneRender));
           s.pc_scene->render_frame(s.runtime->event_state,s.runtime->mode_state.current);}))s.render_failed=true;
    bool drawn=false;
    {ProfileScope prof(profile_key(ProfileRenderer,0,ProfDrawTotal));
     trace_mark("draw");drawn=!s.render_failed&&p.draw();trace_mark("drawn");}
    if(!s.render_failed&&!drawn){
        s.render_failed=true;
    }else if(!s.render_failed&&s.runtime&&s.frontend_visible&&s.runtime->mode_state.current==32u){
        // Feed authored SUMO_FE completion back to the original key-22
        // owner on the next tick instead of inventing a one-frame finish.
        s.runtime->event_function36.frontend_authored_animation_ready=p.frontend_animation_complete();
    }
    if(mixer.run)audio_main_acquire();
    return 0u;
}

void cleanup_call(void* user,std::uint32_t,std::uint32_t,std::uint32_t){
    auto& s=*static_cast<GameHost*>(user);
    ++s.calls;
    ++s.cleanup_calls;
}

std::int32_t steering_byte(std::int32_t stick_x){
    return std::clamp(stick_x/256,-128,127);
}

// The pad as the game's inputs: the direct mapping, the frontend features,
// the PC device record of 453BB0 and the DirectInput pad of the PC input layer.
void map_pad(GameHost& s,platform::NativeInputState& out,std::uint64_t held,std::uint64_t down,const PadStick& left,const PadStick& right){
    out.left_stick_x=left.x;out.left_stick_y=left.y;
    out.right_stick_x=right.x;out.right_stick_y=right.y;
    out.steering=steering_byte(left.x);
    if((held&PadLeft)!=0u)out.steering=-128;
    if((held&PadRight)!=0u)out.steering=127;
    out.accelerator=(held&(PadZR|PadA))!=0u?255u:0u;
    out.brake=(held&(PadZL|PadB))!=0u?255u:0u;
    out.shift_up=(down&PadR)!=0u;
    out.shift_down=(down&PadL)!=0u;
    out.menu_left=(down&PadLeft)!=0u;
    out.menu_right=(down&PadRight)!=0u;
    out.menu_up=(down&PadUp)!=0u;
    out.menu_down=(down&PadDown)!=0u;
    out.menu_confirm=(down&(PadA|PadPlus))!=0u;
    out.menu_cancel=(down&PadB)!=0u;
    // Platform mapping only; PC 48F4F0 owns the deadzone/repeat and 48F5F0
    // owns simultaneous-key priority. Do not replace these axes with edges.
    const bool in_race=s.runtime&&(s.runtime->mode_state.current==16u||s.runtime->mode_state.current==18u);
    if(out.connected){
        out.frontend.axes={std::int16_t(steering_byte(left.x)),std::int16_t(steering_byte(-left.y)),
                           std::int16_t(steering_byte(right.x)),std::int16_t(steering_byte(-right.y))};
        // Start and A both confirm in 48F5F0, but 4D7300 separately checks
        // feature bit 1 to resume rather than select the highlighted row.
        out.frontend.feature_mask=((down&PadPlus)?1u:0u)|((down&PadA)?4u:0u)|(out.menu_cancel?8u:0u)|
            (out.menu_up?0x400u:0u)|(out.menu_left?0x1000u:0u)|
            (out.menu_down?0x800u:0u)|(out.menu_right?0x2000u:0u)|
            // In the race (modes 16 / 18) L/R are only the gear shifts (PC device bits 0x8 / 0x2):
            // feature 0x10 would reach the owner's selector 0x8000 (445817). + stays feature 1,
            // the start key the race owner state 3 turns into the pause menu (444470, key 0x2C).
            (in_race?0u:(((down&PadL)?0x10u:0u)|((down&PadR)?0x20u:0u)))|
            ((down&PadX)?0x8000u:0u)|((down&PadY)?0x4000u:0u)|
            ((down&PadX)?0x40000u:0u);   // view change (484A40: feature 0x40000; X is the PC device view button 0x10)
        // PC device bits 0x1000 / 0x2000 (4536C0, 407930 +8) are buttons of their own, not the
        // D-pad: 0x2000 toggles the car class (4C9290) and the music list (4C9940). They were
        // mapped to Left/Right, so Right also changed the class. ZL / ZR now (not used in menus).
        out.frontend.device_held=((down&PadZL)?0x1000u:0u)|((down&PadZR)?0x2000u:0u);   // one frame per press (4C9290 toggles on every frame it sees the bit)
    }
    // PC device record for 453BB0 (joystick configuration 5A7B50[0]: steer
    // axis 0x0E, accelerator 0x0D, brake 0x0C, shift up 0x2, shift down 0x8,
    // view 0x10; the pedals are 0..255 axis words, the D-pad the digital
    // steering bits filtered by 453860: 0x80 raises the filter toward +1 =
    // right, the stick's sign, 0x100 lowers it = left).
    {
        pf::PcInputDevice d{};
        if(out.connected){
            d.axes_94[0x0e]=std::int16_t(std::clamp<std::int32_t>(left.x,-32768,32767));
            d.axes_94[0x0d]=std::int16_t((held&(PadZR|PadA))?255:0);
            d.axes_94[0x0c]=std::int16_t((held&(PadZL|PadB))?255:0);
            d.buttons_04=((held&PadPlus)?0x1u:0u)|((held&PadR)?0x2u:0u)|((held&PadL)?0x8u:0u)|
                ((held&PadX)?0x10u:0u)|((held&PadRight)?0x80u:0u)|((held&PadLeft)?0x100u:0u);
            // Race-end prompts (modes 34/35: 4536F0 masks 4 and 8 = device
            // bits 0x2 and 0x4): B is device 0x4 (no race role in 5A7B50[0]);
            // A adds 0x2 outside GAME only (0x2 is shift up while racing).
            if(held&PadB)d.buttons_04|=0x4u;
            if((held&PadA)&&s.runtime&&s.runtime->mode_state.current!=16u)d.buttons_04|=0x2u;
        }
        out.pc_device=d;
    }
    // The pad as the DirectInput joystick of the PC input layer (pc_input_devices.hpp):
    // buttons 0 A, 1 B, 2 X, 3 Y, 4 L, 5 R, 6 Minus, 7 Plus, 8/9 stick clicks,
    // 10 ZL, 11 ZR; POV = D-pad; lX/lY left stick, lRx/lRy right stick (DirectInput
    // Y grows downward).
    out.pc_input_layer=s.pc_input_layer;
    if(out.connected){
        auto& p=out.pc_pad;
        static constexpr std::uint64_t order[12]{PadA,PadB,PadX,PadY,PadL,PadR,PadMinus,PadPlus,PadStickL,PadStickR,PadZL,PadZR};
        for(unsigned k=0;k<12;++k)p.buttons[k]=(held&order[k])?0x80u:0u;
        auto axis=[](std::int32_t v){return std::clamp<std::int32_t>(v,-32768,32767);};
        p.axes[0]=axis(left.x);p.axes[1]=axis(-left.y);p.axes[3]=axis(right.x);p.axes[4]=axis(-right.y);
        const bool u=held&PadUp,dn=held&PadDown,l=held&PadLeft,r=held&PadRight;
        p.pov=u?(r?4500u:l?31500u:0u):dn?(r?13500u:l?22500u:18000u):r?9000u:l?27000u:0xffffffffu;
    }
    // Production r155: neither X nor Y may bypass the original owner chain.
    out.menu_preview=false;
    out.menu_start_probe=false;
    out.start=(down&PadPlus)!=0u;
    const auto exit_mask=PadPlus|PadL|PadR;
    out.exit_requested=s.render_failed||((held&exit_mask)==exit_mask&&(down&exit_mask)!=0u);
}

#if defined(OR2_HOST_HOOKS)
// Host test hooks and traces (environment variables of the host runner).
void host_hooks(GameHost& s,std::uint64_t down){
    // Host test hook OR2_HOST_EXE_STATE=1: at the first race frame, the FNV-1a of the
    // context's copies of EXE tables (robots .data, particles .data / tire marks), so a
    // test can check that they were taken from an installed image (host_nro_cache_only).
    if(s.runtime&&s.runtime->mode_state.current==16u&&std::getenv("OR2_HOST_EXE_STATE")){
        static bool done=false;
        if(!done){
            done=true;
            auto fnv=[](const std::vector<std::uint8_t>& v){std::uint32_t h=0x811c9dc5u;for(auto b:v){h^=b;h*=0x01000193u;}return h;};
            const auto& r=*s.runtime;
            std::fprintf(stderr,"exe-state robots=%08x particles=%08x tire-marks=%08x\n",
                fnv(r.race.robots.state.data),fnv(r.race_effects.particles.data),fnv(r.race_effects.particles.tire_data));
        }
    }
    // Host test hook OR2_HOST_GOAL=N: the race manager's goal state (450230(1),
    // as its goal line crossing sets it) once the race has run N frames, to
    // exercise modes 19/33 without driving a whole route.
    if(s.runtime&&s.runtime->mode_state.current==16u)if(const char* g=std::getenv("OR2_HOST_GOAL")){
        static bool done=false;
        // OR2_HOST_GOAL=N:state:every re-arms for every race (each OutRun stage, to reach the ending).
        if(done&&std::strstr(g,":every")&&s.runtime->race.frames_8367f4<std::strtoul(g,nullptr,10))done=false;
        if(!done&&s.runtime->race.frames_8367f4>=std::strtoul(g,nullptr,10)){
            // OR2_HOST_GOAL=N[:state] (state 1 goal, 2 time over, ...).
            const char* colon=std::strchr(g,':');const std::uint32_t over=colon?std::uint32_t(std::strtoul(colon+1,nullptr,10)):1u;
            done=true;pf::race_set_over_state_450230(s.runtime->race.manager.state,over);
            std::fprintf(stderr,"[%u] host: race goal state set\n",s.input_samples);}
    }
    // Host test hook OR2_HOST_GOAL_COURSE=k: the player's course key ([[799D18]+68], 450380(8))
    // held at k during the goal modes 19 / 33, so the ending 4527F0 (mode 24 init) picks the
    // column of course k (10..14: the five last stages A..E).
    if(s.runtime&&(s.runtime->mode_state.current==19u||s.runtime->mode_state.current==33u))
        if(const char* k=std::getenv("OR2_HOST_GOAL_COURSE")){
            auto& area=pf::native_race_area_memory(*s.runtime,nullptr,false);
            const std::uint32_t car=area.u32(0x799d18u);if(car)area.put32(car+0x68u,std::uint32_t(std::strtoul(k,nullptr,0)));}
    // Host test hook OR2_HOST_GOAL_SCORE=n: the player's score ([8447DC + id*4], 4B99D0) held at n
    // during the goal modes 19 / 33, so the ranking of the name entry (mode 26) takes a place.
    if(s.runtime&&s.runtime->race_hud&&(s.runtime->mode_state.current==19u||s.runtime->mode_state.current==33u))
        if(const char* k=std::getenv("OR2_HOST_GOAL_SCORE")){
            auto& area=pf::native_race_area_memory(*s.runtime,nullptr,false);
            const std::uint32_t car=area.u32(0x799d18u);const std::uint32_t id=car?area.u32(car):0u;
            auto& g=s.runtime->race_hud->navi.g842800;const std::size_t at=(0x8447dcu-0x842800u)+std::size_t(id)*4u;
            if(id<0x20u&&at+4u<=g.size()){const std::uint32_t v=std::uint32_t(std::strtoul(k,nullptr,0));std::memcpy(g.data()+at,&v,4);}}
    // Host test hook OR2_HOST_OVERLAY=1: a sample of the console performance overlay (main.cpp
    // overlay_frame) to check where it lands over the HUD.
    if(std::getenv("OR2_HOST_OVERLAY"))pf::g_pc_overlay_text="FPS 59.9 16.7ms\nCPU 12.3ms 74%\nGPU 10.1ms 61%\nCLK 1020/768 MHz\nSND 40ms skip 0\n100% noAA 16:9";
    // Host test hook OR2_HOST_UNLOCK=b1,b2,...: licence unlock bits (4474E0: [7C23E0+0x28+b/8] bit b%8)
    // set in the active licence once a profile is loaded (locked menus such as the 15 stage modes).
    if(s.runtime)if(const char* u=std::getenv("OR2_HOST_UNLOCK")){
        auto& lic=s.runtime->event_function36.frontend_profiles.active;
        if(lic.size()>0x60u)for(const char* p=u;*p;){char* e;const auto b=std::strtoul(p,&e,0);if(e==p)break;
            if(0x28u+b/8u<lic.size())lic[0x28u+b/8u]|=std::uint8_t(1u<<(b%8u));p=*e?e+1:e;}
    }
    // Silent refusals: the frontend/title/START owners record an unported screen
    // key or PC entry in their last_missing fields and stay where they are (the
    // menu freezes without a message). One stderr line per distinct value, so the
    // final check (tools/decomp_final_check.py, "not ported") counts them.
    if(s.runtime){
        const auto& r=*s.runtime;const auto& f=r.event_function36;
        struct Watch{const char* what;std::uint32_t value;};
        const Watch watches[]{
            {"frontend screen key",f.frontend_last_missing_key},{"frontend action",f.frontend_last_missing_action},
            {"frontend ui service",f.frontend_ui_missing_pc},{"title service",f.title_last_missing_service},
            // START: the scene owner's fault (its missing_service field also marks normal async polls)
            {"START service",r.start_mode.last_missing_service},{"START scene fault",r.start_mode.scene_owner_fault}};
        static std::set<std::pair<unsigned,std::uint32_t>> seen;
        for(unsigned i=0;i<std::size(watches);++i){const auto v=watches[i].value;
            if(v!=0u&&v!=~0u&&seen.insert({i,v}).second)
                std::fprintf(stderr,"[%u] [stall] not ported: %s %X (mode %u, the owner waits here)\n",s.input_samples,watches[i].what,v,r.mode_state.current);}
        // Flow: every game mode change, and a stall when nothing the scripts can
        // move (mode, frontend child/depth, title stage, START stages; not the race)
        // changed for OR2_HOST_STALL frames (default 5400; mode 24, the ending and
        // credits, gets 4x). A run that never returns to the frontend shows here.
        static std::uint32_t last_mode=~0u,same=0;static std::uint64_t last_sig=0;static bool stalled=false;
        if(r.mode_state.current!=last_mode){
            std::fprintf(stderr,"[%u] [flow] mode %u -> %u\n",s.input_samples,last_mode,r.mode_state.current);last_mode=r.mode_state.current;}
        const outrun::driving::Bytes owner(const_cast<std::uint8_t*>(f.object.data()),f.object.size());
        std::uint32_t title_stage{};std::memcpy(&title_stage,f.title_owner_object.data()+0x9ac,4);
        std::uint64_t sig=0xcbf29ce484222325ull;
        for(const std::uint64_t v:{std::uint64_t(r.mode_state.current),std::uint64_t(f.state2_selector_child_state),std::uint64_t(owner.u32(0x484)),
                                   std::uint64_t(title_stage),std::uint64_t(r.start_mode.stage),std::uint64_t(r.start_mode.scene_owner_stage),
                                   std::uint64_t(r.game_mode.active)}){sig^=v;sig*=0x100000001b3ull;}
        if(sig!=last_sig||r.mode_state.current==16u){last_sig=sig;same=0;stalled=false;}   // a race (mode 16) runs as long as it is driven
        else if(!stalled){
            static const std::uint32_t limit=[]{const char* e=std::getenv("OR2_HOST_STALL");return e?std::uint32_t(std::strtoul(e,nullptr,10)):5400u;}();
            if(++same>=(r.mode_state.current==24u?limit*4u:limit)){stalled=true;
                std::fprintf(stderr,"[%u] [stall] no progress for %u frames: mode %u child %u depth %u title %u START %u/%u race_frames %u key=%x/%x\n",
                    s.input_samples,same,unsigned(r.mode_state.current),unsigned(f.state2_selector_child_state),owner.u32(0x484),title_stage,unsigned(r.start_mode.stage),
                    unsigned(r.start_mode.scene_owner_stage),unsigned(r.race.frames_8367f4),f.frontend_last_missing_key,f.frontend_last_missing_action);}
        }
    }
    // Host test hook OR2_HOST_CAMERA=off:frame (off 94/98/9C): sets the camera word
    // [79F574]+off to 1 once the race has run `frame` frames (485FE0 controllers).
    if(s.runtime&&s.runtime->mode_state.current==16u)if(const char* g=std::getenv("OR2_HOST_CAMERA")){
        static bool done=false;const char* colon=std::strchr(g,':');
        const std::uint32_t off=std::uint32_t(std::strtoul(g,nullptr,16)),at=colon?std::uint32_t(std::strtoul(colon+1,nullptr,10)):60u;
        auto& cam=s.runtime->event_function36.car_select.camera_79fe10;
        if(!done&&s.runtime->race.frames_8367f4>=at&&off+4u<=cam.size()){done=true;cam[off]=1;
            std::fprintf(stderr,"[%u] host: camera +%X = 1\n",s.input_samples,off);}
    }
    // Host test hook OR2_HOST_MODE=mode:frame: requests a game mode once (boot logos,
    // demo route) to exercise mode callbacks no menu reaches.
    if(s.runtime)if(const char* g=std::getenv("OR2_HOST_MODE")){
        static bool done=false;const char* colon=std::strchr(g,':');
        const std::uint32_t want=std::uint32_t(std::strtoul(g,nullptr,10)),at=colon?std::uint32_t(std::strtoul(colon+1,nullptr,10)):300u;
        if(!done&&s.input_samples>=at){done=true;(void)pf::native_runtime_request_mode(*s.runtime,want);
            std::fprintf(stderr,"[%u] host: mode %u requested\n",s.input_samples,want);}
    }
    // Host test hook OR2_HOST_SKIP_ENDING=1: mode 24 (the goal ending, not ported) requests
    // mode 25 at once, to exercise the result / name entry modes after a goal.
    if(s.runtime&&s.runtime->mode_state.current==24u&&std::getenv("OR2_HOST_SKIP_ENDING")){
        static std::uint32_t skips=0;
        if(s.runtime->mode_state.requested!=25u&&pf::native_runtime_request_mode(*s.runtime,25u)&&++skips==1u)
            std::fprintf(stderr,"[%u] host: ending mode 24 skipped to 25\n",s.input_samples);
    }
    // host_nro trace: one line whenever the frontend/START/GAME position moves.
    if(s.runtime&&(std::getenv("OR2_HOST_TRACE")||frame_profile_live())){   // also over the live log
        const auto& r=*s.runtime;const auto& f=r.event_function36;
        const outrun::driving::Bytes owner(const_cast<std::uint8_t*>(f.object.data()),f.object.size());
        std::uint32_t title_stage{};std::memcpy(&title_stage,f.title_owner_object.data()+0x9ac,4);
        char line[360];std::snprintf(line,sizeof line,"mode=%u child=%u depth=%u welcome=%x start=%u/%u owner=%u missing=%x/%x/%u game=%d/%d ui_missing=%x key=%x/%x cl=%u/%u/%u title=%u/%x",
            r.mode_state.current,f.state2_selector_child_state,owner.u32(0x484),f.frontend_welcome_scene_token,unsigned(r.start_mode.active),
            r.start_mode.stage,r.start_mode.scene_owner_stage,r.start_mode.last_missing_service,r.start_mode.scene_owner_missing_service,r.start_mode.scene_owner_fault,
            int(r.game_mode.active),int(r.game_mode.gameplay_ready),f.frontend_ui_missing_pc,f.frontend_last_missing_key,f.frontend_last_missing_action,r.game_mode.course_load_calls,r.game_mode.course_load_success,r.game_mode.course_loader_phase,
            title_stage,f.title_last_missing_service);
        // Once in GAME: the active events (id/function/callbacks).
        static unsigned game_dump=0;
        if(std::getenv("OR2_HOST_RACERS")&&r.mode_state.current==16u&&game_dump==0u){static bool once=false;if(!once){once=true;
            const auto& rs=r.mission.racers;std::fprintf(stderr,"racers count=%u special=%u cfg=%x:",rs.count_80fb04,rs.special_80fb28,rs.config_80fb0c);
            for(std::uint32_t i=0;i<rs.count_80fb04;++i){const auto* p=rs.racers_80fb00.data()+std::size_t(i)*0xa0u;std::uint32_t a,b;std::memcpy(&a,p+0x54,4);std::memcpy(&b,p+0x5c,4);
                std::fprintf(stderr," [%u %x %x m%u]",i,a,b,p[0x72]);}
            std::fprintf(stderr,"\nmodels 8367C0:");for(unsigned k=0;k<44;++k)if(r.start_mode.scene_owner_models_8367c0[k])std::fprintf(stderr," %u",k);std::fprintf(stderr,"\n");}}
        if((r.mode_state.current==16u||r.mode_state.current==18u)&&(s.input_samples%30u)==0u){
            const auto& cw=r.race.car_world;const auto& car=r.event_function36.car_select.car_799d18;
            float px,py,pz,sp;std::memcpy(&px,car.data()+0x14,4);std::memcpy(&py,car.data()+0x18,4);std::memcpy(&pz,car.data()+0x1c,4);std::memcpy(&sp,car.data()+0x1c4,4);
            std::fprintf(stderr,"[%u] car pos=(%.3f,%.3f,%.3f) speed=%.4f gear=%u rpm=%.1f vol=%d/%d/%d timer=%d err=%x %s\n",s.input_samples,px,py,pz,sp,
                car[0x208]|(car[0x209]<<8),[&]{float v;std::memcpy(&v,car.data()+0x21c,4);return v;}(),cw.analog_7d6810.current[0],cw.analog_7d6810.current[1],cw.analog_7d6810.current[2],
                int(r.game_mode.start_countdown),cw.last_error_pc,cw.last_error.c_str());
            if(std::getenv("OR2_HOST_OWNERDEBUG")){
                const auto& o=r.event_function36.object;std::int8_t depth=std::int8_t(o[0x220]);
                std::uint32_t st,tok,cur;std::memcpy(&st,o.data()+0x218,4);std::memcpy(&tok,o.data()+0x21c,4);std::memcpy(&cur,o.data()+0x488,4);
                std::fprintf(stderr,"   owner state=%u token=%x depth=%d h221=%u h241=%u 518=%d cur488=%x feature=%x\n",st,tok,depth,
                    o[0x221+std::size_t(std::max<int>(depth,0))],o[0x241+std::size_t(std::max<int>(depth,0))],[&]{std::int32_t v;std::memcpy(&v,o.data()+0x518,4);return v;}(),cur,
                    r.event_function36.frontend_input.feature_mask);
                std::fprintf(stderr,"   sel_child=%u sel_result=%u resumes=%u last_missing_action=%x title_stage=%u\n",r.event_function36.state2_selector_child_state,r.event_function36.state2_selector_last_result,r.event_function36.pause_resumes,r.event_function36.frontend_last_missing_action,[&]{std::uint32_t v;std::memcpy(&v,r.event_function36.title_owner_object.data()+0x9ac,4);return v;}());}
            if(std::getenv("OR2_HOST_CARDEBUG")){
                const outrun::driving::Bytes e(const_cast<std::uint8_t*>(car.data()),car.size());
                const outrun::driving::Bytes w(const_cast<std::uint8_t*>(r.event_function36.car_select.body_82e7f0.data()),0x900);
                std::fprintf(stderr,"   e4=%08x e34=%d e38=%d e3c=%d e13=%u dbc=%g e20c=%d e210=%d e48=%d w0=%u w5c=(%g,%g,%g) e20=(%g,%g,%g) e178=%g\n",
                    e.u32(4),e.i32(0x34),e.i32(0x38),e.i32(0x3c),e.u8(0x13),e.f32(0xdbc),e.i32(0x20c),e.i32(0x210),e.i32(0x48),w.u32(0),
                    w.f32(0x5c),w.f32(0x60),w.f32(0x64),e.f32(0x20),e.f32(0x24),e.f32(0x28),e.f32(0x178));
                std::fprintf(stderr,"   e2e=%d e160=%d d4c=%d d4e=%d d50=%d place5c=%08x/%08x/%08x/%08x e68=%08x e27c=%d\n",e.i16(0x2e),e.i16(0x160),e.i16(0xd4c),e.i16(0xd4e),e.i16(0xd50),
                    e.u32(0x5c),e.u32(0x60),e.u32(0x64),e.u32(0x68),e.u32(0x68),e.i32(0x27c));
            }
        }
        if(pf::native_race_end_mode_active(r.mode_state.current)&&(s.input_samples%60u)==0u){
            const auto& o=r.race.camera_override;
            std::fprintf(stderr,"[%u] mode %u %s | override d8=%d scene=%u t=%.1f ec=%u | effects unplayed:",s.input_samples,r.mode_state.current,
                pf::native_race_end_status(r).c_str(),o.override_82e7d8,unsigned(o.scene_82e7d4),o.time_82e7e0,o.steering_82e7ec);
            for(const auto& [id,n]:s.race_effects_unplayed)std::fprintf(stderr," %x x%u",id,n);
            std::fprintf(stderr,"\n");
        }
        // ---- race AREA/SKY trace (OR2_HOST_AREA): begin ----
        if(r.mode_state.current==16u&&std::getenv("OR2_HOST_AREA")&&(s.input_samples%10u)==0u)
            std::fprintf(stderr,"[%u] %s\n",s.input_samples,pf::native_race_area_status(r).c_str());
        // ---- race AREA/SKY trace: end ----
        // ---- race manager trace (OR2_HOST_RACE): begin ----
        if(r.mode_state.current==16u&&std::getenv("OR2_HOST_RACE")&&(s.input_samples%10u)==0u){
            const auto& car=r.event_function36.car_select.car_799d18;float px,py,pz,sp;
            std::memcpy(&px,car.data()+0x14,4);std::memcpy(&py,car.data()+0x18,4);std::memcpy(&pz,car.data()+0x1c,4);std::memcpy(&sp,car.data()+0x1c4,4);
            std::uint32_t e5c,e68;std::int16_t e64;std::memcpy(&e5c,car.data()+0x5c,4);std::memcpy(&e64,car.data()+0x64,2);std::memcpy(&e68,car.data()+0x68,4);
            std::fprintf(stderr,"[%u] race variant=%u preset=%u cd=%d car=(%.3f,%.3f,%.3f) v=%.4f place=%x/%d key=%x 7D2E80=%x 7D2E88=%x | %s\n",s.input_samples,
                r.game_mode.game_variant,r.start_mode.course_preset,int(r.game_mode.start_countdown),px,py,pz,sp,e5c,int(e64),e68,
                r.race.area_state_7d2e80,r.race.area_state_7d2e88,pf::native_race_manager_status(r).c_str());
        }
        // ---- race manager trace: end ----
        // OR2_HOST_POOL=N: every N frames (1 = 100, the old meaning)
        if(const char* pe=std::getenv("OR2_HOST_POOL");pe&&(s.input_samples%std::max(1ul,std::strtoul(pe,nullptr,10)==1ul?100ul:std::strtoul(pe,nullptr,10)))==0u){      // SPRANI pool (event 398 / frontend)
            std::fprintf(stderr,"[%u] mode %u pool:",s.input_samples,r.mode_state.current);
            const auto& in=r.event_function36.frontend_sprites.instances();
            for(std::uint32_t h=0;h<in.size();++h)if(in[h].allocated)std::fprintf(stderr," %u:%08x%s m%u f%.0f",h,in[h].token,in[h].visible?"":"(hidden)",in[h].mode,in[h].frame);
            std::fprintf(stderr,"\n");
        }
        if(r.mode_state.current==16u&&std::getenv("OR2_HOST_SOUND")&&(s.input_samples%60u)==0u)
            std::fprintf(stderr,"[%u] %s\n",s.input_samples,pf::native_race_sound_status(r).c_str());
        if(std::getenv("OR2_HOST_DISPLAY")&&(s.input_samples%60u)==0u){      // events with a given display callback
            const auto want=std::uint32_t(std::strtoul(std::getenv("OR2_HOST_DISPLAY"),nullptr,16));
            for(std::uint32_t id=0;id<r.event_state.slots.size();++id){const auto& e=r.event_state.slots[id];
                if((e.flags&3u)&&e.disp_callback==want)std::fprintf(stderr,"[%u] mode %u event %u/%x f%x (%x,%x,%x,%x) work %x\n",s.input_samples,r.mode_state.current,id,e.function_id,
                    unsigned(e.flags),e.init_callback,e.ctrl_callback,e.disp_callback,e.dest_callback,e.work_token);}
        }
        if((((r.mode_state.current==16u||(r.mode_state.current==13u&&std::getenv("OR2_HOST_START_EVENTS")))&&game_dump<4u&&(s.input_samples%60u)==0u))||(r.mode_state.current==24u&&(s.input_samples%300u)==0u)||(std::getenv("OR2_HOST_ALL_EVENTS")&&std::strstr((std::string(",")+std::getenv("OR2_HOST_ALL_EVENTS")+",").c_str(),(","+std::to_string(s.input_samples)+",").c_str()))){
            ++game_dump;std::fprintf(stderr,"[%u] active events:",s.input_samples);
            for(std::uint32_t id=0;id<r.event_state.slots.size();++id){const auto& e=r.event_state.slots[id];
                if(e.flags&3u)std::fprintf(stderr," %u/%x f%x s%x(%x,%x,%x,%x)",id,e.function_id,unsigned(e.flags),e.display_scene,e.init_callback,e.ctrl_callback,e.disp_callback,e.dest_callback);}
            std::fprintf(stderr,"\n  unhandled callbacks:");
            for(const auto& [k,n]:s.unhandled_callbacks)std::fprintf(stderr," %u/%x x%u",unsigned(k>>32),unsigned(k),n);
            if(s.pc_scene){std::fprintf(stderr,"\n  unported displays:");
                for(const auto& [k,n]:s.pc_scene->unported_displays)std::fprintf(stderr," %x x%u",k,n);
                std::fprintf(stderr,"\n  unported leaves:");
                for(const auto& [k,n]:s.pc_scene->unported_leaves)std::fprintf(stderr," %x x%u",k,n);
                std::fprintf(stderr,"\n  preview car: displays=%u bank_failures=%u preview_inits=%u controls=%u select(46C090)=%u",s.pc_scene->car_displays,s.pc_scene->bank_failures,
                    r.event_function36.car_select.preview_inits,r.event_function36.car_select.preview_controls,s.pc_scene->select_car_draws());
                std::fprintf(stderr,"\n  race car: inits=%u displays=%u car_fault=%x error=%s",r.event_function36.car_select.race_inits,
                    s.pc_scene->race_car_displays,r.event_function36.car_select.fault,s.pc_scene->last_error.c_str());
                std::fprintf(stderr,"\n  car shadows: allocs=%u draws=%u failures=%u unserved=%u error=%s",s.pc_scene->shadow_allocs,s.pc_scene->shadow_draws,
                    s.pc_scene->shadow_failures,r.event_function36.car_select.shadow_unported,s.pc_scene->shadow_error.c_str());
                {const auto& cw=r.race.car_world;const auto& car=r.event_function36.car_select.car_799d18;
                 float px,py,pz,sp;std::memcpy(&px,car.data()+0x14,4);std::memcpy(&py,car.data()+0x18,4);std::memcpy(&pz,car.data()+0x1c,4);std::memcpy(&sp,car.data()+0x1c4,4);
                 std::fprintf(stderr,"\n  player car: frames=%u pos=(%.3f,%.3f,%.3f) speed=%.4f vol=%d/%d/%d error=%x %s missing:",cw.frames,px,py,pz,sp,
                     cw.analog_7d6810.current[0],cw.analog_7d6810.current[1],cw.analog_7d6810.current[2],cw.last_error_pc,cw.last_error.c_str());
                 for(const auto& [k,n]:cw.missing)std::fprintf(stderr," %x x%u",k,n);}
                std::fprintf(stderr,"\n  START resources:");
                for(std::uint32_t k=0;k<r.start_mode.scene_owner_resource_count&&k<r.start_mode.scene_owner_resource_ids.size();++k)
                    std::fprintf(stderr," %x/%u",r.start_mode.scene_owner_resource_ids[k],r.start_mode.scene_owner_resource_modes[k]);
                std::fprintf(stderr,"\n  camera: inits=%u controls=%u unanswered=%u env: inits=%u controls=%u unanswered=%u frames=%u last=%s",
                    s.pc_scene->camera_inits,s.pc_scene->camera_controls,s.pc_scene->camera_unanswered,s.pc_scene->environment_inits,
                    s.pc_scene->environment_controls,s.pc_scene->environment_unanswered,s.pc_scene->frames,s.pc_scene->last_error.c_str());
                {const auto& fx=r.race_effects;                                                     // scn-efc
                 std::fprintf(stderr,"\n  race effects: SCN_EFC inits=%u controls=%u destroys=%u fault=%x %s | PART_EFC inits=%u controls=%u displays=%u dropped=%u fault=%x %s",
                     fx.scene_inits,fx.scene_controls,fx.scene_destroys,fx.scene_fault_pc,fx.scene_fault.c_str(),fx.particle_inits,fx.particle_controls,
                     fx.particle_displays,fx.display_dropped,fx.particles_fault_pc,fx.particles_fault.c_str());}
                if(r.race_ghosts){const auto& g=*r.race_ghosts;   // Time Attack ghosts
                 std::fprintf(stderr,"\n  ghosts: init=%u load=%u files=%u save r/w=%u/%u cars open=%u init=%u ctl=%u destroy=%u disp=%u shadow=%u drawn=%u faults=%u fault=%x %s %s",
                     g.init_calls,g.load_calls,g.file_loads,g.save_reads,g.save_writes,g.cars_opened,g.car_inits,g.car_controls,g.car_destroys,
                     g.displays,g.shadows,g.objects_drawn,g.display_faults,g.fault,g.error.c_str(),g.display_error.c_str());}
                std::fprintf(stderr,"\n  %s",pf::native_race_end_status(r).c_str());
                {const auto& rb=r.race.robots;   // race robots ROB01/ROB03
                 std::fprintf(stderr,"\n  robots: inits=%u controls=%u displays=%u destroys=%u completed=%u skipped=%u faults=%x/%x/%x missing:",
                     rb.inits,rb.controls,rb.displays,rb.destroys,rb.completed,rb.skipped,rb.fault[0],rb.fault[1],rb.fault[2]);
                 std::fprintf(stderr,"\n  autoscene: calls=%u draws=%u skipped=%u fault=%x %s unplayed:",rb.autoscene_calls,rb.autoscene_draws,rb.autoscene_skipped,rb.autoscene_fault,rb.autoscene_error.c_str());
                 for(const auto& [k,n]:rb.autoscene_unplayed)std::fprintf(stderr," %x x%u",k,n);
                 for(const auto& [k,n]:rb.missing)std::fprintf(stderr," %x x%u",k,n);
                 std::fprintf(stderr," unmapped:");for(const auto& [k,n]:rb.unmapped)std::fprintf(stderr," %x x%u",k,n);
                 std::fprintf(stderr," %s",rb.last_error.c_str());for(unsigned q=0;q<8;++q)if(!rb.errors[q].empty())std::fprintf(stderr," [ROB%02u: %s]",q+1u,rb.errors[q].c_str());
                 const auto& db=r.object_db;std::fprintf(stderr,"\n  object db: builds=%u entries=%u lookups=%u misses=%u %s",db.builds,db.count_7d25e0,db.lookups,db.misses,db.error.c_str());
                 std::fprintf(stderr," | chr 488B80: loads=%u files=%u ready=%d fault=%x %s",rb.chr_loads,rb.chr_files,int(rb.chr_ready),rb.chr_fault,rb.chr_error.c_str());std::fprintf(stderr," | motion groups=%u %s",rb.motion_tables.groups_loaded,rb.motion_tables.error.c_str());}}
            std::fprintf(stderr,"\n");
        }
        static std::string last;
        if(last!=line||down){last=line;std::fprintf(stderr,"[%u] %s down=%llx %s | %s\n",s.input_samples,line,(unsigned long long)down,
            r.start_mode.scene_owner_fault_reason.c_str(),r.start_mode.scene_owner_collision_error.c_str());}
    }
}
#else
// Live log (nxlink / TCP): mode changes and, in a race, the car every 30 frames (position, speed,
// gear, steering input, the +4 flags and the +38/+3C/+20C/+210/+5C words) to follow
// collisions and slides from the PC.
void live_log(GameHost& s){
    if(!s.runtime||!frame_profile_live())return;
    const auto& r=*s.runtime;
    static std::uint32_t last_mode=0xffffffffu;
    if(r.mode_state.current!=last_mode){last_mode=r.mode_state.current;
        std::fprintf(stderr,"[%u] mode=%u variant=%u preset=%u\n",s.input_samples,r.mode_state.current,r.game_mode.game_variant,r.start_mode.course_preset);}
    if(r.mode_state.current==16u&&(s.input_samples%30u)==0u){
        const outrun::driving::Bytes e(const_cast<std::uint8_t*>(r.event_function36.car_select.car_799d18.data()),r.event_function36.car_select.car_799d18.size());
        std::fprintf(stderr,"[%u] car pos=(%.2f,%.2f,%.2f) speed=%.3f gear=%u steer=%d e4=%08x e38=%d e3c=%d e20c=%d e210=%d e5c=%08x e48=%d\n",s.input_samples,
            double(e.f32(0x14)),double(e.f32(0x18)),double(e.f32(0x1c)),double(e.f32(0x1c4)),unsigned(std::uint16_t(e.i16(0x208))),int(r.race.car_world.analog_7d6810.current[0]),
            e.u32(4),e.i32(0x38),e.i32(0x3c),e.i32(0x20c),e.i32(0x210),e.u32(0x5c),e.i32(0x48));
    }
}
#endif

void input_sample(void* user,pf::NativeInputState& out){
    auto& s=*static_cast<GameHost*>(user);
    if(s.runtime){const auto m=s.runtime->mode_state.current;
        crash_breadcrumbs().mode=m;
        frame_profile().frame_start(m==16u||pf::native_race_end_mode_active(m),m);}
    const PadSnapshot pad=input_replay().filter(s.read_pad?s.read_pad():PadSnapshot{});
    ensure_autoplay(s);
    auto& ap=autoplay();
    std::uint64_t held=pad.held,down=pad.down;
    if(ap.on){
        const unsigned f=++ap.frame;
        std::uint64_t h=ap.at(f),prev=f>1?ap.at(f-1):0;
        if(ap.frames&&f>=ap.frames){h=PadL|PadR|PadPlus;prev=f>ap.frames?h:0;}
        held|=h;down|=h&~prev;
        if(ap.trace&&s.runtime){const auto& r=*s.runtime;
            std::fprintf(ap.trace,"f=%u mode=%u child=%u start=%u/%u game=%d\n",f,r.mode_state.current,r.event_function36.state2_selector_child_state,
                unsigned(r.start_mode.active),r.start_mode.stage,int(r.game_mode.active));std::fflush(ap.trace);}
        if(f==ap.diag_frame&&s.presenter)for(unsigned k=0;k<ap.diag_mode;++k)s.presenter->cycle_pc_diagnostic();
    }
    out.connected=pad.connected;
    out.raw_buttons_held=held;
    out.raw_buttons_down=down;
    if(s.platform_keys)s.platform_keys(held,down);
    out.raw_buttons_up=pad.up;
    if(s.map_input)s.map_input(held,down,pad,out);
    else map_pad(s,out,held,down,pad.left,pad.right);
    // Every frame (the configuration screen polls the pad outside 453BB0 too).
    if(s.runtime&&out.pc_input_layer)pf::native_pc_input_set_pad(*s.runtime,out.pc_pad);
#if defined(OR2_HOST_HOOKS)
    host_hooks(s,down);
#else
    live_log(s);
#endif
    s.input=out;
    s.seen_buttons|=held|down;
    ++s.input_samples;
    if(s.sample_status)s.sample_status(out,held,down,pad.up);
}

void event_invoke(void* user,std::uint32_t callback,std::uint32_t work,std::uint32_t event_id){
    auto& s=*static_cast<GameHost*>(user);
    ++s.event_callbacks;s.last_event=event_id;s.last_callback=callback;
    if(autoplay().trace){char t[32];std::snprintf(t,sizeof t,"e%u:%x",unsigned(event_id),unsigned(callback));trace_mark(t);}
    ProfileScope prof(profile_key(ProfileEvent,event_id,callback));
    if(event_id==405u&&callback==0x49e490u)++s.event_405_init;
    if(event_id==405u&&callback==0x49e4a0u)++s.event_405_ctrl;
    if(event_id==406u&&callback==0x414940u)++s.event_406_init;
    if(event_id==406u&&callback==0x414980u)++s.event_406_ctrl;
    if(event_id==406u&&callback==0x414bb0u)++s.event_406_dest;
    if(s.runtime&&pf::native_mission_event_invoke(*s.runtime,callback))return;
    if(s.runtime&&pf::native_race_end_event_invoke(*s.runtime,callback))return;   // event 0x188 result screens
#if defined(OR2_HOST_HOOKS)
    try{
#endif
    if(s.pc_scene&&s.runtime){
        auto& scene=*s.pc_scene;
        const pf::NativeEnvironmentHook environment=[&scene](outrun::driving::Bytes v,outrun::driving::Bytes c,
            const std::function<void(outrun::driving::PcEnvironmentFrame&,const outrun::driving::CourseCollisionTables&,
                                     outrun::driving::PcEnvironmentBlendContext&)>& fn){scene.with_environment(v,c,fn);};
        // ---- race sound (events 383 SOUND / 360 COMM_TRANS): begin ----
        if(pf::native_sprite2d_event(*s.runtime,callback))++s.pc_scene_callbacks;   // event 398 in GAME
        else if(pf::native_race_hud_event_invoke(*s.runtime,callback,work,event_id,scene.matrices(),&scene))++s.pc_scene_callbacks; // HUD 389/388
        else if(pf::native_race_sound_invoke(*s.runtime,callback,&scene))++s.pc_scene_callbacks;
        else
        // ---- race sound: end ----
        // ---- race manager (event 359: 450790/4515B0/44FE10): begin ----
        if(pf::native_race_manager_invoke(*s.runtime,callback,scene.matrices(),&scene))++s.pc_scene_callbacks;
        else
        // ---- race manager: end ----
        // ---- race AREA/SKY (events 390/391): begin ----
        if(pf::native_race_area_event_invoke(*s.runtime,callback,work,event_id,scene.matrices(),&scene))++s.pc_scene_callbacks;
        else
        // ---- race AREA/SKY: end ----
        if(pf::native_car_event_invoke(*s.runtime,callback,environment,scene.matrices()))++s.pc_scene_callbacks;
        else if(scene.invoke(callback,0,event_id,s.runtime->mode_state.current,s.runtime->game_mode.game_variant))++s.pc_scene_callbacks;
        else if(pf::native_race_requests_event(*s.runtime,callback,work,scene.matrices()))++s.pc_scene_callbacks;   // event 0x190 (variant 5)
        else if(pf::native_bulk_event_invoke(*s.runtime,callback,work,scene.matrices()))++s.pc_scene_callbacks;   // translated original
#if defined(OR2_HOST_HOOKS)
        // Callbacks whose work the native platform does instead (not missing code): 398 sound loop
        // 427F70, 405 title owner 49E490/49E4A0 (445BE0), 406 Win32 message pump 414940/414980/414BB0.
        else if(!(callback==0x427f70u||callback==0x49e490u||callback==0x49e4a0u||callback==0x414940u||callback==0x414980u||callback==0x414bb0u))
            ++s.unhandled_callbacks[(std::uint64_t(event_id)<<32)|callback];
#endif
    }
#if defined(OR2_HOST_HOOKS)
    }catch(const std::exception& e){std::fprintf(stderr,"event %u callback %x threw: %s\n",event_id,callback,e.what());throw;}
#endif
}

void mode_invoke(void* user,std::uint32_t callback,std::uint32_t,pf::NativeModePhase phase){
    auto& s=*static_cast<GameHost*>(user);
    ++s.mode_callbacks;
    if(phase==pf::NativeModePhase::Init)++s.mode_init;
    else if(phase==pf::NativeModePhase::Control)++s.mode_ctrl;
    else ++s.mode_exit;
    s.last_callback=callback;
}

bool start_ready(void* user,std::uint32_t pc_entry){
    auto& s=*static_cast<GameHost*>(user);
    return s.runtime&&pf::native_start_owned_resource_ready(*s.runtime,pc_entry);
}

bool start_call(void* user,std::uint32_t pc_entry,const std::uint32_t* args,std::size_t count,std::uint32_t& result){
    auto& s=*static_cast<GameHost*>(user);
    return s.runtime&&pf::native_start_owned_call(*s.runtime,pc_entry,args,count,result);
}

bool input_continue(void* user,std::uint32_t){
    const auto& state=*static_cast<GameHost*>(user);
    // Plus is a frontend/START input. Only the advertised L+R+Plus chord
    // exits the native runtime.
    return !state.render_failed&&!state.input.exit_requested;
}
}

bool bind_runtime(GameHost& host,pf::NativeRuntimeContext& context,RuntimeBinding& b){
    host.runtime=&context;
#if defined(OR2_HOST_HOOKS)
    {auto& h=pf::dev_hooks();
#if defined(OR2_HOST_NRO)
     h.log_faults=true;
#endif
     h.fixed_clock=std::getenv("OR2_HOST_FIXEDCLOCK")!=nullptr;
     h.trace_glyphs=std::getenv("OR2_HOST_GLYPHS")!=nullptr;
     if(const char* v=std::getenv("OR2_HOST_VARIANT"))h.force_variant=std::uint32_t(std::strtoul(v,nullptr,0));
     if(std::getenv("OR2_HOST_LIVE"))frame_profile_live()=true;}  // live log reports (window profile, stutter log)
#endif
    if(!b.platform.ticks)b.platform=pf::make_system_runtime_platform();
    b.input=pf::NativeRuntimeInput{&host,input_sample};
    // Opaque nonzero system tokens keep the already-validated ownership paths
    // active without pretending that the renderer/audio/input objects exist yet.
    context.primary_system_token=0x1001u;
    context.secondary_system_token=0x1002u;
    context.platform_init_state.window_token=0x2001u;
    context.platform_init_state.resource_token_740ca0=0x2002u;
    context.loop_setup_state.object_95b218=0x3001u;
    context.loop_setup_state.mode_78026c=0x10u;
    auto& services=b.services;
    services.startup={&host,startup_call};
    services.platform_init={&host,startup_call};
    services.use_native_platform_init=true;
    services.use_native_frame_loop=true;
    services.loop.setup={&host,setup_call};
    services.loop.frame={&host,frame_call,nullptr,nullptr,nullptr};
    services.loop.cleanup={&host,cleanup_call};
    services.loop.user=&host;
    services.loop.continue_running=input_continue;
    services.loop.platform=&b.platform;
    services.loop.input=&b.input;
    services.loop.event={&host,event_invoke,nullptr};
    services.loop.mode={&host,mode_invoke,start_ready,start_call};
    services.loop.use_native_event_control=true;
    services.loop.use_native_mode_control=true;
    // Diagnostic transition through the real mode-table entry SUMO_FE (32).
    // Its init callback is the sole static xref that resets the event36 owner.
    return pf::native_runtime_request_mode(context,32u);
}

// ---------------------------------------------------------------- game data
bool load_game_data(pf::NativeRuntimeContext& context,pf::RetailAssetStore& retail,const std::string& retail_root,GameData& d,
                    const std::function<void(const std::string&)>& status,std::uint32_t& code,const char*& phase,std::string& error){
    const auto fail=[&](std::uint32_t c,const char* p){code=c;phase=p;return false;};
    d.metadata_path="OR2006C2C.EXE/event+mode tables";
    if(!pf::parse_event_metadata(pf::EmbeddedEventMetadata,pf::EmbeddedEventMetadataSize,context,&error))return fail(3u,"embedded-metadata");

    if(!pf::build_retail_start_loading(retail,d.start_loading,&error)||
       !pf::build_retail_font_pack(retail,d.fonts,&error)||
       !pf::load_retail_sprani(retail,"SUMO_FE",224u,d.sumo_fe_timeline,&error)||
       !pf::load_retail_sprani(retail,"ETC",319u,d.etc_timeline,&error)){
        status("Frontend data failed: "+error+"\n");
        return fail(4u,"frontend-data");
    }

    if(!pf::native_runtime_attach_retail_assets(context,retail)){
        error="retail loader core resources are incomplete";return fail(7u,"retail-loader");}
    d.loader_assets_path="retail:"+retail_root;

    {std::vector<std::uint8_t> csc,pack;
    if(!pf::retail_asset_read_relative(retail,"Scripts/bin/csc_data_cvt.bin",csc,pf::CourseCvtBlobBytes,&error)||
       !pf::build_course_asset_pack(csc,pack,&error)||
       !pf::parse_course_asset_pack(pack.data(),pack.size(),d.course_assets,&error)||
       d.course_assets.course_blob.size()!=pf::CourseCvtBlobBytes||
       !pf::native_runtime_attach_course_assets(context,d.course_assets))return fail(9u,"retail-course");}
    d.course_assets_path="OR2006C2C.EXE descriptors + "+pf::retail_asset_path(retail,"Scripts/bin/csc_data_cvt.bin");

    if(!pf::parse_stage17_asset_pack(pf::EmbeddedStage17Assets,pf::EmbeddedStage17AssetsSize,d.stage17_assets,&error)||
       !pf::native_runtime_attach_stage17_assets(context,d.stage17_assets))return fail(15u,"embedded-stage17");
    d.stage17_assets_path="OR2006C2C.EXE/stage17";

    d.race_assets_path=pf::retail_asset_path(retail,"Scripts/bin/Races.bin");
    d.race_assignment_path=pf::retail_asset_path(retail,"Scripts/bin/RaceAssignment.bin");
    if(!pf::load_race_asset_pack_file(d.race_assets_path.c_str(),d.race_assets,&error)||
       !pf::load_race_assignment_pack_file(d.race_assignment_path.c_str(),d.race_assignment,&error)||
       !pf::native_runtime_attach_race_assets(context,d.race_assets)||
       !pf::native_runtime_attach_race_assignment(context,d.race_assignment))return fail(16u,"retail-races");

    if(!pf::build_beac_world_source_from_retail(retail,d.course_assets,d.world_source,&error)||
       !pf::native_runtime_attach_world_source(context,d.world_source))return fail(17u,"retail-beac-world");
    d.world_source_path=retail_root+"/Stage/BEAC/*.sz";

    d.course_collision_path=pf::retail_asset_path(retail,"Stage/BEAC/coli_CS_BEAC_bin.sz");
    const auto* collision_bytes=pf::world_source_bytes(d.world_source,d.world_source.entries[0]);
    if(!collision_bytes||!pf::parse_pc_coli0200(collision_bytes,d.world_source.entries[0].size,d.course_collision,&error)||
       !d.course_world.admit_lane(0u,d.course_collision.pc_coli0200.data(),d.course_collision.pc_coli0200.size(),{},&error)||
       !d.course_world.set_transform(0u,pf::course_world_identity_transform(),&error))return fail(10u,"retail-course-collision");

    if(!pf::parse_driving_data_pack(pf::EmbeddedDrivingData,pf::EmbeddedDrivingDataSize,d.driving_data,&error)||
       !pf::native_runtime_attach_gameplay_assets(context,d.course_world,d.driving_data))return fail(12u,"embedded-driving");
    d.driving_data_path="OR2006C2C.EXE/driving tables";
    return true;
}

// ---------------------------------------------------------------- PC scene
void attach_pc_scene(GameHost& host,pf::NativeRuntimeContext& context,pf::RetailAssetStore& retail,pf::PcD3D9Device& device){
    host.pc_scene=std::make_unique<pf::PcSceneRenderer>(device);
    auto& scene=*host.pc_scene;auto& shared=context.event_function36.car_select;
    scene.bind_sun_lists(&shared.sun_lists_7d26a8);
    scene.bind_select_table(&shared.select_table_844a08);
    scene.clear_colour_89bd5c=&shared.clear_colour_89bd5c;
    scene.vehicle_camera=[&shared](outrun::driving::Bytes& v,outrun::driving::Bytes& c){
        v=outrun::driving::Bytes(shared.car_799d18.data(),shared.car_799d18.size());
        c=outrun::driving::Bytes(shared.camera_79fe10.data(),shared.camera_79fe10.size());return true;};
    scene.timer_49b2d0=[&context]{return std::uint16_t(context.game_mode.start_countdown);};
    // Car stencil shadows (422550 / 422740): on (host 2026-10-01: the darkening lands
    // under and left of the car, 2.7k pixels); OR2_SHADOW_OFF (host) turns them off.
    scene.shadow_draw_enabled=std::getenv("OR2_SHADOW_OFF")==nullptr&&pf::g_pc_car_shadows;
    shared.shadow_service=[&scene](std::uint32_t pc,outrun::driving::Bytes car){return pc==0x46ba20u?scene.shadow_alloc(car):scene.shadow_free(car);};
    ensure_autoplay(host);
    if(autoplay().trace)scene.trace=[](const char* stage,std::uint32_t v){auto& c=crash_breadcrumbs();c.stage=stage;c.value=v;char t[48];std::snprintf(t,sizeof t,"%s:%x",stage,unsigned(v));trace_mark(t);};
    else scene.trace=[](const char* stage,std::uint32_t v){auto& c=crash_breadcrumbs();c.stage=stage;c.value=v;};   // crash.txt breadcrumb
    context.camera_reset_4857c0=[&scene]{scene.camera_reset_4857c0();};   // pause Retry/Quit (4D7B80 / 4D6630)
    scene.race_camera_inputs=[&context,&scene](outrun::driving::PcRaceCameraInputs& in){
        in.timer_8367bc=std::uint16_t(context.game_mode.start_countdown);
        // 7C24B8 = active license +D8 (the license block lives at 7C23E0): the camera
        // view chosen in the options, 2 (chase) from the 4471A0 constructor.
        in.byte_7c24b8=context.event_function36.frontend_profiles.active[0xd8];
        in.network_7f9460_60=0u;        // offline: no LAN session object (46C480 shake 800AD0 stays 0)
        in.crt_random_580f40=&context.event_function36.pc_crt_random_state;
        // 49EED0 = [83DAF4] (attract step); 4B6F40's demo record needs the [7F1958] loader (45AE40), not ported: none.
        if(context.race_end)std::memcpy(&in.demo_round_83daf4,context.race_end->state.boot_83dae0.data()+0x14,4);
        auto& course=context.game_mode.course_runtime;
        in.area_matrix_7d2da0=outrun::driving::Bytes(course.matrix_7d2da0.data(),64);
        in.area_matrix_7d3190=outrun::driving::Bytes(course.matrix_7d3190.data(),64);
        in.steering_82e7ec=context.race.camera_override.steering_82e7ec;   // 487310: event 7 (4874A0 = the 486EF0 result)
        in.feature_mask_7d6778=context.event_function36.frontend_input.feature_mask;
        in.byte_780270=context.event_function36.title_game_state_780270;
        in.ground_43eb60=[&context,&scene](outrun::driving::CourseProbe& p){
            const auto tables=context.start_mode.scene_owner_course_world.tables();
            outrun::driving::CourseWorldQuery q{tables,scene.matrices(),context.event_function36.car_select.race_prediction};
            outrun::driving::get_y_position_prog(q,0x100u,p);};
        return true;};
    scene.robot_display=[&context,&scene](std::uint32_t callback,std::uint32_t work){ProfileScope prof(profile_key(ProfileDisplay,0,callback));   // race robots ROB01/ROB03
        return pf::native_race_robots_display(context,callback,work,scene.matrices());};
    scene.ghost_display=[&context,&scene](std::uint32_t callback,std::uint32_t work){ProfileScope prof(profile_key(ProfileDisplay,0,callback));   // Time Attack ghost cars
        return pf::native_ghost_car_display(context,scene,callback,work)||pf::native_race_objects_display(context,scene,callback,work);};   // + course objects (OSO)
    context.race.robots.renderer=&scene;
    scene.autoscene_work_799ca0=context.race.robots.autoscene_work.data();   // 4493E0 / 4B5FD0
    scene.body_view=[&shared](outrun::driving::Bytes& b){
        b=outrun::driving::Bytes(shared.body_82e7f0.data(),shared.body_82e7f0.size());return true;};
    // ---- race AREA/SKY display (44F120 / 4521C0): begin ----
    scene.race_area_display=[&context,&scene](std::uint32_t callback,std::uint32_t work,std::uint32_t event){ProfileScope prof(profile_key(ProfileDisplay,event,callback));
        return pf::native_race_area_display(context,scene,callback,work,event)||pf::native_race_end_scene_display(context,scene,callback);};
    // ---- race AREA/SKY display: end ----
    // ---- race manager display 44FE00 (event 359): begin ----
    scene.race_manager_display=[&context](std::uint32_t callback){ProfileScope prof(profile_key(ProfileDisplay,0,callback));return pf::native_race_manager_display(context,callback);};
    // ---- race manager display: end ----
    // PC 2D sprite renderer: pool display 428170 and the 42D710 flush leaf;
    // the platform's frontend then keeps only its glyph, image and icon layers.
    scene.sprite2d_display=[&context,&scene](std::uint32_t cb){ProfileScope prof(profile_key(ProfileDisplay,0,cb));
        return pf::native_sprite2d_display(context,scene,cb)||pf::native_race_hud_display(context,scene,cb,0u,0u);};
    // Frame leaves: 42D710 (2D flush) and 414340 (the car reflection cube; autoplay
    // nocube=1 / OR2_NOCUBE leave it unported, as before its port).
    pf::native_car_reflection_init(scene);
    // 416AA0 (headlight pool): resource bank 3 texture 9, loaded on first use like the 2D banks.
    scene.headlight_texture=[&context,&scene]()->std::uint32_t{
        pf::native_sprite2d_ensure_bank(context,scene.flush_context().device,3u);
        const auto& b=pf::native_sprite2d(context).state.banks[3];
        return b.state==2u&&b.textures.size()>9u?b.textures[9]:0u;};
    scene.leaf_hook=[&context,&scene,&host](std::uint32_t pc,std::uint32_t a){
        if(pc==0x42d710u&&host.presenter)host.presenter->scene_to_frame();   // 2D layer at full resolution
        if(pc==0x4bfa20u||pc==0x49a650u)return pf::native_race_end_layer_invoke(context,pc);   // 448FD0 layer callbacks
        if(pf::native_course_pass_leaf(context,scene,pc,a))return true;                // 40BD80 / 40BE70 course passes
        if(pc==0x4c50a0u){
            // 4C50A0 (frontend, layer 8): while the transition byte 84A9FD is set, the
            // 2D layers below [68A670] are flushed before the 3D car preview draws.
            const auto& t=context.event_function36.loader_stage12_state.transition_globals;
            if(t.active)(void)pf::native_sprite2d_leaf(context,scene,0x42d710u,t.token);
            return true;}
        if(pc==0x414340u){
            if(cube_disabled())return false;
            ProfileScope prof(profile_key(ProfileDisplay,0,pc));
            return pf::native_car_reflection_leaf(context,scene);}
        return pf::native_sprite2d_leaf(context,scene,pc,a);};
    scene.layer_callback=[&context](std::uint32_t layer){return pf::native_race_end_layer_callback(context,layer);};
    scene.mode_84a318=[&context]{return pf::native_race_end_84a318(context);};
    scene.course_effects=[&context](std::uint32_t& work,std::uint32_t& flags){
        work=pf::native_course_pass_work(context);flags=pf::native_course_pass_flags(context);};
    scene.profile=[](std::uint32_t kind,std::uint32_t pc,std::uint64_t ns){
        frame_profile().add(profile_key(ProfileDisplay,kind?0xffffu:0xfffeu,pc),ns*OR2_PROFILE_FREQ()/1000000000u);};
    // scn-efc: SCN_EFC/PART_EFC read the environment light table; PART_EFC displays through the renderer.
    context.race_effects.lights_899b98=scene.environment().lights_899b98.data();
    context.race_effects.glow_8a8c18=reinterpret_cast<std::uint8_t*>(&scene.globals().w(0x8a8c18));  // 4160F0 glow record (renderer-owned)
    scene.external_display=[&context,&scene](std::uint32_t cb){return pf::native_race_effects_display(context,cb,scene);};
    // Race sound (event 383) audio layer: 95B248 = the audio device is open;
    // 42F0D0 -> the native sample banks (a race SE outside MENU.pak is
    // refused and counted, not a failure); 427630 stops the effect voices.
    context.race.sound.audio_ready=[&host]{return host.audio_open;};
    context.race.sound.effect=[&host,&context](std::uint32_t id){
        if(!pump_audio(host)){host.render_failed=true;return false;}
        context.pc_sound.effect_42f0d0(id);return context.pc_sound.mapped(id);};
    context.race.sound.stop_all=[&host]{host.menu_audio.stop_effects();};
    context.race_effects.bank=[&scene,&retail](std::uint32_t id)->pf::PcPmtResources*{
        if(!scene.bank_loaded(id)){                                    // the START loader requests 0x57 in mode 8
            const auto* record=pf::retail_asset_lookup(retail,id,8u);std::vector<std::uint8_t> pmt;std::string e;
            if(!record||!pf::retail_asset_inflate_sz(record->bytes,pmt)||!scene.load_bank(id,std::move(pmt),e))return nullptr;}
        return scene.bank_resources(id);};
    scene.bank_source=[&retail](std::uint32_t id,std::vector<std::uint8_t>& pmt){
        const auto* record=pf::retail_asset_lookup(retail,id,id==0xbbu||id==0xbau?2u:11u);
        return record&&pf::retail_asset_inflate_sz(record->bytes,pmt);};
}

// ---------------------------------------------------------------- frontend
bool bind_frontend_timing(pf::FrontendSprites& sprites,const GameData& d){
    return sprites.bind(0x44u,d.sumo_fe_timeline)&&sprites.bind(0x2cu,d.etc_timeline);
}
bool open_frontend(GameHost& host,pf::NativeRuntimeContext& context,pf::RetailAssetStore& retail,const pf::FrontendFontPack* fonts,
                   FrontendSession& fe,const char*& phase,std::string& error){
    // English-US is the current startup default; binding the original language
    // option remains necessary. Use the retail table, not baked menu names.
    const auto text_path=pf::retail_asset_path(retail,"Text/English_US.bin");
    if(!fonts||!fe.text.load(text_path.c_str(),&error)){
        if(error.empty())error="Missing frontend fonts";
        phase="frontend-text";return false;
    }
    auto& state=context.event_function36;
    state.frontend_fonts=fonts;state.frontend_text=&fe.text;
    fe.title_widgets=std::make_unique<pf::FrontendTitleWidgets>(state.title_owner_object.data(),state.title_owner_object.size(),
        outrun::driving::Bytes(state.object.data(),state.object.size()),state.frontend_sprites,
        *fonts,fe.text,state.frontend_input,state.title_base_global_6591e4);
    state.title_widgets=fe.title_widgets.get();
    fe.license_owners=std::make_unique<pf::FrontendLicenseOwners>(context,*fonts,fe.text);
    state.license_owners=fe.license_owners.get();
    state.frontend_save_directory=host.save_directory.empty()?home()+"/SaveGame":host.save_directory;
    (void)pf::make_directory(state.frontend_save_directory);
    fe.license_owners->persistence(state.frontend_save_directory);
    if(!host.menu_audio.load_file(pf::retail_asset_path(retail,"Sound/MENU.pak"),error)||
       !host.menu_audio.load_frontend_file(pf::retail_asset_path(retail,"Sound/FE.PAK"),error)||
       !host.audio_output||!host.audio_output->open(error)){
        state.title_widgets=nullptr;state.license_owners=nullptr;
        phase="menu-audio";return false;
    }
    // Bootstrap may read large retail assets before the first serviced frame.
    // Start the PCM timeline on that frame, not before those startup reads.
    host.audio_open=true;host.audio_clock_started=false;host.audio_dropped=0;audio_thread().start(host);
    // PC sound driver (95B248 set by 42EBF0, slots emptied by 424600).
    context.pc_sound.read_file=[&host](const std::string& name,std::vector<std::uint8_t>& bytes,std::string& e){return read_sound_file(host,name.c_str(),bytes,e);};
    context.pc_sound.ready=true;context.pc_sound.boot_424600();host.menu_audio.bind_driver(&context.pc_sound);
    fe.title_widgets->effect(&host,menu_effect);fe.title_widgets->volume(&host,menu_volume);fe.license_owners->platform(&host,menu_platform);
    state.frontend_effect_user=&host;state.frontend_effect=menu_effect;
    host.retail=&retail;state.music_user=&host;state.music_play=menu_music_play;state.music_stop=menu_music_stop;
    // OutRun2SP button: 4165C0 rankings.dat / 4F3CC0 ranking tables (race_end_runtime).
    state.frontend_choice_service_user=&context;
    state.frontend_choice_service=[](void* u,unsigned pc){
        return pf::native_sp_rankings_service(*static_cast<pf::NativeRuntimeContext*>(u),pc);};
    host.movie_path=pf::retail_asset_path(retail,"mv/TitleScreen.bik");
    return true;
}

void close_frontend(GameHost& host,pf::NativeRuntimeContext& context,FrontendSession& fe){
    auto& state=context.event_function36;
    if(fe.title_widgets)fe.title_widgets->reset();state.title_widgets=nullptr;
    if(fe.license_owners)fe.license_owners->reset();state.license_owners=nullptr;
    state.frontend_effect=nullptr;state.frontend_effect_user=nullptr;
    state.music_play=nullptr;state.music_stop=nullptr;state.music_user=nullptr;host.menu_audio.stop_music();
    state.frontend_choice_service=nullptr;state.frontend_choice_service_user=nullptr;
    host.menu_audio.bind_driver(nullptr);context.pc_sound.ready=false;
    audio_thread().stop();audio_main_release();
    input_replay().close();
    if(host.audio_output)host.audio_output->close();
    host.audio_open=false;host.title_movie.close();
}

std::string pc_scene_summary(const GameHost& host){
    std::string summary;
    if(!host.pc_scene)return summary;
    const auto& p=*host.pc_scene;
    summary="init_gaps="+std::to_string(p.renderer_init_gaps)+" frames="+std::to_string(p.frames)+" env_init="+std::to_string(p.environment_inits)+
        " env_control="+std::to_string(p.environment_controls)+" env_unanswered="+std::to_string(p.environment_unanswered)+
        " alpha_flushes="+std::to_string(p.alpha_flushes)+" headlight_draws="+std::to_string(p.headlight_draws())+" texture_swaps="+std::to_string(p.texture_swaps)+"/deferred "+std::to_string(p.texture_swaps_deferred)+"/pending "+std::to_string(p.pending_swaps_406630.size())+"/failed "+std::to_string(p.texture_swap_failures)+" unported_displays=";
    for(const auto& [cb,n]:p.unported_displays){char b[32];std::snprintf(b,sizeof b,"%x:%u,",cb,n);summary+=b;}
    summary+=" unported_leaves=";
    for(const auto& [pc,n]:p.unported_leaves){char b[32];std::snprintf(b,sizeof b,"%x:%u,",pc,n);summary+=b;}
    if(!p.last_error.empty())summary+=" last="+p.last_error;
    return summary;
}

// ---------------------------------------------------------------- result log
std::FILE* open_result_log(const char*& used_path,int& last_error){
    static std::string path;
    path=home()+"/"+product().log_file;
    used_path=nullptr;last_error=0;errno=0;
    if(std::FILE* log=std::fopen(path.c_str(),"wb")){used_path=path.c_str();return log;}
    last_error=errno;
    return nullptr;
}

void result_printf(std::FILE* log,const char* format,...){
    std::va_list console_args,file_args;
    va_start(console_args,format);
    va_copy(file_args,console_args);
    std::vprintf(format,console_args);
    if(log)std::vfprintf(log,format,file_args);
    va_end(file_args);
    va_end(console_args);
}

void write_failure_log(std::uint32_t code,const char* phase,const char* detail){
    const char* used_path=nullptr;int last_error=0;
    std::FILE* log=open_result_log(used_path,last_error);
    if(!log)return;
    std::fprintf(log,"%s - authored frontend/title ownership\n\n",product().title.c_str());
    std::fprintf(log,"verdict=FAIL\nphase=%s\ncode=%u\n",phase?phase:"unknown",code);
    std::fprintf(log,"log_path=%s\n",used_path);
    if(detail&&detail[0]!='\0')std::fprintf(log,"detail=%s\n",detail);
    std::fflush(log);
    std::fclose(log);
}
}
