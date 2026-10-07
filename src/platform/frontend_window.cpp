#include "frontend_window.hpp"
#include <algorithm>
#include <cmath>

namespace outrun::platform {
using driving::Bytes;
namespace {
bool integer(float f){return std::isfinite(f)&&double(f)>=-2147483648.&&double(f)<2147483648.;}
void position(Bytes b,unsigned off,float x,float y){b.putf(off+0x34,x);b.putf(off+0x38,y);}
void layout(Bytes b){
    const float x=b.f32(0x1280),y=b.f32(0x1284),w=float(b.i16(0x36)),h=float(b.i16(0x38));
    position(b,0x48,x+6,y+5);position(b,0x4d4,x+14,y+38);
    position(b,0x960,w+x-45,h+y-38);position(b,0xdec,x+45,h+y-38);
}
}
bool frontend_window_height_48cef0(Bytes b,const FrontendFontPack& fonts,FrontendTextLines& lines,std::uint8_t center,int extra){
    b.check(0,PcFrontendWindowBytes);b.put8(0x12a6,center);b.puti(0x12a8,extra);
    const int old=b.i16(0x38);int text_height{};
    if(!frontend_text_height_48ef90(b.sub(0x4d4,PcTextWidgetBytes),fonts,lines,text_height))return false;
    const auto padding=std::uint16_t(std::uint16_t(extra)+((b.u8(0x42)==1||b.u8(0x41)==1)?30:0));
    b.put16(0x38,std::uint16_t(unsigned(padding)+unsigned(text_height)+76));
    if(!center)b.put16(0x38,std::uint16_t(old));
    else if(old!=b.i16(0x38)){
        const float shift=float((old-b.i16(0x38))/2);
        b.putf(0x1284,b.f32(0x1284)+shift);b.putf(0x1290,b.f32(0x1290)+shift);layout(b);
    }
    const float bottom=(float(text_height)+b.f32(0x1284))+38;
    if(!integer(bottom))return false;
    b.puti(0x12ac,int(bottom));return true;
}
bool frontend_window_init_48d0c0(Bytes b,const FrontendFontPack& fonts,const FrontendTextTable& text,
    FrontendTextLines& lines,std::string_view title,std::string_view body,std::uint8_t flags,
    float x,float y,float width,float height,unsigned layer,std::uint8_t decoration){
    b.check(0,PcFrontendWindowBytes);width=std::max(width,116.f);
    // Reject out-of-range host casts without manufacturing valid dialog state.
    if(!integer(x)||!integer(y)||!integer(width)||!integer(height)||!integer(x+14)||!integer(y+38)||
       !integer(width-28)||!integer(height-76))return false;
    const auto* yes=text.get(0x1f1);const auto* no=text.get(0x1f2);if(!yes||!no)return false;
    b.put8(0x34,1);b.put8(0x40,1);for(unsigned o=0x1280;o<=0x12a0;o+=4)b.put32(o,0);
    b.putf(0x1280,x);b.putf(0x1284,y);b.putf(0x128c,x);b.putf(0x1290,y);b.put8(0x12a4,1);
    b.put16(0x36,std::uint16_t(int(width)));b.put16(0x38,std::uint16_t(unsigned(int(height))+((flags&7)?32:0)));b.put32(0x3c,layer);
    for(unsigned off:{0x48u,0x4d4u,0x960u,0xdecu})frontend_text_init_48e640(b.sub(off,PcTextWidgetBytes));
    if(!frontend_text_set_48f280(b.sub(0x48,PcTextWidgetBytes),title,9,~0u)||
       !frontend_text_set_48f280(b.sub(0x960,PcTextWidgetBytes),*no,9,~0u)||
       !frontend_text_set_48f280(b.sub(0xdec,PcTextWidgetBytes),*yes,9,~0u))return false;
    const int left=int(x+14),top=int(y+38);
    const std::int64_t right=std::int64_t(left)+int(width-28),bottom=std::int64_t(top)+int(height-76);
    if(right<INT32_MIN||right>INT32_MAX||bottom<INT32_MIN||bottom>INT32_MAX)return false;
    if(!frontend_text_box_48f1b0(b.sub(0x4d4,PcTextWidgetBytes),body,255,{left,top,int(right),int(bottom)},1,9,layer,0xff000000,1,1))return false;
    layout(b);position(b,0x4d4,float(left),float(top));
    for(unsigned off:{0x48u,0x4d4u,0x960u,0xdecu})b.put32(off+0x454,layer);
    b.put32(0xde0,2);b.put8(0x41,(flags&2)?1:0);b.put8(0x42,(flags&5)?1:0);
    b.put32(0x1278,1);b.put32(0x127c,(flags&5)?((flags&1)?5:9):5);b.put8(0x43,decoration);
    return frontend_window_height_48cef0(b,fonts,lines,0,0);
}
void frontend_window_suspend_48ca30(Bytes b){b.check(0,PcFrontendWindowBytes);b.put8(0x34,0);b.put8(0x41,0);b.put8(0x42,0);}
const std::vector<std::array<std::uint32_t,3>>& frontend_title_sprite_table(){
    static const std::vector<std::array<std::uint32_t,3>> table{
        {0x2c00f3,0,10},{0x2c00f3,10,10},{0x2c00f3,12,12},
        {0x2c00f6,0,10},{0x2c00f6,10,10},{0x2c00f6,12,12},
        {0x2c00f5,7,7},{0x2c00f5,8,8},{0x2c00f5,9,9},{0x2c00f5,10,10},
        {0x2c00f5,0,0},{0x2c00f5,1,1},{0x2c00f5,3,3},{0x2c00f5,4,4},{0x2c00f5,6,6},
        {0x2c00f4,3,3},{0x2c00f4,4,4},{0x2c00f4,5,5},{0x2c00f4,8,8},
        // 19/20 Controls (4D7780), 21/22 Audio (4D78C0) rows.
        {0x2c00f4,9,9},{0x2c00f4,10,10},{0x2c00f4,12,12},{0x2c00f4,13,13}};
    return table;
}
bool frontend_title_init_4d5e40(Bytes owner,FrontendChoiceList& list,const FrontendFontPack& fonts,
    const FrontendTextTable& text,FrontendTextLines& lines,TitleMenuGlobals& globals,
    const TitleOwnerGlobals& layers,const TitleMenuLayout& layout,unsigned& repeat){
    if(owner.size()<TitleOwnerPcSize||list.size()||!std::isfinite(layout.list_x)||!std::isfinite(layout.list_y))return false;
    const auto* yes=text.get(0x295);const auto* no=text.get(0x296);if(!yes||!no)return false;
    std::array<std::uint8_t,0x438> data{};Bytes format(data.data(),data.size());frontend_text_format_construct_48d570(format);
    format.put32(0x404,layers.scene_ids[2]);format.put32(0x408,0x20);format.put8(0x41c,0);
    format.putf(0x42c,-70);format.putf(0x430,-7);
    if(!list.initialize_48d970(0x5cbf10,frontend_title_sprite_table(),1,2,1,0,96,&format,1))return false;
    auto header=owner.sub(0x34,PcFrontendChoiceListBytes);header.putf(0x24,layout.list_x);header.putf(0x28,layout.list_y);
    header.puti(0x468,layout.indent);header.put32(0x2c,layers.scene_ids[1]);
    globals.delay_692c9c=globals.root_state==3?15.05f:0;
    const auto records=title_menu_records(globals);unsigned index{};
    for(unsigned i=0;i<records.entry_count;++i)if(!list.add_sprite_48e390(records.entries[i],repeat,index))return false;
    auto window=owner.sub(0x9bc,PcFrontendWindowBytes);
    if(!frontend_window_init_48d0c0(window,fonts,text,lines,"","",3,float(layout.window_x),float(layout.window_y),
        float(layout.width),float(layout.height),layers.scene_ids[0],0))return false;
    auto yes_widget=window.sub(0xdec,PcTextWidgetBytes),no_widget=window.sub(0x960,PcTextWidgetBytes);
    if(!frontend_text_set_48f280(yes_widget,*yes,yes_widget.u32(0x450),yes_widget.u32(0x474))||
       !frontend_text_set_48f280(no_widget,*no,no_widget.u32(0x450),no_widget.u32(0x474)))return false;
    // 48E200 visible count, then 48CAC0 adds its own 80 pixels. No layout
    // tick after this height write: original positions update next frame.
    const float height=float(records.entry_count)*header.f32(0x20)+24.f;
    window.put16(0x38,std::uint16_t(unsigned(int(height))+80));
    owner.put32(0x9ac,owner.u32(0x9ac)+1);return true;
}
bool frontend_window_display_48c5f0(Bytes b,const FrontendFontPack& fonts,FrontendTextLines& lines,
    std::vector<FrontendGlyph>& glyphs,std::vector<FrontendListImage>& images,std::vector<FrontendWindowIcon>& icons){
    b.check(0,PcFrontendWindowBytes);if(!b.u8(0x34))return true;
    if(!integer(b.f32(0x1280))||!integer(b.f32(0x1284)))return false;
    const int x=int(b.f32(0x1280)),y=int(b.f32(0x1284)),w=b.i16(0x36),h=b.i16(0x38);
    const float layer=float(b.i32(0x3c));
    auto image=[&](unsigned mode,unsigned token,int px,int py,float width,float height){
        images.push_back({0x42d300,mode,token,~0u,px,py,0,width,height,layer});};
    image(3,0x3004d,x+3,y+26,float(w-9),float(h-42));
    image(1,0x3004c,x+3,y+32,float(w-9),6);
    if(b.u8(0x42)==1||b.u8(0x41)==1)image(1,0x30040,x+3,y+h-46,float(w-9),30);
    if(b.u8(0x43)==1){image(1,0x30040,x+3,y+26,float(w-9),30);image(1,0x30040,x+3,y+h-46,float(w-9),30);}
    image(1,0x30043,x+9,y,float(w-115),26);image(1,0x30046,x+9,y+h-16,float(w-20),16);
    image(0,0x30042,x,y,9,26);image(0,0x30041,x+w-106,y,106,26);
    image(0,0x30045,x,y+h-16,9,16);image(0,0x30044,x+w-11,y+h-16,11,16);
    image(2,0x3004f,x,y+26,3,float(h-42));image(2,0x3004e,x+w-6,y+26,6,float(h-42));
    auto draw=[&](unsigned off){return frontend_text_display_48f3c0(b.sub(off,PcTextWidgetBytes),fonts,lines,glyphs);};
    if(!draw(0x48)||!draw(0x4d4))return false;
    // Button token table and icon coordinates are checked against 48C5F0.
    if(b.u8(0x41)){
        if(!draw(0x960))return false;
        icons.push_back({0x2c013a,(float(w)+b.f32(0x1280)-27)-320,(float(h)+b.f32(0x1284)-30)-240,b.i32(0x3c)+1,25});
    }
    if(b.u8(0x42)){
        if(!draw(0xdec))return false;
        const auto index=b.u32(0x127c);if(index>=12)return false;
        // Filled from the retail button table rather than a diagnostic icon.
        const unsigned token=index<4?0x2c013a:index<8?0x2c013b:0x2c0139;
        icons.push_back({token,(b.f32(0x1280)+27)-320,(float(h)+b.f32(0x1284)-30)-240,b.i32(0x3c)+1,25});
    }return true;
}
}
