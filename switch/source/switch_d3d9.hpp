#pragma once
// PcD3D9Device on deko3d: the Direct3D 9 subset the ported PC renderer calls
// (pc_render_flush / pc_pmt_loader), drawn with the generated shader
// ubershaders (pc_vs_uber, pc_ps14_uber, pc_ps11_uber; tools/generate_pc_shaders.py).
//  * buffers: persistently mapped GPU memory (Lock returns the CPU address);
//  * textures: DDS levels per D3DX rules (pc_dds), BC1/2/3 uploaded as is,
//    cube maps as 6-layer images; image/sampler descriptors in growing pools;
//  * vertex input: declaration elements mapped to the v registers declared
//    by the vertex shader (D3DCOLOR as BGRA unorm, UBYTE4 as uscaled);
//  * render states: cull, depth, blend, colour write, depth bias; alpha test
//    and fog in the pixel ubershader epilogue; D3D9 clip-space conventions
//    (y flip, half-pixel offset) in the vertex ubershader.
// Draws are recorded between begin_frame() and end_frame() into the caller's
// command buffer, which already has the render target bound.
#include "platform/pc_d3d9_state.hpp"
#include "platform/pc_dds.hpp"
#include "platform/pc_shader_programs.hpp"
#include <array>
#include <string>
#include <vector>
#ifdef __SWITCH__
#include <deko3d.h>
#endif
namespace outrun::switch_runtime {
// Development switch (autoplay.txt "nospec=1"): every program uses the uber shaders.
inline bool g_pc_spec_disabled=false;
// Bound-state / attribute / sampler caches of SwitchD3D9Device (options.ini state_cache=0 turns them off).
#ifndef OR2_STATE_CACHE_DEFAULT
#define OR2_STATE_CACHE_DEFAULT true
#endif
inline bool g_pc_state_cache=OR2_STATE_CACHE_DEFAULT;
// options.ini decompress_textures=1: BC1/BC2/BC3 files are uploaded as RGBA8 (bisect of
// the narrow-texture corruption seen on console: NOT AVAILABLE label, menu dashes).
inline bool g_pc_decompress_bc=false;
// GPU cost of the stencil draws (car shadow volumes: colour writes off; the
// darkening quad: colour writes on), measured with SamplesPassed reports around
// each draw and shown in the performance overlay. [0] volumes, [1] darkening.
// [2..7]: the draw groups of platform::g_pc_gpu_zone 1..6 (tire marks, particles).
struct PcStencilProbe { double ms[8]{}; std::uint64_t samples[8]{}; unsigned draws[8]{}; };
inline PcStencilProbe g_pc_stencil_probe;
// Fragment shader of the particles' 623830 draws (DrawPrimitiveUP): 0 the
// specialised uber program, 1 pc_particle_lean (full resolution), 2
// pc_particle_half (half-resolution pass with a half-resolution depth,
// composited over the scene, draw_particles_half). Console, camera in the beach
// smoke at 307 MHz: 14.7 ms (1) against 2.9 ms (2), the same image. X + Y
// cycles it on the console (overlay line PM).
inline unsigned g_pc_particle_mode=2;

#ifdef __SWITCH__
class SwitchD3D9Device final:public platform::PcD3D9StateDevice {
public:
    bool initialize(DkDevice device,DkQueue queue,std::string& error);
    void shutdown();
    // The PC screen (pc_w x pc_h, 640x480) is drawn into the framebuffer
    // rectangle (vx, vy, vw, vh); D3D9 viewports are scaled into it.
    // Descriptor sets and uniform-buffer bindings of begin_frame, again (after a
    // pass recorded by the renderer in the same command buffer, e.g. FXAA).
    void rebind_frame_resources();
    void begin_frame(DkCmdBuf cmd,std::uint32_t vx,std::uint32_t vy,std::uint32_t vw,std::uint32_t vh,
                     std::uint32_t pc_w=640,std::uint32_t pc_h=480);
    void clear(std::uint32_t argb,float depth,bool colour,bool zbuffer);
    void end_frame();
    // IDirect3DDevice9 frame calls used by the frame renderer 449050. Only
    // the back buffer and its depth buffer exist; another render target is
    // recorded as an error (no offscreen targets yet).
    void clear(std::uint32_t flags,std::uint32_t colour,float z,std::uint32_t stencil)override;
    void set_viewport(const std::uint32_t v[6])override;
    std::uint32_t get_render_target(std::uint32_t index)override{return index==0u?colour_target_:0u;}
    std::uint32_t get_depth_stencil_surface()override{return depth_target_;}
    // The framebuffer images of the frame begin_frame starts (rebound when the
    // game returns to the back buffer after an offscreen target).
    // ms: the back buffer's sample mode (options.ini antialiasing); offscreen
    // targets (the reflection cube) always render single-sampled.
    void set_back_buffer(const DkImage* colour,const DkImage* depth,DkMsMode ms=DkMsMode_1x){fb_colour_=colour;fb_depth_=depth;fb_ms_=ms;}
    // Render scale / antialiasing: when the 2D layer starts, the back buffer
    // becomes the full-resolution frame (rectangle x,y,w,h, 2D range ui_x/ui_w)
    // with a cleared depth buffer; the game's viewport state is kept.
    void retarget_back_buffer(const DkImage* colour,const DkImage* depth,std::uint32_t x,std::uint32_t y,std::uint32_t w,std::uint32_t h,
                              std::uint32_t ui_x,std::uint32_t ui_w);
    // Pretransformed draws on the back buffer use this x range instead of the frame rectangle.
    void set_ui_rect(bool on,std::uint32_t x,std::uint32_t w){ui_rect_on_=on;ui_x_=x;ui_w_=w;}
    std::uint32_t target_switches{};
    void set_render_target(std::uint32_t index,std::uint32_t surface)override;
    void set_depth_stencil_surface(std::uint32_t surface)override;
    static constexpr std::uint32_t BackBuffer=0xb0000001u,DepthBuffer=0xb0000002u;
    // Occlusion queries (the sun flare 4114E0 / 40CBC0), handles QueryBase + n.
    static constexpr std::uint32_t QueryBase=0xb1000000u,MaxQueries=64u;
    std::uint32_t create_occlusion_query()override;
    void query_issue(std::uint32_t query,std::uint32_t flags)override;
    std::uint32_t query_get_data(std::uint32_t query,std::uint32_t& samples)override;
    std::uint32_t create_vertex_buffer(std::uint32_t,std::uint32_t,std::uint32_t,std::uint32_t)override;
    std::uint32_t create_index_buffer(std::uint32_t,std::uint32_t,std::uint32_t,std::uint32_t)override;
    std::uint8_t* lock(std::uint32_t,std::uint32_t,std::uint32_t,std::uint32_t)override;
    void unlock(std::uint32_t)override{}
    std::uint32_t create_vertex_declaration(const platform::PcVertexElement*)override;
    std::uint32_t create_vertex_shader(const std::uint32_t*)override;
    std::uint32_t create_pixel_shader(const std::uint32_t*)override;
    std::uint32_t create_texture_from_file(const platform::PcTextureFileRequest&)override;
    void release(std::uint32_t)override{}
    std::uint32_t draw_indexed_primitive(std::uint32_t,std::int32_t,std::uint32_t,std::uint32_t,std::uint32_t,std::uint32_t)override;
    // 2D path (pc_sprite_2d): DrawPrimitiveUP of pretransformed FVF vertices
    // (XYZRHW, optional DIFFUSE/SPECULAR, up to 4 two-float texture sets) with
    // the fixed-function stage cascade (pc_ffp_vsh / pc_ffp_fsh). The vertex
    // bytes are copied into a per-frame ring; the draw ignores the D3D
    // viewport transform (positions are PC-screen pixels) and is clipped to
    // the viewport rectangle.
    void draw_primitive_up(std::uint32_t type,std::uint32_t count,const void* data,std::uint32_t stride)override;
    bool texture_level_size(std::uint32_t texture,std::uint32_t level,std::uint32_t& width,std::uint32_t& height)override;
    // ID3DXSprite (42A0A0 image nodes): the software device's model
    // (PcSoftD3DXSprite): each Draw is an immediate pretransformed
    // XYZRHW|DIFFUSE|TEX1 quad through draw_primitive_up, stage 0 MODULATE,
    // cull none and, with D3DXSPRITE_ALPHABLEND, SRCALPHA/INVSRCALPHA unless
    // D3DXSPRITE_DONOTMODIFY_RENDERSTATE; the device states are restored.
    class Sprite final:public platform::PcD3DXSprite {
    public:
        explicit Sprite(SwitchD3D9Device& d):d_(d){}
        void set_transform(const float m[16])override{for(unsigned k=0;k<16;++k)m_[k]=m[k];}
        void begin(std::uint32_t flags)override{flags_=flags;states_.begin(d_,flags);}
        void draw(std::uint32_t texture,const std::int32_t* rect,const float* center,const float* position,std::uint32_t colour)override;
        void end()override{states_.end(d_);}
    private:
        SwitchD3D9Device& d_;std::uint32_t flags_{};
        platform::PcD3DXSpriteStates states_{};
        float m_[16]{1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1};
    };
    platform::PcD3DXSprite* sprite()override{return &sprite_;}
    // Framebuffer slot of the frame begin_frame starts (selects the ring half).
    void set_frame_slot(unsigned slot){slot_=slot&1u;collect_probes();}
    std::uint32_t get_declaration(std::uint32_t,platform::PcVertexElement*)override;
    // Renderer-init objects (404250): textures created empty (the 420B80
    // normalisation cube map, the 4227C0 shadow target), their surfaces and
    // a depth-stencil surface. LockRect returns a CPU copy in the D3D layout;
    // UnlockRect converts it and uploads the level. Render targets other than
    // the back buffer are still recorded as errors by set_render_target.
    std::uint32_t create_cube_texture(std::uint32_t,std::uint32_t,std::uint32_t,std::uint32_t,std::uint32_t)override;
    std::uint32_t create_texture(std::uint32_t,std::uint32_t,std::uint32_t,std::uint32_t,std::uint32_t,std::uint32_t)override;
    std::uint32_t create_depth_stencil_surface(std::uint32_t,std::uint32_t,std::uint32_t,std::uint32_t,std::uint32_t,std::uint32_t)override;
    std::uint32_t get_cube_map_surface(std::uint32_t,std::uint32_t,std::uint32_t)override;
    std::uint32_t get_surface_level(std::uint32_t,std::uint32_t)override;
    std::uint8_t* lock_rect(std::uint32_t,std::uint32_t&)override;
    void unlock_rect(std::uint32_t)override;
    std::vector<std::string> errors;
    std::uint32_t draws{};
    // Console diagnostics of DrawIndexedPrimitive: skip reasons, alpha test
    // without a texture image, and one vertex per draw run through the CPU
    // copy of the vertex shader (clip position on / off screen / invalid).
    struct DipStats { std::uint32_t calls{},no_decl{},no_vs{},no_ib{},drawn{},alpha_no_image{},cpu_on{},cpu_off{},cpu_behind{},cpu_bad{},cpu_unread{},ffp_ps{},spec_vs{},spec_ps{},uber_vs{},uber_ps{},up_ps{},const_bytes{},push_bytes{},push_skipped{}; } dip;
    std::string diagnostics()const;
    // Console diagnostic mode of the uber pixel shaders (0 normal, 1 no alpha
    // test, 2 output alpha, 3 vertex diffuse alpha), cycled with Minus.
    std::uint32_t diag_mode{};
private:
    struct Object {
        std::uint32_t kind{};
        DkGpuAddr gpu{};std::uint8_t* cpu{};std::uint32_t size{};
        std::vector<platform::PcVertexElement> elements;
        platform::PcShaderProgram program;
        std::int32_t spec{-1};                         // specialised shader (spec_), -1 = uber
        // Vertex shader: its input attributes for the last declaration drawn with it.
        std::uint32_t attr_decl{};std::uint32_t attr_streams{};std::array<DkVtxAttribState,16> attr{};
        std::uint32_t image{};bool cube{};
        // Empty textures / surfaces: D3DFORMAT, size, levels; surfaces keep
        // their parent texture, face, level and the locked texels.
        std::uint32_t format{},width{},height{},levels{},parent{},face{},level{};
        std::vector<std::uint8_t> texels;
    };
    // New image (descriptor slot in index); false when memory/pool is full.
    bool create_image(DkImageType type,DkImageFormat format,std::uint32_t w,std::uint32_t h,std::uint32_t layers,
                      std::uint32_t levels,std::uint32_t flags,std::uint32_t& index);
    std::uint32_t empty_texture(bool cube,std::uint32_t w,std::uint32_t h,std::uint32_t levels,std::uint32_t format,std::uint32_t usage);
    std::uint32_t surface_of(std::uint32_t texture,std::uint32_t face,std::uint32_t level);
    Object* object(std::uint32_t h){return h&&h<=objects_.size()?&objects_[h-1]:nullptr;}
    bool allocate(std::uint32_t size,std::uint32_t align,DkGpuAddr& gpu,std::uint8_t*& cpu);
    bool load_shader(DkShader& shader,const char* path,std::string& error);
    std::uint32_t sampler_index(std::uint32_t stage);
    std::uint32_t sampler_lookup(std::uint32_t stage,const std::array<std::uint32_t,9>& key);
    DkDevice device_{};DkQueue queue_{};DkCmdBuf cmd_{};
    DkCmdBuf upload_cmd_{};DkMemBlock upload_cmd_mem_{};
    std::vector<DkMemBlock> data_blocks_;std::uint32_t data_used_{},data_block_size_{};
    std::vector<DkMemBlock> image_blocks_;std::uint32_t image_used_{},image_block_size_{};
    DkMemBlock code_{};std::uint32_t code_used_{};
    DkShader vs_{},ps14_{},ps11_{};
    // Specialised shaders (tools/generate_pc_spec_shaders.py): one per known
    // program; created shaders of other programs use the uber shaders.
    struct Spec { std::uint32_t kind{};std::vector<std::uint32_t> blobs;DkShader shader{}; };
    std::vector<Spec> spec_;
    std::int32_t find_spec(std::uint32_t kind,const std::vector<std::uint32_t>& blobs);
    std::vector<std::uint8_t> ubo_shadow_;
    void push_ubo(std::uint32_t at,std::uint32_t range,std::uint32_t off,std::uint32_t size,const void* data);
    DkMemBlock descriptors_{};std::uint32_t image_count_{},sampler_count_{};
    std::vector<DkImage> images_;
    std::vector<std::array<std::uint32_t,9>> samplers_; // cached sampler state tuples
    DkMemBlock uniforms_{};
    std::vector<Object> objects_;
    std::uint32_t dummy2d_{},dummycube_{};             // opaque black images bound to stages without a texture
    bool solid_image(bool cube,std::uint32_t& index);
    float clip_fix_[4]{};
    std::uint32_t frame_x_{},frame_y_{},frame_w_{1},frame_h_{1},pc_w_{640},pc_h_{480};
    void apply_viewport();
    // Render targets: the back buffer (framebuffer rectangle, PC 640x480
    // scaled into it) or a level surface of a D3DUSAGE_RENDERTARGET texture
    // (a cube face: layer = face), with the back buffer's depth or a
    // depth-stencil surface. Returning to the back buffer issues a fragment
    // barrier and an image cache invalidate so the target can be sampled.
    std::uint32_t colour_target_{BackBuffer},depth_target_{DepthBuffer};
    const DkImage* fb_colour_{};const DkImage* fb_depth_{};DkMsMode fb_ms_{DkMsMode_1x};
    std::uint32_t fb_x_{},fb_y_{},fb_w_{1},fb_h_{1},fb_pc_w_{640},fb_pc_h_{480};
    bool bind_targets();
    bool ui_rect_on_{};std::uint32_t ui_x_{},ui_w_{};
    void apply_render_states();                       // cull, depth, blend, colour write, depth bias
    void push_ps_fixed();                             // alpha test and fog (UboPsFixed)
    DkShader ffp_vs_{},ffp_fs_{};
public:
    std::uint32_t buffer_size(std::uint32_t h)override;
private:
    void bind_programmable(Object* vs,Object* ps);
    const DkShader* bind_pixel_program(Object* ps);
    void bind_pixel_textures();
    DkShader particle_fs_{};                          // pc_particle_lean (g_pc_particle_mode 1)
    DkShader particle_half_fs_{},particle_comp_vs_{},particle_comp_fs_{},particle_depth_fs_{};   // mode 2
    std::uint32_t half_image_=~0u,half_depth_=~0u,depth_desc_=~0u;const DkImage* depth_desc_image_{};
    bool draw_particles_half(DkPrimitive prim,std::uint32_t vertices);
    DkShader ffp_vsps_{};                             // pc_ffp_vsh transform for a bound pixel shader (DrawPrimitiveUP)
    void push_ffp_stages();                           // FfpStages (UboPsProgram) of the fixed-function cascade
    DkShader ffp_vsin_fs_{};                          // the cascade behind pc_vs_uber (SetPixelShader(NULL))
    static constexpr std::uint32_t UpRingSize=4u<<20;  // per frame slot
    DkMemBlock up_ring_{};std::uint32_t up_used_{};unsigned slot_{};
    struct Query { bool begun{},pending{}; std::uint64_t issued{}; std::uint32_t result{}; };
    std::vector<Query> queries_;DkMemBlock reports_{};std::uint64_t frame_serial_{};
    static constexpr unsigned MaxProbes=512u;
    DkMemBlock probes_{};unsigned probe_n_[2]{};std::uint8_t probe_kind_[2][MaxProbes]{};
    int probe_begin();void probe_end(int probe);void collect_probes();
    Sprite sprite_{*this};
    // What the command buffer already has bound: identical state is not bound
    // again (the deko3d state persists in command order). Invalidated when the
    // frame starts and whenever other code records into the command buffer
    // (FXAA pass, back-buffer retarget, the DrawPrimitiveUP bindings).
    struct Bound {
        bool raster{},depth{},colour{},write{},blend{},shaders{},textures{},vertex{},index{};
        std::array<std::uint32_t,5> raster_key{};std::array<std::uint32_t,14> depth_key{};
        std::uint32_t colour_key{},write_key{};std::array<std::uint32_t,3> blend_key{};
        const DkShader* shader[2]{};
        std::array<DkResHandle,12> texture{};
        std::array<DkVtxAttribState,16> attribs{};std::array<DkVtxBufferState,4> bufs{};std::uint32_t buf_count{};
        std::array<DkGpuAddr,4> vb{};std::array<std::uint32_t,4> vb_size{};
        DkGpuAddr ib{};
    } bound_;
    void invalidate_bound(){bound_=Bound{};}
    // sampler_index per stage: the last state tuple and its descriptor index.
    std::array<std::array<std::uint32_t,10>,6> sampler_memo_{};
    std::array<bool,6> sampler_memo_valid_{};
public:
    std::uint32_t binds_skipped{};                    // state binds the cache avoided (diagnostics)
private:
};
#endif
}
