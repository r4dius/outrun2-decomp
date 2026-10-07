#pragma once
// Small PC leaves the arcade ending reaches (mode 24, AUTOSCENE, event 0x187), transliterated
// over PC addresses (PcRaceMemory) so the runtimes and the oracle share one implementation:
//   lights     407CF0 407D60 407DD0 407E40 407F40 (44A430) 408030 4080A0 408390 4083C0 on the
//              light records 899B98 (b 0: + (a + c) * 0xA0, b 1: 89A138 + (c + a * 2) * 0xA0,
//              else 899D78 + (c + a * 2) * 0xA0), 451800 on the fog records 7D3A10 + i * 0x1C
//   camera     483E10(&eye, &target, &roll, &fov) on the work [79F574]
//   robots     487D10(work, &matrix | 0) 487D40(work, on) 514800 / 514880(work, value, which)
//              (ROBDISPWORK 8577D8 + [work] * 0x60), 514830(work, model, frame) (its 4066D0 /
//              4103F0 calls through the service)
//   RobMotion  48F4E0 4ED890 4F1E30 (ECX = motion object)
#include "platform/race_area.hpp"
#include <cstdint>
namespace outrun::platform {
// One of the light / fog setters above (k.pc); false when k.pc is not one of them.
// 407F40's 44A430 pushes, rotates and pops on the given (PC) matrix stack.
bool ending_light_service(PcRaceMemory&,driving::PcMatrixStack&,const PcRaceCall&);
void camera_set_483e10(PcRaceMemory&,std::uint32_t eye,std::uint32_t target,std::uint32_t roll,std::uint32_t fov);
void robot_matrix_487d10(PcRaceMemory&,driving::PcMatrixStack&,std::uint32_t work,std::uint32_t matrix);
void robot_flag_487d40(PcRaceMemory&,std::uint32_t work,std::uint32_t on);
void rob_disp_word_514800(PcRaceMemory&,std::uint32_t work,std::uint32_t value,std::uint32_t which);
void rob_disp_vector_514880(PcRaceMemory&,std::uint32_t work,std::uint32_t vector,std::uint32_t which);
void rob_disp_model_514830(PcRaceContext&,std::uint32_t work,std::uint32_t model,std::uint32_t frame);
// 45BF30(n): the mean of the stage scores 7F2564 + i * 0x44 / max(count byte 7F2588 + i * 0x44, 1)
// (negative quotients 0) over the stages i <= n of five, divided by 5 (truncated).
std::uint32_t navi_score_mean_45bf30(PcRaceMemory&,std::uint32_t n);
std::uint32_t rob_motion_word_48f4e0(PcRaceMemory&,std::uint32_t motion);
void rob_motion_speed_4ed890(PcRaceMemory&,std::uint32_t motion,std::uint32_t speed);
void rob_motion_frame_end_4f1e30(PcRaceMemory&,std::uint32_t motion,float frame);
}
