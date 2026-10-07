#include "control.hpp"
#include <stdexcept>

namespace outrun::reconstructed {
namespace {
void required_call(ModeCallback fn, ModeMachine& m) {
    if (fn == nullptr) {
        throw std::logic_error("Unreconstructed/null mode callback; cannot run game");
    }
    fn(m, m.user);
}
}

void mode_control(ModeMachine& m) {
    m.state.snapshot_destination = m.state.snapshot_source;
    if (m.state.transition_pending != 0) {
        const auto requested = m.state.requested;
        const auto previous = m.state.current;
        m.state.current = requested;
        m.state.transition_pending = 0;
        m.state.previous = previous;
        required_call(m.callbacks.at(requested).init, m);
    }
    // Must re-read current after init: callbacks may change it.
    required_call(m.callbacks.at(m.state.current).control, m);
    if (m.state.transition_pending != 0) {
        required_call(m.callbacks.at(m.state.current).exit, m);
        // Tail helper 0x424080. Do not clear transition_pending here.
        m.state.exit_byte_unknown = 0;
    }
}

void event_control(EventSystem& e) {
    for (std::size_t i = 0; i < kEventCount; ++i) {
        e.current_index = i;
        // Do not cache flags/parameter across calls: they can change in callbacks.
        if ((e.flags[i] & 0x04u) != 0) {
            const auto fn = e.slots[i].on_bit4;
            if (fn != nullptr) {
                fn(e, e.slots[i].parameter, e.user);
            }
            e.flags[i] = static_cast<std::uint8_t>(e.flags[i] & 0xfbu);
        }
        if ((e.flags[i] & 0x01u) != 0) {
            const auto fn = e.slots[i].on_bit1;
            if (fn != nullptr) {
                fn(e, e.slots[i].parameter, e.user);
            }
            e.flags[i] = static_cast<std::uint8_t>((e.flags[i] & 0xfeu) | 0x02u);
        }
        const auto flags = e.flags[i];
        if ((flags & 0x18u) == 0 && (flags & 0x02u) != 0) {
            const auto fn = e.slots[i].on_active;
            if (fn != nullptr) {
                fn(e, e.slots[i].parameter, e.user);
            }
        }
    }
}
} // namespace outrun::reconstructed
