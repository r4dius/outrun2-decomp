#pragma once
#include "platform/retail_asset_store.hpp"
#include "platform/frontend_text.hpp"
#include "platform/game_ui_pack.hpp"
#include <cstdint>
#include <string>
#include <vector>

namespace outrun::platform {
// The START loading picture (frontend_preview_pack format), built in memory
// from the player's SprAni/DDS files.
bool build_retail_start_loading(RetailAssetStore& store,std::vector<std::uint8_t>& pack,std::string* error=nullptr);
// The authored timeline of a SprAni bank (Sprani/ani_SPRANI_<bank>_CVT.sz),
// e.g. SUMO_FE (224 scenes) or ETC (319): the sprite timing of the frontend.
bool load_retail_sprani(RetailAssetStore&,const std::string& bank,std::uint32_t scenes,GameUiPack&,std::string* error=nullptr);
// Font metrics and atlases of Sprite/spr_font_xst.sz, built in memory.
bool build_retail_font_pack(RetailAssetStore&,FrontendFontPack&,std::string* error=nullptr);
}
