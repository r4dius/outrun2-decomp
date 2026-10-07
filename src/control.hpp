#pragma once
#include <array>
#include <cstddef>
#include <cstdint>

// OutRunSwitch r001: semantic reconstruction of TWO control dispatchers only.
// Original callbacks, physics, rendering, assets and platform services are absent.
// Host structures are NOT overlays of x86 memory and contain NO executable VAs.
namespace outrun::reconstructed {
inline constexpr std::size_t kModeCount = 37;
inline constexpr std::size_t kEventCount = 410;

struct ModeMachine;
using ModeCallback = void (*)(ModeMachine&, void*);
struct ModeCallbacks {
    ModeCallback init = nullptr;
    ModeCallback control = nullptr;
    ModeCallback exit = nullptr;
};
struct ModeState {
    std::uint32_t snapshot_source = 0;       // guest VA 0x00780264; meaning unknown
    std::uint32_t snapshot_destination = 0;  // guest VA 0x00780244
    std::uint32_t requested = 0;             // guest VA 0x0078025c
    std::uint32_t current = 0;               // guest VA 0x0078026c
    std::uint32_t previous = 0;              // guest VA 0x00780268
    std::uint32_t transition_pending = 0;    // guest VA 0x00780274; any nonzero
    std::uint8_t exit_byte_unknown = 0;      // guest VA 0x0095b008
};
struct ModeMachine {
    ModeState state{};
    std::array<ModeCallbacks, kModeCount> callbacks{};
    void* user = nullptr; // host test/port context, not an original guest field
};
// Original: VA 0x0043fa20..0x0043fa87 + helper 0x00424080..0x00424087.
// Invalid mode / null required callback throws instead of reading/calling invalid
// memory. For valid states the access and callback ordering follows disassembly.
void mode_control(ModeMachine& machine);

struct EventSystem;
using EventCallback = void (*)(EventSystem&, std::uint32_t parameter, void*);
struct EventSlot {
    std::uint32_t parameter = 0; // opaque original 32-bit value at guest record+8
    EventCallback on_bit4 = nullptr;   // guest record+0x20; purpose not assumed
    EventCallback on_bit1 = nullptr;   // guest record+0x10
    EventCallback on_active = nullptr; // guest record+0x14
};
struct EventSystem {
    std::array<EventSlot, kEventCount> slots{};
    std::array<std::uint8_t, kEventCount> flags{}; // guest VA 0x0079fb48
    // This is a host index. Original stored a guest pointer at VA 0x007a01d0.
    std::size_t current_index = kEventCount;
    void* user = nullptr;
};
// Original: VA 0x0043fab0..0x0043fb3a. Visits every slot, does not clear
// current_index at the end, re-reads flags and callback arguments after callbacks.
void event_control(EventSystem& system);
} // namespace outrun::reconstructed
