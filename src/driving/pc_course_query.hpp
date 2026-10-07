#pragma once
#include "driving/pc_course_topology.hpp"
#include "driving/pc_course_spline.hpp"
#include "driving/pc_matrix_stack.hpp"
namespace outrun::driving {
// All roots are selected, bounded native views. Serialized guest pointers,
// including those in a PC header, are never followed by this code.
struct CourseCollisionTables {
    CourseRunTables runs;
    Bytes kinds;       // 0x780100[type], one byte per polygon; shift count modulo 32
    Bytes polygons;    // 0x780110[type], 0x40-byte records
    Bytes normals;     // 0x780120[type], 0x30-byte records
    Bytes area_lists;  // 0x7801f8[type], u16 count followed by u16 polygon indices
    std::uint32_t load_type=0;
    bool polygons_present=true;
};
// PC 0x43D6E0, including its original protected header-load preamble in the oracle.
// mode 101/103 follows spline flag runs. Other modes deliberately retain the
// PC's unaligned u16 lookahead at lengths + 2*index + 1 (not +2).
std::int32_t find_secondary_course_run(const CourseCollisionTables&,std::int16_t length,
                                     std::int32_t mode,std::int32_t& first,std::int32_t& last);
bool coli_get_forward_polygon_number(const CourseCollisionTables&,std::int32_t index,std::int32_t& out); // 0x43DF20
bool coli_get_back_polygon_number(const CourseCollisionTables&,std::int32_t index,std::int32_t& out);    // 0x43E0D0
bool coli_get_left_polygon_number(const CourseCollisionTables&,std::int32_t index,std::int32_t& out);    // 0x43E250
bool coli_get_right_polygon_number(const CourseCollisionTables&,std::int32_t index,std::int32_t& out);   // 0x43E300
// PC 0x43D390. Average/normalize the four 12-byte normal vectors from the
// selected 0x30-byte polygon-normal record, then transform by the selected
// course matrix. A missing normal table returns world up without touching the
// matrix stack.
CourseProbe course_collision_world_normal(const CourseCollisionTables&,std::int32_t index,
                                          PcMatrixStack&,Bytes course_transform);
// PC 43E570: clamp request +8, resolve run and produce center/corners/width.
// Request +0 is table type, +4 secondary mode; output is the full 58-byte view.
bool course_run_geometry_43e570(const CourseCollisionTables&,Bytes request,Bytes output,std::int32_t hint);
bool course_run_length_43e6c0(const CourseCollisionTables&,Bytes request,Bytes output,std::int32_t hint);

struct RoadCondition {
    float y;
    std::int32_t polygon;
    std::uint16_t flags;
};
// PC 0x43E7E0. list_offset is a WORD offset, not a byte offset or a cell number.
// mode is compared for equality to 0x100 / 0x400, not tested as a mask.
// No hit -> {FLT_MAX, -1, 0}. Ties retain the earlier polygon; flag 4 accepts
// the first containing polygon immediately. Results commit only on success;
// malformed native views throw without partially writing caller outputs.
RoadCondition get_road_cond(const CourseCollisionTables&,std::uint16_t list_offset,
                           std::uint32_t mode,float x,float reference_y,float z,
                           CourseSplineTuning tuning={});
}
