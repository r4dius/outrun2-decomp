#pragma once
// PC event 387 SCN_EFC (scene effects: rain sheet, tunnel/night transition,
// headlight projector), function table entry 0x34 (59C288):
//   init 4AFBB0, control 4AFDC0, display 49A650 (RET), destroy 4AFC70.
// Work 780280 (0x1B4 bytes, [79F5EC] while the event is open):
//   +000 flags: 1 rain sheet, 2 transition, 4 (shadow pass, set by AREA),
//        8 headlights, 0x10 (set by AREA)
//   +010 rain sheet scale, +01C offset, +028 offset step, +040 matrix
//   +090 transition { +0 1, +4 target, +8 current, +C blend, +14 i16 timer }
//   +0B0 matrix
//   +130 headlight projection (+00 perspective, +40 view*projection,
//        +80 = 0x30009)
// The headlight parameters are the .bss block 842130..842198, filled by init
// from the debug table 5C4EA0 (EXE .rdata, embedded here and checked by the
// oracle). 4AF630 writes the two headlight spot lights (type 1, ids 0/1) of
// scene environments 1 and 2 in the light table 899B98 (89A138 block) with
// the light setters 407C30..4082B0 (type 1/2 paths only; the type 0 paths of
// 407D60/4082B0 jump into protected code and are never reached from here).
// All state is addressed by PC address through PcRaceMemory: the caller maps
// the work, 79F5EC, the parameter block, the light table, 799D18 and the
// player car fields (+5C, +B0..+F0). Nothing outside the module is called:
// every callee is ported (matrix stack, D3DX generic paths, FSIN/FCOS).
#include "platform/race_area.hpp"
#include <array>
#include <cstdint>
namespace outrun::platform {
struct PcSceneEffectsState {
    static constexpr std::uint32_t WorkBase=0x780280u,WorkSize=0x1b4u;
    static constexpr std::uint32_t ParamBase=0x842130u,ParamEnd=0x842198u;
    static constexpr std::uint32_t WorkPointer=0x79f5ecu;
    std::array<std::uint8_t,WorkSize> work{};
    std::array<std::uint8_t,ParamEnd-ParamBase> params{};
    std::uint32_t work_pointer_79f5ec{};         // event 387 work token (0 when closed)
    void map(PcRaceMemory& m){
        m.map(WorkBase,work.data(),work.size());
        m.map(ParamBase,params.data(),params.size());
        m.map(WorkPointer,reinterpret_cast<std::uint8_t*>(&work_pointer_79f5ec),4);
    }
};
// 5C4EA0: (name, target, value, step) records ending with a null name.
struct PcSceneEffectsParameter { std::uint32_t target,value; };
extern const std::array<PcSceneEffectsParameter,25> SceneEffectsParameters5c4ea0;
// Event callbacks; work is the event work address (780280).
void scene_effects_init_4afbb0(PcRaceContext&,std::uint32_t work);
void scene_effects_control_4afdc0(PcRaceContext&,std::uint32_t work);
void scene_effects_destroy_4afc70(PcRaceMemory&,std::uint32_t work);
// Parts.
void scene_effects_headlight_init_4af5b0(PcRaceContext&,std::uint32_t object);  // EAX = work+130
void scene_effects_headlights_4af630(PcRaceContext&,std::uint32_t object);      // EAX = work+130
void scene_effects_transition_4afcc0(PcRaceMemory&,std::uint32_t object);       // EAX = work+90
// Setters/getters used by the AREA (390), the player car (8, 4A25F0), the
// sky (391) and the shadow pass (4400C0), all through [79F5EC].
void scene_effects_4af550(PcRaceMemory&);   // [w+80] = 7FFFFFFF
void scene_effects_4af560(PcRaceMemory&);   // flags |= 1
void scene_effects_4af570(PcRaceMemory&);   // flags &= ~1
void scene_effects_4af580(PcRaceMemory&);   // flags |= 4 (protected snippet 4480BF reads [79F5EC])
void scene_effects_4af590(PcRaceMemory&);   // flags &= ~4
void scene_effects_4afb40(PcRaceMemory&);   // flags |= 8
void scene_effects_4afb50(PcRaceMemory&);   // flags &= ~8
std::uint32_t scene_effects_4afb60(const PcRaceMemory&); // (byte flags >> 3) & 1
void scene_effects_4afb70(PcRaceMemory&);   // flags |= 0x10
void scene_effects_4afb80(PcRaceMemory&);   // flags &= ~0x10
void scene_effects_4afd90(PcRaceMemory&,std::uint32_t kind); // transition request
std::uint32_t scene_effects_4af5a0(const PcRaceMemory&); // (byte flags >> 2) & 1 (shadow pass, 4400C0)
std::uint32_t scene_effects_4afb90(const PcRaceMemory&); // (byte flags >> 4) & 1 (player car 4A25F0)
void scene_effects_4afba0(PcRaceMemory&);   // flags &= ~2 (sky 452673)
// Dispatch of the PC entries above for services (returns false for other PCs).
bool scene_effects_setter(PcRaceMemory&,const PcRaceCall&,std::uint32_t& eax);
// Light setters 407C30..4082B0 for type != 0 (type 0 -> std::logic_error).
std::uint32_t scene_light_address_4082b0(std::uint32_t env,std::uint32_t type,std::uint32_t id);
}
