#pragma once
// Parent key 4 (factory 441810, 0x474 bytes): the music choice shown after
// the transmission. Owner 4C9A90, vtable 5C9F3C: init 4C9790, control
// 4C9C20, display 49A650 (RET), suspend 4C9910, input 48F5F0.
// Three track lists (84B0F4 = list 0..2 with 9/7/12 tracks, 68F36C) with a
// cursor each (84B0EC int16[3]); carousel +35C over 5C9B20/5C9BF8/5C9CA0,
// title +3C (5C9DC0/5C9E30/5C9E88), header +DC (440088), help +17C
// (440087), lock icon +21C (440047) for tracks above 6 whose unlock bit
// (5C9AB0) is clear, list label +2BC (5C9F18). The chosen track 68F370[i]
// goes to 830364 (48B1D0); previews play 401000(0, track + 0x21, 1).
#include "platform/frontend_carousel.hpp"
#include "platform/frontend_profiles.hpp"
#include "platform/title_owner.hpp"
namespace outrun::platform {
struct FrontendMusic {
    std::array<std::uint8_t,0x474> object{};
    unsigned fault{};
    bool constructed{};
};
// Process globals shared with other owners.
struct FrontendMusicGlobals {
    std::array<std::int16_t,3> cursors_84b0ec{};
    std::int16_t list_84b0f4{};
    std::uint8_t track_830364{};
};
struct FrontendMusicServices {
    FrontendUiResources& ui;
    driving::Bytes root;                 // owner 4035F0 (442F20, 440EA0 labels)
    driving::PcUiNotifyGlobals& globals;
    const PcLicense& profile;            // 7C23E0 (4474E0 unlock bits)
    const FrontendInputSnapshot& input;  // 48F5F0; device_held for 407930 +8 bit 0x2000
    unsigned& repeat;
    FrontendMusicGlobals& music;
    std::uint32_t& crt_random;           // 580F40 state
    bool random_held_4536c0{};           // 4536C0(0x10)
    float timer{};
    void* user{};
    // 401000(channel, track, loop) / 401030(channel): music playback.
    bool (*play_401000)(void*,unsigned channel,unsigned track,unsigned loop){};
    bool (*stop_401030)(void*,unsigned channel){};
    unsigned missing{};
};
bool frontend_music_construct_4c9a90(FrontendMusic&,unsigned& repeat);
bool frontend_music_init_4c9790(FrontendMusic&,FrontendMusicServices&);
// result: 0 stay, 5 proceed (confirmed or random), 2 back.
bool frontend_music_control_4c9c20(FrontendMusic&,FrontendMusicServices&,unsigned& result);
bool frontend_music_suspend_4c9910(FrontendMusic&,FrontendMusicServices&);
}
