// Cross-architecture replay of original race-manager outputs recorded by the
// PC oracle (compare_pc race_manager_probe with OR2_RACE_MANAGER_REPLAY=<file>).
// Checks the float-sensitive paths on the build host (e.g. aarch64 under
// qemu): 449B30 (x87 extended arithmetic + truncation), 4506B0 (SSE with
// inf/NaN/denormal operands) and the 450AC0 goal ratio (x87 fdivr).
// Usage: test_race_manager_replay <dump>
#include "platform/race_manager.hpp"
#include "driving/pc_x87.hpp"
#include <cstdio>
#include <cstring>
#include <fstream>
#include <vector>
using namespace outrun::platform;
namespace {
float f(std::uint32_t u){float x;std::memcpy(&x,&u,4);return x;}
std::uint32_t u(float x){std::uint32_t v;std::memcpy(&v,&x,4);return v;}
// Replay services: only the values the replayed paths read are scripted.
// Any other callee reaching here means the replay setup is wrong.
struct Replay final:RaceManagerServices{
    std::uint32_t o3{},o4{};std::uint16_t end{};bool bad=false;
    void no(){bad=true;}
    void pc_46fab0()override{no();}
    void pc_47dac0()override{no();}
    void pc_47dc00()override{no();}
    void pc_47ec00()override{no();}
    std::uint32_t pc_44c2c0()override{no();return 0;}
    std::uint8_t pc_43f860()override{no();return 0;}
    std::uint8_t pc_48b1a0()override{no();return 0;}
    std::uint32_t pc_43f960()override{no();return 0;}
    std::uint8_t pc_456d60()override{no();return 0;}
    void pc_4f0dd0()override{no();}
    void pc_4f0e40()override{no();}
    void pc_456720(std::uint32_t)override{no();}
    void pc_4f2ac0()override{no();}
    void pc_4f2df0()override{no();}
    void pc_4f2b20(std::int32_t)override{no();}
    std::uint8_t pc_45a2b0(std::uint8_t)override{no();return 0;}
    std::uint32_t pc_55a930()override{no();return 0;}
    std::uint8_t pc_48b350()override{no();return 0;}
    std::uint32_t pc_46c500()override{no();return 0;}
    std::uint32_t pc_44bec0()override{no();return 0;}
    void pc_409f90(std::uint32_t)override{no();}
    void pc_40a240()override{no();}
    RaceVec3 pc_40a7d0(const std::uint8_t*)override{no();return {};}
    void pc_40a010()override{no();}
    void pc_44b900(std::uint32_t)override{no();}
    void pc_46c410(std::uint32_t)override{no();}
    void pc_46c400(std::uint32_t)override{no();}
    std::uint8_t pc_44b7b0(std::uint32_t)override{no();return 0;}
    std::uint8_t pc_4957f0()override{no();return 0;}
    std::uint8_t pc_48b310()override{no();return 0;}
    std::uint8_t pc_495490()override{no();return 0;}
    std::uint32_t pc_44bdd0()override{no();return 0;}
    void pc_495b60()override{no();}
    std::uint32_t pc_495b10()override{no();return 0;}
    std::uint32_t pc_495b70()override{no();return 0;}
    std::uint32_t pc_495b30()override{no();return 0;}
    std::uint8_t pc_4b00d0()override{no();return 0;}
    std::uint32_t pc_4b0190()override{no();return 0;}
    void pc_4b0110(std::uint32_t,std::uint32_t)override{no();}
    void pc_48b3a0()override{no();}
    void pc_495610()override{no();}
    void pc_45a0a0()override{no();}
    std::uint32_t pc_456d10()override{no();return 0;}
    void pc_456d20(std::uint32_t)override{no();}
    void pc_476760(RaceEventWork)override{no();}
    std::uint32_t pc_456dc0(std::uint32_t,std::uint32_t&,std::uint32_t)override{no();return 0;}
    std::uint16_t pc_49b2d0()override{return 0;}
    std::uint32_t pc_456d50()override{no();return 0;}
    std::int32_t pc_456d40()override{no();return 0;}
    std::uint32_t pc_456de0(std::int32_t)override{no();return 0;}
    std::uint16_t pc_43d470(std::uint32_t)override{return end;}
    void pc_4aeef0(std::uint32_t,std::uint32_t,float)override{}
    void pc_458450()override{}
    void pc_467190()override{no();}
    std::uint8_t pc_4962a0()override{no();return 0;}
    void pc_453080(std::uint32_t)override{no();}
    std::uint32_t pc_4ef710()override{no();return 0;}
    void pc_465f20()override{no();}
    std::int32_t pc_44dc50(std::uint32_t)override{no();return 0;}
    std::uint32_t pc_48b330()override{no();return 0;}
    void pc_48b340()override{no();}
    std::pair<float,float> pc_4a4440(std::uint32_t,const std::uint8_t*,const std::uint8_t*)override{return {f(o3),f(o4)};}
    void pc_48b3d0()override{no();}
    void pc_467e00()override{no();}
};
}
int main(int argc,char** argv){
    if(argc<2){std::fprintf(stderr,"usage: %s <replay dump>\n",argv[0]);return 2;}
    std::ifstream in(argv[1],std::ios::binary);std::uint32_t head[5]{};
    if(!in.read(reinterpret_cast<char*>(head),20)||head[0]!=0x50524d52u){std::fprintf(stderr,"bad replay dump\n");return 2;}
    // The dump records the oracle's x87 control word; the X87 model follows it
    // (the fast model only supports 0x007F, the in-game precision).
    if(!outrun::driving::x87_set_control(std::uint16_t(head[1]))){
        std::printf("{\"routine\":\"race_manager_replay\",\"skipped\":\"x87 control %04x not modelled by this build\"}\n",head[1]);return 0;}
    std::vector<std::uint32_t> t1(std::size_t(head[2])*6),t2(std::size_t(head[3])*3),t3(std::size_t(head[4])*3);
    for(auto* v:{&t1,&t2,&t3})if(!in.read(reinterpret_cast<char*>(v->data()),std::streamsize(v->size()*4))){std::fprintf(stderr,"short dump\n");return 2;}
    unsigned bad1=0,bad2=0,bad3=0;Replay s;
    for(std::size_t i=0;i<t1.size();i+=6){
        const auto t=race_frames_to_time_449b30(t1[i],f(t1[i+1]));
        if(t.hours!=t1[i+2]||t.minutes!=t1[i+3]||t.seconds!=t1[i+4]||t.ms!=t1[i+5])
            if(++bad1<6)std::fprintf(stderr,"449B30 frames=%08x frac=%08x original %u:%u:%u.%u native %u:%u:%u.%u\n",t1[i],t1[i+1],t1[i+2],t1[i+3],t1[i+4],t1[i+5],t.hours,t.minutes,t.seconds,t.ms);
    }
    std::vector<std::uint8_t> work(0x1000,0);
    for(std::size_t i=0;i<t2.size();i+=3){
        s.o3=t2[i];s.o4=t2[i+1];const auto r=u(race_sector_progress_4506b0(s,work.data()));
        if(r!=t2[i+2]&&++bad2<6)std::fprintf(stderr,"4506B0 o3=%08x o4=%08x original %08x native %08x\n",t2[i],t2[i+1],t2[i+2],r);
    }
    for(std::size_t i=0;i<t3.size();i+=3){
        RaceManagerState st{};RaceManagerWorld w{};std::vector<std::uint8_t> e(0x1000,0);
        w.event_work_799b38[8]=e.data();w.mode_78026c=0x10;w.variant_780258=0;w.time_decrement_637911=1;st.time_7d394c=1;
        const std::uint16_t pos=std::uint16_t(t3[i]);std::memcpy(e.data()+0x64,&pos,2);s.end=std::uint16_t(t3[i+1]);
        race_check_game_timer_450ac0(st,w,s);
        if(u(st.goal_ratio_7d3878)!=t3[i+2]&&++bad3<6)std::fprintf(stderr,"450AC0 pos=%d end=%u original %08x native %08x\n",int(std::int16_t(pos)),t3[i+1],t3[i+2],u(st.goal_ratio_7d3878));
    }
    std::printf("{\"routine\":\"race_manager_replay\",\"x87_control\":\"%04x\",\"x87_fast\":%d,\"449b30\":%u,\"4506b0\":%u,\"450ac0\":%u,\"mismatches\":%u,\"unexpected_callee\":%s}\n",
        head[1],int(OR2_X87_FAST),head[2],head[3],head[4],bad1+bad2+bad3,s.bad?"true":"false");
    return (bad1+bad2+bad3||s.bad)?1:0;
}
