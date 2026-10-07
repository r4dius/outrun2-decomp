#pragma once
// Port platform option: the half width (in 480-line PC pixels) of the view the
// game draws. 320 is the PC's 4:3 screen (6281C8 / 5DA2D4); the widescreen
// option (Y) sets 240*16/9 so the screen-bound visibility tests (4F02D0) keep
// objects until they leave the wider view.
#include <string>
namespace outrun::platform {
inline float g_pc_view_half_width=320.0f;
// Port performance overlay (Minus): lines separated by '\n', drawn by the 2D
// flush 42D710 on top of the frame with the game's font 9. Empty = hidden.
inline std::string g_pc_overlay_text;
// Port option (options.ini car_shadows=0): the car stencil shadow volumes (422550 /
// 422740) are not drawn. Console bisect of the GPU cost of the car select screens.
inline bool g_pc_car_shadows=true;
// Port optimisation: a car draw list without the darkening leaf 422740 (the select
// screens' 46A560 calls 422550 twice and never 422740, checked in the EXE: the only
// 422740 calls are 4698D9 and 46A1AA) only fills a stencil nothing reads, so its
// volumes are skipped. Nothing on screen changes; on Switch those volumes cost about
// 45 ms of GPU per frame in the OutRun2SP car select.
inline bool g_pc_skip_unread_shadow_volumes=true;
// Port measurement: the draw group being recorded (0 = none), used by the Switch
// device to time each group's draws for the performance overlay. 1 tire marks,
// particles by draw function: 2 418CF0, 3 419DC0, 4 41A6F0, 5 41AE40, 6 others.
inline unsigned g_pc_gpu_zone=0;
}
