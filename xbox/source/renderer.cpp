#include "d3d11_display.hpp"
#include "d3d11_d3d9.hpp"
#include <memory>
#include <stdexcept>
#include "renderer.hpp"
#include "frame_profile.hpp"
#include "platform/frontend_preview_pack.hpp"
#include "platform/game_ui_pack.hpp"
#include "platform/mesh_preview_pack.hpp"
#include "platform/frontend_glyph_render.hpp"
#include "platform/pc_soft_d3d9.hpp"
#include <cstdlib>
#include "platform/pc_dds.hpp"
#include "platform/frontend_images.hpp"
#include "platform/frontend_sprites.hpp"
#include <map>


#include <algorithm>
#include <cstdio>
#include <chrono>
#include <cmath>
#include <cstring>
#include <limits>
#include <new>
#include <utility>
#include <vector>



namespace outrun::xbox_runtime {

namespace {
constexpr unsigned SharedTextureBase=0x10000,FontTextureBase=0x20000,RawImageTextureBase=0x30000,MaxFontDraws=0x230;
struct MenuGraphics {
    bool game_backdrop{};
    platform::GameUiPack shared;
    platform::FrontendFontPack fonts;
    bool fonts_loaded{};
    unsigned font_first_draw{},font_texture_slot{};
    std::vector<platform::FrontendGlyph> glyphs;
    platform::FrontendImageBank image_bank;
    std::vector<platform::FrontendListImage> images;
    std::vector<platform::FrontendSprite> icons;
    std::vector<unsigned> icon_layers;
    unsigned image_texture_slot{};
    bool images_loaded{};
    unsigned descriptors()const{return (fonts_loaded?10u:0u)+unsigned(image_bank.textures.size());}
};
bool append_menu_graphics(platform::FrontendPreviewPack& frontend,MenuGraphics& menu,
                          const char* shared_path,const char* font_path,const char* image_path,std::string& error){
    if(shared_path){
        if(!platform::load_shared_ui_pack_file(shared_path,menu.shared,&error))return false;
        for(unsigned i=0;i<menu.shared.textures.size();++i){const auto& t=menu.shared.textures[i];
            frontend.textures.push_back({SharedTextureBase+i,t.width,t.height,t.format,t.bytes});}
        for(unsigned i=0;i<menu.shared.scene_count;++i){platform::GameUiScene s;std::vector<platform::GameUiDraw> draws;
            if(!platform::game_ui_scene(menu.shared,i,s)||!platform::game_ui_scene_frame_draws(menu.shared,i,0,draws,&error))return false;
            frontend.scenes.push_back({0x2c0000+i,s.width,s.height,unsigned(frontend.draws.size()),unsigned(draws.size())});
            for(const auto& d:draws)frontend.draws.push_back({SharedTextureBase+d.texture,d.crop,d.corners,0,0});
        }
    }
    if(font_path){
        if(!platform::load_frontend_font_pack(font_path,menu.fonts,&error))return false;
        menu.fonts_loaded=true;menu.font_texture_slot=unsigned(frontend.textures.size());
        for(unsigned i=0;i<menu.fonts.textures.size();++i){const auto& t=menu.fonts.textures[i];
            frontend.textures.push_back({FontTextureBase+i,t.width,t.height,t.format,t.bytes});}
        menu.font_first_draw=unsigned(frontend.draws.size());
        // Reserved geometry only. These are not artificial menu scenes.
        for(unsigned i=0;i<MaxFontDraws;++i)frontend.draws.push_back({FontTextureBase,{{0,0,1,1}},{},0,0});
    }
    if(image_path){
        if(!menu.fonts_loaded){error="raw menu images require the font submission page";return false;}
        if(!platform::load_frontend_image_bank(image_path,menu.image_bank,&error)||menu.descriptors()>256)return false;
        menu.images_loaded=true;menu.image_texture_slot=unsigned(frontend.textures.size());
        for(unsigned i=0;i<menu.image_bank.textures.size();++i){const auto& t=menu.image_bank.textures[i];
            frontend.textures.push_back({RawImageTextureBase+i,t.width,t.height,t.format,t.bytes});}
    }
    return true;
}
bool make_course_camera_transform(const std::array<float,3>& eye,
                                  const std::array<float,3>& target,
                                  float vertical_fov_degrees,
                                  platform::MeshPreviewTransform& transform){
    constexpr float Aspect=16.0f/9.0f,Near=0.25f,Far=3000.0f,Pi=3.14159265358979323846f;
    for(float value:eye)if(!std::isfinite(value))return false;
    for(float value:target)if(!std::isfinite(value))return false;
    if(!std::isfinite(vertical_fov_degrees)||vertical_fov_degrees<10.0f||vertical_fov_degrees>150.0f)return false;
    const auto normalize=[](std::array<float,3> value,std::array<float,3>& out){
        const float length=std::sqrt(value[0]*value[0]+value[1]*value[1]+value[2]*value[2]);
        if(!std::isfinite(length)||length<=1.0e-6f)return false;
        for(unsigned i=0;i<3u;++i)out[i]=value[i]/length;
        return true;
    };
    const auto cross=[](const std::array<float,3>& a,const std::array<float,3>& b){return std::array<float,3>{{a[1]*b[2]-a[2]*b[1],a[2]*b[0]-a[0]*b[2],a[0]*b[1]-a[1]*b[0]}};};
    const auto dot=[](const std::array<float,3>& a,const std::array<float,3>& b){return a[0]*b[0]+a[1]*b[1]+a[2]*b[2];};
    std::array<float,3> forward{},right{},up{};
    if(!normalize({{target[0]-eye[0],target[1]-eye[1],target[2]-eye[2]}},forward)||
       !normalize(cross(forward,{{0.0f,1.0f,0.0f}}),right)||
       !normalize(cross(right,forward),up))return false;
    const float focal=1.0f/std::tan(vertical_fov_degrees*Pi/360.0f);
    const float depth_scale=Far/(Far-Near),depth_offset=-Far*Near/(Far-Near);
    const float rows[4][4]={
        {focal/Aspect*right[0],focal/Aspect*right[1],focal/Aspect*right[2],-focal/Aspect*dot(right,eye)},
        {focal*up[0],focal*up[1],focal*up[2],-focal*dot(up,eye)},
        {depth_scale*forward[0],depth_scale*forward[1],depth_scale*forward[2],-depth_scale*dot(forward,eye)+depth_offset},
        {forward[0],forward[1],forward[2],-dot(forward,eye)},
    };
    const float normal_rows[4][4]={
        {right[0],right[1],right[2],0.0f},
        {up[0],up[1],up[2],0.0f},
        {forward[0],forward[1],forward[2],0.0f},
        {0.0f,0.0f,0.0f,1.0f},
    };
    for(unsigned column=0;column<4u;++column)for(unsigned row=0;row<4u;++row){
        transform.position[column*4u+row]=rows[row][column];
        transform.normal[column*4u+row]=normal_rows[row][column];
    }
    return true;
}

struct DescriptorCounts {std::uint32_t resident{},frontend{};};
DescriptorCounts descriptor_counts(std::size_t frontend){
    DescriptorCounts out{};
    if(1u+frontend<=std::numeric_limits<std::uint32_t>::max())out.resident=out.frontend=static_cast<std::uint32_t>(1u+frontend);
    return out;
}
bool append_start_loading(platform::FrontendPreviewPack& frontend,
                          const std::vector<std::uint8_t>* bytes,std::uint32_t& scene_index,
                          platform::GameUiPack& animation,
                          XboxRendererStats& stats,std::string& error){
    if(!bytes)return true;
    platform::FrontendPreviewPack loading{};
    if(!platform::parse_frontend_preview_pack(bytes->data(),bytes->size(),loading,&error))return false;
    if(loading.source_texture_count!=5u||loading.source_scene_count!=9u||
       loading.scenes.size()!=9u||loading.textures.size()!=5u||
       loading.draws.size()!=171u||loading.scenes[0].token!=0x002f0000u){
        error="START loading scene pack does not match original 2F/2 archive";
        return false;
    }
    if(!platform::make_frontend_animation_view(loading,animation,&error))return false;
    scene_index=static_cast<std::uint32_t>(frontend.scenes.size());
    stats.loading_scenes=static_cast<std::uint32_t>(loading.scenes.size());
    stats.loading_textures=static_cast<std::uint32_t>(loading.textures.size());
    stats.loading_draws=static_cast<std::uint32_t>(loading.draws.size());
    const auto first_draw=static_cast<std::uint32_t>(frontend.draws.size());
    for(auto& texture:loading.textures){
        texture.source_index+=platform::FrontendPreviewSourceTextureCount;
        frontend.textures.push_back(std::move(texture));
    }
    for(auto& draw:loading.draws){
        draw.source_texture+=platform::FrontendPreviewSourceTextureCount;
        frontend.draws.push_back(std::move(draw));
    }
    for(auto& scene:loading.scenes){
        scene.first_draw+=first_draw;
        frontend.scenes.push_back(scene);
    }
    return true;
}
// ---- frontend quads, shared by the deko3d renderer and the host_nro
// compositor (same vertices on both) ----
constexpr std::uint32_t FramebufferWidth=1280u;
constexpr std::uint32_t FramebufferHeight=720u;
constexpr std::uint32_t FrontendTransformIndex=8u;
constexpr std::uint32_t FrontendVerticesPerDraw=4u;
// The SPRANI instance pool is drawn by the ported PC 2D renderer (428170 ->
// 42D710 on the PC device): this renderer then keeps only its glyph, image
// and icon layers.
bool g_pc_sprites{};
// The frontend text glyphs and raw images are queued in the PC 2D renderer too.
bool g_pc_text{};
const platform::FrontendPreviewTexture* find_frontend_texture(
    const platform::FrontendPreviewPack& pack,std::uint32_t source){
    for(const auto& texture:pack.textures)
        if(texture.source_index==source)return &texture;
    return nullptr;
}

std::size_t frontend_texture_slot(const platform::FrontendPreviewPack& pack,
                                  std::uint32_t source){
    for(std::size_t i=0;i<pack.textures.size();++i)
        if(pack.textures[i].source_index==source)return i;
    return pack.textures.size();
}

std::array<platform::MeshPreviewVertex,FrontendVerticesPerDraw>
make_frontend_quad(const platform::FrontendPreviewPack& pack,
                   const platform::FrontendPreviewDraw& draw){
    const auto* texture=find_frontend_texture(pack,draw.source_texture);
    constexpr float scene_width=float(platform::FrontendPreviewSceneWidth);
    constexpr float scene_height=float(platform::FrontendPreviewSceneHeight);
    const float scene_aspect=scene_width/scene_height;
    constexpr float screen_aspect=float(FramebufferWidth)/float(FramebufferHeight);
    float half_x=0.90f,half_y=0.90f;
    if(scene_aspect<screen_aspect)half_x=half_y*scene_aspect/screen_aspect;
    else half_y=half_x*screen_aspect/scene_aspect;
    constexpr std::array<float,3> normal{{0.401422f,0.752666f,0.521849f}};
    std::array<platform::MeshPreviewVertex,FrontendVerticesPerDraw> vertices{};
    for(std::size_t i=0;i<vertices.size();++i){
        const float x=-half_x+(draw.corners[i][0]/scene_width)*(2.0f*half_x);
        const float y= half_y-(draw.corners[i][1]/scene_height)*(2.0f*half_y);
        const bool right=i==1u||i==2u;const bool bottom=i>=2u;
        const float crop_x=float(draw.crop[right?2u:0u]);
        const float crop_y=float(draw.crop[bottom?3u:1u]);
        // PC 428BD0 -> 42A3A0: u = crop x / width, v = 1 - crop y / height
        // for every SPRANI bank (SUMO_FE, START, shared); the XST DDS pages
        // are stored bottom-up and never mirrored horizontally.
        vertices[i].position={{x,y,0.0f}};vertices[i].normal=normal;
        vertices[i].uv={{crop_x/float(texture->width),1.0f-crop_y/float(texture->height)}};
        vertices[i].color={{1.0f,1.0f,1.0f,1.0f}};
        vertices[i].transform_index=float(FrontendTransformIndex);
    }
    return vertices;
}

std::array<platform::MeshPreviewVertex,FrontendVerticesPerDraw>
make_animated_frontend_quad(const platform::FrontendPreviewPack& pack,
                            const platform::GameUiDraw& animation,
                            unsigned texture_base){
    platform::FrontendPreviewDraw pose{};
    pose.source_texture=animation.texture+texture_base;
    pose.crop=animation.crop;
    pose.corners=animation.corners;
    auto vertices=make_frontend_quad(pack,pose);
    for(auto& vertex:vertices)
        vertex.color[3]=animation.visible?animation.opacity:0.0f;
    return vertices;
}

std::array<platform::MeshPreviewVertex,FrontendVerticesPerDraw>
make_game_ui_quad(const platform::GameUiPack& pack,
                  const platform::GameUiDraw& draw){
    const auto& texture=pack.textures[draw.texture];
    constexpr float scene_width=640.0f,scene_height=480.0f;
    constexpr float screen_aspect=float(FramebufferWidth)/float(FramebufferHeight);
    constexpr float half_y=0.90f;
    constexpr float half_x=half_y*(scene_width/scene_height)/screen_aspect;
    constexpr std::array<float,3> normal{{0.401422f,0.752666f,0.521849f}};
    std::array<platform::MeshPreviewVertex,FrontendVerticesPerDraw> vertices{};
    for(std::size_t i=0;i<vertices.size();++i){
        const bool right=i==1u||i==2u,bottom=i>=2u;
        const float u=float(draw.crop[right?2u:0u])/float(texture.width);
        const float v=float(draw.crop[bottom?3u:1u])/float(texture.height);
        auto& vertex=vertices[i];
        vertex.position={{-half_x+draw.corners[i][0]*(2.0f*half_x/scene_width),
                          half_y-draw.corners[i][1]*(2.0f*half_y/scene_height),0.0f}};
        vertex.normal=normal;
        vertex.uv={{u,1.0f-v}};                 // PC 428BD0 (see make_frontend_quad)
        vertex.color={{1.0f,1.0f,1.0f,draw.visible?draw.opacity:0.0f}};
        vertex.transform_index=float(FrontendTransformIndex);
    }
    return vertices;
}

}


namespace {
struct RendererImpl {
    MenuGraphics menu;
    using Display=D3D11Display;
    using Device=D3D11D3D9Device;
    std::unique_ptr<Display> display;
    std::unique_ptr<Device> device;
    std::function<void(platform::PcD3D9Device&)> scene;
    std::string last_error;
    unsigned raster_frames{};
    std::uint64_t raster_us{},raster_peak_us{};
    float loading_frame{};
    std::vector<std::uint8_t> movie_rgba;
    unsigned movie_width{},movie_height{};
    platform::FrontendSprites* frontend_sprites{};
    platform::FrontendPreviewPack frontend;
    platform::GameUiPack frontend_animation;
    platform::GameUiPack loading_animation;
    XboxRendererStats stats{};
    std::size_t frontend_scene_index{};
    std::uint32_t frontend_overlay_token{};
    float frontend_overlay_frame{};
    float frontend_frame{};
    std::vector<platform::GameUiDraw> frontend_frame_draws;
    std::uint32_t loading_scene_index{};
    std::uint32_t loading_scene_offset{};
    bool frontend_visible{true};
    bool video_pending{},video_fxaa{};       // Options > Settings enhancement rows (xbox_renderer_set_video)
    unsigned video_width{},video_height{},video_msaa{1};
    bool movie_visible{};
    bool start_loading_visible{};
    std::uint32_t primary_frontend_scenes{};
    std::uint32_t primary_frontend_textures{};
};
void initialize_frontend_stats(RendererImpl& r){
    r.stats.frontend_textures=r.primary_frontend_textures;
    r.stats.frontend_scenes=r.primary_frontend_scenes;
    if(r.frontend_scene_index>=r.frontend.scenes.size())return;
    const auto& scene=r.frontend.scenes[r.frontend_scene_index];
    r.stats.frontend_draws=scene.draw_count;r.stats.frontend_token=scene.token;
    r.stats.frontend_scene_width=scene.width;r.stats.frontend_scene_height=scene.height;
}
}
bool xbox_renderer_initialize(XboxRenderer& renderer,const char* frontend_path,std::string& error,
                         const std::vector<std::uint8_t>* start_loading,const char* shared_ui_path,
                         const char* font_path,const char* image_path){
    auto* impl=new(std::nothrow) RendererImpl{};if(!impl){error="renderer allocation failed";return false;}
    if(frontend_path&&!platform::load_frontend_preview_pack_file(frontend_path,impl->frontend,&error)){delete impl;return false;}
    impl->primary_frontend_scenes=static_cast<std::uint32_t>(impl->frontend.scenes.size());
    impl->primary_frontend_textures=static_cast<std::uint32_t>(impl->frontend.textures.size());
    if(!impl->frontend.animation.empty()&&
       !platform::make_frontend_animation_view(impl->frontend,
            impl->frontend_animation,&error)){delete impl;return false;}
    if(!append_start_loading(impl->frontend,start_loading,impl->loading_scene_index,
                             impl->loading_animation,impl->stats,error)){
        delete impl;return false;
    }
    if(!append_menu_graphics(impl->frontend,impl->menu,shared_ui_path,font_path,image_path,error)){delete impl;return false;}
    impl->stats.shared_ui_scenes=impl->menu.shared.scene_count;
    impl->stats.font_textures=impl->menu.fonts_loaded?10:0;
    for(std::size_t i=0u;i<impl->frontend.scenes.size();++i)
        if(impl->frontend.scenes[i].token==platform::FrontendPreviewInitialToken){
            impl->frontend_scene_index=i;break;
        }
    const auto descriptors=descriptor_counts(impl->frontend.textures.size());
    impl->stats.resident_images=descriptors.resident;impl->stats.frontend_descriptors=descriptors.frontend;
    initialize_frontend_stats(*impl);
    impl->display=std::make_unique<RendererImpl::Display>();
    if(!impl->display->open(FramebufferWidth,FramebufferHeight,error)){delete impl;return false;}
    try{impl->device=std::make_unique<RendererImpl::Device>(impl->display->device(),impl->display->context(),640u,480u);}
    catch(const std::exception& e){error=e.what();delete impl;return false;}
    renderer.impl=impl;return true;
}
void xbox_renderer_set_video(XboxRenderer& renderer,unsigned width,unsigned height,bool widescreen,unsigned msaa,bool fxaa){
    auto* r=static_cast<RendererImpl*>(renderer.impl);if(!r)return;
    g_widescreen=widescreen;r->video_width=width;r->video_height=height;r->video_msaa=msaa;r->video_fxaa=fxaa;r->video_pending=true;
}
void xbox_renderer_scene_to_frame(XboxRenderer& renderer){
    auto* r=static_cast<RendererImpl*>(renderer.impl);if(r&&r->device)r->device->finish_scene();
}
bool xbox_renderer_set_frontend_visible(XboxRenderer& renderer,bool visible){auto* impl=static_cast<RendererImpl*>(renderer.impl);if(!impl)return false;impl->frontend_visible=visible;return true;}
bool xbox_renderer_set_movie_frame(XboxRenderer& renderer,const std::uint8_t* rgba,unsigned width,unsigned height){auto* r=static_cast<RendererImpl*>(renderer.impl);if(!r)return false;if(rgba&&(width==0u||height==0u||width>1920u||height>1080u))return false;r->movie_visible=rgba!=nullptr;
    if(rgba){r->movie_rgba.assign(rgba,rgba+std::size_t(width)*height*4u);r->movie_width=width;r->movie_height=height;}
    else{r->movie_rgba.clear();r->movie_width=r->movie_height=0;}
    return true;}
bool xbox_renderer_set_start_loading_visible(XboxRenderer& renderer,bool visible){auto* impl=static_cast<RendererImpl*>(renderer.impl);if(!impl||(visible&&impl->stats.loading_scenes==0u))return false;impl->start_loading_visible=visible;return true;}
bool xbox_renderer_set_start_loading_scene(XboxRenderer& renderer,std::uint32_t scene){auto* impl=static_cast<RendererImpl*>(renderer.impl);if(!impl||scene>=impl->stats.loading_scenes)return false;if(impl->loading_scene_offset!=scene)impl->loading_frame=0;impl->loading_scene_offset=scene;return true;}
bool xbox_renderer_set_frontend_token(XboxRenderer& renderer,std::uint32_t token){auto* impl=static_cast<RendererImpl*>(renderer.impl);if(!impl)return false;if(!impl->primary_frontend_scenes)return true;for(std::size_t i=0;i<impl->primary_frontend_scenes;++i)if(impl->frontend.scenes[i].token==token){if(impl->frontend_scene_index!=i){++impl->stats.frontend_scene_changes;impl->frontend_frame=0.0f;impl->stats.frontend_animation_complete=0u;}impl->frontend_scene_index=i;initialize_frontend_stats(*impl);return true;}return false;}
bool xbox_renderer_set_frontend_overlay(XboxRenderer& renderer,std::uint32_t token,float frame){
    auto* r=static_cast<RendererImpl*>(renderer.impl);
    if(!r||!std::isfinite(frame)||frame<0.0f)return false;
    if(!r->primary_frontend_scenes)return true;
    if(token!=0u&&!platform::find_frontend_preview_scene(r->frontend,token))return false;
    r->frontend_overlay_token=token;r->frontend_overlay_frame=frame;return true;
}

namespace {
template<class Canvas> void paint_frontend(RendererImpl& r,Canvas& c,unsigned frame){
    auto draw_scene=[&](const platform::FrontendSprite& instance){
        if(std::getenv("OR2_HOST_FE_DUMP"))std::fprintf(stderr,"fe%u inst token=%08x frame=%.1f canvas=%.0fx%.0f m=[%.3f %.3f %.3f %.3f | %.1f %.1f]\n",frame,instance.token,instance.frame,instance.canvas_width,instance.canvas_height,instance.matrix[0],instance.matrix[1],instance.matrix[4],instance.matrix[5],instance.matrix[12],instance.matrix[13]);
        const auto* scene=platform::find_frontend_preview_scene(r.frontend,instance.token);if(!scene)return;
        const bool shared=(instance.token>>16u)==0x2cu;
        const auto& animation=shared?r.menu.shared:r.frontend_animation;
        const auto base=shared?SharedTextureBase:0u;
        std::vector<platform::GameUiDraw> draws;
        if(!platform::game_ui_scene_frame_draws(animation,instance.token&0xffffu,instance.frame,draws))return;
        for(auto& d:draws){
            platform::frontend_sprite_transform(d,instance,instance.canvas_width,instance.canvas_height);
            const auto slot=frontend_texture_slot(r.frontend,d.texture+base);if(slot>=r.frontend.textures.size())continue;
            const auto& t=r.frontend.textures[slot];
            c.quad(make_animated_frontend_quad(r.frontend,d,base),c.texels(&t,t.format,t.width,t.height,t.bytes),t.width,t.height);
        }
    };
    const auto& instances=r.frontend_sprites->instances();
    for(unsigned layer=0;layer<platform::FrontendSprites::Layers;++layer){
        if(r.menu.fonts_loaded&&!g_pc_text){
            for(const auto& image:r.menu.images){
                if(unsigned(std::clamp(int(image.layer),0,20))!=layer)continue;
                platform::FrontendGlyphQuad q;unsigned texture{};
                if(!platform::frontend_image_quad(image,r.menu.image_bank,q,texture)||texture>=r.menu.image_bank.textures.size())continue;
                const auto& t=r.menu.image_bank.textures[texture];c.quad(q,c.texels(&t,t.format,t.width,t.height,t.bytes),t.width,t.height);
            }
            for(const auto& g:r.menu.glyphs){
                if(platform::frontend_glyph_layer(g)!=layer||g.token>=10)continue;
                const auto& t=r.menu.fonts.textures[g.token];platform::FrontendGlyphQuad q;
                if(platform::frontend_glyph_quad(g,t,q))c.quad(q,c.texels(&t,t.format,t.width,t.height,t.bytes),t.width,t.height);
            }
        }
        for(unsigned i=0;i<r.menu.icons.size()&&!g_pc_text;++i)if(r.menu.icon_layers[i]==layer)draw_scene(r.menu.icons[i]);
        for(std::uint32_t h=layer*64u;h<(layer+1u)*64u&&!g_pc_sprites;++h){const auto& in=instances[h];if(in.allocated&&in.visible)draw_scene(in);}
    }
}
}

#include "renderer_present.inc"
bool xbox_renderer_draw(XboxRenderer& renderer){auto* impl=static_cast<RendererImpl*>(renderer.impl);if(!impl)return false;++impl->stats.frames;

if(impl->movie_visible)++impl->stats.movie_frames;if(impl->start_loading_visible)++impl->stats.loading_frames;else if(impl->frontend_visible&&impl->frontend_sprites){impl->stats.font_draws=static_cast<unsigned>(impl->menu.glyphs.size());if(impl->stats.font_draws)++impl->stats.font_frames;impl->stats.frontend_instances=0;impl->stats.frontend_instance_draws=0;const auto& instances=impl->frontend_sprites->instances();for(std::uint32_t h=0;h<instances.size()&&!g_pc_sprites;++h){const auto& instance=instances[h];if(!instance.allocated||!instance.visible)continue;const auto* scene=platform::find_frontend_preview_scene(impl->frontend,instance.token);const auto bank=instance.token>>16u;if(!scene||(bank!=0x44u&&bank!=0x2cu)){std::fprintf(stderr,"frontend unsupported instance token=%08x\n",instance.token);return false;}std::vector<platform::GameUiDraw> draws;std::string frame_error;if(!platform::game_ui_scene_frame_draws(bank==0x2cu?impl->menu.shared:impl->frontend_animation,instance.token&0xffffu,instance.frame,draws,&frame_error)){std::fprintf(stderr,"frontend token=%08x frame=%f: %s\n",instance.token,instance.frame,frame_error.c_str());return false;}for(auto& draw:draws)platform::frontend_sprite_transform(draw,instance,instance.canvas_width,instance.canvas_height);++impl->stats.frontend_instances;impl->stats.frontend_instance_draws+=static_cast<std::uint32_t>(draws.size());if(instance.token==impl->frontend.scenes[impl->frontend_scene_index].token){++impl->stats.frontend_animation_updates;impl->stats.frontend_animation_frame=instance.frame;impl->stats.frontend_animation_last_frame=instance.last;impl->stats.frontend_animation_complete=instance.status!=1u;}impl->frontend_sprites->drawn(h);}++impl->stats.frontend_instance_frames;++impl->stats.frontend_frames;}else if(impl->frontend_visible){if(!impl->frontend_animation.animation.empty()){const auto& scene=impl->frontend.scenes[impl->frontend_scene_index];const auto source_scene=scene.token&0xffffu;if(!platform::game_ui_scene_frame_draws(impl->frontend_animation,source_scene,0.0f,impl->frontend_frame_draws)||impl->frontend_frame_draws.size()!=scene.draw_count)return false;float last=0.0f;bool moving=false;for(const auto& draw:impl->frontend_frame_draws){last=std::max(last,float(std::max<int>(0,draw.last_frame)));moving|=draw.has_keyframes||draw.last_frame>1;}impl->stats.frontend_animation_last_frame=last;if(moving){const float frame=std::min(impl->frontend_frame,last);if(!platform::game_ui_scene_frame_draws(impl->frontend_animation,source_scene,frame,impl->frontend_frame_draws))return false;++impl->stats.frontend_animation_updates;impl->stats.frontend_animation_frame=frame;impl->stats.frontend_animation_complete=frame>=last?1u:0u;if(impl->frontend_frame<last)impl->frontend_frame+=1.0f;}else{impl->stats.frontend_animation_frame=0.0f;impl->stats.frontend_animation_complete=1u;}}else impl->stats.frontend_animation_complete=1u;if(impl->frontend_overlay_token!=0u){std::vector<platform::GameUiDraw> overlay;if(!platform::game_ui_scene_frame_draws(impl->frontend_animation,impl->frontend_overlay_token&0xffffu,impl->frontend_overlay_frame,overlay))return false;for(const auto& draw:overlay)if(draw.texture>=impl->frontend_animation.textures.size())return false;++impl->stats.frontend_overlay_frames;impl->stats.frontend_overlay_token=impl->frontend_overlay_token;impl->stats.frontend_overlay_draws=static_cast<std::uint32_t>(overlay.size());}++impl->stats.frontend_frames;}else{++impl->stats.unowned_frames;}return present_frame(*impl);}
XboxRendererStats xbox_renderer_stats(const XboxRenderer& renderer){const auto* impl=static_cast<const RendererImpl*>(renderer.impl);return impl?impl->stats:XboxRendererStats{};}
void xbox_renderer_shutdown(XboxRenderer& renderer){
    auto* r=static_cast<RendererImpl*>(renderer.impl);
    if(r&&r->device)std::fprintf(stdout,"Xbox raster: presented=%u total_us=%llu peak_us=%llu pixels=%llu live_objects=%zu errors=%zu\n",
        r->raster_frames,(unsigned long long)r->raster_us,(unsigned long long)r->raster_peak_us,
        (unsigned long long)r->device->pixels_shaded,r->device->live_objects(),r->device->errors.size());
    delete r;renderer.impl=nullptr;
}void xbox_renderer_set_pc_sprites(XboxRenderer&,bool on){g_pc_sprites=on;}
void xbox_renderer_set_pc_text(XboxRenderer&,bool on){g_pc_text=on;}
bool xbox_renderer_attach_frontend_sprites(XboxRenderer& renderer,platform::FrontendSprites& sprites){
    auto* r=static_cast<RendererImpl*>(renderer.impl);
    if(!r||(r->frontend_animation.scene_count&&!sprites.bind(0x44u,r->frontend_animation)))return false;
    if(r->menu.shared.scene_count&&!sprites.bind(0x2cu,r->menu.shared))return false;
    r->frontend_sprites=&sprites;return true;
}

const platform::FrontendFontPack* xbox_renderer_frontend_fonts(const XboxRenderer& renderer){
    const auto* r=static_cast<const RendererImpl*>(renderer.impl);
    return r&&r->menu.fonts_loaded?&r->menu.fonts:nullptr;
}
bool xbox_renderer_set_frontend_icons(XboxRenderer& renderer,const std::vector<platform::FrontendWindowIcon>& icons){
    auto* r=static_cast<RendererImpl*>(renderer.impl);if(!r||icons.size()>MaxFontDraws)return false;
    std::vector<platform::FrontendSprite> poses;std::vector<unsigned> layers;unsigned count=0;
    for(const auto& icon:icons){
        if(icon.layer<0||icon.layer>=21||icon.frame<0||!std::isfinite(icon.x)||!std::isfinite(icon.y)||(icon.token>>16)!=0x2c)return false;
        platform::GameUiScene scene;std::vector<platform::GameUiDraw> draws;
        if(!platform::game_ui_scene(r->menu.shared,icon.token&0xffff,scene)||
           !platform::game_ui_scene_frame_draws(r->menu.shared,icon.token&0xffff,float(icon.frame),draws))return false;
        platform::FrontendSprite pose;pose.token=icon.token;pose.frame=float(icon.frame);
        pose.canvas_width=scene.width;pose.canvas_height=scene.height;
        pose.matrix={1,0,0,0,0,1,0,0,0,0,1,0,icon.x,icon.y,0,1};
        for(auto& draw:draws)platform::frontend_sprite_transform(draw,pose,scene.width,scene.height);
        count+=unsigned(draws.size());poses.push_back(pose);layers.push_back(unsigned(icon.layer));
    }
    r->menu.icons=std::move(poses);r->menu.icon_layers=std::move(layers);
    r->stats.menu_icons=unsigned(icons.size());r->stats.menu_icon_draws=count;return true;
}
bool xbox_renderer_set_frontend_game_backdrop(XboxRenderer& renderer,bool visible){
    auto* r=static_cast<RendererImpl*>(renderer.impl);if(!r)return false;r->menu.game_backdrop=visible;return true;
}
bool xbox_renderer_set_frontend_glyphs(XboxRenderer& renderer,const std::vector<platform::FrontendGlyph>& glyphs){
    auto* r=static_cast<RendererImpl*>(renderer.impl);
    if(!r||glyphs.size()+r->menu.images.size()>MaxFontDraws||(!glyphs.empty()&&!r->menu.fonts_loaded))return false;
    platform::FrontendGlyphQuad quad;
    for(const auto& g:glyphs)if(g.token>=10||!platform::frontend_glyph_quad(g,r->menu.fonts.textures[g.token],quad))return false;
    r->menu.glyphs=glyphs;return true;
}
bool xbox_renderer_set_frontend_images(XboxRenderer& renderer,const std::vector<platform::FrontendListImage>& images){
    auto* r=static_cast<RendererImpl*>(renderer.impl);
    if(!r||images.size()+r->menu.glyphs.size()>MaxFontDraws||(!images.empty()&&!r->menu.images_loaded))return false;
    platform::FrontendGlyphQuad quad;unsigned texture{};
    for(const auto& image:images)if(!std::isfinite(image.layer)||double(image.layer)<-2147483648.||double(image.layer)>=2147483648.||
        !platform::frontend_image_quad(image,r->menu.image_bank,quad,texture))return false;
    r->menu.images=images;r->stats.menu_image_draws=unsigned(images.size());return true;
}

} // namespace outrun::xbox_runtime
