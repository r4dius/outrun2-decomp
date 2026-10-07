#include "platform/race_asset_pack.hpp"
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <limits>
#include <utility>

namespace outrun::platform {
namespace {
constexpr std::size_t MaxRaceBytes=2u*1024u*1024u;
constexpr std::uint32_t RetailRaceCount=94u;
constexpr std::size_t RaceRecordBytes=0x44u;
constexpr std::size_t CourseRecordBytes=driving::PcCourseRecord44d720Size;
void fail(std::string* error,const char* message){if(error)*error=message;}
std::uint32_t u32(const std::uint8_t* p){
    return std::uint32_t(p[0])|(std::uint32_t(p[1])<<8u)|
        (std::uint32_t(p[2])<<16u)|(std::uint32_t(p[3])<<24u);
}
bool relocated_target(const driving::PcRelocCategoryBlobR077& blob,
                      std::size_t patch,std::size_t& target){
    const auto* bytes=static_cast<const std::uint8_t*>(blob.data);
    for(std::size_t i=0;i<blob.relocation_count;++i){
        const auto at=8u+i*8u;
        if(u32(bytes+at)==patch){target=u32(bytes+at+4u);return true;}
    }
    return false;
}
bool category_at_offset(const driving::PcRelocCategoryBlobR077& blob,
                        std::size_t offset,std::uint32_t& count){
    const auto* bytes=static_cast<const std::uint8_t*>(blob.data);
    for(std::size_t i=0;i<blob.category_count;++i){
        const auto at=blob.index_offset+i*12u;
        if(u32(bytes+at+8u)==offset){count=u32(bytes+at+4u);return true;}
    }
    return false;
}
bool linked_course(const driving::PcRelocCategoryBlobR077& blob,
                   std::size_t record_offset,std::size_t& target,
                   std::uint32_t& count){
    if(!relocated_target(blob,record_offset+0x18u,target)||
       !category_at_offset(blob,target,count)||count==0u||count>15u||
       target>blob.size||std::size_t(count)>((blob.size-target)/CourseRecordBytes))return false;
    const auto* bytes=static_cast<const std::uint8_t*>(blob.data);
    for(std::uint32_t i=0;i<count;++i){
        const auto* record=bytes+target+std::size_t(i)*CourseRecordBytes;
        if(u32(record+0x1cu)>=driving::PcCoursePrimaryDescriptorCountR078||
           u32(record+0x20u)>=driving::PcCourseSecondaryDescriptorCountR078)return false;
    }
    return true;
}
}

bool race_asset_pointer(const RaceAssetPack& pack,std::size_t field_offset,std::size_t& target){
    driving::PcRelocCategoryBlobR077 blob{};
    if(pack.bytes.empty()||!driving::course_reloc_blob_open_r077(const_cast<std::uint8_t*>(pack.bytes.data()),pack.bytes.size(),blob))return false;
    return relocated_target(blob,field_offset,target)&&target<pack.bytes.size();
}
bool parse_race_asset_pack(const std::uint8_t* data,std::size_t size,
                           RaceAssetPack& pack,std::string* error){
    if(error)error->clear();
    if(!data||size<8u||size>MaxRaceBytes){fail(error,"Races.bin size outside bounds");return false;}
    RaceAssetPack next{};
    next.bytes.assign(data,data+size);
    driving::PcRelocCategoryBlobR077 blob{};
    if(!driving::course_reloc_blob_open_r077(next.bytes.data(),size,blob)||
       driving::runtime_category_count_4f1ba0("Races",blob)!=
           static_cast<std::int32_t>(RetailRaceCount)){
        fail(error,"Races.bin container/category invalid");return false;
    }
    const auto* records=static_cast<const std::uint8_t*>(
        driving::runtime_category_records_4f1a90("Races",blob));
    const auto* base=next.bytes.data();
    if(!records||records<base||std::size_t(records-base)>size||
       RetailRaceCount*RaceRecordBytes>size-std::size_t(records-base)){
        fail(error,"Races record table outside file");return false;
    }
    next.races_offset=std::size_t(records-base);
    next.race_count=RetailRaceCount;
    for(std::uint32_t i=0;i<RetailRaceCount;++i){
        std::size_t target=0u;std::uint32_t count=0u;
        if(u32(base+next.races_offset+std::size_t(i)*RaceRecordBytes+0x14u)!=3u){
            fail(error,"Races unsupported retail kind");return false;
        }
        if(!linked_course(blob,next.races_offset+std::size_t(i)*RaceRecordBytes,
                          target,count)){
            fail(error,"Races linked course records invalid");return false;
        }
    }
    pack=std::move(next);return true;
}

bool load_race_asset_pack_file(const char* path,RaceAssetPack& pack,
                               std::string* error){
    if(error)error->clear();
    if(!path){fail(error,"null Races.bin path");return false;}
    std::FILE* file=std::fopen(path,"rb");
    if(!file){fail(error,"cannot open Races.bin");return false;}
    if(std::fseek(file,0,SEEK_END)!=0){std::fclose(file);fail(error,"cannot size Races.bin");return false;}
    const auto length=std::ftell(file);
    if(length<0||static_cast<std::uint64_t>(length)>MaxRaceBytes){
        std::fclose(file);fail(error,"Races.bin file outside bounds");return false;
    }
    std::rewind(file);
    std::vector<std::uint8_t> bytes(static_cast<std::size_t>(length));
    const auto read=bytes.empty()?0u:std::fread(bytes.data(),1,bytes.size(),file);
    const int extra=std::fgetc(file);std::fclose(file);
    if(read!=bytes.size()||extra!=EOF){fail(error,"Races.bin read mismatch");return false;}
    return parse_race_asset_pack(bytes.data(),bytes.size(),pack,error);
}

bool parse_race_assignment_pack(const std::uint8_t* data,std::size_t size,
                                RaceAssignmentPack& pack,std::string* error){
    if(error)error->clear();
    if(!data||size<8u||size>64u*1024u){
        fail(error,"RaceAssignment.bin size outside bounds");return false;
    }
    std::vector<std::uint8_t> copy(data,data+size);
    driving::PcRelocCategoryBlobR077 blob{};
    if(!driving::course_reloc_blob_open_r077(copy.data(),copy.size(),blob)||
       driving::runtime_category_count_4f1ba0("RACE_MAPPING_ARRAY",blob)!=40){
        fail(error,"RaceAssignment category invalid");return false;
    }
    const auto* records=static_cast<const std::uint8_t*>(
        driving::runtime_category_records_4f1a90("RACE_MAPPING_ARRAY",blob));
    if(!records||records<copy.data()||
       std::size_t(records-copy.data())>copy.size()||
       40u*8u>copy.size()-std::size_t(records-copy.data())){
        fail(error,"RaceAssignment mapping outside file");return false;
    }
    RaceAssignmentPack next{};next.menu_count=40u;
    for(std::size_t i=0;i<next.menu_count;++i){
        const auto key=u32(records+i*8u),kind=u32(records+i*8u+4u);
        if(key==0u||kind!=1u){
            fail(error,"RaceAssignment mapping entry invalid");return false;
        }
        next.menu_race_keys[i]=key;
    }
    pack=next;return true;
}

bool load_race_assignment_pack_file(const char* path,RaceAssignmentPack& pack,
                                    std::string* error){
    if(error)error->clear();
    if(!path){fail(error,"null RaceAssignment.bin path");return false;}
    std::FILE* file=std::fopen(path,"rb");
    if(!file){fail(error,"cannot open RaceAssignment.bin");return false;}
    if(std::fseek(file,0,SEEK_END)!=0){std::fclose(file);fail(error,"cannot size RaceAssignment.bin");return false;}
    const auto length=std::ftell(file);
    if(length<0||length>64*1024){
        std::fclose(file);fail(error,"RaceAssignment.bin file outside bounds");return false;
    }
    std::rewind(file);
    std::vector<std::uint8_t> bytes(static_cast<std::size_t>(length));
    const auto read=bytes.empty()?0u:std::fread(bytes.data(),1,bytes.size(),file);
    const int extra=std::fgetc(file);std::fclose(file);
    if(read!=bytes.size()||extra!=EOF){fail(error,"RaceAssignment.bin read mismatch");return false;}
    return parse_race_assignment_pack(bytes.data(),bytes.size(),pack,error);
}

bool race_course_select_4965a0(const RaceAssetPack& pack,
                                std::uint32_t race_key,std::uint32_t sub_key,
                                RaceCourseSelection& selection){
    if(pack.race_count!=RetailRaceCount||pack.bytes.empty()||
       pack.races_offset>pack.bytes.size()||
       RetailRaceCount*RaceRecordBytes>pack.bytes.size()-pack.races_offset)return false;
    driving::PcRelocCategoryBlobR077 blob{};
    if(!driving::course_reloc_blob_open_r077(
           const_cast<std::uint8_t*>(pack.bytes.data()),pack.bytes.size(),blob))return false;
    const auto* base=pack.bytes.data();
    for(std::uint32_t i=0;i<RetailRaceCount;++i){
        const auto at=pack.races_offset+std::size_t(i)*RaceRecordBytes;
        const auto* record=base+at;
        if(u32(record)!=race_key||u32(record+4u)!=sub_key)continue;
        std::size_t target=0u;std::uint32_t count=0u;
        if(!linked_course(blob,at,target,count))return false;
        RaceCourseSelection next{};
        next.race_index=i;next.race_kind=u32(record+0x14u);
        next.course_count=count;
        next.course_records.assign(base+target,
            base+target+std::size_t(count)*CourseRecordBytes);
        selection=std::move(next);return true;
    }
    return false;
}

bool race_record_type(const RaceAssetPack& races,std::uint32_t key,std::uint32_t sub_key,std::uint32_t& type){
    if(races.races_offset>races.bytes.size()||races.race_count>(races.bytes.size()-races.races_offset)/RaceRecordBytes)return false;
    for(std::uint32_t i=0;i<races.race_count;++i){const auto* r=races.bytes.data()+races.races_offset+std::size_t(i)*RaceRecordBytes;
        if(u32(r)==key&&u32(r+4)==sub_key){type=u32(r+0x20);return true;}}
    return false;
}

std::uint32_t race_count_with_key_495930(const RaceAssetPack& races,std::uint32_t key){
    if(races.race_count!=RetailRaceCount||races.races_offset>races.bytes.size()||
       RetailRaceCount*RaceRecordBytes>races.bytes.size()-races.races_offset)return 0u;
    std::uint32_t count=0u;
    for(std::uint32_t i=0;i<RetailRaceCount;++i)
        count+=u32(races.bytes.data()+races.races_offset+std::size_t(i)*RaceRecordBytes)==key?1u:0u;
    return count;
}

bool race_menu_choice_4eeb50(const RaceAssetPack& races,
                             const RaceAssignmentPack& assignment,
                             std::uint32_t menu_index,std::uint32_t sub_index,
                             std::uint32_t& race_key,std::uint32_t& sub_key){
    if(menu_index>=assignment.menu_count||races.race_count!=RetailRaceCount||
       races.races_offset>races.bytes.size()||
       RetailRaceCount*RaceRecordBytes>races.bytes.size()-races.races_offset)
        return false;
    const auto key=assignment.menu_race_keys[menu_index];
    std::uint32_t count=0u;
    for(std::uint32_t i=0;i<RetailRaceCount;++i)
        count+=u32(races.bytes.data()+races.races_offset+
                   std::size_t(i)*RaceRecordBytes)==key?1u:0u;
    if(count==0u)return false;
    const auto sub=std::min(sub_index,count-1u)+1u;
    RaceCourseSelection verify{};
    if(!race_course_select_4965a0(races,key,sub,verify))return false;
    race_key=key;sub_key=sub;return true;
}

bool race_menu_type_4958c0(const RaceAssetPack& races,const RaceAssignmentPack& assignment,
    std::uint32_t category,std::uint32_t sub_key,std::uint32_t& type){
    if(category>=assignment.menu_count||category>=assignment.menu_race_keys.size()||
       races.races_offset>races.bytes.size()||races.race_count>(races.bytes.size()-races.races_offset)/RaceRecordBytes)return false;
    const auto key=assignment.menu_race_keys[category];
    for(unsigned i=0;i<races.race_count;++i){const auto* r=races.bytes.data()+races.races_offset+i*RaceRecordBytes;
        if(u32(r)==key&&u32(r+4)==sub_key){type=u32(r+0x20);return true;}}
    return false;
}

bool parse_frontend_course_table(std::uint32_t lane,const std::uint8_t* data,
    std::size_t size,FrontendCourseTable& out,std::string* error){
    if(error)error->clear();
    constexpr std::array<const char*,4> names{{"Request_Course_SP",
        "Request_Course_OR2","Req_Course_MIX_PS2","Req_Course_MIX_PSP"}};
    if(lane>=names.size()||!data||size>MaxRaceBytes){
        fail(error,"frontend course source outside bounds");return false;
    }
    driving::PcRelocCategoryBlobR077 blob{};
    if(!driving::course_reloc_blob_open_r077(const_cast<std::uint8_t*>(data),size,blob)){
        fail(error,"frontend course container invalid");return false;
    }
    const auto count=driving::runtime_category_count_4f1ba0(names[lane],blob);
    const auto* records=static_cast<const std::uint8_t*>(
        driving::runtime_category_records_4f1a90(names[lane],blob));
    if(count<15||!records||std::size_t(records-data)>size||
       std::size_t(count)>(size-std::size_t(records-data))/CourseRecordBytes){
        fail(error,"frontend course category missing or truncated");return false;
    }
    for(std::int32_t i=0;i<count;++i)
        if(u32(records+std::size_t(i)*CourseRecordBytes+0x1cu)>=
           driving::PcCoursePrimaryDescriptorCountR078){
            fail(error,"frontend championship type outside license grades");return false;
        }
    FrontendCourseTable next{};next.count=static_cast<std::uint32_t>(count);
    next.records.assign(records,records+std::size_t(count)*CourseRecordBytes);
    out=std::move(next);return true;
}

bool frontend_license_progress_tables(const RaceAssetPack* races,
    const RaceAssignmentPack* assignment,const std::array<FrontendCourseTable,4>& courses,
    std::uint32_t mode,LicenseProgressTables& out){
    LicenseProgressTables next{};next.mode=mode;
    if(mode!=32u){out=next;return true;}
    if(!races||!assignment||assignment->menu_count!=next.category_counts.size()||
       races->race_count!=RetailRaceCount||races->races_offset>races->bytes.size()||
       RetailRaceCount*RaceRecordBytes>races->bytes.size()-races->races_offset)return false;
    for(std::size_t lane=0;lane<next.category_counts.size();++lane){
        std::uint32_t count=0;
        for(std::uint32_t i=0;i<races->race_count;++i)
            count+=u32(races->bytes.data()+races->races_offset+i*RaceRecordBytes)==
                assignment->menu_race_keys[lane]?1u:0u;
        // Five grade bytes per license category. Never silently truncate.
        if(count>5u)return false;
        next.category_counts[lane]=static_cast<std::uint8_t>(count);
    }
    next.categories_ready=true;
    for(std::size_t lane=0;lane<next.championship_types.size();++lane){
        const auto& table=courses[lane];
        if(table.count<15u||table.count>table.records.size()/CourseRecordBytes)return false;
        for(std::size_t i=0;i<15u;++i){
            const auto type=u32(table.records.data()+i*CourseRecordBytes+0x1cu);
            if(type>=driving::PcCoursePrimaryDescriptorCountR078)return false;
            next.championship_types[lane][i]=type;
        }
        next.championship_present[lane]=true;
    }
    out=next;return true;
}

} // namespace outrun::platform
