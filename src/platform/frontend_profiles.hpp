#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
namespace outrun::platform {
constexpr std::size_t PcLicenseBytes=0x40cu,PcCommonSaveBytes=0x10be4u;
using PcLicense = std::array<std::uint8_t,PcLicenseBytes>;
struct FrontendProfiles {
    std::array<std::array<std::uint8_t,PcLicenseBytes>,4> licenses{};
    std::array<std::uint8_t,PcCommonSaveBytes> common{};
    std::array<std::uint8_t,PcLicenseBytes> active{};
    std::array<bool,4> loaded{};
    std::uint32_t loaded_count{},invalid_files{},selected{~0u};
    bool queried{},common_loaded{},active_loaded{};
};
// PC 406E50 layout: LE payload length followed by the unmodified payload.
// Read-only: never creates/overwrites a user's PC saves.
void frontend_profiles_load(FrontendProfiles&,const std::string& save_directory);
// 4162D0's four in-place 4471A0 constructors. Allocation/replay-file setup is
// outside this projection; random results are supplied by the shared CRT stream.
void frontend_profiles_initialize_bank_4162d0(FrontendProfiles&,
    const std::array<std::uint32_t,4>& random_values);
// 416380 reads over the already initialized bank. Missing/corrupt files must
// not replace a valid constructor result with zeroes. No writes are performed.
void frontend_profiles_reload_416380(FrontendProfiles&,const std::string& save_directory);
std::uint32_t frontend_crt_random_580f40(std::uint32_t& state);
std::uint32_t frontend_profiles_next_key(const FrontendProfiles&);
int frontend_profiles_free_slot(const FrontendProfiles&); // 4E0A40, bit +3F4
// In-place PC constructors preserve bytes the original does not initialize.
// random_value is the result of the PC CRT rand() call at 447324.
void frontend_license_reset_4471a0(PcLicense&,std::uint32_t random_value);
void frontend_license_unlock_447360(PcLicense&);
void frontend_license_name_4dd590(PcLicense&,const std::array<std::uint8_t,16>&);
bool frontend_profiles_select_448520(FrontendProfiles&,std::uint32_t slot);
void frontend_profiles_sync_active(FrontendProfiles&);
struct FrontendProfileList {
    std::array<std::uint32_t,4> slots{};
    std::uint32_t occupied{};
    int new_index{-1};
};
FrontendProfileList frontend_profiles_list_4e1c00(FrontendProfiles&);
// Views of the live 495930 category counts and 84B7E0 championship records.
// An absent championship table is the PC's null pointer, not 15 dummy races.
// Counts/types must be obtained from loaded PC data before SUMO_FE completion.
struct LicenseProgressTables {
    std::array<std::uint8_t,40> category_counts{};
    std::array<std::array<std::uint32_t,15>,3> championship_types{};
    std::array<bool,3> championship_present{};
    bool categories_ready{};
    std::uint32_t mode{32};
};
bool frontend_license_completion_447400(const PcLicense&,const LicenseProgressTables&,double&);
bool frontend_category_grade_4e81d0(const PcLicense&,const LicenseProgressTables&,unsigned,int&);
bool frontend_group_grade_4e82a0(const PcLicense&,const LicenseProgressTables&,unsigned,int&);
// 499730: choose the newly earned C2C notice from the seven 4E82A0 grades,
// then record its bit in licence +11C and the dirty bit in +3F4.
std::int32_t frontend_unlock_notice_499730(PcLicense&,const std::array<int,7>& grades);
bool frontend_category_unlocked_4e8410(const PcLicense&,const LicenseProgressTables&,unsigned,bool&);
bool frontend_category_picture_4e8b20(const PcLicense&,const LicenseProgressTables&,unsigned,unsigned&);
// Explicit writes only. The caller supplies its save directory; loading never
// writes. A checked temporary + backup replacement preserves the previous save
// on failure. No directory is created and no PC installation is chosen here.
struct FrontendProfileSaveResult {
    bool common_written{},license_written{};
    int error{},common_error{},license_error{};
    // Original 416420: 1 on success/disabled/no selection, 2 if the license
    // write failed. 4164D0 returns 1 regardless of the common-file result.
    std::uint32_t pc_result{1};
    explicit operator bool()const{return error==0;}
};
FrontendProfileSaveResult frontend_profiles_save_common_4164d0(FrontendProfiles&,const std::string&);
FrontendProfileSaveResult frontend_profiles_save_416420(FrontendProfiles&,const std::string&,bool enabled);
// 416500 resets the slot (and active copy when selected) before removing its
// file. Random results are explicit, so tests and the PC CRT stream agree.
bool frontend_profiles_delete_416500(FrontendProfiles&,std::uint32_t slot,
                                    const std::string&,std::uint32_t slot_random,
                                    std::uint32_t active_random,int& error);
}
