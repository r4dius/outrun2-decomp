#include "frontend_title_widgets.hpp"
#include "frontend_profiles.hpp"
#include "enhancements/settings_rows.hpp"
#include <cstring>
#include <tuple>

namespace outrun::platform {
using driving::Bytes;
namespace {
std::uint32_t bits(float f){std::uint32_t u;std::memcpy(&u,&f,4);return u;}
// cvttss2si: out-of-range and NaN give 0x80000000.
std::int32_t cvttss2si(float f){return f>-2147483904.f&&f<2147483648.f?std::int32_t(f):std::int32_t(0x80000000u);}
// Retail layout globals read by the options screens (EXE .data defaults).
constexpr float list_x=-13,list_y=-72;            // 692BBC..692BD8 (x, y pairs)
constexpr float field_x=321,field_y=161;           // 692BE4..692C00 (x, y pairs)
constexpr float row_step=16;                       // 5B018C
constexpr std::uint32_t label_color=0xff3f474a;
// 446340 sprite tables: {token, first, last} per slider value + low entry.
const std::vector<std::array<std::uint32_t,3>>* slider_table(std::uint32_t pc){
    static const auto make=[](std::uint32_t token,std::uint32_t n){
        std::vector<std::array<std::uint32_t,3>> t;for(std::uint32_t i=0;i<n;++i)t.push_back({token,i,i});return t;};
    static const auto audio=make(0x2c00f8,11),steering=make(0x2c0100,10);
    return pc==0x5cc048?&audio:pc==0x5cc118?&steering:nullptr;
}
void arrows(std::vector<FrontendListImage>& images,bool left,bool right,int x,int y,int width,float layer){
    if(left)images.push_back({0x42d280,0,0x3004a,~0u,x-12,y+3,0,0,0,layer});
    if(right)images.push_back({0x42d280,0,0x3004a,~0u,x+width-12,y+3,1,0,0,layer});
}
}

void frontend_option_init_445fe0(Bytes b,std::int32_t first,std::int32_t last,std::uint8_t wrap,std::uint32_t layer){
    b.check(0,PcFrontendOptionBytes);b.puti(4,last);b.puti(0,first);b.puti(8,first);
    b.put32(0xc,std::uint32_t(last)-std::uint32_t(first));b.put8(0x10,wrap);b.put32(0x14,layer);
}
bool frontend_option_next_446010(Bytes b,bool& sound){
    sound=false;
    if(b.i32(8)<std::int32_t(b.u32(0xc)+b.u32(0))){b.puti(8,b.i32(8)+1);sound=true;}
    else if(b.u8(0x10)){b.put32(8,b.u32(0));sound=true;}
    return true;
}
bool frontend_option_prev_446050(Bytes b,bool& sound){
    sound=false;
    if(b.i32(8)>b.i32(0)){b.puti(8,b.i32(8)-1);sound=true;}
    else if(b.u8(0x10)){b.put32(8,b.u32(4));sound=true;}
    return true;
}
std::int32_t frontend_option_index_446080(Bytes b){return std::int32_t(b.u32(8)-b.u32(0));}
void frontend_option_set_446100(Bytes b,std::int32_t index){
    if(index>=0&&index<=b.i32(0xc))b.put32(8,b.u32(0)+std::uint32_t(index));
}
void frontend_option_arrows_446090(Bytes b,int x,int y,int width,std::vector<FrontendListImage>& images){
    arrows(images,b.i32(8)>b.i32(0),b.i32(8)<b.i32(4),x,y,width,float(b.i32(0x14)));
}
void frontend_slider_init_446340(Bytes b,std::uint32_t table,std::int32_t low,std::int32_t high,float x,float y,std::uint32_t layer){
    b.check(0,PcFrontendSliderBytes);b.put32(0,table);b.puti(0xa8,high);b.puti(0xa4,low);b.putf(0xb4,x);
    b.put32(0xac,0);b.put32(0xb0,std::uint32_t(high)-std::uint32_t(low));b.putf(0xb8,y);b.put32(0xbc,layer);
}
void frontend_slider_arrows_4463a0(Bytes b,int x,int y,int width,std::vector<FrontendListImage>& images){
    arrows(images,b.i32(0xac)>0,b.i32(0xac)<b.i32(0xb0),x,y,width,float(b.i32(0xbc)));
}

FrontendTitleWidgets::FrontendTitleWidgets(std::uint8_t* object,std::size_t size,Bytes root,FrontendSprites& sprites,
    const FrontendFontPack& fonts,const FrontendTextTable& text,const FrontendInputSnapshot& input,unsigned& previous)
    :storage_(object),object_(object,size),root_(root),ui_{sprites},fonts_(fonts),text_(text),input_(input),previous_(previous){
    object_.check(0,TitleOwnerPcSize);root_.check(0,0xda4);
}
void FrontendTitleWidgets::reset(){list_.reset();frontend_window_suspend_48ca30(object_.sub(0x9bc,PcFrontendWindowBytes));begin_frame();globals_={};}
bool FrontendTitleWidgets::clear(){return !list_||list_->clear_48e440();}
void FrontendTitleWidgets::begin_frame(){glyphs_.clear();images_.clear();icons_.clear();missing_=0;ui_.missing_pc=0;}
void FrontendTitleWidgets::effect(void* user,bool(*callback)(void*,unsigned)){ui_.effect_user=user;ui_.effect_4249f0=callback;}
void FrontendTitleWidgets::frame(const TitleMenuGlobals& globals,const TitleOwnerGlobals& layers,
    float delta,float timer,unsigned pause,float motion){
    // Cooldown is owned by 4D5E40/4D7300, not reset by each input snapshot.
    const float delay=globals_.delay_692c9c;globals_=globals;globals_.delay_692c9c=delay;
    layers_=layers;owner_delta_=delta;list_timer_=timer;ui_.pause_domain=pause;ui_.motion_step=motion;
}
bool FrontendTitleWidgets::service(unsigned pc,std::uint8_t* object,std::size_t offset,int arg,unsigned& result){
    result=0;
    auto window=object_.sub(0x9bc,PcFrontendWindowBytes);
    switch(pc){
    case 0x48cc00:return title_controller_motion_48cc00(object,PcFrontendWindowBytes,owner_delta_);
    case 0x48f5f0:
        return frontend_input_action_48f5f0(object+offset,0x34,input_,arg,previous_,this,
            [](void* p,unsigned key,int argument){auto& self=*static_cast<FrontendTitleWidgets*>(p);
                return self.ui_.input_feedback(self.root_,key,argument);},result);
    case 0x47f110:result=driving::runtime_current_player_47f110();return true;
    case 0x4249f0:
        if(ui_.effect_4249f0&&ui_.effect_4249f0(ui_.effect_user,unsigned(arg)))return true;
        missing_=pc;return false;
    case 0x48dda0:return list_&&list_->display_48dda0(list_timer_,lines_,glyphs_);
    case 0x48dc10:case 0x48dc60:{
        bool sound{};if(!list_||!list_->move(pc==0x48dc60,sound))return false;
        return !sound||service(0x4249f0,object,0,1,result);
    }
    case 0x48e440:return clear();
    case 16:frontend_window_suspend_48ca30(window);return true;
    case 0x4469c0:return slider_tick_4469c0(offset);
    case 0x446440:return slider_refresh_446440(offset);
    case 0x42efa0:case 0x42fc90:return volume(pc,std::uint32_t(arg));
    default:missing_=pc;return false;
    }
}

bool FrontendTitleWidgets::volume(std::uint32_t pc,std::uint32_t arg){
    if(volume_&&volume_(volume_user_,pc,arg))return true;
    missing_=pc;return false;
}
bool FrontendTitleWidgets::list_ready(){
    if(!list_)list_=std::make_unique<FrontendChoiceList>(object_.sub(0x34,PcFrontendChoiceListBytes),ui_,fonts_);
    return list_->size()==0;
}
// 48D0C0(window, "", "", 2 = back button only, ...) then 48CBE0(465EB0(0x296)).
bool FrontendTitleWidgets::options_window(float x,float y,float width,float height){
    auto window=object_.sub(0x9bc,PcFrontendWindowBytes);
    if(!frontend_window_init_48d0c0(window,fonts_,text_,lines_,"","",2,x,y,width,height,layers_.scene_ids[0],0))return false;
    const auto* back=text_.get(0x296);auto widget=window.sub(0x960,PcTextWidgetBytes);
    return back&&frontend_text_set_48f280(widget,*back,widget.u32(0x450),widget.u32(0x474));
}
// 446440: release, 465860 with the table entry of value + low, commit.
bool FrontendTitleWidgets::slider_refresh_446440(std::size_t off){
    auto s=object_.sub(off,PcFrontendSliderBytes);auto res=s.sub(4,0xa0);unsigned r{};
    if(!ui_.call(0x465250,res,nullptr,0,r))return false;
    const auto* table=slider_table(s.u32(0));const auto index=std::int64_t(s.i32(0xac))+s.i32(0xa4);
    if(!table||index<0||index>=std::int64_t(table->size())){missing_=0x446440;return false;}
    const auto& t=(*table)[std::size_t(index)];
    const std::uint32_t args[11]{t[0],t[1],t[2],s.u32(0xbc),0,s.u32(0xb4),s.u32(0xb8),bits(1),bits(1),bits(1),0};
    return ui_.call(0x465860,res,args,11,r)&&ui_.call(0x465970,res,nullptr,0,r);
}
// 4469C0: animation tick, then refresh when finished (4652E0) or never set.
bool FrontendTitleWidgets::slider_tick_4469c0(std::size_t off){
    auto res=object_.sub(off+4,0xa0);unsigned r{},ready{};
    if(!ui_.call(0x4659f0,res,nullptr,0,r)||!ui_.call(0x4652e0,res,nullptr,0,ready))return false;
    return (ready==0&&res.u32(8)!=~0u)||slider_refresh_446440(off);
}
namespace {
// Ordered PC callee of an options control: (pc, ecx offset in the owner, arg).
struct OptionCalls {
    std::uint8_t* owner;const TitleControllerServices& s;
    bool operator()(std::uint32_t pc,std::size_t offset,std::int32_t arg,std::uint32_t& r)const{return s.call(s.user,pc,owner,offset,arg,r);}
    bool operator()(std::uint32_t pc,std::size_t offset,std::int32_t arg=0)const{std::uint32_t r{};return (*this)(pc,offset,arg,r);}
};
// 4469F0 / 446A20: step inside 0..+B0 with 4249F0(1), then 446440 in every case.
bool slider_step(Bytes owner,const OptionCalls& call,std::size_t off,bool up){
    auto s=owner.sub(off,PcFrontendSliderBytes);const auto v=s.i32(0xac);
    if(up?v<s.i32(0xb0):v>0){s.puti(0xac,up?v+1:v-1);if(!call(0x4249f0,off,1))return false;}
    return call(0x446440,off);
}
// 446010 / 446050 with 4249F0(1) when the value moved (or wrapped).
bool option_step(Bytes owner,const OptionCalls& call,std::size_t off,bool up){
    bool changed{};auto o=owner.sub(off,PcFrontendOptionBytes);
    (up?frontend_option_next_446010:frontend_option_prev_446050)(o,changed);
    return !changed||call(0x4249f0,off,1);
}
// 42EFA0(trunc(value * 0.1f * 128.f)) or 42FC90(value * 0.1f), single precision.
bool slider_volume(Bytes owner,const OptionCalls& call,std::size_t off,std::uint32_t pc){
    const float level=float(owner.i32(off+0xac))*0.1f;
    return call(pc,0,pc==0x42efa0?cvttss2si(level*128.f):std::int32_t(bits(level)));
}
}

// ---- Settings: 4D7440 init, 4D86F0 control, 4D60C0 display, 4D76F0 apply.
// Options at +6DC (texts 3..5, license +D8), +6F4 (6..7, +DC), +70C (8..C,
// +E0 difficulty), +724 (12..13, byte +E9); value labels at +4AC + i*8C.
// In the race pause menu (root 3) only +6F4 and +724 are listed and shown.
bool FrontendTitleWidgets::settings_init_4d7440(){
    if(license_.size()<PcLicenseBytes||!list_ready()){missing_=0x4d7440;return false;}
    std::array<std::uint8_t,0x438> data{};Bytes format(data.data(),data.size());frontend_text_format_construct_48d570(format);
    format.put32(0x404,layers_.scene_ids[2]);format.put32(0x408,0x20);format.put8(0x41c,0);
    format.putf(0x42c,-150);format.putf(0x430,-7);
    if(!list_->initialize_48d970(0x5cbf10,frontend_title_sprite_table(),1,2,1,0,96,&format,1))return false;
    auto header=object_.sub(0x34,PcFrontendChoiceListBytes);
    header.putf(0x24,list_x);header.putf(0x28,list_y);header.put32(0x2c,layers_.scene_ids[1]);
    const bool race=globals_.root_state==3;unsigned index{};
    for(unsigned row=0xf;row<=0x12;++row)
        if(!(race&&(row==0xf||row==0x11))&&!list_->add_sprite_48e390(row,previous_,index))return false;
    const auto layer=layers_.scene_ids[2];
    frontend_option_init_445fe0(object_.sub(0x6dc,0x18),3,5,0,layer);
    frontend_option_init_445fe0(object_.sub(0x6f4,0x18),6,7,0,layer);
    frontend_option_init_445fe0(object_.sub(0x70c,0x18),8,0xc,0,layer);
    frontend_option_init_445fe0(object_.sub(0x724,0x18),0x12,0x13,0,layer);
    if(globals_.root_state==2)frontend_option_set_446100(object_.sub(0x6dc,0x18),license_.i32(0xd8));
    frontend_option_set_446100(object_.sub(0x6f4,0x18),license_.i32(0xdc));
    frontend_option_set_446100(object_.sub(0x70c,0x18),license_.i32(0xe0));
    frontend_option_set_446100(object_.sub(0x724,0x18),license_.i8(0xe9));
    if(!options_window(120,110,380,200))return false;   // 692C20 / 692C10 / 692C24 / 692C28
    object_.put32(0x9ac,object_.u32(0x9ac)+1);
    const auto* blank=text_.get(0);if(!blank)return false;
    for(unsigned i=0;i<4;++i){auto label=object_.sub(0x4ac+i*0x8c,0x8c);
        if(!frontend_scroll_text_init_4edaf0(label,fonts_.fonts[9],*blank,int(field_x),
            int(float(int(field_y))+float(i)*row_step),0x83,label_color))return false;
        frontend_scroll_text_layer_4f1ce0(label,layer);
    }
    return enhancements::SettingsAccess::init(*this);   // port: enhancement rows after the original four
}
bool title_settings_control_4d86f0(std::uint8_t* object,std::size_t size,Bytes license,
                                   const TitleMenuGlobals& g,const TitleControllerServices& services){
    if(!object||size<TitleOwnerPcSize||!services.call||license.size()<PcLicenseBytes)return false;
    Bytes owner(object,size);const OptionCalls call{object,services};
    std::int32_t selected=owner.i32(0x34);
    if(!call(0x48dda0,0x34))return false;
    // Protected bridge after 4035F0: the race pause list lacks rows 0 and 2,
    // the same filter as 4D7440 / 4D60C0 (list index 0 -> option 1, 1 -> 3).
    if(g.root_state==3){if(selected>=0)++selected;if(selected>=2)++selected;}
    std::uint32_t action{};if(!call(0x48f5f0,0,1,action))return false;
    switch(action){
    case 0:if(g.feature_mask&1){owner.put32(0x9ac,owner.u32(0x9ac)+1);owner.put32(0x9b0,2);}return true;
    case 1:title_settings_apply_4d76f0(owner,license,g.root_state);
        owner.put32(0x9ac,owner.u32(0x9ac)+1);owner.put32(0x9b4,0);owner.put32(0x9b0,1);return true;
    case 2:return call(0x48dc10,0x34,1);
    case 4:return call(0x48dc60,0x34,1);
    case 3:case 5:
        if(selected>3)return enhancements::settings_step(std::size_t(selected-4),action==5,[&]{return call(0x4249f0,0,1);});   // port: enhancement rows
        return selected<0||option_step(owner,call,0x6dc+std::size_t(selected)*0x18,action==5);
    default:return true;
    }
}
void title_settings_apply_4d76f0(Bytes owner,Bytes license,std::uint32_t root_state){
    const auto flag=[&]{license.put8(0x3f4,std::uint8_t(license.u8(0x3f4)|2));};
    if(root_state==2){license.puti(0xd8,frontend_option_index_446080(owner.sub(0x6dc,0x18)));flag();}
    license.puti(0xdc,frontend_option_index_446080(owner.sub(0x6f4,0x18)));flag();
    license.puti(0xe0,frontend_option_index_446080(owner.sub(0x70c,0x18)));flag();
    license.put8(0xe9,std::uint8_t(frontend_option_index_446080(owner.sub(0x724,0x18))));flag();
}
bool FrontendTitleWidgets::settings_display_4d60c0(){
    const auto& font=fonts_.fonts[9];int row=0;
    for(unsigned i=0;i<4;++i){
        if(globals_.root_state==3&&(i==0||i==2))continue;
        const float step=float(row)*row_step;
        const int x=int(field_x),y=int(float(int(field_y))+step);
        auto label=object_.sub(0x4ac+i*0x8c,0x8c);auto option=object_.sub(0x6dc+i*0x18,0x18);
        const auto* text=text_.get(option.u32(8));   // 564C90 -> 465EB0
        if(!text){missing_=0x465eb0;return false;}
        if(i<3){
            if(!frontend_scroll_text_set_4edcf0(label,font,*text))return false;
            frontend_option_arrows_446090(option,x,y,0x97,images_);
            frontend_scroll_text_move_4edcc0(label,x,y);
        }else{
            const int shift=language_==3?100:0;   // 4493C0() == 3
            frontend_scroll_text_move_4edcc0(label,x+shift,y);
            if(!frontend_scroll_text_set_4edcf0(label,font,*text))return false;
            frontend_option_arrows_446090(option,x+shift,y,0x97-shift,images_);
        }
        if(!frontend_scroll_text_draw_4edb90(label,font,glyphs_))return false;
        ++row;
    }
    return enhancements::SettingsAccess::display(*this,row);   // port: enhancement rows
}

// ---- Controls: 4D7780 init, 4D8890 control, 4D62A0 display, 4D6340 close.
// Row 0 opens the key configuration (stage 21, 4D7E00); row 1 is the
// steering slider +73C (5CC118 values 1..9, license byte +EB).
bool FrontendTitleWidgets::controls_init_4d7780(){
    if(license_.size()<PcLicenseBytes||!list_ready()){missing_=0x4d7780;return false;}
    if(!list_->initialize_48d970(0x5cbf10,frontend_title_sprite_table(),1,2,1,0,96,nullptr,0))return false;
    auto header=object_.sub(0x34,PcFrontendChoiceListBytes);
    header.putf(0x24,list_x);header.putf(0x28,list_y);header.put32(0x2c,layers_.scene_ids[1]);
    unsigned index{};
    for(unsigned row=0x13;row<=0x14;++row)if(!list_->add_sprite_48e390(row,previous_,index))return false;
    auto slider=object_.sub(0x73c,PcFrontendSliderBytes);
    frontend_slider_init_446340(slider,0x5cc118,1,9,list_x+68,list_y+row_step,layers_.scene_ids[2]);
    slider.puti(0xac,license_.i8(0xeb));
    object_.put32(0x9ac,object_.u32(0x9ac)+1);
    return options_window(70,110,500,100);   // 692C40 / 692C10 / 692C3C / 692C34
}
bool title_controls_control_4d8890(std::uint8_t* object,std::size_t size,Bytes license,
                                   const TitleMenuGlobals&,const TitleControllerServices& services){
    if(!object||size<TitleOwnerPcSize||!services.call||license.size()<PcLicenseBytes)return false;
    Bytes owner(object,size);const OptionCalls call{object,services};
    if(!call(0x48dda0,0x34)||!call(0x4469c0,0x73c))return false;
    std::uint32_t action{};if(!call(0x48f5f0,0,1,action))return false;
    switch(action){
    case 0:
        if(owner.i32(0x34)!=0)return true;
        owner.put32(0x9b4,0x15);owner.put32(0x9b0,1);owner.put32(0x9ac,owner.u32(0x9ac)+1);
        return call(0x4249f0,0,0x40);
    case 1:   // 49A650(value) after the store is a bare RET
        license.put8(0x3f4,std::uint8_t(license.u8(0x3f4)|2));
        license.put8(0xeb,std::uint8_t(owner.u32(0x73c+0xac)));
        owner.put32(0x9ac,owner.u32(0x9ac)+1);owner.put32(0x9b4,0);owner.put32(0x9b0,1);return true;
    case 2:return call(0x48dc10,0x34,1);
    case 4:return call(0x48dc60,0x34,1);
    case 3:case 5:return owner.i32(0x34)!=1||slider_step(owner,call,0x73c,action==5);
    default:return true;
    }
}
bool FrontendTitleWidgets::controls_display_4d62a0(){
    // Each row sets font 9 / layer / colour / cursor (42CA60..42CC00) without
    // printing; only the slider row draws its arrows.
    frontend_slider_arrows_4463a0(object_.sub(0x73c,PcFrontendSliderBytes),int(field_x),
        int(float(int(field_y))+row_step),0x97,images_);
    return true;
}
bool FrontendTitleWidgets::controls_close_4d6340(){
    unsigned r{};
    if(!call(0x48e440,0x34,0,r)||!ui_.call(0x465250,object_.sub(0x73c+4,0xa0),nullptr,0,r)||!call(16,0x9bc,0,r))return false;
    object_.put8(0x9b8,1);return true;
}

// ---- Audio: 4D78C0 init, 4D89B0 control, 4D6370 display, 4D7AB0 apply,
// 4D63F0 close. Sliders +7FC (SE, license +D4 -> 42EFA0) and +8BC (BGM,
// +D0 -> 42FC90), 5CC048 values 0..10; option +97C (texts 23..24, byte +ED)
// has no list row.
bool FrontendTitleWidgets::audio_init_4d78c0(){
    if(license_.size()<PcLicenseBytes||!list_ready()){missing_=0x4d78c0;return false;}
    if(!list_->initialize_48d970(0x5cbf10,frontend_title_sprite_table(),1,2,1,0,96,nullptr,0))return false;
    auto header=object_.sub(0x34,PcFrontendChoiceListBytes);
    header.putf(0x24,list_x);header.putf(0x28,list_y);header.put32(0x2c,layers_.scene_ids[1]);
    unsigned index{};
    for(unsigned row=0x15;row<=0x16;++row)if(!list_->add_sprite_48e390(row,previous_,index))return false;
    const auto layer=layers_.scene_ids[2];
    for(auto [off,field,y]:{std::tuple<std::size_t,std::size_t,float>{0x7fc,0xd4,list_y},{0x8bc,0xd0,list_y+row_step}}){
        auto slider=object_.sub(off,PcFrontendSliderBytes);
        frontend_slider_init_446340(slider,0x5cc048,0,10,list_x,y,layer);
        auto value=license_.i32(field);if(value>10||value<0){value=0;license_.puti(field,0);}
        slider.puti(0xac,value);
    }
    frontend_option_init_445fe0(object_.sub(0x97c,0x18),0x23,0x24,0,layer);
    frontend_option_set_446100(object_.sub(0x97c,0x18),license_.i8(0xed));
    // 48E200 counts the rows whose +4 is set (all of them here).
    int visible{};for(std::size_t i=0;i<list_->size();++i)visible+=list_->row(i).u32(4)!=0;
    const float height=float(visible)*header.f32(0x20)+24.f;
    if(!options_window(120,110,380,160))return false;   // 692C20 / 692C10 / 692C24 / 692C4C
    object_.sub(0x9bc,PcFrontendWindowBytes).put16(0x38,std::uint16_t(unsigned(int(height))+80));   // 48CAC0
    object_.put32(0x9ac,object_.u32(0x9ac)+1);
    return true;
}
bool title_audio_control_4d89b0(std::uint8_t* object,std::size_t size,Bytes license,
                                const TitleMenuGlobals& g,const TitleControllerServices& services){
    if(!object||size<TitleOwnerPcSize||!services.call||license.size()<PcLicenseBytes)return false;
    Bytes owner(object,size);const OptionCalls call{object,services};
    std::uint32_t action{};if(!call(0x48f5f0,0,0,action))return false;
    switch(action){
    case 0:if(g.feature_mask&1){owner.put32(0x9ac,owner.u32(0x9ac)+1);owner.put32(0x9b0,2);}return true;
    case 1:
        if(!slider_volume(owner,call,0x7fc,0x42efa0))return false;
        title_audio_apply_4d7ab0(owner,license);
        owner.put32(0x9ac,owner.u32(0x9ac)+1);owner.put32(0x9b4,0);owner.put32(0x9b0,1);return true;
    case 2:case 4:   // row 1 then calls 42E020(0, 1.0, 1) = mov eax,1
        return slider_volume(owner,call,0x7fc,0x42efa0)&&call(action==2?0x48dc10:0x48dc60,0x34,1);
    case 3:case 5:{
        const bool up=action==5;
        switch(owner.i32(0x34)){
        case 0:return slider_step(owner,call,0x7fc,up)&&slider_volume(owner,call,0x7fc,0x42efa0);
        // The BGM row also sends its own level to 42EFA0 (PC behaviour).
        case 1:return slider_step(owner,call,0x8bc,up)&&slider_volume(owner,call,0x8bc,0x42fc90)&&
                      slider_volume(owner,call,0x8bc,0x42efa0);
        case 2:return option_step(owner,call,0x97c,up);
        default:return true;
        }
    }
    default:return true;
    }
}
void title_audio_apply_4d7ab0(Bytes owner,Bytes license){
    const auto flag=[&]{license.put8(0x3f4,std::uint8_t(license.u8(0x3f4)|2));};
    // cvtsi2ss / cvttss2si round trip of each slider value.
    flag();license.puti(0xd4,cvttss2si(float(owner.i32(0x7fc+0xac))));
    flag();license.puti(0xd0,cvttss2si(float(owner.i32(0x8bc+0xac))));
    flag();license.put8(0xed,std::uint8_t(frontend_option_index_446080(owner.sub(0x97c,0x18))));
}
bool FrontendTitleWidgets::audio_display_4d6370(){
    unsigned r{};
    if(!call(0x48dda0,0x34,0,r)||!slider_tick_4469c0(0x7fc)||!slider_tick_4469c0(0x8bc))return false;
    const int x=int(field_x),y=int(float(int(field_y)));
    frontend_slider_arrows_4463a0(object_.sub(0x7fc,PcFrontendSliderBytes),x,y,0x97,images_);
    frontend_slider_arrows_4463a0(object_.sub(0x8bc,PcFrontendSliderBytes),x,int(float(int(field_y))+row_step),0x97,images_);
    return true;
}
bool FrontendTitleWidgets::audio_close_4d63f0(){
    unsigned r{};
    if(!call(0x48e440,0x34,0,r)||!ui_.call(0x465250,object_.sub(0x7fc+4,0xa0),nullptr,0,r)||
       !ui_.call(0x465250,object_.sub(0x8bc+4,0xa0),nullptr,0,r)||!call(16,0x9bc,0,r))return false;
    object_.put8(0x9b8,1);return true;
}

// ---- Pause confirmations (root 3): 4D6430/4D7B20/4D7B80 Retry, 4D6530/
// 4D7C70/4D6630 Quit, 4D6510 their display, 4D66D0/4D7DA0 the B close.
// 84B208 is the dialog step. The window +9BC is reused for the A/B prompt
// (48D0C0 kind 3, x 692C20 = 120, y 692C10 = 110, w 692C24 = 380, h 692C44
// (Retry) / 692C48 (Quit), both 150, layer 692B28).
bool FrontendTitleWidgets::pause_call(unsigned pc,unsigned a0,unsigned a1,unsigned& out){
    out=0;if(pause_&&pause_(pause_user_,pc,a0,a1,out))return true;
    missing_=pc;return false;
}
bool FrontendTitleWidgets::confirm_window(unsigned id){
    const auto* body=text_.get(id);if(!body){missing_=0x465eb0;return false;}
    return frontend_window_init_48d0c0(object_.sub(0x9bc,PcFrontendWindowBytes),fonts_,text_,lines_,"",*body,
        3,120,110,380,150,layers_.scene_ids[0],0);
}
// 7C24EC (license +10C) += 4505D0, then license +3F4 |= 2.
bool FrontendTitleWidgets::add_play_time(){
    if(license_.size()<PcLicenseBytes){missing_=0x7c24ec;return false;}
    unsigned frames{};if(!pause_call(0x4505d0,0,0,frames))return false;
    license_.put8(0x3f4,std::uint8_t(license_.u8(0x3f4)|2));
    license_.put32(0x10c,license_.u32(0x10c)+frames);return true;
}
bool FrontendTitleWidgets::retry_init_4d6430(){
    object_.put32(0x9ac,object_.u32(0x9ac)+1);confirm_84b208_=0;
    const auto v=globals_.variant;
    return confirm_window(v==0||v==7?0x3d2:0x3a7);
}
// 4D6530: the 3A8 text needs the LAN session 7D68AC (+5 and +7 set).
bool FrontendTitleWidgets::quit_init_4d6530(){
    object_.put32(0x9ac,object_.u32(0x9ac)+1);confirm_84b208_=0;
    const auto v=globals_.variant;
    if(v==3||v==4){missing_=0x7d68ac;return false;}
    return confirm_window(v==0||v==7?0x3d2:0x3a7);
}
// 48F5F0(1): 0 (A / Start) confirms, 1 (B) cancels; both return 0.
bool FrontendTitleWidgets::retry_control_4d7b20(){
    if(confirm_84b208_==1){object_.put32(0x9ac,object_.u32(0x9ac)+1);object_.put32(0x9b0,2);return true;}
    if(confirm_84b208_!=0)return true;
    unsigned action{};if(!call(0x48f5f0,0,1,action))return false;
    if(action==0)confirm_84b208_=1;
    else if(action==1){object_.put32(0x9ac,object_.u32(0x9ac)+1);object_.put32(0x9b4,0);object_.put32(0x9b0,1);}
    return true;
}
// 4D7B80: Retry asks mode 0x1E (0x22 with 43F860, after the 4857C0 camera
// reset). 456E10 (LAN players) - 1 > 0 also runs 4964B0 / 4EEF20 (not
// reached offline: 456E10 is 0 without a session).
bool FrontendTitleWidgets::retry_close_4d7b80(){
    unsigned r{};if(!call(16,0x9bc,0,r))return false;
    if(std::int32_t(confirm_84b208_)>0){
        if(!add_play_time()||!pause_call(0x450230,6))return false;
        unsigned flag{};if(!pause_call(0x43f860,0,0,flag))return false;
        if((flag&0xff)!=0&&!pause_call(0x4857c0))return false;
        if(!pause_call(0x440de0,(flag&0xff)!=0?0x22:0x1e))return false;
        unsigned players{};if(!pause_call(0x456e10,0,0,players))return false;
        if(std::int32_t(players)-1>0){                                  // LAN: the leave penalty on the licence rating
            unsigned rated{};if(!pause_call(0x4964b0,0,0,rated))return false;
            if((rated&0xff)!=0){
                unsigned r{};if(!pause_call(0x4eef20,players-1,0,r))return false;
                if(license_.size()>0x3f4)license_.put8(0x3f4,std::uint8_t(license_.u8(0x3f4)|2u));
            }
        }
        if(!pause_call(0x440a10,8,0x18))return false;
        const auto v=globals_.variant;
        if((v==3||v==4)&&!pause_call(0x4edce0,0x10))return false;
    }
    object_.put8(0x9b8,1);object_.put32(0x9b0,2);return true;
}
bool FrontendTitleWidgets::quit_control_4d7c70(){
    const auto v=globals_.variant;
    unsigned r{};
    switch(confirm_84b208_){
    case 0:{
        unsigned action{};if(!call(0x48f5f0,0,1,action))return false;
        if(action==0){++confirm_84b208_;return call(16,0x9bc,0,r);}
        if(action==1){object_.put32(0x9ac,object_.u32(0x9ac)+1);object_.put32(0x9b4,0);object_.put32(0x9b0,1);}
        return true;
    }
    case 1:
        // Variants 3/4: 456E10/4964B0/4EEF20 (the leave penalty), then the LAN leave
        // 4D7D05 (444B20(0, 1, 0), 454220(1), 7D68D8 = 0x14, the session +4 call, 450230(7)).
        if(v==3||v==4){
            unsigned players{};if(!pause_call(0x456e10,0,0,players))return false;
            if(std::int32_t(players)-1>0){
                unsigned rated{};if(!pause_call(0x4964b0,0,0,rated))return false;
                if((rated&0xff)!=0){
                    unsigned r{};if(!pause_call(0x4eef20,players-1,0,r))return false;
                    if(license_.size()>0x3f4)license_.put8(0x3f4,std::uint8_t(license_.u8(0x3f4)|2u));
                }
            }
            unsigned r{};if(!pause_call(0x4d7d05,0,0,r))return false;
        }
        ++confirm_84b208_;return true;
    case 2:
        if(v==3||v==4){                                   // 830C00 / 441260: wait for the LAN leave
            unsigned busy{};if(!pause_call(0x4d7c9a,0,0,busy))return false;
            if(busy)return true;
        }
        object_.put32(0x9ac,object_.u32(0x9ac)+1);object_.put32(0x9b0,2);return true;
    default:return true;
    }
}
// 4D6630: Quit asks mode 0x1B (0x22 with 43F860).
bool FrontendTitleWidgets::quit_close_4d6630(){
    unsigned r{};if(!call(16,0x9bc,0,r))return false;
    if(std::int32_t(confirm_84b208_)>0){
        if(!pause_call(0x450230,7)||!add_play_time()||!pause_call(0x4857c0))return false;
        unsigned flag{};if(!pause_call(0x43f860,0,0,flag))return false;
        if(!pause_call(0x440de0,(flag&0xff)!=0?0x22:0x1b)||!pause_call(0x440a10,8,0x18)||!pause_call(0x4edce0,8))return false;
    }
    object_.put8(0x9b8,1);object_.put32(0x9b0,2);return true;
}
// 4D66D0: step 2 in the race pause (root 3), else 0 (save, then step 2).
bool FrontendTitleWidgets::close_init_4d66d0(){
    object_.put32(0x9ac,object_.u32(0x9ac)+1);
    confirm_84b208_=globals_.root_state==3?2:0;return true;
}
bool FrontendTitleWidgets::close_control_4d7da0(){
    const auto save=[&]{return license_.size()<PcLicenseBytes||(license_.u8(0x3f4)&2)==0||pause_call(0x416420);};
    switch(confirm_84b208_){
    case 0:if(!save())return false;++confirm_84b208_;return true;
    case 1:confirm_84b208_=2;return true;
    case 2:if(!save())return false;object_.put32(0x9ac,object_.u32(0x9ac)+1);object_.put32(0x9b0,2);return true;
    default:return true;
    }
}

bool FrontendTitleWidgets::config_call(std::uint32_t pc,std::size_t offset,const std::uint32_t* a,std::size_t n,
                                       const std::vector<std::string>& s,std::uint32_t& result){
    result=0;
    auto arg=[&](std::size_t k){return k<n?a[k]:0u;};
    auto f=[&](std::size_t k){float v;const std::uint32_t u=arg(k);std::memcpy(&v,&u,4);return v;};
    auto str=[&](std::size_t k)->std::string_view{return k<s.size()?std::string_view(s[k]):std::string_view();};
    switch(pc){
    case 0x48f5f0:case 0x4249f0:{unsigned r{};const bool ok=service(pc,storage_,offset,int(arg(0)),r);result=r;return ok;}
    case 0x4536f0:result=globals_.feature_mask&arg(0);return true;
    case 0x48d0c0:   // (title, body, flags, x, y, width, height, layer, decoration) on the window +9BC
        return frontend_window_init_48d0c0(object_.sub(offset,PcFrontendWindowBytes),fonts_,text_,lines_,str(0),str(1),
            std::uint8_t(arg(2)),f(3),f(4),f(5),f(6),arg(7),std::uint8_t(arg(8)));
    case 0x48ee80:{auto w=object_.sub(offset,PcTextWidgetBytes);return frontend_text_set_48f280(w,str(1),w.u32(0x450),w.u32(0x474));}
    case 0x446340:   // (table, low, high, x, y, layer)
        frontend_slider_init_446340(object_.sub(offset,PcFrontendSliderBytes),arg(0),std::int32_t(arg(1)),std::int32_t(arg(2)),f(3),f(4),arg(5));
        return true;
    case 0x4469c0:return slider_tick_4469c0(offset);
    case 0x465250:{unsigned r{};return ui_.call(0x465250,object_.sub(offset,0xa0),nullptr,0,r);}
    case 0x48ca30:frontend_window_suspend_48ca30(object_.sub(offset,PcFrontendWindowBytes));return true;   // window vtable +10
    case 0x42d280:   // (token, x, y, frame, layer, colour)
        images_.push_back({0x42d280,0,arg(0),arg(5),std::int32_t(arg(1)),std::int32_t(arg(2)),std::int32_t(arg(3)),0,0,float(std::int32_t(arg(4)))});
        return true;
    case 0x42ca60:print_.font=arg(0);return print_.font<fonts_.fonts.size();
    case 0x42ccb0:print_.layer=arg(0);return true;
    case 0x42cca0:print_.color=arg(0);return true;
    case 0x42cc00:print_.x=std::int32_t(arg(0));print_.y=std::int32_t(arg(1));return true;
    case 0x42ce70:{  // (width limit, format...): the formatted text, glyph by glyph while the advance fits
        const auto& font=fonts_.fonts[print_.font];
        FrontendTextStyle style;style.mode=print_.layer;style.color=print_.color;style.flags=1|0x100;
        FrontendTextCursor cursor{std::int16_t(print_.x),std::int16_t(print_.x),std::int16_t(print_.y)};
        const auto text=str(1);std::int32_t width=0;
        for(std::size_t i=0;i<text.size()&&text[i];++i){
            const auto c=std::uint8_t(text[i]);
            const char one[2]{char(c),0};
            if(c!=0xa)width+=frontend_text_width_42c480(font,style,std::string_view(one,1));
            if(width>std::int32_t(arg(0)))break;
            FrontendGlyph g;
            if(frontend_text_glyph_42c860(font,style,cursor,c,g))glyphs_.push_back(g);
            frontend_text_advance_42c610(font,style,cursor,c,i+1<text.size()?std::uint8_t(text[i+1]):0);
        }
        print_.x=cursor.x;print_.y=cursor.y;return true;}
    }
    if(offset==0x1c6c){   // the 4ECFB0 list
        if(pc==0x4ecfb0){
            if(!config_list_)config_list_=std::make_unique<FrontendList>(object_.sub(0x1c6c,PcFrontendListBytes),ui_,fonts_.fonts[9]);
            return config_list_->initialize_4ecfb0(arg(0),{},f(1),std::uint8_t(arg(2)),std::uint8_t(arg(3)),std::int32_t(arg(4)));
        }
        if(!config_list_)return pc==0x4eda60;   // cleared already
        auto& l=*config_list_;
        switch(pc){
        case 0x4ed160:{const auto t=str(0);unsigned index{};if(!l.add_text_4ed160(&t,std::int32_t(arg(1)),index))return false;result=index;return true;}
        case 0x4ed360:return l.set_enabled_4ed300(arg(0),true);
        case 0x4ed390:return l.show_4ed390(arg(0));
        case 0x4ed250:case 0x4ed2a0:{bool sound{};if(!l.move(pc==0x4ed2a0,sound))return false;
            if(!sound)return true;unsigned r{};return service(0x4249f0,storage_,0,1,r);}
        case 0x4ed3e0:return l.display_4ed3e0(list_timer_,glyphs_,images_);
        case 0x4eda60:{const bool ok=l.clear_4eda60();config_list_.reset();return ok;}
        }
        return l.setter(pc,a,pc==0x4ed8a0?2u:1u);   // 4ED880 / 4ED8C0 (one argument), 4ED8A0 (two)
    }
    missing_=pc;return false;
}
bool FrontendTitleWidgets::invoke(unsigned pc,std::size_t offset,unsigned& result){
    result=0;bool ok=false;
    const TitleControllerServices services{this,[](void* p,unsigned entry,std::uint8_t* object,
        std::size_t off,int arg,unsigned& out){return static_cast<FrontendTitleWidgets*>(p)->service(entry,object,off,arg,out);},globals_.root_state};
    auto* object=storage_;
    // Controls > Configuration (stage 21, 4D7E00 / 4D7FB0) remaps PC DirectInput
    // devices and its control loop goes through the protected code: not ported.
    // Port behaviour: the choice closes the Controls window (stage 8, 4D6340)
    // and returns to the Options list instead of leaving the menu stuck.
    if(offset==0&&(pc==0x4d7e00||pc==0x4d7fb0||pc==0x4d6e50)&&config_runner_){
        ok=config_runner_(config_user_,pc,*this,result);
        if(!ok&&!missing_)missing_=pc;
        return ok;
    }
    if(pc==0x4d7e00&&offset==0){object_.put32(0x9ac,8);return true;}
    if(pc==0x4d5e40&&offset==0){
        if(object_.u32(0x9ac)==0&&(!list_||list_->size()==0)){
            list_=std::make_unique<FrontendChoiceList>(object_.sub(0x34,PcFrontendChoiceListBytes),ui_,fonts_);
            ok=frontend_title_init_4d5e40(object_,*list_,fonts_,text_,lines_,globals_,layers_,{},previous_);
            if(!ok)list_.reset();
        }
    }else if(pc==0x48d420&&offset==0x9bc){
        ok=title_controller_tick_48d420(object+offset,PcFrontendWindowBytes,services,result);
    }else if(pc==0x4d7300&&offset==0){
        ok=title_menu_control_4d7300(object,object_.size(),globals_,services,result);
    }else if(pc==0x4d6090&&offset==0){
        ok=title_menu_close_4d6090(object,object_.size(),services);
    }else if(offset==0&&(pc==0x4d7440||pc==0x4d7780||pc==0x4d78c0)){
        ok=pc==0x4d7440?settings_init_4d7440():pc==0x4d7780?controls_init_4d7780():audio_init_4d78c0();
        result=ok;
    }else if(offset==0&&(pc==0x4d86f0||pc==0x4d8890||pc==0x4d89b0)){
        if(license_.size()<PcLicenseBytes)missing_=pc;
        else ok=(pc==0x4d86f0?title_settings_control_4d86f0:pc==0x4d8890?title_controls_control_4d8890:title_audio_control_4d89b0)
            (object,object_.size(),license_,globals_,services);
    }
    else if(pc==0x4d6340&&offset==0)ok=controls_close_4d6340();
    else if(pc==0x4d63f0&&offset==0)ok=audio_close_4d63f0();
    else if(offset==0&&pc==0x4d6430)ok=retry_init_4d6430();
    else if(offset==0&&pc==0x4d7b20)ok=retry_control_4d7b20();
    else if(offset==0&&pc==0x4d7b80)ok=retry_close_4d7b80();
    else if(offset==0&&pc==0x4d6530)ok=quit_init_4d6530();
    else if(offset==0&&pc==0x4d7c70)ok=quit_control_4d7c70();
    else if(offset==0&&pc==0x4d6630)ok=quit_close_4d6630();
    else if(offset==0&&pc==0x4d66d0)ok=close_init_4d66d0();
    else if(offset==0&&pc==0x4d7da0)ok=close_control_4d7da0();
    else if(pc==0x4d7260&&offset==0){
        ok=frontend_window_display_48c5f0(object_.sub(0x9bc,PcFrontendWindowBytes),fonts_,lines_,glyphs_,images_,icons_);
        const auto target=title_owner_control_target_4d7260(object_.u32(0x9ac));
        if(ok&&target==0x4d60c0)ok=settings_display_4d60c0();
        else if(ok&&target==0x4d62a0)ok=controls_display_4d62a0();
        else if(ok&&target==0x4d6370)ok=audio_display_4d6370();
        else if(ok&&target==0x4d6510){   // 4D6510: the window +C again while 84B208 is 0 (drawn twice, as on PC)
            if(confirm_84b208_==0)ok=frontend_window_display_48c5f0(object_.sub(0x9bc,PcFrontendWindowBytes),fonts_,lines_,glyphs_,images_,icons_);
        }
        else if(ok&&target==0x4d6a60&&config_runner_){unsigned r{};ok=config_runner_(config_user_,target,*this,r);if(!ok&&!missing_)missing_=target;}
        else if(ok&&target){missing_=target;ok=false;} // later original display bodies, not empty successes
    }
    if(!ok&&!missing_)missing_=ui_.missing_pc?ui_.missing_pc:pc;
    return ok;
}
}
