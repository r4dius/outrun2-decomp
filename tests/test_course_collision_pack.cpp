#include "platform/course_collision_pack.hpp"
#include <cmath>
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
std::vector<std::uint8_t> synthetic(){
    std::vector<std::uint8_t> bytes(CourseCollisionHeaderSize+CourseCollisionQuadStride);const std::uint8_t magic[8]={'O','R','2','C','O','L','1',0};std::memcpy(bytes.data(),magic,8);put32(bytes,8,1u);put32(bytes,12,CourseCollisionHeaderSize);put32(bytes,16,1u);put32(bytes,20,CourseCollisionQuadStride);put32(bytes,24,CourseCollisionHeaderSize);put32(bytes,28,static_cast<std::uint32_t>(bytes.size()));put32(bytes,32,688900u);put32(bytes,36,0x258c0u);
    const float vertices[4][3]={{-2.0f,1.0f,-2.0f},{2.0f,1.0f,-2.0f},{2.0f,2.0f,2.0f},{-2.0f,2.0f,2.0f}};const auto base=CourseCollisionHeaderSize;put32(bytes,base,7u);for(unsigned v=0;v<4u;++v)for(unsigned a=0;a<3u;++a)putf(bytes,base+4u+(v*3u+a)*4u,vertices[v][a]);put32(bytes,base+52u,3u);putf(bytes,base+56u,0.0f);putf(bytes,base+60u,0.0f);return bytes;
}
}
int main(int argc,char** argv){
    auto bytes=synthetic();CourseCollisionPack pack{};std::string error;require(parse_course_collision_pack(bytes.data(),bytes.size(),pack,&error),"valid synthetic OR2COL1");require(pack.quads.size()==1u&&pack.quads[0].flags==7u&&pack.quads[0].material==3u,"synthetic fields retained");CourseGroundSample ground{};require(course_collision_ground_at(pack,0.0f,0.0f,1.5f,ground)&&ground.height>1.49f&&ground.height<1.51f&&ground.normal[1]>0.9f,"sloped quad ground interpolation");require(!course_collision_ground_at(pack,3.0f,0.0f,1.5f,ground),"outside quad rejected");auto bad=bytes;put32(bad,16,2u);require(!parse_course_collision_pack(bad.data(),bad.size(),pack,&error),"bad layout rejected");bad=bytes;put32(bad,72,1u);require(!parse_course_collision_pack(bad.data(),bad.size(),pack,&error),"reserved header rejected");
    if(argc>=2){CourseCollisionPack real{};require(load_course_collision_pack_file(argv[1],real,&error),"real OR2COL2 loads");require(real.quads.size()==4417u&&real.source_bytes==688900u&&real.source_quad_offset==0x258c0u&&real.pc_coli0200.size()==688900u,"full original collision tables retained");require(course_collision_ground_at(real,0.0f,-4.0f,0.0f,ground)&&ground.height>-0.1f&&ground.height<0.1f&&ground.surface_flags==2u,"PC world/grid/spline query and kind classification at start");require(real.quads[ground.quad_index].material==ground.surface_flags,"per-polygon retail kind maps to PC surface bits");require(!course_collision_ground_at(real,3000.0f,3000.0f,0.0f,ground),"empty original grid cell rejects");
        const float positions[][3]={{150.32f,5.14f,-463.20f},{251.37f,21.66f,-336.63f},{868.22f,23.0f,-682.35f}};
        for(const auto& position:positions)require(course_collision_ground_at(real,position[0],position[2],position[1],ground)&&ground.quad_index<real.quads.size()&&ground.surface_flags==real.quads[ground.quad_index].material&&std::isfinite(ground.height),"PC query resolves previously observed on-road hardware poses");
    }
    if(argc>=3){CourseCollisionPack beach{};require(load_course_collision_pack_file(argv[2],beach,&error),"original BEAC COLI0200 loads through generic parser");require(beach.quads.size()==5597u&&beach.source_bytes==835204u&&beach.source_quad_offset==0x26ea0u,"BEAC has independent grid, polygon count and source layout");bool sampled=false;for(const auto& quad:beach.quads){if(course_collision_ground_at(beach,quad.center_xz[0],quad.center_xz[1],quad.vertices[0][1],ground)&&ground.quad_index<beach.quads.size()){sampled=true;break;}}require(sampled,"original BEAC grid resolves a live polygon center");}
    std::printf("course_collision_pack: %u checks passed\n",checks);return 0;
}
