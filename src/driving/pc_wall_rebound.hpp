#pragma once
#include "driving/pc_wall_response.hpp"
namespace outrun::driving {
// 44C940: this is a cached record+8 value, not a bit-test. Do not invalidate
// automatically when the explicit record view changes: PC does not do so.
std::uint32_t pc_stage_property(const PcStageViews&,std::uint32_t id,Bytes cache);
struct PcRouteContext {
    Bytes choices;       // 7D39A0: at least 14 32-bit choices for normal indices
    Bytes selection;     // explicit 7D3188 pointee, fields +2C and +30
    Bytes save;          // explicit 7DE418 block; retained packed route at +364
    std::uint8_t slot;   // 7DD138, save record stride 6C
    std::uint32_t mode;  // 780258
    std::uint32_t clock; // 7F1938
    std::array<std::uint8_t,3> enabled; // 836374,830394,8361B4
};
std::uint32_t pc_pack_route(Bytes choices);                         // 4503D0
void pc_unpack_route(Bytes choices,std::uint32_t packed);           // 450490
void pc_save_route_progress(Bytes save,std::uint8_t slot,std::uint32_t packed,std::uint32_t clock); // 456820
void pc_set_route_choice(PcRouteContext&,std::int32_t index,std::uint32_t value); // 451140
std::uint32_t pc_get_route_choice(PcRouteContext&,std::int32_t index);           // 451350
struct PcSoundQueue {
    Bytes entries; // 32 u32 entries at 9563E8 (whole table participates in dedup)
    Bytes state;   // read position, write position (explicit 9560C0/956124)
    std::uint8_t control; // 79FCC7: same byte also read by 440A50(17F)
};
void pc_enqueue_sound(PcSoundQueue&,std::uint32_t command); // 424940, enqueue only, not playback
// 5033F0: alternate floating-point rebound heading, modifies event only.
void calc_rebound_heading_float(Bytes event,std::int16_t difference);
// 487790: normalized X/Z direction (ax-bx,az-bz). Output stores are ordered
// and may alias; a zero-length direction yields (0,1), not a fabricated wall.
void pc_direction_xz(float ax,float az,float bx,float bz,Bytes out_x,Bytes out_z);
struct WallReboundContext {
    WallResponseContext response;
    Bytes stage_cache;
    PcRouteContext route;
    PcSoundQueue sounds;
};
// 503A20: complete original rebound response, including route-cache/save and
// sound enqueue effects. Event, work and mutable context views are disjoint.
// All required views are checked before the first write. No serialized guest
// pointer is dereferenced. Does not load, push, pop or otherwise change matrices.
void cw_rebound_status(Bytes event,Bytes work,CourseProbe normal,WallReboundContext&);
}
