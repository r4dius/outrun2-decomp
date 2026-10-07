#pragma once
// OutRun2SP rankings (PC 452B80..453330, 4F3AD0..4F4574, 4165C0/4165F0).
// The book [63960C] = 7D3A90 (0x2C90 bytes, the rankings.dat image; its first
// 0x100 bytes are the race-end bookkeeping header) and the ranking tables
// 84DF40..850B00 that the SP screens and races read:
//   84DF40 / 8504C0: 2 x 5 x 10 records (0x10 each),
//   84E580 + i*0xC80: per (e, s) 0x320 planes of 50 records,
//   84FE80: 2 x 10 records, 84FFC0 + i*0x280: per (e, s) 0xA0 planes.
// Record copies between the book and the tables: 453090/453120/453170 (book ->
// tables, 4F3CC0) and 452F00/452F70/452FC0 (tables -> book, 4F3AD0). 4F3EA0
// writes the default records (names 5DA328/2C/30, 4:50.000 / 14:40.000).
// 453440 is the boot initialisation (the 47F110 debug key is 0: no dump).
#include "platform/race_area.hpp"
#include <array>
#include <vector>
#include <cstdint>
namespace outrun::platform {
constexpr std::uint32_t SpBookBase=0x7d3a90u,SpBookSize=0x2c90u;
constexpr std::uint32_t SpTablesBase=0x84df40u,SpTablesEnd=0x850b00u;
constexpr std::uint32_t SpStatsBase=0x7d6720u,SpStatsSize=0x44u;
// 453090 / 453120 / 453170: one 16-byte record book -> dst.
void sp_book_record_453090(PcRaceMemory&,std::uint32_t dst,std::uint32_t i,std::uint32_t j,std::uint32_t k);
void sp_book_record_453120(PcRaceMemory&,std::uint32_t dst,std::uint32_t i,std::uint32_t j,std::uint32_t k);
void sp_book_record_453170(PcRaceMemory&,std::uint32_t dst,std::uint32_t i,std::uint32_t e,std::uint32_t s,std::uint32_t j,std::uint32_t k);
// 452F00 / 452F70 / 452FC0: the inverse copies (src -> book).
void sp_book_store_452f00(PcRaceMemory&,std::uint32_t src,std::uint32_t i,std::uint32_t j,std::uint32_t k);
void sp_book_store_452f70(PcRaceMemory&,std::uint32_t src,std::uint32_t i,std::uint32_t j,std::uint32_t k);
void sp_book_store_452fc0(PcRaceMemory&,std::uint32_t src,std::uint32_t i,std::uint32_t e,std::uint32_t s,std::uint32_t j,std::uint32_t k);
void sp_tables_from_book_4f3cc0(PcRaceMemory&);   // bridge 4F3CC6 (measured): [esp+1C] = EAX = 84E580
void sp_tables_defaults_4f3ea0(PcRaceMemory&);   // 4F3CC0, then the default records (47F110 = 0)
void sp_book_from_tables_4f3ad0(PcRaceMemory&);   // then 453330 (47F110 = 0: nothing)
void sp_stats_from_book_453210(PcRaceMemory&);
void sp_book_header_452b80(PcRaceMemory&);
void sp_book_init_453440(PcRaceMemory&);          // boot path (47F110 = 0)
void sp_stats_set_452ed0(PcRaceMemory&,std::uint32_t a,std::uint32_t b);
void sp_stats_count_452cd0(PcRaceMemory&,std::uint32_t i,std::uint32_t j);
void sp_stats_average_452d20(PcRaceMemory&,std::uint32_t i,std::uint32_t d);
// The name entry's record accessors: addresses of 4F3810 / 4F3870 / 4F3910's records,
// 4F3950 / 4F39E0 / 4F3A80's store (at = the same address) and 4F3010's insertion.
std::uint32_t sp_record_4f3810(std::uint32_t preset,std::uint32_t count,std::uint32_t i);
std::uint32_t sp_record_4f3870(std::uint32_t preset,std::uint32_t a,std::uint32_t b,std::uint32_t count,std::uint32_t i);
std::uint32_t sp_record_4f3910(std::uint32_t preset,std::uint32_t count,std::uint32_t i);
void sp_store_record(PcRaceMemory&,std::uint32_t at,const std::array<std::uint32_t,4>& record);
// 4F3440: 4F3010's table choice, nibble check and sort without the insertion: the rank (0..9, -1
// when below the ten) of each sorted record, records given by value (n = records.size(), 1..4).
std::int32_t sp_rank_query_4f3440(const PcRaceMemory&,std::vector<std::array<std::uint32_t,4>> records,std::vector<std::int32_t>& ranks,
                                  std::uint32_t variant,std::uint32_t preset,std::uint32_t count,std::uint32_t a,std::uint32_t b);
std::int32_t sp_rank_insert_4f3010(PcRaceMemory&,std::int32_t n,std::uint32_t records,std::uint32_t ranks,std::uint32_t variant,
                                   std::uint32_t preset,std::uint32_t count,std::uint32_t a,std::uint32_t b);
}
