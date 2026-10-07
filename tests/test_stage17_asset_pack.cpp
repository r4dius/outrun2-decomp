#include "platform/course_asset_pack.hpp"
#include "platform/native_runtime.hpp"
#include "platform/stage17_asset_pack.hpp"
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iterator>
#include <stdexcept>
#include <vector>
using namespace outrun::platform;
namespace {
unsigned checks{};
void require(bool ok,const char* message){++checks;if(!ok)throw std::runtime_error(message);}
std::uint32_t u32(const std::uint8_t* p){return std::uint32_t(p[0])|(std::uint32_t(p[1])<<8u)|(std::uint32_t(p[2])<<16u)|(std::uint32_t(p[3])<<24u);}
}
int main(int argc,char** argv){
    try{
        require(argc==3,"usage: test_stage17_asset_pack STAGE17 COURSE");
        Stage17AssetPack pack{};CourseAssetPack course{};std::string error;
        require(load_stage17_asset_pack_file(argv[1],pack,&error),"stage17 asset pack loads");
        require(load_course_asset_pack_file(argv[2],course,&error),"course pack loads");
        require(pack.records.size()==Stage17AssetRecordCount&&pack.payload_crc32!=0u,"stage17 pack cardinality/CRC retained");
        for(std::size_t i=0;i<pack.records.size();++i){
            const auto& descriptor=course.descriptors.storage[i];
            require(pack.records[i].token68==u32(descriptor.data()+0x68u)&&
                    pack.records[i].token6c==u32(descriptor.data()+0x6cu),
                    "stage17 tokens match complete primary descriptor table");
        }
        require(pack.records[15].token68==0x0071da9cu&&pack.records[15].token6c==0x0071b154u,
                "BEAC exact +0x68/+0x6c tokens");
        NativeRuntimeContext context{};
        require(!native_runtime_attach_stage17_assets(context,pack),"stage17 pack cannot attach before course identities");
        require(native_runtime_attach_course_assets(context,course)&&
                native_runtime_attach_stage17_assets(context,pack),"verified stage17 pack attaches after course pack");
        std::ifstream file(argv[1],std::ios::binary);std::vector<std::uint8_t> raw(
            (std::istreambuf_iterator<char>(file)),std::istreambuf_iterator<char>());
        require(raw.size()==Stage17AssetPackFileSize,"serialized stage17 pack exact size");
        raw.back()^=0x01u;Stage17AssetPack bad{};
        require(!parse_stage17_asset_pack(raw.data(),raw.size(),bad,&error)&&!error.empty(),
                "payload mutation rejected by CRC");
        std::printf("stage17_asset_pack: %u checks passed; %zu records; %zu bytes; BEAC tokens %08x/%08x\n",
            checks,pack.records.size(),Stage17AssetPackFileSize,pack.records[15].token68,pack.records[15].token6c);
        return 0;
    }catch(const std::exception& e){std::fprintf(stderr,"FAILED: %s\n",e.what());return 1;}
}
