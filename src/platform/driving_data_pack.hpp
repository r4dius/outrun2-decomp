#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace outrun::platform {

// OR2DRV1 contains only static data copied from the hash-pinned owned PC EXE:
// the complete 19-column player-car parameter arena, four 15-car selector
// maps, two engine-torque curves and the brake-pressure curve.
struct DrivingDataPack {
    std::vector<std::uint8_t> parameter_arena;
    std::array<std::array<std::uint32_t,15>,4> selection_maps{};
    std::array<std::vector<std::uint8_t>,2> torque_tables;
    std::vector<std::uint8_t> brake_table;
    std::array<std::uint8_t,32> source_sha256{};
    std::uint32_t source_bytes{};
};

constexpr std::size_t DrivingDataHeaderSize=192u;
constexpr std::size_t DrivingParameterColumns=19u;
constexpr std::size_t DrivingParameterViewBytes=0x2650u;
constexpr std::size_t DrivingParameterArenaBytes=
    DrivingParameterViewBytes+(DrivingParameterColumns-1u)*4u;
constexpr std::size_t DrivingSelectionMapCount=4u;
constexpr std::size_t DrivingSelectionMapEntries=15u;
constexpr std::size_t DrivingTorqueTableBytes=648u;
constexpr std::size_t DrivingBrakeTableBytes=1024u;
// OR2DRV1 header +88/+92 contains the old prototype selection, mislabelled
// "250 GTO" in r124. Keep its binary format readable, NOT as a game default.
// Real model9/24 selection is resolved by vehicle_parameter_choice_5051d0.
constexpr std::uint32_t DrivingPackV1LegacyCarId=5u;
constexpr std::uint32_t DrivingPackV1LegacyColumn=6u;

bool parse_driving_data_pack(const std::uint8_t* data,std::size_t size,
                             DrivingDataPack& pack,std::string* error=nullptr);
bool load_driving_data_pack_file(const char* path,DrivingDataPack& pack,
                                 std::string* error=nullptr);
bool copy_driving_parameter_view(const DrivingDataPack& pack,
                                 std::uint32_t selector_map,
                                 std::uint32_t car_id,
                                 std::uint8_t* output,std::size_t output_size,
                                 std::uint32_t* selected_column=nullptr);

} // namespace outrun::platform
