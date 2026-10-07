#pragma once
#include <cstdint>
namespace outrun::platform {
// Camera parameter tables copied by PC 0x484EE0 (EXE-owned data: views of the
// player's EXE image, bound by exe_tables.cpp; 0x64C, 0x438 and 0x5460 bytes).
extern const std::uint8_t* EmbeddedCameraTable5b4a30;
extern const std::uint8_t* EmbeddedCameraTable5ba4e0;
extern const std::uint8_t* EmbeddedCameraTable5b5080;
void bind_embedded_camera_tables();
} // namespace outrun::platform
