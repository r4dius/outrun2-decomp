// PART_EFC core: nlParticle source/particle management, init and the
// function-pointer dispatch. See race_particles.hpp.
#include "platform/race_particles_port.hpp"
namespace outrun::platform {
using namespace particles_detail;
namespace particles_detail {
// Effect functions (race_particles_effects.cpp).
void make_plcar_param_41bd50(P&);
void make_occar_param_41c190(P&);
void pc_tire_smoke_init_41c420(P&);
void pc_tire_smoke_req_41c570(P&);
void oc_tire_smoke_req_41c940(P&);
void spark_init_41d6b0(P&);
void spark_req_41d810(P&);
void gravel_init_41de20(P&);
void gravel_req_41df00(P&);
void grass_init_41e360(P&);
void grass_req_41e440(P&);
void water_init_41e910(P&);
void water_req_41e9f0(P&);
void misc_init_41ee40(P&);
void misc_req_41ef00(P&);
void backfire_init_41f4b0(P&);
void backfire_req_41f5f0(P&);
void tire_mark_init_41fb10(P&);
void tire_mark_req_41fb50(P&);
void effect_init_420020(P&);
void effect_init_420200(P&);
void effect_init_420470(P&);
void glow_colour_4160f0(P&);
bool effect_callback(P&,std::uint32_t pc,std::uint32_t a0,std::uint32_t a1);
}
namespace {
// 4182D0: one step of the module generator: value = multiplier * value,
// then the sum of six masked shifts of it, scaled (X(sum) * scale - offset,
// left in ST0).
X random_4182d0(P& p,const ParticleGenerator& g){
    auto& value=p.at<std::uint32_t>(ParticleGeneratorValue);
    value=g.multiplier*value;
    const std::uint32_t r=value;
    std::uint32_t sum=std::uint32_t(std::int32_t(r)>>16)&g.mask;
    for(unsigned k=1;k<=5;++k)sum+=(r>>k)&g.mask;
    return X(std::int32_t(sum))*X(g.scale)-X(g.offset);
}
}
// 418350 nlParticleBorn(EDI = particle) with the current source [95AF50]:
// rejected by the generator against the source's limit, else the record
// gets the source's function, a random position and velocity, and the
// source's born function runs on it.
void particles_born_418350(PcParticleContext& pc,std::uint32_t particle){
    P p(pc);
    const std::uint32_t source_at=p.u(ParticleCurrentSource);
    auto& src=p.at<ParticleSource>(source_at);
    const std::uint32_t born=src.born;
    if(!particle)return;
    auto& g=p.at<ParticleGenerator>(ParticleGeneratorTable);
    const std::uint32_t step=g.multiplier*g.seed;
    p.put(ParticleGeneratorValue,step);
    if(std::int32_t(std::uint32_t(std::int32_t(step)>>16)&g.mask)>src.born_limit){g.seed=step;return;}
    auto& q=p.at<Particle>(particle);
    q.function=born;
    for(unsigned k=0;k<6;++k){
        const X v=random_4182d0(p,g)*X(src.spread[k])+X(src.base[k]);
        (k<3?q.position[k]:q.velocity[k-3])=st(v);
    }
    ++src.live;
    particles_call(pc,born,particle,source_at);
    g.seed=p.u(ParticleGeneratorValue);
}
// 418420 nlParticleGetWork(EAX = id): a free record of source id (popped
// from its free stack, or the first record without a function), 0 when
// full; the source becomes the current one. The entry is a protected VM
// jump ([103998C] -> 1043030); measured by the oracle: it performs
// `imul eax,eax,0x84` and resumes at 418426 (the Lindbergh
// nlParticleGetWork has the same body).
std::uint32_t particles_get_work_418420(PcParticleContext& pc,std::uint32_t id){
    P p(pc);
    const std::uint32_t source_at=source_address(id);
    auto& src=p.at<ParticleSource>(source_at);
    p.put(ParticleCurrentSource,source_at);
    if(src.free_list){
        if(src.free_count<=0)return 0;
        --src.free_count;
        return p.u(src.free_list+std::uint32_t(src.free_count)*4u);
    }
    for(std::uint32_t k=0,a=src.buffer;k<src.slots;++k,a+=src.stride)
        if(p.at<Particle>(a).function==0)return a;
    return 0;
}
// 418540 (EAX = buffer, ESI = source): runs every particle's function and
// counts the live ones; free records go on the free stack when the source
// keeps one.
void particles_source_ctrl_418540(PcParticleContext& pc,std::uint32_t buffer,std::uint32_t source_at){
    P p(pc);
    auto& src=p.at<ParticleSource>(source_at);
    src.free_count=0;
    std::uint32_t live=0;
    // The records and the free stack resolved once (no particle function
    // changes the buffer, size or stride of a source).
    auto* records=p.array<std::uint8_t>(buffer,src.slots*src.stride);
    auto* stack=src.free_list?p.array<std::uint32_t>(src.free_list,src.slots):nullptr;
    for(std::uint32_t k=0,a=buffer;k<src.slots;++k,a+=src.stride){
        auto& q=*reinterpret_cast<Particle*>(records+std::size_t(k)*src.stride);
        if(q.function){
            particles_call(pc,q.function,a,source_at);
            if(q.state<0)++live;
        }else if(stack){
            stack[src.free_count]=a;
            ++src.free_count;
        }
    }
    src.live=live;
}
// 41FF70(EAX = colour, a, r, g, b): each channel (unsigned, x87) times its
// factor, truncated by _ftol2, low byte. The 41FF78 relocated snippet
// (40EAAD) is `and eax,[1039DE0]` = `and eax,0xFF` (EXE value).
std::uint32_t particles_colour_scale_41ff70(PcParticleContext&,std::uint32_t colour,float fa,float fr,float fg,float fb_){
    auto chan=[](std::uint32_t v,float k){X x=X(std::int32_t(v));if(std::int32_t(v)<0)x=x+X(4294967296.0f);return ftol(x*X(k));};
    std::uint32_t ebx=chan((colour>>16)&0xffu,fr)&0xffu;
    ebx=(ebx&0xffff00ffu)|((chan(colour>>24,fa)&0xffu)<<8);
    ebx<<=8;
    ebx|=chan((colour>>8)&0xffu,fg)&0xffu;
    ebx<<=8;
    ebx|=chan(colour&0xffu,fb_)&0xffu;
    return ebx;
}
namespace particles_detail {
// 4182B0 nlParticleDead(particle, source).
void particle_dead_4182b0(P& p,std::uint32_t a){
    auto& q=p.at<Particle>(a);
    q.state=0;q.function=0;q.position[2]=0;q.position[1]=0;q.position[0]=0;
}
}
void particles_call(PcParticleContext& pc,std::uint32_t fn,std::uint32_t a0,std::uint32_t a1){
    P p(pc);
    if(fn==0x4182b0u){particle_dead_4182b0(p,a0);return;}
    if(effect_callback(p,fn,a0,a1))return;
    // An unported function pointer: report it through the service (the
    // caller latches it); nothing is executed in its place.
    p.call(fn,{a0,a1});
}
// 41FB10 then 41C420 alone (the goal camera script op 21): the tire marks and the tire smoke reset.
void particles_tire_reset_41fb10_41c420(PcParticleContext& pc){P p(pc);tire_mark_init_41fb10(p);pc_tire_smoke_init_41c420(p);}
// 41BBF0 ParticleEfc_Init: the eleven sources from the template, then each
// effect's setup (the arcade ending mode 0x18 only has 420470).
void particles_init_41bbf0(PcParticleContext& pc){
    P p(pc);
    p.copy(source_address(0),0x74e8e0u,0x21);
    p.copy(source_address(1),source_address(0),0x14a);  // overlapping forward copy: the ten other sources
    p.put(0x8a8ce4u,0);
    tire_mark_init_41fb10(p);
    pc_tire_smoke_init_41c420(p);
    if(p.u(RootMode)==0x18u){effect_init_420470(p);return;}
    spark_init_41d6b0(p);gravel_init_41de20(p);grass_init_41e360(p);water_init_41e910(p);
    misc_init_41ee40(p);backfire_init_41f4b0(p);effect_init_420020(p);effect_init_420200(p);
}
// 41BC60 ParticleEfc_Ctrl: car parameters, the effect requests (none in
// mode 0x18; no sparks in mode 0x13; nothing while event 8 is suspended or,
// in a network race, while the player car is out), glow colours, then every
// active source's particles.
void particles_control_41bc60(PcParticleContext& pc){
    P p(pc);
    const std::uint32_t car=p.u(PlayerCar);
    make_plcar_param_41bd50(p);
    make_occar_param_41c190(p);
    const std::uint32_t mode=p.u(RootMode);
    bool requests=false,spark=false;
    auto suspended=[&]{return (p.u8(EventFlags+8u)&0x10u)!=0u;};   // 440A50(8)
    if(mode==0x13u){requests=!suspended();}
    else if(mode!=0x18u){
        if(!suspended()){
            // 46C550: 55A930(7F9460) ? [800ABC] : 0
            const std::uint32_t net=p.u(0x7f9460u+0x60u)?p.u(0x800abcu):0u;
            if(!(net&&p.i(car+0xd18)>0)){spark=true;requests=true;}
        }
    }
    if(spark)spark_req_41d810(p);
    if(requests){
        gravel_req_41df00(p);grass_req_41e440(p);water_req_41e9f0(p);misc_req_41ef00(p);backfire_req_41f5f0(p);
        pc_tire_smoke_req_41c570(p);oc_tire_smoke_req_41c940(p);tire_mark_req_41fb50(p);
    }
    glow_colour_4160f0(p);
    for(std::uint32_t id=0;id<ParticleSourceCount;++id){
        const auto& src=source(p,id);
        if(src.state<0)particles_source_ctrl_418540(pc,src.buffer,source_address(id));
    }
}
}
