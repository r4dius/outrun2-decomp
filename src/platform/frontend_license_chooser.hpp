#pragma once
#include "frontend_license_editor.hpp"
namespace outrun::platform {
// 449B30's four unsigned 16-bit outputs: hours, minutes, seconds, millis.
// Keep the PC's float rounding and 16-bit intermediate seconds wrap.
std::array<std::uint16_t,4> license_time_449b30(std::uint32_t frames);
constexpr std::size_t PcLicenseChooserBytes=0xc768;
struct LicenseChooserState {
    float slide{},step{},target{}; // PC 84B220/224/228
    std::int8_t restore_slot{-1}; // PC 698AF9
};
float license_slide_4e0a70(LicenseChooserState&);
bool license_chooser_init_4e19e0(driving::Bytes,FrontendProfiles&,LicenseChooserState&,LicenseEditorServices&);
bool license_chooser_reset_4e1a70(driving::Bytes,FrontendProfiles&,LicenseChooserState&);
bool license_chooser_suspend_4e1ac0(driving::Bytes,FrontendProfiles&,LicenseChooserState&,LicenseEditorServices&);
void license_chooser_list_4e1c00(driving::Bytes,FrontendProfiles&);
bool license_chooser_context_4e1cf0(driving::Bytes,FrontendProfiles&,LicenseChooserState&,
                                   LicenseEditorServices&,std::uint32_t&);
// Dependencies (panel/text widgets and existing-profile context dialogs) are
// explicit calls, not synthesized action codes. A missing service fails the
// tick with missing_pc set and never returns an owner transition.
bool license_chooser_tick_4e2970(driving::Bytes,FrontendProfiles&,LicenseChooserState&,
                                LicenseEditorServices&,std::uint32_t& owner_action);
}
