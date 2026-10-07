#pragma once
#include "driving/pc_driving.hpp"
#include "driving/pc_matrix_stack.hpp"
namespace outrun::driving {
struct PcRoadInfoContext;
struct PcStageViews;
struct PcPlWreckerContext;
struct PcCourseAdvanceContext;
struct RearGripInputs {
    std::int32_t volume1{};
    std::int32_t volume2{};
    std::int32_t old_volume1{};
    std::int32_t old_volume2{};
};
// PC 0x004A2FA0 RearGripCtrl. The four analogue-volume reads are explicit
// platform inputs; event+0x2b4 is supplied as a bounded parameter view.
void rear_grip_ctrl(Bytes event,Bytes work,Bytes params,const RearGripInputs& inputs);
// PC 0x004A3260: interpolation helper used by SlipAngleCtrl.
float rear_grip_curve_4a3260(Bytes event,Bytes params,std::int32_t index);
struct SlipAngleInputs {
    std::uint32_t assist_gate{}; // PC [0x7f9460+0x60]
    float assist_state{};        // PC [0x800aac]
};
// PC 0x004A3310 SlipAngleCtrl. Hidden platform globals are explicit inputs.
void slip_angle_ctrl(Bytes event,Bytes work,Bytes params,const SlipAngleInputs& inputs);
// PC 0x004A3950 CorneringCtrl. Uses the explicit native matrix-stack state.
void cornering_ctrl(Bytes event,Bytes work,Bytes params,PcMatrixStack& matrices);
// PC 0x004A5470 AssistWanderer.
void assist_wanderer_4a5470(Bytes event,Bytes work,PcMatrixStack& matrices);
// PC 0x004A0200 base rebound-body helper selected by RebPlBody.
void reb_pl_body_base_4a0200(Bytes event,Bytes work,PcMatrixStack& matrices);
// PC 0x004A08F0 RebPlBody_Sub3. Road-info is an explicit native service;
// the function mutates the current matrix just like the original child body.
void reb_pl_body_sub3_4a08f0(Bytes event,Bytes work,PcRoadInfoContext& road);
void reb_pl_body_sub2_4a0320(Bytes event,Bytes work,PcRoadInfoContext& road,const PcStageViews& stages);
void reb_pl_body_4a62e0(Bytes event,Bytes work,PcRoadInfoContext& road,const PcStageViews& stages);
// PC 0x004A0C70 WrecPlBody. Recovery target/velocity steering and final PlWrecker dispatch.
void wrec_pl_body_4a0c70(Bytes event,Bytes work,Bytes body_params,Bytes wheel_block,PcPlWreckerContext& context);
// Explicit read-only platform/course inputs used by PC 0x005184B0 assCompulsiveMove.
// PC 0x44BE50 selects one of two global course gates from the OnRoadPlace type;
// keeping those booleans here prevents native code from consulting hidden globals.
struct PcAssCompulsiveMoveContext {
    PcCourseAdvanceContext& advance;
    PcRoadInfoContext& road;
    bool primary_course_gate{};
    bool secondary_course_gate{};
};
// PC 0x005184B0 assCompulsiveMove. event+0x2B4 is supplied as body_params
// rather than followed as a serialized guest pointer.
void ass_compulsive_move_5184b0(Bytes event,Bytes work,Bytes body_params,PcAssCompulsiveMoveContext& context);
// Explicit services/views used by PC 0x004A7EC0 CalcPlBody_2nd.  The original
// reaches these through globals and serialized guest pointers; native code keeps
// them bounded and explicit while preserving the original call ordering.
struct PcCalcPlBody2Context {
    PcAssCompulsiveMoveContext& compulsive;
    PcRoadInfoContext& road;
    const PcStageViews& stages;
    PcPlWreckerContext& wrecker;
};
void calc_pl_body_2nd_4a7ec0(Bytes event,Bytes work,Bytes body_params,
                             const std::array<Bytes,4>& tires,Bytes wheel_block,
                             PcCalcPlBody2Context& context);

struct PcNightTunnelInputs {
    bool night{};
    bool tunnel{};
    std::uint8_t course_flags{};
};
// PC 0x004A25F0 CheckNightAndTunnel. Platform queries are explicit inputs.
void check_night_and_tunnel_4a25f0(Bytes event,const PcNightTunnelInputs& inputs);
// PC 0x0046EB40 CalcDispSteeringAngle. Updates event+0x280 from event+0x268/0x27c.
void calc_disp_steering_angle_46eb40(Bytes event);
// PC 0x004A1140 CopyCarWork.  The serialized event wheel/body pointers are
// explicit bounded native views; matrices preserves the original current-matrix side effect.
void copy_car_work_4a1140(Bytes event,Bytes work,Bytes body_params,
                           const std::array<Bytes,4>& tires,PcMatrixStack& matrices);
// PC 0x004A1680 SetCarCamera. Rebuilds the road-relative camera frame
// from event heading/velocity and writes event camera Euler deltas.
void set_car_camera_4a1680(Bytes event,Bytes work,PcMatrixStack& matrices);
struct PcDrivingSkillInputs {
    std::int32_t stage_level{};     // PC GetNowStageLevel(event stage)
    std::uint8_t entry_nodes{};     // PC CommRace_GetEntryNodes()
};
// PC 0x004A4830 CheckDrivingSkill.  The two game-service queries are explicit;
// the original eight-level promotion table is immutable program data.
void check_driving_skill_4a4830(Bytes event,const PcDrivingSkillInputs& inputs);
struct PcChickenDriverInputs {
    std::array<std::int32_t,3> query_status{};       // GetColiPnumByCsLen return values
    std::array<std::int32_t,3> offset_direction{};  // CopGetOfsDir results for center/+8/-8 probes
    std::int32_t volume1{};                         // PC GetVolume(1)
};
// PC 0x004A4900 CheckChickenDriver. Course-query and analogue-volume services
// are explicit native inputs; the complete score/smoothing/counter logic remains local.
void check_chicken_driver_4a4900(Bytes event,const PcChickenDriverInputs& inputs);
struct PcAssistChickenInputs {
    std::uint8_t entry_nodes{}; // PC CommRace_GetEntryNodes()
    std::int32_t volume1{};     // PC GetVolume(1)
    std::int32_t volume2{};     // PC GetVolume(2)
};
// PC 0x004A4BA0 AssistChickenDriver. Network-node and analogue-volume queries
// are explicit native inputs; event-local assist/handicap accumulation is complete.
void assist_chicken_driver_4a4ba0(Bytes event,const PcAssistChickenInputs& inputs);
// PC 0x004A2D70 CalcVibrateMatrix. The original keeps one 30-float (0x78-byte)
// smoothing history per car in process-global storage, indexed by event id-8.
// `histories` is that explicit native storage, laid out as consecutive 0x78-byte slots.
void calc_vibrate_matrix_4a2d70(Bytes event,Bytes histories);
struct PcReverseCarInputs {
    float param20{}; // PC vehicle-parameter +0x20
    float param24{}; // PC vehicle-parameter +0x24
    float param28{}; // PC vehicle-parameter +0x28
    float param2c{}; // PC vehicle-parameter +0x2c
    std::uint32_t random_value{}; // return value consumed from the PC RNG (low 15 bits used)
    std::int32_t game_mode{};     // PC global 0x0078026c
};
// PC 0x004A2910 CheckReverseCar. Vehicle-parameter lookup, RNG and game-mode
// state are explicit inputs; all event-local phase/vibration arithmetic and the
// complete display-matrix construction remain native.
void check_reverse_car_4a2910(Bytes event,const PcReverseCarInputs& inputs,PcMatrixStack& matrices);
// PC 0x004A3D40 CalcLightRate. Event-local light-vector limiting and display
// matrix yaw correction; no hidden platform service is required.
void calc_light_rate_4a3d40(Bytes event,PcMatrixStack& matrices);
struct PcOfsLeftLaneInputs {
    std::int32_t selector{}; // protected PC road/lane selector; -1 forces neutral output
    std::array<float,4> rates{}; // PC road service outputs consumed by the routine
    std::array<CourseProbe,4> points{}; // PC collision/sample points consumed by the routine
};
// PC 0x004A45F0 CalcOfsLeftLane. The packed selector and two road/collision
// service calls are explicit inputs; nearest-point selection and smoothing stay native.
void calc_ofs_left_lane_4a45f0(Bytes event,const PcOfsLeftLaneInputs& inputs);
struct PcRecordGhostInputs {
    std::int32_t steering_override_active{}; // PC global queried by 0x487310
    std::int16_t steering_override{};        // PC 0x487490 low-word result
    std::int32_t game_mode{};                // PC global 0x0078026c
};
// PC 0x004A4710 RecordGhostCar. Platform steering/mode state is explicit and
// `history` is the 30 signed-word moving-average buffer formerly at 0x83db30.
void record_ghost_car_4a4710(Bytes event,Bytes history,const PcRecordGhostInputs& inputs);
struct PcHandicapInputs {
    std::uint8_t active_nodes{};
    std::uint8_t local_car_id{};
    bool race_ready{};
    std::uint16_t reference_course_position{};
    std::uint8_t handicap_table_index{};
    std::int32_t stage_current{};
    std::int32_t stage_reference{};
    std::uint16_t max_cs_len0{};
    std::uint16_t max_cs_len1{};
};
// PC 0x00458E40 HandicapControl. Network/race and course-length services are
// explicit inputs; all event-local handicap ramps and fades are reconstructed.
void handicap_control_458e40(Bytes event,const PcHandicapInputs& inputs);
// PC 0x004962A0 / 0x004962D0: tiny session-manager state predicates used by
// the post-ghost CommonPlCar platform gate.  The process-global manager pointer
// is represented by explicit bounded state instead of a guest pointer.
struct PcSessionModeInputs {
    bool manager_active{}; // PC byte 0x836374
    bool state_present{};  // PC pointer 0x83637c != nullptr
    std::uint32_t state_code{}; // PC manager state +0x20
};
bool session_mode4_4962a0(const PcSessionModeInputs& inputs);
bool session_mode6_4962d0(const PcSessionModeInputs& inputs);

// PC 0x0046F040 / 0x0046F190: three-point X/Z curvature radius helpers used
// by othcarGetR.  The alternate variant converts near-straight and over-limit
// radii to +100000 exactly as the PC body does.
float othcar_calc_r_46f040(CourseProbe a,CourseProbe b,CourseProbe c);
float othcar_calc_r_alt_46f190(CourseProbe a,CourseProbe b,CourseProbe c);

struct PcOthcarGetRInputs {
    std::uint16_t max_cs_len{};                 // closed GetMaxCsLen service result
    std::array<bool,3> road_ok{{true,true,true}}; // three closed road-info calls
    std::array<CourseProbe,3> centers{};        // road-info center vectors (+0x08)
    std::array<bool,2> next_ok{{true,true}};    // two closed CheckNextPlace calls
    bool alternate_radius{};                    // PC global 0x80fb14
};
// PC 0x00479670 == Lindbergh othcarGetR.  Previously closed course services are
// explicit results here; this routine owns the event-local throttle, range gates,
// three-point radius selection and b52/b54 state updates.
void othcar_get_r_479670(Bytes event,const PcOthcarGetRInputs& inputs);

// r044: PC platform/race helpers used by the final CommonPlCar service tail.
// 0x0044FF10 returns bit 2 of the PC race-service flags word.
bool platform_flag_bit2_44ff10(std::uint32_t flags);
// 0x004505A0 is a direct indexed DWORD lookup at the supplied bounded table.
std::uint32_t platform_index_value_4505a0(Bytes table,std::uint32_t index);
// 0x00450630 indexes a flattened PC table at column + row*4.
std::uint32_t platform_pair_value_450630(Bytes table,std::uint32_t row,std::uint32_t column);
// 0x00450750 selects slot 14 when the external gate is active, otherwise slot 4.
bool platform_slot_kind_450750(std::int32_t slot,bool external_gate);
// 0x0048B350 is the signed frame/window predicate used by the PC ghost service.
bool platform_counter_lt_60_48b350(std::int32_t counter);

struct PcPlatformGhost4671Inputs {
    std::int32_t game_mode{};       // PC 0x78026c
    std::int32_t route_state{};     // PC 0x780258
    PcSessionModeInputs session{};  // closed 0x4962A0 dependency
    std::int16_t timer{};           // closed platform timer service result
    std::uint8_t packet_flags{};    // PC 0x7f9220 low byte
    std::int32_t frame_counter{};   // PC 0x656234 consumed by 0x48B350
};
struct PcPlatformGhost4671State {
    // Pointer globals are normalized to offsets from the native packet buffer.
    std::uint32_t writer_offset{};  // PC 0x7f91f0 - packet_base
    std::uint32_t reader_offset{};  // PC 0x7f8d84 - packet_base
    std::uint32_t stream_index{};   // PC 0x7f8ef4
    std::uint8_t packet_11b{};      // packet_base[0x11b]
    std::uint8_t packet_11c{};      // packet_base[0x11c]
    std::uint8_t service_pending{}; // PC 0x64bfec
    std::uint32_t short_window{};   // PC 0x7f92b8
    bool serialize_called{};        // explicit boundary for PC 0x466E50
};
// PC 0x004671D0. The large serializer at 0x466E50 is deliberately an explicit
// service boundary; every local branch/global side effect of this wrapper is native.
void platform_ghost_route_4671d0(Bytes event,const PcPlatformGhost4671Inputs& inputs,
                                 PcPlatformGhost4671State& state);

struct PcPlatformGhost47f780Inputs {
    std::int32_t entry_slot{};          // PC 0x44C940(event+0x68) result
    std::int32_t game_mode{};           // PC 0x78026c
    std::int32_t route_state{};         // PC 0x780258
    bool protected_gate{};              // protected PC 0x450130 result
    std::uint32_t flags{};              // PC 0x7d39f0 (bit 2 is 0x44FF10)
    std::uint32_t allocate_result{};    // PC 0x451180(0) result
    bool slot_external_gate{};          // PC 0x43F960 consumed by 0x450750
    std::int16_t timer{};               // PC 0x49B2D0 low word
    std::uint32_t slot_token{};         // PC 0x810440
};
struct PcPlatformGhost47f780State {
    std::uint32_t init_state{};         // PC 0x81010c
    std::uint32_t sequence{};           // PC 0x810110
    std::uint32_t frame_counter{};      // PC 0x81043c
    std::uint32_t reset114_bits{};      // PC float globals 0x810114..0x81011c
    std::uint32_t reset118_bits{};
    std::uint32_t reset11c_bits{};
    std::uint8_t marker121{};           // PC 0x810121
    std::uint8_t marker123{};           // PC 0x810123
    std::uint8_t record_ready{};        // PC 0x813748
    bool reset_service_called{};        // explicit 0x47ED90 tail-service boundary
    bool record_service_called{};       // explicit 0x47F330 writer boundary
};
// PC 0x0047F780.  Platform lookup/protection/allocation and the two downstream
// record services are explicit inputs/boundaries.  The complete local record
// header/table construction and all directly-owned global state remain native.
// `records` contains consecutive 0xFD4-byte record slots.
void platform_ghost_record_47f780(Bytes event,Bytes records,Bytes index_table,Bytes pair_table,
                                  const PcPlatformGhost47f780Inputs& inputs,
                                  PcPlatformGhost47f780State& state);

// PC 0x004A82C4..0x004A832C inline tail of CommonPlCar. This is parent-owned
// state, not a separate original-routine count.
void common_pl_car_inline_tail_4a82c4(Bytes event,Bytes work);

// r045: small course/service routines reached by GetRoadOfs and the surrounding
// course-state layer. Guest pointers returned by selector entries are normalized
// to explicit choices; the original oracle compares the corresponding PC address.
struct PcCourseServiceState {
    std::uint32_t clear_bc{};      // PC 0x7D33BC
    std::uint32_t clear_c4{};      // PC 0x7D33C4
    std::uint32_t manager_ptr{};   // normalized PC 0x7D3188 ownership cell
    std::uint32_t ready{};         // PC 0x7D2D8C
    std::uint8_t mode_byte{};      // PC 0x7D31D0
};
// PC 0x0044BDB0: returns zero when the course manager is absent, otherwise the
// nested state marker at manager->0x14->0x3C.
std::uint32_t course_nested_marker_44bdb0(bool manager_present,std::uint32_t marker);
// PC 0x0044BE00: direct global stage-limit value.
std::uint32_t course_stage_limit_44be00(std::uint32_t value);
// PC 0x0044BE10: descriptor->+4, or 15 when no descriptor is active.
std::uint32_t course_active_slot_44be10(bool descriptor_present,std::uint32_t slot);
// PC 0x0044BE30 / 0x0044BE40: signed state predicates.
bool course_primary_ready_44be30(std::int32_t state);
bool course_secondary_ready_44be40(std::int32_t state);
// PC 0x0044BE50: selects the primary/secondary course gate from the supplied type.
bool course_type_gate_44be50(std::int32_t type,std::int32_t primary_state,std::int32_t secondary_state);
// PC 0x0044BE80: bounded 66-entry DWORD lookup; out-of-range high indexes use entry 0.
std::uint32_t course_index_lookup_44be80(Bytes table,std::int32_t index);
enum class PcCourseMatrixChoice : std::uint8_t { Primary=0, Secondary=1 };
// PC 0x0044BED0 / 0x0044BEF0: pointer-return selectors normalized to a choice.
PcCourseMatrixChoice course_disp_matrix_choice_44bed0(bool secondary);
PcCourseMatrixChoice course_area_matrix_choice_44bef0(bool secondary);
// PC 0x0044BF10 clears three course-service globals.
void course_clear_service_state_44bf10(PcCourseServiceState& state);
// PC 0x0044C080 / 0x0044C090 / 0x0044C0A0: ready flag and byte service.
void course_mark_ready_44c080(PcCourseServiceState& state);
std::uint8_t course_get_mode_byte_44c090(const PcCourseServiceState& state);
void course_set_mode_byte_44c0a0(PcCourseServiceState& state,std::uint8_t value);
// PC 0x0044C0B0: fixed 30-DWORD (120-byte) snapshot copy.
void course_copy_snapshot_44c0b0(Bytes destination,Bytes source);


// r046: remaining road-offset service layer immediately below GetRoadOfs.
// Pointer-returning PC table selectors are normalized to bounded table indexes.
std::int32_t course_stage_unique_44dc50(bool descriptor_present,
                                        std::uint32_t descriptor_value,
                                        std::uint32_t fallback_value);
std::int32_t road_stage_window_44ddc0(std::int32_t stage_unique,
                                      std::uint16_t course_position,
                                      std::uint16_t rolling_reference);
std::uint32_t road_stage_gate_44f0f0(std::int32_t stage_unique,
                                     std::uint16_t course_position,
                                     std::uint16_t rolling_reference,
                                     std::uint32_t protected_gate_value);

// PC 0x0046FFC0 decodes one packed six-byte road sample to five floats.
// Output layout is x/y/z at +0/+4/+8, signed surface scalar at +0x0c,
// and zero at +0x10.
void road_decode_sample_46ffc0(Bytes packed_sample,Bytes output);

struct PcRoadTableChoice {
    bool valid{};
    bool secondary{};
    std::int32_t index{};
    std::size_t byte_offset{};
};
PcRoadTableChoice road_table_choice_4700d0(std::int32_t type,
                                           std::int32_t row,
                                           std::int32_t selector);
struct PcRoadSampleTables {
    const Bytes* primary_blocks{};
    std::size_t primary_count{};
    const Bytes* secondary_blocks{};
    std::size_t secondary_count{};
};
// PC 0x00479A70.  Table storage is explicit native data; selection, packed
// record decoding, surface gate and the distance comparison remain native.
bool road_side_test_479a70(CourseProbe point,Bytes on_road_place,
                            std::int8_t current_selector,
                            std::int8_t alternate_selector,
                            const PcRoadSampleTables& tables);

struct PcRoadLaneInputs {
    float route_width{};                 // PC 0x479D90 result
    std::uint8_t route_flags{};          // PC 0x46F7A0 result
    std::int32_t stage_unique{};         // PC 0x44DC50 result
    std::uint16_t rolling_reference{};   // only used by 0x44DDC0 stage 0x3a path
    std::uint32_t protected_gate_value{};// PC protected global returned by 0x44F0F0
};
// PC 0x0047B890.  The two larger route services are explicit read-only inputs;
// all lane arithmetic, c32/c33 updates, clamping and stage/type limits are native.
void road_lane_classify_47b890(Bytes event,const PcRoadLaneInputs& inputs);

struct PcRoadCacheRefreshInputs {
    std::int32_t selector_result{}; // protected selector result at PC 0x4A3F8A
    bool query_success{};           // closed GetCsRoadInfoByCsLen service result
    std::array<std::uint8_t,0x58> query_output{}; // post-service road-info bytes
};
// PC 0x004A3F80 cached-road refresh.  The already-closed road query is a
// service boundary here; cache validation/invalidation/publication is complete.
bool refresh_cached_road_4a3f80(Bytes event,std::int32_t polygon_hint,
                                 const PcRoadCacheRefreshInputs& inputs);

struct PcGetRoadOfsInputs {
    PcRoadCacheRefreshInputs cache{};
    PcRoadLaneInputs lane{};
    bool use_lane_classifier{}; // PC global 0x80FB14
};
// PC 0x004A4010 GetRoadOfs.  All leaf services are now native: cached road
// refresh, direction helpers, stage gate, inverse display transform, side test
// and optional lane classification.  The display matrix/tables are explicit
// asset views rather than process-global addresses.
void get_road_ofs_4a4010(Bytes event,const PcGetRoadOfsInputs& inputs,
                         Bytes primary_display_matrix,
                         const PcRoadSampleTables& tables);

// r046: complete PC 0x004A8100 CommonPlCar parent/orchestrator.  Every direct
// child is independently closed elsewhere; this parent exposes the child-call
// boundary as a callback so parent-owned control flow and inline state can be
// differentially validated without duplicating those child implementations.
using PcCommonPlCarServiceCallback = void(*)(void* user,std::uint32_t pc_entry);
struct PcCommonPlCarServices {
    void* user{};
    PcCommonPlCarServiceCallback callback{};
};
struct PcCommonPlCarParentInputs {
    std::int32_t game_mode{};      // PC 0x78026C
    std::int16_t timer{};          // PC 0x49B2D0 low word
    std::int32_t route_state{};    // PC 0x780258
    bool session_mode4{};          // PC 0x4962A0 return
};
void common_pl_car_4a8100(Bytes event,Bytes work,Bytes body_params,
                          const std::array<Bytes,4>& wheels,
                          const PcCommonPlCarParentInputs& inputs,
                          const PcCommonPlCarServices& services);

// PC 0x004A3C60: projected force correction used by CalcPlBody_2nd.
void calc_pl_body_force_limit_4a3c60(Bytes work,float threshold);

// r047: small game/platform state accessors used at the GamePlCar_Ctrl layer.
struct PcGameControlGlobals {
    std::uint8_t flag_780248{};
    std::uint8_t flag_780270{};
    std::uint32_t value_780278{};
};
std::uint8_t game_flag_43f9c0(const PcGameControlGlobals& state);
void set_game_flag_43f9d0(PcGameControlGlobals& state,std::uint8_t value);
void set_game_state_byte_43f9e0(PcGameControlGlobals& state,std::uint8_t value);
std::uint8_t game_state_byte_43f9f0(const PcGameControlGlobals& state);
std::uint32_t game_state_dword_43fa00(const PcGameControlGlobals& state);
void set_game_state_dword_43fa10(PcGameControlGlobals& state,std::uint32_t value);

struct PcGameBroadcastState {
    std::array<std::uint32_t,15> slots{}; // PC 0x7801a8..0x7801e0
    std::uint32_t state_780240{};
    std::uint32_t value_78023c{};
};
// PC 0x0043CC20: broadcasts one value into the per-player game-state slots.
void game_broadcast_43cc20(PcGameBroadcastState& state,std::uint32_t value);

struct PcOperationInputInputs {
    std::int32_t game_mode{}; // PC 0x78026c
    std::int32_t volume0{};
    std::int32_t volume1{};
    std::int32_t volume2{};
};
// PC 0x0049FAD0 OperationInput. Analogue/platform reads are explicit inputs.
void operation_input_49fad0(Bytes event,const PcOperationInputInputs& inputs);

struct PcShiftWarningInputs {
    std::int32_t volume1{};
    std::int32_t volume2{};
};
// PC 0x004A50F0 CheckShiftWarning. The parameter pointer and volume reads are
// explicit bounded inputs; all event-local debounce/state logic is native.
void check_shift_warning_4a50f0(Bytes event,Bytes params,const PcShiftWarningInputs& inputs);

// PC 0x00455F50 CarCalcTotalCsLen. `history` is the 64*0x24-byte global ring;
// the original global sample counter is explicit and is not incremented here.
void car_calc_total_cs_len_455f50(Bytes event,Bytes history,std::uint32_t sample_counter);

struct PcStageProgressInputs {
    std::uint16_t course_end0{};
    std::uint16_t course_end1{};
    bool global_gate{}; // PC 0x80fb14 != 0
    bool record_gate{}; // PC 0x4957F0 return when global_gate is false
};
struct PcStageProgressHistory {
    std::uint32_t count{}; // PC 0x680bd0, capped at 30 by the original
    std::array<std::uint16_t,30> offset{};   // PC 0x841b52 + index*4
    std::array<std::uint16_t,30> position{}; // PC 0x841b50 + index*4
};
// PC 0x004A2130 CarCalcCurrentStageProgress.
void car_calc_current_stage_progress_4a2130(Bytes event,const PcStageProgressInputs& inputs,
                                            PcStageProgressHistory& history);

// PC 0x0045C440 GetNowHeartCalcMode: explicit process-global value getter.
std::uint32_t get_now_heart_calc_mode_45c440(std::uint32_t value);

// PC 0x004A2EE0 SetOldParamBuffer. The 20-sample direction history, wrapped
// heading accumulation and event flag side effect are all event-local.
void set_old_param_buffer_4a2ee0(Bytes event);
}

