#include "platform/frontend_rankings.hpp"
#include "platform/frontend_rankings50_tables.hpp"
#include "platform/sp_rankings.hpp"
#include "driving/pc_x87.hpp"
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iterator>
namespace outrun::platform {
using driving::Bytes;using driving::X87;using driving::x87_float;
namespace {
struct Fault {unsigned pc;};
[[noreturn]] void fault(unsigned pc){throw Fault{pc};}
using S=FrontendRankingServices;
constexpr unsigned Rec=0x144c,ArrA=0x30,ArrB=0x130a4,List=0x33c,One=0x3f800000;
// 51C050 record: text +68 (score digits), +4F4 (name), +980 (time); UI
// resources +E0C, +EAC (never drawn), +F4C .. +13AC.
constexpr unsigned Drawn[9]{0xe0c,0xf4c,0xfec,0x108c,0x112c,0x11cc,0x126c,0x130c,0x13ac};
unsigned bits(float f){unsigned u;std::memcpy(&u,&f,4);return u;}
// cvttss2si: out of range gives the integer indefinite value.
std::int32_t cvtt(float f){if(!(f>-2147483904.f&&f<2147483648.f))return std::int32_t(0x80000000u);return std::int32_t(f);}
Bytes rec(Bytes list,unsigned array,int i){if(i<0||i>=15)fault(array==ArrA?0x51e140:0x51f280);return list.sub(array+unsigned(i)*Rec,Rec);}
Bytes recA(Bytes list,int i){return rec(list,ArrA,i);}
Bytes recB(Bytes list,int i){return rec(list,ArrB,i);}
// Row highlight flag (+34) addressed like the PC, [list+130D8+i*144C]: a
// focus index past the 15 displayed records still lands inside the list
// (header +26118..), so only a write leaving the list is latched.
Bytes flag_row(Bytes list,int i){const long long off=(long long)ArrB+(long long)i*Rec;
    if(off<0||off+0x38>(long long)list.size())fault(0x51f280);return list.sub(std::size_t(off),0x38);}
Bytes res_at(Bytes b,unsigned off){return b.sub(off,0xa0);}
std::uint32_t mode(const S& s){return s.root.u32(0x20c);}   // 535EF0()+0x208
void log(S& s,unsigned pc,const std::uint8_t* at,std::initializer_list<unsigned> a={}){
    if(!s.trace)return;std::array<std::uint32_t,14> e{};e[0]=pc;e[1]=at?unsigned(at-s.trace_base):0;
    unsigned i=2;for(unsigned v:a)if(i<e.size())e[i++]=v;s.trace->push_back(e);
}
void call(S& s,Bytes r,unsigned pc,std::initializer_list<unsigned> a={},unsigned* out=nullptr){
    unsigned result{};log(s,pc,r.data(),a);
    if(!s.ui.call(pc,r.sub(0,0xa0),a.size()?a.begin():nullptr,a.size(),result)||s.ui.missing_pc)fault(s.ui.missing_pc?s.ui.missing_pc:pc);
    if(out)*out=result;
}
void release(S& s,Bytes r){call(s,r,0x465250);}
void tick(S& s,Bytes r){call(s,r,0x4659f0);}
void configure(S& s,Bytes r,unsigned token,unsigned first,unsigned last,unsigned layer,unsigned mode,
               float x,float y,bool commit=true){
    call(s,r,0x465860,{token,first,last,layer,mode,bits(x),bits(y),One,One,One,0});
    if(commit)call(s,r,0x465970);
}
void sound(S& s,unsigned id){if(!s.ui.effect_4249f0||!s.ui.effect_4249f0(s.ui.effect_user,id))fault(0x4249f0);}
void labels(S& s,std::initializer_list<unsigned> a){
    unsigned result{};log(s,0x440ea0,nullptr,a);
    if(!s.ui.commands(0x440ea0,s.root.sub(0x51c,s.root.size()-0x51c),a.begin(),a.size(),s.globals,result))
        fault(s.ui.missing_pc?s.ui.missing_pc:0x440ea0);
}
const std::string& localized(S& s,unsigned id){const auto* v=s.text?s.text->get(id):nullptr;if(!v)fault(0x465eb0);return *v;}
const FrontendFont& font9(S& s){if(!s.fonts)fault(0x42ca60);return s.fonts->fonts[9];}
bool unlocked_4474e0(const S& s,int bit){
    int q=bit;if(q<0)q+=7;const int byte=q>>3;int b=bit%8;if(b<0)b+=8;   // and 0x80000007 / sar 3
    return (s.profile.at(std::size_t(0x28+byte))&(1u<<b))!=0;
}
// ---- text widget (48E590) helpers -----------------------------------------
std::string_view cstr(std::string_view v){return v.substr(0,v.find('\0'));}
// 48F280(widget,format,font,color): the argument is a printf FORMAT. Strings
// without '%' format to themselves; anything else is not reproduced.
void text_set(Bytes w,std::string_view format,unsigned font,unsigned color){
    format=cstr(format);if(format.find('%')!=std::string_view::npos)fault(0x48f280);
    if(!frontend_text_set_48f280(w,format,font,color))fault(0x48f280);
}
// 48F280 whose varargs are known stack words: "%d" consumes them in order,
// "%%" is a percent sign; any other conversion is not reproduced.
void text_format(Bytes w,std::string_view format,unsigned font,unsigned color,std::initializer_list<std::int32_t> args){
    format=cstr(format);std::string out;auto next=args.begin();
    for(std::size_t i=0;i<format.size();++i){
        if(format[i]!='%'){out+=format[i];continue;}
        if(i+1<format.size()&&format[i+1]=='%'){out+='%';++i;continue;}
        if(i+1<format.size()&&format[i+1]=='d'&&next!=args.end()){out+=std::to_string(*next++);++i;continue;}
        fault(0x48f280);
    }
    if(!frontend_text_set_48f280(w,out,font,color))fault(0x48f280);
}
// 48EE80(widget,format,...): vsnprintf into +4E, then +458 = strlen(FORMAT).
void text_printf(Bytes w,std::string_view format){
    format=cstr(format);if(format.find('%')!=std::string_view::npos||format.size()>=0x400)fault(0x48ee80);
    for(std::size_t i=0;i<format.size();++i)w.put8(0x4eu+i,std::uint8_t(format[i]));
    w.put8(0x4eu+format.size(),0);w.put32(0x458,unsigned(format.size()));
}
void text_position(Bytes w,float x,float y){w.putf(0x34,x);w.putf(0x38,y);}   // 48E530
// 48F2D0(widget,format block 48D570).
void text_apply_format(Bytes w,Bytes f){
    text_printf(w,std::string_view(reinterpret_cast<const char*>(f.data()),0x400));
    w.put32(0x45c,f.u32(0x408));for(unsigned i=0;i<16;i+=4)w.put32(0x460+i,f.u32(0x40c+i));
    if(w.i32(0x468)-w.i32(0x460)>0&&w.i32(0x46c)-w.i32(0x464)>0)w.put8(0x4d,1);
    w.put32(0x450,f.u32(0x400));w.put32(0x474,f.u32(0x420));w.put32(0x454,f.u32(0x404));
    w.put32(0x47c,f.u32(0x428));w.put32(0x478,f.u32(0x424));w.put8(0x470,f.u8(0x41c));
    w.put32(0x34,f.u32(0x42c));w.put32(0x38,f.u32(0x430));w.put32(0x480,f.u32(0x434));
}
void text_display(S& s,Bytes w,FrontendRankings& c){
    log(s,0x48f3c0,w.data());
    if(!s.fonts||!frontend_text_display_48f3c0(w,*s.fonts,c.lines,c.glyphs))fault(0x48f3c0);
}
// 4EDD50 with the current font 9 (42CA60(9) just before) and flags 0x100.
void scroll_set_4edd50(Bytes b,const FrontendFont& f,std::string_view t){
    t=cstr(t);FrontendTextStyle style;style.flags=0x100;
    b.puti(0x10,frontend_text_width_42c480(f,style,t));b.put32(0x18,unsigned(t.size()));
    for(unsigned i=0;i<100;++i)b.put8(0x24+i,i<t.size()?std::uint8_t(t[i]):0);
    b.put8(0x87,0);
}
// ---- tables -----------------------------------------------------------------
struct Frame {unsigned token,first,last;};
// 5E70F8: frame table indexed by the 51C710 results.
constexpr Frame Frames5e70f8[150]{
    {0x440023,0,10},{0x440023,10,10},{0x440023,11,11},{0x440023,10,0},{0x44002e,0,0},{0x44002e,1,1},{0x44002e,2,2},{0x44002e,3,3},{0x44002e,4,4},{0x44002e,5,5},
    {0x44002e,6,6},{0x44002e,7,7},{0x44002e,8,8},{0x44002e,9,9},{0x44002e,10,10},{0x44002e,11,11},{0x44002e,12,12},{0x44002e,13,13},{0x44002e,14,14},{0x44002e,15,15},
    {0x44002e,16,16},{0x44002e,17,17},{0x44002e,18,18},{0x44002e,19,19},{0x44002e,20,20},{0x44002e,21,21},{0x44002e,22,22},{0x44002e,23,23},{0x44002e,24,24},{0x44002e,25,25},
    {0x44002e,26,26},{0x44002e,27,27},{0x44002e,28,28},{0x44002e,29,29},{0x44002e,30,30},{0x44002e,31,31},{0x440025,0,0},{0x440025,1,1},{0x440026,0,0},{0x440026,1,1},
    {0x440026,2,2},{0x440026,3,3},{0x440026,4,4},{0x440026,5,5},{0x440026,6,6},{0x440026,7,7},{0x440026,8,8},{0x440026,9,9},{0x440026,10,10},{0x440026,11,11},
    {0x440026,12,12},{0x440026,13,13},{0x440026,14,14},{0x440026,15,15},{0x440026,16,16},{0x440026,17,17},{0x440026,18,18},{0x440026,19,19},{0x440026,20,20},{0x440026,21,21},
    {0x440026,22,22},{0x440026,23,23},{0x440026,24,24},{0x440026,25,25},{0x440027,0,0},{0x440027,1,1},{0x440027,2,2},{0x440028,0,0},{0x440028,1,1},{0x440028,2,2},
    {0x440028,3,3},{0x440028,4,4},{0x440028,5,5},{0x440028,6,6},{0x440028,7,7},{0x440028,8,8},{0x440028,9,9},{0x440028,10,10},{0x440028,11,11},{0x440028,12,12},
    {0x440028,13,13},{0x440028,14,14},{0x440029,0,10},{0x440029,10,10},{0x440029,11,11},{0x440029,12,12},{0x440029,10,0},{0x44002d,0,10},{0x44002d,10,10},{0x44002d,11,11},
    {0x44002d,12,12},{0x44002d,10,0},{0x44002f,0,0},{0x44002f,1,1},{0x440030,0,0},{0x440030,1,1},{0x440030,2,2},{0x440030,3,3},{0x440030,4,4},{0x440030,5,5},
    {0x440030,6,6},{0x440030,7,7},{0x440030,8,8},{0x440030,9,9},{0x440030,10,10},{0x440030,11,11},{0x440030,12,12},{0x440030,13,13},{0x440030,14,14},{0x440030,15,15},
    {0x440030,16,16},{0x440030,17,17},{0x440030,18,18},{0x440030,19,19},{0x440046,0,0},{0x440046,1,1},{0x440046,2,2},{0x440046,3,3},{0x440046,4,4},{0x440046,5,5},
    {0x440085,0,0},{0x440085,1,1},{0x440085,2,2},{0x440085,3,3},{0x440085,4,4},{0x440085,5,5},{0x440085,6,6},{0x440085,7,7},{0x440085,8,8},{0x440085,9,9},
    {0x440085,10,10},{0x440085,11,11},{0x440085,12,12},{0x440085,13,13},{0x440085,14,14},{0x440085,15,15},{0x440085,16,16},{0x440085,17,17},{0x440085,18,18},{0x440085,19,19},
    {0x440085,20,20},{0x440085,21,21},{0x440085,22,22},{0x440085,23,23},{0x440085,24,24},{0x440085,25,25},{0x440085,26,26},{0x440085,27,27},{0x440085,28,28},{0x440085,29,29},
};
const Frame& frame(int i,unsigned pc){if(i<0||i>=150)fault(pc);return Frames5e70f8[i];}
// 5CA008: filter titles (4CA9B0).
constexpr Frame Titles5ca008[9]{{0x4400de,2,2},{0x4400de,3,3},{0x4400de,4,4},{0x4400de,5,5},{0x4400de,6,6},
    {0x4400de,7,7},{0x4400de,8,8},{0x4400cc,1,1},{0x440047,5,5}};
// 5C9F98 (mode 1, 9 ranges) and 5C9F58 (5 ranges): course carousel.
constexpr FrontendCarouselDescriptor Carousel5c9f98[9]{{0x440086,0,10},{0x440086,10,10},{0x440086,10,40},{0x440086,40,40},
    {0x440086,40,70},{0x440086,70,70},{0x440086,70,100},{0x440086,100,100},{0x440086,100,70}};
constexpr FrontendCarouselDescriptor Carousel5c9f58[5]{{0x44002a,0,10},{0x44002a,10,10},{0x44002a,10,40},{0x44002a,40,40},{0x44002a,40,10}};
FrontendCarouselTable carousel_table(const S& s){
    return (mode(s)&0xe0)==0x20?FrontendCarouselTable{Carousel5c9f98,9,0x5c9f98}:FrontendCarouselTable{Carousel5c9f58,5,0x5c9f58};
}
// ---- globals ----------------------------------------------------------------
std::uint8_t& g85b317(S& s){return s.g.area_85b308[0xf];}
std::uint8_t& g85b318(S& s){return s.g.area_85b308[0x10];}
std::uint8_t& g85b319(S& s){return s.g.area_85b308[0x11];}
std::uint8_t& g85b324(S& s){return s.g.area_85b308[0x1c];}
void online(S& s,unsigned pc){if(s.g.online_7d68bc)fault(pc);}
// ---- record (51C050) ----------------------------------------------------------
void record_construct_51c050(Bytes r,S& s){
    for(unsigned off:{0x68u,0x4f4u,0x980u})if(!title_widget_construct_48e590(r.data()+off,0x48c,s.repeat))fault(0x48e590);
    for(unsigned off=0xe0c;off<Rec;off+=0xa0)if(!title_ui_resource_construct_465160(r.data()+off,0xa0))fault(0x465160);
    r.put8(0x60,0);r.put8(0x64,0);r.put16(0x62,0);
}
// 51F0D0 assignment: memberwise, padding bytes and the text vtables untouched.
void record_copy_51f0d0(Bytes d,Bytes s){
    auto copy=[&](unsigned off,unsigned n){std::memmove(d.data()+off,s.data()+off,n);};
    d.check(0,Rec);s.check(0,Rec);
    copy(0,0x61);copy(0x62,3);
    for(unsigned w:{0x68u,0x4f4u,0x980u}){copy(w+4,0x44a);copy(w+0x450,0x21);copy(w+0x474,0x18);}
    copy(0xe0c,Rec-0xe0c);
}
void record_hide_51c310(Bytes r,S& s){for(unsigned off:Drawn)release(s,res_at(r,off));}
void record_show_51c380(Bytes r,int index,S& s){
    const float y=float(index)*36.f-100.f;
    configure(s,res_at(r,0xe0c),0x440023,0,0xa,4,3,99.f,y);
    for(unsigned off:{0x108cu,0xf4cu,0xfecu,0x112cu,0x11ccu,0x126cu,0x130cu,0x13acu})release(s,res_at(r,off));
    r.put32(4,1);
}
// 51C220: integrate +C by +24 * +8 until +18 is reached on x and y.
void record_move_51c220(Bytes r){
    if(!r.u32(0x30))return;
    const float vx=r.f32(0x24),vy=r.f32(0x28);
    bool fx=vx==0.f,fy=vy==0.f;
    const X87 t(r.f32(8));
    float step[3];for(unsigned i=0;i<3;++i)step[i]=x87_float(t*X87(r.f32(0x24+4*i)));
    for(unsigned i=0;i<3;++i)r.putf(0xc+4*i,x87_float(X87(r.f32(0xc+4*i))+X87(step[i])));
    const float px=r.f32(0xc),py=r.f32(0x10),tx=r.f32(0x18),ty=r.f32(0x1c);
    if((0.f>vx&&tx>=px)||(vx>0.f&&px>=tx))fx=true;
    if((0.f>vy&&ty>=py)||(vy>0.f&&py>=ty))fy=true;
    if(fx&&fy){for(unsigned i=0;i<3;++i)r.put32(0xc+4*i,r.u32(0x18+4*i));r.put32(0x30,0);}
}
// 51C160: D3DXMatrixTranslation(+C) into every drawn resource (4287B0).
void record_matrices_51c160(Bytes r,S& s){
    std::array<float,16> m{};m[0]=m[5]=m[10]=m[15]=1.f;m[12]=r.f32(0xc);m[13]=r.f32(0x10);m[14]=r.f32(0x14);
    for(unsigned off:Drawn){const auto h=r.i32(off+8);if(h<0)continue;
        if(!s.ui.sprites.set_matrix(std::uint32_t(h),m))fault(0x4287b0);}
}
bool in_set(int k,std::initializer_list<int> v){for(int x:v)if(k==x)return true;return false;}
// 51DB70 (record, t).
void record_tick_51db70(Bytes r,float t,S& s){
    r.putf(8,t);
    tick(s,res_at(r,0xe0c));
    const int kind=r.i32(0x3c);
    const bool big=in_set(kind,{3,5,0,6,7});
    if(big&&r.i32(0x40)<=0x71)tick(s,res_at(r,0xf4c));
    tick(s,res_at(r,0xfec));tick(s,res_at(r,0x112c));
    if(kind!=3&&kind!=5)for(unsigned off:{0x108cu,0x11ccu,0x126cu,0x130cu,0x13acu})tick(s,res_at(r,off));
    if(r.u32(4)){
        for(unsigned off:Drawn)release(s,res_at(r,off));
        auto y=[&]{return float(r.i32(0))*36.f-100.f;};
        const unsigned f=r.u32(0x34)?0xb:0xa;
        configure(s,res_at(r,0xe0c),0x440023,f,f,4,0,99.f,y());
        auto put=[&](unsigned field,unsigned off,unsigned layer,bool commit=true){
            const auto& e=frame(r.i32(field),0x51db70);configure(s,res_at(r,off),e.token,e.first,e.last,layer,0,99.f,y(),commit);};
        put(0x40,0xf4c,8,!(big&&r.i32(0x40)>0x71));
        if(kind==2)put(0x44,0xfec,8);
        put(0x4c,0x112c,8);
        if(kind!=3&&kind!=5){
            put(0x48,0x108c,8);put(0x50,0x11cc,9);put(0x54,0x126c,9);
            if(kind!=4)put(0x58,0x130c,9);
            if(in_set(kind,{4,1,2,6,7,0}))put(0x5c,0x13ac,9);
        }
    }
    if(r.u32(0x30))record_move_51c220(r);
    record_matrices_51c160(r,s);
}
// 51DAF0: decimal digits of v (leading zeros skipped, 7 digits then units)
// into 85B308; returns the string.
std::string digits_51daf0(int v,S& s){
    auto& a=s.g.area_85b308;int n=0;bool started=false;int div=1000000;
    auto put=[&](int k,std::uint8_t c){if(k<0||k>=int(a.size()))fault(0x51daf0);a[std::size_t(k)]=c;};
    do{const int d=(v%(div*10))/div;
        if(d!=0||started){put(n,std::uint8_t(d+0x30));n=std::int8_t(n+1);started=true;}
        div/=10;
    }while(div!=1);
    put(n,std::uint8_t(v%10+0x30));n=std::int8_t(n+1);put(n,0);
    return std::string(reinterpret_cast<const char*>(a.data()),std::size_t(n));
}
}
// 51C710(kind,value): sprite frame of a ranking column.
int frontend_rankings_frame_51c710(int kind,int v,const FrontendRankingServices& s,bool& valid){
    valid=true;
    static constexpr std::int8_t B30[30]{0x18,0xa,0,7,9,0xb,8,0x11,0x1a,0xd,0x1c,0x15,1,0x10,5,0x1b,2,0x17,0x1d,3,4,0x14,6,0xf,0x12,0xe,0x13,0xc,0x16,0x19};
    static constexpr std::int8_t B16[16]{0,4,3,0xa,2,9,7,0xe,1,8,6,0xd,5,0xc,0xb,0xf};
    static constexpr int D30[30]{0xd,8,1,4,7,9,0xe,3,0xb,0,0xa,2,0xc,5,6,0xd,8,1,4,7,9,0xe,3,0xb,0,0xa,2,0xc,5,6};
    // The three tables are locals of one stack frame (after PUSH EBX): +4 B30 bytes, +22/+23
    // never written, +24 B16 bytes, +34 D30 dwords up to the return address at +AC. The
    // original indexes them without bounds (the OUTRUN mode screen asks kind 9 with v = 30:
    // a byte of D30), so an index past one table reads the next one.
    std::array<std::uint8_t,0xac> frame{};std::array<bool,0xac> known{};
    for(int i=0;i<30;++i){frame[std::size_t(4+i)]=std::uint8_t(B30[i]);known[std::size_t(4+i)]=true;}
    for(int i=0;i<16;++i){frame[std::size_t(0x24+i)]=std::uint8_t(B16[i]);known[std::size_t(0x24+i)]=true;}
    for(int i=0;i<30;++i)for(int b=0;b<4;++b){frame[std::size_t(0x34+i*4+b)]=std::uint8_t(std::uint32_t(D30[i])>>(8*b));known[std::size_t(0x34+i*4+b)]=true;}
    auto byte_at=[&](int off)->int{
        if(off<0||off>=int(frame.size())||!known[std::size_t(off)]){valid=false;return 0;}
        return int(std::int8_t(frame[std::size_t(off)]));};
    switch(kind){
    case 0:case 10:return v+0x14;
    case 1:return v+0x24;
    case 2:return v+0x26;
    case 3:if(s.g.ghost_84b0f6==1)return v+0x40;return (v!=0)*2+0x40;
    case 4:{const int off=0x34+v*4;if(v<0||off+4>int(frame.size())||!known[std::size_t(off)]){valid=false;return 0;}
            std::uint32_t d=0;for(int b=0;b<4;++b)d|=std::uint32_t(frame[std::size_t(off+b)])<<(8*b);return int(d)+0x43;}
    case 5:return v+0x5c;
    case 6:return v+0x5e;
    case 7:return v+0x72;
    case 8:return v+0x23;
    case 9:return byte_at(0x24+v)+4;
    case 11:{const int i=(s.root.u32(0x20c)&3)?v+15:v;return byte_at(4+i)+0x78;}
    default:return 0;
    }
}
namespace {
int frames(int kind,int v,S& s){bool ok{};const int r=frontend_rankings_frame_51c710(kind,v,s,ok);if(!ok){if(std::getenv("OR2_FE_DUMP"))std::fprintf(stderr,"51C710 kind=%d v=%d\n",kind,v);fault(0x51c710);}return r;}
// 51D0F0(result, w1..w6): one record from the common save tables.
std::array<std::uint8_t,0x2c> fetch_51d0f0(S& s,std::int16_t w1,std::int16_t w2,std::int16_t w3,std::int16_t w4,std::int16_t w5,std::int16_t w6){
    if(w1!=0)fault(0x51d103);                     // returns its uninitialised local
    std::array<std::uint8_t,0x2c> out{};Bytes o(out.data(),out.size());
    if(s.g.ghost_84b0f6){                          // 51D7D5: OutRun2SP tables (4F3810 / 4F3870 / 4F3910)
        if(!s.sp_tables_84df40)fault(0x51d7d5);
        const unsigned low=mode(s)&3,count=unsigned(std::int32_t(w5)),i=unsigned(std::int32_t(w6));
        const auto dword=[&](unsigned off){std::uint32_t v;std::memcpy(&v,s.g.area_85b308.data()+off,4);return v;};
        std::uint32_t at;
        switch(w2){
        case 3:at=sp_record_4f3910(low==0?1:0,count,i);break;
        case 2:at=g85b318(s)==0?sp_record_4f3810(low==0?1:0,count,i):sp_record_4f3810(low==1?2:3,count,i);break;
        case 0:{const unsigned preset=g85b318(s)==0?(low==1?0:1):(low==1?2:3);
            at=sp_record_4f3870(preset,dword(0x14),dword(0x18),count,i);break;}
        default:fault(0x51da50);                   // uninitialised locals
        }
        if(at<SpTablesBase||at+16>SpTablesBase+s.sp_tables_size)fault(0x51da33);
        std::uint32_t r[4];std::memcpy(r,s.sp_tables_84df40+(at-SpTablesBase),16);
        o.put32(0,r[0]&0xffffffu);o.put32(0xc,r[1]);o.put32(8,r[0]>>28);
        o.put8(0x19,std::uint8_t((r[2]>>18)&1));o.put8(0x18,std::uint8_t((r[2]>>17)&1));
        bool end=false;for(unsigned k=0;k<4;++k){const auto ch=end?std::uint8_t(0):std::uint8_t(r[3]>>(8*k));if(!ch)end=true;o.put8(0x1a+k,ch);}   // strncpy 4
        o.put8(0x1e,0);o.put32(0x14,(r[2]>>12)&0x1f);o.put32(4,(r[0]>>24)&0xf);
        if(w2==3){o.put32(0x10,r[2]&0xfff);o.put32(0,r[2]&0xfff);}else o.put32(0x10,0);
        return out;
    }
    Bytes c(const_cast<std::uint8_t*>(s.common.data()),s.common.size());
    const std::uint8_t m=std::uint8_t(mode(s)&3),b324=g85b324(s);
    auto flags=[&](unsigned at){const std::uint8_t b=c.u8(at);o.put8(0x18,b&1);o.put8(0x19,(b>>1)&1);o.put32(8,b>>2);};
    auto name=[&](unsigned at){c.check(at,16);bool end=false;       // strncpy(+1A,src,16)
        for(unsigned i=0;i<16;++i){const auto ch=end?std::uint8_t(0):c.u8(at+i);if(!ch)end=true;o.put8(0x1a+i,ch);}};
    auto idx=[&](unsigned mul){return std::int8_t(std::uint8_t(std::uint8_t(std::uint8_t(m+std::uint8_t(b324<<1))*mul)+std::uint8_t(w5)));};
    auto at=[&](long e,unsigned size){if(e<0)fault(0x51d0f0);return unsigned(e)*size;};
    switch(w2){
    case 0:
        if(w4!=0){const unsigned off=at(long(idx(15))*10+w6,0x1c);
            o.put32(0,c.u32(0x4d98+off));o.put32(0xc,c.u32(0x4d98+off));flags(0x4d9c+off);name(0x4d84+off);
            o.put32(0x14,c.u8(0x4d94+off));o.put32(4,0);
        }else if(g85b318(s)==0){const unsigned off=at(long(idx(5))*10+w6,0x30);
            o.put32(0,c.u32(0x2818+off));o.put32(0xc,c.u32(0x2818+off));flags(0x2830+off);name(0x2804+off);
            o.put32(0x14,c.u8(0x2814+off));o.put32(4,c.u8(0x2815+off));
        }else{const unsigned off=at((long(w3)+std::int8_t(b324)*2)*10+w6,0x58);
            o.put32(0,c.u32(0x9168+off));o.put32(0xc,c.u32(0x9168+off));flags(0x91a8+off);name(0x9154+off);
            o.put32(0x14,c.u8(0x9164+off));o.put32(4,0);
        }break;
    case 1:
        if(w4==0){const unsigned off=at(long(idx(5))*10+w6,0x30);
            o.put32(0,c.u32(0x9f28+off));o.put32(0xc,c.u32(0x9f28+off));flags(0x9f40+off);name(0x9f14+off);
            o.put32(0x14,c.u8(0x9f24+off));o.put32(4,c.u8(0x9f25+off));
        }else{const unsigned off=at(long(idx(15))*10+w6,0x1c);
            o.put32(0,c.u32(0xc4a8+off));o.put32(0xc,c.u32(0xc4a8+off));flags(0xc4ac+off);name(0xc494+off);
            o.put32(0x14,c.u8(0xc4a4+off));o.put32(4,0);
        }break;
    case 2:
        if(g85b318(s)==0){const unsigned off=at((long(w3)*5+w5)*10+w6,0x20);
            o.put32(0,c.u32(0x18+off));o.put32(0xc,c.u32(0x1c+off));flags(0x20+off);name(4+off);
            o.put32(0x14,c.u8(0x14+off));o.put32(4,c.u8(0x15+off));
        }else{const unsigned off=at(long(m)*10+w6,0x20);
            o.put32(0,c.u32(0xc98+off));o.put32(0xc,c.u32(0xc9c+off));flags(0xca0+off);name(0xc84+off);
            o.put32(0x14,c.u8(0xc94+off));o.put32(4,0);
        }break;
    case 3:case 4:{const unsigned base=w2==3?0xf04:0x1b84;const unsigned off=at((long(w3)*5+w5)*10+w6,0x20);
        o.put32(0,c.u32(base+0x14+off));o.put32(0xc,c.u32(base+0x18+off));flags(base+0x1c+off);name(base+off);
        o.put32(0x14,c.u8(base+0x10+off));o.put32(4,c.u8(base+0x11+off));break;}
    default:fault(0x51d1c4);                      // uninitialised local
    }
    o.put32(0x10,0);
    return out;
}
}
// ---- list (51E080) ------------------------------------------------------------
bool frontend_rankings_list_reset_51c440(Bytes L,int m,bool flag,FrontendRankingServices& s){
    const auto& f=font9(s);
    for(int i=0;i<15;++i){recA(L,i).put32(0x34,0);recB(L,i).put32(0x34,0);
        if(!frontend_scroll_text_init_4edaf0(L.sub(0x26670+unsigned(i)*0x8c,0x8c),f,"",0,0,0x90,~0u))fault(0x4edaf0);}
    L.put32(0,0);L.put32(0x26118,0x5e70f8);L.puti(0x26650,m);L.put8(0x26654,0);L.put8(0x26655,0);L.put32(0x26658,0);
    L.put8(0x2611c,0);L.put8(0x2666d,0);L.putf(0x26660,0);L.putf(0x26664,0);L.putf(0x26668,0);L.put8(0x2666c,0);L.puti(0x2664c,m);
    // "Total Players: %d" (0x343): the pushed 465EB0 id is the only word left
    // above the 48F280 arguments, so the PC prints 835.
    auto T=L.sub(0x261c0,0x48c);frontend_text_init_48e640(T);text_format(T,localized(s,0x343),9,0xff3f474a,{0x343});T.put32(0x454,0xb);
    auto R=L.sub(0x26120,0xa0);
    switch(m){
    case 1:L.put32(0x2665c,5);configure(s,R,0x440029,0,0xa,8,3,0,0);break;
    case 3:case 5:break;
    case 6:L.put8(0x2666c,1);break;
    case 4:L.put8(0x2666c,1);[[fallthrough]];
    default:L.put32(0x2665c,6);configure(s,R,0x44002d,0,0xa,8,3,0,0);break;
    }
    if(L.u32(0x26650)==0)L.put8(0x26654,1);
    if(flag)L.put8(0x26656,1);else L.put8(0x26654,1);
    return true;
}
bool frontend_rankings_list_construct_51e080(Bytes L,FrontendRankingServices& s){
    L.check(0,0x26ea4);
    for(int i=0;i<15;++i)record_construct_51c050(recA(L,i),s);
    for(int i=0;i<15;++i)record_construct_51c050(recB(L,i),s);
    if(!title_ui_resource_construct_465160(L.data()+0x26120,0xa0))fault(0x465160);
    if(!title_widget_construct_48e590(L.data()+0x261c0,0x48c,s.repeat))fault(0x48e590);
    return frontend_rankings_list_reset_51c440(L,3,true,s);   // 570AC0 scroll texts: no constructor
}
namespace {
void list_release_51c600(Bytes L,S& s){
    release(s,L.sub(0x26120,0xa0));
    constexpr unsigned order[9]{0xe0c,0x108c,0xf4c,0xfec,0x112c,0x11cc,0x126c,0x130c,0x13ac};
    for(int i=0;i<L.i32(0);++i){
        auto a=recA(L,i);for(unsigned off:order)release(s,res_at(a,off));a.put32(4,0);
        auto b=recB(L,i);for(unsigned off:order)release(s,res_at(b,off));b.put32(4,0);
    }
}
// 51CB70 (down) / 51CA90 (up): shift every displayed record by rows*36.
void list_scroll(Bytes L,int first,int rows,bool up,S& s){
    const int visible=5+(L.u8(0x2666c)!=0);const float d=float(rows)*36.f;
    for(int i=0;i<L.i32(0);++i){auto r=recB(L,i);
        const float y=up?d+r.f32(0x10):r.f32(0x10)-d;
        r.putf(0xc,99.f);r.putf(0x10,y);r.putf(0x14,0);r.putf(0x18,99.f);r.putf(0x1c,y);
        for(unsigned off=0x20;off<0x30;off+=4)r.putf(off,0);r.put32(0x30,1);
        if(i>=first&&i<first+visible)r.put32(4,1);else{r.put32(4,0);record_hide_51c310(r,s);}
    }
}
// 51E140: append one record (12 stack arguments) to the staging array.
void list_add_51e140(Bytes L,const std::array<std::int32_t,12>& a,S& s){
    const int kind=a[0];
    if(a[9]&0xff)fault(0x51e1a0);                 // network ranking rows (65F0C0)
    auto A=recA(L,L.i32(0));
    A.puti(0x3c,kind);A.put32(0x34,0);A.put32(0x38,g85b318(s));
    g85b319(s)=std::uint8_t(a[9]);
    const unsigned m=(mode(s)>>5)&7;
    if(m!=0){
        static constexpr std::int16_t kinds[8]{0,2,3,4,0,1,0,0};
        const auto r=fetch_51d0f0(s,0,kinds[m],std::int16_t(mode(s)&3),g85b317(s),std::int16_t(a[7]),std::int16_t(a[8]));
        std::memcpy(L.data()+4,r.data(),r.size());
    }
    auto T=A.sub(0x4f4,0x48c);frontend_text_init_48e640(T);
    text_set(T,std::string_view(reinterpret_cast<const char*>(L.data()+0x1e),0x10),9,~0u);T.put32(0x454,0xb);
    auto W=A.sub(0x980,0x48c);frontend_text_init_48e640(W);
    {   const int t=L.i32(0x10),q=t/1000,ms=t%1000;const unsigned minutes=unsigned(q)/60u;const int sec=int(unsigned(q)-minutes*60u);
        char buf[40];const int n=std::snprintf(buf,sizeof buf,"%2d'%.2d'%.3d",int(minutes),sec,ms);
        if(n<0||n>=int(s.g.area_85b308.size()))fault(0x5802dd);
        std::memcpy(s.g.area_85b308.data(),buf,std::size_t(n)+1);
        text_set(W,std::string_view(buf,std::size_t(n)),9,~0u);}
    W.put32(0x454,0xb);
    auto D=A.sub(0x68,0x48c);frontend_text_init_48e640(D);
    text_set(D,digits_51daf0(L.i32(4),s),9,~0u);D.put32(0x454,0xb);
    A.puti(0,L.i32(0));A.put32(4,0);for(unsigned off=8;off<0x30;off+=4)A.putf(off,0);A.put32(0x30,0);
    L.put8(0x2666d,0);
    switch(kind){
    case 1:case 4:A.puti(0x40,frames(7,a[1],s));break;
    case 2:A.puti(0x40,frames(0,a[1],s));A.puti(0x44,frames(0xb,std::int16_t(a[7]),s));break;
    case 0:case 3:case 5:case 6:case 7:A.puti(0x40,frames(6,a[1],s));L.put8(0x2666d,1);break;
    default:break;
    }
    A.puti(0x48,frames(1,L.u8(0x1c),s));
    A.puti(0x4c,frames(2,L.i32(0x18),s));
    A.puti(0x50,frames(3,L.u8(0x1d),s));
    A.puti(0x54,frames(4,L.i32(0xc),s));
    A.puti(0x58,frames(5,a[6],s));
    if(in_set(kind,{6,1,4}))A.puti(0x5c,A.u32(0x38)?frames(8,0,s):frames(9,L.i32(8),s));
    else if(kind==7)A.puti(0x5c,frames(0xa,std::int16_t(a[7]),s));
    else A.puti(0x5c,frames(5,0,s));
    const float y=float(L.i32(0))*36.f-100.f;
    A.putf(0xc,99.f);A.putf(0x10,y);A.putf(0x14,0);A.putf(0x18,99.f);A.putf(0x1c,y);
    for(unsigned off=0x20;off<0x30;off+=4)A.putf(off,0);A.put32(0x30,1);
    L.puti(0,L.i32(0)+1);
}
// 51CC40(value,row): score digits as 42D280 images (x from 0xF8).
void number_51cc40(int v,int row,FrontendRankings& c,S& s){
    if(v<0)fault(0x51cc40);
    if(v>99999)v=99999;
    constexpr int Wbig[10]{9,5,9,9,9,9,9,9,9,9},Wsmall[10]{14,8,14,14,14,14,14,14,14,14};
    constexpr unsigned Tsmall[10]{0x4405e6,0x4405eb,0x4405e5,0x4405ea,0x4405e4,0x4405e9,0x4405e3,0x4405e8,0x4405e7,0x4405e2};
    constexpr unsigned Tbig[10]{0x440602,0x44060b,0x440609,0x440608,0x440607,0x44060a,0x440606,0x440605,0x440604,0x440603};
    const int y=cvtt(float(row)*36.f+132.f);
    const int d4=v/10000,d3=v/1000%10,d2=v/100%10,d1=v/10%10,d0=v%10;
    auto img=[&](unsigned token,int x,int py){c.images.push_back({0x42d280,0,token,~0u,x,py,0,0,0,10.f});
        log(s,0x42d280,nullptr,{token,unsigned(x),unsigned(py),0,0x41200000,~0u});};
    if(v<100){img(Tsmall[d1],0xf8,y);img(Tsmall[d0],0xf8+Wsmall[d1],y);}
    else if(v<1000){img(Tsmall[d2],0xf8,y);img(Tsmall[d1],0xf8+Wsmall[d2],y);img(Tsmall[d0],0xf8+Wsmall[d2]+Wsmall[d1],y);}
    else if(v<10000){const int yy=y+4;int x=0xf8;for(int d:{d3,d2,d1,d0}){img(Tbig[d],x,yy);x+=Wbig[d];}}
    else{const int yy=y+4;int x=0xf8;for(int d:{d4,d3,d2,d1,d0}){img(Tbig[d],x,yy);x+=Wbig[d];}}
}
}
bool frontend_rankings_list_fill_51f280(Bytes L,int filter,int first,bool clamp,FrontendRankingServices& s){
    const int visible=5+(L.u8(0x2666c)!=0);
    for(int i=0;i<L.i32(0);++i){auto b=recB(L,i);for(unsigned off:Drawn)release(s,res_at(b,off));}
    const std::int8_t f=std::int8_t(filter);
    if((f>=0&&f<=3)||(f>=7&&f<=10))for(int i=0;i<L.i32(0);++i)record_copy_51f0d0(recB(L,i),recA(L,i));
    const std::int8_t top=std::int8_t(first);
    const float shift=float(top)*36.f;
    for(int i=0;i<L.i32(0);++i){auto r=recB(L,i);r.put32(4,0);
        const float y=float(i)*36.f-100.f-shift;
        r.putf(0xc,99.f);r.putf(0x10,y);r.putf(0x14,0);r.putf(0x18,99.f);r.putf(0x1c,y);
        for(unsigned off=0x20;off<0x30;off+=4)r.putf(off,0);r.put32(0x30,1);}
    int n=visible;if(clamp&&L.i32(0)<=visible)n=L.i32(0);
    for(int j=top;j<top+n;++j){auto r=recB(L,j);r.put32(4,1);record_show_51c380(r,j,s);}
    return true;
}
bool frontend_rankings_list_tick_51ec80(Bytes L,FrontendRankingServices& s){
    for(int i=0;i<L.i32(0);++i)record_tick_51db70(recB(L,i),L.f32(0x26668),s);
    auto R=L.sub(0x26120,0xa0);
    if(R.i32(8)==-1)return true;
    release(s,R);
    unsigned f;switch(L.i8(0x2611c)){case 0:f=0xa;break;case 1:f=0xb;break;case 2:f=0xc;break;default:return true;}
    configure(s,R,L.u8(0x2666c)?0x44002d:0x440029,f,f,8,0,0,0);
    return true;
}
bool frontend_rankings_list_display_51edc0(Bytes L,FrontendRankings& c,FrontendRankingServices& s){
    L.putf(0x26660,L.f32(0x26664));
    float k62812c;{const std::uint32_t u=0x3c881469;std::memcpy(&k62812c,&u,4);}
    const X87 now=X87(s.timer)*X87(k62812c);                // 4AF500 * [62812C]
    L.putf(0x26664,x87_float(now));L.putf(0x26668,x87_float(now-X87(L.f32(0x26660))));
    if(g85b319(s)&&L.u8(0x2666d)){auto T=L.sub(0x261c0,0x48c);text_position(T,280.f,98.f);text_display(s,T,c);}
    int row=0;
    for(int i=0;i<L.i32(0);++i){auto r=recB(L,i);
        if(r.i32(4)!=1)continue;
        const float y=float(row)*36.f+124.f;const int iy=cvtt(y);
        text_position(r.sub(0x4f4,0x48c),316.f,float(iy));
        text_position(r.sub(0x980,0x48c),484.f,y);text_position(r.sub(0x68,0x48c),484.f,y);
        auto t=L.sub(0x26670+unsigned(i)*0x8c,0x8c);const auto& f=font9(s);
        scroll_set_4edd50(t,f,frontend_text_value_48eec0(r.sub(0x4f4,0x48c)));
        t.puti(4,0x13c);t.puti(8,iy);
        log(s,0x4edb90,t.data());if(!frontend_scroll_text_draw_4edb90(t,f,c.glyphs))fault(0x4edb90);
        const int kind=r.i32(0x3c);
        if(kind!=3){
            if((mode(s)&0xe0)==0x80&&s.g.ghost_84b0f6!=2)text_display(s,r.sub(0x980,0x48c),c);
            else text_display(s,r.sub(0x68,0x48c),c);
        }
        if(in_set(kind,{3,5,0,6,7})&&r.i32(0x40)>0x71)number_51cc40(r.i32(0x40)-0x5d,row,c,s);
        ++row;
    }
    return true;
}
bool frontend_rankings_list_add_51e140(Bytes L,const std::array<std::int32_t,12>& a,FrontendRankingServices& s){
    list_add_51e140(L,a,s);return true;
}
namespace {
// 51F460(first,online,wide,a4,step): filter cycling; offline it always resets
// the filter to 0 and refills the list.
bool list_filter_51f460(Bytes L,int a1,int a2,int a3,int a4,int a5,S& s){
    const bool on=s.g.online_7d68bc!=0;
    if(std::uint8_t(a5)==1){
        const std::int8_t f=L.i8(0x2611d);
        if(f<10){if(on&&f==0){fault(0x51f480);}L.put8(0x2611d,std::uint8_t(f+1));return false;}
        L.put8(0x2611d,on?0:7);return false;
    }
    const std::int8_t limit=on?(std::uint8_t(a3)==0?3:6):0;
    const std::int8_t f=L.i8(0x2611d);
    if(f<limit){
        if(std::uint8_t(a2)&&std::uint8_t(a4)&&f==0)L.put8(0x2611d,4);
        else if(on)L.put8(0x2611d,f==0?3:std::uint8_t(f+1));
        else{frontend_rankings_list_fill_51f280(L,L.u8(0x2611d),a1,false,s);return false;}
    }else L.put8(0x2611d,0);
    if(on)return true;
    frontend_rankings_list_fill_51f280(L,L.u8(0x2611d),a1,false,s);return false;
}
// ---- screen -------------------------------------------------------------------
Bytes obj(FrontendRankings& c){return Bytes(c.object.data(),c.object.size());}
Bytes list_of(FrontendRankings& c){return obj(c).sub(List,0x26ea4);}
Bytes rec_row(FrontendRankings& c,int index){return flag_row(list_of(c),index);}   // [esi+0x13414] = B+0x34
std::int32_t& cursor(S& s){return s.g.cursor_68fa84;}
FrontendCarouselServices carousel_api(S& s){return {s.ui,s.timer};}
void carousel_fail(const FrontendCarouselServices& api){fault(api.missing?api.missing:0x51be30);}
// 4CA740: fill the staging list from the common save.
void load_4ca740(FrontendRankings& c,S& s){
    auto b=obj(c);auto L=list_of(c);
    int count=5,kind;const bool first_mode=(mode(s)&0xe0)==0x20;const int ec=b.i32(0xec);
    if(first_mode&&ec!=0&&ec!=1){kind=6;g85b318(s)=1;count=10;}else{kind=1;g85b318(s)=0;}
    g85b317(s)=0;g85b324(s)=0;
    if(first_mode&&ec>1)for(int i=0;i<count;++i)list_add_51e140(L,{kind,i,0,0,0,i,0,0,i,0,0,-1},s);
    else for(int i=0;i<count;++i)list_add_51e140(L,{kind,i,0,0,0,i,0,i,0,0,0,-1},s);
    if(kind==6)return;
    const unsigned m=mode(s)&0xe0;
    if(m==0x20||m==0x80){g85b318(s)=1;b.put32(0x27f94,6);list_add_51e140(L,{1,5,0,0,0,0,0,5,0,0,0,-1},s);g85b318(s)=0;}
    else{g85b318(s)=0;b.put32(0x27f94,5);}
}
// 4CA9B0: filter title.
void title_4ca9b0(FrontendRankings& c,S& s,int k){
    auto r=obj(c).sub(0x150,0xa0);release(s,r);
    const auto& e=Titles5ca008[k];configure(s,r,e.token,e.first,e.last,0xf,0,25.f,-338.f);
}
// 4CB270
void filter_label_4cb270(FrontendRankings& c,S& s){
    online(s,0x4cb285);
    switch(obj(c).i8(0x26459)){
    case 0:title_4ca9b0(c,s,6);break;
    case 1:title_4ca9b0(c,s,2);break;
    case 2:title_4ca9b0(c,s,1);break;
    default:text_printf(obj(c).sub(0x27afc,0x48c),"UNKNOWN FILTER");break;
    }
}
// Offsets that differ between the key-36 and key-37 boards (4CAD70/4CE7A0,
// 4CAB60/4CE5A0 are the same code on different fields).
struct Board {
    Bytes obj,list;
    unsigned focus,top,labels;          // byte, int, byte
    std::int32_t& cursor;               // 68FA84 / 690E7C
    Bytes row(int i)const{return flag_row(list,i);}
};
// 4CAD70 / 4CE7A0: Back-only labels while a row is highlighted.
void board_labels(Board b,S& s){
    if(s.g.online_7d68bc)return;
    bool any=false;
    for(int i=0;i<10;++i)if(b.row(i).u32(0x34)){any=true;break;}
    if(any){if(!b.obj.u8(b.labels)){labels(s,{~0u,~0u,~0u,~0u,~0u,~0u,8,0x296});b.obj.put8(b.labels,1);}}
    else if(b.obj.u8(b.labels)){labels(s,{4,0x295,~0u,~0u,~0u,~0u,8,0x296});b.obj.put8(b.labels,0);}
}
// 4CAA10: rebuild the list for the current carousel index.
void reload_4caa10(FrontendRankings& c,S& s){
    auto b=obj(c);auto L=list_of(c);
    b.put8(0x271e0,0);text_printf(b.sub(0x271e4,0x48c),"");b.put32(0x34,0);
    list_release_51c600(L,s);
    if((mode(s)&0xe0)==0x20){
        if(b.i32(0xec)<2){b.put32(0x27f98,6);cursor(s)=5;frontend_rankings_list_reset_51c440(L,6,false,s);}
        else{b.put32(0x27f98,5);cursor(s)=4;frontend_rankings_list_reset_51c440(L,1,false,s);}
    }else frontend_rankings_list_reset_51c440(L,b.i32(0x27f98)==5?5:6,false,s);
    if(s.g.online_7d68bc){const auto f=b.u8(0x26459);if(f==0||f==4||f==6||f==5)fault(0x4caacd);}
    load_4ca740(c,s);
    frontend_rankings_list_fill_51f280(L,b.u8(0x26459),b.u8(0x34),false,s);
}
// 4CAB60 / 4CE5A0 (a1,a2,a3,a4): restore the highlighted row after a rebuild.
void board_restore(Board b,S& s,int a1,int a2,int a3,int a4){
    auto o=b.obj;auto L=b.list;bool done=false;
    auto set_top=[&](int v){o.puti(b.top,v);};
    auto reset=[&]{o.put8(b.focus,0);L.put8(0x2611c,0);b.cursor=5;};
    auto mark=[&](int edi,int ebp){if(!done)b.row(edi+ebp).put32(0x34,1);b.cursor=edi;set_top(ebp);};
    const bool on=s.g.online_7d68bc!=0;const std::int8_t f=L.i8(0x2611d);
    if(std::uint8_t(a1)==0){
        if(on){reset();return;}
        if(f!=3&&f!=0)return;
        const std::uint8_t row=o.u8(b.focus);if(!row)return;
        if(a4==1){
            if(row==1){L.put8(0x2611c,1);set_top(a2);b.cursor=5;return;}
            if(row==7){o.put8(b.focus,7);L.put8(0x2611c,2);set_top(a2);b.cursor=0;return;}
        }
        b.row(a2+a3).put32(0x34,1);set_top(a2);b.cursor=a3;return;
    }
    if(on&&f!=3){reset();return;}
    if(f!=3){if(f!=0||on)return;}
    const std::uint8_t row=o.u8(b.focus);if(!row)return;
    int edi=a3,ebp=a2;
    if(a4==6){
        const int m=L.i32(0x2664c);
        if(m==5&&a3==5){edi=4;o.put8(b.focus,1);}
        if(m==1){o.put8(b.focus,0);L.put8(0x2611c,0);return;}
        mark(edi,ebp);return;
    }
    if(a4==1){
        const int m=L.i32(0x2664c);
        if(m==6){o.put8(b.focus,0);L.put8(0x2611c,0);return;}
        if(m==1&&row==1){edi=5;L.put8(0x2611c,1);done=true;}
        else if(m==1&&row==7){edi=0;o.put8(b.focus,7);L.put8(0x2611c,2);done=true;}
        if(m==5){const std::uint8_t now=o.u8(b.focus);ebp=0;
            if(now==1)edi=4;else if(now==7){edi=0;o.put8(b.focus,5);}}
        if(m==1)list_scroll(L,ebp,ebp,false,s);
        mark(edi,ebp);return;
    }
    mark(edi,ebp);
}
Board board36(FrontendRankings& c,S& s){return Board{obj(c),list_of(c),0x331,0x34,0x330,s.g.cursor_68fa84};}
void labels_4cad70(FrontendRankings& c,S& s){board_labels(board36(c,s),s);}
void restore_4cab60(FrontendRankings& c,S& s,int a1,int a2,int a3,int a4){board_restore(board36(c,s),s,a1,a2,a3,a4);}
}
bool frontend_rankings_construct_4cb300(FrontendRankings& c,FrontendRankingServices& s){
    c.fault=0;c.constructed=false;c.glyphs.clear();c.images.clear();
    try{
        auto b=obj(c);auto* p=c.object.data();
        if(!title_base_construct_48f480(p,c.object.size(),s.repeat))fault(0x48f480);
        b.put32(0,0x5ca074);
        for(unsigned off:{0x3cu,0x150u,0x1f0u,0x290u})if(!title_ui_resource_construct_465160(p+off,0xa0))fault(0x465160);
        frontend_rankings_list_construct_51e080(list_of(c),s);
        for(unsigned off:{0x271e4u,0x27670u,0x27afcu})if(!title_widget_construct_48e590(p+off,0x48c,s.repeat))fault(0x48e590);
        b.put32(8,0x24);b.put8(0x334,0);b.put8(0x331,0);
        const unsigned m=mode(s)&0xe0;
        if(m==0xa0||m==0x40||m==0x60){b.put32(0x27f98,5);cursor(s)=4;}else{b.put32(0x27f98,6);cursor(s)=5;}
        b.put32(0x27f94,5);g85b317(s)=0;
        c.constructed=true;return true;
    }catch(const Fault& f){c.fault=s.missing=f.pc;}catch(const std::out_of_range&){c.fault=s.missing=0x4cb300;}
    return false;
}
bool frontend_rankings_init_4caf80(FrontendRankings& c,FrontendRankingServices& s){
    try{
        if(!c.constructed)fault(0x4caf80);
        auto b=obj(c);auto L=list_of(c);
        b.put8(0x330,0);b.put32(4,0x53);b.put8(0x334,0);b.put8(0x271e0,0);b.put8(0x333,0);g85b324(s)=0;b.put32(0x27f8c,0);
        b.putf(0x27f88,s.timer);
        std::array<std::uint8_t,0x438> fmt{};Bytes f(fmt.data(),fmt.size());frontend_text_format_construct_48d570(f);
        f.put32(0x400,9);f.put32(0x404,0xd);f.put8(0x41c,0);f.put32(0x420,0xff3f474a);
        for(auto [off,y]:{std::pair<unsigned,float>{0x271e4,342.f},{0x27afc,82.f}}){auto w=b.sub(off,0x48c);
            frontend_text_init_48e640(w);text_apply_format(w,f);text_printf(w,"");text_position(w,60.f,y);}
        auto api=carousel_api(s);auto w=b.sub(0x38,0x118);const auto t=carousel_table(s);
        auto& root=s.root;const auto bits=[&]{return root.u32(0x20c);};
        if((bits()&0xe0)==0x20){
            if(!frontend_carousel_init_51b7b0(w,t,0,8,api))carousel_fail(api);
            frontend_carousel_select_51bdb0(w,int(bits()&3));
            if((bits()&3)!=0&&(bits()&3)!=2)root.put32(0x20c,(bits()&~2u)|1u);
            else root.put32(0x20c,bits()&~3u);
        }else{
            if(!frontend_carousel_init_51b7b0(w,t,0,4,api))carousel_fail(api);
            if((bits()&3)<2)frontend_carousel_select_51bdb0(w,int(bits()&3));
        }
        int m=6;if(b.i32(0x27f98)==5)m=b.i32(0xec)>1?1:5;
        frontend_rankings_list_reset_51c440(L,m,false,s);
        b.put8(0x26459,0);b.put32(0x34,0);
        if(s.g.online_7d68bc&&!s.g.online_7d68d2)fault(0x4cb1b8);
        load_4ca740(c,s);frontend_rankings_list_fill_51f280(L,0,0,false,s);
        labels(s,{4,0x295,~0u,~0u,~0u,~0u,8,0x296});
        frontend_rankings_list_tick_51ec80(L,s);
        return true;
    }catch(const Fault& f){c.fault=s.missing=f.pc;}catch(const std::out_of_range&){c.fault=s.missing=0x4caf80;}
    return false;
}
bool frontend_rankings_control_4cb480(FrontendRankings& c,FrontendRankingServices& s,unsigned& result){
    result=0;
    try{
        if(!c.constructed)fault(0x4cb480);
        auto b=obj(c);auto L=list_of(c);auto& root=s.root;
        auto bits=[&]{return root.u32(0x20c);};auto set_bits=[&](unsigned v){root.put32(0x20c,v);};
        online(s,0x4cb49d);
        if(b.u8(0x333))fault(0x4cb55f);           // online labels restore
        labels_4cad70(c,s);
        auto cursor_res=b.sub(0x290,0xa0);
        if(!b.u8(0x331)){if(cursor_res.i32(8)==-1)configure(s,cursor_res,0x4400cc,1,1,4,0,0,0);}
        else if(cursor_res.i32(8)!=-1)release(s,cursor_res);
        tick(s,cursor_res);
        const int ec=b.i32(0xec);auto lock=b.sub(0x1f0,0xa0);
        if(ec>1&&!unlocked_4474e0(s,0xcb+(ec==2))){release(s,lock);configure(s,lock,0x440047,5,5,4,0,-363.f,-206.f);}
        else release(s,lock);
        driving::object_store_depth_pair_442f20(root,0,0);
        if(b.u8(0x334)){result=b.u32(0x338);return true;}
        auto api=carousel_api(s);auto w=b.sub(0x38,0x118);
        if(!frontend_carousel_tick_51be30(w,carousel_table(s),api))carousel_fail(api);
        if(b.u8(0x271e0))fault(0x4cb708);         // network ranking wait
        filter_label_4cb270(c,s);
        unsigned action{};
        if(!frontend_input_action_48f5f0(c.object.data(),c.object.size(),s.input,0,s.repeat,&s,
            [](void* p,unsigned key,int arg){auto& x=*static_cast<S*>(p);return x.ui.input_feedback(x.root,key,arg);},action))
            fault(s.ui.missing_pc?s.ui.missing_pc:0x48f5f0);
        auto row=[&](int i){return rec_row(c,i);};
        auto leave=[&](unsigned key){b.put32(0x338,1);b.put32(4,key);};
        switch(action){
        case 0:{
            const std::uint8_t focus=b.u8(0x331);
            if(!focus){
                const unsigned m=(bits()>>5)&7;const int e=b.i32(0xec);
                if(m>=1&&m<=3){
                    if(e>1&&!unlocked_4474e0(s,0xcb+(e==2)))sound(s,3);
                    else{set_bits(bits()^((bits()^unsigned(e))&3));b.put32(4,0xa);b.put32(0x338,1);sound(s,0x40);b.put8(0x334,1);return true;}
                }else if(m==4||m==5){leave(0x25);set_bits(bits()|0x800);sound(s,0x40);b.put8(0x334,1);}
                else if(m==6){leave(0x2b);sound(s,0x40);b.put8(0x334,1);}
                if(b.i32(0xec)==0)set_bits(bits()&~3u);else set_bits((bits()&~2u)|1u);
                break;
            }
            if((bits()&0xe0)!=0x20||b.i32(0xec)<=1)break;
            if(focus==1){
                if(b.i32(0x34)<5){list_scroll(L,b.i32(0x34)+5,5,false,s);b.puti(0x34,b.i32(0x34)+5);}
                b.put8(0x26458,1);sound(s,0x40);
            }else if(focus==7){
                if(b.i32(0x34)>0){list_scroll(L,b.i32(0x34)-5,5,true,s);b.puti(0x34,b.i32(0x34)-5);}
                b.put8(0x26458,2);sound(s,0x40);
            }
            break;
        }
        case 1:
            set_bits(bits()&~3u);set_bits(bits()&~0x800u);
            row(b.i32(0x34)+cursor(s)).put32(0x34,0);
            b.put32(0x338,2);b.put8(0x334,1);break;
        case 2:{
            if(b.u8(0x271e0)||(bits()&0xe0)!=0x20||b.i32(0xec)<=1)break;
            const std::int8_t focus=b.i8(0x331);
            if(focus==0){b.put8(0x331,1);b.put8(0x26458,1);sound(s,1);}
            else if(focus==1){b.put8(0x331,7);b.put8(0x26458,2);sound(s,1);}
            else if(focus>=2&&focus<=5){int k=cursor(s);b.put8(0x26458,0);
                if(k>0){row(b.i32(0x34)+k).put32(0x34,0);--k;row(b.i32(0x34)+k).put32(0x34,1);cursor(s)=k;}
                row(b.i32(0x34)+k).put32(0x34,1);b.put8(0x331,std::uint8_t(focus+1));sound(s,1);}
            else if(focus==6){row(b.i32(0x34)+cursor(s)).put32(0x34,0);b.put8(0x331,7);b.put8(0x26458,2);sound(s,1);}
            break;
        }
        case 3:case 5:{
            const bool right=action==5;const int e=b.i32(0xec);
            if(right){const int limit=(bits()&0xe0)==0x20?3:1;if(e==limit)break;}
            else if(e==0)break;
            const int saved=L.i32(0x2664c),top=b.i32(0x34),cur=cursor(s);
            b.put32(0x34,0);list_release_51c600(L,s);
            frontend_rankings_list_reset_51c440(L,b.i32(0x27f98)==5?5:6,false,s);
            if(!frontend_carousel_move(w,carousel_table(s),right,api))carousel_fail(api);
            const int now=b.i32(0xec);
            if(now==0||now==2)set_bits(bits()&~3u);else set_bits((bits()&~2u)|1u);
            reload_4caa10(c,s);restore_4cab60(c,s,1,top,cur,saved);
            break;
        }
        case 4:{
            if((bits()&0xe0)!=0x20||b.i32(0xec)<=1)break;
            const std::int8_t focus=b.i8(0x331);
            if(focus==1){b.put8(0x331,0);b.put8(0x26458,0);sound(s,1);}
            else if(focus==2){b.put8(0x331,1);b.put8(0x26458,1);row(b.i32(0x34)+cursor(s)).put32(0x34,0);sound(s,1);}
            else if(focus>=3&&focus<=6){int k=cursor(s);b.put8(0x26458,0);
                if(k<4){row(b.i32(0x34)+k).put32(0x34,0);++k;row(b.i32(0x34)+k).put32(0x34,1);cursor(s)=k;}
                row(b.i32(0x34)+k).put32(0x34,1);b.put8(0x331,std::uint8_t(focus-1));sound(s,1);}
            else if(focus==7){b.put8(0x331,1);b.put8(0x26458,1);sound(s,1);}
            break;
        }
        case 6:case 8:{
            const int e=b.i32(0xec),saved=L.i32(0x2664c),top=b.i32(0x34),cur=cursor(s);
            if(list_filter_51f460(L,b.u8(0x34),s.g.online_7d68bc,e>1,0,0,s))fault(0x4ca8c0);
            restore_4cab60(c,s,0,top,cur,saved);
            labels(s,{4,0x295,~0u,~0u,~0u,~0u,8,0x296});
            break;
        }
        default:break;
        }
        frontend_rankings_list_tick_51ec80(L,s);
        return true;
    }catch(const Fault& f){c.fault=s.missing=f.pc;}catch(const std::out_of_range&){c.fault=s.missing=0x4cb480;}
    return false;
}
bool frontend_rankings_display_4cc390(FrontendRankings& c,FrontendRankingServices& s){
    c.glyphs.clear();c.images.clear();
    try{
        if(!c.constructed)fault(0x4cc390);
        frontend_rankings_list_display_51edc0(list_of(c),c,s);
        text_display(s,obj(c).sub(0x271e4,0x48c),c);text_display(s,obj(c).sub(0x27afc,0x48c),c);
        return true;
    }catch(const Fault& f){c.fault=s.missing=f.pc;}catch(const std::out_of_range&){c.fault=s.missing=0x4cc390;}
    return false;
}
bool frontend_rankings_suspend_4ca6f0(FrontendRankings& c,FrontendRankingServices& s){
    c.glyphs.clear();c.images.clear();
    try{
        if(!c.constructed)return true;
        auto b=obj(c);auto api=carousel_api(s);
        if(!frontend_carousel_release_51bc30(b.sub(0x38,0x118),api))carousel_fail(api);
        for(unsigned off:{0x1f0u,0x150u,0x290u,0x2645cu})release(s,b.sub(off,0xa0));
        list_release_51c600(list_of(c),s);b.put8(0x334,0);
        return true;
    }catch(const Fault& f){c.fault=s.missing=f.pc;}catch(const std::out_of_range&){c.fault=s.missing=0x4ca6f0;}
    return false;
}
// 4CAEB0: resource releases of the destructor chain (4CAE00 list, 465250).
bool frontend_rankings_destroy_4caeb0(FrontendRankings& c,FrontendRankingServices& s){
    c.glyphs.clear();c.images.clear();
    if(!c.constructed)return true;
    try{
        auto b=obj(c);auto L=list_of(c);
        release(s,L.sub(0x26120,0xa0));
        for(int i=14;i>=0;--i){auto r=recB(L,i);for(int k=9;k>=0;--k)release(s,res_at(r,0xe0c+unsigned(k)*0xa0));}
        for(int i=14;i>=0;--i){auto r=recA(L,i);for(int k=9;k>=0;--k)release(s,res_at(r,0xe0c+unsigned(k)*0xa0));}
        for(unsigned off:{0x290u,0x1f0u,0x150u,0x3cu})release(s,b.sub(off,0xa0));
        c.constructed=false;return true;
    }catch(const Fault& f){c.fault=s.missing=f.pc;}catch(const std::out_of_range&){c.fault=s.missing=0x4caeb0;}
    return false;
}

// ============================================================================
// Key 37 (4CEBF0): Time Attack board.
namespace {
constexpr unsigned GList=0x298;
constexpr Frame Titles5ca350[8]{{0x4400de,2,2},{0x4400de,3,3},{0x4400de,4,4},{0x4400de,5,5},{0x4400de,6,6},
    {0x4400de,7,7},{0x4400de,8,8},{0x4400cc,1,1}};
constexpr FrontendCarouselDescriptor Carousel5ca2d8[5]{{0x44002b,0,10},{0x44002b,10,10},{0x44002b,10,40},{0x44002b,40,40},{0x44002b,40,10}};
constexpr FrontendCarouselDescriptor Carousel5ca314[5]{{0x440084,0,10},{0x440084,10,10},{0x440084,10,40},{0x440084,40,40},{0x440084,40,10}};
constexpr std::uint32_t NextKey690e80[2]{0x26,0x27};   // 690E80 + carousel*8 (.data)
FrontendCarouselTable ghost_table(const S& s){
    return (mode(s)&3)?FrontendCarouselTable{Carousel5ca314,5,0x5ca314}:FrontendCarouselTable{Carousel5ca2d8,5,0x5ca2d8};
}
Bytes gobj(FrontendGhostBoard& c){return Bytes(c.object.data(),c.object.size());}
Bytes glist(FrontendGhostBoard& c){return gobj(c).sub(GList,0x26ea4);}
Board board37(FrontendGhostBoard& c,S& s){return Board{gobj(c),glist(c),0x34,0x2713c,0x291,s.g.cursor_690e7c};}
// 4CE420: carousel over 5CA2D8/5CA314, selected by the 0x800 bit.
void ghost_carousel_4ce420(FrontendGhostBoard& c,S& s){
    auto api=carousel_api(s);auto w=gobj(c).sub(0x38,0x118);
    if(!frontend_carousel_init_51b7b0(w,ghost_table(s),0,4,api))carousel_fail(api);
    frontend_carousel_select_51bdb0(w,(mode(s)&0x800)?0:1);
}
// 4CE480: five local records (+ one for modes 0x20/0x80).
void ghost_load_4ce480(FrontendGhostBoard& c,S& s){
    auto b=gobj(c);auto L=glist(c);g85b318(s)=0;
    for(int i=0;i<5;++i)list_add_51e140(L,{1,i,0,0,0,i,0,i,0,0,0,-1},s);
    b.put8(0x27a65,5);
    const unsigned m=mode(s)&0xe0;
    if(m==0x20||m==0x80){g85b318(s)=1;b.put8(0x27a64,6);b.put8(0x27a65,6);list_add_51e140(L,{1,5,0,0,0,0,0,5,0,0,0,-1},s);g85b318(s)=0;}
    else{g85b318(s)=0;b.put8(0x27a64,5);}
}
void ghost_fifteen(FrontendGhostBoard& c,S& s){
    auto L=glist(c);for(int i=0;i<15;++i)list_add_51e140(L,{2,i,1,4,1,2,0,i,0,0,0,-1},s);
}
// 4CE8E0(keep_carousel): rebuild the board for the current carousel index.
void ghost_reload_4ce8e0(FrontendGhostBoard& c,S& s,bool keep_carousel){
    auto b=gobj(c);auto L=glist(c);
    text_printf(b.sub(0x27144,0x48c),"");b.put8(0x27140,0);
    if(!keep_carousel)ghost_carousel_4ce420(c,s);
    b.put32(0x2713c,0);
    if(b.i32(0xec)!=0){
        frontend_rankings_list_reset_51c440(L,b.i8(0x27a65)==6?6:1,false,s);g85b317(s)=1;
        online(s,0x4ce955);
        ghost_fifteen(c,s);
    }else{
        frontend_rankings_list_reset_51c440(L,(mode(s)&0xe0)==0x80?6:5,false,s);g85b317(s)=0;
        online(s,0x4ce9e7);
        ghost_load_4ce480(c,s);
    }
    frontend_rankings_list_fill_51f280(L,b.u8(0x263b5),b.u8(0x2713c),false,s);
}
// 4CE540: filter title (layer 4).
void ghost_title_4ce540(FrontendGhostBoard& c,S& s,int k){
    auto r=gobj(c).sub(0x150,0xa0);release(s,r);
    const auto& e=Titles5ca350[k];configure(s,r,e.token,e.first,e.last,4,0,25.f,-338.f);
}
// 4CEB60
void ghost_filter_label_4ceb60(FrontendGhostBoard& c,S& s){
    online(s,0x4ceb75);
    switch(gobj(c).i8(0x263b5)){
    case 0:ghost_title_4ce540(c,s,6);break;
    case 1:ghost_title_4ce540(c,s,2);break;
    case 2:ghost_title_4ce540(c,s,1);break;
    default:text_printf(gobj(c).sub(0x275d0,0x48c),"UNKNOWN FILTER");break;
    }
}
}
bool frontend_ghosts_construct_4cebf0(FrontendGhostBoard& c,FrontendRankingServices& s){
    c.fault=0;c.constructed=false;c.glyphs.clear();c.images.clear();
    if(c.object.size()!=PcGhostBoardBytes)c.object.assign(PcGhostBoardBytes,0);   // keeps a bound storage
    try{
        auto b=gobj(c);auto* p=c.object.data();
        if(!title_base_construct_48f480(p,c.object.size(),s.repeat))fault(0x48f480);
        b.put32(0,0x5ca3b0);
        for(unsigned off:{0x3cu,0x150u,0x1f0u})if(!title_ui_resource_construct_465160(p+off,0xa0))fault(0x465160);
        frontend_rankings_list_construct_51e080(glist(c),s);
        for(unsigned off:{0x27144u,0x275d0u})if(!title_widget_construct_48e590(p+off,0x48c,s.repeat))fault(0x48e590);
        b.put8(0x292,0);b.put8(0x34,0);b.put32(8,0x25);b.put8(0x27a65,6);g85b317(s)=0;s.g.cursor_690e7c=5;
        c.constructed=true;return true;
    }catch(const Fault& f){c.fault=s.missing=f.pc;}catch(const std::out_of_range&){c.fault=s.missing=0x4cebf0;}
    return false;
}
bool frontend_ghosts_init_4cece0(FrontendGhostBoard& c,FrontendRankingServices& s){
    try{
        if(!c.constructed)fault(0x4cece0);
        auto b=gobj(c);auto L=glist(c);
        b.put8(0x291,0);b.put8(0x293,0);b.put32(4,0x53);b.put8(0x292,0);b.put8(0x27140,0);b.put32(0x27a60,0);
        b.putf(0x27a5c,s.timer);
        std::array<std::uint8_t,0x438> fmt{};Bytes f(fmt.data(),fmt.size());frontend_text_format_construct_48d570(f);
        f.put32(0x400,9);f.put32(0x404,0xd);f.put8(0x41c,0);f.put32(0x420,0xff3f474a);
        for(auto [off,y]:{std::pair<unsigned,float>{0x27144,342.f},{0x275d0,82.f}}){auto w=b.sub(off,0x48c);
            frontend_text_init_48e640(w);text_apply_format(w,f);text_printf(w,"");text_position(w,60.f,y);}
        ghost_carousel_4ce420(c,s);
        b.put32(0x2713c,0);
        const unsigned bits=s.root.u32(0x20c);const bool wide=(bits&0x800)!=0;
        if((bits&0xe0)==0xa0){frontend_rankings_list_reset_51c440(L,wide?5:1,false,s);b.put8(0x27a65,5);s.g.cursor_690e7c=4;}
        else if(wide){frontend_rankings_list_reset_51c440(L,6,false,s);b.put8(0x27a65,6);s.g.cursor_690e7c=5;}
        else{frontend_rankings_list_reset_51c440(L,1,false,s);b.put8(0x27a65,5);s.g.cursor_690e7c=4;}
        b.put8(0x263b5,0);
        online(s,0x4cee7f);
        g85b318(s)=0;g85b324(s)=0;g85b317(s)=(mode(s)&0x800)?0:1;
        ghost_reload_4ce8e0(c,s,true);
        labels(s,{4,0x295,~0u,~0u,~0u,~0u,8,0x296});
        frontend_rankings_list_tick_51ec80(L,s);
        return true;
    }catch(const Fault& f){c.fault=s.missing=f.pc;}catch(const std::out_of_range&){c.fault=s.missing=0x4cece0;}
    return false;
}
bool frontend_ghosts_control_4cef30(FrontendGhostBoard& c,FrontendRankingServices& s,unsigned& result){
    result=0;
    try{
        if(!c.constructed)fault(0x4cef30);
        auto b=gobj(c);auto L=glist(c);auto& root=s.root;auto bd=board37(c,s);
        auto bits=[&]{return root.u32(0x20c);};auto set_bits=[&](unsigned v){root.put32(0x20c,v);};
        online(s,0x4cef4d);
        if(b.u8(0x293))fault(0x4cf001);
        board_labels(bd,s);
        if(s.g.download_672df7)fault(0x4cf09e);
        auto cur=b.sub(0x1f0,0xa0);
        if(!b.u8(0x34)){if(cur.i32(8)==-1)configure(s,cur,0x4400cc,1,1,4,0,0,0);}
        else if(cur.i32(8)!=-1)release(s,cur);
        tick(s,cur);
        driving::object_store_depth_pair_442f20(root,0,0);
        if(b.u8(0x292)){result=b.u32(0x294);return true;}
        auto api=carousel_api(s);auto w=b.sub(0x38,0x118);
        if(!frontend_carousel_tick_51be30(w,ghost_table(s),api))carousel_fail(api);
        if(b.u8(0x27140))fault(0x4cf17c);
        ghost_filter_label_4ceb60(c,s);
        const int top=b.i32(0x2713c),cursor=s.g.cursor_690e7c,saved=L.i32(0x2664c);
        unsigned action{};
        if(!frontend_input_action_48f5f0(c.object.data(),c.object.size(),s.input,1,s.repeat,&s,
            [](void* p,unsigned key,int arg){auto& x=*static_cast<S*>(p);return x.ui.input_feedback(x.root,key,arg);},action))
            fault(s.ui.missing_pc?s.ui.missing_pc:0x48f5f0);
        auto row=[&](int i){return flag_row(L,i);};
        const int ec=b.i32(0xec);
        switch(action){
        case 6:case 8:
            if(list_filter_51f460(L,b.u8(0x2713c),s.g.online_7d68bc,0,0,0,s))fault(0x4cea20);
            board_restore(bd,s,0,top,cursor,saved);
            labels(s,{4,0x295,~0u,~0u,~0u,~0u,8,0x296});
            break;
        case 0:{
            const std::uint8_t focus=b.u8(0x34);
            if(!focus){
                b.put32(0x294,1);
                if(ec<0||ec>1)fault(0x4cf3b2);                        // 690E80 holds two keys
                b.put32(4,NextKey690e80[ec]);
                if(ec==0)set_bits(bits()|0x800);else set_bits(bits()&~0x800u);
                g85b318(s)=0;b.put8(0x292,1);
            }else if(focus==1){
                if(ec==1&&top<10){list_scroll(L,top+5,5,false,s);b.puti(0x2713c,top+5);}
                L.put8(0x2611c,1);
            }else if(focus==7){
                if(ec==1&&top>=5){list_scroll(L,top-5,5,true,s);b.puti(0x2713c,top-5);}
                L.put8(0x2611c,2);
            }
            break;
        }
        case 1:
            b.put32(0x294,2);set_bits(bits()|0x800);g85b317(s)=0;
            row(top+s.g.cursor_690e7c).put32(0x34,0);b.put8(0x292,1);break;
        case 3:{
            if(ec==0)break;
            b.put32(0x2713c,0);list_release_51c600(L,s);
            if((bits()&0xe0)==0xa0){b.put8(0x27a65,5);frontend_rankings_list_reset_51c440(L,5,false,s);}
            else{frontend_rankings_list_reset_51c440(L,6,false,s);b.put8(0x27a65,6);}
            s.g.cursor_690e7c=b.i8(0x27a65)-1;
            if(!frontend_carousel_move(w,ghost_table(s),false,api))carousel_fail(api);
            g85b317(s)=0;
            if(s.g.online_7d68bc&&b.i8(0x263b5)==0)fault(0x4cf5a0);
            ghost_load_4ce480(c,s);
            frontend_rankings_list_fill_51f280(L,b.u8(0x263b5),b.u8(0x2713c),false,s);
            board_restore(bd,s,1,top,cursor,saved);
            break;
        }
        case 5:{
            if(ec==1)break;
            b.put32(0x2713c,0);list_release_51c600(L,s);
            b.put8(0x27a65,5);frontend_rankings_list_reset_51c440(L,1,false,s);
            if(!frontend_carousel_move(w,ghost_table(s),true,api))carousel_fail(api);
            g85b317(s)=1;
            if(s.g.online_7d68bc&&b.i8(0x263b5)==0)fault(0x4cf633);
            ghost_fifteen(c,s);
            frontend_rankings_list_fill_51f280(L,b.u8(0x263b5),b.u8(0x2713c),false,s);
            board_restore(bd,s,1,top,cursor,saved);
            break;
        }
        case 2:{
            if(b.u8(0x27140)||ec==0)break;
            const std::int8_t focus=b.i8(0x34);
            if(focus==0){b.put8(0x34,1);L.put8(0x2611c,1);sound(s,1);}
            else if(focus==1){sound(s,1);b.put8(0x34,7);L.put8(0x2611c,2);}
            else if(focus>=2&&focus<=5){int k=s.g.cursor_690e7c;L.put8(0x2611c,0);
                if(k>0){row(b.i32(0x2713c)+k).put32(0x34,0);--k;s.g.cursor_690e7c=k;row(b.i32(0x2713c)+k).put32(0x34,1);}
                row(b.i32(0x2713c)+k).put32(0x34,1);b.put8(0x34,std::uint8_t(focus+1));}
            else if(focus==6){row(b.i32(0x2713c)+s.g.cursor_690e7c).put32(0x34,0);b.put8(0x34,7);L.put8(0x2611c,2);}
            break;
        }
        case 4:{
            if(ec==0)break;
            const std::int8_t focus=b.i8(0x34);
            if(focus==1){b.put8(0x34,0);L.put8(0x2611c,0);sound(s,1);}
            else if(focus==2){b.put8(0x34,1);row(b.i32(0x2713c)+s.g.cursor_690e7c).put32(0x34,0);L.put8(0x2611c,1);}
            else if(focus>=3&&focus<=6){int k=s.g.cursor_690e7c;L.put8(0x2611c,0);
                if(k<4){row(b.i32(0x2713c)+k).put32(0x34,0);++k;s.g.cursor_690e7c=k;row(b.i32(0x2713c)+k).put32(0x34,1);}
                row(b.i32(0x2713c)+k).put32(0x34,1);b.put8(0x34,std::uint8_t(focus-1));}
            else if(focus==7){sound(s,1);b.put8(0x34,1);L.put8(0x2611c,1);}
            break;
        }
        default:break;
        }
        frontend_rankings_list_tick_51ec80(L,s);
        return true;
    }catch(const Fault& f){c.fault=s.missing=f.pc;}catch(const std::out_of_range&){c.fault=s.missing=0x4cef30;}
    return false;
}
bool frontend_ghosts_display_4ce3b0(FrontendGhostBoard& c,FrontendRankingServices& s){
    c.glyphs.clear();c.images.clear();
    try{
        if(!c.constructed)fault(0x4ce3b0);
        frontend_rankings_list_display_51edc0(glist(c),c,s);
        text_display(s,gobj(c).sub(0x27144,0x48c),c);text_display(s,gobj(c).sub(0x275d0,0x48c),c);
        return true;
    }catch(const Fault& f){c.fault=s.missing=f.pc;}catch(const std::out_of_range&){c.fault=s.missing=0x4ce3b0;}
    return false;
}
bool frontend_ghosts_suspend_4ce3e0(FrontendGhostBoard& c,FrontendRankingServices& s){
    c.glyphs.clear();c.images.clear();
    try{
        if(!c.constructed)return true;
        auto b=gobj(c);auto api=carousel_api(s);
        if(!frontend_carousel_release_51bc30(b.sub(0x38,0x118),api))carousel_fail(api);
        release(s,b.sub(0x150,0xa0));release(s,b.sub(0x1f0,0xa0));
        list_release_51c600(glist(c),s);b.put8(0x292,0);
        return true;
    }catch(const Fault& f){c.fault=s.missing=f.pc;}catch(const std::out_of_range&){c.fault=s.missing=0x4ce3e0;}
    return false;
}
bool frontend_ghosts_destroy_4ce830(FrontendGhostBoard& c,FrontendRankingServices& s){
    c.glyphs.clear();c.images.clear();
    if(!c.constructed)return true;
    try{
        auto b=gobj(c);auto L=glist(c);
        release(s,L.sub(0x26120,0xa0));
        for(int i=14;i>=0;--i){auto r=recB(L,i);for(int k=9;k>=0;--k)release(s,res_at(r,0xe0c+unsigned(k)*0xa0));}
        for(int i=14;i>=0;--i){auto r=recA(L,i);for(int k=9;k>=0;--k)release(s,res_at(r,0xe0c+unsigned(k)*0xa0));}
        for(unsigned off:{0x1f0u,0x150u,0x3cu})release(s,b.sub(off,0xa0));
        c.constructed=false;return true;
    }catch(const Fault& f){c.fault=s.missing=f.pc;}catch(const std::out_of_range&){c.fault=s.missing=0x4ce830;}
    return false;
}

// ============================================================================
// Key 38 (4D05E0): Time Attack course board.
namespace {
constexpr unsigned CList=0x3dc;
// 5CA3C8/5CA468/5CA508/5CA5A8: 51B7B0(table,0,0x18) reads 25 records; the
// 13 authored ones are followed by the next table's words (EXE data).
constexpr FrontendCarouselDescriptor Carousel5ca3c8[25]{{0x44002c,0,10},{0x44002c,10,10},{0x44002c,10,40},{0x44002c,40,40},{0x44002c,40,70},{0x44002c,70,70},{0x44002c,70,100},{0x44002c,100,100},{0x44002c,100,130},{0x44002c,130,130},{0x44002c,130,160},{0x44002c,160,160},{0x44002c,160,130},{0x0,4456606,0},{0xa,4456606,10},{0xa,4456606,10},{0x28,4456606,40},{0x28,4456606,40},{0x46,4456606,70},{0x46,4456606,70},{0x64,4456606,100},{0x64,4456606,100},{0x82,4456606,130},{0x82,4456606,130},{0xa0,4456606,160}};
constexpr FrontendCarouselDescriptor Carousel5ca468[25]{{0x44009e,0,10},{0x44009e,10,10},{0x44009e,10,40},{0x44009e,40,40},{0x44009e,40,70},{0x44009e,70,70},{0x44009e,70,100},{0x44009e,100,100},{0x44009e,100,130},{0x44009e,130,130},{0x44009e,130,160},{0x44009e,160,160},{0x44009e,160,130},{0x0,4456498,0},{0xa,4456498,10},{0xa,4456498,10},{0x28,4456498,40},{0x28,4456498,40},{0x46,4456498,70},{0x46,4456498,70},{0x64,4456498,100},{0x64,4456498,100},{0x82,4456498,130},{0x82,4456498,130},{0xa0,4456498,160}};
constexpr FrontendCarouselDescriptor Carousel5ca508[25]{{0x440032,0,10},{0x440032,10,10},{0x440032,10,40},{0x440032,40,40},{0x440032,40,70},{0x440032,70,70},{0x440032,70,100},{0x440032,100,100},{0x440032,100,130},{0x440032,130,130},{0x440032,130,160},{0x440032,160,160},{0x440032,160,130},{0x0,4456605,0},{0xa,4456605,10},{0xa,4456605,10},{0x28,4456605,40},{0x28,4456605,40},{0x46,4456605,70},{0x46,4456605,70},{0x64,4456605,100},{0x64,4456605,100},{0x82,4456605,130},{0x82,4456605,130},{0xa0,4456605,160}};
constexpr FrontendCarouselDescriptor Carousel5ca5a8[25]{{0x44009d,0,10},{0x44009d,10,10},{0x44009d,10,40},{0x44009d,40,40},{0x44009d,40,70},{0x44009d,70,70},{0x44009d,70,100},{0x44009d,100,100},{0x44009d,100,130},{0x44009d,130,130},{0x44009d,130,160},{0x44009d,160,160},{0x44009d,160,130},{0x0,4456670,2},{0x2,4456670,3},{0x3,4456670,4},{0x4,4456670,5},{0x5,4456670,6},{0x6,4456670,7},{0x7,4456670,8},{0x8,4456652,1},{0x1,4456665,1},{0x1,4456665,0},{0x0,4456519,5},{0x5,5047984,5046688}};
constexpr Frame Titles5ca648[9]{{0x4400de,2,2},{0x4400de,3,3},{0x4400de,4,4},{0x4400de,5,5},{0x4400de,6,6},
    {0x4400de,7,7},{0x4400de,8,8},{0x4400cc,1,1},{0x4400d9,1,1}};
Bytes cobj(FrontendCourseBoard& c){return Bytes(c.object.data(),c.object.size());}
Bytes clist(FrontendCourseBoard& c){return cobj(c).sub(CList,0x26ea4);}
FrontendCarouselTable course_table(FrontendCourseBoard& c,const S& s){
    if((mode(s)&0xe0)!=0x80)fault(0x4cfce6);                              // 51B7B0(a1,a1,a1): no table
    const bool alt=cobj(c).u8(0x3d3)!=0;
    if(mode(s)&3)return alt?FrontendCarouselTable{Carousel5ca5a8,25,0x5ca5a8}:FrontendCarouselTable{Carousel5ca508,25,0x5ca508};
    return alt?FrontendCarouselTable{Carousel5ca468,25,0x5ca468}:FrontendCarouselTable{Carousel5ca3c8,25,0x5ca3c8};
}
// 4CFC40(toggle): course set (+3D3 <-> 84B0F8) and its carousel.
void course_carousel_4cfc40(FrontendCourseBoard& c,S& s,int a1){
    auto b=cobj(c);
    if(a1==1){const std::uint8_t v=b.u8(0x3d3)==0;b.put8(0x3d3,v);s.g.variant_84b0f8=v;}
    else b.put8(0x3d3,s.g.variant_84b0f8);
    const int old=b.i32(0xec);
    auto api=carousel_api(s);auto w=b.sub(0x38,0x118);
    if(!frontend_carousel_release_51bc30(w,api))carousel_fail(api);
    const auto t=course_table(c,s);
    if(!frontend_carousel_init_51b7b0(w,t,0,0x18,api))carousel_fail(api);
    if(a1==1){frontend_carousel_select_51bdb0(w,old);return;}
    const int bits=int((mode(s)>>12)&0x3f);
    frontend_carousel_select_51bdb0(w,b.u8(0x3d3)?bits-6:bits);
}
// 4CFB90: record set flags 85B318/85B324; returns the course column.
std::int8_t course_column_4cfb90(FrontendCourseBoard& c,S& s){
    auto b=cobj(c);const unsigned m=(mode(s)>>5)&7;const int ec=b.i32(0xec);
    g85b318(s)=0;g85b324(s)=0;
    if(m==1){if(ec>=5){g85b318(s)=1;return 0;}return std::int8_t(ec);}
    if(m==4||m==5){
        if(ec<5){g85b324(s)=b.u8(0x3d3)?1:0;return std::int8_t(ec);}
        if(m==4&&ec==5){g85b318(s)=1;g85b324(s)=1;if(!b.u8(0x3d3))g85b324(s)=0;}
        return 0;
    }
    return std::int8_t(ec);
}
void course_ten(FrontendCourseBoard& c,S& s,bool extra){
    auto L=clist(c);const std::int16_t v=course_column_4cfb90(c,s);
    for(int i=0;i<10;++i){
        if(extra)list_add_51e140(L,{6,i,1,4,1,2,0,v,i,0,0,-1},s);
        else list_add_51e140(L,{6,i,0,0,0,0,0,v,i,0,0,-1},s);
    }
}
// 4D03A0(keep): rebuild after a set/course change.
void course_reload_4d03a0(FrontendCourseBoard& c,S& s,int a1){
    auto b=cobj(c);auto L=clist(c);
    text_printf(b.sub(0x27288,0x48c),"");b.put8(0x27284,0);
    b.put32(0x27280,0);list_release_51c600(L,s);frontend_rankings_list_reset_51c440(L,1,false,s);
    b.put8(0x264f9,0);
    if(a1==0)course_carousel_4cfc40(c,s,0);
    online(s,0x4d0402);
    course_ten(c,s,false);
    frontend_rankings_list_fill_51f280(L,b.u8(0x264f9),b.u8(0x27280),false,s);
    if(a1==0)course_carousel_4cfc40(c,s,0);
}
// 4CFDB0: filter title on +1F0.
void course_title_4cfdb0(FrontendCourseBoard& c,S& s,int k){
    auto r=cobj(c).sub(0x1f0,0xa0);release(s,r);
    const auto& e=Titles5ca648[k];configure(s,r,e.token,e.first,e.last,0xf,0,25.f,-338.f);
}
void course_filter_label_4d0550(FrontendCourseBoard& c,S& s){
    online(s,0x4d0565);
    switch(cobj(c).i8(0x264f9)){
    case 0:course_title_4cfdb0(c,s,6);break;
    case 1:course_title_4cfdb0(c,s,2);break;
    case 2:course_title_4cfdb0(c,s,1);break;
    default:text_printf(cobj(c).sub(0x27714,0x48c),"UNKNOWN FILTER");break;
    }
}
// 4CFE10(a1 scroll, a2 rebuilt, top, cursor, list mode).
void course_restore_4cfe10(FrontendCourseBoard& c,S& s,int a1,int a2,int a3,int a4,int a5){
    auto b=cobj(c);auto L=clist(c);bool done=false;
    auto row=[&](int i){return flag_row(L,i);};
    auto reset=[&]{b.put8(0x34,0);L.put8(0x2611c,0);s.g.cursor_691534=5;};
    auto finish=[&](int edi,int ebp){if(!done)row(edi+ebp).put32(0x34,1);b.puti(0x27280,ebp);s.g.cursor_691534=edi;};
    const bool on=s.g.online_7d68bc!=0;const std::int8_t f=L.i8(0x2611d);
    if(std::uint8_t(a2)==0){
        if(on){reset();return;}
        if(f!=3&&f!=0)return;
        const std::uint8_t focus=b.u8(0x34);if(!focus)return;
        int edi=a4;
        if(a5==1){
            if(focus==1){edi=5;L.put8(0x2611c,1);done=true;}
            else if(focus==7){edi=0;b.put8(0x34,7);L.put8(0x2611c,2);done=true;}
        }
        const int ebp=a3;
        if(std::uint8_t(a1)&&a5==1&&L.i32(0x2664c)==1)list_scroll(L,ebp,ebp,false,s);
        finish(edi,ebp);return;
    }
    if(on&&f!=3){reset();return;}
    if(f!=3){if(f!=0||on)return;}
    const std::uint8_t focus=b.u8(0x34);if(!focus)return;
    int edi=a4,ebp=a3;
    if(a5==6){
        const int m=L.i32(0x2664c);
        if(m==5&&edi==5){edi=4;b.put8(0x34,1);}
        if(m==1&&edi==5){edi=4;b.put8(0x34,2);}
        finish(edi,ebp);return;
    }
    if(a5==1){
        const int m=L.i32(0x2664c);
        if(m==6){ebp=0;
            if(focus==1)edi=5;
            else if(focus==7){edi=0;b.put8(0x34,6);}
            else edi=a4;
        }
        if(m==1){const std::uint8_t now=b.u8(0x34);
            if(now==1){edi=5;L.put8(0x2611c,1);done=true;}
            else if(now==7){edi=0;b.put8(0x34,7);L.put8(0x2611c,2);done=true;}}
        if(m==5){const std::uint8_t now=b.u8(0x34);
            if(now==1)edi=4;else if(now==7){edi=0;b.put8(0x34,5);}
            ebp=0;}
        if(m==1)list_scroll(L,ebp,ebp,false,s);
        finish(edi,ebp);return;
    }
    finish(edi,ebp);
}
// Lock bit (4474E0) of the course the carousel shows; -1 when there is none.
int course_lock_bit(FrontendCourseBoard& c,S& s){
    auto b=cobj(c);const unsigned m=mode(s)&0xe0;const int ec=b.i32(0xec);const bool alt=b.u8(0x3d3)!=0;
    if(m==0x80){
        if(ec==5)return (mode(s)&3)?0xcb+2*int(alt):0xcc+2*int(alt);
        if(ec<5&&alt)return (mode(s)&3)?0xc1+2*ec:0xc2+2*ec;
        return -1;
    }
    if(m==0xa0&&ec<5&&alt)return (mode(s)&3)?0xc1+2*ec:0xc2+2*ec;
    return -1;
}
}
bool frontend_courses_construct_4d05e0(FrontendCourseBoard& c,FrontendRankingServices& s){
    c.fault=0;c.constructed=false;c.glyphs.clear();c.images.clear();
    if(c.object.size()!=PcCourseBoardBytes)c.object.assign(PcCourseBoardBytes,0);
    try{
        auto b=cobj(c);auto* p=c.object.data();
        if(!title_base_construct_48f480(p,c.object.size(),s.repeat))fault(0x48f480);
        b.put32(0,0x5ca6cc);
        for(unsigned off:{0x3cu,0x150u,0x1f0u,0x290u,0x330u})if(!title_ui_resource_construct_465160(p+off,0xa0))fault(0x465160);
        frontend_rankings_list_construct_51e080(clist(c),s);
        for(unsigned off:{0x27288u,0x27714u})if(!title_widget_construct_48e590(p+off,0x48c,s.repeat))fault(0x48e590);
        b.put32(8,0x26);b.put8(0x3d4,0);b.put8(0x3d3,0);
        c.constructed=true;return true;
    }catch(const Fault& f){c.fault=s.missing=f.pc;}catch(const std::out_of_range&){c.fault=s.missing=0x4d05e0;}
    return false;
}
bool frontend_courses_init_4d01a0(FrontendCourseBoard& c,FrontendRankingServices& s){
    try{
        if(!c.constructed)fault(0x4d01a0);
        auto b=cobj(c);auto L=clist(c);
        b.put8(0x34,0);b.put8(0x3d0,0);b.put8(0x3d2,0);b.put32(4,0x53);b.put8(0x3d4,0);b.put8(0x27284,0);
        g85b317(s)=0;s.g.cursor_691534=5;b.put32(0x27ba4,0);b.putf(0x27ba0,s.timer);
        std::array<std::uint8_t,0x438> fmt{};Bytes f(fmt.data(),fmt.size());frontend_text_format_construct_48d570(f);
        f.put32(0x400,9);f.put32(0x404,0xd);f.put8(0x41c,0);f.put32(0x420,0xff3f474a);
        for(auto [off,y]:{std::pair<unsigned,float>{0x27288,342.f},{0x27714,82.f}}){auto w=b.sub(off,0x48c);
            frontend_text_init_48e640(w);text_apply_format(w,f);text_printf(w,"");text_position(w,60.f,y);}
        course_carousel_4cfc40(c,s,0);
        b.put32(0x27280,0);frontend_rankings_list_reset_51c440(L,1,false,s);b.put8(0x264f9,0);
        if(s.g.online_7d68bc&&!s.g.online_7d68d2)fault(0x4d02d3);
        course_ten(c,s,false);
        frontend_rankings_list_fill_51f280(L,0,0,false,s);
        labels(s,{4,0x295,~0u,~0u,~0u,~0u,8,0x296});
        frontend_rankings_list_tick_51ec80(L,s);
        return true;
    }catch(const Fault& f){c.fault=s.missing=f.pc;}catch(const std::out_of_range&){c.fault=s.missing=0x4d01a0;}
    return false;
}
bool frontend_courses_control_4d06d0(FrontendCourseBoard& c,FrontendRankingServices& s,unsigned& result){
    result=0;
    try{
        if(!c.constructed)fault(0x4d06d0);
        auto b=cobj(c);auto L=clist(c);auto& root=s.root;
        auto bits=[&]{return root.u32(0x20c);};auto set_bits=[&](unsigned v){root.put32(0x20c,v);};
        online(s,0x4d06eb);
        if(b.u8(0x3d0))fault(0x4d07b2);
        Board bd{b,L,0x34,0x27280,0x3d2,s.g.cursor_691534};
        board_labels(bd,s);                                              // 4D0040
        if(s.g.download_672df7)fault(0x4d0849);
        auto set=b.sub(0x330,0xa0);
        if(set.i32(8)==-1){const unsigned f=b.u8(0x3d3)?1:0;configure(s,set,0x4400d9,f,f,4,0,0,0);}
        tick(s,set);
        auto cur=b.sub(0x290,0xa0);
        if(!b.u8(0x34)){if(cur.i32(8)==-1)configure(s,cur,0x4400cc,1,1,4,0,0,0);}
        else if(cur.i32(8)!=-1)release(s,cur);
        tick(s,cur);
        bool locked=false;auto lock=b.sub(0x150,0xa0);
        {const int bit=course_lock_bit(c,s);
         if(bit>=0&&!unlocked_4474e0(s,bit)){release(s,lock);configure(s,lock,0x440047,5,5,4,0,-363.f,-206.f);locked=true;}
         else release(s,lock);}
        driving::object_store_depth_pair_442f20(root,0,0);
        if(b.u8(0x3d4)){result=b.u32(0x3d8);return true;}
        auto api=carousel_api(s);auto w=b.sub(0x38,0x118);
        if(!frontend_carousel_tick_51be30(w,course_table(c,s),api))carousel_fail(api);
        if(b.u8(0x27284))fault(0x4d0b2e);
        course_filter_label_4d0550(c,s);
        unsigned action{};
        if(!frontend_input_action_48f5f0(c.object.data(),c.object.size(),s.input,0,s.repeat,&s,
            [](void* p,unsigned key,int arg){auto& x=*static_cast<S*>(p);return x.ui.input_feedback(x.root,key,arg);},action))
            fault(s.ui.missing_pc?s.ui.missing_pc:0x48f5f0);
        const int cursor=s.g.cursor_691534,top=b.i32(0x27280),saved=L.i32(0x2664c);
        auto row=[&](int i){return flag_row(L,i);};
        const int ec=b.i32(0xec);const unsigned m=bits()&0xe0;
        auto leave_with_course=[&](int course,bool wide){
            if(wide)set_bits(bits()|0x800);
            set_bits(bits()^((bits()^(unsigned(course)<<12))&0x3f000u));
            b.put32(0x3d8,1);b.put32(4,0xa);sound(s,0x40);b.put8(0x3d4,1);};
        switch(action){
        case 6:case 8:
            if(list_filter_51f460(L,b.u8(0x27280),s.g.online_7d68bc,1,0,0,s))fault(0x4d0470);
            course_restore_4cfe10(c,s,0,0,top,cursor,saved);
            labels(s,{4,0x295,~0u,~0u,~0u,~0u,8,0x296});
            break;
        case 11:
            course_carousel_4cfc40(c,s,1);course_reload_4d03a0(c,s,1);
            course_restore_4cfe10(c,s,1,0,top,cursor,saved);
            release(s,set);
            break;
        case 0:{
            const std::uint8_t focus=b.u8(0x34);
            if(!focus){
                const bool alt=b.u8(0x3d3)!=0;
                if(m==0x80){
                    if(ec==5){
                        if(!unlocked_4474e0(s,course_lock_bit(c,s))){sound(s,3);break;}
                        leave_with_course(ec+(alt?6:0),true);return true;
                    }
                    if(ec<5&&alt){
                        if(!unlocked_4474e0(s,course_lock_bit(c,s))){sound(s,3);break;}
                        leave_with_course(ec+6,true);return true;
                    }
                    b.put32(0x3d8,1);b.put32(4,0xa);set_bits(bits()|0x800);
                    set_bits(bits()^((bits()^(unsigned(ec)<<12))&0x3f000u));sound(s,0x40);b.put8(0x3d4,1);return true;
                }
                if(m!=0xa0)break;
                if(ec<5&&alt){
                    if(!unlocked_4474e0(s,course_lock_bit(c,s))){sound(s,3);break;}
                    leave_with_course(ec+5,false);return true;
                }
                leave_with_course(ec,false);return true;
            }
            if(focus==1){
                online(s,0x4d10c6);
                if(top<5){list_scroll(L,top+5,5,false,s);b.puti(0x27280,top+5);sound(s,0x40);}
                L.put8(0x2611c,1);
            }else if(focus==7){
                if(top>0){online(s,0x4d1223);list_scroll(L,top-5,5,true,s);b.puti(0x27280,top-5);sound(s,0x40);}
                L.put8(0x2611c,2);
            }else if(!locked&&s.g.online_7d68bc)fault(0x4d1390);
            break;
        }
        case 1:
            s.g.variant_84b0f8=0;set_bits(bits()&0xfffc0fffu);b.put32(0x3d8,2);
            row(top+s.g.cursor_691534).put32(0x34,0);b.put8(0x3d4,1);break;
        case 3:case 5:{
            const bool right=action==5;
            if(right?ec==5:ec==0)break;
            b.put32(0x27280,0);list_release_51c600(L,s);frontend_rankings_list_reset_51c440(L,1,false,s);
            if(!right||m==0x80||ec!=4){if(!frontend_carousel_move(w,course_table(c,s),right,api))carousel_fail(api);}
            if(s.g.online_7d68bc){const auto f=b.u8(0x264f9);if(f==0||f==4||f==6||f==5)fault(right?0x4d1599:0x4d14a8);}
            course_ten(c,s,true);
            frontend_rankings_list_fill_51f280(L,b.u8(0x264f9),b.u8(0x27280),false,s);
            course_restore_4cfe10(c,s,0,1,top,cursor,saved);
            break;
        }
        case 2:{
            if(b.u8(0x27284))break;
            const std::int8_t focus=b.i8(0x34);
            if(focus==0){sound(s,1);b.put8(0x34,1);L.put8(0x2611c,1);}
            else if(focus==1){if(s.g.online_7d68bc)fault(0x4d1671);sound(s,1);b.put8(0x34,7);L.put8(0x2611c,2);}
            else if(focus>=2&&focus<=5){sound(s,1);int k=s.g.cursor_691534;L.put8(0x2611c,0);
                if(k>0){row(b.i32(0x27280)+k).put32(0x34,0);--k;s.g.cursor_691534=k;row(b.i32(0x27280)+k).put32(0x34,1);}
                row(b.i32(0x27280)+k).put32(0x34,1);b.put8(0x34,std::uint8_t(focus+1));}
            else if(focus==6){sound(s,1);row(b.i32(0x27280)+s.g.cursor_691534).put32(0x34,0);b.put8(0x34,7);L.put8(0x2611c,2);}
            break;
        }
        case 4:{
            const std::int8_t focus=b.i8(0x34);
            if(focus==1){sound(s,1);b.put8(0x34,0);L.put8(0x2611c,0);}
            else if(focus==2){b.put8(0x34,1);L.put8(0x2611c,1);row(top+cursor).put32(0x34,0);sound(s,1);}
            else if(focus>=3&&focus<=6){sound(s,1);L.put8(0x2611c,0);int k=s.g.cursor_691534;
                if(k<4){row(b.i32(0x27280)+k).put32(0x34,0);++k;row(b.i32(0x27280)+k).put32(0x34,1);s.g.cursor_691534=k;}
                row(b.i32(0x27280)+k).put32(0x34,1);b.put8(0x34,std::uint8_t(focus-1));}
            else if(focus==7){sound(s,1);if(s.g.online_7d68bc&&L.i8(0x2611d)!=3)fault(0x4d19d2);b.put8(0x34,1);L.put8(0x2611c,1);}
            break;
        }
        default:break;
        }
        frontend_rankings_list_tick_51ec80(L,s);
        return true;
    }catch(const Fault& f){c.fault=s.missing=f.pc;}catch(const std::out_of_range&){c.fault=s.missing=0x4d06d0;}
    return false;
}
bool frontend_courses_display_4cfb10(FrontendCourseBoard& c,FrontendRankingServices& s){
    c.glyphs.clear();c.images.clear();
    try{
        if(!c.constructed)fault(0x4cfb10);
        frontend_rankings_list_display_51edc0(clist(c),c,s);
        text_display(s,cobj(c).sub(0x27288,0x48c),c);text_display(s,cobj(c).sub(0x27714,0x48c),c);
        return true;
    }catch(const Fault& f){c.fault=s.missing=f.pc;}catch(const std::out_of_range&){c.fault=s.missing=0x4cfb10;}
    return false;
}
bool frontend_courses_suspend_4cfb40(FrontendCourseBoard& c,FrontendRankingServices& s){
    c.glyphs.clear();c.images.clear();
    try{
        if(!c.constructed)return true;
        auto b=cobj(c);auto api=carousel_api(s);
        if(!frontend_carousel_release_51bc30(b.sub(0x38,0x118),api))carousel_fail(api);
        for(unsigned off:{0x150u,0x1f0u,0x290u,0x330u})release(s,b.sub(off,0xa0));
        list_release_51c600(clist(c),s);b.put8(0x3d4,0);
        return true;
    }catch(const Fault& f){c.fault=s.missing=f.pc;}catch(const std::out_of_range&){c.fault=s.missing=0x4cfb40;}
    return false;
}
bool frontend_courses_destroy_4d00d0(FrontendCourseBoard& c,FrontendRankingServices& s){
    c.glyphs.clear();c.images.clear();
    if(!c.constructed)return true;
    try{
        auto b=cobj(c);auto L=clist(c);
        release(s,L.sub(0x26120,0xa0));
        for(int i=14;i>=0;--i){auto r=recB(L,i);for(int k=9;k>=0;--k)release(s,res_at(r,0xe0c+unsigned(k)*0xa0));}
        for(int i=14;i>=0;--i){auto r=recA(L,i);for(int k=9;k>=0;--k)release(s,res_at(r,0xe0c+unsigned(k)*0xa0));}
        for(unsigned off:{0x330u,0x290u,0x1f0u,0x150u,0x3cu})release(s,b.sub(off,0xa0));
        c.constructed=false;return true;
    }catch(const Fault& f){c.fault=s.missing=f.pc;}catch(const std::out_of_range&){c.fault=s.missing=0x4d00d0;}
    return false;
}

// ============================================================================
// Key 51 (4CE170): rankings mode carousel.
namespace {
constexpr FrontendCarouselDescriptor Carousel5ca198[10]{{0x44007b,0,10},{0x44007b,10,10},{0x44007b,10,40},{0x44007b,40,40},
    {0x44007b,40,70},{0x44007b,70,70},{0x44007b,70,100},{0x44007b,100,100},{0x44007b,100,130},{0x44007b,130,130}};
constexpr FrontendCarouselDescriptor Carousel5ca250[6]{{0x44007c,0,10},{0x44007c,10,10},{0x44007c,10,40},{0x44007c,40,40},
    {0x44007c,40,70},{0x44007c,70,70}};
FrontendCarouselTable mode_table(const S& s){
    return s.g.ghost_84b0f6==0?FrontendCarouselTable{Carousel5ca198,10,0x5ca198}:FrontendCarouselTable{Carousel5ca250,6,0x5ca250};
}
Bytes mobj(FrontendModeBoard& c){return Bytes(c.object.data(),c.object.size());}
Bytes mlist(FrontendModeBoard& c){return mobj(c).sub(0x1f4,0x26ea4);}
void clear_85b31c(S& s){for(unsigned i=0x14;i<0x1c;++i)s.g.area_85b308[i]=0;}   // 85B31C, 85B320
// 4CDD70: game mode bits (+20C bits 5..7) and 84B20D of the carousel index.
void mode_bits(S& s,int index){
    const auto bits=s.root.u32(0x20c);
    if(index==0){s.root.put32(0x20c,(bits&~0xc0u)|0x20);s.g.ta_84b20d=0;}
    else if(index==1){s.root.put32(0x20c,(bits&~0xa0u)|0x40);s.g.ta_84b20d=0;}
    else if(index==2){s.root.put32(0x20c,(bits&~0x60u)|0x80);s.g.ta_84b20d=1;}
}
// The 48D570 block of every board text (font 9, layer D, colour FF3F474A), then "" at (60, y).
void board_text(Bytes w,float y){
    std::array<std::uint8_t,0x438> fmt{};Bytes f(fmt.data(),fmt.size());frontend_text_format_construct_48d570(f);
    f.put32(0x400,9);f.put32(0x404,0xd);f.put8(0x41c,0);f.put32(0x420,0xff3f474a);
    frontend_text_init_48e640(w);text_apply_format(w,f);text_printf(w,"");text_position(w,60.f,y);
}
void release_list_records(Bytes L,S& s){
    release(s,L.sub(0x26120,0xa0));
    for(int i=14;i>=0;--i){auto r=recB(L,i);for(int k=9;k>=0;--k)release(s,res_at(r,0xe0c+unsigned(k)*0xa0));}
    for(int i=14;i>=0;--i){auto r=recA(L,i);for(int k=9;k>=0;--k)release(s,res_at(r,0xe0c+unsigned(k)*0xa0));}
}
}
bool frontend_modes_construct_4ce170(FrontendModeBoard& c,FrontendRankingServices& s){
    c.fault=0;c.constructed=false;c.glyphs.clear();c.images.clear();
    try{
        auto b=mobj(c);auto* p=c.object.data();
        if(!title_base_construct_48f480(p,c.object.size(),s.repeat))fault(0x48f480);
        b.put32(0,0x5ca2bc);
        for(unsigned off:{0x38u,0x14cu})if(!title_ui_resource_construct_465160(p+off,0xa0))fault(0x465160);
        frontend_rankings_list_construct_51e080(mlist(c),s);
        for(unsigned off:{0x270a0u,0x2752cu})if(!title_widget_construct_48e590(p+off,0x48c,s.repeat))fault(0x48e590);
        b.put32(8,0x33);b.put8(0x1ec,0);g85b317(s)=0;
        c.constructed=true;return true;
    }catch(const Fault& f){c.fault=s.missing=f.pc;}catch(const std::out_of_range&){c.fault=s.missing=0x4ce170;}
    return false;
}
bool frontend_modes_init_4cdc10(FrontendModeBoard& c,FrontendRankingServices& s){
    try{
        if(!c.constructed)fault(0x4cdc10);
        auto b=mobj(c);clear_85b31c(s);
        b.put32(4,0x53);b.put8(0x1ec,0);b.put8(0x27098,0);b.put32(0x279bc,0);b.putf(0x279b8,s.timer);
        board_text(b.sub(0x270a0,0x48c),342.f);board_text(b.sub(0x2752c,0x48c),82.f);
        auto api=carousel_api(s);auto w=b.sub(0x34,0x118);
        if(!frontend_carousel_init_51b7b0(w,mode_table(s),0,s.g.ghost_84b0f6==0?9:5,api))carousel_fail(api);
        frontend_carousel_select_51bdb0(w,s.g.mode_cursor_84b0f7);
        frontend_rankings_list_reset_51c440(mlist(c),6,false,s);
        b.put8(0x26311,0);
        return true;
    }catch(const Fault& f){c.fault=s.missing=f.pc;}catch(const std::out_of_range&){c.fault=s.missing=0x4cdc10;}
    return false;
}
bool frontend_modes_control_4cdf40(FrontendModeBoard& c,FrontendRankingServices& s,unsigned& result){
    result=0;
    try{
        if(!c.constructed)fault(0x4cdf40);
        auto b=mobj(c);
        if(b.u8(0x1ec)){result=b.u32(0x1f0);return true;}
        auto api=carousel_api(s);auto w=b.sub(0x34,0x118);
        if(!frontend_carousel_tick_51be30(w,mode_table(s),api))carousel_fail(api);
        unsigned action{};
        if(!frontend_input_action_48f5f0(c.object.data(),c.object.size(),s.input,1,s.repeat,&s,
            [](void* p,unsigned key,int arg){auto& x=*static_cast<S*>(p);return x.ui.input_feedback(x.root,key,arg);},action))
            fault(s.ui.missing_pc?s.ui.missing_pc:0x48f5f0);
        auto leave=[&](unsigned r){b.put32(0x1f0,r);if(!frontend_carousel_release_51bc30(w,api))carousel_fail(api);b.put8(0x1ec,1);};
        // After a move: OutRun2SP keeps the 4CDD70 mapping; the local table
        // stores index + 1 (left: Time Attack for index 2 and up).
        auto bits_after=[&](bool left){
            const int i=b.i32(0xe8);
            if(s.g.ghost_84b0f6){mode_bits(s,i);return;}
            auto bits=s.root.u32(0x20c);
            if(!left||i<2)bits^=((unsigned(i+1)<<5)^bits)&0xe0u;else bits=(bits&~0x60u)|0x80u;
            s.root.put32(0x20c,bits);
        };
        switch(action){
        case 0:mode_bits(s,b.i32(0xe8));s.g.mode_cursor_84b0f7=std::int8_t(b.u8(0xe8));b.put32(4,0x30);leave(1);break;
        case 1:s.g.mode_cursor_84b0f7=0;leave(2);break;
        case 3:
            if(b.i32(0xe8)==0)break;
            if(!frontend_carousel_move(w,mode_table(s),false,api))carousel_fail(api);
            mode_bits(s,b.i32(0xe8));bits_after(true);break;
        case 5:
            if(b.i32(0xe8)==2)break;
            if(!frontend_carousel_move(w,mode_table(s),true,api))carousel_fail(api);
            bits_after(false);break;
        default:break;
        }
        return true;
    }catch(const Fault& f){c.fault=s.missing=f.pc;}catch(const std::out_of_range&){c.fault=s.missing=0x4cdf40;}
    return false;
}
bool frontend_modes_suspend_4cdd50(FrontendModeBoard& c,FrontendRankingServices& s){
    c.glyphs.clear();c.images.clear();
    try{
        if(!c.constructed)return true;
        auto api=carousel_api(s);
        if(!frontend_carousel_release_51bc30(mobj(c).sub(0x34,0x118),api))carousel_fail(api);
        mobj(c).put8(0x1ec,0);return true;
    }catch(const Fault& f){c.fault=s.missing=f.pc;}catch(const std::out_of_range&){c.fault=s.missing=0x4cdd50;}
    return false;
}
bool frontend_modes_destroy_4cdea0(FrontendModeBoard& c,FrontendRankingServices& s){
    c.glyphs.clear();c.images.clear();
    if(!c.constructed)return true;
    try{
        release_list_records(mlist(c),s);
        for(unsigned off:{0x14cu,0x38u})release(s,mobj(c).sub(off,0xa0));
        c.constructed=false;return true;
    }catch(const Fault& f){c.fault=s.missing=f.pc;}catch(const std::out_of_range&){c.fault=s.missing=0x4cdea0;}
    return false;
}

// ============================================================================
// Keys 48/57 (4CCFC0): rankings course board.
namespace {
constexpr FrontendCarouselDescriptor Carousel5ca0a0[4]{{0x44002a,0,10},{0x44002a,10,10},{0x44002a,10,40},{0x44002a,40,40}};
constexpr Frame Titles5ca0e0[13]{{0x4400de,2,2},{0x4400de,3,3},{0x4400de,4,4},{0x4400de,5,5},{0x4400de,6,6},
    {0x4400de,7,7},{0x4400de,8,8},{0x4400de,9,9},{0x4400de,10,10},{0x4400de,11,11},{0x4400de,12,12},
    {0x440047,5,5},{0x4400cc,1,1}};
FrontendCarouselTable table48(){return {Carousel5ca0a0,4,0x5ca0a0};}
bool ghost_ta(S& s){return (mode(s)&0xe0)==0x80&&s.g.ghost_84b0f6;}
// 48FB40(filter, a2, a3) on 659930: the text of the filter button (key 0x10).
unsigned filter_text_48fb40(S& s,int f,bool a2,bool a3){
    const bool sp_ta=s.g.ghost_84b0f6==1&&(mode(s)&0xe0)==0x80;
    switch(f){
    case 0:if(a3)return 0x2a5;if(sp_ta)return s.g.flag_84b20e?0x297:0x2b2;return 0x2a6;
    case 1:return 0x2a8;
    case 2:return 0x2a6;
    case 3:return a2?0x2a5:0x47f;
    case 4:return 0x2a9;
    case 5:return 0x2aa;
    case 6:return s.g.flag_84b20e&&sp_ta?0x2b2:0x47f;
    case 7:return 0x2b3;
    case 8:return 0x2b5;
    case 9:return 0x2b4;
    case 10:return s.g.online_7d68bc?0x47f:0x2b2;
    default:return ~0u;
    }
}
// The OutRun2SP Time Attack filters 7..10 walk (85B31C, 85B320) through 00, 10, 11, 01.
void toggle_85b31c(S& s){
    auto& a=s.g.area_85b308;
    auto get=[&](unsigned o){std::uint32_t v;std::memcpy(&v,a.data()+o,4);return v;};
    auto set=[&](unsigned o,std::uint32_t v){std::memcpy(a.data()+o,&v,4);};
    if(get(0x14)==0){if(get(0x18)==0)set(0x14,1);else set(0x18,0);}
    else{if(get(0x18)==0)set(0x18,1);else set(0x14,0);}
}
void labels48(FrontendBoard48& c,S& s){
    if(ghost_ta(s)){labels(s,{4,0x295,0x10,filter_text_48fb40(s,obj(c).i8(0x26459),false,false),~0u,~0u,8,0x296});return;}
    labels(s,{4,0x295,~0u,~0u,~0u,~0u,8,0x296});
}
// 4CC410: the staging list from the common save (key 36's 4CA740 with one row layout).
void load_4cc410(FrontendBoard48& c,S& s){
    auto b=obj(c);auto L=list_of(c);
    int count=5,kind;const int ec=b.i32(0xec);
    if((mode(s)&0xe0)==0x20&&ec!=0&&ec!=1){kind=6;g85b318(s)=1;count=10;}else{kind=1;g85b318(s)=0;}
    g85b317(s)=0;g85b324(s)=0;
    for(int i=0;i<count;++i)list_add_51e140(L,{kind,i,0,0,0,i,0,i,0,0,0,-1},s);
    if(kind==6)return;
    const unsigned m=mode(s)&0xe0;
    if(m==0x20||m==0x80){g85b318(s)=1;b.put32(0x27f94,6);list_add_51e140(L,{1,5,0,0,0,0,0,5,0,0,0,-1},s);g85b318(s)=0;}
    else{g85b318(s)=0;b.put32(0x27f94,5);}
}
void title_4cc5f0(FrontendBoard48& c,S& s,int k){
    auto r=obj(c).sub(0x150,0xa0);release(s,r);
    const auto& e=Titles5ca0e0[k];configure(s,r,e.token,e.first,e.last,0xf,0,25.f,-338.f);
}
// 4CCE50 (offline, local tables).
void filter_label_4cce50(FrontendBoard48& c,S& s){
    online(s,0x4cce88);
    const auto f=obj(c).i8(0x26459);
    if(ghost_ta(s)){if(f>=7&&f<=10)title_4cc5f0(c,s,f);return;}
    switch(f){
    case 0:title_4cc5f0(c,s,6);break;
    case 1:title_4cc5f0(c,s,2);break;
    case 2:title_4cc5f0(c,s,1);break;
    default:text_printf(obj(c).sub(0x27afc,0x48c),"UNKNOWN FILTER");break;
    }
}
// 4CC650: rebuild the list for the current carousel index.
void reload_4cc650(FrontendBoard48& c,S& s){
    auto b=obj(c);auto L=list_of(c);
    b.put8(0x271e0,0);text_printf(b.sub(0x271e4,0x48c),"");b.put32(0x34,0);
    list_release_51c600(L,s);
    if((mode(s)&0xe0)==0x20){
        if(b.i32(0xec)<2){b.put32(0x27f98,6);s.g.cursor_69012c=5;frontend_rankings_list_reset_51c440(L,6,false,s);}
        else{b.put32(0x27f98,5);s.g.cursor_69012c=4;frontend_rankings_list_reset_51c440(L,1,false,s);}
    }else frontend_rankings_list_reset_51c440(L,b.i32(0x27f98)==5?5:6,false,s);
    if(s.g.online_7d68bc){const auto f=b.u8(0x26459);if(f==0||f==4||f==6||f==5)fault(0x4cc716);}
    load_4cc410(c,s);
    frontend_rankings_list_fill_51f280(L,b.u8(0x26459),b.u8(0x34),false,s);
}
// 4CC740 (a1, top, cursor, saved list mode): restore the highlighted row.
void restore_4cc740(FrontendBoard48& c,S& s,int a1,int a2,int a3,int a4){
    auto o=obj(c);auto L=list_of(c);auto& cur=s.g.cursor_69012c;
    auto row=[&](int i){return rec_row(c,i);};
    const bool on=s.g.online_7d68bc!=0;const std::int8_t f=L.i8(0x2611d);
    auto reset=[&]{o.put8(0x330,0);L.put8(0x2611c,0);cur=5;};
    if(std::uint8_t(a1)==0){
        if(on){reset();return;}
        if(f!=3&&f!=0)return;
        const std::uint8_t focus=o.u8(0x330);if(!focus)return;
        if(a4==1){
            if(focus==1){L.put8(0x2611c,1);o.puti(0x34,a2);cur=5;return;}
            if(focus==7){o.put8(0x330,7);L.put8(0x2611c,2);o.puti(0x34,a2);cur=0;return;}
        }
        row(a2+a3).put32(0x34,1);o.puti(0x34,a2);cur=a3;return;
    }
    if(on&&f!=3){reset();return;}
    if(f!=3&&(f!=0||on))return;
    if(!o.u8(0x330))return;
    int edi=a3,ebp=a2;bool done=false;
    const int m=L.i32(0x2664c);
    if(a4==6){
        if(m==5&&a3==5){edi=4;o.put8(0x330,1);}
        if(m==1&&edi==5){edi=4;o.put8(0x330,2);}
    }else if(a4==1){
        const std::uint8_t focus=o.u8(0x330);
        if(m==6){ebp=0;
            if(focus==1)edi=5;else if(focus==7){edi=0;o.put8(0x330,6);}}
        if(m==1){const std::uint8_t now=o.u8(0x330);
            if(now==1){edi=5;L.put8(0x2611c,1);done=true;}
            else if(now==7){edi=0;o.put8(0x330,7);L.put8(0x2611c,2);done=true;}}
        if(m==5){const std::uint8_t now=o.u8(0x330);ebp=0;
            if(now==1)edi=4;else if(now==7){edi=0;o.put8(0x330,5);}}
        if(m==1)list_scroll(L,ebp,ebp,false,s);
    }
    if(!done)row(edi+ebp).put32(0x34,1);
    cur=edi;o.puti(0x34,ebp);
}
// 4CC970: A + Back labels, refreshed when a row highlight appears or goes.
void labels_4cc970(FrontendBoard48& c,S& s){
    if(s.g.online_7d68bc)return;
    auto o=obj(c);bool any=false;
    for(int i=0;i<10;++i)if(rec_row(c,i).u32(0x34)){any=true;break;}
    if(any){if(!o.u8(0x334)){labels48(c,s);o.put8(0x334,1);}}
    else if(o.u8(0x334)){labels48(c,s);o.put8(0x334,0);}
}
}
bool frontend_board48_construct_4ccfc0(FrontendBoard48& c,FrontendRankingServices& s){
    c.fault=0;c.constructed=false;c.glyphs.clear();c.images.clear();
    try{
        auto b=obj(c);auto* p=c.object.data();
        if(!title_base_construct_48f480(p,c.object.size(),s.repeat))fault(0x48f480);
        b.put32(0,0x5ca17c);
        for(unsigned off:{0x3cu,0x150u,0x1f0u,0x290u})if(!title_ui_resource_construct_465160(p+off,0xa0))fault(0x465160);
        frontend_rankings_list_construct_51e080(list_of(c),s);
        for(unsigned off:{0x271e4u,0x27670u,0x27afcu})if(!title_widget_construct_48e590(p+off,0x48c,s.repeat))fault(0x48e590);
        b.put32(8,0x39);b.put8(0x333,0);b.put8(0x330,0);
        const unsigned m=mode(s)&0xe0;
        if(m==0xa0||m==0x40||m==0x60){b.put32(0x27f98,5);s.g.cursor_69012c=4;}else{b.put32(0x27f98,6);s.g.cursor_69012c=5;}
        b.put32(0x27f94,5);g85b317(s)=0;
        c.constructed=true;return true;
    }catch(const Fault& f){c.fault=s.missing=f.pc;}catch(const std::out_of_range&){c.fault=s.missing=0x4ccfc0;}
    return false;
}
bool frontend_board48_init_4ccbc0(FrontendBoard48& c,FrontendRankingServices& s){
    try{
        if(!c.constructed)fault(0x4ccbc0);
        auto b=obj(c);auto L=list_of(c);
        b.put8(0x332,0);b.put8(0x334,0);clear_85b31c(s);b.put32(4,0x53);b.put8(0x333,0);b.put8(0x271e0,0);g85b324(s)=0;
        b.put32(0x27f8c,0);b.putf(0x27f88,s.timer);
        board_text(b.sub(0x271e4,0x48c),342.f);board_text(b.sub(0x27afc,0x48c),82.f);
        auto api=carousel_api(s);auto w=b.sub(0x38,0x118);
        if(!frontend_carousel_init_51b7b0(w,table48(),0,4,api))carousel_fail(api);
        if((mode(s)&3)<2)frontend_carousel_select_51bdb0(w,int(mode(s)&3));
        frontend_rankings_list_reset_51c440(L,b.i32(0x27f98)==5?5:6,false,s);
        b.put8(0x26459,!s.g.online_7d68bc&&ghost_ta(s)?7:0);
        b.put32(0x34,0);
        if(s.g.online_7d68bc)fault(0x4ccd6d);
        load_4cc410(c,s);frontend_rankings_list_fill_51f280(L,0,0,false,s);
        labels48(c,s);
        frontend_rankings_list_tick_51ec80(L,s);
        return true;
    }catch(const Fault& f){c.fault=s.missing=f.pc;}catch(const std::out_of_range&){c.fault=s.missing=0x4ccbc0;}
    return false;
}
bool frontend_board48_control_4cd140(FrontendBoard48& c,FrontendRankingServices& s,unsigned& result){
    result=0;
    try{
        if(!c.constructed)fault(0x4cd140);
        auto b=obj(c);auto L=list_of(c);auto& root=s.root;
        auto bits=[&]{return root.u32(0x20c);};auto set_bits=[&](unsigned v){root.put32(0x20c,v);};
        online(s,0x4cd15b);
        if(b.u8(0x332))fault(0x4cd222);            // online labels restore
        labels_4cc970(c,s);
        auto cursor_res=b.sub(0x290,0xa0);
        if(!b.u8(0x330)){if(cursor_res.i32(8)==-1)configure(s,cursor_res,0x4400cc,1,1,4,0,0,0);}
        else if(cursor_res.i32(8)!=-1)release(s,cursor_res);
        tick(s,cursor_res);
        const int ec=b.i32(0xec);auto lock=b.sub(0x1f0,0xa0);
        if(ec>1&&!unlocked_4474e0(s,0xcb+(ec==2))){release(s,lock);configure(s,lock,0x440047,5,5,4,0,-363.f,-206.f);}
        else release(s,lock);
        driving::object_store_depth_pair_442f20(root,0,0);
        if(b.u8(0x333)){result=b.u32(0x338);return true;}
        auto api=carousel_api(s);auto w=b.sub(0x38,0x118);
        if(!frontend_carousel_tick_51be30(w,table48(),api))carousel_fail(api);
        if(b.u8(0x271e0))fault(0x4cd462);         // network ranking wait
        filter_label_4cce50(c,s);
        const int saved_cursor=s.g.cursor_69012c,saved_mode=L.i32(0x2664c),top=b.i32(0x34);
        unsigned action{};
        if(!frontend_input_action_48f5f0(c.object.data(),c.object.size(),s.input,1,s.repeat,&s,
            [](void* p,unsigned key,int arg){auto& x=*static_cast<S*>(p);return x.ui.input_feedback(x.root,key,arg);},action))
            fault(s.ui.missing_pc?s.ui.missing_pc:0x48f5f0);
        auto low_bits=[&]{if(b.i32(0xec)==0)set_bits(bits()&~3u);else set_bits((bits()&~2u)|1u);};
        switch(action){
        case 0:{
            const std::uint8_t focus=b.u8(0x330);
            if(!focus){
                const unsigned m=(bits()>>5)&7;const int e=b.i32(0xec);
                if(m>=1&&m<=3){
                    if(e>1){if(!unlocked_4474e0(s,0xcb+(e==2))){low_bits();break;}
                        set_bits(bits()^((bits()^unsigned(e))&3));}
                    b.put32(0x338,1);b.put32(4,0x32);b.put8(0x333,1);return true;   // no list tick
                }
                if(m==4||m==5){b.put32(0x338,1);b.put32(4,0x32);set_bits(bits()|0x800);b.put8(0x333,1);}
                low_bits();break;
            }
            if((bits()&0xe0)==0x20&&b.i32(0xec)>1){
                if(focus==1){
                    if(b.i32(0x34)<5){list_scroll(L,b.i32(0x34)+5,5,false,s);b.puti(0x34,b.i32(0x34)+5);}
                    b.put8(0x26458,1);
                }else if(focus==7){
                    if(b.i32(0x34)>0){list_scroll(L,b.i32(0x34)-5,5,true,s);b.puti(0x34,b.i32(0x34)-5);}
                    b.put8(0x26458,2);
                }
            }
            // 494030(6 - focus): a downloaded record in the 65F0C0 manager.
            const int slot=6-int(std::int8_t(focus));
            if(!s.records_65f0c0||slot<0)fault(0x494030);
            std::uint64_t entry{};std::memcpy(&entry,s.records_65f0c0+0x30+unsigned(slot)*0x40,8);
            if(entry)fault(0x4378e3);                // ghost download (672DF0) not ported
            break;
        }
        case 1:
            set_bits(bits()&~3u);set_bits(bits()&~0x800u);
            rec_row(c,b.i32(0x34)+s.g.cursor_69012c).put32(0x34,0);
            b.put32(0x338,2);b.put8(0x333,1);break;
        case 3:case 5:{
            const bool right=action==5;
            if(right?b.i32(0xec)==1:b.i32(0xec)==0)break;
            b.put32(0x34,0);list_release_51c600(L,s);
            frontend_rankings_list_reset_51c440(L,b.i32(0x27f98)==5?5:6,false,s);
            if(right)set_bits((bits()&~2u)|1u);else set_bits(bits()&~3u);
            if(!frontend_carousel_move(w,table48(),right,api))carousel_fail(api);
            if(s.g.online_7d68bc){const auto f=b.u8(0x26459);if(f==0||f==4||f==6||f==5)fault(0x4cdadd);}
            reload_4cc650(c,s);restore_4cc740(c,s,1,top,saved_cursor,saved_mode);
            break;
        }
        case 6:case 8:{
            if(s.g.ghost_84b0f6==1&&(mode(s)&0xe0)==0x80){          // OutRun2SP filters 7..10
                const auto f=b.i8(0x26459);if(f>=7&&f<=10)toggle_85b31c(s);
                list_filter_51f460(L,b.u8(0x34),0,0,0,1,s);reload_4cc650(c,s);
            }else if(list_filter_51f460(L,b.u8(0x34),s.g.online_7d68bc,b.i32(0xec)>1,0,0,s))fault(0x4cd681);
            restore_4cc740(c,s,0,top,saved_cursor,saved_mode);
            labels48(c,s);
            break;
        }
        default:break;
        }
        frontend_rankings_list_tick_51ec80(L,s);
        return true;
    }catch(const Fault& f){c.fault=s.missing=f.pc;}catch(const std::out_of_range&){c.fault=s.missing=0x4cd140;}
    return false;
}
bool frontend_board48_suspend_4cc3c0(FrontendBoard48& c,FrontendRankingServices& s){
    c.glyphs.clear();c.images.clear();
    try{
        if(!c.constructed)return true;
        auto b=obj(c);auto api=carousel_api(s);
        if(!frontend_carousel_release_51bc30(b.sub(0x38,0x118),api))carousel_fail(api);
        for(unsigned off:{0x1f0u,0x150u,0x290u,0x2645cu})release(s,b.sub(off,0xa0));
        list_release_51c600(list_of(c),s);b.put8(0x333,0);
        return true;
    }catch(const Fault& f){c.fault=s.missing=f.pc;}catch(const std::out_of_range&){c.fault=s.missing=0x4cc3c0;}
    return false;
}
bool frontend_board48_destroy_4ccaf0(FrontendBoard48& c,FrontendRankingServices& s){
    c.glyphs.clear();c.images.clear();
    if(!c.constructed)return true;
    try{
        release_list_records(list_of(c),s);
        for(unsigned off:{0x290u,0x1f0u,0x150u,0x3cu})release(s,obj(c).sub(off,0xa0));
        c.constructed=false;return true;
    }catch(const Fault& f){c.fault=s.missing=f.pc;}catch(const std::out_of_range&){c.fault=s.missing=0x4ccaf0;}
    return false;
}

// ============================================================================
// Keys 50/53 (4DA2F0): rankings list. Focus is the byte +34, the list top
// +271E0, the board cursor 6939F4.
namespace {
FrontendCarouselTable t50(const FrontendCarouselDescriptor* r,unsigned n,unsigned pc){return {r,n,pc};}
#define T50(a) t50(Rankings50_##a,unsigned(std::size(Rankings50_##a)),0x##a)
Bytes row50(FrontendBoard50& c,int i){return rec_row(c,i);}
// 440EB0 / 440EC0: the current label words of the root +51C slots.
std::uint32_t label_word(S& s,unsigned base,int i){
    auto slots=s.root.sub(0x51c,s.root.size()-0x51c);const auto page=slots.u32(0x408);
    if(page>=32)fault(0x4464b0);
    return slots.u32(base+(unsigned(std::int8_t(i))+page*8)*4);
}
// The 4DAF30 / 4DB051 label rebuild: (k0, t0, ec0(0), eb0(1), ec0(1), eb0(2), ec0(2), eb0(3)).
void labels_shifted(S& s,unsigned k0,unsigned t0){
    std::uint32_t a[4],b[4];for(int i=0;i<4;++i){a[i]=label_word(s,0x8,i);b[i]=label_word(s,0x18,i);}
    labels(s,{k0,t0,a[0],b[1],a[1],b[2],a[2],b[3]});
}
// 4D9750(keep): course carousel by mode, +20C low bits, 84B0F6 and the set byte +334.
void carousel_4d9750(FrontendBoard50& c,S& s,bool keep){
    auto b=obj(c);auto w=b.sub(0x38,0x118);auto api=carousel_api(s);const int saved=b.i32(0xec);
    if(!frontend_carousel_release_51bc30(w,api))carousel_fail(api);
    const unsigned m=(mode(s)>>5)&7;const bool low=(mode(s)&3)!=0;const bool alt=b.u8(0x334)!=0;
    FrontendCarouselTable t{};unsigned end=0;
    if(!low){
        if(m==1){t=T50(5cd490);end=0xc;}
        else if(m==2||m==3){t=T50(5cd5d0);end=0xa;}
        else if(m==4){if(s.g.ghost_84b0f6){t=T50(5cd490);end=0xc;}else if(alt){t=T50(5cce78);end=0x2a;}else{t=T50(5ccc70);end=0x2a;}}
    }else{
        if(m==1){t=T50(5cd530);end=0xc;}
        else if(m==2||m==3){t=T50(5cd658);end=0xa;}
        else if(m==4){if(s.g.ghost_84b0f6){t=T50(5cd530);end=0xc;}else if(alt){t=T50(5cd288);end=0x2a;}else{t=T50(5cd080);end=0x2a;}}
    }
    if(end&&!frontend_carousel_init_51b7b0(w,t,0,end,api))carousel_fail(api);
    frontend_carousel_select_51bdb0(w,keep?saved:0);
}
FrontendCarouselTable table50(FrontendBoard50& c,S& s){
    const unsigned m=(mode(s)>>5)&7;const bool low=(mode(s)&3)!=0;const bool alt=obj(c).u8(0x334)!=0;
    if(!low){
        if(m==1)return T50(5cd490);
        if(m==2||m==3)return T50(5cd5d0);
        if(m==4)return s.g.ghost_84b0f6?T50(5cd490):alt?T50(5cce78):T50(5ccc70);
    }else{
        if(m==1)return T50(5cd530);
        if(m==2||m==3)return T50(5cd658);
        if(m==4)return s.g.ghost_84b0f6?T50(5cd530):alt?T50(5cd288):T50(5cd080);
    }
    return {};
}
void title_4d98b0(FrontendBoard50& c,S& s,int k){
    auto r=obj(c).sub(0x150,0xa0);release(s,r);
    const auto& e=Rankings50_5cd6e0[k];configure(s,r,e.token,e.first,e.last,0xf,0,25.f,-338.f);
}
void filter_label_4da180(FrontendBoard50& c,S& s){
    online(s,0x4da1b9);
    const auto f=obj(c).i8(0x26459);
    if(ghost_ta(s)){if(f>=7&&f<=10)title_4d98b0(c,s,f);return;}
    switch(f){
    case 0:title_4d98b0(c,s,6);break;
    case 1:title_4d98b0(c,s,2);break;
    case 2:title_4d98b0(c,s,1);break;
    default:text_printf(obj(c).sub(0x27674,0x48c),"UNKNOWN FILTER");break;
    }
}
// 4D94A0: the ten rows of the selected course from the common save.
void load_4d94a0(FrontendBoard50& c,S& s){
    auto b=obj(c);auto L=list_of(c);
    g85b317(s)=0;g85b324(s)=0;g85b318(s)=0;
    const unsigned m=(mode(s)>>5)&7;const int ec=b.i32(0xec);const bool alt=b.u8(0x334)!=0;int edi=0;
    if(m==4){
        g85b324(s)=alt?1:0;
        if(ec<5){g85b317(s)=0;g85b318(s)=0;edi=ec;}
        else if(ec==5){g85b317(s)=0;g85b318(s)=1;edi=0;}
        else{g85b317(s)=1;g85b318(s)=0;edi=ec-6;}
    }else if(m==5){
        g85b324(s)=alt?1:0;g85b318(s)=0;
        if(ec<5){g85b317(s)=0;edi=ec;}else{g85b317(s)=1;edi=ec-5;}
    }else if(m==1){g85b324(s)=0;g85b317(s)=0;g85b318(s)=ec==5?1:0;}
    else if(m==2||m==3){g85b317(s)=0;g85b324(s)=0;g85b318(s)=0;}
    for(int i=0;i<10;++i){
        int course=edi;
        if(m!=4&&m!=5){g85b318(s)=ec==5?1:0;course=std::int32_t(std::uint16_t(ec));}
        list_add_51e140(L,{g85b317(s)?7:6,i,0,0,0,0,0,course,i,0,0,-1},s);
    }
}
// 4D9FC0(keep): rebuild; keep == 0 toggles the course set +334 and rebuilds the carousel.
void reload_4d9fc0(FrontendBoard50& c,S& s,bool keep){
    auto b=obj(c);auto L=list_of(c);
    b.put8(0x271e4,0);text_printf(b.sub(0x271e8,0x48c),"");
    const auto filter=b.u8(0x26459);b.put32(0x271e0,0);
    list_release_51c600(L,s);frontend_rankings_list_reset_51c440(L,1,true,s);
    if(!keep){b.put8(0x334,b.u8(0x334)?0:1);b.put8(0x26459,0);carousel_4d9750(c,s,true);}
    else b.put8(0x26459,filter);
    if(s.g.online_7d68bc){const auto f=b.i8(0x26459);if(f<7&&f!=1&&f!=2&&f!=3)fault(0x4da06d);}
    load_4d94a0(c,s);
    frontend_rankings_list_fill_51f280(L,b.u8(0x26459),b.u8(0x271e0),false,s);
}
// 4D9910(scroll, kind, top, cursor, saved list mode).
void restore_4d9910(FrontendBoard50& c,S& s,int a1,int a2,int a3,int a4,int a5){
    auto o=obj(c);auto L=list_of(c);auto& cur=s.g.cursor_6939f4;
    const bool on=s.g.online_7d68bc!=0;const std::int8_t f=L.i8(0x2611d);
    auto reset=[&]{o.put8(0x34,0);L.put8(0x2611c,0);cur=5;};
    auto accept=[&]{return f==3||((f==0)&&!on)||((mode(s)&0xe0)==0x80&&f>=7);};
    const int m=L.i32(0x2664c);int edi=a4,ebp=a3;bool done=false;
    if(std::uint8_t(a2)==0){
        if(on){reset();return;}
        if(!(f==3||f==0||((mode(s)&0xe0)==0x80&&f>=7)))return;
        const std::uint8_t focus=o.u8(0x34);if(!focus)return;
        if(a5==1){
            if(focus==1){edi=5;L.put8(0x2611c,1);done=true;}
            else if(focus==7){edi=0;o.put8(0x34,7);L.put8(0x2611c,2);done=true;}
        }
        if(std::uint8_t(a1)&&a5==1&&m==1)list_scroll(L,ebp,ebp,false,s);
    }else{
        if(on&&f!=3){reset();return;}
        if(!accept())return;
        const std::uint8_t focus=o.u8(0x34);if(!focus)return;
        if(a5==6){
            if(m==5&&a4==5){edi=4;o.put8(0x34,1);}
            if(m==1&&edi==5){edi=4;o.put8(0x34,2);}
        }else if(a5==1){
            if(m==6){ebp=0;if(focus==1)edi=5;else if(focus==7){edi=0;o.put8(0x34,6);}}
            if(m==1){const std::uint8_t now=o.u8(0x34);
                if(now==1){edi=5;L.put8(0x2611c,1);done=true;}
                else if(now==7){edi=0;o.put8(0x34,7);L.put8(0x2611c,2);done=true;}}
            if(m==5){const std::uint8_t now=o.u8(0x34);ebp=0;
                if(now==1)edi=4;else if(now==7){edi=0;o.put8(0x34,5);}}
            if(m==1)list_scroll(L,ebp,ebp,false,s);
        }
    }
    if(!done)row50(c,edi+ebp).put32(0x34,1);
    o.puti(0x271e0,ebp);cur=edi;
}
// Key 50 labels: Back, plus the OutRun2SP Time Attack filter button.
void labels50(FrontendBoard50& c,S& s){
    if(ghost_ta(s)){labels(s,{~0u,~0u,0x10,filter_text_48fb40(s,obj(c).i8(0x26459),true,false),~0u,~0u,8,0x296});return;}
    labels(s,{~0u,~0u,~0u,~0u,~0u,~0u,8,0x296});
}
// 4D9B90: Back-only labels while nothing is highlighted on the board.
void labels_4d9b90(FrontendBoard50& c,S& s){
    if(s.g.online_7d68bc)return;
    auto o=obj(c);bool any=false;
    for(int i=0;i<10;++i)if(row50(c,i).u32(0x34)){any=true;break;}
    if(o.u8(0x34)==0||any){if(o.u8(0x333))return;}
    else if(!o.u8(0x333))return;
    labels50(c,s);
}
}
bool frontend_board50_construct_4da2f0(FrontendBoard50& c,FrontendRankingServices& s){
    c.fault=0;c.constructed=false;c.glyphs.clear();c.images.clear();
    try{
        auto b=obj(c);auto* p=c.object.data();
        if(!title_base_construct_48f480(p,c.object.size(),s.repeat))fault(0x48f480);
        b.put32(0,0x5cd788);
        for(unsigned off:{0x3cu,0x150u,0x1f0u,0x290u})if(!title_ui_resource_construct_465160(p+off,0xa0))fault(0x465160);
        frontend_rankings_list_construct_51e080(list_of(c),s);
        for(unsigned off:{0x271e8u,0x27674u})if(!title_widget_construct_48e590(p+off,0x48c,s.repeat))fault(0x48e590);
        b.put32(8,0x35);g85b317(s)=s.g.ta_84b20d?1:0;
        b.put8(0x334,0);b.put8(0x332,0);b.put8(0x34,0);s.g.cursor_6939f4=5;
        c.constructed=true;return true;
    }catch(const Fault& f){c.fault=s.missing=f.pc;}catch(const std::out_of_range&){c.fault=s.missing=0x4da2f0;}
    return false;
}
bool frontend_board50_init_4d9d70(FrontendBoard50& c,FrontendRankingServices& s){
    try{
        if(!c.constructed)fault(0x4d9d70);
        auto b=obj(c);auto L=list_of(c);
        s.g.flag_84b20e=1;b.put8(0x333,0);b.put8(0x331,0);b.put8(0x330,0);clear_85b31c(s);b.put32(4,0x53);
        b.put8(0x332,0);b.put8(0x271e4,0);b.put32(0x27b04,0);b.putf(0x27b00,s.timer);
        board_text(b.sub(0x271e8,0x48c),342.f);board_text(b.sub(0x27674,0x48c),82.f);
        carousel_4d9750(c,s,false);
        b.put32(0x271e0,0);frontend_rankings_list_reset_51c440(L,1,true,s);
        b.put8(0x26459,!s.g.online_7d68bc&&ghost_ta(s)?7:0);
        if(s.g.online_7d68bc)fault(0x4d9ed6);
        g85b317(s)=0;g85b324(s)=0;g85b318(s)=0;
        for(int i=0;i<10;++i)list_add_51e140(L,{6,i,0,0,0,0,0,0,i,0,0,-1},s);
        frontend_rankings_list_fill_51f280(L,0,0,false,s);
        labels50(c,s);
        frontend_rankings_list_tick_51ec80(L,s);
        return true;
    }catch(const Fault& f){c.fault=s.missing=f.pc;}catch(const std::out_of_range&){c.fault=s.missing=0x4d9d70;}
    return false;
}
bool frontend_board50_control_4da3f0(FrontendBoard50& c,FrontendRankingServices& s,unsigned& result){
    result=0;
    try{
        if(!c.constructed)fault(0x4da3f0);
        auto b=obj(c);auto L=list_of(c);
        online(s,0x4da40d);
        if(b.u8(0x331))fault(0x4da4c7);            // online labels restore
        labels_4d9b90(c,s);
        auto cursor_res=b.sub(0x1f0,0xa0);
        if(!b.u8(0x34)){if(cursor_res.i32(8)==-1)configure(s,cursor_res,0x4400cc,1,1,4,0,0,0);}
        else if(cursor_res.i32(8)!=-1)release(s,cursor_res);
        tick(s,cursor_res);
        driving::object_store_depth_pair_442f20(s.root,0,0);
        const int max_index=s.g.ta_84b20d?0x29:0xb;
        if((mode(s)&0xe0)==0x80&&!s.g.ghost_84b0f6){
            auto set=b.sub(0x290,0xa0);
            if(set.i32(8)==-1){const unsigned f=b.u8(0x334)?1:0;configure(s,set,0x4400d9,f,f,4,0,0,0);}
            tick(s,set);
        }
        if(b.u8(0x332)){result=b.u32(0x338);return true;}
        auto api=carousel_api(s);auto w=b.sub(0x38,0x118);
        if(!frontend_carousel_tick_51be30(w,table50(c,s),api))carousel_fail(api);
        if(b.u8(0x271e4))fault(0x4da722);          // network ranking wait
        filter_label_4da180(c,s);
        const int cursor=s.g.cursor_6939f4,saved_mode=L.i32(0x2664c),top=b.i32(0x271e0);
        unsigned action{};
        if(!frontend_input_action_48f5f0(c.object.data(),c.object.size(),s.input,1,s.repeat,&s,
            [](void* p,unsigned key,int arg){auto& x=*static_cast<S*>(p);return x.ui.input_feedback(x.root,key,arg);},action))
            fault(s.ui.missing_pc?s.ui.missing_pc:0x48f5f0);
        auto row=[&](int i){return row50(c,i);};
        auto& cur=s.g.cursor_6939f4;
        auto move_course=[&](bool right){
            b.put32(0x271e0,0);list_release_51c600(L,s);frontend_rankings_list_reset_51c440(L,1,false,s);
            if(!frontend_carousel_move(w,table50(c,s),right,api))carousel_fail(api);
            if(s.g.online_7d68bc){const auto f=b.u8(0x26459);if(f==0||f==4||f==6||f==5)fault(0x4dae89);}
            b.put32(0x271e0,0);list_release_51c600(L,s);frontend_rankings_list_reset_51c440(L,1,true,s);
            load_4d94a0(c,s);frontend_rankings_list_fill_51f280(L,b.u8(0x26459),b.u8(0x271e0),false,s);
            restore_4d9910(c,s,0,1,top,cursor,saved_mode);
        };
        bool click=false;
        switch(action){
        case 0:{
            const std::uint8_t focus=b.u8(0x34);
            if(focus==1){
                if(b.i32(0x271e0)<5){list_scroll(L,b.i32(0x271e0)+5,5,false,s);b.puti(0x271e0,b.i32(0x271e0)+5);}
                b.put8(0x26458,1);
            }else if(focus==7){
                if(b.i32(0x271e0)>=5){list_scroll(L,b.i32(0x271e0)-5,5,true,s);b.puti(0x271e0,b.i32(0x271e0)-5);}
                b.put8(0x26458,2);
            }
            break;
        }
        case 1:
            s.g.flag_84b20e=0;b.put32(0x338,2);g85b317(s)=0;
            row(b.i32(0x271e0)+cur).put32(0x34,0);b.put8(0x332,1);break;
        case 2:{
            const std::int8_t focus=b.i8(0x34);
            if(focus==0){b.put8(0x34,1);b.put8(0x26458,1);labels_shifted(s,4,0x295);click=true;}
            else if(focus==1){b.put8(0x34,7);b.put8(0x26458,2);click=true;}
            else if(focus>=2&&focus<=5){int k=cur;b.put8(0x26458,0);
                if(k>0){row(b.i32(0x271e0)+k).put32(0x34,0);--k;cur=k;row(b.i32(0x271e0)+k).put32(0x34,1);}
                row(b.i32(0x271e0)+k).put32(0x34,1);b.put8(0x34,std::uint8_t(focus+1));click=true;}
            else if(focus==6){row(b.i32(0x271e0)+cur).put32(0x34,0);b.put8(0x34,7);b.put8(0x26458,2);click=true;}
            break;
        }
        case 4:{
            const std::int8_t focus=b.i8(0x34);
            if(focus==1){b.put8(0x34,0);b.put8(0x26458,0);labels_shifted(s,~0u,~0u);click=true;}
            else if(focus==2){b.put8(0x34,1);row(b.i32(0x271e0)+cur).put32(0x34,0);b.put8(0x26458,1);click=true;}
            else if(focus>=3&&focus<=6){int k=cur;b.put8(0x26458,0);
                if(k<4){row(b.i32(0x271e0)+k).put32(0x34,0);++k;cur=k;row(b.i32(0x271e0)+k).put32(0x34,1);}
                row(b.i32(0x271e0)+k).put32(0x34,1);b.put8(0x34,std::uint8_t(focus-1));click=true;}
            else if(focus==7){b.put8(0x34,1);b.put8(0x26458,1);click=true;}
            break;
        }
        case 3:if(b.i32(0xec)!=0)move_course(false);break;
        case 5:if(b.i32(0xec)!=max_index)move_course(true);break;
        case 6:case 8:{
            if(s.g.ghost_84b0f6==1&&(mode(s)&0xe0)==0x80){          // OutRun2SP filters 7..10
                const auto f=b.i8(0x26459);if(f>=7&&f<=10)toggle_85b31c(s);
                list_filter_51f460(L,b.u8(0x271e0),0,1,0,1,s);reload_4d9fc0(c,s,true);
                restore_4d9910(c,s,1,0,top,cursor,saved_mode);
            }else if(list_filter_51f460(L,b.u8(0x271e0),s.g.online_7d68bc,1,0,0,s))fault(0x4da0b0);
            restore_4d9910(c,s,0,0,top,cursor,saved_mode);
            labels50(c,s);
            break;
        }
        case 11:{
            const unsigned m=mode(s)&0xe0;
            if((m==0x80||m==0xa0)&&!s.g.ghost_84b0f6){
                release(s,b.sub(0x290,0xa0));reload_4d9fc0(c,s,false);restore_4d9910(c,s,1,0,top,cursor,saved_mode);
            }
            break;
        }
        default:break;
        }
        if(click)sound(s,1);
        frontend_rankings_list_tick_51ec80(L,s);
        return true;
    }catch(const Fault& f){c.fault=s.missing=f.pc;}catch(const std::out_of_range&){c.fault=s.missing=0x4da3f0;}
    return false;
}
bool frontend_board50_display_4d9430(FrontendBoard50& c,FrontendRankingServices& s){
    c.glyphs.clear();c.images.clear();
    try{
        if(!c.constructed)fault(0x4d9430);
        frontend_rankings_list_display_51edc0(list_of(c),c,s);
        text_display(s,obj(c).sub(0x271e8,0x48c),c);text_display(s,obj(c).sub(0x27674,0x48c),c);
        return true;
    }catch(const Fault& f){c.fault=s.missing=f.pc;}catch(const std::out_of_range&){c.fault=s.missing=0x4d9430;}
    return false;
}
bool frontend_board50_suspend_4d9460(FrontendBoard50& c,FrontendRankingServices& s){
    c.glyphs.clear();c.images.clear();
    try{
        if(!c.constructed)return true;
        auto b=obj(c);auto api=carousel_api(s);
        if(!frontend_carousel_release_51bc30(b.sub(0x38,0x118),api))carousel_fail(api);
        for(unsigned off:{0x290u,0x150u,0x1f0u})release(s,b.sub(off,0xa0));
        list_release_51c600(list_of(c),s);b.put8(0x332,0);
        return true;
    }catch(const Fault& f){c.fault=s.missing=f.pc;}catch(const std::out_of_range&){c.fault=s.missing=0x4d9460;}
    return false;
}
bool frontend_board50_destroy_4d9cb0(FrontendBoard50& c,FrontendRankingServices& s){
    c.glyphs.clear();c.images.clear();
    if(!c.constructed)return true;
    try{
        release_list_records(list_of(c),s);
        for(unsigned off:{0x290u,0x1f0u,0x150u,0x3cu})release(s,obj(c).sub(off,0xa0));
        c.constructed=false;return true;
    }catch(const Fault& f){c.fault=s.missing=f.pc;}catch(const std::out_of_range&){c.fault=s.missing=0x4d9cb0;}
    return false;
}
}
