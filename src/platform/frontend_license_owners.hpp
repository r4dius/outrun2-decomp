#pragma once
#include "frontend_license_widgets.hpp"
namespace outrun::platform {
struct NativeRuntimeContext;
// Owned objects for the real key-21/key-24 factories. Runtime, pool, fonts and
// text are borrowed. The adapter must be detached before any of them die.
class FrontendLicenseOwners {
    NativeRuntimeContext& runtime_;
    FrontendUiResources ui_;
    const FrontendFontPack& fonts_;
    const FrontendTextTable& text_;
    std::array<std::uint8_t,PcLicenseChooserBytes> chooser_{};
    std::array<std::uint8_t,PcLicenseEditorBytes> editor_{};
    std::array<std::unique_ptr<FrontendLicenseWidgets>,2> widgets_;
    LicenseChooserState chooser_state_;
    std::uint8_t editing_existing_{};
    bool keyboard_focus_{};
    std::array<std::uint32_t,2> fault_{};
    void* platform_user_{};
    bool (*platform_call_)(void*,unsigned,const unsigned*,std::size_t,unsigned&){};
    std::string save_directory_;
    bool saves_enabled_{true};
    unsigned save_calls_{},delete_calls_{};
    int persistence_error_{};
    bool external(LicenseEditorServices&,unsigned,std::size_t,const unsigned*,std::size_t,unsigned&);
public:
    FrontendLicenseOwners(NativeRuntimeContext&,const FrontendFontPack&,const FrontendTextTable&);
    ~FrontendLicenseOwners();
    FrontendLicenseOwners(const FrontendLicenseOwners&)=delete;
    FrontendLicenseOwners& operator=(const FrontendLicenseOwners&)=delete;
    void platform(void* user,bool(*call)(void*,unsigned,const unsigned*,std::size_t,unsigned&));
    // Explicit write authorization/destination, independent of the read-only
    // retail asset root. Empty destination disables this service entirely.
    void persistence(std::string directory,bool enabled=true);
    // 416420 for owners outside the license screens (Showroom purchases).
    bool save_active();
    unsigned save_calls()const{return save_calls_;}
    unsigned delete_calls()const{return delete_calls_;}
    int persistence_error()const{return persistence_error_;}
    bool construct(unsigned key);
    bool invoke(unsigned key,unsigned slot,unsigned& result);
    void reset();
    void begin_frame();
    void* storage(unsigned key);
    std::size_t size(unsigned key)const;
    unsigned missing(unsigned key)const;
    const FrontendLicenseWidgets* widgets(unsigned key)const;
};
}
