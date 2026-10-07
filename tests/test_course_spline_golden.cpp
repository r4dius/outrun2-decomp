// Portable replay of synthetic expected values captured from the original PC.
// No executable/DLL, oracle, fixed mapping or x86 CPU is required by this test.
#include "driving/pc_course_spline.hpp"
#include <array>
#include <fstream>
#include <iostream>
#include <vector>
#include <cstring>
#include <stdexcept>
using namespace outrun::driving;
int main(int argc,char** argv){try{
    if(argc!=2)throw std::runtime_error("expected snapshot file path");
    std::ifstream in(argv[1],std::ios::binary);if(!in)throw std::runtime_error("cannot open snapshot");
    std::vector<std::uint8_t> data((std::istreambuf_iterator<char>(in)),{});Bytes b(data.data(),data.size());
    if(data.size()<20||std::memcmp(data.data(),"OR2S014\0",8)||b.u32(8)!=1)throw std::runtime_error("invalid spline snapshot header");
    const auto count=b.u32(16);constexpr std::size_t record_size=36+0x380*2;
    if(count==0||data.size()!=20+std::size_t(count)*record_size)throw std::runtime_error("invalid record count/size");
    std::array<unsigned,11> counts{};unsigned failures=0;
    for(unsigned r=0;r<count;++r){const auto pos=20+std::size_t(r)*record_size;auto rec=b.sub(pos,record_size);
        const auto id=rec.u32(0),seed=rec.u32(4),expected_scalar=rec.u32(8);if(id>=11)throw std::runtime_error("invalid routine id");++counts[id];
        CourseSplineTuning tuning{rec.f32(12),rec.f32(16)};const float u=rec.f32(20),v=rec.f32(24),value=rec.f32(28);const auto mask=rec.u32(32);
        std::array<std::uint8_t,0x380> image{};for(unsigned j=0;j<image.size();++j)image[j]=rec.u8(36+j);Bytes im(image.data(),image.size());
        const auto p=course_quad_vertices(im.sub(0x100,0x30)),normals=course_quad_vertices(im.sub(0x140,0x30));
        const auto forward=course_quad_vertices(im.sub(0x180,0x30)),back=course_quad_vertices(im.sub(0x1c0,0x30));
        const auto left=course_quad_vertices(im.sub(0x200,0x30)),right=course_quad_vertices(im.sub(0x240,0x30));
        const CourseSplineNeighbors n{mask&1u?&forward:nullptr,mask&2u?&back:nullptr,mask&4u?&left:nullptr,mask&8u?&right:nullptr};
        auto readv=[&](unsigned off){return CourseVec3{im.f32(off),im.f32(off+4),im.f32(off+8)};};
        auto writev=[&](unsigned off,const CourseVec3& t){im.putf(off,t.x);im.putf(off+4,t.y);im.putf(off+8,t.z);};
        const auto point=readv(0x280);CourseTangents t{};for(unsigned j=0;j<8;++j)t[j]=readv(0x300+j*12);
        const std::array<float,3> coef{im.f32(0x2a0),im.f32(0x2a4),im.f32(0x2a8)};float scalar=0;bool has_scalar=false;
        switch(id){
        case 0:scalar=float(course_vec3_distance(p[0],p[2]));has_scalar=true;break;
        case 1:scalar=float(course_vec3_length_squared(p[0]));has_scalar=true;break;
        case 2:{CourseVec3 a{},c{};calc_normal_to_delta(p[0],p[1],normals[0],normals[1],tuning.lateral,a,c);writev(0x20,a);writev(0x2c,c);break;}
        case 3:{CourseQuad a{},c{};calc_hermite_direction_vector2(a,c,p,n);for(unsigned j=0;j<4;++j){writev(0x20+j*12,a[j]);writev(0x50+j*12,c[j]);}break;}
        case 4:calc_hermite_tangent(t,p,normals,tuning);for(unsigned j=0;j<8;++j)writev(0x20+j*12,t[j]);break;
        case 5:for(unsigned j=0;j<8;++j)t[j]=readv(0x20+j*12);calc_hermite_tangent2(t,p,n,tuning);for(unsigned j=0;j<8;++j)writev(0x20+j*12,t[j]);break;
        case 6:{const auto c=calc_hermite_coefficients(p[0],p[3],t[0],t[1]);for(unsigned j=0;j<3;++j)im.putf(0x20+j*4,c[j]);break;}
        case 7:scalar=calc_carry_variable(value,coef);has_scalar=true;break;
        case 8:{CourseVec3 out{};solve_hermite(out,point,u,v,p,t);writev(0x20,out);break;}
        case 9:scalar=calc_hermite2(point,u,v,p,normals,n,tuning);has_scalar=true;break;
        case 10:scalar=calc_y_pos_spl(point.x,point.z,p,normals,n,tuning);has_scalar=true;break;
        }
        bool match=true;
        if(has_scalar){std::uint32_t bits;std::memcpy(&bits,&scalar,4);match=bits==expected_scalar;}
        for(unsigned j=0;j<image.size();++j)if(image[j]!=rec.u8(36+0x380+j)){match=false;break;}
        if(!match){++failures;if(failures<=10)std::cerr<<"record="<<r<<" routine="<<id<<" case="<<seed<<" differs from original x86\n";}
    }
    for(auto c:counts)if(c==0)throw std::runtime_error("routine missing from golden corpus");
    std::cout<<count<<" original x86 snapshots, "<<failures<<" mismatches; guest x87 control="<<std::hex<<b.u32(12)<<"\n";
    return failures?1:0;
}catch(const std::exception& e){std::cerr<<e.what()<<"\n";return 2;}}
