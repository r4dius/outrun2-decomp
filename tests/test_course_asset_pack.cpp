#include "platform/native_runtime.hpp"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>
using namespace outrun::platform;
namespace {
unsigned checks{};
void require(bool value,const char* message){++checks;if(!value){std::fprintf(stderr,"FAIL: %s\n",message);std::exit(1);}}
void put32(std::vector<std::uint8_t>& b,std::size_t o,std::uint32_t v){for(unsigned i=0;i<4u;++i)b[o+i]=std::uint8_t(v>>(i*8u));}
std::uint32_t crc32(const std::uint8_t* data,std::size_t size){std::uint32_t crc=0xffffffffu;for(std::size_t i=0;i<size;++i){crc^=data[i];for(unsigned bit=0;bit<8u;++bit)crc=(crc>>1u)^(0xedb88320u&std::uint32_t(0u-(crc&1u)));}return ~crc;}
std::vector<std::uint8_t> synthetic(unsigned version=CourseAssetPackVersion){
    std::vector<std::uint8_t> desc(version==1u?CourseDescriptorPackBytesLegacy:
                                   version==2u?CourseDescriptorPackBytes:
                                   CourseDescriptorPackBytesFull);
    std::memcpy(desc.data(),"ORC78TBL",8u);
    put32(desc,8u,version);put32(desc,12u,66u);put32(desc,16u,77u);
    std::vector<std::uint8_t> course(CourseCvtBlobBytes);
    put32(course,0u,1u);put32(course,4u,0u);
    put32(course,8u,outrun::driving::runtime_category_hash_4f1260("csc_data_cvt_course"));
    put32(course,12u,15u);put32(course,16u,20u);
    const auto course_off=CourseAssetPackHeaderSize+desc.size();
    std::vector<std::uint8_t> out(course_off+course.size());
    const char* magic=version==1u?"OR2CRS1\0":version==2u?"OR2CRS2\0":"OR2CRS3\0";
    std::memcpy(out.data(),magic,8u);
    put32(out,8u,version);put32(out,12u,CourseAssetPackHeaderSize);
    put32(out,16u,std::uint32_t(desc.size()));put32(out,20u,std::uint32_t(course.size()));
    put32(out,24u,crc32(desc.data(),desc.size()));put32(out,28u,crc32(course.data(),course.size()));
    put32(out,32u,CourseAssetPackHeaderSize);put32(out,36u,std::uint32_t(course_off));put32(out,40u,std::uint32_t(out.size()));
    std::memcpy(out.data()+CourseAssetPackHeaderSize,desc.data(),desc.size());std::memcpy(out.data()+course_off,course.data(),course.size());return out;
}
}
int main(int argc,char** argv){
    std::array<std::uint8_t,0x70> primary_fields{},secondary_fields{};
    for(std::size_t offset=0;offset<primary_fields.size();offset+=4u){
        for(unsigned byte=0;byte<4u;++byte){
            primary_fields[offset+byte]=std::uint8_t((0xa0000000u+offset)>>(byte*8u));
            secondary_fields[offset+byte]=std::uint8_t((0xb0000000u+offset)>>(byte*8u));
        }
    }
    constexpr std::array<std::uint32_t,18> pc_fields{{
        0xa000000cu,0xb000000cu,0xb0000018u,0xb000001cu,
        0xa0000020u,0xa0000024u,0xa0000028u,0xa000002cu,
        0xa0000030u,0xb0000030u,0xa0000034u,0xb0000034u,
        0xa0000064u,0xa0000068u,0xb0000068u,0xa000006cu,0u,0u
    }};
    for(std::uint32_t index=0;index<pc_fields.size();++index){
        std::uint32_t field=0xffffffffu;
        require(course_field_44c220(primary_fields.data(),primary_fields.size(),
                                    secondary_fields.data(),secondary_fields.size(),
                                    index,field)&&field==pc_fields[index],
                "original 44C220 VM field dispatch matches all 16 cases plus default");
    }
    std::uint32_t invalid_field=0x12345678u;
    require(!course_field_44c220(nullptr,0u,secondary_fields.data(),secondary_fields.size(),
                                 0u,invalid_field)&&invalid_field==0x12345678u,
            "44C220 refuses a missing selected primary descriptor");
    require(!course_field_44c220(primary_fields.data(),primary_fields.size(),nullptr,0u,
                                 1u,invalid_field)&&invalid_field==0x12345678u,
            "44C220 refuses a missing selected secondary descriptor");
    require(course_field_44c220(nullptr,0u,nullptr,0u,16u,invalid_field)&&invalid_field==0u,
            "44C220 out-of-range default does not dereference either descriptor");
    auto bytes=synthetic();CourseAssetPack pack{};std::string error;
    require(parse_course_asset_pack(bytes.data(),bytes.size(),pack,&error),"valid synthetic OR2CRS3");
    require(pack.descriptors.primary_count==66u&&pack.descriptors.secondary_count==77u&&pack.course_blob.size()==CourseCvtBlobBytes&&pack.descriptors.world_resource_fields_present,"course payload inventory");
    const auto old=synthetic(1u);CourseAssetPack legacy{};
    require(parse_course_asset_pack(old.data(),old.size(),legacy,&error)&&
            !legacy.descriptors.world_resource_fields_present,"OR2CRS1 remains readable without world IDs");
    const auto old2=synthetic(2u);
    require(parse_course_asset_pack(old2.data(),old2.size(),legacy,&error)&&
            legacy.descriptors.world_resource_fields_present,
            "OR2CRS2 remains readable with partial world descriptor fields");
    auto bad=bytes;bad[0]='X';require(!parse_course_asset_pack(bad.data(),bad.size(),pack,&error),"bad course magic rejected");
    bad=bytes;bad.back()^=1u;require(!parse_course_asset_pack(bad.data(),bad.size(),pack,&error),"corrupt course payload rejected");
    bad=bytes;const auto course_off=CourseAssetPackHeaderSize+CourseDescriptorPackBytesFull;
    put32(bad,course_off+20u+0x1cu,66u);
    put32(bad,28u,crc32(bad.data()+course_off,CourseCvtBlobBytes));
    require(!parse_course_asset_pack(bad.data(),bad.size(),pack,&error),"out-of-range course descriptor rejected");
    if(argc>=2){
        CourseAssetPack real{};
        require(load_course_asset_pack_file(argv[1],real,&error),"generated OR2CRS3 loads");
        require(real.descriptors.primary_count==66u&&real.descriptors.secondary_count==77u&&real.course_blob.size()==1820u,"generated course inventory");
        const auto& world=real.descriptors.storage[35u];
        require(real.descriptors.world_resource_fields_present&&
                outrun::driving::Bytes(const_cast<std::uint8_t*>(world.data()),world.size()).u32(0x00u)==0x23u&&
                outrun::driving::Bytes(const_cast<std::uint8_t*>(world.data()),world.size()).u32(0x04u)==0x162u&&
                outrun::driving::Bytes(const_cast<std::uint8_t*>(world.data()),world.size()).u32(0x38u)==0x19eu&&
                outrun::driving::Bytes(const_cast<std::uint8_t*>(world.data()),world.size()).u32(0x60u)==0x180u,
                "pinned PC descriptor 35 retains marker and world IDs");
        const auto& beach=real.descriptors.storage[15u];
        std::uint32_t source_field=0u;
        require(course_field_44c220(beach.data(),beach.size(),nullptr,0u,0u,
                                     source_field)&&source_field==0x005d7558u&&
                outrun::driving::Bytes(const_cast<std::uint8_t*>(beach.data()),
                                       beach.size()).u32(0x20u)==0x005d7538u,
                "full EXE descriptor retains BEAC collision and course-bin source tokens");
        NativeRuntimeContext runtime{};
        runtime.mode_descriptors[16u]={0u,0x00499d90u,0x0049c840u,0x00499e50u};
        runtime.mode_state.current=32u;
        require(native_runtime_attach_course_assets(runtime,real),"attach generated course pack");
        require(native_runtime_request_mode(runtime,16u)&&native_runtime_mode_control(runtime,{}),"GAME init invokes native course chain");
        const auto& game=runtime.game_mode;
        require(game.course_load_calls==1u&&game.course_load_success==1u&&game.course_read_calls==1u&&game.course_bytes==CourseCvtBlobBytes,"real csc_data_cvt loaded exactly once");
        require(game.course_provider_loaded&&game.course_loader_phase==3u&&game.course_load.force_sync_7d2d8c==1u,"course provider reaches synchronous phase 3");
        require(game.course_runtime.active_count_7d33c4==CourseCvtRecordCount&&game.course_runtime.max_depth_7d33c0==4&&game.course_runtime.selected_index==0,"real 15-record course tree rebuilt");
        require(game.course_runtime.selected_copy_active&&game.course_matrix_ready,"selected course and matrix retained");
        NativeRuntimeContext start{};
        start.mode_descriptors[13u]={0u,0x0049db20u,0x0049dd40u,0x00499d80u};
        LoaderAssetPack start_resources{};
        start_resources.records.push_back({LoaderAssetStartLoadingId,2u,{0x78u,0xdau,1u}});
        start.event_function36.loader_assets=&start_resources;
        require(native_runtime_attach_course_assets(start,real),"START attaches owned course data");
        require(native_runtime_request_mode(start,13u)&&native_runtime_mode_control(start,{}),"START enters original course-loading mode");
        require(start.mode_state.current==13u&&start.start_mode.stage==2u&&start.start_mode.course_load_attempts==1u&&start.start_mode.course_load_success==1u&&start.game_mode.course_load_success==1u,"START stage 0 loads real course before requesting 2F/2");
        require(start.start_mode.resource_id==LoaderAssetStartLoadingId&&start.start_mode.resource_ready==1u&&start.start_mode.event_open_count==1u,"START stage 2 retains PC readiness gate and does not open event 185 early");
        // 49BA80 world ids come from 44DBB0 on the live 7D30BC descriptor
        // (scene_owner_49ba80 + native_scene_owner_service).
    }
    std::printf("course_asset_pack: %u checks passed\n",checks);return 0;
}
