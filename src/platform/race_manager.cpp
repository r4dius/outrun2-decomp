#include "platform/race_manager.hpp"
#include "driving/pc_x87.hpp"
#include <cmath>
#include <cstring>
#include <limits>
#include <stdexcept>
namespace outrun::platform {
namespace {
constexpr std::uint32_t kTop=RaceManagerState::base+sizeof(RaceManagerState);
constexpr float K62812C=0x1.1028d2p-6f;   // 3C881469: 1/60.2
constexpr float K5A460C=216.72000122070312f;
constexpr float K5A29EC=1000.0f;
constexpr std::uint32_t kFrameCap=0x57e3fu;
// .data 0x636904 (read only): per stage-key time correction, keys 0..14.
constexpr std::int16_t k636904[15]{0,0,-60,0,-60,-90,0,-60,-90,-120,0,-60,-90,-120,-150};
// .rdata 0x5A4590..0x5A4627 as (first,second) word pairs; entry n = 0x5A4518+n*4
// for n in 30..69. 450E10 reads [0x5A4590+s*4] (s<30) = entry s+30, or
// [0x5A4518+s*4] (s>=30) = entry s.
constexpr std::int16_t k5a4518[40][2]{
    {215,468},{230,434},{230,410},{273,453},{253,508},{225,455},{220,455},{278,445},{270,435},{261,482},
    {263,495},{284,457},{299,508},{221,416},{184,445},{182,378},{220,434},{228,400},{225,529},{272,515},
    {225,453},{200,490},{220,424},{275,445},{300,500},{242,538},{280,457},{228,467},{253,432},{163,363},
    {0,-16384},{-18350,17240},{16927,14729},{0,0},{205,0},{207,0},{207,0},{4,0},{7,0},{8,0}};

std::uint32_t rd32(const std::uint8_t* p,std::size_t o){std::uint32_t v;std::memcpy(&v,p+o,4);return v;}
std::int16_t rdi16(const std::uint8_t* p,std::size_t o){std::int16_t v;std::memcpy(&v,p+o,2);return v;}
float rdf(const std::uint8_t* p,std::size_t o){float v;std::memcpy(&v,p+o,4);return v;}
void wr16(std::uint8_t* p,std::size_t o,std::uint16_t v){std::memcpy(p+o,&v,2);}
void wrf(std::uint8_t* p,std::size_t o,float v){std::memcpy(p+o,&v,4);}
std::uint32_t bits(float f){std::uint32_t u;std::memcpy(&u,&f,4);return u;}
float fbits(std::uint32_t u){float f;std::memcpy(&f,&u,4);return f;}

// SSE scalar arithmetic is plain float. Only a NaN result is normalised to the
// x86 rules (NaN operand propagates quieted, first operand first; an invalid
// operation gives FFC00000) so non-x86 hosts store the same bits; the common
// path is one float operation and one predictable branch.
float quiet(float f){return fbits(bits(f)|0x00400000u);}
float sse_nan(float a,float b){return std::isnan(a)?quiet(a):std::isnan(b)?quiet(b):fbits(0xffc00000u);}
inline float sadd(float a,float b){const float r=a+b;return std::isnan(r)?sse_nan(a,b):r;}
inline float ssub(float a,float b){const float r=a-b;return std::isnan(r)?sse_nan(a,b):r;}
inline float smul(float a,float b){const float r=a*b;return std::isnan(r)?sse_nan(a,b):r;}
inline float sdiv(float a,float b){const float r=a/b;return std::isnan(r)?sse_nan(a,b):r;}
std::int32_t cvtt(float v){
    if(!(v>=-2147483648.0f&&v<2147483648.0f))return std::numeric_limits<std::int32_t>::min();
    return static_cast<std::int32_t>(v);
}
}

// ----- state ---------------------------------------------------------------
std::uint32_t RaceManagerState::u32(std::uint32_t pc) const{
    if(pc<base||pc+4u>kTop||pc+4u<pc)throw std::out_of_range("race manager read outside 7D3650..7D39FF");
    std::uint32_t v;std::memcpy(&v,reinterpret_cast<const std::uint8_t*>(this)+(pc-base),4);return v;
}
void RaceManagerState::put32(std::uint32_t pc,std::uint32_t v){
    if(pc<base||pc+4u>kTop||pc+4u<pc)throw std::out_of_range("race manager write outside 7D3650..7D39FF");
    std::memcpy(reinterpret_cast<std::uint8_t*>(this)+(pc-base),&v,4);
}
std::uint8_t RaceManagerState::u8(std::uint32_t pc) const{
    if(pc<base||pc>=kTop)throw std::out_of_range("race manager read outside 7D3650..7D39FF");
    return reinterpret_cast<const std::uint8_t*>(this)[pc-base];
}
void RaceManagerState::put8(std::uint32_t pc,std::uint8_t v){
    if(pc<base||pc>=kTop)throw std::out_of_range("race manager write outside 7D3650..7D39FF");
    reinterpret_cast<std::uint8_t*>(this)[pc-base]=v;
}
RaceEventWork RaceManagerWorld::work(std::uint32_t id) const{
    if(id>=event_work_799b38.size()||!event_work_799b38[id])throw std::out_of_range("race manager event work not provided");
    return event_work_799b38[id];
}
std::uint8_t RaceManagerWorld::open(std::uint32_t id) const{
    if(id>=event_open_79fb48.size())throw std::out_of_range("race manager event open byte not provided");
    return event_open_79fb48[id];
}

// ----- stage lookup (44C8D0 / 44C940) ---------------------------------------
namespace {
const std::uint8_t* find_stage_44c8d0(const RaceManagerWorld& w,std::uint32_t key){
    if(w.stage_count_7d33c4<=0)return nullptr;
    if(!w.stage_records_7d33bc)throw std::out_of_range("stage records not provided");
    for(std::int32_t k=0;k<w.stage_count_7d33c4;++k){
        const auto* r=w.stage_records_7d33bc+std::size_t(k)*0x78u;
        if(rd32(r,4)==key)return r;
    }
    return nullptr;
}
std::uint32_t level_of(RaceManagerWorld& w,std::uint32_t key){return race_stage_level_44c940(w,key);}
std::uint32_t difficulty_of(RaceManagerWorld& w,RaceManagerServices& s,std::int32_t variant){
    if(s.pc_43f860())return 0;
    return variant==4?2u:std::uint32_t(w.difficulty_7c24c0);
}
std::uint32_t route_at(const RaceManagerState& st,std::int32_t level){return st.u32(0x7d39a0u+std::uint32_t(level)*4u);}
// x87 code runs on the X87 model (driving/pc_x87.hpp): every arithmetic op
// rounds like the PC under the live precision control (0x007F in game).
using driving::X87;
// 0x582194 (_ftol2) on the x87 register value: n = FISTP (round to nearest);
// 0 and the indefinite 8000000000000000 are returned as is; otherwise n moves
// one step toward zero when the float store of FSUB(x,n) has the opposite sign
// of x. (Equals truncation except at the round-to-nearest overflow edge.)
std::int64_t pc_ftol2(X87 x){
    constexpr std::int64_t indefinite=std::numeric_limits<std::int64_t>::min();
    const auto v=x.v;
    if(std::isnan(v))return indefinite;
    const auto r=std::nearbyint(v);
    if(!(r<9223372036854775808.0&&r>=-9223372036854775808.0))return indefinite;
    std::int64_t n=static_cast<std::int64_t>(r);
    if(n==0||n==indefinite)return n;
    const float diff=driving::x87_float(x-X87(r));
    if(!std::signbit(driving::x87_float(x))){if(diff<0.0f)--n;}
    else if(diff>0.0f)++n;
    return n;
}
struct X87Seconds{float rounded;std::int64_t truncated;};
// 449B30: FILD m32 (+ FADD m32 2^32 when negative: unsigned frames); FADD m32
// frac; FMUL m32 K; FST m32; _ftol2 of the register.
X87Seconds x87_frame_seconds(std::uint32_t frames,float frac){
    X87 x=X87(static_cast<int>(frames));   // FILD m32 (signed)
    if(std::int32_t(frames)<0)x=x+X87(4294967296.0f);
    x=x+X87(frac);
    x=x*X87(K62812C);
    return {driving::x87_float(x),pc_ftol2(x)};
}
// 450AC0: FILD m32 pos; FSTP m32; FILD m32 end (u16); FDIVR m32; FSTP m32.
// 0/0 is the x87 default NaN (FFC00000), made explicit for non-x86 hosts.
float x87_ratio_u16(std::int16_t pos,std::uint16_t end){
    if(end==0){
        if(pos==0)return fbits(0xffc00000u);
        return pos>0?std::numeric_limits<float>::infinity():-std::numeric_limits<float>::infinity();
    }
    return driving::x87_float(X87(float(pos))/X87(int(end)));
}
}
std::uint32_t race_stage_level_44c940(RaceManagerWorld& w,std::uint32_t key){
    if(key==w.stage_cache_key_635f2c)return w.stage_cache_value_635f30;
    const auto* r=find_stage_44c8d0(w,key);
    const std::uint32_t value=r?rd32(r,8):0u;
    w.stage_cache_key_635f2c=key;w.stage_cache_value_635f30=value;
    return value;
}

// ----- 44DC70 GetExtendedTime ----------------------------------------------
std::int32_t race_extended_time_44dc70(RaceManagerWorld& w,RaceManagerServices& s,std::int32_t variant,std::int32_t key,std::int32_t difficulty){
    const std::uint8_t selection=s.pc_48b1a0();
    if(w.course_10!=0)return std::int32_t(std::uint32_t(w.course_10)*0x3cu);
    std::int32_t row;
    if(variant==1)row=0;
    else if(variant==2||variant==5)row=1;
    else if(variant==0||variant==7)row=2;
    else row=0;
    std::int32_t stage_key=key;std::uint32_t level;
    if(key>=0&&key<15)level=level_of(w,std::uint32_t(key));
    else{stage_key=0xe;level=level_of(w,0xeu);}   // inline copy of the 44C940 cache logic
    const std::uint32_t idx=std::uint32_t(row*15)+level;
    const std::uint32_t entry=std::uint32_t(difficulty)+idx*5u;
    if(!w.time_table_7d2e98)throw std::out_of_range("area time table not provided");
    if(entry>=3u*15u*5u)throw std::out_of_range("area time table index outside 7D2E98 table");
    std::int32_t time=w.time_table_7d2e98[entry];
    if(s.pc_43f960()==0&&s.pc_456d60()<=1)time+=k636904[stage_key];
    if(selection!=1&&level!=0)time+=0x3c;
    return time;
}

// ----- route choices ---------------------------------------------------------
std::uint32_t race_pack_route_4503d0(const RaceManagerState& st){
    std::uint32_t r=0;
    for(int n=13;n>=0;--n)r=(r<<2)|(st.route_7d39a0[n]&3u);
    return r;
}
void race_unpack_route_450490(RaceManagerState& st,std::uint32_t packed){
    for(unsigned n=0;n<14;++n)st.route_7d39a0[n]=(packed>>(2*n))&3u;
}
void race_set_route_451140(RaceManagerState& st,RaceManagerWorld& w,RaceManagerServices& s,std::int32_t level,std::uint32_t value){
    if(level>=14)return;
    // Protected (jmp [1039A20]): the VM stores the choice; the relocated tail
    // (bytes 451154..451176) is: if [780258]==4 { 456D20(4503D0());
    // 450490(456D10()); }. Measured against the original by the oracle.
    st.put32(0x7d39a0u+std::uint32_t(level)*4u,value);
    if(w.variant_780258!=4)return;
    s.pc_456d20(race_pack_route_4503d0(st));
    race_unpack_route_450490(st,s.pc_456d10());
}
std::uint32_t race_get_route_451350(RaceManagerState& st,RaceManagerWorld& w,RaceManagerServices& s,std::int32_t level){
    const bool gate=s.pc_4957f0()||s.pc_48b310()||s.pc_495490();
    if(gate&&route_at(st,level)==2u){
        const std::int32_t a=w.course_2c,b=w.course_30;
        if(a==-1){if(b!=-1)race_set_route_451140(st,w,s,level,1);}
        else if(b==-1)race_set_route_451140(st,w,s,level,0);
    }
    return level<14?route_at(st,level):2u;
}
void race_store_route_456cf0(const RaceManagerState& st,RaceManagerServices& s){s.pc_456720(race_pack_route_4503d0(st));}

// ----- CalcFrame2Time / time ----------------------------------------------------
RaceFrameTime race_frames_to_time_449b30(std::uint32_t frames,float fraction){
    const auto x=x87_frame_seconds(frames,fraction);
    std::uint32_t ax=std::uint32_t(x.truncated);
    float rest=smul(ssub(x.rounded,float(std::uint16_t(ax))),K5A29EC);
    std::uint16_t seconds=std::uint16_t(ax);
    if(0.0f>rest){--ax;rest=sadd(rest,K5A29EC);seconds=std::uint16_t(ax);}
    const std::uint32_t msv=std::uint32_t(pc_ftol2(X87(rest)));   // FLD m32; _ftol2
    std::uint16_t ms=std::uint16_t(msv);
    if(ms>=1000u){ms=std::uint16_t(msv-1000u);seconds=std::uint16_t(seconds+1u);}
    RaceFrameTime t{};
    std::uint16_t minutes=std::uint16_t(std::int32_t(seconds)/60);
    seconds=std::uint16_t(std::int32_t(seconds)%60);
    t.hours=std::uint16_t(std::int32_t(minutes)/60);
    t.minutes=std::uint16_t(std::int32_t(minutes)%60);
    t.seconds=seconds;t.ms=ms;
    return t;
}
std::int32_t race_time_ms_449c10(std::uint16_t h,std::uint16_t m,std::uint16_t s,std::uint16_t ms){
    std::uint32_t v=std::uint32_t(h)*60u+m;v=v*60u+s;return std::int32_t(v*1000u+ms);
}

// ----- 451180 race time (ms) -----------------------------------------------------
// With a request and 48B350: the last stage frames 7D3948 with the player's
// sector progress (4506B0 on [799D18]) as CalcFrame2Time; otherwise the
// stored time 7D3738 (43F960) / 7D3698. -1 reads as 0.
std::uint32_t race_time_451180(const RaceManagerState& st,RaceManagerServices& s,RaceEventWork player,std::uint32_t request){
    if(request&&s.pc_48b350()){
        const float frac=race_sector_progress_4506b0(s,player);
        const auto t=race_frames_to_time_449b30(st.last_stage_frames_7d3948,frac);
        const std::int32_t v=race_time_ms_449c10(t.hours,t.minutes,t.seconds,t.ms);
        return v==-1?0u:std::uint32_t(v);
    }
    const std::uint32_t v=s.pc_43f960()?st.u32(0x7d3738u):st.u32(0x7d3698u);
    return v==0xffffffffu?0u:v;
}

// ----- 4506B0 sector progress ---------------------------------------------------
float race_sector_progress_4506b0(RaceManagerServices& s,RaceEventWork e){
    const auto first=s.pc_4a4440(rd32(e,0x1c0),e+0x5c,e+0x14);
    const float a=ssub(0.0f,first.second);
    const auto second=s.pc_4a4440(rd32(e,0x1c0),e+0x184,e+0x16c);
    const float b=ssub(0.0f,second.first);
    float r=ssub(0.0f,sdiv(a,ssub(a,b)));
    if(-2.0f>r)r=-2.0f;
    else if(r>2.0f)r=2.0f;
    return r;
}

// ----- 44FE80 rank table -------------------------------------------------------------
void race_rank_table_44fe80(RaceManagerState& st,RaceManagerWorld& w,RaceManagerServices& s){
    if(s.pc_456d60()<=1){st.rank_event_7d388c[0]=8;return;}
    for(std::int32_t id=8;id<w.player_events_680ad4+8;++id){
        if((w.open(std::uint32_t(id))&3u)!=2u)continue;
        const std::uint8_t rank=s.pc_45a2b0(w.work(std::uint32_t(id))[0x10]);
        st.put32(0x7d388cu+std::uint32_t(rank)*4u,std::uint32_t(id));
    }
}

// ----- 4513C0 goal check -------------------------------------------------------------
namespace {
void latch_speed(RaceManagerState& st,RaceManagerWorld& w,RaceEventWork e){
    st.stage_speed_latch_7d39f8=st.average_speed_7d3888;
    const auto level=level_of(w,rd32(e,0x68));
    st.put32(0x7d3838u+level*4u,bits(st.stage_speed_latch_7d39f8));
}
}
void race_check_goal_4513c0(RaceManagerState& st,RaceManagerWorld& w,RaceManagerServices& s,RaceEventWork e){
    if(st.flags_7d39f0&4u)return;
    const std::int32_t variant=w.variant_780258;
    bool network=false;
    if(s.pc_55a930()!=0&&w.network_7f95a8!=0)network=true;
    else if(variant==7&&s.pc_48b350())network=true;
    if(network){
        if(s.pc_46c500()==0&&variant!=7){
            if(rd32(e,0x5c)!=0&&rd32(e,0x184)==0){st.flags_7d39f0|=4u;latch_speed(st,w,e);}
        }else{
            const std::uint32_t level=level_of(w,rd32(e,0x68));
            if(rd32(e,0x5c)!=0&&rdi16(e,0x64)==0x38&&rdi16(e,0x18c)!=0x38){
                const auto m=s.pc_44bec0();s.pc_409f90(m);s.pc_40a240();
                const RaceVec3 v=s.pc_40a7d0(e+0xd28);s.pc_40a010();
                if(0.0f>v.x){s.pc_44b900(0);race_set_route_451140(st,w,s,std::int32_t(level),0);s.pc_46c410(1);}
                else{s.pc_44b900(1);race_set_route_451140(st,w,s,std::int32_t(level),1);s.pc_46c410(2);}
            }
            const auto r=race_get_route_451350(st,w,s,std::int32_t(level));
            if(rd32(e,0x5c)!=0&&r==1u&&rdi16(e,0x64)>0x96){
                st.flags_7d39f0|=4u;s.pc_46c400(r);latch_speed(st,w,e);
            }
        }
    }
    if(!w.course_present_7d3188)return;
    if(!s.pc_44b7b0(0))return;
    if(rd32(e,0x5c)==0||rd32(e,0x184)!=0)return;
    st.flags_7d39f0|=4u;latch_speed(st,w,e);
}

// ----- 44FF20 stage clear -------------------------------------------------------------
bool race_check_stage_clear_44ff20(RaceManagerState& st,RaceManagerWorld& w,RaceManagerServices& s,RaceEventWork e){
    const std::uint8_t b=std::uint8_t(st.branch_state_7d38e8);
    if(!(b&2u)||(b&4u)||rd32(e,0x5c)==1u)return false;
    const std::uint16_t lamp=std::uint16_t(rdi16(e,0x25c));
    if(!(lamp&0x10u)&&!(lamp&0x100u))return false;
    st.flags_7d39f0|=8u;++st.stage_clear_count_7d3950;
    const std::uint32_t next=s.pc_44bdd0();
    std::memcpy(e+0x68,&next,4);wrf(e,0x2b0,0.0f);
    st.branch_state_7d38e8=(st.branch_state_7d38e8&~2u)|4u;
    st.flags_7d39f0|=2u;
    st.stage_speed_latch_7d39f8=st.average_speed_7d3888;
    st.last_stage_frames_7d3948=st.stage_frames_7d3930;
    st.stage_frames_7d3930=0;st.speed_sum_7d3938=0.0f;
    st.put32(0x7d3834u+level_of(w,rd32(e,0x68))*4u,bits(st.stage_speed_latch_7d39f8));
    st.put32(0x7d38f4u+level_of(w,rd32(e,0x68))*4u,rd32(e,0x68));
    const std::uint8_t rank=s.pc_45a2b0(e[0x10]);
    st.put8(0x7d39dfu+level_of(w,rd32(e,0x68)),rank);
    st.put32(0x7d38a8u+level_of(w,rd32(e,0x68))*4u,1u);
    bool latch=false;
    if(s.pc_4957f0()){
        s.pc_495b60();
        if(s.pc_495b10()==2u){
            const std::uint32_t count=s.pc_495b70();
            if(count>=s.pc_495b30()){st.flags_7d39f0|=4u;latch=true;}
        }
    }else if(s.pc_4b00d0()){
        const auto v=s.pc_4b0190();s.pc_4b0110(rd32(e,0x68),v);
    }else if(s.pc_48b310())s.pc_48b3a0();
    else if(s.pc_495490())s.pc_495610();
    else if(w.area_7d33b0==0)latch=true;
    if(latch)latch_speed(st,w,e);
    if(!s.pc_4957f0())st.put32(0x7d39a0u+level_of(w,rd32(e,0x68))*4u,2u);
    return true;
}

// ----- 451220 / 4512C0 branch off --------------------------------------------------------
namespace {
// 451220 passes the player's E+5C to 44B7B0, 4512C0 passes 0.
void branch_off(RaceManagerState& st,RaceManagerWorld& w,RaceManagerServices& s,RaceEventWork car,bool player){
    const std::uint32_t level=level_of(w,rd32(w.work(rd32(car,0)),0x68));
    if(s.pc_44b7b0(player?rd32(car,0x5c):0u))return;
    if(!(car[0x25d]&0x0cu))return;
    st.branch_state_7d38e8=(st.branch_state_7d38e8&~8u)|1u;
    if(route_at(st,std::int32_t(level))!=2u)return;
    race_set_route_451140(st,w,s,std::int32_t(level),(car[0x25d]&8u)?1u:0u);
}
}
void race_check_branch_off_451220(RaceManagerState& st,RaceManagerWorld& w,RaceManagerServices& s,RaceEventWork e){
    if(w.variant_780258==4){s.pc_45a0a0();race_unpack_route_450490(st,s.pc_456d10());}
    const std::uint8_t b=std::uint8_t(st.branch_state_7d38e8);
    if(!(b&8u)||(b&1u))return;
    branch_off(st,w,s,e,true);
}
void race_check_branch_off_4512c0(RaceManagerState& st,RaceManagerWorld& w,RaceManagerServices& s,RaceEventWork car){
    if(s.pc_4b00d0())return;
    const std::uint8_t b=std::uint8_t(st.branch_state_7d38e8);
    if(!(b&8u)||(b&1u))return;
    branch_off(st,w,s,car,false);
}

// ----- 450CC0 time extend -----------------------------------------------------------------
void race_check_time_extend_450cc0(RaceManagerState& st,RaceManagerWorld& w,RaceManagerServices& s,RaceEventWork e){
    std::uint32_t key=rd32(e,0x68);
    const std::uint32_t difficulty=difficulty_of(w,s,w.variant_780258);
    if(s.pc_456d60()>1u||w.variant_780258==4){
        if(st.flags_7d39f0&8u)s.pc_4f2b20(cvtt(smul(float(std::int32_t(st.last_stage_frames_7d3948)),K62812C)));
        const std::uint32_t r=s.pc_456dc0(st.extend_count_7d3990,key,(st.flags_7d39f0>>3)&1u);
        if(r)st.flags_7d39f0|=8u;else st.flags_7d39f0&=~8u;
    }
    if(!(st.flags_7d39f0&8u))return;
    ++st.extend_count_7d3990;
    const std::int32_t variant=w.variant_780258;
    w.flag_84490c=1;
    std::int32_t t;
    if(variant==6)t=std::int32_t(std::uint32_t(st.time_7d394c)+0xf3cu);
    else if(s.pc_46c500())t=race_extended_time_44dc70(w,s,variant,std::int32_t(key),std::int32_t(difficulty));
    else{
        const std::int32_t v=variant==7?0:variant;
        const auto add=race_extended_time_44dc70(w,s,v,std::int32_t(key),std::int32_t(difficulty));
        t=std::int32_t(std::uint32_t(st.time_7d394c)+std::uint32_t(add));
    }
    st.time_7d394c=std::int32_t(std::uint32_t(t)-std::uint32_t(st.time_adjust_7d3880));
    st.time_adjust_7d3880=0;
    if(w.variant_780258!=4)s.pc_4f2b20(cvtt(smul(float(std::int32_t(st.last_stage_frames_7d3948)),K62812C)));
}

// ----- 450AC0 game timer -------------------------------------------------------------------
void race_check_game_timer_450ac0(RaceManagerState& st,RaceManagerWorld& w,RaceManagerServices& s){
    const std::int32_t mode=w.mode_78026c;   // edi=[0x799D18] is loaded here but only dereferenced below
    if(std::int16_t(s.pc_49b2d0())>60)return;
    bool timeup=false;
    const bool heart=(mode==0x10||mode==0x12||mode==0x13);
    bool saved_mode=false;
    if(heart){
        if(w.variant_780258==4)saved_mode=true;
        else{
            if(st.flags_7d39f0&4u)return;
            if(w.time_decrement_637911)st.time_7d394c=std::int32_t(std::uint32_t(st.time_7d394c)-1u);
            timeup=st.time_7d394c<=0;
        }
    }else{
        if(w.variant_780258!=4)return;
        saved_mode=true;
    }
    if(saved_mode){
        if(w.time_decrement_637911)st.time_7d394c=std::int32_t(std::uint32_t(st.time_7d394c)-1u);
        if(s.pc_456d50()==1u){
            const std::int32_t v=s.pc_456d40();
            st.time_7d394c=v;timeup=v<=0;
        }else{
            if(st.flags_7d39f0&4u)return;
            timeup=s.pc_456de0(st.time_7d394c)!=0;
        }
    }
    if(std::int32_t(st.total_frames_7d39dc)<std::int32_t(kFrameCap))++st.total_frames_7d39dc;
    else st.total_frames_7d39dc=kFrameCap;
    if(std::int32_t(st.stage_frames_7d3930)<std::int32_t(kFrameCap)){
        st.speed_sum_7d3938=sadd(rdf(w.work(8),0x1c4),st.speed_sum_7d3938);
        ++st.stage_frames_7d3930;
        st.average_speed_7d3888=smul(sdiv(st.speed_sum_7d3938,float(std::int32_t(st.stage_frames_7d3930))),K5A460C);
    }else st.stage_frames_7d3930=kFrameCap;
    if(!timeup)return;
    RaceEventWork car=w.work(8);
    const std::uint32_t key=rd32(car,0x68);
    const std::uint32_t level=level_of(w,key);
    const std::uint32_t course=rd32(car,0x5c);
    const std::int16_t pos=rdi16(car,0x64);
    const std::uint16_t end=s.pc_43d470(course);
    const float ratio=x87_ratio_u16(pos,end);
    s.pc_4aeef0(course,level,ratio);
    if(!(st.flags_7d39f0&1u)){
        // 49A650 (empty) receives the stripped debug arguments; nothing to do.
        st.goal_key_7d3940=std::uint16_t(key);
        st.goal_first_stage_7d3740=course==0?1u:0u;
        st.goal_ratio_7d3878=ratio;
        s.pc_458450();
    }
    st.flags_7d39f0=(st.flags_7d39f0&~8u)|1u;
}

// ----- 450170 game over -------------------------------------------------------------------
void race_check_game_over_450170(RaceManagerState& st,RaceManagerWorld& w,RaceManagerServices& s){
    const std::int32_t v=w.variant_780258;
    if(v==3||v==4){
        if(st.over_state_7d38f0!=7u&&st.over_state_7d38f0!=6u){
            const std::uint8_t f=std::uint8_t(st.flags_7d39f0);
            if(f&4u)st.over_state_7d38f0=3;
            else if(f&1u){
                const auto r=s.pc_456d50();
                st.over_state_7d38f0=4;
                if(r==0)st.over_state_7d38f0=5;
            }
        }
    }else{
        const std::uint8_t f=std::uint8_t(st.flags_7d39f0);
        if(f&1u)st.over_state_7d38f0=2;
        else if(f&4u){
            st.over_state_7d38f0=1;
            if(v==7||s.pc_4962a0())s.pc_467190();
        }
    }
    if(st.flags_7d39f0&4u){
        s.pc_453080(1);
        const std::uint32_t slot=s.pc_4ef710();
        st.put32(0x7d38acu+slot*4u,1u);
    }
}

// ----- 450E10 sector time -------------------------------------------------------------------
namespace {
void split_time(RaceManagerState& st,std::int32_t level,std::int32_t value){
    const std::uint32_t at=0x7d3954u+std::uint32_t(level)*4u;
    st.put32(at,std::uint32_t(value));
    for(std::int32_t k=0;k<level;++k)st.put32(at,st.u32(at)-st.u32(0x7d3954u+std::uint32_t(k)*4u));
}
std::pair<std::uint16_t,std::uint16_t> sector_bounds(RaceManagerServices& s,std::int32_t n){
    const std::uint32_t sw=std::uint32_t(n-0x3c);
    if(sw<=5u){
        switch(sw){
        case 0:case 2:return {0xd7,0x1d4};
        case 1:case 3:return {0xb6,0x17a};
        case 4:{const std::uint16_t end=s.pc_43d470(0);return {std::uint16_t(end-0x1d4),std::uint16_t(end-0xd7)};}
        default:{const std::uint16_t end=s.pc_43d470(0);return {std::uint16_t(end-0x17a),std::uint16_t(end-0xb6)};}
        }
    }
    if(n>=0x1e){
        if(n>=70)throw std::out_of_range("450E10 stage number outside embedded 5A4518 table");
        const std::uint16_t end=s.pc_43d470(0);
        const auto& e=k5a4518[n-30];
        return {std::uint16_t(end-std::uint16_t(e[1])),std::uint16_t(end-std::uint16_t(e[0]))};
    }
    if(n<0)throw std::out_of_range("450E10 negative stage number");
    const auto& e=k5a4518[n];
    return {std::uint16_t(e[0]),std::uint16_t(e[1])};
}
}
void race_check_sector_time_450e10(RaceManagerState& st,RaceManagerWorld& w,RaceManagerServices& s,RaceEventWork e){
    st.sector_new_7d3994=0;st.sector_set_7d3998=0;
    if(std::int32_t(st.sector_banner_7d3934)>0){
        --st.sector_banner_7d3934;
        if(st.sector_banner_7d3934==0&&s.pc_48b310()&&s.pc_48b350())s.pc_465f20();
    }
    st.sector_zone_prev_7d399c=st.sector_zone_7d399d;
    if(rd32(e,0x5c)!=0)st.sector_zone_7d399d=3;
    else{
        const std::int32_t n=s.pc_44dc50(rd32(e,0x68));
        const auto [lo,hi]=sector_bounds(s,n);
        const std::int32_t pos=rdi16(e,0x64);
        if(pos<std::int32_t(lo))st.sector_zone_7d399d=0;
        else st.sector_zone_7d399d=std::int8_t((pos>=std::int32_t(hi)?1:0)+1);
    }
    std::int32_t level=0,sector;
    const bool attack=s.pc_48b310()&&s.pc_48b350();
    if(attack){
        sector=std::int32_t(st.sector_zone_7d399d)-1;
        if(sector<0){if(s.pc_48b330()==0)return;sector=3;}
    }else{
        level=std::int32_t(level_of(w,rd32(e,0x68)));
        sector=std::int32_t(st.sector_zone_7d399d)-1;
        if(sector<0){level=std::int32_t(std::uint32_t(level)-1u);if(level<0)return;sector=3;}
    }
    const std::uint32_t idx=std::uint32_t(sector)+std::uint32_t(level)*4u;
    st.sector_level_7d3944=level;st.sector_index_7d3874=sector;
    if(st.u32(0x7d3650u+idx*4u)!=0xffffffffu)return;
    std::uint32_t frames;
    if(s.pc_48b310()&&s.pc_48b350())frames=sector==3?st.last_stage_frames_7d3948:st.stage_frames_7d3930;
    else frames=st.total_frames_7d39dc;
    const float frac=race_sector_progress_4506b0(s,e);
    const auto t=race_frames_to_time_449b30(frames,frac);
    const std::int32_t total=race_time_ms_449c10(t.hours,t.minutes,t.seconds,t.ms);
    st.put32(0x7d3650u+idx*4u,std::uint32_t(total));
    st.sector_banner_7d3934=300;st.sector_set_7d3998=1;
    const auto t2=race_frames_to_time_449b30(st.stage_frames_7d3930,frac);
    st.put32(0x7d3748u+idx*4u,std::uint32_t(race_time_ms_449c10(t2.hours,t2.minutes,t2.seconds,t2.ms)));
    const bool final_stage=s.pc_44b7b0(0)!=0;
    if(final_stage&&sector==2){
        split_time(st,level,total);
        st.sector_ms_7d393c=total;st.split_ms_7d387c=std::int32_t(st.u32(0x7d3954u+std::uint32_t(level)*4u));
        st.sector_new_7d3994=1;st.sector_goal_7d39f4=1;
        return;
    }
    if(sector==3){
        split_time(st,level,total);
        st.split_ms_7d387c=std::int32_t(st.u32(0x7d3954u+std::uint32_t(level)*4u));
        st.sector_new_7d3994=1;
        if(s.pc_48b310())s.pc_48b3d0();
    }
    st.sector_ms_7d393c=total;
}

// ----- event functions -----------------------------------------------------------------------
void race_manager_init_450790(RaceManagerState& st,RaceManagerWorld& w,RaceManagerServices& s){
    const std::int32_t count=w.player_events_680ad4;
    for(auto& r:st.route_7d39a0)r=2;
    st.flags_7d39f0=0;st.branch_state_7d38e8=4;
    for(std::int32_t k=0;k<count;++k)st.put32(0x7d388cu+std::uint32_t(k)*4u,0x19au);
    for(auto& r:st.stage_rank_7d39e0)r=-1;
    s.pc_46fab0();s.pc_47dac0();
    RaceEventWork last=nullptr;
    for(std::uint32_t id=8;id<32;++id){
        last=w.work(id);
        const std::uint32_t key=s.pc_44c2c0();
        std::memcpy(last+0x68,&key,4);
    }
    const std::int32_t variant=w.variant_780258;
    const std::uint32_t difficulty=difficulty_of(w,s,variant);
    const std::uint32_t key=s.pc_44c2c0();
    st.time_7d394c=race_extended_time_44dc70(w,s,variant,std::int32_t(key),std::int32_t(difficulty));
    st.total_frames_7d39dc=0;st.stage_frames_7d3930=0;st.last_stage_frames_7d3948=0;st.split_ms_7d387c=0;
    st.speed_sum_7d3938=0.0f;st.average_speed_7d3888=0.0f;st.stage_speed_latch_7d39f8=0.0f;
    st.stage_clear_count_7d3950=0;st.extend_count_7d3990=0;st.over_state_7d38f0=0;st.time_adjust_7d3880=0;st.display_count_7d38ec=0;
    s.pc_4f0dd0();
    for(auto& v:st.stage_speed_7d3838)v=0.0f;
    for(auto& v:st.stage_split_7d3954)v=0;
    for(auto& v:st.stage_key_7d38f4)v=0xffffffffu;
    const std::uint32_t last_key=rd32(last,0x68);
    st.put32(0x7d38f4u+level_of(w,last_key)*4u,last_key);
    if(w.variant_780258==4)race_store_route_456cf0(st,s);
    s.pc_4f2ac0();
    for(auto& v:st.stage_flag_7d38ac)v=0;
    for(auto& v:st.sector_ms_7d3650)v=-1;
    st.goal_key_7d3940=0;st.goal_first_stage_7d3740=0;st.sector_zone_prev_7d399c=0;st.sector_zone_7d399d=0;
    st.sector_index_7d3874=0;st.sector_level_7d3944=0;st.sector_banner_7d3934=0;st.sector_ms_7d393c=0;
    st.sector_goal_7d39f4=0;st.countdown_frames_7d3884=0;st.goal_ratio_7d3878=0.0f;st.w7d38e4=0;
}
void race_manager_control_4515b0(RaceManagerState& st,RaceManagerWorld& w,RaceManagerServices& s){
    st.flags_7d39f0&=0xfffffff5u;
    race_rank_table_44fe80(st,w,s);
    RaceEventWork e=w.work(8);
    race_check_goal_4513c0(st,w,s,e);
    const bool cleared=race_check_stage_clear_44ff20(st,w,s,e);
    race_check_branch_off_451220(st,w,s,e);
    if(w.ranking_80fb14!=0){
        for(std::int32_t id=8;id<w.player_events_680ad4+8;++id){
            if((w.open(std::uint32_t(id))&3u)!=2u)continue;
            RaceEventWork car=w.work(std::uint32_t(id));
            if(!(car[4]&1u))race_check_branch_off_4512c0(st,w,s,car);
            s.pc_476760(car);
        }
    }
    race_check_time_extend_450cc0(st,w,s,e);
    race_check_game_timer_450ac0(st,w,s);
    if(std::int16_t(s.pc_49b2d0())<1)++st.countdown_frames_7d3884;
    race_check_game_over_450170(st,w,s);
    std::uint32_t b=st.branch_state_7d38e8;
    if((b&1u)&&!(b&2u)&&std::int8_t(e[0x25c])<0){
        st.flags_7d39f0|=0x20u;b=(b&~1u)|2u;st.branch_state_7d38e8=b;
    }
    if((b&4u)&&!(b&8u)&&rd32(e,0x5c)!=1u){
        if(std::int32_t(rdi16(e,0x64))>std::int32_t(w.course_14_5c)+1){st.flags_7d39f0|=0x40u;b=(b&~4u)|8u;}
    }
    if((b&0x10u)&&(b&1u)&&!(b&2u)){st.flags_7d39f0|=0x20u;b=(b&~1u)|2u;}
    if((b&0x20u)&&(b&4u)&&!(b&8u)){st.flags_7d39f0|=0x40u;b=(b&~4u)|8u;}
    st.branch_state_7d38e8=b&0xffffffcfu;
    s.pc_47ec00();s.pc_4f0e40();s.pc_4f2df0();   // then 49A650 (empty)
    race_check_sector_time_450e10(st,w,s,e);
    if(!cleared||!s.pc_48b310()||!s.pc_48b350())return;
    s.pc_467e00();
    wr16(e,0x260,std::uint16_t(rdi16(e,0x64)));wr16(e,0x25e,0);
    const std::int32_t variant=w.variant_780258;
    for(auto& v:st.sector_ms_7d3650)v=-1;
    const std::uint32_t difficulty=difficulty_of(w,s,variant);
    const std::uint32_t key=s.pc_44c2c0();
    st.time_7d394c=race_extended_time_44dc70(w,s,variant,std::int32_t(key),std::int32_t(difficulty));
    if(s.pc_48b330())s.pc_48b340();
}
void race_manager_display_44fe00(RaceManagerState& st){++st.display_count_7d38ec;}
void race_manager_destroy_44fe10(RaceManagerState& st,RaceManagerServices& s){
    s.pc_47dc00();   // then 49A650 (empty)
    st.flags_7d39f0=0;st.stage_clear_count_7d3950=0;st.extend_count_7d3990=0;
}

// ----- accessors -------------------------------------------------------------------------
std::uint32_t race_countdown_frames_44fdf0(const RaceManagerState& st){return st.countdown_frames_7d3884;}
void race_set_time_44fe30(RaceManagerState& st,std::int32_t v){st.time_7d394c=v;}
std::int32_t race_time_44fe40(const RaceManagerState& st){return st.time_7d394c;}
void race_set_flag0_44fe50(RaceManagerState& st,std::uint32_t v){st.flags_7d39f0^=(st.flags_7d39f0^v)&1u;}
std::uint32_t race_flag0_44fe70(const RaceManagerState& st){return st.flags_7d39f0&1u;}
void race_set_flag2_44fef0(RaceManagerState& st,std::uint32_t v){st.flags_7d39f0^=(st.flags_7d39f0^(v*4u))&4u;}
std::uint32_t race_flag2_44ff10(const RaceManagerState& st){return (st.flags_7d39f0>>2)&1u;}
void race_set_flag1_450110(RaceManagerState& st,std::uint32_t v){st.flags_7d39f0^=(st.flags_7d39f0^(v*2u))&2u;}
std::uint32_t race_flag1_450130(const RaceManagerState& st){return (st.flags_7d39f0>>1)&1u;}
void race_set_flag3_450140(RaceManagerState& st,std::uint32_t v){st.flags_7d39f0^=(st.flags_7d39f0^(v*8u))&8u;}
std::uint32_t race_flag3_450160(const RaceManagerState& st){return (st.flags_7d39f0>>3)&1u;}
void race_set_over_state_450230(RaceManagerState& st,std::uint32_t v){st.over_state_7d38f0=v;}
std::uint32_t race_over_state_450240(const RaceManagerState& st){return st.over_state_7d38f0;}
std::int32_t race_next_stage_key_450250(RaceManagerWorld& w,std::uint16_t preset,std::int32_t key,std::int32_t branch){
    if(preset==2||preset==3)return key+1;
    std::int32_t r;
    switch(race_stage_level_44c940(w,std::uint32_t(key))){
    case 0:r=1;break;
    case 1:r=key+2;if(r==-1)return r;break;
    case 2:r=key+3;if(r==-1)return r;break;
    case 3:r=key+4;if(r==-1)return r;break;
    default:return -1;
    }
    if(branch==1)++r;
    return r;
}
std::uint32_t race_flag5_4502c0(const RaceManagerState& st){return (st.flags_7d39f0>>5)&1u;}
void race_clear_flag5_4502d0(RaceManagerState& st){st.flags_7d39f0&=~0x20u;}
void race_request_lamp_4502e0(RaceManagerState& st,std::int32_t which){
    if(which==0)st.branch_state_7d38e8|=0x10u;
    else if(which==1)st.branch_state_7d38e8|=0x20u;
}
std::uint32_t race_flag6_450300(const RaceManagerState& st){return (st.flags_7d39f0>>6)&1u;}
void race_clear_flag6_450310(RaceManagerState& st){st.flags_7d39f0&=~0x40u;}
std::uint32_t race_route_number_450320(const RaceManagerState& st,RaceManagerServices& s){
    if(s.pc_43f960()){
        std::uint32_t r=0;
        for(unsigned k=0;k<14;k+=2)r|=(st.route_7d39a0[k+1]<<((k+1)&31))|(st.route_7d39a0[k]<<(k&31));
        return r;
    }
    return ((((st.route_7d39a0[3]*2u)|st.route_7d39a0[2])*2u|st.route_7d39a0[1])*2u)|st.route_7d39a0[0];
}
std::uint32_t race_event_stage_key_450380(const RaceManagerWorld& w,std::uint32_t id){return rd32(w.work(id),0x68);}
std::uint16_t race_event_position_4503a0(const RaceManagerWorld& w,std::uint32_t id){return std::uint16_t(rdi16(w.work(id),0x64));}
std::uint32_t race_leader_event_4503c0(const RaceManagerState& st){return st.rank_event_7d388c[0];}
std::uint32_t race_stage_key_450560(const RaceManagerState& st,std::int32_t level){return st.u32(0x7d38f4u+std::uint32_t(level)*4u);}
std::uint32_t race_stage_frames_450570(const RaceManagerState& st){return st.stage_frames_7d3930;}
std::int32_t race_split_450580(const RaceManagerState& st){return st.split_ms_7d387c;}
std::int32_t race_sector_ms_450590(const RaceManagerState& st){return st.sector_ms_7d393c;}
std::int32_t race_stage_split_4505a0(const RaceManagerState& st,std::int32_t level){return std::int32_t(st.u32(0x7d3954u+std::uint32_t(level)*4u));}
std::uint32_t race_sector_new_4505b0(const RaceManagerState& st){return st.sector_new_7d3994;}
std::uint32_t race_sector_set_4505c0(const RaceManagerState& st){return st.sector_set_7d3998;}
std::uint32_t race_total_frames_4505d0(const RaceManagerState& st){return st.total_frames_7d39dc;}
float race_average_speed_4505e0(const RaceManagerState& st){return st.average_speed_7d3888;}
float race_stage_speed_4505f0(const RaceManagerState& st,std::int32_t level){return fbits(st.u32(0x7d3838u+std::uint32_t(level)*4u));}
void race_set_time_adjust_450600(RaceManagerState& st,std::int32_t v){st.time_adjust_7d3880=v;}
std::int32_t race_sector_time_450610(const RaceManagerState& st,std::int32_t level,std::int32_t sector){
    return std::int32_t(st.u32(0x7d3650u+(std::uint32_t(sector)+std::uint32_t(level)*4u)*4u));
}
std::int32_t race_sector_stage_time_450630(const RaceManagerState& st,std::int32_t level,std::int32_t sector){
    return std::int32_t(st.u32(0x7d3748u+(std::uint32_t(sector)+std::uint32_t(level)*4u)*4u));
}
void race_latest_sector_450650(const RaceManagerState& st,std::int32_t& level,std::int32_t& sector){level=st.sector_level_7d3944;sector=st.sector_index_7d3874;}
std::uint32_t race_sector_banner_450670(const RaceManagerState& st){return st.sector_banner_7d3934;}
std::uint32_t race_display_count_450680(const RaceManagerState& st){return st.display_count_7d38ec;}
std::uint32_t race_sector_goal_450690(const RaceManagerState& st){return st.sector_goal_7d39f4;}
std::uint32_t race_stage_flag_4506a0(const RaceManagerState& st,std::int32_t index){return st.u32(0x7d38acu+std::uint32_t(index)*4u);}
std::uint32_t race_is_last_level_450750(RaceManagerServices& s,std::int32_t level){
    if(s.pc_43f960())return level==14?1u:0u;
    return level==4?1u:0u;
}
std::uint32_t race_last_level_450780(RaceManagerServices& s){return s.pc_43f960()?14u:4u;}
void race_area_state_44b9c0(std::int32_t& a){a=0x16;}
void race_area_state_44b9d0(std::int32_t& a){a=0x17;}
void race_area_state_44b9e0(std::int32_t& a){a=0x19;}
void race_area_state_44b9f0(std::int32_t& a){a=0x18;}
void race_reset_8037bc_46fab0(std::uint16_t& w){w=0;}
void race_visibility_init_4f0dd0(std::uint8_t* b){
    const std::uint32_t zero=0;
    for(std::uint32_t a=0x84cec8u;a<0x84d6e8u;a+=0x34u)std::memcpy(b+(a-0x84cec8u),&zero,4);
    for(std::uint32_t a:{0x84d8f0u,0x84d700u,0x84d734u,0x84d768u,0x84d79cu,0x84d7d0u,0x84d804u,0x84d838u,0x84d86cu,0x84d8a0u,0x84d8d4u,0x84d8e0u})
        std::memcpy(b+(a-0x84cec8u),&zero,4);
}
void race_score_init_4f2ac0(std::uint8_t* b){
    const std::uint32_t zero=0;
    for(std::uint32_t a:{0x84df18u,0x84def8u,0x84df08u,0x84df14u,0x84defcu,0x84df04u,0x84df00u,0x84df0cu})std::memcpy(b+(a-0x84def8u),&zero,4);
    b[0x84df38u-0x84def8u]=0;b[0x84df10u-0x84def8u]=0;
}
}
