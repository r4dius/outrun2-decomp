#pragma once
// GAME-mode race sound manager ("Sound" in the Lindbergh build): event 383
// SOUND, function 0x17 of the event function table 59BE78 (stride 20):
//   init    0x424650   control 0x424700   display 0x49A650 (RET)   destroy 0x424790
// and event 360 COMM_TRANS, function 0x48: control 0x45A280 only.
//
// Ported line by line from the PC EXE (Lindbergh names for orientation):
//   424650 Sound_Init        424700 Sound_Ctrl        424790 Sound_Dest
//   424820 FlushIcsQueue     424880 InitSndQueue      424940 SetSndQueue
//   4249A0 FlushSndQueue     424A00 InitCarSoundWork  424B10 pause clear
//   4247B0 SetIcsQueue       (protected entry: the VM executes
//                             "mov ecx,[0x956140]" then returns to 4247B6;
//                             measured with area_bridge_probe)
//   424B50/424B90/424BF0/424C60 stage predicates      424C80 cheer targets
//   424F80 PlayCheer         425100 PlayCarSound      4252E0 PlayEngine
//   4256F0 MakePlcarParam    425A40 PlayRoadCondition 4260E0 PlaySkid
//   4264C0 PlayWall          4267C0 nearest cars      426AC0 NaN test
//   426B10 PlayEnCar         426F50 passing cars      427110 PlayEnvironment
//   427320 ICS voice set-up  4279C0 volume balance    427A20 ICS volume scale
//
// The sound manager owns the block 0x955AE0..0x956467 (RaceSoundState,
// byte-for-byte in PC layout: ICS parameter cache 955AE0 [17 channels x 6],
// ICS queue 955CA0 [88 x {ch,param,value}] with write index 955C78 and read
// index 956140, balance tables 9560D0/9560F0, CarSound 956148 (0x2A0 bytes),
// SE queue 9563E8 [32] with read/write indices 9560C0/956124) plus a few
// separate globals (95B208/95B20C cheer targets, 98ABD0/98ABD4 cheer phase,
// 780024/780028 last surface sounds).
//
// The PC audio layer is the platform boundary and is a service:
//   42EFF0(ch, value, code)  ICS channel parameter (codes table 623EA4)
//   42F0D0(command)          sound effect command (same entry the frontend
//                            reaches through 4249F0)
//   427630()                 ClearAllSound (JMP 4D4A91 -> 427635, DirectSound
//                            buffers 7763E0 and ICS channels 957BE8)
// Every other foreign callee is a virtual service too. The CRT helpers are
// ported: _ftol2 582194, _CIpow 5826D0 (x87 path, see race_sound.cpp),
// _isnan 5811AF, fsin/fsqrt/fabs leaves 449360/449380/449390, vector leaves
// 40EFA0/40EFF0 and D3DXVec3Normalize (4393E8 -> pinned d3dx9_29 generic).
#include "driving/pc_x87.hpp"
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <stdexcept>
namespace outrun::platform {

struct RaceSoundState {
    static constexpr std::uint32_t base=0x955ae0u,end=0x956468u;
    std::array<std::uint8_t,end-base> block{};     // PC 0x955AE0..0x956467
    std::uint32_t cheer_95b208{},cheer_95b20c{};    // cheer volume targets
    std::uint32_t counter_98abd0{};                 // cheer frame counter
    float phase_98abd4{};                           // cheer oscillator phase
    std::uint32_t surface_780024{},surface_780028{};// last road-condition sounds B / A

