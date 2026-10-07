#include "frontend_license_owners.hpp"
#include "native_runtime.hpp"
namespace outrun::platform {
using driving::Bytes;
namespace {unsigned index(unsigned key){return key==21?0:key==24?1:2;}}
FrontendLicenseOwners::FrontendLicenseOwners(NativeRuntimeContext& runtime,const FrontendFontPack& fonts,const FrontendTextTable& text)
    :runtime_(runtime),ui_{runtime.event_function36.frontend_sprites},fonts_(fonts),text_(text){}
FrontendLicenseOwners::~FrontendLicenseOwners(){reset();}
void FrontendLicenseOwners::persistence(std::string directory,bool enabled){
    save_directory_=std::move(directory);saves_enabled_=enabled;
}
bool FrontendLicenseOwners::save_active(){
    if(save_directory_.empty())return true;   // no write destination: the service is disabled
    const auto saved=frontend_profiles_save_416420(runtime_.event_function36.frontend_profiles,save_directory_,saves_enabled_);
    ++save_calls_;persistence_error_=saved.error;return true;
}
void FrontendLicenseOwners::platform(void* user,bool(*call)(void*,unsigned,const unsigned*,std::size_t,unsigned&)){
    platform_user_=user;platform_call_=call;
    ui_.effect_user=this;ui_.effect_4249f0=[](void* p,unsigned id){
        auto& self=*static_cast<FrontendLicenseOwners*>(p);unsigned result{};
        return self.platform_call_&&self.platform_call_(self.platform_user_,0x4249f0,&id,1,result);
    };
}
void FrontendLicenseOwners::reset(){
    widgets_[1].reset();widgets_[0].reset();keyboard_focus_=false;fault_={};editing_existing_=0;chooser_state_={};
    ui_.missing_pc=0;
}
void FrontendLicenseOwners::begin_frame(){for(auto& w:widgets_)if(w)w->begin_frame();}
void* FrontendLicenseOwners::storage(unsigned key){return key==21?chooser_.data():key==24?editor_.data():nullptr;}
std::size_t FrontendLicenseOwners::size(unsigned key)const{return key==21?chooser_.size():key==24?editor_.size():0;}
unsigned FrontendLicenseOwners::missing(unsigned key)const{return index(key)<2?fault_[index(key)]:0x443eb0;}
const FrontendLicenseWidgets* FrontendLicenseOwners::widgets(unsigned key)const{return index(key)<2?widgets_[index(key)].get():nullptr;}
bool FrontendLicenseOwners::construct(unsigned key){
    const auto i=index(key);if(i>=2)return false;
    widgets_[i].reset();if(key==24)keyboard_focus_=false;fault_[i]=0;
    auto& state=runtime_.event_function36;
    if(!state.frontend_profiles_initialized){fault_[i]=0x4162d0;return false;}
    auto w=std::make_unique<FrontendLicenseWidgets>(Bytes(storage(key),size(key)),ui_,fonts_,text_);
    w->profiles(state.frontend_profiles,&runtime_,[](void* p,const PcLicense& l,double& result){
        return native_runtime_license_completion(*static_cast<NativeRuntimeContext*>(p),l,result);
    });
    w->chooser_state(chooser_state_);w->input(state.frontend_input,state.title_base_global_6591e4);
    w->external(this,[](void* p,LicenseEditorServices& s,unsigned pc,std::size_t off,const unsigned* a,std::size_t n,unsigned& r){
        return static_cast<FrontendLicenseOwners*>(p)->external(s,pc,off,a,n,r);
    });
    // The original on-screen keyboard retains exclusive input focus; this is
    // native ownership, not a request to open an OS/software keyboard.
    w->keyboard_focus(this,[](void* p,bool active){auto& self=*static_cast<FrontendLicenseOwners*>(p);
        if(active&&self.keyboard_focus_)return false;
        self.keyboard_focus_=active;return true;
    });
    const bool ok=key==21?w->construct_chooser_4e1890(state.title_base_global_6591e4):
        w->construct_editor_4dd5c0(state.title_base_global_6591e4);
    if(!ok){fault_[i]=key==21?0x4e1890:0x4dd5c0;return false;}
    widgets_[i]=std::move(w);return true;
}
bool FrontendLicenseOwners::external(LicenseEditorServices& s,unsigned pc,std::size_t off,
    const unsigned* args,std::size_t count,unsigned& result){
    result=0;if(off||(!args&&count))return false;
    auto& state=runtime_.event_function36;Bytes root(state.object.data(),state.object.size());
    if((pc==0x416420||pc==0x4164d0)&&count==0&&!save_directory_.empty()){
        const auto saved=pc==0x416420?frontend_profiles_save_416420(state.frontend_profiles,save_directory_,saves_enabled_):
            frontend_profiles_save_common_4164d0(state.frontend_profiles,save_directory_);
        ++save_calls_;persistence_error_=saved.error;result=saved.pc_result;
        return true; // I/O failure is an original result, not an unbound call.
    }
    if(pc==0x416500&&count==1&&!save_directory_.empty()){
        unsigned slot_random{},active_random{};
        if(args[0]<4){
            slot_random=frontend_crt_random_580f40(state.pc_crt_random_state);
            if(args[0]==state.frontend_profiles.selected)active_random=frontend_crt_random_580f40(state.pc_crt_random_state);
        }
        result=frontend_profiles_delete_416500(state.frontend_profiles,args[0],save_directory_,slot_random,active_random,persistence_error_);
        ++delete_calls_;return true;
    }
    if(pc==0x442f20&&count==2){driving::object_store_depth_pair_442f20(root,std::uint8_t(args[0]),std::uint8_t(args[1]));return true;}
    if(pc==0x442ec0&&count==0){
        driving::PcObjectStateServices services{};services.user=&state;
        services.call_u32=[](void* p,unsigned,unsigned token){auto& state=*static_cast<NativeEventFunction36State*>(p);
            return driving::native_handle_state_564c90({state.frontend_handle_bindings.data(),state.frontend_handle_count},token);
        };
        result=driving::object_query_previous_state_442ec0(root,services);return true;
    }
    if(pc==0x440ea0){const bool ok=ui_.commands(pc,root.sub(0x51c,root.size()-0x51c),args,count,state.frontend_ui_globals,result);
        if(!ok)s.missing_pc=ui_.missing_pc;
        return ok;}
    if(pc==0x440ed0&&count==2){const bool ok=ui_.input_feedback(root,args[0],static_cast<int>(args[1]));
        if(!ok)s.missing_pc=ui_.missing_pc?ui_.missing_pc:0x440ed0;
        return ok;}
    return platform_call_&&platform_call_(platform_user_,pc,args,count,result);
}
bool FrontendLicenseOwners::invoke(unsigned key,unsigned slot,unsigned& result){
    result=0;const auto i=index(key);if(i>=2||!widgets_[i])return false;
    if(slot==0){widgets_[i].reset();if(key==24)keyboard_focus_=false;return true;}
    if(fault_[i])return false;
    auto& state=runtime_.event_function36;auto& w=*widgets_[i];auto& svc=w.services();
    Bytes root(state.object.data(),state.object.size()),object(storage(key),size(key));
    w.owner_clock(root.f32(0xda0),root.u32(0x218));
    w.list_timer(float(runtime_.frame_state.frame_counter_95af0c));
    ui_.motion_step=state.frontend_ui_motion_step;svc.editing_existing=editing_existing_;svc.missing_pc=0;
    bool ok=false;
    if(key==21)ok=svc.call(svc.user,slot,0,nullptr,0,result);
    else if(slot==4){
        ok=state.frontend_profiles.active_loaded&&license_editor_init_4dd7c0(object,state.frontend_profiles.active,svc);result=ok;
    }else if(slot==8)ok=license_editor_tick_4de2b0(object,state.frontend_profiles.active,svc,result);
    else if(slot==12||slot==16)ok=true; // 5CD9F8 +C/+10 both point to RET 49A650.
    editing_existing_=svc.editing_existing;
    if(!ok){fault_[i]=svc.missing_pc?svc.missing_pc:(key==21?0x4e2970:0x4de2b0);result=0;}
    return ok;
}
}
