#include "system/exe_image.hpp"
#include "frontend_keyboard.hpp"
#include "title_owner.hpp"
#include <algorithm>
#include <cstring>

namespace outrun::platform {
using driving::Bytes;
namespace {
// Retail BD AFA88A... image: 5B0720/5B09E0, 44 cells x left/up/right/down.
alignas(4) static unsigned navigation[44][4]{}; OR2_EXE_COPY(navigation,0x5B0720u,0x2C0u);
unsigned next_cell(unsigned cell,unsigned dir,bool alternate){
    if(alternate){
        if(cell==0&&dir==0)return 9;
        if(cell==9&&dir==2)return 0;
        if(cell==30&&dir==0)return 38;
        if(cell==38&&dir==2)return 30;
        if(cell==40&&dir==1)return 43;
        if(cell==41&&dir==3)return 43;
        if(cell==43&&dir==1)return 41;
        if(cell==43&&dir==3)return 40;
    }return navigation[cell][dir];
}
alignas(4) static float positions[44][2]{}; OR2_EXE_COPY(positions,0x64C6D8u,0x160u);
alignas(4) static std::uint8_t symbols[37]{}; OR2_EXE_COPY(symbols,0x64C865u,0x25u);
alignas(4) static std::uint8_t accents[37]{}; OR2_EXE_COPY(accents,0x64C8D4u,0x25u);
unsigned bits(float f){unsigned u;std::memcpy(&u,&f,4);return u;}
struct Calls {
    Bytes b;FrontendUiResources& ui;bool ok=true;
    unsigned operator()(unsigned pc,unsigned off,std::initializer_list<unsigned> a={}){
        unsigned r{};
        if(ok){
            ok=ui.call(pc,b.sub(off,0xa0),a.begin(),a.size(),r);
            // A missing ETC archive must fail the integration, never look like
            // an instantly completed opening because its handle is -1.
            if(ok&&pc==0x465970&&b.u32(off+8)==~0u)ok=false;
        }
        return r;
    }
    void configure(unsigned off,unsigned token,int first,int last,unsigned mode,float x,float y,float speed=1){
        (*this)(0x465860,off,{token,unsigned(first),unsigned(last),12,mode,bits(x),bits(y),bits(1),bits(1),bits(speed),0});
    }
    void position(unsigned off,float x,float y){(*this)(0x4653c0,off,{bits(x),bits(y),0,0});}
    void release_alphabets(){
        auto count=b.i32(0x550);if(count<0||count>7){ok=false;return;}
        for(int i=0;i<count;++i)(*this)(0x465250,0xf0+i*0xa0);
    }
};
bool sound(FrontendKeyboardServices& s,unsigned id){return s.sound&&s.sound(s.user,id);}
bool exit_animation(Bytes b,FrontendUiResources& ui){
    Calls c{b,ui};c(0x465250,0x554);c.release_alphabets();c(0x465250,0x4c);
    c.configure(0x4c,0x2c0110,20,0,3,b.f32(0x34),b.f32(0x38),-1);
    c(0x465970,0x4c);b.put8(0x6fc,1);return c.ok;
}
}
bool keyboard_construct_468e40(Bytes b,unsigned& repeat){
    if(b.size()<PcKeyboardBytes)return false;
    std::array<std::uint8_t,0x34> base{};for(unsigned i=0;i<base.size();++i)base[i]=b.u8(i);
    title_base_construct_48f480(base.data(),base.size(),repeat);
    for(unsigned i=0;i<base.size();++i)b.put8(i,base[i]);
    for(auto o:{0x34u,0x38u,0x3cu,0x40u})b.put32(o,0);
    b.put8(0x4d,0);
    b.put32(0,0x5b0ca0);
    auto resource=[&](unsigned o){std::array<std::uint8_t,0xa0> data;
        for(unsigned i=0;i<data.size();++i)data[i]=b.u8(o+i);
        title_ui_resource_construct_465160(data.data(),data.size());
        for(unsigned i=0;i<data.size();++i)b.put8(o+i,data[i]);};
    resource(0x4c);for(unsigned i=0;i<7;++i)resource(0xf0+i*0xa0);resource(0x554);
    b.put8(0x6fc,0);b.put32(0x708,0);for(unsigned i=0;i<257;++i)b.put8(0x5f4+i,0);
    for(auto o:{0x6f6u,0x6f8u,0x6fau})b.put16(o,0);
    b.put32(8,18);b.put32(4,24);b.put32(0x550,6);b.put32(0x704,13);return true;
}
std::string keyboard_name_468770(Bytes b){
    std::string s;for(unsigned i=0;i<257&&b.u8(0x5f4+i);++i)s+=char(b.u8(0x5f4+i));return s;
}
bool keyboard_name_468710(Bytes b,std::string_view s){
    if(b.size()<PcKeyboardBytes||s.size()>256||s.find('\0')!=s.npos)return false;
    // The PC strncpy fills the remainder of its 256-byte destination with NUL.
    for(unsigned i=0;i<256;++i)b.put8(0x5f4+i,i<s.size()?std::uint8_t(s[i]):0);
    b.put16(0x6f6,std::uint16_t(s.size()));return true;
}
void keyboard_limits_468780(Bytes b,int minimum,int maximum){
    if(maximum>=256)maximum=255;
    minimum=std::min(minimum,maximum);
    b.put16(0x6f8,std::uint16_t(minimum));b.put16(0x6fa,std::uint16_t(maximum));
}
bool keyboard_append_468d80(Bytes b,std::uint8_t ch){
    const int len=b.i16(0x6f6);if(len<0)return false;
    if(len<256&&len<b.i16(0x6fa)-1){b.put8(0x5f4+len,ch);b.put16(0x6f6,len+1);}return true;
}
std::uint8_t keyboard_character_468d40(Bytes b,unsigned cell){
    if(cell==37)cell=36;
    if(cell>36)return 0;
    switch(b.u32(0xec)){
    case 1:return std::uint8_t("1234567890abcdefghijklmnopqrstuvwxyz "[cell]);
    case 2:return symbols[cell];case 3:case 4:return std::uint8_t("1234567890ABCDEFGHIJKLMNOPQRSTUVWXYZ "[cell]);
    case 5:return accents[cell];default:return 0;
    }
}
bool keyboard_position_4687c0(Bytes b,FrontendUiResources& ui,float x,float y){
    Calls c{b,ui};const auto count=b.i32(0x550);if(count<0||count>7)return false;
    for(int i=0;i<count;++i)c.position(0xf0+i*0xa0,x,y);
    c.position(0x4c,x,y);c.position(0x554,x+132-320,y+187-240);
    b.putf(0x34,x);b.putf(0x38,y);return c.ok;
}
bool keyboard_init_468880(Bytes b,FrontendUiResources& ui,FrontendKeyboardServices& s){
    if(b.size()<PcKeyboardBytes||!s.focus||s.focused)return false;
    b.put8(0x6fc,0);b.put32(0xec,1);for(unsigned i=0;i<257;++i)b.put8(0x5f4+i,0);
    s.selected=0;for(auto o:{0x6f6u,0x6f8u,0x6fau})b.put16(o,0);
    Calls c{b,ui};const float x=b.f32(0x34),y=b.f32(0x38);
    c.configure(0x4c,0x2c0110,0,20,3,x,y);c(0x465970,0x4c);
    for(unsigned i=1;i<=5;++i)c.configure(0xf0+i*0xa0,0x2c0110,19+i,19+i,0,x,y);
    b.put32(0x550,6);for(unsigned i=0;i<6;++i)c.position(0xf0+i*0xa0,x,y);
    c.configure(0x554,0x2c012f,0,30,0,x+132-320,y+187-240);
    if(!c.ok||!s.focus(s.user,true))return false;
    s.focused=true;return true;
}
bool keyboard_move_468c00(Bytes b,FrontendUiResources& ui,FrontendKeyboardServices& s,unsigned dir){
    const auto old=s.selected;if(dir>=4||old>=44)return false;
    s.selected=next_cell(old,dir,(b.u32(0x708)&2)!=0);const auto cell=s.selected;
    if(cell!=old&&!sound(s,1))return false;
    Calls c{b,ui};unsigned token{};
    if(cell<36&&old>=36)token=0x2c012f;
    else if(cell==37&&cell!=old)token=0x2c012c;
    else if(cell==38&&cell!=old)token=0x2c012d;
    else if(cell>=39&&cell!=old)token=0x2c0130;
    if(token){c(0x465250,0x554);b.put32(0x554,token);}
    c.position(0x554,b.f32(0x34)+positions[cell][0]-320,b.f32(0x38)+positions[cell][1]-240);
    if(b.u32(0x55c)==~0u)c(0x465970,0x554);
    return c.ok;
}
bool keyboard_suspend_468f20(Bytes b,FrontendUiResources& ui,FrontendKeyboardServices& s){
    Calls c{b,ui};c(0x465250,0x554);c.release_alphabets();c(0x465250,0x4c);b.put32(0x708,0);
    if(!c.ok)return false;
    if(s.focused){if(!s.focus||!s.focus(s.user,false))return false;s.focused=false;}
    return true;
}
bool keyboard_finish_469030(Bytes b,FrontendUiResources& ui,FrontendKeyboardServices& s,bool accept){
    if(accept&&std::uint32_t(b.i16(0x6f8))>keyboard_name_468770(b).size())return true;
    b.put32(4,83);b.put32(0x700,accept?1:2);
    return exit_animation(b,ui)&&sound(s,accept?64:0);
}
bool keyboard_tick_469130(Bytes b,FrontendUiResources& ui,FrontendKeyboardServices& s,unsigned& result){
    result=0;if(b.size()<PcKeyboardBytes)return false;Calls c{b,ui};
    if((b.u32(0x708)&2)&&b.u32(0x4b8)==~0u&&c(0x4652e0,0x4c)){
        c.configure(0x4b0,0x2c0110,29,29,0,b.f32(0x34),b.f32(0x38));b.put32(0x550,7);
        c(0x465970,0x4b0);c(0x4659f0,0x4b0);
    }
    c(0x4659f0,0x4c);if(!c(0x4652e0,0x4c))return c.ok;
    if(b.u8(0x6fc)){if(!keyboard_suspend_468f20(b,ui,s))return false;result=b.u32(0x700);return true;}
    if(b.u32(0x55c)==~0u)c(0x465970,0x554);
    c(0x4659f0,0x554);
    const auto alphabet=b.u32(0xec);if(alphabet<1||alphabet>5)return false;
    if(b.u32(0xf8+alphabet*0xa0)==~0u){c.release_alphabets();c(0x465970,0xf0+alphabet*0xa0);
        if(b.u32(0x550)==7)c(0x465970,0x4b0);}
    if(b.u32(0x550)>7)return false;
    for(unsigned i=0;i<b.u32(0x550);++i)c(0x4659f0,0xf0+i*0xa0);
    int action=-1;if(!c.ok||!s.input||!s.input(s.user,b,action))return false;
    if(unsigned(action)>5)return true;
    if(action==1)return keyboard_finish_469030(b,ui,s,false);
    if(action>=2){constexpr unsigned dirs[]={1,0,3,2};return keyboard_move_468c00(b,ui,s,dirs[action-2]);}
    const auto cell=s.selected;if(cell>=44)return false;
    if(cell<38){
        if(b.i16(0x6f6)>=b.i16(0x6fa)-1)return sound(s,3);
        if(!keyboard_append_468d80(b,keyboard_character_468d40(b,cell)))return false;
        if(alphabet==3)b.put32(0xec,1);
        return sound(s,64);
    }
    if(cell==38){const int len=b.i16(0x6f6);if(len<0||len>256)return false;
        if(len)b.put16(0x6f6,len-1);
        if(!sound(s,len?64:3))return false;
        b.put8(0x5f4+b.i16(0x6f6),0);return true;}
    if(cell<43){const unsigned target=cell-37,mask=1u<<(cell-38);
        if(b.u32(0x708)&mask)return true;
        c(0x465250,0xf0+alphabet*0xa0);
        b.put32(0xec,alphabet==target?1:target);return c.ok&&sound(s,64);}
    // The tick's short-name branch plays 0x40; 469030 itself stays silent.
    if(std::uint32_t(b.i16(0x6f8))>keyboard_name_468770(b).size())return sound(s,64);
    return keyboard_finish_469030(b,ui,s,true);
}
}
