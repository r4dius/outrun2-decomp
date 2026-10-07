#include "support/ground_fixture.hpp"
#include <fstream>
#include <iostream>
#include <stdexcept>
using namespace outrun::testing;
namespace {
void exact(std::ifstream& f,void* dst,std::size_t n){if(!f.read(static_cast<char*>(dst),static_cast<std::streamsize>(n)))throw std::runtime_error("truncated ground corpus");}
std::uint32_t u32(std::ifstream& f){unsigned char b[4];exact(f,b,4);return std::uint32_t(b[0])|(std::uint32_t(b[1])<<8)|(std::uint32_t(b[2])<<16)|(std::uint32_t(b[3])<<24);}
struct Control {
#if defined(__i386__) || defined(__x86_64__)
    unsigned short old{},selected{};
    explicit Control(unsigned short cw):selected(cw){__asm__ volatile("fnstcw %0":"=m"(old));__asm__ volatile("fldcw %0"::"m"(selected));}
    ~Control(){__asm__ volatile("fldcw %0"::"m"(old));}
#else
    explicit Control(unsigned short){throw std::runtime_error("x87 golden precision is not ARM64 validation");}
#endif
};
}
int main(int argc,char** argv){try{
    if(argc!=2)throw std::runtime_error("usage: test_ground_golden FILE");
    std::ifstream in(argv[1],std::ios::binary);if(!in)throw std::runtime_error("cannot open ground corpus");
    char magic[8];exact(in,magic,8);if(std::memcmp(magic,"OR2G016\0",8)||u32(in)!=1)throw std::runtime_error("ground corpus header");
    const auto cw=u32(in),count=u32(in);if((cw!=0x37f&&cw!=0x27f)||!count||count>10000u)throw std::runtime_error("invalid/incomplete ground corpus");
    Control control(static_cast<unsigned short>(cw));unsigned stages=0;
    for(unsigned record=0;record<count;++record){
        const auto which=u32(in),id=u32(in),steps=u32(in);
        if(which>1u||steps!=(which?10u:1u))throw std::runtime_error("invalid ground corpus sequence descriptor");
        GroundFixture f;exact(in,f.world.image.data(),f.world.image.size());exact(in,f.car.data(),f.car.size());f.rebuild_grids();
        std::array<std::uint8_t,world_image_size> expected_world{};std::array<std::uint8_t,ground_car_size> expected_car{};
        for(unsigned stage=0;stage<steps;++stage){
            exact(in,expected_world.data(),expected_world.size());exact(in,expected_car.data(),expected_car.size());
            run_ground_native(f,stage%5u);++stages;
            // Never copy an expected after-image back into the native state.
            if(f.world.image!=expected_world||f.car!=expected_car)throw std::runtime_error("ground memory mismatch case="+std::to_string(id)+" stage="+std::to_string(stage));
        }
    }
    if(in.peek()!=std::char_traits<char>::eof())throw std::runtime_error("trailing ground corpus data");
    std::cout<<count<<" original-x86 sequences, "<<stages<<" independently compared stages; no resynchronization\n";return 0;
}catch(const std::exception& ex){std::cerr<<ex.what()<<'\n';return 1;}}
