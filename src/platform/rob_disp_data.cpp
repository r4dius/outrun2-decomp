// EXE bytes of the robot display / osage modules (OR2006C2C.EXE, generated).
#include "system/exe_image.hpp"
#include "platform/rob_disp.hpp"
namespace outrun::platform {
// .rdata 5E62F0..5E6B40: ROBDISPWORK face/eye tables, object names of
// 513750/513840/513A30/513BA0, osage sphere definitions and their
// per-kind pointer tables 5E6740/5E6900/5E6AE0 (515040).
alignas(16) std::uint8_t RobDispRdata[0x850]{}; OR2_EXE_COPY(RobDispRdata,0x5E62F0u,0x850u);
// .rdata 5C09C0..5C0B80: the character file names of the 654860 records (488B80).
alignas(16) std::uint8_t RobChrPaths[0x1c0]{}; OR2_EXE_COPY(RobChrPaths,0x5C09C0u,0x1C0u);
// .data 7134E0..7137A8 (never written by the EXE): 7134EC osage damping,
// 7134F0/713570 per-node damping lists, 7136D0 sphere storage pointers.
alignas(16) std::uint8_t RobOsageData[0x2c8]{}; OR2_EXE_COPY(RobOsageData,0x7134E0u,0x2C8u);
}
