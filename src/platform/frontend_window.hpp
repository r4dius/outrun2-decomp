#pragma once
#include "frontend_list.hpp"
#include "title_owner.hpp"

namespace outrun::platform {
constexpr std::size_t PcFrontendWindowBytes=0x12b0;
// Original immediate 429530 icon submissions, separate from raw bank-3 images.
struct FrontendWindowIcon {std::uint32_t token{};float x{},y{};int layer{},frame{};};
bool frontend_window_init_48d0c0(driving::Bytes,const FrontendFontPack&,const FrontendTextTable&,
    FrontendTextLines&,std::string_view title,std::string_view body,std::uint8_t flags,
    float x,float y,float width,float height,unsigned layer,std::uint8_t decoration);
bool frontend_window_height_48cef0(driving::Bytes,const FrontendFontPack&,FrontendTextLines&,std::uint8_t center,int extra);
bool frontend_window_display_48c5f0(driving::Bytes,const FrontendFontPack&,FrontendTextLines&,
    std::vector<FrontendGlyph>&,std::vector<FrontendListImage>&,std::vector<FrontendWindowIcon>&);
void frontend_window_suspend_48ca30(driving::Bytes);
// Mutable layout globals 692BB4/B8 and 692C0C..1C. Defaults are retail data,
// not a new layout; callers can bind their live values.
struct TitleMenuLayout {
    float list_x{-13},list_y{-72};
    std::int32_t indent{50},window_y{110},window_x{135},width{350},height{140};
};
const std::vector<std::array<std::uint32_t,3>>& frontend_title_sprite_table();
bool frontend_title_init_4d5e40(driving::Bytes,FrontendChoiceList&,const FrontendFontPack&,
    const FrontendTextTable&,FrontendTextLines&,TitleMenuGlobals&,const TitleOwnerGlobals&,
    const TitleMenuLayout&,std::uint32_t& shared_input);
}
