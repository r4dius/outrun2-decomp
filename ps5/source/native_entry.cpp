#include <cstdio>
#include <sys/stat.h>
// Compiled with the native title CRT; the game archive retains C++ exceptions.
int or2_ps5_main(int argc,char** argv);
int main(int argc,char** argv){
    mkdir("/download0/OutRunPS5",0777);
    // Both streams must append: independent write/append positions otherwise
    // let stdout overwrite the driver's stderr diagnostics.
    if(auto* log=std::fopen("/download0/OutRunPS5/runtime.log","w"))std::fclose(log);
    if(std::freopen("/download0/OutRunPS5/runtime.log","a",stdout))std::setvbuf(stdout,nullptr,_IONBF,0);
    if(std::freopen("/download0/OutRunPS5/runtime.log","a",stderr))std::setvbuf(stderr,nullptr,_IONBF,0);
    return or2_ps5_main(argc,argv);
}
// thread_local objects with destructors (race_translated.cpp) register them
// through this C++ ABI hook, which the native title CRT does not export. The
// game threads live until the process ends, so the destructors are not needed.
#ifndef OR2_PS5_VULKAN
extern "C" int __cxa_thread_atexit_impl(void (*)(void*),void*,void*){return 0;}
#endif
