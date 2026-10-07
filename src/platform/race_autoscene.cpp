#include "platform/race_autoscene.hpp"
#include "driving/pc_crash.hpp"
#include "driving/pc_matrix_stack.hpp"
#include "driving/pc_x87.hpp"
#include "platform/vehicle_model_draw.hpp"
#include <array>
#include <cstring>
#include <string>
namespace outrun::platform {
namespace {
using driving::X87;
std::uint32_t call(PcRaceContext& c,std::uint32_t pc,std::initializer_list<std::uint32_t> args,std::uint32_t ecx=0){
    PcRaceCall k;k.pc=pc;k.ecx=ecx;k.argc=std::uint32_t(args.size());
    std::size_t i=0;for(auto a:args)k.args[i++]=a;
    return c.service(k);
}
std::uint32_t fb(float f){std::uint32_t v;std::memcpy(&v,&f,4);return v;}
float bf(std::uint32_t v){float f;std::memcpy(&f,&v,4);return f;}
std::int32_t cvtt(float f){
    if(!(f>-2147483904.0f&&f<2147483648.0f))return std::int32_t(0x80000000u);
    return std::int32_t(f);
}
constexpr std::uint32_t Work=0x799ca0u;
float k(PcRaceMemory& m,std::uint32_t a){return m.f32(a);}
// 513650(channel, time): the {count, keys} pair's sample (x87).
X87 sample(PcRaceMemory& m,std::uint32_t ch,float t){
    const std::int16_t count=m.i16(ch);
    const std::uint32_t keys=m.u32(ch+4u);
    driving::Bytes view(nullptr,0);
    if(count>0)view=driving::Bytes(m.at(keys,std::size_t(count)*16u),std::size_t(count)*16u);
    return driving::pc_sample_crash_channel_x87({count,view},t);
}
driving::CourseProbe vec3(PcRaceMemory& m,std::uint32_t a){return {m.f32(a),m.f32(a+4u),m.f32(a+8u)};}
// 40A2D0 (translate by the vector at a), 40A4E0 (rotate z, y, x by the angles at a).
void translate_40a2d0(PcRaceContext& c,std::uint32_t a){driving::pc_matrix_translate_vector(c.matrices,vec3(c.m,a));}
void rotate_40a4e0(PcRaceContext& c,std::uint32_t a){
    const float x=c.m.f32(a),y=c.m.f32(a+4u),z=c.m.f32(a+8u);
    driving::pc_matrix_rotate_z(c.matrices,z);driving::pc_matrix_rotate_y(c.matrices,y);driving::pc_matrix_rotate_x(c.matrices,x);
}
void store_40a0d0(PcRaceContext& c,std::uint32_t out){driving::pc_matrix_get(c.matrices,c.m.bytes(out,0x40));}
void load_40a170(PcRaceContext& c,std::uint32_t in){driving::pc_matrix_load(c.matrices,c.m.bytes(in,0x40));}
std::string cstring(PcRaceMemory& m,std::uint32_t a){std::string s;for(;;++a){const auto ch=m.u8(a);if(!ch)break;s+=char(ch);}return s;}
bool has(PcRaceMemory& m,std::uint32_t name,std::uint32_t needle){
    const std::string n=cstring(m,needle);return cstring(m,name).find(n)!=std::string::npos;   // 581000 (strstr)
}
// The PC stack buffers passed by address (44FC60's size slot, the motion names).
struct Locals {
    PcRaceMemory& m;std::size_t mark;std::array<std::uint8_t,0x100> bytes{};
    explicit Locals(PcRaceMemory& mem):m(mem),mark(mem.mark()){m.map(PcAutosceneLocals,bytes.data(),bytes.size());}
    ~Locals(){m.release(mark);}
};
// The per-scene callback of slot k (51B740, measured 0); a non-zero one is not ported.
void scene_callback(std::uint32_t fn){if(fn)throw PcAutosceneUnported{fn};}
// 440D10(0) 440D50(0) 580253(size + 8) 440D30 440D70: a block whose last 8 bytes are
// {start, 0}; returns the address of that trailer.
std::uint32_t alloc_block(PcRaceContext& c,std::uint32_t size){
    call(c,0x440d10u,{0u});call(c,0x440d50u,{0u});
    const std::uint32_t p=call(c,0x580253u,{size+8u});
    const std::uint32_t blk=p+size;
    c.m.put32(blk,p);c.m.put32(blk+4u,0);
    call(c,0x440d30u,{});call(c,0x440d70u,{});
    return blk;
}

// 4B5180 (ESI = work): the camera of the +64 track at time +4: eye +30..+38, target +3C..+44,
// fov +48 = 2 atan(([track] * 0.5) / (channel 7 * 0.03937)), roll +4C.
void camera_4b5180(PcRaceContext& c,std::uint32_t w){
    auto& m=c.m;
    const std::uint32_t t=m.u32(w+0x64u);
    if(!t)return;
    static constexpr std::uint32_t Out[6]{0x30u,0x34u,0x38u,0x3cu,0x40u,0x44u};
    for(std::uint32_t i=0;i<6u;++i)m.putf(w+Out[i],driving::x87_float(sample(m,t+4u+i*8u,m.f32(w+4u))));
    const X87 a=sample(m,t+0x3cu,m.f32(w+4u));
    const X87 e=X87(m.f32(t))*X87(k(m,0x628064u));
    const X87 q=e/(a*X87(k(m,0x5c09c0u)));
    X87 r=driving::x87_atan2(q,X87(1.0f));r=r+r;
    m.putf(w+0x48u,driving::x87_float(r));
    m.putf(w+0x4cu,driving::x87_float(sample(m,t+0x34u,m.f32(w+4u))));
}
void camera_set_483e10(PcRaceContext& c,std::uint32_t w){call(c,0x483e10u,{w+0x30u,w+0x3cu,w+0x4cu,w+0x48u});}
// 4B5230 (EAX = work): 1 when the frame ([w+4] * 60 + 0.1) is the last frame of one of the
// +64 track's key spans (key i at f, key i+1 at f + 1); writes +30 like 4B5180.
bool key_step_4b5230(PcRaceContext& c,std::uint32_t w){
    auto& m=c.m;
    float f=m.f32(w+4u)*k(m,0x628134u);f=f+k(m,0x62813cu);
    const std::int32_t frame=cvtt(f);
    const std::uint32_t t=m.u32(w+0x64u);
    if(!t)return false;
    m.putf(w+0x30u,driving::x87_float(sample(m,t+4u,m.f32(w+4u))));
    const std::int32_t n=m.i32(t+4u)-1;std::uint32_t key=m.u32(t+8u);
    for(std::int32_t i=0;i<n;++i,key+=0x10u){
        float a=m.f32(key)*k(m,0x628134u);a=a+k(m,0x62813cu);
        float b=m.f32(key+0x10u)*k(m,0x628134u);b=b+k(m,0x62813cu);
        const std::int32_t ia=cvtt(a),ib=cvtt(b);
        if(ia>frame||frame>ib)continue;
        if(ia+1==ib)return true;
    }
    return false;
}
// 4B52E0 (ESI = work): frees the matrix block +84, the robot records of the kind-2 nodes,
// 42E020(0, 0, 0x3C) and the node block +6C.
void release_4b52e0(PcRaceContext& c,std::uint32_t w){
    auto& m=c.m;
    if(m.u32(w+0x84u))call(c,0x440cd0u,{w+0x84u});
    const std::uint32_t n=m.u32(w+0x68u);
    m.put32(w+0x80u,0);m.put32(w+0x88u,0);
    if(!n||!m.u32(w+0x70u))return;
    for(std::uint32_t i=0;std::int32_t(i)<m.i32(w+0x68u);++i){
        const std::uint32_t node=m.u32(w+0x70u)+i*0x24u;
        if(m.u32(node+8u)==2u&&m.u32(node+0x10u))call(c,0x440cd0u,{node+0xcu});
    }
    call(c,0x42e020u,{0u,0u,0x3cu});
    if(m.u32(w+0x6cu))call(c,0x440cd0u,{w+0x6cu});
    m.put32(w+0x70u,0);m.put32(w+0x68u,0);
}
// The relocations of the loaded script (base [w+60]).
void reloc(PcRaceMemory& m,std::uint32_t at,std::uint32_t base){m.put32(at,m.u32(at)+base);}
void reloc_pairs(PcRaceMemory& m,std::uint32_t ch,std::uint32_t pairs,std::uint32_t base){
    for(std::uint32_t k2=0;k2<pairs;++k2)if(m.u32(ch+k2*8u))reloc(m,ch+k2*8u+4u,base);
}
// 4B5380 (EAX = camera track, ECX = work): eight channel pairs from +4.
void reloc_camera_4b5380(PcRaceMemory& m,std::uint32_t e,std::uint32_t w){reloc_pairs(m,e+4u,8u,m.u32(w+0x60u));}
// 4B53F0(work, node): +C name, +38 channels (ten pairs), +40 children, recursively.
void reloc_node_4b53f0(PcRaceMemory& m,std::uint32_t w,std::uint32_t node){
    const std::uint32_t base=m.u32(w+0x60u);
    for(std::uint32_t o:{0xcu,0x38u,0x40u})if(m.u32(node+o))m.put32(node+o,m.u32(node+o)+base);
    if(const std::uint32_t ch=m.u32(node+0x38u))reloc_pairs(m,ch,10u,m.u32(w+0x60u));
    for(std::uint32_t i=0;std::int32_t(i)<m.i32(node+0x3cu);++i)reloc_node_4b53f0(m,w,m.u32(node+0x40u)+i*0x44u);
}
// 4B54E0 (EAX = robot track, ECX = work): +8, +C, then the +C list (+2C, +30, +38 next) and
// each item's ten channel pairs at +30.
void reloc_robot_4b54e0(PcRaceMemory& m,std::uint32_t e,std::uint32_t w){
    if(!m.u32(e+0xcu))return;
    reloc(m,e+8u,m.u32(w+0x60u));
    std::uint32_t d=m.u32(e+0xcu)+m.u32(w+0x60u);m.put32(e+0xcu,d);
    while(d){
        for(std::uint32_t o:{0x2cu,0x30u,0x38u})if(m.u32(d+o))m.put32(d+o,m.u32(w+0x60u)+m.u32(d+o));
        if(const std::uint32_t ch=m.u32(d+0x30u))reloc_pairs(m,ch,10u,m.u32(w+0x60u));
        d=m.u32(d+0x38u);
    }
}
// 4B55D0 (ECX = event list, EBX = work): kinds 6 / 7 hold a name pointer at +C unless its
// high word is a sound bank (0xBE..0xCF, 0x1EB..0x1ED).
void reloc_events_4b55d0(PcRaceMemory& m,std::uint32_t list,std::uint32_t w){
    if(m.u32(list)==0xffffffffu)return;
    std::uint32_t item=list;
    for(;;){
        const std::int32_t kind=m.i32(item+4u);
        if(kind>=6&&kind<=7){
            const std::uint32_t v=m.u32(item+0xcu);
            const std::uint32_t hi=std::uint32_t(std::int32_t(v)>>16);
            const bool bank=(hi>=0xbeu&&hi<=0xcfu)||(hi>=0x1ebu&&hi<=0x1edu);
            if(!bank)m.put32(item+0xcu,m.u32(w+0x60u)+v);
        }
        item+=m.u32(item+8u)*4u;
        if(m.u32(item)==0xffffffffu)return;
    }
}
// 4B5630 (ECX = list, EDX = work): pairs {id, pointer} until -1, each pointee holding two
// {count, keys} channels.
void reloc_list_4b5630(PcRaceMemory& m,std::uint32_t list,std::uint32_t w){
    if(m.u32(list)==0xffffffffu)return;
    for(;;){
        const std::uint32_t p=m.u32(list+4u)+m.u32(w+0x60u);m.put32(list+4u,p);
        if(m.u32(p))reloc(m,p+4u,m.u32(w+0x60u));
        if(m.u32(p+8u))reloc(m,p+0xcu,m.u32(w+0x60u));
        const std::uint32_t next=m.u32(list+8u);list+=8u;
        if(next==0xffffffffu)return;
    }
}
// 4B6430 (EAX = work): the entries of the script header ([d+8] count, [d+C] 12-byte rows).
void relocate_4b6430(PcRaceMemory& m,std::uint32_t w){
    const std::uint32_t d=m.u32(w+0x60u);
    for(std::uint32_t i=0;std::int32_t(i)<m.i32(d+8u);++i){
        const std::uint32_t e=m.u32(d+0xcu)+i*0xcu;
        if(m.u32(e+4u))m.put32(e+4u,m.u32(e+4u)+d);
        if(m.u32(e+8u))m.put32(e+8u,m.u32(e+8u)+d);
        switch(m.u32(e)){
        case 0:reloc_camera_4b5380(m,m.u32(e+4u),w);break;
        case 1:reloc_node_4b53f0(m,w,m.u32(e+4u));break;
        case 2:reloc_robot_4b54e0(m,m.u32(e+4u),w);reloc_events_4b55d0(m,m.u32(e+8u),w);break;
        case 5:{const std::uint32_t q=m.u32(e+4u);
            if(m.u32(q))reloc(m,q+4u,m.u32(w+0x60u));
            if(m.u32(q+8u))reloc(m,q+0xcu,m.u32(w+0x60u));
            break;}
        case 6:reloc_list_4b5630(m,m.u32(e+4u),w);break;
        default:break;
        }
    }
}
// 4B5FF0 (ESI = node): car part names (containing "CAR") -> tokens BD0000..BD001A.
void car_part_4b5ff0(PcRaceMemory& m,std::uint32_t node){
    const std::uint32_t name=m.u32(node+0xcu);
    if(!has(m,name,0x5c6a54u))return;
    struct P{std::uint32_t s,t;};
    static constexpr P Parts[]{{0x5c6a4cu,0xbd0000u},{0x5c6a44u,0xbd0001u},{0x5c6a3cu,0xbd0002u},{0x5c6a30u,0xbd0003u},
        {0x5c6a24u,0xbd0004u},{0x5c6a18u,0xbd0005u},{0x5c6a0cu,0xbd0006u},{0x5c6a04u,0xbd001au},{0x5c69f8u,0xbd0007u},
        {0x5c69ecu,0xbd0008u},{0x5c69dcu,0xbd0009u},{0x5c69d0u,0xbd000au},{0x5c69c4u,0xbd000bu},{0x5c69b8u,0xbd000cu},
        {0x5c69a8u,0xbd000du},{0x5c699cu,0xbd000eu},{0x5c6990u,0xbd000fu},{0x5c6984u,0xbd0010u},{0x5c6974u,0xbd0011u},
        {0x5c6968u,0xbd0012u},{0x5c695cu,0xbd0013u},{0x5c6950u,0xbd0014u},{0x5c6940u,0xbd0015u},{0x5c6934u,0xbd0016u},
        {0x5c692cu,0xbd0017u},{0x5c6924u,0xbd0018u},{0x5c691cu,0xbd0019u}};
    for(const auto& p:Parts)if(has(m,name,p.s)){m.put32(node+8u,p.t);return;}
}
// 4B6320 (ESI = node): eye / face names (containing "EYE") -> BD001C..BD001F.
void eye_4b6320(PcRaceMemory& m,std::uint32_t node){
    const std::uint32_t name=m.u32(node+0xcu);
    if(!has(m,name,0x5c6a88u))return;
    if(has(m,name,0x5c6a80u)){m.put32(node+8u,0xbd001cu);return;}
    if(has(m,name,0x5c6a78u)){m.put32(node+8u,0xbd001du);return;}
    if(has(m,name,0x5c6a70u)){m.put32(node+8u,0xbd001eu);return;}
    if(has(m,name,0x5c6a68u)){m.put32(node+8u,0xbd001fu);return;}
    if(has(m,name,0x5c6a60u)){m.put32(node+8u,0xbd001eu);return;}
    if(has(m,name,0x5c6a58u))m.put32(node+8u,0xbd001fu);
}
// 4B6510(work, node): counts the matrix slots (+34 != -1) in +80, resolves the model token
// (448CD0 of the name, then the car part / eye / "KEM" smoke tokens), children, +0 |= 0x300.
void node_setup_4b6510(PcRaceContext& c,std::uint32_t w,std::uint32_t node){
    auto& m=c.m;
    if(m.u32(node+0x34u)!=0xffffffffu)m.put32(w+0x80u,m.u32(w+0x80u)+1u);
    if(const std::uint32_t name=m.u32(node+0xcu)){
        m.put32(node+8u,call(c,0x448cd0u,{name}));
        car_part_4b5ff0(m,node);
        eye_4b6320(m,node);
        if(has(m,m.u32(node+0xcu),0x5c6a8cu))m.put32(node+8u,0xbd0020u);
    }
    for(std::uint32_t i=0;std::int32_t(i)<m.i32(node+0x3cu);++i)node_setup_4b6510(c,w,m.u32(node+0x40u)+i*0x44u);
    std::uint32_t f=m.u32(node+0)|0x100u;m.put32(node,f);
    if(!(f&0x200u))m.put32(node,f|0x200u);
}
// 4B5A80 (EAX = character): the robot kind.
std::uint32_t robot_kind_4b5a80(PcRaceMemory& m,std::int32_t v){
    const std::int32_t mode=m.i32(0x7d3a7cu);
    std::uint32_t r;
    if(v==0)r=0x10u;
    else if(v<0||v>2){const std::uint32_t p=m.u32(0x78024cu);r=(p==1u||p==3u)?3u:4u;}
    else{
        if(mode==1)return 0xdu;
        if(mode==2)return 0xeu;
        if(mode==3)return 0xfu;
        r=(v!=1?1u:0u)+0xdu;
    }
    if(mode==0&&r==0xeu)r=0xfu;
    return r;
}
// 4B65B0 (EBX = node): a robot record (0x70 bytes in a block) bound to a free event (440890)
// whose work gets 487C30(kind) / 487D40(0).
void robot_4b65b0(PcRaceContext& c,std::uint32_t node){
    auto& m=c.m;
    const std::uint32_t blk=alloc_block(c,0x70u);
    if(!blk)return;
    call(c,0x49a650u,{blk});call(c,0x49a650u,{blk});
    const std::uint32_t q=m.u32(blk);
    for(std::uint32_t a=0;a<0x70u;a+=4)m.put32(q+a,0);
    m.put32(node+0x10u,q);
    m.put32(node+0xcu,blk);                                           // 440CC0(node + C, &block)
    m.put32(q+8u,m.u32(node+0x18u));
    m.put32(q+4u,robot_kind_4b5a80(m,m.i32(m.u32(node+0x18u)+4u)));
    m.put32(q,0x200u);
    const std::uint32_t id=call(c,0x440890u,{});
    m.put32(q+0xcu,id);
    if(id==0x19au){m.put32(q+0x10u,0);return;}
    const std::uint32_t rw=m.u32(0x799b38u+id*0x3cu);
    m.put32(q+0x10u,rw);
    call(c,0x487c30u,{rw,std::uint32_t(m.u8(q+4u))});
    call(c,0x487d40u,{m.u32(q+0x10u),0u});
}
// 4B66B0 (EAX = work): the node list (+68 count, +70 array of 0x24-byte rows) from the script
// entries (plus the row's extra entry), then the matrix block (+80 slots, +88 array).
void setup_4b66b0(PcRaceContext& c,std::uint32_t w){
    auto& m=c.m;
    const std::uint32_t d=m.u32(w+0x60u);
    if(!d)return;
    const std::uint32_t row=m.u32(w+0x5cu);
    std::uint32_t n=m.u32(d+8u);if(m.u32(row+8u))++n;
    m.put32(w+0x68u,n);
    if(!n)return;
    m.put32(w+0x6cu,alloc_block(c,n*0x24u));
    call(c,0x49a650u,{m.u32(w+0x6cu)});call(c,0x49a650u,{m.u32(w+0x6cu)});
    m.put32(w+0x70u,m.u32(m.u32(w+0x6cu)));
    std::uint32_t i=0;
    for(;std::int32_t(i)<m.i32(d+8u);++i){
        const std::uint32_t node=m.u32(w+0x70u)+i*0x24u,e=m.u32(d+0xcu)+i*0xcu;
        m.put32(node,1u);m.put32(node+4u,i);m.put32(node+8u,m.u32(e));m.put32(node+0x18u,m.u32(e+4u));
        m.put32(node+0x14u,e);m.put32(node+0x1cu,m.u32(e+8u));m.put32(node+0x20u,m.u32(e+8u));
        switch(m.u32(e)){
        case 0:m.put32(w+0x64u,m.u32(e+4u));camera_4b5180(c,w);camera_set_483e10(c,w);break;
        case 1:m.put32(node+0x10u,m.u32(node+0x18u));node_setup_4b6510(c,w,m.u32(e+4u));break;
        case 2:robot_4b65b0(c,node);break;
        case 5:m.put32(w+0x74u,node);break;
        case 6:m.put32(node,0x81u);break;
        default:break;
        }
    }
    if(const std::uint32_t x=m.u32(row+8u)){
        const std::uint32_t node=m.u32(w+0x70u)+(m.u32(w+0x68u)-1u)*0x24u;
        m.put32(node,1u);m.put32(node+4u,i);m.put32(node+8u,m.u32(x));m.put32(node+0x18u,m.u32(x+4u));
        m.put32(node+0x14u,x);m.put32(node+0x1cu,m.u32(x+8u));m.put32(node+0x20u,m.u32(x+8u));
    }
    if(!m.u32(w+0x80u))return;
    m.put32(w+0x84u,alloc_block(c,m.u32(w+0x80u)*64u));
    call(c,0x49a650u,{m.u32(w+0x84u)});call(c,0x49a650u,{m.u32(w+0x84u)});
    const std::uint32_t p=m.u32(m.u32(w+0x84u));
    m.put32(w+0x88u,p);
    for(std::uint32_t a=0;a<m.u32(w+0x80u)*64u;++a)m.put8(p+a,0);
}
// 4B63E0 (EAX = work): the scene's release.
void release_4b63e0(PcRaceContext& c,std::uint32_t w){
    auto& m=c.m;
    if(!m.u32(w))return;
    const std::uint32_t row=m.u32(w+0x5cu);
    if(!row)return;
    if(m.u32(row)==1u&&m.u32(w+0x118u))call(c,0x440cd0u,{w+0x118u});
    m.put32(w+0x60u,0);
    release_4b52e0(c,w);
    (void)call(c,0x4af520u,{0x3f800000u});
    m.put32(w,0);
}
// 4B6A70 (ESI = work): the scene starts (state 3, callbacks, times, nodes, end time).
void begin_4b6a70(PcRaceContext& c,std::uint32_t w){
    auto& m=c.m;
    m.put32(w,m.u32(w)|0x1008u);
    m.put32(w+0x1cu,3u);
    m.put32(w+0x24u,call(c,0x51b740u,{m.u32(w+0x54u),0u}));
    m.put32(w+0x28u,call(c,0x51b740u,{m.u32(w+0x54u),1u}));
    m.put32(w+0x2cu,call(c,0x51b740u,{m.u32(w+0x54u),2u}));
    m.put32(w+8u,0);m.put32(w+4u,0);m.put32(w+0xcu,0);m.put32(w+0x58u,0);
    m.put32(w+0x10u,0);m.put32(w+0x74u,0);m.putf(w+0x78u,1.0f);m.putf(w+0x7cu,1.0f);
    setup_4b66b0(c,w);
    const std::uint32_t t=m.u32(m.u32(w+0x60u)+4u);
    float end=float(m.i32(t+4u));end=end/m.f32(t+8u);
    m.putf(w+8u,end);
}
// 4B6B00 (ESI = work): the script file (row kind 1): 44FD80 request, then 44F880 ready ->
// 44FC60 data, header relocations (+4, +C, +10) and 4B6430. 1 while loading.
std::uint32_t load_4b6b00(PcRaceContext& c,std::uint32_t w){
    auto& m=c.m;
    const std::uint32_t row=m.u32(w+0x5cu);
    switch(m.u32(w+0x120u)){
    case 0:{
        const std::uint32_t path=m.u32(row+4u);
        if(!path){m.put32(w+0x120u,2u);return 0;}
        m.put32(w+0x11cu,call(c,0x44fd80u,{path,9u}));
        m.put32(w+0x120u,1u);
        return 1;}
    case 1:{
        if(!call(c,0x44f880u,{m.u32(w+0x11cu)}))return 1;
        {Locals l(m);call(c,0x44fc60u,{m.u32(w+0x11cu),w+0x60u,PcAutosceneLocals,w+0x118u});}
        if(m.u32(w+0x118u)){
            const std::uint32_t d=m.u32(w+0x60u);
            m.put32(d+0xcu,m.u32(d+0xcu)+d);m.put32(d+4u,m.u32(d+4u)+d);
            if(m.u32(d+0x10u))m.put32(d+0x10u,m.u32(d+0x10u)+d);
            relocate_4b6430(m,w);
        }
        m.put32(w+0x11cu,0);m.put32(w+0x120u,2u);
        return 0;}
    default:return 0;
    }
}
// 4B5670(work, node): the node tree's channels (nine values, +0 bit 0x100 from channel 9 >
// 0.5) and, when visible, its matrix into the +88 slot +34.
void node_frame_4b5670(PcRaceContext& c,std::uint32_t w,std::uint32_t node){
    auto& m=c.m;
    if(const std::uint32_t ch=m.u32(node+0x38u))
        for(std::uint32_t k2=0;k2<10u;++k2){
            if(!m.u32(ch+k2*8u))continue;
            const float v=driving::x87_float(sample(m,ch+k2*8u,m.f32(w+4u)));
            if(k2==9u){
                if(v>k(m,0x628064u))m.put32(node,m.u32(node)|0x100u);
                else m.put32(node,m.u32(node)&0xfffffeffu);
            }else m.putf(node+0x10u+k2*4u,v);
        }
    if(!(m.u32(node)&0x100u))return;
    driving::pc_matrix_push(c.matrices);
    translate_40a2d0(c,node+0x10u);rotate_40a4e0(c,node+0x1cu);
    if(m.u32(node+0x34u)!=0xffffffffu)store_40a0d0(c,m.u32(w+0x88u)+(m.u32(node+0x34u)<<6));
    for(std::uint32_t i=0;std::int32_t(i)<m.i32(node+0x3cu);++i)node_frame_4b5670(c,w,m.u32(node+0x40u)+i*0x44u);
    driving::pc_matrix_pop(c.matrices);
}
// 4B5B20 (EAX = node row, arg work): the robot record's pose list (+8 track +C items): matrix
// slots, channels (channel 9 hides it), translate / rotate; the result into record +20 and,
// visible, 487D10(robot work, record + 20).
void robot_frame_4b5b20(PcRaceContext& c,std::uint32_t row,std::uint32_t w){
    auto& m=c.m;
    const std::uint32_t q=m.u32(row+0x10u);
    std::uint32_t d=m.u32(m.u32(q+8u)+0xcu);
    m.put32(q,m.u32(q)|0x200u);
    {const std::uint32_t rw=m.u32(q+0x10u);m.put32(rw+4u,m.u32(rw+4u)|0x8000u);}
    driving::pc_matrix_push_unit(c.matrices);
    for(;d;d=m.u32(d+0x38u)){
        bool apply=true;
        if(const std::uint32_t slot=m.u32(d+0x2cu)){load_40a170(c,m.u32(w+0x88u)+(m.u32(slot+0x34u)<<6));apply=false;}
        if(const std::uint32_t ch=m.u32(d+0x30u)){
            for(std::uint32_t k2=0;k2<10u;++k2){
                if(!m.u32(ch+k2*8u))continue;
                const float v=driving::x87_float(sample(m,ch+k2*8u,m.f32(w+4u)));
                if(k2==9u){
                    if(k(m,0x628064u)>v){m.put32(q,m.u32(q)&0xfffffdffu);const std::uint32_t rw=m.u32(q+0x10u);m.put32(rw+4u,m.u32(rw+4u)&0xffff7fffu);}
                }else m.putf(d+4u+k2*4u,v);
            }
            apply=true;
        }
        if(apply){translate_40a2d0(c,d+4u);rotate_40a4e0(c,d+0x10u);}
    }
    store_40a0d0(c,q+0x20u);
    if(m.u32(q)&0x200u)call(c,0x487d10u,{m.u32(q+0x10u),q+0x20u});
    driving::pc_matrix_pop(c.matrices);
}
// 4B5AF0 (ECX = offset, ESI = name): the motion name's car letter ([6870CC + (mode-1) % 3])
// then "AL".
void motion_suffix_4b5af0(PcRaceMemory& m,std::uint32_t name,std::uint32_t at){
    const std::int32_t v=m.i32(0x7d3a7cu);
    if(!v)return;
    const std::int32_t idx=(v-1)%3;
    m.put8(name+at,m.u8(0x6870ccu+std::uint32_t(idx)));m.put8(name+at+1u,0x41);m.put8(name+at+2u,0x4c);
}
// 4B5C40(work, node row): the row's event list +20 up to the frame +10: 1 robot motion, 2 / 4
// sounds, 6 / 7 motion names (514830 / 514800), 10 hide the row.
void events_4b5c40(PcRaceContext& c,std::uint32_t w,std::uint32_t row){
    auto& m=c.m;
    if(!m.u32(row+0x1cu))return;
    std::uint32_t s=m.u32(row+0x20u);
    if(!s)return;
    const std::uint32_t q=m.u32(row+0x10u);
    while(m.u32(s)!=0xffffffffu&&!(m.i32(s)>m.i32(w+0x10u))){
        const std::uint32_t b=s+0xcu;
        switch(m.u32(s+4u)){
        case 1:{
            if(m.u32(row+8u)!=2u)break;
            const std::uint32_t slot=m.u32(m.u32(q+0x10u))*0xa4u+0x82f5f0u;
            const std::uint32_t before=call(c,0x48f4e0u,{},slot);
            call(c,0x4f2280u,{m.u32(b)},slot);
            call(c,0x4f1d30u,{fb(float(m.i32(b+4u)-1))},slot);
            call(c,0x4f2520u,{},slot);
            call(c,0x4f1d30u,{fb(float(m.i32(b+4u)-1))},slot);
            call(c,0x4f1e30u,{fb(float(m.i32(b+8u)+m.i32(b+4u)-1))},slot);
            m.put32(q+0x60u,m.u32(b));
            m.put32(q+0x68u,std::uint32_t(cvtt(m.f32(b+0xcu))));
            float f=1.0f;f=f/m.f32(b+0x10u);
            m.putf(q+0x64u,f);
            call(c,0x4ed890u,{fb(f)},slot);
            call(c,0x4f1e60u,{m.u32(q+0x68u)},slot);
            if(call(c,0x48f4e0u,{},slot)!=before)call(c,0x514f60u,{m.u32(q+0x10u),slot});
            if(key_step_4b5230(c,w))call(c,0x514f60u,{m.u32(q+0x10u),slot});
            break;}
        case 2:{
            const std::uint32_t v=m.u32(b);
            if((v&0xffffu)==0x4a9u)call(c,0x4249f0u,{m.u32(0x687030u+std::uint32_t(std::int32_t(v)>>16)*4u)});
            break;}
        case 4:call(c,0x4249f0u,{m.u32(0x687030u+std::uint32_t(std::int32_t(m.i16(b+2u)))*4u)});break;
        case 6:case 7:{
            Locals l(m);
            const std::uint32_t buf=PcAutosceneLocals+(m.u32(s+4u)==6u?0u:0x80u);
            const std::string name=cstring(m,m.u32(b));
            for(std::size_t i=0;i<name.size();++i)m.put8(buf+std::uint32_t(i),std::uint8_t(name[i]));
            m.put8(buf+std::uint32_t(name.size()),0);
            const std::uint32_t rw=m.u32(q+0x10u);
            if(m.u32(rw+8u)!=0x10u)motion_suffix_4b5af0(m,buf,5u);
            if(m.u32(rw+8u)==0xfu){
                std::uint32_t end=buf;while(m.u8(end))++end;
                m.put32(end,m.u32(0x5c6914u));m.put8(end+4u,m.u8(0x5c6918u));
            }
            const std::uint32_t token=call(c,0x448cd0u,{buf});
            call(c,m.u32(s+4u)==6u?0x514830u:0x514800u,{m.u32(q+0x10u),token,m.u32(b+4u)});
            break;}
        case 10:m.put32(row,m.u32(row)&0xfffffffeu);break;
        default:break;
        }
        scene_callback(m.u32(w+0x2cu));
        s+=m.u32(s+8u)*4u;
    }
    m.put32(row+0x20u,s);
}
// 4B68F0 (EDI = work): every active row: events, then node trees (4B5670), robots (4B5B20)
// and the kind-8 lists (420560 for the entries spanning the frame).
void frame_4b68f0(PcRaceContext& c,std::uint32_t w){
    auto& m=c.m;
    for(std::uint32_t i=0;std::int32_t(i)<m.i32(w+0x68u);++i){
        const std::uint32_t row=m.u32(w+0x70u)+i*0x24u;
        const std::uint32_t f=m.u32(row);
        if(!(f&1u))continue;
        if((m.u32(w)&0x40u)&&!(f&0x80u))continue;
        events_4b5c40(c,w,row);
        switch(m.u32(row+8u)){
        case 1:node_frame_4b5670(c,w,m.u32(row+0x18u));break;
        case 2:robot_frame_4b5b20(c,row,w);break;
        case 8:{
            std::uint32_t s=m.u32(row+0x1cu);
            for(;m.u32(s)!=0xffffffffu;s+=m.u32(s+8u)*4u){
                const std::int32_t t=m.i32(w+0x10u),at=m.i32(s);
                if(at<t&&m.i32(s+0x10u)+at>t&&m.u32(s+0xcu)==0u)call(c,0x420560u,{});
            }
            break;}
        default:break;
        }
    }
}
// 4B6BD0 (EAX = work): the scene clock (+C accumulated, +10 frame, +4 = +C / 60, flag 4 at
// the end time +8), fade track 4AF520, camera, rows, end states 5 / 4.
void clock_4b6bd0(PcRaceContext& c,std::uint32_t w){
    auto& m=c.m;
    float speed=1.0f;
    if(const std::uint32_t t=m.u32(w+0x74u)){
        std::uint32_t ch=m.u32(t+0x18u);
        if(m.u32(ch))m.putf(w+0x7cu,driving::x87_float(sample(m,ch,m.f32(w+4u))));
        m.put32(w+0x78u,m.u32(w+0x7cu));
        ch+=8u;
        if(m.u32(ch))m.putf(w+0x78u,driving::x87_float(sample(m,ch,m.f32(w+4u))));
        (void)call(c,0x4af520u,{m.u32(w+0x78u)});
        speed=m.f32(w+0x7cu);
    }
    std::uint32_t flags=m.u32(w);
    if(flags&0x40u)speed=0.0f;
    float acc=speed+m.f32(w+0xcu);
    m.put32(w+0x10u,std::uint32_t(cvtt(acc)));
    m.putf(w+0xcu,acc);
    const float now=acc*k(m,0x6280e8u);
    m.putf(w+4u,now);
    if(now>m.f32(w+8u)){flags|=4u;m.put32(w,flags);}
    if(now>m.f32(w+8u))m.put32(w+4u,m.u32(w+8u));
    camera_4b5180(c,w);
    camera_set_483e10(c,w);
    frame_4b68f0(c,w);
    if(m.u32(w)&0x40u)return;
    scene_callback(m.u32(w+0x24u));
    if(m.u32(w)&2u)m.put32(w,m.u32(w)&0xfffffffdu);
    if(m.u32(w)&4u){
        m.put32(w+0x1cu,5u);
        if(m.u32(w)&0x800u)m.put32(w+0x1cu,4u);
    }
}
// 4B5750(work, node) (display): the node tree's draws: car parts (car +330 light bits, its
// +B0 matrix, wheel / door words), the two robots' eyes (514880), smoke (4208A0), models
// (405360).
void draw_4b5750(PcRaceContext& c,std::uint32_t w,std::uint32_t node);
void draw_children(PcRaceContext& c,std::uint32_t w,std::uint32_t node){
    for(std::uint32_t i=0;std::int32_t(i)<c.m.i32(node+0x3cu);++i)draw_4b5750(c,w,c.m.u32(node+0x40u)+i*0x44u);
}
void draw_4b5750(PcRaceContext& c,std::uint32_t w,std::uint32_t node){
    auto& m=c.m;
    driving::pc_matrix_push(c.matrices);
    if(m.u32(node+8u)!=0xffffffffu&&(m.u32(node)&0x100u)){
        // VM 4B576D (measured: mov edi,5C6908): the name's first 10 bytes against
        // "HAKO_HUTA " (repz cmpsb); the box lid moves by (-0.045, 0.056, 0) (40A290).
        const std::uint32_t name=m.u32(node+0xcu);
        bool same=true;for(std::uint32_t i=0;i<10u&&same;++i)same=m.u8(name+i)==m.u8(0x5c6908u+i);
        if(same)driving::pc_matrix_translate_vector(c.matrices,{bf(0xbd3851ecu),bf(0x3d65c91du),0.0f});
    }
    translate_40a2d0(c,node+0x10u);rotate_40a4e0(c,node+0x1cu);
    const std::uint32_t t=m.u32(node+8u);
    if(t>=0xbd0000u&&t<=0xbd001bu){
        const std::uint32_t car=m.u32(0x799d18u);
        const std::uint32_t mask=t&0xffffu;
        std::uint32_t lights=m.u32(car+0x330u);
        if(m.u32(node)&0x100u)lights|=mask;else lights&=~mask;
        m.put32(car+0x330u,lights);
        auto word=[&](std::uint32_t at,std::uint32_t src){float f=m.f32(src)*k(m,0x6282c0u);m.put16(car+at,std::uint16_t(cvtt(f)));};
        switch(t-0xbd0000u){
        case 0:store_40a0d0(c,car+0xb0u);break;
        case 1:word(0x164u,node+0x20u);break;
        case 2:word(0x166u,node+0x20u);break;
        case 7:word(0x32u,node+0x20u);break;
        case 8:word(0x40u,node+0x1cu);break;
        case 0xc:word(0x42u,node+0x1cu);break;
        case 0x10:word(0x44u,node+0x1cu);break;
        case 0x14:word(0x46u,node+0x1cu);break;
        default:break;
        }
        draw_children(c,w,node);
    }else if(t>=0xbd001cu&&t<=0xbd001fu){
        std::uint32_t a=m.u32(0x79f010u),b=m.u32(0x79f04cu);
        if(m.u32(a+8u)!=0x10u)std::swap(a,b);
        switch(t-0xbd001cu){
        case 0:call(c,0x514880u,{a,node+0x1cu,0u});break;
        case 1:call(c,0x514880u,{a,node+0x1cu,1u});break;
        case 2:call(c,0x514880u,{b,node+0x1cu,0u});break;
        default:call(c,0x514880u,{b,node+0x1cu,1u});break;
        }
    }else if(t==0xbd0020u){
        if(m.u32(node)&0x100u)call(c,0x4208a0u,{0x60u,node+0x10u,0x80ecdfcbu});
    }else if(m.u32(node)&0x100u){
        if(t!=0xffffffffu){                                             // 405360 model leaf on the current matrix
            if(!c.draws)throw PcAutosceneUnported{0x405360u};
            PcVehicleDrawCall d{};d.pc=0x405360u;d.argc=6;d.args={t,0u,0u,0u,0xffffffffu,0u};
            const auto cur=c.matrices.current();for(unsigned b=0;b<64;++b)d.matrix[b]=cur.u8(b);
            c.draws->push_back(d);
        }
        draw_children(c,w,node);
    }
    driving::pc_matrix_pop(c.matrices);
}
// 4B69C0 (ESI = work): the rows of this pass (+20: 0 = rows without flag 0x400): node trees
// (4B5750), kind 8 marks [84281C] = row index.
void display_rows_4b69c0(PcRaceContext& c,std::uint32_t w){
    auto& m=c.m;
    for(std::uint32_t i=0;std::int32_t(i)<m.i32(w+0x68u);++i){
        const std::uint32_t row=m.u32(w+0x70u)+i*0x24u;
        const std::uint32_t f=m.u32(row);
        if(!(f&1u))continue;
        const bool pass=m.u32(w+0x20u)!=0u;
        if(pass?!(f&0x400u):(f&0x400u)!=0u)continue;
        switch(m.u32(row+8u)){
        case 1:draw_4b5750(c,w,m.u32(row+0x18u));break;
        case 8:m.put32(0x84281cu,i);break;
        default:break;
        }
    }
}
}

void autoscene_init_4b5150(PcRaceContext& c){
    const std::uint32_t w=call(c,0x440a60u,{0x124u,0u});
    for(std::uint32_t a=0;a<0x124u;a+=4)c.m.put32(w+a,0);
    c.m.put32(w,0);c.m.put32(w+0x1cu,0);
}
void autoscene_start_4b5f60(PcRaceContext& c,std::uint32_t scene){
    auto& m=c.m;
    if(!scene)return;
    const std::uint32_t w=m.u32(Work);
    if(!w||m.u32(w))return;
    m.putf(w+8u,0.0f);m.putf(w+4u,0.0f);m.putf(w+0xcu,0.0f);m.putf(w+0x58u,0.0f);
    m.put32(w,3u);m.put32(w+0x54u,scene);m.put32(w+0x1cu,1u);m.put32(w+0x14u,0);m.put32(w+0x10u,0);m.put32(w+0x74u,0);
    m.putf(w+0x78u,1.0f);m.putf(w+0x7cu,1.0f);
}
std::uint32_t autoscene_frame_4b5fc0(PcRaceContext& c){return c.m.u32(c.m.u32(Work)+0x10u);}
std::uint32_t autoscene_state_4b5fd0(PcRaceContext& c){
    if((c.m.u8(0x79fb4eu)&3u)!=2u)return 0;
    return c.m.u32(c.m.u32(Work)+0x1cu);
}
void autoscene_control_4b6cd0(PcRaceContext& c,std::uint32_t w){
    auto& m=c.m;
    m.put32(w+0x14u,m.u32(w+0x14u)+1u);
    if(m.u32(w+0x1cu)==1u){
        const std::uint32_t row=m.u32(w+0x54u)*12u+0x717d68u;
        m.put32(w+0x5cu,row);m.put32(w+0x60u,m.u32(row+4u));
        if(m.u32(row)==1u){m.put32(w+0x1cu,2u);m.put32(w+0x118u,0);m.put32(w+0x11cu,0);m.put32(w+0x120u,0);}
    }
    switch(m.u32(w+0x1cu)){
    case 1:begin_4b6a70(c,w);if(m.u32(w+0x1cu)==3u)clock_4b6bd0(c,w);break;
    case 2:if(!load_4b6b00(c,w)){begin_4b6a70(c,w);clock_4b6bd0(c,w);m.put32(w+0x1cu,3u);}break;
    case 3:clock_4b6bd0(c,w);break;
    case 5:release_4b63e0(c,w);m.put32(w+0x1cu,0);break;
    default:break;
    }
    const std::uint32_t f=m.u32(w);
    if(!(f&1u))return;
    if(!(f&8u)){m.putf(w+0x58u,0.0f);return;}
    if(f&0x1000u){
        const float end=m.f32(w+8u)*k(m,0x628134u);
        const float frame=float(m.i32(w+0x10u));
        if(frame>end-k(m,0x6282d4u)){float v=end-frame;v=v*k(m,0x628128u);m.putf(w+0x58u,v);return;}
        if(1.0f>m.f32(w+0x58u)){
            const float v=m.f32(w+0x58u)+k(m,0x628128u);
            m.putf(w+0x58u,1.0f>v?v:1.0f);
        }
        return;
    }
    if(m.f32(w+0x58u)>0.0f){
        const float v=m.f32(w+0x58u)-k(m,0x628128u);
        m.putf(w+0x58u,v>0.0f?v:0.0f);
    }
}
void autoscene_display_4b6a30(PcRaceContext& c,std::uint32_t w){
    auto& m=c.m;
    if(!(m.u32(w)&1u))return;
    const std::uint32_t cb=m.u32(w+0x28u);
    driving::pc_matrix_push_unit(c.matrices);
    m.put32(w+0x20u,0);
    display_rows_4b69c0(c,w);
    scene_callback(cb);
    driving::pc_matrix_pop(c.matrices);
}
void autoscene_dest_4b6690(PcRaceContext& c,std::uint32_t w){
    call(c,0x44fd00u,{});call(c,0x42ebf0u,{});call(c,0x4489f0u,{});
    release_4b63e0(c,w);
    call(c,0x440b20u,{});                                                // the event's work
}
}
