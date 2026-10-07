#pragma once
#include "driving/pc_camera.hpp"
#include "platform/mesh_preview_pack.hpp"
#include <array>
namespace outrun::platform {
// Converts the PC camera slots (95D860[0] view, [1] projection, Direct3D
// row-vector convention) into the renderer's column-major clip transform.
// D3D computes clip = v * View * Proj; the renderer computes clip = A * v, so
// A in column-major storage is exactly View*Proj in D3D row-major storage.
// Both use z in [0,1] (PerspectiveOffCenterRH, deko3d DepthZeroToOne) and +Y
// up in NDC. The product uses the ported D3DXMatrixMultiply order.
bool camera_scene_transform(const std::array<std::uint8_t,64>& view,
    const std::array<std::uint8_t,64>& projection,MeshPreviewTransform& out);
// Same, prefixed by an object world matrix (D3D row-vector): World*View*Proj.
bool camera_object_transform(const std::array<std::uint8_t,64>& world,
    const std::array<std::uint8_t,64>& view,const std::array<std::uint8_t,64>& projection,
    MeshPreviewTransform& out);

// Event8 display 49F500 -> 46C140 as a draw command: 46AE70(car, model +11, 0)
// under world matrix +B0, with material slot 2's colour scaled by +58 for the
// draw and restored afterwards.
struct PcVehicleDisplay {
    std::int8_t model{};
    std::array<std::uint8_t,64> world{};
    float colour_scale{};
};
PcVehicleDisplay vehicle_display_46c140(driving::Bytes car);
}
