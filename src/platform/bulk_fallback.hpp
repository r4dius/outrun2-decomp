#pragma once
// The remaining original functions translated in bulk (tools/x86tr/lists/bulk.txt ->
// bulk_tr.cpp, every function the port does not implement yet, network excluded): a
// module's service runs one over the module's own memory when it has no port of it.
// Its calls to ported functions go back to that service.
#include "platform/race_translated.hpp"
#include <cstdint>
#include <map>
namespace outrun::platform {
bool bulk_translated(std::uint32_t pc);
// pc with the call's EAX / ECX and stack words over m; throws what the translated code
// throws (an unmapped address, an unported callee of the service).
// `next`: another translated set its code may call straight into (the LAN network set).
std::uint32_t bulk_call(PcRaceMemory& m,const PcRaceService& service,const PcRaceCall& k,
                        driving::PcMatrixStack* matrices=nullptr,std::uint32_t* crt_random=nullptr,
                        const TranslatedModule* next=nullptr);
struct BulkStats {std::map<std::uint32_t,std::uint32_t> calls,faults;};
BulkStats& bulk_stats();
}
