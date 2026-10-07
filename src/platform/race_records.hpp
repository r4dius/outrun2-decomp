#pragma once
// PC record module of the arcade variant 0 (OR2006C2C.EXE 47EC00..481180): the "QHOT" course
// records of the 15-course Time Attack / OutRun runs, kept in two heap tables ([81373C]: 15 x
// 0xFD4, [813740]: 30 x 0xFD4) and in the .bss words 80FB40..81044F / 813730..81374F, loaded
// from the GHOST%02d.DAT save slot (416700, shared with the Time Attack ghost module).
// Line-by-line transliteration over PC addresses (PcRaceMemory); every PC function outside the
// module is one PcRaceService call (580253 malloc, 416700 save slot request).
#include "platform/race_area.hpp"
#include <array>
#include <cstdint>
namespace outrun::platform {
constexpr std::uint32_t PcRecordBytes=0xfd4u;                 // one QHOT record
constexpr std::uint32_t PcRecordTable1Bytes=0xed6cu,PcRecordTable2Bytes=0x1dad8u;   // 47ECA0 mallocs
struct PcRecordState {
    static constexpr std::uint32_t BlockBase=0x80fb40u,BlockEnd=0x810454u;   // 80FB40 slot, 80FB44/45, 80FB48..81044F, 810450
    static constexpr std::uint32_t TailBase=0x813730u,TailEnd=0x813750u;     // 813730/34/38 slot request, 81373C/40 tables, 813748
    std::array<std::uint8_t,BlockEnd-BlockBase> block{};
    std::array<std::uint8_t,TailEnd-TailBase> tail{};
    void map(PcRaceMemory& m){m.map(BlockBase,block.data(),block.size());m.map(TailBase,tail.data(),tail.size());}
    void map_const(PcRaceMemory& m) const {m.map_const(BlockBase,block.data(),block.size());m.map_const(TailBase,tail.data(),tail.size());}
};
// 47ECA0: [81373C] / [813740] allocated once (580253 0xED6C / 0x1DAD8), then both cleared.
void records_tables_47eca0(PcRaceContext&);
// 480AD0(bl = level, arg = kind): the save slot request of the default record
// ("\RecordData\gc_default_<old|cvt>_<nml|tn1|tn2><_a|_m>_00_0.rec" is formatted into a stack
// buffer nobody reads), 416700 -> [80FB40].
void records_request_480ad0(PcRaceContext&,std::uint8_t level,std::uint32_t kind);
// 480FE0(a, b, c) (scene owner 49BA80 stage 6, variant 0): 47ECA0, 80FB48..81044F cleared,
// [810128] = 2, [810444] = [78024C], [810445..47] = a, b, c, then 480AD0(bl = b, c).
void records_init_480fe0(PcRaceContext&,std::uint32_t a,std::uint32_t b,std::uint32_t c);
void records_path_480130(PcRaceContext&,std::uint32_t buf,std::uint32_t level,std::uint32_t kind,std::uint32_t half,
    std::uint32_t media,std::uint32_t index);
void records_load_480bc0(PcRaceContext&,std::uint32_t rec,std::uint32_t index,std::uint32_t level,std::uint32_t kind,std::uint32_t half);
void records_load_4810a0(PcRaceContext&,std::uint32_t level,std::uint32_t ebx);
// 481230(a, b, c) (scene owner 49BA80 stage 7, variant 0): every QHOT record of [813740].
void records_load_481230(PcRaceContext&,std::uint32_t a,std::uint32_t b,std::uint32_t c);
// ---- record ghost car playback (480220) and its helpers ----
std::uint32_t records_sample_47f930(PcRaceMemory&,std::uint32_t cursor,std::uint32_t stream,std::uint32_t out,std::uint32_t ref);
void records_bezier_47ef90(PcRaceMemory&,float t,std::uint32_t out,std::uint32_t cur,std::uint32_t next,std::uint32_t prev);
void records_words_4800f0(PcRaceMemory&,std::uint32_t next,std::uint32_t out,std::uint32_t cur,float t);
void records_word_47f0e0(PcRaceMemory&,std::uint32_t a,std::uint32_t b,float t,std::uint32_t out);
void records_drop_47ed00(PcRaceMemory&,std::uint32_t slot);
std::uint32_t records_state_47f140(const PcRaceMemory&,std::uint32_t slot);
std::uint32_t records_frame_47f1a0(const PcRaceMemory&,std::uint32_t slot);
// 480220(work, slot, frame): services 450130 (race manager flag) and 43F370 (course length).
std::uint32_t records_play_480220(PcRaceContext&,std::uint32_t work,std::uint32_t slot,std::uint32_t frame);
// ---- per-update record scheduling (480F80) ----
void records_bind_47f260(PcRaceMemory&,std::uint32_t event,std::uint32_t slot,std::uint32_t model,std::uint32_t rec);
// 47FF60 / 47FD40 / 480F80: services 44C940, 49B2D0, 451350, 450250, 450780, 43F9C0.
void records_schedule_47ff60(PcRaceContext&);
void records_advance_47fd40(PcRaceContext&);
void records_frame_480f80(PcRaceContext&);
// ---- the player's recording (CommonPlCar 47F780) ----
void records_restart_47ed90(PcRaceContext&);
void records_encode_47f330(PcRaceContext&,std::uint32_t rec,std::uint32_t state,std::uint32_t work,std::uint32_t every);
// 47F780: services 44C940, 450130, 44FF10, 451180, 450750, 4505A0, 450630, 49B2D0, 48B1A0, 450780.
void records_player_47f780(PcRaceContext&,std::uint32_t work);
// ---- variant 0 race end (mode 25 / the stage table): services 450780 ----
void records_stage_flags_47ee70(PcRaceContext&,std::uint32_t out);
// Variant 0 name entry (4B5141 path): 480D00 picks the save record (416700 -> [80FB40]),
// 481180(name) stamps the name on each stage record and stores the better ones (480DF0 ->
// 4169D0), 481100 the current stage's record when it beats the loaded one, then 4167F0.
// Read from the Steam build (their 4168D0 launcher guard is the protection's, not ported).
void records_prepare_480d00(PcRaceContext&);
void records_store_480df0(PcRaceContext&,std::uint32_t rec,std::int32_t level,std::uint32_t kind);
void records_better_481100(PcRaceContext&);
void records_store_481180(PcRaceContext&,std::uint32_t name);
std::uint32_t records_ranking_47ef30(PcRaceContext&,std::uint32_t preset);
}
