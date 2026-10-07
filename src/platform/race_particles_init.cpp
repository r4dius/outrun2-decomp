// PART_EFC init functions (41BBF0 children). See race_particles.hpp.
#include "platform/race_particles_port.hpp"
namespace outrun::platform::particles_detail {
namespace {
void init_particles(P& p,std::uint32_t buffer,std::uint32_t count,std::uint32_t stride){
    for(std::uint32_t k=0,a=buffer;k<count;++k,a+=stride)p.copy(a,ParticleRecordTemplate,8);
}
// fild m32 + (negative ? 2^32) : the unsigned conversion, stored as float.
float unsigned_float(std::uint32_t v){X x=X(std::int32_t(v));if(std::int32_t(v)<0)x=x+X(4294967296.0f);return st(x);}
// 406600(id): resource id>>16, object id&0xFFFF of its list (0 when out of range).
std::uint32_t resource_object_406600(P& p,std::uint32_t id){
    const std::uint32_t r=P::resource(id>>16),k=id&0xffffu;
    if(!(k<p.u(p.u(r)+8)))return 0;
    return p.u(p.u(p.u(r+0x24))+k*4u);
}
}
// 486220: tire-mark vertex templates (5BA990, .rdata) and the three textures.
void tire_mark_setup_486220(P& p){
    static constexpr std::uint32_t Vertex5ba990[6]={0,0,0,0xffffffffu,0,0};
    for(std::uint32_t esi=0x64fe24u,n=0;n<3;++n,esi+=0x14u){
        std::uint32_t edx=p.u(esi+4);
        for(std::uint32_t k=0;std::int32_t(k)<std::int32_t(p.u(esi)*6u);++k,edx+=0x18u)
            for(unsigned w=0;w<6;++w)p.put(edx+w*4,Vertex5ba990[w]);
    }
    p.put(0x64fe2cu,resource_object_406600(p,0x570027u));
    p.put(0x64fe40u,resource_object_406600(p,0x570028u));
    p.put(0x64fe54u,resource_object_406600(p,0x570029u));
}
// 41FB10 tire_mark_init. The instruction at 41FB17 is a protected VM jump
// ([1039CFC] -> 1041D30); measured by the oracle: it resumes the clear loop
// with EAX = 0 and ECX = 0 (96 records of 0x28 bytes at 9174D8).
void tire_mark_init_41fb10(P& p){
    tire_mark_setup_486220(p);
    for(std::uint32_t eax=0;eax<0xf00u;eax+=0x28u){
        p.put(eax+0x9174d8u,0);p.put(eax+0x9174dcu,0);p.put(eax+0x9174e0u,0);p.put(eax+0x9174fcu,0);
    }
}
namespace {
// The buffer setup every effect init shares: `slots` records of `stride`
// bytes cleared from the template, no free stack yet.
void buffer_setup(P& p,ParticleSource& src,std::uint32_t buffer,std::uint32_t slots,std::uint32_t stride){
    src.slots=slots;src.buffer=buffer;
    init_particles(p,buffer,slots,stride);
    src.free_list=0;src.free_count=0;src.stride=stride;
}
// State: the draw kind bits `clear` replaced by `kind`, and active.
void activate(ParticleSource& src,std::uint32_t clear,std::uint32_t kind){
    src.state=std::int32_t(((std::uint32_t(src.state)&~clear)|kind)|0x80000000u);
}
void blend(ParticleSource& src,std::uint32_t source_blend,std::uint32_t dest_blend){src.src_blend=source_blend;src.dest_blend=dest_blend;}
void texture(ParticleSource& src,std::uint32_t handle){src.texture=handle;src.texture_list=0;src.texture_count=0;}
// The 420020 / 420200 / 420470 effects: 0x28-byte records with a free stack.
void stack_effect(P& p,ParticleSource& src,std::uint32_t buffer,std::uint32_t slots,std::uint32_t stack,std::uint32_t born,float size){
    buffer_setup(p,src,buffer,slots,0x28);
    src.free_list=stack;p.fill(stack,0,slots);
    src.born=born;src.size=size;src.point_size=size;src.born_limit=0x7fff;src.colour=0xffffffffu;
    src.plane=0x915228u;
}
}
// 41C420 PcTireSmokeInit (source 4): 512 records of 0x5C at 922018.
void pc_tire_smoke_init_41c420(P& p){
    auto& src=source(p,SourceTireSmoke);
    p.put(0x95afacu,0);
    buffer_setup(p,src,0x922018u,0x200,0x5c);
    src.born=0x41d1e0u;
    const float spread=fb(0x3c23d70au);   // 0.01
    src.base[0]=0;src.base[1]=1.0f;src.base[2]=0;src.base[3]=0;src.base[4]=fb(0x3ca3d70au);src.base[5]=0;
    for(auto& v:src.spread)v=spread;
    src.born_limit=0x7fff;activate(src,4,3);
    texture(src,p.texture(0x57,0x19,0x64));
    blend(src,5,6);src.pixel_shader=1;
}
// 41D6B0 PtclSparkInit (source 3): 500 records of 0x50 at 9183D8 with a
// free stack, sized by the spark texture.
void spark_init_41d6b0(P& p){
    const std::uint32_t handle=p.texture(0x57,0x14,0x50);
    // IDirect3DTexture9::GetLevelDesc(handle, 0, &desc) (vtable +0x44).
    std::array<std::uint8_t,0x20> desc{};
    const std::size_t mark=p.m.mark();p.m.map(PcParticleLocalDesc,desc.data(),desc.size());
    try{p.call(PcParticleGetLevelDesc,{handle,0,PcParticleLocalDesc});}catch(...){p.m.release(mark);throw;}
    p.m.release(mark);
    std::uint32_t width,height;std::memcpy(&width,desc.data()+0x18,4);std::memcpy(&height,desc.data()+0x1c,4);
    auto& src=source(p,SourceSpark);
    buffer_setup(p,src,0x9183d8u,0x1f4,0x50);
    src.free_list=0x904340u;p.fill(0x904340u,0,0x1f4);
    src.born=0x41dc60u;
    src.size=unsigned_float(width);src.point_size=unsigned_float(height);
    texture(src,handle);blend(src,2,2);
    p.fill(0x9366c8u,0,4);                    // the four wheel spark timers
    src.born_limit=0x7fff;activate(src,2,5);
    src.plane=0x934448u;                       // player tire records
    p.fill(0x8fa32cu,0xffffffffu,4);           // their pool sprite handles
}
// 41DE20 gravel init (source 1): 256 records of 0x50 at 8FA340.
void gravel_init_41de20(P& p){
    auto& src=source(p,SourceGravel);
    buffer_setup(p,src,0x8fa340u,0x100,0x50);
    src.born=0x41e190u;src.size=fb(0x3d4ccccdu);src.point_size=fb(0x3d4ccccdu);   // 0.05
    src.born_limit=0x7fff;src.colour=0xffffffffu;activate(src,5,2);
    blend(src,5,6);texture(src,p.texture(0x57,0x15,0x54));
}
// 41E360 grass init (source 2): 256 records of 0x50 at 90EB10.
void grass_init_41e360(P& p){
    auto& src=source(p,SourceGrass);
    buffer_setup(p,src,0x90eb10u,0x100,0x50);
    src.born=0x41e780u;src.size=fb(0x3d99999au);src.point_size=fb(0x3d99999au);   // 0.075
    src.born_limit=0x7fff;src.colour=0xffffffffu;activate(src,5,2);
    blend(src,5,6);texture(src,p.texture(0x57,0x16,0x58));
}
// 41E910 water init (source 5): 256 records of 0x5C at 92E848.
void water_init_41e910(P& p){
    auto& src=source(p,SourceWater);
    buffer_setup(p,src,0x92e848u,0x100,0x5c);
    const float size=p.f(0x74ffd0u);
    src.born=0x41ed00u;src.size=size;src.point_size=size;
    src.born_limit=0x7fff;activate(src,4,3);
    texture(src,p.texture(0x57,0x18,0x60));
    p.fill(0x9154c8u,0,4);                    // the four wheel splash timers
    src.plane=0x934448u;
}
// 41EE40 misc init (source 6): 64 records of 0x5C at 913B10 (born kept from the template).
void misc_init_41ee40(P& p){
    auto& src=source(p,SourceMisc);
    buffer_setup(p,src,0x913b10u,0x40,0x5c);
    src.born_limit=0x7fff;activate(src,4,3);
    texture(src,p.texture(0x57,0x19,0x64));src.z_enable=0;
    blend(src,5,6);src.pixel_shader=1;
}
// 41F4B0 backfire init (source 7): eight records of 0x5C at 954750.
void backfire_init_41f4b0(P& p){
    auto& src=source(p,SourceBackfire);
    init_particles(p,0x954750u,8,0x5c);
    const float size=p.f(0x75005cu);
    src.slots=8;src.buffer=0x954750u;src.free_list=0;src.free_count=0;src.stride=0x5c;
    src.born=0x41f7f0u;src.size=size;src.point_size=size;src.born_limit=0x7fff;activate(src,4,3);
    texture(src,p.texture(0x57,0x1d,0x74));
    src.unknown80=0xbc23d70au;                 // -0.01
    blend(src,2,2);
}
// 420020 (source 8): 1024 records at 904B10, free stack 92D818.
void effect_init_420020(P& p){
    auto& src=source(p,SourceEffect8);
    stack_effect(p,src,0x904b10u,0x400,0x92d818u,0x420110u,32.0f);
    p.fill(0x915210u,0,6);
    activate(src,1,6);blend(src,5,6);
}
// 420200 (source 9): 2048 records at 940750, free stack 9154D8.
void effect_init_420200(P& p){
    auto& src=source(p,SourceEffect9);
    stack_effect(p,src,0x940750u,0x800,0x9154d8u,0x420350u,24.0f);
    p.fill(0x9406f0u,0,0x17);
    activate(src,1,6);blend(src,5,6);
    p.put(0x94074cu,0);
}
// 420470 (source 10, the arcade ending): 1024 records at 9366D8, free stack 954A30.
void effect_init_420470(P& p){
    auto& src=source(p,SourceEffect10);
    stack_effect(p,src,0x9366d8u,0x400,0x954a30u,0x4207b0u,32.0f);
    p.fill(0x9406d8u,0,6);
    activate(src,1,6);blend(src,5,2);
}
}
