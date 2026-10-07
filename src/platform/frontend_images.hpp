#pragma once
#include "frontend_list.hpp"
#include "frontend_glyph_render.hpp"
namespace outrun::platform {
struct FrontendImageRegion {unsigned texture{};std::array<unsigned,4> crop{};};
struct FrontendImageBank {unsigned bank{3};std::vector<FrontendImageRegion> regions;std::vector<GameUiTexture> textures;};
// Inflated original XST: two size words, 32-byte XST header, relative screen
// table and DDS payloads. No generated crop atlas or guessed region indices.
bool parse_frontend_image_bank(const std::uint8_t*,std::size_t,FrontendImageBank&,std::string* error=nullptr);
bool load_frontend_image_bank(const char*,FrontendImageBank&,std::string* error=nullptr);
bool frontend_image_quad(const FrontendListImage&,const FrontendImageBank&,FrontendGlyphQuad&,unsigned& texture);
}
