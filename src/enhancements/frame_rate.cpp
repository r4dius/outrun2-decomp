#include "enhancements/frame_rate.hpp"
#include "driving/pc_matrix_stack.hpp"
#include "driving/pc_wrecker.hpp"
#include <algorithm>
#include <array>
#include <cstring>
#include <exception>
#include <memory>
#include <vector>

namespace outrun::enhancements {
namespace {
struct Replay {const void* key;std::function<void()> replay,restore;bool owed{},car{};};
struct State {
    std::uint32_t rate{60};
    float blend{1.f};
    std::uint32_t ticks{1};
    std::vector<Replay> replays;          // builders of the last tick, in order
    std::vector<std::function<void()>> pending;   // restores owed before the next tick
    std::array<std::uint8_t,64u*64u> arena{};
    driving::PcMatrixStack scratch{driving::Bytes(arena.data(),arena.size()),0,0,64};
};
State& state(){static State s;return s;}
}
std::uint32_t display_rate(){return state().rate;}
void set_display_rate(std::uint32_t rate){
    auto& s=state();
    if(rate<=60u){display_before_tick();s.replays.clear();rate=60u;}
    s.rate=rate;
}
bool display_frames(){return state().rate>60u;}
float display_blend(){return state().blend;}

void display_before_tick(){
    auto& s=state();
    s.blend=1.f;
    // Newest first: a value changed by two replays gets its tick value back last.
    for(auto it=s.pending.rbegin();it!=s.pending.rend();++it)(*it)();
    s.pending.clear();
    s.replays.clear();
}
std::uint32_t display_ticks(){return display_frames()?state().ticks:1u;}
void display_after_ticks(float blend,std::uint32_t ticks){
    auto& s=state();
    if(!display_frames())return;
    s.ticks=ticks;
    s.blend=std::clamp(blend,0.f,1.f);
    // The values stay interpolated through the draw; display_before_tick
    // puts them back. A builder that fails keeps its tick result on screen.
    // Cars first: the characters and the camera follow their replayed +B0.
    for(const bool cars:{true,false})for(auto& r:s.replays){
        if(r.car!=cars)continue;
        try{r.replay();}catch(const std::exception&){}
        if(r.restore&&!r.owed){s.pending.push_back(r.restore);r.owed=true;}
    }
}
void display_note(const void* key,std::function<void()> replay,std::function<void()> restore){
    auto& s=state();
    if(!display_frames())return;
    for(auto& r:s.replays)if(r.key==key){r.replay=std::move(replay);r.restore=std::move(restore);return;}
    s.replays.push_back({key,std::move(replay),std::move(restore)});
}
driving::PcMatrixStack& display_matrices(){return state().scratch;}
void display_note_car(driving::Bytes car,std::function<float()> blend,std::uint8_t scene_code){
    if(!display_frames())return;
    // +B0 (the display matrix) gets the tick's value back before the next
    // tick: tick code run before this car's control reads it (the camera).
    auto tick_b0=std::make_shared<std::array<std::uint8_t,64>>();std::memcpy(tick_b0->data(),car.data()+0xb0,64);
    display_note(car.data(),[car,blend=std::move(blend),scene_code]{
        auto& s=state();
        std::array<std::uint8_t,12> d28{};std::memcpy(d28.data(),car.data()+0xd28,12);
        driving::PcDispMatrixContext display{s.scratch,blend(),scene_code};
        driving::pc_calc_disp_matrix(car,display);
        std::memcpy(car.data()+0xd28,d28.data(),12);
    },[car,tick_b0]{std::memcpy(car.data()+0xb0,tick_b0->data(),64);});
    for(auto& r:state().replays)if(r.key==car.data())r.car=true;
}
}
