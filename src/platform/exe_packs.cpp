// Packs built from the player's EXE image, byte for byte the output of the
// Python extractors that used to be compiled in (see embedded_exe_data.hpp).
#include "platform/embedded_exe_data.hpp"
#include "system/exe_image.hpp"
#include <cstring>
#include <stdexcept>
#include <zlib.h>
namespace outrun::platform {
namespace {
using Bytes=std::vector<std::uint8_t>;
Bytes& event_pack(){static Bytes b;return b;}
Bytes& stage17_pack(){static Bytes b;return b;}
Bytes& driving_pack(){static Bytes b;return b;}
void put(Bytes& out,const void* p,std::size_t n){const auto* b=static_cast<const std::uint8_t*>(p);out.insert(out.end(),b,b+n);}
void put32(Bytes& out,std::uint32_t v){put(out,&v,4);}
void put_va(Bytes& out,std::uint32_t va,std::uint32_t n){put(out,exe_image_bytes(va,n),n);}
void set32(Bytes& out,std::size_t at,std::uint32_t v){std::memcpy(out.data()+at,&v,4);}
std::uint32_t crc(const std::uint8_t* p,std::size_t n){return static_cast<std::uint32_t>(::crc32(0L,p,static_cast<uInt>(n)));}
bool fail(std::string* error,const char* text){if(error)*error=text;return false;}

// Event metadata pack (OR2EVT2).
void build_event_metadata(Bytes& out){
    constexpr std::uint32_t Events=410,Functions=128,Modes=37;
    out.clear();put(out,"OR2EVT2\0",8);put32(out,2u);put32(out,Events);put32(out,Functions);put32(out,Modes);
    put_va(out,0x599808u,Events*0x18u);
    put_va(out,0x59BE78u,Functions*0x14u);
    put_va(out,0x5995B4u,Modes*0x10u);
}

// Stage 17 tables pack (OR2S17P1).
void build_stage17(Bytes& out){
    constexpr std::uint32_t Primary=66,Table68=0x60,Table6C=0x15E,Record=4+Table68+4+Table6C,Header=32;
    Bytes payload;
    for(std::uint32_t i=0;i<Primary;++i){
        const auto descriptor=exe_image_u32(0x6A54E0u+i*4u);
        const auto t68=exe_image_u32(descriptor+0x68u),t6c=exe_image_u32(descriptor+0x6Cu);
        if(!t68||!t6c)throw std::runtime_error("stage-17 descriptor without table");
        put32(payload,t68);put_va(payload,t68,Table68);
        put32(payload,t6c);put_va(payload,t6c,Table6C);
    }
    out.clear();put(out,"OR2S17P1",8);
    put32(out,1u);put32(out,Header);put32(out,Primary);put32(out,Record);
    put32(out,crc(payload.data(),payload.size()));put32(out,static_cast<std::uint32_t>(Header+payload.size()));
    put(out,payload.data(),payload.size());
}

// Driving data pack (OR2DRV1).
void build_driving(Bytes& out){
    constexpr std::uint32_t Header=192,ParameterBase=0x5E3140u,Columns=19,View=0x2650u,Arena=View+(Columns-1)*4;
    constexpr std::uint32_t MapBase=0x5E3050u,Maps=4,Entries=15,MapBytes=Maps*Entries*4;
    constexpr std::uint32_t Torque0=0x5E94D8u,Torque1=0x5E9760u,TorqueBytes=648,Brake=0x5E9DF0u,BrakeBytes=1024;
    // Pack identity fields checked by driving_data_pack.cpp (historical:
    // size and SHA-256 of the EXE the format was defined from).
    constexpr std::uint32_t LegacyCar=5,LegacyColumn=6,LegacySourceBytes=14950400u;
    static const std::uint8_t LegacySourceSha[32]={0xbd,0xaf,0xa8,0x8a,0x5a,0xbd,0xd2,0xa9,0x74,0x3f,0x6b,0xdc,0xc5,0xe2,0x18,0x92,
                                                   0x88,0xc0,0x20,0x37,0x90,0x41,0x2f,0xce,0x10,0xb9,0xbb,0x64,0x99,0x33,0x48,0x2f};
    for(std::uint32_t k=0;k<Maps*Entries;++k)
        if(exe_image_u32(MapBase+k*4u)>=Columns)throw std::runtime_error("player-car selector references an unknown parameter column");
    if(exe_image_u32(MapBase+LegacyCar*4u)!=LegacyColumn)throw std::runtime_error("legacy parameter selector is not column 6");
    const std::uint32_t parameters=Header,maps=parameters+Arena,t0=maps+MapBytes,t1=t0+TorqueBytes,brake=t1+TorqueBytes,size=brake+BrakeBytes;
    out.assign(Header,0);std::memcpy(out.data(),"OR2DRV1\0",8);
    const std::uint32_t fields[24]={1u,Header,size,parameters,Arena,ParameterBase,Columns,View,maps,Maps,Entries,MapBase,
                                    t0,t1,TorqueBytes,Torque0,Torque1,brake,BrakeBytes,Brake,LegacyCar,LegacyColumn,LegacySourceBytes,0u};
    std::memcpy(out.data()+8,fields,sizeof fields);
    std::memcpy(out.data()+104,LegacySourceSha,32);
    set32(out,136,crc(exe_image_bytes(ParameterBase,Arena),Arena));
    set32(out,140,crc(exe_image_bytes(MapBase,MapBytes),MapBytes));
    set32(out,144,crc(exe_image_bytes(Torque0,TorqueBytes),TorqueBytes));
    set32(out,148,crc(exe_image_bytes(Torque1,TorqueBytes),TorqueBytes));
    set32(out,152,crc(exe_image_bytes(Brake,BrakeBytes),BrakeBytes));
    put_va(out,ParameterBase,Arena);put_va(out,MapBase,MapBytes);
    put_va(out,Torque0,TorqueBytes);put_va(out,Torque1,TorqueBytes);put_va(out,Brake,BrakeBytes);
}
}

const std::uint8_t* EmbeddedEventMetadata=nullptr;
std::size_t EmbeddedEventMetadataSize=0;
const std::uint8_t* EmbeddedStage17Assets=nullptr;
std::size_t EmbeddedStage17AssetsSize=0;
const std::uint8_t* EmbeddedDrivingData=nullptr;
std::size_t EmbeddedDrivingDataSize=0;

void build_exe_packs(){
    build_event_metadata(event_pack());build_stage17(stage17_pack());build_driving(driving_pack());
    EmbeddedEventMetadata=event_pack().data();EmbeddedEventMetadataSize=event_pack().size();
    EmbeddedStage17Assets=stage17_pack().data();EmbeddedStage17AssetsSize=stage17_pack().size();
    EmbeddedDrivingData=driving_pack().data();EmbeddedDrivingDataSize=driving_pack().size();
}
OR2_EXE_BIND(build_exe_packs);

// Course tables (ORC78TBL in OR2CRS3).
bool build_course_asset_pack(const std::vector<std::uint8_t>& csc,std::vector<std::uint8_t>& pack,std::string* error){
    if(!exe_image_loaded())return fail(error,"OR2006C2C.EXE image not loaded");
    constexpr std::uint32_t Primary=66,Secondary=77,Descriptor=0x98,Header=48;
    Bytes d;put(d,"ORC78TBL",8);put32(d,3u);put32(d,Primary);put32(d,Secondary);put32(d,0u);
    for(std::uint32_t i=0;i<Primary;++i){const auto token=exe_image_u32(0x6A54E0u+i*4u);put32(d,token);put_va(d,token,Descriptor);}
    for(std::uint32_t i=0;i<Secondary;++i)put32(d,exe_image_u32(0x6A55E8u+i*4u));
    pack.clear();put(pack,"OR2CRS3\0",8);
    const std::uint32_t fields[10]={3u,Header,static_cast<std::uint32_t>(d.size()),static_cast<std::uint32_t>(csc.size()),
        crc(d.data(),d.size()),crc(csc.data(),csc.size()),Header,static_cast<std::uint32_t>(Header+d.size()),
        static_cast<std::uint32_t>(Header+d.size()+csc.size()),0u};
    put(pack,fields,sizeof fields);put(pack,d.data(),d.size());put(pack,csc.data(),csc.size());
    return true;
}

// Font metrics of the 10 font atlases.
bool build_font_metrics(const std::uint32_t (&atlas)[10][3],std::vector<std::uint8_t>& metrics,std::string* error){
    if(!exe_image_loaded())return fail(error,"OR2006C2C.EXE image not loaded");
    Bytes records,payload;
    for(std::uint32_t i=0;i<10;++i){
        const auto* d=exe_image_bytes(exe_image_u32(0x76F7B8u+i*4u),32);
        std::uint32_t token,map,kern;std::int16_t width,height;std::int32_t first,dx,dy,stride;
        std::memcpy(&token,d,4);std::memcpy(&map,d+4,4);std::memcpy(&kern,d+8,4);std::memcpy(&width,d+12,2);std::memcpy(&height,d+14,2);
        std::memcpy(&first,d+16,4);std::memcpy(&dx,d+20,4);std::memcpy(&dy,d+24,4);std::memcpy(&stride,d+28,4);
        const std::int32_t count=kern?stride:128-first;
        if(token>=10||(count!=96&&count!=128&&count!=192)||width<=0||height<=0||first<0)return fail(error,"unexpected font descriptor");
        const auto kern_bytes=kern?std::uint32_t(count)*std::uint32_t(count):0u;
        const std::uint32_t row[12]={token,std::uint32_t(width),std::uint32_t(height),std::uint32_t(first),std::uint32_t(count),
            std::uint32_t(dx),std::uint32_t(dy),static_cast<std::uint32_t>(payload.size()),kern_bytes,atlas[token][0],atlas[token][1],atlas[token][2]};
        put(records,row,sizeof row);
        put_va(payload,map,std::uint32_t(count)*2u);
        if(kern)put_va(payload,kern,kern_bytes);
    }
    metrics=std::move(records);put(metrics,payload.data(),payload.size());
    return true;
}
}
