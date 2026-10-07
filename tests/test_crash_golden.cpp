#include "support/crash_fixture.hpp"
#include <fstream>
#include <iostream>
#include <string>
using namespace outrun::testing;
namespace {
void exact(std::ifstream& f,void* p,std::size_t n){if(!f.read(static_cast<char*>(p),static_cast<std::streamsize>(n)))throw std::runtime_error("truncated crash corpus");}
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
    if(argc!=2)throw std::runtime_error("usage: test_crash_golden FILE");
    std::ifstream in(argv[1],std::ios::binary);if(!in)throw std::runtime_error("cannot open crash corpus");
    char magic[8];exact(in,magic,8);if(std::memcmp(magic,"OR2R020\0",8))throw std::runtime_error("invalid crash corpus magic");
    if(u32(in)!=2)throw std::runtime_error("unsupported crash version");
    const auto cw=u32(in),count=u32(in),size=u32(in);
    if(cw!=0x37f&&cw!=0x27f)throw std::runtime_error("invalid x87 precision");
    if(!count||count>100000||size!=crash_image_size)throw std::runtime_error("invalid crash corpus size/count");
    Control control(static_cast<unsigned short>(cw));unsigned compared=0;
    for(unsigned record=0;record<count;++record){
        const auto id=u32(in),test_case=u32(in),stages=u32(in);
        if(id>=crash_selector_count||stages!=crash_stage_count(id))throw std::runtime_error("invalid crash routine/sequence length");
        CrashFixture f;exact(in,f.image.data(),f.image.size());
        for(unsigned stage=0;stage<stages;++stage){
            const auto expected_return=u32(in);std::array<std::uint8_t,12> fp{};exact(in,fp.data(),12);std::array<std::uint8_t,crash_image_size> expected{};exact(in,expected.data(),expected.size());
            // Expected snapshots are NEVER copied into the ongoing native state.
            const auto actual=crash_stage_id(id,stage);long double ext=0;
            const auto ret=run_crash_native(f,actual,&ext);
            const bool fp_return=actual==0||actual==4||actual==6||actual==8||actual==9;
            if(fp_return&&std::memcmp(&ext,fp.data(),10))throw std::runtime_error("crash extended return mismatch");
            if(fp[10]||fp[11]||(!fp_return&&std::any_of(fp.begin(),fp.end(),[](auto v){return v!=0;})))throw std::runtime_error("crash fp framing mismatch");
            if(ret!=expected_return)throw std::runtime_error("crash return mismatch case="+std::to_string(test_case)+" stage="+std::to_string(stage));
            if(f.image!=expected){std::size_t off=0;while(f.image[off]==expected[off])++off;
                throw std::runtime_error("crash memory mismatch case="+std::to_string(test_case)+" stage="+std::to_string(stage)+" offset="+std::to_string(off));}
            ++compared;
        }
    }
    if(in.peek()!=std::char_traits<char>::eof())throw std::runtime_error("trailing crash corpus data");
    std::cout<<count<<" original-x86 crash sequences; "<<compared<<" full-arena comparisons without resynchronization\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
