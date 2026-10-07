#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>
#include "platform/retail_asset_store.hpp"
#include "platform/course_asset_pack.hpp"

namespace outrun::platform {

struct WorldSourceEntry {
    std::uint32_t descriptor_field{};
    std::uint32_t guest_path_token{};
    std::uint32_t offset{};
    std::uint32_t size{};
    std::uint32_t compressed_size{};
    std::uint32_t crc32{};
};

// Files referenced by the hash-pinned PC descriptor, inflated on the host.
// Tokens are only compared with descriptor fields; they are not native ptrs.
struct WorldSourcePack {
    std::vector<std::uint8_t> bytes;
    std::array<WorldSourceEntry,7> entries{};
    std::uint32_t descriptor_index{};
    std::uint32_t descriptor_token{};
};

bool parse_world_source_pack(const std::uint8_t* data,std::size_t size,
                             WorldSourcePack& pack,std::string* error=nullptr);
bool load_world_source_pack_file(const char* path,WorldSourcePack& pack,
                                 std::string* error=nullptr);
const WorldSourceEntry* world_source_find(const WorldSourcePack& pack,
                                          std::uint32_t field,
                                          std::uint32_t guest_path_token);
const std::uint8_t* world_source_bytes(const WorldSourcePack& pack,
                                       const WorldSourceEntry& entry);
bool build_beac_world_source_from_retail(RetailAssetStore& store,
                                         const CourseAssetPack& course,
                                         WorldSourcePack& pack,
                                         std::string* error=nullptr);

} // namespace outrun::platform
