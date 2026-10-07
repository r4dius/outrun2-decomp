#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <string>

namespace outrun::platform {

// Private r146 pack containing the immutable tables consumed after the
// 0x46FE50 target scheduler in START stage 17.  The original x86 pointers are
// retained only as identity tokens; ARM64 never dereferences them.
constexpr std::size_t Stage17Table68Bytes=0x60u;
constexpr std::size_t Stage17Table6cBytes=0x15eu;
constexpr std::size_t Stage17AssetRecordCount=66u;
constexpr std::uint32_t Stage17AssetPackVersion=1u;
constexpr std::size_t Stage17AssetPackHeaderSize=32u;
constexpr std::size_t Stage17AssetRecordSize=4u+Stage17Table68Bytes+4u+Stage17Table6cBytes;
constexpr std::size_t Stage17AssetPackFileSize=Stage17AssetPackHeaderSize+
    Stage17AssetRecordCount*Stage17AssetRecordSize;

struct Stage17AssetRecord {
    std::uint32_t token68{};
    std::array<std::uint8_t,Stage17Table68Bytes> table68{};
    std::uint32_t token6c{};
    std::array<std::uint8_t,Stage17Table6cBytes> table6c{};
};

struct Stage17AssetPack {
    std::array<Stage17AssetRecord,Stage17AssetRecordCount> records{};
    std::uint32_t payload_crc32{};
};

bool parse_stage17_asset_pack(const std::uint8_t* data,std::size_t size,
                              Stage17AssetPack& pack,std::string* error=nullptr);
bool load_stage17_asset_pack_file(const char* path,Stage17AssetPack& pack,
                                  std::string* error=nullptr);

} // namespace outrun::platform
