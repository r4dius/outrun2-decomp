#pragma once
// Native runtime binding of the race sound manager (race_sound.hpp):
//   event 383 SOUND       424650 init / 424700 control / 424790 destroy
//                         (display 49A650 is a RET)
//   event 360 COMM_TRANS  45A280 control
// World:
//   79FB48+id            the event slots' flags (79FCC7 = event 383's byte);
//   event works 799B38    8 = the player car work (car_799d18 at the event-8
//                         work token), 9..31 = the race manager's car works
//                         7815A0 (no native owner: an open one latches);
//   82E7F0 / *(car+2B4)   body_82e7f0 / car_select.parameters.
// The SE queue 9563E8 and its cursors 9560C0/956124 are the ones the player
// car fills (RaceCarWorld::sound_entries_9563e8 / sound_state): copied into
// the manager block around every callback, so there is one queue.
// Services: stage records/levels on the AREA owner memory (as the race
// manager binding), 450570 = race manager 7D3930, 43F9C0 = pause byte
// 780248, 48B310/48B350/55A930 from START, 48B1C0 = byte 83036C (.bss 0;
// 0xFF from mode-10 init 48B210 and mode-12 exit 48B2A0, tracked by
// native_runtime_mode_control; its other writer 48B1B0 is only reached from
// the unported demo/replay paths 4C0874/4C5022).
// Audio boundary (the platform callbacks):
//   42F0D0  effect(command): true when the native sample banks played it;
//           a refused command is counted per id (the race banks are not
//           loaded natively) and never stops the game;
//   42EFF0  ICS channel parameter: mirrored per channel/code and counted
//           (no ICS synthesiser natively);
//   427630  stop_all() (ClearAllSound).
// 95B248 (audio layer ready) is audio_ready(). 45A280 reads the CommRace
// manager [7D68AC], which is not modelled: counted as missing.
#include "platform/race_sound.hpp"
#include <functional>
#include <map>
#include <string>
namespace outrun::platform {
struct NativeRuntimeContext;
class PcSceneRenderer;
struct NativeRaceSound {
    RaceSoundState state{};
    std::uint8_t option_83036c{};                         // byte 83036C (48B1C0)
    std::function<bool()> audio_ready;                    // 95B248
    std::function<bool(std::uint32_t)> effect;            // 42F0D0
    std::function<void()> stop_all;                       // 427630
    std::map<std::uint64_t,std::uint32_t> ics_last;       // (channel<<32 | code) -> last value (42EFF0)
    std::map<std::uint32_t,std::uint32_t> played,refused; // 42F0D0 command -> count
    std::map<std::uint32_t,std::uint32_t> routed,missing; // services answered / not ported
    std::uint32_t inits{},controls{},destroys{},skipped{},ics_updates{},clear_alls{},comm_trans{};
    bool initialized{},latched{};
    std::uint32_t fault_pc{};
    std::string last_error;
};
// Event-383/360 callbacks; false when `callback` is not one of them.
bool native_race_sound_invoke(NativeRuntimeContext&,std::uint32_t callback,PcSceneRenderer* renderer);
// Mode callbacks writing 83036C (48B210 init / 48B2A0 exit).
void native_race_sound_mode_token(NativeRuntimeContext&,std::uint32_t token);
std::string native_race_sound_status(const NativeRuntimeContext&);
}
