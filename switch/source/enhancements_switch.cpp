#include "enhancements_switch.hpp"
#include "switch_renderer.hpp"
#include <algorithm>

namespace outrun::switch_runtime {
namespace en=outrun::enhancements;
namespace {
// The scene rectangle of the 1280x720 frame is 1280 (16:9) or 960 (4:3) wide.
constexpr unsigned FrameHeight=720u;
}
std::vector<std::uint32_t> SwitchVideoBackend::heights()const{
    std::vector<std::uint32_t> list;
    for(unsigned p=50u;p<=100u;p+=10u)list.push_back(FrameHeight*p/100u);   // 360, 432, ... 720
    return list;
}
std::vector<en::Antialiasing> SwitchVideoBackend::antialiasing_modes()const{
    std::vector<en::Antialiasing> modes{en::Antialiasing::Off};
    if(!hooks_.fxaa_ready||hooks_.fxaa_ready())modes.push_back(en::Antialiasing::Fxaa);
    modes.push_back(en::Antialiasing::Msaa2x);modes.push_back(en::Antialiasing::Msaa4x);
    return modes;
}
en::VideoSettings SwitchVideoBackend::current()const{
    en::VideoSettings s;s.widescreen=g_widescreen;
    s.resolution=en::resolution_at(en::nearest_height(heights(),FrameHeight*std::clamp(g_render_scale_percent,50u,100u)/100u),g_widescreen);
    s.antialiasing=g_aa_mode<AaModeCount?en::Antialiasing(g_aa_mode):en::Antialiasing::Off;   // 0 off, 1/2 MSAA 2x/4x, 3 FXAA
    return s;
}
void SwitchVideoBackend::apply(const en::VideoSettings& s){
    g_widescreen=s.widescreen;
    const unsigned percent=std::clamp((s.resolution.height*100u+FrameHeight/2u)/FrameHeight,50u,100u);
    const bool docked=hooks_.docked&&hooks_.docked();
    if(unsigned* scale=docked?hooks_.docked_scale:hooks_.handheld_scale)*scale=percent;
    if(hooks_.scale_override)*hooks_.scale_override=0u;
    g_render_scale_percent=percent;
    g_aa_mode=unsigned(s.antialiasing);
    if(!hooks_.options_path.empty())
        en::write_options(hooks_.options_path,{{"widescreen",s.widescreen?"1":"0"},
            {"antialiasing",en::antialiasing_key(s.antialiasing)},
            {docked?"docked_scale":"handheld_scale",std::to_string(percent)}});
}
}
