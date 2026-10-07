#pragma once
// PC race "robots" (the Lindbergh Rob* events: skinned characters animated by
// RobMotion and drawn by ROBDISPWORK), module PC 487810..4897EB:
//   event 362 ROB01 = RobDriver  (the player's driver in the car):
//       init 4884D0, control 488DD0, display 4879B0, destroy 487980
//   event 364 ROB03 = RobFlagMan (the start-line flag man):
//       init 488710, control 488820, display 4879D0, destroy 487A00
// Their event works are 0x90-byte tagEvWorkRobot records (ROB01..ROB08 at
// 7A01E0 + i*0x90); work+0 is the index of the RobMotion object
// 82F5F0 + i*0xA4 the record drives.
//
// The port is a line-by-line transliteration over PC addresses (the
// PcRaceMemory/PcRaceCall conventions of race_area.hpp): the module .bss
// (82F11C..82FB10, including the eight RobMotion objects), the robot works
// and the module .data image (654568..6549C8) live in PcRaceRobotState;
// the car works (799D18 -> 7804B0 + i*0x10F0), camera work (79F574), event
// flags (79FB48+id) and the .rdata tables (race_robots_map_tables) are mapped
// by their owners. Reading an unmapped address throws PcRaceUnmapped.
//
// Ported in the module (verified by the oracle against the original):
// the math/matrix leaves (40A060, 40EF70, 40EFA0, 40EFD0, 40F140, 40EEB0,
// 4493A0, matrix stack 409EF0/409F30/409F90/40A010/40A0D0/40A170/40A220/
// 40A2D0/40A3E0/40A410/40A440/40A7D0/40A820), the RobMotion accessors
// 4ED890 SetSpeed, 4F1CE0 SetBoneID, 4F1E70 IsMotionEnd, 4F1EC0 get_matrix,
// 4F1F00 GetMotName, 4F1F10 get_mot_name, the getters 455AD0, 455C10,
// 450580, 450160, 450650, 45B380, 46BC10 GetCharPosition, 55A930 (net
// session word) and the CRT qsort 580CB0 (with the module comparator
// 487920). Every other callee is a service (PcRaceCall, see
// race_robots_services below); the display side is service calls too
// (40D840 SceneEnvironment, 514E60 rob_disp_disp, 409E00 mxSetD3DTransform,
// 402170 flagman_cloth_disp) made with the current matrix on the stack.
#include "platform/race_area.hpp"
#include <cstddef>
#include <cstdint>
#include <vector>
namespace outrun::platform {
struct PcRaceRobotTable { std::uint32_t base; const std::uint8_t* data; std::size_t size; };
extern const PcRaceRobotTable RaceRobotTables[6];      // EXE .rdata ranges (race_robots_data.cpp)
extern std::uint8_t RaceRobotsDataImage[0x460]; // EXE .data 654568..6549C8
void race_robots_map_tables(PcRaceMemory&);
struct PcRaceRobotState {
    static constexpr std::uint32_t BlockBase=0x82f0f0u,BlockEnd=0x82fb10u;   // module .bss (82F0F0..82F11B: 488340)
    static constexpr std::uint32_t WorkBase=0x7a01e0u,WorkEnd=0x7a0660u;     // ROB01..ROB08 works
    static constexpr std::uint32_t DataBase=0x654568u,DataEnd=0x6549c8u;     // module .data
    static constexpr std::uint32_t MotionBase=0x82f5f0u,MotionSize=0xa4u;
    std::vector<std::uint8_t> block=std::vector<std::uint8_t>(BlockEnd-BlockBase);
    std::vector<std::uint8_t> works=std::vector<std::uint8_t>(WorkEnd-WorkBase);
    std::vector<std::uint8_t> data;
    PcRaceRobotState(); // data = EXE image
    void map(PcRaceMemory& m){
        m.map(BlockBase,block.data(),block.size());m.map(WorkBase,works.data(),works.size());
        m.map(DataBase,data.data(),data.size());
    }
};
// Symbolic addresses of the 488DD0 stack locals 46BC30 writes (the two
// 12-byte vectors [esp+0x34] / [esp+0x28] of the PC frame).
constexpr std::uint32_t PcRobotLocal488dd0A=0x7fff0100u;
constexpr std::uint32_t PcRobotLocal488dd0B=0x7fff0110u;
// Services (PcRaceCall.pc, ECX = RobMotion object for the thiscall ones):
//   RobMotion   4F1CF0 SetBone(a,b) 4F1D30 SetFrame(f) 4F1E60 SetLoop(n)
//               4F2260 UnsetBone() 4F2280 SetMotion(id) 4F2320
//               SetMotionConnect(id,f,f) 4F2520 Calc()
//   ROBDISPWORK 514BF0 rob_disp_init 514F60 rob_osage_init(work,motion)
//               5147D0 rob_disp_ctrl 515040 rob_osage_ctrl(work,motion)
//               5148D0 set_hand_gu 5148F0 set_hand_pa 514E60 rob_disp_disp
//               514E80 rob_osage_dest
//   flag cloth  402100 init 402140 ctrl 402170 disp 4021A0 dest
//   scene       4082B0 GetLightWorkAddress(2,0,0) -> light record pointer
//               40D840 SceneEnvironment(2) 409E00 mxSetD3DTransform(6)
//   game        49B2D0 GetStartUpTimer 43F860 440D50/440D70 (allocator
//               mode) 44BCE0 44BD20 44DC50 44FE70 44FF10 450240 450380
//               451350 45A2B0 46BC30 GetHandleGripPositionLocal
//               (car,f,A,B) 47FBD0 417F70 (seed) 580F33 srand 580F40 rand
constexpr std::uint32_t PcRaceRobotServices[]={
    0x4f1cf0u,0x4f1d30u,0x4f1e60u,0x4f2260u,0x4f2280u,0x4f2320u,0x4f2520u,
    0x514bf0u,0x514f60u,0x5147d0u,0x515040u,0x5148d0u,0x5148f0u,0x514e60u,0x514e80u,
    0x402100u,0x402140u,0x402170u,0x4021a0u,0x4082b0u,0x40d840u,0x409e00u,
    0x49b2d0u,0x43f860u,0x440d50u,0x440d70u,0x44bce0u,0x44bd20u,0x44dc50u,0x44fe70u,0x44ff10u,
    0x450240u,0x450380u,0x451350u,0x45a2b0u,0x46bc30u,0x47fbd0u,0x417f70u,0x580f33u,0x580f40u};
// Event callbacks (work = the event work address).
void race_robot_driver_init_4884d0(PcRaceContext&,std::uint32_t work);
void race_robot_driver_control_488dd0(PcRaceContext&,std::uint32_t work);
void race_robot_driver_display_4879b0(PcRaceContext&,std::uint32_t work);
void race_robot_driver_destroy_487980(PcRaceContext&,std::uint32_t work);
void race_robot_flagman_init_488710(PcRaceContext&,std::uint32_t work);
void race_robot_flagman_control_488820(PcRaceContext&,std::uint32_t work);
void race_robot_flagman_display_4879d0(PcRaceContext&,std::uint32_t work);
void race_robot_flagman_destroy_487a00(PcRaceContext&,std::uint32_t work);
// Parts, exposed for the probe.
void race_robot_common_init_487810(PcRaceContext&,std::uint32_t work);
std::uint32_t race_robot_set_chara_487c30(PcRaceContext&,std::uint32_t work,std::uint32_t chara);
std::uint32_t race_robot_driver_chara_487ee0(PcRaceContext&);
bool race_robot_hand_motion_487940(const PcRaceMemory&,std::uint32_t motion_id);
void race_robot_display_487880(PcRaceContext&,std::uint32_t work);
std::uint32_t race_robot_flagman_runaway_487a30(PcRaceContext&);
void race_robot_motion_connect_487b70(PcRaceContext&,std::uint32_t work,std::uint32_t motion_id,std::uint32_t loop);
void race_robot_control_488450(PcRaceContext&,std::uint32_t work);
// The passenger (event 0x16B, function 0x39: 4885A0 init, 489A90 control, 4879B0 display,
// 487980 destroy): her motions, the race reactions and the navigator voice queue 82F268
// (488130 / 4881C0).
// 45B380(model byte): 1 for the model classes without the jump-table "0" entry.
std::uint32_t race_robot_model_flag_45b380(std::uint32_t model_byte);
void race_robot_passenger_init_4885a0(PcRaceContext&,std::uint32_t work);
void race_robot_passenger_control_489a90(PcRaceContext&,std::uint32_t work);
// Functions 0x38 (4884D0 init, 489850 control) and 0x3A (4885A0 init, 48AD60 control): the
// driver and the passenger of the other start contexts (4B7B66 / 4B8303).
void race_robot_driver_control_489850(PcRaceContext&,std::uint32_t work);
void race_robot_passenger_control_48ad60(PcRaceContext&,std::uint32_t work);
void race_robot_driver_matrix_488af0(PcRaceContext&,std::uint32_t work,std::uint32_t position,std::uint32_t car_esi);
// 46BC30 GetHandleGripPositionLocal(car, angle, a, b): the two steering
// wheel grip points (radius 0.04 + table +5C at 0.314 / 2.827 rad) through
// the wheel frame (table +4C, z + 0.05, rotation X table +58, Z angle).
void race_char_grip_46bc30(PcRaceContext&,std::uint32_t car,float angle,std::uint32_t a,std::uint32_t b);
void pc_crt_qsort_580cb0(PcRaceMemory&,std::uint32_t base,std::uint32_t num,std::uint32_t width); // comparator 487920
}
