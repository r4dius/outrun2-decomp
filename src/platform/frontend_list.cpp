#include "frontend_list.hpp"
#include "title_owner.hpp"
#include <algorithm>
#include <cmath>
#include <limits>

namespace outrun::platform {
using driving::Bytes;
void frontend_text_format_construct_48d570(Bytes b){
    b.check(0,0x438);b.put8(0,0);b.put32(0x400,9);b.put32(0x404,4);b.put32(0x408,0x400);
    b.put32(0x40c,0);b.put32(0x410,0);b.put32(0x414,480);b.put32(0x418,640);
    b.put8(0x41c,1);b.put32(0x420,0xff3f474a);
    for(unsigned i=0x424;i<=0x430;i+=4)b.putf(i,1);b.put32(0x434,1);
}
FrontendChoiceList::FrontendChoiceList(Bytes b,FrontendUiResources& ui,const FrontendFontPack& f):b_(b),ui_(ui),fonts_(f){
    b_.check(0,PcFrontendChoiceListBytes);frontend_text_format_construct_48d570(b_.sub(0x30,0x438));
    for(unsigned i:{0u,0x10u,0x14u})b_.put32(i,~0u);
    for(unsigned i:{4u,8u,12u,0x468u,0x470u})b_.put32(i,0);
    for(unsigned i:{0x18u,0x1au,0x1bu,0x1cu,0x1du,0x46cu})b_.put8(i,0);
    b_.put8(0x19,1);b_.putf(0x20,16);b_.putf(0x24,0);b_.putf(0x28,0);b_.put32(0x2c,12);b_.put32(0x474,96);
}
FrontendChoiceList::~FrontendChoiceList(){clear_48e440();}
bool FrontendChoiceList::initialize_48d970(unsigned id,const std::vector<std::array<unsigned,3>>& table,
    std::uint8_t vertical,int selected,int ordinary,std::uint8_t scroll,int limit,const Bytes* format,std::uint8_t text_enabled){
    if(!rows_.empty()||limit<0||(format&&format->size()<0x438))return false;
    table_=table;b_.put32(0xc,id);b_.put32(4,0);b_.put32(0,~0u);b_.puti(0x10,selected);b_.puti(0x14,ordinary);
    b_.put8(0x18,selected!=-1&&ordinary!=-1);b_.put8(0x19,vertical);b_.put8(0x46c,scroll);
    b_.putf(0x20,16);b_.put8(0x1a,0);b_.put8(0x1b,0);b_.putf(0x24,0);b_.putf(0x28,0);b_.put32(0x2c,12);
    b_.put32(0x468,0);b_.put32(0x470,0);b_.puti(0x474,limit);b_.put32(8,0);b_.put8(0x1c,0);b_.put8(0x1d,0);
    if(format){b_.put8(0x1c,text_enabled);for(unsigned i=0;i<0x438;++i)b_.put8(0x30+i,format->u8(i));}return true;
}
bool FrontendChoiceList::add_text_48db30(std::string_view text,unsigned& repeat,unsigned& index){
    text=text.substr(0,text.find('\0'));if(text.size()>=0x400||rows_.size()>=4096||b_.u32(4)!=rows_.size())return false;
    rows_.emplace_back();auto r=row(rows_.size()-1);unsigned ignored{};
    for(unsigned o:{0x10u,0xb0u})if(!ui_.call(0x465160,r.sub(o,0xa0),nullptr,0,ignored)){rows_.pop_back();return false;}
    auto& storage=rows_.back();if(!title_widget_construct_48e590(storage.data()+0x150,PcTextWidgetBytes,repeat)){rows_.pop_back();return false;}
    auto w=r.sub(0x150,PcTextWidgetBytes);frontend_text_init_48e640(w);auto fmt=b_.sub(0x30,0x438);
    // Original 48F2D0 descriptor copy, followed by 48EE80 literal row text.
    w.put32(0x45c,fmt.u32(0x408));for(unsigned i=0;i<4;++i)w.put32(0x460+4*i,fmt.u32(0x40c+4*i));
    if(w.i32(0x468)>w.i32(0x460)&&w.i32(0x46c)>w.i32(0x464))w.put8(0x4d,1);
    w.put32(0x450,fmt.u32(0x400));w.put32(0x474,fmt.u32(0x420));w.put32(0x454,fmt.u32(0x404));
    w.put32(0x478,fmt.u32(0x424));w.put32(0x47c,fmt.u32(0x428));w.put8(0x470,fmt.u8(0x41c));
    w.put32(0x34,fmt.u32(0x42c));w.put32(0x38,fmt.u32(0x430));w.put32(0x480,fmt.u32(0x434));
    if(!frontend_text_set_48f280(w,text,w.u32(0x450),w.u32(0x474))){rows_.pop_back();return false;}
    index=b_.u32(4);r.put32(0,index);r.put32(4,1);r.put32(8,1);r.put32(12,~0u);r.put32(0x5dc,0);
    b_.put32(4,index+1);if(!index)b_.put32(0,0);return true;
}
bool FrontendChoiceList::clear_48e440(){
    for(unsigned i=0;i<rows_.size();++i)for(unsigned off:{0x10u,0xb0u}){unsigned result{};
        if(!ui_.call(0x465250,row(i).sub(off,0xa0),nullptr,0,result))return false;}
    rows_.clear();b_.put32(8,0);return true; // PC preserves selection and count.
}
bool FrontendChoiceList::add_sprite_48e390(unsigned table_index,unsigned& repeat,unsigned& index){
    if(table_index>=table_.size()||rows_.size()>=4096||b_.u32(4)!=rows_.size())return false;
    rows_.emplace_back();auto r=row(rows_.size()-1);unsigned ignored{};
    for(unsigned off:{0x10u,0xb0u})if(!ui_.call(0x465160,r.sub(off,0xa0),nullptr,0,ignored)){rows_.pop_back();return false;}
    if(!title_widget_construct_48e590(rows_.back().data()+0x150,PcTextWidgetBytes,repeat)){rows_.pop_back();return false;}
    index=b_.u32(4);r.put32(0,index);r.put32(4,1);r.put32(8,1);r.put32(12,table_index);r.put32(0x5dc,0);
    b_.put32(4,index+1);if(!index)b_.put32(0,0);return true;
}
bool FrontendChoiceList::move(bool forward,bool& sound){
    sound=false;const int count=int(rows_.size());int selected=b_.i32(0);
    if(!count||count!=b_.i32(4)||selected<0||selected>=count)return false;
    for(int tries=0;tries<count;++tries){selected+=forward?1:-1;if(selected<0)selected=count-1;else if(selected>=count)selected=0;
        if(row(unsigned(selected)).u32(8)){b_.puti(0,selected);sound=true;return true;}}
    return false; // Invalid all-disabled state would loop forever on PC.
}
void FrontendChoiceList::reset_selection_48e2b0(){for(unsigned i=0;i<rows_.size();++i)if(row(i).u32(4)&&row(i).u32(8)){b_.put32(0,i);break;}}
bool FrontendChoiceList::display_48dda0(float timer,FrontendTextLines& lines,std::vector<FrontendGlyph>& glyphs){
    if(!std::isfinite(timer)||!std::isfinite(b_.f32(0x24))||!std::isfinite(b_.f32(0x28))||!std::isfinite(b_.f32(0x20)))return false;
    // Arrow-enabled variants require their original immediate-image path.
    // Confirmation windows initialize this byte to zero; never fake success
    // for a caller that requests an as-yet-unimplemented variant.
    if(b_.u8(0x1d))return false;
    if(b_.u8(0x46c)){auto start=b_.i32(0x470);const auto selected=b_.i32(0);
        if(start+b_.i32(0x474)<=selected){if(selected<b_.i32(4))++start;}
        else if(selected<start)--start;else if(start<0)start=0;b_.puti(0x470,start);
    }else{b_.put32(0x474,96);b_.put32(0x470,0);}
    float x=b_.f32(0x24),y=b_.f32(0x28);
    auto sprite=[&](Bytes resource,unsigned table_index,float px,float py,unsigned layer,int fixed=-1){
        if(table_index>=table_.size())return false;const auto& t=table_[table_index];unsigned args[11]={t[0],t[1],t[2],layer,1,0,0,0x3f800000,0x3f800000,0x3f800000,0};
        if(fixed>=0)args[1]=args[2]=unsigned(fixed);
        std::memcpy(args+5,&px,4);std::memcpy(args+6,&py,4);unsigned result{};
        return ui_.call(0x465860,resource,args,11,result)&&ui_.call(0x465970,resource,nullptr,0,result);
    };
    for(unsigned i=0;i<rows_.size();++i){auto r=row(i);unsigned result{};
        for(unsigned off:{0x10u,0xb0u})if(!ui_.call(0x465250,r.sub(off,0xa0),nullptr,0,result))return false;
        if(int(i)<b_.i32(0x470)||int(i)>=b_.i32(0x470)+b_.i32(0x474))continue;
        if(!r.u32(4)||b_.u8(0x1a))continue;
        if(b_.u8(0x18)){
            if(r.u32(12)==~0u&&b_.u8(0x1c)){
                auto w=r.sub(0x150,PcTextWidgetBytes);
                w.putf(0x34,(b_.f32(0x45c)+320.f)+x-float(b_.i32(0x468)));
                w.putf(0x38,(b_.f32(0x460)+240.f)+y);w.put32(0x454,b_.u32(0x2c)+1);
                if(!frontend_text_display_48f3c0(w,fonts_,lines,glyphs))return false;
            }else if(!sprite(r.sub(0x10,0xa0),r.u32(12),x-float(b_.i32(0x468)),y,b_.u32(0x2c)+1))return false;
            const auto selected=int(i)==b_.i32(0)&&!b_.u8(0x1b)?b_.u32(0x10):b_.u32(0x14);
            if(!sprite(r.sub(0xb0,0xa0),selected,x,y,b_.u32(0x2c)))return false;
        }else if(!sprite(r.sub(0x10,0xa0),r.u32(12),x,y,b_.u32(0x2c)+1,int(i)==b_.i32(0)?2:1))return false;
        if(b_.u8(0x19))y+=b_.f32(0x20);else x+=b_.f32(0x20);
    }return true;
}
namespace {
int trunc32(float f){return std::isfinite(f)&&double(f)>=-2147483648.&&double(f)<2147483648.?int(f):std::numeric_limits<int>::min();}
std::uint32_t bits(float f){std::uint32_t v;std::memcpy(&v,&f,4);return v;}
std::string value(Bytes b){std::string s;for(unsigned i=0;i<100&&b.u8(0x24+i);++i)s.push_back(char(b.u8(0x24+i)));return s;}
void list_init(Bytes b,unsigned table,float width,std::uint8_t vertical,std::uint8_t scroll,int limit){
    b.puti(0,-1);b.put32(4,0);b.put32(8,0);b.put32(0xc,table);
    b.put8(0x10,1);b.put8(0x11,vertical);for(unsigned off=0x12;off<=0x15;++off)b.put8(off,0);
    b.putf(0x18,width);b.putf(0x1c,16);b.putf(0x20,0);b.putf(0x24,0);
    b.put32(0x28,12);b.put32(0x2c,0);b.put8(0x30,scroll);b.put32(0x34,0);b.puti(0x38,limit);
}
}
void frontend_scroll_text_construct_4edc80(Bytes b){
    b.check(0,0x8c);for(unsigned off=0;off<0x88;off+=4)b.put32(off,off==0x14?640:0);
    b.put32(0x88,~0u);
}
bool frontend_scroll_text_init_4edaf0(Bytes b,const FrontendFont& font,std::string_view text,int x,int y,int width,unsigned color){
    if(b.size()<0x8c)return false;
    text=text.substr(0,text.find('\0'));FrontendTextStyle style;style.flags=0x100;
    b.put32(0,14);b.puti(4,x);b.puti(8,y);b.put32(0xc,0);b.put32(0x1c,0);b.put32(0x20,0);
    b.puti(0x10,frontend_text_width_42c480(font,style,text));b.puti(0x14,width);b.put32(0x18,unsigned(text.size()));
    // Original strncpy(...,100), followed by byte 99 = 0.
    for(unsigned i=0;i<100;++i)b.put8(0x24+i,i<text.size()?std::uint8_t(text[i]):0);
    b.put8(0x87,0);b.put32(0x88,color);return true;
}
bool frontend_scroll_text_set_4edcf0(Bytes b,const FrontendFont& font,std::string_view text){
    if(b.size()<0x8c)return false;
    text=text.substr(0,text.find('\0'));FrontendTextStyle style;style.flags=0x100;
    b.puti(0x10,frontend_text_width_42c480(font,style,text));b.put32(0x18,unsigned(text.size()));
    b.put32(0x20,0);b.put32(0xc,0);
    for(unsigned i=0;i<100;++i)b.put8(0x24+i,i<text.size()?std::uint8_t(text[i]):0);
    b.put8(0x87,0);return true;
}
void frontend_scroll_text_move_4edcc0(Bytes b,int x,int y){b.puti(4,x);b.puti(8,y);}
void frontend_scroll_text_layer_4f1ce0(Bytes b,std::uint32_t layer){b.put32(0,layer);}
bool frontend_scroll_text_draw_4edb90(Bytes b,const FrontendFont& font,std::vector<FrontendGlyph>& glyphs){
    if(b.size()<0x8c)return false;
    FrontendTextStyle style;style.mode=b.u32(0);style.color=b.u32(0x88);style.flags=1;
    auto draw=[&](int x){FrontendTextCursor cursor{std::int16_t(x),std::int16_t(x),std::int16_t(b.i32(8))};
        return frontend_text_draw(font,style,cursor,value(b),glyphs);};
    if(b.i32(0x10)<=b.i32(0x14))return draw(b.i32(4));
    const int gap=std::min(32,b.i32(0x14)),length=b.i32(0x10);
    int shift=b.i32(0xc)-1;if(shift<=-length)shift+=length+gap;b.puti(0xc,shift);
    style.clip_left=b.i32(4);style.clip_right=b.i32(4)+b.i32(0x14);
    const int x=b.i32(4)+shift;
    return draw(x)&&draw(x+length+gap);
}
FrontendList::FrontendList(Bytes b,FrontendUiResources& ui,const FrontendFont& font):b_(b),ui_(ui),font_(font){
    b_.check(0,PcFrontendListBytes);list_init(b_,0,0,0,0,0);
}
FrontendList::~FrontendList(){clear_4eda60(true);}
bool FrontendList::initialize_4ecfb0(unsigned id,const std::vector<std::array<unsigned,3>>& table,float width,std::uint8_t vertical,std::uint8_t scroll,int limit){
    if(!std::isfinite(width)||limit<0||!clear_4eda60())return false;
    table_=table;list_init(b_,id,width,vertical,scroll,limit);return true;
}
bool FrontendList::append(unsigned token,int indent,const std::string_view* text,unsigned& index){
    if(rows_.size()>=4096||b_.u32(4)!=rows_.size())return false;
    rows_.emplace_back();auto r=row(rows_.size()-1);unsigned result{};
    if(!ui_.call(0x465160,r.sub(0x14,0xa0),nullptr,0,result)){rows_.pop_back();return false;}
    // 4ED100 calls 570AC0 for text, which is an empty constructor, not 4EDC80.
    r.put8(0xb4+0x30,0);r.put8(0xb4+0x31,0);
    index=b_.u32(4);r.put32(0,index);r.puti(4,indent);r.put32(8,1);r.put32(0xc,1);r.put32(0x10,token);
    if(text&&!frontend_scroll_text_init_4edaf0(r.sub(0xe8,0x8c),font_,*text,trunc32(b_.f32(0x20)),
        trunc32(float(index)*b_.f32(0x1c)+b_.f32(0x20)),trunc32(b_.f32(0x18)),0xff3f474a))return false;
    b_.put32(4,index+1);if(index==0)b_.put32(0,0);return true;
}
bool FrontendList::add_text_4ed160(const std::string_view* text,int indent,unsigned& index){return append(~0u,indent,text,index);}
bool FrontendList::add_sprite_4ed9b0(unsigned token,int indent,unsigned& index){
    if(token>=table_.size())return false;
    return append(token,indent,nullptr,index);
}
bool FrontendList::clear_4eda60(bool destructor){
    for(unsigned i=0;i<rows_.size();++i){unsigned result{};if(!ui_.call(0x465250,row(i).sub(0x14,0xa0),nullptr,0,result))return false;}
    rows_.clear();b_.put32(8,0);if(!destructor)b_.put32(4,0);return true;
}
bool FrontendList::move(bool forward,bool& sound){
    sound=false;const int count=int(rows_.size());int current=b_.i32(0);
    if(count==0||b_.i32(4)!=count||current<0||current>=count)return false;
    for(int tries=0;tries<count;++tries){current+=forward?1:-1;if(current<0)current=count-1;else if(current>=count)current=0;
        if(row(current).u32(0xc)){b_.puti(0,current);sound=count>1;return true;}}
    return false; // Original loops forever with no enabled node; report invalid data.
}
bool FrontendList::set_enabled_4ed300(unsigned index,bool hide){
    if(index>=rows_.size())return false;
    auto r=row(index);r.put32(0xc,0);
    // The PC inspects only the immediately following node. No wrap/search.
    if(b_.u32(0)==index&&index+1<rows_.size()&&row(index+1).u32(0xc))b_.put32(0,index+1);
    if(hide)r.put32(8,0);
    return true;
}
bool FrontendList::show_4ed390(unsigned index){if(index>=rows_.size())return false;auto r=row(index);r.put32(8,1);r.put32(0xc,1);return true;}
void FrontendList::reset_selection_4ed8d0(){for(unsigned i=0;i<rows_.size();++i)if(row(i).u32(8)&&row(i).u32(0xc)){b_.put32(0,i);break;}}
bool FrontendList::select_4ed930(int index){if(b_.i32(0)<0||b_.i32(0)>=b_.i32(4))return false;b_.puti(0,index);return true;}
int FrontendList::visible_index_4ed7c0()const{
    int hidden=0;for(int i=0;i<int(rows_.size())&&i<b_.i32(0);++i){const auto& r=rows_[i];if(!(r[8]||r[9]||r[10]||r[11]))++hidden;}
    return b_.i32(0)-hidden;
}
int FrontendList::height_count_4ed810()const{
    int visible=0;for(const auto& r:rows_)if(r[8]||r[9]||r[10]||r[11])++visible;
    return b_.u8(0x30)?std::min(b_.i32(0x38),visible+1):visible;
}
bool FrontendList::setter(unsigned pc,const unsigned* a,std::size_t n){
    if(!a)return false;
    if(n==1&&(pc==0x4ed880||pc==0x4ed890||pc==0x4ed8c0)){b_.put32(pc==0x4ed880?0x1c:pc==0x4ed890?0x18:0x28,a[0]);return true;}
    if(n==2&&pc==0x4ed8a0){b_.put32(0x20,a[0]);b_.put32(0x24,a[1]);return true;}return false;
}
bool FrontendList::display_4ed3e0(float timer,std::vector<FrontendGlyph>& glyphs,std::vector<FrontendListImage>& images){
    if(b_.u32(4)!=rows_.size()||!std::isfinite(timer))return false;
    float x=b_.f32(0x20),y=b_.f32(0x24);int top=b_.i32(0x34),limit=b_.i32(0x38),selected=b_.i32(0);
    if(b_.u8(0x30)){
        if(std::int64_t(top)+limit<=selected){if(selected<b_.i32(4))++top;}
        else if(selected<top)--top;else if(top<0)top=0;
    }else{top=0;limit=96;b_.puti(0x38,limit);}b_.puti(0x34,top);
    const auto ticks=std::uint32_t(trunc32(timer))*4u;std::int32_t signed_ticks;std::memcpy(&signed_ticks,&ticks,4);
    const unsigned alpha=unsigned(std::abs(signed_ticks%256-128)+127),color=(alpha<<24)|0xffffff;
    auto arrow=[&](int frame,float py){images.push_back({0x42d280,0,0x3004b,color,trunc32(b_.f32(0x20)-10),trunc32(py),frame,0,0,float(b_.i32(0x28))+1});};
    if(b_.u8(0x15)&&top!=0&&!b_.u8(0x12))arrow(0,y+2);
    int visible=0,hidden=0;
    for(unsigned i=0;i<rows_.size();++i){auto r=row(i);unsigned result{};
        if(!ui_.call(0x465250,r.sub(0x14,0xa0),nullptr,0,result))return false;
        if(!r.u32(8)||b_.u8(0x12)){++hidden;continue;}
        if(int(i)<top||std::int64_t(i)>=std::int64_t(top)+limit)continue;
        ++visible;const float indent=float(r.i32(4));
        auto sprite=[&](bool highlighted){const auto index=r.u32(0x10);if(index>=table_.size())return false;
            const auto& t=table_[index];const unsigned frame=i==b_.u32(0)?2:1;
            const unsigned args[]={t[0],highlighted?t[1]:frame,highlighted?t[2]:frame,b_.u32(0x28)+1,1,
                bits(highlighted?b_.f32(0x18)*0.5f+x-320-float(b_.i32(0x2c))+indent:x+indent),
                bits(highlighted?y+7-240:y),bits(1),bits(1),bits(1),0};
            return ui_.call(0x465860,r.sub(0x14,0xa0),args,11,result)&&ui_.call(0x465970,r.sub(0x14,0xa0),nullptr,0,result);};
        if(b_.u8(0x10)){
            if(r.u32(0x10)==~0u){auto t=r.sub(0xe8,0x8c);t.puti(4,trunc32(indent+x+7));t.puti(8,trunc32(y));
                t.put32(0,b_.u32(0x28)+1);t.puti(0x14,trunc32(b_.f32(0x18)-14));
                if(!frontend_scroll_text_draw_4edb90(t,font_,glyphs))return false;
            }else if(!sprite(true))return false;
            // 51F580 / 51F6A0 / 51F5B0: three original image pieces, not a
            // replacement rectangle. Their width argument is a pixel extent.
            auto h=r.sub(0xb4,0x34);h.putf(0,float(trunc32(x)+r.i32(4)));h.putf(4,float(trunc32(y)));h.putf(8,0);
            h.puti(0x28,trunc32(b_.f32(0x18)-indent));h.put32(0x2c,b_.u32(0x28));h.put8(0x30,1);h.put8(0x31,i==b_.u32(0)&&!b_.u8(0x13));
            const unsigned c=h.u8(0x31)?0xffffce0a:0xff8b9091;const float layer=float(h.i32(0x2c));
            images.push_back({0x42d300,0,0x30048,c,trunc32(h.f32(0)),trunc32(h.f32(4)),0,14,15,layer});
            images.push_back({0x42d300,1,0x30049,c,trunc32(h.f32(0)+14),trunc32(h.f32(4)),0,float(h.i32(0x28)-28),15,layer});
            images.push_back({0x42d300,0,0x30047,c,trunc32(float(h.i32(0x28)-28)+h.f32(0)+14),trunc32(h.f32(4)),0,14,15,layer});
        }else if(!sprite(false))return false;
        if(b_.u8(0x11))y+=b_.f32(0x1c);else x+=b_.f32(0x1c);
    }
    if(b_.u8(0x15)&&int(rows_.size())-hidden>top+visible&&!b_.u8(0x12))arrow(2,y);
    return true;
}
}
