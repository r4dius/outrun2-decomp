#pragma once
// Guest C runtime for translated modules: a heap in a guest block (the
// statically linked VS2005 CRT's malloc / free / operator new / realloc) and
// the string, memory and formatting functions, answered natively by their
// Steam addresses (the translated code calls them as PcRaceService callees).
#include "platform/race_area.hpp"
#include <cstdint>
#include <functional>
#include <map>
#include <string>
#include <vector>
namespace outrun::platform {
class GuestHeap {
public:
    GuestHeap(std::uint32_t base,std::uint32_t size);
    void map(PcRaceMemory&);
    std::uint32_t alloc(std::uint32_t n);               // 0 when the block is full
    void free(std::uint32_t a);                         // ignores 0 and unknown addresses
    std::uint32_t realloc(PcRaceMemory&,std::uint32_t a,std::uint32_t n);
    std::uint32_t size_of(std::uint32_t a)const;
    std::uint32_t used()const{return used_bytes_;}
private:
    std::uint32_t base_,size_,used_bytes_{};
    std::vector<std::uint8_t> bytes_;
    std::map<std::uint32_t,std::uint32_t> used_,free_;  // address -> size (16-byte granules)
};
// printf-style formatting over guest memory (integer, character and string
// conversions with flags / width / precision); arg() yields the next argument.
std::string guest_format(PcRaceMemory&,std::uint32_t format,const std::function<std::uint32_t()>& arg);
std::string guest_string(PcRaceMemory&,std::uint32_t address);
void guest_put_string(PcRaceMemory&,std::uint32_t address,const std::string&);
// The CRT callee `call.pc` (malloc 580253, free 580BC2 / 5801A7, new 5802CF,
// _callnewh 580195, realloc 581B19, memmove 580340, strchr 5810D0, strncpy
// 581780, sprintf 5802DD, _vsnprintf 580265, sscanf 580F62, toupper 5822D2):
// true with EAX when it is one of them.
bool translated_crt_call(PcRaceMemory&,GuestHeap&,const PcRaceCall&,std::uint32_t& eax);
}