namespace outrun::driving {
// r048: timer/flag state immediately below ControlTimeUpBraking.
std::uint32_t race_counter_44fdf0(std::uint32_t value);
void set_timeup_counter_44fe30(std::uint32_t& value,std::uint32_t input);
std::uint32_t get_timeup_counter_44fe40(std::uint32_t value);
void set_race_flag0_44fe50(std::uint32_t& flags,bool enabled);
bool get_race_flag0_44fe70(std::uint32_t flags);
void set_race_flag2_44fef0(std::uint32_t& flags,bool enabled);
float ham_nos_speed_45d0e0(float value);

using PcGamePlCarServiceCallback = void(*)(void* user,std::uint32_t pc_entry);
struct PcGamePlCarServices {
    void* user{};
    PcGamePlCarServiceCallback callback{};
};
struct PcGamePlCarParentInputs {
    std::uint8_t game_state_byte{};   // 0x43F9F0 return
    std::uint8_t game_flag{};         // 0x43F9C0 return
    std::uint32_t entry_mode{};       // 0x451350 return; low 2 bits are published at event+4[24:25]
    std::int32_t route_state{};       // PC global 0x780258
    std::uint8_t rank{};              // 0x45A2B0 return
    std::uint32_t heart_mode{};       // 0x45C440 return
    bool network_tail_active{};       // 0x55A930 nonzero
    std::uint16_t course_end{};       // 0x43D470(0) return
    std::uint32_t stage_denominator_base{}; // PC global 0x841FA4 on nonzero event type
    float world_scale_divisor{1.0f};  // PC 0x7162C4
    std::array<std::uint16_t,6> capture_words{}; // stack snapshot words published when 0x800000 is set
};
struct PcGamePlCarParentState {
    std::uint32_t stage_denominator_base{}; // updated PC 0x841FA4
    std::array<std::uint16_t,6> capture_words{}; // PC 0x82EA7A/EB6E/EA78/EB6C/EC60/ED54
    std::array<float,3> world_position{}; // PC 0x82E84C..0x82E854
};
// PC 0x004A8330 GamePlCar_Ctrl parent. All direct child routines remain explicit
// service boundaries here; this function owns and reproduces the inline state,
// branch decisions and call ordering of the parent itself.
void game_pl_car_ctrl_4a8330(Bytes event,const PcGamePlCarParentInputs& inputs,
                             PcGamePlCarParentState& state,
                             const PcGamePlCarServices& services);
}

namespace outrun::driving {
// r049: closes the substantial GamePlCar children immediately beneath the r048
// parent while keeping process-global/platform providers explicit and bounded.
// PC 0x0049FB70 ControlTimeUpBraking.  The already-closed timer getter result is
// an explicit signed input; event+0x2B4 is supplied as the bounded parameter view.
void control_timeup_braking_49fb70(Bytes event,Bytes work,Bytes params,std::int32_t timeup_counter);

// PC 0x004A5260 CheckWanderer.  GetGameMode/route state and the 0x44C940
// stage-level query are explicit inputs; all event/work geometry and timer state
// remain native.
struct PcCheckWandererInputs {
    std::int32_t route_state{};
    std::int32_t stage_level{};
};
void check_wanderer_4a5260(Bytes event,Bytes work,const PcCheckWandererInputs& inputs);

// PC 0x004A5650 HAM_Nos_SetSpeed.  The already-closed 0x45D0E0 getter result is
// explicit; the remaining vector, parameter, conversion and wheel-speed writes
// are reproduced locally.
void ham_nos_set_speed_4a5650(Bytes event,Bytes work,Bytes params,float nos_speed);

// PC 0x0055A930: tiny network/session object state getter.
std::uint32_t network_tail_state_55a930(Bytes network_object);

using PcNetworkTailCallback = void(*)(void* user,Bytes first,Bytes second);
struct PcNetworkTailServices {
    void* user{};
    PcNetworkTailCallback callback{};
};
// PC 0x0046C390: forwarding wrapper into the external manager method 0x4FB870.
// The manager implementation itself is intentionally not folded into this wrapper.
void network_tail_forward_46c390(Bytes first,Bytes second,const PcNetworkTailServices& services);

using PcRankProviderCallback = std::uint32_t(*)(void* user,std::uint8_t player_id);
struct PcRankProviderServices {
    void* user{};
    PcRankProviderCallback callback{};       // mode 16, variants 3/4: 456870 ? 459D10(player) : 459E10(player) (LAN)
    std::uint32_t game_mode_78026c{0x10};
    std::uint32_t game_variant_780258{3};
    Bytes ranks_7df118{nullptr,0};           // byte per player id
};
// PC 0x0045A2B0 (the FXT EXE gates its first instruction through 0x01039CCC; the
// Steam build has it in plain code): outside mode 16 the byte 7DF118[player]; in
// mode 16, 0 unless variant 3/4, where the LAN rank provider (callback) answers.
std::uint8_t rank_provider_gateway_45a2b0(std::uint8_t player_id,const PcRankProviderServices& services);

// r050: PC 0x004A4D20 CheckSlipStream.  The original scans event ids 9..31
// through process-global event tables.  Native code receives those 23 candidate
// records explicitly; only the candidate cooldown field is mutable.
struct PcSlipstreamCandidate {
    std::uint8_t open_state{};      // low two bits of PC 0x79FB48[event_id]
    std::uint32_t flags{};          // candidate event +0x04
    std::uint32_t network_state{};  // candidate event +0x0D14
    CourseProbe position{};         // candidate event +0x14
    CourseProbe direction{};        // candidate event +0x20
    float speed{};                  // candidate event +0x1C4
    std::int16_t cooldown{};        // candidate event +0x0B72, updated in place
};
struct PcCheckSlipStreamInputs {
    bool network_session_active{};  // PC 0x55A930(0x7F9460) != 0
    std::array<PcSlipstreamCandidate,23> candidates{}; // ids 9..31
};
void check_slipstream_4a4d20(Bytes event,PcCheckSlipStreamInputs& inputs);

// r051: PC 0x00475720..0x004758B1, aligned with symbolized Lindbergh
// PasPlCar_Ctrl(void*).  Direct children remain explicit service boundaries;
// this parent owns the petty-auto gate, four-record loop and event/road writes.
struct PcPasPlCarWheelInput {
    CourseProbe transformed_point{}; // result of PC 0x40A7D0
    float road_y{};                  // point.y after PC 0x43EB60
    std::uint32_t road_polygon{};    // value written through road-record +0x10
    std::uint32_t road_result{};     // final out value tested against 1
    CourseProbe tire_position{};     // PC 0x46BBF0 result
    CourseProbe road_normal{};       // PC 0x43D390 result
};
struct PcPasPlCarInputs {
    std::uint8_t petty_auto_scene_list{}; // PC 0x4872F0 return
    std::array<PcPasPlCarWheelInput,4> wheel{};
};
using PcPasPlCarServiceCallback = void(*)(void* user,std::uint32_t pc_entry,std::uint32_t wheel_index);
struct PcPasPlCarServices {
    void* user{};
    PcPasPlCarServiceCallback callback{};
};
void pas_pl_car_ctrl_475720(Bytes event,const std::array<Bytes,4>& road_records,
                            const PcPasPlCarInputs& inputs,const PcPasPlCarServices& services);
}

