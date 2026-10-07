#pragma once
#include "driving/pc_matrix_stack.hpp"
#include <array>
#include <functional>
#include <vector>
namespace outrun::platform {
// A loaded PMT bank (PC 7C2800 + id*0x48). The PC loader 42E4F0 relocates
// the 0x3C-byte object records by adding the system base to fields +04..+38;
// natively those fields stay system-section offsets. Objects start at +0x18.
struct PcModelBank {
    std::uint32_t resource{};
    driving::Bytes system{nullptr,0};
    std::uint32_t object_count{};
};
// Render queue (PC 864EF8 opaque / 8612D8 alpha, reset by 405160).
// Entries are the first 15 dwords of the 0x48 draw descriptor. Pointer
// fields are symbolic: +04 matrix pool index, +10 resource ID, +14 object
// record offset, +18 node offset, +1C mesh entry offset, +24 colour list ID,
// +3C queue (0 opaque, 1 alpha). +20 and +30 are never written by the PC
// (stack contents) and stay 0 here.
struct PcRenderQueue {
    std::uint32_t count{},capacity{},matrix_count{},matrix_capacity{};
    std::vector<std::array<std::uint32_t,15>> entries;
    std::vector<std::array<std::uint8_t,64>> matrices;
    // Opaque queue +1C: 4056D0 material overrides (899598 list, at most 0x80),
    // undone in reverse order by 4052C0 after the flush.
    struct Undo { std::uint32_t resource{},offset{},value{}; };
    std::vector<Undo> undo;
};
void render_queues_reset_405160(PcRenderQueue& opaque,PcRenderQueue& alpha);
struct PcRenderView {
    std::array<std::uint8_t,64> view_95d860{};    // camera view slot 0
    std::array<float,6> frustum_95bf40{};         // l, r, b, t, near, far
    std::array<std::array<float,4>,4> planes_95bf58{};
    std::array<float,3> eye{},target{};           // camera 79F574 +F8 / +104
};
struct PcRenderContext {
    PcRenderQueue& opaque;
    PcRenderQueue& alpha;
    const PcRenderView& view;
    driving::PcMatrixStack& matrices;
    std::function<const PcModelBank*(std::uint32_t)> bank; // 448810
    std::int32_t lod_threshold_8999b8{};
    std::array<std::uint32_t,64> flag_stack_899560{};
    std::int32_t flag_depth_8999a0{};
    // 8999B0: non-zero = every 405360 is drawn at once (4052C0 after the
    // walk); 4052B0 clears it to queue until the next 4052C0.
    std::uint32_t immediate_8999b0{};
    std::function<void()> flush_4052c0; // owner's 4052C0 (render_queue_flush_4052c0)
    std::uint32_t morph_weight_95aecc{};  // 406800: weight word of the next 405450 draws
};
// PC 405360: builds the
// descriptor and walks the object's node tree into the selected queue.
// node_list: optional u16 node list terminated by 0xFFFF (4050F0).
void render_object_405360(PcRenderContext&,std::uint32_t object_id,std::int32_t opaque,
    driving::Bytes node_list,std::uint32_t colour,std::int32_t colour_byte,std::int32_t flag);
// PC 4056D0: sets +0C of every colour record (object +38, stride 0x48) to
// value, recording the old values in opaque.undo (skipped entirely when the
// list would reach 0x80), then 405360(object, 1, none, colour, colour_byte, 0).
void render_object_override_4056d0(PcRenderContext&,std::uint32_t object_id,std::uint32_t value,
    std::uint32_t colour,std::int32_t colour_byte);
// PC 405450(a, b, opaque, colour, colour byte): object a drawn with object
// b as its morph target (descriptor +2C = b, +30 = weight 95AECC); a and b
// must share the bank (the only case the robots use).
void render_object_morph_405450(PcRenderContext&,std::uint32_t a,std::uint32_t b,std::int32_t opaque,
    std::uint32_t colour,std::int32_t colour_byte);
// PC 405580(object, opaque, colour, colour byte, palette, count): count
// matrices appended to the queue's pool (skipped when the pool is full),
// the walk under palette[0] with descriptor +08 = count (404A80 palette).
void render_object_palette_405580(PcRenderContext&,std::uint32_t object_id,std::int32_t opaque,
    std::uint32_t colour,std::int32_t colour_byte,const std::uint8_t* palette,std::uint32_t count);
// PC 4052B0: queue mode.
inline void render_queue_mode_4052b0(PcRenderContext& c){c.immediate_8999b0=0;}
// PC 40A580: rotation about the unit axis by (sin, cos), current = R * current.
void render_rotate_axis_40a580(driving::PcMatrixStack&,const driving::CourseProbe& axis,float sn,float cs);
// PC 404B00: visibility and LOD of one node under the current matrix.
bool render_node_visible_404b00(PcRenderContext&,driving::Bytes node,driving::CourseProbe& view_centre,
    std::int32_t& lod);
}
