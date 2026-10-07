#pragma once
// Enhancement rows of Options > Settings on the Switch (src/enhancements/):
// 16:9, the 3D render resolution and the antialiasing of switch_renderer, all
// applied live. The frame is always 1280x720 (the console scales it when
// docked), so a resolution is the 3D scene's size inside it (height 360..720):
// the render scale percent of handheld_scale / docked_scale (50..100) for the
// current mode.
#include "enhancements/video.hpp"
#include <functional>
#include <string>

namespace outrun::switch_runtime {
class SwitchVideoBackend final : public enhancements::VideoBackend {
public:
    struct Hooks {
        std::string options_path;           // options.ini, rewritten on every change
        unsigned* handheld_scale{};         // options.ini percents, read each frame by the caller
        unsigned* docked_scale{};
        unsigned* scale_override{};         // the LS hotkey's override, cleared by a menu choice
        std::function<bool()> docked;       // current operation mode
        std::function<bool()> fxaa_ready;   // the FXAA pass could be built
    };
    explicit SwitchVideoBackend(Hooks hooks):hooks_(std::move(hooks)){}
    bool widescreen_available()const override{return true;}
    std::vector<std::uint32_t> heights()const override;
    std::vector<enhancements::Antialiasing> antialiasing_modes()const override;
    enhancements::VideoSettings current()const override;
    void apply(const enhancements::VideoSettings&)override;
private:
    Hooks hooks_;
};
}
