#include "platform/frontend_record_manager.hpp"
#include <cstdio>
#include <cstdlib>
#include <vector>
using namespace outrun::platform;
using outrun::driving::Bytes;
#define CHECK(x) do {if(!(x)){std::fprintf(stderr,"line %d: %s\n",__LINE__,#x);std::exit(1);}}while(0)
int main(){
    FrontendRecordManager manager;Bytes r(manager.records.data(),manager.records.size()),q(manager.request.data(),manager.request.size());
    CHECK(!manager.missing_pc&&manager.reset_calls==0&&q.u32(0)==0x627ee0&&q.u32(12)==~0u&&q.u32(0x14)==9);
    for(unsigned i=0;i<15;++i)CHECK(r.u32(0x14+64*i)==~0u&&r.u8(0x1d9fc+20*i)==1);
    r.put32(0x13d1c,99);r.put32(0x1db20,7);r.put8(0x13d34,1);q.put8(0x395,1);q.put32(0x388,7);
    CHECK(manager.reset());CHECK(manager.reset_calls==1&&r.u32(0x13d20)==99&&r.u32(0x13d1c)==0);
    CHECK(r.u32(0x1db20)==0&&r.u8(0x13d34)==0&&q.u8(0x395)==0&&q.u32(0x388)==7);
    // Retain opaque ownership on failure. A request pointer is never a host pointer.
    q.put32(0x24,0x1234);r.put32(0x13d1c,13);
    CHECK(!manager.reset()&&manager.missing_pc==0x525f80&&q.u32(0x24)==0x1234&&r.u32(0x13d1c)==13);
    struct Trace {std::vector<std::array<unsigned,3>> calls;unsigned fail{};} trace;
    manager.services={&trace,[](void* p,unsigned pc,unsigned token,unsigned arg){auto& t=*static_cast<Trace*>(p);t.calls.push_back({pc,token,arg});return token!=t.fail;}};
    unsigned h=1;for(unsigned off:{0x24u,0x28u,0x2cu,0x18u,0x1cu,0x20u,0x30u})q.put32(off,h++);
    trace.fail=3;CHECK(!manager.reset()&&q.u32(0x24)==0&&q.u32(0x28)==0&&q.u32(0x2c)==3);
    CHECK(trace.calls.size()==3&&r.u32(0x13d1c)==13&&manager.reset_calls==1);
    trace.fail=0;trace.calls.clear();CHECK(manager.reset());CHECK(trace.calls.size()==5);
    CHECK((trace.calls.back()==std::array<unsigned,3>{0,7,1}));
    CHECK(q.u32(0x30)==0&&r.u32(0x13d20)==13&&manager.reset_calls==2);
    CHECK(manager.reset()&&trace.calls.size()==5&&manager.reset_calls==3);
    unsigned missing{};CHECK(!record_request_reset_435ff0(q.sub(0,q.size()-1),{},missing)&&missing==0x435ff0);
    CHECK(!record_manager_reset_4940d0(r.sub(0,r.size()-1),q,{},missing)&&missing==0x4940d0);
    std::puts("record manager initialization, persistent state, release ordering, failure retention and repeated reset pass");
}
