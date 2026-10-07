#pragma once
// PcNetworkPlatform over BSD sockets (POSIX, and libnx on the Switch once the
// NRO has initialised the socket service). Interfaces come from getifaddrs
// where it exists; a platform without it supplies its own (the Switch: nifm,
// switch/source/switch_network.hpp). Empty without BSD sockets.
#include "platform/pc_network.hpp"
namespace outrun::platform {
PcNetworkPlatform pc_network_bsd_platform();
}
