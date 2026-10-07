#include "platform/race_goal_camera.hpp"
#include "platform/pc_address_view.hpp"
#include "driving/pc_x87.hpp"
#include "driving/pc_crash.hpp"
#include "driving/pc_d3dx.hpp"
#include "driving/pc_car_services_ghost.hpp"
#include <array>
#include <cstring>
namespace outrun::platform {
namespace {
using driving::X87;
std::uint32_t call(PcRaceContext& c,std::uint32_t pc,std::initializer_list<std::uint32_t> args,std::uint32_t ecx=0){
    PcRaceCall k;k.pc=pc;k.ecx=ecx;k.argc=std::uint32_t(args.size());
    std::size_t i=0;for(auto a:args)k.args[i++]=a;
    return c.service(k);
}
std::uint32_t bits(float f){std::uint32_t v;std::memcpy(&v,&f,4);return v;}
// cvttss2si: truncation; NaN and out-of-range give 0x80000000.
std::int32_t cvtt(float f){
    if(!(f>-2147483904.0f&&f<2147483648.0f))return std::int32_t(0x80000000u);
    return std::int32_t(f);
}
constexpr float K6282c0=10430.3779296875f;          // 65536 / 2pi
constexpr float K62806c=1.0f,K6280e8=0.01666666753590107f,K628064=0.5f;
constexpr float K5c09c0=0.03937000036239624f,K5c09bc=0.7086499929428101f;
driving::CourseProbe get3(const PcRaceMemory& m,std::uint32_t a){return {m.f32(a),m.f32(a+4),m.f32(a+8)};}
void put3(const PcRaceMemory& m,std::uint32_t a,const driving::CourseProbe& v){m.putf(a,v.x);m.putf(a+4,v.y);m.putf(a+8,v.z);}
// The 486730 stack frame (X = ESP inside the record loop): mapped while it runs.
struct Frame {
    PcRaceMemory& m;std::size_t mark;std::array<std::uint8_t,0xb0> bytes{};
    explicit Frame(PcRaceMemory& mem):m(mem),mark(mem.mark()){m.map(PcGoalLocals,bytes.data(),bytes.size());}
    ~Frame(){m.release(mark);}
};
constexpr std::uint32_t L(std::uint32_t o){return PcGoalLocals+o;}
// 40F180(out, a, ka, b, kb): a*ka + b*kb; A.z, B.y and B.z are spilled to
// floats before the additions, the other products stay extended.
driving::CourseProbe weighted_sum_40f180(const driving::CourseProbe& a,float ka,const driving::CourseProbe& b,float kb){
    const X87 ax=X87(ka)*a.x,ay=X87(ka)*a.y;
    const float az=driving::x87_float(X87(ka)*a.z);
    const X87 bx=X87(kb)*b.x;
    const float by=driving::x87_float(X87(kb)*b.y),bz=driving::x87_float(X87(kb)*b.z);
    return {driving::x87_float(bx+ax),driving::x87_float(X87(by)+ay),driving::x87_float(X87(bz)+az)};
}
// 449640(out, matrix): angles of a matrix whose rows it normalises in place.
void angles_449640(PcRaceMemory& m,std::uint32_t out,std::uint32_t matrix){
    const auto a=driving::pc_matrix_angles_449640(m.bytes(matrix,0x30));
    m.putf(out,a[0]);m.putf(out+4,a[1]);m.putf(out+8,a[2]);
}
// 4493B0 of an SSE product: car +2C/+2E/+30 from the three angles at a.
void car_angles(PcRaceMemory& m,std::uint32_t car,std::uint32_t a){
    m.put16(car+0x2c,std::uint16_t(cvtt(m.f32(a)*K6282c0)));
    m.put16(car+0x2e,std::uint16_t(cvtt(m.f32(a+4)*K6282c0)));
    m.put16(car+0x30,std::uint16_t(cvtt(m.f32(a+8)*K6282c0)));
}
// 43D920(&first, &last, path, hint): the run of course segments carrying the path's
// segment id (path: +0 the course part, +4 the side 0x64..0x67, +8 the id), from the
// part's id words [780228 + part * 4] (count = [780140 + part * 4] + C); returns their
// count (0 when the part has no table). Part 0 (43D4D0): with no hint (or 780190) the
// range comes from the pair table [780218] (the id clamped to [780238] - 1); with a hint
// in range, the run around it. Other parts (43D6E0): from the first segment of the id;
// sides 65 / 67 skip the 0x400 .. 0x800 flagged stretches of [780110] (64-byte records,
// flags +3C). The side 64 / 66 run reads its next ids one byte off (word at +2k+1).
std::uint32_t goal_segments_43d920(PcRaceContext& c,std::uint32_t first,std::uint32_t last,std::uint32_t path,std::uint32_t hint){
    auto& m=c.m;
    const std::uint32_t part=m.u32(path);
    const std::uint32_t tbl=m.u32(0x780140u+part*4u);
    if(!tbl)return 0;
    const std::int32_t count=m.i32(tbl+0xcu);
    const std::uint32_t ids=m.u32(0x780228u+part*4u);
    auto id=[&](std::int32_t k)->std::uint32_t{return k==-1?0u:m.u16(ids+std::uint32_t(k)*2u);};
    auto result=[&](std::int32_t k){m.put32(last,std::uint32_t(k));return std::uint32_t(k-m.i32(first)+1);};
    if(part==0u){                                                            // 43D4D0
        std::int32_t seg=m.i16(path+8u);
        const std::int32_t top=m.i32(0x780238u)-1;
        if(seg>top)seg=top;
        std::int32_t at=std::int32_t(hint);
        if(at>=count)at=-1;
        if(m.u8(0x780190u)||at==-1){
            const std::uint32_t pairs=m.u32(0x780218u+part*4u);
            m.put32(last,m.u16(pairs+std::uint32_t(seg)*4u+2u));
            m.put32(first,m.u16(pairs+std::uint32_t(seg)*4u));
            return m.u32(last)-m.u32(first)+1u;
        }
        if(id(at)==std::uint32_t(seg)){
            std::int32_t k=at;
            while(k>0&&id(k-1)==std::uint32_t(seg))--k;
            m.put32(first,std::uint32_t(k));
            k=at;
            while(k<count-1&&id(k+1)==std::uint32_t(seg))++k;
            return result(k);
        }
        // a hint off the id: search toward it (401810: the id at the hint)
        const std::uint32_t here=id(at);
        if(std::int32_t(here)<seg){
            std::int32_t k=at+1;
            while(k<count&&id(k)!=std::uint32_t(seg))++k;
            if(k>=count)return 0;
            m.put32(first,std::uint32_t(k));
            while(k<count&&id(k)==std::uint32_t(seg))++k;
            m.put32(last,std::uint32_t(k-1));
            return m.u32(last)-m.u32(first)+1u;
        }
        std::int32_t k=at;
        while(k>=0&&id(k)!=std::uint32_t(seg))--k;
        if(k<0)return 0;
        m.put32(last,std::uint32_t(k));
        while(k>=0&&id(k)==std::uint32_t(seg))--k;
        m.put32(first,std::uint32_t(k+1));
        return m.u32(last)-m.u32(first)+1u;
    }
    const std::uint32_t seg=std::uint32_t(std::int32_t(m.i16(path+8u)));               // 43D6E0
    const std::uint32_t side=m.u32(path+4u);
    if(side==0x65u||side==0x67u){
        const std::uint32_t recs=m.u32(0x780110u+part*4u);
        auto flags=[&](std::int32_t k)->std::uint32_t{return (!recs||k==-1)?0u:m.u16(recs+std::uint32_t(k)*0x40u+0x3cu);};
        auto skip=[&](std::int32_t k){
            if((flags(k)&0x400u)&&k<count)while(!(flags(k)&0x800u)){++k;if(k>=count)break;}
            return k;
        };
        std::int32_t k=0;
        while(k<count){k=skip(k);if(id(k)==seg)break;++k;}
        m.put32(first,std::uint32_t(k));
        while(k<count-1){
            k=skip(k);
            if(id(k+1)!=seg)break;
            ++k;
        }
        return result(k);
    }
    std::int32_t k=0;
    while(k<count&&id(k)!=seg)++k;
    m.put32(first,std::uint32_t(k));
    while(k<count-1){
        const std::uint32_t next=k==-2?0u:m.u16(ids+std::uint32_t(k)*2u+1u);
        if(next!=seg)break;
        ++k;
    }
    return result(k);
}
// Opcode 7: places the player car on the course matrix.
void goal_place(PcRaceContext& c,std::uint32_t rec,std::uint32_t car){
    auto& m=c.m;auto& s=c.matrices;
    bool network=false;
    if(call(c,0x46c3f0u,{})!=0u&&m.u32(rec+4)==1u)network=true;
    else if((call(c,0x4957f0u,{})&0xffu)&&call(c,0x495b10u,{})==2u)network=true;
    else if((call(c,0x48b310u,{})&0xffu)&&std::int32_t(call(c,0x48b320u,{}))<0x3c)network=true;
    else if((call(c,0x495490u,{})&0xffu)&&std::int32_t(call(c,0x4954f0u,{}))<0x3c)network=true;
    Frame f(m);
    if(!network){
        const std::uint32_t which=m.u32(rec+4);
        std::uint32_t matrix;
        if(which==0u)matrix=0x7d2da0u;                       // 44BEA0
        else if(which==1u)matrix=0x7d3190u;                  // 44BEC0
        else return;
        driving::pc_matrix_push_load(s,m.bytes(matrix,0x40));                  // 409F90
        put3(m,car+0x14,driving::pc_matrix_point(s,{0.0f,0.0f,0.0f}));           // 40A7D0(car+14, X+64 = 0)
        angles_449640(m,L(0x18),matrix);
        car_angles(m,car,L(0x18));
        driving::pc_matrix_pop(s);                                             // 40A010
        return;
    }
    call(c,0x46c400u,{0u});
    for(std::uint32_t k=0;k<4;++k)m.put32(L(0x24)+k*4,m.u32(car+0x5c+k*4));
    driving::pc_matrix_push_load(s,m.bytes(0x7d3190u,0x40));                   // 44BEC0, 409F90
    driving::pc_d3dx_matrix_inverse(s.current(),nullptr,s.current());          // 40A240
    put3(m,L(0x40),driving::pc_matrix_point(s,get3(m,car+0xd28)));
    driving::pc_matrix_pop(s);
    m.put32(L(0x28),0.0f>m.f32(L(0x40))?0x64u:0x65u);                          // comiss/ja
    goal_segments_43d920(c,L(0x3c),L(0x34),L(0x24),0xffffffffu);
    const std::uint32_t table=m.u32(0x780110u+m.u32(L(0x24))*4u);
    const std::uint32_t a=table+(m.u32(L(0x3c))<<6),b=table+(m.u32(L(0x34))<<6);
    for(std::uint32_t k=0;k<3;++k){m.put32(L(0x4c)+k*4,m.u32(a+0x24+k*4));m.put32(L(0x58)+k*4,m.u32(b+0x18+k*4));}
    put3(m,L(0x40),weighted_sum_40f180(get3(m,L(0x58)),0.5f,get3(m,L(0x4c)),0.5f));
    driving::pc_matrix_push_load(s,m.bytes(0x7d3190u,0x40));
    put3(m,car+0x14,driving::pc_matrix_point(s,get3(m,L(0x40))));
    driving::pc_matrix_rotate_y(s,m.u32(L(0x28))==0x64u?1.57079637050628662f:-1.57079637050628662f);   // 40A410
    driving::pc_matrix_get(s,m.bytes(L(0x70),0x40));                          // 40A0D0
    angles_449640(m,L(0x18),L(0x70));
    car_angles(m,car,L(0x18));
    driving::pc_matrix_pop(s);
}
// One 513650 sample of channel k of the motion record.
X87 sample(const PcRaceMemory& m,std::uint32_t data,std::uint32_t k,float t){
    const std::int16_t count=m.i16(data+k*8);
    const std::uint32_t keys=m.u32(data+k*8+4);
    driving::Bytes view(nullptr,0);
    if(count>0)view=driving::Bytes(m.at(keys,std::size_t(count)*16u),std::size_t(count)*16u);
    return driving::pc_sample_crash_channel_x87({count,view},t);
}
}
void goal_camera_map_tables(PcRaceMemory& m){
    static const std::array<std::uint8_t,0x8573bcu-0x8572ecu> bss_8572ec{};
    m.map_const(0x8572ecu,bss_8572ec.data(),bss_8572ec.size());
    for(std::size_t i=0;i<EmbeddedExeRangeCount;++i){
        const auto& r=EmbeddedExeRanges[i];
        if(r.base==0x5e61a0u||r.base==0x6bb3c8u)m.map_const(r.base,r.data,r.size);
    }
}
void goal_script_486730(PcRaceContext& c){
    auto& m=c.m;
    std::uint32_t rec=m.u32(0x653788u+std::uint32_t(m.u8(0x82e7d4u))*4u);
    const std::uint32_t camera=m.u32(0x79f574u);             // [esp+38]
    if(m.i16(rec+0x24)<0)return;
    const std::uint32_t car=m.u32(0x799d18u);
    for(;m.i16(rec+0x24)>=0;rec+=0x2cu){
        if(m.u32(rec+0x28)==0u)continue;
        if(m.i16(rec+0x24)>m.i16(0x82e7c4u))continue;
        const std::uint32_t op=m.u32(rec);
        m.put32(rec+0x28,0);
        if(op>0x1eu)continue;
        auto w=[&](std::uint32_t o){return m.u32(rec+o);};
        switch(op){
        case 0:call(c,0x428320u,{w(4),w(8),w(0xc)});break;
        case 1:call(c,0x401000u,{w(4),w(8),w(0xc)});break;
        case 2:call(c,0x42e020u,{w(4),w(0x14),w(8)});break;
        case 3:if(w(4)==3u)call(c,0x401050u,{});else call(c,0x401030u,{w(4)});break;
        case 4:{
            const std::int8_t a=std::int8_t(call(c,0x48b1e0u,{}));
            call(c,0x401000u,{w(4),std::uint32_t(std::int32_t(a)),1u});
            const std::int8_t b=std::int8_t(call(c,0x48b1e0u,{}));
            const std::uint32_t level=call(c,0x427aa0u,{std::uint32_t(std::int32_t(b))});   // x87 result, stored as a float
            call(c,0x42e020u,{w(4),level,0u});
            break;}
        case 5:
            if(w(4)==0x16au&&call(c,0x4493c0u,{}))break;
            if(call(c,0x450240u,{})==6u)break;
            if(call(c,0x450240u,{})==7u)break;
            call(c,0x4249f0u,{w(4)});
            break;
        case 6:   // 486942 SelectPlCarCtrl
            if(w(4)==0u){call(c,0x440bb0u,{8u,0x4a8330u});m.put32(car+4,m.u32(car+4)&0xff7fffffu);}
            else if(w(4)==1u){call(c,0x440bb0u,{8u,0x475720u});m.put32(car+4,m.u32(car+4)|0x800000u);}
            break;
        case 7:goal_place(c,rec,car);break;
        case 8:m.put32(0x82e7c8u,w(4));break;
        case 9:case 10:{
            static constexpr std::uint32_t Ids[7][2]{{8,1},{0x181,1},{0x18d,1},{0x187,1},{0x20,0xfa},{0x11a,0x1a},{0x16a,0x15}};
            for(const auto& r:Ids)call(c,op==9u?0x440a10u:0x440a30u,{r[0],r[1]});
            break;}
        case 11:
            if(w(4)==1u){m.put32(car+0x38,0xffu);m.put32(car+0x1f4,0x1e0u);}
            else{m.put32(car+0x38,0);m.put32(car+0x1f4,0);}
            break;
        case 12:m.put32(0x82e7e4u,w(4));m.put16(0x82e7dcu,m.u16(rec+8));break;
        case 13:call(c,0x487b70u,{m.u32(0x799b38u+w(4)*0x3cu),w(8),0u});break;
        case 14:m.put32(0x82e7d0u,w(4));break;
        case 15:m.put32(0x82e7c0u,w(4));break;
        case 16:call(c,0x41fb10u,{});call(c,0x49a650u,{});break;
        case 17:{const std::uint32_t r=call(c,0x4532e0u,{});call(c,0x428320u,{r==1u?w(4):w(8),w(0xc),w(0x10)});break;}
        case 18:call(c,0x49a650u,{0x16au,w(4)});call(c,0x49a650u,{0x16bu,w(4)});break;
        case 19:m.put32(camera+0xbc,w(0x14));break;
        case 20:m.put32(camera+0xc0,w(0x14));break;
        case 21:call(c,0x41fb10u,{});call(c,0x41c420u,{});call(c,0x4a7d70u,{w(4),w(8)});break;
        case 22:call(c,0x49f7c0u,{w(4),w(8)});break;
        case 23:call(c,0x428460u,{w(4),w(8),w(0xc),std::uint32_t(cvtt(m.f32(rec+0x14))),std::uint32_t(cvtt(m.f32(rec+0x18)))});break;
        case 24:{
            const std::uint32_t r=call(c,0x4532e0u,{});
            call(c,0x428460u,{r==1u?w(4):w(8),w(0xc),w(0x10),std::uint32_t(cvtt(m.f32(rec+0x14))),std::uint32_t(cvtt(m.f32(rec+0x18)))});
            break;}
        case 25:call(c,0x440a10u,{w(4),w(8)});break;
        case 26:call(c,0x440a30u,{w(4),w(8)});break;
        case 27:call(c,0x49f7d0u,{w(4),w(8)});break;
        case 28:case 29:call(c,0x4502e0u,{w(4)});break;
        case 30:m.putf(car+0x2dc,0.0f);m.put32(0x82e7d8u,0);break;
        }
    }
}
std::uint32_t goal_motion_486ef0(PcRaceContext& c){
    auto& m=c.m;
    const std::int32_t scene=std::int8_t(m.u8(0x82e7d4u));   // 513730/513740: movsx
    const std::uint32_t data=m.u32(0x5e61d0u+std::uint32_t(scene)*8u);
    const std::uint32_t car=m.u32(0x799d18u),camera=m.u32(0x79f574u);
    if(data==0u)return 0u;
    const float t=m.f32(0x82e7e8u);
    if(t>m.f32(0x5e61d4u+std::uint32_t(scene)*8u))return 0u;  // fcomip; jbe (unordered continues)
    auto s=[&](std::uint32_t k){return sample(m,data,k,t);};
    auto angle=[&](std::uint32_t k){return std::uint16_t(cvtt(driving::x87_float(s(k)*X87(K6282c0))));};
    m.putf(car+0x2d8,driving::x87_float(s(0)));
    if(m.u32(0x78026cu)==0x10u&&call(c,0x44ff10u,{})==0u)m.putf(car+0x2dc,driving::x87_float(s(1)));
    else m.put32(car+0x2dc,m.u32(0x82ea70u));
    m.putf(car+0x2e0,driving::x87_float(s(2)));
    m.putf(car+0x2e8,driving::x87_float(s(3)));
    m.putf(car+0x2e4,driving::x87_float(s(4)));
    m.putf(car+0x2ec,driving::x87_float(s(5)));
    m.put16(0x82ea7au,angle(6));
    m.put16(0x82eb6eu,angle(7));
    m.put16(0x82ea78u,angle(8));
    m.put16(0x82eb6cu,angle(9));
    m.put16(0x82ec60u,angle(10));
    m.put16(0x82ed54u,angle(11));
    m.put16(car+0x32,m.u16(0x82ea7au));
    m.put16(0x82e7ccu,m.u16(0x82ea7au));
    if(m.u32(0x82e7d0u)!=1u)return 1u;
    m.putf(camera+0xe0,driving::x87_float(s(12)));
    m.putf(camera+0xe4,driving::x87_float(s(13)));
    m.putf(camera+0xe8,driving::x87_float(s(14)));
    m.putf(camera+0xec,driving::x87_float(s(15)));
    m.putf(camera+0xf0,driving::x87_float(s(16)));
    m.putf(camera+0xf4,driving::x87_float(s(17)));
    m.putf(camera+0x130,driving::x87_float(s(18)));
    const float ratio=driving::x87_float(X87(K5c09bc)/(s(19)*X87(K5c09c0)));   // fmul; fdivr; fstp
    const X87 half=driving::x87_atan2(X87(ratio),X87(1.0f));                    // 449350
    const X87 fov=half+half;                                                     // fadd st,st
    m.putf(camera+0xa4,driving::x87_float(fov));
    m.putf(camera+0xac,driving::x87_float(fov));
    if(m.u32(0x780258u)==4u)m.putf(camera+0xe4,m.f32(camera+0xe4)+K628064);
    return 1u;
}
void camera_override_control_4874a0(PcRaceContext& c){
    auto& m=c.m;
    if(m.u32(0x82e7d8u)==0u)return;
    goal_script_486730(c);
    const std::uint32_t r=goal_motion_486ef0(c);
    const float t=m.f32(0x82e7e0u)+K62806c;
    m.put32(0x82e7ecu,r);
    m.put16(0x82e7c4u,std::uint16_t(cvtt(t)));
    m.putf(0x82e7e0u,t);
    m.putf(0x82e7e8u,t*K6280e8);
}
void pas_save_4755c0(PcRaceMemory& m,std::uint32_t car){
    for(std::uint32_t k=0;k<3;++k)m.put32(car+0x16c+k*4,m.u32(car+0x14+k*4));    // VM 4755C6: lea edx,[eax+0x16C]
    m.put32(car+0x178,m.u32(car+0x1c4));
    m.put16(car+0x17c,m.u16(car+0x2c));m.put16(car+0x17e,m.u16(car+0x2e));m.put16(car+0x180,m.u16(car+0x30));
    for(std::uint32_t k=0;k<4;++k)m.put32(car+0x184+k*4,m.u32(car+0x5c+k*4));
    for(std::uint32_t k=0;k<3;++k)m.put32(car+0x1040+k*4,m.u32(car+0x2d8+k*4));
    for(std::uint32_t k=0;k<3;++k)m.put32(car+0x1034+k*4,m.u32(car+0x2e4+k*4));
    m.put16(car+0xc2e,m.u16(car+0xc2c));
}
void pas_pl_car_475720(PcRaceContext& c,std::uint32_t car){
    auto& m=c.m;auto& s=c.matrices;
    Frame f(m);
    m.put32(L(0x30),car);                                    // the argument slot, reused for 43EB60's result
    pas_save_4755c0(m,car);
    m.put32(car+4,m.u32(car+4)|0x800000u);
    const std::uint8_t scene=m.u8(0x82e7d4u);                // 4872F0
    if(scene!=0u&&(scene<=7u||scene>0xeu)){
        call(c,0x4a2650u,{car});
        driving::pc_matrix_push_unit(s);                     // 409F30
        driving::pc_matrix_get(s,m.bytes(car+0xf0,0x40));    // 40A0D0
        driving::pc_matrix_pop(s);
        call(c,0x4a2ee0u,{car});
        call(c,0x4a4710u,{car});
    }else call(c,0x4a8330u,{car});
    driving::pc_matrix_push_load(s,m.bytes(car+0xb0,0x40));
    for(std::uint32_t i=0;i<4;++i){
        const std::uint32_t rec=m.u32(0x82ea38u+i*4u),base=car+i*12u;
        const auto p=driving::pc_matrix_point(s,get3(m,base+0x130));
        put3(m,L(0x14),p);put3(m,L(0x20),p);
        m.put32(L(0x10),rec+0x10);
        call(c,0x43eb60u,{0x400u,L(0x14),rec+0x10,0u,L(0x30)});
        const std::uint32_t kind=m.u32(L(0x30));
        if(kind==1u)continue;
        m.put32(car+0x24c+i*4,kind);
        m.putf(rec+0x3c,m.f32(L(0x18)));
        m.putf(rec+0xe0,0.0f);
        m.put16(rec+0xee,0);
        m.putf(rec+0xe4,1.0f);
        const std::uint32_t tire=m.u32(0x5b2f68u+std::uint32_t(std::int32_t(std::int8_t(m.u8(car+0x11))))*4u)+(i<<5)+0x98u;   // 46BBF0
        float y=m.f32(tire+4)+m.f32(L(0x18));
        m.putf(L(0x18),y);
        y=y-m.f32(L(0x24));
        y=y+m.f32(base+0x134);
        m.putf(base+0x134,y);
        call(c,0x43d390u,{m.u32(m.u32(L(0x10))),m.u32(car+0x5c),rec+0x70});
    }
    driving::pc_matrix_pop(s);
}
}
