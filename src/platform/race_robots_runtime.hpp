#pragma once
// Native runtime binding of the race robots (race_robots.hpp): events 362
// ROB01 (RobDriver) and 364 ROB03 (RobFlagMan) and their display callbacks.
// The module runs over a PcRaceMemory holding what the runtime owns:
//   module .bss/.data/works (PcRaceRobotState), EXE tables,
//   799D18 -> the shared car (car_799d18 at the CAR01 work token),
//   79F010 -> the ROB01 work token, 79F574 -> the CAMERA (385) work
//   (camera_79fe10), 79FB48+id event flags, 78024C course preset,
//   780258 race variant, 78026C mode, 7F94C0 = 0 (offline, no LAN session
//   object, as the race car display assumes).
// Nothing else is mapped: a read of another module's global throws and is
// reported. No RobMotion/ROBDISPWORK/cloth/scene service is ported, so every
// service call is reported as missing (never answered with an invented
// value) and latches a fault on that robot: its later callbacks are skipped
// and counted.
#include "platform/race_robots.hpp"
#include "platform/rob_motion.hpp"
#include "platform/rob_motion_tables.hpp"
#include "platform/rob_disp.hpp"
#include "platform/rob_osage.hpp"
#include "platform/rob_motion_engine.hpp"
#include "platform/rob_flag.hpp"
#include <array>
#include <map>
#include <string>
namespace outrun::platform {
struct NativeRuntimeContext;
class PcSceneRenderer;
struct NativeRaceRobots {
    PcRaceRobotState state;
    PcRaceMemory memory;                              // re-mapped per call; capacity reused
    RobotHeap heap;                                   // 580253 buffers of the robots (SetBone)
    std::array<std::uint8_t,4> cell_799d18{},cell_79f010{},cell_79f574{},cell_7f94c0{};
    std::array<std::uint8_t,4> cell_78024c{},cell_780258{},cell_78026c{};
    std::array<std::uint8_t,0x19a> flags_79fb48{};
    std::array<std::uint32_t,8> fault{};              // per ROB01..ROB08: first missing PC / unmapped address
    std::map<std::uint32_t,std::uint32_t> missing;    // service PC -> calls reached
    std::map<std::uint32_t,std::uint32_t> routed;     // race manager binding: services answered natively
    std::map<std::uint32_t,std::uint32_t> unmapped;   // PC address -> reads/writes reached
    std::uint32_t inits{},controls{},displays{},destroys{},skipped{},completed{};
    std::string last_error;
    std::array<std::string,8> errors;                // first error per robot
    NativeRobMotionTables motion_tables;              // RobMotion 84D970.. (4F2470), rob_motion_tables.hpp
    PcRobDispState disp;                              // ROBDISPWORK 8577D0.. (rob_disp.hpp)
    bool disp_constructed{};                          // 595430 static construction done
    PcRobFlagState flag;                              // flagman flag cloth 95B270.. / mesh 85FF20.. (rob_flag.hpp)
    PcRobOsageState osage;                            // osage chains 858710.. / 85DE50.. (rob_osage.hpp)
    PcRobMotionEngineState engine;                    // RobMotion engine globals 85B2B8.. (rob_motion_engine.hpp)
    std::array<std::uint8_t,4> cell_842114{};         // frame step (race clock, read by 4F2520)
    // Renderer of the bank objects (448CD0 handles: 4066D0 / 406730 / 4103F0),
    // bound by the platform; null: those services are reported missing.
    PcSceneRenderer* renderer{};
    // 488B80 character files (CHR_*.bin): 580253 buffers at 71000000..
    static constexpr std::uint32_t ChrHeapBase=0x71000000u,ChrHeapEnd=0x72000000u;
    std::vector<NativeRobMotionTables::Heap> chr_heaps;
    std::uint32_t chr_heap_next{ChrHeapBase};
    // AUTOSCENE (event 6 function 0x19, race_autoscene.hpp) runs on this memory: its event
    // work (440A60: 0x124 bytes + the {work, heap} trailer), the script files (44FD80 /
    // 44F880 / 44FC60, read synchronously: payload + the {payload, mode} trailer, never
    // reused), the robot event records 799B38 + id * 0x3C (ids 0x16A..0x17F) and 799CA0.
    static constexpr std::uint32_t AutosceneWorkBase=0x73800000u,AutosceneWorkSize=0x124u;
    static constexpr std::uint32_t AutosceneFileBase=0x73900000u,AutosceneFileEnd=0x74000000u;
    static constexpr std::uint32_t AutosceneFileHandle=0x75000001u;
    std::array<std::uint8_t,0x130> autoscene_work{};
    std::vector<NativeRobMotionTables::Heap> autoscene_files;
    std::uint32_t autoscene_file_next{AutosceneFileBase};
    std::vector<std::uint32_t> autoscene_handles;            // 44FD80 handle - AutosceneFileHandle -> file base (0: refused)
    std::array<std::array<std::uint8_t,4>,0x16> cell_records_799b38{};
    std::array<std::uint8_t,4> cell_799ca0{};
    std::uint32_t autoscene_fault{};                         // first missing service PC / unmapped address
    std::string autoscene_error;
    std::uint32_t autoscene_calls{},autoscene_draws{},autoscene_skipped{};
    std::map<std::uint32_t,std::uint32_t> autoscene_unplayed; // effect emitters 4208A0 / 420560 (PART_EFC) not ported: counted
    std::uint32_t chr_loads{},chr_files{},chr_fault{};
    bool chr_ready{};
    std::string chr_error;
};
// Event callbacks (init/control/destroy): returns false when the callback is
// not a robot callback. The work is the current event slot's work token.
bool native_race_robots_invoke(NativeRuntimeContext&,std::uint32_t callback,driving::PcMatrixStack&);
// PC 4F2470 (RobMotion system init), called by the shared loader 49E580
// stage 0: fills context.race.robots.motion_tables from the retail files
// Common\bone.bin / Common\motdata_table.bin (retail tree, else the loader
// pack). A service without a native answer or a missing file is reported
// in motion_tables.fault/error and leaves the tables unmapped.
void native_rob_motion_tables_init(NativeRuntimeContext&);
// PC 4F2020(group, mode) (START 49B420) and its completion by the scheduler
// 4F2130/4F2060 (the async load of \Anims\<name> into 84DD68[group]), run
// synchronously: false (with motion_tables.error) when the tables are not
// ready or the file cannot be read.
bool native_rob_motion_group_request_4f2020(NativeRuntimeContext&,std::uint32_t group,std::uint32_t mode);
// PC 4F21C0(group): the motion group released, its slot idle again.
bool native_rob_motion_group_release_4f21c0(NativeRuntimeContext&,std::uint32_t group);
// PC 488B80 (boot mode 0 loader 49E6A0 stage 0x0C, 49E73D): the character
// files CHR_*.bin of the robots into 82F388/82F488 (rob_disp.hpp). Errors
// are reported in chr_error/chr_fault.
void native_rob_chr_load_488b80(NativeRuntimeContext&);
// AUTOSCENE: event 6 callbacks 4B5150 / 4B6CD0 / 4B6690 (and the scene robots' function 0x66
// control 488450 / destroy 487980) are answered by native_race_robots_invoke; the displays
// 4B6A30 / 487880 by native_race_robots_display. The mode-24 calls 4B5F60(scene), 4B5FC0,
// 4B5FD0: one call on the robots' memory (false: refused, see autoscene_error).
bool native_autoscene_call(NativeRuntimeContext&,std::uint32_t pc,std::uint32_t arg,std::uint32_t& eax);
// Display callbacks 4879B0 / 4879D0.
bool native_race_robots_display(NativeRuntimeContext&,std::uint32_t callback,std::uint32_t work_token,driving::PcMatrixStack&);
// PC 487BE0(out, work, bone) on the robots' memory: the world position of
// the robot's bone; false (out untouched) when its motion has no such bone.
bool native_robot_bone_position_487be0(NativeRuntimeContext&,std::uint32_t work,std::uint32_t bone,
                                       driving::PcMatrixStack&,driving::CourseProbe& out);
}
