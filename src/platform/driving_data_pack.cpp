#include "platform/driving_data_pack.hpp"
#include <algorithm>
#include <array>
#include <cstdio>
#include <cstring>
#include <utility>

namespace outrun::platform {
namespace {
constexpr std::array<std::uint8_t,8> Magic{{'O','R','2','D','R','V','1',0}};
constexpr std::uint32_t Version=1u;
constexpr std::uint32_t ParameterBaseVa=0x005e3140u;
constexpr std::uint32_t SelectionMapBaseVa=0x005e3050u;
constexpr std::uint32_t Torque0Va=0x005e94d8u;
constexpr std::uint32_t Torque1Va=0x005e9760u;
constexpr std::uint32_t BrakeVa=0x005e9df0u;
constexpr std::uint32_t ExpectedSourceBytes=14950400u;
constexpr std::array<std::uint8_t,32> ExpectedSourceSha{{
    0xbd,0xaf,0xa8,0x8a,0x5a,0xbd,0xd2,0xa9,
    0x74,0x3f,0x6b,0xdc,0xc5,0xe2,0x18,0x92,
    0x88,0xc0,0x20,0x37,0x90,0x41,0x2f,0xce,
    0x10,0xb9,0xbb,0x64,0x99,0x33,0x48,0x2f}};

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
bool range_is(std::uint32_t offset,std::uint32_t bytes,std::uint32_t expected,
              std::uint32_t expected_bytes,std::size_t file_size){
    return offset==expected&&bytes==expected_bytes&&
           std::uint64_t(offset)+bytes<=file_size;
}
}

bool parse_driving_data_pack(const std::uint8_t* data,std::size_t size,
                             DrivingDataPack& pack,std::string* error){
    if(error)error->clear();
    constexpr std::uint32_t ParameterOffset=DrivingDataHeaderSize;
    constexpr std::uint32_t MapOffset=ParameterOffset+DrivingParameterArenaBytes;
    constexpr std::uint32_t MapBytes=DrivingSelectionMapCount*DrivingSelectionMapEntries*4u;
    constexpr std::uint32_t Torque0Offset=MapOffset+MapBytes;
    constexpr std::uint32_t Torque1Offset=Torque0Offset+DrivingTorqueTableBytes;
    constexpr std::uint32_t BrakeOffset=Torque1Offset+DrivingTorqueTableBytes;
    constexpr std::uint32_t FileBytes=BrakeOffset+DrivingBrakeTableBytes;
    if(!data||size<DrivingDataHeaderSize||
       std::memcmp(data,Magic.data(),Magic.size())!=0){
        fail(error,"driving data magic/header mismatch");return false;
    }
    if(u32(data+8u)!=Version||u32(data+12u)!=DrivingDataHeaderSize||
       u32(data+16u)!=size||size!=FileBytes){
        fail(error,"driving data file layout mismatch");return false;
    }
    if(!range_is(u32(data+20u),u32(data+24u),ParameterOffset,
                 DrivingParameterArenaBytes,size)||
       u32(data+28u)!=ParameterBaseVa||
       u32(data+32u)!=DrivingParameterColumns||
       u32(data+36u)!=DrivingParameterViewBytes||
       !range_is(u32(data+40u),MapBytes,MapOffset,MapBytes,size)||
       u32(data+44u)!=DrivingSelectionMapCount||
       u32(data+48u)!=DrivingSelectionMapEntries||
       u32(data+52u)!=SelectionMapBaseVa||
       u32(data+56u)!=Torque0Offset||u32(data+60u)!=Torque1Offset||
       u32(data+64u)!=DrivingTorqueTableBytes||
       u32(data+68u)!=Torque0Va||u32(data+72u)!=Torque1Va||
       u32(data+76u)!=BrakeOffset||u32(data+80u)!=DrivingBrakeTableBytes||
       u32(data+84u)!=BrakeVa||u32(data+88u)!=DrivingPackV1LegacyCarId||
       u32(data+92u)!=DrivingPackV1LegacyColumn||
       u32(data+96u)!=ExpectedSourceBytes||u32(data+100u)!=0u){
        fail(error,"driving data pinned metadata mismatch");return false;
    }
    if(std::memcmp(data+104u,ExpectedSourceSha.data(),ExpectedSourceSha.size())!=0){
        fail(error,"driving data source hash mismatch");return false;
    }
    for(std::size_t i=156u;i<DrivingDataHeaderSize;++i)if(data[i]!=0u){
        fail(error,"driving data reserved header is nonzero");return false;
    }
    const auto crc_ok=[&](std::size_t header_offset,std::size_t offset,std::size_t bytes){
        return u32(data+header_offset)==crc32(data+offset,bytes);
    };
    if(!crc_ok(136u,ParameterOffset,DrivingParameterArenaBytes)||
       !crc_ok(140u,MapOffset,MapBytes)||
       !crc_ok(144u,Torque0Offset,DrivingTorqueTableBytes)||
       !crc_ok(148u,Torque1Offset,DrivingTorqueTableBytes)||
       !crc_ok(152u,BrakeOffset,DrivingBrakeTableBytes)){
        fail(error,"driving data payload CRC mismatch");return false;
    }

    DrivingDataPack next{};
    next.source_bytes=u32(data+96u);
    std::memcpy(next.source_sha256.data(),data+104u,next.source_sha256.size());
    next.parameter_arena.assign(data+ParameterOffset,
                                data+ParameterOffset+DrivingParameterArenaBytes);
    for(std::size_t map=0;map<DrivingSelectionMapCount;++map)
        for(std::size_t car=0;car<DrivingSelectionMapEntries;++car){
            const auto column=u32(data+MapOffset+(map*DrivingSelectionMapEntries+car)*4u);
            if(column>=DrivingParameterColumns){
                fail(error,"driving data selector column outside arena");return false;
            }
            next.selection_maps[map][car]=column;
        }
    if(next.selection_maps[0][DrivingPackV1LegacyCarId]!=DrivingPackV1LegacyColumn){
        fail(error,"driving data legacy selector mismatch");return false;
    }
    next.torque_tables[0].assign(data+Torque0Offset,
                                 data+Torque0Offset+DrivingTorqueTableBytes);
    next.torque_tables[1].assign(data+Torque1Offset,
                                 data+Torque1Offset+DrivingTorqueTableBytes);
    next.brake_table.assign(data+BrakeOffset,data+BrakeOffset+DrivingBrakeTableBytes);
    pack=std::move(next);
    return true;
}

bool load_driving_data_pack_file(const char* path,DrivingDataPack& pack,
                                 std::string* error){
    if(error)error->clear();
    if(!path){fail(error,"null driving data path");return false;}
    std::FILE* file=std::fopen(path,"rb");
    if(!file){fail(error,"cannot open driving data pack");return false;}
    if(std::fseek(file,0,SEEK_END)!=0){
        std::fclose(file);fail(error,"cannot size driving data pack");return false;
    }
    const auto length=std::ftell(file);std::rewind(file);
    if(length<0||length>64*1024){
        std::fclose(file);fail(error,"driving data pack size outside bounds");return false;
    }
    std::vector<std::uint8_t> bytes(static_cast<std::size_t>(length));
    const auto read=bytes.empty()?0u:std::fread(bytes.data(),1,bytes.size(),file);
    const int extra=std::fgetc(file);std::fclose(file);
    if(read!=bytes.size()||extra!=EOF){
        fail(error,"driving data pack read mismatch");return false;
    }
    return parse_driving_data_pack(bytes.data(),bytes.size(),pack,error);
}

bool copy_driving_parameter_view(const DrivingDataPack& pack,
                                 std::uint32_t selector_map,
                                 std::uint32_t car_id,
                                 std::uint8_t* output,std::size_t output_size,
                                 std::uint32_t* selected_column){
    if(selector_map>=DrivingSelectionMapCount||car_id>=DrivingSelectionMapEntries||
       !output||output_size<DrivingParameterViewBytes||
       pack.parameter_arena.size()!=DrivingParameterArenaBytes)return false;
    const auto column=pack.selection_maps[selector_map][car_id];
    if(column>=DrivingParameterColumns)return false;
    const auto offset=std::size_t(column)*4u;
    if(offset+DrivingParameterViewBytes>pack.parameter_arena.size())return false;
    std::copy_n(pack.parameter_arena.data()+offset,DrivingParameterViewBytes,output);
    if(selected_column)*selected_column=column;
    return true;
}

} // namespace outrun::platform
