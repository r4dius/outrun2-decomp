#pragma once
// Screen 0x3C (factory 442650, 4E9E50): the C2C request map of the bottom row of the
// categories screen (groups 4..6, Clarissa's traffic requests): 15 stages, the stage's
// results and, in detail mode, the request lists REQUEST_NORMAL / SPECIAL1..3 of the stage's
// course script (Scripts/bin/Req_*.bin, the 84B400 + type * 0x10 records).
#include "frontend_categories.hpp"
#include "driving/pc_common_control.hpp"
#include <functional>
namespace outrun::platform {
constexpr unsigned PcRequestOwnerBytes=0x2408;
struct FrontendRequestState {
    std::array<std::uint8_t,PcRequestOwnerBytes> object{};
    unsigned fault{};
    bool constructed{};
    FrontendTextLines lines;
    std::vector<FrontendGlyph> glyphs;
};
struct FrontendRequestServices : FrontendCategoryServices {
    FrontendCategoryState& categories;
    // The course script of a course type (the 84B400 + type * 0x10 record), false when missing.
    std::function<bool(unsigned type,driving::PcRelocCategoryBlobR077&)> script;
};
bool frontend_requests_construct_4e9e50(FrontendRequestState&,unsigned& repeat);
bool frontend_requests_init_4ea330(FrontendRequestState&,FrontendRequestServices&);
bool frontend_requests_control_4ea3c0(FrontendRequestState&,FrontendRequestServices&,unsigned& result);
bool frontend_requests_display_4ea610(FrontendRequestState&,FrontendRequestServices&);
bool frontend_requests_suspend_4eaeb0(FrontendRequestState&,FrontendRequestServices&);
// 4EA070(type, list, i): the caption index (5CEB98) of request i of the course script's list.
bool frontend_request_caption_4ea070(const driving::PcRelocCategoryBlobR077&,unsigned list,unsigned i,unsigned& caption);
}
