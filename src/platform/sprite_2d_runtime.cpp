#include <algorithm>
#include "platform/sprite_2d_runtime.hpp"
#include "system/dev_hooks.hpp"
#include "platform/native_runtime.hpp"
#include "platform/pc_scene_renderer.hpp"
#include "platform/retail_asset_store.hpp"
#include "platform/race_hud.hpp"
#include "platform/race_end_runtime.hpp"
#include "platform/pc_screen.hpp"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <sstream>
namespace outrun::platform {
namespace {
// 755860[bank - 0x20]: SPRANI animation paths (read from OR2006C2C.EXE).
constexpr const char* AniPath755860[PcSpriteBankCount-0x20u]={
    "SPRANI/ani_SPRANIOLL_LOGO.sz","SPRANI/ani_SPRANI_LOGO_E.sz","SPRANI/ani_SPRANI_TITLE_CVT.sz",
    "SPRANI/ani_SPRANI_METER_246GTS.sz","SPRANI/ani_SPRANI_METER_288GTS.sz","SPRANI/ani_SPRANI_METER_360.sz",
    "SPRANI/ani_SPRANI_METER_365GTS.sz","SPRANI/ani_SPRANI_METER_F40.sz","SPRANI/ani_SPRANI_METER_F50.sz",
    "SPRANI/ani_SPRANI_METER_FX.sz","SPRANI/ani_SPRANI_METER_TESTA.sz","SPRANI/ani_SPRANI_GAME_CVT.sz",
    "SPRANI/ani_SPRANI_ETC_CVT.sz","SPRANI/ani_SPRANI_FIGHT.sz","SPRANI/ani_SPRANI_SELECTOR_CVT.sz",
    "SPRANI/ani_SPRANI_LOADING_CVT.sz","SPRANI/ani_SPRANI_METER_512BB.sz","SPRANI/ani_SPRANI_METER_250GTO.sz",
    "SPRANI/ani_SPRANI_ADV_CVT.sz","SPRANI/ani_SPRANI_COMMON_CVT.sz","SPRANI/ani_SPRANI_ROUTE_CVT.sz",
    "SPRANI/ani_SPRANI_ENDING_CVT.sz","SPRANI/ani_SPRANI_NAME_CMN_CVT.sz","SPRANI/ani_SPRANI_NAME5A_CVT.sz",
    "SPRANI/ani_SPRANI_NAME5B_CVT.sz","SPRANI/ani_SPRANI_NAME5C_CVT.sz","SPRANI/ani_SPRANI_NAME5D_CVT.sz",
    "SPRANI/ani_SPRANI_NAME5E_CVT.sz","SPRANI/ani_SPRANI_EFCT_FLOR.sz","SPRANI/ani_SPRANI_RANKING_CVT.sz",
    "SPRANI/ani_SPRANI_FLAG_RANK.sz","SPRANI/ani_SPRANI_CLAR_RANK.sz","SPRANI/ani_SPRANI_JENN_RANK.sz",
    "SPRANI/ani_SPRANI_HOLL_RANK.sz","SPRANI/ani_SPRANI_CONGRATS_CVT.sz","SPRANI/ani_SPRANI_STAFFROLL_CVT.sz",
    "SPRANI/ani_SPRANI_SUMO_FE_CVT.sz",nullptr,"SPRANI/ani_SPRANI_FRUITY_CVT.sz",
    "SPRANI/ani_SPRANI_SPLASH_CVT.sz","SPRANI/ani_SPRANI_SUMO_LOADING.sz","SPRANI/ani_SPRANI_SUMO_VSLOAD.sz",
    "SPRANI/ani_SPRANI_TAEXTRA_CVT.sz"};
// 639CB8[bank]: the English XST file of every bank (read from OR2006C2C.EXE;
// the other languages' columns follow it at 639DE4..).
constexpr const char* XstPath639cb8[PcSpriteBankCount]={
"sprite/spr_font_xst.sz",
nullptr,
nullptr,
"sprite/spr_etc_xst.sz",
nullptr,
nullptr,
nullptr,
nullptr,
nullptr,
nullptr,
nullptr,
nullptr,
nullptr,
nullptr,
nullptr,
nullptr,
nullptr,
nullptr,
"sprite/spr_select_xst.sz",
"sprite/spr_warning_xst.sz",
"sprite/spr_name_entry_xst.sz",
"sprite/spr_ranking_xst.sz",
nullptr,
"sprite/spr_sumo_fe_xst.sz",
nullptr,
nullptr,
nullptr,
nullptr,
nullptr,
nullptr,
nullptr,
nullptr,
"sprite/spr_sprani_logo_xst.sz",
"sprite/spr_sprani_logo_e_xst.sz",
"sprite/spr_sprani_title_cvt_xst.sz",
"sprite/spr_sprani_meter_246gts_xst.sz",
"sprite/spr_sprani_meter_288gts_xst.sz",
"sprite/spr_sprani_meter_360_xst.sz",
"sprite/spr_sprani_meter_365gts_xst.sz",
"sprite/spr_sprani_meter_f40_xst.sz",
"sprite/spr_sprani_meter_f50_xst.sz",
"sprite/spr_sprani_meter_fx_xst.sz",
"sprite/spr_sprani_meter_testa_xst.sz",
"sprite/spr_sprani_game_cvt_Exst.sz",
"sprite/spr_sprani_etc_cvt_Exst.sz",
"sprite/spr_sprani_fight_Exst.sz",
"sprite/spr_sprani_selector_cvt_Exst.sz",
"sprite/spr_sprani_loading_cvt_Exst.sz",
"sprite/spr_sprani_meter_512bb_xst.sz",
"sprite/spr_sprani_meter_250gto_xst.sz",
"sprite/spr_sprani_adv_cvt_xst.sz",
"sprite/spr_sprani_common_cvt_xst.sz",
"sprite/spr_sprani_route_cvt_Exst.sz",
"sprite/spr_sprani_ending_cvt_xst.sz",
"sprite/spr_sprani_name_cmn_cvt_xst.sz",
"sprite/spr_sprani_name5a_cvt_xst.sz",
"sprite/spr_sprani_name5b_cvt_xst.sz",
"sprite/spr_sprani_name5c_cvt_xst.sz",
"sprite/spr_sprani_name5d_cvt_xst.sz",
"sprite/spr_sprani_name5e_cvt_xst.sz",
"sprite/spr_sprani_efct_flor_xst.sz",
"sprite/spr_sprani_ranking_cvt_Exst.sz",
"sprite/spr_sprani_FLAG_RANK_Exst.sz",
"sprite/spr_sprani_CLAR_RANK_Exst.sz",
"sprite/spr_sprani_JENN_RANK_Exst.sz",
"sprite/spr_sprani_HOLL_RANK_Exst.sz",
"sprite/spr_sprani_congrats_cvt_Exst.sz",
"sprite/spr_sprani_staffroll_cvt_xst.sz",
"sprite/spr_sprani_sumo_fe_cvt_Exst.sz",
"sprite/spr_sprani_demosplash_cvt_xst.sz",
"sprite/spr_sprani_fruity_cvt_Exst.sz",
"sprite/spr_sprani_splash_cvt_xst.sz",
"SPRITE/spr_sprani_sumo_loading_Exst.sz",
"SPRITE/spr_sprani_sumo_vsload_Exst.sz",
"SPRITE/spr_sprani_taextra_cvt_xst.sz"};
void ensure_bank(NativeRuntimeContext& c,NativeSprite2d& o,PcD3D9Device& device,std::uint32_t bank){
    if(bank>=PcSpriteBankCount||o.bank_status.count(bank))return;
    auto& b=o.state.banks[bank];
    auto* store=c.event_function36.retail_assets;
    if(!store){o.bank_status[bank]="no retail store";return;}
    if(bank<0x20u){
        const char* path=XstPath639cb8[bank];std::string error;
        if(!path){o.bank_status[bank]="no XST path";return;}
        const auto* bytes=retail_asset_guest_path(*store,path,true,&error);
        if(bytes&&pc_sprite_bank_load_xst(b,device,*bytes,error))o.bank_status[bank]="xst loaded ("+std::to_string(b.textures.size())+" textures)";
        else{o.bank_status[bank]="xst failed: "+error;b.state=0;}
        return;
    }
    if(!AniPath755860[bank-0x20u]){o.bank_status[bank]="no SPRANI path";return;}
    const std::string ani=AniPath755860[bank-0x20u];
    std::string status,error;
    if(const auto* bytes=retail_asset_guest_path(*store,ani,true,&error);bytes&&pc_sprite_bank_load_animation(b,bank,*bytes,error))
        status="ani loaded";
    else status="ani failed: "+error;
    error.clear();
    const std::string xst=XstPath639cb8[bank]?XstPath639cb8[bank]:"";std::vector<std::uint8_t> inflated;
    if(!xst.empty()&&retail_asset_read_relative_inflated(*store,xst,inflated,128u*1024u*1024u,&error)&&pc_sprite_bank_load_xst(b,device,inflated,error))
        status+=", xst loaded ("+std::to_string(b.textures.size())+" textures)";
    else{status+=", xst failed: "+error;b.state=0;}
    o.bank_status[bank]=status;
}
}
const char* native_sprite2d_ani_path(std::uint32_t bank){return bank>=0x20u&&bank<PcSpriteBankCount?AniPath755860[bank-0x20u]:nullptr;}
const char* native_sprite2d_xst_path(std::uint32_t bank){return bank<PcSpriteBankCount?XstPath639cb8[bank]:nullptr;}
void native_sprite2d_ensure_bank(NativeRuntimeContext& c,PcD3D9Device& device,std::uint32_t bank){ensure_bank(c,native_sprite2d(c),device,bank);}
namespace {
// 42C860 / 42C720's 42CFE0 record: token, rectangle, scale, position, colour.
void glyph_record(NativeSprite2d& o,const FrontendGlyph& g){
    std::array<std::uint8_t,0x48> r{};
    auto w32=[&](unsigned off,std::uint32_t v){std::memcpy(r.data()+off,&v,4);};
    auto wf=[&](unsigned off,float v){std::memcpy(r.data()+off,&v,4);};
    w32(0,g.token);w32(4,std::uint32_t(g.left));w32(8,std::uint32_t(g.top));w32(0xc,std::uint32_t(g.right));w32(0x10,std::uint32_t(g.bottom));
    wf(0x14,g.scale_x);wf(0x18,g.scale_y);wf(0x24,g.x);wf(0x28,g.y);w32(0x2c,g.color);
    pc_image_record_42cfe0(o.state,r,float(std::int32_t(g.mode)));
}
}
void native_sprite2d_glyph_records(NativeRuntimeContext& c,PcD3D9Device& device,const std::vector<FrontendGlyph>& glyphs){
    if(glyphs.empty())return;
    auto& o=native_sprite2d(c);ensure_bank(c,o,device,0u);       // bank 0: the font XST
    o.state.update_index_8a8cdc=c.frame_state.update_index_8a8cdc;
    for(const auto& g:glyphs)glyph_record(o,g);
}
NativeSprite2d& native_sprite2d(NativeRuntimeContext& c){
    if(!c.sprite2d)c.sprite2d=std::make_shared<NativeSprite2d>();
    return *c.sprite2d;
}
bool native_sprite2d_event(NativeRuntimeContext& c,std::uint32_t callback){
    if(callback!=0x427e30u&&(callback!=0x427f70u||(c.mode_state.current!=16u&&!native_race_end_mode_active(c.mode_state.current))))return false;
    auto& o=native_sprite2d(c);auto& pool=c.event_function36.frontend_sprites;
    if(callback==0x427e30u){SpraniGlobals g{};sprani_init_427e30(pool,g);o.state.id_7551b4=g.current_7551b4;++o.inits_398;o.last_init_mode=c.mode_state.current;}
    else{pool.tick(sprani_clock_427f70(1u,std::int32_t(c.frame_state.update_index_8a8cdc),0,c.event_function36.title_pause_flag_95b214));++o.controls_398;}
    return true;
}
void native_sprite2d_set_frontend(NativeRuntimeContext& c,const std::vector<FrontendGlyph>& glyphs,const std::vector<FrontendListImage>& images,
                                  const std::vector<FrontendWindowIcon>& icons){
    auto& o=native_sprite2d(c);o.glyphs=glyphs;o.images=images;o.icons=icons;
}
bool native_sprite2d_display(NativeRuntimeContext& c,PcSceneRenderer& r,std::uint32_t callback){
    if(callback==0x49e4b0u){
        // Event 405 display: the frontend UI's image and text submissions of
        // this frame into the 2D queue (the widget displays' 42D280 / 42D300
        // and 42C860 -> 42CFE0 calls, images first).
        auto& o=native_sprite2d(c);auto& device=r.flush_context().device;
        if(o.images.empty()&&o.glyphs.empty()&&o.icons.empty())return true;
        ensure_bank(c,o,device,0u);ensure_bank(c,o,device,3u);
        o.state.update_index_8a8cdc=c.frame_state.update_index_8a8cdc;
        for(const auto& im:o.images){
            if(im.pc==0x42d280u)pc_image_42d280(o.state,im.token,im.x,im.y,std::uint32_t(im.frame),im.layer,im.color);
            else if(im.pc==0x42d300u)pc_image_42d300(o.state,im.mode,im.token,im.x,im.y,im.width,im.height,im.layer,im.color);
        }
        if(dev_hooks().trace_glyphs){std::fprintf(stderr,"49E4B0 glyphs:");for(const auto& g:o.glyphs)std::fprintf(stderr," [t%u m%u %.0f,%.0f c%08x]",g.token,g.mode,g.x,g.y,g.color);std::fprintf(stderr,"\n");}
        for(const auto& g:o.glyphs)glyph_record(o,g);
        // 429530(token, x, y, layer, frame): 428A10 under T(x, y) (race_hud
        // sprani_draw_428a10), then 429460 with that matrix and blend stack.
        auto& pool=c.event_function36.frontend_sprites;auto& stack=r.matrices();
        for(const auto& ic:o.icons){
            ensure_bank(c,o,device,ic.token>>16);
            SpraniGlobals g{};g.current_7551b4=o.state.id_7551b4;
            std::vector<SpraniDraw> draws;std::array<float,3> blend{};
            const auto t=sprani_translation(ic.x,ic.y);
            if(!sprani_draw_428a10(pool,g,stack,ic.token,&t,std::uint32_t(ic.layer),ic.frame,1.0f,1.0f,0.0f,draws,blend))continue;
            for(const auto& d:draws){
                const auto root=pc_sprani_scene_root(o.state,d.token);
                if(!root){++o.state.pool_missing_roots;continue;}
                driving::pc_matrix_push(stack);
                auto top=stack.current();for(unsigned k=0;k<16;++k)top.putf(k*4,d.matrix[k]);
                o.state.blend_9564e0=blend;o.state.id_7551b4=d.id_7551b4;o.state.mask_986b28=0;
                pc_sprani_render_429460(o.state,device,stack,root,d.frame,d.scale,d.layer);
                driving::pc_matrix_pop(stack);
            }
        }
        ++o.ui_displays;o.ui_glyphs+=std::uint32_t(o.glyphs.size());o.ui_images+=std::uint32_t(o.images.size());o.ui_icons+=std::uint32_t(o.icons.size());
        return true;
    }
    if(callback!=0x428170u)return false;
    auto& o=native_sprite2d(c);auto& pool=c.event_function36.frontend_sprites;
    auto& device=r.flush_context().device;
    for(const auto& in:pool.instances())if(in.allocated&&in.visible)ensure_bank(c,o,device,in.token>>16);
    o.state.update_index_8a8cdc=c.frame_state.update_index_8a8cdc;
    pc_sprite_pool_display_428170(o.state,device,r.matrices(),pool);
    ++o.displays;
    return true;
}
bool native_sprite2d_leaf(NativeRuntimeContext& c,PcSceneRenderer& r,std::uint32_t pc,std::uint32_t arg){
    if(pc!=0x42d710u)return false;
    auto& o=native_sprite2d(c);auto& fc=r.flush_context();
    // Port overlay (not original code): monospace lines right-aligned on the right edge, under the
    // mission bubble and clear of the left HUD (progress bar, Next Stage panel), queued in the
    // last layer (20) just before the frame's flush.
    if(!g_pc_overlay_text.empty()&&arg>20u){
        const auto* fonts=c.event_function36.frontend_fonts;
        if(fonts&&fonts->fonts[9].token==9u){
            const auto& font=fonts->fonts[9];
            constexpr float Scale=0.7f;
            std::size_t longest=0,line=0;
            for(const char ch:g_pc_overlay_text){if(ch==10){line=0;continue;}longest=std::max(longest,++line);}
            const int left=640-8-int(float(longest)*float(font.width)*Scale);
            // A black copy one pixel down-right first: readable on the sky and the road.
            std::vector<FrontendGlyph> glyphs;
            for(const auto [dx,colour]:{std::pair<int,std::uint32_t>{1,0xff000000u},{0,0xffffffffu}}){
                FrontendTextStyle style;style.scale_x=Scale;style.scale_y=Scale;style.color=colour;style.mode=20u;style.flags=1u;
                FrontendTextCursor cursor;cursor.origin_x=cursor.x=std::int16_t(left+dx);cursor.y=std::int16_t(150+dx);
                for(const char ch:g_pc_overlay_text){
                    if(ch==10){cursor.x=cursor.origin_x;cursor.y=std::int16_t(cursor.y+int(float(font.height)*style.scale_y)+3);continue;}
                    FrontendGlyph g;
                    if(frontend_text_glyph_42c720(font,style,cursor,std::uint8_t(ch),g))glyphs.push_back(g);
                    frontend_text_advance_42c5a0(font,style,cursor,std::uint8_t(ch));
                }
            }
            native_sprite2d_glyph_records(c,fc.device,glyphs);
        }
    }
    const auto before=o.state.draws;
    o.state.sprite_95b218=fc.device.sprite()!=nullptr;          // D3DXCreateSprite succeeded on this device
    pc_2d_flush_42d710(o.state,fc,fc.device.sprite(),0,arg,r.frame().layer_7d25f0);
    o.last_flush_draws=o.state.draws-before;++o.flushes;
    if(o.last_flush_draws)++o.frames_drawn;
    return true;
}
std::string native_sprite2d_status(const NativeRuntimeContext& c){
    std::ostringstream s;
    if(!c.sprite2d){s<<"2D renderer: not used";return s.str();}
    const auto& o=*c.sprite2d;const auto& st=o.state;
    s<<"2D renderer event398 inits="<<o.inits_398<<" (last in mode "<<o.last_init_mode<<")"<<" controls="<<o.controls_398<<" displays="<<o.displays<<" ui_displays="<<o.ui_displays<<" glyphs="<<o.ui_glyphs<<" images="<<o.ui_images<<" icons="<<o.ui_icons<<" flushes="<<o.flushes<<" frames_drawn="<<o.frames_drawn<<" last_draws="<<o.last_flush_draws
     <<" pool_draws="<<st.pool_draws<<" missing_roots="<<st.pool_missing_roots<<" sprite_nodes="<<st.sprite_nodes<<" strips="<<st.sprite_draws;
    s<<std::hex<<" missing:";for(const auto& [pc,n]:st.missing_counts)s<<' '<<pc<<'x'<<std::dec<<n<<std::hex;
    s<<std::dec<<" banks:";for(const auto& [b,t]:o.bank_status)s<<" ["<<std::hex<<b<<std::dec<<": "<<t<<"]";
    return s.str();
}
}
