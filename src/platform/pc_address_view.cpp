#include "platform/pc_address_view.hpp"
namespace outrun::platform {
bool pc_relocate_blob(const std::vector<std::uint8_t>& file,std::uint32_t base,std::vector<std::uint8_t>& out){
    if(file.size()<8u)return false;
    std::uint32_t relocs;std::memcpy(&relocs,file.data()+4,4);
    if(std::size_t(relocs)>(file.size()-8u)/8u)return false;
    std::vector<std::uint8_t> copy(file);
    for(std::size_t i=0;i<relocs;++i){
        std::uint32_t patch,target;std::memcpy(&patch,file.data()+8+i*8,4);std::memcpy(&target,file.data()+12+i*8,4);
        if(patch>file.size()||file.size()-patch<4u||target>file.size())return false;
        const std::uint32_t value=base+target;                       // 4F13F0: [base+patch] = base+target
        std::memcpy(copy.data()+patch,&value,4);
    }
    out=std::move(copy);return true;
}
}
