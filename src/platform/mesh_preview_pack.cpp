#include "platform/mesh_preview_pack.hpp"
#include <cmath>
#include <cstdio>
#include <cstring>
#include <limits>

namespace outrun::platform {
namespace {
constexpr std::array<std::uint8_t,8> MagicV1{{'O','R','2','M','S','H','1',0}};
constexpr std::array<std::uint8_t,8> MagicV2{{'O','R','2','M','S','H','2',0}};
constexpr std::array<std::uint8_t,8> MagicV3{{'O','R','2','M','S','H','3',0}};
constexpr std::size_t MaxPackBytes=128u*1024u*1024u;
constexpr std::uint32_t MaxVertices=2000000u;
constexpr std::uint32_t MaxIndices=6000000u;
constexpr std::uint32_t MaxBatches=100000u;
// The first vehicle diagnostic needed only 31 textures. Real course PMTs use
// substantially larger material sets; deko3d exposes 256 image descriptors and
// the renderer reserves eleven slots for fallback/frontend images.
constexpr std::uint32_t MaxTextures=192u;
constexpr std::uint32_t MaxTransforms=32u;

void fail(std::string* error,const char* message){if(error)*error=message;}
std::uint32_t u32(const std::uint8_t* p){return std::uint32_t(p[0])|(std::uint32_t(p[1])<<8u)|(std::uint32_t(p[2])<<16u)|(std::uint32_t(p[3])<<24u);}
std::int32_t i32(const std::uint8_t* p){const auto value=u32(p);return value<=0x7fffffffu?std::int32_t(value):std::int32_t(std::int64_t(value)-0x100000000LL);}
float f32(const std::uint8_t* p){const auto bits=u32(p);float value{};std::memcpy(&value,&bits,4);return value;}
bool add_size(std::size_t a,std::size_t b,std::size_t& out){if(b>std::numeric_limits<std::size_t>::max()-a)return false;out=a+b;return true;}
bool mul_size(std::size_t a,std::size_t b,std::size_t& out){if(b&&a>std::numeric_limits<std::size_t>::max()/b)return false;out=a*b;return true;}

bool infer_texture_mip_levels(std::uint32_t width,std::uint32_t height,
                              std::uint32_t format,std::size_t bytes,
                              std::uint32_t& levels){
    const std::size_t block_bytes=format==1u?8u:16u;
    std::size_t running=0u;levels=0u;
    while(true){
        std::size_t blocks{},level_bytes{};
        if(!mul_size(std::max(1u,(width+3u)/4u),std::max(1u,(height+3u)/4u),blocks)||
           !mul_size(blocks,block_bytes,level_bytes)||!add_size(running,level_bytes,running)||
           running>bytes)return false;
        ++levels;
        if(running==bytes)return true;
        if(width==1u&&height==1u)return false;
        width=std::max(1u,width/2u);height=std::max(1u,height/2u);
    }
}

bool read_bounds(const std::uint8_t* data,std::size_t bounds_offset,MeshPreviewPack& pack,std::string* error){
    auto read3=[&](std::size_t offset,std::array<float,3>& values){for(unsigned i=0;i<3u;++i){values[i]=f32(data+offset+i*4u);if(!std::isfinite(values[i]))return false;}return true;};
    if(!read3(bounds_offset,pack.source_min)||!read3(bounds_offset+12u,pack.source_max)||!read3(bounds_offset+24u,pack.packed_min)||!read3(bounds_offset+36u,pack.packed_max)){fail(error,"mesh pack non-finite bounds");return false;}
    for(unsigned i=0;i<3u;++i)if(pack.source_max[i]<pack.source_min[i]||pack.packed_max[i]<pack.packed_min[i]){fail(error,"mesh pack inverted bounds");return false;}
    std::memcpy(pack.source_sha256.data(),data+bounds_offset+48u,pack.source_sha256.size());
    return true;
}

bool read_vertex(const std::uint8_t* p,bool has_uv,bool normalized_position,MeshPreviewVertex& vertex,std::string* error){
    for(unsigned j=0;j<3u;++j)vertex.position[j]=f32(p+j*4u);
    for(unsigned j=0;j<3u;++j)vertex.normal[j]=f32(p+12u+j*4u);
    if(has_uv)for(unsigned j=0;j<2u;++j)vertex.uv[j]=f32(p+24u+j*4u);
    const auto color_offset=has_uv?32u:24u;
    for(unsigned j=0;j<4u;++j)vertex.color[j]=f32(p+color_offset+j*4u);
    for(float value:vertex.position)if(!std::isfinite(value)){fail(error,"mesh pack non-finite position");return false;}
    for(float value:vertex.normal)if(!std::isfinite(value)){fail(error,"mesh pack non-finite normal");return false;}
    for(float value:vertex.uv)if(!std::isfinite(value)){fail(error,"mesh pack non-finite UV");return false;}
    for(float value:vertex.color)if(!std::isfinite(value)||value<0.0f||value>1.0f){fail(error,"mesh pack invalid color");return false;}
    if(normalized_position&&(vertex.position[0]<-1.01f||vertex.position[0]>1.01f||vertex.position[1]<-1.01f||vertex.position[1]>1.01f||vertex.position[2]<0.0f||vertex.position[2]>1.0f)){fail(error,"mesh pack normalized position outside clip bounds");return false;}
    if(!normalized_position)for(float value:vertex.position)if(value<-10000.0f||value>10000.0f){fail(error,"mesh pack local position outside bounds");return false;}
    return true;
}

bool parse_v1(const std::uint8_t* data,std::size_t size,MeshPreviewPack& pack,std::string* error){
    constexpr std::size_t header_size=128u,vertex_stride=40u;
    if(size<header_size){fail(error,"mesh pack v1 header truncated");return false;}
    const auto vertex_count=u32(data+16u),index_count=u32(data+20u),vertex_offset=u32(data+112u),index_offset=u32(data+116u);
    if(u32(data+8u)!=1u||u32(data+12u)!=header_size||u32(data+24u)!=vertex_stride||u32(data+28u)!=1u||u32(data+120u)!=0u||u32(data+124u)!=0u){fail(error,"mesh pack v1 header mismatch");return false;}
    if(vertex_count==0u||vertex_count>MaxVertices||index_count==0u||index_count>MaxIndices||(index_count%3u)!=0u){fail(error,"mesh pack counts outside bounds");return false;}
    std::size_t vertex_bytes{},index_bytes{},expected{};
    if(!mul_size(vertex_count,vertex_stride,vertex_bytes)||!mul_size(index_count,4u,index_bytes)||vertex_offset!=header_size||!add_size(vertex_offset,vertex_bytes,expected)||index_offset!=expected||!add_size(index_offset,index_bytes,expected)||expected!=size){fail(error,"mesh pack v1 ranges mismatch");return false;}
    MeshPreviewPack next{};next.format_version=1u;if(!read_bounds(data,32u,next,error))return false;
    next.vertices.resize(vertex_count);for(std::size_t i=0;i<vertex_count;++i)if(!read_vertex(data+vertex_offset+i*vertex_stride,false,true,next.vertices[i],error))return false;
    next.indices.resize(index_count);for(std::size_t i=0;i<index_count;++i){next.indices[i]=u32(data+index_offset+i*4u);if(next.indices[i]>=vertex_count){fail(error,"mesh pack index outside vertex range");return false;}}
    next.batches.push_back({0u,index_count,-1,0u,0u,0u,0u});pack=std::move(next);return true;
}

bool parse_v2(const std::uint8_t* data,std::size_t size,MeshPreviewPack& pack,std::string* error){
    if(size<MeshPreviewPackHeaderSize){fail(error,"mesh pack v2 header truncated");return false;}
    const auto vertex_count=u32(data+16u),index_count=u32(data+20u),batch_count=u32(data+24u),texture_count=u32(data+28u);
    if(u32(data+8u)!=MeshPreviewPackVersion2||u32(data+12u)!=MeshPreviewPackHeaderSize||u32(data+32u)!=MeshPreviewVertexStride||u32(data+36u)!=MeshPreviewBatchStride||u32(data+40u)!=MeshPreviewTextureStride||u32(data+44u)!=7u){fail(error,"mesh pack v2 header mismatch");return false;}
    if(vertex_count==0u||vertex_count>MaxVertices||index_count==0u||index_count>MaxIndices||(index_count%3u)!=0u||batch_count==0u||batch_count>MaxBatches||texture_count>MaxTextures){fail(error,"mesh pack counts outside bounds");return false;}
    const auto vertex_offset=u32(data+128u),index_offset=u32(data+132u),batch_offset=u32(data+136u),texture_offset=u32(data+140u),texture_data_offset=u32(data+144u),file_size=u32(data+148u);
    if(u32(data+152u)!=0u||u32(data+156u)!=0u||file_size!=size){fail(error,"mesh pack v2 trailer mismatch");return false;}
    std::size_t vertex_bytes{},index_bytes{},batch_bytes{},texture_bytes{},expected{};
    if(!mul_size(vertex_count,MeshPreviewVertexStride,vertex_bytes)||!mul_size(index_count,4u,index_bytes)||!mul_size(batch_count,MeshPreviewBatchStride,batch_bytes)||!mul_size(texture_count,MeshPreviewTextureStride,texture_bytes)||vertex_offset!=MeshPreviewPackHeaderSize||!add_size(vertex_offset,vertex_bytes,expected)||index_offset!=expected||!add_size(index_offset,index_bytes,expected)||batch_offset!=expected||!add_size(batch_offset,batch_bytes,expected)||texture_offset!=expected||!add_size(texture_offset,texture_bytes,expected)||texture_data_offset!=expected||texture_data_offset>size){fail(error,"mesh pack v2 ranges mismatch");return false;}

    MeshPreviewPack next{};next.format_version=2u;if(!read_bounds(data,48u,next,error))return false;
    next.vertices.resize(vertex_count);for(std::size_t i=0;i<vertex_count;++i)if(!read_vertex(data+vertex_offset+i*MeshPreviewVertexStride,true,true,next.vertices[i],error))return false;
    next.indices.resize(index_count);for(std::size_t i=0;i<index_count;++i){next.indices[i]=u32(data+index_offset+i*4u);if(next.indices[i]>=vertex_count){fail(error,"mesh pack index outside vertex range");return false;}}
    next.batches.reserve(batch_count);std::uint64_t batch_indices=0u;
    for(std::size_t i=0;i<batch_count;++i){const auto* p=data+batch_offset+i*MeshPreviewBatchStride;MeshPreviewBatch batch{u32(p),u32(p+4u),i32(p+8u),u32(p+12u),u32(p+16u),u32(p+20u),0u};
        if(batch.index_count==0u||(batch.index_count%3u)!=0u||batch.first_index!=batch_indices||batch.first_index>index_count||batch.index_count>index_count-batch.first_index||batch.texture_index < -1||(batch.texture_index>=0&&std::uint32_t(batch.texture_index)>=texture_count)||(batch.flags&~1u)!=0u||u32(p+24u)!=0u||u32(p+28u)!=0u){fail(error,"mesh pack invalid batch");return false;}batch_indices+=batch.index_count;next.batches.push_back(batch);}
    if(batch_indices!=index_count){fail(error,"mesh pack batch coverage mismatch");return false;}
    next.textures.reserve(texture_count);std::size_t running=texture_data_offset;
    for(std::size_t i=0;i<texture_count;++i){const auto* p=data+texture_offset+i*MeshPreviewTextureStride;MeshPreviewTexture texture{};texture.source_index=u32(p);texture.width=u32(p+4u);texture.height=u32(p+8u);texture.format=static_cast<MeshPreviewTextureFormat>(u32(p+12u));const auto offset=u32(p+16u),bytes=u32(p+20u);
        const auto format=std::uint32_t(texture.format);if(texture.width==0u||texture.height==0u||texture.width>16384u||texture.height>16384u||format<1u||format>3u||u32(p+24u)!=0u||u32(p+28u)!=0u||offset!=running){fail(error,"mesh pack invalid texture metadata");return false;}
        if(!infer_texture_mip_levels(texture.width,texture.height,format,bytes,texture.mip_levels)||!add_size(running,bytes,running)||running>size){fail(error,"mesh pack invalid texture payload");return false;}
        texture.bytes.assign(data+offset,data+offset+bytes);next.textures.push_back(std::move(texture));}
    if(running!=size){fail(error,"mesh pack trailing texture data");return false;}pack=std::move(next);return true;
}

bool parse_v3(const std::uint8_t* data,std::size_t size,MeshPreviewPack& pack,std::string* error){
    if(size<MeshPreviewPackHeaderSize){fail(error,"mesh pack v3 header truncated");return false;}
    const auto vertex_count=u32(data+16u),index_count=u32(data+20u),batch_count=u32(data+24u),texture_count=u32(data+28u),transform_count=u32(data+156u);
    if(u32(data+8u)!=MeshPreviewPackVersion3||u32(data+12u)!=MeshPreviewPackHeaderSize||u32(data+32u)!=MeshPreviewVertexStride||u32(data+36u)!=MeshPreviewBatchStride||u32(data+40u)!=MeshPreviewTextureStride||u32(data+44u)!=7u){fail(error,"mesh pack v3 header mismatch");return false;}
    if(vertex_count==0u||vertex_count>MaxVertices||index_count==0u||index_count>MaxIndices||(index_count%3u)!=0u||batch_count==0u||batch_count>MaxBatches||texture_count>MaxTextures||transform_count==0u||transform_count>MaxTransforms){fail(error,"mesh pack v3 counts outside bounds");return false;}
    const auto vertex_offset=u32(data+128u),index_offset=u32(data+132u),batch_offset=u32(data+136u),texture_offset=u32(data+140u),transform_offset=u32(data+144u),texture_data_offset=u32(data+148u),file_size=u32(data+152u);
    if(file_size!=size){fail(error,"mesh pack v3 file size mismatch");return false;}
    std::size_t vertex_bytes{},index_bytes{},batch_bytes{},texture_bytes{},transform_bytes{},expected{};
    if(!mul_size(vertex_count,MeshPreviewVertexStride,vertex_bytes)||!mul_size(index_count,4u,index_bytes)||!mul_size(batch_count,MeshPreviewBatchStride,batch_bytes)||!mul_size(texture_count,MeshPreviewTextureStride,texture_bytes)||!mul_size(transform_count,MeshPreviewTransformStride,transform_bytes)||vertex_offset!=MeshPreviewPackHeaderSize||!add_size(vertex_offset,vertex_bytes,expected)||index_offset!=expected||!add_size(index_offset,index_bytes,expected)||batch_offset!=expected||!add_size(batch_offset,batch_bytes,expected)||texture_offset!=expected||!add_size(texture_offset,texture_bytes,expected)||transform_offset!=expected||!add_size(transform_offset,transform_bytes,expected)||texture_data_offset!=expected||texture_data_offset>size){fail(error,"mesh pack v3 ranges mismatch");return false;}

    MeshPreviewPack next{};next.format_version=3u;if(!read_bounds(data,48u,next,error))return false;
    next.vertices.resize(vertex_count);for(std::size_t i=0;i<vertex_count;++i)if(!read_vertex(data+vertex_offset+i*MeshPreviewVertexStride,true,false,next.vertices[i],error))return false;
    next.indices.resize(index_count);for(std::size_t i=0;i<index_count;++i){next.indices[i]=u32(data+index_offset+i*4u);if(next.indices[i]>=vertex_count){fail(error,"mesh pack index outside vertex range");return false;}}
    next.batches.reserve(batch_count);std::uint64_t batch_indices=0u;
    for(std::size_t i=0;i<batch_count;++i){const auto* p=data+batch_offset+i*MeshPreviewBatchStride;MeshPreviewBatch batch{u32(p),u32(p+4u),i32(p+8u),u32(p+12u),u32(p+16u),u32(p+20u),u32(p+24u)};
        if(batch.index_count==0u||(batch.index_count%3u)!=0u||batch.first_index!=batch_indices||batch.first_index>index_count||batch.index_count>index_count-batch.first_index||batch.texture_index < -1||(batch.texture_index>=0&&std::uint32_t(batch.texture_index)>=texture_count)||(batch.flags&~1u)!=0u||batch.transform_index>=transform_count||u32(p+28u)!=0u){fail(error,"mesh pack v3 invalid batch");return false;}batch_indices+=batch.index_count;next.batches.push_back(batch);}
    if(batch_indices!=index_count){fail(error,"mesh pack v3 batch coverage mismatch");return false;}
    std::vector<std::int32_t> vertex_transforms(vertex_count,-1);
    for(const auto& batch:next.batches)for(std::size_t i=batch.first_index;i<std::size_t(batch.first_index)+batch.index_count;++i){const auto vertex=next.indices[i];const auto transform=static_cast<std::int32_t>(batch.transform_index);if(vertex_transforms[vertex]>=0&&vertex_transforms[vertex]!=transform){fail(error,"mesh pack v3 vertex shared across transforms");return false;}vertex_transforms[vertex]=transform;}
    for(std::size_t i=0;i<vertex_count;++i){if(vertex_transforms[i]<0){fail(error,"mesh pack v3 unreferenced vertex");return false;}next.vertices[i].transform_index=static_cast<float>(vertex_transforms[i]);}
    next.transforms.resize(transform_count);
    for(std::size_t i=0;i<transform_count;++i){const auto* p=data+transform_offset+i*MeshPreviewTransformStride;auto& transform=next.transforms[i];for(unsigned j=0;j<16u;++j){transform.position[j]=f32(p+j*4u);transform.normal[j]=f32(p+64u+j*4u);if(!std::isfinite(transform.position[j])||!std::isfinite(transform.normal[j])){fail(error,"mesh pack v3 non-finite transform");return false;}}}
    next.textures.reserve(texture_count);std::size_t running=texture_data_offset;
    for(std::size_t i=0;i<texture_count;++i){const auto* p=data+texture_offset+i*MeshPreviewTextureStride;MeshPreviewTexture texture{};texture.source_index=u32(p);texture.width=u32(p+4u);texture.height=u32(p+8u);texture.format=static_cast<MeshPreviewTextureFormat>(u32(p+12u));const auto offset=u32(p+16u),bytes=u32(p+20u);
        const auto format=std::uint32_t(texture.format);if(texture.width==0u||texture.height==0u||texture.width>16384u||texture.height>16384u||format<1u||format>3u||u32(p+24u)!=0u||u32(p+28u)!=0u||offset!=running){fail(error,"mesh pack v3 invalid texture metadata");return false;}
        if(!infer_texture_mip_levels(texture.width,texture.height,format,bytes,texture.mip_levels)||!add_size(running,bytes,running)||running>size){fail(error,"mesh pack v3 invalid texture payload");return false;}
        texture.bytes.assign(data+offset,data+offset+bytes);next.textures.push_back(std::move(texture));}
    if(running!=size){fail(error,"mesh pack v3 trailing texture data");return false;}pack=std::move(next);return true;
}
}

bool parse_mesh_preview_pack(const std::uint8_t* data,std::size_t size,MeshPreviewPack& pack,std::string* error){
    if(error)error->clear();
    if((!data&&size!=0u)||size<8u||size>MaxPackBytes){fail(error,"mesh pack size outside bounds");return false;}
    if(std::memcmp(data,MagicV2.data(),MagicV2.size())==0)return parse_v2(data,size,pack,error);
    if(std::memcmp(data,MagicV3.data(),MagicV3.size())==0)return parse_v3(data,size,pack,error);
    if(std::memcmp(data,MagicV1.data(),MagicV1.size())==0)return parse_v1(data,size,pack,error);
    fail(error,"mesh pack magic mismatch");return false;
}

bool load_mesh_preview_pack_file(const char* path,MeshPreviewPack& pack,std::string* error){
    if(error)error->clear();
    if(!path){fail(error,"null mesh pack path");return false;}
    std::FILE* file=std::fopen(path,"rb");if(!file){fail(error,"cannot open mesh pack");return false;}
    if(std::fseek(file,0,SEEK_END)!=0){std::fclose(file);fail(error,"cannot size mesh pack");return false;}
    const auto length=std::ftell(file);
    if(length<0||static_cast<std::uint64_t>(length)>MaxPackBytes){std::fclose(file);fail(error,"mesh pack file outside bounds");return false;}
    std::rewind(file);std::vector<std::uint8_t> bytes(static_cast<std::size_t>(length));
    const auto read=bytes.empty()?0u:std::fread(bytes.data(),1,bytes.size(),file);const int extra=std::fgetc(file);std::fclose(file);
    if(read!=bytes.size()||extra!=EOF){fail(error,"mesh pack read mismatch");return false;}
    return parse_mesh_preview_pack(bytes.data(),bytes.size(),pack,error);
}

} // namespace outrun::platform
