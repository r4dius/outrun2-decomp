#include "platform/rob_motion.hpp"
#include <stdexcept>
namespace outrun::platform {
std::uint32_t RobotHeap::malloc(std::uint32_t n){
    const std::uint32_t size=(std::max(n,1u)+15u)&~15u;
    if(n>0x1000000u||next+size>End)return 0;
    Block b;b.base=next;b.bytes.assign(size,0xcd);b.live=true;       // uninitialised heap bytes
    next+=size;++mallocs;blocks.push_back(std::move(b));return blocks.back().base;
}
void RobotHeap::free(std::uint32_t a){
    for(auto& b:blocks)if(b.base==a&&b.live){b.live=false;++frees;return;}
    if(a)throw std::runtime_error("race robots: 580BC2 frees an address the robot heap did not return");
}
void RobotHeap::map(PcRaceMemory& m){for(auto& b:blocks)m.map(b.base,b.bytes.data(),b.bytes.size());}
namespace {
std::int32_t i16(const PcRaceMemory& m,std::uint32_t a){return std::int32_t(std::int16_t(m.u16(a)));}
// 522DB0: sum of the bone records' +30 words.
std::uint32_t extra_522db0(const PcRaceMemory& m,std::uint32_t set){
    std::uint32_t sum=0;const std::int32_t n=i16(m,set+0x10);
    std::uint32_t rec=m.u32(set+0x18)+0x30u;
    for(std::int32_t k=0;k<n;++k,rec+=0x38u)sum+=m.u32(rec);
    return sum;
}
// 522DD0(set, records, bones, extra).
void records_522dd0(PcRaceMemory& m,std::uint32_t set,std::uint32_t buf,std::uint32_t bones,std::uint32_t extra){
    const std::int32_t n=i16(m,set+0x10);
    std::uint32_t rec=m.u32(set+0x18),out=buf+0x40u;
    for(std::int32_t i=0;i<n;++i,out+=0x350u,rec+=0x38u){
        m.put32(out+0x2f8u,std::uint32_t(i));
        m.put32(out,m.u32(rec)+bones);
        m.put8(out+4,m.u8(rec+4));m.put8(out+5,m.u8(rec+6));m.put8(out+6,m.u8(rec+7));
        m.put32(out+0x1f8u,m.u32(rec+0x24));m.put32(out+0x120u,m.u32(rec+0x18));m.put32(out+0x48u,m.u32(rec+0xc));
        m.put32(out+0x240u,m.u32(rec+0x28));m.put32(out+0x168u,m.u32(rec+0x1c));m.put32(out+0x90u,m.u32(rec+0x10));
        m.put32(out+0x288u,m.u32(rec+0x2c));m.put32(out+0x1b0u,m.u32(rec+0x20));m.put32(out+0xd8u,m.u32(rec+0x14));
        m.put8(out+7,m.u8(rec+0x30));
        if(m.u32(rec+0x30)){
            m.put32(out+0x2f0u,extra);
            extra+=std::uint32_t(std::int32_t(std::int8_t(m.u8(out+7))))*4u;
        }else m.put32(out+0x2f0u,0);
        for(std::int32_t j=0;j<std::int32_t(std::int8_t(m.u8(out+7)));++j){
            const std::uint32_t id=m.u16(m.u32(rec+0x34)+std::uint32_t(j)*2u+bones);
            m.put32(m.u32(out+0x2f0u)+std::uint32_t(j)*4u,id*0x350u+buf);
        }
    }
}
// 5206E0: release the bone set.
void release_5206e0(PcRaceMemory& m,RobotHeap& heap,driving::PcAllocatorStateStacks& st,std::uint32_t set){
    if(const auto buf=m.u32(set+0x1c)){
        const std::int32_t n=i16(m,set+0x10);
        for(std::int32_t k=0;k<n;++k){const auto a=buf+0x330u+std::uint32_t(k)*0x350u;if(m.u32(a))m.put32(a,0);}
    }
    if(const auto buf=m.u32(set+0x1c)){
        driving::push_alloc_state_a_440d10(st,0);heap.free(buf);(void)driving::pop_alloc_state_a_440d30(st);
        m.put32(set+0x1c,0);
    }
    for(std::uint32_t k=0;k<0x44u;k+=4)m.put32(set+k,0);
}
// 520620: build the bone set.
void build_520620(PcRaceMemory& m,RobotHeap& heap,driving::PcAllocatorStateStacks& st,std::uint32_t set,std::uint32_t id,std::uint32_t entry,std::uint32_t bones){
    for(std::uint32_t k=0;k<0x44u;k+=4)m.put32(set+k,0);
    m.put16(set+0x14,m.u16(entry));m.put16(set+0x10,m.u16(entry+2));
    m.put32(set+0x18,m.u32(entry+8)+bones);m.put32(set+8,id);
    const std::uint32_t extra=extra_522db0(m,set);
    driving::push_alloc_state_a_440d10(st,0);driving::push_alloc_state_b_440d50(st,0);
    const std::int32_t n=i16(m,set+0x10);
    const std::uint32_t buf=heap.malloc((std::uint32_t(n)*0xd4u+extra)*4u);
    if(buf){auto& b=heap.blocks.back();m.map(b.base,b.bytes.data(),b.bytes.size());}
    m.put32(set+0x1c,buf);
    (void)driving::pop_alloc_state_a_440d30(st);(void)driving::pop_alloc_state_b_440d70(st);
    if(!buf)return;
    const std::uint32_t bytes=std::uint32_t(n)*0x350u;
    for(std::uint32_t k=0;k<bytes;++k)m.put8(buf+k,0);
    records_522dd0(m,set,buf,bones,buf+bytes);
}
}
void rob_unset_bone_4f2260(PcRaceMemory& m,RobotHeap& heap,driving::PcAllocatorStateStacks& st,std::uint32_t motion){
    release_5206e0(m,heap,st,motion+0x58u);
    m.put32(motion+0x54u,0);
}
void rob_set_bone_4f1cf0(PcRaceMemory& m,RobotHeap& heap,driving::PcAllocatorStateStacks& st,std::uint32_t motion,std::uint32_t entry,std::uint32_t bones){
    const std::uint32_t set=motion+0x58u;
    release_5206e0(m,heap,st,set);
    if(!entry)return;
    m.put32(motion+0x54u,entry);
    build_520620(m,heap,st,set,m.u32(motion),entry,bones);
}
}
