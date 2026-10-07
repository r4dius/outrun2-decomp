#include "platform/course_asset_pack.hpp"
#include <array>
#include <cstdio>
#include <cstring>
#include <utility>

namespace outrun::platform {
namespace {
constexpr std::array<std::uint8_t,8> Magic{{'O','R','2','C','R','S','3',0}};
constexpr std::array<std::uint8_t,8> LegacyMagic2{{'O','R','2','C','R','S','2',0}};
constexpr std::array<std::uint8_t,8> LegacyMagic1{{'O','R','2','C','R','S','1',0}};
constexpr std::size_t MaxPackBytes=64u*1024u;
void fail(std::string* error,const char* message){if(error)*error=message;}
std::uint32_t u32(const std::uint8_t* p){return std::uint32_t(p[0])|(std::uint32_t(p[1])<<8u)|(std::uint32_t(p[2])<<16u)|(std::uint32_t(p[3])<<24u);}
std::uint32_t crc32(const std::uint8_t* data,std::size_t size){
    std::uint32_t crc=0xffffffffu;
    for(std::size_t i=0;i<size;++i){crc^=data[i];for(unsigned bit=0;bit<8u;++bit)crc=(crc>>1u)^(0xedb88320u&std::uint32_t(0u-(crc&1u)));}
    return ~crc;
}
}

bool parse_course_asset_pack(const std::uint8_t* data,std::size_t size,
                             CourseAssetPack& pack,std::string* error){
    if(error)error->clear();
    if((!data&&size!=0u)||size<CourseAssetPackHeaderSize||size>MaxPackBytes){fail(error,"course pack size outside bounds");return false;}
    const bool legacy1=std::memcmp(data,LegacyMagic1.data(),LegacyMagic1.size())==0;
    const bool legacy2=std::memcmp(data,LegacyMagic2.data(),LegacyMagic2.size())==0;
    if(!legacy1&&!legacy2&&std::memcmp(data,Magic.data(),Magic.size())!=0){fail(error,"course pack magic mismatch");return false;}
    const auto descriptor_bytes=u32(data+16u),course_bytes=u32(data+20u);
    const auto descriptor_offset=u32(data+32u),course_offset=u32(data+36u),file_size=u32(data+40u);
    if(u32(data+8u)!=(legacy1?1u:legacy2?2u:CourseAssetPackVersion)||u32(data+12u)!=CourseAssetPackHeaderSize||
       descriptor_bytes!=(legacy1?CourseDescriptorPackBytesLegacy:
                         legacy2?CourseDescriptorPackBytes:CourseDescriptorPackBytesFull)||
       course_bytes!=CourseCvtBlobBytes||
       descriptor_offset!=CourseAssetPackHeaderSize||
       course_offset!=descriptor_offset+descriptor_bytes||file_size!=size||
       u32(data+44u)!=0u||course_offset>size||course_bytes>size-course_offset||
       crc32(data+descriptor_offset,descriptor_bytes)!=u32(data+24u)||
       crc32(data+course_offset,course_bytes)!=u32(data+28u)){
        fail(error,"course pack header/payload mismatch");return false;
    }
    CourseAssetPack next{};
    if(!driving::course_descriptor_pack_open_r078(
           const_cast<std::uint8_t*>(data+descriptor_offset),descriptor_bytes,next.descriptors)||
       next.descriptors.primary_count!=driving::PcCoursePrimaryDescriptorCountR078||
       next.descriptors.secondary_count!=driving::PcCourseSecondaryDescriptorCountR078){
        fail(error,"course descriptor payload invalid");return false;
    }
    next.course_blob.assign(data+course_offset,data+course_offset+course_bytes);
    driving::PcRelocCategoryBlobR077 blob{};
    if(!driving::course_reloc_blob_open_r077(next.course_blob.data(),next.course_blob.size(),blob)||
       driving::runtime_category_count_4f1ba0("csc_data_cvt_course",blob)!=CourseCvtRecordCount){
        fail(error,"course csc_data_cvt payload invalid");return false;
    }
    const auto* records=static_cast<const std::uint8_t*>(
        driving::runtime_category_records_4f1a90("csc_data_cvt_course",blob));
    const auto* base=next.course_blob.data();
    const auto record_bytes=std::size_t(CourseCvtRecordCount)*driving::PcCourseRecord44d720Size;
    if(records==nullptr||records<base||std::size_t(records-base)>next.course_blob.size()||
       record_bytes>next.course_blob.size()-std::size_t(records-base)){
        fail(error,"course record table outside payload");return false;
    }
    for(std::int32_t i=0;i<CourseCvtRecordCount;++i){
        const auto* record=records+std::size_t(i)*driving::PcCourseRecord44d720Size;
        if(u32(record+0x1cu)>=driving::PcCoursePrimaryDescriptorCountR078||
           u32(record+0x20u)>=driving::PcCourseSecondaryDescriptorCountR078){
            fail(error,"course descriptor index outside tables");return false;
        }
        const auto own=u32(record+0x04u);
        for(const auto child_offset:std::array<std::size_t,2>{{0x2cu,0x30u}}){
            const auto child=u32(record+child_offset);
            if(child==0xffffffffu||child<=own)continue;
            bool found=false;
            for(std::int32_t j=0;j<CourseCvtRecordCount;++j)
                if(u32(records+std::size_t(j)*driving::PcCourseRecord44d720Size+0x04u)==child){
                    found=true;break;
                }
            if(!found){fail(error,"course child key missing from record table");return false;}
        }
    }
    pack=std::move(next);
    // Moving rewrites the address of descriptor storage. Re-open in the final
    // object so every primary native view points at its stable owner.
    if(!driving::course_descriptor_pack_open_r078(
           const_cast<std::uint8_t*>(data+descriptor_offset),descriptor_bytes,pack.descriptors)){
        pack={};fail(error,"course descriptor finalization failed");return false;
    }
    return true;
}

bool load_course_asset_pack_file(const char* path,CourseAssetPack& pack,
                                 std::string* error){
    if(error)error->clear();
    if(!path){fail(error,"null course pack path");return false;}
    std::FILE* file=std::fopen(path,"rb");if(!file){fail(error,"cannot open course pack");return false;}
    if(std::fseek(file,0,SEEK_END)!=0){std::fclose(file);fail(error,"cannot size course pack");return false;}
    const auto length=std::ftell(file);
    if(length<0||static_cast<std::uint64_t>(length)>MaxPackBytes){std::fclose(file);fail(error,"course pack file outside bounds");return false;}
    std::rewind(file);std::vector<std::uint8_t> bytes(static_cast<std::size_t>(length));
    const auto read=bytes.empty()?0u:std::fread(bytes.data(),1,bytes.size(),file);const int extra=std::fgetc(file);std::fclose(file);
    if(read!=bytes.size()||extra!=EOF){fail(error,"course pack read mismatch");return false;}
    return parse_course_asset_pack(bytes.data(),bytes.size(),pack,error);
}

bool course_field_44c220(const std::uint8_t* primary,std::size_t primary_size,
                         const std::uint8_t* secondary,std::size_t secondary_size,
                         std::uint32_t index,std::uint32_t& value){
    // The exact public-entry/VM dispatch table, measured with two distinct
    // PC-layout objects. The switch's default branch returns zero.
    static constexpr std::array<std::uint8_t,16> offsets{{
        0x0c,0x0c,0x18,0x1c,0x20,0x24,0x28,0x2c,
        0x30,0x30,0x34,0x34,0x64,0x68,0x68,0x6c
    }};
    static constexpr std::array<bool,16> use_secondary{{
        false,true,true,true,false,false,false,false,
        false,true,false,true,false,false,true,false
    }};
    if(index>=offsets.size()){value=0u;return true;}
    const bool alternate=use_secondary[index];
    const auto* data=alternate?secondary:primary;
    const auto size=alternate?secondary_size:primary_size;
    const auto offset=offsets[index];
    if(!data||size<std::size_t(offset)+4u)return false;
    value=u32(data+offset);
    return true;
}

} // namespace outrun::platform
