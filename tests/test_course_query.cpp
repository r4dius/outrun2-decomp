#include "support/course_query_fixture.hpp"
#include <iostream>
#include <stdexcept>
using namespace outrun::driving;
using namespace outrun::testing;
namespace {
void require(bool ok,const char* what){if(!ok)throw std::runtime_error(what);}
template<class F>void rejects(F call){bool threw=false;try{call();}catch(const std::out_of_range&){threw=true;}catch(const std::invalid_argument&){threw=true;}require(threw,"invalid view was not rejected");}
}
int main(){try{
    // All sixteen four-neighbor combinations, independently expected from the
    // table layout/discontinuities, not from outputs of the native function.
    for(unsigned mask=0;mask<16;++mask){auto f=make_course_query_fixture(mask);auto b=f.bytes();auto t=f.tables();
        const auto before=f.image;const auto i=b.i32(0xf10);const int cols=int(t.runs.header.i32(12)/3);
        std::int32_t out=0x13579;
        require(coli_get_forward_polygon_number(t,i,out)==bool(mask&1),"forward mask");require(out==((mask&1)?i+cols:0x13579),"forward output");out=0x13579;
        require(coli_get_back_polygon_number(t,i,out)==bool(mask&2),"back mask");require(out==((mask&2)?i-cols:0x13579),"back output");out=0x13579;
        require(coli_get_left_polygon_number(t,i,out)==bool(mask&4),"left mask");require(out==((mask&4)?i-1:0x13579),"left output");out=0x13579;
        require(coli_get_right_polygon_number(t,i,out)==bool(mask&8),"right mask");require(out==((mask&8)?i+1:0x13579),"right output");
        require(f.image==before,"neighbor changed immutable table/canary");
    }
    auto f=make_course_query_fixture(15);auto b=f.bytes();auto t=f.tables();const auto i=b.i32(0xf10),forward=i+3;
    std::int32_t out=12345;
    b.putf(0x400+unsigned(forward)*0x40+0x24+4,0.5f);
    require(coli_get_forward_polygon_number(t,i,out)&&out==forward,"exact 0.5 must contact");
    b.putf(0x400+unsigned(forward)*0x40+0x24+4,std::nextafter(0.5f,1.f));out=12345;
    require(!coli_get_forward_polygon_number(t,i,out)&&out==12345,"above 0.5 must reject without writing");
    b.putf(0x400+unsigned(forward)*0x40+0x24+4,0.f);
    b.putf(0x400+unsigned(forward)*0x40+0x18+4,std::nextafter(0.5f,1.f));
    require(!coli_get_forward_polygon_number(t,i,out)&&out==12345,"second vertex continuity");
    auto invalid=t;invalid.kinds=Bytes(nullptr,0);rejects([&]{coli_get_forward_polygon_number(invalid,i,out);});require(out==12345,"rejected kinds changed output");
    invalid=t;invalid.polygons=b.sub(0x400,0x40);rejects([&]{coli_get_forward_polygon_number(invalid,i,out);});require(out==12345,"rejected polygons changed output");
    invalid=t;invalid.load_type=4;rejects([&]{coli_get_back_polygon_number(invalid,i,out);});require(out==12345,"rejected type changed output");
    rejects([&]{coli_get_back_polygon_number(t,-2,out);});require(out==12345,"invalid negative index changed output");
    invalid=t;invalid.load_type=1;invalid.kinds=Bytes(nullptr,0);require(!coli_get_forward_polygon_number(invalid,-99,out),"disabled forward must not touch tables");
    invalid=t;invalid.runs.present=false;std::int32_t first=17,last=19;
    require(find_secondary_course_run(invalid,1,0,first,last)==0&&first==17&&last==19,"absent secondary header writes");
    invalid=t;invalid.runs.lengths=Bytes(nullptr,0);rejects([&]{find_secondary_course_run(invalid,1,0,first,last);});require(first==17&&last==19,"secondary rejection partial output");
    b.puti(0x14c,3);for(unsigned k=0;k<3;++k)b.put16(0x200+k*2,1);
    require(find_secondary_course_run(t,1,0,first,last)==1&&first==0&&last==0,"PC byte+1 lookahead must not become +2");
    for(unsigned k=0;k<3;++k)b.put16(0x200+k*2,0x101);
    require(find_secondary_course_run(t,0x101,100,first,last)==3&&first==0&&last==2,"unaligned repeating word run");
    b.puti(0x14c,0);require(find_secondary_course_run(t,9,0,first,last)==1&&first==0&&last==0,"PC zero-count present-header behavior");
    // Sequential category (kind 8 -> 0x100 in 0x3b00), including the
    // independent header+8 limit used only by Forward's sequential branch.
    auto seq=make_course_query_fixture(3);auto sb=seq.bytes();auto st=seq.tables();
    for(unsigned k=0;k<3;++k)sb.put8(0x180+k,8);
    out=12345;require(coli_get_forward_polygon_number(st,1,out)&&out==2,"sequential forward");
    require(coli_get_back_polygon_number(st,1,out)&&out==0,"sequential back");
    sb.puti(0x148,1);out=12345;
    require(!coli_get_forward_polygon_number(st,1,out)&&out==12345,"header+8 sequential limit");
    require(coli_get_back_polygon_number(st,1,out)&&out==0,"back must not inherit forward bound");
    // Lateral lookups read kind before the disabled-type test, unlike Forward.
    auto disabled=st;disabled.load_type=1;disabled.kinds=Bytes(nullptr,0);out=12345;
    rejects([&]{coli_get_left_polygon_number(disabled,1,out);});require(out==12345,"lateral rejection modified output");
    // Nearest stacked surface, stable ties, flag-4 first acceptance and WORD
    // list offsets. Expectations below are independent of the oracle fixture.
    auto stack=make_course_query_fixture(15,true);auto vb=stack.bytes();auto vt=stack.tables();
    std::array<std::uint8_t,64> shape{};const auto source=0x400u+unsigned(vb.i32(0xf10))*64u;
    for(unsigned k=0;k<64;++k)shape[k]=vb.u8(source+k);
    for(unsigned j=0;j<2;++j){
        for(unsigned k=0;k<64;++k)vb.put8(0x400+j*64+k,shape[k]);
        for(unsigned k=0;k<4;++k)vb.putf(0x400+j*64+k*12+4,j?6.f:2.f);
        vb.put16(0x400+j*64+0x3c,2);vb.put8(0x180+j,0);
    }
    vb.put16(0xc00+7*2,2);vb.put16(0xc02+7*2,0);vb.put16(0xc04+7*2,1);
    const auto sx=vb.f32(source),sz=vb.f32(source+8); // vertex lies on inclusive boundary
    auto hit=get_road_cond(vt,7,0,sx,5.f,sz);require(hit.polygon==1&&hit.y==6.f,"nearest surface");
    hit=get_road_cond(vt,7,0,sx,4.f,sz);require(hit.polygon==0&&hit.y==2.f,"stable equal-distance tie");
    vb.put16(0xc02+7*2,1);vb.put16(0xc04+7*2,0);
    hit=get_road_cond(vt,7,0,sx,4.f,sz);require(hit.polygon==1&&hit.y==6.f,"tie follows list order");
    vb.put16(0x400+64+0x3c,6);
    hit=get_road_cond(vt,7,0,sx,2.f,sz);require(hit.polygon==1&&hit.flags==6&&hit.y==6.f,"first flag-4 bypasses nearest");
    // Unrecognized mode 0x500 must NOT be treated as either bit-masked mode.
    // An absent normal table is valid on this constant-vertex path.
    vt.normals=Bytes(nullptr,0);
    hit=get_road_cond(vt,7,0x500,sx,2.f,sz);require(hit.polygon==1&&hit.y==6.f,"mode is equality, not a mask");
    // Empty list and bad explicit list view. No original guest pointer is used.
    auto road=make_course_query_fixture(15,true);auto rb=road.bytes();auto rt=road.tables();rb.put16(0xc00,0);
    auto rc=get_road_cond(rt,0,0x400,0,0,0);require(rc.polygon==-1&&rc.flags==0&&rc.y==std::numeric_limits<float>::max(),"empty list output");
    rt.area_lists=Bytes(nullptr,0);rejects([&]{get_road_cond(rt,0,0x400,0,0,0);});
    rt=road.tables();rb.put16(0xc00,2);rt.area_lists=rb.sub(0xc00,4);rejects([&]{get_road_cond(rt,0,0x400,0,0,0);});
    rt=road.tables();rb.put16(0xc00,1);rb.put16(0xc02,65535);rejects([&]{get_road_cond(rt,0,0,0,0,0);});
    // No known-good original pointers exist in these headers. This is the
    // explicit-view poison-pointer test, not a reinterpret_cast of guest RAM.
    require(rb.u32(0x140)==0xdeadbeefu&&rb.u32(0x144)==0xffffffffu,"fixture guest pointers not poisoned");
    std::cout<<"course query masks, boundaries, malformed views and preservation: PASS\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
