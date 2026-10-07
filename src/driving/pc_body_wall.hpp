#pragma once
#include "driving/pc_wall_controller.hpp"
namespace outrun::driving {
// Explicit dependencies of PC CarBodyWallColiCheck 0x00504CE0.  The original
// obtains body parameters and wheel records through serialized guest pointers
// only when it routes into PlWrecker; native code receives those views
// explicitly.  The alternate 0x43EEE0 query's unrelated game-state getters are
// represented by bk rather than hidden callbacks.
struct PcBodyWallContext {
    CourseWorldQuery& query;
    PcBkQueryContext bk;
    PcWallControllerContext& wall;
    Bytes body_params;
    Bytes wheel_block; // four contiguous 0xF4-byte wheel records
    PcPlWreckerContext& wrecker;
};
// Complete PC CarBodyWallColiCheck 0x00504CE0 on the bounded local-contact
// domain.  The original owns eight 16-byte local CBW_GETY_WORK records in its
// stack frame; counts beyond eight would overwrite caller state and are
// rejected natively.  Count <= 0 preserves the original no-probe -> tow route.
void car_body_wall_coli_check(Bytes event,Bytes work,PcBodyWallContext&);
}
