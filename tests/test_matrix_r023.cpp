#include "driving/pc_matrix_stack.hpp"
#include <array>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <stdexcept>
using namespace outrun::driving;
namespace {
void req(bool v,const char* m){if(!v)throw std::runtime_error(m);}
std::uint32_t bits(float f){std::uint32_t u;std::memcpy(&u,&f,4);return u;}
void put_pattern(Bytes b){for(std::size_t i=0;i<b.size();++i)b.put8(i,std::uint8_t((i*37u+11u)&0xffu));}
}
int main(){try{
    {
        std::array<std::uint8_t,64*5> raw{};Bytes all(raw.data(),raw.size());put_pattern(all);
        auto before=raw;PcMatrixStack s{all,64,1,5};pc_matrix_push_unit(s);
        req(s.current_offset==128 && s.depth==2,"push-unit stack state");auto m=s.current();
        for(unsigned k=0;k<16;++k)req(m.u32(k*4)==(k%5==0?0x3f800000u:0u),"push-unit identity");
        for(std::size_t k=0;k<128;++k)req(raw[k]==before[k],"push-unit touched prior slots");
    }
    {
        std::array<std::uint8_t,64*3> raw{};Bytes all(raw.data(),raw.size());put_pattern(all);
        auto before=raw;PcMatrixStack s{all,64,2,3};pc_matrix_push_unit(s);
        req(s.current_offset==64 && s.depth==3,"overflow push-unit semantics");req(raw==before,"overflow push-unit wrote data");
    }
    {
        std::array<std::uint8_t,128> raw{};Bytes all(raw.data(),raw.size());put_pattern(all);
        PcMatrixStack s{all,0,0,2};auto before=raw;pc_matrix_unit_rotation(s);auto m=s.current();
        const unsigned diag[]={0x00,0x14,0x28};for(auto o:diag)req(m.u32(o)==0x3f800000u,"unit-rotation diagonal");
        const unsigned zeros[]={0x04,0x08,0x10,0x18,0x20,0x24};for(auto o:zeros)req(m.u32(o)==0,"unit-rotation off-diagonal");
        for(auto o:{0x0cu,0x1cu,0x2cu,0x30u,0x34u,0x38u,0x3cu})
            req(m.u32(o)==Bytes(before.data(),before.size()).u32(o),"unit-rotation clobbered preserved word");
    }
    {
        std::array<std::uint8_t,160> raw{};Bytes all(raw.data(),raw.size());put_pattern(all);
        PcMatrixStack s{all,0,0,2};std::array<std::uint8_t,64> out{};pc_matrix_get(s,Bytes(out.data(),out.size()));
        req(std::memcmp(out.data(),raw.data(),64)==0,"mxGetMatrix copy");
    }
    {
        // Forward REP MOVSD overlap: destination begins one dword after source,
        // so each copied dword becomes the source for the following iteration.
        std::array<std::uint8_t,160> raw{};Bytes all(raw.data(),raw.size());
        for(unsigned k=0;k<40;++k)all.put32(k*4,0x11110000u+k);
        PcMatrixStack s{all,0,0,2};const auto first=all.u32(0);pc_matrix_get(s,all.sub(4,64));
        for(unsigned k=0;k<16;++k)req(all.u32(4+k*4)==first,"mxGetMatrix overlap order");
    }
    std::cout<<"r023 matrix primitives: OK\n";return 0;
}catch(const std::exception& e){std::cerr<<"FAIL: "<<e.what()<<"\n";return 1;}}
