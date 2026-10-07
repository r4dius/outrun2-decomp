#pragma once
#include "driving/pc_crash_entry.hpp"
namespace outrun::driving {
// Explicit native dependencies of PC CbwColiWall 0x005041B0.  The original
// selected one byte-per-polygon material table through 0x780170[type] and a
// 16-byte material-mode table at 0x5E0DE0; native code follows neither guest
// pointer and receives both as bounded views.
struct PcWallControllerContext {
    CourseWorldTables world;
    std::array<Bytes,4> surface_codes{Bytes(nullptr,0),Bytes(nullptr,0),Bytes(nullptr,0),Bytes(nullptr,0)};
    Bytes material_modes; // indexed by low material nibble
    WallReboundContext rebound;
    PcCrashEntryContext crash;
    PcCrushSelection crush;
    PcMaterialSounds material_sounds;
    std::array<PcCourseEndView,4> course_ends;
    std::uint32_t mode=0;
    std::uint32_t variant=0;
};
// Complete PC wall controller 0x005041B0 over the documented well-formed
// domain: work+0x68C contains 1..32 contact/shape records and at least one
// contact has a qualifying wall flag.  The original reads uninitialised local
// face data if no qualifying record exists; native code rejects that malformed
// input instead.  Matrix effects from CopColiPoint, Load/RotateY, ColiSet and
// the selected response path are retained.
void cbw_coli_wall(Bytes event,Bytes work,Bytes contacts,PcMatrixStack&,PcWallControllerContext&);
}
