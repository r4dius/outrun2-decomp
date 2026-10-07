#pragma once
#include <cstddef>
#include <cstdint>
namespace outrun::platform {
// EXE-owned renderer data by PC address (tools/generate_shader_tables.py):
// shader fragment tables/pointers, renderer state tables, float constants.
struct EmbeddedRenderRange { std::uint32_t va,size; const std::uint8_t* bytes; };
// bytes point into the player's EXE image (bound by exe_tables.cpp).
extern EmbeddedRenderRange EmbeddedRenderRanges[];
extern const std::size_t EmbeddedRenderRangeCount;
void bind_embedded_render_ranges();
}
