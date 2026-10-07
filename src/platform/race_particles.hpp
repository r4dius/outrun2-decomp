#pragma once
// PC event 397 PART_EFC (particle effects), function table entry 0x1B
// (59C094): init 41BBF0, control 41BC60, display 41BD10, destroy 41BD40
// (= 49A650, RET). Symbol names from the Lindbergh build of the engine:
//   41BBF0 ParticleEfc_Init: nlParticleInit (11 sources 8A8D18 from the
//     template 74E8E0), tire_mark_init 41FB10 (+ 486220), PcTireSmokeInit
//     41C420, then by the root mode (78026C == 0x18: 420470) or the race
//     set: spark 41D6B0, gravel 41DE20, grass 41E360, water 41E910, misc
//     41EE40, backfire 41F4B0, 420020, 420200.
//   41BC60 ParticleEfc_Ctrl: MakePlcarParam 41BD50, MakeOccarParam 41C190,
//     the requests (spark 41D810, gravel 41DF00, grass 41E440, water 41E9F0,
//     misc 41EF00, backfire 41F5F0, PcTireSmoke 41C570, OcTireSmoke 41C940,
//     tire_mark_req 41FB50) gated by the root mode / event 8 suspension /
//     network state, CtrlGlowColor 4160F0 and nlParticleCtrl (418540 per
//     active source).
//   41BD10 ParticleEfc_Disp: when 74FE7C != 0, render state save 41B5C0,
//     tire_mark_disp 486450, restore 41B7A0, save, nlParticleDraw 418490,
//     restore.
// Memory: the module .data 74E8E0..750080 (EXE image, mutated), its .bss
// 8A8CE4..955A30, the glow colours 8A8C18..8A8C5C, 95AF4C..95AFBC and the
// tire-mark part (486xxx: .data 64FE1C..64FE58, .bss 819638..82E7C0) live in
// PcParticleState. 95AF0C is the runtime frame counter (417xxx), read only. Everything else (car works through 799D18 /
// 799D54, event flags 79FB48, the light table 899B98, the camera 79F574,
// resource entries 7C2800, 82E7C0.., 95E028.. of the 427xxx module, ...) is
// read and written by PC address through PcRaceMemory, mapped by the caller;
// an unmapped address throws PcRaceUnmapped (nothing is invented).
// Callees outside the module go through PcRaceContext::service
// (PcRaceCall), listed in race_particles.cpp (ParticleServices).
#include "platform/race_area.hpp"
#include <array>
#include <cstdint>
#include <vector>
namespace outrun::platform {
struct PcParticleState {
    static constexpr std::uint32_t DataBase=0x74e8e0u,DataEnd=0x750080u;
    static constexpr std::uint32_t BssBase=0x8a8ce4u,BssEnd=0x955a30u;
    static constexpr std::uint32_t GlowBase=0x8a8c18u,GlowEnd=0x8a8c5cu;
    static constexpr std::uint32_t MiscBase=0x95af4cu,MiscEnd=0x95afc4u;
    static constexpr std::uint32_t TireDataBase=0x64fe1cu,TireDataEnd=0x64fe58u;
    static constexpr std::uint32_t TireBssBase=0x819638u,TireBssEnd=0x82e7c0u;
    static constexpr std::uint32_t OnceBase=0x98abccu;        // 41BD50 mode-16 one-shot flag
    std::array<std::uint8_t,4> once_98abcc{};
    std::vector<std::uint8_t> data;                           // EXE .data image
    std::vector<std::uint8_t> bss=std::vector<std::uint8_t>(BssEnd-BssBase);
    std::array<std::uint8_t,GlowEnd-GlowBase> glow{};
    std::array<std::uint8_t,MiscEnd-MiscBase> misc{};
    std::vector<std::uint8_t> tire_data;                      // EXE .data image
    std::vector<std::uint8_t> tire_bss=std::vector<std::uint8_t>(TireBssEnd-TireBssBase);
    PcParticleState();
    void map(PcRaceMemory& m){
        m.map(DataBase,data.data(),data.size());m.map(BssBase,bss.data(),bss.size());
        m.map(GlowBase,glow.data(),glow.size());m.map(MiscBase,misc.data(),misc.size());
        m.map(TireDataBase,tire_data.data(),tire_data.size());m.map(TireBssBase,tire_bss.data(),tire_bss.size());
        m.map(OnceBase,once_98abcc.data(),once_98abcc.size());
    }
};
// 41B5C0 / 41B7A0: the render states saved in the particle bss (state,
// slot); the stage 0..3 textures go to 8A92D0..8A92DC.
inline constexpr std::uint32_t ParticleSavedStates41b5c0[19][2]={{0x16,0x8a92c8u},{0x18,0x8fa320u},{0x1b,0x8d1300u},{0x0f,0x8de308u},
    {0xab,0x8a92ecu},{0x13,0x8fa328u},{0x14,0x8bc2f0u},{0x07,0x8a92e4u},{0x0e,0x8d12f8u},{0x89,0x8de318u},{0x1c,0x8de314u},
    {0xa8,0x8a92c4u},{0x1d,0x8a92ccu},{0x9c,0x8d12fcu},{0x9d,0x8de310u},{0x9a,0x8a92e0u},{0x9e,0x8de30cu},{0x9f,0x8fa324u},{0xa0,0x8a92e8u}};
extern std::uint8_t RaceParticleDataImage[0x17a0];   // EXE .data 74E8E0..750080
extern std::uint8_t RaceTireMarkDataImage[0x3c];      // EXE .data 64FE1C..64FE58
extern std::uint8_t RaceParticleRdataImage[0x16d8];   // EXE .rdata 622528..623C00
void particles_map_tables(PcRaceMemory&);                   // maps the .rdata tables (read-only)
// Pseudo PCs of COM calls reported through the service (vtable offsets):
// IDirect3DTexture9::GetLevelDesc(texture, level, desc) with desc a PC
// address (PcParticleLocalDesc) the service fills through PcRaceMemory.
constexpr std::uint32_t PcParticleGetLevelDesc=0x10000044u;
constexpr std::uint32_t PcParticleLocalDesc=0x7fff0100u;  // 0x20-byte D3DSURFACE_DESC
// Pseudo PC of the renderer-global writes of 41B7A0 (89EDE0/89EDE8 = 0):
// service args {address, value}.
constexpr std::uint32_t PcParticleRenderGlobal=0x10000100u;
// IDirect3DDevice9 subset of the display, in PC call order (vtable slot).
class PcParticleDevice {
public:
    virtual ~PcParticleDevice()=default;
    virtual std::uint32_t get_render_state(std::uint32_t state)=0;                           // 58
    virtual void set_render_state(std::uint32_t state,std::uint32_t value)=0;                // 57
    virtual std::uint32_t get_texture(std::uint32_t stage)=0;                                // 64
    virtual void set_texture(std::uint32_t stage,std::uint32_t texture)=0;                   // 65
    virtual std::uint32_t get_texture_stage_state(std::uint32_t stage,std::uint32_t type)=0; // 66
    virtual void set_texture_stage_state(std::uint32_t stage,std::uint32_t type,std::uint32_t value)=0; // 67
    virtual std::uint32_t get_sampler_state(std::uint32_t sampler,std::uint32_t type)=0;     // 68
    virtual void set_sampler_state(std::uint32_t sampler,std::uint32_t type,std::uint32_t value)=0;     // 69
    virtual void set_transform(std::uint32_t state,const float* matrix)=0;                   // 44
    virtual void draw_primitive_up(std::uint32_t type,std::uint32_t count,const std::uint8_t* data,std::uint32_t stride)=0; // 83
    virtual void set_fvf(std::uint32_t fvf)=0;                                               // 89
    virtual void set_vertex_shader(std::uint32_t shader)=0;                                  // 92
    virtual void set_pixel_shader(std::uint32_t shader)=0;                                   // 107
    virtual void release(std::uint32_t object)=0;                                            // IUnknown::Release
};
// The CRT rand() state (holdrand of the thread data, 585BEC()+0x14) and the
// display device (display only).
class PcD3D9Device;
struct PcParticleContext {
    PcRaceContext& race;
    std::uint32_t& crt_random;
    PcParticleDevice* device{};
    PcD3D9Device* d3d{};          // the device behind `device`, for the translated draw paths
};
void particles_init_41bbf0(PcParticleContext&);
// 41FB10 + 41C420 (tire marks, tire smoke) outside the init.
void particles_tire_reset_41fb10_41c420(PcParticleContext&);
void particles_control_41bc60(PcParticleContext&);
void particles_display_41bd10(PcParticleContext&);
// Parts (exposed for the probes).
std::uint32_t particles_get_work_418420(PcParticleContext&,std::uint32_t id);   // VM entry, measured
void particles_born_418350(PcParticleContext&,std::uint32_t particle);
// 4208A0(count, &pos, colour) with the position words by value (the AUTOSCENE smoke node).
void particles_smoke_emit_4208a0(PcParticleContext&,std::uint32_t count,std::uint32_t x,std::uint32_t y,std::uint32_t z,std::uint32_t colour);
// 420560 (the arcade ending's falling pieces: twelve type-10 particles in front of the camera).
void particles_flare_420560(PcParticleContext&);
void particles_source_ctrl_418540(PcParticleContext&,std::uint32_t buffer,std::uint32_t source);
std::uint32_t particles_colour_scale_41ff70(PcParticleContext&,std::uint32_t colour,float a,float r,float g,float b);
// Dispatch of a PC function pointer stored in particle/source memory
// (unported pointers are reported through the service).
void particles_call(PcParticleContext&,std::uint32_t pc,std::uint32_t a0,std::uint32_t a1);
}
