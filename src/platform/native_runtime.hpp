#pragma once
#include "driving/pc_common_control.hpp"
#include "platform/course_asset_pack.hpp"
#include "platform/course_world_runtime.hpp"
#include "platform/course_environment_runtime.hpp"
#include "platform/loader_asset_pack.hpp"
#include "platform/retail_asset_store.hpp"
#include "platform/frontend_profiles.hpp"
#include "platform/frontend_record_manager.hpp"
#include "platform/vehicle_creation_queue.hpp"
#include "platform/frontend_sprites.hpp"
#include "platform/driving_data_pack.hpp"
#include "platform/pc_vehicle_control.hpp"
#include "platform/vehicle_pose_filter.hpp"
#include "platform/race_asset_pack.hpp"
#include "platform/stage17_asset_pack.hpp"
#include "platform/runtime_input.hpp"
#include "platform/runtime_platform.hpp"
#include "platform/title_owner.hpp"
#include "platform/frontend_title_widgets.hpp"
#include "platform/frontend_license_owners.hpp"
#include "platform/frontend_categories.hpp"
#include "platform/frontend_missions.hpp"
#include "platform/frontend_requests.hpp"
#include "platform/object_db.hpp"
#include "platform/frontend_car_select.hpp"
#include "platform/frontend_transmission.hpp"
#include "platform/frontend_music.hpp"
#include "platform/frontend_rankings.hpp"
#include "platform/frontend_showroom.hpp"
#include "platform/frontend_fixed_choice.hpp"
#include "platform/frontend_network_menus.hpp"
#include "platform/mission_manager.hpp"
#include "platform/race_events.hpp"
#include "platform/native_race_effects.hpp"   // scn-efc: events 387 SCN_EFC / 397 PART_EFC
#include "platform/race_player_car.hpp"
#include "platform/race_robots_runtime.hpp"
#include "platform/race_sound_runtime.hpp"      // events 383 SOUND / 360 COMM_TRANS
#include "platform/pc_sound.hpp"               // PC sound driver (banks, 42F0D0, ICS)
#include "platform/race_traffic_runtime.hpp"   // rival racers / traffic
#include "platform/race_manager_runtime.hpp"   // race manager (event 359) owner
#include "platform/racer_setup.hpp"
#include "driving/pc_environment_blend.hpp"
#include <functional>
#include "platform/world_source_pack.hpp"
#include <array>
#include <memory>
#include <cstdint>
#include <string>
#include <vector>

namespace outrun::platform {

constexpr std::size_t NativeEventFunction36ObjectSize = 0x1000u;
constexpr std::size_t NativeModeCount = 37u;

struct NativeModeDescriptor {
    std::uint32_t unknown_0{};
    std::uint32_t init_callback{};
    std::uint32_t control_callback{};
    std::uint32_t exit_callback{};
};

enum class NativeModePhase : std::uint32_t { Init=0u,Control=1u,Exit=2u };
using NativeModeInvokeCallback = void(*)(void* user,std::uint32_t callback_token,
                                         std::uint32_t mode_index,NativeModePhase phase);
using NativeStartServiceCall = bool(*)(void* user,std::uint32_t pc_entry,
                                       const std::uint32_t* args,std::size_t count,
                                       std::uint32_t& result);
struct NativeModeServices {
    void* user{};
    NativeModeInvokeCallback invoke{};
    // START's unported PC subsystem predicates remain fail-closed.  The
    // callback identifies the original entry point, never a synthetic timer.
    bool (*start_ready)(void* user,std::uint32_t pc_entry){};
    NativeStartServiceCall start_call{};
};

// Host-owned mirror of the globals consumed by PC ModeControl 0x43FA20.  Mode
// callbacks remain 32-bit semantic tokens loaded from the user's EXE.
struct NativeModeControlState {
    std::uint32_t snapshot_source{};       // PC 0x780264
    std::uint32_t snapshot_destination{};  // PC 0x780244
    std::uint32_t requested{};             // PC 0x78025C
    std::uint32_t current{};               // PC 0x78026C
    std::uint32_t previous{};              // PC 0x780268
    std::uint32_t transition_pending{};     // PC 0x780274
    std::uint8_t exit_byte_unknown{};       // PC 0x95B008
    std::uint32_t dispatcher_calls{};
    std::uint32_t init_callbacks{};
    std::uint32_t control_callbacks{};
    std::uint32_t exit_callbacks{};
    std::uint32_t last_callback{};
    std::uint32_t last_mode{};
    std::uint32_t sumo_fe_reset_calls{};
    std::uint32_t sumo_fe_event_setup_calls{};
    std::uint32_t sumo_fe_event_close_calls{};
    std::uint32_t sumo_fe_game_requests{};
    std::uint32_t sumo_fe_owner_command_polls{};
    std::uint32_t sumo_fe_last_owner_command{0xffffffffu};
    bool sumo_fe_reset_pending{};
};

// Native ownership for the PC GAME mode row (index 16).  This mirrors the
// recovered 0x499D90/0x49C840/0x499E50 lifecycle and keeps the still-external
// game subsystems as measured boundaries.  Event resume ranges are applied to
// the real 410-slot scheduler when the original previous-mode gate permits it.
struct NativeGameModeState {
    std::uint32_t init_calls{};
    std::uint32_t control_calls{};
    std::uint32_t exit_calls{};
    std::uint32_t previous_mode{};
    std::uint32_t event_resume_calls{};
    std::uint32_t event_resume_slots{};
    std::uint32_t empty_440d90_calls{};
    std::uint32_t global_effect_calls{};
    std::uint32_t global_effect_last{};
    std::uint32_t timing_reset_calls{};
    std::uint32_t owner_command_polls{};
    std::uint32_t last_owner_command{0xffffffffu};
    std::uint32_t menu_state_polls{};
    // r119: GAME owns the real 0x44DA00 -> 0x44D720 course-loader chain.
    // The immutable pack is borrowed; the relocatable file is copied into
    // mutable storage because the PC builder annotates its 15 records in place.
    const CourseAssetPack* course_assets{};
    // Other \\Scripts\\bin course files (csc_data_2, _ren, _easy) are read
    // from the retail tree like the PC 44D720 loader does.
    RetailAssetStore* course_retail{};
    std::string course_last_path{};
    std::vector<std::uint8_t> course_work{};
    std::array<std::uint8_t,128> course_matrix_stack{};
    driving::PcCourseRuntimeState44d720 course_runtime{};
    driving::PcCourseLoadState44da00 course_load{};
    std::uint32_t course_load_calls{};
    std::uint32_t course_load_success{};
    std::uint32_t course_read_calls{};
    std::uint32_t course_bytes{};
    std::uint32_t course_loader_phase{};
    bool course_provider_loaded{};
    bool course_matrix_ready{};
    std::uint32_t game_variant{1u};       // PC 0x780258
    std::int32_t timeout_countdown{};     // PC 0x836CEC
    std::int16_t start_countdown{};       // PC 0x8367BC
    std::uint8_t transition_latched{};    // PC 0x836CE8
    std::uint32_t pause_controls{};        // mode 18 (pause) 49C980 calls

