#include "platform/frontend_ui_resources.hpp"
#include "platform/title_owner.hpp"
#include "platform/pc_address_view.hpp"
#include <algorithm>
#include <cmath>
#include <cstring>
namespace outrun::platform {
driving::PcUiNotifyServices FrontendUiResources::notify(){
    driving::PcUiNotifyServices s{};s.user=this;
    s.handle_void=[](void* p,std::uint32_t pc,std::uint32_t h){
        static_cast<FrontendUiResources*>(p)->sprite_call(pc,&h,1);};
    s.handle_i32=[](void* p,std::uint32_t pc,std::uint32_t h){
        return static_cast<std::int32_t>(static_cast<FrontendUiResources*>(p)->sprite_call(pc,&h,1));};
    return s;
}
driving::PcUiResourceCommitServices465970 FrontendUiResources::commit(){
    return {this,[](void* p,std::uint32_t pc,const std::uint32_t* a,std::size_t n){
        return static_cast<FrontendUiResources*>(p)->sprite_call(pc,a,n);},
        [](void* p,std::uint32_t,driving::Bytes r){static_cast<FrontendUiResources*>(p)->finalize(r);}};
}
driving::PcUiResourceTickServices4659f0 FrontendUiResources::ticks(){
    return {this,[](void* p,std::uint32_t pc,driving::Bytes r){
        std::uint32_t ignored{};static_cast<FrontendUiResources*>(p)->call(pc,r,nullptr,0,ignored);}};
}
driving::PcEmbeddedSlotsServices446cf0 FrontendUiResources::embedded_slots(){
    return {this,notify(),ticks(),
        [](void* p,unsigned pc,driving::Bytes child,unsigned token,unsigned,unsigned x,unsigned y){
            auto& ui=*static_cast<FrontendUiResources*>(p);
            if(pc!=0x446a80&&pc!=0x446b20&&pc!=0x446c50){ui.missing_pc=pc;return;}
            const unsigned args[]{token,pc==0x446a80?0u:25u,pc==0x446c50?0u:25u,14,
                pc==0x446b20?0u:3u,x,y,0x3f800000,0x3f800000,pc==0x446c50?0xbf800000u:0x3f800000u,0};
            unsigned result{};ui.call(0x465860,child,args,11,result);
        },[](void* p,unsigned pc,driving::Bytes child){unsigned result{};auto& ui=*static_cast<FrontendUiResources*>(p);
            if(!ui.missing_pc)ui.call(pc,child,nullptr,0,result);}};
}
bool FrontendUiResources::input_feedback(driving::Bytes root,unsigned key,std::int32_t argument){
    if(root.size()<0x21c){missing_pc=0x440ed0;return false;}
    if(root.u32(0x218)!=2)return true; // Original 440ED0 gate.
    if(root.size()<0x51c+0x68d){missing_pc=0x446f30;return false;}
    auto embedded=root.sub(0x51c,root.size()-0x51c);
    if(embedded.u32(0x408)>=32){missing_pc=0x446f30;return false;}
    driving::PcEmbeddedConfigServices446f30 svc{};svc.user=this;svc.ui_reset_services=notify();
    svc.configure_ui=[](void* p,driving::Bytes child,unsigned token,unsigned,unsigned x,unsigned y){
        auto& ui=*static_cast<FrontendUiResources*>(p);unsigned ignored{};
        const unsigned args[]{token,25,50,14,1,x,y,0x3f800000,0x3f800000,0x3f800000,0};
        if(!ui.missing_pc)ui.call(0x465860,child,args,11,ignored);
    };
    svc.finalize_ui=[](void* p,driving::Bytes child){auto& ui=*static_cast<FrontendUiResources*>(p);unsigned ignored{};
        if(!ui.missing_pc)ui.call(0x465970,child,nullptr,0,ignored);};
    svc.global_effect=[](void* p,unsigned value){auto& ui=*static_cast<FrontendUiResources*>(p);
        if(!ui.missing_pc&&(!ui.effect_4249f0||!ui.effect_4249f0(ui.effect_user,value)))ui.missing_pc=0x4249f0;};
    driving::embedded_configure_446f30(embedded,key,static_cast<std::int8_t>(argument),svc);
    return missing_pc==0;
}
bool FrontendUiResources::commands(std::uint32_t pc,driving::Bytes b,const std::uint32_t* a,
    std::size_t n,driving::PcUiNotifyGlobals& g,std::uint32_t& result){
    result=0;
    if(pc==0x442ac0&&n==0&&b.size()>=0x68c){
        driving::PcObjectInitServices init{};init.user=this;
        init.construct_array=[](void* p,std::uint32_t,driving::Bytes object,std::size_t offset,
            std::uint32_t size,std::uint32_t count,std::uint32_t ctor,std::uint32_t){
            auto& ui=*static_cast<FrontendUiResources*>(p);std::uint32_t ignored{};
            for(unsigned i=0;i<count;++i)ui.call(ctor,object.sub(offset+i*size,size),nullptr,0,ignored);
        };
        driving::object_block_init_442ac0(b,0,init);return missing_pc==0;
    }
    if(b.size()<0x68c||(n&&!a)||b.u32(0x408)>=32){missing_pc=pc;return false;}
    const auto s=embedded_slots();
    if((pc==0x440ea0||pc==0x446d90)&&n==8){
        std::array<std::uint32_t,8> args;std::copy(a,a+8,args.begin());
        result=driving::embedded_slots_set_446d90(b,args,g,s);
    }else if(pc==0x446ea0&&n==1)result=driving::embedded_slots_select_446ea0(b,a[0],g,s);
    else if(pc==0x446fc0&&n==0)driving::embedded_slots_push_446fc0(b,g,s);
    else if(pc==0x447000&&n==2)driving::embedded_slots_visible_447000(b,std::uint8_t(a[0]),std::uint8_t(a[1]),s);
    else if((pc==0x447090||pc==0x4470f0)&&n==0)driving::embedded_slots_clear_447090(b,g,s);
    else if(pc==0x446cf0&&n==0)driving::embedded_slots_tick_446cf0(b,g,s);
    else {missing_pc=pc;return false;}
    return missing_pc==0;
}
bool frontend_slot_captions_446a50(driving::Bytes embedded,const FrontendFontPack* fonts,
                                   const FrontendTextTable* table,std::vector<FrontendGlyph>& glyphs){
    embedded.check(0,0x68cu);
    if(!embedded.u8(0))return true;
    auto exe_u32=[](std::uint32_t a,std::uint32_t& v){
        for(std::size_t i=0;i<EmbeddedExeRangeCount;++i){const auto& r=EmbeddedExeRanges[i];
            if(r.data&&a>=r.base&&a+4u<=r.base+r.size){std::memcpy(&v,r.data+(a-r.base),4);return true;}}
        return false;};
    auto cvtt=[](float f){return (f>-2147483904.0f&&f<2147483648.0f)?std::int32_t(f):std::int32_t(0x80000000u);};
    for(std::uint32_t i=0;i<4u;++i){
        const std::uint32_t row=(i+embedded.u32(0x408u)*8u)*4u;
        if(embedded.u32(row+8u)==0xffffffffu||embedded.u32(row+0x18u)==0xffffffffu)continue;
        if(!fonts||fonts->fonts[9].token!=9u||!table)return false;
        std::uint32_t xb,yb,off;
        if(!exe_u32(0x59dc84u+i*8u,xb)||!exe_u32(0x59dc88u+i*8u,yb)||!exe_u32(0x59dca4u+i*4u,off))return false;
        float fx,fy;std::memcpy(&fx,&xb,4);std::memcpy(&fy,&yb,4);
        FrontendTextStyle style;style.scale_x=1.0f;style.scale_y=1.0f;style.mode=0xeu;style.color=0xffffffffu;
        std::int32_t x=cvtt(fx);
        if(i>1u){style.flags=0xau;x-=0x14;}else{style.flags=9u;x+=0x11;}   // 42C360: right / left, centred vertically
        x=x-std::int32_t(off)+0x140;
        FrontendTextCursor cursor;cursor.x=std::int16_t(x);cursor.y=std::int16_t(cvtt(fy)+0xf0);cursor.origin_x=cursor.x;
        const std::string* s=table->get(embedded.u32(row+0x18u));
        if(!s||!frontend_text_draw(fonts->fonts[9],style,cursor,*s,glyphs))return false;
    }
    return true;
}
std::uint32_t FrontendUiResources::sprite_call(std::uint32_t pc,const std::uint32_t* a,std::size_t n){
    if(n&&!a){missing_pc=pc;return ~0u;}
    if(pc==0x428320u&&n==3)return sprites.create(a[0],a[1],a[2],-1,-1,false,pause_domain);
    if(pc==0x428460u&&n==5)return sprites.create(a[0],a[1],a[2],static_cast<std::int32_t>(a[3]),static_cast<std::int32_t>(a[4]),true,pause_domain);
    if(pc==0x4285a0u&&n==1){sprites.release(a[0]);return 0;}
    if(pc==0x428880u&&n==1)return sprites.status(a[0]);
    if(pc==0x428800u&&n==2){float speed;std::memcpy(&speed,a+1,4);if(sprites.set_speed(a[0],speed))return 0;}
    if(pc==0x4288c0u&&n==2&&sprites.set_frame(a[0],static_cast<std::int32_t>(a[1])))return 0;
    missing_pc=pc;return ~0u;
}
bool FrontendUiResources::finalize(driving::Bytes r){
    if(r.size()<0xa0u){missing_pc=0x4656b0u;return false;}
    if(r.u32(0x9cu)!=0u){missing_pc=0x4656b0u;return false;} // explicit guest follow-pointer boundary
    if(r.u32(0x24u)!=0u)return true;
    if(r.u32(8u)==~0u)return true;
    std::array<float,16> matrix{};
    matrix[0]=r.f32(0x64u);matrix[5]=r.f32(0x68u);matrix[10]=r.f32(0x6cu);matrix[15]=1.0f;
    matrix[12]=r.f32(0x40u);matrix[13]=r.f32(0x44u);matrix[14]=r.f32(0x48u);
    if(sprites.set_matrix(r.u32(8u),matrix))return true;
    missing_pc=0x4656b0u;return false;
}
bool FrontendUiResources::call(std::uint32_t pc,driving::Bytes r,const std::uint32_t* a,
    std::size_t n,std::uint32_t& result){
    result=0;
    if(r.size()<0xa0u||(n&&!a)){missing_pc=pc;return false;}
    if(pc==0x465160u&&n==0){
        for(auto off:{8u,12u,16u})r.put32(off,~0u);
        for(auto off:{0x40u,0x44u,0x48u,0x4cu,0x50u,0x54u,0u,4u,0x1cu,0x20u,
                      0x24u,0x28u,0x88u,0x8cu,0x90u,0x94u,0x98u,0x9cu})r.put32(off,0);
        for(auto off:{0x64u,0x68u,0x6cu,0x70u,0x74u,0x78u})r.putf(off,1.0f);
        r.put32(0x18u,1u);return true;
    }
    if(pc==0x465250u&&n==0){driving::ui_resource_reset_465250(r,notify());return true;}
    if(pc==0x4652e0u&&n==0){result=driving::ui_resource_ready_4652e0(r,notify());return true;}
    if(pc==0x465860u&&n==11){
        std::array<std::uint32_t,11> args{};std::copy(a,a+11,args.begin());
        driving::ui_resource_configure_465860(r,args,notify());return true;
    }
    if(pc==0x465970u&&n==0){driving::ui_resource_commit_465970(r,commit());return missing_pc==0u;}
    if(pc==0x4659f0u&&n==0){driving::ui_resource_tick_4659f0(r,notify(),ticks());return missing_pc==0u;}
    if(pc==0x4656b0u&&n==0)return finalize(r);
    if((pc==0x465310u||pc==0x4653c0u)&&n==4){
        float x,y,duration;std::memcpy(&x,a,4);std::memcpy(&y,a+1,4);std::memcpy(&duration,a+2,4);
        if(!std::isfinite(x)||!std::isfinite(y)||!std::isfinite(duration)){missing_pc=pc;return false;}
        const bool scale=pc==0x465310u;
        const unsigned current=scale?0x64u:0x40u,target=current+12u,step=current+24u;
        r.put32(scale?0x8cu:0x88u,1);r.putf(step+8u,0);r.putf(target,x);r.putf(target+4,y);
        if(!scale)r.putf(target+8,0);
        if(duration==0){r.putf(current,x);r.putf(current+4,y);r.putf(current+8,0);
            if(scale)r.putf(target+8,0);
            r.putf(step,0);r.putf(step+4,0);
        }else{
            if(scale)r.putf(target+8,1);
            const float inverse=1.0f/duration;
            r.putf(step,(x-r.f32(current))*inverse);r.putf(step+4,(y-r.f32(current+4))*inverse);
        }
        r.put32(scale?0x94u:0x90u,a[3]);return true;
    }
    if((pc==0x465590u||pc==0x465460u)&&n==0){
        const bool scale=pc==0x465460u;
        const unsigned flag=scale?0x8cu:0x88u,current=scale?0x64u:0x40u;
        const unsigned target=current+12u,step=current+24u;
        if(r.u32(8)==~0u||!r.u32(flag))return true;
        if(!std::isfinite(motion_step)){missing_pc=pc;return false;}
        bool finished=true;
        for(unsigned axis=0;axis<3;++axis){
            const auto offset=axis*4u;const auto delta=r.f32(step+offset);
            const float value=r.f32(current+offset)+delta*motion_step;
            r.putf(current+offset,value);
            if(axis<2){const auto end=r.f32(target+offset);
                finished=finished&&(delta==0||(delta<0&&value<=end)||(delta>0&&value>=end));}
        }
        // PC waits for BOTH axes, then copies the complete target vector.
        if(finished){for(unsigned axis=0;axis<3;++axis)r.put32(current+axis*4,r.u32(target+axis*4));
            r.put32(flag,0);if(r.u32(scale?0x94u:0x90u))driving::ui_resource_reset_465250(r,notify());}
        return true;
    }
    // Follow-pointers remain an explicit guest-address boundary.
    missing_pc=pc;return false;
}
}