    std::uint32_t u32(std::uint32_t pc)const{std::uint32_t v;std::memcpy(&v,at(pc,4),4);return v;}
    std::int32_t i32(std::uint32_t pc)const{return std::int32_t(u32(pc));}
    float f32(std::uint32_t pc)const{float v;std::memcpy(&v,at(pc,4),4);return v;}
    std::uint16_t u16(std::uint32_t pc)const{std::uint16_t v;std::memcpy(&v,at(pc,2),2);return v;}
    void put32(std::uint32_t pc,std::uint32_t v){std::memcpy(at(pc,4),&v,4);}
    void putf(std::uint32_t pc,float v){std::memcpy(at(pc,4),&v,4);}
    void put16(std::uint32_t pc,std::uint16_t v){std::memcpy(at(pc,2),&v,2);}
    // Named cells used by the integration.
    static constexpr std::uint32_t ics_write_955c78=0x955c78u,ics_read_956140=0x956140u,
        se_read_9560c0=0x9560c0u,se_write_956124=0x956124u,se_queue_9563e8=0x9563e8u,
        running_955c7c=0x955c7cu,car_sound_956148=0x956148u;
private:
    const std::uint8_t* at(std::uint32_t pc,std::size_t n)const{
        if(pc<base||pc+n>end)throw std::out_of_range("race sound state address outside 955AE0..956467");
        return block.data()+(pc-base);}
    std::uint8_t* at(std::uint32_t pc,std::size_t n){
        if(pc<base||pc+n>end)throw std::out_of_range("race sound state address outside 955AE0..956467");
        return block.data()+(pc-base);}
};

// A PC object known by its PC address (the value stored in the PC pointer
// cells) with its bytes. Size 0 = not provided.
struct RaceSoundView {
    std::uint32_t pc{};
    std::uint8_t* data{};
    std::size_t size{};
};

struct RaceSoundWorld {
    std::uint8_t enabled_95b248{};                  // audio layer ready (42ED00 sets 1)
    std::int32_t mode_78026c{};                     // root mode (0x0D, 0x10 GAME, 0x13, 0x18, ...)
    const std::uint8_t* event_flags_79fb48{};       // byte [0x79FB48+id], at least 0x180 bytes
    std::array<RaceSoundView,32> event_work{};      // [id] = *(0x799B38+id*0x3C) (8 = player, 9..31 other cars)
    RaceSoundView car_82e7f0{};                     // player tagCAR_WORK 0x82E7F0 (ESI of 4256F0)
    RaceSoundView params{};                         // *(player+0x2B4) (4256F0 reads +0x10A0)
    const float* distance_802af0{};                 // car distance table 0x802AF0 (4267C0)
    std::size_t distance_count{};
    // Looks an address up in the event works, car work and parameters.
    const std::uint8_t* resolve(std::uint32_t pc,std::size_t n)const;
    std::uint8_t flags(std::uint32_t id)const{
        if(!event_flags_79fb48||id>=0x180)throw std::out_of_range("event flag 79FB48 outside view");
        return event_flags_79fb48[id];}
};

// One virtual per foreign PC callee (C argument order); values are the raw PC
// return registers. The last three are the audio platform boundary.
struct RaceSoundServices {
    virtual ~RaceSoundServices()=default;
    virtual std::int32_t pc_44dc50(std::uint32_t key)=0;            // stage number of a stage key
    virtual std::uint32_t pc_44c940(std::uint32_t key)=0;           // GetNowStageLevel
    virtual std::uint16_t pc_44dd60(std::uint32_t key,std::uint32_t side)=0;
    virtual std::uint16_t pc_44dd90(std::uint32_t key,std::uint32_t side)=0;
    virtual std::uint8_t pc_44b7b0(std::uint32_t arg)=0;           // course final-stage test (protected entry)
    virtual std::uint32_t pc_44b800()=0;                            // [7D30D4]==0 && [7D30D8]==0
    virtual std::uint32_t pc_450570()=0;                            // [7D3930] stage frames
    virtual std::uint8_t pc_48b310()=0;                             // byte [830394]
    virtual std::uint8_t pc_48b350()=0;                             // [656234] < 60
    virtual std::uint8_t pc_48b1c0()=0;                             // byte [83036C] sound balance option
    virtual std::uint8_t pc_43f9c0()=0;                             // byte [780248] pause flag
    virtual std::uint32_t pc_55a930()=0;                            // thiscall(0x7F9460) network active
    virtual void pc_42eff0(std::uint32_t channel,std::uint32_t value,std::uint32_t code)=0;
    virtual void pc_42f0d0(std::uint32_t command)=0;
    virtual void pc_427630()=0;
};

// ----- event 383 SOUND -------------------------------------------------------
void race_sound_init_424650(RaceSoundState&,RaceSoundWorld&,RaceSoundServices&);
void race_sound_control_424700(RaceSoundState&,RaceSoundWorld&,RaceSoundServices&);
void race_sound_destroy_424790(RaceSoundState&,RaceSoundServices&);
// 49A650 (display) is RET.

// ----- callees (public for the oracle) ---------------------------------------
void race_sound_flush_ics_424820(RaceSoundState&,RaceSoundServices&);
void race_sound_init_se_queue_424880(RaceSoundState&);
void race_sound_set_se_424940(RaceSoundState&,const RaceSoundWorld&,std::uint32_t command);
void race_sound_flush_se_4249a0(RaceSoundState&,RaceSoundServices&);
void race_sound_init_car_424a00(RaceSoundState&);
void race_sound_pause_clear_424b10(RaceSoundState&,RaceSoundServices&);
void race_sound_set_ics_4247b0(RaceSoundState&,std::uint32_t channel,std::uint32_t param,std::uint32_t value);
std::uint32_t race_sound_stage_424b50(const RaceSoundWorld&,RaceSoundServices&,std::uint32_t work_pc);
std::uint32_t race_sound_stage_424b90(const RaceSoundWorld&,RaceSoundServices&,std::uint32_t work_pc);
std::uint32_t race_sound_stage_424bf0(const RaceSoundWorld&,RaceSoundServices&,std::uint32_t work_pc);
std::uint32_t race_sound_stage_424c60(const RaceSoundWorld&,RaceSoundServices&,std::uint32_t work_pc);
void race_sound_cheer_targets_424c80(RaceSoundState&,RaceSoundWorld&,RaceSoundServices&);
void race_sound_cheer_424f80(RaceSoundState&,RaceSoundWorld&,RaceSoundServices&);
void race_sound_car_425100(RaceSoundState&,RaceSoundWorld&,RaceSoundServices&);
void race_sound_engine_4252e0(RaceSoundState&,std::uint32_t car_sound,std::uint32_t channel_a,std::uint32_t channel_b);
void race_sound_car_param_4256f0(RaceSoundState&,const RaceSoundWorld&,std::uint32_t car_sound,std::uint32_t work_pc);
void race_sound_road_425a40(RaceSoundState&,std::uint32_t car_sound,std::uint32_t channel_a,std::uint32_t channel_b);
void race_sound_skid_4260e0(RaceSoundState&,std::uint32_t car_sound,std::uint32_t channel);
void race_sound_wall_4264c0(RaceSoundState&,std::uint32_t car_sound,std::uint32_t channel);
void race_sound_near_cars_4267c0(RaceSoundState&,const RaceSoundWorld&,std::uint32_t car_sound,std::uint32_t player_pc,std::uint32_t pair);
void race_sound_enemy_426b10(RaceSoundState&,const RaceSoundWorld&,std::uint32_t channel,std::uint32_t car_sound,std::uint32_t slot,float range,float gain);
void race_sound_passing_426f50(RaceSoundState&,const RaceSoundWorld&,RaceSoundServices&,std::uint32_t player_pc,std::uint32_t car_sound);
void race_sound_environment_427110(RaceSoundState&,RaceSoundWorld&,RaceSoundServices&);
void race_sound_voices_427320(RaceSoundState&);
void race_sound_balance_4279c0(RaceSoundState&,const RaceSoundWorld&,RaceSoundServices&);
void race_sound_preview_427b10(RaceSoundState&,std::int8_t option,bool held);
std::uint32_t race_sound_volume_427a20(const RaceSoundState&,std::uint32_t channel,std::int32_t value);
// CRT _CIpow (5826D0) x87 path for x >= 0 finite (the domain of 4252E0);
// sets domain_fault for inputs the port does not reproduce (x < 0, inf, NaN).
driving::X87 race_sound_pow_5826d0(double x,double y,bool& domain_fault);

// ----- event 360 COMM_TRANS ----------------------------------------------------
// 45A280: if the manager 7D68AC has a session object (+0x58) whose state (+8)
// is neither 0 nor 7, tail-jumps to 45A0C0 (CommRace transfer, not ported).
struct CommTransWorld {
    bool manager_known{};          // the integrator knows [7D68AC] and its session
    std::uint32_t manager_7d68ac{};
    std::uint32_t session_58{};    // [[7D68AC]+0x58]
    std::uint32_t state_8{};       // [[[7D68AC]+0x58]+8]
};
enum class CommTransResult : std::uint8_t {Idle,Transfer45a0c0,Unknown};
CommTransResult comm_trans_control_45a280(const CommTransWorld&);
}
