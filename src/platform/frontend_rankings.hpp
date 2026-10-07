#pragma once
// Parent key 36 (factory 441880, 0x27F9C bytes): the course select + local
// ranking board shown before the car select for the modes whose root bits
// (4035F0()+0x20C >> 5) & 7 are 1..6. Owner 4CB300, vtable 5CA074: destroy
// 4CB460 (4CAEB0), init 4CAF80, control 4CB480, display 4CC390, suspend
// 4CA6F0, input 48F5F0.
//
// Layout: carousel +38 (51B7B0 family, table 5C9F98 x9 for mode 1, else
// 5C9F58 x5; +EC is its current index), filter title +150 (4CA9B0, table
// 5CA008), lock icon +1F0 (440047), cursor frame +290 (4400CC), +330 label
// state, +331 focus row, +333 online labels, +334 leaving, +338 result,
// list +33C (51E080: 15-record staging array +30, 15 displayed records
// +130A4, 0x144C each; header +26118.., scroll texts +26670), status text
// +271E4, text +27670, filter text +27AFC, timer +27F88, dots +27F8C,
// +27F94, list mode +27F98.
//
// Offline only: every branch taken when 7D68BC (network session) is set, and
// the ghost-table branch of 51D0F0 (84B0F6 set, 4F3810/4F3870/4F3910), are
// latched as faults with their PC address instead of being approximated.
#include "platform/frontend_carousel.hpp"
#include "platform/frontend_list.hpp"
#include "platform/frontend_profiles.hpp"
#include "platform/title_owner.hpp"
namespace outrun::platform {
constexpr unsigned PcRankingOwnerBytes=0x27f9c;
struct FrontendRankings {
    std::vector<std::uint8_t> object=std::vector<std::uint8_t>(PcRankingOwnerBytes);
    unsigned fault{};
    bool constructed{};
    FrontendTextLines lines;
    std::vector<FrontendGlyph> glyphs;
    std::vector<FrontendListImage> images;
};
// Process globals written by this owner. area_85b308 covers 85B308..85B327:
// the 51DAF0/sprintf scratch string (+0), 85B317 (+0xF), 85B318 (+0x10),
// 85B319 (+0x11), 85B31C (+0x14), 85B320 (+0x18), 85B324 (+0x1C). One
// array keeps an overlong scratch string overlapping 85B317 like the PC.
struct FrontendRankingGlobals {
    std::array<std::uint8_t,0x20> area_85b308{};
    std::int32_t cursor_68fa84{};
    std::int32_t cursor_690e7c{};
    std::int32_t cursor_691534{};
    std::uint8_t variant_84b0f8{};   // key 38 course set toggle (+3D3)
    std::uint8_t download_672df7{};  // network ghost download request (unported when set)
    std::uint8_t ghost_84b0f6{};   // set by other owners; nonzero is unported here
    std::uint8_t online_7d68bc{},online_7d68d2{};
    // Rankings menu (keys 49 -> 51 -> 48 -> 50): 84B0F7 mode carousel index,
    // 84B20D Time Attack flag, 69012C board cursor (key 48).
    std::int8_t mode_cursor_84b0f7{};
    std::uint8_t ta_84b20d{};
    std::int32_t cursor_69012c{};
    std::uint8_t flag_84b20e{};   // set while the key 50 list is open
    std::int32_t cursor_6939f4{5};
};
struct FrontendRankingServices {
    FrontendUiResources& ui;
    driving::Bytes root;                   // 4035F0 owner: +0x20C mode bits, 440EA0 labels, 442F20
    driving::PcUiNotifyGlobals& globals;
    const PcLicense& profile;              // 7C23E0 (4474E0 unlock bits)
    const std::array<std::uint8_t,PcCommonSaveBytes>& common; // 7B17F8 record tables
    const FrontendInputSnapshot& input;    // 48F5F0
    unsigned& repeat;                      // 6591E4
    FrontendRankingGlobals& g;
    const FrontendFontPack* fonts{};
    const FrontendTextTable* text{};       // 465EB0
    float timer{};                         // 842110 (4AF500)
    const std::uint8_t* records_65f0c0{};  // FrontendRecordManager records (494030 slot test)
    // OutRun2SP ranking tables 84DF40..850B00 (sp_rankings.hpp), loaded by 4165C0/4F3CC0
    // when the first menu opens Rankings; read by 51D0F0 when 84B0F6 is set.
    const std::uint8_t* sp_tables_84df40{};
    std::size_t sp_tables_size{};
    unsigned missing{};
    // Oracle only: ordered UI resource / label / display calls as
    // {pc, offset from trace_base, arguments...}.
    std::vector<std::array<std::uint32_t,14>>* trace{};
    const std::uint8_t* trace_base{};
};
bool frontend_rankings_construct_4cb300(FrontendRankings&,FrontendRankingServices&);
bool frontend_rankings_init_4caf80(FrontendRankings&,FrontendRankingServices&);
// result: 0 stay, otherwise the +338 value once +334 is set (1 confirm, 2 back).
bool frontend_rankings_control_4cb480(FrontendRankings&,FrontendRankingServices&,unsigned& result);
bool frontend_rankings_display_4cc390(FrontendRankings&,FrontendRankingServices&);
bool frontend_rankings_suspend_4ca6f0(FrontendRankings&,FrontendRankingServices&);
bool frontend_rankings_destroy_4caeb0(FrontendRankings&,FrontendRankingServices&);
// Parent key 37 (factory 441990, 0x27A68 bytes, 4CEBF0): the Time Attack
// board after key 36 (carousel +38 over 5CA2D8/5CA314, title +150, cursor
// frame +1F0, focus byte +34, labels +291, leaving +292, result +294, list
// +298, list top +2713C, status text +27144, filter text +275D0). vtable
// 5CA3B0: destroy 4CECC0 (4CE830), init 4CECE0, control 4CEF30, display
// 4CE3B0, suspend 4CE3E0, input 48F5F0. Confirm leaves with 690E80[carousel]
// (keys 0x26/0x27). Its cursor global is 690E7C.
constexpr unsigned PcGhostBoardBytes=0x27a68;
using FrontendGhostBoard=FrontendRankings;          // same owned state, different layout
bool frontend_ghosts_construct_4cebf0(FrontendGhostBoard&,FrontendRankingServices&);
bool frontend_ghosts_init_4cece0(FrontendGhostBoard&,FrontendRankingServices&);
bool frontend_ghosts_control_4cef30(FrontendGhostBoard&,FrontendRankingServices&,unsigned& result);
bool frontend_ghosts_display_4ce3b0(FrontendGhostBoard&,FrontendRankingServices&);
bool frontend_ghosts_suspend_4ce3e0(FrontendGhostBoard&,FrontendRankingServices&);
bool frontend_ghosts_destroy_4ce830(FrontendGhostBoard&,FrontendRankingServices&);
// Parent key 38 (factory 4419F0, 0x27BA8 bytes, 4D05E0): the Time Attack
// course board (carousel of 5CA3C8/5CA468/5CA508/5CA5A8 by mode bits and the
// +3D3 set toggle), lock +150, title +1F0, cursor frame +290, set frame +330,
// list +3DC, top +27280. vtable 5CA6CC: destroy 4D06B0 (4D00D0), init 4D01A0,
// control 4D06D0, display 4CFB10, suspend 4CFB40. Confirm writes the course
// into root +20C bits 12..17 and leaves for key 10. Cursor global 691534.
constexpr unsigned PcCourseBoardBytes=0x27ba8;
using FrontendCourseBoard=FrontendRankings;
bool frontend_courses_construct_4d05e0(FrontendCourseBoard&,FrontendRankingServices&);
bool frontend_courses_init_4d01a0(FrontendCourseBoard&,FrontendRankingServices&);
bool frontend_courses_control_4d06d0(FrontendCourseBoard&,FrontendRankingServices&,unsigned& result);
bool frontend_courses_display_4cfb10(FrontendCourseBoard&,FrontendRankingServices&);
bool frontend_courses_suspend_4cfb40(FrontendCourseBoard&,FrontendRankingServices&);
bool frontend_courses_destroy_4d00d0(FrontendCourseBoard&,FrontendRankingServices&);
// Exposed for the oracle: the list component on its own (list = object+0x33C).
bool frontend_rankings_list_construct_51e080(driving::Bytes list,FrontendRankingServices&);
bool frontend_rankings_list_reset_51c440(driving::Bytes list,int mode,bool flag,FrontendRankingServices&);
bool frontend_rankings_list_add_51e140(driving::Bytes list,const std::array<std::int32_t,12>& args,FrontendRankingServices&);
bool frontend_rankings_list_fill_51f280(driving::Bytes list,int filter,int first,bool clamp,FrontendRankingServices&);
bool frontend_rankings_list_tick_51ec80(driving::Bytes list,FrontendRankingServices&);
bool frontend_rankings_list_display_51edc0(driving::Bytes list,FrontendRankings&,FrontendRankingServices&);
int frontend_rankings_frame_51c710(int kind,int value,const FrontendRankingServices&,bool& valid);
// Parent key 51 (factory 441930, 0x279C4 bytes, 4CE170): the rankings mode
// carousel (OutRun / Heart Attack / Time Attack) after the key 49 selector.
// vtable 5CA2BC: destroy 4CE210 (4CDEA0), init 4CDC10, control 4CDF40,
// display 49A650 (RET), suspend 4CDD50. Carousel +34 (5CA198 x9 local,
// 5CA250 x5 OutRun2SP), list +1F4 (reset only), texts +270A0 / +2752C,
// leaving +1EC, result +1F0. Confirm leaves for key 48 (+4 = 0x30).
constexpr unsigned PcModeBoardBytes=0x279c4;
using FrontendModeBoard=FrontendRankings;
bool frontend_modes_construct_4ce170(FrontendModeBoard&,FrontendRankingServices&);
bool frontend_modes_init_4cdc10(FrontendModeBoard&,FrontendRankingServices&);
bool frontend_modes_control_4cdf40(FrontendModeBoard&,FrontendRankingServices&,unsigned& result);
bool frontend_modes_suspend_4cdd50(FrontendModeBoard&,FrontendRankingServices&);
bool frontend_modes_destroy_4cdea0(FrontendModeBoard&,FrontendRankingServices&);
// Parent keys 48/57 (factory 4418D0, 0x27F9C bytes, 4CCFC0): the rankings
// course board, a sibling of key 36 with its own helpers (4CC410 load, 4CC5F0
// titles 5CA0E0, 4CC650 reload, 4CC740 restore, 4CC970 labels, 4CCE50
// filter label), carousel 5CA0A0 (2 entries), focus +330, labels +334,
// leaving +333, result +338, cursor 69012C. vtable 5CA17C: destroy 4CD120
// (4CCAF0), init 4CCBC0, control 4CD140, display 4CC390 (key 36's),
// suspend 4CC3C0. Confirm leaves for key 50 (+4 = 0x32).
using FrontendBoard48=FrontendRankings;
bool frontend_board48_construct_4ccfc0(FrontendBoard48&,FrontendRankingServices&);
bool frontend_board48_init_4ccbc0(FrontendBoard48&,FrontendRankingServices&);
bool frontend_board48_control_4cd140(FrontendBoard48&,FrontendRankingServices&,unsigned& result);
bool frontend_board48_suspend_4cc3c0(FrontendBoard48&,FrontendRankingServices&);
bool frontend_board48_destroy_4ccaf0(FrontendBoard48&,FrontendRankingServices&);
// Parent keys 50/53 (factory 441BD0, 0x27B08 bytes, 4DA2F0): the rankings
// list of one course (ten records), after the key 48 board. vtable 5CD788:
// destroy 4DA3D0 (4D9CB0), init 4D9D70, control 4DA3F0, display 4D9430,
// suspend 4D9460. Carousel +38 (courses: 5CD490/5CD530 OutRun, 5CD5D0/
// 5CD658 Heart Attack, 5CCC70/5CCE78/5CD080/5CD288 Time Attack), titles
// 5CD6E0 +150, cursor +1F0, course set icon +290, focus byte +34, list +33C,
// top +271E0, leaving +332, result +338, set toggle +334, cursor 6939F4.
constexpr unsigned PcBoard50Bytes=0x27b08;
using FrontendBoard50=FrontendRankings;
bool frontend_board50_construct_4da2f0(FrontendBoard50&,FrontendRankingServices&);
bool frontend_board50_init_4d9d70(FrontendBoard50&,FrontendRankingServices&);
bool frontend_board50_control_4da3f0(FrontendBoard50&,FrontendRankingServices&,unsigned& result);
bool frontend_board50_display_4d9430(FrontendBoard50&,FrontendRankingServices&);
bool frontend_board50_suspend_4d9460(FrontendBoard50&,FrontendRankingServices&);
bool frontend_board50_destroy_4d9cb0(FrontendBoard50&,FrontendRankingServices&);
}
