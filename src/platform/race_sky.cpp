#include "platform/race_sky.hpp"
#include <cstring>
namespace outrun::platform {
extern std::uint8_t RaceSkyDataImage[0x10];
PcRaceSkyState::PcRaceSkyState():data(RaceSkyDataImage,RaceSkyDataImage+0x10){}
namespace {
constexpr std::uint32_t None=0xffffffffu;
std::uint32_t fbits(float f){std::uint32_t u;std::memcpy(&u,&f,4);return u;}
struct Sky {
    PcRaceContext& c;PcRaceMemory& m;
    explicit Sky(PcRaceContext& cc):c(cc),m(cc.m){}
    std::uint32_t call(std::uint32_t pc,std::initializer_list<std::uint32_t> args){
        PcRaceCall k{};k.pc=pc;k.argc=std::uint32_t(args.size());unsigned i=0;for(auto a:args)k.args[i++]=a;
        if(!c.service)throw std::logic_error("race sky: no service for PC call");
        return c.service(k);
    }
    void draw(std::uint32_t pc,std::initializer_list<std::uint32_t> args){
        if(!c.draws)throw std::logic_error("race sky: display without draw list");
        PcVehicleDrawCall d{};d.pc=pc;d.argc=std::uint32_t(args.size());unsigned i=0;for(auto a:args)d.args[i++]=a;
        const auto cur=c.matrices.current();for(unsigned k=0;k<64;++k)d.matrix[k]=cur.u8(k);
        c.draws->push_back(d);
    }
    void copy16(std::uint32_t to,std::uint32_t from){for(unsigned k=0;k<16;++k)m.put32(to+k*4,m.u32(from+k*4));}
};
std::int16_t third(std::int16_t v){return std::int16_t(std::int32_t(v)/3);}  // imul 0x55555556
}
void race_sky_init_451a30(PcRaceContext& c,std::uint32_t w){
    Sky s(c);auto& m=c.m;
    s.call(0x407c30u,{0,0,0,0});
    for(std::uint32_t i=0;i<2;++i)s.call(0x407c30u,{0,2,i,0});
    for(std::uint32_t i=0;i<2;++i)s.call(0x407c30u,{0,1,i,0});
    s.call(0x407e40u,{0,0,0,0,0,0});
    for(std::uint32_t layer=0;layer<2;++layer){
        const std::uint32_t r=w+layer*0x90u;
        for(std::uint32_t k=0;k<3;++k){
            m.putf(r+0x10+k*4,0.f);
            m.put32(r+0x1c+k*4,m.u32(0x638668u+k*4));
            m.put32(r+k*4,None);
        }
        m.put16(r+0x28,0);m.put16(r+0x2a,0);m.put16(r+0x2c,0);
        m.putf(r+0x30,1.f);m.putf(r+0x34,1.f);m.putf(r+0x40,1.f);
    }
    m.put32(w+0x38,1);m.put32(w+0x3c,0);m.put32(w+0xc8,0);m.put32(w+0xcc,0);
    m.put32(0x7d3a64u,2);
    s.copy16(w+0x50,0x7d2da0u);   // 44BEA0
    s.copy16(w+0xe0,0x7d3130u);   // 44BEB0
}
void race_sky_control_451b40(PcRaceContext& c,std::uint32_t w){
    auto& m=c.m;
    constexpr std::uint32_t Out=0x7fff0040u; // 451B40 [esp+8] buffer
    for(std::uint32_t i=0;i<2;++i){
        const std::uint32_t rec=i?m.u32(0x7d31dcu):m.u32(0x7d3188u);
        if(!rec){(void)m.u32(0x7d31dcu);throw std::logic_error("451B40: 44C420 null course record leaves [esp+14]/[esp+18] unwritten");}
        std::array<std::uint8_t,0x1c> buffer{};
        const auto mark=m.mark();m.map(Out,buffer.data(),buffer.size());
        try{race_area_sky_ids_44c420(c,Out,i);}catch(...){m.release(mark);throw;}
        const std::uint32_t b1=m.u32(Out+4),b2=m.u32(Out+8),b3=m.u32(Out+0xc),b4=m.u32(Out+0x10);
        const float f5=m.f32(Out+0x14),f6=m.f32(Out+0x18);
        m.release(mark);
        const std::uint32_t r=w+i*0x90u;
        const float K=[]{std::uint32_t u=0x3989421fu;float f;std::memcpy(&f,&u,4);return f;}(); // 5A4610
        m.putf(r+0x1c,f5);m.putf(r+0x20,f6);m.put32(r,b1);m.put32(r+4,b2);
        const float x=f5*K+m.f32(r+0x10);
        const float y=f6*K+m.f32(r+0x14);
        m.put32(r+8,b3);m.put32(r+0xc,b4);m.putf(r+0x10,x);m.putf(r+0x14,y);
    }
    for(std::uint32_t k=0;k<0x10;k+=4)if(m.u32(w+k)==m.u32(w+0x90+k))m.put32(w+0x90+k,None);
}
void race_sky_object_452080(PcRaceContext& c,std::uint32_t o,float alpha){
    Sky s(c);auto& m=c.m;
    const std::int32_t n=std::int32_t(s.call(0x406780u,{o,1}));
    const float full=m.f32(0x8a8c18u)*0.25f;   // 4162B0(0)
    if(n>1){
        if(alpha>=0.899999976f){
            s.draw(0x4044f0u,{0,0,0,0,0xf});s.draw(0x4044f0u,{1,1,0,0,8});
            s.draw(0x4056d0u,{o,fbits(full),0,None});
            s.draw(0x4044f0u,{0,0,0,0,0});s.draw(0x4044f0u,{1,1,0,8,7});
            s.draw(0x4056d0u,{o,fbits(alpha),0,None});
        }else{
            s.draw(0x4044f0u,{0,1,0,8,7});s.draw(0x4044f0u,{1,1,0,8,7});
            s.draw(0x4056d0u,{o,fbits(alpha),0,None});
        }
    }else{
        if(alpha>=0.899999976f){
            s.draw(0x4044f0u,{0,0,0,0,0xf});s.draw(0x4044f0u,{1,0,0,0,0xf});
            s.draw(0x4056d0u,{o,fbits(full),0,None});
        }else{
            s.draw(0x4044f0u,{0,1,0,8,7});s.draw(0x4044f0u,{1,1,0,8,7});
            s.draw(0x4056d0u,{o,fbits(alpha),0,None});
        }
    }
    s.draw(0x404540u,{});
}
void race_sky_display_4521c0(PcRaceContext& c,std::uint32_t w){
    Sky s(c);auto& m=c.m;
    driving::pc_matrix_push_load(c.matrices,m.bytes(0x95dba0u,64));   // 411220
    s.draw(PcRaceSetRenderState,{7,0});
    for(std::uint32_t layer=0;layer<2;++layer){
        const std::uint32_t r=w+layer*0x90u;
        driving::pc_matrix_load_rotation(c.matrices,m.bytes(r+0x50,64));
        auto step=[&](std::uint32_t k){
            if(m.u32(r+0x38+k*4)==0)return;
            const float a=m.f32(r+0x30+k*4);
            if(0.100000001f>=a)return;
            if(k==0){
                if(m.u32(r)!=None)race_sky_object_452080(c,m.u32(r),a);
                if(m.u32(r+4)!=None)race_sky_object_452080(c,m.u32(r+4),a);
            }else if(k==1){
                if(m.u32(r+8)!=None)race_sky_object_452080(c,m.u32(r+8),a);
            }
        };
        if(layer==0){step(0);step(1);}else{step(1);step(0);}
    }
    s.draw(PcRaceSetRenderState,{7,1});
    driving::pc_matrix_pop(c.matrices);
}
void race_sky_reset_451dd0(PcRaceContext& c){
    auto& m=c.m;
    if(m.u32(0x7d33b4u)!=0)return;
    const std::uint32_t w=m.u32(0x79f6dcu);
    const std::uint8_t al=std::uint8_t(race_area_sky_steps_44c640(m));
    const std::uint16_t cx=std::uint16_t(std::int16_t(std::int8_t(al)));
    m.put16(w+0x28,0);m.putf(w+0x30,1.f);m.putf(w+0x34,1.f);m.put32(w+0x38,0);m.put32(w+0x3c,0);m.put16(w+0x2c,0);
    const std::uint16_t t=std::uint16_t(std::uint32_t(cx)*0x3cu);
    const std::uint16_t ax=std::uint16_t(third(std::int16_t(t)));
    m.put16(w+0x2a,ax);m.put16(w+0xb8,ax);m.put16(w+0xba,ax);
    m.put16(w+0xbc,t);
    m.putf(w+0x40,1.f);m.putf(w+0xc0,1.f);m.putf(w+0xc4,1.f);m.put32(w+0xc8,1);m.put32(w+0xcc,1);m.putf(w+0xd0,1.f);
    m.put16(0x638664u,t);
    for(unsigned k=0;k<16;++k)m.put32(w+0xe0+k*4,m.u32(w+0x50+k*4));
    m.put32(0x7d3a64u,0);
}
std::uint32_t race_sky_step_451e90(PcRaceContext& c){
    auto& m=c.m;
    if(m.u32(0x7d33b4u)!=0)return 0;   // protected entry bridge: EAX = [7D33B4]
    const std::uint32_t s=m.u32(0x7d3a64u);
    const std::uint32_t w=m.u32(0x79f6dcu);
    std::uint32_t result=1;
    if(s==0){
        const std::uint32_t car=m.u32(0x799d18u);
        const std::uint32_t t=race_area_sky_time_44c610(m);
        if(m.i16(car+0x64)<std::int16_t(t))return 1;
        for(unsigned k=0;k<16;++k)m.put32(w+0x50+k*4,m.u32(0x7d2da0u+k*4));
        m.put32(0x7d3a64u,1);
    }else if(s!=1)return 0;
    const std::int16_t steps=m.i16(0x638664u);
    auto fraction=[&](std::uint32_t word,std::uint32_t out){ // returns the decremented word
        const std::int16_t v=m.i16(word);
        const float f=float(std::int32_t(v))/float(std::int32_t(third(steps)));
        const std::int16_t next=std::int16_t(v-1);
        m.putf(out,f);m.put16(word,std::uint16_t(next));
        return next;
    };
    if(m.i16(w+0xb8)>0){
        if(fraction(w+0xb8,w+0xc0)<=0){m.putf(w+0xc0,0.f);m.put32(w+0xc8,0);m.put32(w+0xcc,1);m.put32(w+0x3c,1);}
    }else if(m.i16(w+0xba)>0){
        if(fraction(w+0xba,w+0xc4)<=0){m.putf(w+0xc4,0.f);m.put32(w+0xcc,0);m.put32(w+0x3c,1);m.put32(w+0x38,1);}
    }else if(m.i16(w+0x2a)>0){
        if(fraction(w+0x2a,w+0x34)<=0){m.putf(w+0x34,0.f);m.put32(w+0x3c,0);m.put32(w+0x38,1);result=0;m.put32(0x7d3a64u,2);}
    }
    {
        const std::int16_t v=m.i16(w+0xbc);
        const float f=float(std::int32_t(v))/float(std::int32_t(steps));
        const std::int16_t next=std::int16_t(v-1);
        m.putf(w+0xd0,f);m.put16(w+0xbc,std::uint16_t(next));
        if(next<=0)m.putf(w+0xd0,0.f);
    }
    return result;
}
}
