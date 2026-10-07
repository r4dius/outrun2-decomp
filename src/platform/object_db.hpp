#pragma once
// Object name database of the PC EXE (Common\object_db_bin.sz): model
// object handles (resource << 16 | object) by name.
//   448AB0 reset of the loader state (7C27F0 slot, 7C27F4 state, 7C27F8)
//   448B90 loader step (shared loader 49E580 stage 4): async load, then 43
//          groups of {name offset, handle} (handle -1 ends a group) are
//          hashed into the table 7CC1E0 (count 7D25E0, sorted flag 7D25E4)
//   448A70 name hash (h = h * 0x83 + toupper(c))
//   448B10 lookup (qsort on first use, binary search), 448CD0 = 448B10 on 7CC1E0
// The count is never reset by the PC: every build appends.
#include "driving/pc_driving.hpp"
#include <array>
#include <cstdint>
#include <string>
namespace outrun::platform {
struct PcObjectDb {
    static constexpr std::uint32_t Capacity=0xc80u;               // 7CC1E0..7D25E0
    struct Entry { std::int32_t hash{}; std::uint32_t handle{}; };
    std::array<Entry,Capacity> table_7cc1e0{};
    std::uint32_t count_7d25e0{};
    bool sorted_7d25e4{};
    std::uint32_t slot_7c27f0{},state_7c27f4{},word_7c27f8{};
    bool built{};                                                // a 448B90 build ran
    std::uint32_t builds{},lookups{},misses{};
    std::string error;                                           // file / build failure (lookups then throw)
};
void object_db_reset_448ab0(PcObjectDb&);
// 448A70. Throws for a byte outside ASCII (the CRT toupper of a negative
// char is locale data the port does not model).
std::int32_t object_db_hash_448a70(const std::string& name);
// 448BF1..448C7C over the loaded payload (the .sz payload after its size
// word). Throws on a malformed file or a full table.
void object_db_build_448b90(PcObjectDb&,driving::Bytes payload);
// 448B10: handle of a name, 0xFFFFFFFF when absent. Throws when two entries
// share the hash of the name with different handles (the PC result would
// depend on the qsort order).
std::uint32_t object_db_find_448b10(PcObjectDb&,const std::string& name);
}
