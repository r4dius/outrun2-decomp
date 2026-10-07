#pragma once
#include "platform/native_runtime.hpp"
#include <stdexcept>
#include <vector>
namespace frontend_fixture {
inline void put(std::vector<std::uint8_t>& b,std::size_t at,std::uint32_t v){
    for(unsigned i=0;i<4;++i)b.at(at+i)=std::uint8_t(v>>(8*i));
}
inline std::vector<std::uint8_t> script(unsigned lane){
    // Empty category container for ordinary scripts: 8-byte header + sentinel.
    if(lane>=4)return std::vector<std::uint8_t>(20);
    constexpr const char* names[]={"Request_Course_SP","Request_Course_OR2",
        "Req_Course_MIX_PS2","Req_Course_MIX_PSP"};
    std::vector<std::uint8_t> b(32+15*0x78);
    put(b,0,1);put(b,8,outrun::driving::runtime_category_hash_4f1260(names[lane]));
    put(b,12,15);put(b,16,32);
    for(unsigned i=0;i<15;++i)put(b,32+i*0x78+0x1c,lane*15+i);
    return b;
}
constexpr unsigned TotalBytes=4*(32+15*0x78)+60*20;
inline outrun::platform::RaceAssetPack races(){
    // Valid Races category, its 94 relocation pairs and one direct course.
    constexpr unsigned records=8+94*8+3*12,course=records+94*0x44;
    std::vector<std::uint8_t> b(course+0x78);
    put(b,0,2);put(b,4,94);
    for(unsigned i=0;i<94;++i){
        put(b,8+i*8,records+i*0x44+0x18);put(b,12+i*8,course);
        put(b,records+i*0x44,i+1);put(b,records+i*0x44+4,1);
        put(b,records+i*0x44+0x14,3);
    }
    constexpr unsigned index=8+94*8;
    put(b,index,outrun::driving::runtime_category_hash_4f1260("Races"));
    put(b,index+4,94);put(b,index+8,records);
    put(b,index+12,~0u);put(b,index+16,1);put(b,index+20,course);
    put(b,course+0x1c,15);put(b,course+0x20,15);
    put(b,course+0x2c,~0u);put(b,course+0x30,~0u);
    outrun::platform::RaceAssetPack pack;std::string error;
    if(!outrun::platform::parse_race_asset_pack(b.data(),b.size(),pack,&error))
        throw std::runtime_error("Races fixture: "+error);
    return pack;
}
}
