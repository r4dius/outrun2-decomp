#pragma once
#include "frontend_window.hpp"
#include <memory>

namespace outrun::enhancements { struct SettingsAccess; }   // port: Options > Settings enhancement rows
namespace outrun::platform {
// 445FE0 option selector (0x18 bytes): +0 first text id, +4 last id, +8 current
// id, +C last-first, +10 wrap byte, +14 arrow layer. 4249F0(1) on every change.
constexpr std::size_t PcFrontendOptionBytes=0x18;
void frontend_option_init_445fe0(driving::Bytes,std::int32_t first,std::int32_t last,
                                 std::uint8_t wrap,std::uint32_t layer);
bool frontend_option_next_446010(driving::Bytes,bool& sound);
bool frontend_option_prev_446050(driving::Bytes,bool& sound);
std::int32_t frontend_option_index_446080(driving::Bytes);
void frontend_option_set_446100(driving::Bytes,std::int32_t index); // ignored outside 0..+C
// Arrows 3004A (42D280 frame 0 left / 1 right) beside a field of `width` at x,y.
void frontend_option_arrows_446090(driving::Bytes,int x,int y,int width,std::vector<FrontendListImage>&);
// 446340 slider (0xC0 bytes): +0 sprite table, +4 UI resource (465160), +A4 low
// entry, +A8 high, +AC value, +B0 high-low, +B4/+B8 position, +BC layer. The
// table identity is the PC address (5CC048 audio, 5CC118 steering).
constexpr std::size_t PcFrontendSliderBytes=0xc0;
void frontend_slider_init_446340(driving::Bytes,std::uint32_t table,std::int32_t low,std::int32_t high,
                                 float x,float y,std::uint32_t layer);
void frontend_slider_arrows_4463a0(driving::Bytes,int x,int y,int width,std::vector<FrontendListImage>&);

// Options screen controls of the key-44 owner. `license` is the active license
// 7C23E0 (0x40C bytes). Every PC callee is an ordered service call (pc, ecx
// offset in the owner, argument): 48DDA0/48DC10/48DC60 (+34), 48F5F0 (+0),
// 4249F0 (the stepped widget), 4469C0/446440 (slider), 42EFA0/42FC90 (0).
// 4536F0(1) is globals.feature_mask & 1; root 3 is the race pause menu.
bool title_settings_control_4d86f0(std::uint8_t*,std::size_t,driving::Bytes license,
                                   const TitleMenuGlobals&,const TitleControllerServices&);
bool title_controls_control_4d8890(std::uint8_t*,std::size_t,driving::Bytes license,
                                   const TitleMenuGlobals&,const TitleControllerServices&);
bool title_audio_control_4d89b0(std::uint8_t*,std::size_t,driving::Bytes license,
                                const TitleMenuGlobals&,const TitleControllerServices&);
void title_settings_apply_4d76f0(driving::Bytes owner,driving::Bytes license,std::uint32_t root_state);
void title_audio_apply_4d7ab0(driving::Bytes owner,driving::Bytes license);

// Owner-scoped adapter. Borrowed backing/root/pool/fonts/text/input must all
// outlive this instance. No copy: row resources belong to one actual owner.
class FrontendTitleWidgets {
    friend struct enhancements::SettingsAccess;
    std::uint8_t* storage_;
    driving::Bytes object_,root_;
    FrontendUiResources ui_;
    const FrontendFontPack& fonts_;
    const FrontendTextTable& text_;
    const FrontendInputSnapshot& input_;
    std::uint32_t& previous_;
    std::unique_ptr<FrontendChoiceList> list_;
    FrontendTextLines lines_;
    std::vector<FrontendGlyph> glyphs_;
    std::vector<FrontendListImage> images_;
    std::vector<FrontendWindowIcon> icons_;
    TitleMenuGlobals globals_;
    TitleOwnerGlobals layers_;
    float owner_delta_{},list_timer_{};
    std::uint32_t missing_{};
    // Options screens: the active license 7C23E0 (0x40C bytes), 4493C0 and the
    // 42EFA0 (SE master, arg = int) / 42FC90 (BGM, arg = float bits) services.
    driving::Bytes license_{nullptr,0};
    std::uint32_t language_{};
    void* volume_user_{};
    bool (*volume_)(void*,std::uint32_t pc,std::uint32_t arg){};
    bool service(unsigned,std::uint8_t*,std::size_t,int,unsigned&);
    bool call(unsigned pc,std::size_t offset,int arg,unsigned& result){return service(pc,storage_,offset,arg,result);}
    bool volume(std::uint32_t pc,std::uint32_t arg);
    bool list_ready();
    bool options_window(float x,float y,float width,float height);
    bool slider_refresh_446440(std::size_t);
    bool slider_tick_4469c0(std::size_t);
    bool settings_init_4d7440();
    bool settings_display_4d60c0();
    bool controls_init_4d7780();
    bool controls_display_4d62a0();
    bool controls_close_4d6340();
    bool audio_init_4d78c0();
    bool audio_display_4d6370();
    bool audio_close_4d63f0();
    // Pause confirmations (stages 12..20): 84B208 is the dialog step. The race
    // services of their close bodies go through pause_: 416420, 4505D0,
    // 450230(v), 43F860, 4857C0, 440DE0(v) on 4035F0, 456E10, 440A10(a, b),
    // 4EDCE0(v) on 659930.
    std::uint32_t confirm_84b208_{};
    void* pause_user_{};
    bool (*pause_)(void*,std::uint32_t pc,std::uint32_t a0,std::uint32_t a1,std::uint32_t& out){};
    bool pause_call(std::uint32_t pc,std::uint32_t a0,std::uint32_t a1,std::uint32_t& out);
    bool pause_call(std::uint32_t pc,std::uint32_t a0=0,std::uint32_t a1=0){std::uint32_t o{};return pause_call(pc,a0,a1,o);}
    bool confirm_window(std::uint32_t text);
    bool retry_init_4d6430();
    bool retry_control_4d7b20();
    bool retry_close_4d7b80();
    bool quit_init_4d6530();
    bool quit_control_4d7c70();
    bool quit_close_4d6630();
    bool close_init_4d66d0();
    bool close_control_4d7da0();
    bool add_play_time();
    // Options > Controls > Configuration (stages 21..23).
    void* config_user_{};
    bool (*config_runner_)(void*,std::uint32_t,FrontendTitleWidgets&,std::uint32_t&){};
    std::unique_ptr<FrontendList> config_list_;           // +1C6C
    struct Print { std::uint32_t font{9},layer{},color{~0u}; std::int32_t x{},y{}; } print_;   // 42CA60..42CC00 state
public:
    // Options > Controls > Configuration: 4D7E00 init, 4D7FB0 control, 4D6A60
    // display and 4D6E50 close run translated in the input device layer
    // (pc_input_devices.cpp, which owns the device objects) through `runner`;
    // it reaches the owner's children with config_call: PC callee `pc` with ECX
    // (or the first argument for 48EE80) = owner + offset, the stack arguments
    // and the text arguments already read from guest memory. Without a runner
    // the choice closes the Controls window (the earlier port behaviour).
    using ConfigRunner=bool(*)(void* user,std::uint32_t pc,FrontendTitleWidgets&,std::uint32_t& result);
    void config(void* user,ConfigRunner runner){config_user_=user;config_runner_=runner;}
    bool config_call(std::uint32_t pc,std::size_t offset,const std::uint32_t* a,std::size_t n,
                     const std::vector<std::string>& strings,std::uint32_t& result);
    driving::Bytes owner_bytes(){return object_;}
    const std::string* text(std::uint32_t id)const{return text_.get(id);}
    FrontendTitleWidgets(std::uint8_t*,std::size_t,driving::Bytes,FrontendSprites&,
        const FrontendFontPack&,const FrontendTextTable&,const FrontendInputSnapshot&,std::uint32_t&);
    FrontendTitleWidgets(const FrontendTitleWidgets&)=delete;
    FrontendTitleWidgets& operator=(const FrontendTitleWidgets&)=delete;
    // Reset before resetting the owner bytes or the shared sprite pool.
    void reset();
    bool clear();
    void begin_frame();
    void frame(const TitleMenuGlobals&,const TitleOwnerGlobals&,float owner_delta,float list_timer,
               std::uint32_t pause_domain,float motion_step);
    void effect(void* user,bool(*callback)(void*,std::uint32_t));
    void options(driving::Bytes license,std::uint32_t language){license_=license;language_=language;}
    void volume(void* user,bool(*callback)(void*,std::uint32_t pc,std::uint32_t arg)){volume_user_=user;volume_=callback;}
    bool call_volume(std::uint32_t pc,std::uint32_t arg){return volume(pc,arg);}   // 42EFA0 / 42FC90 for other screens
    void pause(void* user,bool(*callback)(void*,std::uint32_t pc,std::uint32_t a0,std::uint32_t a1,std::uint32_t& out)){pause_user_=user;pause_=callback;}
    bool invoke(std::uint32_t pc,std::size_t offset,std::uint32_t&);
    std::uint32_t missing_pc()const{return missing_;}
    std::size_t rows()const{return list_?list_->size():0;}
    bool visible()const{return object_.u8(0x9bc+0x34)!=0;}
    const auto& glyphs()const{return glyphs_;}
    const auto& images()const{return images_;}
    const auto& icons()const{return icons_;}
};
}
