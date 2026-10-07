#pragma once
// PC render-queue flush 405890 and its state machine, ported down to the
// Direct3D 9 call boundary (PcD3D9Device):
//   405890 flush (two passes, world/palette matrices, 404700 draws)
//   404600 pass states, 40AE80 blend state, 404700 draw of one entry,
//   408AF0/408BB0 flush begin/end, 408C80 material (textures, colour lists,
//   408F90 render states, 409430 texture stages, 410DD0 material colours),
//   410680 declaration/vertex shader, 40AF80/40B0C0/40B200 pixel pipeline
//   (combiner block and the pixel shader linker/cache), 40B4B0/40B710/40B760/
//   40B7B0/40B800 combiner entries, 410D00 light colours, 410F90/410FF0/
//   411060/4111C0/411230 transform constants, 40ECC0 texture animation,
//   4499E0 alpha sort.
// The PC globals of this tree live in word arrays at their PC addresses
// (PcRenderGlobals::w, also mapped by the translated modules); the flush
// reads and writes them through the named layouts of pc_render_state.hpp.
// Device object pointers stored there are PcD3D9Device handles.
#include "platform/pc_d3d9.hpp"
#include "platform/pc_pmt_loader.hpp"
#include "platform/pc_render_queue.hpp"
#include "platform/pc_render_state.hpp"
#include <array>
#include <functional>
#include <map>
#include <unordered_map>
#include <vector>
namespace outrun::driving { struct PcMatrixStack; }
namespace outrun::platform {
class PcRenderGlobals {
public:
    struct Range { std::uint32_t base,size; std::vector<std::uint32_t> words; };
    PcRenderGlobals();
    // Word at a PC address inside one of the ranges below (throws outside).
    std::uint32_t& w(std::uint32_t address){
        // Hot path (~50 000 calls per frame, the material code alternates
        // between ranges): one table read gives the range of the word.
        const std::uint32_t off=address-table_base_;
        if(!(address&3u)&&off<table_.size()*4u){
            const auto k=table_[off>>2];
            if(k!=0xffu){auto& r=ranges[k];return r.words[(address-r.base)>>2];}
        }
        return w_search(address);
    }
    std::uint32_t& w_search(std::uint32_t address);
    // Words [address, address+bytes) when they lie in one range (else nullptr).
    std::uint32_t* span(std::uint32_t address,std::uint32_t bytes){
        const std::uint32_t off=address-table_base_;
        if((address&3u)||off>=table_.size()*4u)return nullptr;
        const auto k=table_[off>>2];if(k==0xffu)return nullptr;
        auto& r=ranges[k];if(address-r.base+bytes>r.size)return nullptr;
        return &r.words[(address-r.base)>>2];
    }
    float f(std::uint32_t address);
    void putf(std::uint32_t address,float value);
    std::int32_t i(std::uint32_t address){return std::int32_t(w(address));}
    std::uint8_t byte(std::uint32_t address);
    void put_byte(std::uint32_t address,std::uint8_t value);
    std::array<float,16> matrix(std::uint32_t address);
    void put_matrix(std::uint32_t address,const std::array<float,16>& m);
    // Named views of the modelled ranges (pc_render_state.hpp; same words).
    render_state::PassRecord& pass(std::uint32_t k){return overlay<render_state::PassRecord>(RangePass)[k];}
    render_state::MaterialCache& material(){return *overlay<render_state::MaterialCache>(RangeMaterial);}
    render_state::PixelPipeline& pixel(){return *overlay<render_state::PixelPipeline>(RangePixel);}
    render_state::AnimationState& animation(){return *overlay<render_state::AnimationState>(RangeAnimation);}
    render_state::ReflectionState& reflection(){return *overlay<render_state::ReflectionState>(RangeReflection);}
    render_state::ShaderState& shader(){return *overlay<render_state::ShaderState>(RangeShader);}
    render_state::ViewState& view(){return *overlay<render_state::ViewState>(RangeView);}
    render_state::TransformState& transform(){return *overlay<render_state::TransformState>(RangeTransform);}
    bool pixel_shader_14(){return (ranges[RangeShaderVersion].words[1]&0xffu)!=0;}   // 740C9C byte
    std::vector<Range> ranges;
    // 40ED70: the float blocks of \media\morph_var.dat; animation() table +8 indexes this vector.
    std::vector<float> animation_values;
    std::size_t last_range{};     // w_search(): range of the previous access
    // Word -> range index (0xFF: none) over [table_base_, table_base_ + 4*size), built with the ranges.
    std::vector<std::uint8_t> table_;std::uint32_t table_base_{};
    void build_table();    // Symbolic value of an object record pointer (89A4F8 comparisons).
    std::uint32_t object_key(std::uint32_t resource,std::uint32_t offset);
    std::unordered_map<std::uint64_t,std::uint32_t> object_keys;
    // 404700: vertex size of a declaration (offset + size of its last element);
    // declarations do not change once created (host cache, no PC state).
    std::unordered_map<std::uint32_t,std::uint32_t> declaration_sizes;
    std::uint64_t object_key_last{~0ull};std::uint32_t object_key_last_id{};   // most draws repeat the previous object
    // 410DD0 stack vector reused as pixel constant 2 (xyz of the last lit
    // light; uninitialised on the PC when no light is enabled).
    std::array<float,4> stack_410dd0{};
    // Pixel shader cache 95AEEC (list of {Adler-32 of 89BC28[0x108], shader}).
    struct PixelShader { std::uint32_t hash{},shader{}; };
    std::vector<PixelShader> pixel_shaders_95aeec;
    // Last 40B200 combiner block and its Adler-32 (host cache, no PC state).
    // The last combiner blocks 40B200 hashed and their Adler-32 (host cache,
    // no PC state: the draws alternate between a few blocks).
    struct PsBlock { std::array<std::uint8_t,0x108> block{}; std::uint32_t hash{}; bool valid{}; };
    std::array<PsBlock,8> ps_blocks{};std::uint32_t ps_block_next{};
private:
    // Index in `ranges` of each named range (constructor order).
    enum : std::size_t { RangePass=0,RangeMaterial=1,RangePixel=3,RangeAnimation=4,RangeReflection=5,RangeShader=6,
                         RangeView=7,RangeTransform=8,RangeShaderVersion=9 };
    template<class T> T* overlay(std::size_t k){return reinterpret_cast<T*>(ranges[k].words.data());}
};
// Loaded model data the flush reads through entry fields.
struct PcFlushBank {
    PcPmtResources* resources{};  // system section (+ device handles), texture table
};
struct PcFlushContext {
    PcD3D9Device& device;
    PcRenderGlobals& g;
    // 448810: loaded bank of a resource ID (nullptr when not loaded).
    std::function<PcPmtResources*(std::uint32_t resource)> bank;
    // 408C80 colour lists: alternative texture ID for (list, key, index);
    // false when the list has no entry for key.
    std::function<bool(std::uint32_t list,std::uint32_t key,std::uint32_t index,std::uint32_t& alt)> colour_alt;
    std::uint32_t queue_tag{};    // identifies the flushed queue's matrix pool
};
// 408C80 colour lists of the EXE (64D120 per car model: NULL-terminated
// pointer lists of {texture ID, alternatives...}): entry[index] for key.
bool pc_colour_list_alt_408c80(std::uint32_t list,std::uint32_t key,std::uint32_t index,std::uint32_t& alt);
// 4499E0: quick sort of entry indices by the depth float (entry +00).
void render_queue_sort_4499e0(const PcRenderQueue&,std::vector<std::uint32_t>& order,std::int32_t lo,std::int32_t hi);
// 405890 over `order` (queue entry indices in the PC pointer-array order).
void render_queue_flush_405890(PcFlushContext&,PcRenderQueue&,const std::vector<std::uint32_t>& order);
// 4052C0: flush the opaque queue (insertion order), undo the 4056D0
// overrides in reverse order, empty the queue and set 8999B0 = 1.
void render_queue_flush_4052c0(PcFlushContext&,PcRenderContext&);
// 410F90: matrix into device slot 95D860+slot*40 and its constant updates.
void render_set_matrix_410f90(PcFlushContext&,const std::array<float,16>& m,std::uint32_t slot);
// The texture-stage pieces of the material code the course passes
// (40BF10 .. 40C8D0) apply directly to the renderer state.
struct PcRenderStageOps {
    PcFlushContext& c;
    void mode_40b800(std::uint32_t stage,std::uint32_t mode);
    void colour_40b710(std::uint32_t stage,std::uint32_t op,std::uint32_t arg);
    void alpha_40b7b0(std::uint32_t stage,std::uint32_t op,std::uint32_t arg);
    void alpha_default3(std::uint32_t stage);                 // inline alpha mode 3 (temp, 0x10301010)
    void constants_40b4b0(std::uint32_t a,std::uint32_t stage,std::uint32_t b);
    void bound(std::uint32_t stage,std::uint32_t value);      // 89B5FC + stage*0x10 (dirty when it changes)
    // 95DED4[stage] = type, then 411230(mode, stage).
    void texture_matrix_411230(std::uint32_t mode,std::uint32_t stage,std::uint32_t type);
    void sampler(std::uint32_t stage,std::uint32_t type,std::uint32_t value);   // get, compare, set
};
// 4116F0 (from 4114E0): object `index` of a loaded bank drawn at once (its node
// matrices multiplied onto the stack and loaded by 411060, 410680 shaders,
// DrawIndexedPrimitive per draw record); false when index >= the object count.
bool pmt_object_draw_4116f0(PcFlushContext&,PcPmtResources& bank,std::uint32_t index,driving::PcMatrixStack&);
// 4044F0 / 404540: pass records 897D30.
void render_pass_record_4044f0(PcRenderGlobals&,std::uint32_t pass,std::uint32_t a1,std::uint32_t a2,std::uint32_t a3,std::uint32_t a4);
// 404540: pass defaults of the current layer (448DA0 = 7D25F0).
void render_pass_defaults_404540(PcRenderGlobals&,std::uint32_t layer_7d25f0);
// Light API: 410740 reset, 410710 ambient 95DC30, 4107A0 add a light
// (0x94-byte record: +00 enabled, +04 kind 1/2/3, +08 diffuse, +28 ..., +50..+68
// attenuation/cone, +90 specular power).
void render_lights_reset_410740(PcFlushContext&);
void render_light_ambient_410710(PcFlushContext&,const std::array<float,4>&);
void render_light_add_4107a0(PcFlushContext&,driving::Bytes desc);
// 40D840 (via 40EB59: byte 79FCCA): when (flags & 3) == 2 and env != -1,
// 40D870 fog render states from 7D3A10+env*0x1C, 40DD70 light colours
// 95DC20/95DC30 and 40DB80 lights 0..4 (899B98+env*0xA0, 89A138/89A1D8 and
// 899D78/899E18 + env*0x140): LightEnable/SetLight and 4107A0 each.
struct PcEnvironmentRenderTables {
    driving::Bytes lights_899b98{nullptr,0}; // 0x960 bytes, 899B98..89A4F8
    driving::Bytes fog_7d3a10{nullptr,0};    // 0x54 bytes, three 0x1C fog records
    std::uint8_t pixel_fog_740c88{};// non-zero: table fog density modes
};
void render_environment_40d840(PcFlushContext&,std::int32_t env,std::uint8_t flags_79fcca,const PcEnvironmentRenderTables&);
// 4089A0: environment matrix 95C018 (inverse of 7D2DA0 for environment 1,
// identity otherwise; D3DX leaves it unchanged for a singular matrix), then
// 408A80 (95BF98).
void render_environment_matrix_4089a0(PcFlushContext&,std::int32_t env,const std::array<float,16>& matrix_7d2da0);
// 40F6B0: renderer globals and constants at device setup.
void render_state_init_40f6b0(PcFlushContext&);
// 408880: render states back to the layer defaults (404540, 404600(0),
// FOGENABLE saved in 89A54C, 408F90 and 409430 of stages 0..2 with 0x22400).
void render_reset_states_408880(PcFlushContext&,std::uint32_t layer_7d25f0);
// 40AC60: pixel pipeline state 89B5B4..89BD48 at device setup (40AF40 fixed
// pixel shaders: 420B00 five into 89B5B8.., the table 73DBE4 six into 89B5CC..).
void render_pixel_state_init_40ac60(PcFlushContext&);
// 420B80: 64x64 normalisation cube map (X8R8G8B8, managed) into out;
// returns the PC HRESULT (0, or 0x80004005 when the cube cannot be created).
std::uint32_t render_normal_cube_420b80(PcFlushContext&,std::uint32_t edge,std::uint32_t& out);
// 404250: renderer initialisation after the device is created (40E4B0).
// Render/texture-stage defaults (incl. 4041E0 alpha test >= 0), 404540,
// 404600(0), 40F6B0, 405160 (the queues), 4083F0 (stage defaults, 40AC60,
// 420B80 cube 89A548, 408880, the matrices 95BF98/95BFD8/95C018), 4227C0
// (shadow depth surface 95AFCC, texture 95AFC8 and its surface 95AFC4) and
// the 8999A0..8999AC / 860D28..860EB8 / 893D20 words.
// Returns the device gaps met (0 on a complete device): bit 0 the cube map
// could not be created (89A548 unchanged, as on the PC), bit 1 the shadow
// texture could not be created (the PC would dereference null in 4227C0; the
// port skips GetSurfaceLevel and reports it instead).
enum : std::uint32_t { RendererInitNoCube=1u, RendererInitNoShadowTexture=2u };
// Host census (tools/host/pc_shader_census): every material of every object
// of a loaded bank through 408C80 after 408AF0 / 404600(0) (textures, stages,
// the 40B0C0 combiner and the 40B200 pixel shader linker), as 404700 sets a
// material before drawing it.
void render_material_census(PcFlushContext&,std::uint32_t resource);
std::uint32_t renderer_init_404250(PcFlushContext&,PcRenderContext& queue,std::uint32_t layer_7d25f0);
}
