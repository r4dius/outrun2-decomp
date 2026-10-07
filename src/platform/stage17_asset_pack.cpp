#include "platform/stage17_asset_pack.hpp"
#include <array>
#include <cstdio>
#include <cstring>
#include <vector>

namespace outrun::platform {
namespace {
constexpr std::array<std::uint8_t,8> Magic{{'O','R','2','S','1','7','P','1'}};
void fail(std::string* error,const char* message){if(error)*error=message;}
std::uint32_t u32(const std::uint8_t* p){
    return std::uint32_t(p[0])|(std::uint32_t(p[1])<<8u)|
           (std::uint32_t(p[2])<<16u)|(std::uint32_t(p[3])<<24u);
}
std::uint32_t crc32(const std::uint8_t* data,std::size_t size){
    std::uint32_t crc=0xffffffffu;
    for(std::size_t i=0;i<size;++i){
        crc^=data[i];
        for(unsigned bit=0;bit<8u;++bit)
            crc=(crc>>1u)^(0xedb88320u&std::uint32_t(0u-(crc&1u)));
    }
    return ~crc;
}
}

bool parse_stage17_asset_pack(const std::uint8_t* data,std::size_t size,
                              Stage17AssetPack& pack,std::string* error){
    if(error)error->clear();
    if((!data&&size!=0u)||size!=Stage17AssetPackFileSize){fail(error,"stage17 pack size mismatch");return false;}
    if(std::memcmp(data,Magic.data(),Magic.size())!=0){fail(error,"stage17 pack magic mismatch");return false;}
    if(u32(data+8u)!=Stage17AssetPackVersion||u32(data+12u)!=Stage17AssetPackHeaderSize||
       u32(data+16u)!=Stage17AssetRecordCount||u32(data+20u)!=Stage17AssetRecordSize||
       u32(data+28u)!=size){fail(error,"stage17 pack header mismatch");return false;}
    const auto expected_crc=u32(data+24u);
    if(crc32(data+Stage17AssetPackHeaderSize,size-Stage17AssetPackHeaderSize)!=expected_crc){
        fail(error,"stage17 pack payload CRC mismatch");return false;
    }
    Stage17AssetPack next{};next.payload_crc32=expected_crc;
    const auto* p=data+Stage17AssetPackHeaderSize;
    for(auto& record:next.records){
        record.token68=u32(p);p+=4u;
        std::memcpy(record.table68.data(),p,record.table68.size());p+=record.table68.size();
        record.token6c=u32(p);p+=4u;
        std::memcpy(record.table6c.data(),p,record.table6c.size());p+=record.table6c.size();
        // The retail primary descriptors used by this pack always reference
        // both tables. A zero token indicates an extraction/layout mismatch.
        if(record.token68==0u||record.token6c==0u){fail(error,"stage17 pack contains null table token");return false;}
    }
    if(p!=data+size){fail(error,"stage17 pack record cursor mismatch");return false;}
    pack=next;return true;
}

bool load_stage17_asset_pack_file(const char* path,Stage17AssetPack& pack,
                                  std::string* error){
    if(error)error->clear();
    if(!path){fail(error,"null stage17 pack path");return false;}
    std::FILE* file=std::fopen(path,"rb");if(!file){fail(error,"cannot open stage17 pack");return false;}
    if(std::fseek(file,0,SEEK_END)!=0){std::fclose(file);fail(error,"cannot size stage17 pack");return false;}
    const auto length=std::ftell(file);
    if(length<0||static_cast<std::size_t>(length)!=Stage17AssetPackFileSize){
        std::fclose(file);fail(error,"stage17 pack file size mismatch");return false;
    }
    std::rewind(file);std::vector<std::uint8_t> bytes(static_cast<std::size_t>(length));
    const auto read=std::fread(bytes.data(),1,bytes.size(),file);const int extra=std::fgetc(file);std::fclose(file);
    if(read!=bytes.size()||extra!=EOF){fail(error,"stage17 pack read mismatch");return false;}
    return parse_stage17_asset_pack(bytes.data(),bytes.size(),pack,error);
}

} // namespace outrun::platform
