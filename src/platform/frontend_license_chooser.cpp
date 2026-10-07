#include "frontend_license_chooser.hpp"
#include "driving/pc_x87.hpp"
#include <algorithm>
#include <cstring>
namespace outrun::platform {
namespace {
using driving::Bytes;
unsigned bits(float f){unsigned u;std::memcpy(&u,&f,4);return u;}
struct Calls {
    LicenseEditorServices& services;bool ok{true};
    unsigned operator()(unsigned pc,unsigned offset=0,std::initializer_list<unsigned> args={}){
        unsigned result{};
        if(ok&&(!services.call||!services.call(services.user,pc,offset,args.begin(),args.size(),result))){
            ok=false;if(!services.missing_pc)services.missing_pc=pc;
        }return result;
    }
    void tick(unsigned off){(*this)(0x4659f0,off);}
    void release(unsigned off){(*this)(0x465250,off);}
    void ui(unsigned off,unsigned token,unsigned first,unsigned last,float x,float y){
        (*this)(0x465860,off,{token,first,last,1,0,bits(x),bits(y),bits(1),bits(1),bits(1),0});
        (*this)(0x465970,off);
    }
    void position(float x,float y){(*this)(0x4653c0,0x1b8,{bits(x),bits(y),0,0});}
};
void exit(Bytes b,LicenseChooserState& slide,Calls& c){
    slide.slide=0;slide.step=-38;slide.target=-380;
    c.release(0x258);c.release(0x2f8);b.put32(0x38,5);c(0x4e0d80);
}
bool select_mapping(Bytes b,FrontendProfiles& p,unsigned index){
    if(index>=4)return false;
    return frontend_profiles_select_448520(p,b.u32(0x9c94+12*index));
}
bool finish_exit(Bytes b,FrontendProfiles& p,LicenseChooserState& slide){
    b.put8(0x35,0);b.put32(0x38,0);b.put32(0x3c,~0u);b.put32(0x4c,0);b.put32(0x48,p.selected);
    if(p.selected!=~0u&&(p.active[0x3f4]&1u)==0){
        slide.restore_slot=b.i8(0x54);
        // This byte indexes the owner's mapping, NOT the save bank. Even -1
        // is a signed mapping read on PC, not an implicit deselect command.
        const auto off=std::size_t(0x9c94+12*int(slide.restore_slot));
        return frontend_profiles_select_448520(p,b.u32(off));
    }return true;
}
}
std::array<std::uint16_t,4> license_time_449b30(std::uint32_t frames){
    using driving::X87;
    const unsigned bits=0x3c881469;float rate;std::memcpy(&rate,&bits,4);
    // 449B30: fild low dword (+2^32 when negative), fadd high dword as float
    // (0 for these 32-bit frame counters), fmul rate. Each fadd/fmul rounds
    // to the x87 precision control; _ftol2 truncates the register value.
    X87 exact=X87(std::int32_t(frames));
    if(std::int32_t(frames)<0)exact=exact+X87(4294967296.0f);
    exact=(exact+X87(0.0f))*X87(rate);
    std::uint16_t seconds=static_cast<std::uint16_t>(driving::x87_ftol64(exact));
    float fraction=(driving::x87_float(exact)-float(seconds))*1000.f;
    if(fraction<0){--seconds;fraction+=1000.f;}
    // PC _ftol2 returns a 64-bit integer; only the low word is consumed.
    std::uint16_t millis=static_cast<std::uint16_t>(static_cast<std::int64_t>(fraction));
    if(millis>=1000){millis=std::uint16_t(millis-1000);++seconds;}
    const auto minutes=std::uint16_t(seconds/60);
    return {std::uint16_t(minutes/60),std::uint16_t(minutes%60),std::uint16_t(seconds%60),millis};
}
float license_slide_4e0a70(LicenseChooserState& s){
    s.slide+=s.step;
    if((s.step<0&&s.target>s.slide)||(s.step>0&&s.slide>s.target))s.slide=s.target;
    return s.slide;
}
bool license_chooser_init_4e19e0(Bytes b,FrontendProfiles& p,LicenseChooserState& slide,LicenseEditorServices& services){
    b.check(0,PcLicenseChooserBytes);Calls c{services};
    if(slide.restore_slot!=-1){
        if(!frontend_profiles_select_448520(p,unsigned(int(slide.restore_slot))))return false;
        slide.restore_slot=-1;
    }
    b.put8(0x35,0);b.put8(0x34,0);b.put32(0x578,2);b.put32(0x38,0);b.put32(0x3c,~0u);
    c(0x4e0c70);if(!c.ok)return false;
    b.put32(0x4c,0);b.put32(0x48,p.selected);b.put8(0x1323,0);
    c(0x442f20,0,{0,0});
    // Original 103C803 pushes 0x296 (FC665E71 + 0399A425), then
    // returns to 4E1A48: this is the eighth argument, not an empty bridge.
    c(0x440ea0,0,{4,0x295,~0u,~0u,~0u,~0u,8,0x296});return c.ok;
}
bool license_chooser_reset_4e1a70(Bytes b,FrontendProfiles& p,LicenseChooserState& slide){
    b.check(0,PcLicenseChooserBytes);return finish_exit(b,p,slide);
}
bool license_chooser_suspend_4e1ac0(Bytes b,FrontendProfiles& p,LicenseChooserState& slide,LicenseEditorServices& services){
    b.check(0,PcLicenseChooserBytes);Calls c{services};
    c(16,0x9d54);c(0x4eda60,0xb004);if(!c.ok||!finish_exit(b,p,slide))return false;
    // 4D8 and the special panel are deliberately absent from this PC suspend
    // entry. Their owner/destructor lifecycle must not be conflated with it.
    for(unsigned off:{0x1b8u,0x258u,0x2f8u,0x398u,0x438u})c.release(off);
    for(unsigned i=0;i<3;++i)for(unsigned j=0;j<4;++j)c.release(0x555c+i*0x225c+j*0xa0);
    if(!c.ok)return false;slide.restore_slot=-1;return true;
}
void license_chooser_list_4e1c00(Bytes b,FrontendProfiles& profiles){
    b.check(0,PcLicenseChooserBytes);auto list=frontend_profiles_list_4e1c00(profiles);
    b.put32(0x40,list.occupied);
    const auto count=list.occupied+(list.new_index>=0?1u:0u);
    for(unsigned i=0;i<count;++i){
        b.put32(0x9c94+12*i,list.slots[i]);b.put32(0x9c98+12*i,~0u);b.put8(0x9c9c+12*i,0);
    }
}
bool license_chooser_context_4e1cf0(Bytes b,FrontendProfiles& p,LicenseChooserState& slide,
                                   LicenseEditorServices& services,unsigned& result){
    b.check(0,PcLicenseChooserBytes);Calls c{services};result=0;
    const auto index=b.u32(0x44);if(index>=4)return false;
    const auto mapping=0x9c94+12*index;
    if(b.u8(0x36)){
        const auto answer=static_cast<int>(c(0x4e17e0));c(8,0xb040);
        if(!c.ok)return false;
        if(answer==-1||answer==1){
            b.put8(0x36,0);c(16,0xb040);c(0x48e440,0xc2f0);
            if(answer==1){
                b.put8(0x1323,1);c(0x416500,0,{b.u32(mapping)});if(!c.ok)return false;
                if(b.u32(mapping)==p.selected){
                    if(index>0){b.put32(0x44,index-1);if(!select_mapping(b,p,index-1))return false;}
                    else{b.put32(0x44,1);
                        if(b.u32(0x3c)==1){if(!frontend_profiles_select_448520(p,~0u))return false;}
                        else if(!select_mapping(b,p,1))return false;
                    }
                }
                c(0x416420);c(16);c(4);
            }
        }
        return c.ok;
    }
    c(8,0x9d54);const int input=static_cast<int>(c(0x48f5f0,0,{1}));if(!c.ok)return false;
    switch(input){
    case 0:
        switch(b.u32(0xb004)){
        case 0:
            if(b.u8(mapping+8))break;
            if(!select_mapping(b,p,index))return false;
            if(index==b.u32(0x3c)){
                p.active[0x3f4]|=1;c(0x465eb0,0,{0x219});if(!c.ok)return false;
                frontend_license_name_4dd590(p.active,services.name_text);b.put32(4,24);b.put32(0x578,4);
            }else{
                const auto previous=c(0x442ec0);if(!c.ok)return false;
                if(index!=b.u32(0x48)){b.put32(4,1);b.put32(0x578,previous?3:1);}
                else if(!previous){b.put32(4,1);b.put32(0x578,1);}
                else b.put32(0x578,2);
            }
            c(0x4164d0);c(0x4e1680);if(!c.ok)return false;exit(b,slide,c);break;
        case 1:
            b.put32(4,24);b.put32(0x578,4);services.editing_existing=1;
            if(b.u32(mapping)!=p.selected){slide.restore_slot=static_cast<std::int8_t>(p.selected);
                if(!select_mapping(b,p,index))return false;}
            // Editing an existing slot does not write common.dat here.
            slide.slide=0;slide.step=-38;slide.target=-380;c(0x4e1680);
            if(c.ok)exit(b,slide,c);
            break;
        case 2:b.put8(0xb016,1);c(0x4e16a0);b.put8(0x36,1);break;
        default:break; // Other entries are intentionally inert in this PC routine.
        }break;
    case 1:result=2;break;
    case 2:c(0x4ed250,0xb004);break;
    case 4:c(0x4ed2a0,0xb004);break;
    default:break;
    }
    return c.ok;
}
bool license_chooser_tick_4e2970(Bytes b,FrontendProfiles& p,LicenseChooserState& slide,
                                LicenseEditorServices& services,unsigned& result){
    b.check(0,PcLicenseChooserBytes);Calls c{services};result=0;
    const auto previous=c(0x442ec0);if(!c.ok)return false;
    switch(b.u32(0x38)){
    case 0:b.put32(0x38,1);break;
    case 1:{
        c.ui(0x1b8,0x440075,0,0,480,-380);slide.slide=-380;slide.step=38;slide.target=0;
        license_chooser_list_4e1c00(b,p);
        const auto count=b.u32(0x40);
        for(unsigned i=0;i<count;++i){
            const auto& record=p.licenses[b.u32(0x9c94+12*i)];
            unsigned j=0;for(;j<21&&record[j];++j)b.put8(0x58+22*i+j,record[j]);b.put8(0x58+22*i+j,0);
        }
        b.put32(0x44,p.selected);if(b.i32(0x44)<0||b.u32(0x44)>=count)b.put32(0x44,0);
        const auto free=frontend_profiles_free_slot(p);b.puti(0x50,free);
        if(free>=0){
            b.put32(0x40,count+1);b.put32(0x3c,count);c(0x465eb0,0,{0x218});
            if(c.ok){
                const auto& label=services.localized_text;
                if(!label.empty()){
                    const auto length=std::min<std::size_t>(21,label.size());
                    for(unsigned j=0;j<length;++j)b.put8(0x58+22*count+j,std::uint8_t(label[j]));
                    b.put8(0x58+22*count+length,0);
                }else{for(unsigned j=0;j<16;++j)b.put8(0x58+22*count+j,services.name_text[j]);b.put8(0x58+22*count+16,0);}
            }
        }
        b.put32(0x38,2);break;
    }
    case 2:
        c.position(480,license_slide_4e0a70(slide));c.tick(0x1b8);
        if(slide.slide==0){
            if(previous==12||previous==13){b.put32(0x38,11);c(0x4e0dd0);}
            else{
                b.put32(0x38,3);
                if(b.i32(0x44)>0)c.ui(0x258,0x44006d,1,1,-200,0);
                if(b.i32(0x44)<b.i32(0x40)-1)c.ui(0x2f8,0x44006c,1,1,200,0);
                c(0x4e2150);
            }
        }break;
    case 3:{
        c.tick(0x1b8);const int action=static_cast<int>(c(0x48f5f0,0,{1}));c.tick(0x258);c.tick(0x2f8);
        if(!c.ok)break;
        if(action==0){
            if(b.u32(0x44)==b.u32(0x3c)){
                b.put32(0x54,p.selected);
                if(!select_mapping(b,p,b.u32(0x44)))return false;
                p.active[0x3f4]|=1;c(0x465eb0,0,{0x219});
                if(c.ok)frontend_license_name_4dd590(p.active,services.name_text);
                b.put32(4,24);b.put32(0x578,4);exit(b,slide,c);
            }else{c(0x4e12b0);b.put32(0x38,4);}
        }else if(action==1){b.put32(4,0);b.put32(0x578,p.selected==~0u?3:2);exit(b,slide,c);}
        else if(action==3&&b.i32(0x44)>0){
            c.release(0x258);c.ui(0x258,0x44006d,2,22,-200,0);
            b.put32(0x38,7);slide.slide=480;slide.step=48;slide.target=960;c(0x4e0d80);c(0x4249f0,0,{1});
        }else if(action==5&&b.i32(0x44)<b.i32(0x40)-1){
            c.release(0x2f8);c.ui(0x2f8,0x44006c,2,22,200,0);
            b.put32(0x38,6);slide.slide=480;slide.step=-48;slide.target=0;c(0x4e0d80);c(0x4249f0,0,{1});
        }break;
    }
    case 4:
        c.tick(0x1b8);
        {unsigned context_result{};
            if(!c.ok||!license_chooser_context_4e1cf0(b,p,slide,services,context_result))return false;
            if(context_result){b.put32(0x38,3);c(0x4e1680);}
        }break;
    case 5:
        c.position(480,license_slide_4e0a70(slide));c.tick(0x1b8);
        if(slide.slide==-380){
            if(!finish_exit(b,p,slide))return false;
            c.release(0x1b8);result=b.u32(0x578);
        }break;
    case 6:case 7:{
        const bool right=b.u32(0x38)==6;c.tick(0x258);c.tick(0x2f8);
        c.position(license_slide_4e0a70(slide),0);
        if(slide.slide==(right?0.0f:960.0f)){
            b.put32(0x38,right?8:9);slide.slide=right?960.0f:0.0f;slide.step=right?-48.0f:48.0f;slide.target=480;
        }break;
    }
    case 8:case 9:{
        const bool right=b.u32(0x38)==8;c.tick(0x258);c.tick(0x2f8);c.position(license_slide_4e0a70(slide),0);
        if(slide.slide==480){
            c.position(480,0);b.put32(0x38,3);
            if(right){
                if(b.u32(0x44)==0)c.ui(0x258,0x44006d,1,1,-200,0);
                b.put32(0x44,b.u32(0x44)+1);c.release(0x2f8);
                if(b.i32(0x44)<b.i32(0x40)-1)c.ui(0x2f8,0x44006c,1,1,200,0);
            }else{
                if(b.i32(0x44)==b.i32(0x40)-1)c.ui(0x2f8,0x44006c,1,1,200,0);
                b.put32(0x44,b.u32(0x44)-1);c.release(0x258);
                if(b.i32(0x44)>0)c.ui(0x258,0x44006d,1,1,-200,0);
            }c(0x4e2150);
        }break;
    }
    case 11:{
        c.tick(0x1b8);const auto action=c(0x48f5f0,0,{1});
        if(action<=1){
            b.put32(0x578,2);slide.slide=0;slide.step=-38;slide.target=-380;
            c.release(0x258);c.release(0x2f8);b.put32(0x38,5);
            for(auto off:{0x3300u,0x33a0u,0x3440u,0x34e0u})c.release(off);
        }break;
    }
    default:break;
    }
    if(!c.ok)result=0;
    return c.ok;
}
}
