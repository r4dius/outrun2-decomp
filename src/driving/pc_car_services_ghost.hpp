#pragma once
// Ghost recording services of the PC game (car services, OR2006C2C.EXE).
// All PC globals are explicit inputs/outputs; guest pointers are never
// followed. Byte views follow pc_driving.hpp (bounded, little-endian).
#include "driving/pc_driving.hpp"
#include "driving/pc_matrix_stack.hpp"
#include <array>
#include <cstdint>
namespace outrun::driving {

// ---------------------------------------------------------------- 0x47ED90
// Record-slot ghost reset (tail target of 0x47F780).
struct PcGhostRecorderGlobals {
    Bytes recorder_81010c;          // 0x81010C..0x810123 (0x18 bytes; 47F330 ESI block)
    std::uint8_t player_model_810445=0;
    std::uint32_t frame_81043c=0;
};
struct PcGhostResetInputs {
    std::int32_t game_mode_78024c=0;   // 43F960: mode 2/3 -> 450780 returns 14, else 4
    std::uint8_t byte_83036d=0;        // 48B1A0
    std::uint8_t event_11=0,event_12=0,event_13=0; // [0x799D18]+0x11/+0x12/+0x13
};
// records_81373c: slot array at [0x81373C], stride 0xFD4, at least
// (450780()+1) slots (5 or 15).
void pc_ghost_record_reset_47ed90(PcGhostRecorderGlobals&,Bytes records_81373c,const PcGhostResetInputs&);
std::int32_t pc_ghost_slot_limit_450780(std::int32_t game_mode_78024c);

// ---------------------------------------------------------------- 0x47F330
// Ghost record writer. PC ABI: ESI = recorder block (0x81010C), ECX = record
// slot ([0x81373C]+slot*0xFD4), [esp+4] = car event, [esp+8] = divisor.
struct PcGhostRecordWriteInputs {
    std::uint32_t frame_81043c=0;
    std::uint32_t divisor=0;          // DIV: 0 raises #DE on the PC (throws here)
    Bytes area_matrix_7d2da0;         // 44BED0(car+0x5c == 0)
    Bytes area_matrix_7d3190;         // 44BED0(car+0x5c != 0)
};
// car: event view (>= 0x2F0 bytes); reads +0x04,+0x14..+0x1C,+0x2C/+0x2E/+0x30,
// +0x33,+0x38,+0x5C,+0x2C8,+0x2D8..+0x2EC; writes +0x2C8 (=0) on the no-float path.
void pc_ghost_record_write_47f330(Bytes recorder_81010c,Bytes record_slot,Bytes car,
    const PcGhostRecordWriteInputs&,PcMatrixStack&);

// ---------------------------------------------------------------- 0x466E50
// Ghost packet frame (0x30 bytes, the 7F92C8 layout) and packet codecs.
// Key packet (0x1C/0x14 bytes)  : 466A20 encode, 466AF0 decode.
// Delta packet (0x10/0x08 bytes): 4667D0 encode, 4668F0 decode.
void pc_ghost_key_encode_466a20(Bytes packet,Bytes frame);       // EAX=packet, ECX=frame
void pc_ghost_key_decode_466af0(Bytes frame,Bytes packet);       // EAX=frame,  ECX=packet
void pc_ghost_frame_delta_466030(Bytes delta,Bytes frame,Bytes prev); // EAX,ECX,[esp+4]
void pc_ghost_delta_encode_4667d0(Bytes packet,Bytes delta);     // EAX=packet, ECX=delta
void pc_ghost_delta_decode_4668f0(Bytes delta,Bytes packet);     // EAX=delta,  ECX=packet
void pc_ghost_frame_add_466720(Bytes frame,Bytes delta,Bytes prev);   // EAX,ECX,[esp+4]
// 0x449640 with a NULL matrix argument uses a copy of the current matrix
// (40A0D0); with a matrix, its rows are normalised in place.
std::array<float,3> pc_matrix_angles_449640(Bytes matrix);

struct PcGhostPacketState {
    std::uint32_t overflow_7f8ef8=0;
    std::uint32_t count_7f8ef4=0;
    std::uint32_t cursor_7f91f0=0;   // guest address inside the packet buffer
    std::uint32_t base_7f8d84=0;     // guest address of the packet buffer start
    Bytes prev_frame_7f92c8;         // 0x30 bytes
    Bytes matrix_7f9300;             // 0x40 bytes
};
struct PcGhostPacketBuffer {
    Bytes bytes;                     // guest range [guest_base, guest_base+size)
    std::uint32_t guest_base=0;
};
struct PcGhostPacketInputs {
    std::uint32_t player_5c=0;       // [[0x799D18]+0x5C]
    std::int32_t frame_counter_656234=0; // 48B350: < 0x3C selects the matrix path
    Bytes area_matrix_7d2da0;        // 44BEA0
};
// PC 0x466E50 (EAX = car event). car reads +0x14..+0x1C,+0x2C..+0x32,+0x40,
// +0x44,+0x260,+0x2C8,+0x2D8..+0x2EC.
void pc_ghost_packet_write_466e50(PcGhostPacketState&,PcGhostPacketBuffer,Bytes car,
    const PcGhostPacketInputs&,PcMatrixStack&);
}
