#include "metal_display.hpp"
#include <cstdio>
#include <vector>
int main(){
    outrun::mac::MetalDisplay d;std::string error;
    if(!d.open(8,8,false,error)){std::fprintf(stderr,"%s\n",error.c_str());return 1;}
    // Four quadrants test channel order and the D3D top-left image origin.
    std::vector<std::uint8_t> pixels(4*4*4);
    for(unsigned y=0;y<4;++y)for(unsigned x=0;x<4;++x){
        auto k=(y*4+x)*4;pixels[k]=x<2?255:0;pixels[k+1]=y<2?255:0;pixels[k+2]=x>=2?255:0;pixels[k+3]=255;
    }
    if(!d.present(pixels.data(),4,4,error)){std::fprintf(stderr,"%s\n",error.c_str());return 1;}
    std::vector<std::uint8_t> actual;
    if(!d.readback(actual,error))return 1;
    for(unsigned y=0;y<8;++y)for(unsigned x=0;x<8;++x){
        auto a=(y*8+x)*4,b=((y/2)*4+x/2)*4;
        if(actual[a]!=pixels[b+2]||actual[a+1]!=pixels[b+1]||actual[a+2]!=pixels[b]||actual[a+3]!=255){std::fprintf(stderr,"pixel mismatch %u,%u\n",x,y);return 1;}
    }
    // A 2:1 source occupies the middle four rows, leaving opaque black bars.
    std::vector<std::uint8_t> wide(4*2*4,255);
    if(!d.present(wide.data(),4,2,error)||!d.readback(actual,error))return 1;
    for(unsigned y=0;y<8;++y)for(unsigned x=0;x<8;++x){
        const auto k=(y*8+x)*4;const unsigned expected=y>=2&&y<6?255:0;
        if(actual[k]!=expected||actual[k+1]!=expected||actual[k+2]!=expected||actual[k+3]!=255){std::fprintf(stderr,"letterbox mismatch %u,%u\n",x,y);return 1;}
    }
    if(d.present(nullptr,4,4,error))return 1;
    std::printf("Metal RGBA upload / shader / BGRA readback verified on %s\n",d.device_name().c_str());return 0;
}
