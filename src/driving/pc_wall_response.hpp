#pragma once
#include "driving/pc_wall_geometry.hpp"
#include <optional>
#include <vector>
namespace outrun::driving {
// Explicit native replacements for 0x7D33BC/count and record+0x14 pointers.
// The serialized pointers in records are intentionally ignored. Descriptor
// views are indexed by record, NOT by the requested course id.
struct PcStageViews {
    Bytes records;
    std::int32_t count;
    std::vector<Bytes> descriptors;
    Bytes fallback_descriptor;
};
std::optional<std::size_t> pc_find_stage_record(const PcStageViews&,std::uint32_t id); // 0x44C8D0
std::int32_t pc_stage_number(const PcStageViews&,std::uint32_t id); // 0x44DC50
struct WallResponseContext {
    PcStageViews stages;
    Bytes crush_ranges;    // PC root 0x5E0F20: stage * 0x40, 16 signed endpoint pairs
    Bytes friction_ranges; // PC root 0x5E1FA0
    std::uint8_t level;    // explicit value read by 0x456D60 from 0x7DE418
    float rotation_odd=2608.0f;   // original mutable global 0x5E302C
    float rotation_even=-2608.0f;// original mutable global 0x5E3030
};
bool check_crush_entrapment_length(Bytes event,std::uint16_t position,const WallResponseContext&); // 0x5036C0
bool check_friction_entrapment_length(Bytes event,std::uint16_t position,const WallResponseContext&); // 0x503720
void set_collision_timer(Bytes event,std::int32_t mode,std::uint8_t level); // 0x4A47F0
// Complete PC 0x503BF0, not the parent CbwColiWall. The caller must supply the
// already selected wall normal at work+0x64C and the original float angle argument.
// Event, work, stack and context tables must be disjoint. All required views
// are validated before the first write. No x86 pointer in event/work is used.
void calc_friction_status(Bytes event,Bytes work,float angle,PcMatrixStack&,const WallResponseContext&);
// PC 0x503380, integer heading adjustment used by CwReboundStatus.
void calc_rebound_heading(Bytes event,std::int16_t difference);
}
