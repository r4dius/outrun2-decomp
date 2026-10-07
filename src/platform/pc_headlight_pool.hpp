#pragma once
// Car headlight pool on the road (GamePlCar display 49F400 -> 4695C0 -> 416AA0)
// and its static vertex buffer 8A8C84 (bootstrap 417881 -> 416A40 / 4171F0 /
// 417550): 160 XYZ|DIFFUSE|TEX1 vertices (0x18 bytes), 20 strips of 8, built
// from the .data constants 746B0C..746B20 (no writer in the EXE).
#include "driving/pc_driving.hpp"
#include "driving/pc_matrix_stack.hpp"
#include "platform/pc_d3d9.hpp"
#include "platform/pc_render_flush.hpp"
#include <array>
#include <cstdint>
#include <vector>
namespace outrun::platform {
constexpr std::size_t PcHeadlightVertices=160,PcHeadlightStride=0x18;
// 4171F0 (with 417550): the vertex buffer contents.
std::vector<std::uint8_t> pc_headlight_vertices_4171f0();
// 4695C0(car): drawn when car+2C8 <= 0 and car+4 has bits 0x100 and 0x800;
// 409F90(car+B0) / 416AA0 / 40A010. 416AA0 needs the resource bank 3 loaded
// (956D88[3] state 2) and its texture 9 (956E14 -> +24): texture 0 = not drawn.
// Returns true when 416AA0 drew.
bool pc_headlight_display_4695c0(driving::Bytes car,PcFlushContext& flush,PcD3D9Device& device,
    driving::PcMatrixStack& matrices,const std::vector<std::uint8_t>& vertices,std::uint32_t texture);
}
