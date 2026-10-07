// PC record module 47EC00..481180 (arcade variant 0), see race_records.hpp.
#include "platform/race_records.hpp"
#include "platform/race_ghosts.hpp"
#include "driving/pc_car_services_ghost.hpp"
#include "driving/pc_matrix_stack.hpp"
#include "driving/pc_d3dx.hpp"
#include "driving/pc_x87.hpp"
#include <cmath>
#include <cstring>
#include <limits>
#include <cstdio>
#include <stdexcept>
#include <string>
namespace outrun::platform {
namespace {
std::uint32_t call(PcRaceContext& c,std::uint32_t pc,std::initializer_list<std::uint32_t> args){
    PcRaceCall k;k.pc=pc;k.argc=std::uint32_t(args.size());
    std::size_t i=0;for(auto a:args)k.args[i++]=a;
    return c.service(k);
}
void zero(const PcRaceMemory& m,std::uint32_t p,std::uint32_t n){for(std::uint32_t k=0;k<n;k+=4)m.put32(p+k,0);}
// 480BC0's 0x100-byte stack buffer (the record path), mapped while it runs.
constexpr std::uint32_t Local480bc0=0x7ffe0400u;
struct Local {
    PcRaceMemory& m;std::size_t mark;std::array<std::uint8_t,0x100> bytes{};
    Local(PcRaceMemory& mem,std::uint32_t at):m(mem),mark(mem.mark()){m.map(at,bytes.data(),bytes.size());}
    ~Local(){m.release(mark);}
};
// .data string tables (no writer): 64E87C level, 64E888 kind.
const char* level_name(std::int32_t i){
    static const char* const t[3]{"nml","tn1","tn2"};
    if(i<0||i>2)throw std::out_of_range("record level outside 64E87C (0..2)");
    return t[i];
}
const char* kind_name(std::int32_t i){
    static const char* const t[2]{"_a","_m"};
    if(i<0||i>1)throw std::out_of_range("record kind outside 64E888 (0..1)");
    return t[i];
}
// 43F960: [78024C] (course preset) is 2 or 3.
bool preset_2_3(const PcRaceMemory& m){const std::uint32_t p=m.u32(0x78024cu);return p==2u||p==3u;}
}
void records_tables_47eca0(PcRaceContext& c){
    auto& m=c.m;
    if(!m.u32(0x81373cu))m.put32(0x81373cu,call(c,0x580253u,{PcRecordTable1Bytes}));
    std::uint32_t edx=m.u32(0x813740u);
    if(!edx){edx=call(c,0x580253u,{PcRecordTable2Bytes});m.put32(0x813740u,edx);}
    zero(m,m.u32(0x81373cu),PcRecordTable1Bytes);
    zero(m,edx,PcRecordTable2Bytes);
}
void records_request_480ad0(PcRaceContext& c,std::uint8_t level,std::uint32_t kind){
    auto& m=c.m;
    const std::int32_t ebx=std::int8_t(level);
    // 43F960 (preset 2 or 3): its result is overwritten at once.
    const std::uint8_t preset=m.u8(0x810444u);
    const std::uint32_t cvt=(preset==0u||preset==2u)?1u:0u;
    const std::uint8_t k=std::uint8_t(kind);
    m.put8(0x813738u,k);
    m.put8(0x813730u,std::uint8_t(cvt));
    m.put8(0x810450u,0);
    m.put8(0x80fb45u,std::uint8_t(ebx));
    m.put8(0x80fb44u,0);
    m.put32(0x813734u,0);
    // 5802DD sprintf(local, "%s\gc_default_%s_%s%s_%02d_%d%s.rec", "\RecordData", cvt ? "cvt" :
    // "old", [64E87C + level*4], [64E888 + kind*4], 0, 0, ""): the buffer is not used again
    // (and those .data string tables have no writer), so nothing of it is reproduced.
    const std::uint32_t slot=call(c,0x416700u,{1u,m.u8(0x813730u)?1u:0u,m.u8(0x80fb45u),m.u8(0x813738u),
        m.u32(0x813734u)?1u:0u,m.u8(0x80fb44u),m.u8(0x810450u)});
    m.put32(0x80fb40u,slot);
}
void records_init_480fe0(PcRaceContext& c,std::uint32_t a,std::uint32_t b,std::uint32_t k){
    auto& m=c.m;
    // 440D90 (push ebp / mov ebp,esp / pop ebp / ret) around each step: nothing.
    records_tables_47eca0(c);
    zero(m,0x80fb48u,0x242u*4u);
    const std::uint8_t preset=m.u8(0x78024cu);
    m.put32(0x810128u,2u);
    for(std::uint32_t at:{0x81043cu,0x8101bcu,0x8101c0u,0x810258u,0x81025cu,0x8102f4u,0x8102f8u,0x810390u,0x810394u,
            0x81042cu,0x810430u,0x810438u,0x810124u})m.put32(at,0);
    m.put8(0x810444u,preset);
    m.put8(0x81044cu,0);
    m.put8(0x810446u,std::uint8_t(b));
    m.put8(0x810445u,std::uint8_t(a));
    m.put8(0x810447u,std::uint8_t(k));
    records_request_480ad0(c,std::uint8_t(b),k);
}
// 480130(buf, level, kind, half, media) (EBX = record index): the request bytes 80FB45 /
// 813738 / 80FB44 / 810450 / 813730 / 813734 and the record path
// "<\Media|\RecordData>\gc_default_<cvt|old>_<level><kind or "">_%02d_%d<r or "">.rec".
void records_path_480130(PcRaceContext& c,std::uint32_t buf,std::uint32_t level,std::uint32_t kind,std::uint32_t half,
    std::uint32_t media,std::uint32_t index){
    auto& m=c.m;
    if(std::uint8_t(media)==1u&&std::int32_t(index)>0)level=0;
    const std::uint32_t r=(preset_2_3(m)&&std::int32_t(index)>=0xa&&std::int32_t(index)<=0xe)?1u:0u;
    const std::uint8_t preset=m.u8(0x810444u);
    const bool cvt=preset==0u||preset==2u;
    m.put8(0x80fb45u,std::uint8_t(level));
    m.put8(0x810450u,std::uint8_t(index));
    m.put8(0x813738u,std::uint8_t(kind));
    m.put8(0x80fb44u,std::uint8_t(half));
    m.put8(0x813730u,cvt?1u:0u);
    m.put32(0x813734u,r);
    const char* suffix=r?"r":"";
    const char* k=std::uint8_t(media)==0u?kind_name(std::int8_t(kind)):"";
    char text[0x100];
    const int n=std::snprintf(text,sizeof text,"%s\\gc_default_%s_%s%s_%02d_%d%s.rec",std::uint8_t(media)?"\\Media":"\\RecordData",
        cvt?"cvt":"old",level_name(std::int32_t(level)),k,int(index),int(std::int8_t(half)),suffix);
    if(n<0||n>=int(sizeof text))throw std::length_error("480130 record path longer than the PC buffer");
    for(int i=0;i<=n;++i)m.put8(buf+std::uint32_t(i),std::uint8_t(text[i]));
}
namespace {
// 449A80 over the record with +26 (its stored CRC) replaced by FFFF, +26 restored: equal?
bool crc_ok(PcRaceContext& c,std::uint32_t rec,std::int32_t n){
    auto& m=c.m;
    const std::uint16_t saved=m.u16(rec+0x26u);
    m.put16(rec+0x26u,0xffffu);
    const std::uint16_t crc=ghost_crc_449a80(m,rec,n);
    m.put16(rec+0x26u,saved);
    return crc==saved;
}
}
// 480BC0(rec, index, level, kind, half) (protected head: sub esp,0x100): the record cleared, then
// copied from the save slot (416930) and kept when it is a QHOT record (+24 = 0x132) whose CRC
// over +C + 0x40 bytes holds; otherwise read from the retail "\Media\gc_default_..." file
// (4239C0 / 423F10 / 423CB0 / 423BD0) with its CRC over 0xFD4 bytes; else +0 = 0.
void records_load_480bc0(PcRaceContext& c,std::uint32_t rec,std::uint32_t index,std::uint32_t level,std::uint32_t kind,std::uint32_t half){
    auto& m=c.m;
    Local local(m,Local480bc0);
    zero(m,rec,PcRecordBytes);
    m.put32(rec,0);
    records_path_480130(c,Local480bc0,std::uint32_t(std::int32_t(std::int8_t(level))),kind,half,0u,index);
    PcRaceCall k;k.pc=0x416930u;k.argc=9;
    k.args[0]=rec;k.args[1]=PcRecordBytes;k.args[2]=1u;k.args[3]=std::uint32_t(std::int32_t(m.i8(0x813730u)));
    k.args[4]=m.u8(0x80fb45u);k.args[5]=m.u8(0x813738u);k.args[6]=half;k.args[7]=m.u32(0x813734u);k.args[8]=index;
    (void)c.service(k);
    if(m.u32(rec)==0x544f4851u&&m.u16(rec+0x24u)==0x132u){
        if(crc_ok(c,rec,std::int32_t(m.u32(rec+0xcu)+0x40u)))return;
        m.put32(rec,0);
    }
    records_path_480130(c,Local480bc0,0u,0u,half,1u,index);
    zero(m,rec,PcRecordBytes);
    const std::uint32_t f=call(c,0x4239c0u,{Local480bc0,0x62563cu});
    if(f){
        const std::uint32_t size=call(c,0x423f10u,{f});
        (void)call(c,0x423cb0u,{rec,size,1u,f});
        (void)call(c,0x423bd0u,{f});
        if(crc_ok(c,rec,std::int32_t(PcRecordBytes)))return;
    }
    m.put32(rec,0);
}
// 4810A0(level) (BL = kind): the 15 record pairs of [813740] (480BC0 halves 0 / 1), then 416830.
void records_load_4810a0(PcRaceContext& c,std::uint32_t level,std::uint32_t ebx){
    auto& m=c.m;
    for(std::uint32_t esi=0,edi=0;std::int32_t(esi)<std::int32_t(PcRecordTable2Bytes);esi+=2u*PcRecordBytes,++edi){
        records_load_480bc0(c,m.u32(0x813740u)+esi,edi,level,ebx,0u);
        records_load_480bc0(c,m.u32(0x813740u)+esi+PcRecordBytes,edi,level,ebx,1u);
    }
    (void)call(c,0x416830u,{});
}
// 481230(a, b, c) (scene owner 49BA80 stage 7, variant 0): 4810A0(b) with BL = c.
void records_load_481230(PcRaceContext& c,std::uint32_t,std::uint32_t b,std::uint32_t k){records_load_4810a0(c,b,k);}
// ==== record ghost car playback (480220 and its helpers) =========================================
namespace {
using driving::X87;
float fbits(std::uint32_t u){float f;std::memcpy(&f,&u,4);return f;}
const float KAngle=fbits(0x38c90fdbu),KAngleInv=fbits(0x4622f983u);   // 628254, 6282C0
const float KQuarter=fbits(0x3e800000u),KHalf=fbits(0x3f000000u),KTwo=fbits(0x40000000u),KOne=fbits(0x3f800000u);
const float KTenth=fbits(0x3dcccccdu),KEps=fbits(0x38d1b717u);         // 62813C, 5A29E0
const float KScale=fbits(0x39800000u),K1000=fbits(0x447a0000u),K100=fbits(0x42c80000u);   // 62818C, 5A29EC, 6282CC
std::int32_t cvtt(float v){
    if(!(v>-2147483904.0f&&v<2147483648.0f))return std::numeric_limits<std::int32_t>::min();
    return std::int32_t(v);
}
bool jbe(float a,float b){return std::isnan(a)||std::isnan(b)||a<=b;}   // COMISS a,b; JBE
// 4493A0 (FILD) * 628254, rounded by FSTP to a float.
float word_radians(std::uint16_t w){return driving::x87_float(X87(std::int32_t(std::int16_t(w)))*X87(KAngle));}
std::int16_t be16(const PcRaceMemory& m,std::uint32_t p){return std::int16_t(std::uint16_t((m.u8(p)<<8)|m.u8(p+1)));}
// 40F140(a, b): x87 |a - b|.
X87 distance_40f140(const PcRaceMemory& m,std::uint32_t a,std::uint32_t b){
    const X87 dx=X87(m.f32(a))-X87(m.f32(b)),dy=X87(m.f32(a+4))-X87(m.f32(b+4)),dz=X87(m.f32(a+8))-X87(m.f32(b+8));
    return driving::x87_sqrt((dz*dz+dx*dx)+dy*dy);
}
// 480220's frame: E = ESP after its register pushes (0x44 locals, then the args at +0x58).
constexpr std::uint32_t Local480220=0x7ffe2000u;
struct Frame {
    std::array<std::uint8_t,0x70> bytes{};
    PcRaceMemory& m;std::size_t mark;
    explicit Frame(PcRaceMemory& mm):m(mm),mark(mm.mark()){m.map(Local480220,bytes.data(),bytes.size());}
    ~Frame(){m.release(mark);}
};
std::uint32_t E(std::uint32_t o){return Local480220+o;}
// 409F90(44BED0(flag)) + 40A7D0(out, in): the sample point in the selected area matrix (pushed, not popped).
void area_point(PcRaceContext& c,std::uint32_t flag,std::uint32_t out,std::uint32_t in){
    auto& m=c.m;
    driving::pc_matrix_push_load(c.matrices,m.bytes(flag?0x7d3190u:0x7d2da0u,0x40));
    const auto p=driving::pc_matrix_point(c.matrices,{m.f32(in),m.f32(in+4),m.f32(in+8)});
    m.putf(out,p.x);m.putf(out+4,p.y);m.putf(out+8,p.z);
}
// 40A410 / 40A3E0 / 40A440 by the sample's words +22 / +20 / +24, then 449640(out, NULL), 40A010.
void sample_angles(PcRaceContext& c,std::uint32_t sample,std::uint32_t out){
    auto& m=c.m;
    driving::pc_matrix_rotate_y(c.matrices,word_radians(m.u16(sample+0x22u)));
    driving::pc_matrix_rotate_x(c.matrices,word_radians(m.u16(sample+0x20u)));
    driving::pc_matrix_rotate_z(c.matrices,word_radians(m.u16(sample+0x24u)));
    std::array<std::uint8_t,0x40> cur{};driving::pc_matrix_get(c.matrices,driving::Bytes(cur.data(),cur.size()));
    const auto a=driving::pc_matrix_angles_449640(driving::Bytes(cur.data(),cur.size()));
    m.putf(out,a[0]);m.putf(out+4,a[1]);m.putf(out+8,a[2]);
    driving::pc_matrix_pop(c.matrices);
}
// 4493B0 x3 of the 449640 angles * 6282C0: the words x, y, z.
void angle_words(PcRaceMemory& m,std::uint32_t angles,std::uint32_t wx,std::uint32_t wy,std::uint32_t wz){
    m.put16(wx,std::uint16_t(cvtt(m.f32(angles)*KAngleInv)));
    m.put16(wy,std::uint16_t(cvtt(m.f32(angles+4)*KAngleInv)));
    m.put16(wz,std::uint16_t(cvtt(m.f32(angles+8)*KAngleInv)));
}
}
// 47F930(EAX = cursor, ESI = stream, EDI = out sample, [esp+4] = previous sample or 0): one
// packed record sample (big-endian fields, 0x80 = unchanged); returns the bytes consumed.
std::uint32_t records_sample_47f930(PcRaceMemory& m,std::uint32_t cursor,std::uint32_t stream,std::uint32_t out,std::uint32_t ref){
    std::uint32_t eax=cursor;
    auto vec=[&](std::uint32_t o,float k2){
        if(m.u8(stream+eax)==0x80u){
            if(ref)m.put32(out+o,m.u32(ref+o));else m.putf(out+o,0.0f);
            ++eax;
        }else{
            m.putf(out+o,float(be16(m,stream+eax))*KScale*k2);
            eax+=2;
        }
    };
    vec(0,K1000);vec(4,K100);vec(8,K1000);
    const std::uint8_t dl=m.u8(stream+eax);++eax;
    auto word=[&](std::uint8_t bit,std::uint32_t o){
        if(dl&bit){m.put16(out+o,ref?m.u16(ref+o):0u);}
        else{m.put16(out+o,std::uint16_t(m.u8(stream+eax)<<8));++eax;}
    };
    word(4,0x20);word(8,0x22);word(0x10,0x24);word(2,0x2c);
    m.put32(out+0x1cu,(std::uint32_t(std::uint8_t(~dl))>>5)&1u);
    m.put8(out+0x2eu,std::uint8_t((dl>>6)&1u));
    if(dl&1u){
        if(m.u8(stream+eax)==0xffu)eax-=1u;   // [out+18] kept; the next fields start one byte back
        else{
            const std::uint32_t b=(std::uint32_t(m.u8(stream+eax))<<24)|(std::uint32_t(m.u8(stream+eax+1))<<16)|
                (std::uint32_t(m.u8(stream+eax+2))<<8)|m.u8(stream+eax+3);
            m.put32(out+0x18u,b);eax+=4u;
        }
        m.putf(out+0xcu,float(be16(m,stream+eax))*KScale);eax+=2u;
        m.putf(out+0x10u,float(be16(m,stream+eax))*KScale);eax+=2u;
        m.putf(out+0x14u,float(be16(m,stream+eax))*KScale);eax+=2u;
        m.put16(out+0x26u,std::uint16_t(m.u8(stream+eax)<<8));
        m.put16(out+0x28u,std::uint16_t(m.u8(stream+eax+1)<<8));
        m.put16(out+0x2au,std::uint16_t(m.u8(stream+eax+2)<<8));
        eax+=3u;
    }else{
        m.put16(out+0x2au,0);m.put16(out+0x28u,0);m.put16(out+0x26u,0);
        m.putf(out+0x18u,0.0f);m.putf(out+0x14u,0.0f);m.putf(out+0x10u,0.0f);m.putf(out+0xcu,0.0f);
    }
    return eax-cursor;
}
// 47EF90(t, out) (ESI = current, EDI = next, EBX = previous): the quadratic Bezier from current
// to next whose control point leans on the previous step ((current + next) / 2 below 1e-4).
void records_bezier_47ef90(PcRaceMemory& m,float t,std::uint32_t out,std::uint32_t cur,std::uint32_t next,std::uint32_t prev){
    const X87 d=distance_40f140(m,prev,cur);
    float x0,x1,x2;
    if(d>X87(KEps)){
        x0=m.f32(cur)-m.f32(prev);x1=m.f32(cur+4)-m.f32(prev+4);
        x0=x0+m.f32(next);x1=x1+m.f32(next+4);
        x2=m.f32(cur+8)-m.f32(prev+8);x2=x2+m.f32(next+8);
        x0=x0-m.f32(cur);x1=x1-m.f32(cur+4);x2=x2-m.f32(cur+8);
        x0=x0*KQuarter;x0=x0+m.f32(cur);
        x1=x1*KQuarter;x1=x1+m.f32(cur+4);
        x2=x2*KQuarter;x2=x2+m.f32(cur+8);
    }else{
        x2=m.f32(next+8);
        x0=m.f32(cur)+m.f32(next);x1=m.f32(cur+4)+m.f32(next+4);x2=x2+m.f32(cur+8);
        x0=x0*KHalf;x1=x1*KHalf;x2=x2*KHalf;
    }
    const float u=KOne-t;
    float x6=m.f32(cur)*u;x0=x0*t;x0=x0*KTwo;x6=x6+x0;
    float y=m.f32(next)*t;y=y*t;
    x1=x1*t;x1=x1*KTwo;
    x6=x6*u;x6=x6+y;m.putf(out,x6);
    float r=u*m.f32(cur+4);r=r+x1;r=r*u;
    float q=t*t;q=q*m.f32(next+4);r=r+q;m.putf(out+4,r);
    float z1=m.f32(next+8);
    float z=u*m.f32(cur+8);
    x2=x2*t;x2=x2*KTwo;z=z+x2;
    z1=z1*t;z=z*u;z1=z1*t;z=z+z1;m.putf(out+8,z);
}
// 4800F0(EAX = next words, ECX = out, EDX = current words, XMM0 = t): three angle words.
void records_words_4800f0(PcRaceMemory& m,std::uint32_t next,std::uint32_t out,std::uint32_t cur,float t){
    for(std::uint32_t k=0;k<6u;k+=2){
        const std::uint32_t a=m.u16(cur+k),b=m.u16(next+k);
        const std::int32_t d=std::int16_t(std::uint16_t(b-a));
        m.put16(out+k,std::uint16_t(std::uint32_t(cvtt(float(d)*t))+a));
    }
}
// 47F0E0(a, b, t, out): out = a + trunc((short)(b - a) * t).
void records_word_47f0e0(PcRaceMemory& m,std::uint32_t a,std::uint32_t b,float t,std::uint32_t out){
    const std::int32_t d=std::int16_t(std::uint16_t(b-a));
    m.put16(out,std::uint16_t(std::uint32_t(cvtt(float(d)*t))+a));
}
// 47ED00(EAX = slot): the slot's record pointer cleared and, when its event 9+slot runs, the
// +C5C bits 0x300000 of the event's work.
void records_drop_47ed00(PcRaceMemory& m,std::uint32_t slot){
    const std::uint32_t ev=slot+9u;
    m.put32(slot*0xa4u+0x80fbd8u,0);
    if(m.u8(ev+0x79fb48u)&3u){const std::uint32_t w=m.u32(ev*0x3cu+0x799b38u);m.put32(w+0xc5cu,m.u32(w+0xc5cu)&0xffcfffffu);}
}
// 47F140(slot): 0 no QHOT record, 3 finished (+A0), 1 cursor 0 (+94), else 2.
std::uint32_t records_state_47f140(const PcRaceMemory& m,std::uint32_t slot){
    const std::uint32_t s=slot*0xa4u+0x80fb48u,r=m.u32(s+0x90u);
    if(!r||m.u32(r)!=0x544f4851u||m.u16(r+0x24u)!=0x132u)return 0u;
    if(m.u32(s+0xa0u))return 3u;
    return m.u32(s+0x94u)?2u:1u;
}
// 47F1A0(slot): the slot's frame counter +98 for a QHOT record with +32 = 1, else [81043C].
std::uint32_t records_frame_47f1a0(const PcRaceMemory& m,std::uint32_t slot){
    const std::uint32_t s=slot*0xa4u+0x80fb48u,r=m.u32(s+0x90u);
    if(r&&m.u32(r)==0x544f4851u&&m.u16(r+0x24u)==0x132u&&m.u8(r+0x32u)==1u)return m.u32(s+0x98u);
    return m.u32(0x81043cu);
}
// 480220(work, slot, frame) (protected head: EBP = slot*0xA4 + 80FB48): one record car frame;
// EAX = 1 on every path.
std::uint32_t records_play_480220(PcRaceContext& c,std::uint32_t work,std::uint32_t slot,std::uint32_t frame){
    auto& m=c.m;
    const std::uint32_t ebp=slot*0xa4u+0x80fb48u;
    const std::uint32_t rec=m.u32(ebp+0x90u),player=m.u32(0x799d18u);
    if(!rec||m.u32(rec)!=0x544f4851u||m.i32(rec+0xcu)<=0)return 1u;
    if(m.u8(rec+0x32u)==0u&&call(c,0x450130u,{})!=0u&&
       std::int32_t(m.i8(m.u32(ebp+0x90u)+0x2fu))!=m.i32(player+0x68u)){records_drop_47ed00(m,slot);return 1u;}
    if(frame==0u)m.put32(ebp+0x94u,0);
    Frame frame_bytes(m);
    m.put32(E(0x10),player);m.put32(E(0x58),work);m.put32(E(0x5c),rec);m.put32(E(0x60),frame);
    for(std::uint32_t k=0;k<12u;k+=4)m.put32(work+0x16cu+k,m.u32(work+0x14u+k));
    m.put16(work+0x17cu,m.u16(work+0x2cu));m.put16(work+0x17eu,m.u16(work+0x2eu));m.put16(work+0x180u,m.u16(work+0x30u));
    for(std::uint32_t k=0;k<12u;k+=4)m.put32(work+0x1040u+k,m.u32(work+0x2d8u+k));
    for(std::uint32_t k=0;k<12u;k+=4)m.put32(work+0x1034u+k,m.u32(work+0x2e4u+k));
    m.put16(work+0xc2eu,m.u16(work+0xc2cu));
    if(m.u8(rec+0x32u)&&m.u8(0x81044cu)!=1u)return 1u;
    m.put32(work+4u,m.u32(work+4u)^((m.u32(player+4u)^m.u32(work+4u))&0x3000000u));
    const std::int32_t level=m.i8(rec+0x2fu);
    m.put8(0x813748u,1);
    const std::uint32_t n=std::uint32_t(std::int32_t(m.i8(rec+0x33u)));
    bool interpolate=m.i32(player+0x68u)==level&&m.u32(ebp+0xa0u)==0u;
    if(interpolate){
        if(n==0u)throw std::domain_error("480220: record sample interval 0 (DIV)");
        interpolate=frame%n!=0u;
    }
    if(interpolate){
        const std::uint32_t r=frame%n;
        m.put32(E(0x5c),n);m.put32(E(0x1c),r);
        X87 q=X87(std::int32_t(r));
        if(std::int32_t(r)<0)q=q+X87(4294967296.0f);
        q=q/X87(std::int32_t(n));
        const float t=driving::x87_float(q);m.putf(E(0x5c),t);
        // the next sample's point and angles
        area_point(c,m.u32(ebp+0x7cu),E(0x30),ebp+0x60u);
        sample_angles(c,ebp+0x60u,E(0x24));
        angle_words(m,E(0x24),E(0x1c),E(0x1e),E(0x20));
        // the current sample's
        area_point(c,m.u32(ebp+0x4cu),E(0x3c),ebp+0x30u);
        sample_angles(c,ebp+0x30u,E(0x24));
        angle_words(m,E(0x24),E(0x14),E(0x16),E(0x18));
        // the previous sample's point
        area_point(c,m.u32(ebp+0x1cu),E(0x48),ebp);
        driving::pc_matrix_pop(c.matrices);
        records_bezier_47ef90(m,t,work+0x14u,E(0x3c),E(0x30),E(0x48));
        records_words_4800f0(m,E(0x1c),work+0x2cu,E(0x14),t);
        records_word_47f0e0(m,m.u16(ebp+0x5cu),m.u16(ebp+0x8cu),t,work+0x32u);
        float v=KOne-t;v=v*m.f32(ebp+0x48u);
        float w1=m.f32(ebp+0x78u)*t;v=v+w1;
        m.putf(work+0x2c8u,v);
        if(!jbe(KTenth,v))m.putf(work+0x2c8u,0.0f);
        records_bezier_47ef90(m,t,work+0x2d8u,ebp+0x3cu,ebp+0x6cu,ebp+0xcu);
        records_words_4800f0(m,ebp+0x86u,E(0x1c),ebp+0x56u,t);
        m.putf(work+0x2e4u,word_radians(m.u16(E(0x1c))));
        m.putf(work+0x2e8u,word_radians(m.u16(E(0x1e))));
        m.putf(work+0x2ecu,word_radians(m.u16(E(0x20))));
        m.put32(work+0x38u,std::uint32_t(std::int32_t(m.i8(ebp+0x5eu))));
    }else{
        const std::uint32_t cursor=m.u32(ebp+0x94u);
        for(std::uint32_t k=0;k<0x30u;k+=4)m.put32(ebp+k,m.u32(ebp+0x30u+k));
        const std::uint32_t stream=rec+0x34u;
        m.put32(E(0x1c),stream);
        const std::uint32_t used=records_sample_47f930(m,cursor,stream,ebp+0x30u,cursor?ebp:0u);
        m.put32(E(0x14),used);
        if(!(m.i32(ebp+0x94u)>0))for(std::uint32_t k=0;k<0x30u;k+=4)m.put32(ebp+k,m.u32(ebp+0x30u+k));
        area_point(c,m.u32(ebp+0x4cu),work+0x14u,ebp+0x30u);
        sample_angles(c,ebp+0x30u,E(0x24));
        angle_words(m,E(0x24),work+0x2cu,work+0x2eu,work+0x30u);
        m.put32(work+0x2c8u,m.u32(ebp+0x48u));
        m.put32(work+0x38u,std::uint32_t(std::int32_t(m.i8(ebp+0x5eu))));
        if(!jbe(m.f32(work+0x2c8u),0.0f)){
            for(std::uint32_t k=0;k<12u;k+=4)m.put32(work+0x2d8u+k,m.u32(ebp+0x3cu+k));
            m.putf(work+0x2e4u,word_radians(m.u16(ebp+0x56u)));
            m.putf(work+0x2e8u,word_radians(m.u16(ebp+0x58u)));
            m.putf(work+0x2ecu,word_radians(m.u16(ebp+0x5au)));
        }
        if(m.u32(ebp+0xa0u)==0u){
            const std::uint32_t at=m.u32(ebp+0x94u),next=at+used,length=m.u32(rec+0xcu);
            if(std::int32_t(next)>=std::int32_t(length))m.put32(ebp+0xa0u,1u);
            else if(std::int32_t(m.i8(rec+0x2fu))==m.i32(player+0x68u)&&std::int32_t(at)<std::int32_t(length)){
                m.put32(ebp+0x94u,next);
                (void)records_sample_47f930(m,next,stream,ebp+0x60u,ebp+0x30u);
            }
        }
    }
    // 4808BA: the wheel spin by the distance run, the record bit 0x80000000, the course length.
    m.putf(E(0x5c),driving::x87_float(distance_40f140(m,work+0x14u,work+0x16cu)));
    const std::uint32_t model=0x650500u+std::uint32_t(std::int32_t(m.i8(work+0x11u)))*0x44u;   // 4866C0
    {   X87 r=X87(m.f32(E(0x5c)))/X87(m.f32(model+8u));r=r*X87(KAngleInv);
        m.put16(work+0x40u,std::uint16_t(m.u16(work+0x40u)+std::uint16_t(cvtt(driving::x87_float(r)))));}
    {   X87 r=X87(m.f32(E(0x5c)))/X87(m.f32(model+8u));r=r*X87(KAngleInv);
        m.put16(work+0x44u,std::uint16_t(m.u16(work+0x44u)+std::uint16_t(cvtt(driving::x87_float(r)))));}
    std::uint32_t f=m.u32(work+4u)&0x7fffffffu;
    if(!jbe(m.f32(work+0x2c8u),0.0f))f|=0x80000000u;
    m.put32(work+4u,f);
    const std::uint32_t length=call(c,0x43f370u,{work+0x14u});
    m.put16(work+0x262u,m.u16(work+0x260u));
    m.put16(work+0x64u,std::uint16_t(length));
    m.put16(work+0x260u,std::uint16_t(m.u16(player+0x25eu)+std::uint16_t(length)));
    if(m.u8(m.u32(ebp+0x90u)+0x32u))m.put32(ebp+0x98u,m.u32(ebp+0x98u)+1u);
    return 1u;
}
// 47F260(event, slot, model, rec): a QHOT record bound to the slot (+9C event, +90 record, cursor
// +94 and finished +A0 cleared; a running event's work gets +C5C bits (slot%2 + 1) << 20 and
// +11 / +12 from the record +28 / +31); anything else drops the slot (47ED00's body).
void records_bind_47f260(PcRaceMemory& m,std::uint32_t event,std::uint32_t slot,std::uint32_t,std::uint32_t rec){
    if(m.u32(rec)!=0x544f4851u){records_drop_47ed00(m,slot);return;}
    const std::uint32_t work=m.u32(event*0x3cu+0x799b38u),s=slot*0xa4u+0x80fb48u;
    m.put32(s+0x9cu,event);m.put32(s+0x90u,rec);m.put32(s+0x94u,0);m.put32(s+0xa0u,0);
    if(!(m.u8(event+0x79fb48u)&3u))return;
    const std::uint32_t half=std::uint32_t(std::int32_t(slot)%2+1);
    m.put32(work+0xc5cu,m.u32(work+0xc5cu)^(((half<<20)^m.u32(work+0xc5cu))&0x300000u));
    m.put8(work+0x11u,m.u8(rec+0x28u));
    m.put8(work+0x12u,m.u8(rec+0x31u));
}
// 47FF60: during the countdown, slots 0 / 1 (events 9 / 10) take the first record pair and slots
// 2 / 3 are dropped; then, on a new route decision (451350 != 2 while [810128] == 2), the stage's
// record +30 takes it and two free events among 9..12 take the next stage's pair (450250).
void records_schedule_47ff60(PcRaceContext& c){
    auto& m=c.m;
    const std::uint32_t player=m.u32(0x799d18u);
    const std::uint32_t stage=call(c,0x44c940u,{m.u32(player+0x68u)});
    if(std::int16_t(call(c,0x49b2d0u,{}))>0x3c){
        const std::uint32_t t=m.u32(0x813740u);
        records_bind_47f260(m,9u,0u,m.u8(player+0x11u),t);
        records_bind_47f260(m,0xau,1u,m.u8(player+0x11u),t+PcRecordBytes);
        m.put32(0x80fd20u,0);
        if(m.u8(0x79fb53u)&3u){const std::uint32_t w=m.u32(0x799dccu);m.put32(w+0xc5cu,m.u32(w+0xc5cu)&0xffcfffffu);}
        m.put32(0x80fdc4u,0);
        if(m.u8(0x79fb54u)&3u){const std::uint32_t w=m.u32(0x799e08u);m.put32(w+0xc5cu,m.u32(w+0xc5cu)&0xffcfffffu);}
    }
    const std::uint32_t route=call(c,0x451350u,{stage});
    if(m.u32(0x810128u)!=2u||route==2u){m.put32(0x810128u,route);return;}
    if(route==0u)m.put8(m.u32(0x81373cu)+stage*PcRecordBytes+0x30u,0);
    else if(route==1u)m.put8(m.u32(0x81373cu)+stage*PcRecordBytes+0x30u,1);
    const std::uint32_t next=call(c,0x450250u,{m.u32(player+0x68u),route});
    std::uint32_t taken=0;
    for(std::uint32_t i=0;i<4u;++i){
        const std::uint32_t work=m.u32(0x799d54u+i*0x3cu);
        if(m.u32(work+0xc5cu)&0x300000u)continue;
        const std::uint32_t rec=(taken+next*2u)*PcRecordBytes+m.u32(0x813740u);
        records_bind_47f260(m,i+9u,i,m.u8(player+0x11u),rec);
        ++taken;
        if(taken==2u)break;
        if(stage==call(c,0x450780u,{})-1u)break;
    }
    m.put32(0x810128u,route);
}
// 47FD40: the five "+32" record players 81012C + i*0x9C ({previous, current, next} samples, +90
// frame, +94 cursor) of records 21 + 2i: one of the player's stage takes a free event 9..12
// (810124 / 810438) until its time +10 (81044C = 1, or 2 when 60 frames late); every
// started one decodes a sample each +33 frames.
void records_advance_47fd40(PcRaceContext& c){
    auto& m=c.m;
    if(m.u8(0x81044cu))return;
    const std::uint32_t player=m.u32(0x799d18u);
    const std::uint32_t stage=call(c,0x44c940u,{m.u32(player+0x68u)});
    std::uint32_t ebx=m.u32(0x810124u);
    bool advance=false;
    if(stage!=call(c,0x450780u,{})){advance=ebx==0u;}
    else if(ebx==0u){
        std::uint32_t rec=m.u32(0x813740u)+21u*PcRecordBytes,state=0x81012cu;
        bool found=false;
        for(;state<0x810438u;state+=0x9cu,rec+=2u*PcRecordBytes){
            if(rec&&m.u32(rec)==0x544f4851u&&m.u16(rec+0x24u)==0x132u&&m.u8(rec+0x32u)&&
               std::int32_t(m.i8(rec+0x2fu))==m.i32(player+0x68u)){found=true;break;}
        }
        if(found){
            if(m.u32(0x810440u)>m.u32(rec+0x10u)+0x3cu){m.put8(0x81044cu,2);return;}
            std::uint32_t i=0,work=0;bool free=false;
            for(;i<4u;++i){work=m.u32(0x799d54u+i*0x3cu);if(!(m.u32(work+0xc5cu)&0x300000u)){free=true;break;}}
            if(free){
                ebx=i*0xa4u+0x80fb48u;
                records_bind_47f260(m,i+9u,i,std::uint32_t(std::int32_t(m.i8(rec+0x28u))),rec);
                m.put32(ebx+0x94u,m.u32(state+0x94u));
                m.put32(ebx+0x98u,m.u32(state+0x90u));
                for(std::uint32_t k=0;k<0x90u;k+=4)m.put32(ebx+k,m.u32(state+k));
                m.put32(0x810124u,ebx);
                m.put32(0x810438u,work);
            }else advance=true;
        }else advance=true;
        if(!advance&&ebx==0u)advance=true;
    }
    if(!advance){
        if(m.u32(0x810440u)<m.u32(m.u32(ebx+0x90u)+0x10u))return;
        m.put8(0x81044cu,1);return;
    }
    for(std::uint32_t b=0x8101c0u,r=m.u32(0x813740u)+21u*PcRecordBytes;b<0x8104ccu;b+=0x9cu,r+=2u*PcRecordBytes){
        if(m.u32(0x810440u)<m.u32(r+0x10u))continue;
        if(m.u32(r)!=0x544f4851u||m.u16(r+0x24u)!=0x132u||!m.u8(r+0x32u))continue;
        if(!(m.i32(b)<m.i32(r+0xcu)))continue;
        const std::int32_t n=m.i8(r+0x33u);
        if(n==0)throw std::domain_error("47FD40: record sample interval 0 (IDIV)");
        if(m.i32(b-4u)%n==0){
            for(std::uint32_t k=0;k<0x30u;k+=4)m.put32(b-0x94u+k,m.u32(b-0x64u+k));
            const std::uint32_t used=records_sample_47f930(m,m.u32(b),r+0x34u,b-0x64u,0u);
            m.put32(b,m.u32(b)+used);
            if(m.i32(b)<m.i32(r+0xcu))(void)records_sample_47f930(m,m.u32(b),r+0x34u,b-0x34u,b-0x64u);
        }
        m.put32(b-4u,m.u32(b-4u)+1u);
    }
}
// 47ED90 (during the countdown): the encoder state 81010C reset and the headers of the records
// 0..450780() (+24 = 0x132, +28 / +31 / +2A the player's model bytes, +29 = 48B1A0, +33 = 16).
void records_restart_47ed90(PcRaceContext& c){
    auto& m=c.m;
    const std::uint32_t player=m.u32(0x799d18u);
    m.put8(0x810445u,m.u8(player+0x11u));
    for(std::uint32_t a:{0x81011cu,0x810118u,0x810114u})m.put32(a,0xc7c34fffu);   // 5B4488
    m.put8(0x810121u,0);m.put8(0x810123u,0);m.put32(0x810110u,0);m.put32(0x81010cu,1u);
    std::uint32_t k=0,off=0;
    if(std::int32_t(call(c,0x450780u,{}))>=0){
        do{
            const std::uint32_t rec=m.u32(0x81373cu)+off;
            m.put8(rec+0x30u,0);m.put8(rec+0x28u,m.u8(player+0x11u));m.put16(rec+0x24u,0x132u);m.put8(rec+0x33u,0x10u);
            m.put8(rec+0x31u,m.u8(player+0x12u));m.put8(rec+0x2au,m.u8(player+0x13u));
            const std::uint8_t al=std::uint8_t(call(c,0x48b1a0u,{}));
            m.put8(m.u32(0x81373cu)+off+0x29u,al);
            ++k;off+=PcRecordBytes;
        }while(std::int32_t(k)<=std::int32_t(call(c,0x450780u,{})));
    }
    m.put8(m.u32(0x81373cu)+0x2fu,0);
    m.put32(0x81043cu,0);
}
// 47F330(ECX = record, ESI = 81010C encoder state, work, every): one sample of the player's
// run into the record stream (+34) every 'every' frames of [81043C]: the area-local position
// (409F90 + 40A240 inverse) quantized to 0.01, the angle bytes, the flags byte and, while
// driving (+2C8 > 0 and +4 < 0), +2C8, +2D8.. and the +2E4.. angle bytes; else +2C8 = 0.
// The record is dropped (+0 = 0, [81010C] = 0) once the cursor reaches 0xF60.
void records_encode_47f330(PcRaceContext& c,std::uint32_t rec,std::uint32_t state,std::uint32_t work,std::uint32_t every){
    auto& m=c.m;
    if(!(m.i32(state+4u)<0xf60)){m.put32(rec,0);m.put32(state,0);return;}
    if(every==0u)throw std::domain_error("47F330: interval 0 (DIV)");
    if(m.u32(0x81043cu)%every)return;
    const std::uint32_t stream=rec+0x34u;
    driving::pc_matrix_push_load(c.matrices,m.bytes(m.u32(work+0x5cu)?0x7d3190u:0x7d2da0u,0x40));
    {const auto cur=c.matrices.current();(void)driving::pc_d3dx_matrix_inverse(cur,nullptr,cur);}
    const auto p=driving::pc_matrix_point(c.matrices,{m.f32(work+0x14u),m.f32(work+0x18u),m.f32(work+0x1cu)});
    driving::pc_matrix_rotate_y(c.matrices,word_radians(m.u16(work+0x2eu)));
    driving::pc_matrix_rotate_x(c.matrices,word_radians(m.u16(work+0x2cu)));
    driving::pc_matrix_rotate_z(c.matrices,word_radians(m.u16(work+0x30u)));
    std::array<std::uint8_t,0x40> cur{};driving::pc_matrix_get(c.matrices,driving::Bytes(cur.data(),cur.size()));
    const auto a=driving::pc_matrix_angles_449640(driving::Bytes(cur.data(),cur.size()));
    driving::pc_matrix_pop(c.matrices);
    const float K100=fbits(0x42c80000u),K001=fbits(0x3c23d70au);   // 6282CC, 6281C0
    auto quantize=[&](float v){return float(cvtt(v*K100))*K001;};
    const float qx=quantize(p.x),qy=quantize(p.y),qz=quantize(p.z);
    const std::uint8_t ax=std::uint8_t(cvtt(a[0]*KAngleInv)>>8),ay=std::uint8_t(cvtt(a[1]*KAngleInv)>>8),az=std::uint8_t(cvtt(a[2]*KAngleInv)>>8);
    std::uint8_t dl=0;
    if(m.i32(state+4u)>0){
        if(m.u8(work+0x33u)==m.u8(state+0x17u))dl=2;
        if(ax==m.u8(state+0x14u))dl|=4;
        if(ay==m.u8(state+0x15u))dl|=8;
        if(az==m.u8(state+0x16u))dl|=0x10;
    }
    if(!jbe(m.f32(work+0x2c8u),0.0f)&&std::int32_t(m.u32(work+4u))<0)dl|=1;
    if(m.u32(work+0x5cu)==0u)dl|=0x20;
    if(m.i32(work+0x38u)>0)dl|=0x40;
    auto at=[&](){return m.u32(state+4u);};
    auto put=[&](std::uint8_t b){m.put8(stream+at(),b);m.put32(state+4u,at()+1u);};
    auto coord=[&](float q,std::uint32_t prev,float scale){
        const float old=m.f32(prev);
        if(!std::isnan(q)&&!std::isnan(old)&&q==old){put(0x80);return;}
        const std::uint32_t v=std::uint32_t(cvtt(q*scale));
        m.put8(stream+at(),std::uint8_t(v>>8));m.put8(stream+at()+1u,std::uint8_t(v));m.put32(state+4u,at()+2u);
    };
    coord(qx,state+8u,fbits(0x4083126fu));     // 5B4374
    coord(qy,state+0xcu,fbits(0x4223d70au));   // 5B448C
    coord(qz,state+0x10u,fbits(0x4083126fu));
    m.putf(state+8u,qx);m.putf(state+0xcu,qy);m.putf(state+0x10u,qz);
    put(dl);
    if(!(dl&4))put(ax);
    if(!(dl&8))put(ay);
    if(!(dl&0x10))put(az);
    m.put8(state+0x15u,ay);m.put8(state+0x14u,ax);m.put8(state+0x16u,az);
    if(!(dl&2))put(m.u8(work+0x33u));
    if(!(dl&1)){m.putf(work+0x2c8u,0.0f);return;}
    const std::uint32_t bits=m.u32(work+0x2c8u);
    for(unsigned k=0;k<4;++k)m.put8(stream+at()+k,std::uint8_t(bits>>(24-8*k)));
    m.put32(state+4u,at()+4u);
    const float K4096=fbits(0x45800000u);   // 5B0420
    for(std::uint32_t o:{0x2d8u,0x2dcu,0x2e0u}){
        const std::uint32_t v=std::uint32_t(cvtt(m.f32(work+o)*K4096));
        m.put8(stream+at(),std::uint8_t(v>>8));m.put8(stream+at()+1u,std::uint8_t(v));m.put32(state+4u,at()+2u);
    }
    for(std::uint32_t o:{0x2e4u,0x2e8u,0x2ecu})put(std::uint8_t(cvtt(m.f32(work+o)*KAngleInv)>>8));
}
// 47F780(work) (CommonPlCar, every frame outside TA): in GAME and variant 0, a stage change
// (450130) or a goal (44FF10) closes the stage's record (QHOT header, +C length [810110], +4 /
// +20 / +1C times 4505A0, +14.. sectors 450630, +8 451180(0) on a goal, +10 / +2F otherwise) and
// restarts the encoder; during the countdown 47ED90, else 47F330 every 16 frames.
void records_player_47f780(PcRaceContext& c,std::uint32_t work){
    auto& m=c.m;
    const std::uint32_t stage=call(c,0x44c940u,{m.u32(work+0x68u)});
    if(m.u32(0x78026cu)!=0x10u||m.u32(0x780258u))return;
    if(call(c,0x450130u,{})||call(c,0x44ff10u,{})){
        std::uint32_t target;
        if(call(c,0x44ff10u,{})){
            target=stage;
            const std::uint32_t v=call(c,0x451180u,{0u});
            m.put32(stage*PcRecordBytes+m.u32(0x81373cu)+8u,v);
        }else{
            target=stage-1u;
            const std::uint32_t kind=call(c,0x450750u,{stage});
            if(kind)m.put32(stage*PcRecordBytes+m.u32(0x81373cu)+0x10u,m.u32(0x810440u));
            m.put8(stage*PcRecordBytes+m.u32(0x81373cu)+0x2fu,m.u8(work+0x68u));
        }
        const std::uint32_t rec=target*PcRecordBytes+m.u32(0x81373cu);
        if(m.u32(0x81010cu)==1u){
            m.put32(rec,0x544f4851u);m.put32(rec+0xcu,m.u32(0x810110u));
            m.put32(rec+4u,call(c,0x4505a0u,{target}));
        }
        for(std::uint32_t k=0;k<4u;++k)m.put32(rec+0x14u+k*4u,call(c,0x450630u,{target,k}));
        m.put32(rec+0x20u,call(c,0x4505a0u,{target}));
        if(call(c,0x450750u,{target}))m.put32(rec+0x1cu,call(c,0x4505a0u,{target}));
        m.put32(0x81043cu,0);
        for(std::uint32_t a:{0x81011cu,0x810118u,0x810114u})m.put32(a,0xc7c34fffu);
        m.put8(0x810121u,0);m.put8(0x810123u,0);m.put32(0x810110u,0);m.put32(0x81010cu,1u);
    }
    if(call(c,0x44ff10u,{}))return;
    if(std::int16_t(call(c,0x49b2d0u,{}))>0x3c){records_restart_47ed90(c);return;}
    m.put8(0x813748u,1);
    records_encode_47f330(c,stage*PcRecordBytes+m.u32(0x81373cu),0x81010cu,work,0x10u);
}
// 47EE70(out) (its protected step at 47EE89 only enters the loop): out[k] for the stages
// 0..450780() of [81373C]: 2 without a QHOT record (or no length), else 1 when the player's time
// +4 is not below the loaded record's of the same level (route +30; 0x0FFFFFFF without one),
// 0 for a new record. No table: nothing written.
void records_stage_flags_47ee70(PcRaceContext& c,std::uint32_t out){
    auto& m=c.m;
    std::uint32_t k=0;
    if(std::int32_t(call(c,0x450780u,{}))<0)return;
    for(std::uint32_t off=0;;off+=PcRecordBytes){
        const std::uint32_t t=m.u32(0x81373cu);
        if(t){
            const std::uint32_t rec=t+off;
            if(m.u32(rec+0xcu)==0u||m.u32(rec)!=0x544f4851u)m.put32(out+k*4u,2u);
            else{
                const std::int8_t level=m.i8(rec+0x2fu);
                const std::uint32_t loaded=std::uint32_t(std::int32_t(m.i8(rec+0x30u))+std::int32_t(level)*2)*PcRecordBytes+m.u32(0x813740u);
                std::int32_t best=0x0fffffff;
                if(m.u32(loaded)==0x544f4851u&&std::int8_t(m.u8(loaded+0x2fu))==level)best=m.i32(loaded+4u);
                m.put32(out+k*4u,m.i32(rec+4u)>=best?1u:0u);
            }
        }
        ++k;
        if(!(std::int32_t(k)<=std::int32_t(call(c,0x450780u,{}))))break;
    }
}
// 47EF30(preset): 1 when one of the first 5 stages (15 for preset 2) is a new record (47EE70
// flag 0). A flag 47EE70 left unwritten is the PC's stack garbage: it faults here.
std::uint32_t records_ranking_47ef30(PcRaceContext& c,std::uint32_t preset){
    auto& m=c.m;
    constexpr std::uint32_t Local47ef30=0x7ffe3000u;
    std::array<std::uint8_t,0x3c> bytes{};
    const auto mark=m.mark();m.map(Local47ef30,bytes.data(),bytes.size());
    constexpr std::uint32_t Unset=0xdeadbeefu;
    for(std::uint32_t k=0;k<15u;++k)m.put32(Local47ef30+k*4u,Unset);
    try{records_stage_flags_47ee70(c,Local47ef30);}catch(...){m.release(mark);throw;}
    const std::uint32_t n=preset==2u?15u:5u;
    std::uint32_t r=0;
    for(std::uint32_t k=0;k<n;++k){
        const std::uint32_t v=m.u32(Local47ef30+k*4u);
        if(v==Unset){m.release(mark);throw std::logic_error("47EF30: a stage flag 47EE70 did not write (PC stack garbage)");}
        if(v==0u){r=1u;break;}
    }
    m.release(mark);
    return r;
}
// 480F80 (every update, 417CF9): in GAME, not paused (43F9C0), variant 0 and with the record
// tables: 47FF60, then after the countdown 47FD40 and the frame counters 81043C / 810440.
void records_frame_480f80(PcRaceContext& c){
    auto& m=c.m;
    if(!m.u32(0x81373cu)||m.u32(0x78026cu)!=0x10u)return;
    if(call(c,0x43f9c0u,{})&0xffu)return;
    if(m.u32(0x780258u))return;
    records_schedule_47ff60(c);
    if(std::int16_t(call(c,0x49b2d0u,{}))>0x3c)return;
    records_advance_47fd40(c);
    m.put32(0x81043cu,m.u32(0x81043cu)+1u);
    m.put32(0x810440u,m.u32(0x810440u)+1u);
    m.put8(0x813748u,0);
}

// ---- variant 0 name entry: the record save (480D00 / 481180) ----
void records_prepare_480d00(PcRaceContext& c){
    auto& m=c.m;
    const std::uint8_t b447=m.u8(0x810447u),b446=m.u8(0x810446u);
    (void)call(c,0x43f960u,{});
    const std::uint8_t mode=m.u8(0x810444u);const std::uint8_t cvt=(mode==0u||mode==2u)?1u:0u;
    m.put8(0x813738u,b447);m.put8(0x813730u,cvt);m.put8(0x810450u,0);m.put8(0x80fb45u,b446);m.put8(0x80fb44u,0);m.put32(0x813734u,0);
    // the "\RecordData\..." name is formatted into a stack buffer nobody reads
    m.put32(0x80fb40u,ghost_save_open_416700(c,{1u,m.u8(0x813730u)?1u:0u,m.u8(0x80fb45u),m.u8(0x813738u),
        m.u32(0x813734u)?1u:0u,m.u8(0x80fb44u),m.u8(0x810450u)}));
}
void records_store_480df0(PcRaceContext& c,std::uint32_t rec,std::int32_t level,std::uint32_t kind){
    auto& m=c.m;
    if(m.u8(rec+0x32u)){
        const std::uint32_t stage=call(c,0x450780u,{});
        const std::uint32_t base=m.u32(0x81373cu)+stage*PcRecordBytes;
        for(std::uint32_t i=0;i<4u;++i)m.put32(base+0x14u+i*4u,call(c,0x450610u,{stage,i}));
        m.put32(base+0x1cu,call(c,0x451180u,{0u}));
    }
    const std::uint32_t n=m.u32(rec+0xcu)+0x40u;
    const std::uint8_t b447=m.u8(0x810447u),b446=m.u8(0x810446u);
    m.put16(rec+0x26u,0xffffu);
    m.put16(rec+0x26u,ghost_crc_449a80(m,rec,std::int32_t(n)));
    std::uint32_t big=0;
    if(call(c,0x43f960u,{})&&level>=10&&level<=14)big=1u;
    const std::uint8_t mode=m.u8(0x810444u);const std::uint8_t cvt=(mode==0u||mode==2u)?1u:0u;
    m.put8(0x80fb45u,b446);m.put8(0x810450u,std::uint8_t(level));m.put8(0x813738u,b447);m.put8(0x80fb44u,std::uint8_t(kind));
    m.put8(0x813730u,cvt);m.put32(0x813734u,big);
    (void)ghost_save_write_4169d0(c,{rec,n,1u,std::uint32_t(std::int32_t(m.i8(0x813730u))),m.u8(0x80fb45u),m.u8(0x813738u),
        kind,m.u32(0x813734u),std::uint32_t(level)});
}
void records_better_481100(PcRaceContext& c){
    auto& m=c.m;
    const std::uint32_t time=call(c,0x451180u,{0u});
    const std::uint32_t cur=call(c,0x450780u,{});
    const std::uint32_t rec=m.u32(0x81373cu)+cur*PcRecordBytes;
    const std::int32_t level=m.i8(rec+0x2fu);
    if(!time)return;
    const std::uint32_t base=m.u32(0x813740u)+std::uint32_t(level)*0x1fa8u,second=base+PcRecordBytes;
    if(m.u32(second)==0x544f4851u&&m.u16(second+0x24u)==0x132u&&time>=m.u32(base+0xfdcu))return;
    m.put8(rec+0x32u,1);
    records_store_480df0(c,rec,level,1u);
    m.put8(m.u32(0x81373cu)+cur*PcRecordBytes+0x32u,0);
}
void records_store_481180(PcRaceContext& c,std::uint32_t name){
    auto& m=c.m;
    constexpr std::uint32_t Flags=0x7ffe0400u;                     // the 0x3C-byte stack array of 47EE70
    std::array<std::uint8_t,0x40> flags{};const auto mark=m.mark();m.map(Flags,flags.data(),flags.size());
    try{
        records_stage_flags_47ee70(c,Flags);
        if(std::int32_t(call(c,0x450780u,{}))>=0){
            for(std::uint32_t k=0;;++k){
                const std::uint32_t rec=m.u32(0x81373cu)+k*PcRecordBytes;
                for(std::uint32_t i=0;i<4u;++i)m.put8(rec+0x2bu+i,m.u8(name+i));
                if(m.u32(Flags+k*4u)==0u)records_store_480df0(c,rec,m.i8(rec+0x2fu),m.u8(rec+0x30u));
                if(!(std::int32_t(k+1)<=std::int32_t(call(c,0x450780u,{}))))break;
            }
        }
    }catch(...){m.release(mark);throw;}
    m.release(mark);
    const std::uint32_t cur=call(c,0x450780u,{});
    const std::uint32_t rec=m.u32(0x81373cu)+cur*PcRecordBytes;
    if(m.i32(rec+0xcu)>0&&m.u32(rec)==0x544f4851u)records_better_481100(c);
    (void)ghost_save_flush_4167f0(c);
}
}
