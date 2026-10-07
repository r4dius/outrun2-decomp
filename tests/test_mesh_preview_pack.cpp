#include "platform/mesh_preview_pack.hpp"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>
using namespace outrun::platform;
namespace {
unsigned checks{};
void require(bool value,const char* message){++checks;if(!value){std::fprintf(stderr,"FAIL: %s\n",message);std::exit(1);}}
void put32(std::vector<std::uint8_t>& bytes,std::size_t offset,std::uint32_t value){bytes[offset]=std::uint8_t(value);bytes[offset+1]=std::uint8_t(value>>8u);bytes[offset+2]=std::uint8_t(value>>16u);bytes[offset+3]=std::uint8_t(value>>24u);}
void putf(std::vector<std::uint8_t>& bytes,std::size_t offset,float value){std::uint32_t bits{};std::memcpy(&bits,&value,4);put32(bytes,offset,bits);}

std::vector<std::uint8_t> synthetic_v2(){
    constexpr std::uint32_t vertices=3u,indices=3u,batches=1u,textures=1u;
    constexpr std::uint32_t vertex_offset=MeshPreviewPackHeaderSize,index_offset=vertex_offset+vertices*MeshPreviewVertexStride;
    constexpr std::uint32_t batch_offset=index_offset+indices*4u,texture_offset=batch_offset+batches*MeshPreviewBatchStride;
    constexpr std::uint32_t texture_data_offset=texture_offset+textures*MeshPreviewTextureStride,file_size=texture_data_offset+8u;
    std::vector<std::uint8_t> bytes(file_size);const std::uint8_t magic[8]={'O','R','2','M','S','H','2',0};std::memcpy(bytes.data(),magic,8);
    put32(bytes,8,MeshPreviewPackVersion2);put32(bytes,12,MeshPreviewPackHeaderSize);put32(bytes,16,vertices);put32(bytes,20,indices);put32(bytes,24,batches);put32(bytes,28,textures);put32(bytes,32,MeshPreviewVertexStride);put32(bytes,36,MeshPreviewBatchStride);put32(bytes,40,MeshPreviewTextureStride);put32(bytes,44,7u);
    for(unsigned axis=0;axis<3u;++axis){putf(bytes,48u+axis*4u,-1.0f);putf(bytes,60u+axis*4u,1.0f);putf(bytes,72u+axis*4u,axis==2u?0.1f:-0.5f);putf(bytes,84u+axis*4u,axis==2u?0.9f:0.5f);}
    put32(bytes,128,vertex_offset);put32(bytes,132,index_offset);put32(bytes,136,batch_offset);put32(bytes,140,texture_offset);put32(bytes,144,texture_data_offset);put32(bytes,148,file_size);
    const float position[3][3]={{0.0f,0.5f,0.2f},{-0.5f,-0.5f,0.8f},{0.5f,-0.5f,0.8f}};
    for(unsigned i=0;i<vertices;++i){const auto base=vertex_offset+i*MeshPreviewVertexStride;for(unsigned j=0;j<3u;++j)putf(bytes,base+j*4u,position[i][j]);putf(bytes,base+20u,1.0f);putf(bytes,base+24u,float(i&1u));putf(bytes,base+28u,float(i>>1u));for(unsigned j=0;j<4u;++j)putf(bytes,base+32u+j*4u,1.0f);put32(bytes,index_offset+i*4u,i);}
    put32(bytes,batch_offset,0u);put32(bytes,batch_offset+4u,indices);put32(bytes,batch_offset+8u,0u);put32(bytes,batch_offset+16u,7u);put32(bytes,batch_offset+20u,0x5401u);
    put32(bytes,texture_offset,9u);put32(bytes,texture_offset+4u,4u);put32(bytes,texture_offset+8u,4u);put32(bytes,texture_offset+12u,1u);put32(bytes,texture_offset+16u,texture_data_offset);put32(bytes,texture_offset+20u,8u);for(unsigned i=0;i<8u;++i)bytes[texture_data_offset+i]=std::uint8_t(i+1u);return bytes;
}

std::vector<std::uint8_t> synthetic_v3(){
    auto old=synthetic_v2();constexpr std::uint32_t transform_count=1u,transform_bytes=MeshPreviewTransformStride;
    const auto old_texture_data=std::uint32_t(old.size()-8u);std::vector<std::uint8_t> bytes(old.size()+transform_bytes);
    std::memcpy(bytes.data(),old.data(),old_texture_data);std::memcpy(bytes.data()+old_texture_data+transform_bytes,old.data()+old_texture_data,8u);
    const std::uint8_t magic[8]={'O','R','2','M','S','H','3',0};std::memcpy(bytes.data(),magic,8);put32(bytes,8,MeshPreviewPackVersion3);
    put32(bytes,144,old_texture_data);put32(bytes,148,old_texture_data+transform_bytes);put32(bytes,152,std::uint32_t(bytes.size()));put32(bytes,156,transform_count);
    const auto texture_offset=std::uint32_t(MeshPreviewPackHeaderSize+3u*MeshPreviewVertexStride+3u*4u+MeshPreviewBatchStride);put32(bytes,texture_offset+16u,old_texture_data+transform_bytes);
    for(unsigned matrix=0;matrix<2u;++matrix)for(unsigned i=0;i<16u;++i)putf(bytes,old_texture_data+matrix*64u+i*4u,(i%5u)==0u?1.0f:0.0f);
    putf(bytes,MeshPreviewPackHeaderSize,2.0f);return bytes;
}
std::vector<std::uint8_t> synthetic_v2_mips(){
    auto bytes=synthetic_v2();
    const auto texture_offset=std::uint32_t(MeshPreviewPackHeaderSize+3u*MeshPreviewVertexStride+3u*4u+MeshPreviewBatchStride);
    const auto texture_data_offset=texture_offset+MeshPreviewTextureStride;
    bytes.resize(texture_data_offset+48u);
    put32(bytes,148,std::uint32_t(bytes.size()));
    put32(bytes,texture_offset+4u,8u);put32(bytes,texture_offset+8u,8u);
    put32(bytes,texture_offset+20u,48u);
    for(unsigned i=0;i<48u;++i)bytes[texture_data_offset+i]=std::uint8_t(i);
    return bytes;
}
}
int main(int argc,char** argv){
    auto bytes=synthetic_v2();MeshPreviewPack pack{};std::string error;
    require(parse_mesh_preview_pack(bytes.data(),bytes.size(),pack,&error),"valid synthetic OR2MSH2");
    require(pack.format_version==2u&&pack.vertices.size()==3u&&pack.indices[2]==2u&&pack.batches.size()==1u&&pack.batches[0].transform_index==0u&&pack.textures.size()==1u,"v2 payload retained");
    auto bad=bytes;const auto index_offset=MeshPreviewPackHeaderSize+3u*MeshPreviewVertexStride;put32(bad,index_offset+8u,3u);require(!parse_mesh_preview_pack(bad.data(),bad.size(),pack,&error),"out-of-range index rejected");
    const auto batch_offset=index_offset+12u;bad=bytes;put32(bad,batch_offset+4u,6u);require(!parse_mesh_preview_pack(bad.data(),bad.size(),pack,&error),"out-of-range batch rejected");
    const auto texture_offset=batch_offset+MeshPreviewBatchStride;bad=bytes;put32(bad,texture_offset+20u,12u);require(!parse_mesh_preview_pack(bad.data(),bad.size(),pack,&error),"bad BC payload size rejected");
    auto mips=synthetic_v2_mips();require(parse_mesh_preview_pack(mips.data(),mips.size(),pack,&error)&&pack.textures[0].mip_levels==3u&&pack.textures[0].bytes.size()==48u,"complete BC mip chain inferred");
    bad=bytes;bad[0]='X';require(!parse_mesh_preview_pack(bad.data(),bad.size(),pack,&error),"bad magic rejected");
    auto v3=synthetic_v3();require(parse_mesh_preview_pack(v3.data(),v3.size(),pack,&error),"valid synthetic OR2MSH3");
    require(pack.format_version==3u&&pack.transforms.size()==1u&&pack.transforms[0].position[0]==1.0f&&pack.vertices[0].position[0]==2.0f,"v3 local geometry and transform retained");
    bad=v3;put32(bad,batch_offset+24u,1u);require(!parse_mesh_preview_pack(bad.data(),bad.size(),pack,&error),"v3 out-of-range transform rejected");
    if(argc==2){MeshPreviewPack real{};require(load_mesh_preview_pack_file(argv[1],real,&error),"generated OR2MSH3 loads");require(real.format_version==3u&&real.vertices.size()==8757u&&real.indices.size()==29505u&&real.batches.size()==54u&&real.textures.size()==25u&&real.transforms.size()==7u,"generated r127 vehicle OR2MSH3 counts");require(real.textures[0].mip_levels>1u,"vehicle DDS mip chains retained");}
    if(argc==3&&std::strcmp(argv[1],"--course")==0){MeshPreviewPack real{};require(load_mesh_preview_pack_file(argv[2],real,&error),"generated course OR2MSH3 loads");require(real.format_version==3u&&real.vertices.size()==137098u&&real.indices.size()==388653u&&real.batches.size()==1312u&&real.textures.size()==94u&&real.transforms.size()==1u,"generated r127 course/environment OR2MSH3 counts");require(real.textures[0].mip_levels>1u,"course DDS mip chains retained");}
    std::printf("mesh_preview_pack: %u checks passed\n",checks);return 0;
}
