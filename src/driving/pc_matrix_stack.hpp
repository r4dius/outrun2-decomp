#pragma once
#include <stdexcept>
#include "driving/pc_collision.hpp"
#include <cstddef>
namespace outrun::driving {
// Depth and current pointer are separate in the original. Overflow/underflow
// can desynchronise them. Offsets refer only to this explicit native arena.
struct PcMatrixStack {
    Bytes storage;
    std::ptrdiff_t current_offset=0;
    std::int32_t depth=0;
    std::int32_t capacity=0;
    Bytes current() const;
};
CourseProbe pc_matrix_translation(const PcMatrixStack&); // PC 0x40A250
void pc_matrix_set_translation(PcMatrixStack&,const CourseProbe&); // PC 0x40A270
void pc_matrix_push(PcMatrixStack&);                  // PC 0x409EF0
void pc_matrix_push_unit(PcMatrixStack&);             // PC 0x409F30 / mxPushUnitMatrix
void pc_matrix_push_load(PcMatrixStack&,Bytes);       // PC 0x409F90
void pc_matrix_pop(PcMatrixStack&);                   // PC 0x40A010
void pc_matrix_load(PcMatrixStack&,Bytes);            // PC 0x40A170
// PC 0x40A190: copy the 3x3 orientation, zero +0x0c/+0x1c/+0x2c,
// preserve ALL four translation-row words. Not a full matrix load/inverse.
void pc_matrix_load_rotation(PcMatrixStack&,Bytes);
CourseProbe pc_matrix_inverse_vector(const PcMatrixStack&,const CourseProbe&); // 0x40A8A0
CourseProbe pc_inverse_vector(Bytes,const CourseProbe&);
CourseProbe pc_matrix_inverse_point(const PcMatrixStack&,const CourseProbe&); // 0x40A840
CourseProbe pc_matrix_point(const PcMatrixStack&,const CourseProbe&);         // 0x40A7D0
CourseProbe pc_matrix_vector(const PcMatrixStack&,const CourseProbe&);        // 0x40A820
CourseProbe pc_normalize_vector_40ef00(CourseProbe); // D3DX29 generic path
long double pc_unit_vector_40eeb0(CourseProbe&); // original x87 length + in-place normalization
CourseProbe pc_inverse_point(Bytes,const CourseProbe&);
CourseProbe pc_transform_point(Bytes,const CourseProbe&);
}
namespace outrun::driving {
void pc_matrix_identity(PcMatrixStack&); // 0x40A020
// PC mxUnitRotation 0x40A0A0: reset only the 3x3 orientation.  The three
// row-w words and the complete translation row are intentionally preserved.
void pc_matrix_unit_rotation(PcMatrixStack&);
// PC mxGetMatrix 0x40A0D0 non-null path.  Forward REP MOVSD preserves the
// original overlap behavior; the null-pointer return path is represented by
// PcMatrixStack::current() rather than by fabricating a guest pointer.
void pc_matrix_get(const PcMatrixStack&,Bytes output);
// Store the nine orientation words in PC forward-write order. Other words in
// output are untouched; overlapping views have the original propagation.
void pc_matrix_store_rotation(const PcMatrixStack&,Bytes output); // 0x40A100
void pc_matrix_multiply_current(PcMatrixStack&,Bytes left); // 0x40A220; left * current
void pc_matrix_rotate_x(PcMatrixStack&,float radians); // 0x40A3E0; generic D3DX29 path
void pc_matrix_rotate_y(PcMatrixStack&,float radians); // 0x40A410; generic D3DX29 path
void pc_matrix_rotate_z(PcMatrixStack&,float radians); // 0x40A440; generic D3DX29 path
void pc_matrix_rotate_axis(PcMatrixStack&,const CourseProbe& axis,float radians); // 0x40A550 / mxRotateAxe
void pc_matrix_translate_vector(PcMatrixStack&,const CourseProbe&); // 0x40A2D0 / mxTranslateV
}
