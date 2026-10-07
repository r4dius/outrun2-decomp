#pragma once
#include <cstdint>
namespace outrun::platform {
// Per-model draw layouts (5B0CB8, 30 x 0x128) and colour-list IDs (64D120)
// used by PC 0x46AE70. EXE-owned data, bound to the player's EXE image.
extern const std::uint8_t* EmbeddedVehicleLayouts;   // 30*0x128 bytes
extern std::uint32_t EmbeddedVehicleColourIds[30];
void bind_embedded_vehicle_draw_tables();
} // namespace outrun::platform
