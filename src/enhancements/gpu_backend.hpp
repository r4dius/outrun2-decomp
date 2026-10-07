#pragma once
// The VideoBackend of the GPU renderers that draw the PC scene into a back
// buffer of any size (macOS Metal, PS5 OpenGL, Xbox Direct3D 11): 16:9,
// resolutions by height (480 .. 2160, the width follows the aspect), FXAA and
// MSAA 2x / 4x, applied live through `push` and saved to options.ini.
#include "enhancements/video.hpp"
#include <functional>
#include <string>

namespace outrun::enhancements {
class GpuVideoBackend final : public VideoBackend {
public:
    // The renderer call: back buffer size, 16:9, MSAA samples (1, 2 or 4), FXAA.
    using Push=std::function<void(std::uint32_t width,std::uint32_t height,bool widescreen,unsigned msaa,bool fxaa)>;
    // options.ini path, rewritten on every change; the settings loaded from it.
    GpuVideoBackend(std::string options_path,const VideoSettings& start);
    // Frame rates the platform's display can show (60 only by default) and
    // the platform call that paces its presentation to one of them. With a
    // call the FRAME RATE row applies at once; without, at the next launch.
    using RatePush=std::function<void(std::uint32_t frames_per_second)>;
    void set_frame_rates(std::vector<std::uint32_t> rates,RatePush pace={});
    // The renderer once initialized (an empty function detaches it): the
    // settings are applied to it from then on.
    void attach(Push);
    bool widescreen_available()const override{return true;}
    std::vector<std::uint32_t> heights()const override{return {480,720,900,1080,1440,1800,2160};}
    std::vector<Antialiasing> antialiasing_modes()const override{return {Antialiasing::Off,Antialiasing::Fxaa,Antialiasing::Msaa2x,Antialiasing::Msaa4x};}
    std::vector<std::uint32_t> frame_rates()const override{return rates_;}
    VideoSettings current()const override{return settings_;}
    void apply(const VideoSettings&)override;
    bool needs_restart(const VideoSettings& s)const override{return !pace_&&s.frame_rate!=start_rate_;}
    VideoSettings supported(VideoSettings)const;   // a loaded setting made valid
    // options.ini keys: widescreen=0/1, resolution=WxH, antialiasing=off/msaa2/msaa4/fxaa, framerate=60/120.
    static bool parse_option(const std::string& key,const std::string& value,VideoSettings&);
private:
    void push()const;
    std::string path_;
    VideoSettings settings_;
    std::vector<std::uint32_t> rates_{60};
    std::uint32_t start_rate_{60},saved_rate_{60};   // the rate of this launch; the one loaded from options.ini
    Push push_;
    RatePush pace_;
};
}
