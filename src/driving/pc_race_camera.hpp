#pragma once
// Race camera paths of the CAMERA event (385, function 0x0D control 485FE0)
// for PC game mode 78026C == 16: 485250 (start view), 4858D0 (race view,
// offline) and their helpers. Car is the event-8 work 799D18, camera the
// event-385 work (79F574 points to it). Tables are the live copies built by
// 484EE0: 818FE8 (view entries 0x34), 818BB0 (per model 0x24) and 813750
// (per view 2..21 and model, 0x24).
#include "driving/pc_camera.hpp"
#include "driving/pc_course_world.hpp"
#include <cstdint>
#include <functional>
namespace outrun::driving {
struct PcRaceCameraInputs {
    Bytes car{nullptr,0};                 // 799D18
    Bytes live_818fe8{nullptr,0},live_818bb0{nullptr,0},live_813750{nullptr,0};
    std::uint16_t timer_8367bc{};         // 49B2D0 (protected getter, measured)
    std::uint8_t byte_7c24b8{};
    std::uint32_t network_7f9460_60{};    // 55A930(7F9460)
    std::int32_t game_variant_780258{};
    std::uint32_t steering_82e7ec{};      // 487310
    std::uint32_t feature_mask_7d6778{};  // 4536F0 player-0 mask
    std::uint8_t byte_780270{};           // 43F9F0
    // 43EB60(0x100, point, 0, 0, 0) in 485590: only point.y changes.
    std::function<void(CourseProbe& point)> ground_43eb60;
    float network_shake_800ad0{};         // 46C480: the LAN shake amount (fld [800AD0])
    std::uint32_t* crt_random_580f40{};   // CRT rand() state (shake jitter, demo target jitter)
    std::int32_t demo_round_83daf4{};    // 49EED0: the attract loop step (mode 3 dispatch)
    // 4B6F40: the demo camera record ([7F1958] + [842860] * 0x44); empty when [7F1958] is null.
    Bytes demo_record_4b6f40{nullptr,0};
    Bytes area_matrix_7d2da0{nullptr,0},area_matrix_7d3190{nullptr,0};   // 44BEA0 / 44BEC0
};
// 483110 (EBX camera, ESI car): yaw of the view.
float race_camera_yaw_483110(Bytes camera,Bytes car);
// 484810 (EAX camera, ECX car): car matrices +280/+200/+240; the protected
// entry bridge selects the 818FE8 entry of view +34A (measured: ESI = ECX,
// EDI = (i8)+34A * 0x34).
void race_camera_car_matrices_484810(Bytes camera,Bytes car,PcMatrixStack&,Bytes live_818fe8);
// 484DF0: pitch/yaw angles +128/+12C from eye/target.
void race_camera_angles_484df0(Bytes camera);
// 4833E0 (EAX camera, CX): previous-state save and +33C/+368 counters.
void race_camera_save_4833e0(Bytes camera,std::uint16_t cx);
// 4832F0 (ESI camera, EDI car): view-change timers from the car flags +2F0.
void race_camera_view_timers_4832f0(Bytes camera,Bytes car,std::uint32_t network_7f9460_60,std::int32_t game_variant_780258);
// 484A40 (ESI camera): view switching (input 0x40000).
void race_camera_view_switch_484a40(Bytes camera,std::uint32_t feature_mask_7d6778,std::uint8_t byte_780270);
// 485590 (EAX car; camera, entry, frames): eye/look position of the view.
void race_camera_eye_look_485590(Bytes car,Bytes camera,Bytes entry,std::int32_t frames,
    PcMatrixStack&,const PcRaceCameraInputs&);
// 485250 / 4858D0 (ESI camera). In a LAN session (55A930 != 0) 4858D0 adds the
// shake: eye pushed back by 46C480 and the random jitter of +36C..+380.
void race_camera_start_485250(Bytes camera,PcMatrixStack&,PcCameraDevice&,const PcCameraScreen&,
    const PcCameraBlend&,const PcRaceCameraInputs&);
void race_camera_race_4858d0(Bytes camera,PcMatrixStack&,PcCameraDevice&,const PcCameraScreen&,
    const PcCameraBlend&,const PcRaceCameraInputs&);
// 485300 (variants 3/4, countdown above 180): the start view in world space
// (+280/+200/+240 identity, +D4 zero), then 485250's eye/target and view switch.
void race_camera_start_lan_485300(Bytes camera,PcMatrixStack&,PcCameraDevice&,const PcCameraScreen&,
    const PcCameraBlend&,const PcRaceCameraInputs&);
// 485B40 (mode 3, the attract demo): without a demo record, view 2 behind the car;
// record kind 1, the record's race view; kind 0, a fixed point of the course area
// looking at the car with a FOV from the distance.
void race_camera_demo_485b40(Bytes camera,PcMatrixStack&,PcCameraDevice&,const PcCameraScreen&,
    const PcCameraBlend&,const PcRaceCameraInputs&);
// 4853D0 (ESI camera): the scripted views of modes 19, 27, 34, 35 (485FE0
// table 4861F8): car-space eye/target +E0/+EC (486EF0) through the car
// matrix +280; +D4 = the eye.
void race_camera_goal_4853d0(Bytes camera,PcMatrixStack&,PcCameraDevice&,const PcCameraScreen&,
    const PcCameraBlend&,const PcRaceCameraInputs&);
// 485470 (mode 24, the arcade ending): eye +F8 = +E0 (also +D4), target +104 = +EC (both set by
// the AUTOSCENE through 483E10), +34C = 1, 484DF0 angles, then 484BD0's projection.
void race_camera_timeover_485830(Bytes c,PcMatrixStack& s,PcCameraDevice& d,const PcCameraScreen& screen,
    const PcCameraBlend& blend,const PcRaceCameraInputs& in);
void race_camera_ending_485470(Bytes camera,PcMatrixStack&,PcCameraDevice&,const PcCameraScreen&,const PcCameraBlend&);
// 485FE0 mode-16 dispatch after the common head (+94/+98/+9C zero):
// timer > 180 -> 485250 (485300 in variants 3/4), else 4858D0.
void race_camera_mode16(Bytes camera,PcMatrixStack&,PcCameraDevice&,const PcCameraScreen&,
    const PcCameraBlend&,const PcRaceCameraInputs&);
}
