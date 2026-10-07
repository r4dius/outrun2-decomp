#pragma once
#include "frontend_text.hpp"
#include "frontend_ui_resources.hpp"
#include <deque>

namespace outrun::platform {
constexpr std::size_t PcFrontendListBytes=0x3c, PcFrontendListRowBytes=0x178;
// Raw image submissions, not animation handles. These retain the exact PC
// 42D280/42D300 arguments; the renderer must resolve the original image bank.
struct FrontendListImage {
    std::uint32_t pc{},mode{},token{},color{};
    std::int32_t x{},y{},frame{};
    float width{},height{},layer{};
};
void frontend_scroll_text_construct_4edc80(driving::Bytes);
bool frontend_scroll_text_init_4edaf0(driving::Bytes,const FrontendFont&,
    std::string_view,int x,int y,int width,std::uint32_t color);
bool frontend_scroll_text_draw_4edb90(driving::Bytes,const FrontendFont&,
    std::vector<FrontendGlyph>&);
// 4EDCF0 replaces the text only (width +10 measured with flag 0x100, length
// +18, scroll +C/+20 reset); position, limit, layer and colour are kept. Its
// head is a protection gateway: the width font is not visible, font 9 is the
// one every caller (key-44 options) has active.
bool frontend_scroll_text_set_4edcf0(driving::Bytes,const FrontendFont&,std::string_view);
void frontend_scroll_text_move_4edcc0(driving::Bytes,int x,int y);       // +4 / +8
void frontend_scroll_text_layer_4f1ce0(driving::Bytes,std::uint32_t);    // +0 (42CCB0 layer)

// Owns rows in native memory. Header +8/+C and row +174 are never dereferenced
// as host pointers. +C retains the source table identity for diagnostics only.
class FrontendList {
    driving::Bytes b_;
    FrontendUiResources& ui_;
    const FrontendFont& font_;
    std::deque<std::array<std::uint8_t,PcFrontendListRowBytes>> rows_;
    std::vector<std::array<std::uint32_t,3>> table_;
    bool append(std::uint32_t,int,const std::string_view*,std::uint32_t&);
public:
    FrontendList(driving::Bytes,FrontendUiResources&,const FrontendFont&);
    FrontendList(const FrontendList&)=delete;
    FrontendList& operator=(const FrontendList&)=delete;
    ~FrontendList();
    bool initialize_4ecfb0(std::uint32_t table_id,
        const std::vector<std::array<std::uint32_t,3>>&,float width,
        std::uint8_t vertical,std::uint8_t scroll,int limit);
    bool add_text_4ed160(const std::string_view*,int indent,std::uint32_t& index);
    bool add_sprite_4ed9b0(std::uint32_t table_index,int indent,std::uint32_t& index);
    bool clear_4eda60(bool destructor=false);
    bool move(bool forward,bool& sound); // sound 4249F0(1), even if only one row enabled
    bool set_enabled_4ed300(std::uint32_t index,bool hide=false);
    bool show_4ed390(std::uint32_t index);
    void reset_selection_4ed8d0();
    bool select_4ed930(int);
    int visible_index_4ed7c0()const;
    int height_count_4ed810()const;
    bool setter(std::uint32_t,const std::uint32_t*,std::size_t);
    bool display_4ed3e0(float pc_timer,std::vector<FrontendGlyph>&,
                       std::vector<FrontendListImage>&);
    std::size_t size()const{return rows_.size();}
    driving::Bytes row(std::size_t index){auto& r=rows_.at(index);return {r.data(),r.size()};}
};

// The older 48D970 list owns full text widgets and two animation resources per
// row. It is not layout-compatible with the compact 4ECFB0 context list.
constexpr std::size_t PcFrontendChoiceListBytes=0x478, PcFrontendChoiceRowBytes=0x5e0;
void frontend_text_format_construct_48d570(driving::Bytes);
class FrontendChoiceList {
    driving::Bytes b_;
    FrontendUiResources& ui_;
    const FrontendFontPack& fonts_;
    std::deque<std::array<std::uint8_t,PcFrontendChoiceRowBytes>> rows_;
    std::vector<std::array<std::uint32_t,3>> table_;
public:
    FrontendChoiceList(driving::Bytes,FrontendUiResources&,const FrontendFontPack&);
    ~FrontendChoiceList();
    FrontendChoiceList(const FrontendChoiceList&)=delete;
    FrontendChoiceList& operator=(const FrontendChoiceList&)=delete;
    bool initialize_48d970(std::uint32_t,const std::vector<std::array<std::uint32_t,3>>&,
        std::uint8_t vertical,int selected,int ordinary,std::uint8_t scroll,int limit,
        const driving::Bytes* format,std::uint8_t text_enabled);
    bool add_text_48db30(std::string_view,std::uint32_t& repeat,std::uint32_t& index);
    bool add_sprite_48e390(std::uint32_t table_index,std::uint32_t& repeat,std::uint32_t& index);
    bool move(bool forward,bool& sound);
    void reset_selection_48e2b0();
    bool clear_48e440();
    bool display_48dda0(float timer,FrontendTextLines&,std::vector<FrontendGlyph>&);
    std::size_t size()const{return rows_.size();}
    driving::Bytes row(std::size_t i){auto& r=rows_.at(i);return {r.data(),r.size()};}
};
}
