#include "support/course_query_fixture.hpp"
#include <fstream>
#include <iostream>
#include <cstring>
using namespace outrun::testing;
std::uint32_t read32(std::istream& s){unsigned char b[4];if(!s.read(reinterpret_cast<char*>(b),4))throw std::runtime_error("truncated query fixture");return std::uint32_t(b[0])|(std::uint32_t(b[1])<<8)|(std::uint32_t(b[2])<<16)|(std::uint32_t(b[3])<<24);}
int main(int argc,char** argv){try{
    if(argc!=2)throw std::runtime_error("Usage: test_course_query_golden fixture.bin");
    std::ifstream s(argv[1],std::ios::binary);char magic[8]{};
    if(!s.read(magic,8)||std::memcmp(magic,"OR2Q015\0",8)!=0||read32(s)!=1)throw std::runtime_error("invalid query fixture header");
    const auto cw=read32(s),n=read32(s);if((cw!=0x037f&&cw!=0x027f)||n==0||n>100000)throw std::runtime_error("invalid control word or record count");
    unsigned failures=0;
    for(unsigned r=0;r<n;++r){const auto id=read32(s),index=read32(s),expected_return=read32(s);CourseQueryFixture input,expected;
        if(!s.read(reinterpret_cast<char*>(input.image.data()),4096)||!s.read(reinterpret_cast<char*>(expected.image.data()),4096))throw std::runtime_error("truncated query snapshot");
        const auto actual_return=run_course_query_native(input,id);
        if(expected_return!=actual_return||input.image!=expected.image){++failures;if(failures<5)std::cerr<<"query snapshot mismatch id="<<id<<" input="<<index<<'\n';}
    }
    if(s.peek()!=std::char_traits<char>::eof())throw std::runtime_error("query fixture trailing bytes");
    std::cout<<"{\"records\":"<<n<<",\"mismatches\":"<<failures<<",\"guest_x87_control\":"<<cw<<"}\n";return failures?1:0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 2;}}