    // r149: GAME-side native ownership. The direct SUMO_FE -> GAME test path
    // may borrow an explicitly attached course world; the ordinary START path
    // only reuses START's selected world. A deterministic road-supported spawn
    // is permitted only for the explicit direct diagnostic path until the
    // original grid/route spawn owner is reconstructed.
    const DrivingDataPack* driving_data{};
    CourseWorldRuntime gameplay_fallback_world{};
    CourseWorldRuntime gameplay_world{};
    PcVehicleControlState gameplay_vehicle{};
    PcVehicleControlOutput gameplay_vehicle_output{};
    VehiclePoseFilterState gameplay_pose_filter{};
    std::array<float,3> gameplay_position{{0.0f,0.0f,0.0f}};
    std::array<float,3> gameplay_ground_normal{{0.0f,1.0f,0.0f}};
    std::array<float,3> gameplay_camera_eye{{0.0f,3.3f,-4.0f}};
    std::array<float,3> gameplay_camera_target{{0.0f,0.9f,6.0f}};
    float gameplay_yaw{};
    float gameplay_speed{};
    float gameplay_steering_normalized{};
    float gameplay_yaw_step{};
    float gameplay_steering_peak{};
    float gameplay_yaw_step_peak{};
    float gameplay_chase_height{3.3f};
    std::uint32_t gameplay_spawn_quad{};
    std::uint32_t gameplay_spawn_attempts{};
    std::uint32_t gameplay_frames{};
    std::uint32_t gameplay_ground_queries{};
    std::uint32_t gameplay_ground_hits{};
    std::uint32_t gameplay_ground_rejects{};
    std::uint32_t gameplay_sweep_poses{};
    std::uint32_t gameplay_last_quad{};
    bool gameplay_fallback_world_ready{};
    bool gameplay_world_from_start{};
    bool gameplay_direct_fallback{};
    bool gameplay_ready{};
    bool active{};
};

// PC START (mode 13): owns the course bootstrap before GAME.  Stages match
// 0x8367A8; missing platform/service predicates never advance implicitly.
struct NativeStartModeState {
    std::uint32_t init_calls{};
    std::uint32_t control_calls{};
    std::uint32_t exit_calls{};
    std::uint32_t stage{};
    std::uint32_t course_preset{};       // PC 0x78024C
    // 4EEC80 outputs written by the SUMO_FE commit 444350 (656234, 830395,
    // 836174, 780260, 7F94C4); 78024C/780258/836374 live in their own fields.
    driving::PcRuntimePrepareState frontend_prepare{};
    std::uint32_t frontend_prepare_calls{};
    std::uint32_t resource_id{};
    std::uint32_t resource_bytes{};
    std::uint32_t resource_requests{};
    std::uint32_t resource_ready{};
    std::uint32_t gate_waits{};
    std::uint32_t event_open_count{};
    std::uint32_t race_event_count{};
    std::uint32_t stage60_phase{};
    std::uint32_t stage60_route{};
    std::uint32_t stage64_cleanup_step{};
    std::uint32_t game_requests{};
    std::uint32_t course_load_attempts{};
    std::uint32_t course_load_success{};
    std::uint32_t pending_1ee{};
    std::uint32_t stage3_counter{};
    std::uint32_t last_missing_service{};

    std::uint32_t manager_state_7f94c0{};
    std::uint32_t render_mode_754b0c{};
    std::uint32_t course_choice_655b59{};
    std::uint8_t vehicle_colour_655b5a{};
    std::uint8_t vehicle_variant_83036d{};
    std::int32_t mode_countdown_780250{};
    std::uint32_t first_race_flags_7d3a10{};
    std::uint32_t original_route_requests{};
    std::uint32_t scene_owner_calls{};       // PC 0x49BA80
    std::uint32_t scene_owner_fault{};          // first callee without a native equivalent
    std::string scene_owner_fault_reason{};
    std::uint32_t scene_owner_marker_440d90{};
    std::uint8_t flag_830394{};                 // 48B310
    // Time Attack manager (event 0x193, function 0x22, 48B8F0 / 48B950 / 48B920): the goal
    // course loader 830384 {+0, +4, +8 phase 83038C} of 48B550 and the stage result 830398.
    std::uint32_t ta_loader_830384{},ta_loader_830388{},ta_phase_83038c{},ta_result_830398{};
    std::vector<std::uint32_t> scene_owner_list_836958,scene_owner_list_8367f8,scene_owner_list_836850,scene_owner_list_836720;
    std::uint32_t scene_owner_rearm_448ad0{},scene_owner_collision_releases{},scene_owner_target_reset_46fc30{};
    std::array<std::uint8_t,44> scene_owner_models_8367c0{};
    std::array<std::uint32_t,4> scene_owner_collision_lane_state{};
    std::vector<std::string> scene_owner_model_paths;   // 448AD0 requests resolved through 633558
    std::uint32_t scene_owner_stage{5u};     // PC 0x836718
    std::uint32_t scene_owner_missing_service{};
    std::uint32_t scene_owner_sprite_requests{};
    std::uint32_t scene_owner_sprite_bytes{};
    // 0x49B390 / 0x49B3C0 record ordered resource ownership. Unknown PC
    // predicates are -1 and must not silently select a branch.
    std::array<std::uint32_t,64> scene_owner_resource_ids{};
    std::array<std::uint32_t,64> scene_owner_resource_modes{};
    std::uint32_t scene_owner_resource_count{};
    std::uint32_t scene_owner_resource_bytes{};
    std::uint32_t scene_owner_release_count{};
    std::uint32_t scene_owner_meter_id{};
    // PC 0x4276B0 clears the two audio command globals. The current Switch
    // build has no audio channels; a future backend must close live channels
    // before this source boundary may advance.
    std::uint32_t scene_owner_audio_channels_active{};
    std::uint32_t scene_owner_audio_command{}; // PC 0x9560EC
    std::uint32_t scene_owner_audio_phase{};   // PC 0x956110
    std::uint32_t scene_owner_audio_resets{};
    std::uint32_t scene_owner_scheduler_passes{}; // 0x448980, after stage 21
    std::uint32_t scene_owner_sound_checks{};     // 0x427700 x2, audio disabled
    std::uint32_t scene_owner_stage22_requests{}; // 0x49B3F0 mode-8 group
    std::uint32_t scene_owner_stage23_requests{}; // 0x488090(0) -> 0x49B3F0
    std::uint32_t scene_owner_stage23_dynamic_id{};
    std::uint32_t scene_owner_motion_requests{}; // 0x49B420 -> 0x4F2020 groups 6,32,35-39,34
    std::uint32_t scene_owner_motion_bytes{};
    std::uint32_t scene_owner_stage48_reset_driver{}; // 0x488090(1) -> BF/0
    std::uint32_t scene_owner_stage48_character_id{}; // C3/9, CHR_AUT04_CVT
    std::uint32_t scene_owner_stage48_motion_group{}; // 0x17 / mot_ETC
    std::uint32_t scene_owner_stage48_reset_common{}; // 67/0
    // 0x4965A0 performs the 0x49-seeded 30-slot route shuffle only for
    // variant 4, manager kind 1. The manager's load/selection must have
    // actually completed before its route can be used by 0x44DBB0.
    bool scene_owner_route_manager_ready{};
    std::int32_t scene_owner_route_manager_kind{-1}; // [0x83637C+0]
    const RaceAssetPack* scene_owner_race_assets{}; // loaded PC Races.bin
    const RaceAssignmentPack* scene_owner_assignment_assets{}; // PC 0x836370
    std::uint32_t scene_owner_race_key{}; // PC 0x67E6A4, frontend-owned
    std::uint32_t scene_owner_race_sub_key{}; // PC 0x67E6A8
    bool scene_owner_race_key_known{};
    std::uint32_t scene_owner_race_record_index{};
    std::uint32_t scene_owner_race_course_count{};
    // 0x44DBB0(0/2/6) reads descriptor +04/+38/+60. No preview ID is used.
    std::array<std::uint32_t,3> scene_owner_world_ids{};
    bool scene_owner_world_ids_ready{};
    std::uint32_t scene_owner_world_descriptor_index{};
    std::uint32_t scene_owner_stage51_requests{};
    const WorldSourcePack* scene_owner_world_source{};
    std::uint32_t scene_owner_collision_state{}; // PC 0x780194 lane 0: 0/1/2
    std::uint32_t scene_owner_collision_requests{};
    std::uint32_t scene_owner_collision_bytes{};
    CourseWorldRuntime scene_owner_course_world{}; // admitted native roots, not a preview
    std::string scene_owner_collision_error{};
    std::uint32_t scene_owner_world_reset_count{}; // PC 0x44A080
    std::array<std::uint32_t,3> scene_owner_environment_states{}; // 0x7D28CC
    std::uint32_t scene_owner_environment_requests{};
    std::uint32_t scene_owner_environment_ready{};
    std::uint32_t scene_owner_environment_bytes{};
    CourseEnvironmentRuntime scene_owner_environment{};
    std::string scene_owner_environment_error{};
    std::uint32_t scene_owner_environment_fixups{};
    // 0x44AA80 completion globals, not START's own scene-owner stage.
    std::array<std::uint32_t,6> environment_flags_7d28b0{};
    std::uint32_t environment_phase_7d28c8{};
    std::uint32_t scene_owner_course_objects_reset_count{}; // 0x44C2E0
    std::array<std::uint32_t,2> scene_owner_course_object_states{};
    // 0x44C310 transfers two allocation handles. On this hash-pinned build
    // 0x49A650 is literally RET: these are owned files, not a decoded scene.
    std::array<std::vector<std::uint8_t>,2> scene_owner_course_objects{};
    std::array<CourseWorldSourceIdentity,2> scene_owner_course_object_identities{};
    std::uint32_t scene_owner_course_object_requests{};
    std::uint32_t scene_owner_course_object_ready{};
    std::uint32_t scene_owner_course_object_bytes{};
    std::string scene_owner_course_object_error{};

