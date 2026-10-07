#pragma once
#include "frontend_ui_resources.hpp"
#include "title_owner.hpp"
namespace outrun::platform {
struct FixedChoiceServices {
    FrontendUiResources& ui;
    driving::Bytes root;
    driving::PcUiNotifyGlobals& globals;
    const FrontendInputSnapshot& input;
    unsigned& repeat;
    float timer{};
    void* user{};
    bool (*external)(void*,unsigned){}; // 4940D0 / 4165C0 / 4F3CC0, never implicit success
    unsigned missing{};
};
bool fixed_choice_construct(std::uint8_t*,std::size_t,unsigned key,unsigned& repeat);
bool fixed_choice_init(driving::Bytes,unsigned key,FixedChoiceServices&);
bool fixed_choice_tick(std::uint8_t*,std::size_t,unsigned key,FixedChoiceServices&,unsigned& result);
bool fixed_choice_suspend(driving::Bytes,unsigned key,FixedChoiceServices&);
// Parent key 49 (factory 441B70, 0x5EC bytes, 4D9010): the rankings
// selector of the first menu (Local / Ghost / Online), vtable 5CCC54: destroy
// 4D9080 (4D8E70), init 4D8EE0, control 4D90A0, display 4D8FC0 (the +158
// text, always ""), suspend 4D8FD0. Carousel +34 over 5CCC00 (6 records,
// 4400D7), +14D leaving, +150 result, +5E4 timer at init, +5E8.
constexpr std::size_t PcRankingSelectorBytes=0x5ec;
struct RankingSelectorGlobals {
    std::uint8_t& ghost_84b0f6;      // 0 local, 1 ghost table, 2 online (the key 51/52 boards)
    std::int8_t& cursor_84b20c;      // carousel index kept across visits
    std::uint32_t& notice_63aa6c;    // 2 when Online is chosen without a session
    bool online_7d68bc{};
};
bool ranking_selector_construct_4d9010(std::uint8_t*,std::size_t,unsigned& repeat);
bool ranking_selector_init_4d8ee0(driving::Bytes,FixedChoiceServices&,RankingSelectorGlobals&);
bool ranking_selector_control_4d90a0(driving::Bytes,FixedChoiceServices&,RankingSelectorGlobals&,unsigned& result);
bool ranking_selector_suspend_4d8fd0(driving::Bytes,FixedChoiceServices&);
}
