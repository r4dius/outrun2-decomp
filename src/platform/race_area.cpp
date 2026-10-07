#include "platform/race_area.hpp"
#include "platform/pc_address_view.hpp"
#include "system/perf.hpp"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <set>
#include <sstream>
#if __has_include(<execinfo.h>)
#define OR2_HAVE_BACKTRACE 1
#include <execinfo.h>
#endif
namespace outrun::platform {
extern std::uint8_t RaceAreaDataImage[0xc98];
namespace {
std::string unmapped_text(std::uint32_t a,std::size_t n){
    std::ostringstream s;s<<"race area: PC address 0x"<<std::hex<<a<<" (+"<<std::dec<<n<<") is not mapped";return s.str();
}
}
PcRaceUnmapped::PcRaceUnmapped(std::uint32_t a,std::size_t n):std::out_of_range(unmapped_text(a,n)),address(a){
#if defined(OR2_HAVE_BACKTRACE)
    if(std::getenv("OR2_UNMAPPED_TRACE")){void* f[24];const int k=backtrace(f,24);backtrace_symbols_fd(f,k,2);}
#endif
}
namespace {
bool same_regions(const std::vector<PcRaceMemory::Region>& a,const std::vector<PcRaceMemory::Region>& b){
    if(a.size()!=b.size())return false;
    for(std::size_t i=0;i<a.size();++i)
        if(a[i].base!=b[i].base||a[i].data!=b[i].data||a[i].size!=b[i].size||a[i].writable!=b[i].writable)return false;
    return true;
}
// The pieces by the definition: each interval between consecutive edges is
// owned by the newest region covering it (O(edges * regions)); kept to check
// the sweep below (OR2_MEMORY_BUILD_CHECK=1).
std::vector<PcRaceMemory::Piece> pieces_reference(const std::vector<PcRaceMemory::Region>& regions){
    std::vector<std::uint64_t> edges;std::vector<PcRaceMemory::Piece> out;
    for(const auto& r:regions)if(r.size){edges.push_back(r.base);edges.push_back(std::uint64_t(r.base)+r.size);}
    std::sort(edges.begin(),edges.end());edges.erase(std::unique(edges.begin(),edges.end()),edges.end());
    for(std::size_t e=0;e+1<edges.size();++e){
        const auto lo=edges[e],hi=edges[e+1];
        for(std::size_t i=regions.size();i-->0;){
            const auto& r=regions[i];
            if(r.size&&lo>=r.base&&hi<=std::uint64_t(r.base)+r.size){
                if(!out.empty()&&out.back().region==i&&out.back().hi==lo)out.back().hi=hi;
                else out.push_back({lo,hi,i});
                break;
            }
        }
    }
    return out;
}
}
void PcRaceMemory::build() const {
    OR2_PERF_ZONE("race memory build");
    // Same list as an earlier build: its table (pieces index the same regions).
    for(auto& c:cache_)if(!c.regions.empty()&&same_regions(c.regions,regions_)){
        table_=std::size_t(&c-cache_.data());dirty_=false;last_=0;last2_=0;built_=regions_.size();
        overlay_lo_=~0ull;overlay_hi_=0;return;}
    // Sweep over the region edges with the set of covering regions: the
    // newest (highest index) owns the interval up to the next edge.
    struct Edge { std::uint64_t at; std::size_t region; bool open; };
    std::vector<Edge> ev;ev.reserve(regions_.size()*2u);
    for(std::size_t i=0;i<regions_.size();++i)if(regions_[i].size){
        ev.push_back({regions_[i].base,i,true});ev.push_back({std::uint64_t(regions_[i].base)+regions_[i].size,i,false});}
    std::sort(ev.begin(),ev.end(),[](const Edge& x,const Edge& y){return x.at<y.at;});
    auto& pieces_=scratch_;pieces_.clear();
    std::set<std::size_t> active;
    for(std::size_t k=0;k<ev.size();){
        const std::uint64_t at=ev[k].at;
        for(;k<ev.size()&&ev[k].at==at;++k){if(ev[k].open)active.insert(ev[k].region);else active.erase(ev[k].region);}
        if(k==ev.size()||active.empty())continue;
        const std::uint64_t hi=ev[k].at;const std::size_t owner=*active.rbegin();
        if(!pieces_.empty()&&pieces_.back().region==owner&&pieces_.back().hi==at)pieces_.back().hi=hi;
        else pieces_.push_back({at,hi,owner});
    }
    static const bool check=std::getenv("OR2_MEMORY_BUILD_CHECK")!=nullptr;
    if(check){const auto ref=pieces_reference(regions_);
        bool same=ref.size()==pieces_.size();
        for(std::size_t i=0;same&&i<ref.size();++i)same=ref[i].lo==pieces_[i].lo&&ref[i].hi==pieces_[i].hi&&ref[i].region==pieces_[i].region;
        if(!same)throw std::logic_error("PcRaceMemory: sweep table differs from the reference");}
    dirty_=false;last_=0;last2_=0;built_=regions_.size();overlay_lo_=~0ull;overlay_hi_=0;
    auto& c=cache_[cache_next_];cache_next_=(cache_next_+1u)%cache_.size();
    c.regions=regions_;c.pieces.swap(pieces_);table_=std::size_t(&c-cache_.data());
}
const PcRaceMemory::Region* PcRaceMemory::find(std::uint32_t a,std::size_t n) const {
    // Later mappings take precedence (temporary locals, overrides).
    if(n==0){
        for(auto it=regions_.rbegin();it!=regions_.rend();++it)
            if(a>=it->base&&std::size_t(a-it->base)<=it->size)return &*it;
        return nullptr;
    }
    if(dirty_)build();
    const std::uint64_t lo=a,hi=std::uint64_t(a)+n;
    // Overlays: the newest region touching the access owns it if it covers it
    // whole; touching only part of it, the access straddles a piece edge of
    // the full table and is refused like there.
    if(regions_.size()>built_&&hi>overlay_lo_&&lo<overlay_hi_)for(std::size_t i=regions_.size();i-->built_;){
        const auto& r=regions_[i];
        if(!r.size)continue;
        const std::uint64_t rlo=r.base,rhi=std::uint64_t(r.base)+r.size;
        if(hi<=rlo||lo>=rhi)continue;
        return lo>=rlo&&hi<=rhi?&r:nullptr;
    }
    const auto& pieces_=cache_[table_].pieces;
    if(last_<pieces_.size()){const auto& p=pieces_[last_];if(lo>=p.lo&&hi<=p.hi)return &regions_[p.region];}
    if(last2_<pieces_.size()){const auto& p=pieces_[last2_];if(lo>=p.lo&&hi<=p.hi){std::swap(last_,last2_);return &regions_[p.region];}}
    std::size_t l=0,h=pieces_.size();
    while(l<h){const auto m=(l+h)/2;if(pieces_[m].hi<=lo)l=m+1;else h=m;}
    if(l<pieces_.size()&&lo>=pieces_[l].lo&&hi<=pieces_[l].hi){last2_=last_;last_=l;return &regions_[pieces_[l].region];}
    return nullptr;
}
std::uint8_t* PcRaceMemory::at(std::uint32_t a,std::size_t n,bool write) const {
    const auto* r=find(a,n);if(!r)throw PcRaceUnmapped(a,n);
    if(write&&!r->writable)throw std::logic_error(unmapped_text(a,n)+" (read-only)");
    return r->data+(a-r->base);
}
std::uint16_t PcRaceMemory::u16(std::uint32_t a) const {std::uint16_t v;std::memcpy(&v,at(a,2),2);return v;}
std::uint32_t PcRaceMemory::u32(std::uint32_t a) const {std::uint32_t v;std::memcpy(&v,at(a,4),4);return v;}
float PcRaceMemory::f32(std::uint32_t a) const {float v;std::memcpy(&v,at(a,4),4);return v;}
void PcRaceMemory::put16(std::uint32_t a,std::uint16_t v) const {std::memcpy(at(a,2,true),&v,2);}
void PcRaceMemory::put32(std::uint32_t a,std::uint32_t v) const {std::memcpy(at(a,4,true),&v,4);}
void PcRaceMemory::putf(std::uint32_t a,float v) const {std::memcpy(at(a,4,true),&v,4);}
PcRaceAreaState::PcRaceAreaState():data(RaceAreaDataImage,RaceAreaDataImage+(DataEnd-DataBase)){}
void race_area_map_tables(PcRaceMemory& m){
    for(std::size_t i=0;i<EmbeddedExeRangeCount;++i){
        const auto& r=EmbeddedExeRanges[i];
        if(r.base<=0x5d4dc8u&&0x5d5e58u<=r.base+r.size){m.map_const(r.base,r.data,r.size);return;}
    }
    throw std::logic_error("race area: embedded EXE range 5D4DC8..5D5E58 missing");
}
namespace {
using driving::Bytes;
using driving::CourseProbe;
constexpr std::uint32_t None=0xffffffffu;
constexpr float F01=0.100000001f,F1=1.0f,Fm1=-1.0f;
struct Port {
    PcRaceContext& c;
    PcRaceMemory& m;
    explicit Port(PcRaceContext& cc):c(cc),m(cc.m){}
    // Globals of other modules, read through memory like the PC does.
    std::uint32_t mode(){return m.u32(0x780258u);}        // race mode
    std::uint32_t preset(){return m.u32(0x78024cu);}
    std::uint32_t game(){return m.u32(0x78026cu);}
    std::uint32_t car(){return m.u32(0x799d18u);}
    std::uint32_t call(std::uint32_t pc,std::initializer_list<std::uint32_t> args,std::uint32_t eax=0,std::uint32_t ecx=0){
        PcRaceCall k{};k.pc=pc;k.eax=eax;k.ecx=ecx;k.argc=std::uint32_t(args.size());
        unsigned i=0;for(auto a:args)k.args[i++]=a;
        if(!c.service)throw std::logic_error("race area: no service for PC call");
        return c.service(k);
    }
    bool byte_call(std::uint32_t pc){return (call(pc,{})&0xffu)!=0u;}   // test al,al
    void draw(std::uint32_t pc,std::initializer_list<std::uint32_t> args){
        if(!c.draws)throw std::logic_error("race area: display without draw list");
        PcVehicleDrawCall d{};d.pc=pc;d.argc=std::uint32_t(args.size());
        unsigned i=0;for(auto a:args)d.args[i++]=a;
        const auto cur=c.matrices.current();for(unsigned k=0;k<64;++k)d.matrix[k]=cur.u8(k);
        c.draws->push_back(d);
    }
    void obj(std::uint32_t id,std::uint32_t opaque,std::uint32_t nodes,std::uint32_t colour,std::uint32_t byte,std::uint32_t flag){
        draw(0x405360u,{id,opaque,nodes,colour,byte,flag});
    }
    // 55A930 (ECX = 7F9460) is `mov eax,[ecx+0x60]`: the network session
    // word 7F94C0 (0 offline), read in place.
    bool net(){return m.u32(0x7f9460u+0x60u)!=0u;}
    // 44B730: 55A930() && [7F95A8].
    bool net_active(){return net()&&m.u32(0x7f95a8u)!=0u;}
    bool both_none(std::uint32_t rec){return m.u32(rec+0x2c)==None&&m.u32(rec+0x30)==None;}
    // Matrix leaves on the native 89B564 stack.
    void push(){driving::pc_matrix_push(c.matrices);}
    void pop(){driving::pc_matrix_pop(c.matrices);}
    void push_load(std::uint32_t a){driving::pc_matrix_push_load(c.matrices,m.bytes(a,64));}
    void load(std::uint32_t a){driving::pc_matrix_load(c.matrices,m.bytes(a,64));}
    void get(std::uint32_t a){driving::pc_matrix_get(c.matrices,m.bytes(a,64));}
    void translate(float x,float y,float z){driving::pc_matrix_translate_vector(c.matrices,{x,y,z});}
    void translate_at(std::uint32_t a){translate(m.f32(a),m.f32(a+4),m.f32(a+8));}
    void scale(float x,float y,float z){ // 40A360: D3DXMatrixScaling then S * current
        std::array<float,16> s{x,0,0,0, 0,y,0,0, 0,0,z,0, 0,0,0,1};
        driving::pc_matrix_multiply_current(c.matrices,Bytes(s.data(),64));
    }
    void angles(std::uint32_t out,std::uint32_t matrix){ // 449640(out, matrix)
        const auto a=matrix_angles_449640(m.bytes(matrix,0x30));
        m.putf(out,a[0]);m.putf(out+4,a[1]);m.putf(out+8,a[2]);
    }
    // Temporarily maps a PC stack local at a symbolic address.
    struct Local {
        PcRaceMemory& m;std::array<std::uint8_t,8> bytes{};std::size_t mark;
        Local(PcRaceMemory& mm,std::uint32_t a):m(mm),mark(mm.mark()){m.map(a,bytes.data(),bytes.size());}
        ~Local(){m.release(mark);}
    };
    std::uint32_t kind(std::uint32_t index){ // course kind of a table index (-1 -> 0x42)
        if(index==None)return 0x42u;
        return m.u32(index*0x78u+m.u32(0x7d33bcu));
    }
    // 44C8D0 / 44C940 / 44C9A0 / 44C9C0 / 44CA00 / 44DC50
    std::uint32_t record(std::uint32_t id){return race_area_record_44c8d0(m,id);}
    std::uint32_t value(std::uint32_t id){return race_area_value_44c940(m,id);}
    std::uint32_t kind_a(){return kind(m.u32(m.u32(0x7d3188u)+0x24));}
    std::uint32_t kind_b(){
        if(byte_call(0x48b310u)&&byte_call(0x48b350u))return 0x42u;
        return kind(m.u32(m.u32(0x7d3188u)+0x28));
    }
    void stage_tables_44ca00(){
        const std::uint32_t desc=m.u32(m.u32(0x7d3188u)+0x14);
        const std::uint32_t a=kind(m.u32(m.u32(0x7d3188u)+0x24));
        const std::uint32_t b=kind_b();
        const std::uint32_t id=m.u32(desc);
        const std::uint32_t ai=a<<4,bi=b<<4;
        m.put32(0x63685cu,m.u32(id*0x14u+0x5d540cu));
        const std::uint32_t edx=m.u32(ai+0x5d4fdcu),ecx=m.u32(ai+0x5d4fe4u);
        m.put32(0x636860u,edx);
        m.put32(0x636864u,m.u32(m.u32(desc)*0x14u+0x5d5414u));
        m.put32(0x636868u,ecx);
        m.put32(0x63686cu,m.u32(m.u32(desc)*0x14u+0x5d5410u));
        const std::uint32_t edi=m.u32(bi+0x5d4fe0u),eax=m.u32(bi+0x5d4fe4u);
        m.put32(0x636870u,edi);
        const std::uint32_t esi=m.u32(m.u32(desc)*0x14u+0x5d5418u);
        m.put32(0x6368a4u,edi);m.put32(0x636874u,esi);m.put32(0x636878u,eax);
        m.put32(0x636894u,edx);m.put32(0x63689cu,ecx);m.put32(0x6368acu,eax);
    }
    std::uint32_t course_value_44dc50(std::uint32_t id){
        const auto r=record(id);
        if(r)return m.u32(m.u32(r+0x14));
        return m.u32(m.u32(0x7d2df4u));
    }
    void stats_reset_44ba00(){
        for(std::uint32_t a=0x7d3074u;a<=0x7d30a4u;a+=4)m.put32(a,0);
        m.put32(0x7d308cu,0x3e8u);
    }
    void sky_objects_44c4b0(){
        const std::uint32_t d=m.u32(m.u32(0x7d3188u)+0x14);
        for(unsigned k=0;k<3;++k){const auto v=m.u32(d+0x4c+k*4);if(v!=None)call(0x4103f0u,{v,5});}
        const auto v=m.u32(d+0x48);if(v!=None)call(0x4103f0u,{v,0xa});
    }
    void weather_44bd50(){
        const std::uint32_t bits=m.u32(m.u32(m.u32(0x7d3188u)+0x14)+0x3c);
        call(0x4afd90u,{bits&2u});
        call(bits&1u?0x4af560u:0x4af570u,{});
        call(bits&8u?0x4afb40u:0x4afb50u,{});
        call(bits&0x10u?0x4afb70u:0x4afb80u,{});
        call(bits&4u?0x4af580u:0x4af590u,{});
    }
    // Event record callbacks (6369C0 +18 / +1C): the ported sound triggers,
    // 49A650 (RET); any other pointer is reached through the service.
    void event_callback(std::uint32_t pc,bool has_arg,std::uint32_t arg){
        const std::int32_t v=std::int32_t(arg);
        auto se=[&](std::uint32_t id){call(0x4249f0u,{id});};
        switch(pc){
        case 0x49a650u:return;
        case 0x44ba50u:if(v==0)se(0x884);else if(v==0x190)se(0x8084);return;
        case 0x44bcb0u:se(0x8084);return;
        case 0x44ba80u:if(v%0x19==0)se(0x83);return;
        case 0x44baa0u:if(v==0)se(0x86);return;
        case 0x44bac0u:if(v==0)se(0x89);return;
        case 0x44bae0u:if(v<0x110){if(v%0x50==0)se(0x8b);if(v%0x8c==0)se(0x8a);}return;
        case 0x44bb30u:
            if(v==0){se(0x88d);}
            else if(v>=0x258){if(v==0x258)se(0x808c);return;}
            if(v%0xc8==0||v%0x140==0)se(0x8c);
            if(v==0x258)se(0x808c);
            return;
        case 0x44bcc0u:se(0x808c);return;
        case 0x44bb90u:if(v==0)se(0x8e);return;
        case 0x44bbb0u:if(v==0)se(0x90);else if(v==0xc8||v==0x12c)se(0x8f);return;
        case 0x44bcd0u:se(0x8090);return;
        case 0x44bbf0u:if(v<0x12c){if(v%0x64==0)se(0x8b);if(v%0xa0==0)se(0x91);}return;
        default:
            if(has_arg)call(pc,{arg});else call(pc,{});
        }
    }
};
}
std::uint32_t race_area_record_44c8d0(const PcRaceMemory& m,std::uint32_t id){
    const std::int32_t n=m.i32(0x7d33c4u);
    if(n<=0)return 0;
    const std::uint32_t table=m.u32(0x7d33bcu);
    for(std::int32_t i=0;i<n;++i)if(m.u32(table+4+std::uint32_t(i)*0x78u)==id)return std::uint32_t(i)*0x78u+table;
    return 0;
}
std::uint32_t race_area_value_44c940(PcRaceMemory& m,std::uint32_t id){
    if(id==m.u32(0x635f2cu))return m.u32(0x635f30u);
    const auto r=race_area_record_44c8d0(m,id);
    const std::uint32_t v=r?m.u32(r+8):0u;
    m.put32(0x635f2cu,id);m.put32(0x635f30u,v);return v;
}
std::uint32_t race_area_sky_time_44c610(const PcRaceMemory& m){
    if((m.u8(0x79fcceu)&3u)!=2u)return 0x64u;
    const std::uint32_t p=m.u32(m.u32(m.u32(0x7d3188u)+0x14)+0x58);
    if(!p)return 0x64u;
    return (p&0xffff0000u)|m.u16(p);
}
std::uint32_t race_area_sky_steps_44c640(const PcRaceMemory& m){
    // Protected entry bridge (jmp 103C630) measured: AL = byte [79FCCE].
    if((m.u8(0x79fcceu)&3u)!=2u)return 2u;
    const std::uint32_t p=m.u32(m.u32(m.u32(0x7d3188u)+0x14)+0x58);
    if(!p)return 2u;
    return m.u8(p+2);
}
void race_area_sky_ids_44c420(PcRaceContext& c,std::uint32_t out,std::uint32_t second){
    Port p(c);auto& m=c.m;
    const std::uint32_t a=m.u32(0x7d3188u),b=m.u32(0x7d31dcu); // CMOVNE always reads 7D31DC
    const std::uint32_t rec=second?b:a;
    if(!rec){m.put32(out+4,None);m.put32(out+8,None);m.putf(out+0x14,0.f);m.putf(out+0x18,0.f);return;}
    const std::uint32_t d=m.u32(rec+0x14);
    if(p.call(0x448820u,{m.u32(d+0x44)})==1u){
        m.put32(out+4,m.u32(d+0x4c));m.put32(out+8,m.u32(d+0x50));m.put32(out+0xc,m.u32(d+0x54));m.put32(out+0x10,m.u32(d+0x48));
    }else{
        m.put32(out+4,None);m.put32(out+8,None);m.put32(out+0xc,None);m.put32(out+0x10,None);
    }
    m.putf(out+0x14,0.f);m.putf(out+0x18,0.f);
}
std::uint32_t race_area_fade_44c530(PcRaceContext& c){
    auto& m=c.m;const std::uint32_t car=m.u32(0x799d18u);
    if(m.u32(car+0x5c)==0){m.putf(0x7d3184u,F1);return 0;}
    const std::int16_t cx=m.i16(car+0x64);
    if(cx>=0xf0){m.putf(0x7d3184u,F1);return 0;}
    if(cx<=0x84){m.putf(0x7d3184u,0.f);m.putf(0x7d2e6cu,Fm1);return 1;}
    float x2=m.f32(0x7d2e6cu);
    if(0.f>x2){x2=m.f32(car+0x2b0);m.putf(0x7d2e6cu,x2);}
    const std::uint32_t mode=m.u32(0x780258u);
    const float div=(mode==3||mode==4)?0.0799999982f:0.200000003f;
    const float x0=(m.f32(car+0x2b0)-x2)/div;
    m.putf(0x7d3184u,x0);
    if(0.f>x0){m.putf(0x7d3184u,0.f);return 1;}
    if(x0>F1){m.putf(0x7d3184u,F1);return 0;}
    return 1;
}
std::uint32_t race_area_geometry_load_44c310(PcRaceContext& c,std::uint32_t token,std::uint32_t lane){
    Port p(c);auto& m=c.m;
    const std::uint32_t st=lane*4u+0x7d2e70u,handle=lane*4u+0x7d2e64u,geometry=lane*4u+0x7d306cu;
    const std::uint32_t s=m.u32(st);
    if(s==0){
        if(token==0){m.put32(st,2);return 0;}
        const auto h=p.call(0x44fd80u,{token,7});
        m.put32(handle,h);m.put32(st,1);return 1;
    }
    if(s!=1)return 0;
    if(p.call(0x44f880u,{m.u32(handle)})==0)return 1;
    {
        // 44FC60(handle, &local, &lane argument slot, 7D306C + lane*4).
        Port::Local a(m,PcRaceLocal44C310A),b(m,PcRaceLocal44C310B);
        std::memcpy(b.bytes.data(),&lane,4);
        p.call(0x44fc60u,{m.u32(handle),PcRaceLocal44C310A,PcRaceLocal44C310B,geometry});
    }
    (void)m.u32(geometry); // 49A650([7D306C+lane*4]) is a RET
    m.put32(handle,0);m.put32(st,2);return 0;
}
std::uint32_t race_area_ending_44d410(PcRaceContext& c){
    Port p(c);auto& m=c.m;
    const std::uint32_t over=m.u32(0x6368c4u);
    if(over!=None)return over;
    const std::uint32_t mode=m.u32(0x780258u),car=m.u32(0x799d18u);
    const std::uint32_t t=mode*0x1cu+0x6366b8u;
    auto at=[&](std::uint32_t k){return m.u32(t+k);};
    auto ratio=[&](bool num_first){
        const float a=float(m.i32(0x7d3088u)),b=float(m.i32(0x7d3094u));(void)num_first;return a/b;};
    if(mode==3||mode==4){
        const std::uint32_t e=p.value(m.u32(car+0x68));
        const std::int32_t a=std::int32_t(p.call(0x455b80u,{e}))-1;
        const std::int32_t b=std::int32_t(p.call(0x455ba0u,{e}))-1;
        if(a<0||b<0)return None;
        return m.u32(std::uint32_t(a*7+b)*4u+0x6366b8u);
    }
    if(m.u32(0x7d3074u)||m.u32(0x7d3078u))return at(0);
    if(m.u32(0x7d307cu))return at(4);
    const float x=ratio(true);
    if(x>=F01)return at(4);
    if(mode==0){
        if(!(F01>x))return at(8);
        const std::int32_t d=m.i32(0x7d3090u);
        if(d==0)return at(8);
        const std::int32_t e=m.i32(0x7d308cu);
        if(e<0x104)return at(0xc);
        if(e<0x113)return at(0x10);
        if(d<0x3c)return at(0x10);
        const std::uint32_t v=p.value(m.u32(car+0x68));
        if(!v)return at(0x10);
        const std::uint32_t bx=p.call(0x450630u,{v,2});
        const std::uint32_t bp=p.call(0x47fbd0u,{v,2,0});
        const std::uint32_t ax=p.call(0x47fbd0u,{v,2,1});
        if(std::int32_t(bx-bp)>=0)return at(0x14);
        if(std::int32_t(bx-ax)>=0)return at(0x14);
        return at(0x18);
    }
    if(mode==1){
        if(!(F01>x))return at(8);
        if(!(0.f==x))return at(8);
        if(m.u32(0x7d3080u)||m.u32(0x7d3084u))return at(0xc);
        if(m.i32(0x7d30a4u)<0x258)return at(0x10);
        if(!p.value(m.u32(car+0x68)))return at(0x10);
        if(m.i32(0x7d308cu)>=0x113)return at(0x18);
        return at(0x14);
    }
    if(!(0.f==x))return at(8);
    if(m.u32(0x7d3080u))return at(0xc);
    const std::int32_t e=m.i32(0x7d3084u);
    if(e>=0x3c)return at(0x10);
    if(m.i32(0x7d30a4u)<0x12c)return at(8);
    if(e>0)return at(0x14);
    return at(0x18);
}
void race_area_stats_44ede0(PcRaceContext& c){
    Port p(c);auto& m=c.m;
    const std::uint32_t car=m.u32(0x799d18u);
    const std::uint32_t ax=std::uint32_t(std::abs(std::int32_t(m.i16(car+0x4e))));
    const std::uint32_t cx=m.u32(car+0x1f4);
    const bool slide=cx>=0x64u&&ax>0x1800u&&ax<0x4000u;
    if(p.call(0x450130u,{}))p.stats_reset_44ba00();
    if(m.u32(0x78026cu)!=0x10u)return;
    if(m.u8(car+4)&8u)return;
    if(p.call(0x4556f0u,{})==0){
        const std::uint32_t id=m.u8(car+0x10);
        m.put8(car+0x101b,m.u8(car+0x100c));
        m.put8(car+0x100c,std::uint8_t(p.call(0x455bd0u,{id})));
    }
    const std::uint32_t mode=m.u32(0x780258u);
    if(mode==3||mode==4){
        p.value(m.u32(car+0x68));
        if(m.u8(0x7d33d0u)==0){
            const std::uint32_t r=m.u32(0x7d3188u);
            if(p.both_none(r))m.put8(0x7d33d0u,1);
            else{
                if(m.u32(0x7d3074u)||m.u32(0x7d3078u))m.put8(car+0x100c,std::uint8_t(m.u8(car+0x100c)|1u));
                if((m.u8(car+0x244)&0x9cu)||m.u32(0x7d3080u)||m.u32(0x7d307cu))m.put8(car+0x100c,std::uint8_t(m.u8(car+0x100c)|2u));
            }
        }
    }
    if(m.u32(car+0x5c))return;
    const std::uint32_t r=p.record(m.u32(car+0x68));
    const std::int32_t sections=r?std::int32_t(m.i16(m.u32(r+0x14)+0x7c)):0;
    m.put32(0x7d3094u,std::uint32_t(sections));
    const float lim=m.f32(0x636bbcu);
    if(m.u32(car+0x68)==0&&m.i16(car+0x64)<0xa0){
        m.put32(0x7d3094u,std::uint32_t(sections-0xa0));
    }else{
        const float best=float(m.i32(0x7d308cu));
        const float f=m.f32(car+0x1f8);
        if(best>f){
            std::int32_t t;
            if(std::isnan(f)||f>=2147483648.f||f<-2147483648.f)t=std::int32_t(0x80000000u);else t=std::int32_t(f);
            m.put32(0x7d308cu,std::uint32_t(t));
        }
        if(lim>std::fabs(m.f32(car+0xb54))&&m.i16(car+0x64)>m.i16(car+0x18c))m.put32(0x7d3098u,m.u32(0x7d3098u)+1);
        if(!slide&&(m.u8(car+0x244)&0x9cu)&&m.i16(car+0x64)>m.i16(car+0x18c))m.put32(0x7d3088u,m.u32(0x7d3088u)+1);
    }
    m.put32(0x7d309cu,std::uint32_t(std::int32_t(m.i16(car+0x64))-std::int32_t(m.i16(car+0x18c))));
    if((m.u32(car+4)&0x80000u)||(m.u8(car+0x2f0)&1u)){
        const std::uint32_t k=((m.u32(car+0x2f0)>>2)&0x1fu)-1u;
        if(k<=0xcu){
            if(k==0)m.put32(0x7d3074u,m.u32(0x7d3074u)+1);
            else if(k==1||k==4||k==5||k==11||k==12)m.put32(0x7d3078u,m.u32(0x7d3078u)+1);
        }
    }
    if(m.u8(car+6)&1u)m.put32(0x7d3080u,m.u32(0x7d3080u)+1);
    if(m.u32(car+4)&0x20000u)m.put32(0x7d307cu,m.u32(0x7d307cu)+1);
    if(m.u32(car+4)&0x400000u)m.put32(0x7d3084u,m.u32(0x7d3084u)+1);
    if(lim>std::fabs(m.f32(car+0xb54))&&!(m.u32(car+4)&0x80000u)){
        const std::uint32_t bits=m.u32(car+0x258)|m.u32(car+0x254)|m.u32(car+0x250)|m.u32(car+0x24c);
        if((bits&0x2003b00u)&&!(m.u32(car+0x2a8)&0x10f407cu)){
            const float f=m.f32(car+0xb54),g=m.f32(car+0x26c);
            bool inc=false;
            if(f>0.f&&g>0.f)inc=true;
            else if(0.f>f&&0.f>g)inc=true;
            if(inc)m.put32(0x7d3090u,m.u32(0x7d3090u)+1);
        }
    }
    if(m.f32(car+0xe68)>0.180000007f){
        if(std::abs(std::int32_t(m.i16(car+0x162)))<0x5dc)m.put32(0x7d30a4u,m.u32(0x7d30a4u)+1);
    }
}
void race_area_init_44cb00(PcRaceContext& c){
    Port p(c);auto& m=c.m;
    m.put32(0x7d2e80u,0x14);m.put32(0x7d2e88u,0x14);m.put32(0x7d2d80u,0x14);
    m.put32(0x7d3188u,0x7d30a8u);m.put32(0x7d31dcu,0x7d30a8u);
    const std::uint32_t id=m.u32(0x7d30acu);
    const std::uint32_t mode=m.u32(0x780258u);(void)mode;
    m.put32(0x7d2e5cu,0xf);
    const std::uint32_t v=p.value(id);
    const std::uint32_t a=m.u32(0x7d30bcu),b=m.u32(0x7d30c0u);
    m.put32(0x7d33acu,v);
    m.put32(0x7d2e90u,m.u32(a+4));m.put32(0x7d2e8cu,m.u32(a));
    m.put32(0x7d2e78u,m.u32(b+4));
    m.put32(0x7d3174u,m.u32(b));m.put32(0x7d3060u,m.u32(a+0x38));m.put32(0x7d31e0u,m.u32(b+0x38));
    const std::uint32_t b60=m.u32(b+0x60);
    m.put32(0x7d31e4u,m.u32(a+0x60));m.put32(0x7d2e60u,b60);m.put32(0x7d2e58u,m.u32(a+0x44));
    m.put32(0x7d3064u,0);m.put8(0x7d31d0u,0);
    p.sky_objects_44c4b0();
    p.weather_44bd50();
    p.call(0x46fc30u,{1});
    const std::uint32_t a78=m.u32(a+0x78);
    p.stats_reset_44ba00();
    m.putf(0x7d3184u,F1);
    m.put32(0x7d2e84u,0);m.put32(0x7d305cu,0);m.put32(0x7d33c8u,0);m.put32(0x7d33ccu,0);
    m.putf(0x7d2e6cu,Fm1);m.put32(0x7d31d4u,a78);
    const std::uint32_t k=p.kind(m.u32(m.u32(0x7d3188u)+0x24));
    m.put32(0x7d3170u,m.u32((k<<4)+0x5d4fd8u));
    std::uint32_t k2;
    if(p.byte_call(0x48b310u)&&p.byte_call(0x48b350u))k2=0x42u;
    else k2=p.kind(m.u32(m.u32(0x7d3188u)+0x28));
    m.put32(0x7d31d8u,m.u32((k2<<4)+0x5d4fd8u));
    p.stage_tables_44ca00();
}
void race_area_destroy_44b7f0(PcRaceContext& c){c.m.put32(0x7d3188u,0);}
void race_area_control_44e590(PcRaceContext& c,std::uint32_t entry_ecx){
    Port p(c);auto& m=c.m;
    const std::uint32_t desc=m.u32(m.u32(0x7d3188u)+0x14);
    auto S=[&](std::uint32_t v){m.put32(0x7d2e80u,v);};
    std::uint32_t esi=entry_ecx;
    std::uint32_t s=m.u32(0x7d2e80u);
    auto fade_451e90=[&]{return p.call(0x451e90u,{});};
    auto fade=[&]{return race_area_fade_44c530(c);};
    // Common tail from 44ECA6 (branch to the next area).
    auto next_area=[&]{
        const std::uint32_t v=p.value(esi);
        m.put32(0x7d2e5cu,v);
        const std::uint32_t k=p.call(0x451350u,{v});
        m.put32(0x7d31dcu,m.u32(0x7d3188u));
        m.put32(0x7d2e7cu,k);
        std::uint32_t rec;
        if(p.net()&&m.u32(0x7f95a8u)&&p.call(0x46c500u,{})){
            rec=m.u32(m.u32(0x7d3188u)+0x24)*0x78u+m.u32(0x7d33bcu);
            m.put32(0x7d3188u,rec);
        }else{
            rec=m.u32(0x7d3188u);
            const std::uint32_t i=m.u32(rec+m.u32(0x7d2e7cu)*4u+0x24);
            if(i!=None){rec=i*0x78u+m.u32(0x7d33bcu);m.put32(0x7d3188u,rec);}
        }
        const std::uint32_t a=m.u32(m.u32(0x7d31dcu)+0x14),b=m.u32(rec+0x14);
        m.put32(0x7d33b4u,m.u32(a)==m.u32(b)?1u:0u);
        p.call(0x451dd0u,{});
        m.put32(0x7d33c8u,0);
        S(1);
        p.call(0x448990u,{m.u32(0x7d2e90u)});S(2);
    };
    auto running=[&](bool check){ // 44EBF7 / 44EC0E
        if(check){if(p.call(0x4502c0u,{})==0)return;S(0x15);}
        if(m.u32(0x780258u)==4){
            const auto r=p.call(0x451350u,{p.value(p.call(0x450380u,{8}))});
            if(r==2)return;
        }
        p.call(0x4502d0u,{});
        esi=p.call(0x450380u,{8});
        if(m.u32(0x7d33b4u)==0){
            const std::uint32_t cur=m.u32(0x7d2e5cu);
            if(cur==p.value(esi))return;
        }
        m.putf(0x7d3184u,0.f);S(0);m.putf(0x7d2e6cu,Fm1);m.put32(0x7d33c8u,0);m.put32(0x7d33ccu,0);
        next_area();
    };
    auto result_state=[&](std::uint32_t r){if(r==0)S(0x14);};
    switch(s){
    case 0:next_area();return;
    case 1:p.call(0x448990u,{m.u32(0x7d2e90u)});S(2);return;
    case 2:p.call(0x448990u,{m.u32(0x7d3060u)});S(3);return;
    case 3:p.call(0x448990u,{m.u32(0x7d31e4u)});S(4);return;
    case 4:p.call(0x4af550u,{});m.put32(0x7d3120u,m.u32(0x7d2e58u));S(5);return;
    case 5:
        p.call(0x43de50u,{0});
        // 44C3D0: release both geometry lanes.
        for(std::uint32_t a=0x7d306cu;a<0x7d3074u;a+=4)if(m.u32(a))p.call(0x440cd0u,{a});
        m.put32(0x7d306cu,0);m.put32(0x7d2e64u,0);m.put32(0x7d2e70u,0);m.put32(0x7d3070u,0);m.put32(0x7d2e68u,0);m.put32(0x7d2e74u,0);
        p.call(0x44a1a0u,{});p.call(0x4f11b0u,{0});p.call(0x4f0600u,{0});p.call(0x46fc30u,{0});
        p.call(0x4ef860u,{});p.call(0x4ef850u,{});p.call(0x4299c0u,{0x3c});p.call(0x42dfb0u,{0x3c});
        S(6);return;
    case 6:{
        const bool m5=m.u32(0x780258u)==5;
        S(7);
        if(m5){p.call(0x46c4c0u,{});p.call(0x46c260u,{});p.call(0x46c2c0u,{1});}
        if(m.u32(0x7d2e5cu))return;
        const std::uint32_t pr=m.u32(0x78024cu);
        p.call(0x448990u,{(pr==1||pr==3)?0xc3u:0x12bu});
        p.call(0x4f21c0u,{0x17});p.call(0x4f21c0u,{0x21});p.call(0x4401d0u,{0x16c});
        return;}
    case 7:{
        if(m.u32(0x780258u)==5&&p.call(0x46c240u,{})==0)return;
        const std::uint32_t a=m.u32(0x7d2d80u);
        if(a==1||a==2||a==0xe)return;
        for(unsigned k=0;k<16;++k)m.put32(0x7d3130u+k*4,m.u32(0x7d2da0u+k*4));
        p.push_load(0x7d3190u);
        const std::uint32_t k=m.u32(0x7d2e7cu);
        if(k==0){p.translate_at(0x636048u);driving::pc_matrix_rotate_y(c.matrices,m.f32(0x636058u));
            driving::pc_matrix_rotate_x(c.matrices,m.f32(0x636054u));driving::pc_matrix_rotate_z(c.matrices,m.f32(0x63605cu));}
        else if(k==1){p.translate_at(0x636060u);driving::pc_matrix_rotate_y(c.matrices,m.f32(0x636070u));
            driving::pc_matrix_rotate_x(c.matrices,m.f32(0x63606cu));driving::pc_matrix_rotate_z(c.matrices,m.f32(0x636074u));}
        p.get(0x7d2da0u);p.angles(0x7d3124u,0x7d2da0u);p.pop();
        S(8);}
        [[fallthrough]];
    case 8:
        if(p.call(0x43dba0u,{m.u32(desc+0xc),0}))return;
        S(9);
        [[fallthrough]];
    case 9:{
        if(race_area_geometry_load_44c310(c,m.u32(desc+0x20),0))return;
        if(race_area_geometry_load_44c310(c,m.u32(desc+0x64),1))return;
        if(p.call(0x46fe50u,{0,m.u32(desc)}))return;
        m.put32(0x7d33ccu,m.u32(0x7d33c8u)!=0?1u:0u);
        if(m.u32(0x7d33b4u)==0){const auto t=m.u32(desc+0x44);m.put32(0x7d2e58u,t);p.call(0x448ad0u,{t,0xa});}
        S(0xa);}
        [[fallthrough]];
    case 0xa:
        if(p.call(0x448980u,{})==0)return;
        if(p.call(0x44aa80u,{m.u32(desc+0x24),m.u32(desc+0x28),m.u32(desc+0x2c)}))return;
        p.sky_objects_44c4b0();
        S(0x10);return;
    case 0xb:
        fade_451e90();
        if(p.call(0x448980u,{})==0)return;
        p.call(0x448ad0u,{m.u32(0x7d31e4u),9});
        m.put32(0x7d3064u,1);S(0xc);return;
    case 0xc:
        fade_451e90();fade();
        if(p.call(0x448980u,{})==0)return;
        S(0xd);return;
    case 0xd:{
        fade_451e90();fade();p.weather_44bd50();
        const std::uint32_t mode=m.u32(0x780258u);
        if(mode==2){
            const std::uint32_t id=m.u32(m.u32(0x7d3188u)+4);
            const std::uint32_t r=m.u32(0x78024cu)==1?p.call(0x45dfb0u,{id}):p.call(0x45e000u,{id});
            if(p.call(0x427700u,{r}))return;
        }else if(mode==5){
            if(p.call(0x46c250u,{}))return;
        }
        S(0xe);}
        [[fallthrough]];
    case 0xe:{
        fade_451e90();fade();
        if(m.u32(0x780258u)==2){
            const std::uint32_t r=p.call(0x45c470u,{m.u32(m.u32(0x7d3188u)+4)});
            if(r==0){
                if(p.call(0x4f10d0u,{m.u32(desc+0x30),0}))return;
            }else{
                // 5802DD sprintf(7D3450, "\OSO\%s", r)
                std::uint32_t o=0x7d3450u;
                for(char ch:std::string("\\OSO\\"))m.put8(o++,std::uint8_t(ch));
                for(std::uint32_t i=0;;++i){const auto ch=m.u8(r+i);m.put8(o++,ch);if(!ch)break;}
                if(p.call(0x4f10d0u,{0x7d3450u,0}))return;
            }
        }else{
            if(p.call(0x4f10d0u,{m.u32(desc+0x30),0}))return;
            if(p.call(0x46c4d0u,{})==0)return;
        }
        if(p.call(0x4f0430u,{m.u32(desc+0x34),0}))return;
        p.call(0x4f0d10u,{0});
        if(p.course_value_44dc50(m.u32(m.u32(0x7d3188u)+4))==0x1cu){
            p.call(0x42deb0u,{0x3c,9});p.call(0x429920u,{0x3c,9});
        }
        if(m.u32(0x780258u)==5){
            Port::Local l(m,PcRaceLocal44E590);
            if(p.call(0x46c380u,{PcRaceLocal44E590}))return;
            p.call(0x4f03a0u,{PcRaceLocal44E590,0});
        }
        S(0xf);return;}
    case 0xf:{
        fade_451e90();fade();
        p.call(0x4efb50u,{m.u32(desc+0x68)});
        p.call(0x4efaf0u,{m.u32(desc+0x6c)});
        const std::uint32_t e=m.u32(m.u32(0x7d3188u)+0x64);
        if(e)p.call(0x4efd20u,{e,0});
        if(p.call(0x42df90u,{})==0)return;
        if(p.call(0x4299a0u,{})==0)return;
        S(0x11);return;}
    case 0x10:{
        fade_451e90();
        p.call(0x4103a0u,{1,m.u32(desc+0x3c)});
        const std::uint32_t d0=m.u32(desc),d4=m.u32(desc+4),d38=m.u32(desc+0x38);
        m.put32(0x7d2e8cu,d0);
        const std::uint32_t d60=m.u32(desc+0x60);
        m.put32(0x7d2e90u,d4);m.put32(0x7d3060u,d38);m.put32(0x7d31e4u,d60);
        p.call(0x448ad0u,{d4,9});
        p.call(0x448ad0u,{m.u32(0x7d3060u),9});
        S(0xb);return;}
    case 0x11:
        fade_451e90();
        if(fade())return;
        if(fade_451e90())return;
        if(m.u32(0x7d33b4u)==0)p.call(0x448990u,{m.u32(0x7d3120u)});
        S(0x14);m.put32(0x7d31d4u,m.u32(desc+0x78));
        return; // 440D90 is an empty function
    case 0x15:running(false);return;
    case 0x16:{
        if(p.call(0x427700u,{0x206}))return;
        result_state(p.call(0x427700u,{m.u32(0x78024cu)==1?0x8443u:0x841au}));return;}
    case 0x17:
        if(p.call(0x427700u,{0x206}))return;
        result_state(p.call(0x46c250u,{}));return;
    case 0x18:result_state(p.call(0x427700u,{0x206}));return;
    case 0x19:
        if(p.call(0x427700u,{0x206}))return;
        result_state(p.call(0x427700u,{0x879d}));return;
    default:running(true);return;
    }
}
std::uint32_t race_area_stage_44cf00(PcRaceContext& c){
    Port p(c);auto& m=c.m;
    p.call(0x440d10u,{1});
    auto finish=[&]{return p.call(0x440d30u,{});};
    if(m.u32(0x7d2e80u)!=0x14u)return finish();
    auto T=[&](std::uint32_t v){m.put32(0x7d2e88u,v);};
    auto special=[&]{
        const std::uint32_t mode=m.u32(0x780258u);
        if(mode==3||mode==4)return true;
        if(p.byte_call(0x4957f0u)||p.byte_call(0x48b310u)||p.byte_call(0x495490u))return true;
        return p.net_active()&&p.call(0x46c500u,{})!=0;
    };
    const std::uint32_t t=m.u32(0x7d2e88u);
    auto stage_objects=[&](std::uint32_t rec){ // 44D222
        if(p.call(0x4f10d0u,{m.u32(m.u32(rec+0x18)+0x30),1}))return;
        if(p.call(0x4f0430u,{m.u32(m.u32(m.u32(0x7d3188u)+0x18)+0x34),1}))return;
        const std::uint32_t r=m.u32(0x7d3188u);
        if(p.both_none(r)){
            if(p.call(0x45aee0u,{})==0)return;
            if(p.call(0x46fe50u,{1,0xa}))return;
        }else if(m.u32(0x7d2e84u)==0){
            if(p.call(0x46fe50u,{1,0}))return;
            m.put32(0x7d2e84u,1);
        }
        p.call(0x4f0d10u,{1});T(0xf);
    };
    switch(t){
    case 1:
        p.call(0x448990u,{m.u32(0x7d3170u)});p.call(0x448990u,{m.u32(0x7d31d8u)});p.call(0x448990u,{m.u32(0x7d2e78u)});
        T(3);break;
    case 3:p.call(0x448990u,{m.u32(0x7d2e60u)});T(4);break;
    case 4:{
        p.call(0x43de50u,{1});p.call(0x4f11b0u,{1});p.call(0x4f0600u,{1});
        bool load=true;
        if(m.u8(0x7d33d0u)==0){
            const std::uint32_t r=m.u32(0x7d3188u);
            if(p.both_none(r))m.put8(0x7d33d0u,1);else load=false;
        }
        if(load)p.call(0x46fc30u,{1});
        const std::uint32_t r=m.u32(0x7d3188u);
        T(m.u32(m.u32(r+0x18)+4)!=0x223u?0x7u:0x14u);
        break;}
    case 7:{
        p.push_load(0x7d2da0u);
        const std::uint32_t d=m.u32(m.u32(0x7d3188u)+0x14);
        p.translate_at(d+0x80);
        driving::pc_matrix_rotate_y(c.matrices,m.f32(m.u32(m.u32(0x7d3188u)+0x14)+0x90));
        driving::pc_matrix_rotate_x(c.matrices,m.f32(m.u32(m.u32(0x7d3188u)+0x14)+0x8c));
        driving::pc_matrix_rotate_z(c.matrices,m.f32(m.u32(m.u32(0x7d3188u)+0x14)+0x94));
        p.get(0x7d3190u);p.angles(0x7d3178u,0x7d3190u);p.pop();
        std::uint32_t r,path;
        if(special()){
            r=m.u32(0x7d3188u);
            path=p.both_none(r)?m.u32(m.u32(r+0x18)+4):m.u32(m.u32(r+0x18)+0x10);
        }else{r=m.u32(0x7d3188u);path=m.u32(m.u32(r+0x18)+4);}
        const std::uint32_t alt=m.u32(r+0x18);
        m.put32(0x7d2e78u,path);m.put32(0x7d31e0u,0x67);m.put32(0x7d3174u,m.u32(alt));m.put32(0x7d2e60u,m.u32(alt+0x60));
        p.call(0x448ad0u,{path,2});p.call(0x448ad0u,{m.u32(0x7d2e60u),2});
        T(0xb);break;}
    case 8:{
        std::uint32_t path;
        if(special()&&m.u8(0x7d33d0u)==0){
            const std::uint32_t r=m.u32(0x7d3188u);
            if(p.both_none(r)){m.put8(0x7d33d0u,1);path=m.u32(m.u32(r+0x18)+0xc);}
            else path=0x5a42f4u;
        }else path=m.u32(m.u32(m.u32(0x7d3188u)+0x18)+0xc);
        if(p.call(0x43dba0u,{path,1}))break;
        const bool flag=m.u8(0x7d33d0u)!=0;
        T(0xe);
        std::uint32_t r=m.u32(0x7d3188u);
        if(!flag&&p.both_none(r))m.put8(0x7d33d0u,1);
        stage_objects(r);break;}
    case 0xe:stage_objects(m.u32(0x7d3188u));break;
    case 0xb:
        if(p.call(0x448980u,{})==0)break;
        T(8);m.put32(0x7d3064u,0);break;
    case 0xf:{
        const std::uint32_t e=m.u32(m.u32(0x7d3188u)+0x68);
        if(e)p.call(0x4efd20u,{e,1});
        T(0x12);break;}   // 440D90 is an empty function
    case 0x12:{
        m.put32(0x7d3170u,m.u32((p.kind_a()<<4)+0x5d4fd8u));
        m.put32(0x7d31d8u,m.u32((p.kind_b()<<4)+0x5d4fd8u));
        p.call(0x448ad0u,{m.u32(0x7d3170u),2});p.call(0x448ad0u,{m.u32(0x7d31d8u),2});
        T(0x13);break;}
    case 0x13:
        if(p.call(0x448980u,{})==0)break;
        p.stage_tables_44ca00();T(0x14);break;
    default:{
        if(p.call(0x450300u,{})==0)break;
        p.call(0x450310u,{});
        m.put32(0x7d33acu,p.value(m.u32(m.u32(0x7d3188u)+4)));
        T(1);
        if(p.net()){
            const std::uint32_t r=m.u32(0x7d3188u);
            p.call(0x46c360u,{(m.u32(r+0x24)!=None&&m.u32(r+0x28)==None)?1u:0u});
        }
        break;}
    }
    return finish();
}
void race_area_objects_44f190(PcRaceContext& c){
    Port p(c);auto& m=c.m;
    p.call(0x450380u,{8});
    const std::uint32_t mode=m.u32(0x780258u);
    const std::uint32_t car=m.u32(0x799d18u);
    if(mode==2||mode==5)return;
    race_area_stats_44ede0(c);
    const std::uint32_t s=m.u32(0x7d2d80u);
    auto release=[&]{ // 44F25C / 44F460: stop the previous event's objects
        const std::uint32_t rec=m.u32(0x7d349cu);
        for(std::int32_t i=std::int32_t(m.u32(rec+0xc))-1;i>=0;--i)
            if(m.u8(std::uint32_t(i)+0x79fca4u)&3u)p.call(0x4401d0u,{std::uint32_t(i)+0x15cu});
        p.call(0x4f0600u,{2});p.call(0x4f11b0u,{2});
        p.call(0x448990u,{m.u32(m.u32(0x7d349cu))});
    };
    auto state2=[&]{
        const std::int32_t a=m.i32(0x7d2e80u);
        if(a==0x14||a<7)return;
        if(m.u32(0x7d349cu)){release();m.put32(0x7d349cu,0);}
        const std::uint32_t rec=m.u32(0x7d34a4u);
        m.put32(0x7d2d80u,0xe);
        p.call(0x448ad0u,{m.u32(rec),2});
    };
    auto state15=[&]{
        const std::uint32_t rec=m.u32(0x7d34a4u);
        if(m.i16(car+0x64)<=m.i16(rec+0x10))return;
        if(m.u32(rec+4)==0){
            const std::uint32_t h=p.call(0x4f0910u,{2});
            std::uint32_t r=m.u32(0x7d34a4u);
            for(std::uint32_t i=0,off=0;std::int32_t(i)<m.i32(r+0xc);++i,off+=0x40){
                p.call(0x4f0cb0u,{i+0x15cu,0x4d,m.u32(r+0x20)+off});
                r=m.u32(0x7d34a4u);
                const std::uint32_t inst=m.u32(r+0x20)+off;
                m.put32(inst+0x30,0);m.putf(inst+0x34,0.f);
                m.putf(m.u32(r+0x20)+off+0x38,0.f);m.putf(m.u32(r+0x20)+off+0x3c,0.f);
            }
            p.call(0x4f0910u,{h});
        }else p.call(0x4f0d10u,{2});
        m.put32(0x7d2d80u,0x14);m.put32(0x7d349cu,m.u32(0x7d34a4u));m.put32(0x7d34a4u,0);
    };
    switch(s){
    case 1:{
        std::int32_t e=m.i32(0x7d3068u);
        if(e<0){
            e=std::int32_t(race_area_ending_44d410(c));m.put32(0x7d3068u,std::uint32_t(e));
            if(e<0){if(m.i16(car+0x64)>=0x3c)m.put32(0x7d2d80u,0x14);return;}
        }
        m.put32(0x7d34a4u,std::uint32_t(e)*0x24u+0x6369c0u);m.put32(0x7d2d80u,2);m.put32(0x7d34a0u,0);
        state2();return;}
    case 2:state2();return;
    case 14:{
        const std::uint32_t rec=m.u32(0x7d34a4u);
        if(p.call(0x448960u,{m.u32(rec)})==0)return;
        if(p.call(0x427700u,{m.u16(m.u32(0x7d34a4u)+0x12)}))return;
        if(p.call(0x4f10d0u,{m.u32(m.u32(0x7d34a4u)+4),2}))return;
        if(p.call(0x4f0430u,{m.u32(m.u32(0x7d34a4u)+8),2}))return;
        m.put32(0x7d2d80u,0xf);
        state15();return;}
    case 15:state15();return;
    default:break;
    }
    // 44F403
    const std::uint32_t a184=m.u32(car+0x184);
    bool run=false;
    if(a184==0){
        if(m.u32(car+0x5c)!=0&&m.u8(0x7d33d0u)==0){
            const std::uint32_t r=m.u32(0x7d3188u);
            if(p.both_none(r))m.put8(0x7d33d0u,1);
            else{
                m.put32(0x7d3068u,race_area_ending_44d410(c));m.put32(0x7d2d80u,1);run=true;
            }
        }
        run=true;
    }else if(m.u32(car+0x5c)!=0)run=true;
    if(!run){
        if(m.u32(0x7d349cu)==0)return;
        release();
        if(m.u32(0x78026cu)==0x10u)p.event_callback(m.u32(m.u32(0x7d349cu)+0x1c),false,0);
        p.call(0x4278c0u,{0x300});
        m.put32(0x7d349cu,0);m.put32(0x7d34a0u,0);m.put32(0x7d3068u,0);
        return;
    }
    if(m.u32(0x7d349cu)==0)return;
    const std::uint32_t side=p.call(0x451350u,{p.value(m.u32(car+0x68))});
    if(side==2)return;
    std::array<float,16> local{1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1};   // 40A060
    if(side==1)local[0]=Fm1;
    const float k=m.f32(car+0xdbc);
    CourseProbe v{m.f32(car+0x20)*k,m.f32(car+0x24)*k,m.f32(car+0x28)*k};
    p.push_load(0x7d3190u);
    {
        const CourseProbe at=driving::pc_matrix_inverse_point(c.matrices,{m.f32(car+0x14),m.f32(car+0x18),m.f32(car+0x1c)});
        m.putf(0x7d3490u,at.x);m.putf(0x7d3494u,at.y);m.putf(0x7d3498u,at.z);
    }
    v=driving::pc_matrix_inverse_vector(c.matrices,v);
    std::uint32_t rec=m.u32(0x7d349cu);
    for(std::uint32_t i=0,off=0,slot=0x79ecc8u;std::int32_t(i)<m.i32(rec+0xc);++i,off+=0x40,slot+=0x3c){
        if(!(m.u8(i+0x79fca4u)&3u))continue;
        const std::uint32_t obj=m.u32(slot);
        p.load(0x7d3190u);
        rec=m.u32(0x7d349cu);
        const std::uint32_t inst=m.u32(rec+0x20)+off;
        if(m.u32(inst+0x30)==0){
            m.put32(inst+0x34,m.u32(0x7d3490u));m.put32(inst+0x38,m.u32(0x7d3494u));m.put32(inst+0x3c,m.u32(0x7d3498u));
        }else{
            const float lim=m.f32(rec+0x14);
            if(lim>std::fabs(v.x)&&(m.u8(inst+0x28)&4u))m.putf(inst+0x34,m.f32(inst+0x34)-lim*local[0]);
            else m.putf(inst+0x34,v.x+m.f32(inst+0x34));
        }
        const std::uint32_t in2=m.u32(rec+0x20)+off;
        const std::uint32_t flags=m.u32(in2+0x28);
        float x=0.f,z=0.f;
        if(flags&1u)x=m.f32(in2+0x34);
        if(flags&2u)z=m.f32(0x636bc0u);
        p.translate(x,0.f,z);
        if(local[0]==Fm1)m.put16(obj+0x90,1);
        m.put32(obj+0x8c,(std::uint32_t(m.u8(m.u32(m.u32(0x7d349cu)+0x20)+off+0x28))>>3)&1u);
        driving::pc_matrix_multiply_current(c.matrices,Bytes(local.data(),64));
        p.get(obj+0xa0);
        rec=m.u32(0x7d349cu);
        m.put32(obj+0x88,1);
        if(m.u32(m.u32(rec+0x20)+off+0x30)==0){
            m.putf(obj+0x74,m.f32(obj+0x74)-m.f32(m.u32(rec+0x20)+off+0x2c));
            m.put32(m.u32(rec+0x20)+off+0x30,1);
        }
    }
    p.pop();
    if(m.u32(0x78026cu)==0x10u)p.event_callback(m.u32(m.u32(0x7d349cu)+0x18),true,m.u32(0x7d34a0u));
    m.put32(0x7d34a0u,m.u32(0x7d34a0u)+1);
}
void race_area_control_44f7c0(PcRaceContext& c,std::uint32_t entry_ecx){
    race_area_control_44e590(c,entry_ecx);
    race_area_stage_44cf00(c);
    race_area_objects_44f190(c);
}
void race_area_geometry_44de10(PcRaceContext& c,std::uint32_t car,std::uint32_t desc){
    Port p(c);auto& m=c.m;
    p.draw(0x4052b0u,{});
    auto done=[&]{p.draw(0x4052c0u,{});};
    if(m.i32(0x7d2e80u)<=0xb){done();return;}
    const float fade=m.f32(0x7d3184u);
    if(F01>fade){done();return;}
    p.load(0x7d2da0u);
    if(F1>m.f32(0x7d3184u))p.scale(F1,m.f32(0x7d3184u),F1);
    if(m.u32(0x7d306cu)==0){
        std::uint32_t id=m.u32(m.u32(desc+8));
        for(std::uint32_t k=0;id!=None;){
            p.obj(id,1,0,0,None,0);
            ++k;id=m.u32(m.u32(desc+8)+k*4);
        }
        done();return;
    }
    std::uint32_t section;
    bool have=false;
    if(p.call(0x4872e0u,{})==1u){section=p.call(0x487320u,{});have=(section&0xffffu)!=0xffffu;}
    if(!have){
        if(m.u32(car+0x5c)!=0)section=m.u32(0x7d3064u)?0u:p.call(0x43d470u,{0});
        else section=m.u16(car+0x64);
    }
    const std::uint32_t bp=section&0xffffu;
    const std::uint32_t geo=m.u32(m.u32(0x7d306cu));
    if(m.u32(geo)!=0){
        p.obj(m.u32(m.u32(desc+8)),0,geo+m.u32(geo+bp*4),0,None,0);
    }else{
        const std::uint32_t res=m.u32(desc+4);
        const std::int32_t count=std::int32_t(p.call(0x448810u,{res}));
        const std::uint32_t base=m.u32(desc+4)<<16;
        const std::uint32_t r=p.call(0x40ecb0u,{});
        const std::uint32_t byte=std::uint32_t(std::int32_t(std::int8_t(std::uint8_t(r%0x28u+1u))));
        for(std::int32_t i=0;i<count;++i){
            const std::uint32_t part=m.u32(geo+8+std::uint32_t(i)*4)+geo;
            const std::uint32_t nodes=m.u32(part+bp*4)+part;
            p.obj(std::uint32_t(i)|base,0,nodes,m.u32(desc+0x70),byte,0);
        }
    }
    // Protected bridge at 44DFBE (measured): EAX = [5D4DC8 + [desc]*4].
    const std::uint32_t sky=m.u32(0x5d4dc8u+m.u32(desc)*4u);
    if(sky!=None)p.obj(sky,0,0,0,None,0);
    const std::uint32_t goal=m.u32(m.u32(desc)*0x14u+0x5d5408u);
    if(goal!=None&&m.u32(car+0x5c)==0&&m.u8(0x7d33d0u)==0){
        const std::uint32_t r=m.u32(0x7d3188u);
        if(p.both_none(r))m.put8(0x7d33d0u,1);
        else{
            bool ok=true;
            if(p.net()&&m.u32(0x7f95a8u)&&p.call(0x46c500u,{})!=1u)ok=false;
            if(ok&&m.u32(0x7d2e88u)==0x14u&&m.i16(car+0x64)>m.i16(desc+0x5e))p.obj(goal,0,0,0x63687cu,1,0);
        }
    }
    const std::uint32_t id=m.u32(desc);
    const bool gate=m.u32(m.u32(0x7d3188u)+8)==0?true:(id==0||id==0xf);
    if(gate){
        std::uint32_t a1,a2,a3;
        if(id==0){a1=0x580002u;a2=0x580003u;a3=0x580004u;}
        else if(id==0xf){a1=0xfe0001u;a2=0xfe0002u;a3=0xfe0003u;}
        else{a1=0x1ef0001u;a2=0x1ef0002u;a3=0x1ef0003u;}
        std::uint32_t sign=None;
        if(m.u32(0x78026cu)==3u)sign=a3;
        else if(std::int16_t(p.call(0x49b2d0u,{}))<=0x3c)sign=a3;
        else if(std::int16_t(p.call(0x49b2d0u,{}))<=0x78)sign=a2;
        else if(std::int16_t(p.call(0x49b2d0u,{}))<=0xb4)sign=a1;
        const std::uint32_t d0=m.u32(desc);
        if(d0!=0&&d0!=0xf){
            p.push();p.translate(0.f,0.f,-25.f);
            p.obj(0x1ef0004u,1,0,0,None,0);
            if(sign!=None)p.obj(sign,0,0,0,None,0);
            p.pop();
        }else p.obj(sign,0,0,0,None,0);
    }
    if(m.u32(desc)==0xbu&&p.call(0x44ff10u,{}))p.obj(0x630001u,0,0,0,None,0);
    if(m.u32(desc)==0x1cu){
        if(p.call(0x43f960u,{})){done();return;}
        if(p.call(0x46c500u,{})||(p.byte_call(0x48b310u)&&p.byte_call(0x48b350u))){m.put32(0x636bb8u,2);done();return;}
        const std::uint32_t r=m.u32(0x7d3188u),table=m.u32(0x7d33bcu);
        std::uint32_t i=m.u32(r+0x24);
        if(i!=None&&m.u32(i*0x78u+table)!=0x42u){done();return;}
        i=m.u32(r+0x28);
        if(i!=None&&m.u32(i*0x78u+table)!=0x42u){done();return;}
        if(p.call(0x44ff10u,{})==0&&m.u32(car+0x5c)==0&&m.i16(car+0x64)>=0x190){p.obj(0x1010002u,0,0,0,None,0);done();return;}
        if(p.call(0x44ff10u,{})&&m.u32(0x636bb8u)){
            p.obj(0x1010002u,0,0,0,None,0);m.put32(0x636bb8u,m.u32(0x636bb8u)-1);
        }
    }
    done();
}
void race_area_stage_draw_44e290(PcRaceContext& c,std::uint32_t car,std::uint32_t desc){
    Port p(c);auto& m=c.m;
    const std::uint32_t mode=m.u32(0x780258u);
    const std::uint32_t v=p.value(m.u32(car+0x68));
    if(m.u32(0x7d2e88u)!=0x14u)return;
    const std::uint32_t flag=m.u32(car+0x5c);
    std::uint16_t bx=m.u16(car+0x64);
    std::uint32_t bp=0;
    if(flag==0){
        if(p.call(0x4872e0u,{})==1u){
            bx=std::uint16_t(p.call(0x487320u,{}));
            {const std::int32_t sx=std::int16_t(bx);if(std::uint32_t(sx)==0x0000ffffu)bx=m.u16(car+0x64);} // MOVSX: never true
        }
        const std::int16_t ax=m.i16(desc+0x5c);
        if(std::int16_t(bx)<ax)bp=None;
        else if(std::int16_t(bx)<=m.i16(desc+0x5e))return;
    }
    const std::uint32_t side=p.call(0x451350u,{v+bp});
    p.load(0x7d3190u);
    p.draw(0x4052b0u,{});
    if(!(m.u8(0x7d33d0u)!=0&&flag==0)){
        const std::uint32_t r=m.u32(0x7d3188u);
        if(p.both_none(r)&&flag==0)m.put8(0x7d33d0u,1);
        else if(bp==0){
            bool ok=true;
            if(p.net()&&m.u32(0x7f95a8u)&&p.call(0x46c500u,{})!=1u)ok=false;
            if(ok)p.obj(0x670000u,0,0,0x6368b0u,1,0);
        }
    }
    // Protected bridge at 44E3F1 (measured): ESI = [7D2E78].
    std::uint32_t obj=m.u32(0x7d2e78u)<<16;
    const std::int16_t sbx=std::int16_t(bx);
    if((sbx<0xe4&&flag!=0)||(flag==0&&bp==0))
        p.obj(obj,0,0,0,None,0);
    if((sbx>0x1c&&flag!=0)||bp==None){
        if(p.net()&&m.u32(0x7f95a8u)&&p.call(0x46c500u,{})){
            p.obj(obj|2u,0,0,0,None,0);obj|=1u;p.obj(obj,0,0,0,None,0);
        }else if(side==1){obj|=2u;p.obj(obj,0,0,0,None,0);}
        else if(side==0){obj|=1u;p.obj(obj,0,0,0,None,0);}
    }
    auto done=[&]{p.draw(0x4052c0u,{});};
    if(mode==3||mode==4||p.byte_call(0x4957f0u)||p.byte_call(0x48b310u)||p.byte_call(0x495490u)){
        if(m.u8(0x7d33d0u)!=0&&flag==0){done();return;}
        const std::uint32_t r=m.u32(0x7d3188u);
        if(p.both_none(r)&&flag==0){m.put8(0x7d33d0u,1);done();return;}
        if(side==0)p.obj(0x570018u,0,0,0,None,0);
        else if(side==1)p.obj(0x57004cu,0,0,0,None,0);
    }
    done();
}
void race_env_model_44cd30(PcRaceContext& c){
    Port p(c);auto& m=c.m;
    const std::uint32_t desc=m.u32(m.u32(0x7d3188u)+0x14);
    const std::uint32_t car=m.u32(0x799d18u);
    p.push();
    p.draw(0x4044e0u,{6});
    if(m.u32(car+0x5c)!=0){
        if(m.u32(0x7d2e88u)==0x14u){                                          // bridge 44CD63
            const std::uint32_t res=m.u32(0x7d2e60u);
            if(res!=0x223u){
                const std::uint32_t side=p.call(0x451350u,{p.value(m.u32(car+0x68))});
                const std::int16_t at=std::int16_t(m.u16(car+0x64));
                (void)p.value(m.u32(car+0x68));
                p.load(0x7d3190u);
                const std::uint32_t base=res<<16;
                if(at<0x94)p.obj(base,0,0,0,None,0);
                if(at>0x1c){
                    if(side==1u)p.obj(base|2u,0,0,0,None,0);
                    else if(side==0u)p.obj(base|1u,0,0,0,None,0);
                }
            }
        }
    }else if(m.i32(0x7d2e80u)>=0xe&&m.u32(0x7d3070u)!=0u){
        const std::uint32_t list=m.u32(m.u32(0x7d3070u));
        const std::uint32_t section=std::uint32_t(m.u16(car+0x64));        // 44B8D0 (car +5C == 0)
        p.load(0x7d2da0u);
        if(m.u32(list)!=0u){
            p.obj(m.u32(desc+0x60)<<16,0,list+m.u32(list+section*4u),0,None,0);
        }else{
            const std::int32_t count=std::int32_t(p.call(0x448810u,{m.u32(desc+0x60)}));
            const std::uint32_t base=m.u32(desc+0x60)<<16;
            for(std::int32_t i=0;i<count;++i){
                const std::uint32_t part=m.u32(list+8u+std::uint32_t(i)*4u)+list;
                p.obj(std::uint32_t(i)|base,0,part+m.u32(part+section*4u),0,None,0);
            }
        }
        const std::uint32_t extra=m.u32(0x5d4ed0u+m.u32(desc)*4u);
        if(extra!=None)p.obj(extra,0,0,0,None,0);
    }
    p.draw(0x4044e0u,{7});
    p.pop();
}
void race_env_sky_451c20(PcRaceContext& c,std::uint32_t w){
    Port p(c);auto& m=c.m;
    driving::pc_matrix_push_load(c.matrices,m.bytes(0x95dba0u,64));          // 411220, 409F90
    for(std::uint32_t k=0;k<2u;++k){
        const std::uint32_t r=w+k*0x90u;
        const float alpha=m.f32(r+0x40u);
        if(!(alpha>1.1920928955078125e-07f))continue;                                // [6281F0] = 2^-23; comiss / jbe: unordered skips
        if(m.u32(r+0xcu)==None)continue;
        driving::pc_matrix_load_rotation(c.matrices,m.bytes(r+0x50u,64));    // 40A190
        if(k==0u){
            p.draw(0x4044f0u,{0,0,0,8,0xf});p.draw(0x4044f0u,{1,0,0,8,0xf});
            p.obj(m.u32(r+0xcu),1,0,0,None,0);
        }else{
            const std::uint32_t blend=(1.0f>alpha)?1u:0u;                        // [62806C] = 1.0; comiss / jbe
            p.draw(0x4044f0u,{0,blend,0,8,0xf});p.draw(0x4044f0u,{1,blend,0,8,0xf});
            p.draw(0x4056d0u,{m.u32(r+0xcu),m.u32(r+0x40u),0,None});
        }
    }
    p.draw(0x404540u,{});
    p.pop();
}
void race_area_display_44f120(PcRaceContext& c){
    Port p(c);auto& m=c.m;
    const std::uint32_t desc=m.u32(m.u32(0x7d3188u)+0x14);
    const std::uint32_t car=m.u32(0x799d18u);
    p.push();
    p.draw(0x4044e0u,{6});
    if(m.u32(car+0x5c)==0){race_area_geometry_44de10(c,car,desc);race_area_stage_draw_44e290(c,car,desc);}
    else{race_area_stage_draw_44e290(c,car,desc);race_area_geometry_44de10(c,car,desc);}
    p.draw(0x4044e0u,{7});
    p.pop();
}
}
