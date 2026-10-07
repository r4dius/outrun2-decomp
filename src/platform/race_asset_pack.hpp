#pragma once
#include "driving/pc_common_control.hpp"
#include "platform/frontend_profiles.hpp"
#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace outrun::platform {

// The unmodified, user-owned Scripts/bin/Races.bin. Pointers in its 0x44-byte
// Races records are resolved through the file's relocation table, never from
// their serialized x86 address-shaped values.
struct RaceAssetPack {
    std::vector<std::uint8_t> bytes;
    std::size_t races_offset{};
    std::uint32_t race_count{};
};

// 0x496130 loads RaceAssignment.bin/RACE_MAPPING_ARRAY into PC 0x836370.
struct RaceAssignmentPack {
    std::array<std::uint32_t,40> menu_race_keys{};
    std::uint32_t menu_count{};
};

struct RaceCourseSelection {
    std::uint32_t race_index{};
    std::uint32_t race_kind{}; // PC selected record +0x14
    std::uint32_t course_count{};
    std::vector<std::uint8_t> course_records; // private mutable 0x78-byte copies
};

// Actual 84B7E0..84B7EC category records, not serialized/host pointers. The
// four 4E8620 categories have 0x78-byte course records; completion uses +1C
// from the first fifteen records in each of the first three categories.
struct FrontendCourseTable {
    std::vector<std::uint8_t> records;
    std::uint32_t count{};
};
bool parse_frontend_course_table(std::uint32_t lane,const std::uint8_t* data,
    std::size_t size,FrontendCourseTable&,std::string* error=nullptr);
// Atomic: unavailable or malformed source data leaves the previous output
// untouched. mode is the actual 78026C value; non-SUMO_FE returns zero progress
// through 447400 without touching the course tables, as on PC.
bool frontend_license_progress_tables(const RaceAssetPack*,const RaceAssignmentPack*,
    const std::array<FrontendCourseTable,4>&,std::uint32_t mode,
    LicenseProgressTables&);

// A pointer field of Races.bin (a byte offset in the file) resolved through
// the relocation table: the file offset it points to (false when not relocated).
bool race_asset_pointer(const RaceAssetPack&,std::size_t field_offset,std::size_t& target);
bool parse_race_asset_pack(const std::uint8_t* data,std::size_t size,
                           RaceAssetPack& pack,std::string* error=nullptr);
bool load_race_asset_pack_file(const char* path,RaceAssetPack& pack,
                               std::string* error=nullptr);
bool parse_race_assignment_pack(const std::uint8_t* data,std::size_t size,
                                RaceAssignmentPack& pack,std::string* error=nullptr);
bool load_race_assignment_pack_file(const char* path,RaceAssignmentPack& pack,
                                    std::string* error=nullptr);
// PC 0x496974..0x4969D5: match the two UI-owned selection dwords, follow
// record +0x18 via 0x4F12A0 relocation and find its category count as in
// 0x4F1210. The result feeds the already ported 0x44D720 direct-record path.
bool race_course_select_4965a0(const RaceAssetPack& pack,
                                std::uint32_t race_key,std::uint32_t sub_key,
                                RaceCourseSelection& selection);
// PC 0x4EEB6E..0x4EEB92 + 0x495930/0x4958A0: clamp the menu sub-choice to
// the number of Races records for this mapping key, then publish two dwords.
// Races record +0x20 (mission type) of the (key, sub key) record.
bool race_record_type(const RaceAssetPack& races,std::uint32_t key,std::uint32_t sub_key,std::uint32_t& type);
// PC 0x495930 (Races loaded): number of race records whose key matches.
std::uint32_t race_count_with_key_495930(const RaceAssetPack& races,std::uint32_t key);
bool race_menu_choice_4eeb50(const RaceAssetPack& races,
                             const RaceAssignmentPack& assignment,
                             std::uint32_t menu_index,std::uint32_t sub_index,
                             std::uint32_t& race_key,std::uint32_t& sub_key);
// 4958C0 exact category mapping and one-based race lookup; no clamping.
bool race_menu_type_4958c0(const RaceAssetPack&,const RaceAssignmentPack&,
    std::uint32_t category,std::uint32_t sub_key,std::uint32_t& type);

} // namespace outrun::platform
