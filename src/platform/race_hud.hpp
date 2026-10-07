#pragma once
// GAME-mode (mode 16) race HUD / 2D overlay events of a C2C mission race.
//
//  event 398 SPRANI (function 0x1A): the global sprite pool (FrontendSprites).
//      init 427E30, control 427F70 (FrontendSprites::tick), display 428170.
//  event 389 GAD_PUB: init 4970F0, control 498590, display 4998C0 (RET in
//      mode 16), destroy 497210 (-> 428600).
//  event 388 NAVI_PUB: see the second half of this header.
//
// PC globals are explicit native state. Draws are recorded, not rendered:
// 428170's 429460 submissions carry the matrix 429010 leaves on the matrix
// stack. Callees that are not ported are reported through `missing` (the PC
// address of the first unported callee), never replaced by invented results.
#include "driving/pc_matrix_stack.hpp"
#include "platform/frontend_sprites.hpp"
#include <array>
#include <cstdint>
#include <vector>
namespace outrun::platform {
// ---------------------------------------------------------------- event 398
struct SpraniGlobals {
    std::int32_t current_7551b4{-1};   // last drawn instance +0x2C
    std::uint32_t paused_95b214{};     // pause flag / allocation pause domain
    std::uint32_t flag_986b28{};       // 428170/428A10 clear it before a draw
};
// One 429460(root component, frame, 1.0f, layer) submission.
struct SpraniDraw {
    std::uint32_t handle{},token{},layer{};   // handle ~0u: immediate 428A10 draw
    float frame{},scale{1.0f};                // 429460 frame and third argument
    std::array<float,16> matrix{};     // matrix-stack top left by 429010
    std::int32_t id_7551b4{-1};
    std::uint8_t depth{};              // 428AF0: 1 = 9564F4, 2 = 9564F8 set around the 429460 call
};
void sprani_init_427e30(FrontendSprites&,SpraniGlobals&);
// 427F70 is FrontendSprites::tick with clock {740CA4!=0, 8A8CDC>=8A8CA8, 95B214!=0}.
inline FrontendSpriteClock sprani_clock_427f70(std::uint32_t sixty_740ca4,std::int32_t frame_8a8cdc,
                                               std::int32_t limit_8a8ca8,std::uint32_t paused_95b214){
    return {sixty_740ca4!=0u,frame_8a8cdc>=limit_8a8ca8,paused_95b214!=0u};
}
// 429010: top = T(-w/2,-h/2) * instance * T(w/2,h/2) * T(320-w/2,240-h/2);
// also 9564E0 = 0, 9564E4 = 9564E8 = 1.0f (returned in `blend`).
void sprani_matrix_429010(driving::PcMatrixStack&,float canvas_width,float canvas_height,
                          const std::array<float,16>& instance,std::array<float,3>& blend_9564e0);
// 428170. Mode-4 instances are released after their draw. The PC null root
// component (+0x74 == 0) return cannot occur: native instances always have one.
void sprani_display_428170(FrontendSprites&,SpraniGlobals&,driving::PcMatrixStack&,
                           std::vector<SpraniDraw>&,std::array<float,3>& blend_9564e0);
// 428A10 (ecx token, esi optional matrix M): immediate SPRANI scene draw used by
// 429530/429580/4289B0. Resolves the bank scene (no draw when it does not
// exist), clears 986B28, pushes the matrix stack, builds 428EA0
//   top = T(-w/2,-h/2) * S(s,s,1) * Rz(angle) * T(w/2,h/2) * T(320-w/2,240-h/2)
// (9564E0 = angle, 9564E4 = 9564E8 = s), then top = M * top, records the
// 429460(root, float(frame), scale, layer) submission and pops the stack.
bool sprani_draw_428a10(const FrontendSprites&,SpraniGlobals&,driving::PcMatrixStack&,std::uint32_t token,
                        const std::array<float,16>* matrix,std::uint32_t layer,std::int32_t frame,float scale,
                        float s,float angle,std::vector<SpraniDraw>&,std::array<float,3>& blend_9564e0);
// 428AF0(token in ecx, matrix in ebx, frame, 1.0, s, angle, mode): 428A10 at layer 0 with the
// 9564F4 (mode 0) / 9564F8 (else) flag of the 3D sprites (4295D0).
bool sprani_draw_428af0(const FrontendSprites&,SpraniGlobals&,driving::PcMatrixStack&,std::uint32_t token,
                        const std::array<float,16>& matrix,std::int32_t frame,float s,float angle,std::uint32_t mode,
                        std::vector<SpraniDraw>&,std::array<float,3>& blend_9564e0);
// D3DXMatrixTranslation(x, y, 0) as 429530/429580 build it.
std::array<float,16> sprani_translation(float x,float y);

// ------------------------------------------------ navi voice queue (45BD30)
// Shared by the mission manager and NAVI_PUB: 8 requests of 12 bytes at
// 7F1998 {id, duration, int16 priority}, tail 7F1994, head 7F1C64 and the
// start time 7F8B40 (4AF500 race clock 842110 through _ftol2 582194).
struct NaviVoiceEntry {std::int32_t id{},duration{};std::int16_t priority{};std::uint16_t pad{};};
struct NaviVoiceQueue {
    std::int32_t tail_7f1994{},head_7f1c64{};
    std::array<NaviVoiceEntry,8> entries_7f1998{};
    std::uint32_t last_7f8b40{};
};
struct NaviVoiceServices {
    float time_842110{};                     // 4AF500
    std::uint32_t busy_7f94c0{},busy_7f95c4{}; // 46C530: [7F9460+0x60] ? [7F95C4] : 0
    std::vector<std::uint32_t>* sounds_424940{}; // ordered 424940(id) requests
};
std::uint32_t pc_ftol2_582194(float);
// 45BD30(id, duration, priority): returns the original EAX (0 or 1).
std::uint32_t navi_voice_request_45bd30(NaviVoiceQueue&,const NaviVoiceServices&,std::int32_t id,
                                        std::int16_t duration,std::int16_t priority);
void navi_voice_pump_45bcd0(NaviVoiceQueue&,const NaviVoiceServices&);

// ---------------------------------------------------------------- event 389
// The GAD_PUB globals as PC bytes: .bss 836630..836718 and .data
// 67EE38..67EE50 (EXE image: 67EE3A = 1, 67EE3C = 1, the Miles digit
// position 67EE40/44/48 = 540, 210, 32, 67EE4C = -1). The race end displays
// (race_end_modes) reach them through PcRaceMemory (map()).
struct GadPubState {
    static constexpr std::uint32_t Base=0x836630u,Size=0xe8u,DataBase=0x67ee38u,DataSize=0x1cu;
    // 67EF60..67EF70: .data -1, 5, 0x1E (4979E0 rows), 67EF6C / 67EF6D voice flags = 1.
    static constexpr std::uint32_t Data2Base=0x67ef60u,Data2Size=0x10u;
    std::array<std::uint8_t,Data2Size> data2{{0xff,0xff,0xff,0xff, 0x05,0x00,0x00,0x00, 0x1e,0x00,0x00,0x00, 0x01,0x01,0x00,0x00}};
    std::array<std::uint8_t,Size> block{};
    std::array<std::uint8_t,DataSize> data{{0xf3,0x01,0x01,0x00, 0x01,0x00,0x00,0x00, 0x00,0x00,0x07,0x44,
                                            0x00,0x00,0x52,0x43, 0x00,0x00,0x00,0x42, 0xff,0xff,0xff,0xff}};
    GadPubState();                                // sprite handles 836640.. and 836690/836694 start at -1
    std::uint8_t* at(std::uint32_t a,std::size_t n);
    const std::uint8_t* at(std::uint32_t a,std::size_t n)const{return const_cast<GadPubState*>(this)->at(a,n);}
    std::uint32_t u32(std::uint32_t a)const;
    std::int32_t i32(std::uint32_t a)const{return std::int32_t(u32(a));}
    void put32(std::uint32_t a,std::uint32_t v);
    void put16(std::uint32_t a,std::uint16_t v);
    void put8(std::uint32_t a,std::uint8_t v){*at(a,1)=v;}
    std::uint8_t u8(std::uint32_t a)const{return *at(a,1);}
};
void gad_pub_init_4970f0(GadPubState&);
// 498590. Mode 16: release the 836640..836650 sprites whose status is 3,
// then 4EF410(0.0f) stores 84BD00. Modes 0x13/0x15/0x16 test 495B00
// (780258 == 4): the LAN results branch 4985E4 (65A7A4, the owner state 564C90, the
// rating 498410 into 84BD00, the online upload 494140) runs through `lan_results`:
// 0 = take the 4EF410(0) tail, 1 = return keeping 84BD00, -1 = not run (missing =
// 0x4985E4). Other variants take the tail.
bool gad_pub_control_498590(GadPubState&,FrontendSprites&,std::uint32_t mode_78026c,std::uint32_t variant_780258,
                           float& value_84bd00,std::uint32_t& missing,int (*lan_results)(void*)=nullptr,void* lan_user=nullptr);
// 4998C0: true when the mode's dispatch is the plain RET (mode 16 and the other
// table-9/table-1 modes); other modes are reported missing (their routine).
bool gad_pub_display_4998c0(std::uint32_t mode_78026c,std::uint32_t& missing);
// 497210 -> 428600: FrontendSprites::clear_allocations (no 7551B4/95B214 writes).
inline void gad_pub_destroy_497210(FrontendSprites& pool){pool.clear_allocations();}
}
