#include "platform/course_collision_pack.hpp"
#include "driving/pc_course_world.hpp"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <limits>

namespace outrun::platform {
namespace {
constexpr std::array<std::uint8_t,8> Magic{{'O','R','2','C','O','L','1',0}};
constexpr std::array<std::uint8_t,8> OriginalMagic{{'O','R','2','C','O','L','2',0}};
constexpr std::uint32_t Version=1u,MaxQuads=100000u;
void fail(std::string* error,const char* message){if(error)*error=message;}
std::uint32_t u32(const std::uint8_t* p){return std::uint32_t(p[0])|(std::uint32_t(p[1])<<8u)|(std::uint32_t(p[2])<<16u)|(std::uint32_t(p[3])<<24u);}
float f32(const std::uint8_t* p){const auto bits=u32(p);float value{};std::memcpy(&value,&bits,4);return value;}

bool original_ground_at(const CourseCollisionPack& pack,float x,float z,
                        float reference_height,CourseGroundSample& sample){
    using namespace outrun::driving;
    try{
        const auto course=course_collision_tables(pack);
        std::array<std::uint8_t,128> matrix_stack{};
        std::array<std::uint8_t,64> identity{};
        PcMatrixStack matrices{Bytes(matrix_stack.data(),matrix_stack.size()),0,0,2};
        pc_matrix_identity(matrices);
        Bytes transform(identity.data(),identity.size());
        for(unsigned i=0;i<4u;++i)transform.putf((i*4u+i)*4u,1.0f);
        CourseWorldTables tables{};
        tables.courses[0]=course;
        tables.grids[0]=course_collision_grid(pack);
        tables.grids_present[0]=true;
        tables.transforms={transform,transform};
        EasyLctPredictionState prediction{};
        CourseWorldQuery query{tables,matrices,prediction};
        CourseProbe probe{x,reference_height,z};
        std::uint32_t index=0xffffffffu,flags=1u;
        get_y_position_spl_chk(query,probe,&index,nullptr,&flags);
        if(flags==1u||index>=pack.quads.size()||!std::isfinite(probe.y))return false;
        const auto normal=course_collision_world_normal(course,
            static_cast<std::int32_t>(index),matrices,transform);
        if(!std::isfinite(normal.x)||!std::isfinite(normal.y)||
           !std::isfinite(normal.z))return false;
        sample={probe.y,{normal.x,normal.y,normal.z},index,flags};
        return true;
    }catch(const std::exception&){return false;}
}

bool triangle_height(const std::array<float,3>& a,const std::array<float,3>& b,
                     const std::array<float,3>& c,float x,float z,
                     float& height,std::array<float,3>& normal){
    const float denominator=(b[2]-c[2])*(a[0]-c[0])+(c[0]-b[0])*(a[2]-c[2]);
    if(std::fabs(denominator)<1.0e-6f)return false;
    const float wa=((b[2]-c[2])*(x-c[0])+(c[0]-b[0])*(z-c[2]))/denominator;
    const float wb=((c[2]-a[2])*(x-c[0])+(a[0]-c[0])*(z-c[2]))/denominator;
    const float wc=1.0f-wa-wb;
    constexpr float Epsilon=-1.0e-4f;
    if(wa<Epsilon||wb<Epsilon||wc<Epsilon)return false;
    height=wa*a[1]+wb*b[1]+wc*c[1];
    const std::array<float,3> ab{{b[0]-a[0],b[1]-a[1],b[2]-a[2]}};
    const std::array<float,3> ac{{c[0]-a[0],c[1]-a[1],c[2]-a[2]}};
    normal={{ab[1]*ac[2]-ab[2]*ac[1],ab[2]*ac[0]-ab[0]*ac[2],ab[0]*ac[1]-ab[1]*ac[0]}};
    float length=std::sqrt(normal[0]*normal[0]+normal[1]*normal[1]+normal[2]*normal[2]);
    if(!std::isfinite(height)||!std::isfinite(length)||length<=1.0e-6f)return false;
    if(normal[1]<0.0f)for(float& value:normal)value=-value;
    for(float& value:normal)value/=length;
    return normal[1]>=0.35f;
}
}

bool parse_course_collision_pack(const std::uint8_t* data,std::size_t size,
                                 CourseCollisionPack& pack,std::string* error){
    if(error)error->clear();
    if(data&&size>=CourseCollisionHeaderSize&&
       std::memcmp(data,OriginalMagic.data(),OriginalMagic.size())==0){
        const auto count=u32(data+16u),stride=u32(data+20u),offset=u32(data+24u);
        const auto file_size=u32(data+28u),source_size=u32(data+32u);
        if(u32(data+8u)!=2u||u32(data+12u)!=CourseCollisionHeaderSize||
           count==0u||count>MaxQuads||stride!=64u||offset!=CourseCollisionHeaderSize||
           file_size!=size||std::uint64_t(offset)+source_size!=size||
           source_size<0x20044u){
            fail(error,"original course collision layout mismatch");return false;
        }
        for(std::size_t i=72u;i<CourseCollisionHeaderSize;++i)if(data[i]!=0u){
            fail(error,"original course collision reserved header is nonzero");return false;}
        CourseCollisionPack next{};
        if(!parse_pc_coli0200(data+offset,source_size,next,error))return false;
        if(next.pc_layout.polygon_count!=count||
           next.source_quad_offset!=u32(data+36u)){
            fail(error,"original COLI0200 wrapper/source mismatch");return false;
        }
        std::memcpy(next.source_sha256.data(),data+40u,next.source_sha256.size());
        pack=std::move(next);return true;
    }
    if(!data||size<CourseCollisionHeaderSize||std::memcmp(data,Magic.data(),Magic.size())!=0){fail(error,"course collision magic/header mismatch");return false;}
    const auto count=u32(data+16u),stride=u32(data+20u),offset=u32(data+24u),file_size=u32(data+28u);
    if(u32(data+8u)!=Version||u32(data+12u)!=CourseCollisionHeaderSize||count==0u||count>MaxQuads||stride!=CourseCollisionQuadStride||offset!=CourseCollisionHeaderSize||file_size!=size||std::uint64_t(offset)+std::uint64_t(count)*stride!=size){fail(error,"course collision layout mismatch");return false;}
    for(std::size_t i=72u;i<CourseCollisionHeaderSize;++i)if(data[i]!=0u){fail(error,"course collision reserved header is nonzero");return false;}
    CourseCollisionPack next{};next.source_bytes=u32(data+32u);next.source_quad_offset=u32(data+36u);std::memcpy(next.source_sha256.data(),data+40u,next.source_sha256.size());next.quads.resize(count);
    for(std::size_t i=0;i<count;++i){const auto* p=data+offset+i*stride;auto& quad=next.quads[i];quad.flags=u32(p);for(unsigned vertex=0;vertex<4u;++vertex)for(unsigned axis=0;axis<3u;++axis){quad.vertices[vertex][axis]=f32(p+4u+(vertex*3u+axis)*4u);if(!std::isfinite(quad.vertices[vertex][axis])){fail(error,"course collision non-finite vertex");return false;}}quad.material=u32(p+52u);quad.center_xz={{f32(p+56u),f32(p+60u)}};if(!std::isfinite(quad.center_xz[0])||!std::isfinite(quad.center_xz[1])){fail(error,"course collision non-finite center");return false;}}
    pack=std::move(next);return true;
}

bool load_course_collision_pack_file(const char* path,CourseCollisionPack& pack,
                                     std::string* error){
    if(error)error->clear();
    if(!path){fail(error,"null course collision path");return false;}
    std::FILE* file=std::fopen(path,"rb");if(!file){fail(error,"cannot open course collision pack");return false;}
    if(std::fseek(file,0,SEEK_END)!=0){std::fclose(file);fail(error,"cannot size course collision pack");return false;}const auto length=std::ftell(file);std::rewind(file);
    if(length<0||length>16*1024*1024){std::fclose(file);fail(error,"course collision pack size outside bounds");return false;}
    std::vector<std::uint8_t> bytes(static_cast<std::size_t>(length));const auto read=bytes.empty()?0u:std::fread(bytes.data(),1,bytes.size(),file);const int extra=std::fgetc(file);std::fclose(file);if(read!=bytes.size()||extra!=EOF){fail(error,"course collision pack read mismatch");return false;}return parse_course_collision_pack(bytes.data(),bytes.size(),pack,error);
}

bool course_collision_ground_at(const CourseCollisionPack& pack,float x,float z,
                                float reference_height,CourseGroundSample& sample){
    if(!std::isfinite(x)||!std::isfinite(z)||!std::isfinite(reference_height))return false;
    if(!pack.pc_coli0200.empty())return original_ground_at(pack,x,z,reference_height,sample);
    bool found=false;float best_distance=std::numeric_limits<float>::infinity();CourseGroundSample best{};
    for(std::size_t index=0;index<pack.quads.size();++index){const auto& quad=pack.quads[index];
        const float min_x=std::min({quad.vertices[0][0],quad.vertices[1][0],quad.vertices[2][0],quad.vertices[3][0]});const float max_x=std::max({quad.vertices[0][0],quad.vertices[1][0],quad.vertices[2][0],quad.vertices[3][0]});const float min_z=std::min({quad.vertices[0][2],quad.vertices[1][2],quad.vertices[2][2],quad.vertices[3][2]});const float max_z=std::max({quad.vertices[0][2],quad.vertices[1][2],quad.vertices[2][2],quad.vertices[3][2]});
        if(x<min_x-1.0e-4f||x>max_x+1.0e-4f||z<min_z-1.0e-4f||z>max_z+1.0e-4f)continue;
        float height{};std::array<float,3> normal{};
        if(!triangle_height(quad.vertices[0],quad.vertices[1],quad.vertices[2],x,z,height,normal)&&!triangle_height(quad.vertices[0],quad.vertices[2],quad.vertices[3],x,z,height,normal))continue;
        const float distance=std::fabs(height-reference_height);if(distance<best_distance){found=true;best_distance=distance;best={height,normal,static_cast<std::uint32_t>(index),quad.material};}
    }
    if(found)sample=best;
    return found;
}

} // namespace outrun::platform
