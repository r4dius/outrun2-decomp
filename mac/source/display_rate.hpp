#pragma once
// The refresh of the screen the game shows on (Options > Settings FRAME RATE,
// src/enhancements/frame_rate.hpp): 120 on ProMotion and high-refresh
// displays, 60 elsewhere. Before the window exists: the main screen.
namespace outrun::mac_runtime {
unsigned screen_max_frames_per_second();
}
