#pragma once
// The game host shared by every platform: what sits between the ported
// runtime (platform/native_runtime.hpp) and a platform's main. It loads the
// game data, wires the PC scene renderer and the frontend, mixes the audio,
// turns the pad into the game's inputs, runs the frame / event / mode
// callbacks of the runtime and keeps the development traces and autoplay.
// A platform supplies the pad, the audio output and the presenter
// (runtime/host_platform.hpp), its own startup, options and result log.
#include "runtime/host_platform.hpp"
#include "runtime/frame_profile.hpp"
#include "platform/native_runtime.hpp"
#include "platform/runtime_platform.hpp"
#include "platform/pc_scene_renderer.hpp"
#include "system/title_movie.hpp"
#include "platform/menu_audio.hpp"
#include "platform/retail_asset_store.hpp"
#include "platform/retail_gpu_cache.hpp"
#include "platform/game_ui_pack.hpp"
#include "platform/course_asset_pack.hpp"
#include "platform/course_collision_pack.hpp"
#include "platform/course_world_runtime.hpp"
#include "platform/driving_data_pack.hpp"
#include "platform/world_source_pack.hpp"
#include "platform/frontend_text.hpp"
#include "platform/frontend_title_widgets.hpp"
#include "platform/frontend_license_owners.hpp"
#include <atomic>
#include <chrono>
#include <cstdio>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>
namespace outrun::runtime {
// The launch folder: every file the game reads or writes (original data,
// GPU cache, SaveGame, logs, autoplay) lives there, nothing else is searched
// (like the PC executable in its installation folder).
std::string& home();
void set_home(int argc,char** argv);          // argv[0]'s folder, or "home=..." in the arguments
// The product names in the result log and its file name.
struct ProductNames { std::string title{"OutRunSwitch r155"},log_file{"OutRunSwitch.log"}; };
ProductNames& product();
// Breadcrumbs of the last scene stage and mode, for crash reports.
struct CrashBreadcrumbs { const char* stage{}; std::uint32_t value{},mode{}; };
CrashBreadcrumbs& crash_breadcrumbs();
// Development autoplay (emulator / unattended runs): autoplay.txt in the
// launch folder, lines "script=F:BTN,F-G:BTN+BTN", "frames=N" (then
// L+R+Plus), "diag=F:MODE" (PC shader diagnostic mode from frame F),
// "nospec=1" (uber shaders only), "nocube=1", "trace=1" (one flushed line
// per frame in trace.txt: crash breadcrumbs). Absent file: no effect.
struct Autoplay {
    bool loaded{},on{};std::string script;unsigned frames{},frame{},diag_frame{0xffffffffu},diag_mode{};
    bool no_specialised_shaders{};
    std::FILE* trace{};
    static std::uint64_t buttons(const std::string& names);   // PadButton bits
    std::uint64_t at(unsigned frame)const;
    void load();
};
Autoplay& autoplay();
// Input recording / replay: replay.txt in the launch folder, "record" or
// "play" on its first line (an optional second line names the file,
// default replay.bin). Record writes the pad of every frame; play feeds the
// recorded pads instead of the real one until the file ends, then the real
// pad takes over. Absent file: no effect.
struct InputReplay {
    enum class Mode { Off, Record, Play } mode{Mode::Off};
    bool loaded{};
    std::FILE* file{};
    std::uint64_t frames{};
    void load();
    PadSnapshot filter(const PadSnapshot& live);   // the pad the game sees this frame
    void close();
};
InputReplay& input_replay();
void trace_mark(const char* tag);
// The car reflection cube (414340) off: OR2_NOCUBE, autoplay nocube=1, a platform option.
bool& cube_disabled();

struct GameHost {
    // Counters of the runtime callbacks (result log).
    std::uint32_t calls{},cleanup_calls{},input_samples{},event_callbacks{};
    std::uint32_t event_405_init{},event_405_ctrl{},event_406_init{},event_406_ctrl{},event_406_dest{};
    std::uint32_t mode_callbacks{},mode_init{},mode_ctrl{},mode_exit{},last_event{},last_callback{};
    std::uint32_t next_token{0x90000000u};
    std::uint64_t seen_buttons{};
    platform::NativeInputState input{};
    std::uint32_t frontend_view_toggles{};
    bool frontend_visible{},frontend_gate_open{};
    // Title movie, menu sounds and music, mixed into the platform's output.
    platform::TitleMovie title_movie;
    AudioOutput* audio_output{};
    platform::MenuAudio menu_audio;
    std::vector<std::int16_t> mixed_pcm;
    std::chrono::steady_clock::time_point audio_clock{};
    bool audio_open{},audio_clock_started{};
    std::uint64_t audio_dropped{},audio_drops{};   // wall-clock frames not mixed after a stall (latency cap)
    std::vector<std::int16_t> movie_pcm;
    std::uint32_t movie_generation{};
    std::chrono::steady_clock::time_point movie_clock{};
    // A platform clock for the title movie instead of steady_clock (macOS:
    // paused while the window is hidden): reset at a new movie, then the
    // elapsed seconds; `paused` drops the queued movie audio.
    std::function<void()> movie_clock_reset;
    std::function<double(bool& paused)> movie_clock_read;
    std::string movie_path,movie_error;
    std::uint64_t movie_decoded_total{};
    std::uint64_t movie_uploaded_frame{~0ull};
    platform::NativeRuntimeContext* runtime{};
    Presenter* presenter{};
    // Ported PC renderer (SCN_ENV + frame renderer 449050) on the platform's
    // D3D9 device; absent when the device could not be created.
    std::unique_ptr<platform::PcSceneRenderer> pc_scene;
    std::uint32_t pc_scene_callbacks{};
    std::map<std::uint64_t,std::uint32_t> unhandled_callbacks;     // (event<<32)|callback -> count (host traces)
    std::map<std::uint32_t,std::uint32_t> race_effects_unplayed;   // 4249F0 ids outside MENU.pak (race SE banks not ported)
    // 401000/401030/401050 streamed BGM (Sound/<771AD8 track>.ogg).
    platform::RetailAssetStore* retail{};
    std::string music_error;
    std::uint32_t music_starts{},music_failures{},music_stops{};
    unsigned music_track{~0u};
    std::uint64_t game_frames{};   // frames serviced (42F330 calls)
    bool render_failed{};
    // Platform state the game follows.
    bool widescreen{};             // 16:9 PC screen (Hor+), else 640x480
    bool pc_input_layer{true};     // race controls through the PC DirectInput layer (406FA0)
    std::string save_directory;    // empty: <home>/SaveGame
    // ---- platform hooks of the input sample, in call order ----
    // The pad for this frame (per-frame platform settings applied first).
    std::function<PadSnapshot()> read_pad;
    // Platform keys, with the autoplay buttons merged (overlay, video settings).
    std::function<void(std::uint64_t held,std::uint64_t down)> platform_keys;
    // The game inputs from the merged buttons, instead of the shared pad
    // mapping (the PS5/macOS DualSense adapter).
    std::function<void(std::uint64_t held,std::uint64_t down,const PadSnapshot&,platform::NativeInputState& out)> map_input;
    // After the sample (the Switch's text console).
    std::function<void(const platform::NativeInputState&,std::uint64_t held,std::uint64_t down,std::uint64_t up)> sample_status;
    // Run on the mixer thread when it starts (core affinity).
    std::function<void()> audio_thread_setup;
    // Starts the mixer thread on a platform thread; returns the join (std::thread when unset).
    std::function<std::function<void()>(std::function<void()>)> spawn_audio_thread;
};
// Loads autoplay.txt once (applies nospec=1 to the presenter).
void ensure_autoplay(GameHost&);

// ---- audio ----
// Audio mixing on its own thread (port, not original code). The game state
// the mixer reads (PcSound voices, MenuAudio, music, movie PCM) is guarded
// by audio_mutex(): the main thread holds it while the game logic runs and
// releases it during the frame renderer (449050), where the mixer thread
// mixes; the few sound calls made from the renderer take it themselves.
std::recursive_mutex& audio_mutex();
void audio_main_acquire();
void audio_main_release();
bool pump_audio(GameHost&);
struct AudioThread {
    std::thread thread;std::function<void()> join;std::atomic<bool> run{false},failed{false};std::atomic<std::uint64_t> pumps{};
    void start(GameHost&);
    void stop();
};
AudioThread& audio_thread();
bool read_sound_file(GameHost&,const char* name,std::vector<std::uint8_t>& bytes,std::string& error);

// ---- game data ----
// Everything run_native_runtime needs from the retail tree and the
// embedded EXE tables, attached to the context (which keeps pointers into
// it: GameData outlives the run).
struct GameData {
    std::vector<std::uint8_t> start_loading;   // START loading picture (frontend_preview_pack)
    platform::FrontendFontPack fonts{};   // metrics of the frontend text
    platform::GameUiPack sumo_fe_timeline{},etc_timeline{};   // sprite timing of the frontend banks 44 and 2C
    platform::CourseAssetPack course_assets{};
    platform::Stage17AssetPack stage17_assets{};
    platform::RaceAssetPack race_assets{};
    platform::RaceAssignmentPack race_assignment{};
    platform::WorldSourcePack world_source{};
    platform::CourseCollisionPack course_collision{};
    platform::CourseWorldRuntime course_world{};
    platform::DrivingDataPack driving_data{};
    std::string metadata_path,loader_assets_path,course_assets_path,stage17_assets_path,driving_data_path;
    std::string race_assets_path,race_assignment_path,world_source_path,course_collision_path;
};
// On failure: false, the result-log code and phase, and the error. `status`
// receives the console lines (GPU cache progress).
bool load_game_data(platform::NativeRuntimeContext&,platform::RetailAssetStore&,const std::string& retail_root,GameData&,
                    const std::function<void(const std::string&)>& status,std::uint32_t& code,const char*& phase,std::string& error);

// ---- runtime binding ----
struct RuntimeBinding {
    platform::NativeRuntimePlatform platform;
    platform::NativeRuntimeInput input{};
    platform::NativeRuntimeServices services{};
};
// The host's runtime callbacks (startup, frame, events, modes, input), the
// system tokens, then the SUMO_FE (32) mode request. The platform record is
// the system one unless the platform set its own (ticks) before. False: the request failed.
bool bind_runtime(GameHost&,platform::NativeRuntimeContext&,RuntimeBinding&);
// The PC scene renderer on the platform's D3D9 device, bound to the game state.
void attach_pc_scene(GameHost&,platform::NativeRuntimeContext&,platform::RetailAssetStore&,platform::PcD3D9Device&);
// Frontend text, title widgets, licence owners, saves, menu audio and the
// audio output (started). False: the failing phase ("frontend-text",
// "menu-audio") and the error.
struct FrontendSession {
    platform::FrontendTextTable text;
    std::unique_ptr<platform::FrontendTitleWidgets> title_widgets;
    std::unique_ptr<platform::FrontendLicenseOwners> license_owners;
};
// Binds the authored timing of the SUMO_FE (44) and ETC (2C) banks to the sprite pool.
bool bind_frontend_timing(platform::FrontendSprites&,const GameData&);
bool open_frontend(GameHost&,platform::NativeRuntimeContext&,platform::RetailAssetStore&,const platform::FrontendFontPack* fonts,
                   FrontendSession&,const char*& phase,std::string& error);
// After the run: services unbound, audio stopped (the licence owners stay
// readable for the result log).
void close_frontend(GameHost&,platform::NativeRuntimeContext&,FrontendSession&);
// One-line summary of the PC scene renderer for the result log.
std::string pc_scene_summary(const GameHost&);

// ---- result log ----
std::FILE* open_result_log(const char*& used_path,int& last_error);
void result_printf(std::FILE* log,const char* format,...);
void write_failure_log(std::uint32_t code,const char* phase,const char* detail=nullptr);
}
