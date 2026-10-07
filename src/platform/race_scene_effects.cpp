#include "platform/race_scene_effects.hpp"
#include "driving/pc_d3dx.hpp"
#include "driving/pc_x87.hpp"
#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <stdexcept>
namespace outrun::platform {
// 5C4EA0 (EXE .rdata): target and value of every record, in table order.
const std::array<PcSceneEffectsParameter,25> SceneEffectsParameters5c4ea0{{
    {0x842170u,0x00000000u},{0x842160u,0x3e4ccccdu},{0x84216cu,0xc0800000u},{0x842168u,0x41400000u},
    {0x842150u,0x41a00000u},{0x842178u,0x44160000u},{0x842130u,0x00000000u},{0x842194u,0x3ba3d70au},
    {0x84215cu,0x3f800000u},{0x842144u,0x3dcccccdu},{0x842148u,0x3f800000u},{0x84218cu,0xc0000000u},
    {0x842188u,0x3f19999au},{0x84217cu,0xc0c00000u},{0x842180u,0x42200000u},{0x842154u,0x42700000u},
    {0x842140u,0x42f00000u},{0x84214cu,0x00000000u},{0x842134u,0x3ccccccdu},{0x842158u,0x3f800000u},
    {0x842174u,0x3dcccccdu},{0x84213cu,0x3f800000u},{0x842184u,0x40000000u},{0x842164u,0x3fe66666u},
    {0x842138u,0xc1a00000u}}};
namespace {
using driving::Bytes;
using driving::CourseProbe;
using X=driving::X87;
float st(X v){return driving::x87_float(v);}
float fbits(std::uint32_t u){float f;std::memcpy(&f,&u,4);return f;}
constexpr float DegToRad=0.0174532924f;     // 6281C4
constexpr float Ten=10.0f;                  // 628244
constexpr float One=1.0f;                   // 62806C
constexpr float Step=0.00830564741f;        // 5C518C (0x3C081469)
constexpr float Pi=3.14159274f;             // 6280C8
constexpr float Eps=1.1920929e-07f;         // 6281F0
struct Port {
    PcRaceContext& c;PcRaceMemory& m;
    explicit Port(PcRaceContext& cc):c(cc),m(cc.m){}
    void push(){driving::pc_matrix_push(c.matrices);}
    void push_unit(){driving::pc_matrix_push_unit(c.matrices);}
    void pop(){driving::pc_matrix_pop(c.matrices);}
    void identity(){driving::pc_matrix_identity(c.matrices);}
    void get(std::uint32_t a){driving::pc_matrix_get(c.matrices,m.bytes(a,64));}
    void load(std::uint32_t a){driving::pc_matrix_load(c.matrices,m.bytes(a,64));}
    void multiply(Bytes left){driving::pc_matrix_multiply_current(c.matrices,left);}   // 40A220
    CourseProbe vec(std::uint32_t a){return {m.f32(a),m.f32(a+4),m.f32(a+8)};}
};
// 449360 / 449370: fld [esp+4]; fsin / fcos.
X sin_449360(float a){return driving::x87_sin(X(a));}
X cos_449370(float a){return driving::x87_cos(X(a));}
// 40EF40 / 40EFA0: out = a + b / a - b, one x87 operation per component.
CourseProbe add_40ef40(const CourseProbe& a,const CourseProbe& b){return {st(X(a.x)+X(b.x)),st(X(a.y)+X(b.y)),st(X(a.z)+X(b.z))};}
CourseProbe sub_40efa0(const CourseProbe& a,const CourseProbe& b){return {st(X(a.x)-X(b.x)),st(X(a.y)-X(b.y)),st(X(a.z)-X(b.z))};}
// fcomp result helpers: C0 is also set for unordered operands.
bool c0(float a,float b){return !(a>=b);}               // a < b or unordered
bool c0_or_c3(float a,float b){return !(a>b);}          // a <= b or unordered
}
std::uint32_t scene_light_address_4082b0(std::uint32_t env,std::uint32_t type,std::uint32_t id){
    if(type==0u)return 0x899b98u+(env+id)*5u*0x20u;              // 4082B0 type 0 (plain on Steam): (env + id) * 0xA0
    const std::uint32_t index=(id+env*2u)*5u*0x20u;
    return (type==1u?0x89a138u:0x899d78u)+index;
}
namespace {
struct Light {
    PcRaceMemory& m;std::uint32_t a;
    Light(PcRaceMemory& mm,std::uint32_t env,std::uint32_t type,std::uint32_t id):m(mm),a(scene_light_address_4082b0(env,type,id)){}
    void enable_407c30(std::uint32_t v){m.put32(a,v);}
    void type_407c90(std::uint32_t v){m.put32(a+4,v);}
    void diffuse_407cf0(std::uint32_t r,std::uint32_t g,std::uint32_t b){m.put32(a+8,r);m.put32(a+0xc,g);m.put32(a+0x10,b);m.put32(a+0x14,0x3f800000u);}
    void intensity_407d60(std::uint32_t v){m.put32(a+0x90,v);}
    void specular_407dd0(std::uint32_t r,std::uint32_t g,std::uint32_t b){m.put32(a+0x18,r);m.put32(a+0x1c,g);m.put32(a+0x20,b);m.put32(a+0x24,0);}
    void ambient_407e40(std::uint32_t r,std::uint32_t g,std::uint32_t b){m.put32(a+0x28,r);m.put32(a+0x2c,g);m.put32(a+0x30,b);m.put32(a+0x34,0);}
    void position_407eb0(std::uint32_t x,std::uint32_t y,std::uint32_t z){m.put32(a+0x38,x);m.put32(a+0x3c,y);m.put32(a+0x40,z);}
    void direction_407fc0(const CourseProbe& d){m.putf(a+0x44,d.x);m.putf(a+0x48,d.y);m.putf(a+0x4c,d.z);}
    void range_4080a0(std::uint32_t v){m.put32(a+0x50,v);}
    void falloff_408100(float v){if(!c0(v,0.f))m.putf(a+0x54,v);}
    void attenuation_408130(float a0,float a1,float a2){
        m.putf(a+0x58,a0);m.putf(a+0x60,a2);m.putf(a+0x5c,a1);
        if(std::fabs(a0)<Eps)m.putf(a+0x58,Eps);
        if(std::fabs(a1)<Eps)m.putf(a+0x5c,Eps);
        if(std::fabs(a2)<Eps)m.putf(a+0x60,Eps);
    }
    void cone_4081f0(float theta,float phi){
        if(!c0(m.f32(a+0x68),0.f)){
            if(!c0_or_c3(Pi,phi))m.putf(a+0x68,phi);else m.put32(a+0x68,0x40490fdbu);
        }
        if(!c0(m.f32(a+0x64),0.f)){
            const float p=m.f32(a+0x68);
            if(!c0_or_c3(p,theta))m.putf(a+0x64,theta);
            else m.putf(a+0x64,st(X(p)-X(One)));
        }
    }
};
}
void scene_effects_headlight_init_4af5b0(PcRaceContext& c,std::uint32_t o){
    Port p(c);auto& m=c.m;
    m.put32(o+0x80,0x30009u);
    for(const auto& r:SceneEffectsParameters5c4ea0)m.put32(r.target,r.value);   // [5C4EA0] != 0
    m.put32(0x842190u,999u);
    p.push();
    driving::pc_d3dx_perspective_fov_rh(c.matrices.current(),fbits(0x3f060a92u),2.0f,fbits(0x3dcccccdu),5000.0f); // 40A950
    p.get(o);
    p.identity();
    p.get(o+0x40);
    p.pop();
}
void scene_effects_init_4afbb0(PcRaceContext& c,std::uint32_t w){
    Port p(c);auto& m=c.m;
    m.putf(w+0x10,500.0f);m.putf(w+0x14,500.0f);m.putf(w+0x18,500.0f);
    m.putf(w+0x28,0.100000001f);
    m.putf(w+0x1c,0.f);m.putf(w+0x20,0.f);m.putf(w+0x24,0.f);
    m.putf(w+0x2c,-0.100000001f);
    m.putf(w+0x30,0.f);
    p.push_unit();p.get(w+0x40);p.pop();
    m.put16(w+0xa0,0);
    m.put32(w+0x90,1);m.put32(w+0x94,1);m.put32(w+0x98,1);
    m.putf(w+0x9c,One);
    m.put16(w+0xa4,0xffffu);
    p.push_unit();p.get(w+0xb0);p.pop();
    scene_effects_headlight_init_4af5b0(c,w+0x130);
}
void scene_effects_transition_4afcc0(PcRaceMemory& m,std::uint32_t o){
    const std::uint32_t car=m.u32(0x799d18u);
    if(m.u32(car+0x5c)==0u)return;
    const std::int16_t cx=m.i16(o+0x14);
    if(cx<0)return;
    const std::uint32_t edx=m.u32(o+4),esi=m.u32(o+8);
    if(esi==edx){m.put16(o+0x14,0xffffu);return;}
    if(esi==1u&&edx==0u){
        if(cx==0x78)m.putf(o+0xc,One);
        if(cx>0){const float f=m.f32(o+0xc)-Step;m.put16(o+0x14,std::uint16_t(m.u16(o+0x14)-1u));m.putf(o+0xc,f);return;}
        m.putf(o+0xc,0.f);m.put32(o+8,0);
        const std::uint32_t w=m.u32(0x79f5ecu);m.put32(w,m.u32(w)&0xfffffffdu);
        m.put16(o+0x14,std::uint16_t(m.u16(o+0x14)-1u));
        return;
    }
    if(cx==0x78)m.putf(o+0xc,0.f);
    if(cx>0){const float f=m.f32(o+0xc)+Step;m.put16(o+0x14,std::uint16_t(m.u16(o+0x14)-1u));m.putf(o+0xc,f);return;}
    m.put16(o+0x14,std::uint16_t(m.u16(o+0x14)-1u));
    m.put32(o+8,edx);
    m.putf(o+0xc,One);
}
void scene_effects_headlights_4af630(PcRaceContext& c,std::uint32_t o){
    Port p(c);auto& m=c.m;
    const std::uint32_t car=m.u32(0x799d18u);
    const float p160=m.f32(0x842160u),p170=m.f32(0x842170u),p188=m.f32(0x842188u),p18c=m.f32(0x84218cu);
    const float p164=m.f32(0x842164u),p184=m.f32(0x842184u);
    // Three (0, y, z) points and their 10-unit offsets from the degree angles.
    const float a1=m.f32(0x84216cu)*DegToRad;
    const float v1y=st(sin_449360(a1)*X(Ten)+X(p160));
    const float v1z=st(X(p170)-cos_449370(m.f32(0x84216cu)*DegToRad)*X(Ten));
    const float a2=m.f32(0x84217cu)*DegToRad;
    const float v2y=st(sin_449360(a2)*X(Ten)+X(p188));
    const float v2z=st(X(p18c)-cos_449370(m.f32(0x84217cu)*DegToRad)*X(Ten));
    const float a3=m.f32(0x842138u)*DegToRad;
    const float v3y=st(sin_449360(a3)*X(Ten)+X(p164));
    const float v3z=st(X(p184)-cos_449370(m.f32(0x842138u)*DegToRad)*X(Ten));
    (void)a1;(void)a2;(void)a3;
    CourseProbe p1{0.f,p160,p170},p2{0.f,p188,p18c},p3{0.f,p164,p184};
    const CourseProbe v1{0.f,v1y,v1z},v2{0.f,v2y,v2z},v3{0.f,v3y,v3z};
    const CourseProbe up{0.f,1.f,0.f};
    p.push();p.identity();p.push_unit();
    p.load(car+0xb0);
    CourseProbe a=add_40ef40(p1,v1),b=add_40ef40(p2,v2),cc=add_40ef40(p3,v3);
    p1=driving::pc_matrix_point(c.matrices,p1);
    p2=driving::pc_matrix_point(c.matrices,p2);
    p3=driving::pc_matrix_point(c.matrices,p3);
    a=driving::pc_matrix_point(c.matrices,a);
    b=driving::pc_matrix_point(c.matrices,b);
    cc=driving::pc_matrix_point(c.matrices,cc);
    p.pop();
    const CourseProbe d=driving::pc_normalize_vector_40ef00(sub_40efa0(a,p1));
    const CourseProbe e=driving::pc_normalize_vector_40ef00(sub_40efa0(b,p2));
    auto u=[&](std::uint32_t addr){return m.u32(addr);};
    auto bitsf=[](float f){std::uint32_t v;std::memcpy(&v,&f,4);return v;};
    for(std::uint32_t env=1;env<=2;++env){
        Light l0(m,env,1,0);
        l0.position_407eb0(bitsf(p1.x),bitsf(p1.y),bitsf(p1.z));
        l0.direction_407fc0(d);
        l0.diffuse_407cf0(u(0x84215cu),u(0x84215cu),u(0x84215cu));
        l0.ambient_407e40(u(0x842144u),u(0x842144u),u(0x842144u));
        l0.specular_407dd0(u(0x842148u),u(0x842148u),u(0x842148u));
        l0.type_407c90(2);
        l0.attenuation_408130(m.f32(0x842130u),m.f32(0x842194u),0.f);
        l0.cone_4081f0(m.f32(0x842168u)*DegToRad,m.f32(0x842150u)*DegToRad);
        l0.range_4080a0(u(0x842178u));
        l0.enable_407c30(1);
        l0.falloff_408100(1.f);
        l0.intensity_407d60(0x3f800000u);
        Light l1(m,env,1,1);
        l1.position_407eb0(bitsf(p2.x),bitsf(p2.y),bitsf(p2.z));
        l1.direction_407fc0(e);
        l1.diffuse_407cf0(u(0x842158u),u(0x842158u),u(0x842158u));
        l1.ambient_407e40(u(0x842174u),u(0x842174u),u(0x842174u));
        l1.specular_407dd0(u(0x84213cu),u(0x84213cu),u(0x84213cu));
        l1.type_407c90(2);
        l1.attenuation_408130(m.f32(0x84214cu),m.f32(0x842134u),0.f);
        l1.cone_4081f0(m.f32(0x842180u)*DegToRad,m.f32(0x842154u)*DegToRad);
        l1.range_4080a0(u(0x842140u));
        l1.enable_407c30(1);
        l1.falloff_408100(1.f);
        l1.intensity_407d60(0x3f800000u);
    }
    driving::pc_d3dx_look_at_rh(c.matrices.current(),{p3.x,p3.y,p3.z},{cc.x,cc.y,cc.z},{up.x,up.y,up.z}); // 40A900
    std::array<float,16> view{};
    driving::pc_matrix_get(c.matrices,Bytes(view.data(),64));
    p.identity();
    p.multiply(m.bytes(o,64));
    p.multiply(Bytes(view.data(),64));
    p.get(o+0x40);
    p.pop();
}
void scene_effects_control_4afdc0(PcRaceContext& c,std::uint32_t w){
    Port p(c);auto& m=c.m;
    if(m.u8(w)&1u){
        const std::uint32_t s=w+0x10;
        const auto o=p.vec(s+0xc),d=p.vec(s+0x18);                     // 40EF10(s+C, s+18)
        m.putf(s+0xc,st(X(o.x)+X(d.x)));
        m.putf(s+0x10,st(X(d.y)+X(m.f32(s+0x10))));
        m.putf(s+0x14,st(X(d.z)+X(m.f32(s+0x14))));
        p.push_unit();
        driving::pc_matrix_rotate_x(c.matrices,fbits(0x3fc90fdbu));
        driving::pc_matrix_translate_vector(c.matrices,p.vec(s+0xc));
        {   const auto v=p.vec(s);                                      // 40A3A0: scaling * current
            std::array<float,16> sc{};driving::pc_d3dx_matrix_scaling(Bytes(sc.data(),64),v.x,v.y,v.z);
            p.multiply(Bytes(sc.data(),64));}
        p.get(s+0x30);
        p.pop();
    }
    if(m.u8(w)&2u)scene_effects_transition_4afcc0(m,w+0x90);
    if(m.u8(w)&8u)scene_effects_headlights_4af630(c,w+0x130);
}
void scene_effects_destroy_4afc70(PcRaceMemory& m,std::uint32_t w){
    for(std::uint32_t bit:{1u,2u,4u,8u,0x10u}){const auto v=m.u32(w);if(v&bit)m.put32(w,v&~bit);}
}
namespace {
std::uint32_t work(const PcRaceMemory& m){return m.u32(0x79f5ecu);}
void orw(PcRaceMemory& m,std::uint32_t v){const auto w=work(m);m.put32(w,m.u32(w)|v);}
void andw(PcRaceMemory& m,std::uint32_t v){const auto w=work(m);m.put32(w,m.u32(w)&v);}
}
void scene_effects_4af550(PcRaceMemory& m){m.put32(work(m)+0x80,0x7fffffffu);}
void scene_effects_4af560(PcRaceMemory& m){orw(m,1);}
void scene_effects_4af570(PcRaceMemory& m){andw(m,0xfffffffeu);}
void scene_effects_4af580(PcRaceMemory& m){orw(m,4);}
void scene_effects_4af590(PcRaceMemory& m){andw(m,0xfffffffbu);}
void scene_effects_4afb40(PcRaceMemory& m){orw(m,8);}
void scene_effects_4afb50(PcRaceMemory& m){andw(m,0xfffffff7u);}
std::uint32_t scene_effects_4afb60(const PcRaceMemory& m){return (m.u8(work(m))>>3)&1u;}
void scene_effects_4afb70(PcRaceMemory& m){orw(m,0x10);}
void scene_effects_4afb80(PcRaceMemory& m){andw(m,0xffffffefu);}
std::uint32_t scene_effects_4af5a0(const PcRaceMemory& m){return (m.u8(work(m))>>2)&1u;}
std::uint32_t scene_effects_4afb90(const PcRaceMemory& m){return (m.u8(work(m))>>4)&1u;}
void scene_effects_4afba0(PcRaceMemory& m){andw(m,0xfffffffdu);}
void scene_effects_4afd90(PcRaceMemory& m,std::uint32_t kind){
    const std::uint32_t w=work(m);const std::uint32_t on=kind==2u?1u:0u;
    m.put16(w+0xa4,0x78);
    m.put32(w+0x94,on);
    if(on){const std::uint32_t w2=work(m);m.put32(w2,m.u32(w2)|2u);}
}
bool scene_effects_setter(PcRaceMemory& m,const PcRaceCall& k,std::uint32_t& eax){
    switch(k.pc){
    case 0x4af550u:scene_effects_4af550(m);eax=work(m);return true;
    case 0x4af560u:scene_effects_4af560(m);eax=work(m);return true;
    case 0x4af570u:scene_effects_4af570(m);eax=work(m);return true;
    case 0x4af580u:scene_effects_4af580(m);eax=work(m);return true;
    case 0x4af590u:scene_effects_4af590(m);eax=work(m);return true;
    case 0x4afb40u:scene_effects_4afb40(m);eax=work(m);return true;
    case 0x4afb50u:scene_effects_4afb50(m);eax=work(m);return true;
    case 0x4afb60u:eax=scene_effects_4afb60(m);return true;
    case 0x4afb70u:scene_effects_4afb70(m);eax=work(m);return true;
    case 0x4afb80u:scene_effects_4afb80(m);eax=work(m);return true;
    case 0x4afd90u:scene_effects_4afd90(m,k.args[0]);eax=m.u32(0x79f5ecu);return true;
    case 0x4af5a0u:eax=scene_effects_4af5a0(m);return true;
    case 0x4afb90u:eax=scene_effects_4afb90(m);return true;
    case 0x4afba0u:scene_effects_4afba0(m);eax=work(m);return true;
    default:return false;
    }
}
}
