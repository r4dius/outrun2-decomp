#pragma once
// Code translated mechanically by tools/x86tr (OR2_TR_PLAIN=1, from the unpacked
// Steam listing: the same build, with the protected functions in plain code) run
// over a native module's PcRaceMemory. Each translated instruction does what the
// original does; memory goes through the module's mapped regions and a guest
// stack mapped for the call; a call to code that is not translated goes to the
// module's PcRaceService as a PcRaceCall (EAX, ECX and the stack arguments from
// [esp+4]); stdcall / thiscall callees pop their arguments as listed.
#include "platform/race_area.hpp"
#include "driving/pc_matrix_stack.hpp"
#include "recomp/x86rt.hpp"
#include <cstdint>
#include <functional>
#include <initializer_list>
#include <vector>
namespace outrun::platform {
class PcD3D9Device;
struct TranslatedFunction { std::uint32_t pc; or2x86::Fn fn; };
struct TranslatedPops { std::uint32_t pc; std::uint32_t bytes; };   // RET n of a callee
// Bytes of .text the translated code reads as data (switch index tables).
// .text bytes read as data (jump tables); bytes == nullptr: read from the
// player's EXE image (system/exe_image).
struct TranslatedCodeData { std::uint32_t base; std::uint32_t size; const std::uint8_t* bytes; };
struct TranslatedModule {
    const TranslatedFunction* functions{};   // ends with {0, nullptr}
    const TranslatedPops* pops{};            // ends with {0, 0}
    // A callee taking register arguments the PcRaceCall does not carry (ESI, EBX...):
    // handled here, true when done (the handler pops the return address and arguments).
    std::function<bool(or2x86::Cpu&,std::uint32_t pc)> special;
    // The D3D device [89BD60] seen by the translated code: a guest object whose
    // vtable entries are thunks dispatched to this device (stdcall, `this`
    // first). Null: [89BD60] is whatever the module maps.
    PcD3D9Device* device{};
    // The renderer matrix stack 89B564 (current pointer) / 89B568 (depth) /
    // 89B56C (capacity) over this native stack, synchronised around every
    // call that leaves the translated code.
    driving::PcMatrixStack* matrices{};
    // CRT rand() 580F40 state (the runtime's 7xx seed); null: rand is a service call.
    std::uint32_t* crt_random{};
    const TranslatedCodeData* code_data{};   // ends with {0, 0, nullptr}
    std::uint32_t fs_base{};                 // guest thread block FS: points at (SEH chain head at +0); 0: none
    // COM methods of proxies (translated_object) the module owns (DirectInput...):
    // true when handled, with EAX and the bytes popped after the return address
    // (`this` included). Unhandled ones go to the device's surface/query methods.
    std::function<bool(or2x86::Cpu&,std::uint32_t handle,std::uint32_t offset,std::uint32_t& eax,std::uint32_t& popped)> com;
    // Another translated set the code may call into on the same CPU and guest stack
    // (stdcall callees, _chkstk frames), searched after this one's functions; null: none.
    const TranslatedModule* next{};
};
// Guest stack of the translated calls (unused PC addresses).
constexpr std::uint32_t TranslatedStackBase=0x0ff00000u,TranslatedStackSize=0x10000u;
// The device object, its vtable and the thunks, and the matrix stack storage.
constexpr std::uint32_t TranslatedDeviceBase=0x0fe00000u,TranslatedDeviceThunks=0x0fe10000u,TranslatedMatrixBase=0x0fd00000u;
// COM object proxies (surfaces, queries) and their vtable thunks.
constexpr std::uint32_t TranslatedObjectBase=0x0fe20000u,TranslatedObjectThunks=0x0fe30000u;
// Guest pointer of a proxy holding a device handle (one reference, released by
// the guest's Release); 0 for 0. Proxies persist across calls.
std::uint32_t translated_object(std::uint32_t handle);
// fn(args...) with ECX = ecx (cdecl arguments pushed right to left); returns EAX.
std::uint32_t translated_call(PcRaceMemory& m,const PcRaceService& service,const TranslatedModule& module,
                              std::uint32_t pc,std::uint32_t ecx,std::initializer_list<std::uint32_t> args);
// The same with register arguments (EAX / EDX / ESI ... as the PC caller loads them).
struct TranslatedRegisters { std::uint32_t eax{},ecx{},edx{},ebx{},esi{},edi{}; };
std::uint32_t translated_call_registers(PcRaceMemory& m,const PcRaceService& service,const TranslatedModule& module,
                              std::uint32_t pc,const TranslatedRegisters& registers,std::initializer_list<std::uint32_t> args);
// The same with the stack arguments as a list (callers that forward a PcRaceCall).
std::uint32_t translated_call_list(PcRaceMemory& m,const PcRaceService& service,const TranslatedModule& module,
                              std::uint32_t pc,const TranslatedRegisters& registers,const std::vector<std::uint32_t>& args);
bool translated_has(const TranslatedModule&,std::uint32_t pc);
// Helpers for TranslatedModule::special: the stack argument n ([esp+4+4n]) and the return.
std::uint32_t translated_arg(or2x86::Cpu&,unsigned n);
void translated_return(or2x86::Cpu&,std::uint32_t eax,std::uint32_t popped_bytes=0);
// Rewrite check: `reference` (the translated original) then `candidate` (its native
// rewrite) from the same state, compared on every writable mapped byte, the live
// matrix stack, the draw count and the service calls (pc, ECX when the candidate
// passes one, the candidate's arguments, result). Mismatches go to stderr under
// `name`; returns true when both runs agree. The state after `candidate` is kept.
struct RaceDiffState { void* data; std::size_t size; };
// A service whose argument `arg` points at a record (a stack local of the caller):
// compared on `size` bytes from `offset` instead of on the address.
struct RaceDiffPointee { std::uint32_t pc,offset,size,arg{}; };
struct RaceDiffOptions {
    std::vector<std::uint32_t> ignored;     // pure queries the rewrite inlines: left out of the comparison
    std::vector<RaceDiffState> state;       // service state outside the mapped memory, saved and restored with it
    std::function<void()> rewind;           // undoes what the reference run did to the rest (allocators)
    bool keep=true;                         // false: the state before both runs is put back (a dry check)
    // The candidate's service calls are answered from the reference run's: its
    // results and the memory / matrix bytes each call changed, so services with
    // state outside the memory (mode requests, sprite pools, events) run once.
    bool replay=false;
    // For code whose services tear owners down (mode exits): the candidate runs
    // first against services that only record (and return 0), its writes are
    // undone, then the reference runs for real; compared are the calls and the
    // bytes the candidate wrote. (Code that uses service results cannot use it.)
    bool calls_only=false;
    std::function<std::uint32_t(const PcRaceCall&)> mock;   // calls_only / pure: the recording services' results (default 0)
    // Both runs against `mock` services (no effects), then everything is put back:
    // a dry check of the logic alone, for any service results the mock invents.
    bool pure=false;
    std::vector<RaceDiffPointee> pointees;
};
bool race_diff_run(PcRaceContext& c,const char* name,const std::function<void(PcRaceContext&)>& reference,
                   const std::function<void(PcRaceContext&)>& candidate,const RaceDiffOptions& options={});
}
