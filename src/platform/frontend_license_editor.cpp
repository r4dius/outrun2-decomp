#include "frontend_license_editor.hpp"
#include <algorithm>
#include <cstring>
namespace outrun::platform {
namespace {
using driving::Bytes;
std::uint32_t bits(float x){std::uint32_t n;std::memcpy(&n,&x,4);return n;}
struct Calls {
    LicenseEditorServices& s;bool ok{true};
    std::uint32_t operator()(std::uint32_t pc,std::size_t off=0,
                             std::initializer_list<std::uint32_t> args={}){
        std::uint32_t result{};
        if(ok&&(!s.call||!s.call(s.user,pc,off,args.begin(),args.size(),result))){
            ok=false;if(!s.missing_pc)s.missing_pc=pc;
        }
        return result;
    }
    void reset(unsigned off){(*this)(0x465250,off);}
    void tick(unsigned off){(*this)(0x4659f0,off);}
    void ui(unsigned off,unsigned token,unsigned first,unsigned last,unsigned flags,
            float x=0,float y=0,float speed=1,unsigned entry=0){
        (*this)(0x465860,off,{token,first,last,flags,entry,bits(x),bits(y),bits(1),bits(1),bits(speed),0});
        (*this)(0x465970,off);
    }
};
void display(Calls& c){for(auto off:{0x4cu,0x18cu,0xecu,0x22cu})c.tick(off);c(12,0x5ec);}
void exit_setup(Bytes b,Calls& c){
    for(auto off:{0x4cu,0x18cu,0xecu,0x22cu,0x4acu,0x54cu})c.reset(off);
    c.ui(0x4ac,0x440073,51,0,5,0,0,-1,1);
    c.ui(0x54c,0x440081,51,0,5,-8,0,-1,1);b.put32(0x38,6);
}
void highlight(Bytes b,Calls& c,unsigned choice){
    // Original protected 4DD994 load resolves to this+4AC. Verified by the
    // original arithmetic bridge in frontend_license_probe.
    c.reset(0x4ac);c.ui(0x4ac,0x440073,choice+47,choice+47,5);
    if(choice==8){c.reset(0x22c);c.ui(0x22c,0x440076,2,2,4,-84,110);}
    c(0x4249f0,0,{1});(void)b;
}
void keyboard_open(Bytes b,Calls& c){
    b.put8(0x1184,0);c(4,0xa78);b.put8(0x1184,1);
    c(0x468780,0xa78,{1,16});c(0x48eec0,0x5ec);
    c(0x468710,0xa78);c(0x4687c0,0xa78,{0,bits(60)});c(0x468d70,0xa78,{1});
}
void name_result(Bytes b,PcLicense& p,Calls& c,unsigned result){
    if(result==1||result==2){
        b.put8(0x1184,0);c(0x468d70,0xa78,{0});
        if(result==1){c(0x48eec0,0x5ec);if(c.ok){p[0x3f4]|=2;frontend_license_name_4dd590(p,c.s.name_text);}}
    }else if(result<=5&&b.u8(0x1184)){
        c(0x468770,0xa78);c(0x48ee80,0x5ec);
    }
}
void menu(Bytes b,PcLicense& p,int action,Calls& c){
    const auto item=b.u32(0x3c);
    auto select=[&](unsigned next){b.put32(0x3c,next);highlight(b,c,next+4);};
    if(item==0){
        if(action==0){b.put32(0x38,2);keyboard_open(b,c);}
        else if(action==4)select(2); // PC deliberately skips item 1 from name.
    }else if(item==1){
        if(action==0){b.put32(0x38,3);auto f=b.u32(0x40)+1;c.ui(0x2cc,0x440072,f,f,4);}
        else if(action==2)select(0);else if(action==4)select(2);
    }else if(item==2){
        if(action==0){b.put32(0x38,4);auto f=b.u32(0x44)+1;c.ui(0x36c,0x440074,f,f,4);}
        else if(action==2)select(0);else if(action==4||action==5)select(3);
    }else if(item==3){
        if(action==0){b.put32(0x38,5);auto f=b.u32(0x48)+1;c.ui(0x40c,0x440071,f,f,4);}
        else if(action==2||action==3)select(2);else if(action==4)select(4);
    }else if(item==4){
        if(action==2){select(3);c.reset(0x22c);c.ui(0x22c,0x440076,1,1,4,-84,110);}
        else if(action==0){
            if(c.s.editing_existing){c.s.editing_existing=0;b.put32(0x11a0,2);}
            else {b.put32(4,1);b.put32(0x11a0,1);}
            exit_setup(b,c);
        }
    }
    (void)p;
}
}
bool license_editor_init_4dd7c0(Bytes b,PcLicense& p,LicenseEditorServices& s){
    b.check(0,PcLicenseEditorBytes);s.missing_pc=0;Calls c{s};
    c.ui(0x4ac,0x440073,0,51,5,0,0,1,1);c.ui(0x54c,0x440081,0,51,4,-8,0,1,1);
    if(!c.ok)return false;
    const Bytes active(p.data(),p.size());b.put32(0x38,0);
    b.put32(0x40,active.u32(0x20));b.put32(0x44,active.u32(0x1c));b.put32(0x48,active.u32(0x18));
    for(unsigned i=0;i<16;++i)b.put8(0x1186+i,p[i]);
    b.put32(0x1198,active.u32(0x1c));b.put32(0x119c,active.u32(0x18));
    b.put8(0x1184,0);b.put8(0x1185,0);return true;
}
bool license_editor_grid(Bytes b,PcLicense& p,unsigned stage,int action,LicenseEditorServices& s){
    b.check(0,PcLicenseEditorBytes);if(stage<3||stage>5)return false;
    Calls c{s};
    const unsigned index=stage-3,off=0x2cc+index*0xa0,field=0x40+index*4;
    constexpr unsigned tokens[]={0x440072,0x440074,0x440071};
    constexpr unsigned columns[]={2,4,3},counts[]={6,26,12};
    constexpr unsigned small_tokens[]={0x44006a,0x440077,0x440078};
    constexpr unsigned small_offsets[]={0x4c,0xec,0x18c},profile_offsets[]={0x20,0x1c,0x18};
    constexpr float xs[]={-199,-216,-182},ys[]={-8,44,44};
    c.tick(off);const auto old=b.i32(field);auto selected=old;
    if(action==0){
        c.reset(small_offsets[index]);c.ui(small_offsets[index],small_tokens[index],old,old,4,xs[index],ys[index]);
        c.reset(off);
        if(c.ok){b.put32(0x38,1);Bytes(p.data(),p.size()).puti(profile_offsets[index],old);p[0x3f4]|=2;}
    }else if(action==1){b.put32(0x38,1);c.reset(off);}
    else {
        const auto cols=int(columns[index]),count=int(counts[index]);
        if(action==2&&selected>=cols)selected-=cols;
        else if(action==4&&selected<count-cols)selected+=cols;
        else if(action==3&&selected%cols!=0)--selected;
        else if(action==5&&selected%cols!=cols-1&&selected<count-1)++selected;
        if(selected!=old){b.puti(field,selected);c.reset(off);c.ui(off,tokens[index],selected+1,selected+1,4);c(0x4249f0,0,{1});}
    }
    return c.ok;
}
bool license_editor_tick_4de2b0(Bytes b,PcLicense& p,LicenseEditorServices& s,std::uint32_t& result){
    b.check(0,PcLicenseEditorBytes);result=0;Calls c{s};
    const int input=static_cast<std::int32_t>(c(0x48f5f0,0,{b.u8(0x1184)||b.u32(0x38)==6?~0u:1u}));
    c.tick(0x4ac);c.tick(0x54c);if(!c.ok)return false;
    switch(b.u32(0x38)){
    case 0:
        if(c(0x4652e0,0x4ac)){
            c.reset(0x4ac);c.reset(0x54c);b.put32(0x38,1);b.put32(0x3c,0);
            c.ui(0x4ac,0x440073,51,51,5);c.ui(0x54c,0x440081,51,51,4,-8);
            c.ui(0x4c,0x44006a,b.u32(0x40),b.u32(0x40),4,-199,-8);
            c.ui(0xec,0x440077,b.u32(0x44),b.u32(0x44),4,-216,44);
            c.ui(0x18c,0x440078,b.u32(0x48),b.u32(0x48),4,-182,44);
            c.ui(0x22c,0x440076,1,1,4,-84,110);
            c(4,0x5ec);std::copy_n(p.begin(),16,s.name_text.begin());
            c(0x48f280,0x5ec,{0x626468,9,~0u});c(0x48e530,0x5ec,{bits(126),bits(168)});
            c(0x48ef40,0x5ec,{5});b.put8(0x1184,0);b.put8(0x1185,0);
        }break;
    case 1:
        display(c);
        if(input==1){
            b.put32(0x11a0,2);b.put8(0x1185,1);
            // Original name cheats are live branches, not dead EXE contents.
            if(std::memcmp(p.data(),"MILESANDMILES",14)==0){
                Bytes active(p.data(),p.size());active.putf(0x24,active.f32(0x24)+1000000.0f);p[0x3f4]|=2;
            }else if(std::memcmp(p.data(),"ENTIRETY",9)==0)frontend_license_unlock_447360(p);
            Bytes active(p.data(),p.size());active.put32(0x1c,b.u32(0x1198));active.put32(0x18,b.u32(0x119c));
            p[0x3f4]|=2;for(unsigned i=0;i<16;++i)p[i]=b.u8(0x1186+i);
            exit_setup(b,c);
        }else menu(b,p,input,c);
        break;
    case 2:
        display(c);
        if(b.u8(0x1184)){auto action=c(8,0xa78);if(c.ok)name_result(b,p,c,action);}
        else b.put32(0x38,1);
        break;
    case 3:case 4:case 5:
        display(c);if(c.ok)return license_editor_grid(b,p,b.u32(0x38),input,s);break;
    case 6:
        if(c(0x4652e0,0x4ac)&&c.ok){
            if(!b.u8(0x1185)){
                b.put32(0x38,7);result=c(0x416420);
                if(result==2&&b.u32(0x11a0)==1)result=1;
            }else{
                if(s.editing_existing)s.editing_existing=0;else p[0x3f4]&=0xfe;
                b.put8(0x1185,0);result=b.u32(0x11a0);
            }
        }break;
    case 7:result=b.u32(0x11a0);break;
    default:break;
    }
    if(!c.ok)result=0;
    return c.ok;
}
}
