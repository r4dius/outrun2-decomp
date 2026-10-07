#pragma once
// PC Time Attack ghost module (OR2006C2C.EXE 465F30..4687C0) and the ghost
// save slot it reads through the profile bank's save object (8A8C7C).
// Line-by-line transliteration over PC addresses (PcRaceMemory, see
// race_area.hpp): the module .bss block 7F8D80..7F9420, the save layout
// table 7457C8 (416610), the words 8A8C74/8A8C78, the byte 64BFED and the
// save object [8A8C7C] (0x13C bytes) are mapped by the caller; the heap
// blocks (7F9228 work record, 7F9224 three playback records, 467340
// scratch) are mapped by the malloc service at the address it returns.
// Every PC function outside the module is one PcRaceService call:
//   580253 malloc(n) / 580BC2 free(p); 580C33 new(n) / 580C38 delete(p)
//   4239C0 open(name,"rb") / 423F10 size(f) / 423CB0 read(buf,size,count,f)
//     / 423BD0 close(f)                      (retail tree, "\Ghosts\...")
//   406DB0 exists: ECX = save object (name at +0), returns 0/1
//   406E50 read / 406C50 write: ECX = save object, args {buffer, size}
//     (the PC passes the size in EBX); return the PC status (0 = done)
//   48B320 -> [656234] (TA course code), 466340 network ghost (unported:
//     the service must refuse it).
// The CRT sprintf calls (5802DD) are done here with the same formats.
#include "platform/race_area.hpp"
#include <array>
#include <cstdint>
#include <vector>
namespace outrun::platform {
constexpr std::uint32_t PcGhostRecordBytes=0x9cac;
struct PcGhostState {
    static constexpr std::uint32_t BlockBase=0x7f8d80u,BlockEnd=0x7f9420u;
    static constexpr std::uint32_t TableBase=0x7457c8u,TableEnd=0x746470u;
    static constexpr std::uint32_t WordsBase=0x8a8c74u,WordsEnd=0x8a8c80u;  // 8A8C74 files, 8A8C78 slot, 8A8C7C object
    static constexpr std::uint32_t SaveObjectBytes=0x13c;
    static constexpr std::uint32_t QueueBase=0x841fa8u,QueueEnd=0x841ff0u;  // 4ACFC0 ghost car queue
    std::vector<std::uint8_t> block=std::vector<std::uint8_t>(BlockEnd-BlockBase);
    std::vector<std::uint8_t> table;                    // EXE boot image, then 416610
    std::array<std::uint8_t,WordsEnd-WordsBase> words{};
    std::array<std::uint8_t,QueueEnd-QueueBase> queue{};
    std::array<std::uint8_t,2> bytes_64bfec{};              // 64BFEC playback flag, 64BFED course code
    PcGhostState();
    void map(PcRaceMemory& m){
        m.map(BlockBase,block.data(),block.size());m.map(TableBase,table.data(),table.size());
        m.map(WordsBase,words.data(),words.size());m.map(QueueBase,queue.data(),queue.size());m.map(0x64bfecu,bytes_64bfec.data(),2);
    }
};
// Read-only EXE tables: 5A27D8 CRC-16 words, 5B0570 "C2C ".
void ghost_map_tables(PcRaceMemory&);
// 416610: record offsets/files of the 404 save records (4162D0 at boot).
void ghost_save_layout_416610(PcRaceMemory&);
// 416700(a0..a6): request the GHOST%02d.DAT file of a record (VM entry =
// sub esp,0x108). Returns the record index or -1 when a file is loaded.
std::uint32_t ghost_save_open_416700(PcRaceContext&,const std::array<std::uint32_t,7>& args);
// 423030 (ECX = save object, EAX = size, [esp+4] = name): returns AL.
std::uint32_t ghost_save_load_423030(PcRaceContext&,std::uint32_t object,std::uint32_t size,std::uint32_t name);
bool ghost_save_copy_416870(PcRaceContext&,std::uint32_t index,std::uint32_t dst,std::uint32_t n);
void ghost_save_close_416830(PcRaceContext&);
void ghost_save_read_416930(PcRaceContext&,const std::array<std::uint32_t,9>& args);
// 4168D0(index, src, n): copy into the save buffer (423100), +138 dirty; 4167F0:
// write the buffer back (406C50) when dirty. Read from the Steam build; the
// launcher handshake (40E400) that guards them is the protection's, not ported.
bool ghost_save_write_4168d0(PcRaceContext&,std::uint32_t index,std::uint32_t src,std::uint32_t n);
bool ghost_save_flush_4167f0(PcRaceContext&);
// 4169D0(src, n, flag, cvt, level, kind, a7, a8, a9): 416930's record index, then 4168D0.
bool ghost_save_write_4169d0(PcRaceContext&,const std::array<std::uint32_t,9>& args);
// Race end of a Time Attack run: 467880 picks the save record of a better ghost
// (4675F0 finalize, 467760 -> 416700), 467960 writes it (4677B0: 4168D0 + 4167F0).
void ghost_save_prepare_467880(PcRaceContext&);
void ghost_save_store_467960(PcRaceContext&);
std::uint16_t ghost_crc_449a80(const PcRaceMemory&,std::uint32_t p,std::int32_t n);
void ghost_init_467ac0(PcRaceContext&);
void ghost_load_4686c0(PcRaceContext&);
// 467340(code, slot, flag): one playback record into slot.
void ghost_load_record_467340(PcRaceContext&,std::uint32_t code,std::uint32_t slot,std::uint32_t flag);
void ghost_path_466be0(PcRaceContext&,std::uint32_t buffer,std::uint32_t code,std::uint32_t kind);
void ghost_total_time_466460(PcRaceMemory&,std::uint32_t ghost);
void ghost_trim_467260(PcRaceMemory&,std::uint32_t ghost);
void ghost_sections_4662b0(PcRaceMemory&,std::uint32_t ghost,std::int32_t slot);
// ---- ghost cars (race_ghost_car.cpp): events 9..12, car works 7815A0 + k*0x10F0
// 4ACFC0(event, k, a2, a3) / 4ACFB0: the creation queue 841FA8 (count),
// 841FA9 (next), 841FB0 + n*16 {+0 event, +8 a2, +C k, +D a3}.
void ghost_car_queue_4acfc0(PcRaceMemory&,std::uint32_t event,std::uint8_t k,std::uint32_t a2,std::uint8_t a3);
void ghost_car_queue_reset_4acfb0(PcRaceMemory&);
// 4AD000 init: services 440BD0(event, 0x100), 440BA0(0).
void ghost_car_init_4ad000(PcRaceContext&,std::uint32_t car);
void car_reset_46f350(PcRaceMemory&,std::uint32_t car);
// 4ACE40 control: services 49B2D0, 4962A0, 4401D0(event), 4A2650(car);
// reads 78026C, 780258, 656234, 799D18 (player car), 7D2DA0; matrices on
// PcRaceContext::matrices.
void ghost_car_control_4ace40(PcRaceContext&,std::uint32_t car);
std::uint32_t ghost_play_467e30(PcRaceContext&,std::uint32_t car,std::uint32_t slot);
std::uint32_t ghost_state_466250(const PcRaceMemory&,std::uint32_t slot);
// 4671D0(car): player ghost recording into 7F9228 (466E50; services 49B2D0, 4962A0).
void ghost_record_4671d0(PcRaceContext&,std::uint32_t car);
// Finish of a Time Attack run: 467190 goal (last packet), 467E00 restart
// (4ACFB0, 467C60 slot 1 -> 4675F0 finalize, 466120 rewind). Services:
// 48B310, 48B320, 4505D0, 451180(1), 450610(level, sector), 450320, 424940,
// 440200(event), 440180(event, 0x58), 580253 / 580BC2.
void ghost_packet_466e50(PcRaceContext&,std::uint32_t car);
void ghost_goal_467190(PcRaceContext&);
void ghost_record_reset_466120(PcRaceMemory&);
std::uint32_t ghost_length_4662f0(const PcRaceMemory&,std::uint32_t record);
void ghost_finalize_4675f0(PcRaceContext&,std::uint16_t code,std::uint32_t name);
std::uint8_t ghost_best_467c60(PcRaceContext&,std::uint32_t slot);
void ghost_restart_467e00(PcRaceContext&);
void ghost_release_465fa0(PcRaceContext&);            // mode 28/30 teardown
// 4666A0: per-update counter 7F9220 (services 49B2D0; reads [799D54] = event 9 work).
void ghost_frame_4666a0(PcRaceContext&);
void ghost_car_destroy_470560(PcRaceMemory&,std::uint32_t car);
// Variant 0 record ghost cars (440750(4): events 9..12): queue 4AD1A0 (841FF0 count, 841FF1 read
// index, 841FF8 + i*0x10), init 4AD1F0, control 4AD080 (480220 / 47F1A0 / 47F140 are services).
void record_car_queue_4ad1a0(PcRaceMemory&,std::uint32_t event,std::uint8_t k,std::uint32_t a2,std::uint8_t a3);
void record_car_init_4ad1f0(PcRaceContext&,std::uint32_t car);
void record_car_control_4ad080(PcRaceContext&,std::uint32_t car);
// 4F6290(spline, value, time) / 454FC0(spline, time) (x87 result, stored as float).
void ghost_spline_fit_4f6290(PcRaceMemory&,std::uint32_t spline,float value,std::uint32_t time);
float ghost_spline_454fc0(const PcRaceMemory&,std::uint32_t spline,std::uint32_t time);
// Display (draw lists on PcRaceContext::draws): 4AE5F0 (ghost/other car,
// leaves 405360/4056D0/4044F0/4044A0/4044C0/4044E0/4052B0/4052C0/404540/
// 405350; services 46F990, 49EED0, 4957F0/495B00/4962D0/495860) and the
// shadow 4ADAC0 (4AD380). Reads 7D39F0, 7C24C9, 79FB48, 7F94C0 and the EXE
// car tables mapped by ghost_map_car_tables.
void ghost_map_car_tables(PcRaceMemory&);
void other_car_draw_4ae0f0(PcRaceContext&,std::uint32_t car);
void ghost_car_display_4ae5f0(PcRaceContext&,std::uint32_t car);
void ghost_car_shadow_4adac0(PcRaceContext&,std::uint32_t car);
// 465F40(i): car byte of playback record i ('NHOT' at +0), else -1 (AL).
std::int8_t ghost_car_465f40(const PcRaceMemory&,std::uint32_t i);
}
