#include "driving/pc_course_spline.hpp"
#include "driving/pc_course_topology.hpp"
#include <iostream>
#include <cstring>
#include <cmath>
#include <limits>
using namespace outrun::driving;
namespace {
void require(bool condition,const char* why){if(!condition)throw std::runtime_error(why);}
template<class E,class F> void throws(F f,const char* why){bool ok=false;try{f();}catch(const E&){ok=true;}require(ok,why);}
bool same(const CourseVec3& a,const CourseVec3& b){return std::memcmp(&a,&b,sizeof(a))==0;}
}
int main(){try{
    const CourseQuad p{CourseVec3{0,0,0},{0,0,2},{4,0,2},{4,0,0}};
    const CourseQuad normals{CourseVec3{0,1,0},{0,1,0},{0,1,0},{0,1,0}};
    CourseTangents t{};calc_hermite_tangent(t,p,normals);
    const CourseVec3 lateral{0,0,2},longitudinal{4,-0.0f,0};
    for(unsigned j=0;j<4;++j)require(same(t[j],lateral),"normal-derived lateral tangent");
    for(unsigned j=4;j<8;++j)require(same(t[j],longitudinal),"normal-derived longitudinal tangent");
    const CourseSplineNeighbors none{};
    CourseTangents geometry{};require(calc_hermite_tangent2(geometry,p,none)==255,"ordinary tangent write mask");
    for(unsigned j=0;j<8;++j)require(t[j].x==geometry[j].x&&t[j].y==geometry[j].y&&t[j].z==geometry[j].z,"normal/geometry tangent value agreement on exact rectangle");
    for(float u:{0.0f,0.25f,0.5f,0.75f,1.0f})for(float v:{0.0f,0.5f,1.0f}){
        require(calc_hermite2({4*v,123,2*u},u,v,p,normals,none)==0.0f,"planar arithmetic fixture");
        require(calc_y_pos_spl(4*v,2*u,p,normals,none)==0.0f,"full spline inversion on arithmetic fixture");
    }
    // Lower helper leaves original output untouched for degenerate directions.
    CourseQuad collapsed{};CourseTangents sentinels{};for(unsigned j=0;j<8;++j)sentinels[j]={float(j+1),-3.0f,7.0f};
    const auto before=sentinels;
    require(calc_hermite_tangent2(sentinels,collapsed,none)==0,"zero direction write mask");
    require(std::memcmp(before.data(),sentinels.data(),sizeof(before))==0,"degenerate direction destroyed caller storage");
    const CourseSplineNeighbors degenerate{&collapsed,&collapsed,nullptr,nullptr};
    throws<std::domain_error>([&]{(void)calc_hermite2({},0.5f,0.5f,collapsed,normals,degenerate);},"undefined PC stack tangent was silently replaced");
    CourseVec3 da{2,3,4},db{5,6,7};calc_normal_to_delta({}, {}, {0,1,0},{0,1,0},1,da,db);
    require(da.x==0&&da.y==0&&da.z==0&&db.x==0&&db.y==0&&db.z==0,"coincident endpoints");
    require(calc_carry_variable(0.25f,{0,4,3})==0.25f,"zero leading coefficient path");
    require(calc_carry_variable(0.5f,{-2,3,0})==0.5f,"exact extremum root path");
    require(calc_carry_variable(0.0f,{-2,3,0})==0.0f,"exact zero root path");
    require(calc_carry_variable(1.0f,{-2,3,0})==1.0f,"exact one root path");
    // Dominant-axis equality uses Z, not X; distinct tangents expose the choice.
    const auto c=calc_hermite_coefficients({0,0,0},{1,0,1},{7,0,1},{9,0,1});
    require(c[0]==0&&c[1]==0&&c[2]==1,"coefficient dominant-axis tie");
    std::array<std::uint8_t,0x40> raw{};Bytes rb(raw.data(),raw.size());
    for(unsigned j=0;j<4;++j){rb.putf(j*12,p[j].x);rb.putf(j*12+4,p[j].y);rb.putf(j*12+8,p[j].z);}
    rb.put32(0x30,0xdeadbeef);rb.put32(0x34,0xffff0001);rb.put32(0x38,0xcafebabe);
    require(std::memcmp(course_quad_vertices(rb).data(),p.data(),sizeof(p))==0,"record tail misread as pointer/vertex");
    throws<std::out_of_range>([&]{(void)course_quad_vertices(rb.sub(0,0x2f));},"truncated polygon accepted");
    // Course-run topology: direct ranges, scans, failure and atomic rejection.
    std::array<std::uint8_t,16> header{},lengths{},ranges{};
    Bytes h(header.data(),header.size()),l(lengths.data(),lengths.size()),r(ranges.data(),ranges.size());
    h.puti(12,8);const unsigned entries[]={0,0,1,1,1,2,3,3};
    for(unsigned j=0;j<8;++j)l.put16(j*2,std::uint16_t(entries[j]));
    const unsigned starts[]={0,2,5,6},ends[]={1,4,5,7};
    for(unsigned j=0;j<4;++j){r.put16(j*4,std::uint16_t(starts[j]));r.put16(j*4+2,std::uint16_t(ends[j]));}
    CourseRunTables tables{h,l,r,4,false,true};
    for(int hint=-1;hint<=9;++hint)for(int target=0;target<6;++target){int first=-99,last=-98;
        const auto n=find_primary_course_run(tables,std::int16_t(target),hint,first,last);
        const auto k=unsigned(target>3?3:target);require(first==int(starts[k])&&last==int(ends[k])&&n==int(ends[k]-starts[k]+1),"course run branch mismatch");}
    int first=-99,last=-98;tables.present=false;tables.header=Bytes(nullptr,0);
    require(find_primary_course_run(tables,1,0,first,last)==0&&first==-99&&last==-98,"absent table mutated outputs");
    tables={h,l,r,5,false,true};require(find_primary_course_run(tables,4,0,first,last)==0&&first==-99&&last==-98,"search miss mutated outputs");
    tables={h,l,r.sub(0,3),4,true,true};
    throws<std::out_of_range>([&]{find_primary_course_run(tables,0,-1,first,last);},"truncated pair accepted");
    require(first==-99&&last==-98,"rejected range partially wrote outputs");
    tables={h,l,r,4,false,true};
    throws<std::out_of_range>([&]{find_primary_course_run(tables,0,-2,first,last);},"negative native index accepted");
    std::cout<<"Spline exact-boundary / explicit-view / rejected-state tests passed\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<"\n";return 1;}}
