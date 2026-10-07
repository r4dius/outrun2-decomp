#include "setup_display.hpp"
#ifdef OR2_PS5_OPENGL
#include "gl_display.hpp"
#elif defined(OR2_PS5_VULKAN)
#include "vk_display.hpp"
#else
#include "sdl_backend.hpp"
#endif
#include <string>
namespace outrun::ps5_runtime {
struct SetupDisplay::Impl {
#ifdef OR2_PS5_OPENGL
    GlDisplay display;
#elif defined(OR2_PS5_VULKAN)
    VulkanDisplay display;
#else
    SdlDisplay display;
#endif
};
SetupDisplay::SetupDisplay()=default;
SetupDisplay::~SetupDisplay(){close();}
void SetupDisplay::show(const platform::SetupScreen& screen){
    if(!impl_&&!failed_){
        impl_=std::make_unique<Impl>();std::string error;
        if(!impl_->display.open(unsigned(screen.width()),unsigned(screen.height()),error)){impl_.reset();failed_=true;return;}
    }
    if(!impl_)return;
    impl_->display.clear();
    impl_->display.rgba(this,reinterpret_cast<const std::uint8_t*>(screen.pixels()),unsigned(screen.width()),unsigned(screen.height()),
                        0,0,screen.width(),screen.height());
    std::string error;impl_->display.present(error);
}
void SetupDisplay::close(){impl_.reset();}
platform::SetupPlatform SetupDisplay::platform(){
    platform::SetupPlatform p;
    p.present=[this](const platform::SetupScreen& s){show(s);};
    p.keep_running=[]{return true;};
    p.exit_hint="Press OPTIONS to exit.";
    return p;
}
}
