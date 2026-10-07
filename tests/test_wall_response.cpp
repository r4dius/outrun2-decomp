#include "support/wall_response_fixture.hpp"
#include <iostream>
#include <limits>
#include <string>
using namespace outrun::testing;
namespace {
unsigned checks=0;
void require(bool v,const char* text){++checks;if(!v)throw std::runtime_error(text);}
template<class Fn>void rejects_unchanged(WallResponseFixture& f,Fn fn,const char* text){
    const auto old=f.image;bool threw=false;try{fn();}catch(const std::exception&){threw=true;}
    require(threw,text);require(f.image==old,"rejected call changed memory");
}
}
int main(){try{
    for(unsigned i=0;i<2048;++i){
        auto f=make_wall_response_fixture(i,9),p=f;
        const auto before=f.image;auto b=f.bytes();const auto c=f.context();
        auto s=f.stack();calc_friction_status(b.sub(0,0x1000),b.sub(0x1000,0x800),b.f32(0x4020),s,c);
        auto pb=p.bytes();for(unsigned k=0;k<4;++k)pb.put32(0x1c14+k*0x78,0xfffffffdu-k);
        auto ps=p.stack();
        calc_friction_status(pb.sub(0,0x1000),pb.sub(0x1000,0x800),pb.f32(0x4020),ps,p.context());
        for(unsigned k=0;k<4;++k){require(pb.u32(0x1c14+k*0x78)==(0xfffffffdu-k),"serialized descriptor modified");pb.put32(0x1c14+k*0x78,b.u32(0x1c14+k*0x78));}
        require(p.image==f.image,"native followed serialized descriptor pointer");
        require(s.depth==int(i%8)&&s.current_offset==128,"friction changed stack depth");
        require((b.u32(0x1244)&8)!=0,"friction bit missing");
        require(!std::memcmp(before.data()+0x1040,f.image.data()+0x1040,16),"work translation/last word changed");
        require(!std::memcmp(before.data()+0x1880+64,f.image.data()+0x1880+64,64),"unused stack slot changed");
        for(unsigned id=0;id<11;++id){auto x=make_wall_response_fixture(i,id);run_wall_response_native(x,id);}
    }
    auto f=make_wall_response_fixture(13,9);auto b=f.bytes();b.put32(0x68,100);b.puti(0x400c,4);b.put32(0x5c,0);b.puti(0x1e00,0);b.put16(0x64,100);
    auto c=f.context();
    for(unsigned table:{0x2000u,0x3000u}){
        for(unsigned k=0;k<16;++k){b.put16(table+k*4,10);b.put16(table+k*4+2,9);}
        b.put16(table+60,100);b.put16(table+62,200);
    }
    for(unsigned p:{99u,100u,200u,201u,32767u,32768u,65535u}){
        const bool expected=p>=100&&p<=200;
        require(check_crush_entrapment_length(b.sub(0,0x1000),std::uint16_t(p),c)==expected,"last crush range / unsigned position");
        require(check_friction_entrapment_length(b.sub(0,0x1000),std::uint16_t(p),c)==expected,"last friction range / unsigned position");
    }
    b.put16(0x2000,0xffff);require(!check_crush_entrapment_length(b.sub(0,0x1000),100,c),"negative sentinel not terminal");
    b.put32(0x5c,1);require(!check_friction_entrapment_length(b.sub(0,0x1000),100,c),"alternate course condition ignored");b.put32(0x5c,0);
    rejects_unchanged(f,[&]{auto bad=c;bad.stages.descriptors.clear();auto s=f.stack();calc_friction_status(b.sub(0,0x1000),b.sub(0x1000,0x800),1,s,bad);},"missing descriptor accepted");
    rejects_unchanged(f,[&]{auto bad=c;bad.friction_ranges=b.sub(0x3000,4);auto s=f.stack();calc_friction_status(b.sub(0,0x1000),b.sub(0x1000,0x800),1,s,bad);},"truncated friction table accepted");
    rejects_unchanged(f,[&]{auto s=f.stack();s.current_offset=1024;calc_friction_status(b.sub(0,0x1000),b.sub(0x1000,0x800),1,s,c);},"bad current matrix accepted");
    rejects_unchanged(f,[&]{auto s=f.stack();pc_matrix_multiply_current(s,b.sub(0x4100,63));},"short multiplication source accepted");
    rejects_unchanged(f,[&]{auto s=f.stack();pc_matrix_store_rotation(s,b.sub(0x4100,43));},"short rotation output accepted");
    // A NaN angle propagates through FSINCOS as on the PC (osage chains, rob_osage_probe); infinities and |x| >= 2^63 are refused.
    for(float v:{std::numeric_limits<float>::infinity(),-std::numeric_limits<float>::infinity(),0x1p63f})
        rejects_unchanged(f,[&]{auto s=f.stack();pc_matrix_rotate_y(s,v);},"out-of-domain FSINCOS accepted");
    for(int mode:{-1,3,2147483647})set_collision_timer(Bytes(nullptr,0),mode,255);
    set_collision_timer(Bytes(nullptr,0),0,1);set_collision_timer(Bytes(nullptr,0),1,0);
    for(int mode:{0,1,2}){b.puti(0xdec,-1);set_collision_timer(b,mode,2);require(b.i32(0xdec)==(mode==2?120:180),"timer threshold");b.puti(0xdec,181);set_collision_timer(b,mode,2);require(b.i32(0xdec)==181,"timer reduced");}
    std::cout<<checks<<" wall-response boundaries, conservation, explicit views and rejection checks passed\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
