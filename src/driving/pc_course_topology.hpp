#pragma once
#include "driving/pc_driving.hpp"
namespace outrun::driving {
// Explicit selected-course table views, not 32-bit guest root addresses.
struct CourseRunTables {
    Bytes header;             // PC root 0x780140[type], i32 record count +0x0c
    Bytes lengths;            // PC root 0x780228[type], u16 per polygon
    Bytes ranges;             // PC root 0x780218[type], {u16 first,u16 last}
    std::int32_t total_length; // PC global 0x780238
    bool force_ranges;        // PC byte 0x780190 != 0
    bool present=true;
};
// PC 0x0043D4D0. Failure returns zero without changing output indices. Native
// out-of-bounds table accesses throw instead of following invalid guest pointers.
std::int32_t find_primary_course_run(const CourseRunTables&,std::int16_t requested_length,
                                    std::int32_t hint,std::int32_t& first,std::int32_t& last);
}
