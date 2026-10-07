#include "support/wall_rebound_fixture.hpp"
#include <iostream>
#include <functional>
#include <limits>
using namespace outrun::testing;
namespace {
unsigned checks=0;
void require(bool ok,const char* message){++checks;if(!ok)throw std::runtime_error(message);}
void rejects_unchanged(WallReboundFixture& f,const std::function<void()>& call){
    const auto before=f.image;bool rejected=false;
    try{call();}catch(const std::out_of_range&){rejected=true;}catch(const std::invalid_argument&){rejected=true;}
    require(rejected,"invalid input was not rejected");require(f.image==before,"invalid input changed state");
}
void poison(WallReboundFixture& f){auto b=f.bytes();b.put32(0x2b4,0xdeadc0de);b.put32(0x1248,0xfefefefe);for(unsigned k=0;k<4;++k)b.put32(0x1c14+k*0x78,1u+k);}
}
int main(){try{
    // Stage cache is deliberately stale until the key changes. A cache hit
    // must not consult records or descriptors (including invalid guest values).
    {
        auto f=make_wall_rebound_fixture(7,0);auto b=f.bytes();auto c=f.context();
        b.put32(0x5000,123);b.put32(0x5004,0xaabbccdd);
        c.response.stages.records=Bytes(nullptr,0);c.response.stages.count=4;
        require(pc_stage_property(c.response.stages,123,c.stage_cache)==0xaabbccdd,"cache hit consulted records");
        rejects_unchanged(f,[&](){pc_stage_property(c.response.stages,124,c.stage_cache);});
        b.puti(0x400c,4);c=f.context();b.put32(0x1c04,123);b.put32(0x1c08,4);b.put32(0x1c04+0x78,123);b.put32(0x1c08+0x78,9);
        b.put32(0x5000,124);poison(f);
        require(pc_stage_property(c.response.stages,123,c.stage_cache)==4,"duplicate stage order");
        b.put32(0x1c08,7);require(pc_stage_property(c.response.stages,123,c.stage_cache)==4,"cache invalidated without key change");
        require(pc_stage_property(c.response.stages,987654,c.stage_cache)==0,"absent stage property");
    }
    // Sound dedup scans every slot, including stale / currently inactive entries.
    for(unsigned slot=0;slot<32;++slot){
        auto f=make_wall_rebound_fixture(slot,6);auto b=f.bytes();auto c=f.context();c.sounds.control=2;
        b.put32(0x6984,slot);b.put32(0x6980,(slot+1)%32);
        for(unsigned k=0;k<32;++k)b.put32(0x6900+4*k,1000+k);
        auto before=f.image;pc_enqueue_sound(c.sounds,1000+(slot+17)%32);require(before==f.image,"sound whole-table dedup");
        pc_enqueue_sound(c.sounds,75);require(b.u32(0x6900+4*slot)==75,"sound enqueue wrong slot");require(b.u32(0x6984)==(slot+1)%32,"sound cursor wrap");
        before=f.image;pc_enqueue_sound(c.sounds,75);require(before==f.image,"sound full/equal cursor must stop");
    }
    for(unsigned flags=0;flags<256;++flags){
        std::array<std::uint8_t,128> entries{};std::array<std::uint8_t,8> state{};
        PcSoundQueue q{Bytes(entries.data(),128),Bytes(state.data(),8),std::uint8_t(flags)};q.state.put32(0,1);
        pc_enqueue_sound(q,75);require(q.state.u32(4)==(((flags&3)==2&&!(flags&16))?1u:0u),"sound control mask");
    }
    // Paused/disabled audio does not require queue storage. Invalid active
    // storage is rejected before touching cursors or entries.
    {auto f=make_wall_rebound_fixture(1,6);auto c=f.context();c.sounds.control=0;c.sounds.entries=Bytes(nullptr,0);c.sounds.state=Bytes(nullptr,0);pc_enqueue_sound(c.sounds,75);
     c.sounds.control=2;rejects_unchanged(f,[&](){pc_enqueue_sound(c.sounds,75);});}
    // Route pack/unpack and progress: first unresolved choice wins. Progress
    // compare is unsigned, not signed; unchanged or worse progress is a no-op.
    for(unsigned first=0;first<=14;++first)for(unsigned old=0;old<=15;++old){
        auto f=make_wall_rebound_fixture(first*16+old,3);auto b=f.bytes();auto c=f.context();
        for(unsigned k=0;k<14;++k)c.route.choices.put32(k*4,k==first?2u:(k&1u));
        const auto packed=pc_pack_route(c.route.choices);std::array<std::uint8_t,56> copy{};Bytes out(copy.data(),56);pc_unpack_route(out,packed);
        for(unsigned k=0;k<14;++k)require(out.u32(k*4)==c.route.choices.u32(k*4),"route packing order");
        const auto row=std::size_t(c.route.slot)*0x6c;c.route.save.put32(row+0x38,old);const auto before=f.image;
        pc_save_route_progress(c.route.save,c.route.slot,packed,0x87654321);
        if(first>old){require(c.route.save.u32(row+0x38)==first,"route progress");require(c.route.save.u32(row+0x30)==0x87654321,"route clock");require(c.route.save.u32(row+0x34)==packed,"route packed save");}
        else require(before==f.image,"nonprogressing route changed save");
    }
    {
        auto f=make_wall_rebound_fixture(5,4);auto c=f.context();c.route.mode=4;c.route.choices.put32(0,2);c.route.save.put32(0x364,0x05555555);
        c.route.save.put32(std::size_t(c.route.slot)*0x6c+0x38,0xffffffffu);
        pc_set_route_choice(c.route,0,0xabcdef01);
        for(unsigned k=0;k<14;++k)require(c.route.choices.u32(k*4)==1,"mode4 must reload saved route, not new choice");
        c.route.save=Bytes(nullptr,0);rejects_unchanged(f,[&](){pc_set_route_choice(c.route,0,1);});
        const auto before=f.image;pc_set_route_choice(c.route,14,1);require(before==f.image,"out-of-range route setter must skip");
        rejects_unchanged(f,[&](){pc_get_route_choice(c.route,-1);});
    }
    // Output alias order and the genuine zero / small-positive branches.
    {
        std::array<float,2> out{99,99};Bytes x(out.data(),4),z(out.data()+1,4);
        pc_direction_xz(1,2,1,2,x,z);require(out[0]==0&&out[1]==1,"zero XZ fallback");
        pc_direction_xz(0,0.00001f,0,0,x,z);require(out[1]==0.00001f,"small positive must not normalize or fallback");
        pc_direction_xz(3,4,0,0,x,z);require(out[0]==0.6f&&out[1]==0.8f,"XZ direction normalization");
        pc_direction_xz(3,4,0,0,x,x);require(out[0]==0.8f,"overlapping output stores");
        bool thrown=false;try{pc_direction_xz(1,2,3,4,x,Bytes(nullptr,0));}catch(const std::out_of_range&){thrown=true;}require(thrown&&out[0]==0.8f,"XZ invalid output atomicity");
    }
    // Alternate heading fallback uses signed 16-bit difference and signed
    // integer division toward zero, including the -32768 endpoint.
    for(int difference=-32768;difference<32768;difference+=257){
        auto f=make_wall_rebound_fixture(unsigned(difference+32768),7);auto b=f.bytes();b.putf(0xdbc,-1);b.putf(0x1c4,1);b.putf(0x26c,0);b.put16(0x160,0x9abc);
        calc_rebound_heading_float(b.sub(0,0x1000),std::int16_t(difference));
        require(b.u32(0x290)==0&&b.u8(0x282)==0,"float heading fallback status");require(std::uint16_t(b.i16(0x286))==std::uint16_t(0x9abc-(difference*5)/4),"float heading signed wrap");
    }
    // Native pointer poisoning and write-set checks on independent repeated
    // calls. This is not an oracle: original-output captures are separate tests.
    for(unsigned i=0;i<512;++i){
        auto a=make_wall_rebound_fixture(i,9),b=a;poison(b);
        auto ac=a.context(),bc=b.context();auto ab=a.bytes(),bb=b.bytes();
        const auto old=a.image;CourseProbe n{ab.f32(0x6b00),ab.f32(0x6b04),ab.f32(0x6b08)};
        cw_rebound_status(ab.sub(0,0x1000),ab.sub(0x1000,0x800),n,ac);
        cw_rebound_status(bb.sub(0,0x1000),bb.sub(0x1000,0x800),n,bc);
        poison(a);require(a.image==b.image,"native followed serialized guest pointers");
        require(std::memcmp(old.data()+0x1800,a.image.data()+0x1800,0x400)==0,"rebound changed matrix stack");
        require(std::memcmp(old.data()+0x1010,a.image.data()+0x1010,64)==0,"rebound changed work matrix");
        require((ab.u32(8)&~0x140u)==(Bytes(const_cast<std::uint8_t*>(old.data()),old.size()).u32(8)&~0x140u),"unrelated event flags changed");
        require(ab.u8(0x283)==30&&ab.u8(0x284)==36,"rebound countdowns");
        require(ab.u32(0x28c)==ab.u32(0x1c4),"rebound speed copy");
    }
    // All effectful dependencies preflight: rejection must not set flags or timer.
    {
        auto f=make_wall_rebound_fixture(9,9);auto b=f.bytes();auto c=f.context();
        rejects_unchanged(f,[&](){cw_rebound_status(b.sub(0,0x1000),Bytes(nullptr,0),{1,0,0},c);});
        b.put32(0xdf8,0);b.put32(0x1f4,100);b.put8(0x284,0);b.put32(0x5c,1);b.put32(0x5000,b.u32(0x68));b.put32(0x5004,1);
        c=f.context();c.route.choices=Bytes(nullptr,0);rejects_unchanged(f,[&](){cw_rebound_status(b.sub(0,0x1000),b.sub(0x1000,0x800),{1,0,0},c);});
        c=f.context();b.put32(0x5c,0);b.put8(0xd23,0);c.sounds.control=2;c.sounds.state=Bytes(nullptr,0);
        rejects_unchanged(f,[&](){cw_rebound_status(b.sub(0,0x1000),b.sub(0x1000,0x800),{1,0,0},c);});
        c=f.context();c.response.stages.descriptors.clear();b.puti(0x400c,1);b.put32(0x68,b.u32(0x1c04));
        c.response.stages.count=1;rejects_unchanged(f,[&](){cw_rebound_status(b.sub(0,0x1000),b.sub(0x1000,0x800),{1,0,0},c);});
    }
    std::cout<<checks<<" rebound/route/sound boundary and view checks\n";return 0;
}catch(const std::exception& e){std::cerr<<"FAIL: "<<e.what()<<'\n';return 1;}}
