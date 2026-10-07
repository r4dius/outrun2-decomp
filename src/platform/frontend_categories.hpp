#pragma once
#include "frontend_profiles.hpp"
#include "frontend_ui_resources.hpp"
#include "frontend_text.hpp"
#include "title_owner.hpp"
namespace outrun::platform {
constexpr unsigned PcCategoryOwnerBytes=0x674;
struct FrontendCategoryState {
    std::array<std::uint8_t,PcCategoryOwnerBytes> object{};
    // PC process globals, retained when this child is suspended/reconstructed.
    unsigned selected_84b7f0{},selection_84b7f4{},selection_84b7f8{},query_83639c{};
    unsigned fault{};
    bool constructed{};
    std::vector<FrontendGlyph> glyphs;
};
struct FrontendCategoryServices {
    FrontendUiResources& ui;
    driving::Bytes root;
    const PcLicense& profile;
    const LicenseProgressTables& progress;
    const FrontendInputSnapshot& input;
    unsigned& repeat;
    const FrontendFontPack* fonts{};
    const FrontendTextTable* text{};
    unsigned missing{};
};
bool frontend_categories_construct_4e8130(FrontendCategoryState&,unsigned& repeat);
bool frontend_categories_init_4e8760(FrontendCategoryState&,FrontendCategoryServices&);
bool frontend_categories_control_4e8780(FrontendCategoryState&,FrontendCategoryServices&,unsigned& result);
bool frontend_categories_display_4e8c60(FrontendCategoryState&,FrontendCategoryServices&);
bool frontend_categories_suspend_4e8a30(FrontendCategoryState&,FrontendCategoryServices&);
}
