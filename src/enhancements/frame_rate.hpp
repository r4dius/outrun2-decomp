#pragma once
// Port enhancement (not in the PC game), after OutRun2006Tweaks by emoose:
// above 60 frames per second the game still ticks at 60 Hz (417C7B), and a
// frame without a tick draws the scene again. Cars and camera are then drawn
// between their last two tick positions through the game's own blend: 4493E0
// reads 634B34, a .data constant 1.0 on the PC, which display_blend() stands
// for. At 60 frames per second nothing differs from the PC.
#include <cstdint>
#include <functional>

namespace outrun::driving { class Bytes; struct PcMatrixStack; }
namespace outrun::enhancements {
// Frames shown per second: 60 (one per tick, as on the PC) or 120.
std::uint32_t display_rate();
void set_display_rate(std::uint32_t);
// display_rate() above 60: 417C7B may run frames without a tick.
bool display_frames();
// 634B34 for the display builders: 1.0 during ticks; after them, the time of
// the frame between the last two ticks (0 = previous tick, 1 = last tick).
float display_blend();

// 417C7B: before each tick (puts back what the last replays changed) and once
// after the ticks, before the scene is drawn, with the sub-tick time 0..1.
void display_before_tick();
void display_after_ticks(float blend,std::uint32_t ticks);
// Ticks run by this frame: 1 at 60 frames per second, 0 on a display-only
// frame. Display code that advances a counter once per frame (an original
// 60 Hz assumption) advances it only when this is not 0.
std::uint32_t display_ticks();
// A display builder a tick ran (4A2650 for a car, the camera view), redone
// after the ticks of each frame. `key` keeps one replay per builder and tick;
// `restore` (optional) runs before the next tick to put back values that tick
// code reads.
void display_note(const void* key,std::function<void()> replay,std::function<void()> restore={});
// 4A2650 (CalcDispMatrix) of `car`, redone with `blend()` on a scratch matrix
// stack. +D28, a world point the tick reads (collisions, forces, goal), keeps
// the tick's value.
void display_note_car(driving::Bytes car,std::function<float()> blend,std::uint8_t scene_code);
// A scratch matrix stack for replays.
driving::PcMatrixStack& display_matrices();
}
