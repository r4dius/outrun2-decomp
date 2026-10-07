#pragma once
// Read access to data addressed by 32-bit PC virtual addresses, for ported
// code that follows pointers stored in retail files or EXE tables. Regions
// are byte copies (embedded EXE ranges, relocated retail blobs placed at a
// chosen base); no host pointer is ever formed from a PC address.
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <vector>
namespace outrun::platform {
struct PcExeRange {std::uint32_t base;const std::uint8_t* data;std::size_t size;};
// EXE-owned ranges (embedded_exe_ranges.cpp); data points into the player's
// EXE image once it is loaded (system/exe_image, bound by exe_tables.cpp).
extern PcExeRange EmbeddedExeRanges[];
extern const std::size_t EmbeddedExeRangeCount;
void bind_embedded_exe_ranges();
class PcAddressView {
public:
    struct Region {std::uint32_t base;const std::uint8_t* data;std::size_t size;};
    void add(std::uint32_t base,const std::uint8_t* data,std::size_t size){regions_.push_back({base,data,size});}
    void add_exe(){for(std::size_t i=0;i<EmbeddedExeRangeCount;++i)add(EmbeddedExeRanges[i].base,EmbeddedExeRanges[i].data,EmbeddedExeRanges[i].size);}
    const std::uint8_t* at(std::uint32_t address,std::size_t size)const{
        for(const auto& r:regions_)
            if(address>=r.base&&std::size_t(address-r.base)<=r.size&&size<=r.size-std::size_t(address-r.base))
                return r.data+(address-r.base);
        return nullptr;
    }
    bool readable(std::uint32_t address,std::size_t size)const{return at(address,size)!=nullptr;}
    // Reads return false for addresses outside every region.
    bool u8(std::uint32_t a,std::uint8_t& v)const{const auto* p=at(a,1);if(!p)return false;v=*p;return true;}
    bool u16(std::uint32_t a,std::uint16_t& v)const{const auto* p=at(a,2);if(!p)return false;std::memcpy(&v,p,2);return true;}
    bool u32(std::uint32_t a,std::uint32_t& v)const{const auto* p=at(a,4);if(!p)return false;std::memcpy(&v,p,4);return true;}
    bool f32(std::uint32_t a,float& v)const{const auto* p=at(a,4);if(!p)return false;std::memcpy(&v,p,4);return true;}
private:
    std::vector<Region> regions_;
};
// Copy of a relocatable retail container (Races.bin layout: dword categories,
// dword relocation count, (patch,target) pairs) with every patch rewritten to
// base+target, as 4F12A0 does in memory.
bool pc_relocate_blob(const std::vector<std::uint8_t>& file,std::uint32_t base,std::vector<std::uint8_t>& out);
}
