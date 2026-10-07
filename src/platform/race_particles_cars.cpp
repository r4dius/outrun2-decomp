// PART_EFC: car tire parameters (MakePlcarParam 41BD50, MakeOccarParam
// 41C190) and the tire smoke requests (41C570, 41C940, 41CB30, 41CED0 and
// the tire-smoke particle functions 41D1E0/41D270/41D330/41D5C0).
// The tire records are 0x5C bytes: 934448 (player, 4), 9345B8 + car*0x170
// (cars 1..23, 4 each).
#include "platform/race_particles_port.hpp"
#include <cstring>
#include <limits>
namespace outrun::platform::particles_detail {
namespace {
using driving::CourseProbe;
// FSQRT of (a*a + b*b) + c*c with the x87 operand order given by the caller.
X hyp(X a,X b,X c){return driving::x87_sqrt((a*a+b*b)+c*c);}
CourseProbe vec(const P& p,std::uint32_t a){return {p.f(a),p.f(a+4),p.f(a+8)};}
void putv(const P& p,std::uint32_t a,const CourseProbe& v){p.putf(a,v.x);p.putf(a+4,v.y);p.putf(a+8,v.z);}
// 43939A D3DXVec3TransformCoord(out, in, [89B564]).
void transform_coord(P& p,std::uint32_t out,std::uint32_t in){putv(p,out,driving::pc_matrix_point(p.c.matrices,vec(p,in)));}
bool smoke_event(std::uint32_t kind){return kind==2u||kind==5u||kind==0xfu||kind==0xcu||kind==0x11u||kind==0x12u||kind==0x13u||kind==0xdu;}
}
// 41BD50 MakePlcarParam: the player's four tire records from the car work and
// its tire works (82EA38[4]).
void make_plcar_param_41bd50(P& p){
    const std::uint8_t cl=p.u8(0x79fb50u)&3u;
    const std::uint32_t mode=p.u(0x78026cu);
    const std::uint32_t car=p.u(0x799d18u);
    if(cl!=2u)return;
    p.push_load(car+0xb0);
    p.copy(0x915228u,car+0x14,3);
    p.put(0x915238u,p.u(0x915234u));
    p.put(0x915234u,p.u(car+0x34));
    p.put(0x91523cu,p.u(car+0x48));
    p.put8(0x915240u,p.u8(car+0x296));
    TireRecord* t=p.array<TireRecord>(0x934448u,4);
    const std::uint32_t lit_mask=p.u(0x82ee60u);
    for(unsigned n=0;n<4;++n){
        const std::uint32_t tire=p.u(0x82ea38u+n*4u);
        if(!tire)continue;
        auto& r=t[n];
        const auto w=p.m.bytes(tire,0xf0u);
        r.prev_state=r.state;
        std::memcpy(r.prev_pos,r.pos,12);
        const auto at=driving::pc_matrix_point(p.c.matrices,vec(p,car+0x130u+n*0xcu));   // 43939A
        r.pos[0]=at.x;r.pos[1]=at.y;r.pos[2]=at.z;
        std::memcpy(&r.prev_08,&r.pos[1],4);
        std::memcpy(&r.pos[1],w.data()+0x3c,4);                         // +08 <- tire +3C
        const X dx=X(r.prev_pos[0])-X(r.pos[0]),dy=X(r.prev_pos[1])-X(r.pos[1]),dz=X(r.prev_pos[2])-X(r.pos[2]);
        r.moved=st(hyp(dz,dy,dx));
        std::memcpy(r.normal,w.data()+0x70,12);
        r.surface=std::uint16_t(w.i16(0xee));
        std::memcpy(&r.slip,w.data()+0xe0,4);
        std::memcpy(&r.grip,w.data()+0xe4,4);
        std::memcpy(r.velocity,w.data()+0x40,12);
        r.lit=(1u<<(0x1fu-n))&lit_mask;
    }
    const std::int32_t d9c=p.i(car+0xd9c);
    if(d9c>0){                                                          // rear grip fades in after a spin
        const X v=X(t[2].grip)-X(d9c)*X(fb(0x3c881469u));
        t[2].grip=st(v);
        std::memcpy(&t[3].grip,&t[2].grip,4);
        const X w=X(1.0f)-X(t[2].grip);
        t[2].slip=st(w);t[3].slip=st(w);
    }
    for(unsigned n=0;n<4;++n)t[n].state=p.u(car+0x24cu+n*4u);
    if(mode==0x13u)
        for(unsigned n=0;n<4;++n){t[n].surface=0;t[n].slip=0.0f;t[n].grip=1.0f;t[n].state=2;t[n].lit=0;}
    // Four ground directions: (0,0,-1) rotated by the car yaw, dotted with
    // the diagonal table 623894 (.rdata).
    static constexpr std::uint32_t Diagonals[12]={0xbf3504f3u,0,0xbf3504f3u,0x3f3504f3u,0,0xbf3504f3u,0xbf3504f3u,0,0x3f3504f3u,0x3f3504f3u,0,0x3f3504f3u};
    for(unsigned k=0;k<4;++k){
        driving::pc_matrix_identity(p.c.matrices);
        const float angle=st(X(std::int32_t(p.i16(car+0x162)))*X(fb(0x38c90fdbu)));
        driving::pc_matrix_rotate_y(p.c.matrices,angle);
        const auto o=driving::pc_matrix_vector(p.c.matrices,{0.f,0.f,-1.f});
        const X r=(X(o.y)*X(fb(Diagonals[k*3+1]))+X(o.x)*X(fb(Diagonals[k*3])))+X(o.z)*X(fb(Diagonals[k*3+2]));
        t[k].ground=st(r);
    }
    p.pop();
    if(mode!=0x10u)return;
    const std::uint32_t any=p.u(0x93459cu)|p.u(0x934540u)|p.u(0x9344e4u)|p.u(0x934488u);
    if(any&0xf03f02u){p.put(0x98abccu,0);return;}
    if(p.u(0x98abccu))return;
    // The block then builds the arguments of a call to 49A650 (RET): car
    // +64/+5C/+68, 43D470(car+5C) ([780140+id*4] -> [780228+id*4]), 78024C
    // and 780258. Only its reads remain observable.
    const std::uint32_t id=p.u(car+0x5c);(void)p.i16(car+0x64);(void)p.u16(car+0x68);
    if(const std::uint32_t t=p.u(0x780140u+id*4u)){const std::uint32_t n=p.u(t+0xc)-1u;if(n!=0xffffffffu)(void)p.u16(p.u(0x780228u+id*4u)+n*2u);}
    (void)p.u16(0x78024cu);(void)p.u16(0x780258u);
    p.put(0x98abccu,1);
}
// 41C190 MakeOccarParam (cars 1..23): the four tire records of every running
// traffic car from its model's wheel points (4866C0 model, +3C/+4C/+5C/+6C).
void make_occar_param_41c190(P& p){
    const std::uint8_t* open=p.array<std::uint8_t>(0x79fb50u,0x18u);
    for(std::uint32_t id=1;id<0x18u;++id){
        if((open[id]&3u)!=2u)continue;
        const std::uint32_t car=p.u(0x799d18u+id*0x3cu);
        TireRecord* t=p.array<TireRecord>(0x9345b8u+(id-1u)*0x170u,4);
        // The records are updated in an interleaved order: +44 then +1C of each.
        for(unsigned n=0;n<4;++n){
            t[n].prev_state=t[n].state;
            std::memcpy(t[n].prev_pos,t[n].pos,12);
        }
        const auto c=p.m.bytes(car,0xd40u);
        const std::uint32_t model=p.u(p.u(p.u(0x650500u+std::uint32_t(std::int32_t(std::int8_t(c.u8(0x11))))*0x44u)));  // 4866C0
        p.push_load(car+0xb0);
        for(unsigned n=0;n<4;++n){                                     // 43939A D3DXVec3TransformCoord
            const auto w=driving::pc_matrix_point(p.c.matrices,vec(p,model+0x3cu+n*0x10u));
            t[n].pos[0]=w.x;t[n].pos[1]=w.y;t[n].pos[2]=w.z;
        }
        p.pop();
        const std::uint32_t kind=c.u32(0x324);
        bool drifting=false;
        if(kind==5u||kind==6u){
            const float v=st(X(std::int32_t(c.i16(0xd34)))*X(fb(0x3bb40000u)));
            drifting=jp5(driving::x87_abs(X(v)),X(10.0f));
        }
        for(unsigned n=0;n<4;++n){
            auto& r=t[n];
            std::memcpy(&r.prev_08,&r.pos[1],4);
            const X dx=X(r.prev_pos[0])-X(r.pos[0]),dy=X(r.prev_pos[1])-X(r.pos[1]),dz=X(r.prev_pos[2])-X(r.pos[2]);
            r.normal[0]=0.0f;r.normal[1]=1.0f;r.normal[2]=0.0f;
            r.surface=0;
            r.moved=st(hyp(dz,dx,dy));
            r.slip=drifting?1.0f:0.0f;r.grip=drifting?0.0f:1.0f;
            for(unsigned k=0;k<3;++k){const std::uint32_t v=c.u32(0xd0+k*4);std::memcpy(&r.velocity[k],&v,4);}
            r.lit=0;
        }
        for(unsigned n=0;n<4;++n)t[n].state=2;
    }
}
// 41D180 (ECX = tire record): smoke strength test against 74FF5C.
bool tire_smoke_test_41d180(P& p,std::uint32_t r){
    X v=p.x(r+0x38);
    if(jp5(v,X(0.0f)))v=v*X(fb(0x3f19999au));
    else v=driving::x87_sqrt(-v);
    const X t=p.x(r+0x3c);
    if(jp5(t,X(fb(0x3f7fbe77u))))v=X(0.0f);
    else v=v+(X(1.0f)-t)*X(fb(0x3f4ccccdu));
    return !c0c3(v,p.x(0x74ff5cu));
}
namespace {
// 41CB30/41CED0 colour table: 4 colours of the tire record (+58) scaled by
// the sun light 1 diffuse (899CE0) times s plus its ambient (899D44), each
// channel clamped to 1.
template<class F> void smoke_colours(P& p,std::uint32_t rec,F factor){
    float r=st(X(p.f(0x899ce0u))*factor()+X(p.f(0x899d44u)));
    float g=st(X(p.f(0x899ce4u))*factor()+X(p.f(0x899d48u)));
    float b=st(X(p.f(0x899ce8u))*factor()+X(p.f(0x899d4cu)));
    for(std::uint32_t esi=0;esi<0x10u;esi+=4){
        float cr=r,cg=g,cb=b;
        if(!c0c3(X(cr),X(1.0f)))cr=1.0f;
        if(!c0c3(X(cg),X(1.0f)))cg=1.0f;
        if(!c0c3(X(cb),X(1.0f)))cb=1.0f;
        const std::uint32_t colour=p.u(p.u(rec+0x58)+esi);
        p.put(0x74fed4u+esi,particles_colour_scale_41ff70(p.pc,colour,1.0f,cr,cg,cb));
    }
}
}
// 41CB30(EAX = tire record, a): the player's tire smoke.
void tire_smoke_41cb30(P& p,std::uint32_t edi,float a){
    const X s=(X(1.0f)-X(a))*X(0.5f)+X(a);
    const std::uint32_t car=p.u(0x799d18u);
    float l3c=fb(0x3c23d70au),l40=l3c,l44=l3c;
    const X q=p.x(edi)/p.x(0x74fea4u);
    std::uint32_t ebp;
    if(!c0c3(p.x(edi),p.x(0x74fea4u))){
        const float k=st(p.x(0x74fea8u)*q*X(fb(0x3c23d70au)));
        l3c=l40=l44=k;
        ebp=ftol(q);
    }else ebp=1;
    p.put(0x8a8f38u,0x7fff);
    const X dx=p.x(edi+0x1c)-p.x(edi+4),dy=p.x(edi+0x20)-p.x(edi+8),dz=p.x(edi+0x24)-p.x(edi+0xc);
    float l30=p.f(edi+0x1c),l34=p.f(edi+8),l38=p.f(edi+0x24);
    float l50=st(dz);
    const float inv=st(X(1.0f)/X(std::int32_t(ebp)));
    const float l48=st(dx*X(inv));
    const float l4c=st(X(inv)*dy);
    l50=st(X(l50)*X(inv));
    smoke_colours(p,edi,[&]{return s;});
    p.put(0x8a8f6cu,p.u(0x74fe88u));p.put(0x8a8f70u,p.u(0x74fe88u));p.put(0x8a8f84u,0x934448u);
    const std::uint32_t kind=p.u(edi+0x54);
    auto o=driving::pc_matrix_vector(p.c.matrices,vec(p,0x74feacu));
    const X k=p.x(0x74fea0u);
    float l24=st(k*X(o.x)),l28=st(k*X(o.y)),l2c=st(k*X(o.z));
    if(kind!=0u&&kind!=3u&&kind!=5u){
        l24=st(X(l24)+p.x(car+0x20));
        l28=st((p.x(0x74fe94u)+p.x(car+0x24))+X(l28));
        l2c=st(X(l2c)+p.x(car+0x28));
    }else{
        const X c90=p.x(0x74fe90u);
        l24=st(c90*p.x(car+0x20)+X(l24));
        l28=st((c90*p.x(car+0x24)+p.x(0x74fe94u))+X(l28));
        l2c=st(c90*p.x(car+0x28)+X(l2c));
    }
    p.putf(0x8a8f48u,l24);p.putf(0x8a8f4cu,l28);p.putf(0x8a8f50u,l2c);
    p.put(0x8a8f60u,p.u(0x74feb8u));p.put(0x8a8f64u,p.u(0x74febcu));p.put(0x8a8f68u,p.u(0x74fec0u));
    if(std::int32_t(ebp)<=0)return;
    for(std::uint32_t n=ebp;n;--n){
        p.putf(0x8a8f3cu,l30);p.putf(0x8a8f44u,l38);p.putf(0x8a8f40u,l34);
        p.putf(0x8a8f54u,l3c);p.putf(0x8a8f58u,l40);p.putf(0x8a8f5cu,l44);
        p.put(0x8a8f34u,0x41d1e0u);
        const std::uint32_t w=particles_get_work_418420(p.pc,4);
        particles_born_418350(p.pc,w);
        l30=st(X(l30)+X(l48));
        l34=st(X(l4c)+X(l34));
        l38=st(X(l38)+X(l50));
    }
}
// 41CED0(EAX = tire record, a): the rival cars' tire smoke.
void tire_smoke_41ced0(P& p,std::uint32_t edi,float a){
    const float s=st((X(1.0f)-X(a))*X(0.5f)+X(a));
    const X c=(p.u(0x82e7d8u)&&p.u(0x82e7c0u)!=1u)?X(0.5f):p.x(0x74fea4u);
    std::uint32_t ebx;
    if(jp5(c,p.x(edi)))ebx=1;
    else ebx=ftol(p.x(edi)/c);
    p.put(0x8a8f38u,0x7fff);
    const X dx=p.x(edi+0x1c)-p.x(edi+4),dy=p.x(edi+0x20)-p.x(edi+8),dz=p.x(edi+0x24)-p.x(edi+0xc);
    float l20=p.f(edi+0x1c),l24=p.f(edi+8),l28=p.f(edi+0x24);
    float l34=st(dz);
    const float inv=st(X(1.0f)/X(std::int32_t(ebx)));
    const float l2c=st(dx*X(inv));
    const float l30=st(dy*X(inv));
    l34=st(X(l34)*X(inv));
    smoke_colours(p,edi,[&]{return X(s);});
    for(std::uint32_t a_:{0x8a8f48u,0x8a8f4cu,0x8a8f50u,0x8a8f60u,0x8a8f64u,0x8a8f68u})p.put(a_,0);
    if(std::int32_t(ebx)<=0)return;
    for(std::uint32_t n=ebx;n;--n){
        p.putf(0x8a8f3cu,l20);p.putf(0x8a8f44u,l28);p.putf(0x8a8f40u,l24);
        p.put(0x8a8f54u,0);p.put(0x8a8f58u,0);p.put(0x8a8f5cu,0);
        p.put(0x8a8f34u,0x41d270u);
        const std::uint32_t w=particles_get_work_418420(p.pc,4);
        particles_born_418350(p.pc,w);
        l20=st(X(l20)+X(l2c));
        l24=st(X(l30)+X(l24));
        l28=st(X(l28)+X(l34));
    }
}
// 41C570 PcTireSmokeReq.
void pc_tire_smoke_req_41c570(P& p){
    const std::uint32_t car=p.u(0x799d18u);
    std::uint32_t xflag=smoke_event((p.u(car+0x2f0)>>2)&0x1fu)?1u:0u;
    const std::uint32_t y=p.u(car+0x58);
    const std::uint32_t frame=p.u(0x95af0cu);
    const std::uint32_t course=p.u(p.u(0x799b38u+8u*0x3cu)+0x68);          // 450380(8)
    std::uint32_t z;{const std::uint32_t r=race_area_record_44c8d0(p.m,course);z=r?p.u(p.u(r+0x14)):p.u(p.u(0x7d2df4u));}  // 44DC50
    std::uint32_t ebp=0,ebx,esi;
    p.push_load(car+0xb0);
    if(p.u(0x82e7d8u)){
        esi=0;
        if(p.u(0x82e7c8u)&1u)esi=3;
        if(p.u(0x82e7c8u)&2u)esi|=0xc;
        ebx=p.u(0x95afacu)|esi;esi=ebx;
        p.put(0x95afacu,ebx);p.put(0x95afa8u,esi);
        if(ebx)xflag=0;
    }else{ebx=p.u(0x95afacu);esi=p.u(0x95afa8u);}
    std::uint32_t edi=0;
    for(std::uint32_t edx=0x934448u;edx<0x9345b8u;edx+=0x5c,++edi){
        const auto hot=[&](std::uint32_t r){return !c0c3(p.x(r),X(fb(0x3e19999au)));};
        if(p.u(car+0x2f0)&2u){
            if(xflag){esi|=1u<<edi;ebx=esi;}
        }else{
            const std::uint32_t f=p.u(edx+0x40);
            if(f&0xd02u){
                if(tire_smoke_test_41d180(p,edx)){
                    ebp=0;
                    if(frame&1u)esi|=1u<<edi;
                    else esi|=1u<<((1u-(edi&1u))|(edi&2u));
                    ebx=esi;
                }else if(!p.u(car+0x5c)&&(z==9u||z==0xcu)){
                    ebp=3;
                    if((frame&1u)&&hot(edx)){esi|=1u<<edi;ebx=esi;}
                }
            }else if(f&0x800000u){
                ebp=tire_smoke_test_41d180(p,edx)?0u:5u;
                if((frame&1u)&&hot(edx)){esi|=1u<<edi;ebx=esi;}
            }else{
                bool test=false;
                if(f&0x2000000u){ebp=4;test=true;}
                else if(f&0x10u){ebp=1;test=true;}
                else if(f&0x8u){
                    ebp=2;
                    if(z!=0xfu)test=true;
                    else{const std::int16_t ax=p.i16(car+0x64);
                        if(ax<=0xe6||ax>=0x177)test=true;else test=(frame&1u)!=0u;}
                }
                if(test&&hot(edx)){esi|=1u<<edi;ebx=esi;}
            }
        }
        p.put(edx+0x54,ebp);p.put(edx+0x58,p.u(0x74ff44u+ebp*4u));
    }
    p.put(0x95afa8u,esi);p.put(0x95afacu,ebx);
    p.put(car+4,p.u(car+4)&0xf7ffffffu);
    edi=0;
    for(std::uint32_t r=0x934448u;r<0x9345b8u;r+=0x5c,++edi){
        if(!c0c3(p.x(r+0x50),X(fb(0x3f3504f3u)))){ebx&=~(1u<<edi);p.put(0x95afacu,ebx);}
        if(!(ebx&(1u<<edi)))continue;
        if(p.u(0x82e7d8u)&&p.u(0x82e7c0u))tire_smoke_41ced0(p,r,1.0f);
        else tire_smoke_41cb30(p,r,fb(y));
        const std::uint32_t ecx=p.u(car+4)|0x08000000u;
        const std::uint32_t k=std::int32_t(ebp)<0?0u:(std::int32_t(ebp)>5?5u:ebp);
        p.put(car+4,ecx);
        ebx=p.u(0x95afacu);
        p.put(car+4,(((k<<28)^ecx)&0x70000000u)^ecx);
    }
    p.pop();
    p.put(0x95afacu,0);
}
// 41C940 OcTireSmokeReq (cars 1..23, rear tires 2 and 3).
void oc_tire_smoke_req_41c940(P& p){
    std::uint32_t cars=0x799d54u,tires=0x9345b8u;
    for(std::uint32_t id=1;id<0x18u;++id,cars+=0x3c,tires+=0x170){
        if((p.u8(0x79fb50u+id)&3u)!=2u)continue;
        const std::uint32_t car=p.u(cars);
        const bool xflag=smoke_event((p.u(car+0x2f0)>>2)&0x1fu);
        p.push_load(car+0xb0);
        std::uint32_t edi=0;
        for(std::uint32_t esi=2;esi<4;++esi){
            const std::uint32_t f4=p.u(car+4);
            std::uint32_t ebp=0;
            bool set=false;
            if(f4&0x08000000u){ebp=(f4>>28)&7u;set=true;}
            else if(p.u8(car+0x2f0)&2u){
                if(xflag){
                    const float v=st(p.x(car+0x2ec)*X(fb(0x4622f983u)));
                    const std::int32_t t=std::isnan(v)||!(v>-2147483649.0f&&v<2147483648.0f)?std::numeric_limits<std::int32_t>::min():std::int32_t(v); // cvttss2si
                    const std::int16_t ax=std::int16_t(t);
                    if(esi==2){if(ax>std::int16_t(0xe000)&&ax<0x7fff){edi=p.u(0x95afa8u)|4u;p.put(0x95afa8u,edi);}}
                    else if(ax<=0x2000){edi=p.u(0x95afa8u)|(1u<<esi);p.put(0x95afa8u,edi);}
                }
            }else{
                const std::uint32_t r=tires+esi*0x5c;
                if((p.u(r+0x40)&0xd02u)&&tire_smoke_test_41d180(p,r)&&!c0c3(p.x(r),X(fb(0x3dcccccdu))))set=true;
            }
            if(set)edi|=1u<<esi;
            if(edi&(1u<<esi)){
                const std::uint32_t r=tires+esi*0x5c;
                p.put(r+0x54,ebp);p.put(r+0x58,p.u(0x74ff44u+ebp*4u));
                tire_smoke_41ced0(p,r,1.0f);
            }
        }
        p.pop();
    }
}
// Tire smoke particle functions (records: SmokeParticle).
namespace {
void advance(Particle& q){for(unsigned k=0;k<3;++k)q.position[k]=st(X(q.position[k])+X(q.velocity[k]));}
void kill(Particle& q){q.state=0;q.function=0;q.position[2]=0;q.position[1]=0;q.position[0]=0;}
void set_alpha(Billboard& q,std::uint32_t alpha24){for(auto& c:q.colour)c=(c&0xffffffu)|alpha24;}
void smoke_uv(Billboard& q){q.uv_max[1]=1.0f;q.uv_max[0]=1.0f;q.uv_min[1]=0;q.uv_min[0]=0;}
}
// 41D1E0 (player smoke born): size [74FE88], life 5 + rand * [74FE80],
// the colours 74FED4.. and their alpha byte.
void smoke_born_41d1e0(P& p,std::uint32_t at){
    auto& q=p.at<SmokeParticle>(at);
    const float size=p.f(0x74fe88u);
    q.state=std::int32_t(0x80000000u);q.function=0x41d330u;q.size=size;
    const std::uint32_t r=p.rand();
    const std::uint32_t n=ftol(X(std::int32_t(r))*X(fb(0x38000100u))*p.x(0x74fe80u))+5u;
    q.life=n;q.life_start=n;
    q.alpha=p.u8(0x74fed7u);
    for(unsigned k=0;k<4;++k)q.colour[k]=p.u(0x74fed4u+k*4);
    smoke_uv(q);
}
// 41D270 (rival smoke born): a random size in [1, 5] * [74FEC8], alpha
// falling from [74FED0] as the size grows to [74FECC].
void smoke_born_41d270(P& p,std::uint32_t at){
    auto& q=p.at<SmokeParticle>(at);
    q.state=std::int32_t(0x80000000u);q.function=0x41d5c0u;
    const std::uint32_t r=p.rand();
    const X size=((X(std::int32_t(r))*X(fb(0x38000100u)))*X(4.0f)+X(1.0f))*p.x(0x74fec8u);
    q.size=st(size);
    X a=((size-p.x(0x74fec8u))/(p.x(0x74feccu)-p.x(0x74fec8u)));
    a=(X(1.0f)-a)*p.x(0x74fed0u);
    if(!c0c3(a,X(1.0f)))a=X(1.0f);
    const std::uint32_t alpha=ftol(a*X(255.0f))<<24;
    for(unsigned k=0;k<4;++k)q.colour[k]=(p.u(0x74fed4u+k*4)&0xffffffu)|alpha;
    smoke_uv(q);
}
// 41D330 (player smoke): drag by road surface, growth, fade over its life;
// cleared behind the camera or past 30 m; its bottom kept above the road.
void smoke_ctrl_41d330(P& p,std::uint32_t at,std::uint32_t source_at){
    auto& q=p.at<SmokeParticle>(at);
    const auto& src=p.at<ParticleSource>(source_at);
    const auto& road=p.at<RoadPlane>(src.plane);
    const auto& cam=p.at<CameraWork>(p.u(Camera));
    advance(q);
    const std::uint32_t surface=road.surface;
    const X drag=(surface==0u||surface==3u||surface==5u)?p.x(0x74fe98u):p.x(0x74fe9cu);
    q.velocity[0]=st(drag*X(q.velocity[0]));
    q.velocity[2]=st(drag*X(q.velocity[2]));
    const std::uint32_t life=q.life-1u;q.life=life;
    const float size=st(p.x(0x74fe84u)+X(q.size));q.size=size;
    if(life==0u)kill(q);
    const std::uint32_t alpha=std::uint32_t(std::int32_t(q.alpha*life)/std::int32_t(q.life_start));
    {   // behind the camera plane: (p - eye) . (look - eye) < 0
        const X a=X(cam.look[0])-X(cam.eye[0]),b=X(cam.look[1])-X(cam.eye[1]),c=X(cam.look[2])-X(cam.eye[2]);
        const X px=X(q.position[0])-X(cam.eye[0]),py=X(q.position[1])-X(cam.eye[1]),pz=X(q.position[2])-X(cam.eye[2]);
        if(!jp5((pz*c+py*b)+px*a,X(0.0f)))kill(q);
    }
    {   const X dx=X(cam.position[0])-X(q.position[0]),dy=X(cam.position[1])-X(q.position[1]),dz=X(cam.position[2])-X(q.position[2]);
        if(!c0c3(hyp(dz,dy,dx),X(30.0f)))kill(q);
    }
    {   // The bottom of the puff (y - size * source size) under the road plane: lifted onto it.
        const X ox=X(road.point[0]),oy=X(road.point[1]),oz=X(road.point[2]),nx=X(road.normal[0]),ny=X(road.normal[1]),nz=X(road.normal[2]);
        const float px=q.position[0],pz=q.position[2];
        const float py=st(X(q.position[1])-X(size)*X(src.size));
        const X d=((X(pz)-oz)*nz+(X(py)-oy)*ny)+(X(px)-ox)*nx;
        if(!jp5(d,X(0.0f))){
            const X offset=-((nz*oz+ny*oy)+ox*nx);
            const X depth=driving::x87_abs(offset+((X(pz)*nz+X(py)*ny)+X(px)*nx));
            const X t=depth/driving::x87_sqrt((nx*nx+ny*ny)+nz*nz);
            const float lift_y=st(t*ny),lift_z=st(t*nz);
            q.position[0]=st(t*nx+X(q.position[0]));
            q.position[1]=st(X(lift_y)+X(q.position[1]));
            q.position[2]=st(X(lift_z)+X(q.position[2]));
        }
    }
    set_alpha(q,alpha<<24);
}
// 41D5C0 (rival smoke): grows by a shared step to [74FECC] while fading;
// cleared when transparent or past 400 m from the camera.
void smoke_ctrl_41d5c0(P& p,std::uint32_t at){
    auto& q=p.at<SmokeParticle>(at);
    const std::uint32_t cam=p.u(Camera);
    const X range=p.x(0x74feccu)-p.x(0x74fec8u);
    p.putx(0x95afb0u,range/p.x(0x74fec4u));
    const X size=p.x(0x95afb0u)+X(q.size);
    q.size=st(size);
    X a=(X(1.0f)-(size-p.x(0x74fec8u))/range)*p.x(0x74fed0u);
    if(!c0c3(a,X(1.0f)))a=X(1.0f);
    else if(!jp5(a,X(0.0f))){kill(q);return;}
    const X dx=p.x(cam+0xd4)-X(q.position[0]),dy=p.x(cam+0xd8)-X(q.position[1]),dz=p.x(cam+0xdc)-X(q.position[2]);
    if(!c0c3(hyp(dz,dx,dy),X(400.0f))){kill(q);return;}
    set_alpha(q,ftol(a*X(255.0f))<<24);
}
}
