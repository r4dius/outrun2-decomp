#include "driving/pc_car_services_net.hpp"
#include "driving/pc_matrix_stack.hpp"
#include "driving/pc_wall_rebound.hpp"
#include "driving/pc_wrecker.hpp"
#include <cmath>
#include <cstring>
#include <limits>
#include <stdexcept>

namespace outrun::driving {
namespace {
using X=long double;
float bits_f(std::uint32_t u){float f;std::memcpy(&f,&u,4);return f;}
CourseProbe rd3(Bytes b,std::size_t o){return {b.f32(o),b.f32(o+4),b.f32(o+8)};}
// 0x40EFA0: x87 (a-b) per component, spilled to float.
CourseProbe sub3_40efa0(CourseProbe a,CourseProbe b){
    return {static_cast<float>(static_cast<X>(a.x)-b.x),static_cast<float>(static_cast<X>(a.y)-b.y),
            static_cast<float>(static_cast<X>(a.z)-b.z)};
}
// 0x40EFF0(out,a,b): x87 products subtracted then spilled.
CourseProbe cross_40eff0(CourseProbe a,CourseProbe b){
    return {static_cast<float>(static_cast<X>(b.z)*a.y-static_cast<X>(a.z)*b.y),
            static_cast<float>(static_cast<X>(a.z)*b.x-static_cast<X>(b.z)*a.x),
            static_cast<float>(static_cast<X>(a.x)*b.y-static_cast<X>(b.x)*a.y)};
}
X x87_sqrt(X v){
#if defined(__i386__) || defined(__x86_64__)
    X r;__asm__("fsqrt":"=t"(r):"0"(v));return r;
#else
    return std::sqrt(v);
#endif
}
// 0x40F0E0: sqrt((x*x+y*y)+z*z) in x87, returned in ST0.
X length_40f0e0(CourseProbe v){
    return x87_sqrt((static_cast<X>(v.x)*v.x+static_cast<X>(v.y)*v.y)+static_cast<X>(v.z)*v.z);
}
// 0x40EFD0(a,b): (z*z'+y*y')+x*x' in x87, returned in ST0.
X dot_40efd0(CourseProbe a,CourseProbe b){
    return (static_cast<X>(a.z)*b.z+static_cast<X>(a.y)*b.y)+static_cast<X>(a.x)*b.x;
}
// 0x449360 / 0x449370: fsin / fcos of a float argument (x87 instruction).
X x87_sin(float a){
#if defined(__i386__) || defined(__x86_64__)
    X r;__asm__("fsin":"=t"(r):"0"(static_cast<X>(a)));return r;
#else
    return std::sin(static_cast<X>(a));
#endif
}
X x87_cos(float a){
#if defined(__i386__) || defined(__x86_64__)
    X r;__asm__("fcos":"=t"(r):"0"(static_cast<X>(a)));return r;
#else
    return std::cos(static_cast<X>(a));
#endif
}
// 0x582194 (MSVC _ftol2): truncation toward zero; NaN/out of range give the
// x87 integer indefinite 0x8000000000000000.
std::int64_t ftol2(X v){
    if(!(v>=X(-9223372036854775808.0L)&&v<X(9223372036854775808.0L)))return std::numeric_limits<std::int64_t>::min();
    return static_cast<std::int64_t>(v);
}
void check_road(const PcRoadInfoResult& r){if(r.found)r.info.check(0,0x58);}
PcRoadInfoResult query_road(std::array<std::uint8_t,0x64>& buf,Bytes place,std::int32_t hint,PcRoadInfoContext& c){
    buf.fill(0);Bytes out(buf.data(),buf.size());
    const bool ok=pc_get_cs_road_info_by_cs_len(out,place,hint,c);
    return {ok,out};
}
}

long double pc_route_progress_457480(Bytes event,const PcRoadInfoResult& road){
    event.check(0x14,12);event.check(0x260,2);check_road(road);
    if(!road.found)return static_cast<X>(bits_f(0u)); // [0x619A34] == 0.0f
    const Bytes r=road.info;
    const CourseProbe v24=rd3(r,0x24),v30=rd3(r,0x30),v3c=rd3(r,0x3c),v48=rd3(r,0x48),n=rd3(r,0x18);
    const CourseProbe pos=rd3(event,0x14);
    const CourseProbe d1=sub3_40efa0(v30,v24);
    const CourseProbe d2=sub3_40efa0(v48,v3c);
    const CourseProbe c1=cross_40eff0(d1,n);
    const CourseProbe c2=cross_40eff0(n,d2);
    const CourseProbe p1=sub3_40efa0(pos,v24);
    const CourseProbe p2=sub3_40efa0(pos,v3c);
    const float len1=static_cast<float>(length_40f0e0(c1));
    const float len2=static_cast<float>(length_40f0e0(c2));
    const float d1s=static_cast<float>(dot_40efd0(p1,c1)/len1);
    const X q2=dot_40efd0(p2,c2)/len2;
    const X ratio=q2/(static_cast<X>(d1s)+q2);
    return ratio+static_cast<X>(static_cast<std::int32_t>(static_cast<std::uint16_t>(event.i16(0x260))));
}
long double pc_route_progress_457480(Bytes event,PcRoadInfoContext& c){
    event.check(0x5c,12);std::array<std::uint8_t,0x64> buf{};
    const auto r=query_road(buf,event.sub(0x5c,12),-1,c);
    return pc_route_progress_457480(event,r);
}
void pc_route_state4_457770(Bytes event,std::uint8_t slot,Bytes table,const PcRoadInfoResult& road){
    table.check(std::size_t(slot)*0x6c,4);
    table.putf(std::size_t(slot)*0x6c,static_cast<float>(pc_route_progress_457480(event,road)));
}
void pc_route_state4_457770(Bytes event,std::uint8_t slot,Bytes table,PcRoadInfoContext& c){
    table.check(std::size_t(slot)*0x6c,4);
    table.putf(std::size_t(slot)*0x6c,static_cast<float>(pc_route_progress_457480(event,c)));
}

void pc_manager_forced_cruise_4fb870(Bytes m,Bytes e,Bytes w,Bytes p){
    m.check(0x184,4);m.check(0x7668,0x10);
    const float zero=0.0f;
    if(zero>=m.f32(0x7670))return; // comiss/jae: ordered 0 >= value exits
    e.check(0x14,0x20);e.check(0x3c,4);e.check(0x48,4);e.check(0x178,4);e.check(0x1c4,4);e.check(0x1f4,4);e.check(0x208,0x18);
    w.check(0x32c,0x2e4);p.check(0xb48,4);p.check(0xb94,4);p.check(0x10a0,4);p.check(0x1644,4);
    if(m.u32(0x7668)!=0u&&m.i32(0x766c)>0){
        e.putf(0x1c4,e.f32(0x1c4)+bits_f(0x3b973320u));
        const std::int32_t n=static_cast<std::int32_t>(m.u32(0x766c)-1u);m.puti(0x766c,n);
        if(n==0)m.put32(0x7668,0u);
    }
    if(m.f32(0x7674)>m.f32(0x7670)){
        const float x=m.f32(0x184)*bits_f(0x40800000u)+m.f32(0x7670);
        m.putf(0x7670,x);
        if(x>=m.f32(0x7674))m.put32(0x7670,m.u32(0x7674)); // comiss/jb: unordered keeps x
    }else if(m.f32(0x7670)>zero){
        const float d=m.f32(0x184)*bits_f(0x3f0ccccdu);
        const float a=m.f32(0x7674)-d,b=m.f32(0x7670)-d;
        m.putf(0x7670,b);m.putf(0x7674,a);
        if(zero>a)m.putf(0x7674,zero);
        if(zero>b)m.putf(0x7670,zero);
    }
    if(e.f32(0x178)>e.f32(0x1c4))e.put32(0x1c4,e.u32(0x178));
    if(e.u32(0x1f4)!=0u){
        CourseProbe v=rd3(e,0x20);pc_unit_vector_40eeb0(v);
        const float s=e.f32(0x1c4); // 0x40F050(v,v,s)
        e.putf(0x20,static_cast<float>(static_cast<X>(s)*v.x));
        e.putf(0x24,static_cast<float>(static_cast<X>(s)*v.y));
        e.putf(0x28,static_cast<float>(static_cast<X>(s)*v.z));
    }else{
        const float unit=bits_f(0x38c90fdbu); // [0x628254] 2*pi/65536
        const float a1=static_cast<float>(static_cast<X>(e.i16(0x2e))*unit);
        const X sn=x87_sin(a1)*e.f32(0x1c4);
        e.putf(0x24,0.0f);
        e.putf(0x20,static_cast<float>(-sn));
        const float a2=static_cast<float>(static_cast<X>(e.i16(0x2e))*unit);
        const X cs=x87_cos(a2)*e.f32(0x1c4);
        e.putf(0x28,static_cast<float>(-cs));
    }
    e.put32(0x208,p.u32(0x10a0));
    e.put32(0x21c,p.u32(0x1644));
    const X t=static_cast<X>(e.f32(0x21c))*bits_f(0x4118c9ebu);
    e.put32(0x3c,0xffu);
    const auto iv=static_cast<std::uint32_t>(static_cast<std::uint64_t>(ftol2(t)));
    const float k=bits_f(0x4270cccdu);
    e.put32(0x20c,iv);e.put32(0x210,iv);e.put32(0x48,iv);
    const float sp=e.f32(0x1c4)*k;
    w.putf(0x608,sp);w.putf(0x514,sp);w.putf(0x420,sp);w.putf(0x32c,sp);
    const float r1=e.f32(0x1c4)/p.f32(0xb48)*k;
    w.putf(0x424,r1);w.putf(0x330,r1);
    const float r2=e.f32(0x1c4)/p.f32(0xb94)*k;
    w.putf(0x60c,r2);w.putf(0x518,r2);
}

float pc_manager_road_mu_4fc390(float mu,std::uint32_t,std::uint32_t flags){
    const float one=bits_f(0x3f800000u);
    auto cond=[&]{return mu>bits_f(0x3f733333u)?bits_f(0x3f333333u):bits_f(0x3fc00000u);};
    switch(flags){
    case 1u:return 0.0f;
    case 2u:case 0x100u:case 0x400u:case 0x100000u:case 0x200000u:case 0x400000u:case 0x800000u:return mu;
    case 4u:case 8u:case 0x10u:case 0x8000u:return cond();
    default:return one;
    }
}
float pc_manager_road_mu_lookup(void* ctx,std::uint32_t road_id,std::uint32_t flags){
    if(!ctx)throw std::invalid_argument("road-mu manager context");
    float mu;std::memcpy(&mu,ctx,4);return pc_manager_road_mu_4fc390(mu,road_id,flags);
}

PcRoadSideOffsets pc_road_side_offsets_4a4440(const PcRoadInfoResult& road,CourseProbe pos){
    check_road(road);
    if(!road.found)return {bits_f(0x40800000u),bits_f(0xc0800000u)};
    const Bytes r=road.info;
    const float half=bits_f(0x3f000000u);
    auto mid=[&](std::size_t a,std::size_t b){return static_cast<float>((static_cast<X>(r.f32(a))+r.f32(b))*half);};
    const float ax=mid(0x30,0x24),az=mid(0x38,0x2c),bx=mid(0x3c,0x48),bz=mid(0x44,0x50);
    std::array<std::uint8_t,8> d{};
    pc_direction_xz(bx,bz,ax,az,Bytes(d.data(),4),Bytes(d.data()+4,4));
    const Bytes dv(d.data(),8);const float dx=dv.f32(0),dz=dv.f32(4);
    const float zero=0.0f;
    const float s3=(r.f32(0x30)-pos.x)*dx+(r.f32(0x38)-pos.z)*dz;
    const float s4=(r.f32(0x48)-pos.x)*dx+(r.f32(0x50)-pos.z)*dz;
    return {zero-s3,zero-s4};
}
float pc_lane_ratio_4506b0(Bytes e,const PcRoadInfoResult& r5c,const PcRoadInfoResult& r184){
    const auto first=pc_road_side_offsets_4a4440(r5c,rd3(e,0x14));
    const auto second=pc_road_side_offsets_4a4440(r184,rd3(e,0x16c));
    const float zero=0.0f;
    // L2 = 0 - first.a4 (spilled before the second call); the second call
    // overwrites L1/L0 with its a3/a4.
    const float n=zero-first.a4;
    const float m=zero-second.a3;
    const float q=n/(n-m);
    float v=zero-q;
    const float lo=bits_f(0xc0000000u),hi=bits_f(0x40000000u);
    if(lo>v)v=lo;else if(v>hi)v=hi;
    return v;
}
float pc_lane_ratio_4506b0(Bytes e,PcRoadInfoContext& c){
    e.check(0x5c,12);e.check(0x184,12);
    const std::int32_t hint=e.i32(0x1c0);
    std::array<std::uint8_t,0x64> a{},b{};
    const auto r5c=query_road(a,e.sub(0x5c,12),hint,c);
    const auto r184=query_road(b,e.sub(0x184,12),hint,c);
    return pc_lane_ratio_4506b0(e,r5c,r184);
}
std::array<std::uint16_t,4> pc_time_split_449b30(std::uint32_t frames,float extra){
    X st=static_cast<X>(static_cast<std::int32_t>(frames));
    if(static_cast<std::int32_t>(frames)<0)st+=bits_f(0x4f800000u);
    st+=extra;st*=bits_f(0x3c881469u);
    const float stored=static_cast<float>(st);
    std::uint32_t eax=static_cast<std::uint32_t>(static_cast<std::uint64_t>(ftol2(st)));
    float frac=stored-static_cast<float>(static_cast<std::int32_t>(eax&0xffffu));
    const float thousand=bits_f(0x447a0000u);
    frac=frac*thousand;
    std::uint16_t sec=static_cast<std::uint16_t>(eax);
    if(0.0f>frac){--eax;frac=frac+thousand;sec=static_cast<std::uint16_t>(eax);}
    std::uint32_t ms=static_cast<std::uint32_t>(static_cast<std::uint64_t>(ftol2(static_cast<X>(frac))));
    std::uint16_t msw=static_cast<std::uint16_t>(ms);
    if(msw>=1000u){msw=static_cast<std::uint16_t>(ms-1000u);sec=static_cast<std::uint16_t>(sec+1u);}
    const std::uint16_t minutes=static_cast<std::uint16_t>(sec/60u);
    return {static_cast<std::uint16_t>(minutes/60u),static_cast<std::uint16_t>(minutes%60u),
            static_cast<std::uint16_t>(sec%60u),msw};
}
std::uint32_t pc_time_join_449c10(std::uint16_t h,std::uint16_t m,std::uint16_t s,std::uint16_t ms){
    return ((std::uint32_t(h)*60u+m)*60u+s)*1000u+ms;
}
namespace {
std::uint32_t allocate_tail(std::uint32_t v){return v==0xffffffffu?0u:v;}
bool live_451180(std::uint32_t request,const PcAllocateResultState& s){return request!=0u&&s.counter_656234<0x3c;}
std::uint32_t stored_451180(const PcAllocateResultState& s){
    return allocate_tail((s.mode_78024c==2u||s.mode_78024c==3u)?s.result_7d3738:s.result_7d3698);
}
std::uint32_t timed_451180(const PcAllocateResultState& s,float ratio){
    const auto t=pc_time_split_449b30(s.frames_7d3948,ratio);
    return allocate_tail(pc_time_join_449c10(t[0],t[1],t[2],t[3]));
}
}
std::uint32_t pc_allocate_result_451180(std::uint32_t request,const PcAllocateResultState& s,Bytes e,
                                        const PcRoadInfoResult& r5c,const PcRoadInfoResult& r184){
    if(!live_451180(request,s))return stored_451180(s);
    return timed_451180(s,pc_lane_ratio_4506b0(e,r5c,r184));
}
std::uint32_t pc_allocate_result_451180(std::uint32_t request,const PcAllocateResultState& s,Bytes e,PcRoadInfoContext& c){
    if(!live_451180(request,s))return stored_451180(s);
    return timed_451180(s,pc_lane_ratio_4506b0(e,c));
}
} // namespace outrun::driving
