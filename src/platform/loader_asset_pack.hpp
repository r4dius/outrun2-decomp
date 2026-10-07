#pragma once
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace outrun::platform {

struct LoaderAssetRecord {
    std::uint32_t resource_id{};
    std::uint32_t request_mode{};
    std::vector<std::uint8_t> bytes;
};

struct LoaderAssetPack {
    std::vector<LoaderAssetRecord> records;
};

constexpr std::uint32_t LoaderAssetPackVersion=12u;
constexpr std::size_t LoaderAssetPackHeaderSize=64u;
constexpr std::size_t LoaderAssetPackRecordSize=32u;
constexpr std::size_t LoaderAssetFrontendScriptCount=64u;
constexpr std::size_t LoaderAssetPackExpectedRecords=55u+LoaderAssetFrontendScriptCount;
constexpr std::uint32_t LoaderAssetStartLoadingId=0x2fu;
constexpr std::uint32_t LoaderAssetStartVersusId=0x49u;
constexpr std::uint32_t LoaderAssetGameSpritesId=0x2bu;
constexpr std::uint32_t LoaderAssetSelectTableId=0x10000u;
constexpr std::size_t LoaderAssetSelectTableBytes=22528u;
constexpr std::uint32_t LoaderAssetFrontendScriptBaseId=0x20000u;
constexpr std::size_t LoaderAssetFrontendScriptBytes=379696u;
// The PC 0x4F2020 motion graph uses its own indices, not 0x448AD0 IDs.
constexpr std::uint32_t LoaderAssetMotionTableId=0x30000u;
constexpr std::uint32_t LoaderAssetBoneTableId=0x30001u;
constexpr std::uint32_t LoaderAssetMotionGroup6Id=0x30006u;
constexpr std::uint32_t LoaderAssetMotionGroup32Id=0x30020u;
constexpr std::uint32_t loader_motion_group_id(std::uint32_t group){
    return LoaderAssetMotionTableId+group;
}

bool parse_loader_asset_pack(const std::uint8_t* data,std::size_t size,
                             LoaderAssetPack& pack,std::string* error=nullptr);
bool load_loader_asset_pack_file(const char* path,LoaderAssetPack& pack,
                                 std::string* error=nullptr);
const LoaderAssetRecord* find_loader_asset(const LoaderAssetPack& pack,
                                            std::uint32_t resource_id,
                                            std::uint32_t request_mode);

} // namespace outrun::platform
