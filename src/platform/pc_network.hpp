#pragma once
// LAN layer (reports/decomp/network.md): the network tick 454670 that the frame
// loop 417B20 runs once per frame (the service object 83612C and its state
// machine 494790, the sessions [7D68AC] / [7D68B0], the lobby 988F40), run
// translated (pc_network_tr.cpp) over its own persistent memory. The game's
// LAN protocol is UDP on ports 41455..41457: its socket wrappers 491990 /
// 491A90 / 491B60 and the interface list 516D10 are answered natively over BSD
// sockets (the platform supplies them through PcNetworkPlatform).
#include <cstdint>
#include <functional>
#include <initializer_list>
#include <string>
#include <vector>
namespace outrun::platform {
struct NativeRuntimeContext;
class PcRaceMemory;
// Platform sockets (POSIX on the host, libnx BSD sockets on the Switch).
struct PcNetworkInterface { std::uint32_t address{},broadcast{},netmask{}; };   // network byte order
struct PcNetworkPlatform {
    // UDP socket bound to INADDR_ANY:port, non-blocking, SO_BROADCAST when asked; -1 on failure.
    std::function<int(std::uint16_t port,bool broadcast)> open_udp;
    std::function<void(int)> close;
    // Datagram to address:port (network byte order); bytes sent or -1.
    std::function<int(int socket,const std::uint8_t* data,std::uint32_t size,std::uint32_t address,std::uint16_t port)> send_to;
    // Next datagram into data (at most size bytes); bytes, 0 when none is waiting, -1 on error.
    std::function<int(int socket,std::uint8_t* data,std::uint32_t size,std::uint32_t& address,std::uint16_t& port)> receive_from;
    std::function<std::vector<PcNetworkInterface>()> interfaces;
};
void native_network_set_platform(PcNetworkPlatform);
// The frame's 454670.
bool native_network_tick_454670(NativeRuntimeContext&);
// A network-module function called by the frontend screens (454100 / 454140 /
// 454220 on the LAN menu): false while the layer is not started or after it faults.
// Maps the network-owned blocks the race code reads (the remote players' car states
// 7DD139..7DE418, the start state 850B00.., the session pointer 7D68AC and the guest heap)
// into a race module's memory, as views of the network module's memory. No-op offline.
void native_network_map_shared(PcRaceMemory&);
bool native_network_invoke(NativeRuntimeContext&,std::uint32_t pc,std::uint32_t ecx,std::initializer_list<std::uint32_t> args,std::uint32_t* eax=nullptr);
// The network module's memory (830C30 service ready, 7D68BC signed in, the
// 830C10 player name): 0 / ignored before the first tick maps it.
std::uint8_t native_network_u8(std::uint32_t address);
std::uint32_t native_network_u32(std::uint32_t address);
void native_network_put8(std::uint32_t address,std::uint8_t value);
bool native_network_mapped();
constexpr std::uint32_t NetworkOwnerBase=0x0ef00000u;   // the frontend owner 7B17E8 as the network module sees it (4035F0)
// Network screens (keys 14, 15...): the PC factory runs translated and the object
// lives in the guest heap; native_network_view gives its bytes, slots run its vtable.
std::uint32_t native_network_screen_create(NativeRuntimeContext&,std::uint32_t factory);
bool native_network_screen_slot(NativeRuntimeContext&,std::uint32_t object,std::uint32_t slot,std::uint32_t& result);
std::uint8_t* native_network_view(std::uint32_t address,std::uint32_t size);
std::uint32_t native_network_size(std::uint32_t address);
std::string native_network_string(std::uint32_t address);                       // a guest C string ("" when unmapped)
std::string native_network_format(std::uint32_t format,const std::uint32_t* args,std::size_t n);   // guest printf   // heap allocation (16-byte granules)                           // the layer's memory exists (first tick done)
void native_network_put_string(std::uint32_t address,const std::string&);
void native_network_put32(std::uint32_t address,std::uint32_t value);
struct NativeNetworkStats { std::uint32_t ticks{},failures{},sent{},received{}; std::string last_error; };
NativeNetworkStats& native_network_stats();
}
