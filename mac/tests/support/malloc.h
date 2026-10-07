#pragma once
// macOS compatibility for the inherited Switch audio test only. The real
// Mac game uses game/audio.cpp (SDL); Switch keeps its original libnx code.
// Keep the allocation shim here rather than adding Apple code to switch/.
#include <cstdlib>

inline void* memalign(std::size_t alignment,std::size_t size){
    void* memory=nullptr;
    return posix_memalign(&memory,alignment,size)==0?memory:nullptr;
}
