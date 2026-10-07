#include "platform/mission_manager.hpp"
#include <cmath>
#include <cstring>
namespace outrun::platform {
namespace {
constexpr std::size_t RaceRecordBytes=0x44u;
std::uint32_t u32(const std::uint8_t* p){std::uint32_t v;std::memcpy(&v,p,4);return v;}
const std::uint8_t* record(const MissionManagerState& m,const MissionManagerServices& s){
    if(m.record_83637c<0||!s.races||std::uint32_t(m.record_83637c)>=s.races->race_count)return nullptr;
    return s.races->bytes.data()+s.races->races_offset+std::size_t(m.record_83637c)*RaceRecordBytes;
}
}
namespace {
// +0x7E of the 77 secondary descriptors of table 6A55E8 (EXE data).
constexpr std::uint16_t SecondaryLength6a55e8[77]{
    241,241,241,241,241,241,241,241,241,241,241,241,241,241,241,241,241,241,241,241,
    241,241,241,241,241,241,241,241,241,241,241,241,241,241,241,241,241,241,241,241,
    241,241,241,241,241,241,241,241,241,241,241,241,241,241,241,241,241,241,241,241,
    0,0,0,0,0,241,241,241,241,241,241,241,241,241,241,0,241};
bool primary_length(const driving::PcCourseDescriptorPackR078& d,std::uint32_t token,std::uint16_t& out){
    if(token==0)return false;
    for(std::size_t i=0;i<d.primary_count;++i)if(d.primary[i].token==token){std::memcpy(&out,d.storage[i].data()+0x7e,2);return true;}
    return false;
}
bool secondary_length(const driving::PcCourseDescriptorPackR078& d,std::uint32_t token,std::uint16_t& out){
    if(token==0)return false;
    for(std::size_t i=0;i<d.secondary_count&&i<77;++i)if(d.secondary_tokens[i]==token){out=SecondaryLength6a55e8[i];return true;}
    return false;
}
}
bool course_length_44b820(const std::uint8_t* selected,const std::uint8_t* records,std::size_t record_bytes,
                          const driving::PcCourseDescriptorPackR078& d,std::int32_t laps,std::uint32_t& length){
    std::uint16_t p,q;
    if(laps>0){
        if(!primary_length(d,u32(selected+0x14),p)||!secondary_length(d,u32(selected+0x18),q))return false;
        length=(std::uint32_t(std::uint16_t(p+q))+2u)*std::uint32_t(laps);return true;
    }
    std::uint32_t sum=0;const std::uint8_t* cur=selected;
    for(std::size_t guard=0;guard<=record_bytes/0x78u+1u;++guard){
        if(!primary_length(d,u32(cur+0x14),p))return false;
        sum+=std::uint16_t(p+1);
        const auto next_key=u32(cur+0x2c);
        if(next_key==0xffffffffu&&u32(cur+0x30)==0xffffffffu){length=sum;return true;}
        if(!secondary_length(d,u32(cur+0x18),q))return false;
        sum+=std::uint16_t(q+1);
        const auto index=u32(cur+(next_key!=0xffffffffu?0x24:0x28));
        if(std::size_t(index)*0x78u+0x78u>record_bytes)return false;
        cur=records+std::size_t(index)*0x78u;
    }
    return false;
}
void mission_manager_init_495b90(MissionManagerState& m){
    m.loader_836350=0;m.buffer_836354=0;m.status_836358=1;
    m.v836378=0;m.v836380=0;m.stage_836384=0;m.timer_836388=0;m.v83638c=0;
    m.countdown_836390=0x168;m.v836394=0;m.v836398=0;
    m.flag_67e6ac=1;m.v67e6b0=0;m.v8363a0=0;m.v8363a4=0;m.v67e6b8=-1;
    m.v688b3c=0;                                                      // 4B8DE0(0)
    m.v8363a8=0;
}
void mission_manager_destroy_4964d0(MissionManagerState& m){
    m.selection_836374=1;m.loader_836350=0;m.buffer_836354=0;          // 580C38(836354)
    m.stage_836384=0;m.record_83637c=-1;m.status_836358=1;m.selection={};
}
bool mission_manager_time_4961f0(const MissionManagerState& m,const MissionManagerServices& s,std::int32_t& frames){
    const auto* r=record(m,s);if(!r)return false;
    float seconds;std::memcpy(&seconds,r+0x28,4);
    const float value=seconds*60.0f;
    if(!std::isfinite(value)||value>=2147483648.0f||value<-2147483648.0f)frames=std::int32_t(0x80000000u);
    else frames=std::int32_t(value);                                   // cvttss2si
    return true;
}
bool mission_manager_select_4965a0(MissionManagerState& m,MissionManagerServices& s,bool& result){
    result=false;
    if(s.variant_780258==4u){
        // 4965C0..49688F (LAN race): stage 0 -> 3; stage 3 loads the course script of the
        // session record (type 0: one course, 3: continuous, 5: one goal) and sets the
        // time limit (race kind 1 / 6: 300 / players seconds); stage 4: ready.
        if(m.stage_836384==4u){result=true;return true;}
        if(m.stage_836384==0u)m.stage_836384=3;
        else if(m.stage_836384!=3u)return true;                         // xor al,al
        if(!s.lan.present){s.missing=0x4965e7;return false;}             // [83637C] null: the PC reads [0+14]
        static constexpr const char* Goals[5]{"GOAL_A","GOAL_B","GOAL_C","GOAL_D","GOAL_E"};
        const std::uint32_t i=s.lan.index;
        const char* data=nullptr;const char* category=nullptr;std::uint32_t preset=0;
        if(s.lan.type==5u){
            if(i>=20u){s.missing=0x496673;return false;}                 // past the 67E70C table
            if(i<5u){data="OR2_goals";preset=1;}else if(i<10u)data="SP_goals";
            else if(i<15u){data="OR2_reverse_goals";preset=1;}else data="SP_reverse_goals";
            category=Goals[i%5u];
        }else if(s.lan.type==3u){
            static constexpr const char* Ren[4][2]{{"csc_data_2_ren","csc_data_2_ren_course"},{"csc_data_cvt_ren","csc_data_cvt_ren_course"},
                {"csc_data_2_ren","csc_data_2_reverse_ren_course"},{"csc_data_cvt_ren","csc_data_cvt_reverse_ren_course"}};
            if(i>3u)return true;                                          // ja 496A09
            data=Ren[i][0];category=Ren[i][1];preset=(i&1u)?2u:3u;
        }else if(s.lan.type==0u){
            if(i==0u){data="csc_data_2";category="csc_data_2_course";preset=1;}
            else{data="csc_data_cvt";category="csc_data_cvt_course";preset=0;}
        }else return true;                                                // 496A09: xor al,al
        if(!s.apply_named_44d720||!s.preset_43f950){s.missing=0x44d720;return false;}
        const bool loaded=s.apply_named_44d720(data,category);
        s.preset_43f950(preset);
        if(!loaded)return true;
        if(s.lan.kind==6u||s.lan.kind==1u){
            // variant 4: 300 / players seconds (0x12C / [456D60] * 0x3C)
            if(!s.lan.players){s.missing=0x49685b;return false;}         // idiv by 0
            m.timer_836388=std::uint32_t(std::int32_t(0x12cu/s.lan.players)*0x3c);m.v8363a0=m.timer_836388;
        }
        m.stage_836384=4;result=true;return true;
    }
    if(m.stage_836384==4u){result=true;return true;}
    if(m.stage_836384==0u){
        if(!s.races||s.races->race_count==0){s.missing=0x4f12a0;return false;}   // Races.bin load
        const auto* base=s.races->bytes.data()+s.races->races_offset;
        std::int32_t found=-1;
        for(std::uint32_t i=0;i<s.races->race_count;++i)
            if(u32(base+i*RaceRecordBytes)==s.race_key_67e6a4&&u32(base+i*RaceRecordBytes+4)==s.race_sub_67e6a8){found=std::int32_t(i);break;}
        if(found<0)return true;                                          // PC returns 0 every frame
        m.record_83637c=found;
        const auto* r=record(m,s);
        if(u32(r+4)==1u)m.v83639c=0;
        const auto type=u32(r+0x20);
        if(type==6u||type==1u){std::int32_t t{};mission_manager_time_4961f0(m,s,t);m.timer_836388=std::uint32_t(t);}
        m.stage_836384=3;return true;
    }
    if(m.stage_836384==3u){
        const auto* r=record(m,s);if(!r){s.missing=0x4968b1;return false;}
        if(m.selection.course_records.empty()&&
           !race_course_select_4965a0(*s.races,u32(r),u32(r+4),m.selection)){s.missing=0x4f1210;return false;}
        if(!s.apply_44d720){s.missing=0x44d720;return false;}
        if(!s.apply_44d720(m.selection,u32(r+0x14)==4u))return true;     // still loading
        if(!s.profile){s.missing=0x7c25b7;return false;}
        const auto offset=0x1d7u+5u*s.race_key_67e6a4+s.race_sub_67e6a8;
        if(offset>=s.profile->size()){s.missing=0x496923;return false;}
        m.v67e6b4=(*s.profile)[offset];
        if(u32(r+0x20)!=4u){
            if(!s.traffic_47cf40){s.missing=0x47cf40;return false;}
            if(!s.traffic_47cf40(std::uint32_t(m.record_83637c)))return false;
        }
        m.stage_836384=4;result=true;return true;
    }
    return true;
}
}
// ------------------------------------------------------------------ 496A30
namespace outrun::platform {
namespace {
using driving::Bytes;
std::uint32_t fb(float f){std::uint32_t u;std::memcpy(&u,&f,4);return u;}
// EXE tables (5C1D44 .rdata words, 67E6BC/67E6F8/67E710/67E72C/67E76C .data).
constexpr std::uint16_t Voice5c1d44[8]{0x1e7,0x219,0x1e8,0x1ea,0x1ea,0x1ea,0x218,0};
constexpr std::uint32_t Tag67e6bc[8]{0x2b0063,0x2c0114,0x2b0065,0x2b0064,0xffffffffu,0xffffffffu,0x2c0113,1};
constexpr std::uint32_t Rival67e6f8[6]{0x2b0054,0x2b0058,0x2b0059,0x2b0057,0x2b0055,0x2b0056};
constexpr std::uint32_t Caught67e710[6]{0x2b004e,0x2b0052,0x2b0053,0x2b0051,0x2b004f,0x2b0050};
constexpr float Grade67e72c[4]{0.30000001192092896f,0.5f,0.699999988079071f,1.0f};
constexpr std::uint32_t Grade67e76c[7]{0x2b000f,0x2b0010,0x2b0011,0x2b0012,0x2b0013,0x2b0014,0x2b0015};
using Element=std::array<std::uint8_t,0xa0>;
struct Race {
    MissionManagerState& m;const MissionManagerServices& s;MissionRaceServices& r;
    bool fail(std::uint32_t pc){if(!r.missing)r.missing=pc;return false;}
    const std::uint8_t* rec() const {
        if(m.record_83637c<0||!s.races||std::uint32_t(m.record_83637c)>=s.races->race_count)return nullptr;
        return s.races->bytes.data()+s.races->races_offset+std::size_t(m.record_83637c)*0x44u;}
    std::uint32_t type() const {const auto* p=rec();return p?u32(p+0x20):0u;}               // 495B20
    bool call(Element& e,std::uint32_t pc,std::initializer_list<std::uint32_t> a={}){
        std::uint32_t result{};std::array<std::uint32_t,11> args{};std::size_t n=0;for(auto v:a)args[n++]=v;
        if(!r.ui->call(pc,Bytes(e.data(),e.size()),n?args.data():nullptr,n,result)||r.ui->missing_pc)
            return fail(r.ui->missing_pc?r.ui->missing_pc:pc);
        return true;}
    bool configure(Element& e,std::uint32_t token,std::uint32_t a1,std::uint32_t a2,std::uint32_t a3,std::uint32_t a4,
                   float x,float y,float sx,float sy,float sz){
        return call(e,0x465860,{token,a1,a2,a3,a4,fb(x),fb(y),fb(sx),fb(sy),fb(sz),0})&&call(e,0x465970);}
    bool motion(Element& e){                                         // 465310(1,1,0.2,0), 4653C0(170,-80,0.2,0)
        return call(e,0x465310,{fb(1.0f),fb(1.0f),fb(0.2f),0})&&call(e,0x4653c0,{fb(170.0f),fb(-80.0f),fb(0.2f),0});}
    Bytes racer(std::uint32_t index){return Bytes(r.racers->racers_80fb00.data()+std::size_t(index)*0xa0u,0xa0u);}
    // 477350: racer [64E194] or null.
    bool rival(Bytes& out,bool& present){
        present=false;if(r.racers->v64e194<0)return true;
        if((std::size_t(r.racers->v64e194)+1u)*0xa0u>r.racers->racers_80fb00.size())return fail(0x47735c);
        out=racer(std::uint32_t(r.racers->v64e194));present=true;return true;}
    bool last_place(){return std::uint32_t(r.car.u8(0xc36))==r.racers->v80fb2c-1u;}   // 496460 (variant != 4)
    void profile_495a20(){
        const auto* p=rec();const auto off=0x1d7u+5u*u32(p)+u32(p+4);
        auto& pr=*r.profile;const std::int32_t have=pr[off];
        if(std::int32_t(m.v67e6b0)>have||have==7){pr[0x3f4]|=2;pr[off]=std::uint8_t(m.v67e6b0);}
    }
    std::uint32_t score_496580(){                                   // 496580 / 4960A0
        const float speed=r.car.f32(0x1c4)*216.72000122070312f;
        const float limit=(r.car_spec_134c-2.299999952316284f)*-131.38677978515625f+318.0f;
        const bool fast=type()==3u?speed>limit:speed>limit*0.949999988079071f;
        return fast?20u:10u;}
    bool release_477430(Bytes rc){
        const auto car_index=rc.u32(0x4c);rc.put32(0x54,0);rc.put32(0x58,1);
        if(car_index!=0xffffffffu){if(!r.release_car_4763d0||!r.release_car_4763d0(r.user,car_index))return fail(0x4763d0);}
        const auto mine=rc.u8(0x74);
        if(mine<r.car.u8(0xc36))r.car.put8(0xc36,std::uint8_t(r.car.u8(0xc36)-1));
        for(std::uint32_t j=0;j<r.racers->count_80fb04;++j){auto o=racer(j);if(mine<o.u8(0x74))o.put8(0x74,std::uint8_t(o.u8(0x74)-1));}
        return true;}
    bool caught_496230(Bytes rc){
        m.v836394=0xb4;
        if(rec()&&type()==6u){auto i=std::uint32_t(rc.u8(0x76));if(i>=6)i=0;
            return configure(m.ui_836450,Caught67e710[i],~0u,~0u,0xa,3,0,0,1,1,1);}
        return true;}
    bool rivals_477b60(){
        for(std::uint32_t i=0;i<r.racers->count_80fb04;++i){
            if((std::size_t(i)+1u)*0xa0u>r.racers->racers_80fb00.size())return fail(0x477b80);
            auto rc=racer(i);
            if(!m.selection_836374)continue;                               // 4957F0
            const auto t=type();bool timer=false;
            if(t==2u||t==3u){const auto bit=t==2u?2u:1u;const auto f=rc.u8(0x71);
                if((f&bit)&&rc.u32(0x6c)==0)rc.put32(0x98,rc.u32(0x98)+((f&8u)?0x14u:0xau));continue;}
            if(t==6u){
                if(std::uint32_t(rc.u8(0x74))==r.racers->v80fb2c-1u){
                    if(rc.i32(0x98)>0)rc.puti(0x98,rc.i32(0x98)-1);
                    if(rc.u32(0x98)==0){
                        if(!caught_496230(rc))return false;
                        if(r.racers->v80fb2c==2u){
                            profile_495a20();r.race_end_7d38f0=1;m.v8363a4=1;             // 495A20, 450230(1), 495810(1)
                            const auto k=rc.u8(0x76);if(k>=6)return fail(0x495844);       // 495840 reads 67E710[k]
                            m.v67e6b8=std::int32_t(Caught67e710[k]);
                        }
                        if(!release_477430(rc))return false;
                    }
                }
                timer=true;
            }else timer=t==1u;
            if(!timer)continue;
            auto v=rc.i32(0x9c);
            if(std::uint32_t(rc.u8(0x74))==r.racers->v80fb2c-1u){++v;if(v>=600)v=600;}else{--v;if(v<=0)v=0;}
            rc.puti(0x9c,v);
        }
        return true;}
};
}
void mission_manager_construct_ui_465160(MissionManagerState& m,FrontendUiResources& ui){
    std::uint32_t r{};
    for(auto* e:{&m.ui_836590,&m.ui_836450,&m.ui_8364f0,&m.ui_8363b0})ui.call(0x465160,Bytes(e->data(),e->size()),nullptr,0,r);
}
bool mission_project_495d90(MissionRaceServices& r,bool flag,float& x,float& y){
    if(r.camera.i8(0x34a)<=1){x=234.0f;y=108.0f;return true;}
    if(!r.matrices){r.missing=0x495df7;return false;}
    auto& st=*r.matrices;
    driving::CourseProbe v{r.model_offset_5e0ac8[0],r.model_offset_5e0ac8[1],r.model_offset_5e0ac8[2]};
    if(flag)v.x=v.x-0.699999988079071f;
    driving::pc_matrix_push_load(st,r.car.sub(0xb0,64));                   // 409F90
    v=driving::pc_matrix_point(st,v);                                       // 40A7D0
    driving::pc_matrix_load(st,r.camera.sub(0x140,64));                     // 40A170
    const float a0=r.camera.f32(0xb0),a1=r.camera.f32(0xb4);               // 449940
    const auto t=driving::pc_matrix_point(st,v);
    const float z=t.z;
    // fcomip eps,|z|; jbe: projection unless eps > |z| (NaN projects).
    if(1.1920928955078125e-07f>std::fabs(z))v={0,0,0};                        // fcomip: exact compare
    else{const float w=1.0f/(0.0f-z);const float wx=w*a0;const float wy=w*a1;v={wx*t.x,t.y*wy,z};}
    driving::pc_matrix_pop(st);                                             // 40A010
    x=v.x;y=0.0f-v.y;return true;
}
bool mission_marker_495e90(MissionManagerState& m,const MissionManagerServices& s,MissionRaceServices& r){
    Race q{m,s,r};
    if(r.camera.u32(0x354)!=0||!q.rec())return true;
    const auto t=q.type();float x{},y{};
    if(t==3u||t==2u){
        if(!mission_project_495d90(r,true,x,y))return false;
        const auto diff=std::int32_t(m.timer_836388-m.v8363a0);
        if(diff>500){
            if(!q.call(m.ui_8364f0,0x465250))return false;
            if(!q.call(m.ui_8364f0,0x465860,{0x2b0015,0x3d,0x69,5,1,fb(x),fb(y),fb(1.0f),fb(1.0f),fb(2.0f),0}))return false;
            if(!q.call(m.ui_8364f0,0x4653c0,{fb(210.0f),fb(-160.0f),fb(0.25f),0})||!q.call(m.ui_8364f0,0x465970))return false;
            if(r.sounds_424940)r.sounds_424940->push_back(0x52);m.v8363a0=m.timer_836388;return true;
        }
        if(diff<0){
            if(!q.call(m.ui_8364f0,0x465250)||!q.configure(m.ui_8364f0,0x2b000e,~0u,~0u,4,1,x,y,1.0f,1.0f,3.0f))return false;
            m.v8363a0=m.timer_836388;return true;
        }
        float v=float(diff)*0.0020000000949949026f*7.0f;
        if(!(6.0f>v))v=6.0f;
        if(!q.call(m.ui_8363b0,0x465250))return false;
        const auto i=std::int32_t(v);
        if(i<0||i>6)return q.fail(0x496056);
        return q.configure(m.ui_8363b0,Grade67e76c[i],0x1f,0x3c,3,0,x,y,1.0f,1.0f,1.0f);
    }
    if(t==6u){
        if(!mission_project_495d90(r,false,x,y))return false;
        auto diff=std::int32_t(m.timer_836388-m.v8363a0);if(diff<0)diff=-diff;
        if(diff>0x46){
            if(!q.call(m.ui_8364f0,0x465250)||!q.configure(m.ui_8364f0,0x2b000e,~0u,~0u,4,1,x,y,1.0f,1.0f,3.0f))return false;
            m.v8363a0=m.timer_836388;
        }
    }
    return true;
}
bool mission_rank_495c20(const MissionManagerState& m,const MissionManagerServices& s,MissionRaceServices& r){
    auto& mm=const_cast<MissionManagerState&>(m);Race q{mm,s,r};
    const auto t=q.type();const std::int32_t pos=r.car.u8(0xc36);
    auto from_position=[&]{auto v=6-pos;if(v<=0)v=0;mm.v67e6b0=std::uint32_t(v);};
    const auto last=std::int32_t(r.racers->v80fb2c)-1;
    if(t==0u){from_position();if(pos==last)mm.v67e6b0=0;return true;}
    if(t==1u){from_position();return true;}
    const auto* p=q.rec();
    if(t==2u||t==3u){
        // 47DA80: rival racer ([64E190], else rank 1 through 80FB1C).
        const auto& rs=*r.racers;std::int32_t index=-1;
        if(!rs.racers_80fb00.empty()&&std::int32_t(rs.v80fb2c)>1){
            if(rs.v64e190>=0)index=rs.v64e190;
            else{
                if(rs.table_80fb1c.size()<0x28)return q.fail(0x477dd0);
                std::memcpy(&index,rs.table_80fb1c.data()+0x24,4);
            }
        }
        if(index<0||(std::size_t(index)+1u)*0xa0u>rs.racers_80fb00.size())return q.fail(0x495c9e);   // PC reads [null+0x98]
        std::int32_t target;std::memcpy(&target,rs.racers_80fb00.data()+std::size_t(index)*0xa0u+0x98,4);
        const auto score=std::int32_t(m.timer_836388);
        if(score>target){mm.v67e6b0=4;
            if(score>std::int32_t(u32(p+0x3c)))mm.v67e6b0=6;else if(score>std::int32_t(u32(p+0x38)))mm.v67e6b0=5;return true;}
        const float ratio=float(score)/float(target);
        for(std::uint32_t i=0;i<4;++i)if(Grade67e72c[i]>ratio){mm.v67e6b0=i;return true;}
        return true;
    }
    if(t==6u){
        const auto v=(std::int32_t(m.timer_836388)+5)/6;
        if(v>std::int32_t(u32(p+0x3c)))mm.v67e6b0=6;else mm.v67e6b0=4u+(v>std::int32_t(u32(p+0x38))?1u:0u);
        if(pos==last)mm.v67e6b0=0;
    }
    return true;
}
bool mission_manager_control_496a30(MissionManagerState& m,MissionManagerServices& s,MissionRaceServices& r){
    bool ready{};
    if(!mission_manager_select_4965a0(m,s,ready)){r.missing=s.missing;return false;}
    if(!ready)return true;
    if(!r.ui||!r.voice||!r.racers||!r.profile){r.missing=0x496a3d;return false;}
    Race q{m,s,r};
    r.voice_services.sounds_424940=r.sounds_424940;
    for(auto* e:{&m.ui_836450,&m.ui_8364f0,&m.ui_836590,&m.ui_8363b0})if(!q.call(*e,0x4659f0))return false;
    auto finish=[&]{navi_voice_pump_45bcd0(*r.voice,r.voice_services);return true;};
    if(r.mode_78026c!=16u||r.race_end_7d38f0!=0u||r.pause_8367bc>0)return finish();
    if(r.car.size()<0xdc0){r.missing=0x496a95;return false;}
    auto& car=r.car;
    if(m.countdown_836390!=0u){
        const auto cd=m.countdown_836390;
        if(cd==0x168u){
            if(!q.configure(m.ui_836590,0x2b0020,~0u,~0u,9,3,170.0f,20.0f,1.0f,4.0f,1.0f)||!q.motion(m.ui_836590))return false;
            if(!q.configure(m.ui_836450,0x2b003e,~0u,~0u,0xa,3,170.0f,20.0f,1.0f,4.0f,1.0f)||!q.motion(m.ui_836450))return false;
        }else if(cd==0xb4u){
            const auto t=q.type();if(t>7u)return q.fail(0x496b5e);
            navi_voice_request_45bd30(*r.voice,r.voice_services,Voice5c1d44[t],0xb4,0xf);
            if(!q.call(m.ui_836590,0x465250)||!q.configure(m.ui_836590,0x2b0020,~0u,~0u,9,5,170.0f,20.0f,1.0f,4.0f,1.0f)||!q.motion(m.ui_836590))return false;
            if(!q.call(m.ui_836450,0x465250)||!q.configure(m.ui_836450,Tag67e6bc[t],~0u,~0u,0xa,3,170.0f,20.0f,1.0f,4.0f,1.0f)||!q.motion(m.ui_836450))return false;
        }
        std::int32_t e=std::int32_t(m.countdown_836390)-1;bool reset=false;
        if(e<=0){m.countdown_836390=0;reset=true;}else{--e;m.countdown_836390=std::uint32_t(e);reset=e==0;}
        if(reset&&(!q.call(m.ui_836590,0x465250)||!q.call(m.ui_836450,0x465250)))return false;
    }
    {std::int32_t e=std::int32_t(m.v836394)-1;m.v836394=e<=0?0u:std::uint32_t(e-1);}
    const auto t=q.type();
    auto score=[&]{return std::int32_t(m.timer_836388);};
    switch(t){
    case 3u:{
        if(car.u8(0x244)&0x9cu)break;
        if(car.i8(0xda4)>0)break;
        const auto flags=car.u32(4);
        if((flags&0x10000u)&&(flags&0x4000000u)){auto v=score()-2000;if(v<0)v=0;m.timer_836388=std::uint32_t(v);car.put8(0xda4,0x1e);break;}
        const bool fast=car.f32(0xe68)>0.18000000715255737f&&car.i16(0x162)>-1500&&car.i16(0x162)<1500;   // 4B8EF0
        if(fast)m.timer_836388+=q.score_496580();
        break;}
    case 2u:{
        if(car.i8(0xd36)<=0)break;
        if(!(car.f32(0x1c4)*216.72000122070312f>10.0f))break;
        if(car.u8(0x244)&0x9cu)break;
        if(car.i8(0xda4)>0)break;
        m.timer_836388+=q.score_496580();break;}
    case 6u:{
        if(q.last_place()){
            auto v=score();
            if(v>0){--v;m.timer_836388=std::uint32_t(v);}
            if(v==0){q.profile_495a20();r.race_end_7d38f0=2;m.v8363a4=1;}
        }
        break;}                                   // not last: 49702A directly (variant != 4)
    case 1u:{
        {auto v=score()-1;if(v<=0)v=0;m.timer_836388=std::uint32_t(v);}
        Bytes rc{nullptr,0};bool present{};
        if(!q.rival(rc,present))return false;
        if(present&&r.racers->v80fb2c==2u){const auto k=rc.u8(0x76);if(k>=6)return q.fail(0x496e3e);m.v67e6b8=std::int32_t(Rival67e6f8[k]);}
        const auto c=score();const auto seconds=c/60;
        if(seconds<=5&&(std::int32_t(m.v8363a8)!=seconds||c==1)){m.v8363a8=std::uint32_t(seconds);if(r.sounds_424940)r.sounds_424940->push_back(0x518d);}
        if(c==0x168){
            if(!q.call(m.ui_836450,0x465250)||!q.configure(m.ui_836450,0x2b0062,~0u,~0u,0xa,3,0,0,1,1,1))return false;
        }else if(c<0x168&&c>0x3c){
            if(m.flag_67e6ac&&q.last_place()){navi_voice_request_45bd30(*r.voice,r.voice_services,0x1f2,0x3c,0xa);m.flag_67e6ac=0;}
        }else if(c==0){
            if(q.last_place()){q.profile_495a20();r.race_end_7d38f0=2;}
            else{
                Bytes rv{nullptr,0};bool have{};if(!q.rival(rv,have))return false;
                if(have){
                    if(r.racers->v80fb2c==2u){r.race_end_7d38f0=1;m.v8363a4=1;}
                    if(!q.release_477430(rv))return false;
                    m.v836394=0xb4;
                    auto i=std::uint32_t(rv.u8(0x76));if(i>=6)i=0;
                    m.v67e6b8=std::int32_t(Rival67e6f8[i]);
                    if(!q.call(m.ui_836450,0x465250)||!q.configure(m.ui_836450,Rival67e6f8[i],~0u,~0u,0xa,3,0,0,1,1,1))return false;
                    navi_voice_request_45bd30(*r.voice,r.voice_services,0x1f0,0x3c,0xa);
                }
            }
            std::int32_t frames{};
            if(!mission_manager_time_4961f0(m,s,frames))return q.fail(0x49700b);
            m.timer_836388=std::uint32_t(frames);m.flag_67e6ac=1;
        }
        break;}
    default:break;                                // 4, 5 and others: 49701D
    }
    // 49701D protected test (cmp [780258],4): variant != 4 continues at 49702A.
    if(t==6u||t==1u){
        Bytes rc{nullptr,0};bool present{};if(!q.rival(rc,present))return false;
        auto v=std::int32_t(m.v83638c);
        if(!present){++v;if(v>=600)v=600;}else{--v;if(v<=0)v=0;}
        m.v83638c=std::uint32_t(v);
    }
    car.putf(0xdbc,float(std::int32_t(m.v83638c))*0.0016666667070239782f*0.17000000178813934f+1.0f);
    if(!q.rivals_477b60()||!mission_rank_495c20(m,s,r)||!mission_marker_495e90(m,s,r))return false;
    return finish();
}
}
