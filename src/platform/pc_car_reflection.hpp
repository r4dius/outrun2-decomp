#pragma once
// PC car reflection cube (8A89F0..8A8BF8): the player car's environment map,
// a 128x128 A8R8G8B8 render-target cube texture [8A89F4] with its D24S8
// depth surface [8A8A00], two or three of its six faces redrawn per frame.
//   413B30 init (bootstrap 41781A): the device objects, VS constant c9 from
//          6222F0, the scene constants 8A8BD4..8A8BF0 and the six face view
//          rotations 8A8A50 + face*0x40 (+X, -X, +Y, -Y, +Z, -Z of D3D).
//   414340 frame layer-0 leaf (449050): while the player car event (8) runs
//          and not (44B7B0 and car +5C): projection and view saved on the
//          matrix stack (409EA0(1), 409EA0(0)), the eye car +B0 * (0, 2, 1.5),
//          the 90 degree projection, the back buffer targets saved, then per
//          face (round robin 8A8BD0) its view, the face surface as target and
//          414050; the face time (449DF0) decides 3 or 2 faces next frame.
//          413FC0 restores the targets, the camera projection is rebuilt and
//          the saved view and projection come back from the stack.
//   414050 face scene: clear (colour 808080, alpha [8A8BD8]*255, z 1,
//          stencil 0), the scene light 899C38 scaled by [8A8BD4], the pass
//          records, 44CD30 (course environment model), 405830, the light
//          again, 404540, VS c10, 451C20 (environment sky) and, with SCN_EFC
//          flag 2, its object 0x57000F aligned to the light direction.
// The renderer globals live in PcFlushContext::g at their PC addresses; the
// course model, the sky and the 4056D0 draw are services of the caller (the
// race AREA owner draws them on the same device and matrix stack).
#include "platform/pc_render_flush.hpp"
#include "platform/pc_render_queue.hpp"
#include "driving/pc_matrix_stack.hpp"
#include <functional>
namespace outrun::platform {
struct PcCarReflectionInputs {
    std::uint8_t player_flags_79fb50{};     // event 8 flags
    std::uint8_t final_stage_7d33d0{};      // 44B7B0(0): byte [7D33D0]
    driving::Bytes car{nullptr,0};          // [799D18]: +04 view mode, +5C, +B0 world matrix
    driving::Bytes camera{nullptr,0};       // [79F574]: +A0 fov, +B8 aspect, +BC/+C0 near/far, +C4/+C8 shift
    driving::Bytes light_899c38{nullptr,0}; // 0xA0-byte scene light record
    driving::Bytes scn_efc{nullptr,0};      // [79F5EC] work: +00 flags, +9C object value (empty: event closed)
    std::uint32_t layer_7d25f0{};           // 404540 layer
};
struct PcCarReflectionServices {
    std::function<void()> env_model_44cd30;
    std::function<void()> env_sky_451c20;
    std::function<void(std::uint32_t object,std::uint32_t value,std::uint32_t colour,std::uint32_t colour_byte)> draw_4056d0;
    std::function<void()> flush_alpha_405830;
    std::function<float()> elapsed_449df0;   // milliseconds (x87 result)
};
// Errors the port met (a service missing, the SCN_EFC work closed) are counted here.
struct PcCarReflectionStats {
    std::uint32_t inits{},frames{},faces{},skipped_closed_scn_efc{};
};
// 413B30 on the device and the renderer globals. The PC builds the face
// matrices in the current stack matrix (leaving RotationY(pi) there at
// bootstrap); `scratch` is the stack it uses here.
void car_reflection_init_413b30(PcFlushContext&,driving::PcMatrixStack& scratch);
// 413F50: releases the cube and the depth surface, clears 8A89F0..8A8BF8.
void car_reflection_release_413f50(PcFlushContext&);
// 414340 / 414050 / 413FC0. view: the renderer's culling copy of 95D860 and
// the 404310 frustum (PcRenderContext::view), kept equal to the globals.
void car_reflection_frame_414340(PcFlushContext&,driving::PcMatrixStack&,PcRenderView& view,
                                 const PcCarReflectionInputs&,const PcCarReflectionServices&,PcCarReflectionStats&);
void car_reflection_scene_414050(PcFlushContext&,driving::PcMatrixStack&,const PcCarReflectionInputs&,
                                 const PcCarReflectionServices&,PcCarReflectionStats&);
void car_reflection_restore_413fc0(PcFlushContext&,std::uint8_t player_flags_79fb50);
// 40A6D0(a, b): current = R * current, R the rotation taking b onto a's
// cross-product axis by their dot product (40A580); the degenerate case
// turns by pi about (b.y, b.z, b.x) when the dot product is negative.
void car_reflection_align_40a6d0(driving::PcMatrixStack&,const driving::CourseProbe& a,const driving::CourseProbe& b);
// 409EA0(slot): push, and the matrix below the new top becomes 95D860+slot*0x40.
void matrix_push_slot_409ea0(driving::PcMatrixStack&,PcRenderGlobals&,std::uint32_t slot);
}
