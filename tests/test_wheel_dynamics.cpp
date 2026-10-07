#include "driving/pc_wheel_dynamics.hpp"
#include <array>
#include <cmath>
#include <iostream>
#include <limits>
using namespace outrun::driving;
namespace {
unsigned checks=0;
void require(bool b,const char* why){++checks;if(!b)throw std::runtime_error(why);}
template<class F>void rejects(F&& f){bool caught=false;try{f();}catch(const std::exception&){caught=true;}require(caught,"expected checked exception");}
struct Fixture {
    std::array<std::uint8_t,event_size> es{};std::array<std::uint8_t,work_size> ws{};
    std::array<std::uint8_t,parameter_size> ps{};
    std::array<std::uint8_t,648> ta{},tb{};std::array<std::uint8_t,1024> br{};
    Bytes e(){return {es.data(),es.size()};}Bytes w(){return {ws.data(),ws.size()};}Bytes p(){return {ps.data(),ps.size()};}
    Tables tables(){return {{Bytes(ta.data(),ta.size()),Bytes(tb.data(),tb.size())},Bytes(br.data(),br.size())};}
    Fixture(){
        auto ev=e(),pa=p();auto t=tables();
        // Synthetic tables: intentionally not copied from the commercial executable.
        for(unsigned c=0;c<2;++c){t.torque[c].putf(0,0.1f);t.torque[c].putf(4,4.0f);
            for(unsigned i=0;i<160;++i)t.torque[c].putf(8+i*4,100.0f+float(i));}
        for(unsigned i=0;i<256;++i)t.brake.putf(i*4,float(i)/255.0f);
        ev.putf(0x21c,400);ev.puti(0x3c,255);ev.putf(0x22c,1);ev.putf(0x2a0,1);ev.put32(0x208,1);ev.putf(0x228,500);
        ev.putf(0x214,200);ev.put32(0x308,100);ev.putf(0xdc0,1);ev.puti(0x34,128);
        // Deliberately invalid x86 pointers: native code must use explicit views.
        ev.put32(0x2b4,0xdead0001);for(unsigned i=0;i<4;++i)w().put32(0x248+i*4,0xbad00001+i);
        pa.putf(0,1200);pa.putf(0xb48,0.3f);pa.putf(0xb94,0.3f);pa.putf(0xc78,2);pa.putf(0xcc4,2);pa.putf(0x16dc,0.5f);
        pa.putf(0xf70,10000);pa.putf(0xfbc,10000);pa.putf(0xda8,100000);pa.putf(0xdf4,100000);pa.putf(0xd10,0.01f);pa.putf(0xd5c,0.01f);
        pa.putf(0x17c0,8);pa.putf(0x180c,1);pa.putf(0x1858,1);pa.putf(0x134c,3);pa.putf(0x10ec,1);
        for(unsigned gear=0;gear<=7;++gear)pa.putf((gear+0x3a)*0x4c,3.0f/(float(gear)+1));
        pa.putf(0x1644,1000);pa.putf(0x1690,1250);pa.putf(0x15f8,100);pa.put32(0x10a0,6);
        pa.putf(0x1560,0.08f);pa.putf(0x15ac,0.08f);pa.putf(0x1514,8);pa.putf(0x1728,1);pa.putf(0x1774,500);
        pa.putf(0xe40,2);pa.putf(0xed8,1);pa.putf(0xe8c,2);pa.putf(0xf24,1);
        for(auto q:embedded_wheels(w())){q.putf(0x34,3000);q.putf(0x38,3000);q.putf(0xe8,1);q.putf(0xc0,3000);
            q.putf(0xd4,12);q.putf(0xd8,40);q.putf(0xdc,40);q.putf(0xc8,0.0001f);}
    }
};
}
int main(){try{
    Fixture x;auto e=x.e(),w=x.w(),p=x.p();auto wheels=embedded_wheels(w);auto t=x.tables();
    for(unsigned i=0;i<4;++i){wheels[i].put32(0x10,0x12345678+i);require(w.u32(0x258+i*0xf4+0x10)==0x12345678+i,"bounded embedded wheel view");}
    rejects([&]{(void)embedded_wheels(w.sub(0,0x620));});
    // Exact no-slip states and independently expected signs.
    for(auto q:wheels){q.putf(0xd4,0);q.putf(0xd8,0);}slip_ratio(p,wheels);
    for(auto q:wheels)require(q.f32(0xe0)==0,"stationary slip zero");
    for(auto q:wheels){q.putf(0xd4,12);q.putf(0xd8,0);}slip_ratio(p,wheels);
    for(auto q:wheels)require(q.f32(0xe0)==1,"locked rotating-contact wheel slip positive");
    for(auto q:wheels){q.putf(0xd4,0);q.putf(0xd8,40);}slip_ratio(p,wheels);
    for(auto q:wheels)require(q.f32(0xe0)==-1,"wheelspin slip negative");
    // Front braking at zero crossing must stop, rather than reverse, the wheel.
    for(int sign:{-1,1}){for(auto q:wheels){q.putf(0xd8,float(sign));q.putf(0xb0,0);q.putf(0xc4,float(sign)*10000);}
        front_wheel_rotation(p,wheels);require(wheels[0].f32(0xd8)==0&&wheels[1].f32(0xd8)==0,"front zero-crossing clamp");}
    // Force resolution at exactly zero angle is identity.
    for(auto q:wheels){q.put16(0xee,0);q.putf(0xac,123);q.putf(0xb0,-456);}resolve_wheel_forces(wheels);
    for(auto q:wheels)require(q.f32(0xb4)==123&&q.f32(0xb8)==-456,"force resolution zero angle");
    // Low grip: finite outputs even when both force components are zero.
    for(auto q:wheels){q.putf(0xc0,0);q.putf(0xac,0);q.putf(0xb0,0);q.putf(0xdc,0);q.putf(0xd4,0);}
    friction_circle(e,p,wheels);for(auto q:wheels)require(q.f32(0xac)==0&&q.f32(0xb0)==0&&q.f32(0xe4)==1,"zero friction vector");
    for(auto q:wheels){q.putf(0xac,20);q.putf(0xb0,10);}friction_circle(e,p,wheels);
    for(auto q:wheels)require(q.f32(0xac)==0&&q.f32(0xb0)==0,"zero grip rejects nonzero traction");
    // The high brake branch really changes the lateral limiter, not a redundant condition.
    auto brake_case=[&](int pedal){for(auto q:wheels){q.putf(0xc0,1000);q.putf(0xac,1000);q.putf(0xb0,1000);}
        e.puti(0x38,pedal);friction_circle(e,p,wheels);return wheels[0].f32(0xac);};
    require(brake_case(250)!=brake_case(251),"250/251 lateral braking threshold");
    e.put32(0x1f4,19);e.puti(0xd90,1);for(auto q:wheels)q.putf(0xd0,0);
    cornering_power(e,p,wheels);const auto low=wheels[0].f32(0xc8);e.put32(0x1f4,20);cornering_power(e,p,wheels);
    require(low>wheels[0].f32(0xc8),"low speed cornering threshold");
    for(auto q:wheels){q.putf(0xd0,-1);}cornering_power(e,p,wheels);for(auto q:wheels)require(q.f32(0xc8)==0,"cornering lower clamp");
    e.puti(0x38,0);distribute_brake_torque(e,w,p,t);for(auto q:wheels)require(q.f32(0xc4)==0,"zero brake distribution");
    e.puti(0x38,255);distribute_brake_torque(e,w,p,t);require(wheels[0].f32(0xc4)>0&&wheels[2].f32(0xc4)>0,"both axle brakes written");
    p.putf(0xb48,0);rejects([&]{slip_ratio(p,wheels);});rejects([&]{front_driving_force(p,wheels);});p.putf(0xb48,0.3f);
    p.putf(0xc78,0);rejects([&]{front_wheel_rotation(p,wheels);});p.putf(0xc78,2);
    p.putf(0x16dc,0);rejects([&]{rear_wheel_rotation(e,w,p);});p.putf(0x16dc,0.5f);
    p.putf(0xda8,0);rejects([&]{rolling_resistance(e,p,wheels);});p.putf(0xda8,100000);
    p.putf(0x180c,-1);rejects([&]{distribute_brake_torque(e,w,p,t);});p.putf(0x180c,1);
    e.put32(0x208,17);rejects([&]{rear_driving_force(e,w,p);});e.put32(0x208,1);
    p.putf(0xf70,std::numeric_limits<float>::infinity());rejects([&]{cornering_power(e,p,wheels);});
    // Stateful, sanitizer-friendly native-only regression. No road simulation is claimed.
    Fixture chain;auto ce=chain.e(),cw=chain.w(),cp=chain.p();auto ct=chain.tables();
    bool engine_changed=false,wheel_changed=false;const auto initial_engine=ce.f32(0x21c),initial_wheel=embedded_wheels(cw)[2].f32(0xd8);
    const RunningResistanceTuning rr{0.0225f,0.2f,1.0f,0.9f,0.175f,0.175f};
    for(unsigned frame=0;frame<4096;++frame){
        ce.puti(0x34,int((frame*17)%256));ce.puti(0x38,frame%500<70?255:0);ce.put32(0x208,(frame/300)%7);
        ce.put32(0x1f4,(frame*3)%240);ce.puti(0xd90,frame%700<50?1:0);ce.putf(0xdc0,frame%900<90?2:1);
        for(unsigned i=0;i<4;++i){auto q=embedded_wheels(cw)[i];q.putf(0xd4,float((frame*11+i*5)%700)*0.1f);
            q.putf(0x34,frame%500<10?0:3000);q.putf(0xe8,frame%700<30?0.01f:1);q.putf(0xd0,0);
            const auto angle=std::uint16_t(int((frame*13+i*37)%6000)-3000);q.put16(0xee,angle);q.put16(0x32,angle);q.put16(0xec,0);}
        known_driving_tail_from_contacts(ce,cw,cp,ct,rr,0.75f);
        require(std::isfinite(ce.f32(0x50))&&std::isfinite(ce.f32(0x54)),"physical work summaries finite");
        require(std::isfinite(ce.f32(0x21c))&&ce.f32(0x21c)>=0&&ce.f32(0x21c)<=cp.f32(0x1690),"retained engine finite and bounded");
        for(auto q:embedded_wheels(cw)){for(auto offset:{0xac,0xb0,0xb4,0xb8,0xbc,0xd8,0xe0})require(std::isfinite(q.f32(offset)),"retained wheel output finite");
            require(q.f32(0xe0)>=-1&&q.f32(0xe0)<=1,"slip ratio bounded");}
        engine_changed|=ce.f32(0x21c)!=initial_engine;wheel_changed|=embedded_wheels(cw)[2].f32(0xd8)!=initial_wheel;
    }
    require(engine_changed&&wheel_changed,"stage updates real retained engine and wheel state");
    require(ce.u32(0x2b4)==0xdead0001,"guest pointer not dereferenced or overwritten");
    std::cout<<"wheel dynamics: "<<checks<<" checks passed; 4096 synthetic retained known-tail steps, not a recorded race\n";return 0;
}catch(const std::exception& e){std::cerr<<"FAIL after "<<checks<<" checks: "<<e.what()<<"\n";return 1;}}
