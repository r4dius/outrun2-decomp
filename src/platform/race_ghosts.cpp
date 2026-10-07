#include "system/exe_image.hpp"
#include "platform/race_ghosts.hpp"
#include "driving/pc_car_services_ghost.hpp"
#include "driving/pc_x87.hpp"
#include <cmath>
#include <cstdio>
#include <cstring>
#include <stdexcept>
#include <string>
namespace outrun::platform {
namespace {
#include "platform/race_ghosts_tables.inc"
constexpr std::uint32_t Block=0x7f8d80u;
// Stack buffers passed to services by address (mapped while the call runs).
constexpr std::uint32_t Local416700=0x7ffe0000u,Local467340=0x7ffe0100u,Local4686c0=0x7ffe0200u;
std::uint32_t call(PcRaceContext& c,std::uint32_t pc,std::initializer_list<std::uint32_t> args,std::uint32_t eax=0,std::uint32_t ecx=0){
    PcRaceCall k;k.pc=pc;k.eax=eax;k.ecx=ecx;k.argc=std::uint32_t(args.size());
    std::size_t i=0;for(auto a:args)k.args[i++]=a;
    return c.service(k);
}
// A PC stack char buffer of 0x100 bytes.
struct Local {
    PcRaceMemory& m;std::size_t mark;std::array<std::uint8_t,0x100> bytes{};
    Local(PcRaceMemory& mem,std::uint32_t at):m(mem),mark(mem.mark()){m.map(at,bytes.data(),bytes.size());}
    ~Local(){m.release(mark);}
};
void put_string(PcRaceMemory& m,std::uint32_t at,const std::string& s){
    for(std::size_t i=0;i<s.size();++i)m.put8(at+std::uint32_t(i),std::uint8_t(s[i]));
    m.put8(at+std::uint32_t(s.size()),0);
}
std::string format2(const char* format,const char* prefix,int value){
    char out[0x100];
    if(prefix)std::snprintf(out,sizeof out,format,prefix,value);else std::snprintf(out,sizeof out,format,value);
    return out;
}
// 449A60 / REP MOVSD + REP MOVSB: forward, so an overlapping copy to a
// higher address repeats the source like the PC (467260 copies a 0x1C
// packet over the next one, 8 or 0x10 bytes further).
void copy(PcRaceMemory& m,std::uint32_t dst,std::uint32_t src,std::uint32_t n){
    if(!n)return;
    const auto* s=m.at(src,n);auto* d=m.at(dst,n,true);
    std::uint32_t i=0;
    for(;i+4<=n;i+=4){std::uint32_t w;std::memcpy(&w,s+i,4);std::memcpy(d+i,&w,4);}
    for(;i<n;++i)d[i]=s[i];
}
void zero(PcRaceMemory& m,std::uint32_t dst,std::uint32_t n){if(n)std::memset(m.at(dst,n,true),0,n);}
// 449C10(h,m,s,ms) = ((h*60+m)*60+s)*1000+ms
constexpr std::uint32_t time_449c10(std::uint32_t h,std::uint32_t mi,std::uint32_t s,std::uint32_t ms){return ((h*60u+mi)*60u+s)*1000u+ms;}
// 4F6250: zero a 0x28-byte pose (floats +0..+C,+18,+1C; words +10,+14,+20,+24).
void pose_clear_4f6250(PcRaceMemory& m,std::uint32_t p){for(std::uint32_t k=0;k<0x28;k+=4)m.put32(p+k,0);}
}

PcGhostState::PcGhostState():table(Exe7457c8,Exe7457c8+sizeof Exe7457c8){}
void ghost_map_tables(PcRaceMemory& m){m.map_const(0x5a27d8u,Exe5a27d8,sizeof Exe5a27d8);m.map_const(0x5b0570u,Exe5b0570,sizeof Exe5b0570);}

void ghost_save_layout_416610(PcRaceMemory& m){
    std::uint32_t file=0,offset=0;
    for(std::uint32_t e=0;e<0x54u;++e){
        const std::uint32_t size=m.u16(0x7457ceu+e*8u);
        if(std::int32_t(size+offset)>0x17ff0)++file,offset=0;
        m.put32(0x7457c8u+e*8u,offset);m.put16(0x7457ccu+e*8u,std::uint16_t(file));
        offset+=size;
    }
    std::uint32_t e=0x54u;
    for(unsigned f=0;f<8u;++f){
        ++file;offset=0;
        for(unsigned k=0;k<40u;++k,++e){
            m.put32(0x7457c8u+e*8u,offset);offset+=m.u16(0x7457ceu+e*8u);m.put16(0x7457ccu+e*8u,std::uint16_t(file));
        }
        (void)m.u16(0x7457ceu+e*8u);   // the unrolled loop reads the next size ahead
    }
    m.put32(0x8a8c74u,file+1u);
}

std::uint32_t ghost_save_open_416700(PcRaceContext& c,const std::array<std::uint32_t,7>& a){
    auto& m=c.m;
    std::uint32_t index;
    if(std::uint8_t(a[0])){
        std::uint32_t edx=(a[2]&0xffu)+(std::uint8_t(a[1])==0?2u:0u);
        std::uint32_t eax=(a[3]&0xffu)+edx*2u;
        eax=(a[6]&0xffu)+eax*20u;
        eax=(a[5]&0xffu)+eax*2u+0x54u;
        if(std::uint8_t(a[4]))eax+=10u;
        index=eax;
    }else index=a[6]&0xffu;
    const std::uint32_t object=m.u32(0x8a8c7cu);
    if(m.u32(object+0x130u)!=0u)return 0xffffffffu;
    Local name(m,Local416700);
    put_string(m,Local416700,format2("GHOST%02d.DAT",nullptr,m.u16(0x7457ccu+index*8u)));
    (void)ghost_save_load_423030(c,m.u32(0x8a8c7cu),0x17ff0u,Local416700);
    m.put32(0x8a8c78u,index);
    return index;
}

std::uint32_t ghost_save_load_423030(PcRaceContext& c,std::uint32_t o,std::uint32_t size,std::uint32_t name){
    auto& m=c.m;
    if(const auto old=m.u32(o+0x130u)){call(c,0x580c38u,{old});m.put32(o+0x130u,0);m.put32(o+0x134u,0);}
    m.put32(o+0x134u,size);
    m.put32(o+0x130u,call(c,0x580c33u,{size}));
    for(std::uint32_t i=0;;++i){const auto ch=m.u8(name+i);m.put8(o+i,ch);if(!ch)break;}
    if(call(c,0x406db0u,{},0,o)&0xffu){
        (void)call(c,0x406e50u,{m.u32(o+0x130u),m.u32(o+0x134u)},0,o);
        m.put8(o+0x138u,0);return 1u;
    }
    zero(m,m.u32(o+0x130u),m.u32(o+0x134u));
    if(call(c,0x406c50u,{m.u32(o+0x130u),m.u32(o+0x134u)},0,o)==0u){m.put8(o+0x138u,0);return 1u;}
    return 0u;
}

bool ghost_save_copy_416870(PcRaceContext& c,std::uint32_t index,std::uint32_t dst,std::uint32_t n){
    auto& m=c.m;const std::uint32_t o=m.u32(0x8a8c7cu);
    const std::uint32_t buffer=m.u32(o+0x130u);
    if(!buffer)return false;
    const std::uint32_t offset=m.u32(0x7457c8u+index*8u);
    if(std::int32_t(offset)<0)return false;
    std::uint32_t count=n;const std::uint32_t size=m.u32(o+0x134u);
    if(std::int32_t(offset+n)>std::int32_t(size))count=size-offset;
    copy(m,dst,buffer+offset,count);
    return true;
}

// 416930(dst, n, flag, cvt, level, kind, a7, a8, a9): the save record index as 416700 forms it
// (flag != 0) or a9, then the 416870 copy (its AL is not read by the record module).
void ghost_save_read_416930(PcRaceContext& c,const std::array<std::uint32_t,9>& a){
    std::uint32_t index;
    if(a[2]){
        const std::uint32_t edx=(a[4]&0xffu)+(a[3]==0u?2u:0u);
        std::uint32_t eax=(a[5]&0xffu)+edx*2u;
        eax=(a[8]&0xffu)+eax*20u;
        eax=(a[6]&0xffu)+eax*2u+0x54u;
        if(a[7])eax+=10u;
        index=eax;
    }else index=a[8]&0xffu;
    (void)ghost_save_copy_416870(c,index,a[0],a[1]);
}

bool ghost_save_write_4168d0(PcRaceContext& c,std::uint32_t index,std::uint32_t src,std::uint32_t n){
    auto& m=c.m;const std::uint32_t o=m.u32(0x8a8c7cu);
    const std::uint32_t buffer=m.u32(o+0x130u);
    if(!buffer)return false;
    const std::uint32_t offset=m.u32(0x7457c8u+index*8u);
    if(std::int32_t(offset)<0)return false;                                  // 423100: jl
    if(std::int32_t(offset+n)>=std::int32_t(m.u32(o+0x134u)))return false;   // jge
    copy(m,buffer+offset,src,n);
    m.put8(o+0x138u,1);return true;
}
bool ghost_save_write_4169d0(PcRaceContext& c,const std::array<std::uint32_t,9>& a){
    std::uint32_t index;
    if(a[2]){
        const std::uint32_t edx=(a[4]&0xffu)+(a[3]==0u?2u:0u);
        std::uint32_t eax=(a[5]&0xffu)+edx*2u;
        eax=(a[8]&0xffu)+eax*20u;
        eax=(a[6]&0xffu)+eax*2u+0x54u;
        if(a[7])eax+=10u;
        index=eax;
    }else index=a[8]&0xffu;
    return ghost_save_write_4168d0(c,index,a[0],a[1]);
}
bool ghost_save_flush_4167f0(PcRaceContext& c){
    auto& m=c.m;const std::uint32_t o=m.u32(0x8a8c7cu);
    const std::uint32_t buffer=m.u32(o+0x130u);
    if(!buffer)return false;
    if(m.u8(o+0x138u)&&call(c,0x406c50u,{buffer,m.u32(o+0x134u)},0,o)==0u)m.put8(o+0x138u,0);
    return true;
}
namespace {
// The ghost to beat: the loaded playback record's time ('NHOT' header) or none.
std::uint32_t ghost_best_time(PcRaceMemory& m){
    const std::uint32_t rec=m.u32(0x7f9224u);
    return m.u32(rec+0x9cacu)==0x544f484eu?m.u32(rec+0x9cb0u):0x7fffffffu;
}
// 466BE0 into a stack buffer (the name is formatted, 416700 does not take it).
void ghost_path_local(PcRaceContext& c,std::uint32_t code){Local path(c.m,Local467340);ghost_path_466be0(c,Local467340,code,1);}
std::uint32_t ghost_open_record(PcRaceContext& c){
    return ghost_save_open_416700(c,{0,0,0,0,0,0,c.m.u8(0x64bfedu)});
}
}
void ghost_save_prepare_467880(PcRaceContext& c){
    auto& m=c.m;
    if(!m.u32(0x7f92c4u)||!m.u32(0x7f92c0u))return;
    if(!m.u32(0x7f92b8u)){
        if(call(c,0x4962a0u,{})&0xffu)return;
        if(ghost_best_time(m)<=call(c,0x451180u,{1u}))return;
        const std::uint32_t code=call(c,0x48b320u,{});
        ghost_finalize_4675f0(c,std::uint16_t(code),0x7c23e0u);
        (void)call(c,0x48b320u,{});
        ghost_path_local(c,m.u8(m.u32(0x7f9228u)+0x11du));                   // 467760
        m.put32(0x7f8d80u,ghost_open_record(c));
        return;
    }
    if(!m.u32(0x7f92bcu))return;
    ghost_path_local(c,m.u8(m.u32(0x7f9224u)+0x9dc9u));
    m.put32(0x7f8d80u,ghost_open_record(c));
}
void ghost_save_store_467960(PcRaceContext& c){
    auto& m=c.m;
    if(!m.u32(0x7f92c4u)||!m.u32(0x7f92c0u))return;
    if(!m.u32(0x7f92b8u)){
        if(!(call(c,0x4962a0u,{})&0xffu)&&ghost_best_time(m)>call(c,0x451180u,{1u})){
            (void)call(c,0x48b320u,{});
            const std::uint32_t rec=m.u32(0x7f9228u);                          // 4677B0
            ghost_path_local(c,m.u8(rec+0x11du));
            (void)ghost_save_write_4168d0(c,m.u32(0x7f8d80u),rec,ghost_length_4662f0(m,rec));
            (void)ghost_save_flush_4167f0(c);
        }
        m.put32(0x7f92c4u,0);return;
    }
    if(m.u32(0x7f92bcu)){
        const std::uint32_t rec=m.u32(0x7f9224u)+0x9cacu;
        const std::uint32_t n=ghost_length_4662f0(m,rec);
        ghost_path_local(c,m.u8(rec+0x11du));
        (void)ghost_save_write_4168d0(c,m.u32(0x7f8d80u),rec,n);
        (void)ghost_save_flush_4167f0(c);
    }
    m.put32(0x7f92c4u,0);
}
void ghost_save_close_416830(PcRaceContext& c){
    auto& m=c.m;const std::uint32_t o=m.u32(0x8a8c7cu);
    call(c,0x580c38u,{m.u32(o+0x130u)});
    m.put32(o+0x134u,0);m.put32(o+0x130u,0);m.put32(0x8a8c78u,0xffffffffu);
}

// 449A80 (VM at 449A8A = mov eax,0xFFFF): CRC-16/CCITT over n bytes.
std::uint16_t ghost_crc_449a80(const PcRaceMemory& m,std::uint32_t p,std::int32_t n){
    std::uint32_t eax=0xffffu;
    if(n<=0)return std::uint16_t(eax);
    for(std::int32_t i=0;i<n;++i){
        const std::uint32_t index=(m.u8(p+std::uint32_t(i))^(eax>>8))&0xffu;
        eax=((eax&0xffu)<<8)^m.u16(0x5a27d8u+index*2u);
    }
    return std::uint16_t(eax);
}

void ghost_path_466be0(PcRaceContext& c,std::uint32_t buffer,std::uint32_t code,std::uint32_t kind){
    auto& m=c.m;const std::uint8_t al=std::uint8_t(code);
    m.put8(0x64bfedu,al);
    if(!buffer)return;
    const int v=std::int8_t(al);
    static const char* const formats[12]{"%sRecordData\\gc_TA_last_r%02d.rec","%sRecordData\\gc_TA_fast_r%02d.rec",nullptr,nullptr,
        "%sConvertGhosts\\gc_default_cvt_nml_%02d_0.rec","%sRecordData\\%02d\\default\\gc.rec","%sRecordData\\gc_default_%02d_0.rec",
        "%sGhosts\\gc_RACE_%02d.rec","%sRecordData\\%02d\\XboxLive\\gc.rec","%sRecordData\\%02d\\XboxLive\\gc.rec",
        "%sGhosts\\gc_default_cvt_%02d_0.rec","%sGhosts\\gc_default_TA_%02d.rec"};
    static constexpr bool plus_one[12]{true,true,false,false,false,true,true,false,true,true,false,true};
    const std::uint32_t k=kind&0xffu;
    if(k>11u||!formats[k])return;
    put_string(m,buffer,format2(formats[k],"\\",plus_one[k]?v+1:v));
}

void ghost_total_time_466460(PcRaceMemory& m,std::uint32_t g){
    if(m.u32(g+4u)!=0u)return;
    std::uint32_t esi=0;
    for(std::uint32_t e=0;e<15u;++e){
        const std::uint32_t row=g+8u+e*16u;
        for(std::uint32_t k=0;k<4u;++k){
            const std::uint32_t v=m.u32(row+k*4u);
            if(v==0xffffffffu){m.put32(g+4u,esi);break;}
            esi=v;
        }
    }
}

namespace {
// Walk count packets from the record frames (+120): the first is a key
// packet (0x1C full / 0x14 short), the rest deltas (0x10 / 0x08).
std::uint32_t packet_at(const PcRaceMemory& m,std::uint32_t first,std::int32_t count){
    std::uint32_t p=first;
    for(std::int32_t i=0;i<count;++i){
        if(m.u8(p)&1u)p+=i?0x10u:0x1cu;else p+=i?0x8u:0x14u;
    }
    return p;
}
// FUCOMIP const,|v| ; LAHF ; TEST AH,44h: PF clear only when equal (ordered).
bool abs_equals(float v,float limit){const double a=std::fabs(double(v));return !std::isnan(a)&&a==double(limit);}
}
void ghost_trim_467260(PcRaceMemory& m,std::uint32_t g){
    const std::int32_t count=m.i32(g+0xf8u);
    const std::uint32_t first=g+0x120u;
    const std::uint32_t last=packet_at(m,first,count-1>0?count-1:0);
    std::array<std::uint8_t,0x30> delta{};
    driving::pc_ghost_delta_decode_4668f0(driving::Bytes(delta.data(),delta.size()),m.bytes(last,0x10));
    float x,y,z;std::memcpy(&x,delta.data(),4);std::memcpy(&y,delta.data()+4,4);std::memcpy(&z,delta.data()+8,4);
    float c628114,c5b0414;{const std::uint32_t a=0x427c0000u,b=0x41f80000u;std::memcpy(&c628114,&a,4);std::memcpy(&c5b0414,&b,4);}  // 63.0, 31.0
    if(!(abs_equals(x,c628114)||abs_equals(y,c5b0414)||abs_equals(z,c628114)))return;
    const std::uint32_t previous=packet_at(m,first,count-2>0?count-2:0);
    copy(m,last,previous,0x1c);
}

void ghost_sections_4662b0(PcRaceMemory& m,std::uint32_t g,std::int32_t slot){
    copy(m,0x7f8f00u+std::uint32_t(slot)*0xf0u,g+8u,0xf0u);
}

void ghost_load_record_467340(PcRaceContext& c,std::uint32_t code,std::uint32_t slot_arg,std::uint32_t flag){
    auto& m=c.m;
    const std::uint32_t esi=call(c,0x580253u,{PcGhostRecordBytes});
    const std::uint32_t ebx=call(c,0x580253u,{0x9e00u});
    const std::int32_t slot=std::int8_t(slot_arg);
    zero(m,ebx,0x9e00u);zero(m,esi,PcGhostRecordBytes);
    m.put32(0x7f91f8u+std::uint32_t(slot)*4u,0);
    Local path(m,Local467340);
    std::uint32_t state=0;
    bool loaded=false;
    auto crc_matches=[&](std::uint32_t length){
        m.put32(ebx+0xfcu,0xffffu);
        return std::uint32_t(ghost_crc_449a80(m,ebx,std::int32_t(length)))==m.u32(esi+0xfcu);
    };
    if(ghost_save_copy_416870(c,m.u32(0x7f8d80u),esi,PcGhostRecordBytes)){
        const std::uint32_t length=m.u32(esi+0x100u);
        copy(m,ebx,esi,PcGhostRecordBytes);
        const std::uint32_t t=m.u32(esi+4u);
        if(t>time_449c10(0,0,15,0)&&t<=time_449c10(0,17,30,0)&&crc_matches(length)){state=2;loaded=true;}
    }
    bool from_file=false;
    if(!loaded){
        ghost_path_466be0(c,Local467340,code,0xb);
        auto read_file=[&](bool whole)->bool{
            const std::uint32_t f=call(c,0x4239c0u,{Local467340,0x62563cu});
            if(!f)return false;
            std::uint32_t size=PcGhostRecordBytes;
            if(whole){size=call(c,0x423f10u,{f});if(!size)return false;}
            (void)call(c,0x423cb0u,{esi,size,1u,f});
            (void)call(c,0x423bd0u,{f});
            zero(m,ebx,0x9e00u);copy(m,ebx,esi,PcGhostRecordBytes);
            return true;
        };
        if(!read_file(true))goto release;
        if(!crc_matches(m.u32(ebx+0x100u))){
            zero(m,ebx,0x9e00u);zero(m,esi,PcGhostRecordBytes);
            if(!read_file(false))goto release;
            if(!crc_matches(0x9e00u))goto release;
        }
        ghost_total_time_466460(m,esi);
        if(!(m.u32(esi+4u)>time_449c10(0,0,15,0)))goto release;
        m.put32(esi+0x104u,m.u32(0x5b0570u));m.put8(esi+0x108u,m.u8(0x5b0574u));
        m.put8(esi+0x11cu,1);
        state=1;from_file=true;
    }
    (void)from_file;
    ghost_trim_467260(m,esi);
    copy(m,m.u32(0x7f9224u)+std::uint32_t(slot)*PcGhostRecordBytes,esi,PcGhostRecordBytes);
    ghost_sections_4662b0(m,esi,slot);
    m.put32(0x7f91f8u+std::uint32_t(slot)*4u,flag?flag:state);
    // 449AC0 splits [record+4] into four stack words nobody reads.
release:
    call(c,0x580bc2u,{ebx});
    call(c,0x580bc2u,{esi});
}

void ghost_init_467ac0(PcRaceContext& c){
    auto& m=c.m;
    if(!m.u32(0x7f9228u))m.put32(0x7f9228u,call(c,0x580253u,{PcGhostRecordBytes}));
    if(!m.u32(0x7f9224u))m.put32(0x7f9224u,call(c,0x580253u,{3u*PcGhostRecordBytes}));
    const std::uint32_t work=m.u32(0x7f9228u);
    zero(m,work,PcGhostRecordBytes);zero(m,m.u32(0x7f9224u),3u*PcGhostRecordBytes);
    m.put32(0x7f91e4u,0);m.put32(0x7f91e8u,0);
    zero(m,0x7f8f00u,0xb4u*4u);
    m.put32(0x7f91ecu,0);
    m.put32(0x7f91f4u,0);m.put32(0x7f8ef8u,0);m.put8(0x7f922cu,0);
    m.put32(0x7f91d4u,0);m.put32(0x7f91d8u,0);m.put32(0x7f91dcu,0);m.put32(0x7f91e0u,0);
    m.put8(0x7f91d0u,0);m.put8(0x7f9204u,1);m.put8(0x7f9208u,0);m.put8(0x7f8ef0u,0);
    m.put32(0x7f8ef4u,0);
    m.put32(0x7f91f0u,work+0x120u);m.put32(0x7f8d84u,work+0x120u);
    m.put32(0x7f9220u,0);
    for(std::uint32_t k=0,pose=0x7f8db0u;k<3u;++k,pose+=0x78u){
        m.put32(0x7f91f8u+k*4u,0);
        pose_clear_4f6250(m,pose-0x28u);pose_clear_4f6250(m,pose);pose_clear_4f6250(m,pose+0x28u);
    }
    m.put32(0x7f92b8u,0);m.put32(0x7f92bcu,0);m.put32(0x7f92c0u,0);
    if(m.u32(0x780258u)!=7u){m.put32(0x7f92c4u,0);return;}
    const std::uint32_t code=call(c,0x48b320u,{});
    if(std::int32_t(std::int8_t(code))==m.i32(0x672df0u)){
        (void)call(c,0x466340u,{1u,0u});                 // downloaded ghost (network only)
        m.put32(0x7f92c4u,1);return;
    }
    m.put32(0x7f8d80u,ghost_save_open_416700(c,{0,0,0,0,0,0,code&0xffu}));
    m.put32(0x7f92c0u,1);m.put32(0x7f92c4u,1);
}

void ghost_load_4686c0(PcRaceContext& c){
    auto& m=c.m;
    if(!m.u32(0x7f92c0u))return;
    const std::uint32_t code=call(c,0x48b320u,{})&0xffu;
    {Local buffer(m,Local4686c0);ghost_path_466be0(c,Local4686c0,code,1);}   // formatted, unused
    ghost_load_record_467340(c,code,1,0);
    ghost_save_close_416830(c);
}

std::int8_t ghost_car_465f40(const PcRaceMemory& m,std::uint32_t i){
    const std::uint32_t base=m.u32(0x7f9224u);
    if(!base)return -1;
    const std::uint32_t p=base+i*PcGhostRecordBytes;
    if(m.u32(p)!=0x544f484eu)return -1;
    return std::int8_t(m.u8(p+0x11bu));
}
}
