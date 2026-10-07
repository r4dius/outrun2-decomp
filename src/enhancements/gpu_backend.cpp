#include "enhancements/gpu_backend.hpp"
#include "enhancements/frame_rate.hpp"
#include <algorithm>
#include <cstdlib>

namespace outrun::enhancements {
GpuVideoBackend::GpuVideoBackend(std::string path,const VideoSettings& start):path_(std::move(path)),saved_rate_(start.frame_rate){settings_=supported(start);}
void GpuVideoBackend::attach(Push push){push_=std::move(push);this->push();}
void GpuVideoBackend::set_frame_rates(std::vector<std::uint32_t> rates,RatePush pace){
    rates_=rates.empty()?std::vector<std::uint32_t>{60}:std::move(rates);settings_.frame_rate=saved_rate_;settings_=supported(settings_);
    pace_=std::move(pace);
    // Without a pacing call the display mode is chosen at launch: a new rate
    // applies at the next one.
    start_rate_=settings_.frame_rate;set_display_rate(start_rate_);
    if(pace_)pace_(start_rate_);}
VideoSettings GpuVideoBackend::supported(VideoSettings s)const{
    s.resolution=resolution_at(nearest_height(heights(),s.resolution.height),s.widescreen);
    const auto modes=antialiasing_modes();
    if(std::find(modes.begin(),modes.end(),s.antialiasing)==modes.end())s.antialiasing=Antialiasing::Off;
    if(std::find(rates_.begin(),rates_.end(),s.frame_rate)==rates_.end())s.frame_rate=rates_.front();
    return s;
}
void GpuVideoBackend::push()const{
    if(push_)push_(settings_.resolution.width,settings_.resolution.height,settings_.widescreen,
        settings_.antialiasing==Antialiasing::Msaa4x?4u:settings_.antialiasing==Antialiasing::Msaa2x?2u:1u,
        settings_.antialiasing==Antialiasing::Fxaa);
}
void GpuVideoBackend::apply(const VideoSettings& s){
    settings_=supported(s);push();
    if(pace_){set_display_rate(settings_.frame_rate);pace_(settings_.frame_rate);}
    if(!path_.empty())write_options(path_,{{"widescreen",settings_.widescreen?"1":"0"},
        {"resolution",resolution_key(settings_.resolution)},{"antialiasing",antialiasing_key(settings_.antialiasing)},
        {"framerate",std::to_string(settings_.frame_rate)}});
}
bool GpuVideoBackend::parse_option(const std::string& key,const std::string& value,VideoSettings& s){
    if(key=="widescreen"){s.widescreen=std::strtoul(value.c_str(),nullptr,10)!=0;return true;}
    if(key=="resolution")return parse_resolution(value,s.resolution);
    if(key=="antialiasing")return parse_antialiasing(value,s.antialiasing);
    if(key=="framerate"){const auto r=std::strtoul(value.c_str(),nullptr,10);if(r!=60&&r!=120)return false;s.frame_rate=std::uint32_t(r);return true;}
    return false;
}
}
