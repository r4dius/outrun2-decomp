#include "asset_views.hpp"
#include "carview_layouts.hpp"
#include <functional>
#include <iostream>
#include <stdexcept>
using namespace outrun::assets;
namespace {
unsigned checks=0;
void check(bool b) { ++checks; if(!b) throw std::runtime_error("asset check failed"); }
void put(std::vector<std::uint8_t>& v,std::size_t o,std::uint32_t x) {
    for(unsigned i=0;i<4;++i) v.at(o+i)=std::uint8_t(x>>(i*8));
}
void bad(const std::function<void()>& f) {
    bool rejected=false; try{f();}catch(const FormatError&){rejected=true;} check(rejected);
}
std::vector<std::uint8_t> xst() {
    std::vector<std::uint8_t> v(84);
    put(v,8,1);put(v,16,1);put(v,20,32);put(v,24,1);put(v,28,56);
    put(v,32,1);put(v,36,40);return v;
}
}
int main() {
    namespace L=outrun::carview_layout;
    static_assert(L::ObjectHeader::byte_size==52);
    static_assert(L::tag_XSTHEAD::byte_size==32);
    static_assert(L::VtxFormatList::byte_size==44);
    static_assert(L::MatAttrib_member::flags_mask==0xffu);
    static_assert(L::TexAttrib_member::coord_index_shift==28);
    auto v=xst();const auto copy=v;
    auto x=inspect_xst(ByteView(v),ScreenLayout::Classic28);
    check(x.texture_count==1);check(x.display_tables.size()==1);
    check(x.display_tables[0].data_offset==40);check(x.screen_table_offset==56);check(v==copy);
    bad([&]{inspect_xst(ByteView(v),ScreenLayout::Rotated32);});
    v.resize(88);check(inspect_xst(ByteView(v),ScreenLayout::Rotated32).screen_count==1);
    for(std::size_t n=0;n<32;++n) {
        std::vector<std::uint8_t> small(n);
        bad([&]{inspect_xst(ByteView(small),ScreenLayout::Classic28);});
    }
    v=xst();put(v,16,0xffffffffu);bad([&]{inspect_xst(ByteView(v),ScreenLayout::Classic28);});
    v=xst();put(v,20,0xfffffffcu);bad([&]{inspect_xst(ByteView(v),ScreenLayout::Classic28);});
    v=xst();put(v,32,0xffffffffu);bad([&]{inspect_xst(ByteView(v),ScreenLayout::Classic28);});
    v=xst();put(v,36,0xfffffff0u);bad([&]{inspect_xst(ByteView(v),ScreenLayout::Classic28);});
    v=xst();put(v,24,0xffffffffu);bad([&]{inspect_xst(ByteView(v),ScreenLayout::Classic28);});
    v=xst();bad([&]{inspect_xst(ByteView(v),static_cast<ScreenLayout>(99));});
    std::vector<std::uint8_t> empty(32);check(inspect_xst(ByteView(empty),ScreenLayout::Classic28).display_tables.empty());
    bad([&]{inspect_xst(ByteView(nullptr,32),ScreenLayout::Classic28);});
    std::vector<std::uint8_t> ob(352); for(unsigned i=0;i<9;++i) put(ob,i*4,52);
    put(ob,4,52);put(ob,8,116);put(ob,16,116);put(ob,24,148);put(ob,28,192);put(ob,32,280);
    for(unsigned i=0;i<4;++i)put(ob,36+i*4,1);
    auto o=inspect_object(ByteView(ob));check(o.matrix_count==1);check(o.material_count==1);
    auto orig=ob;put(ob,44,0xffffffffu);bad([&]{inspect_object(ByteView(ob));});
    ob=orig;ob.pop_back();bad([&]{inspect_object(ByteView(ob));});
    ob=orig;put(ob,8,115);bad([&]{inspect_object(ByteView(ob));});
    ob=orig;put(ob,8,0);bad([&]{inspect_object(ByteView(ob));});
    ob=orig;put(ob,24,0xfffffff0u);bad([&]{inspect_object(ByteView(ob));});
    for(std::size_t n=0;n<52;++n) {
        std::vector<std::uint8_t> small(n);bad([&]{inspect_object(ByteView(small));});
    }
    std::cout<<"asset_views: "<<checks<<" checks passed (synthetic resources only)\n";
}
