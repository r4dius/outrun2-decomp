#pragma once
#include "frontend_license_chooser.hpp"
#include "frontend_text.hpp"
#include "frontend_ui_resources.hpp"
#include "frontend_keyboard.hpp"
#include "frontend_list.hpp"
#include "frontend_window.hpp"
#include "title_owner.hpp"
#include <map>
#include <memory>
#include <set>
namespace outrun::platform {
// Live PC manager +18 pointer table, selected by 659940. Bindings are borrowed;
// null manager/record is different from an unresolved non-null guest token.
struct LicenseSpecialProfileSource {
    std::int32_t selected{-1};
    std::uint32_t manager{};
    driving::PcNativeHandleResolver handles{};
};
const char* license_rank_4eeee0(float score);
// Owner-scoped services: object offsets remain bounded; no 32-bit guest pointer
// is reinterpreted as a host pointer. Keyboard, input, sound, save and owner
// commands must be supplied explicitly, not returned as synthetic successes.
class FrontendLicenseWidgets {
public:
    using ExternalCall=bool(*)(void*,LicenseEditorServices&,std::uint32_t,std::size_t,
                              const std::uint32_t*,std::size_t,std::uint32_t&);
private:
    driving::Bytes object_;
    FrontendUiResources& ui_;
    const FrontendFontPack& fonts_;
    const FrontendTextTable& text_;
    std::set<std::size_t> widgets_,resources_;
    LicenseEditorServices services_;
    void* external_user_{};
    ExternalCall external_call_{};
    FrontendTextLines lines_;
    std::vector<FrontendGlyph> glyphs_;
    std::vector<FrontendListImage> images_;
    std::vector<FrontendWindowIcon> icons_;
    std::set<std::size_t> windows_;
    float owner_delta_{};
    unsigned root_state_{};
    bool owner_clock_ready_{};
    std::map<std::size_t,std::unique_ptr<FrontendList>> lists_;
    std::map<std::size_t,std::unique_ptr<FrontendChoiceList>> choice_lists_;
    // Borrowed PC-global input repeat state: its owner must outlive this
    // adapter, just like object_, ui_, fonts_ and text_. Appending original
    // choice-row text widgets resets this shared state through their ctor.
    unsigned* input_repeat_{};
    const FrontendInputSnapshot* input_snapshot_{}; // borrowed per-ReadIO frame
    float list_timer_{};
    bool list_timer_ready_{};
    FrontendKeyboardServices keyboard_;
    bool keyboard_constructed_{};
    void* focus_user_{};
    bool (*focus_call_)(void*,bool){};
    FrontendProfiles* profiles_{};
    LicenseChooserState* chooser_state_{}; // Borrowed owner state, same lifetime as profiles_.
    bool chooser_constructed_{};
    const LicenseSpecialProfileSource* special_source_{};
    void* progress_user_{};
    // 447400 depends on the live mission/category tables. Absence is an error,
    // never a fabricated completion percentage for an existing license.
    bool (*progress_call_)(void*,const PcLicense&,double&){};
    bool call(std::uint32_t,std::size_t,const std::uint32_t*,std::size_t,std::uint32_t&);
public:
    FrontendLicenseWidgets(driving::Bytes,FrontendUiResources&,const FrontendFontPack&,const FrontendTextTable&);
    ~FrontendLicenseWidgets();
    FrontendLicenseWidgets(const FrontendLicenseWidgets&)=delete;
    FrontendLicenseWidgets& operator=(const FrontendLicenseWidgets&)=delete;
    LicenseEditorServices& services(){return services_;}
    void external(void* user,ExternalCall call){external_user_=user;external_call_=call;}
    void input(const FrontendInputSnapshot& snapshot,unsigned& shared_previous){
        input_snapshot_=&snapshot;input_repeat_=&shared_previous;
    }
    void keyboard_focus(void* user,bool (*call)(void*,bool)){focus_user_=user;focus_call_=call;}
    const FrontendKeyboardServices& keyboard_state()const{return keyboard_;}
    bool construct_widget(std::size_t,std::uint32_t& input_repeat);
    bool construct_resource(std::size_t);
    bool construct_editor_resources(std::uint32_t& input_repeat);
    bool construct_editor_4dd5c0(std::uint32_t& input_repeat);
    bool construct_chooser_4e1890(std::uint32_t& input_repeat);
    void chooser_state(LicenseChooserState& state){chooser_state_=&state;}
    bool construct_panel_4e0ab0(std::size_t,std::uint32_t& input_repeat);
    bool construct_list(std::size_t);
    bool construct_window(std::size_t,std::uint32_t& input_repeat);
    bool open_context_4e12b0();
    bool close_context_4e1680();
    bool construct_choice_list(std::size_t,std::uint32_t& input_repeat);
    FrontendChoiceList* choice_list(std::size_t off){auto i=choice_lists_.find(off);return i==choice_lists_.end()?nullptr:i->second.get();}
    bool open_delete_4e16a0();
    bool control_delete_4e17e0(std::uint32_t&);
    bool display_delete_4e1860();
    void owner_clock(float delta,unsigned root_state){owner_delta_=delta;root_state_=root_state;owner_clock_ready_=true;}
    FrontendList* list(std::size_t off){auto i=lists_.find(off);return i==lists_.end()?nullptr:i->second.get();}
    void list_timer(float pc_timer){list_timer_=pc_timer;list_timer_ready_=true;}
    bool initialize_panels_4e0c70();
    bool release_panels_4e0d80();
    void profiles(FrontendProfiles& p,void* user,bool (*progress)(void*,const PcLicense&,double&)){
        profiles_=&p;progress_user_=user;progress_call_=progress;
    }
    void profiles(FrontendProfiles& p,LicenseProgressTables& tables){
        profiles(p,&tables,[](void* user,const PcLicense& record,double& result){
            return frontend_license_completion_447400(record,*static_cast<LicenseProgressTables*>(user),result);
        });
    }
    bool populate_panels_4e2150();
    void special_profile_source(const LicenseSpecialProfileSource& source){special_source_=&source;}
    bool populate_special_4e0dd0();
    bool display_special_4e1060();
    bool display_panel_4e2640();
    bool display_chooser_4e3240();
    void begin_frame(){glyphs_.clear();images_.clear();icons_.clear();}
    const std::vector<FrontendGlyph>& glyphs()const{return glyphs_;}
    const std::vector<FrontendListImage>& images()const{return images_;}
    const std::vector<FrontendWindowIcon>& icons()const{return icons_;}
};
}
