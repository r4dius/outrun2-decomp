#include "support/course_world_fixture.hpp"
#include <iostream>
#include <limits>
#include <stdexcept>
using namespace outrun::driving;
using namespace outrun::testing;
namespace {
void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
template<class F> void rejects(F&& call){bool caught=false;try{call();}catch(const std::exception&){caught=true;}require(caught,"malformed native view not rejected");}
void test_matrix(){
    std::array<std::uint8_t,256> memory{};Bytes b(memory.data(),memory.size());
    for(unsigned i=0;i<256;++i)b.put8(i,static_cast<std::uint8_t>(i));
    PcMatrixStack s{b,64,1,4};pc_matrix_push(s);
    require(s.depth==2&&s.current_offset==128,"push metadata");
    for(unsigned k=0;k<64;++k)require(b.u8(64+k)==b.u8(128+k),"push matrix copy");
    b.put32(128,0xdeadbeef);pc_matrix_pop(s);
    require(s.depth==1&&s.current_offset==64&&b.u32(128)==0xdeadbeef,"pop must retain temporary slot");
    s.capacity=2;pc_matrix_push(s);require(s.depth==2&&s.current_offset==64,"overflowing push");
    pc_matrix_pop(s);require(s.depth==1&&s.current_offset==0,"overflow and pop are asymmetric");
    s.depth=0;pc_matrix_pop(s);require(s.depth==-1&&s.current_offset==0,"underflowing pop");
    pc_matrix_push(s);require(s.depth==0&&s.current_offset==64,"push after underflow");
    s.current_offset=192;s.depth=3;s.capacity=10;const auto before=memory;
    rejects([&]{pc_matrix_push(s);});require(s.current_offset==192&&s.depth==3&&memory==before,"out-of-view push published changes");
    rejects([&]{pc_matrix_load(s,b.sub(0,63));});require(memory==before,"short load published changes");
    s.current_offset=-64;rejects([&]{s.current();});
    s.current_offset=64;const auto repeated=b.u32(60);pc_matrix_load(s,b.sub(60,64));
    for(unsigned k=0;k<64;k+=4)require(b.u32(64+k)==repeated,"forward overlapping copy semantics");
}
void test_world_contracts(){
    for(unsigned scenario=0;scenario<16;++scenario)for(unsigned opts=0;opts<8;++opts){
        auto f=make_course_world_fixture(scenario*8+opts,5);const auto before=f.image;
        auto b=f.bytes();const auto px=b.u32(0x4600),pz=b.u32(0x4608);
        const auto ret=run_course_world_native(f,5);
        const bool hit=scenario<=3||scenario==6||scenario==11||scenario==12||scenario==13||scenario==14;
        require(b.u32(0x4600)==px&&b.u32(0x4608)==pz,"query changed world X/Z");
        const auto kind=!hit||scenario==11?1u:scenario==12?256u:2u;
        if(opts&1)require(b.u32(0x4620)==(hit?0u:0xffffffffu),"polygon output");
        else require(b.u32(0x4620)==0xdeadbeef,"null polygon output written");
        if(opts&4)require(b.u32(0x4628)==kind,"classification output");
        else require(b.u32(0x4628)==0xabcddcba,"null kind output written");
        require(b.u32(0x4624)==((opts&2)&&hit&&scenario!=11&&scenario!=12?0u:0xcafebabeu),"special output overwrite rule");
        require(b.u32(0x4540)==(hit?15u:14u),"prediction cursor rule");
        if(!hit)require(b.f32(0x4604)==-0.1f,"no-hit sentinel");
        if(scenario>=1&&scenario<=3)require(ret==1&&b.f32(0x4604)==-7.f,"secondary world transform");
        if(scenario==13)require(ret==3&&b.f32(0x4604)==-7.f,"nonzero predicted type");
        if(scenario==14)require(b.i32(0x4550)==64&&b.i32(0x4554)==2,"query overflow/pop effect");
        (void)before;
    }
    for(unsigned scenario=7;scenario<=9;++scenario){
        auto f=make_course_world_fixture(scenario*8+7,6);run_course_world_native(f,6);auto b=f.bytes();
        require(b.f32(0x4604)==3.f&&b.u32(0x4628)==2u,"closed wrapper offset retry");
    }
    auto f=make_course_world_fixture(11*8+7,6);run_course_world_native(f,6);
    require(f.bytes().u32(0x4540)==2u,"four retries must preserve prediction history");
}
void test_native_views(){
    for(unsigned fault=0;fault<8;++fault){
        auto f=make_course_world_fixture(7,5);auto tables=f.tables();auto s=f.matrix();auto p=f.prediction();
        switch(fault){
        case 0:tables.grids[0]=Bytes(nullptr,0);break;
        case 1:tables.transforms[0]=f.bytes().sub(0x4400,63);break;
        case 2:tables.courses[0].polygons=Bytes(nullptr,0);break;
        case 3:tables.courses[0].area_lists=f.bytes().sub(0xc00,2);break;
        case 4:tables.courses[0].kinds=Bytes(nullptr,0);break;
        case 5:tables.courses[0].load_type=3;break;
        case 6:p.easy=4;break;
        case 7:s.current_offset=std::numeric_limits<std::ptrdiff_t>::max()-128;break;
        }
        CourseWorldQuery q{tables,s,p};CourseProbe point{5,9,5};std::uint32_t index=77,special=88,kind=99;
        const auto memory=f.image;const auto depth=s.depth;const auto offset=s.current_offset;const auto pred=p;
        rejects([&]{get_y_position_prog(q,0x400,point,&index,&special,&kind);});
        require(f.image==memory&&s.depth==depth&&s.current_offset==offset,"native rejection matrix rollback");
        require(p.recent==pred.recent&&p.cursor==pred.cursor&&p.easy==pred.easy,"native rejection prediction rollback");
        require(point.x==5&&point.y==9&&point.z==5&&index==77&&special==88&&kind==99,"native rejection output rollback");
    }
}
}
int main(){try{test_matrix();test_world_contracts();test_native_views();std::cout<<"matrix/world boundaries and explicit views passed\n";return 0;}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
