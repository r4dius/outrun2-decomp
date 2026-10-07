// Xbox (UWP, Developer Mode) entry: the application's CoreWindow. The game
// loop runs in IFrameworkView::Run on the window's thread and dispatches the
// window events once per frame (pump_window_events).
#include "native_window.hpp"
#include <windows.h>
#include <winrt/Windows.ApplicationModel.h>
#include <winrt/Windows.ApplicationModel.Core.h>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.Graphics.Display.Core.h>
#include <winrt/Windows.Storage.h>
#include <winrt/Windows.UI.Core.h>
#include <cstdio>
namespace outrun::xbox_runtime {
using namespace winrt::Windows::ApplicationModel::Core;
using namespace winrt::Windows::UI::Core;
namespace {
CoreWindow window{nullptr};
bool closed=false;
struct App:winrt::implements<App,IFrameworkViewSource,IFrameworkView> {
    IFrameworkView CreateView(){return *this;}
    void Initialize(const CoreApplicationView&){}
    void Load(const winrt::hstring&){}
    void Uninitialize(){}
    void SetWindow(const CoreWindow& w){window=w;window.Closed([](auto&&,auto&&){closed=true;});}
    void Run(){
        window.Activate();
        const auto log=local_state_folder()+"/runtime.log";
        if(std::freopen(log.c_str(),"w",stdout))std::setvbuf(stdout,nullptr,_IONBF,0);
        if(std::freopen(log.c_str(),"a",stderr))std::setvbuf(stderr,nullptr,_IONBF,0);
        char name[]="OutRunXbox";char* argv[]{name,nullptr};
        or2_xbox_main(1,argv);
        CoreApplication::Exit();
    }
};
}
NativeWindow& native_window(){
    static NativeWindow w;
    // The Xbox shell gives a game a 1920x1080 window; the swap chain is stretched to the output.
    if(!w.core_window){w.core_window=winrt::get_abi(window);w.width=1920;w.height=1080;}
    return w;
}
bool pump_window_events(){window.Dispatcher().ProcessEvents(CoreProcessEventsOption::ProcessAllIfPresent);return !closed;}
std::string launch_folder(){return winrt::to_string(winrt::Windows::ApplicationModel::Package::Current().InstalledLocation().Path());}
std::string local_state_folder(){return winrt::to_string(winrt::Windows::Storage::ApplicationData::Current().LocalFolder().Path());}
unsigned select_refresh_rate(unsigned hz){
    using namespace winrt::Windows::Graphics::Display::Core;
    try{
        const auto hdmi=HdmiDisplayInformation::GetForCurrentView();if(!hdmi)return 60;
        const auto current=hdmi.GetCurrentDisplayMode();
        if(current.RefreshRate()>=hz-1.0)return unsigned(current.RefreshRate()+0.5);
        // The same resolution and colour space at the wanted rate (Xbox One X: 1080p/1440p at 120 Hz).
        for(const auto& mode:hdmi.GetSupportedDisplayModes())
            if(mode.ResolutionWidthInRawPixels()==current.ResolutionWidthInRawPixels()&&mode.ResolutionHeightInRawPixels()==current.ResolutionHeightInRawPixels()&&
               mode.ColorSpace()==current.ColorSpace()&&mode.RefreshRate()>=hz-1.0){
                hdmi.RequestSetCurrentDisplayModeAsync(mode);   // the shell switches the TV asynchronously
                return unsigned(mode.RefreshRate()+0.5);
            }
        return unsigned(current.RefreshRate()+0.5);
    }catch(...){return 60;}
}
}
int WINAPI wWinMain(HINSTANCE,HINSTANCE,PWSTR,int){
    winrt::Windows::ApplicationModel::Core::CoreApplication::Run(winrt::make<outrun::xbox_runtime::App>());
    return 0;
}
