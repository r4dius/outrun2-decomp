#include "platform/racer_setup.hpp"
#include "driving/pc_x87.hpp"
#include <cmath>
#include <cstdio>
#include <cstring>
#include <limits>
namespace outrun::platform {
namespace {
constexpr float K5B447C=0.0015259021893143654f;   // 1/655.35
constexpr float K5B4478=17.99198341369629f;
constexpr float K5B4474=0.003389830468222499f;
constexpr float K5B4470=1.0431493520736694f;
constexpr float K5B446C=3612.0f;
constexpr float K5B4324=1.52587890625e-05f;       // 1/65536
constexpr float K5B4320=1.5259021893143654e-05f;  // 1/65535
constexpr float K62806C=1.0f;
constexpr float K628134=60.0f;                    // 4AF540
std::uint32_t step(std::uint32_t& seed){seed=(seed*0x5e5u+0x29u)&0xffffu;return seed;}
// x87 helpers: every fmul/fdiv/fsub rounds to the current precision control
// (24-bit after the Direct3D device is created), stores round to float.
using driving::X87;
// fld 60.0; fmul per; fdivr numerator; fmul k; fstp dword.
float x87_ratio(float numerator,float per,float k){
    return driving::x87_float((X87(numerator)/(X87(K628134)*X87(per)))*X87(k));
}
// fild a; fild b; fsubp (a-b); fdiv per; fmul k1; fmul k2; fstp dword.
float x87_int_ratio(std::int32_t a,std::int32_t b,float per,float k1,float k2){
    return driving::x87_float((((X87(a)-X87(b))/X87(per))*X87(k1))*X87(k2));
}
// fild a; fsub f; fstp dword.
float x87_int_minus(std::int32_t a,float f){
    return driving::x87_float(X87(a)-X87(f));
}
// 582194 (MSVC _ftol2): signed truncation toward zero, low dword.
std::uint32_t ftol(float v){
    if(!std::isfinite(v)||v>=9.2233720368547758e18f||v<-9.2233720368547758e18f)return 0u;   // indefinite 0x8000000000000000
    return std::uint32_t(std::int64_t(v));
}
struct Racer {
    std::uint8_t* p;
    void f(std::size_t o,float v){std::memcpy(p+o,&v,4);}
    void d(std::size_t o,std::uint32_t v){std::memcpy(p+o,&v,4);}
    float gf(std::size_t o)const{float v;std::memcpy(&v,p+o,4);return v;}
    std::uint32_t gd(std::size_t o)const{std::uint32_t v;std::memcpy(&v,p+o,4);return v;}
};
bool fail(RacerSetupServices& s,unsigned pc){s.missing=pc;return false;}
}
bool racer_setup_47cf40(RacerSetupState& st,RacerSetupServices& s,std::uint32_t arg,bool flag){
    if(!s.view)return fail(s,0x47cf40);
    const auto& v=*s.view;
    auto rd=[&](std::uint32_t a,std::uint32_t& out){return v.u32(a,out);};
    auto rf=[&](std::uint32_t a,float& out){return v.f32(a,out);};
    auto ri=[&](std::uint32_t a,std::int32_t& out){std::uint32_t u;if(!v.u32(a,u))return false;out=std::int32_t(u);return true;};
    st.special_80fb28=0;
    std::uint32_t cfg=arg;
    if(cfg==0){
        if(!s.car_present)return fail(s,0x47cf57);
        cfg=s.car_byte11>=15?0x64e040u:0x64dff0u;
    }
    st.config_80fb0c=cfg;
    std::uint32_t c2c,c40;std::int32_t count;
    if(!rd(cfg+0x2c,c2c)||!rd(cfg+0x40,c40)||!ri(cfg,count))return fail(s,0x47cf71);
    st.seed_6a4e2c=c2c?(c2c&0xffffu):0xa5deu;
    if(c40){st.names_64def0=c40;st.names_64deec=c40+0x20;}else{st.names_64def0=0x5b3a3cu;st.names_64deec=0x5b3a30u;}
    if(count<0||count>0x4000)return fail(s,0x47cfc7);                 // malloc(count*0xA0) bound
    st.count_80fb04=std::uint32_t(count);
    st.racers_80fb00.assign(std::size_t(count)*RacerRecordBytes,0u);
    st.flag_80fb08=0;st.flag_80fb09=0;
    // 4774B0
    st.v64e190=-1;st.v64e194=-1;st.v80fb2c=0;st.v8037b8=0;
    st.table_80fb1c.assign(std::size_t(count*5+5)*4u,0u);st.table_80fb30.assign(std::size_t(count)*8u+8u,0u);st.v80fb18=0;
    // 476550
    st.table_80fb20.assign(std::size_t(count+1)*0x1cu,0u);
    for(std::int32_t i=0;i<count+1;++i)st.table_80fb20[std::size_t(i)*0x1cu+0x10u]=0xff;
    st.v80fb24=0;
    std::int32_t c1c,c18,c10,c14,c4,c8;
    if(!ri(cfg+0x1c,c1c)||!ri(cfg+0x18,c18)||!ri(cfg+0x10,c10)||!ri(cfg+0x14,c14)||!ri(cfg+4,c4)||!ri(cfg+8,c8))return fail(s,0x47cfec);
    st.v680ad4=std::uint32_t(c1c+1);st.v680ad8=std::uint32_t(0x18-(c1c+1));
    std::int32_t ebx=0,edi=0;
    if(flag){
        ebx=s.car_present?std::int32_t(s.car_word25e):0;
        edi=std::int32_t(s.network_686258)+ebx;
    }else{
        std::uint32_t length{};
        if(!s.course_length_44b820||!s.course_length_44b820(c18,length))return fail(s,0x44b820);
        edi=std::int32_t(length&0xffffu);
    }
    st.length_803710=std::uint32_t(edi);
    st.per_lap_804388=c18>0?std::int16_t(edi/c18):std::int16_t(-1);
    const float fcount=float(count);
    float f28=float(c10)/fcount;
    float f14=float(c14)+float(ebx);
    const float f2c=float(c8-c4)/float(count-1);
    float f38=float(c8);
    std::uint32_t profile;if(!rd(cfg+0x38,profile))return fail(s,0x47d0cb);
    if(!profile)profile=0x64defcu;
    st.profile_64df64=profile;
    float p8,pc;if(!rf(profile+8,p8)||!rf(profile+0xc,pc))return fail(s,0x47d0dd);
    const float f30=(p8-pc)/fcount;
    float f18=f30+pc;
    const float f20=float(edi);
    std::uint32_t c24,c28u,c3c;
    if(!rd(cfg+0x24,c24)||!rd(cfg+0x28,c28u)||!rd(cfg+0x3c,c3c))return fail(s,0x47d13e);
    for(std::int32_t i=0;i<count;++i){
        Racer r{st.racers_80fb00.data()+std::size_t(i)*RacerRecordBytes};
        const std::array<std::uint8_t,4>* preset=std::size_t(i)<s.preset_8361c0.size()?&s.preset_8361c0[std::size_t(i)]:nullptr;
        if(preset){r.p[0x72]=(*preset)[0];r.p[0x73]=(*preset)[1];}
        else{
            std::uint32_t list,list_count;
            if(!rd(0x64e090u+c24*8u,list)||!rd(0x64e094u+c24*8u,list_count)||list_count==0)return fail(s,0x47d15e);
            const auto pick=step(st.seed_6a4e2c)%list_count;
            std::uint8_t model;if(!v.u8(list+pick,model))return fail(s,0x47d16c);
            r.p[0x72]=model;
            const auto roll=step(st.seed_6a4e2c);
            if(float(std::int32_t(c28u))>float(std::int32_t(roll))*K5B447C)r.p[0x73]=1;
            else{
                std::uint32_t j=0;
                for(;;++j){
                    std::uint8_t cl;if(!v.u8(0x5b3c35u+2u*j,cl))return fail(s,0x47d1b0);
                    if(std::int8_t(cl)>=0&&cl==r.p[0x72])break;
                }
                std::uint8_t k;v.u8(0x5b3c34u+2u*j,k);
                std::uint8_t colour;
                for(;;){
                    const auto seed=step(st.seed_6a4e2c);
                    if(!v.u8(0x5b3a80u+(seed&7u)+std::uint32_t(k)*8u,colour))return fail(s,0x47d1e1);
                    if(colour!=1)break;
                }
                r.p[0x73]=colour;
            }
        }
        r.p[0x70]=std::uint8_t(i);
        r.d(0x4c,0xffffffffu);r.d(0x50,1);
        r.d(0x98,s.selection_836374&&s.mission_type_495b20==6u?std::uint32_t(s.time_4961f0):0u);
        r.d(0x9c,0);
        r.f(0x40,f14);
        r.d(0x3c,ftol(f14)&0xffffu);
        const float f1c=f20-f14;
        r.f(0x00,((f1c/f38)*K5B4478)*K5B4474);
        const float r44=x87_ratio(f1c,f38,K5B4470);
        r.f(0x04,f18);
        r.f(0x28,0.0f);r.f(0x24,0.0f);
        r.f(0x34,f38);r.f(0x38,K62806C);
        r.d(0x54,1);r.d(0x58,0);r.d(0x6c,0);r.d(0x60,0);r.d(0x64,0);r.d(0x5c,0);r.d(0x68,0);
        r.p[0x77]=std::uint8_t(i);r.p[0x76]=0xff;
        r.f(0x44,r44);
        std::int32_t q1c;float q2c,q30,q24,q20,q28;
        if(!ri(profile+0x1c,q1c)||!rf(profile+0x2c,q2c)||!rf(profile+0x30,q30)||!rf(profile+0x24,q24)||
           !rf(profile+0x20,q20)||!rf(profile+0x28,q28))return fail(s,0x47d2ed);
        {
            const auto s1=step(st.seed_6a4e2c);
            float x5=q2c,x0=float(q1c),x6=q30,x3=q24,x7=float(q1c),x2=float(std::int32_t(s1));
            x0*=x5;x2*=x0;const float f24=x2;x3*=K5B446C;x2=K5B4324;x0=f24*x2;
            float x4=K62806C-x5;x7*=x4;x7+=x0;r.f(0x08,x7);
            const auto s2=step(st.seed_6a4e2c);
            x0=float(std::int32_t(s2))*x3;float x1=K62806C-x6;x0*=x6;x7=x1;x0*=x2;x7*=x3;x0+=x7;r.f(0x0c,x0);
            const auto s3=step(st.seed_6a4e2c);
            x3=float(std::int32_t(s3))*x0;r.f(0x08,r.gf(0x08)*K5B4474);x3*=x2;r.f(0x10,x3);
            const auto s4=step(st.seed_6a4e2c);
            x0=q20;x3=q28;x0*=x5;x3*=K5B446C;x5=float(std::int32_t(s4))*x0;x0=q20*x4;x5*=x2;x5+=x0;r.f(0x14,x5);
            const auto s5=step(st.seed_6a4e2c);
            x0=float(std::int32_t(s5))*x3;x1*=x3;x0*=x6;x0*=x2;x0+=x1;r.f(0x18,x0);
            const auto s6=step(st.seed_6a4e2c);
            x1=float(std::int32_t(s6))*x0;x1*=x2;r.f(0x1c,x1);
        }
        if(preset){
            std::uint16_t w;std::memcpy(&w,preset->data()+2,2);
            r.f(0x20,float(std::int32_t(w))*K5B4320);
        }else{
            const auto s7=step(st.seed_6a4e2c);
            float x0=float(std::int32_t(s7));
            if(c3c){float a,b;if(!rf(c3c+4,b)||!rf(c3c,a))return fail(s,0x47d4a8);x0=x0*b;x0=x0*K5B4324;x0=x0+a;}
            else{float a,b;if(!rf(0x64def8u,b)||!rf(0x64def4u,a))return fail(s,0x47d4c0);x0=x0*b;x0=x0*K5B4324;x0=x0+a;}
            r.f(0x20,x0);
            if(x0>K62806C)r.f(0x20,K62806C);
        }
        char name[64];std::snprintf(name,sizeof name,"%s %d",s.text_3d7.c_str(),i+1);
        const std::size_t length=std::strlen(name)+1;
        if(0x78u+length>RacerRecordBytes)return fail(s,0x47d50b);
        std::memcpy(r.p+0x78,name,length);
        f14+=f28;f38-=f2c;f18+=f30;
    }
    // Special racers (cfg+0x34), 0x28-byte entries terminated by a negative first dword.
    std::uint32_t special;if(!rd(cfg+0x34,special))return fail(s,0x47d564);
    std::int32_t first=-1;
    if(special&&ri(special,first)&&first>=0){
        float per=f38;                                                    // [esp+0x38] slot
        for(std::uint32_t e=special;;e+=0x28u){
            std::int32_t e0,e4,e8,ec,e10,e14,e18;std::uint32_t e24;float e1c,e20;
            if(!ri(e,e0)||!ri(e+4,e4)||!ri(e+8,e8)||!ri(e+0xc,ec)||!ri(e+0x10,e10)||!ri(e+0x14,e14)||!ri(e+0x18,e18)||
               !rf(e+0x1c,e1c)||!rf(e+0x20,e20)||!rd(e+0x24,e24))return fail(s,0x47d590);
            const std::int32_t slot=count-ec;
            if(slot<0||slot>=count)return fail(s,0x47d5a1);
            Racer r{st.racers_80fb00.data()+std::size_t(slot)*RacerRecordBytes};
            const std::uint8_t cl=std::uint8_t(e0);
            std::uint8_t al;
            if(cl==0x1e)al=0x1d;
            else{
                std::uint32_t j=0;
                for(;;++j){std::uint8_t b;if(!v.u8(0x5b3c74u+2u*j,b))return fail(s,0x47d5b4);if(std::int8_t(b)>=0&&b==cl)break;}
                v.u8(0x5b3c75u+2u*j,al);
            }
            r.p[0x72]=al;
            r.p[0x73]=std::uint8_t(e4);
            if(e1c!=0.0f||std::isnan(e1c))r.f(0x38,e1c);
            std::int32_t cc,c8b;if(!ri(cfg+0xc,cc)||!ri(cfg+8,c8b))return fail(s,0x47d616);
            auto timing=[&](){
                if(!e18)return false;
                const float fe=float(e18);r.f(0x40,fe);r.d(0x3c,ftol(fe)&0xffffu);
                const float a=x87_int_minus(edi,r.gf(0x40));
                r.f(0x44,x87_ratio(a,r.gf(0x34),K5B4470));
                return true;
            };
            bool to_71a=false;
            if(cc!=0){
                const float x1=float(c8b-cc)/float(count-1);
                const float x0=float(ec-1)*x1+float(cc);
                r.f(0x34,x0);per=x0;
                timing();to_71a=true;
            }else if(e14!=0){
                per=float(e14);r.f(0x34,per);
                timing();to_71a=true;
            }else{
                if(e10!=0)r.f(0x00,float(e10)*K5B4474);
                timing();
            }
            if(to_71a)r.f(0x00,x87_int_ratio(edi,std::int32_t(r.gd(0x3c)),per,K5B4478,K5B4474));
            if(e20!=0.0f||std::isnan(e20))r.f(0x04,e20);
            if(e8>=0&&e8<6){
                r.p[0x76]=std::uint8_t(e8);r.d(0x60,1);st.special_80fb28=1;
                if(e24){
                    float a,b;if(!rf(e24+4,b)||!rf(e24,a))return fail(s,0x47d7f1);
                    const auto seed=step(st.seed_6a4e2c);
                    float x0=float(std::int32_t(seed))*b;x0*=K5B4324;x0+=a;
                    r.f(0x20,x0);if(x0>K62806C)r.f(0x20,K62806C);
                }else r.f(0x20,st.rival_8514a4[std::size_t(e8)]);
                std::uint32_t name;if(!rd(0x6af250u+std::uint32_t(e8)*4u,name))return fail(s,0x505390);
                const auto* src=v.at(name,32);if(!src)return fail(s,0x47d85b);
                std::memcpy(r.p+0x78,src,32);
            }
            std::int32_t next;if(!ri(e+0x28,next))return fail(s,0x47d863);
            if(next<0)break;
        }
    }
    std::uint32_t flags;if(!rd(cfg+0x30,flags))return fail(s,0x47d879);
    if(!(flags&8u)&&!(flags&0x10u))st.flag_80fb08=1;
    return true;
}
}
