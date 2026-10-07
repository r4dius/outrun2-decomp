#pragma once
// Port enhancements in Options > Settings (not in the PC game): rows after the
// original four of 4D7440 / 4D86F0 / 4D60C0 for the platform's VideoBackend
// (video.hpp): ASPECT RATIO, RESOLUTION, ANTI-ALIASING and FRAME RATE, each only when the
// platform offers a choice. They use the screen's own list, value font, colour
// and arrows; the labels are list text rows styled like the sprite labels.
// Every value is applied (and saved) as soon as it changes. Without a backend
// the screen is the original one.
#include <cstddef>
#include <functional>

namespace outrun::platform { class FrontendTitleWidgets; }
namespace outrun::enhancements {
struct SettingsAccess {
    // End of 4D7440: the added list rows and their value labels.
    static bool init(platform::FrontendTitleWidgets&);
    // End of 4D60C0: the added rows' values and arrows, from display row `row` on.
    static bool display(platform::FrontendTitleWidgets&,int row);
};
// 4D86F0 Left / Right on an added row (`row` 0 = the first added row); `sound`
// is 4249F0(1), played when the value moved.
bool settings_step(std::size_t row,bool next,const std::function<bool()>& sound);
}
