#include "platform/rob_motion_tables.hpp"
#include <stdexcept>
namespace outrun::platform {
namespace {
// EXE bytes (checked against the pinned EXE by rob_motion_probe).
constexpr std::uint8_t BoneName[]="\\Common\\bone.bin";                // 5DA304 (17 bytes)
constexpr std::uint8_t TableName[]="\\Common\\motdata_table.bin";      // 5DA2E8 (26 bytes)
constexpr std::uint8_t OpenMode[]="rb";                                // 62563C (3 bytes)
std::uint32_t call(const PcRaceService& s,std::uint32_t pc,std::initializer_list<std::uint32_t> args){
    PcRaceCall k{};k.pc=pc;k.argc=std::uint32_t(args.size());
    unsigned i=0;for(auto a:args)k.args[i++]=a;
    if(!s)throw std::logic_error("rob motion: no service for PC call");
    return s(k);
}
}
const PcRobMotionRdata RobMotionRdata[3]={
    {0x5da304u,BoneName,sizeof BoneName},{0x5da2e8u,TableName,sizeof TableName},{0x62563cu,OpenMode,sizeof OpenMode}};
void rob_motion_map_rdata(PcRaceMemory& m){for(const auto& t:RobMotionRdata)m.map_const(t.base,t.data,t.size);}

// CRT strrchr 581090 (repne scasb for the terminator, then a backwards
// repne scasb): the last c in the string (the terminator when c == 0), else 0.
std::uint32_t pc_crt_strrchr_581090(const PcRaceMemory& m,std::uint32_t s,std::uint8_t c){
    std::uint32_t len=0;while(m.u8(s+len))++len;
    for(std::uint32_t p=s+len;;--p){if(m.u8(p)==c)return p;if(p==s)return 0u;}
}

// 4F1F30: relocates the motion table in place (offsets -> table + offset):
// entry {name, list, count, -}, the list's count words; counts entries in
// 84DE70; stops at the entry whose name is 0.
void rob_motion_relocate_4f1f30(PcRaceMemory& m,std::uint32_t esi){
    m.put32(0x84de70u,0u);
    if(m.u32(esi)==0u)return;
    std::uint32_t eax=esi+4u,ecx;
    do{
        m.put32(0x84de70u,m.u32(0x84de70u)+1u);
        m.put32(eax-4u,m.u32(eax-4u)+esi);
        ecx=m.u32(eax);
        if(ecx!=0u){
            std::uint32_t edi=m.u32(eax+4u);
            ecx+=esi;std::uint32_t edx=0u;
            m.put32(eax,ecx);
            if(std::int32_t(edi)>0){
                do{
                    m.put32(ecx,m.u32(ecx)+esi);
                    edi=m.u32(eax+4u);                 // re-read every step (as the PC does)
                    ++edx;ecx+=4u;
                }while(std::int32_t(edx)<std::int32_t(edi));
            }
        }
        ecx=m.u32(eax+0xcu);eax+=0x10u;
    }while(ecx!=0u);
}

// 4F1F90(name, out): reads a whole file into a malloc'd buffer of size+8 whose
// last 8 bytes are the header {buffer, 0}; *out = header. Returns 1, or 0
// (out untouched) when the open fails.
std::uint32_t rob_motion_load_file_4f1f90(PcRaceMemory& m,const PcRaceService& s,std::uint32_t name,std::uint32_t out){
    const std::uint32_t handle=call(s,0x4239c0u,{name,0x62563cu});
    if(handle==0u)return 0u;
    const std::uint32_t size=call(s,0x423f10u,{handle});
    // 440D90(name): empty function
    call(s,0x440d10u,{0u});
    call(s,0x440d50u,{0u});
    const std::uint32_t buffer=call(s,0x580253u,{size+8u});
    const std::uint32_t header=buffer+size;
    m.put32(header,buffer);m.put32(header+4u,0u);m.put32(out,header);
    call(s,0x440d30u,{});
    call(s,0x440d70u,{});
    // 440D90(0): empty function
    const std::uint32_t data=m.u32(m.u32(out));
    call(s,0x423cb0u,{data,size,1u,handle});
    call(s,0x423bd0u,{handle});
    return 1u;
}

// 4F2470: RobMotion system init.
void rob_motion_init_4f2470(PcRaceMemory& m,const PcRaceService& s){
    for(std::uint32_t k=0;k<0x3fu;++k)m.put32(0x84dd68u+k*4u,0u);
    for(std::uint32_t edx=0x84d97cu;edx<0x84dd6cu;edx+=0x10u){
        m.put32(edx-4u,0u);m.put32(edx,2u);m.put32(edx+4u,0u);m.put32(edx+8u,4u);
    }
    (void)rob_motion_load_file_4f1f90(m,s,0x5da304u,0x84de68u);
    const std::uint32_t bones=m.u32(m.u32(0x84de68u));
    m.put32(0x84d974u,bones);
    (void)rob_motion_load_file_4f1f90(m,s,0x5da2e8u,0x84de6cu);
    const std::uint32_t table=m.u32(m.u32(0x84de6cu));
    m.put32(0x84d970u,table);
    rob_motion_relocate_4f1f30(m,table);
    // Names "mot_X_bin.gz" -> "mot_X_bin.sz": the byte after the last '.'.
    for(std::uint32_t esi=0;esi<0x3f0u;esi+=0x10u){
        const std::uint32_t p=pc_crt_strrchr_581090(m,m.u32(esi+m.u32(0x84d970u)),'.');
        if(p!=0u)m.put8(p+1u,0x73u);
    }
}
}