namespace outrun::driving {
// r052: native model of the PC event scheduler.  The PC build owns 410
// 0x3c-byte records at 0x799B30 and a separate 410-byte flag array at 0x79FB48.
// Callback pointers and work pointers are represented as 32-bit tokens so the
// same state can be compared directly with the original i386 implementation.
constexpr std::size_t PcEventSlotCount = 410;
constexpr std::uint32_t PcEventInvalidSlot = 0xffffffffu;
struct PcEventSlot {
    std::uint8_t flags{};          // PC 0x79FB48 + event id
    std::uint32_t descriptor_token{}; // record +0x00, static descriptor token
    std::uint32_t event_id{};      // record +0x04
    std::uint32_t work_token{};    // record +0x08
    std::uint32_t display_scene{}; // record +0x0c
    std::uint32_t init_callback{}; // record +0x10
    std::uint32_t ctrl_callback{}; // record +0x14
    std::uint32_t disp_callback{}; // record +0x18
    std::uint32_t shadow_callback{}; // record +0x1c
    std::uint32_t dest_callback{}; // record +0x20
    std::uint32_t aux24{};         // record +0x24, cleared by InitEventControl
    std::uint32_t aux28{};         // record +0x28, descriptor column +0x08
    std::uint32_t close_guard{};   // record +0x2c, descriptor startup/open guard
    std::uint32_t function_id{};   // record +0x30, descriptor function id
    std::uint32_t aux34{};         // record +0x34, cleared by InitEventControl
    std::uint32_t aux38{};         // record +0x38, cleared by InitEventControl
};
struct PcEventControlState {
    std::array<PcEventSlot,PcEventSlotCount> slots{};
    std::uint32_t current_slot{PcEventInvalidSlot};
    // PC 0x007A0DB0, owned by SetEvPauseFlag/ClrEvPauseFlag.
    std::uint32_t pause_depth{};
};
using PcEventInvokeCallback = void(*)(void* user,std::uint32_t callback_token,
                                      std::uint32_t work_token,std::uint32_t event_id);
using PcEventSetupCallback = void(*)(void* user,PcEventControlState& state,
                                     std::uint32_t event_id,std::uint32_t function_id);
struct PcEventServices {
    void* user{};
    PcEventInvokeCallback invoke{};
    PcEventSetupCallback setup{};
};

// PC 0x0043FAB0 EventControl.  Owns the 410-slot dest/init/ctrl dispatch loop.
void event_control_43fab0(PcEventControlState& state,const PcEventServices& services);
// PC 0x00440180 EventOpen.  The r086 direct-table overload below closes its
// former 0x440110 setup/provider boundary while this service form is retained
// for inherited differential tests.
void event_open_440180(PcEventControlState& state,std::uint32_t event_id,
                       std::uint32_t function_id,const PcEventServices& services);
// PC 0x004401D0/0x00440200/0x00440240/0x00440330.
void event_close_4401d0(PcEventControlState& state,std::uint32_t event_id);
void event_close_immediate_440200(PcEventControlState& state,std::uint32_t event_id,
                                  const PcEventServices& services);
void event_close_all_440240(PcEventControlState& state);
void event_close_serial_440330(PcEventControlState& state,std::uint32_t first,std::uint32_t count);
// PC 0x00440370 CheckEventDestructing.
bool check_event_destructing_440370(const PcEventControlState& state,std::uint32_t event_id);
// PC 0x00440B30 GetEventId.  The supplied array is the PC static 410-entry
// work/default-token column; 410 is the original not-found sentinel.
std::uint32_t get_event_id_440b30(const std::array<std::uint32_t,PcEventSlotCount>& defaults,
                                 std::uint32_t token);
// PC 0x00440B80 / 0x00440B90 / 0x00440BA0.
std::uint32_t get_now_event_id_440b80(const PcEventControlState& state);
void change_now_event_ctrl_func_440b90(PcEventControlState& state,std::uint32_t callback_token);
void change_now_event_shadow_func_440ba0(PcEventControlState& state,std::uint32_t callback_token);
// PC 0x00440BB0 ChangeCtrlFunc and 0x00440BD0 ChangeDispScene.
void change_ctrl_func_440bb0(PcEventControlState& state,std::uint32_t event_id,
                             std::uint32_t callback_token);
void change_disp_scene_440bd0(PcEventControlState& state,std::uint32_t event_id,
                              std::uint32_t display_scene);


struct PcEventInitDescriptor {
    std::uint32_t descriptor_token{}; // PC table 0x599808 + id*0x18 +0x00
    std::uint32_t work_token{};       // +0x04
    std::uint32_t aux28{};            // +0x08
    std::uint32_t startup{};          // +0x0c: non-zero installs function callbacks
    std::uint32_t function_id{};      // +0x10: index into 0x59BE78 function table
    std::uint32_t display_scene{};    // +0x14
};
struct PcEventFunctionDescriptor {
    std::uint32_t init_callback{};
    std::uint32_t ctrl_callback{};
    std::uint32_t disp_callback{};
    std::uint32_t shadow_callback{};
    std::uint32_t dest_callback{};
};
constexpr std::size_t PcEventFunctionTableCount = 128;

// r086 PC 0x00440110..0x00440176. Static EvFunc provider used by EventOpen.
// The protected transfer after loading event_id supplies the current slot flag
// byte; the visible body clears bits 0x18, sets init bit 0x01, copies all five
// callbacks, and refreshes display_scene. Tables are explicit native data.
void event_setup_440110(
    PcEventControlState& state,std::uint32_t event_id,std::uint32_t function_id,
    const std::array<PcEventInitDescriptor,PcEventSlotCount>& descriptors,
    const std::array<PcEventFunctionDescriptor,PcEventFunctionTableCount>& functions);

// Direct native EventOpen path with the former provider boundary removed.
void event_open_static_440180(
    PcEventControlState& state,std::uint32_t event_id,std::uint32_t function_id,
    const std::array<PcEventInitDescriptor,PcEventSlotCount>& descriptors,
    const std::array<PcEventFunctionDescriptor,PcEventFunctionTableCount>& functions,
    const PcEventServices& services);

// r053 PC 0x00440BF0 InitEventControl: clear/fill all 410 records from
// the two static descriptor tables.  Table contents remain explicit inputs.
void init_event_control_440bf0(
    PcEventControlState& state,
    const std::array<PcEventInitDescriptor,PcEventSlotCount>& descriptors,
    const std::array<PcEventFunctionDescriptor,PcEventFunctionTableCount>& functions);
// r053 PC 0x00440A10/0x00440A30/0x00440A50.
void event_suspend_440a10(PcEventControlState& state,std::uint32_t first,std::uint32_t count);
void event_resume_440a30(PcEventControlState& state,std::uint32_t first,std::uint32_t count);
std::uint32_t check_event_suspend_440a50(const PcEventControlState& state,std::uint32_t event_id);

// r054: pause ownership around the 410-slot scheduler.  The three PC child
// services are platform/audio boundaries; the callback records their exact
// original entry and argument while the event flags remain native state.
using PcEventBoundaryCallback = void(*)(void* user,std::uint32_t pc_entry,std::uint32_t argument);
struct PcEventPauseServices {
    void* user{};
    PcEventBoundaryCallback boundary{};
};
void set_ev_pause_flag_440930(PcEventControlState& state,const PcEventPauseServices& services);
void clr_ev_pause_flag_4409c0(PcEventControlState& state,const PcEventPauseServices& services);

// r054 PC MallocNowEventWork/FreeNowEventWork.  The PC allocation appends an
// 8-byte ownership trailer {base,type} at base+requested_size and stores that
// trailer token in event record +0x24 while +0x08 receives the base work token.
// Native code keeps that trailer as explicit metadata instead of dereferencing
// a guest pointer.
struct PcEventWorkHandle {
    std::uint32_t wrapper_token{};
    std::uint32_t base_token{};
    std::uint32_t heap_type{};
    bool live{};
};
using PcEventAllocateCallback = std::uint32_t(*)(void* user,std::uint32_t total_bytes);
using PcEventReleaseCallback = void(*)(void* user,std::uint32_t base_token);
struct PcEventWorkServices {
    void* user{};
    PcEventBoundaryCallback boundary{};
    PcEventAllocateCallback allocate{};
    PcEventReleaseCallback release{};
};
std::uint32_t malloc_now_event_work_440a60(PcEventControlState& state,PcEventWorkHandle& handle,
                                           std::uint32_t requested_size,std::uint32_t heap_type,
                                           const PcEventWorkServices& services);
void free_event_work_handle_440cd0(PcEventControlState& state,PcEventWorkHandle& handle,
                                   const PcEventWorkServices& services);
void free_now_event_work_440b20(PcEventControlState& state,PcEventWorkHandle& handle,
                                const PcEventWorkServices& services);

// r055: two independent four-level allocator state stacks used around the PC
// heap wrapper.  The public entries 0x440D10/30 and protected 0x440D50/70
// own only these stack/depth transitions; the actual allocator remains a
// separate service.  Five storage cells are retained because the original
// pop reads stack[depth] before decrementing, including the sentinel cell at
// depth 4.
struct PcAllocatorStateStacks {
    std::array<std::uint32_t,5> stack_a{};
    std::uint32_t depth_a{};
    std::array<std::uint32_t,5> stack_b{};
    std::uint32_t depth_b{};
};
void push_alloc_state_a_440d10(PcAllocatorStateStacks& state,std::uint32_t value);
std::uint32_t pop_alloc_state_a_440d30(PcAllocatorStateStacks& state);
void push_alloc_state_b_440d50(PcAllocatorStateStacks& state,std::uint32_t value);
std::uint32_t pop_alloc_state_b_440d70(PcAllocatorStateStacks& state);

// PC 0x440CC0: the small handle-slot transfer used by several ownership
// wrappers.  Jennifer's corresponding helper is hmmHandleMove; the PC body
// visible here transfers the source slot token into the destination slot.
void hmm_handle_move_440cc0(std::uint32_t& destination,std::uint32_t source);

// PC 0x440CA0 is a compact bit-mask wrapper around service 0x43FB40.  No
// Jennifer source name is claimed.  Keep the child call explicit.
using PcMaskedServiceCallback = void(*)(void* user,std::uint32_t mask,std::uint32_t zero,std::uint32_t id);
void masked_service_440ca0(std::uint32_t bit_index,void* user,PcMaskedServiceCallback callback);

// r056: compact object/state helpers immediately following the allocator wrappers.
// These routines operate on a PC object whose validated fields extend through
// +0x51c.  Names are descriptive unless a source-symbol correspondence is
// explicitly stated elsewhere; no full C++ class declaration is claimed.
std::uint32_t object_take_token_440dc0(Bytes object);
void object_set_token_440de0(Bytes object,std::uint32_t value);
void object_set_field4_440df0(Bytes object,std::uint32_t value);
void object_set_field8_440e00(Bytes object,std::uint32_t value);

using PcObjectChildVoidCallback = void(*)(void* user,std::uint32_t pc_entry,std::uint32_t child_offset);
using PcObjectChildBool2Callback = std::uint8_t(*)(void* user,std::uint32_t pc_entry,std::uint32_t child_offset,
                                                   std::uint32_t arg0,std::uint32_t arg1);
struct PcObjectChildServices {
    void* user{};
    PcObjectChildVoidCallback call_void{};
    PcObjectChildBool2Callback call_bool2{};
};
// PC 0x440E10/0x440E60: depth byte at +0x220 with two parallel byte stacks
// beginning +0x221/+0x241.  The signed comparisons and byte wrap are retained.
void object_push_byte_state_440e10(Bytes object,const PcObjectChildServices& services);
void object_pop_byte_state_440e60(Bytes object,const PcObjectChildServices& services);
// PC 0x440EA0/B0/C0: thin tail-call wrappers on the embedded child at +0x51c.
void object_child_call_a_440ea0(Bytes object,const PcObjectChildServices& services);
void object_child_call_b_440eb0(Bytes object,const PcObjectChildServices& services);
void object_child_call_c_440ec0(Bytes object,const PcObjectChildServices& services);
// PC 0x440ED0: only forwards when object+0x218 == 2; otherwise returns AL=0.
std::uint8_t object_child_conditional_440ed0(Bytes object,std::uint32_t arg0,std::uint32_t arg1,
                                            const PcObjectChildServices& services);

// r057: compact float transition controller at PC 0x440EF0..0x441193 and
// its directly observed global-state leaves at 0x4C50C0/4C50E0/4C50F0/4C5100/4C5110.
// Names remain descriptive until a source-symbol mapping is proven.
struct PcFloatTransitionGlobals {
    std::uint32_t token{7};             // 68A670: .data initial value 7 (4C50A0's 2D flush bound)
    float primary{1.0f};
    float secondary{1.0f};
    std::uint8_t active{};
};
std::uint8_t transition_global_get_active_4c50c0(const PcFloatTransitionGlobals& globals);
void transition_global_set_token_4c50e0(PcFloatTransitionGlobals& globals,std::uint32_t value);
void transition_global_set_primary_4c50f0(PcFloatTransitionGlobals& globals,float value);
void transition_global_set_secondary_4c5100(PcFloatTransitionGlobals& globals,float value);
void transition_global_clear_active_4c5110(PcFloatTransitionGlobals& globals);
// PC 0x4C50D0 (protected jmp [1039BF4]): measured by bridge_4c50d0_probe, it
// stores 1 into the active byte 84A9FD and nothing else.
void transition_global_set_active_4c50d0(PcFloatTransitionGlobals& globals);

using PcFloatTransitionActivateCallback = void(*)(void* user);
struct PcFloatTransitionServices {
    void* user{};
    PcFloatTransitionActivateCallback activate{};
};
void object_transition_init_440ef0(Bytes object,PcFloatTransitionGlobals& globals);
void object_transition_config_440f70(Bytes object,std::uint8_t mode,std::uint32_t frames,std::uint32_t unused,
                                     PcFloatTransitionGlobals& globals);
void object_transition_update_441020(Bytes object,PcFloatTransitionGlobals& globals,
                                     const PcFloatTransitionServices& services);
void object_transition_latch_441130(Bytes object);

// r058: compact runtime/object controller at PC 0x4411A0..0x441366 plus
// directly consumed leaves at 0x454200/220/230/240, 0x4464F0 and 0x564C90.
// Names are descriptive; no Jennifer source-name identity is claimed here.
struct PcRuntimeControlGlobals {
    std::uint8_t enabled_d2{};
    std::uint8_t blocked_bf{};
    std::uint8_t gate_d4{};
    std::uint8_t started_d1{};
    std::uint32_t current_ac{};
    std::uint32_t alternate_b0{};
    std::uint32_t mode_836130{};
    std::uint32_t reset_word_659930{};
};
using PcRuntimeVoidHandle = void(*)(void* user,std::uint32_t pc_entry,std::uint32_t handle);
using PcRuntimeVoidHandleArg = void(*)(void* user,std::uint32_t pc_entry,std::uint32_t handle,std::uint32_t arg);
using PcRuntimeU8Handle = std::uint8_t(*)(void* user,std::uint32_t pc_entry,std::uint32_t handle);
using PcRuntimeU32Handle = std::uint32_t(*)(void* user,std::uint32_t pc_entry,std::uint32_t handle);
using PcRuntimeBoolPair = std::uint8_t(*)(void* user,std::uint32_t pc_entry,std::uint32_t lhs,std::uint32_t rhs);
using PcRuntimeI8Handle = std::int8_t(*)(void* user,std::uint32_t pc_entry,std::uint32_t handle);
struct PcRuntimeControlServices {
    void* user{};
    PcRuntimeVoidHandle call_void{};
    PcRuntimeVoidHandleArg call_void_arg{};
    PcRuntimeU8Handle call_u8{};
    PcRuntimeU32Handle call_u32{};
    PcRuntimeBoolPair call_bool_pair{};
    PcRuntimeI8Handle call_i8{};
};
void runtime_manager_reset_454200(Bytes manager,PcRuntimeControlGlobals& globals);
void runtime_set_gate_454220(PcRuntimeControlGlobals& globals,std::uint8_t value);
std::uint8_t runtime_get_gate_454230(const PcRuntimeControlGlobals& globals);
std::uint8_t runtime_finish_454240(PcRuntimeControlGlobals& globals,const PcRuntimeControlServices& services);
void runtime_child_counter_dec_4464f0(Bytes child);
std::uint32_t runtime_child_state_564c90(Bytes child);
void runtime_shutdown_4411a0(PcRuntimeControlGlobals& globals,Bytes current_object,
                              const PcRuntimeControlServices& services);
void runtime_release_handle_441200(Bytes object,std::uint32_t handle,const PcRuntimeControlServices& services);
std::uint8_t runtime_ready_441260(Bytes object,const PcRuntimeControlGlobals& globals,
                                  const PcRuntimeControlServices& services);
std::uint32_t runtime_status_4412c0(Bytes object,const PcRuntimeControlServices& services);
std::uint8_t runtime_has_handle_4412f0(Bytes object);
std::uint8_t runtime_close_if_status_441300(Bytes object,std::uint32_t requested,
                                            const PcRuntimeControlServices& services);

// r059: compact UI/resource controller around PC 0x441370..0x441442 plus
// directly consumed leaves at 0x465250/0x4652E0/0x42CC00/0x42CCA0/0x42CCB0/0x465EB0.
// Names are descriptive; no exact Jennifer source-name identity is claimed.
struct PcUiNotifyGlobals {
    std::int16_t x{};          // PC 0x956BB8
    std::int16_t y{};          // PC 0x956BBA
    std::int16_t base_x{};     // PC 0x956BB4
    std::int16_t base_y{};     // PC 0x956BB6
    std::uint32_t color{};     // PC 0x956BCC
    std::uint32_t mode{};      // PC 0x956BD0
    std::uint32_t source_x{};  // PC 0x631B30
    std::uint32_t source_y{};  // PC 0x631B34
    std::uint8_t alternate{};  // PC 0x7D68BC
};
using PcUiHandleVoid = void(*)(void* user,std::uint32_t pc_entry,std::uint32_t handle);
using PcUiHandleI32 = std::int32_t(*)(void* user,std::uint32_t pc_entry,std::uint32_t handle);
using PcUiU32Void = void(*)(void* user,std::uint32_t pc_entry,std::uint32_t value);
using PcUiLookup = std::uint32_t(*)(void* user,std::uint32_t pc_entry,std::uint32_t id);
using PcUiDraw = void(*)(void* user,std::uint32_t pc_entry,std::uint32_t format,std::uint32_t argument);
struct PcUiNotifyServices {
    void* user{};
    PcUiHandleVoid handle_void{};
    PcUiHandleI32 handle_i32{};
    PcUiU32Void value_void{};
    PcUiLookup lookup{};
    PcUiDraw draw{};
};
void ui_resource_reset_465250(Bytes object,const PcUiNotifyServices& services);
std::uint32_t ui_resource_ready_4652e0(Bytes object,const PcUiNotifyServices& services);
void ui_text_set_xy_42cc00(PcUiNotifyGlobals& globals,std::uint32_t x,std::uint32_t y);
void ui_text_set_color_42cca0(PcUiNotifyGlobals& globals,std::uint32_t color);
void ui_text_set_mode_42ccb0(PcUiNotifyGlobals& globals,std::uint32_t mode);
std::uint32_t ui_table_lookup_465eb0(Bytes table,std::uint32_t index);
void ui_notify_441370(Bytes object,PcUiNotifyGlobals& globals,const PcUiNotifyServices& services);
void ui_set_active_4413f0(Bytes object,std::uint8_t active,const PcUiNotifyServices& services);
void compact_u32_list_441410(Bytes object,std::int32_t index);

// r060: normal-path semantic reconstruction of the first continuous factory
// wrapper block at PC 0x441450..0x441928.  The Windows SEH registration in
// each wrapper is platform plumbing, not part of the Switch runtime model.
// On the normal path each factory performs exactly two semantic operations:
// allocate the fixed byte count at PC 0x5802CF, then (only on success) invoke
// its fixed thiscall constructor and return that constructor's EAX.  A failed
// allocation returns 0 without invoking the constructor.
using PcFactoryAllocateCallback = std::uint32_t(*)(void* user,std::uint32_t pc_entry,std::uint32_t bytes);
using PcFactoryConstructCallback = std::uint32_t(*)(void* user,std::uint32_t pc_entry,std::uint32_t object_token);
struct PcFactoryServices {
    void* user{};
    PcFactoryAllocateCallback allocate{};
    PcFactoryConstructCallback construct{};
};
std::uint32_t object_factory_441450(const PcFactoryServices& services); // 0x758 -> ctor 0x4C5120
std::uint32_t object_factory_4414b0(const PcFactoryServices& services); // 0x154 -> ctor 0x4C5550
std::uint32_t object_factory_441510(const PcFactoryServices& services); // 0x154 -> ctor 0x4C5980
std::uint32_t object_factory_441570(const PcFactoryServices& services); // 0x14C -> ctor 0x4C5CD0
std::uint32_t object_factory_4415d0(const PcFactoryServices& services); // 0x14C -> ctor 0x4C5EF0
std::uint32_t object_factory_441630(const PcFactoryServices& services); // 0x1F4 -> ctor 0x4C60B0
std::uint32_t object_factory_441690(const PcFactoryServices& services); // 0x2200 -> ctor 0x4C6300
std::uint32_t object_factory_4416f0(const PcFactoryServices& services); // 0x31C8 -> ctor 0x4C6FD0
std::uint32_t object_factory_441750(const PcFactoryServices& services); // 0x1F10 -> ctor 0x4C8030
std::uint32_t object_factory_4417b0(const PcFactoryServices& services); // 0x1674 -> ctor 0x4C8DE0
std::uint32_t object_factory_441810(const PcFactoryServices& services); // 0x474 -> ctor 0x4C9A90
std::uint32_t object_factory_441870(const PcFactoryServices& services); // 0x27F9C -> ctor 0x4CB300
std::uint32_t object_factory_4418d0(const PcFactoryServices& services); // 0x27F9C -> ctor 0x4CCFC0

// r061: continuation of the same regular normal-path factory-wrapper series.
// Exact PC block: 0x441930..0x4429A8.  0x4429B0 changes shape and is
// deliberately outside this batch.  SEH scope boundary is identical to r060.
std::uint32_t object_factory_441930(const PcFactoryServices& services); // 0x279C4 -> ctor 0x4CE170
std::uint32_t object_factory_441990(const PcFactoryServices& services); // 0x27A68 -> ctor 0x4CEBF0
std::uint32_t object_factory_4419f0(const PcFactoryServices& services); // 0x27BA8 -> ctor 0x4D05E0
std::uint32_t object_factory_441a50(const PcFactoryServices& services); // 0x27CE8 -> ctor 0x4D2710
std::uint32_t object_factory_441ab0(const PcFactoryServices& services); // 0x28FC -> ctor 0x4D4230
std::uint32_t object_factory_441b10(const PcFactoryServices& services); // 0x1D90 -> ctor 0x4D7140
std::uint32_t object_factory_441b70(const PcFactoryServices& services); // 0x5EC -> ctor 0x4D9010
std::uint32_t object_factory_441bd0(const PcFactoryServices& services); // 0x27B08 -> ctor 0x4DA2F0
std::uint32_t object_factory_441c30(const PcFactoryServices& services); // 0x275D8 -> ctor 0x4DB680
std::uint32_t object_factory_441c90(const PcFactoryServices& services); // 0x1324 -> ctor 0x4926F0
std::uint32_t object_factory_441cf0(const PcFactoryServices& services); // 0x12B0 -> ctor 0x48C490
std::uint32_t object_factory_441d50(const PcFactoryServices& services); // 0xD8 -> ctor 0x4DC080
std::uint32_t object_factory_441db0(const PcFactoryServices& services); // 0x12E8 -> ctor 0x4DCC00
std::uint32_t object_factory_441e10(const PcFactoryServices& services); // 0x1F4 -> ctor 0x4DD410
std::uint32_t object_factory_441e70(const PcFactoryServices& services); // 0x11A4 -> ctor 0x4DD5C0
std::uint32_t object_factory_441ed0(const PcFactoryServices& services); // 0x12EC -> ctor 0x4DE6F0
std::uint32_t object_factory_441f30(const PcFactoryServices& services); // 0x17C -> ctor 0x4DE9D0
std::uint32_t object_factory_441f90(const PcFactoryServices& services); // 0x17B8 -> ctor 0x4DEC60
std::uint32_t object_factory_441ff0(const PcFactoryServices& services); // 0x271C -> ctor 0x4DF820
std::uint32_t object_factory_442050(const PcFactoryServices& services); // 0x1B68 -> ctor 0x4DFAF0
std::uint32_t object_factory_4420b0(const PcFactoryServices& services); // 0x1444 -> ctor 0x4E0680
std::uint32_t object_factory_442110(const PcFactoryServices& services); // 0xC768 -> ctor 0x4E1890
std::uint32_t object_factory_442170(const PcFactoryServices& services); // 0x448C -> ctor 0x4E3AC0
std::uint32_t object_factory_4421d0(const PcFactoryServices& services); // 0x449C -> ctor 0x4E4C60
std::uint32_t object_factory_442230(const PcFactoryServices& services); // 0x20B0 -> ctor 0x4E5590
std::uint32_t object_factory_442290(const PcFactoryServices& services); // 0x1768 -> ctor 0x4E5720
std::uint32_t object_factory_4422f0(const PcFactoryServices& services); // 0x1DEC -> ctor 0x4E5B20
std::uint32_t object_factory_442350(const PcFactoryServices& services); // 0x1B48 -> ctor 0x4E60C0
std::uint32_t object_factory_4423b0(const PcFactoryServices& services); // 0x26F8 -> ctor 0x4E6220
std::uint32_t object_factory_442410(const PcFactoryServices& services); // 0x22D4 -> ctor 0x493020
std::uint32_t object_factory_442470(const PcFactoryServices& services); // 0x130E8 -> ctor 0x4E7600
std::uint32_t object_factory_4424d0(const PcFactoryServices& services); // 0x88A8 -> ctor 0x4E7A90
std::uint32_t object_factory_442530(const PcFactoryServices& services); // 0x132C -> ctor 0x4E7D60
std::uint32_t object_factory_442590(const PcFactoryServices& services); // 0x674 -> ctor 0x4E8130
std::uint32_t object_factory_4425f0(const PcFactoryServices& services); // 0x4488 -> ctor 0x4E9160
std::uint32_t object_factory_442650(const PcFactoryServices& services); // 0x2408 -> ctor 0x4E9E50
std::uint32_t object_factory_4426b0(const PcFactoryServices& services); // 0x1C4C -> ctor 0x4EB100
std::uint32_t object_factory_442710(const PcFactoryServices& services); // 0x1C10 -> ctor 0x4302C0
std::uint32_t object_factory_442770(const PcFactoryServices& services); // 0x1C10 -> ctor 0x4AFE30
std::uint32_t object_factory_4427d0(const PcFactoryServices& services); // 0x1C14 -> ctor 0x4EB2E0
std::uint32_t object_factory_442830(const PcFactoryServices& services); // 0x1C10 -> ctor 0x4EB5B0
std::uint32_t object_factory_442890(const PcFactoryServices& services); // 0x1C10 -> ctor 0x4EB8D0
std::uint32_t object_factory_4428f0(const PcFactoryServices& services); // 0x1C10 -> ctor 0x4EB9E0
std::uint32_t object_factory_442950(const PcFactoryServices& services); // 0x1C10 -> ctor 0x4EBD10

// r062: first constructor/state-initializer block after the regular factory series.
// The original PC constructors call several still-open child constructors and the
// MSVC vector-constructor helper.  Native code therefore owns only the writes and
// call ordering visible in these parents; every child is an explicit service.
using PcObjectInitThisCallback = void(*)(void* user,std::uint32_t pc_entry,Bytes object,std::size_t object_offset);
using PcObjectInitThisArgCallback = void(*)(void* user,std::uint32_t pc_entry,Bytes object,std::size_t object_offset,std::uint32_t arg0);
using PcObjectInitArrayCallback = void(*)(void* user,std::uint32_t helper_pc,Bytes object,std::size_t object_offset,
                                          std::uint32_t element_size,std::uint32_t count,
                                          std::uint32_t constructor_pc,std::uint32_t destructor_pc);
struct PcObjectInitServices {
    void* user{};
    PcObjectInitThisCallback call_this{};
    PcObjectInitThisArgCallback call_this_arg{};
    PcObjectInitArrayCallback construct_array{};
};

// PC 0x004429B0..0x00442A5D.  Returns self_token exactly as the original returns ESI.
std::uintptr_t object_ctor_4429b0(Bytes object,std::uintptr_t self_token,const PcObjectInitServices& services);
// PC 0x00442A60..0x00442AB6.
std::uintptr_t object_ctor_442a60(Bytes object,std::uintptr_t self_token,const PcObjectInitServices& services);
// PC 0x00442AC0..0x00442B1D.  Initializes the 32 x 0x20 sentinel grid then a
// four-element 0xA0 child array through the original vector-constructor boundary.
std::uintptr_t object_block_init_442ac0(Bytes object,std::uintptr_t self_token,const PcObjectInitServices& services);
// PC 0x00442B20..0x00442C17.  The global byte at PC 0x6319A1 is an explicit input.
std::uintptr_t object_state_ctor_442b20(Bytes object,std::uintptr_t self_token,std::uint8_t mode_global_6319a1,
                                       const PcObjectInitServices& services);

// r063: immediate state-reset and selector block following the r062 constructors.
// 0x4AF500 is a direct float getter used by 0x442C20 and is closed independently.
float timer_value_4af500(float timer_global_842110);

using PcObjectResetFloatCallback = float(*)(void* user,std::uint32_t pc_entry);
struct PcObjectResetServices {
    void* user{};
    PcObjectResetFloatCallback call_float{};
};
// PC 0x00442C20..0x00442CAD.  Returns AL=1.  The 0x4AF500 child is an
// explicit boundary and scale_62812c is the immutable PC constant at 0x62812C.
std::uint8_t object_state_reset_442c20(Bytes object,std::uint8_t mode_global_6319a1,float scale_62812c,
                                      const PcObjectResetServices& services);

struct PcObjectSelectorGlobals {
    std::uint32_t manager_handle{}; // PC global 0x7D68AC
    std::uint8_t manager_flag5{};   // current manager +0x05
    std::uint8_t manager_flag8{};   // current manager +0x08
};
using PcObjectSelectorVirtualU32 = std::uint32_t(*)(void* user,std::uint32_t handle,std::uint32_t vtable_byte_offset);
using PcObjectSelectorVoidHandle = void(*)(void* user,std::uint32_t pc_entry,std::uint32_t handle);
struct PcObjectSelectorServices {
    void* user{};
    PcObjectSelectorVirtualU32 call_virtual_u32{};
    PcObjectSelectorVoidHandle call_void_handle{};
};
// PC 0x00442CB0..0x00442D6B.  Writes selected_kind (-1/0/1/2/3), calls
// virtual slot +0x08 on the chosen handle, and preserves the 0x4EE930 manager
// side effect as an explicit service/global state transition.
std::uint32_t object_select_primary_442cb0(Bytes object,std::int32_t& selected_kind,
                                           PcObjectSelectorGlobals& globals,
                                           const PcObjectSelectorServices& services);
// PC 0x00442D70..0x00442DF0.  Similar selector without the manager tail or
// the indexed +0x484 fallback.
std::uint32_t object_select_secondary_442d70(Bytes object,std::int32_t& selected_kind,
                                             const PcObjectSelectorServices& services);

// PC 0x004DEF50: classify the active SUMO_FE gate's cursor (+0x38) relative
// to its authored item count (+0x34). These are child action classes, not
// mode indices and not the child's state key returned by 0x564C90.
std::uint32_t frontend_gate_choice_class_4def50(std::int32_t item_count,
                                                 std::int32_t cursor);
// 0x4DED80 constructs a list of profile rows plus the four fixed actions.
// 0x4ED250/0x4ED2A0 wrap the cursor and skip disabled rows. The linked PC
// widgets are represented as bounded enabled flags until their renderer lands.
struct PcFrontendGateList {
    std::array<std::uint8_t,13> enabled{};
    std::uint32_t count{};
};
void frontend_gate_list_initialize_4ded80(Bytes child,std::uint32_t profile_count,
                                           PcFrontendGateList& list);
bool frontend_gate_list_move_4ed250_4ed2a0(Bytes child,
                                             const PcFrontendGateList& list,
                                             bool forward);
// FXT 0x4DF120, key-22 child: the source's completed-animation and
// first-level choice paths. input_action is the 0x48F5F0 virtual result;
// UINT32_MAX means no input this frame. The animation completion flag is
// supplied by the caller, never inferred from the action latch itself.
std::uint32_t frontend_gate_control_4df120(Bytes child,
                                           std::uint32_t input_action,
                                           bool animation_ready,
                                           bool owner_busy,
                                           std::uint32_t menu_variant,
                                           const PcFrontendGateList* list=nullptr);

// r064: immediate dispatch/state-table helpers following the r063 selector block.
// Guest child handles remain opaque 32-bit tokens; virtual bodies, the embedded
// 0x446A50 tail routine, and direct child-state reads are explicit services.
using PcObjectStateVirtualVoid = void(*)(void* user,std::uint32_t handle,std::uint32_t vtable_byte_offset);
using PcObjectStateEmbeddedVoid = void(*)(void* user,std::uint32_t pc_entry,Bytes object,std::size_t object_offset);
using PcObjectStateU32Handle = std::uint32_t(*)(void* user,std::uint32_t pc_entry,std::uint32_t handle);
struct PcObjectStateServices {
    void* user{};
    PcObjectStateVirtualVoid call_virtual_void{};
    PcObjectStateEmbeddedVoid call_embedded_void{};
    PcObjectStateU32Handle call_u32{};
};
// PC 0x00442E00..0x00442E6F. Dispatch virtual slot +0x0C in original
// priority/order, then tail-call 0x446A50 with self+0x51C. Source semantics are void.
void object_dispatch_state_442e00(Bytes object,const PcObjectStateServices& services);
// PC 0x00442E70..0x00442EBF. Fill 128 state slots with 0x53, then refresh
// count entries from handles at +0x284 using the already-closed 0x564C90 leaf.
void object_refresh_state_table_442e70(Bytes object,const PcObjectStateServices& services);
// PC 0x00442EC0..0x00442EE0. Return 0x53 unless count>=2; otherwise query
// the handle at +0x27C+count*4 through 0x564C90.
std::uint32_t object_query_previous_state_442ec0(Bytes object,const PcObjectStateServices& services);
// PC 0x00442F20..0x00442F46. Store one byte into each depth-indexed 32-byte array.
void object_store_depth_pair_442f20(Bytes object,std::uint8_t first,std::uint8_t second);
// PC 0x00443040..0x0044305B. Query the last active indexed handle, or 0x53.
std::uint32_t object_query_last_state_443040(Bytes object,const PcObjectStateServices& services);
// PC 0x00443060..0x004430AE. Return AL=1 when the object has active state
// according to the +0x218 mode-specific gate, else AL=0.
std::uint8_t object_has_active_state_443060(Bytes object);

// r065: state-pair lookup/update and flagged-handle release block.  The PC
// key/pair records beginning at 0x6319A8 are immutable data, but are supplied
// explicitly so native code never consults raw PC globals.  The original
// sentinel key 0x53 is not part of entries; missing keys use pair (1,1).
struct PcObjectStatePairEntry {
    std::uint32_t key{};
    std::uint8_t first{};
    std::uint8_t second{};
};
struct PcObjectStatePairTable {
    const PcObjectStatePairEntry* entries{};
    std::size_t count{};
};
constexpr std::size_t PcObjectStatePairCount=48u;
// Portable copy of the 48 pre-sentinel records at PC 0x6319A8.
const std::array<PcObjectStatePairEntry,PcObjectStatePairCount>& pc_object_state_pair_table_r065();
// PC 0x00442F50..0x00442FC2. Store the pair selected by key at the current
// signed depth index.  Key 0x53 and keys absent from the explicit table fall
// back to (1,1), matching the PC sentinel path.
void object_store_state_pair_442f50(Bytes object,std::uint32_t key,const PcObjectStatePairTable& table);

using PcObjectStateEmbeddedU32 = void(*)(void* user,std::uint32_t pc_entry,Bytes object,
                                         std::size_t object_offset,std::uint32_t argument);
struct PcObjectStateUpdateServices {
    void* user{};
    PcObjectStateU32Handle call_u32{};
    PcObjectStateEmbeddedU32 call_embedded_u32{};
};
// PC 0x00442FD0..0x0044303F. Select the current child state using original
// priority, feed it to embedded 0x446EA0(self+0x51C), then update the current
// depth pair through the reconstructed 0x442F50 helper.
void object_update_state_442fd0(Bytes object,const PcObjectStatePairTable& table,
                                const PcObjectStateUpdateServices& services);
// PC 0x004430B0..0x00443104. Release flagged +0x490 then +0x488 handles through
// already-closed 0x441200, preserving exact clear order and leaving handles
// untouched when their corresponding flags are already zero.
void object_release_flagged_handles_4430b0(Bytes object,const PcRuntimeControlServices& services);

// r066: UI/resource state machine and event-id dispatcher immediately following r065.
// Its historical callback interface is retained for call-order/oracle coverage.
// r114 now supplies reusable native 0x465860/0x465970 bodies separately;
// 0x4659F0 was closed in r080. Already-closed 0x465250, 0x4652E0, 0x441370
// and 0x442EC0 are reused directly and are not recounted.
using PcObjectUiOpenVoid = void(*)(void* user,std::uint32_t call_site,std::uint32_t pc_entry,
                                   Bytes object,std::size_t object_offset);
using PcObjectUiOpenConfig = void(*)(void* user,std::uint32_t call_site,std::uint32_t pc_entry,
                                     Bytes object,std::size_t object_offset,
                                     const std::array<std::uint32_t,11>& args);
struct PcObjectUiOpenServices {
    void* user{};
    PcObjectUiOpenVoid call_void{};
    PcObjectUiOpenConfig call_config{};
};
// PC 0x00443110..0x004432A1. Manage the two embedded UI resources at
// +0xBA8/+0xC48 and pending/current ids at +0xCE8/+0xCEC.
void object_ui_state_update_443110(Bytes object,PcUiNotifyGlobals& globals,
                                   const PcUiNotifyServices& ui_services,
                                   const PcObjectUiOpenServices& open_services);
// PC 0x004432B0..0x00443413, including the local jump/classification tables.
// Input 46 recursively redispatches the previous child state from 0x442EC0.
std::uint32_t object_event_dispatch_4432b0(Bytes object,std::uint32_t state,
                                           const PcObjectStateServices& services);

// r067: event callback dispatch immediately following the r066 local classifier.
// The 83-entry PC table at 0x7B11D8 is runtime-initialized elsewhere, so it is
// explicit portable input here rather than a raw PC-global dependency.
struct PcObjectEventCallbackEntry {
    std::uint32_t key{};
    std::uint32_t callback_token{};
};
struct PcObjectEventCallbackTable {
    const PcObjectEventCallbackEntry* entries{};
    std::size_t count{};
};
constexpr std::size_t PcObjectEventCallbackCount=0x53u;
using PcObjectEventEmbeddedVoid = void(*)(void* user,std::uint32_t pc_entry,Bytes object,std::size_t object_offset);
using PcObjectEventEmbeddedU32 = void(*)(void* user,std::uint32_t pc_entry,Bytes object,std::size_t object_offset,std::uint32_t argument);
using PcObjectEventCallback = std::uint32_t(*)(void* user,std::uint32_t callback_token,std::size_t table_index);
struct PcObjectEventDispatchServices {
    void* user{};
    PcObjectEventEmbeddedVoid call_embedded_void{};
    PcObjectEventEmbeddedU32 call_embedded_u32{};
    PcObjectEventCallback call_callback{};
};
// PC 0x00443420..0x004434B2. Search the first 83 callback entries by key.
// On a hit, extend/copy the depth history while signed depth < 32, notify
// embedded +0x51C, update the state pair through 0x442F50, then invoke the
// selected callback. A miss returns zero without touching the object.
std::uint32_t object_dispatch_callback_443420(Bytes object,std::uint32_t key,
                                               const PcObjectStatePairTable& pair_table,
                                               const PcObjectEventCallbackTable& callback_table,
                                               const PcObjectEventDispatchServices& services);
// PC 0x004434C0..0x00443518 and 0x00443520..0x00443578. Normal-path
// allocation wrappers using the established factory service boundary.
std::uint32_t object_factory_4434c0(const PcFactoryServices& services); // 0x2980 -> ctor 0x4429B0
std::uint32_t object_factory_443520(const PcFactoryServices& services); // 0x1320 -> ctor 0x442A60

// r068: two deleting-destructor wrappers and their in-place destructor bodies.
// Child destructors/vector teardown and operator delete remain explicit service
// boundaries; no child body is counted transitively.
using PcObjectDestroyThis = void(*)(void* user,std::uint32_t pc_entry,Bytes object,std::size_t object_offset);
using PcObjectDestroyVector = void(*)(void* user,std::uint32_t pc_entry,Bytes object,std::size_t object_offset,
                                      std::uint32_t element_size,std::uint32_t count,std::uint32_t destructor_pc);
using PcObjectDestroyFree = void(*)(void* user,std::uint32_t pc_entry,std::uint32_t object_token);
struct PcObjectDestroyServices {
    void* user{};
    PcObjectDestroyThis destroy_this{};
    PcObjectDestroyVector destroy_vector{};
    PcObjectDestroyFree release{};
};
// PC 0x00443580..0x0044359B. Call body 0x4435A0, optionally delete self when
// flags&1, and return the original self token.
std::uint32_t object_destroy_443580(Bytes object,std::uint32_t object_token,std::uint8_t flags,
                                    const PcObjectDestroyServices& services);
// PC 0x004435A0..0x00443638. Reverse-destroy the two 4-element arrays and the
// embedded children at +0x1E4/+0x1A8, then the base at +0.
void object_destroy_body_4435a0(Bytes object,const PcObjectDestroyServices& services);
// PC 0x00443640..0x0044365B. Analogous deleting-destructor wrapper for the
// smaller object body at 0x443660.
std::uint32_t object_destroy_443640(Bytes object,std::uint32_t object_token,std::uint8_t flags,
                                    const PcObjectDestroyServices& services);
// PC 0x00443660..0x004436B8. Destroy +0x70, then +0x34, then base +0.
void object_destroy_body_443660(Bytes object,const PcObjectDestroyServices& services);

// r069: runtime callback-table/object initializer. The PC routine writes only
// selected entries of the 83-slot table at 0x7B11D8; untouched entries are
// deliberately preserved, matching the original routine rather than silently
// clearing the whole table. The embedded +0x51C initializer remains an explicit
// boundary; the already-closed 0x465250 resource reset is executed natively.
using PcObjectRuntimeInitEmbedded = void(*)(void* user,std::uint32_t pc_entry,Bytes object,std::size_t object_offset);
struct PcObjectRuntimeInitServices {
    void* user{};
    PcObjectRuntimeInitEmbedded init_embedded{};
    PcUiNotifyServices ui_services{};
};
// PC 0x004436C0..0x00443C26. Initializes the runtime callback table and the
// object state consumed by 0x443420/0x443EB0. global_mode_6319a1 is the byte
// read from PC 0x6319A1. Returns the exact EAX value left by the original.
std::uint32_t object_runtime_init_4436c0(
    Bytes object,
    std::array<PcObjectEventCallbackEntry,PcObjectEventCallbackCount>& callback_table,
    std::uint8_t global_mode_6319a1,
    const PcObjectRuntimeInitServices& services);

// r070: full runtime teardown/reset. The protected transfer at PC 0x443C38
// reaches the linear continuation at 0x443C3E in the original runtime; the
// portable implementation models the owned semantics directly. Handle virtual
// methods and global/event-system calls stay explicit service boundaries.
using PcObjectRuntimeTeardownThis = void(*)(void* user,std::uint32_t pc_entry,Bytes object,std::size_t object_offset);
using PcObjectRuntimeTeardownHandle = std::uint32_t(*)(void* user,std::uint32_t vtable_offset,std::uint32_t handle,std::uint32_t argument);
using PcObjectRuntimeTeardownGlobal = std::uint32_t(*)(void* user,std::uint32_t pc_entry,std::uint32_t argument,bool has_argument);
struct PcObjectRuntimeTeardownServices {
    void* user{};
    PcObjectRuntimeTeardownThis reset_runtime{};
    PcObjectRuntimeTeardownHandle handle_virtual{};
    PcObjectRuntimeTeardownGlobal global_call{};
    PcUiNotifyServices ui_services{};
};
// PC 0x00443C30..0x00443E87. Tears down all active runtime handles, resets
// both embedded UI resources, switches the object to mode 4, and performs the
// final event-system reset calls. Returns the EAX value left by the final
// 0x42DFB0(0x47) boundary. PC 0x443E90 is only a jump thunk and is not counted.
std::uint32_t object_runtime_teardown_443c30(Bytes object,const PcObjectRuntimeTeardownServices& services);

// r071: second callback dispatcher, handle-opening wrapper and compact state-code
// dispatcher.  The 83-slot callback table/state-pair table remain explicit inputs;
// platform/global calls are service boundaries so native code never reads PC globals.
struct PcObjectEventModeInputs {
    std::uint8_t game_flag_780248{};
    std::uint32_t route_state_780258{};
    std::uint32_t fallback_state_78025c{};
    std::uint32_t game_mode_78026c{};
};
using PcObjectEventGlobalCall = void(*)(void* user,std::uint32_t pc_entry,
                                        std::uint32_t argument,bool has_argument);
struct PcObjectEventDispatch443eb0Services {
    void* user{};
    PcObjectEventEmbeddedVoid call_embedded_void{};
    PcObjectEventEmbeddedU32 call_embedded_u32{};
    PcObjectEventCallback call_callback{};
    PcObjectEventGlobalCall call_global{};
};
// PC 0x00443EB0..0x00443F94.  Search the same 83-entry callback table as
// 0x443420, extend history while signed depth<32, update +0x51C/state-pair,
// perform the original mode-3 pause/state side effects, then invoke the callback.
std::uint32_t object_dispatch_callback_443eb0(
    Bytes object,std::uint32_t key,
    const PcObjectStatePairTable& pair_table,
    const PcObjectEventCallbackTable& callback_table,
    const PcObjectEventModeInputs& inputs,
    const PcObjectEventDispatch443eb0Services& services);

using PcObjectHandleReady = std::uint32_t(*)(void* user,std::uint32_t handle,
                                             std::uint32_t vtable_byte_offset);
struct PcObjectOpenCallbackServices {
    void* user{};
    PcObjectHandleReady handle_ready{};
};
// PC 0x00443FA0..0x00443FED.  Close any previously flagged runtime handles,
// dispatch/open the requested callback, store it at +0x488 and keep it only
// when virtual slot +4 reports true; otherwise release it through 0x441200.
void object_open_callback_443fa0(
    Bytes object,std::uint32_t key,
    const PcObjectStatePairTable& pair_table,
    const PcObjectEventCallbackTable& callback_table,
    const PcObjectEventModeInputs& inputs,
    const PcObjectEventDispatch443eb0Services& dispatch_services,
    const PcRuntimeControlServices& runtime_services,
    const PcObjectOpenCallbackServices& open_services);

// PC 0x00443FF0..0x004440EC, including its jump table and 61-byte classifier.
// State 46 redirects through 0x4035F0 and already-closed 0x442EC0; `related`
// is that explicit native object view.  All other classes return the exact
// original constant token or 0xFFFFFFFF.
std::uint32_t object_state_code_443ff0(Bytes object,std::uint32_t state,
                                       Bytes related,
                                       const PcObjectStateServices& related_services);


// r072: callback-list insertion/reopen plus runtime snapshot/commit immediately
// following the r071 classifier.  Guest handles remain opaque 32-bit tokens;
// virtual methods, link/update calls and global platform calls are explicit
// service boundaries.
using PcObjectListGetU32 = std::uint32_t(*)(void* user,std::uint32_t pc_entry,std::uint32_t handle);
using PcObjectListVirtual = std::uint32_t(*)(void* user,std::uint32_t handle,
                                             std::uint32_t vtable_byte_offset,
                                             std::uint32_t argument,bool has_argument);
using PcObjectListLink = void(*)(void* user,std::uint32_t pc_entry,
                                 std::uint32_t existing_handle,std::uint32_t new_handle);
struct PcObjectListServices {
    void* user{};
    PcObjectListGetU32 get_u32{};
    PcObjectListVirtual call_virtual{};
    PcObjectListLink link_handle{};
};
// PC 0x004440F0..0x0044429B, including the four-entry local jump table.
// Reject duplicate/matching handles, classify insertion mode through virtual
// slots +0x18/+0x1C, create through already-closed 0x443420, and reject a
// non-ready new handle through slots +0x10/+0 while popping one history level.
std::uint32_t object_insert_callback_4440f0(
    Bytes object,std::uint32_t key,
    const PcObjectStatePairTable& pair_table,
    const PcObjectEventCallbackTable& callback_table,
    const PcObjectEventDispatchServices& dispatch_services,
    const PcObjectListServices& list_services);

// PC 0x004442A0..0x0044434F including its four-entry jump table.  `unused`
// is the second stack argument consumed by RET 8; the original does not read it.
// Mode 0/1/2 reopens from the indexed/flagged handle key, while mode 3 reopens
// from list[0], releases that old handle through 0x441200 and compacts the list.
std::uint32_t object_reopen_callback_4442a0(
    Bytes object,std::uint32_t mode,std::uint32_t unused,
    const PcObjectStatePairTable& pair_table,
    const PcObjectEventCallbackTable& callback_table,
    const PcObjectEventModeInputs& inputs,
    const PcObjectEventDispatch443eb0Services& dispatch_services,
    const PcRuntimeControlServices& runtime_services,
    const PcObjectOpenCallbackServices& open_services,
    const PcObjectListServices& list_services);

struct PcObjectRuntimeSnapshot444350 {
    std::array<std::uint8_t,0x210> bytes{}; // PC 0x7B15D8..0x7B17E7
    std::uint8_t active{};                 // PC 0x7B17EC, set to 1 only when count>0
};
using PcObjectCommitPrepare = void(*)(void* user,std::uint32_t pc_entry,Bytes object,
                                      std::size_t object_offset);
using PcObjectCommitGlobal = void(*)(void* user,std::uint32_t pc_entry,
                                     std::uint32_t arg0,std::uint32_t arg1,
                                     std::uint32_t argument_count);
struct PcObjectRuntimeCommitServices {
    void* user{};
    PcObjectCommitPrepare prepare{};
    PcObjectCommitGlobal global_call{};
    PcObjectListVirtual call_virtual{};
};
// PC 0x00444350..0x00444459.  Prepare object+4, rebuild the 128-state table,
// copy the exact 0x210-byte snapshot to portable state, apply the mode-specific
// global side effects, release flagged handles, then double-close/delete every
// indexed handle exactly as the original does before clearing +0x484.
void object_runtime_commit_444350(
    Bytes object,PcObjectRuntimeSnapshot444350& snapshot,
    const PcObjectStateServices& state_services,
    const PcRuntimeControlServices& runtime_services,
    const PcObjectRuntimeCommitServices& commit_services);


// r073: runtime commit-mode preparation and two callback-object primitives.
// This closes the unconditional 0x4EEC80 child exposed by r072 together with
// its local dispatch helpers 0x4EEA70/0x4EEAD0/0x4EEB50. Process globals are
// represented explicitly; only the dynamic menu/table queries behind
// 0x495930/0x4958A0 remain service boundaries.
struct PcRuntimePrepareState {
    std::uint32_t primary_mode_78024c{};
    std::uint32_t route_state_780258{};
    std::uint32_t output_code_656234{};
    std::uint8_t output_flag_830395{};
    std::uint32_t alternate_code_836174{};
    std::uint8_t selection_active_836374{};
    std::uint8_t game_flag_780260{};
    std::array<std::uint32_t,5> event_config_7f94c4{};
};
using PcRuntimePrepareCount = std::uint32_t(*)(void* user,std::uint32_t category);
using PcRuntimePrepareSelect = void(*)(void* user,std::uint32_t category,std::uint32_t index);
struct PcRuntimePrepareServices {
    void* user{};
    PcRuntimePrepareCount count_entries{};   // open 0x495930 boundary
    PcRuntimePrepareSelect select_entry{};   // open 0x4958A0 boundary
};
// PC 0x004EEA70..0x004EEAC7. Decode 2-bit primary mode through [1,0,3,2].
bool runtime_primary_mode_4eea70(std::uint32_t selector,PcRuntimePrepareState& state);
// PC 0x004EEAD0..0x004EEB4F. Decode route selector 0..6; selector 0 is a
// successful no-op and selectors >6 fail without changing state.
bool runtime_route_mode_4eead0(std::uint32_t selector,PcRuntimePrepareState& state);
// PC 0x004EEB50..0x004EEC57 including the seven-entry local jump table.
bool runtime_route_config_4eeb50(std::uint32_t selector,std::uint32_t arg2,std::uint32_t arg3,
                                 PcRuntimePrepareState& state,
                                 const PcRuntimePrepareServices& services);
// PC 0x004EEC80..0x004EEE4A. Decode the packed +0x208 runtime word and apply
// the exact global/output side effects. `runtime` is the object+4 view used by
// 0x444350, therefore +0x208/+0x20C are relative to that embedded view.
void runtime_prepare_4eec80(Bytes runtime,PcRuntimePrepareState& state,
                            const PcRuntimePrepareServices& services);

// PC 0x0048F4E0..0x0048F4E3: callback object key getter.
std::uint32_t callback_key_48f4e0(Bytes callback_object);
// PC 0x0048D870..0x0048D892: append a non-null handle at +0x4B0[count] and
// increment +0x4D0. Returns false for a null incoming handle.
bool callback_append_48d870(Bytes callback_object,std::uint32_t new_handle);


// r074: native category-table services and boot/runtime gate closure.
// The PC 0x836370 category table and 0x836350 record container are represented
// as ordinary native arrays; no PC pointers survive in the portable API.
struct PcRuntimeCategoryRecord495930 {
    std::uint32_t key{};
    std::array<std::uint8_t,0x40> tail{}; // original record stride is 0x44
};
struct PcRuntimeCategoryState {
    std::uint32_t mode_836358{};
    const std::uint32_t* category_keys{};
    std::size_t category_count{};
    const PcRuntimeCategoryRecord495930* records{};
    std::size_t record_count{};
    std::uint32_t selected_key_67e6a4{};
    std::uint32_t selected_index_67e6a8{};
};
// PC 0x004958A0..0x004958BC. Select category key and 1-based entry index.
void runtime_select_entry_4958a0(PcRuntimeCategoryState& state,
                                  std::uint32_t category,std::uint32_t index);
// PC 0x00495930..0x00495985. Return number of records whose first dword
// equals the key selected by category. Mode must be exactly 3.
std::uint32_t runtime_count_entries_495930(const PcRuntimeCategoryState& state,
                                           std::uint32_t category);
// Adapters usable directly by PcRuntimePrepareServices.
std::uint32_t runtime_count_entries_495930_service(void* user,std::uint32_t category);
void runtime_select_entry_4958a0_service(void* user,std::uint32_t category,std::uint32_t index);

// Tiny mandatory global leaves exposed by the runtime/commit path.
void runtime_game_flag_43f870(PcRuntimePrepareState& state,std::uint8_t value);
void runtime_selection_disable_4957e0(PcRuntimePrepareState& state);
std::uint32_t runtime_external_block_4872e0(std::uint32_t value_82e7d8);
bool runtime_system_handle_active_4999c0(std::uint32_t handle_67f614);
std::uint32_t runtime_menu_state_450240(std::uint32_t state_7d38f0);
std::int32_t runtime_current_player_47f110();
std::uint32_t runtime_feature_mask_4536f0(std::uint32_t player0_mask,std::uint32_t requested_mask);

// Minimal token->native-object binding used to replace guest-pointer casts.
// It is deliberately simple: ownership stays with the caller; this registry
// only resolves stable 32-bit guest-style tokens to bounded native byte views.
struct PcNativeHandleBinding {
    std::uint32_t token{};
    void* object{};
    std::size_t size{};
};
struct PcNativeHandleResolver {
    const PcNativeHandleBinding* bindings{};
    std::size_t count{};
};
const PcNativeHandleBinding* native_handle_find(const PcNativeHandleResolver& resolver,
                                                std::uint32_t token);
std::uint32_t native_handle_state_564c90(const PcNativeHandleResolver& resolver,
                                         std::uint32_t token);
std::uint32_t native_handle_key_48f4e0(const PcNativeHandleResolver& resolver,
                                       std::uint32_t token);

struct PcRuntimeGate444470Inputs {
    std::uint32_t game_mode_78026c{};
    std::uint32_t external_block_82e7d8{};
    std::uint32_t system_handle_67f614{0xffffffffu};
    std::uint32_t menu_state_7d38f0{};
    std::uint32_t player0_feature_mask{};
};
using PcRuntimeGateConfigure = void(*)(void* user,Bytes embedded,
                                       std::uint32_t selector,std::uint8_t flag);
struct PcRuntimeGate444470Services {
    void* user{};
    PcRuntimeGateConfigure configure_446f30{}; // remaining embedded-config boundary
};
// PC 0x00444470..0x0044452F. All scalar/global gates and handle-state reads
// are native. `current_object` is the explicit result of the PC lazy singleton
// 0x4035F0; passing a native view avoids retaining that guest allocation path.
void object_runtime_gate_444470(
    Bytes object,Bytes current_object,
    const PcNativeHandleResolver& handles,
    const PcRuntimeGate444470Inputs& gate_inputs,
    const PcObjectStatePairTable& pair_table,
    const PcObjectEventCallbackTable& callback_table,
    const PcObjectEventModeInputs& event_inputs,
    const PcObjectEventDispatch443eb0Services& dispatch_services,
    const PcRuntimeControlServices& runtime_services,
    const PcObjectOpenCallbackServices& open_services,
    const PcRuntimeGate444470Services& gate_services);


// r075: close the two highest runtime boundaries exposed by r072/r074.
// 0x44DA00 selects one of the immutable course-data names, formats the
// "_course" companion name, then delegates the actual data application to
// 0x44D720. The loader body remains a separate explicit boundary.
struct PcCourseLoadState44da00 {
    std::uint32_t force_sync_7d2d8c{};
};
using PcCourseLoadApply44d720 = std::uint32_t(*)(void* user,const char* data_name,
                                                 const char* course_name,std::uint8_t alternate);
struct PcCourseLoadServices44da00 {
    void* user{};
    PcCourseLoadApply44d720 apply_44d720{};
};
bool runtime_course_load_44da00(std::uint32_t mode,std::uint8_t alternate,
                                 PcCourseLoadState44da00& state,
                                 const PcCourseLoadServices44da00& services);

// 0x446BB0 refreshes one of four embedded 0xA0-byte resources. The resource
// reset leaf 0x465250 is already native; only the larger UI configure/finalize
// children stay explicit. Immutable PC lookup/coordinate tables are embedded
// as portable constants.
using PcEmbeddedUiConfigure446bb0 = void(*)(void* user,Bytes child,std::uint32_t effect_id,
                                            std::uint32_t slot,std::uint32_t x_bits,
                                            std::uint32_t y_bits);
using PcEmbeddedUiFinalize446bb0 = void(*)(void* user,Bytes child);
using PcEmbeddedGlobalEffect4249f0 = void(*)(void* user,std::uint32_t value);
struct PcEmbeddedConfigServices446f30 {
    void* user{};
    PcUiNotifyServices ui_reset_services{};
    PcEmbeddedUiConfigure446bb0 configure_ui{};
    PcEmbeddedUiFinalize446bb0 finalize_ui{};
    PcEmbeddedGlobalEffect4249f0 global_effect{};
};
void embedded_slot_refresh_446bb0(Bytes embedded,std::uint32_t slot,
                                  const PcEmbeddedConfigServices446f30& services);
bool embedded_configure_446f30(Bytes embedded,std::uint32_t selector,std::int8_t flag,
                               const PcEmbeddedConfigServices446f30& services);

// Mutable native token registry. This is integration infrastructure, not an
// additional counted PC routine. Object storage remains caller-owned, while
// registration/unregistration makes token lifetime deterministic.
struct PcNativeHandleRegistry {
    PcNativeHandleBinding* bindings{};
    std::size_t capacity{};
    std::size_t count{};
    std::uint32_t next_token{1u};
};
PcNativeHandleResolver native_handle_resolver(const PcNativeHandleRegistry& registry);
std::uint32_t native_handle_register(PcNativeHandleRegistry& registry,void* object,
                                     std::size_t size,std::uint32_t preferred_token=0u);
bool native_handle_unregister(PcNativeHandleRegistry& registry,std::uint32_t token);

// r076: native reconstruction of the course runtime builder reached by r075's
// 0x44DA00. Platform file/category acquisition remains explicit, while record
// resolution, shuffle, tree annotation, selected snapshots and matrix rebuild
// are native. Opaque 32-bit descriptor tokens deliberately remain tokens on
// ARM64; no serialized PC pointer is cast to a host pointer.
constexpr std::size_t PcCourseRecord44d720Size=0x78u;
struct PcCourseDescriptor44d720 {
    std::uint32_t token{};
    void* data{};
    std::size_t size{};
};
struct PcCourseDescriptorTables44d720 {
    const PcCourseDescriptor44d720* primary{}; // PC table 0x6A54E0
    std::size_t primary_count{};
    const std::uint32_t* secondary_tokens{};   // PC table 0x6A55E8
    std::size_t secondary_count{};
};
struct PcCourseRecordSource44d720 {
    void* records{};
    std::size_t bytes{};
    std::int32_t count{};
};
struct PcCourseLoaderState44d720 {
    bool present{};
    std::int32_t phase{}; // PC loader +0x08; >=3 skips a fresh load.
};
struct PcCourseApplyRequest44d720 {
    const char* data_name{};
    const char* category_name{};
    PcCourseLoaderState44d720 loader{};
    std::uint32_t selected_key{};
    bool force_selected{};
    std::uint32_t loader_mode{};
    bool shuffle_groups{};
    void* direct_records{}; // PC arg8; when non-null, lookup is bypassed.
    std::size_t direct_bytes{};
    std::int32_t direct_count{}; // PC arg9
    std::uint32_t required{};    // PC arg10 forwarded to the loader.
};
struct PcCourseRuntimeState44d720 {
    std::int32_t stage_key_635f2c{-1};
    std::int32_t stage_value_635f30{-1};
    std::uint32_t resource_type_635f34{0x42u};
    std::uint32_t resource_source_635f38{};
    std::uint32_t fallback_gate_7d33b0{};
    std::uint32_t source_7d33f8{};
    std::uint32_t type_7d33f4{};
    std::uint32_t fixed_7d33e8{};
    std::int32_t fallback_left_7d3404{};
    std::int32_t fallback_right_7d3408{};
    std::uint32_t reset_7d3448{};
    std::uint32_t reset_7d3444{};
    std::uint32_t reset_7d343c{};
    std::uint32_t reset_7d3440{};
    std::uint32_t reset_7d33dc{};
    std::int32_t active_count_7d33c4{};
    std::int32_t max_depth_7d33c0{};
    std::uint32_t force_mode_7f95a8{};
    std::uint32_t goal_word_7f95ac{};   // 46C400 (451514 goal side, 4869DF goal camera) / read by 46C3F0
    std::uint32_t goal_word_7f95b0{};   // 46C410 (4514E0), never read
    std::int32_t selected_index{}; // native replacement for 0x7D33BC arithmetic.
    // 0x7D33BC itself: the record table 44D720 walked (mission direct records
    // or the 44DA00 file records inside the course work). Not owned; null
    // with records_fallback when it is the 7D33D8 fallback record.
    std::uint8_t* records_7d33bc{};
    std::size_t records_bytes{};
    bool records_fallback{};
    bool selected_copy_active{};   // native replacement for 0x7D3188 == 0x7D30A8.
    std::array<std::uint8_t,PcCourseRecord44d720Size> fallback_record_7d33d8{};
    std::array<std::uint8_t,PcCourseRecord44d720Size> selected_7d30a8{};
    std::array<std::uint8_t,PcCourseRecord44d720Size> selected_7d2de0{};
    std::array<std::uint8_t,64> matrix_7d2da0{};
    std::array<std::uint8_t,64> matrix_7d3130{};
    std::array<std::uint8_t,64> matrix_7d3190{};
    std::array<float,3> zero_7d3124{};
    std::array<float,3> zero_7d3178{};
};
using PcCourseBinaryLoad44d720 = bool(*)(void* user,const char* path,
                                         std::uint32_t mode,std::uint32_t required,
                                         PcCourseRuntimeState44d720& state);
using PcCourseRecordLookup44d720 = PcCourseRecordSource44d720(*)(void* user,
                                                                  const char* category_name);
using PcCourseRandom44bf30 = std::int32_t(*)(void* user);
struct PcCourseRuntimeServices44d720 {
    void* user{};
    PcCourseBinaryLoad44d720 load_binary{};
    PcCourseRecordLookup44d720 lookup_records{};
    PcCourseRandom44bf30 random_value{};
};
// PC 0x0044BF30. Only the +0x1C/+0x20 fields are shuffled, exactly as on PC.
void runtime_course_shuffle_44bf30(Bytes records,std::int32_t count,
                                   void* user,PcCourseRandom44bf30 random_value);
// PC 0x0044C850. Child indices are relative to the supplied root record, which
// mirrors the original 0x7D33BC base contract.
void runtime_course_tree_44c850(Bytes records,std::int32_t count,std::int32_t root_index,
                                std::int32_t depth,std::int32_t branch,std::int32_t& max_depth);
// PC 0x0044C0D0. descriptor.token==0 reproduces the original null fast path.
void runtime_course_matrix_44c0d0(const PcCourseDescriptor44d720& descriptor,
                                  PcMatrixStack& matrices,PcCourseRuntimeState44d720& state);
// PC 0x0046C360 tiny global setter used by the forced-selection branch.
void runtime_course_force_mode_46c360(PcCourseRuntimeState44d720& state,std::uint32_t value);
// PC 0x0044D720. Returns the original boolean/EAX contract. The load/lookup
// services are data-source boundaries only; the runtime transformation itself is native.
bool runtime_apply_course_data_44d720(const PcCourseApplyRequest44d720& request,
                                      const PcCourseDescriptorTables44d720& tables,
                                      PcCourseRuntimeState44d720& state,
                                      PcMatrixStack& matrices,
                                      const PcCourseRuntimeServices44d720& services);


// r077: native view of the relocatable binary container consumed by 0x4F12A0
// and exact fast-path category accessors 0x4F1260/0x4F1A90/0x4F1BA0.
// The file keeps 32-bit offsets on disk. Native code validates and resolves
// offsets relative to the caller-owned byte buffer; it never writes host
// pointers back into serialized game data.
struct PcRelocCategoryBlobR077 {
    void* data{};
    std::size_t size{};
    std::uint32_t category_count{};
    std::uint32_t relocation_count{};
    std::size_t index_offset{};
};
// Validate the PC container header/relocation table and expose the category
// index that 0x4F12A0 publishes after reaching loader phase 3.
bool course_reloc_blob_open_r077(void* data,std::size_t size,PcRelocCategoryBlobR077& out);
// PC 0x004F1260: case-insensitive 0x83 rolling hash used by the category index.
std::uint32_t runtime_category_hash_4f1260(const char* name);
// PC 0x004F1A90 / 0x004F1BA0 fast path, expressed against the validated native
// blob view rather than the original loader's two guest pointers.
void* runtime_category_records_4f1a90(const char* category_name,const PcRelocCategoryBlobR077& blob);
std::int32_t runtime_category_count_4f1ba0(const char* category_name,const PcRelocCategoryBlobR077& blob);

// Concrete, caller-owned course provider. Storage is supplied by the port so
// the core does not impose heap or filesystem policy. The read callback receives
// the original loader mode/required flags and writes an already-decoded binary
// container into storage.
using PcCourseProviderReadR077 = bool(*)(void* user,const char* path,
                                         std::uint32_t mode,std::uint32_t required,
                                         void* destination,std::size_t capacity,
                                         std::size_t& bytes_read);
struct PcCourseProviderR077 {
    void* storage{};
    std::size_t capacity{};
    std::size_t size{};
    PcRelocCategoryBlobR077 blob{};
    void* read_user{};
    PcCourseProviderReadR077 read_file{};
    bool loaded{};
};
bool course_provider_load_binary_r077(void* user,const char* path,
                                      std::uint32_t mode,std::uint32_t required,
                                      PcCourseRuntimeState44d720& state);
PcCourseRecordSource44d720 course_provider_lookup_records_r077(void* user,const char* category_name);
PcCourseRuntimeServices44d720 course_provider_services_r077(PcCourseProviderR077& provider,
                                                             PcCourseRandom44bf30 random_value=nullptr);

// Host/libnx-compatible stdio reader for already-decoded game files. Backslash
// guest paths are normalized to '/', then appended below `root`. A custom reader
// can be supplied when loader mode != 0 needs platform-specific decoding.
struct PcCourseStdioRootR077 { const char* root{}; };
bool course_provider_stdio_read_r077(void* user,const char* path,
                                     std::uint32_t mode,std::uint32_t required,
                                     void* destination,std::size_t capacity,
                                     std::size_t& bytes_read);

// Exact integration adapter for the r075 0x44DA00 -> r076 0x44D720 path.
// `alternate` becomes loader_mode 1, all other fixed arguments match the two
// original 44DA00 call sites, and a successful load advances the native loader
// to phase 3 for subsequent calls.
struct PcCourseRuntimeBridgeR077 {
    PcCourseLoaderState44d720 loader{true,1};
    PcCourseDescriptorTables44d720 tables{};
    PcCourseRuntimeState44d720* state{};
    PcMatrixStack* matrices{};
    PcCourseRuntimeServices44d720 services{};
};
std::uint32_t runtime_course_apply_bridge_r077(void* user,const char* data_name,
                                               const char* course_name,std::uint8_t alternate);
PcCourseLoadServices44da00 runtime_course_load_services_r077(PcCourseRuntimeBridgeR077& bridge);


// r078: portable descriptor metadata extracted from the user's pinned PC EXE.
// The game stores 66 primary pointers at 0x6A54E0 and 77 secondary tokens at
// 0x6A55E8. The source tree never embeds those commercial bytes; a host tool
// emits a compact ORC78TBL pack which this parser expands into bounded native
// descriptor storage expected by the already-native r076 course builder.
constexpr std::size_t PcCoursePrimaryDescriptorCountR078=66u;
constexpr std::size_t PcCourseSecondaryDescriptorCountR078=77u;
constexpr std::size_t PcCourseDescriptorBytesR078=0x98u;
struct PcCourseDescriptorPackR078 {
    std::array<std::array<std::uint8_t,PcCourseDescriptorBytesR078>,PcCoursePrimaryDescriptorCountR078> storage{};
    std::array<PcCourseDescriptor44d720,PcCoursePrimaryDescriptorCountR078> primary{};
    std::array<std::uint32_t,PcCourseSecondaryDescriptorCountR078> secondary_tokens{};
    std::size_t primary_count{};
    std::size_t secondary_count{};
    bool world_resource_fields_present{};
};
bool course_descriptor_pack_open_r078(void* data,std::size_t size,PcCourseDescriptorPackR078& out);
PcCourseDescriptorTables44d720 course_descriptor_tables_r078(const PcCourseDescriptorPackR078& pack);

// Concrete owner bridge for PC 0x444350 mode-specific side effects. This binds
// the already-native 0x4EEC80 prepare path, 0x43F870 / 0x4957E0 globals and
// r075 0x44DA00 course load. Virtual handle calls remain caller-owned.
struct PcRuntimeCommitBridgeR078 {
    PcRuntimePrepareState* prepare_state{};
    PcRuntimePrepareServices prepare_services{};
    PcCourseLoadState44da00* course_load_state{};
    PcCourseLoadServices44da00 course_services{};
    void* virtual_user{};
    PcObjectListVirtual virtual_call{};
};
void runtime_commit_prepare_bridge_r078(void* user,std::uint32_t pc_entry,Bytes object,
                                        std::size_t object_offset);
void runtime_commit_global_bridge_r078(void* user,std::uint32_t pc_entry,
                                       std::uint32_t arg0,std::uint32_t arg1,
                                       std::uint32_t argument_count);
std::uint32_t runtime_commit_virtual_bridge_r078(void* user,std::uint32_t handle,
                                                  std::uint32_t slot,std::uint32_t arg,
                                                  bool has_arg);
PcObjectRuntimeCommitServices runtime_commit_services_r078(PcRuntimeCommitBridgeR078& bridge);

// r079: next concrete startup/update owner above 0x444350.  The tiny dispatchers
// 0x445430/0x4454B0 are reconstructed directly; their still-open action bodies
// stay explicit callbacks.  The owner 0x445BE0 reuses the already-native
// selectors, commit parent and flagged-handle release.  The remaining prepare/
// UI/tick children are explicit thiscall boundaries, preserving exact order and
// embedded-object offsets without pretending those bodies are already closed.
using PcRuntimeTransitionActionR079 = void(*)(void* user,std::uint32_t pc_entry,
                                               Bytes object,std::int32_t selected_kind,
                                               std::uint32_t variant);
struct PcRuntimeTransitionServicesR079 {
    void* user{};
    PcRuntimeTransitionActionR079 call_action{};
};
// PC 0x00445430..0x004454AB. Returns true only for result code 5. Codes
// 1/2/3/4/6 dispatch to 0x4450A0/0x445160/0x4448C0/0x4442A0/0x445310
// respectively with variant 0; other codes are no-ops returning false.
bool object_runtime_dispatch_primary_445430(Bytes object,std::uint32_t result,
                                            std::int32_t selected_kind,
                                            const PcRuntimeTransitionServicesR079& services);
// PC 0x004454B0..0x004454F3. Returns true only for result code 5. Codes
// 1/2/3 share 0x445160(selected,1), code 4 uses 0x4442A0(selected,1).
bool object_runtime_dispatch_secondary_4454b0(Bytes object,std::uint32_t result,
                                              std::int32_t selected_kind,
                                              const PcRuntimeTransitionServicesR079& services);

// r106: first closed slice of the multi-stage loader 0x445500.  Stage 3 calls
// the complete 0x48BF20 initializer on the PC global at 0x83039C, then advances
// the owner object to stage 4.  The initializer's two calls into the still-open
// global resource table routine 0x448AD0 remain explicit semantic boundaries.
constexpr std::size_t PcRuntimeLoaderState48bf20Size = 0x34u;
using PcRuntimeLoaderRequestR106 = void(*)(void* user,std::uint32_t pc_entry,
                                           std::uint32_t resource_id,
                                           std::uint32_t request_mode);
struct PcRuntimeLoaderServices48bf20 {
    void* user{};
    PcRuntimeLoaderRequestR106 request{};
};
// PC 0x0048BF20..0x0048BF74.
void runtime_loader_begin_48bf20(Bytes loader,
                                 const PcRuntimeLoaderServices48bf20& services);
// PC 0x004455DF..0x004455F1, reached only when the 0x445500 preamble selected
// stage 3 and object+0x518 is non-positive. Returns true when the slice ran.
bool object_runtime_loader_stage3_4455df(
    Bytes object,Bytes loader,const PcRuntimeLoaderServices48bf20& services);

// r107: projected fields of one 0x48-byte entry from the 0x7C2800 table used by
// complete PC request routine 0x448AD0.  Only the three fields touched/read by
// that routine are represented; the native payload owner stays platform-side.
struct PcRuntimeResourceEntry448ad0 {
    std::uint32_t ownership_10{};
    std::uint32_t status_14{};
    std::uint32_t request_mode_30{};
};
// PC 0x00448AD0..0x00448B0F. Returns true only when the original mutation ran.
bool runtime_resource_request_448ad0(PcRuntimeResourceEntry448ad0& entry,
                                     std::uint32_t resource_id,
                                     std::uint32_t request_mode,
                                     std::uint32_t& pending_7cc1d8);
// Complete PC 0x00448980..0x0044898D.
bool runtime_resource_ready_448980(std::uint32_t pending_7cc1d8);
// Exact top-loader stage-4 gate 0x004455F2..0x00445604. The following
// 0x49E580 state machine is exposed separately so callers preserve fallthrough.
bool object_runtime_loader_stage4_4455f2(Bytes object,
                                         std::uint32_t pending_7cc1d8);

// Complete seven-state PC loader 0x0049E580..0x0049E67B. Larger subsystem and
// resource-manager children remain explicit call tokens with original args.
struct PcRuntimeSharedLoaderState49e580 {
    std::uint32_t stage_83db18{};
};
using PcRuntimeSharedLoaderCallR107 = std::uint32_t(*)(
    void* user,std::uint32_t pc_entry,const std::uint32_t* args,std::size_t arg_count);
struct PcRuntimeSharedLoaderServices49e580 {
    void* user{};
    PcRuntimeSharedLoaderCallR107 call{};
};
std::uint8_t runtime_shared_loader_49e580(
    PcRuntimeSharedLoaderState49e580& state,
    const PcRuntimeSharedLoaderServices49e580& services);

// r110: exact top-loader continuation 0x445627..0x4456E9. Stage 6 completes
// resource 0x44/9 and its optional select table, stage 7 waits on the large
// front-end loader, and ready stages 8..11 fall through to stage 12.
using PcRuntimeTopLoaderCallR110 = std::uint32_t(*)(
    void* user,std::uint32_t pc_entry,const std::uint32_t* args,std::size_t arg_count);
struct PcRuntimeTopLoaderServicesR110 {
    void* user{};
    PcRuntimeTopLoaderCallR110 call{};
};
bool object_runtime_loader_stages6_to11_445627(
    Bytes object,const PcRuntimeTopLoaderServicesR110& services);

// r111: complete frontend request initializer 0x4E85E0 and its 64-lane
// loader 0x4E8620. The state retains the PC layout's four special records and
// 62 formatted-script records; only the first 60 formatted records are used by
// 0x4E8620. PC addresses passed to services remain semantic tokens.
struct PcFrontendBulkLoaderState4e8620 {
    std::array<std::uint32_t,4> special_status{};
    std::array<std::uint32_t,62> script_status{};
    std::array<std::uint32_t,4> special_handles{};
};
using PcFrontendBulkLoaderCallR111 = std::uint32_t(*)(
    void* user,std::uint32_t pc_entry,const std::uint32_t* args,std::size_t arg_count);
struct PcFrontendBulkLoaderServicesR111 {
    void* user{};
    PcFrontendBulkLoaderCallR111 call{};
};
void frontend_bulk_loader_initialize_4e85e0(
    PcFrontendBulkLoaderState4e8620& state);
bool frontend_bulk_loader_4e8620(
    PcFrontendBulkLoaderState4e8620& state,
    const PcFrontendBulkLoaderServicesR111& services);

// r112: exact terminal body of top loader 0x445500, PC
// 0x004456EA..0x004457D3. Opaque child objects and platform/global services
// remain explicit boundaries, while the parent-owned list insertion, float
// transition initialization and stage 12 -> 13 -> 14 mutations are native.
struct PcRuntimeTopLoaderStage12StateR112 {
    std::uint8_t mode_6319a1{};
    std::uint8_t alternate_7b17ec{};
    std::uint32_t optional_resource_631b38{};
    std::int32_t optional_index_7b17f8{-1};
    std::uint8_t optional_flags_7c27d4{};
    PcFloatTransitionGlobals transition_globals{};
};
using PcRuntimeTopLoaderGlobalCallR112 = std::uint32_t(*)(
    void* user,std::uint32_t pc_entry,const std::uint32_t* args,std::size_t arg_count);
using PcRuntimeTopLoaderThisCallR112 = std::uint32_t(*)(
    void* user,std::uint32_t pc_entry,Bytes object,std::size_t object_offset,
    const std::uint32_t* args,std::size_t arg_count);
using PcRuntimeTopLoaderHandleVirtualR112 = std::uint32_t(*)(
    void* user,std::uint32_t handle,std::uint32_t vtable_byte_offset);
struct PcRuntimeTopLoaderStage12ServicesR112 {
    void* user{};
    PcRuntimeTopLoaderGlobalCallR112 global_call{};
    PcRuntimeTopLoaderThisCallR112 this_call{};
    PcRuntimeTopLoaderHandleVirtualR112 handle_virtual{};
};
bool object_runtime_loader_stage12_4456ea(
    Bytes object,PcRuntimeTopLoaderStage12StateR112& state,
    const PcRuntimeTopLoaderStage12ServicesR112& services);

using PcRuntimeOwnerThisCallR079 = void(*)(void* user,std::uint32_t pc_entry,
                                            Bytes object,std::size_t object_offset);
struct PcRuntimeOwnerInputs445be0 {
    float timer_value_842110{}; // return value of already-closed 0x4AF500
    float scale_62812c{};       // immutable PC scale read by the owner
};
struct PcRuntimeOwnerServices445be0 {
    void* user{};
    PcRuntimeOwnerThisCallR079 call_this{};
    PcObjectSelectorServices selector_services{};
    PcRuntimeTransitionServicesR079 transition_services{};
    PcObjectStateServices state_services{};
    PcRuntimeControlServices runtime_services{};
    PcObjectRuntimeCommitServices commit_services{};
};
// PC 0x00445BE0..0x00445D27 including the five-entry state jump table.  Return
// value is the original EAX boolean: 0 only when state 2 reaches 0x444350 or
// state 3 reaches the final 0x4430B0 transition, otherwise 1.
std::uint8_t object_runtime_owner_445be0(Bytes object,
                                         PcObjectRuntimeSnapshot444350& snapshot,
                                         PcObjectSelectorGlobals& selector_globals,
                                         const PcRuntimeOwnerInputs445be0& inputs,
                                         const PcRuntimeOwnerServices445be0& services);

// r080: first mandatory state-2 child below r079's 0x445BE0 owner.
// 0x4659F0 advances the embedded resource controller at object+0xCF4.  The
// three larger animation/draw bodies and the final tail-call remain explicit
// service boundaries; already-closed 0x4652E0/0x465250 are called natively.
using PcUiResourceTickCallR080 = void(*)(void* user,std::uint32_t pc_entry,Bytes resource);
struct PcUiResourceTickServices4659f0 {
    void* user{};
    PcUiResourceTickCallR080 call{};
};
// PC 0x004659F0..0x00465A9A.
void ui_resource_tick_4659f0(Bytes resource,
                             const PcUiNotifyServices& ui_services,
                             const PcUiResourceTickServices4659f0& services);

// 0x444840 always ticks object+0xCF4.  If +0xCFC is -1 and +0xD94 is set,
// it resolves object[0x280 + object[0x484]*4] through already-closed 0x564C90
// and forwards that state value to 0x4447D0. r114 connects its known valid
// state path while retaining this callback seam for historical oracle tests.
using PcRuntimeUiOpenR080 = void(*)(void* user,std::uint32_t pc_entry,
                                    Bytes object,std::uint32_t key);
struct PcRuntimeUiTickServices444840 {
    void* user{};
    PcRuntimeUiOpenR080 open_state{};
};
// PC 0x00444840..0x0044487C.
void object_runtime_ui_tick_444840(Bytes object,
                                   const PcNativeHandleResolver& handles,
                                   const PcUiNotifyServices& ui_services,
                                   const PcUiResourceTickServices4659f0& tick_services,
                                   const PcRuntimeUiTickServices444840& services);

// r114: the ordinary, unprotected UI-resource path reached by 0x4447D0.
// 0x465860 receives eleven raw 32-bit stack arguments; keeping their bit
// patterns intact is important for the five float fields.  The larger matrix
// application tail at 0x4656B0 remains an explicit service boundary.
using PcUiResourceCallR114 = std::uint32_t(*)(void* user,std::uint32_t pc_entry,
                                              const std::uint32_t* args,std::size_t count);
using PcUiResourceFinalizeR114 = void(*)(void* user,std::uint32_t pc_entry,Bytes resource);
struct PcUiResourceCommitServices465970 {
    void* user{};
    PcUiResourceCallR114 call{};
    PcUiResourceFinalizeR114 finalize{};
};
void ui_resource_configure_465860(Bytes resource,
                                  const std::array<std::uint32_t,11>& args,
                                  const PcUiNotifyServices& ui_services);
void ui_resource_commit_465970(Bytes resource,
                               const PcUiResourceCommitServices465970& services);

// The state-code -1 route in 0x4447D0 jumps through protected pointer cell
// 0x01039CC4.  Valid state codes are reconstructed here; the protected route
// is deliberately reported through `invalid_tail` rather than invented.
using PcRuntimeUiInvalidR114 = void(*)(void* user,std::uint32_t pc_entry,
                                       Bytes object,std::uint32_t key);
struct PcRuntimeUiOpenServices4447d0 {
    PcUiNotifyServices ui_services{};
    PcUiResourceCommitServices465970 commit_services{};
    void* invalid_user{};
    PcRuntimeUiInvalidR114 invalid_tail{};
};
bool object_runtime_ui_open_known_4447d0(
    Bytes object,std::uint32_t key,Bytes related,
    const PcObjectStateServices& related_services,
    const PcRuntimeUiOpenServices4447d0& services);

// r081: second mandatory state-2 child below r079's 0x445BE0 owner.
// 0x446A80/0x446B20 configure one of four embedded 0xA0-byte UI resources,
// while 0x446CF0 ticks all four resources, normalizes the 0x297/0x298 mode
// marker and dispatches to the appropriate configurator. Their historical
// service seams remain available; r114 supplies native reusable 0x465860 and
// 0x465970 bodies for concrete callers.
using PcEmbeddedUiConfigureR081 = void(*)(void* user,std::uint32_t pc_entry,
                                          Bytes child,std::uint32_t effect_id,
                                          std::uint32_t slot,std::uint32_t x_bits,
                                          std::uint32_t y_bits);
using PcEmbeddedUiFinalizeR081 = void(*)(void* user,std::uint32_t pc_entry,Bytes child);
struct PcEmbeddedSlotsServices446cf0 {
    void* user{};
    PcUiNotifyServices ui_services{};
    PcUiResourceTickServices4659f0 tick_services{};
    PcEmbeddedUiConfigureR081 configure_ui{};
    PcEmbeddedUiFinalizeR081 finalize_ui{};
};
// PC 0x00446A80..0x00446B12. The callback pc_entry distinguishes this
// primary layout from 0x446B20 so an eventual 0x465860 backend can preserve
// the original fixed arguments exactly. A successful primary setup clears
// embedded byte +1+slot after 0x465970.
void embedded_slot_open_primary_446a80(Bytes embedded,std::uint32_t slot,
                                       const PcEmbeddedSlotsServices446cf0& services);
// PC 0x00446B20..0x00446BAD. Same immutable key/effect/coordinate lookup as
// the primary path but with the secondary 0x465860 fixed arguments and no
// clear of byte +1+slot.
void embedded_slot_open_secondary_446b20(Bytes embedded,std::uint32_t slot,
                                         const PcEmbeddedSlotsServices446cf0& services);
// PC 0x00446CF0..0x00446D8B. `globals.alternate` models byte 0x7D68BC.
void embedded_slots_tick_446cf0(Bytes embedded,PcUiNotifyGlobals& globals,
                                const PcEmbeddedSlotsServices446cf0& services);
void embedded_slot_close_446c50(Bytes,std::uint32_t,const PcEmbeddedSlotsServices446cf0&);
bool embedded_slots_set_446d90(Bytes,const std::array<std::uint32_t,8>&,
                               PcUiNotifyGlobals&,const PcEmbeddedSlotsServices446cf0&);
void embedded_slots_push_446fc0(Bytes,PcUiNotifyGlobals&,const PcEmbeddedSlotsServices446cf0&);
bool embedded_slots_select_446ea0(Bytes,std::uint32_t,PcUiNotifyGlobals&,const PcEmbeddedSlotsServices446cf0&);
void embedded_slots_visible_447000(Bytes,std::uint8_t visible,std::uint8_t immediate,
                                  const PcEmbeddedSlotsServices446cf0&);
void embedded_slots_clear_447090(Bytes,PcUiNotifyGlobals&,const PcEmbeddedSlotsServices446cf0&);

// r082: third mandatory state-2 child below r079's 0x445BE0 owner.
// The protected transfer at 0x44581C is normalized to an explicit native
// current-object view; the remaining body is ordinary runtime gating.
struct PcRuntimeGate445810Inputs {
    std::uint32_t system_handle_67f614{0xffffffffu};
    std::uint32_t menu_state_7d38f0{};
    std::uint32_t player0_feature_mask{};
    std::uint32_t game_mode_78026c{};
    std::uint32_t route_state_780258{};
    std::uint8_t alternate_7d68bc{};
};
using PcRuntimeGateConfigure445810 = void(*)(void* user,Bytes embedded,
                                             std::uint32_t selector,std::uint8_t flag);
using PcRuntimeGateAction445810 = void(*)(void* user,std::uint32_t pc_entry,
                                          Bytes object,std::uint32_t key);
struct PcRuntimeGate445810Services {
    void* user{};
    PcRuntimeGateConfigure445810 configure_446f30{};
    PcRuntimeGateAction445810 action_444fe0{};
};
// PC 0x00445810..0x004459EE. Larger action 0x444FE0 remains an explicit
// callback. Final key opens reuse the already-native 0x443FA0 path.
void object_runtime_gate_445810(
    Bytes object,Bytes current_object,
    const PcNativeHandleResolver& handles,
    const PcRuntimeGate445810Inputs& gate_inputs,
    const PcObjectStatePairTable& pair_table,
    const PcObjectEventCallbackTable& callback_table,
    const PcObjectEventModeInputs& event_inputs,
    const PcObjectEventDispatch443eb0Services& dispatch_services,
    const PcRuntimeControlServices& runtime_services,
    const PcObjectOpenCallbackServices& open_services,
    const PcRuntimeGate445810Services& gate_services);

// r083: fourth mandatory state-2 child below r079's 0x445BE0 owner.
// The two PC bytes at 0x7D68CB/0x7D68D0 are explicit mutable state here.
// Existing runtime-ready, callback dispatch, flagged-handle release and handle
// readiness primitives are reused rather than reproduced as opaque callbacks.
struct PcRuntimeTransitionFlags444530 {
    std::uint8_t request_7d68cb{};
    std::uint8_t active_7d68d0{};
};
// PC 0x00444530..0x0044464A. Selects transition key 0x0E or 0x0F, preserves
// an existing +0x490 handle when its state already matches/blocks replacement,
// opens the requested callback and commits it only when virtual slot +4 is ready.
void object_runtime_transition_444530(
    Bytes object,
    PcRuntimeTransitionFlags444530& flags,
    const PcRuntimeControlGlobals& runtime_globals,
    const PcNativeHandleResolver& handles,
    const PcObjectStatePairTable& pair_table,
    const PcObjectEventCallbackTable& callback_table,
    const PcObjectEventModeInputs& event_inputs,
    const PcObjectEventDispatch443eb0Services& dispatch_services,
    const PcRuntimeControlServices& runtime_services,
    const PcObjectOpenCallbackServices& open_services);

// r084: final mandatory state-2 child reached by r079's 0x445BE0 owner.
// 0x434DE0 is the small manager-idle predicate consumed by the parent.
std::uint8_t runtime_pair_idle_434de0(Bytes manager);

// 0x4459F0 opens key 0x2E through the already-native 0x443FA0 path when
// `direct_action` is zero.  The alternate nonzero path still routes through
// the larger 0x444FE0 body and therefore remains an explicit action boundary.
struct PcRuntimeOpen4459f0Services {
    void* user{};
    PcRuntimeGateAction445810 action_444fe0{};
};
void object_runtime_open_4459f0(
    Bytes object,std::uint8_t direct_action,
    const PcObjectStatePairTable& pair_table,
    const PcObjectEventCallbackTable& callback_table,
    const PcObjectEventModeInputs& event_inputs,
    const PcObjectEventDispatch443eb0Services& dispatch_services,
    const PcRuntimeControlServices& runtime_services,
    const PcObjectOpenCallbackServices& open_services,
    const PcRuntimeOpen4459f0Services& action_services);

constexpr std::size_t PcRuntimeQueueSlotCount445a50=20u;
constexpr std::size_t PcRuntimeQueueSlotSize445a50=0x30u;
struct PcRuntimeQueueState445a50 {
    std::uint8_t gate_7d68d2{};
    std::uint8_t alternate_7d68bc{};
    std::uint8_t request_7d68cb{};
    std::uint8_t busy_98a5f4{};
    std::uint32_t gate_988f44{};
    std::uint32_t active_989318{};
    std::uint32_t out_989320{};
    std::uint32_t out_989324{};
    std::uint32_t out_989328{};
    std::uint32_t out_98932c{};
    std::array<std::array<std::uint8_t,PcRuntimeQueueSlotSize445a50>,PcRuntimeQueueSlotCount445a50> slots{};
};
using PcRuntimeQueueFormatEmit445a50 = void(*)(
    void* user,std::uint32_t pc_entry,std::uint32_t mode,
    std::uint32_t format_token,Bytes slot,std::size_t text_offset,
    std::int32_t lifetime,std::uint32_t limit);
struct PcRuntimeQueueServices445a50 {
    void* user{};
    PcRuntimeQueueFormatEmit445a50 format_emit{};
};
// PC 0x00445A50..0x00445BD9.  The protected 0x4035F0 singleton result is
// supplied as `current_object`; the formatter/output pair 0x5802DD/0x492690
// remains an explicit native service, while 0x465EB0 lookup itself is reused.
void object_runtime_queue_445a50(
    Bytes object,Bytes current_object,Bytes manager_988f40,Bytes ui_lookup_table,
    PcRuntimeQueueState445a50& queue,
    const PcRuntimeControlGlobals& runtime_globals,
    const PcObjectStatePairTable& pair_table,
    const PcObjectEventCallbackTable& callback_table,
    const PcObjectEventModeInputs& event_inputs,
    const PcObjectEventDispatch443eb0Services& dispatch_services,
    const PcRuntimeControlServices& runtime_services,
    const PcObjectOpenCallbackServices& open_services,
    const PcRuntimeOpen4459f0Services& action_services,
    const PcRuntimeQueueServices445a50& queue_services);

// r085: registered runtime callback thunk above the r079 owner.  The PC body
// 0x49E4A0 calls protected singleton resolver 0x4035F0, moves the returned
// pointer into ECX and tail-jumps to 0x445BE0.  Native ownership supplies the
// current object explicitly, so no guest singleton/allocation mechanism is kept.
// PC 0x0049E4A0..0x0049E4AB.
std::uint8_t runtime_owner_entry_49e4a0(
    Bytes current_object,
    PcObjectRuntimeSnapshot444350& snapshot,
    PcObjectSelectorGlobals& selector_globals,
    const PcRuntimeOwnerInputs445be0& inputs,
    const PcRuntimeOwnerServices445be0& services);

// r086: the remaining EvFuncID 36 callback thunks surrounding r085.  As with
// 0x49E4A0, the protected 0x4035F0 singleton lookup is replaced by explicit
// native ownership of current_object.  0x443E90 is the destroy-side tail alias
// into the already-closed 0x443C30 teardown body.
std::uint32_t runtime_init_entry_49e490(
    Bytes current_object,
    std::array<PcObjectEventCallbackEntry,PcObjectEventCallbackCount>& callback_table,
    std::uint8_t global_mode_6319a1,const PcObjectRuntimeInitServices& services);
void runtime_display_entry_49e4b0(Bytes current_object,const PcObjectStateServices& services);
std::uint32_t runtime_teardown_alias_443e90(Bytes current_object,const PcObjectRuntimeTeardownServices& services);
std::uint32_t runtime_destroy_entry_49e4c0(Bytes current_object,const PcObjectRuntimeTeardownServices& services);



// r087: top-level subsystem bootstrap reached before the regular runtime/event
// loop.  PC 0x00417810..0x0041788A performs a fixed sequence of subsystem
// initializers, calls InitEventControl in the middle, performs two virtual
// registration calls through the global system object, and tail-jumps to the
// final subsystem initializer.  The non-event subsystems remain explicit
// native boundaries: r087 closes the orchestration/order, not those callees.
using PcBootstrapCall417810 = std::uint32_t(*)(
    void* user,std::uint32_t pc_entry,std::uint32_t arg0,
    std::uint32_t arg1,std::uint32_t arg2);
struct PcBootstrapServices417810 {
    void* user{};
    PcBootstrapCall417810 call{};
    std::uint32_t system_token{};
};
std::uint32_t runtime_bootstrap_417810(
    PcEventControlState& event_state,
    const std::array<PcEventInitDescriptor,PcEventSlotCount>& descriptors,
    const std::array<PcEventFunctionDescriptor,PcEventFunctionTableCount>& functions,
    const PcBootstrapServices417810& services);


// r090: platform/runtime initialization gate immediately below the r088 owner.
// PC 0x00417740..0x00417800.  The orchestration itself is portable; Win32
// CreateThread/SetThreadPriority and the remaining subsystem bodies are
// represented as explicit service calls.  Former absolute globals are owned by
// this state instead of being addressed at their x86 VAs.
struct PcPlatformInitState417740 {
    std::uint32_t window_token{};          // PC global 0x8A8C88
    std::uint32_t resource_token_740ca0{}; // PC global 0x740CA0
    std::uint32_t global_8a8ce0{};
    std::uint32_t global_8a8cac{};
    std::uint32_t global_89f684{};
    std::uint32_t global_89f66c{};
    std::array<std::uint32_t,0x75> scratch_8999c0{};
    std::uint32_t thread_token{};          // PC global 0x955AD8
};
struct PcPlatformInitServices417740 {
    void* user{};
    PcBootstrapCall417810 call{};
};
std::uint32_t runtime_platform_init_417740(
    PcPlatformInitState417740& state,
    std::uint32_t primary_system_token,
    const PcPlatformInitServices417740& services);

// r093: QPC/QPF frame-quantization helper used by the 0x417B20 runtime loop.
// PC 0x00417890..0x0041796F samples frequency/counter, accumulates elapsed
// ticks, converts them to target-rate frame quanta and keeps the sub-frame
// remainder.  The Windows clock itself is a portable service.
struct PcRuntimeTimingState417890 {
    std::int64_t frequency_8a8c98{};
    std::int64_t current_counter_8a8ca0{};
    std::int64_t previous_counter_8a8c90{};
    std::int64_t accumulator_8a8cd0{};
    std::uint32_t gate_8a8cc8{1u};
    float frame_scale_95af40{};
};
using PcRuntimeClockQuery417890 = std::int64_t(*)(void* user,std::uint32_t import_address);
struct PcRuntimeTimingServices417890 {
    void* user{};
    PcRuntimeClockQuery417890 query{};
};
std::uint32_t runtime_frame_ticks_417890(
    PcRuntimeTimingState417890& state,
    std::int32_t target_rate,
    const PcRuntimeTimingServices417890& services);

// r094: platform-neutral extraction of the central update/render block inside
// PC 0x00417B20.  The source region 0x00417C7B..0x00417DD4 is not a standalone
// original function and therefore does not increase the closed-routine count.
struct PcRuntimeFrameState417c7b {
    std::uint32_t mode_78026c{};
    std::uint32_t updates_95af48{};
    std::uint32_t update_index_8a8cdc{};
    std::uint32_t primary_system_token{};
    std::uint32_t frame_counter_95af0c{};
    float elapsed_8a8cb4{};
    std::uint32_t slow_frame_flag_8a8cc0{};
    std::uint32_t slow_frame_count_8a8cc4{};
};
using PcRuntimeFrameCall417c7b = std::uint32_t(*)(
    void* user,std::uint32_t pc_or_call_site,const std::uint32_t* args,std::size_t arg_count);
using PcRuntimeFrameElapsed417c7b = float(*)(void* user,std::uint32_t pc_or_call_site);
// Port enhancement (enhancements/frame_rate.hpp), not in the PC game: with
// `display_frames` a frame may run no tick (display above 60 Hz). Called
// before each tick and once after the ticks, before 454670 and the draw.
struct PcRuntimeTickHooks417c7b {
    bool display_frames{};
    void(*before_tick)(){};
    void(*after_ticks)(const PcRuntimeTimingState417890*,std::uint32_t updates){};
};
struct PcRuntimeFrameServices417c7b {
    void* user{};
    PcRuntimeFrameCall417c7b call{};
    PcRuntimeFrameElapsed417c7b elapsed{};
    PcRuntimeTimingState417890* timing_state{};
    const PcRuntimeTimingServices417890* timing_services{};
    const PcRuntimeTickHooks417c7b* ticks{};   // null: the PC loop
};
struct PcRuntimeFrameResult417c7b {
    std::uint32_t updates{};
    bool platform_wait_before_next_frame{};
};
PcRuntimeFrameResult417c7b runtime_frame_step_417c7b(
    PcRuntimeFrameState417c7b& state,
    const PcRuntimeFrameServices417c7b& services);

// r092: companion setup helper used before the 0x417B20 runtime loop.
// PC 0x00417A20..0x00417B10 initializes the loop-owned resources and a small
// set of runtime/UI subsystems.  System/UI objects remain opaque native tokens;
// the callback receives exact semantic call sites plus the original logical
// argument vector.  Calls that create an out-handle return the native token
// that becomes owned by this state.
struct PcRuntimeLoopSetupState417a20 {
    std::uint32_t system_token{};       // PC global 0x89BD60 (borrowed)
    std::uint32_t object_95b218{};      // borrowed
    std::uint32_t optional_7f94e8{};    // borrowed / optional
    std::uint8_t feature_79fb50{};
    std::uint32_t mode_78026c{};
    std::uint32_t handle_8a89f4{};      // created by virtual +0x64
    std::uint32_t handle_8a8a00{};      // created by virtual +0x74
    std::uint32_t handle_89f680{};      // created by virtual +0x1D8
    std::uint32_t global_89f684{};
    std::uint32_t global_89f66c{};
    std::uint8_t flag_73e2b0{};
};
using PcRuntimeLoopSetupCall417a20 = std::uint32_t(*)(
    void* user,std::uint32_t pc_or_call_site,
    const std::uint32_t* args,std::size_t arg_count);
struct PcRuntimeLoopSetupServices417a20 {
    void* user{};
    PcRuntimeLoopSetupCall417a20 call{};
};
std::uint32_t runtime_loop_setup_417a20(
    PcRuntimeLoopSetupState417a20& state,
    const PcRuntimeLoopSetupServices417a20& services);

// r091: local cleanup helper used by the 0x417B20 runtime loop.  PC
// 0x00417970..0x00417A11 releases five loop-owned handles, runs the
// intermediate shutdown boundary, invokes two system/UI virtual cleanup
// slots, then releases the final system-created handle.  Guest pointers are
// represented as opaque native tokens; the callback receives the original
// call site and virtual byte offset so ordering remains testable.
struct PcRuntimeLoopCleanupState417970 {
    std::uint32_t handle_8a89f4{};
    std::uint32_t handle_8a8a00{};
    std::uint32_t handle_95afcc{};
    std::uint32_t handle_95afc4{};
    std::uint32_t handle_95afc8{};
    std::uint32_t object_95b218{};
    std::uint32_t optional_7f94e8{};
    std::uint32_t handle_89f680{};
};
using PcRuntimeLoopCleanupCall417970 = void(*)(
    void* user,std::uint32_t pc_or_call_site,std::uint32_t token,
    std::uint32_t vtable_byte_offset);
struct PcRuntimeLoopCleanupServices417970 {
    void* user{};
    PcRuntimeLoopCleanupCall417970 call{};
};
void runtime_loop_cleanup_417970(
    PcRuntimeLoopCleanupState417970& state,
    const PcRuntimeLoopCleanupServices417970& services);

// r088: startup/shutdown owner directly above r087.  PC
// 0x004176E0..0x0041773B gates the r087 bootstrap through 0x417740, then
// runs two larger runtime stages, performs unconditional platform shutdown
// calls and releases the two global system objects through virtual slot +8.
// The large gate/runtime/platform callees remain explicit boundaries; when the
// gate succeeds the already-native r087 bootstrap is invoked directly.
struct PcStartupOwnerServices4176e0 {
    void* user{};
    PcBootstrapCall417810 call{};
    PcPlatformInitState417740* platform_init_state{};
    const PcPlatformInitServices417740* platform_init_services{};
};
std::uint32_t runtime_startup_owner_4176e0(
    PcEventControlState& event_state,
    const std::array<PcEventInitDescriptor,PcEventSlotCount>& descriptors,
    const std::array<PcEventFunctionDescriptor,PcEventFunctionTableCount>& functions,
    std::uint32_t& primary_system_token,
    std::uint32_t& secondary_system_token,
    const PcStartupOwnerServices4176e0& services);

// Exact switch-case body at PC 0x00486942 inside a larger command handler.
// This is intentionally not counted as a closed original routine in r052.
// command 0 installs GamePlCar_Ctrl, command 1 installs PasPlCar_Ctrl.
void select_pl_car_ctrl_486942(Bytes event,PcEventControlState& state,std::int32_t command);
}
