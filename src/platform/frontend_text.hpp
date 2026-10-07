#pragma once
#include "driving/pc_driving.hpp"
#include "platform/game_ui_pack.hpp"
#include <string_view>

namespace outrun::platform {
struct FrontendFont {
    std::uint32_t token{};
    std::int16_t width{},height{};
    std::uint32_t first{};
    float spacing_x{},spacing_y{};
    std::vector<std::array<std::int8_t,2>> metrics;
    std::vector<std::int8_t> kerning;
};
struct FrontendFontPack {
    std::array<FrontendFont,10> fonts;
    std::array<GameUiTexture,10> textures;
};
bool parse_frontend_font_pack(const std::uint8_t*,std::size_t,FrontendFontPack&,std::string* error=nullptr);
bool load_frontend_font_pack(const char*,FrontendFontPack&,std::string* error=nullptr);

// 465DF0 compresses UTF-16 code units to their LOW byte, not UTF-8/CP1252.
// Preserve that PC behavior, without rebasing guest pointers or altering input.
class FrontendTextTable {
    std::vector<std::string> strings_;
public:
    bool parse(const std::uint8_t*,std::size_t,std::string* error=nullptr);
    bool load(const char*,std::string* error=nullptr);
    const std::string* get(std::uint32_t index) const;
    std::size_t size() const {return strings_.size();}
};
struct FrontendTextStyle {
    float scale_x{1},scale_y{1};
    std::uint32_t color{~0u},mode{4},flags{1};
    std::int32_t clip_left{},clip_right{640};
};
struct FrontendTextCursor {std::int16_t origin_x{},x{},y{};};
// Native 42CFE0 submission, before the platform-specific GPU adapter.
struct FrontendGlyph {
    std::uint32_t token{};
    std::int32_t left{},top{},right{},bottom{};
    float scale_x{1},scale_y{1},x{},y{};
    std::uint32_t color{},mode{};
};
int frontend_text_width_42c480(const FrontendFont&,const FrontendTextStyle&,
                             std::string_view,float space_fraction=0.3f);
void frontend_text_advance_42c610(const FrontendFont&,const FrontendTextStyle&,
                                FrontendTextCursor&,std::uint8_t,std::uint8_t next);
bool frontend_text_glyph_42c860(const FrontendFont&,const FrontendTextStyle&,
                              const FrontendTextCursor&,std::uint8_t,FrontendGlyph&);
// 42CCC0 (in-race text, 4B9200): 42C720 monospace glyph (characters 0..7F
// except TAB/LF/space; source cell from the index, no bearing or clipping;
// the caller checks the font texture 956BA0) then 42C5A0 monospace advance.
bool frontend_text_glyph_42c720(const FrontendFont&,const FrontendTextStyle&,
                              const FrontendTextCursor&,std::uint8_t,FrontendGlyph&);
void frontend_text_advance_42c5a0(const FrontendFont&,const FrontendTextStyle&,
                                FrontendTextCursor&,std::uint8_t);
bool frontend_text_draw(const FrontendFont&,FrontendTextStyle,FrontendTextCursor,
                        std::string_view,std::vector<FrontendGlyph>&);

constexpr std::size_t PcTextWidgetBytes=0x48c;
void frontend_text_init_48e640(driving::Bytes);
// Resolved bounded text, not a guest printf format/pointer. Embedded NUL ends it.
bool frontend_text_set_48f280(driving::Bytes,std::string_view,std::uint32_t font,std::uint32_t color);
std::string frontend_text_value_48eec0(driving::Bytes);
bool frontend_text_setter(std::uint32_t pc,driving::Bytes,const std::uint32_t*,std::size_t);
struct FrontendTextLines {std::array<std::array<char,128>,16> lines{};};
bool frontend_text_split_48ea50(driving::Bytes,std::string_view,FrontendTextLines&);
bool frontend_text_wrap_48eb60(driving::Bytes,const FrontendFont&,FrontendTextLines&);
// Uses the shared PC line scratch state when the widget is not wrapped.
bool frontend_text_height_48ef90(driving::Bytes,const FrontendFontPack&,FrontendTextLines&,int&);
bool frontend_text_box_48f1b0(driving::Bytes,std::string_view,int limit,
    const std::array<int,4>& rect,std::uint8_t enabled,std::uint32_t font,
    std::uint32_t layer,std::uint32_t color,float scale_x,float scale_y);
bool frontend_text_display_48f3c0(driving::Bytes,const FrontendFontPack&,FrontendTextLines&,
                                 std::vector<FrontendGlyph>&);
}
