#pragma once
// Owner of the ported PC renderer state for the native runtime: renderer
// globals, render queues, the 89B564 matrix stack, the SCN_ENV environment,
// the frame targets and the scene display globals. It executes the event
// callbacks it owns (SCN_ENV init 449FC0 / control 44A890) and runs the
// frame renderer 449050 over the runtime's event state, dispatching the
// display callbacks that are ported:
//   405830 EXEC_DRAW alpha flush, 49A650 (RET), 49F500 -> 46C140 (when the
//   shared car, the body work and the bank of the car are available).
// It also owns the CAMERA event (385, function 0x0D: 484EE0 init, 485FE0
// control, whose device stores go through 410F90 and set the queue view).
// Other display callbacks are counted in unported_displays and skipped, so a
// frame never pretends to draw something that is not ported.
#include "platform/pc_frame_render.hpp"
#include "platform/pc_scene_environment.hpp"
#include "platform/pc_vehicle_display.hpp"
#include "driving/pc_camera.hpp"
#include "driving/pc_race_camera.hpp"
#include "platform/frontend_vehicle_data.hpp"
#include <map>
#include <memory>
#include <functional>
#include <string>
namespace outrun::platform {
class PcRaceMemory;
class PcSceneRenderer {
public:
    explicit PcSceneRenderer(PcD3D9Device& device);
    ~PcSceneRenderer();
    // Event callback owned here (returns false when the token is not).
    bool invoke(std::uint32_t callback,std::uint32_t work,std::uint32_t event_id,std::uint32_t mode_78026c,
                std::uint32_t race_mode_780258);
    // 4857C0 (pause Retry/Quit): views 0/1 move to the scripted view 0x1A
    // (4833E0, 485590 over 818FE8 entry 0x1A in 30 frames, 484DF0, 484BD0).
    void camera_reset_4857c0();
    // One frame (449050) over the runtime event state.
    void render_frame(driving::PcEventControlState& events,std::uint32_t mode_78026c);
    PcD3D9Device& device(){return device_;}
    PcRenderGlobals& globals(){return globals_;}
    PcSceneEnvironment& environment(){return environment_;}
    PcFrameState& frame(){return frame_;}
    std::map<std::uint32_t,std::uint32_t> unported_displays;  // callback -> count
    std::map<std::uint32_t,std::uint32_t> unported_leaves;    // pc -> count
    std::uint32_t frames{},environment_inits{},environment_controls{},alpha_flushes{};
    // Device capabilities 404250 could not use (RendererInitNoCube / NoShadowTexture).
    std::uint32_t renderer_init_gaps{};
    // 44A890 gates that need a protected query the runtime does not answer
    // yet: the frame's environment control is skipped and counted here.
    std::uint32_t environment_unanswered{};
    std::string last_error;
    // Shared car 799D18 and camera 79F574 views for the 44A8DF update
    // (false when the runtime has none).
    std::function<bool(driving::Bytes& vehicle,driving::Bytes& camera)> vehicle_camera;
    // 49B2D0 (protected): the start countdown word 8367BC (44A890 table-2 gate).
    std::function<std::uint16_t()> timer_49b2d0;
    // Race inputs of the CAMERA control in game mode 16 (timer 8367BC, 780258,
    // 82E7EC, 7D6778, 780270, 43EB60); car and live tables are filled here.
    std::function<bool(driving::PcRaceCameraInputs&)> race_camera_inputs;
    // Body work 82E7F0 of the shared car (46AE70 wheel angles).
    std::function<bool(driving::Bytes& body)> body_view;
    // Race robot displays 4879B0/4879D0 (race_robots_runtime.hpp): false when
    // the callback is not one of them.
    std::function<bool(std::uint32_t callback,std::uint32_t work)> robot_display;
    std::function<bool(std::uint32_t callback,std::uint32_t work)> ghost_display;   // Time Attack ghost cars 4AE5F0/4ADAC0
    // ---- race manager (event 359) display 44FE00: begin ----
    std::function<bool(std::uint32_t callback)> race_manager_display;
    // ---- race manager display: end ----
    // Event state of the runtime (flags of 385/386 read by the callbacks).
    void set_events(const driving::PcEventControlState* events){events_=events;}
    const driving::PcEventControlState* events()const{return events_;}
    std::uint32_t current_mode()const{return current_mode_;}   // [78026C] of the frame being rendered
    // Environment blend words 7D2934 / 7D28D8 (44AB10), read by the sun flare (44A690).
    std::int16_t& environment_time_7d2934(){return time_7d2934_;}
    float& environment_duration_7d28d8(){return duration_7d28d8_;}
    // Frontend camera inputs (819634 preset, 780248 pause, 82E7D4 scene).
    std::int32_t camera_preset_819634{};
    std::uint8_t pause_780248{},scene_82e7d4{};
    std::int32_t interpolation_override_82e7d8{};   // 82E7D8 (4872E0), read by 4493E0
    // [799CA0]: the AUTOSCENE (event 6) work, read by 4493E0 through 4B5FD0 (+1C) while event 6 runs;
    // bound by the platform to the robots runtime's work (race_robots_runtime.hpp).
    const std::uint8_t* autoscene_work_799ca0{};
    std::uint32_t camera_inits{},camera_controls{},camera_unanswered{},car_displays{},race_car_displays{};
    // PMT banks (inflated archive bytes) loaded through the native loader
    // 42E490..42E8C0 onto the device.
    bool load_bank(std::uint32_t resource,std::vector<std::uint8_t> pmt,std::string& error);
    bool bank_loaded(std::uint32_t resource)const{return owned_banks_.count(resource)!=0;}
    // Port: the PC loads the banks of its 448AD0 requests in the background, a
    // few objects and textures at a time. queue_bank starts such a load,
    // step_banks advances the queued loads for up to budget_ms (once per frame),
    // and load_bank finishes a queued bank at once when it is drawn earlier.
    // Without this the whole load lands on the frame of the first draw (the
    // first traffic car: 200-300 ms on the console).
    bool queue_bank(std::uint32_t resource,std::vector<std::uint8_t> pmt,std::string& error);
    bool bank_queued(std::uint32_t resource)const;
    void step_banks(double budget_ms);
    // Inflated PMT bytes of a resource ID (the display loads a missing bank
    // through it; false when the resource is not available).
    std::function<bool(std::uint32_t resource,std::vector<std::uint8_t>& pmt)> bank_source;
    std::uint32_t bank_failures{};
    // PC 4103F0(handle, kind): SetVertexShader(NULL), then the shader record
    // of every material of object handle&0xFFFF of bank handle>>16 (4104D0).
    // 0 when the bank has no such object (448810 +0C: 0 when not loaded).
    std::uint32_t object_shaders_4103f0(std::uint32_t handle,std::uint32_t kind);
    // PC 4066D0(handle, type): group type word (+28) of every group of the
    // object; PC 406730(handle, and, or): flags of its first mesh record.
    // Both return 0 for -1 or an object the bank does not have.
    std::uint32_t object_group_type_4066d0(std::uint32_t handle,std::uint32_t type);
    std::uint32_t object_mesh_flags_406730(std::uint32_t handle,std::uint32_t and_mask,std::uint32_t or_mask);
    // 89EDBC (4103A0) / 89EDD4 / 1039EC0 read by the material setup.
    PcShaderGlobals shader_globals{};
    // Clear colour 89BD5C written by 40EC60 (owned by the runtime).
    const std::uint32_t* clear_colour_89bd5c{};
    // Shared sun list words 7D26A8 (three u32, owned by the runtime and also
    // written by the preview 48C170) and the select table 844A08
    // (common/sel_dl_edit0.tgt) that token 0x844A08 resolves to.
    void bind_sun_lists(std::array<std::uint8_t,12>* lists){sun_lists_=lists;}
    void bind_select_table(const std::vector<std::uint8_t>* table){select_table_=table;}
    // Runs fn over the environment frame/context (fog, sun lists resolved,
    // local lights, blend context) and writes the retained state back. Used
    // by 44A8DF and by the preview initializer 4A7270 (449F50).
    void with_environment(driving::Bytes vehicle,driving::Bytes camera,
        const std::function<void(driving::PcEnvironmentFrame&,const driving::CourseCollisionTables&,driving::PcEnvironmentBlendContext&)>& fn);
    driving::PcMatrixStack& matrices(){return matrices_;}
    // scn-efc: loaded bank resources (nullptr when the bank is not loaded).
    PcPmtResources* bank_resources(std::uint32_t resource){auto it=banks_.find(resource);return it==banks_.end()?nullptr:it->second;}
    // scn-efc: display callbacks owned outside the renderer (41BD10 PART_EFC);
    // returns true when the callback was handled.
    std::function<bool(std::uint32_t callback)> external_display;
    std::function<void(const char* stage,std::uint32_t value)> trace;   // development breadcrumbs (autoplay trace=1)
    // ---- race AREA/SKY (events 390/391) display hook: begin ----
    // Display callbacks not handled above are offered to this hook first
    // (callback, work, event id); true when it handled the callback.
    std::function<bool(std::uint32_t callback,std::uint32_t work,std::uint32_t event_id)> race_area_display;
    // PC 2D sprite renderer: display 428170 and the frame leaf 42D710 (sprite_2d_runtime).
    std::function<bool(std::uint32_t callback)> sprite2d_display;
    std::function<bool(std::uint32_t pc,std::uint32_t arg)> leaf_hook;
    // Port profiling (optional): inclusive time of each display callback
    // (kind 0) and frame leaf (kind 1) of render_frame, in nanoseconds.
    void (*profile)(std::uint32_t kind,std::uint32_t pc,std::uint64_t ns){};
    PcFlushContext& flush_context(){return *flush_;}
    // Car shadow volumes (pc_shadow_volume): 46BA20 / 46BB20 on the shared car
    // work (the shadow objects and blocks live in a renderer-owned heap at
    // PC-shaped addresses stored in the car words +2B8..+2C4), and the draw
    // leaves 422550 / 422740 of 46BD30. False (and shadow_error) when a bank
    // or the device cannot serve them.
    // Bootstrap 41784F: the pixel shader 623830 of 95AF9C (particles 41B550),
    // created on first use.
    std::uint32_t pixel_shader_95af9c();
    bool shadow_alloc(driving::Bytes car);
    bool shadow_free(driving::Bytes car);
    std::uint32_t shadow_allocs{},shadow_draws{},shadow_failures{};
    // The draw leaves stay reported as unported until the stencil/vertex
    // shader path is validated on screen (host: OR2_SHADOW_DRAW=1).
    bool shadow_draw_enabled{};
    std::string shadow_error;
    PcRenderContext& queue_context(){return *queue_;}
    // Car reflection cube (414340): the culling view the leaf keeps in step
    // with 95D860 / 404310, and the alpha flush 405830 of its face scenes.
    PcRenderView& render_view(){return view_;}
    // PC screen size 740C8C/740C90 seen by the camera (camera_fov 483C10, +B8 at 484EE0).
    void set_screen(float width,float height){screen_.width_740c8c=width;screen_.height_740c90=height;}
    float screen_width()const{return screen_.width_740c8c;}
    // 406630(token, texture): the bank texture slot token&FFFF of bank token>>16 takes
    // `texture` (the previous one released). False when the bank is not loaded (the
    // PC reads through a null record there); indices past the bank's count do nothing.
    bool texture_swap_406630(std::uint32_t token,std::uint32_t texture);
    std::uint32_t texture_swaps{},texture_swap_failures{},texture_swaps_deferred{};
    // Swaps asked before their bank was loaded (the PC loads it earlier): applied by load_bank.
    std::vector<std::pair<std::uint32_t,std::uint32_t>> pending_swaps_406630;
    // 46BBC0(car): 406630(5B2FE4[car +11 model], [8A89F4] the reflection cube).
    bool environment_map_46bbc0(driving::Bytes car);
    void flush_alpha_405830(){flush_alpha();}
    // ---- race AREA/SKY display hook: end ----
private:
    std::array<std::uint8_t,12>* sun_lists_{};
    const std::vector<std::uint8_t>* select_table_{};
    driving::Bytes environment_list(std::uint32_t token);
    void pull_lists();
    void push_lists();
    struct Bank { std::vector<std::uint8_t> pmt; PcPmtResources resources; PcModelBank model; };
    std::map<std::uint32_t,std::unique_ptr<Bank>> owned_banks_;
    struct PendingBank { std::uint32_t resource{}; std::unique_ptr<Bank> bank; PcPmtLoadCursor cursor{}; bool textures{}; };
    std::vector<PendingBank> pending_banks_;
    std::unique_ptr<Bank> open_bank(std::vector<std::uint8_t> pmt,std::string& error);
    bool step_bank(PendingBank& p);                  // one object or texture; true when the bank is complete
    void install_bank(std::uint32_t resource,std::unique_ptr<Bank> bank);
    PcShaderCache shader_cache_{};
    const driving::PcEventControlState* events_{};
    std::uint32_t current_mode_{};
    driving::PcCameraScreen screen_{};
    std::vector<std::uint8_t> camera_live_fe8_,camera_live_bb0_,camera_live_750_;
    void apply_camera(const driving::PcCameraDevice&,driving::Bytes camera);
    // Port enhancement (enhancements/frame_rate.hpp): the 485FE0 view redone
    // between ticks from the camera before (`before`) and after this tick.
    void note_camera_replay(driving::Bytes camera,const std::vector<std::uint8_t>& before);
    bool display_car();
    bool display_race_car();
    // 49F400 (GamePlCar display): the headlight pool (pc_headlight_pool.hpp).
    bool display_headlight_49f400();
    std::vector<std::uint8_t> headlight_vb_;std::uint32_t headlight_draws_{};
public:
    // Resource bank 3 texture 9 (956D88[3].+8[9]) for 416AA0; 0 while the bank is not loaded.
    std::function<std::uint32_t()> headlight_texture;
    std::uint32_t headlight_draws()const{return headlight_draws_;}
    std::uint32_t select_car_draws()const{return select_car_displays;}
    // 448FD0 layer callbacks (7D2620 + layer * 0xC) and [84A318] for 449050; unset: none / 0.
    std::function<std::uint32_t(std::uint32_t layer)> layer_callback;
    std::function<std::uint32_t()> mode_84a318;
    // [79F5EC] (SCN_EFC work, 0 when event 387 is closed) and its flags word for the course passes 40BD80 / 40BE70.
    std::function<void(std::uint32_t& work,std::uint32_t& flags)> course_effects;
private:
    // 49F4C0 = 46C060 (the arcade ending's car, event 8 function 0x2D): 409F90(+B0), 469FF0(car, +11), 40A010.
    bool display_ending_car();
    std::uint32_t ending_car_displays{};
    // 49F4D0 = 46C090 (the OUTRUN2SP car-select car, event 8 function 0x2B): pc_scene_select_car.cpp.
    bool display_select_car();
    std::uint32_t select_car_displays{};
    PcRaceCarDisplayState race_display_{};
    struct ShadowHeap {
        std::map<std::uint32_t,std::vector<std::uint8_t>> blocks;   // PC-shaped address -> bytes
        std::uint32_t next{0x7a000000u};
        std::map<std::uint32_t,std::uint32_t> bank_base;             // resource -> system base
        std::uint32_t next_bank{0x60000000u};
        std::uint32_t vs_955a40{};bool vs_created{};
        std::uint32_t saved_955a30[4]{};
    } shadow_;
    template<class F> bool with_shadow(driving::Bytes car,F&& body);
    std::uint32_t ps_95af9c_{};bool ps_95af9c_created_{};
    bool shadow_leaf(const PcVehicleDrawCall& call,driving::Bytes car);
    void update_environment(driving::Bytes vehicle,driving::Bytes camera);
    std::int16_t time_7d2934_{};
    float duration_7d28d8_{};
    std::array<std::uint8_t,0x1e0> saved_sun_7d26d0_{};
    std::array<std::uint8_t,0x54> saved_fog_7d28e0_{};
    std::array<std::uint8_t,24> nearest_7d2d58_{};
    PcD3D9Device& device_;
    PcRenderGlobals globals_;
    std::map<std::uint32_t,PcPmtResources*> banks_;
    std::unique_ptr<PcFlushContext> flush_;
    std::vector<std::uint8_t> matrix_arena_;
    driving::PcMatrixStack matrices_;
    PcRenderQueue opaque_,alpha_;
    PcRenderView view_{};
    std::unique_ptr<PcRenderContext> queue_;
    PcSceneEnvironment environment_{};
    PcFrameState frame_{};
    PcSceneDisplayGlobals scene_{};
    void flush_alpha();
};
}