    // r144: PC stage 16 resets two independent owner families for lanes 0/1.
    // 0x4F0380 clears three pointer/state cells; 0x4F0400 clears four.
    std::array<std::uint32_t,2> scene_owner_actor_reset_4f0380{};
    std::array<std::uint32_t,2> scene_owner_event_reset_4f0400{};
    std::uint32_t scene_owner_global_reset_4ef860{};
    std::uint32_t scene_owner_global_reset_4ef850{};
    // Stage 17 0x4F10D0 owns selector-8 (+0x30) data.  For BEAC the
    // following selector-10 (+0x34) source is genuinely null.
    std::array<std::uint32_t,2> scene_owner_object_states{};
    std::array<std::vector<std::uint8_t>,2> scene_owner_object_files{};
    std::array<CourseWorldSourceIdentity,2> scene_owner_object_identities{};
    std::uint32_t scene_owner_object_requests{};
    std::uint32_t scene_owner_object_ready{};
    std::uint32_t scene_owner_object_bytes{};
    std::string scene_owner_object_error{};

    // r146: exact ordinary stage-17 0x46FE50 lane-0 target scheduler.  The
    // table has 12 states, but 0x46FC40 issues 0x44FD20 only for indices 0..7;
    // indices 8..11 are marked complete by the original code without I/O.
    std::uint32_t scene_owner_target_outer_state{};   // PC 0x803730[0]
    std::uint32_t scene_owner_target_index{};         // PC 0x800D44[0], 0..12
    std::array<std::uint32_t,12> scene_owner_target_states{}; // PC 0x804390...
    std::uint32_t scene_owner_target_requests{};
    std::string scene_owner_target_path{};

    // Immutable tables copied by 0x4EFB50/0x4EFAF0 once 0x46FE50 finishes.
    // Bytes come from a hash-pinned private pack; x86 addresses remain tokens.
    std::array<std::uint8_t,Stage17Table68Bytes> scene_owner_table68{};
    std::array<std::uint8_t,Stage17Table6cBytes> scene_owner_table6c{};
    std::uint32_t scene_owner_table68_token{};
    std::uint32_t scene_owner_table6c_token{};
    std::uint32_t scene_owner_stage17_table_bytes{};
    bool scene_owner_table68_ready{};
    bool scene_owner_table6c_ready{};
    // The selected 0x7D30A8 record supplies +0x64 to 0x4EFD20. BEAC is null;
    // preserve the observed token and do not invent a default table.
    std::uint32_t scene_owner_selected64_token{};
    std::uint32_t scene_owner_loader_reset_44fcc0{};
    // 0x44DBB0(10) => primary +0x44. BEAC requests ID 0x117 in mode 10,
    // then the PC moves to stage 50 and waits through 0x448980.
    std::uint32_t scene_owner_stage17_resource_id{};
    std::uint32_t scene_owner_stage17_resource_mode{};
    std::uint32_t scene_owner_stage17_resource_requests{};
    std::uint32_t scene_owner_stage17_resource_ready{};
    std::uint32_t scene_owner_stage17_resource_bytes{};

    // r147: tail of PC scene owner 0x49BA80 after stage 17.  Once the
    // mode-10 sky resource is genuinely owned, the original function falls
    // through 50 -> 53 -> 54 -> 55 -> 63 -> 56 and returns ready.
    std::uint32_t scene_owner_stage50_scheduler_passes{}; // 0x448980
    std::uint32_t scene_owner_stage53_noop_49a650{};       // RET on pinned EXE
    std::uint32_t scene_owner_stage54_noop_47f110{};       // returns 0
    std::uint32_t scene_owner_stage55_restore_452e30{};
    std::uint32_t scene_owner_stage55_restore_452db0{};
    std::uint32_t scene_owner_stage55_variant{};
    std::uint32_t scene_owner_stage55_course_preset{};
    std::array<std::uint32_t,4> scene_owner_stage63_sound_ids{};
    std::uint32_t scene_owner_stage63_sound_checks{};
    std::uint32_t scene_owner_complete_returns{};

