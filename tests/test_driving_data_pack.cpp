#include "platform/driving_data_pack.hpp"
#include <cmath>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <vector>

using namespace outrun::platform;
namespace {
unsigned checks{};
void require(bool value,const char* message){++checks;if(!value)throw std::runtime_error(message);}
void put32(std::vector<std::uint8_t>& b,std::size_t o,std::uint32_t v){
    for(unsigned i=0;i<4u;++i)b[o+i]=std::uint8_t(v>>(i*8u));
}
void putf(std::vector<std::uint8_t>& b,std::size_t o,float f){
    std::uint32_t v{};std::memcpy(&v,&f,4);put32(b,o,v);
}
std::uint32_t crc32(const std::uint8_t* data,std::size_t size){
    std::uint32_t crc=0xffffffffu;
    for(std::size_t i=0;i<size;++i){crc^=data[i];for(unsigned bit=0;bit<8u;++bit)
        crc=(crc>>1u)^(0xedb88320u&std::uint32_t(0u-(crc&1u)));}return ~crc;
}
std::vector<std::uint8_t> fixture(){
    constexpr std::size_t map_bytes=4u*15u*4u;
    constexpr std::size_t param_off=DrivingDataHeaderSize;
    constexpr std::size_t map_off=param_off+DrivingParameterArenaBytes;
    constexpr std::size_t torque0_off=map_off+map_bytes;
    constexpr std::size_t torque1_off=torque0_off+DrivingTorqueTableBytes;
    constexpr std::size_t brake_off=torque1_off+DrivingTorqueTableBytes;
    std::vector<std::uint8_t> b(brake_off+DrivingBrakeTableBytes);
    std::memcpy(b.data(),"OR2DRV1\0",8);
    const std::uint32_t fields[24]={1u,DrivingDataHeaderSize,std::uint32_t(b.size()),
        param_off,DrivingParameterArenaBytes,0x005e3140u,
        DrivingParameterColumns,DrivingParameterViewBytes,map_off,4u,15u,0x005e3050u,
        torque0_off,torque1_off,DrivingTorqueTableBytes,0x005e94d8u,0x005e9760u,
        brake_off,DrivingBrakeTableBytes,0x005e9df0u,5u,6u,14950400u,0u};
    for(std::size_t i=0;i<24u;++i)put32(b,8u+i*4u,fields[i]);
    const std::uint8_t source_sha[32]={
        0xbd,0xaf,0xa8,0x8a,0x5a,0xbd,0xd2,0xa9,0x74,0x3f,0x6b,0xdc,0xc5,0xe2,0x18,0x92,
        0x88,0xc0,0x20,0x37,0x90,0x41,0x2f,0xce,0x10,0xb9,0xbb,0x64,0x99,0x33,0x48,0x2f};
    std::memcpy(b.data()+104u,source_sha,32u);
    for(std::size_t map=0;map<4u;++map)for(std::size_t car=0;car<15u;++car)
        put32(b,map_off+(map*15u+car)*4u,std::uint32_t((map+car)%19u));
    put32(b,map_off+5u*4u,6u);
    putf(b,param_off+6u*4u+0x15f8u,157.0796356201172f);
    putf(b,param_off+6u*4u+0x1690u,1308.9969482421875f);
    putf(b,torque0_off,0.1273239552974701f);putf(b,torque0_off+4u,4.3f);
    putf(b,torque1_off,0.1273239552974701f);putf(b,torque1_off+4u,4.5f);
    putf(b,brake_off+255u*4u,1.0f);
    put32(b,136u,crc32(b.data()+param_off,DrivingParameterArenaBytes));
    put32(b,140u,crc32(b.data()+map_off,map_bytes));
    put32(b,144u,crc32(b.data()+torque0_off,DrivingTorqueTableBytes));
    put32(b,148u,crc32(b.data()+torque1_off,DrivingTorqueTableBytes));
    put32(b,152u,crc32(b.data()+brake_off,DrivingBrakeTableBytes));
    return b;
}
float readf(const std::uint8_t* p){float f{};std::memcpy(&f,p,4);return f;}
}

