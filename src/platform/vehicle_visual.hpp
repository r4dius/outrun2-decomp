#pragma once
#include "platform/mesh_preview_pack.hpp"
#include <array>
#include <cstdint>

namespace outrun::platform {

using VehicleVisualMatrix=std::array<float,16>;

constexpr std::size_t VehicleVisualObjectCount=7u;
constexpr std::array<std::array<float,3>,VehicleVisualObjectCount>
VehicleVisualObjectTranslations{{
    {{0.0f,0.0f,0.0f}},
    {{-0.6769999861717224f,0.34200000762939453f,-1.2669999599456787f}},
    {{ 0.6769999861717224f,0.34200000762939453f,-1.2669999599456787f}},
    {{-0.7149999737739563f,0.35100001096725464f, 1.1579999923706055f}},
    {{ 0.7149999737739563f,0.35100001096725464f, 1.1579999923706055f}},
    {{-0.6340000033378601f,0.0f,-0.3070000112056732f}},
    {{ 0.6340000033378601f,0.0f,-0.3070000112056732f}},
}};

// These fields mirror the display-facing values reconstructed from the PC
// wheel record.  spin is wheel+0x30 and direction is wheel+0x32.  Translation
// is a delta from the model record copied to event+0x140; it permits the same
// matrix path to carry suspension movement without baking new geometry.
struct VehicleVisualWheel {
    std::array<float,3> translation_delta{};
    std::int16_t spin{};
    std::int16_t direction{};
    float model_rotation_z{};
};

struct VehicleVisualTransform {
    VehicleVisualMatrix position{};
    VehicleVisualMatrix normal{};
};

float pc_wheel_visual_angle(std::int16_t value);
VehicleVisualMatrix vehicle_visual_identity();
VehicleVisualMatrix vehicle_visual_multiply(const VehicleVisualMatrix& left,
                                             const VehicleVisualMatrix& right);
VehicleVisualTransform compose_vehicle_visual_transform(
    const MeshPreviewTransform& base,const VehicleVisualWheel& wheel);
bool compose_course_vehicle_transform(
    const MeshPreviewTransform& camera,
    const std::array<float,3>& vehicle_position,
    float vehicle_yaw,
    const std::array<float,3>& ground_normal,
    std::size_t object_index,
    const VehicleVisualWheel* wheel,
    VehicleVisualTransform& transform);

} // namespace outrun::platform
