#pragma once
// The LAN layer's sockets on the Switch: BSD sockets (system/network_bsd.hpp)
// with the interface address from nifm.
#include "platform/pc_network.hpp"
namespace outrun::switch_runtime {
platform::PcNetworkPlatform switch_network_platform();
}
