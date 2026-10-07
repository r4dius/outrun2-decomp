// Portable native-code replay. Does not map or execute any x86/game code.
#include "driving/pc_wheel_dynamics.hpp"
#include <algorithm>
#include <array>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>
#include <vector>
using namespace outrun::driving;
namespace {
constexpr std::size_t state_bytes = event_size + work_size + parameter_size;
constexpr std::size_t record_bytes = 8 + 2 * state_bytes;
constexpr const char* source_hash = "bdafa88a5abdd2a9743f6bdcc5e2189288c0203790412fce10b9bb649933482f";
void check(bool condition, const char* why) { if (!condition) throw std::runtime_error(why); }
void poison(Bytes event, Bytes work) {
    event.put32(0x2b4, 0xdead0001);
    for (unsigned n=0;n<4;++n) work.put32(0x248+n*4, 0xbaad0001+n*0x100);
}
void execute(std::uint32_t id, Bytes event, Bytes work, Bytes params) {
    auto wheels=embedded_wheels(work);
    switch(id) {
    case 1: cornering_power(event,params,wheels); break;
    case 2: side_force(wheels); break;
    case 3: front_driving_force(params,wheels); break;
    case 4: rear_driving_force(event,work,params); break;
    case 5: friction_circle(event,params,wheels); break;
    case 6: resolve_wheel_forces(wheels); break;
    case 7: front_wheel_rotation(params,wheels); break;
    case 8: rear_wheel_rotation(event,work,params); break;
    case 9: rolling_resistance(event,params,wheels); break;
    case 10: slip_ratio(params,wheels); break;
    default: throw std::runtime_error("unknown snapshot routine id");
    }
}
}
int main(int argc, char** argv) { try {
    if (argc<2 || argc>3 || (argc==3 && std::string(argv[2])!="--poison-guest-pointers"))
        throw std::runtime_error("usage: replay_wheels FILE [--poison-guest-pointers]");
    const bool poisoned=argc==3;
    std::ifstream f(argv[1],std::ios::binary|std::ios::ate);
    check(bool(f),"cannot open snapshot file");
    const auto length=f.tellg();
    check(length>=96 && length<=64*1024*1024,"invalid snapshot file size");
    f.seekg(0);std::vector<std::uint8_t> data(static_cast<std::size_t>(length));
    f.read(reinterpret_cast<char*>(data.data()),static_cast<std::streamsize>(data.size()));
    check(bool(f),"snapshot read failed");Bytes view(data.data(),data.size());
    check(std::memcmp(data.data(),"OR2W005\0",8)==0,"invalid snapshot magic");
    check(view.u32(8)==1,"unsupported snapshot format version");
    check(view.u32(12)==event_size && view.u32(16)==work_size && view.u32(20)==parameter_size,"wrong snapshot state sizes");
    const auto count=view.u32(24),cw=view.u32(28);
    check(count>0 && count<=2048,"invalid snapshot record count");
    check(cw==0x037f || cw==0x027f,"unknown source x87 control word");
    check(std::memcmp(data.data()+32,source_hash,64)==0,"wrong original executable provenance");
    check(data.size()==96+std::size_t(count)*record_bytes,"truncated or trailing snapshot bytes");
    std::array<std::size_t,10> coverage{};std::size_t mismatches=0;
    for(std::uint32_t n=0;n<count;++n){
        const auto at=96+std::size_t(n)*record_bytes;const auto id=view.u32(at),index=view.u32(at+4);
        check(id>=1 && id<=10,"unknown snapshot routine id");++coverage[id-1];
        auto start=data.begin()+static_cast<std::ptrdiff_t>(at+8);
        std::vector<std::uint8_t> native(start,start+state_bytes),expected(start+state_bytes,start+2*state_bytes);
        auto state=[&](std::vector<std::uint8_t>& b){return std::array<Bytes,3>{Bytes(b.data(),event_size),Bytes(b.data()+event_size,work_size),Bytes(b.data()+event_size+work_size,parameter_size)};};
        auto x=state(native),y=state(expected);
        if(poisoned){poison(x[0],x[1]);poison(y[0],y[1]);}
        execute(id,x[0],x[1],x[2]);
        auto mismatch=std::mismatch(native.begin(),native.end(),expected.begin());
        if(mismatch.first!=native.end()){
            ++mismatches;std::cerr<<"mismatch id="<<id<<" index="<<index<<" state_offset="<<(mismatch.first-native.begin())<<"\n";
        }
    }
    std::cout<<"{\"records\":"<<count<<",\"mismatches\":"<<mismatches<<",\"bytes_compared\":"<<std::size_t(count)*state_bytes
             <<",\"poisoned_guest_pointers\":"<<(poisoned?"true":"false")<<",\"source_x87_control\":"<<cw<<",\"coverage\":[";
    for(unsigned n=0;n<coverage.size();++n)std::cout<<(n?",":"")<<coverage[n];
    std::cout<<"]}\n";return mismatches?1:0;
} catch(const std::exception& x) {std::cerr<<"ERROR: "<<x.what()<<"\n";return 2;}}
