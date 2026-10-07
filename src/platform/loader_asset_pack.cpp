#include "platform/loader_asset_pack.hpp"
#include <array>
#include <cstdio>
#include <cstring>
#include <limits>
#include <utility>

namespace outrun::platform {
namespace {
constexpr std::array<std::uint8_t,8> Magic{{'O','R','2','L','D','R','1','2'}};
constexpr std::array<std::uint8_t,8> LegacyMagic11{{'O','R','2','L','D','R','1','1'}};
constexpr std::array<std::uint8_t,8> LegacyMagic10{{'O','R','2','L','D','R','1','0'}};
constexpr std::array<std::uint8_t,8> LegacyMagic9{{'O','R','2','L','D','R','9',0}};
constexpr std::array<std::uint8_t,8> LegacyMagic8{{'O','R','2','L','D','R','8',0}};
constexpr std::array<std::uint8_t,8> LegacyMagic7{{'O','R','2','L','D','R','7',0}};
constexpr std::array<std::uint8_t,8> LegacyMagic6{{'O','R','2','L','D','R','6',0}};
constexpr std::array<std::uint8_t,8> LegacyMagic5{{'O','R','2','L','D','R','5',0}};
constexpr std::array<std::uint8_t,8> LegacyMagic4{{'O','R','2','L','D','R','4',0}};
constexpr std::size_t MaxPackBytes=24u*1024u*1024u;
void fail(std::string* error,const char* message){if(error)*error=message;}
std::uint32_t u32(const std::uint8_t* p){return std::uint32_t(p[0])|(std::uint32_t(p[1])<<8u)|(std::uint32_t(p[2])<<16u)|(std::uint32_t(p[3])<<24u);}
std::uint32_t crc32(const std::uint8_t* data,std::size_t size){
    std::uint32_t crc=0xffffffffu;
    for(std::size_t i=0;i<size;++i){crc^=data[i];for(unsigned bit=0;bit<8u;++bit)crc=(crc>>1u)^(0xedb88320u&std::uint32_t(0u-(crc&1u)));}
    return ~crc;
}
bool zlib_header(const std::uint8_t* data,std::size_t size){
    if(size<2u||(data[0]&0x0fu)!=8u)return false;
    return ((std::uint32_t(data[0])<<8u)|data[1])%31u==0u;
}
}

const LoaderAssetRecord* find_loader_asset(const LoaderAssetPack& pack,
                                            std::uint32_t resource_id,
                                            std::uint32_t request_mode){
    for(const auto& record:pack.records)
        if(record.resource_id==resource_id&&record.request_mode==request_mode)return &record;
    return nullptr;
}

bool parse_loader_asset_pack(const std::uint8_t* data,std::size_t size,
                             LoaderAssetPack& pack,std::string* error){
    if(error)error->clear();
    if((!data&&size!=0u)||size<LoaderAssetPackHeaderSize||size>MaxPackBytes){fail(error,"loader pack size outside bounds");return false;}
    const bool legacy4=std::memcmp(data,LegacyMagic4.data(),LegacyMagic4.size())==0;
    const bool legacy5=std::memcmp(data,LegacyMagic5.data(),LegacyMagic5.size())==0;
    const bool legacy6=std::memcmp(data,LegacyMagic6.data(),LegacyMagic6.size())==0;
    const bool legacy7=std::memcmp(data,LegacyMagic7.data(),LegacyMagic7.size())==0;
    const bool legacy8=std::memcmp(data,LegacyMagic8.data(),LegacyMagic8.size())==0;
    const bool legacy9=std::memcmp(data,LegacyMagic9.data(),LegacyMagic9.size())==0;
    const bool legacy10=std::memcmp(data,LegacyMagic10.data(),LegacyMagic10.size())==0;
    const bool legacy11=std::memcmp(data,LegacyMagic11.data(),LegacyMagic11.size())==0;
    if(!legacy4&&!legacy5&&!legacy6&&!legacy7&&!legacy8&&!legacy9&&!legacy10&&!legacy11&&std::memcmp(data,Magic.data(),Magic.size())!=0){fail(error,"loader pack magic mismatch");return false;}
    const auto count=u32(data+16u),record_size=u32(data+20u),record_offset=u32(data+24u),payload_offset=u32(data+28u),file_size=u32(data+32u);
    const auto expected_count=legacy4?7u+LoaderAssetFrontendScriptCount:
        legacy5?9u+LoaderAssetFrontendScriptCount:
        legacy6?17u+LoaderAssetFrontendScriptCount:
        legacy7?28u+LoaderAssetFrontendScriptCount:
        legacy8?33u+LoaderAssetFrontendScriptCount:
        legacy9?48u+LoaderAssetFrontendScriptCount:
        legacy10?49u+LoaderAssetFrontendScriptCount:
        legacy11?52u+LoaderAssetFrontendScriptCount:LoaderAssetPackExpectedRecords;
    const auto expected_version=legacy4?4u:legacy5?5u:legacy6?6u:legacy7?7u:legacy8?8u:legacy9?9u:legacy10?10u:legacy11?11u:LoaderAssetPackVersion;
    if(u32(data+8u)!=expected_version||u32(data+12u)!=LoaderAssetPackHeaderSize||count!=expected_count||record_size!=LoaderAssetPackRecordSize||record_offset!=LoaderAssetPackHeaderSize||payload_offset!=LoaderAssetPackHeaderSize+expected_count*LoaderAssetPackRecordSize||file_size!=size){fail(error,"loader pack header mismatch");return false;}
    for(std::size_t off=36u;off<LoaderAssetPackHeaderSize;off+=4u)if(u32(data+off)!=0u){fail(error,"loader pack reserved header field");return false;}

    LoaderAssetPack next{};next.records.reserve(count);std::size_t running=payload_offset;
    constexpr std::array<std::uint32_t,55> fixed_ids{{
        0xbau,0xbbu,0x2cu,0x33u,0x48u,0x44u,LoaderAssetSelectTableId,
        LoaderAssetStartLoadingId,LoaderAssetStartVersusId,
        LoaderAssetGameSpritesId,0x2du,0x3eu,0x3fu,0x40u,0x41u,0x46u,0x4au,
        0x23u,0x24u,0x25u,0x26u,0x27u,0x28u,0x29u,0x2au,0x30u,0x31u,0x3cu,
        0xbau,0x57u,0x1efu,0xbbu,0xbeu,
        LoaderAssetMotionTableId,LoaderAssetBoneTableId,
        LoaderAssetMotionGroup6Id,LoaderAssetMotionGroup32Id,
        0xbcu,0xc8u,0x0bu,
        loader_motion_group_id(34u),loader_motion_group_id(35u),
        loader_motion_group_id(36u),loader_motion_group_id(37u),
        loader_motion_group_id(38u),loader_motion_group_id(39u),
        loader_motion_group_id(23u),loader_motion_group_id(33u),0xc3u,
        0x162u,0x19eu,0x180u,
        0xdbu,0xfeu,0x126u}};
    constexpr std::array<std::uint32_t,55> fixed_modes{{
        2u,2u,8u,8u,8u,9u,0u,2u,2u,8u,8u,8u,8u,8u,8u,8u,8u,
        8u,8u,8u,8u,8u,8u,8u,8u,8u,8u,9u,8u,8u,8u,8u,8u,
        0u,0u,0u,0u,8u,8u,8u,
        0u,0u,0u,0u,0u,0u,0u,0u,9u,9u,9u,9u,9u,9u,9u}};
    const auto fixed_count=legacy4?7u:legacy5?9u:legacy6?17u:
        legacy7?28u:legacy8?33u:legacy9?48u:legacy10?49u:
        legacy11?52u:fixed_ids.size();
    for(std::size_t i=0;i<count;++i){
        const auto* r=data+record_offset+i*LoaderAssetPackRecordSize;
        const auto id=u32(r),mode=u32(r+4u),offset=u32(r+8u),bytes=u32(r+12u),expected_crc=u32(r+16u);
        const auto expected_id=i<fixed_count?fixed_ids[i]:
            LoaderAssetFrontendScriptBaseId+static_cast<std::uint32_t>(i-fixed_count);
        const auto expected_mode=i<fixed_count?fixed_modes[i]:0u;
        if(id!=expected_id||mode!=expected_mode||offset!=running||bytes==0u||bytes>size-running||u32(r+20u)!=0u||u32(r+24u)!=0u||u32(r+28u)!=0u){fail(error,"loader pack record mismatch");return false;}
        const bool direct_table=id==LoaderAssetSelectTableId;
        const bool motion_table=id==LoaderAssetMotionTableId;
        const bool bone_table=id==LoaderAssetBoneTableId;
        const bool motion_group6=id==LoaderAssetMotionGroup6Id;
        const bool motion_group32=id==LoaderAssetMotionGroup32Id;
        const bool extra_motion_group=i>=40u&&i<48u&&!legacy4&&!legacy5&&
            !legacy6&&!legacy7&&!legacy8;
        const bool direct_chara=!legacy9&&i==48u&&id==0xc3u&&mode==9u;
        const bool frontend_script=id>=LoaderAssetFrontendScriptBaseId&&
            id<LoaderAssetFrontendScriptBaseId+LoaderAssetFrontendScriptCount;
        if(crc32(data+offset,bytes)!=expected_crc||
           (direct_table?bytes!=LoaderAssetSelectTableBytes:
            motion_table?bytes!=15360u:
            bone_table?bytes!=42496u:
            motion_group6?(bytes!=125444u||u32(data+offset)!=125440u):
            motion_group32?(bytes!=52740u||u32(data+offset)!=52736u):
            extra_motion_group?(bytes<8u||bytes>1024u*1024u||
                                u32(data+offset)!=bytes-4u):
            direct_chara?bytes!=1712u:
            (!frontend_script&&!zlib_header(data+offset,bytes)))){fail(error,"loader pack payload validation failed");return false;}
        LoaderAssetRecord record{};record.resource_id=id;record.request_mode=mode;record.bytes.assign(data+offset,data+offset+bytes);next.records.push_back(std::move(record));running+=bytes;
    }
    if(running!=size){fail(error,"loader pack trailing bytes");return false;}
    pack=std::move(next);return true;
}

bool load_loader_asset_pack_file(const char* path,LoaderAssetPack& pack,
                                 std::string* error){
    if(error)error->clear();
    if(!path){fail(error,"null loader pack path");return false;}
    std::FILE* file=std::fopen(path,"rb");if(!file){fail(error,"cannot open loader pack");return false;}
    if(std::fseek(file,0,SEEK_END)!=0){std::fclose(file);fail(error,"cannot size loader pack");return false;}
    const auto length=std::ftell(file);if(length<0||static_cast<std::uint64_t>(length)>MaxPackBytes){std::fclose(file);fail(error,"loader pack file outside bounds");return false;}
    std::rewind(file);std::vector<std::uint8_t> bytes(static_cast<std::size_t>(length));const auto read=bytes.empty()?0u:std::fread(bytes.data(),1,bytes.size(),file);const int extra=std::fgetc(file);std::fclose(file);
    if(read!=bytes.size()||extra!=EOF){fail(error,"loader pack read mismatch");return false;}
    return parse_loader_asset_pack(bytes.data(),bytes.size(),pack,error);
}

} // namespace outrun::platform