    // r148: ordinary offline START tail after the scene owner returns ready.
    // These mirror only the globals actually consumed on the first playable
    // variant-1 path; network/session branches remain fail-closed.
    std::uint8_t network_mode_7df108{};
    std::uint8_t network_player_count_7df10f{};
    bool network_manager_7df34c_present{};
    std::array<bool,2> route_aux_roots_84d6cc{};
    std::uint8_t route_suppress_830394{};
    std::uint8_t route_gate_8361b4{};
    std::uint32_t start_ready_4557f0_checks{};
    std::uint32_t start_bootstrap_440380_calls{};
    std::uint32_t start_bootstrap_event8_setups{};
    std::uint32_t start_bootstrap_records_49fa80{};
    std::array<std::uint32_t,2> start_route_aux_4f0d10_calls{};
    std::uint32_t start_route_init_4871a0_calls{};
    std::uint32_t start_route_entry_count{};
    std::uint32_t start_route_player_and_mask{0xffffffb9u};
    std::uint32_t start_route_player_or_bits{0x38u};
    std::uint8_t start_route_scene_82e7d4{};
    std::uint32_t start_route_mode_82e7d8{};
    std::uint32_t start_route_state_82e7e4{};
    std::uint32_t start_gate_45a920_checks{};
    std::int32_t start_system_handle_67f614{-1};
    std::uint32_t start_cleanup_4999f0_calls{};
    std::uint32_t start_cleanup_428600_calls{};
    std::array<std::uint32_t,3> start_cleanup_42dfb0_ids{};
    std::uint32_t start_cleanup_42dfb0_count{};
    std::array<std::uint32_t,2> start_cleanup_4299c0_ids{};
    std::uint32_t start_cleanup_4299c0_count{};
    std::uint32_t start_loader_mode_7d34c0{};
    std::uint32_t start_loader_mode_writes{};

