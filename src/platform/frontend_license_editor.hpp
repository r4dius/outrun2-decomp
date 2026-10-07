#pragma once
#include "frontend_profiles.hpp"
#include "driving/pc_driving.hpp"
#include <initializer_list>
namespace outrun::platform {
constexpr std::size_t PcLicenseEditorBytes=0x11a4;
// Calls refer to byte offsets inside the key-24 object, never guest pointers.
// UI calls use the exact eleven PC arguments. Widget/keyboard services share
// name_text (48EEC0/468770); 48F280 initializes it from the active license.
// Virtual calls are identified by their slot (4/8/12), with a nonzero offset.
struct LicenseEditorServices {
    void* user{};
    bool (*call)(void*,std::uint32_t pc,std::size_t offset,
                 const std::uint32_t* args,std::size_t count,std::uint32_t& result){};
    std::array<std::uint8_t,16> name_text{};
    // Localized labels are not license names: CREATE NEW LICENSE is 18 bytes.
    // Keep the full 465EB0 result separate from the fixed 16-byte save field.
    std::string localized_text;
    std::uint8_t editing_existing{}; // PC 84B214, shared with key 21
    std::uint32_t missing_pc{};
};
bool license_editor_init_4dd7c0(driving::Bytes,PcLicense&,LicenseEditorServices&);
bool license_editor_grid(driving::Bytes,PcLicense&,unsigned stage,int input,LicenseEditorServices&);
bool license_editor_tick_4de2b0(driving::Bytes,PcLicense&,LicenseEditorServices&,std::uint32_t& owner_action);
}
