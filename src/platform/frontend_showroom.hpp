#pragma once
// Parent key 43 (factory 441AB0, 0x28FC bytes): the Showroom, where OutRun
// Miles buy cars, colours, music and the other unlocks. Owner 4D4230, vtable
// 5CBEE8: destroy 4D5B20 (4D5760), init 4D4A30, control 4D5880, display
// 4D4350, suspend 4D4B70, input 48F5F0. Read from the Steam build (the
// retail EXE routes this owner through its protection).
//
// Layout: 30 records of 0x1C bytes at +34 (+34 unlocked, +38 model, +3C
// unlock bit, +40 menu index, +44 affordable, +45 preview colour, +46..+4D
// colours, +4E), +37C state 0..23, choice list +380 (48D970 over 5CAE00),
// UI resources +7F8, +898 (frame), +938 (page title), +9D8 icon strip
// (446710, resource +9F0), +AA0/+AA1 music index, preview +AA4 (48BF00),
// +B38 owned icon kind, +B3C owned icon, +BDC refresh, +BE0 item base (CF =
// the main page, -1 = by record), +BE4 item / +BE8 return state of the
// purchase dialog, window +BEC (48C490), +1E9C yes/no icon, +1F3C dialog
// choice (3 yes, 5 no), +1F40 header, texts +1FE0 (price) / +246C (Miles),
// +28F8 affordable.
#include "platform/frontend_list.hpp"
#include "platform/frontend_profiles.hpp"
#include "platform/frontend_vehicle_preview.hpp"
#include "platform/frontend_window.hpp"
#include <memory>
namespace outrun::platform {
constexpr unsigned PcShowroomBytes=0x28fc;
struct FrontendShowroom {
    std::vector<std::uint8_t> object=std::vector<std::uint8_t>(PcShowroomBytes);
    FrontendSprites* pool{};                      // borrowed, set at construction
    std::unique_ptr<FrontendUiResources> ui;      // created on first use from pool
    std::unique_ptr<FrontendChoiceList> list;     // +380 (rows are not copied)
    unsigned fault{};
    bool constructed{};
    FrontendTextLines lines;
    std::vector<FrontendGlyph> glyphs;
    std::vector<FrontendListImage> images;
    std::vector<FrontendWindowIcon> icons;
    FrontendShowroom()=default;
    FrontendShowroom(const FrontendShowroom& o){*this=o;}
    FrontendShowroom& operator=(const FrontendShowroom& o){
        if(this==&o)return *this;
        object=o.object;pool=o.pool;ui.reset();list.reset();fault=o.fault;constructed=o.constructed;
        lines=o.lines;glyphs=o.glyphs;images=o.images;icons=o.icons;return *this;}
};
struct FrontendShowroomServices {
    driving::Bytes root;                       // 4035F0 owner (+51C labels 440EA0, 442F20)
    driving::PcUiNotifyGlobals& globals;
    PcLicense& profile;                        // 7C23E0: +24 Miles, +28 unlock bits, +3F4 save flag
    const FrontendInputSnapshot& input;        // 48F5F0
    unsigned& repeat;                          // 6591E4
    const FrontendFontPack& fonts;
    const FrontendTextTable& text;             // 465EB0
    const std::uint8_t* prices{};              // 84B920 (4EF4D0), 0xCF dwords from 4EF280
    FrontendVehiclePreviewServices* preview{};
    std::int32_t& camera_preset_819634;        // 482E70
    float timer{};                             // 842110 (choice list)
    float owner_delta{};                       // root +DA0 (48CC00)
    std::uint32_t root_state{};
    // 4C50D0/4C50F0/4C5100/4C5110 (transition globals), 401000/401030 (BGM),
    // 42E020 (BGM volume) and 416420 (licence save).
    void* user{};
    bool (*external)(void*,unsigned pc,const unsigned* args,std::size_t count){};
    unsigned missing{};
};
bool frontend_showroom_construct_4d4230(FrontendShowroom&,FrontendSprites&,unsigned& repeat);
bool frontend_showroom_init_4d4a30(FrontendShowroom&,FrontendShowroomServices&);
// result: 2 when the main page is left (back), otherwise 0.
bool frontend_showroom_control_4d5880(FrontendShowroom&,FrontendShowroomServices&,unsigned& result);
bool frontend_showroom_display_4d4350(FrontendShowroom&,FrontendShowroomServices&);
bool frontend_showroom_suspend_4d4b70(FrontendShowroom&,FrontendShowroomServices&);
bool frontend_showroom_destroy_4d5760(FrontendShowroom&,FrontendShowroomServices&);
}
