#include "platform/frontend_license_widgets.hpp"
#include "platform/frontend_preview_pack.hpp"
#include <cstdio>
#include <cstdlib>
using namespace outrun::platform;
using outrun::driving::Bytes;
#define CHECK(x) do {if(!(x)){std::fprintf(stderr,"line %d: %s\n",__LINE__,#x);std::exit(1);}}while(0)
int main(int argc,char** argv){
    CHECK(argc==5);FrontendFontPack fonts;FrontendTextTable text;FrontendPreviewPack preview;GameUiPack animation,shared;std::string error;
    CHECK(load_frontend_font_pack(argv[1],fonts,&error));CHECK(text.load(argv[2],&error));
    CHECK(load_frontend_preview_pack_file(argv[3],preview,&error));CHECK(make_frontend_animation_view(preview,animation,&error));
    FrontendSprites sprites;CHECK(sprites.bind(0x44,animation));FrontendUiResources ui{sprites};
    CHECK(load_shared_ui_pack_file(argv[4],shared,&error));CHECK(sprites.bind(0x2c,shared));
    std::array<std::uint8_t,PcLicenseEditorBytes> storage{};Bytes b(storage.data(),storage.size());
    FrontendLicenseWidgets widgets(b,ui,fonts,text);unsigned repeat{};CHECK(widgets.construct_editor_resources(repeat));CHECK(repeat==12);
    struct InputFrame {FrontendInputSnapshot snapshot;
        void operator=(int action){snapshot={};constexpr unsigned masks[]{4,8,0x400,0x1000,0x800,0x2000};
            if(action>=0&&action<6)snapshot.feature_mask=masks[action];}
    } input;
    widgets.input(input.snapshot,repeat);
    std::array<std::uint8_t,0xc00> parent{};Bytes parent_bytes(parent.data(),parent.size());parent_bytes.put32(0x218,2);
    auto commands=parent_bytes.sub(0x51c,parent_bytes.size()-0x51c);outrun::driving::PcUiNotifyGlobals globals;unsigned ignored{};
    CHECK(ui.commands(0x442ac0,commands,nullptr,0,globals,ignored));
    const unsigned keys[]{4,0x295,~0u,~0u,~0u,~0u,8,0x296};
    CHECK(ui.commands(0x440ea0,commands,keys,8,globals,ignored));
    unsigned sounds{};ui.effect_user=&sounds;ui.effect_4249f0=[](void* p,unsigned){++*static_cast<unsigned*>(p);return true;};
    struct InputOwner {FrontendUiResources* ui;Bytes root;unsigned calls{};} owner{&ui,parent_bytes};
    widgets.external(&owner,[](void* p,LicenseEditorServices& s,unsigned pc,std::size_t,const unsigned* args,std::size_t n,unsigned&){
        auto& o=*static_cast<InputOwner*>(p);
        if(pc==0x4249f0)return true; // explicit audio-device fixture
        if(pc!=0x440ed0||n!=2)return false;
        ++o.calls;const bool ok=o.ui->input_feedback(o.root,args[0],int(args[1]));
        if(!ok)s.missing_pc=o.ui->missing_pc;return ok;
    });
    bool focus=false;widgets.keyboard_focus(&focus,[](void* p,bool active){*static_cast<bool*>(p)=active;return true;});
    PcLicense profile{};frontend_license_reset_4471a0(profile,1234);unsigned action{};
    std::array<std::uint8_t,16> default_name{};
    std::copy(text.get(0x219)->begin(),text.get(0x219)->end(),default_name.begin());
    frontend_license_name_4dd590(profile,default_name); // chooser's new-slot path
    CHECK(license_editor_init_4dd7c0(b,profile,widgets.services()));
    CHECK(license_editor_tick_4de2b0(b,profile,widgets.services(),action));CHECK(b.u32(0x38)==0);
    for(unsigned i=0;i<60;++i)sprites.tick();
    CHECK(license_editor_tick_4de2b0(b,profile,widgets.services(),action));CHECK(b.u32(0x38)==1);
    CHECK(frontend_text_value_48eec0(b.sub(0x5ec,PcTextWidgetBytes))=="OR2C2C");
    widgets.begin_frame();CHECK(license_editor_tick_4de2b0(b,profile,widgets.services(),action));CHECK(widgets.glyphs().size()==6);
    CHECK(widgets.glyphs()[0].token==9);CHECK(widgets.glyphs()[0].y==168);
    auto& s=widgets.services();unsigned result{};const unsigned id=0x218;
    CHECK(s.call(s.user,0x465eb0,0,&id,1,result));CHECK(s.localized_text=="CREATE NEW LICENSE");
    // Real editor -> original keyboard -> live name widget -> profile commit.
    // Input snapshots are fixtures; actual shared input and command-sprite
    // feedback execute. Only the audio device and focus are captured leaves.
    input=0;CHECK(license_editor_tick_4de2b0(b,profile,s,action));CHECK(b.u32(0x38)==2);CHECK(focus);
    input=-1;for(unsigned i=0;i<60;++i){sprites.tick();CHECK(license_editor_tick_4de2b0(b,profile,s,action));}
    CHECK(sprites.used(12)==2); // opening transition released; alphabet + cursor
    input=0;CHECK(license_editor_tick_4de2b0(b,profile,s,action)); // first key = '1'
    CHECK(frontend_text_value_48eec0(b.sub(0x5ec,PcTextWidgetBytes))=="OR2C2C1");
    // UP reaches SPACE (37), RIGHT reaches ENTER (43) in the PC navigation table.
    input=2;CHECK(license_editor_tick_4de2b0(b,profile,s,action));CHECK(widgets.keyboard_state().selected==37);
    input=5;CHECK(license_editor_tick_4de2b0(b,profile,s,action));CHECK(widgets.keyboard_state().selected==43);
    input=0;CHECK(license_editor_tick_4de2b0(b,profile,s,action));CHECK(b.u8(0xa78+0x6fc));
    CHECK(b.u8(0x1184)); // result is delayed until the reverse animation ends
    input=-1;for(unsigned i=0;i<60;++i){sprites.tick();CHECK(license_editor_tick_4de2b0(b,profile,s,action));}
    CHECK(!focus);CHECK(!b.u8(0x1184));CHECK(b.u32(0x38)==1);CHECK(sprites.used(12)==0);
    CHECK(std::string(reinterpret_cast<const char*>(profile.data()))=="OR2C2C1");CHECK(profile[0x3f4]&2);
    // Cancel a second editing session: a changed widget must not change the save.
    input=0;CHECK(license_editor_tick_4de2b0(b,profile,s,action));
    input=-1;for(unsigned i=0;i<60;++i){sprites.tick();CHECK(license_editor_tick_4de2b0(b,profile,s,action));}
    input=0;CHECK(license_editor_tick_4de2b0(b,profile,s,action));
    input=1;CHECK(license_editor_tick_4de2b0(b,profile,s,action));
    input=-1;for(unsigned i=0;i<60;++i){sprites.tick();CHECK(license_editor_tick_4de2b0(b,profile,s,action));}
    CHECK(std::string(reinterpret_cast<const char*>(profile.data()))=="OR2C2C1");CHECK(!focus);
    CHECK(owner.calls>0); // keyboard's argument -1 reached real owner feedback
    CHECK(ui.commands(0x447090,commands,nullptr,0,globals,ignored));
    std::vector<std::uint8_t> chooser(PcLicenseChooserBytes,0xa5);Bytes cb(chooser.data(),chooser.size());
    FrontendLicenseWidgets panels(cb,ui,fonts,text);
    CHECK(panels.construct_resource(0x1b8));
    for(auto off:{0x258u,0x2f8u})CHECK(panels.construct_resource(off));
    CHECK(panels.construct_widget(0xa08,repeat));CHECK(panels.construct_widget(0xe94,repeat));
    for(unsigned i=0;i<3;++i)CHECK(panels.construct_panel_4e0ab0(0x3580+i*0x225c,repeat));
    CHECK(panels.initialize_panels_4e0c70());
    for(unsigned i=0;i<3;++i){auto off=0x3580+i*0x225c;CHECK(cb.u32(off)==0);CHECK(cb.u32(off+4)==0);CHECK(cb.u32(off+0x1fd0)==2);
        for(unsigned j=0;j<7;++j){auto w=cb.sub(off+8+j*0x48c,PcTextWidgetBytes);CHECK(w.u32(0)==0x5c18a0);CHECK(w.u32(0x454)==3);CHECK(w.u8(0x4e)==0);}
    }
    CHECK(panels.release_panels_4e0d80());CHECK(!panels.construct_panel_4e0ab0(chooser.size()-4,repeat));
    FrontendProfiles profiles;for(auto& p:profiles.licenses)frontend_license_reset_4471a0(p,1234);profiles.selected=~0u;
    panels.profiles(profiles,nullptr,nullptr);
    panels.external(nullptr,[](void*,LicenseEditorServices&,unsigned pc,std::size_t,const unsigned*,std::size_t,unsigned& r){r=0;return pc==0x442ec0;});
    cb.put32(0x38,1);LicenseChooserState slide;
    CHECK(license_chooser_tick_4e2970(cb,profiles,slide,panels.services(),action));
    std::string label;for(unsigned i=0;i<22&&cb.u8(0x58+i);++i)label+=char(cb.u8(0x58+i));CHECK(label=="CREATE NEW LICENSE");
    for(unsigned frame=0;frame<20;++frame){sprites.tick();CHECK(license_chooser_tick_4e2970(cb,profiles,slide,panels.services(),action));if(cb.u32(0x38)==3)break;}
    CHECK(cb.u32(0x38)==3);CHECK(frontend_text_value_48eec0(cb.sub(0x3588,PcTextWidgetBytes))==label);
    panels.begin_frame();CHECK(panels.display_chooser_4e3240());CHECK(!panels.glyphs().empty());
    CHECK(cb.f32(0x3588+0x34)==202&&cb.f32(0x3588+0x38)==167);
    // Existing sparse slots populate the real fields and authored portrait /
    // nationality / flag resources, without selecting a different save.
    profiles.licenses[2]=profile;profiles.licenses[2][0x3f4]|=1;
    Bytes saved(profiles.licenses[2].data(),PcLicenseBytes);saved.putf(0x108,4);saved.putf(0x104,3);saved.put32(0x10c,219660);saved.putf(0x24,12345);
    license_chooser_list_4e1c00(cb,profiles);cb.put32(0x40,2);cb.put32(0x3c,1);cb.put32(0x44,0);
    CHECK(!panels.populate_panels_4e2150());CHECK(panels.services().missing_pc==0x447400);
    panels.profiles(profiles,nullptr,[](void*,const PcLicense&,double& value){value=25.125;return true;});
    CHECK(panels.populate_panels_4e2150());CHECK(profiles.selected==~0u);
    auto value=[&](unsigned index){return frontend_text_value_48eec0(cb.sub(0x3588+index*0x48c,PcTextWidgetBytes));};
    CHECK(value(0)=="OR2C2C1");CHECK(value(1)=="25.12%");CHECK(value(3)=="75.00%");CHECK(value(4)==" 1''00'48");CHECK(value(5)=="12345");
    CHECK(!cb.u8(0x1320)&&!cb.u8(0x1321)&&!cb.u8(0x1322));panels.begin_frame();CHECK(panels.display_chooser_4e3240());
    CHECK(cb.f32(0x3a14+0x34)==354);CHECK(panels.release_panels_4e0d80());
    LicenseProgressTables progress;panels.profiles(profiles,progress);
    CHECK(!panels.populate_panels_4e2150()); // tables not yet supplied by the loader
    progress.categories_ready=true;CHECK(panels.populate_panels_4e2150());CHECK(value(1)==" 0.00%");
    progress.category_counts.fill(5);frontend_license_unlock_447360(profiles.licenses[2]);
    for(unsigned g=0;g<3;++g){progress.championship_present[g]=true;
        for(unsigned race=0;race<15;++race)progress.championship_types[g][race]=g*15+race;}
    CHECK(panels.populate_panels_4e2150());CHECK(value(1)=="100.00%");
    CHECK(panels.release_panels_4e0d80());progress.category_counts[0]=6;double fraction{};
    CHECK(!frontend_license_completion_447400(profiles.licenses[2],progress,fraction));
    cb.put32(0x44,5);CHECK(!panels.populate_panels_4e2150());CHECK(!panels.display_panel_4e2640());
    // The actual chooser context controller calls the owned original list,
    // not an external selection stub. Dialog window ticking is an explicit
    // fixture until the generic 48D0C0 window is ported.
    CHECK(panels.construct_list(0xb004));CHECK(!panels.construct_list(0xb004));
    auto* context=panels.list(0xb004);CHECK(context);
    CHECK(context->initialize_4ecfb0(0x5cdc20,{},320,1,0,0));
    for(unsigned id:{0x233u,0x237u,0x234u,0x2d7u,0x2d8u,0x2dau,0x2d9u}){
        CHECK(text.get(id));const std::string_view label=*text.get(id);unsigned index{};CHECK(context->add_text_4ed160(&label,0,index));}
    CHECK(context->set_enabled_4ed300(3,true));CHECK(context->set_enabled_4ed300(4,true));
    struct InputFixture{int input{-1};unsigned sounds{};Bytes* object{};} context_input;
    panels.external(&context_input,[](void* p,LicenseEditorServices&,unsigned pc,std::size_t off,const unsigned* a,std::size_t n,unsigned& r){
        auto& f=*static_cast<InputFixture*>(p);r=0;
        if(pc==8&&off==0x9d54&&n==0)return true;
        if(pc==0x48f5f0&&off==0&&n==1&&a[0]==1){r=unsigned(f.input);return true;}
        if(pc==0x4249f0&&n==1&&a[0]==1){++f.sounds;return true;}
        return false;
    });
    cb.put8(0x36,0);cb.put32(0x44,0);
    context_input.input=2;CHECK(license_chooser_context_4e1cf0(cb,profiles,slide,panels.services(),action));CHECK(cb.u32(0xb004)==6);
    context_input.input=4;CHECK(license_chooser_context_4e1cf0(cb,profiles,slide,panels.services(),action));CHECK(cb.u32(0xb004)==0);
    CHECK(context_input.sounds==2);auto& ps=panels.services();
    CHECK(!ps.call(ps.user,0x4ed3e0,0xb004,nullptr,0,result)); // real clock must be supplied
    panels.list_timer(12);panels.begin_frame();CHECK(ps.call(ps.user,0x4ed3e0,0xb004,nullptr,0,result));
    CHECK(!panels.glyphs().empty()&&panels.images().size()==15);
    panels.begin_frame();CHECK(panels.glyphs().empty()&&panels.images().empty());
    CHECK(ps.call(ps.user,0x4eda60,0xb004,nullptr,0,result)&&context->size()==0);
    // Replace the window fixture with the original window and context creator.
    CHECK(panels.construct_window(0x9d54,repeat));CHECK(!panels.construct_window(0x9d54,repeat));
    CHECK(ps.call(ps.user,4,0x9d54,nullptr,0,result));
    context_input.object=&cb;
    panels.external(&context_input,[](void* p,LicenseEditorServices&,unsigned pc,std::size_t off,const unsigned* a,std::size_t n,unsigned& r){
        auto& f=*static_cast<InputFixture*>(p);r=0;
        if(pc==0x48f5f0&&n==1&&(off==0||off==0x9d54)){
            if(off==0x9d54)f.object->put32(off+0x1c,0x12345678);
            r=unsigned(off==0?f.input:-1);return true;
        }
        if(pc==0x4249f0&&n==1){++f.sounds;return true;}return false;
    });
    for(unsigned mask=0;mask<8;++mask){
        cb.put8(0x1320,mask&1);cb.put8(0x1321,(mask>>1)&1);cb.put8(0x1322,(mask>>2)&1);cb.put8(0x9c9c,0);
        CHECK(ps.call(ps.user,0x4e12b0,0,nullptr,0,result));CHECK(context->size()==7);
        const unsigned visible=3+((mask&1)?1:0)+((mask&2)?1:0)+((mask&4)?2:0);
        CHECK(context->height_count_4ed810()==int(visible));CHECK(cb.i16(0x9d54+0x38)==int(visible*17+80));
        CHECK(cb.f32(0xb004+0x24)==154);CHECK(cb.u32(0xb004)==0);
        if(mask==0)CHECK(!ps.call(ps.user,8,0x9d54,nullptr,0,result)); // no invented owner delta
        panels.owner_clock(1,2);CHECK(ps.call(ps.user,8,0x9d54,nullptr,0,result));
        CHECK(ps.call(ps.user,8,0x9d54,nullptr,0,result));CHECK(cb.u32(0x9d54+0x1c)==0x12345678);
        CHECK(cb.f32(0x9d54+0x48+0x34)==cb.f32(0x9d54+0x1280)+6);
        panels.begin_frame();CHECK(ps.call(ps.user,12,0x9d54,nullptr,0,result));
        CHECK(panels.images().size()==10&&!panels.glyphs().empty()&&panels.icons().empty());
        CHECK(ps.call(ps.user,0x4ed3e0,0xb004,nullptr,0,result));CHECK(panels.images().size()==10+3*visible);
        CHECK(ps.call(ps.user,0x4e1680,0,nullptr,0,result));CHECK(context->size()==0&&!cb.u8(0x9d54+0x34));
    }
    cb.put8(0x9c9c,1);cb.put8(0x1320,0);cb.put8(0x1321,0);cb.put8(0x1322,1);
    CHECK(panels.open_context_4e12b0());CHECK(context->height_count_4ed810()==2&&cb.u32(0xb004)==5);
    CHECK(panels.close_context_4e1680());
    cb.put8(0x1322,0);CHECK(panels.open_context_4e12b0());
    CHECK(context->height_count_4ed810()==0&&cb.u32(0xb004)==1); // PC hide order, not a new fallback selection
    CHECK(panels.close_context_4e1680());
    // Retained parent -> context -> full-widget confirmation -> close path.
    // Persistence remains deliberately unavailable here: never touch retail
    // saves, nor interpret an absent delete service as success.
    CHECK(panels.construct_window(0xb040,repeat));CHECK(panels.construct_choice_list(0xc2f0,repeat));
    panels.external(&context_input,[](void* p,LicenseEditorServices&,unsigned pc,std::size_t off,const unsigned* a,std::size_t n,unsigned& r){
        auto& f=*static_cast<InputFixture*>(p);r=0;
        if(pc==0x48f5f0&&n==1&&(off==0||off==0x9d54||off==0xb040)){
            r=unsigned(off==0?f.input:-1);return true;
        }
        if(pc==0x4249f0&&n==1&&a[0]==1){++f.sounds;return true;}return false;
    });
    const auto bank_before=profiles.licenses;
    cb.put8(0x9c9c,0);CHECK(panels.open_context_4e12b0());
    for(unsigned outcome=0;outcome<3;++outcome){
        const auto layer5_before=sprites.used(5); // Other live widgets share this pool.
        cb.put32(0xb004,2);context_input.input=0;
        CHECK(license_chooser_context_4e1cf0(cb,profiles,slide,ps,action));
        CHECK(cb.u8(0x36)==1&&cb.u32(0xc2f0)==0&&panels.choice_list(0xc2f0)->size()==2);
        context_input.input=-1;
        CHECK(license_chooser_context_4e1cf0(cb,profiles,slide,ps,action));
        panels.begin_frame();CHECK(panels.display_delete_4e1860());
        CHECK(!panels.glyphs().empty()&&!panels.images().empty());
        if(outcome==2){
            context_input.input=4;CHECK(license_chooser_context_4e1cf0(cb,profiles,slide,ps,action));
            CHECK(cb.u32(0xc2f0)==1);
        }
        context_input.input=outcome==1?1:0;
        const bool ok=license_chooser_context_4e1cf0(cb,profiles,slide,ps,action);
        CHECK(ok==(outcome!=2));
        if(outcome==2)CHECK(ps.missing_pc==0x416500);
        CHECK(!cb.u8(0x36)&&!cb.u8(0xb016)&&!cb.u8(0xb040+0x34));
        CHECK(panels.choice_list(0xc2f0)->size()==0&&sprites.used(5)==layer5_before);
        CHECK(profiles.licenses==bank_before);
    }
    CHECK(panels.close_context_4e1680());
    std::puts("retail license resources + editable name glyphs + three seven-widget panels pass");
    // Construct the whole original owner, not a hand-picked subset of children,
    // then retain it over init -> cards -> context -> suspend -> re-entry.
    {
        std::vector<std::uint8_t> owner(PcLicenseChooserBytes);Bytes o(owner.data(),owner.size());
        FrontendSprites pool;CHECK(pool.bind(0x44,animation));CHECK(pool.bind(0x2c,shared));FrontendUiResources resources{pool};
        unsigned owner_repeat{};auto live_owner=std::make_unique<FrontendLicenseWidgets>(o,resources,fonts,text);auto& live=*live_owner;
        CHECK(live.construct_chooser_4e1890(owner_repeat));CHECK(owner_repeat==12&&o.u32(8)==21&&o.u32(0)==0x5cdc90);
        CHECK(!live.construct_chooser_4e1890(owner_repeat));
        FrontendProfiles saves;for(auto& l:saves.licenses)frontend_license_reset_4471a0(l,12);
        saves.licenses[2]=profile;saves.licenses[2][0x3f4]|=1;CHECK(frontend_profiles_select_448520(saves,2));
        LicenseProgressTables tables;tables.categories_ready=true;live.profiles(saves,tables);LicenseChooserState state;
        auto& svc=live.services();CHECK(!svc.call(svc.user,4,0,nullptr,0,action)); // no implicit owner state
        live.chooser_state(state);
        CHECK(!svc.call(svc.user,4,0,nullptr,0,action));CHECK(svc.missing_pc==0x442f20); // preserve nested boundary, not outer virtual slot 4
        struct OwnerCalls{unsigned configure{},commands{},previous{};int input{-1};} calls;
        live.external(&calls,[](void* p,LicenseEditorServices&,unsigned pc,std::size_t,const unsigned* args,std::size_t n,unsigned& r){
            auto& c=*static_cast<OwnerCalls*>(p);r=0;
            if(pc==0x442f20&&n==2&&args[0]==0&&args[1]==0){++c.configure;return true;}
            if(pc==0x440ea0&&n==8){const unsigned expected[]{4,0x295,~0u,~0u,~0u,~0u,8,0x296};
                if(!std::equal(args,args+8,expected))return false;++c.commands;return true;}
            if(pc==0x442ec0&&n==0){r=c.previous;return true;}
            if(pc==0x48f5f0&&n==1){r=unsigned(c.input);return true;}return false;
        });
        live.owner_clock(1,2);live.list_timer(0);
        for(unsigned cycle=0;cycle<4;++cycle){
            CHECK(svc.call(svc.user,4,0,nullptr,0,action)&&action==1);CHECK(o.u32(0x38)==0&&o.u32(0x578)==2);
            for(unsigned frame=0;frame<30&&o.u32(0x38)!=3;++frame){
                pool.tick();CHECK(svc.call(svc.user,8,0,nullptr,0,action));
            }
            CHECK(o.u32(0x38)==3&&o.u32(0x40)==2);live.begin_frame();CHECK(svc.call(svc.user,12,0,nullptr,0,action));CHECK(!live.glyphs().empty());
            CHECK(live.open_context_4e12b0());CHECK(live.list(0xb004)->size()==7);
            CHECK(svc.call(svc.user,16,0,nullptr,0,action));
            CHECK(!o.u8(0x9d54+0x34)&&live.list(0xb004)->size()==0&&state.restore_slot==-1&&o.u32(0x38)==0);
            CHECK(pool.used(1)==0&&pool.used(2)==0&&pool.used(5)==0);
        }
        CHECK(calls.configure==4&&calls.commands==4);
        CHECK(!svc.call(svc.user,0x4e0dd0,0,nullptr,0,action)&&svc.missing_pc==0x4e0dd0);
        std::array<std::uint8_t,0x28> remote_manager{};std::array<std::uint8_t,0x94> remote_record{};
        Bytes manager(remote_manager.data(),remote_manager.size()),remote(remote_record.data(),remote_record.size());
        manager.put32(0x20,0x910002);for(unsigned i=0;i<7;++i)remote.put8(0x20+i,std::uint8_t("REMOTE"[i]));
        remote.putf(0x60,25);remote.putf(0x64,75);remote.putf(0x68,12345);remote.putf(0x80,1234);remote.put32(0x84,3600);
        outrun::driving::PcNativeHandleBinding bindings[]{{0x910001,remote_manager.data(),remote_manager.size()},{0x910002,remote_record.data(),remote_record.size()}};
        LicenseSpecialProfileSource source{2,0x910001,{bindings,2}};live.special_profile_source(source);
        for(unsigned previous:{12u,13u}){
            calls.previous=previous;calls.input=-1;
            CHECK(svc.call(svc.user,4,0,nullptr,0,action));
            for(unsigned frame=0;frame<30&&o.u32(0x38)!=11;++frame){pool.tick();CHECK(svc.call(svc.user,8,0,nullptr,0,action));}
            CHECK(o.u32(0x38)==11&&pool.used(2)==3);
            live.begin_frame();CHECK(svc.call(svc.user,12,0,nullptr,0,action));CHECK(!live.glyphs().empty());
            CHECK(frontend_text_value_48eec0(o.sub(0x132c,PcTextWidgetBytes))=="REMOTE");
            CHECK(frontend_text_value_48eec0(o.sub(0x17b8,PcTextWidgetBytes))=="25.00%");
            CHECK(frontend_text_value_48eec0(o.sub(0x20d0,PcTextWidgetBytes))=="75.00%");
            const auto icon=o.u32(0x3308);source.selected=-1;
            CHECK(svc.call(svc.user,0x4e0dd0,0,nullptr,0,action));
            CHECK(frontend_text_value_48eec0(o.sub(0x132c,PcTextWidgetBytes)).empty());
            CHECK(o.u32(0x3308)==icon&&pool.get(icon)->allocated); // PC absence clears text, not existing sprites.
            source.selected=2;bindings[1].size=0x93;
            CHECK(!svc.call(svc.user,0x4e0dd0,0,nullptr,0,action)&&svc.missing_pc==0x4e0dd0);
            bindings[1].size=remote_record.size();source.selected=0x7fffffff;
            CHECK(!svc.call(svc.user,0x4e0dd0,0,nullptr,0,action));source.selected=2;
            CHECK(svc.call(svc.user,0x4e0dd0,0,nullptr,0,action));
            calls.input=previous==12?0:1;CHECK(svc.call(svc.user,8,0,nullptr,0,action));CHECK(o.u32(0x38)==5&&pool.used(2)==0);
            calls.input=-1;for(unsigned frame=0;frame<15&&o.u32(0x38)==5;++frame){pool.tick();CHECK(svc.call(svc.user,8,0,nullptr,0,action));}
            CHECK(action==2&&o.u32(0x38)==0);CHECK(svc.call(svc.user,16,0,nullptr,0,action));
        }
        const unsigned args[]{0x440004,0,0,6,0,0,0,0x3f800000,0x3f800000,0x3f800000,0};
        CHECK(svc.call(svc.user,0x465860,0x4d8,args,11,action));CHECK(svc.call(svc.user,0x465970,0x4d8,nullptr,0,action));
        CHECK(pool.used(6)==1);CHECK(svc.call(svc.user,16,0,nullptr,0,action));CHECK(pool.used(6)==1);
        live_owner.reset();CHECK(pool.used(6)==0); // Suspend != destruction; shared pool outlives owner.
    }
}
