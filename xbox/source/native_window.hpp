#pragma once
// The one game window: a Win32 window in the PC build (window_win32.cpp),
// the application's CoreWindow on Xbox (window_uwp.cpp). Both run
// or2_xbox_main (main.cpp) on the thread that owns the window.
#include <string>
namespace outrun::xbox_runtime {
struct NativeWindow {
    void* hwnd{};          // HWND (PC build)
    void* core_window{};   // ABI ICoreWindow* (UWP build)
    unsigned width{},height{};   // swap chain size in pixels
};
NativeWindow& native_window();
// Dispatches the pending window events; false once the window was closed.
bool pump_window_events();
// The folder the package or the executable was started from, and the
// folders the game data may be in, in search order (Xbox: removable drives,
// then the application's LocalState).
std::string launch_folder();
std::string local_state_folder();
// Switches the output to a refresh rate of at least `hz` when it offers one
// (Xbox: an HDMI mode at the current resolution; PC: the monitor's current
// rate is kept). Returns the rate the output runs at afterwards.
unsigned select_refresh_rate(unsigned hz);
}
int or2_xbox_main(int argc,char** argv);
