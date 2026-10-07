#pragma once
#include "frontend_ui_resources.hpp"
namespace outrun::platform {
struct FrontendCarouselDescriptor {unsigned token,first,last;};
struct FrontendCarouselTable {
    const FrontendCarouselDescriptor* records{};
    unsigned count{},pc_address{};
};
struct FrontendCarouselServices {FrontendUiResources& ui;float timer{};unsigned missing{};};
bool frontend_carousel_init_51b7b0(driving::Bytes,FrontendCarouselTable,unsigned first,unsigned end,FrontendCarouselServices&);
bool frontend_carousel_release_51bc30(driving::Bytes,FrontendCarouselServices&);
void frontend_carousel_select_51bdb0(driving::Bytes,int index);
void frontend_carousel_position_51bd00(driving::Bytes,float x,float y,float duration);
void frontend_carousel_scale_51bc40(driving::Bytes,float x,float y,float duration);
bool frontend_carousel_move(driving::Bytes,FrontendCarouselTable,bool forward,FrontendCarouselServices&);
bool frontend_carousel_tick_51be30(driving::Bytes,FrontendCarouselTable,FrontendCarouselServices&);
}
