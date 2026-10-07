#include "control.hpp"
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>
using namespace outrun::reconstructed;

namespace {
int checks = 0;
void check(bool value, const char* message) {
    ++checks;
    if (!value) throw std::runtime_error(message);
}
struct Trace { std::vector<std::string> calls; };
Trace& trace(void* user) { return *static_cast<Trace*>(user); }
void mi(ModeMachine& m, void* u) { trace(u).calls.push_back("i"+std::to_string(m.state.current)); }
void mc(ModeMachine& m, void* u) { trace(u).calls.push_back("c"+std::to_string(m.state.current)); }
void mx(ModeMachine& m, void* u) { trace(u).calls.push_back("x"+std::to_string(m.state.current)); }
ModeMachine modes(Trace& t) {
    ModeMachine m{}; m.user=&t;
    for(auto& cb:m.callbacks) cb={mi,mc,mx};
    return m;
}
void test_mode_idle() {
    Trace t; auto m=modes(t); m.state.current=16;
    m.state.snapshot_source=0xdeadbeefu; m.state.exit_byte_unknown=17;
    mode_control(m);
    check(t.calls==std::vector<std::string>{"c16"},"idle: only control");
    check(m.state.snapshot_destination==0xdeadbeefu,"snapshot copied");
    check(m.state.exit_byte_unknown==17,"no transition: byte preserved");
}
void test_mode_transition() {
    Trace t; auto m=modes(t); m.state.current=9;m.state.requested=16;m.state.transition_pending=42;
    mode_control(m);
    check(t.calls==std::vector<std::string>{"i16","c16"},"entry before control");
    check(m.state.previous==9&&m.state.current==16,"previous/current");
    check(m.state.transition_pending==0,"entry clears pending");
}
void test_mode_request_in_control() {
    Trace t; auto m=modes(t);m.state.current=16;m.state.exit_byte_unknown=255;
    m.callbacks[16].control=[](ModeMachine& m,void* u){mc(m,u);m.state.requested=19;m.state.transition_pending=1;};
    mode_control(m);
    check(t.calls==std::vector<std::string>{"c16","x16"},"exit old mode at end of tick");
    check(m.state.current==16&&m.state.transition_pending==1,"transition remains pending");
    check(m.state.exit_byte_unknown==0,"tail helper resets byte");
    t.calls.clear();mode_control(m);
    check(t.calls==std::vector<std::string>{"i19","c19"},"new mode begins next tick");
    check(m.state.previous==16&&m.state.current==19,"next tick previous/current");
}
void test_mode_callback_mutation() {
    Trace t;auto m=modes(t);m.state.transition_pending=1;m.state.requested=1;
    m.callbacks[1].init=[](ModeMachine& m,void* u){mi(m,u);m.state.current=2;m.state.transition_pending=3;};
    m.callbacks[2].exit=[](ModeMachine& m,void* u){mx(m,u);m.state.transition_pending=0;m.state.exit_byte_unknown=99;};
    mode_control(m);
    check(t.calls==std::vector<std::string>{"i1","c2","x2"},"current and flags re-read after init");
    check(m.state.exit_byte_unknown==0,"helper runs even if exit clears pending");
    check(m.state.transition_pending==0,"exit flag change retained");
}
void test_mode_control_changes_current() {
    Trace t;auto m=modes(t);m.state.current=1;
    m.callbacks[1].control=[](ModeMachine& m,void* u){mc(m,u);m.state.current=2;m.state.transition_pending=1;};
    mode_control(m);
    check(t.calls==std::vector<std::string>{"c1","x2"},"exit uses latest current mode");
}
void test_mode_invalid() {
    Trace t;auto m=modes(t);m.state.current=37;bool threw=false;
    try{mode_control(m);}catch(const std::out_of_range&){threw=true;}
    check(threw,"invalid mode rejects rather than using arbitrary address");
    m.state.current=0;m.callbacks[0].control=nullptr;threw=false;
    try{mode_control(m);}catch(const std::logic_error&){threw=true;}
    check(threw,"missing game callback explicitly fails");
}
void e4(EventSystem& e,std::uint32_t p,void* u){trace(u).calls.push_back("4:"+std::to_string(e.current_index)+":"+std::to_string(p));}
void e1(EventSystem& e,std::uint32_t p,void* u){trace(u).calls.push_back("1:"+std::to_string(e.current_index)+":"+std::to_string(p));}
void ec(EventSystem& e,std::uint32_t p,void* u){trace(u).calls.push_back("c:"+std::to_string(e.current_index)+":"+std::to_string(p));}
void test_all_event_masks() {
    // Exhaust all 256 flag values, at first, middle and final slots.
    for(std::size_t index: {std::size_t(0),std::size_t(207),kEventCount-1}) {
        for(unsigned mask=0;mask<256;++mask) {
            Trace t;EventSystem e{};e.user=&t;e.slots[index]={123,e4,e1,ec};e.flags[index]=static_cast<std::uint8_t>(mask);
            event_control(e);
            std::vector<std::string> expected;
            const auto suffix=":"+std::to_string(index)+":123";
            if((mask&4u)!=0)expected.push_back("4"+suffix);
            if((mask&1u)!=0)expected.push_back("1"+suffix);
            if((mask&0x18u)==0 && (mask&3u)!=0)expected.push_back("c"+suffix);
            const auto flags=(mask&~5u) | ((mask&1u)!=0?2u:0u);
            check(t.calls==expected,"event sequence for flag mask");
            check(e.flags[index]==flags,"event flags for flag mask");
            check(e.current_index==kEventCount-1,"current pointer remains at last slot");
        }
    }
}
void test_event_mutation() {
    Trace t;EventSystem e{};e.user=&t;e.flags[0]=4;e.slots[0]={10,e4,e1,ec};
    e.slots[0].on_bit4=[](EventSystem& e,std::uint32_t p,void* u){e4(e,p,u);e.flags[0]=0x81;e.slots[0].parameter=20;};
    e.slots[0].on_bit1=[](EventSystem& e,std::uint32_t p,void* u){e1(e,p,u);e.flags[0]|=0x10;e.slots[0].parameter=30;};
    event_control(e);
    check(t.calls==std::vector<std::string>{"4:0:10","1:0:20"},"callbacks re-read parameters; init can inhibit control");
    check(e.flags[0]==0x92,"unknown high flags and callback mutations preserved");
}
void test_event_null_callbacks() {
    EventSystem e{};e.flags[0]=5;event_control(e);
    check(e.flags[0]==2,"null callbacks still advance flag transitions");
}
void test_event_future_slot() {
    Trace t;EventSystem e{};e.user=&t;e.flags[0]=2;e.slots[0]={12,e4,e1,ec};e.slots[409]={77,e4,e1,ec};
    e.slots[0].on_active=[](EventSystem& e,std::uint32_t p,void* u){ec(e,p,u);e.flags[409]=1;};
    event_control(e);
    check(t.calls==std::vector<std::string>{"c:0:12","1:409:77","c:409:77"},"new later event runs during same sweep");
}
void test_event_new_bit4_waits() {
    Trace t;EventSystem e{};e.user=&t;e.flags[0]=1;e.slots[0]={8,e4,e1,ec};
    e.slots[0].on_bit1=[](EventSystem& e,std::uint32_t p,void* u){e1(e,p,u);e.flags[0]|=4;};
    event_control(e);
    check(t.calls==std::vector<std::string>{"1:0:8","c:0:8"},"new bit4 does not run retroactively");
    check(e.flags[0]==6,"bit4 preserved when set during bit1 callback");
    t.calls.clear();event_control(e);
    check(t.calls==std::vector<std::string>{"4:0:8","c:0:8"},"pending bit4 runs next sweep");
}
}
int main() {
    try {
        test_mode_idle();test_mode_transition();test_mode_request_in_control();
        test_mode_callback_mutation();test_mode_control_changes_current();test_mode_invalid();
        test_all_event_masks();test_event_mutation();test_event_null_callbacks();
        test_event_future_slot();test_event_new_bit4_waits();
        std::cout<<"PASS: 11 test groups, "<<checks<<" checks (including 768 event-mask cases).\n";
        std::cout<<"Unit tests only: original Windows EXE not executed; no Switch runtime validation.\n";
        return 0;
    }catch(const std::exception& e){std::cerr<<"FAIL: "<<e.what()<<'\n';return 1;}
}
