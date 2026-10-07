#pragma once
// Course passes of the frame renderer (449050 -> 40BD80 before / 40BE70 after
// the displays of a layer), driven by the SCN_EFC work (event 387, 780280,
// [79F5EC] flags): rain 40BF10 / 40C150 (bit 0), sun flare 40CBC0 (bit 1),
// the shadow map 422820 / 422F20 at layer 2 and its projection 40C1B0 /
// 40C4A0 (bit 2), headlights 40C550 / 40C8D0 (bit 3). Native over the
// SCN_EFC work, the renderer globals, the lights, the camera and the car;
// the render queue (4056D0, 4052C0, 404540, 408880), the bank lookup and the
// sun occlusion query (IDirect3DQuery9 89F680) are the runtime's.
#include <cstdint>
#include <string>
namespace outrun::platform {
struct NativeRuntimeContext;
class PcSceneRenderer;
// The frame leaf `pc` with its PC argument (work + offset, as 40BD80 passes
// it); false when pc is not a course pass.
bool native_course_pass_leaf(NativeRuntimeContext&,PcSceneRenderer&,std::uint32_t pc,std::uint32_t arg);
// [79F5EC] (0 while event 387 is closed) and its flags word, for PcFrameInputs.
std::uint32_t native_course_pass_work(NativeRuntimeContext&);
std::uint32_t native_course_pass_flags(NativeRuntimeContext&);
struct NativeCoursePassStats { std::uint32_t runs{},failures{}; std::string last_error; };
NativeCoursePassStats& native_course_pass_stats();
}
