#pragma once
#include "driving/pc_common_control.hpp"
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace outrun::platform {

// Private Switch pack: the EXE-derived complete ORC78TBL descriptors and the
// user's real Scripts/bin/csc_data_cvt.bin. Serialized PC pointers are kept
// only as tokens; no x86 address is dereferenced on ARM64.
struct CourseAssetPack {
    driving::PcCourseDescriptorPackR078 descriptors{};
    std::vector<std::uint8_t> course_blob;
};

constexpr std::uint32_t CourseAssetPackVersion=3u;
constexpr std::size_t CourseAssetPackHeaderSize=48u;
constexpr std::size_t CourseDescriptorPackBytesLegacy=2444u;
constexpr std::size_t CourseDescriptorPackBytes=3236u;
constexpr std::size_t CourseDescriptorPackBytesFull=10628u;
constexpr std::size_t CourseCvtBlobBytes=1820u;
constexpr std::int32_t CourseCvtRecordCount=15;

bool parse_course_asset_pack(const std::uint8_t* data,std::size_t size,
                             CourseAssetPack& pack,std::string* error=nullptr);
bool load_course_asset_pack_file(const char* path,CourseAssetPack& pack,
                                 std::string* error=nullptr);

// Original 0x44C220: select a dword from the primary (0x7D30BC) or
// secondary (0x7D30C0) live course descriptor. Returns false only when a
// selected descriptor is missing/truncated; indices >= 16 return PC zero.
bool course_field_44c220(const std::uint8_t* primary,std::size_t primary_size,
                         const std::uint8_t* secondary,std::size_t secondary_size,
                         std::uint32_t index,std::uint32_t& value);

} // namespace outrun::platform
