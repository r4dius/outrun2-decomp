#include "platform/frontend_text.hpp"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
using namespace outrun::platform;
using outrun::driving::Bytes;
#define CHECK(x) do {if(!(x)){std::fprintf(stderr,"line %d: %s\n",__LINE__,#x);std::exit(1);}}while(0)
int main(int argc,char** argv){
    std::array<std::uint8_t,PcTextWidgetBytes> object;object.fill(0xa5);Bytes b(object.data(),object.size());
    frontend_text_init_48e640(b);CHECK(b.u32(0)==0xa5a5a5a5);CHECK(b.u32(0x34)==0xa5a5a5a5);CHECK(b.u32(0x450)==9);
    CHECK(frontend_text_set_48f280(b,"CREATE NEW LICENSE",9,~0u));CHECK(b.u32(0x458)==18);
    CHECK(frontend_text_value_48eec0(b)=="CREATE NEW LICENSE");
    CHECK(!frontend_text_set_48f280(b,std::string(1024,'x'),9,~0u));CHECK(b.u32(0x458)==18);
    unsigned rect[]{0,0,100,40};CHECK(frontend_text_setter(0x48eee0,b,rect,4));CHECK(b.u8(0x4d)==1);
    unsigned invalid[]{100,0,0,0};CHECK(frontend_text_setter(0x48eee0,b,invalid,4));CHECK(b.u8(0x4d)==1);
    CHECK(!frontend_text_setter(0x48ef30,b,nullptr,1));CHECK(!frontend_text_setter(0x999999,b,nullptr,0));
    FrontendTextLines lines; b.put32(0x45c,12);
    CHECK(frontend_text_split_48ea50(b,"ONE TWO THREE FOUR",lines));CHECK(std::string(lines.lines[0].data())=="ONE TWO");
    CHECK(frontend_text_split_48ea50(b,"ONE<newline>TWO",lines));CHECK(std::string(lines.lines[1].data())=="TWO");
    FrontendTextTable table;std::array<std::uint8_t,26> raw{};Bytes rb(raw.data(),raw.size());
    rb.put32(0,0x74657874);rb.put32(4,26);rb.put32(8,16);rb.put16(16,0x41);rb.put16(18,0x1e9);
    CHECK(table.parse(raw.data(),raw.size()));CHECK(table.size()==1);CHECK(*table.get(0)==std::string("A\xe9"));CHECK(!table.get(1));
    rb.put32(8,0xfffffff0);CHECK(!table.parse(raw.data(),raw.size()));CHECK(table.size()==1);
    if(argc==3){
        FrontendFontPack pack;std::string error;CHECK(load_frontend_font_pack(argv[1],pack,&error));CHECK(table.load(argv[2],&error));
        CHECK(*table.get(0x218)=="CREATE NEW LICENSE");CHECK(*table.get(0x219)=="OR2C2C");CHECK(table.size()>1200);
        CHECK(pack.fonts[9].width==17);CHECK(pack.fonts[9].metrics.size()==192);CHECK(pack.fonts[9].kerning.size()==192*192);
        CHECK(pack.textures[9].width==512);CHECK(pack.textures[9].height==256);
        frontend_text_init_48e640(b);b.putf(0x34,126);b.putf(0x38,168);
        CHECK(frontend_text_set_48f280(b,"OR2C2C",9,~0u));std::vector<FrontendGlyph> glyphs;
        CHECK(frontend_text_display_48f3c0(b,pack,lines,glyphs));CHECK(glyphs.size()==6);CHECK(glyphs[1].x>glyphs[0].x);
        CHECK(glyphs[0].token==9);CHECK(glyphs[0].color==~0u);
        CHECK(frontend_text_setter(0x48ee60,b,nullptr,0));glyphs.clear();CHECK(frontend_text_display_48f3c0(b,pack,lines,glyphs));CHECK(glyphs.empty());
        std::ifstream file(argv[1],std::ios::binary);std::vector<std::uint8_t> data((std::istreambuf_iterator<char>(file)),{});
        data.back()^=1;CHECK(!parse_frontend_font_pack(data.data(),data.size(),pack));
        std::printf("retail fonts=10 localized strings=%zu\n",table.size());
    }
}