    std::uint32_t scene_owner_stage26_requests{}; // 0xBC/8 ordinary driver
    std::uint32_t scene_owner_stage28_requests{}; // optional 0xC8/8
    std::uint32_t scene_owner_stage24_car_id{};  // 0x46BBE0(0x655B59)
    // 0x477210 reads 80FB28 (.bss, 0 at boot): 47CF40 sets it for a
    // mission special driver, 47D970 (HUD destroy 47DC00) clears it. START
    // does not touch it.
    std::int32_t scene_owner_special_driver_80fb28{0};
    // 0x4B00D0 is the original VM-backed test of global 0x8421C0 != 0
    // (bridge_4b00d0_probe: 0 -> 0, any other value -> 1). 0x8421C0 is BSS
    // (zero at process start) and every write is inside the event-function
    // 0x21 owner (init 4B0400 -> 4B0040 sets 1, controls 4B0410..4B075C),
    // which START opens (event 0x192) only for variant 9. That owner is not
    // ported, so the value becomes unknown once it is opened.
    std::uint32_t scene_state_8421c0{};
    bool scene_state_8421c0_known{true};
    std::int32_t scene_owner_course_marker{-1};   // PC 0x44C2D0
    std::uint32_t scene_owner_resolve_fail{};      // diagnostic: failing check in scene_owner_resolve_world_ids
    bool session_manager_active_836374{};
    bool selection_active_836374{};
    std::uint8_t game_flag_780248{};
    bool use_versus_resource{};         // PC 0x43F860 / 0x8369C0 decision
    bool active{};
};

// Native ownership for the EvFuncID 36 object reached by event 405.  The PC
// singleton pointer is replaced by bounded storage; callback tokens remain
// semantic dispatch identifiers and are never treated as host pointers.
struct NativeEventFunction36State {
    std::uint32_t pause_resumes{};                                    // 445160 state-3 branch (pause menu closed)
    std::uint32_t owner_teardowns{};                                  // 443C30 at the SUMO_FE exit (49E433)
    std::map<std::uint32_t,std::uint32_t> teardown_globals_skipped;   // its 428600 / 4299C0 / 42DFB0 calls (pc | arg<<24)
    std::array<std::uint8_t,NativeEventFunction36ObjectSize> object{};
    std::array<std::uint8_t,driving::PcRuntimeLoaderState48bf20Size> loader_83039c{};
    std::array<driving::PcObjectEventCallbackEntry,driving::PcObjectEventCallbackCount> callback_table{};
    driving::PcObjectRuntimeSnapshot444350 snapshot{};
    driving::PcObjectSelectorGlobals selector_globals{};
    std::uint32_t init_calls{};
    std::uint32_t control_calls{};
    std::uint32_t display_calls{};
    std::uint32_t destroy_calls{};
    std::uint32_t control_true_returns{};
    std::uint8_t global_mode_6319a1{};
    std::uint8_t last_control_result{};
    std::uint32_t loader_begin_calls{};
    std::uint32_t loader_request_calls{};
    std::uint32_t loader_last_request{};
    std::uint32_t loader_last_request_mode{};
    const LoaderAssetPack* loader_assets{}; // legacy/test provider
    RetailAssetStore* retail_assets{}; // r151 retail PC tree, lazy owner
    std::array<driving::PcRuntimeResourceEntry448ad0,2> loader_resource_entries{};
    std::uint32_t loader_resource_pending{};
    std::uint32_t loader_ready_count{};
    std::uint32_t loader_ready_bytes{};
    std::uint32_t loader_stage4_passes{};
    driving::PcRuntimeSharedLoaderState49e580 shared_loader{};
    std::uint32_t shared_loader_calls{};
    std::uint32_t shared_resource_requests{};
    std::uint32_t shared_last_resource{};
    std::uint32_t shared_last_mode{};
    std::array<std::uint8_t,3> shared_resource_ready{};
    std::uint32_t shared_resource_pending{};
    std::uint32_t shared_ready_count{};
    std::uint32_t shared_ready_bytes{};
    std::uint32_t shared_release_calls{};
    std::uint32_t shared_table_reset_calls{};
    std::uint32_t shared_finalize_calls{};
    std::uint32_t frontend_resource_requests{};
    std::uint32_t frontend_resource_pending{};
    std::uint32_t frontend_ready_count{};
    std::uint32_t frontend_ready_bytes{};
    std::uint32_t frontend_release_calls{};
    std::uint32_t select_table_open_calls{};
    std::uint32_t select_table_read_calls{};
    std::uint32_t select_table_close_calls{};
    std::uint32_t select_table_bytes{};
    std::uint32_t frontend_init_calls{};
    std::uint32_t frontend_async_polls{};
    std::uint32_t frontend_bulk_polls{};
    driving::PcFrontendBulkLoaderState4e8620 frontend_bulk_loader{};
    std::array<FrontendCourseTable,4> frontend_course_tables{};
    std::uint32_t frontend_bulk_missing_pc{};
    std::uint32_t frontend_bulk_requests{};
    std::uint32_t frontend_bulk_pending{};
    std::uint32_t frontend_bulk_ready_count{};
    std::uint32_t frontend_bulk_ready_bytes{};
    std::uint32_t frontend_bulk_format_calls{};
    std::uint32_t frontend_bulk_bind_calls{};
    std::uint32_t frontend_bulk_last_lane{};
    std::uint32_t loader_stage12_passes{};
    driving::PcRuntimeTopLoaderStage12StateR112 loader_stage12_state{};
    std::uint32_t loader_stage12_body_calls{};
    std::uint32_t loader_stage14_passes{};
    std::uint32_t stage12_global_init_calls{};
    std::uint32_t stage12_embedded_init_calls{};
    std::uint32_t stage12_embedded_reset_calls{};
    std::uint32_t stage12_dispatch_calls{};
    std::uint32_t stage12_factory_allocate_calls{};
    std::uint32_t stage12_factory_construct_calls{};
    std::uint32_t stage12_ready_checks{};
    std::uint32_t stage12_transition_init_calls{};
    std::uint32_t stage12_scene_init_calls{};
    std::uint32_t stage12_manager_reset_calls{};
    std::uint32_t stage12_audio_calls{};
    std::uint32_t stage12_optional_calls{};
    std::uint32_t stage12_last_handle{};
    // r113/r114: bounded native backing for the two callback objects reached by
    // the terminal bootstrap (key 0) and the active owner gate (key 22).
    // Their constructors and virtual control methods remain semantic service
    // boundaries.  State key 22 is not the virtual control result.
    std::array<std::uint8_t,0x758u> frontend_root_object{};
    std::uint32_t frontend_welcome_control_calls{};
    std::uint32_t frontend_welcome_scene_token{};
    std::uint32_t frontend_welcome_resource{};
    void* frontend_effect_user{};
    bool (*frontend_effect)(void*,std::uint32_t){};
    std::int32_t frontend_movie_request{-1};
    std::uint32_t frontend_movie_generation{};
    std::uint32_t frontend_choice_scene_token{};
    float frontend_choice_scene_frame{};
    std::array<unsigned,2> frontend_choice_faults{};
    float frontend_choice_timer{};
    void* frontend_choice_service_user{};
    bool (*frontend_choice_service)(void*,unsigned){};
    FrontendRecordManager frontend_records; // process-owned; not reset with event36
    FrontendCategoryState frontend_categories;
    FrontendMissionState frontend_missions;
    FrontendRequestState frontend_requests;
    const RaceAssetPack* frontend_races{};
    const RaceAssignmentPack* frontend_assignment{};
    LicenseProgressTables frontend_category_progress;
    const FrontendFontPack* frontend_fonts{};
    const FrontendTextTable* frontend_text{};
    std::uint32_t frontend_last_missing_key{0xffffffffu};
    std::uint32_t frontend_last_missing_action{};
    std::uint32_t frontend_stack_pushes{};
    std::uint32_t frontend_stack_pops{};
    std::uint32_t frontend_factory_key{};
    std::array<std::uint8_t,0x17b8u> frontend_gate_object{};
    driving::PcFrontendGateList frontend_gate_list{};
    std::uint32_t frontend_profile_count{}; // valid original SaveGame/LicenseN.dat records
    FrontendProfiles frontend_profiles;
    std::string frontend_save_directory; // explicit native save bank; never implicit PC writes
    bool frontend_profiles_initialized{};
    // PC main-thread CRT state, initially 1. Keep it across menu/event resets;
    // future native rand/srand consumers must share this state, not reseed it.
    std::uint32_t pc_crt_random_state{1u};
    FrontendSprites frontend_sprites;
    std::uint32_t frontend_ui_missing_pc{};
    float frontend_ui_motion_step{}; // native frame interval, PC UI seconds
    // PC 7D38F0 (450230/450240) is owned by the race manager: read through
    // runtime->race.manager.state.over_state_7d38f0 (race manager binding).
    std::array<std::uint8_t,0x154u> frontend_first_menu_object{};
    std::array<std::uint8_t,0x154u> frontend_second_menu_object{};
    std::array<std::uint8_t,TitleOwnerPcSize> title_owner_object{};
    // Borrowed owner-specific UI, attached after retail fonts/text are loaded.
    // Must be detached before copying/moving this runtime or releasing fonts.
    FrontendTitleWidgets* title_widgets{};
    FrontendLicenseOwners* license_owners{};
    TitleMenuGlobals title_menu_globals{};
    bool title_manager_resolved{}; // global 7D68AC, required only for variant 4
    TitleOwnerGlobals title_owner_globals{};
    std::uint32_t title_base_global_6591e4{};
    FrontendInputSnapshot frontend_input{};
    std::uint8_t title_game_state_780270{};
    std::uint8_t title_flag_95b250{};
    std::uint32_t title_game_mode_78026c{};
    std::uint32_t title_variant_780258{1u};
    std::uint32_t title_player0_feature_mask{}; // PC 0x453640/0x453BB0 device map not yet ported
    driving::PcEventControlState* title_event_state{}; // borrowed during event-405 control
    std::uint8_t* title_game_flag_780248{}; // borrowed during event-405 control
    std::uint32_t title_pause_flag_95b214{};
    std::uint32_t title_pause_flag_7d2614{};
    std::uint32_t title_pause_calls{};
    std::uint32_t title_last_handle{};
    std::uint32_t title_construct_calls{};
    std::uint32_t title_init_calls{};
    std::uint32_t title_gate_ticks{};
    std::uint32_t title_control_calls{};
    std::uint32_t title_controller_ticks{};
    std::uint32_t title_timeline_service_calls{};
    std::uint32_t title_initial_ui_calls{};
    TitleOwnerInitialUiState4d5e40 title_initial_ui{};
    std::uint32_t title_last_missing_service{};
    std::array<driving::PcNativeHandleBinding,48> frontend_handle_bindings{};
    // Parent key 16 (transmission choice after the car select).
    FrontendTransmission transmission{};
    std::uint32_t transmission_selection_84b210{};
    // Parent key 4 (music choice after the transmission). 84B0EC/84B0F4/830364
    // are process globals kept across visits.
    FrontendMusic music{};
    FrontendMusicGlobals music_globals{};
    // Parent key 36 (course select + local ranking board before the car
    // select for the Heart Attack / Time Attack family, 4CB300).
    FrontendRankings rankings{};
    FrontendRankingGlobals ranking_globals{};
    // Parent key 37 (Time Attack board after key 36, 4CEBF0).
    FrontendGhostBoard ghost_board{};
    FrontendCourseBoard course_board{};
    // Rankings menu boards: key 51 (mode carousel 4CE170) and key 48 (course board 4CCFC0).
    FrontendModeBoard mode_board{};
    FrontendBoard48 board48{};
    FrontendBoard50 board50{};   // key 50/53 rankings list (4DA2F0)
    // Network menus: key 6 (ONLINE / LAN, 4C5CD0) and key 8 (LAN join / create, 4C60B0).
    struct NetworkMenu { std::vector<std::uint8_t> object; bool constructed{}; unsigned fault{}; };
    NetworkMenu network_menu6{std::vector<std::uint8_t>(PcNetworkMenuBytes)},lan_menu8{std::vector<std::uint8_t>(PcLanMenuBytes)},
        online_menu7{std::vector<std::uint8_t>(PcNetworkMenuBytes)};
    // Screens of the network layer (keys in native_network_screen_keys): handle
    // 0x73000100 + key -> the object in the network module's guest heap.
    std::array<std::uint32_t,0x60> network_screens{};
    // Output of the network screens' text leaves (42CA60 font, 42CC00 cursor, 42CDD0
    // printf...): glyphs drawn with the frontend, cleared each frame.
    struct NetworkWidgets {
        const FrontendFont* font{};FrontendTextStyle style{};FrontendTextCursor cursor{};
        std::vector<FrontendGlyph> glyphs_;std::vector<FrontendListImage> images_;std::vector<FrontendWindowIcon> icons_;
        const std::vector<FrontendGlyph>& glyphs()const{return glyphs_;}
        const std::vector<FrontendListImage>& images()const{return images_;}
        const std::vector<FrontendWindowIcon>& icons()const{return icons_;}
        void clear(){glyphs_.clear();images_.clear();icons_.clear();}
    } network_widgets;
    // 401000/401030 streamed BGM (42FBA0): played by the host (game_host menu_music_play,
    // MusicStream) through music_play/music_stop; the requests are also counted.
    std::uint32_t music_play_requests{},music_stop_requests{},music_last_track{~0u};
    std::uint32_t stage12_restore_calls{};    // 444E10 (menu history restored after a race)
    // 40ED70 (shared loader): \media\morph_var.dat as the 89BDB8 table {base, count, value block}
    // (the block is an index into morph_values) and its floats; 95AF10 = morph_loaded.
    std::array<std::array<std::uint32_t,3>,0x400> morph_table_89bdb8{};
    std::vector<float> morph_values;
    bool morph_loaded{};
    std::uint32_t display_captions_446a50{};  // 49E4B0 tails left to the host frame's slot captions
    void* music_user{};
    bool (*music_play)(void*,unsigned channel,unsigned track,unsigned loop){};
    bool (*music_stop)(void*,unsigned channel){};
    // Parent key 10 (car select) and the objects it shares with the preview
    // events and the renderer.
    FrontendCarSelect car_select{};
    // Parent key 43 (Showroom, 4D4230) and the frontend camera preset 819634
    // it switches (482E70), read by the scene renderer.
    FrontendShowroom showroom{};
    // Parent key 49 (rankings selector, 4D9010) and the globals it sets for
    // the key 51/52 boards: 84B20C cursor, 84B0F6 is ranking_globals.ghost_84b0f6.
    std::array<std::uint8_t,PcRankingSelectorBytes> ranking_selector{};
    std::uint32_t ranking_selector_fault{};
    std::int8_t ranking_selector_84b20c{};
    std::uint32_t ranking_notice_63aa6c{};
    std::int32_t camera_preset_819634{};
    struct NativeRuntimeContext* runtime{};
    std::size_t frontend_handle_count{};
    driving::PcUiNotifyGlobals frontend_ui_globals{};
    driving::PcRuntimeControlGlobals frontend_runtime_globals{};
    driving::PcRuntimeTransitionFlags444530 frontend_transition_flags{};
    driving::PcRuntimeQueueState445a50 frontend_queue{};
    std::array<std::uint8_t,0x16dcu> frontend_manager{};
    std::array<std::uint8_t,0x0d08u> frontend_ui_lookup{};
    std::uint32_t state2_frames{};
    std::uint32_t state2_transition_ticks{};
    std::uint32_t state2_ui_state_ticks{};
    std::uint32_t state2_ui_ticks{};
    std::uint32_t state2_embedded_ticks{};
    std::uint32_t state2_gate_ticks{};
    std::uint32_t state2_runtime_transition_ticks{};
    std::uint32_t state2_queue_ticks{};
    std::uint32_t state2_shutdown_ticks{};
    std::uint32_t state2_ui_open_calls{};
    std::uint32_t state2_ui_open_last_key{};
    std::uint32_t state2_ui_valid_calls{};
    std::uint32_t state2_ui_invalid_calls{};
    std::uint32_t state2_ui_last_token{};
    std::uint32_t state2_ui_configure_calls{};
    std::uint32_t state2_ui_create3_calls{};
    std::uint32_t state2_ui_create5_calls{};
    std::uint32_t state2_ui_property_calls{};
    std::uint32_t state2_ui_finalize_calls{};
    std::uint32_t state2_ui_release_calls{};
    std::uint32_t state2_ui_resource_handle{};
    std::uint32_t state2_callback_dispatch_calls{};
    std::uint32_t state2_factory_allocate_calls{};
    std::uint32_t state2_factory_construct_calls{};
    std::uint32_t state2_ready_checks{};
    std::uint32_t state2_last_handle{};
    std::uint32_t state2_selector_calls{};
    std::uint32_t state2_selector_last_result{};
    std::uint32_t state2_selector_child_state{};
    std::uint32_t frontend_gate_input_action{0xffffffffu};
    // A/B edges may arrive while the retail loader is finishing; keep only
    // that discrete edge until the original key owner becomes ready.
    std::uint32_t frontend_pending_input_action{0xffffffffu};
    std::uint32_t frontend_gate_animation_pending{};
    // Updated by the renderer after an authored SUMO_FE frame.  Key 22 must
    // not manufacture animation completion on its own.
    std::uint32_t frontend_authored_animation_ready{};
    std::uint32_t frontend_gate_control_calls{};
    std::uint32_t frontend_gate_owner_actions{};
    std::uint32_t frontend_first_menu_opens{};
    std::uint32_t frontend_first_menu_handle{};
    std::uint32_t frontend_second_menu_handle{};
    std::uint32_t frontend_second_menu_opens{};
    std::uint32_t state2_transition_activations{};
    std::uint32_t state2_gate_configure_calls{};
    std::uint32_t state2_gate_action_calls{};
    // Temporary Switch D-pad menu shim. These nine tokens are genuine
    // 0x443FF0 results, but PC selection/event ownership is not wired yet.
    std::uint32_t frontend_menu_index{};
    std::uint32_t frontend_menu_token{0x00440094u};
    std::uint32_t frontend_menu_navigation{};
    std::uint32_t frontend_menu_confirms{};
    std::uint32_t frontend_menu_preview_requests{};
    std::uint32_t frontend_menu_cancels{};
    bool frontend_menu_committed{};
    std::uint8_t shared_last_result{};
    bool initialized{};
};

// Portable metadata copied from the PC's static event tables.  Values that were
// x86 pointers are retained only as stable 32-bit semantic tokens; the native
// runtime must never dereference them as host pointers.
// Event function 0x20 (event 0x191): mission manager and its 47CF40 racers.
struct NativeMissionState {
    MissionManagerState manager{};
    RacerSetupState racers{};
    std::vector<std::uint8_t> races_relocated;   // Races.bin as 4F12A0 leaves it
    PcAddressView view{};
    bool view_ready{};
    std::uint32_t init_calls{},control_calls{},destroy_calls{},game_part_calls{};
    std::uint32_t missing{};
};
constexpr std::uint32_t NativeRacesBlobBase=0x30000000u;
// Race-time PC globals owned by the small GAME events (race_events.hpp).
struct NativeRaceState {
    RaceGameClock clock{};
    std::uint32_t frames_8367f4{};
    RaceCameraOverride camera_override{};
    std::uint32_t missing{};                  // first missing PC callee of these events
    // Area-manager states read/written by the GAME control (44BE40, 44B9C0..F0)
    // until the AREA event owner (390) is integrated.
    std::uint32_t area_state_7d2e80{},area_state_7d2e88{};
    std::vector<std::uint32_t> sound_commands;   // 4278C0 requests (race audio not ported)
    RaceCarWorld car_world{};                     // globals of the event-8 GamePlCar tree
    std::uint32_t clock_controls{},frame_controls{},override_controls{},rank_controls{};
    NativeRaceRobots robots{};                    // events 362 ROB01 / 364 ROB03 (race_robots_runtime.hpp)
    NativeRaceSound sound{};                      // events 383 SOUND / 360 COMM_TRANS (race_sound_runtime.hpp)
    // ---- race manager (event 359) binding: begin ----
    // Single owner of PC 7D3650..7D39FF (time 7D394C, routes 7D39A0, flags
    // 7D39F0, game-over state 7D38F0 ...): race_manager_runtime.hpp.
    NativeRaceManager manager{};
    // ---- race manager binding: end ----
};
struct NativeRaceAreaRuntime;   // race AREA/SKY owner (race_area_runtime.hpp)
struct NativeSprite2d;          // PC 2D sprite renderer owner (sprite_2d_runtime.hpp)
struct NativeRaceHudRuntime;    // race HUD owner: events 389 / 388 / 401 (race_hud_runtime.hpp)
struct NativeRaceEnd;           // race end modes 20/25/27/28 (race_end_runtime.hpp)
struct NativeGhostRuntime;      // Time Attack ghost module 465F30..4687C0 (race_ghosts_runtime.hpp)
struct NativeRuntimeContext {
    driving::PcEventControlState event_state{};
    std::array<driving::PcEventInitDescriptor,driving::PcEventSlotCount> event_descriptors{};
    std::array<driving::PcEventFunctionDescriptor,driving::PcEventFunctionTableCount> event_functions{};
    std::array<NativeModeDescriptor,NativeModeCount> mode_descriptors{};
    NativeModeControlState mode_state{};
    NativeGameModeState game_mode{};
    NativeStartModeState start_mode{};
    NativeMissionState mission{};
    NativeRaceState race{};
    PcSound pc_sound{};                     // 427700/42F0D0/42EFF0/42F330 (pc_sound.hpp)
    NativeRaceEffectsState race_effects{};       // scn-efc: events 387 / 397 (native_race_effects.hpp)
    VehicleCreationQueue vehicle_creation{};
    driving::PcPlatformInitState417740 platform_init_state{};
    driving::PcRuntimeLoopSetupState417a20 loop_setup_state{};
    driving::PcRuntimeLoopCleanupState417970 loop_cleanup_state{};
    driving::PcRuntimeTimingState417890 timing_state{};
    driving::PcRuntimeFrameState417c7b frame_state{};
    NativeEventFunction36State event_function36{};
    NativeInputState input_state{};
    const Stage17AssetPack* stage17_assets{};
    std::uint32_t completed_frames{};
    std::uint32_t primary_system_token{};
    std::uint32_t secondary_system_token{};
    // Race AREA (event 390) / SKY (event 391) owner, created on first use
    // (native_race_area); also holds the course road tables 46FAC0/46FDE0.
    std::shared_ptr<NativeRaceAreaRuntime> race_area;
    const void* race_area_hooked_renderer{};      // renderer whose 408C80 hooks reach race_area (hook_colours)
    // PC 2D sprite renderer (428170 pool display, 42D710 flush, sprite banks).
    std::shared_ptr<NativeSprite2d> sprite2d;
    std::shared_ptr<NativeRaceHudRuntime> race_hud;
    std::shared_ptr<NativeGhostRuntime> race_ghosts;
    std::shared_ptr<NativeRaceEnd> race_end;
    std::shared_ptr<NativeRaceTraffic> race_traffic;          // race_traffic_runtime.hpp
    PcObjectDb object_db{};                       // 448AB0/448B90/448CD0 (object_db.hpp)
    // 4857C0 (pause Retry/Quit): camera view 0x1A, run by the scene renderer
    // that owns the camera tables and matrix stack (bound by the platform).
    std::function<void()> camera_reset_4857c0;
};


struct NativeRuntimeLoopServices {
    driving::PcRuntimeLoopSetupServices417a20 setup{};
    driving::PcRuntimeFrameServices417c7b frame{};
    driving::PcRuntimeLoopCleanupServices417970 cleanup{};
    void* user{};
    bool (*continue_running)(void* user,std::uint32_t completed_frames){};
    void (*platform_wait)(void* user){};
    NativeRuntimePlatform* platform{}; // r096 concrete clock/wait/exit backend
    NativeRuntimeInput* input{};       // r098 controller sample at ReadIO
    driving::PcEventServices event{};  // r098 native EventControl callbacks
    NativeModeServices mode{};         // native ModeControl callback observer
    bool use_native_event_control{};
    bool use_native_mode_control{};
};

struct NativeRuntimeServices {
    driving::PcStartupOwnerServices4176e0 startup{};
    driving::PcPlatformInitServices417740 platform_init{};
    NativeRuntimeLoopServices loop{};
    bool use_native_platform_init{};
    bool use_native_frame_loop{};
};

constexpr std::uint32_t LegacyEventMetadataVersion = 1u;
constexpr std::uint32_t EventMetadataVersion = 2u;
constexpr std::size_t EventMetadataHeaderSize = 24u;
constexpr std::size_t EventMetadataDescriptorSize = 24u;
constexpr std::size_t EventMetadataFunctionSize = 20u;
constexpr std::size_t EventMetadataModeSize = 16u;
constexpr std::size_t LegacyEventMetadataFileSize = EventMetadataHeaderSize +
    driving::PcEventSlotCount*EventMetadataDescriptorSize +
    driving::PcEventFunctionTableCount*EventMetadataFunctionSize;
constexpr std::size_t EventMetadataFileSize = LegacyEventMetadataFileSize +
    NativeModeCount*EventMetadataModeSize;

bool parse_event_metadata(const std::uint8_t* data,std::size_t size,NativeRuntimeContext& context,std::string* error=nullptr);
bool load_event_metadata_file(const char* path,NativeRuntimeContext& context,std::string* error=nullptr);

// Execute the already-reconstructed EvFuncID 36 lifecycle entries against the
// owned native object. Returns false for callbacks/events outside this bridge.
bool native_runtime_event_function36_invoke(
    NativeRuntimeContext& context,std::uint32_t callback_token,std::uint32_t event_id);
// Execute the display entry when event 405 is active in the current scene.
// Frontend functions called by the network layer and its screens (pc_network.cpp
// FrontendCalls): owner methods (ECX = the owner), 465160 resources and the screen
// base / input (ECX = a guest object of the network module).
bool native_frontend_owner_call(NativeRuntimeContext&,std::uint32_t pc,std::uint32_t ecx,const std::uint32_t* args,std::size_t n,std::uint32_t& eax);
bool native_runtime_event_function36_display(NativeRuntimeContext& context);

// Attach the r113 private pack (layout unchanged from r111) that additionally owns front-end group 44/9,
// the direct sel_dl_edit0.tgt table and all 64 stage-7 scripts. The pack
// remains borrowed for runtime.
bool native_runtime_attach_loader_assets(NativeRuntimeContext& context,
                                         const LoaderAssetPack& pack);
bool native_runtime_attach_retail_assets(NativeRuntimeContext& context,
                                         RetailAssetStore& store);

// Attach the private r119 descriptor/course pack used when GAME mode is
// initialized. The pack remains borrowed for the runtime lifetime.
bool native_runtime_attach_course_assets(NativeRuntimeContext& context,
                                         const CourseAssetPack& pack);
// Attach EXE-derived stage-17 static tables. Course descriptors must already
// be attached so every serialized token can be cross-checked before use.
bool native_runtime_attach_stage17_assets(NativeRuntimeContext& context,
                                          const Stage17AssetPack& pack);
bool native_runtime_attach_race_assets(NativeRuntimeContext& context,
                                       const RaceAssetPack& pack);
bool native_runtime_attach_race_assignment(NativeRuntimeContext& context,
                                           const RaceAssignmentPack& pack);
// Shared PC frontend loader services, used both at bootstrap and after a
// race: 496170/496180 (Races) and 4E85E0/4E8620/4E86E0 (64 scripts).
// A recognized poll may return zero: missing/malformed data is never ready.
bool native_frontend_loader_call(NativeRuntimeContext&,std::uint32_t pc,
                                std::uint32_t& result);
// 4999F0: release the loading animation only if 67F614 still names token 480000.
// Shared by START, the frontend loader and the end-of-race controllers.
void native_loading_animation_close_4999f0(NativeRuntimeContext&);
// Live 447400 provider for license widgets: current PC mode, attached race
// mapping and the actual frontend loader's owned course categories.
bool native_runtime_license_completion(const NativeRuntimeContext&,const PcLicense&,double&);
bool native_runtime_attach_world_source(NativeRuntimeContext& context,
                                        const WorldSourcePack& pack);
// r149 direct-GAME test ownership: attach a validated host-owned road world
// and retail driving tables. This does not mark START assets ready and never
// bypasses the natural START scene-owner gates.
bool native_runtime_attach_gameplay_assets(NativeRuntimeContext& context,
                                           const CourseWorldRuntime& world,
                                           const DrivingDataPack& driving);

// Request a PC mode transition and run the already-reconstructed 0x43FA20
// dispatcher against the portable mode table.  The native path owns the
// recovered SUMO_FE reset and loader-stage-3 subpaths; other bodies remain
// service tokens.
bool native_runtime_request_mode(NativeRuntimeContext& context,std::uint32_t mode_index);
// Source-backed part of the PC START service bridge. Unknown entry points
// return false; callers must never interpret an absent owner as ready.
bool native_start_owned_resource_ready(const NativeRuntimeContext& context,
                                       std::uint32_t pc_entry);
bool native_start_owned_call(NativeRuntimeContext& context,std::uint32_t pc_entry,
                             const std::uint32_t* args,std::size_t count,
                             std::uint32_t& result);
bool native_runtime_mode_control(NativeRuntimeContext& context,const NativeModeServices& services);
// Car select (parent key 10, owner 4C8DE0): virtual slots 4 init, 8 control,
// 12 display, 16/0 suspend; 0x4C8DE0 constructs. Returns the PC result.
std::uint32_t native_car_select_virtual(NativeRuntimeContext& context,std::uint32_t slot);
// 448AD0 / 448960 over the 7C2800 resource table (shared with the car select preview).
bool native_frontend_resource_request(NativeRuntimeContext&,unsigned id,unsigned mode);
bool native_frontend_resource_ready(NativeRuntimeContext&,unsigned id,bool& ready);
void native_runtime_load_course(NativeRuntimeContext&,std::uint32_t mode,std::uint8_t alternate);   // 44DA00
void native_scene_owner_reset_49ba50(NativeRuntimeContext&);
// Parent key 16 (owner 4DD410): slots 4 init, 8 control, 12 display (RET), 16/0 suspend.
std::uint32_t native_transmission_virtual(NativeRuntimeContext& context,std::uint32_t slot);
// Parent key 4 (owner 4C9A90): slots 4 init, 8 control, 12 display (RET), 16/0 suspend.
std::uint32_t native_music_virtual(NativeRuntimeContext& context,std::uint32_t slot);
// Parent key 43 (Showroom, owner 4D4230): slots 0 destroy, 4 init, 8 control,
// 12 display, 16 suspend; 0x4D4230 constructs.
std::uint32_t native_showroom_virtual(NativeRuntimeContext& context,std::uint32_t slot);
// Event function 0x20 callbacks 4964C0 (495B90), 4970E0 (496A30), 49A650, 4964D0.
bool native_mission_event_invoke(NativeRuntimeContext& context,std::uint32_t callback);
// 48B550(&830384, code, flag): the Time Attack goal course (44D720 on the OR2/SP goal scripts,
// or one stage of csc_data_cvt through 43D950). True once loaded (the native read is synchronous).
bool native_goal_course_48b550(NativeRuntimeContext&,std::uint32_t code,std::uint8_t flag_830395);
// The same on another loader (its phase word: 83038C for Time Attack, 83616C for variant 8).
bool native_goal_course_48b550(NativeRuntimeContext&,std::uint32_t code,std::uint8_t flag_830395,std::uint32_t& phase);
// 44D720 of a named course script (data, category; resource >= 0: single-stage resource) at loader phase `phase`.
bool native_course_load_named(NativeRuntimeContext&,const char* data,const char* category,std::int32_t resource,std::uint32_t phase);
// 48BFE0 model preloader tick on 83039C (mode 32 control 49E3E0).
bool native_car_loader_tick_48bfe0(NativeRuntimeContext& context);
// Preview event callbacks owned by the runtime: 4A7270 (event 8 init; its
// 449F50 runs through the renderer environment) and 4A5B20 (control).
// Returns false when the callback is not one of them.
using NativeEnvironmentHook=std::function<void(driving::Bytes vehicle,driving::Bytes camera,
    const std::function<void(driving::PcEnvironmentFrame&,const driving::CourseCollisionTables&,driving::PcEnvironmentBlendContext&)>&)>;
bool native_car_event_invoke(NativeRuntimeContext& context,std::uint32_t callback,
    const NativeEnvironmentHook& environment,driving::PcMatrixStack& matrices);

// Run the portable replacement for the PC 0x417B20 loop boundary.  With no
// continue_running callback it executes one frame; production platforms provide
// an exit/event policy through the callback.
std::uint32_t run_native_frame_loop(NativeRuntimeContext& context,const NativeRuntimeLoopServices& services);

// Platform-neutral replacement entry below the PC WinMain shell.  Switch main
// will populate platform hooks/tokens and call this directly.
std::uint32_t run_native_runtime(NativeRuntimeContext& context,const NativeRuntimeServices& services);

std::uint32_t native_object_db_step_448b90(NativeRuntimeContext&);
} // namespace outrun::platform
