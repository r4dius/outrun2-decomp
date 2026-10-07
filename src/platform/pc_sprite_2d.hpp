#pragma once
// PC 2D sprite renderer, ported down to the Direct3D 9 / ID3DXSprite boundary.
//
// Producers (called by displays):
//   429460 SPRANI scene renderer: 429350 layer walk (429150/4291D0 blend and
//          matrix push/pop, 429220 layer matrix chain with 48BC00/48B970
//          channel evaluation, 48BCF0/48BCB0 frame windows), 428BD0 footage
//          record (48BBA0 frame, 42A030/42DDF0 texture and size), queued by
//          42D0C0 (sprite node) or stashed as a mask (986B28/986B30/986B34).
//   42D280 raw image, 42D5F0 image group (42C2F0 image lookup), queued by
//          42CFE0 (image node).
//   42DD50 node allocation (pool [[956BEC]], cursor 956BF4, count 95B220,
//          per-layer lists 956C00[0..0x14]).
// Flush 42D710(first, last): per layer 429C60 (states, 408880, sprite Begin),
//   the layer's nodes: sprite nodes 42A3A0 (DrawPrimitiveUP with
//   D3DXVec4Transform corners), image nodes 42A2B0 + 42A0A0 (ID3DXSprite with
//   D3DXMatrixTransformation2D), the 100000/200000 depth/alpha save/restore
//   (render-state shadow 8606F0), then 429F60 (sprite End, states).
//   Not ported (counted, the node is skipped, never replaced): 42A800 (mask
//   draw, record flag 0x40000) and 42C0F0 (text glyph image, flag 0x100).
//
// Memory model: the SPRANI animation images of the banks are relocated at
// PcSpraniAnimationBase(bank) as 429AC0/48BDD0/48BD40/429B00 do on the PC,
// and every pointer the renderer follows is a PC address inside them. The
// queue nodes live at Pc2dNodeBase + index*0x110 (the pool the PC reaches
// through [[956BEC]]), so node links and records keep their PC values.
// An out-of-range PC read (the PC would read unrelated memory or fault) is
// reported through `missing` (the PC function) and `fault` (the address).
#include "platform/pc_d3d9.hpp"
#include "platform/pc_render_flush.hpp"
#include "platform/frontend_sprites.hpp"
#include "driving/pc_matrix_stack.hpp"
#include <array>
#include <cstdint>
#include <map>
#include <string>
#include <vector>
namespace outrun::platform {
constexpr std::uint32_t Pc2dNodeBase=0x5d000000u;       // [[956BEC]] (any free range: node links are compared as values)
constexpr std::uint32_t Pc2dNodeCount=0x230u,Pc2dNodeBytes=0x110u,Pc2dLayerCount=0x15u;
constexpr std::uint32_t Pc2dMaskBase=0x986b34u,Pc2dMaskCount=0x30u,Pc2dRecordBytes=0xb8u;
constexpr std::uint32_t PcSpriteBankCount=0x4bu;
constexpr std::uint32_t PcSpraniAnimationBase(std::uint32_t bank){return 0x40000000u+(bank<<22);}
// One resource bank: the 956D88 + bank*0x2C entry (state 2 = loaded, +4 the
// XST header with its tables, +8 the texture objects) and 9568B8[bank] (the
// relocated SPRANI animation, empty when the bank has none).
struct PcSpriteBank {
    std::uint32_t state{};                   // [+0x00]
    std::vector<std::uint8_t> header;        // [+0x04]: XST header and tables (offsets relative to it)
    std::vector<std::uint32_t> textures;     // [+0x08]: device texture objects
    std::vector<std::uint8_t> animation;     // 9568B8[bank] image at PcSpraniAnimationBase(bank)
};
// Loaders (the resource system's result, not a PC transliteration of the
// async loader): XST = the inflated spr_*.sz file (two size words, the
// 32-byte header {0, meta size, texture count, 0, group count, group offset,
// image count, image offset}, tables, DDS payloads). Textures are created on
// the device from the DDS payloads (D3DX defaults, as the PMT textures).
bool pc_sprite_bank_load_xst(PcSpriteBank&,PcD3D9Device&,const std::vector<std::uint8_t>& inflated,std::string& error);
// ani_*.sz inflated (size word + data): 429AC0 relocation (48BDD0/48BD40)
// and 429B00 (bank << 16 into each footage frame texture word).
bool pc_sprite_bank_load_animation(PcSpriteBank&,std::uint32_t bank,const std::vector<std::uint8_t>& inflated,std::string& error);
struct PcSprite2dState {
    PcSprite2dState();
    // ---- queue ----
    std::vector<std::uint8_t> nodes;                         // Pc2dNodeCount * 0x110
    std::uint32_t cursor_956bf4{},count_95b220{};
    std::array<std::uint32_t,Pc2dLayerCount> heads_956c00{}; // node addresses (0 = empty)
    std::uint32_t update_index_8a8cdc{};                     // nonzero: no allocation (42DD50)
    // ---- SPRANI scene renderer ----
    std::array<float,3> blend_9564e0{};                      // 9564E0/E4/E8
    std::array<float,9> stack_956468{};
    std::array<std::uint32_t,18> stack_956490{};             // {9564E4, 9564E8} pairs
    std::int32_t depth_9564ec{};
    std::uint32_t flag_9564f4{},flag_9564f8{};
    std::uint32_t mask_986b28{},mask_986b2c{},mask_986b30{};
    std::vector<std::uint8_t> masks_986b34;                  // 0x30 * 0xB8
    std::int32_t id_7551b4{-1};
    // ---- flush ----
    std::uint32_t blend_956bf8{},blend_956b94{},blend_956b98{},w956bfc{};
    // Render-state shadow 8606F0 ([893D20]): saved words and dirty bytes the flush uses.
    std::uint32_t shadow_1c{},shadow_38{},shadow_3c{},shadow_5c{},shadow_60{},shadow_64{};
    std::uint8_t shadow_63f{},shadow_646{},shadow_647{},shadow_64f{},shadow_650{},shadow_651{};
    std::array<std::uint8_t,0x800> shadow_8606f0{};              // 42A800 texture/sampler words and dirty bytes
    std::array<std::uint8_t,0x70> vertices_98b868{};          // 4 FVF 0x144 vertices
    float screen_740c94{1.0f},screen_740c98{1.0f};
    bool sprite_95b218{};                                     // [95B218] != 0 (the device's ID3DXSprite)
    // ---- banks ----
    std::array<PcSpriteBank,PcSpriteBankCount> banks;
    // ---- reports ----
    std::uint32_t missing{},fault{};
    std::map<std::uint32_t,std::uint32_t> missing_counts;      // pc -> count (every missing event)
    std::uint32_t sprite_nodes{},image_nodes{},draws{},sprite_draws{};
    std::uint32_t pool_draws{},pool_missing_roots{};
    void clear_report(){missing=0;fault=0;}
    // Node helpers.
    std::uint8_t* node(std::uint32_t address);
    // SPRANI animation reads (PC addresses); false: out of range (fault latched).
    bool anim_ok(std::uint32_t address,std::uint32_t size);
};
// Bank lookups used by the producers (PC pointer values of the tables).
std::uint32_t pc_sprani_scene_root(const PcSprite2dState&,std::uint32_t token); // 9568B8[bank][index] -> last component (0 = none)
// 429460(root component, frame, scale, layer) on the matrix stack top. The
// stack top and blend_9564e0 must hold what the caller's 429010/428EA0 left.
void pc_sprani_render_429460(PcSprite2dState&,PcD3D9Device&,driving::PcMatrixStack&,std::uint32_t root,float frame,float scale,std::uint32_t layer);
// 429010: the component canvas (int16 width/height at root +0/+2) centred
// in the 640x480 screen around the instance matrix, on the stack top:
// top = T(-w/2,-h/2) * M * T(w/2,h/2) * T(320-w/2,240-h/2) * I; then the
// blend stack 9564E0 = {0, 1, 1}.
void pc_sprite_canvas_429010(PcSprite2dState&,driving::PcMatrixStack&,std::uint32_t root,const std::array<float,16>& matrix);
// 428170: the SPRANI instance pool display (event 398, 21 layers x 64):
// every allocated and visible instance is drawn by 429460(root, frame, 1.0,
// its layer) under a pushed 429010 matrix with 7551B4 = its +2C word and
// 986B28 = 0; a mode-4 instance is released after its draw. The root is
// 48BD20 of the scene (pc_sprani_scene_root); an instance whose bank
// animation is not loaded natively has none: the PC would stop the whole
// display there (root 0), natively the instance is counted and skipped.
void pc_sprite_pool_display_428170(PcSprite2dState&,PcD3D9Device&,driving::PcMatrixStack&,FrontendSprites&);
// 42D280(token, x, y, flip mask, layer f32, colour) and
// 42D5F0(token, x, y, layer f32, colour, flags).
void pc_image_42d280(PcSprite2dState&,std::uint32_t token,std::int32_t x,std::int32_t y,std::uint32_t flip,float layer,std::uint32_t colour);
// 42D200: 42D280 with a horizontal scale (the GOAL stage bars of 4979E0).
void pc_image_42d200(PcSprite2dState&,std::uint32_t token,std::int32_t x,std::int32_t y,float scale_x,std::uint32_t flip,float layer,std::uint32_t colour);
void pc_image_group_42d5f0(PcSprite2dState&,std::uint32_t token,std::int32_t x,std::int32_t y,float layer,std::uint32_t colour,std::uint32_t flags);
// 42D300(mode, token, x, y, width, height, ?, layer, colour): a stretched
// image as a 42A3A0 sprite record (42D0C0): the 42C2F0 rectangle, mode 0
// right/bottom, 1 left/bottom, 2 top = bottom, 3 left/top + 3 (jump table
// 42D5B0); u = column / 128.5, v = 1 - row / 64.5 (6281B4 / 6281B0), corners
// (x, y), (x, y+h), (x+w, y), (x+w, y+h); flags 0x45, identity matrix,
// sampler modes 3/3, texture 956D88[bank].+8[index] (0 without the bank).
void pc_image_42d300(PcSprite2dState&,std::uint32_t mode,std::uint32_t token,std::int32_t x,std::int32_t y,float width,float height,float layer,std::uint32_t colour);
// 42CFE0 with a record the caller built (42C860 glyphs: token, left, top,
// right, bottom, scale x/y, x, y, colour; flags 0).
void pc_image_record_42cfe0(PcSprite2dState&,const std::array<std::uint8_t,0x48>& record,float layer);
// 42C2F0(rec, token): the image table entry of a loaded bank into rec +0 (texture
// token), +4/+8/+C/+10 (left, top, right, bottom); nothing when the bank is absent.
void pc_image_entry_42c2f0(PcSprite2dState&,std::uint8_t* rec,std::uint32_t token);
// 42D710(first, last): flush and empty the lists of layers first..last-1.
// `sprite` is the ID3DXSprite of 95B218 (null: [95B218] == 0).
void pc_2d_flush_42d710(PcSprite2dState&,PcFlushContext&,PcD3DXSprite* sprite,std::uint32_t first,std::uint32_t last,std::uint32_t layer_7d25f0);
// D3DX pieces (generic x87 paths of the pinned d3dx9_29.dll).
std::array<float,4> pc_d3dx_vec4_transform(const std::array<float,4>& v,const std::array<float,16>& m);      // 44067A
// 444B56 with pScalingCenter = NULL, ScalingRotation = 0, pRotationCenter = NULL (42A0A0's call).
std::array<float,16> pc_d3dx_transformation_2d(const float scaling[2],float rotation,const float translation[2]);
std::array<float,16> pc_d3dx_rotation_z(float angle);                                                         // 441521
}
