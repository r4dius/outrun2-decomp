#include "system/files.hpp"
#include "switch_renderer.hpp"
#include "platform/frontend_glyph_render.hpp"
#include "platform/frontend_license_widgets.hpp"
#include "platform/frontend_vehicle_menu.hpp"
#include "platform/frontend_vehicle_preview.hpp"
#include "platform/native_runtime.hpp"
#include "platform/menu_audio.hpp"
#include "platform/retail_gpu_cache.hpp"
#include "platform/pc_scene_renderer.hpp"
#include "platform/pc_soft_d3d9.hpp"
#include "platform/embedded_exe_data.hpp"
#include "platform/loader_asset_pack.hpp"
#include "platform/course_collision_pack.hpp"
#include "platform/driving_data_pack.hpp"
#include "platform/world_source_pack.hpp"
#include "platform/retail_asset_store.hpp"
#include <iterator>
#include <filesystem>
#include <fstream>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <limits>
using namespace outrun::platform;
using namespace outrun::switch_runtime;
using outrun::driving::Bytes;
#define CHECK(x) do {if(!(x)){std::fprintf(stderr,"line %d: %s (%s)\n",__LINE__,#x,error.c_str());std::exit(1);}}while(0)
int main(int argc,char** argv){
    std::string error;CHECK(argc==5);SwitchRenderer renderer;
    namespace fs=std::filesystem;
    const auto root=fs::temp_directory_path()/("outrun-menu-render-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    CHECK(fs::create_directory(root));
    struct Cleanup {fs::path root;~Cleanup(){fs::remove_all(root);}} cleanup{root};
    const auto retail=fs::path(argv[4]).parent_path().parent_path(),fixtures=fs::path(argv[1]).parent_path();
    auto copy=[&](const fs::path& from,const fs::path& to){fs::create_directories(to.parent_path());CHECK(fs::copy_file(from,to));};
    for(const char* file:{"Sprani/ani_SPRANI_SUMO_FE_CVT.sz","Sprite/spr_SPRANI_SUMO_FE_CVT_Exst.sz",
                         "Sprani/ani_SPRANI_ETC_CVT.sz","Sprite/spr_SPRANI_ETC_CVT_Exst.sz","Sprite/spr_font_xst.sz","Sprite/spr_etc_xst.sz"})copy(retail/file,root/file);
    RetailAssetStore store;store.root=root.string();
    auto bytes=[](const fs::path& p){std::ifstream f(p,std::ios::binary);return std::vector<char>(std::istreambuf_iterator<char>(f),{});};
    // The game builds its font metrics in memory; they match the reference pack.
    FrontendFontPack built_fonts,reference_fonts;CHECK(build_retail_font_pack(store,built_fonts,&error));
    CHECK(load_frontend_font_pack(argv[3],reference_fonts,&error));
    for(unsigned i=0;i<10;++i)CHECK(built_fonts.textures[i].bytes==reference_fonts.textures[i].bytes&&
                                    built_fonts.fonts[i].metrics==reference_fonts.fonts[i].metrics&&built_fonts.fonts[i].kerning==reference_fonts.fonts[i].kerning);
    // The legacy menu graphics of the host renderer: shared UI and font fixtures, raw ETC image bank.
    const auto menu_images=root/"menu_images.xst";
    {std::vector<std::uint8_t> etc;CHECK(retail_asset_read_relative_inflated(store,"Sprite/spr_etc_xst.sz",etc,16u*1024u*1024u,&error));
     std::ofstream f(menu_images,std::ios::binary);f.write(reinterpret_cast<const char*>(etc.data()),std::streamsize(etc.size()));}
    FrontendImageBank images;CHECK(load_frontend_image_bank(menu_images.string().c_str(),images,&error));
    CHECK(images.regions.size()==117&&images.textures.size()==13&&images.regions[64].texture==11);
    CHECK((images.regions[64].crop==std::array<unsigned,4>{{0,34,4,64}}));
    const auto raw_chars=bytes(menu_images);const std::vector<std::uint8_t> raw(raw_chars.begin(),raw_chars.end());
    auto reject_bank=[&](const std::vector<std::uint8_t>& bad){CHECK(!parse_frontend_image_bank(bad.data(),bad.size(),images,&error));
        CHECK(images.regions.size()==117&&images.textures.size()==13&&images.regions[64].texture==11);};
    for(std::size_t n:{0u,39u,3759u,3760u,272687u})reject_bank({raw.begin(),raw.begin()+n});
    auto damage=raw;damage[0]^=1;reject_bank(damage); // section sizes
    damage=raw;damage[8]=1;reject_bank(damage); // unsupported XST flags
    damage=raw;damage[36]=0xff;damage[37]=0xff;reject_bank(damage); // table outside metadata
    damage=raw;damage[3760+84]='?';reject_bank(damage); // DDS format
    damage=raw;damage[8+152]=13;reject_bank(damage); // invalid texture reference
    damage=raw;damage[8+152+24]=0xff;damage[8+152+25]=0xff;reject_bank(damage); // invalid crop
    damage=raw;damage.push_back(0);reject_bank(damage);
    CHECK(switch_renderer_initialize(renderer,nullptr,error,
          nullptr,argv[2],argv[3],menu_images.string().c_str()));
    // The sprite timing of the SUMO_FE and ETC banks comes from the original SprAni files, as in the game.
    GameUiPack sumo_fe_timeline,etc_timeline;
    CHECK(load_retail_sprani(store,"SUMO_FE",224u,sumo_fe_timeline,&error)&&load_retail_sprani(store,"ETC",319u,etc_timeline,&error));
    const auto bind_timing=[&](FrontendSprites& sprites){return sprites.bind(0x44u,sumo_fe_timeline)&&sprites.bind(0x2cu,etc_timeline);};
    const auto* fonts=switch_renderer_frontend_fonts(renderer);CHECK(fonts);
    auto stats=switch_renderer_stats(renderer);CHECK(stats.shared_ui_scenes==319&&stats.font_textures==10);
    FrontendSprites pool;CHECK(bind_timing(pool)&&switch_renderer_attach_frontend_sprites(renderer,pool));
    // Every shared scene, including small cursor components and empty graphs,
    // must traverse the same instance/evaluation path as the SUMO_FE bank.
    for(unsigned id=0;id<319;++id){auto h=pool.create(0x2c0000+id,id%21,4,0,0,true);CHECK(h!=~0u);
        CHECK(switch_renderer_draw(renderer));CHECK(!pool.get(h)->allocated);
    }
    FrontendTextTable table;CHECK(table.load(argv[4],&error));FrontendUiResources ui{pool};
    // Production owner routing, not direct calls to the menu routine. This
    // state-3 fixture supplies only the mode/connected-device entry conditions;
    // native factory, virtual control/display and retail widgets run together.
    {
        auto runtime=std::make_unique<NativeRuntimeContext>();auto& state=runtime->event_function36;
        CHECK(bind_timing(state.frontend_sprites)&&switch_renderer_attach_frontend_sprites(renderer,state.frontend_sprites));
        FrontendTitleWidgets widgets(state.title_owner_object.data(),state.title_owner_object.size(),
            Bytes(state.object.data(),state.object.size()),state.frontend_sprites,*fonts,table,state.frontend_input,state.title_base_global_6591e4);
        state.title_widgets=&widgets;MenuAudio sounds;
        CHECK(sounds.load_file((retail/"Sound/MENU.pak").string(),error));
        widgets.effect(&sounds,[](void* p,unsigned id){std::string error;return static_cast<MenuAudio*>(p)->command(id,error);});
        CHECK(native_runtime_event_function36_invoke(*runtime,0x49e490,405));
        runtime->mode_state.current=16;runtime->game_mode.game_variant=1;runtime->input_state.connected=true;
        Bytes root(state.object.data(),state.object.size());root.put32(0x218,3);
        runtime->event_state.slots[405].flags=2;runtime->event_state.slots[405].disp_callback=0x49e4b0;
        auto frame=[&]{
            CHECK(native_runtime_event_function36_invoke(*runtime,0x49e4a0,405));
            CHECK(native_runtime_event_function36_display(*runtime));
            CHECK(switch_renderer_set_frontend_glyphs(renderer,widgets.glyphs()));
            CHECK(switch_renderer_set_frontend_images(renderer,widgets.images()));
            CHECK(switch_renderer_set_frontend_icons(renderer,widgets.icons()));
            CHECK(switch_renderer_draw(renderer));
        };
        // In the race (mode 16) the pause menu (444470, key 0x2C) opens on the start key only.
        runtime->input_state.frontend.feature_mask=1;frame();runtime->input_state.frontend.feature_mask=0;
        for(unsigned i=1;i<20;++i)frame();
        Bytes title(state.title_owner_object.data(),state.title_owner_object.size());
        CHECK(state.title_construct_calls==1&&state.title_initial_ui_calls==1&&widgets.rows()==6&&title.u32(0x9ac)==1);
        CHECK(state.title_last_missing_service==0&&widgets.images().size()>0&&widgets.icons().size()==2);
        CHECK(switch_renderer_stats(renderer).menu_icons==2&&switch_renderer_stats(renderer).menu_icon_draws>0);
        const auto used=state.frontend_sprites.used(15);auto invalid_icons=widgets.icons();invalid_icons[0].layer=21;
        CHECK(!switch_renderer_set_frontend_icons(renderer,invalid_icons));
        CHECK(switch_renderer_stats(renderer).menu_icons==2&&state.frontend_sprites.used(15)==used);
        invalid_icons=widgets.icons();invalid_icons[0].token=0x2cffff;
        CHECK(!switch_renderer_set_frontend_icons(renderer,invalid_icons));
        // Rendering immediate button icons must not consume persistent slots.
        CHECK(switch_renderer_set_frontend_icons(renderer,widgets.icons()));
        CHECK(state.frontend_sprites.used(15)==used);
        runtime->input_state.frontend.feature_mask=0x800;frame();CHECK(title.u32(0x34)==1&&sounds.stats.starts==1);
        runtime->input_state.frontend.feature_mask=4;frame();CHECK(title.u32(0x9ac)==2&&title.u32(0x9b4)==12);
        runtime->input_state.frontend.feature_mask=0;frame();CHECK(widgets.rows()==0&&title.u8(0x9b8));
        frame();CHECK(title.u32(0x9ac)==13&&state.title_last_missing_service==0);   // 4D6430 is ported (2026-10-04)
        CHECK(native_runtime_event_function36_invoke(*runtime,0x49e4c0,405));CHECK(widgets.rows()==0);
        CHECK(!widgets.visible()&&widgets.glyphs().empty()&&widgets.images().empty()&&widgets.icons().empty());
        // Restart the same bound objects; no stale row handles or cooldown.
        CHECK(native_runtime_event_function36_invoke(*runtime,0x49e490,405));root.put32(0x218,3);
        runtime->input_state.frontend.feature_mask=1;frame();runtime->input_state.frontend.feature_mask=0;for(unsigned i=1;i<20;++i)frame();CHECK(widgets.rows()==6&&title.u32(0x9ac)==1);
        widgets.effect(nullptr,nullptr);runtime->input_state.frontend.feature_mask=0x800;frame();
        CHECK(state.title_last_missing_service==0x4249f0&&title.u32(0x9ac)==1);
        CHECK(native_runtime_event_function36_invoke(*runtime,0x49e4c0,405));CHECK(widgets.rows()==0&&!widgets.visible());
        state.title_widgets=nullptr;
        CHECK(switch_renderer_set_frontend_glyphs(renderer,{}));CHECK(switch_renderer_set_frontend_images(renderer,{}));
        CHECK(switch_renderer_set_frontend_icons(renderer,{}));CHECK(bind_timing(pool)&&switch_renderer_attach_frontend_sprites(renderer,pool));
    }
    // Actual runtime factory/history/control/display chain: welcome -> empty
    // bank chooser -> editor -> cancel -> chooser. Real retail PCM sounds are
    // mixed here; hardware audio output is independently device-fixture tested.
    {
        RetailAssetStore assets;CHECK(retail_asset_store_open(assets,retail.string(),&error));
        auto runtime=std::make_unique<NativeRuntimeContext>();auto& state=runtime->event_function36;
        FrontendLicenseOwners licenses(*runtime,*fonts,table);state.license_owners=&licenses;
        state.frontend_fonts=fonts;state.frontend_text=&table;
        MenuAudio effects;CHECK(effects.load_file((retail/"Sound/MENU.pak").string(),error));
        CHECK(effects.load_frontend_file((retail/"Sound/FE.PAK").string(),error));
        const auto effect=[](void* p,unsigned id){std::string error;return static_cast<MenuAudio*>(p)->command(id,error);};
        const auto effect_call=[](void* p,unsigned pc,const unsigned* args,std::size_t n,unsigned& result){
            if(pc!=0x4249f0||n!=1)return false;std::string error;result=0;return static_cast<MenuAudio*>(p)->command(args[0],error);
        };
        licenses.platform(&effects,effect_call);state.frontend_effect_user=&effects;state.frontend_effect=effect;
        std::vector<std::int16_t> mixed;std::uint64_t audible_samples=0;
        CHECK(native_runtime_attach_retail_assets(*runtime,assets));
        RaceAssetPack races;RaceAssignmentPack assignments;
        CHECK(load_race_asset_pack_file((retail/"Scripts/bin/Races.bin").string().c_str(),races,&error));
        CHECK(load_race_assignment_pack_file((retail/"Scripts/bin/RaceAssignment.bin").string().c_str(),assignments,&error));
        CHECK(native_runtime_attach_race_assets(*runtime,races));
        CHECK(native_runtime_attach_race_assignment(*runtime,assignments));
        state.frontend_save_directory=(root/"SaveGame").string(); // absent isolated bank, never PC saves
        CHECK(bind_timing(state.frontend_sprites)&&switch_renderer_attach_frontend_sprites(renderer,state.frontend_sprites));
        CHECK(native_runtime_event_function36_invoke(*runtime,0x49e490,405));
        runtime->mode_state.current=32;runtime->input_state.connected=true;
        runtime->event_state.slots[405].flags=2;runtime->event_state.slots[405].disp_callback=0x49e4b0;
        Bytes owner(state.object.data(),state.object.size());owner.put32(0x218,1);owner.put32(0,7);
        state.frontend_init_calls=2;outrun::driving::frontend_bulk_loader_initialize_4e85e0(state.frontend_bulk_loader);
        {   // 83039C as the bootstrap stage 3 (48BF20) leaves it: flush set, three
            // empty mode-11 slots; BA/BB owned and ready.
            Bytes l(state.loader_83039c.data(),state.loader_83039c.size());l.put8(0,1);
            for(unsigned k=0;k<3;++k){l.put32(4+4*k,30);l.put32(0x10+12*k,11);l.put32(0x14+12*k,30);l.put32(0x18+12*k,0);}
            for(auto& e:state.loader_resource_entries)e.status_14=7;
        }
        auto frame=[&](unsigned input=0,bool welcome=false){
            ++runtime->frame_state.frame_counter_95af0c;
            runtime->input_state.frontend.feature_mask=input;runtime->input_state.menu_confirm=welcome;
            state.frontend_ui_motion_step=1.f/60.f;
            CHECK(native_runtime_event_function36_invoke(*runtime,0x49e4a0,405));
            CHECK(native_runtime_event_function36_display(*runtime));
            CHECK(state.frontend_ui_missing_pc==0);
            CHECK(effects.mix(800,mixed,error));
            for(auto sample:mixed)audible_samples+=sample!=0;
            std::vector<FrontendGlyph> glyphs;std::vector<FrontendListImage> images;std::vector<FrontendWindowIcon> icons;
            for(unsigned key:{21u,24u})if(const auto* w=licenses.widgets(key)){
                if(licenses.missing(key))std::fprintf(stderr,"license key=%u missing=%08x frame=%u\n",key,licenses.missing(key),unsigned(runtime->frame_state.frame_counter_95af0c));
                CHECK(licenses.missing(key)==0);
                glyphs.insert(glyphs.end(),w->glyphs().begin(),w->glyphs().end());
                images.insert(images.end(),w->images().begin(),w->images().end());
                icons.insert(icons.end(),w->icons().begin(),w->icons().end());
            }
            glyphs.insert(glyphs.end(),state.frontend_categories.glyphs.begin(),state.frontend_categories.glyphs.end());
            glyphs.insert(glyphs.end(),state.frontend_missions.glyphs.begin(),state.frontend_missions.glyphs.end());
            CHECK(switch_renderer_set_frontend_glyphs(renderer,glyphs));CHECK(switch_renderer_set_frontend_images(renderer,images));
            CHECK(switch_renderer_set_frontend_icons(renderer,icons));CHECK(switch_renderer_draw(renderer));
        };
        for(unsigned f=0;f<30;++f)frame();frame(0,true);
        for(unsigned f=0;f<30;++f)frame();
        CHECK(licenses.widgets(21)&&state.frontend_handle_count==6);
        Bytes chooser(licenses.storage(21),licenses.size(21));CHECK(chooser.u32(0x38)==3&&chooser.u32(0x3c)==0);
        CHECK(state.frontend_profiles.licenses[0][0x1dd]==7&&state.frontend_profiles.licenses[0][0x18]==10);
        frame(4);for(unsigned f=0;f<90;++f)frame();
        if(!licenses.widgets(24))std::fprintf(stderr,"chooser state=%u target=%u action=%u selected=%u stack=%u missing-key=%u pc=%08x selector=%u\n",chooser.u32(0x38),chooser.u32(4),chooser.u32(0x578),state.frontend_profiles.selected,owner.u32(0x484),state.frontend_last_missing_key,state.frontend_ui_missing_pc,state.state2_selector_child_state);
        CHECK(licenses.widgets(24)&&state.frontend_handle_count==7);
        Bytes editor(licenses.storage(24),licenses.size(24));CHECK(editor.u32(0x38)==1&&editor.u32(8)==24);
        CHECK(state.frontend_profiles.selected==0&&state.frontend_profiles.active_loaded);
        CHECK(switch_renderer_stats(renderer).font_draws>0&&effects.stats.starts>0&&audible_samples>0);
        frame(8);for(unsigned f=0;f<90;++f)frame();
        CHECK(!licenses.widgets(24)&&licenses.widgets(21)&&chooser.u32(0x38)==3);
        CHECK(state.frontend_pending_input_action==~0u&&!(state.frontend_profiles.licenses[0][0x3f4]&1));
        // Reopen the same overlay twice, exercise the real keyboard's focus,
        // then cancel its name and the editor. No stale handles or input edges.
        const auto depth=owner.u32(0x484);const auto history=owner.u8(0x220);
        const auto sprite_count=state.frontend_sprites.used(12);
        for(unsigned cycle=0;cycle<2;++cycle){
            frame(4);for(unsigned f=0;f<90;++f)frame();
            CHECK(licenses.widgets(24)&&editor.u32(0x38)==1);
            CHECK(owner.u32(0x484)==depth&&owner.u32(0x488)==0x73000007&&owner.u8(0x48c));
            frame(4);for(unsigned f=0;f<62;++f)frame();CHECK(editor.u32(0x38)==2&&editor.u8(0x1184));
            frame(8);for(unsigned f=0;f<62;++f)frame();CHECK(editor.u32(0x38)==1);
            frame(8);for(unsigned f=0;f<90;++f)frame();
            CHECK(!licenses.widgets(24)&&chooser.u32(0x38)==3&&owner.u32(0x488)==0&&!owner.u8(0x48c));
            CHECK(owner.u32(0x484)==depth&&owner.u8(0x220)==history);
            CHECK(state.frontend_sprites.used(12)==sprite_count);
        }
        // An absent device effect is not turned into success or retried past
        // the failed input call. Reinitialization is needed before resuming.
        licenses.platform(nullptr,nullptr);
        runtime->input_state.frontend.feature_mask=4;
        CHECK(native_runtime_event_function36_invoke(*runtime,0x49e4a0,405));
        CHECK(licenses.missing(21)==0x4249f0&&state.frontend_ui_missing_pc==0x4249f0);
        CHECK(!licenses.widgets(24)&&chooser.u32(0x38)==3);
        for(unsigned f=0;f<5;++f){
            runtime->input_state.frontend.feature_mask=0;
            CHECK(native_runtime_event_function36_invoke(*runtime,0x49e4a0,405));
            CHECK(licenses.missing(21)==0x4249f0&&!licenses.widgets(24)&&chooser.u32(0x38)==3);
        }
        const auto random_state=state.pc_crt_random_state;
        CHECK(native_runtime_event_function36_invoke(*runtime,0x49e4c0,405));
        CHECK(!licenses.widgets(21)&&!licenses.widgets(24));
        CHECK(native_runtime_event_function36_invoke(*runtime,0x49e490,405));
        CHECK(state.pc_crt_random_state==random_state); // bank is process-owned, not reset on reopening UI
        licenses.platform(&effects,effect_call);
        auto restart_loader=[&]{
            owner.put32(0x218,1);owner.put32(0,7);state.frontend_init_calls=2;
            outrun::driving::frontend_bulk_loader_initialize_4e85e0(state.frontend_bulk_loader);
            runtime->event_state.slots[405].flags=2;runtime->event_state.slots[405].disp_callback=0x49e4b0;
            for(unsigned f=0;f<30;++f)frame();frame(0,true);
            for(unsigned f=0;f<30;++f)frame();
        };
        restart_loader();
        if(!licenses.widgets(21)||chooser.u32(0x38)!=3)std::fprintf(stderr,"restart state=%u loader=%u child=%u depth=%u gate=%u pending=%u profile=%u factory=%u missing=%u/%08x\n",owner.u32(0x218),owner.u32(0),state.state2_selector_child_state,owner.u32(0x484),state.frontend_gate_input_action,state.frontend_pending_input_action,state.frontend_profiles.loaded_count,state.frontend_factory_key,state.frontend_last_missing_key,state.frontend_ui_missing_pc);
        CHECK(licenses.widgets(21)&&chooser.u32(0x38)==3);
        // Complete the authored editor through the same input/controller path,
        // writing only this test's private save bank, then reload real files.
        CHECK(fs::create_directory(root/"SaveGame"));licenses.persistence((root/"SaveGame").string());
        frame(4);for(unsigned f=0;f<90;++f)frame();CHECK(editor.u32(0x38)==1);
        for(unsigned item:{2u,3u,4u}){
            frame(0x800);frame();CHECK(editor.u32(0x3c)==item);
        }
        frame(4);for(unsigned f=0;f<90;++f)frame();
        CHECK(!licenses.widgets(24)&&licenses.save_calls()==1&&licenses.persistence_error()==0);
        CHECK(fs::exists(root/"SaveGame/License1.dat")&&fs::exists(root/"SaveGame/common.dat"));
        FrontendProfiles saved;frontend_profiles_load(saved,(root/"SaveGame").string());
        CHECK(saved.loaded_count==1&&saved.selected==0&&saved.active==state.frontend_profiles.active);
        CHECK(saved.active[0x3f4]&1);
        CHECK(state.state2_selector_child_state==1&&owner.u32(0x484)==3&&owner.u32(0x488)==0);
        CHECK(native_runtime_event_function36_invoke(*runtime,0x49e4c0,405));
        CHECK(native_runtime_event_function36_invoke(*runtime,0x49e490,405));
        restart_loader();
        CHECK(state.frontend_profiles.loaded_count==1&&state.frontend_profiles.active==saved.active);
        CHECK(state.state2_selector_child_state==1&&!licenses.widgets(21)&&!licenses.widgets(24));
        Bytes main_choice(state.frontend_first_menu_object.data(),state.frontend_first_menu_object.size());
        Bytes mode_choice(state.frontend_second_menu_object.data(),state.frontend_second_menu_object.size());
        const auto choice_image=[&](Bytes child,unsigned token,unsigned position){
            const auto* sprite=state.frontend_sprites.get(child.u32(0x40));
            CHECK(sprite&&sprite->allocated&&sprite->token==token&&sprite->frame==float(position));
            CHECK(state.frontend_choice_scene_token==0); // no legacy static preview
            CHECK(switch_renderer_stats(renderer).frontend_instance_draws>0);
        };
        choice_image(main_choice,0x4400dd,20);
        for(unsigned choice=1;choice<5;++choice){
            frame(0x2000);for(unsigned f=0;f<30;++f)frame();
            CHECK(main_choice.u32(0xe8)==choice);choice_image(main_choice,0x4400dd,20*(choice+1));
        }
        for(unsigned choice=4;choice>0;--choice){frame(0x1000);for(unsigned f=0;f<30;++f)frame();}
        choice_image(main_choice,0x4400dd,20);
        // Actual persistent record manager, animation, owner stack, command
        // labels and sound; no successful external-manager fixture.
        Bytes records(state.frontend_records.records.data(),state.frontend_records.records.size());
        Bytes request(state.frontend_records.request.data(),state.frontend_records.request.size());
        const auto main_layer_instances=state.frontend_sprites.used(0);
        for(unsigned cycle=0;cycle<3;++cycle){
            records.put32(0x13d1c,cycle+10);records.put32(0x30,0x1234);request.put32(0x390,44);
            frame(4);for(unsigned f=0;f<30;++f)frame();CHECK(state.frontend_records.reset_calls==cycle+1);
            CHECK(records.u32(0x13d20)==cycle+10&&records.u32(0x13d1c)==0&&records.u32(0x30)==0);
            CHECK(request.u32(0x390)==0&&request.u32(0x14)==9);
            choice_image(mode_choice,0x440017,20);
            for(unsigned choice=1;choice<5;++choice){
                frame(0x2000);for(unsigned f=0;f<30;++f)frame();
                CHECK(mode_choice.u32(0xe8)==choice);choice_image(mode_choice,0x440017,20*(choice+1));
            }
            frame(8);for(unsigned f=0;f<30;++f)frame();
            CHECK(state.state2_selector_child_state==1);choice_image(main_choice,0x4400dd,20);
            CHECK(state.frontend_sprites.used(0)==main_layer_instances);
        }
        // Real primary menu -> mode -> category; authored resources and localized
        // glyphs, locked input (FE sound), parent pop and repeated reconstruction.
        for(unsigned cycle=0;cycle<3;++cycle){
            frame(4);for(unsigned f=0;f<30;++f)frame();CHECK(state.state2_selector_child_state==2);
            frame(4);for(unsigned f=0;f<30;++f)frame();CHECK(state.state2_selector_child_state==0x3a);
            CHECK(state.frontend_categories.constructed&&!state.frontend_categories.fault);
            CHECK(!state.frontend_categories.glyphs.empty());
            auto category=Bytes(state.frontend_categories.object.data(),PcCategoryOwnerBytes);
            for(unsigned i=0;i<7;++i){const auto* sprite=state.frontend_sprites.get(category.u32(0x3c+i*0xa0));
                CHECK(sprite&&sprite->token==0x4400b7);}
            frame(0x2000);frame();CHECK(state.frontend_categories.selected_84b7f0==1);
            const auto starts=effects.stats.starts;
            frame(4);frame();CHECK(state.state2_selector_child_state==0x3a&&effects.stats.starts>starts);
            frame(0x1000);frame();CHECK(state.frontend_categories.selected_84b7f0==0);
            frame(4);for(unsigned f=0;f<30;++f)frame();CHECK(state.state2_selector_child_state==0x3b);
            CHECK(state.frontend_missions.constructed&&!state.frontend_missions.fault);
            CHECK(!state.frontend_missions.glyphs.empty());
            const auto race_count=state.frontend_category_progress.category_counts[0];
            state.frontend_category_progress.category_counts[0]=0;
            CHECK(native_runtime_attach_race_assignment(*runtime,assignments));
            CHECK(!state.frontend_category_progress.categories_ready);
            frame();CHECK(state.frontend_category_progress.category_counts[0]==race_count);
            auto missions=Bytes(state.frontend_missions.object.data(),PcMissionOwnerBytes);
            CHECK(state.frontend_sprites.get(missions.u32(0x3c+8))->token==0x4400ac);
            CHECK(missions.u32(0x8fc+8)==~0u); // PC grade7 has no sprite
            frame(4);frame();CHECK(missions.u8(0x34)==1&&missions.u32(0x38)==0);
            frame(0x2000);frame();CHECK(missions.u32(0x38)==0); // fresh profile: second race locked
            const auto mission_depth=owner.u32(0x484);
            frame(4);frame(); // original next key is 0A, not a direct GAME command
            // The mission owner's next key 0A now opens the native car select
            // (factory 4417B0, owner 4C8DE0) instead of stopping at a missing key.
            CHECK(missions.u32(4)==0xa&&state.frontend_last_missing_key==~0u);
            CHECK(state.state2_selector_child_state==10&&state.car_select.menu.constructed&&!state.car_select.fault);
            // Car select end to end on the host: mode-32 control's 48BFE0 tick
            // (49E3E0), the event control 43FAB0 with the preview callbacks
            // (4A7270/4A5B20 in the runtime, CAMERA 484EE0/485FE0 and SCN_ENV
            // in the renderer owner) and one 449050 frame whose event 8
            // display 49F500 draws the selected car through 46C140.
            {
                DrivingDataPack driving;CHECK(parse_driving_data_pack(EmbeddedDrivingData,EmbeddedDrivingDataSize,driving,&error));
                runtime->game_mode.driving_data=&driving;
                {NativeRuntimeContext meta;CHECK(parse_event_metadata(EmbeddedEventMetadata,EmbeddedEventMetadataSize,meta,&error));
                 runtime->event_descriptors=meta.event_descriptors;runtime->event_functions=meta.event_functions;runtime->mode_descriptors=meta.mode_descriptors;}
                PcSoftD3D9Device soft(640,480);PcSceneRenderer scene(soft);
                auto& shared=state.car_select;shared.preview_inits=0;shared.preview_controls=0;
                // 844A08: the loader's common\sel_dl_edit0.tgt read (4239C0/423CB0),
                // not run by this fixture; the same retail file is read here.
                if(shared.select_table_844a08.empty()){
                    std::ifstream in(retail/"Common"/"sel_dl_edit0.tgt",std::ios::binary);
                    shared.select_table_844a08.assign(std::istreambuf_iterator<char>(in),{});
                    CHECK(shared.select_table_844a08.size()==LoaderAssetSelectTableBytes);
                }
                scene.bind_sun_lists(&shared.sun_lists_7d26a8);scene.bind_select_table(&shared.select_table_844a08);
                scene.clear_colour_89bd5c=&shared.clear_colour_89bd5c;
                scene.vehicle_camera=[&shared](Bytes& v,Bytes& c){v=Bytes(shared.car_799d18.data(),shared.car_799d18.size());
                    c=Bytes(shared.camera_79fe10.data(),shared.camera_79fe10.size());return true;};
                scene.body_view=[&shared](Bytes& b){b=Bytes(shared.body_82e7f0.data(),shared.body_82e7f0.size());return true;};
                scene.bank_source=[&assets](std::uint32_t id,std::vector<std::uint8_t>& pmt){
                    const auto* record=retail_asset_lookup(assets,id,id==0xbbu||id==0xbau?2u:11u);
                    return record&&retail_asset_inflate_sz(record->bytes,pmt);};
                // SCN_ENV (386) and EXEC_DRAW (396) are startup events on the PC.
                for(unsigned id:{386u,396u}){outrun::driving::event_setup_440110(runtime->event_state,id,runtime->event_descriptors[id].function_id,
                    runtime->event_descriptors,runtime->event_functions);}
                struct Bridge{NativeRuntimeContext* rt;PcSceneRenderer* scene;unsigned owned{},other{};};
                Bridge bridge{runtime.get(),&scene};
                const outrun::driving::PcEventServices events{&bridge,[](void* u,std::uint32_t cb,std::uint32_t,std::uint32_t id){
                    auto& b=*static_cast<Bridge*>(u);auto& sc=*b.scene;
                    const NativeEnvironmentHook env=[&sc](Bytes v,Bytes c,const std::function<void(outrun::driving::PcEnvironmentFrame&,
                        const outrun::driving::CourseCollisionTables&,outrun::driving::PcEnvironmentBlendContext&)>& fn){sc.with_environment(v,c,fn);};
                    if(native_car_event_invoke(*b.rt,cb,env,sc.matrices())||sc.invoke(cb,0,id,b.rt->mode_state.current,b.rt->game_mode.game_variant))++b.owned;
                    else ++b.other;},nullptr};
                for(unsigned f=0;f<60;++f){
                    frame();CHECK(native_car_loader_tick_48bfe0(*runtime));
                    outrun::driving::event_control_43fab0(runtime->event_state,events);
                }
                Bytes preview(shared.menu.object.data()+0x34,0x94);
                std::fprintf(stderr,"car select: fault=%x preview=%u ev8=%u ev385=%u inits=%u controls=%u camera=%u/%u owned=%u other=%u env=%u\n",
                    shared.fault,preview.u32(0),unsigned(runtime->event_state.slots[8].flags),unsigned(runtime->event_state.slots[385].flags),
                    shared.preview_inits,shared.preview_controls,scene.camera_inits,scene.camera_controls,bridge.owned,bridge.other,scene.environment_inits);
                {Bytes l(state.loader_83039c.data(),state.loader_83039c.size());std::fprintf(stderr,"slots %u %u %u %u %u loader %u/%u/%u %u/%u/%u %u/%u/%u desired %u %u %u child %u\n",shared.slot_calls[0],shared.slot_calls[1],shared.slot_calls[2],shared.slot_calls[3],shared.slot_calls[4],l.u32(0x10),l.u32(0x14),l.u32(0x18),l.u32(0x1c),l.u32(0x20),l.u32(0x24),l.u32(0x28),l.u32(0x2c),l.u32(0x30),l.u32(4),l.u32(8),l.u32(12),state.state2_selector_child_state);}
                CHECK(!shared.fault&&preview.u32(0)==1);
                CHECK((runtime->event_state.slots[8].flags&3u)==2u&&(runtime->event_state.slots[385].flags&3u)==2u);
                CHECK(shared.preview_inits==1&&shared.preview_controls>0&&scene.camera_inits==1&&scene.camera_controls>0);
                scene.render_frame(runtime->event_state,32);
                std::fprintf(stderr,"car select frame: displays=%u triangles=%llu pixels=%llu bank_failures=%u last=%s\n",scene.car_displays,
                    (unsigned long long)soft.triangles_drawn,(unsigned long long)soft.pixels_shaded,scene.bank_failures,scene.last_error.c_str());
                CHECK(scene.car_displays==1&&soft.triangles_drawn>1000&&soft.errors.empty());
                if(const char* png=std::getenv("OR2_CAR_SELECT_PNG"))CHECK(soft.write_png(png));
                // Confirm the car: 4C9290 action 4 with next key 0x10 opens the
                // transmission owner (key 16, 4DD410); its back returns here.
                frame(4);for(unsigned f=0;f<30;++f){frame();CHECK(native_car_loader_tick_48bfe0(*runtime));outrun::driving::event_control_43fab0(runtime->event_state,events);}
                std::fprintf(stderr,"after car confirm: child=%u trans=%d fault=%x/%x start model=%u colour=%u missing=%x/%x next=%x\n",state.state2_selector_child_state,
                    int(state.transmission.constructed),state.car_select.fault,state.transmission.fault,
                    unsigned(runtime->start_mode.course_choice_655b59),unsigned(runtime->start_mode.vehicle_colour_655b5a),state.frontend_last_missing_key,state.frontend_last_missing_action,
                    Bytes(state.car_select.menu.object.data(),0x1674).u32(4));
                CHECK(state.state2_selector_child_state==16&&state.transmission.constructed&&!state.transmission.fault);
                frame(0x2000);for(unsigned f=0;f<10;++f)frame();
                CHECK(Bytes(state.transmission.object.data(),0x1f4).u32(0xe8)==1);
                frame(8);for(unsigned f=0;f<30;++f){frame();CHECK(native_car_loader_tick_48bfe0(*runtime));outrun::driving::event_control_43fab0(runtime->event_state,events);}
                std::fprintf(stderr,"after transmission back: child=%u 84B210=%u fault=%x\n",state.state2_selector_child_state,
                    state.transmission_selection_84b210,state.transmission.fault);
                CHECK(state.transmission_selection_84b210==8&&state.state2_selector_child_state==10);
                {
                    // Confirm car and transmission: 4DD4A0 leaves with +4 = 4 and
                    // 4450A0 pushes the music owner (key 4, 4C9A90).
                    auto tick=[&](unsigned n){for(unsigned f=0;f<n;++f){frame();CHECK(native_car_loader_tick_48bfe0(*runtime));outrun::driving::event_control_43fab0(runtime->event_state,events);}};
                    const auto plays0=state.music_play_requests,stops0=state.music_stop_requests;
                    const int cursor0=state.music_globals.cursors_84b0ec[0];CHECK(state.music_globals.list_84b0f4==0&&cursor0<8);
                    frame(4);tick(30);frame(4);tick(60);
                    std::fprintf(stderr,"after transmission confirm: child=%u 84B210=%u 830374=%u music=%d/%x plays=%u last=%x missing=%x/%x mode=%u\n",
                        state.state2_selector_child_state,state.transmission_selection_84b210,unsigned(state.car_select.transmission_830374),
                        int(state.music.constructed),state.music.fault,state.music_play_requests,state.music_last_track,
                        state.frontend_last_missing_key,state.frontend_last_missing_action,runtime->mode_state.current);
                    CHECK(state.state2_selector_child_state==4&&state.music.constructed&&!state.music.fault);
                    CHECK(state.music_play_requests==plays0+1&&state.music_last_track==unsigned(0x21+cursor0));
                    // Right moves the list 0 cursor and previews the next track.
                    frame(0x2000);tick(10);
                    std::fprintf(stderr,"music right: cursor=%d list=%d plays=%u last=%x fault=%x\n",state.music_globals.cursors_84b0ec[0],
                        state.music_globals.list_84b0f4,state.music_play_requests,state.music_last_track,state.music.fault);
                    CHECK(state.music_globals.cursors_84b0ec[0]==cursor0+1&&state.music_play_requests==plays0+2&&state.music_last_track==unsigned(0x22+cursor0));
                    if(std::getenv("OR2_CAR_SELECT_START")){
                        // Confirm: 48B1D0 stores the track, the preview stops, and
                        // result 5 commits the owner (444350), which leaves START
                        // (mode 13) in +21C for the SUMO_FE mode control 49E3E0.
                        frame(4);tick(90);
                        Bytes root(state.object.data(),state.object.size());
                        std::fprintf(stderr,"after music confirm: 830364=%u stops=%u +21c=%x depth=%u missing=%x/%x\n",
                            unsigned(state.music_globals.track_830364),state.music_stop_requests,root.u32(0x21c),root.u32(0x484),
                            state.frontend_last_missing_key,state.frontend_last_missing_action);
                        CHECK(state.music_globals.track_830364==cursor0+1&&state.music_stop_requests==stops0+1&&!state.music.fault);
                        CHECK(root.u32(0x21c)==13u&&root.u32(0x484)==0u);
                        if(std::getenv("OR2_START_TRACE")){
                            auto owned_ready=[](void* u,std::uint32_t pc){return native_start_owned_resource_ready(*static_cast<NativeRuntimeContext*>(u),pc);};
                            auto owned_call=[](void* u,std::uint32_t pc,const std::uint32_t* a,std::size_t n,std::uint32_t& v){
                                return native_start_owned_call(*static_cast<NativeRuntimeContext*>(u),pc,a,n,v);};
                            const NativeModeServices services{runtime.get(),nullptr,owned_ready,owned_call};
                            // The same course/stage17/world/gameplay attachments as switch/source/main.cpp.
                            static CourseAssetPack course_assets;static Stage17AssetPack stage17;static WorldSourcePack world;
                            static CourseCollisionPack collision;static CourseWorldRuntime course_world;static DrivingDataPack driving;
                            std::vector<std::uint8_t> csc,course_pack;
                            CHECK(retail_asset_read_relative(assets,"Scripts/bin/csc_data_cvt.bin",csc,CourseCvtBlobBytes,&error)&&
                                  build_course_asset_pack(csc,course_pack,&error)&&
                                  parse_course_asset_pack(course_pack.data(),course_pack.size(),course_assets,&error)&&
                                  native_runtime_attach_course_assets(*runtime,course_assets));
                            CHECK(parse_stage17_asset_pack(EmbeddedStage17Assets,EmbeddedStage17AssetsSize,stage17,&error)&&
                                  native_runtime_attach_stage17_assets(*runtime,stage17));
                            CHECK(build_beac_world_source_from_retail(assets,course_assets,world,&error)&&native_runtime_attach_world_source(*runtime,world));
                            const auto* coli=world_source_bytes(world,world.entries[0]);
                            CHECK(coli&&parse_pc_coli0200(coli,world.entries[0].size,collision,&error)&&
                                  course_world.admit_lane(0u,collision.pc_coli0200.data(),collision.pc_coli0200.size(),{},&error)&&
                                  course_world.set_transform(0u,course_world_identity_transform(),&error));
                            CHECK(parse_driving_data_pack(EmbeddedDrivingData,EmbeddedDrivingDataSize,driving,&error)&&
                                  native_runtime_attach_gameplay_assets(*runtime,course_world,driving));
                            std::fprintf(stderr,"variant=%u mode=%u stage14=%u shared=%u/%u/%u pend=%u/%u front=%u\n",runtime->game_mode.game_variant,runtime->mode_state.current,
                                state.loader_stage14_passes,state.shared_loader.stage_83db18,state.shared_ready_count,unsigned(state.shared_resource_ready.size()),
                                state.shared_resource_pending,state.frontend_resource_pending,state.frontend_ready_count);
                            for(unsigned f=0;f<200;++f){
                                if(runtime->mode_state.current==32u)frame();
                                const bool ok=native_runtime_mode_control(*runtime,services);
                                if(f<5||f%20==0)std::fprintf(stderr,"f%u ok=%d mode=%u start stage=%u owner=%u missing=%x/%x game=%d preset=%u attempts=%u waits=%u loaded=%u\n",f,int(ok),runtime->mode_state.current,
                                    runtime->start_mode.stage,runtime->start_mode.scene_owner_stage,runtime->start_mode.last_missing_service,
                                    runtime->start_mode.scene_owner_missing_service,int(runtime->game_mode.active),runtime->start_mode.course_preset,
                                    runtime->start_mode.course_load_attempts,runtime->start_mode.gate_waits,runtime->game_mode.course_load_success);
                            }
                        }
                        return 0;
                    }
                    // Back restarts the menu music (401000 track 0x1E) and pops the
                    // stack to the car select (the transmission overlay was closed
                    // by the 4450A0 push).
                    frame(8);tick(40);
                    std::fprintf(stderr,"music back: child=%u stops=%u last=%x fault=%x missing=%x/%x\n",state.state2_selector_child_state,
                        state.music_stop_requests,state.music_last_track,state.music.fault,state.frontend_last_missing_key,state.frontend_last_missing_action);
                    CHECK(state.music_stop_requests==stops0+1&&state.music_last_track==0x1e&&state.state2_selector_child_state==10&&!state.music.fault);
                }
                runtime->game_mode.driving_data=nullptr;
            }
            CHECK(owner.u32(0x484)==mission_depth+1&&runtime->mode_state.current==32);
            // Back leaves the car select (4C8D90 suspend: preview 48C450 closes
            // CAMERA 385 and events 8..31) and returns to the missions.
            frame(8);for(unsigned f=0;f<30;++f){frame();CHECK(native_car_loader_tick_48bfe0(*runtime));}
            std::fprintf(stderr,"car select back: child=%u depth=%u/%u fault=%x ev385=%u slots=%u,%u\n",state.state2_selector_child_state,
                owner.u32(0x484),mission_depth,state.car_select.fault,unsigned(runtime->event_state.slots[385].flags),
                state.car_select.slot_calls[0],state.car_select.slot_calls[4]);
            CHECK(state.state2_selector_child_state==0x3b&&owner.u32(0x484)==mission_depth&&!state.car_select.fault);
            // The missions owner is back at its race list (+34 = 0): the next
            // back leaves it for the categories.
            CHECK(missions.u8(0x34)==0);
            frame(8);for(unsigned f=0;f<30;++f)frame();CHECK(state.state2_selector_child_state==0x3a);
            frame(8);for(unsigned f=0;f<30;++f)frame();CHECK(state.state2_selector_child_state==2);
            frame(8);for(unsigned f=0;f<30;++f)frame();CHECK(state.state2_selector_child_state==1);
            CHECK(state.frontend_sprites.used(0)==main_layer_instances);
        }
        // An actually owned request cannot be silently dropped if its native
        // release service is absent. A clean offline owner needs no such leaf.
        request.put32(0x24,0xdeadbeef);frame(4);
        runtime->input_state.frontend.feature_mask=0;
        CHECK(native_runtime_event_function36_invoke(*runtime,0x49e4a0,405));
        CHECK(state.frontend_choice_faults[1]==0x525f80&&state.frontend_ui_missing_pc==0x525f80&&request.u32(0x24)==0xdeadbeef);
        for(unsigned f=0;f<5;++f){CHECK(native_runtime_event_function36_invoke(*runtime,0x49e4a0,405));CHECK(state.frontend_choice_faults[1]==0x525f80);}
        request.put32(0x24,0); // discard only the injected non-resource fixture
        CHECK(native_runtime_event_function36_invoke(*runtime,0x49e4c0,405));
        // Missing welcome audio latches too: even a valid saved license must
        // not hide a disconnected sound producer by advancing to key 1.
        CHECK(native_runtime_event_function36_invoke(*runtime,0x49e490,405));
        owner.put32(0x218,1);owner.put32(0,7);state.frontend_init_calls=2;
        outrun::driving::frontend_bulk_loader_initialize_4e85e0(state.frontend_bulk_loader);
        runtime->event_state.slots[405].flags=2;runtime->event_state.slots[405].disp_callback=0x49e4b0;
        for(unsigned f=0;f<30;++f)frame();
        state.frontend_effect=nullptr;runtime->input_state.menu_confirm=true;
        CHECK(native_runtime_event_function36_invoke(*runtime,0x49e4a0,405));
        CHECK(state.frontend_ui_missing_pc==0x4249f0&&!state.frontend_profiles.queried);
        state.frontend_effect=effect;runtime->input_state.menu_confirm=false;
        for(unsigned f=0;f<5;++f)CHECK(native_runtime_event_function36_invoke(*runtime,0x49e4a0,405));
        CHECK(state.frontend_ui_missing_pc==0x4249f0&&!state.frontend_profiles.queried);
        CHECK(native_runtime_event_function36_invoke(*runtime,0x49e4c0,405));
        state.license_owners=nullptr;
        CHECK(switch_renderer_set_frontend_glyphs(renderer,{}));CHECK(switch_renderer_set_frontend_images(renderer,{}));
        CHECK(switch_renderer_set_frontend_icons(renderer,{}));CHECK(bind_timing(pool)&&switch_renderer_attach_frontend_sprites(renderer,pool));
    }
    // Parent owner command -> real ETC timing -> the same renderer submission
    // used by live frontend instances. No synthetic ready/icon callback.
    {
        std::array<std::uint8_t,0x68c> bytes{};Bytes commands(bytes.data(),bytes.size());
        outrun::driving::PcUiNotifyGlobals globals{};unsigned result{};
        CHECK(ui.commands(0x442ac0,commands,nullptr,0,globals,result));
        const unsigned visible[]{1,0},hide[]{0,0},menu=1;
        CHECK(ui.commands(0x447000,commands,visible,2,globals,result));
        CHECK(ui.commands(0x446ea0,commands,&menu,1,globals,result)&&result==1);
        CHECK(pool.used(14)==4);
        for(unsigned frame=0;frame<60;++frame){
            pool.tick();CHECK(ui.commands(0x446cf0,commands,nullptr,0,globals,result));
            CHECK(switch_renderer_draw(renderer));
        }
        CHECK(pool.get(commands.u32(0x414))->token==0x2c013b);
        CHECK(pool.get(commands.u32(0x414))->frame==25);
        CHECK(ui.commands(0x447000,commands,hide,2,globals,result));
        for(unsigned frame=0;frame<30;++frame){
            pool.tick();CHECK(ui.commands(0x446cf0,commands,nullptr,0,globals,result));
            CHECK(switch_renderer_draw(renderer));
        }
        CHECK(pool.used(14)==0);CHECK(ui.commands(0x447090,commands,nullptr,0,globals,result));
    }
    // Complete original title window/list -> shared input -> navigation ->
    // confirmation -> close. Retail ETC scenes go through the real renderer.
    for(unsigned state:{2u,3u})for(unsigned variant:{1u,4u}){
        std::array<std::uint8_t,TitleOwnerPcSize> owner{};Bytes parent(owner.data(),owner.size());unsigned shared_input{};std::uint8_t game{},flag{};
        CHECK(title_owner_construct_complete_4d7140(owner.data(),owner.size(),game,flag,shared_input));
        FrontendChoiceList choices(parent.sub(0x34,PcFrontendChoiceListBytes),ui,*fonts);FrontendTextLines lines;
        TitleMenuGlobals globals{state,variant,variant==4,7};TitleOwnerGlobals layers;
        for(unsigned i=0;i<3;++i)layers.scene_ids[i]=(state==2?8:14)+i;
        CHECK(frontend_title_init_4d5e40(parent,choices,*fonts,table,lines,globals,layers,{},shared_input));
        CHECK(choices.size()==(state==2?4u:variant==4?5u:6u));
        struct Menu {FrontendChoiceList* choices;FrontendTextLines* lines;FrontendInputSnapshot input;
            unsigned* previous;unsigned sounds{},feedback{};std::vector<FrontendGlyph> glyphs;
        } menu{&choices,&lines,{},&shared_input};
        TitleControllerServices services{&menu,[](void* p,unsigned pc,std::uint8_t* object,std::size_t off,int arg,unsigned& result){
            auto& m=*static_cast<Menu*>(p);result=0;
            if(pc==0x48dda0){m.glyphs.clear();return m.choices->display_48dda0(0,*m.lines,m.glyphs);}
            if(pc==0x48f5f0)return frontend_input_action_48f5f0(object+off,0x34,m.input,arg,*m.previous,&m,
                [](void* q,unsigned,int){++static_cast<Menu*>(q)->feedback;return true;},result);
            if(pc==0x48dc10||pc==0x48dc60){bool sound{};const bool ok=m.choices->move(pc==0x48dc60,sound);if(sound)++m.sounds;return ok;}
            if(pc==0x48e440)return m.choices->clear_48e440();
            if(pc==16){frontend_window_suspend_48ca30(Bytes(object+off,PcFrontendWindowBytes));return true;}
            return false;
        },state};
        unsigned result{};for(unsigned frame=0;frame<20;++frame){
            CHECK(title_controller_motion_48cc00(owner.data()+0x9bc,PcFrontendWindowBytes,1));
            CHECK(title_menu_control_4d7300(owner.data(),owner.size(),globals,services,result));
            std::vector<FrontendListImage> images;std::vector<FrontendWindowIcon> icons;
            CHECK(frontend_window_display_48c5f0(parent.sub(0x9bc,PcFrontendWindowBytes),*fonts,lines,menu.glyphs,images,icons));
            CHECK(switch_renderer_set_frontend_glyphs(renderer,menu.glyphs));CHECK(switch_renderer_set_frontend_images(renderer,images));
            CHECK(switch_renderer_set_frontend_icons(renderer,icons));
            CHECK(switch_renderer_draw(renderer));
        }
        menu.input.feature_mask=0x800;CHECK(title_menu_control_4d7300(owner.data(),owner.size(),globals,services,result));
        CHECK(parent.u32(0x34)==1&&menu.sounds==1&&menu.feedback==1);
        menu.input.feature_mask=4;CHECK(title_menu_control_4d7300(owner.data(),owner.size(),globals,services,result));
        CHECK(parent.u32(0x9ac)==2&&parent.u32(0x9b0)==1&&parent.u32(0x9b4)==(state==2?6u:variant==4?15u:12u));
        CHECK(title_menu_close_4d6090(owner.data(),owner.size(),services));CHECK(parent.u8(0x9b8)&&choices.size()==0&&!parent.u8(0x9bc+0x34));
        CHECK(pool.used(layers.scene_ids[1])==0&&pool.used(layers.scene_ids[2])==0);
        CHECK(switch_renderer_set_frontend_glyphs(renderer,{}));CHECK(switch_renderer_set_frontend_images(renderer,{}));
        CHECK(switch_renderer_set_frontend_icons(renderer,{}));
    }
    std::array<std::uint8_t,PcLicenseEditorBytes> object{};Bytes b(object.data(),object.size());
    FrontendLicenseWidgets widgets(b,ui,*fonts,table);unsigned repeat{},action{};CHECK(widgets.construct_editor_resources(repeat));
    int input=-1;widgets.external(&input,[](void* p,LicenseEditorServices&,unsigned pc,std::size_t,const unsigned*,std::size_t,unsigned& r){
        if(pc==0x4249f0)return true;if(pc!=0x48f5f0)return false;r=unsigned(*static_cast<int*>(p));return true;
    });
    bool focus{};widgets.keyboard_focus(&focus,[](void* p,bool v){*static_cast<bool*>(p)=v;return true;});
    PcLicense profile{};frontend_license_reset_4471a0(profile,1234);std::array<std::uint8_t,16> name{{'O','R','2'}};
    frontend_license_name_4dd590(profile,name);CHECK(license_editor_init_4dd7c0(b,profile,widgets.services()));
    auto frame=[&]{pool.tick();widgets.begin_frame();CHECK(license_editor_tick_4de2b0(b,profile,widgets.services(),action));
        CHECK(switch_renderer_set_frontend_glyphs(renderer,widgets.glyphs()));CHECK(switch_renderer_draw(renderer));};
    for(unsigned i=0;i<62;++i)frame();CHECK(b.u32(0x38)==1);
    CHECK(switch_renderer_stats(renderer).font_draws==3);
    input=0;frame();CHECK(focus);input=-1;for(unsigned i=0;i<62;++i)frame();
    CHECK(pool.used(12)==2);input=0;frame();CHECK(frontend_text_value_48eec0(b.sub(0x5ec,PcTextWidgetBytes))=="OR21");
    input=-1;frame();CHECK(switch_renderer_stats(renderer).font_draws==4); // PC display precedes this tick's keyboard edit
    input=2;frame();input=5;frame();input=0;frame();input=-1;for(unsigned i=0;i<62;++i)frame();
    CHECK(!focus&&pool.used(12)==0);CHECK(std::string(reinterpret_cast<const char*>(profile.data()))=="OR21");
    CHECK(switch_renderer_stats(renderer).font_frames>100);
    auto glyphs=widgets.glyphs();CHECK(!glyphs.empty());const auto original=glyphs;
    glyphs[0].token=10;CHECK(!switch_renderer_set_frontend_glyphs(renderer,glyphs));
    glyphs=original;glyphs[0].x=std::numeric_limits<float>::infinity();CHECK(!switch_renderer_set_frontend_glyphs(renderer,glyphs));
    glyphs.assign(561,original[0]);CHECK(!switch_renderer_set_frontend_glyphs(renderer,glyphs));
    CHECK(switch_renderer_draw(renderer));CHECK(switch_renderer_stats(renderer).font_draws==4); // rejection is atomic
    CHECK(switch_renderer_set_frontend_glyphs(renderer,{}));CHECK(switch_renderer_draw(renderer));CHECK(switch_renderer_stats(renderer).font_draws==0);
    FrontendGlyph g{0,0,0,16,16,1,1,0,0,~0u,0};FrontendGlyphQuad q;
    CHECK(frontend_glyph_quad(g,fonts->textures[0],q));CHECK(q[0].uv[0]==0&&q[0].uv[1]==1);
    CHECK(q[1].uv[0]==15.f/fonts->textures[0].width);
    for(unsigned mode:{0u,4u,20u,21u,~0u}){g.mode=mode;CHECK(frontend_glyph_layer(g)==(mode==~0u?0:std::min(mode,20u)));}
    // The selector's populated profile card must use those same live sprite
    // instances and font submissions (not an independently drawn test menu).
    pool.reset();std::vector<std::uint8_t> chooser(PcLicenseChooserBytes);Bytes cb(chooser.data(),chooser.size());
    FrontendProfiles profiles;
    RetailAssetStore progress_store;RaceAssetPack progress_races;RaceAssignmentPack progress_assignment;
    auto progress_runtime=std::make_unique<NativeRuntimeContext>();
    FrontendLicenseWidgets panels(cb,ui,*fonts,table);
    CHECK(panels.construct_widget(0xa08,repeat));CHECK(panels.construct_widget(0xe94,repeat));
    for(unsigned i=0;i<3;++i)CHECK(panels.construct_panel_4e0ab0(0x3580+i*0x225c,repeat));
    CHECK(panels.initialize_panels_4e0c70());
    for(auto& l:profiles.licenses)frontend_license_reset_4471a0(l,123);
    profiles.licenses[2]=profile;profiles.licenses[2][0x3f4]|=1;
    license_chooser_list_4e1c00(cb,profiles);cb.put32(0x3c,1);cb.put32(0x40,2);cb.put32(0x44,0);cb.put32(0x38,3);
    // Use the real retail loader -> mapping/course provider -> completion ->
    // card chain, not zero category counts that happen to display "0%".
    CHECK(retail_asset_store_open(progress_store,retail.string(),&error));
    CHECK(load_race_asset_pack_file((retail/"Scripts/bin/Races.bin").string().c_str(),progress_races,&error));
    CHECK(load_race_assignment_pack_file((retail/"Scripts/bin/RaceAssignment.bin").string().c_str(),progress_assignment,&error));
    CHECK(native_runtime_attach_retail_assets(*progress_runtime,progress_store));
    CHECK(native_runtime_attach_race_assets(*progress_runtime,progress_races));
    CHECK(native_runtime_attach_race_assignment(*progress_runtime,progress_assignment));
    CHECK(native_runtime_event_function36_invoke(*progress_runtime,0x49e490,405));
    progress_runtime->mode_state.current=32;
    panels.profiles(profiles,progress_runtime.get(),[](void* p,const PcLicense& l,double& result){
        return native_runtime_license_completion(*static_cast<NativeRuntimeContext*>(p),l,result);
    });
    CHECK(!panels.populate_panels_4e2150()); // data not yet bound, no fake completion
    auto& progress_state=progress_runtime->event_function36;
    Bytes progress_owner(progress_state.object.data(),progress_state.object.size());
    progress_owner.put32(0x218,1);progress_owner.put32(0,7);progress_state.frontend_init_calls=2;
    outrun::driving::frontend_bulk_loader_initialize_4e85e0(progress_state.frontend_bulk_loader);
    CHECK(native_runtime_event_function36_invoke(*progress_runtime,0x49e4a0,405));
    CHECK(progress_state.frontend_bulk_ready_count==64);
    CHECK(panels.populate_panels_4e2150());panels.begin_frame();CHECK(panels.display_chooser_4e3240());
    CHECK(switch_renderer_set_frontend_glyphs(renderer,panels.glyphs()));CHECK(switch_renderer_draw(renderer));
    CHECK(switch_renderer_stats(renderer).font_draws==panels.glyphs().size());CHECK(panels.glyphs().size()==20);
    CHECK(pool.get(cb.u32(0x555c+8))->token==0x44006a);
    CHECK(panels.release_panels_4e0d80());cb.put32(0x44,1);CHECK(panels.populate_panels_4e2150());
    panels.begin_frame();CHECK(panels.display_chooser_4e3240());
    CHECK(switch_renderer_set_frontend_glyphs(renderer,panels.glyphs()));CHECK(switch_renderer_draw(renderer));
    CHECK(panels.release_panels_4e0d80());CHECK(switch_renderer_set_frontend_glyphs(renderer,{}));
    CHECK(panels.construct_window(0x9d54,repeat));CHECK(panels.construct_list(0xb004));cb.put32(0x44,0);
    CHECK(panels.open_context_4e12b0());panels.owner_clock(1,2);panels.list_timer(0);
    unsigned result{};auto& services=panels.services();CHECK(services.call(services.user,8,0x9d54,nullptr,0,result));
    panels.begin_frame();CHECK(services.call(services.user,12,0x9d54,nullptr,0,result));CHECK(services.call(services.user,0x4ed3e0,0xb004,nullptr,0,result));
    CHECK(switch_renderer_set_frontend_images(renderer,panels.images()));CHECK(switch_renderer_set_frontend_glyphs(renderer,panels.glyphs()));
    CHECK(switch_renderer_draw(renderer));CHECK(switch_renderer_stats(renderer).menu_image_draws==panels.images().size());
    auto invalid_images=panels.images();invalid_images[0].token=0x300ff;CHECK(!switch_renderer_set_frontend_images(renderer,invalid_images));
    CHECK(switch_renderer_stats(renderer).menu_image_draws==panels.images().size());
    invalid_images=panels.images();invalid_images[0].layer=std::numeric_limits<float>::infinity();
    CHECK(!switch_renderer_set_frontend_images(renderer,invalid_images));
    invalid_images=panels.images();invalid_images[0].width=std::numeric_limits<float>::quiet_NaN();invalid_images[0].pc=0x42d300;
    CHECK(!switch_renderer_set_frontend_images(renderer,invalid_images));
    invalid_images.assign(561,panels.images()[0]);CHECK(!switch_renderer_set_frontend_images(renderer,invalid_images));
    // Original deletion confirmation: localized No/Yes text plus the actual
    // SUMO_FE row backgrounds, not a replacement platform dialog.
    CHECK(panels.construct_window(0xb040,repeat));CHECK(panels.construct_choice_list(0xc2f0,repeat));
    panels.external(&input,[](void* p,LicenseEditorServices&,unsigned pc,std::size_t,const unsigned*,std::size_t,unsigned& r){
        if(pc==0x4249f0)return true;if(pc!=0x48f5f0)return false;r=unsigned(*static_cast<int*>(p));return true;
    });
    for(unsigned cycle=0;cycle<4;++cycle){
        CHECK(panels.open_delete_4e16a0());CHECK(cb.u32(0xc2f0)==0&&panels.choice_list(0xc2f0)->size()==2);
        CHECK(services.call(services.user,8,0xb040,nullptr,0,result));
        for(unsigned frame=0;frame<8;++frame){
            input=frame%2?4:2;CHECK(panels.control_delete_4e17e0(result)&&result==0);
            panels.begin_frame();CHECK(panels.display_delete_4e1860());
            CHECK(pool.used(5)==2);CHECK(switch_renderer_set_frontend_images(renderer,{}));CHECK(switch_renderer_set_frontend_glyphs(renderer,{}));
            CHECK(switch_renderer_set_frontend_images(renderer,panels.images()));CHECK(switch_renderer_set_frontend_glyphs(renderer,panels.glyphs()));CHECK(switch_renderer_draw(renderer));
        }
        input=0;CHECK(panels.control_delete_4e17e0(result)&&result==~0u); // default is No
        input=4;CHECK(panels.control_delete_4e17e0(result)&&result==0);
        input=0;CHECK(panels.control_delete_4e17e0(result)&&result==1); // explicit Yes
        input=1;CHECK(panels.control_delete_4e17e0(result)&&result==~0u); // Cancel never confirms
        CHECK(services.call(services.user,16,0xb040,nullptr,0,result));
        CHECK(services.call(services.user,0x48e440,0xc2f0,nullptr,0,result));CHECK(pool.used(5)==0);
    }
    CHECK(panels.close_context_4e1680());CHECK(switch_renderer_set_frontend_images(renderer,{}));
    CHECK(panels.construct_panel_4e0ab0(0x1324,repeat));
    std::array<std::uint8_t,0x1c> manager_bytes{};std::array<std::uint8_t,0x94> remote_bytes{};
    Bytes remote(remote_bytes.data(),remote_bytes.size());Bytes(manager_bytes.data(),manager_bytes.size()).put32(0x18,0x920002);
    for(unsigned i=0;i<8;++i)remote.put8(0x20+i,std::uint8_t("NETWORK"[i]));
    remote.putf(0x60,51.25f);remote.putf(0x64,62.5f);remote.putf(0x68,4500);remote.putf(0x80,2000);remote.put32(0x84,10000);
    const outrun::driving::PcNativeHandleBinding bindings[]{{0x920001,manager_bytes.data(),manager_bytes.size()},{0x920002,remote_bytes.data(),remote_bytes.size()}};
    LicenseSpecialProfileSource source{0,0x920001,{bindings,2}};panels.special_profile_source(source);
    CHECK(panels.populate_special_4e0dd0());cb.put32(0x38,11);panels.begin_frame();CHECK(panels.display_chooser_4e3240());
    CHECK(!panels.glyphs().empty());CHECK(switch_renderer_set_frontend_glyphs(renderer,panels.glyphs()));
    CHECK(switch_renderer_draw(renderer));CHECK(switch_renderer_stats(renderer).font_draws==panels.glyphs().size());
    // Vehicle selection uses the actual retail animation bank and renderer
    // evaluation, not the uniform timing fixtures used by the unit/oracle
    // tests. The preview lifecycle, preloader and creation queue are real;
    // their resource/event/scene leaves are captured. This does NOT validate
    // the unbound 3D preview renderer or transmission owner.
    CHECK(switch_renderer_set_frontend_glyphs(renderer,{}));
    CHECK(switch_renderer_set_frontend_images(renderer,{}));
    CHECK(switch_renderer_set_frontend_icons(renderer,{}));
    for(unsigned locked=0;locked<2;++locked)for(unsigned index=0;index<30;++index){
        FrontendSprites vehicle_sprites;CHECK(bind_timing(vehicle_sprites)&&switch_renderer_attach_frontend_sprites(renderer,vehicle_sprites));
        FrontendUiResources vehicle_ui{vehicle_sprites};FrontendVehicleMenu car_menu;
        std::array<std::uint8_t,0xe00> owner{};Bytes root_owner(owner.data(),owner.size());
        outrun::driving::PcUiNotifyGlobals notify;PcLicense license{};license.fill(locked?0:255);
        FrontendInputSnapshot buttons;unsigned repeat_key{},action{};
        CHECK(vehicle_ui.commands(0x442ac0,root_owner.sub(0x51c,owner.size()-0x51c),nullptr,0,notify,action));
        MenuAudio audio;CHECK(audio.load_file((retail/"Sound/MENU.pak").string(),error));
        CHECK(audio.load_frontend_file((retail/"Sound/FE.pak").string(),error));
        vehicle_ui.effect_user=&audio;vehicle_ui.effect_4249f0=[](void* p,unsigned id){std::string e;return static_cast<MenuAudio*>(p)->command(id,e);};
        std::vector<std::array<unsigned,4>> captured;
        FrontendVehicleMenuServices api{vehicle_ui,root_owner,notify,license,buttons,repeat_key};
        api.user=&captured;api.external=[](void* p,unsigned pc,Bytes,const unsigned* args,std::size_t n){
            switch(pc){case 0x4c50d0:case 0x4c50f0:case 0x4c5100:case 0x4c5110:
                case 0x48b130:case 0x48b190:case 0x48b150:break;default:return false;}
            if(n>3)return false;std::array<unsigned,4> row{pc,0,0,0};
            for(std::size_t i=0;i<n;++i)row[i+1]=args[i];
            static_cast<std::vector<std::array<unsigned,4>>*>(p)->push_back(row);return true;
        };
        FrontendVehicleLoader preloader;VehicleCreationQueue creation;
        std::array<std::uint8_t,0x100> shared_car{};std::array<std::uint8_t,12> shared_lists{};
        FrontendVehiclePreviewServices preview{Bytes(preloader.object.data(),preloader.object.size()),
            Bytes(shared_car.data(),shared_car.size()),Bytes(shared_lists.data(),shared_lists.size())};
        preview.creation=&creation;preview.user=&captured;
        preview.call=[](void* p,unsigned pc,const unsigned* args,std::size_t n){
            switch(pc){case 0x40ec60:case 0x44c0a0:case 0x49a650:case 0x44c0d0:
                case 0x440110:case 0x4401d0:case 0x440330:case 0x44c3d0:case 0x44a1a0:case 0x4f2210:break;default:return false;}
            if(n>3)return false;std::array<unsigned,4> row{pc,0,0,0};for(std::size_t i=0;i<n;++i)row[i+1]=args[i];
            static_cast<std::vector<std::array<unsigned,4>>*>(p)->push_back(row);return true;
        };
        preview.ready_448960=[](void*,unsigned,bool& ready){ready=true;return true;};
        FrontendVehicleLoaderServices loader_io{nullptr,[](void*,unsigned,unsigned){return true;},preview.ready_448960,[](void*,unsigned){return true;}};
        CHECK(frontend_vehicle_loader_init_48bf20(preloader,loader_io));api.preview=&preview;
        CHECK(frontend_vehicle_menu_construct_4c8de0(car_menu,repeat_key));
        car_menu.cursor_84b0e8=std::int8_t(index%15);car_menu.variant_84b0e9=std::int8_t(index/15);
        CHECK(frontend_vehicle_menu_init_4c9010(car_menu,api));
        auto draw_frame=[&]{api.timer+=1;vehicle_sprites.tick();CHECK(frontend_vehicle_loader_tick_48bfe0(preloader,loader_io));
            CHECK(frontend_vehicle_menu_control_4c9290(car_menu,api,action));
            CHECK(frontend_vehicle_menu_display_4c8d70(car_menu,api));
            CHECK(switch_renderer_draw(renderer));CHECK(car_menu.fault==0&&vehicle_ui.missing_pc==0);
            CHECK(switch_renderer_stats(renderer).frontend_draws>0);
        };
        for(unsigned cycle=0;cycle<3;++cycle){
            for(unsigned frame=0;frame<64;++frame)draw_frame();
            Bytes car(car_menu.object.data(),car_menu.object.size());
            const auto* sprite=vehicle_sprites.get(car.u32(0xcc+8));CHECK(sprite);
            CHECK(sprite->token==0x4400c0+unsigned(car_menu.variant_84b0e9));
            CHECK(sprite->first==20*unsigned(car_menu.cursor_84b0e8+1));
            CHECK(car.u32(0x34)==1);
            Bytes queued(creation.object.data(),creation.object.size());
            CHECK(queued.u8(0x109)==1&&queued.u32(8)==VehicleMenuModels[unsigned(car_menu.variant_84b0e9*15+car_menu.cursor_84b0e8)]);
            buttons.feature_mask=4;draw_frame();
            CHECK(action==(vehicle_unlocked_4c8f90(license,unsigned(car_menu.variant_84b0e9*15+car_menu.cursor_84b0e8))?4u:0u));
            buttons.feature_mask=8;draw_frame();CHECK(action==2);
            buttons={};CHECK(frontend_vehicle_menu_suspend_4c8d90(car_menu,api));
            CHECK(frontend_vehicle_menu_init_4c9010(car_menu,api));
            buttons.device_held=0x2000;draw_frame();buttons={};
            buttons.feature_mask=cycle%2?0x1000:0x2000;draw_frame();buttons={};
            api.colour_held_4536c0=true;draw_frame();api.colour_held_4536c0=false;
        }
        CHECK(frontend_vehicle_menu_suspend_4c8d90(car_menu,api));
        for(unsigned i=0;i<12;++i)CHECK(frontend_vehicle_loader_tick_48bfe0(preloader,loader_io));
        CHECK(frontend_vehicle_loader_empty_48bfc0(preloader));
        // Detach before the per-case pool dies. The fixture owns this entire
        // pool; production teardown must release only its own resources.
        CHECK(bind_timing(pool)&&switch_renderer_attach_frontend_sprites(renderer,pool));
    }
    switch_renderer_shutdown(renderer);CHECK(!switch_renderer_frontend_fonts(renderer));
    CHECK(switch_renderer_initialize(renderer,nullptr,error,nullptr,nullptr,nullptr,nullptr));CHECK(!switch_renderer_frontend_fonts(renderer));
    CHECK(!switch_renderer_set_frontend_glyphs(renderer,original));switch_renderer_shutdown(renderer);
    // The copy keeps the source permissions (read-only on some installations): replace it.
    fs::remove(root/"Sprite/spr_font_xst.sz");
    {std::ofstream damaged(root/"Sprite/spr_font_xst.sz",std::ios::binary|std::ios::trunc);damaged<<"broken font file";}
    FrontendFontPack damaged_fonts;CHECK(!build_retail_font_pack(store,damaged_fonts,&error));CHECK(!error.empty());
    std::puts("ETC scenes + profile panels + editor/keyboard + context window raw images/text, corrupt assets and lifecycle pass (host, not GPU pixels)");
}
