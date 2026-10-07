#include "driving/pc_environment_blend.hpp"
#include "driving/pc_course_world.hpp"
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <stdexcept>
using namespace outrun::driving;
#define CHECK(x) do{if(!(x)){std::fprintf(stderr,"environment line %d: %s\n",__LINE__,#x);std::exit(1);}}while(0)
template<std::size_t N> Bytes view(std::array<std::uint8_t,N>& a){return Bytes(a.data(),a.size());}
int main(){
    std::array<std::uint8_t,0x1e0> sun{},saved_sun{};
    std::array<std::uint8_t,0x54> fog{},saved_fog{};
    std::array<std::uint8_t,8*64> stack{};
    std::array<std::uint8_t,0xb0> authored{};
    auto s=view(sun),f=view(fog),a=view(authored);
    PcMatrixStack matrices{view(stack),0,0,8};pc_matrix_identity(matrices);
    PcEnvironmentBlendContext c{0,0,20,view(saved_sun),view(saved_fog),s,f,matrices.current(),matrices};
    std::array<Bytes,3> records{a,Bytes(nullptr,0),Bytes(nullptr,0)};
    f.putf(8,10);f.putf(12,20);f.putf(16,30);
    f.put8(0x18,20);f.put8(0x19,40);f.put8(0x1a,60);
    course_environment_blend_44ab10(records,0,c);CHECK(saved_fog==fog);
    a.put32(0x34,7);a.putf(0x38,50);a.putf(0x3c,60);a.putf(0x40,70);
    a.put8(0x48,60);a.put8(0x49,80);a.put8(0x4a,100);
    c.phase_7d28c8=2;c.time_7d2934=10;
    course_environment_blend_44ab10(records,0,c);
    CHECK(f.f32(8)==30&&f.f32(12)==40&&f.f32(16)==50);
    CHECK(f.u32(4)==7&&f.u32(0x14)==0x283c50);
    auto previous=fog;c.phase_7d28c8=3;
    course_environment_blend_44ab10(records,0,c);CHECK(fog==previous);
    c.phase_7d28c8=0;course_environment_blend_44ab10(records,1,c);CHECK(saved_sun==sun);
    c.phase_7d28c8=2;c.time_7d2934=0;a.putf(0x60,0);a.putf(0x64,0);
    course_environment_blend_44ab10(records,1,c);
    CHECK(s.f32(0x14)==1&&s.f32(0x24)==0&&s.f32(0x4c)==-1);
    CHECK(matrices.depth==0&&matrices.current_offset==0);
    std::array<std::uint8_t,0x110> camera{};
    std::array<std::uint8_t,24> nearest{};
    std::array<std::uint8_t,6*0xa0+16> lights{};
    std::array<std::uint8_t,4*0x2c> list{};
    auto cam=view(camera),n=view(nearest),l=view(lights),p=view(list);
    cam.putf(0x10c,1); // camera looks toward positive Z
    for(unsigned i=0;i<3;++i){auto r=p.sub(i*0x2c,0x2c);r.putf(0xc,20);r.putf(0x18,30.f-i*10.f);r.putf(0x1c,5);r.putf(0,1.f+i);}
    p.putf(3*0x2c+0xc,-999.9f);
    course_environment_lights_44a1d0(p,cam,n,l);
    CHECK(n.u32(4)==2&&n.u32(12)==1&&n.u32(20)==0);
    CHECK(n.f32(0)==10&&n.f32(8)==20);
    CHECK(l.u32(0x140)==1&&l.f32(0x148)==3&&l.f32(0x198)==1.1920928955078125e-7f);
    CHECK(l.u32(0x280)==1&&l.u32(0x1e0)==1&&l.u32(0x320)==1);
    auto old_lights=lights;
    course_environment_lights_44a1d0(Bytes(nullptr,0),cam,n,l);
    CHECK(n.f32(0)==std::numeric_limits<float>::max()&&n.f32(8)==std::numeric_limits<float>::max());
    for(unsigned i:{2u,3u,4u,5u}){CHECK(l.u32(i*0xa0)==0);for(unsigned k=4;k<0xa0;++k)CHECK(l.u8(i*0xa0+k)==old_lights[i*0xa0+k]);}
    for(unsigned i=0x3c0;i<lights.size();++i)CHECK(lights[i]==0);
    // A rear light outside its authored range must not remain selected.
    p.putf(0x18,-30);p.putf(0x2c+0xc,-100);p.putf(2*0x2c+0xc,-100);
    course_environment_lights_44a1d0(p,cam,n,l);CHECK(n.f32(0)==std::numeric_limits<float>::max());
    p.putf(0x1c,30);course_environment_lights_44a1d0(p,cam,n,l);CHECK(n.u32(4)==0&&n.f32(0)==30);
    bool rejected=false;try{course_environment_lights_44a1d0(p.sub(0,0x2c),cam,n,l);}catch(const std::exception&){rejected=true;}CHECK(rejected);
    // 44A000 phase machine with absent (defaults 100/2) and course transition records.
    {
        std::array<std::uint8_t,0x70> car{};auto v=view(car);
        PcEnvironmentTransition none{};
        c.phase_7d28c8=0;course_environment_phase_44a000(v,none,c);
        CHECK(c.phase_7d28c8==1&&c.time_7d2934==120&&c.duration_7d28d8==2.f*60.2f);
        v.put16(0x64,99);course_environment_phase_44a000(v,none,c);CHECK(c.phase_7d28c8==1&&c.time_7d2934==120);
        v.put16(0x64,100);course_environment_phase_44a000(v,none,c);CHECK(c.phase_7d28c8==2&&c.time_7d2934==119);
        for(int k=0;k<119;++k)course_environment_phase_44a000(v,none,c);
        CHECK(c.phase_7d28c8==2&&c.time_7d2934==0);
        course_environment_phase_44a000(v,none,c);CHECK(c.phase_7d28c8==3&&c.time_7d2934==-1);
        course_environment_phase_44a000(v,none,c);CHECK(c.phase_7d28c8==3&&c.time_7d2934==-1);
        std::array<std::uint8_t,4> rec{};auto r=view(rec);r.put16(0,40);r.put8(2,0xfd);
        PcEnvironmentTransition course{2,r},ignored{1,r};
        CHECK(environment_trigger_44c610(course)==40&&environment_duration_44c640(course)==-3);
        CHECK(environment_trigger_44c610(ignored)==100&&environment_duration_44c640(ignored)==2);
        c.phase_7d28c8=0;course_environment_phase_44a000(v,course,c);
        CHECK(c.time_7d2934==-180&&c.duration_7d28d8==-3.f*60.2f);
    }
    // 44B020 phase 3: fixed records, flag gating and positional interpolation.
    {
        const auto primary=empty_world_course();
        std::array<std::uint8_t,0x70> car{};auto v=view(car);
        std::array<std::uint32_t,6> flag_words{};Bytes flags(reinterpret_cast<std::uint8_t*>(flag_words.data()),24);
        std::array<std::uint8_t,3*0xb0+2> fixed_list{};auto fl=view(fixed_list);
        fl.put16(0,0xfffe);fl.put16(0xb0,5);fl.put16(0xb0+0x10,5);fl.put16(0x160,0xffff);
        auto e=fl.sub(0xb0,0xb0);e.put32(0x34,9);e.putf(0x38,1);e.putf(0x3c,2);e.putf(0x40,3);
        e.put8(0x48,11);e.put8(0x49,22);e.put8(0x4a,33);
        std::array<Bytes,3> lists{fl,Bytes(nullptr,0),Bytes(nullptr,0)};
        fog.fill(0xcc);c.phase_7d28c8=3;v.put16(0x64,6);
        course_environment_progress_44b020(lists,0,v,flags,primary,c);CHECK(fog[0]==0xcc&&fog[8]==0xcc);
        flag_words[0]=1;course_environment_progress_44b020(lists,0,v,flags,primary,c);
        CHECK(flag_words[0]==0&&f.u32(4)==9&&f.f32(8)==1&&f.f32(16)==3&&f.u32(0x14)==0x0b1621&&f.u8(0x1b)==0xcc);
        CHECK(fog[0]==0xcc&&fog[0x1c]==0xcc);
        // Interpolated record already prepared by 44A520 (start 0,0,0, 1/10, direction -Z).
        std::array<std::uint8_t,0xb0+2> run_list{};auto rl=view(run_list);
        rl.put16(0,0);rl.put16(0x10,10);rl.putf(4,0);rl.putf(8,0);rl.putf(12,0);
        rl.putf(0x20,0.1f);rl.putf(0x24,0);rl.putf(0x28,0);rl.putf(0x2c,-1);
        rl.put8(0x48,0);rl.put8(0x64,200);rl.putf(0x38,0);rl.putf(0x54,10);rl.put16(0xb0,0xffff);
        lists={Bytes(nullptr,0),rl,Bytes(nullptr,0)};v.putf(0x1c,5);
        course_environment_progress_44b020(lists,0,v,flags,primary,c);
        CHECK(f.u8(0x1c+0x18)==100&&f.f32(0x1c+8)==5);
        v.putf(0x1c,-5);course_environment_progress_44b020(lists,0,v,flags,primary,c);
        CHECK(f.u8(0x1c+0x18)==0&&f.f32(0x1c+8)==0); // behind the start: t=0
        // Sun lane, fixed record: leaf stores and balanced matrix stack.
        lists={fl,Bytes(nullptr,0),Bytes(nullptr,0)};v.put16(0x64,5);
        e.putf(0x30,4);e.putf(0x60,0);e.putf(0x64,0);e.putf(0x68,0.5f);e.putf(0x6c,77);
        course_environment_progress_44b020(lists,1,v,flags,primary,c);
        CHECK(s.f32(8)==4&&s.f32(0x14)==1&&s.f32(0x8c)==0.5f&&s.f32(0x90)==77&&s.f32(0x4c)==-1);
        CHECK(matrices.depth==0&&matrices.current_offset==0);
        bool bad_lane=false;try{course_environment_progress_44b020(lists,2,v,flags,primary,c);}catch(const std::exception&){bad_lane=true;}
        CHECK(bad_lane);
    }
    std::puts("environment snapshot/interpolation/local lights/progression: PASS");
}
