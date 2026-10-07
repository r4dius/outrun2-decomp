#pragma once
#include "frontend_categories.hpp"
#include "race_asset_pack.hpp"
namespace outrun::platform {
constexpr unsigned PcMissionOwnerBytes=0x4488;
struct FrontendMissionState {
    std::array<std::uint8_t,PcMissionOwnerBytes> object{};
    unsigned fault{};
    bool constructed{};
    FrontendTextLines lines;
    std::vector<FrontendGlyph> glyphs;
};
struct FrontendMissionServices : FrontendCategoryServices {
    FrontendCategoryState& categories;
    const RaceAssetPack* races{};
    const RaceAssignmentPack* assignment{};
};
bool frontend_missions_construct_4e9160(FrontendMissionState&,unsigned& repeat);
bool frontend_missions_init_4e92d0(FrontendMissionState&,FrontendMissionServices&);
bool frontend_missions_control_4e9360(FrontendMissionState&,FrontendMissionServices&,unsigned& result);
bool frontend_missions_display_4e95a0(FrontendMissionState&,FrontendMissionServices&);
bool frontend_missions_suspend_4e9bb0(FrontendMissionState&,FrontendMissionServices&);
bool frontend_mission_unlocked_4e90e0(FrontendMissionServices&,unsigned node,bool&);
}
