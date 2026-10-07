#pragma once
#include <stdexcept>
// The IDirect3DDevice9 (and D3DX loader) subset the PC renderer calls. The
// ported renderer talks to this interface exactly where the PC calls the
// device, so the call sequence can be compared with the original code running
// against the logging fake device (tools/oracle/fake_d3d9.inc), and the Switch
// backend implements the same calls on deko3d.
//
// Objects are 32-bit handles: the PC stores device object pointers in 32-bit
// PMT fields, and the native loader stores these handles in the same fields.
// A handle is never 0; 0 is what the PC code stores when a create call fails.
#include <array>
#include <cstdint>
namespace outrun::platform {
struct PcVertexElement { // D3DVERTEXELEMENT9
    std::uint16_t stream{},offset{};
    std::uint8_t type{},method{},usage{},usage_index{};
};
constexpr unsigned PcMaxFvfDeclSize=65; // MAX_FVF_DECL_SIZE
constexpr std::uint16_t PcDeclEndStream=0xff;
// Arguments of D3DXCreateTextureFromFileInMemoryEx and the cube variant
// (which has one Size instead of Width/Height; height is ignored for cubes).
struct PcTextureFileRequest {
    const std::uint8_t* data{};
    std::uint32_t size{};
    bool cube{};
    std::uint32_t width{},height{},mip_levels{},usage{},format{},pool{},filter{},mip_filter{},colour_key{};
};
// ---- 2D sprite renderer: begin ----
// ID3DXSprite as the 2D queue flush uses it (vtable slot offsets in the
// comments); matrices are row-major D3DXMATRIX, rect is a RECT (4 LONG).
class PcD3DXSprite {
public:
    virtual ~PcD3DXSprite()=default;
    virtual void set_transform(const float matrix[16])=0;                          // +0x14
    virtual void begin(std::uint32_t flags)=0;                                     // +0x20
    virtual void draw(std::uint32_t texture,const std::int32_t* rect,const float* center,
                      const float* position,std::uint32_t colour)=0;               // +0x24
    virtual void end()=0;                                                          // +0x2C
};
// ---- 2D sprite renderer: end ----
class PcD3D9Device {
public:
    virtual ~PcD3D9Device()=default;
    // CreateVertexBuffer / CreateIndexBuffer (vtable 0x68 / 0x6C).
    virtual std::uint32_t create_vertex_buffer(std::uint32_t length,std::uint32_t usage,std::uint32_t fvf,std::uint32_t pool)=0;
    virtual std::uint32_t create_index_buffer(std::uint32_t length,std::uint32_t usage,std::uint32_t format,std::uint32_t pool)=0;
    // IDirect3D{Vertex,Index}Buffer9::Lock / Unlock. Lock returns the mapped
    // bytes [offset, offset+size) or nullptr.
    virtual std::uint8_t* lock(std::uint32_t buffer,std::uint32_t offset,std::uint32_t size,std::uint32_t flags)=0;
    virtual void unlock(std::uint32_t buffer)=0;
    // Size in bytes of a vertex/index buffer (0 when unknown).
    virtual std::uint32_t buffer_size(std::uint32_t buffer){(void)buffer;return 0;}
    virtual std::size_t debug_stencil_nonzero()const{return 0;}
    virtual std::array<int,4> debug_stencil_box()const{return {0,0,-1,-1};}
    virtual std::uint64_t debug_colour_sum()const{return 0;}   // x0,y0,x1,y1 of stencil != 0
    virtual std::size_t debug_shaded()const{return 0;}
    // CreateVertexDeclaration (0x158): elements end with Stream 0xFF.
    virtual std::uint32_t create_vertex_declaration(const PcVertexElement* elements)=0;
    // CreateVertexShader (0x16C): D3D shader tokens ending with 0x0000FFFF.
    virtual std::uint32_t create_vertex_shader(const std::uint32_t* tokens)=0;
    // SetVertexShaderConstantF (0x178).
    virtual void set_vertex_shader_constant_f(std::uint32_t start,const float* data,std::uint32_t vec4_count)=0;
    // D3DXCreate[Cube]TextureFromFileInMemoryEx.
    virtual std::uint32_t create_texture_from_file(const PcTextureFileRequest&)=0;
    // IUnknown::Release.
    virtual void add_ref(std::uint32_t object){(void)object;} // Trace devices have no owned resources.
    virtual void release(std::uint32_t object)=0;
    // Render pipeline state (vtable slot in the comment).
    virtual std::uint32_t get_render_state(std::uint32_t state)=0;                          // 58
    virtual void set_render_state(std::uint32_t state,std::uint32_t value)=0;              // 57
    virtual std::uint32_t get_texture(std::uint32_t stage)=0;                               // 64 (adds a reference)
    virtual void set_texture(std::uint32_t stage,std::uint32_t texture)=0;                 // 65
    virtual std::uint32_t get_texture_stage_state(std::uint32_t stage,std::uint32_t type)=0;// 66
    virtual void set_texture_stage_state(std::uint32_t stage,std::uint32_t type,std::uint32_t value)=0; // 67
    virtual std::uint32_t get_sampler_state(std::uint32_t sampler,std::uint32_t type)=0;    // 68
    virtual void set_sampler_state(std::uint32_t sampler,std::uint32_t type,std::uint32_t value)=0; // 69
    virtual std::uint32_t draw_indexed_primitive(std::uint32_t type,std::int32_t base_vertex,std::uint32_t min_index,
        std::uint32_t vertices,std::uint32_t start_index,std::uint32_t primitives)=0;       // 82, HRESULT
    virtual void set_vertex_declaration(std::uint32_t declaration)=0;                       // 87
    virtual void set_vertex_shader(std::uint32_t shader)=0;                                 // 92
    virtual void set_stream_source(std::uint32_t stream,std::uint32_t buffer,std::uint32_t offset,std::uint32_t stride)=0; // 100
    virtual void set_indices(std::uint32_t buffer)=0;                                       // 104
    virtual std::uint32_t create_pixel_shader(const std::uint32_t* tokens)=0;               // 106
    virtual void set_pixel_shader(std::uint32_t shader)=0;                                  // 107
    virtual void set_pixel_shader_constant_f(std::uint32_t start,const float* data,std::uint32_t vec4_count)=0; // 109
    // IDirect3DVertexDeclaration9::GetDeclaration: copies the elements
    // (including the end marker) and returns their count.
    virtual std::uint32_t get_declaration(std::uint32_t declaration,PcVertexElement* out)=0;
    // Fixed-function lights (SetLight 51 with a D3DLIGHT9 of 26 words,
    // LightEnable 53). The vertex shaders read the lights from constants, so
    // devices may ignore these; oracle devices log them.
    virtual void set_light(std::uint32_t,const std::uint32_t*){}
    virtual void light_enable(std::uint32_t,std::uint32_t){}
    // SetMaterial (49): D3DMATERIAL9 as 17 floats (diffuse, ambient, specular,
    // emissive RGBA, power).
    virtual void set_material(const float material[17]){(void)material;}
    // DrawIndexedPrimitiveUP (84): `count` primitives over the 16/32-bit
    // (format 0x65 / 0x66) indices and `stride`-byte vertices in user memory.
    virtual void draw_indexed_primitive_up(std::uint32_t type,std::uint32_t min_vertex,std::uint32_t vertices,
        std::uint32_t count,const void* indices,std::uint32_t index_format,const void* data,std::uint32_t stride){
        (void)type;(void)min_vertex;(void)vertices;(void)count;(void)indices;(void)index_format;(void)data;(void)stride;
        throw std::logic_error("DrawIndexedPrimitiveUP is not implemented by this device");}
    // Frame targets (the frame renderer 449050 family). Surfaces are handles;
    // GetRenderTarget/GetDepthStencilSurface add a reference released by
    // release(). Viewport: X, Y, Width, Height, MinZ, MaxZ (floats as bits).
    virtual std::uint32_t get_render_target(std::uint32_t){return 0;}              // 38
    virtual void set_render_target(std::uint32_t,std::uint32_t){}                  // 37
    virtual std::uint32_t get_depth_stencil_surface(){return 0;}                   // 40
    virtual void set_depth_stencil_surface(std::uint32_t){}                        // 39
    virtual void clear(std::uint32_t flags,std::uint32_t colour,float z,std::uint32_t stencil){ // 43 (no rects)
        (void)flags;(void)colour;(void)z;(void)stencil;}
    virtual void get_viewport(std::uint32_t out[6]){for(unsigned k=0;k<6;++k)out[k]=viewport_[k];} // 48
    virtual void set_viewport(const std::uint32_t v[6]){for(unsigned k=0;k<6;++k)viewport_[k]=v[k];} // 47
    // ---- 2D sprite renderer (42D710 family, pc_sprite_2d.hpp): begin ----
    // DrawPrimitiveUP (83): `count` primitives read from `stride`-byte vertex
    // records at `data` (the PC's user-memory vertex array).
    virtual void draw_primitive_up(std::uint32_t type,std::uint32_t count,const void* data,std::uint32_t stride){
        (void)type;(void)count;(void)data;(void)stride;}
    virtual void set_fvf(std::uint32_t fvf){(void)fvf;}                                   // 89
    // SetTransform (44): D3DTS_VIEW 2, PROJECTION 3, WORLD 0x100 (row-major, p' = p*M).
    virtual void set_transform(std::uint32_t state,const float matrix[16]){(void)state;(void)matrix;}
    // IDirect3DTexture9::GetLevelDesc (texture vtable +0x44): Width/Height of
    // the level; false when the device does not know the texture.
    virtual bool texture_level_size(std::uint32_t texture,std::uint32_t level,std::uint32_t& width,std::uint32_t& height){
        (void)texture;(void)level;width=height=0;return false;}
    // D3DXCreateSprite on this device (the object 95B218 points to); null when
    // the device has no sprite implementation.
    virtual PcD3DXSprite* sprite(){return nullptr;}
    // ---- 2D sprite renderer: end ----
    // Renderer initialisation 404250 (4083F0 -> 420B80 normalisation cube
    // map, 4227C0 shadow targets). A device that cannot create the object
    // returns 0, which the PC code treats as a failed create.
    virtual std::uint32_t create_cube_texture(std::uint32_t edge,std::uint32_t levels,std::uint32_t usage,
        std::uint32_t format,std::uint32_t pool){(void)edge;(void)levels;(void)usage;(void)format;(void)pool;return 0;} // 25
    virtual std::uint32_t create_texture(std::uint32_t width,std::uint32_t height,std::uint32_t levels,std::uint32_t usage,
        std::uint32_t format,std::uint32_t pool){(void)width;(void)height;(void)levels;(void)usage;(void)format;(void)pool;return 0;} // 23
    virtual std::uint32_t create_depth_stencil_surface(std::uint32_t width,std::uint32_t height,std::uint32_t format,
        std::uint32_t multisample,std::uint32_t quality,std::uint32_t discard){
        (void)width;(void)height;(void)format;(void)multisample;(void)quality;(void)discard;return 0;} // 29
    // IDirect3DCubeTexture9::GetCubeMapSurface (18) / IDirect3DTexture9::
    // GetSurfaceLevel (18): a surface handle holding a reference.
    virtual std::uint32_t get_cube_map_surface(std::uint32_t cube,std::uint32_t face,std::uint32_t level){(void)cube;(void)face;(void)level;return 0;}
    virtual std::uint32_t get_surface_level(std::uint32_t texture,std::uint32_t level){(void)texture;(void)level;return 0;}
    // IDirect3DSurface9::LockRect(whole surface, flags 0) / UnlockRect: the
    // texels in the format's D3D memory layout (X8R8G8B8: B,G,R,X bytes).
    virtual std::uint8_t* lock_rect(std::uint32_t surface,std::uint32_t& pitch){(void)surface;pitch=0;return nullptr;}
    virtual void unlock_rect(std::uint32_t surface){(void)surface;}
    // Occlusion query (CreateQuery(D3DQUERYTYPE_OCCLUSION) at 41776A, 89F680):
    // 0 when the device has none. Issue: D3DISSUE_BEGIN 2 / END 1 (+18).
    // GetData (+1C): 0 (S_OK) with the samples that passed the depth test
    // between the last begin and end (0 before the first issue), 1 (S_FALSE)
    // while the result is pending.
    virtual std::uint32_t create_occlusion_query(){return 0;}
    virtual void query_issue(std::uint32_t query,std::uint32_t flags){(void)query;(void)flags;}
    virtual std::uint32_t query_get_data(std::uint32_t query,std::uint32_t& samples){(void)query;samples=0;return 1;}
protected:
    std::uint32_t viewport_[6]{0,0,640,480,0,0x3f800000u};
};
// D3DX9 ID3DXSprite::Begin without D3DXSPRITE_DONOTMODIFY_RENDERSTATE (2) sets the sprite
// states on the device for the whole Begin..End (42D710 draws its DrawPrimitiveUP records
// between them): stage 0 colour and alpha MODULATE texture * diffuse, coordinate set 0, no
// texture transform, stage 1 disabled, stencil off, cull none, all colour channels written.
// End restores them unless D3DXSPRITE_DONOTSAVESTATE (1).
class PcD3DXSpriteStates {
public:
    void begin(PcD3D9Device& d,std::uint32_t flags){
        saved_=false;
        if(flags&2u)return;
        if(!(flags&1u)){saved_=true;for(std::size_t k=0;k<Count;++k)old_[k]=get(d,List[k]);}
        for(std::size_t k=0;k<Count;++k)set(d,List[k],List[k].value);
    }
    void end(PcD3D9Device& d){
        if(!saved_)return;
        for(std::size_t k=0;k<Count;++k)set(d,List[k],old_[k]);
        saved_=false;
    }
private:
    struct State { int stage; std::uint32_t type,value; };   // stage -1: render state
    static constexpr std::size_t Count=14;
    static constexpr State List[Count]{
        {0,5,2},{0,6,0},{0,4,4},{0,2,2},{0,3,0},{0,1,4},{0,11,0},{0,24,0},{1,4,1},{1,1,1},   // TSS
        {-1,52,0},{-1,22,1},{-1,168,0xf},{-1,8,3}};                                             // STENCILENABLE, CULLMODE, COLORWRITEENABLE, FILLMODE
    static std::uint32_t get(PcD3D9Device& d,const State& s){return s.stage<0?d.get_render_state(s.type):d.get_texture_stage_state(std::uint32_t(s.stage),s.type);}
    static void set(PcD3D9Device& d,const State& s,std::uint32_t v){
        if(get(d,s)==v)return;
        if(s.stage<0)d.set_render_state(s.type,v);else d.set_texture_stage_state(std::uint32_t(s.stage),s.type,v);
    }
    std::uint32_t old_[Count]{};
    bool saved_{};
};
}
