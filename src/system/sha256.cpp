#include "system/sha256.hpp"
#include <cstring>
namespace outrun::platform {
namespace {
constexpr std::uint32_t K[64]={
    0x428a2f98u,0x71374491u,0xb5c0fbcfu,0xe9b5dba5u,0x3956c25bu,0x59f111f1u,0x923f82a4u,0xab1c5ed5u,
    0xd807aa98u,0x12835b01u,0x243185beu,0x550c7dc3u,0x72be5d74u,0x80deb1feu,0x9bdc06a7u,0xc19bf174u,
    0xe49b69c1u,0xefbe4786u,0x0fc19dc6u,0x240ca1ccu,0x2de92c6fu,0x4a7484aau,0x5cb0a9dcu,0x76f988dau,
    0x983e5152u,0xa831c66du,0xb00327c8u,0xbf597fc7u,0xc6e00bf3u,0xd5a79147u,0x06ca6351u,0x14292967u,
    0x27b70a85u,0x2e1b2138u,0x4d2c6dfcu,0x53380d13u,0x650a7354u,0x766a0abbu,0x81c2c92eu,0x92722c85u,
    0xa2bfe8a1u,0xa81a664bu,0xc24b8b70u,0xc76c51a3u,0xd192e819u,0xd6990624u,0xf40e3585u,0x106aa070u,
    0x19a4c116u,0x1e376c08u,0x2748774cu,0x34b0bcb5u,0x391c0cb3u,0x4ed8aa4au,0x5b9cca4fu,0x682e6ff3u,
    0x748f82eeu,0x78a5636fu,0x84c87814u,0x8cc70208u,0x90befffau,0xa4506cebu,0xbef9a3f7u,0xc67178f2u};
inline std::uint32_t ror(std::uint32_t v,unsigned n){return (v>>n)|(v<<(32u-n));}
}
Sha256::Sha256():h_{0x6a09e667u,0xbb67ae85u,0x3c6ef372u,0xa54ff53au,0x510e527fu,0x9b05688cu,0x1f83d9abu,0x5be0cd19u}{}
void Sha256::block(const std::uint8_t* p){
    std::uint32_t w[64];
    for(int i=0;i<16;++i)w[i]=std::uint32_t(p[i*4])<<24|std::uint32_t(p[i*4+1])<<16|std::uint32_t(p[i*4+2])<<8|p[i*4+3];
    for(int i=16;i<64;++i){
        const auto s0=ror(w[i-15],7)^ror(w[i-15],18)^(w[i-15]>>3),s1=ror(w[i-2],17)^ror(w[i-2],19)^(w[i-2]>>10);
        w[i]=w[i-16]+s0+w[i-7]+s1;
    }
    std::uint32_t a=h_[0],b=h_[1],c=h_[2],d=h_[3],e=h_[4],f=h_[5],g=h_[6],h=h_[7];
    for(int i=0;i<64;++i){
        const auto t1=h+(ror(e,6)^ror(e,11)^ror(e,25))+((e&f)^(~e&g))+K[i]+w[i];
        const auto t2=(ror(a,2)^ror(a,13)^ror(a,22))+((a&b)^(a&c)^(b&c));
        h=g;g=f;f=e;e=d+t1;d=c;c=b;b=a;a=t1+t2;
    }
    h_[0]+=a;h_[1]+=b;h_[2]+=c;h_[3]+=d;h_[4]+=e;h_[5]+=f;h_[6]+=g;h_[7]+=h;
}
void Sha256::update(const std::uint8_t* data,std::size_t size){
    total_+=size;
    if(used_){
        const auto n=size<64-used_?size:64-used_;
        std::memcpy(buffer_+used_,data,n);used_+=n;data+=n;size-=n;
        if(used_<64)return;
        block(buffer_);used_=0;
    }
    for(;size>=64;data+=64,size-=64)block(data);
    if(size){std::memcpy(buffer_,data,size);used_=size;}
}
Sha256Digest Sha256::finish(){
    const std::uint64_t bits=total_*8u;
    const std::uint8_t one=0x80u,zero=0;
    update(&one,1);
    while(used_!=56)update(&zero,1);
    std::uint8_t length[8];
    for(int i=0;i<8;++i)length[i]=std::uint8_t(bits>>(56-8*i));
    update(length,8);
    Sha256Digest out{};
    for(int i=0;i<8;++i)for(int k=0;k<4;++k)out[i*4+k]=std::uint8_t(h_[i]>>(24-8*k));
    return out;
}
Sha256Digest sha256(const std::uint8_t* data,std::size_t size){Sha256 s;s.update(data,size);return s.finish();}
std::string sha256_hex(const Sha256Digest& digest){
    static const char hex[]="0123456789abcdef";std::string s;
    for(auto b:digest){s+=hex[b>>4];s+=hex[b&15];}
    return s;
}
}
