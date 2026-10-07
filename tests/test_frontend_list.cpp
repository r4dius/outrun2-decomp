#include "platform/frontend_list.hpp"
#include "platform/title_owner.hpp"
#include <cmath>
#include <iostream>
#include <stdexcept>
using namespace outrun::platform;
using outrun::driving::Bytes;
static void require(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
int main(int argc,char** argv){try{
    require(argc==2,"font pack argument");FrontendFontPack fonts;require(load_frontend_font_pack(argv[1],fonts),"retail fonts");
    FrontendSprites sprites;require(sprites.bind_timing(0x44,std::vector<FrontendSpriteTiming>(224,{60,60})),"animation fixture timing");
    FrontendUiResources ui{sprites};std::array<std::uint8_t,PcFrontendListBytes> object;object.fill(0xa5);Bytes b(object.data(),object.size());
    FrontendList list(b,ui,fonts.fonts[9]);require(b.i32(0)==-1&&b.f32(0x1c)==16&&b.u8(0x16)==0xa5,"constructor preservation");
    require(list.initialize_4ecfb0(0x5cdc20,{{0x440001,0,2}},300,1,0,0),"list init");
    unsigned index{};const std::string_view labels[]{"LOAD","EDIT LICENSE","DELETE","CREATE NEW LICENSE"};
    for(auto& label:labels)require(list.add_text_4ed160(&label,0,index),"append text");
    require(index==3&&b.i32(0)==0&&list.height_count_4ed810()==4,"rows and selection");
    require(list.set_enabled_4ed300(0,true)&&b.i32(0)==1,"hide selected moves to immediate next");
    require(list.set_enabled_4ed300(2)&&list.set_enabled_4ed300(1)&&b.i32(0)==1,"disabled next is not skipped");
    bool sound{};require(list.move(true,sound)&&sound&&b.i32(0)==3,"skip disabled nodes");
    require(list.move(true,sound)&&sound&&b.i32(0)==3,"same selection still emits navigation sound");
    require(list.set_enabled_4ed300(3)&&!list.move(true,sound)&&!sound,"all disabled list is rejected without hanging");
    list.reset_selection_4ed8d0();require(b.i32(0)==3,"empty selectable set preserves selection");
    require(!list.show_4ed390(9)&&!list.set_enabled_4ed300(9),"invalid row rejected");
    require(list.show_4ed390(0),"show");list.reset_selection_4ed8d0();require(b.i32(0)==0,"reset selects first visible enabled row");
    require(list.select_4ed930(99)&&!list.select_4ed930(0),"PC setter validates current not requested selection");
    b.puti(0,0);b.putf(0x18,80);b.putf(0x20,200);b.putf(0x24,100);b.putf(0x1c,17);b.put32(0x28,5);
    std::vector<FrontendGlyph> glyphs;std::vector<FrontendListImage> images;
    for(unsigned frame=0;frame<300;++frame){glyphs.clear();images.clear();require(list.display_4ed3e0(float(frame),glyphs,images),"display/scroll");
        require(!glyphs.empty()&&images.size()==12,"all visible rows, including disabled rows, draw");
        require(images[0].token==0x30048&&images[0].color==0xffffce0a&&images[0].width==14,"original selected highlight");
        for(const auto& g:glyphs)require(g.x>=0&&g.x<640&&g.mode==6,"glyph layout/layer bounded");}
    require(list.row(3).i32(0xe8+0xc)<0,"long text scrolls independently");
    b.put8(0x30,1);b.put32(0x38,2);b.put32(0,3);b.put32(0x34,0);b.put8(0x15,1);
    glyphs.clear();images.clear();require(list.display_4ed3e0(10,glyphs,images)&&b.i32(0x34)==1,"scroll advances only one row per draw");
    require(images.front().pc==0x42d280&&images.front().frame==0&&images.back().frame==2,"both scroll arrows");
    b.put8(0x12,1);glyphs.clear();images.clear();require(list.display_4ed3e0(10,glyphs,images)&&glyphs.empty()&&images.empty(),"hidden list suppresses all draws");
    require(!list.display_4ed3e0(NAN,glyphs,images),"invalid timer rejected");
    const auto selection=b.u32(0);require(list.clear_4eda60()&&list.size()==0&&b.u32(4)==0&&b.u32(0)==selection,"clear preserves selection");
    require(list.add_sprite_4ed9b0(0,3,index),"sprite row");b.put8(0x12,0);b.put8(0x30,0);b.put8(0x10,0);
    require(list.display_4ed3e0(0,glyphs,images)&&sprites.used(6)==1,"original sprite row committed");
    require(list.clear_4eda60()&&sprites.used(6)==0,"clear releases native sprite handle");
    require(!list.add_sprite_4ed9b0(9,0,index),"out-of-table sprite rejected");
    std::array<std::uint8_t,0x12b0> window{};Bytes wb(window.data(),window.size());
    require(title_controller_rect_48d4e0(window.data(),window.size(),100,110,10,190)&&wb.i16(0x36)==116,"original minimum window width");
    require(title_controller_move_48cb00(window.data(),window.size(),120,170,2),"window slide");
    require(title_controller_motion_48cc00(window.data(),window.size(),0.5f)&&wb.f32(0x1280)==105&&wb.f32(0x1284)==125,"owner delta drives window motion");
    require(wb.f32(0x7c)==111&&wb.f32(0x80)==130,"title text follows window");
    require(title_controller_motion_48cc00(window.data(),window.size(),2)&&wb.f32(0x1280)==120&&wb.f32(0x1284)==170&&!wb.u8(0x12a4),"window snap at original target");
    require(!title_controller_motion_48cc00(window.data(),window.size(),NAN),"invalid owner timing fails");
    require(!title_controller_rect_48d4e0(window.data(),window.size(),0,0,INFINITY,100),"invalid geometry fails");
    std::cout<<"Original list navigation, scroll text, raw image composition and sprite lifetime passed\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
