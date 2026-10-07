#pragma once
#include "driving/pc_driving.hpp"
#include <cstdint>

namespace outrun::driving {

// Platform/service boundary used by PC GetRoadMu (0x00500B30).  The original
// asks the PC road manager whether dynamic surface lookup is available and, if
// so, queries it with (event+0xD38, wheel surface flags).  Keeping that lookup
// explicit lets the same recovered game logic be used by a Switch road backend.
using RoadMuLookup = float (*)(void* context, std::uint32_t road_id,
                               std::uint32_t surface_flags);
struct RoadMuSource {
    bool service_available{};
    void* context{};
    RoadMuLookup lookup{};
};

void ass_mu_control(Bytes work);
void get_road_mu(Bytes event, Bytes work, const RoadMuSource& source);

} // namespace outrun::driving
