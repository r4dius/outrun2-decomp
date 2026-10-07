#include "platform/race_robots.hpp"
#include "enhancements/frame_rate.hpp"
#include "platform/race_translated.hpp"
#include "driving/pc_matrix_stack.hpp"
#include "driving/pc_x87.hpp"
#include <array>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <stdexcept>
namespace outrun::platform {
PcRaceRobotState::PcRaceRobotState():data(RaceRobotsDataImage,RaceRobotsDataImage+(DataEnd-DataBase)){}
void race_robots_map_tables(PcRaceMemory& m){
    for(const auto& t:RaceRobotTables)m.map_const(t.base,t.data,t.size);
}
namespace {
using driving::Bytes;
using driving::CourseProbe;
using driving::X87;
constexpr std::uint32_t F0=0x00000000u,F4=0x40800000u,F10=0x41200000u,F20=0x41a00000u,F85=0x42aa0000u;
std::uint32_t bits(float f){std::uint32_t u;std::memcpy(&u,&f,4);return u;}
float bits_to_float(std::uint32_t u){float f;std::memcpy(&f,&u,4);return f;}
// CVTTSS2SI: truncation, the integer indefinite value when out of range/NaN.
std::int32_t cvtt(float f){
    if(!(f>-2147483904.0f&&f<2147483648.0f))return std::int32_t(0x80000000u);
    return std::int32_t(f);
}
struct Port {
    PcRaceContext& c;
    PcRaceMemory& m;
    explicit Port(PcRaceContext& cc):c(cc),m(cc.m){}
    std::uint32_t call(std::uint32_t pc,std::initializer_list<std::uint32_t> args,std::uint32_t ecx=0){
        PcRaceCall k{};k.pc=pc;k.ecx=ecx;k.argc=std::uint32_t(args.size());
        unsigned i=0;for(auto a:args)k.args[i++]=a;
        if(!c.service)throw std::logic_error("race robots: no service for PC call");
        return c.service(k);
    }
    std::uint32_t car(){return m.u32(0x799d18u);}
    std::uint32_t motion(std::uint32_t work){return PcRaceRobotState::MotionBase+m.u32(work)*PcRaceRobotState::MotionSize;}
    static std::uint32_t model(std::uint8_t b){return std::uint32_t(std::int32_t(std::int8_t(b)));}   // movsx
    std::uint32_t model_of(std::uint32_t car){return model(m.u8(car+0x11));}
    // ---- math leaves ------------------------------------------------------
    void unit_40a060(std::uint32_t a){
        for(unsigned k:{0x38u,0x34u,0x30u,0x2cu,0x24u,0x20u,0x1cu,0x18u,0x10u,0x0cu,0x08u,0x04u})m.put32(a+k,0);
        for(unsigned k:{0x3cu,0x28u,0x14u,0x00u})m.put32(a+k,0x3f800000u);
    }
    CourseProbe vec(std::uint32_t a){return {m.f32(a),m.f32(a+4),m.f32(a+8)};}
    void put_vec(std::uint32_t a,const CourseProbe& v){m.putf(a,v.x);m.putf(a+4,v.y);m.putf(a+8,v.z);}
    static CourseProbe sub(const CourseProbe& a,const CourseProbe& b){  // 40EFA0 / 40EF70 (x87, spilled per lane)
        return {driving::x87_float(X87(a.x)-X87(b.x)),driving::x87_float(X87(a.y)-X87(b.y)),driving::x87_float(X87(a.z)-X87(b.z))};
    }
    static X87 dot(const CourseProbe& a,const CourseProbe& b){           // 40EFD0: (z*z' + y*y') + x*x'
        return (X87(a.z)*X87(b.z)+X87(a.y)*X87(b.y))+X87(a.x)*X87(b.x);
    }
    static X87 length(const CourseProbe& a,const CourseProbe& b){        // 40F140: sqrt((dz^2+dx^2)+dy^2)
        const X87 dx=X87(a.x)-X87(b.x),dy=X87(a.y)-X87(b.y),dz=X87(a.z)-X87(b.z);
        return driving::x87_sqrt((dz*dz+dx*dx)+dy*dy);
    }
    // ---- matrix leaves on the native 89B564 stack ---------------------------
    void push(){driving::pc_matrix_push(c.matrices);}
    void push_unit(){driving::pc_matrix_push_unit(c.matrices);}
    void push_load(std::uint32_t a){driving::pc_matrix_push_load(c.matrices,m.bytes(a,64));}
    void pop(){driving::pc_matrix_pop(c.matrices);}
    void load(std::uint32_t a){driving::pc_matrix_load(c.matrices,m.bytes(a,64));}
    void get(std::uint32_t a){driving::pc_matrix_get(c.matrices,m.bytes(a,64));}
    void multiply(std::uint32_t a){driving::pc_matrix_multiply_current(c.matrices,m.bytes(a,64));}
    void translate(const CourseProbe& v){driving::pc_matrix_translate_vector(c.matrices,v);}
    // ---- RobMotion accessors (4F1xxx) ----------------------------------------
    bool motion_end_4f1e70(std::uint32_t mot){return ((m.u32(mot+8)>>2)&1u)!=0u;}
    std::uint32_t mot_name_4f1f00(std::uint32_t mot){return m.u32(mot+0xa0);}
    std::uint32_t get_mot_name_4f1f10(std::uint32_t id){
        const std::uint32_t table=m.u32(0x84d970u);
        const std::uint32_t list=m.u32(((id>>16)<<4)+table+4u);
        return m.u32(list+(id&0xffffu)*4u);
    }
    std::uint32_t get_matrix_4f1ec0(std::uint32_t mot,std::uint32_t id){
        const std::int32_t n=std::int16_t(m.u16(mot+0x68));
        const std::uint32_t i=id&0x7fffu;
        if(!(n>std::int32_t(i)))return 0;
        if(id&0x8000u)return 0;
        return m.u32(mot+0x74)+i*0x350u;
    }
    // Inline strcmp of 488820: 0 when the two strings are equal.
    bool same_string(std::uint32_t a,std::uint32_t b){
        for(;;){const std::uint8_t x=m.u8(a),y=m.u8(b);if(x!=y)return false;if(!x)return true;++a;++b;}
    }
    // 46BC10 GetCharPosition(car, n): the per-model position table entry.
    std::uint32_t char_position_46bc10(std::uint32_t car,std::uint32_t n){
        return m.u32(0x5b2f68u+model_of(car)*4u)+(n<<4)+0x108u;
    }
    // 45B380(model byte): 1 for the model classes without the jump-table
    // "0" entry (from the 45B3AC byte table), else 0.
    static std::uint32_t model_flag_45b380(std::uint32_t arg){
        static constexpr std::uint8_t table[0x17]={0,0,1,0,1,1,0,0,1,1,0,1,1,0,1,0,0,1,1,1,1,1,0};
        const std::uint32_t n=model(std::uint8_t(arg))-2u;
        if(n>0x16u)return 0;
        return table[n]==0?1u:0u;
    }
};
}
// ---- CRT qsort 580CB0 with the comparator 487920 ------------------------------
// 487920(a,b): -1 when *a < *b (unsigned), else 0 (never positive).
void pc_crt_qsort_580cb0(PcRaceMemory& m,std::uint32_t base,std::uint32_t num,std::uint32_t width){
    auto comp=[&](std::uint32_t a,std::uint32_t b)->int{return m.u32(a)<m.u32(b)?-1:0;};
    auto swap=[&](std::uint32_t a,std::uint32_t b){
        if(a==b)return;
        for(std::uint32_t k=0;k<width;++k){const std::uint8_t x=m.u8(a+k),y=m.u8(b+k);m.put8(b+k,x);m.put8(a+k,y);}
    };
    if(num<2u||width==0u)return;
    std::array<std::uint32_t,30> lostk{},histk{};int stkptr=0;
    std::uint32_t lo=base,hi=base+(num-1u)*width;
    for(;;){
        const std::uint32_t size=(hi-lo)/width+1u;
        if(size<=8u){
            // 580C40 shortsort
            std::uint32_t h=hi;
            while(h>lo){
                std::uint32_t max=lo;
                for(std::uint32_t p=lo+width;p<=h;p+=width)if(comp(p,max)>0)max=p;
                if(width)swap(max,h);
                h-=width;
            }
        }else{
            std::uint32_t mid=lo+(size/2u)*width;
            if(comp(lo,mid)>0)swap(lo,mid);
            if(comp(lo,hi)>0)swap(lo,hi);
            if(comp(mid,hi)>0)swap(mid,hi);
            std::uint32_t loguy=lo,higuy=hi;
            for(;;){
                if(mid>loguy){
                    do{loguy+=width;}while(loguy<mid&&comp(loguy,mid)<=0);
                }
                if(mid<=loguy){
                    do{loguy+=width;}while(loguy<=hi&&comp(loguy,mid)<=0);
                }
                do{higuy-=width;}while(higuy>mid&&comp(higuy,mid)>0);
                if(higuy<loguy)break;
                swap(loguy,higuy);
                if(mid==higuy)mid=loguy;
            }
            higuy+=width;
            if(mid<higuy){
                do{higuy-=width;}while(higuy>mid&&comp(higuy,mid)==0);
            }
            if(mid>=higuy){
                do{higuy-=width;}while(higuy>lo&&comp(higuy,mid)==0);
            }
            if(std::int32_t(higuy-lo)>=std::int32_t(hi-loguy)){
                if(lo<higuy){lostk.at(std::size_t(stkptr))=lo;histk.at(std::size_t(stkptr))=higuy;++stkptr;}
                if(loguy<hi){lo=loguy;continue;}
            }else{
                if(loguy<hi){lostk.at(std::size_t(stkptr))=loguy;histk.at(std::size_t(stkptr))=hi;++stkptr;}
                if(lo<higuy){hi=higuy;continue;}
            }
        }
        --stkptr;
        if(stkptr<0)return;
        lo=lostk[std::size_t(stkptr)];hi=histk[std::size_t(stkptr)];
    }
}
// ---- module ---------------------------------------------------------------------
void race_robot_common_init_487810(PcRaceContext& c,std::uint32_t work){
    Port p(c);auto& m=p.m;
    const std::uint32_t first=m.u32(0x79f010u);   // bridge 4480ED: EAX = [79F010] (event 362 work)
    const std::uint32_t index=std::uint32_t((std::uint64_t(work-first)*0x38e38e39u)>>32)>>5;   // /0x90
    m.put32(work,index);
    const std::uint32_t mot=PcRaceRobotState::MotionBase+index*PcRaceRobotState::MotionSize;
    m.put32(mot+0x18,0x3f800000u);                  // 4ED890 SetSpeed(1.0)
    p.unit_40a060(work+0x10);
    for(unsigned k:{0x68u,0x6cu,0x70u,0x74u})m.put32(work+k,0xffffffffu);
    m.put32(work+4,0x2800cu);
    m.put32(work+0x78,0);m.put32(work+0x7c,0);m.put32(work+0x80,0);
    m.put8(work+0x84,0);m.put8(work+0x85,0);
}
std::uint32_t race_robot_set_chara_487c30(PcRaceContext& c,std::uint32_t work,std::uint32_t chara){
    Port p(c);auto& m=p.m;
    const std::uint32_t value=std::uint32_t(std::int32_t(std::int8_t(std::uint8_t(chara))));  // bridge 103C50C: movsx eax,[esp+8]
    m.put32(work+8,value);
    const std::uint32_t index=m.u32(work);
    const std::uint32_t mot=PcRaceRobotState::MotionBase+index*PcRaceRobotState::MotionSize;
    m.put32(mot,index);                             // 4F1CE0 SetBoneID(index)
    const std::uint32_t n=m.u32(work+8)-3u;
    static constexpr std::uint8_t cases[0x11]={0,0,1,1,1,1,1,1,2,2,3,3,3,4,2,2,1};
    std::uint32_t kind;
    switch(n>0x10u?2u:cases[n]){
    case 0:p.call(0x440d50u,{1});kind=4;break;
    case 1:p.call(0x440d50u,{0});kind=2;break;
    case 3:p.call(0x440d50u,{0});kind=0;break;
    case 4:p.call(0x440d50u,{0});kind=5;break;
    default:p.call(0x440d50u,{0});kind=3;break;
    }
    const std::uint32_t bones=m.u32(0x84d974u);
    p.call(0x4f1cf0u,{(kind<<4)+bones,bones},mot);  // SetBone
    p.call(0x514bf0u,{work});                        // rob_disp_init
    p.call(0x514f60u,{work,mot});                    // rob_osage_init
    p.call(0x440d70u,{});
    return 1;
}
std::uint32_t race_robot_driver_chara_487ee0(PcRaceContext& c){
    Port p(c);auto& m=p.m;
    if(m.u32(0x78026cu)==3u)return 0xc;
    if((p.call(0x43f860u,{})&0xffu)==0u)return 0xb;
    // [esp + [7C2400]*4] over the stack list {0,1,2,4,3,5}.
    const std::uint32_t i=m.u32(0x7c2400u);
    static constexpr std::uint32_t list[6]={0,1,2,4,3,5};
    if(i>=6u)throw std::out_of_range("race robots: 487EE0 index [7C2400] outside its 6-entry stack list");
    return m.u32(list[i]*4u+0x6549b0u);
}
bool race_robot_hand_motion_487940(const PcRaceMemory& m,std::uint32_t id){
    std::int32_t lo=-1,hi=0x22,mid;
    do{
        mid=(hi+lo)>>1;
        if(id<=m.u32(std::uint32_t(mid)*4u+0x6546c0u))hi=mid;else lo=mid;
    }while(hi-lo>1);
    return id==m.u32(std::uint32_t(mid)*4u+0x6546c0u);
}
void race_robot_motion_connect_487b70(PcRaceContext& c,std::uint32_t work,std::uint32_t id,std::uint32_t loop){
    Port p(c);const std::uint32_t mot=p.motion(work);
    p.call(0x4f2320u,{id,F0,F10},mot);              // SetMotionConnect(id, 0, 10)
    p.call(0x4f1e60u,{loop},mot);                   // SetLoop(loop)
}
void race_robot_control_488450(PcRaceContext& c,std::uint32_t work){
    Port p(c);auto& m=p.m;
    if(m.u32(work+4)&0x10000u)return;
    const std::uint32_t mot=p.motion(work);
    p.call(0x4f2520u,{},mot);                        // Calc
    const std::uint32_t f=m.u32(work+4);
    if(f&0x20000u)m.put32(work+4,f&0xfffdffffu);
    if(m.u32(0x85a714u)==0u)p.call(0x515040u,{work,mot});   // rob_osage_ctrl
    for(unsigned k=0;k<12;k+=4)m.put32(work+0x5c+k,m.u32(work+0x50+k));
    for(unsigned k=0;k<12;k+=4)m.put32(work+0x50+k,m.u32(work+0x40+k));
    p.call(0x5147d0u,{work});                        // rob_disp_ctrl
}
void race_robot_display_487880(PcRaceContext& c,std::uint32_t work){
    Port p(c);auto& m=p.m;
    const std::uint32_t car=p.car();
    const std::uint32_t light=p.call(0x4082b0u,{2,0,0});   // GetLightWorkAddress
    const float scale=m.f32(car+0x58);
    const float l8=m.f32(light+8);
    const std::uint32_t s8=m.u32(light+8),sc=m.u32(light+0xc),s10=m.u32(light+0x10),s14=m.u32(light+0x14);
    m.putf(light+8,l8*scale);
    m.putf(light+0xc,scale*m.f32(light+0xc));
    m.putf(light+0x10,scale*m.f32(light+0x10));
    p.call(0x40d840u,{2});                           // SceneEnvironment
    p.call(0x514e60u,{work});                        // rob_disp_disp
    m.put32(light+8,s8);m.put32(light+0xc,sc);m.put32(light+0x10,s10);m.put32(light+0x14,s14);
    p.call(0x40d840u,{2});
}
void race_robot_driver_display_4879b0(PcRaceContext& c,std::uint32_t work){
    Port p(c);
    if((p.m.u32(p.car()+4)&0xc000u)==0x4000u)race_robot_display_487880(c,work);
}
void race_robot_flagman_display_4879d0(PcRaceContext& c,std::uint32_t work){
    Port p(c);
    race_robot_display_487880(c,work);
    if(p.m.u32(work+4)&0x8000u){
        p.push_unit();
        p.call(0x409e00u,{6});                       // mxSetD3DTransform
        p.call(0x402170u,{});                        // flagman_cloth_disp
        p.pop();
    }
}
void race_robot_driver_destroy_487980(PcRaceContext& c,std::uint32_t work){
    Port p(c);
    p.call(0x4f2260u,{},p.motion(work));             // UnsetBone
    p.call(0x514e80u,{work});
}
void race_robot_flagman_destroy_487a00(PcRaceContext& c,std::uint32_t work){
    race_robot_driver_destroy_487980(c,work);
    Port(c).call(0x4021a0u,{});                      // flagman_cloth_dest
}
void race_robot_driver_init_4884d0(PcRaceContext& c,std::uint32_t work){
    Port p(c);auto& m=p.m;
    const std::uint32_t car=p.car();
    race_robot_common_init_487810(c,work);
    race_robot_set_chara_487c30(c,work,race_robot_driver_chara_487ee0(c));
    const std::uint32_t mot=p.motion(work);
    p.call(0x4f2280u,{m.u32(p.model_of(car)*4u+0x5c0b80u)},mot);   // SetMotion
    p.call(0x4f1d30u,{F0},mot);                      // SetFrame(0)
    p.call(0x4f2520u,{},mot);                        // Calc
    p.call(0x4f1d30u,{F85},mot);                     // SetFrame(85)
    p.call(0x4f1e60u,{0},mot);                       // SetLoop(0)
    m.put32(0x82f588u,8);m.put16(0x82f58cu,0);m.put16(0x82f58eu,0);m.put32(0x82f59cu,0);
    m.put8(0x82f2f8u,0);m.put8(0x82f2f9u,0);
    pc_crt_qsort_580cb0(m,0x6546c0u,0x22,4);
    if(race_robot_hand_motion_487940(m,m.u32(mot+4)))p.call(0x5148d0u,{work});   // set_hand_gu
    else p.call(0x5148f0u,{work});                                                // set_hand_pa
}
void race_robot_flagman_init_488710(PcRaceContext& c,std::uint32_t work){
    Port p(c);auto& m=p.m;
    const std::uint32_t preset=m.u32(0x78024cu);
    race_robot_common_init_487810(c,work);
    race_robot_set_chara_487c30(c,work,preset==1u||preset==3u?3u:4u);
    const std::uint32_t e=preset*12u;
    m.put32(0x82f11cu,preset*24u+0x654760u);
    const std::uint32_t motion_id=m.u32(preset*4u+0x6547d8u);
    m.put32(0x82f2fcu,m.u32(e+0x5c1564u));
    const std::uint32_t y=m.u32(e+0x5c1568u),z=m.u32(e+0x5c156cu);
    m.put32(0x82f304u,z);
    m.put32(0x82f378u,m.u32(e+0x5c15dcu));
    const std::uint32_t ry=m.u32(e+0x5c15e0u),rz=m.u32(e+0x5c15e4u);
    m.put32(0x82f300u,y);m.put32(0x82f5d4u,motion_id);m.put32(0x82f5e4u,0);m.put32(0x82f5e8u,0);
    m.put32(0x82f37cu,ry);m.put32(0x82f380u,rz);
    const std::uint32_t mot=p.motion(work);
    p.call(0x4f2280u,{motion_id},mot);               // SetMotion
    p.call(0x4f1e60u,{0},mot);                       // SetLoop(0)
    p.push_unit();
    p.translate(p.vec(0x82f2fcu));
    driving::pc_matrix_rotate_z(c.matrices,m.f32(0x82f380u));
    driving::pc_matrix_rotate_y(c.matrices,m.f32(0x82f37cu));
    driving::pc_matrix_rotate_x(c.matrices,m.f32(0x82f378u));
    p.get(work+0x10);
    p.pop();
    p.call(0x402100u,{});                            // flagman_cloth_init
}
std::uint32_t race_robot_flagman_runaway_487a30(PcRaceContext& c){
    Port p(c);auto& m=p.m;
    const CourseProbe near_axis{0.0f,0.0f,m.f32(0x6280c4u)},far_point{0.0f,0.0f,m.f32(0x6282a4u)};
    std::uint32_t result=0;
    const std::uint32_t first=p.car();
    p.push();
    for(std::uint32_t i=0;i<4;++i){
        if(!(m.u8(0x79fb50u+i)&3u))continue;             // CAR01..CAR04 open
        const std::uint32_t car=first+i*0x10f0u;
        const float distance=driving::x87_float(Port::length(p.vec(car+0x14),p.vec(0x82f2fcu)));
        const float limit=m.f32(0x654858u);
        if(!(distance>limit))continue;
        p.load(car+0xb0);
        const CourseProbe axis=driving::pc_matrix_vector(c.matrices,near_axis);    // 40A820
        const CourseProbe point=driving::pc_matrix_point(c.matrices,far_point);    // 40A7D0
        CourseProbe dir=Port::sub(p.vec(0x82f2fcu),point);                         // 40EFA0
        driving::pc_unit_vector_40eeb0(dir);
        const X87 cosine=Port::dot(dir,axis);
        const X87 ratio=driving::x87_sqrt(X87(distance)*X87(distance)-X87(limit)*X87(limit))/X87(distance);
        if(!(cosine>ratio))continue;
        if(m.f32(0x654854u)>distance)result=1;
    }
    p.pop();
    return result;
}
void race_char_grip_46bc30(PcRaceContext& c,std::uint32_t car,float angle,std::uint32_t a,std::uint32_t b){
    Port p(c);auto& m=c.m;
    const std::uint32_t t=m.u32(0x5b2f68u+p.model_of(car)*4u);
    const float r=driving::x87_float(X87(bits_to_float(0x3d23d70au))+X87(m.f32(t+0x5c)));   // 64D19C 0.04
    const X87 a0=X87(bits_to_float(0x3ea0d97cu)),b0=X87(bits_to_float(0x4034f4acu));
    m.putf(a,driving::x87_float(driving::x87_cos(a0)*X87(r)));
    m.putf(a+8,0.0f);m.putf(a+4,driving::x87_float(driving::x87_sin(a0)*X87(r)));
    m.putf(b,driving::x87_float(driving::x87_cos(b0)*X87(r)));
    m.putf(b+8,0.0f);m.putf(b+4,driving::x87_float(driving::x87_sin(b0)*X87(r)));
    p.push_unit();
    CourseProbe pos=p.vec(t+0x4c);
    pos.z=pos.z+bits_to_float(0x3d4ccccdu);                                               // 628128 0.05 (ADDSS)
    p.translate(pos);
    driving::pc_matrix_rotate_x(c.matrices,m.f32(t+0x58));
    driving::pc_matrix_rotate_z(c.matrices,angle);
    p.put_vec(a,driving::pc_matrix_point(c.matrices,p.vec(a)));
    p.put_vec(b,driving::pc_matrix_point(c.matrices,p.vec(b)));
    p.pop();
}
void race_robot_driver_matrix_488af0(PcRaceContext& c,std::uint32_t work,std::uint32_t position,std::uint32_t car){
    Port p(c);auto& m=p.m;
    // VM bridge at 488AF3 (measured): EAX = ESI + 0xB0, all else unchanged.
    p.push_load(car+0xb0);
    p.multiply(car+0xf0);
    const std::uint32_t at=p.char_position_46bc10(car,position);
    std::uint32_t x=m.u32(at),y=m.u32(at+4);const std::uint32_t z=m.u32(at+8);
    if(m.u32(0x7f9460u+0x60u)!=0u&&(m.u8(car+4)&1u)){   // 55A930 (ECX 7F9460): [7F94C0]
        float fy;std::memcpy(&fy,&y,4);
        fy=float(m.i32(car+0xd18))*m.f32(0x5a91bcu)+fy;
        y=bits(fy);
    }
    CourseProbe v;std::memcpy(&v.x,&x,4);std::memcpy(&v.y,&y,4);std::memcpy(&v.z,&z,4);
    p.translate(v);
    p.get(work+0x10);
    p.pop();
    // Port enhancement (enhancements/frame_rate.hpp): the character rides the
    // car drawn between ticks; its matrix is rebuilt from the replayed +B0.
    if(enhancements::display_frames()){
        const driving::Bytes body=m.bytes(car+0xb0,0x80),out=m.bytes(work+0x10,64);
        auto tick=std::make_shared<std::array<std::uint8_t,64>>();std::memcpy(tick->data(),out.data(),64);
        enhancements::display_note(out.data(),[body,out,v]{
            auto& s=enhancements::display_matrices();
            driving::pc_matrix_push_load(s,body.sub(0,64));
            driving::pc_matrix_multiply_current(s,body.sub(0x40,64));
            driving::pc_matrix_translate_vector(s,v);
            driving::pc_matrix_get(s,out);
            driving::pc_matrix_pop(s);
        },[out,tick]{std::memcpy(out.data(),tick->data(),64);});
    }
}
void race_robot_driver_control_488dd0(PcRaceContext& c,std::uint32_t work){
    Port p(c);auto& m=p.m;
    const std::uint32_t mot=p.motion(work);
    const std::uint32_t mode=m.u32(0x780258u);
    const std::uint32_t car=p.car();
    const std::uint32_t table=(p.model_of(car)<<4)+0x5c0c70u;
    std::uint32_t r45=Port::model_flag_45b380(m.u8(car+0x11));
    if(m.u32(car+0x5c)==0u)m.put32(0x82f59cu,0);
    race_robot_driver_matrix_488af0(c,work,0,car);
    const std::int32_t steer=std::int16_t(m.u16(car+0x204));
    const float frame=float(std::int32_t(steer+0x7fff))*m.f32(0x5b4320u)*m.f32(0x5c1628u);
    {   // 46BC30 GetHandleGripPositionLocal(car, -(float)(int)(steer*k)*c, A, B) into the
        // two stack vectors, then both minus the offset driver position (40EF70).
        const std::int32_t angle=cvtt(float(steer)*m.f32(0x5c1624u));
        const float arg=driving::x87_float(-(X87(angle)*X87(m.f32(0x628254u))));
        std::array<std::uint8_t,16> a{},b{};
        const std::size_t mark=m.mark();
        m.map(PcRobotLocal488dd0A,a.data(),a.size());m.map(PcRobotLocal488dd0B,b.data(),b.size());
        try{
            p.call(0x46bc30u,{car,bits(arg),PcRobotLocal488dd0A,PcRobotLocal488dd0B});
            const std::uint32_t at=p.char_position_46bc10(car,0);
            CourseProbe base=p.vec(at);
            base.y=m.f32(0x65474cu)+base.y;
            base.z=m.f32(0x654750u)+base.z;
            p.put_vec(PcRobotLocal488dd0B,Port::sub(p.vec(PcRobotLocal488dd0B),base));
            p.put_vec(PcRobotLocal488dd0A,Port::sub(p.vec(PcRobotLocal488dd0A),base));
        }catch(...){m.release(mark);throw;}
        m.release(mark);
    }
    const std::uint32_t state=m.u32(0x82f588u);
    const std::uint32_t observer=m.u8(0x79fcafu)&3u;     // event 359 flags
    auto connect=[&](std::uint32_t id,std::uint32_t loop){race_robot_motion_connect_487b70(c,work,id,loop);};
    auto state_a=[&]{
        if(state==8u){
            if(std::int16_t(p.call(0x49b2d0u,{}))>0x77)return;
            m.put32(0x82f588u,1);return;
        }
        if(observer==2u){
            if(p.call(0x44ff10u,{})){
                if(m.u32(0x82f588u)!=9u){m.put32(0x82f588u,9);m.put32(0x82f590u,0);}
                return;
            }
            if(p.call(0x44fe70u,{})!=0u||p.call(0x450240u,{})==4u){
                if(m.u32(0x82f588u)==0xau)return;
                if(mode==1u)connect(0x60000u,0xffffffffu);
                else if(mode==2u)connect(0x6003cu,0xffffffffu);
                else if(mode==0u||mode==7u)connect(0x60044u,0xffffffffu);
                else if(mode==3u||mode==4u){
                    std::uint32_t rank=p.call(0x45a2b0u,{std::uint32_t(m.u8(0x7dd138u))})&0xffu;   // 455AD0 -> 45A2B0
                    if(rank==0u)connect(m.u32(p.model_of(car)*4u+0x5c1488u),0xffffffffu);
                    else{
                        if(rank>3u)rank=3;
                        const std::int32_t k=std::int8_t(m.u8(0x7df10bu))+1;                          // 455C10
                        const std::uint32_t alt=(k==1||k==4)?0u:1u;
                        connect(m.u32((rank+alt*4u)*4u+(r45?0x5c1468u:0x5c1448u)),0xffffffffu);
                    }
                }
                m.put32(0x82f588u,0xa);
                return;
            }
        }
        const std::int32_t pending=m.i32(work+0x68);
        if(pending>0){
            p.call(0x4f2320u,{std::uint32_t(pending),F0,F20},mot);
            p.call(0x4f1e60u,{0},mot);
            m.put32(work+0x68,0xfffffffeu);
            return;
        }
        if(pending==-2){
            if(p.motion_end_4f1e70(mot))m.put32(work+0x68,0xffffffffu);
            return;
        }
        if(m.f32(car+0x2f8)>m.f32(0x619a34u)){
            const std::uint32_t s=m.u32(0x82f588u);
            if(s==6u||s==7u)return;
            const std::uint32_t k=((m.u32(car+0x2f0)>>2)&0x1fu)-1u;
            static constexpr std::uint8_t cases[0xf]={0,2,0,0,2,0,0,2,2,2,2,2,2,2,1};
            const bool second=k>0xeu||cases[k]==2u;
            const std::uint32_t mode2=m.u32(0x780258u);
            m.put32(0x82f588u,second?6u:7u);
            if(mode2==0u||mode==7u){connect(0x6005au,0);return;}
            const std::uint32_t b=Port::model(second?m.u8(0x82f2f9u):m.u8(0x82f2f8u));
            const std::uint32_t tab=second?(mode2==2u?0x5c0e50u:0x5c0e60u):(mode2==2u?0x5c0e70u:0x5c0e80u);
            connect(m.u32((b+r45*2u)*4u+tab),0);
            return;
        }
        if(p.call(0x44bce0u,{})!=0u&&m.u32(0x82f588u)!=0xdu&&m.u32(0x82f59cu)==0u){
            const std::int32_t r=std::int32_t(p.call(0x44bd20u,{}));
            if(r<0)return;
            std::int32_t id;
            if(m.u32(0x780258u)!=0u&&mode!=7u)id=m.i32(m.u32(std::uint32_t(r)*4u+0x654688u)+p.model_of(car)*4u);
            else id=m.i32(p.model_of(car)*4u+0x5c1270u);
            if(id>=0){connect(std::uint32_t(id),0);m.put32(0x82f588u,0xd);}
            m.put32(0x82f59cu,1);
            return;
        }
        if(m.u32(0x82f588u)!=0xcu){
            const std::uint32_t mode2=m.u32(0x780258u);
            if((mode2==0u||mode2==7u)&&r45==0u&&((m.u32(0x7d39f0u)>>3)&1u)){      // 450160
                const std::uint32_t v=m.u32(0x7d3944u);r45=v;                          // 450650 writes [esp+0x10]
                const std::uint32_t elapsed=m.u32(0x7d387cu);                          // 450580
                const std::uint32_t lap=p.call(0x451350u,{v});
                const std::uint32_t limit=p.call(0x47fbd0u,{v,3,lap});
                if(elapsed>=limit)return;
                p.call(0x580f33u,{p.call(0x417f70u,{})});                              // srand(seed)
                const std::int32_t r=std::int32_t(p.call(0x580f40u,{}));              // rand()
                const std::int32_t i=cvtt(float(r)*m.f32(0x5c1620u));
                p.call(0x4f2320u,{m.u32(std::uint32_t(i)*4u+0x5c0e90u),F0,F10},mot);
                p.call(0x4f1e60u,{0},mot);
                m.put32(0x82f588u,0xc);
                return;
            }
        }
        const std::uint32_t s=m.u32(0x82f588u);
        auto count=[&]{
            if(frame>m.f32(0x5c161cu)&&m.f32(0x5c1618u)>frame)m.put16(0x82f58cu,std::uint16_t(m.u16(0x82f58cu)+1u));
        };
        if(s==1u){
            if(std::int16_t(m.u16(0x82f58cu))>0x78&&m.i32(table)>=0){
                const std::uint32_t id=m.u32(table);
                m.put32(0x82f588u,2);connect(id,0);m.put16(0x82f58cu,0);return;
            }
            count();return;
        }
        if(s==2u){
            if(p.motion_end_4f1e70(mot)){
                m.put32(0x82f588u,4);
                p.call(0x4f2320u,{m.u32(table+8),bits(frame),F10},mot);
                p.call(0x4f1e60u,{0},mot);
            }
            return;
        }
        if(s==4u){
            if(std::int16_t(m.u16(0x82f58cu))>0x78){
                const std::uint32_t id=m.u32(table+0xc);
                m.put32(0x82f588u,5);connect(id,0);m.put16(0x82f58cu,0);return;
            }
            count();return;
        }
        if(s==5u||s==6u||s==7u||s==0xcu){
            if(p.motion_end_4f1e70(mot)){
                const std::uint32_t id=m.u32(p.model_of(car)*4u+0x5c0b80u);
                m.put32(0x82f588u,1);
                p.call(0x4f2320u,{id,bits(frame),F10},mot);
                p.call(0x4f1e60u,{0},mot);
            }
        }
    };
    state_a();
    // Bridge 40EADB: EAX = [82F588].
    const std::uint32_t k=m.u32(0x82f588u)-1u;
    static constexpr std::uint8_t cases[0xd]={0,4,4,1,4,4,4,0,2,4,4,4,3};
    switch(k>0xcu?4u:cases[k]){
    case 0:case 1:p.call(0x4f1d30u,{bits(frame)},mot);break;       // SetFrame(frame)
    case 2:{
        const std::uint32_t sub=m.u32(0x82f590u);
        auto countdown=[&]{m.put32(0x82f594u,m.u32(0x82f594u)-1u);};
        switch(sub){
        case 0:{
            const std::uint32_t row=m.u32(p.model_of(car)*4u+0x5c0f00u);
            std::uint32_t entry;
            if(p.call(0x44dc50u,{p.call(0x450380u,{8})})==0x1bu)entry=row*8u+0x5c0ebcu;
            else if(p.call(0x44dc50u,{p.call(0x450380u,{8})})==0x1au)entry=row*8u+0x5c0edcu;
            else entry=p.call(0x44dc50u,{p.call(0x450380u,{8})})==0x1cu?row*8u+0x5c0edcu:row*8u+0x5c0e9cu;
            m.put32(0x82f590u,1);m.put32(0x82f598u,entry);
            break;}
        case 1:case 5:{
            const std::uint32_t rec=m.u32(m.u32(0x82f598u)+(sub==1u?0u:4u));
            m.put32(0x82f594u,sub==1u?0xf0u:0x168u);
            if(rec==0u){m.put32(0x82f590u,sub==1u?4u:8u);break;}
            const std::int32_t id=m.i32(rec);
            if(id<0){m.put32(0x82f594u,0xffffffffu);m.put32(0x82f590u,sub==1u?2u:6u);break;}
            connect(std::uint32_t(id),0);
            m.put32(0x82f590u,sub==1u?2u:6u);
            break;}
        case 2:case 6:{
            const std::uint32_t entry=m.u32(0x82f598u)+(sub==2u?0u:4u);
            if(!p.motion_end_4f1e70(mot)){
                const std::int32_t t=m.i32(0x82f594u);
                if(t>=0){
                    if(t!=0){countdown();break;}
                    const std::int32_t next=m.i32(m.u32(entry)+8);
                    m.put32(0x82f590u,(sub==2u?3u:7u)+(next<0?1u:0u));
                    break;
                }
            }
            connect(m.u32(m.u32(entry)+4),0xffffffffu);
            m.put32(0x82f594u,sub==2u?0xf0u:0x168u);
            break;}
        case 3:
            connect(m.u32(m.u32(m.u32(0x82f598u))+8),0);
            m.put32(0x82f590u,4);m.put32(0x82f594u,0x78);
            break;
        case 4:{
            const std::int32_t t=m.i32(0x82f594u);
            m.put32(0x82f594u,std::uint32_t(t-1));
            if(t>=0&&!p.motion_end_4f1e70(mot))break;
            m.put32(0x82f590u,5);
            break;}
        case 7:
            connect(m.u32(m.u32(m.u32(0x82f598u)+4)+8),0);
            m.put32(0x82f590u,8);
            break;
        default:break;
        }
        break;}
    case 3:
        if(p.motion_end_4f1e70(mot)||m.u32(car+0x5c)!=1u){
            connect(m.u32(p.model_of(car)*4u+0x5c0b80u),0);
            m.put32(0x82f588u,1);
        }
        break;
    default:break;
    }
    if(!(m.u8(mot+8)&0x20u)){
        if(race_robot_hand_motion_487940(m,m.u32(mot+4)))p.call(0x5148d0u,{work});
        else p.call(0x5148f0u,{work});
    }
    race_robot_control_488450(c,work);
}
void race_robot_flagman_control_488820(PcRaceContext& c,std::uint32_t work){
    Port p(c);auto& m=p.m;
    const std::uint32_t preset=m.u32(0x78024cu);
    const std::uint32_t camera=m.u32(0x79f574u);        // event 385 work
    const std::uint32_t mot=p.motion(work);
    const std::uint32_t pos=work+0x40,eye=camera+0xf8;
    const X87 distance=Port::length(p.vec(eye),p.vec(pos));
    if(distance>X87(m.f32(0x654850u))){m.put32(work+4,m.u32(work+4)&0xffff7fffu);return;}
    m.put32(work+4,m.u32(work+4)|0x8000u);
    const CourseProbe view=Port::sub(p.vec(camera+0x104),p.vec(eye));
    const CourseProbe to=Port::sub(p.vec(pos),p.vec(eye));
    if(X87(m.f32(0x619a34u))>Port::dot(view,to))return;
    if(std::int16_t(p.call(0x49b2d0u,{}))>0x77)p.call(0x4f1d30u,{F0},mot);
    auto same_motion=[&]{return p.same_string(p.mot_name_4f1f00(mot),p.get_mot_name_4f1f10(m.u32(preset*4u+0x6547d8u)));};
    if(p.motion_end_4f1e70(mot)&&m.u32(0x82f5e4u)==0u){
        const std::uint32_t list=m.u32(0x82f11cu),i=m.u32(0x82f5e8u);
        if(i==0u&&same_motion())p.call(0x4f2320u,{m.u32(list+i*4u),F0,F0},mot);
        else p.call(0x4f2320u,{m.u32(list+i*4u),F0,F20},mot);
        p.call(0x4f1e60u,{0},mot);
        const std::uint32_t at=preset*12u+0x5c15a0u;
        const std::uint32_t x=m.u32(at),y=m.u32(at+4),z=m.u32(at+8);
        m.put32(0x82f300u,y);
        std::uint32_t next=m.u32(0x82f5e8u)+1u;
        m.put32(0x82f2fcu,x);m.put32(0x82f304u,z);m.put32(0x82f5e8u,next);
        if(!(next<6u))m.put32(0x82f5e8u,0);
    }
    if(m.u32(0x82f5e4u)==0u&&race_robot_flagman_runaway_487a30(c)!=0u&&std::int16_t(p.call(0x49b2d0u,{}))<1){
        const std::uint32_t id=same_motion()?m.u32(preset*4u+0x6547ecu):m.u32(preset*4u+0x654800u);
        p.call(0x4f2320u,{id,F0,F4},mot);
        m.put32(0x82f5e4u,1);
    }
    p.push_unit();
    p.translate(p.vec(0x82f2fcu));
    driving::pc_matrix_rotate_z(c.matrices,m.f32(0x82f380u));
    driving::pc_matrix_rotate_y(c.matrices,m.f32(0x82f37cu));
    driving::pc_matrix_rotate_x(c.matrices,m.f32(0x82f378u));
    p.get(work+0x10);
    p.pop();
    race_robot_control_488450(c,work);
    const std::uint32_t bone=p.get_matrix_4f1ec0(mot,preset==1u||preset==3u?0x11u:0x1au);
    if(bone){
        p.push_load(work+0x10);
        p.multiply(bone);
        p.call(0x402140u,{});                        // flagman_cloth_ctrl
        p.pop();
    }
}

// ---- the passenger (function 0x39) --------------------------------------------
namespace {
// The passenger's reaction state (.bss 82F2xx..82F5xx) and the comment queue of
// the navigator voice (8 entries of 16 bytes at 82F268: sound, delay, priority,
// state 1 queued / 2 playing).
constexpr std::uint32_t Reaction=0x82f5a4u,ReactionTimer=0x82f5a8u,        // int, int16 (frames idle)
    PickIdle=0x82f5aau,PickScared=0x82f5abu,PickSpin=0x82f5acu,PickTurn=0x82f5aeu,   // round-robin bytes
    Ending=0x82f5b0u,EndingTimer=0x82f5b4u,EndingMotions=0x82f5b8u,       // goal sequence (state 11)
    AllRivalsDown=0x82f5bcu,AreaCheered=0x82f5c0u,
    PickJump=0x82f2f8u,PickLand=0x82f2f9u,Honked=0x82f23cu,
    Queue=0x82f268u,QueueHead=0x82f2e8u,QueueTail=0x82f2ecu,
    Pending=0x82f308u,PendingCount=0x82f328u,                             // up to 8 candidate comments
    Lap=0x82f228u,DriftFrames=0x82f2f4u,DriftShown=0x82f258u,Drifting=0x82f344u,
    TimeComment=0x82f5a0u,DriftComment1=0x82f240u,DriftComment2=0x82f5ccu,AreaComment=0x82f374u;
// Reaction states (82F5A4).
enum : std::uint32_t { Idle=0,Bored=1,Scared=2,Spin=3,Crash=4,Turn=5,Land=6,Jump=7,Air=8,Pose=9,Start=10,
    Goal=11,Over=12,Cheer=13,Hit=14,Rank=15,AreaSwitch=16 };
struct Passenger : Port {
    using Port::Port;
    void set_motion(std::uint32_t work,std::uint32_t id,std::uint32_t loop){call(0x487b70u,{work,id,loop});}
    void voice(std::uint32_t id){call(0x424940u,{id});}
    void blend(std::uint32_t mot,std::uint32_t id,std::uint32_t frame,float length){call(0x4f2320u,{id,frame,bits(length)},mot);}
    void play(std::uint32_t mot,std::uint32_t loop){call(0x4f1e60u,{loop},mot);}
    std::uint32_t q(std::uint32_t pc,std::initializer_list<std::uint32_t> args={}){return call(pc,args);}
    std::uint32_t rand(){return call(0x580f40u,{});}
    // Round-robin pick: list[index], index advanced and wrapped at count.
    std::uint32_t pick(std::uint32_t counter,std::uint32_t list){return m.u32(list+std::uint32_t(std::int32_t(m.i8(counter)))*4u);}
    void advance(std::uint32_t counter,std::uint32_t count_at){
        const std::uint8_t b=std::uint8_t(m.u8(counter)+1u);m.put8(counter,b);
        if(std::int32_t(std::int8_t(b))>=m.i32(count_at))m.put8(counter,0);
    }
    // Voice cooldown: work +84 alternates 0/1, +80 the frames left.
    void cooldown(std::uint32_t work,std::uint32_t frames){
        const std::uint8_t b=std::uint8_t(m.u8(work+0x84)+1u);m.put8(work+0x84,b);
        if(std::int8_t(b)>=2)m.put8(work+0x84,0);
        m.put32(work+0x80,frames);
    }
    // ---- comment queue (488130 / 4881C0) ---------------------------------------
    void queue_488130(std::uint32_t entry){
        const std::uint32_t head=m.u32(QueueHead);
        std::uint32_t last=head-1u;if(std::int32_t(last)<0)last=7;
        const std::uint32_t prev=Queue+last*16u;
        if(m.i32(prev+4)>0&&m.i16(prev+8)>=m.i16(entry+8))return;   // a comment of at least this priority still waits
        if(std::uint32_t(std::int32_t(head+1u)%8)==m.u32(QueueTail))return;
        const std::uint32_t d=Queue+head*16u;
        m.put32(d,m.u32(entry));m.put32(d+4,m.u32(entry+4));m.put16(d+8,m.u16(entry+8));
        m.put32(Queue+m.u32(QueueHead)*16u+0xc,1);
        const std::uint32_t next=m.u32(QueueHead)+1u;m.put32(QueueHead,next);
        if(std::int32_t(next)>=8)m.put32(QueueHead,0);
    }
    void queue_play_4881c0(){
        const std::uint32_t tail=m.u32(QueueTail);
        if(tail==m.u32(QueueHead))return;
        const std::uint32_t e=Queue+tail*16u;
        if(m.u32(e+0xc)==1u){voice(m.u32(e));m.put32(e+0xc,2);return;}
        const std::uint32_t left=m.u32(e+4)-1u;m.put32(e+4,left);
        if(left)return;
        const std::uint32_t next=m.u32(QueueTail)+1u;m.put32(QueueTail,next);
        if(std::int32_t(next)>=8)m.put32(QueueTail,0);
    }
    void reset_comments_4880c0(){
        for(std::uint32_t a:{PendingCount,QueueHead,QueueTail})m.put32(a,0);
        for(std::uint32_t k=0;k<8;++k)m.put32(Queue+k*16u+0xc,0);
        for(std::uint32_t a:{AreaComment,Lap,DriftShown,Drifting,DriftFrames,TimeComment,DriftComment1,DriftComment2})m.put32(a,0);
    }
    // A random pending candidate is queued, then the list is emptied.
    void queue_pending(){
        const std::int32_t n=m.i32(PendingCount);
        if(n<=0)return;
        const std::int32_t r=std::int32_t(rand());
        queue_488130(m.u32(Pending+std::uint32_t(r%m.i32(PendingCount))*4u));
        m.put32(PendingCount,0);
    }
    // 488BD0 (OutRun mode): the area comments (6549E0 table, 16-byte entries
    // until a negative priority) once the car is past 40 units into the area.
    void area_comments_488bd0(std::uint32_t car){
        if(m.u32(0x780258u)!=1u)return;
        if(q(0x450160u))m.put32(AreaComment,1);
        else if(m.u32(AreaComment)==1u){
            const std::int32_t into=std::int32_t(m.u16(car+0x260))-std::int32_t(m.u16(car+0x25e));
            const std::uint32_t area=q(0x44c940u,{q(0x450380u,{8})});
            if(m.i16(0x6549e8u)>=0)
                for(std::uint32_t e=0x6549e0u;;){
                    if(into>0x28&&m.u32(e+0xc)==area){m.put32(AreaComment,0);queue_488130(e);break;}
                    e+=0x10;
                    if(m.i16(e+8)<0)break;
                }
        }
        queue_pending();
    }
    // 488C90 (OutRun / C2C race modes): the lap comments of 654A30 (24-byte
    // entries: exact lap +C, else a range +10..+14, else from +10 on as candidates).
    void lap_comments_488c90(){
        const std::uint32_t v=m.u32(0x780258u);
        if(v!=1u&&v!=3u&&v!=4u)return;
        if(m.u32(DriftShown)==1u&&m.u32(TimeComment)==0u&&m.i16(0x654a38u)>=0){
            const std::int32_t lap=m.i32(Lap);
            for(std::uint32_t e=0x654a30u;;){
                const std::int32_t exact=m.i32(e+0xc);
                if(exact!=-1){
                    if(exact==lap){queue_488130(e);m.put32(TimeComment,1);break;}
                }else{
                    const std::int32_t from=m.i32(e+0x10),to=m.i32(e+0x14);
                    if(from!=-1&&to!=-1){
                        if(from<=lap&&to>=lap){queue_488130(e);m.put32(TimeComment,1);break;}
                    }else if(from<=lap){
                        m.put32(TimeComment,1);
                        const std::int32_t n=m.i32(PendingCount);
                        if(n<8){m.put32(Pending+std::uint32_t(n)*4u,e);m.put32(PendingCount,std::uint32_t(m.i32(PendingCount)+1));}
                    }
                }
                e+=0x18;
                if(m.i16(e+8)<0)break;
            }
        }
        queue_pending();
    }
    // 488220: the drift comments of 654B08 (16-byte entries keyed by the drift frame count).
    void drift_comments_488220(){
        const std::uint32_t v=m.u32(0x780258u);
        if(v!=1u&&v!=3u&&v!=4u)return;
        if(m.i16(0x654b10u)<0)return;
        std::uint32_t first=m.u32(DriftComment1),second=m.u32(DriftComment2);
        const std::uint32_t frames=m.u32(DriftFrames);
        for(std::uint32_t e=0x654b14u;;e+=0x10){
            if(!first&&m.u32(e)==frames){queue_488130(e-0xc);first=1;}
            else if(!second&&m.u32(e)==frames){queue_488130(e-0xc);second=1;}
            if(m.i16(e+0x10-4)<0)break;
        }
        m.put32(DriftComment2,second);m.put32(DriftComment1,first);
    }
    // 4882B0: drift frame counter (car +248 sliding bits); 120 frames count a drift.
    void drift_count_4882b0(std::uint32_t car){
        const bool sliding=(m.u32(car+0x248)&0x10f407cu)!=0u;
        std::uint32_t drifting=m.u32(Drifting);
        if(sliding&&drifting==0u){drifting=1;m.put32(Drifting,1);}
        if(drifting==1u){
            const std::uint32_t f=m.u32(DriftFrames)+1u;m.put32(DriftFrames,f);
            if(std::int32_t(f)>=0x78&&m.u32(DriftShown)==0u){m.put32(DriftShown,1);m.put32(Lap,m.u32(Lap)+1u);}
        }
        if(!sliding&&drifting==1u)
            for(std::uint32_t a:{DriftShown,Drifting,DriftFrames,TimeComment,DriftComment1,DriftComment2})m.put32(a,0);
    }
    // 488D80: the navigator comments, outside the goal and the race start.
    void comments_488d80(){
        const std::uint32_t car=this->car();
        if((q(0x44b7b0u,{m.u32(car+0x5c)})&0xffu)&&q(0x44ff10u))return;
        if(std::int16_t(q(0x49b2d0u))>0x3c)return;
        area_comments_488bd0(car);
        lap_comments_488c90();
        drift_comments_488220();
        queue_play_4881c0();
        drift_count_4882b0(car);
    }
    // 4886B0: back to the idle loop once the motion ended.
    void back_to_idle_4886b0(std::uint32_t work){
        const std::uint32_t mot=motion(work);
        if(!motion_end_4f1e70(mot))return;
        blend(mot,0x6004bu,0,10.0f);play(mot,0xffffffffu);
        m.put32(Reaction,Idle);m.put16(ReactionTimer,0);
    }
    // 487BB0: a motion from its first frame.
    void restart_487bb0(std::uint32_t work,std::uint32_t id,std::uint32_t loop){
        const std::uint32_t mot=motion(work);blend(mot,id,0,0.0f);play(mot,loop);
    }
    // 4F1E80: a bone's override weight (the hair of the convertible models).
    void bone_weight_4f1e80(std::uint32_t mot,std::uint8_t bone,std::uint32_t weight){
        const std::uint32_t b=m.u32(mot+0x74)+std::uint32_t(bone)*0x350u;
        if(m.u32(0x6a81e4u))m.put32(b+0x33c,2);
        m.put32(b+0x344,weight);
    }
    // 487F50: the passenger's character for the mode (6549C8 table index).
    std::uint32_t character_487f50(){
        (void)q(0x455c10u);
        static constexpr std::uint32_t Licence[6]{0,1,2,4,3,5};
        const std::uint32_t slot=m.u32(0x7c2400u);
        if(slot>=6u)throw std::out_of_range("487F50: licence slot 7C2400 outside 0..5");
        const std::uint32_t who=Licence[slot];
        if(q(0x43f860u)&0xffu){
            std::uint32_t pick=6;
            switch(m.u32(0x780258u)){
            case 2:pick=q(0x505340u,{who,1});break;
            case 9:pick=q(0x505340u,{who,2});break;
            case 5:switch(q(0x46c520u)){case 0:pick=3;break;case 1:pick=4;break;case 2:pick=5;break;default:pick=6;break;}break;
            default:pick=q(0x505340u,{who,0});break;
            }
            return m.u32(0x6549c8u+pick*4u);
        }
        if(m.u32(0x78026cu)==3u)return 8;
        const std::uint32_t v=m.u32(0x780258u);
        return v==1u||v==3u||v==4u||v==5u?9u:6u;
    }
    void init_4885a0(std::uint32_t work){
        const std::uint32_t car=this->car();
        const std::uint32_t convertible=model_flag_45b380(m.u8(car+0x11));
        call(0x487810u,{work});
        call(0x487c30u,{work,character_487f50()});
        const std::uint32_t mot=motion(work);
        const std::uint32_t first=m.u32(0x780258u)==2u?(convertible?0x6004bu:0x260012u):0x25000cu;
        call(0x4f2280u,{first},mot);
        call(0x4f1d30u,{0},mot);
        call(0x4f2520u,{},mot);
        call(0x4f1d30u,{0},mot);
        play(mot,0xffffffffu);
        m.put32(Reaction,Start);m.put16(ReactionTimer,0);
        for(std::uint32_t a:{PickIdle,PickScared,PickSpin,PickSpin+1u,PickTurn})m.put8(a,0);
        m.put32(AllRivalsDown,0);m.put32(AreaCheered,0);
        call(race_robot_hand_motion_487940(m,m.u32(mot+4))?0x5148d0u:0x5148f0u,{work});
        for(std::uint32_t a:{0x82f234u,0x82f238u,Honked,0x82f5dcu,0x82f5e0u})m.put32(a,0);
        reset_comments_4880c0();
    }
    // ---- 489A90 ------------------------------------------------------------------
    // Per frame: the reaction to the drive (jumps, landings, spins, crashes,
    // scares, turns, boredom; the goal / time over / rank poses) picked from the
    // mode's motion lists, her voices, the navigator comments, then the motion
    // control (488450).
    struct Lists {
        std::uint32_t motions;   // 5C0FD8 / 5C1018 (C2C) or 5C1110 / 5C1150: {list, count} per reaction
        std::uint32_t jump_voices,land_voices;
        std::uint32_t air_voices;   // 5C1190 / 5C119C
    };
    enum class Exit { Reset,Plain,Honk,Skip };
    void control_489a90(std::uint32_t work){
        const std::uint32_t car=this->car();
        const std::uint32_t variant=m.u32(0x780258u);
        const std::uint32_t mot=motion(work);
        const std::uint32_t convertible=model_flag_45b380(m.u8(car+0x11));
        const std::uint32_t stage=q(0x451350u,{q(0x44c940u,{m.u32(car+0x68)})});
        Lists l{};
        if(variant==2){l.motions=convertible?0x5c1018u:0x5c0fd8u;l.land_voices=0x5c0fb0u;l.jump_voices=0x5c0fd0u;}
        else{l.motions=convertible?0x5c1150u:0x5c1110u;l.land_voices=0x5c10e8u;l.jump_voices=0x5c1108u;}
        const std::uint32_t kind=m.u32(work+8);
        l.air_voices=kind==9u||kind==10u?0x5c1190u:0x5c119cu;
        // OutRun mode: all five rivals of the last stage out of the race.
        std::uint32_t rivals_down=0;
        if(variant==1&&convertible==0u&&m.u32(car+0x5c)==1u&&m.u32(AllRivalsDown)==0u){
            for(std::uint32_t k=0,slot=0x79dd8cu;slot<0x79e3a4u;slot+=0x78u,k+=2u){
                for(unsigned h=0;h<2;++h){
                    if((m.u8(0x79fc62u+k+h)&3u)!=2u)continue;
                    const std::uint32_t other=m.u32(h?slot:slot-0x3cu);
                    if(m.u32(other+0x5c)==4u&&m.u32(other+0x3f0)!=0u)++rivals_down;
                }
            }
            if(rivals_down==5u)m.put32(AllRivalsDown,1);
        }
        if(m.u32(car+0x5c)==0u){m.put32(AllRivalsDown,0);m.put32(AreaCheered,0);}
        race_robot_driver_matrix_488af0(c,work,1,car);
        if(q(0x46c530u)){call(0x488450u,{work});return;}
        Exit e=react(work,car,variant,mot,convertible,stage,l,rivals_down);
        if(e==Exit::Reset){m.put32(work+0x6c,0xffffffffu);m.put32(work+0x74,0xffffffffu);e=Exit::Plain;}
        if(e==Exit::Plain)e=variant==1u?Exit::Honk:Exit::Skip;
        if(e==Exit::Honk&&(m.u32(car+4)&0x4000000u)&&(m.u8(car+0x2f0)&2u)&&m.i32(work+0x80)<=0){
            voice(m.u32(0x5c1550u+(rand()%5u)*4u));cooldown(work,0x5a);
        }
        comments_488d80();
        animate(work,car,variant,mot,convertible,l);
        if(m.u32(0x654758u)&&convertible){
            bone_weight_4f1e80(mot,0x2c,m.u32(0x654754u));
            bone_weight_4f1e80(mot,0x1c,m.u32(0x654754u));
        }
        if(!(m.u8(mot+8)&0x20u))call(race_robot_hand_motion_487940(m,m.u32(mot+4))?0x5148d0u:0x5148f0u,{work});
        if(m.i32(work+0x80)>0)m.put32(work+0x80,m.u32(work+0x80)-1u);
        call(0x488450u,{work});
    }
    // The 48a647 tail stores -1 in work +6C / +74 (Exit::Reset).
    Exit react(std::uint32_t work,std::uint32_t car,std::uint32_t variant,std::uint32_t mot,std::uint32_t convertible,
               std::uint32_t stage,const Lists& l,std::uint32_t rivals_down){
        std::uint32_t st=m.u32(Reaction);
        if(st==Start){m.put32(Reaction,Idle);m.put32(work+0x6c,0xffffffffu);m.put32(work+0x74,0xffffffffu);return Exit::Plain;}
        if((m.u8(0x79fcafu)&3u)==2u){
            if(q(0x44ff10u)){
                if(m.u32(Reaction)!=Goal){m.put32(Reaction,Goal);m.put32(Ending,0);}
                return Exit::Reset;
            }
            if(q(0x44fe70u)||q(0x450240u)==4u){
                if(m.u32(Reaction)==Over)return Exit::Reset;
                if(variant==1){set_motion(work,0x60036u,0xffffffffu);m.put32(Reaction,Over);return Exit::Reset;}
                const std::uint32_t now=m.u32(0x780258u);
                if(variant==2||now==0u||now==7u){set_motion(work,0x60006u,0xffffffffu);m.put32(Reaction,Over);return Exit::Reset;}
                if(variant==3||variant==4){
                    std::int32_t rank=std::int32_t(q(0x45a2b0u,{q(0x455ad0u)&0xffu})&0xffu);
                    bool pose=true;
                    if(rank>3)rank=3;
                    else if(rank==0&&convertible)pose=false;
                    if(pose){
                        const std::uint32_t r=q(0x455c10u);
                        const std::uint32_t row=r==1u||r==4u?0u:1u;
                        const std::uint8_t model=m.u8(car+0x11);
                        const std::uint32_t table=model==5u||model==3u||model==9u?0x5c1520u:0x5c1500u;
                        set_motion(work,m.u32(table+(std::uint32_t(rank)+row*4u)*4u),0xffffffffu);
                    }
                }
                m.put32(Reaction,Over);return Exit::Reset;
            }
            st=m.u32(Reaction);
        }
        if(m.f32(car+0x2c8)>0.0f){                                     // in the air
            if(st==Air)return Exit::Reset;
            const std::uint32_t k=((m.u32(car+0x2f0)>>2)&0x1fu)-1u;
            if(k>6u)return Exit::Reset;
            switch(k){
            case 0:case 1:case 4:{
                m.put32(Reaction,Air);set_motion(work,0x6005fu,0);
                if(m.u32(0x780258u)!=2u)voice(m.u32(l.air_voices+(k==0?0u:k==1?4u:8u)));
                return Exit::Reset;}
            case 6:m.put32(Reaction,Air);return Exit::Reset;
            default:return Exit::Reset;
            }
        }
        if(m.f32(car+0x2f8)>0.0f){                                     // landing
            if(st==Land||st==Jump)return Exit::Reset;
            const std::uint32_t k=((m.u32(car+0x2f0)>>2)&0x1fu)-1u;
            static constexpr std::uint8_t Big[7]{0,1,0,0,1,0,0};
            if(k<=6u&&Big[k]==0u){
                m.put32(Reaction,Jump);
                set_motion(work,pick(PickJump,m.u32(l.motions+0x38)),0);
                voice(m.u32(l.jump_voices+std::uint32_t(std::int32_t(m.i8(PickJump)))*4u));
                advance(PickJump,l.motions+0x3c);
            }else{
                m.put32(Reaction,Land);
                set_motion(work,pick(PickLand,m.u32(l.motions+0x30)),0);
                voice(m.u32(l.land_voices+std::uint32_t(std::int32_t(m.i8(PickLand)))*4u));
                advance(PickLand,l.motions+0x34);
            }
            return Exit::Reset;
        }
        if(st==Land||st==Jump)return Exit::Plain;
        if(m.u32(car+4)&0x30000u){                                     // crash
            if(st==Crash)return Exit::Reset;
            m.put32(Reaction,Crash);
            set_motion(work,crash_motion(car,variant),0);
            return Exit::Reset;
        }
        const std::uint32_t slide=m.u32(car+0x248);
        if((slide&0x10f407cu)&&st!=Hit){
            const std::uint8_t model=m.u8(car+0x11);   // C2C: 0x22000D but for models 3 / 9
            set_motion(work,!(slide&4u)?0x220007u:variant!=2u?0x230003u:model!=3u&&model!=9u?0x22000du:0x220007u,0);
            m.put32(Reaction,Hit);return Exit::Reset;
        }
        if(q(0x450160u)){
            if(convertible)return Exit::Reset;
            m.put32(Ending,1);m.put32(Reaction,Rank);return Exit::Reset;
        }
        std::uint32_t cur=m.u32(Reaction);
        if(m.u32(car+0x184)==0u&&m.u32(car+0x5c)==1u&&cur!=Cheer){   // stage start
            if(variant!=3&&variant!=4)return Exit::Plain;
            if(stage==2u)return Exit::Plain;
            set_motion(work,stage==0u?0x60075u:0x60030u,0);m.put32(Reaction,Cheer);return Exit::Reset;
        }
        if(variant!=2){
            const std::uint32_t switching=q(0x44bce0u);
            cur=m.u32(Reaction);
            if(switching&&cur!=AreaSwitch&&m.u32(AreaCheered)==0u){
                const std::uint32_t k=q(0x44bd20u);
                const std::int32_t id=m.i32((variant==3||variant==4?0x5c13d8u:0x5c1410u)+k*4u);
                if(id>=0){set_motion(work,std::uint32_t(id),0);m.put32(Reaction,AreaSwitch);}
                m.put32(AreaCheered,1);return Exit::Plain;
            }
        }
        if(rivals_down==5u&&m.u32(AllRivalsDown)!=0u&&cur!=Cheer){
            set_motion(work,0x60070u,0);m.put32(Reaction,Cheer);return Exit::Reset;
        }
        if(cur==Cheer){
            if(!motion_end_4f1e70(mot)&&m.u32(car+0x5c)==1u)return Exit::Reset;
            m.put32(Reaction,Idle);set_motion(work,0x6004bu,0xffffffffu);m.put16(ReactionTimer,0);
            return Exit::Reset;
        }
        if(m.i32(work+0x68)>=0){                                       // requested pose (+68: motion id)
            m.put32(Reaction,Pose);
            if(m.u32(work+0x68)!=m.u32(work+0x6c)){
                blend(mot,m.u32(work+0x68),0,20.0f);play(mot,0);m.put32(work+0x6c,m.u32(work+0x68));
            }
            m.put32(work+0x68,0xffffffffu);return Exit::Plain;
        }
        if(m.i32(work+0x70)>=0){                                       // requested pose (+70: 5C1058 row)
            m.put32(Reaction,Pose);
            if(convertible)m.put32(work+0x70,1);
            if(m.u32(work+0x70)!=m.u32(work+0x74)){
                blend(mot,m.u32(0x5c1058u+m.u32(work+0x70)*12u),0,20.0f);play(mot,0);m.put32(work+0x74,m.u32(work+0x70));
            }
            m.put32(work+0x70,0xffffffffu);return Exit::Plain;
        }
        if(cur==Pose){
            if(motion_end_4f1e70(mot)&&m.u32(work+0x78)==2u){m.put32(Reaction,Idle);m.put16(ReactionTimer,0);return Exit::Reset;}
            return Exit::Plain;
        }
        const std::uint32_t flags=m.u32(car+4);
        const std::uint32_t state=m.u32(car+0xc);
        const bool scared=(flags&0x1000u)||((state&1u)&&(((state>>4)&0xfu)==5u||((state>>4)&0xfu)<3u));
        if(scared){
            if(cur==Scared)return Exit::Reset;
            m.put32(Reaction,Scared);
            const std::uint32_t f=m.u32(car+4),s=m.u32(car+0xc);
            const bool hard=(f&0x1000u)||((s&1u)&&(s&0xf0u)==0x50u);
            if(!hard){
                if(m.u8(car+0x11)==9u)set_motion(work,0x6003du,0);
                else set_motion(work,m.u32(0x5c1264u+(std::int32_t(rand())>0x3fff?4u:0u)),0);
            }else{
                const std::uint32_t id=m.u8(car+0x11)==9u?0x6003du:convertible?0x6004bu:
                    m.u32(m.u32(l.motions+0x10)+std::uint32_t(std::int32_t(m.i8(PickScared)))*4u);
                set_motion(work,id,0);
                advance(PickScared,l.motions+0x14);
            }
            if(variant==3||variant==4){
                if(m.i32(work+0x80)<=0){voice(m.u32(0x5c1540u+std::uint32_t(std::int32_t(m.i8(work+0x84)))*4u));cooldown(work,0x5a);}
            }else if(variant==1&&m.i32(work+0x80)<=0)cooldown(work,0x5a);
            return Exit::Reset;
        }
        if(flags&0x2000u){                                             // spin
            if(cur==Spin)return Exit::Reset;
            m.put32(Reaction,Spin);
            std::int32_t i=m.i8(PickSpin);
            if(convertible)i%=2;
            set_motion(work,m.u32(m.u32(l.motions+0x18)+std::uint32_t(i)*4u),0);
            advance(PickSpin,l.motions+0x1c);
            if((variant==3||variant==4)&&m.i32(work+0x80)<=0){
                voice(m.u32(0x5c1548u+std::uint32_t(std::int32_t(m.i8(work+0x85)))*4u));
                const std::uint8_t b=std::uint8_t(m.u8(work+0x85)+1u);m.put8(work+0x85,b);
                if(std::int8_t(b)>=2)m.put8(work+0x85,0);
                m.put32(work+0x80,0x96);
            }
            return Exit::Reset;
        }
        if(m.u32(car+0x1f4)>=100u){                                    // a sharp turn at speed
            const std::int32_t a=std::abs(std::int32_t(m.i16(car+0x4e)));
            if(a>0x2000&&a<0x4000){
                if(cur==Turn||convertible)return Exit::Plain;
                m.put32(Reaction,Turn);
                set_motion(work,m.u32(m.u32(l.motions+0x28)+std::uint32_t(std::int32_t(m.i8(PickTurn)))*4u),0);
                advance(PickTurn,l.motions+0x2c);
                return Exit::Reset;
            }
        }
        if(m.i16(ReactionTimer)>0xb4){                                 // bored after 3 seconds of nothing
            m.put32(Reaction,Bored);
            set_motion(work,m.u32(m.u32(l.motions+8)+std::uint32_t(std::int32_t(m.i8(PickIdle)))*4u),0);
            advance(PickIdle,l.motions+0xc);
            m.put16(ReactionTimer,0);
            return Exit::Reset;
        }
        if(variant!=1)return Exit::Skip;
        if(m.u32(Honked)){
            if(m.i32(work+0x80)<=0){voice(0x163);cooldown(work,0x5a);}
            m.put32(Honked,0);
        }
        return Exit::Honk;
    }
    std::uint32_t crash_motion(std::uint32_t car,std::uint32_t variant){
        if(variant!=2)return 0x250003u;
        if(q(0x45c440u)==3u)return 0x250003u;
        return (m.u8(car+6)&1u)?0x60024u:0x60026u;
    }
    // 48A6BC..48AC40: the running reaction's motion.
    void animate(std::uint32_t work,std::uint32_t car,std::uint32_t variant,std::uint32_t mot,std::uint32_t convertible,const Lists& l){
        const std::uint32_t st=m.u32(Reaction);
        switch(st){
        case Idle:m.put16(ReactionTimer,std::uint16_t(m.u16(ReactionTimer)+1u));return;
        case Bored:case Scared:case Spin:back_to_idle_4886b0(work);return;
        case Crash:
            if(!motion_end_4f1e70(mot))return;
            if(!(m.u32(car+4)&0x30000u)){m.put32(Reaction,Idle);m.put16(ReactionTimer,0);return;}
            set_motion(work,crash_motion(car,variant),0);return;
        case Turn:{
            if(!motion_end_4f1e70(mot))return;
            const std::uint32_t list=l.motions+m.u32(Reaction)*8u;
            const std::int32_t a=std::abs(std::int32_t(m.i16(car+0x4e)));
            if(a>0x2000&&a<0x4000){set_motion(work,m.u32(m.u32(list)+std::uint32_t(std::int32_t(m.i8(PickTurn)))*4u),0);return;}
            m.put32(Reaction,Idle);set_motion(work,0x6004bu,0xffffffffu);
            advance(PickTurn,list+4);
            m.put16(ReactionTimer,0);return;}
        case Land:case Jump:
            if(variant!=2){back_to_idle_4886b0(work);return;}
            if(!motion_end_4f1e70(mot))return;
            set_motion(work,convertible?0x6004bu:0x60055u,0);m.put32(Reaction,Idle);m.put16(ReactionTimer,0);return;
        case Air:
            if(!(m.f32(car+0x2c8)==m.f32(Zero_619a34))||(m.u32(car+0x2f0)&0x7cu)!=0x1cu)return;
            m.put32(Reaction,Jump);
            set_motion(work,m.u32(m.u32(l.motions+0x20)+4u),0);
            voice(m.u32(l.jump_voices+std::uint32_t(std::int32_t(m.i8(PickJump)))*4u));
            advance(PickJump,l.motions+0x24);return;
        case Pose:{
            if(!motion_end_4f1e70(mot)||m.i32(work+0x74)<0)return;
            const std::uint32_t row=0x5c105cu+m.u32(work+0x74)*12u;   // {in, loop, out} after the 5C1058 start
            const std::uint32_t step=m.u32(work+0x78);
            if(step==0u){m.put32(work+0x78,1);blend(mot,m.u32(row),0,2.0f);play(mot,0);return;}
            if(step==1u){
                if(m.u32(work+0x7c)){blend(mot,m.u32(row),0,2.0f);play(mot,0);return;}
                blend(mot,m.u32(row+4),0,2.0f);m.put32(work+0x78,2);
            }
            play(mot,0);return;}
        case Goal:goal(work,variant,mot,convertible);return;
        case Hit:{
            if(!motion_end_4f1e70(mot))return;
            const std::uint32_t slide=m.u32(car+0x248);
            if(!(slide&0x10f407cu)){back_to_idle_4886b0(work);return;}
            set_motion(work,!(slide&4u)?0x220007u:variant==2u&&!convertible?0x22000du:0x230003u,0);   // C2C closed models: 0x22000D
            m.put32(Reaction,Hit);return;}
        case Rank:rank(work,mot);return;
        case AreaSwitch:
            if(motion_end_4f1e70(mot)||m.u32(car+0x5c)!=1u)back_to_idle_4886b0(work);
            return;
        default:return;   // Start, Over, Cheer: held
        }
    }
    static constexpr std::uint32_t Zero_619a34=0x619a34u;
    // State 11 (goal): Ending 0 picks the pose (C2C: 6545C8 / 654628 by the
    // result), 1 starts it, 2 holds it 360 frames, 3 the last motion, 8 done.
    void goal(std::uint32_t work,std::uint32_t variant,std::uint32_t mot,std::uint32_t convertible){
        switch(m.u32(Ending)){
        case 0:{
            if(variant==2){
                const std::uint32_t r=q(0x45bf30u,{4});
                const std::uint32_t list=(convertible?0x654628u:0x6545c8u)+r*12u;
                m.put32(EndingMotions,list);m.put32(EndingTimer,0xffffffffu);
                m.put32(Ending,m.i32(list)<0?2u:1u);
                return;
            }
            std::uint32_t id=0x60049u;
            if(convertible)id=0x25000cu;
            else if(variant==3||variant==4){const std::uint32_t r=q(0x455c10u);if(r!=1u&&r!=4u)id=0x60071u;}
            set_motion(work,id,0xffffffffu);m.put32(Ending,8);return;}
        case 1:
            set_motion(work,m.u32(m.u32(EndingMotions)),0);
            m.put32(EndingTimer,0x168);m.put32(Ending,2);return;
        case 2:{
            const std::int32_t left=m.i32(EndingTimer);
            if(!motion_end_4f1e70(mot)&&left>=0){
                if(left==0)m.put32(Ending,3);else m.put32(EndingTimer,std::uint32_t(left-1));
                return;
            }
            set_motion(work,m.u32(m.u32(EndingMotions)+4u),0xffffffffu);
            if(m.i32(EndingTimer)<0)m.put32(Ending,8);
            return;}
        case 3:
            set_motion(work,m.u32(m.u32(EndingMotions)+8u),0);m.put32(Ending,8);return;
        default:return;
        }
    }
    // State 15 (rank of the stage): the 5C11B0 row of the stage's rank.
    void rank(std::uint32_t work,std::uint32_t mot){
        const std::uint32_t row=0x5c11b0u+q(0x44c940u,{q(0x450380u,{8})})*12u;
        switch(m.u32(Ending)){
        case 1:{
            const std::int32_t id=m.i32(row);
            if(id<0){back_to_idle_4886b0(work);return;}
            set_motion(work,std::uint32_t(id),0);m.put32(Ending,2);return;}
        case 2:
            if(!motion_end_4f1e70(mot))return;
            restart_487bb0(work,m.u32(row+4),0);m.put32(Ending,3);return;
        case 3:{
            if(!motion_end_4f1e70(mot))return;
            const std::int32_t id=m.i32(row+8);
            if(id<0){back_to_idle_4886b0(work);return;}
            restart_487bb0(work,std::uint32_t(id),0);m.put32(Ending,8);return;}
        case 8:
            if(motion_end_4f1e70(mot))back_to_idle_4886b0(work);
            return;
        default:return;
        }
    }
    // ---- function 0x38: the driver of the other start contexts (489850) ----------
    // Her steering follows the wheel angle (car +204) as the motion frame; the
    // 5C0C70 row of the model: {wave, -, look, back} motions every 120 frames
    // of a steady wheel, 5C0BF8 the model's return motion. (The original also
    // computes the hand grip points, 46BC30 / 46BC10 into stack locals, and
    // never uses them.)
    void driver_control_489850(std::uint32_t work){
        const std::uint32_t car=this->car();
        const std::uint32_t row=0x5c0c70u+std::uint32_t(std::int32_t(m.i8(car+0x11)))*16u;
        const std::uint32_t mot=motion(work);
        race_robot_driver_matrix_488af0(c,work,0,car);
        const float frame=float(std::int32_t(m.i16(car+0x204))+0x7fff)*m.f32(0x5b4320u)*m.f32(0x5c1628u);
        constexpr std::uint32_t State=0x82f588u,Steady=0x82f58cu;
        std::uint32_t st=m.u32(State);
        auto steady=[&]{
            if(frame>m.f32(0x5c161cu)&&m.f32(0x5c1618u)>frame)m.put16(Steady,std::uint16_t(m.u16(Steady)+1u));
        };
        auto start=[&](std::uint32_t next,std::uint32_t id){
            m.put32(State,next);call(0x487b70u,{work,id,0});m.put16(Steady,0);st=m.u32(State);
        };
        auto settle=[&](std::uint32_t next,std::uint32_t id){
            if(motion_end_4f1e70(mot)){
                m.put32(State,next);call(0x4f2320u,{id,bits(frame),bits(10.0f)},mot);call(0x4f1e60u,{0},mot);
            }
            st=m.u32(State);
        };
        switch(st){
        case 1:
            if(m.i16(Steady)>0x78&&m.i32(row)>=0)start(2,m.u32(row));
            else steady();
            break;
        case 2:settle(4,m.u32(row+8));break;
        case 4:
            if(m.i16(Steady)>0x78)start(5,m.u32(row+0xc));
            else steady();
            break;
        case 5:case 6:case 7:settle(1,m.u32(0x5c0bf8u+std::uint32_t(std::int32_t(m.i8(car+0x11)))*4u));break;
        case 8:m.put32(State,1);st=1;break;
        default:break;
        }
        if(st==1u||st==4u)call(0x4f1d30u,{bits(frame)},mot);   // the frame follows the wheel
        call(0x488450u,{work});
    }
    // ---- function 0x3A: the passenger of the other start contexts (48AD60) -------
    // The two 5C11A8 idle motions in turn.
    void passenger_control_48ad60(std::uint32_t work){
        const std::uint32_t mot=motion(work);
        race_robot_driver_matrix_488af0(c,work,1,this->car());
        if(m.u32(Reaction)==Start){blend(mot,0x6004bu,0,10.0f);play(mot,0);m.put32(Reaction,Idle);}
        if(motion_end_4f1e70(mot)){
            blend(mot,m.u32(0x5c11a8u+std::uint32_t(std::int32_t(m.i8(PickIdle)))*4u),0,10.0f);play(mot,0);
            const std::uint8_t b=std::uint8_t(m.u8(PickIdle)+1u);m.put8(PickIdle,b);
            if(b>=2u)m.put8(PickIdle,0);
        }
        if(m.u32(Reaction)==Idle)m.put16(ReactionTimer,std::uint16_t(m.u16(ReactionTimer)+1u));
        call(0x488450u,{work});
    }
};
}

std::uint32_t race_robot_model_flag_45b380(std::uint32_t model_byte){return Port::model_flag_45b380(model_byte);}
void race_robot_passenger_init_4885a0(PcRaceContext& c,std::uint32_t work){Passenger(c).init_4885a0(work);}
void race_robot_passenger_control_489a90(PcRaceContext& c,std::uint32_t work){Passenger(c).control_489a90(work);}
void race_robot_driver_control_489850(PcRaceContext& c,std::uint32_t work){Passenger(c).driver_control_489850(work);}
void race_robot_passenger_control_48ad60(PcRaceContext& c,std::uint32_t work){Passenger(c).passenger_control_48ad60(work);}
}
