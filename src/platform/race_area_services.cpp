// Small PC services of the race AREA/SKY runtime (race_area_runtime.cpp),
// ported here as free functions so the oracle probe
// tools/oracle/race_area_services_probe.inc can compare them with the
// original code: 44F980 (mode-7 transfer into a handle allocation),
// 44FC60 (+ protected bridge 44FCA4, measured), 407C30, 407E40, 4103A0,
// 406780, 43F960.
#include "platform/race_area_runtime.hpp"
#include <cstdio>
#include <cstring>
namespace outrun::platform {
namespace {
std::uint32_t le32(const std::uint8_t* p){std::uint32_t v;std::memcpy(&v,p,4);return v;}
std::string hex(std::uint32_t v){char t[16];std::snprintf(t,sizeof t,"%x",v);return t;}
}
bool race_area_loader_image(const std::vector<std::uint8_t>& file,bool sz,std::uint32_t base,std::uint32_t mode,
    std::vector<std::uint8_t>& image,std::string& error){
    // 44F980 state 0: .sz ("rbgx") reads the 4-byte size from the inflated
    // stream, others ("rbux") take the file size (423F10); state 1:
    // 580253(size+8) -> p, [p+size] = p, [p+size+4] = mode, handle = p+size,
    // dest = p; 423CF0 reads size bytes to p.
    std::size_t offset=0,size=file.size();
    if(sz){
        if(file.size()<4){error="inflated .sz shorter than its size word";return false;}
        size=le32(file.data());offset=4;
        if(size!=file.size()-4u){error="inflated .sz size word "+hex(std::uint32_t(size))+" != payload "+hex(std::uint32_t(file.size()-4u));return false;}
    }
    image.assign(file.begin()+std::ptrdiff_t(offset),file.begin()+std::ptrdiff_t(offset+size));
    const std::uint32_t words[2]={base,mode};
    image.insert(image.end(),reinterpret_cast<const std::uint8_t*>(words),reinterpret_cast<const std::uint8_t*>(words)+8);
    return true;
}
std::uint32_t race_async_take_44fc60(PcRaceAsyncSlot& s,std::uint32_t* size_24,std::uint32_t* size_28,std::uint32_t* handle){
    std::uint32_t freed=0;
    if(s.word0!=0u&&s.state==4u){
        if(size_24)*size_24=s.dest_24;
        if(size_28)*size_28=s.size_28;
        if(handle){*handle=s.handle_14;s.handle_14=0u;}       // 440CC0, bridge 44FCA4: mov [esi],0
        freed=s.handle_14;                                      // 44FBE0 state 4: 440CD0(&+14) when still set
    }else if(s.word0!=0u&&s.state!=0u)
        throw std::logic_error("44FC60 on a slot in flight (44FBE0 would free it)");
    s=PcRaceAsyncSlot{};                                        // 44FBE0 -> 44F950 zeroes the slot
    return freed;
}
void race_light_word_407c30(driving::Bytes lights,std::uint32_t a,std::uint32_t b,std::uint32_t c,std::uint32_t v){
    lights.put32(race_light_offset(a,b,c),v);
}
void race_light_words_407e40(driving::Bytes lights,std::uint32_t a,std::uint32_t b,std::uint32_t c,
    std::uint32_t v0,std::uint32_t v1,std::uint32_t v2){
    const auto o=race_light_offset(a,b,c);
    lights.put32(o+0x28,v0);lights.put32(o+0x2c,v1);lights.put32(o+0x30,v2);lights.put32(o+0x34,0u);
}
std::uint32_t race_light_offset(std::uint32_t a,std::uint32_t b,std::uint32_t c){
    // b == 0: 899B98 + (a+c)*0xA0; b == 1: 89A138 + (c+a*2)*0xA0; else 899D78 + (c+a*2)*0xA0
    if(b==0u)return (a+c)*0xa0u;
    if(b==1u)return 0x5a0u+(c+a*2u)*0xa0u;
    return 0x1e0u+(c+a*2u)*0xa0u;
}
std::array<std::uint32_t,3> race_shader_mode_4103a0(std::uint32_t a0,std::uint32_t a1){
    std::array<std::uint32_t,3> w{a0,0u,0u};                    // 89EDBC, 89EDC0, 89EDC4
    if(a0!=0u){
        if(a1&4u)w[2]=1u;
        if(a1&8u){w[2]=2u;w[1]=1u;}
    }
    return w;
}
std::uint32_t race_mesh_count_406780(driving::Bytes system,std::uint32_t object_count,std::uint32_t object,std::uint32_t mask){
    if(object==0xffffffffu)return 0u;
    const auto index=object&0xffffu;
    if(index>=object_count)return 0u;
    const auto record=0x18u+index*0x3cu;                        // object +18: mesh range, +1C: mesh records
    const auto range=system.u32(record+0x18);
    const std::uint32_t n=(system.u32(range+4)-system.u32(range))/0x38u;
    std::uint32_t mesh=system.u32(record+0x1c),count=0;
    for(std::uint32_t i=0;i<n;++i,mesh+=0x38u)if(system.u32(mesh)&mask)++count;
    return count;
}
std::uint32_t race_preset_43f960(std::uint32_t preset_78024c){return (preset_78024c==2u||preset_78024c==3u)?1u:0u;}
}
// ---- course-prog: stage-object lanes / area-switch services: begin ----
namespace outrun::platform {
void race_objects_relocate_4f0430(const PcRaceMemory& m,std::uint32_t p){
    // 4F049A: eax = [p]; nothing when 0. Entries are 8 bytes from p: the
    // first dword is made absolute (+p), then the ten optional pointers of
    // the entry (+4, +0C, ... +4C) when non-zero; the walk stops at the
    // entry whose (unrelocated) first dword is 0.
    if(m.u32(p)==0u)return;
    for(std::uint32_t e=0;;e+=8u){
        m.put32(p+e,m.u32(p+e)+p);
        const std::uint32_t q=m.u32(p+e);
        for(std::uint32_t f=4u;f<=0x4cu;f+=8u){const auto v=m.u32(q+f);if(v!=0u)m.put32(q+f,v+p);}
        if(m.u32(p+e+8u)==0u)break;
    }
}
std::uint32_t race_objects_frame_4f0910(const PcRaceMemory& m,std::uint32_t v){
    const std::uint32_t old=m.u32(0x84d8f0u);m.put32(0x84d8f0u,v);return old;
}
void race_objects_dynamic_open_4f0cb0(const PcRaceMemory& m,std::uint32_t id,std::uint32_t function,std::uint32_t data,
    void(*open)(void*,std::uint32_t,std::uint32_t),void* user){
    open(user,id,function);
    const std::uint32_t r=0x84d6d8u+(id-0x15cu)*0x34u;
    m.put32(r+0x28u,1u);
    for(std::uint32_t k=0;k<0x28u;k+=4)m.put32(r+k,m.u32(data+k));
    m.put32(r+0x2cu,function);m.put32(r+0x30u,m.u32(0x84d8f0u));
}
std::uint32_t race_objects_open_4f0d10(const PcRaceMemory& m,std::uint32_t lane,std::uint32_t root,
    void(*open)(void*,std::uint32_t,std::uint32_t),void* user){
    // Protected prologue: EAX = [84D6CC + lane*4]; the VM's flags are those
    // of the whole register (a root whose low 16 bits are 0 still opens,
    // checked by course_objects_probe), then EAX = [EAX].
    if(root==0u)return 0u;
    std::uint32_t r=m.u32(root);
    if(r==0u)return 0u;
    const float limit=-10000.0f;                                  // [628180]
    auto below=[&](std::uint32_t a){const float v=m.f32(a);return limit>v;};   // COMISS / JA (NaN: not taken)
    if(below(r+4u))return 0u;
    std::uint32_t event=m.u32(0x5da050u+lane*8u),opened=0;
    do{
        const std::uint32_t type=m.u32(r+0x1cu);
        open(user,event,m.u32(0x5da068u+type*4u));
        r+=0x28u;++event;++opened;
    }while(!below(r+4u));
    return opened;
}
void race_objects_copy_4efd20(const PcRaceMemory& m,std::uint32_t e,std::uint32_t lane){
    if(lane!=0u){
        for(std::uint32_t k=0;k<6u;++k)m.put32(0x6a5dc4u+k*4u,m.u32(e+k*4u));
        return;
    }
    std::uint32_t d=0x84bd08u;
    for(std::uint32_t n=0;m.u16(e)!=0xffffu&&n<7u;++n,e+=12u,d+=12u)
        for(std::uint32_t k=0;k<12u;k+=4u)m.put32(d+k,m.u32(e+k));
    m.put16(d,0xffffu);
}
}
// ---- course-prog: end ----
