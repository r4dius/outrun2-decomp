#pragma once
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <vector>

namespace outrun::assets {
// This is a checked, non-mutating reader for the EXPLICIT carview-derived layout.
// It is not a renderer, texture decoder or proof of compatibility with PC assets.
struct FormatError : std::runtime_error { using std::runtime_error::runtime_error; };
struct ByteView {
    const std::uint8_t* data = nullptr;
    std::size_t size = 0;
    explicit ByteView(const std::vector<std::uint8_t>& v) : data(v.data()), size(v.size()) {}
    ByteView(const std::uint8_t* p, std::size_t n) : data(p), size(n) {}
    void require(std::size_t offset, std::size_t length) const;
    std::uint32_t u32(std::size_t offset) const;
};
struct DisplayTable {
    std::uint32_t record_count;
    std::uint32_t data_offset;
};
enum class ScreenLayout { Classic28, Rotated32 };
struct XstView {
    std::uint32_t flags, texture_offset, texture_count;
    std::uint32_t display_table_offset, screen_table_offset, screen_count;
    ScreenLayout screen_layout;
    std::vector<DisplayTable> display_tables;
};
// Replaces x86 pointer relocation with checked relative offsets. It leaves all
// input bytes untouched. ReadXstsetSub in Jennifer corroborates offsets 0x10,
// 0x14, 0x1c and the 8-byte display table. Classic28 is carview's tag_SCRTBL;
// Rotated32 is its tag_SCRTBL2. Selection MUST be explicit: no guessed autodetect.
XstView inspect_xst(ByteView input, ScreenLayout layout);

struct ObjectHeaderView {
    std::uint32_t offsets[9];
    std::uint32_t vertex_format_count, material_group_count, material_count, color_count;
    std::uint32_t matrix_count;
};
// A bounded reader of a DECOMPRESSED ObjectHeader-based sub-blob. It is not a
// parser for an entire XMT/SMT container and does not validate vertex payloads.
ObjectHeaderView inspect_object(ByteView object_blob);
} // namespace outrun::assets
