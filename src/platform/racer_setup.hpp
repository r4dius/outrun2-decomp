#pragma once
// PC 0x47CF40: rival racer set-up for a race (table 80FB00, count 80FB04,
// 0xA0 bytes per racer). The configuration graph (cfg, cfg+0x34 special
// racers, +0x38 speed profile, +0x3C, +0x40 names) is read through PC
// addresses: the relocated Races.bin record or the EXE defaults 64DFF0/64E040.
#include "platform/pc_address_view.hpp"
#include <array>
#include <cstdint>
#include <functional>
#include <string>
#include <vector>
namespace outrun::platform {
constexpr std::size_t RacerRecordBytes=0xa0u;
struct RacerSetupState {
    std::vector<std::uint8_t> racers_80fb00;
    std::uint32_t count_80fb04{};
    std::uint8_t flag_80fb08{},flag_80fb09{};
    std::uint32_t config_80fb0c{};
    std::uint8_t v80fb18{};
    std::vector<std::uint8_t> table_80fb1c;   // 4774B0 malloc((count*5+5)*4), not initialised
    std::vector<std::uint8_t> table_80fb20;   // 476550 (count+1)*0x1C, +0x10 = 0xFF
    std::uint8_t v80fb24{};
    std::uint32_t special_80fb28{};
    std::uint32_t v80fb2c{};
    std::vector<std::uint8_t> table_80fb30;   // 4774B0 malloc(count*8+8)
    std::uint32_t seed_6a4e2c{};
    std::uint32_t names_64deec{0x5b3a30u},names_64def0{0x5b3a3cu};
    std::int32_t v64e190{},v64e194{};
    std::uint32_t v8037b8{};
    std::uint32_t v680ad4{},v680ad8{};
    std::uint32_t length_803710{};
    std::int16_t per_lap_804388{};
    std::uint32_t profile_64df64{};
    std::array<float,6> rival_8514a4{};       // 505360 / 505370
};
struct RacerSetupServices {
    const PcAddressView* view{};
    bool car_present{};                       // 799D18
    std::int8_t car_byte11{};                 // [799D18]+0x11
    std::uint16_t car_word25e{};              // [799D18]+0x25E
    std::uint32_t network_686258{};           // 4B02A0
    // 44B820(laps): course length from the live course records.
    std::function<bool(std::int32_t laps,std::uint32_t& length)> course_length_44b820;
    // 496300(i): 8361C0 entries (count 83639C), 4 bytes each.
    std::vector<std::array<std::uint8_t,4>> preset_8361c0;
    bool selection_836374{};
    std::uint32_t mission_type_495b20{};
    std::int32_t time_4961f0{};
    std::string text_3d7;                     // 465EB0(0x3D7)
    unsigned missing{};
};
bool racer_setup_47cf40(RacerSetupState&,RacerSetupServices&,std::uint32_t config,bool flag);
}