int main(int argc,char** argv){
try{
    auto bytes=fixture();DrivingDataPack pack{};std::string error;
    require(parse_driving_data_pack(bytes.data(),bytes.size(),pack,&error),"valid OR2DRV1 parses");
    require(pack.parameter_arena.size()==DrivingParameterArenaBytes,"parameter arena size");
    require(pack.torque_tables[0].size()==648u&&pack.torque_tables[1].size()==648u,
            "two torque table sizes");
    require(pack.brake_table.size()==1024u,"brake table size");
    require(pack.selection_maps[0][5]==6u,"legacy model5 maps to column 6");
    std::vector<std::uint8_t> view(DrivingParameterViewBytes);std::uint32_t column=99u;
    require(copy_driving_parameter_view(pack,0u,5u,view.data(),view.size(),&column),
            "legacy parameter view copies");
    require(column==6u,"selected parameter column reported");
    require(std::fabs(readf(view.data()+0x15f8u)-157.0796356201172f)<0.0001f,
            "selected view preserves interleaved idle RPM");
    require(std::fabs(readf(pack.torque_tables[1].data()+4u)-4.5f)<0.0001f,
            "manual torque gain preserved");
    require(readf(pack.brake_table.data()+255u*4u)==1.0f,"full brake sample preserved");
    require(!copy_driving_parameter_view(pack,4u,5u,view.data(),view.size()),
            "invalid selector map rejected");
    require(!copy_driving_parameter_view(pack,0u,15u,view.data(),view.size()),
            "invalid car id rejected");
    require(!copy_driving_parameter_view(pack,0u,5u,view.data(),view.size()-1u),
            "short destination rejected");
    auto corrupted=bytes;corrupted[DrivingDataHeaderSize+17u]^=0x80u;
    require(!parse_driving_data_pack(corrupted.data(),corrupted.size(),pack,&error)&&
            error=="driving data payload CRC mismatch","payload corruption rejected");
    corrupted=bytes;corrupted[104u]^=1u;
    require(!parse_driving_data_pack(corrupted.data(),corrupted.size(),pack,&error)&&
            error=="driving data source hash mismatch","wrong source hash rejected");
    corrupted=bytes;put32(corrupted,88u,6u);
    require(!parse_driving_data_pack(corrupted.data(),corrupted.size(),pack,&error),
            "wrong selected car metadata rejected");
    require(!parse_driving_data_pack(bytes.data(),bytes.size()-1u,pack,&error),
            "truncated pack rejected");
    if(argc==2){
        DrivingDataPack actual{};
        require(load_driving_data_pack_file(argv[1],actual,&error),
                "generated owned OR2DRV1 parses");
        std::vector<std::uint8_t> gto(DrivingParameterViewBytes);
        require(copy_driving_parameter_view(actual,0u,5u,gto.data(),gto.size(),&column)&&
                column==6u,"generated pack retains legacy column metadata");
        require(std::fabs(readf(gto.data()+0x15f8u)-157.0796356201172f)<0.0001f&&
                std::fabs(readf(gto.data()+0x1644u)-890.117919921875f)<0.0001f&&
                std::fabs(readf(gto.data()+0x1690u)-1308.9969482421875f)<0.0001f,
                "generated pack carries exact legacy column engine limits");
        require(std::fabs(readf(actual.torque_tables[0].data())-0.1273239552974701f)<1e-7f&&
                readf(actual.brake_table.data()+255u*4u)==1.0f,
                "generated pack carries exact lookup endpoints");
    }else if(argc!=1)throw std::runtime_error("usage: test_driving_data_pack [OR2DRV1]");
    std::cout<<"driving data pack: "<<checks<<" checks passed\n";return 0;
}catch(const std::exception& ex){
    std::cerr<<"FAIL after "<<checks<<" checks: "<<ex.what()<<"\n";return 1;
}
}
