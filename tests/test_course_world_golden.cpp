#include "support/course_world_fixture.hpp"
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
using namespace outrun::testing;
namespace {
void read_exact(std::ifstream& f,void* dst,std::size_t n){if(!f.read(static_cast<char*>(dst),static_cast<std::streamsize>(n)))throw std::runtime_error("truncated world corpus");}
std::uint32_t read_u32(std::ifstream& f){unsigned char b[4];read_exact(f,b,4);return std::uint32_t(b[0])|(std::uint32_t(b[1])<<8)|(std::uint32_t(b[2])<<16)|(std::uint32_t(b[3])<<24);}
struct Control {
#if defined(__i386__) || defined(__x86_64__)
    unsigned short old{},selected{};
    explicit Control(unsigned short value):selected(value){__asm__ volatile("fnstcw %0":"=m"(old));__asm__ volatile("fldcw %0"::"m"(selected));}
    ~Control(){__asm__ volatile("fldcw %0"::"m"(old));}
#else
    explicit Control(unsigned short){throw std::runtime_error("this x87-specific corpus runner has not been validated on ARM64");}
#endif
};
}
int main(int argc,char** argv){try{
    if(argc!=2)throw std::runtime_error("usage: test_course_world_golden FILE");
    std::ifstream in(argv[1],std::ios::binary);if(!in)throw std::runtime_error("cannot open world corpus");
    char magic[8];read_exact(in,magic,8);if(std::memcmp(magic,"OR2W016\0",8))throw std::runtime_error("world corpus magic");
    if(read_u32(in)!=1)throw std::runtime_error("unsupported world corpus version");
    const auto cw=read_u32(in);if(cw!=0x37f&&cw!=0x27f)throw std::runtime_error("unsupported world corpus x87 precision");
    const auto count=read_u32(in);if(!count||count>100000u)throw std::runtime_error("invalid/incomplete world corpus count");
    Control control(static_cast<unsigned short>(cw));
    for(unsigned record=0;record<count;++record){
        const auto id=read_u32(in),test_case=read_u32(in),expected_return=read_u32(in);
        if(id>=7)throw std::runtime_error("unsupported world corpus routine id");
        CourseWorldFixture fixture;std::array<std::uint8_t,world_image_size> expected{};
        read_exact(in,fixture.image.data(),fixture.image.size());read_exact(in,expected.data(),expected.size());fixture.rebuild_grids();
        const auto got=run_course_world_native(fixture,id);
        if(got!=expected_return)throw std::runtime_error("return mismatch id="+std::to_string(id)+" case="+std::to_string(test_case));
        if(fixture.image!=expected){
            std::size_t at=0;while(fixture.image[at]==expected[at])++at;
            throw std::runtime_error("memory mismatch id="+std::to_string(id)+" case="+std::to_string(test_case)+" offset="+std::to_string(at));
        }
    }
    if(in.peek()!=std::char_traits<char>::eof())throw std::runtime_error("trailing world corpus data");
    std::cout<<count<<" original-x86 world records passed; entire "<<world_image_size<<"-byte arena compared\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
