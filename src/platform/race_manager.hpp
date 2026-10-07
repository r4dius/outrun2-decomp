#pragma once
// GAME-mode race manager ("Observer" in the Lindbergh build): event 359,
// function 0 of the START event table.
//   init    0x450790   control 0x4515B0   display 0x44FE00   destroy 0x44FE10
// It owns the race time (0x7D394C), stage/sector progression, the route
// choices (0x7D39A0..), the result flags (0x7D39F0) and every global in
// 0x7D3650..0x7D39FF. That block is RaceManagerState, byte-for-byte in PC
// layout (so indexed PC accesses such as [level*4+0x7D3834] keep their exact
// aliasing).
//
// Everything owned by another system is explicit:
//  * RaceManagerWorld: globals the manager code reads/writes directly
//    (event table 0x799B38 / open bytes 0x79FB48, game variant 0x780258,
//    course selection *0x7D3188, stage records 0x7D33BC/0x7D33C4 and the
//    GetNowStageLevel cache 0x635F2C, area time table 0x7D2E98, ...).
//  * RaceManagerServices: every non-manager function called, one virtual per
//    PC callee, invoked in the original order with the original arguments.
//    Nothing is replaced: a service the integrator cannot provide must be
//    reported, not stubbed with invented behaviour.
// 44C940/44C8D0 (GetNowStageLevel + record search) are ported here on the
// explicit stage view because 44DC70 inlines their cache logic.
#include <array>
#include <cstddef>
#include <cstdint>
#include <utility>
namespace outrun::platform {

struct RaceManagerState {                               // PC 0x7D3650..0x7D39FF
    std::int32_t sector_ms_7d3650[60]{};                // [level*4+sector] race time (449C10 ms), -1 = not reached
    std::uint32_t goal_first_stage_7d3740{};            // (E+5C==0) latched at time-up
    std::uint32_t unused_7d3744{};
    std::int32_t sector_stage_ms_7d3748[60]{};          // [level*4+sector] time inside the stage (ms)
    float stage_speed_7d3838[15]{};                     // average speed per stage level
    std::int32_t sector_index_7d3874{};                 // last sector (0..3)
    float goal_ratio_7d3878{};                          // position / course end at time-up
    std::int32_t split_ms_7d387c{};
    std::int32_t time_adjust_7d3880{};                  // subtracted from the time at the next extend
    std::uint32_t countdown_frames_7d3884{};            // frames with start countdown < 1
    float average_speed_7d3888{};
    std::uint32_t rank_event_7d388c[8]{};               // 44FE80: [rank] = event id
    std::uint32_t stage_flag_7d38ac[14]{};              // [level-1] set at stage clear / [4EF710()] at goal
    std::uint32_t w7d38e4{};
    std::uint32_t branch_state_7d38e8{};                // bit0..5 branch lamp state machine
    std::uint32_t display_count_7d38ec{};
    std::uint32_t over_state_7d38f0{};                  // game over / goal state (450170)
    std::uint32_t stage_key_7d38f4[15]{};               // [level] stage key
    std::uint32_t stage_frames_7d3930{};                // frames in the current stage (<= 0x57E3F)
    std::uint32_t sector_banner_7d3934{};               // sector display countdown (300)
    float speed_sum_7d3938{};
    std::int32_t sector_ms_7d393c{};
    std::uint16_t goal_key_7d3940{};
    std::uint16_t unused_7d3942{};
    std::int32_t sector_level_7d3944{};
    std::uint32_t last_stage_frames_7d3948{};
    std::int32_t time_7d394c{};                         // race time (frames), 44FE30/44FE40
    std::uint32_t stage_clear_count_7d3950{};
    std::int32_t stage_split_7d3954[15]{};
    std::uint32_t extend_count_7d3990{};
    std::uint32_t sector_new_7d3994{};
    std::uint32_t sector_set_7d3998{};
    std::int8_t sector_zone_prev_7d399c{};
    std::int8_t sector_zone_7d399d{};
    std::uint8_t unused_7d399e[2]{};
    std::uint32_t route_7d39a0[14]{};                   // branch record per level (0,1, 2 = undecided)
    std::uint32_t unused_7d39d8{};
    std::uint32_t total_frames_7d39dc{};                // <= 0x57E3F
    std::int8_t stage_rank_7d39e0[15]{};                // [level-1] 45A2B0 rank at stage clear
    std::uint8_t unused_7d39ef{};
    std::uint32_t flags_7d39f0{};                       // bit0 time up, 1 stage clear, 2 goal, 3 extend, 5/6 lamps
    std::uint32_t sector_goal_7d39f4{};
    float stage_speed_latch_7d39f8{};
    std::uint32_t unused_7d39fc{};

