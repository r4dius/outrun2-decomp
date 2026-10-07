#include "enhancements/settings_rows.hpp"
#include "enhancements/video.hpp"
#include "platform/frontend_title_widgets.hpp"
#include <algorithm>
#include <array>
#include <string>
#include <vector>

namespace outrun::enhancements {
using driving::Bytes;
using platform::FrontendTitleWidgets;
namespace {
// Layout of Options > Settings (frontend_title_widgets.cpp, EXE .data).
constexpr float field_x=321,field_y=161,row_step=16;
constexpr std::uint32_t label_color=0xff3f474a;
constexpr int field_width=0x97;
// The sprite labels (2C00F4 frames) are font 9's capitals, 10% taller, a
// pixel further left and up than a text row of this list would put them.
constexpr std::uint32_t LabelFont=9;
constexpr float LabelScaleX=1.f,LabelScaleY=1.1f,LabelX=-152,LabelY=-9;   // 4D7440 formats text rows at -150, -7

enum class Option { Aspect, Resolution, Antialiasing, FrameRate };
struct Rows {
    std::vector<Option> options;
    VideoSettings edit;
    std::array<std::array<std::uint8_t,0x8c>,4> values{};   // 4EDAF0 value labels
    Bytes value(std::size_t i){return {values[i].data(),values[i].size()};}
};
Rows& rows(){static Rows r;return r;}

std::vector<Option> available(const VideoBackend& b){
    std::vector<Option> o;
    if(b.widescreen_available())o.push_back(Option::Aspect);
    if(b.heights().size()>1)o.push_back(Option::Resolution);
    if(b.antialiasing_modes().size()>1)o.push_back(Option::Antialiasing);
    if(b.frame_rates().size()>1)o.push_back(Option::FrameRate);
    return o;
}
const char* label(Option o){return o==Option::Aspect?"ASPECT RATIO":o==Option::Resolution?"RESOLUTION":o==Option::FrameRate?"FRAME RATE":"ANTI-ALIASING";}
// The choices of a row and the index of the edited value among them.
std::vector<std::string> choices(const VideoBackend& b,const VideoSettings& s,Option o,int& index){
    std::vector<std::string> out;index=0;
    if(o==Option::Aspect){out={"4:3","16:9"};index=s.widescreen?1:0;}
    else if(o==Option::Resolution){
        const auto heights=b.heights();
        for(std::size_t i=0;i<heights.size();++i){
            out.push_back(resolution_label(resolution_at(heights[i],s.widescreen)));
            if(heights[i]==s.resolution.height)index=int(i);
        }
    }else if(o==Option::FrameRate){
        const auto rates=b.frame_rates();
        for(std::size_t i=0;i<rates.size();++i){out.push_back(std::to_string(rates[i])+" FPS");if(rates[i]==s.frame_rate)index=int(i);}
    }else{
        const auto modes=b.antialiasing_modes();
        for(std::size_t i=0;i<modes.size();++i){out.push_back(antialiasing_label(modes[i]));if(modes[i]==s.antialiasing)index=int(i);}
    }
    return out;
}
// One step of a row's value; false at either end (no wrap, as 446010 / 446050).
bool step(const VideoBackend& b,VideoSettings& s,Option o,bool next){
    int index{};const auto n=int(choices(b,s,o,index).size());
    const int to=index+(next?1:-1);if(to<0||to>=n)return false;
    if(o==Option::Aspect){s.widescreen=to==1;s.resolution=resolution_at(s.resolution.height,s.widescreen);}   // same height, width of the aspect
    else if(o==Option::Resolution)s.resolution=resolution_at(b.heights()[std::size_t(to)],s.widescreen);
    else if(o==Option::FrameRate)s.frame_rate=b.frame_rates()[std::size_t(to)];
    else s.antialiasing=b.antialiasing_modes()[std::size_t(to)];
    return true;
}
}

bool SettingsAccess::init(FrontendTitleWidgets& w){
    auto& r=rows();r.options.clear();
    const auto* backend=video_backend();if(!backend||!w.list_)return true;
    r.options=available(*backend);r.edit=backend->current();
    r.edit.resolution=resolution_at(nearest_height(backend->heights(),r.edit.resolution.height),r.edit.widescreen);
    const auto* blank=w.text_.get(0);if(!blank){w.missing_=0x4d7440;return false;}
    const auto& font=w.fonts_.fonts[9];const auto layer=w.layers_.scene_ids[2];
    // Only the added rows are text rows in this list: their offset from the list position (48DDA0).
    auto header=w.object_.sub(0x34,platform::PcFrontendChoiceListBytes);header.putf(0x45c,LabelX);header.putf(0x460,LabelY);
    const int first=int(w.list_->size());   // display rows of the original options (4, or 2 in the race pause)
    for(std::size_t i=0;i<r.options.size();++i){
        std::uint32_t index{};
        if(!w.list_->add_text_48db30(label(r.options[i]),w.previous_,index))return false;
        auto text=w.list_->row(index).sub(0x150,platform::PcTextWidgetBytes);   // 48F2D0 font / scale of the row widget
        text.put32(0x450,LabelFont);text.putf(0x478,LabelScaleX);text.putf(0x47c,LabelScaleY);
        if(!platform::frontend_scroll_text_init_4edaf0(r.value(i),font,*blank,int(field_x),
            int(float(int(field_y))+float(first+int(i))*row_step),0x83,label_color))return false;
        platform::frontend_scroll_text_layer_4f1ce0(r.value(i),layer);
    }
    return true;
}
bool SettingsAccess::display(FrontendTitleWidgets& w,int row){
    auto& r=rows();const auto* backend=video_backend();
    if(!backend)return true;
    const auto& font=w.fonts_.fonts[9];
    for(std::size_t i=0;i<r.options.size();++i,++row){
        int index{};const auto values=choices(*backend,r.edit,r.options[i],index);
        if(values.empty())continue;
        const int x=int(field_x),y=int(float(int(field_y))+float(row)*row_step);
        auto value=r.value(i);
        if(!platform::frontend_scroll_text_set_4edcf0(value,font,values[std::size_t(index)]))return false;
        std::array<std::uint8_t,platform::PcFrontendOptionBytes> option{};Bytes o(option.data(),option.size());
        platform::frontend_option_init_445fe0(o,0,std::int32_t(values.size())-1,0,w.layers_.scene_ids[2]);
        platform::frontend_option_set_446100(o,index);
        platform::frontend_option_arrows_446090(o,x,y,field_width,w.images_);
        platform::frontend_scroll_text_move_4edcc0(value,x,y);
        if(!platform::frontend_scroll_text_draw_4edb90(value,font,w.glyphs_))return false;
    }
    return true;
}
bool settings_step(std::size_t row,bool next,const std::function<bool()>& sound){
    auto& r=rows();auto* backend=video_backend();
    if(!backend||row>=r.options.size()||!step(*backend,r.edit,r.options[row],next))return true;
    backend->apply(r.edit);
    return sound();
}
}
