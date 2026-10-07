#pragma once
#include "driving/pc_common_control.hpp"
#include <array>
namespace outrun::platform {
// Persistent PC record/cache manager (65F0C0) and its request owner (98A708).
// These are NOT the gameplay session object tested by 4962A0/4962D0.
constexpr std::size_t PcRecordManagerBytes=0x1db40, PcRecordRequestBytes=0x398;
struct RecordReleaseServices {
    void* user{};
    // 525F80(handle), or virtual slot zero(handle, deleting=1).
    // A nonzero owned handle must be genuinely released; no implicit success.
    bool (*release)(void*,unsigned pc,unsigned handle,unsigned argument){};
};
bool record_request_reset_435ff0(driving::Bytes,const RecordReleaseServices&,unsigned& missing);
bool record_request_construct_435f70(driving::Bytes,unsigned& missing);
bool record_manager_reset_4940d0(driving::Bytes records,driving::Bytes request,
                               const RecordReleaseServices&,unsigned& missing);
struct FrontendRecordManager {
    std::array<std::uint8_t,PcRecordManagerBytes> records{};
    std::array<std::uint8_t,PcRecordRequestBytes> request{};
    RecordReleaseServices services;
    unsigned missing_pc{},reset_calls{};
    FrontendRecordManager(); // original static initialization, once per process
    bool reset();
};
}
