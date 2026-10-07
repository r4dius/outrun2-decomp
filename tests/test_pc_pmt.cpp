#include "pc_pmt.hpp"
#include <cmath>
#include <cstring>
#include <functional>
#include <iostream>
#include <limits>
#include <stdexcept>
using namespace outrun::assets;
namespace {
using Bytes=std::vector<std::uint8_t>;
std::size_t assertions=0;
void expect(bool value,const char* message) {
    ++assertions;
    if(!value)throw std::runtime_error(message);
}
void reject(const std::function<void()>& action,const char* message) {
    try {action();} catch(const FormatError&) {expect(true,message);return;}
    expect(false,message);
}
void put(Bytes& b,std::size_t off,std::uint32_t value) {
    for(unsigned i=0;i<4;++i)b.at(off+i)=static_cast<std::uint8_t>(value>>(i*8));
}
void fp(Bytes& b,std::size_t off,float value) {
    std::uint32_t raw;std::memcpy(&raw,&value,4);put(b,off,raw);
}
void fields(Bytes& b,std::size_t off,std::initializer_list<std::uint32_t> values) {
    for(auto value:values){put(b,off,value);off+=4;}
}
Bytes dds(unsigned faces=1) {
    Bytes b(128+8*faces);
    put(b,0,0x20534444);put(b,4,124);put(b,8,0x81007);
    put(b,12,4);put(b,16,4);put(b,20,8);put(b,28,1);
    put(b,76,32);put(b,80,4);put(b,84,0x31545844);put(b,108,0x1000);
    if(faces==6)put(b,112,0xfc00); // Actual PC quirk: no generic 0x200 cube bit.
    b[128]=0;b[129]=0xf8; // Entire 4x4 block is red, entirely synthetic.
    return b;
}
Bytes fixture(bool relative=false) {
    // Deliberately tiny synthetic PMT; no commercial bytes in this test.
    Bytes b(16+640+272);
    fields(b,0,{1,1,640,272});fields(b,16,{0,1,1,0,0,0});
    fields(b,16+24,{0,516,520,0,600,128,128,180,236,244,264,296,312,356,444});
    fields(b,16+92,{0x30525058,168,32,0,0,0,0,0});
    fields(b,16+128,{180,600,236,244,264,296,312,356,444,1,1,1,1});
    if(relative)for(unsigned i=0;i<9;++i){auto v=ByteView(b).u32(16+128+i*4);put(b,16+128+i*4,v-128);}
    fields(b,16+244,{0,0,0xffffffff,1,0}); // Empty second interval may use sentinel.
    fields(b,16+264,{0,0,0,1});
    fields(b,16+296,{6,0,2,999}); // Stored polygon count is not authoritative.
    fields(b,16+312,{1,0,0,0,0,0,8,128,0x112,32,0});
    put(b,16+356,0);for(unsigned i=0;i<4;++i)put(b,16+356+24+i*20,0xffffffff);
    put(b,16+356+24,0);
    for(unsigned i=0;i<4;++i)fp(b,16+444+i*4,1.0f);
    put(b,16+516,536);put(b,16+520,548);
    put(b,16+536+4,600);put(b,16+548+4,144);
    for(unsigned i=0;i<4;++i)b[16+600+i*2]=static_cast<std::uint8_t>(i);
    const auto texture=dds();std::copy(texture.begin(),texture.end(),b.begin()+16+640);
    for(unsigned i=0;i<4;++i) {
        const auto off=16+640+144+i*32;
        fp(b,off,static_cast<float>(i&1));fp(b,off+4,static_cast<float>(i>>1));
        fp(b,off+20,1);fp(b,off+24,static_cast<float>(i&1));fp(b,off+28,static_cast<float>(i>>1));
    }
    return b;
}
void mutation_rejected(std::size_t off,std::uint32_t v,const char* message) {
    auto b=fixture();put(b,off,v);reject([&]{inspect_pc_pmt(ByteView(b));},message);
}
void test_dds() {
    auto b=dds();auto d=inspect_dds(ByteView(b));
    expect(d.width==4 && d.height==4 && d.mips==1 && d.faces==1 && d.file_bytes==136,"DDS DXT1 layout");
    b=dds(6);d=inspect_dds(ByteView(b));expect(d.faces==6 && d.file_bytes==176,"Cube face bits without generic flag");
    put(b,112,0xfe00);expect(inspect_dds(ByteView(b)).faces==6,"Regular cubemap");
    b=dds();put(b,112,0x600);expect(inspect_dds(ByteView(b)).faces==1,"Single cube face");
    put(b,112,0x200);reject([&]{inspect_dds(ByteView(b));},"Cube without declared face must fail");
    b=dds();put(b,112,0x200000);reject([&]{inspect_dds(ByteView(b));},"Volume not implemented");
    b=dds();put(b,84,0x30315844);reject([&]{inspect_dds(ByteView(b));},"DX10 not implemented");
    b=dds();put(b,16,0);reject([&]{inspect_dds(ByteView(b));},"Zero width");
    b=dds();put(b,12,16385);reject([&]{inspect_dds(ByteView(b));},"Huge height");
    b=dds();put(b,28,4);reject([&]{inspect_dds(ByteView(b));},"Excess mips");
    b=dds();put(b,28,3);b.resize(152);expect(inspect_dds(ByteView(b)).file_bytes==152,"DXT1 full mip chain");
    b.resize(151);reject([&]{inspect_dds(ByteView(b));},"Truncated last mip");
    for(auto cc:{0x32545844u,0x33545844u,0x34545844u,0x35545844u}) {
        b=dds();b.resize(144);put(b,84,cc);expect(inspect_dds(ByteView(b)).file_bytes==144,"16-byte DXT block");
    }
    b=dds();b.resize(192);put(b,80,0x40);put(b,84,0);put(b,88,32);put(b,8,0x100f);put(b,20,16);
    expect(inspect_dds(ByteView(b)).file_bytes==192,"Raw RGBA32 with pitch");
    put(b,20,15);reject([&]{inspect_dds(ByteView(b));},"Short pitch");
    put(b,20,0xffffffff);reject([&]{inspect_dds(ByteView(b));},"Huge pitch bounded by payload");
}
void test_topology() {
    std::uint64_t removed=99;
    expect(triangulate({0,1,2,3},6,&removed)==std::vector<std::uint32_t>({0,1,2,2,1,3}) && removed==0,"Strip alternating winding");
    expect(triangulate({0,1,1,2,3},6,&removed)==std::vector<std::uint32_t>({1,2,3}) && removed==2,"Strip parity survives skipped degenerates");
    expect(triangulate({0,1,2,4,4,5},5,&removed)==std::vector<std::uint32_t>({0,1,2}) && removed==1,"Triangle list degenerates");
    expect(triangulate({0,1,2,3},8)==std::vector<std::uint32_t>({0,1,2,0,2,3}),"Xbox quad list");
    reject([]{triangulate({0,1},6);},"Short strip");
    reject([]{triangulate({0,1,2,3},5);},"Partial triangle list");
    reject([]{triangulate({0,1,2},8);},"Partial quad list");
    reject([]{triangulate({0,1,2},4);},"Unsupported topology");
}
void test_pmt() {
    for(bool relative:{false,true}) {
        auto b=fixture(relative);const auto p=inspect_pc_pmt(ByteView(b));
        expect(p.system.size==640 && p.video.size==272,"Segment boundaries");
        expect(p.objects.size()==1 && p.objects[0].header_relative==relative,"Both offset conventions");
        expect(p.textures.size()==1 && p.textures[0].is_dds,"Indexed DDS texture");
        expect(p.checked_index_references==4,"Checks the strip's four indices");
        auto meshes=decode_static_object(p,0);
        expect(meshes.size()==1 && meshes[0].vertices.size()==4 && meshes[0].indices.size()==6,"Synthetic square export");
        expect(meshes[0].vertices[3].position[0]==1 && meshes[0].vertices[3].uv[1]==1,"Position and UV read");
        expect(meshes[0].vertices[0].normal[2]==1,"Normal read");
        reject([&]{decode_static_object(p,1);},"Object bounds");
    }
    mutation_rejected(8,639,"Wrong segment length");
    mutation_rejected(16+4,2,"Repeated count mismatch");
    mutation_rejected(16+92,0,"Bad XPR signature");
    mutation_rejected(16+92+8,31,"Short XPR header");
    mutation_rejected(16+92+4,9999,"XPR data out of video range");
    mutation_rejected(16+92+16,9999,"Texture start out of range");
    mutation_rejected(16+24+5*4,0xfffffffc,"Object header pointer bounds");
    mutation_rejected(16+128,181,"Irreconcilable header convention");
    mutation_rejected(16+128+9*4,0xffffffff,"Signed count rejection");
    mutation_rejected(16+312,5,"Too many streams");
    mutation_rejected(16+312+36,0,"Zero stride");
    mutation_rejected(16+312+24,7,"Odd index bytes");
    mutation_rejected(16+312+28,127,"Partial vertex");
    mutation_rejected(16+516,0xfffffff8,"Index resource pointer");
    mutation_rejected(16+536+4,639,"Index range");
    mutation_rejected(16+548+4,270,"Vertex range");
    mutation_rejected(16+356,1,"Color index out of range");
    mutation_rejected(16+356,0xfffffffe,"Invalid negative color");
    mutation_rejected(16+356+24,1,"Texture index out of range");
    mutation_rejected(16+356+24,0xfffffffe,"Invalid negative texture");
    mutation_rejected(16+444,0x7fc00000,"NaN material");
    mutation_rejected(16+264+4,1,"Invalid material index");
    mutation_rejected(16+264+12,100,"Primitive table range");
    mutation_rejected(16+244,1,"Invalid format index");
    mutation_rejected(16+244+12,2,"Material interval range");
    mutation_rejected(16+296,4,"Invalid primitive kind");
    mutation_rejected(16+296+8,10,"Index span out of buffer");
    mutation_rejected(16+264,1,"Base vertex must be added before validation");
    mutation_rejected(16+600,0xffff,"Index beyond vertex count");
    {
        auto b=fixture();put(b,16+356,0xffffffff);put(b,16+356+24,0xffffffff);
        auto p=inspect_pc_pmt(ByteView(b));expect(p.objects[0].materials[0].diffuse[0]==1,"Optional absent color");
    }
    {
        auto b=fixture();put(b,16+640,0x12345678);
        expect(!inspect_pc_pmt(ByteView(b)).textures[0].is_dds,"Raw XPR must stay explicitly undecoded");
    }
    for(auto off:{16+312+40,16+312+32}) {
        auto b=fixture();put(b,static_cast<std::size_t>(off),3);auto p=inspect_pc_pmt(ByteView(b));
        reject([&]{decode_static_object(p,0);},"Unsupported shader/FVF is not a successful export");
    }
    {
        auto b=fixture();put(b,16+640+144,0x7f800000);auto p=inspect_pc_pmt(ByteView(b));
        reject([&]{decode_static_object(p,0);},"Nonfinite position rejected during geometry decode");
    }
}
}
int main() {
    try {
        test_dds();test_topology();test_pmt();
        const auto logical=assertions;
        const auto b=fixture();
        for(std::size_t n=0;n<b.size();++n)reject([&]{inspect_pc_pmt(ByteView(b.data(),n));},"Every truncated PMT must fail");
        std::uint32_t rng=0x12345678u;std::size_t accepted=0,rejected=0;
        for(unsigned n=0;n<4096;++n) {
            auto m=b;rng=rng*1664525u+1013904223u;const auto pos=rng%m.size();
            rng=rng*1664525u+1013904223u;m[pos]^=static_cast<std::uint8_t>((rng>>24)|1);
            try {auto p=inspect_pc_pmt(ByteView(m));decode_static_object(p,0);++accepted;}
            catch(const FormatError&){++rejected;}
        }
        std::cout<<logical<<" targeted PMT/DDS/topology checks passed; "<<b.size()
                 <<" exhaustive truncations rejected; 4096 deterministic byte mutations handled ("
                 <<accepted<<" accepted, "<<rejected<<" rejected). Not an equivalence proof.\n";
        return 0;
    } catch(const std::exception& e){std::cerr<<"FAIL: "<<e.what()<<'\n';return 1;}
}
