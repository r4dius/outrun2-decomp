#include "support/wall_geometry_fixture.hpp"
#include <fstream>
#include <iostream>
#include <string>
using namespace outrun::testing;
namespace {
void exact(std::ifstream& f,void* p,std::size_t n){if(!f.read(static_cast<char*>(p),static_cast<std::streamsize>(n)))throw std::runtime_error("truncated wall corpus");}
std::uint32_t u32(std::ifstream& f){unsigned char p[4];exact(f,p,4);return std::uint32_t(p[0])|(std::uint32_t(p[1])<<8)|(std::uint32_t(p[2])<<16)|(std::uint32_t(p[3])<<24);}
struct Control {
#if defined(__i386__) || defined(__x86_64__)
    unsigned short saved{},selected{};
    explicit Control(unsigned short cw):selected(cw){__asm__ volatile("fnstcw %0":"=m"(saved));__asm__ volatile("fldcw %0"::"m"(selected));}
    ~Control(){__asm__ volatile("fldcw %0"::"m"(saved));}
#else
    explicit Control(unsigned short){throw std::runtime_error("x87 corpus is not an ARM64 equivalence proof; this replay gate requires x86");}
#endif
};
}
int main(int argc,char** argv){try{
    if(argc!=2)throw std::runtime_error("usage: test_wall_geometry_golden FILE");
    std::ifstream in(argv[1],std::ios::binary);if(!in)throw std::runtime_error("cannot open wall corpus");
    char magic[8];exact(in,magic,8);if(std::memcmp(magic,"OR2L017\0",8))throw std::runtime_error("invalid wall corpus magic");
    if(u32(in)!=1)throw std::runtime_error("unsupported wall version");
    const auto cw=u32(in),count=u32(in),size=u32(in);
    if(cw!=0x37f&&cw!=0x27f)throw std::runtime_error("invalid x87 precision");
    if(!count||count>100000||size!=wall_image_size)throw std::runtime_error("invalid wall corpus size/count");
    Control control(static_cast<unsigned short>(cw));unsigned compared=0;
    for(unsigned record=0;record<count;++record){
        const auto id=u32(in),test_case=u32(in),stages=u32(in);
        if(id>=7||stages!=(id==6?8u:1u))throw std::runtime_error("invalid wall routine/sequence length");
        WallGeometryFixture f;exact(in,f.image.data(),f.image.size());
        for(unsigned stage=0;stage<stages;++stage){
            const auto expected_return=u32(in);std::array<std::uint8_t,wall_image_size> expected{};exact(in,expected.data(),expected.size());
            // Expected snapshots are NEVER copied into the ongoing native state.
            const auto ret=run_wall_geometry_native(f,id==6?wall_sequence[stage]:id);
            if(ret!=expected_return)throw std::runtime_error("wall return mismatch case="+std::to_string(test_case)+" stage="+std::to_string(stage));
            if(f.image!=expected){std::size_t off=0;while(f.image[off]==expected[off])++off;
                throw std::runtime_error("wall memory mismatch case="+std::to_string(test_case)+" stage="+std::to_string(stage)+" offset="+std::to_string(off));}
            ++compared;
        }
    }
    if(in.peek()!=std::char_traits<char>::eof())throw std::runtime_error("trailing wall corpus data");
    std::cout<<count<<" original-x86 wall sequences; "<<compared<<" full-arena comparisons without resynchronization\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
