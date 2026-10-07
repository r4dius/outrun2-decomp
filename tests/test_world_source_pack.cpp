#include "platform/native_runtime.hpp"
#include "platform/world_source_pack.hpp"
#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

using namespace outrun::platform;
namespace {
unsigned checks=0;
void require(bool ok,const char* message){
    ++checks;if(!ok){std::fprintf(stderr,"FAILED: %s\n",message);std::exit(1);}
}
void put32(std::vector<std::uint8_t>& b,std::size_t at,std::uint32_t v){
    b[at]=std::uint8_t(v);b[at+1]=std::uint8_t(v>>8u);
    b[at+2]=std::uint8_t(v>>16u);b[at+3]=std::uint8_t(v>>24u);
}
std::uint32_t crc32(const std::uint8_t* data,std::size_t size){
    std::uint32_t value=0xffffffffu;
    for(std::size_t i=0;i<size;++i){value^=data[i];
        for(unsigned j=0;j<8;++j)value=(value>>1u)^((value&1u)?0xedb88320u:0u);}
    return ~value;
}
std::vector<std::uint8_t> fixture(){
    constexpr std::array<std::uint32_t,7> fields{{0x0c,0x20,0x24,0x28,0x2c,0x30,0x64}};
    std::vector<std::uint8_t> b(32u+7u*24u+64u+6u*4u);
    std::memcpy(b.data(),"OR2SRC1\0",8u);
    put32(b,8,1);put32(b,12,7);put32(b,16,32);
    put32(b,20,static_cast<std::uint32_t>(b.size()));
    put32(b,24,15);put32(b,28,0x005d1128u);
    std::size_t at=32u+7u*24u;
    for(std::size_t i=0;i<7;++i){
        const auto size=i==0u?64u:4u;
        put32(b,32u+i*24u,fields[i]);
        put32(b,32u+i*24u+4u,0x005d7000u+static_cast<std::uint32_t>(i)*0x20u);
        put32(b,32u+i*24u+8u,static_cast<std::uint32_t>(at));
        put32(b,32u+i*24u+12u,static_cast<std::uint32_t>(size));
        put32(b,32u+i*24u+16u,16u);
        if(i==0u){put32(b,at,60u);std::memcpy(b.data()+at+4u,"COLI0200",8u);
            put32(b,at+12u,1u);put32(b,at+20u,64u);}
        put32(b,32u+i*24u+20u,crc32(b.data()+at,size));
        at+=size;
    }
    return b;
}
}

int main(int argc,char** argv){
    auto bytes=fixture();WorldSourcePack pack{};std::string error;
    require(parse_world_source_pack(bytes.data(),bytes.size(),pack,&error),
            "bounded synthetic original-world pack accepted");
    require(pack.entries[0].size==64u&&world_source_find(pack,0x0cu,0x005d7000u)&&
            world_source_bytes(pack,pack.entries[0]),"guest token resolves collision bytes");
    bytes.back()^=1u;
    require(!parse_world_source_pack(bytes.data(),bytes.size(),pack,&error),
            "payload corruption rejected");
    bytes=fixture();put32(bytes,32u+24u,0x0cu);
    require(!parse_world_source_pack(bytes.data(),bytes.size(),pack,&error),
            "duplicate descriptor field rejected");
    if(argc>=2){
        require(load_world_source_pack_file(argv[1],pack,&error),
                "owned BEAC world source pack accepted");
        require(pack.descriptor_index==15u&&pack.entries[0].size==835204u&&
                pack.entries.size()==7u,"owned BEAC collision and seven resources intact");
        // START world loading is covered by the faithful 49BA80 port
        // (scene-owner oracle) and the host_nro menu-to-GAME run.
        }
    std::printf("world_source_pack: %u checks passed\n",checks);
    return 0;
}
