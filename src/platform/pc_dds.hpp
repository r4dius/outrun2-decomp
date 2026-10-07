#pragma once
// DDS texture files of the PMT video sections, as D3DXCreate[Cube]Texture-
// FromFileInMemoryEx builds them for the PC loader 42E8C0: levels taken from
// the file, missing levels of the requested chain generated with the mip
// filter (D3DX_FILTER_NONE copies without scaling, i.e. the top-left part).
// Formats present in the retail data: DXT1/DXT3/DXT5 and A8R8G8B8.
#include "platform/pc_d3d9.hpp"
#include <string>
#include <vector>
namespace outrun::platform {
struct PcDdsTexture {
    enum class Format { bc1, bc2, bc3, rgba8 };
    Format format{Format::rgba8};
    bool cube{};
    std::uint32_t width{},height{},levels{};
    // faces[face][level]: BC blocks or RGBA8 texels (R,G,B,A bytes).
    struct Level { std::uint32_t width{},height{}; std::vector<std::uint8_t> data; };
    std::vector<std::vector<Level>> faces;
};
bool pc_d3dx_texture_from_file(const PcTextureFileRequest&,PcDdsTexture& out,std::string& error);
// RGBA8 texels of one level (decoding BC blocks).
std::vector<std::uint8_t> pc_dds_rgba(const PcDdsTexture&,unsigned face,unsigned level);
}
