// PC build of the Xbox port: a Win32 window with the same Direct3D 11,
// XAudio2 and gamepad layers, for testing on a PC before the console. The
// executable goes in the Steam game folder, like the original one.
#include "native_window.hpp"
#include <windows.h>
#include <shellapi.h>
#include <winrt/base.h>
#include <cstdio>
#include <string>
#include <vector>
namespace outrun::xbox_runtime {
namespace {
bool closed=false;
LRESULT CALLBACK window_proc(HWND hwnd,UINT message,WPARAM w,LPARAM l){
    if(message==WM_CLOSE){closed=true;return 0;}
    return DefWindowProcW(hwnd,message,w,l);
}
std::string utf8(const std::wstring& w){
    if(w.empty())return {};const int n=WideCharToMultiByte(CP_UTF8,0,w.data(),int(w.size()),nullptr,0,nullptr,nullptr);
    std::string s(std::size_t(n),'\0');WideCharToMultiByte(CP_UTF8,0,w.data(),int(w.size()),s.data(),n,nullptr,nullptr);return s;
}
}
NativeWindow& native_window(){
    static NativeWindow w;
    if(w.hwnd)return w;
    const HINSTANCE instance=GetModuleHandleW(nullptr);
    WNDCLASSW c{};c.lpfnWndProc=window_proc;c.hInstance=instance;c.lpszClassName=L"OutRunXbox";c.hCursor=LoadCursor(nullptr,IDC_ARROW);
    RegisterClassW(&c);
    RECT r{0,0,1280,720};AdjustWindowRect(&r,WS_OVERLAPPEDWINDOW,FALSE);
    w.hwnd=CreateWindowW(L"OutRunXbox",L"OutRun 2006 Coast 2 Coast",WS_OVERLAPPEDWINDOW|WS_VISIBLE,CW_USEDEFAULT,CW_USEDEFAULT,
                         r.right-r.left,r.bottom-r.top,nullptr,nullptr,instance,nullptr);
    w.width=1280;w.height=720;
    return w;
}
bool pump_window_events(){
    MSG m;while(PeekMessageW(&m,nullptr,0,0,PM_REMOVE)){TranslateMessage(&m);DispatchMessageW(&m);}
    return !closed;
}
std::string launch_folder(){
    std::wstring path(MAX_PATH,L'\0');
    const DWORD n=GetModuleFileNameW(nullptr,path.data(),DWORD(path.size()));path.resize(n);
    const auto slash=path.find_last_of(L"\\/");return utf8(slash==std::wstring::npos?L".":path.substr(0,slash));
}
std::string local_state_folder(){return {};}
unsigned select_refresh_rate(unsigned){
    // A desktop window presents at the monitor's rate; the PC build never changes the mode.
    MONITORINFOEXW info{};info.cbSize=sizeof info;
    if(!GetMonitorInfoW(MonitorFromWindow(static_cast<HWND>(native_window().hwnd),MONITOR_DEFAULTTOPRIMARY),&info))return 60;
    DEVMODEW mode{};mode.dmSize=sizeof mode;
    return EnumDisplaySettingsW(info.szDevice,ENUM_CURRENT_SETTINGS,&mode)&&mode.dmDisplayFrequency>1?unsigned(mode.dmDisplayFrequency):60u;
}
}
// A crash writes its address and the return addresses on the stack (as
// offsets in the executable, for llvm-symbolizer with the PDB) to runtime.log.
static LONG WINAPI crash_report(EXCEPTION_POINTERS* e){
    const auto base=reinterpret_cast<std::uintptr_t>(GetModuleHandleW(nullptr));
    std::fprintf(stderr,"crash: exception %08lx at exe+0x%llx\n",e->ExceptionRecord->ExceptionCode,
        (unsigned long long)(reinterpret_cast<std::uintptr_t>(e->ExceptionRecord->ExceptionAddress)-base));
    if(e->ExceptionRecord->NumberParameters>=2)std::fprintf(stderr,"  %s address %016llx\n",e->ExceptionRecord->ExceptionInformation[0]?"write":"read",
        (unsigned long long)e->ExceptionRecord->ExceptionInformation[1]);
    const auto& r=*e->ContextRecord;
    std::fprintf(stderr,"  rax=%016llx rbx=%016llx rcx=%016llx rdx=%016llx\n  rsi=%016llx rdi=%016llx r8=%016llx r9=%016llx rsp=%016llx\n",
        r.Rax,r.Rbx,r.Rcx,r.Rdx,r.Rsi,r.Rdi,r.R8,r.R9,r.Rsp);
    CONTEXT c=*e->ContextRecord;
    for(int depth=0;depth<24;++depth){
        DWORD64 image=0;auto* f=RtlLookupFunctionEntry(c.Rip,&image,nullptr);
        if(!f){c.Rip=*reinterpret_cast<DWORD64*>(c.Rsp);c.Rsp+=8;}
        else{void* data=nullptr;DWORD64 frame=0;RtlVirtualUnwind(UNW_FLAG_NHANDLER,image,c.Rip,f,&c,&data,&frame,nullptr);}
        if(!c.Rip)break;
        std::fprintf(stderr,"  #%d %s+0x%llx\n",depth,image==base?"exe":"other",(unsigned long long)(c.Rip-(image?image:0)));
    }
    std::fflush(stderr);
    return EXCEPTION_CONTINUE_SEARCH;
}
int WINAPI wWinMain(HINSTANCE,HINSTANCE,PWSTR,int){
    SetUnhandledExceptionFilter(crash_report);
    winrt::init_apartment();   // Windows.Gaming.Input
    const auto log=outrun::xbox_runtime::launch_folder()+"/runtime.log";
    if(std::freopen(log.c_str(),"w",stdout))std::setvbuf(stdout,nullptr,_IONBF,0);
    if(std::freopen(log.c_str(),"a",stderr))std::setvbuf(stderr,nullptr,_IONBF,0);
    std::vector<std::string> args;int count=0;
    if(wchar_t** argv=CommandLineToArgvW(GetCommandLineW(),&count)){
        for(int i=0;i<count;++i)args.push_back(outrun::xbox_runtime::utf8(argv[i]));LocalFree(argv);}
    std::vector<char*> argv;for(auto& a:args)argv.push_back(a.data());argv.push_back(nullptr);
    return or2_xbox_main(int(args.size()),argv.data());
}
