#pragma once
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>
namespace outrun::platform {
// Packs built from the player's EXE image when it is installed (exe_packs.cpp).
extern const std::uint8_t* EmbeddedEventMetadata;
extern std::size_t EmbeddedEventMetadataSize;
extern const std::uint8_t* EmbeddedStage17Assets;
extern std::size_t EmbeddedStage17AssetsSize;
extern const std::uint8_t* EmbeddedDrivingData;
extern std::size_t EmbeddedDrivingDataSize;
// OR2CRS3 course pack: EXE course descriptors + the retail
// Scripts/bin/csc_data_cvt.bin (tools/build_switch_course_pack_r119.py).
bool build_course_asset_pack(const std::vector<std::uint8_t>& csc_data_cvt,std::vector<std::uint8_t>& pack,std::string* error);
// Ten font records + glyph maps + kerning of the PC font descriptors
// (tools/build_switch_font_pack.py --metrics-source layout): 480 bytes of
// records, then the payloads. atlas[token] = {width, height, format code}.
bool build_font_metrics(const std::uint32_t (&atlas)[10][3],std::vector<std::uint8_t>& metrics,std::string* error);
void build_exe_packs();
 } // namespace outrun::platform
