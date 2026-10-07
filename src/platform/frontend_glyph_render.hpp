#pragma once
#include "frontend_text.hpp"
namespace outrun::platform {
using FrontendGlyphQuad=std::array<MeshPreviewVertex,4>;
// 42A0A0/D3DXSprite path for the zero-flags descriptors emitted by 42C860.
// Fixed 640x480 PC coordinates, transformed to the existing letterboxed NDC.
bool frontend_glyph_quad(const FrontendGlyph&,const GameUiTexture&,FrontendGlyphQuad&);
unsigned frontend_glyph_layer(const FrontendGlyph&);
}
