#pragma once
// Software Direct3D 9 device (reference renderer): runs the PC renderer's
// device calls on the CPU with the generated shader code, so the ported
// pipeline can be checked visually on the host (the Switch uses deko3d with
// the same shader translation). Rasterisation follows D3D9 rules: pixel
// centres at integer coordinates, top-left fill, clipping to 0<=z<=w,
// clockwise front faces, perspective-correct attributes, alpha test, fog,
// depth test/write, blending and colour write mask. Texture sampling
// approximates the LOD from the texcoord gradients of the stage's own
// texture coordinate set.
#include "platform/pc_d3d9_state.hpp"
#include "platform/pc_dds.hpp"
#include "platform/pc_shader_programs.hpp"
#include <map>
#include <string>
#include <vector>
namespace outrun::platform {
class PcSoftD3D9Device;
// ID3DXSprite on the software device. Host visualisation boundary, not an
// oracle-verified port of D3DX: every draw() is emitted immediately (no
// batching or sorting) as a pretransformed XYZRHW|DIFFUSE|TEX1 quad through
// the device's DrawPrimitiveUP path. Corners (0,0),(w,0),(0,h),(w,h) of the
// source rect (default: whole level 0) minus center, plus position, times the
// transform (row vectors, p' = p*M). The device's current render and sampler
// states are used, except what D3DX Begin sets unless
// D3DXSPRITE_DONOTMODIFY_RENDERSTATE (2): cull none, and with
// D3DXSPRITE_ALPHABLEND (0x10) alpha blending SRCALPHA/INVSRCALPHA. Stage 0
// is MODULATE texture*diffuse for colour and alpha, stage 1 disabled, no
// pixel shader. No -0.5 pixel offset is applied (D3DX9 does not add one).
class PcSoftD3DXSprite final:public PcD3DXSprite {
public:
    explicit PcSoftD3DXSprite(PcSoftD3D9Device& device):device_(device){}
    void set_transform(const float matrix[16])override;
    void begin(std::uint32_t flags)override;
    void draw(std::uint32_t texture,const std::int32_t* rect,const float* center,const float* position,std::uint32_t colour)override;
    void end()override;
    std::uint32_t flags()const{return flags_;}
private:
    PcSoftD3D9Device& device_;
    std::uint32_t flags_{};
    PcD3DXSpriteStates states_{};
    float transform_[16]{1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1};
};
class PcSoftD3D9Device:public PcD3D9StateDevice {
public:
    PcSoftD3D9Device(std::uint32_t width,std::uint32_t height);
    PcSoftD3D9Device(const PcSoftD3D9Device&)=delete;
    PcSoftD3D9Device& operator=(const PcSoftD3D9Device&)=delete;
    std::uint32_t create_vertex_buffer(std::uint32_t,std::uint32_t,std::uint32_t,std::uint32_t)override;
    std::uint32_t create_index_buffer(std::uint32_t,std::uint32_t,std::uint32_t,std::uint32_t)override;
    std::uint8_t* lock(std::uint32_t,std::uint32_t,std::uint32_t,std::uint32_t)override;
    void unlock(std::uint32_t)override{}
    std::uint32_t create_vertex_declaration(const PcVertexElement*)override;
    std::uint32_t create_vertex_shader(const std::uint32_t*)override;
    std::uint32_t create_pixel_shader(const std::uint32_t*)override;
    std::uint32_t create_texture_from_file(const PcTextureFileRequest&)override;
    void add_ref(std::uint32_t)override;
    void release(std::uint32_t)override;
    void set_texture(std::uint32_t,std::uint32_t)override;
    void set_vertex_declaration(std::uint32_t)override;
    void set_vertex_shader(std::uint32_t)override;
    void set_pixel_shader(std::uint32_t)override;
    void set_stream_source(std::uint32_t,std::uint32_t,std::uint32_t,std::uint32_t)override;
    void set_indices(std::uint32_t)override;
    std::size_t live_objects()const;
    std::size_t total_objects()const{return objects_.size();}
    // per kind: created / live (host diagnostics)
    std::string object_kinds()const{std::uint32_t n[16]{},l[16]{};for(const auto& o:objects_){n[o.kind&15u]++;if(o.refs)l[o.kind&15u]++;}
        std::string r;for(unsigned k=0;k<16;++k)if(n[k]||released_kinds[k])r+=" k"+std::to_string(k)+"="+std::to_string(n[k])+"/"+std::to_string(l[k])+"/r"+std::to_string(released_kinds[k]);return r;}
    std::uint32_t released_kinds[16]{};
    std::size_t texture_bytes{};
    std::uint32_t draw_indexed_primitive(std::uint32_t,std::int32_t,std::uint32_t,std::uint32_t,std::uint32_t,std::uint32_t)override;
    std::uint32_t get_declaration(std::uint32_t,PcVertexElement*)override;
    // Frame control (IDirect3DDevice9::Clear / SetViewport equivalents).
    void clear(std::uint32_t argb,float depth);
    void set_viewport(std::uint32_t x,std::uint32_t y,std::uint32_t w,std::uint32_t h,float min_z,float max_z);
    // IDirect3DDevice9 forms used by the frame renderer (8-bit stencil buffer).
    void clear(std::uint32_t flags,std::uint32_t colour,float z,std::uint32_t stencil)override;
    void get_viewport(std::uint32_t out[6])override;
    void set_viewport(const std::uint32_t v[6])override;
    // Render targets: the back buffer, or a level surface of a texture created
    // with D3DUSAGE_RENDERTARGET (the car reflection cube faces); depth: the
    // back buffer's, or a depth-stencil surface. The bound target's pixels live
    // in colour_/depth_/stencil_ (swapped with their owner); SetRenderTarget
    // resets the viewport to the whole target as D3D9 does.
    std::uint32_t get_render_target(std::uint32_t index)override{if(index!=0u)return 0u;add_ref(colour_target_);return colour_target_;}
    std::uint32_t get_depth_stencil_surface()override{add_ref(depth_target_);return depth_target_;}
    void set_render_target(std::uint32_t index,std::uint32_t surface)override;
    void set_depth_stencil_surface(std::uint32_t surface)override;
    std::uint32_t render_target_switches{};
    // 2D path: DrawPrimitiveUP with pretransformed FVF vertices (XYZRHW with
    // optional DIFFUSE/SPECULAR/TEXn; x,y in render-target pixels, z used as
    // depth directly, rhw for perspective-correct attributes; clipped only to
    // the viewport rectangle). Triangle list/strip/fan. Without a pixel shader
    // the fixed-function texture stage cascade is evaluated. XYZ vertices go
    // through the bound vertex shader when there is one (FVF inputs by usage),
    // else through the fixed-function transform.
    void draw_primitive_up(std::uint32_t type,std::uint32_t count,const void* data,std::uint32_t stride)override;
    std::uint32_t fvf()const{return fvf_;}
    std::size_t debug_shaded()const override{return std::size_t(pixels_shaded);}
    std::size_t debug_stencil_nonzero()const override{std::size_t n=0;for(auto v:stencil_)n+=v!=0;return n;}
    std::uint64_t debug_colour_sum()const override{std::uint64_t s=0;for(auto v:colour_)s+=v;return s;}
    std::array<int,4> debug_stencil_box()const override{std::array<int,4> b{int(width_),int(height_),-1,-1};
        for(std::size_t k=0;k<stencil_.size();++k)if(stencil_[k]){const int x=int(k%width_),y=int(k/width_);
            b[0]=std::min(b[0],x);b[1]=std::min(b[1],y);b[2]=std::max(b[2],x);b[3]=std::max(b[3],y);}return b;}
    std::uint32_t buffer_size(std::uint32_t h)override{auto* o=object(h);return o?std::uint32_t(o->bytes.size()):0u;}
    bool texture_level_size(std::uint32_t texture,std::uint32_t level,std::uint32_t& width,std::uint32_t& height)override;
    PcD3DXSprite* sprite()override{return &sprite_;}
    // Renderer-init objects (404250): cube/2D textures created empty,
    // surfaces of their levels, depth-stencil surfaces (no stencil buffer is
    // rasterised; the surface only exists as an object).
    std::uint32_t create_cube_texture(std::uint32_t edge,std::uint32_t levels,std::uint32_t usage,std::uint32_t format,std::uint32_t pool)override;
    std::uint32_t create_texture(std::uint32_t width,std::uint32_t height,std::uint32_t levels,std::uint32_t usage,std::uint32_t format,std::uint32_t pool)override;
    std::uint32_t create_depth_stencil_surface(std::uint32_t width,std::uint32_t height,std::uint32_t format,std::uint32_t,std::uint32_t,std::uint32_t)override;
    std::uint32_t get_cube_map_surface(std::uint32_t cube,std::uint32_t face,std::uint32_t level)override;
    std::uint32_t get_surface_level(std::uint32_t texture,std::uint32_t level)override;
    std::uint8_t* lock_rect(std::uint32_t surface,std::uint32_t& pitch)override;
    void unlock_rect(std::uint32_t surface)override;
    static constexpr std::uint32_t BackBuffer=0xb0000001u,DepthBuffer=0xb0000002u;
    // Occlusion queries (handles QueryBase + n): the samples that pass the
    // depth/stencil tests between BEGIN and END, available at END.
    static constexpr std::uint32_t QueryBase=0xb0001000u;
    std::uint32_t create_occlusion_query()override;
    void query_issue(std::uint32_t query,std::uint32_t flags)override;
    std::uint32_t query_get_data(std::uint32_t query,std::uint32_t& samples)override;
    bool write_png(const std::string& path)const;
    std::uint32_t width()const{return width_;}
    std::uint32_t height()const{return height_;}
    const std::vector<std::uint8_t>& rgba()const{return colour_;}
    std::vector<std::string> errors;
    std::uint64_t pixels_shaded{},triangles_drawn{},pixels_depth_rejected{};
    // Host CPU benchmark (host_nro OR2_HOST_BENCH): draws stop after the state
    // census and clears do nothing, so a frame costs what the console CPU
    // records minus the deko3d calls.
    bool count_only{false};
    // Host lockstep check (OR2_HOST_DRAWHASH): every draw folds the full
    // pipeline state (render/stage/sampler states, textures, streams,
    // shaders, constants, FVF) and its arguments into draw_hash, so two
    // builds can be compared frame by frame.
    bool hash_draws{false};
    std::uint64_t draw_hash{0xcbf29ce484222325ull};
    std::uint64_t draws_hashed{};
    std::uint32_t dump_frame{};
    void hash_draw(std::uint32_t kind,std::initializer_list<std::uint32_t> args);
protected:
    struct Object {
        std::uint32_t refs{1};
        std::uint32_t kind{};
        std::vector<std::uint8_t> bytes;
        std::vector<PcVertexElement> elements;
        PcShaderProgram program;
        PcDdsTexture texture;
        std::vector<std::vector<std::vector<std::uint8_t>>> rgba; // [face][level]
        // Textures created empty: D3DFORMAT. Surfaces: parent texture (0 for
        // a stand-alone depth-stencil surface), face, level, locked texels.
        std::uint32_t format{},parent{},face{},level{},width{},height{};
        std::vector<float> depth;std::vector<std::uint8_t> stencil;   // depth-stencil surface contents
    };
    struct Query { bool active{}; std::uint32_t count{},result{}; };
    std::vector<Query> queries_;std::uint32_t active_queries_{};
    std::map<std::uint64_t,std::uint32_t> surfaces_;   // (texture, face, level) -> its surface (one object, as D3D)
    std::uint32_t colour_target_{BackBuffer},depth_target_{DepthBuffer};
    std::vector<std::uint8_t> back_colour_;std::vector<float> back_depth_;std::vector<std::uint8_t> back_stencil_;
    std::uint32_t back_width_{},back_height_{};
    bool targets_consistent()const{return depth_.size()==std::size_t(width_)*height_&&stencil_.size()==depth_.size();}
    std::uint32_t empty_texture(bool cube,std::uint32_t width,std::uint32_t height,std::uint32_t levels,std::uint32_t format);
    std::uint32_t surface_of(std::uint32_t texture,std::uint32_t face,std::uint32_t level);
    Object* object(std::uint32_t h){return h&&h<=objects_.size()&&objects_[h-1].refs?&objects_[h-1]:nullptr;}
    void rebind(std::uint32_t& slot,std::uint32_t handle);
    struct Vertex { float pos[4]; pc_shader::V4 d[2],t[8];float fog; };
    bool shade_vertex(std::uint32_t index,std::int32_t base,Vertex&);
    void triangle(const Vertex&,const Vertex&,const Vertex&);
    virtual void raster(const Vertex*,unsigned n);
    pc_shader::V4 sample(int stage,const pc_shader::V4& c,float dudx,float dvdx,float dudy,float dvdy);
    pc_shader::V4 fixed_function(const pc_shader::V4& diffuse,const pc_shader::V4& specular,const std::array<pc_shader::V4,8>& tc,
                                 const std::array<pc_shader::V4,8>& tx,const std::array<pc_shader::V4,8>& ty);
    std::uint32_t width_,height_;
    std::vector<std::uint8_t> colour_;
    std::vector<float> depth_;
    std::vector<std::uint8_t> stencil_;
    struct Viewport { float x{},y{},w{},h{},min_z{},max_z{1}; } soft_viewport_{};
    std::vector<Object> objects_;
    const PcShaderProgram* ps_{};
    bool pretransformed_{};     // raster(): Vertex::pos is x,y,z,rhw in pixels
    std::vector<Vertex> up_vertices_; // DrawPrimitiveUP decode buffer (reused)
    PcSoftD3DXSprite sprite_{*this};
};
}