    static constexpr std::uint32_t base=0x7d3650u;
    // PC-address access with the original aliasing; throws outside the block.
    std::uint32_t u32(std::uint32_t pc) const;
    void put32(std::uint32_t pc,std::uint32_t v);
    std::uint8_t u8(std::uint32_t pc) const;
    void put8(std::uint32_t pc,std::uint8_t v);
};
static_assert(sizeof(RaceManagerState)==0x3b0,"PC layout 0x7D3650..0x7D39FF");

// Car event work (tagEVWORK_CAR). Offsets used by the manager: +0 event id,
// +4, +10, +5C, +64, +68, +184, +18C, +1C0, +1C4, +25C/+25D, +25E, +260,
// +2B0, +D28 (point). The integrator guarantees the pointee is large enough.
using RaceEventWork=std::uint8_t*;

struct RaceManagerWorld {
    std::array<RaceEventWork,32> event_work_799b38{};   // [id] = *(0x799B38+id*0x3C); ids 8..31 used by init
    std::array<std::uint8_t,32> event_open_79fb48{};    // [id] = byte 0x79FB48+id (open state in bits 0..1)
    std::int32_t player_events_680ad4{4};               // player car events 8..8+n-1
    std::int32_t variant_780258{};
    std::int32_t mode_78026c{};
    std::int32_t difficulty_7c24c0{};
    std::uint8_t time_decrement_637911{1};              // .data byte, no PC writer found
    std::uint32_t ranking_80fb14{};
    std::uint32_t network_7f95a8{};
    // *0x7D3188 course selection record (pointer non-null = course_present).
    bool course_present_7d3188{true};
    std::int32_t course_10{},course_2c{-1},course_30{-1};
    std::int16_t course_14_5c{};                        // word [[sel+0x14]+0x5C]
    std::uint32_t area_7d33b0{};
    const std::int16_t* time_table_7d2e98{};            // 3*15*5 words (area manager)
    // Stage records (0x7D33BC pointer, 0x7D33C4 count; stride 0x78, key +4, level +8)
    const std::uint8_t* stage_records_7d33bc{};
    std::int32_t stage_count_7d33c4{};
    std::uint32_t stage_cache_key_635f2c{0xffffffffu},stage_cache_value_635f30{};
    // Written by the manager, owned elsewhere.
    std::uint8_t flag_84490c{};

