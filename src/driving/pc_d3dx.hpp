#pragma once
#include "driving/pc_driving.hpp"
#include <array>
namespace outrun::driving {
// Generic x87 paths of the pinned d3dx9_29.dll used by the PC camera.
// Operation order and the float stores between x87 steps follow the DLL.
// All inputs are read before any output is written, so in-place calls are
// valid (the PC calls D3DXMatrixInverse(current, NULL, current)).

// D3DXMatrixInverse. Returns false (the DLL's NULL result) when the
// determinant is zero or 1/det is not finite; out is then left untouched.
// det, when non-null, receives the float determinant before that test.
bool pc_d3dx_matrix_inverse(Bytes out,float* det,Bytes in);
using PcMatrix16=std::array<float,16>;
using PcVec3=std::array<float,3>;
// D3DXMatrixMultiply (generic 440943): a*b, row-vector convention.
// Element r*4+c is ((p[o0]+p[o1])+p[o2])+p[o3] with p[k] = a[r][k]*b[k][c]
// and o = PcD3dxMultiplyOrder[r*4+c]: the DLL sums every element in its own
// order (read from 440962..440B7A).
inline constexpr unsigned char PcD3dxMultiplyOrder[16][4]={{3,0,2,1},{1,3,2,0},{3,0,2,1},{3,2,0,1},{3,0,1,2},{2,3,1,0},{1,3,0,2},{1,3,2,0},
    {3,0,1,2},{3,2,1,0},{1,0,3,2},{3,2,1,0},{3,2,0,1},{3,1,2,0},{2,1,0,3},{3,1,0,2}};
PcMatrix16 pc_d3dx_matrix_multiply(const PcMatrix16& a,const PcMatrix16& b);
// D3DXMatrixTranspose.
PcMatrix16 pc_d3dx_matrix_transpose(const PcMatrix16& m);
// D3DXMatrixInverse on arrays; false leaves out unchanged.
bool pc_d3dx_matrix_inverse(PcMatrix16& out,const PcMatrix16& in);
// D3DXVec3TransformCoord (generic 443AA8), including the division by w
// unless w is 1 within FLT_EPSILON (w = 0 divides as the x87 does).
PcVec3 pc_d3dx_vec3_transform_coord(const PcVec3& v,const PcMatrix16& m);
// D3DXVec3TransformNormal (generic 440070).
PcVec3 pc_d3dx_vec3_transform_normal(const PcVec3& v,const PcMatrix16& m);
// D3DXVec3Normalize (generic 4438C4).
PcVec3 pc_d3dx_vec3_normalize(const PcVec3& v);
// D3DXMatrixOrthoOffCenterRH (d3dx9_29 44220C).
void pc_d3dx_ortho_off_center_rh(Bytes out,float left,float right,float bottom,float top,float zn,float zf);
// D3DXMatrixPerspectiveOffCenterRH.
void pc_d3dx_perspective_off_center_rh(Bytes out,float left,float right,
    float bottom,float top,float near_z,float far_z);
// D3DXMatrixScaling (generic 441288): sx/sy/sz are moved bit for bit, the
// other words are 0 and m[15] = 1.
void pc_d3dx_matrix_scaling(Bytes out,float sx,float sy,float sz);
// D3DXMatrixPerspectiveFovRH (generic 441E96): FSINCOS of the float half
// angle, both results stored as floats; zf/(zn-zf) kept in a register.
void pc_d3dx_perspective_fov_rh(Bytes out,float fovy,float aspect,float zn,float zf);
// D3DXMatrixLookAtRH (generic 441AF6, normalization through the generic
// 4438C4). Every input is read before out is written (the PC passes stack
// locals and the matrix stack top, which never alias).
void pc_d3dx_look_at_rh(Bytes out,const PcVec3& eye,const PcVec3& at,const PcVec3& up);
}
