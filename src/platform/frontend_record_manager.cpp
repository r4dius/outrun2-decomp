#include "frontend_record_manager.hpp"
namespace outrun::platform {
using driving::Bytes;
bool record_request_reset_435ff0(Bytes b,const RecordReleaseServices& s,unsigned& missing){
    missing=0;if(b.size()<PcRecordRequestBytes){missing=0x435ff0;return false;}
    b.put8(0x395,0); // original protected entry bridge, compared in executable oracle
    b.put8(0x394,0);b.put8(0x396,0);b.put32(0x38c,0);b.put32(0x390,0);
    for(unsigned offset:{0x24u,0x28u,0x2cu,0x18u,0x1cu,0x20u,0x30u}){
        const auto handle=b.u32(offset);if(!handle)continue;
        const unsigned pc=offset==0x30?0:0x525f80,argument=offset==0x30?1:0;
        if(!s.release||!s.release(s.user,pc,handle,argument)){missing=pc?pc:0x435ff0;return false;}
        b.put32(offset,0);
    }
    b.put32(0x14,9);b.put32(4,0);b.put32(8,0);b.put32(12,~0u);return true;
}
bool record_request_construct_435f70(Bytes b,unsigned& missing){
    if(b.size()<PcRecordRequestBytes){missing=0x435f70;return false;}
    b.put32(0,0x627ee0);b.put32(4,0);b.put32(8,0);b.put32(12,~0u);b.put32(16,0);
    for(unsigned offset=0x18;offset<=0x30;offset+=4)b.put32(offset,0);
    return record_request_reset_435ff0(b,{},missing);
}
bool record_manager_reset_4940d0(Bytes records,Bytes request,const RecordReleaseServices& s,unsigned& missing){
    if(records.size()<PcRecordManagerBytes){missing=0x4940d0;return false;}
    if(!record_request_reset_435ff0(request,s,missing))return false;
    records.put32(0x13d20,records.u32(0x13d1c));
    records.put32(0x13d1c,0);records.put32(0x13d24,0);records.put32(0x1db20,0);records.put8(0x13d34,0);
    for(unsigned i=0;i<15;++i){
        const auto r=0x1d9f4+20*i;records.put32(r,0);records.put32(r+4,0);records.put8(r+8,1);
        records.put32(0x14+64*i,~0u);records.put32(0x30+64*i,0);records.put32(0x34+64*i,0);
    }
    return true;
}
FrontendRecordManager::FrontendRecordManager(){
    record_request_construct_435f70(Bytes(request.data(),request.size()),missing_pc);
    if(!missing_pc)record_manager_reset_4940d0(Bytes(records.data(),records.size()),Bytes(request.data(),request.size()),services,missing_pc);
}
bool FrontendRecordManager::reset(){
    if(!record_manager_reset_4940d0(Bytes(records.data(),records.size()),Bytes(request.data(),request.size()),services,missing_pc))return false;
    ++reset_calls;return true;
}
}
