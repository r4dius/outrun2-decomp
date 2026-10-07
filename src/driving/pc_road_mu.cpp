#include "driving/pc_road_mu.hpp"
#include "driving/pc_wheel_dynamics.hpp"
#include <stdexcept>

namespace outrun::driving {
namespace {
float fallback_mu(std::uint32_t surface) {
    if (surface == 1u) return 0.0f;
    if (surface == 4u || surface == 8u || surface == 16u || surface == 0x8000u)
        return 0.699999988079071044921875f; // PC 0x005C403C
    return 1.0f;                            // PC 0x0062806C
}
}

// PC assMuControl 0x00518420.  These offsets are exactly the four embedded
// wheel Mu fields and the rear-wheel signed tire angles in the recovered work
// layout; no guest pointers are dereferenced on the native side.
void ass_mu_control(Bytes w) {
    auto q = embedded_wheels(w);
    if (q[2].i16(0xee) < 0 && q[1].f32(0xe8) > q[0].f32(0xe8))
        q[1].putf(0xe8, q[0].f32(0xe8));
    if (q[3].i16(0xee) > 0 && q[0].f32(0xe8) > q[1].f32(0xe8))
        q[0].putf(0xe8, q[1].f32(0xe8));
    if (q[0].f32(0xe8) > q[2].f32(0xe8))
        q[2].putf(0xe8, q[0].f32(0xe8));
    if (q[1].f32(0xe8) > q[3].f32(0xe8))
        q[3].putf(0xe8, q[1].f32(0xe8));
}

// PC GetRoadMu 0x00500B30.  When the road service is present every surface
// branch calls the same lookup; the switch tree only selects fallback Mu when
// that service is unavailable.  This is also what the symbolized Lindbergh
// routine reduces to when its platform road lookup is absent.
void get_road_mu(Bytes e, Bytes w, const RoadMuSource& source) {
    if (source.service_available && !source.lookup)
        throw std::invalid_argument("road Mu service marked available without lookup");
    const std::uint32_t road_id = e.u32(0xd38);
    for (auto q : embedded_wheels(w)) {
        const std::uint32_t surface = q.u32(0x14);
        const float mu = source.service_available
            ? source.lookup(source.context, road_id, surface)
            : fallback_mu(surface);
        q.putf(0xe8, mu);
    }
    ass_mu_control(w);
}

} // namespace outrun::driving
