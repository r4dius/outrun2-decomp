#include "asset_views.hpp"
#include "carview_layouts.hpp"
#include <limits>

namespace outrun::assets {
void ByteView::require(std::size_t offset, std::size_t length) const {
    if ((data == nullptr && size != 0) || offset > size || length > size - offset)
        throw FormatError("Resource range outside input");
}
std::uint32_t ByteView::u32(std::size_t offset) const {
    require(offset, 4);
    return std::uint32_t(data[offset]) | (std::uint32_t(data[offset + 1]) << 8)
        | (std::uint32_t(data[offset + 2]) << 16) | (std::uint32_t(data[offset + 3]) << 24);
}
namespace {
void require_records(ByteView b, std::uint32_t offset, std::uint32_t count, std::size_t stride) {
    b.require(offset, 0);
    if (stride == 0 || count > (b.size - offset) / stride)
        throw FormatError("Record count exceeds available resource bytes");
}
std::uint32_t signed_count(ByteView b, std::size_t offset) {
    const auto count = b.u32(offset);
    if (count > std::uint32_t(std::numeric_limits<std::int32_t>::max()))
        throw FormatError("Negative signed record count");
    return count;
}
}
XstView inspect_xst(ByteView b, ScreenLayout layout) {
    using H = carview_layout::tag_XSTHEAD;
    b.require(0, H::byte_size);
    if (layout != ScreenLayout::Classic28 && layout != ScreenLayout::Rotated32)
        throw FormatError("Unsupported explicit XST screen layout");
    XstView v{b.u32(H::flag_offset), b.u32(H::tex_ofs_offset), b.u32(H::nb_tex_offset),
              b.u32(H::dsptbl_offset), b.u32(H::scrtbl_offset), b.u32(H::nb_scrtbl_offset),layout,{}};
    const auto count = b.u32(H::nb_dsptbl_offset);
    require_records(b, v.display_table_offset, count, carview_layout::tag_DSPTBL::byte_size);
    require_records(b, v.screen_table_offset, v.screen_count,
                    layout == ScreenLayout::Classic28 ? carview_layout::tag_SCRTBL::byte_size
                                                     : carview_layout::tag_SCRTBL2::byte_size);
    // Avoid oversized allocations from malformed input: count was bounded by b.
    v.display_tables.reserve(count);
    for (std::size_t i = 0; i < count; ++i) {
        const auto offset = std::size_t(v.display_table_offset) + i * carview_layout::tag_DSPTBL::byte_size;
        DisplayTable t{b.u32(offset), b.u32(offset + 4)};
        require_records(b, t.data_offset, t.record_count, carview_layout::tag_DSPDATA::byte_size);
        v.display_tables.push_back(t);
    }
    // texture_offset/texture_count remain metadata. Texture storage and pixel
    // formats are not decoded or claimed validated by this function.
    return v;
}
ObjectHeaderView inspect_object(ByteView b) {
    using H = carview_layout::ObjectHeader;
    b.require(0, H::byte_size);
    ObjectHeaderView v{};
    for (std::size_t i = 0; i < 9; ++i) { v.offsets[i] = b.u32(i * 4); b.require(v.offsets[i], 0); }
    v.vertex_format_count = signed_count(b, H::numof_vtx_formats_offset);
    v.material_group_count = signed_count(b, H::numof_mat_groups_offset);
    v.material_count = signed_count(b, H::numof_materials_offset);
    v.color_count = signed_count(b, H::numof_mat_colors_offset);
    require_records(b,v.offsets[6],v.vertex_format_count,carview_layout::VtxFormatList::byte_size);
    require_records(b,v.offsets[4],v.material_group_count,carview_layout::MatGroupInfo::byte_size);
    require_records(b,v.offsets[7],v.material_count,carview_layout::MaterialList::byte_size);
    require_records(b,v.offsets[8],v.color_count,carview_layout::MaterialColor::byte_size);
    if (v.offsets[2] < v.offsets[1] || (v.offsets[2] - v.offsets[1]) % 64 != 0)
        throw FormatError("Matrix range is not a whole number of 64-byte matrices");
    v.matrix_count = (v.offsets[2] - v.offsets[1]) / 64;
    return v;
}
} // namespace outrun::assets
