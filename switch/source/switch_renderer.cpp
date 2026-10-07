#include "switch_renderer.hpp"
#include "frame_profile.hpp"
#include "platform/frontend_preview_pack.hpp"
#include "platform/game_ui_pack.hpp"
#include "platform/mesh_preview_pack.hpp"
#include "platform/frontend_glyph_render.hpp"
#if defined(OR2_HOST_NRO)
#include "platform/pc_soft_d3d9.hpp"
#include <cstdlib>
#include "platform/pc_dds.hpp"
#include "platform/frontend_images.hpp"
#include "platform/frontend_sprites.hpp"
#include <map>
#endif

#include <algorithm>
#include <cstdio>
#include <chrono>
#include <cmath>
#include <cstring>
#include <limits>
#include <new>
#include <utility>
#include <vector>

#if !defined(OUTRUN_SWITCH_STUB)
#include <switch.h>
#include <deko3d.h>
#include "switch_d3d9.hpp"
#include <memory>
#include <cstddef>
#endif

namespace outrun::switch_runtime {

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
                          SwitchRendererStats& stats,std::string& error){
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

#if defined(OUTRUN_SWITCH_STUB)
namespace {
struct RendererImpl {
    MenuGraphics menu;
    platform::FrontendSprites* frontend_sprites{};
    platform::FrontendPreviewPack frontend;
    platform::GameUiPack frontend_animation;
    platform::GameUiPack loading_animation;
    SwitchRendererStats stats{};
    std::size_t frontend_scene_index{};
    std::uint32_t frontend_overlay_token{};
    float frontend_overlay_frame{};
    float frontend_frame{};
    std::vector<platform::GameUiDraw> frontend_frame_draws;
    std::uint32_t loading_scene_index{};
    std::uint32_t loading_scene_offset{};
    bool frontend_visible{true};
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
bool switch_renderer_initialize(SwitchRenderer& renderer,const char* frontend_path,std::string& error,
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
    initialize_frontend_stats(*impl);renderer.impl=impl;return true;
}
bool switch_renderer_set_frontend_visible(SwitchRenderer& renderer,bool visible){auto* impl=static_cast<RendererImpl*>(renderer.impl);if(!impl)return false;impl->frontend_visible=visible;return true;}
bool switch_renderer_set_movie_frame(SwitchRenderer& renderer,const std::uint8_t* rgba,unsigned width,unsigned height){auto* r=static_cast<RendererImpl*>(renderer.impl);if(!r)return false;if(rgba&&(width==0u||height==0u||width>1920u||height>1080u))return false;r->movie_visible=rgba!=nullptr;return true;}
bool switch_renderer_set_start_loading_visible(SwitchRenderer& renderer,bool visible){auto* impl=static_cast<RendererImpl*>(renderer.impl);if(!impl||(visible&&impl->stats.loading_scenes==0u))return false;impl->start_loading_visible=visible;return true;}
bool switch_renderer_set_start_loading_scene(SwitchRenderer& renderer,std::uint32_t scene){auto* impl=static_cast<RendererImpl*>(renderer.impl);if(!impl||scene>=impl->stats.loading_scenes)return false;impl->loading_scene_offset=scene;return true;}
bool switch_renderer_set_frontend_token(SwitchRenderer& renderer,std::uint32_t token){auto* impl=static_cast<RendererImpl*>(renderer.impl);if(!impl)return false;if(!impl->primary_frontend_scenes)return true;for(std::size_t i=0;i<impl->primary_frontend_scenes;++i)if(impl->frontend.scenes[i].token==token){if(impl->frontend_scene_index!=i){++impl->stats.frontend_scene_changes;impl->frontend_frame=0.0f;impl->stats.frontend_animation_complete=0u;}impl->frontend_scene_index=i;initialize_frontend_stats(*impl);return true;}return false;}
bool switch_renderer_set_frontend_overlay(SwitchRenderer& renderer,std::uint32_t token,float frame){
    auto* r=static_cast<RendererImpl*>(renderer.impl);
    if(!r||!std::isfinite(frame)||frame<0.0f)return false;
    if(!r->primary_frontend_scenes)return true;
    if(token!=0u&&!platform::find_frontend_preview_scene(r->frontend,token))return false;
    r->frontend_overlay_token=token;r->frontend_overlay_frame=frame;return true;
}
#if defined(OR2_HOST_NRO)
// host_nro: the ported PC renderer draws into the software D3D9 device; every
// OR2_HOST_PNG_EVERY-th frame is rendered and written as host_frame_NNNNN.png.
namespace {
struct HostPcScene {
    // OR2_HOST_WIDE: a 854x480 back buffer to check the 16:9 view (Y in the script)
    platform::PcSoftD3D9Device device{std::getenv("OR2_HOST_WIDE")?854u:640u,480u};
    std::function<void(platform::PcD3D9Device&)> scene;
    unsigned frame{};
};
HostPcScene& host_pc_scene(){static HostPcScene s;return s;}
// host_nro frontend compositor (OR2_HOST_FE=1 with OR2_HOST_PNG_EVERY): the
// deko3d frontend passes (glyph/image layers, icons, sprite instances) are
// rasterised from the very same vertices into host_fe_NNNNN.ppm, over the
// PC scene of that frame scaled into the 4:3 area. Diagnostic only.
struct HostCanvas {
    static constexpr int W=int(FramebufferWidth),H=int(FramebufferHeight);
    std::vector<float> rgb=std::vector<float>(std::size_t(W)*H*3,0.0f);
    std::map<const void*,std::vector<std::uint8_t>> decoded;
    const std::vector<std::uint8_t>& texels(const void* key,platform::MeshPreviewTextureFormat format,
                                            std::uint32_t w,std::uint32_t h,const std::vector<std::uint8_t>& bytes){
        auto it=decoded.find(key);if(it!=decoded.end())return it->second;
        platform::PcDdsTexture t{};t.width=w;t.height=h;t.levels=1;
        t.format=format==platform::MeshPreviewTextureFormat::bc1?platform::PcDdsTexture::Format::bc1:
                 format==platform::MeshPreviewTextureFormat::bc2?platform::PcDdsTexture::Format::bc2:
                 format==platform::MeshPreviewTextureFormat::bc3?platform::PcDdsTexture::Format::bc3:platform::PcDdsTexture::Format::rgba8;
        t.faces.resize(1);t.faces[0].push_back({w,h,bytes});
        return decoded[key]=platform::pc_dds_rgba(t,0,0);
    }
    // Two triangles (0,1,2)(2,3,0); deko3d upper-left origin: NDC +1 is the top.
    void quad(const std::array<platform::MeshPreviewVertex,4>& v,const std::vector<std::uint8_t>& tex,std::uint32_t tw,std::uint32_t th){
        if(tex.size()<std::size_t(tw)*th*4u)return;
        auto px=[&](const platform::MeshPreviewVertex& q){return std::array<float,2>{{(q.position[0]+1.0f)*0.5f*W,(1.0f-q.position[1])*0.5f*H}};};
        const int tri[2][3]={{0,1,2},{2,3,0}};
        for(const auto& t:tri){
            const auto a=px(v[t[0]]),b=px(v[t[1]]),c=px(v[t[2]]);
            const float area=(b[0]-a[0])*(c[1]-a[1])-(b[1]-a[1])*(c[0]-a[0]);
            if(std::fabs(area)<1e-6f)continue;
            const int x0=std::max(0,int(std::floor(std::min({a[0],b[0],c[0]})))),x1=std::min(W-1,int(std::ceil(std::max({a[0],b[0],c[0]}))));
            const int y0=std::max(0,int(std::floor(std::min({a[1],b[1],c[1]})))),y1=std::min(H-1,int(std::ceil(std::max({a[1],b[1],c[1]}))));
            for(int y=y0;y<=y1;++y)for(int x=x0;x<=x1;++x){
                const float fx=x+0.5f,fy=y+0.5f;
                const float w0=((b[0]-fx)*(c[1]-fy)-(b[1]-fy)*(c[0]-fx))/area;
                const float w1=((c[0]-fx)*(a[1]-fy)-(c[1]-fy)*(a[0]-fx))/area;
                const float w2=1.0f-w0-w1;
                if(w0<0||w1<0||w2<0)continue;
                const auto& A=v[t[0]];const auto& B=v[t[1]];const auto& C=v[t[2]];
                float u=w0*A.uv[0]+w1*B.uv[0]+w2*C.uv[0],vv=w0*A.uv[1]+w1*B.uv[1]+w2*C.uv[1];
                u=std::clamp(u,0.0f,1.0f);vv=std::clamp(vv,0.0f,1.0f);
                const auto tx=std::min(tw-1u,std::uint32_t(u*float(tw))),ty=std::min(th-1u,std::uint32_t(vv*float(th)));
                const auto* s=tex.data()+(std::size_t(ty)*tw+tx)*4u;
                float col[4];for(int k=0;k<4;++k)col[k]=(w0*A.color[k]+w1*B.color[k]+w2*C.color[k])*float(s[k])/255.0f;
                if(col[3]<0.05f)continue;
                float* d=rgb.data()+(std::size_t(y)*W+x)*3u;
                for(int k=0;k<3;++k)d[k]=col[k]*col[3]+d[k]*(1.0f-col[3]);
            }
        }
    }
    bool write(const char* name)const{
        std::FILE* f=std::fopen(name,"wb");if(!f)return false;
        std::fprintf(f,"P6\n%d %d\n255\n",W,H);
        std::vector<std::uint8_t> row(std::size_t(W)*3);
        for(int y=0;y<H;++y){for(int i=0;i<W*3;++i)row[i]=std::uint8_t(std::clamp(rgb[std::size_t(y)*W*3+i],0.0f,1.0f)*255.0f+0.5f);std::fwrite(row.data(),1,row.size(),f);}
        std::fclose(f);return true;
    }
};
// The deko3d frontend passes (glyph/image layers, icons, sprite instances)
// as textured quads on any canvas (host PNG rasteriser, PC Direct3D 9).
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
void host_frontend_png(RendererImpl& r,unsigned frame){
    HostCanvas c;
    const auto& pc=host_pc_scene().device;
    const int ox=int((FramebufferWidth-FramebufferHeight*4u/3u)/2u),ow=int(FramebufferHeight*4u/3u);
    for(int y=0;y<HostCanvas::H;++y)for(int x=0;x<ow;++x){
        const auto sx=std::uint32_t(x)*pc.width()/std::uint32_t(ow),sy=std::uint32_t(y)*pc.height()/std::uint32_t(HostCanvas::H);
        const auto* s=pc.rgba().data()+(std::size_t(sy)*pc.width()+sx)*4u;float* d=c.rgb.data()+(std::size_t(y)*HostCanvas::W+ox+x)*3u;
        for(int k=0;k<3;++k)d[k]=float(s[k])/255.0f;
    }
    paint_frontend(r,c,frame);
    char name[64];std::snprintf(name,sizeof name,"host_fe_%05u.ppm",frame);(void)c.write(name);
}
}
platform::PcD3D9Device* switch_renderer_pc_device(SwitchRenderer&){return &host_pc_scene().device;}
bool switch_renderer_set_pc_scene(SwitchRenderer& renderer,std::function<void(platform::PcD3D9Device&)> scene){
    if(!renderer.impl)return false;
    auto& h=host_pc_scene();h.scene=std::move(scene);++h.frame;
    if(std::getenv("OR2_HOST_OBJS")&&h.frame%100u==0u)std::fprintf(stderr,"objs %u total=%zu live=%zu%s texbytes=%zu\n",h.frame,h.device.total_objects(),h.device.live_objects(),h.device.object_kinds().c_str(),h.device.texture_bytes);
    static const bool drawhash=std::getenv("OR2_HOST_DRAWHASH")!=nullptr;
    if(drawhash&&h.scene){
        h.device.hash_draws=true;h.device.count_only=true;h.device.draw_hash=0xcbf29ce484222325ull;h.device.draws_hashed=0;h.device.dump_frame=h.frame;
        try{h.scene(h.device);}catch(const std::exception& e){std::fprintf(stderr,"host frame %u drawhash threw: %s\n",h.frame,e.what());}
        h.device.count_only=false;h.device.hash_draws=false;
        std::fprintf(stderr,"drawhash %u %016llx %llu\n",h.frame,(unsigned long long)h.device.draw_hash,(unsigned long long)h.device.draws_hashed);
        // With OR2_HOST_PNG_EVERY too, every frame runs its displays (as on the console)
        // and the chosen frames are also rendered below.
        const char* every=std::getenv("OR2_HOST_PNG_EVERY");
        if(!every||h.frame%std::max(1ul,std::strtoul(every,nullptr,10))!=0u)return true;
    }
    const char* every=std::getenv("OR2_HOST_PNG_EVERY");
    const unsigned n=every?unsigned(std::strtoul(every,nullptr,10)):0u;
    // OR2_HOST_BENCH=1: every frame's scene runs on the count-only device;
    // the CPU time per frame is printed every 300 frames.
    if(h.scene&&std::getenv("OR2_HOST_BENCH")&&!(n&&h.frame%n==0u)){
        static double total=0.0;static unsigned frames=0;
        h.device.count_only=true;
        const auto t0=std::chrono::steady_clock::now();
        // OR2_HOST_BENCH=N>1 repeats the scene N times (profiling); the
        // printed time is per run.
        const unsigned repeat=std::max(1u,unsigned(std::strtoul(std::getenv("OR2_HOST_BENCH"),nullptr,10)));
        try{for(unsigned k=0;k<repeat;++k)h.scene(h.device);}catch(const std::exception& e){std::fprintf(stderr,"host frame %u bench threw: %s\n",h.frame,e.what());}
        total+=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-t0).count()/repeat;
        h.device.count_only=false;
        if(++frames==300u){std::fprintf(stderr,"host bench: PC scene %.3f ms/frame (300 frames)\n",total/frames);total=0.0;frames=0;}
    }
    if(n&&h.scene&&h.frame%n==0u){
        h.device.clear(3u,0xff000000u,1.0f,0u);
        try{h.scene(h.device);}
        catch(const std::exception& e){std::fprintf(stderr,"host frame %u render threw: %s\n",h.frame,e.what());return false;}
        char name[64];std::snprintf(name,sizeof name,"host_frame_%05u.png",h.frame);
        (void)h.device.write_png(name);
    }
    return true;
}
void switch_renderer_cycle_pc_diag(SwitchRenderer&){}
void switch_renderer_disable_pc_spec(){}
void switch_renderer_scene_to_frame(SwitchRenderer&){}
std::string switch_renderer_pc_error(const SwitchRenderer&){return "states:"+host_pc_scene().device.dip_state_summary(16);}
#else
platform::PcD3D9Device* switch_renderer_pc_device(SwitchRenderer&){return nullptr;}
bool switch_renderer_set_pc_scene(SwitchRenderer& renderer,std::function<void(platform::PcD3D9Device&)>){return renderer.impl!=nullptr;}
void switch_renderer_cycle_pc_diag(SwitchRenderer&){}
void switch_renderer_disable_pc_spec(){}
void switch_renderer_scene_to_frame(SwitchRenderer&){}
std::string switch_renderer_pc_error(const SwitchRenderer&){return "host renderer has no GPU device";}
#endif
bool switch_renderer_draw(SwitchRenderer& renderer){auto* impl=static_cast<RendererImpl*>(renderer.impl);if(!impl)return false;++impl->stats.frames;
#if defined(OR2_HOST_NRO)
    if(impl->frontend_visible&&impl->frontend_sprites&&!impl->start_loading_visible&&std::getenv("OR2_HOST_FE")){
        const char* every=std::getenv("OR2_HOST_PNG_EVERY");const unsigned n=every?unsigned(std::strtoul(every,nullptr,10)):0u;
        if(n&&impl->stats.frames%n==0u)host_frontend_png(*impl,impl->stats.frames);
    }
#endif
if(impl->movie_visible)++impl->stats.movie_frames;if(impl->start_loading_visible)++impl->stats.loading_frames;else if(impl->frontend_visible&&impl->frontend_sprites){impl->stats.font_draws=static_cast<unsigned>(impl->menu.glyphs.size());if(impl->stats.font_draws)++impl->stats.font_frames;impl->stats.frontend_instances=0;impl->stats.frontend_instance_draws=0;const auto& instances=impl->frontend_sprites->instances();for(std::uint32_t h=0;h<instances.size()&&!g_pc_sprites;++h){const auto& instance=instances[h];if(!instance.allocated||!instance.visible)continue;const auto* scene=platform::find_frontend_preview_scene(impl->frontend,instance.token);const auto bank=instance.token>>16u;if(!scene&&bank==0x44u&&!impl->primary_frontend_scenes){impl->frontend_sprites->drawn(h);continue;}if(!scene||(bank!=0x44u&&bank!=0x2cu)){std::fprintf(stderr,"frontend unsupported instance token=%08x\n",instance.token);return false;}std::vector<platform::GameUiDraw> draws;std::string frame_error;if(!platform::game_ui_scene_frame_draws(bank==0x2cu?impl->menu.shared:impl->frontend_animation,instance.token&0xffffu,instance.frame,draws,&frame_error)){std::fprintf(stderr,"frontend token=%08x frame=%f: %s\n",instance.token,instance.frame,frame_error.c_str());return false;}for(auto& draw:draws)platform::frontend_sprite_transform(draw,instance,instance.canvas_width,instance.canvas_height);++impl->stats.frontend_instances;impl->stats.frontend_instance_draws+=static_cast<std::uint32_t>(draws.size());if(instance.token==impl->frontend.scenes[impl->frontend_scene_index].token){++impl->stats.frontend_animation_updates;impl->stats.frontend_animation_frame=instance.frame;impl->stats.frontend_animation_last_frame=instance.last;impl->stats.frontend_animation_complete=instance.status!=1u;}impl->frontend_sprites->drawn(h);}++impl->stats.frontend_instance_frames;++impl->stats.frontend_frames;}else if(impl->frontend_visible){if(!impl->frontend_animation.animation.empty()){const auto& scene=impl->frontend.scenes[impl->frontend_scene_index];const auto source_scene=scene.token&0xffffu;if(!platform::game_ui_scene_frame_draws(impl->frontend_animation,source_scene,0.0f,impl->frontend_frame_draws)||impl->frontend_frame_draws.size()!=scene.draw_count)return false;float last=0.0f;bool moving=false;for(const auto& draw:impl->frontend_frame_draws){last=std::max(last,float(std::max<int>(0,draw.last_frame)));moving|=draw.has_keyframes||draw.last_frame>1;}impl->stats.frontend_animation_last_frame=last;if(moving){const float frame=std::min(impl->frontend_frame,last);if(!platform::game_ui_scene_frame_draws(impl->frontend_animation,source_scene,frame,impl->frontend_frame_draws))return false;++impl->stats.frontend_animation_updates;impl->stats.frontend_animation_frame=frame;impl->stats.frontend_animation_complete=frame>=last?1u:0u;if(impl->frontend_frame<last)impl->frontend_frame+=1.0f;}else{impl->stats.frontend_animation_frame=0.0f;impl->stats.frontend_animation_complete=1u;}}else impl->stats.frontend_animation_complete=1u;if(impl->frontend_overlay_token!=0u){std::vector<platform::GameUiDraw> overlay;if(!platform::game_ui_scene_frame_draws(impl->frontend_animation,impl->frontend_overlay_token&0xffffu,impl->frontend_overlay_frame,overlay))return false;for(const auto& draw:overlay)if(draw.texture>=impl->frontend_animation.textures.size())return false;++impl->stats.frontend_overlay_frames;impl->stats.frontend_overlay_token=impl->frontend_overlay_token;impl->stats.frontend_overlay_draws=static_cast<std::uint32_t>(overlay.size());}++impl->stats.frontend_frames;}else{++impl->stats.unowned_frames;}return true;}
SwitchRendererStats switch_renderer_stats(const SwitchRenderer& renderer){const auto* impl=static_cast<const RendererImpl*>(renderer.impl);return impl?impl->stats:SwitchRendererStats{};}
void switch_renderer_shutdown(SwitchRenderer& renderer){delete static_cast<RendererImpl*>(renderer.impl);renderer.impl=nullptr;}
#else
namespace {
constexpr unsigned FramebufferCount=2u;
constexpr std::uint32_t CodeMemorySize=64u*1024u;
// The full PC SUMO_FE pack records 2,428 sprite draws across 124 screens.
// Their three framebuffer command-list copies no longer fit the preview
// renderer's 512 KiB command pool.
constexpr std::uint32_t CommandMemorySize=8u*1024u*1024u;
constexpr std::uint32_t FontCommandMemorySize=256u*1024u;
constexpr std::uint32_t PcCommandMemorySize=8u*1024u*1024u;
constexpr std::uint32_t SamplerCount=9u;
constexpr std::uint32_t TransformCount=9u;
constexpr std::uint32_t FrontendIndicesPerDraw=6u;

struct TransformBlock {
    std::array<std::array<float,16>,TransformCount> position{};
    std::array<std::array<float,16>,TransformCount> normal{};
};
static_assert(sizeof(TransformBlock)==TransformCount*2u*16u*sizeof(float),"std140 transform block layout");

std::uint32_t align_up(std::uint32_t value,std::uint32_t alignment){return (value+alignment-1u)&~(alignment-1u);}
std::uint64_t align_up64(std::uint64_t value,std::uint64_t alignment){return (value+alignment-1u)&~(alignment-1u);}

DkImageFormat image_format(platform::MeshPreviewTextureFormat format){
    switch(format){
    case platform::MeshPreviewTextureFormat::bc1:return DkImageFormat_RGBA_BC1;
    case platform::MeshPreviewTextureFormat::bc2:return DkImageFormat_RGBA_BC2;
    case platform::MeshPreviewTextureFormat::bc3:return DkImageFormat_RGBA_BC3;
    }
    return DkImageFormat_None;
}

unsigned wrap_index(std::uint32_t code){return code==2u?1u:(code==3u?2u:0u);}
DkWrapMode wrap_mode(unsigned index){return index==1u?DkWrapMode_MirroredRepeat:(index==2u?DkWrapMode_ClampToEdge:DkWrapMode_Repeat);}
unsigned sampler_index(std::uint32_t attrib){return wrap_index((attrib>>10u)&7u)*3u+wrap_index((attrib>>13u)&7u);}

struct RendererImpl {
    MenuGraphics menu;
    platform::FrontendSprites* frontend_sprites{};
    platform::FrontendPreviewPack frontend;
    platform::GameUiPack frontend_animation;
    platform::GameUiPack loading_animation;
    std::vector<platform::GameUiDraw> frontend_frame_draws;
    float frontend_frame{};
    float loading_frame{};
    SwitchRendererStats stats{};
    DkDevice device{};
    DkQueue queue{};
    DkMemBlock framebuffer_memory{};
    DkMemBlock depth_memory{};
    DkMemBlock texture_memory{};
    DkMemBlock code_memory{};
    DkMemBlock command_memory{};
    DkMemBlock data_memory{};
    DkMemBlock font_command_memory{};
    DkCmdBuf font_command_buffer{};
    DkCmdList font_setup[FramebufferCount]{};
    std::array<DkCmdList,21> font_layers{};
    std::uint32_t font_descriptor_offset{};
    DkMemBlock movie_image_memory{},movie_upload_memory{},movie_command_memory{};
    DkImage movie_image{};
    DkCmdBuf movie_command_buffer{};
    DkCmdList movie_commands[FramebufferCount]{};
    unsigned movie_width{},movie_height{};
    bool movie_visible{};
    DkImage framebuffers[FramebufferCount]{};
    // The scaled / multisampled scene of the frame being recorded: finished
    // (resolved, blitted to the frame) when the 2D layer starts or at the end.
    struct SceneScale { bool pending{};int slot{};unsigned scale{100};bool msaa{};std::uint32_t fx{},fw{},fh{};bool fxaa{}; } scene_scale;
    DkImage msaa_colour{},msaa_depth{};DkMemBlock msaa_memory{};DkMsMode msaa_mode{DkMsMode_1x};
    // FXAA (g_aa_mode 3): shaders, one image + one sampler descriptor (the
    // scene in scaled_target) and its uniform.
    DkShader fxaa_vertex{},fxaa_fragment{};bool fxaa_ready{};
    DkMemBlock fxaa_memory{};
    DkImage scaled_target{};                 // options.ini render scale < 100: the PC scene renders here, then is blitted
    DkImage depth{};
    std::vector<DkImage> textures;
    DkSwapchain swapchain{};
    DkShader vertex_shader{};
    DkShader fragment_shader{};
    DkCmdBuf command_buffer{};
    DkCmdList bind_framebuffer[FramebufferCount]{};
    DkCmdList frontend_clear[FramebufferCount]{};
    std::uint32_t frontend_descriptor_offset{};
    std::size_t frontend_texture_base{};
    std::array<std::vector<DkCmdList>,FramebufferCount> frontend_commands{};
    std::uint32_t code_offset{};
    std::uint32_t index_offset{};
    std::uint32_t frontend_first_index{};
    std::array<std::uint32_t,FramebufferCount> uniform_offsets{};
    std::uint8_t* data_cpu{};
    std::size_t frontend_scene_index{};
    std::uint32_t frontend_overlay_token{};
    float frontend_overlay_frame{};
    std::uint32_t loading_scene_index{};
    std::uint32_t loading_scene_offset{};
    bool frontend_visible{true};
    bool start_loading_visible{};
    std::uint32_t primary_frontend_scenes{};
    std::uint32_t primary_frontend_textures{};
    // Ported PC renderer (449050 path).
    std::unique_ptr<SwitchD3D9Device> pc_device;
    std::string pc_error;
    DkMemBlock pc_command_memory{};
    DkCmdBuf pc_command_buffer{};
    std::array<DkFence,FramebufferCount> pc_fences{};
    std::array<bool,FramebufferCount> pc_fence_used{};
    // GPU timestamps around the PC scene (DkCounter_Timestamp reports, 16
    // bytes each: begin at slot*32, end at slot*32+16; the time at +8), read
    // back once the slot's fence passed: the profile's "PC scene GPU" bucket.
    DkMemBlock pc_timestamps{};
    std::array<bool,FramebufferCount> pc_times_valid{};
    std::function<void(platform::PcD3D9Device&)> pc_scene;
};

TransformBlock make_transform_block(){
    TransformBlock block{};const auto identity=platform::vehicle_visual_identity();
    for(unsigned i=0;i<TransformCount;++i){block.position[i]=identity;block.normal[i]=identity;}
    return block;
}

void initialize_frontend_stats(RendererImpl& r){
    r.stats.frontend_textures=r.primary_frontend_textures;
    r.stats.frontend_scenes=r.primary_frontend_scenes;
    if(r.frontend_scene_index>=r.frontend.scenes.size())return;
    const auto& scene=r.frontend.scenes[r.frontend_scene_index];
    r.stats.frontend_draws=scene.draw_count;r.stats.frontend_token=scene.token;
    r.stats.frontend_scene_width=scene.width;r.stats.frontend_scene_height=scene.height;
}

bool load_shader(RendererImpl& r,DkShader& shader,const char* path,std::string& error){
    std::FILE* file=std::fopen(path,"rb");if(!file){error=std::string("cannot open shader: ")+path;return false;}
    if(std::fseek(file,0,SEEK_END)!=0){std::fclose(file);error="cannot size shader";return false;}
    const auto length=std::ftell(file);std::rewind(file);
    if(length<=0||static_cast<std::uint64_t>(length)>CodeMemorySize){std::fclose(file);error="shader size outside code memory";return false;}
    const auto size=static_cast<std::uint32_t>(length),offset=align_up(r.code_offset,DK_SHADER_CODE_ALIGNMENT),next=align_up(offset+size,DK_SHADER_CODE_ALIGNMENT);
    if(next>CodeMemorySize){std::fclose(file);error="combined shaders exceed code memory";return false;}
    auto* target=static_cast<std::uint8_t*>(dkMemBlockGetCpuAddr(r.code_memory))+offset;
    const auto read=std::fread(target,1,size,file);std::fclose(file);if(read!=size){error="shader read failed";return false;}
    DkShaderMaker maker;dkShaderMakerDefaults(&maker,r.code_memory,offset);dkShaderInitialize(&shader,&maker);r.code_offset=next;return true;
}

void destroy_impl(RendererImpl& r){
    if(r.queue)dkQueueWaitIdle(r.queue);
    if(r.pc_device){r.pc_device->shutdown();r.pc_device.reset();}
    if(r.pc_command_buffer){dkCmdBufDestroy(r.pc_command_buffer);r.pc_command_buffer=nullptr;}
    if(r.pc_command_memory){dkMemBlockDestroy(r.pc_command_memory);r.pc_command_memory=nullptr;}
    if(r.pc_timestamps){dkMemBlockDestroy(r.pc_timestamps);r.pc_timestamps=nullptr;}
    if(r.queue){dkQueueWaitIdle(r.queue);dkQueueDestroy(r.queue);r.queue=nullptr;}
    if(r.command_buffer){dkCmdBufDestroy(r.command_buffer);r.command_buffer=nullptr;}
    if(r.font_command_buffer)dkCmdBufDestroy(r.font_command_buffer);
    if(r.font_command_memory)dkMemBlockDestroy(r.font_command_memory);
    if(r.movie_command_buffer)dkCmdBufDestroy(r.movie_command_buffer);
    if(r.movie_command_memory)dkMemBlockDestroy(r.movie_command_memory);
    if(r.movie_upload_memory)dkMemBlockDestroy(r.movie_upload_memory);
    if(r.movie_image_memory)dkMemBlockDestroy(r.movie_image_memory);
    if(r.swapchain){dkSwapchainDestroy(r.swapchain);r.swapchain=nullptr;}
    if(r.texture_memory){dkMemBlockDestroy(r.texture_memory);r.texture_memory=nullptr;}
    if(r.data_memory){dkMemBlockDestroy(r.data_memory);r.data_memory=nullptr;}
    if(r.command_memory){dkMemBlockDestroy(r.command_memory);r.command_memory=nullptr;}
    if(r.code_memory){dkMemBlockDestroy(r.code_memory);r.code_memory=nullptr;}
    if(r.depth_memory){dkMemBlockDestroy(r.depth_memory);r.depth_memory=nullptr;}
    if(r.msaa_memory){dkMemBlockDestroy(r.msaa_memory);r.msaa_memory=nullptr;}
    if(r.fxaa_memory){dkMemBlockDestroy(r.fxaa_memory);r.fxaa_memory=nullptr;}
    if(r.framebuffer_memory){dkMemBlockDestroy(r.framebuffer_memory);r.framebuffer_memory=nullptr;}
    if(r.device){dkDeviceDestroy(r.device);r.device=nullptr;}
}

// Multisampled scene targets (antialiasing 2x / 4x), resolved into
// scaled_target every frame. samples other than 2 / 4 frees them (1x).
void create_msaa(RendererImpl& r,unsigned samples){
    if(r.msaa_memory){dkMemBlockDestroy(r.msaa_memory);r.msaa_memory=nullptr;}
    r.msaa_mode=DkMsMode_1x;
    if(samples!=2u&&samples!=4u)return;
    const DkMsMode ms=samples==4u?DkMsMode_4x:DkMsMode_2x;
    DkImageLayoutMaker image_maker;DkImageLayout cl,dl;
    dkImageLayoutMakerDefaults(&image_maker,r.device);image_maker.flags=DkImageFlags_UsageRender;image_maker.format=DkImageFormat_RGBA8_Unorm;
    image_maker.msMode=ms;image_maker.dimensions[0]=FramebufferWidth;image_maker.dimensions[1]=FramebufferHeight;dkImageLayoutInitialize(&cl,&image_maker);
    dkImageLayoutMakerDefaults(&image_maker,r.device);image_maker.flags=DkImageFlags_UsageRender|DkImageFlags_HwCompression;image_maker.format=DkImageFormat_Z24S8;
    image_maker.msMode=ms;image_maker.dimensions[0]=FramebufferWidth;image_maker.dimensions[1]=FramebufferHeight;dkImageLayoutInitialize(&dl,&image_maker);
    const auto ca=dkImageLayoutGetAlignment(&cl),da=dkImageLayoutGetAlignment(&dl);
    const auto csize=align_up(static_cast<std::uint32_t>(dkImageLayoutGetSize(&cl)),da>ca?da:ca);
    const auto total=align_up(csize+static_cast<std::uint32_t>(dkImageLayoutGetSize(&dl)),DK_MEMBLOCK_ALIGNMENT);
    DkMemBlockMaker memory_maker;dkMemBlockMakerDefaults(&memory_maker,r.device,total);memory_maker.flags=DkMemBlockFlags_GpuCached|DkMemBlockFlags_Image;
    r.msaa_memory=dkMemBlockCreate(&memory_maker);
    if(r.msaa_memory){dkImageInitialize(&r.msaa_colour,&cl,r.msaa_memory,0u);dkImageInitialize(&r.msaa_depth,&dl,r.msaa_memory,csize);r.msaa_mode=ms;}
}
// FXAA resources: optional (a missing shader leaves the mode unavailable).
constexpr std::uint32_t FxaaImageDescriptor=0x0u,FxaaSamplerDescriptor=0x40u,FxaaUniform=0x100u,FxaaMemory=0x1000u;
void create_fxaa(RendererImpl& r){
    std::string error;
    if(!load_shader(r,r.fxaa_vertex,"romfs:/shaders/fxaa_vsh.dksh",error)||!load_shader(r,r.fxaa_fragment,"romfs:/shaders/fxaa_fsh.dksh",error))return;
    DkMemBlockMaker memory_maker;dkMemBlockMakerDefaults(&memory_maker,r.device,align_up(FxaaMemory,DK_MEMBLOCK_ALIGNMENT));
    memory_maker.flags=DkMemBlockFlags_CpuUncached|DkMemBlockFlags_GpuCached;
    r.fxaa_memory=dkMemBlockCreate(&memory_maker);if(!r.fxaa_memory)return;
    auto* cpu=static_cast<std::uint8_t*>(dkMemBlockGetCpuAddr(r.fxaa_memory));
    DkImageView view;dkImageViewDefaults(&view,&r.scaled_target);
    DkImageDescriptor image;dkImageDescriptorInitialize(&image,&view,false,false);std::memcpy(cpu+FxaaImageDescriptor,&image,sizeof(image));
    DkSampler sampler;dkSamplerDefaults(&sampler);sampler.minFilter=DkFilter_Linear;sampler.magFilter=DkFilter_Linear;
    sampler.wrapMode[0]=DkWrapMode_ClampToEdge;sampler.wrapMode[1]=DkWrapMode_ClampToEdge;
    DkSamplerDescriptor sd;dkSamplerDescriptorInitialize(&sd,&sampler);std::memcpy(cpu+FxaaSamplerDescriptor,&sd,sizeof(sd));
    r.fxaa_ready=true;r.stats.fxaa_available=1u;
}
// FXAA: draws scaled_target (the scene, src rectangle) into the frame (dst
// rectangle) in the PC command buffer, in place of the plain upscale blit.
// The PC device then re-binds its frame resources.
void fxaa_draw(RendererImpl& r,DkCmdBuf cmd,int slot,const float dst[4],const float src[4]){
    DkImageView frame;dkImageViewDefaults(&frame,&r.framebuffers[slot]);
    const DkImageView* colours[1]{&frame};dkCmdBufBindRenderTargets(cmd,colours,1u,nullptr);
    const DkViewport viewport{dst[0],dst[1],dst[2],dst[3],0.0f,1.0f};dkCmdBufSetViewports(cmd,0u,&viewport,1u);
    const DkScissor scissor{std::uint32_t(dst[0]),std::uint32_t(dst[1]),std::uint32_t(dst[2]),std::uint32_t(dst[3])};dkCmdBufSetScissors(cmd,0u,&scissor,1u);
    const DkShader* shaders[2]{&r.fxaa_vertex,&r.fxaa_fragment};dkCmdBufBindShaders(cmd,DkStageFlag_GraphicsMask,shaders,2u);
    DkRasterizerState rasterizer;dkRasterizerStateDefaults(&rasterizer);rasterizer.cullMode=DkFace_None;dkCmdBufBindRasterizerState(cmd,&rasterizer);
    DkColorState colour;dkColorStateDefaults(&colour);dkCmdBufBindColorState(cmd,&colour);
    DkColorWriteState write;dkColorWriteStateDefaults(&write);dkCmdBufBindColorWriteState(cmd,&write);
    DkDepthStencilState depth;dkDepthStencilStateDefaults(&depth);depth.depthTestEnable=false;depth.depthWriteEnable=false;depth.stencilTestEnable=false;
    dkCmdBufBindDepthStencilState(cmd,&depth);
    const auto gpu=dkMemBlockGetGpuAddr(r.fxaa_memory);
    dkCmdBufBindImageDescriptorSet(cmd,gpu+FxaaImageDescriptor,1u);dkCmdBufBindSamplerDescriptorSet(cmd,gpu+FxaaSamplerDescriptor,1u);
    dkCmdBufBindTexture(cmd,DkStage_Fragment,0u,dkMakeTextureHandle(0u,0u));
    float params[12];for(unsigned k=0;k<4;++k){params[k]=dst[k];params[4+k]=src[k];}
    params[8]=1.0f/float(FramebufferWidth);params[9]=1.0f/float(FramebufferHeight);params[10]=params[11]=0.0f;   // scaled_target size
    dkCmdBufBindUniformBuffer(cmd,DkStage_Fragment,0u,gpu+FxaaUniform,DK_UNIFORM_BUF_ALIGNMENT);
    dkCmdBufPushConstants(cmd,gpu+FxaaUniform,DK_UNIFORM_BUF_ALIGNMENT,0u,sizeof(params),params);
    dkCmdBufBindVtxAttribState(cmd,nullptr,0u);dkCmdBufBindVtxBufferState(cmd,nullptr,0u);
    dkCmdBufDraw(cmd,DkPrimitive_Triangles,3u,1u,0u,0u);
    ++r.stats.fxaa_frames;
}
bool initialize_impl(RendererImpl& r,std::string& error){
    DkDeviceMaker device_maker;dkDeviceMakerDefaults(&device_maker);r.device=dkDeviceCreate(&device_maker);if(!r.device){error="deko3d device creation failed";return false;}

    DkImageLayoutMaker image_maker;dkImageLayoutMakerDefaults(&image_maker,r.device);image_maker.flags=DkImageFlags_UsageRender|DkImageFlags_UsagePresent|DkImageFlags_Usage2DEngine;image_maker.format=DkImageFormat_RGBA8_Unorm;image_maker.dimensions[0]=FramebufferWidth;image_maker.dimensions[1]=FramebufferHeight;
    DkImageLayout framebuffer_layout;dkImageLayoutInitialize(&framebuffer_layout,&image_maker);
    const auto framebuffer_alignment=dkImageLayoutGetAlignment(&framebuffer_layout);const auto framebuffer_size=align_up(static_cast<std::uint32_t>(dkImageLayoutGetSize(&framebuffer_layout)),framebuffer_alignment);
    DkMemBlockMaker memory_maker;dkMemBlockMakerDefaults(&memory_maker,r.device,align_up((FramebufferCount+1u)*framebuffer_size,DK_MEMBLOCK_ALIGNMENT));memory_maker.flags=DkMemBlockFlags_GpuCached|DkMemBlockFlags_Image;r.framebuffer_memory=dkMemBlockCreate(&memory_maker);if(!r.framebuffer_memory){error="framebuffer memory allocation failed";return false;}
    const DkImage* swapchain_images[FramebufferCount]{};for(unsigned i=0;i<FramebufferCount;++i){dkImageInitialize(&r.framebuffers[i],&framebuffer_layout,r.framebuffer_memory,i*framebuffer_size);swapchain_images[i]=&r.framebuffers[i];}
    dkImageInitialize(&r.scaled_target,&framebuffer_layout,r.framebuffer_memory,FramebufferCount*framebuffer_size);

    dkImageLayoutMakerDefaults(&image_maker,r.device);image_maker.flags=DkImageFlags_UsageRender|DkImageFlags_HwCompression;image_maker.format=DkImageFormat_Z24S8;image_maker.dimensions[0]=FramebufferWidth;image_maker.dimensions[1]=FramebufferHeight;
    DkImageLayout depth_layout;dkImageLayoutInitialize(&depth_layout,&image_maker);dkMemBlockMakerDefaults(&memory_maker,r.device,align_up(static_cast<std::uint32_t>(dkImageLayoutGetSize(&depth_layout)),DK_MEMBLOCK_ALIGNMENT));memory_maker.flags=DkMemBlockFlags_GpuCached|DkMemBlockFlags_Image;r.depth_memory=dkMemBlockCreate(&memory_maker);if(!r.depth_memory){error="depth memory allocation failed";return false;}dkImageInitialize(&r.depth,&depth_layout,r.depth_memory,0u);

    create_msaa(r,g_msaa_samples);
    DkSwapchainMaker swapchain_maker;dkSwapchainMakerDefaults(&swapchain_maker,r.device,nwindowGetDefault(),swapchain_images,FramebufferCount);r.swapchain=dkSwapchainCreate(&swapchain_maker);if(!r.swapchain){error="swapchain creation failed";return false;}

    dkMemBlockMakerDefaults(&memory_maker,r.device,CodeMemorySize);memory_maker.flags=DkMemBlockFlags_CpuUncached|DkMemBlockFlags_GpuCached|DkMemBlockFlags_Code;r.code_memory=dkMemBlockCreate(&memory_maker);if(!r.code_memory){error="shader memory allocation failed";return false;}
    if(!load_shader(r,r.vertex_shader,"romfs:/shaders/mesh_preview_vsh.dksh",error)||!load_shader(r,r.fragment_shader,"romfs:/shaders/mesh_preview_fsh.dksh",error))return false;
    create_fxaa(r);

    // Images: white, then the frontend textures (SUMO_FE, START loading, menu graphics).
    const std::size_t frontend_image_base=1u;
    const std::size_t image_count=frontend_image_base+r.frontend.textures.size();
    const std::size_t frontend_descriptor_count=1u+r.frontend.textures.size();
    if(frontend_descriptor_count!=r.stats.frontend_descriptors){error="descriptor layout accounting mismatch";return false;}
    const auto texture_format=[&](std::size_t i){return r.frontend.textures[i-1u].format;};
    const auto texture_width=[&](std::size_t i){return r.frontend.textures[i-1u].width;};
    const auto texture_height=[&](std::size_t i){return r.frontend.textures[i-1u].height;};
    const auto texture_mips=[&](std::size_t){return 1u;};
    const auto& texture_bytes=[&](std::size_t i)->const std::vector<std::uint8_t>&{return r.frontend.textures[i-1u].bytes;};
    std::vector<DkImageLayout> texture_layouts(image_count);std::vector<std::uint32_t> texture_offsets(image_count);r.textures.resize(image_count);
    std::uint64_t texture_memory_size=0u;
    for(std::size_t i=0;i<image_count;++i){
        dkImageLayoutMakerDefaults(&image_maker,r.device);image_maker.type=DkImageType_2D;image_maker.format=i==0u?DkImageFormat_RGBA8_Unorm:image_format(texture_format(i));image_maker.dimensions[0]=i==0u?1u:texture_width(i);image_maker.dimensions[1]=i==0u?1u:texture_height(i);image_maker.mipLevels=i==0u?1u:texture_mips(i);
        if(image_maker.format==DkImageFormat_None){error="unsupported texture format";return false;}dkImageLayoutInitialize(&texture_layouts[i],&image_maker);texture_memory_size=align_up64(texture_memory_size,dkImageLayoutGetAlignment(&texture_layouts[i]));if(texture_memory_size>std::numeric_limits<std::uint32_t>::max()){error="texture memory offset overflow";return false;}texture_offsets[i]=static_cast<std::uint32_t>(texture_memory_size);texture_memory_size+=dkImageLayoutGetSize(&texture_layouts[i]);
    }
    texture_memory_size=align_up64(texture_memory_size,DK_MEMBLOCK_ALIGNMENT);if(texture_memory_size==0u||texture_memory_size>std::numeric_limits<std::uint32_t>::max()){error="texture memory size overflow";return false;}
    dkMemBlockMakerDefaults(&memory_maker,r.device,static_cast<std::uint32_t>(texture_memory_size));memory_maker.flags=DkMemBlockFlags_GpuCached|DkMemBlockFlags_Image;r.texture_memory=dkMemBlockCreate(&memory_maker);if(!r.texture_memory){error="texture image memory allocation failed";return false;}
    for(std::size_t i=0;i<image_count;++i)dkImageInitialize(&r.textures[i],&texture_layouts[i],r.texture_memory,texture_offsets[i]);

    const auto frontend_vertex_count=static_cast<std::uint32_t>(r.frontend.draws.size()*FrontendVerticesPerDraw);
    const auto vertex_bytes=static_cast<std::uint32_t>(frontend_vertex_count*sizeof(platform::MeshPreviewVertex));
    r.index_offset=align_up(vertex_bytes,alignof(std::uint32_t));
    const auto frontend_index_count=static_cast<std::uint32_t>(r.frontend.draws.size()*FrontendIndicesPerDraw);
    const auto index_bytes=static_cast<std::uint32_t>(frontend_index_count*sizeof(std::uint32_t));
    r.frontend_first_index=0u;
    const auto frontend_image_descriptor_offset=align_up(r.index_offset+index_bytes,DK_IMAGE_DESCRIPTOR_ALIGNMENT);
    std::size_t frontend_draw_descriptors=1u;
    for(const auto& scene:r.frontend.scenes)frontend_draw_descriptors=std::max(frontend_draw_descriptors,std::size_t(scene.draw_count));
    const auto frontend_image_descriptor_bytes=static_cast<std::uint32_t>(frontend_draw_descriptors*sizeof(DkImageDescriptor));
    r.frontend_descriptor_offset=frontend_image_descriptor_offset;r.frontend_texture_base=frontend_image_base;
    r.font_descriptor_offset=align_up(frontend_image_descriptor_offset+frontend_image_descriptor_bytes,DK_IMAGE_DESCRIPTOR_ALIGNMENT);
    const auto sampler_descriptor_offset=align_up(r.font_descriptor_offset+r.menu.descriptors()*sizeof(DkImageDescriptor),DK_SAMPLER_DESCRIPTOR_ALIGNMENT);const auto sampler_descriptor_bytes=SamplerCount*sizeof(DkSamplerDescriptor);
    std::vector<std::uint32_t> staging_offsets(image_count);std::uint64_t data_end=sampler_descriptor_offset+sampler_descriptor_bytes;
    for(unsigned i=0;i<FramebufferCount;++i){data_end=align_up64(data_end,DK_UNIFORM_BUF_ALIGNMENT);if(data_end>std::numeric_limits<std::uint32_t>::max()){error="uniform buffer offset overflow";return false;}r.uniform_offsets[i]=static_cast<std::uint32_t>(data_end);data_end+=sizeof(TransformBlock);}
    data_end=align_up64(data_end,DK_IMAGE_LINEAR_STRIDE_ALIGNMENT);
    for(std::size_t i=0;i<image_count;++i){data_end=align_up64(data_end,DK_IMAGE_LINEAR_STRIDE_ALIGNMENT);if(data_end>std::numeric_limits<std::uint32_t>::max()){error="texture staging offset overflow";return false;}staging_offsets[i]=static_cast<std::uint32_t>(data_end);data_end+=i==0u?4u:texture_bytes(i).size();}
    data_end=align_up64(data_end,DK_MEMBLOCK_ALIGNMENT);if(data_end>std::numeric_limits<std::uint32_t>::max()){error="renderer data memory size overflow";return false;}
    dkMemBlockMakerDefaults(&memory_maker,r.device,static_cast<std::uint32_t>(data_end));memory_maker.flags=DkMemBlockFlags_CpuUncached|DkMemBlockFlags_GpuCached;r.data_memory=dkMemBlockCreate(&memory_maker);if(!r.data_memory){error="renderer data memory allocation failed";return false;}
    auto* data=static_cast<std::uint8_t*>(dkMemBlockGetCpuAddr(r.data_memory));r.data_cpu=data;
    for(std::size_t draw=0;draw<r.frontend.draws.size();++draw){
        const auto quad=make_frontend_quad(r.frontend,r.frontend.draws[draw]);
        std::memcpy(data+draw*sizeof(quad),quad.data(),sizeof(quad));
    }
    constexpr std::array<std::uint32_t,FrontendIndicesPerDraw> quad_indices{{0u,1u,2u,2u,3u,0u}};
    for(std::size_t draw=0;draw<r.frontend.draws.size();++draw){
        std::array<std::uint32_t,FrontendIndicesPerDraw> indices{};
        const auto base=static_cast<std::uint32_t>(draw*FrontendVerticesPerDraw);
        for(std::size_t i=0;i<indices.size();++i)indices[i]=base+quad_indices[i];
        std::memcpy(data+r.index_offset+draw*sizeof(indices),indices.data(),sizeof(indices));
    }
    const auto initial_transforms=make_transform_block();for(unsigned i=0;i<FramebufferCount;++i)std::memcpy(data+r.uniform_offsets[i],&initial_transforms,sizeof(initial_transforms));const std::uint8_t white[4]={255u,255u,255u,255u};std::memcpy(data+staging_offsets[0],white,4u);for(std::size_t i=1;i<image_count;++i)std::memcpy(data+staging_offsets[i],texture_bytes(i).data(),texture_bytes(i).size());

    dkMemBlockMakerDefaults(&memory_maker,r.device,CommandMemorySize);memory_maker.flags=DkMemBlockFlags_CpuUncached|DkMemBlockFlags_GpuCached;r.command_memory=dkMemBlockCreate(&memory_maker);if(!r.command_memory){error="command memory allocation failed";return false;}
    DkCmdBufMaker command_maker;dkCmdBufMakerDefaults(&command_maker,r.device);r.command_buffer=dkCmdBufCreate(&command_maker);if(!r.command_buffer){error="command buffer creation failed";return false;}dkCmdBufAddMemory(r.command_buffer,r.command_memory,0u,CommandMemorySize);
    const auto gpu=dkMemBlockGetGpuAddr(r.data_memory);
    // Sprite descriptors are draw-local and refreshed from the evaluated
    // atlas frame before submission. Each binding page is at most 256 entries.

    for(std::size_t i=0;i<image_count;++i){
        std::uint32_t width=i==0u?1u:texture_width(i),height=i==0u?1u:texture_height(i);
        const std::uint32_t levels=i==0u?1u:texture_mips(i);
        std::uint32_t source_offset=staging_offsets[i];
        for(std::uint32_t level=0;level<levels;++level){
            DkImageView level_view;dkImageViewDefaults(&level_view,&r.textures[i]);
            level_view.mipLevelOffset=static_cast<std::uint8_t>(level);level_view.mipLevelCount=1u;
            const DkCopyBuf source{gpu+source_offset,0u,0u};
            const DkImageRect rect{0u,0u,0u,width,height,1u};
            dkCmdBufCopyBufferToImage(r.command_buffer,&source,&level_view,&rect,0u);
            if(i==0u)source_offset+=4u;
            else{
                const std::uint32_t block_bytes=texture_format(i)==platform::MeshPreviewTextureFormat::bc1?8u:16u;
                source_offset+=std::max(1u,(width+3u)/4u)*std::max(1u,(height+3u)/4u)*block_bytes;
            }
            width=std::max(1u,width/2u);height=std::max(1u,height/2u);
        }
    }
    for(unsigned s=0;s<3u;++s)for(unsigned t=0;t<3u;++t){DkSampler sampler;dkSamplerDefaults(&sampler);sampler.minFilter=DkFilter_Linear;sampler.magFilter=DkFilter_Linear;sampler.mipFilter=DkMipFilter_Linear;sampler.maxAnisotropy=8.0f;sampler.wrapMode[0]=wrap_mode(s);sampler.wrapMode[1]=wrap_mode(t);DkSamplerDescriptor descriptor;dkSamplerDescriptorInitialize(&descriptor,&sampler);const auto id=s*3u+t;dkCmdBufPushData(r.command_buffer,gpu+sampler_descriptor_offset+id*sizeof(DkSamplerDescriptor),&descriptor,sizeof(descriptor));}
    if(r.menu.fonts_loaded)for(unsigned i=0;i<r.menu.descriptors();++i){
        const unsigned texture=i<10?r.menu.font_texture_slot+i:r.menu.image_texture_slot+i-10;
        DkImageView view;dkImageViewDefaults(&view,&r.textures[frontend_image_base+texture]);
        DkImageDescriptor descriptor;dkImageDescriptorInitialize(&descriptor,&view,false,false);
        dkCmdBufPushData(r.command_buffer,gpu+r.font_descriptor_offset+i*sizeof(descriptor),&descriptor,sizeof(descriptor));
    }
    const auto upload_commands=dkCmdBufFinishList(r.command_buffer);

    for(unsigned i=0;i<FramebufferCount;++i){DkImageView color_view,depth_view;dkImageViewDefaults(&color_view,&r.framebuffers[i]);dkImageViewDefaults(&depth_view,&r.depth);const DkImageView* colors[1]{&color_view};dkCmdBufBindRenderTargets(r.command_buffer,colors,1u,&depth_view);r.bind_framebuffer[i]=dkCmdBufFinishList(r.command_buffer);}

    for(unsigned slot=0;slot<FramebufferCount;++slot){
        const DkViewport viewport{0.0f,0.0f,float(FramebufferWidth),float(FramebufferHeight),0.0f,1.0f};const DkScissor scissor{0u,0u,FramebufferWidth,FramebufferHeight};
        const DkShader* shaders[2]{&r.vertex_shader,&r.fragment_shader};
        DkRasterizerState rasterizer;DkColorWriteState write;dkRasterizerStateDefaults(&rasterizer);rasterizer.cullMode=DkFace_None;dkColorWriteStateDefaults(&write);
        DkVtxAttribState attributes[5]{};const auto attribute=[&](DkVtxAttribState& state,std::uint32_t offset,DkVtxAttribSize size){state.bufferId=0u;state.isFixed=0u;state.offset=offset;state.size=size;state.type=DkVtxAttribType_Float;state.isBgra=0u;};attribute(attributes[0],static_cast<std::uint32_t>(offsetof(platform::MeshPreviewVertex,position)),DkVtxAttribSize_3x32);attribute(attributes[1],static_cast<std::uint32_t>(offsetof(platform::MeshPreviewVertex,normal)),DkVtxAttribSize_3x32);attribute(attributes[2],static_cast<std::uint32_t>(offsetof(platform::MeshPreviewVertex,uv)),DkVtxAttribSize_2x32);attribute(attributes[3],static_cast<std::uint32_t>(offsetof(platform::MeshPreviewVertex,color)),DkVtxAttribSize_4x32);attribute(attributes[4],static_cast<std::uint32_t>(offsetof(platform::MeshPreviewVertex,transform_index)),DkVtxAttribSize_1x32);
        const DkVtxBufferState vertex_state{sizeof(platform::MeshPreviewVertex),0u};

        r.frontend_commands[slot].reserve(r.frontend.scenes.size());
        // The PC sprite canvas clips its off-canvas wipe layers at 640x480.
        // Keep a full-screen clear, then clip only sprite drawing to that
        // letterboxed canvas (208,36)-(1072,684) at 1280x720.
        constexpr float ui_half_y=0.90f;
        constexpr float ui_half_x=ui_half_y*(640.0f/480.0f)/
                                  (float(FramebufferWidth)/float(FramebufferHeight));
        const DkScissor ui_scissor{
            static_cast<std::uint32_t>((1.0f-ui_half_x)*0.5f*FramebufferWidth),
            static_cast<std::uint32_t>((1.0f-ui_half_y)*0.5f*FramebufferHeight),
            static_cast<std::uint32_t>(ui_half_x*FramebufferWidth),
            static_cast<std::uint32_t>(ui_half_y*FramebufferHeight)};
        dkCmdBufSetViewports(r.command_buffer,0u,&viewport,1u);dkCmdBufSetScissors(r.command_buffer,0u,&scissor,1u);dkCmdBufClearColorFloat(r.command_buffer,0u,DkColorMask_RGBA,0.025f,0.04f,0.07f,1.0f);dkCmdBufClearDepthStencil(r.command_buffer,true,1.0f,0xffu,0u);
        r.frontend_clear[slot]=dkCmdBufFinishList(r.command_buffer);
        if(r.menu.fonts_loaded){
            dkCmdBufBarrier(r.command_buffer,DkBarrier_Full,DkInvalidateFlags_Descriptors|DkInvalidateFlags_L2Cache);
            dkCmdBufSetViewports(r.command_buffer,0u,&viewport,1u);
            dkCmdBufSetScissors(r.command_buffer,0u,&ui_scissor,1u);
            dkCmdBufBindShaders(r.command_buffer,DkStageFlag_GraphicsMask,shaders,2u);
            dkCmdBufBindRasterizerState(r.command_buffer,&rasterizer);
            dkCmdBufBindColorWriteState(r.command_buffer,&write);
            dkCmdBufBindImageDescriptorSet(r.command_buffer,gpu+r.font_descriptor_offset,r.menu.descriptors());
            dkCmdBufBindSamplerDescriptorSet(r.command_buffer,gpu+sampler_descriptor_offset,SamplerCount);
            dkCmdBufBindUniformBuffer(r.command_buffer,DkStage_Vertex,0u,gpu+r.uniform_offsets[slot],sizeof(TransformBlock));
            dkCmdBufBindVtxBuffer(r.command_buffer,0u,gpu,vertex_bytes);
            dkCmdBufBindVtxAttribState(r.command_buffer,attributes,5u);
            dkCmdBufBindVtxBufferState(r.command_buffer,&vertex_state,1u);
            dkCmdBufBindIdxBuffer(r.command_buffer,DkIdxFormat_Uint32,gpu+r.index_offset);
            DkColorState color;DkBlendState blend;DkDepthStencilState depth_state;
            dkColorStateDefaults(&color);dkBlendStateDefaults(&blend);dkDepthStencilStateDefaults(&depth_state);
            dkColorStateSetBlendEnable(&color,0u,true);
            depth_state.depthTestEnable=false;depth_state.depthWriteEnable=false;
            dkCmdBufBindColorState(r.command_buffer,&color);dkCmdBufBindBlendState(r.command_buffer,0u,&blend);
            dkCmdBufBindDepthStencilState(r.command_buffer,&depth_state);
            r.font_setup[slot]=dkCmdBufFinishList(r.command_buffer);
        }
        for(const auto& scene:r.frontend.scenes){
            dkCmdBufBarrier(r.command_buffer,DkBarrier_Full,
                DkInvalidateFlags_Descriptors|DkInvalidateFlags_L2Cache);
            dkCmdBufSetViewports(r.command_buffer,0u,&viewport,1u);
            dkCmdBufSetScissors(r.command_buffer,0u,&ui_scissor,1u);
            dkCmdBufBindShaders(r.command_buffer,DkStageFlag_GraphicsMask,shaders,2u);dkCmdBufBindRasterizerState(r.command_buffer,&rasterizer);dkCmdBufBindColorWriteState(r.command_buffer,&write);dkCmdBufBindSamplerDescriptorSet(r.command_buffer,gpu+sampler_descriptor_offset,SamplerCount);dkCmdBufBindUniformBuffer(r.command_buffer,DkStage_Vertex,0u,gpu+r.uniform_offsets[slot],sizeof(TransformBlock));dkCmdBufBindVtxBuffer(r.command_buffer,0u,gpu,vertex_bytes);dkCmdBufBindVtxAttribState(r.command_buffer,attributes,5u);dkCmdBufBindVtxBufferState(r.command_buffer,&vertex_state,1u);dkCmdBufBindIdxBuffer(r.command_buffer,DkIdxFormat_Uint32,gpu+r.index_offset);
            DkColorState color;DkBlendState blend;DkDepthStencilState depth_state;dkColorStateDefaults(&color);dkBlendStateDefaults(&blend);dkDepthStencilStateDefaults(&depth_state);dkColorStateSetBlendEnable(&color,0u,true);depth_state.depthTestEnable=false;depth_state.depthWriteEnable=false;dkCmdBufBindColorState(r.command_buffer,&color);dkCmdBufBindBlendState(r.command_buffer,0u,&blend);dkCmdBufBindDepthStencilState(r.command_buffer,&depth_state);
            const auto draw_end=scene.first_draw+scene.draw_count;
            for(std::uint32_t draw=scene.first_draw;draw<draw_end;++draw){
                const auto local=draw-scene.first_draw;
                if(local%256u==0u)dkCmdBufBindImageDescriptorSet(r.command_buffer,
                    gpu+frontend_image_descriptor_offset+local*sizeof(DkImageDescriptor),
                    std::min(256u,scene.draw_count-local));
                dkCmdBufBindTexture(r.command_buffer,DkStage_Fragment,0u,dkMakeTextureHandle(local%256u,8u));
                dkCmdBufDrawIndexed(r.command_buffer,DkPrimitive_Triangles,FrontendIndicesPerDraw,1u,r.frontend_first_index+draw*FrontendIndicesPerDraw,0,0u);
            }
            r.frontend_commands[slot].push_back(dkCmdBufFinishList(r.command_buffer));
        }
    }

    if(r.menu.fonts_loaded){
        dkMemBlockMakerDefaults(&memory_maker,r.device,FontCommandMemorySize);
        memory_maker.flags=DkMemBlockFlags_CpuUncached|DkMemBlockFlags_GpuCached;
        r.font_command_memory=dkMemBlockCreate(&memory_maker);
        DkCmdBufMaker maker;dkCmdBufMakerDefaults(&maker,r.device);
        r.font_command_buffer=dkCmdBufCreate(&maker);
        if(!r.font_command_memory||!r.font_command_buffer){error="font command allocation failed";return false;}
        dkCmdBufAddMemory(r.font_command_buffer,r.font_command_memory,0u,FontCommandMemorySize);
    }
    DkQueueMaker queue_maker;dkQueueMakerDefaults(&queue_maker,r.device);queue_maker.flags=DkQueueFlags_Graphics;r.queue=dkQueueCreate(&queue_maker);if(!r.queue){error="render queue creation failed";return false;}dkQueueSubmitCommands(r.queue,upload_commands);dkQueueWaitIdle(r.queue);if(dkQueueIsInErrorState(r.queue)){error="texture upload queue error";return false;}return true;
}
}

bool switch_renderer_initialize(SwitchRenderer& renderer,const char* frontend_path,std::string& error,
                                const std::vector<std::uint8_t>* start_loading,const char* shared_ui_path,
                                const char* font_path,const char* image_path){
    error.clear();auto* impl=new(std::nothrow) RendererImpl{};if(!impl){error="renderer allocation failed";return false;}
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
    if(impl->frontend.textures.empty()||impl->frontend.scenes.empty()){delete impl;error="frontend pack is empty";return false;}
    initialize_frontend_stats(*impl);renderer.impl=impl;
    if(!initialize_impl(*impl,error)){switch_renderer_shutdown(renderer);return false;}
    // The ported PC renderer is optional for the preview renderer: a failure
    // is kept in pc_error and reported by switch_renderer_pc_error.
    {   auto device=std::make_unique<SwitchD3D9Device>();
        if(device->initialize(impl->device,impl->queue,impl->pc_error)){
            DkMemBlockMaker m;dkMemBlockMakerDefaults(&m,impl->device,PcCommandMemorySize*FramebufferCount);
            m.flags=DkMemBlockFlags_CpuUncached|DkMemBlockFlags_GpuCached;
            impl->pc_command_memory=dkMemBlockCreate(&m);
            DkCmdBufMaker cm;dkCmdBufMakerDefaults(&cm,impl->device);impl->pc_command_buffer=dkCmdBufCreate(&cm);
            DkMemBlockMaker tm;dkMemBlockMakerDefaults(&tm,impl->device,DK_MEMBLOCK_ALIGNMENT);
            tm.flags=DkMemBlockFlags_CpuUncached|DkMemBlockFlags_GpuCached;
            impl->pc_timestamps=dkMemBlockCreate(&tm);                    // optional (profile only)
            if(impl->pc_command_memory&&impl->pc_command_buffer)impl->pc_device=std::move(device);
            else{impl->pc_error="pc command memory";device->shutdown();}
        }else device->shutdown();
    }
    return true;
}
platform::PcD3D9Device* switch_renderer_pc_device(SwitchRenderer& renderer){
    auto* r=static_cast<RendererImpl*>(renderer.impl);return r?r->pc_device.get():nullptr;}
bool switch_renderer_set_pc_scene(SwitchRenderer& renderer,std::function<void(platform::PcD3D9Device&)> scene){
    auto* r=static_cast<RendererImpl*>(renderer.impl);if(!r||(scene&&!r->pc_device))return false;r->pc_scene=std::move(scene);return true;}
void switch_renderer_disable_pc_spec(){g_pc_spec_disabled=true;}
void switch_renderer_cycle_pc_diag(SwitchRenderer& renderer){
    auto* r=static_cast<RendererImpl*>(renderer.impl);if(r&&r->pc_device)r->pc_device->diag_mode=(r->pc_device->diag_mode+1u)%9u;}
std::string switch_renderer_pc_error(const SwitchRenderer& renderer){
    const auto* r=static_cast<const RendererImpl*>(renderer.impl);if(!r)return "no renderer";
    if(!r->pc_error.empty())return r->pc_error;
    if(r->pc_device)return r->pc_device->diagnostics();
    return {};}
bool switch_renderer_set_frontend_visible(SwitchRenderer& renderer,bool visible){auto* r=static_cast<RendererImpl*>(renderer.impl);if(!r)return false;r->frontend_visible=visible;return true;}
bool switch_renderer_set_movie_frame(SwitchRenderer& renderer,const std::uint8_t* rgba,unsigned width,unsigned height){
    auto* r=static_cast<RendererImpl*>(renderer.impl);if(!r)return false;
    if(!rgba){r->movie_visible=false;return true;}
    if(!width||!height||width>1920u||height>1080u)return false;
    dkQueueWaitIdle(r->queue);
    if(!r->movie_image_memory){
        DkImageLayoutMaker lm;dkImageLayoutMakerDefaults(&lm,r->device);
        lm.flags=DkImageFlags_Usage2DEngine;lm.format=DkImageFormat_RGBA8_Unorm;
        lm.dimensions[0]=width;lm.dimensions[1]=height;DkImageLayout layout;dkImageLayoutInitialize(&layout,&lm);
        DkMemBlockMaker mm;dkMemBlockMakerDefaults(&mm,r->device,align_up(dkImageLayoutGetSize(&layout),DK_MEMBLOCK_ALIGNMENT));
        mm.flags=DkMemBlockFlags_GpuCached|DkMemBlockFlags_Image;r->movie_image_memory=dkMemBlockCreate(&mm);
        if(!r->movie_image_memory)return false;
        dkImageInitialize(&r->movie_image,&layout,r->movie_image_memory,0u);
        const auto stride=align_up(width*4u,DK_IMAGE_LINEAR_STRIDE_ALIGNMENT);
        dkMemBlockMakerDefaults(&mm,r->device,align_up(stride*height,DK_MEMBLOCK_ALIGNMENT));
        mm.flags=DkMemBlockFlags_CpuUncached|DkMemBlockFlags_GpuCached;r->movie_upload_memory=dkMemBlockCreate(&mm);
        dkMemBlockMakerDefaults(&mm,r->device,65536u);mm.flags=DkMemBlockFlags_CpuUncached|DkMemBlockFlags_GpuCached;
        r->movie_command_memory=dkMemBlockCreate(&mm);
        if(!r->movie_upload_memory||!r->movie_command_memory)return false;
        DkCmdBufMaker cm;dkCmdBufMakerDefaults(&cm,r->device);r->movie_command_buffer=dkCmdBufCreate(&cm);
        if(!r->movie_command_buffer)return false;
        dkCmdBufAddMemory(r->movie_command_buffer,r->movie_command_memory,0u,65536u);
        DkImageView source;dkImageViewDefaults(&source,&r->movie_image);
        const DkImageRect src{0u,0u,0u,width,height,1u};
        const DkCopyBuf upload{dkMemBlockGetGpuAddr(r->movie_upload_memory),stride,0u};
        // Same 640x480 authored canvas as frontend sprites. No UV mirroring.
        const DkImageRect dst{208u,36u,0u,864u,648u,1u};
        for(unsigned slot=0;slot<FramebufferCount;++slot){
            dkCmdBufBarrier(r->movie_command_buffer,DkBarrier_Full,DkInvalidateFlags_L2Cache);
            dkCmdBufCopyBufferToImage(r->movie_command_buffer,&upload,&source,&src,0u);
            dkCmdBufBarrier(r->movie_command_buffer,DkBarrier_Full,0u);
            DkImageView destination;dkImageViewDefaults(&destination,&r->framebuffers[slot]);
            dkCmdBufBlitImage(r->movie_command_buffer,&source,&src,&destination,&dst,DkBlitFlag_FilterLinear,0u);
            dkCmdBufBarrier(r->movie_command_buffer,DkBarrier_Full,0u);
            r->movie_commands[slot]=dkCmdBufFinishList(r->movie_command_buffer);
        }
        r->movie_width=width;r->movie_height=height;
    }
    if(width!=r->movie_width||height!=r->movie_height||!r->movie_upload_memory)return false;
    const auto stride=align_up(width*4u,DK_IMAGE_LINEAR_STRIDE_ALIGNMENT);
    auto* dst=static_cast<std::uint8_t*>(dkMemBlockGetCpuAddr(r->movie_upload_memory));
    for(unsigned y=0;y<height;++y)std::memcpy(dst+y*stride,rgba+std::size_t(y)*width*4u,width*4u);
    r->movie_visible=true;return true;
}
bool switch_renderer_set_start_loading_visible(SwitchRenderer& renderer,bool visible){auto* r=static_cast<RendererImpl*>(renderer.impl);if(!r||(visible&&r->stats.loading_scenes==0u))return false;r->start_loading_visible=visible;return true;}
bool switch_renderer_set_start_loading_scene(SwitchRenderer& renderer,std::uint32_t scene){auto* r=static_cast<RendererImpl*>(renderer.impl);if(!r||scene>=r->stats.loading_scenes)return false;if(r->loading_scene_offset!=scene)r->loading_frame=0.0f;r->loading_scene_offset=scene;return true;}
bool switch_renderer_set_frontend_token(SwitchRenderer& renderer,std::uint32_t token){auto* r=static_cast<RendererImpl*>(renderer.impl);if(!r)return false;if(!r->primary_frontend_scenes)return true;for(std::size_t i=0;i<r->primary_frontend_scenes;++i)if(r->frontend.scenes[i].token==token){if(r->frontend_scene_index!=i){++r->stats.frontend_scene_changes;r->frontend_frame=0.0f;r->stats.frontend_animation_complete=0u;}r->frontend_scene_index=i;initialize_frontend_stats(*r);return true;}return false;}
bool switch_renderer_set_frontend_overlay(SwitchRenderer& renderer,std::uint32_t token,float frame){
    auto* r=static_cast<RendererImpl*>(renderer.impl);
    if(!r||!std::isfinite(frame)||frame<0.0f)return false;
    if(!r->primary_frontend_scenes)return true;
    if(token!=0u&&!platform::find_frontend_preview_scene(r->frontend,token))return false;
    r->frontend_overlay_token=token;r->frontend_overlay_frame=frame;return true;
}
bool update_frontend_animation(RendererImpl& r,std::size_t scene_index,bool loading,
                               float requested_frame=-1.0f,const platform::FrontendSprite* instance=nullptr){
    const auto& scene=r.frontend.scenes[scene_index];
    const bool shared=(scene.token>>16u)==0x2cu;
    const auto& animation=shared?r.menu.shared:(loading?r.loading_animation:r.frontend_animation);
    const auto texture_base=shared?SharedTextureBase:(loading?platform::FrontendPreviewSourceTextureCount:0u);
    float last_frame=0.0f;
    float& cursor=loading?r.loading_frame:r.frontend_frame;
    const float frame=instance?instance->frame:(requested_frame>=0.0f?requested_frame:cursor);
    if(!animation.animation.empty()){
        if(!platform::game_ui_scene_frame_draws(animation,scene.token&0xffffu,frame,
            r.frontend_frame_draws)||r.frontend_frame_draws.size()!=scene.draw_count)return false;
        for(const auto& draw:r.frontend_frame_draws)
            last_frame=std::max(last_frame,float(std::max<int>(0,draw.last_frame)));
        if(instance)for(auto& draw:r.frontend_frame_draws)
            platform::frontend_sprite_transform(draw,*instance,instance->canvas_width,instance->canvas_height);
    }
    // Shared geometry and descriptor pages must not be changed while a prior
    // scene (including a background in the same frame) is still in flight.
    dkQueueWaitIdle(r.queue);
    for(std::size_t local=0;local<scene.draw_count;++local){
        const auto index=scene.first_draw+local;
        const auto source=animation.animation.empty()?r.frontend.draws[index].source_texture:
            r.frontend_frame_draws[local].texture+texture_base;
        const auto texture_slot=frontend_texture_slot(r.frontend,source);
        if(texture_slot>=r.frontend.textures.size())return false;
        DkImageView view;dkImageViewDefaults(&view,&r.textures[r.frontend_texture_base+texture_slot]);
        DkImageDescriptor descriptor;dkImageDescriptorInitialize(&descriptor,&view,false,false);
        std::memcpy(r.data_cpu+r.frontend_descriptor_offset+local*sizeof(descriptor),&descriptor,sizeof(descriptor));
        const auto quad=animation.animation.empty()?make_frontend_quad(r.frontend,r.frontend.draws[index]):
            make_animated_frontend_quad(r.frontend,r.frontend_frame_draws[local],texture_base);
        std::memcpy(r.data_cpu+index*sizeof(quad),quad.data(),sizeof(quad));
    }
    if(requested_frame<0.0f&&!instance){
        if(loading){++r.stats.loading_animation_updates;r.stats.loading_animation_frame=std::min(frame,last_frame);}
        else{++r.stats.frontend_animation_updates;r.stats.frontend_animation_frame=std::min(frame,last_frame);
             r.stats.frontend_animation_last_frame=last_frame;r.stats.frontend_animation_complete=frame>=last_frame;}
        if(cursor<last_frame)cursor+=1.0f;
    }
    return true;
}
bool prepare_frontend_glyphs(RendererImpl& r){
    r.font_layers.fill({});r.stats.font_draws=0;r.stats.menu_image_draws=0;
    if(g_pc_text)return true;
    if(!r.menu.fonts_loaded)return r.menu.glyphs.empty();
    // Geometry and command memory are reused only after prior submissions finish.
    dkQueueWaitIdle(r.queue);
    dkCmdBufClear(r.font_command_buffer);
    for(unsigned layer=0;layer<21;++layer){
        bool any=false;
        for(unsigned i=0;i<r.menu.images.size();++i){const auto& image=r.menu.images[i];
            if(unsigned(std::clamp(int(image.layer),0,20))!=layer)continue;
            platform::FrontendGlyphQuad quad;unsigned texture{};
            if(!platform::frontend_image_quad(image,r.menu.image_bank,quad,texture))return false;
            const unsigned draw=r.menu.font_first_draw+unsigned(r.menu.glyphs.size())+i;
            std::memcpy(r.data_cpu+draw*sizeof(quad),quad.data(),sizeof(quad));
            dkCmdBufBindTexture(r.font_command_buffer,DkStage_Fragment,0u,dkMakeTextureHandle(10+texture,8u));
            dkCmdBufDrawIndexed(r.font_command_buffer,DkPrimitive_Triangles,6u,1u,r.frontend_first_index+draw*6u,0,0u);
            any=true;++r.stats.menu_image_draws;
        }
        for(unsigned i=0;i<r.menu.glyphs.size();++i){const auto& g=r.menu.glyphs[i];
            if(platform::frontend_glyph_layer(g)!=layer)continue;
            platform::FrontendGlyphQuad quad;
            if(g.token>=10||!platform::frontend_glyph_quad(g,r.menu.fonts.textures[g.token],quad))return false;
            std::memcpy(r.data_cpu+(r.menu.font_first_draw+i)*sizeof(quad),quad.data(),sizeof(quad));
            dkCmdBufBindTexture(r.font_command_buffer,DkStage_Fragment,0u,dkMakeTextureHandle(g.token,8u));
            dkCmdBufDrawIndexed(r.font_command_buffer,DkPrimitive_Triangles,6u,1u,
                r.frontend_first_index+(r.menu.font_first_draw+i)*6u,0,0u);
            any=true;++r.stats.font_draws;
        }
        if(any)r.font_layers[layer]=dkCmdBufFinishList(r.font_command_buffer);
    }
    return true;
}
// Render scale / antialiasing: resolve the multisampled scene, blit the
// scaled rectangle to the frame, and continue on the full-resolution frame
// (the 2D layer: HUD, menus, text keep the native resolution).
void finish_scaled_scene(RendererImpl& r){
    auto& s=r.scene_scale;
    if(!s.pending)return;
    s.pending=false;
    auto sc=[&](std::uint32_t v){return v*s.scale/100u;};
    const auto cmd=r.pc_command_buffer;
    if(s.msaa){
        DkMultisampleState one;dkMultisampleStateDefaults(&one);dkCmdBufBindMultisampleState(cmd,&one);
        dkCmdBufBarrier(cmd,DkBarrier_Fragments,0);
        DkImageView ms,res;dkImageViewDefaults(&ms,&r.msaa_colour);dkImageViewDefaults(&res,&r.scaled_target);
        dkCmdBufResolveImage(cmd,&ms,&res);
    }
    dkCmdBufBarrier(cmd,DkBarrier_Fragments,0);
    if(s.fxaa){
        dkCmdBufBarrier(cmd,DkBarrier_Full,DkInvalidateFlags_Image|DkInvalidateFlags_L2Cache);   // scaled_target is sampled next
        const float dst[4]{float(s.fx),0.0f,float(s.fw),float(s.fh)},src[4]{float(sc(s.fx)),0.0f,float(sc(s.fw)),float(sc(s.fh))};
        fxaa_draw(r,cmd,s.slot,dst,src);
        dkCmdBufBarrier(cmd,DkBarrier_Fragments,0);
        r.pc_device->rebind_frame_resources();
    }else{
        DkImageView src,dst;dkImageViewDefaults(&src,&r.scaled_target);dkImageViewDefaults(&dst,&r.framebuffers[s.slot]);
        const DkImageRect from{sc(s.fx),0u,0u,sc(s.fw),sc(s.fh),1u},to{s.fx,0u,0u,s.fw,s.fh,1u};
        dkCmdBufBlitImage(cmd,&src,&from,&dst,&to,DkBlitFlag_FilterLinear,0);
    }
    dkCmdBufBarrier(cmd,DkBarrier_Full,DkInvalidateFlags_Image|DkInvalidateFlags_L2Cache);
    if(g_widescreen)r.pc_device->retarget_back_buffer(&r.framebuffers[s.slot],&r.depth,0u,0u,FramebufferWidth,FramebufferHeight,
        (FramebufferWidth-FramebufferHeight*4u/3u)/2u,FramebufferHeight*4u/3u);
    else r.pc_device->retarget_back_buffer(&r.framebuffers[s.slot],&r.depth,s.fx,0u,s.fw,s.fh,0u,0u);
    ++r.stats.scaled_scene_frames;
}
// Frame clear followed by the ported PC scene (the PC frame renderer clears
// its own viewport; the full clear covers the pillarbox bands).
void submit_clear_and_pc_scene(RendererImpl& r,int slot){
    dkQueueSubmitCommands(r.queue,r.frontend_clear[slot]);
    if(!r.pc_scene||!r.pc_device)return;
    if(r.pc_fence_used[slot]){ProfileScope prof(profile_key(ProfileRenderer,0,ProfPcFence));dkFenceWait(&r.pc_fences[slot],-1);}
    if(r.pc_timestamps&&r.pc_times_valid[slot]){
        const auto* t=static_cast<const std::uint8_t*>(dkMemBlockGetCpuAddr(r.pc_timestamps))+std::size_t(slot)*32u;
        std::uint64_t begin,end;std::memcpy(&begin,t+8,8);std::memcpy(&end,t+16+8,8);
        if(end>begin){const std::uint64_t ns=dkTimestampToNs(end-begin);
            frame_profile().add(profile_key(ProfileRenderer,0,ProfPcGpu),ns*OR2_PROFILE_FREQ()/1000000000u);r.stats.last_gpu_ns=ns;}
    }
    dkCmdBufClear(r.pc_command_buffer);
    dkCmdBufAddMemory(r.pc_command_buffer,r.pc_command_memory,std::uint32_t(slot)*PcCommandMemorySize,PcCommandMemorySize);
    r.pc_device->set_frame_slot(unsigned(slot));
    // Render scale (options.ini): the scene is drawn into the same rectangle
    // scaled down in an offscreen image and blitted (linear filter) to the frame.
    const unsigned scale=std::clamp(g_render_scale_percent,50u,100u);
    {   // runtime antialiasing switch (mode 1 / 2 = MSAA 2x / 4x)
        const unsigned want=g_aa_mode==1u?2u:g_aa_mode==2u?4u:1u;
        const unsigned have=r.msaa_mode==DkMsMode_4x?4u:r.msaa_mode==DkMsMode_2x?2u:1u;
        if(want!=have){dkQueueWaitIdle(r.queue);create_msaa(r,want);++r.stats.msaa_switches;}
    }
    const bool msaa=r.msaa_mode!=DkMsMode_1x;
    const bool fxaa=g_aa_mode==3u&&r.fxaa_ready&&!msaa;
    const bool scaled=scale<100u||msaa||fxaa;   // the multisampled scene is resolved into scaled_target
    std::uint32_t fx,fw;
    if(g_widescreen){fx=0u;fw=FramebufferWidth;}
    else{fx=(FramebufferWidth-FramebufferHeight*4u/3u)/2u;fw=FramebufferHeight*4u/3u;}
    const std::uint32_t fh=FramebufferHeight;
    auto sc=[&](std::uint32_t v){return v*scale/100u;};
    if(msaa)r.pc_device->set_back_buffer(&r.msaa_colour,&r.msaa_depth,r.msaa_mode);
    else r.pc_device->set_back_buffer(scaled?&r.scaled_target:&r.framebuffers[slot],&r.depth);
    if(g_widescreen){
        r.pc_device->begin_frame(r.pc_command_buffer,0u,0u,sc(fw),sc(fh));
        r.pc_device->set_ui_rect(true,sc((FramebufferWidth-FramebufferHeight*4u/3u)/2u),sc(FramebufferHeight*4u/3u));
    }else{
        r.pc_device->begin_frame(r.pc_command_buffer,sc(fx),0u,sc(fw),sc(fh));
        r.pc_device->set_ui_rect(false,0u,0u);
    }
    r.scene_scale={scaled,slot,scale,msaa,fx,fw,fh,fxaa};
    const DkGpuAddr times=r.pc_timestamps?dkMemBlockGetGpuAddr(r.pc_timestamps)+std::uint32_t(slot)*32u:0u;
    if(times)dkCmdBufReportCounter(r.pc_command_buffer,DkCounter_Timestamp,times);
    {ProfileScope prof(profile_key(ProfileRenderer,0,ProfPcRecord));r.pc_scene(*r.pc_device);}
    finish_scaled_scene(r);
    r.pc_device->end_frame();
    if(times){dkCmdBufReportCounter(r.pc_command_buffer,DkCounter_Timestamp,times+16u);r.pc_times_valid[slot]=true;}
    dkQueueSubmitCommands(r.queue,dkCmdBufFinishList(r.pc_command_buffer));
    dkQueueSignalFence(r.queue,&r.pc_fences[slot],false);r.pc_fence_used[slot]=true;
}
void switch_renderer_scene_to_frame(SwitchRenderer& renderer){
    auto* r=static_cast<RendererImpl*>(renderer.impl);
    if(r&&r->pc_device)finish_scaled_scene(*r);
}
bool switch_renderer_draw(SwitchRenderer& renderer){
    auto* r=static_cast<RendererImpl*>(renderer.impl);
    if(!r||!r->queue||!r->swapchain)return false;
    int slot=-1;
    {ProfileScope prof(profile_key(ProfileRenderer,0,ProfAcquire));slot=dkQueueAcquireImage(r->queue,r->swapchain);}
    if(slot<0||slot>=int(FramebufferCount))return false;
    const auto transforms=make_transform_block();
    std::memcpy(r->data_cpu+r->uniform_offsets[slot],&transforms,sizeof(transforms));
    dkQueueSubmitCommands(r->queue,r->bind_framebuffer[slot]);
    if(r->start_loading_visible){
        dkQueueSubmitCommands(r->queue,r->frontend_clear[slot]);
        const auto index=r->loading_scene_index+r->loading_scene_offset;
        if(index>=r->frontend_commands[slot].size())return false;
        if(!update_frontend_animation(*r,index,true))return false;
        dkQueueSubmitCommands(r->queue,r->frontend_commands[slot][index]);
        ++r->stats.loading_frames;
    }else if(r->frontend_visible){
        submit_clear_and_pc_scene(*r,slot);
        if(r->movie_visible){dkQueueSubmitCommands(r->queue,r->movie_commands[slot]);++r->stats.movie_frames;}
        if(r->frontend_sprites){
            if(!prepare_frontend_glyphs(*r))return false;
            r->stats.frontend_instances=0;r->stats.frontend_instance_draws=0;
            const auto& instances=r->frontend_sprites->instances();
            for(unsigned layer=0;layer<platform::FrontendSprites::Layers;++layer){
            if(r->font_layers[layer]){
                dkQueueSubmitCommands(r->queue,r->font_setup[slot]);
                dkQueueSubmitCommands(r->queue,r->font_layers[layer]);
            }
            for(unsigned i=0;i<r->menu.icons.size()&&!g_pc_text;++i){
                if(r->menu.icon_layers[i]!=layer)continue;
                const auto& icon=r->menu.icons[i];std::size_t index=0;
                for(;index<r->frontend.scenes.size();++index)if(r->frontend.scenes[index].token==icon.token)break;
                if(index==r->frontend.scenes.size()||!update_frontend_animation(*r,index,false,icon.frame,&icon))return false;
                dkQueueSubmitCommands(r->queue,r->frontend_commands[slot][index]);
            }
            for(std::uint32_t handle=layer*64;handle<(layer+1)*64&&!g_pc_sprites;++handle){
                const auto& instance=instances[handle];
                if(!instance.allocated||!instance.visible)continue;
                std::size_t index=0;
                for(;index<r->frontend.scenes.size();++index)
                    if(r->frontend.scenes[index].token==instance.token)break;
                if(index==r->frontend.scenes.size()||
                   !update_frontend_animation(*r,index,false,instance.frame,&instance))return false;
                dkQueueSubmitCommands(r->queue,r->frontend_commands[slot][index]);
                ++r->stats.frontend_instances;r->stats.frontend_instance_draws+=r->frontend.scenes[index].draw_count;
                if(index==r->frontend_scene_index){
                    ++r->stats.frontend_animation_updates;r->stats.frontend_animation_frame=instance.frame;
                    r->stats.frontend_animation_last_frame=instance.last;
                    r->stats.frontend_animation_complete=instance.status!=1u;
                }
                r->frontend_sprites->drawn(handle);
            }
            }
            if(r->stats.font_draws)++r->stats.font_frames;
            ++r->stats.frontend_instance_frames;
        }else{
            if(r->frontend_scene_index>=r->frontend_commands[slot].size())return false;
            if(!update_frontend_animation(*r,r->frontend_scene_index,false))return false;
            dkQueueSubmitCommands(r->queue,r->frontend_commands[slot][r->frontend_scene_index]);
        }
        if(r->frontend_overlay_token!=0u){
            std::size_t overlay=0u;
            for(;overlay<r->primary_frontend_scenes;++overlay)
                if(r->frontend.scenes[overlay].token==r->frontend_overlay_token)break;
            if(overlay==r->primary_frontend_scenes||
               !update_frontend_animation(*r,overlay,false,r->frontend_overlay_frame))return false;
            dkQueueSubmitCommands(r->queue,r->frontend_commands[slot][overlay]);
            ++r->stats.frontend_overlay_frames;
            r->stats.frontend_overlay_token=r->frontend_overlay_token;
            r->stats.frontend_overlay_draws=r->frontend.scenes[overlay].draw_count;
        }
        ++r->stats.frontend_frames;
    }else{
        submit_clear_and_pc_scene(*r,slot);++r->stats.unowned_frames;
    }
    {ProfileScope prof(profile_key(ProfileRenderer,0,ProfPresent));dkQueuePresentImage(r->queue,r->swapchain,slot);}
    if(dkQueueIsInErrorState(r->queue))return false;
    ++r->stats.frames;return true;
}
SwitchRendererStats switch_renderer_stats(const SwitchRenderer& renderer){const auto* impl=static_cast<const RendererImpl*>(renderer.impl);return impl?impl->stats:SwitchRendererStats{};}
void switch_renderer_shutdown(SwitchRenderer& renderer){auto* impl=static_cast<RendererImpl*>(renderer.impl);if(!impl)return;destroy_impl(*impl);delete impl;renderer.impl=nullptr;}
#endif

void switch_renderer_set_pc_sprites(SwitchRenderer&,bool on){g_pc_sprites=on;}
void switch_renderer_set_pc_text(SwitchRenderer&,bool on){g_pc_text=on;}
bool switch_renderer_attach_frontend_sprites(SwitchRenderer& renderer,platform::FrontendSprites& sprites){
    auto* r=static_cast<RendererImpl*>(renderer.impl);
    if(!r||(r->frontend_animation.scene_count&&!sprites.bind(0x44u,r->frontend_animation)))return false;
    if(r->menu.shared.scene_count&&!sprites.bind(0x2cu,r->menu.shared))return false;
    r->frontend_sprites=&sprites;return true;
}

const platform::FrontendFontPack* switch_renderer_frontend_fonts(const SwitchRenderer& renderer){
    const auto* r=static_cast<const RendererImpl*>(renderer.impl);
    return r&&r->menu.fonts_loaded?&r->menu.fonts:nullptr;
}
bool switch_renderer_set_frontend_icons(SwitchRenderer& renderer,const std::vector<platform::FrontendWindowIcon>& icons){
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
bool switch_renderer_set_frontend_game_backdrop(SwitchRenderer& renderer,bool visible){
    auto* r=static_cast<RendererImpl*>(renderer.impl);if(!r)return false;r->menu.game_backdrop=visible;return true;
}
bool switch_renderer_set_frontend_glyphs(SwitchRenderer& renderer,const std::vector<platform::FrontendGlyph>& glyphs){
    auto* r=static_cast<RendererImpl*>(renderer.impl);
    if(!r||glyphs.size()+r->menu.images.size()>MaxFontDraws||(!glyphs.empty()&&!r->menu.fonts_loaded))return false;
    platform::FrontendGlyphQuad quad;
    for(const auto& g:glyphs)if(g.token>=10||!platform::frontend_glyph_quad(g,r->menu.fonts.textures[g.token],quad))return false;
    r->menu.glyphs=glyphs;return true;
}
bool switch_renderer_set_frontend_images(SwitchRenderer& renderer,const std::vector<platform::FrontendListImage>& images){
    auto* r=static_cast<RendererImpl*>(renderer.impl);
    if(!r||images.size()+r->menu.glyphs.size()>MaxFontDraws||(!images.empty()&&!r->menu.images_loaded))return false;
    platform::FrontendGlyphQuad quad;unsigned texture{};
    for(const auto& image:images)if(!std::isfinite(image.layer)||double(image.layer)<-2147483648.||double(image.layer)>=2147483648.||
        !platform::frontend_image_quad(image,r->menu.image_bank,quad,texture))return false;
    r->menu.images=images;r->stats.menu_image_draws=unsigned(images.size());return true;
}

} // namespace outrun::switch_runtime
