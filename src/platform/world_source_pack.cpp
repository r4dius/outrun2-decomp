#include "platform/world_source_pack.hpp"
#include <cstdio>
#include <cstring>
#include <utility>

namespace outrun::platform {
namespace {
constexpr std::size_t HeaderBytes=32u,EntryBytes=24u,MaxBytes=4u*1024u*1024u;
constexpr std::array<std::uint32_t,7> Fields{{0x0cu,0x20u,0x24u,0x28u,0x2cu,0x30u,0x64u}};
void fail(std::string* error,const char* message){if(error)*error=message;}
std::uint32_t u32(const std::uint8_t* p){
    return std::uint32_t(p[0])|(std::uint32_t(p[1])<<8u)|
        (std::uint32_t(p[2])<<16u)|(std::uint32_t(p[3])<<24u);
}
std::uint32_t crc32(const std::uint8_t* data,std::size_t size){
    std::uint32_t result=0xffffffffu;
    for(std::size_t i=0;i<size;++i){
        result^=data[i];
        for(unsigned bit=0;bit<8u;++bit)
            result=(result>>1u)^((result&1u)?0xedb88320u:0u);
    }
    return ~result;
}
}

bool parse_world_source_pack(const std::uint8_t* data,std::size_t size,
                             WorldSourcePack& pack,std::string* error){
    if(error)error->clear();
    if(!data||size<HeaderBytes+Fields.size()*EntryBytes||size>MaxBytes||
       std::memcmp(data,"OR2SRC1\0",8u)!=0||u32(data+8u)!=1u||
       u32(data+12u)!=Fields.size()||u32(data+16u)!=HeaderBytes||
       u32(data+20u)!=size||u32(data+24u)!=15u||u32(data+28u)==0u){
        fail(error,"world source header mismatch");return false;
    }
    WorldSourcePack next{};
    next.descriptor_index=u32(data+24u);next.descriptor_token=u32(data+28u);
    next.bytes.assign(data,data+size);
    std::size_t expected_offset=HeaderBytes+Fields.size()*EntryBytes;
    for(std::size_t i=0;i<Fields.size();++i){
        const auto* p=data+HeaderBytes+i*EntryBytes;
        auto& entry=next.entries[i];
        entry={u32(p),u32(p+4u),u32(p+8u),u32(p+12u),u32(p+16u),u32(p+20u)};
        if(entry.descriptor_field!=Fields[i]||entry.guest_path_token==0u||
           entry.offset!=expected_offset||entry.size==0u||
           entry.size>2u*1024u*1024u||entry.compressed_size==0u||
           entry.offset>size||entry.size>size-entry.offset||
           crc32(data+entry.offset,entry.size)!=entry.crc32){
            fail(error,"world source entry mismatch");return false;
        }
        expected_offset+=entry.size;
    }
    if(expected_offset!=size){fail(error,"world source tail mismatch");return false;}
    const auto& collision=next.entries[0];
    const auto* raw=data+collision.offset;
    if(collision.size<64u||u32(raw)+4u!=collision.size||
       std::memcmp(raw+4u,"COLI0200",8u)!=0||u32(raw+20u)!=64u||
       u32(raw+12u)==0u){
        fail(error,"world source collision mismatch");return false;
    }
    pack=std::move(next);return true;
}

bool load_world_source_pack_file(const char* path,WorldSourcePack& pack,
                                 std::string* error){
    if(error)error->clear();
    if(!path){fail(error,"null world source path");return false;}
    std::FILE* file=std::fopen(path,"rb");
    if(!file){fail(error,"cannot open world source pack");return false;}
    if(std::fseek(file,0,SEEK_END)!=0){std::fclose(file);fail(error,"cannot size world source pack");return false;}
    const auto length=std::ftell(file);
    if(length<0||static_cast<std::uint64_t>(length)>MaxBytes){
        std::fclose(file);fail(error,"world source size outside bounds");return false;
    }
    std::rewind(file);
    std::vector<std::uint8_t> bytes(static_cast<std::size_t>(length));
    const auto read=bytes.empty()?0u:std::fread(bytes.data(),1,bytes.size(),file);
    const int extra=std::fgetc(file);std::fclose(file);
    if(read!=bytes.size()||extra!=EOF){fail(error,"world source read mismatch");return false;}
    return parse_world_source_pack(bytes.data(),bytes.size(),pack,error);
}

const WorldSourceEntry* world_source_find(const WorldSourcePack& pack,
                                          std::uint32_t field,
                                          std::uint32_t guest_path_token){
    for(const auto& entry:pack.entries)
        if(entry.descriptor_field==field&&entry.guest_path_token==guest_path_token)
            return &entry;
    return nullptr;
}

const std::uint8_t* world_source_bytes(const WorldSourcePack& pack,
                                       const WorldSourceEntry& entry){
    if(entry.offset>pack.bytes.size()||entry.size>pack.bytes.size()-entry.offset)
        return nullptr;
    return pack.bytes.data()+entry.offset;
}

bool build_beac_world_source_from_retail(RetailAssetStore& store,
                                         const CourseAssetPack& course,
                                         WorldSourcePack& pack,
                                         std::string* error){
    if(error)error->clear();
    if(course.descriptors.primary_count<=15u){fail(error,"BEAC descriptor unavailable");return false;}
    static constexpr std::array<const char*,7> paths{{
        "Stage/BEAC/coli_CS_BEAC_bin.sz","Stage/BEAC/cs_CS_BEAC_bin.sz",
        "Stage/BEAC/scn_env_fog_BEAC_bin.sz","Stage/BEAC/scn_env_sun_BEAC_bin.sz",
        "Stage/BEAC/maya_spl_BEAC_bin.sz","Stage/BEAC/oso_cs_beac.sz",
        "Stage/BEAC/cs_ENV_BEAC_bin.sz"}};
    const auto& descriptor=course.descriptors.storage[15u];
    WorldSourcePack next{};next.descriptor_index=15u;next.descriptor_token=0x007d30bcu;
    for(std::size_t i=0;i<Fields.size();++i){
        std::vector<std::uint8_t> inflated,compressed;
        std::string local_error;
        if(!retail_asset_read_relative(store,paths[i],compressed,2u*1024u*1024u,&local_error)||
           !retail_asset_read_relative_inflated(store,paths[i],inflated,2u*1024u*1024u,&local_error)){
            if(error)*error=local_error.empty()?"BEAC retail world asset unavailable":local_error;
            return false;
        }
        auto& e=next.entries[i];e.descriptor_field=Fields[i];e.guest_path_token=u32(descriptor.data()+Fields[i]);
        if(e.guest_path_token==0u||inflated.empty()){fail(error,"BEAC descriptor/path token mismatch");return false;}
        e.offset=static_cast<std::uint32_t>(next.bytes.size());e.size=static_cast<std::uint32_t>(inflated.size());
        e.compressed_size=static_cast<std::uint32_t>(compressed.size());e.crc32=crc32(inflated.data(),inflated.size());
        next.bytes.insert(next.bytes.end(),inflated.begin(),inflated.end());
    }
    const auto& collision=next.entries[0];const auto* raw=world_source_bytes(next,collision);
    if(!raw||collision.size<64u||u32(raw)+4u!=collision.size||std::memcmp(raw+4u,"COLI0200",8u)!=0){
        fail(error,"retail BEAC collision payload invalid");return false;
    }
    pack=std::move(next);return true;
}

} // namespace outrun::platform
