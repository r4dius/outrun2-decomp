#include "driving/pc_wrecker.hpp"
#include "support/course_world_fixture.hpp"
#include <array>
#include <cstring>
#include <iostream>
#include <stdexcept>
using namespace outrun::driving;
namespace {
void check(bool v,const char* m){if(!v)throw std::runtime_error(m);}
template<std::size_t N> Bytes bytes(std::array<std::uint8_t,N>& a){return {a.data(),a.size()};}
struct AdvanceFixture {
    std::array<std::array<std::uint8_t,16>,4> headers{};
    std::array<std::array<std::uint8_t,4>,4> ends{};
    std::array<PcCourseEndView,4> course;
    std::array<std::uint8_t,0xf0> records{};
    std::array<std::uint8_t,4> fallback{};
    std::array<std::uint8_t,8> cache{};
    std::array<std::uint8_t,56> choices{};
    std::array<std::uint8_t,0x34> selection{};
    std::array<std::uint8_t,0x400> save{};
    PcStageViews stages;
    PcRouteContext route;
    AdvanceFixture():course{{
        PcCourseEndView{Bytes(headers[0].data(),headers[0].size()),Bytes(ends[0].data(),ends[0].size())},
        PcCourseEndView{Bytes(headers[1].data(),headers[1].size()),Bytes(ends[1].data(),ends[1].size())},
        PcCourseEndView{Bytes(headers[2].data(),headers[2].size()),Bytes(ends[2].data(),ends[2].size())},
        PcCourseEndView{Bytes(headers[3].data(),headers[3].size()),Bytes(ends[3].data(),ends[3].size())}}},
        stages{bytes(records),2,{},bytes(fallback)},route{bytes(choices),bytes(selection),bytes(save),0,0,0,{0,0,0}}{
        for(unsigned t=0;t<4;++t){
            auto h=bytes(headers[t]),p=bytes(ends[t]);h.put32(0xc,1);p.put16(0,10);
        }
        auto r=bytes(records);r.put32(0x04,0x1111);r.put32(0x08,3);r.put32(0x78+0x04,0x2222);r.put32(0x78+0x08,1);
        auto c=bytes(cache);c.put32(0,0xffffffffu);c.put32(4,0xaaaaaaaa);
        auto q=bytes(choices);for(unsigned k=0;k<14;++k)q.put32(k*4,2);q.put32(4,0); // property 1 -> route choice 0
    }
    PcCourseAdvanceContext context(std::uint8_t limit=3,bool transition=true){return {course,stages,bytes(cache),route,0x1111,limit,transition};}
};
std::array<std::uint8_t,16> place(std::uint32_t type,std::int16_t length,std::uint8_t lane,std::uint32_t old=0x2222){
    std::array<std::uint8_t,16> p{};auto b=bytes(p);b.put32(0,type);b.puti(4,type?101:0);b.put16(8,std::uint16_t(length));b.put8(0xa,lane);b.put32(0xc,old);return p;
}
}
int main(){
    try{
        {
            AdvanceFixture f;auto p=place(0,10,8);auto c=f.context();check(pc_advance_on_road_place(bytes(p),1,c),"primary crossing rejected");auto b=bytes(p);
            check(b.u32(0)==1&&b.i16(8)==0&&b.u8(0xa)==2,"primary crossing state");check(bytes(f.cache).u32(0)==0x1111&&bytes(f.cache).u32(4)==3,"primary cache side effect");
        }
        {
            AdvanceFixture f;auto p=place(0,10,8);auto c=f.context(2,true);check(!pc_advance_on_road_place(bytes(p),1,c),"primary property limit acceptance");
            check(bytes(p).u32(0)==1&&bytes(p).i16(8)==0,"rejected primary did not preserve PC mutations");
        }
        {
            AdvanceFixture f;auto p=place(1,10,1);auto c=f.context(3,true);check(pc_advance_on_road_place(bytes(p),1,c),"secondary crossing rejected");auto b=bytes(p);
            check(b.u32(0)==0&&b.i16(8)==0&&b.u8(0xa)==4&&b.u32(0xc)==0x1111,"secondary route state");
            check(bytes(f.cache).u32(0)==0x2222&&bytes(f.cache).u32(4)==1,"secondary cache ordering");
        }
        {
            AdvanceFixture f;auto p=place(1,10,1);auto c=f.context(3,false);check(!pc_advance_on_road_place(bytes(p),1,c),"disabled transition accepted");
            check(bytes(p).u32(0)==0&&bytes(p).u8(0xa)==4&&bytes(p).u32(0xc)==0x1111,"disabled transition lost PC side effects");
        }
        {
            AdvanceFixture f;bytes(f.choices).put32(4,1);auto p=place(1,10,1);auto c=f.context();check(!pc_advance_on_road_place(bytes(p),1,c),"route mismatch accepted");
            check(bytes(p).u32(0)==0&&bytes(p).i16(8)==0,"route mismatch did not wrap state");
        }
        {
            AdvanceFixture f;auto p=place(0,5,8);auto before_cache=f.cache;auto c=f.context();check(pc_advance_on_road_place(bytes(p),1,c),"ordinary advance rejected");
            check(bytes(p).i16(8)==6&&bytes(p).u32(0)==0,"ordinary advance state");check(f.cache==before_cache,"ordinary advance touched stage cache");
        }
        {
            AdvanceFixture f;auto p=place(0,0,0);auto before_cache=f.cache;auto c=f.context();check(pc_advance_on_road_place(bytes(p),-1,c),"negative clamp rejected");
            check(bytes(p).i16(8)==0&&f.cache==before_cache,"negative clamp/cache");
        }
        {
            AdvanceFixture f;auto p=place(4,7,3);auto before=p;auto c=f.context();bool threw=false;try{(void)pc_advance_on_road_place(bytes(p),1,c);}catch(const std::out_of_range&){threw=true;}
            check(threw&&p==before,"invalid course type not fail-closed");
        }
        {
            // The still-open immediate PlWrecker branch must reject before any event write.
            AdvanceFixture f;auto a=f.context();std::array<std::uint8_t,0xe00> event{};for(unsigned k=0;k<event.size();++k)event[k]=std::uint8_t(k*13u+7u);auto before=event;
            auto wf=outrun::testing::make_course_world_fixture(0,11);auto tables=wf.tables();auto stack=wf.matrix();auto wb=wf.bytes();
            PcRoadInfoContext road{tables,stack,{wb.f32(0x4638),wb.f32(0x463c)}};
            bool threw=false;try{pc_pl_wrecker_delayed(bytes(event),1,a,road);}catch(const std::invalid_argument&){threw=true;}
            check(threw&&event==before,"immediate PlWrecker path mutated before rejecting");
        }
        std::cout<<"r022 wrecker host tests passed\n";return 0;
    }catch(const std::exception& e){std::cerr<<e.what()<<"\n";return 1;}
}
