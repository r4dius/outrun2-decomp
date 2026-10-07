#pragma once
#include "driving/pc_matrix_stack.hpp"
#include <array>
#include <vector>
namespace outrun::driving {
// Event181/function0D camera (object at 79F574). Offsets are PC fields:
// +A0/+A4 vertical/horizontal FOV, +AC authored FOV, +B0/+B4 pixel scales,
// +B8..+C8 projection inputs, +D4, +F8 eye, +104 target, +128 angles,
// +140 view, +180 previous view, +1C0 inverse view, +384/+390/+39C/+3A8
// previous angles/eye/target/+D4.

// Screen size globals 740C8C/740C90 (640x480 in the shipped build).
struct PcCameraScreen {float width_740c8c{640.f},height_740c90{480.f};};

// The render-device boundary. 409DF0(n) stores the current matrix in the
// retained transform table 95D860[n] (slot 0 view, 1 projection) and 409E00(n)
// hands it to Direct3D. Natively both are recorded for the Switch renderer;
// the D3D shader-constant work behind them is not reproduced here.
struct PcCameraDevice {
    struct Store {std::uint32_t slot{};std::array<std::uint8_t,64> matrix{};};
    std::vector<Store> stores;              // 409DF0 calls, in order
    std::vector<std::uint32_t> transforms;  // 409E00 calls, in order
    std::array<std::array<std::uint8_t,64>,10> slots_95d860{};
    std::array<float,6> frustum_95bf40{};   // l, r, b, t, near, far
    std::array<std::array<float,4>,4> planes_95bf58{};
};
// 4493E0: frame interpolation factor used by the view builder.
struct PcCameraBlend {
    std::int32_t interpolation_override_82e7d8{};
    std::uint8_t owner_flags_79fb4e{};
    std::int32_t owner_mode_799ca0_1c{}; // [[799CA0]+1C], read only when 79FB4E&3 == 2 (4B5FD0 bridge)
    float frame_blend_634b34{};
};
float camera_blend_4493e0(const PcCameraBlend&);

// D3DXPlaneFromPoints (generic d3dx9_29 path).
std::array<float,4> pc_d3dx_plane_from_points(const CourseProbe& a,const CourseProbe& b,const CourseProbe& c);
// 449580: wrap both angles and their difference to [-pi,pi], then a + d*t.
float camera_angle_lerp_449580(float from,float to,float t);
// 483C10: +A0/+A4 from the authored FOV +AC and the screen aspect.
void camera_fov_483c10(Bytes camera,const PcCameraScreen&);
// 404310 (via 40A980): perspective into the current matrix, frustum globals
// 95BF40..95BF54 and the four side planes 95BF58..95BF97.
void camera_perspective_404310(PcMatrixStack&,float fov,float aspect,float near_z,
    float far_z,float shift_x,float shift_y,PcCameraDevice&);
// 482F20: interpolated view matrix into slot 0 and +140, previous state update.
void camera_view_482f20(Bytes camera,PcMatrixStack&,PcCameraDevice&,float blend);
// 484BD0: projection (slot 1), view, inverse view +1C0, pixel scales +B0/+B4.
void camera_project_484bd0(Bytes camera,PcMatrixStack&,PcCameraDevice&,
    const PcCameraScreen&,const PcCameraBlend&);
// Tables used by 484EE0: the read-only authored tables and their writable
// copies (818FE8 <- 5B4A30, 818BB0 <- 5BA4E0, 813750 <- 5B5080).
struct PcCameraTables {
    Bytes rom_5b4a30{nullptr,0},rom_5ba4e0{nullptr,0},rom_5b5080{nullptr,0};
    Bytes live_818fe8{nullptr,0},live_818bb0{nullptr,0},live_813750{nullptr,0};
};
// 484EE0 (event181/function0D init). steering_82e7ec with the 82E7D8
// override skips the authored FOV/position setup, as on PC.
void camera_init_484ee0(Bytes camera,PcMatrixStack&,PcCameraDevice&,const PcCameraScreen&,
    const PcCameraBlend&,std::int32_t steering_override_82e7ec,PcCameraTables&);
// Inputs of the 485FE0 control path ported so far (head, 482E80, 4A2BA0 and
// the 78026C == 32 frontend branch).
struct PcRaceCameraInputs;
struct PcCameraControl {
    std::uint8_t pause_780248{};      // 43F9C0
    std::int32_t game_mode_78026c{};
    std::int32_t preset_819634{};     // frontend preset (two authored entries)
    std::uint8_t display_79fcc9{};    // 4A2BA0 gate
    Bytes car_799d18{nullptr,0};      // shared car: +04 flags, +11 model, +162, +E9C, +EA0
    Bytes box_5ba4e0{nullptr,0};      // authored table (rom), +0C + model*0x24 in-car box
    const PcRaceCameraInputs* race{}; // mode 16 (race views), see pc_race_camera.hpp
};
// 4A2BA0: car display flags/fade from the camera state.
void camera_car_flags_4a2ba0(Bytes car,Bytes camera,std::uint8_t display_79fcc9,std::int32_t game_mode);
// 482E80: in-car test against the model box, then 4A2BA0.
void camera_car_test_482e80(Bytes camera,const PcCameraControl&);
// 485FE0. Returns false WITHOUT any change for the +94/+98/+9C debug cameras
// (never enabled in the retail game) and for the race modes without race inputs.
bool race_camera_mode_48618d(std::int32_t game_mode_78026c);
bool camera_control_485fe0(Bytes camera,PcMatrixStack&,PcCameraDevice&,const PcCameraScreen&,
    const PcCameraBlend&,const PcCameraControl&);
// 409DF0 + 409E00 pair at the device boundary.
void camera_device_store(PcMatrixStack&,PcCameraDevice&,std::uint32_t slot);
}
