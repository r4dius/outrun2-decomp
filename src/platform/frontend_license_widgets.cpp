#include "frontend_license_widgets.hpp"
#include "title_owner.hpp"
#include <algorithm>
#include <cmath>
#include <cstdio>

namespace outrun::platform {
using driving::Bytes;
FrontendLicenseWidgets::FrontendLicenseWidgets(Bytes b,FrontendUiResources& ui,const FrontendFontPack& fonts,const FrontendTextTable& text)
    :object_(b),ui_(ui),fonts_(fonts),text_(text){
    services_.user=this;services_.call=[](void* p,unsigned pc,std::size_t off,const unsigned* a,std::size_t n,unsigned& r){
        auto& w=*static_cast<FrontendLicenseWidgets*>(p);
        w.services_.missing_pc=0;
        const bool ok=w.call(pc,off,a,n,r);
        if(!ok&&!w.services_.missing_pc)w.services_.missing_pc=pc;return ok;
    };
    keyboard_.user=this;
    keyboard_.input=[](void* p,Bytes,int& input){auto& w=*static_cast<FrontendLicenseWidgets*>(p);
        unsigned value{},arg=~0u;if(!w.call(0x48f5f0,0xa78,&arg,1,value))return false;
        input=static_cast<int>(value);return true;};
    keyboard_.sound=[](void* p,unsigned id){auto& w=*static_cast<FrontendLicenseWidgets*>(p);unsigned result{};
        return w.external_call_&&w.external_call_(w.external_user_,w.services_,0x4249f0,0,&id,1,result);};
    keyboard_.focus=[](void* p,bool active){auto& w=*static_cast<FrontendLicenseWidgets*>(p);
        return w.focus_call_&&w.focus_call_(w.focus_user_,active);};
}
bool FrontendLicenseWidgets::construct_widget(std::size_t off,unsigned& repeat){
    if(off>object_.size()||object_.size()-off<PcTextWidgetBytes)return false;
    // Constructors operate in place; unspecified PC bytes stay untouched.
    std::array<std::uint8_t,PcTextWidgetBytes> data;
    for(std::size_t i=0;i<data.size();++i)data[i]=object_.u8(off+i);
    if(!title_widget_construct_48e590(data.data(),data.size(),repeat))return false;
    for(std::size_t i=0;i<data.size();++i)object_.put8(off+i,data[i]);
    widgets_.insert(off);return true;
}
FrontendLicenseWidgets::~FrontendLicenseWidgets(){
    // Resource ownership is independent of normal virtual suspend. In
    // particular +4D8 and the special panel survive 4E1AC0, but not 4E2000.
    // Do not call borrowed owner/platform callbacks during native destruction.
    choice_lists_.clear();lists_.clear();
    for(auto i=resources_.rbegin();i!=resources_.rend();++i){unsigned ignored{};
        ui_.call(0x465250,object_.sub(*i,0xa0),nullptr,0,ignored);}
}
bool FrontendLicenseWidgets::construct_resource(std::size_t off){
    if(off>object_.size()||object_.size()-off<0xa0)return false;
    unsigned result{};if(!ui_.call(0x465160,object_.sub(off,0xa0),nullptr,0,result))return false;
    resources_.insert(off);return true;
}
bool FrontendLicenseWidgets::construct_editor_resources(unsigned& repeat){
    if(object_.size()<PcLicenseEditorBytes)return false;
    for(auto off:{0x4cu,0xecu,0x18cu,0x22cu,0x2ccu,0x36cu,0x40cu,0x4acu,0x54cu})if(!construct_resource(off))return false;
    if(!construct_widget(0x5ec,repeat)||!keyboard_construct_468e40(object_.sub(0xa78,PcKeyboardBytes),repeat))return false;
    resources_.insert(0xa78+0x4c);resources_.insert(0xa78+0x554);
    for(unsigned i=0;i<7;++i)resources_.insert(0xa78+0xf0+i*0xa0);
    keyboard_constructed_=true;return true;
}
bool FrontendLicenseWidgets::construct_editor_4dd5c0(unsigned& repeat){
    if(object_.size()<PcLicenseEditorBytes||!widgets_.empty()||!resources_.empty()||keyboard_constructed_)return false;
    std::array<std::uint8_t,0x34> base;
    for(unsigned i=0;i<base.size();++i)base[i]=object_.u8(i);
    if(!title_base_construct_48f480(base.data(),base.size(),repeat))return false;
    for(unsigned i=0;i<base.size();++i)object_.put8(i,base[i]);
    object_.put32(0,0x5cd9f8);
    if(!construct_editor_resources(repeat))return false;
    object_.put32(8,24);object_.put32(4,1);return true;
}
bool FrontendLicenseWidgets::construct_panel_4e0ab0(std::size_t off,unsigned& repeat){
    if(off>object_.size()||object_.size()-off<0x225c)return false;
    for(unsigned i=0;i<7;++i)if(!construct_widget(off+8+i*0x48c,repeat))return false;
    for(unsigned i=0;i<4;++i)if(!construct_resource(off+0x1fdc+i*0xa0))return false;
    return true;
}
bool FrontendLicenseWidgets::construct_chooser_4e1890(unsigned& repeat){
    if(object_.size()<PcLicenseChooserBytes||!widgets_.empty()||!resources_.empty()||
       !windows_.empty()||!lists_.empty()||!choice_lists_.empty()||keyboard_constructed_)return false;
    std::array<std::uint8_t,0x34> base;
    for(unsigned i=0;i<base.size();++i)base[i]=object_.u8(i);
    if(!title_base_construct_48f480(base.data(),base.size(),repeat))return false;
    for(unsigned i=0;i<base.size();++i)object_.put8(i,base[i]);object_.put32(0,0x5cdc90);
    for(unsigned off:{0x1b8u,0x258u,0x2f8u,0x398u,0x438u,0x4d8u})if(!construct_resource(off))return false;
    for(unsigned off:{0x57cu,0xa08u,0xe94u})if(!construct_widget(off,repeat))return false;
    if(!construct_panel_4e0ab0(0x1324,repeat))return false;
    for(unsigned i=0;i<3;++i)if(!construct_panel_4e0ab0(0x3580+i*0x225c,repeat))return false;
    if(!construct_window(0x9d54,repeat)||!construct_list(0xb004)||
       !construct_window(0xb040,repeat)||!construct_choice_list(0xc2f0,repeat))return false;
    object_.put32(8,21);chooser_constructed_=true;return true;
}
bool FrontendLicenseWidgets::construct_list(std::size_t off){
    if(off>object_.size()||object_.size()-off<PcFrontendListBytes||lists_.count(off))return false;
    lists_.emplace(off,std::make_unique<FrontendList>(object_.sub(off,PcFrontendListBytes),ui_,fonts_.fonts[9]));return true;
}
bool FrontendLicenseWidgets::construct_window(std::size_t off,unsigned& repeat){
    if(off>object_.size()||object_.size()-off<PcFrontendWindowBytes||windows_.count(off))return false;
    std::array<std::uint8_t,PcFrontendWindowBytes> data;
    for(std::size_t i=0;i<data.size();++i)data[i]=object_.u8(off+i);
    if(!title_controller_construct_48c490(data.data(),data.size(),repeat))return false;
    for(std::size_t i=0;i<data.size();++i)object_.put8(off+i,data[i]);
    for(unsigned child:{0x48u,0x4d4u,0x960u,0xdecu})widgets_.insert(off+child);
    windows_.insert(off);return true;
}
bool FrontendLicenseWidgets::open_context_4e12b0(){
    if(object_.size()<PcLicenseChooserBytes||!windows_.count(0x9d54))return false;
    auto* l=list(0xb004);if(!l)return false;
    const auto* title=text_.get(0x3a6);if(!title)return false;
    const unsigned ids[]{0x233,0x237,0x234,0x2d7,0x2d8,0x2da,0x2d9};
    std::array<std::string_view,7> labels;
    for(unsigned i=0;i<7;++i){const auto* t=text_.get(ids[i]);if(!t)return false;labels[i]=*t;}
    auto b=object_;const int selected=b.i32(0x44);if(selected<0||selected>=4)return false;
    b.put8(0x36,0);auto w=b.sub(0x9d54,PcFrontendWindowBytes);
    if(!frontend_window_init_48d0c0(w,fonts_,text_,lines_,*title,"",0,120,110,380,190,4,0))return false;
    // This table is not dereferenced for text rows; retain its PC identity.
    if(!l->initialize_4ecfb0(0x5cdc20,{},float(w.i16(0x36)-37),1,0,0))return false;
    auto header=b.sub(0xb004,PcFrontendListBytes);header.put8(0x13,0);
    header.putf(0x20,w.f32(0x1280)+17);header.putf(0x24,w.f32(0x1284)+38);
    header.putf(0x1c,17);header.put32(0x28,5);
    for(const auto& label:labels){unsigned index{};if(!l->add_text_4ed160(&label,0,index))return false;}
    const bool special=b.u8(0x9c9c+12*selected)!=0;
    const bool visible[]{!special,!special,!special,b.u8(0x1320)!=0,b.u8(0x1321)!=0,b.u8(0x1322)!=0,b.u8(0x1322)!=0};
    // Hide order matters: 4ED360 changes selection as it disables each row.
    // If no row remains visible, 4ED8D0 preserves that intermediate selection.
    for(unsigned i:{4u,3u,5u,6u,0u,2u,1u})if(!visible[i]&&!l->set_enabled_4ed300(i,true))return false;
    int width=0;FrontendTextStyle style;style.flags=0x100;
    try{for(unsigned i=0;i<7;++i){
        if(visible[i])width=std::max(width,frontend_text_width_42c480(fonts_.fonts[9],style,labels[i]));
    }}catch(const std::out_of_range&){return false;}
    width=std::clamp(width+50,320,630);
    // 48D4E0 instant move then 48CAC0 height, without a premature layout tick.
    w.putf(0x1280,float(320-width/2));w.putf(0x1284,110);w.putf(0x1288,0);
    w.putf(0x128c,w.f32(0x1280));w.putf(0x1290,110);w.putf(0x1294,0);
    w.put32(0x1298,0);w.put32(0x129c,0);w.put32(0x12a0,0);w.put8(0x12a4,1);
    w.put16(0x36,std::uint16_t(width));w.put16(0x38,std::uint16_t(l->height_count_4ed810()*17+80));
    header.putf(0x20,w.f32(0x1280)+17);header.putf(0x24,w.f32(0x1284)+44);header.putf(0x18,float(width-37));
    l->reset_selection_4ed8d0();return true;
}
bool FrontendLicenseWidgets::close_context_4e1680(){
    if(!windows_.count(0x9d54)||!list(0xb004))return false;
    frontend_window_suspend_48ca30(object_.sub(0x9d54,PcFrontendWindowBytes));return list(0xb004)->clear_4eda60();
}
bool FrontendLicenseWidgets::construct_choice_list(std::size_t off,unsigned& repeat){
    if(off>object_.size()||object_.size()-off<PcFrontendChoiceListBytes||choice_lists_.count(off))return false;
    choice_lists_.emplace(off,std::make_unique<FrontendChoiceList>(object_.sub(off,PcFrontendChoiceListBytes),ui_,fonts_));
    input_repeat_=&repeat;return true;
}
bool FrontendLicenseWidgets::open_delete_4e16a0(){
    if(object_.size()<PcLicenseChooserBytes||!windows_.count(0xb040)||!input_repeat_)return false;
    auto* list=choice_list(0xc2f0);if(!list||list->size())return false;
    const auto* title=text_.get(0x3a6);const auto* body=text_.get(0x3a7);
    const auto* no=text_.get(0x1f2);const auto* yes=text_.get(0x1f1);
    if(!title||!body||!no||!yes)return false;
    if(!frontend_window_init_48d0c0(object_.sub(0xb040,PcFrontendWindowBytes),fonts_,text_,lines_,*title,*body,0,120,110,380,190,4,0))return false;
    std::array<std::uint8_t,0x438> data{};Bytes format(data.data(),data.size());frontend_text_format_construct_48d570(format);
    format.put32(0x400,9);format.put32(0x404,6);format.put8(0x41c,0);format.put32(0x420,0xff3f474a);
    format.putf(0x42c,-122);format.putf(0x430,-8);
    const std::vector<std::array<unsigned,3>> table{{{0x440004,0,11}},{{0x440004,11,11}},{{0x440004,12,12}},{{0x440004,11,0}}};
    if(!list->initialize_48d970(0x5cdc20,table,1,2,1,0,96,&format,1))return false;
    auto header=object_.sub(0xc2f0,PcFrontendChoiceListBytes);header.put8(0x1b,0);header.putf(0x24,0);header.putf(0x28,-30);header.putf(0x20,17);header.put32(0x2c,5);
    unsigned index{};if(!list->add_text_48db30(*no,*input_repeat_,index)||!list->add_text_48db30(*yes,*input_repeat_,index))return false;
    list->reset_selection_48e2b0();return true;
}
bool FrontendLicenseWidgets::control_delete_4e17e0(unsigned& result){
    result=0;auto* list=choice_list(0xc2f0);if(!list)return false;
    unsigned input{},arg=1;if(!call(0x48f5f0,0,&arg,1,input))return false;
    switch(static_cast<int>(input)){
    case 0:object_.put8(0xb016,0);result=object_.i32(0xc2f0)?1u:~0u;return true;
    case 1:object_.put8(0xb016,0);result=~0u;return true;
    case 2:case 4:{bool sound{};if(!list->move(input==4,sound))return false;
        if(sound){const unsigned id=1;unsigned ignored{};return external_call_(external_user_,services_,0x4249f0,0,&id,1,ignored);}return true;}
    default:return true;
    }
}
bool FrontendLicenseWidgets::display_delete_4e1860(){
    auto* choices=choice_list(0xc2f0);auto* context=list(0xb004);
    if(!windows_.count(0xb040)||!choices||!context||!list_timer_ready_)return false;
    return frontend_window_display_48c5f0(object_.sub(0xb040,PcFrontendWindowBytes),fonts_,lines_,glyphs_,images_,icons_)&&
        choices->display_48dda0(list_timer_,lines_,glyphs_)&&context->display_4ed3e0(list_timer_,glyphs_,images_);
}
bool FrontendLicenseWidgets::initialize_panels_4e0c70(){
    if(object_.size()<PcLicenseChooserBytes)return false;
    for(unsigned i=0;i<3;++i){const auto panel=0x3580+i*0x225c;
        for(unsigned j=0;j<7;++j){const auto off=panel+8+j*0x48c;if(!widgets_.count(off))return false;
            auto w=object_.sub(off,PcTextWidgetBytes);frontend_text_init_48e640(w);w.put32(0x454,3);}
        object_.put32(panel+0x1fd0,2);object_.put32(panel,0);object_.put32(panel+4,0);
    }return true;
}
bool FrontendLicenseWidgets::release_panels_4e0d80(){
    if(object_.size()<PcLicenseChooserBytes)return false;
    for(unsigned i=0;i<3;++i)for(unsigned j=0;j<4;++j){const auto off=0x3580+i*0x225c+0x1fdc+j*0xa0;
        unsigned result{};if(!resources_.count(off)||!ui_.call(0x465250,object_.sub(off,0xa0),nullptr,0,result))return false;
    }return true;
}
bool FrontendLicenseWidgets::populate_panels_4e2150(){
    if(object_.size()<PcLicenseChooserBytes||!profiles_)return false;
    auto b=object_;const int selected=b.i32(0x44),count=b.i32(0x40),fresh=b.i32(0x3c);
    if(selected<0||selected>=count||count>4)return false;
    auto set=[&](std::size_t off,std::string_view value,unsigned color=0xff3f474a){
        return widgets_.count(off)&&frontend_text_set_48f280(b.sub(off,PcTextWidgetBytes),value,9,color);
    };
    auto sprite=[&](unsigned off,unsigned token,unsigned frame,float x,float y){
        auto bits=[](float f){unsigned u;std::memcpy(&u,&f,4);return u;};
        const unsigned args[]={token,frame,frame,2,0,bits(x),bits(y),bits(1),bits(1),bits(1),0};unsigned r{};
        return resources_.count(off)&&ui_.call(0x465860,b.sub(off,0xa0),args,11,r)&&
            ui_.call(0x465970,b.sub(off,0xa0),nullptr,0,r);
    };
    for(int i=0;i<3;++i){
        const int index=selected+i;const unsigned w=0x3588+i*0x225c;
        if(index==fresh){
            const auto* label=text_.get(0x218);if(!label||!set(w,*label,~0u))return false;
            const char* values[]={"0.00%","?","?","0","0",""};
            for(unsigned j=0;j<6;++j)if(!set(w+(j+1)*0x48c,values[j]))return false;
        }else{
            if(index>=count)break;
            const auto slot=b.u32(0x9c94+12*index);if(slot>=4)return false;
            // 4E1BB0 copies the live active record back into the selected bank.
            if(slot==profiles_->selected)profiles_->licenses[slot]=profiles_->active;
            auto& record=profiles_->licenses[slot];Bytes p(record.data(),record.size());
            double completion{};
            if(!progress_call_||!progress_call_(progress_user_,record,completion)||!std::isfinite(completion)){
                services_.missing_pc=0x447400;return false;
            }
            const auto end=std::find(record.begin(),record.end(),0);
            if(!set(w,std::string_view(reinterpret_cast<const char*>(record.data()),std::size_t(end-record.begin())),~0u))return false;
            char value[128];std::snprintf(value,sizeof(value),"%5.2f%%",completion);
            if(!set(w+0x48c,value))return false;
            if(p.f32(0x108)<=0){
                if(!set(w+0x918,"?")||!set(w+0xda4,"?"))return false;
            }else{
                static constexpr const char* ranks[]={"F","F+","E-","E","E+","D-","D","D+","C-","C","C+","B-",
                    "B","B+","A-","A","A+","A++","AA","AA+","AA++","AAA","AAA+","AAA++"};
                const auto rank=p.u32(0x118);const float score=p.f32(0x110);
                if(rank>=24||!std::isfinite(score)||double(score)<-2147483648.||double(score)>=2147483648.)return false;
                std::snprintf(value,sizeof(value),"%s (%d)",ranks[rank],int(score));
                if(!set(w+0x918,value))return false;
                const float ratio=(p.f32(0x104)/p.f32(0x108))*100.f;
                if(!std::isfinite(ratio))return false;
                std::snprintf(value,sizeof(value),"%5.2f%%",double(ratio));if(!set(w+0xda4,value))return false;
            }
            const auto time=license_time_449b30(p.u32(0x10c));
            std::snprintf(value,sizeof(value),"%2d''%02d'%02d",int(time[0]),int(time[1]),int(time[2]));
            if(!set(w+0x1230,value)||!std::isfinite(p.f32(0x24)))return false;
            std::snprintf(value,sizeof(value),"%.0f",double(p.f32(0x24)));if(!set(w+0x16bc,value))return false;
            for(unsigned j=0;j<6;++j)b.put32(w+j*0x48c+0x454,3);
            const float x=float(i*480);
            if(!sprite(w+0x1fd4,0x44006a,p.u32(0x20),x-120,-8)||
               !sprite(w+0x2074,0x440077,p.u32(0x1c),x-137,44)||
               !sprite(w+0x2114,0x440078,p.u32(0x18),x-103,44))return false;
        }
    }
    for(auto off:{0xa08u,0xe94u}){if(!widgets_.count(off))return false;frontend_text_init_48e640(b.sub(off,PcTextWidgetBytes));}
    b.put32(0xa08+0x454,3);if(!set(0xa08," "))return false;
    const unsigned xy[]={0x43c80000,0x42c80000};
    if(!frontend_text_setter(0x48e530,b.sub(0xa08,PcTextWidgetBytes),xy,2))return false;
    b.put8(0x1320,0);b.put8(0x1321,0);b.put8(0x1322,0);
    if(selected!=fresh){
        const auto off=0x9c94+12*selected;
        if(b.u8(off+8))b.put8(0x1321,1);
        else if(b.i32(off+4)>=0)b.put8(0x1322,1);
    }
    return true;
}
bool FrontendLicenseWidgets::display_panel_4e2640(){
    if(object_.size()<PcLicenseChooserBytes||!profiles_)return false;
    auto b=object_;const int selected=b.i32(0x44);
    if(selected<0||selected>=b.i32(0x40)||selected>=4)return false;
    b.putf(0x3580,80);b.putf(0x3584,60);
    auto position=[&](unsigned off,float x,float y){
        if(!widgets_.count(off))return false;
        unsigned args[2];std::memcpy(args,&x,4);std::memcpy(args+1,&y,4);
        return frontend_text_setter(0x48e530,b.sub(off,PcTextWidgetBytes),args,2);
    };
    auto draw=[&](unsigned off){return widgets_.count(off)&&frontend_text_display_48f3c0(b.sub(off,PcTextWidgetBytes),fonts_,lines_,glyphs_);};
    auto tick=[&](unsigned off){unsigned r{};return resources_.count(off)&&ui_.call(0x4659f0,b.sub(off,0xa0),nullptr,0,r);};
    if(selected==b.i32(0x3c)){
        if(!position(0x3a14,359,209)||!position(0x432c,364,255)||!position(0x47b8,359,278))return false;
    }else{
        const auto slot=b.u32(0x9c94+12*selected);if(slot>=4)return false;
        if(slot==profiles_->selected)profiles_->licenses[slot]=profiles_->active;
        // The PC compares +108 here, but both outcomes share the same layout.
        if(!position(0x432c,359,255)||!position(0x3a14,354,209)||!position(0x47b8,354,278))return false;
    }
    if(!position(0x3588,202,167)||!draw(0x3588)||!draw(0x3a14)||
       !position(0x3ea0,359,232)||!draw(0x3ea0)||!draw(0x432c)||!draw(0x47b8)||
       !position(0x4c44,210,311)||!draw(0x4c44)||!tick(0x555c)||!tick(0x569c)||!tick(0x55fc)||
       !position(0x50d0,470,143)||!draw(0x50d0)||!tick(0x573c))return false;
    return true;
}
const char* license_rank_4eeee0(float score){
    // Exact single-precision thresholds at PC 5CED30, strict greater-than.
    static constexpr unsigned bits[]{0x443ca2e9,0x44511746,0x44658ba3,0x447a0000,
        0x44873a2f,0x4491745d,0x449bae8c,0x44a5e8ba,0x44b022e9,0x44ba5d17,
        0x44c49746,0x44ced174,0x44d90ba3,0x44e345d1,0x44ed8000,0x44f7ba2f,
        0x4500fa2f,0x45061746,0x450b345d,0x45105174,0x45156e8c,0x451a8ba3,
        0x451fa8ba,0x49742400};
    static constexpr const char* names[]{"F","F+","E-","E","E+","D-","D","D+","C-","C","C+","B-",
        "B","B+","A-","A","A+","A++","AA","AA+","AA++","AAA","AAA+","AAA++"};
    for(unsigned i=0;i<24;++i){float threshold;std::memcpy(&threshold,bits+i,4);if(threshold>score)return names[i];}
    return nullptr; // PC table[-1] == NULL, including unordered comparisons.
}
bool FrontendLicenseWidgets::populate_special_4e0dd0(){
    if(object_.size()<PcLicenseChooserBytes||!special_source_){services_.missing_pc=0x4e0dd0;return false;}
    auto b=object_;
    for(unsigned i=0;i<6;++i){const auto off=0x132c+i*0x48c;if(!widgets_.count(off))return false;
        frontend_text_init_48e640(b.sub(off,PcTextWidgetBytes));}
    const auto& source=*special_source_;
    if(source.selected<0||!source.manager)return true;
    const auto* manager=driving::native_handle_find(source.handles,source.manager);
    const auto off=std::uint64_t(0x18)+std::uint64_t(source.selected)*4;
    if(!manager||!manager->object||off+4>manager->size){services_.missing_pc=0x4e0dd0;return false;}
    const auto token=Bytes(manager->object,manager->size).u32(std::size_t(off));if(!token)return true;
    const auto* record=driving::native_handle_find(source.handles,token);
    if(!record||!record->object||record->size<0x94){services_.missing_pc=0x4e0dd0;return false;}
    const auto p=Bytes(record->object,record->size).sub(0x20,0x74);
    std::string name;for(unsigned i=0;i<0x40&&p.u8(i);++i)name.push_back(char(p.u8(i)));
    if(name.size()==0x40)return false; // Never scan outside the bounded name.
    for(unsigned field:{0x40u,0x44u,0x48u,0x60u})if(!std::isfinite(p.f32(field)))return false;
    auto set=[&](unsigned i,std::string_view value,unsigned color=0xff3f474a){
        return frontend_text_set_48f280(b.sub(0x132c+i*0x48c,PcTextWidgetBytes),value,9,color);};
    if(!set(0,name,~0u))return false;
    char text[128];std::snprintf(text,sizeof(text),"%5.2f%%",double(p.f32(0x40)));if(!set(1,text))return false;
    const float score=p.f32(0x60);const char* rank=license_rank_4eeee0(score);
    const int integer=double(score)<-2147483648.||double(score)>=2147483648.?(-2147483647-1):int(score);
    std::snprintf(text,sizeof(text),"%s (%d)",rank?rank:"(null)",integer);if(!set(2,text))return false;
    std::snprintf(text,sizeof(text),"%5.2f%%",double(p.f32(0x44)));if(!set(3,text))return false;
    const auto time=license_time_449b30(p.u32(0x64));
    std::snprintf(text,sizeof(text),"%2d''%02d'%02d",int(time[0]),int(time[1]),int(time[2]));if(!set(4,text))return false;
    std::snprintf(text,sizeof(text),"%.0f",double(p.f32(0x48)));if(!set(5,text))return false;
    for(unsigned i=0;i<6;++i)b.put32(0x132c+i*0x48c+0x454,3);
    constexpr unsigned fields[]{0x6c,0x68,0x70},tokens[]{0x44006a,0x440077,0x440078};
    constexpr float x[]{-120,-137,-103},y[]{-8,44,44};
    for(unsigned i=0;i<3;++i){const auto at=0x3300+i*0xa0;if(!resources_.count(at))return false;
        unsigned xb,yb;std::memcpy(&xb,x+i,4);std::memcpy(&yb,y+i,4);const unsigned frame=p.u32(fields[i]);
        const unsigned args[]{tokens[i],frame,frame,2,0,xb,yb,0x3f800000,0x3f800000,0x3f800000,0};unsigned result{};
        if(!ui_.call(0x465860,b.sub(at,0xa0),args,11,result)||!ui_.call(0x465970,b.sub(at,0xa0),nullptr,0,result))return false;
    }return true;
}
bool FrontendLicenseWidgets::display_special_4e1060(){
    if(object_.size()<PcLicenseChooserBytes)return false;
    auto b=object_;b.putf(0x1324,80);b.putf(0x1328,60);
    const float x[]{202,b.u32(0x44)==b.u32(0x3c)?359.f:354.f,359,359,359,210};
    constexpr float y[]{167,209,232,255,278,311};
    for(unsigned i=0;i<6;++i){const auto at=0x132c+i*0x48c;if(!widgets_.count(at))return false;
        unsigned args[2];std::memcpy(args,x+i,4);std::memcpy(args+1,y+i,4);
        if(!frontend_text_setter(0x48e530,b.sub(at,PcTextWidgetBytes),args,2)||
           !frontend_text_display_48f3c0(b.sub(at,PcTextWidgetBytes),fonts_,lines_,glyphs_))return false;
    }
    for(unsigned at:{0x3300u,0x3440u,0x33a0u}){unsigned result{};
        if(!resources_.count(at)||!ui_.call(0x4659f0,b.sub(at,0xa0),nullptr,0,result))return false;}
    return true;
}
bool FrontendLicenseWidgets::display_chooser_4e3240(){
    if(object_.size()<PcLicenseChooserBytes)return false;
    unsigned r{};
    switch(object_.u32(0x38)){
    case 3:return display_panel_4e2640();
    case 4:
        if(!display_panel_4e2640())return false;
        if(object_.u8(0x36))return call(0x4e1860,0,nullptr,0,r);
        return call(12,0x9d54,nullptr,0,r)&&call(0x4ed3e0,0xb004,nullptr,0,r);
    case 11:return call(0x4e1060,0,nullptr,0,r);
    default:return true;
    }
}
bool FrontendLicenseWidgets::call(unsigned pc,std::size_t off,const unsigned* a,std::size_t n,unsigned& result){
    result=0;if(n&&!a)return false;
    if(pc==0x48f5f0){
        if(!input_snapshot_)return external_call_&&external_call_(external_user_,services_,pc,off,a,n,result);
        if(n!=1||!input_repeat_||off>object_.size()||object_.size()-off<0x34)return false;
        std::array<std::uint8_t,0x34> data;
        for(unsigned i=0;i<data.size();++i)data[i]=object_.u8(off+i);
        const bool ok=frontend_input_action_48f5f0(data.data(),data.size(),*input_snapshot_,
            static_cast<std::int32_t>(a[0]),*input_repeat_,this,
            [](void* p,unsigned key,int argument){
                auto& w=*static_cast<FrontendLicenseWidgets*>(p);unsigned ignored{};
                const unsigned args[]{key,unsigned(argument)};
                if(!w.external_call_){w.services_.missing_pc=0x440ed0;return false;}
                const bool ok=w.external_call_(w.external_user_,w.services_,0x440ed0,0,args,2,ignored);
                if(!ok&&!w.services_.missing_pc)w.services_.missing_pc=0x440ed0;
                return ok;
            },result);
        for(unsigned i=0;i<data.size();++i)object_.put8(off+i,data[i]);
        return ok;
    }
    if(chooser_constructed_&&off==0&&n==0&&
       (pc==4||pc==8||pc==12||pc==16||pc==0x4e19e0||pc==0x4e1a70||pc==0x4e1ac0||pc==0x4e2970)){
        if(!profiles_||!chooser_state_)return false;
        if(pc==4||pc==0x4e19e0){const bool ok=license_chooser_init_4e19e0(object_,*profiles_,*chooser_state_,services_);result=ok;return ok;}
        if(pc==8||pc==0x4e2970)return license_chooser_tick_4e2970(object_,*profiles_,*chooser_state_,services_,result);
        if(pc==12)return display_chooser_4e3240();
        if(pc==0x4e1a70)return license_chooser_reset_4e1a70(object_,*profiles_,*chooser_state_);
        return license_chooser_suspend_4e1ac0(object_,*profiles_,*chooser_state_,services_);
    }
    if(keyboard_constructed_&&off==0xa78){
        auto b=object_.sub(off,PcKeyboardBytes);
        if(pc==4&&n==0){const bool ok=keyboard_init_468880(b,ui_,keyboard_);result=ok;return ok;}
        if(pc==8&&n==0)return keyboard_tick_469130(b,ui_,keyboard_,result);
        if(pc==16&&n==0)return keyboard_suspend_468f20(b,ui_,keyboard_);
        if(pc==0x468780&&n==2){keyboard_limits_468780(b,static_cast<int>(a[0]),static_cast<int>(a[1]));return true;}
        if(pc==0x468710&&n==0){const auto end=std::find(services_.name_text.begin(),services_.name_text.end(),0);
            return keyboard_name_468710(b,std::string_view(reinterpret_cast<const char*>(services_.name_text.data()),std::size_t(end-services_.name_text.begin())));}
        if(pc==0x468770&&n==0){const auto s=keyboard_name_468770(b);services_.name_text.fill(0);
            std::copy_n(s.begin(),std::min(s.size(),services_.name_text.size()),services_.name_text.begin());return true;}
        if(pc==0x4687c0&&n==2){float x,y;std::memcpy(&x,a,4);std::memcpy(&y,a+1,4);return keyboard_position_4687c0(b,ui_,x,y);}
        if(pc==0x468d70&&n==1){b.put8(0x44,std::uint8_t(a[0]));return true;}
        if(pc==0x468870&&n==1){b.put32(0x708,a[0]);return true;}
    }
    if(pc==0x465eb0&&off==0&&n==1){const auto* s=text_.get(a[0]);if(!s)return false;
        services_.localized_text=*s;services_.name_text.fill(0);
        std::copy_n(s->begin(),std::min(s->size(),services_.name_text.size()),services_.name_text.begin());return true;}
    if(pc==0x4e0c70&&off==0&&n==0)return initialize_panels_4e0c70();
    if(pc==0x4e0d80&&off==0&&n==0)return release_panels_4e0d80();
    if(pc==0x4e2150&&off==0&&n==0)return populate_panels_4e2150();
    if(pc==0x4e0dd0&&off==0&&n==0)return populate_special_4e0dd0();
    if(pc==0x4e1060&&off==0&&n==0)return display_special_4e1060();
    if(pc==0x4e2640&&off==0&&n==0)return display_panel_4e2640();
    if(pc==0x4e3240&&off==0&&n==0)return display_chooser_4e3240();
    if(pc==0x4e12b0&&off==0&&n==0)return open_context_4e12b0();
    if(pc==0x4e1680&&off==0&&n==0)return close_context_4e1680();
    if(pc==0x4e16a0&&off==0&&n==0)return open_delete_4e16a0();
    if(pc==0x4e17e0&&off==0&&n==0)return control_delete_4e17e0(result);
    if(pc==0x4e1860&&off==0&&n==0)return display_delete_4e1860();
    if(auto* choices=choice_list(off)){
        if(pc==0x48e440&&n==0)return choices->clear_48e440();
        if(pc==0x48dda0&&n==0)return list_timer_ready_&&choices->display_48dda0(list_timer_,lines_,glyphs_);
        if(pc==0x48e2b0&&n==0){choices->reset_selection_48e2b0();return true;}
        return false;
    }
    if(windows_.count(off)){
        auto w=object_.sub(off,PcFrontendWindowBytes);
        if(pc==12&&n==0)return frontend_window_display_48c5f0(w,fonts_,lines_,glyphs_,images_,icons_);
        if(pc==16&&n==0){frontend_window_suspend_48ca30(w);return true;}
        if((pc==4||pc==8)&&n==0){
            if(pc==8&&!owner_clock_ready_)return false;
            std::array<std::uint8_t,PcFrontendWindowBytes> data;
            for(std::size_t i=0;i<data.size();++i)data[i]=w.u8(i);
            struct Context{FrontendLicenseWidgets* owner;std::size_t offset;} ctx{this,off};
            TitleControllerServices svc;svc.user=&ctx;svc.root_state=root_state_;
            svc.call=[](void* p,unsigned entry,std::uint8_t* object,std::size_t child,int arg,unsigned& r){
                auto& c=*static_cast<Context*>(p);auto& self=*c.owner;
                if(entry==0x48cc00)return title_controller_motion_48cc00(object,PcFrontendWindowBytes,self.owner_delta_);
                if(entry==0x47f110){r=0;return true;} // Original shared empty virtual tick.
                const unsigned a=unsigned(arg);
                // Input repeat lives inside this same object. Let the platform
                // adapter observe prior motion and retain its field updates.
                for(std::size_t i=0;i<PcFrontendWindowBytes;++i)self.object_.put8(c.offset+i,object[i]);
                const bool ok=self.call(entry,c.offset+child,&a,1,r);
                for(std::size_t i=0;i<PcFrontendWindowBytes;++i)object[i]=self.object_.u8(c.offset+i);
                return ok;
            };
            const bool ok=pc==4?title_controller_init_48c5b0(data.data(),data.size()):title_controller_tick_48d420(data.data(),data.size(),svc,result);
            for(std::size_t i=0;i<data.size();++i)w.put8(i,data[i]);
            return ok;
        }
        return false;
    }
    if(auto* l=list(off)){
        if(pc==0x4ed3e0&&n==0)return list_timer_ready_&&l->display_4ed3e0(list_timer_,glyphs_,images_);
        if(pc==0x4eda60&&n==0)return l->clear_4eda60();
        if(pc==0x4ed810&&n==0){result=unsigned(l->height_count_4ed810());return true;}
        if(pc==0x4ed7c0&&n==0){result=unsigned(l->visible_index_4ed7c0());return true;}
        if(pc==0x4ed8d0&&n==0){l->reset_selection_4ed8d0();return true;}
        if(pc==0x4ed930&&n==1){result=l->select_4ed930(static_cast<int>(a[0]));return true;}
        if((pc==0x4ed300||pc==0x4ed360)&&n==1)return l->set_enabled_4ed300(a[0],pc==0x4ed360);
        if(pc==0x4ed390&&n==1)return l->show_4ed390(a[0]);
        if((pc==0x4ed250||pc==0x4ed2a0)&&n==0){bool sound{};if(!l->move(pc==0x4ed2a0,sound))return false;
            if(!sound)return true;
            const unsigned id=1;return external_call_&&external_call_(external_user_,services_,0x4249f0,0,&id,1,result);}
        if(l->setter(pc,a,n))return true;
        // An owned list must not fall through to a diagnostic external stub.
        return false;
    }
    if(resources_.count(off)&&pc>=0x465160&&pc<=0x4659f0)return ui_.call(pc,object_.sub(off,0xa0),a,n,result);
    if(widgets_.count(off)){
        const auto w=object_.sub(off,PcTextWidgetBytes);
        if(pc==4&&n==0){frontend_text_init_48e640(w);return true;}
        if(pc==12&&n==0)return frontend_text_display_48f3c0(w,fonts_,lines_,glyphs_);
        if(pc==0x48f280&&n==3&&a[0]==0x626468){
            const auto end=std::find(services_.name_text.begin(),services_.name_text.end(),0);
            return frontend_text_set_48f280(w,std::string_view(reinterpret_cast<const char*>(services_.name_text.data()),std::size_t(end-services_.name_text.begin())),a[1],a[2]);
        }
        if(pc==0x48eec0&&n==0){const auto s=frontend_text_value_48eec0(w);services_.name_text.fill(0);
            std::copy_n(s.begin(),std::min(s.size(),services_.name_text.size()),services_.name_text.begin());return true;}
        if(pc==0x48ee80&&n==0){
            const auto end=std::find(services_.name_text.begin(),services_.name_text.end(),0);
            return frontend_text_set_48f280(w,std::string_view(reinterpret_cast<const char*>(services_.name_text.data()),std::size_t(end-services_.name_text.begin())),w.u32(0x450),w.u32(0x474));
        }
        if(frontend_text_setter(pc,w,a,n))return true;
    }
    if(!external_call_)return false;
    return external_call_(external_user_,services_,pc,off,a,n,result);
}
}