    RaceEventWork work(std::uint32_t id) const;         // throws on id>=32 or null
    std::uint8_t open(std::uint32_t id) const;
};

struct RaceVec3 {float x{},y{},z{};};
// One virtual per foreign PC callee (names = PC entry). Argument order is the
// PC push order reversed (i.e. C order). Values are the raw PC return registers.
struct RaceManagerServices {
    virtual ~RaceManagerServices()=default;
    virtual void pc_46fab0()=0;                               // word 0x8037BC = 0
    virtual void pc_47dac0()=0;                               // 2D race display init
    virtual void pc_47dc00()=0;                               // 2D race display destroy
    virtual void pc_47ec00()=0;                               // 2D race display control
    virtual std::uint32_t pc_44c2c0()=0;                      // area current stage key [0x7D30AC]
    virtual std::uint8_t pc_43f860()=0;                       // protected: byte [0x780260]
    virtual std::uint8_t pc_48b1a0()=0;                       // byte [0x83036D]
    virtual std::uint32_t pc_43f960()=0;                      // [0x78024C] in {2,3}
    virtual std::uint8_t pc_456d60()=0;                       // save byte [0x7DE418]
    virtual void pc_4f0dd0()=0;                               // visibility table init
    virtual void pc_4f0e40()=0;                               // visibility control
    virtual void pc_456720(std::uint32_t packed_route)=0;     // save: thiscall(0x7DE418) store route
    virtual void pc_4f2ac0()=0;                               // score init
    virtual void pc_4f2df0()=0;                               // score control
    virtual void pc_4f2b20(std::int32_t seconds)=0;           // score stage bonus
    virtual std::uint8_t pc_45a2b0(std::uint8_t car)=0;       // protected rank provider
    virtual std::uint32_t pc_55a930()=0;                      // thiscall(0x7F9460) network active
    virtual std::uint8_t pc_48b350()=0;                       // [0x656234] < 60
    virtual std::uint32_t pc_46c500()=0;                      // network host check
    virtual std::uint32_t pc_44bec0()=0;                      // area matrix (0x7D3190), opaque
    virtual void pc_409f90(std::uint32_t matrix)=0;           // matrix push + load
    virtual void pc_40a240()=0;                               // matrix inverse
    virtual RaceVec3 pc_40a7d0(const std::uint8_t* point)=0;  // transform point (out, in)
    virtual void pc_40a010()=0;                               // matrix pop
    virtual void pc_44b900(std::uint32_t side)=0;             // area branch select
    virtual void pc_46c410(std::uint32_t value)=0;            // [0x7F95B0] = value
    virtual void pc_46c400(std::uint32_t value)=0;            // [0x7F95AC] = value
    virtual std::uint8_t pc_44b7b0(std::uint32_t arg)=0;      // area/course final-stage test
    virtual std::uint8_t pc_4957f0()=0;                       // byte [0x836374] mission active
    virtual std::uint8_t pc_48b310()=0;                       // byte [0x830394]
    virtual std::uint8_t pc_495490()=0;                       // protected snippet
    virtual std::uint32_t pc_44bdd0()=0;                      // area next stage key
    virtual void pc_495b60()=0;                               // ++[0x836378]
    virtual std::uint32_t pc_495b10()=0;
    virtual std::uint32_t pc_495b70()=0;
    virtual std::uint32_t pc_495b30()=0;
    virtual std::uint8_t pc_4b00d0()=0;                       // protected
    virtual std::uint32_t pc_4b0190()=0;
    virtual void pc_4b0110(std::uint32_t key,std::uint32_t value)=0;
    virtual void pc_48b3a0()=0;
    virtual void pc_495610()=0;
    virtual void pc_45a0a0()=0;                               // save: thiscall(0x7DE418) 459C30
    virtual std::uint32_t pc_456d10()=0;                      // save packed route [0x7DE77C]
    virtual void pc_456d20(std::uint32_t packed_route)=0;     // save: 456820(packed,[0x7F1938]) on 0x7DE418
    virtual void pc_476760(RaceEventWork work)=0;             // ranking update
    // save: thiscall(0x7DE418) 4568E0(count,&key,flag); may rewrite key.
    virtual std::uint32_t pc_456dc0(std::uint32_t count,std::uint32_t& key,std::uint32_t flag)=0;
    virtual std::uint16_t pc_49b2d0()=0;                      // protected: word [0x8367BC] start countdown
    virtual std::uint32_t pc_456d50()=0;                      // byte [0x7DE784] != 0
    virtual std::int32_t pc_456d40()=0;                       // protected snippet (save time)
    virtual std::uint32_t pc_456de0(std::int32_t time)=0;     // save: thiscall(0x7DE418) 4569D0
    virtual std::uint16_t pc_43d470(std::uint32_t course)=0;  // course end position
    virtual void pc_4aeef0(std::uint32_t a,std::uint32_t level,float ratio)=0;
    virtual void pc_458450()=0;                               // [0x7DE388] = [0x7F1938]
    virtual void pc_467190()=0;
    virtual std::uint8_t pc_4962a0()=0;
    virtual void pc_453080(std::uint32_t value)=0;            // [0x7D6764] = value
    virtual std::uint32_t pc_4ef710()=0;                      // stage slot (4 or 14)
    virtual void pc_465f20()=0;
    virtual std::int32_t pc_44dc50(std::uint32_t key)=0;      // stage number of key
    virtual std::uint32_t pc_48b330()=0;
    virtual void pc_48b340()=0;                               // tail call of control
    virtual std::pair<float,float> pc_4a4440(std::uint32_t v,const std::uint8_t* a,const std::uint8_t* b)=0; // (out3,out4)
    virtual void pc_48b3d0()=0;
    virtual void pc_467e00()=0;
};

struct RaceFrameTime {std::uint16_t hours,minutes,seconds,ms;};

// ----- event functions --------------------------------------------------
void race_manager_init_450790(RaceManagerState&,RaceManagerWorld&,RaceManagerServices&);
void race_manager_control_4515b0(RaceManagerState&,RaceManagerWorld&,RaceManagerServices&);
void race_manager_display_44fe00(RaceManagerState&);          // 49A650 is an empty function
void race_manager_destroy_44fe10(RaceManagerState&,RaceManagerServices&);

// ----- callees (manager-owned) -------------------------------------------
std::uint32_t race_stage_level_44c940(RaceManagerWorld&,std::uint32_t key);
std::int32_t race_extended_time_44dc70(RaceManagerWorld&,RaceManagerServices&,std::int32_t variant,std::int32_t key,std::int32_t difficulty);
void race_rank_table_44fe80(RaceManagerState&,RaceManagerWorld&,RaceManagerServices&);
void race_check_goal_4513c0(RaceManagerState&,RaceManagerWorld&,RaceManagerServices&,RaceEventWork e);
bool race_check_stage_clear_44ff20(RaceManagerState&,RaceManagerWorld&,RaceManagerServices&,RaceEventWork e);
void race_check_branch_off_451220(RaceManagerState&,RaceManagerWorld&,RaceManagerServices&,RaceEventWork e);
void race_check_branch_off_4512c0(RaceManagerState&,RaceManagerWorld&,RaceManagerServices&,RaceEventWork w);
void race_check_time_extend_450cc0(RaceManagerState&,RaceManagerWorld&,RaceManagerServices&,RaceEventWork e);
void race_check_game_timer_450ac0(RaceManagerState&,RaceManagerWorld&,RaceManagerServices&);
void race_check_game_over_450170(RaceManagerState&,RaceManagerWorld&,RaceManagerServices&);
void race_check_sector_time_450e10(RaceManagerState&,RaceManagerWorld&,RaceManagerServices&,RaceEventWork e);
float race_sector_progress_4506b0(RaceManagerServices&,RaceEventWork e);
std::uint32_t race_pack_route_4503d0(const RaceManagerState&);
void race_unpack_route_450490(RaceManagerState&,std::uint32_t packed);
void race_set_route_451140(RaceManagerState&,RaceManagerWorld&,RaceManagerServices&,std::int32_t level,std::uint32_t value); // protected, measured
std::uint32_t race_get_route_451350(RaceManagerState&,RaceManagerWorld&,RaceManagerServices&,std::int32_t level);
void race_store_route_456cf0(const RaceManagerState&,RaceManagerServices&);
RaceFrameTime race_frames_to_time_449b30(std::uint32_t frames,float fraction); // CalcFrame2Time
std::int32_t race_time_ms_449c10(std::uint16_t h,std::uint16_t m,std::uint16_t s,std::uint16_t ms);

// ----- small accessors used by other systems --------------------------------
std::uint32_t race_countdown_frames_44fdf0(const RaceManagerState&);
void race_set_time_44fe30(RaceManagerState&,std::int32_t);
std::int32_t race_time_44fe40(const RaceManagerState&);
void race_set_flag0_44fe50(RaceManagerState&,std::uint32_t);
std::uint32_t race_flag0_44fe70(const RaceManagerState&);
void race_set_flag2_44fef0(RaceManagerState&,std::uint32_t);
std::uint32_t race_flag2_44ff10(const RaceManagerState&);
void race_set_flag1_450110(RaceManagerState&,std::uint32_t);
std::uint32_t race_flag1_450130(const RaceManagerState&);   // protected snippet, measured
void race_set_flag3_450140(RaceManagerState&,std::uint32_t);
std::uint32_t race_flag3_450160(const RaceManagerState&);
void race_set_over_state_450230(RaceManagerState&,std::uint32_t);
std::uint32_t race_over_state_450240(const RaceManagerState&);
std::int32_t race_next_stage_key_450250(RaceManagerWorld&,std::uint16_t preset_78024c,std::int32_t key,std::int32_t branch);
std::uint32_t race_flag5_4502c0(const RaceManagerState&);
void race_clear_flag5_4502d0(RaceManagerState&);
void race_request_lamp_4502e0(RaceManagerState&,std::int32_t which);
std::uint32_t race_flag6_450300(const RaceManagerState&);
void race_clear_flag6_450310(RaceManagerState&);
std::uint32_t race_route_number_450320(const RaceManagerState&,RaceManagerServices&);
std::uint32_t race_event_stage_key_450380(const RaceManagerWorld&,std::uint32_t id);
std::uint16_t race_event_position_4503a0(const RaceManagerWorld&,std::uint32_t id);
std::uint32_t race_leader_event_4503c0(const RaceManagerState&);
std::uint32_t race_stage_key_450560(const RaceManagerState&,std::int32_t level); // protected, measured
std::uint32_t race_stage_frames_450570(const RaceManagerState&);
std::int32_t race_split_450580(const RaceManagerState&);
std::int32_t race_sector_ms_450590(const RaceManagerState&);
std::int32_t race_stage_split_4505a0(const RaceManagerState&,std::int32_t level);
std::uint32_t race_sector_new_4505b0(const RaceManagerState&);
std::uint32_t race_sector_set_4505c0(const RaceManagerState&);
std::uint32_t race_total_frames_4505d0(const RaceManagerState&);
std::uint32_t race_time_451180(const RaceManagerState&,RaceManagerServices&,RaceEventWork player,std::uint32_t request);
float race_average_speed_4505e0(const RaceManagerState&);
float race_stage_speed_4505f0(const RaceManagerState&,std::int32_t level);
void race_set_time_adjust_450600(RaceManagerState&,std::int32_t);
std::int32_t race_sector_time_450610(const RaceManagerState&,std::int32_t level,std::int32_t sector);
std::int32_t race_sector_stage_time_450630(const RaceManagerState&,std::int32_t level,std::int32_t sector);
void race_latest_sector_450650(const RaceManagerState&,std::int32_t& level,std::int32_t& sector);
std::uint32_t race_sector_banner_450670(const RaceManagerState&);
std::uint32_t race_display_count_450680(const RaceManagerState&);
std::uint32_t race_sector_goal_450690(const RaceManagerState&);
std::uint32_t race_stage_flag_4506a0(const RaceManagerState&,std::int32_t index);
std::uint32_t race_is_last_level_450750(RaceManagerServices&,std::int32_t level);
std::uint32_t race_last_level_450780(RaceManagerServices&);
// 44B9C0/44B9D0/44B9E0/44B9F0: area manager state [0x7D2E80] = 0x16/0x17/0x19/0x18.
void race_area_state_44b9c0(std::int32_t& area_7d2e80);
void race_area_state_44b9d0(std::int32_t& area_7d2e80);
void race_area_state_44b9e0(std::int32_t& area_7d2e80);
void race_area_state_44b9f0(std::int32_t& area_7d2e80);

// ----- trivial foreign service bodies (owned by other systems) ---------------
// Ported so the integrator can implement pc_46fab0 / pc_4f0dd0 / pc_4f2ac0
// on those systems' own storage. Views are the raw PC byte ranges.
void race_reset_8037bc_46fab0(std::uint16_t& w8037bc);
constexpr std::uint32_t kVisibility84cec8Size=0x84d8f4u-0x84cec8u;  // 0x84CEC8..0x84D8F3
void race_visibility_init_4f0dd0(std::uint8_t* block_84cec8);        // 40 x 0x34 records +0, 10 x +0x18, 84D8E0, 84D8F0
// 0x84DEF8..0x84DF38 and the padding up to the SP ranking tables 84DF40: one region, since the
// name entry's commit at rank -1 writes its record over 84DF30..84DF3F (dword at 84DF38).
constexpr std::uint32_t kScore84def8Size=0x84df40u-0x84def8u;
void race_score_init_4f2ac0(std::uint8_t* block_84def8);
}
